/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
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

/*
 * DOC: csr_inside_api.h
 *
 * Define interface only used by CSR.
 */
#ifndef CSR_INSIDE_API_H__
#define CSR_INSIDE_API_H__

#include "csr_support.h"
#include "sme_inside.h"
#include "cds_reg_service.h"
#include "wlan_objmgr_vdev_obj.h"

#ifdef QCA_WIFI_3_0_EMU
#define CSR_ACTIVE_SCAN_LIST_CMD_TIMEOUT (1000*30*20)
#else
#define CSR_ACTIVE_SCAN_LIST_CMD_TIMEOUT (1000*30)
#endif
/* ***************************************************************************
 * The MAX BSSID Count should be lower than the command timeout value.
 * As in some case auth timeout can take upto 5 sec (in case of SAE auth) try
 * (command timeout/5000 - 1) candidates.
 * ***************************************************************************/
#define CSR_MAX_BSSID_COUNT     (SME_ACTIVE_LIST_CMD_TIMEOUT_VALUE/5000) - 1
#define CSR_CUSTOM_CONC_GO_BI    100
extern uint8_t csr_wpa_oui[][CSR_WPA_OUI_SIZE];
bool csr_is_supported_channel(struct mac_context *mac, uint32_t chan_freq);

#ifndef FEATURE_CM_ENABLE
enum csr_scancomplete_nextcommand {
	eCsrNextScanNothing,
	eCsrNexteScanForSsidSuccess,
	eCsrNexteScanForSsidFailure,
	eCsrNextCheckAllowConc,
};
#endif

enum csr_roamcomplete_result {
#ifndef FEATURE_CM_ENABLE
	eCsrJoinSuccess,
#endif
	eCsrJoinFailure,
	eCsrReassocSuccess,
	eCsrReassocFailure,
	eCsrNothingToJoin,
	eCsrStartBssSuccess,
	eCsrStartBssFailure,
#ifndef FEATURE_CM_ENABLE
	eCsrSilentlyStopRoamingSaveState,
#endif
	eCsrJoinFailureDueToConcurrency,
	eCsrStopBssSuccess,
	eCsrStopBssFailure,
};

struct tag_csrscan_result {
	tListElem Link;
	/* Preferred Encryption type that matched with profile. */
	eCsrEncryptionType ucEncryptionType;
	eCsrEncryptionType mcEncryptionType;
	/* Preferred auth type that matched with the profile. */
	enum csr_akm_type authType;
	int  bss_score;
	uint8_t retry_count;

	tCsrScanResultInfo Result;
	/*
	 * WARNING - Do not add any element here
	 * This member Result must be the last in the structure because the end
	 * of struct bss_description (inside) is an array with nonknown size at
	 * this time.
	 */
};

struct scan_result_list {
	tDblLinkList List;
	tListElem *pCurEntry;
};

#define CSR_IS_ENC_TYPE_STATIC(encType) ((eCSR_ENCRYPT_TYPE_NONE == (encType)) \
			|| (eCSR_ENCRYPT_TYPE_WEP40_STATICKEY == (encType)) || \
			(eCSR_ENCRYPT_TYPE_WEP104_STATICKEY == (encType)))

#define CSR_IS_AUTH_TYPE_FILS(auth_type) \
		((eCSR_AUTH_TYPE_FILS_SHA256 == auth_type) || \
		(eCSR_AUTH_TYPE_FILS_SHA384 == auth_type) || \
		(eCSR_AUTH_TYPE_FT_FILS_SHA256 == auth_type) || \
		(eCSR_AUTH_TYPE_FT_FILS_SHA384 == auth_type))
#define CSR_IS_WAIT_FOR_KEY(mac, sessionId) \
		 (CSR_IS_ROAM_JOINED(mac, sessionId) && \
		  CSR_IS_ROAM_SUBSTATE_WAITFORKEY(mac, sessionId))
/* WIFI has a test case for not using HT rates with TKIP as encryption */
/* We may need to add WEP but for now, TKIP only. */

#define CSR_IS_11n_ALLOWED(encType) ((eCSR_ENCRYPT_TYPE_TKIP != (encType)) && \
			(eCSR_ENCRYPT_TYPE_WEP40_STATICKEY != (encType)) && \
			(eCSR_ENCRYPT_TYPE_WEP104_STATICKEY != (encType)) && \
				     (eCSR_ENCRYPT_TYPE_WEP40 != (encType)) && \
				       (eCSR_ENCRYPT_TYPE_WEP104 != (encType)))

#ifndef FEATURE_CM_ENABLE
#define CSR_IS_DISCONNECT_COMMAND(pCommand) ((eSmeCommandRoam == \
		(pCommand)->command) && \
		((eCsrForcedDisassoc == (pCommand)->u.roamCmd.roamReason) || \
		(eCsrForcedDeauth == (pCommand)->u.roamCmd.roamReason) || \
					(eCsrSmeIssuedDisassocForHandoff == \
					(pCommand)->u.roamCmd.roamReason) || \
					(eCsrForcedDisassocMICFailure == \
					(pCommand)->u.roamCmd.roamReason)))
#else
#define CSR_IS_DISCONNECT_COMMAND(pCommand) false
#endif

enum csr_roam_state csr_roam_state_change(struct mac_context *mac,
					  enum csr_roam_state NewRoamState,
					  uint8_t sessionId);
void csr_roaming_state_msg_processor(struct mac_context *mac, void *msg_buf);
void csr_roam_joined_state_msg_processor(struct mac_context *mac,
					 void *msg_buf);

#ifndef FEATURE_CM_ENABLE
void csr_scan_callback(struct wlan_objmgr_vdev *vdev,
				struct scan_event *event, void *arg);
QDF_STATUS csr_roam_save_connected_bss_desc(struct mac_context *mac,
					    uint32_t sessionId,
					    struct bss_description *bss_desc);
#endif
void csr_release_command_roam(struct mac_context *mac, tSmeCmd *pCommand);
void csr_release_command_wm_status_change(struct mac_context *mac,
					  tSmeCmd *pCommand);

QDF_STATUS csr_roam_copy_profile(struct mac_context *mac,
				 struct csr_roam_profile *pDstProfile,
				 struct csr_roam_profile *pSrcProfile,
				 uint8_t vdev_id);
QDF_STATUS csr_scan_open(struct mac_context *mac);
QDF_STATUS csr_scan_close(struct mac_context *mac);

#ifndef FEATURE_CM_ENABLE
bool
csr_scan_append_bss_description(struct mac_context *mac,
				struct bss_description *pSirBssDescription);

QDF_STATUS csr_scan_for_ssid(struct mac_context *mac, uint32_t sessionId,
			     struct csr_roam_profile *pProfile, uint32_t roamId,
			     bool notify);
#endif
/**
 * csr_scan_abort_mac_scan() - Generic API to abort scan request
 * @mac: pointer to pmac
 * @vdev_id: pdev id
 * @scan_id: scan id
 *
 * Generic API to abort scans
 *
 * Return: 0 for success, non zero for failure
 */
QDF_STATUS csr_scan_abort_mac_scan(struct mac_context *mac, uint32_t vdev_id,
				   uint32_t scan_id);

void csr_free_scan_result_entry(struct mac_context *mac, struct tag_csrscan_result
				*pResult);

QDF_STATUS csr_roam_call_callback(struct mac_context *mac, uint32_t sessionId,
				  struct csr_roam_info *roam_info,
				  uint32_t roamId,
				  eRoamCmdStatus u1, eCsrRoamResult u2);
QDF_STATUS csr_roam_issue_connect(struct mac_context *mac, uint32_t sessionId,
				  struct csr_roam_profile *pProfile,
				  tScanResultHandle hBSSList,
				  enum csr_roam_reason reason, uint32_t roamId,
				  bool fImediate, bool fClearScan);
#ifndef FEATURE_CM_ENABLE
QDF_STATUS csr_roam_issue_reassoc(struct mac_context *mac, uint32_t sessionId,
				  struct csr_roam_profile *pProfile,
				 tCsrRoamModifyProfileFields *pModProfileFields,
				  enum csr_roam_reason reason, uint32_t roamId,
				  bool fImediate);
#endif
void csr_roam_complete(struct mac_context *mac, enum csr_roamcomplete_result Result,
		       void *Context, uint8_t session_id);

/**
 * csr_issue_set_context_req_helper  - Function to fill unicast/broadcast keys
 * request to set the keys to fw
 * @mac:         Poiner to mac context
 * @profile:     Pointer to connected profile
 * @vdev_id:     vdev id
 * @bssid:       Connected BSSID
 * @addkey:      Is add key request to crypto
 * @unicast:     Unicast(1) or broadcast key(0)
 * @key_direction: Key used in TX or RX or both Tx and RX path
 * @key_id:       Key index
 * @key_length:   Key length
 * @key:          Pointer to the key
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
csr_issue_set_context_req_helper(struct mac_context *mac,
				 struct csr_roam_profile *profile,
				 uint32_t session_id,
				 tSirMacAddr *bssid, bool addkey,
				 bool unicast, tAniKeyDirection key_direction,
				 uint8_t key_id, uint16_t key_length,
				 uint8_t *key);
/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
QDF_STATUS csr_roam_process_disassoc_deauth(struct mac_context *mac,
						tSmeCmd *pCommand,
					    bool fDisassoc, bool fMICFailure);
#endif
QDF_STATUS
csr_roam_save_connected_information(struct mac_context *mac,
				    uint32_t sessionId,
				    struct csr_roam_profile *pProfile,
				    struct bss_description *pSirBssDesc,
				    tDot11fBeaconIEs *pIes);

void csr_roam_check_for_link_status_change(struct mac_context *mac,
					tSirSmeRsp *pSirMsg);

QDF_STATUS csr_roam_issue_start_bss(struct mac_context *mac, uint32_t sessionId,
				    struct csr_roamstart_bssparams *pParam,
				    struct csr_roam_profile *pProfile,
				    struct bss_description *bss_desc,
					uint32_t roamId);
QDF_STATUS csr_roam_issue_stop_bss(struct mac_context *mac, uint32_t sessionId,
				   enum csr_roam_substate NewSubstate);
#ifndef FEATURE_CM_ENABLE
bool csr_is_roam_command_waiting_for_session(struct mac_context *mac,
					uint32_t sessionId);
eRoamCmdStatus csr_get_roam_complete_status(struct mac_context *mac,
					    uint32_t sessionId);
#endif
/* pBand can be NULL if caller doesn't need to get it */
QDF_STATUS csr_roam_issue_disassociate_cmd(struct mac_context *mac,
					   uint32_t sessionId,
					   eCsrRoamDisconnectReason reason,
					   enum wlan_reason_code mac_reason);
#ifndef FEATURE_CM_ENABLE
/* pCommand may be NULL */
void csr_roam_remove_duplicate_command(struct mac_context *mac, uint32_t sessionId,
				       tSmeCmd *pCommand,
				       enum csr_roam_reason eRoamReason);

bool csr_is_same_profile(struct mac_context *mac, tCsrRoamConnectedProfile
			*pProfile1, struct csr_roam_profile *pProfile2,
			uint8_t vdev_id);
QDF_STATUS csr_send_join_req_msg(struct mac_context *mac, uint32_t sessionId,
				 struct bss_description *pBssDescription,
				 struct csr_roam_profile *pProfile,
				 tDot11fBeaconIEs *pIes, uint16_t messageType);
#endif
QDF_STATUS csr_send_mb_disassoc_req_msg(struct mac_context *mac, uint32_t sessionId,
					tSirMacAddr bssId, uint16_t reasonCode);
QDF_STATUS csr_send_mb_deauth_req_msg(struct mac_context *mac, uint32_t sessionId,
				      tSirMacAddr bssId, uint16_t reasonCode);
QDF_STATUS csr_send_mb_disassoc_cnf_msg(struct mac_context *mac,
					struct disassoc_ind *pDisassocInd);
QDF_STATUS csr_send_mb_deauth_cnf_msg(struct mac_context *mac,
				      struct deauth_ind *pDeauthInd);
QDF_STATUS csr_send_assoc_cnf_msg(struct mac_context *mac,
				  struct assoc_ind *pAssocInd,
				  QDF_STATUS status,
				  enum wlan_status_code mac_status_code);
QDF_STATUS csr_send_mb_start_bss_req_msg(struct mac_context *mac,
					 uint32_t sessionId,
					 eCsrRoamBssType bssType,
					 struct csr_roamstart_bssparams *pParam,
					 struct bss_description *bss_desc);
QDF_STATUS csr_send_mb_stop_bss_req_msg(struct mac_context *mac,
					uint32_t sessionId);

/* Caller should put the BSS' ssid to fiedl bssSsid when
 * comparing SSID for a BSS.
 */
bool csr_is_ssid_match(struct mac_context *mac, uint8_t *ssid1, uint8_t ssid1Len,
		       uint8_t *bssSsid, uint8_t bssSsidLen,
			bool fSsidRequired);
bool csr_is_phy_mode_match(struct mac_context *mac, uint32_t phyMode,
			   struct bss_description *pSirBssDesc,
			   struct csr_roam_profile *pProfile,
			   enum csr_cfgdot11mode *pReturnCfgDot11Mode,
			   tDot11fBeaconIEs *pIes);

/**
 * csr_get_cfg_valid_channels() - Get valid channel frequency list
 * @mac: mac context
 * @ch_freq_list: valid channel frequencies
 * @num_ch_freq: valid channel nummber
 *
 * This function returns the valid channel frequencies.
 *
 * Return: QDF_STATUS_SUCCESS for success.
 */
QDF_STATUS csr_get_cfg_valid_channels(struct mac_context *mac,
				      uint32_t *ch_freq_list,
				      uint32_t *num_ch_freq);

int8_t csr_get_cfg_max_tx_power(struct mac_context *mac, uint32_t ch_freq);

/* To free the last roaming profile */
void csr_free_roam_profile(struct mac_context *mac, uint32_t sessionId);
#ifndef FEATURE_CM_ENABLE
void csr_free_connect_bss_desc(struct mac_context *mac, uint32_t sessionId);
#endif

/* to free memory allocated inside the profile structure */
void csr_release_profile(struct mac_context *mac,
			 struct csr_roam_profile *pProfile);

enum csr_cfgdot11mode
csr_get_cfg_dot11_mode_from_csr_phy_mode(struct csr_roam_profile *pProfile,
					 eCsrPhyMode phyMode,
					 bool fProprietary);

uint32_t csr_translate_to_wni_cfg_dot11_mode(struct mac_context *mac,
				    enum csr_cfgdot11mode csrDot11Mode);
void csr_save_channel_power_for_band(struct mac_context *mac, bool fPopulate5GBand);
void csr_apply_channel_power_info_to_fw(struct mac_context *mac,
					struct csr_channel *pChannelList,
					uint8_t *countryCode);
void csr_apply_power2_current(struct mac_context *mac);

#ifndef FEATURE_CM_ENABLE
/* return a bool to indicate whether roaming completed or continue. */
bool csr_roam_complete_roaming(struct mac_context *mac, uint32_t sessionId,
			       bool fForce, eCsrRoamResult roamResult);
void csr_roam_completion(struct mac_context *mac, uint32_t sessionId,
			 struct csr_roam_info *roam_info, tSmeCmd *pCommand,
			 eCsrRoamResult roamResult, bool fSuccess);
void csr_roam_cancel_roaming(struct mac_context *mac, uint32_t sessionId);
#endif
void csr_apply_channel_power_info_wrapper(struct mac_context *mac);
QDF_STATUS csr_save_to_channel_power2_g_5_g(struct mac_context *mac,
					uint32_t tableSize,
					struct pwr_channel_info *channelTable);

/*
 * csr_prepare_vdev_delete() - CSR api to delete vdev
 * @mac_ctx: pointer to mac context
 * @vdev_id: vdev id to be deleted.
 * @cleanup: clean up vdev session on true
 *
 * Return QDF_STATUS
 */
QDF_STATUS csr_prepare_vdev_delete(struct mac_context *mac_ctx,
				   uint8_t vdev_id, bool cleanup);

/*
 * csr_cleanup_vdev_session() - CSR api to cleanup vdev
 * @mac_ctx: pointer to mac context
 * @vdev_id: vdev id to be deleted.
 *
 * This API is used to clean up vdev information gathered during
 * vdev was enabled.
 *
 * Return QDF_STATUS
 */
void csr_cleanup_vdev_session(struct mac_context *mac, uint8_t vdev_id);

QDF_STATUS csr_roam_get_session_id_from_bssid(struct mac_context *mac,
						struct qdf_mac_addr *bssid,
					      uint32_t *pSessionId);
enum csr_cfgdot11mode csr_find_best_phy_mode(struct mac_context *mac,
							uint32_t phyMode);

#ifndef FEATURE_CM_ENABLE
/*
 * csr_copy_ssids_from_roam_params() - copy SSID from roam_params to scan filter
 * @rso_usr_cfg: rso user config
 * @filter: scan filter
 *
 * Return void
 */
void csr_copy_ssids_from_roam_params(struct rso_config_params *rso_usr_cfg,
				     struct scan_filter *filter);

/*
 * csr_update_scan_filter_dot11mode() - update dot11mode for scan filter
 * @mac_ctx: csr auth type
 * @filter: scan filter
 *
 * Return void
 */
void csr_update_scan_filter_dot11mode(struct mac_context *mac_ctx,
				      struct scan_filter *filter);

/**
 * csr_roam_get_scan_filter_from_profile() - prepare scan filter from
 * given roam profile
 * @mac: Pointer to Global MAC structure
 * @profile: roam profile
 * @filter: Populated scan filter based on the connected profile
 * @is_roam: if filter is for roam
 * @vdev_id: vdev
 *
 * This function creates a scan filter based on the roam profile. Based on this
 * filter, scan results are obtained.
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE otherwise
 */
QDF_STATUS
csr_roam_get_scan_filter_from_profile(struct mac_context *mac_ctx,
				      struct csr_roam_profile *profile,
				      struct scan_filter *filter,
				      bool is_roam, uint8_t vdev_id);
/**
 * csr_neighbor_roam_get_scan_filter_from_profile() - prepare scan filter from
 * connected profile
 * @mac: Pointer to Global MAC structure
 * @filter: Populated scan filter based on the connected profile
 * @vdev_id: Session ID
 *
 * This function creates a scan filter based on the currently
 * connected profile. Based on this filter, scan results are obtained
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE otherwise
 */
QDF_STATUS
csr_neighbor_roam_get_scan_filter_from_profile(struct mac_context *mac,
					       struct scan_filter *filter,
					       uint8_t vdev_id);
#endif
/*
 * csr_scan_get_result() - Return scan results based on filter
 * @mac: Pointer to Global MAC structure
 * @filter: If pFilter is NULL, all cached results are returned
 * @phResult: an object for the result.
 * @scoring_required: if scoding is required for AP
 *
 * Return QDF_STATUS
 */
QDF_STATUS csr_scan_get_result(struct mac_context *mac,
			       struct scan_filter *filter,
			       tScanResultHandle *phResult,
			       bool scoring_required);

/**
 * csr_scan_get_result_for_bssid - gets the scan result from scan cache for the
 *      bssid specified
 * @mac_ctx: mac context
 * @bssid: bssid to get the scan result for
 * @res: pointer to tCsrScanResultInfo
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_scan_get_result_for_bssid(struct mac_context *mac_ctx,
					 struct qdf_mac_addr *bssid,
					 tCsrScanResultInfo *res);

/*
 * csr_scan_filter_results() -
 *  Filter scan results based on valid channel list.
 *
 * mac - Pointer to Global MAC structure
 * Return QDF_STATUS
 */
QDF_STATUS csr_scan_filter_results(struct mac_context *mac);

/*
 * csr_scan_result_get_first
 * Returns the first element of scan result.
 *
 * hScanResult - returned from csr_scan_get_result
 * tCsrScanResultInfo * - NULL if no result
 */
tCsrScanResultInfo *csr_scan_result_get_first(struct mac_context *mac,
					      tScanResultHandle hScanResult);
/*
 * csr_scan_result_get_next
 * Returns the next element of scan result. It can be called without calling
 * csr_scan_result_get_first first
 *
 * hScanResult - returned from csr_scan_get_result
 * Return Null if no result or reach the end
 */
tCsrScanResultInfo *csr_scan_result_get_next(struct mac_context *mac,
					     tScanResultHandle hScanResult);

/*
 * csr_get_regulatory_domain_for_country() -
 * This function is to get the regulatory domain for a country.
 * This function must be called after CFG is downloaded and all the band/mode
 * setting already passed into CSR.
 *
 * pCountry - Caller allocated buffer with at least 3 bytes specifying the
 * country code
 * pDomainId - Caller allocated buffer to get the return domain ID upon
 * success return. Can be NULL.
 * source - the source of country information.
 * Return QDF_STATUS
 */
QDF_STATUS csr_get_regulatory_domain_for_country(struct mac_context *mac,
						 uint8_t *pCountry,
						 v_REGDOMAIN_t *pDomainId,
						 enum country_src source);

/* some support functions */
bool csr_is11h_supported(struct mac_context *mac);
bool csr_is11e_supported(struct mac_context *mac);
bool csr_is_wmm_supported(struct mac_context *mac);

/* Return SUCCESS is the command is queued, failed */
QDF_STATUS csr_queue_sme_command(struct mac_context *mac, tSmeCmd *pCommand,
				 bool fHighPriority);
tSmeCmd *csr_get_command_buffer(struct mac_context *mac);
void csr_release_command(struct mac_context *mac, tSmeCmd *pCommand);
void csr_release_command_buffer(struct mac_context *mac, tSmeCmd *pCommand);

#ifndef FEATURE_CM_ENABLE
#ifdef FEATURE_WLAN_WAPI
bool csr_is_profile_wapi(struct csr_roam_profile *pProfile);
#endif /* FEATURE_WLAN_WAPI */
#endif
/**
 * csr_get_vdev_type_nss() - gets the nss value based on vdev type
 * @dev_mode: current device operating mode.
 * @nss2g: Pointer to the 2G Nss parameter.
 * @nss5g: Pointer to the 5G Nss parameter.
 *
 * Fills the 2G and 5G Nss values based on device mode.
 *
 * Return: None
 */
void csr_get_vdev_type_nss(enum QDF_OPMODE dev_mode, uint8_t *nss_2g,
			   uint8_t *nss_5g);

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR

/* Security */
#define WLAN_SECURITY_EVENT_REMOVE_KEY_REQ  5
#define WLAN_SECURITY_EVENT_REMOVE_KEY_RSP  6
#define WLAN_SECURITY_EVENT_PMKID_CANDIDATE_FOUND  7
#define WLAN_SECURITY_EVENT_PMKID_UPDATE    8
#define WLAN_SECURITY_EVENT_MIC_ERROR       9
#define WLAN_SECURITY_EVENT_SET_UNICAST_REQ  10
#define WLAN_SECURITY_EVENT_SET_UNICAST_RSP  11
#define WLAN_SECURITY_EVENT_SET_BCAST_REQ    12
#define WLAN_SECURITY_EVENT_SET_BCAST_RSP    13

#define NO_MATCH    0
#define MATCH       1

#define WLAN_SECURITY_STATUS_SUCCESS        0
#define WLAN_SECURITY_STATUS_FAILURE        1

/* Scan */
#define WLAN_SCAN_EVENT_ACTIVE_SCAN_REQ     1
#define WLAN_SCAN_EVENT_ACTIVE_SCAN_RSP     2
#define WLAN_SCAN_EVENT_PASSIVE_SCAN_REQ    3
#define WLAN_SCAN_EVENT_PASSIVE_SCAN_RSP    4
#define WLAN_SCAN_EVENT_HO_SCAN_REQ         5
#define WLAN_SCAN_EVENT_HO_SCAN_RSP         6

#define WLAN_SCAN_STATUS_SUCCESS        0
#define WLAN_SCAN_STATUS_FAILURE        1
#define WLAN_SCAN_STATUS_ABORT          2

#define AUTO_PICK       0
#define SPECIFIED       1

#define WLAN_IBSS_STATUS_SUCCESS        0
#define WLAN_IBSS_STATUS_FAILURE        1

/* 11d */
#define WLAN_80211D_EVENT_COUNTRY_SET   0
#define WLAN_80211D_EVENT_RESET         1

#define WLAN_80211D_DISABLED         0
#define WLAN_80211D_SUPPORT_MULTI_DOMAIN     1
#define WLAN_80211D_NOT_SUPPORT_MULTI_DOMAIN     2
#endif /* #ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR */
/*
 * csr_scan_result_purge() -
 * Remove all items(tCsrScanResult) in the list and free memory for each item
 * hScanResult - returned from csr_scan_get_result. hScanResult is considered
 * gone by calling this function and even before this function reutrns.
 * Return QDF_STATUS
 */
QDF_STATUS csr_scan_result_purge(struct mac_context *mac,
				 tScanResultHandle hScanResult);

/* /////////////////////////////////////////Common Scan ends */

#ifndef FEATURE_CM_ENABLE
/*
 * csr_connect_security_valid_for_6ghz() - check if profile is vlid fro 6Ghz
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @profile: connect profile
 *
 * Return bool
 */
#ifdef CONFIG_BAND_6GHZ
bool csr_connect_security_valid_for_6ghz(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id,
					 struct csr_roam_profile *profile);
#else
static inline bool
csr_connect_security_valid_for_6ghz(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id,
				    struct csr_roam_profile *profile)
{
	return true;
}
#endif

/*
 * csr_roam_reassoc() -
 * To inititiate a re-association
 * pProfile - can be NULL to join the currently connected AP. In that
 * case modProfileFields should carry the modified field(s) which could trigger
 * reassoc
 * modProfileFields - fields which are part of tCsrRoamConnectedProfile
 * that might need modification dynamically once STA is up & running and this
 * could trigger a reassoc
 * pRoamId - to get back the request ID
 * Return QDF_STATUS
 */
QDF_STATUS csr_roam_reassoc(struct mac_context *mac, uint32_t sessionId,
			    struct csr_roam_profile *pProfile,
			    tCsrRoamModifyProfileFields modProfileFields,
			    uint32_t *pRoamId);
#endif

/*
 * csr_roam_connect() -
 * To inititiate an association
 * pProfile - can be NULL to join to any open ones
 * pRoamId - to get back the request ID
 * Return QDF_STATUS
 */
QDF_STATUS csr_roam_connect(struct mac_context *mac, uint32_t sessionId,
			    struct csr_roam_profile *pProfile,
			    uint32_t *pRoamId);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/*
 * csr_get_pmk_info(): store PMK in pmk_cache
 * @mac_ctx: pointer to global structure for MAC
 * @session_id: Sme session id
 * @pmk_cache: pointer to a structure of Pmk
 *
 * This API gets the PMK from the session and
 * stores it in the pmk_cache
 *
 * Return: none
 */
void csr_get_pmk_info(struct mac_context *mac_ctx, uint8_t session_id,
		      struct wlan_crypto_pmksa *pmk_cache);

/*
 * csr_roam_set_psk_pmk() - store PSK/PMK in CSR session
 *
 * @mac  - pointer to global structure for MAC
 * @vdev_id - vdev id
 * @psk_pmk - pointer to an array of PSK/PMK
 * @update_to_fw - Send RSO update config command to firmware to update
 * PMK
 *
 * Return QDF_STATUS - usually it succeed unless sessionId is not found
 */
QDF_STATUS csr_roam_set_psk_pmk(struct mac_context *mac, uint8_t vdev_id,
				uint8_t *psk_pmk, size_t pmk_len,
				bool update_to_fw);
#endif

#ifndef FEATURE_CM_ENABLE
/*
 * csr_roam_get_wpa_rsn_req_ie() -
 * Return the WPA or RSN IE CSR passes to PE to JOIN request or START_BSS
 * request
 * pLen - caller allocated memory that has the length of pBuf as input.
 * Upon returned, *pLen has the needed or IE length in pBuf.
 * pBuf - Caller allocated memory that contain the IE field, if any, upon return
 * Return QDF_STATUS - when fail, it usually means the buffer allocated is not
 * big enough
 */
QDF_STATUS csr_roam_get_wpa_rsn_req_ie(struct mac_context *mac, uint32_t sessionId,
				       uint32_t *pLen, uint8_t *pBuf);
#endif

void csr_roam_free_connect_profile(tCsrRoamConnectedProfile *profile);

/*
 * csr_apply_channel_and_power_list() -
 *  HDD calls this function to set the CFG_VALID_CHANNEL_LIST base on the
 * band/mode settings. This function must be called after CFG is downloaded
 * and all the band/mode setting already passed into CSR.

 * Return QDF_STATUS
 */
QDF_STATUS csr_apply_channel_and_power_list(struct mac_context *mac);

/*
 * csr_roam_disconnect() - To disconnect from a network
 * @mac: pointer to mac context
 * @session_id: Session ID
 * @reason: CSR disconnect reason code as per @enum eCsrRoamDisconnectReason
 * @mac_reason: Mac Disconnect reason code as per @enum wlan_reason_code
 *
 * Return QDF_STATUS
 */
QDF_STATUS csr_roam_disconnect(struct mac_context *mac, uint32_t session_id,
			       eCsrRoamDisconnectReason reason,
			       enum wlan_reason_code mac_reason);

/* This function is used to stop a BSS. It is similar of csr_roamIssueDisconnect
 * but this function doesn't have any logic other than blindly trying to stop
 * BSS
 */
QDF_STATUS csr_roam_issue_stop_bss_cmd(struct mac_context *mac, uint32_t sessionId,
				       bool fHighPriority);

#ifndef FEATURE_CM_ENABLE
void csr_call_roaming_completion_callback(struct mac_context *mac,
					  struct csr_roam_session *pSession,
					  struct csr_roam_info *roam_info,
					  uint32_t roamId,
					  eCsrRoamResult roamResult);
#endif
/**
 * csr_roam_issue_disassociate_sta_cmd() - disassociate a associated station
 * @mac:          Pointer to global structure for MAC
 * @sessionId:     Session Id for Soft AP
 * @p_del_sta_params: Pointer to parameters of the station to disassoc
 *
 * CSR function that HDD calls to issue a deauthenticate station command
 *
 * Return: QDF_STATUS_SUCCESS on success or another QDF_STATUS_* on error
 */
QDF_STATUS csr_roam_issue_disassociate_sta_cmd(struct mac_context *mac,
					       uint32_t sessionId,
					       struct csr_del_sta_params
					       *p_del_sta_params);
/**
 * csr_roam_issue_deauth_sta_cmd() - issue deauthenticate station command
 * @mac:          Pointer to global structure for MAC
 * @sessionId:     Session Id for Soft AP
 * @pDelStaParams: Pointer to parameters of the station to deauthenticate
 *
 * CSR function that HDD calls to issue a deauthenticate station command
 *
 * Return: QDF_STATUS_SUCCESS on success or another QDF_STATUS_** on error
 */
QDF_STATUS csr_roam_issue_deauth_sta_cmd(struct mac_context *mac,
		uint32_t sessionId,
		struct csr_del_sta_params *pDelStaParams);

/*
 * csr_send_chng_mcc_beacon_interval() -
 *   csr function that HDD calls to send Update beacon interval
 *
 * sessionId - session Id for Soft AP
 * Return QDF_STATUS
 */
QDF_STATUS
csr_send_chng_mcc_beacon_interval(struct mac_context *mac, uint32_t sessionId);

#ifndef FEATURE_CM_ENABLE
/**
 * csr_roam_ft_pre_auth_rsp_processor() - Handle the preauth response
 * @mac_ctx: Global MAC context
 * @preauth_rsp: Received preauthentication response
 *
 * Return: None
 */
#ifdef WLAN_FEATURE_HOST_ROAM
void csr_roam_ft_pre_auth_rsp_processor(struct mac_context *mac_ctx,
					tpSirFTPreAuthRsp pFTPreAuthRsp);
#else
static inline
void csr_roam_ft_pre_auth_rsp_processor(struct mac_context *mac_ctx,
					tpSirFTPreAuthRsp pFTPreAuthRsp)
{}
#endif
#endif

#ifdef FEATURE_WLAN_ESE
void update_cckmtsf(uint32_t *timeStamp0, uint32_t *timeStamp1,
		    uint64_t *incr);
void csr_update_prev_ap_info(struct csr_roam_session *session,
			     struct wlan_objmgr_vdev *vdev);

#else
static inline void csr_update_prev_ap_info(struct csr_roam_session *session,
					   struct wlan_objmgr_vdev *vdev) {}
#endif

#ifndef FEATURE_CM_ENABLE
QDF_STATUS csr_roam_enqueue_preauth(struct mac_context *mac, uint32_t sessionId,
				    struct bss_description *pBssDescription,
				    enum csr_roam_reason reason,
				    bool fImmediate);

QDF_STATUS csr_dequeue_roam_command(struct mac_context *mac,
				enum csr_roam_reason reason,
				uint8_t session_id);

QDF_STATUS csr_scan_create_entry_in_scan_cache(struct mac_context *mac,
						uint32_t sessionId,
						struct qdf_mac_addr bssid,
						uint32_t ch_freq);
#endif

QDF_STATUS csr_update_channel_list(struct mac_context *mac);

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * csr_clear_sae_single_pmk - API to clear single_pmk_info cache
 * @psoc: psoc common object
 * @vdev_id: session id
 * @pmksa: pmk info
 *
 * Return : None
 */
void csr_clear_sae_single_pmk(struct wlan_objmgr_psoc *psoc,
			      uint8_t vdev_id, struct wlan_crypto_pmksa *pmksa);
#else
static inline void
csr_clear_sae_single_pmk(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			 struct wlan_crypto_pmksa *pmksa)
{
}
#endif

QDF_STATUS csr_send_ext_change_freq(struct mac_context *mac_ctx,
				    qdf_freq_t ch_freq, uint8_t session_id);

/**
 * csr_csa_start() - request CSA IE transmission from PE
 * @mac_ctx: handle returned by mac_open
 * @session_id: SAP session id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_csa_restart(struct mac_context *mac_ctx, uint8_t session_id);

/**
 * csr_sta_continue_csa() - Continue for CSA for STA after HW mode change
 * @mac_ctx: handle returned by mac_open
 * @vdev_id: STA VDEV ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_sta_continue_csa(struct mac_context *mac_ctx,
				uint8_t vdev_id);

#ifdef QCA_HT_2040_COEX
QDF_STATUS csr_set_ht2040_mode(struct mac_context *mac, uint32_t sessionId,
			       ePhyChanBondState cbMode, bool obssEnabled);
#endif
#ifndef FEATURE_CM_ENABLE
QDF_STATUS csr_scan_handle_search_for_ssid(struct mac_context *mac_ctx,
					   uint32_t session_id);
QDF_STATUS csr_scan_handle_search_for_ssid_failure(struct mac_context *mac,
		uint32_t session_id);
void csr_saved_scan_cmd_free_fields(struct mac_context *mac_ctx,
				    struct csr_roam_session *session);
#endif

struct bss_description*
csr_get_fst_bssdescr_ptr(tScanResultHandle result_handle);

#ifndef FEATURE_CM_ENABLE
struct bss_description*
csr_get_bssdescr_from_scan_handle(tScanResultHandle result_handle,
				  struct bss_description *bss_descr);
#endif
bool is_disconnect_pending(struct mac_context *mac_ctx,
				   uint8_t sessionid);

QDF_STATUS
csr_roam_prepare_bss_config_from_profile(struct mac_context *mac_ctx,
					 struct csr_roam_profile *profile,
					 struct bss_config_param *bss_cfg,
					 struct bss_description *bss_desc);

void
csr_roam_prepare_bss_params(struct mac_context *mac_ctx, uint32_t session_id,
			    struct csr_roam_profile *profile,
			    struct bss_description *bss_desc,
			    struct bss_config_param *bss_cfg,
			    tDot11fBeaconIEs *ies);

/**
 * csr_remove_bssid_from_scan_list() - remove the bssid from
 * scan list
 * @mac_tx: mac context.
 * @bssid: bssid to be removed
 *
 * This function remove the given bssid from scan list.
 *
 * Return: void.
 */
void csr_remove_bssid_from_scan_list(struct mac_context *mac_ctx,
				     tSirMacAddr bssid);

QDF_STATUS
csr_roam_set_bss_config_cfg(struct mac_context *mac_ctx, uint32_t session_id,
			    struct csr_roam_profile *profile,
			    struct bss_description *bss_desc,
			    struct bss_config_param *bss_cfg,
			    tDot11fBeaconIEs *ies, bool reset_country);

void csr_prune_channel_list_for_mode(struct mac_context *mac,
				     struct csr_channel *pChannelList);

#ifndef FEATURE_CM_ENABLE

/**
 * csr_lookup_fils_pmkid  - Lookup FILS PMKID using ssid and cache id
 * @mac:       Pointer to mac context
 * @vdev_id:   vdev id
 * @cache_id:  FILS cache id
 * @ssid:      SSID pointer
 * @ssid_len:  SSID length
 * @bssid:     Pointer to the BSSID to lookup
 *
 * Return: True if lookup is successful
 */
bool csr_lookup_fils_pmkid(struct mac_context *mac, uint8_t vdev_id,
			   uint8_t *cache_id, uint8_t *ssid,
			   uint8_t ssid_len, struct qdf_mac_addr *bssid);
#endif

/**
 * csr_is_pmkid_found_for_peer() - check if pmkid sent by peer is present
				   in PMK cache. Used in SAP mode.
 * @mac: pointer to mac
 * @session: sme session pointer
 * @peer_mac_addr: mac address of the connecting peer
 * @pmkid: pointer to pmkid(s) send by peer
 * @pmkid_count: number of pmkids sent by peer
 *
 * Return: true if pmkid is found else false
 */
bool csr_is_pmkid_found_for_peer(struct mac_context *mac,
				 struct csr_roam_session *session,
				 tSirMacAddr peer_mac_addr,
				 uint8_t *pmkid, uint16_t pmkid_count);
#ifdef WLAN_FEATURE_11BE

/**
 * csr_update_session_eht_cap() - update sme session eht capabilities
 * @mac_ctx: pointer to mac
 * @session: sme session pointer
 *
 * Return: None
 */
void csr_update_session_eht_cap(struct mac_context *mac_ctx,
				struct csr_roam_session *session);
#else
static inline void csr_update_session_eht_cap(struct mac_context *mac_ctx,
					      struct csr_roam_session *session)
{
}
#endif

#ifdef WLAN_FEATURE_11AX
void csr_update_session_he_cap(struct mac_context *mac_ctx,
			struct csr_roam_session *session);
#else
static inline void csr_update_session_he_cap(struct mac_context *mac_ctx,
			struct csr_roam_session *session)
{
}
#endif

#ifndef FEATURE_CM_ENABLE
/**
 * csr_get_channel_for_hw_mode_change() - This function to find
 *                                       out if HW mode change
 *                                       is needed for any of
 *                                       the candidate AP which
 *                                       STA could join
 * @mac_ctx: mac context
 * @result_handle: an object for the result.
 * @session_id: STA session ID
 *
 * This function is written to find out for any bss from scan
 * handle a HW mode change to DBS will be needed or not.
 *
 * Return: AP channel freq for which DBS HW mode will be needed. 0
 * means no HW mode change is needed.
 */
uint32_t
csr_get_channel_for_hw_mode_change(struct mac_context *mac_ctx,
				   tScanResultHandle result_handle,
				   uint32_t session_id);

/**
 * csr_scan_get_channel_for_hw_mode_change() - This function to find
 *                                       out if HW mode change
 *                                       is needed for any of
 *                                       the candidate AP which
 *                                       STA could join
 * @mac_ctx: mac context
 * @session_id: STA session ID
 * @profile: profile
 *
 * This function is written to find out for any bss from scan
 * handle a HW mode change to DBS will be needed or not.
 * If there is no candidate AP which requires DBS, this function will return
 * the first Candidate AP's chan.
 *
 * Return: AP channel freq for which HW mode change will be needed. 0
 * means no candidate AP to connect.
 */
uint32_t
csr_scan_get_channel_for_hw_mode_change(
	struct mac_context *mac_ctx, uint32_t session_id,
	struct csr_roam_profile *profile);
#endif

/**
 * csr_setup_vdev_session() - API to setup vdev mac session
 * @vdev_mlme: vdev mlme private object
 *
 * This API setsup the vdev session for the mac layer
 *
 * Returns: QDF_STATUS
 */
QDF_STATUS csr_setup_vdev_session(struct vdev_mlme_obj *vdev_mlme);


#ifdef WLAN_UNIT_TEST
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
/**
 * csr_cm_get_sta_cxn_info() - This function populates all the connection
 *			    information which is formed by DUT-STA to AP
 * @mac_ctx: pointer to mac context
 * @vdev_id: vdev id
 * @buf: pointer to char buffer to write all the connection information.
 * @buf_size: maximum size of the provided buffer
 *
 * Returns: None (information gets populated in buffer)
 */
void csr_cm_get_sta_cxn_info(struct mac_context *mac_ctx, uint8_t vdev_id,
			     char *buf, uint32_t buf_sz);

#endif
#endif

#endif /* CSR_INSIDE_API_H__ */
