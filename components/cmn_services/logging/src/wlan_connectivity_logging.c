/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * DOC: wlan_cm_roam_logging.c
 *
 * Implementation for the connectivity and roam logging api.
 */

#include "wlan_mlme_api.h"
#include "wlan_mlme_main.h"
#include "wlan_connectivity_logging.h"

#ifdef WLAN_FEATURE_CONNECTIVITY_LOGGING
static struct wlan_connectivity_log_buf_data global_cl;

static void
wlan_connectivity_logging_register_callbacks(struct wlan_cl_hdd_cbks *hdd_cbks)
{
	global_cl.hdd_cbks.wlan_connectivity_log_send_to_usr =
			hdd_cbks->wlan_connectivity_log_send_to_usr;
}

void wlan_connectivity_logging_start(struct wlan_cl_hdd_cbks *hdd_cbks)
{
	global_cl.head = vzalloc(sizeof(*global_cl.head) *
					 WLAN_MAX_LOG_RECORDS);
	if (!global_cl.head) {
		QDF_BUG(0);
		return;
	}

	global_cl.write_idx = 0;
	global_cl.read_idx = 0;

	qdf_atomic_init(&global_cl.dropped_msgs);
	qdf_spinlock_create(&global_cl.write_ptr_lock);

	global_cl.read_ptr = global_cl.head;
	global_cl.write_ptr = global_cl.head;
	global_cl.max_records = WLAN_MAX_LOG_RECORDS;

	wlan_connectivity_logging_register_callbacks(hdd_cbks);
	qdf_atomic_set(&global_cl.is_active, 1);
}

void wlan_connectivity_logging_stop(void)
{
	if (!qdf_atomic_read(&global_cl.is_active))
		return;

	qdf_atomic_set(&global_cl.is_active, 0);
	global_cl.read_ptr = NULL;
	global_cl.write_ptr = NULL;
	qdf_spinlock_destroy(&global_cl.write_ptr_lock);
	global_cl.read_idx = 0;
	global_cl.write_idx = 0;

	vfree(global_cl.head);
	global_cl.head = NULL;
}
#endif

static bool wlan_logging_is_queue_empty(void)
{
	if (!qdf_atomic_read(&global_cl.is_active))
		return true;

	qdf_spin_lock_bh(&global_cl.write_ptr_lock);

	if (global_cl.read_ptr == global_cl.write_ptr &&
	    !global_cl.write_ptr->is_record_filled) {
		qdf_spin_unlock_bh(&global_cl.write_ptr_lock);
		return true;
	}

	qdf_spin_unlock_bh(&global_cl.write_ptr_lock);

	return false;
}

QDF_STATUS
wlan_connectivity_log_enqueue(struct wlan_log_record *new_record)
{
	struct wlan_log_record *write_block;

	/*
	 * This API writes to the logging buffer if the buffer is empty.
	 * 1. Acquire the write spinlock.
	 * 2. Copy the record to the write block.
	 * 3. Update the write pointer
	 * 4. Release the spinlock
	 */
	qdf_spin_lock_bh(&global_cl.write_ptr_lock);

	write_block = global_cl.write_ptr;
	/* If the buffer is not empty, increment the dropped msgs counter and
	 * return
	 */
	if (global_cl.read_ptr == global_cl.write_ptr &&
	    write_block->is_record_filled) {
		qdf_spin_unlock_bh(&global_cl.write_ptr_lock);
		qdf_atomic_inc(&global_cl.dropped_msgs);
		logging_debug("vdev:%d dropping msg sub-type:%d total drpd:%d",
			      new_record->vdev_id, new_record->log_subtype,
			      qdf_atomic_read(&global_cl.dropped_msgs));
		wlan_logging_set_connectivity_log();

		return QDF_STATUS_E_NOMEM;
	}

	*write_block = *new_record;
	write_block->is_record_filled = true;

	global_cl.write_idx++;
	global_cl.write_idx %= global_cl.max_records;

	global_cl.write_ptr =
		&global_cl.head[global_cl.write_idx];

	qdf_spin_unlock_bh(&global_cl.write_ptr_lock);

	wlan_logging_set_connectivity_log();

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_connectivity_log_dequeue(void)
{
	struct wlan_log_record *data;
	uint8_t idx = 0;
	uint64_t current_timestamp, time_delta;

	if (wlan_logging_is_queue_empty())
		return QDF_STATUS_SUCCESS;

	data = qdf_mem_malloc(MAX_RECORD_IN_SINGLE_EVT * sizeof(*data));
	if (!data)
		return QDF_STATUS_E_NOMEM;

	while (global_cl.read_ptr->is_record_filled) {
		current_timestamp = qdf_get_time_of_the_day_ms();
		time_delta = current_timestamp -
				global_cl.first_record_timestamp_in_last_sec;
		/*
		 * Don't send logs if the time difference between the first
		 * packet queued and current timestamp is less than 1 second and
		 * the sent messages count is 20.
		 * Else if the current record to be dequeued is 1 sec apart from
		 * the previous first packet timestamp, then reset the
		 * sent messages counter and first packet timestamp.
		 */
		if (time_delta < 1000 &&
		    global_cl.sent_msgs_count >= WLAN_RECORDS_PER_SEC) {
			break;
		} else if (time_delta > 1000) {
			global_cl.sent_msgs_count = 0;
			global_cl.first_record_timestamp_in_last_sec =
							current_timestamp;
		}

		global_cl.sent_msgs_count %= WLAN_RECORDS_PER_SEC;
		data[idx] = *global_cl.read_ptr;

		global_cl.read_idx++;
		global_cl.read_idx %= global_cl.max_records;

		global_cl.read_ptr =
			&global_cl.head[global_cl.read_idx];

		global_cl.sent_msgs_count++;
		idx++;
		if (idx >= MAX_RECORD_IN_SINGLE_EVT)
			break;
	}

	if (global_cl.hdd_cbks.wlan_connectivity_log_send_to_usr)
		global_cl.hdd_cbks.wlan_connectivity_log_send_to_usr(data, idx);

	qdf_mem_free(data);

	return QDF_STATUS_SUCCESS;
}
