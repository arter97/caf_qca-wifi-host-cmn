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

/**
 * DOC: declare internal API related to the mlme TWT functionality
 */

#ifndef _WLAN_MLME_TWT_API_H_
#define _WLAN_MLME_TWT_API_H_

#include <wlan_mlme_public_struct.h>
#include <wlan_mlme_twt_public_struct.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_cmn.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include "wlan_cm_roam_public_struct.h"

#ifdef WLAN_SUPPORT_TWT
/**
 * mlme_is_twt_setup_in_progress() - Get if TWT setup command is in progress
 * for given dialog id
 * @psoc: Pointer to global psoc object
 * @peer_mac: Global peer mac address
 * @dialog_id: Dialog ID
 *
 * Return: True if Setup is in progress
 */
bool mlme_is_twt_setup_in_progress(struct wlan_objmgr_psoc *psoc,
				   struct qdf_mac_addr *peer_mac,
				   uint8_t dialog_id);

/**
 * mlme_add_twt_session()  - Add TWT session entry in the TWT context
 * @psoc: Pointer to global psoc object
 * @peer_mac: Global peer mac address
 * @dialog_id: Dialog ID
 *
 * Return: None
 */
void mlme_add_twt_session(struct wlan_objmgr_psoc *psoc,
			  struct qdf_mac_addr *peer_mac,
			  uint8_t dialog_id);

/**
 * mlme_set_twt_setup_done()  - Set TWT setup complete for given dialog ID
 * @psoc: Pointer to psoc object
 * @peer_mac: Pointer to peer mac address
 * @dialog_id: Dialog id
 * @is_set: Set or clear the setup done flag
 *
 * Return: None
 */
void mlme_set_twt_setup_done(struct wlan_objmgr_psoc *psoc,
			     struct qdf_mac_addr *peer_mac,
			     uint8_t dialog_id, bool is_set);

/**
 * mlme_is_twt_setup_done()  - Get if TWT session is established for given
 * dialog id.
 * @psoc: Pointer to psoc object
 * @peer_mac: Pointer to peer mac address
 * @dialog_id: Dialog id
 *
 * Return: Return true if TWT session exists for given dialog ID.
 */
bool mlme_is_twt_setup_done(struct wlan_objmgr_psoc *psoc,
			    struct qdf_mac_addr *peer_mac, uint8_t dialog_id);

/**
 * mlme_set_twt_session_state() - Set the TWT session state for the given dialog
 * id in TWT context
 * @peer: Pointer to peer object
 * @peer_mac: Pointer to peer mac address
 * @dialog_id: Dialog id
 * @state: TWT session state
 *
 * Return: None
 */
void mlme_set_twt_session_state(struct wlan_objmgr_psoc *psoc,
				struct qdf_mac_addr *peer_mac,
				uint8_t dialog_id,
				enum wlan_twt_session_state state);

/**
 * mlme_get_twt_session_state()  - Get TWT session state for given dialog id
 * @psoc: Pointer to psoc object
 * @peer_mac: Pointer to peer mac address
 * @dialog_id: Dialog id
 *
 * Return:  TWT session state.
 */
enum wlan_twt_session_state
mlme_get_twt_session_state(struct wlan_objmgr_psoc *psoc,
			   struct qdf_mac_addr *peer_mac, uint8_t dialog_id);

/**
 * mlme_get_twt_peer_capabilities  - Get TWT peer capabilities
 * @psoc: Pointer to psoc object
 * @peer_mac: Pointer to peer mac address
 *
 * Return: Peer TWT capabilities
 */
uint8_t mlme_get_twt_peer_capabilities(struct wlan_objmgr_psoc *psoc,
				       struct qdf_mac_addr *peer_mac);

/**
 * mlme_set_twt_peer_capabilities() - Set the TWT peer capabilities in TWT
 * context
 * @psoc: Pointer to psoc object
 * @peer_mac: Pointer to peer mac address
 * @he_cap: Pointer to HE capabilities IE
 * @he_op: Pointer to HE operations IE
 */
void mlme_set_twt_peer_capabilities(struct wlan_objmgr_psoc *psoc,
				    struct qdf_mac_addr *peer_mac,
				    tDot11fIEhe_cap *he_cap,
				    tDot11fIEhe_op *he_op);

/**
 * mlme_init_twt_context() - Initialize TWT context structure
 * @psoc: Pointer to psoc object
 * @peer_mac: Pointer to peer mac address
 * @dialog_id: Dialog id
 *
 * This API will be called on disconnection from AP.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_init_twt_context(struct wlan_objmgr_psoc *psoc,
				 struct qdf_mac_addr *peer_mac,
				 uint8_t dialog_id);

/**
 * mlme_twt_is_notify_done()  - Check if notify is done.
 * @psoc: Pointer to psoc object
 * @peer_mac: Pointer to peer mac address
 *
 * Return: True if notify is done
 */
bool mlme_twt_is_notify_done(struct wlan_objmgr_psoc *psoc,
			     struct qdf_mac_addr *peer_mac);

/**
 * mlme_twt_set_wait_for_notify()  - Set wait for notify flag.
 * @psoc: Pointer to psoc object
 * @peer_mac: Pointer to peer mac address
 * @is_set: Set or clear notify flag
 *
 * Return: None
 */
void mlme_twt_set_wait_for_notify(struct wlan_objmgr_psoc *psoc,
				  struct qdf_mac_addr *peer_mac,
				  bool is_set);

/**
 * mlme_is_flexible_twt_enabled() - Check if flexible TWT is enabled.
 * @psoc: Pointer to psoc object
 *
 * Return: True if flexible TWT is supported
 */
bool mlme_is_flexible_twt_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * mlme_set_twt_command_in_progress() - Set TWT command is in progress.
 * @psoc: Pointer to psoc object
 * @peer_mac: Pointer to peer mac address
 * @dialog_id: Dialog id
 * @cmd: TWT command
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_set_twt_command_in_progress(struct wlan_objmgr_psoc *psoc,
					    struct qdf_mac_addr *peer_mac,
					    uint8_t dialog_id,
					    enum wlan_twt_commands cmd);

/**
 * mlme_twt_is_command_in_progress() - Get TWT command in progress.
 * @psoc: Pointer to psoc object
 * @peer_mac: Pointer to peer mac address
 * @dialog_id: Dialog id
 * @cmd: TWT command
 *
 * Return: True if given command is in progress.
 */
bool mlme_twt_is_command_in_progress(struct wlan_objmgr_psoc *psoc,
				     struct qdf_mac_addr *peer_mac,
				     uint8_t dialog_id,
				     enum wlan_twt_commands cmd);
#else
static inline
void mlme_set_twt_peer_capabilities(struct wlan_objmgr_psoc *psoc,
				    struct qdf_mac_addr *peer_mac,
				    tDot11fIEhe_cap *he_cap,
				    tDot11fIEhe_op *he_op)
{}
#endif /* WLAN_SUPPORT_TWT */
#endif /* _WLAN_MLME_TWT_API_H_ */
