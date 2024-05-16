/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: Define PSOC MLME public APIs
 */

#ifndef _WLAN_PSOC_MLME_API_H_
#define _WLAN_PSOC_MLME_API_H_

#include <include/wlan_psoc_mlme.h>

#ifdef WLAN_FEATURE_PEER_TRANS_HIST
#define MAX_PEER_HIST_LIST_SIZE 256
#endif

/**
 * wlan_psoc_mlme_get_cmpt_obj() - Returns PSOC MLME component object
 * @psoc: PSOC object
 *
 * Retrieves MLME component object from PSOC object
 *
 * Return: comp handle on SUCCESS
 *         NULL, if it fails to retrieve
 */
struct psoc_mlme_obj *wlan_psoc_mlme_get_cmpt_obj(
						struct wlan_objmgr_psoc *psoc);

#ifdef WLAN_FEATURE_PEER_TRANS_HIST
/**
 * wlan_mlme_psoc_init_peer_trans_history() - Initialize PSOC peer trans history
 * @psoc_mlme: PSOC MLME priv object
 *
 * Return: void
 */
static inline void
wlan_mlme_psoc_init_peer_trans_history(struct psoc_mlme_obj *psoc_mlme)
{
	qdf_list_create(&psoc_mlme->peer_history_list, 0);
}

/**
 * wlan_mlme_psoc_peer_trans_hist_remove_back() - Remove entry from peer history
 * @peer_history: Peer history list
 *
 * Removes one entry from back in the @peer_history list and free the memory
 *
 * Returns: void
 */
void wlan_mlme_psoc_peer_trans_hist_remove_back(qdf_list_t *peer_history);

/**
 * wlan_mlme_psoc_peer_tbl_trans_add_entry() - Add entry to peer history table
 * @psoc: PSOC object manager pointer
 * @peer_trans_entry: Entry to add.
 *
 * Adds the entry pointed by @peer_trans_entry to peer_history table in PSOC
 * MLME. The new node is added to the front and if the list size becomes more
 * than MAX_PEER_HIST_LIST_SIZE, then entry existing at the back of the list
 * will be flushed before adding this entry.
 *
 * Caller to take care of mem_free of buffer pointed by @peer_trans_entry
 * in case of error.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_psoc_peer_tbl_trans_add_entry(struct wlan_objmgr_psoc *psoc,
					struct wlan_peer_tbl_trans_entry *peer_trans_entry);

/**
 * wlan_mlme_psoc_flush_peer_trans_history() - Flush the peer trans table
 * @psoc: PSOC object manager pointer
 *
 * Flush all the entries of peer transition table and free the memory.
 */
void wlan_mlme_psoc_flush_peer_trans_history(struct wlan_objmgr_psoc *psoc);
#else
static inline void
wlan_mlme_psoc_init_peer_trans_history(struct psoc_mlme_obj *psoc_mlme)
{
}

static inline void
wlan_mlme_psoc_flush_peer_trans_history(struct wlan_objmgr_psoc *psoc)
{
}
#endif
/**
 * wlan_psoc_mlme_get_ext_hdl() - Returns legacy handle
 * @psoc: PSOC object
 *
 * Retrieves legacy handle from psoc mlme component object
 *
 * Return: legacy handle on SUCCESS
 *         NULL, if it fails to retrieve
 */
mlme_psoc_ext_t *wlan_psoc_mlme_get_ext_hdl(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_psoc_mlme_set_ext_hdl() - Set legacy handle
 * @psoc_mlme: psoc_mlme object
 * @psoc_ext_hdl: PSOC level legacy handle
 *
 * Sets legacy handle in psoc mlme component object
 *
 * Return: Void
 */
void wlan_psoc_mlme_set_ext_hdl(struct psoc_mlme_obj *psoc_mlme,
				mlme_psoc_ext_t *psoc_ext_hdl);

/**
 * wlan_psoc_set_phy_config() - Init psoc phy related configs
 * @psoc: pointer to psoc object
 * @phy_config: phy related configs score config
 *
 * Return: void
 */
void wlan_psoc_set_phy_config(struct wlan_objmgr_psoc *psoc,
			      struct psoc_phy_config *phy_config);

/**
 * mlme_psoc_open() - MLME component Open
 * @psoc: pointer to psoc object
 *
 * Open the MLME component and initialize the MLME structure
 *
 * Return: QDF Status
 */
QDF_STATUS mlme_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * mlme_psoc_close() - MLME component close
 * @psoc: pointer to psoc object
 *
 * Open the MLME component and initialize the MLME structure
 *
 * Return: QDF Status
 */
QDF_STATUS mlme_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_psoc_mlme_get_11be_capab() - Get the 11be capability for target
 * @psoc: psoc handle
 * @val: pointer to the output variable
 *
 * return: QDF_STATUS
 */
QDF_STATUS
wlan_psoc_mlme_get_11be_capab(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_psoc_mlme_set_11be_capab() - Set the 11be capability for target
 * @psoc: psoc handle
 * @val: pointer to the output variable
 *
 * return: QDF_STATUS
 */
QDF_STATUS
wlan_psoc_mlme_set_11be_capab(struct wlan_objmgr_psoc *psoc, bool val);
#endif
