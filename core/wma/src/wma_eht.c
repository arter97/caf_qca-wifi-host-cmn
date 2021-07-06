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
 * DOC: wma_eht.c
 *
 * WLAN Host Device Driver 802.11be - Extremely High Throughput Implementation
 */

#include "wma_eht.h"
#include "wmi_unified.h"
#include "service_ready_param.h"
#include "target_if.h"

/**
 * wma_convert_eht_cap() - convert EHT capabilities into dot11f structure
 * @eht_cap: pointer to dot11f structure
 * @mac_cap: Received EHT MAC capability
 * @phy_cap: Received EHT PHY capability
 *
 * This function converts various EHT capability received as part of extended
 * service ready event into dot11f structure.
 *
 * Return: None
 */
static void wma_convert_eht_cap(tDot11fIEeht_cap *eht_cap, uint32_t *mac_cap,
				uint32_t *phy_cap)
{
	eht_cap->present = true;
	qdf_mem_copy(eht_cap->eht_mac_cap, mac_cap,
		     sizeof(eht_cap->eht_mac_cap));
	qdf_mem_copy(eht_cap->phy_cap_bytes, phy_cap,
		     sizeof(eht_cap->phy_cap_bytes));
}

void wma_eht_update_tgt_services(struct wmi_unified *wmi_handle,
				 struct wma_tgt_services *cfg)
{
	if (wmi_service_enabled(wmi_handle, wmi_service_11be)) {
		cfg->en_11be = true;
		wma_set_fw_wlan_feat_caps(DOT11BE);
		wma_debug("11be is enabled");
	} else {
		wma_debug("11be is not enabled");
	}
}

void wma_update_target_ext_eht_cap(struct target_psoc_info *tgt_hdl,
				   struct wma_tgt_cfg *tgt_cfg)
{
	tDot11fIEeht_cap *eht_cap = &tgt_cfg->eht_cap;
	tDot11fIEeht_cap *eht_cap_2g = &tgt_cfg->eht_cap_2g;
	tDot11fIEeht_cap *eht_cap_5g = &tgt_cfg->eht_cap_5g;
	int i, num_hw_modes, total_mac_phy_cnt;
	tDot11fIEeht_cap eht_cap_mac;
	struct wlan_psoc_host_mac_phy_caps_ext2 *mac_cap, *mac_phy_cap;
	struct wlan_psoc_host_mac_phy_caps *host_cap;
	uint32_t supported_bands;

	qdf_mem_zero(eht_cap_2g, sizeof(tDot11fIEeht_cap));
	qdf_mem_zero(eht_cap_5g, sizeof(tDot11fIEeht_cap));
	num_hw_modes = target_psoc_get_num_hw_modes(tgt_hdl);
	mac_phy_cap = target_psoc_get_mac_phy_cap_ext2(tgt_hdl);
	host_cap = target_psoc_get_mac_phy_cap(tgt_hdl);
	total_mac_phy_cnt = target_psoc_get_total_mac_phy_cnt(tgt_hdl);
	if (!mac_phy_cap || !host_cap) {
		wma_err("Invalid MAC PHY capabilities handle");
		eht_cap->present = false;
		return;
	}

	if (!num_hw_modes) {
		wma_err("No extended EHT cap for current SOC");
		eht_cap->present = false;
		return;
	}

	if (!tgt_cfg->services.en_11be) {
		wma_info("Target does not support 11BE");
		eht_cap->present = false;
		return;
	}

	supported_bands = host_cap->supported_bands;
	for (i = 0; i < total_mac_phy_cnt; i++) {
		qdf_mem_zero(&eht_cap_mac, sizeof(tDot11fIEeht_cap));
		mac_cap = &mac_phy_cap[i];
		if (supported_bands & WLAN_2G_CAPABILITY) {
			wma_convert_eht_cap(&eht_cap_mac,
					    mac_cap->eht_cap_info_2G,
					    mac_cap->eht_cap_phy_info_2G);
			wma_convert_eht_cap(eht_cap_2g,
					    mac_cap->eht_cap_info_2G,
					    mac_cap->eht_cap_phy_info_2G);
		}

		if (supported_bands & WLAN_5G_CAPABILITY) {
			qdf_mem_zero(&eht_cap_mac, sizeof(tDot11fIEeht_cap));
			wma_convert_eht_cap(&eht_cap_mac,
					    mac_cap->eht_cap_info_5G,
					    mac_cap->eht_cap_phy_info_5G);
			wma_convert_eht_cap(eht_cap_5g,
					    mac_cap->eht_cap_info_5G,
					    mac_cap->eht_cap_phy_info_5G);
		}
	}
	qdf_mem_copy(eht_cap, &eht_cap_mac, sizeof(tDot11fIEeht_cap));
	wma_print_eht_cap(eht_cap);
}

void wma_update_vdev_eht_ops(uint32_t *eht_ops, tDot11fIEeht_op *eht_op)
{
}

void wma_print_eht_cap(tDot11fIEeht_cap *eht_cap)
{
	if (!eht_cap->present)
		return;

	wma_debug("EHT Capabilities:");
}

void wma_print_eht_phy_cap(uint32_t *phy_cap)
{
	wma_debug("EHT PHY Capabilities:");
}

void wma_print_eht_mac_cap_w1(uint32_t mac_cap)
{
	wma_debug("EHT MAC Capabilities:");
}

void wma_print_eht_mac_cap_w2(uint32_t mac_cap)
{
}

void wma_print_eht_op(tDot11fIEeht_op *eht_ops)
{
}

void wma_populate_peer_eht_cap(struct peer_assoc_params *peer,
			       tpAddStaParams params)
{
	tDot11fIEeht_cap *eht_cap = &params->eht_config;
	uint32_t *phy_cap = peer->peer_eht_cap_phyinfo;
	uint32_t mac_cap[PSOC_HOST_MAX_MAC_SIZE] = {0};

	if (params->eht_capable)
		peer->eht_flag = 1;
	else
		return;

	qdf_mem_copy(peer->peer_eht_cap_macinfo, peer->peer_he_cap_macinfo,
		     sizeof(mac_cap));
	qdf_mem_copy(peer->peer_he_cap_phyinfo, peer->peer_he_cap_phyinfo,
		     sizeof(peer->peer_he_cap_phyinfo));

	qdf_mem_copy(peer->peer_eht_rx_mcs_set, peer->peer_he_rx_mcs_set,
		     sizeof(peer->peer_he_rx_mcs_set));
	qdf_mem_copy(peer->peer_eht_tx_mcs_set, peer->peer_he_tx_mcs_set,
		     sizeof(peer->peer_he_tx_mcs_set));

	peer->peer_eht_mcs_count = peer->peer_he_mcs_count;

	wma_print_eht_cap(eht_cap);
	wma_debug("Peer EHT Capabilities:");
	wma_print_eht_phy_cap(phy_cap);
	wma_print_eht_mac_cap_w1(mac_cap[0]);
	wma_print_eht_mac_cap_w2(mac_cap[1]);
}

void wma_vdev_set_eht_bss_params(tp_wma_handle wma, uint8_t vdev_id,
				 struct vdev_mlme_eht_ops_info *eht_info)
{
	if (!eht_info->eht_ops)
		return;
}

QDF_STATUS wma_get_eht_capabilities(struct eht_capability *eht_cap)
{
	tp_wma_handle wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle)
		return QDF_STATUS_E_FAILURE;

	qdf_mem_copy(eht_cap->phy_cap,
		     &wma_handle->eht_cap.phy_cap,
		     WMI_MAX_EHTCAP_PHY_SIZE);
	eht_cap->mac_cap = wma_handle->eht_cap.mac_cap;
	return QDF_STATUS_SUCCESS;
}

void wma_set_peer_assoc_params_bw_320(struct peer_assoc_params *params,
				      enum phy_ch_width ch_width)
{
	if (ch_width == CH_WIDTH_320MHZ)
		params->bw_320 = 1;
}
