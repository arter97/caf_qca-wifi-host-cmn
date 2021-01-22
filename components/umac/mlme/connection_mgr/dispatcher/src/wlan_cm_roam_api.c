/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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
 * DOC: wlan_cm_roam_api.c
 *
 * Implementation for the Common Roaming interfaces.
 */

#include "wlan_cm_roam_api.h"
#include "wlan_vdev_mlme_api.h"
#include "wlan_mlme_main.h"
#include "wlan_policy_mgr_api.h"
#include <wmi_unified_priv.h>
#include <../../core/src/wlan_cm_vdev_api.h>

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
QDF_STATUS
wlan_cm_enable_roaming_on_connected_sta(struct wlan_objmgr_pdev *pdev,
					uint8_t vdev_id)
{
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t sta_vdev_id = WLAN_INVALID_VDEV_ID;
	uint32_t count;
	uint32_t idx;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	sta_vdev_id = policy_mgr_get_roam_enabled_sta_session_id(psoc, vdev_id);
	if (sta_vdev_id != WLAN_UMAC_VDEV_ID_MAX)
		return QDF_STATUS_E_FAILURE;

	count = policy_mgr_get_mode_specific_conn_info(psoc,
						       op_ch_freq_list,
						       vdev_id_list,
						       PM_STA_MODE);

	if (!count)
		return QDF_STATUS_E_FAILURE;

	/*
	 * Loop through all connected STA vdevs and roaming will be enabled
	 * on the STA that has a different vdev id from the one passed as
	 * input and has non zero roam trigger value.
	 */
	for (idx = 0; idx < count; idx++) {
		if (vdev_id_list[idx] != vdev_id &&
		    mlme_get_roam_trigger_bitmap(psoc, vdev_id_list[idx])) {
			sta_vdev_id = vdev_id_list[idx];
			break;
		}
	}

	if (sta_vdev_id == WLAN_INVALID_VDEV_ID)
		return QDF_STATUS_E_FAILURE;

	mlme_debug("ROAM: Enabling roaming on vdev[%d]", sta_vdev_id);

	return cm_roam_state_change(pdev,
				    sta_vdev_id,
				    WLAN_ROAM_RSO_ENABLED,
				    REASON_CTX_INIT);
}
#endif

char *cm_roam_get_requestor_string(enum wlan_cm_rso_control_requestor requestor)
{
	switch (requestor) {
	case RSO_INVALID_REQUESTOR:
	default:
		return "No requestor";
	case RSO_START_BSS:
		return "SAP start";
	case RSO_CHANNEL_SWITCH:
		return "CSA";
	case RSO_CONNECT_START:
		return "STA connection";
	case RSO_SAP_CHANNEL_CHANGE:
		return "SAP Ch switch";
	case RSO_NDP_CON_ON_NDI:
		return "NDP connection";
	case RSO_SET_PCL:
		return "Set PCL";
	}
}

QDF_STATUS
wlan_cm_rso_set_roam_trigger(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			     struct wlan_roam_triggers *trigger)
{
	QDF_STATUS status;

	status = cm_roam_acquire_lock();
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_FAILURE;

	status = cm_rso_set_roam_trigger(pdev, vdev_id, trigger);

	cm_roam_release_lock();

	return status;
}

QDF_STATUS wlan_cm_disable_rso(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			       enum wlan_cm_rso_control_requestor requestor,
			       uint8_t reason)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	QDF_STATUS status;

	status = cm_roam_acquire_lock();
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_FAILURE;

	if (reason == REASON_DRIVER_DISABLED && requestor)
		mlme_set_operations_bitmap(psoc, vdev_id, requestor, false);

	mlme_debug("ROAM_CONFIG: vdev[%d] Disable roaming - requestor:%s",
		   vdev_id, cm_roam_get_requestor_string(requestor));

	status = cm_roam_state_change(pdev, vdev_id, WLAN_ROAM_RSO_STOPPED,
				      REASON_DRIVER_DISABLED);
	cm_roam_release_lock();

	return status;
}

QDF_STATUS wlan_cm_enable_rso(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			      enum wlan_cm_rso_control_requestor requestor,
			      uint8_t reason)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	QDF_STATUS status;

	if (reason == REASON_DRIVER_ENABLED && requestor)
		mlme_set_operations_bitmap(psoc, vdev_id, requestor, true);

	status = cm_roam_acquire_lock();
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_FAILURE;

	mlme_debug("ROAM_CONFIG: vdev[%d] Enable roaming - requestor:%s",
		   vdev_id, cm_roam_get_requestor_string(requestor));

	status = cm_roam_state_change(pdev, vdev_id, WLAN_ROAM_RSO_ENABLED,
				      REASON_DRIVER_ENABLED);
	cm_roam_release_lock();

	return status;
}

QDF_STATUS wlan_cm_abort_rso(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	QDF_STATUS status;

	status = cm_roam_acquire_lock();
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_FAILURE;

	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(psoc, vdev_id) ||
	    wlan_cm_neighbor_roam_in_progress(psoc, vdev_id)) {
		cm_roam_release_lock();
		return QDF_STATUS_E_BUSY;
	}

	/* RSO stop cmd will be issued with lock held to avoid
	 * any racing conditions with wma/csr layer
	 */
	wlan_cm_disable_rso(pdev, vdev_id, REASON_DRIVER_DISABLED,
			    RSO_INVALID_REQUESTOR);

	cm_roam_release_lock();
	return QDF_STATUS_SUCCESS;
}

bool wlan_cm_roaming_in_progress(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	QDF_STATUS status;

	status = cm_roam_acquire_lock();
	if (QDF_IS_STATUS_ERROR(status))
		return false;

	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(psoc, vdev_id) ||
	    MLME_IS_ROAMING_IN_PROG(psoc, vdev_id) ||
	    mlme_is_roam_invoke_in_progress(psoc, vdev_id) ||
	    wlan_cm_neighbor_roam_in_progress(psoc, vdev_id)) {
		cm_roam_release_lock();
		return true;
	}

	cm_roam_release_lock();

	return false;
}

QDF_STATUS wlan_cm_roam_state_change(struct wlan_objmgr_pdev *pdev,
				     uint8_t vdev_id,
				     enum roam_offload_state requested_state,
				     uint8_t reason)
{
	return cm_roam_state_change(pdev, vdev_id, requested_state, reason);
}

QDF_STATUS wlan_cm_roam_send_rso_cmd(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id, uint8_t rso_command,
				     uint8_t reason)
{
	return cm_roam_send_rso_cmd(psoc, vdev_id, rso_command, reason);
}

QDF_STATUS wlan_cm_roam_stop_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				 uint8_t reason)
{
	return cm_roam_stop_req(psoc, vdev_id, reason);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS
wlan_cm_roam_extract_btm_response(wmi_unified_t wmi, void *evt_buf,
				  struct roam_btm_response_data *dst,
				  uint8_t idx)
{
	if (wmi->ops->extract_roam_btm_response_stats)
		return wmi->ops->extract_roam_btm_response_stats(wmi, evt_buf,
								 dst, idx);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wlan_cm_roam_extract_roam_initial_info(wmi_unified_t wmi, void *evt_buf,
				       struct roam_initial_data *dst,
				       uint8_t idx)
{
	if (wmi->ops->extract_roam_initial_info)
		return wmi->ops->extract_roam_initial_info(wmi, evt_buf,
							   dst, idx);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wlan_cm_roam_extract_roam_msg_info(wmi_unified_t wmi, void *evt_buf,
				   struct roam_msg_info *dst, uint8_t idx)
{
	if (wmi->ops->extract_roam_msg_info)
		return wmi->ops->extract_roam_msg_info(wmi, evt_buf, dst, idx);

	return QDF_STATUS_E_FAILURE;
}

void wlan_cm_roam_activate_pcl_per_vdev(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id, bool pcl_per_vdev)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	/* value - true (vdev pcl) false - pdev pcl */
	mlme_priv->cm_roam.pcl_vdev_cmd_active = pcl_per_vdev;
	mlme_debug("CM_ROAM: vdev[%d] SET PCL cmd level - [%s]", vdev_id,
		   pcl_per_vdev ? "VDEV" : "PDEV");
}

bool wlan_cm_roam_is_pcl_per_vdev_active(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return false;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return false;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return mlme_priv->cm_roam.pcl_vdev_cmd_active;
}

bool
wlan_cm_dual_sta_is_freq_allowed(struct wlan_objmgr_psoc *psoc,
				 uint32_t freq,
				 enum QDF_OPMODE opmode)
{
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	enum reg_wifi_band band;
	uint32_t count, connected_sta_freq;

	/*
	 * Check if already there is 1 STA connected. If this API is
	 * called for 2nd STA and if dual sta roaming is enabled, then
	 * don't allow the intra band frequencies of the 1st sta for
	 * connection on 2nd STA.
	 */
	count = policy_mgr_get_mode_specific_conn_info(psoc, op_ch_freq_list,
						       vdev_id_list,
						       PM_STA_MODE);
	if (!count || !wlan_mlme_get_dual_sta_roaming_enabled(psoc) ||
	    opmode != QDF_STA_MODE)
		return true;

	connected_sta_freq = op_ch_freq_list[0];
	band = wlan_reg_freq_to_band(connected_sta_freq);
	if ((band == REG_BAND_2G && WLAN_REG_IS_24GHZ_CH_FREQ(freq)) ||
	    (band == REG_BAND_5G && !WLAN_REG_IS_24GHZ_CH_FREQ(freq)))
		return false;

	return true;
}

void
wlan_cm_dual_sta_roam_update_connect_channels(struct wlan_objmgr_psoc *psoc,
					      struct scan_filter *filter)
{
	uint32_t i, num_channels = 0;
	uint32_t *channel_list;
	bool is_ch_allowed;
	QDF_STATUS status;

	if (!wlan_mlme_get_dual_sta_roaming_enabled(psoc))
		return;

	channel_list = qdf_mem_malloc(NUM_CHANNELS * sizeof(uint32_t));
	if (!channel_list)
		return;

	/*
	 * Get Reg domain valid channels and update to the scan filter
	 * if already 1st sta is in connected state. Don't allow channels
	 * on which the 1st STA is connected.
	 */
	status = policy_mgr_get_valid_chans(psoc, channel_list,
					    &num_channels);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Error in getting valid channels");
		qdf_mem_free(channel_list);
		return;
	}

	filter->num_of_channels = 0;
	for (i = 0; i < num_channels; i++) {
		is_ch_allowed =
			wlan_cm_dual_sta_is_freq_allowed(psoc, channel_list[i],
							 QDF_STA_MODE);
		if (!is_ch_allowed)
			continue;

		filter->chan_freq_list[filter->num_of_channels] =
					channel_list[i];
		filter->num_of_channels++;
	}
	qdf_mem_free(channel_list);
}

void
wlan_cm_roam_disable_vendor_btm(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return;
	}

	/* Set default value of reason code */
	mlme_priv->cm_roam.vendor_btm_param.user_roam_reason =
						DISABLE_VENDOR_BTM_CONFIG;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
}

void
wlan_cm_roam_set_vendor_btm_params(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id,
				   struct wlan_cm_roam_vendor_btm_params
									*param)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return;
	}

	qdf_mem_copy(&mlme_priv->cm_roam.vendor_btm_param, param,
		     sizeof(struct wlan_cm_roam_vendor_btm_params));
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
}

void
wlan_cm_roam_get_vendor_btm_params(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id,
				   struct wlan_cm_roam_vendor_btm_params *param)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return;
	}

	qdf_mem_copy(param, &mlme_priv->cm_roam.vendor_btm_param,
		     sizeof(struct wlan_cm_roam_vendor_btm_params));

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
}
#endif

QDF_STATUS wlan_cm_roam_cfg_get_value(struct wlan_objmgr_psoc *psoc,
				      uint8_t vdev_id,
				      enum roam_cfg_param roam_cfg_type,
				      struct cm_roam_values_copy *dst_config)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct rso_config *rso_cfg;
	struct rso_cfg_params *src_cfg;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}
	src_cfg = &rso_cfg->cfg_param;
	switch (roam_cfg_type) {
	case RSSI_CHANGE_THRESHOLD:
		dst_config->int_value = rso_cfg->rescan_rssi_delta;
		break;
	case BEACON_RSSI_WEIGHT:
		dst_config->uint_value = rso_cfg->beacon_rssi_weight;
		break;
	case HI_RSSI_DELAY_BTW_SCANS:
		dst_config->uint_value = rso_cfg->hi_rssi_scan_delay;
		break;
	case EMPTY_SCAN_REFRESH_PERIOD:
		dst_config->uint_value = src_cfg->empty_scan_refresh_period;
		break;
	case SCAN_MIN_CHAN_TIME:
		dst_config->uint_value = src_cfg->min_chan_scan_time;
		break;
	case SCAN_MAX_CHAN_TIME:
		dst_config->uint_value = src_cfg->max_chan_scan_time;
		break;
	case NEIGHBOR_SCAN_PERIOD:
		dst_config->uint_value = src_cfg->neighbor_scan_period;
		break;
	case FULL_ROAM_SCAN_PERIOD:
		dst_config->uint_value = src_cfg->full_roam_scan_period;
		break;
	case ROAM_RSSI_DIFF:
		dst_config->uint_value = src_cfg->roam_rssi_diff;
		break;
	case NEIGHBOUR_LOOKUP_THRESHOLD:
		dst_config->uint_value = src_cfg->neighbor_lookup_threshold;
		break;
	case SCAN_N_PROBE:
		dst_config->uint_value = src_cfg->roam_scan_n_probes;
		break;
	case SCAN_HOME_AWAY:
		dst_config->uint_value = src_cfg->roam_scan_home_away_time;
		break;
	case NEIGHBOUR_SCAN_REFRESH_PERIOD:
		dst_config->uint_value =
				src_cfg->neighbor_results_refresh_period;
		break;
	case ROAM_CONTROL_ENABLE:
		dst_config->bool_value = rso_cfg->roam_control_enable;
		break;
	default:
		mlme_err("Invalid roam config requested:%d", roam_cfg_type);
		status = QDF_STATUS_E_FAILURE;
		break;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return status;
}

void wlan_cm_set_disable_hi_rssi(struct wlan_objmgr_pdev *pdev,
				 uint8_t vdev_id, bool value)
{
	static struct rso_config *rso_cfg;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}
	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
		return;
	}

	rso_cfg->disable_hi_rssi = value;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
}

static QDF_STATUS
cm_roam_update_cfg(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		   uint8_t reason)
{
	QDF_STATUS status;

	status = cm_roam_acquire_lock();
	if (QDF_IS_STATUS_ERROR(status))
		return status;
	if (!MLME_IS_ROAM_STATE_RSO_ENABLED(psoc, vdev_id)) {
		mlme_debug("Update cfg received while ROAM RSO not started");
		cm_roam_release_lock();
		return QDF_STATUS_E_INVAL;
	}

	status = cm_roam_send_rso_cmd(psoc, vdev_id,
				      ROAM_SCAN_OFFLOAD_UPDATE_CFG, reason);
	cm_roam_release_lock();

	return status;
}

static void cm_dump_freq_list(struct rso_chan_info *chan_info)
{
	uint8_t *channel_list;
	uint8_t i = 0, j = 0;
	uint32_t buflen = CFG_VALID_CHANNEL_LIST_LEN * 4;

	channel_list = qdf_mem_malloc(buflen);
	if (!channel_list)
		return;

	if (chan_info->freq_list) {
		for (i = 0; i < chan_info->num_chan; i++) {
			if (j < buflen)
				j += snprintf(channel_list + j, buflen - j,
					      "%d ", chan_info->freq_list[i]);
			else
				break;
		}
	}

	mlme_debug("frequency list [%u]: %s", i, channel_list);
	qdf_mem_free(channel_list);
}

static uint8_t
cm_append_pref_chan_list(struct rso_chan_info *chan_info, qdf_freq_t *freq_list,
			 uint8_t num_chan)
{
	uint8_t i = 0, j = 0;

	for (i = 0; i < chan_info->num_chan; i++) {
		for (j = 0; j < num_chan; j++)
			if (chan_info->freq_list[i] == freq_list[j])
				break;

		if (j < num_chan)
			continue;
		if (num_chan == ROAM_MAX_CHANNELS)
			break;
		freq_list[num_chan++] = chan_info->freq_list[i];
	}

	return num_chan;
}

static QDF_STATUS cm_create_bg_scan_roam_channel_list(struct rso_chan_info *chan_info,
						const qdf_freq_t *chan_freq_lst,
						const uint8_t num_chan)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t i;

	chan_info->freq_list = qdf_mem_malloc(sizeof(qdf_freq_t) * num_chan);
	if (!chan_info->freq_list)
		return QDF_STATUS_E_NOMEM;

	chan_info->num_chan = num_chan;
	for (i = 0; i < num_chan; i++)
		chan_info->freq_list[i] = chan_freq_lst[i];

	return status;
}

static void cm_flush_roam_channel_list(struct rso_chan_info *channel_info)
{
	/* Free up the memory first (if required) */
	if (channel_info->freq_list) {
		qdf_mem_free(channel_info->freq_list);
		channel_info->freq_list = NULL;
		channel_info->num_chan = 0;
	}
}

static QDF_STATUS
cm_update_roam_scan_channel_list(uint8_t vdev_id,
				 struct rso_chan_info *chan_info,
				 qdf_freq_t *freq_list, uint8_t num_chan,
				 bool update_preferred_chan)
{
	uint16_t pref_chan_cnt = 0;

	if (chan_info->num_chan) {
		mlme_debug("Current channels:");
		cm_dump_freq_list(chan_info);
	}

	if (update_preferred_chan) {
		pref_chan_cnt = cm_append_pref_chan_list(chan_info, freq_list,
							 num_chan);
		num_chan = pref_chan_cnt;
	}
	cm_flush_roam_channel_list(chan_info);
	cm_create_bg_scan_roam_channel_list(chan_info, freq_list, num_chan);

	mlme_debug("New channels:");
	cm_dump_freq_list(chan_info);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_cm_roam_cfg_set_value(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			   enum roam_cfg_param roam_cfg_type,
			   struct cm_roam_values_copy *src_config)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct rso_config *rso_cfg;
	struct rso_cfg_params *dst_cfg;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}
	dst_cfg = &rso_cfg->cfg_param;
	mlme_debug("roam_cfg_type %d, uint val %d int val %d bool val %d num chan %d",
		   roam_cfg_type, src_config->uint_value, src_config->int_value,
		   src_config->bool_value, src_config->chan_info.num_chan);
	switch (roam_cfg_type) {
	case RSSI_CHANGE_THRESHOLD:
		rso_cfg->rescan_rssi_delta  = src_config->uint_value;
		break;
	case BEACON_RSSI_WEIGHT:
		rso_cfg->beacon_rssi_weight = src_config->uint_value;
		break;
	case HI_RSSI_DELAY_BTW_SCANS:
		rso_cfg->hi_rssi_scan_delay = src_config->uint_value;
		break;
	case EMPTY_SCAN_REFRESH_PERIOD:
		dst_cfg->empty_scan_refresh_period = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					  REASON_EMPTY_SCAN_REF_PERIOD_CHANGED);
		break;
	case FULL_ROAM_SCAN_PERIOD:
		dst_cfg->full_roam_scan_period = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					  REASON_ROAM_FULL_SCAN_PERIOD_CHANGED);
		break;
	case ENABLE_SCORING_FOR_ROAM:
		dst_cfg->enable_scoring_for_roam = src_config->bool_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_SCORING_CRITERIA_CHANGED);
		break;
	case SCAN_MIN_CHAN_TIME:
		mlme_obj->cfg.lfr.neighbor_scan_min_chan_time =
							src_config->uint_value;
		dst_cfg->min_chan_scan_time = src_config->uint_value;
		break;
	case SCAN_MAX_CHAN_TIME:
		dst_cfg->max_chan_scan_time = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_SCAN_CH_TIME_CHANGED);
		break;
	case NEIGHBOR_SCAN_PERIOD:
		dst_cfg->neighbor_scan_period = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_SCAN_HOME_TIME_CHANGED);
		break;
	case ROAM_CONFIG_ENABLE:
		rso_cfg->roam_control_enable = src_config->bool_value;
		if (!rso_cfg->roam_control_enable)
			break;
		dst_cfg->roam_scan_period_after_inactivity = 0;
		dst_cfg->roam_inactive_data_packet_count = 0;
		dst_cfg->roam_scan_inactivity_time = 0;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_ROAM_CONTROL_CONFIG_ENABLED);
		break;
	case ROAM_PREFERRED_CHAN:
		if (dst_cfg->specific_chan_info.num_chan) {
			mlme_err("Specific channel list is already configured");
			break;
		}
		status = cm_update_roam_scan_channel_list(vdev_id,
					&dst_cfg->pref_chan_info,
					src_config->chan_info.freq_list,
					src_config->chan_info.num_chan,
					true);
		if (QDF_IS_STATUS_ERROR(status))
			break;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_CHANNEL_LIST_CHANGED);
		break;
	case ROAM_SPECIFIC_CHAN:
		cm_update_roam_scan_channel_list(vdev_id,
					&dst_cfg->specific_chan_info,
					src_config->chan_info.freq_list,
					src_config->chan_info.num_chan,
					false);
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_CHANNEL_LIST_CHANGED);
		break;
	case ROAM_RSSI_DIFF:
		dst_cfg->roam_rssi_diff = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_RSSI_DIFF_CHANGED);
		break;
	case NEIGHBOUR_LOOKUP_THRESHOLD:
		dst_cfg->neighbor_lookup_threshold = src_config->uint_value;
		break;
	case SCAN_N_PROBE:
		dst_cfg->roam_scan_n_probes = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_NPROBES_CHANGED);
		break;
	case SCAN_HOME_AWAY:
		dst_cfg->roam_scan_home_away_time = src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled &&
		    src_config->bool_value)
			cm_roam_update_cfg(psoc, vdev_id,
					   REASON_HOME_AWAY_TIME_CHANGED);
		break;
	case NEIGHBOUR_SCAN_REFRESH_PERIOD:
		dst_cfg->neighbor_results_refresh_period =
						src_config->uint_value;
		mlme_obj->cfg.lfr.neighbor_scan_results_refresh_period =
				src_config->uint_value;
		if (mlme_obj->cfg.lfr.roam_scan_offload_enabled)
			cm_roam_update_cfg(psoc, vdev_id,
				REASON_NEIGHBOR_SCAN_REFRESH_PERIOD_CHANGED);
		break;
	default:
		mlme_err("Invalid roam config requested:%d", roam_cfg_type);
		status = QDF_STATUS_E_FAILURE;
		break;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return status;
}

static void cm_rso_chan_to_freq_list(struct wlan_objmgr_pdev *pdev,
				     qdf_freq_t *freq_list,
				     const uint8_t *chan_list,
				     uint32_t chan_list_len)
{
	uint32_t count;

	for (count = 0; count < chan_list_len; count++)
		freq_list[count] =
			wlan_reg_chan_to_freq(pdev, chan_list[count]);
}

QDF_STATUS wlan_cm_rso_config_init(struct wlan_objmgr_vdev *vdev)
{
	struct rso_chan_info *chan_info;
	struct rso_config *rso_cfg;
	struct rso_cfg_params *cfg_params;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_INVAL;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_INVAL;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg)
		return QDF_STATUS_E_INVAL;
	cfg_params = &rso_cfg->cfg_param;
	cfg_params->max_chan_scan_time =
		mlme_obj->cfg.lfr.neighbor_scan_max_chan_time;
	cfg_params->min_chan_scan_time =
		mlme_obj->cfg.lfr.neighbor_scan_min_chan_time;
	cfg_params->neighbor_lookup_threshold =
		mlme_obj->cfg.lfr.neighbor_lookup_rssi_threshold;
	cfg_params->rssi_thresh_offset_5g =
		mlme_obj->cfg.lfr.rssi_threshold_offset_5g;
	cfg_params->opportunistic_threshold_diff =
		mlme_obj->cfg.lfr.opportunistic_scan_threshold_diff;
	cfg_params->roam_rescan_rssi_diff =
		mlme_obj->cfg.lfr.roam_rescan_rssi_diff;

	cfg_params->roam_bmiss_first_bcn_cnt =
		mlme_obj->cfg.lfr.roam_bmiss_first_bcnt;
	cfg_params->roam_bmiss_final_cnt =
		mlme_obj->cfg.lfr.roam_bmiss_final_bcnt;

	cfg_params->neighbor_scan_period =
		mlme_obj->cfg.lfr.neighbor_scan_timer_period;
	cfg_params->neighbor_scan_min_period =
		mlme_obj->cfg.lfr.neighbor_scan_min_timer_period;
	cfg_params->neighbor_results_refresh_period =
		mlme_obj->cfg.lfr.neighbor_scan_results_refresh_period;
	cfg_params->empty_scan_refresh_period =
		mlme_obj->cfg.lfr.empty_scan_refresh_period;
	cfg_params->full_roam_scan_period =
		mlme_obj->cfg.lfr.roam_full_scan_period;
	cfg_params->enable_scoring_for_roam =
		mlme_obj->cfg.roam_scoring.enable_scoring_for_roam;
	cfg_params->roam_scan_n_probes =
		mlme_obj->cfg.lfr.roam_scan_n_probes;
	cfg_params->roam_scan_home_away_time =
		mlme_obj->cfg.lfr.roam_scan_home_away_time;
	cfg_params->roam_scan_inactivity_time =
		mlme_obj->cfg.lfr.roam_scan_inactivity_time;
	cfg_params->roam_inactive_data_packet_count =
		mlme_obj->cfg.lfr.roam_inactive_data_packet_count;
	cfg_params->roam_scan_period_after_inactivity =
		mlme_obj->cfg.lfr.roam_scan_period_after_inactivity;

	chan_info = &cfg_params->specific_chan_info;
	chan_info->num_chan =
		mlme_obj->cfg.lfr.neighbor_scan_channel_list_num;
	mlme_debug("number of channels: %u", chan_info->num_chan);
	if (chan_info->num_chan) {
		chan_info->freq_list =
			qdf_mem_malloc(sizeof(qdf_freq_t) *
				       chan_info->num_chan);
		if (!chan_info->freq_list) {
			chan_info->num_chan = 0;
			return QDF_STATUS_E_NOMEM;
		}
		/* Update the roam global structure from CFG */
		cm_rso_chan_to_freq_list(pdev, chan_info->freq_list,
			mlme_obj->cfg.lfr.neighbor_scan_channel_list,
			mlme_obj->cfg.lfr.neighbor_scan_channel_list_num);
	} else {
		chan_info->freq_list = NULL;
	}

	cfg_params->hi_rssi_scan_max_count =
		mlme_obj->cfg.lfr.roam_scan_hi_rssi_maxcount;
	cfg_params->hi_rssi_scan_rssi_delta =
		mlme_obj->cfg.lfr.roam_scan_hi_rssi_delta;

	cfg_params->hi_rssi_scan_delay =
		mlme_obj->cfg.lfr.roam_scan_hi_rssi_delay;

	cfg_params->hi_rssi_scan_rssi_ub =
		mlme_obj->cfg.lfr.roam_scan_hi_rssi_ub;
	cfg_params->roam_rssi_diff =
		mlme_obj->cfg.lfr.roam_rssi_diff;
	cfg_params->bg_rssi_threshold =
		mlme_obj->cfg.lfr.bg_rssi_threshold;

	return QDF_STATUS_SUCCESS;
}

void wlan_cm_rso_config_deinit(struct wlan_objmgr_vdev *vdev)
{
	struct rso_config *rso_cfg;
	struct rso_cfg_params *cfg_params;

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg)
		return;

	cfg_params = &rso_cfg->cfg_param;

	cm_flush_roam_channel_list(&cfg_params->specific_chan_info);
	cm_flush_roam_channel_list(&cfg_params->pref_chan_info);
}

#ifdef FEATURE_CM_ENABLE
struct rso_config *wlan_cm_get_rso_config(struct wlan_objmgr_vdev *vdev)
{
	struct cm_ext_obj *cm_ext_obj;

	cm_ext_obj = cm_get_ext_hdl(vdev);
	if (!cm_ext_obj)
		return NULL;

	return &cm_ext_obj->rso_cfg;
}
#else
struct rso_config *wlan_cm_get_rso_config(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		return NULL;
	}

	return &mlme_priv->rso_cfg;
}
#endif

#ifdef WLAN_FEATURE_FILS_SK
QDF_STATUS wlan_cm_update_mlme_fils_connection_info(
		struct wlan_objmgr_psoc *psoc,
		struct wlan_fils_connection_info *src_fils_info,
		uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	if (!src_fils_info) {
		mlme_debug("FILS: vdev:%d Clear fils info", vdev_id);
		qdf_mem_free(mlme_priv->fils_con_info);
		mlme_priv->fils_con_info = NULL;
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_SUCCESS;
	}

	if (mlme_priv->fils_con_info)
		qdf_mem_free(mlme_priv->fils_con_info);

	mlme_priv->fils_con_info =
		qdf_mem_malloc(sizeof(struct wlan_fils_connection_info));
	if (!mlme_priv->fils_con_info) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_NOMEM;
	}

	mlme_debug("FILS: vdev:%d update fils info", vdev_id);
	qdf_mem_copy(mlme_priv->fils_con_info, src_fils_info,
		     sizeof(struct wlan_fils_connection_info));

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}

struct wlan_fils_connection_info *wlan_cm_get_fils_connection_info(
				struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_fils_connection_info *fils_info;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return NULL;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return NULL;
	}

	fils_info = mlme_priv->fils_con_info;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return fils_info;
}

QDF_STATUS wlan_cm_update_fils_ft(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id, uint8_t *fils_ft,
				  uint8_t fils_ft_len)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	if (!mlme_priv->fils_con_info || !fils_ft || !fils_ft_len ||
	    !mlme_priv->fils_con_info->is_fils_connection) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->fils_con_info->fils_ft_len = fils_ft_len;
	qdf_mem_copy(mlme_priv->fils_con_info->fils_ft, fils_ft, fils_ft_len);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS
wlan_cm_update_roam_scan_scheme_bitmap(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id,
				       uint32_t roam_scan_scheme_bitmap)
{
	struct wlan_objmgr_vdev *vdev;
	struct rso_config *rso_cfg;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);

	if (!vdev) {
		mlme_err("vdev%d: vdev object is NULL", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}
	rso_cfg->roam_scan_scheme_bitmap = roam_scan_scheme_bitmap;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}

uint32_t wlan_cm_get_roam_scan_scheme_bitmap(struct wlan_objmgr_psoc *psoc,
					     uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	uint32_t roam_scan_scheme_bitmap;
	struct rso_config *rso_cfg;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);

	if (!vdev) {
		mlme_err("vdev%d: vdev object is NULL", vdev_id);
		return 0;
	}

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return 0;
	}

	roam_scan_scheme_bitmap = rso_cfg->roam_scan_scheme_bitmap;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return roam_scan_scheme_bitmap;
}
#endif
