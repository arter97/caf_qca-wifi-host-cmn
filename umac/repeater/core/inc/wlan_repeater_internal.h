/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
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
 * DOC: This file has main repeater structures.
 */

#ifndef _WLAN_REPEATER_INTERNAL_H_
#define _WLAN_REPEATER_INTERNAL_H_

#include <wlan_repeater_api.h>

#define RPTR_MAX_RADIO_CNT 3

#define RPTR_GLOBAL_LOCK(_x)    qdf_spin_lock_bh(_x)
#define RPTR_GLOBAL_UNLOCK(_x)  qdf_spin_unlock_bh(_x)

#define RPTR_PDEV_LOCK(_x)    qdf_spin_lock_bh(_x)
#define RPTR_PDEV_UNLOCK(_x)  qdf_spin_unlock_bh(_x)

#if REPEATER_SAME_SSID
#define ROOTAP_ACCESS_MASK 0x0F
#define STAVAP_CONNECTION_MASK 0xF0

/**
 * AP preference type for same ssid feature
 * 0 - No preference
 * 1 - connect to RootAP
 * 2 - connect to RE, whose bssid matches with preferred_list_stavap
 */
enum wlan_rptr_ap_preference_type {
    ap_preference_type_init = 0,
    ap_preference_type_root = 1,
    ap_preference_type_rptr = 2,
};

/**
 * struct wlan_rptr_same_ssid_feature - reapeter same ssid feature
 * @extender_info:          extender info
 *  0x00->no stavap connection and RootAP access on Repeater AP
 *  0x0F->invalid state
 *  0xF0->stavap connection is there,but no rootap access on Repeater AP
 *  0xFF->stavap connection and rootap access is there on Repeater AP
 * @num_rptr_clients:       num_rptr_clients
 * @preferred_bssid_list:   list of AP bssid mac to which RE STAVAP
 *                          has to connect
 * @denied_client_list:     list of RE STAVAP mac to which AP has to deauth
 * @ap_preference:          ap_preference
 * @same_ssid_disable:      disable same ssid feature
 * @rootap_access_downtime: rootap access downtime
 */
typedef struct wlan_rptr_same_ssid_feature {
	u8      extender_info;
	u8      num_rptr_clients;
	u8      preferred_bssid_list[RPTR_MAX_RADIO_CNT][QDF_MAC_ADDR_SIZE];
	u8      denied_client_list[RPTR_MAX_RADIO_CNT][QDF_MAC_ADDR_SIZE];
	enum wlan_rptr_ap_preference_type ap_preference;
	bool    same_ssid_disable;
	systime_t    rootap_access_downtime;
} wlan_rptr_same_ssid_feature_t;
#endif

/**
 * struct wlan_rptr_global_priv - reapeter global priv structure
 * @global_feature_caps:    global feature caps
 * @ss_info:                same ssid info
 * @preferred_mlo_bssid     preferred mlo bssid
 * @num_stavaps_up:         number stavaps up
 * @disconnect_timeout:     Interface manager app uses this timeout to bring
 *                          down AP VAPs after STAVAP disconnection
 * @reconfiguration_timeout:Interface manager app uses this timeout to bring
 *                          down AP VAPs after STAVAP disconnection
 * @ext_cbacks:             legacy extended callbacks
 * @rptr_global_lock:       rptr global private spinlock
 */
struct wlan_rptr_global_priv {
	u32 global_feature_caps;
#if REPEATER_SAME_SSID
	wlan_rptr_same_ssid_feature_t   ss_info;
	u8     preferred_mlo_bssid[QDF_MAC_ADDR_SIZE];
#endif
	u8     num_stavaps_up;
	u16    disconnect_timeout;
	u16    reconfiguration_timeout;
	struct rptr_ext_cbacks ext_cbacks;
	qdf_spinlock_t  rptr_global_lock;
};

/**
 * struct wlan_rptr_psoc_priv - reapeter psoc priv structure
 * @psoc:                     objmgr psoc
 * @psoc_feature_caps:        psoc feature caps
 */
struct wlan_rptr_psoc_priv {
	struct wlan_objmgr_psoc *psoc;
	u32 psoc_feature_caps;
};

#if REPEATER_SAME_SSID
/**
 * Extender connection type for same ssid feature
 * 0 - init value
 * 1 - connecting RE has no RootAP access
 * 2 - connecting RE has RootAP access
 */
enum wlan_rptr_ext_connection_type {
    ext_connection_type_init = 0,
    ext_connection_type_no_root_access = 1,
    ext_connection_type_root_access = 2,
};
#endif

/**
 * struct wlan_rptr_move- Repeater move structure
 * @state: repeater move state
 * @ssid: repeater move ssid
 * @chan: repeater move channel
 * @same_chan: repeater move same channel
 * @bssid: repeater move bssid
 */
struct wlan_rptr_move {
	enum wlan_rptr_move_state state;
	struct wlan_ssid ssid;
	u_int32_t chan;
	bool same_chan;
	struct qdf_mac_addr bssid;
};

#ifdef CONFIG_AFC_SUPPORT
/**
 * struct afc_list_node - afc list node structure
 * @power_mode:               power mode info of ap
 * @bssid:                    ap bssid
 */
struct wlan_rptr_afc_list_node {
	qdf_list_node_t node;
	u8 power_mode;
	u8 bssid[QDF_MAC_ADDR_SIZE];
};
#endif

/**
 * struct wlan_rptr_pdev_priv - reapeter pdev priv structure
 * @pdev:                     objmgr pdev
 * @pdev_feature_caps:        pdev feature caps
 * @extender_connection:      extender connection
 * @preferred_bssid:          preferred bssid for same ssid feature
 * @preferredUplink:          preferred uplink for Tri-radio fastlane feature
 * @nscanpsta:                number scan psta
 * @afc_list:                 afc list
 * @rptr_pdev_lock:           rptr pdev private spinlock
 * @sta_vdev:                 rptr sta vdev
 */
struct wlan_rptr_pdev_priv {
	struct wlan_objmgr_pdev  *pdev;
	u32    pdev_feature_caps;
#if REPEATER_SAME_SSID
	enum wlan_rptr_ext_connection_type  extender_connection;
	u8     preferred_bssid[QDF_MAC_ADDR_SIZE];
#endif
	u8     preferredUplink;
	u8     nscanpsta;
#ifdef CONFIG_AFC_SUPPORT
	qdf_list_t afc_list;
#endif
	qdf_spinlock_t  rptr_pdev_lock;
	struct wlan_rptr_move rptr_move;
#ifdef WLAN_FEATURE_11BE_MLO
	struct wlan_rptr_vdev_priv *sta_vdev;
#endif
};

/**
 * struct wlan_rptr_vdev_priv - reapeter vdev priv structure
 * @vdev:                     objmgr vdev
 * @vdev_feature_caps:        vdev feature caps
 * @vdev_allow_events:        vdev allow events
 * @vdev_scan_id:             vdev scan ID
 */
struct wlan_rptr_vdev_priv {
	struct wlan_objmgr_vdev *vdev;
	u32    vdev_feature_caps;
	u8     vdev_allow_events;
	uint32_t vdev_scan_id;
};

/**
 * struct wlan_rptr_peer_priv - reapeter peer priv structure
 * @peer:                     objmgr peer
 * @is_extender_client:       is extender client for same ssid feature
 */
struct wlan_rptr_peer_priv {
	struct wlan_objmgr_peer *peer;
#if REPEATER_SAME_SSID
	u8     is_extender_client;
#endif
};

QDF_STATUS
wlan_rptr_core_register_ext_cb(struct rptr_ext_cbacks *ext_cbacks);
void wlan_rptr_core_vdev_set_feat_cap(struct wlan_objmgr_vdev *vdev,
				      uint32_t cap);
void wlan_rptr_core_vdev_clear_feat_cap(struct wlan_objmgr_vdev *vdev,
					uint32_t cap);
uint8_t wlan_rptr_core_vdev_is_feat_cap_set(struct wlan_objmgr_vdev *vdev,
					 uint32_t cap);
void wlan_rptr_core_pdev_set_feat_cap(struct wlan_objmgr_pdev *pdev,
				      uint32_t cap);
void wlan_rptr_core_pdev_clear_feat_cap(struct wlan_objmgr_pdev *pdev,
					uint32_t cap);
uint8_t wlan_rptr_core_pdev_is_feat_cap_set(struct wlan_objmgr_pdev *pdev,
					 uint32_t cap);
void wlan_rptr_core_psoc_set_feat_cap(struct wlan_objmgr_psoc *psoc,
				      uint32_t cap);
void wlan_rptr_core_psoc_clear_feat_cap(struct wlan_objmgr_psoc *psoc,
					uint32_t cap);
uint8_t wlan_rptr_core_psoc_is_feat_cap_set(struct wlan_objmgr_psoc *psoc,
					 uint32_t cap);
void wlan_rptr_core_global_set_feat_cap(uint32_t cap);
void wlan_rptr_core_global_clear_feat_cap(uint32_t cap);
uint8_t wlan_rptr_core_global_is_feat_cap_set(uint32_t cap);
void wlan_rptr_core_reset_pdev_flags(struct wlan_objmgr_pdev *pdev);
void wlan_rptr_core_reset_global_flags(void);
struct wlan_rptr_global_priv *wlan_rptr_get_global_ctx(void);
#if REPEATER_SAME_SSID
void wlan_rptr_core_global_same_ssid_disable_set(u_int32_t value);
void wlan_rptr_core_global_same_ssid_disable_get(uint32_t *value);
void wlan_rptr_core_ss_parse_scan_entries(struct wlan_objmgr_vdev *vdev,
					  struct scan_event *event);
#endif
#ifdef CONFIG_AFC_SUPPORT
void
wlan_rptr_afc_core_parse_scan_entries(struct wlan_objmgr_vdev *vdev,
				      struct scan_event *event);
QDF_STATUS
wlan_rptr_afc_core_get_ap_power_mode(struct wlan_objmgr_vdev *vdev,
				     uint8_t *bssid, uint8_t *pwr_mode);
#endif
void
wlan_rptr_core_pdev_pref_uplink_set(struct wlan_objmgr_pdev *pdev,
				    u32 value);
void
wlan_rptr_core_pdev_pref_uplink_get(struct wlan_objmgr_pdev *pdev,
				    u32 *val);
#ifdef WLAN_FEATURE_11BE_MLO
struct wlan_objmgr_vdev *
wlan_rptr_core_pdev_get_stavdev(struct wlan_objmgr_pdev *pdev);
#endif
void
wlan_rptr_core_global_disconnect_timeout_set(u_int32_t value);
void
wlan_rptr_core_global_disconnect_timeout_get(uint32_t *value);
void
wlan_rptr_core_global_reconfig_timeout_set(u_int32_t value);
void
wlan_rptr_core_global_reconfig_timeout_get(uint32_t *value);
#if REPEATER_SAME_SSID
QDF_STATUS
wlan_rptr_core_validate_stavap_bssid(struct wlan_objmgr_vdev *vdev,
				     u_int8_t *bssid);
#endif
struct wlan_rptr_pdev_priv *
wlan_rptr_get_pdev_priv(struct wlan_objmgr_pdev *pdev);
struct wlan_rptr_psoc_priv *
wlan_rptr_get_psoc_priv(struct wlan_objmgr_psoc *psoc);
struct wlan_rptr_vdev_priv *
wlan_rptr_get_vdev_priv(struct wlan_objmgr_vdev *vdev);
struct wlan_rptr_peer_priv *
wlan_rptr_get_peer_priv(struct wlan_objmgr_peer *peer);
#endif
