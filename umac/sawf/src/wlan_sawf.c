/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: wlan_sawf.c
 * This file defines functions needed by the SAWF framework.
 */

#include "wlan_sawf.h"
#include <qdf_util.h>
#include <qdf_types.h>
#include <qdf_mem.h>
#include <qdf_trace.h>
#include <qdf_module.h>
#include <cdp_txrx_sawf.h>
#include <wlan_cfg80211.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_osif_priv.h>
#include <cfg80211_external.h>
#ifdef WLAN_FEATURE_11BE_MLO
#include <wlan_mlo_mgr_peer.h>
#endif
#include <wlan_telemetry_agent.h>

static struct sawf_ctx *g_wlan_sawf_ctx;

QDF_STATUS wlan_sawf_init(void)
{
	if (g_wlan_sawf_ctx) {
		sawf_err("SAWF global context is already allocated");
		return QDF_STATUS_E_FAILURE;
	}

	g_wlan_sawf_ctx = qdf_mem_malloc(sizeof(struct sawf_ctx));
	if (!g_wlan_sawf_ctx) {
		sawf_err("Mem alloc failed for SAWF context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_spinlock_create(&g_wlan_sawf_ctx->lock);

	sawf_info("SAWF: SAWF ctx is initialized");
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_sawf_deinit(void)
{
	if (!g_wlan_sawf_ctx) {
		sawf_err("SAWF gloabl context is already freed");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_spinlock_destroy(&g_wlan_sawf_ctx->lock);

	qdf_mem_free(g_wlan_sawf_ctx);

	return QDF_STATUS_SUCCESS;
}

struct sawf_ctx *wlan_get_sawf_ctx(void)
{
	return g_wlan_sawf_ctx;
}
qdf_export_symbol(wlan_get_sawf_ctx);

void wlan_print_service_class(struct wlan_sawf_svc_class_params *params)
{
	char buf[512];
	int nb;

	nb = snprintf(buf, sizeof(buf), "\n%s\nService ID: %d\nApp Name: %s\n"
		      "Min throughput: %d\nMax throughput: %d\n"
		      "Burst Size: %d\nService Interval: %d\n"
		      "Delay Bound: %d\nMSDU TTL: %d\nPriority: %d\n"
		      "TID: %d\nMSDU Loss Rate: %d\n"
		      "UL Burst Size: %d\nUL Service Interval: %d\n"
		      "UL Min throughput: %d\nUL Max Latency: %d\n"
		      "Service class type: %d\nRef Count: %d\nPeer Count: %d\n"
		      "Disabled_Modes: %u\nEnabled Params Mask: 0x%04X\n"
		      "[def %u|scs %u|epcs %u]",
		      SAWF_LINE_FORMAT,
		      params->svc_id, params->app_name,
		      params->min_thruput_rate, params->max_thruput_rate,
		      params->burst_size, params->service_interval,
		      params->delay_bound, params->msdu_ttl, params->priority,
		      params->tid, params->msdu_rate_loss,
		      params->ul_burst_size, params->ul_service_interval,
		      params->ul_min_tput, params->ul_max_latency,
		      params->type, params->ref_count, params->peer_count,
		      params->disabled_modes, params->enabled_param_mask,
		      params->type_ref_count[WLAN_SAWF_SVC_TYPE_DEF],
		      params->type_ref_count[WLAN_SAWF_SVC_TYPE_SCS],
		      params->type_ref_count[WLAN_SAWF_SVC_TYPE_EPCS]);

	if (nb > 0 && nb >= sizeof(buf))
		sawf_err("Small buffer (buffer size %zu required size %d)",
			 sizeof(buf), nb);

	qdf_nofl_info(buf);
}

qdf_export_symbol(wlan_print_service_class);

bool wlan_service_id_valid(uint8_t svc_id)
{
	if (svc_id <  SAWF_SVC_CLASS_MIN || svc_id > SAWF_SVC_CLASS_MAX)
		return false;
	else
		return true;
}

qdf_export_symbol(wlan_service_id_valid);

bool wlan_service_id_configured_nolock(uint8_t svc_id)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return false;
	}

	if (!(sawf->svc_classes[svc_id - 1].configured))
		return false;

	return true;
}

qdf_export_symbol(wlan_service_id_configured_nolock);

uint8_t wlan_service_id_tid_nolock(uint8_t svc_id)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		goto err;
	}

	if (wlan_service_id_configured_nolock(svc_id))
		return sawf->svc_classes[svc_id - 1].tid;
err:
	return SAWF_INVALID_TID;
}

qdf_export_symbol(wlan_service_id_tid_nolock);

bool wlan_delay_bound_configured_nolock(uint8_t svc_id)
{
	struct sawf_ctx *sawf;
	struct wlan_sawf_svc_class_params *svclass_param;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return false;
	}
	if (svc_id <  SAWF_SVC_CLASS_MIN ||
	    svc_id > SAWF_SVC_CLASS_MAX) {
		sawf_err("Invalid svc-class id");
		return false;
	}

	svclass_param = &sawf->svc_classes[svc_id - 1];
	if (svclass_param->delay_bound &&
			svclass_param->delay_bound != SAWF_DEF_PARAM_VAL)
		return true;

	return false;
}

qdf_export_symbol(wlan_delay_bound_configured_nolock);

struct wlan_sawf_svc_class_params *
wlan_get_svc_class_params(uint8_t svc_id)
{
	struct sawf_ctx *sawf;
	struct wlan_sawf_svc_class_params *svclass_param;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return NULL;
	}
	if (svc_id <  SAWF_SVC_CLASS_MIN ||
	    svc_id > SAWF_SVC_CLASS_MAX) {
		sawf_err("Invalid svc-class id");
		return NULL;
	}

	qdf_spin_lock_bh(&sawf->lock);
	svclass_param = &sawf->svc_classes[svc_id - 1];
	qdf_spin_unlock_bh(&sawf->lock);
	return svclass_param;
}

qdf_export_symbol(wlan_get_svc_class_params);

void wlan_update_sawf_params_nolock(struct wlan_sawf_svc_class_params *params)
{
	struct sawf_ctx *sawf;
	struct wlan_sawf_svc_class_params *new_param;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return;
	}

	new_param = &sawf->svc_classes[params->svc_id - 1];
	new_param->svc_id = params->svc_id;
	new_param->min_thruput_rate = params->min_thruput_rate;
	new_param->max_thruput_rate = params->max_thruput_rate;
	new_param->burst_size = params->burst_size;
	new_param->service_interval = params->service_interval;
	new_param->delay_bound = params->delay_bound;
	new_param->msdu_ttl = params->msdu_ttl;
	new_param->priority = params->priority;
	new_param->tid = params->tid;
	new_param->msdu_rate_loss = params->msdu_rate_loss;
	new_param->ul_burst_size = params->ul_burst_size;
	new_param->ul_service_interval = params->ul_service_interval;
	new_param->ul_min_tput = params->ul_min_tput;
	new_param->ul_max_latency = params->ul_max_latency;
	new_param->enabled_param_mask = params->enabled_param_mask;
}

qdf_export_symbol(wlan_update_sawf_params_nolock);

QDF_STATUS wlan_validate_sawf_params(struct wlan_sawf_svc_class_params *params)
{
	uint32_t value;

	value = params->min_thruput_rate;
	if (value != SAWF_DEF_PARAM_VAL && (value < SAWF_MIN_MIN_THROUGHPUT ||
	    value > SAWF_MAX_MIN_THROUGHPUT)) {
		sawf_err("Invalid Min throughput: %d", value);
		return QDF_STATUS_E_FAILURE;
	}

	value = params->max_thruput_rate;
	if (value != SAWF_DEF_PARAM_VAL && (value < SAWF_MIN_MAX_THROUGHPUT ||
	    value > SAWF_MAX_MAX_THROUGHPUT)) {
		sawf_err("Invalid Max througput: %d", value);
		return QDF_STATUS_E_FAILURE;
	}

	value = params->burst_size;
	if (value != SAWF_DEF_PARAM_VAL && (value < SAWF_MIN_BURST_SIZE ||
	    value > SAWF_MAX_BURST_SIZE)) {
		sawf_err("Invalid Burst Size: %d", value);
		return QDF_STATUS_E_FAILURE;
	}

	value = params->delay_bound;
	if (value != SAWF_DEF_PARAM_VAL && (value < SAWF_MIN_DELAY_BOUND
	    || value > SAWF_MAX_DELAY_BOUND)) {
		sawf_err("Invalid Delay Bound: %d", value);
		return QDF_STATUS_E_FAILURE;
	}

	value = params->service_interval;
	if (value != SAWF_DEF_PARAM_VAL && (value < SAWF_MIN_SVC_INTERVAL ||
	    value > SAWF_MAX_SVC_INTERVAL)) {
		sawf_err("Invalid Service Interval: %d", value);
		return QDF_STATUS_E_FAILURE;
	}

	value = params->msdu_ttl;
	if (value != SAWF_DEF_PARAM_VAL && (value < SAWF_MIN_MSDU_TTL ||
	    value > SAWF_MAX_MSDU_TTL)) {
		sawf_err("Invalid MSDU TTL: %d", value);
		return QDF_STATUS_E_FAILURE;
	}

	value = params->priority;
	if (value != SAWF_DEF_PARAM_VAL && (value < SAWF_MIN_PRIORITY ||
	    value > SAWF_MAX_PRIORITY)) {
		sawf_err("Invalid Priority: %d", value);
		return QDF_STATUS_E_FAILURE;
	}

	value = params->tid;
	if (value != SAWF_DEF_PARAM_VAL && (value < SAWF_MIN_TID ||
	    value > SAWF_MAX_TID)) {
		sawf_err("Invalid TID %d", value);
		return QDF_STATUS_E_FAILURE;
	}

	value = params->msdu_rate_loss;
	if (value != SAWF_DEF_PARAM_VAL && (value < SAWF_MIN_MSDU_LOSS_RATE ||
	    value > SAWF_MAX_MSDU_LOSS_RATE)) {
		sawf_err("Invalid MSDU Loss rate: %d", value);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(wlan_validate_sawf_params);

QDF_STATUS
wlan_sawf_get_uplink_params(uint8_t svc_id, uint8_t *tid,
			    uint32_t *service_interval, uint32_t *burst_size,
			    uint32_t *min_tput, uint32_t *max_latency)
{
	struct sawf_ctx *sawf;
	struct wlan_sawf_svc_class_params *svc_param;

	if (!wlan_service_id_valid(svc_id) ||
	    !wlan_service_id_configured(svc_id))
		return QDF_STATUS_E_INVAL;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock_bh(&sawf->lock);
	svc_param = &sawf->svc_classes[svc_id - 1];

	if (tid)
		*tid = svc_param->tid;

	if (service_interval)
		*service_interval = svc_param->ul_service_interval;

	if (burst_size)
		*burst_size = svc_param->ul_burst_size;

	if (min_tput)
		*min_tput = svc_param->ul_min_tput;

	if (max_latency)
		*max_latency = svc_param->ul_max_latency;

	qdf_spin_unlock_bh(&sawf->lock);
	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(wlan_sawf_get_uplink_params);

int wlan_sawf_get_tput_stats(void *soc, void *arg, uint64_t *in_bytes,
			     uint64_t *in_cnt, uint64_t *tx_bytes,
			     uint64_t *tx_cnt, uint8_t tid, uint8_t msduq)
{
	return cdp_get_throughput_stats(soc, arg, in_bytes, in_cnt,
					tx_bytes, tx_cnt,
					tid, msduq);
}
qdf_export_symbol(wlan_sawf_get_tput_stats);

int wlan_sawf_get_mpdu_stats(void *soc, void *arg, uint64_t *svc_int_pass,
			     uint64_t *svc_int_fail,
			     uint64_t *burst_pass, uint64_t *burst_fail,
			     uint8_t tid, uint8_t msduq)
{
	return cdp_get_mpdu_stats(soc, arg, svc_int_pass, svc_int_fail,
				  burst_pass, burst_fail,
				  tid, msduq);
}
qdf_export_symbol(wlan_sawf_get_mpdu_stats);

int wlan_sawf_get_drop_stats(void *soc, void *arg, uint64_t *pass,
			     uint64_t *drop, uint64_t *drop_ttl,
			     uint8_t tid, uint8_t msduq)
{
	return cdp_get_drop_stats(soc, arg, pass, drop, drop_ttl, tid, msduq);
}
qdf_export_symbol(wlan_sawf_get_drop_stats);

#ifdef WLAN_FEATURE_11BE_MLO
static inline QDF_STATUS wlan_sawf_fill_mld_mac(struct wlan_objmgr_peer *peer,
						struct sk_buff *vendor_event)
{
	if (wlan_peer_is_mlo(peer)) {
		if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_SLA_PEER_MLD_MAC,
			    QDF_MAC_ADDR_SIZE,
			    (void *)(wlan_peer_mlme_get_mldaddr(peer)))) {
			sawf_err("nla put fail");
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS wlan_sawf_fill_mld_mac(struct wlan_objmgr_peer *peer,
						struct sk_buff *vendor_event)
{
	return QDF_STATUS_SUCCESS;
}
#endif

int
wlan_sawf_sla_process_sla_event(uint8_t svc_id, uint8_t *peer_mac,
				uint8_t *peer_mld_mac, uint8_t flag)
{
	return telemetry_sawf_reset_peer_stats(peer_mac);
}

static void wlan_sawf_send_breach_nl(struct wlan_objmgr_peer *peer,
				     struct psoc_peer_iter *itr)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	struct vdev_osif_priv *osif_vdev;
	struct sk_buff *vendor_event;
	uint8_t ac = 0;

	vdev = wlan_peer_get_vdev(peer);
	if (!vdev) {
		sawf_info("Unable to find vdev");
		return;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		sawf_info("Unable to find pdev");
		return;
	}

	osif_vdev  = wlan_vdev_get_ospriv(vdev);
	if (!osif_vdev) {
		sawf_info("Unable to find osif_vdev");
		return;
	}

	vendor_event =
		wlan_cfg80211_vendor_event_alloc(
				osif_vdev->wdev->wiphy, osif_vdev->wdev,
				4000,
				QCA_NL80211_VENDOR_SUBCMD_SAWF_SLA_BREACH_INDEX,
				GFP_ATOMIC);

	if (!vendor_event) {
		sawf_info("Failed to allocate vendor event");
		return;
	}

	if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_SLA_PEER_MAC,
		    QDF_MAC_ADDR_SIZE,
		    (void *)(wlan_peer_get_macaddr(peer)))) {
		sawf_err("nla put fail");
		goto error_cleanup;
	}

	if (wlan_sawf_fill_mld_mac(peer, vendor_event)) {
			sawf_err("nla put fail");
		goto error_cleanup;
	}

	if (nla_put_u8(vendor_event, QCA_WLAN_VENDOR_ATTR_SLA_SVC_ID,
		       itr->svc_id)) {
		sawf_err("nla put fail");
		goto error_cleanup;
	}

	if (nla_put_u8(vendor_event, QCA_WLAN_VENDOR_ATTR_SLA_PARAM,
		       itr->param)) {
		sawf_err("nla put fail");
		goto error_cleanup;
	}

	if (nla_put_u8(vendor_event, QCA_WLAN_VENDOR_ATTR_SLA_SET_CLEAR,
		       itr->set_clear)) {
		sawf_err("nla put fail");
		goto error_cleanup;
	}

	ac = TID_TO_WME_AC(itr->tid);
	if (nla_put_u8(vendor_event, QCA_WLAN_VENDOR_ATTR_SLA_AC,
		       ac)) {
		sawf_err("nla put fail");
		goto error_cleanup;
	}

	wlan_cfg80211_vendor_event(vendor_event, GFP_ATOMIC);

	return;

error_cleanup:
	wlan_cfg80211_vendor_free_skb(vendor_event);
}

static void wlan_sawf_get_psoc_peer(struct wlan_objmgr_psoc *psoc,
				    void *arg,
				    uint8_t index)
{
	struct wlan_objmgr_peer *peer;
	struct psoc_peer_iter *itr;

	itr = (struct psoc_peer_iter *)arg;

	peer = wlan_objmgr_get_peer_by_mac(
			psoc, itr->mac_addr, WLAN_SAWF_ID);

	if (peer) {
		wlan_sawf_send_breach_nl(peer, itr);
		wlan_objmgr_peer_release_ref(peer, WLAN_SAWF_ID);
	}
}

void wlan_sawf_notify_breach(uint8_t *mac_addr,
			     uint8_t svc_id,
			     uint8_t param,
			     bool set_clear,
			     uint8_t tid)
{
	struct psoc_peer_iter itr = {0};

	itr.mac_addr = mac_addr;
	itr.set_clear = set_clear;
	itr.svc_id = svc_id;
	itr.param = param;
	itr.tid = tid;

	wlan_objmgr_iterate_psoc_list(wlan_sawf_get_psoc_peer,
				      &itr, WLAN_SAWF_ID);
}

qdf_export_symbol(wlan_sawf_notify_breach);

bool wlan_service_id_configured(uint8_t svc_id)
{
	struct sawf_ctx *sawf;
	bool ret;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return false;
	}

	qdf_spin_lock_bh(&sawf->lock);
	ret = wlan_service_id_configured_nolock(svc_id);
	qdf_spin_unlock_bh(&sawf->lock);
	return ret;
}

qdf_export_symbol(wlan_service_id_configured);

uint8_t wlan_service_id_tid(uint8_t svc_id)
{
	struct sawf_ctx *sawf;
	uint8_t tid;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		goto err;
	}

	qdf_spin_lock_bh(&sawf->lock);
	tid = wlan_service_id_tid_nolock(svc_id);
	qdf_spin_unlock_bh(&sawf->lock);
	return tid;
err:
	return SAWF_INVALID_TID;
}

qdf_export_symbol(wlan_service_id_tid);

bool wlan_delay_bound_configured(uint8_t svc_id)
{
	struct sawf_ctx *sawf;
	bool ret;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return false;
	}
	if (svc_id <  SAWF_SVC_CLASS_MIN ||
	    svc_id > SAWF_SVC_CLASS_MAX) {
		sawf_err("Invalid svc-class id");
		return false;
	}

	qdf_spin_lock_bh(&sawf->lock);
	ret = wlan_delay_bound_configured_nolock(svc_id);
	qdf_spin_unlock_bh(&sawf->lock);
	return ret;
}

qdf_export_symbol(wlan_delay_bound_configured);

void wlan_update_sawf_params(struct wlan_sawf_svc_class_params *params)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return;
	}

	qdf_spin_lock_bh(&sawf->lock);
	wlan_update_sawf_params_nolock(params);
	qdf_spin_unlock_bh(&sawf->lock);
}

qdf_export_symbol(wlan_update_sawf_params);

uint8_t wlan_service_id_get_type_nolock(uint8_t svc_id)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		goto err;
	}

	if (wlan_service_id_configured_nolock(svc_id))
		return sawf->svc_classes[svc_id - 1].type;
err:
	return SAWF_INVALID_TYPE;
}

qdf_export_symbol(wlan_service_id_get_type_nolock);

uint8_t wlan_service_id_get_type(uint8_t svc_id)
{
	struct sawf_ctx *sawf;
	uint8_t type;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		goto err;
	}

	qdf_spin_lock_bh(&sawf->lock);
	type = wlan_service_id_get_type_nolock(svc_id);
	qdf_spin_unlock_bh(&sawf->lock);
	return type;
err:
	return SAWF_INVALID_TYPE;
}

qdf_export_symbol(wlan_service_id_get_type);

void wlan_service_id_set_type_nolock(uint8_t svc_id, uint8_t type)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return;
	}

	sawf->svc_classes[svc_id - 1].type = type;
}

qdf_export_symbol(wlan_service_id_set_type_nolock);

void wlan_service_id_set_type(uint8_t svc_id, uint8_t type)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return;
	}

	qdf_spin_lock_bh(&sawf->lock);
	wlan_service_id_set_type_nolock(svc_id, type);
	qdf_spin_unlock_bh(&sawf->lock);
}

qdf_export_symbol(wlan_service_id_set_type);

void wlan_service_id_inc_ref_count_nolock(uint8_t svc_id)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return;
	}

	if (wlan_service_id_configured_nolock(svc_id))
		sawf->svc_classes[svc_id - 1].ref_count++;
}

qdf_export_symbol(wlan_service_id_inc_ref_count_nolock);

void wlan_service_id_dec_ref_count_nolock(uint8_t svc_id)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return;
	}

	if (wlan_service_id_configured_nolock(svc_id) &&
	    sawf->svc_classes[svc_id - 1].ref_count > 0)
		sawf->svc_classes[svc_id - 1].ref_count--;
}

qdf_export_symbol(wlan_service_id_dec_ref_count_nolock);

void wlan_service_id_inc_peer_count_nolock(uint8_t svc_id)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return;
	}

	if (wlan_service_id_configured_nolock(svc_id))
		sawf->svc_classes[svc_id - 1].peer_count++;
}

qdf_export_symbol(wlan_service_id_inc_peer_count_nolock);

void wlan_service_id_dec_peer_count_nolock(uint8_t svc_id)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return;
	}

	if (wlan_service_id_configured_nolock(svc_id) &&
	    sawf->svc_classes[svc_id - 1].peer_count > 0)
		sawf->svc_classes[svc_id - 1].peer_count--;
}

qdf_export_symbol(wlan_service_id_dec_peer_count_nolock);

void wlan_service_id_inc_ref_count(uint8_t svc_id)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return;
	}

	qdf_spin_lock_bh(&sawf->lock);
	wlan_service_id_inc_ref_count_nolock(svc_id);
	qdf_spin_unlock_bh(&sawf->lock);
}

qdf_export_symbol(wlan_service_id_inc_ref_count);

void wlan_service_id_dec_ref_count(uint8_t svc_id)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return;
	}

	qdf_spin_lock_bh(&sawf->lock);
	wlan_service_id_dec_ref_count_nolock(svc_id);
	qdf_spin_unlock_bh(&sawf->lock);
}

qdf_export_symbol(wlan_service_id_dec_ref_count);

void wlan_service_id_inc_peer_count(uint8_t svc_id)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return;
	}

	qdf_spin_lock_bh(&sawf->lock);
	wlan_service_id_inc_peer_count_nolock(svc_id);
	qdf_spin_unlock_bh(&sawf->lock);
}

qdf_export_symbol(wlan_service_id_inc_peer_count);

void wlan_service_id_dec_peer_count(uint8_t svc_id)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return;
	}

	qdf_spin_lock_bh(&sawf->lock);
	wlan_service_id_dec_peer_count_nolock(svc_id);
	qdf_spin_unlock_bh(&sawf->lock);
}

qdf_export_symbol(wlan_service_id_dec_peer_count);

uint32_t wlan_service_id_get_ref_count_nolock(uint8_t svc_id)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		goto err;
	}

	if (wlan_service_id_configured_nolock(svc_id))
		return sawf->svc_classes[svc_id - 1].ref_count;
err:
	return 0;
}

qdf_export_symbol(wlan_service_id_get_ref_count_nolock);

uint32_t wlan_service_id_get_ref_count(uint8_t svc_id)
{
	struct sawf_ctx *sawf;
	uint32_t count;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		goto err;
	}

	qdf_spin_lock_bh(&sawf->lock);
	count = wlan_service_id_get_ref_count_nolock(svc_id);
	qdf_spin_unlock_bh(&sawf->lock);
	return count;
err:
	return 0;
}

qdf_export_symbol(wlan_service_id_get_ref_count);

uint32_t wlan_service_id_get_peer_count_nolock(uint8_t svc_id)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		goto err;
	}

	if (wlan_service_id_configured_nolock(svc_id))
		return sawf->svc_classes[svc_id - 1].peer_count;
err:
	return 0;
}

qdf_export_symbol(wlan_service_id_get_peer_count_nolock);

uint32_t wlan_service_id_get_peer_count(uint8_t svc_id)
{
	struct sawf_ctx *sawf;
	uint32_t count;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		goto err;
	}

	qdf_spin_lock_bh(&sawf->lock);
	count = wlan_service_id_get_peer_count_nolock(svc_id);
	qdf_spin_unlock_bh(&sawf->lock);
	return count;
err:
	return 0;
}

qdf_export_symbol(wlan_service_id_get_peer_count);

void wlan_disable_service_class(uint8_t svc_id)
{
	struct sawf_ctx *sawf;
	struct wlan_sawf_svc_class_params *svclass_param;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		sawf_err("SAWF ctx is invalid");
		return;
	}

	qdf_spin_lock_bh(&sawf->lock);
	svclass_param = &sawf->svc_classes[svc_id - 1];
	qdf_mem_zero(svclass_param,
		     sizeof(struct wlan_sawf_svc_class_params));
	qdf_spin_unlock_bh(&sawf->lock);
}

qdf_export_symbol(wlan_disable_service_class);

#ifdef WLAN_SUPPORT_SCS
bool wlan_service_id_scs_valid(uint8_t sawf_rule_type, uint8_t service_id)
{
	if ((sawf_rule_type == SAWF_RULE_TYPE_SCS) &&
	    (service_id >= SAWF_SCS_SVC_CLASS_MIN) &&
	    (service_id <= SAWF_SCS_SVC_CLASS_MAX))
		return true;
	else
		return false;
}
#else
bool wlan_service_id_scs_valid(uint8_t sawf_rule_type, uint8_t service_id)
{
	return false;
}
#endif

qdf_export_symbol(wlan_service_id_scs_valid);

uint16_t wlan_service_id_get_enabled_param_mask(uint8_t svc_id)
{
	struct sawf_ctx *sawf;
	uint16_t enabled_param_mask;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		qdf_err("SAWF ctx is invalid");
		return 0;
	}

	qdf_spin_lock_bh(&sawf->lock);
	enabled_param_mask = sawf->svc_classes[svc_id - 1].enabled_param_mask;
	qdf_spin_unlock_bh(&sawf->lock);

	return enabled_param_mask;
}

qdf_export_symbol(wlan_service_id_get_enabled_param_mask);

void wlan_service_id_inc_type_ref_count_nolock(uint8_t svc_id, uint8_t type)
{
	struct sawf_ctx *sawf;
	struct wlan_sawf_svc_class_params *svc;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		qdf_err("SAWF ctx is invalid");
		return;
	}

	svc = &sawf->svc_classes[svc_id - 1];

	svc->type_ref_count[type]++;
}

void wlan_service_id_inc_type_ref_count(uint8_t svc_id, uint8_t type)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		qdf_err("SAWF ctx is invalid");
		return;
	}

	qdf_spin_lock_bh(&sawf->lock);
	wlan_service_id_inc_type_ref_count_nolock(svc_id, type);
	qdf_spin_unlock_bh(&sawf->lock);
}

void wlan_service_id_dec_type_ref_count_nolock(uint8_t svc_id, uint8_t type)
{
	struct sawf_ctx *sawf;
	struct wlan_sawf_svc_class_params *svc;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		qdf_err("SAWF ctx is invalid");
		return;
	}

	svc = &sawf->svc_classes[svc_id - 1];

	if (svc->type_ref_count[type])
		svc->type_ref_count[type]--;
}

void wlan_service_id_dec_type_ref_count(uint8_t svc_id, uint8_t type)
{
	struct sawf_ctx *sawf;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		qdf_err("SAWF ctx is invalid");
		return;
	}

	qdf_spin_lock_bh(&sawf->lock);
	wlan_service_id_dec_type_ref_count_nolock(svc_id, type);
	qdf_spin_unlock_bh(&sawf->lock);
}

uint32_t wlan_service_id_get_type_ref_count_nolock(uint8_t svc_id, uint8_t type)
{
	struct sawf_ctx *sawf;
	struct wlan_sawf_svc_class_params *svc;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		qdf_err("SAWF ctx is invalid");
		return 0;
	}

	svc = &sawf->svc_classes[svc_id - 1];

	return svc->type_ref_count[type];
}

uint32_t wlan_service_id_get_type_ref_count(uint8_t svc_id, uint8_t type)
{
	struct sawf_ctx *sawf;
	uint32_t ref_count;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		qdf_err("SAWF ctx is invalid");
		return 0;
	}

	qdf_spin_lock_bh(&sawf->lock);
	ref_count = wlan_service_id_get_type_ref_count_nolock(svc_id, type);
	qdf_spin_unlock_bh(&sawf->lock);

	return ref_count;
}

uint32_t wlan_service_id_get_total_type_ref_count_nolock(uint8_t svc_id)
{
	struct sawf_ctx *sawf;
	uint32_t ref_count = 0;
	uint8_t type;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		qdf_err("SAWF ctx is invalid");
		return 0;
	}

	for (type = 0; type < WLAN_SAWF_SVC_TYPE_MAX; type++)
		ref_count += sawf->svc_classes[svc_id - 1].type_ref_count[type];

	return ref_count;
}

qdf_export_symbol(wlan_service_id_get_total_type_ref_count_nolock);

uint32_t wlan_service_id_get_total_type_ref_count(uint8_t svc_id)
{
	struct sawf_ctx *sawf;
	uint32_t ref_count = 0;

	sawf = wlan_get_sawf_ctx();
	if (!sawf) {
		qdf_err("SAWF ctx is invalid");
		return 0;
	}

	qdf_spin_lock_bh(&sawf->lock);
	ref_count = wlan_service_id_get_total_type_ref_count_nolock(svc_id);
	qdf_spin_unlock_bh(&sawf->lock);

	return ref_count;
}

qdf_export_symbol(wlan_service_id_get_total_type_ref_count);
