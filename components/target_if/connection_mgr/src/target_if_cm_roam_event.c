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

/**
 * DOC: This file contains definitions for target_if roaming events.
 */
#include "qdf_types.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wmi_unified_api.h"
#include "scheduler_api.h"
#include <wmi_unified.h>
#include "target_if_cm_roam_event.h"
#include "wlan_psoc_mlme_api.h"
#include "wlan_mlme_main.h"
#include <../../core/src/wlan_cm_roam_i.h>
#include "wlan_cm_roam_api.h"

static inline struct wlan_cm_roam_rx_ops *
target_if_cm_get_roam_rx_ops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *psoc_ext_priv;
	struct wlan_cm_roam_rx_ops *rx_ops;

	if (!psoc) {
		target_if_err("psoc object is NULL");
		return NULL;
	}
	psoc_ext_priv = wlan_psoc_mlme_get_ext_hdl(psoc);
	if (!psoc_ext_priv) {
		target_if_err("psoc legacy private object is NULL");
		return NULL;
	}

	rx_ops = &psoc_ext_priv->rso_rx_ops;
	return rx_ops;
}

void
target_if_cm_roam_register_rx_ops(struct wlan_cm_roam_rx_ops *rx_ops)
{
#ifdef ROAM_TARGET_IF_CONVERGENCE
	rx_ops->roam_sync_event = cm_roam_sync_event_handler;
	rx_ops->roam_sync_frame_event = cm_roam_sync_frame_event_handler;
	rx_ops->roam_event_rx = cm_roam_event_handler;
	rx_ops->btm_blacklist_event = cm_btm_blacklist_event_handler;
	rx_ops->vdev_disconnect_event = cm_vdev_disconnect_event_handler;
	rx_ops->roam_scan_chan_list_event = cm_roam_scan_ch_list_event_handler;
	rx_ops->roam_stats_event_rx = cm_roam_stats_event_handler;
	rx_ops->roam_auth_offload_event = cm_roam_auth_offload_event_handler;
	rx_ops->roam_pmkid_request_event_rx = cm_roam_pmkid_request_handler;
#endif
}

#ifdef ROAM_TARGET_IF_CONVERGENCE
int
target_if_cm_roam_sync_frame_event(ol_scn_t scn,
				   uint8_t *event,
				   uint32_t len)
{
	QDF_STATUS qdf_status;
	struct roam_synch_frame_ind *frame_ind_ptr;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
	int status = 0;

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is null");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return -EINVAL;
	}

	frame_ind_ptr = qdf_mem_malloc(sizeof(*frame_ind_ptr));

	if (!frame_ind_ptr)
		return -ENOMEM;

	qdf_status = wmi_extract_roam_sync_frame_event(wmi_handle, event,
						       len,
						       frame_ind_ptr);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		status = -EINVAL;
		goto err;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);

	if (!roam_rx_ops || !roam_rx_ops->roam_sync_frame_event) {
		target_if_err("No valid roam rx ops");
		status = -EINVAL;
		goto err;
	}

	qdf_status = roam_rx_ops->roam_sync_frame_event(psoc,
						    frame_ind_ptr);

	if (QDF_IS_STATUS_ERROR(qdf_status))
		status = -EINVAL;

err:
	qdf_mem_free(frame_ind_ptr);
	return status;
}

int target_if_cm_roam_sync_event(ol_scn_t scn, uint8_t *event,
				 uint32_t len)
{
	QDF_STATUS qdf_status;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
	struct roam_offload_synch_ind *sync_ind = NULL;
	int status = 0;

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is null");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return -EINVAL;
	}

	status = wmi_extract_roam_sync_event(wmi_handle, event,
					     len, &sync_ind);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("parsing of event failed, %d", status);
		status = -EINVAL;
		goto err;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);

	if (!roam_rx_ops || !roam_rx_ops->roam_sync_event) {
		target_if_err("No valid roam rx ops");
		status = -EINVAL;
		goto err;
	}

	qdf_status = roam_rx_ops->roam_sync_event(psoc,
						  event,
						  len,
						  sync_ind);

	if (QDF_IS_STATUS_ERROR(qdf_status))
		status = -EINVAL;

err:
	if (sync_ind && sync_ind->ric_tspec_data)
		qdf_mem_free(sync_ind->ric_tspec_data);
	if (sync_ind)
		qdf_mem_free(sync_ind);
	return status;
}

static QDF_STATUS target_if_roam_event_dispatcher(struct scheduler_msg *msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;

	switch (msg->type) {
	case ROAM_PMKID_REQ_EVENT:
	{
		struct roam_pmkid_req_event *data = msg->bodyptr;

		if (!data->psoc) {
			target_if_err("psoc is null");
			status = QDF_STATUS_E_NULL_VALUE;
			goto done;
		}
		roam_rx_ops = target_if_cm_get_roam_rx_ops(data->psoc);
		if (!roam_rx_ops || !roam_rx_ops->roam_pmkid_request_event_rx) {
			target_if_err("No valid roam rx ops");
			status = QDF_STATUS_E_INVAL;
			goto done;
		}

		status = roam_rx_ops->roam_pmkid_request_event_rx(data);
	}
	break;
	case ROAM_EVENT:
	{
		struct roam_offload_roam_event *data = msg->bodyptr;

		if (!data->psoc) {
			target_if_err("psoc is null");
			status = QDF_STATUS_E_NULL_VALUE;
			goto done;
		}
		roam_rx_ops = target_if_cm_get_roam_rx_ops(data->psoc);
		if (!roam_rx_ops || !roam_rx_ops->roam_event_rx) {
			target_if_err("No valid roam rx ops");
			status = QDF_STATUS_E_INVAL;
			goto done;
		}

		status = roam_rx_ops->roam_event_rx(data);
	}
	break;
	case ROAM_VDEV_DISCONNECT_EVENT:
	{
		struct vdev_disconnect_event_data *data = msg->bodyptr;

		if (!data->psoc) {
			target_if_err("psoc is null");
			status = QDF_STATUS_E_NULL_VALUE;
			goto done;
		}

		roam_rx_ops = target_if_cm_get_roam_rx_ops(data->psoc);
		if (!roam_rx_ops || !roam_rx_ops->vdev_disconnect_event) {
			target_if_err("No valid roam rx ops");
			status = -EINVAL;
			goto done;
		}
		status = roam_rx_ops->vdev_disconnect_event(data);
	}
	break;
	default:
		target_if_err("invalid msg type %d", msg->type);
		status = QDF_STATUS_E_INVAL;
	}

done:
	qdf_mem_free(msg->bodyptr);
	msg->bodyptr = NULL;
	return status;
}

static QDF_STATUS target_if_roam_event_flush_cb(struct scheduler_msg *msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	switch (msg->type) {
	case ROAM_PMKID_REQ_EVENT:
	case ROAM_EVENT:
	case ROAM_VDEV_DISCONNECT_EVENT:
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	default:
		target_if_err("invalid msg type %d", msg->type);
		status = QDF_STATUS_E_INVAL;
		goto free_res;
	}
	return status;

free_res:
	qdf_mem_free(msg->bodyptr);
	msg->bodyptr = NULL;
	return status;
}

int target_if_cm_roam_event(ol_scn_t scn, uint8_t *event, uint32_t len)
{
	QDF_STATUS qdf_status;
	struct wmi_unified *wmi_handle;
	struct roam_offload_roam_event *roam_event = NULL;
	struct wlan_objmgr_psoc *psoc;
	struct scheduler_msg msg = {0};

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is null");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return -EINVAL;
	}

	roam_event = qdf_mem_malloc(sizeof(*roam_event));
	if (!roam_event)
		return -ENOMEM;

	qdf_status = wmi_extract_roam_event(wmi_handle, event, len, roam_event);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		qdf_mem_free(roam_event);
		return -EINVAL;
	}

	msg.bodyptr = roam_event;
	msg.type = ROAM_EVENT;
	msg.callback = target_if_roam_event_dispatcher;
	msg.flush_callback = target_if_roam_event_flush_cb;
	target_if_debug("ROAM_EVENT sent: %d", msg.type);
	qdf_status = scheduler_post_message(QDF_MODULE_ID_TARGET_IF,
					    QDF_MODULE_ID_TARGET_IF,
					    QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		target_if_roam_event_flush_cb(&msg);

	return qdf_status_to_os_return(qdf_status);
}

static int
target_if_cm_btm_blacklist_event(ol_scn_t scn, uint8_t *event, uint32_t len)
{
	QDF_STATUS qdf_status;
	int status = 0;
	struct roam_blacklist_event *dst_list = NULL;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is null");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return -EINVAL;
	}

	qdf_status = wmi_extract_btm_blacklist_event(wmi_handle, event, len,
						     &dst_list);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		return -EINVAL;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->btm_blacklist_event) {
		target_if_err("No valid roam rx ops");
		status = -EINVAL;
		goto done;
	}
	qdf_status = roam_rx_ops->btm_blacklist_event(psoc, dst_list);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		status = -EINVAL;

done:
	qdf_mem_free(dst_list);
	return status;
}

int
target_if_cm_roam_vdev_disconnect_event_handler(ol_scn_t scn, uint8_t *event,
						uint32_t len)
{
	QDF_STATUS qdf_status;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct vdev_disconnect_event_data *data;
	struct scheduler_msg msg = {0};

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is null");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return -EINVAL;
	}

	data = qdf_mem_malloc(sizeof(*data));
	if (!data)
		return -ENOMEM;
	qdf_status = wmi_extract_vdev_disconnect_event(wmi_handle, event, len,
						       data);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		qdf_mem_free(data);
		return -EINVAL;
	}

	msg.bodyptr = data;
	msg.type = ROAM_VDEV_DISCONNECT_EVENT;
	msg.callback = target_if_roam_event_dispatcher;
	msg.flush_callback = target_if_roam_event_flush_cb;
	target_if_debug("ROAM_VDEV_DISCONNECT_EVENT sent: %d", msg.type);
	qdf_status = scheduler_post_message(QDF_MODULE_ID_TARGET_IF,
					    QDF_MODULE_ID_TARGET_IF,
					    QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		target_if_roam_event_flush_cb(&msg);

	return qdf_status_to_os_return(qdf_status);
}

int
target_if_cm_roam_scan_chan_list_event_handler(ol_scn_t scn, uint8_t *event,
					       uint32_t len)
{
	QDF_STATUS qdf_status;
	int status = 0;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
	struct cm_roam_scan_ch_resp *data = NULL;

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is null");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return -EINVAL;
	}

	qdf_status = wmi_extract_roam_scan_chan_list(wmi_handle, event, len,
						     &data);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		return -EINVAL;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->roam_scan_chan_list_event) {
		target_if_err("No valid roam rx ops");
		qdf_mem_free(data);
		return -EINVAL;
	}
	qdf_status = roam_rx_ops->roam_scan_chan_list_event(data);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		status = -EINVAL;

	return status;
}

int
target_if_cm_roam_stats_event(ol_scn_t scn, uint8_t *event, uint32_t len)
{
	QDF_STATUS qdf_status;
	int status = 0;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
	struct roam_stats_event *stats_info = NULL;

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is null");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return -EINVAL;
	}

	qdf_status = wmi_extract_roam_stats_event(wmi_handle, event, len,
						  &stats_info);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		return -EINVAL;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->roam_stats_event_rx) {
		target_if_err("No valid roam rx ops");
		status = -EINVAL;
		if (stats_info) {
			if (stats_info->roam_msg_info)
				qdf_mem_free(stats_info->roam_msg_info);
			qdf_mem_free(stats_info);
		}
		goto err;
	}

	qdf_status = roam_rx_ops->roam_stats_event_rx(psoc, stats_info);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		status = -EINVAL;

err:
	return status;
}

int
target_if_cm_roam_auth_offload_event(ol_scn_t scn, uint8_t *event, uint32_t len)
{
	QDF_STATUS qdf_status;
	int status = 0;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_cm_roam_rx_ops *roam_rx_ops;
	struct auth_offload_event auth_event = {0};

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is null");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return -EINVAL;
	}

	qdf_status = wmi_extract_auth_offload_event(wmi_handle, event, len,
						    &auth_event);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		return -EINVAL;
	}

	roam_rx_ops = target_if_cm_get_roam_rx_ops(psoc);
	if (!roam_rx_ops || !roam_rx_ops->roam_auth_offload_event) {
		target_if_err("No valid roam rx ops");
		return -EINVAL;
	}
	qdf_status = roam_rx_ops->roam_auth_offload_event(&auth_event);
	if (QDF_IS_STATUS_ERROR(status))
		status = -EINVAL;

	return status;
}

int
target_if_pmkid_request_event_handler(ol_scn_t scn, uint8_t *event,
				      uint32_t len)
{
	QDF_STATUS qdf_status;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct roam_pmkid_req_event *data = NULL;
	struct scheduler_msg msg = {0};

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("psoc is null");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return -EINVAL;
	}

	qdf_status = wmi_extract_roam_pmkid_request(wmi_handle, event, len,
						    &data);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		target_if_err("parsing of event failed, %d", qdf_status);
		qdf_mem_free(data);
		return -EINVAL;
	}

	msg.bodyptr = data;
	msg.type = ROAM_PMKID_REQ_EVENT;
	msg.callback = target_if_roam_event_dispatcher;
	msg.flush_callback = target_if_roam_event_flush_cb;
	target_if_debug("ROAM_PMKID_REQ_EVENT sent: %d", msg.type);
	qdf_status = scheduler_post_message(QDF_MODULE_ID_TARGET_IF,
					    QDF_MODULE_ID_TARGET_IF,
					    QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		target_if_roam_event_flush_cb(&msg);

	return qdf_status_to_os_return(qdf_status);
}

QDF_STATUS
target_if_roam_offload_register_events(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS ret;
	wmi_unified_t handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!handle) {
		target_if_err("handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/* Register for roam offload event */
	ret = wmi_unified_register_event_handler(handle,
						 wmi_roam_synch_event_id,
						 target_if_cm_roam_sync_event,
						 WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event registration failed, ret: %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	/* Register for roam offload event */
	ret = wmi_unified_register_event_handler(handle,
						 wmi_roam_synch_frame_event_id,
						 target_if_cm_roam_sync_frame_event,
						 WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event registration failed, ret: %d", ret);
		return QDF_STATUS_E_FAILURE;
	}
	ret = wmi_unified_register_event_handler(handle, wmi_roam_event_id,
						 target_if_cm_roam_event,
						 WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event registration failed, ret: %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_register_event_handler(handle,
					wmi_roam_blacklist_event_id,
					target_if_cm_btm_blacklist_event,
					WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event(%u) registration failed, ret: %d",
			      wmi_roam_blacklist_event_id, ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_register_event_handler(handle,
				wmi_vdev_disconnect_event_id,
				target_if_cm_roam_vdev_disconnect_event_handler,
				WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event(%u) registration failed, ret: %d",
			      wmi_vdev_disconnect_event_id, ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_register_event_handler(handle,
				wmi_roam_scan_chan_list_id,
				target_if_cm_roam_scan_chan_list_event_handler,
				WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event(%u) registration failed, ret: %d",
			      wmi_roam_scan_chan_list_id, ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_register_event_handler(handle,
						 wmi_roam_stats_event_id,
						 target_if_cm_roam_stats_event,
						 WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event registration failed, ret: %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_register_event_handler(handle,
				wmi_roam_auth_offload_event_id,
				target_if_cm_roam_auth_offload_event,
				WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event(%u) registration failed, ret: %d",
			      wmi_roam_auth_offload_event_id, ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = wmi_unified_register_event_handler(handle,
				wmi_roam_pmkid_request_event_id,
				target_if_pmkid_request_event_handler,
				WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(ret)) {
		target_if_err("wmi event(%u) registration failed, ret: %d",
			      wmi_roam_stats_event_id, ret);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}
#endif
