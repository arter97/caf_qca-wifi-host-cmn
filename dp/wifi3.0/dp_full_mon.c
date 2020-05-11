/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#include "dp_types.h"
#include "hal_rx.h"
#include "hal_api.h"
#include "qdf_trace.h"
#include "qdf_nbuf.h"
#include "hal_api_mon.h"
#include "dp_rx.h"
#include "dp_rx_mon.h"
#include "dp_internal.h"
#include "dp_htt.h"
#include "dp_full_mon.h"
#include "qdf_mem.h"

#ifdef QCA_SUPPORT_FULL_MON

uint32_t
dp_rx_mon_status_process(struct dp_soc *soc,
			 uint32_t mac_id,
			 uint32_t quota);

/*
 * dp_rx_mon_status_buf_validate () - Validate first monitor status buffer addr
 * against status buf addr given in monitor destination ring
 *
 * @pdev: DP pdev handle
 * @mac_id: lmac id
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_rx_mon_status_buf_validate(struct dp_pdev *pdev, uint32_t mac_id)
{
	struct dp_soc *soc = pdev->soc;
	hal_soc_handle_t hal_soc;
	void *mon_status_srng;
	void *ring_entry;
	uint32_t rx_buf_cookie;
	qdf_nbuf_t status_nbuf;
	struct dp_rx_desc *rx_desc;
	uint64_t buf_paddr;
	struct rx_desc_pool *rx_desc_pool;
	uint32_t tlv_tag;
	void *rx_tlv;
	struct hal_rx_ppdu_info *ppdu_info;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	QDF_STATUS buf_status;

	mon_status_srng = soc->rxdma_mon_status_ring[mac_id].hal_srng;

	qdf_assert(mon_status_srng);
	if (!mon_status_srng || !hal_srng_initialized(mon_status_srng)) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			  "%s %d : HAL Monitor Status Ring Init Failed -- %pK",
			  __func__, __LINE__, mon_status_srng);
		return status;
	}

	hal_soc = soc->hal_soc;

	qdf_assert(hal_soc);

	if (qdf_unlikely(hal_srng_access_start(hal_soc, mon_status_srng))) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			  "%s %d : HAL SRNG access Failed -- %pK",
			  __func__, __LINE__, mon_status_srng);
		return status;
	}

	ring_entry = hal_srng_src_peek_n_get_next(hal_soc, mon_status_srng);
	if (!ring_entry) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			"%s %d : HAL SRNG entry is NULL srng:-- %pK",
			__func__, __LINE__, mon_status_srng);
		goto done;
	}

	ppdu_info = &pdev->ppdu_info;

	rx_desc_pool = &soc->rx_desc_status[mac_id];

	buf_paddr = (HAL_RX_BUFFER_ADDR_31_0_GET(ring_entry) |
		     ((uint64_t)(HAL_RX_BUFFER_ADDR_39_32_GET(ring_entry)) << 32));

	if (!buf_paddr) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			  "%s %d : buf addr is NULL -- %pK",
			  __func__, __LINE__, mon_status_srng);
		goto done;
	}

	rx_buf_cookie = HAL_RX_BUF_COOKIE_GET(ring_entry);
	rx_desc = dp_rx_cookie_2_va_mon_status(soc, rx_buf_cookie);

	qdf_assert(rx_desc);

	status_nbuf = rx_desc->nbuf;

	qdf_nbuf_sync_for_cpu(soc->osdev, status_nbuf,
			      QDF_DMA_FROM_DEVICE);

	rx_tlv = qdf_nbuf_data(status_nbuf);

	buf_status = hal_get_rx_status_done(rx_tlv);

	/* If status buffer DMA is not done,
	 * hold on to mon destination ring.
	 */
	if (buf_status != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			  FL("Monitor status ring: DMA is not done "
			     "for nbuf: %pK buf_addr: %llx"),
			     status_nbuf, buf_paddr);
		pdev->rx_mon_stats.tlv_tag_status_err++;
		pdev->hold_mon_dest_ring = true;
		goto done;
	}

	qdf_nbuf_unmap_nbytes_single(soc->osdev, status_nbuf,
			QDF_DMA_FROM_DEVICE,
			rx_desc_pool->buf_size);

	rx_tlv = hal_rx_status_get_next_tlv(rx_tlv);

	tlv_tag = HAL_RX_GET_USER_TLV32_TYPE(rx_tlv);

	if (tlv_tag == WIFIRX_PPDU_START_E) {
		rx_tlv = (uint8_t *)rx_tlv + HAL_RX_TLV32_HDR_SIZE;
		ppdu_info->com_info.ppdu_id = HAL_RX_GET(rx_tlv,
							 RX_PPDU_START_0,
							 PHY_PPDU_ID);
		pdev->status_buf_addr = buf_paddr;
	}

	/* If Monitor destination ring is on hold and ppdu id matches,
	 * deliver PPDU data which was on hold.
	 */
	if (pdev->hold_mon_dest_ring &&
	    (pdev->mon_desc->ppdu_id == pdev->ppdu_info.com_info.ppdu_id)) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			  FL("Monitor destination was on Hold "
			     "PPDU id matched"));

		pdev->hold_mon_dest_ring = false;
		goto done;
	}

	if ((pdev->mon_desc->status_buf.paddr != buf_paddr) ||
	     (pdev->mon_desc->ppdu_id != pdev->ppdu_info.com_info.ppdu_id)) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			  FL("Monitor: PPDU id or status buf_addr mismatch "
			     "status_ppdu_id: %d dest_ppdu_id: %d "
			     "status_addr: %llx status_buf_cookie: %d "
			     "dest_addr: %llx tlv_tag: %d"
			     " status_nbuf: %pK"),
			      pdev->ppdu_info.com_info.ppdu_id,
			      pdev->mon_desc->ppdu_id, pdev->status_buf_addr,
			      rx_buf_cookie,
			      pdev->mon_desc->status_buf.paddr, tlv_tag,
			      status_nbuf);
	}

	/* Monitor Status ring is reaped in two cases:
	 * a. If first status buffer's buf_addr_info matches
	 * with latched status buffer addr info in monitor
	 * destination ring.
	 * b. If monitor status ring is lagging behind
	 * monitor destination ring. Hold on to monitor
	 * destination ring in this case until status ring
	 * and destination ring ppdu id matches.
	 */
	if ((pdev->mon_desc->status_buf.paddr == buf_paddr) ||
	    (pdev->mon_desc->ppdu_id > pdev->ppdu_info.com_info.ppdu_id)) {
		if (pdev->mon_desc->ppdu_id >
		    pdev->ppdu_info.com_info.ppdu_id) {

			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
				  FL("Monitor status ring is lagging behind "
				     "monitor destination ring "
				     "status_ppdu_id: %d dest_ppdu_id: %d "
				     "status_nbuf: %pK tlv_tag: %d "
				     "status_addr: %llx dest_addr: %llx "),
				     ppdu_info->com_info.ppdu_id,
				     pdev->mon_desc->ppdu_id,
				     status_nbuf, tlv_tag,
				     pdev->status_buf_addr,
				     pdev->mon_desc->status_buf.paddr);

			pdev->rx_mon_stats.ppdu_id_mismatch++;
			pdev->rx_mon_stats.status_ppdu_drop++;
			pdev->hold_mon_dest_ring = true;
		}
		status = QDF_STATUS_SUCCESS;
	}
done:
	hal_srng_access_end(hal_soc, mon_status_srng);
	return status;
}

/*
 * dp_rx_mon_prepare_mon_mpdu () - API to prepare dp_mon_mpdu object
 *
 * @pdev: DP pdev object
 * @head_msdu: Head msdu
 * @tail_msdu: Tail msdu
 *
 */
static inline struct dp_mon_mpdu *
dp_rx_mon_prepare_mon_mpdu(struct dp_pdev *pdev,
			   qdf_nbuf_t head_msdu,
			   qdf_nbuf_t tail_msdu)
{
	struct dp_mon_mpdu *mon_mpdu = NULL;

	mon_mpdu = qdf_mem_malloc(sizeof(struct dp_mon_mpdu));

	if (!mon_mpdu) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
			  FL("Monitor MPDU object allocation failed -- %pK"),
			     pdev);
		qdf_assert_always(0);
	}

	mon_mpdu->head = head_msdu;
	mon_mpdu->tail = tail_msdu;
	mon_mpdu->rs_flags = pdev->ppdu_info.rx_status.rs_flags;
	mon_mpdu->ant_signal_db = pdev->ppdu_info.rx_status.ant_signal_db;
	mon_mpdu->is_stbc = pdev->ppdu_info.rx_status.is_stbc;
	mon_mpdu->sgi = pdev->ppdu_info.rx_status.sgi;
	mon_mpdu->beamformed = pdev->ppdu_info.rx_status.beamformed;

	return mon_mpdu;
}

static inline void
dp_rx_mon_drop_ppdu(struct dp_pdev *pdev, uint32_t mac_id)
{
	struct dp_mon_mpdu *mpdu = NULL;
	struct dp_mon_mpdu *temp_mpdu = NULL;
	qdf_nbuf_t mon_skb, skb_next;

	if (!TAILQ_EMPTY(&pdev->mon_mpdu_q)) {
		TAILQ_FOREACH_SAFE(mpdu,
				   &pdev->mon_mpdu_q,
				   mpdu_list_elem,
				   temp_mpdu) {
				   TAILQ_REMOVE(&pdev->mon_mpdu_q,
						mpdu, mpdu_list_elem);

			mon_skb = mpdu->head;
			while (mon_skb) {
				skb_next = qdf_nbuf_next(mon_skb);

				QDF_TRACE(QDF_MODULE_ID_DP,
					  QDF_TRACE_LEVEL_DEBUG,
					  "[%s][%d] mon_skb=%pK len %u"
					  " __func__, __LINE__",
					  mon_skb, mon_skb->len);

				qdf_nbuf_free(mon_skb);
				mon_skb = skb_next;
			}

			qdf_mem_free(mpdu);
		}
	}
}

/*
 * dp_rx_monitor_deliver_ppdu () - API to deliver all MPDU for a MPDU
 * to upper layer stack
 *
 * @soc: DP soc handle
 * @mac_id: lmac id
 */
static inline QDF_STATUS
dp_rx_monitor_deliver_ppdu(struct dp_soc *soc, uint32_t mac_id)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	struct dp_mon_mpdu *mpdu = NULL;
	struct dp_mon_mpdu *temp_mpdu = NULL;

	if (!TAILQ_EMPTY(&pdev->mon_mpdu_q)) {
		TAILQ_FOREACH_SAFE(mpdu,
				   &pdev->mon_mpdu_q,
				   mpdu_list_elem,
				   temp_mpdu) {
			TAILQ_REMOVE(&pdev->mon_mpdu_q,
				     mpdu, mpdu_list_elem);

			pdev->ppdu_info.rx_status.rs_flags = mpdu->rs_flags;
			pdev->ppdu_info.rx_status.ant_signal_db =
				mpdu->ant_signal_db;
			pdev->ppdu_info.rx_status.is_stbc = mpdu->is_stbc;
			pdev->ppdu_info.rx_status.sgi = mpdu->sgi;
			pdev->ppdu_info.rx_status.beamformed = mpdu->beamformed;

			dp_rx_mon_deliver(soc, mac_id,
					  mpdu->head, mpdu->tail);

			qdf_mem_free(mpdu);
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_mon_reap_status_ring () - Reap status_buf_count of status buffers for
 * status ring.
 *
 * @soc: DP soc handle
 * @mac_id: mac id on which interrupt is received
 * @quota: number of status ring entries to be reaped
 * @desc_info: Rx ppdu desc info
 */
static inline uint32_t
dp_rx_mon_reap_status_ring(struct dp_soc *soc,
			   uint32_t mac_id,
			   uint32_t quota,
			   struct hal_rx_mon_desc_info *desc_info)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	uint8_t status_buf_count;
	uint32_t work_done = 0;

	status_buf_count = desc_info->status_buf_count;
	desc_info->drop_ppdu = false;

	if (dp_rx_mon_status_buf_validate(pdev, mac_id) == QDF_STATUS_SUCCESS)
		work_done = dp_rx_mon_status_process(soc,
						     mac_id,
						     status_buf_count);

	if (desc_info->ppdu_id != pdev->ppdu_info.com_info.ppdu_id) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			  FL("Monitor: PPDU id mismatch "
			     "status_ppdu_id: %d dest_ppdu_id: %d "
			     "status_addr: %llx dest_addr: %llx "
			     "count: %d quota: %d work_done: %d "),
			      pdev->ppdu_info.com_info.ppdu_id,
			      pdev->mon_desc->ppdu_id, pdev->status_buf_addr,
			      pdev->mon_desc->status_buf.paddr,
			       status_buf_count, quota, work_done);
	}

	if (desc_info->ppdu_id < pdev->ppdu_info.com_info.ppdu_id) {
		pdev->rx_mon_stats.ppdu_id_mismatch++;
		desc_info->drop_ppdu = true;
		pdev->rx_mon_stats.dest_ppdu_drop++;

		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			  FL("Monitor destination ring is lagging behind "
			     "monitor status ring "
			     "status_ppdu_id: %d dest_ppdu_id: %d "
			     "status_addr: %llx dest_addr: %llx "),
			     pdev->ppdu_info.com_info.ppdu_id,
			     pdev->mon_desc->ppdu_id,
			     pdev->status_buf_addr,
			     pdev->mon_desc->status_buf.paddr);
	}

	return work_done;
}

/**
 * dp_rx_mon_mpdu_reap () - This API reaps a mpdu from mon dest ring descriptor
 * and returns link descriptor to HW (WBM)
 *
 * @soc: DP soc handle
 * @mac_id: lmac id
 * @ring_desc: SW monitor ring desc
 * @head_msdu: nbuf pointing to first msdu in a chain
 * @tail_msdu: nbuf pointing to last msdu in a chain
 * @head_desc: head pointer to free desc list
 * @tail_desc: tail pointer to free desc list
 *
 * Return: number of reaped buffers
 */
static inline uint32_t
dp_rx_mon_mpdu_reap(struct dp_soc *soc, uint32_t mac_id, void *ring_desc,
		    qdf_nbuf_t *head_msdu, qdf_nbuf_t *tail_msdu,
		    union dp_rx_desc_list_elem_t **head_desc,
		    union dp_rx_desc_list_elem_t **tail_desc)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	struct dp_rx_desc *rx_desc = NULL;
	struct hal_rx_msdu_list msdu_list;
	uint32_t rx_buf_reaped = 0;
	uint16_t num_msdus = 0, msdu_index, rx_hdr_tlv_len, l3_hdr_pad;
	uint32_t total_frag_len = 0, frag_len = 0;
	bool drop_mpdu = false;
	bool msdu_frag = false;
	void *link_desc_va;
	uint8_t *rx_tlv_hdr;
	qdf_nbuf_t msdu = NULL, last_msdu = NULL;
	uint32_t rx_link_buf_info[HAL_RX_BUFFINFO_NUM_DWORDS];
	struct hal_rx_mon_desc_info *desc_info;

	desc_info = pdev->mon_desc;

	qdf_mem_zero(desc_info, sizeof(struct hal_rx_mon_desc_info));

	/* Read SW Mon ring descriptor */
	hal_rx_sw_mon_desc_info_get((struct hal_soc *)soc->hal_soc,
				    ring_desc,
				    (void *)desc_info);

	/* If end_of_ppdu is 1, return*/
	if (desc_info->end_of_ppdu)
		return rx_buf_reaped;

	/* If there is rxdma error, drop mpdu */
	if (qdf_unlikely(dp_rx_mon_is_rxdma_error(desc_info)
			== QDF_STATUS_SUCCESS)) {
		drop_mpdu = true;
		pdev->rx_mon_stats.dest_mpdu_drop++;
	}

	/*
	 * while loop iterates through all link descriptors and
	 * reaps msdu_count number of msdus for one SW_MONITOR_RING descriptor
	 * and forms nbuf queue.
	 */
	while (desc_info->msdu_count && desc_info->link_desc.paddr) {
		link_desc_va = dp_rx_cookie_2_mon_link_desc(pdev,
							    desc_info->link_desc,
							    mac_id);

		qdf_assert_always(link_desc_va);

		hal_rx_msdu_list_get(soc->hal_soc,
				     link_desc_va,
				     &msdu_list,
				     &num_msdus);

		for (msdu_index = 0; msdu_index < num_msdus; msdu_index++) {
			rx_desc = dp_rx_get_mon_desc(soc,
						     msdu_list.sw_cookie[msdu_index]);

			qdf_assert_always(rx_desc);

			msdu = rx_desc->nbuf;

			if (rx_desc->unmapped == 0) {
				qdf_nbuf_unmap_single(soc->osdev,
						      msdu,
						      QDF_DMA_FROM_DEVICE);
				rx_desc->unmapped = 1;
			}

			if (drop_mpdu) {
				qdf_nbuf_free(msdu);
				msdu = NULL;
				desc_info->msdu_count--;
				goto next_msdu;
			}

			rx_tlv_hdr = qdf_nbuf_data(msdu);

			if (hal_rx_desc_is_first_msdu(soc->hal_soc,
						      rx_tlv_hdr))
				hal_rx_mon_hw_desc_get_mpdu_status(soc->hal_soc,
								   rx_tlv_hdr,
								   &pdev->ppdu_info.rx_status);

			/** If msdu is fragmented, spread across multiple
			 *  buffers
			 *   a. calculate len of each fragmented buffer
			 *   b. calculate the number of fragmented buffers for
			 *      a msdu and decrement one msdu_count
			 */
			if (msdu_list.msdu_info[msdu_index].msdu_flags
			    & HAL_MSDU_F_MSDU_CONTINUATION) {
				if (!msdu_frag) {
					total_frag_len = msdu_list.msdu_info[msdu_index].msdu_len;
					msdu_frag = true;
				}
				dp_mon_adjust_frag_len(&total_frag_len,
						       &frag_len);
			} else {
				if (msdu_frag)
					dp_mon_adjust_frag_len(&total_frag_len,
							       &frag_len);
				else
					frag_len = msdu_list.msdu_info[msdu_index].msdu_len;
				msdu_frag = false;
				desc_info->msdu_count--;
			}

			rx_hdr_tlv_len = SIZE_OF_MONITOR_TLV;

			/*
			 * HW structures call this L3 header padding.
			 * this is actually the offset
			 * from the buffer beginning where the L2
			 * header begins.
			 */

			l3_hdr_pad = hal_rx_msdu_end_l3_hdr_padding_get(
								soc->hal_soc,
								rx_tlv_hdr);

			/*******************************************************
			 *                    RX_PACKET                        *
			 * ----------------------------------------------------*
			 |   RX_PKT_TLVS  |   L3 Padding header  |  msdu data| |
			 * ----------------------------------------------------*
			 ******************************************************/

			qdf_nbuf_set_pktlen(msdu,
					    rx_hdr_tlv_len +
					    l3_hdr_pad +
					    frag_len);

			if (head_msdu && !*head_msdu)
				*head_msdu = msdu;
			else if (last_msdu)
				qdf_nbuf_set_next(last_msdu, msdu);

			last_msdu = msdu;

next_msdu:
			rx_buf_reaped++;
			dp_rx_add_to_free_desc_list(head_desc,
						    tail_desc,
						    rx_desc);

			QDF_TRACE(QDF_MODULE_ID_DP,
				  QDF_TRACE_LEVEL_DEBUG,
				  FL("%s total_len %u frag_len %u flags %u"),
				  total_frag_len, frag_len,
				  msdu_list.msdu_info[msdu_index].msdu_flags);
		}

		hal_rxdma_buff_addr_info_set(rx_link_buf_info,
					     desc_info->link_desc.paddr,
					     desc_info->link_desc.sw_cookie,
					     desc_info->link_desc.rbm);

		/* Get next link desc VA from current link desc */
		hal_rx_mon_next_link_desc_get(link_desc_va,
					      &desc_info->link_desc);

		/* return msdu link descriptor to WBM */
		if (dp_rx_monitor_link_desc_return(pdev,
						   (hal_buff_addrinfo_t)rx_link_buf_info,
						   mac_id,
						   HAL_BM_ACTION_PUT_IN_IDLE_LIST)
				!= QDF_STATUS_SUCCESS) {
			QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
				  "dp_rx_monitor_link_desc_return failed");
		}
	}
	pdev->rx_mon_stats.dest_mpdu_done++;

	if (last_msdu)
		qdf_nbuf_set_next(last_msdu, NULL);

	*tail_msdu = msdu;

	return rx_buf_reaped;
}

/*
 * dp_rx_mon_deliver_prev_ppdu () - Deliver previous PPDU
 *
 * @pdev: DP pdev handle
 * @mac_id: lmac id
 * @quota: quota
 *
 * Return: remaining qouta
 */
static inline uint32_t
dp_rx_mon_deliver_prev_ppdu(struct dp_pdev *pdev,
			    uint32_t mac_id,
			    uint32_t quota)
{

	struct dp_soc *soc = pdev->soc;
	struct hal_rx_mon_desc_info *desc_info = pdev->mon_desc;
	uint32_t work_done = 0;
	bool hold_mon_dest_ring = false;

	while (pdev->hold_mon_dest_ring) {
		if (dp_rx_mon_status_buf_validate(pdev, mac_id) == QDF_STATUS_SUCCESS) {
			work_done = dp_rx_mon_status_process(soc, mac_id, 1);
		}
		quota -= work_done;

		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
			  FL("Hold on Monitor destination ring "
			      "work_done: %d quota: %d status_ppdu_id: %d "
			      "dest_ppdu_id: %d s_addr: %llx d_addr: %llx "),
			      work_done, quota,
			      pdev->ppdu_info.com_info.ppdu_id,
			      desc_info->ppdu_id, pdev->status_buf_addr,
			      desc_info->status_buf.paddr);

		hold_mon_dest_ring = true;

		if (!quota)
			return quota;
	}
	if (hold_mon_dest_ring) {
		if (quota >= desc_info->status_buf_count) {
			qdf_err("DEBUG:");
			work_done = dp_rx_mon_status_process(soc, mac_id,
							     desc_info->status_buf_count);
			dp_rx_monitor_deliver_ppdu(soc, mac_id);
			hold_mon_dest_ring = false;
		} else {
			pdev->hold_mon_dest_ring = true;
			return quota;
		}
		quota -= work_done;
	}

	return quota;
}

/**
 * dp_rx_mon_process () - Core brain processing for monitor mode
 *
 * This API processes monitor destination ring followed by monitor status ring
 * Called from bottom half (tasklet/NET_RX_SOFTIRQ)
 *
 * @soc: datapath soc context
 * @mac_id: mac_id on which interrupt is received
 * @quota: Number of status ring entry that can be serviced in one shot.
 *
 * @Return: Number of reaped status ring entries
 */
uint32_t dp_rx_mon_process(struct dp_soc *soc, uint32_t mac_id, uint32_t quota)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	union dp_rx_desc_list_elem_t *head_desc = NULL;
	union dp_rx_desc_list_elem_t *tail_desc = NULL;
	uint32_t rx_bufs_reaped = 0;
	struct dp_mon_mpdu *mon_mpdu;
	struct cdp_pdev_mon_stats *rx_mon_stats = &pdev->rx_mon_stats;
	hal_rxdma_desc_t ring_desc;
	hal_soc_handle_t hal_soc;
	hal_ring_handle_t mon_dest_srng;
	qdf_nbuf_t head_msdu = NULL;
	qdf_nbuf_t tail_msdu = NULL;
	struct hal_rx_mon_desc_info *desc_info;
	int mac_for_pdev = mac_id;
	QDF_STATUS status;
	uint32_t work_done = 0;

	qdf_spin_lock_bh(&pdev->mon_lock);
	if (qdf_unlikely(!dp_soc_is_full_mon_enable(pdev))) {
		quota -= dp_rx_mon_status_process(soc, mac_id, quota);
		qdf_spin_unlock_bh(&pdev->mon_lock);
		return quota;
	}

	desc_info = pdev->mon_desc;

	quota = dp_rx_mon_deliver_prev_ppdu(pdev, mac_id, quota);

	/* Do not proceed if quota expires */
	if (!quota || pdev->hold_mon_dest_ring) {
		qdf_spin_unlock_bh(&pdev->mon_lock);
		return quota;
	}

	mon_dest_srng = dp_rxdma_get_mon_dst_ring(pdev, mac_for_pdev);

	if (qdf_unlikely(!mon_dest_srng ||
			 !hal_srng_initialized(mon_dest_srng))) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
			  FL("HAL Monitor Destination Ring Init Failed -- %pK"),
			  mon_dest_srng);
		goto done1;
	}

	hal_soc = soc->hal_soc;

	qdf_assert_always(hal_soc && pdev);


	if (qdf_unlikely(hal_srng_access_start(hal_soc, mon_dest_srng))) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
			  FL("HAL Monitor Destination Ring access Failed -- %pK"),
			  mon_dest_srng);
		goto done1;
	}

	/* Each entry in mon dest ring carries mpdu data
	 * reap all msdus for a mpdu and form skb chain
	 */
	while (qdf_likely(ring_desc =
			  hal_srng_dst_peek(hal_soc, mon_dest_srng))) {
		head_msdu = NULL;
		tail_msdu = NULL;
		rx_bufs_reaped = dp_rx_mon_mpdu_reap(soc, mac_id,
						     ring_desc, &head_msdu,
						     &tail_msdu, &head_desc,
						     &tail_desc);

		/* Assert if end_of_ppdu is zero and number of reaped buffers
		 * are zero.
		 */
		if (qdf_unlikely(!desc_info->end_of_ppdu && !rx_bufs_reaped)) {
			qdf_err("end_of_ppdu and rx_bufs_reaped are zero");
		}

		rx_mon_stats->mon_rx_bufs_reaped_dest += rx_bufs_reaped;

		/* replenish rx_bufs_reaped buffers back to
		 * RxDMA Monitor buffer ring
		 */
		if (rx_bufs_reaped) {
			status = dp_rx_buffers_replenish(soc, mac_id,
							 dp_rxdma_get_mon_buf_ring(pdev,
										   mac_for_pdev),
							 dp_rx_get_mon_desc_pool(soc, mac_id,
										 pdev->pdev_id),
										 rx_bufs_reaped,
										 &head_desc, &tail_desc);
			if (status != QDF_STATUS_SUCCESS)
				qdf_assert_always(0);

			rx_mon_stats->mon_rx_bufs_replenished_dest += rx_bufs_reaped;
		}

		head_desc = NULL;
		tail_desc = NULL;

		/* If end_of_ppdu is zero, it is a valid data mpdu
		 *    a. Add head_msdu and tail_msdu to mpdu list
		 *    b. continue reaping next SW_MONITOR_RING descriptor
		 */

		if (!desc_info->end_of_ppdu) {
			/*
			 * In case of rxdma error, MPDU is dropped
			 * from sw_monitor_ring descriptor.
			 * in this case, head_msdu remains NULL.
			 * move srng to next and continue reaping next entry
			 */
			if (!head_msdu) {
				ring_desc = hal_srng_dst_get_next(hal_soc,
								  mon_dest_srng);

				continue;
			}

			/*
			 * Prepare a MPDU object which holds chain of msdus
			 * and MPDU specific status and add this is to
			 * monitor mpdu queue
			 */
			mon_mpdu = dp_rx_mon_prepare_mon_mpdu(pdev,
							      head_msdu,
							      tail_msdu);

			QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
				  FL("Dest_srng: %pK MPDU_OBJ: %pK "
				  "head_msdu: %pK tail_msdu: %pK -- "),
				  mon_dest_srng,
				  mon_mpdu,
				  head_msdu,
				  tail_msdu);

			TAILQ_INSERT_TAIL(&pdev->mon_mpdu_q,
					  mon_mpdu,
					  mpdu_list_elem);

			head_msdu = NULL;
			tail_msdu = NULL;
			ring_desc = hal_srng_dst_get_next(hal_soc,
							  mon_dest_srng);
			continue;
		}

		/*
		 * end_of_ppdu is one,
		 *  a. update ppdu_done stattistics
		 *  b. Replenish buffers back to mon buffer ring
		 *  c. reap status ring for a PPDU and deliver all mpdus
		 *     to upper layer
		 */
		rx_mon_stats->dest_ppdu_done++;

		work_done = dp_rx_mon_reap_status_ring(soc, mac_id,
							quota, desc_info);
		/* Deliver all MPDUs for a PPDU */
		if (desc_info->drop_ppdu)
			dp_rx_mon_drop_ppdu(pdev, mac_id);
		else if (!pdev->hold_mon_dest_ring)
			dp_rx_monitor_deliver_ppdu(soc, mac_id);

		hal_srng_dst_get_next(hal_soc, mon_dest_srng);
		quota -= work_done;
		break;
	}

	hal_srng_access_end(hal_soc, mon_dest_srng);

done1:
	qdf_spin_unlock_bh(&pdev->mon_lock);

	return quota;
}

/**
 * dp_full_mon_attach() - attach full monitor mode
 *              resources
 * @pdev: Datapath PDEV handle
 *
 * Return: void
 */
void dp_full_mon_attach(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;

	if (!soc->full_mon_mode) {
		qdf_debug("Full monitor is not enabled");
		return;
	}

	pdev->mon_desc = qdf_mem_malloc(sizeof(struct hal_rx_mon_desc_info));

	if (!pdev->mon_desc) {
		qdf_err("Memory allocation failed for hal_rx_mon_desc_info ");
		return;
	}
	TAILQ_INIT(&pdev->mon_mpdu_q);
}

/**
 * dp_full_mon_detach() - detach full monitor mode
 *              resources
 * @pdev: Datapath PDEV handle
 *
 * Return: void
 *
 */
void dp_full_mon_detach(struct dp_pdev *pdev)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_mon_mpdu *mpdu = NULL;
	struct dp_mon_mpdu *temp_mpdu = NULL;

	if (!soc->full_mon_mode) {
		qdf_debug("Full monitor is not enabled");
		return;
	}

	if (pdev->mon_desc)
		qdf_mem_free(pdev->mon_desc);

	if (!TAILQ_EMPTY(&pdev->mon_mpdu_q)) {
		TAILQ_FOREACH_SAFE(mpdu,
				   &pdev->mon_mpdu_q,
				   mpdu_list_elem,
				   temp_mpdu) {
			qdf_mem_free(mpdu);
		}
	}
}
#endif
