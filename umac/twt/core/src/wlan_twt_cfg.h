/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <wlan_objmgr_psoc_obj.h>

#if defined(WLAN_SUPPORT_TWT) && defined(WLAN_TWT_CONV_SUPPORTED)
/**
 * wlan_twt_cfg_init() - Initialize twt config params
 * @psoc: Pointer to global psoc
 *
 * This function initializes the twt private cfg params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_twt_cfg_init(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_twt_get_bcast() - Get broadcast enable value
 * @psoc: pointer to psoc
 * @val: pointer reference to retrieved value

 * Return: QDF_STATUS - Success or Failure
 */
QDF_STATUS wlan_twt_cfg_get_bcast(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_twt_get_requestor() - Get requestor value
 * @psoc: pointer to psoc
 * @val: pointer reference to retrieved value

 * Return: QDF_STATUS - Success or Failure
 */
QDF_STATUS
wlan_twt_cfg_get_requestor(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_twt_get_responder() - Get responder value
 * @psoc: pointer to psoc
 * @val: pointer reference to retrieved value

 * Return: QDF_STATUS - Success or Failure
 */
QDF_STATUS
wlan_twt_cfg_get_responder(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_twt_cfg_get_congestion_timeout() - get congestion timeout
 * @psoc: Pointer to global psoc
 * @val: pointer to output variable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_twt_cfg_get_congestion_timeout(struct wlan_objmgr_psoc *psoc,
				    uint32_t *val);
/**
 * wlan_twt_cfg_get_bcast_requestor() - get bcast requestor
 * @psoc: Pointer to global psoc
 * @val: pointer to output variable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_twt_cfg_get_bcast_requestor(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_twt_cfg_get_bcast_responder() - get bcast responder
 * @psoc: Pointer to global psoc
 * @val: pointer to output variable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_twt_cfg_get_bcast_responder(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_twt_cfg_get_rtwt_requestor() - get rTWT requestor
 * @psoc: Pointer to global psoc
 * @val: pointer to output variable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_twt_cfg_get_rtwt_requestor(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_twt_cfg_set_requestor_flag() - set requestor flag
 * @psoc: Pointer to global psoc
 * @val: value to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_twt_cfg_set_requestor_flag(struct wlan_objmgr_psoc *psoc, bool val);

/**
 * wlan_twt_cfg_set_responder_flag() - set responder flag
 * @psoc: Pointer to global psoc
 * @val: value to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_twt_cfg_set_responder_flag(struct wlan_objmgr_psoc *psoc, bool val);

#endif

