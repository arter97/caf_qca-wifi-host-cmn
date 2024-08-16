/*
 * Copyright (c) 2018, 2020-2021 The Linux Foundation. All rights reserved.
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
/**
 * DOC: Declare public API related to the wlan ipa called by north bound
 */

#ifndef _WLAN_IPA_OBJ_MGMT_H_
#define _WLAN_IPA_OBJ_MGMT_H_

#include "wlan_ipa_public_struct.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_ipa_main.h"

#ifdef IPA_OFFLOAD

/**
 * ipa_init() - IPA module initialization
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ipa_init(void);

/**
 * ipa_deinit() - IPA module deinitialization
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ipa_deinit(void);

/**
 * ipa_register_is_ipa_ready() - Register IPA ready callback
 * @pdev: pointer to pdev
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ipa_register_is_ipa_ready(struct wlan_objmgr_pdev *pdev);

/**
 * ipa_disable_register_cb() - Reset the IPA is ready flag
 *
 * Return: Set the ipa_is_ready flag to false when module is
 * unloaded to indicate that ipa ready cb is not registered
 */
void ipa_disable_register_cb(void);

/**
 * wlan_ipa_config_is_enabled() - api to get IPA enable status
 *
 * Return: true - ipa is enabled
 *         false - ipa is not enabled
 */
static inline bool wlan_ipa_config_is_enabled(void)
{
	return ipa_config_is_enabled();
}

#ifdef IPA_OPT_WIFI_DP_CTRL
/**
 * ipa_tx_pkt_opt_dp_ctrl() - Handle opt_dp_ctrl tx pkt
 * @vdev_id: vdev id
 * @nbuf: nbuf
 */
void ipa_tx_pkt_opt_dp_ctrl(uint8_t vdev_id,
			    qdf_nbuf_t nbuf);

/**
 * ipa_opt_dpath_enable_clk_req() - send clock enable request io ipa
 * @soc: soc
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ipa_opt_dpath_enable_clk_req(void *soc);

/**
 * ipa_opt_dpath_disable_clk_req() - send clock enable request io ipa
 * @soc: soc
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ipa_opt_dpath_disable_clk_req(void *soc);

/**
 * wlan_ipa_set_fw_cap_opt_dp_ctrl() - set fw capability of
 *              opt_dp_ctrl
 * @psoc: psoc object
 * @fw_cap: flag for fw capability in opt_dp_ctrl
 *
 */
QDF_STATUS wlan_ipa_set_fw_cap_opt_dp_ctrl(struct wlan_objmgr_psoc *psoc,
					   bool fw_cap);
#endif

/**
 * wlan_ipa_get_hdl() - Get ipa hdl set by IPA driver
 * @soc: void psoc object
 * @pdev_id: pdev id
 *
 * Return: IPA handle
 */
qdf_ipa_wdi_hdl_t wlan_ipa_get_hdl(void *soc, uint8_t pdev_id);

/**
 * wlan_ipa_is_vlan_enabled() - get IPA vlan support enable status
 *
 * Return: true - ipa vlan support is enabled
 *         false - ipa vlan support is not enabled
 */
bool wlan_ipa_is_vlan_enabled(void);

/**
 * wlan_ipa_config_is_opt_wifi_dp_enabled() - Is IPA optional wifi dp enabled?
 *
 * Return: true if IPA opt wifi dp is enabled in IPA config
 */
bool wlan_ipa_config_is_opt_wifi_dp_enabled(void);

/**
 * get_ipa_config() - API to get IPAConfig INI
 * @psoc : psoc handle
 *
 * Return: IPA config value
 */
uint32_t get_ipa_config(struct wlan_objmgr_psoc *psoc);
#else

static inline QDF_STATUS ipa_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ipa_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ipa_register_is_ipa_ready(
	struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline void ipa_disable_register_cb(void)
{
}

static inline bool wlan_ipa_config_is_opt_wifi_dp_enabled(void)
{
	return false;
}

static inline bool wlan_ipa_config_is_enabled(void)
{
	return false;
}

static inline bool wlan_ipa_is_vlan_enabled(void)
{
	return false;
}
#endif /* IPA_OFFLOAD */

#ifndef IPA_OPT_WIFI_DP_CTRL
static inline QDF_STATUS ipa_opt_dpath_disable_clk_req(void *soc)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS wlan_ipa_set_fw_cap_opt_dp_ctrl(void *soc, bool fw_cap)
{
	return QDF_STATUS_E_FAILURE;
}
#endif
#endif /* _WLAN_IPA_OBJ_MGMT_H_ */
