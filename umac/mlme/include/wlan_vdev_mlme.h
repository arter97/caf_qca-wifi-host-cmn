/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
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
 * DOC: Define VDEV MLME structure and APIs
 */
#ifndef _WLAN_VDEV_MLME_H_
#define _WLAN_VDEV_MLME_H_

#include <wlan_vdev_mgr_tgt_if_rx_defs.h>

struct vdev_mlme_obj;

/* Requestor ID for multiple vdev restart */
#define MULTIPLE_VDEV_RESTART_REQ_ID 0x1234

/* values for vdev_type */
#define WLAN_VDEV_MLME_TYPE_AP   0x1
#define WLAN_VDEV_MLME_TYPE_STA  0x2
#define WLAN_VDEV_MLME_TYPE_IBSS 0x3
#define WLAN_VDEV_MLME_TYPE_MONITOR 0x4

/* values for vdev_subtype */
#define WLAN_VDEV_MLME_SUBTYPE_P2P_DEVICE 0x1
#define WLAN_VDEV_MLME_SUBTYPE_P2P_CLIENT 0x2
#define WLAN_VDEV_MLME_SUBTYPE_P2P_GO 0x3
#define WLAN_VDEV_MLME_SUBTYPE_PROXY_STA 0x4
#define WLAN_VDEV_MLME_SUBTYPE_MESH 0x5

/* vdev control flags (per bits) */
#define WLAN_VDEV_MLME_FLAGS_NON_MBSSID_AP      0x00000001
#define WLAN_VDEV_MLME_FLAGS_TRANSMIT_AP        0x00000002
#define WLAN_VDEV_MLME_FLAGS_NON_TRANSMIT_AP    0x00000004

/**
 * struct vdev_mlme_proto_generic - generic mlme proto structure
 * sent in frames
 * @dtim_period: frequency of data transmissions per beacon 1-255
 * @slot_time: slot time
 * @protection_mode: rts cts protection mode
 * @beacon_interval: beacon interval
 * @ldpc: low density parity check value
 * @nss: number of spatial stream
 * @nss_2g: 2.4GHz number of spatial stream
 * @nss_5g: 5GHz number of spatial stream
 * @tsfadjust: adjusted timer sync value
 */
struct vdev_mlme_proto_generic {
	uint8_t dtim_period;
	uint32_t slot_time;
	uint32_t protection_mode;
	uint16_t beacon_interval;
	uint8_t ldpc;
	uint8_t nss;
	uint8_t nss_2g;
	uint8_t nss_5g;
	uint64_t tsfadjust;
};

/**
 * struct vdev_mlme_proto_ap - ap specific mlme protocol
 * @.
 */
struct vdev_mlme_proto_ap {
};

/**
 * struct vdev_mlme_proto_sta - sta specific mlme protocol
 * @assoc_id: association id of station
 * @uapsd_cfg: uapsd configuration
 */
struct vdev_mlme_proto_sta {
	uint16_t assoc_id;
	uint16_t uapsd_cfg;
};

/**
 * struct vdev_mlme_proto_bss_color - bss color cfg
 * @flags: proposed for future use cases, currently not used.
 * @evt_type: bss color collision event.
 * @current_bss_color: current bss color.
 * @detection_period_ms: scan interval for both AP and STA mode.
 * @scan_period_ms: scan period for passive scan to detect collision.
 * @free_slot_expiry_time_ms: FW to notify host at timer expiry after
 *                            which Host will disable the bss color.
 */
struct vdev_mlme_proto_bss_color {
	uint32_t flags;
	uint8_t  evt_type;
	uint32_t current_bss_color;
	uint32_t detection_period_ms;
	uint32_t scan_period_ms;
	uint32_t free_slot_expiry_time_ms;
};

/**
 * struct vdev_mlme_vht_info - vdev VHT information
 * @caps: vht capabilities
 * @subfer: su beam former capability
 * @subfee: su beam formee capability
 * @mubfer: mu beam former capability
 * @mubfee: mu beam formee capability
 * @implicit_bf: Implicit BF support
 * @sounding_dimension: Beamformer number of sounding dimension
 * @en_2gvht: 256 qam status
 * @allow_vht: vht capability status
 */
struct vdev_mlme_vht_info {
	uint32_t caps;
	uint8_t  subfer;
	uint8_t  mubfer;
	uint8_t  subfee;
	uint8_t  mubfee;
	uint8_t  implicit_bf;
	uint8_t  sounding_dimension;
	uint8_t  en_2gvht;
	bool     allow_vht;
};

/**
 * struct vdev_mlme_ht_info - vdev HT information
 * @ht_caps: HT capabilities
 * @allow_ht: HT capability status
 */
struct vdev_mlme_ht_info {
	uint32_t ht_caps;
	bool     allow_ht;
};

/**
 * struct vdev_mlme_he_ops_info - vdev mlme HEOPS information
 * @he_ops: he ops
 */
struct vdev_mlme_he_ops_info {
	uint32_t he_ops;
};

/**
 * struct vdev_mlme_he_ops_info - vdev protocol structure holding information
 * that is used in frames
 * @generic: generic protocol information
 * @ap: ap specific protocol information
 * @sta: sta specific protocol information
 * @vht_info: vht information
 * @ht_info: ht capabilities information
 * @he_ops_info: he ops information
 * @bss_color: 11ax HE BSS Color information
 */
struct vdev_mlme_proto {
	struct vdev_mlme_proto_generic generic;
	struct vdev_mlme_proto_ap ap;
	struct vdev_mlme_proto_sta sta;
	struct vdev_mlme_vht_info vht_info;
	struct vdev_mlme_ht_info ht_info;
	struct vdev_mlme_he_ops_info he_ops_info;
	struct vdev_mlme_proto_bss_color bss_color;
};

/**
 * struct vdev_mlme_mgmt_generic - generic vdev mlme mgmt cfg
 * @rts_threshold: RTS threshold
 * @frag_threshold: Fragmentation threshold
 * @probe_delay: time in msec for delaying to send first probe request
 * @repeat_probe_time: probe request transmission time
 * @drop_unencry: drop unencrypted status
 * @ tx_pwrlimit: Tx power limit
 * @tx_power: Tx power
 * @minpower: Min power
 * @maxpower: Max power
 * @maxregpower: max regulatory power
 * @antennamax: max antenna
 * @ampdu: ampdu limit
 * @amsdu: amsdu limit
 * @ssid: service set identifier
 * @ssid_len: ssid length
 * @type: vdev type
 * @sub_type: vdev subtype
 * @rx_decap_type: rx decap type
 * @tx_decap_type: tx decap type
 * @disable_hw_ack: disable ha ack flag
 * @chan_flags: channel flags
 * @chan_flags_ext: extended channel flags
 * @cfreq0: channel frequency index0
 * @cfreq1: channel frequency index1
 * @dfs_set: DFS status flag
 * @dfs_set_cfreq2: DFS status channel index
 * @is_chan_passive: channel passive indication flag
 * @reg_class_id: reg domain class id
 * @is_11ax_stub_enabled: 11AX stub status indication flag
 * @nss_2g: 2G spatial streams
 * @nss_5g: 5G spatial streams
 * @bssid: bssid
 */
struct vdev_mlme_mgmt_generic {
	uint32_t rts_threshold;
	uint32_t frag_threshold;
	uint32_t probe_delay;
	uint32_t repeat_probe_time;
	uint32_t drop_unencry;
	uint32_t tx_pwrlimit;
	uint8_t tx_power;
	uint8_t minpower;
	uint8_t maxpower;
	uint8_t maxregpower;
	uint8_t antennamax;
	uint8_t ampdu;
	uint8_t amsdu;
	char ssid[WLAN_SSID_MAX_LEN + 1];
	uint8_t ssid_len;
	uint8_t type;
	uint8_t subtype;
	uint8_t rx_decap_type;
	uint8_t tx_decap_type;
	bool disable_hw_ack;
	uint64_t chan_flags;
	uint16_t chan_flags_ext;
	uint32_t cfreq1;
	uint32_t cfreq2;
	bool dfs_set;
	bool dfs_set_cfreq2;
	bool is_chan_passive;
	uint8_t reg_class_id;
	uint8_t is_11ax_stub_enabled;
	uint8_t nss_2g;
	uint8_t nss_5g;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
};

/**
 * struct vdev_mlme_mgmt_ap - ap specific vdev mlme mgmt cfg
 * @.
 */
struct vdev_mlme_mgmt_ap {
};

/**
 * struct vdev_mlme_mgmt_sta - sta specific vdev mlme mgmt cfg
 * @.
 */
struct vdev_mlme_mgmt_sta {
};

/**
 * struct vdev_mlme_inactivity_params - vdev mlme inactivity parameters
 * @bmiss_first_bcnt: bmiss first time
 * @bmiss_final_bcnt: bmiss final time
 * @keepalive_min_idle_inactive_time_secs: min time AP consider STA to be
 * inactive
 * @keepalive_max_idle_inactive_time_secs: max inactive idle time for AP to send
 * data-null
 * @keepalive_max_unresponsive_time_secs: max time to send WMI_STA_KICKOUT
 */
struct vdev_mlme_inactivity_params {
	uint32_t bmiss_first_bcnt;
	uint32_t bmiss_final_bcnt;
	uint32_t keepalive_min_idle_inactive_time_secs;
	uint32_t keepalive_max_idle_inactive_time_secs;
	uint32_t keepalive_max_unresponsive_time_secs;
};

/**
 * struct vdev_mlme_rate_info - vdev mlme rate information
 * @rate_flags: dynamic bandwidth info
 * @per_band_tx_mgmt_rate: per band Tx mgmt rate
 * @max_rate: max bandwidth rate
 * @tx_mgmt_rate: Tx Mgmt rate
 * @bcn_tx_rate: beacon Tx rate
 * @type: Type of ratemask configuration
 * @lower32: Lower 32 bits in the 1st 64-bit value
 * @higher32: Higher 32 bits in the 1st 64-bit value
 * @lower32_2: Lower 32 bits in the 2nd 64-bit value
 * @half_rate: Half rate
 * @quarter_rate: quarter rate
 */
struct vdev_mlme_rate_info {
	uint32_t rate_flags;
	uint32_t per_band_tx_mgmt_rate;
	uint32_t max_rate;
	uint32_t tx_mgmt_rate;
	uint32_t bcn_tx_rate;
	uint8_t  type;
	uint32_t lower32;
	uint32_t higher32;
	uint32_t lower32_2;
	bool     half_rate;
	bool     quarter_rate;
};

/**
 * struct vdev_mlme_chainmask_info - vdev mlme chainmask information
 * @tx_chainmask: Tx chainmask
 * @rx_chainmask: Rx Chainmask
 * @num_rx_chain: Num of bits set in Rx chain
 * @num_tx_chain: Num of bits set in Tx chain
 */
struct vdev_mlme_chainmask_info {
	uint8_t tx_chainmask;
	uint8_t rx_chainmask;
	uint8_t num_rx_chain;
	uint8_t num_tx_chain;
};

/**
 * struct vdev_mlme_powersave_info - vdev mlme powersave information
 * @packet_powersave: packet powersave
 * @max_li_of_moddtim: max mod dtim
 * @dyndtim_cnt: dynamic dtim count
 * @listen_interval: listen interval
 * @moddtim_cnt: mod dtim count
 */
struct vdev_mlme_powersave_info {
	uint32_t packet_powersave;
	uint32_t max_li_of_moddtim;
	uint32_t dyndtim_cnt;
	uint32_t listen_interval;
	uint32_t moddtim_cnt;
};

/**
 * struct vdev_mlme_beacon_info - vdev mlme beacon information
 * @beacon_buffer: buffer allocated for beacon frame
 * @beacon_offsets: beacon IE's offsets
 */
struct vdev_mlme_beacon_info {
	qdf_nbuf_t beacon_buffer;
	void *beacon_offsets;
};

/**
 * struct vdev_mlme_mbss_11ax - mbss 11ax fields required for up cmd
 * @profile_idx: profile index of the connected non-trans ap (mbssid case).
 *              0  means invalid.
 * @profile_num: the total profile numbers of non-trans aps (mbssid
 * case).
 *              0 means non-MBSS AP.
 * @mbssid-flags: MBSS IE flags indicating vdev type
 * @vdevid_trans: id of transmitting vdev for MBSS IE
 * @trans_bssid: bssid of transmitted AP (MBSS IE case)
 */
struct vdev_mlme_mbss_11ax {
	uint32_t profile_idx;
	uint32_t profile_num;
	uint32_t mbssid_flags;
	uint8_t vdevid_trans;
	uint8_t trans_bssid[QDF_MAC_ADDR_SIZE];
};

/**
 * struct vdev_mlme_mgmt - vdev mlme mgmt related cfg
 * @generic: generic mgmt information
 * @ap: ap specific mgmt information
 * @sta: sta specific mgmt information
 * @inactivity_params: inactivity parameters
 * @rate_info: bandwidth rate information
 * @chainmask_info: Chainmask information
 * @powersave_info: Power save parameters
 * @beacon_info: beacon buffer information
 * @mbss_11ax: MBSS 11ax information
 */
struct vdev_mlme_mgmt {
	struct vdev_mlme_mgmt_generic generic;
	struct vdev_mlme_mgmt_ap ap;
	struct vdev_mlme_mgmt_sta sta;
	struct vdev_mlme_inactivity_params inactivity_params;
	struct vdev_mlme_rate_info rate_info;
	struct vdev_mlme_chainmask_info chainmask_info;
	struct vdev_mlme_powersave_info powersave_info;
	struct vdev_mlme_beacon_info beacon_info;
	struct vdev_mlme_mbss_11ax mbss_11ax;
};

/**
 * enum beacon_update_op - Beacon update op type
 * @BEACON_INIT:      Initialize beacon
 * @BEACON_REINIT:    Re-initialize beacon
 * @BEACON_UPDATE:    Update dynamic fields of beacon
 * @BEACON_CSA:       Enable CSA IE
 * @BEACON_FREE:      Beacon buffer free
 */
enum beacon_update_op {
	BEACON_INIT,
	BEACON_REINIT,
	BEACON_UPDATE,
	BEACON_CSA,
	BEACON_FREE,
};

/**
 * enum vdev_cmd_type - Command type
 * @START_REQ:      Start request
 * @RESTART_REQ:    Restart request
 * @STOP_REQ: STOP request
 * @DELETE_REQ: DELETE request
 */
enum vdev_cmd_type {
	START_REQ,
	RESTART_REQ,
	STOP_REQ,
	DELETE_REQ,
};

/**
 * enum vdev_start_resp_type - start respone type
 * @START_RESPONSE:  Start response
 * @RESTART_RESPONSE: Restart response
 */
enum vdev_start_resp_type {
	START_RESPONSE = 0,
	RESTART_RESPONSE,
};

/**
 * enum vdev_rsp_type - Response type
 * @START_RSP:      Start Response
 * @RESTART_RSP:    Restart Response
 * @STOP_RSP: STOP Response
 * @DELETE_RSP: DELETE Response
 */
enum vdev_rsp_type {
	START_RSP = 0,
	RESTART_RSP,
	STOP_RSP,
	DELETE_RSP,
};

/**
 * struct vdev_mlme_ops - VDEV MLME operation callbacks structure
 * @mlme_vdev_validate_basic_params:    callback to validate VDEV basic params
 * @mlme_vdev_reset_proto_params:       callback to Reset protocol params
 * @mlme_vdev_start_send:               callback to initiate actions of VDEV
 *                                      MLME start operation
 * @mlme_vdev_restart_send:             callback to initiate actions of VDEV
 *                                      MLME restart operation
 * @mlme_vdev_stop_start_send:          callback to block start/restart VDEV
 *                                      request command
 * @mlme_vdev_start_continue:           callback to initiate operations on
 *                                      LMAC/FW start response
 * @mlme_vdev_up_send:                  callback to initiate actions of VDEV
 *                                      MLME up operation
 * @mlme_vdev_notify_up_complete:       callback to notify VDEV MLME on moving
 *                                      to UP state
 * @mlme_vdev_notify_roam_start:        callback to initiate roaming
 * @mlme_vdev_update_beacon:            callback to initiate beacon update
 * @mlme_vdev_disconnect_peers:         callback to initiate disconnection of
 *                                      peers
 * @mlme_vdev_dfs_cac_timer_stop:       callback to stop the DFS CAC timer
 * @mlme_vdev_stop_send:                callback to initiate actions of VDEV
 *                                      MLME stop operation
 * @mlme_vdev_stop_continue:            callback to initiate operations on
 *                                      LMAC/FW stop response
 * @mlme_vdev_bss_peer_delete_continue: callback to initiate operations on BSS
 *                                      peer delete completion
 * @mlme_vdev_down_send:                callback to initiate actions of VDEV
 *                                      MLME down operation
 */
struct vdev_mlme_ops {
	QDF_STATUS (*mlme_vdev_validate_basic_params)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_reset_proto_params)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_start_send)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_restart_send)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_stop_start_send)(
				struct vdev_mlme_obj *vdev_mlme,
				enum vdev_cmd_type type,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_start_continue)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_sta_conn_start)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_start_req_failed)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_up_send)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_notify_up_complete)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_notify_roam_start)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_update_beacon)(
				struct vdev_mlme_obj *vdev_mlme,
				enum beacon_update_op op,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_disconnect_peers)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_dfs_cac_timer_stop)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_stop_send)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_stop_continue)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_down_send)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_notify_down_complete)(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data);
	QDF_STATUS (*mlme_vdev_ext_delete_rsp)(
				struct vdev_mlme_obj *vdev_mlme,
				struct vdev_delete_response *rsp);
	QDF_STATUS (*mlme_vdev_ext_stop_rsp)(
				struct vdev_mlme_obj *vdev_mlme,
				struct vdev_stop_response *rsp);
	QDF_STATUS (*mlme_vdev_ext_start_rsp)(
				struct vdev_mlme_obj *vdev_mlme,
				struct vdev_start_response *rsp);
};

/**
 * struct vdev_mlme_obj - VDEV MLME component object
 * @proto: VDEV MLME proto substructure
 * @mgmt: VDEV MLME mgmt substructure
 * @sm_lock:              VDEV SM lock
 * @vdev_cmd_lock:        VDEV MLME command atomicity
 * @sm_hdl:               VDEV SM handle
 * @vdev: Pointer to vdev objmgr
 * @ops:                  VDEV MLME callback table
 * @ext_vdev_ptr:         VDEV MLME legacy pointer
 * @vdev_rt: VDEV response timer
 */
struct vdev_mlme_obj {
	struct vdev_mlme_proto proto;
	struct vdev_mlme_mgmt  mgmt;
#ifdef VDEV_SM_LOCK_SUPPORT
	qdf_spinlock_t sm_lock;
	qdf_mutex_t vdev_cmd_lock;
#endif
	struct wlan_sm *sm_hdl;
	struct wlan_objmgr_vdev *vdev;
	struct vdev_mlme_ops *ops;
	void *ext_vdev_ptr;
	struct vdev_response_timer vdev_rt;
};

/**
 * mlme_vdev_validate_basic_params - Validate basic params
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API validate MLME VDEV basic parameters
 *
 * Return: SUCCESS on successful validation
 *         FAILURE, if any parameter is not initialized
 */
static inline QDF_STATUS mlme_vdev_validate_basic_params(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_validate_basic_params)
		ret = vdev_mlme->ops->mlme_vdev_validate_basic_params(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_reset_proto_params - Reset VDEV protocol params
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API resets the protocol params fo vdev
 *
 * Return: SUCCESS on successful reset
 *         FAILURE, if it fails due to any
 */
static inline QDF_STATUS mlme_vdev_reset_proto_params(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_reset_proto_params)
		ret = vdev_mlme->ops->mlme_vdev_reset_proto_params(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_start_send - Invokes VDEV start operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API invokes VDEV start operation
 *
 * Return: SUCCESS on successful completion of start operation
 *         FAILURE, if it fails due to any
 */
static inline QDF_STATUS mlme_vdev_start_send(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_start_send)
		ret = vdev_mlme->ops->mlme_vdev_start_send(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_restart_send - Invokes VDEV restart operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API invokes VDEV restart operation
 *
 * Return: SUCCESS on successful completion of restart operation
 *         FAILURE, if it fails due to any
 */
static inline QDF_STATUS mlme_vdev_restart_send(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_restart_send)
		ret = vdev_mlme->ops->mlme_vdev_restart_send(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_stop_start_send - Invoke block VDEV restart operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @restart: restart req/start req
 * @event_data_len: data size
 * @event_data: event data
 *
 * API invokes stops pending VDEV restart operation
 *
 * Return: SUCCESS alsways
 */
static inline QDF_STATUS mlme_vdev_stop_start_send(
				struct vdev_mlme_obj *vdev_mlme,
				uint8_t restart,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_stop_start_send)
		ret = vdev_mlme->ops->mlme_vdev_stop_start_send(
				vdev_mlme, restart, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_start_continue - VDEV start response handling
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API invokes VDEV start response actions
 *
 * Return: SUCCESS on successful completion of start response operation
 *         FAILURE, if it fails due to any
 */
static inline QDF_STATUS mlme_vdev_start_continue(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_start_continue)
		ret = vdev_mlme->ops->mlme_vdev_start_continue(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_start_req_failed - Invoke Station VDEV connection, if it pause
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API invokes on START fail response
 *
 * Return: SUCCESS on successful invocation of callback
 *         FAILURE, if it fails due to any
 */
static inline QDF_STATUS mlme_vdev_start_req_failed(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_start_req_failed)
		ret = vdev_mlme->ops->mlme_vdev_start_req_failed(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_sta_conn_start - Invoke Station VDEV connection, if it pause
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API invokes connection SM to start station connection
 *
 * Return: SUCCESS on successful invocation of connection sm
 *         FAILURE, if it fails due to any
 */
static inline QDF_STATUS mlme_vdev_sta_conn_start(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_sta_conn_start)
		ret = vdev_mlme->ops->mlme_vdev_sta_conn_start(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_up_send - VDEV up operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API invokes VDEV up operations
 *
 * Return: SUCCESS on successful completion of up operation
 *         FAILURE, if it fails due to any
 */
static inline QDF_STATUS mlme_vdev_up_send(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_up_send)
		ret = vdev_mlme->ops->mlme_vdev_up_send(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_notify_up_complete - VDEV up state transition notification
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API notifies MLME on moving to UP state
 *
 * Return: SUCCESS on successful completion of up notification
 *         FAILURE, if it fails due to any
 */
static inline
QDF_STATUS mlme_vdev_notify_up_complete(struct vdev_mlme_obj *vdev_mlme,
					uint16_t event_data_len,
					void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (vdev_mlme->ops && vdev_mlme->ops->mlme_vdev_notify_up_complete)
		ret = vdev_mlme->ops->mlme_vdev_notify_up_complete(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_notify_roam_start - VDEV Roaming notification
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_len: data size
 * @event_data: event data
 *
 * API notifies MLME on roaming
 *
 * Return: SUCCESS on successful completion of up notification
 *         FAILURE, if it fails due to any
 */
static inline
QDF_STATUS mlme_vdev_notify_roam_start(struct vdev_mlme_obj *vdev_mlme,
				       uint16_t event_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (vdev_mlme->ops && vdev_mlme->ops->mlme_vdev_notify_roam_start)
		ret = vdev_mlme->ops->mlme_vdev_notify_roam_start(vdev_mlme,
								  event_len,
								  event_data);

	return ret;
}

/**
 * mlme_vdev_update_beacon - Updates beacon
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @op: beacon update type
 * @event_data_len: data size
 * @event_data: event data
 *
 * API updates/allocates/frees the beacon
 *
 * Return: SUCCESS on successful update of beacon
 *         FAILURE, if it fails due to any
 */
static inline
QDF_STATUS mlme_vdev_update_beacon(struct vdev_mlme_obj *vdev_mlme,
				   enum beacon_update_op op,
				   uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if (vdev_mlme->ops && vdev_mlme->ops->mlme_vdev_update_beacon)
		ret = vdev_mlme->ops->mlme_vdev_update_beacon(vdev_mlme, op,
						event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_disconnect_peers - Disconnect peers
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API trigger stations disconnection with AP VDEV or AP disconnection with STA
 * VDEV
 *
 * Return: SUCCESS on successful invocation of station disconnection
 *         FAILURE, if it fails due to any
 */
static inline QDF_STATUS mlme_vdev_disconnect_peers(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_disconnect_peers)
		ret = vdev_mlme->ops->mlme_vdev_disconnect_peers(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_dfs_cac_timer_stop - Stop CAC timer
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API stops the CAC timer through DFS API
 *
 * Return: SUCCESS on successful CAC timer stop
 *         FAILURE, if it fails due to any
 */
static inline QDF_STATUS mlme_vdev_dfs_cac_timer_stop(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_dfs_cac_timer_stop)
		ret = vdev_mlme->ops->mlme_vdev_dfs_cac_timer_stop(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_stop_send - Invokes VDEV stop operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API invokes VDEV stop operation
 *
 * Return: SUCCESS on successful completion of stop operation
 *         FAILURE, if it fails due to any
 */
static inline QDF_STATUS mlme_vdev_stop_send(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_stop_send)
		ret = vdev_mlme->ops->mlme_vdev_stop_send(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_stop_continue - VDEV stop response handling
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API invokes VDEV stop response actions
 *
 * Return: SUCCESS on successful completion of stop response operation
 *         FAILURE, if it fails due to any
 */
static inline QDF_STATUS mlme_vdev_stop_continue(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_stop_continue)
		ret = vdev_mlme->ops->mlme_vdev_stop_continue(vdev_mlme,
							      event_data_len,
							      event_data);

	return ret;
}

/**
 * mlme_vdev_down_send - VDEV down operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API invokes VDEV down operation
 *
 * Return: SUCCESS on successful completion of VDEV down operation
 *         FAILURE, if it fails due to any
 */
static inline QDF_STATUS mlme_vdev_down_send(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_down_send)
		ret = vdev_mlme->ops->mlme_vdev_down_send(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

/**
 * mlme_vdev_notify_down_complete - VDEV init state transition notification
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @event_data_len: data size
 * @event_data: event data
 *
 * API notifies MLME on moving to INIT state
 *
 * Return: SUCCESS on successful completion of down notification
 *         FAILURE, if it fails due to any
 */
static inline QDF_STATUS mlme_vdev_notify_down_complete(
				struct vdev_mlme_obj *vdev_mlme,
				uint16_t event_data_len, void *event_data)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	if ((vdev_mlme->ops) && vdev_mlme->ops->mlme_vdev_notify_down_complete)
		ret = vdev_mlme->ops->mlme_vdev_notify_down_complete(
					vdev_mlme, event_data_len, event_data);

	return ret;
}

#endif
