/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "qca_wifi_mlo_metadata_info_if.h"
#include "qca_wifi_mlo_metadata_info.h"
#include <ol_if_athvar.h>

static inline
void qca_mlo_metadata_set_tag(uint32_t *hash)
{
	*hash |= (MLO_METADATA_VALID_TAG << MLO_METADATA_TAG_SHIFT);
}

static inline
void qca_mlo_metadata_set_primary_link_id(uint32_t *key, uint8_t link_id)
{
	link_id &= MLO_METADATA_LINK_ID_MASK;
	*key |= (link_id << MLO_METADATA_LINK_ID_SHIFT);
}

static inline
uint8_t qca_mlo_get_ds_node_id(osif_dev *osdev)
{
	wlan_if_t vap = NULL;
#ifdef QCA_SUPPORT_WDS_EXTENDED
	osif_peer_dev *osifp;
	osif_dev *osif_parent = NULL;
#endif
	switch (osdev->dev_type) {
#ifdef QCA_SUPPORT_WDS_EXTENDED
	case OSIF_NETDEV_TYPE_WDS_EXT:
		osifp = (osif_peer_dev *)(osdev);
		osif_parent = osif_wds_ext_get_parent_osif(osifp);
		if (!osif_parent)
			return MLO_METADATA_INVALID_DS_NODE_ID;

		vap = osif_parent->os_if;
		break;
#endif
	case OSIF_NETDEV_TYPE_VAP:
		vap = osdev->os_if;
		break;
	}

	if (!vap)
		return MLO_METADATA_INVALID_DS_NODE_ID;

	if (vap->cp.icp_flags & IEEE80211_PPE_VP_DS_ACCEL)
		return osdev->mlo_link_id;

	return MLO_METADATA_INVALID_DS_NODE_ID;
}

#ifdef WLAN_FEATURE_11BE_MLO
uint32_t qca_mlo_get_mark_metadata(struct qca_mlo_metadata_param *mlo_param)
{
	struct net_device *dest_dev = mlo_param->in_dest_dev;
	uint8_t *dest_mac = mlo_param->in_dest_mac;
	struct osif_mldev *mldev = NULL;
	osif_dev  *osdev = NULL;
	uint32_t mlo_key = 0;

	/* only when DS is enabled valid node id will be updated */
	mlo_param->out_ppe_ds_node_id = MLO_METADATA_INVALID_DS_NODE_ID;

	if (qdf_unlikely(!dest_dev)) {
		qdf_err("dest_dev is NULL");
		return mlo_key;
	}

	if (!dest_dev->ieee80211_ptr)
		return mlo_key;

	mldev = ath_netdev_priv(dest_dev);
	if (qdf_unlikely(!mldev)) {
		qdf_err("No mldev found for dev %s", dest_dev->name);
		return mlo_key;
	}

	if (mldev->dev_type != OSIF_NETDEV_TYPE_MLO) {
		qdf_err("dest_dev is not a mlo dev %s", dest_dev->name);
		/* for non mlo vaps return the valid DS node id */
		mlo_param->out_ppe_ds_node_id =
				qca_mlo_get_ds_node_id((osif_dev *)mldev);
		return mlo_key;
	}

	if (mldev->wdev.iftype == NL80211_IFTYPE_AP)
		osdev = osifp_peer_find_hash_find_osdev(mldev, dest_mac);
	else
		osdev = osif_sta_mlo_find_osdev(mldev);

	if (!osdev) {
		qdf_err("unable to find the peer" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(dest_mac));
		return mlo_key;
	}


	mlo_param->out_ppe_ds_node_id =
			qca_mlo_get_ds_node_id(osdev);
	qca_mlo_metadata_set_primary_link_id(&mlo_key, osdev->mlo_link_id);
	qca_mlo_metadata_set_tag(&mlo_key);
	return mlo_key;
}

#else
uint32_t qca_mlo_get_mark_metadata(struct qca_mlo_metadata_param *mlo_param)
{
	return 0;
}
#endif
