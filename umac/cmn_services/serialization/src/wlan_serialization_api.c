/*
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
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
/**
 * DOC: wlan_serialization_api.c
 * This file provides an interface for the external components
 * to utilize the services provided by the serialization
 * component.
 */

/* Include files */
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_serialization_main_i.h"
#include "wlan_serialization_utils_i.h"

bool wlan_serialization_is_cmd_present_in_pending_queue(
		struct wlan_objmgr_psoc *psoc,
		struct wlan_serialization_command *cmd)
{
	if (!cmd) {
		serialization_err("invalid cmd");
		return false;
	}
	return wlan_serialization_is_cmd_present_queue(cmd, false);
}

bool wlan_serialization_is_cmd_present_in_active_queue(
		struct wlan_objmgr_psoc *psoc,
		struct wlan_serialization_command *cmd)
{
	if (!cmd) {
		serialization_err("invalid cmd");
		return false;
	}
	return wlan_serialization_is_cmd_present_queue(cmd, true);
}

QDF_STATUS
wlan_serialization_register_apply_rules_cb(struct wlan_objmgr_psoc *psoc,
		enum wlan_serialization_cmd_type cmd_type,
		wlan_serialization_apply_rules_cb cb)
{
	struct wlan_serialization_psoc_priv_obj *ser_soc_obj;
	QDF_STATUS status;

	status = wlan_serialization_validate_cmdtype(cmd_type);
	if (status != QDF_STATUS_SUCCESS) {
		serialization_err("invalid cmd_type %d",
				cmd_type);
		return status;
	}
	ser_soc_obj = wlan_serialization_get_psoc_priv_obj(psoc);
	if (!ser_soc_obj) {
		serialization_err("invalid ser_soc_obj");
		return QDF_STATUS_E_FAILURE;
	}
	ser_soc_obj->apply_rules_cb[cmd_type] = cb;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_serialization_deregister_apply_rules_cb(struct wlan_objmgr_psoc *psoc,
		enum wlan_serialization_cmd_type cmd_type)
{
	struct wlan_serialization_psoc_priv_obj *ser_soc_obj;
	QDF_STATUS status;

	status = wlan_serialization_validate_cmdtype(cmd_type);
	if (status != QDF_STATUS_SUCCESS) {
		serialization_err("invalid cmd_type %d",
				cmd_type);
		return status;
	}
	ser_soc_obj = wlan_serialization_get_psoc_priv_obj(psoc);
	if (!ser_soc_obj) {
		serialization_err("invalid ser_soc_obj");
		return QDF_STATUS_E_FAILURE;
	}
	ser_soc_obj->apply_rules_cb[cmd_type] = NULL;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_serialization_register_comp_info_cb(struct wlan_objmgr_psoc *psoc,
		enum wlan_umac_comp_id comp_id,
		enum wlan_serialization_cmd_type cmd_type,
		wlan_serialization_comp_info_cb cb)
{
	struct wlan_serialization_psoc_priv_obj *ser_soc_obj;
	QDF_STATUS status;

	status = wlan_serialization_validate_cmd(comp_id, cmd_type);
	if (status != QDF_STATUS_SUCCESS) {
		serialization_err("invalid comp_id %d or cmd_type %d",
				comp_id, cmd_type);
		return status;
	}
	ser_soc_obj = wlan_serialization_get_psoc_priv_obj(psoc);
	if (!ser_soc_obj) {
		serialization_err("invalid ser_soc_obj");
		return QDF_STATUS_E_FAILURE;
	}
	ser_soc_obj->comp_info_cb[cmd_type][comp_id] = cb;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_serialization_deregister_comp_info_cb(struct wlan_objmgr_psoc *psoc,
		enum wlan_umac_comp_id comp_id,
		enum wlan_serialization_cmd_type cmd_type)
{
	struct wlan_serialization_psoc_priv_obj *ser_soc_obj;
	QDF_STATUS status;

	status = wlan_serialization_validate_cmd(comp_id, cmd_type);
	if (status != QDF_STATUS_SUCCESS) {
		serialization_err("invalid comp_id %d or cmd_type %d",
				comp_id, cmd_type);
		return status;
	}
	ser_soc_obj = wlan_serialization_get_psoc_priv_obj(psoc);
	if (!ser_soc_obj) {
		serialization_err("invalid ser_soc_obj");
		return QDF_STATUS_E_FAILURE;
	}
	ser_soc_obj->comp_info_cb[cmd_type][comp_id] = NULL;

	return QDF_STATUS_SUCCESS;
}

enum wlan_serialization_cmd_status
wlan_serialization_non_scan_cmd_status(struct wlan_objmgr_pdev *pdev,
		enum wlan_serialization_cmd_type cmd_id)
{
	serialization_enter();

	return WLAN_SER_CMD_NOT_FOUND;
}

enum wlan_serialization_cmd_status
wlan_serialization_cancel_request(
		struct wlan_serialization_queued_cmd_info *req)
{
	QDF_STATUS status;

	serialization_enter();
	if (!req) {
		serialization_err("given request is empty");
		return WLAN_SER_CMD_NOT_FOUND;
	}
	status = wlan_serialization_validate_cmd(req->requestor, req->cmd_type);
	if (status != QDF_STATUS_SUCCESS) {
		serialization_err("req is not valid");
		return WLAN_SER_CMD_NOT_FOUND;
	}

	return wlan_serialization_find_and_cancel_cmd(req);
}

void wlan_serialization_remove_cmd(
		struct wlan_serialization_queued_cmd_info *cmd)
{
	QDF_STATUS status;

	serialization_enter();
	if (!cmd) {
		serialization_err("given request is empty");
		QDF_ASSERT(0);
		return;
	}
	status = wlan_serialization_validate_cmd(cmd->requestor, cmd->cmd_type);
	if (status != QDF_STATUS_SUCCESS) {
		serialization_err("cmd is not valid");
		QDF_ASSERT(0);
		return;
	}
	wlan_serialization_find_and_remove_cmd(cmd);

	return;
}

enum wlan_serialization_status
wlan_serialization_request(struct wlan_serialization_command *cmd)
{
	bool is_active_cmd_allowed;
	QDF_STATUS status;
	uint8_t comp_id;
	struct wlan_serialization_psoc_priv_obj *ser_soc_obj;
	union wlan_serialization_rules_info info;

	serialization_enter();
	if (!cmd) {
		serialization_err("serialization cmd is null");
		return WLAN_SER_CMD_DENIED_UNSPECIFIED;
	}
	status = wlan_serialization_validate_cmd(cmd->source, cmd->cmd_type);
	if (status != QDF_STATUS_SUCCESS) {
		serialization_err("cmd is not valid");
		return WLAN_SER_CMD_DENIED_UNSPECIFIED;
	}

	ser_soc_obj = wlan_serialization_get_obj(cmd);

	/*
	 * Get Component Info callback by calling
	 * each registered module
	 */
	for (comp_id = 0; comp_id < WLAN_UMAC_COMP_ID_MAX; comp_id++) {
		if (!ser_soc_obj->comp_info_cb[cmd->cmd_type][comp_id])
			continue;
		(ser_soc_obj->comp_info_cb[cmd->cmd_type][comp_id])(cmd->vdev,
			&info);
		if (!ser_soc_obj->apply_rules_cb[cmd->cmd_type])
			continue;
		if (!ser_soc_obj->apply_rules_cb[cmd->cmd_type](&info, comp_id))
			return WLAN_SER_CMD_DENIED_RULES_FAILED;
	}

	is_active_cmd_allowed = wlan_serialization_is_active_cmd_allowed(cmd);
	return wlan_serialization_enqueue_cmd(cmd, is_active_cmd_allowed);
}

enum wlan_serialization_cmd_status
wlan_serialization_vdev_scan_status(struct wlan_objmgr_vdev *vdev)
{
	bool cmd_in_active, cmd_in_pending;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct wlan_serialization_pdev_priv_obj *ser_pdev_obj =
		wlan_serialization_get_pdev_priv_obj(pdev);

	cmd_in_active =
	wlan_serialization_is_cmd_in_vdev_list(
			vdev, &ser_pdev_obj->active_scan_list);

	cmd_in_pending =
	wlan_serialization_is_cmd_in_vdev_list(
			vdev, &ser_pdev_obj->pending_scan_list);

	return wlan_serialization_is_cmd_in_active_pending(
			cmd_in_active, cmd_in_pending);
}

void wlan_serialization_flush_cmd(
		struct wlan_serialization_queued_cmd_info *cmd)
{
	serialization_enter();
	if (!cmd) {
		serialization_err("cmd is null, can't flush");
		return;
	}
	/* TODO: discuss and fill this API later */

	return;
}

enum wlan_serialization_cmd_status
wlan_serialization_pdev_scan_status(struct wlan_objmgr_pdev *pdev)
{
	bool cmd_in_active, cmd_in_pending;
	struct wlan_serialization_pdev_priv_obj *ser_pdev_obj =
		wlan_serialization_get_pdev_priv_obj(pdev);

	cmd_in_active =
	wlan_serialization_is_cmd_in_pdev_list(
			pdev, &ser_pdev_obj->active_scan_list);

	cmd_in_pending =
	wlan_serialization_is_cmd_in_pdev_list(
			pdev, &ser_pdev_obj->pending_scan_list);

	return wlan_serialization_is_cmd_in_active_pending(
			cmd_in_active, cmd_in_pending);
}
