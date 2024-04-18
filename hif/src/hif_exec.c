/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <hif_exec.h>
#include <ce_main.h>
#include "qdf_module.h"
#include "qdf_net_if.h"
#include <pld_common.h>
#ifdef DP_UMAC_HW_RESET_SUPPORT
#include "if_pci.h"
#endif
#include "qdf_ssr_driver_dump.h"

/* mapping NAPI budget 0 to internal budget 0
 * NAPI budget 1 to internal budget [1,scaler -1]
 * NAPI budget 2 to internal budget [scaler, 2 * scaler - 1], etc
 */
#define NAPI_BUDGET_TO_INTERNAL_BUDGET(n, s) \
	(((n) << (s)) - 1)
#define INTERNAL_BUDGET_TO_NAPI_BUDGET(n, s) \
	(((n) + 1) >> (s))

static struct hif_exec_context *hif_exec_tasklet_create(void);

#ifdef WLAN_FEATURE_DP_EVENT_HISTORY
struct hif_event_history hif_event_desc_history[HIF_NUM_INT_CONTEXTS];
uint32_t hif_event_hist_max = HIF_EVENT_HIST_MAX;

void hif_desc_history_log_register(void)
{
	qdf_ssr_driver_dump_register_region("hif_event_history",
					    hif_event_desc_history,
					    sizeof(hif_event_desc_history));
	qdf_ssr_driver_dump_register_region("hif_event_hist_max",
					    &hif_event_hist_max,
					    sizeof(hif_event_hist_max));
}

void hif_desc_history_log_unregister(void)
{
	qdf_ssr_driver_dump_unregister_region("hif_event_hist_max");
	qdf_ssr_driver_dump_unregister_region("hif_event_history");
}

static inline
int hif_get_next_record_index(qdf_atomic_t *table_index,
			      int array_size)
{
	int record_index = qdf_atomic_inc_return(table_index);

	return record_index & (array_size - 1);
}

/**
 * hif_hist_is_prev_record() - Check if index is the immediate
 *  previous record wrt curr_index
 * @curr_index: curr index in the event history
 * @index: index to be checked
 * @hist_size: history size
 *
 * Return: true if index is immediately behind curr_index else false
 */
static inline
bool hif_hist_is_prev_record(int32_t curr_index, int32_t index,
			     uint32_t hist_size)
{
	return (((index + 1) & (hist_size - 1)) == curr_index) ?
			true : false;
}

/**
 * hif_hist_skip_event_record() - Check if current event needs to be
 *  recorded or not
 * @hist_ev: HIF event history
 * @event: DP event entry
 *
 * Return: true if current event needs to be skipped else false
 */
static bool
hif_hist_skip_event_record(struct hif_event_history *hist_ev,
			   struct hif_event_record *event)
{
	struct hif_event_record *rec;
	struct hif_event_record *last_irq_rec;
	int32_t index;

	index = qdf_atomic_read(&hist_ev->index);
	if (index < 0)
		return false;

	index &= (HIF_EVENT_HIST_MAX - 1);
	rec = &hist_ev->event[index];

	switch (event->type) {
	case HIF_EVENT_IRQ_TRIGGER:
		/*
		 * The prev record check is to prevent skipping the IRQ event
		 * record in case where BH got re-scheduled due to force_break
		 * but there are no entries to be reaped in the rings.
		 */
		if (rec->type == HIF_EVENT_BH_SCHED &&
		    hif_hist_is_prev_record(index,
					    hist_ev->misc.last_irq_index,
					    HIF_EVENT_HIST_MAX)) {
			last_irq_rec =
				&hist_ev->event[hist_ev->misc.last_irq_index];
			last_irq_rec->timestamp = hif_get_log_timestamp();
			last_irq_rec->cpu_id = qdf_get_cpu();
			last_irq_rec->hp++;
			last_irq_rec->tp = last_irq_rec->timestamp -
						hist_ev->misc.last_irq_ts;
			return true;
		}
		break;
	case HIF_EVENT_BH_SCHED:
		if (rec->type == HIF_EVENT_BH_SCHED) {
			rec->timestamp = hif_get_log_timestamp();
			rec->cpu_id = qdf_get_cpu();
			return true;
		}
		break;
	case HIF_EVENT_SRNG_ACCESS_START:
		if (event->hp == event->tp)
			return true;
		break;
	case HIF_EVENT_SRNG_ACCESS_END:
		if (rec->type != HIF_EVENT_SRNG_ACCESS_START)
			return true;
		break;
	case HIF_EVENT_BH_COMPLETE:
	case HIF_EVENT_BH_FORCE_BREAK:
		if (rec->type != HIF_EVENT_SRNG_ACCESS_END)
			return true;
		break;
	default:
		break;
	}

	return false;
}

void hif_hist_record_event(struct hif_opaque_softc *hif_ctx,
			   struct hif_event_record *event, uint8_t intr_grp_id)
{
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ctx);
	struct hif_event_history *hist_ev;
	struct hif_event_record *record;
	int record_index;

	if (!(scn->event_enable_mask & BIT(event->type)))
		return;

	if (qdf_unlikely(intr_grp_id >= HIF_NUM_INT_CONTEXTS)) {
		hif_err("Invalid interrupt group id %d", intr_grp_id);
		return;
	}

	hist_ev = scn->evt_hist[intr_grp_id];
	if (qdf_unlikely(!hist_ev))
		return;

	if (hif_hist_skip_event_record(hist_ev, event))
		return;

	record_index = hif_get_next_record_index(
			&hist_ev->index, HIF_EVENT_HIST_MAX);

	record = &hist_ev->event[record_index];

	if (event->type == HIF_EVENT_IRQ_TRIGGER) {
		hist_ev->misc.last_irq_index = record_index;
		hist_ev->misc.last_irq_ts = hif_get_log_timestamp();
	}

	record->hal_ring_id = event->hal_ring_id;
	record->hp = event->hp;
	record->tp = event->tp;
	record->cpu_id = qdf_get_cpu();
	record->timestamp = hif_get_log_timestamp();
	record->type = event->type;
}

void hif_event_history_init(struct hif_opaque_softc *hif_ctx, uint8_t id)
{
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ctx);

	scn->evt_hist[id] = &hif_event_desc_history[id];
	qdf_atomic_set(&scn->evt_hist[id]->index, -1);

	hif_info("SRNG events history initialized for group: %d", id);
}

void hif_event_history_deinit(struct hif_opaque_softc *hif_ctx, uint8_t id)
{
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ctx);

	scn->evt_hist[id] = NULL;
	hif_info("SRNG events history de-initialized for group: %d", id);
}
#endif /* WLAN_FEATURE_DP_EVENT_HISTORY */

#ifndef QCA_WIFI_WCN6450
/**
 * hif_print_napi_latency_stats() - print NAPI scheduling latency stats
 * @hif_state: hif context
 *
 * return: void
 */
#ifdef HIF_LATENCY_PROFILE_ENABLE
static void hif_print_napi_latency_stats(struct HIF_CE_state *hif_state)
{
	struct hif_exec_context *hif_ext_group;
	int i, j;
	int64_t cur_tstamp;

	const char time_str[HIF_SCHED_LATENCY_BUCKETS][15] =  {
		"0-2   ms",
		"3-10  ms",
		"11-20 ms",
		"21-50 ms",
		"51-100 ms",
		"101-250 ms",
		"251-500 ms",
		"> 500 ms"
	};

	cur_tstamp = qdf_ktime_to_ms(qdf_ktime_get());

	QDF_TRACE(QDF_MODULE_ID_HIF, QDF_TRACE_LEVEL_INFO_HIGH,
		  "Current timestamp: %lld", cur_tstamp);

	for (i = 0; i < hif_state->hif_num_extgroup; i++) {
		if (hif_state->hif_ext_group[i]) {
			hif_ext_group = hif_state->hif_ext_group[i];

			QDF_TRACE(QDF_MODULE_ID_HIF, QDF_TRACE_LEVEL_INFO_HIGH,
				  "ext grp %d Last serviced timestamp: %lld",
				  i, hif_ext_group->tstamp);

			QDF_TRACE(QDF_MODULE_ID_HIF, QDF_TRACE_LEVEL_INFO_HIGH,
				  "Latency Bucket     | Time elapsed");

			for (j = 0; j < HIF_SCHED_LATENCY_BUCKETS; j++) {
				if (hif_ext_group->sched_latency_stats[j])
					QDF_TRACE(QDF_MODULE_ID_HIF,
						  QDF_TRACE_LEVEL_INFO_HIGH,
						  "%s     |    %lld",
						  time_str[j],
						  hif_ext_group->
						  sched_latency_stats[j]);
			}
		}
	}
}
#else
static void hif_print_napi_latency_stats(struct HIF_CE_state *hif_state)
{
}
#endif

/**
 * hif_clear_napi_stats() - reset NAPI stats
 * @hif_ctx: hif context
 *
 * return: void
 */
void hif_clear_napi_stats(struct hif_opaque_softc *hif_ctx)
{
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(hif_ctx);
	struct hif_exec_context *hif_ext_group;
	size_t i;

	for (i = 0; i < hif_state->hif_num_extgroup; i++) {
		hif_ext_group = hif_state->hif_ext_group[i];

		if (!hif_ext_group)
			return;

		qdf_mem_set(hif_ext_group->sched_latency_stats,
			    sizeof(hif_ext_group->sched_latency_stats),
			    0x0);
	}
}

qdf_export_symbol(hif_clear_napi_stats);

#ifdef WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT
/**
 * hif_get_poll_times_hist_str() - Get HIF poll times histogram string
 * @stats: NAPI stats to get poll time buckets
 * @buf: buffer to fill histogram string
 * @buf_len: length of the buffer
 *
 * Return: void
 */
static void hif_get_poll_times_hist_str(struct qca_napi_stat *stats, char *buf,
					uint8_t buf_len)
{
	int i;
	int str_index = 0;

	for (i = 0; i < QCA_NAPI_NUM_BUCKETS; i++)
		str_index += qdf_scnprintf(buf + str_index, buf_len - str_index,
					   "%u|", stats->poll_time_buckets[i]);
}

void hif_print_napi_stats(struct hif_opaque_softc *hif_ctx)
{
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(hif_ctx);
	struct hif_exec_context *hif_ext_group;
	struct qca_napi_stat *napi_stats;
	int i, j;

	/*
	 * Max value of uint_32 (poll_time_bucket) = 4294967295
	 * Thus we need 10 chars + 1 space =11 chars for each bucket value.
	 * +1 space for '\0'.
	 */
	char hist_str[(QCA_NAPI_NUM_BUCKETS * 11) + 1] = {'\0'};

	QDF_TRACE(QDF_MODULE_ID_HIF, QDF_TRACE_LEVEL_INFO_HIGH,
		  "NAPI[#]CPU[#] |scheds |polls  |comps  |dones  |t-lim  |max(us)|hist(500us buckets)");

	for (i = 0;
	     (i < hif_state->hif_num_extgroup && hif_state->hif_ext_group[i]);
	     i++) {
		hif_ext_group = hif_state->hif_ext_group[i];
		for (j = 0; j < num_possible_cpus(); j++) {
			napi_stats = &hif_ext_group->stats[j];
			if (!napi_stats->napi_schedules)
				continue;

			hif_get_poll_times_hist_str(napi_stats,
						    hist_str,
						    sizeof(hist_str));
			QDF_TRACE(QDF_MODULE_ID_HIF,
				  QDF_TRACE_LEVEL_INFO_HIGH,
				  "NAPI[%d]CPU[%d]: %7u %7u %7u %7u %7u %7llu %s",
				  i, j,
				  napi_stats->napi_schedules,
				  napi_stats->napi_polls,
				  napi_stats->napi_completes,
				  napi_stats->napi_workdone,
				  napi_stats->time_limit_reached,
				  qdf_do_div(napi_stats->napi_max_poll_time,
					     1000),
				  hist_str);
		}
	}

	hif_print_napi_latency_stats(hif_state);
}

qdf_export_symbol(hif_print_napi_stats);
#else
static inline
void hif_get_poll_times_hist_str(struct qca_napi_stat *stats, char *buf,
				 uint8_t buf_len)
{
}

void hif_print_napi_stats(struct hif_opaque_softc *hif_ctx)
{
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(hif_ctx);
	struct hif_exec_context *hif_ext_group;
	struct qca_napi_stat *napi_stats;
	int i, j;

	QDF_TRACE(QDF_MODULE_ID_HIF, QDF_TRACE_LEVEL_FATAL,
		"NAPI[#ctx]CPU[#] |schedules |polls |completes |workdone");

	for (i = 0; i < hif_state->hif_num_extgroup; i++) {
		if (hif_state->hif_ext_group[i]) {
			hif_ext_group = hif_state->hif_ext_group[i];
			for (j = 0; j < num_possible_cpus(); j++) {
				napi_stats = &(hif_ext_group->stats[j]);
				if (napi_stats->napi_schedules != 0)
					QDF_TRACE(QDF_MODULE_ID_HIF,
						QDF_TRACE_LEVEL_FATAL,
						"NAPI[%2d]CPU[%d]: "
						"%7d %7d %7d %7d ",
						i, j,
						napi_stats->napi_schedules,
						napi_stats->napi_polls,
						napi_stats->napi_completes,
						napi_stats->napi_workdone);
			}
		}
	}

	hif_print_napi_latency_stats(hif_state);
}
qdf_export_symbol(hif_print_napi_stats);
#endif /* WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT */
#endif /* QCA_WIFI_WCN6450 */

#if defined(WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT) || \
	defined(WLAN_DP_LOAD_BALANCE_SUPPORT)
/**
 * hif_exec_update_service_start_time() - Update NAPI poll start time
 * @hif_ext_group: hif_ext_group of type NAPI
 *
 * The function is called at the beginning of a NAPI poll to record the poll
 * start time.
 *
 * Return: None
 */
static inline void
hif_exec_update_service_start_time(struct hif_exec_context *hif_ext_group)
{
	hif_ext_group->poll_start_time = qdf_time_sched_clock();
}
#else
static inline void
hif_exec_update_service_start_time(struct hif_exec_context *hif_ext_group)
{
}
#endif

#ifdef WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT
/**
 * hif_exec_fill_poll_time_histogram() - fills poll time histogram for a NAPI
 * @hif_ext_group: hif_ext_group of type NAPI
 *
 * The function is called at the end of a NAPI poll to calculate poll time
 * buckets.
 *
 * Return: void
 */
static
void hif_exec_fill_poll_time_histogram(struct hif_exec_context *hif_ext_group)
{
	struct qca_napi_stat *napi_stat;
	unsigned long long poll_time_ns;
	uint32_t poll_time_us;
	uint32_t bucket_size_us = 500;
	uint32_t bucket;
	uint32_t cpu_id = qdf_get_cpu();

	poll_time_ns = qdf_time_sched_clock() - hif_ext_group->poll_start_time;
	poll_time_us = qdf_do_div(poll_time_ns, 1000);

	napi_stat = &hif_ext_group->stats[cpu_id];
	if (poll_time_ns > hif_ext_group->stats[cpu_id].napi_max_poll_time)
		hif_ext_group->stats[cpu_id].napi_max_poll_time = poll_time_ns;

	bucket = poll_time_us / bucket_size_us;
	if (bucket >= QCA_NAPI_NUM_BUCKETS)
		bucket = QCA_NAPI_NUM_BUCKETS - 1;
	++napi_stat->poll_time_buckets[bucket];
}

/**
 * hif_exec_poll_should_yield() - Local function deciding if NAPI should yield
 * @hif_ext_group: hif_ext_group of type NAPI
 *
 * Return: true if NAPI needs to yield, else false
 */
static bool hif_exec_poll_should_yield(struct hif_exec_context *hif_ext_group)
{
	bool time_limit_reached = false;
	unsigned long long poll_time_ns;
	int cpu_id = qdf_get_cpu();
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ext_group->hif);
	struct hif_config_info *cfg = &scn->hif_config;

	poll_time_ns = qdf_time_sched_clock() - hif_ext_group->poll_start_time;
	time_limit_reached =
		poll_time_ns > cfg->rx_softirq_max_yield_duration_ns ? 1 : 0;

	if (time_limit_reached) {
		hif_ext_group->stats[cpu_id].time_limit_reached++;
		hif_ext_group->force_break = true;
	}

	return time_limit_reached;
}

bool hif_exec_should_yield(struct hif_opaque_softc *hif_ctx, uint grp_id)
{
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ctx);
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(scn);
	struct hif_exec_context *hif_ext_group;
	bool ret_val = false;

	if (!(grp_id < hif_state->hif_num_extgroup) ||
	    !(grp_id < HIF_MAX_GROUP))
		return false;

	hif_ext_group = hif_state->hif_ext_group[grp_id];

	if (hif_ext_group->type == HIF_EXEC_NAPI_TYPE)
		ret_val = hif_exec_poll_should_yield(hif_ext_group);

	return ret_val;
}
#else
static inline
void hif_exec_fill_poll_time_histogram(struct hif_exec_context *hif_ext_group)
{
}
#endif /* WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT */

static void hif_exec_tasklet_schedule(struct hif_exec_context *ctx)
{
	struct hif_tasklet_exec_context *t_ctx = hif_exec_get_tasklet(ctx);

	tasklet_schedule(&t_ctx->tasklet);
}

/**
 * hif_exec_tasklet_fn() - grp tasklet
 * @data: context
 *
 * Return: void
 */
static void hif_exec_tasklet_fn(unsigned long data)
{
	struct hif_exec_context *hif_ext_group =
			(struct hif_exec_context *)data;
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ext_group->hif);
	unsigned int work_done;
	int cpu = smp_processor_id();

	work_done =
		hif_ext_group->handler(hif_ext_group->context, HIF_MAX_BUDGET,
				       cpu);

	if (hif_ext_group->work_complete(hif_ext_group, work_done)) {
		qdf_atomic_dec(&(scn->active_grp_tasklet_cnt));
		hif_ext_group->irq_enable(hif_ext_group);
	} else {
		hif_exec_tasklet_schedule(hif_ext_group);
	}
}

/**
 * hif_latency_profile_measure() - calculate latency and update histogram
 * @hif_ext_group: hif exec context
 *
 * Return: None
 */
#ifdef HIF_LATENCY_PROFILE_ENABLE
static void hif_latency_profile_measure(struct hif_exec_context *hif_ext_group)
{
	int64_t cur_tstamp;
	int64_t time_elapsed;

	cur_tstamp = qdf_ktime_to_ms(qdf_ktime_get());

	if (cur_tstamp > hif_ext_group->tstamp)
		time_elapsed = (cur_tstamp - hif_ext_group->tstamp);
	else
		time_elapsed = ~0x0 - (hif_ext_group->tstamp - cur_tstamp);

	hif_ext_group->tstamp = cur_tstamp;

	if (time_elapsed <= HIF_SCHED_LATENCY_BUCKET_0_2)
		hif_ext_group->sched_latency_stats[0]++;
	else if (time_elapsed <= HIF_SCHED_LATENCY_BUCKET_3_10)
		hif_ext_group->sched_latency_stats[1]++;
	else if (time_elapsed <= HIF_SCHED_LATENCY_BUCKET_11_20)
		hif_ext_group->sched_latency_stats[2]++;
	else if (time_elapsed <= HIF_SCHED_LATENCY_BUCKET_21_50)
		hif_ext_group->sched_latency_stats[3]++;
	else if (time_elapsed <= HIF_SCHED_LATENCY_BUCKET_51_100)
		hif_ext_group->sched_latency_stats[4]++;
	else if (time_elapsed <= HIF_SCHED_LATENCY_BUCKET_101_250)
		hif_ext_group->sched_latency_stats[5]++;
	else if (time_elapsed <= HIF_SCHED_LATENCY_BUCKET_251_500)
		hif_ext_group->sched_latency_stats[6]++;
	else
		hif_ext_group->sched_latency_stats[7]++;
}
#else
static inline
void hif_latency_profile_measure(struct hif_exec_context *hif_ext_group)
{
}
#endif

/**
 * hif_latency_profile_start() - Update the start timestamp for HIF ext group
 * @hif_ext_group: hif exec context
 *
 * Return: None
 */
#ifdef HIF_LATENCY_PROFILE_ENABLE
static void hif_latency_profile_start(struct hif_exec_context *hif_ext_group)
{
	hif_ext_group->tstamp = qdf_ktime_to_ms(qdf_ktime_get());
}
#else
static inline
void hif_latency_profile_start(struct hif_exec_context *hif_ext_group)
{
}
#endif

#ifdef FEATURE_NAPI
#ifdef FEATURE_IRQ_AFFINITY
static inline int32_t
hif_is_force_napi_complete_required(struct hif_exec_context *hif_ext_group)
{
	return qdf_atomic_inc_not_zero(&hif_ext_group->force_napi_complete);
}
#else
static inline int32_t
hif_is_force_napi_complete_required(struct hif_exec_context *hif_ext_group)
{
	return 0;
}
#endif

/**
 * hif_irq_disabled_time_limit_reached() - determine if irq disabled limit
 * reached for single MSI
 * @hif_ext_group: hif exec context
 *
 * Return: true if reached, else false.
 */
static bool
hif_irq_disabled_time_limit_reached(struct hif_exec_context *hif_ext_group)
{
	unsigned long long irq_disabled_duration_ns;

	if (hif_ext_group->type != HIF_EXEC_NAPI_TYPE)
		return false;

	irq_disabled_duration_ns = qdf_time_sched_clock() -
					hif_ext_group->irq_disabled_start_time;
	if (irq_disabled_duration_ns >= IRQ_DISABLED_MAX_DURATION_NS) {
		hif_record_event(hif_ext_group->hif, hif_ext_group->grp_id,
				 0, 0, 0, HIF_EVENT_IRQ_DISABLE_EXPIRED);
		return true;
	}

	return false;
}

#ifdef WLAN_DP_LOAD_BALANCE_SUPPORT
/**
 * hif_exec_update_soft_irq_time() - update the time spent in softirq processing
 * @hif_ext_group: hif exec context
 *
 * Return: none
 */
static inline void
hif_exec_update_soft_irq_time(struct hif_exec_context *hif_ext_group)
{
	int cpu = qdf_get_cpu();
	uint64_t time;

	time = qdf_time_sched_clock() - hif_ext_group->poll_start_time;
	if (qdf_get_current() != qdf_this_cpu_ksoftirqd())
		hif_ext_group->total_irq_time[cpu] += time;
	else
		hif_ext_group->ksoftirqd_time[cpu] += time;
}

/**
 * hif_update_irq_handler_start_time() - log the time at which irq
 * handling started
 * @hif_ext_group: hif exec context
 *
 * Return: none
 */
static inline void
hif_update_irq_handler_start_time(struct hif_exec_context *hif_ext_group)
{
	hif_ext_group->irq_start_time = qdf_time_sched_clock();
}

/**
 * hif_update_irq_handle_time() - update the time spent in irq processing
 * @hif_ext_group: hif exec context
 *
 * Return: none
 */
static inline void
hif_update_irq_handle_time(struct hif_exec_context *hif_ext_group)
{
	uint64_t cur_time = qdf_time_sched_clock();
	int cpu = qdf_get_cpu();

	hif_ext_group->total_irq_time[cpu] += cur_time -
					hif_ext_group->irq_start_time;
}

/**
 * hif_get_wlan_rx_time_stats - function to return wlan rx time statistics
 * @hif_ctx: hif handle
 * @wlan_irq_time: Array of NR_CPUS to get the wlan rx napi execution time
 * @wlan_ksoftirqd_time: Array of NR_CPUS to get the wlan ksoftirqd execution
 * time
 *
 * Return: none
 */
void hif_get_wlan_rx_time_stats(struct hif_opaque_softc *hif_ctx,
				uint64_t *wlan_irq_time,
				uint64_t *wlan_ksoftirqd_time)
{
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(hif_ctx);
	struct hif_exec_context *hif_ext_group;
	int i, grp;

	for (i = 0; i < num_possible_cpus(); i++) {
		for (grp = 0; grp < hif_state->hif_num_extgroup; grp++) {
			hif_ext_group = hif_state->hif_ext_group[grp];
			if (hif_ext_group) {
				wlan_irq_time[i] +=
					hif_ext_group->total_irq_time[i];
				wlan_ksoftirqd_time[i] +=
					hif_ext_group->ksoftirqd_time[i];
			}
		}
	}
}

/**
 * hif_check_and_apply_irq_affinity() - function to affine the hif group
 * to the given cpu if it's affined to a different CPU.
 *
 * @hif_ctx: hif context
 * @grp_id: hif group id
 * @cpu_id: cpu id
 *
 * Return: none
 */
void hif_check_and_apply_irq_affinity(struct hif_opaque_softc *hif_ctx,
				      uint8_t grp_id, uint32_t cpu_id)
{
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ctx);
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(scn);
	struct hif_exec_context *hif_ext_group;
	qdf_cpu_mask cpu_mask;
	QDF_STATUS status;
	int i;

	qdf_cpumask_clear(&cpu_mask);
	qdf_cpumask_set_cpu(cpu_id, &cpu_mask);

	if (grp_id >= hif_state->hif_num_extgroup) {
		hif_debug("Invalid group id received %d", grp_id);
		return;
	}

	hif_ext_group = hif_state->hif_ext_group[grp_id];

	for (i = 0; i < hif_ext_group->numirq; i++) {
		if (qdf_cpumask_equal(&hif_ext_group->new_cpu_mask[i],
				      &cpu_mask)) {
			hif_debug("Skip affining same cpu for grp_id %d cpu_mask %*pbl",
				  hif_ext_group->grp_id,
				  qdf_cpumask_pr_args(&cpu_mask));
			continue;
		}

		status = hif_irq_set_affinity_hint(hif_ext_group->os_irq[i],
						   &cpu_mask);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			qdf_cpumask_copy(&hif_ext_group->new_cpu_mask[i],
					 &cpu_mask);
			qdf_atomic_set(&hif_ext_group->force_napi_complete, -1);
			hif_debug("Affined IRQ %d to cpu_mask %*pbl",
				  hif_ext_group->os_irq[i],
				  qdf_cpumask_pr_args(&hif_ext_group->new_cpu_mask[i]));
		} else {
			hif_err("set affinity failed for IRQ %d",
				hif_ext_group->os_irq[i]);
		}
	}
}
#else
static inline void
hif_exec_update_soft_irq_time(struct hif_exec_context *hif_ext_group)
{
}

static inline void
hif_update_irq_handler_start_time(struct hif_exec_context *hif_ext_group)
{
}

static inline void
hif_update_irq_handle_time(struct hif_exec_context *hif_ext_group)
{
}
#endif /* WLAN_DP_LOAD_BALANCE_SUPPORT */

/**
 * hif_exec_poll() - napi poll
 * @napi: napi struct
 * @budget: budget for napi
 *
 * Return: mapping of internal budget to napi
 */
static int hif_exec_poll(struct napi_struct *napi, int budget)
{
	struct hif_napi_exec_context *napi_exec_ctx =
		    qdf_container_of(napi, struct hif_napi_exec_context, napi);
	struct hif_exec_context *hif_ext_group = &napi_exec_ctx->exec_ctx;
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ext_group->hif);
	int work_done;
	int normalized_budget = 0;
	int actual_dones;
	int shift = hif_ext_group->scale_bin_shift;
	int cpu = smp_processor_id();
	bool force_complete = false;

	hif_record_event(hif_ext_group->hif, hif_ext_group->grp_id,
			 0, 0, 0, HIF_EVENT_BH_SCHED);

	hif_ext_group->force_break = false;
	hif_exec_update_service_start_time(hif_ext_group);

	if (budget)
		normalized_budget = NAPI_BUDGET_TO_INTERNAL_BUDGET(budget, shift);

	hif_latency_profile_measure(hif_ext_group);

	work_done = hif_ext_group->handler(hif_ext_group->context,
					   normalized_budget, cpu);

	actual_dones = work_done;

	if (hif_is_force_napi_complete_required(hif_ext_group)) {
		force_complete = true;
		if (work_done >= normalized_budget)
			work_done = normalized_budget - 1;
	}

	if (qdf_unlikely(force_complete) ||
	    (!hif_ext_group->force_break && work_done < normalized_budget) ||
	    ((pld_is_one_msi(scn->qdf_dev->dev) &&
	    hif_irq_disabled_time_limit_reached(hif_ext_group)))) {
		hif_record_event(hif_ext_group->hif, hif_ext_group->grp_id,
				 0, 0, 0, HIF_EVENT_BH_COMPLETE);
		napi_complete(napi);
		qdf_atomic_dec(&scn->active_grp_tasklet_cnt);
		hif_ext_group->irq_enable(hif_ext_group);
		hif_ext_group->stats[cpu].napi_completes++;
	} else {
		/* if the ext_group supports time based yield, claim full work
		 * done anyways */
		hif_record_event(hif_ext_group->hif, hif_ext_group->grp_id,
				 0, 0, 0, HIF_EVENT_BH_FORCE_BREAK);
		work_done = normalized_budget;
	}

	hif_ext_group->stats[cpu].napi_polls++;
	hif_ext_group->stats[cpu].napi_workdone += actual_dones;

	/* map internal budget to NAPI budget */
	if (work_done)
		work_done = INTERNAL_BUDGET_TO_NAPI_BUDGET(work_done, shift);

	hif_exec_fill_poll_time_histogram(hif_ext_group);
	hif_exec_update_soft_irq_time(hif_ext_group);

	return work_done;
}

/**
 * hif_exec_napi_schedule() - schedule the napi exec instance
 * @ctx: a hif_exec_context known to be of napi type
 */
static void hif_exec_napi_schedule(struct hif_exec_context *ctx)
{
	struct hif_napi_exec_context *n_ctx = hif_exec_get_napi(ctx);
	ctx->stats[smp_processor_id()].napi_schedules++;

	napi_schedule(&n_ctx->napi);
}

/**
 * hif_exec_napi_kill() - stop a napi exec context from being rescheduled
 * @ctx: a hif_exec_context known to be of napi type
 */
static void hif_exec_napi_kill(struct hif_exec_context *ctx)
{
	struct hif_napi_exec_context *n_ctx = hif_exec_get_napi(ctx);
	int irq_ind;

	if (ctx->inited) {
		qdf_napi_disable(&n_ctx->napi);
		ctx->inited = 0;
	}

	for (irq_ind = 0; irq_ind < ctx->numirq; irq_ind++)
		hif_irq_affinity_remove(ctx->os_irq[irq_ind]);

	hif_core_ctl_set_boost(false);
	qdf_netif_napi_del(&(n_ctx->napi));
}

struct hif_execution_ops napi_sched_ops = {
	.schedule = &hif_exec_napi_schedule,
	.kill = &hif_exec_napi_kill,
};

/**
 * hif_exec_napi_create() - allocate and initialize a napi exec context
 * @scale: a binary shift factor to map NAPI budget from\to internal
 *         budget
 */
static struct hif_exec_context *hif_exec_napi_create(uint32_t scale)
{
	struct hif_napi_exec_context *ctx;

	ctx = qdf_mem_malloc(sizeof(struct hif_napi_exec_context));
	if (!ctx)
		return NULL;

	ctx->exec_ctx.sched_ops = &napi_sched_ops;
	ctx->exec_ctx.inited = true;
	ctx->exec_ctx.scale_bin_shift = scale;
	qdf_net_if_create_dummy_if((struct qdf_net_if *)&ctx->netdev);
	qdf_netif_napi_add(&(ctx->netdev), &(ctx->napi), hif_exec_poll,
			   QCA_NAPI_BUDGET);
	qdf_napi_enable(&ctx->napi);

	return &ctx->exec_ctx;
}
#else
static struct hif_exec_context *hif_exec_napi_create(uint32_t scale)
{
	hif_warn("FEATURE_NAPI not defined, making tasklet");
	return hif_exec_tasklet_create();
}
#endif


/**
 * hif_exec_tasklet_kill() - stop a tasklet exec context from being rescheduled
 * @ctx: a hif_exec_context known to be of tasklet type
 */
static void hif_exec_tasklet_kill(struct hif_exec_context *ctx)
{
	struct hif_tasklet_exec_context *t_ctx = hif_exec_get_tasklet(ctx);
	int irq_ind;

	if (ctx->inited) {
		tasklet_disable(&t_ctx->tasklet);
		tasklet_kill(&t_ctx->tasklet);
	}
	ctx->inited = false;

	for (irq_ind = 0; irq_ind < ctx->numirq; irq_ind++)
		hif_irq_affinity_remove(ctx->os_irq[irq_ind]);
}

struct hif_execution_ops tasklet_sched_ops = {
	.schedule = &hif_exec_tasklet_schedule,
	.kill = &hif_exec_tasklet_kill,
};

/**
 * hif_exec_tasklet_create() -  allocate and initialize a tasklet exec context
 */
static struct hif_exec_context *hif_exec_tasklet_create(void)
{
	struct hif_tasklet_exec_context *ctx;

	ctx = qdf_mem_malloc(sizeof(struct hif_tasklet_exec_context));
	if (!ctx)
		return NULL;

	ctx->exec_ctx.sched_ops = &tasklet_sched_ops;
	tasklet_init(&ctx->tasklet, hif_exec_tasklet_fn,
		     (unsigned long)ctx);

	ctx->exec_ctx.inited = true;

	return &ctx->exec_ctx;
}

/**
 * hif_exec_get_ctx() - retrieve an exec context based on an id
 * @softc: the hif context owning the exec context
 * @id: the id of the exec context
 *
 * mostly added to make it easier to rename or move the context array
 */
struct hif_exec_context *hif_exec_get_ctx(struct hif_opaque_softc *softc,
					  uint8_t id)
{
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(softc);

	if (id < hif_state->hif_num_extgroup)
		return hif_state->hif_ext_group[id];

	return NULL;
}

int32_t hif_get_int_ctx_irq_num(struct hif_opaque_softc *softc,
				uint8_t id)
{
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(softc);

	if (id < hif_state->hif_num_extgroup)
		return hif_state->hif_ext_group[id]->os_irq[0];
	return -EINVAL;
}

qdf_export_symbol(hif_get_int_ctx_irq_num);

QDF_STATUS hif_configure_ext_group_interrupts(struct hif_opaque_softc *hif_ctx)
{
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ctx);
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(hif_ctx);
	struct hif_exec_context *hif_ext_group;
	int i, status;

	if (scn->ext_grp_irq_configured) {
		hif_err("Called after ext grp irq configured");
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < hif_state->hif_num_extgroup; i++) {
		hif_ext_group = hif_state->hif_ext_group[i];
		status = 0;
		qdf_spinlock_create(&hif_ext_group->irq_lock);
		if (hif_ext_group->configured &&
		    hif_ext_group->irq_requested == false) {
			hif_ext_group->irq_enabled = true;
			status = hif_grp_irq_configure(scn, hif_ext_group);
		}
		if (status != 0) {
			hif_err("Failed for group %d", i);
			hif_ext_group->irq_enabled = false;
		}
	}

	scn->ext_grp_irq_configured = true;

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(hif_configure_ext_group_interrupts);

void hif_deconfigure_ext_group_interrupts(struct hif_opaque_softc *hif_ctx)
{
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ctx);

	if (!scn || !scn->ext_grp_irq_configured) {
		hif_err("scn(%pk) is NULL or grp irq not configured", scn);
		return;
	}

	hif_grp_irq_deconfigure(scn);
	scn->ext_grp_irq_configured = false;
}

qdf_export_symbol(hif_deconfigure_ext_group_interrupts);

#ifdef WLAN_SUSPEND_RESUME_TEST
/**
 * hif_check_and_trigger_ut_resume() - check if unit-test command was used to
 *				       to trigger fake-suspend command, if yes
 *				       then issue resume procedure.
 * @scn: opaque HIF software context
 *
 * This API checks if unit-test command was used to trigger fake-suspend command
 * and if answer is yes then it would trigger resume procedure.
 *
 * Make this API inline to save API-switch overhead and do branch-prediction to
 * optimize performance impact.
 *
 * Return: void
 */
static inline void hif_check_and_trigger_ut_resume(struct hif_softc *scn)
{
	if (qdf_unlikely(hif_irq_trigger_ut_resume(scn)))
		hif_ut_fw_resume(scn);
}
#else
static inline void hif_check_and_trigger_ut_resume(struct hif_softc *scn)
{
}
#endif

/**
 * hif_check_and_trigger_sys_resume() - Check for bus suspend and
 *  trigger system resume
 * @scn: hif context
 * @irq: irq number
 *
 * Return: None
 */
static inline void
hif_check_and_trigger_sys_resume(struct hif_softc *scn, int irq)
{
	if (scn->bus_suspended && scn->linkstate_vote) {
		hif_info_rl("interrupt rcvd:%d trigger sys resume", irq);
		qdf_pm_system_wakeup();
	}
}

/**
 * hif_ext_group_interrupt_handler() - handler for related interrupts
 * @irq: irq number of the interrupt
 * @context: the associated hif_exec_group context
 *
 * This callback function takes care of disabling the associated interrupts
 * and scheduling the expected bottom half for the exec_context.
 * This callback function also helps keep track of the count running contexts.
 */
irqreturn_t hif_ext_group_interrupt_handler(int irq, void *context)
{
	struct hif_exec_context *hif_ext_group = context;
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ext_group->hif);

	if (hif_ext_group->irq_requested) {
		hif_update_irq_handler_start_time(hif_ext_group);
		hif_latency_profile_start(hif_ext_group);

		hif_record_event(hif_ext_group->hif, hif_ext_group->grp_id,
				 0, 0, 0, HIF_EVENT_IRQ_TRIGGER);

		hif_ext_group->irq_disable(hif_ext_group);

		if (pld_is_one_msi(scn->qdf_dev->dev))
			hif_ext_group->irq_disabled_start_time =
							qdf_time_sched_clock();
		/*
		 * if private ioctl has issued fake suspend command to put
		 * FW in D0-WOW state then here is our chance to bring FW out
		 * of WOW mode.
		 *
		 * The reason why you need to explicitly wake-up the FW is here:
		 * APSS should have been in fully awake through-out when
		 * fake APSS suspend command was issued (to put FW in WOW mode)
		 * hence organic way of waking-up the FW
		 * (as part-of APSS-host wake-up) won't happen because
		 * in reality APSS didn't really suspend.
		 */
		hif_check_and_trigger_ut_resume(scn);

		hif_check_and_trigger_sys_resume(scn, irq);

		qdf_atomic_inc(&scn->active_grp_tasklet_cnt);

		hif_ext_group->sched_ops->schedule(hif_ext_group);
		hif_update_irq_handle_time(hif_ext_group);
	}

	return IRQ_HANDLED;
}

/**
 * hif_exec_kill() - grp tasklet kill
 * @hif_ctx: hif_softc
 *
 * return: void
 */
void hif_exec_kill(struct hif_opaque_softc *hif_ctx)
{
	int i;
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(hif_ctx);

	for (i = 0; i < hif_state->hif_num_extgroup; i++)
		hif_state->hif_ext_group[i]->sched_ops->kill(
			hif_state->hif_ext_group[i]);

	qdf_atomic_set(&hif_state->ol_sc.active_grp_tasklet_cnt, 0);
}

#ifdef FEATURE_IRQ_AFFINITY
static inline void
hif_init_force_napi_complete(struct hif_exec_context *hif_ext_group)
{
	qdf_atomic_init(&hif_ext_group->force_napi_complete);
}
#else
static inline void
hif_init_force_napi_complete(struct hif_exec_context *hif_ext_group)
{
}
#endif

/**
 * hif_register_ext_group() - API to register external group
 * interrupt handler.
 * @hif_ctx : HIF Context
 * @numirq: number of irq's in the group
 * @irq: array of irq values
 * @handler: callback interrupt handler function
 * @cb_ctx: context to passed in callback
 * @context_name: context name
 * @type: napi vs tasklet
 * @scale:
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hif_register_ext_group(struct hif_opaque_softc *hif_ctx,
				  uint32_t numirq, uint32_t irq[],
				  ext_intr_handler handler,
				  void *cb_ctx, const char *context_name,
				  enum hif_exec_type type, uint32_t scale)
{
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ctx);
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(scn);
	struct hif_exec_context *hif_ext_group;

	if (scn->ext_grp_irq_configured) {
		hif_err("Called after ext grp irq configured");
		return QDF_STATUS_E_FAILURE;
	}

	if (hif_state->hif_num_extgroup >= HIF_MAX_GROUP) {
		hif_err("Max groups: %d reached", hif_state->hif_num_extgroup);
		return QDF_STATUS_E_FAILURE;
	}

	if (numirq >= HIF_MAX_GRP_IRQ) {
		hif_err("Invalid numirq: %d", numirq);
		return QDF_STATUS_E_FAILURE;
	}

	hif_ext_group = hif_exec_create(type, scale);
	if (!hif_ext_group)
		return QDF_STATUS_E_FAILURE;

	hif_state->hif_ext_group[hif_state->hif_num_extgroup] =
		hif_ext_group;

	hif_ext_group->numirq = numirq;
	qdf_mem_copy(&hif_ext_group->irq[0], irq, numirq * sizeof(irq[0]));
	hif_ext_group->context = cb_ctx;
	hif_ext_group->handler = handler;
	hif_ext_group->configured = true;
	hif_ext_group->grp_id = hif_state->hif_num_extgroup;
	hif_ext_group->hif = hif_ctx;
	hif_ext_group->context_name = context_name;
	hif_ext_group->type = type;
	hif_init_force_napi_complete(hif_ext_group);

	hif_state->hif_num_extgroup++;
	return QDF_STATUS_SUCCESS;
}
qdf_export_symbol(hif_register_ext_group);

/**
 * hif_exec_create() - create an execution context
 * @type: the type of execution context to create
 * @scale:
 */
struct hif_exec_context *hif_exec_create(enum hif_exec_type type,
						uint32_t scale)
{
	hif_debug("%s: create exec_type %d budget %d",
		  __func__, type, QCA_NAPI_BUDGET * scale);

	switch (type) {
	case HIF_EXEC_NAPI_TYPE:
		return hif_exec_napi_create(scale);

	case HIF_EXEC_TASKLET_TYPE:
		return hif_exec_tasklet_create();
	default:
		return NULL;
	}
}

/**
 * hif_exec_destroy() - free the hif_exec context
 * @ctx: context to free
 *
 * please kill the context before freeing it to avoid a use after free.
 */
void hif_exec_destroy(struct hif_exec_context *ctx)
{
	struct hif_softc *scn = HIF_GET_SOFTC(ctx->hif);

	if (scn->ext_grp_irq_configured)
		qdf_spinlock_destroy(&ctx->irq_lock);
	qdf_mem_free(ctx);
}

/**
 * hif_deregister_exec_group() - API to free the exec contexts
 * @hif_ctx: HIF context
 * @context_name: name of the module whose contexts need to be deregistered
 *
 * This function deregisters the contexts of the requestor identified
 * based on the context_name & frees the memory.
 *
 * Return: void
 */
void hif_deregister_exec_group(struct hif_opaque_softc *hif_ctx,
				const char *context_name)
{
	struct hif_softc *scn = HIF_GET_SOFTC(hif_ctx);
	struct HIF_CE_state *hif_state = HIF_GET_CE_STATE(scn);
	struct hif_exec_context *hif_ext_group;
	int i;

	for (i = 0; i < HIF_MAX_GROUP; i++) {
		hif_ext_group = hif_state->hif_ext_group[i];

		if (!hif_ext_group)
			continue;

		hif_debug("%s: Deregistering grp id %d name %s",
			  __func__,
			  hif_ext_group->grp_id,
			  hif_ext_group->context_name);

		if (strcmp(hif_ext_group->context_name, context_name) == 0) {
			hif_ext_group->sched_ops->kill(hif_ext_group);
			hif_state->hif_ext_group[i] = NULL;
			hif_exec_destroy(hif_ext_group);
			hif_state->hif_num_extgroup--;
		}

	}
}
qdf_export_symbol(hif_deregister_exec_group);

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * hif_umac_reset_handler_tasklet() - Tasklet for UMAC HW reset interrupt
 * @data: UMAC HW reset HIF context
 *
 * return: void
 */
static void hif_umac_reset_handler_tasklet(unsigned long data)
{
	struct hif_umac_reset_ctx *umac_reset_ctx =
		(struct hif_umac_reset_ctx *)data;

	/* call the callback handler */
	umac_reset_ctx->cb_handler(umac_reset_ctx->cb_ctx);
}

/**
 * hif_umac_reset_irq_handler() - Interrupt service routine of UMAC HW reset
 * @irq: irq coming from kernel
 * @ctx: UMAC HW reset HIF context
 *
 * return: IRQ_HANDLED if success, else IRQ_NONE
 */
static irqreturn_t hif_umac_reset_irq_handler(int irq, void *ctx)
{
	struct hif_umac_reset_ctx *umac_reset_ctx = ctx;

	/* Schedule the tasklet if it is umac reset interrupt and exit */
	if (umac_reset_ctx->irq_handler(umac_reset_ctx->cb_ctx))
		tasklet_hi_schedule(&umac_reset_ctx->intr_tq);

	return IRQ_HANDLED;
}

QDF_STATUS hif_get_umac_reset_irq(struct hif_opaque_softc *hif_scn,
				  int *umac_reset_irq)
{
	int ret;
	struct hif_softc *hif_sc = HIF_GET_SOFTC(hif_scn);
	struct hif_pci_softc *sc = HIF_GET_PCI_SOFTC(hif_sc);
	struct platform_device *pdev = (struct platform_device *)sc->pdev;

	ret = pfrm_get_irq(&pdev->dev, (struct qdf_pfm_hndl *)pdev,
			   "umac_reset", 0, umac_reset_irq);

	if (ret) {
		hif_err("umac reset get irq failed ret %d", ret);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(hif_get_umac_reset_irq);

QDF_STATUS hif_register_umac_reset_handler(struct hif_opaque_softc *hif_scn,
					   bool (*irq_handler)(void *cb_ctx),
					   int (*tl_handler)(void *cb_ctx),
					   void *cb_ctx, int irq)
{
	struct hif_softc *hif_sc = HIF_GET_SOFTC(hif_scn);
	struct hif_umac_reset_ctx *umac_reset_ctx;
	int ret;

	if (!hif_sc) {
		hif_err("scn is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	umac_reset_ctx = &hif_sc->umac_reset_ctx;

	umac_reset_ctx->irq_handler = irq_handler;
	umac_reset_ctx->cb_handler = tl_handler;
	umac_reset_ctx->cb_ctx = cb_ctx;
	umac_reset_ctx->os_irq = irq;

	/* Init the tasklet */
	tasklet_init(&umac_reset_ctx->intr_tq,
		     hif_umac_reset_handler_tasklet,
		     (unsigned long)umac_reset_ctx);

	/* Register the interrupt handler */
	ret  = pfrm_request_irq(hif_sc->qdf_dev->dev, irq,
				hif_umac_reset_irq_handler,
				IRQF_NO_SUSPEND,
				"umac_hw_reset_irq",
				umac_reset_ctx);
	if (ret) {
		hif_err("request_irq failed: %d", ret);
		return qdf_status_from_os_return(ret);
	}

	umac_reset_ctx->irq_configured = true;

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(hif_register_umac_reset_handler);

QDF_STATUS hif_unregister_umac_reset_handler(struct hif_opaque_softc *hif_scn)
{
	struct hif_softc *hif_sc = HIF_GET_SOFTC(hif_scn);
	struct hif_umac_reset_ctx *umac_reset_ctx;
	int ret;

	if (!hif_sc) {
		hif_err("scn is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	umac_reset_ctx = &hif_sc->umac_reset_ctx;
	if (!umac_reset_ctx->irq_configured) {
		hif_err("unregister called without a prior IRQ configuration");
		return QDF_STATUS_E_FAILURE;
	}

	ret  = pfrm_free_irq(hif_sc->qdf_dev->dev,
			     umac_reset_ctx->os_irq,
			     umac_reset_ctx);
	if (ret) {
		hif_err("free_irq failed: %d", ret);
		return qdf_status_from_os_return(ret);
	}
	umac_reset_ctx->irq_configured = false;

	tasklet_disable(&umac_reset_ctx->intr_tq);
	tasklet_kill(&umac_reset_ctx->intr_tq);

	umac_reset_ctx->cb_handler = NULL;
	umac_reset_ctx->cb_ctx = NULL;

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(hif_unregister_umac_reset_handler);
#endif
