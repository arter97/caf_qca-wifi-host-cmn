/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <target_if_wifi_radar.h>
#include <wlan_lmac_if_def.h>
#include <target_type.h>
#include <hif_hw_version.h>
#include <target_if.h>
#include <wlan_osif_priv.h>
#include <init_deinit_lmac.h>
#include <wlan_wifi_radar_utils_api.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wmi_unified_wifi_radar_api.h>

static QDF_STATUS
target_if_unregister_wifi_radar_cal_status_event_handler
		(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_hdl;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	wmi_hdl = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_hdl) {
		wifi_radar_err("Unable to get wmi handle");
		return QDF_STATUS_E_NULL_VALUE;
	}
	status = wmi_unified_unregister_event
		(wmi_hdl, wmi_pdev_wifi_radar_cal_completion_status_event_id);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_radar_err("Failed to unregister wifi radar cal status event handler");

	return status;
}

static int
target_if_pdev_wifi_radar_cal_status_event_handler(ol_scn_t sc,
						   uint8_t *data,
						   uint32_t datalen)
{
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	struct pdev_wifi_radar *pwr;
	QDF_STATUS retval = 0;
	struct wmi_wifi_radar_cal_status_param param = {0};
	int tx_idx, rx_idx;

	if (!sc || !data) {
		wifi_radar_err("sc or data is null");
		return -EINVAL;
	}

	psoc = target_if_get_psoc_from_scn_hdl(sc);
	if (!psoc) {
		wifi_radar_err("psoc is null");
		return -EINVAL;
	}

	retval = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_WIFI_RADAR_ID);
	if (QDF_IS_STATUS_ERROR(retval)) {
		wifi_radar_err("unable to get psoc reference");
		return -EINVAL;
	}

	wmi_handle = GET_WMI_HDL_FROM_PSOC(psoc);
	if (!wmi_handle) {
		wifi_radar_err("wmi_handle is null");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_RADAR_ID);
		return -EINVAL;
	}

	retval = wmi_extract_wifi_radar_cal_status_event_param
			(wmi_handle, data, &param);
	if (QDF_IS_STATUS_ERROR(retval)) {
		wifi_radar_err("Failed to extract wifi radar cal status tlv");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_RADAR_ID);
		return -EINVAL;
	}

	pdev = wlan_objmgr_get_pdev_by_id
			(psoc, param.pdev_id, WLAN_WIFI_RADAR_ID);
	if (!pdev) {
		wifi_radar_err("pdev is null");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_RADAR_ID);
		return -EINVAL;
	}

	pwr = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						    WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pwr) {
		wifi_radar_err("pdev object for Wifi radar is NULL");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_RADAR_ID);
		wlan_objmgr_pdev_release_ref(pdev, WLAN_WIFI_RADAR_ID);
		return -EINVAL;
	}

	qdf_spin_lock_bh(&pwr->cal_status_lock);
	pwr->host_timestamp_of_cal_status_evt =
		qdf_ktime_to_ns(qdf_ktime_real_get());
	pwr->wifi_radar_pkt_bw = param.wifi_radar_pkt_bw;
	pwr->channel_bw = param.channel_bw;
	pwr->band_center_freq = param.band_center_freq;
	pwr->cal_num_ltf_tx = param.num_ltf_tx;
	pwr->cal_num_skip_ltf_rx = param.num_skip_ltf_rx;
	pwr->cal_num_ltf_accumulation = param.num_ltf_accumulation;
	for (tx_idx = 0; tx_idx < HOST_MAX_CHAINS; tx_idx++) {
		for (rx_idx = 0; rx_idx < HOST_MAX_CHAINS; rx_idx++) {
			pwr->per_chain_comb_cal_status[tx_idx][rx_idx] =
				(param.per_chain_cal_status[tx_idx] &
				 (1 << rx_idx));
		}
	}
	qdf_spin_unlock_bh(&pwr->cal_status_lock);

	wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_RADAR_ID);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_WIFI_RADAR_ID);

	return 0;
}

static QDF_STATUS
target_if_register_wifi_radar_cal_status_event_handler(struct wlan_objmgr_psoc
						       *psoc)
{
	wmi_unified_t wmi_hdl;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	wmi_hdl = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_hdl) {
		wifi_radar_err("Unable to get wmi handle");
		return QDF_STATUS_E_NULL_VALUE;
	}

	ret = wmi_unified_register_event_handler
		(wmi_hdl, wmi_pdev_wifi_radar_cal_completion_status_event_id,
		 target_if_pdev_wifi_radar_cal_status_event_handler,
		 WMI_RX_UMAC_CTX);

	/*
	 * Event registration is called per pdev
	 * Ignore error if event is already registered.
	 */
	if (QDF_IS_STATUS_ERROR(ret)) {
		wifi_radar_err("Failed to register wifi radar cal status event handler");
		ret = QDF_STATUS_SUCCESS;
	}

	return ret;
}


QDF_STATUS wifi_radar_deinit_pdev(struct wlan_objmgr_psoc *psoc,
				  struct wlan_objmgr_pdev *pdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct pdev_wifi_radar *pwr;

	pwr = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						    WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pwr)
		return QDF_STATUS_E_NULL_VALUE;

	/* Add wifi radar specific unregister part here */
	status = target_if_unregister_wifi_radar_cal_status_event_handler(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_radar_err("Failed to unregister cal status handler");

	if (pwr->cal_status_lock_initialized) {
		qdf_spinlock_destroy(&pwr->cal_status_lock);
		pwr->cal_status_lock_initialized = false;
	}

	return status;
}

QDF_STATUS wifi_radar_init_pdev(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_pdev *pdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct pdev_wifi_radar *pwr;
	struct psoc_wifi_radar *wr_sc;

	if (!pdev) {
		wifi_radar_err("PDEV is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!psoc) {
		wifi_radar_err("PSOC is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pwr = wlan_objmgr_pdev_get_comp_private_obj
				(pdev, WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pwr) {
		wifi_radar_err("pwr is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wr_sc = wlan_objmgr_psoc_get_comp_private_obj
				(psoc, WLAN_UMAC_COMP_WIFI_RADAR);

	if (!wr_sc) {
		wifi_radar_err("wr_sc is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = target_if_register_wifi_radar_cal_status_event_handler(psoc);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_radar_err("Failed to register with wifi radar cal status handler");
		return status;
	}

	pwr->is_wifi_radar_capable = 1;
	pwr->subbuf_size = STREAMFS_WIFI_RADAR_MAX_SUBBUF;
	pwr->num_subbufs = STREAMFS_WIFI_RADAR_NUM_SUBBUF;
	qdf_spinlock_create(&pwr->cal_status_lock);
	pwr->cal_status_lock_initialized = true;

	return status;
}

QDF_STATUS
target_if_wifi_radar_init_pdev(struct wlan_objmgr_psoc *psoc,
			       struct wlan_objmgr_pdev *pdev)
{
	struct pdev_wifi_radar *pa;
	struct psoc_wifi_radar *wifi_radar_sc;

	if (wlan_wifi_radar_is_feature_disabled(pdev)) {
		wifi_radar_err("wifi radar is disabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	pa = wlan_objmgr_pdev_get_comp_private_obj
			(pdev, WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pa)
		return QDF_STATUS_E_FAILURE;

	wifi_radar_sc = wlan_objmgr_psoc_get_comp_private_obj
			(psoc, WLAN_UMAC_COMP_WIFI_RADAR);
	if (!wifi_radar_sc)
		return QDF_STATUS_E_FAILURE;

	if (wifi_radar_sc->is_wifi_radar_capable) {
		pa->is_wifi_radar_capable = 1;
		return wifi_radar_init_pdev(psoc, pdev);
	} else {
		return QDF_STATUS_E_NOSUPPORT;
	}
}

QDF_STATUS
target_if_wifi_radar_deinit_pdev(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_pdev *pdev)
{
	struct psoc_wifi_radar *wifi_radar_sc;

	if (wlan_wifi_radar_is_feature_disabled(pdev)) {
		wifi_radar_err("wifi radar is disabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	wifi_radar_sc = wlan_objmgr_psoc_get_comp_private_obj
			(psoc, WLAN_UMAC_COMP_WIFI_RADAR);
	if (!wifi_radar_sc)
		return QDF_STATUS_E_FAILURE;

	if (wifi_radar_sc->is_wifi_radar_capable)
		return wifi_radar_deinit_pdev(psoc, pdev);
	else
		return QDF_STATUS_E_NOSUPPORT;
}

void target_if_wifi_radar_tx_ops_register(struct wlan_lmac_if_tx_ops *tx_ops)
{
	tx_ops->wifi_radar_tx_ops.wifi_radar_init_pdev =
		target_if_wifi_radar_init_pdev;
	tx_ops->wifi_radar_tx_ops.wifi_radar_deinit_pdev =
		target_if_wifi_radar_deinit_pdev;
}

void target_if_wifi_radar_set_support(struct wlan_objmgr_psoc *psoc,
				      uint8_t value)
{
	struct wlan_lmac_if_rx_ops *rx_ops;

	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (!rx_ops) {
		wifi_radar_err("rx_ops is NULL");
		return;
	}
	if (rx_ops->wifi_radar_rx_ops.wifi_radar_support_set)
		rx_ops->wifi_radar_rx_ops.wifi_radar_support_set(psoc, value);
}
