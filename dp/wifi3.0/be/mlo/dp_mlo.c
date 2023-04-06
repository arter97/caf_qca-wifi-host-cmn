/*
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <wlan_utility.h>
#include <dp_internal.h>
#include <dp_htt.h>
#include <hal_be_api.h>
#include "dp_mlo.h"
#include <dp_be.h>
#include <dp_be_rx.h>
#include <dp_htt.h>
#include <dp_internal.h>
#include <wlan_cfg.h>
#include <wlan_mlo_mgr_cmn.h>
#include "dp_umac_reset.h"

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * dp_umac_reset_update_partner_map() - Update Umac reset partner map
 * @mlo_ctx: mlo soc context
 * @chip_id: chip id
 * @set: flag indicating whether to set or clear the bit
 *
 * Return: void
 */
static void dp_umac_reset_update_partner_map(struct dp_mlo_ctxt *mlo_ctx,
					     int chip_id, bool set);
#endif
/**
 * dp_mlo_ctxt_attach_wifi3() - Attach DP MLO context
 * @ctrl_ctxt: CDP control context
 *
 * Return: DP MLO context handle on success, NULL on failure
 */
static struct cdp_mlo_ctxt *
dp_mlo_ctxt_attach_wifi3(struct cdp_ctrl_mlo_mgr *ctrl_ctxt)
{
	struct dp_mlo_ctxt *mlo_ctxt =
		qdf_mem_malloc(sizeof(struct dp_mlo_ctxt));

	if (!mlo_ctxt) {
		dp_err("Failed to allocate DP MLO Context");
		return NULL;
	}

	mlo_ctxt->ctrl_ctxt = ctrl_ctxt;

	if (dp_mlo_peer_find_hash_attach_be
			(mlo_ctxt, DP_MAX_MLO_PEER) != QDF_STATUS_SUCCESS) {
		dp_err("Failed to allocate peer hash");
		qdf_mem_free(mlo_ctxt);
		return NULL;
	}

	qdf_get_random_bytes(mlo_ctxt->toeplitz_hash_ipv4,
			     (sizeof(mlo_ctxt->toeplitz_hash_ipv4[0]) *
			      LRO_IPV4_SEED_ARR_SZ));
	qdf_get_random_bytes(mlo_ctxt->toeplitz_hash_ipv6,
			     (sizeof(mlo_ctxt->toeplitz_hash_ipv6[0]) *
			      LRO_IPV6_SEED_ARR_SZ));

	qdf_spinlock_create(&mlo_ctxt->ml_soc_list_lock);
	qdf_spinlock_create(&mlo_ctxt->grp_umac_reset_ctx.grp_ctx_lock);
	return dp_mlo_ctx_to_cdp(mlo_ctxt);
}

/**
 * dp_mlo_ctxt_detach_wifi3() - Detach DP MLO context
 * @cdp_ml_ctxt: pointer to CDP DP MLO context
 *
 * Return: void
 */
static void dp_mlo_ctxt_detach_wifi3(struct cdp_mlo_ctxt *cdp_ml_ctxt)
{
	struct dp_mlo_ctxt *mlo_ctxt = cdp_mlo_ctx_to_dp(cdp_ml_ctxt);

	if (!cdp_ml_ctxt)
		return;

	qdf_spinlock_destroy(&mlo_ctxt->grp_umac_reset_ctx.grp_ctx_lock);
	qdf_spinlock_destroy(&mlo_ctxt->ml_soc_list_lock);
	dp_mlo_peer_find_hash_detach_be(mlo_ctxt);
	qdf_mem_free(mlo_ctxt);
}

/**
 * dp_mlo_set_soc_by_chip_id() - Add DP soc to ML context soc list
 * @ml_ctxt: DP ML context handle
 * @soc: DP soc handle
 * @chip_id: MLO chip id
 *
 * Return: void
 */
static void dp_mlo_set_soc_by_chip_id(struct dp_mlo_ctxt *ml_ctxt,
				      struct dp_soc *soc,
				      uint8_t chip_id)
{
	qdf_spin_lock_bh(&ml_ctxt->ml_soc_list_lock);
	ml_ctxt->ml_soc_list[chip_id] = soc;

	/* The same API is called during soc_attach and soc_detach
	 * soc parameter is non-null or null accordingly.
	 */
	if (soc)
		ml_ctxt->ml_soc_cnt++;
	else
		ml_ctxt->ml_soc_cnt--;

	dp_umac_reset_update_partner_map(ml_ctxt, chip_id, !!soc);

	qdf_spin_unlock_bh(&ml_ctxt->ml_soc_list_lock);
}

struct dp_soc*
dp_mlo_get_soc_ref_by_chip_id(struct dp_mlo_ctxt *ml_ctxt,
			      uint8_t chip_id)
{
	struct dp_soc *soc = NULL;

	if (!ml_ctxt) {
		dp_warn("MLO context not created, MLO not enabled");
		return NULL;
	}

	qdf_spin_lock_bh(&ml_ctxt->ml_soc_list_lock);
	soc = ml_ctxt->ml_soc_list[chip_id];

	if (!soc) {
		qdf_spin_unlock_bh(&ml_ctxt->ml_soc_list_lock);
		return NULL;
	}

	qdf_atomic_inc(&soc->ref_count);
	qdf_spin_unlock_bh(&ml_ctxt->ml_soc_list_lock);

	return soc;
}

static QDF_STATUS dp_partner_soc_rx_hw_cc_init(struct dp_mlo_ctxt *mlo_ctxt,
					       struct dp_soc_be *be_soc)
{
	uint8_t i;
	struct dp_soc *partner_soc;
	struct dp_soc_be *be_partner_soc;
	uint8_t pool_id;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	for (i = 0; i < WLAN_MAX_MLO_CHIPS; i++) {
		partner_soc = dp_mlo_get_soc_ref_by_chip_id(mlo_ctxt, i);
		if (!partner_soc) {
			dp_err("partner_soc is NULL");
			continue;
		}

		be_partner_soc = dp_get_be_soc_from_dp_soc(partner_soc);

		for (pool_id = 0; pool_id < MAX_RXDESC_POOLS; pool_id++) {
			qdf_status =
				dp_hw_cookie_conversion_init
					(be_soc,
					 &be_partner_soc->rx_cc_ctx[pool_id]);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
				dp_alert("MLO partner soc RX CC init failed");
				return qdf_status;
			}
		}
	}

	return qdf_status;
}

static void dp_mlo_soc_drain_rx_buf(struct dp_soc *soc, void *arg, int chip_id)
{
	uint8_t i = 0;
	uint8_t cpu = 0;
	uint8_t rx_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS] = {0};
	uint8_t rx_err_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS] = {0};
	uint8_t rx_wbm_rel_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS] = {0};
	uint8_t reo_status_ring_mask[WLAN_CFG_INT_NUM_CONTEXTS] = {0};

	/* Save the current interrupt mask and disable the interrupts */
	for (i = 0; i < wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx); i++) {
		rx_ring_mask[i] = soc->intr_ctx[i].rx_ring_mask;
		rx_err_ring_mask[i] = soc->intr_ctx[i].rx_err_ring_mask;
		rx_wbm_rel_ring_mask[i] = soc->intr_ctx[i].rx_wbm_rel_ring_mask;
		reo_status_ring_mask[i] = soc->intr_ctx[i].reo_status_ring_mask;

		soc->intr_ctx[i].rx_ring_mask = 0;
		soc->intr_ctx[i].rx_err_ring_mask = 0;
		soc->intr_ctx[i].rx_wbm_rel_ring_mask = 0;
		soc->intr_ctx[i].reo_status_ring_mask = 0;
	}

	/* make sure dp_service_srngs not running on any of the CPU */
	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		while (qdf_atomic_test_bit(cpu,
					   &soc->service_rings_running))
			;
	}

	for (i = 0; i < wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx); i++) {
		uint8_t ring = 0;
		uint32_t num_entries = 0;
		hal_ring_handle_t hal_ring_hdl = NULL;
		uint8_t rx_mask = wlan_cfg_get_rx_ring_mask(
						soc->wlan_cfg_ctx, i);
		uint8_t rx_err_mask = wlan_cfg_get_rx_err_ring_mask(
						soc->wlan_cfg_ctx, i);
		uint8_t rx_wbm_rel_mask = wlan_cfg_get_rx_wbm_rel_ring_mask(
						soc->wlan_cfg_ctx, i);

		if (rx_mask) {
			/* iterate through each reo ring and process the buf */
			for (ring = 0; ring < soc->num_reo_dest_rings; ring++) {
				if (!(rx_mask & (1 << ring)))
					continue;

				hal_ring_hdl =
					soc->reo_dest_ring[ring].hal_srng;
				num_entries = hal_srng_get_num_entries(
								soc->hal_soc,
								hal_ring_hdl);
				dp_rx_process_be(&soc->intr_ctx[i],
						 hal_ring_hdl,
						 ring,
						 num_entries);
			}
		}

		/* Process REO Exception ring */
		if (rx_err_mask) {
			hal_ring_hdl = soc->reo_exception_ring.hal_srng;
			num_entries = hal_srng_get_num_entries(
						soc->hal_soc,
						hal_ring_hdl);

			dp_rx_err_process(&soc->intr_ctx[i], soc,
					  hal_ring_hdl, num_entries);
		}

		/* Process Rx WBM release ring */
		if (rx_wbm_rel_mask) {
			hal_ring_hdl = soc->rx_rel_ring.hal_srng;
			num_entries = hal_srng_get_num_entries(
						soc->hal_soc,
						hal_ring_hdl);

			dp_rx_wbm_err_process(&soc->intr_ctx[i], soc,
					      hal_ring_hdl, num_entries);
		}
	}

	/* restore the interrupt mask */
	for (i = 0; i < wlan_cfg_get_num_contexts(soc->wlan_cfg_ctx); i++) {
		soc->intr_ctx[i].rx_ring_mask = rx_ring_mask[i];
		soc->intr_ctx[i].rx_err_ring_mask = rx_err_ring_mask[i];
		soc->intr_ctx[i].rx_wbm_rel_ring_mask = rx_wbm_rel_ring_mask[i];
		soc->intr_ctx[i].reo_status_ring_mask = reo_status_ring_mask[i];
	}
}

static void dp_mlo_soc_setup(struct cdp_soc_t *soc_hdl,
			     struct cdp_mlo_ctxt *cdp_ml_ctxt)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_mlo_ctxt *mlo_ctxt = cdp_mlo_ctx_to_dp(cdp_ml_ctxt);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	uint8_t pdev_id;

	if (!cdp_ml_ctxt)
		return;

	be_soc->ml_ctxt = mlo_ctxt;

	for (pdev_id = 0; pdev_id < MAX_PDEV_CNT; pdev_id++) {
		if (soc->pdev_list[pdev_id])
			dp_mlo_update_link_to_pdev_map(soc,
						       soc->pdev_list[pdev_id]);
	}

	dp_mlo_set_soc_by_chip_id(mlo_ctxt, soc, be_soc->mlo_chip_id);
}

static void dp_mlo_soc_teardown(struct cdp_soc_t *soc_hdl,
				struct cdp_mlo_ctxt *cdp_ml_ctxt,
				bool is_force_down)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_mlo_ctxt *mlo_ctxt = cdp_mlo_ctx_to_dp(cdp_ml_ctxt);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	if (!cdp_ml_ctxt)
		return;

	/* During the teardown drain the Rx buffers if any exist in the ring */
	dp_mlo_iter_ptnr_soc(be_soc,
			     dp_mlo_soc_drain_rx_buf,
			     NULL);

	dp_mlo_set_soc_by_chip_id(mlo_ctxt, NULL, be_soc->mlo_chip_id);
	be_soc->ml_ctxt = NULL;
}

static QDF_STATUS dp_mlo_add_ptnr_vdev(struct dp_vdev *vdev1,
				       struct dp_vdev *vdev2,
				       struct dp_soc *soc, uint8_t pdev_id)
{
	struct dp_soc_be *soc_be = dp_get_be_soc_from_dp_soc(soc);
	struct dp_vdev_be *vdev2_be = dp_get_be_vdev_from_dp_vdev(vdev2);

	/* return when valid entry  exists */
	if (vdev2_be->partner_vdev_list[soc_be->mlo_chip_id][pdev_id] !=
							CDP_INVALID_VDEV_ID)
		return QDF_STATUS_SUCCESS;

	vdev2_be->partner_vdev_list[soc_be->mlo_chip_id][pdev_id] =
						vdev1->vdev_id;

	mlo_debug("Add vdev%d to vdev%d list, mlo_chip_id = %d pdev_id = %d\n",
		  vdev1->vdev_id, vdev2->vdev_id, soc_be->mlo_chip_id, pdev_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_update_mlo_ptnr_list(struct cdp_soc_t *soc_hdl,
				   int8_t partner_vdev_ids[], uint8_t num_vdevs,
				   uint8_t self_vdev_id)
{
	int i, j;
	struct dp_soc *self_soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *self_vdev;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(self_soc);
	struct dp_mlo_ctxt *dp_mlo = be_soc->ml_ctxt;

	if (!dp_mlo)
		return QDF_STATUS_E_FAILURE;

	self_vdev = dp_vdev_get_ref_by_id(self_soc, self_vdev_id, DP_MOD_ID_RX);
	if (!self_vdev)
		return QDF_STATUS_E_FAILURE;

	/* go through the input vdev id list and if there are partner vdevs,
	 * - then add the current vdev's id to partner vdev's list using pdev_id and
	 * increase the reference
	 * - add partner vdev to self list  and increase  the reference
	 */
	for (i = 0; i < num_vdevs; i++) {
		if (partner_vdev_ids[i] == CDP_INVALID_VDEV_ID)
			continue;

		for (j = 0; j < WLAN_MAX_MLO_CHIPS; j++) {
			struct dp_soc *soc =
				dp_mlo_get_soc_ref_by_chip_id(dp_mlo, j);
			if (soc) {
				struct dp_vdev *vdev;

				vdev = dp_vdev_get_ref_by_id(soc,
					partner_vdev_ids[i], DP_MOD_ID_RX);
				if (vdev) {
					if (vdev == self_vdev) {
						dp_vdev_unref_delete(soc,
							vdev, DP_MOD_ID_RX);
						/*dp_soc_unref_delete(soc); */
						continue;
					}
					if (qdf_is_macaddr_equal(
						(struct qdf_mac_addr *)self_vdev->mld_mac_addr.raw,
						(struct qdf_mac_addr *)vdev->mld_mac_addr.raw)) {
						if (dp_mlo_add_ptnr_vdev(self_vdev,
							vdev, self_soc,
							self_vdev->pdev->pdev_id) !=
							QDF_STATUS_SUCCESS) {
							dp_err("Unable to add self to partner vdev's list");
							dp_vdev_unref_delete(soc,
								vdev, DP_MOD_ID_RX);
							/* TODO - release soc ref here */
							/* dp_soc_unref_delete(soc);*/
							ret = QDF_STATUS_E_FAILURE;
							goto exit;
						}
						/* add to self list */
						if (dp_mlo_add_ptnr_vdev(vdev, self_vdev, soc,
							vdev->pdev->pdev_id) !=
							QDF_STATUS_SUCCESS) {
							dp_err("Unable to add vdev to self vdev's list");
							dp_vdev_unref_delete(self_soc,
								vdev, DP_MOD_ID_RX);
							/* TODO - release soc ref here */
							/* dp_soc_unref_delete(soc);*/
							ret = QDF_STATUS_E_FAILURE;
							goto exit;
						}
					}
					dp_vdev_unref_delete(soc, vdev,
							     DP_MOD_ID_RX);
				} /* vdev */
				/* TODO - release soc ref here */
				/* dp_soc_unref_delete(soc); */
			} /* soc */
		} /* for */
	} /* for */

exit:
	dp_vdev_unref_delete(self_soc, self_vdev, DP_MOD_ID_RX);
	return ret;
}

void dp_clr_mlo_ptnr_list(struct dp_soc *soc, struct dp_vdev *vdev)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_vdev_be *vdev_be = dp_get_be_vdev_from_dp_vdev(vdev);
	struct dp_mlo_ctxt *dp_mlo = be_soc->ml_ctxt;
	uint8_t soc_id = be_soc->mlo_chip_id;
	uint8_t pdev_id = vdev->pdev->pdev_id;
	int i, j;

	for (i = 0; i < WLAN_MAX_MLO_CHIPS; i++) {
		for (j = 0; j < WLAN_MAX_MLO_LINKS_PER_SOC; j++) {
			struct dp_vdev *pr_vdev;
			struct dp_soc *pr_soc;
			struct dp_soc_be *pr_soc_be;
			struct dp_pdev *pr_pdev;
			struct dp_vdev_be *pr_vdev_be;

			if (vdev_be->partner_vdev_list[i][j] ==
			    CDP_INVALID_VDEV_ID)
				continue;

			pr_soc = dp_mlo_get_soc_ref_by_chip_id(dp_mlo, i);
			if (!pr_soc)
				continue;
			pr_soc_be = dp_get_be_soc_from_dp_soc(pr_soc);
			pr_vdev = dp_vdev_get_ref_by_id(pr_soc,
						vdev_be->partner_vdev_list[i][j],
						DP_MOD_ID_RX);
			if (!pr_vdev)
				continue;

			/* remove self vdev from partner list */
			pr_vdev_be = dp_get_be_vdev_from_dp_vdev(pr_vdev);
			pr_vdev_be->partner_vdev_list[soc_id][pdev_id] =
				CDP_INVALID_VDEV_ID;

			/* remove partner vdev from self list */
			pr_pdev = pr_vdev->pdev;
			vdev_be->partner_vdev_list[pr_soc_be->mlo_chip_id][pr_pdev->pdev_id] =
				CDP_INVALID_VDEV_ID;

			dp_vdev_unref_delete(pr_soc, pr_vdev, DP_MOD_ID_RX);
		}
	}
}

static QDF_STATUS
dp_clear_mlo_ptnr_list(struct cdp_soc_t *soc_hdl, uint8_t self_vdev_id)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_vdev *vdev;

	vdev = dp_vdev_get_ref_by_id(soc, self_vdev_id, DP_MOD_ID_RX);
	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	dp_clr_mlo_ptnr_list(soc, vdev);
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_RX);
	return QDF_STATUS_SUCCESS;
}

static void dp_mlo_setup_complete(struct cdp_mlo_ctxt *cdp_ml_ctxt)
{
	struct dp_mlo_ctxt *mlo_ctxt = cdp_mlo_ctx_to_dp(cdp_ml_ctxt);
	int i;
	struct dp_soc *soc;
	struct dp_soc_be *be_soc;
	QDF_STATUS qdf_status;

	if (!cdp_ml_ctxt)
		return;

	for (i = 0; i < WLAN_MAX_MLO_CHIPS; i++) {
		soc = dp_mlo_get_soc_ref_by_chip_id(mlo_ctxt, i);

		if (!soc)
			continue;
		be_soc = dp_get_be_soc_from_dp_soc(soc);

		qdf_status = dp_partner_soc_rx_hw_cc_init(mlo_ctxt, be_soc);

		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			dp_alert("MLO partner SOC Rx desc CC init failed");
			qdf_assert_always(0);
		}
	}
}

static void dp_mlo_update_delta_tsf2(struct cdp_soc_t *soc_hdl,
				     uint8_t pdev_id, uint64_t delta_tsf2)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_pdev *pdev;
	struct dp_pdev_be *be_pdev;

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3((struct dp_soc *)soc,
						  pdev_id);
	if (!pdev) {
		dp_err("pdev is NULL for pdev_id %u", pdev_id);
		return;
	}

	be_pdev = dp_get_be_pdev_from_dp_pdev(pdev);

	be_pdev->delta_tsf2 = delta_tsf2;
}

static void dp_mlo_update_delta_tqm(struct cdp_soc_t *soc_hdl,
				    uint64_t delta_tqm)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	be_soc->delta_tqm = delta_tqm;
}

static void dp_mlo_update_mlo_ts_offset(struct cdp_soc_t *soc_hdl,
					uint64_t offset)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	be_soc->mlo_tstamp_offset = offset;
}

#ifdef CONFIG_MLO_SINGLE_DEV
static void dp_mlo_aggregate_mld_vdev_stats(struct dp_vdev_be *be_vdev,
					    struct dp_vdev *ptnr_vdev,
					    void *arg)
{
	struct cdp_vdev_stats *tgt_vdev_stats = (struct cdp_vdev_stats *)arg;
	struct cdp_vdev_stats *src_vdev_stats = &ptnr_vdev->stats;

	/* Aggregate vdev ingress stats */
	DP_UPDATE_INGRESS_STATS(tgt_vdev_stats, src_vdev_stats);

	/* Aggregate unmapped peers stats */
	DP_UPDATE_PER_PKT_STATS(tgt_vdev_stats, src_vdev_stats);
	DP_UPDATE_EXTD_STATS(tgt_vdev_stats, src_vdev_stats);

	/* Aggregate associated peers stats */
	dp_vdev_iterate_peer(ptnr_vdev, dp_update_vdev_stats, tgt_vdev_stats,
			     DP_MOD_ID_GENERIC_STATS);
}

static QDF_STATUS dp_mlo_get_mld_vdev_stats(struct cdp_soc_t *soc_hdl,
					    uint8_t vdev_id, void *buf)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct cdp_vdev_stats *vdev_stats;
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_GENERIC_STATS);
	struct dp_vdev_be *vdev_be = NULL;

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	vdev_be = dp_get_be_vdev_from_dp_vdev(vdev);
	if (!vdev_be) {
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_GENERIC_STATS);
		return QDF_STATUS_E_FAILURE;
	}

	vdev_stats = (struct cdp_vdev_stats *)buf;

	dp_aggregate_vdev_stats(vdev, buf);

	/* Aggregate stats from partner vdevs */
	dp_mlo_iter_ptnr_vdev(be_soc, vdev_be,
			      dp_mlo_aggregate_mld_vdev_stats, buf,
			      DP_MOD_ID_GENERIC_STATS);

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_GENERIC_STATS);

	return QDF_STATUS_SUCCESS;
}
#endif

static struct cdp_mlo_ops dp_mlo_ops = {
	.mlo_soc_setup = dp_mlo_soc_setup,
	.mlo_soc_teardown = dp_mlo_soc_teardown,
	.update_mlo_ptnr_list = dp_update_mlo_ptnr_list,
	.clear_mlo_ptnr_list = dp_clear_mlo_ptnr_list,
	.mlo_setup_complete = dp_mlo_setup_complete,
	.mlo_update_delta_tsf2 = dp_mlo_update_delta_tsf2,
	.mlo_update_delta_tqm = dp_mlo_update_delta_tqm,
	.mlo_update_mlo_ts_offset = dp_mlo_update_mlo_ts_offset,
	.mlo_ctxt_attach = dp_mlo_ctxt_attach_wifi3,
	.mlo_ctxt_detach = dp_mlo_ctxt_detach_wifi3,
#ifdef CONFIG_MLO_SINGLE_DEV
	.mlo_get_mld_vdev_stats = dp_mlo_get_mld_vdev_stats,
#endif
};

void dp_soc_mlo_fill_params(struct dp_soc *soc,
			    struct cdp_soc_attach_params *params)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	if (!params->mlo_enabled) {
		dp_warn("MLO not enabled on SOC");
		return;
	}

	be_soc->mlo_chip_id = params->mlo_chip_id;
	be_soc->ml_ctxt = cdp_mlo_ctx_to_dp(params->ml_context);
	be_soc->mlo_enabled = 1;
	soc->cdp_soc.ops->mlo_ops = &dp_mlo_ops;
}

void dp_mlo_update_link_to_pdev_map(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_pdev_be *be_pdev = dp_get_be_pdev_from_dp_pdev(pdev);
	struct dp_mlo_ctxt *ml_ctxt = be_soc->ml_ctxt;
	uint8_t link_id;

	if (!be_soc->mlo_enabled)
		return;

	if (!ml_ctxt)
		return;

	link_id = be_pdev->mlo_link_id;

	if (link_id < WLAN_MAX_MLO_CHIPS * WLAN_MAX_MLO_LINKS_PER_SOC) {
		if (!ml_ctxt->link_to_pdev_map[link_id])
			ml_ctxt->link_to_pdev_map[link_id] = be_pdev;
		else
			dp_alert("Attempt to update existing map for link %u",
				 link_id);
	}
}

void dp_mlo_update_link_to_pdev_unmap(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_pdev_be *be_pdev = dp_get_be_pdev_from_dp_pdev(pdev);
	struct dp_mlo_ctxt *ml_ctxt = be_soc->ml_ctxt;
	uint8_t link_id;

	if (!be_soc->mlo_enabled)
		return;

	if (!ml_ctxt)
		return;

	link_id = be_pdev->mlo_link_id;

	if (link_id < WLAN_MAX_MLO_CHIPS * WLAN_MAX_MLO_LINKS_PER_SOC)
		ml_ctxt->link_to_pdev_map[link_id] = NULL;
}

static struct dp_pdev_be *
dp_mlo_get_be_pdev_from_link_id(struct dp_mlo_ctxt *ml_ctxt, uint8_t link_id)
{
	if (link_id < WLAN_MAX_MLO_CHIPS * WLAN_MAX_MLO_LINKS_PER_SOC)
		return ml_ctxt->link_to_pdev_map[link_id];
	return NULL;
}

void dp_pdev_mlo_fill_params(struct dp_pdev *pdev,
			     struct cdp_pdev_attach_params *params)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(pdev->soc);
	struct dp_pdev_be *be_pdev = dp_get_be_pdev_from_dp_pdev(pdev);

	if (!be_soc->mlo_enabled) {
		dp_info("MLO not enabled on SOC");
		return;
	}

	be_pdev->mlo_link_id = params->mlo_link_id;
}

void dp_mlo_partner_chips_map(struct dp_soc *soc,
			      struct dp_peer *peer,
			      uint16_t peer_id)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *mlo_ctxt = NULL;
	bool is_ml_peer_id =
		HTT_RX_PEER_META_DATA_V1_ML_PEER_VALID_GET(peer_id);
	uint8_t chip_id;
	struct dp_soc *temp_soc;

	/* for non ML peer dont map on partner chips*/
	if (!is_ml_peer_id)
		return;

	mlo_ctxt = be_soc->ml_ctxt;
	if (!mlo_ctxt)
		return;

	qdf_spin_lock_bh(&mlo_ctxt->ml_soc_list_lock);
	for (chip_id = 0; chip_id < DP_MAX_MLO_CHIPS; chip_id++) {
		temp_soc = mlo_ctxt->ml_soc_list[chip_id];

		if (!temp_soc)
			continue;

		/* skip if this is current soc */
		if (temp_soc == soc)
			continue;

		dp_peer_find_id_to_obj_add(temp_soc, peer, peer_id);
	}
	qdf_spin_unlock_bh(&mlo_ctxt->ml_soc_list_lock);
}

qdf_export_symbol(dp_mlo_partner_chips_map);

void dp_mlo_partner_chips_unmap(struct dp_soc *soc,
				uint16_t peer_id)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *mlo_ctxt = be_soc->ml_ctxt;
	bool is_ml_peer_id =
		HTT_RX_PEER_META_DATA_V1_ML_PEER_VALID_GET(peer_id);
	uint8_t chip_id;
	struct dp_soc *temp_soc;

	if (!is_ml_peer_id)
		return;

	if (!mlo_ctxt)
		return;

	qdf_spin_lock_bh(&mlo_ctxt->ml_soc_list_lock);
	for (chip_id = 0; chip_id < DP_MAX_MLO_CHIPS; chip_id++) {
		temp_soc = mlo_ctxt->ml_soc_list[chip_id];

		if (!temp_soc)
			continue;

		/* skip if this is current soc */
		if (temp_soc == soc)
			continue;

		dp_peer_find_id_to_obj_remove(temp_soc, peer_id);
	}
	qdf_spin_unlock_bh(&mlo_ctxt->ml_soc_list_lock);
}

qdf_export_symbol(dp_mlo_partner_chips_unmap);

uint8_t dp_mlo_get_chip_id(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	return be_soc->mlo_chip_id;
}

qdf_export_symbol(dp_mlo_get_chip_id);

struct dp_peer *
dp_link_peer_hash_find_by_chip_id(struct dp_soc *soc,
				  uint8_t *peer_mac_addr,
				  int mac_addr_is_aligned,
				  uint8_t vdev_id,
				  uint8_t chip_id,
				  enum dp_mod_id mod_id)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *mlo_ctxt = be_soc->ml_ctxt;
	struct dp_soc *link_peer_soc = NULL;
	struct dp_peer *peer = NULL;

	if (!mlo_ctxt)
		return NULL;

	link_peer_soc = dp_mlo_get_soc_ref_by_chip_id(mlo_ctxt, chip_id);

	if (!link_peer_soc)
		return NULL;

	peer = dp_peer_find_hash_find(link_peer_soc, peer_mac_addr,
				      mac_addr_is_aligned, vdev_id,
				      mod_id);
	qdf_atomic_dec(&link_peer_soc->ref_count);
	return peer;
}

qdf_export_symbol(dp_link_peer_hash_find_by_chip_id);

void dp_mlo_get_rx_hash_key(struct dp_soc *soc,
			    struct cdp_lro_hash_config *lro_hash)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *ml_ctxt = be_soc->ml_ctxt;

	if (!be_soc->mlo_enabled || !ml_ctxt)
		return dp_get_rx_hash_key_bytes(lro_hash);

	qdf_mem_copy(lro_hash->toeplitz_hash_ipv4, ml_ctxt->toeplitz_hash_ipv4,
		     (sizeof(lro_hash->toeplitz_hash_ipv4[0]) *
		      LRO_IPV4_SEED_ARR_SZ));
	qdf_mem_copy(lro_hash->toeplitz_hash_ipv6, ml_ctxt->toeplitz_hash_ipv6,
		     (sizeof(lro_hash->toeplitz_hash_ipv6[0]) *
		      LRO_IPV6_SEED_ARR_SZ));
}

struct dp_soc *
dp_rx_replensih_soc_get(struct dp_soc *soc, uint8_t chip_id)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *mlo_ctxt = be_soc->ml_ctxt;
	struct dp_soc *replenish_soc;

	if (!be_soc->mlo_enabled || !mlo_ctxt)
		return soc;

	if (be_soc->mlo_chip_id == chip_id)
		return soc;

	replenish_soc = dp_mlo_get_soc_ref_by_chip_id(mlo_ctxt, chip_id);
	if (qdf_unlikely(!replenish_soc)) {
		dp_alert("replenish SOC is NULL");
		qdf_assert_always(0);
	}

	return replenish_soc;
}

uint8_t dp_soc_get_num_soc_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *mlo_ctxt = be_soc->ml_ctxt;

	if (!be_soc->mlo_enabled || !mlo_ctxt)
		return 1;

	return mlo_ctxt->ml_soc_cnt;
}

struct dp_soc *
dp_soc_get_by_idle_bm_id(struct dp_soc *soc, uint8_t idle_bm_id)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *mlo_ctxt = be_soc->ml_ctxt;
	struct dp_soc *partner_soc = NULL;
	uint8_t chip_id;

	if (!be_soc->mlo_enabled || !mlo_ctxt)
		return soc;

	for (chip_id = 0; chip_id < WLAN_MAX_MLO_CHIPS; chip_id++) {
		partner_soc = dp_mlo_get_soc_ref_by_chip_id(mlo_ctxt, chip_id);

		if (!partner_soc)
			continue;

		if (partner_soc->idle_link_bm_id == idle_bm_id)
			return partner_soc;
	}

	return NULL;
}

#ifdef WLAN_MLO_MULTI_CHIP
void dp_mlo_iter_ptnr_vdev(struct dp_soc_be *be_soc,
			   struct dp_vdev_be *be_vdev,
			   dp_ptnr_vdev_iter_func func,
			   void *arg,
			   enum dp_mod_id mod_id)
{
	int i = 0;
	int j = 0;
	struct dp_mlo_ctxt *dp_mlo = be_soc->ml_ctxt;

	for (i = 0; i < WLAN_MAX_MLO_CHIPS ; i++) {
		struct dp_soc *ptnr_soc =
				dp_mlo_get_soc_ref_by_chip_id(dp_mlo, i);

		if (!ptnr_soc)
			continue;
		for (j = 0 ; j < WLAN_MAX_MLO_LINKS_PER_SOC ; j++) {
			struct dp_vdev *ptnr_vdev;

			ptnr_vdev = dp_vdev_get_ref_by_id(
					ptnr_soc,
					be_vdev->partner_vdev_list[i][j],
					mod_id);
			if (!ptnr_vdev)
				continue;
			(*func)(be_vdev, ptnr_vdev, arg);
			dp_vdev_unref_delete(ptnr_vdev->pdev->soc,
					     ptnr_vdev,
					     mod_id);
		}
	}
}

qdf_export_symbol(dp_mlo_iter_ptnr_vdev);
#endif

#ifdef WLAN_MCAST_MLO
struct dp_vdev *dp_mlo_get_mcast_primary_vdev(struct dp_soc_be *be_soc,
					      struct dp_vdev_be *be_vdev,
					      enum dp_mod_id mod_id)
{
	int i = 0;
	int j = 0;
	struct dp_mlo_ctxt *dp_mlo = be_soc->ml_ctxt;
	struct dp_vdev *vdev = (struct dp_vdev *)be_vdev;

	if (be_vdev->mcast_primary) {
		if (dp_vdev_get_ref((struct dp_soc *)be_soc, vdev, mod_id) !=
					QDF_STATUS_SUCCESS)
			return NULL;

		return vdev;
	}

	for (i = 0; i < WLAN_MAX_MLO_CHIPS ; i++) {
		struct dp_soc *ptnr_soc =
				dp_mlo_get_soc_ref_by_chip_id(dp_mlo, i);

		if (!ptnr_soc)
			continue;
		for (j = 0 ; j < WLAN_MAX_MLO_LINKS_PER_SOC ; j++) {
			struct dp_vdev *ptnr_vdev = NULL;
			struct dp_vdev_be *be_ptnr_vdev = NULL;

			ptnr_vdev = dp_vdev_get_ref_by_id(
					ptnr_soc,
					be_vdev->partner_vdev_list[i][j],
					mod_id);
			if (!ptnr_vdev)
				continue;
			be_ptnr_vdev = dp_get_be_vdev_from_dp_vdev(ptnr_vdev);
			if (be_ptnr_vdev->mcast_primary)
				return ptnr_vdev;
			dp_vdev_unref_delete(be_ptnr_vdev->vdev.pdev->soc,
					     &be_ptnr_vdev->vdev,
					     mod_id);
		}
	}
	return NULL;
}

qdf_export_symbol(dp_mlo_get_mcast_primary_vdev);
#endif

/**
 * dp_mlo_iter_ptnr_soc() - iterate through mlo soc list and call the callback
 * @be_soc: dp_soc_be pointer
 * @func: Function to be called for each soc
 * @arg: context to be passed to the callback
 *
 * Return: true if mlo is enabled, false if mlo is disabled
 */
bool dp_mlo_iter_ptnr_soc(struct dp_soc_be *be_soc, dp_ptnr_soc_iter_func func,
			  void *arg)
{
	int i = 0;
	struct dp_mlo_ctxt *dp_mlo = be_soc->ml_ctxt;

	if (!be_soc->mlo_enabled || !be_soc->ml_ctxt)
		return false;

	for (i = 0; i < WLAN_MAX_MLO_CHIPS ; i++) {
		struct dp_soc *ptnr_soc =
				dp_mlo_get_soc_ref_by_chip_id(dp_mlo, i);

		if (!ptnr_soc)
			continue;
		(*func)(ptnr_soc, arg, i);
	}

	return true;
}

qdf_export_symbol(dp_mlo_iter_ptnr_soc);

static inline uint64_t dp_mlo_get_mlo_ts_offset(struct dp_pdev_be *be_pdev)
{
	struct dp_soc *soc;
	struct dp_pdev *pdev;
	struct dp_soc_be *be_soc;
	uint32_t mlo_offset;

	pdev = &be_pdev->pdev;
	soc = pdev->soc;
	be_soc = dp_get_be_soc_from_dp_soc(soc);

	mlo_offset = be_soc->mlo_tstamp_offset;

	return mlo_offset;
}

int32_t dp_mlo_get_delta_tsf2_wrt_mlo_offset(struct dp_soc *soc,
					     uint8_t hw_link_id)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *ml_ctxt = be_soc->ml_ctxt;
	struct dp_pdev_be *be_pdev;
	int32_t delta_tsf2_mlo_offset;
	int32_t mlo_offset, delta_tsf2;

	if (!ml_ctxt)
		return 0;

	be_pdev = dp_mlo_get_be_pdev_from_link_id(ml_ctxt, hw_link_id);
	if (!be_pdev)
		return 0;

	mlo_offset = dp_mlo_get_mlo_ts_offset(be_pdev);
	delta_tsf2 = be_pdev->delta_tsf2;

	delta_tsf2_mlo_offset = mlo_offset - delta_tsf2;

	return delta_tsf2_mlo_offset;
}

int32_t dp_mlo_get_delta_tqm_wrt_mlo_offset(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	int32_t delta_tqm_mlo_offset;
	int32_t mlo_offset, delta_tqm;

	mlo_offset = be_soc->mlo_tstamp_offset;
	delta_tqm = be_soc->delta_tqm;

	delta_tqm_mlo_offset = mlo_offset - delta_tqm;

	return delta_tqm_mlo_offset;
}

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * dp_umac_reset_update_partner_map() - Update Umac reset partner map
 * @mlo_ctx: DP ML context handle
 * @chip_id: chip id
 * @set: flag indicating whether to set or clear the bit
 *
 * Return: void
 */
static void dp_umac_reset_update_partner_map(struct dp_mlo_ctxt *mlo_ctx,
					     int chip_id, bool set)
{
	struct dp_soc_mlo_umac_reset_ctx *grp_umac_reset_ctx =
						&mlo_ctx->grp_umac_reset_ctx;

	if (set)
		qdf_atomic_set_bit(chip_id, &grp_umac_reset_ctx->partner_map);
	else
		qdf_atomic_clear_bit(chip_id, &grp_umac_reset_ctx->partner_map);
}

QDF_STATUS dp_umac_reset_notify_asserted_soc(struct dp_soc *soc)
{
	struct dp_mlo_ctxt *mlo_ctx;
	struct dp_soc_be *be_soc;

	be_soc = dp_get_be_soc_from_dp_soc(soc);
	if (!be_soc) {
		dp_umac_reset_err("null be_soc");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_ctx = be_soc->ml_ctxt;
	if (!mlo_ctx) {
		/* This API can be called for non-MLO SOC as well. Hence, return
		 * the status as success when mlo_ctx is NULL.
		 */
		return QDF_STATUS_SUCCESS;
	}

	dp_umac_reset_update_partner_map(mlo_ctx, be_soc->mlo_chip_id, false);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_umac_reset_complete_umac_recovery() - Complete Umac reset session
 * @soc: dp soc handle
 *
 * Return: void
 */
void dp_umac_reset_complete_umac_recovery(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *mlo_ctx = be_soc->ml_ctxt;
	struct dp_soc_mlo_umac_reset_ctx *grp_umac_reset_ctx;

	if (!mlo_ctx) {
		dp_umac_reset_alert("Umac reset was handled on soc %pK", soc);
		return;
	}

	grp_umac_reset_ctx = &mlo_ctx->grp_umac_reset_ctx;
	qdf_spin_lock_bh(&grp_umac_reset_ctx->grp_ctx_lock);

	grp_umac_reset_ctx->umac_reset_in_progress = false;
	grp_umac_reset_ctx->is_target_recovery = false;
	grp_umac_reset_ctx->response_map = 0;
	grp_umac_reset_ctx->request_map = 0;
	grp_umac_reset_ctx->initiator_chip_id = 0;

	qdf_spin_unlock_bh(&grp_umac_reset_ctx->grp_ctx_lock);

	dp_umac_reset_alert("Umac reset was handled on mlo group ctxt %pK",
			    mlo_ctx);
}

/**
 * dp_umac_reset_initiate_umac_recovery() - Initiate Umac reset session
 * @soc: dp soc handle
 * @is_target_recovery: Flag to indicate if it is triggered for target recovery
 *
 * Return: void
 */
void dp_umac_reset_initiate_umac_recovery(struct dp_soc *soc,
					  bool is_target_recovery)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *mlo_ctx = be_soc->ml_ctxt;
	struct dp_soc_mlo_umac_reset_ctx *grp_umac_reset_ctx;

	if (!mlo_ctx)
		return;

	grp_umac_reset_ctx = &mlo_ctx->grp_umac_reset_ctx;
	qdf_spin_lock_bh(&grp_umac_reset_ctx->grp_ctx_lock);

	if (grp_umac_reset_ctx->umac_reset_in_progress) {
		qdf_spin_unlock_bh(&grp_umac_reset_ctx->grp_ctx_lock);
		return;
	}

	grp_umac_reset_ctx->umac_reset_in_progress = true;
	grp_umac_reset_ctx->is_target_recovery = is_target_recovery;

	/* We don't wait for the 'Umac trigger' message from all socs */
	grp_umac_reset_ctx->request_map = grp_umac_reset_ctx->partner_map;
	grp_umac_reset_ctx->response_map = grp_umac_reset_ctx->partner_map;
	grp_umac_reset_ctx->initiator_chip_id = dp_mlo_get_chip_id(soc);
	grp_umac_reset_ctx->umac_reset_count++;

	qdf_spin_unlock_bh(&grp_umac_reset_ctx->grp_ctx_lock);
}

/**
 * dp_umac_reset_handle_action_cb() - Function to call action callback
 * @soc: dp soc handle
 * @umac_reset_ctx: Umac reset context
 * @action: Action to call the callback for
 *
 * Return: QDF_STATUS status
 */
QDF_STATUS
dp_umac_reset_handle_action_cb(struct dp_soc *soc,
			       struct dp_soc_umac_reset_ctx *umac_reset_ctx,
			       enum umac_reset_action action)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *mlo_ctx = be_soc->ml_ctxt;
	struct dp_soc_mlo_umac_reset_ctx *grp_umac_reset_ctx;

	if (!mlo_ctx) {
		dp_umac_reset_debug("MLO context is Null");
		goto handle;
	}

	grp_umac_reset_ctx = &mlo_ctx->grp_umac_reset_ctx;
	qdf_spin_lock_bh(&grp_umac_reset_ctx->grp_ctx_lock);

	qdf_atomic_set_bit(dp_mlo_get_chip_id(soc),
			   &grp_umac_reset_ctx->request_map);

	dp_umac_reset_debug("partner_map %u request_map %u",
			    grp_umac_reset_ctx->partner_map,
			    grp_umac_reset_ctx->request_map);

	/* This logic is needed for synchronization between mlo socs */
	if ((grp_umac_reset_ctx->partner_map & grp_umac_reset_ctx->request_map)
			!= grp_umac_reset_ctx->partner_map) {
		struct hif_softc *hif_sc = HIF_GET_SOFTC(soc->hif_handle);
		struct hif_umac_reset_ctx *hif_umac_reset_ctx;

		if (!hif_sc) {
			hif_err("scn is null");
			qdf_assert_always(0);
			return QDF_STATUS_E_FAILURE;
		}

		hif_umac_reset_ctx = &hif_sc->umac_reset_ctx;

		/* Mark the action as pending */
		umac_reset_ctx->pending_action = action;
		/* Reschedule the tasklet and exit */
		tasklet_hi_schedule(&hif_umac_reset_ctx->intr_tq);
		qdf_spin_unlock_bh(&grp_umac_reset_ctx->grp_ctx_lock);

		return QDF_STATUS_SUCCESS;
	}

	qdf_spin_unlock_bh(&grp_umac_reset_ctx->grp_ctx_lock);
	umac_reset_ctx->pending_action = UMAC_RESET_ACTION_NONE;

handle:
	if (!umac_reset_ctx->rx_actions.cb[action]) {
		dp_umac_reset_err("rx callback is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return umac_reset_ctx->rx_actions.cb[action](soc);
}

/**
 * dp_umac_reset_post_tx_cmd() - Iterate partner socs and post Tx command
 * @umac_reset_ctx: UMAC reset context
 * @tx_cmd: Tx command to be posted
 *
 * Return: QDF status of operation
 */
QDF_STATUS
dp_umac_reset_post_tx_cmd(struct dp_soc_umac_reset_ctx *umac_reset_ctx,
			  enum umac_reset_tx_cmd tx_cmd)
{
	struct dp_soc *soc = container_of(umac_reset_ctx, struct dp_soc,
					  umac_reset_ctx);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *mlo_ctx = be_soc->ml_ctxt;
	struct dp_soc_mlo_umac_reset_ctx *grp_umac_reset_ctx;

	if (!mlo_ctx) {
		dp_umac_reset_post_tx_cmd_via_shmem(soc, &tx_cmd, 0);
		return QDF_STATUS_SUCCESS;
	}

	grp_umac_reset_ctx = &mlo_ctx->grp_umac_reset_ctx;
	qdf_spin_lock_bh(&grp_umac_reset_ctx->grp_ctx_lock);

	qdf_atomic_set_bit(dp_mlo_get_chip_id(soc),
			   &grp_umac_reset_ctx->response_map);

	/* This logic is needed for synchronization between mlo socs */
	if ((grp_umac_reset_ctx->partner_map & grp_umac_reset_ctx->response_map)
				!= grp_umac_reset_ctx->partner_map) {
		dp_umac_reset_debug(
			"Response(s) pending : expected map %u current map %u",
			grp_umac_reset_ctx->partner_map,
			grp_umac_reset_ctx->response_map);

		qdf_spin_unlock_bh(&grp_umac_reset_ctx->grp_ctx_lock);
		return QDF_STATUS_SUCCESS;
	}

	dp_umac_reset_debug(
		"All responses received: expected map %u current map %u",
		grp_umac_reset_ctx->partner_map,
		grp_umac_reset_ctx->response_map);

	grp_umac_reset_ctx->response_map = 0;
	grp_umac_reset_ctx->request_map = 0;
	qdf_spin_unlock_bh(&grp_umac_reset_ctx->grp_ctx_lock);

	dp_mlo_iter_ptnr_soc(be_soc, &dp_umac_reset_post_tx_cmd_via_shmem,
			     &tx_cmd);

	if (tx_cmd == UMAC_RESET_TX_CMD_POST_RESET_COMPLETE_DONE)
		dp_umac_reset_complete_umac_recovery(soc);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_umac_reset_initiator_check() - Check if soc is the Umac reset initiator
 * @soc: dp soc handle
 *
 * Return: true if the soc is initiator or false otherwise
 */
bool dp_umac_reset_initiator_check(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *mlo_ctx = be_soc->ml_ctxt;

	if (!mlo_ctx)
		return true;

	return (mlo_ctx->grp_umac_reset_ctx.initiator_chip_id ==
				dp_mlo_get_chip_id(soc));
}

/**
 * dp_umac_reset_target_recovery_check() - Check if this is for target recovery
 * @soc: dp soc handle
 *
 * Return: true if the session is for target recovery or false otherwise
 */
bool dp_umac_reset_target_recovery_check(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *mlo_ctx = be_soc->ml_ctxt;

	if (!mlo_ctx)
		return false;

	return mlo_ctx->grp_umac_reset_ctx.is_target_recovery;
}

/**
 * dp_umac_reset_is_soc_ignored() - Check if this soc is to be ignored
 * @soc: dp soc handle
 *
 * Return: true if the soc is ignored or false otherwise
 */
bool dp_umac_reset_is_soc_ignored(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_mlo_ctxt *mlo_ctx = be_soc->ml_ctxt;

	if (!mlo_ctx)
		return false;

	return !qdf_atomic_test_bit(dp_mlo_get_chip_id(soc),
				    &mlo_ctx->grp_umac_reset_ctx.partner_map);
}

QDF_STATUS dp_mlo_umac_reset_stats_print(struct dp_soc *soc)
{
	struct dp_mlo_ctxt *mlo_ctx;
	struct dp_soc_be *be_soc;
	struct dp_soc_mlo_umac_reset_ctx *grp_umac_reset_ctx;

	be_soc = dp_get_be_soc_from_dp_soc(soc);
	if (!be_soc) {
		dp_umac_reset_err("null be_soc");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_ctx = be_soc->ml_ctxt;
	if (!mlo_ctx) {
		/* This API can be called for non-MLO SOC as well. Hence, return
		 * the status as success when mlo_ctx is NULL.
		 */
		return QDF_STATUS_SUCCESS;
	}

	grp_umac_reset_ctx = &mlo_ctx->grp_umac_reset_ctx;

	DP_UMAC_RESET_PRINT_STATS("MLO UMAC RESET stats\n"
		  "\t\tPartner map                   :%x\n"
		  "\t\tRequest map                   :%x\n"
		  "\t\tResponse map                  :%x\n"
		  "\t\tIs target recovery            :%d\n"
		  "\t\tIs Umac reset inprogress      :%d\n"
		  "\t\tNumber of UMAC reset triggered:%d\n"
		  "\t\tInitiator chip ID             :%d\n",
		  grp_umac_reset_ctx->partner_map,
		  grp_umac_reset_ctx->request_map,
		  grp_umac_reset_ctx->response_map,
		  grp_umac_reset_ctx->is_target_recovery,
		  grp_umac_reset_ctx->umac_reset_in_progress,
		  grp_umac_reset_ctx->umac_reset_count,
		  grp_umac_reset_ctx->initiator_chip_id);

	return QDF_STATUS_SUCCESS;
}
#endif
