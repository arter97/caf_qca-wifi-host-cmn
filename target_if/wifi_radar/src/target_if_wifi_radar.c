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

	pwr->is_wifi_radar_capable = 1;
	pwr->subbuf_size = STREAMFS_WIFI_RADAR_MAX_SUBBUF;
	pwr->num_subbufs = STREAMFS_WIFI_RADAR_NUM_SUBBUF;

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
