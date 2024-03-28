/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

/**
 * DOC: This file contains chipset stats implementstion
 */

#ifdef WLAN_CHIPSET_STATS
#include <qdf_mem.h>
#include <qdf_types.h>
#include <qdf_status.h>
#include <qdf_trace.h>
#include <qdf_time.h>
#include <qdf_mc_timer.h>
#include <qdf_lock.h>
#include <qdf_str.h>
#include <qdf_module.h>
#include <wlan_nlink_common.h>
#include <wlan_cp_stats_chipset_stats.h>

struct chipset_stats cstats;
static struct cstats_node *gcstats_buffer[CSTATS_MAX_TYPE];

QDF_STATUS wlan_cp_stats_cstats_init(void)
{
	qdf_list_node_t *tmp_node = NULL;
	int i, j, k;

	for (i = 0; i < CSTATS_MAX_TYPE; i++) {
		qdf_spinlock_create(&cstats.cstats_lock[i]);

		gcstats_buffer[i] = qdf_mem_valloc(MAX_CSTATS_NODE_COUNT *
						   sizeof(struct cstats_node));
		if (!gcstats_buffer[i]) {
			qdf_err("Could not allocate memory for chipset stats");
			for (k = 0; k < i ; k++) {
				qdf_spin_lock_bh(&cstats.cstats_lock[k]);
				cstats.ccur_node[k] = NULL;
				qdf_spin_unlock_bh(&cstats.cstats_lock[k]);

				if (gcstats_buffer[k])
					qdf_mem_vfree(gcstats_buffer[k]);
			}

			return QDF_STATUS_E_NOMEM;
		}

		qdf_mem_zero(gcstats_buffer[i], MAX_CSTATS_NODE_COUNT *
			     sizeof(struct cstats_node));

		qdf_spin_lock_bh(&cstats.cstats_lock[i]);
		qdf_init_list_head(&cstats.cstat_free_list[i].anchor);
		qdf_init_list_head(&cstats.cstat_filled_list[i].anchor);

		for (j = 0; j < MAX_CSTATS_NODE_COUNT; j++) {
			qdf_list_insert_front(&cstats.cstat_free_list[i],
					      &gcstats_buffer[i][j].node);
			gcstats_buffer[i][j].index = j;
		}

		qdf_list_remove_front(&cstats.cstat_free_list[i], &tmp_node);

		cstats.ccur_node[i] =
			qdf_container_of(tmp_node, struct cstats_node, node);
		cstats.cstats_no_flush[i] = true;
		qdf_spin_unlock_bh(&cstats.cstats_lock[i]);
	}

	return 0;
}

void wlan_cp_stats_cstats_deinit(void)
{
	int i;

	for (i = 0; i < CSTATS_MAX_TYPE; i++) {
		qdf_spin_lock_bh(&cstats.cstats_lock[i]);
		cstats.ccur_node[i] = NULL;
		cstats.cstat_drop_cnt[i] = 0;
		qdf_spin_unlock_bh(&cstats.cstats_lock[i]);

		if (gcstats_buffer[i]) {
			qdf_mem_vfree(gcstats_buffer[i]);
			gcstats_buffer[i] = NULL;
		}
	}
}

void wlan_cp_stats_cstats_register_tx_rx_ops(struct cstats_tx_rx_ops *ops)
{
	cstats.ops.cstats_send_data_to_usr = ops->cstats_send_data_to_usr;
}

/* Need to call this with spin_lock acquired */
static void wlan_cp_stats_get_cstats_free_node(enum cstats_types type)
{
	qdf_list_node_t *tmp_node = NULL;

	if (cstats.ccur_node[type]->filled_length) {
		qdf_list_insert_back(&cstats.cstat_filled_list[type],
				     &cstats.ccur_node[type]->node);
	} else {
		return;
	}

	if (!qdf_list_empty(&cstats.cstat_free_list[type])) {
		qdf_list_remove_front(&cstats.cstat_free_list[type], &tmp_node);
		cstats.ccur_node[type] =
			qdf_container_of(tmp_node, struct cstats_node, node);
	} else if (!qdf_list_empty(&cstats.cstat_filled_list[type])) {
		qdf_list_remove_front(&cstats.cstat_filled_list[type],
				      &tmp_node);
		cstats.ccur_node[type] =
			qdf_container_of(tmp_node, struct cstats_node, node);
		qdf_err("Dropping a chipset stats node : count %d",
			++(cstats.cstat_drop_cnt[type]));
	}

	/* Reset the current node values */
	cstats.ccur_node[type]->filled_length = 0;
}

void wlan_cp_stats_cstats_write_to_buff(enum cstats_types type,
					void *to_be_sent,
					uint32_t plen)
{
	char *ptr;
	unsigned int *pfilled_length;
	unsigned int tlen;

	/* tAniNlHdr + Start Marker + End Marker */
	tlen = sizeof(tAniNlHdr) + (2 * CSTATS_MARKER_SZ);

	if ((tlen + plen) > MAX_CSTATS_NODE_LENGTH) {
		qdf_err("The Buffer is too long");
		return;
	}

	if (!cstats.ccur_node[type]) {
		qdf_err("Current Node is NULL");
		return;
	}

	qdf_spin_lock_bh(&cstats.cstats_lock[type]);

	pfilled_length = &cstats.ccur_node[type]->filled_length;

	/* Check if we can accommodate more log into current node/buffer */
	if ((MAX_CSTATS_NODE_LENGTH - *pfilled_length) < (tlen + plen)) {
		wlan_cp_stats_get_cstats_free_node(type);
		pfilled_length = &cstats.ccur_node[type]->filled_length;
	}

	if (type == CSTATS_HOST_TYPE) {
		/* Marker will be added while flushing to userspace*/
		ptr = &cstats.ccur_node[type]->logbuf[sizeof(tAniHdr) +
						      CSTATS_MARKER_SZ];
		memcpy(&ptr[*pfilled_length], to_be_sent, plen);
		*pfilled_length += plen;
	} else if (type == CSTATS_FW_TYPE) {
		ptr = &cstats.ccur_node[type]->logbuf[sizeof(tAniHdr)];
		memcpy(&ptr[*pfilled_length], CSTATS_FW_START_MARKER,
		       CSTATS_MARKER_SZ);
		memcpy(&ptr[*pfilled_length + CSTATS_MARKER_SZ], to_be_sent,
		       plen);
		memcpy(&ptr[*pfilled_length + CSTATS_MARKER_SZ + plen],
		       CSTATS_FW_END_MARKER, CSTATS_MARKER_SZ);
		*pfilled_length += (plen + 2 * CSTATS_MARKER_SZ);
	}

	qdf_spin_unlock_bh(&cstats.cstats_lock[type]);
}

static int wlan_cp_stats_cstats_send_version_to_usr(void)
{
	uint8_t buff[MAX_CSTATS_VERSION_BUFF_LENGTH] = {0};
	uint8_t n;
	int metadata_len;
	int ret = -1;

	metadata_len = sizeof(tAniHdr) + (2 * CSTATS_MARKER_SZ);

	memcpy(&buff[sizeof(tAniHdr)], CSTATS_HOST_START_MARKER,
	       CSTATS_MARKER_SZ);

	n = scnprintf(&buff[sizeof(tAniHdr) + CSTATS_MARKER_SZ],
		      MAX_CSTATS_VERSION_BUFF_LENGTH - metadata_len,
		      "[%s : %d, %s : %d, %s : %d]",
		      "Chispet stats - hdr_version",
		      CHIPSET_STATS_HDR_VERSION, "Endianness",
		      CHIPSET_STATS_MACHINE_ENDIANNESS, "Drop cnt",
		      cstats.cstat_drop_cnt[CSTATS_HOST_TYPE]);

	qdf_mem_copy(&buff[sizeof(tAniHdr) + CSTATS_MARKER_SZ + n],
		     CSTATS_HOST_END_MARKER, CSTATS_MARKER_SZ);

	buff[metadata_len + n] = '\0';

	if (cstats.ops.cstats_send_data_to_usr) {
		ret = cstats.ops.cstats_send_data_to_usr(buff,
							 metadata_len + n,
							 CSTATS_HOST_TYPE);
	}

	if (ret)
		qdf_err("failed to send version info");

	return ret;
}

int wlan_cp_stats_cstats_send_buffer_to_user(enum cstats_types type)
{
	int ret = -1;
	struct cstats_node *clog_msg;
	struct cstats_node *next;
	int payload_len;
	int mark_total;
	char *ptr = NULL;

	qdf_spin_lock_bh(&cstats.cstats_lock[type]);
	wlan_cp_stats_get_cstats_free_node(type);
	qdf_spin_unlock_bh(&cstats.cstats_lock[type]);

	if (type == CSTATS_HOST_TYPE) {
		ret = wlan_cp_stats_cstats_send_version_to_usr();
		if (ret)
			return ret;
	}

	/*
	 * For fw stats the markers are already added at start and end of the
	 * each event
	 */
	if (type == CSTATS_HOST_TYPE)
		mark_total = (2 * CSTATS_MARKER_SZ);
	else if (type == CSTATS_FW_TYPE)
		mark_total = 0;

	qdf_list_for_each_del(&cstats.cstat_filled_list[type],
			      clog_msg, next, node) {
		qdf_spin_lock_bh(&cstats.cstats_lock[type]);

		/* For host stats marksers are added per node basis*/
		if (type == CSTATS_HOST_TYPE) {
			ptr = &clog_msg->logbuf[sizeof(tAniHdr)];
			qdf_mem_copy(ptr, CSTATS_HOST_START_MARKER,
				     CSTATS_MARKER_SZ);
			ptr = &clog_msg->logbuf[sizeof(tAniHdr) +
						CSTATS_MARKER_SZ +
						clog_msg->filled_length];
			qdf_mem_copy(ptr, CSTATS_HOST_END_MARKER,
				     CSTATS_MARKER_SZ);
		}

		if (!cstats.cstats_no_flush[type]) {
			qdf_list_remove_node(&cstats.cstat_free_list[type],
					     &clog_msg->node);
		}

		qdf_spin_unlock_bh(&cstats.cstats_lock[type]);

		payload_len = clog_msg->filled_length + sizeof(tAniHdr) +
			      mark_total;

		if (cstats.ops.cstats_send_data_to_usr) {
			ret = cstats.ops.cstats_send_data_to_usr
			       (clog_msg->logbuf, payload_len, type);
		}

		if (ret) {
			qdf_err("Send Failed %d drop_count = %u", ret,
				++(cstats.cstat_drop_cnt[type]));
		}

		if (!cstats.cstats_no_flush[type]) {
			qdf_spin_lock_bh(&cstats.cstats_lock[type]);
			qdf_list_insert_back(&cstats.cstat_free_list[type],
					     &clog_msg->node);
			qdf_spin_unlock_bh(&cstats.cstats_lock[type]);
		}
	}

	return ret;
}
#endif /* WLAN_CHIPSET_STATS */
