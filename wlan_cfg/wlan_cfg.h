/*
* * Copyright (c) 2013-2017 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_CFG_H
#define __WLAN_CFG_H

/*
 * Temporary place holders. These should come either from target config
 * or platform configuration
 */
#if defined(CONFIG_MCL)
#define MAX_PDEV_CNT 1
#else
#define MAX_PDEV_CNT 3
#endif

/* Tx configuration */
#define MAX_LINK_DESC_BANKS 8
#define MAX_TXDESC_POOLS 4
#define MAX_TCL_DATA_RINGS 4
#define DP_MAX_TX_RINGS 8
#define MAX_TX_HW_QUEUES 3

/* Rx configuration */
#define MAX_RXDESC_POOLS 4
#define MAX_REO_DEST_RINGS 4
#define DP_MAX_RX_RINGS 8
#define MAX_RX_MAC_RINGS 2

/* Miscellaneous configuration */
#define MAX_IDLE_SCATTER_BUFS 16
#define DP_MAX_IRQ_PER_CONTEXT 12
#define DP_MAX_INTERRUPT_CONTEXTS 8
#define DP_MAX_INTERRUPT_CONTEXTS 8
#define MAX_HTT_METADATA_LEN 32
#define MAX_NUM_PEER_ID_PER_PEER 8
#define DP_MAX_TIDS 17
#define DP_NON_QOS_TID 16

struct wlan_cfg_dp_pdev_ctxt;
struct wlan_cfg_dp_soc_ctxt;

/**
 * wlan_cfg_soc_attach() - Attach configuration interface for SoC
 *
 * Allocates context for Soc configuration parameters,
 * Read configuration information from device tree/ini file and
 * returns back handle
 *
 * Return: Handle to configuration context
 */
struct wlan_cfg_dp_soc_ctxt *wlan_cfg_soc_attach(void);

/**
 * wlan_cfg_soc_detach() - Detach soc configuration handle
 * @wlan_cfg_ctx: soc configuration handle
 *
 * De-allocates memory allocated for SoC configuration
 *
 * Return:none
 */
void wlan_cfg_soc_detach(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_pdev_attach() Attach configuration interface for pdev
 *
 * Allocates context for pdev configuration parameters,
 * Read configuration information from device tree/ini file and
 * returns back handle
 *
 * Return: Handle to configuration context
 */
struct wlan_cfg_dp_pdev_ctxt *wlan_cfg_pdev_attach(void);

/**
 * wlan_cfg_pdev_detach() Detach and free pdev configuration handle
 * @wlan_cfg_pdev_ctx - PDEV Configuration Handle
 *
 * Return: void
 */
void wlan_cfg_pdev_detach(struct wlan_cfg_dp_pdev_ctxt *wlan_cfg_pdev_ctx);

void wlan_cfg_set_num_contexts(struct wlan_cfg_dp_soc_ctxt *cfg, int num);
void wlan_cfg_set_tx_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
			       int context, int mask);
void wlan_cfg_set_rx_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
			       int context, int mask);
void wlan_cfg_set_rx_mon_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
				   int context, int mask);
void wlan_cfg_set_ce_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
			       int context, int mask);
void wlan_cfg_set_rxbuf_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg, int context,
				  int mask);
void wlan_cfg_set_max_peer_id(struct wlan_cfg_dp_soc_ctxt *cfg, uint32_t val);

int wlan_cfg_set_rx_err_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
				int context, int mask);
int wlan_cfg_set_rx_wbm_rel_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					int context, int mask);
int wlan_cfg_set_reo_status_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					int context, int mask);
int wlan_cfg_get_rx_err_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					int context);
int wlan_cfg_get_rx_wbm_rel_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					int context);
int wlan_cfg_get_reo_status_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
					int context);
/**
 * wlan_cfg_get_num_contexts() - Number of interrupt contexts to be registered
 * @wlan_cfg_ctx - Configuration Handle
 *
 * For WIN,  DP_NUM_INTERRUPT_CONTEXTS will be equal to  number of CPU cores.
 * Each context (for linux it is a NAPI context) will have a tx_ring_mask,
 * rx_ring_mask ,and rx_monitor_ring mask  to indicate the rings
 * that are processed by the handler.
 *
 * Return: num_contexts
 */
int wlan_cfg_get_num_contexts(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_get_tx_ring_mask() - Return Tx interrupt mask mapped to an
 *				 interrupt context
 * @wlan_cfg_ctx - Configuration Handle
 * @context - Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_tx_ring_mask[context]
 */
int wlan_cfg_get_tx_ring_mask(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx,
		int context);

/**
 * wlan_cfg_get_rx_ring_mask() - Return Rx interrupt mask mapped to an
 *				 interrupt context
 * @wlan_cfg_ctx - Configuration Handle
 * @context - Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_rx_ring_mask[context]
 */
int wlan_cfg_get_rx_ring_mask(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx,
		int context);

/**
 * wlan_cfg_get_rx_mon_ring_mask() - Return Rx monitor ring interrupt mask
 *				   mapped to an interrupt context
 * @wlan_cfg_ctx - Configuration Handle
 * @context - Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_rx_mon_ring_mask[context]
 */
int wlan_cfg_get_rx_mon_ring_mask(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx,
		int context);

/**
 * wlan_cfg_set_rxdma2host_ring_mask() - Set rxdma2host ring interrupt mask
 *				   for the given interrupt context
 * @wlan_cfg_ctx - Configuration Handle
 * @context - Numerical ID identifying the Interrupt/NAPI context
 *
 */
void wlan_cfg_set_rxdma2host_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
	int context, int mask);

/**
 * wlan_cfg_get_rxdma2host_ring_mask() - Return rxdma2host ring interrupt mask
 *				   mapped to an interrupt context
 * @wlan_cfg_ctx - Configuration Handle
 * @context - Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_rxdma2host_ring_mask[context]
 */
int wlan_cfg_get_rxdma2host_ring_mask(struct wlan_cfg_dp_soc_ctxt *cfg,
	int context);

/**
 * wlan_cfg_set_hw_macid() - Set HW MAC Id for the given PDEV index
 *
 * @wlan_cfg_ctx - Configuration Handle
 * @pdev_idx - Index of SW PDEV
 * @hw_macid - HW MAC Id
 *
 */
void wlan_cfg_set_hw_macid(struct wlan_cfg_dp_soc_ctxt *cfg, int pdev_idx,
	int hw_macid);

/**
 * wlan_cfg_get_hw_macid() - Get HW MAC Id for the given PDEV index
 *
 * @wlan_cfg_ctx - Configuration Handle
 * @pdev_idx - Index of SW PDEV
 *
 * Return: HW MAC Id
 */
int wlan_cfg_get_hw_macid(struct wlan_cfg_dp_soc_ctxt *cfg, int pdev_idx);

/**
 * wlan_cfg_get_hw_mac_idx() - Get 0 based HW MAC index for the given
 * PDEV index
 *
 * @wlan_cfg_ctx - Configuration Handle
 * @pdev_idx - Index of SW PDEV
 *
 * Return: HW MAC index
 */
int wlan_cfg_get_hw_mac_idx(struct wlan_cfg_dp_soc_ctxt *cfg, int pdev_idx);

/**
 * wlan_cfg_get_ce_ring_mask() - Return CE ring interrupt mask
 *				mapped to an interrupt context
 * @wlan_cfg_ctx - Configuration Handle
 * @context - Numerical ID identifying the Interrupt/NAPI context
 *
 * Return: int_ce_ring_mask[context]
 */
int wlan_cfg_get_ce_ring_mask(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx,
		int context);

/**
 * wlan_cfg_get_max_clients() - Return maximum number of peers/stations
 *				supported by device
 * @wlan_cfg_ctx - Configuration Handle
 *
 * Return: max_clients
 */
uint32_t wlan_cfg_get_max_clients(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/**
 * wlan_cfg_max_alloc_size() - Return Maximum allocation size for any dynamic
 *			    memory allocation request for this device
 * @wlan_cfg_ctx - Configuration Handle
 *
 * Return: max_alloc_size
 */
uint32_t wlan_cfg_max_alloc_size(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/*
 * wlan_cfg_per_pdev_tx_ring() - Return true if Tx rings are mapped as
 *			       one per radio
 * @wlan_cfg_ctx - Configuration Handle
 *
 * Return: per_pdev_tx_ring
 */
int wlan_cfg_per_pdev_tx_ring(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/*
 * wlan_cfg_num_tcl_data_rings() - Number of TCL Data rings supported by device
 * @wlan_cfg_ctx
 *
 * Return: num_tcl_data_rings
 */
int wlan_cfg_num_tcl_data_rings(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/*
 * wlan_cfg_per_pdev_rx_ring() - Return true if Rx rings are mapped as
 *                              one per radio
 * @wlan_cfg_ctx
 *
 * Return: per_pdev_rx_ring
 */
int wlan_cfg_per_pdev_rx_ring(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/*
 * wlan_cfg_num_reo_dest_rings() - Number of REO Data rings supported by device
 * @wlan_cfg_ctx - Configuration Handle
 *
 * Return: num_reo_dest_rings
 */
int wlan_cfg_num_reo_dest_rings(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/*
 * wlan_cfg_pkt_type() - Default 802.11 encapsulation type
 * @wlan_cfg_ctx - Configuration Handle
 *
 * Return: htt_pkt_type_ethernet
 */
int wlan_cfg_pkt_type(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/*
 * wlan_cfg_get_num_tx_desc_pool() - Number of Tx Descriptor pools for the
 *					device
 * @wlan_cfg_ctx - Configuration Handle
 *
 * Return: num_tx_desc_pool
 */
int wlan_cfg_get_num_tx_desc_pool(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/*
 * wlan_cfg_get_num_tx_ext_desc_pool() -  Number of Tx MSDU ext Descriptor
 *					pools
 * @wlan_cfg_ctx - Configuration Handle
 *
 * Return: num_tx_ext_desc_pool
 */
int wlan_cfg_get_num_tx_ext_desc_pool(
		struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/*
 * wlan_cfg_get_num_tx_desc() - Number of Tx Descriptors per pool
 * @wlan_cfg_ctx - Configuration Handle
 *
 * Return: num_tx_desc
 */
int wlan_cfg_get_num_tx_desc(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/*
 * wlan_cfg_get_num_tx_ext_desc() - Number of Tx MSDU extension Descriptors
 *					per pool
 * @wlan_cfg_ctx - Configuration Handle
 *
 * Return: num_tx_ext_desc
 */
int wlan_cfg_get_num_tx_ext_desc(struct wlan_cfg_dp_soc_ctxt *wlan_cfg_ctx);

/*
 * wlan_cfg_max_peer_id() - Get maximum peer ID
 * @cfg: Configuration Handle
 *
 * Return: maximum peer ID
 */
uint32_t wlan_cfg_max_peer_id(struct wlan_cfg_dp_soc_ctxt *cfg);

/*
 * wlan_cfg_get_dma_mon_buf_ring_size() - Return Size of monitor buffer ring
 * @wlan_cfg_pdev_ctx
 *
 * Return: dma_mon_buf_ring_size
 */
int wlan_cfg_get_dma_mon_buf_ring_size(
		struct wlan_cfg_dp_pdev_ctxt *wlan_cfg_pdev_ctx);

/*
 * wlan_cfg_get_dma_mon_dest_ring_size() - Return Size of RxDMA Monitor
 *					Destination ring
 * @wlan_cfg_pdev_ctx
 *
 * Return: dma_mon_dest_size
 */
int wlan_cfg_get_dma_mon_dest_ring_size(
		struct wlan_cfg_dp_pdev_ctxt *wlan_cfg_pdev_ctx);

/*
 * wlan_cfg_get_dma_mon_stat_ring_size() - Return size of Monitor Status ring
 * @wlan_cfg_pdev_ctx
 *
 * Return: dma_mon_stat_ring_size
 */
int wlan_cfg_get_dma_mon_stat_ring_size(
		struct wlan_cfg_dp_pdev_ctxt *wlan_cfg_pdev_ctx);

/*
 * wlan_cfg_get_rx_dma_buf_ring_size() - Return Size of RxDMA buffer ring
 * @wlan_cfg_pdev_ctx
 *
 * Return: rx_dma_buf_ring_size
 */
int wlan_cfg_get_rx_dma_buf_ring_size(
		struct wlan_cfg_dp_pdev_ctxt *wlan_cfg_pdev_ctx);

/*
 * wlan_cfg_get_num_mac_rings() - Return the number of MAC RX DMA rings
 * per pdev
 * @wlan_cfg_pdev_ctx
 *
 * Return: number of mac DMA rings per pdev
 */
int wlan_cfg_get_num_mac_rings(struct wlan_cfg_dp_pdev_ctxt *cfg);

/*
 * wlan_cfg_is_lro_enabled - Return LRO enabled/disabled
 * @wlan_cfg_pdev_ctx
 *
 * Return: true - LRO enabled false - LRO disabled
 */
bool wlan_cfg_is_lro_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/*
 * wlan_cfg_is_lro_enabled - Return RX hash enabled/disabled
 * @wlan_cfg_pdev_ctx
 *
 * Return: true - enabled false - disabled
 */
bool wlan_cfg_is_rx_hash_enabled(struct wlan_cfg_dp_soc_ctxt *cfg);

/*
 * wlan_cfg_get_dp_pdev_nss_enabled - Return pdev nss enabled/disabled
 * @wlan_cfg_pdev_ctx
 *
 * Return: 1 - enabled 0 - disabled
 */
int wlan_cfg_get_dp_pdev_nss_enabled(struct wlan_cfg_dp_pdev_ctxt *cfg);

/*
 * wlan_cfg_set_dp_pdev_nss_enabled - set pdev nss enabled/disabled
 * @wlan_cfg_pdev_ctx
 */
void wlan_cfg_set_dp_pdev_nss_enabled(struct wlan_cfg_dp_pdev_ctxt *cfg, int nss_enabled);

/*
 * wlan_cfg_get_dp_soc_nss_cfg - Return soc nss config
 * @wlan_cfg_pdev_ctx
 *
 * Return: nss_cfg
 */
int wlan_cfg_get_dp_soc_nss_cfg(struct wlan_cfg_dp_soc_ctxt *cfg);

/*
 * wlan_cfg_set_dp_soc_nss_cfg - set soc nss config
 * @wlan_cfg_pdev_ctx
 *
 */
void wlan_cfg_set_dp_soc_nss_cfg(struct wlan_cfg_dp_soc_ctxt *cfg, int nss_cfg);

/*
 * wlan_cfg_get_int_batch_threshold_tx - Get interrupt mitigation cfg for Tx
 * @wlan_cfg_soc_ctx
 *
 * Return: Batch threshold
 */
int wlan_cfg_get_int_batch_threshold_tx(struct wlan_cfg_dp_soc_ctxt *cfg);

/*
 * wlan_cfg_get_int_timer_threshold_tx - Get interrupt mitigation cfg for Tx
 * @wlan_cfg_soc_ctx
 *
 * Return: Timer threshold
 */
int wlan_cfg_get_int_timer_threshold_tx(struct wlan_cfg_dp_soc_ctxt *cfg);

/*
 * wlan_cfg_get_int_batch_threshold_rx - Get interrupt mitigation cfg for Rx
 * @wlan_cfg_soc_ctx
 *
 * Return: Batch threshold
 */
int wlan_cfg_get_int_batch_threshold_rx(struct wlan_cfg_dp_soc_ctxt *cfg);

/*
 * wlan_cfg_get_int_batch_threshold_rx - Get interrupt mitigation cfg for Rx
 * @wlan_cfg_soc_ctx
 *
 * Return: Timer threshold
 */
int wlan_cfg_get_int_timer_threshold_rx(struct wlan_cfg_dp_soc_ctxt *cfg);

/*
 * wlan_cfg_get_int_batch_threshold_tx - Get interrupt mitigation cfg for other srngs
 * @wlan_cfg_soc_ctx
 *
 * Return: Batch threshold
 */
int wlan_cfg_get_int_batch_threshold_other(struct wlan_cfg_dp_soc_ctxt *cfg);

/*
 * wlan_cfg_get_int_batch_threshold_tx - Get interrupt mitigation cfg for other srngs
 * @wlan_cfg_soc_ctx
 *
 * Return: Timer threshold
 */
int wlan_cfg_get_int_timer_threshold_other(struct wlan_cfg_dp_soc_ctxt *cfg);

/*
 * wlan_cfg_tx_ring_size - Get Tx DMA ring size (TCL Data Ring)
 * @wlan_cfg_soc_ctx
 *
 * Return: Tx Ring Size
 */
int wlan_cfg_tx_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);

/*
 * wlan_cfg_tx_comp_ring_size - Get Tx completion ring size (WBM Ring)
 * @wlan_cfg_soc_ctx
 *
 * Return: Tx Completion ring size
 */
int wlan_cfg_tx_comp_ring_size(struct wlan_cfg_dp_soc_ctxt *cfg);
#ifdef QCA_LL_TX_FLOW_CONTROL_V2
int wlan_cfg_get_tx_flow_stop_queue_th(struct wlan_cfg_dp_soc_ctxt *cfg);

int wlan_cfg_get_tx_flow_start_queue_offset(struct wlan_cfg_dp_soc_ctxt *cfg);
#endif /* QCA_LL_TX_FLOW_CONTROL_V2 */
#endif
