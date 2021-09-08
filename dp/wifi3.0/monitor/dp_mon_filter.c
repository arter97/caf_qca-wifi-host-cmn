/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
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

#include <hal_api.h>
#include <wlan_cfg.h>
#include "dp_types.h"
#include "dp_internal.h"
#include "dp_htt.h"
#include "dp_mon.h"
#include "dp_mon_filter.h"

/**
 * dp_mon_filter_mode_type_to_str
 *	Monitor Filter mode to string
 */
static int8_t *dp_mon_filter_mode_type_to_str[DP_MON_FILTER_MAX_MODE] = {
#ifdef QCA_ENHANCED_STATS_SUPPORT
	"DP MON FILTER ENHACHED STATS MODE",
#endif /* QCA_ENHANCED_STATS_SUPPORT */
#ifdef QCA_MCOPY_SUPPORT
	"DP MON FILTER MCOPY MODE",
#endif /* QCA_MCOPY_SUPPORT */
#if defined(ATH_SUPPORT_NAC_RSSI) || defined(ATH_SUPPORT_NAC)
	"DP MON FILTER SMART MONITOR MODE",
#endif /* ATH_SUPPORT_NAC_RSSI || ATH_SUPPORT_NAC */
	"DP_MON FILTER MONITOR MODE",
#ifdef	WLAN_RX_PKT_CAPTURE_ENH
	"DP MON FILTER RX CAPTURE MODE",
#endif /* WLAN_RX_PKT_CAPTURE_ENH */
#ifdef WDI_EVENT_ENABLE
	"DP MON FILTER PKT LOG FULL MODE",
	"DP MON FILTER PKT LOG LITE_MODE",
#endif /* WDI_EVENT_ENABLE */
};

/**
 * dp_mon_filter_show_filter() - Show the set filters
 * @mon_pdev: Monitor pdev handle
 * @mode: The filter modes
 * @tlv_filter: tlv filter
 */
static void dp_mon_filter_show_filter(struct dp_mon_pdev *mon_pdev,
				      enum dp_mon_filter_mode mode,
				      struct dp_mon_filter *filter)
{
	struct htt_rx_ring_tlv_filter *tlv_filter = &filter->tlv_filter;

	DP_MON_FILTER_PRINT("[%s]: Valid: %d",
			    dp_mon_filter_mode_type_to_str[mode],
			    filter->valid);
	DP_MON_FILTER_PRINT("mpdu_start: %d", tlv_filter->mpdu_start);
	DP_MON_FILTER_PRINT("msdu_start: %d", tlv_filter->msdu_start);
	DP_MON_FILTER_PRINT("packet: %d", tlv_filter->packet);
	DP_MON_FILTER_PRINT("msdu_end: %d", tlv_filter->msdu_end);
	DP_MON_FILTER_PRINT("mpdu_end: %d", tlv_filter->mpdu_end);
	DP_MON_FILTER_PRINT("packet_header: %d",
			    tlv_filter->packet_header);
	DP_MON_FILTER_PRINT("attention: %d", tlv_filter->attention);
	DP_MON_FILTER_PRINT("ppdu_start: %d", tlv_filter->ppdu_start);
	DP_MON_FILTER_PRINT("ppdu_end: %d", tlv_filter->ppdu_end);
	DP_MON_FILTER_PRINT("ppdu_end_user_stats: %d",
			    tlv_filter->ppdu_end_user_stats);
	DP_MON_FILTER_PRINT("ppdu_end_user_stats_ext: %d",
			    tlv_filter->ppdu_end_user_stats_ext);
	DP_MON_FILTER_PRINT("ppdu_end_status_done: %d",
			    tlv_filter->ppdu_end_status_done);
	DP_MON_FILTER_PRINT("header_per_msdu: %d", tlv_filter->header_per_msdu);
	DP_MON_FILTER_PRINT("enable_fp: %d", tlv_filter->enable_fp);
	DP_MON_FILTER_PRINT("enable_md: %d", tlv_filter->enable_md);
	DP_MON_FILTER_PRINT("enable_mo: %d", tlv_filter->enable_mo);
	DP_MON_FILTER_PRINT("fp_mgmt_filter: 0x%x", tlv_filter->fp_mgmt_filter);
	DP_MON_FILTER_PRINT("mo_mgmt_filter: 0x%x", tlv_filter->mo_mgmt_filter);
	DP_MON_FILTER_PRINT("fp_ctrl_filter: 0x%x", tlv_filter->fp_ctrl_filter);
	DP_MON_FILTER_PRINT("mo_ctrl_filter: 0x%x", tlv_filter->mo_ctrl_filter);
	DP_MON_FILTER_PRINT("fp_data_filter: 0x%x", tlv_filter->fp_data_filter);
	DP_MON_FILTER_PRINT("mo_data_filter: 0x%x", tlv_filter->mo_data_filter);
	DP_MON_FILTER_PRINT("md_data_filter: 0x%x", tlv_filter->md_data_filter);
	DP_MON_FILTER_PRINT("md_mgmt_filter: 0x%x", tlv_filter->md_mgmt_filter);
	DP_MON_FILTER_PRINT("md_ctrl_filter: 0x%x", tlv_filter->md_ctrl_filter);
}

/**
 * dp_mon_ht2_rx_ring_cfg() - Send the tlv config to fw for a srng_type
 * based on target
 * @soc: DP soc handle
 * @pdev: DP pdev handle
 * @srng_type: The srng type for which filter wll be set
 * @tlv_filter: tlv filter
 */
static QDF_STATUS
dp_mon_ht2_rx_ring_cfg(struct dp_soc *soc,
		       struct dp_pdev *pdev,
		       enum dp_mon_filter_srng_type srng_type,
		       struct htt_rx_ring_tlv_filter *tlv_filter)
{
	int mac_id;
	int max_mac_rings = wlan_cfg_get_num_mac_rings(pdev->wlan_cfg_ctx);
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/*
	 * Overwrite the max_mac_rings for the status rings.
	 */
	if (srng_type == DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS)
		dp_is_hw_dbs_enable(soc, &max_mac_rings);

	dp_mon_filter_info("%pK: srng type %d Max_mac_rings %d ",
			   soc, srng_type, max_mac_rings);

	/*
	 * Loop through all MACs per radio and set the filter to the individual
	 * macs. For MCL
	 */
	for (mac_id = 0; mac_id < max_mac_rings; mac_id++) {
		int mac_for_pdev =
			dp_get_mac_id_for_pdev(mac_id, pdev->pdev_id);
		int lmac_id = dp_get_lmac_id_for_pdev_id(soc, mac_id, pdev->pdev_id);
		int hal_ring_type, ring_buf_size;
		hal_ring_handle_t hal_ring_hdl;

		switch (srng_type) {
		case DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF:
			hal_ring_hdl = pdev->rx_mac_buf_ring[lmac_id].hal_srng;
			hal_ring_type = RXDMA_BUF;
			ring_buf_size = RX_DATA_BUFFER_SIZE;
			break;

		case DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS:
			/*
			 * If two back to back HTT msg sending happened in
			 * short time, the second HTT msg source SRNG HP
			 * writing has chance to fail, this has been confirmed
			 * by HST HW.
			 * for monitor mode, here is the last HTT msg for sending.
			 * if the 2nd HTT msg for monitor status ring sending failed,
			 * HW won't provide anything into 2nd monitor status ring.
			 * as a WAR, add some delay before 2nd HTT msg start sending,
			 * > 2us is required per HST HW, delay 100 us for safe.
			 */
			if (mac_id)
				qdf_udelay(100);

			hal_ring_hdl =
				soc->rxdma_mon_status_ring[lmac_id].hal_srng;
			hal_ring_type = RXDMA_MONITOR_STATUS;
			ring_buf_size = RX_MON_STATUS_BUF_SIZE;
			break;

		case DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF:
			hal_ring_hdl =
				soc->rxdma_mon_buf_ring[lmac_id].hal_srng;
			hal_ring_type = RXDMA_MONITOR_BUF;
			ring_buf_size = RX_DATA_BUFFER_SIZE;
			break;

		default:
			return QDF_STATUS_E_FAILURE;
		}

		status = htt_h2t_rx_ring_cfg(soc->htt_handle, mac_for_pdev,
					     hal_ring_hdl, hal_ring_type,
					     ring_buf_size,
					     tlv_filter);
		if (status != QDF_STATUS_SUCCESS)
			return status;
	}

	return status;
}

/**
 * dp_mon_filter_ht2_setup() - Setup the filter for the Target setup
 * @soc: DP soc handle
 * @pdev: DP pdev handle
 * @srng_type: The srng type for which filter wll be set
 * @tlv_filter: tlv filter
 */
static void dp_mon_filter_ht2_setup(struct dp_soc *soc, struct dp_pdev *pdev,
				    enum dp_mon_filter_srng_type srng_type,
				    struct dp_mon_filter *filter)
{
	int32_t current_mode = 0;
	struct htt_rx_ring_tlv_filter *tlv_filter = &filter->tlv_filter;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	/*
	 * Loop through all the modes.
	 */
	for (current_mode = 0; current_mode < DP_MON_FILTER_MAX_MODE;
						current_mode++) {
		struct dp_mon_filter *mon_filter =
			&mon_pdev->filter[current_mode][srng_type];
		uint32_t src_filter = 0, dst_filter = 0;

		/*
		 * Check if the correct mode is enabled or not.
		 */
		if (!mon_filter->valid)
			continue;

		filter->valid = true;

		/*
		 * Set the super bit fields
		 */
		src_filter =
			DP_MON_FILTER_GET(&mon_filter->tlv_filter, FILTER_TLV);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_TLV);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_TLV, dst_filter);

		/*
		 * Set the filter management filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_FP_MGMT);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_FP_MGMT);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_FP_MGMT, dst_filter);

		/*
		 * Set the monitor other management filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_MO_MGMT);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MO_MGMT);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MO_MGMT, dst_filter);

		/*
		 * Set the filter pass control filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_FP_CTRL);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_FP_CTRL);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_FP_CTRL, dst_filter);

		/*
		 * Set the monitor other control filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_MO_CTRL);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MO_CTRL);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MO_CTRL, dst_filter);

		/*
		 * Set the filter pass data filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_FP_DATA);
		dst_filter = DP_MON_FILTER_GET(tlv_filter,
					       FILTER_FP_DATA);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter,
				  FILTER_FP_DATA, dst_filter);

		/*
		 * Set the monitor other data filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_MO_DATA);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MO_DATA);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MO_DATA, dst_filter);

		/*
		 * Set the monitor direct data filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_MD_DATA);
		dst_filter = DP_MON_FILTER_GET(tlv_filter,
					       FILTER_MD_DATA);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter,
				  FILTER_MD_DATA, dst_filter);

		/*
		 * Set the monitor direct management filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_MD_MGMT);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MD_MGMT);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MD_MGMT, dst_filter);

		/*
		 * Set the monitor direct management filter.
		 */
		src_filter = DP_MON_FILTER_GET(&mon_filter->tlv_filter,
					       FILTER_MD_CTRL);
		dst_filter = DP_MON_FILTER_GET(tlv_filter, FILTER_MD_CTRL);
		dst_filter |= src_filter;
		DP_MON_FILTER_SET(tlv_filter, FILTER_MD_CTRL, dst_filter);
	}

	dp_mon_filter_show_filter(mon_pdev, 0, filter);
}

#ifdef QCA_MONITOR_PKT_SUPPORT
/**
 * dp_mon_filter_reset_mon_srng()
 * @soc: DP SoC handle
 * @pdev: DP pdev handle
 * @mon_srng_type: Monitor srng type
 */
static void
dp_mon_filter_reset_mon_srng(struct dp_soc *soc, struct dp_pdev *pdev,
			     enum dp_mon_filter_srng_type mon_srng_type)
{
	struct htt_rx_ring_tlv_filter tlv_filter = {0};

	if (dp_mon_ht2_rx_ring_cfg(soc, pdev, mon_srng_type,
				   &tlv_filter) != QDF_STATUS_SUCCESS) {
		dp_mon_filter_err("%pK: Monitor destinatin ring filter setting failed",
				  soc);
	}
}
#endif

#if defined(QCA_MCOPY_SUPPORT) || defined(ATH_SUPPORT_NAC_RSSI) \
	|| defined(ATH_SUPPORT_NAC) || defined(WLAN_RX_PKT_CAPTURE_ENH)
/**
 * dp_mon_filter_check_co_exist() - Check the co-existing of the
 * enabled modes.
 * @pdev: DP pdev handle
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_mon_filter_check_co_exist(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	/*
	 * Check if the Rx Enhanced capture mode, monitor mode,
	 * smart_monitor_mode and mcopy mode can co-exist together.
	 */
	if ((mon_pdev->rx_enh_capture_mode != CDP_RX_ENH_CAPTURE_DISABLED) &&
	    ((mon_pdev->neighbour_peers_added && mon_pdev->mvdev) ||
		 mon_pdev->mcopy_mode)) {
		dp_mon_filter_err("%pK:Rx Capture mode can't exist with modes:\n"
				  "Smart Monitor Mode:%d\n"
				  "M_Copy Mode:%d", pdev->soc,
				  mon_pdev->neighbour_peers_added,
				  mon_pdev->mcopy_mode);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Check if the monitor mode cannot co-exist with any other mode.
	 */
	if ((mon_pdev->mvdev && mon_pdev->monitor_configured) &&
	    (mon_pdev->mcopy_mode || mon_pdev->neighbour_peers_added)) {
		dp_mon_filter_err("%pK: Monitor mode can't exist with modes\n"
				  "M_Copy Mode:%d\n"
				  "Smart Monitor Mode:%d",
				  pdev->soc, mon_pdev->mcopy_mode,
				  mon_pdev->neighbour_peers_added);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Check if the smart monitor mode can co-exist with any other mode
	 */
	if (mon_pdev->neighbour_peers_added &&
	    ((mon_pdev->mcopy_mode) || mon_pdev->monitor_configured)) {
		dp_mon_filter_err("%pk: Smart Monitor mode can't exist with modes\n"
				  "M_Copy Mode:%d\n"
				  "Monitor Mode:%d",
				  pdev->soc, mon_pdev->mcopy_mode,
				  mon_pdev->monitor_configured);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Check if the m_copy, monitor mode and the smart_monitor_mode
	 * can co-exist togther.
	 */
	if (mon_pdev->mcopy_mode &&
	    (mon_pdev->mvdev || mon_pdev->neighbour_peers_added)) {
		dp_mon_filter_err("%pK: mcopy mode can't exist with modes\n"
				  "Monitor Mode:%pK\n"
				  "Smart Monitor Mode:%d",
				  pdev->soc, mon_pdev->mvdev,
				  mon_pdev->neighbour_peers_added);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Check if the Rx packet log lite or full can co-exist with
	 * the enable modes.
	 */
	if ((mon_pdev->rx_pktlog_mode != DP_RX_PKTLOG_DISABLED) &&
	    !mon_pdev->rx_pktlog_cbf &&
	    (mon_pdev->mvdev || mon_pdev->monitor_configured)) {
		dp_mon_filter_err("%pK: Rx pktlog full/lite can't exist with modes\n"
				  "Monitor Mode:%d", pdev->soc,
				  mon_pdev->monitor_configured);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS dp_mon_filter_check_co_exist(struct dp_pdev *pdev)
{
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;

	/*
	 * Check if the Rx packet log lite or full can co-exist with
	 * the enable modes.
	 */
	if ((mon_pdev->rx_pktlog_mode != DP_RX_PKTLOG_DISABLED) &&
	    (mon_pdev->mvdev || mon_pdev->monitor_configured)) {
		dp_mon_filter_err("%pK: Rx pktlog full/lite can't exist with modes\n"
				  "Monitor Mode:%d", pdev->soc,
				  mon_pdev->monitor_configured);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef QCA_MONITOR_PKT_SUPPORT
/**
 * dp_mon_filter_set_mon_cmn() - Setp the common mon filters
 * @mon_pdev: Monitor pdev handle
 * @filter: DP mon filter
 *
 * Return: QDF_STATUS
 */
static void dp_mon_filter_set_mon_cmn(struct dp_mon_pdev *mon_pdev,
				      struct dp_mon_filter *filter)
{
	filter->tlv_filter.mpdu_start = 1;
	filter->tlv_filter.msdu_start = 1;
	filter->tlv_filter.packet = 1;
	filter->tlv_filter.msdu_end = 1;
	filter->tlv_filter.mpdu_end = 1;
	filter->tlv_filter.packet_header = 1;
	filter->tlv_filter.attention = 1;
	filter->tlv_filter.ppdu_start = 0;
	filter->tlv_filter.ppdu_end = 0;
	filter->tlv_filter.ppdu_end_user_stats = 0;
	filter->tlv_filter.ppdu_end_user_stats_ext = 0;
	filter->tlv_filter.ppdu_end_status_done = 0;
	filter->tlv_filter.header_per_msdu = 1;
	filter->tlv_filter.enable_fp =
		(mon_pdev->mon_filter_mode & MON_FILTER_PASS) ? 1 : 0;
	filter->tlv_filter.enable_mo =
		(mon_pdev->mon_filter_mode & MON_FILTER_OTHER) ? 1 : 0;

	filter->tlv_filter.fp_mgmt_filter = mon_pdev->fp_mgmt_filter;
	filter->tlv_filter.fp_ctrl_filter = mon_pdev->fp_ctrl_filter;
	filter->tlv_filter.fp_data_filter = mon_pdev->fp_data_filter;
	filter->tlv_filter.mo_mgmt_filter = mon_pdev->mo_mgmt_filter;
	filter->tlv_filter.mo_ctrl_filter = mon_pdev->mo_ctrl_filter;
	filter->tlv_filter.mo_data_filter = mon_pdev->mo_data_filter;
	filter->tlv_filter.offset_valid = false;
}
#endif

/**
 * dp_mon_filter_set_status_cmn() - Setp the common status filters
 * @mon_pdev: Monitor pdev handle
 * @filter: Dp mon filters
 *
 * Return: QDF_STATUS
 */
static void dp_mon_filter_set_status_cmn(struct dp_mon_pdev *mon_pdev,
					 struct dp_mon_filter *filter)
{
	filter->tlv_filter.mpdu_start = 1;
	filter->tlv_filter.msdu_start = 0;
	filter->tlv_filter.packet = 0;
	filter->tlv_filter.msdu_end = 0;
	filter->tlv_filter.mpdu_end = 0;
	filter->tlv_filter.attention = 0;
	filter->tlv_filter.ppdu_start = 1;
	filter->tlv_filter.ppdu_end = 1;
	filter->tlv_filter.ppdu_end_user_stats = 1;
	filter->tlv_filter.ppdu_end_user_stats_ext = 1;
	filter->tlv_filter.ppdu_end_status_done = 1;
	filter->tlv_filter.enable_fp = 1;
	filter->tlv_filter.enable_md = 0;
	filter->tlv_filter.fp_mgmt_filter = FILTER_MGMT_ALL;
	filter->tlv_filter.fp_ctrl_filter = FILTER_CTRL_ALL;
	filter->tlv_filter.fp_data_filter = FILTER_DATA_ALL;
	filter->tlv_filter.offset_valid = false;

	if (mon_pdev->mon_filter_mode & MON_FILTER_OTHER) {
		filter->tlv_filter.enable_mo = 1;
		filter->tlv_filter.mo_mgmt_filter = FILTER_MGMT_ALL;
		filter->tlv_filter.mo_ctrl_filter = FILTER_CTRL_ALL;
		filter->tlv_filter.mo_data_filter = FILTER_DATA_ALL;
	} else {
		filter->tlv_filter.enable_mo = 0;
	}
}

/**
 * dp_mon_filter_set_status_cbf() - Set the cbf status filters
 * @pdev: DP pdev handle
 * @filter: Dp mon filters
 *
 * Return: void
 */
static void dp_mon_filter_set_status_cbf(struct dp_pdev *pdev,
					 struct dp_mon_filter *filter)
{
	filter->tlv_filter.mpdu_start = 1;
	filter->tlv_filter.msdu_start = 0;
	filter->tlv_filter.packet = 0;
	filter->tlv_filter.msdu_end = 0;
	filter->tlv_filter.mpdu_end = 0;
	filter->tlv_filter.attention = 0;
	filter->tlv_filter.ppdu_start = 1;
	filter->tlv_filter.ppdu_end = 1;
	filter->tlv_filter.ppdu_end_user_stats = 1;
	filter->tlv_filter.ppdu_end_user_stats_ext = 1;
	filter->tlv_filter.ppdu_end_status_done = 1;
	filter->tlv_filter.enable_fp = 1;
	filter->tlv_filter.enable_md = 0;
	filter->tlv_filter.fp_mgmt_filter = FILTER_MGMT_ACT_NO_ACK;
	filter->tlv_filter.fp_ctrl_filter = 0;
	filter->tlv_filter.fp_data_filter = 0;
	filter->tlv_filter.offset_valid = false;
	filter->tlv_filter.enable_mo = 0;
}

#ifdef QCA_MONITOR_PKT_SUPPORT
/**
 * dp_mon_filter_set_cbf_cmn() - Set the common cbf mode filters
 * @pdev: DP pdev handle
 * @filter: Dp mon filters
 *
 * Return: void
 */
static void dp_mon_filter_set_cbf_cmn(struct dp_pdev *pdev,
				      struct dp_mon_filter *filter)
{
	filter->tlv_filter.mpdu_start = 1;
	filter->tlv_filter.msdu_start = 1;
	filter->tlv_filter.packet = 1;
	filter->tlv_filter.msdu_end = 1;
	filter->tlv_filter.mpdu_end = 1;
	filter->tlv_filter.attention = 1;
	filter->tlv_filter.ppdu_start = 0;
	filter->tlv_filter.ppdu_end =  0;
	filter->tlv_filter.ppdu_end_user_stats = 0;
	filter->tlv_filter.ppdu_end_user_stats_ext = 0;
	filter->tlv_filter.ppdu_end_status_done = 0;
	filter->tlv_filter.enable_fp = 1;
	filter->tlv_filter.enable_md = 0;
	filter->tlv_filter.fp_mgmt_filter = FILTER_MGMT_ACT_NO_ACK;
	filter->tlv_filter.offset_valid = false;
	filter->tlv_filter.enable_mo = 0;
}
#endif

#ifdef QCA_ENHANCED_STATS_SUPPORT
/**
 * dp_mon_filter_setup_enhanced_stats() - Setup the enhanced stats filter
 * @mon_pdev: Monitor pdev handle
 */
void dp_mon_filter_setup_enhanced_stats(struct dp_mon_pdev *mon_pdev)
{
	struct dp_mon_filter filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_ENHACHED_STATS_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;

	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev Context is null");
		return;
	}

	/* Enabled the filter */
	filter.valid = true;

	dp_mon_filter_set_status_cmn(mon_pdev, &filter);
	dp_mon_filter_show_filter(mon_pdev, mode, &filter);
	mon_pdev->filter[mode][srng_type] = filter;
}

/**
 * dp_mon_filter_reset_enhanced_stats() - Reset the enhanced stats filter
 * @mon_pdev: Monitor pdev handle
 */
void dp_mon_filter_reset_enhanced_stats(struct dp_mon_pdev *mon_pdev)
{
	struct dp_mon_filter filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_ENHACHED_STATS_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;

	if (!mon_pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev->filter[mode][srng_type] = filter;
}
#endif /* QCA_ENHANCED_STATS_SUPPORT */

#ifdef QCA_MCOPY_SUPPORT
#ifdef QCA_MONITOR_PKT_SUPPORT
static void
dp_mon_filter_set_reset_mcopy_mon_buf(struct dp_pdev *pdev,
				      struct dp_mon_filter *pfilter)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MCOPY_MODE;
	enum dp_mon_filter_srng_type srng_type;

	srng_type = ((soc->wlan_cfg_ctx->rxdma1_enable) ?
			DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF :
			DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF);

	/* Set the filter */
	if (pfilter->valid) {
		dp_mon_filter_set_mon_cmn(mon_pdev, pfilter);

		pfilter->tlv_filter.fp_data_filter = 0;
		pfilter->tlv_filter.mo_data_filter = 0;

		dp_mon_filter_show_filter(mon_pdev, mode, pfilter);
		mon_pdev->filter[mode][srng_type] = *pfilter;
	} else /* Reset the filter */
		mon_pdev->filter[mode][srng_type] = *pfilter;
}
#else
static void
dp_mon_filter_set_reset_mcopy_mon_buf(struct dp_pdev *pdev,
				      struct dp_mon_filter *pfilter)
{
}
#endif

/**
 * dp_mon_filter_setup_mcopy_mode() - Setup the m_copy mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_mcopy_mode(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	struct dp_soc *soc = NULL;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MCOPY_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("Soc Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	if (!mon_pdev) {
		dp_mon_filter_err("monitor pdev Context is null");
		return;
	}
	/* Enabled the filter */
	filter.valid = true;
	dp_mon_filter_set_reset_mcopy_mon_buf(pdev, &filter);

	/* Clear the filter as the same filter will be used to set the
	 * monitor status ring
	 */
	qdf_mem_zero(&(filter), sizeof(struct dp_mon_filter));

	/* Enabled the filter */
	filter.valid = true;
	dp_mon_filter_set_status_cmn(mon_pdev, &filter);

	/* Setup the filter */
	filter.tlv_filter.enable_mo = 1;
	filter.tlv_filter.packet_header = 1;
	filter.tlv_filter.mpdu_end = 1;
	dp_mon_filter_show_filter(mon_pdev, mode, &filter);

	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	mon_pdev->filter[mode][srng_type] = filter;
}

/**
 * dp_mon_filter_reset_mcopy_mode() - Reset the m_copy mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_mcopy_mode(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	struct dp_soc *soc = NULL;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MCOPY_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("Soc Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	dp_mon_filter_set_reset_mcopy_mon_buf(pdev, &filter);

	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	mon_pdev->filter[mode][srng_type] = filter;
}
#endif /* QCA_MCOPY_SUPPORT */

#if defined(ATH_SUPPORT_NAC_RSSI) || defined(ATH_SUPPORT_NAC)
/**
 * dp_mon_filter_setup_smart_monitor() - Setup the smart monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_smart_monitor(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	struct dp_soc *soc = NULL;
	struct dp_mon_soc *mon_soc;

	enum dp_mon_filter_mode mode = DP_MON_FILTER_SMART_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("Soc Context is null");
		return;
	}

	mon_soc = soc->monitor_soc;
	mon_pdev = pdev->monitor_pdev;

	/* Enabled the filter */
	filter.valid = true;
	dp_mon_filter_set_status_cmn(mon_pdev, &filter);

	if (mon_soc->hw_nac_monitor_support) {
		filter.tlv_filter.enable_md = 1;
		filter.tlv_filter.packet_header = 1;
		filter.tlv_filter.md_data_filter = FILTER_DATA_ALL;
	}

	dp_mon_filter_show_filter(mon_pdev, mode, &filter);
	mon_pdev->filter[mode][srng_type] = filter;
}

/**
 * dp_mon_filter_reset_smart_monitor() - Reset the smart monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_smart_monitor(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_SMART_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("monitor pdev Context is null");
		return;
	}
	mon_pdev = pdev->monitor_pdev;
	mon_pdev->filter[mode][srng_type] = filter;
}
#endif /* ATH_SUPPORT_NAC_RSSI || ATH_SUPPORT_NAC */

#ifdef WLAN_RX_PKT_CAPTURE_ENH
#ifdef QCA_MONITOR_PKT_SUPPORT
static void
dp_mon_filter_set_reset_rx_enh_capture_mon_buf(struct dp_pdev *pdev,
					       struct dp_mon_filter *pfilter)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_RX_CAPTURE_MODE;
	enum dp_mon_filter_srng_type srng_type;

	srng_type = ((soc->wlan_cfg_ctx->rxdma1_enable) ?
			DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF :
			DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF);

	/* Set the filter */
	if (pfilter->valid) {
		dp_mon_filter_set_mon_cmn(mon_pdev, pfilter);

		pfilter->tlv_filter.fp_mgmt_filter = 0;
		pfilter->tlv_filter.fp_ctrl_filter = 0;
		pfilter->tlv_filter.fp_data_filter = 0;
		pfilter->tlv_filter.mo_mgmt_filter = 0;
		pfilter->tlv_filter.mo_ctrl_filter = 0;
		pfilter->tlv_filter.mo_data_filter = 0;

		dp_mon_filter_show_filter(mon_pdev, mode, pfilter);
		pdev->monitor_pdev->filter[mode][srng_type] = *pfilter;
	} else /* Reset the filter */
		pdev->monitor_pdev->filter[mode][srng_type] = *pfilter;
}
#else
static void
dp_mon_filter_set_reset_rx_enh_capture_mon_buf(struct dp_pdev *pdev,
					       struct dp_mon_filter *pfilter)
{
}
#endif

/**
 * dp_mon_filter_setup_rx_enh_capture() - Setup the Rx capture mode filters
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_enh_capture(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	struct dp_soc *soc = NULL;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_RX_CAPTURE_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("Soc Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;

	/* Enabled the filter */
	filter.valid = true;
	dp_mon_filter_set_reset_rx_enh_capture_mon_buf(pdev, &filter);

	/* Clear the filter as the same filter will be used to set the
	 * monitor status ring
	 */
	qdf_mem_zero(&(filter), sizeof(struct dp_mon_filter));

	/* Enabled the filter */
	filter.valid = true;
	dp_mon_filter_set_status_cmn(mon_pdev, &filter);

	/* Setup the filter */
	filter.tlv_filter.mpdu_end = 1;
	filter.tlv_filter.enable_mo = 1;
	filter.tlv_filter.packet_header = 1;

	if (mon_pdev->rx_enh_capture_mode == CDP_RX_ENH_CAPTURE_MPDU) {
		filter.tlv_filter.header_per_msdu = 0;
		filter.tlv_filter.enable_mo = 0;
	} else if (mon_pdev->rx_enh_capture_mode ==
			CDP_RX_ENH_CAPTURE_MPDU_MSDU) {
		bool is_rx_mon_proto_flow_tag_enabled =
		wlan_cfg_is_rx_mon_protocol_flow_tag_enabled(soc->wlan_cfg_ctx);
		filter.tlv_filter.header_per_msdu = 1;
		filter.tlv_filter.enable_mo = 0;
		if (mon_pdev->is_rx_enh_capture_trailer_enabled ||
		    is_rx_mon_proto_flow_tag_enabled)
			filter.tlv_filter.msdu_end = 1;
	}

	dp_mon_filter_show_filter(mon_pdev, mode, &filter);

	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	mon_pdev->filter[mode][srng_type] = filter;
}

/**
 * dp_mon_filter_reset_rx_enh_capture() - Reset the Rx capture mode filters
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_enh_capture(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	struct dp_soc *soc = NULL;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_RX_CAPTURE_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("Soc Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	dp_mon_filter_set_reset_rx_enh_capture_mon_buf(pdev, &filter);

	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	mon_pdev->filter[mode][srng_type] = filter;
}
#endif /* WLAN_RX_PKT_CAPTURE_ENH */

#ifdef QCA_MONITOR_PKT_SUPPORT
static void dp_mon_filter_set_reset_mon_buf(struct dp_pdev *pdev,
					    struct dp_mon_filter *pfilter)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type;

	srng_type = ((soc->wlan_cfg_ctx->rxdma1_enable) ?
			DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF :
			DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF);

	/* set the filter */
	if (pfilter->valid) {
		dp_mon_filter_set_mon_cmn(mon_pdev, pfilter);

		dp_mon_filter_show_filter(mon_pdev, mode, pfilter);
		mon_pdev->filter[mode][srng_type] = *pfilter;
	} else /* reset the filter */
		mon_pdev->filter[mode][srng_type] = *pfilter;
}
#else
static void dp_mon_filter_set_reset_mon_buf(struct dp_pdev *pdev,
					    struct dp_mon_filter *pfilter)
{
}
#endif

/**
 * dp_mon_filter_setup_mon_mode() - Setup the Rx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_mon_mode(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	struct dp_soc *soc = NULL;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("Soc Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	filter.valid = true;
	dp_mon_filter_set_reset_mon_buf(pdev, &filter);

	/* Clear the filter as the same filter will be used to set the
	 * monitor status ring
	 */
	qdf_mem_zero(&(filter), sizeof(struct dp_mon_filter));

	/* Enabled the filter */
	filter.valid = true;
	dp_mon_filter_set_status_cmn(mon_pdev, &filter);
	dp_mon_filter_show_filter(mon_pdev, mode, &filter);

	/* Store the above filter */
	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	mon_pdev->filter[mode][srng_type] = filter;
}

/**
 * dp_mon_filter_reset_mon_mode() - Reset the Rx monitor mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_mon_mode(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	struct dp_soc *soc = NULL;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_MONITOR_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("Soc Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	dp_mon_filter_set_reset_mon_buf(pdev, &filter);

	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	mon_pdev->filter[mode][srng_type] = filter;
}

#ifdef WDI_EVENT_ENABLE
/**
 * dp_mon_filter_setup_rx_pkt_log_full() - Setup the Rx pktlog full mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_pkt_log_full(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_FULL_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	/* Enabled the filter */
	filter.valid = true;
	dp_mon_filter_set_status_cmn(mon_pdev, &filter);

	/* Setup the filter */
	filter.tlv_filter.packet_header = 1;
	filter.tlv_filter.msdu_start = 1;
	filter.tlv_filter.msdu_end = 1;
	filter.tlv_filter.mpdu_end = 1;
	filter.tlv_filter.attention = 1;

	dp_mon_filter_show_filter(mon_pdev, mode, &filter);
	mon_pdev->filter[mode][srng_type] = filter;
}

/**
 * dp_mon_filter_reset_rx_pkt_log_full() - Reset the Rx pktlog full mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_pkt_log_full(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_FULL_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	mon_pdev->filter[mode][srng_type] = filter;
}

/**
 * dp_mon_filter_setup_rx_pkt_log_lite() - Setup the Rx pktlog lite mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_pkt_log_lite(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_LITE_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	/* Enabled the filter */
	filter.valid = true;
	dp_mon_filter_set_status_cmn(mon_pdev, &filter);

	dp_mon_filter_show_filter(mon_pdev, mode, &filter);
	mon_pdev->filter[mode][srng_type] = filter;
}

/**
 * dp_mon_filter_reset_rx_pkt_log_lite() - Reset the Rx pktlog lite mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_pkt_log_lite(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_LITE_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("dp pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	mon_pdev->filter[mode][srng_type] = filter;
}

#ifdef QCA_MONITOR_PKT_SUPPORT
static void
dp_mon_filter_set_reset_rx_pkt_log_cbf_mon_buf(struct dp_pdev *pdev,
					       struct dp_mon_filter *pfilter)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_CBF_MODE;
	enum dp_mon_filter_srng_type srng_type;

	srng_type = ((soc->wlan_cfg_ctx->rxdma1_enable) ?
			DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF :
			DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF);

	/*set the filter */
	if (pfilter->valid) {
		dp_mon_filter_set_cbf_cmn(pdev, pfilter);

		dp_mon_filter_show_filter(mon_pdev, mode, pfilter);
		mon_pdev->filter[mode][srng_type] = *pfilter;
	} else /* reset the filter */
		mon_pdev->filter[mode][srng_type] = *pfilter;
}
#else
static void
dp_mon_filter_set_reset_rx_pkt_log_cbf_mon_buf(struct dp_pdev *pdev,
					       struct dp_mon_filter *pfilter)
{
}
#endif

/**
 * dp_mon_filter_setup_rx_pkt_log_cbf() - Setup the Rx pktlog CBF mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_setup_rx_pkt_log_cbf(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	struct dp_soc *soc = NULL;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_CBF_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("Soc Context is null");
		return;
	}

	/* Enabled the filter */
	filter.valid = true;
	dp_mon_filter_set_status_cbf(pdev, &filter);
	dp_mon_filter_show_filter(mon_pdev, mode, &filter);
	mon_pdev->filter[mode][srng_type] = filter;

	/* Clear the filter as the same filter will be used to set the
	 * monitor status ring
	 */
	qdf_mem_zero(&(filter), sizeof(struct dp_mon_filter));

	filter.valid = true;
	dp_mon_filter_set_reset_rx_pkt_log_cbf_mon_buf(pdev, &filter);
}

/**
 * dp_mon_filter_reset_rx_pktlog_cbf() - Reset the Rx pktlog CBF mode filter
 * @pdev: DP pdev handle
 */
void dp_mon_filter_reset_rx_pktlog_cbf(struct dp_pdev *pdev)
{
	struct dp_mon_filter filter = {0};
	struct dp_soc *soc = NULL;
	enum dp_mon_filter_mode mode = DP_MON_FILTER_PKT_LOG_CBF_MODE;
	enum dp_mon_filter_srng_type srng_type =
				DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		QDF_TRACE(QDF_MODULE_ID_MON_FILTER, QDF_TRACE_LEVEL_ERROR,
			  FL("pdev Context is null"));
		return;
	}

	mon_pdev = pdev->monitor_pdev;
	soc = pdev->soc;
	if (!soc) {
		QDF_TRACE(QDF_MODULE_ID_MON_FILTER, QDF_TRACE_LEVEL_ERROR,
			  FL("Soc Context is null"));
		return;
	}

	dp_mon_filter_set_reset_rx_pkt_log_cbf_mon_buf(pdev, &filter);

	srng_type = DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS;
	mon_pdev->filter[mode][srng_type] = filter;
}
#endif /* WDI_EVENT_ENABLE */

#ifdef WLAN_DP_RESET_MON_BUF_RING_FILTER
/**
 * dp_mon_should_reset_buf_ring_filter() - Reset the monitor buf ring filter
 * @pdev: DP PDEV handle
 *
 * WIN has targets which does not support monitor mode, but still do the
 * monitor mode init/deinit, only the rxdma1_enable flag will be set to 0.
 * MCL need to do the monitor buffer ring filter reset always, but this is
 * not needed for WIN targets where rxdma1 is not enabled (the indicator
 * that monitor mode is not enabled.
 * This function is used as WAR till WIN cleans up the monitor mode
 * function for targets where monitor mode is not enabled.
 *
 * Returns: true
 */
static inline bool dp_mon_should_reset_buf_ring_filter(struct dp_pdev *pdev)
{
	return (pdev->monitor_pdev->mvdev) ? true : false;
}
#else
static inline bool dp_mon_should_reset_buf_ring_filter(struct dp_pdev *pdev)
{
	return false;
}
#endif

#ifdef QCA_MONITOR_PKT_SUPPORT
static QDF_STATUS dp_mon_filter_mon_buf_update(struct dp_pdev *pdev,
					       struct dp_mon_filter *pfilter,
					       bool *pmon_mode_set)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_pdev *mon_pdev = pdev->monitor_pdev;
	enum dp_mon_filter_srng_type srng_type;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	srng_type = ((soc->wlan_cfg_ctx->rxdma1_enable) ?
			DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF :
			DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF);

	dp_mon_filter_ht2_setup(soc, pdev, srng_type, pfilter);

	*pmon_mode_set = pfilter->valid;
	if (dp_mon_should_reset_buf_ring_filter(pdev) || *pmon_mode_set) {
		status = dp_mon_ht2_rx_ring_cfg(soc, pdev,
						srng_type,
						&pfilter->tlv_filter);
	} else {
		/*
		 * For WIN case the monitor buffer ring is used and it does need
		 * reset when monitor mode gets enabled/disabled.
		 */
		if (soc->wlan_cfg_ctx->rxdma1_enable) {
			if (mon_pdev->monitor_configured || *pmon_mode_set) {
				status = dp_mon_ht2_rx_ring_cfg(soc, pdev,
								srng_type,
								&pfilter->tlv_filter);
			}
		}
	}

	return status;
}

static void dp_mon_filter_mon_buf_reset(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	enum dp_mon_filter_srng_type srng_type;

	srng_type = ((soc->wlan_cfg_ctx->rxdma1_enable) ?
			DP_MON_FILTER_SRNG_TYPE_RXDMA_MON_BUF :
			DP_MON_FILTER_SRNG_TYPE_RXDMA_BUF);

	dp_mon_filter_reset_mon_srng(soc, pdev, srng_type);
}
#else
static QDF_STATUS dp_mon_filter_mon_buf_update(struct dp_pdev *pdev,
					       struct dp_mon_filter *pfilter,
					       bool *pmon_mode_set)
{
	return QDF_STATUS_SUCCESS;
}

static void dp_mon_filter_mon_buf_reset(struct dp_pdev *pdev)
{
}
#endif

/**
 * dp_mon_filter_update() - Setup the monitor filter setting for a srng
 * type
 * @pdev: DP pdev handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_mon_filter_update(struct dp_pdev *pdev)
{
	struct dp_soc *soc;
	bool mon_mode_set = false;
	struct dp_mon_filter filter = {0};
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dp_mon_pdev *mon_pdev;

	if (!pdev) {
		dp_mon_filter_err("pdev Context is null");
		return QDF_STATUS_E_FAILURE;
	}

	mon_pdev = pdev->monitor_pdev;
	soc = pdev->soc;
	if (!soc) {
		dp_mon_filter_err("Soc Context is null");
		return QDF_STATUS_E_FAILURE;
	}

	status = dp_mon_filter_check_co_exist(pdev);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	if (soc->monitor_soc->monitor_mode_v2) {
		dp_mon_filter_err(" Mon ring not supported for this arch");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Setup the filters for the monitor destination ring.
	 */
	status = dp_mon_filter_mon_buf_update(pdev, &filter,
					      &mon_mode_set);

	if (status != QDF_STATUS_SUCCESS) {
		dp_mon_filter_err("%pK: Monitor destination ring filter setting failed",
				  soc);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Setup the filters for the status ring.
	 */
	qdf_mem_zero(&(filter), sizeof(filter));
	dp_mon_filter_ht2_setup(soc, pdev,
				DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS,
				&filter);

	/*
	 * Reset the monitor filters if the all the modes for the status rings
	 * are disabled. This is done to prevent the HW backpressure from the
	 * monitor destination ring in case the status ring filters
	 * are not enabled.
	 */
	if (!filter.valid && mon_mode_set)
		dp_mon_filter_mon_buf_reset(pdev);

	if (dp_mon_ht2_rx_ring_cfg(soc, pdev,
				   DP_MON_FILTER_SRNG_TYPE_RXDMA_MONITOR_STATUS,
				   &filter.tlv_filter) != QDF_STATUS_SUCCESS) {
		dp_mon_filter_err("%pK: Monitor status ring filter setting failed",
				  soc);
		dp_mon_filter_mon_buf_reset(pdev);
		return QDF_STATUS_E_FAILURE;
	}

	return status;
}

/**
 * dp_mon_filter_dealloc() - Deallocate the filter objects to be stored in
 * the radio object.
 * @pdev: DP pdev handle
 */
void dp_mon_filter_dealloc(struct dp_mon_pdev *mon_pdev)
{
	enum dp_mon_filter_mode mode;
	struct dp_mon_filter **mon_filter = NULL;

	if (!mon_pdev) {
		dp_mon_filter_err("Monitor pdev Context is null");
		return;
	}

	mon_filter = mon_pdev->filter;

	/*
	 * Check if the monitor filters are already allocated to the mon_pdev.
	 */
	if (!mon_filter) {
		dp_mon_filter_err("Found NULL memmory for the Monitor filter");
		return;
	}

	/*
	 * Iterate through the every mode and free the filter object.
	 */
	for (mode = 0; mode < DP_MON_FILTER_MAX_MODE; mode++) {
		if (!mon_filter[mode]) {
			continue;
		}

		qdf_mem_free(mon_filter[mode]);
		mon_filter[mode] = NULL;
	}

	qdf_mem_free(mon_filter);
	mon_pdev->filter = NULL;
}

/**
 * dp_mon_filter_alloc() - Allocate the filter objects to be stored in
 * the radio object.
 * @mon_pdev: Monitor pdev handle
 */
struct dp_mon_filter **dp_mon_filter_alloc(struct dp_mon_pdev *mon_pdev)
{
	struct dp_mon_filter **mon_filter = NULL;
	enum dp_mon_filter_mode mode;

	if (!mon_pdev) {
		dp_mon_filter_err("pdev Context is null");
		return NULL;
	}

	mon_filter = (struct dp_mon_filter **)qdf_mem_malloc(
			(sizeof(struct dp_mon_filter *) *
			 DP_MON_FILTER_MAX_MODE));
	if (!mon_filter) {
		dp_mon_filter_err("Monitor filter mem allocation failed");
		return NULL;
	}

	qdf_mem_zero(mon_filter,
		     sizeof(struct dp_mon_filter *) * DP_MON_FILTER_MAX_MODE);

	/*
	 * Allocate the memory for filters for different srngs for each modes.
	 */
	for (mode = 0; mode < DP_MON_FILTER_MAX_MODE; mode++) {
		mon_filter[mode] = qdf_mem_malloc(sizeof(struct dp_mon_filter) *
						  DP_MON_FILTER_SRNG_TYPE_MAX);
		/* Assign the mon_filter to the pdev->filter such
		 * that the dp_mon_filter_dealloc() can free up the filters. */
		if (!mon_filter[mode]) {
			mon_pdev->filter = mon_filter;
			goto fail;
		}
	}

	return mon_filter;
fail:
	dp_mon_filter_dealloc(mon_pdev);
	return NULL;
}
