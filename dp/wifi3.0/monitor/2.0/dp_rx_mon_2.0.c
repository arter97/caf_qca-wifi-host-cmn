/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "hal_be_hw_headers.h"
#include "dp_types.h"
#include "hal_be_rx.h"
#include "hal_api.h"
#include "qdf_trace.h"
#include "hal_be_api_mon.h"
#include "dp_internal.h"
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include "dp_mon.h"
#include <dp_mon_2.0.h>
#include <dp_rx_mon_2.0.h>
#include <dp_be.h>
#include <hal_be_api_mon.h>

static inline uint32_t
dp_rx_mon_srng_process_2_0(struct dp_soc *soc, struct dp_intr *int_ctx,
			   uint32_t mac_id, uint32_t quota)
{
	struct dp_pdev *pdev = dp_get_pdev_for_lmac_id(soc, mac_id);
	void *rx_mon_dst_ring_desc;
	hal_soc_handle_t hal_soc;
	void *mon_dst_srng;
	struct dp_mon_pdev *mon_pdev;
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mon_soc_be *monitor_soc = be_soc->monitor_soc_be;
	uint32_t work_done = 0;

	if (!pdev) {
		dp_mon_err("%pK: pdev is null for mac_id = %d", soc, mac_id);
		return work_done;
	}

	mon_pdev = pdev->monitor_pdev;
	mon_dst_srng = soc->rxdma_mon_dst_ring[mac_id].hal_srng;

	if (!mon_dst_srng || !hal_srng_initialized(mon_dst_srng)) {
		dp_mon_err("%pK: : HAL Monitor Destination Ring Init Failed -- %pK",
			   soc, mon_dst_srng);
		return work_done;
	}

	hal_soc = soc->hal_soc;

	qdf_assert((hal_soc && pdev));

	qdf_spin_lock_bh(&mon_pdev->mon_lock);

	if (qdf_unlikely(dp_srng_access_start(int_ctx, soc, mon_dst_srng))) {
		dp_mon_err("%s %d : HAL Mon Dest Ring access Failed -- %pK",
			   __func__, __LINE__, mon_dst_srng);
		qdf_spin_unlock_bh(&mon_pdev->mon_lock);
		return work_done;
	}

	while (qdf_likely((rx_mon_dst_ring_desc =
			  (void *)hal_srng_dst_get_next(hal_soc, mon_dst_srng))
				&& quota--)) {
		struct hal_mon_desc hal_mon_rx_desc;
		struct dp_mon_desc *mon_desc;
		struct dp_mon_desc_pool *rx_desc_pool;

		rx_desc_pool = &monitor_soc->rx_desc_mon;
		hal_mon_buf_get(soc->hal_soc,
				rx_mon_dst_ring_desc,
				&hal_mon_rx_desc);
		mon_desc = (struct dp_mon_desc *)(uintptr_t)(hal_mon_rx_desc.buf_addr);
		qdf_assert_always(mon_desc);

		if (!mon_desc->unmapped) {
			qdf_mem_unmap_page(soc->osdev, mon_desc->paddr,
					   QDF_DMA_FROM_DEVICE,
					   rx_desc_pool->buf_size);
			mon_desc->unmapped = 1;
		}

		dp_rx_mon_process_status_tlv(soc, pdev,
					     &hal_mon_rx_desc,
					     mon_desc->paddr);

		qdf_frag_free(mon_desc->buf_addr);
		work_done++;
	}
	dp_srng_access_end(int_ctx, soc, mon_dst_srng);

	qdf_spin_unlock_bh(&mon_pdev->mon_lock);
	dp_mon_info("mac_id: %d, work_done:%d", mac_id, work_done);
	return work_done;
}

uint32_t
dp_rx_mon_process_2_0(struct dp_soc *soc, struct dp_intr *int_ctx,
		      uint32_t mac_id, uint32_t quota)
{
	uint32_t work_done;

	work_done = dp_rx_mon_srng_process_2_0(soc, int_ctx, mac_id, quota);

	return work_done;
}

void
dp_rx_mon_buf_desc_pool_deinit(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mon_soc_be *mon_soc = be_soc->monitor_soc_be;

	dp_mon_desc_pool_deinit(&mon_soc->rx_desc_mon);
}

QDF_STATUS
dp_rx_mon_buf_desc_pool_init(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mon_soc_be *mon_soc = be_soc->monitor_soc_be;
	QDF_STATUS status;

	status = dp_mon_desc_pool_init(&mon_soc->rx_desc_mon);
	if (status != QDF_STATUS_SUCCESS) {
		dp_mon_err("Failed to init rx monior descriptor pool");
		mon_soc->rx_mon_ring_fill_level = 0;
	} else {
		mon_soc->rx_mon_ring_fill_level =
					DP_MON_RING_FILL_LEVEL_DEFAULT;
	}

	return status;
}

void dp_rx_mon_buf_desc_pool_free(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mon_soc_be *mon_soc = be_soc->monitor_soc_be;

	if (mon_soc)
		dp_mon_desc_pool_free(&mon_soc->rx_desc_mon);
}

QDF_STATUS
dp_rx_mon_buf_desc_pool_alloc(struct dp_soc *soc)
{
	struct dp_srng *mon_buf_ring;
	struct dp_mon_desc_pool *rx_mon_desc_pool;
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mon_soc_be *mon_soc = be_soc->monitor_soc_be;

	mon_buf_ring = &soc->rxdma_mon_buf_ring[0];

	rx_mon_desc_pool = &mon_soc->rx_desc_mon;

	return dp_mon_desc_pool_alloc(mon_soc->rx_mon_ring_fill_level,
				      rx_mon_desc_pool);
}

void
dp_rx_mon_buffers_free(struct dp_soc *soc)
{
	struct dp_mon_desc_pool *rx_mon_desc_pool;
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mon_soc_be *mon_soc = be_soc->monitor_soc_be;

	rx_mon_desc_pool = &mon_soc->rx_desc_mon;

	dp_mon_pool_frag_unmap_and_free(soc, rx_mon_desc_pool);
}

QDF_STATUS
dp_rx_mon_buffers_alloc(struct dp_soc *soc)
{
	struct dp_srng *mon_buf_ring;
	struct dp_mon_desc_pool *rx_mon_desc_pool;
	union dp_mon_desc_list_elem_t *desc_list = NULL;
	union dp_mon_desc_list_elem_t *tail = NULL;
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mon_soc_be *mon_soc = be_soc->monitor_soc_be;

	mon_buf_ring = &soc->rxdma_mon_buf_ring[0];

	rx_mon_desc_pool = &mon_soc->rx_desc_mon;

	return dp_mon_buffers_replenish(soc, mon_buf_ring,
					rx_mon_desc_pool,
					mon_soc->rx_mon_ring_fill_level,
					&desc_list, &tail);
}

#ifdef QCA_ENHANCED_STATS_SUPPORT
void
dp_mon_populate_ppdu_usr_info_2_0(struct mon_rx_user_status *rx_user_status,
				  struct cdp_rx_stats_ppdu_user *ppdu_user)
{
	ppdu_user->mpdu_retries = rx_user_status->retry_mpdu;
}

#ifdef WLAN_FEATURE_11BE
void dp_mon_rx_stats_update_2_0(struct dp_peer *peer,
				struct cdp_rx_indication_ppdu *ppdu,
				struct cdp_rx_stats_ppdu_user *ppdu_user)
{
	uint8_t mcs, preamble, ppdu_type;

	preamble = ppdu->u.preamble;
	ppdu_type = ppdu->u.ppdu_type;
	if (ppdu_type == HAL_RX_TYPE_SU)
		mcs = ppdu->u.mcs;
	else
		mcs = ppdu_user->mcs;

	DP_STATS_INC(peer, rx.mpdu_retry_cnt, ppdu_user->mpdu_retries);
	DP_STATS_INCC(peer,
		      rx.su_be_ppdu_cnt.mcs_count[MAX_MCS - 1], 1,
		      ((mcs >= (MAX_MCS - 1)) && (preamble == DOT11_BE) &&
		      (ppdu_type == HAL_RX_TYPE_SU)));
	DP_STATS_INCC(peer,
		      rx.su_be_ppdu_cnt.mcs_count[mcs], 1,
		      ((mcs < (MAX_MCS - 1)) && (preamble == DOT11_BE) &&
		      (ppdu_type == HAL_RX_TYPE_SU)));
	DP_STATS_INCC(peer,
		      rx.rx_mu_be[TXRX_TYPE_MU_OFDMA].ppdu.mcs_count[MAX_MCS - 1],
		      1, ((mcs >= (MAX_MCS - 1)) &&
		      (preamble == DOT11_BE) &&
		      (ppdu_type == HAL_RX_TYPE_MU_OFDMA)));
	DP_STATS_INCC(peer,
		      rx.rx_mu_be[TXRX_TYPE_MU_OFDMA].ppdu.mcs_count[mcs],
		      1, ((mcs < (MAX_MCS - 1)) &&
		      (preamble == DOT11_BE) &&
		      (ppdu_type == HAL_RX_TYPE_MU_OFDMA)));
	DP_STATS_INCC(peer,
		      rx.rx_mu_be[TXRX_TYPE_MU_MIMO].ppdu.mcs_count[MAX_MCS - 1],
		      1, ((mcs >= (MAX_MCS - 1)) &&
		      (preamble == DOT11_BE) &&
		      (ppdu_type == HAL_RX_TYPE_MU_MIMO)));
	DP_STATS_INCC(peer,
		      rx.rx_mu_be[TXRX_TYPE_MU_MIMO].ppdu.mcs_count[mc],
		      1, ((mcs < (MAX_MCS - 1)) &&
		      (preamble == DOT11_BE) &&
		      (ppdu_type == HAL_RX_TYPE_MU_MIMO)));
}

void
dp_mon_populate_ppdu_info_2_0(struct hal_rx_ppdu_info *hal_ppdu_info,
			      struct cdp_rx_indication_ppdu *ppdu)
{
	ppdu->punc_bw = hal_ppdu_info->rx_status.punctured_bw;
}
#else
void dp_mon_rx_stats_update_2_0(struct dp_peer *peer,
				struct cdp_rx_indication_ppdu *ppdu,
				struct cdp_rx_stats_ppdu_user *ppdu_user)
{
	DP_STATS_INC(peer, rx.mpdu_retry_cnt, ppdu_user->mpdu_retries);
}

void
dp_mon_populate_ppdu_info_2_0(struct hal_rx_ppdu_info *hal_ppdu_info,
			      struct cdp_rx_indication_ppdu *ppdu)
{
	ppdu->punc_bw = 0;
}
#endif
#endif
