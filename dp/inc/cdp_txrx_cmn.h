/*
 * Copyright (c) 2011-2018 The Linux Foundation. All rights reserved.
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

/**
 * @file cdp_txrx_cmn.h
 * @brief Define the host data path converged API functions
 * called by the host control SW and the OS interface module
 */
#ifndef _CDP_TXRX_CMN_H_
#define _CDP_TXRX_CMN_H_

#include "qdf_types.h"
#include "qdf_nbuf.h"
#include "cdp_txrx_ops.h"
#include "cdp_txrx_handle.h"
#include "cdp_txrx_cmn_struct.h"
/******************************************************************************
 *
 * Common Data Path Header File
 *
 *****************************************************************************/
#define dp_alert(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_DP, params)
#define dp_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_DP, params)
#define dp_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_DP, params)
#define dp_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_DP, params)
#define dp_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_DP, params)

static inline QDF_STATUS
cdp_soc_attach_target(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return QDF_STATUS_E_INVAL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_soc_attach_target)
		return QDF_STATUS_SUCCESS;

	return soc->ops->cmn_drv_ops->txrx_soc_attach_target(soc);

}

static inline int
cdp_soc_get_nss_cfg(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_soc_get_nss_cfg)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_soc_get_nss_cfg(soc);
}

static inline void
cdp_soc_set_nss_cfg(ol_txrx_soc_handle soc, uint32_t config)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_soc_set_nss_cfg)
		return;

	soc->ops->cmn_drv_ops->txrx_soc_set_nss_cfg(soc, config);
}

static inline struct cdp_vdev *
cdp_vdev_attach(ol_txrx_soc_handle soc, struct cdp_pdev *pdev,
	uint8_t *vdev_mac_addr, uint8_t vdev_id, enum wlan_op_mode op_mode)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_vdev_attach)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_vdev_attach(pdev,
			vdev_mac_addr, vdev_id, op_mode);
}
#ifndef CONFIG_WIN
/**
 * cdp_flow_pool_map() - Create flow pool for vdev
 * @soc - data path soc handle
 * @pdev
 * @vdev_id - vdev_id corresponding to vdev start
 *
 * Create per vdev flow pool.
 *
 * return none
 */
static inline QDF_STATUS cdp_flow_pool_map(ol_txrx_soc_handle soc,
					struct cdp_pdev *pdev, uint8_t vdev_id)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return QDF_STATUS_E_INVAL;
	}

	if (!soc->ops->flowctl_ops ||
	    !soc->ops->flowctl_ops->flow_pool_map_handler)
		return QDF_STATUS_E_INVAL;

	return soc->ops->flowctl_ops->flow_pool_map_handler(soc, pdev, vdev_id);
}

/**
 * cdp_flow_pool_unmap() - Delete flow pool
 * @soc - data path soc handle
 * @pdev
 * @vdev_id - vdev_id corresponding to vdev start
 *
 * Delete flow pool
 *
 * return none
 */
static inline void cdp_flow_pool_unmap(ol_txrx_soc_handle soc,
					struct cdp_pdev *pdev, uint8_t vdev_id)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->flowctl_ops ||
	    !soc->ops->flowctl_ops->flow_pool_unmap_handler)
		return;

	return soc->ops->flowctl_ops->flow_pool_unmap_handler(soc, pdev,
							vdev_id);
}
#endif

static inline void
cdp_vdev_detach(ol_txrx_soc_handle soc, struct cdp_vdev *vdev,
	 ol_txrx_vdev_delete_cb callback, void *cb_context)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_vdev_detach)
		return;

	soc->ops->cmn_drv_ops->txrx_vdev_detach(vdev,
			callback, cb_context);
}

static inline int
cdp_pdev_attach_target(ol_txrx_soc_handle soc, struct cdp_pdev *pdev)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_pdev_attach_target)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_pdev_attach_target(pdev);
}

static inline struct cdp_pdev *cdp_pdev_attach
	(ol_txrx_soc_handle soc, struct cdp_ctrl_objmgr_pdev *ctrl_pdev,
	HTC_HANDLE htc_pdev, qdf_device_t osdev, uint8_t pdev_id)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_pdev_attach)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_pdev_attach(soc, ctrl_pdev,
			htc_pdev, osdev, pdev_id);
}

static inline int cdp_pdev_post_attach(ol_txrx_soc_handle soc,
	struct cdp_pdev *pdev)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_pdev_post_attach)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_pdev_post_attach(pdev);
}

static inline void
cdp_pdev_pre_detach(ol_txrx_soc_handle soc, struct cdp_pdev *pdev, int force)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_pdev_pre_detach)
		return;

	soc->ops->cmn_drv_ops->txrx_pdev_pre_detach(pdev, force);
}

static inline void
cdp_pdev_detach(ol_txrx_soc_handle soc, struct cdp_pdev *pdev, int force)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_pdev_detach)
		return;

	soc->ops->cmn_drv_ops->txrx_pdev_detach(pdev, force);
}

static inline void
cdp_pdev_deinit(ol_txrx_soc_handle soc, struct cdp_pdev *pdev, int force)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_pdev_deinit)
		return;

	soc->ops->cmn_drv_ops->txrx_pdev_deinit(pdev, force);
}

static inline void *cdp_peer_create
	(ol_txrx_soc_handle soc, struct cdp_vdev *vdev,
	uint8_t *peer_mac_addr, struct cdp_ctrl_objmgr_peer *ctrl_peer)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_create)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_peer_create(vdev,
			peer_mac_addr, ctrl_peer);
}

static inline void cdp_peer_setup
	(ol_txrx_soc_handle soc, struct cdp_vdev *vdev, void *peer)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_setup)
		return;

	soc->ops->cmn_drv_ops->txrx_peer_setup(vdev,
			peer);
}

static inline void *cdp_peer_ast_hash_find_soc
	(ol_txrx_soc_handle soc, uint8_t *ast_mac_addr)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_ast_hash_find_soc)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_peer_ast_hash_find_soc(soc,
								  ast_mac_addr);
}

static inline void *cdp_peer_ast_hash_find_by_pdevid
	(ol_txrx_soc_handle soc, uint8_t *ast_mac_addr,
	 uint8_t pdev_id)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_ast_hash_find_by_pdevid)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_peer_ast_hash_find_by_pdevid
					(soc,
					 ast_mac_addr,
					 pdev_id);
}

static inline int cdp_peer_add_ast
	(ol_txrx_soc_handle soc, struct cdp_peer *peer_handle,
	uint8_t *mac_addr, enum cdp_txrx_ast_entry_type type, uint32_t flags)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_add_ast)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_peer_add_ast(soc,
							peer_handle,
							mac_addr,
							type,
							flags);
}

static inline void cdp_peer_reset_ast
	(ol_txrx_soc_handle soc, uint8_t *wds_macaddr, void *vdev_hdl)
{

	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}
	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_reset_ast)
		return;

	soc->ops->cmn_drv_ops->txrx_peer_reset_ast(soc, wds_macaddr, vdev_hdl);
}

static inline void cdp_peer_reset_ast_table
	(ol_txrx_soc_handle soc, void *vdev_hdl)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_reset_ast_table)
		return;

	soc->ops->cmn_drv_ops->txrx_peer_reset_ast_table(soc, vdev_hdl);
}

static inline void cdp_peer_flush_ast_table
	(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_flush_ast_table)
		return;

	soc->ops->cmn_drv_ops->txrx_peer_flush_ast_table(soc);
}

static inline int cdp_peer_update_ast
	(ol_txrx_soc_handle soc, uint8_t *wds_macaddr,
	struct cdp_peer *peer_handle, uint32_t flags)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_update_ast)
		return 0;


	return soc->ops->cmn_drv_ops->txrx_peer_update_ast(soc,
							peer_handle,
							wds_macaddr,
							flags);
}

static inline void cdp_peer_del_ast
	(ol_txrx_soc_handle soc, void *ast_handle)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_del_ast)
		return;

	soc->ops->cmn_drv_ops->txrx_peer_del_ast(soc, ast_handle);
}


static inline uint8_t cdp_peer_ast_get_pdev_id
	(ol_txrx_soc_handle soc, void *ast_handle)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0xff;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_ast_get_pdev_id)
		return 0xff;

	return soc->ops->cmn_drv_ops->txrx_peer_ast_get_pdev_id(soc,
								ast_handle);
}

static inline uint8_t cdp_peer_ast_get_next_hop
	(ol_txrx_soc_handle soc, void *ast_handle)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0xff;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_ast_get_next_hop)
		return 0xff;

	return soc->ops->cmn_drv_ops->txrx_peer_ast_get_next_hop(soc,
								ast_handle);
}

/**
 * cdp_peer_ast_get_type() - Return type (Static, WDS, MEC) of AST entry
 * @soc: DP SoC handle
 * @ast_handle: Opaque handle to AST entry
 *
 * Return: AST entry type (Static/WDS/MEC)
 */
static inline enum cdp_txrx_ast_entry_type cdp_peer_ast_get_type
	(ol_txrx_soc_handle soc, void *ast_handle)

{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_ast_get_type)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_peer_ast_get_type(soc, ast_handle);
}

static inline void cdp_peer_ast_set_type
	(ol_txrx_soc_handle soc, void *ast_handle,
	 enum cdp_txrx_ast_entry_type type)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_ast_set_type)
		return;

	soc->ops->cmn_drv_ops->txrx_peer_ast_set_type(soc, ast_handle, type);
}

#if defined(FEATURE_AST) && defined(AST_HKV1_WORKAROUND)
static inline void cdp_peer_ast_set_cp_ctx(struct cdp_soc_t *soc,
					   void *ast_handle,
					   void *cp_ctx)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "Invalid Instance:");
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_ast_set_cp_ctx)
		return;

	soc->ops->cmn_drv_ops->txrx_peer_ast_set_cp_ctx(soc, ast_handle,
							cp_ctx);
}

static inline void *cdp_peer_ast_get_cp_ctx(struct cdp_soc_t *soc,
					    void *ast_handle)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "Invalid Instance:");
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_ast_get_cp_ctx)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_peer_ast_get_cp_ctx(soc, ast_handle);
}

static inline bool cdp_peer_ast_get_wmi_sent(struct cdp_soc_t *soc,
					     void *ast_handle)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "Invalid Instance:");
		QDF_BUG(0);
		return false;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_ast_get_wmi_sent)
		return false;

	return soc->ops->cmn_drv_ops->txrx_peer_ast_get_wmi_sent(soc,
								 ast_handle);
}

static inline
void cdp_peer_ast_free_entry(struct cdp_soc_t *soc,
			     void *ast_handle)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "Invalid Instance:");
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_ast_free_entry)
		return;

	soc->ops->cmn_drv_ops->txrx_peer_ast_free_entry(soc, ast_handle);
}
#endif

static inline struct cdp_peer *cdp_peer_ast_get_peer
	(ol_txrx_soc_handle soc, void *ast_handle)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_ast_get_peer)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_peer_ast_get_peer(soc, ast_handle);
}

static inline uint32_t cdp_peer_ast_get_nexthop_peer_id
	(ol_txrx_soc_handle soc, void *ast_handle)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return CDP_INVALID_PEER;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_ast_get_nexthop_peer_id)
		return CDP_INVALID_PEER;

	return soc->ops->cmn_drv_ops->txrx_peer_ast_get_nexthop_peer_id
					(soc,
					 ast_handle);
}

static inline void cdp_peer_teardown
	(ol_txrx_soc_handle soc, struct cdp_vdev *vdev, void *peer)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_teardown)
		return;

	soc->ops->cmn_drv_ops->txrx_peer_teardown(vdev, peer);
}

static inline void
cdp_peer_delete(ol_txrx_soc_handle soc, void *peer, uint32_t bitmap)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_peer_delete)
		return;

	soc->ops->cmn_drv_ops->txrx_peer_delete(peer, bitmap);
}

static inline int
cdp_set_monitor_mode(ol_txrx_soc_handle soc, struct cdp_vdev *vdev,
			uint8_t smart_monitor)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_set_monitor_mode)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_set_monitor_mode(vdev,
					smart_monitor);
}

static inline void
cdp_set_curchan(ol_txrx_soc_handle soc,
	struct cdp_pdev *pdev,
	uint32_t chan_mhz)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_set_curchan)
		return;

	soc->ops->cmn_drv_ops->txrx_set_curchan(pdev, chan_mhz);
}

static inline void
cdp_set_privacy_filters(ol_txrx_soc_handle soc, struct cdp_vdev *vdev,
			 void *filter, uint32_t num)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_set_privacy_filters)
		return;

	soc->ops->cmn_drv_ops->txrx_set_privacy_filters(vdev,
			filter, num);
}

static inline int
cdp_set_monitor_filter(ol_txrx_soc_handle soc, struct cdp_pdev *pdev,
		struct cdp_monitor_filter *filter_val)
{
	if (soc->ops->mon_ops->txrx_set_advance_monitor_filter)
		return soc->ops->mon_ops->txrx_set_advance_monitor_filter(pdev,
					filter_val);
	return 0;
}


/******************************************************************************
 * Data Interface (B Interface)
 *****************************************************************************/
static inline void
cdp_vdev_register(ol_txrx_soc_handle soc, struct cdp_vdev *vdev,
	 void *osif_vdev, struct cdp_ctrl_objmgr_vdev *ctrl_vdev,
	 struct ol_txrx_ops *txrx_ops)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_vdev_register)
		return;

	soc->ops->cmn_drv_ops->txrx_vdev_register(vdev,
			osif_vdev, ctrl_vdev, txrx_ops);
}

static inline int
cdp_mgmt_send(ol_txrx_soc_handle soc, struct cdp_vdev *vdev,
	qdf_nbuf_t tx_mgmt_frm,	uint8_t type)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_mgmt_send)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_mgmt_send(vdev,
			tx_mgmt_frm, type);
}

static inline int
cdp_mgmt_send_ext(ol_txrx_soc_handle soc, struct cdp_vdev *vdev,
	 qdf_nbuf_t tx_mgmt_frm, uint8_t type,
	 uint8_t use_6mbps, uint16_t chanfreq)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_mgmt_send_ext)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_mgmt_send_ext
			(vdev, tx_mgmt_frm, type, use_6mbps, chanfreq);
}


static inline void
cdp_mgmt_tx_cb_set(ol_txrx_soc_handle soc, struct cdp_pdev *pdev,
		   uint8_t type, ol_txrx_mgmt_tx_cb download_cb,
		   ol_txrx_mgmt_tx_cb ota_ack_cb, void *ctxt)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_mgmt_tx_cb_set)
		return;

	soc->ops->cmn_drv_ops->txrx_mgmt_tx_cb_set
			(pdev, type, download_cb, ota_ack_cb, ctxt);
}

static inline int cdp_get_tx_pending(ol_txrx_soc_handle soc,
struct cdp_pdev *pdev)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_get_tx_pending)
		return 0;


	return soc->ops->cmn_drv_ops->txrx_get_tx_pending(pdev);
}

static inline void
cdp_data_tx_cb_set(ol_txrx_soc_handle soc, struct cdp_vdev *data_vdev,
		 ol_txrx_data_tx_cb callback, void *ctxt)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_data_tx_cb_set)
		return;

	soc->ops->cmn_drv_ops->txrx_data_tx_cb_set(data_vdev,
			callback, ctxt);
}

/******************************************************************************
 * Statistics and Debugging Interface (C Interface)
 *****************************************************************************/
/**
 * External Device physical address types
 *
 * Currently, both MAC and IPA uController use the same size addresses
 * and descriptors are exchanged between these two depending on the mode.
 *
 * Rationale: qdf_dma_addr_t is the type used internally on the host for DMA
 *            operations. However, external device physical address sizes
 *            may be different from host-specific physical address sizes.
 *            This calls for the following definitions for target devices
 *            (MAC, IPA uc).
 */
#if HTT_PADDR64
typedef uint64_t target_paddr_t;
#else
typedef uint32_t target_paddr_t;
#endif /*HTT_PADDR64 */

static inline int
cdp_aggr_cfg(ol_txrx_soc_handle soc, struct cdp_vdev *vdev,
			 int max_subfrms_ampdu,
			 int max_subfrms_amsdu)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_aggr_cfg)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_aggr_cfg(vdev,
			max_subfrms_ampdu, max_subfrms_amsdu);
}

static inline int
cdp_fw_stats_get(ol_txrx_soc_handle soc, struct cdp_vdev *vdev,
	struct ol_txrx_stats_req *req, bool per_vdev,
	bool response_expected)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_fw_stats_get)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_fw_stats_get(vdev, req,
			per_vdev, response_expected);
}

static inline int
cdp_debug(ol_txrx_soc_handle soc, struct cdp_vdev *vdev, int debug_specs)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_debug)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_debug(vdev, debug_specs);
}

static inline void cdp_fw_stats_cfg(ol_txrx_soc_handle soc,
	 struct cdp_vdev *vdev, uint8_t cfg_stats_type, uint32_t cfg_val)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_fw_stats_cfg)
		return;

	soc->ops->cmn_drv_ops->txrx_fw_stats_cfg(vdev,
			cfg_stats_type, cfg_val);
}

static inline void cdp_print_level_set(ol_txrx_soc_handle soc, unsigned level)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_print_level_set)
		return;

	soc->ops->cmn_drv_ops->txrx_print_level_set(level);
}

static inline uint8_t *
cdp_get_vdev_mac_addr(ol_txrx_soc_handle soc, struct cdp_vdev *vdev)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_get_vdev_mac_addr)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_get_vdev_mac_addr(vdev);

}

/**
 * cdp_get_vdev_struct_mac_addr() - Return handle to struct qdf_mac_addr of
 * vdev
 * @vdev: vdev handle
 *
 * Return: Handle to struct qdf_mac_addr
 */
static inline struct qdf_mac_addr *cdp_get_vdev_struct_mac_addr
	(ol_txrx_soc_handle soc, struct cdp_vdev *vdev)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_get_vdev_struct_mac_addr)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_get_vdev_struct_mac_addr
			(vdev);

}

/**
 * cdp_get_pdev_from_vdev() - Return handle to pdev of vdev
 * @vdev: vdev handle
 *
 * Return: Handle to pdev
 */
static inline struct cdp_pdev *cdp_get_pdev_from_vdev
	(ol_txrx_soc_handle soc, struct cdp_vdev *vdev)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_get_pdev_from_vdev)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_get_pdev_from_vdev(vdev);
}

/**
 * cdp_get_os_rx_handles_from_vdev() - Return os rx handles for a vdev
 * @soc: ol_txrx_soc_handle handle
 * @vdev: vdev for which os rx handles are needed
 * @stack_fn_p: pointer to stack function pointer
 * @osif_handle_p: pointer to ol_osif_vdev_handle
 *
 * Return: void
 */
static inline
void cdp_get_os_rx_handles_from_vdev(ol_txrx_soc_handle soc,
				     struct cdp_vdev *vdev,
				     ol_txrx_rx_fp *stack_fn_p,
				     ol_osif_vdev_handle *osif_handle_p)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_get_os_rx_handles_from_vdev)
		return;

	soc->ops->cmn_drv_ops->txrx_get_os_rx_handles_from_vdev(vdev,
								stack_fn_p,
								osif_handle_p);
}

/**
 * cdp_get_ctrl_pdev_from_vdev() - Return control pdev of vdev
 * @vdev: vdev handle
 *
 * Return: Handle to control pdev
 */
static inline struct cdp_cfg *
cdp_get_ctrl_pdev_from_vdev(ol_txrx_soc_handle soc, struct cdp_vdev *vdev)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_get_ctrl_pdev_from_vdev)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_get_ctrl_pdev_from_vdev
			(vdev);
}

static inline struct cdp_vdev *
cdp_get_vdev_from_vdev_id(ol_txrx_soc_handle soc, struct cdp_pdev *pdev,
		uint8_t vdev_id)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_get_vdev_from_vdev_id)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_get_vdev_from_vdev_id
			(pdev, vdev_id);
}

static inline struct cdp_vdev *
cdp_get_mon_vdev_from_pdev(ol_txrx_soc_handle soc, struct cdp_pdev *pdev)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_get_mon_vdev_from_pdev)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_get_mon_vdev_from_pdev
			(pdev);
}

static inline void
cdp_soc_detach(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_soc_detach)
		return;

	soc->ops->cmn_drv_ops->txrx_soc_detach((void *)soc);
}

/**
 * cdp_soc_init() - Initialize txrx SOC
 * @soc: ol_txrx_soc_handle handle
 * @devid: Device ID
 * @hif_handle: Opaque HIF handle
 * @psoc: Opaque Objmgr handle
 * @htc_handle: Opaque HTC handle
 * @qdf_dev: QDF device
 * @dp_ol_if_ops: Offload Operations
 *
 * Return: DP SOC handle on success, NULL on failure
 */
static inline ol_txrx_soc_handle
cdp_soc_init(ol_txrx_soc_handle soc, u_int16_t devid, void *hif_handle,
	     void *psoc, void *htc_handle, qdf_device_t qdf_dev,
	     struct ol_if_ops *dp_ol_if_ops)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_soc_init)
		return NULL;

	return soc->ops->cmn_drv_ops->txrx_soc_init(soc, psoc,
						    hif_handle,
						    htc_handle, qdf_dev,
						    dp_ol_if_ops, devid);
}

/**
 * cdp_soc_deinit() - Deinitialize txrx SOC
 * @soc: Opaque DP SOC handle
 *
 * Return: None
 */
static inline void
cdp_soc_deinit(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_soc_deinit)
		return;

	soc->ops->cmn_drv_ops->txrx_soc_deinit((void *)soc);
}

/**
 * cdp_tso_soc_attach() - TSO attach function
 * @soc: ol_txrx_soc_handle handle
 *
 * Reserve TSO descriptor buffers
 *
 * Return: QDF_STATUS_SUCCESS on Success or
 * QDF_STATUS_E_FAILURE on failure
 */
static inline QDF_STATUS
cdp_tso_soc_attach(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_tso_soc_attach)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_tso_soc_attach((void *)soc);
}

/**
 * cdp_tso_soc_detach() - TSO detach function
 * @soc: ol_txrx_soc_handle handle
 *
 * Release TSO descriptor buffers
 *
 * Return: QDF_STATUS_SUCCESS on Success or
 * QDF_STATUS_E_FAILURE on failure
 */
static inline QDF_STATUS
cdp_tso_soc_detach(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_tso_soc_detach)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_tso_soc_detach((void *)soc);
}

/**
 * cdp_addba_resp_tx_completion() - Indicate addba response tx
 * completion to dp to change tid state.
 * @soc: soc handle
 * @peer_handle: peer handle
 * @tid: tid
 * @status: Tx completion status
 *
 * Return: success/failure of tid update
 */
static inline int cdp_addba_resp_tx_completion(ol_txrx_soc_handle soc,
					       void *peer_handle,
					       uint8_t tid, int status)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->addba_resp_tx_completion)
		return 0;

	return soc->ops->cmn_drv_ops->addba_resp_tx_completion(peer_handle, tid,
					status);
}

static inline int cdp_addba_requestprocess(ol_txrx_soc_handle soc,
	void *peer_handle, uint8_t dialogtoken, uint16_t tid,
	uint16_t batimeout, uint16_t buffersize, uint16_t startseqnum)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->addba_requestprocess)
		return 0;

	return soc->ops->cmn_drv_ops->addba_requestprocess(peer_handle,
			dialogtoken, tid, batimeout, buffersize, startseqnum);
}

static inline void cdp_addba_responsesetup(ol_txrx_soc_handle soc,
	void *peer_handle, uint8_t tid, uint8_t *dialogtoken,
	uint16_t *statuscode, uint16_t *buffersize, uint16_t *batimeout)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->addba_responsesetup)
		return;

	soc->ops->cmn_drv_ops->addba_responsesetup(peer_handle, tid,
			dialogtoken, statuscode, buffersize, batimeout);
}

static inline int cdp_delba_process(ol_txrx_soc_handle soc,
	void *peer_handle, int tid, uint16_t reasoncode)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->delba_process)
		return 0;

	return soc->ops->cmn_drv_ops->delba_process(peer_handle,
			tid, reasoncode);
}

/**
 * cdp_delba_tx_completion() - Handle delba tx completion
 * to update stats and retry transmission if failed.
 * @soc: soc handle
 * @peer_handle: peer handle
 * @tid: Tid number
 * @status: Tx completion status
 *
 * Return: 0 on Success, 1 on failure
 */

static inline int cdp_delba_tx_completion(ol_txrx_soc_handle soc,
					  void *peer_handle,
					  uint8_t tid, int status)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->delba_tx_completion)
		return 0;

	return soc->ops->cmn_drv_ops->delba_tx_completion(peer_handle,
							  tid, status);
}

static inline void cdp_set_addbaresponse(ol_txrx_soc_handle soc,
	void *peer_handle, int tid, uint16_t statuscode)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->set_addba_response)
		return;

	soc->ops->cmn_drv_ops->set_addba_response(peer_handle, tid, statuscode);
}

/**
 * cdp_get_peer_mac_addr_frm_id: function to return vdev id and and peer
 * mac address
 * @soc: SOC handle
 * @peer_id: peer id of the peer for which mac_address is required
 * @mac_addr: reference to mac address
 *
 * reutm: vdev_id of the vap
 */
static inline uint8_t
cdp_get_peer_mac_addr_frm_id(ol_txrx_soc_handle soc, uint16_t peer_id,
		uint8_t *mac_addr)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return CDP_INVALID_VDEV_ID;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->get_peer_mac_addr_frm_id)
		return CDP_INVALID_VDEV_ID;

	return soc->ops->cmn_drv_ops->get_peer_mac_addr_frm_id(soc,
				peer_id, mac_addr);
}

/**
 * cdp_set_vdev_dscp_tid_map(): function to set DSCP-tid map in the vap
 * @vdev: vdev handle
 * @map_id: id of the tid map
 *
 * Return: void
 */
static inline void cdp_set_vdev_dscp_tid_map(ol_txrx_soc_handle soc,
		struct cdp_vdev *vdev, uint8_t map_id)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->set_vdev_dscp_tid_map)
		return;

	soc->ops->cmn_drv_ops->set_vdev_dscp_tid_map(vdev,
				map_id);
}

/**
 * cdp_ath_get_total_per(): function to get hw retries
 * @soc : soc handle
 * @pdev: pdev handle
 *
 * Return: get hw retries
 */
static inline
int cdp_ath_get_total_per(ol_txrx_soc_handle soc,
			  struct cdp_pdev *pdev)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_get_total_per)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_get_total_per(pdev);
}

/**
 * cdp_set_pdev_dscp_tid_map(): function to change tid values in DSCP-tid map
 * @pdev: pdev handle
 * @map_id: id of the tid map
 * @tos: index value in map that needs to be changed
 * @tid: tid value passed by user
 *
 * Return: void
 */
static inline void cdp_set_pdev_dscp_tid_map(ol_txrx_soc_handle soc,
		struct cdp_pdev *pdev, uint8_t map_id, uint8_t tos, uint8_t tid)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->set_pdev_dscp_tid_map)
		return;

	soc->ops->cmn_drv_ops->set_pdev_dscp_tid_map(pdev,
			map_id, tos, tid);
}

/**
 * cdp_hmmc_tid_override_en(): Function to enable hmmc tid override.
 * @soc : soc handle
 * @pdev: pdev handle
 * @val: hmmc-dscp flag value
 *
 * Return: void
 */
static inline void cdp_hmmc_tid_override_en(ol_txrx_soc_handle soc,
					    struct cdp_pdev *pdev, bool val)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->hmmc_tid_override_en)
		return;

	soc->ops->cmn_drv_ops->hmmc_tid_override_en(pdev, val);
}

/**
 * cdp_set_hmmc_tid_val(): Function to set hmmc tid value.
 * @soc : soc handle
 * @pdev: pdev handle
 * @tid: tid value
 *
 * Return: void
 */
static inline void cdp_set_hmmc_tid_val(ol_txrx_soc_handle soc,
					struct cdp_pdev *pdev, uint8_t tid)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->set_hmmc_tid_val)
		return;

	soc->ops->cmn_drv_ops->set_hmmc_tid_val(pdev, tid);
}

/**
 * cdp_flush_cache_rx_queue() - flush cache rx queue frame
 *
 * Return: None
 */
static inline void cdp_flush_cache_rx_queue(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->flush_cache_rx_queue)
		return;
	soc->ops->cmn_drv_ops->flush_cache_rx_queue();
}

/**
 * cdp_txrx_stats_request(): function to map to host and firmware statistics
 * @soc: soc handle
 * @vdev: virtual device
 * @req: stats request container
 *
 * return: status
 */
static inline
int cdp_txrx_stats_request(ol_txrx_soc_handle soc, struct cdp_vdev *vdev,
		struct cdp_txrx_stats_req *req)
{
	if (!soc || !soc->ops || !soc->ops->cmn_drv_ops || !req) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_ASSERT(0);
		return 0;
	}

	if (soc->ops->cmn_drv_ops->txrx_stats_request)
		return soc->ops->cmn_drv_ops->txrx_stats_request(vdev, req);

	return 0;
}

/**
 * cdp_txrx_intr_attach(): function to attach and configure interrupt
 * @soc: soc handle
 */
static inline QDF_STATUS cdp_txrx_intr_attach(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_intr_attach)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_intr_attach(soc);
}

/**
 * cdp_txrx_intr_detach(): function to detach interrupt
 * @soc: soc handle
 */
static inline void cdp_txrx_intr_detach(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_intr_detach)
		return;

	soc->ops->cmn_drv_ops->txrx_intr_detach(soc);
}

/**
 * cdp_display_stats(): function to map to dump stats
 * @soc: soc handle
 * @value: statistics option
 */
static inline QDF_STATUS
cdp_display_stats(ol_txrx_soc_handle soc, uint16_t value,
		  enum qdf_stats_verbosity_level level)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->display_stats)
		return 0;

	return soc->ops->cmn_drv_ops->display_stats(soc, value, level);
}


/**
  * cdp_set_pn_check(): function to set pn check
  * @soc: soc handle
  * @sec_type: security type
  * #rx_pn: receive pn
  */
static inline int cdp_set_pn_check(ol_txrx_soc_handle soc,
	struct cdp_vdev *vdev, struct cdp_peer *peer_handle, enum cdp_sec_type sec_type,  uint32_t *rx_pn)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->set_pn_check)
		return 0;

	soc->ops->cmn_drv_ops->set_pn_check(vdev, peer_handle,
			sec_type, rx_pn);
	return 0;
}

static inline int cdp_set_key(ol_txrx_soc_handle soc,
			      struct cdp_peer *peer_handle,
			      bool is_unicast, uint32_t *key)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->ctrl_ops ||
	    !soc->ops->ctrl_ops->set_key)
		return 0;

	soc->ops->ctrl_ops->set_key(peer_handle,
			is_unicast, key);
	return 0;
}

/**
 * cdp_update_config_parameters(): function to propagate configuration
 *                                 parameters to datapath
 * @soc: opaque soc handle
 * @cfg: configuration handle
 *
 * Return: status: 0 - Success, non-zero: Failure
 */
static inline
QDF_STATUS cdp_update_config_parameters(ol_txrx_soc_handle soc,
	struct cdp_config_params *cfg)
{
	struct cdp_soc *psoc = (struct cdp_soc *)soc;

	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->update_config_parameters)
		return QDF_STATUS_SUCCESS;

	return soc->ops->cmn_drv_ops->update_config_parameters(psoc,
								cfg);
}

/**
 * cdp_pdev_get_dp_txrx_handle() - get advanced dp handle from pdev
 * @soc: opaque soc handle
 * @pdev: data path pdev handle
 *
 * Return: opaque dp handle
 */
static inline void *
cdp_pdev_get_dp_txrx_handle(ol_txrx_soc_handle soc, void *pdev)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (soc->ops->cmn_drv_ops->get_dp_txrx_handle)
		return soc->ops->cmn_drv_ops->get_dp_txrx_handle(pdev);

	return 0;
}

/**
 * cdp_pdev_set_dp_txrx_handle() - set advanced dp handle in pdev
 * @soc: opaque soc handle
 * @pdev: data path pdev handle
 * @dp_hdl: opaque pointer for dp_txrx_handle
 *
 * Return: void
 */
static inline void
cdp_pdev_set_dp_txrx_handle(ol_txrx_soc_handle soc, void *pdev, void *dp_hdl)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
			!soc->ops->cmn_drv_ops->set_dp_txrx_handle)
		return;

	soc->ops->cmn_drv_ops->set_dp_txrx_handle(pdev, dp_hdl);
}

/*
 * cdp_soc_get_dp_txrx_handle() - get extended dp handle from soc
 * @soc: opaque soc handle
 *
 * Return: opaque extended dp handle
 */
static inline void *
cdp_soc_get_dp_txrx_handle(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return NULL;
	}

	if (soc->ops->cmn_drv_ops->get_soc_dp_txrx_handle)
		return soc->ops->cmn_drv_ops->get_soc_dp_txrx_handle(
				(struct cdp_soc *) soc);

	return NULL;
}

/**
 * cdp_soc_set_dp_txrx_handle() - set advanced dp handle in soc
 * @soc: opaque soc handle
 * @dp_hdl: opaque pointer for dp_txrx_handle
 *
 * Return: void
 */
static inline void
cdp_soc_set_dp_txrx_handle(ol_txrx_soc_handle soc, void *dp_handle)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
			!soc->ops->cmn_drv_ops->set_soc_dp_txrx_handle)
		return;

	soc->ops->cmn_drv_ops->set_soc_dp_txrx_handle((struct cdp_soc *)soc,
			dp_handle);
}

/**
 * cdp_tx_send() - enqueue frame for transmission
 * @soc: soc opaque handle
 * @vdev: VAP device
 * @nbuf: nbuf to be enqueued
 *
 * This API is used by Extended Datapath modules to enqueue frame for
 * transmission
 *
 * Return: void
 */
static inline void
cdp_tx_send(ol_txrx_soc_handle soc, struct cdp_vdev *vdev, qdf_nbuf_t nbuf)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
				"%s: Invalid Instance:", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
			!soc->ops->cmn_drv_ops->tx_send)
		return;

	soc->ops->cmn_drv_ops->tx_send(vdev, nbuf);
}

/*
 * cdp_get_pdev_id_frm_pdev() - return pdev_id from pdev
 * @soc: opaque soc handle
 * @pdev: data path pdev handle
 *
 * Return: pdev_id
 */
static inline
uint8_t cdp_get_pdev_id_frm_pdev(ol_txrx_soc_handle soc,
	struct cdp_pdev *pdev)
{
	if (soc->ops->cmn_drv_ops->txrx_get_pdev_id_frm_pdev)
		return soc->ops->cmn_drv_ops->txrx_get_pdev_id_frm_pdev(pdev);
	return 0;
}

/**
 * cdp_pdev_set_chan_noise_floor() - Set channel noise floor to DP layer
 * @soc: opaque soc handle
 * @pdev: data path pdev handle
 * @chan_noise_floor: Channel Noise Floor (in dbM) obtained from control path
 *
 * Return: None
 */
static inline
void cdp_pdev_set_chan_noise_floor(ol_txrx_soc_handle soc,
				   struct cdp_pdev *pdev,
				   int16_t chan_noise_floor)
{
	if (soc->ops->cmn_drv_ops->txrx_pdev_set_chan_noise_floor)
		return soc->ops->cmn_drv_ops->txrx_pdev_set_chan_noise_floor(
				pdev, chan_noise_floor);
}

/**
 * cdp_set_nac() - set nac
 * @soc: opaque soc handle
 * @peer: data path peer handle
 *
 */
static inline
void cdp_set_nac(ol_txrx_soc_handle soc,
	struct cdp_peer *peer)
{
	if (soc->ops->cmn_drv_ops->txrx_set_nac)
		soc->ops->cmn_drv_ops->txrx_set_nac(peer);
}

/**
 * cdp_set_pdev_tx_capture() - set pdev tx_capture
 * @soc: opaque soc handle
 * @pdev: data path pdev handle
 * @val: value of pdev_tx_capture
 *
 * Return: void
 */
static inline
void cdp_set_pdev_tx_capture(ol_txrx_soc_handle soc,
		struct cdp_pdev *pdev, int val)
{
	if (soc->ops->cmn_drv_ops->txrx_set_pdev_tx_capture)
		return soc->ops->cmn_drv_ops->txrx_set_pdev_tx_capture(pdev,
				val);

}

/**
 * cdp_get_peer_mac_from_peer_id() - get peer mac addr from peer id
 * @soc: opaque soc handle
 * @pdev: data path pdev handle
 * @peer_id: data path peer id
 * @peer_mac: peer_mac
 *
 * Return: void
 */
static inline
void cdp_get_peer_mac_from_peer_id(ol_txrx_soc_handle soc,
	struct cdp_pdev *pdev_handle,
	uint32_t peer_id, uint8_t *peer_mac)
{
	if (soc->ops->cmn_drv_ops->txrx_get_peer_mac_from_peer_id)
		soc->ops->cmn_drv_ops->txrx_get_peer_mac_from_peer_id(
				pdev_handle, peer_id, peer_mac);
}

/**
 * cdp_vdev_tx_lock() - acquire lock
 * @soc: opaque soc handle
 * @vdev: data path vdev handle
 *
 * Return: void
 */
static inline
void cdp_vdev_tx_lock(ol_txrx_soc_handle soc,
	struct cdp_vdev *vdev)
{
	if (soc->ops->cmn_drv_ops->txrx_vdev_tx_lock)
		soc->ops->cmn_drv_ops->txrx_vdev_tx_lock(vdev);
}

/**
 * cdp_vdev_tx_unlock() - release lock
 * @soc: opaque soc handle
 * @vdev: data path vdev handle
 *
 * Return: void
 */
static inline
void cdp_vdev_tx_unlock(ol_txrx_soc_handle soc,
	struct cdp_vdev *vdev)
{
	if (soc->ops->cmn_drv_ops->txrx_vdev_tx_unlock)
		soc->ops->cmn_drv_ops->txrx_vdev_tx_unlock(vdev);
}

/**
 * cdp_ath_getstats() - get updated athstats
 * @soc: opaque soc handle
 * @dev: dp interface handle
 * @stats: cdp network device stats structure
 * @type: device type pdev/vdev
 *
 * Return: void
 */
static inline void cdp_ath_getstats(ol_txrx_soc_handle soc,
		void *dev, struct cdp_dev_stats *stats,
		uint8_t type)
{
	if (soc && soc->ops && soc->ops->cmn_drv_ops->txrx_ath_getstats)
		soc->ops->cmn_drv_ops->txrx_ath_getstats(dev, stats, type);
}

/**
 * cdp_set_gid_flag() - set groupid flag
 * @soc: opaque soc handle
 * @pdev: data path pdev handle
 * @mem_status: member status from grp management frame
 * @user_position: user position from grp management frame
 *
 * Return: void
 */
static inline
void cdp_set_gid_flag(ol_txrx_soc_handle soc,
		struct cdp_pdev *pdev, u_int8_t *mem_status,
		u_int8_t *user_position)
{
	if (soc->ops->cmn_drv_ops->txrx_set_gid_flag)
		soc->ops->cmn_drv_ops->txrx_set_gid_flag(pdev, mem_status, user_position);
}

/**
 * cdp_fw_supported_enh_stats_version() - returns the fw enhanced stats version
 * @soc: opaque soc handle
 * @pdev: data path pdev handle
 *
 */
static inline
uint32_t cdp_fw_supported_enh_stats_version(ol_txrx_soc_handle soc,
		struct cdp_pdev *pdev)
{
	if (soc->ops->cmn_drv_ops->txrx_fw_supported_enh_stats_version)
		return soc->ops->cmn_drv_ops->txrx_fw_supported_enh_stats_version(pdev);
	return 0;
}

/**
 * cdp_get_pdev_id_frm_pdev() - return pdev_id from pdev
 * @soc: opaque soc handle
 * @ni: associated node
 * @force: number of frame in SW queue
 * Return: void
 */
static inline
void cdp_if_mgmt_drain(ol_txrx_soc_handle soc,
		void *ni, int force)
{
	if (soc->ops->cmn_drv_ops->txrx_if_mgmt_drain)
		soc->ops->cmn_drv_ops->txrx_if_mgmt_drain(ni, force);
}

/* cdp_peer_map_attach() - CDP API to allocate PEER map memory
 * @soc: opaque soc handle
 * @max_peers: number of peers created in FW
 * @peer_map_unmap_v2: flag indicates HTT peer map v2 is enabled in FW
 *
 *
 * Return: void
 */
static inline void
cdp_peer_map_attach(ol_txrx_soc_handle soc, uint32_t max_peers,
		    bool peer_map_unmap_v2)
{
	if (soc && soc->ops && soc->ops->cmn_drv_ops &&
	    soc->ops->cmn_drv_ops->txrx_peer_map_attach)
		soc->ops->cmn_drv_ops->txrx_peer_map_attach(soc, max_peers,
							    peer_map_unmap_v2);
}

/**

 * cdp_pdev_set_ctrl_pdev() - set UMAC ctrl pdev to dp pdev
 * @soc: opaque soc handle
 * @pdev: opaque dp pdev handle
 * @ctrl_pdev: opaque ctrl pdev handle
 *
 * Return: void
 */
static inline void
cdp_pdev_set_ctrl_pdev(ol_txrx_soc_handle soc, struct cdp_pdev *dp_pdev,
		       struct cdp_ctrl_objmgr_pdev *ctrl_pdev)
{
	if (soc && soc->ops && soc->ops->cmn_drv_ops &&
	    soc->ops->cmn_drv_ops->txrx_pdev_set_ctrl_pdev)
		soc->ops->cmn_drv_ops->txrx_pdev_set_ctrl_pdev(dp_pdev,
							       ctrl_pdev);
}

/* cdp_txrx_classify_and_update() - To classify the packet and update stats
 * @soc: opaque soc handle
 * @vdev: opaque dp vdev handle
 * @skb: data
 * @dir: rx or tx packet
 * @nbuf_classify: packet classification object
 *
 * Return: 1 on success else return 0
 */
static inline int
cdp_txrx_classify_and_update(ol_txrx_soc_handle soc,
			     struct cdp_vdev *vdev, qdf_nbuf_t skb,
			     enum txrx_direction dir,
			     struct ol_txrx_nbuf_classify *nbuf_class)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance", __func__);
		QDF_BUG(0);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_classify_update)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_classify_update(vdev,
							   skb,
							   dir, nbuf_class);
}

/**
 * cdp_get_dp_capabilities() - get DP capabilities
 * @soc: opaque soc handle
 * @dp_cap: enum of DP capabilities
 *
 * Return: bool
 */
static inline bool
cdp_get_dp_capabilities(struct cdp_soc_t *soc, enum cdp_capabilities dp_caps)
{
	if (soc && soc->ops && soc->ops->cmn_drv_ops &&
	    soc->ops->cmn_drv_ops->get_dp_capabilities)
		return soc->ops->cmn_drv_ops->get_dp_capabilities(soc, dp_caps);
	return false;
}

#ifdef RECEIVE_OFFLOAD
/**
 * cdp_register_rx_offld_flush_cb() - register LRO/GRO flush cb function pointer
 * @soc - data path soc handle
 * @pdev - device instance pointer
 *
 * register rx offload flush callback function pointer
 *
 * return none
 */
static inline void cdp_register_rx_offld_flush_cb(ol_txrx_soc_handle soc,
						  void (rx_ol_flush_cb)(void *))
{
	if (!soc || !soc->ops || !soc->ops->rx_offld_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return;
	}

	if (soc->ops->rx_offld_ops->register_rx_offld_flush_cb)
		return soc->ops->rx_offld_ops->register_rx_offld_flush_cb(
								rx_ol_flush_cb);
}

/**
 * cdp_deregister_rx_offld_flush_cb() - deregister Rx offld flush cb function
 * @soc - data path soc handle
 *
 * deregister rx offload flush callback function pointer
 *
 * return none
 */
static inline void cdp_deregister_rx_offld_flush_cb(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops || !soc->ops->rx_offld_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return;
	}

	if (soc->ops->rx_offld_ops->deregister_rx_offld_flush_cb)
		return soc->ops->rx_offld_ops->deregister_rx_offld_flush_cb();
}
#endif /* RECEIVE_OFFLOAD */

/**
 * @cdp_set_ba_timeout() - set ba aging timeout per AC
 *
 * @soc - pointer to the soc
 * @value - timeout value in millisec
 * @ac - Access category
 *
 * @return - void
 */
static inline void cdp_set_ba_timeout(ol_txrx_soc_handle soc,
				      uint8_t ac, uint32_t value)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_set_ba_aging_timeout)
		return;

	soc->ops->cmn_drv_ops->txrx_set_ba_aging_timeout(soc, ac, value);
}

/**
 * @cdp_get_ba_timeout() - return ba aging timeout per AC
 *
 * @soc - pointer to the soc
 * @ac - access category
 * @value - timeout value in millisec
 *
 * @return - void
 */
static inline void cdp_get_ba_timeout(ol_txrx_soc_handle soc,
				      uint8_t ac, uint32_t *value)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance", __func__);
		QDF_BUG(0);
		return;
	}

	if (!soc->ops->cmn_drv_ops ||
	    !soc->ops->cmn_drv_ops->txrx_get_ba_aging_timeout)
		return;

	soc->ops->cmn_drv_ops->txrx_get_ba_aging_timeout(soc, ac, value);
}

/**
 * cdp_cfg_get() - get cfg for dp enum
 *
 * @soc: pointer to the soc
 * @cfg: cfg enum
 *
 * Return - cfg value
 */
static inline uint32_t cdp_cfg_get(ol_txrx_soc_handle soc, enum cdp_dp_cfg cfg)
{
	if (!soc || !soc->ops) {
		QDF_TRACE(QDF_MODULE_ID_CDP, QDF_TRACE_LEVEL_DEBUG,
			  "%s: Invalid Instance", __func__);
		return 0;
	}

	if (!soc->ops->cmn_drv_ops || !soc->ops->cmn_drv_ops->txrx_get_cfg)
		return 0;

	return soc->ops->cmn_drv_ops->txrx_get_cfg(soc, cfg);
}
#endif /* _CDP_TXRX_CMN_H_ */
