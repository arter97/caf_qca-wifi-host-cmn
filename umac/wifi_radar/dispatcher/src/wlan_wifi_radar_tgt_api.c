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

/*
 * Layer b/w umac and target_if (ol) txops
 * It contains wrapers for txops
 */

#include <wlan_wifi_radar_tgt_api.h>
#include <wlan_wifi_radar_utils_api.h>
#include <wifi_radar_defs_i.h>

static inline struct wlan_lmac_if_wifi_radar_tx_ops *
	wlan_psoc_get_wifi_radar_txops(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_tx_ops *tx_ops;

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		wifi_radar_err("tx ops is NULL");
		return NULL;
	}
	return &tx_ops->wifi_radar_tx_ops;
}

QDF_STATUS tgt_wifi_radar_init_pdev(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_lmac_if_wifi_radar_tx_ops *wifi_radar_tx_ops = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	wifi_radar_tx_ops = wlan_psoc_get_wifi_radar_txops(psoc);

	if (wifi_radar_tx_ops->wifi_radar_init_pdev)
		status = wifi_radar_tx_ops->wifi_radar_init_pdev(psoc, pdev);

	if (QDF_IS_STATUS_ERROR(status))
		wifi_radar_err("Error occurred with exit code %d\n", status);

	return status;
}

QDF_STATUS tgt_wifi_radar_deinit_pdev(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_lmac_if_wifi_radar_tx_ops *wifi_radar_tx_ops = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	wifi_radar_tx_ops = wlan_psoc_get_wifi_radar_txops(psoc);

	if (wifi_radar_tx_ops->wifi_radar_deinit_pdev)
		status = wifi_radar_tx_ops->wifi_radar_deinit_pdev(psoc, pdev);

	if (QDF_IS_STATUS_ERROR(status))
		wifi_radar_err("Error occurred with exit code %d\n", status);

	return status;
}

void tgt_wifi_radar_support_set(struct wlan_objmgr_psoc *psoc, uint32_t value)
{
	struct psoc_wifi_radar *wifi_radar_sc;

	if (!psoc)
		return;

	wifi_radar_sc = wlan_objmgr_psoc_get_comp_private_obj
				(psoc, WLAN_UMAC_COMP_WIFI_RADAR);
	if (!wifi_radar_sc)
		return;

	wifi_radar_sc->is_wifi_radar_capable = !!value;
	wifi_radar_info("wifi_radar: FW support advert=%d",
			wifi_radar_sc->is_wifi_radar_capable);
}

uint32_t tgt_wifi_radar_info_send(struct wlan_objmgr_pdev *pdev, void *head,
				  size_t hlen, void *data, size_t dlen,
				  void *tail, size_t tlen)
{
	struct pdev_wifi_radar *pa;
	uint32_t status;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						   WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pa) {
		wifi_radar_err("pdev_wifi_radar is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (head)
		status = wifi_radar_streamfs_write(pa, (const void *)head,
						   hlen);

	if (data)
		status = wifi_radar_streamfs_write(pa, (const void *)data,
						   dlen);

	if (tail)
		status = wifi_radar_streamfs_write(pa, (const void *)tail,
						   tlen);

	/* finalize the write */
	status = wifi_radar_streamfs_flush(pa);

	return status;
}
