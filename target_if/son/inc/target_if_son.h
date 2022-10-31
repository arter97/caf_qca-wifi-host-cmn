/*
 * Copyright (c) 2017,2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
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
#include <ol_if_athvar.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>

void target_if_son_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops);

u_int32_t son_ol_get_peer_rate(struct wlan_objmgr_peer *peer, u_int8_t type);

QDF_STATUS son_ol_send_null(struct wlan_objmgr_pdev *pdev,
			    u_int8_t *macaddr,
			    struct wlan_objmgr_vdev *vdev);

#if defined(WMI_NON_TLV_SUPPORT) || defined(WMI_TLV_AND_NON_TLV_SUPPORT)
QDF_STATUS son_ol_peer_ext_stats_enable(struct wlan_objmgr_pdev *pdev,
					uint8_t *peer_addr,
					struct wlan_objmgr_vdev *vdev,
					uint32_t stats_count, uint32_t enable);
#else
static inline QDF_STATUS son_ol_peer_ext_stats_enable(
					struct wlan_objmgr_pdev *pdev,
					uint8_t *peer_addr,
					struct wlan_objmgr_vdev *vdev,
					uint32_t stats_count, uint32_t enable)
{
	return QDF_STATUS_SUCCESS;
}
#endif
