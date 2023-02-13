/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
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

#include "osdep.h"
#include "wmi.h"
#include "wmi_unified_priv.h"
#include "wmi_unified_param.h"
#include "target_if_cp_stats.h"
#include <wlan_cp_stats_public_structs.h>

#ifdef WLAN_SUPPORT_INFRA_CTRL_PATH_STATS
#ifdef WLAN_SUPPORT_TWT
static uint32_t
get_stats_req_twt_dialog_id(struct infra_cp_stats_cmd_info *req)
{
	return req->dialog_id;
}

static inline
void wmi_extract_ctrl_path_twt_stats_tlv(void *tag_buf,
					 struct twt_infra_cp_stats_event *param)
{
	wmi_ctrl_path_twt_stats_struct *wmi_stats_buf =
			(wmi_ctrl_path_twt_stats_struct *)tag_buf;

	param->dialog_id = wmi_stats_buf->dialog_id;
	param->status = wmi_stats_buf->status;
	param->num_sp_cycles = wmi_stats_buf->num_sp_cycles;
	param->avg_sp_dur_us = wmi_stats_buf->avg_sp_dur_us;
	param->min_sp_dur_us = wmi_stats_buf->min_sp_dur_us;
	param->max_sp_dur_us = wmi_stats_buf->max_sp_dur_us;
	param->tx_mpdu_per_sp = wmi_stats_buf->tx_mpdu_per_sp;
	param->rx_mpdu_per_sp = wmi_stats_buf->rx_mpdu_per_sp;
	param->tx_bytes_per_sp = wmi_stats_buf->tx_bytes_per_sp;
	param->rx_bytes_per_sp = wmi_stats_buf->rx_bytes_per_sp;

	wmi_debug("dialog_id = %u status = %u", wmi_stats_buf->dialog_id,
		  wmi_stats_buf->status);
	wmi_debug("num_sp_cycles = %u avg_sp_dur_us = 0x%x, \
		  min_sp_dur_us = 0x%x, max_sp_dur_us = 0x%x",
		  wmi_stats_buf->num_sp_cycles, wmi_stats_buf->avg_sp_dur_us,
		  wmi_stats_buf->min_sp_dur_us, wmi_stats_buf->max_sp_dur_us);
	wmi_debug("tx_mpdu_per_sp 0x%x, rx_mpdu_per_sp = 0x%x, \
		  tx_bytes_per_sp = 0x%x, rx_bytes_per_sp = 0x%x",
		  wmi_stats_buf->tx_mpdu_per_sp, wmi_stats_buf->rx_mpdu_per_sp,
		  wmi_stats_buf->tx_bytes_per_sp,
		  wmi_stats_buf->rx_bytes_per_sp);
}

static void wmi_twt_extract_stats_struct(void *tag_buf,
					 struct infra_cp_stats_event *params)
{
	struct twt_infra_cp_stats_event *twt_params;

	twt_params = params->twt_infra_cp_stats +
		     params->num_twt_infra_cp_stats;

	wmi_debug("TWT stats struct found - num_twt_cp_stats %d",
		  params->num_twt_infra_cp_stats);

	params->num_twt_infra_cp_stats++;
	wmi_extract_ctrl_path_twt_stats_tlv(tag_buf, twt_params);
}
#else
static inline
uint32_t get_stats_req_twt_dialog_id(struct infra_cp_stats_cmd_info *req)
{
	return 0;
}

static void wmi_twt_extract_stats_struct(void *tag_buf,
					 struct infra_cp_stats_event *params)
{
}
#endif /* WLAN_SUPPORT_TWT */

/*
 * wmi_stats_extract_tag_struct: function to extract tag structs
 * @tag_type: tag type that is to be printed
 * @tag_buf: pointer to the tag structure
 * @params: buffer to hold parameters extracted from response event
 *
 * Return: None
 */
static void wmi_stats_extract_tag_struct(uint32_t tag_type, void *tag_buf,
					 struct infra_cp_stats_event *params)
{
	wmi_debug("tag_type %d", tag_type);

	switch (tag_type) {
	case WMITLV_TAG_STRUC_wmi_ctrl_path_pdev_stats_struct:
		break;

	case WMITLV_TAG_STRUC_wmi_ctrl_path_mem_stats_struct:
		break;

	case WMITLV_TAG_STRUC_wmi_ctrl_path_twt_stats_struct:
		wmi_twt_extract_stats_struct(tag_buf, params);
		break;

	default:
		break;
	}
}

/*
 * wmi_stats_handler: parse the wmi event and fill the stats values
 * @buff: Buffer containing wmi event
 * @len: length of event buffer
 * @params: buffer to hold parameters extracted from response event
 *
 * Return: QDF_STATUS_SUCCESS on success, else other qdf error values
 */
QDF_STATUS wmi_stats_handler(void *buff, int32_t len,
			     struct infra_cp_stats_event *params)
{
	WMI_CTRL_PATH_STATS_EVENTID_param_tlvs *param_buf;
	wmi_ctrl_path_stats_event_fixed_param *ev;
	uint8_t *buf_ptr = (uint8_t *)buff;
	uint32_t curr_tlv_tag;
	uint32_t curr_tlv_len;
	uint8_t *tag_start_ptr;

	param_buf = (WMI_CTRL_PATH_STATS_EVENTID_param_tlvs *)buff;
	if (!param_buf)
		return QDF_STATUS_E_FAILURE;
	ev = (wmi_ctrl_path_stats_event_fixed_param *)param_buf->fixed_param;

	curr_tlv_tag = WMITLV_GET_TLVTAG(ev->tlv_header);
	curr_tlv_len = WMITLV_GET_TLVLEN(ev->tlv_header);
	buf_ptr = (uint8_t *)param_buf->fixed_param;
	wmi_debug("Fixed param more %d req_id %d status %d", ev->more,
		  ev->request_id, ev->status);
	params->request_id = ev->request_id;
	params->status = ev->status;

	/* buffer should point to next TLV in event */
	buf_ptr += (curr_tlv_len + WMI_TLV_HDR_SIZE);
	len -= (curr_tlv_len + WMI_TLV_HDR_SIZE);

	curr_tlv_tag = WMITLV_GET_TLVTAG(WMITLV_GET_HDR(buf_ptr));
	curr_tlv_len = WMITLV_GET_TLVLEN(WMITLV_GET_HDR(buf_ptr));

	wmi_debug("curr_tlv_len %d curr_tlv_tag %d rem_len %d", len,
		  curr_tlv_len, curr_tlv_tag);

	while ((len >= curr_tlv_len) &&
	       (curr_tlv_tag >= WMITLV_TAG_FIRST_ARRAY_ENUM)) {
		if (curr_tlv_tag == WMITLV_TAG_ARRAY_STRUC) {
			/* Move to next WMITLV_TAG_ARRAY_STRUC */
			buf_ptr += WMI_TLV_HDR_SIZE;
			len -= WMI_TLV_HDR_SIZE;
			if (len <= 0)
				break;
		}
		curr_tlv_tag = WMITLV_GET_TLVTAG(WMITLV_GET_HDR(buf_ptr));
		curr_tlv_len = WMITLV_GET_TLVLEN(WMITLV_GET_HDR(buf_ptr));

		wmi_debug("curr_tlv_len %d curr_tlv_tag %d rem_len %d",
			  len, curr_tlv_len, curr_tlv_tag);
		if (curr_tlv_len) {
			/* point to the tag inside WMITLV_TAG_ARRAY_STRUC */
			tag_start_ptr = buf_ptr + WMI_TLV_HDR_SIZE;
			curr_tlv_tag = WMITLV_GET_TLVTAG(
						WMITLV_GET_HDR(tag_start_ptr));
			wmi_stats_extract_tag_struct(curr_tlv_tag,
						     (void *)tag_start_ptr,
						     params);
		}
		/* Move to next tag */
		buf_ptr += curr_tlv_len + WMI_TLV_HDR_SIZE;
		len -= (curr_tlv_len + WMI_TLV_HDR_SIZE);

		if (len <= 0)
			break;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_infra_cp_stats_tlv - api to extract stats information from
 * event buffer
 * @wmi_handle:  wmi handle
 * @evt_buf:     event buffer
 * @evt_buf_len: length of the event buffer
 * @params:      buffer to populate more flag
 *
 * Return: QDF_STATUS_SUCCESS on success, else other qdf error values
 */
QDF_STATUS
extract_infra_cp_stats_tlv(wmi_unified_t wmi_handle, void *evt_buf,
			   uint32_t evt_buf_len,
			   struct infra_cp_stats_event *params)
{
	wmi_stats_handler(evt_buf, evt_buf_len, params);
	return QDF_STATUS_SUCCESS;
}

/**
 * prepare_infra_cp_stats_buf() - Allocate and prepate wmi cmd request buffer
 * @wmi_handle: wmi handle
 * @stats_req: Request parameters to be filled in wmi cmd request buffer
 * @req_buf_len: length of the output wmi cmd buffer allocated
 *
 * Return: Valid wmi buffer pointer on success and NULL pointer for failure
 */
static wmi_buf_t
prepare_infra_cp_stats_buf(wmi_unified_t wmi_handle,
			   struct infra_cp_stats_cmd_info *stats_req,
			   uint32_t *req_buf_len)
{
	wmi_request_ctrl_path_stats_cmd_fixed_param *cmd_fixed_param;
	uint32_t index;
	wmi_buf_t req_buf;
	uint8_t *buf_ptr;
	uint32_t *pdev_id_array;
	uint32_t *vdev_id_array;
	uint8_t *mac_addr_array;
	uint32_t *dialog_id_array;
	uint32_t num_pdev_ids = stats_req->num_pdev_ids;
	uint32_t num_vdev_ids = stats_req->num_vdev_ids;
	uint32_t num_mac_addr_list = stats_req->num_mac_addr_list;
	uint32_t num_dialog_ids = INFRA_CP_STATS_MAX_REQ_TWT_DIALOG_ID;

	/* Calculate total buffer length */
	*req_buf_len = (sizeof(wmi_request_ctrl_path_stats_cmd_fixed_param) +
		       WMI_TLV_HDR_SIZE + (sizeof(A_UINT32) * (num_pdev_ids)) +
		       WMI_TLV_HDR_SIZE + sizeof(A_UINT32) * (num_vdev_ids) +
		       WMI_TLV_HDR_SIZE +
		       sizeof(wmi_mac_addr) * (num_mac_addr_list) +
		       WMI_TLV_HDR_SIZE +
		       (sizeof(A_UINT32) * (num_dialog_ids)));
	req_buf = wmi_buf_alloc(wmi_handle, *req_buf_len);
	if (!req_buf)
		return NULL;

	cmd_fixed_param = (wmi_request_ctrl_path_stats_cmd_fixed_param *)
				wmi_buf_data(req_buf);

	/*Set TLV header*/
	WMITLV_SET_HDR(&cmd_fixed_param->tlv_header,
		WMITLV_TAG_STRUC_wmi_request_ctrl_path_stats_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
				wmi_request_ctrl_path_stats_cmd_fixed_param));

	index = get_infra_cp_stats_id(stats_req->stats_id);
	cmd_fixed_param->stats_id_mask = (1 << index);

	cmd_fixed_param->request_id = stats_req->action;
	cmd_fixed_param->action = get_infra_cp_stats_action(stats_req->action);

	buf_ptr = (uint8_t *)cmd_fixed_param;
	/* Setting tlv header for pdev id arrays*/
	buf_ptr = buf_ptr + sizeof(*cmd_fixed_param);
	pdev_id_array = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	WMITLV_SET_HDR(buf_ptr,  WMITLV_TAG_ARRAY_UINT32,
		       sizeof(A_UINT32) * num_pdev_ids);

	/* Setting tlv header for vdev id arrays*/
	buf_ptr = buf_ptr + WMI_TLV_HDR_SIZE +
		  (sizeof(A_UINT32) * num_pdev_ids);
	vdev_id_array = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		       sizeof(A_UINT32) * num_vdev_ids);

	/* Setting tlv header for mac addr arrays*/
	buf_ptr = buf_ptr + WMI_TLV_HDR_SIZE +
		  (sizeof(A_UINT32) * num_vdev_ids);
	mac_addr_array = buf_ptr + WMI_TLV_HDR_SIZE;
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_FIXED_STRUC,
		       sizeof(wmi_mac_addr) * num_mac_addr_list);

	/* Setting tlv header for dialog id arrays*/
	buf_ptr = buf_ptr + WMI_TLV_HDR_SIZE +
		  sizeof(wmi_mac_addr) * num_mac_addr_list;
	dialog_id_array = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		       sizeof(A_UINT32) * num_dialog_ids);

	for (index = 0; index < num_pdev_ids; index++)
		pdev_id_array[index] = stats_req->pdev_id[index];

	for (index = 0; index < num_vdev_ids; index++)
		vdev_id_array[index] = stats_req->vdev_id[index];

	for (index = 0; index < num_mac_addr_list; index++) {
		qdf_mem_copy(mac_addr_array, stats_req->peer_mac_addr[index],
			     QDF_MAC_ADDR_SIZE);
		mac_addr_array += QDF_MAC_ADDR_SIZE;
	}

	dialog_id_array[0] = get_stats_req_twt_dialog_id(stats_req);

	wmi_debug("stats_id_mask 0x%x action 0x%x dialog_id %d",
		  cmd_fixed_param->stats_id_mask, cmd_fixed_param->action,
		  dialog_id_array[0]);
	wmi_debug("num_pdev_ids %d num_vdev_ids %d num_dialog_ids %d \
		   num_mac_addr %d", num_pdev_ids, num_vdev_ids,
		   num_dialog_ids, num_mac_addr_list);

	return req_buf;
}

/**
 * send_infra_cp_stats_request_cmd_tlv() - Prepare and send infra_cp_stats
 * wmi cmd to firmware
 * @wmi_handle: wmi handle
 * @param: Pointer to request structure
 *
 * Return: QDF_STATUS_SUCCESS on Success, other QDF_STATUS error codes
 * on failure
 */
static QDF_STATUS
send_infra_cp_stats_request_cmd_tlv(wmi_unified_t wmi_handle,
				    struct infra_cp_stats_cmd_info *param)
{
	uint32_t len;
	wmi_buf_t buf;
	QDF_STATUS status;

	buf = prepare_infra_cp_stats_buf(wmi_handle, param, &len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	wmi_debug("buf_len %d", len);

	wmi_mtrace(WMI_REQUEST_CTRL_PATH_STATS_CMDID, NO_SESSION, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf,
				      len, WMI_REQUEST_CTRL_PATH_STATS_CMDID);

	if (QDF_IS_STATUS_ERROR(status)) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
send_infra_cp_stats_request_cmd_tlv(wmi_unified_t wmi_handle,
				    struct infra_cp_stats_cmd_info *param)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * send_stats_request_cmd_tlv() - WMI request stats function
 * @param wmi_handle: handle to WMI.
 * @param macaddr: MAC address
 * @param param: pointer to hold stats request parameter
 *
 * Return: 0  on success and -ve on failure.
 */
static QDF_STATUS
send_stats_request_cmd_tlv(wmi_unified_t wmi_handle,
			   uint8_t macaddr[QDF_MAC_ADDR_SIZE],
			   struct stats_request_params *param)
{
	int32_t ret;
	wmi_request_stats_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint16_t len = sizeof(wmi_request_stats_cmd_fixed_param);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_request_stats_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_request_stats_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_request_stats_cmd_fixed_param));
	cmd->stats_id = param->stats_id;
	cmd->vdev_id = param->vdev_id;
	cmd->pdev_id = wmi_handle->ops->convert_pdev_id_host_to_target(
							wmi_handle,
							param->pdev_id);

	WMI_CHAR_ARRAY_TO_MAC_ADDR(macaddr, &cmd->peer_macaddr);

	wmi_debug("STATS REQ STATS_ID:%d VDEV_ID:%d PDEV_ID:%d-->",
		 cmd->stats_id, cmd->vdev_id, cmd->pdev_id);

	wmi_mtrace(WMI_REQUEST_STATS_CMDID, cmd->vdev_id, 0);
	ret = wmi_unified_cmd_send_pm_chk(wmi_handle, buf, len,
					  WMI_REQUEST_STATS_CMDID);

	if (ret) {
		wmi_err("Failed to send stats request to fw =%d", ret);
		wmi_buf_free(buf);
	}

	return qdf_status_from_os_return(ret);
}

#ifdef WLAN_FEATURE_BIG_DATA_STATS
/**
 * send_big_data_stats_request_cmd_tlv () - send big data stats cmd
 * @wmi_handle: wmi handle
 * @param : pointer to command request param
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
send_big_data_stats_request_cmd_tlv(wmi_unified_t wmi_handle,
				    struct stats_request_params *param)
{
	int32_t ret = 0;
	wmi_vdev_get_big_data_p2_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint16_t len = sizeof(wmi_vdev_get_big_data_p2_cmd_fixed_param);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_vdev_get_big_data_p2_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(
		&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_vdev_get_big_data_p2_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN
		(wmi_vdev_get_big_data_p2_cmd_fixed_param));

	cmd->vdev_id = param->vdev_id;

	wmi_debug("STATS VDEV_ID:%d -->", cmd->vdev_id);

	wmi_mtrace(WMI_VDEV_GET_BIG_DATA_P2_CMDID, cmd->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_VDEV_GET_BIG_DATA_P2_CMDID);

	if (ret) {
		wmi_err("Failed to send big data stats request to fw =%d", ret);
		wmi_buf_free(buf);
	}

	return qdf_status_from_os_return(ret);
}
#endif

/**
 * extract_all_stats_counts_tlv() - extract all stats count from event
 * @param wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param stats_param: Pointer to hold stats count
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
extract_all_stats_counts_tlv(wmi_unified_t wmi_handle, void *evt_buf,
			     wmi_host_stats_event *stats_param)
{
	wmi_stats_event_fixed_param *ev;
	wmi_per_chain_rssi_stats *rssi_event;
	WMI_UPDATE_STATS_EVENTID_param_tlvs *param_buf;
	uint64_t min_data_len;
	uint32_t i;

	qdf_mem_zero(stats_param, sizeof(*stats_param));
	param_buf = (WMI_UPDATE_STATS_EVENTID_param_tlvs *) evt_buf;
	ev = (wmi_stats_event_fixed_param *) param_buf->fixed_param;
	rssi_event = param_buf->chain_stats;
	if (!ev) {
		wmi_err("event fixed param NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (param_buf->num_data > WMI_SVC_MSG_MAX_SIZE - sizeof(*ev)) {
		wmi_err("num_data : %u is invalid", param_buf->num_data);
		return QDF_STATUS_E_FAULT;
	}

	for (i = 1; i <= WMI_REQUEST_VDEV_EXTD_STAT; i = i << 1) {
		switch (ev->stats_id & i) {
		case WMI_REQUEST_PEER_STAT:
			stats_param->stats_id |= WMI_HOST_REQUEST_PEER_STAT;
			break;

		case WMI_REQUEST_AP_STAT:
			stats_param->stats_id |= WMI_HOST_REQUEST_AP_STAT;
			break;

		case WMI_REQUEST_PDEV_STAT:
			stats_param->stats_id |= WMI_HOST_REQUEST_PDEV_STAT;
			break;

		case WMI_REQUEST_VDEV_STAT:
			stats_param->stats_id |= WMI_HOST_REQUEST_VDEV_STAT;
			break;

		case WMI_REQUEST_BCNFLT_STAT:
			stats_param->stats_id |= WMI_HOST_REQUEST_BCNFLT_STAT;
			break;

		case WMI_REQUEST_VDEV_RATE_STAT:
			stats_param->stats_id |=
				WMI_HOST_REQUEST_VDEV_RATE_STAT;
			break;

		case WMI_REQUEST_BCN_STAT:
			stats_param->stats_id |= WMI_HOST_REQUEST_BCN_STAT;
			break;
		case WMI_REQUEST_PEER_EXTD_STAT:
			stats_param->stats_id |= WMI_REQUEST_PEER_EXTD_STAT;
			break;

		case WMI_REQUEST_PEER_EXTD2_STAT:
			stats_param->stats_id |=
				WMI_HOST_REQUEST_PEER_ADV_STATS;
			break;

		case WMI_REQUEST_PMF_BCN_PROTECT_STAT:
			stats_param->stats_id |=
				WMI_HOST_REQUEST_PMF_BCN_PROTECT_STAT;
			break;

		case WMI_REQUEST_VDEV_EXTD_STAT:
			stats_param->stats_id |=
				WMI_HOST_REQUEST_VDEV_PRB_FILS_STAT;
			break;
		}
	}

	/* ev->num_*_stats may cause uint32_t overflow, so use uint64_t
	 * to save total length calculated
	 */
	min_data_len =
		(((uint64_t)ev->num_pdev_stats) * sizeof(wmi_pdev_stats)) +
		(((uint64_t)ev->num_vdev_stats) * sizeof(wmi_vdev_stats)) +
		(((uint64_t)ev->num_peer_stats) * sizeof(wmi_peer_stats)) +
		(((uint64_t)ev->num_bcnflt_stats) *
		 sizeof(wmi_bcnfilter_stats_t)) +
		(((uint64_t)ev->num_chan_stats) * sizeof(wmi_chan_stats)) +
		(((uint64_t)ev->num_mib_stats) * sizeof(wmi_mib_stats)) +
		(((uint64_t)ev->num_bcn_stats) * sizeof(wmi_bcn_stats)) +
		(((uint64_t)ev->num_peer_extd_stats) *
		 sizeof(wmi_peer_extd_stats)) +
		(((uint64_t)ev->num_mib_extd_stats) *
		 sizeof(wmi_mib_extd_stats));
	if (param_buf->num_data != min_data_len) {
		wmi_err("data len: %u isn't same as calculated: %llu",
			 param_buf->num_data, min_data_len);
		return QDF_STATUS_E_FAULT;
	}

	stats_param->last_event = ev->last_event;
	stats_param->num_pdev_stats = ev->num_pdev_stats;
	stats_param->num_pdev_ext_stats = 0;
	stats_param->num_vdev_stats = ev->num_vdev_stats;
	stats_param->num_peer_stats = ev->num_peer_stats;
	stats_param->num_peer_extd_stats = ev->num_peer_extd_stats;
	stats_param->num_bcnflt_stats = ev->num_bcnflt_stats;
	stats_param->num_chan_stats = ev->num_chan_stats;
	stats_param->num_mib_stats = ev->num_mib_stats;
	stats_param->num_mib_extd_stats = ev->num_mib_extd_stats;
	stats_param->num_bcn_stats = ev->num_bcn_stats;
	stats_param->pdev_id = wmi_handle->ops->convert_pdev_id_target_to_host(
							wmi_handle,
							ev->pdev_id);

	/* if chain_stats is not populated */
	if (!param_buf->chain_stats || !param_buf->num_chain_stats)
		return QDF_STATUS_SUCCESS;

	if (WMITLV_TAG_STRUC_wmi_per_chain_rssi_stats !=
	    WMITLV_GET_TLVTAG(rssi_event->tlv_header))
		return QDF_STATUS_SUCCESS;

	if (WMITLV_GET_STRUCT_TLVLEN(wmi_per_chain_rssi_stats) !=
	    WMITLV_GET_TLVLEN(rssi_event->tlv_header))
		return QDF_STATUS_SUCCESS;

	if (rssi_event->num_per_chain_rssi_stats >=
	    WMITLV_GET_TLVLEN(rssi_event->tlv_header)) {
		wmi_err("num_per_chain_rssi_stats:%u is out of bounds",
			 rssi_event->num_per_chain_rssi_stats);
		return QDF_STATUS_E_INVAL;
	}
	stats_param->num_rssi_stats = rssi_event->num_per_chain_rssi_stats;

	if (param_buf->vdev_extd_stats)
		stats_param->num_vdev_extd_stats =
			param_buf->num_vdev_extd_stats;

	/* if peer_adv_stats is not populated */
	if (param_buf->num_peer_extd2_stats)
		stats_param->num_peer_adv_stats =
			param_buf->num_peer_extd2_stats;

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_pdev_tx_stats() - extract pdev tx stats from event
 */
static void extract_pdev_tx_stats(wmi_host_dbg_tx_stats *tx,
				  struct wlan_dbg_tx_stats *tx_stats)
{
	/* Tx Stats */
	tx->comp_queued = tx_stats->comp_queued;
	tx->comp_delivered = tx_stats->comp_delivered;
	tx->msdu_enqued = tx_stats->msdu_enqued;
	tx->mpdu_enqued = tx_stats->mpdu_enqued;
	tx->wmm_drop = tx_stats->wmm_drop;
	tx->local_enqued = tx_stats->local_enqued;
	tx->local_freed = tx_stats->local_freed;
	tx->hw_queued = tx_stats->hw_queued;
	tx->hw_reaped = tx_stats->hw_reaped;
	tx->underrun = tx_stats->underrun;
	tx->tx_abort = tx_stats->tx_abort;
	tx->mpdus_requed = tx_stats->mpdus_requed;
	tx->data_rc = tx_stats->data_rc;
	tx->self_triggers = tx_stats->self_triggers;
	tx->sw_retry_failure = tx_stats->sw_retry_failure;
	tx->illgl_rate_phy_err = tx_stats->illgl_rate_phy_err;
	tx->pdev_cont_xretry = tx_stats->pdev_cont_xretry;
	tx->pdev_tx_timeout = tx_stats->pdev_tx_timeout;
	tx->pdev_resets = tx_stats->pdev_resets;
	tx->stateless_tid_alloc_failure = tx_stats->stateless_tid_alloc_failure;
	tx->phy_underrun = tx_stats->phy_underrun;
	tx->txop_ovf = tx_stats->txop_ovf;

	return;
}


/**
 * extract_pdev_rx_stats() - extract pdev rx stats from event
 */
static void extract_pdev_rx_stats(wmi_host_dbg_rx_stats *rx,
				  struct wlan_dbg_rx_stats *rx_stats)
{
	/* Rx Stats */
	rx->mid_ppdu_route_change = rx_stats->mid_ppdu_route_change;
	rx->status_rcvd = rx_stats->status_rcvd;
	rx->r0_frags = rx_stats->r0_frags;
	rx->r1_frags = rx_stats->r1_frags;
	rx->r2_frags = rx_stats->r2_frags;
	/* Only TLV */
	rx->r3_frags = 0;
	rx->htt_msdus = rx_stats->htt_msdus;
	rx->htt_mpdus = rx_stats->htt_mpdus;
	rx->loc_msdus = rx_stats->loc_msdus;
	rx->loc_mpdus = rx_stats->loc_mpdus;
	rx->oversize_amsdu = rx_stats->oversize_amsdu;
	rx->phy_errs = rx_stats->phy_errs;
	rx->phy_err_drop = rx_stats->phy_err_drop;
	rx->mpdu_errs = rx_stats->mpdu_errs;

	return;
}

/**
 * extract_pdev_stats_tlv() - extract pdev stats from event
 * @param wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param index: Index into pdev stats
 * @param pdev_stats: Pointer to hold pdev stats
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
extract_pdev_stats_tlv(wmi_unified_t wmi_handle, void *evt_buf, uint32_t index,
		       wmi_host_pdev_stats *pdev_stats)
{
	WMI_UPDATE_STATS_EVENTID_param_tlvs *param_buf;
	wmi_stats_event_fixed_param *ev_param;
	uint8_t *data;

	param_buf = (WMI_UPDATE_STATS_EVENTID_param_tlvs *) evt_buf;
	ev_param = (wmi_stats_event_fixed_param *) param_buf->fixed_param;

	data = param_buf->data;

	if (index < ev_param->num_pdev_stats) {
		wmi_pdev_stats *ev = (wmi_pdev_stats *) ((data) +
				(index * sizeof(wmi_pdev_stats)));

		pdev_stats->chan_nf = ev->chan_nf;
		pdev_stats->tx_frame_count = ev->tx_frame_count;
		pdev_stats->rx_frame_count = ev->rx_frame_count;
		pdev_stats->rx_clear_count = ev->rx_clear_count;
		pdev_stats->cycle_count = ev->cycle_count;
		pdev_stats->phy_err_count = ev->phy_err_count;
		pdev_stats->chan_tx_pwr = ev->chan_tx_pwr;

		extract_pdev_tx_stats(&(pdev_stats->pdev_stats.tx),
			&(ev->pdev_stats.tx));
		extract_pdev_rx_stats(&(pdev_stats->pdev_stats.rx),
			&(ev->pdev_stats.rx));
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_vdev_stats_tlv() - extract vdev stats from event
 * @param wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param index: Index into vdev stats
 * @param vdev_stats: Pointer to hold vdev stats
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_vdev_stats_tlv(wmi_unified_t wmi_handle,
	void *evt_buf, uint32_t index, wmi_host_vdev_stats *vdev_stats)
{
	WMI_UPDATE_STATS_EVENTID_param_tlvs *param_buf;
	wmi_stats_event_fixed_param *ev_param;
	uint8_t *data;

	param_buf = (WMI_UPDATE_STATS_EVENTID_param_tlvs *) evt_buf;
	ev_param = (wmi_stats_event_fixed_param *) param_buf->fixed_param;
	data = (uint8_t *) param_buf->data;

	if (index < ev_param->num_vdev_stats) {
		wmi_vdev_stats *ev = (wmi_vdev_stats *) ((data) +
				((ev_param->num_pdev_stats) *
				sizeof(wmi_pdev_stats)) +
				(index * sizeof(wmi_vdev_stats)));

		vdev_stats->vdev_id = ev->vdev_id;
		vdev_stats->vdev_snr.bcn_snr = ev->vdev_snr.bcn_snr;
		vdev_stats->vdev_snr.dat_snr = ev->vdev_snr.dat_snr;

		OS_MEMCPY(vdev_stats->tx_frm_cnt, ev->tx_frm_cnt,
			sizeof(ev->tx_frm_cnt));
		vdev_stats->rx_frm_cnt = ev->rx_frm_cnt;
		OS_MEMCPY(vdev_stats->multiple_retry_cnt,
				ev->multiple_retry_cnt,
				sizeof(ev->multiple_retry_cnt));
		OS_MEMCPY(vdev_stats->fail_cnt, ev->fail_cnt,
				sizeof(ev->fail_cnt));
		vdev_stats->rts_fail_cnt = ev->rts_fail_cnt;
		vdev_stats->rts_succ_cnt = ev->rts_succ_cnt;
		vdev_stats->rx_err_cnt = ev->rx_err_cnt;
		vdev_stats->rx_discard_cnt = ev->rx_discard_cnt;
		vdev_stats->ack_fail_cnt = ev->ack_fail_cnt;
		OS_MEMCPY(vdev_stats->tx_rate_history, ev->tx_rate_history,
			sizeof(ev->tx_rate_history));
		OS_MEMCPY(vdev_stats->bcn_rssi_history, ev->bcn_rssi_history,
			sizeof(ev->bcn_rssi_history));

	}

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_peer_stats_tlv() - extract peer stats from event
 * @param wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param index: Index into peer stats
 * @param peer_stats: Pointer to hold peer stats
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
extract_peer_stats_tlv(wmi_unified_t wmi_handle, void *evt_buf, uint32_t index,
		       wmi_host_peer_stats *peer_stats)
{
	WMI_UPDATE_STATS_EVENTID_param_tlvs *param_buf;
	wmi_stats_event_fixed_param *ev_param;
	uint8_t *data;

	param_buf = (WMI_UPDATE_STATS_EVENTID_param_tlvs *) evt_buf;
	ev_param = (wmi_stats_event_fixed_param *) param_buf->fixed_param;
	data = (uint8_t *) param_buf->data;

	if (index < ev_param->num_peer_stats) {
		wmi_peer_stats *ev = (wmi_peer_stats *) ((data) +
			((ev_param->num_pdev_stats) * sizeof(wmi_pdev_stats)) +
			((ev_param->num_vdev_stats) * sizeof(wmi_vdev_stats)) +
			(index * sizeof(wmi_peer_stats)));

		OS_MEMSET(peer_stats, 0, sizeof(wmi_host_peer_stats));

		OS_MEMCPY(&(peer_stats->peer_macaddr),
			&(ev->peer_macaddr), sizeof(wmi_mac_addr));

		peer_stats->peer_rssi = ev->peer_rssi;
		peer_stats->peer_tx_rate = ev->peer_tx_rate;
		peer_stats->peer_rx_rate = ev->peer_rx_rate;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_peer_extd_stats_tlv() - extract extended peer stats from event
 * @param wmi_handle: wmi handle
 * @param evt_buf: pointer to event buffer
 * @param index: Index into extended peer stats
 * @param peer_extd_stats: Pointer to hold extended peer stats
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS
extract_peer_extd_stats_tlv(wmi_unified_t wmi_handle,
			    void *evt_buf, uint32_t index,
			    wmi_host_peer_extd_stats *peer_extd_stats)
{
	WMI_UPDATE_STATS_EVENTID_param_tlvs *param_buf;
	wmi_stats_event_fixed_param *ev_param;
	uint8_t *data;

	param_buf = (WMI_UPDATE_STATS_EVENTID_param_tlvs *)evt_buf;
	ev_param = (wmi_stats_event_fixed_param *)param_buf->fixed_param;
	data = (uint8_t *)param_buf->data;
	if (!data)
		return QDF_STATUS_E_FAILURE;

	if (index < ev_param->num_peer_extd_stats) {
		wmi_peer_extd_stats *ev = (wmi_peer_extd_stats *) (data +
			(ev_param->num_pdev_stats * sizeof(wmi_pdev_stats)) +
			(ev_param->num_vdev_stats * sizeof(wmi_vdev_stats)) +
			(ev_param->num_peer_stats * sizeof(wmi_peer_stats)) +
			(ev_param->num_bcnflt_stats *
			sizeof(wmi_bcnfilter_stats_t)) +
			(ev_param->num_chan_stats * sizeof(wmi_chan_stats)) +
			(ev_param->num_mib_stats * sizeof(wmi_mib_stats)) +
			(ev_param->num_bcn_stats * sizeof(wmi_bcn_stats)) +
			(index * sizeof(wmi_peer_extd_stats)));

		qdf_mem_zero(peer_extd_stats, sizeof(wmi_host_peer_extd_stats));
		qdf_mem_copy(&peer_extd_stats->peer_macaddr, &ev->peer_macaddr,
			     sizeof(wmi_mac_addr));

		peer_extd_stats->rx_mc_bc_cnt = ev->rx_mc_bc_cnt;
	}

	return QDF_STATUS_SUCCESS;

}

/**
 * extract_pmf_bcn_protect_stats_tlv() - extract pmf bcn stats from event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @pmf_bcn_stats: Pointer to hold pmf bcn protect stats
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */

static QDF_STATUS
extract_pmf_bcn_protect_stats_tlv(wmi_unified_t wmi_handle, void *evt_buf,
				  wmi_host_pmf_bcn_protect_stats *pmf_bcn_stats)
{
	WMI_UPDATE_STATS_EVENTID_param_tlvs *param_buf;
	wmi_stats_event_fixed_param *ev_param;

	param_buf = (WMI_UPDATE_STATS_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf)
		return QDF_STATUS_E_FAILURE;

	ev_param = (wmi_stats_event_fixed_param *)param_buf->fixed_param;

	if ((ev_param->stats_id & WMI_REQUEST_PMF_BCN_PROTECT_STAT) &&
	    param_buf->pmf_bcn_protect_stats) {
		pmf_bcn_stats->igtk_mic_fail_cnt =
			param_buf->pmf_bcn_protect_stats->igtk_mic_fail_cnt;
		pmf_bcn_stats->igtk_replay_cnt =
			param_buf->pmf_bcn_protect_stats->igtk_replay_cnt;
		pmf_bcn_stats->bcn_mic_fail_cnt =
			param_buf->pmf_bcn_protect_stats->bcn_mic_fail_cnt;
		pmf_bcn_stats->bcn_replay_cnt =
			param_buf->pmf_bcn_protect_stats->bcn_replay_cnt;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_SUPPORT_INFRA_CTRL_PATH_STATS
static void wmi_infra_cp_stats_ops_attach_tlv(struct wmi_ops *ops)
{
	ops->send_infra_cp_stats_request_cmd =
					send_infra_cp_stats_request_cmd_tlv;
}
#else
static void wmi_infra_cp_stats_ops_attach_tlv(struct wmi_ops *ops)
{
}
#endif /* WLAN_SUPPORT_INFRA_CTRL_PATH_STATS */

static QDF_STATUS send_stats_threshold_tlv(wmi_unified_t wmi_handle,
					   void *threshold)
{
	return QDF_STATUS_SUCCESS;
}

#define WMA_FILL_TX_STATS(eve, msg)   do {\
	(msg)->msdus = (eve)->tx_msdu_cnt;\
	(msg)->mpdus = (eve)->tx_mpdu_cnt;\
	(msg)->ppdus = (eve)->tx_ppdu_cnt;\
	(msg)->bytes = (eve)->tx_bytes;\
	(msg)->drops = (eve)->tx_msdu_drop_cnt;\
	(msg)->drop_bytes = (eve)->tx_drop_bytes;\
	(msg)->retries = (eve)->tx_mpdu_retry_cnt;\
	(msg)->mpdu_failed = (eve)->tx_mpdu_fail_cnt;\
	(msg)->ppdu_failed = (eve)->tx_ppdu_fail_cnt;\
} while (0)

#define WMA_FILL_RX_STATS(eve, msg)       do {\
	(msg)->mpdus = (eve)->mac_rx_mpdu_cnt;\
	(msg)->bytes = (eve)->mac_rx_bytes;\
	(msg)->ppdus = (eve)->phy_rx_ppdu_cnt;\
	(msg)->ppdu_bytes = (eve)->phy_rx_bytes;\
	(msg)->mpdu_retry = (eve)->rx_mpdu_retry_cnt;\
	(msg)->mpdu_dup = (eve)->rx_mpdu_dup_cnt;\
	(msg)->mpdu_discard = (eve)->rx_mpdu_discard_cnt;\
} while (0)

/**
 * get_stats_buf_length_tlv() - calculate buffer size for MAC counters
 * @wmi_handle: WMI handle
 * @evt_buf: stats report event buffer
 *
 * Structure of the stats message
 * LL_EXT_STATS
 *  |
 *  |--Channel stats[1~n]
 *  |--Peer[1~n]
 *      |
 *      +---Signal
 *      +---TX
 *      |    +---BE
 *      |    +---BK
 *      |    +---VI
 *      |    +---VO
 *      |
 *      +---RX
 *           +---BE
 *           +---BK
 *           +---VI
 *           +---VO
 * For each Access Category, the arregation and mcs
 * stats are as this:
 *  TX
 *   +-BE/BK/VI/VO
 *         +----tx_mpdu_aggr_array
 *         +----tx_succ_mcs_array
 *         +----tx_fail_mcs_array
 *         +----tx_delay_array
 *  RX
 *   +-BE/BK/VI/VO
 *         +----rx_mpdu_aggr_array
 *         +----rx_mcs_array
 *
 * return: length of result buffer.
 */
static uint32_t
get_stats_buf_length_tlv(wmi_unified_t wmi_handle, void *evt_buf)
{
	WMI_REPORT_STATS_EVENTID_param_tlvs *param_buf;
	wmi_report_stats_event_fixed_param *fixed_param;
	uint32_t buf_len, peer_num;
	uint32_t total_array_len, total_peer_len;

	param_buf = (WMI_REPORT_STATS_EVENTID_param_tlvs *)evt_buf;
	if (!param_buf) {
		wmi_err("Invalid input parameters.");
		return 0;
	}
	fixed_param = param_buf->fixed_param;

	wmi_debug("num_peer_signal_stats=%d num_peer_ac_tx_stats=%d num_peer_ac_rx_stats=%d",
		  fixed_param->num_peer_signal_stats,
		  fixed_param->num_peer_ac_tx_stats,
		  fixed_param->num_peer_ac_rx_stats);
	/* Get the MAX of three peer numbers */
	peer_num = fixed_param->num_peer_signal_stats >
			fixed_param->num_peer_ac_tx_stats ?
			fixed_param->num_peer_signal_stats :
			fixed_param->num_peer_ac_tx_stats;
	peer_num = peer_num > fixed_param->num_peer_ac_rx_stats ?
			peer_num : fixed_param->num_peer_ac_rx_stats;

	if (peer_num == 0) {
		wmi_err("%s: Peer number is zero", __func__);
		return -EINVAL;
	}
	wmi_debug("num_chan_cca_stats=%d tx_mpdu_aggr_array_len=%d tx_succ_mcs_array_len=%d tx_fail_mcs_array_len=%d tx_ppdu_delay_bin_size_ms=%d tx_ppdu_delay_array_len=%d rx_mpdu_aggr_array_len=%d rx_mcs_array_len=%d stats_period_array_len=%d",
		  fixed_param->num_chan_cca_stats,
		  fixed_param->tx_mpdu_aggr_array_len,
		  fixed_param->tx_succ_mcs_array_len,
		  fixed_param->tx_fail_mcs_array_len,
		  fixed_param->tx_ppdu_delay_bin_size_ms,
		  fixed_param->tx_ppdu_delay_array_len,
		  fixed_param->rx_mpdu_aggr_array_len,
		  fixed_param->rx_mcs_array_len,
		  fixed_param->stats_period_array_len);
	/*
	 * Result buffer has a structure like this:
	 *     ---------------------------------
	 *     |      trigger_cond_i           |
	 *     +-------------------------------+
	 *     |      cca_chgd_bitmap          |
	 *     +-------------------------------+
	 *     |      sig_chgd_bitmap          |
	 *     +-------------------------------+
	 *     |      tx_chgd_bitmap           |
	 *     +-------------------------------+
	 *     |      rx_chgd_bitmap           |
	 *     +-------------------------------+
	 *     |      peer_num                 |
	 *     +-------------------------------+
	 *     |      channel_num              |
	 *     +-------------------------------+
	 *     |      time stamp               |
	 *     +-------------------------------+
	 *     |      tx_mpdu_aggr_array_len   |
	 *     +-------------------------------+
	 *     |      tx_succ_mcs_array_len    |
	 *     +-------------------------------+
	 *     |      tx_fail_mcs_array_len    |
	 *     +-------------------------------+
	 *     |      tx_delay_array_len       |
	 *     +-------------------------------+
	 *     |      rx_mpdu_aggr_array_len   |
	 *     +-------------------------------+
	 *     |      rx_mcs_array_len         |
	 *     +-------------------------------+
	 *     |      pointer to CCA stats     |
	 *     +-------------------------------+
	 *     |      CCA stats                |
	 *     +-------------------------------+
	 *     |      peer_stats               |----+
	 *     +-------------------------------+    |
	 *     | TX aggr/mcs parameters array  |    |
	 *     | Length of this buffer is      |    |
	 *     | not fixed.                    |<-+ |
	 *     +-------------------------------+  | |
	 *     |      per peer tx stats        |--+ |
	 *     |         BE                    | <--+
	 *     |         BK                    |    |
	 *     |         VI                    |    |
	 *     |         VO                    |    |
	 *     +-------------------------------+    |
	 *     | TX aggr/mcs parameters array  |    |
	 *     | Length of this buffer is      |    |
	 *     | not fixed.                    |<-+ |
	 *     +-------------------------------+  | |
	 *     |      peer peer rx stats       |--+ |
	 *     |         BE                    | <--+
	 *     |         BK                    |
	 *     |         VI                    |
	 *     |         VO                    |
	 *     ---------------------------------
	 */
	buf_len = sizeof(struct wmi_link_layer_stats);

	if (fixed_param->num_chan_cca_stats > (WMI_SVC_MSG_MAX_SIZE /
				      sizeof(struct wmi_wifi_chan_cca_stats)))
		goto excess_data;

	buf_len += (fixed_param->num_chan_cca_stats *
			sizeof(struct wmi_wifi_chan_cca_stats));
	if (fixed_param->tx_mpdu_aggr_array_len > WMI_SVC_MSG_MAX_SIZE)
		goto excess_data;
	total_array_len = fixed_param->tx_mpdu_aggr_array_len;

	if (fixed_param->tx_succ_mcs_array_len >
			(WMI_SVC_MSG_MAX_SIZE - total_array_len))
		goto excess_data;

	total_array_len += fixed_param->tx_succ_mcs_array_len;

	if (fixed_param->tx_fail_mcs_array_len >
			(WMI_SVC_MSG_MAX_SIZE - total_array_len))
		goto excess_data;

	total_array_len += fixed_param->tx_fail_mcs_array_len;

	if (fixed_param->tx_ppdu_delay_array_len >
			(WMI_SVC_MSG_MAX_SIZE - total_array_len))
		goto excess_data;

	total_array_len += fixed_param->tx_ppdu_delay_array_len;

	if (fixed_param->rx_mpdu_aggr_array_len >
			(WMI_SVC_MSG_MAX_SIZE - total_array_len))
		goto excess_data;

	total_array_len += fixed_param->rx_mpdu_aggr_array_len;

	if (fixed_param->rx_mcs_array_len >
			(WMI_SVC_MSG_MAX_SIZE - total_array_len))
		goto excess_data;

	total_array_len += fixed_param->rx_mcs_array_len;

	if (total_array_len > (WMI_SVC_MSG_MAX_SIZE /
					(peer_num * WLAN_MAX_AC)))
		goto excess_data;

	total_peer_len = (sizeof(uint32_t) * total_array_len +
			  sizeof(struct wmi_wifi_tx) +
			  sizeof(struct wmi_wifi_rx)) *
			  WLAN_MAX_AC +
			  sizeof(struct wmi_wifi_ll_peer_stats);
	buf_len += peer_num * total_peer_len;

	wmi_debug("peer_num=%d wlan counter needs %d bytes", peer_num, buf_len);
	return buf_len + 64;

excess_data:
	wmi_err("%s: excess wmi buffer: peer %d cca %d tx_mpdu %d ",
		 __func__, peer_num, fixed_param->num_chan_cca_stats,
		 fixed_param->tx_mpdu_aggr_array_len);
	wmi_err("tx_succ %d tx_fail %d tx_ppdu %d ",
		 fixed_param->tx_succ_mcs_array_len,
		 fixed_param->tx_fail_mcs_array_len,
		 fixed_param->tx_ppdu_delay_array_len);
	wmi_err("rx_mpdu %d rx_mcs %d",
		 fixed_param->rx_mpdu_aggr_array_len,
		 fixed_param->rx_mcs_array_len);
	return 0;
}

/**
 * extract_ll_tx_stats() - Fix TX stats into result buffer
 * @ll_stats: LL stats buffer
 * @fix_param: parameters with fixed length in WMI event
 * @param_buf: parameters without fixed length in WMI event
 * @buf: buffer for result
 * @buf_length: buffer length
 *
 * Return: TX stats result length
 */
static size_t extract_ll_tx_stats(struct wmi_link_layer_stats *ll_stats,
			wmi_report_stats_event_fixed_param *fix_param,
			WMI_REPORT_STATS_EVENTID_param_tlvs *param_buf,
			uint8_t *buf,
			uint32_t buf_length)
{
	uint8_t *result;
	uint32_t i, j, k;
	wmi_peer_ac_tx_stats *wmi_peer_tx;
	wmi_tx_stats *wmi_tx;
	struct wmi_wifi_tx *tx_stats;
	struct wmi_wifi_ll_peer_stats *peer_stats;
	uint8_t *counter;
	uint32_t *tx_mpdu_aggr, *tx_succ_mcs, *tx_fail_mcs, *tx_delay;
	uint32_t len, dst_len, param_len, tx_mpdu_aggr_array_len,
		 tx_succ_mcs_array_len, tx_fail_mcs_array_len,
		 tx_delay_array_len;

	result = buf;
	dst_len = buf_length;
	tx_mpdu_aggr_array_len = fix_param->tx_mpdu_aggr_array_len;
	ll_stats->tx_mpdu_aggr_array_len = tx_mpdu_aggr_array_len;
	tx_succ_mcs_array_len = fix_param->tx_succ_mcs_array_len;
	ll_stats->tx_succ_mcs_array_len = tx_succ_mcs_array_len;
	tx_fail_mcs_array_len = fix_param->tx_fail_mcs_array_len;
	ll_stats->tx_fail_mcs_array_len = tx_fail_mcs_array_len;
	tx_delay_array_len = fix_param->tx_ppdu_delay_array_len;
	ll_stats->tx_delay_array_len = tx_delay_array_len;
	wmi_peer_tx = param_buf->peer_ac_tx_stats;
	wmi_tx = param_buf->tx_stats;

	len = fix_param->num_peer_ac_tx_stats *
		WLAN_MAX_AC * tx_mpdu_aggr_array_len;
	param_len = param_buf->num_tx_mpdu_aggr * sizeof(uint32_t);
	if (len * sizeof(uint32_t) <= dst_len && len <= param_len) {
		tx_mpdu_aggr = (uint32_t *)result;
		counter = (uint8_t *)param_buf->tx_mpdu_aggr;
		for (i = 0; i < len; i++) {
			result[4 * i] = counter[2 * i];
			result[4 * i + 1] = counter[2 * i + 1];
		}
		wmi_debug("Tx MPDU aggregation array:");
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_DEBUG,
				   param_buf->tx_mpdu_aggr,
				   len * sizeof(uint16_t));
		result += len * sizeof(uint32_t);
		dst_len -= len * sizeof(uint32_t);
	} else {
		wmi_err("TX_MPDU_AGGR invalid arg, %d, %d, %d",
			 len, dst_len, param_len);
		tx_mpdu_aggr = NULL;
	}

	len = fix_param->num_peer_ac_tx_stats * WLAN_MAX_AC *
		tx_succ_mcs_array_len;
	param_len = param_buf->num_tx_succ_mcs * sizeof(uint32_t);
	if (len * sizeof(uint32_t) <= dst_len && len <= param_len) {
		tx_succ_mcs = (uint32_t *)result;
		counter = (uint8_t *)param_buf->tx_succ_mcs;
		for (i = 0; i < len; i++) {
			result[4 * i] = counter[2 * i];
			result[4 * i + 1] = counter[2 * i + 1];
		}
		wmi_debug("Tx success MCS array:");
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_DEBUG,
				   param_buf->tx_succ_mcs,
				   len * sizeof(uint16_t));
		len *= sizeof(uint32_t);
		result += len;
		dst_len -= len;
	} else {
		wmi_err("TX_SUCC_MCS invalid arg, %d, %d, %d",
			 len, dst_len, param_len);
		tx_succ_mcs = NULL;
	}

	len = fix_param->num_peer_ac_tx_stats * WLAN_MAX_AC *
		tx_fail_mcs_array_len;
	param_len = param_buf->num_tx_fail_mcs * sizeof(uint32_t);
	if (len * sizeof(uint32_t) <= dst_len && len <= param_len) {
		tx_fail_mcs = (uint32_t *)result;
		counter = (uint8_t *)param_buf->tx_fail_mcs;
		for (i = 0; i < len; i++) {
			result[4 * i] = counter[2 * i];
			result[4 * i + 1] = counter[2 * i + 1];
		}
		wmi_debug("Tx Failes MCS array:");
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_DEBUG,
				   param_buf->tx_fail_mcs,
				   len * sizeof(uint16_t));
		len *= sizeof(uint32_t);
		result += len;
		dst_len -= len;
	} else {
		wmi_err("TX_SUCC_MCS invalid arg, %d, %d, %d",
			 len, dst_len, param_len);
		tx_fail_mcs = NULL;
	}

	len = fix_param->num_peer_ac_tx_stats *
		WLAN_MAX_AC * tx_delay_array_len;
	param_len = param_buf->num_tx_ppdu_delay * sizeof(uint32_t);
	if (len * sizeof(uint32_t) <= dst_len && len <= param_len) {
		tx_delay = (uint32_t *)result;
		counter = (uint8_t *)param_buf->tx_ppdu_delay;
		for (i = 0; i < len; i++) {
			result[4 * i] = counter[2 * i];
			result[4 * i + 1] = counter[2 * i + 1];
		}
		wmi_debug("Tx PPDU delay array:");
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_DEBUG,
				   param_buf->tx_ppdu_delay,
				   len * sizeof(uint16_t));
		len *= sizeof(uint32_t);
		result += len;
		dst_len -= len;
	} else {
		wmi_err("TX_DELAY invalid arg, %d, %d, %d",
			 len, dst_len, param_len);
		tx_delay = NULL;
	}

	/* per peer tx stats */
	peer_stats = ll_stats->peer_stats;

	for (i = 0; i < fix_param->num_peer_ac_tx_stats; i++) {
		uint32_t peer_id = wmi_peer_tx[i].peer_id;
		struct wmi_wifi_tx *ac;
		wmi_tx_stats *wmi_tx_stats;

		for (j = 0; j < ll_stats->peer_num; j++) {
			peer_stats += j;
			if (peer_stats->peer_id == WIFI_INVALID_PEER_ID ||
			    peer_stats->peer_id == peer_id)
				break;
		}

		if (j < ll_stats->peer_num) {
			peer_stats->peer_id = wmi_peer_tx[i].peer_id;
			peer_stats->vdev_id = wmi_peer_tx[i].vdev_id;
			tx_stats = (struct wmi_wifi_tx *)result;
			for (k = 0; k < WLAN_MAX_AC; k++) {
				wmi_tx_stats = &wmi_tx[i * WLAN_MAX_AC + k];
				ac = &tx_stats[k];
				WMA_FILL_TX_STATS(wmi_tx_stats, ac);
				ac->mpdu_aggr_size = tx_mpdu_aggr;
				ac->aggr_len = tx_mpdu_aggr_array_len *
							sizeof(uint32_t);
				ac->success_mcs = tx_succ_mcs;
				ac->success_mcs_len = tx_succ_mcs_array_len *
							sizeof(uint32_t);
				ac->fail_mcs = tx_fail_mcs;
				ac->fail_mcs_len = tx_fail_mcs_array_len *
							sizeof(uint32_t);
				ac->delay = tx_delay;
				ac->delay_len = tx_delay_array_len *
							sizeof(uint32_t);
				peer_stats->ac_stats[k].tx_stats = ac;
				peer_stats->ac_stats[k].type = k;
				tx_mpdu_aggr += tx_mpdu_aggr_array_len;
				tx_succ_mcs += tx_succ_mcs_array_len;
				tx_fail_mcs += tx_fail_mcs_array_len;
				tx_delay += tx_delay_array_len;
			}
			result += WLAN_MAX_AC * sizeof(struct wmi_wifi_tx);
			dst_len -= WLAN_MAX_AC * sizeof(struct wmi_wifi_tx);
		} else {
			/*
			 * Buffer for Peer TX counter overflow.
			 * There is peer ID mismatch between TX, RX,
			 * signal counters.
			 */
			wmi_err("One peer TX info is dropped.");

			tx_mpdu_aggr += tx_mpdu_aggr_array_len * WLAN_MAX_AC;
			tx_succ_mcs += tx_succ_mcs_array_len * WLAN_MAX_AC;
			tx_fail_mcs += tx_fail_mcs_array_len * WLAN_MAX_AC;
			tx_delay += tx_delay_array_len * WLAN_MAX_AC;
		}
	}
	return result - buf;
}

/**
 * extract_ll_rx_stats() - Fix RX stats into result buffer
 * @ll_stats: LL stats buffer
 * @fix_param: parameters with fixed length in WMI event
 * @param_buf: parameters without fixed length in WMI event
 * @buf: buffer for result
 * @buf_length: buffer length
 *
 * Return: RX stats result length
 */
static size_t extract_ll_rx_stats(struct wmi_link_layer_stats *ll_stats,
				wmi_report_stats_event_fixed_param *fix_param,
				WMI_REPORT_STATS_EVENTID_param_tlvs *param_buf,
				uint8_t *buf,
				uint32_t buf_length)
{
	uint8_t *result;
	uint32_t i, j, k;
	uint32_t *rx_mpdu_aggr, *rx_mcs;
	wmi_rx_stats *wmi_rx;
	wmi_peer_ac_rx_stats *wmi_peer_rx;
	struct wmi_wifi_rx *rx_stats;
	struct wmi_wifi_ll_peer_stats *peer_stats;
	uint32_t len, dst_len, param_len,
		 rx_mpdu_aggr_array_len, rx_mcs_array_len;
	uint8_t *counter;

	rx_mpdu_aggr_array_len = fix_param->rx_mpdu_aggr_array_len;
	ll_stats->rx_mpdu_aggr_array_len = rx_mpdu_aggr_array_len;
	rx_mcs_array_len = fix_param->rx_mcs_array_len;
	ll_stats->rx_mcs_array_len = rx_mcs_array_len;
	wmi_peer_rx = param_buf->peer_ac_rx_stats;
	wmi_rx = param_buf->rx_stats;

	result = buf;
	dst_len = buf_length;
	len = fix_param->num_peer_ac_rx_stats *
		  WLAN_MAX_AC * rx_mpdu_aggr_array_len;
	param_len = param_buf->num_rx_mpdu_aggr * sizeof(uint32_t);
	if (len * sizeof(uint32_t) <= dst_len && len <= param_len) {
		rx_mpdu_aggr = (uint32_t *)result;
		counter = (uint8_t *)param_buf->rx_mpdu_aggr;
		for (i = 0; i < len; i++) {
			result[4 * i] = counter[2 * i];
			result[4 * i + 1] = counter[2 * i + 1];
		}
		wmi_debug("Rx PPDU delay array:");
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_DEBUG,
				   param_buf->rx_mpdu_aggr,
				   len * sizeof(uint16_t));
		len *= sizeof(uint32_t);
		result += len;
		dst_len -= len;
	} else {
		wmi_err("RX_MPDU_AGGR invalid arg %d, %d, %d",
			 len, dst_len, param_len);
		rx_mpdu_aggr = NULL;
	}

	len = fix_param->num_peer_ac_rx_stats *
		  WLAN_MAX_AC * rx_mcs_array_len;
	param_len = param_buf->num_rx_mcs * sizeof(uint32_t);
	if (len * sizeof(uint32_t) <= dst_len && len <= param_len) {
		rx_mcs = (uint32_t *)result;
		counter = (uint8_t *)param_buf->rx_mcs;
		for (i = 0; i < len; i++) {
			result[4 * i] = counter[2 * i];
			result[4 * i + 1] = counter[2 * i + 1];
		}
		wmi_debug("Rx MCS array:");
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_DEBUG,
				   param_buf->rx_mcs,
				   len * sizeof(uint16_t));
		len *= sizeof(uint32_t);
		result += len;
		dst_len -= len;
	} else {
		wmi_err("RX_MCS invalid arg %d, %d, %d",
			 len, dst_len, param_len);
		rx_mcs = NULL;
	}

	/* per peer rx stats */
	peer_stats = ll_stats->peer_stats;
	for (i = 0; i < fix_param->num_peer_ac_rx_stats; i++) {
		uint32_t peer_id = wmi_peer_rx[i].peer_id;
		struct wmi_wifi_rx *ac;
		wmi_rx_stats *wmi_rx_stats;

		for (j = 0; j < ll_stats->peer_num; j++) {
			peer_stats += j;
			if ((peer_stats->peer_id == WIFI_INVALID_PEER_ID) ||
			    (peer_stats->peer_id == peer_id))
				break;
		}

		if (j < ll_stats->peer_num) {
			peer_stats->peer_id = wmi_peer_rx[i].peer_id;
			peer_stats->vdev_id = wmi_peer_rx[i].vdev_id;
			peer_stats->sta_ps_inds = wmi_peer_rx[i].sta_ps_inds;
			peer_stats->sta_ps_durs = wmi_peer_rx[i].sta_ps_durs;
			peer_stats->rx_probe_reqs =
						wmi_peer_rx[i].rx_probe_reqs;
			peer_stats->rx_oth_mgmts = wmi_peer_rx[i].rx_oth_mgmts;
			rx_stats = (struct wmi_wifi_rx *)result;

			for (k = 0; k < WLAN_MAX_AC; k++) {
				wmi_rx_stats = &wmi_rx[i * WLAN_MAX_AC + k];
				ac = &rx_stats[k];
				WMA_FILL_RX_STATS(wmi_rx_stats, ac);
				ac->mpdu_aggr = rx_mpdu_aggr;
				ac->aggr_len = rx_mpdu_aggr_array_len *
							sizeof(uint32_t);
				ac->mcs = rx_mcs;
				ac->mcs_len = rx_mcs_array_len *
							sizeof(uint32_t);
				peer_stats->ac_stats[k].rx_stats = ac;
				peer_stats->ac_stats[k].type = k;
				rx_mpdu_aggr += rx_mpdu_aggr_array_len;
				rx_mcs += rx_mcs_array_len;
			}
			result += WLAN_MAX_AC * sizeof(struct wmi_wifi_rx);
			dst_len -= WLAN_MAX_AC * sizeof(struct wmi_wifi_rx);
		} else {
			/*
			 * Buffer for Peer RX counter overflow.
			 * There is peer ID mismatch between TX, RX,
			 * signal counters.
			 */
			wmi_err("One peer RX info is dropped.");
			rx_mpdu_aggr += rx_mpdu_aggr_array_len * WLAN_MAX_AC;
			rx_mcs += rx_mcs_array_len * WLAN_MAX_AC;
		}
	}
	return result - buf;
}

/**
 * extract_ll_stats_time_stamp() - extract log indication timestamp and
 *  counting duration.
 * @period - counting period on FW side
 * @time_stamp - time stamp for user layer
 *
 * return: none
 */
static void etract_ll_stats_time_stamp(wmi_stats_period *period,
				       struct wmi_wifi_ll_period *time_stamp)
{
	time_stamp->end_time = qdf_system_ticks();
	if (!period) {
		wmi_err("Period buf is null.");
		time_stamp->duration = 0;
		return;
	}
	wmi_debug("On fw side, start time is %d, start count is %d ",
		  period->start_low_freq_msec, period->start_low_freq_count);
	time_stamp->duration = period->end_low_freq_msec -
				period->start_low_freq_msec;
}

/**
 * extract_ll_stats_tlv() - handler for MAC layer counters.
 * @handle - wma handle
 * @event - FW event
 * @len - length of FW event
 *
 * return: 0 success.
 */
static QDF_STATUS extract_ll_stats_tlv(wmi_unified_t wmi_handle, void *evt_buf,
				       struct wmi_link_layer_stats *stats)
{
	WMI_REPORT_STATS_EVENTID_param_tlvs *param_buf;
	wmi_report_stats_event_fixed_param *fixed_param;
	wmi_chan_cca_stats *wmi_cca_stats;
	wmi_peer_signal_stats *wmi_peer_signal;
	wmi_peer_ac_rx_stats *wmi_peer_rx;
	wmi_stats_period *period;
	wmi_stats_interference *intf_stats;
	struct wmi_link_layer_stats *ll_stats;
	struct wmi_wifi_ll_peer_stats *peer_stats;
	struct wmi_wifi_chan_cca_stats *cca_stats;
	struct wmi_wifi_peer_signal_stats *peer_signal;
	uint8_t *result;
	uint32_t i, peer_num, result_size, dst_len;

	param_buf = (WMI_REPORT_STATS_EVENTID_param_tlvs *)evt_buf;
	fixed_param = param_buf->fixed_param;
	wmi_cca_stats = param_buf->chan_cca_stats;
	wmi_peer_signal = param_buf->peer_signal_stats;
	wmi_peer_rx = param_buf->peer_ac_rx_stats;
	period = param_buf->stats_period;
	intf_stats = param_buf->stats_interference;
	wmi_debug("TrigID=0x%x", fixed_param->trigger_cond_id);

	/* Get the MAX of three peer numbers */
	peer_num = fixed_param->num_peer_signal_stats >
			fixed_param->num_peer_ac_tx_stats ?
			fixed_param->num_peer_signal_stats :
			fixed_param->num_peer_ac_tx_stats;
	peer_num = peer_num > fixed_param->num_peer_ac_rx_stats ?
			peer_num : fixed_param->num_peer_ac_rx_stats;

	if (peer_num == 0) {
		wmi_err("Peer number is zero");
		return -EINVAL;
	}

	ll_stats = stats;
	ll_stats->trigger_cond_id = fixed_param->trigger_cond_id;
	ll_stats->cca_chgd_bitmap = fixed_param->cca_chgd_bitmap;
	ll_stats->sig_chgd_bitmap = fixed_param->sig_chgd_bitmap;
	ll_stats->tx_chgd_bitmap = fixed_param->tx_chgd_bitmap;
	ll_stats->rx_chgd_bitmap = fixed_param->rx_chgd_bitmap;
	ll_stats->channel_num = fixed_param->num_chan_cca_stats;
	ll_stats->peer_num = peer_num;

	if (intf_stats) {
		wmi_debug("sa_ant_matrix=%d sa_ant_matrix=%d timestamp=%d",
			  intf_stats->sa_ant_matrix, intf_stats->phyerr_count,
			  intf_stats->timestamp);
		ll_stats->maxtrix = intf_stats->sa_ant_matrix;
		ll_stats->phyerr_count = intf_stats->phyerr_count;
		ll_stats->timestamp = intf_stats->timestamp;
	}

	etract_ll_stats_time_stamp(period, &ll_stats->time_stamp);
	result = (uint8_t *)ll_stats->stats;
	result_size = ll_stats->buf_len;
	peer_stats = (struct wmi_wifi_ll_peer_stats *)result;
	ll_stats->peer_stats = peer_stats;

	for (i = 0; i < peer_num; i++) {
		peer_stats[i].peer_id = WIFI_INVALID_PEER_ID;
		peer_stats[i].vdev_id = WIFI_INVALID_VDEV_ID;
	}

	/* Per peer signal */
	dst_len = sizeof(struct wmi_wifi_peer_signal_stats);
	for (i = 0; i < fixed_param->num_peer_signal_stats; i++) {
		peer_stats[i].peer_id = wmi_peer_signal->peer_id;
		peer_stats[i].vdev_id = wmi_peer_signal->vdev_id;
		peer_signal = &peer_stats[i].peer_signal_stats;

		wmi_debug("%d antennas for peer %d",
			 wmi_peer_signal->num_chains_valid,
			 wmi_peer_signal->peer_id);

		peer_signal->vdev_id = wmi_peer_signal->vdev_id;
		peer_signal->peer_id = wmi_peer_signal->peer_id;
		peer_signal->num_chain =
				wmi_peer_signal->num_chains_valid;
		qdf_mem_copy(peer_signal->per_ant_snr,
			     wmi_peer_signal->per_chain_snr,
			     sizeof(peer_signal->per_ant_snr));
		qdf_mem_copy(peer_signal->nf,
			     wmi_peer_signal->per_chain_nf,
			     sizeof(peer_signal->nf));
		qdf_mem_copy(peer_signal->per_ant_rx_mpdus,
			     wmi_peer_signal->per_antenna_rx_mpdus,
			     sizeof(peer_signal->per_ant_rx_mpdus));
		qdf_mem_copy(peer_signal->per_ant_tx_mpdus,
			     wmi_peer_signal->per_antenna_tx_mpdus,
			     sizeof(peer_signal->per_ant_tx_mpdus));
		WMI_MAC_ADDR_TO_CHAR_ARRAY(&wmi_peer_signal->peer_macaddr,
					   peer_stats[i].mac_address);
		wmi_debug("Peer %d mac address is: %02x:%02x:%02x:%02x:%02x:%02x",
			  peer_stats[i].peer_id,
			  peer_stats[i].mac_address[0],
			  peer_stats[i].mac_address[1],
			  peer_stats[i].mac_address[2],
			  peer_stats[i].mac_address[3],
			  peer_stats[i].mac_address[4],
			  peer_stats[i].mac_address[5]);
		wmi_peer_signal++;
	}

	result += peer_num * sizeof(struct wmi_wifi_ll_peer_stats);
	result_size -= peer_num * sizeof(struct wmi_wifi_ll_peer_stats);

	cca_stats = (struct wmi_wifi_chan_cca_stats *)result;
	ll_stats->cca = cca_stats;
	dst_len = sizeof(struct wmi_wifi_chan_cca_stats);
	for (i = 0; i < ll_stats->channel_num; i++) {
		if (dst_len <= result_size) {
			qdf_mem_copy(&cca_stats[i], &wmi_cca_stats->vdev_id,
				     dst_len);
			result_size -= dst_len;

			if (result_size <= 0) {
				wmi_err("No more buffer for peer signal counters");
				return -EINVAL;
			}
		} else {
			wmi_err("Invalid length of CCA.");
			return -EINVAL;
		}
	}

	result += i * sizeof(struct wmi_wifi_chan_cca_stats);
	result += extract_ll_tx_stats(ll_stats, fixed_param, param_buf,
				      result, result_size);

	extract_ll_rx_stats(ll_stats, fixed_param, param_buf,
			    result, result_size);

	return 0;
}

void wmi_cp_stats_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->send_stats_request_cmd = send_stats_request_cmd_tlv;
#ifdef WLAN_FEATURE_BIG_DATA_STATS
	ops->send_big_data_stats_request_cmd =
				send_big_data_stats_request_cmd_tlv;
#endif
	ops->extract_all_stats_count = extract_all_stats_counts_tlv;
	ops->extract_pdev_stats = extract_pdev_stats_tlv;
	ops->extract_vdev_stats = extract_vdev_stats_tlv;
	ops->extract_peer_stats = extract_peer_stats_tlv;
	ops->extract_peer_extd_stats = extract_peer_extd_stats_tlv;
	wmi_infra_cp_stats_ops_attach_tlv(ops);
	ops->extract_pmf_bcn_protect_stats = extract_pmf_bcn_protect_stats_tlv,
	ops->send_stats_threshold = send_stats_threshold_tlv;
	ops->extract_ll_stats = extract_ll_stats_tlv;
	ops->get_stats_buf_length = get_stats_buf_length_tlv;

	wmi_mc_cp_stats_attach_tlv(wmi_handle);
}
