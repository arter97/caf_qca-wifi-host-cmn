/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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

#include "hal_hw_headers.h"
#include "dp_types.h"
#include "dp_rx.h"
#include "dp_peer.h"
#include "hal_rx.h"
#include "hal_api.h"
#include "qdf_trace.h"
#include "qdf_nbuf.h"
#include "hal_api_mon.h"
#include "dp_rx_mon.h"
#include "wlan_cfg.h"
#include "dp_internal.h"
#ifdef WLAN_TX_PKT_CAPTURE_ENH
#include "dp_rx_mon_feature.h"

static inline void
dp_handle_tx_capture(struct dp_soc *soc, struct dp_pdev *pdev,
		     qdf_nbuf_t mon_mpdu)
{
	struct hal_rx_ppdu_info *ppdu_info = &pdev->ppdu_info;

	if (pdev->tx_capture_enabled
	    == CDP_TX_ENH_CAPTURE_DISABLED)
		return;

	if ((ppdu_info->sw_frame_group_id ==
	      HAL_MPDU_SW_FRAME_GROUP_CTRL_NDPA) ||
	     (ppdu_info->sw_frame_group_id ==
	      HAL_MPDU_SW_FRAME_GROUP_CTRL_BAR))
		dp_handle_tx_capture_from_dest(soc, pdev, mon_mpdu);
}

static void
dp_tx_capture_get_user_id(struct dp_pdev *dp_pdev, void *rx_desc_tlv)
{
	if (dp_pdev->tx_capture_enabled
	    != CDP_TX_ENH_CAPTURE_DISABLED)
		dp_pdev->ppdu_info.rx_info.user_id =
			HAL_RX_HW_DESC_MPDU_USER_ID(rx_desc_tlv);
}
#else
static inline void
dp_handle_tx_capture(struct dp_soc *soc, struct dp_pdev *pdev,
		     qdf_nbuf_t mon_mpdu)
{
}

static void
dp_tx_capture_get_user_id(struct dp_pdev *dp_pdev, void *rx_desc_tlv)
{
}
#endif


/* The maxinum buffer length allocated for radio tap */
#define MAX_MONITOR_HEADER (512)
/*
 * PPDU id is from 0 to 64k-1. PPDU id read from status ring and PPDU id
 * read from destination ring shall track each other. If the distance of
 * two ppdu id is less than 20000. It is assume no wrap around. Otherwise,
 * It is assume wrap around.
 */
#define NOT_PPDU_ID_WRAP_AROUND 20000
/*
 * The destination ring processing is stuck if the destrination is not
 * moving while status ring moves 16 ppdu. the destination ring processing
 * skips this destination ring ppdu as walkaround
 */
#define MON_DEST_RING_STUCK_MAX_CNT 16

/**
 * dp_rx_mon_link_desc_return() - Return a MPDU link descriptor to HW
 *			      (WBM), following error handling
 *
 * @dp_pdev: core txrx pdev context
 * @buf_addr_info: void pointer to monitor link descriptor buf addr info
 * Return: QDF_STATUS
 */
QDF_STATUS
dp_rx_mon_link_desc_return(struct dp_pdev *dp_pdev,
	hal_buff_addrinfo_t buf_addr_info, int mac_id)
{
	struct dp_srng *dp_srng;
	hal_ring_handle_t hal_ring_hdl;
	hal_soc_handle_t hal_soc;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	void *src_srng_desc;

	hal_soc = dp_pdev->soc->hal_soc;

	dp_srng = &dp_pdev->soc->rxdma_mon_desc_ring[mac_id];
	hal_ring_hdl = dp_srng->hal_srng;

	qdf_assert(hal_ring_hdl);

	if (qdf_unlikely(hal_srng_access_start(hal_soc, hal_ring_hdl))) {

		/* TODO */
		/*
		 * Need API to convert from hal_ring pointer to
		 * Ring Type / Ring Id combo
		 */
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"%s %d : \
			HAL RING Access For WBM Release SRNG Failed -- %pK",
			__func__, __LINE__, hal_ring_hdl);
		goto done;
	}

	src_srng_desc = hal_srng_src_get_next(hal_soc, hal_ring_hdl);

	if (qdf_likely(src_srng_desc)) {
		/* Return link descriptor through WBM ring (SW2WBM)*/
		hal_rx_mon_msdu_link_desc_set(hal_soc,
				src_srng_desc, buf_addr_info);
		status = QDF_STATUS_SUCCESS;
	} else {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"%s %d -- Monitor Link Desc WBM Release Ring Full",
			__func__, __LINE__);
	}
done:
	hal_srng_access_end(hal_soc, hal_ring_hdl);
	return status;
}

/**
 * dp_rx_mon_mpdu_pop() - Return a MPDU link descriptor to HW
 *			      (WBM), following error handling
 *
 * @soc: core DP main context
 * @mac_id: mac id which is one of 3 mac_ids
 * @rxdma_dst_ring_desc: void pointer to monitor link descriptor buf addr info
 * @head_msdu: head of msdu to be popped
 * @tail_msdu: tail of msdu to be popped
 * @npackets: number of packet to be popped
 * @ppdu_id: ppdu id of processing ppdu
 * @head: head of descs list to be freed
 * @tail: tail of decs list to be freed
 *
 * Return: number of msdu in MPDU to be popped
 */
static inline uint32_t
dp_rx_mon_mpdu_pop(struct dp_soc *soc, uint32_t mac_id,
	hal_rxdma_desc_t rxdma_dst_ring_desc, qdf_nbuf_t *head_msdu,
	qdf_nbuf_t *tail_msdu, uint32_t *npackets, uint32_t *ppdu_id,
	union dp_rx_desc_list_elem_t **head,
	union dp_rx_desc_list_elem_t **tail)
{
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	void *rx_desc_tlv;
	void *rx_msdu_link_desc;
	qdf_nbuf_t msdu;
	qdf_nbuf_t last;
	struct hal_rx_msdu_list msdu_list;
	uint16_t num_msdus;
	uint32_t rx_buf_size, rx_pkt_offset;
	struct hal_buf_info buf_info;
	uint32_t rx_bufs_used = 0;
	uint32_t msdu_ppdu_id, msdu_cnt;
	uint8_t *data;
	uint32_t i;
	uint32_t total_frag_len = 0, frag_len = 0;
	bool is_frag, is_first_msdu;
	bool drop_mpdu = false;
	uint8_t bm_action = HAL_BM_ACTION_PUT_IN_IDLE_LIST;
	uint64_t nbuf_paddr = 0;
	uint32_t rx_link_buf_info[HAL_RX_BUFFINFO_NUM_DWORDS];

	if (qdf_unlikely(!dp_pdev)) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			  "pdev is null for mac_id = %d", mac_id);
		return rx_bufs_used;
	}

	msdu = 0;

	last = NULL;

	hal_rx_reo_ent_buf_paddr_get(rxdma_dst_ring_desc, &buf_info, &msdu_cnt);

	if ((hal_rx_reo_ent_rxdma_push_reason_get(rxdma_dst_ring_desc) ==
		HAL_RX_WBM_RXDMA_PSH_RSN_ERROR)) {
		uint8_t rxdma_err =
			hal_rx_reo_ent_rxdma_error_code_get(
				rxdma_dst_ring_desc);
		if (qdf_unlikely((rxdma_err == HAL_RXDMA_ERR_FLUSH_REQUEST) ||
		   (rxdma_err == HAL_RXDMA_ERR_MPDU_LENGTH) ||
		   (rxdma_err == HAL_RXDMA_ERR_OVERFLOW) ||
		   (rxdma_err == HAL_RXDMA_ERR_FCS && dp_pdev->mcopy_mode))) {
			drop_mpdu = true;
			dp_pdev->rx_mon_stats.dest_mpdu_drop++;
		}
	}

	is_frag = false;
	is_first_msdu = true;

	do {
		/* WAR for duplicate link descriptors received from HW */
		if (qdf_unlikely(dp_pdev->mon_last_linkdesc_paddr ==
		    buf_info.paddr)) {
			dp_pdev->rx_mon_stats.dup_mon_linkdesc_cnt++;
			return rx_bufs_used;
		}

		rx_msdu_link_desc =
			dp_rx_cookie_2_mon_link_desc(dp_pdev,
						     buf_info, mac_id);

		qdf_assert_always(rx_msdu_link_desc);

		hal_rx_msdu_list_get(soc->hal_soc, rx_msdu_link_desc,
				     &msdu_list, &num_msdus);

		for (i = 0; i < num_msdus; i++) {
			uint32_t l2_hdr_offset;
			struct dp_rx_desc *rx_desc = NULL;
			struct rx_desc_pool *rx_desc_pool;

			rx_desc = dp_rx_get_mon_desc(soc,
						     msdu_list.sw_cookie[i]);

			qdf_assert_always(rx_desc);
			msdu = rx_desc->nbuf;

			if (msdu)
				nbuf_paddr = qdf_nbuf_get_frag_paddr(msdu, 0);
			/* WAR for duplicate buffers received from HW */
			if (qdf_unlikely(dp_pdev->mon_last_buf_cookie ==
				msdu_list.sw_cookie[i] ||
				!msdu ||
				msdu_list.paddr[i] != nbuf_paddr ||
				!rx_desc->in_use)) {
				/* Skip duplicate buffer and drop subsequent
				 * buffers in this MPDU
				 */
				drop_mpdu = true;
				dp_pdev->rx_mon_stats.dup_mon_buf_cnt++;
				dp_pdev->mon_last_linkdesc_paddr =
					buf_info.paddr;
				continue;
			}

			if (rx_desc->unmapped == 0) {
				rx_desc_pool = dp_rx_get_mon_desc_pool(
							soc,
							mac_id,
							dp_pdev->pdev_id);
				qdf_nbuf_unmap_nbytes_single(
							soc->osdev,
							rx_desc->nbuf,
							QDF_DMA_FROM_DEVICE,
							rx_desc_pool->buf_size);
				rx_desc->unmapped = 1;
			}

			if (drop_mpdu) {
				dp_pdev->mon_last_linkdesc_paddr =
					buf_info.paddr;
				qdf_nbuf_free(msdu);
				msdu = NULL;
				goto next_msdu;
			}

			data = qdf_nbuf_data(msdu);

			rx_desc_tlv = HAL_RX_MON_DEST_GET_DESC(data);

			QDF_TRACE(QDF_MODULE_ID_DP,
				QDF_TRACE_LEVEL_DEBUG,
				"[%s] i=%d, ppdu_id=%x, num_msdus = %u",
				__func__, i, *ppdu_id, num_msdus);

			if (is_first_msdu) {
				if (!hal_rx_mpdu_start_tlv_tag_valid(
						soc->hal_soc,
						rx_desc_tlv)) {
					drop_mpdu = true;
					qdf_nbuf_free(msdu);
					msdu = NULL;
					dp_pdev->mon_last_linkdesc_paddr =
						buf_info.paddr;
					goto next_msdu;
				}

				msdu_ppdu_id = hal_rx_hw_desc_get_ppduid_get(
						soc->hal_soc,
						rx_desc_tlv,
						rxdma_dst_ring_desc);
				is_first_msdu = false;

				QDF_TRACE(QDF_MODULE_ID_DP,
					QDF_TRACE_LEVEL_DEBUG,
					"[%s] msdu_ppdu_id=%x",
					__func__, msdu_ppdu_id);

				if (*ppdu_id > msdu_ppdu_id)
					QDF_TRACE(QDF_MODULE_ID_DP,
						QDF_TRACE_LEVEL_DEBUG,
						"[%s][%d] ppdu_id=%d "
						"msdu_ppdu_id=%d",
						__func__, __LINE__, *ppdu_id,
						msdu_ppdu_id);

				if ((*ppdu_id < msdu_ppdu_id) && (
					(msdu_ppdu_id - *ppdu_id) <
						NOT_PPDU_ID_WRAP_AROUND)) {
					*ppdu_id = msdu_ppdu_id;
					return rx_bufs_used;
				} else if ((*ppdu_id > msdu_ppdu_id) && (
					(*ppdu_id - msdu_ppdu_id) >
						NOT_PPDU_ID_WRAP_AROUND)) {
					*ppdu_id = msdu_ppdu_id;
					return rx_bufs_used;
				}

				dp_tx_capture_get_user_id(dp_pdev,
							  rx_desc_tlv);

				if (*ppdu_id == msdu_ppdu_id)
					dp_pdev->rx_mon_stats.ppdu_id_match++;
				else
					dp_pdev->rx_mon_stats.ppdu_id_mismatch
						++;

				dp_pdev->mon_last_linkdesc_paddr =
					buf_info.paddr;
			}

			if (hal_rx_desc_is_first_msdu(soc->hal_soc,
						      rx_desc_tlv))
				hal_rx_mon_hw_desc_get_mpdu_status(soc->hal_soc,
					rx_desc_tlv,
					&(dp_pdev->ppdu_info.rx_status));


			if (msdu_list.msdu_info[i].msdu_flags &
				HAL_MSDU_F_MSDU_CONTINUATION) {
				if (!is_frag) {
					total_frag_len =
					msdu_list.msdu_info[i].msdu_len;
					is_frag = true;
				}
				dp_mon_adjust_frag_len(
					&total_frag_len, &frag_len);
			} else {
				if (is_frag) {
					dp_mon_adjust_frag_len(
						&total_frag_len, &frag_len);
				} else {
					frag_len =
					msdu_list.msdu_info[i].msdu_len;
				}
				is_frag = false;
				msdu_cnt--;
			}
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
				  "%s total_len %u frag_len %u flags %u",
				  __func__, total_frag_len, frag_len,
				  msdu_list.msdu_info[i].msdu_flags);

			rx_pkt_offset = SIZE_OF_MONITOR_TLV;
			/*
			 * HW structures call this L3 header padding
			 * -- even though this is actually the offset
			 * from the buffer beginning where the L2
			 * header begins.
			 */
			l2_hdr_offset =
			hal_rx_msdu_end_l3_hdr_padding_get(soc->hal_soc, data);

			rx_buf_size = rx_pkt_offset + l2_hdr_offset
					+ frag_len;

			qdf_nbuf_set_pktlen(msdu, rx_buf_size);
#if 0
			/* Disble it.see packet on msdu done set to 0 */
			/*
			 * Check if DMA completed -- msdu_done is the
			 * last bit to be written
			 */
			if (!hal_rx_attn_msdu_done_get(rx_desc_tlv)) {

				QDF_TRACE(QDF_MODULE_ID_DP,
					  QDF_TRACE_LEVEL_ERROR,
					  "%s:%d: Pkt Desc",
					  __func__, __LINE__);

				QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_DP,
					QDF_TRACE_LEVEL_ERROR,
					rx_desc_tlv, 128);

				qdf_assert_always(0);
			}
#endif
			QDF_TRACE(QDF_MODULE_ID_DP,
					  QDF_TRACE_LEVEL_DEBUG,
					  "%s: rx_pkt_offset=%d, l2_hdr_offset=%d, msdu_len=%d, addr=%pK skb->len %u",
					  __func__, rx_pkt_offset, l2_hdr_offset,
					  msdu_list.msdu_info[i].msdu_len,
					  qdf_nbuf_data(msdu),
					  (uint32_t)qdf_nbuf_len(msdu));

			if (head_msdu && !*head_msdu) {
				*head_msdu = msdu;
			} else {
				if (last)
					qdf_nbuf_set_next(last, msdu);
			}

			last = msdu;
next_msdu:
			dp_pdev->mon_last_buf_cookie = msdu_list.sw_cookie[i];
			rx_bufs_used++;
			dp_rx_add_to_free_desc_list(head,
				tail, rx_desc);
		}

		/*
		 * Store the current link buffer into to the local
		 * structure to be  used for release purpose.
		 */
		hal_rxdma_buff_addr_info_set(rx_link_buf_info, buf_info.paddr,
					     buf_info.sw_cookie, buf_info.rbm);

		hal_rx_mon_next_link_desc_get(rx_msdu_link_desc, &buf_info);
		if (dp_rx_monitor_link_desc_return(dp_pdev,
						   (hal_buff_addrinfo_t)
						   rx_link_buf_info,
						   mac_id,
						   bm_action)
						   != QDF_STATUS_SUCCESS)
			dp_err_rl("monitor link desc return failed");
	} while (buf_info.paddr && msdu_cnt);

	if (last)
		qdf_nbuf_set_next(last, NULL);

	*tail_msdu = msdu;

	return rx_bufs_used;

}

static inline
void dp_rx_msdus_set_payload(struct dp_soc *soc, qdf_nbuf_t msdu)
{
	uint8_t *data;
	uint32_t rx_pkt_offset, l2_hdr_offset;

	data = qdf_nbuf_data(msdu);
	rx_pkt_offset = SIZE_OF_MONITOR_TLV;
	l2_hdr_offset = hal_rx_msdu_end_l3_hdr_padding_get(soc->hal_soc, data);
	qdf_nbuf_pull_head(msdu, rx_pkt_offset + l2_hdr_offset);
}

static inline
qdf_nbuf_t dp_rx_mon_restitch_mpdu_from_msdus(struct dp_soc *soc,
	uint32_t mac_id, qdf_nbuf_t head_msdu, qdf_nbuf_t last_msdu,
	struct cdp_mon_status *rx_status)
{
	qdf_nbuf_t msdu, mpdu_buf, prev_buf, msdu_orig, head_frag_list;
	uint32_t decap_format, wifi_hdr_len, sec_hdr_len, msdu_llc_len,
		mpdu_buf_len, decap_hdr_pull_bytes, frag_list_sum_len, dir,
		is_amsdu, is_first_frag, amsdu_pad;
	void *rx_desc;
	char *hdr_desc;
	unsigned char *dest;
	struct ieee80211_frame *wh;
	struct ieee80211_qoscntl *qos;
	struct dp_pdev *dp_pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	head_frag_list = NULL;
	mpdu_buf = NULL;

	if (qdf_unlikely(!dp_pdev)) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			  "pdev is null for mac_id = %d", mac_id);
		return NULL;
	}

	/* The nbuf has been pulled just beyond the status and points to the
	   * payload
	*/
	if (!head_msdu)
		goto mpdu_stitch_fail;

	msdu_orig = head_msdu;

	rx_desc = qdf_nbuf_data(msdu_orig);

	if (HAL_RX_DESC_GET_MPDU_LENGTH_ERR(rx_desc)) {
		/* It looks like there is some issue on MPDU len err */
		/* Need further investigate if drop the packet */
		DP_STATS_INC(dp_pdev, dropped.mon_rx_drop, 1);
		return NULL;
	}

	rx_desc = qdf_nbuf_data(last_msdu);

	rx_status->cdp_rs_fcs_err = HAL_RX_DESC_GET_MPDU_FCS_ERR(rx_desc);
	dp_pdev->ppdu_info.rx_status.rs_fcs_err =
		HAL_RX_DESC_GET_MPDU_FCS_ERR(rx_desc);

	/* Fill out the rx_status from the PPDU start and end fields */
	/*   HAL_RX_GET_PPDU_STATUS(soc, mac_id, rx_status); */

	rx_desc = qdf_nbuf_data(head_msdu);

	decap_format = HAL_RX_DESC_GET_DECAP_FORMAT(rx_desc);

	/* Easy case - The MSDU status indicates that this is a non-decapped
	 * packet in RAW mode.
	*/
	if (decap_format == HAL_HW_RX_DECAP_FORMAT_RAW) {
		/* Note that this path might suffer from headroom unavailabilty
		 * - but the RX status is usually enough
		 */

		dp_rx_msdus_set_payload(soc, head_msdu);

		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
				  "[%s][%d] decap format raw head %pK head->next %pK last_msdu %pK last_msdu->next %pK",
				  __func__, __LINE__, head_msdu, head_msdu->next,
				  last_msdu, last_msdu->next);

		mpdu_buf = head_msdu;

		prev_buf = mpdu_buf;

		frag_list_sum_len = 0;
		msdu = qdf_nbuf_next(head_msdu);
		is_first_frag = 1;

		while (msdu) {

			dp_rx_msdus_set_payload(soc, msdu);

			if (is_first_frag) {
				is_first_frag = 0;
				head_frag_list  = msdu;
			}

			frag_list_sum_len += qdf_nbuf_len(msdu);

			/* Maintain the linking of the cloned MSDUS */
			qdf_nbuf_set_next_ext(prev_buf, msdu);

			/* Move to the next */
			prev_buf = msdu;
			msdu = qdf_nbuf_next(msdu);
		}

		qdf_nbuf_trim_tail(prev_buf, HAL_RX_FCS_LEN);

		/* If there were more fragments to this RAW frame */
		if (head_frag_list) {
			if (frag_list_sum_len <
				sizeof(struct ieee80211_frame_min_one)) {
				DP_STATS_INC(dp_pdev, dropped.mon_rx_drop, 1);
				return NULL;
			}
			frag_list_sum_len -= HAL_RX_FCS_LEN;
			qdf_nbuf_append_ext_list(mpdu_buf, head_frag_list,
				frag_list_sum_len);
			qdf_nbuf_set_next(mpdu_buf, NULL);
		}

		goto mpdu_stitch_done;
	}

	/* Decap mode:
	 * Calculate the amount of header in decapped packet to knock off based
	 * on the decap type and the corresponding number of raw bytes to copy
	 * status header
	 */
	rx_desc = qdf_nbuf_data(head_msdu);

	hdr_desc = HAL_RX_DESC_GET_80211_HDR(rx_desc);

	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
		  "[%s][%d] decap format not raw",
		  __func__, __LINE__);


	/* Base size */
	wifi_hdr_len = sizeof(struct ieee80211_frame);
	wh = (struct ieee80211_frame *)hdr_desc;

	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;

	if (dir == IEEE80211_FC1_DIR_DSTODS)
		wifi_hdr_len += 6;

	is_amsdu = 0;
	if (wh->i_fc[0] & QDF_IEEE80211_FC0_SUBTYPE_QOS) {
		qos = (struct ieee80211_qoscntl *)
			(hdr_desc + wifi_hdr_len);
		wifi_hdr_len += 2;

		is_amsdu = (qos->i_qos[0] & IEEE80211_QOS_AMSDU);
	}

	/*Calculate security header length based on 'Protected'
	 * and 'EXT_IV' flag
	 * */
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		char *iv = (char *)wh + wifi_hdr_len;

		if (iv[3] & KEY_EXTIV)
			sec_hdr_len = 8;
		else
			sec_hdr_len = 4;
	} else {
		sec_hdr_len = 0;
	}
	wifi_hdr_len += sec_hdr_len;

	/* MSDU related stuff LLC - AMSDU subframe header etc */
	msdu_llc_len = is_amsdu ? (14 + 8) : 8;

	mpdu_buf_len = wifi_hdr_len + msdu_llc_len;

	/* "Decap" header to remove from MSDU buffer */
	decap_hdr_pull_bytes = 14;

	/* Allocate a new nbuf for holding the 802.11 header retrieved from the
	 * status of the now decapped first msdu. Leave enough headroom for
	 * accomodating any radio-tap /prism like PHY header
	 */
	mpdu_buf = qdf_nbuf_alloc(soc->osdev,
			MAX_MONITOR_HEADER + mpdu_buf_len,
			MAX_MONITOR_HEADER, 4, FALSE);

	if (!mpdu_buf)
		goto mpdu_stitch_done;

	/* Copy the MPDU related header and enc headers into the first buffer
	 * - Note that there can be a 2 byte pad between heaader and enc header
	 */

	prev_buf = mpdu_buf;
	dest = qdf_nbuf_put_tail(prev_buf, wifi_hdr_len);
	if (!dest)
		goto mpdu_stitch_fail;

	qdf_mem_copy(dest, hdr_desc, wifi_hdr_len);
	hdr_desc += wifi_hdr_len;

#if 0
	dest = qdf_nbuf_put_tail(prev_buf, sec_hdr_len);
	adf_os_mem_copy(dest, hdr_desc, sec_hdr_len);
	hdr_desc += sec_hdr_len;
#endif

	/* The first LLC len is copied into the MPDU buffer */
	frag_list_sum_len = 0;

	msdu_orig = head_msdu;
	is_first_frag = 1;
	amsdu_pad = 0;

	while (msdu_orig) {

		/* TODO: intra AMSDU padding - do we need it ??? */

		msdu = msdu_orig;

		if (is_first_frag) {
			head_frag_list  = msdu;
		} else {
			/* Reload the hdr ptr only on non-first MSDUs */
			rx_desc = qdf_nbuf_data(msdu_orig);
			hdr_desc = HAL_RX_DESC_GET_80211_HDR(rx_desc);
		}

		/* Copy this buffers MSDU related status into the prev buffer */

		if (is_first_frag) {
			is_first_frag = 0;
		}

		/* Update protocol and flow tag for MSDU */
		dp_rx_mon_update_protocol_flow_tag(soc, dp_pdev,
						   msdu_orig, rx_desc);

		dest = qdf_nbuf_put_tail(prev_buf,
				msdu_llc_len + amsdu_pad);

		if (!dest)
			goto mpdu_stitch_fail;

		dest += amsdu_pad;
		qdf_mem_copy(dest, hdr_desc, msdu_llc_len);

		dp_rx_msdus_set_payload(soc, msdu);

		/* Push the MSDU buffer beyond the decap header */
		qdf_nbuf_pull_head(msdu, decap_hdr_pull_bytes);
		frag_list_sum_len += msdu_llc_len + qdf_nbuf_len(msdu)
			+ amsdu_pad;

		/* Set up intra-AMSDU pad to be added to start of next buffer -
		 * AMSDU pad is 4 byte pad on AMSDU subframe */
		amsdu_pad = (msdu_llc_len + qdf_nbuf_len(msdu)) & 0x3;
		amsdu_pad = amsdu_pad ? (4 - amsdu_pad) : 0;

		/* TODO FIXME How do we handle MSDUs that have fraglist - Should
		 * probably iterate all the frags cloning them along the way and
		 * and also updating the prev_buf pointer
		 */

		/* Move to the next */
		prev_buf = msdu;
		msdu_orig = qdf_nbuf_next(msdu_orig);

	}

#if 0
	/* Add in the trailer section - encryption trailer + FCS */
	qdf_nbuf_put_tail(prev_buf, HAL_RX_FCS_LEN);
	frag_list_sum_len += HAL_RX_FCS_LEN;
#endif

	frag_list_sum_len -= msdu_llc_len;

	/* TODO: Convert this to suitable adf routines */
	qdf_nbuf_append_ext_list(mpdu_buf, head_frag_list,
			frag_list_sum_len);

	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
		  "%s %d mpdu_buf %pK mpdu_buf->len %u",
		  __func__, __LINE__,
		  mpdu_buf, mpdu_buf->len);

mpdu_stitch_done:
	/* Check if this buffer contains the PPDU end status for TSF */
	/* Need revist this code to see where we can get tsf timestamp */
#if 0
	/* PPDU end TLV will be retrieved from monitor status ring */
	last_mpdu =
		(*(((u_int32_t *)&rx_desc->attention)) &
		RX_ATTENTION_0_LAST_MPDU_MASK) >>
		RX_ATTENTION_0_LAST_MPDU_LSB;

	if (last_mpdu)
		rx_status->rs_tstamp.tsf = rx_desc->ppdu_end.tsf_timestamp;

#endif
	return mpdu_buf;

mpdu_stitch_fail:
	if ((mpdu_buf) && (decap_format != HAL_HW_RX_DECAP_FORMAT_RAW)) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
			  "%s mpdu_stitch_fail mpdu_buf %pK",
			  __func__, mpdu_buf);
		/* Free the head buffer */
		qdf_nbuf_free(mpdu_buf);
	}
	return NULL;
}

/**
 * dp_send_mgmt_packet_to_stack(): send indicataion to upper layers
 *
 * @soc: soc handle
 * @nbuf: Mgmt packet
 * @pdev: pdev handle
 *
 * Return: QDF_STATUS_SUCCESS on success
 *         QDF_STATUS_E_INVAL in error
 */
#ifdef FEATURE_PERPKT_INFO
static inline QDF_STATUS dp_send_mgmt_packet_to_stack(struct dp_soc *soc,
						      qdf_nbuf_t nbuf,
						      struct dp_pdev *pdev)
{
	uint32_t *nbuf_data;
	struct ieee80211_frame *wh;

	if (!nbuf)
		return QDF_STATUS_E_INVAL;

	/*check if this is not a mgmt packet*/
	wh = (struct ieee80211_frame *)qdf_nbuf_data(nbuf);
	if (((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) !=
	     IEEE80211_FC0_TYPE_MGT) &&
	     ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) !=
	     IEEE80211_FC0_TYPE_CTL)) {
		qdf_nbuf_free(nbuf);
		return QDF_STATUS_E_INVAL;
	}
	nbuf_data = (uint32_t *)qdf_nbuf_push_head(nbuf, 4);
	if (!nbuf_data) {
		QDF_TRACE(QDF_MODULE_ID_DP,
			  QDF_TRACE_LEVEL_ERROR,
			  FL("No headroom"));
		qdf_nbuf_free(nbuf);
		return QDF_STATUS_E_INVAL;
	}
	*nbuf_data = pdev->ppdu_info.com_info.ppdu_id;

	dp_wdi_event_handler(WDI_EVENT_RX_MGMT_CTRL, soc, nbuf,
			     HTT_INVALID_PEER,
			     WDI_NO_VAL, pdev->pdev_id);
	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS dp_send_mgmt_packet_to_stack(struct dp_soc *soc,
						      qdf_nbuf_t nbuf,
						      struct dp_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dp_rx_extract_radiotap_info(): Extract and populate information in
 *				struct mon_rx_status type
 * @rx_status: Receive status
 * @mon_rx_status: Monitor mode status
 *
 * Returns: None
 */
static inline
void dp_rx_extract_radiotap_info(struct cdp_mon_status *rx_status,
				struct mon_rx_status *rx_mon_status)
{
	rx_mon_status->tsft = rx_status->cdp_rs_tstamp.cdp_tsf;
	rx_mon_status->chan_freq = rx_status->rs_freq;
	rx_mon_status->chan_num = rx_status->rs_channel;
	rx_mon_status->chan_flags = rx_status->rs_flags;
	rx_mon_status->rate = rx_status->rs_datarate;
	/* TODO: rx_mon_status->ant_signal_db */
	/* TODO: rx_mon_status->nr_ant */
	rx_mon_status->mcs = rx_status->cdf_rs_rate_mcs;
	rx_mon_status->is_stbc = rx_status->cdp_rs_stbc;
	rx_mon_status->sgi = rx_status->cdp_rs_sgi;
	/* TODO: rx_mon_status->ldpc */
	/* TODO: rx_mon_status->beamformed */
	/* TODO: rx_mon_status->vht_flags */
	/* TODO: rx_mon_status->vht_flag_values1 */
}

/*
 * dp_rx_mon_deliver(): function to deliver packets to stack
 * @soc: DP soc
 * @mac_id: MAC ID
 * @head_msdu: head of msdu list
 * @tail_msdu: tail of msdu list
 *
 * Return: status: 0 - Success, non-zero: Failure
 */
QDF_STATUS dp_rx_mon_deliver(struct dp_soc *soc, uint32_t mac_id,
	qdf_nbuf_t head_msdu, qdf_nbuf_t tail_msdu)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	struct cdp_mon_status *rs = &pdev->rx_mon_recv_status;
	qdf_nbuf_t mon_skb, skb_next;
	qdf_nbuf_t mon_mpdu = NULL;

	if (!pdev || (!pdev->monitor_vdev && !pdev->mcopy_mode))
		goto mon_deliver_fail;

	/* restitch mon MPDU for delivery via monitor interface */
	mon_mpdu = dp_rx_mon_restitch_mpdu_from_msdus(soc, mac_id, head_msdu,
				tail_msdu, rs);

	/* monitor vap cannot be present when mcopy is enabled
	 * hence same skb can be consumed
	 */
	if (pdev->mcopy_mode)
		return dp_send_mgmt_packet_to_stack(soc, mon_mpdu, pdev);

	if (mon_mpdu && pdev->monitor_vdev && pdev->monitor_vdev->osif_vdev &&
	    pdev->monitor_vdev->osif_rx_mon) {
		pdev->ppdu_info.rx_status.ppdu_id =
			pdev->ppdu_info.com_info.ppdu_id;
		pdev->ppdu_info.rx_status.device_id = soc->device_id;
		pdev->ppdu_info.rx_status.chan_noise_floor =
			pdev->chan_noise_floor;

		dp_handle_tx_capture(soc, pdev, mon_mpdu);

		if (!qdf_nbuf_update_radiotap(&pdev->ppdu_info.rx_status,
					      mon_mpdu,
					      qdf_nbuf_headroom(mon_mpdu))) {
			DP_STATS_INC(pdev, dropped.mon_radiotap_update_err, 1);
			goto mon_deliver_fail;
		}
		pdev->monitor_vdev->osif_rx_mon(pdev->monitor_vdev->osif_vdev,
						mon_mpdu,
						&pdev->ppdu_info.rx_status);
	} else {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			  "[%s][%d] mon_mpdu=%pK monitor_vdev %pK osif_vdev %pK"
			  , __func__, __LINE__, mon_mpdu, pdev->monitor_vdev,
			  (pdev->monitor_vdev ? pdev->monitor_vdev->osif_vdev
			   : NULL));
		goto mon_deliver_fail;
	}

	return QDF_STATUS_SUCCESS;

mon_deliver_fail:
	mon_skb = head_msdu;
	while (mon_skb) {
		skb_next = qdf_nbuf_next(mon_skb);

		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
				  "[%s][%d] mon_skb=%pK len %u", __func__,
				  __LINE__, mon_skb, mon_skb->len);

		qdf_nbuf_free(mon_skb);
		mon_skb = skb_next;
	}
	return QDF_STATUS_E_INVAL;
}

/**
* dp_rx_mon_deliver_non_std()
* @soc: core txrx main contex
* @mac_id: MAC ID
*
* This function delivers the radio tap and dummy MSDU
* into user layer application for preamble only PPDU.
*
* Return: QDF_STATUS
*/
QDF_STATUS dp_rx_mon_deliver_non_std(struct dp_soc *soc,
				     uint32_t mac_id)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	ol_txrx_rx_mon_fp osif_rx_mon;
	qdf_nbuf_t dummy_msdu;

	/* Sanity checking */
	if (!pdev || !pdev->monitor_vdev || !pdev->monitor_vdev->osif_rx_mon)
		goto mon_deliver_non_std_fail;

	/* Generate a dummy skb_buff */
	osif_rx_mon = pdev->monitor_vdev->osif_rx_mon;
	dummy_msdu = qdf_nbuf_alloc(soc->osdev, MAX_MONITOR_HEADER,
				    MAX_MONITOR_HEADER, 4, FALSE);
	if (!dummy_msdu)
		goto allocate_dummy_msdu_fail;

	qdf_nbuf_set_pktlen(dummy_msdu, 0);
	qdf_nbuf_set_next(dummy_msdu, NULL);

	pdev->ppdu_info.rx_status.ppdu_id =
		pdev->ppdu_info.com_info.ppdu_id;

	/* Apply the radio header to this dummy skb */
	if (!qdf_nbuf_update_radiotap(&pdev->ppdu_info.rx_status, dummy_msdu,
				      qdf_nbuf_headroom(dummy_msdu))) {
		DP_STATS_INC(pdev, dropped.mon_radiotap_update_err, 1);
		qdf_nbuf_free(dummy_msdu);
		goto mon_deliver_non_std_fail;
	}

	/* deliver to the user layer application */
	osif_rx_mon(pdev->monitor_vdev->osif_vdev,
		    dummy_msdu, NULL);

	/* Clear rx_status*/
	qdf_mem_zero(&pdev->ppdu_info.rx_status,
		     sizeof(pdev->ppdu_info.rx_status));
	pdev->mon_ppdu_status = DP_PPDU_STATUS_START;

	return QDF_STATUS_SUCCESS;

allocate_dummy_msdu_fail:
	QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_DP, "[%s][%d] mon_skb=%pK ",
			   __func__, __LINE__, dummy_msdu);

mon_deliver_non_std_fail:
	return QDF_STATUS_E_INVAL;
}

/**
* dp_rx_mon_dest_process() - Brain of the Rx processing functionality
*	Called from the bottom half (tasklet/NET_RX_SOFTIRQ)
* @soc: core txrx main contex
* @hal_ring: opaque pointer to the HAL Rx Ring, which will be serviced
* @quota: No. of units (packets) that can be serviced in one shot.
*
* This function implements the core of Rx functionality. This is
* expected to handle only non-error frames.
*
* Return: none
*/
void dp_rx_mon_dest_process(struct dp_soc *soc, uint32_t mac_id, uint32_t quota)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	uint8_t pdev_id;
	hal_rxdma_desc_t rxdma_dst_ring_desc;
	hal_soc_handle_t hal_soc;
	void *mon_dst_srng;
	union dp_rx_desc_list_elem_t *head = NULL;
	union dp_rx_desc_list_elem_t *tail = NULL;
	uint32_t ppdu_id;
	uint32_t rx_bufs_used;
	uint32_t mpdu_rx_bufs_used;
	int mac_for_pdev = mac_id;
	struct cdp_pdev_mon_stats *rx_mon_stats;

	if (!pdev) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			  "pdev is null for mac_id = %d", mac_id);
		return;
	}

	mon_dst_srng = dp_rxdma_get_mon_dst_ring(pdev, mac_for_pdev);

	if (!mon_dst_srng || !hal_srng_initialized(mon_dst_srng)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"%s %d : HAL Monitor Destination Ring Init Failed -- %pK",
			__func__, __LINE__, mon_dst_srng);
		return;
	}

	hal_soc = soc->hal_soc;

	qdf_assert((hal_soc && pdev));

	qdf_spin_lock_bh(&pdev->mon_lock);

	if (qdf_unlikely(hal_srng_access_start(hal_soc, mon_dst_srng))) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"%s %d : HAL Monitor Destination Ring access Failed -- %pK",
			__func__, __LINE__, mon_dst_srng);
		return;
	}

	pdev_id = pdev->pdev_id;
	ppdu_id = pdev->ppdu_info.com_info.ppdu_id;
	rx_bufs_used = 0;
	rx_mon_stats = &pdev->rx_mon_stats;

	while (qdf_likely(rxdma_dst_ring_desc =
		hal_srng_dst_peek(hal_soc, mon_dst_srng))) {
		qdf_nbuf_t head_msdu, tail_msdu;
		uint32_t npackets;
		head_msdu = (qdf_nbuf_t) NULL;
		tail_msdu = (qdf_nbuf_t) NULL;

		mpdu_rx_bufs_used =
			dp_rx_mon_mpdu_pop(soc, mac_id,
					   rxdma_dst_ring_desc,
					   &head_msdu, &tail_msdu,
					   &npackets, &ppdu_id,
					   &head, &tail);

		rx_bufs_used += mpdu_rx_bufs_used;

		if (mpdu_rx_bufs_used)
			pdev->mon_dest_ring_stuck_cnt = 0;
		else
			pdev->mon_dest_ring_stuck_cnt++;

		if (pdev->mon_dest_ring_stuck_cnt >
		    MON_DEST_RING_STUCK_MAX_CNT) {
			dp_info("destination ring stuck");
			dp_info("ppdu_id status=%d dest=%d",
				pdev->ppdu_info.com_info.ppdu_id, ppdu_id);
			rx_mon_stats->mon_rx_dest_stuck++;
			pdev->ppdu_info.com_info.ppdu_id = ppdu_id;
			continue;
		}

		if (ppdu_id != pdev->ppdu_info.com_info.ppdu_id) {
			rx_mon_stats->stat_ring_ppdu_id_hist[
				rx_mon_stats->ppdu_id_hist_idx] =
				pdev->ppdu_info.com_info.ppdu_id;
			rx_mon_stats->dest_ring_ppdu_id_hist[
				rx_mon_stats->ppdu_id_hist_idx] = ppdu_id;
			rx_mon_stats->ppdu_id_hist_idx =
				(rx_mon_stats->ppdu_id_hist_idx + 1) &
					(MAX_PPDU_ID_HIST - 1);
			pdev->mon_ppdu_status = DP_PPDU_STATUS_START;
			qdf_mem_zero(&(pdev->ppdu_info.rx_status),
				sizeof(pdev->ppdu_info.rx_status));
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
				  "%s %d ppdu_id %x != ppdu_info.com_info.ppdu_id %x",
				  __func__, __LINE__,
				  ppdu_id, pdev->ppdu_info.com_info.ppdu_id);
			break;
		}

		if (qdf_likely((head_msdu) && (tail_msdu))) {
			rx_mon_stats->dest_mpdu_done++;
			dp_rx_mon_deliver(soc, mac_id, head_msdu, tail_msdu);
		}

		rxdma_dst_ring_desc = hal_srng_dst_get_next(hal_soc,
			mon_dst_srng);
	}
	hal_srng_access_end(hal_soc, mon_dst_srng);

	qdf_spin_unlock_bh(&pdev->mon_lock);

	if (rx_bufs_used) {
		rx_mon_stats->dest_ppdu_done++;
		dp_rx_buffers_replenish(soc, mac_id,
					dp_rxdma_get_mon_buf_ring(pdev,
								  mac_for_pdev),
					dp_rx_get_mon_desc_pool(soc, mac_id,
								pdev_id),
					rx_bufs_used, &head, &tail);
	}
}

QDF_STATUS
dp_rx_pdev_mon_buf_buffers_alloc(struct dp_pdev *pdev, uint32_t mac_id,
				 bool delayed_replenish)
{
	uint8_t pdev_id = pdev->pdev_id;
	struct dp_soc *soc = pdev->soc;
	struct dp_srng *mon_buf_ring;
	uint32_t num_entries;
	struct rx_desc_pool *rx_desc_pool;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;

	mon_buf_ring = dp_rxdma_get_mon_buf_ring(pdev, mac_id);

	num_entries = mon_buf_ring->num_entries;

	rx_desc_pool = dp_rx_get_mon_desc_pool(soc, mac_id, pdev_id);

	dp_debug("Mon RX Desc Pool[%d] entries=%u", pdev_id, num_entries);

	/* Replenish RXDMA monitor buffer ring with 8 buffers only
	 * delayed_replenish_entries is actually 8 but when we call
	 * dp_pdev_rx_buffers_attach() we pass 1 less than 8, hence
	 * added 1 to delayed_replenish_entries to ensure we have 8
	 * entries. Once the monitor VAP is configured we replenish
	 * the complete RXDMA monitor buffer ring.
	 */
	if (delayed_replenish) {
		num_entries = soc_cfg_ctx->delayed_replenish_entries + 1;
		status = dp_pdev_rx_buffers_attach(soc, mac_id, mon_buf_ring,
						   rx_desc_pool,
						   num_entries - 1);
	} else {
		union dp_rx_desc_list_elem_t *tail = NULL;
		union dp_rx_desc_list_elem_t *desc_list = NULL;

		status = dp_rx_buffers_replenish(soc, mac_id,
						 mon_buf_ring,
						 rx_desc_pool,
						 num_entries,
						 &desc_list,
						 &tail);
	}

	return status;
}

static QDF_STATUS
dp_rx_pdev_mon_cmn_buffers_alloc(struct dp_pdev *pdev, int mac_id)
{
	struct dp_soc *soc = pdev->soc;
	uint8_t pdev_id = pdev->pdev_id;
	int mac_for_pdev;
	bool delayed_replenish;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;

	delayed_replenish = soc_cfg_ctx->delayed_replenish_entries ? 1 : 0;
	mac_for_pdev = dp_get_lmac_id_for_pdev_id(pdev->soc, mac_id, pdev_id);
	status = dp_rx_pdev_mon_status_buffers_alloc(pdev, mac_for_pdev);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		dp_err("%s: dp_rx_pdev_mon_status_desc_pool_alloc() failed",
		       __func__);
		goto fail;
	}

	if (!soc->wlan_cfg_ctx->rxdma1_enable)
		return status;

	status = dp_rx_pdev_mon_buf_buffers_alloc(pdev, mac_for_pdev,
						  delayed_replenish);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		dp_err("%s: dp_rx_pdev_mon_buf_desc_pool_alloc() failed\n",
		       __func__);
		goto mon_stat_buf_dealloc;
	}

	return status;

mon_stat_buf_dealloc:
	dp_rx_pdev_mon_status_buffers_free(pdev, mac_for_pdev);
fail:
	return status;
}

static void
dp_rx_pdev_mon_buf_desc_pool_init(struct dp_pdev *pdev, uint32_t mac_id)
{
	uint8_t pdev_id = pdev->pdev_id;
	struct dp_soc *soc = pdev->soc;
	struct dp_srng *mon_buf_ring;
	uint32_t num_entries;
	struct rx_desc_pool *rx_desc_pool;
	uint32_t rx_desc_pool_size;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;

	mon_buf_ring = &soc->rxdma_mon_buf_ring[mac_id];

	num_entries = mon_buf_ring->num_entries;

	rx_desc_pool = &soc->rx_desc_mon[mac_id];

	dp_debug("Mon RX Desc buf Pool[%d] init entries=%u",
		 pdev_id, num_entries);

	rx_desc_pool_size = wlan_cfg_get_dp_soc_rx_sw_desc_weight(soc_cfg_ctx) *
		num_entries;

	rx_desc_pool->owner = HAL_RX_BUF_RBM_SW3_BM;
	rx_desc_pool->buf_size = RX_MONITOR_BUFFER_SIZE;
	rx_desc_pool->buf_alignment = RX_MONITOR_BUFFER_ALIGNMENT;

	dp_rx_desc_pool_init(soc, mac_id, rx_desc_pool_size, rx_desc_pool);

	pdev->mon_last_linkdesc_paddr = 0;

	pdev->mon_last_buf_cookie = DP_RX_DESC_COOKIE_MAX + 1;

	/* Attach full monitor mode resources */
	dp_full_mon_attach(pdev);
}

static void
dp_rx_pdev_mon_cmn_desc_pool_init(struct dp_pdev *pdev, int mac_id)
{
	struct dp_soc *soc = pdev->soc;
	uint32_t mac_for_pdev;

	mac_for_pdev = dp_get_lmac_id_for_pdev_id(soc, mac_id, pdev->pdev_id);
	dp_rx_pdev_mon_status_desc_pool_init(pdev, mac_for_pdev);

	if (!soc->wlan_cfg_ctx->rxdma1_enable)
		return;

	dp_rx_pdev_mon_buf_desc_pool_init(pdev, mac_for_pdev);
	dp_link_desc_ring_replenish(soc, mac_for_pdev);
}

static void
dp_rx_pdev_mon_buf_desc_pool_deinit(struct dp_pdev *pdev, uint32_t mac_id)
{
	uint8_t pdev_id = pdev->pdev_id;
	struct dp_soc *soc = pdev->soc;
	struct rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &soc->rx_desc_mon[mac_id];

	dp_debug("Mon RX Desc buf Pool[%d] deinit", pdev_id);

	dp_rx_desc_pool_deinit(soc, rx_desc_pool);

	/* Detach full monitor mode resources */
	dp_full_mon_detach(pdev);
}

static void
dp_rx_pdev_mon_cmn_desc_pool_deinit(struct dp_pdev *pdev, int mac_id)
{
	struct dp_soc *soc = pdev->soc;
	uint8_t pdev_id = pdev->pdev_id;
	int mac_for_pdev = dp_get_lmac_id_for_pdev_id(soc, mac_id, pdev_id);

	dp_rx_pdev_mon_status_desc_pool_deinit(pdev, mac_for_pdev);

	if (!soc->wlan_cfg_ctx->rxdma1_enable)
		return;

	dp_rx_pdev_mon_buf_desc_pool_deinit(pdev, mac_for_pdev);
}

static void
dp_rx_pdev_mon_buf_desc_pool_free(struct dp_pdev *pdev, uint32_t mac_id)
{
	uint8_t pdev_id = pdev->pdev_id;
	struct dp_soc *soc = pdev->soc;
	struct rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &soc->rx_desc_mon[mac_id];

	dp_debug("Mon RX Buf Desc Pool Free pdev[%d]", pdev_id);

	dp_rx_desc_pool_free(soc, rx_desc_pool);
}

static void
dp_rx_pdev_mon_cmn_desc_pool_free(struct dp_pdev *pdev, int mac_id)
{
	struct dp_soc *soc = pdev->soc;
	uint8_t pdev_id = pdev->pdev_id;
	int mac_for_pdev = dp_get_lmac_id_for_pdev_id(soc, mac_id, pdev_id);

	dp_rx_pdev_mon_status_desc_pool_free(pdev, mac_for_pdev);
	dp_rx_pdev_mon_buf_desc_pool_free(pdev, mac_for_pdev);
	dp_hw_link_desc_pool_banks_free(soc, mac_for_pdev);
}

void dp_rx_pdev_mon_buf_buffers_free(struct dp_pdev *pdev, uint32_t mac_id)
{
	uint8_t pdev_id = pdev->pdev_id;
	struct dp_soc *soc = pdev->soc;
	struct rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &soc->rx_desc_mon[mac_id];

	dp_debug("Mon RX Buf buffers Free pdev[%d]", pdev_id);

	dp_rx_desc_nbuf_free(soc, rx_desc_pool);
}

static QDF_STATUS
dp_rx_pdev_mon_buf_desc_pool_alloc(struct dp_pdev *pdev, uint32_t mac_id)
{
	uint8_t pdev_id = pdev->pdev_id;
	struct dp_soc *soc = pdev->soc;
	struct dp_srng *mon_buf_ring;
	uint32_t num_entries;
	struct rx_desc_pool *rx_desc_pool;
	uint32_t rx_desc_pool_size;
	struct wlan_cfg_dp_soc_ctxt *soc_cfg_ctx = soc->wlan_cfg_ctx;

	mon_buf_ring = &soc->rxdma_mon_buf_ring[mac_id];

	num_entries = mon_buf_ring->num_entries;

	rx_desc_pool = &soc->rx_desc_mon[mac_id];

	dp_debug("Mon RX Desc Pool[%d] entries=%u",
		 pdev_id, num_entries);

	rx_desc_pool_size = wlan_cfg_get_dp_soc_rx_sw_desc_weight(soc_cfg_ctx) *
		num_entries;

	return dp_rx_desc_pool_alloc(soc, rx_desc_pool_size, rx_desc_pool);
}

static QDF_STATUS
dp_rx_pdev_mon_cmn_desc_pool_alloc(struct dp_pdev *pdev, int mac_id)
{
	struct dp_soc *soc = pdev->soc;
	uint8_t pdev_id = pdev->pdev_id;
	uint32_t mac_for_pdev;
	QDF_STATUS status;

	mac_for_pdev = dp_get_lmac_id_for_pdev_id(soc, mac_id, pdev_id);

	/* Allocate sw rx descriptor pool for monitor status ring */
	status = dp_rx_pdev_mon_status_desc_pool_alloc(pdev, mac_for_pdev);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		dp_err("%s: dp_rx_pdev_mon_status_desc_pool_alloc() failed",
		       __func__);
		goto fail;
	}

	if (!soc->wlan_cfg_ctx->rxdma1_enable)
		return status;

	/* Allocate sw rx descriptor pool for monitor RxDMA buffer ring */
	status = dp_rx_pdev_mon_buf_desc_pool_alloc(pdev, mac_for_pdev);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		dp_err("%s: dp_rx_pdev_mon_buf_desc_pool_alloc() failed\n",
		       __func__);
		goto mon_status_dealloc;
	}

	/* Allocate link descriptors for the monitor link descriptor ring */
	status = dp_hw_link_desc_pool_banks_alloc(soc, mac_for_pdev);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		dp_err("%s: dp_hw_link_desc_pool_banks_alloc() failed",
		       __func__);
		goto mon_buf_dealloc;
	}
	return status;

mon_buf_dealloc:
	dp_rx_pdev_mon_buf_desc_pool_free(pdev, mac_for_pdev);
mon_status_dealloc:
	dp_rx_pdev_mon_status_desc_pool_free(pdev, mac_for_pdev);
fail:
	return status;
}

static void
dp_rx_pdev_mon_cmn_buffers_free(struct dp_pdev *pdev, int mac_id)
{
	uint8_t pdev_id = pdev->pdev_id;
	struct dp_soc *soc = pdev->soc;
	int mac_for_pdev;

	mac_for_pdev = dp_get_lmac_id_for_pdev_id(pdev->soc, mac_id, pdev_id);
	dp_rx_pdev_mon_status_buffers_free(pdev, mac_for_pdev);

	if (!soc->wlan_cfg_ctx->rxdma1_enable)
		return;

	dp_rx_pdev_mon_buf_buffers_free(pdev, mac_for_pdev);
}

QDF_STATUS
dp_rx_pdev_mon_desc_pool_alloc(struct dp_pdev *pdev)
{
	QDF_STATUS status;
	int mac_id, count;

	for (mac_id = 0; mac_id < NUM_RXDMA_RINGS_PER_PDEV; mac_id++) {
		status = dp_rx_pdev_mon_cmn_desc_pool_alloc(pdev, mac_id);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			QDF_TRACE(QDF_MODULE_ID_DP,
				  QDF_TRACE_LEVEL_ERROR, "%s: %d failed\n",
				  __func__, mac_id);

			for (count = 0; count < mac_id; count++)
				dp_rx_pdev_mon_cmn_desc_pool_free(pdev, count);

			return status;
		}
	}
	return status;
}

void
dp_rx_pdev_mon_desc_pool_init(struct dp_pdev *pdev)
{
	int mac_id;
	for (mac_id = 0; mac_id < NUM_RXDMA_RINGS_PER_PDEV; mac_id++)
		dp_rx_pdev_mon_cmn_desc_pool_init(pdev, mac_id);
	qdf_spinlock_create(&pdev->mon_lock);
}

void
dp_rx_pdev_mon_desc_pool_deinit(struct dp_pdev *pdev)
{
	int mac_id;

	for (mac_id = 0; mac_id < NUM_RXDMA_RINGS_PER_PDEV; mac_id++)
		dp_rx_pdev_mon_cmn_desc_pool_deinit(pdev, mac_id);
	qdf_spinlock_destroy(&pdev->mon_lock);
}

void dp_rx_pdev_mon_desc_pool_free(struct dp_pdev *pdev)
{
	int mac_id;

	for (mac_id = 0; mac_id < NUM_RXDMA_RINGS_PER_PDEV; mac_id++)
		dp_rx_pdev_mon_cmn_desc_pool_free(pdev, mac_id);
}

void
dp_rx_pdev_mon_buffers_free(struct dp_pdev *pdev)
{
	int mac_id;

	for (mac_id = 0; mac_id < NUM_RXDMA_RINGS_PER_PDEV; mac_id++)
		dp_rx_pdev_mon_cmn_buffers_free(pdev, mac_id);
}

QDF_STATUS
dp_rx_pdev_mon_buffers_alloc(struct dp_pdev *pdev)
{
	int mac_id;
	QDF_STATUS status;

	for (mac_id = 0; mac_id < NUM_RXDMA_RINGS_PER_PDEV; mac_id++) {
		status = dp_rx_pdev_mon_cmn_buffers_alloc(pdev, mac_id);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			QDF_TRACE(QDF_MODULE_ID_DP,
				  QDF_TRACE_LEVEL_ERROR, "%s: %d failed\n",
				  __func__, mac_id);
			return status;
		}
	}
	return status;
}

