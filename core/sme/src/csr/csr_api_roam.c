/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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
 * DOC: csr_api_roam.c
 *
 * Implementation for the Common Roaming interfaces.
 */
#include "ani_global.h"          /* for struct mac_context **/
#include "wma_types.h"
#include "wma_if.h"          /* for STA_INVALID_IDX. */
#include "csr_inside_api.h"
#include <include/wlan_psoc_mlme.h>
#include "sme_trace.h"
#include "sme_qos_internal.h"
#include "sme_inside.h"
#include "host_diag_core_event.h"
#include "host_diag_core_log.h"
#include "csr_api.h"
#include "csr_internal.h"
#include "cds_reg_service.h"
#include "mac_trace.h"
#include "csr_neighbor_roam.h"
#include "cds_regdomain.h"
#include "cds_utils.h"
#include "sir_types.h"
#include "cfg_ucfg_api.h"
#include "sme_power_save_api.h"
#include "wma.h"
#include "wlan_policy_mgr_api.h"
#include "sme_nan_datapath.h"
#include "pld_common.h"
#include "wlan_reg_services_api.h"
#include "qdf_crypto.h"
#include <wlan_logging_sock_svc.h>
#include "wlan_objmgr_psoc_obj.h"
#include <wlan_scan_ucfg_api.h>
#include <wlan_cp_stats_mc_ucfg_api.h>
#include <wlan_tdls_tgt_api.h>
#include <wlan_cfg80211_scan.h>
#include <wlan_scan_public_structs.h>
#include <wlan_action_oui_public_struct.h>
#include <wlan_action_oui_ucfg_api.h>
#include "wlan_mlme_api.h"
#include "wlan_mlme_ucfg_api.h"
#include <wlan_utility.h>
#include "cfg_mlme.h"
#include "wlan_mlme_public_struct.h"
#include <wlan_crypto_global_api.h>
#include "wlan_qct_sys.h"
#include "wlan_blm_api.h"
#include "wlan_policy_mgr_i.h"
#include "wlan_scan_utils_api.h"
#include "wlan_p2p_cfg_api.h"
#include "cfg_nan_api.h"
#include "nan_ucfg_api.h"
#include <../../core/src/wlan_cm_vdev_api.h>
#include "wlan_reg_ucfg_api.h"

#include <ol_defines.h>
#include "wlan_pkt_capture_ucfg_api.h"
#include "wlan_psoc_mlme_api.h"
#include "wlan_cm_roam_api.h"
#include "wlan_if_mgr_public_struct.h"
#include "wlan_if_mgr_ucfg_api.h"
#include "wlan_if_mgr_roam.h"
#include "wlan_roam_debug.h"
#include "wlan_cm_roam_public_struct.h"
#include "wlan_mlme_twt_api.h"

#define RSN_AUTH_KEY_MGMT_SAE           WLAN_RSN_SEL(WLAN_AKM_SAE)
#define MAX_PWR_FCC_CHAN_12 8
#define MAX_PWR_FCC_CHAN_13 2

#define CSR_SINGLE_PMK_OUI               "\x00\x40\x96\x03"
#define CSR_SINGLE_PMK_OUI_SIZE          4

#define RSSI_HACK_BMPS (-40)
#define MAX_CB_VALUE_IN_INI (2)

#define MAX_SOCIAL_CHANNELS  3

/* packet dump timer duration of 60 secs */
#define PKT_DUMP_TIMER_DURATION 60

/* Choose the largest possible value that can be accommodated in 8 bit signed */
/* variable. */
#define SNR_HACK_BMPS                         (127)

/*
 * ROAMING_OFFLOAD_TIMER_START - Indicates the action to start the timer
 * ROAMING_OFFLOAD_TIMER_STOP - Indicates the action to stop the timer
 * CSR_ROAMING_OFFLOAD_TIMEOUT_PERIOD - Timeout value for roaming offload timer
 */
#define ROAMING_OFFLOAD_TIMER_START	1
#define ROAMING_OFFLOAD_TIMER_STOP	2
#define CSR_ROAMING_OFFLOAD_TIMEOUT_PERIOD    (5 * QDF_MC_TIMER_TO_SEC_UNIT)


#ifdef WLAN_FEATURE_SAE
/**
 * csr_sae_callback - Update SAE info to CSR roam session
 * @mac_ctx: MAC context
 * @msg_ptr: pointer to SAE message
 *
 * API to update SAE info to roam csr session
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS csr_sae_callback(struct mac_context *mac_ctx,
		tSirSmeRsp *msg_ptr)
{
	struct csr_roam_info *roam_info;
	uint32_t session_id;
	struct sir_sae_info *sae_info;

	sae_info = (struct sir_sae_info *) msg_ptr;
	if (!sae_info) {
		sme_err("SAE info is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	sme_debug("vdev_id %d "QDF_MAC_ADDR_FMT,
		sae_info->vdev_id,
		QDF_MAC_ADDR_REF(sae_info->peer_mac_addr.bytes));

	session_id = sae_info->vdev_id;
	if (session_id == WLAN_UMAC_VDEV_ID_MAX)
		return QDF_STATUS_E_INVAL;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_FAILURE;

	roam_info->sae_info = sae_info;

	csr_roam_call_callback(mac_ctx, session_id, roam_info,
				   0, eCSR_ROAM_SAE_COMPUTE,
				   eCSR_ROAM_RESULT_NONE);
	qdf_mem_free(roam_info);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS csr_sae_callback(struct mac_context *mac_ctx,
		tSirSmeRsp *msg_ptr)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static const uint32_t
social_channel_freq[MAX_SOCIAL_CHANNELS] = { 2412, 2437, 2462 };

static void init_config_param(struct mac_context *mac);
static bool csr_roam_process_results(struct mac_context *mac, tSmeCmd *pCommand,
				     enum csr_roamcomplete_result Result,
				     void *Context);

static void csr_roaming_state_config_cnf_processor(struct mac_context *mac,
			tSmeCmd *pCommand, uint8_t session_id);
static QDF_STATUS csr_roam_open(struct mac_context *mac);
static QDF_STATUS csr_roam_close(struct mac_context *mac);
#ifndef FEATURE_CM_ENABLE
static bool csr_roam_is_same_profile_keys(struct mac_context *mac,
				   tCsrRoamConnectedProfile *pConnProfile,
				   struct csr_roam_profile *pProfile2);
static QDF_STATUS csr_roam_start_roaming_timer(struct mac_context *mac,
					       uint32_t vdev_id,
					       uint32_t interval);
static QDF_STATUS csr_roam_stop_roaming_timer(struct mac_context *mac,
					      uint32_t sessionId);
static void csr_roam_roaming_timer_handler(void *pv);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void csr_roam_roaming_offload_timer_action(struct mac_context *mac_ctx,
		uint32_t interval, uint8_t session_id, uint8_t action);
#endif
static void csr_roam_roaming_offload_timeout_handler(void *timer_data);
#endif

static QDF_STATUS csr_init11d_info(struct mac_context *mac, tCsr11dinfo *ps11dinfo);
static QDF_STATUS csr_init_channel_power_list(struct mac_context *mac,
					      tCsr11dinfo *ps11dinfo);
static QDF_STATUS csr_roam_free_connected_info(struct mac_context *mac,
					       struct csr_roam_connectedinfo *
					       pConnectedInfo);
#ifndef FEATURE_CM_ENABLE
static void csr_roam_link_up(struct mac_context *mac, struct qdf_mac_addr bssid);
#endif
static enum csr_cfgdot11mode
csr_roam_get_phy_mode_band_for_bss(struct mac_context *mac,
				   struct csr_roam_profile *pProfile,
				   uint32_t bss_op_ch_freq,
				   enum reg_wifi_band *pBand);
static QDF_STATUS csr_roam_get_qos_info_from_bss(
struct mac_context *mac, struct bss_description *bss_desc);

#ifndef FEATURE_CM_ENABLE
static uint32_t csr_find_session_by_type(struct mac_context *,
					enum QDF_OPMODE);
static void csr_roam_link_down(struct mac_context *mac, uint32_t sessionId);
static bool csr_is_conn_allow_2g_band(struct mac_context *mac,
						uint32_t chnl);
static bool csr_is_conn_allow_5g_band(struct mac_context *mac,
						uint32_t chnl);
#endif
static QDF_STATUS csr_roam_start_wds(struct mac_context *mac,
						uint32_t sessionId,
				     struct csr_roam_profile *pProfile,
				     struct bss_description *bss_desc);
static void csr_init_session(struct mac_context *mac, uint32_t sessionId);

static QDF_STATUS
csr_roam_get_qos_info_from_bss(struct mac_context *mac,
			       struct bss_description *bss_desc);

static void csr_init_operating_classes(struct mac_context *mac);

static void csr_add_len_of_social_channels(struct mac_context *mac,
		uint8_t *num_chan);
static void csr_add_social_channels(struct mac_context *mac,
		tSirUpdateChanList *chan_list, struct csr_scanstruct *pScan,
		uint8_t *num_chan);

#ifdef WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY
static struct csr_roam_session *csr_roam_roam_session;

/* Allocate and initialize global variables */
static QDF_STATUS csr_roam_init_globals(struct mac_context *mac)
{
	uint32_t buf_size;
	QDF_STATUS status;

	buf_size = WLAN_MAX_VDEVS * sizeof(struct csr_roam_session);

	csr_roam_roam_session = qdf_mem_malloc(buf_size);
	if (csr_roam_roam_session) {
		mac->roam.roamSession = csr_roam_roam_session;
		status = QDF_STATUS_SUCCESS;
	} else {
		status = QDF_STATUS_E_NOMEM;
	}

	return status;
}

/* Free memory allocated dynamically */
static inline void csr_roam_free_globals(void)
{
	qdf_mem_free(csr_roam_roam_session);
	csr_roam_roam_session = NULL;
}

#else /* WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY */
static struct csr_roam_session csr_roam_roam_session[WLAN_MAX_VDEVS];

/* Initialize global variables */
static QDF_STATUS csr_roam_init_globals(struct mac_context *mac)
{
	qdf_mem_zero(&csr_roam_roam_session,
		     sizeof(csr_roam_roam_session));
	mac->roam.roamSession = csr_roam_roam_session;

	return QDF_STATUS_SUCCESS;
}

static inline void csr_roam_free_globals(void)
{
}
#endif /* WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY */

#ifndef FEATURE_CM_ENABLE
/* Returns whether handoff is currently in progress or not */
static
bool csr_roam_is_handoff_in_progress(struct mac_context *mac, uint8_t sessionId)
{
	return csr_neighbor_roam_is_handoff_in_progress(mac, sessionId);
}

static QDF_STATUS
csr_roam_issue_disassociate(struct mac_context *mac, uint32_t sessionId,
			    enum csr_roam_substate NewSubstate,
			    bool fMICFailure)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr bssId = QDF_MAC_ADDR_BCAST_INIT;
	uint16_t reasonCode;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	tpCsrNeighborRoamControlInfo p_nbr_roam_info;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (fMICFailure) {
		reasonCode = REASON_MIC_FAILURE;
	} else if (NewSubstate == eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF) {
		reasonCode = REASON_AUTHORIZED_ACCESS_LIMIT_REACHED;
	} else if (eCSR_ROAM_SUBSTATE_DISASSOC_STA_HAS_LEFT == NewSubstate) {
		reasonCode = REASON_DISASSOC_NETWORK_LEAVING;
		NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_FORCED;
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "set to reason code eSIR_MAC_DISASSOC_LEAVING_BSS_REASON and set back NewSubstate");
	} else {
		reasonCode = REASON_UNSPEC_FAILURE;
	}

	p_nbr_roam_info = &mac->roam.neighborRoamInfo[sessionId];
	if (csr_roam_is_handoff_in_progress(mac, sessionId) &&
	    NewSubstate != eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF &&
	    p_nbr_roam_info->csrNeighborRoamProfile.BSSIDs.numOfBSSIDs) {
		qdf_copy_macaddr(
			&bssId,
			p_nbr_roam_info->csrNeighborRoamProfile.BSSIDs.bssid);
	} else if (pSession->pConnectBssDesc) {
		qdf_mem_copy(&bssId.bytes, pSession->pConnectBssDesc->bssId,
			     sizeof(struct qdf_mac_addr));
	}

	sme_debug("Disassociate Bssid " QDF_MAC_ADDR_FMT " subState: %s reason: %d",
		  QDF_MAC_ADDR_REF(bssId.bytes),
		  mac_trace_getcsr_roam_sub_state(NewSubstate),
		  reasonCode);

	csr_roam_substate_change(mac, NewSubstate, sessionId);

	status = csr_send_mb_disassoc_req_msg(mac, sessionId, bssId.bytes,
					      reasonCode);

	if (QDF_IS_STATUS_SUCCESS(status)) {
		csr_roam_link_down(mac, sessionId);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		/* no need to tell QoS that we are disassociating, it will be
		 * taken care off in assoc req for HO
		 */
		if (eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF != NewSubstate) {
			/* notify QoS module that disassoc happening */
			sme_qos_csr_event_ind(mac, (uint8_t)sessionId,
					      SME_QOS_CSR_DISCONNECT_REQ, NULL);
		}
#endif
	} else {
		sme_warn("csr_send_mb_disassoc_req_msg failed status: %d",
			 status);
	}

	return status;
}
#endif

static void csr_roam_de_init_globals(struct mac_context *mac)
{
	uint8_t i;

	for (i = 0; i < WLAN_MAX_VDEVS; i++) {
		if (mac->roam.roamSession[i].pCurRoamProfile)
			csr_release_profile(mac,
					    mac->roam.roamSession[i].pCurRoamProfile);
	}
	csr_roam_free_globals();
	mac->roam.roamSession = NULL;
}

QDF_STATUS csr_open(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t i;

	do {
		/* Initialize CSR Roam Globals */
		status = csr_roam_init_globals(mac);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;

		for (i = 0; i < WLAN_MAX_VDEVS; i++)
			csr_roam_state_change(mac, eCSR_ROAMING_STATE_STOP, i);

		init_config_param(mac);
		status = csr_scan_open(mac);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			csr_roam_free_globals();
			break;
		}
		status = csr_roam_open(mac);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			csr_roam_free_globals();
			break;
		}
		mac->roam.nextRoamId = 1;      /* Must not be 0 */
	} while (0);

	return status;
}

QDF_STATUS csr_init_chan_list(struct mac_context *mac, uint8_t *alpha2)
{
	QDF_STATUS status;

	mac->scan.countryCodeDefault[0] = alpha2[0];
	mac->scan.countryCodeDefault[1] = alpha2[1];
	mac->scan.countryCodeDefault[2] = alpha2[2];

	sme_debug("init time country code %.2s", mac->scan.countryCodeDefault);

	qdf_mem_copy(mac->scan.countryCodeCurrent,
		     mac->scan.countryCodeDefault, REG_ALPHA2_LEN + 1);
	qdf_mem_copy(mac->scan.countryCodeElected,
		     mac->scan.countryCodeDefault, REG_ALPHA2_LEN + 1);
	status = csr_get_channel_and_power_list(mac);

	return status;
}

QDF_STATUS csr_set_channels(struct mac_context *mac,
			    struct csr_config_params *pParam)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t index = 0;

	qdf_mem_copy(pParam->Csr11dinfo.countryCode,
		     mac->scan.countryCodeCurrent, REG_ALPHA2_LEN + 1);
	for (index = 0; index < mac->scan.base_channels.numChannels;
	     index++) {
		pParam->Csr11dinfo.Channels.channel_freq_list[index] =
			mac->scan.base_channels.channel_freq_list[index];
		pParam->Csr11dinfo.ChnPower[index].first_chan_freq =
			mac->scan.base_channels.channel_freq_list[index];
		pParam->Csr11dinfo.ChnPower[index].numChannels = 1;
		pParam->Csr11dinfo.ChnPower[index].maxtxPower =
			mac->scan.defaultPowerTable[index].tx_power;
	}
	pParam->Csr11dinfo.Channels.numChannels =
		mac->scan.base_channels.numChannels;

	return status;
}

QDF_STATUS csr_close(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	csr_roam_close(mac);
	csr_scan_close(mac);
	/* DeInit Globals */
	csr_roam_de_init_globals(mac);
	return status;
}

static int8_t
csr_find_channel_pwr(struct channel_power *pdefaultPowerTable,
		     uint32_t chan_freq)
{
	uint8_t i;
	/* TODO: if defaultPowerTable is guaranteed to be in ascending */
	/* order of channel numbers, we can employ binary search */
	for (i = 0; i < CFG_VALID_CHANNEL_LIST_LEN; i++) {
		if (pdefaultPowerTable[i].center_freq == chan_freq)
			return pdefaultPowerTable[i].tx_power;
	}
	/* could not find the channel list in default list */
	/* this should not have occurred */
	QDF_ASSERT(0);
	return 0;
}

/**
 * csr_roam_arrange_ch_list() - Updates the channel list modified with greedy
 * order for 5 Ghz preference and DFS channels.
 * @mac_ctx: pointer to mac context.
 * @chan_list:    channel list updated with greedy channel order.
 * @num_channel:  Number of channels in list
 *
 * To allow Early Stop Roaming Scan feature to co-exist with 5G preference,
 * this function moves 5G channels ahead of 2G channels. This function can
 * also move 2G channels, ahead of DFS channel or vice versa. Order is
 * maintained among same category channels
 *
 * Return: None
 */
static void csr_roam_arrange_ch_list(struct mac_context *mac_ctx,
			tSirUpdateChanParam *chan_list, uint8_t num_channel)
{
	bool prefer_5g = CSR_IS_ROAM_PREFER_5GHZ(mac_ctx);
	bool prefer_dfs = CSR_IS_DFS_CH_ROAM_ALLOWED(mac_ctx);
	int i, j = 0;
	tSirUpdateChanParam *tmp_list = NULL;

	if (!prefer_5g)
		return;

	tmp_list = qdf_mem_malloc(sizeof(tSirUpdateChanParam) * num_channel);
	if (!tmp_list)
		return;

	/* Fist copy Non-DFS 5g channels */
	for (i = 0; i < num_channel; i++) {
		if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_list[i].freq) &&
			!wlan_reg_is_dfs_for_freq(mac_ctx->pdev,
						  chan_list[i].freq)) {
			qdf_mem_copy(&tmp_list[j++],
				&chan_list[i], sizeof(tSirUpdateChanParam));
			chan_list[i].freq = 0;
		}
	}
	if (prefer_dfs) {
		/* next copy DFS channels (remaining channels in 5G) */
		for (i = 0; i < num_channel; i++) {
			if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_list[i].freq)) {
				qdf_mem_copy(&tmp_list[j++], &chan_list[i],
					sizeof(tSirUpdateChanParam));
				chan_list[i].freq = 0;
			}
		}
	} else {
		/* next copy 2G channels */
		for (i = 0; i < num_channel; i++) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_list[i].freq)) {
				qdf_mem_copy(&tmp_list[j++], &chan_list[i],
					sizeof(tSirUpdateChanParam));
				chan_list[i].freq = 0;
			}
		}
	}
	/* copy rest of the channels in same order to tmp list */
	for (i = 0; i < num_channel; i++) {
		if (chan_list[i].freq) {
			qdf_mem_copy(&tmp_list[j++], &chan_list[i],
				sizeof(tSirUpdateChanParam));
			chan_list[i].freq = 0;
		}
	}
	/* copy tmp list to original channel list buffer */
	qdf_mem_copy(chan_list, tmp_list,
				 sizeof(tSirUpdateChanParam) * num_channel);
	qdf_mem_free(tmp_list);
}

/**
 * csr_roam_sort_channel_for_early_stop() - Sort the channels
 * @mac_ctx:        mac global context
 * @chan_list:      Original channel list from the upper layers
 * @num_channel:    Number of original channels
 *
 * For Early stop scan feature, the channel list should be in an order,
 * where-in there is a maximum chance to detect an AP in the initial
 * channels in the list so that the scanning can be stopped early as the
 * feature demands.
 * Below fixed greedy channel list has been provided
 * based on most of the enterprise wifi installations across the globe.
 *
 * Identify all the greedy channels within the channel list from user space.
 * Identify all the non-greedy channels in the user space channel list.
 * Merge greedy channels followed by non-greedy channels back into the
 * chan_list.
 *
 * Return: None
 */
static void csr_roam_sort_channel_for_early_stop(struct mac_context *mac_ctx,
			tSirUpdateChanList *chan_list, uint8_t num_channel)
{
	tSirUpdateChanList *chan_list_greedy, *chan_list_non_greedy;
	uint8_t i, j;
	static const uint32_t fixed_greedy_freq_list[] = {2412, 2437, 2462,
		5180, 5240, 5200, 5220, 2457, 2417, 2452, 5745, 5785, 5805,
		2422, 2427, 2447, 5765, 5825, 2442, 2432, 5680, 5700, 5260,
		5580, 5280, 5520, 5320, 5300, 5500, 5600, 2472, 2484, 5560,
		5660, 5755, 5775};
	uint8_t num_fixed_greedy_chan;
	uint8_t num_greedy_chan = 0;
	uint8_t num_non_greedy_chan = 0;
	uint8_t match_found = false;
	uint32_t buf_size;

	buf_size = sizeof(tSirUpdateChanList) +
		(sizeof(tSirUpdateChanParam) * num_channel);
	chan_list_greedy = qdf_mem_malloc(buf_size);
	chan_list_non_greedy = qdf_mem_malloc(buf_size);
	if (!chan_list_greedy || !chan_list_non_greedy)
		goto scan_list_sort_error;
	/*
	 * fixed_greedy_freq_list is an evaluated freq list based on most of
	 * the enterprise wifi deployments and the order of the channels
	 * determines the highest possibility of finding an AP.
	 * chan_list is the channel list provided by upper layers based on the
	 * regulatory domain.
	 */
	num_fixed_greedy_chan = sizeof(fixed_greedy_freq_list) /
							sizeof(uint32_t);
	/*
	 * Browse through the chan_list and put all the non-greedy channels
	 * into a separate list by name chan_list_non_greedy
	 */
	for (i = 0; i < num_channel; i++) {
		for (j = 0; j < num_fixed_greedy_chan; j++) {
			if (chan_list->chanParam[i].freq ==
					 fixed_greedy_freq_list[j]) {
				match_found = true;
				break;
			}
		}
		if (!match_found) {
			qdf_mem_copy(
			  &chan_list_non_greedy->chanParam[num_non_greedy_chan],
			  &chan_list->chanParam[i],
			  sizeof(tSirUpdateChanParam));
			num_non_greedy_chan++;
		} else {
			match_found = false;
		}
	}
	/*
	 * Browse through the fixed_greedy_chan_list and put all the greedy
	 * channels in the chan_list into a separate list by name
	 * chan_list_greedy
	 */
	for (i = 0; i < num_fixed_greedy_chan; i++) {
		for (j = 0; j < num_channel; j++) {
			if (fixed_greedy_freq_list[i] ==
				chan_list->chanParam[j].freq) {
				qdf_mem_copy(
				  &chan_list_greedy->chanParam[num_greedy_chan],
				  &chan_list->chanParam[j],
				  sizeof(tSirUpdateChanParam));
				num_greedy_chan++;
				break;
			}
		}
	}
	QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_DEBUG,
		"greedy=%d, non-greedy=%d, tot=%d",
		num_greedy_chan, num_non_greedy_chan, num_channel);
	if ((num_greedy_chan + num_non_greedy_chan) != num_channel) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			"incorrect sorting of channels");
		goto scan_list_sort_error;
	}
	/* Copy the Greedy channels first */
	i = 0;
	qdf_mem_copy(&chan_list->chanParam[i],
		&chan_list_greedy->chanParam[i],
		num_greedy_chan * sizeof(tSirUpdateChanParam));
	/* Copy the remaining Non Greedy channels */
	i = num_greedy_chan;
	j = 0;
	qdf_mem_copy(&chan_list->chanParam[i],
		&chan_list_non_greedy->chanParam[j],
		num_non_greedy_chan * sizeof(tSirUpdateChanParam));

	/* Update channel list for 5g preference and allow DFS roam */
	csr_roam_arrange_ch_list(mac_ctx, chan_list->chanParam, num_channel);
scan_list_sort_error:
	qdf_mem_free(chan_list_greedy);
	qdf_mem_free(chan_list_non_greedy);
}

/**
 * csr_emu_chan_req() - update the required channel list for emulation
 * @channel: channel number to check
 *
 * To reduce scan time during emulation platforms, this function
 * restricts the scanning to be done on selected channels
 *
 * Return: QDF_STATUS enumeration
 */
#ifdef QCA_WIFI_EMULATION
#define SCAN_CHAN_LIST_5G_LEN 6
#define SCAN_CHAN_LIST_2G_LEN 3
static const uint16_t
csr_scan_chan_list_5g[SCAN_CHAN_LIST_5G_LEN] = { 5180, 5220, 5260, 5280, 5700, 5745 };
static const uint16_t
csr_scan_chan_list_2g[SCAN_CHAN_LIST_2G_LEN] = { 2412, 2437, 2462 };
static QDF_STATUS csr_emu_chan_req(uint32_t channel)
{
	int i;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(channel)) {
		for (i = 0; i < QDF_ARRAY_SIZE(csr_scan_chan_list_2g); i++) {
			if (csr_scan_chan_list_2g[i] == channel)
				return QDF_STATUS_SUCCESS;
		}
	} else if (WLAN_REG_IS_5GHZ_CH_FREQ(channel)) {
		for (i = 0; i < QDF_ARRAY_SIZE(csr_scan_chan_list_5g); i++) {
			if (csr_scan_chan_list_5g[i] == channel)
				return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}
#else
static QDF_STATUS csr_emu_chan_req(uint32_t channel_num)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_ENABLE_SOCIAL_CHANNELS_5G_ONLY
static void csr_add_len_of_social_channels(struct mac_context *mac,
		uint8_t *num_chan)
{
	uint8_t i;
	uint8_t no_chan = *num_chan;

	sme_debug("add len of social channels, before adding - num_chan:%hu",
			*num_chan);
	if (CSR_IS_5G_BAND_ONLY(mac)) {
		for (i = 0; i < MAX_SOCIAL_CHANNELS; i++) {
			if (wlan_reg_get_channel_state_for_freq(
				mac->pdev, social_channel_freq[i]) ==
					CHANNEL_STATE_ENABLE)
				no_chan++;
		}
	}
	*num_chan = no_chan;
	sme_debug("after adding - num_chan:%hu", *num_chan);
}

static void csr_add_social_channels(struct mac_context *mac,
		tSirUpdateChanList *chan_list, struct csr_scanstruct *pScan,
		uint8_t *num_chan)
{
	uint8_t i;
	uint8_t no_chan = *num_chan;

	sme_debug("add social channels chan_list %pK, num_chan %hu", chan_list,
			*num_chan);
	if (CSR_IS_5G_BAND_ONLY(mac)) {
		for (i = 0; i < MAX_SOCIAL_CHANNELS; i++) {
			if (wlan_reg_get_channel_state_for_freq(
					mac->pdev, social_channel_freq[i]) !=
					CHANNEL_STATE_ENABLE)
				continue;
			chan_list->chanParam[no_chan].freq =
				social_channel_freq[i];
			chan_list->chanParam[no_chan].pwr =
				csr_find_channel_pwr(pScan->defaultPowerTable,
						social_channel_freq[i]);
			chan_list->chanParam[no_chan].dfsSet = false;
			if (cds_is_5_mhz_enabled())
				chan_list->chanParam[no_chan].quarter_rate
					= 1;
			else if (cds_is_10_mhz_enabled())
				chan_list->chanParam[no_chan].half_rate = 1;
			no_chan++;
		}
		sme_debug("after adding -num_chan %hu", no_chan);
	}
	*num_chan = no_chan;
}
#else
static void csr_add_len_of_social_channels(struct mac_context *mac,
		uint8_t *num_chan)
{
	sme_debug("skip adding len of social channels");
}
static void csr_add_social_channels(struct mac_context *mac,
		tSirUpdateChanList *chan_list, struct csr_scanstruct *pScan,
		uint8_t *num_chan)
{
	sme_debug("skip social channels");
}
#endif

/**
 * csr_scan_event_handler() - callback for scan event
 * @vdev: wlan objmgr vdev pointer
 * @event: scan event
 * @arg: global mac context pointer
 *
 * Return: void
 */
static void csr_scan_event_handler(struct wlan_objmgr_vdev *vdev,
					    struct scan_event *event,
					    void *arg)
{
	bool success = false;
	QDF_STATUS lock_status;
	struct mac_context *mac = arg;

	if (!mac)
		return;

	if (!util_is_scan_completed(event, &success))
		return;

	lock_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(lock_status))
		return;

	if (mac->scan.pending_channel_list_req)
		csr_update_channel_list(mac);
	sme_release_global_lock(&mac->sme);
}

QDF_STATUS csr_update_channel_list(struct mac_context *mac)
{
	tSirUpdateChanList *pChanList;
	struct csr_scanstruct *pScan = &mac->scan;
	uint8_t numChan = pScan->base_channels.numChannels;
	uint8_t num_channel = 0;
	uint32_t bufLen;
	struct scheduler_msg msg = {0};
	uint8_t i;
	uint8_t channel_state;
	uint16_t unsafe_chan[NUM_CHANNELS];
	uint16_t unsafe_chan_cnt = 0;
	uint16_t cnt = 0;
	uint32_t  channel_freq;
	bool is_unsafe_chan;
	bool is_same_band;
	bool is_5mhz_enabled;
	bool is_10mhz_enabled;
	enum scm_scan_status scan_status;
	QDF_STATUS lock_status;
	struct rso_roam_policy_params *roam_policy;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_ctx)
		return QDF_STATUS_E_FAILURE;

	mlme_obj = mlme_get_psoc_ext_obj(mac->psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;
	roam_policy = &mlme_obj->cfg.lfr.rso_user_config.policy_params;
	lock_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(lock_status))
		return lock_status;

	if (mac->mlme_cfg->reg.enable_pending_chan_list_req) {
		scan_status = ucfg_scan_get_pdev_status(mac->pdev);
		if (scan_status == SCAN_IS_ACTIVE ||
		    scan_status == SCAN_IS_ACTIVE_AND_PENDING) {
			mac->scan.pending_channel_list_req = true;
			sme_release_global_lock(&mac->sme);
			sme_debug("scan in progress postpone channel list req ");
			return QDF_STATUS_SUCCESS;
		}
		mac->scan.pending_channel_list_req = false;
	}
	sme_release_global_lock(&mac->sme);

	pld_get_wlan_unsafe_channel(qdf_ctx->dev, unsafe_chan,
		    &unsafe_chan_cnt,
		    sizeof(unsafe_chan));

	csr_add_len_of_social_channels(mac, &numChan);

	bufLen = sizeof(tSirUpdateChanList) +
		 (sizeof(tSirUpdateChanParam) * (numChan));

	csr_init_operating_classes(mac);
	pChanList = qdf_mem_malloc(bufLen);
	if (!pChanList)
		return QDF_STATUS_E_NOMEM;

	is_5mhz_enabled = cds_is_5_mhz_enabled();
	if (is_5mhz_enabled)
		sme_nofl_debug("quarter_rate enabled");
	is_10mhz_enabled = cds_is_10_mhz_enabled();
	if (is_10mhz_enabled)
		sme_nofl_debug("half_rate enabled");

	for (i = 0; i < pScan->base_channels.numChannels; i++) {
		if (QDF_STATUS_SUCCESS !=
			csr_emu_chan_req(pScan->base_channels.channel_freq_list[i]))
			continue;

		channel_freq = pScan->base_channels.channel_freq_list[i];
		/* Scan is not performed on DSRC channels*/
		if (wlan_reg_is_dsrc_freq(channel_freq))
			continue;

		channel_state =
			wlan_reg_get_channel_state_for_freq(
				mac->pdev, channel_freq);
		if ((CHANNEL_STATE_ENABLE == channel_state) ||
		    mac->scan.fEnableDFSChnlScan) {
			if ((roam_policy->dfs_mode ==
				STA_ROAM_POLICY_DFS_DISABLED) &&
				(channel_state == CHANNEL_STATE_DFS)) {
				QDF_TRACE(QDF_MODULE_ID_SME,
					QDF_TRACE_LEVEL_DEBUG,
					FL("skip dfs channel frequency %d"),
					channel_freq);
				continue;
			}
			if (roam_policy->skip_unsafe_channels &&
			    unsafe_chan_cnt) {
				is_unsafe_chan = false;
				for (cnt = 0; cnt < unsafe_chan_cnt; cnt++) {
					if (unsafe_chan[cnt] == channel_freq) {
						is_unsafe_chan = true;
						break;
					}
				}
				is_same_band =
					(WLAN_REG_IS_24GHZ_CH_FREQ(
							channel_freq) &&
					roam_policy->sap_operating_band ==
							BAND_2G) ||
					(WLAN_REG_IS_5GHZ_CH_FREQ(
							channel_freq) &&
					roam_policy->sap_operating_band ==
							BAND_5G);
				if (is_unsafe_chan && is_same_band) {
					QDF_TRACE(QDF_MODULE_ID_SME,
					QDF_TRACE_LEVEL_DEBUG,
					FL("ignoring unsafe channel freq %d"),
					channel_freq);
					continue;
				}
			}
			pChanList->chanParam[num_channel].freq =
				pScan->base_channels.channel_freq_list[i];
			pChanList->chanParam[num_channel].pwr =
				csr_find_channel_pwr(
				pScan->defaultPowerTable,
				pScan->base_channels.channel_freq_list[i]);

			if (pScan->fcc_constraint) {
				if (2467 ==
					pScan->base_channels.channel_freq_list[i]) {
					pChanList->chanParam[num_channel].pwr =
						MAX_PWR_FCC_CHAN_12;
					QDF_TRACE(QDF_MODULE_ID_SME,
						  QDF_TRACE_LEVEL_DEBUG,
						  "txpow for channel 12 is %d",
						  MAX_PWR_FCC_CHAN_12);
				}
				if (2472 ==
					pScan->base_channels.channel_freq_list[i]) {
					pChanList->chanParam[num_channel].pwr =
						MAX_PWR_FCC_CHAN_13;
					QDF_TRACE(QDF_MODULE_ID_SME,
						  QDF_TRACE_LEVEL_DEBUG,
						  "txpow for channel 13 is %d",
						  MAX_PWR_FCC_CHAN_13);
				}
			}

			if (!ucfg_is_nan_allowed_on_freq(mac->pdev,
				pChanList->chanParam[num_channel].freq))
				pChanList->chanParam[num_channel].nan_disabled =
					true;

			if (CHANNEL_STATE_ENABLE != channel_state)
				pChanList->chanParam[num_channel].dfsSet =
					true;

			pChanList->chanParam[num_channel].quarter_rate =
							is_5mhz_enabled;

			pChanList->chanParam[num_channel].half_rate =
							is_10mhz_enabled;

			num_channel++;
		}
	}

	csr_add_social_channels(mac, pChanList, pScan, &num_channel);

	if (mac->mlme_cfg->lfr.early_stop_scan_enable)
		csr_roam_sort_channel_for_early_stop(mac, pChanList,
						     num_channel);
	else
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("Early Stop Scan Feature not supported"));

	if ((mac->roam.configParam.uCfgDot11Mode ==
				eCSR_CFG_DOT11_MODE_AUTO) ||
			(mac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11AC) ||
			(mac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11AC_ONLY)) {
		pChanList->vht_en = true;
		if (mac->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band)
			pChanList->vht_24_en = true;
	}
	if ((mac->roam.configParam.uCfgDot11Mode ==
				eCSR_CFG_DOT11_MODE_AUTO) ||
			(mac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11N) ||
			(mac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11N_ONLY)) {
		pChanList->ht_en = true;
	}
	if ((mac->roam.configParam.uCfgDot11Mode == eCSR_CFG_DOT11_MODE_AUTO) ||
	    (mac->roam.configParam.uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AX) ||
	    (mac->roam.configParam.uCfgDot11Mode ==
	     eCSR_CFG_DOT11_MODE_11AX_ONLY))
		pChanList->he_en = true;
#ifdef WLAN_FEATURE_11BE
	if ((mac->roam.configParam.uCfgDot11Mode == eCSR_CFG_DOT11_MODE_AUTO) ||
	    CSR_IS_CFG_DOT11_PHY_MODE_11BE(
		mac->roam.configParam.uCfgDot11Mode) ||
	    CSR_IS_CFG_DOT11_PHY_MODE_11BE_ONLY(
		mac->roam.configParam.uCfgDot11Mode))
		pChanList->eht_en = true;
#endif

	pChanList->numChan = num_channel;
	mlme_store_fw_scan_channels(mac->psoc, pChanList);

	msg.type = WMA_UPDATE_CHAN_LIST_REQ;
	msg.reserved = 0;
	msg.bodyptr = pChanList;
	MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			 NO_SESSION, msg.type));
	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		qdf_mem_free(pChanList);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_start(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t i;

	do {
		for (i = 0; i < WLAN_MAX_VDEVS; i++)
			csr_roam_state_change(mac, eCSR_ROAMING_STATE_IDLE, i);

		mac->roam.sPendingCommands = 0;

#ifndef FEATURE_CM_ENABLE
		for (i = 0; i < WLAN_MAX_VDEVS; i++)
			status = csr_neighbor_roam_init(mac, i);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_warn("Couldn't Init HO control blk");
			break;
		}
		/* Register with scan component */
		mac->scan.requester_id = ucfg_scan_register_requester(
						mac->psoc,
						"CSR", csr_scan_callback, mac);
#endif
		if (mac->mlme_cfg->reg.enable_pending_chan_list_req) {
			status = ucfg_scan_register_event_handler(mac->pdev,
					csr_scan_event_handler, mac);

			if (QDF_IS_STATUS_ERROR(status))
				sme_err("scan event registration failed ");
		}
	} while (0);
	return status;
}

QDF_STATUS csr_stop(struct mac_context *mac)
{
	uint32_t sessionId;

	if (mac->mlme_cfg->reg.enable_pending_chan_list_req)
		ucfg_scan_unregister_event_handler(mac->pdev,
						   csr_scan_event_handler,
						   mac);
	ucfg_scan_psoc_set_disable(mac->psoc, REASON_SYSTEM_DOWN);
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	ucfg_scan_unregister_requester(mac->psoc, mac->scan.requester_id);
#endif

	/*
	 * purge all serialization commnad if there are any pending to make
	 * sure memory and vdev ref are freed.
	 */
	csr_purge_pdev_all_ser_cmd_list(mac);
	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++)
		csr_prepare_vdev_delete(mac, sessionId, true);
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++)
		csr_neighbor_roam_close(mac, sessionId);
#endif
	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++)
		if (CSR_IS_SESSION_VALID(mac, sessionId))
			ucfg_scan_flush_results(mac->pdev, NULL);

	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++) {
		csr_roam_state_change(mac, eCSR_ROAMING_STATE_STOP, sessionId);
		csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
					 sessionId);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_ready(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	/* If the gScanAgingTime is set to '0' then scan results aging timeout
	 * based  on timer feature is not enabled
	 */
	status = csr_apply_channel_and_power_list(mac);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("csr_apply_channel_and_power_list failed status: %d",
			status);

	return status;
}

void csr_set_default_dot11_mode(struct mac_context *mac)
{
	mac->mlme_cfg->dot11_mode.dot11_mode =
			csr_translate_to_wni_cfg_dot11_mode(mac,
					  mac->roam.configParam.uCfgDot11Mode);
}

void csr_set_global_cfgs(struct mac_context *mac)
{
	wlan_mlme_set_frag_threshold(mac->psoc, csr_get_frag_thresh(mac));
	wlan_mlme_set_rts_threshold(mac->psoc, csr_get_rts_thresh(mac));
	/* For now we will just use the 5GHz CB mode ini parameter to decide
	 * whether CB supported or not in Probes when there is no session
	 * Once session is established we will use the session related params
	 * stored in PE session for CB mode
	 */
	if (cfg_in_range(CFG_CHANNEL_BONDING_MODE_5GHZ,
			 mac->roam.configParam.channelBondingMode5GHz))
		ucfg_mlme_set_channel_bonding_5ghz(mac->psoc,
						   mac->roam.configParam.
						   channelBondingMode5GHz);
	if (cfg_in_range(CFG_CHANNEL_BONDING_MODE_24GHZ,
			 mac->roam.configParam.channelBondingMode24GHz))
		ucfg_mlme_set_channel_bonding_24ghz(mac->psoc,
						    mac->roam.configParam.
						    channelBondingMode24GHz);

	mac->mlme_cfg->timeouts.heart_beat_threshold =
			cfg_default(CFG_HEART_BEAT_THRESHOLD);

	/* Update the operating mode to configured value during
	 *  initialization, So that client can advertise full
	 *  capabilities in Probe request frame.
	 */
	csr_set_default_dot11_mode(mac);
}

#if defined(WLAN_LOGGING_SOCK_SVC_ENABLE) && \
	defined(FEATURE_PKTLOG) && !defined(REMOVE_PKT_LOG)
/**
 * csr_packetdump_timer_handler() - packet dump timer
 * handler
 * @pv: user data
 *
 * This function is used to handle packet dump timer
 *
 * Return: None
 *
 */
static void csr_packetdump_timer_handler(void *pv)
{
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"%s Invoking packetdump deregistration API", __func__);
	wlan_deregister_txrx_packetdump(OL_TXRX_PDEV_ID);
}

void csr_packetdump_timer_start(void)
{
	QDF_STATUS status;
	mac_handle_t mac_handle;
	struct mac_context *mac;
	QDF_TIMER_STATE cur_state;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	mac = MAC_CONTEXT(mac_handle);
	if (!mac) {
		QDF_ASSERT(0);
		return;
	}
	cur_state = qdf_mc_timer_get_current_state(&mac->roam.packetdump_timer);
	if (cur_state == QDF_TIMER_STATE_STARTING ||
	    cur_state == QDF_TIMER_STATE_STARTING) {
		sme_debug("packetdump_timer is already started: %d", cur_state);
		return;
	}

	status = qdf_mc_timer_start(&mac->roam.packetdump_timer,
				    (PKT_DUMP_TIMER_DURATION *
				     QDF_MC_TIMER_TO_SEC_UNIT) /
				    QDF_MC_TIMER_TO_MS_UNIT);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_debug("cannot start packetdump timer status: %d", status);
}

void csr_packetdump_timer_stop(void)
{
	QDF_STATUS status;
	mac_handle_t mac_handle;
	struct mac_context *mac;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	mac = MAC_CONTEXT(mac_handle);
	if (!mac) {
		QDF_ASSERT(0);
		return;
	}

	status = qdf_mc_timer_stop(&mac->roam.packetdump_timer);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("cannot stop packetdump timer");
}

static QDF_STATUS csr_packetdump_timer_init(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!mac) {
		QDF_ASSERT(0);
		return -EINVAL;
	}

	status = qdf_mc_timer_init(&mac->roam.packetdump_timer,
				   QDF_TIMER_TYPE_SW,
				   csr_packetdump_timer_handler,
				   mac);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("cannot allocate memory for packetdump timer");
		return status;
	}

	return status;
}

static void csr_packetdump_timer_deinit(struct mac_context *mac)
{
	if (!mac) {
		QDF_ASSERT(0);
		return;
	}

	qdf_mc_timer_stop(&mac->roam.packetdump_timer);
	qdf_mc_timer_destroy(&mac->roam.packetdump_timer);
}
#else
static inline QDF_STATUS csr_packetdump_timer_init(struct mac_context *mac)
{
	return QDF_STATUS_SUCCESS;
}

static inline void csr_packetdump_timer_deinit(struct mac_context *mac) {}
#endif

static QDF_STATUS csr_roam_open(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
#ifndef FEATURE_CM_ENABLE
	uint32_t i;
	struct csr_roam_session *pSession;
#endif

#ifndef FEATURE_CM_ENABLE
	for (i = 0; i < WLAN_MAX_VDEVS; i++) {
		pSession = CSR_GET_SESSION(mac, i);

		if (!pSession)
			continue;

		pSession->roamingTimerInfo.mac = mac;
		pSession->roamingTimerInfo.vdev_id = WLAN_UMAC_VDEV_ID_MAX;
	}
#endif
	status = csr_packetdump_timer_init(mac);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	spin_lock_init(&mac->roam.roam_state_lock);

	return status;
}

static QDF_STATUS csr_roam_close(struct mac_context *mac)
{
	uint32_t sessionId;
	struct csr_roam_session *session;

	/*
	 * purge all serialization commnad if there are any pending to make
	 * sure memory and vdev ref are freed.
	 */
	csr_purge_pdev_all_ser_cmd_list(mac);
	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++) {
		session = CSR_GET_SESSION(mac, sessionId);
		if (!session)
			continue;

		csr_prepare_vdev_delete(mac, sessionId, true);
	}

	csr_packetdump_timer_deinit(mac);
	return QDF_STATUS_SUCCESS;
}

void csr_roam_free_connect_profile(tCsrRoamConnectedProfile *profile)
{
	qdf_mem_zero(profile, sizeof(tCsrRoamConnectedProfile));
#ifndef FEATURE_CM_ENABLE
	profile->AuthType = eCSR_AUTH_TYPE_UNKNOWN;
#endif
}

static QDF_STATUS csr_roam_free_connected_info(struct mac_context *mac,
					       struct csr_roam_connectedinfo *
					       pConnectedInfo)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (pConnectedInfo->pbFrames) {
		qdf_mem_free(pConnectedInfo->pbFrames);
		pConnectedInfo->pbFrames = NULL;
	}
	pConnectedInfo->nBeaconLength = 0;
	pConnectedInfo->nAssocReqLength = 0;
	pConnectedInfo->nAssocRspLength = 0;
	pConnectedInfo->nRICRspLength = 0;
#ifdef FEATURE_WLAN_ESE
	pConnectedInfo->nTspecIeLength = 0;
#endif
	return status;
}

void csr_release_command_roam(struct mac_context *mac, tSmeCmd *pCommand)
{
	csr_reinit_roam_cmd(mac, pCommand);
}

void csr_release_command_wm_status_change(struct mac_context *mac,
					  tSmeCmd *pCommand)
{
	csr_reinit_wm_status_change_cmd(mac, pCommand);
}

#ifndef FEATURE_CM_ENABLE
static void csr_release_command_set_hw_mode(struct mac_context *mac,
					    tSmeCmd *cmd)
{
	struct csr_roam_session *session;
	uint32_t session_id;

	if (cmd->u.set_hw_mode_cmd.reason ==
	    POLICY_MGR_UPDATE_REASON_HIDDEN_STA) {
		session_id = cmd->u.set_hw_mode_cmd.session_id;
		session = CSR_GET_SESSION(mac, session_id);
		if (session)
			csr_saved_scan_cmd_free_fields(mac, session);
	}
}
#endif

void csr_roam_substate_change(struct mac_context *mac,
		enum csr_roam_substate NewSubstate, uint32_t sessionId)
{
	if (sessionId >= WLAN_MAX_VDEVS) {
		sme_err("Invalid no of concurrent sessions %d",
			  sessionId);
		return;
	}
	if (mac->roam.curSubState[sessionId] == NewSubstate)
		return;
	sme_nofl_debug("CSR RoamSubstate: [ %s <== %s ]",
		       mac_trace_getcsr_roam_sub_state(NewSubstate),
		       mac_trace_getcsr_roam_sub_state(mac->roam.
		       curSubState[sessionId]));
	spin_lock(&mac->roam.roam_state_lock);
	mac->roam.curSubState[sessionId] = NewSubstate;
	spin_unlock(&mac->roam.roam_state_lock);
}

enum csr_roam_state csr_roam_state_change(struct mac_context *mac,
				    enum csr_roam_state NewRoamState,
				uint8_t sessionId)
{
	enum csr_roam_state PreviousState;

	PreviousState = mac->roam.curState[sessionId];

	if (NewRoamState == mac->roam.curState[sessionId])
		return PreviousState;

	sme_nofl_debug("CSR RoamState[%d]: [ %s <== %s ]", sessionId,
		       mac_trace_getcsr_roam_state(NewRoamState),
		       mac_trace_getcsr_roam_state(
		       mac->roam.curState[sessionId]));
	/*
	 * Whenever we transition OUT of the Roaming state,
	 * clear the Roaming substate.
	 */
	if (CSR_IS_ROAM_JOINING(mac, sessionId)) {
		csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
					 sessionId);
	}

	mac->roam.curState[sessionId] = NewRoamState;

	return PreviousState;
}

static void init_config_param(struct mac_context *mac)
{
	mac->roam.configParam.channelBondingMode24GHz =
		WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
	mac->roam.configParam.channelBondingMode5GHz =
		WNI_CFG_CHANNEL_BONDING_MODE_ENABLE;

	mac->roam.configParam.phyMode = eCSR_DOT11_MODE_AUTO;
	mac->roam.configParam.uCfgDot11Mode = eCSR_CFG_DOT11_MODE_AUTO;
	mac->roam.configParam.HeartbeatThresh50 = 40;
	mac->roam.configParam.Is11eSupportEnabled = true;
	mac->roam.configParam.WMMSupportMode = eCsrRoamWmmAuto;
	mac->roam.configParam.ProprietaryRatesEnabled = true;

	mac->roam.configParam.nVhtChannelWidth =
		WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ + 1;
}

/**
 * csr_flush_roam_scan_chan_lists() - Flush the roam channel lists
 * @mac: Global MAC context
 * @vdev_id: vdev id
 *
 * Flush the roam channel lists pref_chan_info and specific_chan_info.
 *
 * Return: None
 */
static void
csr_flush_roam_scan_chan_lists(struct mac_context *mac, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct rso_config *rso_cfg;
	struct rso_cfg_params *cfg_params;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(mac->pdev, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev)
		return;

	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	cfg_params = &rso_cfg->cfg_param;
	wlan_cm_flush_roam_channel_list(&cfg_params->pref_chan_info);
	wlan_cm_flush_roam_channel_list(&cfg_params->specific_chan_info);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}

#ifdef FEATURE_WLAN_ESE
/**
 * csr_roam_is_ese_assoc() - is this ese association
 * @mac_ctx: Global MAC context
 * @session_id: session identifier
 *
 * Returns whether the current association is a ESE assoc or not.
 *
 * Return: true if ese association; false otherwise
 */
bool csr_roam_is_ese_assoc(struct mac_context *mac_ctx, uint32_t session_id)
{
	return wlan_cm_get_ese_assoc(mac_ctx->pdev, session_id);
}


/**
 * csr_roam_is_ese_ini_feature_enabled() - is ese feature enabled
 * @mac_ctx: Global MAC context
 *
 * Return: true if ese feature is enabled; false otherwise
 */
bool csr_roam_is_ese_ini_feature_enabled(struct mac_context *mac)
{
	return mac->mlme_cfg->lfr.ese_enabled;
}

/**
 * csr_tsm_stats_rsp_processor() - tsm stats response processor
 * @mac: Global MAC context
 * @pMsg: Message pointer
 *
 * Return: None
 */
static void csr_tsm_stats_rsp_processor(struct mac_context *mac, void *pMsg)
{
	tAniGetTsmStatsRsp *pTsmStatsRsp = (tAniGetTsmStatsRsp *) pMsg;

	if (pTsmStatsRsp) {
		/*
		 * Get roam Rssi request is backed up and passed back
		 * to the response, Extract the request message
		 * to fetch callback.
		 */
		tpAniGetTsmStatsReq reqBkp
			= (tAniGetTsmStatsReq *) pTsmStatsRsp->tsmStatsReq;

		if (reqBkp) {
			if (reqBkp->tsmStatsCallback) {
				((tCsrTsmStatsCallback)
				 (reqBkp->tsmStatsCallback))(pTsmStatsRsp->
							     tsmMetrics,
							     reqBkp->
							     pDevContext);
				reqBkp->tsmStatsCallback = NULL;
			}
			qdf_mem_free(reqBkp);
			pTsmStatsRsp->tsmStatsReq = NULL;
		} else {
			if (reqBkp) {
				qdf_mem_free(reqBkp);
				pTsmStatsRsp->tsmStatsReq = NULL;
			}
		}
	} else {
		sme_err("pTsmStatsRsp is NULL");
	}
}

/**
 * csr_send_ese_adjacent_ap_rep_ind() - ese send adjacent ap report
 * @mac: Global MAC context
 * @pSession: Session pointer
 *
 * Return: None
 */
static void csr_send_ese_adjacent_ap_rep_ind(struct mac_context *mac,
					struct csr_roam_session *pSession)
{
	uint32_t roamTS2 = 0;
	struct csr_roam_info *roam_info;
	struct pe_session *pe_session = NULL;
	uint8_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	struct qdf_mac_addr connected_bssid;

	if (!pSession) {
		sme_err("pSession is NULL");
		return;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	roamTS2 = qdf_mc_timer_get_system_time();
	roam_info->tsmRoamDelay = roamTS2 - pSession->roamTS1;
	wlan_mlme_get_bssid_vdev_id(mac->pdev, pSession->vdev_id,
				    &connected_bssid);
	sme_debug("Bssid(" QDF_MAC_ADDR_FMT ") Roaming Delay(%u ms)",
		QDF_MAC_ADDR_REF(connected_bssid.bytes),
		roam_info->tsmRoamDelay);

	pe_session = pe_find_session_by_bssid(mac, connected_bssid.bytes,
					      &sessionId);
	if (!pe_session) {
		sme_err("session %d not found", sessionId);
		qdf_mem_free(roam_info);
		return;
	}

	pe_session->eseContext.tsm.tsmMetrics.RoamingDly
		= roam_info->tsmRoamDelay;

	csr_roam_call_callback(mac, pSession->sessionId, roam_info,
			       0, eCSR_ROAM_ESE_ADJ_AP_REPORT_IND, 0);
	qdf_mem_free(roam_info);
}

/**
 * csr_get_tsm_stats() - get tsm stats
 * @mac: Global MAC context
 * @callback: TSM stats callback
 * @staId: Station id
 * @bssId: bssid
 * @pContext: pointer to context
 * @tid: traffic id
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS csr_get_tsm_stats(struct mac_context *mac,
			     tCsrTsmStatsCallback callback,
			     struct qdf_mac_addr bssId,
			     void *pContext, uint8_t tid)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tAniGetTsmStatsReq *pMsg = NULL;

	pMsg = qdf_mem_malloc(sizeof(tAniGetTsmStatsReq));
	if (!pMsg) {
		return QDF_STATUS_E_NOMEM;
	}
	/* need to initiate a stats request to PE */
	pMsg->msgType = eWNI_SME_GET_TSM_STATS_REQ;
	pMsg->msgLen = (uint16_t) sizeof(tAniGetTsmStatsReq);
	pMsg->tid = tid;
	qdf_copy_macaddr(&pMsg->bssId, &bssId);
	pMsg->tsmStatsCallback = callback;
	pMsg->pDevContext = pContext;
	status = umac_send_mb_message_to_mac(pMsg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Failed to send down the TSM req (status=%d)", status);
		/* pMsg is freed by cds_send_mb_message_to_mac */
		status = QDF_STATUS_E_FAILURE;
	}
	return status;
}

#ifndef FEATURE_CM_ENABLE

/*  Update the TSF with the difference in system time */
static void update_cckmtsf(uint32_t *timestamp0, uint32_t *timestamp1,
			   uint64_t *incr)
{
	uint64_t timestamp64 = ((uint64_t)*timestamp1 << 32) | (*timestamp0);

	timestamp64 = (uint64_t)(timestamp64 + (*incr));
	*timestamp0 = (uint32_t)(timestamp64 & 0xffffffff);
	*timestamp1 = (uint32_t)((timestamp64 >> 32) & 0xffffffff);
}

/**
 * csr_roam_read_tsf() - read TSF
 * @mac: Global MAC context
 * @sessionId: session identifier
 * @pTimestamp: output TSF timestamp
 *
 * This function reads the TSF; and also add the time elapsed since
 * last beacon or probe response reception from the hand off AP to arrive at
 * the latest TSF value.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS csr_roam_read_tsf(struct mac_context *mac, uint8_t *pTimestamp,
			     uint8_t sessionId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tCsrNeighborRoamBSSInfo handoffNode = {{0} };
	uint64_t timer_diff = 0;
	uint32_t timeStamp[2];
	struct bss_description *pBssDescription = NULL;

	csr_neighbor_roam_get_handoff_ap_info(mac, &handoffNode, sessionId);
	if (!handoffNode.pBssDescription) {
		sme_err("Invalid BSS Description");
		return QDF_STATUS_E_INVAL;
	}
	pBssDescription = handoffNode.pBssDescription;
	/* Get the time diff in nano seconds */
	timer_diff = (qdf_get_monotonic_boottime_ns()  -
				pBssDescription->scansystimensec);
	/* Convert msec to micro sec timer */
	timer_diff = do_div(timer_diff, SYSTEM_TIME_NSEC_TO_USEC);
	timeStamp[0] = pBssDescription->timeStamp[0];
	timeStamp[1] = pBssDescription->timeStamp[1];
	update_cckmtsf(&(timeStamp[0]), &(timeStamp[1]), &timer_diff);
	qdf_mem_copy(pTimestamp, (void *)&timeStamp[0], sizeof(uint32_t) * 2);
	return status;
}
#endif
#endif /* FEATURE_WLAN_ESE */

/**
 * csr_roam_is_roam_offload_scan_enabled() - is roam offload enabled
 * @mac_ctx: Global MAC context
 *
 * Returns whether firmware based background scan is currently enabled or not.
 *
 * Return: true if roam offload scan enabled; false otherwise
 */
bool csr_roam_is_roam_offload_scan_enabled(struct mac_context *mac_ctx)
{
	return mac_ctx->mlme_cfg->lfr.roam_scan_offload_enabled;
}

/* The funcns csr_convert_cb_ini_value_to_phy_cb_state and
 * csr_convert_phy_cb_state_to_ini_value have been introduced
 * to convert the ini value to the ENUM used in csr and MAC for CB state
 * Ideally we should have kept the ini value and enum value same and
 * representing the same cb values as in 11n standard i.e.
 * Set to 1 (SCA) if the secondary channel is above the primary channel
 * Set to 3 (SCB) if the secondary channel is below the primary channel
 * Set to 0 (SCN) if no secondary channel is present
 * However, since our driver is already distributed we will keep the ini
 * definition as it is which is:
 * 0 - secondary none
 * 1 - secondary LOW
 * 2 - secondary HIGH
 * and convert to enum value used within the driver in
 * csr_change_default_config_param using this funcn
 * The enum values are as follows:
 * PHY_SINGLE_CHANNEL_CENTERED          = 0
 * PHY_DOUBLE_CHANNEL_LOW_PRIMARY   = 1
 * PHY_DOUBLE_CHANNEL_HIGH_PRIMARY  = 3
 */
ePhyChanBondState csr_convert_cb_ini_value_to_phy_cb_state(uint32_t cbIniValue)
{

	ePhyChanBondState phyCbState;

	switch (cbIniValue) {
	/* secondary none */
	case eCSR_INI_SINGLE_CHANNEL_CENTERED:
		phyCbState = PHY_SINGLE_CHANNEL_CENTERED;
		break;
	/* secondary LOW */
	case eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY:
		phyCbState = PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
		break;
	/* secondary HIGH */
	case eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY:
		phyCbState = PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
		phyCbState =
			PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH;
		break;
	default:
		/* If an invalid value is passed, disable CHANNEL BONDING */
		phyCbState = PHY_SINGLE_CHANNEL_CENTERED;
		break;
	}
	return phyCbState;
}

static
uint32_t csr_convert_phy_cb_state_to_ini_value(ePhyChanBondState phyCbState)
{
	uint32_t cbIniValue;

	switch (phyCbState) {
	/* secondary none */
	case PHY_SINGLE_CHANNEL_CENTERED:
		cbIniValue = eCSR_INI_SINGLE_CHANNEL_CENTERED;
		break;
	/* secondary LOW */
	case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
		cbIniValue = eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY;
		break;
	/* secondary HIGH */
	case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		cbIniValue = eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
		cbIniValue =
			eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
		cbIniValue =
		eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
		cbIniValue =
			eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH;
		break;
	default:
		/* return some invalid value */
		cbIniValue = eCSR_INI_CHANNEL_BONDING_STATE_MAX;
		break;
	}
	return cbIniValue;
}

#ifdef WLAN_FEATURE_11BE
void csr_update_session_eht_cap(struct mac_context *mac_ctx,
				struct csr_roam_session *session)
{
	tDot11fIEeht_cap *eht_cap;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
						    session->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev)
		return;
	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	qdf_mem_copy(&mlme_priv->eht_config,
		     &mac_ctx->mlme_cfg->eht_caps.dot11_eht_cap,
		     sizeof(mlme_priv->eht_config));
	eht_cap = &mlme_priv->eht_config;
	eht_cap->present = true;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}
#endif

#ifdef WLAN_FEATURE_11AX
void csr_update_session_he_cap(struct mac_context *mac_ctx,
			       struct csr_roam_session *session)
{
	enum QDF_OPMODE persona;
	tDot11fIEhe_cap *he_cap;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
						    session->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev)
		return;
	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	qdf_mem_copy(&mlme_priv->he_config,
		     &mac_ctx->mlme_cfg->he_caps.dot11_he_cap,
		     sizeof(mlme_priv->he_config));
	he_cap = &mlme_priv->he_config;
	he_cap->present = true;
	/*
	 * Do not advertise requester role for SAP & responder role
	 * for STA
	 */
	persona = wlan_vdev_mlme_get_opmode(vdev);
	if (persona == QDF_SAP_MODE || persona == QDF_P2P_GO_MODE) {
		he_cap->twt_request = 0;
	} else if (persona == QDF_STA_MODE || persona == QDF_P2P_CLIENT_MODE) {
		he_cap->twt_responder = 0;
	}

	if (he_cap->ppet_present) {
		/* till now operating channel is not decided yet, use 5g cap */
		qdf_mem_copy(he_cap->ppet.ppe_threshold.ppe_th,
			     mac_ctx->mlme_cfg->he_caps.he_ppet_5g,
			     WNI_CFG_HE_PPET_LEN);
		he_cap->ppet.ppe_threshold.num_ppe_th =
			lim_truncate_ppet(he_cap->ppet.ppe_threshold.ppe_th,
					  WNI_CFG_HE_PPET_LEN);
	} else {
		he_cap->ppet.ppe_threshold.num_ppe_th = 0;
	}
	mlme_priv->he_sta_obsspd = mac_ctx->mlme_cfg->he_caps.he_sta_obsspd;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}
#endif

QDF_STATUS csr_change_default_config_param(struct mac_context *mac,
					   struct csr_config_params *pParam)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (pParam) {
		mac->roam.configParam.is_force_1x1 =
			pParam->is_force_1x1;
		mac->roam.configParam.WMMSupportMode = pParam->WMMSupportMode;
		mac->mlme_cfg->wmm_params.wme_enabled =
			(pParam->WMMSupportMode == eCsrRoamWmmNoQos) ? 0 : 1;
		mac->roam.configParam.Is11eSupportEnabled =
			pParam->Is11eSupportEnabled;

		mac->roam.configParam.fenableMCCMode = pParam->fEnableMCCMode;
		mac->roam.configParam.mcc_rts_cts_prot_enable =
						pParam->mcc_rts_cts_prot_enable;
		mac->roam.configParam.mcc_bcast_prob_resp_enable =
					pParam->mcc_bcast_prob_resp_enable;
		mac->roam.configParam.fAllowMCCGODiffBI =
			pParam->fAllowMCCGODiffBI;

		/* channelBondingMode5GHz plays a dual role right now
		 * INFRA STA will use this non zero value as CB enabled
		 * and SOFTAP will use this non-zero value to determine
		 * the secondary channel offset. This is how
		 * channelBondingMode5GHz works now and this is kept intact
		 * to avoid any cfg.ini change.
		 */
		if (pParam->channelBondingMode24GHz > MAX_CB_VALUE_IN_INI)
			sme_warn("Invalid CB value from ini in 2.4GHz band %d, CB DISABLED",
				pParam->channelBondingMode24GHz);
		mac->roam.configParam.channelBondingMode24GHz =
			csr_convert_cb_ini_value_to_phy_cb_state(pParam->
						channelBondingMode24GHz);
		if (pParam->channelBondingMode5GHz > MAX_CB_VALUE_IN_INI)
			sme_warn("Invalid CB value from ini in 5GHz band %d, CB DISABLED",
				pParam->channelBondingMode5GHz);
		mac->roam.configParam.channelBondingMode5GHz =
			csr_convert_cb_ini_value_to_phy_cb_state(pParam->
							channelBondingMode5GHz);
		mac->roam.configParam.phyMode = pParam->phyMode;
		mac->roam.configParam.HeartbeatThresh50 =
			pParam->HeartbeatThresh50;
		mac->roam.configParam.ProprietaryRatesEnabled =
			pParam->ProprietaryRatesEnabled;

		mac->roam.configParam.wep_tkip_in_he = pParam->wep_tkip_in_he;

		mac->roam.configParam.uCfgDot11Mode =
			csr_get_cfg_dot11_mode_from_csr_phy_mode(NULL,
							mac->roam.configParam.
							phyMode,
							mac->roam.configParam.
						ProprietaryRatesEnabled);

		/* Assign this before calling csr_init11d_info */
		if (wlan_reg_11d_enabled_on_host(mac->psoc))
			status = csr_init11d_info(mac, &pParam->Csr11dinfo);

		/* Initialize the power + channel information if 11h is
		 * enabled. If 11d is enabled this information has already
		 * been initialized
		 */
		if (csr_is11h_supported(mac) &&
				!wlan_reg_11d_enabled_on_host(mac->psoc))
			csr_init_channel_power_list(mac, &pParam->Csr11dinfo);

		mac->scan.fEnableDFSChnlScan = pParam->fEnableDFSChnlScan;
		mac->roam.configParam.send_smps_action =
			pParam->send_smps_action;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
		mac->roam.configParam.cc_switch_mode = pParam->cc_switch_mode;
#endif
		mac->roam.configParam.obssEnabled = pParam->obssEnabled;
		mac->roam.configParam.conc_custom_rule1 =
			pParam->conc_custom_rule1;
		mac->roam.configParam.conc_custom_rule2 =
			pParam->conc_custom_rule2;
		mac->roam.configParam.is_sta_connection_in_5gz_enabled =
			pParam->is_sta_connection_in_5gz_enabled;

		/* update interface configuration */
		mac->sme.max_intf_count = pParam->max_intf_count;

		mac->f_sta_miracast_mcc_rest_time_val =
			pParam->f_sta_miracast_mcc_rest_time_val;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
		mac->sap.sap_channel_avoidance =
			pParam->sap_channel_avoidance;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	}
	return status;
}

QDF_STATUS csr_get_config_param(struct mac_context *mac,
				struct csr_config_params *pParam)
{
	struct csr_config *cfg_params = &mac->roam.configParam;

	if (!pParam)
		return QDF_STATUS_E_INVAL;

	pParam->is_force_1x1 = cfg_params->is_force_1x1;
	pParam->WMMSupportMode = cfg_params->WMMSupportMode;
	pParam->Is11eSupportEnabled = cfg_params->Is11eSupportEnabled;
	pParam->channelBondingMode24GHz = csr_convert_phy_cb_state_to_ini_value(
					cfg_params->channelBondingMode24GHz);
	pParam->channelBondingMode5GHz = csr_convert_phy_cb_state_to_ini_value(
					cfg_params->channelBondingMode5GHz);
	pParam->phyMode = cfg_params->phyMode;
	pParam->HeartbeatThresh50 = cfg_params->HeartbeatThresh50;
	pParam->ProprietaryRatesEnabled = cfg_params->ProprietaryRatesEnabled;
	pParam->fEnableDFSChnlScan = mac->scan.fEnableDFSChnlScan;
	pParam->fEnableMCCMode = cfg_params->fenableMCCMode;
	pParam->fAllowMCCGODiffBI = cfg_params->fAllowMCCGODiffBI;

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	pParam->cc_switch_mode = cfg_params->cc_switch_mode;
#endif
	pParam->wep_tkip_in_he = cfg_params->wep_tkip_in_he;
	csr_set_channels(mac, pParam);
	pParam->obssEnabled = cfg_params->obssEnabled;
	pParam->conc_custom_rule1 = cfg_params->conc_custom_rule1;
	pParam->conc_custom_rule2 = cfg_params->conc_custom_rule2;
	pParam->is_sta_connection_in_5gz_enabled =
		cfg_params->is_sta_connection_in_5gz_enabled;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	pParam->sap_channel_avoidance = mac->sap.sap_channel_avoidance;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	pParam->max_intf_count = mac->sme.max_intf_count;
	pParam->f_sta_miracast_mcc_rest_time_val =
		mac->f_sta_miracast_mcc_rest_time_val;
	pParam->send_smps_action = mac->roam.configParam.send_smps_action;

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_prune_ch_list() - prunes the channel list to keep only a type of channels
 * @ch_lst:        existing channel list
 * @is_24_GHz:     indicates if 2.5 GHz or 5 GHz channels are required
 *
 * Return: void
 */
static void csr_prune_ch_list(struct csr_channel *ch_lst, bool is_24_GHz)
{
	uint8_t idx = 0, num_channels = 0;

	for ( ; idx < ch_lst->numChannels; idx++) {
		if (is_24_GHz) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_lst->channel_freq_list[idx])) {
				ch_lst->channel_freq_list[num_channels] =
					ch_lst->channel_freq_list[idx];
				num_channels++;
			}
		} else {
			if (WLAN_REG_IS_5GHZ_CH_FREQ(ch_lst->channel_freq_list[idx])) {
				ch_lst->channel_freq_list[num_channels] =
					ch_lst->channel_freq_list[idx];
				num_channels++;
			}
		}
	}
	/*
	 * Cleanup the rest of channels. Note we only need to clean up the
	 * channels if we had to trim the list. Calling qdf_mem_set() with a 0
	 * size is going to throw asserts on the debug builds so let's be a bit
	 * smarter about that. Zero out the reset of the channels only if we
	 * need to. The amount of memory to clear is the number of channesl that
	 * we trimmed (ch_lst->numChannels - num_channels) times the size of a
	 * channel in the structure.
	 */
	if (ch_lst->numChannels > num_channels) {
		qdf_mem_zero(&ch_lst->channel_freq_list[num_channels],
			     sizeof(ch_lst->channel_freq_list[0]) *
			     (ch_lst->numChannels - num_channels));
	}
	ch_lst->numChannels = num_channels;
}

/**
 * csr_prune_channel_list_for_mode() - prunes the channel list
 * @mac_ctx:       global mac context
 * @ch_lst:        existing channel list
 *
 * Prunes the channel list according to band stored in mac_ctx
 *
 * Return: void
 */
void csr_prune_channel_list_for_mode(struct mac_context *mac_ctx,
				     struct csr_channel *ch_lst)
{
	/* for dual band NICs, don't need to trim the channel list.... */
	if (CSR_IS_OPEARTING_DUAL_BAND(mac_ctx))
		return;
	/*
	 * 2.4 GHz band operation requires the channel list to be trimmed to
	 * the 2.4 GHz channels only
	 */
	if (CSR_IS_24_BAND_ONLY(mac_ctx))
		csr_prune_ch_list(ch_lst, true);
	else if (CSR_IS_5G_BAND_ONLY(mac_ctx))
		csr_prune_ch_list(ch_lst, false);
}

#define INFRA_AP_DEFAULT_CHAN_FREQ 2437

QDF_STATUS csr_get_channel_and_power_list(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t num20MHzChannelsFound = 0;
	QDF_STATUS qdf_status;
	uint8_t Index = 0;

	qdf_status = wlan_reg_get_channel_list_with_power_for_freq(
				mac->pdev,
				mac->scan.defaultPowerTable,
				&num20MHzChannelsFound);

	if ((QDF_STATUS_SUCCESS != qdf_status) ||
	    (num20MHzChannelsFound == 0)) {
		sme_err("failed to get channels");
		status = QDF_STATUS_E_FAILURE;
	} else {
		if (num20MHzChannelsFound > CFG_VALID_CHANNEL_LIST_LEN)
			num20MHzChannelsFound = CFG_VALID_CHANNEL_LIST_LEN;
		mac->scan.numChannelsDefault = num20MHzChannelsFound;
		/* Move the channel list to the global data */
		/* structure -- this will be used as the scan list */
		for (Index = 0; Index < num20MHzChannelsFound; Index++)
			mac->scan.base_channels.channel_freq_list[Index] =
				mac->scan.defaultPowerTable[Index].center_freq;
		mac->scan.base_channels.numChannels =
			num20MHzChannelsFound;
	}
	return status;
}

QDF_STATUS csr_apply_channel_and_power_list(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	csr_prune_channel_list_for_mode(mac, &mac->scan.base_channels);
	csr_save_channel_power_for_band(mac, false);
	csr_save_channel_power_for_band(mac, true);
	csr_apply_channel_power_info_to_fw(mac,
					   &mac->scan.base_channels,
					   mac->scan.countryCodeCurrent);

	csr_init_operating_classes(mac);
	return status;
}

static QDF_STATUS csr_init11d_info(struct mac_context *mac, tCsr11dinfo *ps11dinfo)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t index;
	uint32_t count = 0;
	struct pwr_channel_info *pChanInfo;
	struct pwr_channel_info *pChanInfoStart;
	bool applyConfig = true;

	if (!ps11dinfo)
		return status;

	if (ps11dinfo->Channels.numChannels
	    && (CFG_VALID_CHANNEL_LIST_LEN >=
		ps11dinfo->Channels.numChannels)) {
		mac->scan.base_channels.numChannels =
			ps11dinfo->Channels.numChannels;
		qdf_mem_copy(mac->scan.base_channels.channel_freq_list,
			     ps11dinfo->Channels.channel_freq_list,
			     ps11dinfo->Channels.numChannels);
	} else {
		/* No change */
		return QDF_STATUS_SUCCESS;
	}
	/* legacy maintenance */

	qdf_mem_copy(mac->scan.countryCodeDefault, ps11dinfo->countryCode,
		     REG_ALPHA2_LEN + 1);

	/* Tush: at csropen get this initialized with default,
	 * during csr reset if this already set with some value
	 * no need initilaize with default again
	 */
	if (0 == mac->scan.countryCodeCurrent[0]) {
		qdf_mem_copy(mac->scan.countryCodeCurrent,
			     ps11dinfo->countryCode, REG_ALPHA2_LEN + 1);
	}
	/* need to add the max power channel list */
	pChanInfo =
		qdf_mem_malloc(sizeof(struct pwr_channel_info) *
			       CFG_VALID_CHANNEL_LIST_LEN);
	if (pChanInfo) {
		pChanInfoStart = pChanInfo;
		for (index = 0; index < ps11dinfo->Channels.numChannels;
		     index++) {
			pChanInfo->first_freq = ps11dinfo->ChnPower[index].first_chan_freq;
			pChanInfo->num_chan =
				ps11dinfo->ChnPower[index].numChannels;
			pChanInfo->max_tx_pwr =
				ps11dinfo->ChnPower[index].maxtxPower;
			pChanInfo++;
			count++;
		}
		if (count) {
			status = csr_save_to_channel_power2_g_5_g(mac,
						count *
						sizeof(struct pwr_channel_info),
						pChanInfoStart);
		}
		qdf_mem_free(pChanInfoStart);
	}
	/* Only apply them to CFG when not in STOP state.
	 * Otherwise they will be applied later
	 */
	if (QDF_IS_STATUS_SUCCESS(status)) {
		for (index = 0; index < WLAN_MAX_VDEVS; index++) {
			if ((CSR_IS_SESSION_VALID(mac, index))
			    && CSR_IS_ROAM_STOP(mac, index)) {
				applyConfig = false;
			}
		}

		if (true == applyConfig) {
			/* Apply the base channel list, power info,
			 * and set the Country code.
			 */
			csr_apply_channel_power_info_to_fw(mac,
							   &mac->scan.
							   base_channels,
							   mac->scan.
							   countryCodeCurrent);
		}
	}
	return status;
}

/* Initialize the Channel + Power List in the local cache and in the CFG */
QDF_STATUS csr_init_channel_power_list(struct mac_context *mac,
					tCsr11dinfo *ps11dinfo)
{
	uint8_t index;
	uint32_t count = 0;
	struct pwr_channel_info *pChanInfo;
	struct pwr_channel_info *pChanInfoStart;

	if (!ps11dinfo || !mac)
		return QDF_STATUS_E_FAILURE;

	pChanInfo =
		qdf_mem_malloc(sizeof(struct pwr_channel_info) *
			       CFG_VALID_CHANNEL_LIST_LEN);
	if (pChanInfo) {
		pChanInfoStart = pChanInfo;

		for (index = 0; index < ps11dinfo->Channels.numChannels;
		     index++) {
			pChanInfo->first_freq = ps11dinfo->ChnPower[index].first_chan_freq;
			pChanInfo->num_chan =
				ps11dinfo->ChnPower[index].numChannels;
			pChanInfo->max_tx_pwr =
				ps11dinfo->ChnPower[index].maxtxPower;
			pChanInfo++;
			count++;
		}
		if (count) {
			csr_save_to_channel_power2_g_5_g(mac,
						count *
						sizeof(struct pwr_channel_info),
						pChanInfoStart);
		}
		qdf_mem_free(pChanInfoStart);
	}

	return QDF_STATUS_SUCCESS;
}

#ifndef FEATURE_CM_ENABLE
/**
 * csr_roam_remove_duplicate_cmd_from_list()- Remove duplicate roam cmd from
 * list
 *
 * @mac_ctx: pointer to global mac
 * @vdev_id: vdev_id for the cmd
 * @list: pending list from which cmd needs to be removed
 * @command: cmd to be removed, can be NULL
 * @roam_reason: cmd with reason to be removed
 *
 * Remove duplicate command from the pending list.
 *
 * Return: void
 */
static void csr_roam_remove_duplicate_pending_cmd_from_list(
			struct mac_context *mac_ctx,
			uint32_t vdev_id,
			tSmeCmd *command, enum csr_roam_reason roam_reason)
{
	tListElem *entry, *next_entry;
	tSmeCmd *dup_cmd;
	tDblLinkList local_list;

	qdf_mem_zero(&local_list, sizeof(tDblLinkList));
	if (!QDF_IS_STATUS_SUCCESS(csr_ll_open(&local_list))) {
		sme_err("failed to open list");
		return;
	}
	entry = csr_nonscan_pending_ll_peek_head(mac_ctx, LL_ACCESS_NOLOCK);
	while (entry) {
		next_entry = csr_nonscan_pending_ll_next(mac_ctx, entry,
						LL_ACCESS_NOLOCK);
		dup_cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
		/*
		 * If command is not NULL remove the similar duplicate cmd for
		 * same reason as command. If command is NULL then check if
		 * roam_reason is eCsrForcedDisassoc (disconnect) and remove
		 * all roam command for the sessionId, else if roam_reason is
		 * eCsrHddIssued (connect) remove all connect (non disconenct)
		 * commands.
		 */
		if ((command && (command->vdev_id == dup_cmd->vdev_id) &&
			((command->command == dup_cmd->command) &&
			/*
			 * This peermac check is required for Softap/GO
			 * scenarios. for STA scenario below OR check will
			 * suffice as command will always be NULL for
			 * STA scenarios
			 */
			(!qdf_mem_cmp(dup_cmd->u.roamCmd.peerMac,
				command->u.roamCmd.peerMac,
					sizeof(QDF_MAC_ADDR_SIZE))) &&
				((command->u.roamCmd.roamReason ==
					dup_cmd->u.roamCmd.roamReason) ||
				(eCsrForcedDisassoc ==
					command->u.roamCmd.roamReason) ||
				(eCsrHddIssued ==
					command->u.roamCmd.roamReason)))) ||
			/* OR if pCommand is NULL */
			((vdev_id == dup_cmd->vdev_id) &&
			(eSmeCommandRoam == dup_cmd->command) &&
			((eCsrForcedDisassoc == roam_reason) ||
			(eCsrHddIssued == roam_reason &&
			!CSR_IS_DISCONNECT_COMMAND(dup_cmd))))) {
			sme_debug("RoamReason: %d",
					dup_cmd->u.roamCmd.roamReason);
			/* Insert to local_list and remove later */
			csr_ll_insert_tail(&local_list, entry,
					   LL_ACCESS_NOLOCK);
		}
		entry = next_entry;
	}

	while ((entry = csr_ll_remove_head(&local_list, LL_ACCESS_NOLOCK))) {
		dup_cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
		/* Tell caller that the command is cancelled */
		csr_roam_call_callback(mac_ctx, dup_cmd->vdev_id, NULL,
				dup_cmd->u.roamCmd.roamId,
				eCSR_ROAM_CANCELLED, eCSR_ROAM_RESULT_NONE);
		csr_release_command(mac_ctx, dup_cmd);
	}
	csr_ll_close(&local_list);
}

/**
 * csr_roam_remove_duplicate_command()- Remove duplicate roam cmd
 * from pending lists.
 *
 * @mac_ctx: pointer to global mac
 * @session_id: session id for the cmd
 * @command: cmd to be removed, can be null
 * @roam_reason: cmd with reason to be removed
 *
 * Remove duplicate command from the sme and roam pending list.
 *
 * Return: void
 */
void csr_roam_remove_duplicate_command(struct mac_context *mac_ctx,
			uint32_t session_id, tSmeCmd *command,
			enum csr_roam_reason roam_reason)
{
	csr_roam_remove_duplicate_pending_cmd_from_list(mac_ctx,
		session_id, command, roam_reason);
}

/**
 * csr_roam_populate_channels() - Helper function to populate channels
 * @beacon_ies: pointer to beacon ie
 * @roam_info: Roaming related information
 * @chan1: center freq 1
 * @chan2: center freq2
 *
 * This function will issue populate chan1 and chan2 based on beacon ie
 *
 * Return: none.
 */
static void csr_roam_populate_channels(tDot11fBeaconIEs *beacon_ies,
			struct csr_roam_info *roam_info,
			uint8_t *chan1, uint8_t *chan2)
{
	ePhyChanBondState phy_state;

	if (beacon_ies->VHTOperation.present) {
		*chan1 = beacon_ies->VHTOperation.chan_center_freq_seg0;
		*chan2 = beacon_ies->VHTOperation.chan_center_freq_seg1;
		roam_info->chan_info.info = MODE_11AC_VHT80;
	} else if (beacon_ies->HTInfo.present) {
		if (beacon_ies->HTInfo.recommendedTxWidthSet ==
			eHT_CHANNEL_WIDTH_40MHZ) {
			phy_state = beacon_ies->HTInfo.secondaryChannelOffset;
			if (phy_state == PHY_DOUBLE_CHANNEL_LOW_PRIMARY)
				*chan1 = beacon_ies->HTInfo.primaryChannel +
						CSR_CB_CENTER_CHANNEL_OFFSET;
			else if (phy_state == PHY_DOUBLE_CHANNEL_HIGH_PRIMARY)
				*chan1 = beacon_ies->HTInfo.primaryChannel -
						CSR_CB_CENTER_CHANNEL_OFFSET;
			else
				*chan1 = beacon_ies->HTInfo.primaryChannel;

			roam_info->chan_info.info = MODE_11NA_HT40;
		} else {
			*chan1 = beacon_ies->HTInfo.primaryChannel;
			roam_info->chan_info.info = MODE_11NA_HT20;
		}
		*chan2 = 0;
	} else {
		*chan1 = 0;
		*chan2 = 0;
		roam_info->chan_info.info = MODE_11A;
	}
}
#endif

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
#ifndef FEATURE_CM_ENABLE
static void
csr_cm_connect_info(struct mac_context *mac_ctx, uint8_t vdev_id,
		    bool connect_success, struct qdf_mac_addr *bssid,
		    qdf_freq_t freq)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_ssid ssid;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("vdev object is NULL for vdev %d", vdev_id);
		return;
	}
	wlan_mlme_get_ssid_vdev_id(mac_ctx->pdev, vdev_id,
				   ssid.ssid, &ssid.length);
	cm_connect_info(vdev, connect_success, bssid, &ssid, freq);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
}
#endif
#ifdef WLAN_UNIT_TEST
void csr_cm_get_sta_cxn_info(struct mac_context *mac_ctx, uint8_t vdev_id,
			     char *buf, uint32_t buf_sz)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("vdev object is NULL for vdev %d", vdev_id);
		return;
	}

	cm_get_sta_cxn_info(vdev, buf, buf_sz);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

}
#endif
#else
#ifndef FEATURE_CM_ENABLE
static inline void
csr_cm_connect_info(struct mac_context *mac_ctx, uint8_t vdev_id,
		    bool connect_success, struct qdf_mac_addr *bssid,
		    qdf_freq_t freq)
{
}
#endif
#endif

QDF_STATUS csr_roam_call_callback(struct mac_context *mac, uint32_t sessionId,
				  struct csr_roam_info *roam_info,
				  uint32_t roamId,
				  eRoamCmdStatus u1, eCsrRoamResult u2)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession;
	qdf_freq_t chan_freq;
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	WLAN_HOST_DIAG_EVENT_DEF(connectionStatus,
			host_event_wlan_status_payload_type);
#endif
	tDot11fBeaconIEs *beacon_ies = NULL;
	uint8_t chan1, chan2;
#endif
	if (!CSR_IS_SESSION_VALID(mac, sessionId)) {
		sme_err("Session ID: %d is not valid", sessionId);
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAILURE;
	}
	pSession = CSR_GET_SESSION(mac, sessionId);

	if (false == pSession->sessionActive) {
		sme_debug("Session is not Active");
		return QDF_STATUS_E_FAILURE;
	}
	chan_freq = wlan_get_operation_chan_freq_vdev_id(mac->pdev, sessionId);
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	if (eCSR_ROAM_ASSOCIATION_COMPLETION == u1 &&
			eCSR_ROAM_RESULT_ASSOCIATED == u2 && roam_info) {
		sme_debug("Assoc complete result: %d status: %d reason: %d",
			u2, roam_info->status_code, roam_info->reasonCode);
		beacon_ies = qdf_mem_malloc(sizeof(tDot11fBeaconIEs));
		if ((beacon_ies) && (roam_info->bss_desc)) {
			status = csr_parse_bss_description_ies(
					mac, roam_info->bss_desc,
					beacon_ies);
			csr_roam_populate_channels(beacon_ies, roam_info,
					&chan1, &chan2);
			if (0 != chan1)
				roam_info->chan_info.band_center_freq1 =
					cds_chan_to_freq(chan1);
			else
				roam_info->chan_info.band_center_freq1 = 0;
			if (0 != chan2)
				roam_info->chan_info.band_center_freq2 =
					cds_chan_to_freq(chan2);
			else
				roam_info->chan_info.band_center_freq2 = 0;
		} else {
			roam_info->chan_info.band_center_freq1 = 0;
			roam_info->chan_info.band_center_freq2 = 0;
			roam_info->chan_info.info = 0;
		}
		roam_info->chan_info.mhz = chan_freq;
		roam_info->chan_info.reg_info_1 =
			(csr_get_cfg_max_tx_power(mac, chan_freq) << 16);
		roam_info->chan_info.reg_info_2 =
			(csr_get_cfg_max_tx_power(mac, chan_freq) << 8);
		qdf_mem_free(beacon_ies);
	} else if ((u1 == eCSR_ROAM_FT_REASSOC_FAILED)
			&& (pSession->bRefAssocStartCnt)) {
		/*
		 * Decrement bRefAssocStartCnt for FT reassoc failure.
		 * Reason: For FT reassoc failures, we first call
		 * csr_roam_call_callback before notifying a failed roam
		 * completion through csr_roam_complete. The latter in
		 * turn calls csr_roam_process_results which tries to
		 * once again call csr_roam_call_callback if bRefAssocStartCnt
		 * is non-zero. Since this is redundant for FT reassoc
		 * failure, decrement bRefAssocStartCnt.
		 */
		pSession->bRefAssocStartCnt--;
	} else if (eCSR_ROAM_ASSOCIATION_COMPLETION == u1 && roam_info &&
	    roam_info->u.pConnectedProfile) {
		csr_cm_connect_info(mac, sessionId,
			(u2 == eCSR_ROAM_RESULT_ASSOCIATED) ? true : false,
			&roam_info->bssid, chan_freq);
	}
#endif

	if (mac->session_roam_complete_cb) {
		if (roam_info)
			roam_info->sessionId = (uint8_t) sessionId;
		status = mac->session_roam_complete_cb(mac->psoc, sessionId, roam_info,
						       roamId, u1, u2);
	}
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	/*
	 * EVENT_WLAN_STATUS_V2: eCSR_ROAM_ASSOCIATION_COMPLETION,
	 *                    eCSR_ROAM_LOSTLINK,
	 *                    eCSR_ROAM_DISASSOCIATED,
	 */
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	qdf_mem_zero(&connectionStatus,
			sizeof(host_event_wlan_status_payload_type));

	if ((eCSR_ROAM_MIC_ERROR_IND == u1)
			|| (eCSR_ROAM_RESULT_MIC_FAILURE == u2)) {
		qdf_mem_copy(&connectionStatus,
			     &mac->mlme_cfg->sta.event_payload,
			     sizeof(host_event_wlan_status_payload_type));
		connectionStatus.rssi = mac->mlme_cfg->sta.current_rssi;
		connectionStatus.eventId = DIAG_WLAN_STATUS_DISCONNECT;
		connectionStatus.reason = DIAG_REASON_MIC_ERROR;
		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
	if (eCSR_ROAM_RESULT_FORCED == u2) {
		qdf_mem_copy(&connectionStatus, &mac->mlme_cfg->sta.event_payload,
				sizeof(host_event_wlan_status_payload_type));
		connectionStatus.rssi = mac->mlme_cfg->sta.current_rssi;
		connectionStatus.eventId = DIAG_WLAN_STATUS_DISCONNECT;
		connectionStatus.reason = DIAG_REASON_USER_REQUESTED;
		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
	if (eCSR_ROAM_RESULT_DISASSOC_IND == u2) {
		qdf_mem_copy(&connectionStatus, &mac->mlme_cfg->sta.event_payload,
				sizeof(host_event_wlan_status_payload_type));
		connectionStatus.rssi = mac->mlme_cfg->sta.current_rssi;
		connectionStatus.eventId = DIAG_WLAN_STATUS_DISCONNECT;
		connectionStatus.reason = DIAG_REASON_DISASSOC;
		if (roam_info)
			connectionStatus.reasonDisconnect =
				roam_info->reasonCode;

		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
	if (eCSR_ROAM_RESULT_DEAUTH_IND == u2) {
		qdf_mem_copy(&connectionStatus, &mac->mlme_cfg->sta.event_payload,
				sizeof(host_event_wlan_status_payload_type));
		connectionStatus.rssi = mac->mlme_cfg->sta.current_rssi;
		connectionStatus.eventId = DIAG_WLAN_STATUS_DISCONNECT;
		connectionStatus.reason = DIAG_REASON_DEAUTH;
		if (roam_info)
			connectionStatus.reasonDisconnect =
				roam_info->reasonCode;
		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
#endif

	return status;
}

static bool csr_peer_mac_match_cmd(tSmeCmd *sme_cmd,
				   struct qdf_mac_addr peer_macaddr,
				   uint8_t vdev_id)
{
	if (sme_cmd->command == eSmeCommandRoam &&
	    (sme_cmd->u.roamCmd.roamReason == eCsrForcedDisassocSta ||
	     sme_cmd->u.roamCmd.roamReason == eCsrForcedDeauthSta) &&
	    !qdf_mem_cmp(peer_macaddr.bytes, sme_cmd->u.roamCmd.peerMac,
			 QDF_MAC_ADDR_SIZE))
		return true;

	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	/*
	 * For STA/CLI mode for NB disconnect peer mac is not stored, so check
	 * vdev id as there is only one bssid/peer for STA/CLI.
	 */
	if (CSR_IS_DISCONNECT_COMMAND(sme_cmd) && sme_cmd->vdev_id == vdev_id)
		return true;
#endif

	if (sme_cmd->command == eSmeCommandWmStatusChange) {
		struct wmstatus_changecmd *wms_cmd;

		wms_cmd = &sme_cmd->u.wmStatusChangeCmd;
		if (wms_cmd->Type == eCsrDisassociated &&
		    !qdf_mem_cmp(peer_macaddr.bytes,
				 wms_cmd->u.DisassocIndMsg.peer_macaddr.bytes,
				 QDF_MAC_ADDR_SIZE))
			return true;

		if (wms_cmd->Type == eCsrDeauthenticated &&
		    !qdf_mem_cmp(peer_macaddr.bytes,
				 wms_cmd->u.DeauthIndMsg.peer_macaddr.bytes,
				 QDF_MAC_ADDR_SIZE))
			return true;
	}

	return false;
}

static bool
csr_is_deauth_disassoc_in_pending_q(struct mac_context *mac_ctx,
				    uint8_t vdev_id,
				    struct qdf_mac_addr peer_macaddr)
{
	tListElem *entry = NULL;
	tSmeCmd *sme_cmd;

	entry = csr_nonscan_pending_ll_peek_head(mac_ctx, LL_ACCESS_NOLOCK);
	while (entry) {
		sme_cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
		if ((sme_cmd->vdev_id == vdev_id) &&
		    csr_peer_mac_match_cmd(sme_cmd, peer_macaddr, vdev_id))
			return true;
		entry = csr_nonscan_pending_ll_next(mac_ctx, entry,
						    LL_ACCESS_NOLOCK);
	}

	return false;
}

static bool
csr_is_deauth_disassoc_in_active_q(struct mac_context *mac_ctx,
				   uint8_t vdev_id,
				   struct qdf_mac_addr peer_macaddr)
{
	tSmeCmd *sme_cmd;

	sme_cmd = wlan_serialization_get_active_cmd(mac_ctx->psoc, vdev_id,
						WLAN_SER_CMD_FORCE_DEAUTH_STA);

	if (sme_cmd && csr_peer_mac_match_cmd(sme_cmd, peer_macaddr, vdev_id))
		return true;

	sme_cmd = wlan_serialization_get_active_cmd(mac_ctx->psoc, vdev_id,
					WLAN_SER_CMD_FORCE_DISASSOC_STA);

	if (sme_cmd && csr_peer_mac_match_cmd(sme_cmd, peer_macaddr, vdev_id))
		return true;

	/*
	 * WLAN_SER_CMD_WM_STATUS_CHANGE is of two type, the handling
	 * should take care of both the types.
	 */
	sme_cmd = wlan_serialization_get_active_cmd(mac_ctx->psoc, vdev_id,
						WLAN_SER_CMD_WM_STATUS_CHANGE);
	if (sme_cmd && csr_peer_mac_match_cmd(sme_cmd, peer_macaddr, vdev_id))
		return true;

	return false;
}

/*
 * csr_is_deauth_disassoc_already_active() - Function to check if deauth or
 *  disassoc is already in progress.
 * @mac_ctx: Global MAC context
 * @vdev_id: vdev id
 * @peer_macaddr: Peer MAC address
 *
 * Return: True if deauth/disassoc indication can be dropped
 *  else false
 */
static bool
csr_is_deauth_disassoc_already_active(struct mac_context *mac_ctx,
				      uint8_t vdev_id,
				      struct qdf_mac_addr peer_macaddr)
{
	bool ret = csr_is_deauth_disassoc_in_pending_q(mac_ctx,
						      vdev_id,
						      peer_macaddr);
	if (!ret)
		/**
		 * commands are not found in pending queue, check in active
		 * queue as well
		 */
		ret = csr_is_deauth_disassoc_in_active_q(mac_ctx,
							  vdev_id,
							  peer_macaddr);

	if (ret)
		sme_debug("Deauth/Disassoc already in progress for "QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(peer_macaddr.bytes));

	return ret;
}

static void csr_roam_issue_disconnect_stats(struct mac_context *mac,
					    uint32_t session_id,
					    struct qdf_mac_addr peer_mac)
{
	tSmeCmd *cmd;

	cmd = csr_get_command_buffer(mac);
	if (!cmd) {
		sme_err(" fail to get command buffer");
		return;
	}

	cmd->command = eSmeCommandGetdisconnectStats;
	cmd->vdev_id = session_id;
	qdf_mem_copy(&cmd->u.disconnect_stats_cmd.peer_mac_addr, &peer_mac,
		     QDF_MAC_ADDR_SIZE);
	if (QDF_IS_STATUS_ERROR(csr_queue_sme_command(mac, cmd, true)))
		sme_err("fail to queue get disconnect stats");
}

/**
 * csr_roam_issue_disassociate_sta_cmd() - disassociate a associated station
 * @sessionId:     Session Id for Soft AP
 * @p_del_sta_params: Pointer to parameters of the station to disassoc
 *
 * CSR function that HDD calls to delete a associated station
 *
 * Return: QDF_STATUS_SUCCESS on success or another QDF_STATUS_* on error
 */
QDF_STATUS csr_roam_issue_disassociate_sta_cmd(struct mac_context *mac,
					       uint32_t sessionId,
					       struct csr_del_sta_params
					       *p_del_sta_params)

{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	do {
		if (csr_is_deauth_disassoc_already_active(mac, sessionId,
					      p_del_sta_params->peerMacAddr))
			break;
		pCommand = csr_get_command_buffer(mac);
		if (!pCommand) {
			sme_err("fail to get command buffer");
			status = QDF_STATUS_E_RESOURCES;
			break;
		}
		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamReason = eCsrForcedDisassocSta;
		qdf_mem_copy(pCommand->u.roamCmd.peerMac,
				p_del_sta_params->peerMacAddr.bytes,
				sizeof(pCommand->u.roamCmd.peerMac));
		pCommand->u.roamCmd.reason = p_del_sta_params->reason_code;

		csr_roam_issue_disconnect_stats(
					mac, sessionId,
					p_del_sta_params->peerMacAddr);

		status = csr_queue_sme_command(mac, pCommand, false);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);
	} while (0);

	return status;
}

/**
 * csr_roam_issue_deauthSta() - delete a associated station
 * @sessionId:     Session Id for Soft AP
 * @pDelStaParams: Pointer to parameters of the station to deauthenticate
 *
 * CSR function that HDD calls to delete a associated station
 *
 * Return: QDF_STATUS_SUCCESS on success or another QDF_STATUS_** on error
 */
QDF_STATUS csr_roam_issue_deauth_sta_cmd(struct mac_context *mac,
		uint32_t sessionId,
		struct csr_del_sta_params *pDelStaParams)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	do {
		if (csr_is_deauth_disassoc_already_active(mac, sessionId,
					      pDelStaParams->peerMacAddr))
			break;
		pCommand = csr_get_command_buffer(mac);
		if (!pCommand) {
			sme_err("fail to get command buffer");
			status = QDF_STATUS_E_RESOURCES;
			break;
		}
		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamReason = eCsrForcedDeauthSta;
		qdf_mem_copy(pCommand->u.roamCmd.peerMac,
			     pDelStaParams->peerMacAddr.bytes,
			     sizeof(tSirMacAddr));
		pCommand->u.roamCmd.reason = pDelStaParams->reason_code;

		csr_roam_issue_disconnect_stats(mac, sessionId,
						pDelStaParams->peerMacAddr);

		status = csr_queue_sme_command(mac, pCommand, false);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);
	} while (0);

	return status;
}

#ifndef FEATURE_CM_ENABLE
static
QDF_STATUS csr_roam_issue_deauth(struct mac_context *mac, uint32_t sessionId,
				 enum csr_roam_substate NewSubstate)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr bssId = QDF_MAC_ADDR_BCAST_INIT;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (pSession->pConnectBssDesc) {
		qdf_mem_copy(bssId.bytes, pSession->pConnectBssDesc->bssId,
			     sizeof(struct qdf_mac_addr));
	}
	sme_debug("Deauth to Bssid " QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(bssId.bytes));
	csr_roam_substate_change(mac, NewSubstate, sessionId);

	status =
		csr_send_mb_deauth_req_msg(mac, sessionId, bssId.bytes,
					   REASON_DEAUTH_NETWORK_LEAVING);
	if (QDF_IS_STATUS_SUCCESS(status))
		csr_roam_link_down(mac, sessionId);
	else {
		sme_err("csr_send_mb_deauth_req_msg failed with status %d Session ID: %d"
			QDF_MAC_ADDR_FMT, status, sessionId,
			QDF_MAC_ADDR_REF(bssId.bytes));
	}

	return status;
}

QDF_STATUS csr_roam_save_connected_bss_desc(struct mac_context *mac,
					    uint32_t sessionId,
					    struct bss_description *bss_desc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	uint32_t size;

	if (!pSession) {
		sme_err("  session %d not found ", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	/* If no BSS description was found in this connection
	 * , then nix the BSS description
	 * that we keep around for the connected BSS) and get out.
	 */
	if (!bss_desc) {
		csr_free_connect_bss_desc(mac, sessionId);
	} else {
		size = bss_desc->length + sizeof(bss_desc->length);
		if (pSession->pConnectBssDesc) {
			if (((pSession->pConnectBssDesc->length) +
			     sizeof(pSession->pConnectBssDesc->length)) <
			    size) {
				/* not enough room for the new BSS,
				 * mac->roam.pConnectBssDesc is freed inside
				 */
				csr_free_connect_bss_desc(mac, sessionId);
			}
		}
		if (!pSession->pConnectBssDesc)
			pSession->pConnectBssDesc = qdf_mem_malloc(size);

		if (!pSession->pConnectBssDesc)
			status = QDF_STATUS_E_NOMEM;
		else
			qdf_mem_copy(pSession->pConnectBssDesc, bss_desc, size);
	}
	return status;
}

static
QDF_STATUS csr_roam_prepare_bss_config(struct mac_context *mac,
				       struct csr_roam_profile *pProfile,
				       struct bss_description *bss_desc,
				       struct bss_config_param *pBssConfig,
				       tDot11fBeaconIEs *pIes)
{
	enum csr_cfgdot11mode cfgDot11Mode;
	uint32_t join_timeout;
	enum reg_wifi_band band;

	QDF_ASSERT(pIes);
	if (!pIes)
		return QDF_STATUS_E_FAILURE;
	if (!pProfile) {
		sme_debug("Profile is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (!bss_desc) {
		sme_debug("bss_desc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_copy(&pBssConfig->BssCap, &bss_desc->capabilityInfo,
		     sizeof(tSirMacCapabilityInfo));
	/* get qos */
	pBssConfig->qosType = csr_get_qos_from_bss_desc(mac, bss_desc, pIes);
	/* Take SSID always from profile */
	qdf_mem_copy(&pBssConfig->SSID.ssId,
		     pProfile->SSIDs.SSIDList->SSID.ssId,
		     pProfile->SSIDs.SSIDList->SSID.length);
	pBssConfig->SSID.length = pProfile->SSIDs.SSIDList->SSID.length;

	if (csr_is_nullssid(pBssConfig->SSID.ssId, pBssConfig->SSID.length)) {
		sme_warn("BSS desc SSID is a wild card");
		/* Return failed if profile doesn't have an SSID either. */
		if (pProfile->SSIDs.numOfSSIDs == 0) {
			sme_warn("BSS desc and profile doesn't have SSID");
			return QDF_STATUS_E_FAILURE;
		}
	}

	if (WLAN_REG_IS_5GHZ_CH_FREQ(bss_desc->chan_freq))
		band = REG_BAND_5G;
	else if (WLAN_REG_IS_24GHZ_CH_FREQ(bss_desc->chan_freq))
		band = REG_BAND_2G;
	else if (WLAN_REG_IS_6GHZ_CHAN_FREQ(bss_desc->chan_freq))
		band = REG_BAND_6G;
	else
		return QDF_STATUS_E_FAILURE;

		/* phymode */
	if (csr_is_phy_mode_match(mac, pProfile->phyMode, bss_desc,
				  pProfile, &cfgDot11Mode, pIes)) {
		pBssConfig->uCfgDot11Mode = cfgDot11Mode;
	} else {
		/*
		 * No matching phy mode found, force to 11b/g based on INI for
		 * 2.4Ghz and to 11a mode for 5Ghz
		 */
		sme_warn("Can not find match phy mode");
		if (REG_BAND_2G == band) {
			if (mac->roam.configParam.phyMode &
			    (eCSR_DOT11_MODE_11b | eCSR_DOT11_MODE_11b_ONLY)) {
				pBssConfig->uCfgDot11Mode =
						eCSR_CFG_DOT11_MODE_11B;
			} else {
				pBssConfig->uCfgDot11Mode =
						eCSR_CFG_DOT11_MODE_11G;
			}
		} else if (band == REG_BAND_5G) {
			pBssConfig->uCfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
		} else if (band == REG_BAND_6G) {
			// Still use 11AX even 11BE is supported
			pBssConfig->uCfgDot11Mode =
						eCSR_CFG_DOT11_MODE_11AX_ONLY;
		}
	}

	sme_debug("phyMode=%d, uCfgDot11Mode=%d negotiatedAuthType %d",
		   pProfile->phyMode, pBssConfig->uCfgDot11Mode,
		   pProfile->negotiatedAuthType);

	/* Qos */
	if ((pBssConfig->uCfgDot11Mode != eCSR_CFG_DOT11_MODE_11N) &&
	    (mac->roam.configParam.WMMSupportMode == eCsrRoamWmmNoQos)) {
		/*
		 * Joining BSS is not 11n capable and WMM is disabled on client.
		 * Disable QoS and WMM
		 */
		pBssConfig->qosType = eCSR_MEDIUM_ACCESS_DCF;
	}

	if (((pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11N) ||
	     (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AC)) &&
	     ((pBssConfig->qosType != eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP) &&
	      (pBssConfig->qosType != eCSR_MEDIUM_ACCESS_11e_HCF) &&
	      (pBssConfig->qosType != eCSR_MEDIUM_ACCESS_11e_eDCF))) {
		/* Joining BSS is 11n capable and WMM is disabled on AP. */
		/* Assume all HT AP's are QOS AP's and enable WMM */
		pBssConfig->qosType = eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP;
	}
	/* auth type */
	switch (pProfile->negotiatedAuthType) {
	default:
	case eCSR_AUTH_TYPE_WPA:
	case eCSR_AUTH_TYPE_WPA_PSK:
	case eCSR_AUTH_TYPE_WPA_NONE:
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		pBssConfig->authType = eSIR_OPEN_SYSTEM;
		break;
	case eCSR_AUTH_TYPE_SHARED_KEY:
		pBssConfig->authType = eSIR_SHARED_KEY;
		break;
	case eCSR_AUTH_TYPE_AUTOSWITCH:
		pBssConfig->authType = eSIR_AUTO_SWITCH;
		break;
	case eCSR_AUTH_TYPE_SAE:
	case eCSR_AUTH_TYPE_FT_SAE:
		pBssConfig->authType = eSIR_AUTH_TYPE_SAE;
		break;
	}
	/* short slot time */
	if (eCSR_CFG_DOT11_MODE_11B != cfgDot11Mode)
		mac->mlme_cfg->feature_flags.enable_short_slot_time_11g =
			mac->mlme_cfg->ht_caps.short_slot_time_enabled;
	else
		mac->mlme_cfg->feature_flags.enable_short_slot_time_11g = 0;

	/* power constraint */
	mac->mlme_cfg->power.local_power_constraint =
		wlan_get_11h_power_constraint(mac, &pIes->PowerConstraints);

	/*
	 * Join timeout: if we find a BeaconInterval in the BssDescription,
	 * then set the Join Timeout to be 10 x the BeaconInterval.
	 */
	join_timeout = cfg_default(CFG_JOIN_FAILURE_TIMEOUT);
	if (bss_desc->beaconInterval)
		/* Make sure it is bigger than the minimal */
		join_timeout = QDF_MAX(10 * bss_desc->beaconInterval,
				       cfg_min(CFG_JOIN_FAILURE_TIMEOUT));

	mac->mlme_cfg->timeouts.join_failure_timeout =
		QDF_MIN(join_timeout, cfg_default(CFG_JOIN_FAILURE_TIMEOUT));

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS csr_roam_prepare_bss_config_from_profile(
	struct mac_context *mac, struct csr_roam_profile *pProfile,
					struct bss_config_param *pBssConfig,
					struct bss_description *bss_desc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t bss_op_ch_freq = 0;
	uint8_t qAPisEnabled = false;
	enum reg_wifi_band band;

	/* SSID */
	pBssConfig->SSID.length = 0;
	if (pProfile->SSIDs.numOfSSIDs) {
		/* only use the first one */
		qdf_mem_copy(&pBssConfig->SSID,
			     &pProfile->SSIDs.SSIDList[0].SSID,
			     sizeof(tSirMacSSid));
	} else {
		/* SSID must present */
		return QDF_STATUS_E_FAILURE;
	}
	/* Settomg up the capabilities */
	pBssConfig->BssCap.ess = 1;

	if (eCSR_ENCRYPT_TYPE_NONE !=
	    pProfile->EncryptionType.encryptionType[0])
		pBssConfig->BssCap.privacy = 1;

	/* phymode */
	if (pProfile->ChannelInfo.freq_list)
		bss_op_ch_freq = pProfile->ChannelInfo.freq_list[0];
	pBssConfig->uCfgDot11Mode = csr_roam_get_phy_mode_band_for_bss(
						mac, pProfile, bss_op_ch_freq,
						&band);
	/* QOS */
	/* Is this correct to always set to this // *** */
	if (pBssConfig->BssCap.ess == 1) {
		/*For Softap case enable WMM */
		if (CSR_IS_INFRA_AP(pProfile)
		    && (eCsrRoamWmmNoQos !=
			mac->roam.configParam.WMMSupportMode)) {
			qAPisEnabled = true;
		} else
		if (csr_roam_get_qos_info_from_bss(mac, bss_desc) ==
		    QDF_STATUS_SUCCESS) {
			qAPisEnabled = true;
		} else {
			qAPisEnabled = false;
		}
	} else {
		qAPisEnabled = true;
	}
	if ((eCsrRoamWmmNoQos != mac->roam.configParam.WMMSupportMode &&
	     qAPisEnabled) ||
	    ((eCSR_CFG_DOT11_MODE_11N == pBssConfig->uCfgDot11Mode &&
	      qAPisEnabled))) {
		pBssConfig->qosType = eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP;
	} else {
		pBssConfig->qosType = eCSR_MEDIUM_ACCESS_DCF;
	}

	/* auth type */
	/* Take the preferred Auth type. */
	switch (pProfile->AuthType.authType[0]) {
	default:
	case eCSR_AUTH_TYPE_WPA:
	case eCSR_AUTH_TYPE_WPA_PSK:
	case eCSR_AUTH_TYPE_WPA_NONE:
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		pBssConfig->authType = eSIR_OPEN_SYSTEM;
		break;
	case eCSR_AUTH_TYPE_SHARED_KEY:
		pBssConfig->authType = eSIR_SHARED_KEY;
		break;
	case eCSR_AUTH_TYPE_AUTOSWITCH:
		pBssConfig->authType = eSIR_AUTO_SWITCH;
		break;
	case eCSR_AUTH_TYPE_SAE:
	case eCSR_AUTH_TYPE_FT_SAE:
		pBssConfig->authType = eSIR_AUTH_TYPE_SAE;
		break;
	}
	/* short slot time */
	if (WNI_CFG_PHY_MODE_11B != pBssConfig->uCfgDot11Mode) {
		mac->mlme_cfg->feature_flags.enable_short_slot_time_11g =
			mac->mlme_cfg->ht_caps.short_slot_time_enabled;
	} else {
		mac->mlme_cfg->feature_flags.enable_short_slot_time_11g = 0;
	}

	return status;
}

static QDF_STATUS
csr_roam_get_qos_info_from_bss(struct mac_context *mac,
			       struct bss_description *bss_desc)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tDot11fBeaconIEs *pIes = NULL;

	do {
		if (!QDF_IS_STATUS_SUCCESS(
			csr_get_parsed_bss_description_ies(
				mac, bss_desc, &pIes))) {
			sme_err("csr_get_parsed_bss_description_ies() failed");
			break;
		}
		/* check if the AP is QAP & it supports APSD */
		if (CSR_IS_QOS_BSS(pIes))
			status = QDF_STATUS_SUCCESS;
	} while (0);

	if (pIes)
		qdf_mem_free(pIes);

	return status;
}

static QDF_STATUS csr_set_qos_to_cfg(struct mac_context *mac, uint32_t sessionId,
				     eCsrMediaAccessType qosType)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t QoSEnabled;
	uint32_t WmeEnabled;
	/* set the CFG enable/disable variables based on the
	 * qosType being configured.
	 */
	switch (qosType) {
	case eCSR_MEDIUM_ACCESS_WMM_eDCF_802dot1p:
		QoSEnabled = false;
		WmeEnabled = true;
		break;
	case eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP:
		QoSEnabled = false;
		WmeEnabled = true;
		break;
	case eCSR_MEDIUM_ACCESS_WMM_eDCF_NoClassify:
		QoSEnabled = false;
		WmeEnabled = true;
		break;
	case eCSR_MEDIUM_ACCESS_11e_eDCF:
		QoSEnabled = true;
		WmeEnabled = false;
		break;
	case eCSR_MEDIUM_ACCESS_11e_HCF:
		QoSEnabled = true;
		WmeEnabled = false;
		break;
	default:
	case eCSR_MEDIUM_ACCESS_DCF:
		QoSEnabled = false;
		WmeEnabled = false;
		break;
	}
	/* save the WMM setting for later use */
	mac->roam.roamSession[sessionId].fWMMConnection = (bool) WmeEnabled;
	mac->roam.roamSession[sessionId].fQOSConnection = (bool) QoSEnabled;
	return status;
}

static void csr_set_cfg_rate_set(struct mac_context *mac, eCsrPhyMode phyMode,
				 struct csr_roam_profile *pProfile,
				 struct bss_description *bss_desc,
				 tDot11fBeaconIEs *pIes,
				 uint32_t session_id)
{
	int i;
	uint8_t *pDstRate;
	enum csr_cfgdot11mode cfgDot11Mode;
	/* leave enough room for the max number of rates */
	uint8_t OperationalRates[CSR_DOT11_SUPPORTED_RATES_MAX];
	qdf_size_t OperationalRatesLength = 0;
	/* leave enough room for the max number of rates */
	uint8_t ExtendedOperationalRates
				[CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX];
	qdf_size_t ExtendedOperationalRatesLength = 0;
	uint8_t MCSRateIdxSet[SIZE_OF_SUPPORTED_MCS_SET];
	qdf_size_t MCSRateLength = 0;
	struct wlan_objmgr_vdev *vdev;

	QDF_ASSERT(pIes);
	if (pIes) {
		csr_is_phy_mode_match(mac, phyMode, bss_desc, pProfile,
				      &cfgDot11Mode, pIes);
		/* Originally, we thought that for 11a networks, the 11a rates
		 * are always in the Operational Rate set & for 11b and 11g
		 * networks, the 11b rates appear in the Operational Rate set.
		 * Consequently, in either case, we would blindly put the rates
		 * we support into our Operational Rate set (including the basic
		 * rates, which we have already verified are supported earlier
		 * in the roaming decision). However, it turns out that this is
		 * not always the case.  Some AP's (e.g. D-Link DI-784) ram 11g
		 * rates into the Operational Rate set, too.  Now, we're a
		 * little more careful:
		 */
		pDstRate = OperationalRates;
		if (pIes->SuppRates.present) {
			for (i = 0; i < pIes->SuppRates.num_rates; i++) {
				if (csr_rates_is_dot11_rate_supported
					    (mac, pIes->SuppRates.rates[i])
				    && (OperationalRatesLength <
					CSR_DOT11_SUPPORTED_RATES_MAX)) {
					*pDstRate++ = pIes->SuppRates.rates[i];
					OperationalRatesLength++;
				}
			}
		}
		if (eCSR_CFG_DOT11_MODE_11G == cfgDot11Mode ||
		    eCSR_CFG_DOT11_MODE_11N == cfgDot11Mode ||
		    eCSR_CFG_DOT11_MODE_ABG == cfgDot11Mode) {
			/* If there are Extended Rates in the beacon, we will
			 * reflect those extended rates that we support in out
			 * Extended Operational Rate set:
			 */
			pDstRate = ExtendedOperationalRates;
			if (pIes->ExtSuppRates.present) {
				for (i = 0; i < pIes->ExtSuppRates.num_rates;
				     i++) {
					if (csr_rates_is_dot11_rate_supported
						    (mac, pIes->ExtSuppRates.
							rates[i])
					    && (ExtendedOperationalRatesLength <
						CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX)) {
						*pDstRate++ =
							pIes->ExtSuppRates.
							rates[i];
						ExtendedOperationalRatesLength++;
					}
				}
			}
		}
		/* Enable proprietary MAC features if peer node is Airgo node
		 * and STA user wants to use them For ANI network companions,
		 * we need to populate the proprietary rate set with any
		 * proprietary rates we found in the beacon, only if user allows
		 * them.
		 */
		/* No proprietary modes... */
		/* Get MCS Rate */
		pDstRate = MCSRateIdxSet;
		if (pIes->HTCaps.present) {
			for (i = 0; i < VALID_MAX_MCS_INDEX; i++) {
				if ((unsigned int)pIes->HTCaps.
				    supportedMCSSet[0] & (1 << i)) {
					MCSRateLength++;
					*pDstRate++ = i;
				}
			}
		}
		/* Set the operational rate set CFG variables... */
		vdev = wlan_objmgr_get_vdev_by_id_from_pdev(
						mac->pdev, session_id,
						WLAN_LEGACY_SME_ID);
		if (vdev) {
			mlme_set_opr_rate(vdev, OperationalRates,
					  OperationalRatesLength);
			mlme_set_ext_opr_rate(vdev, ExtendedOperationalRates,
					      ExtendedOperationalRatesLength);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		} else {
			sme_err("null vdev");
		}

		wlan_mlme_set_cfg_str(MCSRateIdxSet,
				      &mac->mlme_cfg->rates.current_mcs_set,
				      MCSRateLength);
	} /* Parsing BSSDesc */
	else
		sme_err("failed to parse BssDesc");
}

static void csr_set_cfg_rate_set_from_profile(struct mac_context *mac,
					      struct csr_roam_profile *pProfile,
					      uint32_t session_id)
{
	tSirMacRateSetIE DefaultSupportedRates11a = { WLAN_ELEMID_RATES,
						      {8,
						       {SIR_MAC_RATE_6,
							SIR_MAC_RATE_9,
							SIR_MAC_RATE_12,
							SIR_MAC_RATE_18,
							SIR_MAC_RATE_24,
							SIR_MAC_RATE_36,
							SIR_MAC_RATE_48,
							SIR_MAC_RATE_54} } };
	tSirMacRateSetIE DefaultSupportedRates11b = { WLAN_ELEMID_RATES,
						      {4,
						       {SIR_MAC_RATE_1,
							SIR_MAC_RATE_2,
							SIR_MAC_RATE_5_5,
							SIR_MAC_RATE_11} } };
	enum csr_cfgdot11mode cfgDot11Mode;
	enum reg_wifi_band band;
	/* leave enough room for the max number of rates */
	uint8_t OperationalRates[CSR_DOT11_SUPPORTED_RATES_MAX];
	qdf_size_t OperationalRatesLength = 0;
	/* leave enough room for the max number of rates */
	uint8_t ExtendedOperationalRates
				[CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX];
	qdf_size_t ExtendedOperationalRatesLength = 0;
	uint32_t bss_op_ch_freq = 0;
	struct wlan_objmgr_vdev *vdev;

	if (pProfile->ChannelInfo.freq_list)
		bss_op_ch_freq = pProfile->ChannelInfo.freq_list[0];
	cfgDot11Mode = csr_roam_get_phy_mode_band_for_bss(mac, pProfile,
							  bss_op_ch_freq,
							  &band);
	/* For 11a networks, the 11a rates go into the Operational Rate set.
	 * For 11b and 11g networks, the 11b rates appear in the Operational
	 * Rate set. In either case, we can blindly put the rates we support
	 * into our Operational Rate set (including the basic rates, which we
	 * have already verified are supported earlier in the roaming decision).
	 */
	if (REG_BAND_5G == band) {
		/* 11a rates into the Operational Rate Set. */
		OperationalRatesLength =
			DefaultSupportedRates11a.supportedRateSet.numRates *
			sizeof(*DefaultSupportedRates11a.supportedRateSet.rate);
		qdf_mem_copy(OperationalRates,
			     DefaultSupportedRates11a.supportedRateSet.rate,
			     OperationalRatesLength);

		/* Nothing in the Extended rate set. */
		ExtendedOperationalRatesLength = 0;
	} else if (eCSR_CFG_DOT11_MODE_11B == cfgDot11Mode) {
		/* 11b rates into the Operational Rate Set. */
		OperationalRatesLength =
			DefaultSupportedRates11b.supportedRateSet.numRates *
			sizeof(*DefaultSupportedRates11b.supportedRateSet.rate);
		qdf_mem_copy(OperationalRates,
			     DefaultSupportedRates11b.supportedRateSet.rate,
			     OperationalRatesLength);
		/* Nothing in the Extended rate set. */
		ExtendedOperationalRatesLength = 0;
	} else {
		/* 11G */

		/* 11b rates into the Operational Rate Set. */
		OperationalRatesLength =
			DefaultSupportedRates11b.supportedRateSet.numRates *
			sizeof(*DefaultSupportedRates11b.supportedRateSet.rate);
		qdf_mem_copy(OperationalRates,
			     DefaultSupportedRates11b.supportedRateSet.rate,
			     OperationalRatesLength);

		/* 11a rates go in the Extended rate set. */
		ExtendedOperationalRatesLength =
			DefaultSupportedRates11a.supportedRateSet.numRates *
			sizeof(*DefaultSupportedRates11a.supportedRateSet.rate);
		qdf_mem_copy(ExtendedOperationalRates,
			     DefaultSupportedRates11a.supportedRateSet.rate,
			     ExtendedOperationalRatesLength);
	}

	/* Set the operational rate set CFG variables... */
	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(mac->pdev,
						    session_id,
						    WLAN_LEGACY_SME_ID);
	if (vdev) {
		mlme_set_opr_rate(vdev, OperationalRates,
				  OperationalRatesLength);
		mlme_set_ext_opr_rate(vdev, ExtendedOperationalRates,
				      ExtendedOperationalRatesLength);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	} else {
		sme_err("null vdev");
	}
}

static void csr_roam_ccm_cfg_set_callback(struct mac_context *mac,
					  uint8_t session_id)
{
	tListElem *pEntry =
		csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	uint32_t sessionId;
	tSmeCmd *pCommand = NULL;

	if (!pEntry) {
		sme_err("CFG_CNF with active list empty");
		return;
	}
	pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
	sessionId = pCommand->vdev_id;

	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(mac->psoc, sessionId))
		sme_debug("LFR3: Set ccm vdev_id:%d", session_id);

	if (CSR_IS_ROAM_JOINING(mac, sessionId)
	    && CSR_IS_ROAM_SUBSTATE_CONFIG(mac, sessionId)) {
		csr_roaming_state_config_cnf_processor(mac, pCommand,
						       session_id);
	}
}

/* pIes may be NULL */
QDF_STATUS csr_roam_set_bss_config_cfg(struct mac_context *mac, uint32_t sessionId,
				       struct csr_roam_profile *pProfile,
				       struct bss_description *bss_desc,
				       struct bss_config_param *pBssConfig,
				       struct sDot11fBeaconIEs *pIes,
				       bool resetCountry)
{
	uint32_t cfgCb = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	uint32_t chan_freq = 0;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	/* Make sure we have the domain info for the BSS we try to connect to.
	 * Do we need to worry about sequence for OSs that are not Windows??
	 */
	if (bss_desc) {
		if ((wlan_reg_11d_enabled_on_host(mac->psoc)) && pIes) {
			if (!pIes->Country.present)
				csr_apply_channel_power_info_wrapper(mac);
		}
	}
	/* Qos */
	csr_set_qos_to_cfg(mac, sessionId, pBssConfig->qosType);
	/* CB */
	if (CSR_IS_INFRA_AP(pProfile))
		chan_freq = pProfile->op_freq;
	else if (bss_desc)
		chan_freq = bss_desc->chan_freq;
	if (0 != chan_freq) {
		/* for now if we are on 2.4 Ghz, CB will be always disabled */
		if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq))
			cfgCb = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
		else
			cfgCb = pBssConfig->cbMode;
	}
	/* Rate */
	/* Fixed Rate */
	if (bss_desc)
		csr_set_cfg_rate_set(mac, (eCsrPhyMode) pProfile->phyMode,
				     pProfile, bss_desc, pIes, sessionId);
	else
		csr_set_cfg_rate_set_from_profile(mac, pProfile, sessionId);

	/* Any roaming related changes should be above this line */
	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(mac->psoc, sessionId)) {
		sme_debug("LFR3: Roam synch is in progress Session_id: %d",
			  sessionId);
		return QDF_STATUS_SUCCESS;
	}
	/* Make this the last CFG to set. The callback will trigger a
	 * join_req Join time out
	 */
	csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_CONFIG, sessionId);

	csr_roam_ccm_cfg_set_callback(mac, sessionId);

	return QDF_STATUS_SUCCESS;
}

#ifndef FEATURE_CM_ENABLE
/**
 * csr_check_for_hidden_ssid_match() - Check if the current connected SSID
 * is hidden ssid and if it matches with the roamed AP ssid.
 * @mac: Global mac context pointer
 * @session: csr session pointer
 * @roamed_bss_desc: pointer to bss descriptor of roamed bss
 * @roamed_bss_ies: Roamed AP beacon/probe IEs pointer
 *
 * Return: True if the SSID is hidden and matches with roamed SSID else false
 */
static bool
csr_check_for_hidden_ssid_match(struct mac_context *mac,
				struct csr_roam_session *session,
				struct bss_description *roamed_bss_desc,
				tDot11fBeaconIEs *roamed_bss_ies)
{
	QDF_STATUS status;
	bool is_null_ssid_match = false;
	tDot11fBeaconIEs *connected_profile_ies = NULL;
	struct wlan_ssid ssid;

	if (!session || !roamed_bss_ies)
		return false;

	status = csr_get_parsed_bss_description_ies(mac,
						    session->pConnectBssDesc,
						    &connected_profile_ies);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Unable to get IES");
		goto error;
	}

	if (!csr_is_nullssid(connected_profile_ies->SSID.ssid,
			     connected_profile_ies->SSID.num_ssid))
		goto error;

	/*
	 * After roam synch indication is received, the driver compares
	 * the SSID of the current AP and SSID of the roamed AP. If
	 * there is a mismatch, driver issues disassociate to current
	 * connected AP. This causes data path queues to be stopped and
	 * M2 to the roamed AP from userspace will fail if EAPOL is
	 * offloaded to userspace. The SSID of the current AP is
	 * parsed from the beacon IEs stored in the connected bss
	 * description. In hidden ssid case the SSID IE has 0 length
	 * and the host receives unicast probe with SSID of the
	 * AP in the roam synch indication. So SSID mismatch happens
	 * and validation fails. So fetch if the connected bss
	 * description has hidden ssid, fill the ssid from the
	 * csr_session connected_profile structure which will
	 * have the SSID.
	 */
	if (!roamed_bss_ies->SSID.present)
		goto error;
	wlan_mlme_get_ssid_vdev_id(mac->pdev, session->vdev_id,
				   ssid.ssid, &ssid.length);

	if (roamed_bss_ies->SSID.num_ssid != ssid.length)
		goto error;

	is_null_ssid_match = !qdf_mem_cmp(ssid.ssid, roamed_bss_ies->SSID.ssid,
					  roamed_bss_ies->SSID.num_ssid);
error:
	if (connected_profile_ies)
		qdf_mem_free(connected_profile_ies);

	return is_null_ssid_match;
}

/**
 * csr_check_for_allowed_ssid() - Function to check if the roamed
 * SSID is present in the configured Allowed SSID list
 * @mac: Pointer to global mac_ctx
 * @bss_desc: bss descriptor pointer
 * @roamed_bss_ies: Pointer to roamed BSS IEs
 *
 * Return: True if SSID match found else False
 */
static bool
csr_check_for_allowed_ssid(struct mac_context *mac,
			   struct bss_description *bss_desc,
			   tDot11fBeaconIEs *roamed_bss_ies)
{
	uint8_t i;
	struct wlan_ssid *ssid_list;
	uint8_t num_ssid_allowed_list;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct rso_config_params *rso_usr_cfg;

	mlme_obj = mlme_get_psoc_ext_obj(mac->psoc);
	if (!mlme_obj)
		return false;

	rso_usr_cfg = &mlme_obj->cfg.lfr.rso_user_config;

	if (!roamed_bss_ies) {
		sme_debug(" Roamed BSS IEs NULL");
		return false;
	}
	num_ssid_allowed_list = rso_usr_cfg->num_ssid_allowed_list;
	ssid_list =  rso_usr_cfg->ssid_allowed_list;

	for (i = 0; i < num_ssid_allowed_list; i++) {
		if (ssid_list[i].length == roamed_bss_ies->SSID.num_ssid &&
		    !qdf_mem_cmp(ssid_list[i].ssid, roamed_bss_ies->SSID.ssid,
				 ssid_list[i].length))
			return true;
	}

	return false;
}

static
QDF_STATUS csr_roam_stop_network(struct mac_context *mac, uint32_t sessionId,
				 struct csr_roam_profile *roam_profile,
				 struct bss_description *bss_desc,
				 tDot11fBeaconIEs *pIes)
{
	QDF_STATUS status;
	struct bss_config_param *pBssConfig;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	bool ssid_match;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	pBssConfig = qdf_mem_malloc(sizeof(struct bss_config_param));
	if (!pBssConfig)
		return QDF_STATUS_E_NOMEM;

	sme_debug("session id: %d", sessionId);

	status = csr_roam_prepare_bss_config(mac, roam_profile, bss_desc,
					     pBssConfig, pIes);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		enum csr_roam_substate substate;

		substate = eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING;
		pSession->bssParams.uCfgDot11Mode = pBssConfig->uCfgDot11Mode;
		/* This will allow to pass cbMode during join req */
		pSession->bssParams.cbMode = pBssConfig->cbMode;

		if (CSR_IS_INFRA_AP(roam_profile))
			csr_roam_prepare_bss_params(mac, sessionId,
						    roam_profile, bss_desc,
						    pBssConfig, pIes);
		if (csr_is_conn_state_infra(mac, sessionId)) {
			/*
			 * we are roaming from
			 * Infra to Infra across SSIDs
			 * (roaming to a new SSID)...
			 * Not worry about WDS connection for now
			 */
			ssid_match =
			    (csr_check_for_allowed_ssid(mac, bss_desc, pIes) ||
			     csr_check_for_hidden_ssid_match(mac, pSession,
							     bss_desc, pIes));
			if (!ssid_match)
				ssid_match = csr_is_ssid_equal(
						mac, pSession->pConnectBssDesc,
						bss_desc, pIes);
			if (bss_desc && !ssid_match)
				status = csr_roam_issue_disassociate(mac,
						sessionId, substate, false);
			else if (bss_desc)
				/*
				 * In an infra & going to an infra network with
				 * the same SSID.  This calls for a reassoc seq.
				 * So issue the CFG sets for this new AP. Set
				 * parameters for this Bss.
				 */
				status = csr_roam_set_bss_config_cfg(
						mac, sessionId, roam_profile,
						bss_desc, pBssConfig, pIes,
						false);
		} else if (bss_desc || CSR_IS_INFRA_AP(roam_profile)) {
			/*
			 * Not in Infra. We can go ahead and set
			 * the cfg for tne new network... nothing to stop.
			 */
			bool is_11r_roaming = false;

			is_11r_roaming = csr_roam_is11r_assoc(mac, sessionId);
			/* Set parameters for this Bss. */
			status = csr_roam_set_bss_config_cfg(mac, sessionId,
							     roam_profile,
							     bss_desc,
							     pBssConfig, pIes,
							     is_11r_roaming);
		}
	} /* Success getting BSS config info */
	qdf_mem_free(pBssConfig);
	return status;
}

/**
 * csr_roam_state_for_same_profile() - Determine roam state for same profile
 * @mac_ctx: pointer to mac context
 * @profile: Roam profile
 * @session: Roam session
 * @session_id: session id
 * @ies_local: local ies
 * @bss_descr: bss description
 *
 * This function will determine the roam state for same profile
 *
 * Return: Roaming state.
 */
static enum csr_join_state csr_roam_state_for_same_profile(
	struct mac_context *mac_ctx, struct csr_roam_profile *profile,
			struct csr_roam_session *session,
			uint32_t session_id, tDot11fBeaconIEs *ies_local,
			struct bss_description *bss_descr)
{
	QDF_STATUS status;
	struct bss_config_param bssConfig;

	if (csr_roam_is_same_profile_keys(mac_ctx, &session->connectedProfile,
				profile))
		return eCsrReassocToSelfNoCapChange;
	/* The key changes */
	qdf_mem_zero(&bssConfig, sizeof(bssConfig));
	status = csr_roam_prepare_bss_config(mac_ctx, profile, bss_descr,
				&bssConfig, ies_local);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		session->bssParams.uCfgDot11Mode =
				bssConfig.uCfgDot11Mode;
		session->bssParams.cbMode =
				bssConfig.cbMode;
		/* reapply the cfg including keys so reassoc happens. */
		status = csr_roam_set_bss_config_cfg(mac_ctx, session_id,
				profile, bss_descr, &bssConfig,
				ies_local, false);
		if (QDF_IS_STATUS_SUCCESS(status))
			return eCsrContinueRoaming;
	}

	return eCsrStopRoaming;

}

static enum csr_join_state csr_roam_join(struct mac_context *mac,
	uint32_t sessionId, tCsrScanResultInfo *pScanResult,
				   struct csr_roam_profile *pProfile)
{
	enum csr_join_state eRoamState = eCsrContinueRoaming;
	struct bss_description *bss_desc = &pScanResult->BssDescriptor;
	tDot11fBeaconIEs *pIesLocal = (tDot11fBeaconIEs *) (pScanResult->pvIes);
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return eCsrStopRoaming;
	}

	if (!pIesLocal &&
		!QDF_IS_STATUS_SUCCESS(csr_get_parsed_bss_description_ies(mac,
				bss_desc, &pIesLocal))) {
		sme_err("fail to parse IEs");
		return eCsrStopRoaming;
	}
	if (csr_is_infra_bss_desc(bss_desc)) {
		/*
		 * If we are connected in infra mode and the join bss descr is
		 * for the same BssID, then we are attempting to join the AP we
		 * are already connected with.  In that case, see if the Bss or
		 * sta capabilities have changed and handle the changes
		 * without disturbing the current association
		 */

		if (csr_is_conn_state_connected_infra(mac, sessionId) &&
			csr_is_bss_id_equal(bss_desc,
					    pSession->pConnectBssDesc) &&
			csr_is_ssid_equal(mac, pSession->pConnectBssDesc,
				bss_desc, pIesLocal)) {
			/*
			 * Check to see if the Auth type has changed in the
			 * profile.  If so, we don't want to reassociate with
			 * authenticating first.  To force this, stop the
			 * current association (Disassociate) and then re 'Join'
			 * the AP, wihch will force an Authentication (with the
			 * new Auth type) followed by a new Association.
			 */
			if (csr_is_same_profile(mac,
				&pSession->connectedProfile, pProfile,
				sessionId)) {
				sme_warn("detect same profile");
				eRoamState =
					csr_roam_state_for_same_profile(mac,
						pProfile, pSession, sessionId,
						pIesLocal, bss_desc);
			} else if (!QDF_IS_STATUS_SUCCESS(
						csr_roam_issue_disassociate(
						mac,
						sessionId,
						eCSR_ROAM_SUBSTATE_DISASSOC_REQ,
						false))) {
				sme_err("fail disassoc session %d",
						sessionId);
				eRoamState = eCsrStopRoaming;
			}
		} else if (!QDF_IS_STATUS_SUCCESS(csr_roam_stop_network(mac,
				sessionId, pProfile, bss_desc, pIesLocal)))
			/* we used to pre-auth here with open auth
			 * networks but that wasn't working so well.
			 * stop the existing network before attempting
			 * to join the new network.
			 */
			eRoamState = eCsrStopRoaming;
	} else if (!QDF_IS_STATUS_SUCCESS(csr_roam_stop_network(mac, sessionId,
						pProfile, bss_desc,
						pIesLocal)))
		eRoamState = eCsrStopRoaming;

	if (pIesLocal && !pScanResult->pvIes)
		qdf_mem_free(pIesLocal);
	return eRoamState;
}

static QDF_STATUS
csr_roam_should_roam(struct mac_context *mac, uint32_t sessionId,
		     struct bss_description *bss_desc, uint32_t roamId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_info *roam_info;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;
	roam_info->bss_desc = bss_desc;
	status = csr_roam_call_callback(mac, sessionId, roam_info, roamId,
					eCSR_ROAM_SHOULD_ROAM,
					eCSR_ROAM_RESULT_NONE);
	qdf_mem_free(roam_info);
	return status;
}
#endif

/* In case no matching BSS is found, use whatever default we can find */
static void csr_roam_assign_default_param(struct mac_context *mac,
					tSmeCmd *pCommand)
{
	/* Need to get all negotiated types in place first */
	/* auth type */
	/* Take the preferred Auth type. */
	switch (pCommand->u.roamCmd.roamProfile.AuthType.authType[0]) {
	default:
	case eCSR_AUTH_TYPE_WPA:
	case eCSR_AUTH_TYPE_WPA_PSK:
	case eCSR_AUTH_TYPE_WPA_NONE:
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		pCommand->u.roamCmd.roamProfile.negotiatedAuthType =
			eCSR_AUTH_TYPE_OPEN_SYSTEM;
		break;

	case eCSR_AUTH_TYPE_SHARED_KEY:
		pCommand->u.roamCmd.roamProfile.negotiatedAuthType =
			eCSR_AUTH_TYPE_SHARED_KEY;
		break;

	case eCSR_AUTH_TYPE_AUTOSWITCH:
		pCommand->u.roamCmd.roamProfile.negotiatedAuthType =
			eCSR_AUTH_TYPE_AUTOSWITCH;
		break;

	case eCSR_AUTH_TYPE_SAE:
	case eCSR_AUTH_TYPE_FT_SAE:
		pCommand->u.roamCmd.roamProfile.negotiatedAuthType =
			eCSR_AUTH_TYPE_SAE;
		break;
	}
	pCommand->u.roamCmd.roamProfile.negotiatedUCEncryptionType =
		pCommand->u.roamCmd.roamProfile.EncryptionType.
		encryptionType[0];
#ifndef FEATURE_CM_ENABLE
	/* In this case, the multicast encryption needs to follow the
	 * uncast ones.
	 */
	pCommand->u.roamCmd.roamProfile.negotiatedMCEncryptionType =
		pCommand->u.roamCmd.roamProfile.EncryptionType.
		encryptionType[0];
#endif
}

#ifndef FEATURE_CM_ENABLE
/**
 * csr_roam_select_bss() - Handle join scenario based on profile
 * @mac_ctx:             Global MAC Context
 * @roam_bss_entry:      The next BSS to join
 * @csr_result_info:     Result of join
 * @csr_scan_result:     Global scan result
 * @vdev_id:             SME Session ID
 * @roam_id:             Roaming ID
 * @roam_state:          Current roaming state
 * @bss_list:            BSS List
 *
 * Return: true if the entire BSS list is done, false otherwise.
 */
static bool csr_roam_select_bss(struct mac_context *mac_ctx,
		tListElem **roam_bss_entry, tCsrScanResultInfo **csr_result_info,
		struct tag_csrscan_result **csr_scan_result,
		uint32_t vdev_id, uint32_t roam_id,
		enum csr_join_state *roam_state,
		struct scan_result_list *bss_list)
{
	uint32_t chan_freq;
	bool status = false;
	struct tag_csrscan_result *scan_result = NULL;
	tCsrScanResultInfo *result = NULL;
	enum QDF_OPMODE op_mode;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS qdf_status;
	struct if_mgr_event_data event_data;
	struct validate_bss_data candidate_info;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(mac_ctx->pdev,
						    vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Vdev ref error");
		return true;
	}
	op_mode = wlan_vdev_mlme_get_opmode(vdev);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	while (*roam_bss_entry) {
		scan_result = GET_BASE_ADDR(*roam_bss_entry, struct
				tag_csrscan_result, Link);
		/*
		 * If concurrency enabled take the
		 * concurrent connected channel first.
		 * Valid multichannel concurrent
		 * sessions exempted
		 */
		result = &scan_result->Result;
		chan_freq = result->BssDescriptor.chan_freq;

		qdf_mem_copy(candidate_info.peer_addr.bytes,
			result->BssDescriptor.bssId,
			sizeof(tSirMacAddr));
		candidate_info.chan_freq = result->BssDescriptor.chan_freq;
		candidate_info.beacon_interval =
			result->BssDescriptor.beaconInterval;
		candidate_info.chan_freq = result->BssDescriptor.chan_freq;
		event_data.validate_bss_info = candidate_info;
		qdf_status = ucfg_if_mgr_deliver_event(vdev,
						WLAN_IF_MGR_EV_VALIDATE_CANDIDATE,
						&event_data);

		result->BssDescriptor.beaconInterval =
						candidate_info.beacon_interval;

		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			*roam_state = eCsrStopRoamingDueToConcurrency;
			status = true;
			*roam_bss_entry = csr_ll_next(&bss_list->List,
						      *roam_bss_entry,
						      LL_ACCESS_LOCK);
			continue;
		}

		if (QDF_IS_STATUS_SUCCESS(csr_roam_should_roam(mac_ctx, vdev_id,
					  &result->BssDescriptor, roam_id))) {
			status = false;
			break;
		}

		*roam_state = eCsrStopRoamingDueToConcurrency;
		status = true;
		*roam_bss_entry = csr_ll_next(&bss_list->List, *roam_bss_entry,
					LL_ACCESS_LOCK);
	}
	*csr_result_info = result;
	*csr_scan_result = scan_result;
	return status;
}
#endif

#ifdef FEATURE_CM_ENABLE
/**
 * csr_roam_join_handle_profile() - Handle join scenario based on profile
 * @mac_ctx:             Global MAC Context
 * @session_id:          SME Session ID
 * @cmd:                 Command
 * @roam_info_ptr:       Pointed to the roaming info for join
 * @roam_state:          Current roaming state
 * @result:              Result of join
 * @scan_result:         Global scan result
 *
 * Return: None
 */
static void csr_roam_join_handle_profile(struct mac_context *mac_ctx,
		uint32_t session_id, tSmeCmd *cmd,
		struct csr_roam_info *roam_info_ptr,
		enum csr_join_state *roam_state, tCsrScanResultInfo *result,
		struct tag_csrscan_result *scan_result)
{
	QDF_STATUS status;
	struct csr_roam_session *session;
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (CSR_IS_INFRA_AP(profile)) {
		/* Attempt to start this WDS... */
		csr_roam_assign_default_param(mac_ctx, cmd);
		/* For AP WDS, we dont have any BSSDescription */
		status = csr_roam_start_wds(mac_ctx, session_id, profile, NULL);
		if (QDF_IS_STATUS_SUCCESS(status))
			*roam_state = eCsrContinueRoaming;
		else
			*roam_state = eCsrStopRoaming;
	} else if (CSR_IS_NDI(profile)) {
		csr_roam_assign_default_param(mac_ctx, cmd);
		status = csr_roam_start_ndi(mac_ctx, session_id, profile);
		if (QDF_IS_STATUS_SUCCESS(status))
			*roam_state = eCsrContinueRoaming;
		else
			*roam_state = eCsrStopRoaming;
	} else {
		/* Nothing we can do */
		sme_warn("cannot continue without BSS list");
		*roam_state = eCsrStopRoaming;
		return;
	}

}
#else
static void csr_roam_join_handle_profile(struct mac_context *mac_ctx,
		uint32_t session_id, tSmeCmd *cmd,
		struct csr_roam_info *roam_info_ptr,
		enum csr_join_state *roam_state, tCsrScanResultInfo *result,
		struct tag_csrscan_result *scan_result)
{
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
	uint8_t acm_mask = 0;
#endif
	QDF_STATUS status;
	struct csr_roam_session *session;
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;
	tDot11fBeaconIEs *ies_local = NULL;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (CSR_IS_INFRASTRUCTURE(profile) && roam_info_ptr) {
		if (session->bRefAssocStartCnt) {
			session->bRefAssocStartCnt--;
			roam_info_ptr->pProfile = profile;
			/*
			 * Complete the last assoc attempt as a
			 * new one is about to be tried
			 */
			csr_roam_call_callback(mac_ctx, session_id,
				roam_info_ptr, cmd->u.roamCmd.roamId,
				eCSR_ROAM_ASSOCIATION_COMPLETION,
				eCSR_ROAM_RESULT_NOT_ASSOCIATED);
		}

		qdf_mem_zero(roam_info_ptr, sizeof(struct csr_roam_info));
		if (!scan_result)
			cmd->u.roamCmd.roamProfile.uapsd_mask = 0;
		else
			ies_local = scan_result->Result.pvIes;

		if (!result) {
			sme_err(" cannot parse IEs");
			*roam_state = eCsrStopRoaming;
			return;
		} else if (scan_result && !ies_local &&
				(!QDF_IS_STATUS_SUCCESS(
					csr_get_parsed_bss_description_ies(
						mac_ctx, &result->BssDescriptor,
						&ies_local)))) {
			sme_err(" cannot parse IEs");
			*roam_state = eCsrStopRoaming;
			return;
		}
		roam_info_ptr->bss_desc = &result->BssDescriptor;
		cmd->u.roamCmd.pLastRoamBss = roam_info_ptr->bss_desc;
		/* dont put uapsd_mask if BSS doesn't support uAPSD */
		if (scan_result && cmd->u.roamCmd.roamProfile.uapsd_mask
				&& CSR_IS_QOS_BSS(ies_local)
				&& CSR_IS_UAPSD_BSS(ies_local)) {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
			acm_mask = sme_qos_get_acm_mask(mac_ctx,
					&result->BssDescriptor, ies_local);
#endif /* WLAN_MDM_CODE_REDUCTION_OPT */
		} else {
			cmd->u.roamCmd.roamProfile.uapsd_mask = 0;
		}
		if (ies_local && !scan_result->Result.pvIes)
			qdf_mem_free(ies_local);
		roam_info_ptr->pProfile = profile;
		session->bRefAssocStartCnt++;
	}
	if (cmd->u.roamCmd.pRoamBssEntry) {
		/*
		 * We have BSS
		 * Need to assign these value because
		 * they are used in csr_is_same_profile
		 */
		scan_result = GET_BASE_ADDR(cmd->u.roamCmd.pRoamBssEntry,
				struct tag_csrscan_result, Link);
		/*
		 * The OSEN IE doesn't provide the cipher suite.Therefore set
		 * to constant value of AES
		 */
		if (cmd->u.roamCmd.roamProfile.bOSENAssociation) {
			cmd->u.roamCmd.roamProfile.negotiatedUCEncryptionType =
				eCSR_ENCRYPT_TYPE_AES;
			cmd->u.roamCmd.roamProfile.negotiatedMCEncryptionType =
				eCSR_ENCRYPT_TYPE_AES;
		} else {
			/* Negotiated while building scan result. */
			cmd->u.roamCmd.roamProfile.negotiatedUCEncryptionType =
				scan_result->ucEncryptionType;
			cmd->u.roamCmd.roamProfile.negotiatedMCEncryptionType =
				scan_result->mcEncryptionType;
		}

		cmd->u.roamCmd.roamProfile.negotiatedAuthType =
			scan_result->authType;
		if (cmd->u.roamCmd.fReassocToSelfNoCapChange) {
			/* trying to connect to the one already connected */
			cmd->u.roamCmd.fReassocToSelfNoCapChange = false;
			*roam_state = eCsrReassocToSelfNoCapChange;
			return;
		}
		/* Attempt to Join this Bss... */
		*roam_state = csr_roam_join(mac_ctx, session_id,
				&scan_result->Result, profile);
		return;
	}

	if (CSR_IS_INFRA_AP(profile)) {
		/* Attempt to start this WDS... */
		csr_roam_assign_default_param(mac_ctx, cmd);
		/* For AP WDS, we dont have any BSSDescription */
		status = csr_roam_start_wds(mac_ctx, session_id, profile, NULL);
		if (QDF_IS_STATUS_SUCCESS(status))
			*roam_state = eCsrContinueRoaming;
		else
			*roam_state = eCsrStopRoaming;
	} else if (CSR_IS_NDI(profile)) {
		csr_roam_assign_default_param(mac_ctx, cmd);
		status = csr_roam_start_ndi(mac_ctx, session_id, profile);
		if (QDF_IS_STATUS_SUCCESS(status))
			*roam_state = eCsrContinueRoaming;
		else
			*roam_state = eCsrStopRoaming;
	} else {
		/* Nothing we can do */
		sme_warn("cannot continue without BSS list");
		*roam_state = eCsrStopRoaming;
		return;
	}

}
#endif

/**
 * csr_roam_join_next_bss() - Pick the next BSS for join
 * @mac_ctx:             Global MAC Context
 * @cmd:                 Command
 * @use_same_bss:        Use Same BSS to Join
 *
 * Return: The Join State
 */
static enum csr_join_state csr_roam_join_next_bss(struct mac_context *mac_ctx,
		tSmeCmd *cmd, bool use_same_bss)
{
	struct tag_csrscan_result *scan_result = NULL;
	enum csr_join_state roam_state = eCsrStopRoaming;
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	struct scan_result_list *bss_list =
		(struct scan_result_list *) cmd->u.roamCmd.hBSSList;
	struct csr_roam_joinstatus *join_status;
	bool done = false;
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;
#endif
	struct csr_roam_info *roam_info = NULL;
	uint32_t session_id = cmd->vdev_id;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	tCsrScanResultInfo *result = NULL;

	if (!session) {
		sme_err("session %d not found", session_id);
		return eCsrStopRoaming;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return eCsrStopRoaming;

	qdf_mem_copy(&roam_info->bssid, &session->joinFailStatusCode.bssId,
			sizeof(tSirMacAddr));
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	/*
	 * When handling AP's capability change, continue to associate
	 * to same BSS and make sure pRoamBssEntry is not Null.
	 */
	if (bss_list) {
		if (!cmd->u.roamCmd.pRoamBssEntry) {
			/* Try the first BSS */
			cmd->u.roamCmd.pLastRoamBss = NULL;
			cmd->u.roamCmd.pRoamBssEntry =
				csr_ll_peek_head(&bss_list->List,
					LL_ACCESS_LOCK);
		} else if (!use_same_bss) {
			cmd->u.roamCmd.pRoamBssEntry =
				csr_ll_next(&bss_list->List,
					cmd->u.roamCmd.pRoamBssEntry,
					LL_ACCESS_LOCK);
			/*
			 * Done with all the BSSs.
			 * In this case, will tell HDD the
			 * completion
			 */
			if (!cmd->u.roamCmd.pRoamBssEntry)
				goto end;
			/*
			 * We need to indicate to HDD that we
			 * are done with this one.
			 */
			roam_info->bss_desc = cmd->u.roamCmd.pLastRoamBss;
			join_status = &session->joinFailStatusCode;
			roam_info->status_code = join_status->status_code;
			roam_info->reasonCode = join_status->reasonCode;
		} else {
			/*
			 * Try connect to same BSS again. Fill roam_info for the
			 * last attemp to indicate to HDD.
			 */
			roam_info->bss_desc = cmd->u.roamCmd.pLastRoamBss;
			join_status = &session->joinFailStatusCode;
			roam_info->status_code = join_status->status_code;
			roam_info->reasonCode = join_status->reasonCode;
		}

		done = csr_roam_select_bss(mac_ctx,
					   &cmd->u.roamCmd.pRoamBssEntry,
					   &result, &scan_result, session_id,
					   cmd->u.roamCmd.roamId,
					   &roam_state, bss_list);
		if (done)
			goto end;
	}
#endif
	roam_info->u.pConnectedProfile = &session->connectedProfile;

	csr_roam_join_handle_profile(mac_ctx, session_id, cmd, roam_info,
		&roam_state, result, scan_result);
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
end:

	if ((eCsrStopRoaming == roam_state) && CSR_IS_INFRASTRUCTURE(profile) &&
		(session->bRefAssocStartCnt > 0)) {
		/*
		 * Need to indicate association_completion if association_start
		 * has been done
		 */
		session->bRefAssocStartCnt--;
		/*
		 * Complete the last assoc attempte as a
		 * new one is about to be tried
		 */
		roam_info->pProfile = profile;
		csr_roam_call_callback(mac_ctx, session_id,
			roam_info, cmd->u.roamCmd.roamId,
			eCSR_ROAM_ASSOCIATION_COMPLETION,
			eCSR_ROAM_RESULT_NOT_ASSOCIATED);
	}
#endif
	qdf_mem_free(roam_info);

	return roam_state;
}

static QDF_STATUS csr_roam(struct mac_context *mac, tSmeCmd *pCommand,
			   bool use_same_bss)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum csr_join_state RoamState;
	enum csr_roam_substate substate;
	uint32_t sessionId = pCommand->vdev_id;

	/* Attept to join a Bss... */
	RoamState = csr_roam_join_next_bss(mac, pCommand, use_same_bss);

	/* if nothing to join.. */
#ifndef FEATURE_CM_ENABLE
	if ((eCsrStopRoaming == RoamState) ||
		(eCsrStopRoamingDueToConcurrency == RoamState))
#else
	if (RoamState == eCsrStopRoaming)
#endif
	{
		bool fComplete = false;
		/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
		/* and if connected in Infrastructure mode... */
		if (csr_is_conn_state_infra(mac, sessionId)) {
			/* ... then we need to issue a disassociation */
			substate = eCSR_ROAM_SUBSTATE_DISASSOC_NOTHING_TO_JOIN;
			status = csr_roam_issue_disassociate(mac, sessionId,
					substate, false);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_warn("fail issuing disassoc status = %d",
					status);
				/*
				 * roam command is completed by caller in the
				 * failed case
				 */
				fComplete = true;
			}
		} else
#endif
		if (csr_is_conn_state_connected_infra_ap(mac,
					sessionId)) {
			substate = eCSR_ROAM_SUBSTATE_STOP_BSS_REQ;
			status = csr_roam_issue_stop_bss(mac, sessionId,
						substate);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_warn("fail issuing stop bss status = %d",
					status);
				/*
				 * roam command is completed by caller in the
				 * failed case
				 */
				fComplete = true;
			}
		} else {
			fComplete = true;
		}

		if (fComplete) {
#ifndef FEATURE_CM_ENABLE
			/* otherwise, we can complete the Roam command here. */
			if (eCsrStopRoamingDueToConcurrency == RoamState)
				csr_roam_complete(mac,
					eCsrJoinFailureDueToConcurrency, NULL,
					sessionId);
			else
#endif
				csr_roam_complete(mac,
					eCsrNothingToJoin, NULL, sessionId);
		}
	}
#ifndef FEATURE_CM_ENABLE
	else if (eCsrReassocToSelfNoCapChange == RoamState) {
		csr_roam_complete(mac, eCsrSilentlyStopRoamingSaveState,
				NULL, sessionId);
	}
#endif
	return status;
}

#ifndef FEATURE_CM_ENABLE
static
QDF_STATUS csr_process_ft_reassoc_roam_command(struct mac_context *mac,
					       tSmeCmd *pCommand)
{
	uint32_t sessionId;
	struct csr_roam_session *pSession;
	struct tag_csrscan_result *pScanResult = NULL;
	struct bss_description *bss_desc = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	sessionId = pCommand->vdev_id;
	pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (CSR_IS_ROAMING(pSession) && pSession->fCancelRoaming) {
		/* the roaming is cancelled. Simply complete the command */
		sme_debug("Roam command canceled");
		csr_roam_complete(mac, eCsrNothingToJoin, NULL, sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	if (pCommand->u.roamCmd.pRoamBssEntry) {
		pScanResult =
			GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry,
				      struct tag_csrscan_result, Link);
		bss_desc = &pScanResult->Result.BssDescriptor;
	} else {
		/* the roaming is cancelled. Simply complete the command */
		sme_debug("Roam command canceled");
		csr_roam_complete(mac, eCsrNothingToJoin, NULL, sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	status = csr_roam_issue_reassociate(mac, sessionId, bss_desc,
					    (tDot11fBeaconIEs *) (pScanResult->
								  Result.pvIes),
					    &pCommand->u.roamCmd.roamProfile);
	return status;
}

/**
 * csr_roam_trigger_reassociate() - Helper function to trigger reassociate
 * @mac_ctx: pointer to mac context
 * @cmd: sme command
 * @session_ptr: session pointer
 * @session_id: session id
 *
 * This function will trigger reassociate.
 *
 * Return: QDF_STATUS for success or failure.
 */
static QDF_STATUS
csr_roam_trigger_reassociate(struct mac_context *mac_ctx, tSmeCmd *cmd,
			     struct csr_roam_session *session_ptr,
			     uint32_t session_id)
{
	tDot11fBeaconIEs *pIes = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_info *roam_info;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	if (session_ptr->pConnectBssDesc) {
		status = csr_get_parsed_bss_description_ies(mac_ctx,
				session_ptr->pConnectBssDesc, &pIes);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("fail to parse IEs");
		} else {
			roam_info->reasonCode =
					eCsrRoamReasonStaCapabilityChanged;
			csr_roam_call_callback(mac_ctx, session_ptr->sessionId,
					roam_info, 0, eCSR_ROAM_ROAMING_START,
					eCSR_ROAM_RESULT_NOT_ASSOCIATED);
			session_ptr->roamingReason = eCsrReassocRoaming;
			roam_info->bss_desc = session_ptr->pConnectBssDesc;
			roam_info->pProfile = &cmd->u.roamCmd.roamProfile;
			/* This is temp ifdef will be removed in near future */
			session_ptr->bRefAssocStartCnt++;
			sme_debug("calling csr_roam_issue_reassociate");
			status = csr_roam_issue_reassociate(mac_ctx, session_id,
					session_ptr->pConnectBssDesc, pIes,
					&cmd->u.roamCmd.roamProfile);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_err("failed status %d", status);
				csr_release_command(mac_ctx, cmd);
			} else {
				csr_neighbor_roam_state_transition(mac_ctx,
					eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING,
					session_id);
			}


			qdf_mem_free(pIes);
			pIes = NULL;
		}
	} else {
		sme_err("reassoc to same AP failed as connected BSS is NULL");
		status = QDF_STATUS_E_FAILURE;
	}
	qdf_mem_free(roam_info);
	return status;
}

/**
 * csr_allow_concurrent_sta_connections() - Wrapper for policy_mgr api
 * @mac: mac context
 * @vdev_id: vdev id
 *
 * This function invokes policy mgr api to check for support of
 * simultaneous connections on concurrent STA interfaces.
 *
 *  Return: If supports return true else false.
 */
static
bool csr_allow_concurrent_sta_connections(struct mac_context *mac,
					  uint32_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	enum QDF_OPMODE vdev_mode;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("vdev object not found for vdev_id %u", vdev_id);
		return false;
	}
	vdev_mode = wlan_vdev_mlme_get_opmode(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

	/* If vdev mode is STA then proceed further */
	if (vdev_mode != QDF_STA_MODE)
		return true;

	if (policy_mgr_allow_concurrency(mac->psoc, PM_STA_MODE, 0,
					 HW_MODE_20_MHZ))
		return true;

	return false;
}
#endif

static void csr_get_peer_rssi_cb(struct stats_event *ev, void *cookie)
{
	struct mac_context *mac = (struct mac_context *)cookie;

	if (!mac) {
		sme_err("Invalid mac ctx");
		return;
	}

	if (!ev->peer_stats) {
		sme_debug("no peer stats");
		goto disconnect_stats_complete;
	}

	mac->peer_rssi = ev->peer_stats->peer_rssi;
	mac->peer_txrate = ev->peer_stats->tx_rate;
	mac->peer_rxrate = ev->peer_stats->rx_rate;
	if (!ev->peer_extended_stats) {
		sme_debug("no peer extended stats");
		goto disconnect_stats_complete;
	}
	mac->rx_mc_bc_cnt = ev->peer_extended_stats->rx_mc_bc_cnt;

disconnect_stats_complete:
	csr_roam_get_disconnect_stats_complete(mac);
}

static void csr_get_peer_rssi(struct mac_context *mac, uint32_t session_id,
			      struct qdf_mac_addr peer_mac)
{
	struct wlan_objmgr_vdev *vdev;
	struct request_info info = {0};
	QDF_STATUS status;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
						mac->psoc,
						session_id,
						WLAN_LEGACY_SME_ID);
	if (vdev) {
		info.cookie = mac;
		info.u.get_peer_rssi_cb = csr_get_peer_rssi_cb;
		info.vdev_id = wlan_vdev_get_id(vdev);
		info.pdev_id = wlan_objmgr_pdev_get_pdev_id(
					wlan_vdev_get_pdev(vdev));
		qdf_mem_copy(info.peer_mac_addr, &peer_mac, QDF_MAC_ADDR_SIZE);
		status = ucfg_mc_cp_stats_send_stats_request(
						vdev,
						TYPE_PEER_STATS,
						&info);
		sme_debug("peer_mac" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(peer_mac.bytes));
		if (QDF_IS_STATUS_ERROR(status))
			sme_err("stats req failed: %d", status);

		wma_get_rx_retry_cnt(mac, session_id, info.peer_mac_addr);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	}
}

QDF_STATUS csr_roam_process_command(struct mac_context *mac, tSmeCmd *pCommand)
{
	QDF_STATUS lock_status, status = QDF_STATUS_SUCCESS;
	uint32_t sessionId = pCommand->vdev_id;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	sme_debug("Roam Reason: %d sessionId: %d",
		pCommand->u.roamCmd.roamReason, sessionId);

	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	pSession->disconnect_reason = pCommand->u.roamCmd.disconnect_reason;
#endif
	switch (pCommand->u.roamCmd.roamReason) {
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	case eCsrForcedDisassoc:
		status = csr_roam_process_disassoc_deauth(mac, pCommand,
				true, false);
		lock_status = sme_acquire_global_lock(&mac->sme);
		if (!QDF_IS_STATUS_SUCCESS(lock_status)) {
			csr_roam_complete(mac, eCsrNothingToJoin, NULL,
					  sessionId);
			return lock_status;
		}
		csr_free_roam_profile(mac, sessionId);
		sme_release_global_lock(&mac->sme);
		break;
	case eCsrSmeIssuedDisassocForHandoff:
		/* Not to free mac->roam.pCurRoamProfile (via
		 * csr_free_roam_profile) because its needed after disconnect
		 */
		status = csr_roam_process_disassoc_deauth(mac, pCommand,
				true, false);

		break;
	case eCsrForcedDisassocMICFailure:
		status = csr_roam_process_disassoc_deauth(mac, pCommand,
				true, true);
		lock_status = sme_acquire_global_lock(&mac->sme);
		if (!QDF_IS_STATUS_SUCCESS(lock_status)) {
			csr_roam_complete(mac, eCsrNothingToJoin, NULL,
					  sessionId);
			return lock_status;
		}
		csr_free_roam_profile(mac, sessionId);
		sme_release_global_lock(&mac->sme);
		break;
	case eCsrForcedDeauth:
		status = csr_roam_process_disassoc_deauth(mac, pCommand,
				false, false);
		lock_status = sme_acquire_global_lock(&mac->sme);
		if (!QDF_IS_STATUS_SUCCESS(lock_status)) {
			csr_roam_complete(mac, eCsrNothingToJoin, NULL,
					  sessionId);
			return lock_status;
		}
		csr_free_roam_profile(mac, sessionId);
		sme_release_global_lock(&mac->sme);
		break;
	case eCsrHddIssuedReassocToSameAP:
	case eCsrSmeIssuedReassocToSameAP:
		status = csr_roam_trigger_reassociate(mac, pCommand,
						      pSession, sessionId);
		break;
	case eCsrSmeIssuedFTReassoc:
		sme_debug("received FT Reassoc Req");
		status = csr_process_ft_reassoc_roam_command(mac, pCommand);
		break;
#endif
	case eCsrStopBss:
		csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING,
				sessionId);
		status = csr_roam_issue_stop_bss(mac, sessionId,
				eCSR_ROAM_SUBSTATE_STOP_BSS_REQ);
		break;

	case eCsrForcedDisassocSta:
		csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING,
				sessionId);
		csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_DISASSOC_REQ,
				sessionId);
		sme_debug("Disassociate issued with reason: %d",
			pCommand->u.roamCmd.reason);

		status = csr_send_mb_disassoc_req_msg(mac, sessionId,
				pCommand->u.roamCmd.peerMac,
				pCommand->u.roamCmd.reason);
		break;

	case eCsrForcedDeauthSta:
		csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING,
				sessionId);
		csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_DEAUTH_REQ,
				sessionId);
		sme_debug("Deauth issued with reason: %d",
			  pCommand->u.roamCmd.reason);

		status = csr_send_mb_deauth_req_msg(mac, sessionId,
				pCommand->u.roamCmd.peerMac,
				pCommand->u.roamCmd.reason);
		break;
#ifndef FEATURE_CM_ENABLE
	case eCsrPerformPreauth:
		sme_debug("Attempting FT PreAuth Req");
		status = csr_roam_issue_ft_preauth_req(mac, sessionId,
				pCommand->u.roamCmd.pLastRoamBss);
		break;
#endif
	case eCsrHddIssued:
		/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE

		/*
		 * Check for simultaneous connection support on
		 * multiple STA interfaces.
		 */
		if (!csr_allow_concurrent_sta_connections(mac, sessionId)) {
			sme_err("No support of conc STA con");
			csr_roam_complete(mac, eCsrNothingToJoin, NULL,
					  sessionId);
			status = QDF_STATUS_E_FAILURE;
			break;
		}
#endif
		/* for success case */
		/* fallthrough */
	default:
		csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING,
				sessionId);

		if (pCommand->u.roamCmd.fUpdateCurRoamProfile) {
			/* Remember the roaming profile */
			lock_status = sme_acquire_global_lock(&mac->sme);
			if (!QDF_IS_STATUS_SUCCESS(lock_status)) {
				csr_roam_complete(mac, eCsrNothingToJoin, NULL,
						  sessionId);
				return lock_status;
			}
			csr_free_roam_profile(mac, sessionId);
			pSession->pCurRoamProfile =
				qdf_mem_malloc(sizeof(struct csr_roam_profile));
			if (pSession->pCurRoamProfile) {
				csr_roam_copy_profile(mac,
					pSession->pCurRoamProfile,
					&pCommand->u.roamCmd.roamProfile,
					sessionId);
			}
			sme_release_global_lock(&mac->sme);
		}

		/*
		 * At this point original uapsd_mask is saved in
		 * pCurRoamProfile. uapsd_mask in the pCommand may change from
		 * this point on. Attempt to roam with the new scan results
		 * (if we need to..)
		 */
		status = csr_roam(mac, pCommand, false);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_warn("csr_roam() failed with status = 0x%08X",
				status);
		break;
	}
	return status;
}

void csr_reinit_roam_cmd(struct mac_context *mac, tSmeCmd *pCommand)
{
	if (pCommand->u.roamCmd.fReleaseBssList) {
		csr_scan_result_purge(mac, pCommand->u.roamCmd.hBSSList);
		pCommand->u.roamCmd.fReleaseBssList = false;
		pCommand->u.roamCmd.hBSSList = CSR_INVALID_SCANRESULT_HANDLE;
	}
	if (pCommand->u.roamCmd.fReleaseProfile) {
		csr_release_profile(mac, &pCommand->u.roamCmd.roamProfile);
		pCommand->u.roamCmd.fReleaseProfile = false;
	}
	pCommand->u.roamCmd.pLastRoamBss = NULL;
	pCommand->u.roamCmd.pRoamBssEntry = NULL;
	/* Because u.roamCmd is union and share with scanCmd and StatusChange */
	qdf_mem_zero(&pCommand->u.roamCmd, sizeof(struct roam_cmd));
}

void csr_reinit_wm_status_change_cmd(struct mac_context *mac,
			tSmeCmd *pCommand)
{
	qdf_mem_zero(&pCommand->u.wmStatusChangeCmd,
			sizeof(struct wmstatus_changecmd));
}

void csr_roam_complete(struct mac_context *mac_ctx,
		       enum csr_roamcomplete_result Result,
		       void *Context, uint8_t session_id)
{
	tSmeCmd *sme_cmd;
	struct wlan_serialization_command *cmd;

	cmd = wlan_serialization_peek_head_active_cmd_using_psoc(
				mac_ctx->psoc, false);
	if (!cmd) {
		sme_err("Roam completion called but cmd is not active");
		return;
	}
	sme_cmd = cmd->umac_cmd;
	if (!sme_cmd) {
		sme_err("sme_cmd is NULL");
		return;
	}
	if (eSmeCommandRoam == sme_cmd->command) {
		csr_roam_process_results(mac_ctx, sme_cmd, Result, Context);
		csr_release_command(mac_ctx, sme_cmd);
	}
}

/* Returns whether the current association is a 11r assoc or not */
bool csr_roam_is11r_assoc(struct mac_context *mac, uint8_t sessionId)
{
	struct cm_roam_values_copy config;

	wlan_cm_roam_cfg_get_value(mac->psoc, sessionId, IS_11R_CONNECTION,
				   &config);

	return config.bool_value;
}

bool csr_roam_is_fast_roam_enabled(struct mac_context *mac, uint8_t vdev_id)
{
	if (wlan_get_opmode_from_vdev_id(mac->pdev, vdev_id) != QDF_STA_MODE)
		return false;

	if (true == CSR_IS_FASTROAM_IN_CONCURRENCY_INI_FEATURE_ENABLED(mac))
		return mac->mlme_cfg->lfr.lfr_enabled;
	else
		return mac->mlme_cfg->lfr.lfr_enabled &&
			(!csr_is_concurrent_session_running(mac));
}

#ifndef FEATURE_CM_ENABLE
static
void csr_update_scan_entry_associnfo(struct mac_context *mac_ctx,
				     uint8_t vdev_id,
				     enum scan_entry_connection_state state)
{
	QDF_STATUS status;
	struct mlme_info mlme;
	struct bss_info bss;
	enum QDF_OPMODE opmode = wlan_get_opmode_from_vdev_id(mac_ctx->pdev,
							      vdev_id);

	if (!CSR_IS_SESSION_VALID(mac_ctx, vdev_id)) {
		sme_debug("vdev_id %d is invalid", vdev_id);
		return;
	}

	if (opmode != QDF_STA_MODE && opmode != QDF_P2P_CLIENT_MODE) {
		sme_debug("not infra return");
		return;
	}

	wlan_mlme_get_bssid_vdev_id(mac_ctx->pdev, vdev_id, &bss.bssid);
	bss.freq = wlan_get_operation_chan_freq_vdev_id(mac_ctx->pdev,
							vdev_id);
	wlan_mlme_get_ssid_vdev_id(mac_ctx->pdev, vdev_id,
				   bss.ssid.ssid, &bss.ssid.length);

	sme_debug("Update MLME info in scan database. bssid "QDF_MAC_ADDR_FMT" ssid:%.*s freq %d state: %d",
		  QDF_MAC_ADDR_REF(bss.bssid.bytes), bss.ssid.length,
		  bss.ssid.ssid, bss.freq, state);
	mlme.assoc_state = state;
	status = ucfg_scan_update_mlme_by_bssinfo(mac_ctx->pdev, &bss, &mlme);
	if (QDF_IS_STATUS_ERROR(status))
		sme_debug("Failed to update the MLME info in scan entry");
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void csr_roam_synch_clean_up(struct mac_context *mac, uint8_t session_id)
{
	struct scheduler_msg msg = {0};
	struct roam_offload_synch_fail *roam_offload_failed = NULL;

	/* Clean up the roam synch in progress for LFR3 */
	sme_err("Roam Synch Failed, Clean Up");

	roam_offload_failed = qdf_mem_malloc(
				sizeof(struct roam_offload_synch_fail));
	if (!roam_offload_failed)
		return;

	roam_offload_failed->session_id = session_id;
	msg.type     = WMA_ROAM_OFFLOAD_SYNCH_FAIL;
	msg.reserved = 0;
	msg.bodyptr  = roam_offload_failed;
	if (!QDF_IS_STATUS_SUCCESS(scheduler_post_message(QDF_MODULE_ID_SME,
							  QDF_MODULE_ID_WMA,
							  QDF_MODULE_ID_WMA,
							  &msg))) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"%s: Unable to post WMA_ROAM_OFFLOAD_SYNCH_FAIL to WMA",
			__func__);
		qdf_mem_free(roam_offload_failed);
	}
}
#endif

#if defined(WLAN_FEATURE_FILS_SK)
/**
 * csr_update_fils_seq_number() - Copy FILS sequence number to roam info
 * @session: CSR Roam Session
 * @roam_info: Roam info
 *
 * Return: None
 */
static void csr_update_fils_seq_number(struct csr_roam_session *session,
					 struct csr_roam_info *roam_info)
{
	roam_info->is_fils_connection = true;
	roam_info->fils_seq_num = session->fils_seq_num;
	pe_debug("FILS sequence number %x", session->fils_seq_num);
}
#else
static inline void csr_update_fils_seq_number(struct csr_roam_session *session,
						struct csr_roam_info *roam_info)
{}
#endif
#endif

/**
 * csr_roam_process_results_default() - Process the result for start bss
 * @mac_ctx:          Global MAC Context
 * @cmd:              Command to be processed
 * @context:          Additional data in context of the cmd
 *
 * Return: None
 */
static void csr_roam_process_results_default(struct mac_context *mac_ctx,
		     tSmeCmd *cmd, void *context, enum csr_roamcomplete_result
			res)
{
	uint32_t session_id = cmd->vdev_id;
	struct csr_roam_session *session;
	struct csr_roam_info *roam_info;
	QDF_STATUS status;
#ifndef FEATURE_CM_ENABLE
	struct csr_roam_connectedinfo *prev_connect_info = NULL;
#endif
	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	session = CSR_GET_SESSION(mac_ctx, session_id);

#ifndef FEATURE_CM_ENABLE
	prev_connect_info = &session->prev_assoc_ap_info;
#endif
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	sme_debug("Assoc ref count: %d", session->bRefAssocStartCnt);
#endif
	if (CSR_IS_ROAM_SUBSTATE_STOP_BSS_REQ(mac_ctx, session_id)
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	    || CSR_IS_INFRASTRUCTURE(&session->connectedProfile)
#endif
	) {
		/*
		 * do not free for the other profiles as we need
		 * to send down stop BSS later
		 */
		/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
		csr_free_connect_bss_desc(mac_ctx, session_id);
#endif
		csr_roam_free_connect_profile(&session->connectedProfile);
		csr_roam_free_connected_info(mac_ctx, &session->connectedInfo);
		csr_set_default_dot11_mode(mac_ctx);
	}

	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	/* Copy FILS sequence number used to be updated to userspace */
	if (session->is_fils_connection)
		csr_update_fils_seq_number(session, roam_info);
#endif
	switch (cmd->u.roamCmd.roamReason) {
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	/*
	 * If this transition is because of an 802.11 OID, then we
	 * transition back to INIT state so we sit waiting for more
	 * OIDs to be issued and we don't start the IDLE timer.
	 */
	case eCsrSmeIssuedFTReassoc:
	case eCsrSmeIssuedAssocToSimilarAP:
	case eCsrHddIssued:
	case eCsrSmeIssuedDisassocForHandoff:
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
			session_id);
		roam_info->bss_desc = cmd->u.roamCmd.pLastRoamBss;
		roam_info->pProfile = &cmd->u.roamCmd.roamProfile;
		roam_info->status_code =
			session->joinFailStatusCode.status_code;
		roam_info->reasonCode = session->joinFailStatusCode.reasonCode;
		qdf_mem_copy(&roam_info->bssid,
			     &session->joinFailStatusCode.bssId,
			     sizeof(struct qdf_mac_addr));
		if (prev_connect_info->pbFrames) {
			roam_info->nAssocReqLength =
					prev_connect_info->nAssocReqLength;
			roam_info->nAssocRspLength =
					prev_connect_info->nAssocRspLength;
			roam_info->nBeaconLength =
				prev_connect_info->nBeaconLength;
			roam_info->pbFrames = prev_connect_info->pbFrames;
		}
		/*
		 * If Join fails while Handoff is in progress, indicate
		 * disassociated event to supplicant to reconnect
		 */
		if (csr_roam_is_handoff_in_progress(mac_ctx, session_id)) {
			csr_neighbor_roam_indicate_connect(mac_ctx,
				(uint8_t)session_id, QDF_STATUS_E_FAILURE);
		}
		if (session->bRefAssocStartCnt > 0) {
			session->bRefAssocStartCnt--;
			if (eCsrJoinFailureDueToConcurrency == res)
				csr_roam_call_callback(mac_ctx, session_id,
						       roam_info,
						       cmd->u.roamCmd.roamId,
				       eCSR_ROAM_ASSOCIATION_COMPLETION,
				       eCSR_ROAM_RESULT_ASSOC_FAIL_CON_CHANNEL);
			else
				csr_roam_call_callback(mac_ctx, session_id,
						       roam_info,
						       cmd->u.roamCmd.roamId,
					       eCSR_ROAM_ASSOCIATION_COMPLETION,
					       eCSR_ROAM_RESULT_FAILURE);
		} else {
			/*
			 * bRefAssocStartCnt is not incremented when
			 * eRoamState == eCsrStopRoamingDueToConcurrency
			 * in csr_roam_join_next_bss API. so handle this in
			 * else case by sending assoc failure
			 */
			csr_roam_call_callback(mac_ctx, session_id,
					       roam_info,
					       cmd->u.roamCmd.roamId,
					       eCSR_ROAM_ASSOCIATION_FAILURE,
					       eCSR_ROAM_RESULT_FAILURE);
		}
		sme_debug("roam(reason %d) failed", cmd->u.roamCmd.roamReason);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_update_hand_off((uint8_t) session_id, false);
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
			SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
		csr_roam_completion(mac_ctx, session_id, NULL, cmd,
			eCSR_ROAM_RESULT_FAILURE, false);
		break;
	case eCsrHddIssuedReassocToSameAP:
	case eCsrSmeIssuedReassocToSameAP:
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
			session_id);

		csr_roam_call_callback(mac_ctx, session_id, NULL,
			cmd->u.roamCmd.roamId, eCSR_ROAM_DISASSOCIATED,
			eCSR_ROAM_RESULT_FORCED);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
			SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
		csr_roam_completion(mac_ctx, session_id, NULL, cmd,
			eCSR_ROAM_RESULT_FAILURE, false);
		break;
	case eCsrForcedDisassoc:
	case eCsrForcedDeauth:
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
			session_id);
		csr_roam_call_callback(
			mac_ctx, session_id, NULL,
			cmd->u.roamCmd.roamId, eCSR_ROAM_DISASSOCIATED,
			eCSR_ROAM_RESULT_FORCED);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
				SME_QOS_CSR_DISCONNECT_IND,
				NULL);
#endif
		csr_roam_link_down(mac_ctx, session_id);

		if (mac_ctx->roam.deauthRspStatus == eSIR_SME_DEAUTH_STATUS) {
			sme_warn("FW still in connected state");
			break;
		}
		break;
	case eCsrForcedDisassocMICFailure:
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
			session_id);

		csr_roam_call_callback(mac_ctx, session_id, NULL,
			cmd->u.roamCmd.roamId, eCSR_ROAM_DISASSOCIATED,
			eCSR_ROAM_RESULT_MIC_FAILURE);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
			SME_QOS_CSR_DISCONNECT_REQ, NULL);
#endif
		break;
#endif /* ndef FEATURE_CM_ENABLE */
	case eCsrStopBss:
		csr_roam_call_callback(mac_ctx, session_id, NULL,
			cmd->u.roamCmd.roamId, eCSR_ROAM_INFRA_IND,
			eCSR_ROAM_RESULT_INFRA_STOPPED);
		break;
	case eCsrForcedDisassocSta:
	case eCsrForcedDeauthSta:
		roam_info->rssi = mac_ctx->peer_rssi;
		roam_info->tx_rate = mac_ctx->peer_txrate;
		roam_info->rx_rate = mac_ctx->peer_rxrate;
		roam_info->rx_mc_bc_cnt = mac_ctx->rx_mc_bc_cnt;
		roam_info->rx_retry_cnt = mac_ctx->rx_retry_cnt;

		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_JOINED,
			session_id);
		session = CSR_GET_SESSION(mac_ctx, session_id);
		if (CSR_IS_SESSION_VALID(mac_ctx, session_id) &&
			CSR_IS_INFRA_AP(&session->connectedProfile)) {
			roam_info->u.pConnectedProfile =
				&session->connectedProfile;
			qdf_mem_copy(roam_info->peerMac.bytes,
				     cmd->u.roamCmd.peerMac,
				     sizeof(tSirMacAddr));
			roam_info->reasonCode = eCSR_ROAM_RESULT_FORCED;
			/* Update the MAC reason code */
			roam_info->disassoc_reason = cmd->u.roamCmd.reason;
			roam_info->status_code = eSIR_SME_SUCCESS;
			status = csr_roam_call_callback(mac_ctx, session_id,
							roam_info,
							cmd->u.roamCmd.roamId,
							eCSR_ROAM_LOSTLINK,
						       eCSR_ROAM_RESULT_FORCED);
		}
		break;
	default:
		csr_roam_state_change(mac_ctx,
			eCSR_ROAMING_STATE_IDLE, session_id);
		break;
	}
	qdf_mem_free(roam_info);
}

/**
 * csr_roam_process_start_bss_success() - Process the result for start bss
 * @mac_ctx:          Global MAC Context
 * @cmd:              Command to be processed
 * @context:          Additional data in context of the cmd
 *
 * Return: None
 */
static void csr_roam_process_start_bss_success(struct mac_context *mac_ctx,
		     tSmeCmd *cmd, void *context)
{
	uint32_t session_id = cmd->vdev_id;
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;
	struct csr_roam_session *session;
	struct bss_description *bss_desc = NULL;
	struct csr_roam_info *roam_info;
	struct start_bss_rsp *start_bss_rsp = NULL;
	eRoamCmdStatus roam_status = eCSR_ROAM_INFRA_IND;
	eCsrRoamResult roam_result = eCSR_ROAM_RESULT_INFRA_STARTED;
	tSirMacAddr bcast_mac = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	QDF_STATUS status;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);

	sme_debug("receives start BSS ok indication");
	status = QDF_STATUS_E_FAILURE;
	start_bss_rsp = (struct start_bss_rsp *) context;
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	if (CSR_IS_INFRA_AP(profile))
		session->connectState =
			eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED;
	else if (CSR_IS_NDI(profile))
		session->connectState = eCSR_CONNECT_STATE_TYPE_NDI_STARTED;
	else
		session->connectState = eCSR_ASSOC_STATE_TYPE_WDS_DISCONNECTED;

	bss_desc = &start_bss_rsp->bssDescription;
	if (CSR_IS_NDI(profile)) {
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_JOINED,
			session_id);
		csr_roam_save_ndi_connected_info(mac_ctx, session_id, profile,
						bss_desc);
		roam_info->u.pConnectedProfile = &session->connectedProfile;
		qdf_mem_copy(&roam_info->bssid, &bss_desc->bssId,
			     sizeof(struct qdf_mac_addr));
	} else {
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_JOINED,
				session_id);
	}

	csr_roam_free_connect_profile(&session->connectedProfile);
	csr_roam_free_connected_info(mac_ctx, &session->connectedInfo);
	csr_roam_save_connected_information(mac_ctx, session_id,
			profile, bss_desc, NULL);
	qdf_mem_copy(&roam_info->bssid, &bss_desc->bssId,
		     sizeof(struct qdf_mac_addr));
	/* We are done with the IEs so free it */
	/*
	 * Only set context for non-WDS_STA. We don't even need it for
	 * WDS_AP. But since the encryption.
	 * is WPA2-PSK so it won't matter.
	 */
	if (session->pCurRoamProfile &&
	    !CSR_IS_INFRA_AP(session->pCurRoamProfile)) {
		if (CSR_IS_ENC_TYPE_STATIC(
				profile->negotiatedUCEncryptionType)) {
			/* NO keys. these key parameters don't matter */
			csr_issue_set_context_req_helper(mac_ctx,
						profile, session_id,
						&bcast_mac, false,
						false, eSIR_TX_RX,
						0, 0, NULL);
		}
	}

	/*
	 * Only tell upper layer is we start the BSS because Vista doesn't like
	 * multiple connection indications. If we don't start the BSS ourself,
	 * handler of eSIR_SME_JOINED_NEW_BSS will trigger the connection start
	 * indication in Vista
	 */
	roam_info->staId = (uint8_t)start_bss_rsp->staId;
	if (CSR_IS_NDI(profile)) {
		csr_roam_update_ndp_return_params(mac_ctx,
						  eCsrStartBssSuccess,
						  &roam_status,
						  &roam_result,
						  roam_info);
	}
	/*
	 * Only tell upper layer is we start the BSS because Vista
	 * doesn't like multiple connection indications. If we don't
	 * start the BSS ourself, handler of eSIR_SME_JOINED_NEW_BSS
	 * will trigger the connection start indication in Vista
	 */
	roam_info->status_code =
			session->joinFailStatusCode.status_code;
	roam_info->reasonCode = session->joinFailStatusCode.reasonCode;
	roam_info->bss_desc = bss_desc;
	if (bss_desc)
		qdf_mem_copy(roam_info->bssid.bytes, bss_desc->bssId,
			     sizeof(struct qdf_mac_addr));

	csr_roam_call_callback(mac_ctx, session_id, roam_info,
			       cmd->u.roamCmd.roamId,
			       roam_status, roam_result);
	qdf_mem_free(roam_info);
}

#ifndef FEATURE_CM_ENABLE
#ifdef WLAN_FEATURE_FILS_SK
/**
 * populate_fils_params_join_rsp() - Copy FILS params from JOIN rsp
 * @mac_ctx: Global MAC Context
 * @roam_info: CSR Roam Info
 * @join_rsp: SME Join response
 *
 * Copy the FILS params from the join results
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS populate_fils_params_join_rsp(struct mac_context *mac_ctx,
						struct csr_roam_info *roam_info,
						struct join_rsp *join_rsp)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct fils_join_rsp_params *roam_fils_info,
				    *fils_join_rsp = join_rsp->fils_join_rsp;

	if (!fils_join_rsp->fils_pmk_len ||
			!fils_join_rsp->fils_pmk || !fils_join_rsp->tk_len ||
			!fils_join_rsp->kek_len || !fils_join_rsp->gtk_len) {
		sme_err("fils join rsp err: pmk len %d tk len %d kek len %d gtk len %d",
			fils_join_rsp->fils_pmk_len,
			fils_join_rsp->tk_len,
			fils_join_rsp->kek_len,
			fils_join_rsp->gtk_len);
		status = QDF_STATUS_E_FAILURE;
		goto free_fils_join_rsp;
	}

	roam_info->fils_join_rsp = qdf_mem_malloc(sizeof(*fils_join_rsp));
	if (!roam_info->fils_join_rsp) {
		status = QDF_STATUS_E_FAILURE;
		goto free_fils_join_rsp;
	}

	roam_fils_info = roam_info->fils_join_rsp;
	roam_fils_info->fils_pmk = qdf_mem_malloc(fils_join_rsp->fils_pmk_len);
	if (!roam_fils_info->fils_pmk) {
		qdf_mem_free(roam_info->fils_join_rsp);
		roam_info->fils_join_rsp = NULL;
		status = QDF_STATUS_E_FAILURE;
		goto free_fils_join_rsp;
	}

	roam_info->fils_seq_num = join_rsp->fils_seq_num;
	roam_fils_info->fils_pmk_len = fils_join_rsp->fils_pmk_len;
	qdf_mem_copy(roam_fils_info->fils_pmk,
		     fils_join_rsp->fils_pmk, roam_fils_info->fils_pmk_len);

	qdf_mem_copy(roam_fils_info->fils_pmkid,
		     fils_join_rsp->fils_pmkid, PMKID_LEN);

	roam_fils_info->kek_len = fils_join_rsp->kek_len;
	qdf_mem_copy(roam_fils_info->kek,
		     fils_join_rsp->kek, roam_fils_info->kek_len);

	roam_fils_info->tk_len = fils_join_rsp->tk_len;
	qdf_mem_copy(roam_fils_info->tk,
		     fils_join_rsp->tk, fils_join_rsp->tk_len);

	roam_fils_info->gtk_len = fils_join_rsp->gtk_len;
	qdf_mem_copy(roam_fils_info->gtk,
		     fils_join_rsp->gtk, roam_fils_info->gtk_len);

	cds_copy_hlp_info(&fils_join_rsp->dst_mac, &fils_join_rsp->src_mac,
			  fils_join_rsp->hlp_data_len, fils_join_rsp->hlp_data,
			  &roam_fils_info->dst_mac, &roam_fils_info->src_mac,
			  &roam_fils_info->hlp_data_len,
			  roam_fils_info->hlp_data);
	sme_debug("FILS connect params copied to CSR!");

free_fils_join_rsp:
	qdf_mem_free(fils_join_rsp->fils_pmk);
	qdf_mem_free(fils_join_rsp);
	return status;
}

/**
 * csr_process_fils_join_rsp() - Process join rsp for FILS connection
 * @mac_ctx: Global MAC Context
 * @profile: CSR Roam Profile
 * @session_id: Session ID
 * @roam_info: CSR Roam Info
 * @bss_desc: BSS description
 * @join_rsp: SME Join rsp
 *
 * Process SME join response for FILS connection
 *
 * Return: None
 */
static void csr_process_fils_join_rsp(struct mac_context *mac_ctx,
					struct csr_roam_profile *profile,
					uint32_t session_id,
					struct csr_roam_info *roam_info,
					struct bss_description *bss_desc,
					struct join_rsp *join_rsp)
{
	QDF_STATUS status;

	if (!join_rsp || !join_rsp->fils_join_rsp) {
		sme_err("Join rsp doesn't have FILS info");
		goto process_fils_join_rsp_fail;
	}

	/* Copy FILS params */
	status = populate_fils_params_join_rsp(mac_ctx, roam_info, join_rsp);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Copy FILS params join rsp fails");
		goto process_fils_join_rsp_fail;
	}

	status = csr_issue_set_context_req_helper(mac_ctx, profile,
				session_id, &bss_desc->bssId, true,
				true, eSIR_TX_RX, 0,
				roam_info->fils_join_rsp->tk_len,
				roam_info->fils_join_rsp->tk);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Set context for unicast fail");
		goto process_fils_join_rsp_fail;
	}

	status = csr_issue_set_context_req_helper(mac_ctx, profile,
			session_id, &bss_desc->bssId, true, false,
			eSIR_RX_ONLY, 2, roam_info->fils_join_rsp->gtk_len,
			roam_info->fils_join_rsp->gtk);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Set context for bcast fail");
		goto process_fils_join_rsp_fail;
	}
	return;

process_fils_join_rsp_fail:
	csr_roam_substate_change(mac_ctx, eCSR_ROAM_SUBSTATE_NONE, session_id);
}
#else

static inline void csr_process_fils_join_rsp(struct mac_context *mac_ctx,
					     struct csr_roam_profile *profile,
					     uint32_t session_id,
					     struct csr_roam_info *roam_info,
					     struct bss_description *bss_desc,
					     struct join_rsp *join_rsp)
{}
#endif

#ifdef WLAN_FEATURE_11BE
static void csr_roam_process_eht_info(struct join_rsp *sme_join_rsp,
				      struct csr_roam_info *roam_info)
{
	roam_info->eht_operation = sme_join_rsp->eht_operation;
}
#else
static inline void csr_roam_process_eht_info(struct join_rsp *sme_join_rsp,
					     struct csr_roam_info *roam_info)
{
}
#endif

#ifdef WLAN_FEATURE_11AX
static void csr_roam_process_he_info(struct join_rsp *sme_join_rsp,
				     struct csr_roam_info *roam_info)
{
	roam_info->he_operation = sme_join_rsp->he_operation;
}
#else
static inline void csr_roam_process_he_info(struct join_rsp *sme_join_rsp,
					    struct csr_roam_info *roam_info)
{
}
#endif

static void csr_update_rsn_intersect_to_fw(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id)
{
	int32_t rsn_val;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);

	if (!vdev) {
		sme_err("Invalid vdev obj for vdev id %d", vdev_id);
		return;
	}

	rsn_val = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_RSN_CAP);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	if (rsn_val < 0) {
		sme_err("Invalid mgmt cipher");
		return;
	}

	if (wma_cli_set2_command(vdev_id, WMI_VDEV_PARAM_RSN_CAPABILITY,
				 rsn_val, 0, VDEV_CMD))
		sme_err("Failed to update WMI_VDEV_PARAM_RSN_CAPABILITY for vdev id %d",
			vdev_id);
}

/**
 * csr_roam_process_join_res() - Process the Join results
 * @mac_ctx:          Global MAC Context
 * @result:           Result after the command was processed
 * @cmd:              Command to be processed
 * @context:          Additional data in context of the cmd
 *
 * Process the join results which are obtained in a successful join
 *
 * Return: None
 */
static void csr_roam_process_join_res(struct mac_context *mac_ctx,
	enum csr_roamcomplete_result res, tSmeCmd *cmd, void *context)
{
	tSirMacAddr bcast_mac = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	sme_QosAssocInfo assoc_info;
	uint32_t key_timeout_interval = 0;
	uint8_t acm_mask = 0;   /* HDD needs ACM mask in assoc rsp callback */
	uint32_t session_id = cmd->vdev_id;
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;
	struct csr_roam_session *session;
	struct bss_description *bss_desc = NULL;
	struct tag_csrscan_result *scan_res = NULL;
	sme_qos_csr_event_indType ind_qos;
	tCsrRoamConnectedProfile *conn_profile = NULL;
	tDot11fBeaconIEs *ies_ptr = NULL;
	struct csr_roam_info *roam_info;
	struct join_rsp *join_rsp = context;
	uint32_t len;
	enum csr_akm_type akm_type;
	uint8_t mdie_present;
	struct cm_roam_values_copy cfg;
	struct wlan_objmgr_vdev *vdev;
	struct qdf_mac_addr connected_bssid;

	if (!join_rsp) {
		sme_err("join_rsp is NULL");
		return;
	}

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);

	conn_profile = &session->connectedProfile;
	sme_debug("receives association indication");

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	if (eCsrReassocSuccess == res) {
		roam_info->reassoc = true;
		ind_qos = SME_QOS_CSR_REASSOC_COMPLETE;
	} else {
		roam_info->reassoc = false;
		ind_qos = SME_QOS_CSR_ASSOC_COMPLETE;
	}

	if (CSR_IS_INFRASTRUCTURE(profile))
		session->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED;
	else
		session->connectState = eCSR_ASSOC_STATE_TYPE_WDS_CONNECTED;

	/*
	 * Use the last connected bssdesc for reassoc-ing to the same AP.
	 * NOTE: What to do when reassoc to a different AP???
	 */
	if ((eCsrHddIssuedReassocToSameAP == cmd->u.roamCmd.roamReason)
		|| (eCsrSmeIssuedReassocToSameAP ==
			cmd->u.roamCmd.roamReason)) {
		bss_desc = session->pConnectBssDesc;
		if (bss_desc)
			qdf_mem_copy(&roam_info->bssid, &bss_desc->bssId,
				     sizeof(struct qdf_mac_addr));
	} else {
		if (cmd->u.roamCmd.pRoamBssEntry) {
			scan_res = GET_BASE_ADDR(cmd->u.roamCmd.pRoamBssEntry,
					struct tag_csrscan_result, Link);
			if (scan_res) {
				bss_desc = &scan_res->Result.BssDescriptor;
				ies_ptr = (tDot11fBeaconIEs *)
					(scan_res->Result.pvIes);
				qdf_mem_copy(&roam_info->bssid,
					     &bss_desc->bssId,
					     sizeof(struct qdf_mac_addr));
			}
		}
	}
	if (bss_desc) {
		roam_info->staId = STA_INVALID_IDX;
		csr_roam_save_connected_information(mac_ctx, session_id,
			profile, bss_desc, ies_ptr);
#ifdef FEATURE_WLAN_ESE
		roam_info->isESEAssoc =
			wlan_cm_get_ese_assoc(mac_ctx->pdev, session_id);
#endif

		/*
		 * csr_roam_state_change also affects sub-state.
		 * Hence, csr_roam_state_change happens first and then
		 * substate change.
		 * Moving even save profile above so that below
		 * mentioned conditon is also met.
		 * JEZ100225: Moved to after saving the profile.
		 * Fix needed in main/latest
		 */
		csr_roam_state_change(mac_ctx,
			eCSR_ROAMING_STATE_JOINED, session_id);

		/*
		 * Make sure the Set Context is issued before link
		 * indication to NDIS.  After link indication is
		 * made to NDIS, frames could start flowing.
		 * If we have not set context with LIM, the frames
		 * will be dropped for the security context may not
		 * be set properly.
		 *
		 * this was causing issues in the 2c_wlan_wep WHQL test
		 * when the SetContext was issued after the link
		 * indication. (Link Indication happens in the
		 * profFSMSetConnectedInfra call).
		 *
		 * this reordering was done on titan_prod_usb branch
		 * and is being replicated here.
		 */

		if (CSR_IS_ENC_TYPE_STATIC
			(profile->negotiatedUCEncryptionType) &&
			!profile->bWPSAssociation) {
			/*
			 * Issue the set Context request to LIM to establish
			 * the Unicast STA context
			 */
			if (QDF_IS_STATUS_ERROR(
				csr_issue_set_context_req_helper(mac_ctx,
					profile, session_id, &bss_desc->bssId,
					false, true, eSIR_TX_RX, 0, 0, NULL))) {
				/* NO keys. these key parameters don't matter */
				sme_debug("Set context for unicast fail");
				csr_roam_substate_change(mac_ctx,
					eCSR_ROAM_SUBSTATE_NONE, session_id);
			}
			/*
			 * Issue the set Context request to LIM
			 * to establish the Broadcast STA context
			 * NO keys. these key parameters don't matter
			 */
			csr_issue_set_context_req_helper(mac_ctx, profile,
							 session_id, &bcast_mac,
							 false, false,
							 eSIR_TX_RX, 0, 0,
							 NULL);
		} else if (CSR_IS_AUTH_TYPE_FILS(profile->negotiatedAuthType)
				&& join_rsp->is_fils_connection) {
			roam_info->is_fils_connection = true;
			csr_process_fils_join_rsp(mac_ctx, profile, session_id,
						  roam_info, bss_desc,
						  join_rsp);
		} else {
			/* Need to wait for supplicant authtication */
			roam_info->fAuthRequired = true;
			/*
			 * Set the substate to WaitForKey in case
			 * authentiation is needed
			 */
			cm_csr_set_ss_wait_for_key(session_id);

			if (profile->bWPSAssociation)
				key_timeout_interval =
					WAIT_FOR_WPS_KEY_TIMEOUT_PERIOD;
			else
				key_timeout_interval =
					WAIT_FOR_KEY_TIMEOUT_PERIOD;

			vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
				mac_ctx->psoc, session_id, WLAN_LEGACY_SME_ID);
			if (!vdev) {
				sme_err("vdev is null for vdev id %d",
					session_id);
				return;
			}
			/*
			 * This time should be long enough for the rest
			 * of the process plus setting key
			 */
			if (!QDF_IS_STATUS_SUCCESS
					(cm_start_wait_for_key_timer(
						vdev,
						key_timeout_interval))) {
				/* Reset state so nothing is blocked. */
				sme_err("Failed preauth timer start");
				csr_roam_substate_change(mac_ctx,
						eCSR_ROAM_SUBSTATE_NONE,
						session_id);
			}
			wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		}

		assoc_info.bss_desc = bss_desc;       /* could be NULL */
		assoc_info.uapsd_mask = join_rsp->uapsd_mask;
		if (context) {
			if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(mac_ctx->psoc,
							   session_id))
				sme_debug("LFR3: Clear Connected info");

			csr_roam_free_connected_info(mac_ctx,
						     &session->connectedInfo);

			akm_type = session->connectedProfile.AuthType;
			wlan_cm_roam_cfg_get_value(mac_ctx->psoc, session_id,
						   MOBILITY_DOMAIN, &cfg);
			mdie_present = cfg.bool_value;
			if (akm_type == eCSR_AUTH_TYPE_FT_SAE && mdie_present) {
				sme_debug("Update the MDID in PMK cache for FT-SAE case");
				cm_update_pmk_cache_ft(mac_ctx->psoc,
							session_id);
			}

			len = join_rsp->assocReqLength +
				join_rsp->assocRspLength +
				join_rsp->beaconLength;
			len += join_rsp->parsedRicRspLen;
#ifdef FEATURE_WLAN_ESE
			len += join_rsp->tspecIeLen;
#endif
			if (len) {
				session->connectedInfo.pbFrames =
					qdf_mem_malloc(len);
				if (session->connectedInfo.pbFrames !=
						NULL) {
					qdf_mem_copy(
						session->connectedInfo.pbFrames,
						join_rsp->frames, len);
					session->connectedInfo.nAssocReqLength =
						join_rsp->assocReqLength;
					session->connectedInfo.nAssocRspLength =
						join_rsp->assocRspLength;
					session->connectedInfo.nBeaconLength =
						join_rsp->beaconLength;
					session->connectedInfo.nRICRspLength =
						join_rsp->parsedRicRspLen;
#ifdef FEATURE_WLAN_ESE
					session->connectedInfo.nTspecIeLength =
						join_rsp->tspecIeLen;
#endif
					roam_info->nAssocReqLength =
						join_rsp->assocReqLength;
					roam_info->nAssocRspLength =
						join_rsp->assocRspLength;
					roam_info->nBeaconLength =
						join_rsp->beaconLength;
					roam_info->pbFrames =
						session->connectedInfo.pbFrames;
				}
			}
			if (cmd->u.roamCmd.fReassoc)
				roam_info->fReassocReq =
					roam_info->fReassocRsp = true;
			roam_info->staId = (uint8_t)join_rsp->staId;
			roam_info->timingMeasCap = join_rsp->timingMeasCap;
			roam_info->chan_info.nss = join_rsp->nss;
			roam_info->chan_info.rate_flags =
				join_rsp->max_rate_flags;
			roam_info->chan_info.ch_width =
				join_rsp->vht_channel_width;
#ifdef FEATURE_WLAN_TDLS
			roam_info->tdls_prohibited = join_rsp->tdls_prohibited;
			roam_info->tdls_chan_swit_prohibited =
				join_rsp->tdls_chan_swit_prohibited;
			sme_debug("tdls:prohibit: %d chan_swit_prohibit: %d",
				roam_info->tdls_prohibited,
				roam_info->tdls_chan_swit_prohibited);
#endif
			roam_info->vht_caps = join_rsp->vht_caps;
			roam_info->ht_caps = join_rsp->ht_caps;
			roam_info->hs20vendor_ie = join_rsp->hs20vendor_ie;
			roam_info->ht_operation = join_rsp->ht_operation;
			roam_info->vht_operation = join_rsp->vht_operation;
			csr_roam_process_he_info(join_rsp, roam_info);
			csr_roam_process_eht_info(join_rsp, roam_info);
		} else {
			if (cmd->u.roamCmd.fReassoc) {
				roam_info->fReassocReq =
					roam_info->fReassocRsp = true;
				roam_info->nAssocReqLength =
					session->connectedInfo.nAssocReqLength;
				roam_info->nAssocRspLength =
					session->connectedInfo.nAssocRspLength;
				roam_info->nBeaconLength =
					session->connectedInfo.nBeaconLength;
				roam_info->pbFrames =
					session->connectedInfo.pbFrames;
			}
		}

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		/*
		 * Indicate SME-QOS with reassoc success event,
		 * only after copying the frames
		 */
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id, ind_qos,
				&assoc_info);
#endif
		roam_info->bss_desc = bss_desc;
		roam_info->status_code =
			session->joinFailStatusCode.status_code;
		roam_info->reasonCode =
			session->joinFailStatusCode.reasonCode;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		acm_mask = sme_qos_get_acm_mask(mac_ctx, bss_desc, NULL);
#endif
		conn_profile->acm_mask = acm_mask;
		conn_profile->modifyProfileFields.uapsd_mask =
						join_rsp->uapsd_mask;
		/*
		 * start UAPSD if uapsd_mask is not 0 because HDD will
		 * configure for trigger frame It may be better to let QoS do
		 * this????
		 */
		if (conn_profile->modifyProfileFields.uapsd_mask) {
			sme_err(
				" uapsd_mask (0x%X) set, request UAPSD now",
				conn_profile->modifyProfileFields.uapsd_mask);
			sme_ps_start_uapsd(MAC_HANDLE(mac_ctx), session_id);
		}
		roam_info->u.pConnectedProfile = conn_profile;

		if (session->bRefAssocStartCnt > 0) {
			session->bRefAssocStartCnt--;
			csr_roam_call_callback(mac_ctx, session_id, roam_info,
					       cmd->u.roamCmd.roamId,
					       eCSR_ROAM_ASSOCIATION_COMPLETION,
					       eCSR_ROAM_RESULT_ASSOCIATED);
		}

		csr_update_scan_entry_associnfo(mac_ctx, session_id,
						SCAN_ENTRY_CON_STATE_ASSOC);
		csr_roam_completion(mac_ctx, session_id, NULL, cmd,
				eCSR_ROAM_RESULT_NONE, true);
	} else {
		sme_warn("Roam command doesn't have a BSS desc");
	}

	if (csr_roam_is_sta_mode(mac_ctx, session_id))
		wlan_cm_roam_state_change(mac_ctx->pdev, session_id,
					  WLAN_ROAM_INIT,
					  REASON_CONNECT);

	/* Not to signal link up because keys are yet to be set.
	 * The linkup function will overwrite the sub-state that
	 * we need to keep at this point.
	 */
	if (!CSR_IS_WAIT_FOR_KEY(mac_ctx, session_id)) {
		if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(mac_ctx->psoc, session_id))
			sme_debug("LFR3: NO WAIT_FOR_KEY do csr_roam_link_up");
		wlan_mlme_get_bssid_vdev_id(mac_ctx->pdev, session_id,
					    &connected_bssid);
		csr_roam_link_up(mac_ctx, connected_bssid);
	}

	csr_update_rsn_intersect_to_fw(mac_ctx->psoc, session_id);
	sme_free_join_rsp_fils_params(roam_info);
	qdf_mem_free(roam_info);
}
#endif

/**
 * csr_roam_process_results() - Process the Roam Results
 * @mac_ctx:      Global MAC Context
 * @cmd:          Command that has been processed
 * @res:          Results available after processing the command
 * @context:      Context
 *
 * Process the available results and make an appropriate decision
 *
 * Return: true if the command can be released, else not.
 */
static bool csr_roam_process_results(struct mac_context *mac_ctx, tSmeCmd *cmd,
				     enum csr_roamcomplete_result res,
					void *context)
{
	bool release_cmd = true;
	struct bss_description *bss_desc = NULL;
	struct csr_roam_info *roam_info;
	uint32_t session_id = cmd->vdev_id;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	struct csr_roam_profile *profile;
	eRoamCmdStatus roam_status = eCSR_ROAM_INFRA_IND;
	eCsrRoamResult roam_result = eCSR_ROAM_RESULT_INFRA_START_FAILED;
	struct start_bss_rsp  *start_bss_rsp = NULL;

	profile = &cmd->u.roamCmd.roamProfile;
	if (!session) {
		sme_err("session %d not found ", session_id);
		return false;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return false;

	sme_debug("Result %d", res);
	switch (res) {
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	case eCsrJoinSuccess:
	case eCsrReassocSuccess:
		csr_roam_process_join_res(mac_ctx, res, cmd, context);
		break;
#endif
	case eCsrStartBssSuccess:
		csr_roam_process_start_bss_success(mac_ctx, cmd, context);
		break;
	case eCsrStartBssFailure:
		start_bss_rsp = (struct start_bss_rsp *)context;

		if (CSR_IS_NDI(profile)) {
			csr_roam_update_ndp_return_params(mac_ctx,
							  eCsrStartBssFailure,
							  &roam_status,
							  &roam_result,
							  roam_info);
		}

		if (context)
			bss_desc = (struct bss_description *) context;
		else
			bss_desc = NULL;
		roam_info->bss_desc = bss_desc;
		csr_roam_call_callback(mac_ctx, session_id, roam_info,
				       cmd->u.roamCmd.roamId, roam_status,
				       roam_result);
		csr_set_default_dot11_mode(mac_ctx);
		break;
#ifndef FEATURE_CM_ENABLE
	case eCsrSilentlyStopRoamingSaveState:
		/* We are here because we try to connect to the same AP */
		/* No message to PE */
		sme_debug("receives silently stop roaming indication");
		qdf_mem_zero(roam_info, sizeof(*roam_info));

		/* to aviod resetting the substate to NONE */
		mac_ctx->roam.curState[session_id] = eCSR_ROAMING_STATE_JOINED;
		/*
		 * No need to change substate to wai_for_key because there
		 * is no state change
		 */
		roam_info->bss_desc = session->pConnectBssDesc;
		if (roam_info->bss_desc)
			qdf_mem_copy(&roam_info->bssid,
				     &roam_info->bss_desc->bssId,
				     sizeof(struct qdf_mac_addr));
		roam_info->status_code =
			session->joinFailStatusCode.status_code;
		roam_info->reasonCode = session->joinFailStatusCode.reasonCode;
		roam_info->nBeaconLength = session->connectedInfo.nBeaconLength;
		roam_info->nAssocReqLength =
			session->connectedInfo.nAssocReqLength;
		roam_info->nAssocRspLength =
			session->connectedInfo.nAssocRspLength;
		roam_info->pbFrames = session->connectedInfo.pbFrames;
		roam_info->u.pConnectedProfile = &session->connectedProfile;
		if (0 == roam_info->staId)
			QDF_ASSERT(0);

		session->bRefAssocStartCnt--;
		csr_roam_call_callback(mac_ctx, session_id, roam_info,
				       cmd->u.roamCmd.roamId,
				       eCSR_ROAM_ASSOCIATION_COMPLETION,
				       eCSR_ROAM_RESULT_ASSOCIATED);
		csr_roam_completion(mac_ctx, session_id, NULL, cmd,
				eCSR_ROAM_RESULT_ASSOCIATED, true);
		break;
#endif /* ndef FEATURE_CM_ENABLE */
	case eCsrReassocFailure:
		/*
		 * Currently Reassoc failure is handled through eCsrJoinFailure
		 * Need to revisit for eCsrReassocFailure handling
		 */
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
				SME_QOS_CSR_REASSOC_FAILURE, NULL);
#endif
		break;
	case eCsrStopBssSuccess:
		if (CSR_IS_NDI(profile)) {
			qdf_mem_zero(roam_info, sizeof(*roam_info));
			csr_roam_update_ndp_return_params(mac_ctx, res,
							  &roam_status,
							  &roam_result,
							  roam_info);
			csr_roam_call_callback(mac_ctx, session_id, roam_info,
					       cmd->u.roamCmd.roamId,
					       roam_status, roam_result);
		}
		break;
	case eCsrStopBssFailure:
		if (CSR_IS_NDI(profile)) {
			qdf_mem_zero(roam_info, sizeof(*roam_info));
			csr_roam_update_ndp_return_params(mac_ctx, res,
							  &roam_status,
							  &roam_result,
							  roam_info);
			csr_roam_call_callback(mac_ctx, session_id, roam_info,
					       cmd->u.roamCmd.roamId,
					       roam_status, roam_result);
		}
		break;
	case eCsrJoinFailure:
	case eCsrNothingToJoin:
	case eCsrJoinFailureDueToConcurrency:
	default:
		csr_roam_process_results_default(mac_ctx, cmd, context, res);
		break;
	}
	qdf_mem_free(roam_info);
	return release_cmd;
}

#ifndef FEATURE_CM_ENABLE
#ifdef WLAN_FEATURE_FILS_SK
/*
 * update_profile_fils_info: API to update FILS info from
 * source profile to destination profile.
 * @des_profile: pointer to destination profile
 * @src_profile: pointer to souce profile
 *
 * Return: None
 */
static void update_profile_fils_info(struct mac_context *mac,
				     struct csr_roam_profile *des_profile,
				     struct csr_roam_profile *src_profile,
				     uint8_t vdev_id)
{
	if (!src_profile || !src_profile->fils_con_info)
		return;

	sme_debug("is fils %d", src_profile->fils_con_info->is_fils_connection);

	if (!src_profile->fils_con_info->is_fils_connection)
		return;

	if (des_profile->fils_con_info)
		qdf_mem_free(des_profile->fils_con_info);

	des_profile->fils_con_info =
		qdf_mem_malloc(sizeof(struct wlan_fils_connection_info));
	if (!des_profile->fils_con_info)
		return;

	qdf_mem_copy(des_profile->fils_con_info,
		     src_profile->fils_con_info,
		     sizeof(struct wlan_fils_connection_info));

	wlan_cm_update_mlme_fils_connection_info(mac->psoc,
						 des_profile->fils_con_info,
						 vdev_id);
}
#else
static inline
void update_profile_fils_info(struct mac_context *mac,
			      struct csr_roam_profile *des_profile,
			      struct csr_roam_profile *src_profile,
			      uint8_t vdev_id)
{}
#endif
#endif

QDF_STATUS csr_roam_copy_profile(struct mac_context *mac,
				 struct csr_roam_profile *pDstProfile,
				 struct csr_roam_profile *pSrcProfile,
				 uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t size = 0;

	qdf_mem_zero(pDstProfile, sizeof(struct csr_roam_profile));
	if (pSrcProfile->BSSIDs.numOfBSSIDs) {
		size = sizeof(struct qdf_mac_addr) * pSrcProfile->BSSIDs.
								numOfBSSIDs;
		pDstProfile->BSSIDs.bssid = qdf_mem_malloc(size);
		if (!pDstProfile->BSSIDs.bssid) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->BSSIDs.numOfBSSIDs =
			pSrcProfile->BSSIDs.numOfBSSIDs;
		qdf_mem_copy(pDstProfile->BSSIDs.bssid,
			pSrcProfile->BSSIDs.bssid, size);
	}
	if (pSrcProfile->SSIDs.numOfSSIDs) {
		size = sizeof(tCsrSSIDInfo) * pSrcProfile->SSIDs.numOfSSIDs;
		pDstProfile->SSIDs.SSIDList = qdf_mem_malloc(size);
		if (!pDstProfile->SSIDs.SSIDList) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->SSIDs.numOfSSIDs =
			pSrcProfile->SSIDs.numOfSSIDs;
		qdf_mem_copy(pDstProfile->SSIDs.SSIDList,
			pSrcProfile->SSIDs.SSIDList, size);
	}
	if (pSrcProfile->nRSNReqIELength) {
		pDstProfile->pRSNReqIE =
			qdf_mem_malloc(pSrcProfile->nRSNReqIELength);
		if (!pDstProfile->pRSNReqIE) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nRSNReqIELength =
			pSrcProfile->nRSNReqIELength;
		qdf_mem_copy(pDstProfile->pRSNReqIE, pSrcProfile->pRSNReqIE,
			pSrcProfile->nRSNReqIELength);
	}

	if (pSrcProfile->ChannelInfo.freq_list) {
		pDstProfile->ChannelInfo.freq_list =
			qdf_mem_malloc(sizeof(uint32_t) *
				       pSrcProfile->ChannelInfo.numOfChannels);
		if (!pDstProfile->ChannelInfo.freq_list) {
			pDstProfile->ChannelInfo.numOfChannels = 0;
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->ChannelInfo.numOfChannels =
			pSrcProfile->ChannelInfo.numOfChannels;
		qdf_mem_copy(pDstProfile->ChannelInfo.freq_list,
			     pSrcProfile->ChannelInfo.freq_list,
			     sizeof(uint32_t) *
			     pSrcProfile->ChannelInfo.numOfChannels);
	}
	pDstProfile->AuthType = pSrcProfile->AuthType;
	pDstProfile->EncryptionType = pSrcProfile->EncryptionType;
	pDstProfile->negotiatedUCEncryptionType =
		pSrcProfile->negotiatedUCEncryptionType;
	pDstProfile->mcEncryptionType = pSrcProfile->mcEncryptionType;
	pDstProfile->negotiatedAuthType = pSrcProfile->negotiatedAuthType;
	pDstProfile->MFPRequired = pSrcProfile->MFPRequired;
	pDstProfile->MFPCapable = pSrcProfile->MFPCapable;
	pDstProfile->BSSType = pSrcProfile->BSSType;
	pDstProfile->phyMode = pSrcProfile->phyMode;
	pDstProfile->csrPersona = pSrcProfile->csrPersona;

	pDstProfile->ch_params.ch_width = pSrcProfile->ch_params.ch_width;
	pDstProfile->ch_params.center_freq_seg0 =
		pSrcProfile->ch_params.center_freq_seg0;
	pDstProfile->ch_params.center_freq_seg1 =
		pSrcProfile->ch_params.center_freq_seg1;
	pDstProfile->ch_params.sec_ch_offset =
		pSrcProfile->ch_params.sec_ch_offset;
	pDstProfile->uapsd_mask = pSrcProfile->uapsd_mask;
	pDstProfile->beaconInterval = pSrcProfile->beaconInterval;
	pDstProfile->privacy = pSrcProfile->privacy;
	pDstProfile->fwdWPSPBCProbeReq = pSrcProfile->fwdWPSPBCProbeReq;
	pDstProfile->csr80211AuthType = pSrcProfile->csr80211AuthType;
	pDstProfile->dtimPeriod = pSrcProfile->dtimPeriod;
	pDstProfile->ApUapsdEnable = pSrcProfile->ApUapsdEnable;
	pDstProfile->SSIDs.SSIDList[0].ssidHidden =
		pSrcProfile->SSIDs.SSIDList[0].ssidHidden;
	pDstProfile->protEnabled = pSrcProfile->protEnabled;
	pDstProfile->obssProtEnabled = pSrcProfile->obssProtEnabled;
	pDstProfile->cfg_protection = pSrcProfile->cfg_protection;
	pDstProfile->wps_state = pSrcProfile->wps_state;
	pDstProfile->ieee80211d = pSrcProfile->ieee80211d;
	pDstProfile->MFPRequired = pSrcProfile->MFPRequired;
	pDstProfile->MFPCapable = pSrcProfile->MFPCapable;
	pDstProfile->add_ie_params = pSrcProfile->add_ie_params;

	pDstProfile->beacon_tx_rate = pSrcProfile->beacon_tx_rate;

	if (pSrcProfile->supported_rates.numRates) {
		qdf_mem_copy(pDstProfile->supported_rates.rate,
				pSrcProfile->supported_rates.rate,
				pSrcProfile->supported_rates.numRates);
		pDstProfile->supported_rates.numRates =
			pSrcProfile->supported_rates.numRates;
	}
	if (pSrcProfile->extended_rates.numRates) {
		qdf_mem_copy(pDstProfile->extended_rates.rate,
				pSrcProfile->extended_rates.rate,
				pSrcProfile->extended_rates.numRates);
		pDstProfile->extended_rates.numRates =
			pSrcProfile->extended_rates.numRates;
	}
	pDstProfile->cac_duration_ms = pSrcProfile->cac_duration_ms;
	pDstProfile->dfs_regdomain   = pSrcProfile->dfs_regdomain;
	pDstProfile->chan_switch_hostapd_rate_enabled  =
		pSrcProfile->chan_switch_hostapd_rate_enabled;
#ifndef FEATURE_CM_ENABLE
	pDstProfile->negotiatedMCEncryptionType =
		pSrcProfile->negotiatedMCEncryptionType;
	pDstProfile->MFPEnabled = pSrcProfile->MFPEnabled;
	if (pSrcProfile->nWPAReqIELength) {
		pDstProfile->pWPAReqIE =
			qdf_mem_malloc(pSrcProfile->nWPAReqIELength);
		if (!pDstProfile->pWPAReqIE) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nWPAReqIELength =
			pSrcProfile->nWPAReqIELength;
		qdf_mem_copy(pDstProfile->pWPAReqIE, pSrcProfile->pWPAReqIE,
			pSrcProfile->nWPAReqIELength);
	}
#ifdef FEATURE_WLAN_WAPI
	if (pSrcProfile->nWAPIReqIELength) {
		pDstProfile->pWAPIReqIE =
			qdf_mem_malloc(pSrcProfile->nWAPIReqIELength);
		if (!pDstProfile->pWAPIReqIE) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nWAPIReqIELength =
			pSrcProfile->nWAPIReqIELength;
		qdf_mem_copy(pDstProfile->pWAPIReqIE, pSrcProfile->pWAPIReqIE,
			pSrcProfile->nWAPIReqIELength);
	}
	if (csr_is_profile_wapi(pSrcProfile))
		if (pDstProfile->phyMode & eCSR_DOT11_MODE_11n)
			pDstProfile->phyMode &= ~eCSR_DOT11_MODE_11n;
#endif /* FEATURE_WLAN_WAPI */
	if (pSrcProfile->nAddIEScanLength && pSrcProfile->pAddIEScan) {
		pDstProfile->pAddIEScan =
			qdf_mem_malloc(pSrcProfile->nAddIEScanLength);
		if (!pDstProfile->pAddIEScan) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nAddIEScanLength =
			pSrcProfile->nAddIEScanLength;
		qdf_mem_copy(pDstProfile->pAddIEScan, pSrcProfile->pAddIEScan,
			pSrcProfile->nAddIEScanLength);
	}
	if (pSrcProfile->nAddIEAssocLength) {
		pDstProfile->pAddIEAssoc =
			qdf_mem_malloc(pSrcProfile->nAddIEAssocLength);
		if (!pDstProfile->pAddIEAssoc) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nAddIEAssocLength =
			pSrcProfile->nAddIEAssocLength;
		qdf_mem_copy(pDstProfile->pAddIEAssoc, pSrcProfile->pAddIEAssoc,
			pSrcProfile->nAddIEAssocLength);
	}
	/*Save the WPS info */
	pDstProfile->bWPSAssociation = pSrcProfile->bWPSAssociation;
	pDstProfile->bOSENAssociation = pSrcProfile->bOSENAssociation;
	pDstProfile->mdid = pSrcProfile->mdid;
	update_profile_fils_info(mac, pDstProfile, pSrcProfile, vdev_id);
	pDstProfile->force_24ghz_in_ht20 = pSrcProfile->force_24ghz_in_ht20;
	pDstProfile->force_rsne_override = pSrcProfile->force_rsne_override;
#endif
end:
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		csr_release_profile(mac, pDstProfile);
		pDstProfile = NULL;
	}

	return status;
}

QDF_STATUS csr_roam_issue_connect(struct mac_context *mac, uint32_t sessionId,
				  struct csr_roam_profile *pProfile,
				  tScanResultHandle hBSSList,
				  enum csr_roam_reason reason, uint32_t roamId,
				  bool fImediate, bool fClearScan)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	pCommand = csr_get_command_buffer(mac);
	if (!pCommand) {
		csr_scan_result_purge(mac, hBSSList);
		sme_err(" fail to get command buffer");
		status = QDF_STATUS_E_RESOURCES;
	} else {
		pCommand->u.roamCmd.fReleaseProfile = false;
		if (!pProfile) {
			/* We can roam now
			 * Since pProfile is NULL, we need to build our own
			 * profile, set everything to default We can only
			 * support open and no encryption
			 */
			pCommand->u.roamCmd.roamProfile.AuthType.numEntries = 1;
			pCommand->u.roamCmd.roamProfile.AuthType.authType[0] =
				eCSR_AUTH_TYPE_OPEN_SYSTEM;
			pCommand->u.roamCmd.roamProfile.EncryptionType.
			numEntries = 1;
			pCommand->u.roamCmd.roamProfile.EncryptionType.
			encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;
			pCommand->u.roamCmd.roamProfile.csrPersona =
				QDF_STA_MODE;
		} else {
			/* make a copy of the profile */
			status = csr_roam_copy_profile(mac, &pCommand->u.
						       roamCmd.roamProfile,
						       pProfile, sessionId);
			if (QDF_IS_STATUS_SUCCESS(status))
				pCommand->u.roamCmd.fReleaseProfile = true;
		}

		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.roamCmd.hBSSList = hBSSList;
		pCommand->u.roamCmd.roamId = roamId;
		pCommand->u.roamCmd.roamReason = reason;

		/* We need to free the BssList when the command is done */
		pCommand->u.roamCmd.fReleaseBssList = true;
		pCommand->u.roamCmd.fUpdateCurRoamProfile = true;

		status = csr_queue_sme_command(mac, pCommand, fImediate);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("fail to send message status: %d", status);
		}
	}

	return status;
}

#ifdef FEATURE_CM_ENABLE
QDF_STATUS csr_roam_connect(struct mac_context *mac, uint32_t vdev_id,
		struct csr_roam_profile *profile,
		uint32_t *pRoamId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t roam_id = 0;
	struct csr_roam_session *session = CSR_GET_SESSION(mac, vdev_id);
	struct wlan_serialization_queued_cmd_info cmd = {0};
	struct wlan_objmgr_vdev *vdev;

	if (!session) {
		sme_err("session does not exist for given sessionId: %d",
			vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (!profile) {
		sme_err("No profile specified");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev not found for id %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	sme_debug("Persona %d authtype %d  encryType %d mc_encType %d",
		  profile->csrPersona, profile->AuthType.authType[0],
		  profile->EncryptionType.encryptionType[0],
		  profile->mcEncryptionType.encryptionType[0]);

	/* Flush any pending vdev start command */
	cmd.vdev = vdev;
	cmd.cmd_type = WLAN_SER_CMD_VDEV_START_BSS;
	cmd.req_type = WLAN_SER_CANCEL_VDEV_NON_SCAN_CMD_TYPE;
	cmd.requestor = WLAN_UMAC_COMP_MLME;
	cmd.queue_type = WLAN_SERIALIZATION_PENDING_QUEUE;

	wlan_serialization_cancel_request(&cmd);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	roam_id = GET_NEXT_ROAM_ID(&mac->roam);
	if (pRoamId)
		*pRoamId = roam_id;

	if (CSR_IS_INFRA_AP(profile) || CSR_IS_NDI(profile)) {
		status = csr_roam_issue_connect(mac, vdev_id, profile, NULL,
						eCsrHddIssued, roam_id,
						false, false);
		if (QDF_IS_STATUS_ERROR(status))
			sme_err("CSR failed to issue start BSS/NDI cmd with status: 0x%08X",
				status);
	}

	return status;
}
#else

QDF_STATUS csr_roam_copy_connected_profile(struct mac_context *mac,
					   uint32_t sessionId,
					   struct csr_roam_profile *pDstProfile)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tCsrRoamConnectedProfile *pSrcProfile =
		&mac->roam.roamSession[sessionId].connectedProfile;
	struct cm_roam_values_copy cfg;
	struct wlan_ssid ssid;

	qdf_mem_zero(pDstProfile, sizeof(struct csr_roam_profile));

	pDstProfile->BSSIDs.bssid = qdf_mem_malloc(sizeof(struct qdf_mac_addr));
	if (!pDstProfile->BSSIDs.bssid) {
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}
	pDstProfile->BSSIDs.numOfBSSIDs = 1;
	wlan_mlme_get_bssid_vdev_id(mac->pdev, sessionId,
				    pDstProfile->BSSIDs.bssid);
	wlan_mlme_get_ssid_vdev_id(mac->pdev, sessionId,
				   ssid.ssid, &ssid.length);
	if (ssid.length) {
		pDstProfile->SSIDs.SSIDList =
			qdf_mem_malloc(sizeof(tCsrSSIDInfo));
		if (!pDstProfile->SSIDs.SSIDList) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->SSIDs.numOfSSIDs = 1;
		pDstProfile->SSIDs.SSIDList[0].SSID.length = ssid.length;
		qdf_mem_copy(pDstProfile->SSIDs.SSIDList[0].SSID.ssId,
			     ssid.ssid, ssid.length);
	}
	pDstProfile->ChannelInfo.freq_list = qdf_mem_malloc(sizeof(uint32_t));
	if (!pDstProfile->ChannelInfo.freq_list) {
		pDstProfile->ChannelInfo.numOfChannels = 0;
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}
	pDstProfile->ChannelInfo.numOfChannels = 1;
	pDstProfile->ChannelInfo.freq_list[0] =
		wlan_get_operation_chan_freq_vdev_id(mac->pdev, sessionId);
	pDstProfile->AuthType.numEntries = 1;
	pDstProfile->AuthType.authType[0] = pSrcProfile->AuthType;
	pDstProfile->negotiatedAuthType = pSrcProfile->AuthType;
	pDstProfile->EncryptionType.numEntries = 1;
	pDstProfile->EncryptionType.encryptionType[0] =
		pSrcProfile->EncryptionType;
	pDstProfile->negotiatedUCEncryptionType =
		pSrcProfile->EncryptionType;
	pDstProfile->mcEncryptionType.numEntries = 1;
	pDstProfile->mcEncryptionType.encryptionType[0] =
		pSrcProfile->mcEncryptionType;
	pDstProfile->negotiatedMCEncryptionType =
		pSrcProfile->mcEncryptionType;
	pDstProfile->BSSType = pSrcProfile->BSSType;
	wlan_cm_roam_cfg_get_value(mac->psoc, sessionId,
				   MOBILITY_DOMAIN, &cfg);
	pDstProfile->mdid.mobility_domain = cfg.uint_value;
	pDstProfile->mdid.mdie_present = cfg.bool_value;
end:
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		csr_release_profile(mac, pDstProfile);
		pDstProfile = NULL;
	}

	return status;
}

QDF_STATUS csr_roam_issue_reassoc(struct mac_context *mac, uint32_t sessionId,
				  struct csr_roam_profile *pProfile,
				  tCsrRoamModifyProfileFields
				*pMmodProfileFields,
				  enum csr_roam_reason reason, uint32_t roamId,
				  bool fImediate)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	pCommand = csr_get_command_buffer(mac);
	if (!pCommand) {
		sme_err("fail to get command buffer");
		status = QDF_STATUS_E_RESOURCES;
	} else {
		csr_scan_abort_mac_scan(mac, sessionId, INVAL_SCAN_ID);
		if (pProfile) {
			/* This is likely trying to reassoc to
			 * different profile
			 */
			pCommand->u.roamCmd.fReleaseProfile = false;
			/* make a copy of the profile */
			status = csr_roam_copy_profile(mac, &pCommand->u.
							roamCmd.roamProfile,
						      pProfile, sessionId);
			pCommand->u.roamCmd.fUpdateCurRoamProfile = true;
		} else {
			status = csr_roam_copy_connected_profile(mac,
							sessionId,
							&pCommand->u.roamCmd.
							roamProfile);
			/* how to update WPA/WPA2 info in roamProfile?? */
			pCommand->u.roamCmd.roamProfile.uapsd_mask =
				pMmodProfileFields->uapsd_mask;
		}
		if (QDF_IS_STATUS_SUCCESS(status))
			pCommand->u.roamCmd.fReleaseProfile = true;
		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamId = roamId;
		pCommand->u.roamCmd.roamReason = reason;
		/* We need to free the BssList when the command is done */
		/* For reassoc there is no BSS list, so the bool set to false */
		pCommand->u.roamCmd.hBSSList = CSR_INVALID_SCANRESULT_HANDLE;
		pCommand->u.roamCmd.fReleaseBssList = false;
		pCommand->u.roamCmd.fReassoc = true;
		csr_roam_remove_duplicate_command(mac, sessionId, pCommand,
						  reason);
		status = csr_queue_sme_command(mac, pCommand, fImediate);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("fail to send message status = %d", status);
			csr_roam_completion(mac, sessionId, NULL, NULL,
					    eCSR_ROAM_RESULT_FAILURE, false);
		}
	}
	return status;
}

QDF_STATUS csr_dequeue_roam_command(struct mac_context *mac,
			enum csr_roam_reason reason,
					uint8_t session_id)
{
	tListElem *pEntry;
	tSmeCmd *pCommand;

	pEntry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);

	if (pEntry) {
		pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
		if ((eSmeCommandRoam == pCommand->command) &&
		    (eCsrPerformPreauth == reason)) {
			sme_debug("DQ-Command = %d, Reason = %d",
				pCommand->command,
				pCommand->u.roamCmd.roamReason);
			if (csr_nonscan_active_ll_remove_entry(mac, pEntry,
				    LL_ACCESS_LOCK)) {
				csr_release_command(mac, pCommand);
			}
		} else if ((eSmeCommandRoam == pCommand->command) &&
			   (eCsrSmeIssuedFTReassoc == reason)) {
			sme_debug("DQ-Command = %d, Reason = %d",
				pCommand->command,
				pCommand->u.roamCmd.roamReason);
			if (csr_nonscan_active_ll_remove_entry(mac, pEntry,
				    LL_ACCESS_LOCK)) {
				csr_release_command(mac, pCommand);
			}
		} else {
			sme_err("Command = %d, Reason = %d ",
				pCommand->command,
				pCommand->u.roamCmd.roamReason);
		}
	} else {
		sme_err("pEntry NULL for eWNI_SME_FT_PRE_AUTH_RSP");
	}
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_FILS_SK
/**
 * csr_is_fils_connection() - API to check if FILS connection
 * @profile: CSR Roam Profile
 *
 * Return: true, if fils connection, false otherwise
 */
static bool csr_is_fils_connection(struct csr_roam_profile *profile)
{
	if (!profile->fils_con_info)
		return false;

	return profile->fils_con_info->is_fils_connection;
}
#else
static bool csr_is_fils_connection(struct csr_roam_profile *pProfile)
{
	return false;
}
#endif

/**
 * csr_roam_print_candidate_aps() - print all candidate AP in sorted
 * score.
 * @results: scan result
 *
 * Return : void
 */
static void csr_roam_print_candidate_aps(tScanResultHandle results)
{
	tListElem *entry;
	struct tag_csrscan_result *bss_desc = NULL;
	struct scan_result_list *bss_list = NULL;

	if (!results)
		return;
	bss_list = (struct scan_result_list *)results;
	entry = csr_ll_peek_head(&bss_list->List, LL_ACCESS_NOLOCK);
	while (entry) {
		bss_desc = GET_BASE_ADDR(entry,
				struct tag_csrscan_result, Link);
		sme_nofl_debug(QDF_MAC_ADDR_FMT " score: %d",
			QDF_MAC_ADDR_REF(bss_desc->Result.BssDescriptor.bssId),
			bss_desc->bss_score);

		entry = csr_ll_next(&bss_list->List, entry,
				LL_ACCESS_NOLOCK);
	}
}

#ifdef CONFIG_BAND_6GHZ
bool csr_connect_security_valid_for_6ghz(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id,
					 struct csr_roam_profile *profile)
{
	const uint8_t *rsnxe;
	uint16_t rsn_caps;
	uint32_t key_mgmt;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev not found for id %d", vdev_id);
		return false;
	}
	key_mgmt = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	rsn_caps = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_RSN_CAP);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	rsnxe = wlan_get_ie_ptr_from_eid(WLAN_ELEMID_RSNXE,
					 profile->pAddIEAssoc,
					 profile->nAddIEAssocLength);

	return wlan_cm_6ghz_allowed_for_akm(psoc, key_mgmt, rsn_caps,
					    rsnxe, 0, profile->bWPSAssociation);
}
#endif

QDF_STATUS csr_roam_connect(struct mac_context *mac, uint32_t sessionId,
		struct csr_roam_profile *pProfile,
		uint32_t *pRoamId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tScanResultHandle hBSSList;
	struct scan_filter *filter;
	uint32_t roamId = 0;
	bool fCallCallback = false;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	struct bss_description *first_ap_profile;
	enum QDF_OPMODE opmode = QDF_STA_MODE;
	uint32_t ch_freq;
	tSirMacSSid ssid;

	if (!pSession) {
		sme_err("session does not exist for given sessionId: %d",
			sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (!pProfile) {
		sme_err("No profile specified");
		return QDF_STATUS_E_FAILURE;
	}

	first_ap_profile = qdf_mem_malloc(sizeof(*first_ap_profile));
	if (!first_ap_profile)
		return QDF_STATUS_E_NOMEM;

	/* Initialize the count before proceeding with the Join requests */
	pSession->join_bssid_count = 0;
	pSession->discon_in_progress = false;
	pSession->is_fils_connection = csr_is_fils_connection(pProfile);
	sme_debug("Persona %d authtype %d  encryType %d mc_encType %d",
		  pProfile->csrPersona, pProfile->AuthType.authType[0],
		  pProfile->EncryptionType.encryptionType[0],
		  pProfile->mcEncryptionType.encryptionType[0]);
	csr_roam_cancel_roaming(mac, sessionId);
	csr_scan_abort_mac_scan(mac, sessionId, INVAL_SCAN_ID);
	csr_roam_remove_duplicate_command(mac, sessionId, NULL, eCsrHddIssued);

	wlan_mlme_get_ssid_vdev_id(mac->pdev, sessionId,
				   ssid.ssId, &ssid.length);

	/* Check whether ssid changes */
	if (csr_is_conn_state_connected(mac, sessionId) &&
	    pProfile->SSIDs.numOfSSIDs &&
	    !csr_is_ssid_in_list(&ssid, &pProfile->SSIDs))
		csr_roam_issue_disassociate_cmd(mac, sessionId,
					eCSR_DISCONNECT_REASON_UNSPECIFIED,
					REASON_UNSPEC_FAILURE);

	/*
	 * If roamSession.connectState is disconnecting that mean
	 * disconnect was received with scan for ssid in progress
	 * and dropped. This state will ensure that connect will
	 * not be issued from scan for ssid completion. Thus
	 * if this fresh connect also issue scan for ssid the connect
	 * command will be dropped assuming disconnect is in progress.
	 * Thus reset connectState here
	 */
	if (mac->roam.roamSession[sessionId].connectState ==
	    eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTING)
		mac->roam.roamSession[sessionId].connectState =
			eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;

	filter = qdf_mem_malloc(sizeof(*filter));
	if (!filter) {
		status = QDF_STATUS_E_NOMEM;
		goto error;
	}

	/* Here is the profile we need to connect to */
	status = csr_roam_get_scan_filter_from_profile(mac, pProfile,
						       filter, false,
						       sessionId);
	opmode = pProfile->csrPersona;
	roamId = GET_NEXT_ROAM_ID(&mac->roam);
	if (pRoamId)
		*pRoamId = roamId;
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_free(filter);
		goto error;
	}

	if (pProfile && CSR_IS_INFRA_AP(pProfile)) {
		/* This can be started right away */
		status = csr_roam_issue_connect(mac, sessionId, pProfile, NULL,
				 eCsrHddIssued, roamId, false, false);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("CSR failed to issue start BSS cmd with status: 0x%08X",
				status);
			fCallCallback = true;
		} else
			sme_debug("Connect request to proceed for sap mode");

		qdf_mem_free(filter);
		goto error;
	}

	if (opmode == QDF_STA_MODE || opmode == QDF_P2P_CLIENT_MODE)
		if (!csr_connect_security_valid_for_6ghz(mac->psoc, sessionId,
							 pProfile))
			filter->ignore_6ghz_channel = true;

	status = csr_scan_get_result(mac, filter, &hBSSList,
				     opmode == QDF_STA_MODE ? true : false);
	qdf_mem_free(filter);
	csr_roam_print_candidate_aps(hBSSList);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* check if set hw mode needs to be done */
		if ((opmode == QDF_STA_MODE) ||
		    (opmode == QDF_P2P_CLIENT_MODE)) {
			bool ok;

			csr_get_bssdescr_from_scan_handle(hBSSList,
							  first_ap_profile);
			status = policy_mgr_is_chan_ok_for_dnbs(
					mac->psoc,
					first_ap_profile->chan_freq, &ok);
			if (QDF_IS_STATUS_ERROR(status)) {
				sme_debug("policy_mgr_is_chan_ok_for_dnbs():error:%d",
					  status);
				csr_scan_result_purge(mac, hBSSList);
				fCallCallback = true;
				goto error;
			}
			if (!ok) {
				sme_debug("freq:%d not ok for DNBS",
					  first_ap_profile->chan_freq);
				csr_scan_result_purge(mac, hBSSList);
				fCallCallback = true;
				status = QDF_STATUS_E_INVAL;
				goto error;
			}

			ch_freq = csr_get_channel_for_hw_mode_change
					(mac, hBSSList, sessionId);
			if (!ch_freq)
				ch_freq = first_ap_profile->chan_freq;

			status = policy_mgr_handle_conc_multiport(
					mac->psoc, sessionId, ch_freq,
					POLICY_MGR_UPDATE_REASON_NORMAL_STA,
					POLICY_MGR_DEF_REQ_ID);
			if ((QDF_IS_STATUS_SUCCESS(status)) &&
				(!csr_wait_for_connection_update(mac, true))) {
					sme_debug("conn update error");
					csr_scan_result_purge(mac, hBSSList);
					fCallCallback = true;
					status = QDF_STATUS_E_TIMEOUT;
					goto error;
			} else if (status == QDF_STATUS_E_FAILURE) {
				sme_debug("conn update error");
				csr_scan_result_purge(mac, hBSSList);
				fCallCallback = true;
				goto error;
			}
		}

		status = csr_roam_issue_connect(mac, sessionId, pProfile,
				hBSSList, eCsrHddIssued, roamId, false, false);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("CSR failed to issue connect cmd with status: 0x%08X",
				status);
			fCallCallback = true;
		}
	} else if (pProfile) {
		if (CSR_IS_NDI(pProfile)) {
			status = csr_roam_issue_connect(mac, sessionId,
					pProfile, NULL, eCsrHddIssued,
					roamId, false, false);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_err("Failed with status = 0x%08X",
					status);
				fCallCallback = true;
			}
		} else if (status == QDF_STATUS_E_EXISTS &&
			   pProfile->BSSIDs.numOfBSSIDs) {
			sme_debug("Scan entries removed either due to rssi reject or assoc disallowed");
			fCallCallback = true;
		} else {
			/* scan for this SSID */
			status = csr_scan_for_ssid(mac, sessionId, pProfile,
						roamId, true);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_err("CSR failed to issue SSID scan cmd with status: 0x%08X",
					status);
				fCallCallback = true;
			}
		}
	} else {
		fCallCallback = true;
	}

error:
	qdf_mem_free(first_ap_profile);

	return status;
}

/**
 * csr_roam_reassoc() - process reassoc command
 * @mac_ctx:       mac global context
 * @session_id:    session id
 * @profile:       roam profile
 * @mod_fields:    AC info being modified in reassoc
 * @roam_id:       roam id to be populated
 *
 * Return: status of operation
 */
QDF_STATUS
csr_roam_reassoc(struct mac_context *mac_ctx, uint32_t session_id,
		 struct csr_roam_profile *profile,
		 tCsrRoamModifyProfileFields mod_fields,
		 uint32_t *roam_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool fCallCallback = true;
	uint32_t roamId = 0;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	tSirMacSSid ssid;

	if (!profile) {
		sme_err("No profile specified");
		return QDF_STATUS_E_FAILURE;
	}
	sme_debug(
		"called  BSSType = %s (%d) authtype = %d  encryType = %d",
		sme_bss_type_to_string(profile->BSSType),
		profile->BSSType, profile->AuthType.authType[0],
		profile->EncryptionType.encryptionType[0]);
	csr_roam_cancel_roaming(mac_ctx, session_id);
	csr_scan_abort_mac_scan(mac_ctx, session_id, INVAL_SCAN_ID);
	csr_roam_remove_duplicate_command(mac_ctx, session_id, NULL,
					  eCsrHddIssuedReassocToSameAP);
	wlan_mlme_get_ssid_vdev_id(mac_ctx->pdev, session_id,
				   ssid.ssId, &ssid.length);
	if (csr_is_conn_state_connected(mac_ctx, session_id)) {
		if (profile) {
			if (profile->SSIDs.numOfSSIDs &&
			    csr_is_ssid_in_list(&ssid, &profile->SSIDs)) {
				fCallCallback = false;
			} else {
				/*
				 * Connected SSID did not match with what is
				 * asked in profile
				 */
				sme_debug("SSID mismatch");
			}
		} else if (qdf_mem_cmp(&mod_fields,
				&session->connectedProfile.modifyProfileFields,
				sizeof(tCsrRoamModifyProfileFields))) {
			fCallCallback = false;
		} else {
			sme_debug(
				/*
				 * Either the profile is NULL or none of the
				 * fields in tCsrRoamModifyProfileFields got
				 * modified
				 */
				"Profile NULL or nothing to modify");
		}
	} else {
		sme_debug("Not connected! No need to reassoc");
	}

	if (!fCallCallback) {
		roamId = GET_NEXT_ROAM_ID(&mac_ctx->roam);
		if (roam_id)
			*roam_id = roamId;
		status = csr_roam_issue_reassoc(mac_ctx, session_id, profile,
				&mod_fields, eCsrHddIssuedReassocToSameAP,
				roamId, false);
	}

	return status;
}
#endif

bool cm_csr_is_ss_wait_for_key(uint8_t vdev_id)
{
	struct mac_context *mac;
	bool is_wait_for_key = false;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return is_wait_for_key;
	}

	spin_lock(&mac->roam.roam_state_lock);
	if (CSR_IS_WAIT_FOR_KEY(mac, vdev_id))
		is_wait_for_key = true;
	spin_unlock(&mac->roam.roam_state_lock);

	return is_wait_for_key;
}

void cm_csr_set_ss_wait_for_key(uint8_t vdev_id)
{
	struct mac_context *mac;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return;
	}

	csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_WAIT_FOR_KEY,
				 vdev_id);
}

void cm_csr_set_joining(uint8_t vdev_id)
{
	struct mac_context *mac;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return;
	}

	csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING, vdev_id);
}

void cm_csr_set_joined(uint8_t vdev_id)
{
	struct mac_context *mac;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return;
	}

	csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINED, vdev_id);
}

void cm_csr_set_idle(uint8_t vdev_id)
{
	struct mac_context *mac;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return;
	}

	csr_roam_state_change(mac, eCSR_ROAMING_STATE_IDLE, vdev_id);
}

void cm_csr_set_ss_none(uint8_t vdev_id)
{
	struct mac_context *mac;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return;
	}

	csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
				 vdev_id);
}

#ifndef FEATURE_CM_ENABLE
bool cm_csr_is_handoff_in_progress(uint8_t vdev_id)
{
	struct mac_context *mac;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return false;
	}

	return csr_neighbor_roam_is_handoff_in_progress(mac, vdev_id);
}

void cm_csr_disconnect_on_wait_key_timeout(uint8_t vdev_id)
{
	struct mac_context *mac;
	struct csr_roam_session *session;
	QDF_STATUS status;
	struct qdf_mac_addr connected_bssid;

	mac = sme_get_mac_context();
	if (!mac) {
		sme_err("mac_ctx is NULL");
		return;
	}

	session = CSR_GET_SESSION(mac, vdev_id);
	if (!session) {
		sme_err("session: %d not found ", vdev_id);
		return;
	}

	status = sme_acquire_global_lock(&mac->sme);
	if (csr_is_conn_state_connected_infra(mac, vdev_id)) {
		wlan_mlme_get_bssid_vdev_id(mac->pdev, vdev_id,
					    &connected_bssid);
		csr_roam_link_up(mac, connected_bssid);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			csr_roam_disconnect(mac, vdev_id,
					    eCSR_DISCONNECT_REASON_UNSPECIFIED,
					    REASON_KEY_TIMEOUT);
		} else {
			sme_debug(" SME session timeout");
			return;
		}
	}
	sme_release_global_lock(&mac->sme);
}

/* This is temp ifdef will be removed in near future */
QDF_STATUS csr_roam_process_disassoc_deauth(struct mac_context *mac,
						tSmeCmd *pCommand,
					    bool fDisassoc, bool fMICFailure)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool fComplete = false;
	enum csr_roam_substate NewSubstate;
	uint32_t sessionId = pCommand->vdev_id;

	if (CSR_IS_WAIT_FOR_KEY(mac, sessionId)) {
		sme_debug("Stop Wait for key timer and change substate to eCSR_ROAM_SUBSTATE_NONE");
		cm_stop_wait_for_key_timer(mac->psoc, sessionId);
		csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
					sessionId);
	}
	/* change state to 'Roaming'... */
	csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING, sessionId);
	mlme_set_discon_reason_n_from_ap(mac->psoc, pCommand->vdev_id, false,
					 pCommand->u.roamCmd.disconnect_reason);
	if (csr_is_conn_state_infra(mac, sessionId)) {
		/*
		 * in Infrastructure, we need to disassociate from the
		 * Infrastructure network...
		 */
		NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_FORCED;
		if (eCsrSmeIssuedDisassocForHandoff ==
		    pCommand->u.roamCmd.roamReason) {
			NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF;
		} else
		if ((eCsrForcedDisassoc == pCommand->u.roamCmd.roamReason)
		    && (pCommand->u.roamCmd.reason ==
			REASON_DISASSOC_NETWORK_LEAVING)) {
			NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_STA_HAS_LEFT;
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
					 "set to substate eCSR_ROAM_SUBSTATE_DISASSOC_STA_HAS_LEFT");
		}
		if (eCsrSmeIssuedDisassocForHandoff !=
				pCommand->u.roamCmd.roamReason) {
			/*
			 * If we are in neighbor preauth done state then
			 * on receiving disassoc or deauth we dont roam
			 * instead we just disassoc from current ap and
			 * then go to disconnected state.
			 * This happens for ESE and 11r FT connections ONLY.
			 */
			if (csr_roam_is11r_assoc(mac, sessionId) &&
				(csr_neighbor_roam_state_preauth_done(mac,
							sessionId)))
				csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac, sessionId);
#ifdef FEATURE_WLAN_ESE
			if (csr_roam_is_ese_assoc(mac, sessionId) &&
				(csr_neighbor_roam_state_preauth_done(mac,
							sessionId)))
				csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac, sessionId);
#endif
			if (csr_roam_is_fast_roam_enabled(mac, sessionId) &&
				(csr_neighbor_roam_state_preauth_done(mac,
							sessionId)))
				csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac, sessionId);
		}
		if (fDisassoc)
			status = csr_roam_issue_disassociate(mac, sessionId,
								NewSubstate,
								fMICFailure);
		else
			status = csr_roam_issue_deauth(mac, sessionId,
						eCSR_ROAM_SUBSTATE_DEAUTH_REQ);
		fComplete = (!QDF_IS_STATUS_SUCCESS(status));
	} else {
		/* we got a dis-assoc request while not connected to any peer */
		/* just complete the command */
		fComplete = true;
		status = QDF_STATUS_E_FAILURE;
	}
	if (fComplete)
		csr_roam_complete(mac, eCsrNothingToJoin, NULL, sessionId);

	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (csr_is_conn_state_infra(mac, sessionId)) {
			/* Set the state to disconnect here */
			mac->roam.roamSession[sessionId].connectState =
				eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
		}
	} else
		sme_warn(" failed with status %d", status);
	return status;
}
#endif

QDF_STATUS csr_roam_issue_disassociate_cmd(struct mac_context *mac,
					uint32_t sessionId,
					eCsrRoamDisconnectReason reason,
					enum wlan_reason_code mac_reason)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

#ifdef FEATURE_CM_ENABLE
	if (reason != eCSR_DISCONNECT_REASON_NDI_DELETE)
		return QDF_STATUS_E_INVAL;
#endif

	do {
		pCommand = csr_get_command_buffer(mac);
		if (!pCommand) {
			sme_err(" fail to get command buffer");
			status = QDF_STATUS_E_RESOURCES;
			break;
		}
		/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
		/* Change the substate in case it is wait-for-key */
		if (CSR_IS_WAIT_FOR_KEY(mac, sessionId)) {
			cm_stop_wait_for_key_timer(mac->psoc, sessionId);
			csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
						 sessionId);
		}
#endif
		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
		sme_debug("Disassociate reason: %d, vdev_id: %d mac_reason %d",
			 reason, sessionId, mac_reason);
		csr_update_scan_entry_associnfo(mac, sessionId,
						SCAN_ENTRY_CON_STATE_NONE);
		switch (reason) {
		case eCSR_DISCONNECT_REASON_MIC_ERROR:
			pCommand->u.roamCmd.roamReason =
				eCsrForcedDisassocMICFailure;
			break;
		case eCSR_DISCONNECT_REASON_DEAUTH:
			pCommand->u.roamCmd.roamReason = eCsrForcedDeauth;
			break;
		case eCSR_DISCONNECT_REASON_HANDOFF:
			pCommand->u.roamCmd.roamReason =
				eCsrSmeIssuedDisassocForHandoff;
			break;
		case eCSR_DISCONNECT_REASON_UNSPECIFIED:
		case eCSR_DISCONNECT_REASON_DISASSOC:
			pCommand->u.roamCmd.roamReason = eCsrForcedDisassoc;
			break;
		case eCSR_DISCONNECT_REASON_ROAM_HO_FAIL:
			pCommand->u.roamCmd.roamReason = eCsrForcedDisassoc;
			break;
		case eCSR_DISCONNECT_REASON_STA_HAS_LEFT:
			pCommand->u.roamCmd.roamReason = eCsrForcedDisassoc;
			pCommand->u.roamCmd.reason =
				REASON_DISASSOC_NETWORK_LEAVING;
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				 "SME convert to internal reason code eCsrStaHasLeft");
			break;
		case eCSR_DISCONNECT_REASON_NDI_DELETE:
			pCommand->u.roamCmd.roamReason = eCsrStopBss;
			pCommand->u.roamCmd.roamProfile.BSSType =
				eCSR_BSS_TYPE_NDI;
		default:
			break;
		}
		pCommand->u.roamCmd.disconnect_reason = mac_reason;
#else
		pCommand->u.roamCmd.roamReason = eCsrStopBss;
		pCommand->u.roamCmd.roamProfile.BSSType =
				eCSR_BSS_TYPE_NDI;
		sme_debug("NDI Stop reason: %d, vdev_id: %d mac_reason %d",
			  reason, sessionId, mac_reason);
#endif
		status = csr_queue_sme_command(mac, pCommand, true);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);

	} while (0);
	return status;
}

QDF_STATUS csr_roam_issue_stop_bss_cmd(struct mac_context *mac, uint32_t sessionId,
				       bool fHighPriority)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	pCommand = csr_get_command_buffer(mac);
	if (pCommand) {
		/* Change the substate in case it is wait-for-key */
		if (CSR_IS_WAIT_FOR_KEY(mac, sessionId)) {
			cm_stop_wait_for_key_timer(mac->psoc, sessionId);
			csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
						 sessionId);
		}
		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamReason = eCsrStopBss;
		status = csr_queue_sme_command(mac, pCommand, fHighPriority);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);
	} else {
		sme_err("fail to get command buffer");
		status = QDF_STATUS_E_RESOURCES;
	}
	return status;
}

#ifndef FEATURE_CM_ENABLE
#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
static void
csr_disable_roaming_offload(struct mac_context *mac_ctx, uint32_t vdev_id)
{
	sme_stop_roaming(MAC_HANDLE(mac_ctx), vdev_id,
			 REASON_DRIVER_DISABLED,
			 RSO_INVALID_REQUESTOR);
}
#else
static inline void
csr_disable_roaming_offload(struct mac_context *mac_ctx, uint32_t session_id)
{}
#endif
#endif

QDF_STATUS csr_roam_disconnect(struct mac_context *mac_ctx, uint32_t session_id,
			       eCsrRoamDisconnectReason reason,
			       enum wlan_reason_code mac_reason)
{
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!session) {
		sme_err("session: %d not found ", session_id);
		return QDF_STATUS_E_FAILURE;
	}
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	session->discon_in_progress = true;

	csr_roam_cancel_roaming(mac_ctx, session_id);
	csr_disable_roaming_offload(mac_ctx, session_id);

	csr_roam_remove_duplicate_command(mac_ctx, session_id, NULL,
					  eCsrForcedDisassoc);

	if (csr_is_conn_state_connected(mac_ctx, session_id)
	    || csr_is_roam_command_waiting_for_session(mac_ctx, session_id)
	    || CSR_IS_CONN_NDI(&session->connectedProfile)) {
		status = csr_roam_issue_disassociate_cmd(mac_ctx, session_id,
							 reason, mac_reason);
	} else if (session->scan_info.profile) {
		mac_ctx->roam.roamSession[session_id].connectState =
				eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTING;
		csr_scan_abort_mac_scan(mac_ctx, session_id, INVAL_SCAN_ID);
		status = QDF_STATUS_CMD_NOT_QUEUED;
		sme_debug("Disconnect cmd not queued, Roam command is not present return with status %d",
			  status);
	}
#else
	if (CSR_IS_CONN_NDI(&session->connectedProfile))
		status = csr_roam_issue_disassociate_cmd(mac_ctx, session_id,
							 reason, mac_reason);
#endif
	return status;
}

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
static void csr_fill_single_pmk(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				struct bss_description *bss_desc)
{
	struct cm_roam_values_copy src_cfg;

	src_cfg.bool_value = bss_desc->is_single_pmk;
	wlan_cm_roam_cfg_set_value(psoc, vdev_id,
				   IS_SINGLE_PMK, &src_cfg);
}
#else
static inline void
csr_fill_single_pmk(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		    struct bss_description *bss_desc)
{}
#endif

QDF_STATUS
csr_roam_save_connected_information(struct mac_context *mac,
				    uint32_t sessionId,
				    struct csr_roam_profile *pProfile,
				    struct bss_description *pSirBssDesc,
				    tDot11fBeaconIEs *pIes)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tDot11fBeaconIEs *pIesTemp = pIes;
	struct csr_roam_session *pSession = NULL;
	tCsrRoamConnectedProfile *pConnectProfile = NULL;
#ifndef FEATURE_CM_ENABLE
	struct cm_roam_values_copy src_cfg;
#endif

	pSession = CSR_GET_SESSION(mac, sessionId);
	if (!pSession) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			 "session: %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	pConnectProfile = &pSession->connectedProfile;
#ifndef FEATURE_CM_ENABLE
	/*
	 * In case of LFR2.0, the connected profile is copied into a temporary
	 * profile and cleared and then is copied back. This is not needed for
	 * LFR3.0, since the profile is not cleared.
	 */
	if (!MLME_IS_ROAM_SYNCH_IN_PROGRESS(mac->psoc, sessionId))
#endif
	{

		qdf_mem_zero(&pSession->connectedProfile,
				sizeof(tCsrRoamConnectedProfile));
#ifndef FEATURE_CM_ENABLE
		pConnectProfile->AuthType = pProfile->negotiatedAuthType;
		pConnectProfile->EncryptionType =
			pProfile->negotiatedUCEncryptionType;
		pConnectProfile->mcEncryptionType =
			pProfile->negotiatedMCEncryptionType;
#endif
		pConnectProfile->BSSType = pProfile->BSSType;
		pConnectProfile->modifyProfileFields.uapsd_mask =
			pProfile->uapsd_mask;
	}
	/* Save bssid */
	if (!pSirBssDesc->beaconInterval)
		sme_err("ERROR: Beacon interval is ZERO");
	if (!pIesTemp)
		status = csr_get_parsed_bss_description_ies(mac, pSirBssDesc,
							   &pIesTemp);

#ifndef FEATURE_CM_ENABLE
	if (CSR_IS_INFRASTRUCTURE(pConnectProfile)) {
		if (pSirBssDesc->mdiePresent) {
			src_cfg.bool_value = true;
			src_cfg.uint_value = (pSirBssDesc->mdie[1] << 8) |
					     (pSirBssDesc->mdie[0]);
		} else {
			src_cfg.bool_value = false;
			src_cfg.uint_value = 0;
		}
		wlan_cm_roam_cfg_set_value(mac->psoc, sessionId,
					   MOBILITY_DOMAIN, &src_cfg);

#ifdef FEATURE_WLAN_ESE
		if ((csr_is_profile_ese(pProfile) ||
		     (QDF_IS_STATUS_SUCCESS(status) &&
		      (pIesTemp->ESEVersion.present) &&
		      (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_OPEN_SYSTEM))) &&
		    (mac->mlme_cfg->lfr.ese_enabled))
			wlan_cm_set_ese_assoc(mac->pdev, sessionId, true);
		else
			wlan_cm_set_ese_assoc(mac->pdev, sessionId, false);
#endif
	}
#endif
	/* save ssid */
	if (QDF_IS_STATUS_SUCCESS(status)) {
#ifndef FEATURE_CM_ENABLE
		if (CSR_IS_INFRASTRUCTURE(pConnectProfile)) {
					/* Save the bss desc */
			status = csr_roam_save_connected_bss_desc(mac, sessionId,
								  pSirBssDesc);
			src_cfg.uint_value = pSirBssDesc->mbo_oce_enabled_ap;
			wlan_cm_roam_cfg_set_value(mac->psoc, sessionId,
						   MBO_OCE_ENABLED_AP, &src_cfg);
			csr_fill_single_pmk(mac->psoc, sessionId, pSirBssDesc);
			if (CSR_IS_QOS_BSS(pIesTemp) || pIesTemp->HTCaps.present)
				/* Some HT AP's dont send WMM IE so in that case we
				 * assume all HT Ap's are Qos Enabled AP's
				 */
				pConnectProfile->qap = true;
			else
				pConnectProfile->qap = false;
			if (pIesTemp->ExtCap.present) {
				struct s_ext_cap *p_ext_cap = (struct s_ext_cap *)
								pIesTemp->ExtCap.bytes;
				pConnectProfile->proxy_arp_service =
					p_ext_cap->proxy_arp_service;
			}
		}
#endif
		if (!pIes)
			/* Free memory if it allocated locally */
			qdf_mem_free(pIesTemp);
	}

#ifndef FEATURE_CM_ENABLE
	/* Save Qos connection */
	pConnectProfile->qosConnection =
		mac->roam.roamSession[sessionId].fWMMConnection;

	if (!QDF_IS_STATUS_SUCCESS(status))
		csr_free_connect_bss_desc(mac, sessionId);
#endif

	return status;
}

#ifndef FEATURE_CM_ENABLE
bool is_disconnect_pending(struct mac_context *pmac, uint8_t vdev_id)
{
	tListElem *entry = NULL;
	tListElem *next_entry = NULL;
	tSmeCmd *command = NULL;
	bool disconnect_cmd_exist = false;

	entry = csr_nonscan_pending_ll_peek_head(pmac, LL_ACCESS_NOLOCK);
	while (entry) {
		next_entry = csr_nonscan_pending_ll_next(pmac,
					entry, LL_ACCESS_NOLOCK);

		command = GET_BASE_ADDR(entry, tSmeCmd, Link);
		if (command && CSR_IS_DISCONNECT_COMMAND(command) &&
				command->vdev_id == vdev_id){
			disconnect_cmd_exist = true;
			break;
		}
		entry = next_entry;
	}

	return disconnect_cmd_exist;
}

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
static void
csr_clear_other_bss_sae_single_pmk_entry(struct mac_context *mac,
					 struct bss_description *bss_desc,
					 uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct cm_roam_values_copy src_cfg;
	uint32_t akm;

	wlan_cm_roam_cfg_get_value(mac->psoc, vdev_id,
				   IS_SINGLE_PMK, &src_cfg);
	if (!src_cfg.bool_value)
		return;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL");
		return;
	}

	akm = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (!QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_SAE)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}

	wlan_crypto_selective_clear_sae_single_pmk_entries(vdev,
			(struct qdf_mac_addr *)bss_desc->bssId);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}

static void
csr_delete_current_bss_sae_single_pmk_entry(struct mac_context *mac,
					    struct bss_description *bss_desc,
					    uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_crypto_pmksa pmksa;
	struct cm_roam_values_copy src_cfg;
	uint32_t akm;

	wlan_cm_roam_cfg_get_value(mac->psoc, vdev_id,
				   IS_SINGLE_PMK, &src_cfg);

	if (!src_cfg.bool_value)
		return;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL");
		return;
	}

	akm = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (!QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_SAE)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}

	qdf_copy_macaddr(&pmksa.bssid,
			 (struct qdf_mac_addr *)bss_desc->bssId);
	wlan_crypto_set_del_pmksa(vdev, &pmksa, false);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}
#else
static inline void
csr_clear_other_bss_sae_single_pmk_entry(struct mac_context *mac,
					 struct bss_description *bss_desc,
					 uint8_t vdev_id)
{}

static inline void
csr_delete_current_bss_sae_single_pmk_entry(struct mac_context *mac,
					    struct bss_description *bss_desc,
					    uint8_t vdev_id)
{}
#endif

static void csr_roam_join_rsp_processor(struct mac_context *mac,
					struct join_rsp *pSmeJoinRsp)
{
	tListElem *pEntry = NULL;
	tSmeCmd *pCommand = NULL;
	mac_handle_t mac_handle = MAC_HANDLE(mac);
	struct csr_roam_session *session_ptr;
	struct csr_roam_profile *profile = NULL;
	struct csr_roam_connectedinfo *prev_connect_info;
	struct wlan_crypto_pmksa *pmksa;
	uint32_t len = 0, roamId = 0, reason_code = 0;
	bool is_dis_pending;
	bool use_same_bss = false;
	uint8_t max_retry_count = 1;
	bool retry_same_bss = false;
	bool attempt_next_bss = true;
	enum csr_akm_type auth_type = eCSR_AUTH_TYPE_NONE;

	if (!pSmeJoinRsp) {
		sme_err("Sme Join Response is NULL");
		return;
	}

	session_ptr = CSR_GET_SESSION(mac, pSmeJoinRsp->vdev_id);
	if (!session_ptr) {
		sme_err("session %d not found", pSmeJoinRsp->vdev_id);
		return;
	}

	cm_update_prev_ap_ie(mac->psoc, pSmeJoinRsp->vdev_id,
			     pSmeJoinRsp->beaconLength, pSmeJoinRsp->frames);
	prev_connect_info = &session_ptr->prev_assoc_ap_info;
	/* The head of the active list is the request we sent */
	pEntry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (pEntry)
		pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);

	if (pSmeJoinRsp->is_fils_connection)
		sme_debug("Fils connection");
	/* Copy Sequence Number last used for FILS assoc failure case */
	if (session_ptr->is_fils_connection)
		session_ptr->fils_seq_num = pSmeJoinRsp->fils_seq_num;

	len = pSmeJoinRsp->assocReqLength +
	      pSmeJoinRsp->assocRspLength + pSmeJoinRsp->beaconLength;
	if (prev_connect_info->pbFrames)
		csr_roam_free_connected_info(mac, prev_connect_info);

	prev_connect_info->pbFrames = qdf_mem_malloc(len);
	if (prev_connect_info->pbFrames) {
		qdf_mem_copy(prev_connect_info->pbFrames, pSmeJoinRsp->frames,
			     len);
		prev_connect_info->nAssocReqLength =
					pSmeJoinRsp->assocReqLength;
		prev_connect_info->nAssocRspLength =
					pSmeJoinRsp->assocRspLength;
		prev_connect_info->nBeaconLength = pSmeJoinRsp->beaconLength;
	}

	if (eSIR_SME_SUCCESS == pSmeJoinRsp->status_code) {
		if (pCommand &&
		    eCsrSmeIssuedAssocToSimilarAP ==
		    pCommand->u.roamCmd.roamReason) {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
			sme_qos_csr_event_ind(mac, pSmeJoinRsp->vdev_id,
					    SME_QOS_CSR_HANDOFF_COMPLETE, NULL);
#endif
		}

		session_ptr->nss = pSmeJoinRsp->nss;
		/*
		 * The join bssid count can be reset as soon as
		 * we are done with the join requests and returning
		 * the response to upper layers
		 */
		session_ptr->join_bssid_count = 0;

		/*
		 * On successful connection to sae single pmk AP,
		 * clear all the single pmk AP.
		 */
		if (pCommand && pCommand->u.roamCmd.pLastRoamBss)
			csr_clear_other_bss_sae_single_pmk_entry(mac,
					pCommand->u.roamCmd.pLastRoamBss,
					pSmeJoinRsp->vdev_id);

		csr_roam_complete(mac, eCsrJoinSuccess, (void *)pSmeJoinRsp,
				  pSmeJoinRsp->vdev_id);

		return;
	}

	/* The head of the active list is the request we sent
	 * Try to get back the same profile and roam again
	 */
	if (pCommand) {
		roamId = pCommand->u.roamCmd.roamId;
		profile = &pCommand->u.roamCmd.roamProfile;
		auth_type = profile->AuthType.authType[0];
	}

	reason_code = pSmeJoinRsp->protStatusCode;
	session_ptr->joinFailStatusCode.reasonCode = reason_code;
	session_ptr->joinFailStatusCode.status_code = pSmeJoinRsp->status_code;

	sme_warn("SmeJoinReq failed with status_code= 0x%08X [%d] reason:%d",
		 pSmeJoinRsp->status_code, pSmeJoinRsp->status_code,
		 reason_code);

	is_dis_pending = is_disconnect_pending(mac, session_ptr->sessionId);
	/*
	 * if userspace has issued disconnection or we have reached mac tries,
	 * driver should not continue for next connection.
	 */
	if (is_dis_pending ||
	    session_ptr->join_bssid_count >= CSR_MAX_BSSID_COUNT)
		attempt_next_bss = false;
	/*
	 * Delete the PMKID of the BSSID for which the assoc reject is
	 * received from the AP due to invalid PMKID reason.
	 * This will avoid the driver trying to connect to same AP with
	 * the same stale PMKID if multiple connection attempts to different
	 * bss fail and supplicant issues connect request back to the same
	 * AP.
	 */
	if (reason_code == STATUS_INVALID_PMKID) {
		pmksa = qdf_mem_malloc(sizeof(*pmksa));
		if (!pmksa)
			return;

		sme_warn("Assoc reject from BSSID:"QDF_MAC_ADDR_FMT" due to invalid PMKID",
			 QDF_MAC_ADDR_REF(session_ptr->joinFailStatusCode.bssId));
		qdf_mem_copy(pmksa->bssid.bytes,
			     &session_ptr->joinFailStatusCode.bssId,
			     sizeof(tSirMacAddr));
		sme_roam_del_pmkid_from_cache(mac_handle, session_ptr->vdev_id,
					      pmksa, false);
		qdf_mem_free(pmksa);
		retry_same_bss = true;
	}

	if (pSmeJoinRsp->messageType == eWNI_SME_JOIN_RSP &&
	    pSmeJoinRsp->status_code == eSIR_SME_ASSOC_TIMEOUT_RESULT_CODE &&
	    (mlme_get_reconn_after_assoc_timeout_flag(mac->psoc,
						     pSmeJoinRsp->vdev_id) ||
	    (auth_type == eCSR_AUTH_TYPE_SAE ||
	     auth_type == eCSR_AUTH_TYPE_FT_SAE))) {
		retry_same_bss = true;
		if (auth_type == eCSR_AUTH_TYPE_SAE ||
		    auth_type == eCSR_AUTH_TYPE_FT_SAE)
			wlan_mlme_get_sae_assoc_retry_count(mac->psoc,
							    &max_retry_count);
	}

	if (pSmeJoinRsp->messageType == eWNI_SME_JOIN_RSP &&
	    pSmeJoinRsp->status_code == eSIR_SME_JOIN_TIMEOUT_RESULT_CODE &&
	    pCommand && pCommand->u.roamCmd.hBSSList) {
		struct scan_result_list *bss_list =
		   (struct scan_result_list *)pCommand->u.roamCmd.hBSSList;

		if (csr_ll_count(&bss_list->List) == 1) {
			retry_same_bss = true;
			sme_err("retry_same_bss is set");
			wlan_mlme_get_sae_assoc_retry_count(mac->psoc,
							    &max_retry_count);
		}
	}

	if (attempt_next_bss && retry_same_bss &&
	    pCommand && pCommand->u.roamCmd.pRoamBssEntry) {
		struct tag_csrscan_result *scan_result;

		scan_result =
			GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry,
				      struct tag_csrscan_result, Link);
		/* Retry with same BSSID without PMKID */
		if (scan_result->retry_count < max_retry_count) {
			sme_info("Retry once with same BSSID, status %d reason %d auth_type %d retry count %d max count %d",
				 pSmeJoinRsp->status_code, reason_code,
				 auth_type, scan_result->retry_count,
				 max_retry_count);
			scan_result->retry_count++;
			use_same_bss = true;
		}
	}

	/*
	 * If connection fails with Single PMK bssid, clear this pmk
	 * entry, Flush in case if we are not trying again with same AP
	 */
	if (!use_same_bss && pCommand && pCommand->u.roamCmd.pLastRoamBss)
		csr_delete_current_bss_sae_single_pmk_entry(
			mac, pCommand->u.roamCmd.pLastRoamBss,
			pSmeJoinRsp->vdev_id);

	/* If Join fails while Handoff is in progress, indicate
	 * disassociated event to supplicant to reconnect
	 */
	if (csr_roam_is_handoff_in_progress(mac, pSmeJoinRsp->vdev_id)) {
		csr_roam_call_callback(mac, pSmeJoinRsp->vdev_id, NULL,
				       roamId, eCSR_ROAM_DISASSOCIATED,
				       eCSR_ROAM_RESULT_FORCED);
		/* Should indicate neighbor roam algorithm about the
		 * connect failure here
		 */
		csr_neighbor_roam_indicate_connect(mac, pSmeJoinRsp->vdev_id,
						   QDF_STATUS_E_FAILURE);
	}

	if (pCommand && attempt_next_bss) {
			csr_roam(mac, pCommand, use_same_bss);
			return;
	}

	/*
	 * When the upper layers issue a connect command, there is a roam
	 * command with reason eCsrHddIssued that gets enqueued and an
	 * associated timer for the SME command timeout is started which is
	 * currently 120 seconds. This command would be dequeued only upon
	 * successful connections. In case of join failures, if there are too
	 * many BSS in the cache, and if we fail Join requests with all of them,
	 * there is a chance of timing out the above timer.
	 */
	if (session_ptr->join_bssid_count >= CSR_MAX_BSSID_COUNT)
		sme_err("Excessive Join Req Failures");

	if (is_dis_pending)
		sme_err("disconnect is pending, complete roam");

	if (session_ptr->bRefAssocStartCnt)
		session_ptr->bRefAssocStartCnt--;

	session_ptr->join_bssid_count = 0;
	csr_roam_call_callback(mac, session_ptr->sessionId, NULL, roamId,
			       eCSR_ROAM_ASSOCIATION_COMPLETION,
			       eCSR_ROAM_RESULT_NOT_ASSOCIATED);
	csr_roam_complete(mac, eCsrNothingToJoin, NULL, pSmeJoinRsp->vdev_id);
}

static QDF_STATUS csr_roam_issue_join(struct mac_context *mac, uint32_t sessionId,
				      struct bss_description *pSirBssDesc,
				      tDot11fBeaconIEs *pIes,
				      struct csr_roam_profile *pProfile,
				      uint32_t roamId)
{
	QDF_STATUS status;

	/* Set the roaming substate to 'join attempt'... */
	csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_JOIN_REQ, sessionId);
	/* attempt to Join this BSS... */
	status = csr_send_join_req_msg(mac, sessionId, pSirBssDesc, pProfile,
					pIes, eWNI_SME_JOIN_REQ);
	return status;
}

static void
csr_roam_reissue_roam_command(struct mac_context *mac, uint8_t session_id)
{
	tListElem *pEntry;
	tSmeCmd *pCommand;
	struct csr_roam_info *roam_info;
	uint32_t sessionId;
	struct csr_roam_session *pSession;

	pEntry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (!pEntry) {
		sme_err("Disassoc rsp can't continue, no active CMD");
		return;
	}
	pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
	if (eSmeCommandRoam != pCommand->command) {
		sme_err("Active cmd, is not a roaming CMD");
		return;
	}
	sessionId = pCommand->vdev_id;
	pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return;
	}

	if (!pCommand->u.roamCmd.fStopWds) {
		if (pSession->bRefAssocStartCnt > 0) {
			/*
			 * bRefAssocStartCnt was incremented in
			 * csr_roam_join_next_bss when the roam command issued
			 * previously. As part of reissuing the roam command
			 * again csr_roam_join_next_bss is going increment
			 * RefAssocStartCnt. So make sure to decrement the
			 * bRefAssocStartCnt
			 */
			pSession->bRefAssocStartCnt--;
		}
		if (eCsrStopRoaming == csr_roam_join_next_bss(mac, pCommand,
							      true)) {
			sme_warn("Failed to reissue join command");
			csr_roam_complete(mac, eCsrNothingToJoin, NULL,
					session_id);
		}
		return;
	}
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	roam_info->bss_desc = pCommand->u.roamCmd.pLastRoamBss;
	roam_info->status_code = pSession->joinFailStatusCode.status_code;
	roam_info->reasonCode = pSession->joinFailStatusCode.reasonCode;

	pSession->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED;
	csr_roam_call_callback(mac, sessionId, roam_info,
			       pCommand->u.roamCmd.roamId,
			       eCSR_ROAM_INFRA_IND,
			       eCSR_ROAM_RESULT_INFRA_DISASSOCIATED);

	if (!QDF_IS_STATUS_SUCCESS(csr_roam_issue_stop_bss(mac, sessionId,
					eCSR_ROAM_SUBSTATE_STOP_BSS_REQ))) {
		sme_err("Failed to reissue stop_bss command for WDS");
		csr_roam_complete(mac, eCsrNothingToJoin, NULL, session_id);
	}
	qdf_mem_free(roam_info);
}

bool csr_is_roam_command_waiting_for_session(struct mac_context *mac,
						uint32_t vdev_id)
{
	bool fRet = false;
	tListElem *pEntry;
	tSmeCmd *pCommand = NULL;

	pEntry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_NOLOCK);
	if (pEntry) {
		pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
		if ((eSmeCommandRoam == pCommand->command)
		    && (vdev_id == pCommand->vdev_id)) {
			fRet = true;
		}
	}
	if (false == fRet) {
		pEntry = csr_nonscan_pending_ll_peek_head(mac,
					 LL_ACCESS_NOLOCK);
		while (pEntry) {
			pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
			if ((eSmeCommandRoam == pCommand->command)
			    && (vdev_id == pCommand->vdev_id)) {
				fRet = true;
				break;
			}
			pEntry = csr_nonscan_pending_ll_next(mac, pEntry,
							LL_ACCESS_NOLOCK);
		}
	}

	return fRet;
}
#endif

static void
csr_roaming_state_config_cnf_processor(struct mac_context *mac_ctx,
				       tSmeCmd *cmd, uint8_t vdev_id)
{
	struct tag_csrscan_result *scan_result = NULL;
	struct bss_description *bss_desc = NULL;
	uint32_t session_id;
	struct csr_roam_session *session;
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	tDot11fBeaconIEs *local_ies = NULL;
	bool is_ies_malloced = false;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
#endif
	if (!cmd) {
		sme_err("given sme cmd is null");
		return;
	}
	session_id = cmd->vdev_id;
	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("session %d not found", session_id);
		return;
	}

#ifndef FEATURE_CM_ENABLE
	if (CSR_IS_ROAMING(session) && session->fCancelRoaming) {
		/* the roaming is cancelled. Simply complete the command */
		sme_warn("Roam command canceled");
		csr_roam_complete(mac_ctx, eCsrNothingToJoin, NULL, vdev_id);
		return;
	}
#endif
	/*
	 * Successfully set the configuration parameters for the new Bss.
	 * Attempt to join the roaming Bss
	 */
	if (cmd->u.roamCmd.pRoamBssEntry) {
		scan_result = GET_BASE_ADDR(cmd->u.roamCmd.pRoamBssEntry,
					    struct tag_csrscan_result,
					    Link);
		bss_desc = &scan_result->Result.BssDescriptor;
	}
	if (CSR_IS_INFRA_AP(&cmd->u.roamCmd.roamProfile) ||
	    CSR_IS_NDI(&cmd->u.roamCmd.roamProfile)) {
		if (!QDF_IS_STATUS_SUCCESS(csr_roam_issue_start_bss(mac_ctx,
						session_id, &session->bssParams,
						&cmd->u.roamCmd.roamProfile,
						bss_desc,
						cmd->u.roamCmd.roamId))) {
			sme_err("CSR start BSS failed");
			/* We need to complete the command */
			csr_roam_complete(mac_ctx, eCsrStartBssFailure, NULL,
					  vdev_id);
		}
		return;
	}

	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	if (!cmd->u.roamCmd.pRoamBssEntry) {
		sme_err("pRoamBssEntry is NULL");
		/* We need to complete the command */
		csr_roam_complete(mac_ctx, eCsrJoinFailure, NULL, vdev_id);
		return;
	}

	if (!scan_result) {
		/* If we are roaming TO an Infrastructure BSS... */
		QDF_ASSERT(scan_result);
		return;
	}

	if (!csr_is_infra_bss_desc(bss_desc)) {
		sme_warn("found BSSType mismatching the one in BSS descp");
		return;
	}

	local_ies = (tDot11fBeaconIEs *) scan_result->Result.pvIes;
	if (!local_ies) {
		status = csr_get_parsed_bss_description_ies(mac_ctx, bss_desc,
							    &local_ies);
		if (!QDF_IS_STATUS_SUCCESS(status))
			return;
		is_ies_malloced = true;
	}

	if (csr_is_conn_state_connected_infra(mac_ctx, session_id))
	{
		if (csr_is_ssid_equal(mac_ctx, session->pConnectBssDesc,
				      bss_desc, local_ies)) {
			cmd->u.roamCmd.fReassoc = true;
			csr_roam_issue_reassociate(mac_ctx, session_id,
						   bss_desc, local_ies,
						   &cmd->u.roamCmd.roamProfile);
		} else {
			/*
			 * otherwise, we have to issue a new Join request to LIM
			 * because we disassociated from the previously
			 * associated AP.
			 */
			status = csr_roam_issue_join(mac_ctx, session_id,
					bss_desc, local_ies,
					&cmd->u.roamCmd.roamProfile,
					cmd->u.roamCmd.roamId);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				/* try something else */
				csr_roam(mac_ctx, cmd, false);
			}
		}
	} else {
		status = QDF_STATUS_SUCCESS;
		/*
		 * We need to come with other way to figure out that this is
		 * because of HO in BMP The below API will be only available for
		 * Android as it uses a different HO algorithm. Reassoc request
		 * will be used only for ESE and 11r handoff whereas other
		 * legacy roaming should use join request
		 */
		if (csr_roam_is_handoff_in_progress(mac_ctx, session_id)
		    && csr_roam_is11r_assoc(mac_ctx, session_id)) {
			status = csr_roam_issue_reassociate(mac_ctx,
					session_id, bss_desc,
					local_ies,
					&cmd->u.roamCmd.roamProfile);
		} else
#ifdef FEATURE_WLAN_ESE
		if (csr_roam_is_handoff_in_progress(mac_ctx, session_id)
		   && csr_roam_is_ese_assoc(mac_ctx, session_id)) {
			/* Now serialize the reassoc command. */
			status = csr_roam_issue_reassociate_cmd(mac_ctx,
								session_id);
		} else
#endif
		if (csr_roam_is_handoff_in_progress(mac_ctx, session_id)
		   && csr_roam_is_fast_roam_enabled(mac_ctx, session_id)) {
			/* Now serialize the reassoc command. */
			status = csr_roam_issue_reassociate_cmd(mac_ctx,
								session_id);
		} else {
			/*
			 * else we are not connected and attempting to Join.
			 * Issue the Join request.
			 */
			status = csr_roam_issue_join(mac_ctx, session_id,
						    bss_desc,
						    local_ies,
						    &cmd->u.roamCmd.roamProfile,
						    cmd->u.roamCmd.roamId);
		}
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			/* try something else */
			csr_roam(mac_ctx, cmd, false);
		}
	}
	if (is_ies_malloced) {
		/* Locally allocated */
		qdf_mem_free(local_ies);
	}
#endif
}

#ifndef FEATURE_CM_ENABLE
static void csr_roam_roaming_state_reassoc_rsp_processor(struct mac_context *mac,
						struct join_rsp *pSmeJoinRsp)
{
	enum csr_roamcomplete_result result;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo = NULL;
	struct csr_roam_session *csr_session;

	if (pSmeJoinRsp->vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid session ID received %d", pSmeJoinRsp->vdev_id);
		return;
	}

	pNeighborRoamInfo =
		&mac->roam.neighborRoamInfo[pSmeJoinRsp->vdev_id];
	if (eSIR_SME_SUCCESS == pSmeJoinRsp->status_code) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			 "CSR SmeReassocReq Successful");
		result = eCsrReassocSuccess;
		csr_session = CSR_GET_SESSION(mac, pSmeJoinRsp->vdev_id);
		if (csr_session)
			csr_session->nss = pSmeJoinRsp->nss;
		/*
		 * Since the neighbor roam algorithm uses reassoc req for
		 * handoff instead of join, we need the response contents while
		 * processing the result in csr_roam_process_results()
		 */
		if (csr_roam_is_handoff_in_progress(mac,
						pSmeJoinRsp->vdev_id)) {
			/* Need to dig more on indicating events to
			 * SME QoS module
			 */
			sme_qos_csr_event_ind(mac, pSmeJoinRsp->vdev_id,
					    SME_QOS_CSR_HANDOFF_COMPLETE, NULL);
			csr_roam_complete(mac, result, pSmeJoinRsp,
					pSmeJoinRsp->vdev_id);
		} else {
			csr_roam_complete(mac, result, NULL,
					pSmeJoinRsp->vdev_id);
		}
	}
	/* Should we handle this similar to handling the join failure? Is it ok
	 * to call csr_roam_complete() with state as CsrJoinFailure
	 */
	else {
		sme_warn(
			"CSR SmeReassocReq failed with status_code= 0x%08X [%d]",
			pSmeJoinRsp->status_code, pSmeJoinRsp->status_code);
		result = eCsrReassocFailure;
		cds_flush_logs(WLAN_LOG_TYPE_FATAL,
			WLAN_LOG_INDICATOR_HOST_DRIVER,
			WLAN_LOG_REASON_ROAM_FAIL,
			false, false);
		if ((eSIR_SME_FT_REASSOC_TIMEOUT_FAILURE ==
		     pSmeJoinRsp->status_code)
		    || (eSIR_SME_FT_REASSOC_FAILURE ==
			pSmeJoinRsp->status_code)
		    || (eSIR_SME_INVALID_PARAMETERS ==
			pSmeJoinRsp->status_code)) {
			/* Inform HDD to turn off FT flag in HDD */
			if (pNeighborRoamInfo) {
				struct csr_roam_info *roam_info;
				uint32_t roam_id = 0;

				roam_info = qdf_mem_malloc(sizeof(*roam_info));
				if (!roam_info)
					return;
				mlme_set_discon_reason_n_from_ap(mac->psoc,
					pSmeJoinRsp->vdev_id, false,
					REASON_HOST_TRIGGERED_ROAM_FAILURE);
				csr_roam_call_callback(mac,
						       pSmeJoinRsp->vdev_id,
						       roam_info, roam_id,
						    eCSR_ROAM_FT_REASSOC_FAILED,
						    eCSR_ROAM_RESULT_SUCCESS);
				/*
				 * Since the above callback sends a disconnect
				 * to HDD, we should clean-up our state
				 * machine as well to be in sync with the upper
				 * layers. There is no need to send a disassoc
				 * since: 1) we will never reassoc to the
				 * current AP in LFR, and 2) there is no need
				 * to issue a disassoc to the AP with which we
				 * were trying to reassoc.
				 */
				csr_roam_complete(mac, eCsrJoinFailure, NULL,
						pSmeJoinRsp->vdev_id);
				qdf_mem_free(roam_info);
				return;
			}
		}
		/* In the event that the Reassociation fails, then we need to
		 * Disassociate the current association and keep roaming. Note
		 * that we will attempt to Join the AP instead of a Reassoc
		 * since we may have attempted a 'Reassoc to self', which AP's
		 * that don't support Reassoc will force a Disassoc. The
		 * isassoc rsp message will remove the command from active list
		 */
		if (!QDF_IS_STATUS_SUCCESS
			    (csr_roam_issue_disassociate
				    (mac, pSmeJoinRsp->vdev_id,
				    eCSR_ROAM_SUBSTATE_DISASSOC_REASSOC_FAILURE,
				false))) {
			csr_roam_complete(mac, eCsrJoinFailure, NULL,
					pSmeJoinRsp->vdev_id);
		}
	}
}
#endif

static void csr_roam_roaming_state_stop_bss_rsp_processor(struct mac_context *mac,
							  tSirSmeRsp *pSmeRsp)
{
	enum csr_roamcomplete_result result_code = eCsrNothingToJoin;
	struct csr_roam_profile *profile;

	mac->roam.roamSession[pSmeRsp->vdev_id].connectState =
		eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	if (CSR_IS_ROAM_SUBSTATE_STOP_BSS_REQ(mac, pSmeRsp->vdev_id)) {
		profile =
		    mac->roam.roamSession[pSmeRsp->vdev_id].pCurRoamProfile;
		if (profile && CSR_IS_CONN_NDI(profile)) {
			result_code = eCsrStopBssSuccess;
			if (pSmeRsp->status_code != eSIR_SME_SUCCESS)
				result_code = eCsrStopBssFailure;
		}
		csr_roam_complete(mac, result_code, NULL, pSmeRsp->vdev_id);
	}
}

#ifndef FEATURE_CM_ENABLE
#ifdef WLAN_FEATURE_HOST_ROAM
/**
 * csr_dequeue_command() - removes a command from active cmd list
 * @mac:          mac global context
 *
 * Return: void
 */
static void
csr_dequeue_command(struct mac_context *mac_ctx)
{
	bool fRemoveCmd;
	tSmeCmd *cmd = NULL;
	tListElem *entry = csr_nonscan_active_ll_peek_head(mac_ctx,
					    LL_ACCESS_LOCK);
	if (!entry) {
		sme_err("NO commands are active");
		return;
	}

	cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
	/*
	 * If the head of the queue is Active and it is a given cmd type, remove
	 * and put this on the Free queue.
	 */
	if (eSmeCommandRoam != cmd->command) {
		sme_err("Roam command not active");
		return;
	}
	/*
	 * we need to process the result first before removing it from active
	 * list because state changes still happening insides
	 * roamQProcessRoamResults so no other roam command should be issued.
	 */
	fRemoveCmd = csr_nonscan_active_ll_remove_entry(mac_ctx, entry,
					 LL_ACCESS_LOCK);
	if (cmd->u.roamCmd.fReleaseProfile) {
		csr_release_profile(mac_ctx, &cmd->u.roamCmd.roamProfile);
		cmd->u.roamCmd.fReleaseProfile = false;
	}
	if (fRemoveCmd)
		csr_release_command(mac_ctx, cmd);
	else
		sme_err("fail to remove cmd reason %d",
			cmd->u.roamCmd.roamReason);
}

/**
 * csr_post_roam_failure() - post roam failure back to csr and issues a disassoc
 * @mac:               mac global context
 * @session_id:         session id
 * @roam_info:          roam info struct
 * @scan_filter:        scan filter to free
 * @cur_roam_profile:   current csr roam profile
 *
 * Return: void
 */
static void
csr_post_roam_failure(struct mac_context *mac_ctx,
		      uint32_t session_id,
		      struct csr_roam_info *roam_info,
		      struct csr_roam_profile *cur_roam_profile)
{
	QDF_STATUS status;

	if (cur_roam_profile)
		qdf_mem_free(cur_roam_profile);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
		csr_roam_synch_clean_up(mac_ctx, session_id);
#endif
	/* Inform the upper layers that the reassoc failed */
	qdf_mem_zero(roam_info, sizeof(struct csr_roam_info));
	csr_roam_call_callback(mac_ctx, session_id, roam_info, 0,
			       eCSR_ROAM_FT_REASSOC_FAILED,
			       eCSR_ROAM_RESULT_SUCCESS);
	/*
	 * Issue a disassoc request so that PE/LIM uses this to clean-up the FT
	 * session. Upon success, we would re-enter this routine after receiving
	 * the disassoc response and will fall into the reassoc fail sub-state.
	 * And, eventually call csr_roam_complete which would remove the roam
	 * command from SME active queue.
	 */
	status = csr_roam_issue_disassociate(mac_ctx, session_id,
			eCSR_ROAM_SUBSTATE_DISASSOC_REASSOC_FAILURE, false);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err(
			"csr_roam_issue_disassociate failed, status %d",
			status);
		csr_roam_complete(mac_ctx, eCsrJoinFailure, NULL, session_id);
	}
}

/**
 * csr_check_profile_in_scan_cache() - finds if roam profile is present in scan
 * cache or not
 * @mac:                  mac global context
 * @neighbor_roam_info:    roam info struct
 * @hBSSList:              scan result
 * @vdev_id: vdev id
 *
 * Return: true if found else false.
 */
static bool
csr_check_profile_in_scan_cache(struct mac_context *mac_ctx,
				tpCsrNeighborRoamControlInfo neighbor_roam_info,
				tScanResultHandle *hBSSList, uint8_t vdev_id)
{
	QDF_STATUS status;
	struct scan_filter *scan_filter;

	scan_filter = qdf_mem_malloc(sizeof(*scan_filter));
	if (!scan_filter)
		return false;

	status = csr_roam_get_scan_filter_from_profile(mac_ctx,
			&neighbor_roam_info->csrNeighborRoamProfile,
			scan_filter, true, vdev_id);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err(
			"failed to prepare scan filter, status %d",
			status);
		qdf_mem_free(scan_filter);
		return false;
	}
	status = csr_scan_get_result(mac_ctx, scan_filter, hBSSList, true);
	qdf_mem_free(scan_filter);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err(
			"csr_scan_get_result failed, status %d",
			status);
		return false;
	}
	return true;
}

static
QDF_STATUS csr_roam_lfr2_issue_connect(struct mac_context *mac,
				uint32_t session_id,
				struct scan_result_list *hbss_list,
				uint32_t roam_id)
{
	struct csr_roam_profile *cur_roam_profile = NULL;
	struct csr_roam_session *session;
	QDF_STATUS status;

	session = CSR_GET_SESSION(mac, session_id);
	if (!session) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"session is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	/*
	 * Copy the connected profile to apply the same for this
	 * connection as well
	 */
	cur_roam_profile = qdf_mem_malloc(sizeof(*cur_roam_profile));
	if (cur_roam_profile) {
		/*
		 * notify sub-modules like QoS etc. that handoff
		 * happening
		 */
		sme_qos_csr_event_ind(mac, session_id,
				      SME_QOS_CSR_HANDOFF_ASSOC_REQ,
				      NULL);
		csr_roam_copy_profile(mac, cur_roam_profile,
				      session->pCurRoamProfile, session_id);
		/* make sure to put it at the head of the cmd queue */
		status = csr_roam_issue_connect(mac, session_id,
				cur_roam_profile, hbss_list,
				eCsrSmeIssuedAssocToSimilarAP,
				roam_id, true, false);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err(
				"issue_connect failed. status %d",
				status);

		csr_release_profile(mac, cur_roam_profile);
		qdf_mem_free(cur_roam_profile);
		return QDF_STATUS_SUCCESS;
	} else {
		sme_err("malloc failed");
		QDF_ASSERT(0);
		csr_dequeue_command(mac);
	}
	return QDF_STATUS_E_NOMEM;
}

QDF_STATUS csr_continue_lfr2_connect(struct mac_context *mac,
				      uint32_t session_id)
{
	uint32_t roam_id = 0;
	struct csr_roam_info *roam_info;
	struct scan_result_list *scan_handle_roam_ap;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	scan_handle_roam_ap =
		mac->roam.neighborRoamInfo[session_id].scan_res_lfr2_roam_ap;
	if (!scan_handle_roam_ap) {
		sme_err("no roam target ap");
		goto POST_ROAM_FAILURE;
	}
	if(is_disconnect_pending(mac, session_id)) {
		sme_err("disconnect pending");
		goto purge_scan_result;
	}

	status = csr_roam_lfr2_issue_connect(mac, session_id,
						scan_handle_roam_ap,
						roam_id);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_free(roam_info);
		return status;
	}

purge_scan_result:
	csr_scan_result_purge(mac, scan_handle_roam_ap);

POST_ROAM_FAILURE:
	csr_post_roam_failure(mac, session_id, roam_info, NULL);
	qdf_mem_free(roam_info);
	return status;
}

static
void csr_handle_disassoc_ho(struct mac_context *mac, uint32_t session_id)
{
	uint32_t roam_id = 0;
	struct csr_roam_info *roam_info;
	struct sCsrNeighborRoamControlInfo *neighbor_roam_info = NULL;
	struct scan_result_list *scan_handle_roam_ap;
	struct sCsrNeighborRoamBSSInfo *bss_node;
	QDF_STATUS status;

	csr_dequeue_command(mac);
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		 "CSR SmeDisassocReq due to HO on session %d", session_id);
	neighbor_roam_info = &mac->roam.neighborRoamInfo[session_id];

	/*
	 * First ensure if the roam profile is in the scan cache.
	 * If not, post a reassoc failure and disconnect.
	 */
	if (!csr_check_profile_in_scan_cache(mac, neighbor_roam_info,
			     (tScanResultHandle *)&scan_handle_roam_ap,
			     session_id))
		goto POST_ROAM_FAILURE;

	/* notify HDD about handoff and provide the BSSID too */
	roam_info->reasonCode = eCsrRoamReasonBetterAP;

	qdf_copy_macaddr(&roam_info->bssid,
		 neighbor_roam_info->csrNeighborRoamProfile.BSSIDs.bssid);

	/*
	 * For LFR2, removal of policy mgr entry for disassociated
	 * AP is handled in eCSR_ROAM_ROAMING_START.
	 * eCSR_ROAM_RESULT_NOT_ASSOCIATED is sent to differentiate
	 * eCSR_ROAM_ROAMING_START sent after FT preauth success
	 */
	csr_roam_call_callback(mac, session_id, roam_info, 0,
			       eCSR_ROAM_ROAMING_START,
			       eCSR_ROAM_RESULT_NOT_ASSOCIATED);

	bss_node = csr_neighbor_roam_next_roamable_ap(mac,
		&neighbor_roam_info->FTRoamInfo.preAuthDoneList,
		NULL);
	if (!bss_node) {
		sme_debug("LFR2DBG: bss_node is NULL");
		goto POST_ROAM_FAILURE;
	}

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  "LFR2DBG: preauthed bss_node->pBssDescription BSSID"\
		  QDF_MAC_ADDR_FMT",freq:%d",
		  QDF_MAC_ADDR_REF(bss_node->pBssDescription->bssId),
		  bss_node->pBssDescription->chan_freq);

	status = policy_mgr_handle_conc_multiport(
			mac->psoc, session_id,
			bss_node->pBssDescription->chan_freq,
			POLICY_MGR_UPDATE_REASON_LFR2_ROAM,
			POLICY_MGR_DEF_REQ_ID);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->roam.neighborRoamInfo[session_id].scan_res_lfr2_roam_ap =
							scan_handle_roam_ap;
		/*if hw_mode change is required then handle roam
		* issue connect in mode change response handler
		*/
		qdf_mem_free(roam_info);
		return;
	} else if (status == QDF_STATUS_E_FAILURE)
		goto POST_ROAM_FAILURE;

	status = csr_roam_lfr2_issue_connect(mac, session_id,
					     scan_handle_roam_ap,
					     roam_id);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_free(roam_info);
		return;
	}
	csr_scan_result_purge(mac, scan_handle_roam_ap);

POST_ROAM_FAILURE:
	mlme_set_discon_reason_n_from_ap(mac->psoc, session_id, false,
					 REASON_HOST_TRIGGERED_ROAM_FAILURE);
	csr_post_roam_failure(mac, session_id, roam_info, NULL);
	qdf_mem_free(roam_info);
}

#else
static
void csr_handle_disassoc_ho(struct mac_context *mac, uint32_t session_id)
{
	return;
}
#endif
#endif

static
void csr_roam_roaming_state_disassoc_rsp_processor(struct mac_context *mac,
						   struct disassoc_rsp *rsp)
{
	uint32_t sessionId;
	struct csr_roam_session *pSession;

	sessionId = rsp->sessionId;
	sme_debug("sessionId %d", sessionId);

	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	if (csr_is_conn_state_infra(mac, sessionId)) {
		mac->roam.roamSession[sessionId].connectState =
			eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	}
#endif
	pSession = CSR_GET_SESSION(mac, sessionId);
	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return;
	}

	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	if (CSR_IS_ROAM_SUBSTATE_DISASSOC_NO_JOIN(mac, sessionId)) {
		sme_debug("***eCsrNothingToJoin***");
		csr_roam_complete(mac, eCsrNothingToJoin, NULL, sessionId);
	} else if (CSR_IS_ROAM_SUBSTATE_DISASSOC_FORCED(mac, sessionId) ||
		   CSR_IS_ROAM_SUBSTATE_DISASSOC_REQ(mac, sessionId)) {
		if (eSIR_SME_SUCCESS == rsp->status_code) {
			sme_debug("CSR force disassociated successful");
			/*
			 * A callback to HDD will be issued from
			 * csr_roam_complete so no need to do anything here
			 */
		}
		csr_roam_complete(mac, eCsrNothingToJoin, NULL, sessionId);
	} else if (CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(mac, sessionId)) {
		csr_handle_disassoc_ho(mac, sessionId);
	} /* else if ( CSR_IS_ROAM_SUBSTATE_DISASSOC_HO( mac ) ) */
	else if (CSR_IS_ROAM_SUBSTATE_REASSOC_FAIL(mac, sessionId)) {
		/* Disassoc due to Reassoc failure falls into this codepath */
		csr_roam_complete(mac, eCsrJoinFailure, NULL, sessionId);
	} else {
		if (eSIR_SME_SUCCESS == rsp->status_code) {
			/*
			 * Successfully disassociated from the 'old' Bss.
			 * We get Disassociate response in three conditions.
			 * Where we are doing an Infra to Infra roam between
			 *    networks with different SSIDs.
			 * In all cases, we set the new Bss configuration here
			 * and attempt to join
			 */
			sme_debug("Disassociated successfully");
		} else {
			sme_err("DisassocReq failed, status_code= 0x%08X",
				rsp->status_code);
		}
		/* We are not done yet. Get the data and continue roaming */
		csr_roam_reissue_roam_command(mac, sessionId);
	}
#else
	if (eSIR_SME_SUCCESS == rsp->status_code) {
		sme_debug("CSR force disassociated successful");
		/*
		 * A callback to HDD will be issued from
		 * csr_roam_complete so no need to do anything here
		 */
	}
	csr_roam_complete(mac, eCsrNothingToJoin, NULL, sessionId);
#endif
}

static void csr_roam_roaming_state_deauth_rsp_processor(struct mac_context *mac,
						struct deauth_rsp *pSmeRsp)
{
	tSirResultCodes status_code;
	status_code = csr_get_de_auth_rsp_status_code(pSmeRsp);
	mac->roam.deauthRspStatus = status_code;
	csr_roam_complete(mac, eCsrNothingToJoin, NULL, pSmeRsp->sessionId);
}

static void
csr_roam_roaming_state_start_bss_rsp_processor(struct mac_context *mac,
					struct start_bss_rsp *pSmeStartBssRsp)
{
	enum csr_roamcomplete_result result;

	if (eSIR_SME_SUCCESS == pSmeStartBssRsp->status_code) {
		sme_debug("SmeStartBssReq Successful");
		result = eCsrStartBssSuccess;
	} else {
		sme_warn("SmeStartBssReq failed with status_code= 0x%08X",
			 pSmeStartBssRsp->status_code);
		/* Let csr_roam_complete decide what to do */
		result = eCsrStartBssFailure;
	}
	csr_roam_complete(mac, result, pSmeStartBssRsp,
				pSmeStartBssRsp->sessionId);
}

/**
 * csr_roam_send_disconnect_done_indication() - Send disconnect ind to HDD.
 *
 * @mac_ctx: mac global context
 * @msg_ptr: incoming message
 *
 * This function gives final disconnect event to HDD after all cleanup in
 * lower layers is done.
 *
 * Return: None
 */
static void
csr_roam_send_disconnect_done_indication(struct mac_context *mac_ctx,
					 tSirSmeRsp *msg_ptr)
{
	struct sir_sme_discon_done_ind *discon_ind =
				(struct sir_sme_discon_done_ind *)(msg_ptr);
	struct csr_roam_info *roam_info;
	struct csr_roam_session *session;
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	struct wlan_objmgr_vdev *vdev;
#endif
	uint8_t vdev_id;

	vdev_id = discon_ind->session_id;
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;

	sme_debug("DISCONNECT_DONE_IND RC:%d", discon_ind->reason_code);

	if (CSR_IS_SESSION_VALID(mac_ctx, vdev_id)) {
		roam_info->reasonCode = discon_ind->reason_code;
		roam_info->status_code = eSIR_SME_STA_NOT_ASSOCIATED;
		qdf_mem_copy(roam_info->peerMac.bytes, discon_ind->peer_mac,
			     ETH_ALEN);

		roam_info->rssi = mac_ctx->peer_rssi;
		roam_info->tx_rate = mac_ctx->peer_txrate;
		roam_info->rx_rate = mac_ctx->peer_rxrate;
		roam_info->disassoc_reason = discon_ind->reason_code;
		roam_info->rx_mc_bc_cnt = mac_ctx->rx_mc_bc_cnt;
		roam_info->rx_retry_cnt = mac_ctx->rx_retry_cnt;
		/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
							    vdev_id,
							    WLAN_LEGACY_SME_ID);
		if (vdev)
			roam_info->disconnect_ies =
				mlme_get_peer_disconnect_ies(vdev);

		csr_roam_call_callback(mac_ctx, vdev_id,
				       roam_info, 0, eCSR_ROAM_LOSTLINK,
				       eCSR_ROAM_RESULT_DISASSOC_IND);
		if (vdev) {
			mlme_free_peer_disconnect_ies(vdev);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		}
#else
		csr_roam_call_callback(mac_ctx, vdev_id,
				       roam_info, 0, eCSR_ROAM_LOSTLINK,
				       eCSR_ROAM_RESULT_DISASSOC_IND);
#endif
		session = CSR_GET_SESSION(mac_ctx, vdev_id);
		if (!CSR_IS_INFRA_AP(&session->connectedProfile))
			csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
					      vdev_id);
		/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
		if (CSR_IS_INFRASTRUCTURE(&session->connectedProfile)) {
			csr_free_roam_profile(mac_ctx, vdev_id);
			csr_free_connect_bss_desc(mac_ctx, vdev_id);
			csr_roam_free_connect_profile(
						&session->connectedProfile);
			csr_roam_free_connected_info(mac_ctx,
						     &session->connectedInfo);
		}
#endif

	} else {
		sme_err("Inactive vdev_id %d", vdev_id);
	}

	/*
	 * Release WM status change command as eWNI_SME_DISCONNECT_DONE_IND
	 * has been sent to HDD and there is nothing else left to do.
	 */
	csr_roam_wm_status_change_complete(mac_ctx, vdev_id);
	qdf_mem_free(roam_info);
}

/**
 * csr_roaming_state_msg_processor() - process roaming messages
 * @mac:       mac global context
 * @msg_buf:    message buffer
 *
 * We need to be careful on whether to cast msg_buf (pSmeRsp) to other type of
 * strucutres. It depends on how the message is constructed. If the message is
 * sent by lim_send_sme_rsp, the msg_buf is only a generic response and can only
 * be used as pointer to tSirSmeRsp. For the messages where sender allocates
 * memory for specific structures, then it can be cast accordingly.
 *
 * Return: status of operation
 */
void csr_roaming_state_msg_processor(struct mac_context *mac, void *msg_buf)
{
	tSirSmeRsp *pSmeRsp;

	pSmeRsp = (tSirSmeRsp *)msg_buf;

	switch (pSmeRsp->messageType) {
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	case eWNI_SME_JOIN_RSP:
		/* in Roaming state, process the Join response message... */
		if (CSR_IS_ROAM_SUBSTATE_JOIN_REQ(mac, pSmeRsp->vdev_id))
			/* We sent a JOIN_REQ */
			csr_roam_join_rsp_processor(mac,
						    (struct join_rsp *)pSmeRsp);
		break;
	case eWNI_SME_REASSOC_RSP:
		/* or the Reassociation response message... */
		if (CSR_IS_ROAM_SUBSTATE_REASSOC_REQ(mac, pSmeRsp->vdev_id))
			csr_roam_roaming_state_reassoc_rsp_processor(mac,
						(struct join_rsp *)pSmeRsp);
		break;
#endif
	case eWNI_SME_STOP_BSS_RSP:
		/* or the Stop Bss response message... */
		csr_roam_roaming_state_stop_bss_rsp_processor(mac, pSmeRsp);
		break;
	case eWNI_SME_DISASSOC_RSP:
		/* or the Disassociate response message... */
		if (CSR_IS_ROAM_SUBSTATE_DISASSOC_REQ(mac, pSmeRsp->vdev_id)
		/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE

		    || CSR_IS_ROAM_SUBSTATE_DISASSOC_NO_JOIN(mac,
							pSmeRsp->vdev_id)
		    || CSR_IS_ROAM_SUBSTATE_REASSOC_FAIL(mac,
							pSmeRsp->vdev_id)
		    || CSR_IS_ROAM_SUBSTATE_DISASSOC_FORCED(mac,
							pSmeRsp->vdev_id)
		    || CSR_IS_ROAM_SUBSTATE_DISCONNECT_CONTINUE(mac,
							pSmeRsp->vdev_id)
		    || CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(mac,
							pSmeRsp->vdev_id)
#endif
		    ) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				 "eWNI_SME_DISASSOC_RSP subState = %s",
				  mac_trace_getcsr_roam_sub_state(
				  mac->roam.curSubState[pSmeRsp->vdev_id]));
			csr_roam_roaming_state_disassoc_rsp_processor(mac,
						(struct disassoc_rsp *) pSmeRsp);
		}
		break;
	case eWNI_SME_DEAUTH_RSP:
		/* or the Deauthentication response message... */
		if (CSR_IS_ROAM_SUBSTATE_DEAUTH_REQ(mac, pSmeRsp->vdev_id))
			csr_roam_roaming_state_deauth_rsp_processor(mac,
						(struct deauth_rsp *) pSmeRsp);
		break;
	case eWNI_SME_START_BSS_RSP:
		/* or the Start BSS response message... */
		if (CSR_IS_ROAM_SUBSTATE_START_BSS_REQ(mac,
						       pSmeRsp->vdev_id))
			csr_roam_roaming_state_start_bss_rsp_processor(mac,
					(struct start_bss_rsp *)pSmeRsp);
		break;
	/* In case CSR issues STOP_BSS, we need to tell HDD about peer departed
	 * because PE is removing them
	 */
	case eWNI_SME_TRIGGER_SAE:
		sme_debug("Invoke SAE callback");
		csr_sae_callback(mac, pSmeRsp);
		break;

	case eWNI_SME_SETCONTEXT_RSP:
		csr_roam_check_for_link_status_change(mac, pSmeRsp);
		break;

	case eWNI_SME_DISCONNECT_DONE_IND:
		csr_roam_send_disconnect_done_indication(mac, pSmeRsp);
		break;

	case eWNI_SME_UPPER_LAYER_ASSOC_CNF:
		csr_roam_joined_state_msg_processor(mac, pSmeRsp);
		break;
	default:
		sme_debug("Unexpected message type: %d[0x%X] received in substate %s",
			pSmeRsp->messageType, pSmeRsp->messageType,
			mac_trace_getcsr_roam_sub_state(
				mac->roam.curSubState[pSmeRsp->vdev_id]));
		/* If we are connected, check the link status change */
		if (!csr_is_conn_state_disconnected(mac, pSmeRsp->vdev_id))
			csr_roam_check_for_link_status_change(mac, pSmeRsp);
		break;
	}
}

void csr_roam_joined_state_msg_processor(struct mac_context *mac, void *msg_buf)
{
	tSirSmeRsp *pSirMsg = (tSirSmeRsp *)msg_buf;

	switch (pSirMsg->messageType) {
	case eWNI_SME_UPPER_LAYER_ASSOC_CNF:
	{
		struct csr_roam_session *pSession;
		tSirSmeAssocIndToUpperLayerCnf *pUpperLayerAssocCnf;
		struct csr_roam_info *roam_info;
		uint32_t sessionId;
		QDF_STATUS status;

		sme_debug("ASSOCIATION confirmation can be given to upper layer ");
		pUpperLayerAssocCnf =
			(tSirSmeAssocIndToUpperLayerCnf *)msg_buf;
		status = csr_roam_get_session_id_from_bssid(mac,
							(struct qdf_mac_addr *)
							   pUpperLayerAssocCnf->
							   bssId, &sessionId);
		pSession = CSR_GET_SESSION(mac, sessionId);

		if (!pSession) {
			sme_err("session %d not found", sessionId);
			if (pUpperLayerAssocCnf->ies)
				qdf_mem_free(pUpperLayerAssocCnf->ies);
			return;
		}

		roam_info = qdf_mem_malloc(sizeof(*roam_info));
		if (!roam_info) {
			if (pUpperLayerAssocCnf->ies)
				qdf_mem_free(pUpperLayerAssocCnf->ies);
			return;
		}
		/* send the status code as Success */
		roam_info->status_code = eSIR_SME_SUCCESS;
		roam_info->u.pConnectedProfile =
			&pSession->connectedProfile;
		roam_info->staId = (uint8_t) pUpperLayerAssocCnf->aid;
		roam_info->rsnIELen =
			(uint8_t) pUpperLayerAssocCnf->rsnIE.length;
		roam_info->prsnIE =
			pUpperLayerAssocCnf->rsnIE.rsnIEdata;
#ifdef FEATURE_WLAN_WAPI
		roam_info->wapiIELen =
			(uint8_t) pUpperLayerAssocCnf->wapiIE.length;
		roam_info->pwapiIE =
			pUpperLayerAssocCnf->wapiIE.wapiIEdata;
#endif
		roam_info->addIELen =
			(uint8_t) pUpperLayerAssocCnf->addIE.length;
		roam_info->paddIE =
			pUpperLayerAssocCnf->addIE.addIEdata;
		qdf_mem_copy(roam_info->peerMac.bytes,
			     pUpperLayerAssocCnf->peerMacAddr,
			     sizeof(tSirMacAddr));
		qdf_mem_copy(&roam_info->bssid,
			     pUpperLayerAssocCnf->bssId,
			     sizeof(struct qdf_mac_addr));
		roam_info->wmmEnabledSta =
			pUpperLayerAssocCnf->wmmEnabledSta;
		roam_info->timingMeasCap =
			pUpperLayerAssocCnf->timingMeasCap;
		qdf_mem_copy(&roam_info->chan_info,
			     &pUpperLayerAssocCnf->chan_info,
			     sizeof(struct oem_channel_info));

		roam_info->ampdu = pUpperLayerAssocCnf->ampdu;
		roam_info->sgi_enable = pUpperLayerAssocCnf->sgi_enable;
		roam_info->tx_stbc = pUpperLayerAssocCnf->tx_stbc;
		roam_info->rx_stbc = pUpperLayerAssocCnf->rx_stbc;
		roam_info->ch_width = pUpperLayerAssocCnf->ch_width;
		roam_info->mode = pUpperLayerAssocCnf->mode;
		roam_info->max_supp_idx = pUpperLayerAssocCnf->max_supp_idx;
		roam_info->max_ext_idx = pUpperLayerAssocCnf->max_ext_idx;
		roam_info->max_mcs_idx = pUpperLayerAssocCnf->max_mcs_idx;
		roam_info->rx_mcs_map = pUpperLayerAssocCnf->rx_mcs_map;
		roam_info->tx_mcs_map = pUpperLayerAssocCnf->tx_mcs_map;
		roam_info->ecsa_capable = pUpperLayerAssocCnf->ecsa_capable;
		if (pUpperLayerAssocCnf->ht_caps.present)
			roam_info->ht_caps = pUpperLayerAssocCnf->ht_caps;
		if (pUpperLayerAssocCnf->vht_caps.present)
			roam_info->vht_caps = pUpperLayerAssocCnf->vht_caps;
		roam_info->capability_info =
					pUpperLayerAssocCnf->capability_info;
		roam_info->he_caps_present =
					pUpperLayerAssocCnf->he_caps_present;

		if (CSR_IS_INFRA_AP(roam_info->u.pConnectedProfile)) {
			if (pUpperLayerAssocCnf->ies_len > 0) {
				roam_info->assocReqLength =
						pUpperLayerAssocCnf->ies_len;
				roam_info->assocReqPtr =
						pUpperLayerAssocCnf->ies;
			}

			mac->roam.roamSession[sessionId].connectState =
				eCSR_ASSOC_STATE_TYPE_INFRA_CONNECTED;
			roam_info->fReassocReq =
				pUpperLayerAssocCnf->reassocReq;
			status = csr_roam_call_callback(mac, sessionId,
						       roam_info, 0,
						       eCSR_ROAM_INFRA_IND,
					eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF);
		}
		if (pUpperLayerAssocCnf->ies)
			qdf_mem_free(pUpperLayerAssocCnf->ies);
		qdf_mem_free(roam_info);
	}
	break;
	default:
		csr_roam_check_for_link_status_change(mac, pSirMsg);
		break;
	}
}

/**
 * csr_update_wep_key_peer_macaddr() - Update wep key peer mac addr
 * @vdev: vdev object
 * @crypto_key: crypto key info
 * @unicast: uncast or broadcast
 * @mac_addr: peer mac address
 *
 * Update peer mac address to key context before set wep key to target.
 *
 * Return void
 */
static void
csr_update_wep_key_peer_macaddr(struct wlan_objmgr_vdev *vdev,
				struct wlan_crypto_key *crypto_key,
				bool unicast,
				struct qdf_mac_addr *mac_addr)
{
	if (!crypto_key || !vdev) {
		sme_err("vdev or crytpo_key null");
		return;
	}

	if (unicast) {
		qdf_mem_copy(&crypto_key->macaddr, mac_addr,
			     QDF_MAC_ADDR_SIZE);
	} else {
		if (vdev->vdev_mlme.vdev_opmode == QDF_STA_MODE ||
		    vdev->vdev_mlme.vdev_opmode == QDF_P2P_CLIENT_MODE)
			qdf_mem_copy(&crypto_key->macaddr, mac_addr,
				     QDF_MAC_ADDR_SIZE);
		else
			qdf_mem_copy(&crypto_key->macaddr,
				     vdev->vdev_mlme.macaddr,
				     QDF_MAC_ADDR_SIZE);
	}
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static void
csr_roam_diag_set_ctx_rsp(struct mac_context *mac_ctx,
			  struct csr_roam_session *session,
			  struct set_context_rsp *pRsp)
{
	WLAN_HOST_DIAG_EVENT_DEF(setKeyEvent,
				 host_event_wlan_security_payload_type);
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(mac_ctx->pdev,
						    session->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev)
		return;
	if (cm_is_open_mode(vdev)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	qdf_mem_zero(&setKeyEvent,
		     sizeof(host_event_wlan_security_payload_type));
	if (qdf_is_macaddr_group(&pRsp->peer_macaddr))
		setKeyEvent.eventId =
			WLAN_SECURITY_EVENT_SET_BCAST_RSP;
	else
		setKeyEvent.eventId =
			WLAN_SECURITY_EVENT_SET_UNICAST_RSP;
	cm_diag_get_auth_enc_type_vdev_id(mac_ctx->psoc,
					  &setKeyEvent.authMode,
					  &setKeyEvent.encryptionModeUnicast,
					  &setKeyEvent.encryptionModeMulticast,
					  session->vdev_id);
	wlan_mlme_get_bssid_vdev_id(mac_ctx->pdev, session->vdev_id,
				    (struct qdf_mac_addr *)&setKeyEvent.bssid);
	if (eSIR_SME_SUCCESS != pRsp->status_code)
		setKeyEvent.status = WLAN_SECURITY_STATUS_FAILURE;
	WLAN_HOST_DIAG_EVENT_REPORT(&setKeyEvent, EVENT_WLAN_SECURITY);
}
#else /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
static void
csr_roam_diag_set_ctx_rsp(struct mac_context *mac_ctx,
			  struct csr_roam_session *session,
			  struct set_context_rsp *pRsp)
{
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

static void
csr_roam_chk_lnk_set_ctx_rsp(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	qdf_freq_t chan_freq;
	struct csr_roam_info *roam_info;
	eCsrRoamResult result = eCSR_ROAM_RESULT_NONE;
	struct set_context_rsp *pRsp = (struct set_context_rsp *)msg_ptr;
	struct qdf_mac_addr connected_bssid;

	if (!pRsp) {
		sme_err("set key response is NULL");
		return;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sessionId = pRsp->sessionId;
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		qdf_mem_free(roam_info);
		return;
	}

	csr_roam_diag_set_ctx_rsp(mac_ctx, session, pRsp);
	chan_freq = wlan_get_operation_chan_freq_vdev_id(mac_ctx->pdev,
							 sessionId);
	wlan_mlme_get_bssid_vdev_id(mac_ctx->pdev, sessionId,
				    &connected_bssid);

	if (CSR_IS_WAIT_FOR_KEY(mac_ctx, sessionId)) {
		/* We are done with authentication, whethere succeed or not */
		csr_roam_substate_change(mac_ctx, eCSR_ROAM_SUBSTATE_NONE,
					 sessionId);
		/* This is temp ifdef will be removed in near future */
		cm_stop_wait_for_key_timer(mac_ctx->psoc, sessionId);

#ifndef FEATURE_CM_ENABLE
		/* We do it here because this linkup function is not called
		 * after association  when a key needs to be set.
		 */
		if (csr_is_conn_state_connected_infra(mac_ctx, sessionId))
			csr_roam_link_up(mac_ctx, connected_bssid);
#else
		cm_roam_start_init_on_connect(mac_ctx->pdev, sessionId);
#endif
	}
	if (eSIR_SME_SUCCESS == pRsp->status_code) {
		qdf_copy_macaddr(&roam_info->peerMac, &pRsp->peer_macaddr);
		/* Make sure we install the GTK before indicating to HDD as
		 * authenticated. This is to prevent broadcast packets go out
		 * after PTK and before GTK.
		 */
		if (qdf_is_macaddr_broadcast(&pRsp->peer_macaddr)) {
			/*
			 * OBSS SCAN Indication will be sent to Firmware
			 * to start OBSS Scan
			 */
			if (mac_ctx->obss_scan_offload &&
			    wlan_reg_is_24ghz_ch_freq(chan_freq) &&
			/* This is temp ifdef will be removed in near future */
#ifdef FEATURE_CM_ENABLE
			    cm_is_vdevid_connected(mac_ctx->pdev, sessionId)
#else
			    (session->connectState ==
			     eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED) &&
			     session->pCurRoamProfile &&
			    (QDF_P2P_CLIENT_MODE ==
			     session->pCurRoamProfile->csrPersona ||
			    (QDF_STA_MODE ==
			     session->pCurRoamProfile->csrPersona))
#endif
			) {
				struct sme_obss_ht40_scanind_msg *msg;

				msg = qdf_mem_malloc(sizeof(
					struct sme_obss_ht40_scanind_msg));
				if (!msg) {
					qdf_mem_free(roam_info);
					return;
				}

				msg->msg_type = eWNI_SME_HT40_OBSS_SCAN_IND;
				msg->length =
				      sizeof(struct sme_obss_ht40_scanind_msg);
				qdf_copy_macaddr(&msg->mac_addr,
					&connected_bssid);
				status = umac_send_mb_message_to_mac(msg);
			}
			result = eCSR_ROAM_RESULT_AUTHENTICATED;
		} else {
			result = eCSR_ROAM_RESULT_NONE;
		}
	} else {
		result = eCSR_ROAM_RESULT_FAILURE;
		sme_err(
			"CSR: setkey command failed(err=%d) PeerMac "
			QDF_MAC_ADDR_FMT,
			pRsp->status_code,
			QDF_MAC_ADDR_REF(pRsp->peer_macaddr.bytes));
	}
	/* keeping roam_id = 0 as nobody is using roam_id for set_key */
	csr_roam_call_callback(mac_ctx, sessionId, roam_info,
			       0, eCSR_ROAM_SET_KEY_COMPLETE, result);
	/* Indicate SME_QOS that the SET_KEY is completed, so that SME_QOS
	 * can go ahead and initiate the TSPEC if any are pending
	 */
	sme_qos_csr_event_ind(mac_ctx, (uint8_t)sessionId,
			      SME_QOS_CSR_SET_KEY_SUCCESS_IND, NULL);
#ifdef FEATURE_WLAN_ESE
	/* Send Adjacent AP repot to new AP. */
	if (result == eCSR_ROAM_RESULT_AUTHENTICATED &&
	    session->isPrevApInfoValid &&
	    wlan_cm_get_ese_assoc(mac_ctx->pdev, sessionId)) {
		csr_send_ese_adjacent_ap_rep_ind(mac_ctx, session);
		session->isPrevApInfoValid = false;
	}
#endif
	qdf_mem_free(roam_info);
}

static QDF_STATUS csr_roam_issue_set_context_req(struct mac_context *mac_ctx,
						 uint32_t session_id,
						 bool add_key, bool unicast,
						 uint8_t key_idx,
						 struct qdf_mac_addr *mac_addr)
{
	enum wlan_crypto_cipher_type cipher;
	struct wlan_crypto_key *crypto_key;
	uint8_t wep_key_idx = 0;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, session_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("VDEV object not found for session_id %d", session_id);
		return QDF_STATUS_E_INVAL;
	}
	cipher = wlan_crypto_get_cipher(vdev, unicast, key_idx);
	if (IS_WEP_CIPHER(cipher)) {
		wep_key_idx = wlan_crypto_get_default_key_idx(vdev, !unicast);
		crypto_key = wlan_crypto_get_key(vdev, wep_key_idx);
		csr_update_wep_key_peer_macaddr(vdev, crypto_key, unicast,
						mac_addr);
	} else {
		crypto_key = wlan_crypto_get_key(vdev, key_idx);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

	sme_debug("session:%d, cipher:%d, ucast:%d, idx:%d, wep:%d, add:%d",
		  session_id, cipher, unicast, key_idx, wep_key_idx, add_key);
	if (!IS_WEP_CIPHER(cipher) && !add_key)
		return QDF_STATUS_E_INVAL;

	return ucfg_crypto_set_key_req(vdev, crypto_key, (unicast ?
				       WLAN_CRYPTO_KEY_TYPE_UNICAST :
				       WLAN_CRYPTO_KEY_TYPE_GROUP));
}

#ifndef FEATURE_CM_ENABLE
static enum wlan_crypto_cipher_type
csr_encr_to_cipher_type(eCsrEncryptionType encr_type)
{
	switch (encr_type) {
	case eCSR_ENCRYPT_TYPE_WEP40:
		return WLAN_CRYPTO_CIPHER_WEP_40;
	case eCSR_ENCRYPT_TYPE_WEP104:
		return WLAN_CRYPTO_CIPHER_WEP_104;
	case eCSR_ENCRYPT_TYPE_TKIP:
		return WLAN_CRYPTO_CIPHER_TKIP;
	case eCSR_ENCRYPT_TYPE_AES:
		return WLAN_CRYPTO_CIPHER_AES_CCM;
	case eCSR_ENCRYPT_TYPE_AES_CMAC:
		return WLAN_CRYPTO_CIPHER_AES_CMAC;
	case eCSR_ENCRYPT_TYPE_AES_GMAC_128:
		return WLAN_CRYPTO_CIPHER_AES_GMAC;
	case eCSR_ENCRYPT_TYPE_AES_GMAC_256:
		return WLAN_CRYPTO_CIPHER_AES_GMAC_256;
	case eCSR_ENCRYPT_TYPE_AES_GCMP:
		return WLAN_CRYPTO_CIPHER_AES_GCM;
	case eCSR_ENCRYPT_TYPE_AES_GCMP_256:
		return WLAN_CRYPTO_CIPHER_AES_GCM_256;
	default:
		return WLAN_CRYPTO_CIPHER_NONE;
	}
}

#ifdef WLAN_FEATURE_FILS_SK
static QDF_STATUS
csr_roam_store_fils_key(struct wlan_objmgr_vdev *vdev,
			bool unicast, uint8_t key_id,
			uint16_t key_length, uint8_t *key,
			tSirMacAddr *bssid,
			eCsrEncryptionType encr_type)
{
	struct wlan_crypto_key *crypto_key = NULL;
	QDF_STATUS status;
	/*
	 * key index is the FW key index.
	 * Key_id is for host crypto component key storage index
	 */
	uint8_t key_index = 0;

	if (unicast)
		key_index = 0;
	else
		key_index = 2;

	crypto_key = wlan_crypto_get_key(vdev, key_id);
	if (!crypto_key) {
		crypto_key = qdf_mem_malloc(sizeof(*crypto_key));
		if (!crypto_key)
			return QDF_STATUS_E_INVAL;

		status = wlan_crypto_save_key(vdev, key_id, crypto_key);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("Failed to save key");
			qdf_mem_free(crypto_key);
			return QDF_STATUS_E_INVAL;
		}
	}
	qdf_mem_zero(crypto_key, sizeof(*crypto_key));
	/* TODO add support for FILS cipher translation in OSIF */
	crypto_key->cipher_type = csr_encr_to_cipher_type(encr_type);
	crypto_key->keylen = key_length;
	crypto_key->keyix = key_index;
	sme_debug("key_len %d, unicast %d", key_length, unicast);
	qdf_mem_copy(&crypto_key->keyval[0], key, key_length);
	qdf_mem_copy(crypto_key->macaddr, bssid, QDF_MAC_ADDR_SIZE);

	return QDF_STATUS_SUCCESS;
}
#else
static inline
QDF_STATUS csr_roam_store_fils_key(struct wlan_objmgr_vdev *vdev,
				   bool unicast, uint8_t key_id,
				   uint16_t key_length, uint8_t *key,
				   tSirMacAddr *bssid,
				   eCsrEncryptionType encr_type)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif
#endif

QDF_STATUS
csr_issue_set_context_req_helper(struct mac_context *mac_ctx,
				 struct csr_roam_profile *profile,
				 uint32_t session_id, tSirMacAddr *bssid,
				 bool addkey, bool unicast,
				 tAniKeyDirection key_direction, uint8_t key_id,
				 uint16_t key_length, uint8_t *key)
{
#ifndef FEATURE_CM_ENABLE
	enum wlan_crypto_cipher_type cipher;
	struct wlan_objmgr_vdev *vdev;
	struct set_context_rsp install_key_rsp;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, session_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("VDEV object not found for session_id %d", session_id);
		return QDF_STATUS_E_INVAL;
	}

	cipher = wlan_crypto_get_cipher(vdev, unicast, key_id);
	if (addkey && !IS_WEP_CIPHER(cipher) &&
	    (profile && csr_is_fils_connection(profile)))
		csr_roam_store_fils_key(vdev, unicast, key_id, key_length,
					key, bssid,
					profile->negotiatedMCEncryptionType);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
	/*
	 * For open mode authentication, send dummy install key response to
	 * send OBSS scan and QOS event.
	 */
	if (profile &&
	    profile->negotiatedUCEncryptionType == eCSR_ENCRYPT_TYPE_NONE &&
	    !unicast) {
		install_key_rsp.length = sizeof(install_key_rsp);
		install_key_rsp.status_code = eSIR_SME_SUCCESS;
		install_key_rsp.sessionId = session_id;
		qdf_mem_copy(install_key_rsp.peer_macaddr.bytes, bssid,
			     QDF_MAC_ADDR_SIZE);

		csr_roam_chk_lnk_set_ctx_rsp(mac_ctx,
					     (tSirSmeRsp *)&install_key_rsp);

		return QDF_STATUS_SUCCESS;
	}
#endif
	return csr_roam_issue_set_context_req(mac_ctx, session_id, addkey,
					      unicast, key_id,
					      (struct qdf_mac_addr *)bssid);
}

#ifndef FEATURE_CM_ENABLE
#ifdef WLAN_FEATURE_FILS_SK
/*
 * csr_create_fils_realm_hash: API to create hash using realm
 * @fils_con_info: fils connection info obtained from supplicant
 * @tmp_hash: pointer to new hash
 *
 * Return: None
 */
static bool
csr_create_fils_realm_hash(struct wlan_fils_connection_info *fils_con_info,
			   uint8_t *tmp_hash)
{
	uint8_t *hash;
	uint8_t *data[1];

	if (!fils_con_info->realm_len)
		return false;

	hash = qdf_mem_malloc(SHA256_DIGEST_SIZE);
	if (!hash)
		return false;

	data[0] = fils_con_info->realm;
	qdf_get_hash(SHA256_CRYPTO_TYPE, 1, data,
			&fils_con_info->realm_len, hash);
	qdf_trace_hex_dump(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_DEBUG,
				   hash, SHA256_DIGEST_SIZE);
	qdf_mem_copy(tmp_hash, hash, 2);
	qdf_mem_free(hash);
	return true;
}

static void csr_update_fils_scan_filter(struct scan_filter *filter,
					struct csr_roam_profile *profile)
{
	if (profile->fils_con_info &&
	    profile->fils_con_info->is_fils_connection) {
		uint8_t realm_hash[2];

		sme_debug("creating realm based on fils info %d",
			profile->fils_con_info->is_fils_connection);
		filter->fils_scan_filter.realm_check =
			csr_create_fils_realm_hash(profile->fils_con_info,
						   realm_hash);
		if (filter->fils_scan_filter.realm_check)
			qdf_mem_copy(filter->fils_scan_filter.fils_realm,
				     realm_hash, REALM_HASH_LEN);
	}
}

#else
static void csr_update_fils_scan_filter(struct scan_filter *filter,
					struct csr_roam_profile *profile)
{ }
#endif

void csr_copy_ssids_from_roam_params(struct rso_config_params *rso_usr_cfg,
				     struct scan_filter *filter)
{
	uint8_t i;
	uint8_t max_ssid;

	if (!rso_usr_cfg->num_ssid_allowed_list)
		return;
	max_ssid = QDF_MIN(WLAN_SCAN_FILTER_NUM_SSID, MAX_SSID_ALLOWED_LIST);

	filter->num_of_ssid = rso_usr_cfg->num_ssid_allowed_list;
	if (filter->num_of_ssid > max_ssid)
		filter->num_of_ssid = max_ssid;
	for  (i = 0; i < filter->num_of_ssid; i++)
		qdf_mem_copy(&filter->ssid_list[i],
			     &rso_usr_cfg->ssid_allowed_list[i],
			     sizeof(struct wlan_ssid));
}

void csr_update_scan_filter_dot11mode(struct mac_context *mac_ctx,
				      struct scan_filter *filter)
{
	eCsrPhyMode phymode = mac_ctx->roam.configParam.phyMode;

	if (phymode == eCSR_DOT11_MODE_11n_ONLY)
		filter->dot11mode = ALLOW_11N_ONLY;
	else if (phymode == eCSR_DOT11_MODE_11ac_ONLY)
		filter->dot11mode = ALLOW_11AC_ONLY;
	else if (phymode == eCSR_DOT11_MODE_11ax_ONLY)
		filter->dot11mode = ALLOW_11AX_ONLY;
}

static inline void csr_copy_ssids(struct wlan_ssid *ssid, tSirMacSSid *from)
{
	ssid->length = from->length;
	if (ssid->length > WLAN_SSID_MAX_LEN)
		ssid->length = WLAN_SSID_MAX_LEN;
	qdf_mem_copy(ssid->ssid, from->ssId, ssid->length);
}

static void csr_copy_ssids_from_profile(tCsrSSIDs *ssid_list,
					struct scan_filter *filter)
{
	uint8_t i;

	filter->num_of_ssid = ssid_list->numOfSSIDs;
	if (filter->num_of_ssid > WLAN_SCAN_FILTER_NUM_SSID)
		filter->num_of_ssid = WLAN_SCAN_FILTER_NUM_SSID;
	for (i = 0; i < filter->num_of_ssid; i++)
		csr_copy_ssids(&filter->ssid_list[i],
			       &ssid_list->SSIDList[i].SSID);
}

static
QDF_STATUS csr_fill_filter_from_vdev_crypto(struct mac_context *mac_ctx,
					    struct csr_roam_profile *profile,
					    struct scan_filter *filter,
					    uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct rso_config *rso_cfg;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Invalid vdev");
		return QDF_STATUS_E_FAILURE;
	}
	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (rso_cfg) {
		rso_cfg->rsn_cap = 0;
		if (profile->MFPRequired)
			rso_cfg->rsn_cap |= WLAN_CRYPTO_RSN_CAP_MFP_REQUIRED;
		if (profile->MFPCapable)
			rso_cfg->rsn_cap |= WLAN_CRYPTO_RSN_CAP_MFP_ENABLED;
	}

	wlan_cm_fill_crypto_filter_from_vdev(vdev, filter);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS csr_fill_crypto_params(struct mac_context *mac_ctx,
					 struct csr_roam_profile *profile,
					 struct scan_filter *filter,
					 uint8_t vdev_id)
{
	if (profile->force_rsne_override) {
		sme_debug("force_rsne_override do not set authmode and set ignore pmf cap");
		filter->ignore_pmf_cap = true;
		return QDF_STATUS_SUCCESS;
	}

	return csr_fill_filter_from_vdev_crypto(mac_ctx, profile, filter,
						vdev_id);
}

QDF_STATUS
csr_roam_get_scan_filter_from_profile(struct mac_context *mac_ctx,
				      struct csr_roam_profile *profile,
				      struct scan_filter *filter,
				      bool is_roam,
				      uint8_t vdev_id)
{
	tCsrChannelInfo *ch_info;
	uint8_t i;
	QDF_STATUS status;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct rso_config_params *rso_usr_cfg;

	mlme_obj = mlme_get_psoc_ext_obj(mac_ctx->psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	rso_usr_cfg = &mlme_obj->cfg.lfr.rso_user_config;

	if (!filter || !profile) {
		sme_err("filter or profile is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_zero(filter, sizeof(*filter));
	if (profile->BSSIDs.numOfBSSIDs) {
		filter->num_of_bssid = profile->BSSIDs.numOfBSSIDs;
		if (filter->num_of_bssid > WLAN_SCAN_FILTER_NUM_BSSID)
			filter->num_of_bssid = WLAN_SCAN_FILTER_NUM_BSSID;
		for (i = 0; i < filter->num_of_bssid; i++)
			qdf_mem_copy(filter->bssid_list[i].bytes,
				     profile->BSSIDs.bssid[i].bytes,
				     QDF_MAC_ADDR_SIZE);
	}

	if (profile->SSIDs.numOfSSIDs) {
		if (is_roam && rso_usr_cfg->num_ssid_allowed_list)
			csr_copy_ssids_from_roam_params(rso_usr_cfg, filter);
		else
			csr_copy_ssids_from_profile(&profile->SSIDs, filter);
	}

	ch_info = &profile->ChannelInfo;
	if (ch_info->numOfChannels && ch_info->freq_list &&
	    ch_info->freq_list[0]) {
		filter->num_of_channels = 0;
		for (i = 0; i < ch_info->numOfChannels; i++) {
			if (filter->num_of_channels >= NUM_CHANNELS) {
				sme_err("max allowed channel(%d) reached",
					filter->num_of_channels);
				break;
			}
			if (wlan_roam_is_channel_valid(&mac_ctx->mlme_cfg->reg,
						       ch_info->freq_list[i]) &&
			    wlan_cm_dual_sta_is_freq_allowed(
				    mac_ctx->psoc, ch_info->freq_list[i],
				    profile->csrPersona)) {
				filter->chan_freq_list[filter->num_of_channels] =
							ch_info->freq_list[i];
				filter->num_of_channels++;
			} else {
				sme_debug("freq (%d) is invalid",
					  ch_info->freq_list[i]);
			}
		}
	} else {
		/*
		 * Channels allowed is not present in the roam_profile.
		 * Update the the channels for this connection if this is
		 * 2nd STA, with the channels other than the 1st connected
		 * STA, as dual sta roaming is supported only on one band.
		 */
		if (profile->csrPersona == QDF_STA_MODE)
			wlan_cm_dual_sta_roam_update_connect_channels(
							mac_ctx->psoc,
							filter);
	}

	status = csr_fill_crypto_params(mac_ctx, profile, filter, vdev_id);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (profile->bWPSAssociation || profile->bOSENAssociation)
		filter->ignore_auth_enc_type = true;

	filter->mobility_domain = profile->mdid.mobility_domain;
	qdf_mem_copy(filter->bssid_hint.bytes, profile->bssid_hint.bytes,
		     QDF_MAC_ADDR_SIZE);

	csr_update_fils_scan_filter(filter, profile);

	filter->enable_adaptive_11r =
		wlan_mlme_adaptive_11r_enabled(mac_ctx->psoc);
	csr_update_scan_filter_dot11mode(mac_ctx, filter);

	return QDF_STATUS_SUCCESS;
}
#endif

static
bool csr_roam_issue_wm_status_change(struct mac_context *mac, uint32_t sessionId,
				     enum csr_roam_wmstatus_changetypes Type,
				     tSirSmeRsp *pSmeRsp)
{
	bool fCommandQueued = false;
	tSmeCmd *pCommand;
	struct qdf_mac_addr peer_mac;
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac, sessionId);
	if (!session)
		return false;

	do {
		/* Validate the type is ok... */
		if ((eCsrDisassociated != Type)
		    && (eCsrDeauthenticated != Type))
			break;
		pCommand = csr_get_command_buffer(mac);
		if (!pCommand) {
			sme_err(" fail to get command buffer");
			break;
		}
		/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
		/* Change the substate in case it is waiting for key */
		if (CSR_IS_WAIT_FOR_KEY(mac, sessionId)) {
			cm_stop_wait_for_key_timer(mac->psoc, sessionId);
			csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
						 sessionId);
		}
#endif
		pCommand->command = eSmeCommandWmStatusChange;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.wmStatusChangeCmd.Type = Type;
		if (eCsrDisassociated == Type) {
			qdf_mem_copy(&pCommand->u.wmStatusChangeCmd.u.
				     DisassocIndMsg, pSmeRsp,
				     sizeof(pCommand->u.wmStatusChangeCmd.u.
					    DisassocIndMsg));
			qdf_mem_copy(&peer_mac, &pCommand->u.wmStatusChangeCmd.
						u.DisassocIndMsg.peer_macaddr,
				     QDF_MAC_ADDR_SIZE);

		} else {
			qdf_mem_copy(&pCommand->u.wmStatusChangeCmd.u.
				     DeauthIndMsg, pSmeRsp,
				     sizeof(pCommand->u.wmStatusChangeCmd.u.
					    DeauthIndMsg));
			qdf_mem_copy(&peer_mac, &pCommand->u.wmStatusChangeCmd.
						u.DeauthIndMsg.peer_macaddr,
				     QDF_MAC_ADDR_SIZE);
		}

		if (CSR_IS_INFRA_AP(&session->connectedProfile))
			csr_roam_issue_disconnect_stats(mac, sessionId,
							peer_mac);

		if (QDF_IS_STATUS_SUCCESS
			    (csr_queue_sme_command(mac, pCommand, false)))
			fCommandQueued = true;
		else
			sme_err("fail to send message");

		/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
		/* AP has issued Dissac/Deauth, Set the operating mode
		 * value to configured value
		 */
		csr_set_default_dot11_mode(mac);
#endif
	} while (0);
	return fCommandQueued;
}

static void csr_update_snr(struct mac_context *mac, void *pMsg)
{
	tAniGetSnrReq *pGetSnrReq = (tAniGetSnrReq *) pMsg;

	if (pGetSnrReq) {
		if (QDF_STATUS_SUCCESS != wma_get_snr(pGetSnrReq)) {
			sme_err("Error in wma_get_snr");
			return;
		}

	} else
		sme_err("pGetSnrReq is NULL");
}

#ifndef FEATURE_CM_ENABLE

#ifdef FEATURE_WLAN_ESE
/**
 * csr_convert_ese_akm_to_ani() - Convert enum csr_akm_type ESE akm value to
 * equivalent enum ani_akm_type value
 * @akm_type: value of type enum ani_akm_type
 *
 * Return: ani_akm_type value corresponding
 */
static enum ani_akm_type csr_convert_ese_akm_to_ani(enum csr_akm_type akm_type)
{
	switch (akm_type) {
	case eCSR_AUTH_TYPE_CCKM_RSN:
		return ANI_AKM_TYPE_CCKM;
	default:
		return ANI_AKM_TYPE_UNKNOWN;
	}
}
#else
static inline enum
ani_akm_type csr_convert_ese_akm_to_ani(enum csr_akm_type akm_type)
{
	return ANI_AKM_TYPE_UNKNOWN;
}
#endif

#ifdef WLAN_FEATURE_SAE
/**
 * csr_convert_sae_akm_to_ani() - Convert enum csr_akm_type SAE akm value to
 * equivalent enum ani_akm_type value
 * @akm_type: value of type enum ani_akm_type
 *
 * Return: ani_akm_type value corresponding
 */
static enum ani_akm_type csr_convert_sae_akm_to_ani(enum csr_akm_type akm_type)
{
	switch (akm_type) {
	case eCSR_AUTH_TYPE_SAE:
		return ANI_AKM_TYPE_SAE;
	case eCSR_AUTH_TYPE_FT_SAE:
		return ANI_AKM_TYPE_FT_SAE;
	default:
		return ANI_AKM_TYPE_UNKNOWN;
	}
}
#else
static inline
enum ani_akm_type csr_convert_sae_akm_to_ani(enum csr_akm_type akm_type)
{
	return ANI_AKM_TYPE_UNKNOWN;
}
#endif

/**
 * csr_convert_csr_to_ani_akm_type() - Convert enum csr_akm_type value to
 * equivalent enum ani_akm_type value
 * @akm_type: value of type enum ani_akm_type
 *
 * Return: ani_akm_type value corresponding
 */
static enum ani_akm_type
csr_convert_csr_to_ani_akm_type(enum csr_akm_type akm_type)
{
	enum ani_akm_type ani_akm;

	switch (akm_type) {
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
	case eCSR_AUTH_TYPE_NONE:
		return ANI_AKM_TYPE_NONE;
	case eCSR_AUTH_TYPE_WPA:
		return ANI_AKM_TYPE_WPA;
	case eCSR_AUTH_TYPE_WPA_PSK:
		return ANI_AKM_TYPE_WPA_PSK;
	case eCSR_AUTH_TYPE_RSN:
		return ANI_AKM_TYPE_RSN;
	case eCSR_AUTH_TYPE_RSN_PSK:
		return ANI_AKM_TYPE_RSN_PSK;
	case eCSR_AUTH_TYPE_FT_RSN:
		return ANI_AKM_TYPE_FT_RSN;
	case eCSR_AUTH_TYPE_FT_RSN_PSK:
		return ANI_AKM_TYPE_FT_RSN_PSK;
	case eCSR_AUTH_TYPE_RSN_PSK_SHA256:
		return ANI_AKM_TYPE_RSN_PSK_SHA256;
	case eCSR_AUTH_TYPE_RSN_8021X_SHA256:
		return ANI_AKM_TYPE_RSN_8021X_SHA256;
	case eCSR_AUTH_TYPE_FILS_SHA256:
		return ANI_AKM_TYPE_FILS_SHA256;
	case eCSR_AUTH_TYPE_FILS_SHA384:
		return ANI_AKM_TYPE_FILS_SHA384;
	case eCSR_AUTH_TYPE_FT_FILS_SHA256:
		return ANI_AKM_TYPE_FT_FILS_SHA256;
	case eCSR_AUTH_TYPE_FT_FILS_SHA384:
		return ANI_AKM_TYPE_FT_FILS_SHA384;
	case eCSR_AUTH_TYPE_DPP_RSN:
		return ANI_AKM_TYPE_DPP_RSN;
	case eCSR_AUTH_TYPE_OWE:
		return ANI_AKM_TYPE_OWE;
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA256:
		return ANI_AKM_TYPE_SUITEB_EAP_SHA256;
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA384:
		return ANI_AKM_TYPE_SUITEB_EAP_SHA384;
	case eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384:
		return ANI_AKM_TYPE_FT_SUITEB_EAP_SHA384;
	case eCSR_AUTH_TYPE_OSEN:
		return ANI_AKM_TYPE_OSEN;
	default:
		ani_akm = ANI_AKM_TYPE_UNKNOWN;
	}

	if (ani_akm == ANI_AKM_TYPE_UNKNOWN)
		ani_akm = csr_convert_sae_akm_to_ani(akm_type);

	if (ani_akm == ANI_AKM_TYPE_UNKNOWN)
		ani_akm = csr_convert_ese_akm_to_ani(akm_type);

	return ani_akm;
}
#endif

/**
 * csr_translate_akm_type() - Convert ani_akm_type value to equivalent
 * enum csr_akm_type
 * @akm_type: value of type ani_akm_type
 *
 * Return: enum csr_akm_type value
 */
static enum csr_akm_type csr_translate_akm_type(enum ani_akm_type akm_type)
{
	enum csr_akm_type csr_akm_type;

	switch (akm_type)
	{
	case ANI_AKM_TYPE_NONE:
		csr_akm_type = eCSR_AUTH_TYPE_NONE;
		break;
#ifdef WLAN_FEATURE_SAE
	case ANI_AKM_TYPE_SAE:
		csr_akm_type = eCSR_AUTH_TYPE_SAE;
		break;
#endif
	case ANI_AKM_TYPE_WPA:
		csr_akm_type = eCSR_AUTH_TYPE_WPA;
		break;
	case ANI_AKM_TYPE_WPA_PSK:
		csr_akm_type = eCSR_AUTH_TYPE_WPA_PSK;
		break;
	case ANI_AKM_TYPE_RSN:
		csr_akm_type = eCSR_AUTH_TYPE_RSN;
		break;
	case ANI_AKM_TYPE_RSN_PSK:
		csr_akm_type = eCSR_AUTH_TYPE_RSN_PSK;
		break;
	case ANI_AKM_TYPE_FT_RSN:
		csr_akm_type = eCSR_AUTH_TYPE_FT_RSN;
		break;
	case ANI_AKM_TYPE_FT_RSN_PSK:
		csr_akm_type = eCSR_AUTH_TYPE_FT_RSN_PSK;
		break;
#ifdef FEATURE_WLAN_ESE
	case ANI_AKM_TYPE_CCKM:
		csr_akm_type = eCSR_AUTH_TYPE_CCKM_RSN;
		break;
#endif
	case ANI_AKM_TYPE_RSN_PSK_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_RSN_PSK_SHA256;
		break;
	case ANI_AKM_TYPE_RSN_8021X_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_RSN_8021X_SHA256;
		break;
	case ANI_AKM_TYPE_FILS_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_FILS_SHA256;
		break;
	case ANI_AKM_TYPE_FILS_SHA384:
		csr_akm_type = eCSR_AUTH_TYPE_FILS_SHA384;
		break;
	case ANI_AKM_TYPE_FT_FILS_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_FT_FILS_SHA256;
		break;
	case ANI_AKM_TYPE_FT_FILS_SHA384:
		csr_akm_type = eCSR_AUTH_TYPE_FT_FILS_SHA384;
		break;
	case ANI_AKM_TYPE_DPP_RSN:
		csr_akm_type = eCSR_AUTH_TYPE_DPP_RSN;
		break;
	case ANI_AKM_TYPE_OWE:
		csr_akm_type = eCSR_AUTH_TYPE_OWE;
		break;
	case ANI_AKM_TYPE_SUITEB_EAP_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_SUITEB_EAP_SHA256;
		break;
	case ANI_AKM_TYPE_SUITEB_EAP_SHA384:
		csr_akm_type = eCSR_AUTH_TYPE_SUITEB_EAP_SHA384;
		break;
	case ANI_AKM_TYPE_OSEN:
		csr_akm_type = eCSR_AUTH_TYPE_OSEN;
		break;
	default:
		csr_akm_type = eCSR_AUTH_TYPE_UNKNOWN;
	}

	return csr_akm_type;
}

static bool csr_is_sae_akm_present(tDot11fIERSN * const rsn_ie)
{
	uint16_t i;

	if (rsn_ie->akm_suite_cnt > 6) {
		sme_debug("Invalid akm_suite_cnt in Rx RSN IE");
		return false;
	}

	for (i = 0; i < rsn_ie->akm_suite_cnt; i++) {
		if (LE_READ_4(rsn_ie->akm_suite[i]) == RSN_AUTH_KEY_MGMT_SAE) {
			sme_debug("SAE AKM present");
			return true;
		}
	}
	return false;
}

static bool csr_is_sae_peer_allowed(struct mac_context *mac_ctx,
				    struct assoc_ind *assoc_ind,
				    struct csr_roam_session *session,
				    tSirMacAddr peer_mac_addr,
				    tDot11fIERSN *rsn_ie,
				    enum wlan_status_code *mac_status_code)
{
	bool is_allowed = false;

	/* Allow the peer if it's SAE authenticated */
	if (assoc_ind->is_sae_authenticated)
		return true;

	/* Allow the peer with valid PMKID */
	if (!rsn_ie->pmkid_count) {
		*mac_status_code = STATUS_NOT_SUPPORTED_AUTH_ALG;
		sme_debug("No PMKID present in RSNIE; Tried to use SAE AKM after non-SAE authentication");
	} else if (csr_is_pmkid_found_for_peer(mac_ctx, session, peer_mac_addr,
					       &rsn_ie->pmkid[0][0],
					       rsn_ie->pmkid_count)) {
		sme_debug("Valid PMKID found for SAE peer");
		is_allowed = true;
	} else {
		*mac_status_code = STATUS_INVALID_PMKID;
		sme_debug("No valid PMKID found for SAE peer");
	}

	return is_allowed;
}

static QDF_STATUS
csr_send_assoc_ind_to_upper_layer_cnf_msg(struct mac_context *mac,
					  struct assoc_ind *ind,
					  QDF_STATUS status,
					  uint8_t vdev_id)
{
	struct scheduler_msg msg = {0};
	tSirSmeAssocIndToUpperLayerCnf *cnf;

	cnf = qdf_mem_malloc(sizeof(*cnf));
	if (!cnf)
		return QDF_STATUS_E_NOMEM;

	cnf->messageType = eWNI_SME_UPPER_LAYER_ASSOC_CNF;
	cnf->length = sizeof(*cnf);
	cnf->sessionId = vdev_id;

	if (QDF_IS_STATUS_SUCCESS(status))
		cnf->status_code = eSIR_SME_SUCCESS;
	else
		cnf->status_code = eSIR_SME_ASSOC_REFUSED;
	qdf_mem_copy(&cnf->bssId, &ind->bssId, sizeof(cnf->bssId));
	qdf_mem_copy(&cnf->peerMacAddr, &ind->peerMacAddr,
		     sizeof(cnf->peerMacAddr));
	cnf->aid = ind->staId;
	cnf->wmmEnabledSta = ind->wmmEnabledSta;
	cnf->rsnIE = ind->rsnIE;
#ifdef FEATURE_WLAN_WAPI
	cnf->wapiIE = ind->wapiIE;
#endif
	cnf->addIE = ind->addIE;
	cnf->reassocReq = ind->reassocReq;
	cnf->timingMeasCap = ind->timingMeasCap;
	cnf->chan_info = ind->chan_info;
	cnf->ampdu = ind->ampdu;
	cnf->sgi_enable = ind->sgi_enable;
	cnf->tx_stbc = ind->tx_stbc;
	cnf->ch_width = ind->ch_width;
	cnf->mode = ind->mode;
	cnf->rx_stbc = ind->rx_stbc;
	cnf->max_supp_idx = ind->max_supp_idx;
	cnf->max_ext_idx = ind->max_ext_idx;
	cnf->max_mcs_idx = ind->max_mcs_idx;
	cnf->rx_mcs_map = ind->rx_mcs_map;
	cnf->tx_mcs_map = ind->tx_mcs_map;
	cnf->ecsa_capable = ind->ecsa_capable;
	if (ind->HTCaps.present)
		cnf->ht_caps = ind->HTCaps;
	if (ind->VHTCaps.present)
		cnf->vht_caps = ind->VHTCaps;
	cnf->capability_info = ind->capability_info;
	cnf->he_caps_present = ind->he_caps_present;
	if (ind->assocReqPtr) {
		if (ind->assocReqLength < MAX_ASSOC_REQ_IE_LEN) {
			cnf->ies = qdf_mem_malloc(ind->assocReqLength);
			if (!cnf->ies) {
				qdf_mem_free(cnf);
				return QDF_STATUS_E_NOMEM;
			}
			cnf->ies_len = ind->assocReqLength;
			qdf_mem_copy(cnf->ies, ind->assocReqPtr,
				     cnf->ies_len);
		} else {
			sme_err("Assoc Ie length is too long");
		}
	}

	msg.type = eWNI_SME_UPPER_LAYER_ASSOC_CNF;
	msg.bodyptr = cnf;
	sys_process_mmh_msg(mac, &msg);

	return QDF_STATUS_SUCCESS;
}

static void
csr_roam_chk_lnk_assoc_ind_upper_layer(
		struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t session_id = WLAN_UMAC_VDEV_ID_MAX;
	struct assoc_ind *assoc_ind;
	QDF_STATUS status;

	assoc_ind = (struct assoc_ind *)msg_ptr;
	status = csr_roam_get_session_id_from_bssid(
			mac_ctx, (struct qdf_mac_addr *)assoc_ind->bssId,
			&session_id);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Couldn't find session_id for given BSSID");
		goto free_mem;
	}
	csr_send_assoc_ind_to_upper_layer_cnf_msg(
					mac_ctx, assoc_ind, status, session_id);
	/*in the association response tx compete case,
	 *memory for assoc_ind->assocReqPtr will be malloced
	 *in the lim_assoc_rsp_tx_complete -> lim_fill_sme_assoc_ind_params
	 *and then assoc_ind will pass here, so after using it
	 *in the csr_send_assoc_ind_to_upper_layer_cnf_msg and
	 *then free the memroy here.
	 */
free_mem:
	if (assoc_ind->assocReqLength != 0 && assoc_ind->assocReqPtr)
		qdf_mem_free(assoc_ind->assocReqPtr);
}

static void
csr_roam_chk_lnk_assoc_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	struct assoc_ind *pAssocInd;
	enum wlan_status_code mac_status_code = STATUS_SUCCESS;
	enum csr_akm_type csr_akm_type;

	sme_debug("Receive WNI_SME_ASSOC_IND from SME");
	pAssocInd = (struct assoc_ind *) msg_ptr;
	sme_debug("Receive WNI_SME_ASSOC_IND from SME vdev id %d",
		  pAssocInd->sessionId);
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
				(struct qdf_mac_addr *) pAssocInd->bssId,
				&sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Couldn't find session_id for given BSSID" QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(pAssocInd->bssId));
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}
	csr_akm_type = csr_translate_akm_type(pAssocInd->akm_type);

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	/* Required for indicating the frames to upper layer */
	roam_info->assocReqLength = pAssocInd->assocReqLength;
	roam_info->assocReqPtr = pAssocInd->assocReqPtr;
	roam_info->status_code = eSIR_SME_SUCCESS;
	roam_info->u.pConnectedProfile = &session->connectedProfile;
	roam_info->staId = (uint8_t)pAssocInd->staId;
	roam_info->rsnIELen = (uint8_t)pAssocInd->rsnIE.length;
	roam_info->prsnIE = pAssocInd->rsnIE.rsnIEdata;
#ifdef FEATURE_WLAN_WAPI
	roam_info->wapiIELen = (uint8_t)pAssocInd->wapiIE.length;
	roam_info->pwapiIE = pAssocInd->wapiIE.wapiIEdata;
#endif
	roam_info->addIELen = (uint8_t)pAssocInd->addIE.length;
	roam_info->paddIE = pAssocInd->addIE.addIEdata;
	qdf_mem_copy(roam_info->peerMac.bytes,
		     pAssocInd->peerMacAddr,
		     sizeof(tSirMacAddr));
	qdf_mem_copy(roam_info->bssid.bytes,
		     pAssocInd->bssId,
		     sizeof(struct qdf_mac_addr));
	roam_info->wmmEnabledSta = pAssocInd->wmmEnabledSta;
	roam_info->timingMeasCap = pAssocInd->timingMeasCap;
	roam_info->ecsa_capable = pAssocInd->ecsa_capable;
	qdf_mem_copy(&roam_info->chan_info,
		     &pAssocInd->chan_info,
		     sizeof(struct oem_channel_info));

	if (pAssocInd->HTCaps.present)
		qdf_mem_copy(&roam_info->ht_caps,
			     &pAssocInd->HTCaps,
			     sizeof(tDot11fIEHTCaps));
	if (pAssocInd->VHTCaps.present)
		qdf_mem_copy(&roam_info->vht_caps,
			     &pAssocInd->VHTCaps,
			     sizeof(tDot11fIEVHTCaps));
	roam_info->capability_info = pAssocInd->capability_info;
	roam_info->he_caps_present = pAssocInd->he_caps_present;

	if (CSR_IS_INFRA_AP(roam_info->u.pConnectedProfile)) {
		if (session->pCurRoamProfile &&
		    CSR_IS_ENC_TYPE_STATIC(
			session->pCurRoamProfile->negotiatedUCEncryptionType)) {
			/* NO keys... these key parameters don't matter. */
			csr_issue_set_context_req_helper(mac_ctx,
					session->pCurRoamProfile, sessionId,
					&roam_info->peerMac.bytes, false, true,
					eSIR_TX_RX, 0, 0, NULL);
			roam_info->fAuthRequired = false;
		} else {
			roam_info->fAuthRequired = true;
		}
		if (csr_akm_type == eCSR_AUTH_TYPE_OWE) {
			roam_info->owe_pending_assoc_ind = qdf_mem_malloc(
							    sizeof(*pAssocInd));
			if (roam_info->owe_pending_assoc_ind)
				qdf_mem_copy(roam_info->owe_pending_assoc_ind,
					     pAssocInd, sizeof(*pAssocInd));
		}
		status = csr_roam_call_callback(mac_ctx, sessionId,
					roam_info, 0, eCSR_ROAM_INFRA_IND,
					eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			/* Refused due to Mac filtering */
			roam_info->status_code = eSIR_SME_ASSOC_REFUSED;
		} else if (pAssocInd->rsnIE.length && WLAN_ELEMID_RSN ==
			   pAssocInd->rsnIE.rsnIEdata[0]) {
			tDot11fIERSN rsn_ie = {0};

			if (dot11f_unpack_ie_rsn(mac_ctx,
						 pAssocInd->rsnIE.rsnIEdata + 2,
						 pAssocInd->rsnIE.length - 2,
						 &rsn_ie, false)
			    != DOT11F_PARSE_SUCCESS ||
			    (csr_is_sae_akm_present(&rsn_ie) &&
			     !csr_is_sae_peer_allowed(mac_ctx, pAssocInd,
						      session,
						      pAssocInd->peerMacAddr,
						      &rsn_ie,
						      &mac_status_code))) {
				status = QDF_STATUS_E_INVAL;
				roam_info->status_code =
						eSIR_SME_ASSOC_REFUSED;
				sme_debug("SAE peer not allowed: Status: %d",
					  mac_status_code);
			}
		}
	}

	if (csr_akm_type != eCSR_AUTH_TYPE_OWE) {
		if (CSR_IS_INFRA_AP(roam_info->u.pConnectedProfile) &&
		    roam_info->status_code != eSIR_SME_ASSOC_REFUSED)
			pAssocInd->need_assoc_rsp_tx_cb = true;
		/* Send Association completion message to PE */
		status = csr_send_assoc_cnf_msg(mac_ctx, pAssocInd, status,
						mac_status_code);
	}

	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_disassoc_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct disassoc_ind *pDisassocInd;

	/*
	 * Check if AP dis-associated us because of MIC failure. If so,
	 * then we need to take action immediately and not wait till the
	 * the WmStatusChange requests is pushed and processed
	 */
	pDisassocInd = (struct disassoc_ind *)msg_ptr;
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
				&pDisassocInd->bssid, &sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Session Id not found for BSSID "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(pDisassocInd->bssid.bytes));

		return;
	}

	if (csr_is_deauth_disassoc_already_active(mac_ctx, sessionId,
	    pDisassocInd->peer_macaddr))
		return;

	sme_nofl_info("disassoc from peer " QDF_MAC_ADDR_FMT
		      "reason: %d status: %d vid %d",
		      QDF_MAC_ADDR_REF(pDisassocInd->peer_macaddr.bytes),
		      pDisassocInd->reasonCode,
		      pDisassocInd->status_code, sessionId);
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session: %d not found", sessionId);
		return;
	}
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	/*
	 * If we are in neighbor preauth done state then on receiving
	 * disassoc or deauth we dont roam instead we just disassoc
	 * from current ap and then go to disconnected state
	 * This happens for ESE and 11r FT connections ONLY.
	 */
	if (csr_roam_is11r_assoc(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
#ifdef FEATURE_WLAN_ESE
	if (csr_roam_is_ese_assoc(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
#endif
	if (csr_roam_is_fast_roam_enabled(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
	csr_update_scan_entry_associnfo(mac_ctx,
					sessionId, SCAN_ENTRY_CON_STATE_NONE);
#endif
	/* Update the disconnect stats */
	session->disconnect_stats.disconnection_cnt++;
	session->disconnect_stats.disassoc_by_peer++;

	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	if (csr_is_conn_state_infra(mac_ctx, sessionId))
		session->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
	sme_qos_csr_event_ind(mac_ctx, (uint8_t) sessionId,
			      SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
	csr_roam_link_down(mac_ctx, sessionId);
#endif
	csr_roam_issue_wm_status_change(mac_ctx, sessionId,
					eCsrDisassociated, msg_ptr);
}

static void
csr_roam_chk_lnk_deauth_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct deauth_ind *pDeauthInd;

	pDeauthInd = (struct deauth_ind *)msg_ptr;
	sme_debug("DEAUTH Indication from MAC for vdev_id %d bssid "QDF_MAC_ADDR_FMT,
		  pDeauthInd->vdev_id,
		  QDF_MAC_ADDR_REF(pDeauthInd->bssid.bytes));

	status = csr_roam_get_session_id_from_bssid(mac_ctx,
						   &pDeauthInd->bssid,
						   &sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return;

	if (csr_is_deauth_disassoc_already_active(mac_ctx, sessionId,
	    pDeauthInd->peer_macaddr))
		return;
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	/* If we are in neighbor preauth done state then on receiving
	 * disassoc or deauth we dont roam instead we just disassoc
	 * from current ap and then go to disconnected state
	 * This happens for ESE and 11r FT connections ONLY.
	 */
	if (csr_roam_is11r_assoc(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
#ifdef FEATURE_WLAN_ESE
	if (csr_roam_is_ese_assoc(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
#endif
	if (csr_roam_is_fast_roam_enabled(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);

	csr_update_scan_entry_associnfo(mac_ctx,
					sessionId, SCAN_ENTRY_CON_STATE_NONE);
#endif
	/* Update the disconnect stats */
	switch (pDeauthInd->reasonCode) {
	case REASON_DISASSOC_DUE_TO_INACTIVITY:
		session->disconnect_stats.disconnection_cnt++;
		session->disconnect_stats.peer_kickout++;
		break;
	case REASON_UNSPEC_FAILURE:
	case REASON_PREV_AUTH_NOT_VALID:
	case REASON_DEAUTH_NETWORK_LEAVING:
	case REASON_CLASS2_FRAME_FROM_NON_AUTH_STA:
	case REASON_CLASS3_FRAME_FROM_NON_ASSOC_STA:
	case REASON_STA_NOT_AUTHENTICATED:
		session->disconnect_stats.disconnection_cnt++;
		session->disconnect_stats.deauth_by_peer++;
		break;
	case REASON_BEACON_MISSED:
		session->disconnect_stats.disconnection_cnt++;
		session->disconnect_stats.bmiss++;
		break;
	default:
		/* Unknown reason code */
		break;
	}

	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	if (csr_is_conn_state_infra(mac_ctx, sessionId))
		session->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
	sme_qos_csr_event_ind(mac_ctx, (uint8_t) sessionId,
			      SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
	csr_roam_link_down(mac_ctx, sessionId);
#endif
	csr_roam_issue_wm_status_change(mac_ctx, sessionId,
					eCsrDeauthenticated,
					msg_ptr);
}

static void
csr_roam_chk_lnk_swt_ch_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct switch_channel_ind *pSwitchChnInd;
	struct csr_roam_info *roam_info;

	/* in case of STA, the SWITCH_CHANNEL originates from its AP */
	sme_debug("eWNI_SME_SWITCH_CHL_IND from SME");
	pSwitchChnInd = (struct switch_channel_ind *)msg_ptr;
	/* Update with the new channel id. The channel id is hidden in the
	 * status_code.
	 */
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
			&pSwitchChnInd->bssid, &sessionId);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}

	if (QDF_IS_STATUS_ERROR(pSwitchChnInd->status)) {
		sme_err("Channel switch failed");
		return;
	}
	/* Update the occupied channel list with the new switched channel */
	wlan_cm_init_occupied_ch_freq_list(mac_ctx->pdev, mac_ctx->psoc,
					   sessionId);
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	roam_info->chan_info.mhz = pSwitchChnInd->freq;
	roam_info->chan_info.ch_width = pSwitchChnInd->chan_params.ch_width;
	roam_info->chan_info.sec_ch_offset =
				pSwitchChnInd->chan_params.sec_ch_offset;
	roam_info->chan_info.band_center_freq1 =
				pSwitchChnInd->chan_params.mhz_freq_seg0;
	roam_info->chan_info.band_center_freq2 =
				pSwitchChnInd->chan_params.mhz_freq_seg1;

	if (IS_WLAN_PHYMODE_HT(pSwitchChnInd->ch_phymode))
		roam_info->mode = SIR_SME_PHY_MODE_HT;
	else if (IS_WLAN_PHYMODE_VHT(pSwitchChnInd->ch_phymode) ||
		 IS_WLAN_PHYMODE_HE(pSwitchChnInd->ch_phymode))
		roam_info->mode = SIR_SME_PHY_MODE_VHT;
#ifdef WLAN_FEATURE_11BE
	else if (IS_WLAN_PHYMODE_EHT(pSwitchChnInd->ch_phymode))
		roam_info->mode = SIR_SME_PHY_MODE_VHT;
#endif
	else
		roam_info->mode = SIR_SME_PHY_MODE_LEGACY;

	status = csr_roam_call_callback(mac_ctx, sessionId, roam_info, 0,
					eCSR_ROAM_STA_CHANNEL_SWITCH,
					eCSR_ROAM_RESULT_NONE);
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_deauth_rsp(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	struct deauth_rsp *pDeauthRsp = (struct deauth_rsp *) msg_ptr;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sme_debug("eWNI_SME_DEAUTH_RSP from SME");
	sessionId = pDeauthRsp->sessionId;
	if (!CSR_IS_SESSION_VALID(mac_ctx, sessionId)) {
		qdf_mem_free(roam_info);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (CSR_IS_INFRA_AP(&session->connectedProfile)) {
		roam_info->u.pConnectedProfile = &session->connectedProfile;
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDeauthRsp->peer_macaddr);
		roam_info->reasonCode = eCSR_ROAM_RESULT_FORCED;
		roam_info->status_code = pDeauthRsp->status_code;
		status = csr_roam_call_callback(mac_ctx, sessionId,
						roam_info, 0,
						eCSR_ROAM_LOSTLINK,
						eCSR_ROAM_RESULT_FORCED);
	}
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_disassoc_rsp(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	/*
	 * session id is invalid here so cant use it to access the array
	 * curSubstate as index
	 */
	struct disassoc_rsp *pDisassocRsp = (struct disassoc_rsp *) msg_ptr;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sme_debug("eWNI_SME_DISASSOC_RSP from SME ");
	sessionId = pDisassocRsp->sessionId;
	if (!CSR_IS_SESSION_VALID(mac_ctx, sessionId)) {
		qdf_mem_free(roam_info);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (CSR_IS_INFRA_AP(&session->connectedProfile)) {
		roam_info->u.pConnectedProfile = &session->connectedProfile;
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDisassocRsp->peer_macaddr);
		roam_info->reasonCode = eCSR_ROAM_RESULT_FORCED;
		roam_info->status_code = pDisassocRsp->status_code;
		status = csr_roam_call_callback(mac_ctx, sessionId,
						roam_info, 0,
						eCSR_ROAM_LOSTLINK,
						eCSR_ROAM_RESULT_FORCED);
	}
	qdf_mem_free(roam_info);
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static void
csr_roam_diag_mic_fail(struct mac_context *mac_ctx, uint32_t sessionId)
{
	WLAN_HOST_DIAG_EVENT_DEF(secEvent,
				 host_event_wlan_security_payload_type);
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, sessionId);

	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}
	qdf_mem_zero(&secEvent, sizeof(host_event_wlan_security_payload_type));
	secEvent.eventId = WLAN_SECURITY_EVENT_MIC_ERROR;
	cm_diag_get_auth_enc_type_vdev_id(mac_ctx->psoc,
					  &secEvent.authMode,
					  &secEvent.encryptionModeUnicast,
					  &secEvent.encryptionModeMulticast,
					  sessionId);
	wlan_mlme_get_bssid_vdev_id(mac_ctx->pdev, sessionId,
				    (struct qdf_mac_addr *)&secEvent.bssid);
	WLAN_HOST_DIAG_EVENT_REPORT(&secEvent, EVENT_WLAN_SECURITY);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

static void
csr_roam_chk_lnk_mic_fail_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	struct mic_failure_ind *mic_ind = (struct mic_failure_ind *)msg_ptr;
	eCsrRoamResult result = eCSR_ROAM_RESULT_MIC_ERROR_UNICAST;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
				&mic_ind->bssId, &sessionId);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		roam_info->u.pMICFailureInfo = &mic_ind->info;
		if (mic_ind->info.multicast)
			result = eCSR_ROAM_RESULT_MIC_ERROR_GROUP;
		else
			result = eCSR_ROAM_RESULT_MIC_ERROR_UNICAST;
		csr_roam_call_callback(mac_ctx, sessionId, roam_info, 0,
				       eCSR_ROAM_MIC_ERROR_IND, result);
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	csr_roam_diag_mic_fail(mac_ctx, sessionId);
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_pbs_probe_req_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	tpSirSmeProbeReqInd pProbeReqInd = (tpSirSmeProbeReqInd) msg_ptr;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sme_debug("WPS PBC Probe request Indication from SME");

	status = csr_roam_get_session_id_from_bssid(mac_ctx,
			&pProbeReqInd->bssid, &sessionId);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		roam_info->u.pWPSPBCProbeReq = &pProbeReqInd->WPSPBCProbeReq;
		csr_roam_call_callback(mac_ctx, sessionId, roam_info,
				       0, eCSR_ROAM_WPS_PBC_PROBE_REQ_IND,
				       eCSR_ROAM_RESULT_WPS_PBC_PROBE_REQ_IND);
	}
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_max_assoc_exceeded(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	tSmeMaxAssocInd *pSmeMaxAssocInd;
	struct csr_roam_info *roam_info;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	pSmeMaxAssocInd = (tSmeMaxAssocInd *) msg_ptr;
	sme_debug(
		"max assoc have been reached, new peer cannot be accepted");
	sessionId = pSmeMaxAssocInd->sessionId;
	roam_info->sessionId = sessionId;
	qdf_copy_macaddr(&roam_info->peerMac, &pSmeMaxAssocInd->peer_mac);
	csr_roam_call_callback(mac_ctx, sessionId, roam_info, 0,
			       eCSR_ROAM_INFRA_IND,
			       eCSR_ROAM_RESULT_MAX_ASSOC_EXCEEDED);
	qdf_mem_free(roam_info);
}

void csr_roam_check_for_link_status_change(struct mac_context *mac,
						tSirSmeRsp *pSirMsg)
{
	if (!pSirMsg) {
		sme_err("pSirMsg is NULL");
		return;
	}
	switch (pSirMsg->messageType) {
	case eWNI_SME_ASSOC_IND:
		csr_roam_chk_lnk_assoc_ind(mac, pSirMsg);
		break;
	case eWNI_SME_ASSOC_IND_UPPER_LAYER:
		csr_roam_chk_lnk_assoc_ind_upper_layer(mac, pSirMsg);
		break;
	case eWNI_SME_DISASSOC_IND:
		csr_roam_chk_lnk_disassoc_ind(mac, pSirMsg);
		break;
	case eWNI_SME_DISCONNECT_DONE_IND:
		csr_roam_send_disconnect_done_indication(mac, pSirMsg);
		break;
	case eWNI_SME_DEAUTH_IND:
		csr_roam_chk_lnk_deauth_ind(mac, pSirMsg);
		break;
	case eWNI_SME_SWITCH_CHL_IND:
		csr_roam_chk_lnk_swt_ch_ind(mac, pSirMsg);
		break;
	case eWNI_SME_DEAUTH_RSP:
		csr_roam_chk_lnk_deauth_rsp(mac, pSirMsg);
		break;
	case eWNI_SME_DISASSOC_RSP:
		csr_roam_chk_lnk_disassoc_rsp(mac, pSirMsg);
		break;
	case eWNI_SME_MIC_FAILURE_IND:
		csr_roam_chk_lnk_mic_fail_ind(mac, pSirMsg);
		break;
	case eWNI_SME_WPS_PBC_PROBE_REQ_IND:
		csr_roam_chk_lnk_pbs_probe_req_ind(mac, pSirMsg);
		break;
	case eWNI_SME_SETCONTEXT_RSP:
		csr_roam_chk_lnk_set_ctx_rsp(mac, pSirMsg);
		break;
#ifdef FEATURE_WLAN_ESE
	case eWNI_SME_GET_TSM_STATS_RSP:
		sme_debug("TSM Stats rsp from PE");
		csr_tsm_stats_rsp_processor(mac, pSirMsg);
		break;
#endif /* FEATURE_WLAN_ESE */
	case eWNI_SME_GET_SNR_REQ:
		sme_debug("GetSnrReq from self");
		csr_update_snr(mac, pSirMsg);
		break;
#ifndef FEATURE_CM_ENABLE
	case eWNI_SME_FT_PRE_AUTH_RSP:
		csr_roam_ft_pre_auth_rsp_processor(mac,
						(tpSirFTPreAuthRsp) pSirMsg);
		break;
#endif
	case eWNI_SME_MAX_ASSOC_EXCEEDED:
		csr_roam_chk_lnk_max_assoc_exceeded(mac, pSirMsg);
		break;
#ifndef FEATURE_CM_ENABLE
	case eWNI_SME_CANDIDATE_FOUND_IND:
		csr_neighbor_roam_candidate_found_ind_hdlr(mac, pSirMsg);
		break;
	case eWNI_SME_HANDOFF_REQ:
		sme_debug("Handoff Req from self");
		csr_neighbor_roam_handoff_req_hdlr(mac, pSirMsg);
		break;
#endif
	default:
		break;
	} /* end switch on message type */
}

#ifndef FEATURE_CM_ENABLE
void csr_call_roaming_completion_callback(struct mac_context *mac,
					  struct csr_roam_session *pSession,
					  struct csr_roam_info *roam_info,
					  uint32_t roamId,
					  eCsrRoamResult roamResult)
{
	if (pSession) {
		if (pSession->bRefAssocStartCnt) {
			pSession->bRefAssocStartCnt--;

			if (0 != pSession->bRefAssocStartCnt) {
				QDF_ASSERT(pSession->bRefAssocStartCnt == 0);
				return;
			}
			/* Need to call association_completion because there
			 * is an assoc_start pending.
			 */
			csr_roam_call_callback(mac, pSession->sessionId, NULL,
					       roamId,
					       eCSR_ROAM_ASSOCIATION_COMPLETION,
					       eCSR_ROAM_RESULT_FAILURE);
		}
		csr_roam_call_callback(mac, pSession->sessionId, roam_info,
				       roamId, eCSR_ROAM_ROAMING_COMPLETION,
				       roamResult);
	} else
		sme_err("pSession is NULL");
}

/* return a bool to indicate whether roaming completed or continue. */
bool csr_roam_complete_roaming(struct mac_context *mac, uint32_t sessionId,
			       bool fForce, eCsrRoamResult roamResult)
{
	bool fCompleted = true;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found ", sessionId);
		return false;
	}
	/* Check whether time is up */
	if (pSession->fCancelRoaming || fForce ||
	    eCsrReassocRoaming == pSession->roamingReason ||
	    eCsrDynamicRoaming == pSession->roamingReason) {
		sme_debug("indicates roaming completion");
		if (pSession->fCancelRoaming
		    && CSR_IS_LOSTLINK_ROAMING(pSession->roamingReason)) {
			/* roaming is cancelled, tell HDD to indicate disconnect
			 * Because LIM overload deauth_ind for both deauth frame
			 * and missed beacon we need to use this logic to
			 * detinguish it. For missed beacon, LIM set reason to
			 * be eSIR_BEACON_MISSED
			 */
			if (pSession->roamingStatusCode ==
			    REASON_BEACON_MISSED) {
				roamResult = eCSR_ROAM_RESULT_LOSTLINK;
			} else if (eCsrLostlinkRoamingDisassoc ==
				   pSession->roamingReason) {
				roamResult = eCSR_ROAM_RESULT_DISASSOC_IND;
			} else if (eCsrLostlinkRoamingDeauth ==
				   pSession->roamingReason) {
				roamResult = eCSR_ROAM_RESULT_DEAUTH_IND;
			} else {
				roamResult = eCSR_ROAM_RESULT_LOSTLINK;
			}
		}
		csr_call_roaming_completion_callback(mac, pSession, NULL, 0,
						     roamResult);
		pSession->roamingReason = eCsrNotRoaming;
	} else {
		pSession->roamResult = roamResult;
		if (!QDF_IS_STATUS_SUCCESS(csr_roam_start_roaming_timer(mac,
					sessionId, QDF_MC_TIMER_TO_SEC_UNIT))) {
			csr_call_roaming_completion_callback(mac, pSession,
							NULL, 0, roamResult);
			pSession->roamingReason = eCsrNotRoaming;
		} else {
			fCompleted = false;
		}
	}
	return fCompleted;
}

void csr_roam_cancel_roaming(struct mac_context *mac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session: %d not found", sessionId);
		return;
	}

	if (CSR_IS_ROAMING(pSession)) {
		sme_debug("Cancel roaming");
		pSession->fCancelRoaming = true;
		if (CSR_IS_ROAM_JOINING(mac, sessionId)
		    && CSR_IS_ROAM_SUBSTATE_CONFIG(mac, sessionId)) {
			/* No need to do anything in here because the handler
			 * takes care of it
			 */
		} else {
			eCsrRoamResult roamResult =
				CSR_IS_LOSTLINK_ROAMING(pSession->
							roamingReason) ?
				eCSR_ROAM_RESULT_LOSTLINK :
							eCSR_ROAM_RESULT_NONE;
			/* Roaming is stopped after here */
			csr_roam_complete_roaming(mac, sessionId, true,
						  roamResult);
			/* Since CSR may be in lostlink roaming situation,
			 * abort all roaming related activities
			 */
			csr_scan_abort_mac_scan(mac, sessionId, INVAL_SCAN_ID);
			csr_roam_stop_roaming_timer(mac, sessionId);
		}
	}
}

void csr_roam_roaming_timer_handler(void *pv)
{
	struct csr_timer_info *info = pv;
	struct mac_context *mac = info->mac;
	uint32_t vdev_id = info->vdev_id;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, vdev_id);

	if (!pSession) {
		sme_err("  session %d not found ", vdev_id);
		return;
	}

	if (false == pSession->fCancelRoaming) {
		csr_call_roaming_completion_callback(mac, pSession,
						NULL, 0,
						pSession->roamResult);
		pSession->roamingReason = eCsrNotRoaming;
	}
}

/**
 * csr_roam_roaming_offload_timeout_handler() - Handler for roaming failure
 * @timer_data: Carries the mac_ctx and session info
 *
 * This function would be invoked when the roaming_offload_timer expires.
 * The timer is waiting in anticipation of a related roaming event from
 * the firmware after receiving the ROAM_START event.
 *
 * Return: None
 */
void csr_roam_roaming_offload_timeout_handler(void *timer_data)
{
	struct csr_timer_info *timer_info = timer_data;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_roam_after_data_stall *vdev_roam_params;
	struct scheduler_msg wma_msg = {0};
	struct roam_sync_timeout_timer_info *req;
	QDF_STATUS status;

	if (timer_info) {
		sme_debug("LFR3:roaming offload timer expired, session: %d",
			  timer_info->vdev_id);
	} else {
		sme_err("Invalid Session");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(timer_info->mac->psoc,
						    timer_info->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL, aborting roam start");
		return;
	}

	vdev_roam_params = mlme_get_roam_invoke_params(vdev);
	if (!vdev_roam_params) {
		sme_err("Invalid vdev roam params, aborting timeout handler");
		status = QDF_STATUS_E_NULL_VALUE;
		goto rel;
	}

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		goto rel;

	req->vdev_id = timer_info->vdev_id;
	wma_msg.type = WMA_ROAM_SYNC_TIMEOUT;
	wma_msg.bodyptr = req;

	status = scheduler_post_message(QDF_MODULE_ID_SME, QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &wma_msg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Post roam offload timer fails, session id: %d",
			timer_info->vdev_id);
		qdf_mem_free(req);
		goto rel;
	}

	vdev_roam_params->roam_invoke_in_progress = false;

rel:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}

QDF_STATUS csr_roam_start_roaming_timer(struct mac_context *mac,
					uint32_t vdev_id,
					uint32_t interval)
{
	QDF_STATUS status;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, vdev_id);

	if (!pSession) {
		sme_err("session %d not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	sme_debug("csrScanStartRoamingTimer");
	pSession->roamingTimerInfo.vdev_id = (uint8_t) vdev_id;
	status = qdf_mc_timer_start(&pSession->hTimerRoaming,
				    interval / QDF_MC_TIMER_TO_MS_UNIT);

	return status;
}

QDF_STATUS csr_roam_stop_roaming_timer(struct mac_context *mac,
		uint32_t sessionId)
{
	return qdf_mc_timer_stop
			(&mac->roam.roamSession[sessionId].hTimerRoaming);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * csr_roam_roaming_offload_timer_action() - API to start/stop the timer
 * @mac_ctx: MAC Context
 * @interval: Value to be set for the timer
 * @vdev_id: Session on which the timer should be operated
 * @action: Start/Stop action for the timer
 *
 * API to start/stop the roaming offload timer
 *
 * Return: None
 */
void csr_roam_roaming_offload_timer_action(
		struct mac_context *mac_ctx, uint32_t interval, uint8_t vdev_id,
		uint8_t action)
{
	struct csr_roam_session *csr_session = CSR_GET_SESSION(mac_ctx,
				vdev_id);
	QDF_TIMER_STATE tstate;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			("LFR3: timer action %d, session %d, intvl %d"),
			action, vdev_id, interval);
	if (mac_ctx) {
		csr_session = CSR_GET_SESSION(mac_ctx, vdev_id);
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				("LFR3: Invalid MAC Context"));
		return;
	}
	if (!csr_session) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				("LFR3: session %d not found"), vdev_id);
		return;
	}
	csr_session->roamingTimerInfo.vdev_id = (uint8_t) vdev_id;

	tstate =
	   qdf_mc_timer_get_current_state(&csr_session->roaming_offload_timer);

	if (action == ROAMING_OFFLOAD_TIMER_START) {
		/*
		 * If timer is already running then re-start timer in order to
		 * process new ROAM_START with fresh timer.
		 */
		if (tstate == QDF_TIMER_STATE_RUNNING)
			qdf_mc_timer_stop(&csr_session->roaming_offload_timer);
		qdf_mc_timer_start(&csr_session->roaming_offload_timer,
				   interval / QDF_MC_TIMER_TO_MS_UNIT);
	}
	if (action == ROAMING_OFFLOAD_TIMER_STOP)
		qdf_mc_timer_stop(&csr_session->roaming_offload_timer);

}
#endif

void csr_roam_completion(struct mac_context *mac, uint32_t vdev_id,
			 struct csr_roam_info *roam_info, tSmeCmd *pCommand,
			 eCsrRoamResult roamResult, bool fSuccess)
{
	eRoamCmdStatus roamStatus = csr_get_roam_complete_status(mac,
								vdev_id);
	uint32_t roamId = 0;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, vdev_id);

	if (!pSession) {
		sme_err("session: %d not found ", vdev_id);
		return;
	}

	if (pCommand) {
		roamId = pCommand->u.roamCmd.roamId;
		if (vdev_id != pCommand->vdev_id) {
			QDF_ASSERT(vdev_id == pCommand->vdev_id);
			return;
		}
	}
	if (eCSR_ROAM_ROAMING_COMPLETION == roamStatus)
		/* if success, force roaming completion */
		csr_roam_complete_roaming(mac, vdev_id, fSuccess,
								roamResult);
	else {
		if (pSession->bRefAssocStartCnt != 0) {
			QDF_ASSERT(pSession->bRefAssocStartCnt == 0);
			return;
		}
		sme_debug("indicates association completion roamResult: %d",
			roamResult);
		csr_roam_call_callback(mac, vdev_id, roam_info, roamId,
				       roamStatus, roamResult);
	}
}
#endif

static
QDF_STATUS csr_roam_lost_link(struct mac_context *mac, uint32_t sessionId,
			      uint32_t type, tSirSmeRsp *pSirMsg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct deauth_ind *pDeauthIndMsg = NULL;
	struct disassoc_ind *pDisassocIndMsg = NULL;
	eCsrRoamResult result = eCSR_ROAM_RESULT_LOSTLINK;
	struct csr_roam_info *roam_info;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	bool from_ap = false;

	if (!pSession) {
		sme_err("session: %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;
	if (eWNI_SME_DISASSOC_IND == type) {
		result = eCSR_ROAM_RESULT_DISASSOC_IND;
		pDisassocIndMsg = (struct disassoc_ind *)pSirMsg;
		pSession->roamingStatusCode = pDisassocIndMsg->status_code;
		pSession->joinFailStatusCode.reasonCode =
			pDisassocIndMsg->reasonCode;
		from_ap = pDisassocIndMsg->from_ap;
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDisassocIndMsg->peer_macaddr);
	} else if (eWNI_SME_DEAUTH_IND == type) {
		result = eCSR_ROAM_RESULT_DEAUTH_IND;
		pDeauthIndMsg = (struct deauth_ind *)pSirMsg;
		pSession->roamingStatusCode = pDeauthIndMsg->status_code;
		pSession->joinFailStatusCode.reasonCode =
			pDeauthIndMsg->reasonCode;
		from_ap = pDeauthIndMsg->from_ap;
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDeauthIndMsg->peer_macaddr);

	} else {
		sme_warn("gets an unknown type (%d)", type);
		result = eCSR_ROAM_RESULT_NONE;
		pSession->joinFailStatusCode.reasonCode = 1;
	}

	mlme_set_discon_reason_n_from_ap(mac->psoc, sessionId, from_ap,
				      pSession->joinFailStatusCode.reasonCode);

	if (eWNI_SME_DISASSOC_IND == type)
		status = csr_send_mb_disassoc_cnf_msg(mac, pDisassocIndMsg);
	else if (eWNI_SME_DEAUTH_IND == type)
		status = csr_send_mb_deauth_cnf_msg(mac, pDeauthIndMsg);

	/* prepare to tell HDD to disconnect */
	qdf_mem_zero(roam_info, sizeof(*roam_info));
	roam_info->status_code = (tSirResultCodes)pSession->roamingStatusCode;
	roam_info->reasonCode = pSession->joinFailStatusCode.reasonCode;
	if (eWNI_SME_DISASSOC_IND == type) {
		/* staMacAddr */
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDisassocIndMsg->peer_macaddr);
		roam_info->staId = (uint8_t)pDisassocIndMsg->staId;
		roam_info->reasonCode = pDisassocIndMsg->reasonCode;
	} else if (eWNI_SME_DEAUTH_IND == type) {
		/* staMacAddr */
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDeauthIndMsg->peer_macaddr);
		roam_info->staId = (uint8_t)pDeauthIndMsg->staId;
		roam_info->reasonCode = pDeauthIndMsg->reasonCode;
		roam_info->rxRssi = pDeauthIndMsg->rssi;
	}
	sme_debug("roamInfo.staId: %d", roam_info->staId);
/* Dont initiate internal driver based roaming after disconnection*/
	qdf_mem_free(roam_info);
	return status;
}

void csr_roam_get_disconnect_stats_complete(struct mac_context *mac)
{
	tListElem *entry;
	tSmeCmd *cmd;

	entry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (!entry) {
		sme_err("NO commands are ACTIVE ...");
		return;
	}

	cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
	if (cmd->command != eSmeCommandGetdisconnectStats) {
		sme_err("Get disconn stats cmd is not ACTIVE ...");
		return;
	}

	if (csr_nonscan_active_ll_remove_entry(mac, entry, LL_ACCESS_LOCK))
		csr_release_command(mac, cmd);
	else
		sme_err("Failed to release command");
}

void csr_roam_wm_status_change_complete(struct mac_context *mac,
					uint8_t session_id)
{
	tListElem *pEntry;
	tSmeCmd *pCommand;

	pEntry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (pEntry) {
		pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
		if (eSmeCommandWmStatusChange == pCommand->command) {
			/* Nothing to process in a Lost Link completion....  It just kicks off a */
			/* roaming sequence. */
			if (csr_nonscan_active_ll_remove_entry(mac, pEntry,
				    LL_ACCESS_LOCK)) {
				csr_release_command(mac, pCommand);
			} else {
				sme_err("Failed to release command");
			}
		} else {
			sme_warn("CSR: LOST LINK command is not ACTIVE ...");
		}
	} else {
		sme_warn("CSR: NO commands are ACTIVE ...");
	}
}

void csr_roam_process_get_disconnect_stats_command(struct mac_context *mac,
						   tSmeCmd *cmd)
{
	csr_get_peer_rssi(mac, cmd->vdev_id,
			  cmd->u.disconnect_stats_cmd.peer_mac_addr);
}

void csr_roam_process_wm_status_change_command(
		struct mac_context *mac, tSmeCmd *pCommand)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tSirSmeRsp *pSirSmeMsg;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac,
						pCommand->vdev_id);

	if (!pSession) {
		sme_err("session %d not found", pCommand->vdev_id);
		goto end;
	}
	sme_debug("session:%d, CmdType : %d",
		pCommand->vdev_id, pCommand->u.wmStatusChangeCmd.Type);

	switch (pCommand->u.wmStatusChangeCmd.Type) {
	case eCsrDisassociated:
		pSirSmeMsg =
			(tSirSmeRsp *) &pCommand->u.wmStatusChangeCmd.u.
			DisassocIndMsg;
		status =
			csr_roam_lost_link(mac, pCommand->vdev_id,
					   eWNI_SME_DISASSOC_IND, pSirSmeMsg);
		break;
	case eCsrDeauthenticated:
		pSirSmeMsg =
			(tSirSmeRsp *) &pCommand->u.wmStatusChangeCmd.u.
			DeauthIndMsg;
		status =
			csr_roam_lost_link(mac, pCommand->vdev_id,
					   eWNI_SME_DEAUTH_IND, pSirSmeMsg);
		break;
	default:
		sme_warn("gets an unknown command %d",
			pCommand->u.wmStatusChangeCmd.Type);
		break;
	}

end:
	if (status != QDF_STATUS_SUCCESS) {
		/*
		 * As status returned is not success, there is nothing else
		 * left to do so release WM status change command here.
		 */
		csr_roam_wm_status_change_complete(mac, pCommand->vdev_id);
	}
}

/**
 * csr_compute_mode_and_band() - computes dot11mode
 * @mac: mac global context
 * @dot11_mode: out param, do11 mode calculated
 * @band: out param, band caclculated
 * @opr_ch_freq: operating channel freq in MHz
 *
 * This function finds dot11 mode based on current mode, operating channel and
 * fw supported modes.
 *
 * Return: void
 */
static void
csr_compute_mode_and_band(struct mac_context *mac_ctx,
			  enum csr_cfgdot11mode *dot11_mode,
			  enum reg_wifi_band *band,
			  uint32_t opr_ch_freq)
{
	bool vht_24_ghz = mac_ctx->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band;

	switch (mac_ctx->roam.configParam.uCfgDot11Mode) {
	case eCSR_CFG_DOT11_MODE_11A:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11A;
		*band = REG_BAND_5G;
		break;
	case eCSR_CFG_DOT11_MODE_11B:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11B;
		*band = REG_BAND_2G;
		break;
	case eCSR_CFG_DOT11_MODE_11G:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11G;
		*band = REG_BAND_2G;
		break;
	case eCSR_CFG_DOT11_MODE_11N:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
	case eCSR_CFG_DOT11_MODE_11AC:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
	case eCSR_CFG_DOT11_MODE_11AC_ONLY:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC_ONLY;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
	case eCSR_CFG_DOT11_MODE_11AX:
	case eCSR_CFG_DOT11_MODE_11AX_ONLY:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AX)) {
			*dot11_mode = mac_ctx->roam.configParam.uCfgDot11Mode;
		} else if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
#ifdef WLAN_FEATURE_11BE
	case eCSR_CFG_DOT11_MODE_11BE:
	case eCSR_CFG_DOT11_MODE_11BE_ONLY:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11BE)) {
			*dot11_mode = mac_ctx->roam.configParam.uCfgDot11Mode;
		} else if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AX)) {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11AX;
		} else if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
#endif
	case eCSR_CFG_DOT11_MODE_AUTO:
#ifdef WLAN_FEATURE_11BE
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11BE)) {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11BE;
		} else
#endif
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AX)) {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11AX;
		} else if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band,
			 * check for INI item to disable VHT operation
			 * in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
	default:
		/*
		 * Global dot11 Mode setting is 11a/b/g. use the channel number
		 * to determine the Mode setting.
		 */
		if (eCSR_OPERATING_CHANNEL_AUTO == opr_ch_freq) {
			*band = (mac_ctx->mlme_cfg->gen.band == BAND_2G ?
				REG_BAND_2G : REG_BAND_5G);
			if (REG_BAND_2G == *band) {
				*dot11_mode = eCSR_CFG_DOT11_MODE_11B;
			} else {
				/* prefer 5GHz */
				*band = REG_BAND_5G;
				*dot11_mode = eCSR_CFG_DOT11_MODE_11A;
			}
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq)) {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11B;
			*band = REG_BAND_2G;
		} else {
			/* else, it's a 5.0GHz channel.  Set mode to 11a. */
			*dot11_mode = eCSR_CFG_DOT11_MODE_11A;
			*band = REG_BAND_5G;
		}
		break;
	} /* switch */
}

/**
 * csr_roam_get_phy_mode_band_for_bss() - This function returns band and mode
 * information.
 * @mac_ctx:       mac global context
 * @profile:       bss profile
 * @bss_op_ch_freq:operating channel freq in MHz
 * @band:          out param, band caclculated
 *
 * This function finds dot11 mode based on current mode, operating channel and
 * fw supported modes. The only tricky part is that if phyMode is set to 11abg,
 * this function may return eCSR_CFG_DOT11_MODE_11B instead of
 * eCSR_CFG_DOT11_MODE_11G if everything is set to auto-pick.
 *
 * Return: dot11mode
 */
static enum csr_cfgdot11mode
csr_roam_get_phy_mode_band_for_bss(struct mac_context *mac_ctx,
				   struct csr_roam_profile *profile,
				   uint32_t bss_op_ch_freq,
				   enum reg_wifi_band *p_band)
{
	enum reg_wifi_band band = REG_BAND_2G;
	uint8_t opr_freq = 0;
	enum csr_cfgdot11mode curr_mode =
		mac_ctx->roam.configParam.uCfgDot11Mode;
	enum csr_cfgdot11mode cfg_dot11_mode =
		csr_get_cfg_dot11_mode_from_csr_phy_mode(
			profile,
			(eCsrPhyMode) profile->phyMode,
			mac_ctx->roam.configParam.ProprietaryRatesEnabled);

	if (bss_op_ch_freq)
		opr_freq = bss_op_ch_freq;
	/*
	 * If the global setting for dot11Mode is set to auto/abg, we overwrite
	 * the setting in the profile.
	 */
	if ((!CSR_IS_INFRA_AP(profile)
	    && ((eCSR_CFG_DOT11_MODE_AUTO == curr_mode)
	    || (eCSR_CFG_DOT11_MODE_ABG == curr_mode)))
	    || (eCSR_CFG_DOT11_MODE_AUTO == cfg_dot11_mode)
	    || (eCSR_CFG_DOT11_MODE_ABG == cfg_dot11_mode)) {
		csr_compute_mode_and_band(mac_ctx, &cfg_dot11_mode,
					  &band, bss_op_ch_freq);
	} /* if( eCSR_CFG_DOT11_MODE_ABG == cfg_dot11_mode ) */
	else {
		/* dot11 mode is set, lets pick the band */
		if (0 == opr_freq) {
			/* channel is Auto also. */
			if (mac_ctx->mlme_cfg->gen.band == BAND_ALL) {
				/* prefer 5GHz */
				band = REG_BAND_5G;
			}
		} else{
			band = wlan_reg_freq_to_band(bss_op_ch_freq);
		}
	}
	if (p_band)
		*p_band = band;

	if (opr_freq == 2484 && wlan_reg_is_24ghz_ch_freq(bss_op_ch_freq)) {
		sme_err("Switching to Dot11B mode");
		cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11B;
	}

	if (wlan_reg_is_24ghz_ch_freq(bss_op_ch_freq) &&
	    !mac_ctx->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band &&
	    (eCSR_CFG_DOT11_MODE_11AC == cfg_dot11_mode ||
	    eCSR_CFG_DOT11_MODE_11AC_ONLY == cfg_dot11_mode))
		cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11N;
	/*
	 * Incase of WEP Security encryption type is coming as part of add key.
	 * So while STart BSS dont have information
	 */
	if ((!CSR_IS_11n_ALLOWED(profile->EncryptionType.encryptionType[0])
	    || ((profile->privacy == 1)
		&& (profile->EncryptionType.encryptionType[0] ==
		eCSR_ENCRYPT_TYPE_NONE)))
		&& ((eCSR_CFG_DOT11_MODE_11N == cfg_dot11_mode) ||
		    (eCSR_CFG_DOT11_MODE_11AC == cfg_dot11_mode) ||
		    (eCSR_CFG_DOT11_MODE_11AX == cfg_dot11_mode) ||
		    CSR_IS_CFG_DOT11_PHY_MODE_11BE(cfg_dot11_mode))) {
		/* We cannot do 11n here */
		if (wlan_reg_is_24ghz_ch_freq(bss_op_ch_freq))
			cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11G;
		else
			cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11A;
	}
	sme_debug("dot11mode: %d phyMode %d fw sup AX %d", cfg_dot11_mode,
		  profile->phyMode, IS_FEATURE_SUPPORTED_BY_FW(DOT11AX));
#ifdef WLAN_FEATURE_11BE
	sme_debug("BE :%d", IS_FEATURE_SUPPORTED_BY_FW(DOT11BE));
#endif
	return cfg_dot11_mode;
}

QDF_STATUS csr_roam_issue_stop_bss(struct mac_context *mac,
		uint32_t sessionId, enum csr_roam_substate NewSubstate)
{
	QDF_STATUS status;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	/* Set the roaming substate to 'stop Bss request'... */
	csr_roam_substate_change(mac, NewSubstate, sessionId);

	/* attempt to stop the Bss (reason code is ignored...) */
	status = csr_send_mb_stop_bss_req_msg(mac, sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_warn(
			"csr_send_mb_stop_bss_req_msg failed with status %d",
			status);
	}
	return status;
}

QDF_STATUS csr_get_cfg_valid_channels(struct mac_context *mac,
				      qdf_freq_t *ch_freq_list,
				      uint32_t *num_ch_freq)
{
	uint8_t num_chan_temp = 0;
	int i;
	uint32_t *valid_ch_freq_list =
				mac->mlme_cfg->reg.valid_channel_freq_list;

	*num_ch_freq = mac->mlme_cfg->reg.valid_channel_list_num;

	for (i = 0; i < *num_ch_freq; i++) {
		if (!wlan_reg_is_dsrc_freq(valid_ch_freq_list[i])) {
			ch_freq_list[num_chan_temp] = valid_ch_freq_list[i];
			num_chan_temp++;
		}
	}

	*num_ch_freq = num_chan_temp;
	return QDF_STATUS_SUCCESS;
}

int8_t csr_get_cfg_max_tx_power(struct mac_context *mac, uint32_t ch_freq)
{
	return wlan_get_cfg_max_tx_power(mac->psoc, mac->pdev, ch_freq);
}

#ifndef FEATURE_CM_ENABLE
static bool csr_is_encryption_in_list(struct mac_context *mac,
				      tCsrEncryptionList *pCipherList,
				      eCsrEncryptionType encryptionType)
{
	bool fFound = false;
	uint32_t idx;

	for (idx = 0; idx < pCipherList->numEntries; idx++) {
		if (pCipherList->encryptionType[idx] == encryptionType) {
			fFound = true;
			break;
		}
	}
	return fFound;
}

static bool csr_is_auth_in_list(struct mac_context *mac, tCsrAuthList *pAuthList,
				enum csr_akm_type authType)
{
	bool fFound = false;
	uint32_t idx;

	for (idx = 0; idx < pAuthList->numEntries; idx++) {
		if (pAuthList->authType[idx] == authType) {
			fFound = true;
			break;
		}
	}
	return fFound;
}

bool csr_is_same_profile(struct mac_context *mac,
			 tCsrRoamConnectedProfile *pProfile1,
			 struct csr_roam_profile *pProfile2,
			 uint8_t vdev_id)
{
	uint32_t i;
	bool fCheck = false;
	struct cm_roam_values_copy cfg;
	enum QDF_OPMODE opmode =
		wlan_get_opmode_from_vdev_id(mac->pdev, vdev_id);
	struct wlan_ssid ssid;

	if (!(pProfile1 && pProfile2))
		return fCheck;

	wlan_mlme_get_ssid_vdev_id(mac->pdev, vdev_id,
				   ssid.ssid, &ssid.length);

	for (i = 0; i < pProfile2->SSIDs.numOfSSIDs; i++) {
		fCheck = csr_is_ssid_match(mac,
				pProfile2->SSIDs.SSIDList[i].SSID.ssId,
				pProfile2->SSIDs.SSIDList[i].SSID.length,
				ssid.ssid, ssid.length, false);
		if (fCheck)
			break;
	}
	if (!fCheck)
		goto exit;

	if (!csr_is_auth_in_list(mac, &pProfile2->AuthType,
				 pProfile1->AuthType)
	    || (pProfile2->csrPersona != opmode)
	    || !csr_is_encryption_in_list(mac, &pProfile2->EncryptionType,
					  pProfile1->EncryptionType)) {
		fCheck = false;
		goto exit;
	}
	wlan_cm_roam_cfg_get_value(mac->psoc, vdev_id,
				   MOBILITY_DOMAIN, &cfg);
	if (cfg.bool_value || pProfile2->mdid.mdie_present) {
		if (cfg.uint_value !=
		    pProfile2->mdid.mobility_domain) {
			fCheck = false;
			goto exit;
		}
	}
	/* Match found */
	fCheck = true;
exit:
	return fCheck;
}

static bool csr_roam_is_same_profile_keys(struct mac_context *mac,
				   tCsrRoamConnectedProfile *pConnProfile,
				   struct csr_roam_profile *pProfile2)
{
	bool fCheck = false;

	do {
		/* Only check for static WEP */
		if (!csr_is_encryption_in_list
			    (mac, &pProfile2->EncryptionType,
			    eCSR_ENCRYPT_TYPE_WEP40_STATICKEY)
		    && !csr_is_encryption_in_list(mac,
				&pProfile2->EncryptionType,
				eCSR_ENCRYPT_TYPE_WEP104_STATICKEY)) {
			fCheck = true;
			break;
		}
		if (!csr_is_encryption_in_list
			    (mac, &pProfile2->EncryptionType,
			    pConnProfile->EncryptionType))
			break;

		fCheck = true;
	} while (0);
	return fCheck;
}
#endif

/**
 * csr_populate_basic_rates() - populates OFDM or CCK rates
 * @rates:         rate struct to populate
 * @is_ofdm_rates:          true: ofdm rates, false: cck rates
 * @is_basic_rates:        indicates if rates are to be masked with
 *                 CSR_DOT11_BASIC_RATE_MASK
 *
 * This function will populate OFDM or CCK rates
 *
 * Return: void
 */
static void
csr_populate_basic_rates(tSirMacRateSet *rate_set, bool is_ofdm_rates,
			 bool is_basic_rates)
{
	wlan_populate_basic_rates(rate_set, is_ofdm_rates, is_basic_rates);
}

/**
 * csr_convert_mode_to_nw_type() - convert mode into network type
 * @dot11_mode:    dot11_mode
 * @band:          2.4 or 5 GHz
 *
 * Return: tSirNwType
 */
static tSirNwType
csr_convert_mode_to_nw_type(enum csr_cfgdot11mode dot11_mode,
			    enum reg_wifi_band band)
{
	switch (dot11_mode) {
	case eCSR_CFG_DOT11_MODE_11G:
		return eSIR_11G_NW_TYPE;
	case eCSR_CFG_DOT11_MODE_11B:
		return eSIR_11B_NW_TYPE;
	case eCSR_CFG_DOT11_MODE_11A:
		return eSIR_11A_NW_TYPE;
	case eCSR_CFG_DOT11_MODE_11N:
	default:
		/*
		 * Because LIM only verifies it against 11a, 11b or 11g, set
		 * only 11g or 11a here
		 */
		if (REG_BAND_2G == band)
			return eSIR_11G_NW_TYPE;
		else
			return eSIR_11A_NW_TYPE;
	}
	return eSIR_DONOT_USE_NW_TYPE;
}

/**
 * csr_populate_supported_rates_from_hostapd() - populates operational
 * and extended rates.
 * from hostapd.conf file
 * @opr_rates:		rate struct to populate operational rates
 * @ext_rates:		rate struct to populate extended rates
 * @profile:		bss profile
 *
 * Return: void
 */
static void csr_populate_supported_rates_from_hostapd(tSirMacRateSet *opr_rates,
		tSirMacRateSet *ext_rates,
		struct csr_roam_profile *profile)
{
	int i = 0;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("supported_rates: %d extended_rates: %d"),
			profile->supported_rates.numRates,
			profile->extended_rates.numRates);

	if (profile->supported_rates.numRates > WLAN_SUPPORTED_RATES_IE_MAX_LEN)
		profile->supported_rates.numRates =
			WLAN_SUPPORTED_RATES_IE_MAX_LEN;

	if (profile->extended_rates.numRates > SIR_MAC_MAX_NUMBER_OF_RATES)
		profile->extended_rates.numRates =
			SIR_MAC_MAX_NUMBER_OF_RATES;

	if (profile->supported_rates.numRates) {
		opr_rates->numRates = profile->supported_rates.numRates;
		qdf_mem_copy(opr_rates->rate,
				profile->supported_rates.rate,
				profile->supported_rates.numRates);
		for (i = 0; i < opr_rates->numRates; i++)
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("Supported Rate is %2x"), opr_rates->rate[i]);
	}
	if (profile->extended_rates.numRates) {
		ext_rates->numRates =
			profile->extended_rates.numRates;
		qdf_mem_copy(ext_rates->rate,
				profile->extended_rates.rate,
				profile->extended_rates.numRates);
		for (i = 0; i < ext_rates->numRates; i++)
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("Extended Rate is %2x"), ext_rates->rate[i]);
	}
}

/**
 * csr_roam_get_bss_start_parms() - get bss start param from profile
 * @mac:          mac global context
 * @pProfile:      roam profile
 * @pParam:        out param, start bss params
 * @skip_hostapd_rate: to skip given hostapd's rate
 *
 * This function populates start bss param from roam profile
 *
 * Return: void
 */
static QDF_STATUS
csr_roam_get_bss_start_parms(struct mac_context *mac,
			     struct csr_roam_profile *pProfile,
			     struct csr_roamstart_bssparams *pParam,
			     bool skip_hostapd_rate)
{
	enum reg_wifi_band band;
	uint32_t opr_ch_freq = 0;
	tSirNwType nw_type;
	uint32_t tmp_opr_ch_freq = 0;
	tSirMacRateSet *opr_rates = &pParam->operationalRateSet;
	tSirMacRateSet *ext_rates = &pParam->extendedRateSet;

	if (pProfile->ChannelInfo.numOfChannels &&
	    pProfile->ChannelInfo.freq_list)
		tmp_opr_ch_freq = pProfile->ChannelInfo.freq_list[0];

	pParam->uCfgDot11Mode =
		csr_roam_get_phy_mode_band_for_bss(mac, pProfile,
						   tmp_opr_ch_freq,
						   &band);

	if (((pProfile->csrPersona == QDF_P2P_CLIENT_MODE)
	    || (pProfile->csrPersona == QDF_P2P_GO_MODE))
	    && (pParam->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11B)) {
		/* This should never happen */
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_FATAL,
			 "For P2P (persona %d) dot11_mode is 11B",
			  pProfile->csrPersona);
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	nw_type = csr_convert_mode_to_nw_type(pParam->uCfgDot11Mode, band);
	ext_rates->numRates = 0;
	/*
	 * hostapd.conf will populate its basic and extended rates
	 * as per hw_mode, but if acs in ini is enabled driver should
	 * ignore basic and extended rates from hostapd.conf and should
	 * populate default rates.
	 */
	if (!cds_is_sub_20_mhz_enabled() && !skip_hostapd_rate &&
			(pProfile->supported_rates.numRates ||
			pProfile->extended_rates.numRates)) {
		csr_populate_supported_rates_from_hostapd(opr_rates,
				ext_rates, pProfile);
		pParam->operation_chan_freq = tmp_opr_ch_freq;
	} else {
		switch (nw_type) {
		default:
			sme_err(
				"sees an unknown pSirNwType (%d)",
				nw_type);
			return QDF_STATUS_E_INVAL;
		case eSIR_11A_NW_TYPE:
			csr_populate_basic_rates(opr_rates, true, true);
			if (eCSR_OPERATING_CHANNEL_ANY != tmp_opr_ch_freq) {
				opr_ch_freq = tmp_opr_ch_freq;
				break;
			}
			if (0 == opr_ch_freq &&
				CSR_IS_PHY_MODE_DUAL_BAND(pProfile->phyMode) &&
				CSR_IS_PHY_MODE_DUAL_BAND(
					mac->roam.configParam.phyMode)) {
				/*
				 * We could not find a 5G channel by auto pick,
				 * let's try 2.4G channels. We only do this here
				 * because csr_roam_get_phy_mode_band_for_bss
				 * always picks 11a  for AUTO
				 */
				nw_type = eSIR_11B_NW_TYPE;
				csr_populate_basic_rates(opr_rates, false, true);
			}
			break;
		case eSIR_11B_NW_TYPE:
			csr_populate_basic_rates(opr_rates, false, true);
			if (eCSR_OPERATING_CHANNEL_ANY != tmp_opr_ch_freq)
				opr_ch_freq = tmp_opr_ch_freq;
			break;
		case eSIR_11G_NW_TYPE:
			/* For P2P Client and P2P GO, disable 11b rates */
			if ((pProfile->csrPersona == QDF_P2P_CLIENT_MODE) ||
				(pProfile->csrPersona == QDF_P2P_GO_MODE) ||
				(eCSR_CFG_DOT11_MODE_11G_ONLY ==
					pParam->uCfgDot11Mode)) {
				csr_populate_basic_rates(opr_rates, true, true);
			} else {
				csr_populate_basic_rates(opr_rates, false,
								true);
				csr_populate_basic_rates(ext_rates, true,
								false);
			}
			if (eCSR_OPERATING_CHANNEL_ANY != tmp_opr_ch_freq)
				opr_ch_freq = tmp_opr_ch_freq;
			break;
		}
		pParam->operation_chan_freq = opr_ch_freq;
	}

	pParam->sirNwType = nw_type;
	pParam->ch_params.ch_width = pProfile->ch_params.ch_width;
	pParam->ch_params.center_freq_seg0 =
		pProfile->ch_params.center_freq_seg0;
	pParam->ch_params.center_freq_seg1 =
		pProfile->ch_params.center_freq_seg1;
	pParam->ch_params.sec_ch_offset =
		pProfile->ch_params.sec_ch_offset;
	return QDF_STATUS_SUCCESS;
}

static void
csr_roam_get_bss_start_parms_from_bss_desc(
					struct mac_context *mac,
					struct bss_description *bss_desc,
					tDot11fBeaconIEs *pIes,
					struct csr_roamstart_bssparams *pParam)
{
	if (!pParam) {
		sme_err("BSS param's pointer is NULL");
		return;
	}

	pParam->sirNwType = bss_desc->nwType;
	pParam->cbMode = PHY_SINGLE_CHANNEL_CENTERED;
	pParam->operation_chan_freq = bss_desc->chan_freq;
	qdf_mem_copy(&pParam->bssid, bss_desc->bssId,
						sizeof(struct qdf_mac_addr));

	if (!pIes) {
		pParam->ssId.length = 0;
		pParam->operationalRateSet.numRates = 0;
		sme_err("IEs struct pointer is NULL");
		return;
	}

	if (pIes->SuppRates.present) {
		pParam->operationalRateSet.numRates = pIes->SuppRates.num_rates;
		if (pIes->SuppRates.num_rates > WLAN_SUPPORTED_RATES_IE_MAX_LEN) {
			sme_err(
				"num_rates: %d > max val, resetting",
				pIes->SuppRates.num_rates);
			pIes->SuppRates.num_rates =
				WLAN_SUPPORTED_RATES_IE_MAX_LEN;
		}
		qdf_mem_copy(pParam->operationalRateSet.rate,
			     pIes->SuppRates.rates,
			     sizeof(*pIes->SuppRates.rates) *
			     pIes->SuppRates.num_rates);
	}
	if (pIes->ExtSuppRates.present) {
		pParam->extendedRateSet.numRates = pIes->ExtSuppRates.num_rates;
		if (pIes->ExtSuppRates.num_rates >
		    SIR_MAC_MAX_NUMBER_OF_RATES) {
			sme_err("num_rates: %d > max val, resetting",
				pIes->ExtSuppRates.num_rates);
			pIes->ExtSuppRates.num_rates =
				SIR_MAC_MAX_NUMBER_OF_RATES;
		}
		qdf_mem_copy(pParam->extendedRateSet.rate,
			     pIes->ExtSuppRates.rates,
			     sizeof(*pIes->ExtSuppRates.rates) *
			     pIes->ExtSuppRates.num_rates);
	}
	if (pIes->SSID.present) {
		pParam->ssId.length = pIes->SSID.num_ssid;
		qdf_mem_copy(pParam->ssId.ssId, pIes->SSID.ssid,
			     pParam->ssId.length);
	}
	pParam->cbMode =
		wlan_get_cb_mode(mac, pParam->operation_chan_freq, pIes);
}

static void csr_roam_determine_max_rate_for_ad_hoc(struct mac_context *mac,
						   tSirMacRateSet *pSirRateSet)
{
	uint8_t MaxRate = 0;
	uint32_t i;
	uint8_t *pRate;

	pRate = pSirRateSet->rate;
	for (i = 0; i < pSirRateSet->numRates; i++) {
		MaxRate = QDF_MAX(MaxRate, (pRate[i] &
					(~CSR_DOT11_BASIC_RATE_MASK)));
	}

	/* Save the max rate in the connected state information.
	 * modify LastRates variable as well
	 */

}

QDF_STATUS csr_roam_issue_start_bss(struct mac_context *mac, uint32_t sessionId,
				    struct csr_roamstart_bssparams *pParam,
				    struct csr_roam_profile *pProfile,
				    struct bss_description *bss_desc,
					uint32_t roamId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum reg_wifi_band band;
	/* Set the roaming substate to 'Start BSS attempt'... */
	csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_START_BSS_REQ,
				 sessionId);
	/* Put RSN information in for Starting BSS */
	pParam->nRSNIELength = (uint16_t) pProfile->nRSNReqIELength;
	pParam->pRSNIE = pProfile->pRSNReqIE;

	pParam->privacy = pProfile->privacy;
	pParam->fwdWPSPBCProbeReq = pProfile->fwdWPSPBCProbeReq;
	pParam->authType = pProfile->csr80211AuthType;
	pParam->beaconInterval = pProfile->beaconInterval;
	pParam->dtimPeriod = pProfile->dtimPeriod;
	pParam->ApUapsdEnable = pProfile->ApUapsdEnable;
	pParam->ssidHidden = pProfile->SSIDs.SSIDList[0].ssidHidden;
	if (CSR_IS_INFRA_AP(pProfile) && (pParam->operation_chan_freq != 0)) {
		if (!wlan_reg_is_freq_present_in_cur_chan_list(mac->pdev,
						pParam->operation_chan_freq)) {
			pParam->operation_chan_freq = INFRA_AP_DEFAULT_CHAN_FREQ;
			pParam->ch_params.ch_width = CH_WIDTH_20MHZ;
		}
	}
	pParam->protEnabled = pProfile->protEnabled;
	pParam->obssProtEnabled = pProfile->obssProtEnabled;
	pParam->ht_protection = pProfile->cfg_protection;
	pParam->wps_state = pProfile->wps_state;

	pParam->uCfgDot11Mode =
		csr_roam_get_phy_mode_band_for_bss(mac, pProfile,
						   pParam->operation_chan_freq,
						   &band);
	pParam->bssPersona = pProfile->csrPersona;

	pParam->mfpCapable = (0 != pProfile->MFPCapable);
	pParam->mfpRequired = (0 != pProfile->MFPRequired);

	pParam->add_ie_params.probeRespDataLen =
		pProfile->add_ie_params.probeRespDataLen;
	pParam->add_ie_params.probeRespData_buff =
		pProfile->add_ie_params.probeRespData_buff;

	pParam->add_ie_params.assocRespDataLen =
		pProfile->add_ie_params.assocRespDataLen;
	pParam->add_ie_params.assocRespData_buff =
		pProfile->add_ie_params.assocRespData_buff;

	pParam->add_ie_params.probeRespBCNDataLen =
		pProfile->add_ie_params.probeRespBCNDataLen;
	pParam->add_ie_params.probeRespBCNData_buff =
		pProfile->add_ie_params.probeRespBCNData_buff;

	if (pProfile->csrPersona == QDF_SAP_MODE)
		pParam->sap_dot11mc = mac->mlme_cfg->gen.sap_dot11mc;
	else
		pParam->sap_dot11mc = 1;

	sme_debug("11MC Support Enabled : %d", pParam->sap_dot11mc);

	pParam->cac_duration_ms = pProfile->cac_duration_ms;
	pParam->dfs_regdomain = pProfile->dfs_regdomain;
	pParam->beacon_tx_rate = pProfile->beacon_tx_rate;

	status = csr_send_mb_start_bss_req_msg(mac, sessionId,
						pProfile->BSSType, pParam,
					      bss_desc);
	return status;
}

void csr_roam_prepare_bss_params(struct mac_context *mac, uint32_t sessionId,
					struct csr_roam_profile *pProfile,
					struct bss_description *bss_desc,
					struct bss_config_param *pBssConfig,
					tDot11fBeaconIEs *pIes)
{
	ePhyChanBondState cbMode = PHY_SINGLE_CHANNEL_CENTERED;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	bool skip_hostapd_rate = !pProfile->chan_switch_hostapd_rate_enabled;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return;
	}

	if (bss_desc) {
		csr_roam_get_bss_start_parms_from_bss_desc(mac, bss_desc, pIes,
							  &pSession->bssParams);
		if (CSR_IS_NDI(pProfile)) {
			qdf_copy_macaddr(&pSession->bssParams.bssid,
				&pSession->self_mac_addr);
		}
	} else {
		csr_roam_get_bss_start_parms(mac, pProfile,
					     &pSession->bssParams,
					     skip_hostapd_rate);
		/* Use the first SSID */
		if (pProfile->SSIDs.numOfSSIDs)
			qdf_mem_copy(&pSession->bssParams.ssId,
				     pProfile->SSIDs.SSIDList,
				     sizeof(tSirMacSSid));
		if (pProfile->BSSIDs.numOfBSSIDs)
			/* Use the first BSSID */
			qdf_mem_copy(&pSession->bssParams.bssid,
				     pProfile->BSSIDs.bssid,
				     sizeof(struct qdf_mac_addr));
		else
			qdf_mem_zero(&pSession->bssParams.bssid,
				    sizeof(struct qdf_mac_addr));
	}
	/* Set operating frequency in pProfile which will be used */
	/* in csr_roam_set_bss_config_cfg() to determine channel bonding */
	/* mode and will be configured in CFG later */
	pProfile->op_freq = pSession->bssParams.operation_chan_freq;

	if (pProfile->op_freq == 0)
		sme_err("CSR cannot find a channel to start");
	else {
		csr_roam_determine_max_rate_for_ad_hoc(mac,
						       &pSession->bssParams.
						       operationalRateSet);
		if (CSR_IS_INFRA_AP(pProfile)) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(pProfile->op_freq)) {
				cbMode =
					mac->roam.configParam.
					channelBondingMode24GHz;
			} else {
				cbMode =
					mac->roam.configParam.
					channelBondingMode5GHz;
			}
			sme_debug("## cbMode %d", cbMode);
			pBssConfig->cbMode = cbMode;
			pSession->bssParams.cbMode = cbMode;
		}
	}
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
void csr_get_pmk_info(struct mac_context *mac_ctx, uint8_t session_id,
		      struct wlan_crypto_pmksa *pmk_cache)
{
	if (!mac_ctx) {
		sme_err("Mac_ctx is NULL");
		return;
	}
	wlan_cm_get_psk_pmk(mac_ctx->pdev, session_id, pmk_cache->pmk,
			    &pmk_cache->pmk_len);
}

QDF_STATUS csr_roam_set_psk_pmk(struct mac_context *mac, uint8_t vdev_id,
				uint8_t *psk_pmk, size_t pmk_len,
				bool update_to_fw)
{
	int32_t akm;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	wlan_cm_set_psk_pmk(mac->pdev, vdev_id, psk_pmk, pmk_len);
	akm = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_CCKM)) {
		sme_debug("PMK update is not required for ESE");
		return QDF_STATUS_SUCCESS;
	}

	if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X) ||
	    QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA256) ||
	    QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FILS_SHA384) ||
	    QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X_SHA384)) {
		sme_debug("Auth type: %x update the MDID in cache", akm);
		cm_update_pmk_cache_ft(mac->psoc, vdev_id);
	}

	if (update_to_fw)
		wlan_roam_update_cfg(mac->psoc, vdev_id,
				     REASON_ROAM_PSK_PMK_CHANGED);

	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
void csr_clear_sae_single_pmk(struct wlan_objmgr_psoc *psoc,
			      uint8_t vdev_id,
			      struct wlan_crypto_pmksa *pmk_cache)
{
	struct wlan_objmgr_vdev *vdev;
	int32_t keymgmt;
	struct mlme_pmk_info pmk_info;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL");
		return;
	}

	keymgmt = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (keymgmt < 0) {
		sme_err("Invalid mgmt cipher");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}

	if (!(keymgmt & (1 << WLAN_CRYPTO_KEY_MGMT_SAE))) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	if (pmk_cache) {
		qdf_mem_copy(&pmk_info.pmk, pmk_cache->pmk, pmk_cache->pmk_len);
		pmk_info.pmk_len = pmk_cache->pmk_len;
		wlan_mlme_clear_sae_single_pmk_info(vdev, &pmk_info);
	} else {
		wlan_mlme_clear_sae_single_pmk_info(vdev, NULL);
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}
#endif

#ifndef FEATURE_CM_ENABLE
QDF_STATUS csr_roam_get_wpa_rsn_req_ie(struct mac_context *mac, uint32_t sessionId,
				       uint32_t *pLen, uint8_t *pBuf)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	uint32_t len;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (pLen) {
		len = *pLen;
		*pLen = pSession->nWpaRsnReqIeLength;
		if (pBuf) {
			if (len >= pSession->nWpaRsnReqIeLength) {
				qdf_mem_copy(pBuf, pSession->pWpaRsnReqIE,
					     pSession->nWpaRsnReqIeLength);
				status = QDF_STATUS_SUCCESS;
			}
		}
	}
	return status;
}

eRoamCmdStatus csr_get_roam_complete_status(struct mac_context *mac,
						uint32_t sessionId)
{
	eRoamCmdStatus retStatus = eCSR_ROAM_CONNECT_COMPLETION;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return retStatus;
	}

	if (CSR_IS_ROAMING(pSession))
		retStatus = eCSR_ROAM_ROAMING_COMPLETION;

	return retStatus;
}
#endif

static QDF_STATUS csr_roam_start_wds(struct mac_context *mac, uint32_t sessionId,
				     struct csr_roam_profile *pProfile,
				     struct bss_description *bss_desc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	struct bss_config_param bssConfig;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (csr_is_conn_state_wds(mac, sessionId)) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_zero(&bssConfig, sizeof(struct bss_config_param));
	/* Assume HDD provide bssid in profile */
	qdf_copy_macaddr(&pSession->bssParams.bssid,
			 pProfile->BSSIDs.bssid);
	/* there is no Bss description before we start an WDS so we
	 * need to adopt all Bss configuration parameters from the
	 * Profile.
	 */
	status = csr_roam_prepare_bss_config_from_profile(mac,
							pProfile,
							&bssConfig,
							bss_desc);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* Save profile for late use */
		csr_free_roam_profile(mac, sessionId);
		pSession->pCurRoamProfile =
			qdf_mem_malloc(sizeof(struct csr_roam_profile));
		if (pSession->pCurRoamProfile) {
			csr_roam_copy_profile(mac,
					      pSession->pCurRoamProfile,
					      pProfile, sessionId);
		}
		/* Prepare some more parameters for this WDS */
		csr_roam_prepare_bss_params(mac, sessionId, pProfile,
					NULL, &bssConfig, NULL);
		status = csr_roam_set_bss_config_cfg(mac, sessionId,
						pProfile, NULL,
						&bssConfig, NULL,
						false);
	}

	return status;
}

#ifdef FEATURE_WLAN_ESE
void csr_update_prev_ap_info(struct csr_roam_session *session,
			     struct wlan_objmgr_vdev *vdev)
{
	struct wlan_ssid ssid;
	QDF_STATUS status;
	enum QDF_OPMODE opmode;
	struct rso_config *rso_cfg;

	opmode = wlan_vdev_mlme_get_opmode(vdev);
	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg)
		return;

	if (!rso_cfg->is_ese_assoc || opmode != QDF_STA_MODE)
		return;
	status = wlan_vdev_mlme_get_ssid(vdev, ssid.ssid, &ssid.length);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err(" failed to find SSID for vdev %d", session->vdev_id);
		return;
	}
	session->isPrevApInfoValid = true;
	session->roamTS1 = qdf_mc_timer_get_system_time();
}
#endif

#ifdef FEATURE_CM_ENABLE
#ifdef WLAN_FEATURE_FILS_SK
static QDF_STATUS csr_cm_update_fils_info(struct wlan_objmgr_vdev *vdev,
					  struct bss_description *bss_desc,
					  struct wlan_cm_vdev_connect_req *req)
{
	uint8_t cache_id[CACHE_ID_LEN] = {0};
	struct scan_cache_entry *entry;
	struct wlan_crypto_pmksa *fils_ssid_pmksa, *bssid_lookup_pmksa;

	if (!req->fils_info || !req->fils_info->is_fils_connection) {
		wlan_cm_update_mlme_fils_info(vdev, NULL);
		return QDF_STATUS_SUCCESS;
	}

	if (bss_desc->fils_info_element.is_cache_id_present) {
		qdf_mem_copy(cache_id, bss_desc->fils_info_element.cache_id,
			     CACHE_ID_LEN);
		sme_debug("FILS_PMKSA: cache_id[0]:%d, cache_id[1]:%d",
			  cache_id[0], cache_id[1]);
	}
	entry = req->bss->entry;
	bssid_lookup_pmksa = wlan_crypto_get_pmksa(vdev, &entry->bssid);
	fils_ssid_pmksa =
			wlan_crypto_get_fils_pmksa(vdev, cache_id,
						   entry->ssid.ssid,
						   entry->ssid.length);

	if ((!req->fils_info->rrk_len ||
	     !req->fils_info->username_len) &&
	     !bss_desc->fils_info_element.is_cache_id_present &&
	     !bssid_lookup_pmksa && !fils_ssid_pmksa)
		return QDF_STATUS_E_FAILURE;

	return wlan_cm_update_mlme_fils_info(vdev, req->fils_info);
}
#else
static inline
QDF_STATUS csr_cm_update_fils_info(struct wlan_objmgr_vdev *vdev,
				   struct bss_description *bss_desc,
				   struct wlan_cm_vdev_connect_req *req)
{
}
#endif

#if defined(WLAN_FEATURE_HOST_ROAM) && defined(FEATURE_WLAN_ESE)
static void csr_update_tspec_info(struct mac_context *mac_ctx,
				  struct wlan_objmgr_vdev *vdev,
				  tDot11fBeaconIEs *ie_struct)
{
	struct mlme_legacy_priv *mlme_priv;
	tESETspecInfo *ese_tspec;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv)
		return;
	if (!cm_is_ese_connection(vdev, ie_struct->ESEVersion.present))
		return;

	ese_tspec = &mlme_priv->connect_info.ese_tspec_info;
	qdf_mem_zero(ese_tspec, sizeof(tESETspecInfo));
	ese_tspec->numTspecs = sme_qos_ese_retrieve_tspec_info(mac_ctx,
					wlan_vdev_get_id(vdev),
					ese_tspec->tspec);
}
#else
static inline void csr_update_tspec_info(struct mac_context *mac_ctx,
					 struct wlan_objmgr_vdev *vdev,
					 tDot11fBeaconIEs *ie_struct) {}
#endif

QDF_STATUS cm_csr_handle_join_req(struct wlan_objmgr_vdev *vdev,
				  struct wlan_cm_vdev_connect_req *req,
				  struct cm_vdev_join_req *join_req,
				  bool reassoc)
{
	struct mac_context *mac_ctx;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	QDF_STATUS status;
	tDot11fBeaconIEs *ie_struct;
	struct bss_description *bss_desc;
	uint32_t ie_len, bss_len;

	/*
	 * This API is to update legacy struct and should be removed once
	 * CSR is cleaned up fully. No new params should be added to CSR, use
	 * vdev/pdev/psoc instead
	 */
	mac_ctx = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	ie_len = util_scan_entry_ie_len(join_req->entry);
	bss_len = (uint16_t)(offsetof(struct bss_description,
			   ieFields[0]) + ie_len);

	bss_desc = qdf_mem_malloc(sizeof(*bss_desc) + bss_len);
	if (!bss_desc)
		return QDF_STATUS_E_NOMEM;

	status = wlan_fill_bss_desc_from_scan_entry(mac_ctx, bss_desc,
						    join_req->entry);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(bss_desc);
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_get_parsed_bss_description_ies(mac_ctx, bss_desc,
						     &ie_struct);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("IE parsing failed vdev id %d", vdev_id);
		qdf_mem_free(bss_desc);
		return QDF_STATUS_E_FAILURE;
	}

	if (reassoc) {
		csr_update_tspec_info(mac_ctx, vdev, ie_struct);
	} else {
		status = csr_cm_update_fils_info(vdev, bss_desc, req);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("failed to update fils info vdev id %d",
				vdev_id);
			qdf_mem_free(ie_struct);
			qdf_mem_free(bss_desc);
			return QDF_STATUS_E_FAILURE;
		}
		sme_qos_csr_event_ind(mac_ctx, vdev_id,
				      SME_QOS_CSR_JOIN_REQ, NULL);
	}

	csr_set_qos_to_cfg(mac_ctx, vdev_id,
			   csr_get_qos_from_bss_desc(mac_ctx, bss_desc,
						     ie_struct));

	if ((wlan_reg_11d_enabled_on_host(mac_ctx->psoc)) &&
	     !ie_struct->Country.present)
		csr_apply_channel_power_info_wrapper(mac_ctx);

	qdf_mem_free(ie_struct);
	qdf_mem_free(bss_desc);

	cm_csr_set_joining(vdev_id);

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_ESE
static uint32_t csr_get_tspec_ie_len(struct cm_vdev_join_rsp *rsp)
{
	return rsp->tspec_ie.len;
}
static inline void csr_copy_tspec_ie_len(struct csr_roam_session *session,
					 uint8_t *frame_ptr,
					 struct cm_vdev_join_rsp *rsp)
{
	session->connectedInfo.nTspecIeLength = rsp->tspec_ie.len;
	if (rsp->tspec_ie.len)
		qdf_mem_copy(frame_ptr, rsp->tspec_ie.ptr,
			     rsp->tspec_ie.len);
}

#else
static inline uint32_t csr_get_tspec_ie_len(struct cm_vdev_join_rsp *rsp)
{
	return 0;
}
static inline void csr_copy_tspec_ie_len(struct csr_roam_session *session,
					 uint8_t *frame_ptr,
					 struct cm_vdev_join_rsp *rsp)
{}
#endif

static void csr_fill_connected_info(struct mac_context *mac_ctx,
				    struct csr_roam_session *session,
				    struct cm_vdev_join_rsp *rsp)
{
	uint32_t len;
	struct wlan_connect_rsp_ies *connect_ies;
	uint8_t *frame_ptr;
	uint32_t beacon_data_len = 0;

	connect_ies = &rsp->connect_rsp.connect_ies;
	csr_roam_free_connected_info(mac_ctx, &session->connectedInfo);
	if (connect_ies->bcn_probe_rsp.len > sizeof(struct wlan_frame_hdr))
		beacon_data_len = connect_ies->bcn_probe_rsp.len -
						sizeof(struct wlan_frame_hdr);
	len = beacon_data_len + connect_ies->assoc_req.len +
		connect_ies->assoc_rsp.len + rsp->ric_resp_ie.len +
		csr_get_tspec_ie_len(rsp);
	if (!len)
		return;

	session->connectedInfo.pbFrames = qdf_mem_malloc(len);
	if (!session->connectedInfo.pbFrames)
		return;

	frame_ptr = session->connectedInfo.pbFrames;
	session->connectedInfo.nBeaconLength = beacon_data_len;
	if (beacon_data_len)
		qdf_mem_copy(frame_ptr,
			     connect_ies->bcn_probe_rsp.ptr +
			     sizeof(struct wlan_frame_hdr),
			     beacon_data_len);
	frame_ptr += beacon_data_len;

	session->connectedInfo.nAssocReqLength = connect_ies->assoc_req.len;
	if (connect_ies->assoc_req.len)
		qdf_mem_copy(frame_ptr,
			     connect_ies->assoc_req.ptr,
			     connect_ies->assoc_req.len);
	frame_ptr += connect_ies->assoc_req.len;

	session->connectedInfo.nAssocRspLength = connect_ies->assoc_rsp.len;
	if (connect_ies->assoc_rsp.len)
		qdf_mem_copy(frame_ptr,
			     connect_ies->assoc_rsp.ptr,
			     connect_ies->assoc_rsp.len);
	frame_ptr += connect_ies->assoc_rsp.len;

	session->connectedInfo.nRICRspLength = rsp->ric_resp_ie.len;
	if (rsp->ric_resp_ie.len)
		qdf_mem_copy(frame_ptr, rsp->ric_resp_ie.ptr,
			     rsp->ric_resp_ie.len);
	frame_ptr += rsp->ric_resp_ie.len;

	csr_copy_tspec_ie_len(session, frame_ptr, rsp);
}

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
static inline void csr_qos_send_disconnect_ind(struct mac_context *mac_ctx,
					       uint8_t vdev_id)
{
	sme_qos_csr_event_ind(mac_ctx, vdev_id, SME_QOS_CSR_DISCONNECT_IND,
			      NULL);
}

static inline void csr_qos_send_assoc_ind(struct mac_context *mac_ctx,
					  uint8_t vdev_id,
					  sme_QosAssocInfo *assoc_info)
{
	sme_qos_csr_event_ind(mac_ctx, vdev_id, SME_QOS_CSR_ASSOC_COMPLETE,
			      assoc_info);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void
csr_qso_disconnect_complete_ind(struct mac_context *mac_ctx,
				struct wlan_cm_connect_resp *connect_rsp)
{
	if (IS_ROAM_REASON_DISCONNECTION(
		connect_rsp->roaming_info->roam_reason))
		sme_qos_csr_event_ind(mac_ctx, connect_rsp->vdev_id,
				      SME_QOS_CSR_DISCONNECT_ROAM_COMPLETE,
				      NULL);
}
#else
static inline void
csr_qso_disconnect_complete_ind(struct mac_context *mac_ctx,
				struct wlan_cm_connect_resp *connect_rsp) {}
#endif

static void
csr_qos_send_reassoc_ind(struct mac_context *mac_ctx,
			 uint8_t vdev_id,
			 sme_QosAssocInfo *assoc_info,
			 struct wlan_cm_connect_resp *connect_rsp)
{
	sme_qos_csr_event_ind(mac_ctx, vdev_id, SME_QOS_CSR_HANDOFF_ASSOC_REQ,
			      NULL);
	sme_qos_csr_event_ind(mac_ctx, vdev_id, SME_QOS_CSR_REASSOC_REQ,
			      NULL);
	sme_qos_csr_event_ind(mac_ctx, vdev_id, SME_QOS_CSR_HANDOFF_COMPLETE,
			      NULL);
	sme_qos_csr_event_ind(mac_ctx, vdev_id, SME_QOS_CSR_REASSOC_COMPLETE,
			      assoc_info);

	csr_qso_disconnect_complete_ind(mac_ctx, connect_rsp);
}
#else
static inline void csr_qos_send_disconnect_ind(struct mac_context *mac_ctx,
					       uint8_t vdev_id)
{}

static inline void csr_qos_send_assoc_ind(struct mac_context *mac_ctx,
					  uint8_t vdev_id,
					  sme_QosAssocInfo *assoc_info)
{}
static inline void
csr_qos_send_reassoc_ind(struct mac_context *mac_ctx,
			 uint8_t vdev_id,
			 sme_QosAssocInfo *assoc_info,
			 struct wlan_cm_connect_resp *connect_rsp)
{}
#endif

static void
csr_update_beacon_in_connect_rsp(struct scan_cache_entry *entry,
				 struct wlan_connect_rsp_ies *connect_ies)
{
	if (!entry)
		return;

	/* no need to update if already present */
	if (connect_ies->bcn_probe_rsp.ptr)
		return;

	/*
	 * In case connection to MBSSID: Non Tx BSS OR host reassoc,
	 * vdev/peer manager doesn't send unicast probe req so fill the
	 * beacon in connect resp IEs here.
	 */
	connect_ies->bcn_probe_rsp.len =
				util_scan_entry_frame_len(entry);
	connect_ies->bcn_probe_rsp.ptr =
		qdf_mem_malloc(connect_ies->bcn_probe_rsp.len);
	if (!connect_ies->bcn_probe_rsp.ptr)
		return;

	qdf_mem_copy(connect_ies->bcn_probe_rsp.ptr,
		     util_scan_entry_frame_ptr(entry),
		     connect_ies->bcn_probe_rsp.len);
}

static void csr_fill_connected_profile(struct mac_context *mac_ctx,
				       struct csr_roam_session *session,
				       struct wlan_objmgr_vdev *vdev,
				       struct cm_vdev_join_rsp *rsp)
{
	tCsrRoamConnectedProfile *conn_profile = NULL;
	struct scan_filter *filter;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	QDF_STATUS status;
	qdf_list_t *list = NULL;
	qdf_list_node_t *cur_lst = NULL;
	struct scan_cache_node *cur_node = NULL;
	uint32_t bss_len, ie_len;
	struct bss_description *bss_desc = NULL;
	tDot11fBeaconIEs *bcn_ies;
	sme_QosAssocInfo assoc_info;
	struct cm_roam_values_copy src_cfg;
	bool is_ese = false;

	conn_profile = &session->connectedProfile;
	qdf_mem_zero(conn_profile, sizeof(tCsrRoamConnectedProfile));
	conn_profile->modifyProfileFields.uapsd_mask = rsp->uapsd_mask;
	filter = qdf_mem_malloc(sizeof(*filter));
	if (!filter)
		return;

	filter->num_of_bssid = 1;
	qdf_copy_macaddr(&filter->bssid_list[0], &rsp->connect_rsp.bssid);
	filter->ignore_auth_enc_type = true;

	list = wlan_scan_get_result(mac_ctx->pdev, filter);
	qdf_mem_free(filter);
	if (!list || (list && !qdf_list_size(list)))
		goto purge_list;


	qdf_list_peek_front(list, &cur_lst);
	if (!cur_lst)
		goto purge_list;

	cur_node = qdf_container_of(cur_lst, struct scan_cache_node, node);
	ie_len = util_scan_entry_ie_len(cur_node->entry);
	bss_len = (uint16_t)(offsetof(struct bss_description,
				      ieFields[0]) + ie_len);
	bss_desc = qdf_mem_malloc(bss_len);
	if (!bss_desc)
		goto purge_list;

	wlan_fill_bss_desc_from_scan_entry(mac_ctx, bss_desc, cur_node->entry);

	src_cfg.uint_value = bss_desc->mbo_oce_enabled_ap;
	wlan_cm_roam_cfg_set_value(mac_ctx->psoc, vdev_id, MBO_OCE_ENABLED_AP,
				   &src_cfg);
	csr_fill_single_pmk(mac_ctx->psoc, vdev_id, bss_desc);
	status = wlan_get_parsed_bss_description_ies(mac_ctx, bss_desc,
						     &bcn_ies);
	if (QDF_IS_STATUS_ERROR(status))
		goto purge_list;

	if (!bss_desc->beaconInterval)
		sme_err("ERROR: Beacon interval is ZERO");

	csr_update_beacon_in_connect_rsp(cur_node->entry,
					 &rsp->connect_rsp.connect_ies);

	if (bss_desc->mdiePresent) {
		src_cfg.bool_value = true;
		src_cfg.uint_value =
			(bss_desc->mdie[1] << 8) | (bss_desc->mdie[0]);
	} else {
		src_cfg.bool_value = false;
		src_cfg.uint_value = 0;
	}
	wlan_cm_roam_cfg_set_value(mac_ctx->psoc, vdev_id,
				   MOBILITY_DOMAIN, &src_cfg);

	assoc_info.bss_desc = bss_desc;
	if (rsp->connect_rsp.is_reassoc) {
		if (cm_is_ese_connection(vdev, bcn_ies->ESEVersion.present))
			is_ese = true;
		wlan_cm_set_ese_assoc(mac_ctx->pdev, vdev_id, is_ese);
		wlan_cm_roam_cfg_get_value(mac_ctx->psoc, vdev_id, UAPSD_MASK,
					   &src_cfg);
		assoc_info.uapsd_mask = src_cfg.uint_value;
		csr_qos_send_reassoc_ind(mac_ctx, vdev_id, &assoc_info,
					 &rsp->connect_rsp);
		if (src_cfg.uint_value)
			sme_ps_start_uapsd(MAC_HANDLE(mac_ctx), vdev_id);
	} else {
		assoc_info.uapsd_mask = rsp->uapsd_mask;
		csr_qos_send_assoc_ind(mac_ctx, vdev_id, &assoc_info);
	}

	qdf_mem_free(bcn_ies);

purge_list:
	if (bss_desc)
		qdf_mem_free(bss_desc);
	if (list)
		wlan_scan_purge_results(list);

}

QDF_STATUS cm_csr_connect_rsp(struct wlan_objmgr_vdev *vdev,
			      struct cm_vdev_join_rsp *rsp)
{
	struct mac_context *mac_ctx;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct csr_roam_session *session;
	struct cm_roam_values_copy src_config;

	/*
	 * This API is to update legacy struct and should be removed once
	 * CSR is cleaned up fully. No new params should be added to CSR, use
	 * vdev/pdev/psoc instead
	 */
	if (QDF_IS_STATUS_ERROR(rsp->connect_rsp.connect_status))
		return QDF_STATUS_SUCCESS;

	/* handle below only in case of success */
	mac_ctx = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session || !CSR_IS_SESSION_VALID(mac_ctx, vdev_id)) {
		sme_err("session not found for vdev_id %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	if (!rsp->connect_rsp.is_reassoc) {
		if (rsp->uapsd_mask)
			sme_ps_start_uapsd(MAC_HANDLE(mac_ctx), vdev_id);
		src_config.uint_value = rsp->uapsd_mask;
		wlan_cm_roam_cfg_set_value(mac_ctx->psoc, vdev_id, UAPSD_MASK,
					   &src_config);
	}
	session->nss = rsp->nss;
	csr_fill_connected_info(mac_ctx, session, rsp);
	csr_fill_connected_profile(mac_ctx, session, vdev, rsp);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cm_csr_connect_done_ind(struct wlan_objmgr_vdev *vdev,
			struct wlan_cm_connect_resp *rsp)
{
	struct mac_context *mac_ctx;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	int32_t count;
	struct set_context_rsp install_key_rsp;
	int32_t rsn_cap, set_value;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct dual_sta_policy *dual_sta_policy;
	bool enable_mcc_adaptive_sch = false;

	/*
	 * This API is to update legacy struct and should be removed once
	 * CSR is cleaned up fully. No new params should be added to CSR, use
	 * vdev/pdev/psoc instead
	 */
	mac_ctx = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	mlme_obj = mlme_get_psoc_ext_obj(mac_ctx->psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	if (QDF_IS_STATUS_ERROR(rsp->connect_status)) {
		cm_csr_set_idle(vdev_id);
		sme_qos_update_hand_off(vdev_id, false);
		sme_qos_csr_event_ind(mac_ctx, vdev_id,
				      SME_QOS_CSR_DISCONNECT_IND, NULL);
		/* Fill legacy structures from resp for failure */

		return QDF_STATUS_SUCCESS;
	}

	dual_sta_policy = &mlme_obj->cfg.gen.dual_sta_policy;
	count = policy_mgr_mode_specific_connection_count(mac_ctx->psoc,
							  PM_STA_MODE, NULL);
	/*
	 * send duty cycle percentage to FW only if STA + STA
	 * concurrency is in MCC.
	 */
	sme_debug("Current iface vdev_id:%d, Primary vdev_id:%d, Dual sta policy:%d, count:%d",
		  vdev_id, dual_sta_policy->primary_vdev_id,
		  dual_sta_policy->concurrent_sta_policy, count);

	if (dual_sta_policy->primary_vdev_id != WLAN_UMAC_VDEV_ID_MAX &&
	    dual_sta_policy->concurrent_sta_policy ==
	    QCA_WLAN_CONCURRENT_STA_POLICY_PREFER_PRIMARY && count == 2 &&
	    policy_mgr_current_concurrency_is_mcc(mac_ctx->psoc)) {
		policy_mgr_get_mcc_adaptive_sch(mac_ctx->psoc,
						&enable_mcc_adaptive_sch);
		if (enable_mcc_adaptive_sch) {
			sme_debug("Disable mcc_adaptive_scheduler");
			policy_mgr_set_dynamic_mcc_adaptive_sch(
							mac_ctx->psoc, false);
			if (QDF_STATUS_SUCCESS != sme_set_mas(false)) {
				sme_err("Failed to disable mcc_adaptive_sched");
				return -EAGAIN;
			}
		}
		set_value =
			wlan_mlme_get_mcc_duty_cycle_percentage(mac_ctx->pdev);
		sme_cli_set_command(vdev_id, WMA_VDEV_MCC_SET_TIME_QUOTA,
				    set_value, VDEV_CMD);
	  } else if (dual_sta_policy->concurrent_sta_policy ==
		     QCA_WLAN_CONCURRENT_STA_POLICY_UNBIASED && count == 2 &&
		     policy_mgr_current_concurrency_is_mcc(mac_ctx->psoc)) {
		policy_mgr_get_mcc_adaptive_sch(mac_ctx->psoc,
						&enable_mcc_adaptive_sch);
		if (enable_mcc_adaptive_sch) {
			sme_debug("Enable mcc_adaptive_scheduler");
			policy_mgr_set_dynamic_mcc_adaptive_sch(
						  mac_ctx->psoc, true);
			if (QDF_STATUS_SUCCESS != sme_set_mas(true)) {
				sme_err("Failed to enable mcc_adaptive_sched");
				return -EAGAIN;
			}
		}
	}

	/*
	 * For open mode authentication, send dummy install key response to
	 * send OBSS scan and QOS event.
	 */
	if (!rsp->is_wps_connection && cm_is_open_mode(vdev)) {
		install_key_rsp.length = sizeof(install_key_rsp);
		install_key_rsp.status_code = eSIR_SME_SUCCESS;
		install_key_rsp.sessionId = vdev_id;
		qdf_copy_macaddr(&install_key_rsp.peer_macaddr, &rsp->bssid);
		csr_roam_chk_lnk_set_ctx_rsp(mac_ctx,
					     (tSirSmeRsp *)&install_key_rsp);
	}

	rsn_cap = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_RSN_CAP);
	if (rsn_cap >= 0) {
		if (wma_cli_set2_command(vdev_id, WMI_VDEV_PARAM_RSN_CAPABILITY,
					 rsn_cap, 0, VDEV_CMD))
			sme_err("Failed to update WMI_VDEV_PARAM_RSN_CAPABILITY for vdev id %d",
				vdev_id);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cm_csr_handle_diconnect_req(struct wlan_objmgr_vdev *vdev,
				       struct wlan_cm_vdev_discon_req *req)
{
	struct mac_context *mac_ctx;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct csr_roam_session *session;

	/*
	 * This API is to update legacy struct and should be removed once
	 * CSR is cleaned up fully. No new params should be added to CSR, use
	 * vdev/pdev/psoc instead
	 */
	mac_ctx = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session || !CSR_IS_SESSION_VALID(mac_ctx, vdev_id)) {
		sme_err("session not found for vdev_id %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	cm_csr_set_joining(vdev_id);

	/* Update the disconnect stats */
	session->disconnect_stats.disconnection_cnt++;
	if (req->req.source == CM_PEER_DISCONNECT) {
		session->disconnect_stats.disassoc_by_peer++;
	} else if (req->req.source == CM_SB_DISCONNECT) {
		switch (req->req.reason_code) {
		case REASON_DISASSOC_DUE_TO_INACTIVITY:
			session->disconnect_stats.peer_kickout++;
			break;
		case REASON_BEACON_MISSED:
			session->disconnect_stats.bmiss++;
			break;
		default:
			/* Unknown reason code */
			break;
		}
	} else {
		session->disconnect_stats.disconnection_by_app++;
	}

	csr_update_prev_ap_info(session, vdev);
	csr_qos_send_disconnect_ind(mac_ctx, vdev_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cm_csr_diconnect_done_ind(struct wlan_objmgr_vdev *vdev,
			  struct wlan_cm_discon_rsp *rsp)
{
	struct mac_context *mac_ctx;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);

	/*
	 * This API is to update legacy struct and should be removed once
	 * CSR is cleaned up fully. No new params should be added to CSR, use
	 * vdev/pdev/psoc instead
	 */
	mac_ctx = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_ctx)
		return QDF_STATUS_E_INVAL;

	cm_csr_set_idle(vdev_id);
	if (cm_is_vdev_roaming(vdev))
		sme_qos_update_hand_off(vdev_id, false);
	csr_set_default_dot11_mode(mac_ctx);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_HOST_ROAM
void cm_csr_preauth_done(struct wlan_objmgr_vdev *vdev)
{
	struct mac_context *mac_ctx;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct cm_roam_values_copy config;
	bool is_11r;

	/*
	 * This API is to update legacy struct and should be removed once
	 * CSR is cleaned up fully. No new params should be added to CSR, use
	 * vdev/pdev/psoc instead
	 */
	mac_ctx = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return;
	}

	wlan_cm_roam_cfg_get_value(mac_ctx->psoc, vdev_id, IS_11R_CONNECTION,
				   &config);
	is_11r = config.bool_value;
	if (is_11r || wlan_cm_get_ese_assoc(mac_ctx->pdev, vdev_id))
		sme_qos_csr_event_ind(mac_ctx, vdev_id,
				      SME_QOS_CSR_PREAUTH_SUCCESS_IND, NULL);
}
#endif

#else /* FEATURE_CM_ENABLE */

#ifdef WLAN_FEATURE_FILS_SK
/*
 * csr_validate_and_update_fils_info: Copy fils connection info to join request
 * @profile: pointer to profile
 * @csr_join_req: csr join request
 *
 * Return: None
 */
static QDF_STATUS
csr_validate_and_update_fils_info(struct mac_context *mac,
				  struct csr_roam_profile *profile,
				  struct bss_description *bss_desc,
				  struct join_req *csr_join_req,
				  uint8_t vdev_id)
{
	uint8_t cache_id[CACHE_ID_LEN] = {0};
	struct qdf_mac_addr bssid;

	if (!profile->fils_con_info) {
		wlan_cm_update_mlme_fils_connection_info(mac->psoc, NULL,
							 vdev_id);
		return QDF_STATUS_SUCCESS;
	}

	if (!profile->fils_con_info->is_fils_connection) {
		sme_debug("FILS_PMKSA: Not a FILS connection");
		return QDF_STATUS_SUCCESS;
	}

	if (bss_desc->fils_info_element.is_cache_id_present) {
		qdf_mem_copy(cache_id, bss_desc->fils_info_element.cache_id,
			     CACHE_ID_LEN);
		sme_debug("FILS_PMKSA: cache_id[0]:%d, cache_id[1]:%d",
			  cache_id[0], cache_id[1]);
	}

	qdf_mem_copy(bssid.bytes,
		     csr_join_req->bssDescription.bssId,
		     QDF_MAC_ADDR_SIZE);

	if ((!profile->fils_con_info->r_rk_length ||
	     !profile->fils_con_info->key_nai_length) &&
	    !bss_desc->fils_info_element.is_cache_id_present &&
	    !csr_lookup_fils_pmkid(mac, vdev_id, cache_id,
				   csr_join_req->ssId.ssId,
				   csr_join_req->ssId.length, &bssid))
		return QDF_STATUS_E_FAILURE;

	return wlan_cm_update_mlme_fils_connection_info(mac->psoc,
							profile->fils_con_info,
							vdev_id);
}
#else
static inline QDF_STATUS
csr_validate_and_update_fils_info(struct mac_context *mac,
				  struct csr_roam_profile *profile,
				  struct bss_description *bss_desc,
				  struct join_req *csr_join_req,
				  uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static QDF_STATUS csr_copy_assoc_ie(struct mac_context *mac, uint8_t vdev_id,
				    struct csr_roam_profile *profile,
				    struct join_req *csr_join_req)
{
	struct rso_config *rso_cfg;
	struct wlan_objmgr_vdev *vdev;
	uint32_t ie_len;
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL for vdev %d", vdev_id);
		return status;
	}
	rso_cfg = wlan_cm_get_rso_config(vdev);
	if (!rso_cfg)
		goto rel_vdev_ref;

	if (profile->nAddIEAssocLength && profile->pAddIEAssoc) {
		ie_len = profile->nAddIEAssocLength;
		if (ie_len > rso_cfg->assoc_ie.len) {
			if (rso_cfg->assoc_ie.ptr && rso_cfg->assoc_ie.len)
				qdf_mem_free(rso_cfg->assoc_ie.ptr);
			rso_cfg->assoc_ie.ptr = qdf_mem_malloc(ie_len);
			if (!rso_cfg->assoc_ie.ptr) {
				rso_cfg->assoc_ie.len = 0;
				goto rel_vdev_ref;
			}
		}
		rso_cfg->assoc_ie.len = ie_len;
		qdf_mem_copy(rso_cfg->assoc_ie.ptr,
			     profile->pAddIEAssoc, ie_len);
		csr_join_req->addIEAssoc.length = ie_len;
		qdf_mem_copy(&csr_join_req->addIEAssoc.addIEdata,
			     profile->pAddIEAssoc, ie_len);
	} else {
		rso_cfg->assoc_ie.len = 0;
		if (rso_cfg->assoc_ie.ptr) {
			qdf_mem_free(rso_cfg->assoc_ie.ptr);
			rso_cfg->assoc_ie.ptr = NULL;
		}
		csr_join_req->addIEAssoc.length = 0;
	}
	status = QDF_STATUS_SUCCESS;

rel_vdev_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);
	return status;
}

/**
 * The communication between HDD and LIM is thru mailbox (MB).
 * Both sides will access the data structure "struct join_req".
 * The rule is, while the components of "struct join_req" can be accessed in the
 * regular way like struct join_req.assocType, this guideline stops at component
 * tSirRSNie;
 * any acces to the components after tSirRSNie is forbidden because the space
 * from tSirRSNie is squeezed with the component "struct bss_description" and
 * since the size of actual 'struct bss_description' varies, the receiving side
 * should keep in mind not to access the components DIRECTLY after tSirRSNie.
 */
QDF_STATUS csr_send_join_req_msg(struct mac_context *mac, uint32_t sessionId,
				 struct bss_description *pBssDescription,
				 struct csr_roam_profile *pProfile,
				 tDot11fBeaconIEs *pIes, uint16_t messageType)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t acm_mask = 0, uapsd_mask;
	uint32_t bss_freq;
	uint16_t msgLen, ieLen;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	uint8_t *wpaRsnIE = NULL;
	struct join_req *csr_join_req;
	tpCsrNeighborRoamControlInfo neigh_roam_info;
	enum csr_akm_type akm;
#ifdef FEATURE_WLAN_ESE
	bool ese_config = false;
#endif
	struct cm_roam_values_copy src_config;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	/* To satisfy klockworks */
	if (!pBssDescription) {
		sme_err(" pBssDescription is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	neigh_roam_info = &mac->roam.neighborRoamInfo[sessionId];
	bss_freq = pBssDescription->chan_freq;
	if ((eWNI_SME_REASSOC_REQ == messageType)) {
		wlan_cm_set_disable_hi_rssi(mac->pdev, sessionId, true);
		sme_debug("Disabling HI_RSSI, AP freq=%d, rssi=%d",
			  pBssDescription->chan_freq, pBssDescription->rssi);
	}

	do {
		pSession->joinFailStatusCode.status_code = eSIR_SME_SUCCESS;
		pSession->joinFailStatusCode.reasonCode = 0;
		qdf_mem_copy(&pSession->joinFailStatusCode.bssId,
		       &pBssDescription->bssId, sizeof(tSirMacAddr));
		/*
		 * the struct join_req which includes a single
		 * bssDescription. it includes a single uint32_t for the
		 * IE fields, but the length field in the bssDescription
		 * needs to be interpreted to determine length of IE fields
		 * So, take the size of the struct join_req, subtract  size of
		 * bssDescription, add the number of bytes indicated by the
		 * length field of the bssDescription, add the size of length
		 * field because it not included in the length field.
		 */
		msgLen = sizeof(struct join_req) - sizeof(*pBssDescription) +
				pBssDescription->length +
				sizeof(pBssDescription->length) +
				/*
				 * add in the size of the WPA IE that
				 * we may build.
				 */
				sizeof(tCsrWpaIe) + sizeof(tCsrWpaAuthIe) +
				sizeof(uint16_t);
		csr_join_req = qdf_mem_malloc(msgLen);
		if (!csr_join_req)
			status = QDF_STATUS_E_NOMEM;
		else
			status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;

		wpaRsnIE = qdf_mem_malloc(DOT11F_IE_RSN_MAX_LEN);
		if (!wpaRsnIE)
			status = QDF_STATUS_E_NOMEM;

		if (!QDF_IS_STATUS_SUCCESS(status))
			break;

		csr_join_req->messageType = messageType;
		csr_join_req->length = msgLen;
		csr_join_req->vdev_id = (uint8_t) sessionId;
		if (pIes->SSID.present &&
		    !csr_is_nullssid(pIes->SSID.ssid,
				     pIes->SSID.num_ssid)) {
			csr_join_req->ssId.length = pIes->SSID.num_ssid;
			qdf_mem_copy(&csr_join_req->ssId.ssId, pIes->SSID.ssid,
				     pIes->SSID.num_ssid);
		} else if (pProfile->SSIDs.numOfSSIDs) {
			csr_join_req->ssId.length =
					pProfile->SSIDs.SSIDList[0].SSID.length;
			qdf_mem_copy(&csr_join_req->ssId.ssId,
				     pProfile->SSIDs.SSIDList[0].SSID.ssId,
				     csr_join_req->ssId.length);
		} else {
			csr_join_req->ssId.length = 0;
		}

		sme_nofl_info("vdev-%d: Connecting to %.*s " QDF_MAC_ADDR_FMT
			      " rssi: %d freq: %d akm %d cipher: uc %d mc %d, CC: %c%c",
			      sessionId, csr_join_req->ssId.length,
			      csr_join_req->ssId.ssId,
			      QDF_MAC_ADDR_REF(pBssDescription->bssId),
			      pBssDescription->rssi, pBssDescription->chan_freq,
			      pProfile->negotiatedAuthType,
			      pProfile->negotiatedUCEncryptionType,
			      pProfile->negotiatedMCEncryptionType,
			      mac->scan.countryCodeCurrent[0],
			      mac->scan.countryCodeCurrent[1]);
		wlan_rec_conn_info(sessionId, DEBUG_CONN_CONNECTING,
				   pBssDescription->bssId,
				   pProfile->negotiatedAuthType,
				   pBssDescription->chan_freq);
		akm = pProfile->negotiatedAuthType;
		csr_join_req->akm = csr_convert_csr_to_ani_akm_type(akm);

		csr_join_req->wps_registration = pProfile->bWPSAssociation;
		csr_join_req->force_24ghz_in_ht20 =
			pProfile->force_24ghz_in_ht20;
		src_config.uint_value = pProfile->uapsd_mask;
		wlan_cm_roam_cfg_set_value(mac->psoc, sessionId, UAPSD_MASK,
					   &src_config);

		/* rsnIE */
		if (csr_is_profile_wpa(pProfile)) {
			/* Insert the Wpa IE into the join request */
			ieLen = csr_retrieve_wpa_ie(mac, sessionId, pProfile,
						    pBssDescription, pIes,
						    (tCsrWpaIe *) (wpaRsnIE));
		} else if (csr_is_profile_rsn(pProfile)) {
			/* Insert the RSN IE into the join request */
			ieLen =
				csr_retrieve_rsn_ie(mac, sessionId, pProfile,
						    pBssDescription, pIes,
						    (tCsrRSNIe *) (wpaRsnIE));
			csr_join_req->force_rsne_override =
						pProfile->force_rsne_override;
		}
#ifdef FEATURE_WLAN_WAPI
		else if (csr_is_profile_wapi(pProfile)) {
			/* Insert the WAPI IE into the join request */
			ieLen =
				csr_retrieve_wapi_ie(mac, sessionId, pProfile,
						     pBssDescription, pIes,
						     (tCsrWapiIe *) (wpaRsnIE));
		}
#endif /* FEATURE_WLAN_WAPI */
		else
			ieLen = 0;
		/* remember the IE for future use */
		if (ieLen) {
			if (ieLen > DOT11F_IE_RSN_MAX_LEN) {
				sme_err("WPA RSN IE length :%d is more than DOT11F_IE_RSN_MAX_LEN, resetting to %d",
					ieLen, DOT11F_IE_RSN_MAX_LEN);
				ieLen = DOT11F_IE_RSN_MAX_LEN;
			}
#ifdef FEATURE_WLAN_WAPI
			if (csr_is_profile_wapi(pProfile)) {
				/* Check whether we need to allocate more mem */
				if (ieLen > pSession->nWapiReqIeLength) {
					if (pSession->pWapiReqIE
					    && pSession->nWapiReqIeLength) {
						qdf_mem_free(pSession->
							     pWapiReqIE);
					}
					pSession->pWapiReqIE =
						qdf_mem_malloc(ieLen);
					if (!pSession->pWapiReqIE)
						status = QDF_STATUS_E_NOMEM;
					else
						status = QDF_STATUS_SUCCESS;
					if (!QDF_IS_STATUS_SUCCESS(status))
						break;
				}
				pSession->nWapiReqIeLength = ieLen;
				qdf_mem_copy(pSession->pWapiReqIE, wpaRsnIE,
					     ieLen);
				csr_join_req->rsnIE.length = ieLen;
				qdf_mem_copy(&csr_join_req->rsnIE.rsnIEdata,
						 wpaRsnIE, ieLen);
			} else  /* should be WPA/WPA2 otherwise */
#endif /* FEATURE_WLAN_WAPI */
			{
				/* Check whether we need to allocate more mem */
				if (ieLen > pSession->nWpaRsnReqIeLength) {
					if (pSession->pWpaRsnReqIE
					    && pSession->nWpaRsnReqIeLength) {
						qdf_mem_free(pSession->
							     pWpaRsnReqIE);
					}
					pSession->pWpaRsnReqIE =
						qdf_mem_malloc(ieLen);
					if (!pSession->pWpaRsnReqIE)
						status = QDF_STATUS_E_NOMEM;
					else
						status = QDF_STATUS_SUCCESS;
					if (!QDF_IS_STATUS_SUCCESS(status))
						break;
				}
				pSession->nWpaRsnReqIeLength = ieLen;
				qdf_mem_copy(pSession->pWpaRsnReqIE, wpaRsnIE,
					     ieLen);
				csr_join_req->rsnIE.length = ieLen;
				qdf_mem_copy(&csr_join_req->rsnIE.rsnIEdata,
						 wpaRsnIE, ieLen);
			}
		} else {
			/* free whatever old info */
			pSession->nWpaRsnReqIeLength = 0;
			if (pSession->pWpaRsnReqIE) {
				qdf_mem_free(pSession->pWpaRsnReqIE);
				pSession->pWpaRsnReqIE = NULL;
			}
#ifdef FEATURE_WLAN_WAPI
			pSession->nWapiReqIeLength = 0;
			if (pSession->pWapiReqIE) {
				qdf_mem_free(pSession->pWapiReqIE);
				pSession->pWapiReqIE = NULL;
			}
#endif /* FEATURE_WLAN_WAPI */
			csr_join_req->rsnIE.length = 0;
		}
		/* addIEScan */
		if (pProfile->nAddIEScanLength && pProfile->pAddIEScan) {
			ieLen = pProfile->nAddIEScanLength;
			if (ieLen > pSession->nAddIEScanLength) {
				if (pSession->pAddIEScan
					&& pSession->nAddIEScanLength) {
					qdf_mem_free(pSession->pAddIEScan);
				}
				pSession->pAddIEScan = qdf_mem_malloc(ieLen);
				if (!pSession->pAddIEScan)
					status = QDF_STATUS_E_NOMEM;
				else
					status = QDF_STATUS_SUCCESS;
				if (!QDF_IS_STATUS_SUCCESS(status))
					break;
			}
			pSession->nAddIEScanLength = ieLen;
			qdf_mem_copy(pSession->pAddIEScan, pProfile->pAddIEScan,
					ieLen);
			csr_join_req->addIEScan.length = ieLen;
			qdf_mem_copy(&csr_join_req->addIEScan.addIEdata,
					pProfile->pAddIEScan, ieLen);
		} else {
			pSession->nAddIEScanLength = 0;
			if (pSession->pAddIEScan) {
				qdf_mem_free(pSession->pAddIEScan);
				pSession->pAddIEScan = NULL;
			}
			csr_join_req->addIEScan.length = 0;
		}
		/* addIEAssoc */
		status = csr_copy_assoc_ie(mac, sessionId, pProfile,
					   csr_join_req);
		if (QDF_IS_STATUS_ERROR(status))
			break;

		if (eWNI_SME_REASSOC_REQ == messageType) {
			/* Unmask any AC in reassoc that is ACM-set */
			uapsd_mask = (uint8_t) pProfile->uapsd_mask;
			if (uapsd_mask && (pBssDescription)) {
				if (CSR_IS_QOS_BSS(pIes)
						&& CSR_IS_UAPSD_BSS(pIes))
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
					acm_mask =
						sme_qos_get_acm_mask(mac,
								pBssDescription,
								pIes);
#endif /* WLAN_MDM_CODE_REDUCTION_OPT */
				else
					uapsd_mask = 0;
			}
		}

		csr_join_req->UCEncryptionType =
				csr_translate_encrypt_type_to_ed_type
					(pProfile->negotiatedUCEncryptionType);

#ifdef FEATURE_WLAN_ESE
		ese_config =  mac->mlme_cfg->lfr.ese_enabled;
#endif

#ifdef FEATURE_WLAN_ESE
		if (eWNI_SME_JOIN_REQ == messageType) {
			tESETspecInfo eseTspec;
			/*
			 * ESE-Tspec IEs in the ASSOC request is presently not
			 * supported. so nullify the TSPEC parameters
			 */
			qdf_mem_zero(&eseTspec, sizeof(tESETspecInfo));
			qdf_mem_copy(&csr_join_req->eseTspecInfo,
					&eseTspec, sizeof(tESETspecInfo));
		} else if (eWNI_SME_REASSOC_REQ == messageType) {
			if ((csr_is_profile_ese(pProfile) ||
				((pIes->ESEVersion.present)
				&& (pProfile->negotiatedAuthType ==
					eCSR_AUTH_TYPE_OPEN_SYSTEM))) &&
				(ese_config)) {
				tESETspecInfo eseTspec;

				qdf_mem_zero(&eseTspec, sizeof(tESETspecInfo));
				eseTspec.numTspecs =
					sme_qos_ese_retrieve_tspec_info(mac,
						sessionId,
						(tTspecInfo *) &eseTspec.
							tspec[0]);
				csr_join_req->eseTspecInfo.numTspecs =
					eseTspec.numTspecs;
				if (eseTspec.numTspecs) {
					qdf_mem_copy(&csr_join_req->eseTspecInfo
						.tspec[0],
						&eseTspec.tspec[0],
						(eseTspec.numTspecs *
							sizeof(tTspecInfo)));
				}
			} else {
				tESETspecInfo eseTspec;
				/*
				 * ESE-Tspec IEs in the ASSOC request is
				 * presently not supported. so nullify the TSPEC
				 * parameters
				 */
				qdf_mem_zero(&eseTspec, sizeof(tESETspecInfo));
				qdf_mem_copy(&csr_join_req->eseTspecInfo,
						&eseTspec,
						sizeof(tESETspecInfo));
			}
		}
#endif /* FEATURE_WLAN_ESE */
		if (pProfile->bOSENAssociation)
			csr_join_req->isOSENConnection = true;
		else
			csr_join_req->isOSENConnection = false;

		/* Move the entire BssDescription into the join request. */
		qdf_mem_copy(&csr_join_req->bssDescription, pBssDescription,
				pBssDescription->length +
				sizeof(pBssDescription->length));

		status = csr_validate_and_update_fils_info(mac, pProfile,
							   pBssDescription,
							   csr_join_req,
							   sessionId);
		if (QDF_IS_STATUS_ERROR(status))
			return status;

		/*
		 * conc_custom_rule1:
		 * If SAP comes up first and STA comes up later then SAP
		 * need to follow STA's channel in 2.4Ghz. In following if
		 * condition we are adding sanity check, just to make sure that
		 * if this rule is enabled then don't allow STA to connect on
		 * 5gz channel and also by this time SAP's channel should be the
		 * same as STA's channel.
		 */
		if (mac->roam.configParam.conc_custom_rule1) {
			if ((0 == mac->roam.configParam.
				is_sta_connection_in_5gz_enabled) &&
				WLAN_REG_IS_5GHZ_CH_FREQ(bss_freq)) {
				QDF_TRACE(QDF_MODULE_ID_SME,
					  QDF_TRACE_LEVEL_ERROR,
					 "STA-conn on 5G isn't allowed");
				status = QDF_STATUS_E_FAILURE;
				break;
			}
			if (!WLAN_REG_IS_5GHZ_CH_FREQ(bss_freq) &&
			    csr_is_conn_allow_2g_band(mac, bss_freq) == false) {
				status = QDF_STATUS_E_FAILURE;
				break;
			}
		}

		/*
		 * conc_custom_rule2:
		 * If P2PGO comes up first and STA comes up later then P2PGO
		 * need to follow STA's channel in 5Ghz. In following if
		 * condition we are just adding sanity check to make sure that
		 * by this time P2PGO's channel is same as STA's channel.
		 */
		if (mac->roam.configParam.conc_custom_rule2 &&
			!WLAN_REG_IS_24GHZ_CH_FREQ(bss_freq) &&
			(!csr_is_conn_allow_5g_band(mac, bss_freq))) {
			status = QDF_STATUS_E_FAILURE;
			break;
		}

		status = umac_send_mb_message_to_mac(csr_join_req);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			/*
			 * umac_send_mb_message_to_mac would've released the mem
			 * allocated by csr_join_req. Let's make it defensive by
			 * assigning NULL to the pointer.
			 */
			csr_join_req = NULL;
			break;
		}

		if (pProfile->csrPersona == QDF_STA_MODE)
			wlan_register_txrx_packetdump(OL_TXRX_PDEV_ID);

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		if (eWNI_SME_JOIN_REQ == messageType) {
			/* Notify QoS module that join happening */
			pSession->join_bssid_count++;
			sme_qos_csr_event_ind(mac, (uint8_t) sessionId,
						SME_QOS_CSR_JOIN_REQ, NULL);
		} else if (eWNI_SME_REASSOC_REQ == messageType)
			/* Notify QoS module that reassoc happening */
			sme_qos_csr_event_ind(mac, (uint8_t) sessionId,
						SME_QOS_CSR_REASSOC_REQ,
						NULL);
#endif
	} while (0);

	/* Clean up the memory in case of any failure */
	if (!QDF_IS_STATUS_SUCCESS(status) && (csr_join_req))
		qdf_mem_free(csr_join_req);

	if (wpaRsnIE)
		qdf_mem_free(wpaRsnIE);

	return status;
}
#endif /* FEATURE_CM_ENABLE */

/* */
QDF_STATUS csr_send_mb_disassoc_req_msg(struct mac_context *mac,
					uint32_t sessionId,
					tSirMacAddr bssId, uint16_t reasonCode)
{
	struct disassoc_req *pMsg;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	enum QDF_OPMODE opmode;
#endif

	if (!CSR_IS_SESSION_VALID(mac, sessionId))
		return QDF_STATUS_E_FAILURE;

	pMsg = qdf_mem_malloc(sizeof(*pMsg));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_DISASSOC_REQ;
	pMsg->length = sizeof(*pMsg);
	pMsg->sessionId = sessionId;

	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	opmode = wlan_get_opmode_from_vdev_id(mac->pdev, sessionId);
	if (opmode == QDF_SAP_MODE || opmode == QDF_P2P_GO_MODE) {
		qdf_mem_copy(&pMsg->bssid.bytes,
			     &pSession->self_mac_addr,
			     QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(&pMsg->peer_macaddr.bytes,
			     bssId,
			     QDF_MAC_ADDR_SIZE);
	} else {
		qdf_mem_copy(&pMsg->bssid.bytes,
			     bssId, QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(&pMsg->peer_macaddr.bytes,
			     bssId, QDF_MAC_ADDR_SIZE);
	}

	pMsg->process_ho_fail = (pSession->disconnect_reason ==
		REASON_FW_TRIGGERED_ROAM_FAILURE) ? true : false;
#else
	qdf_mem_copy(&pMsg->bssid.bytes, &pSession->self_mac_addr,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(&pMsg->peer_macaddr.bytes, bssId, QDF_MAC_ADDR_SIZE);
#endif
	pMsg->reasonCode = reasonCode;

	/* Update the disconnect stats */
	pSession->disconnect_stats.disconnection_cnt++;
	pSession->disconnect_stats.disconnection_by_app++;

#ifndef FEATURE_CM_ENABLE
	/*
	 * The state will be DISASSOC_HANDOFF only when we are doing
	 * handoff. Here we should not send the disassoc over the air
	 * to the AP
	 */
	if ((CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(mac, sessionId)
			&& csr_roam_is11r_assoc(mac, sessionId)) ||
						pMsg->process_ho_fail) {
		/* Set DoNotSendOverTheAir flag to 1 only for handoff case */
		pMsg->doNotSendOverTheAir = 1;
	}
#endif
	return umac_send_mb_message_to_mac(pMsg);
}

QDF_STATUS csr_send_chng_mcc_beacon_interval(struct mac_context *mac,
						uint32_t sessionId)
{
	struct wlan_change_bi *pMsg;
	uint16_t len = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	/* NO need to update the Beacon Params if update beacon parameter flag
	 * is not set
	 */
	if (!mac->roam.roamSession[sessionId].bssParams.updatebeaconInterval)
		return QDF_STATUS_SUCCESS;

	mac->roam.roamSession[sessionId].bssParams.updatebeaconInterval =
		false;

	/* Create the message and send to lim */
	len = sizeof(*pMsg);
	pMsg = qdf_mem_malloc(len);
	if (!pMsg)
		status = QDF_STATUS_E_NOMEM;
	else
		status = QDF_STATUS_SUCCESS;
	if (QDF_IS_STATUS_SUCCESS(status)) {
		pMsg->message_type = eWNI_SME_CHNG_MCC_BEACON_INTERVAL;
		pMsg->length = len;

		qdf_copy_macaddr(&pMsg->bssid, &pSession->self_mac_addr);
		sme_debug("CSR Attempting to change BI for Bssid= "
			  QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(pMsg->bssid.bytes));
		pMsg->session_id = sessionId;
		sme_debug("session %d BeaconInterval %d",
			sessionId,
			mac->roam.roamSession[sessionId].bssParams.
			beaconInterval);
		pMsg->beacon_interval =
			mac->roam.roamSession[sessionId].bssParams.beaconInterval;
		status = umac_send_mb_message_to_mac(pMsg);
	}
	return status;
}

#ifdef QCA_HT_2040_COEX
QDF_STATUS csr_set_ht2040_mode(struct mac_context *mac, uint32_t sessionId,
			       ePhyChanBondState cbMode, bool obssEnabled)
{
	struct set_ht2040_mode *pMsg;
	uint16_t len = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	/* Create the message and send to lim */
	len = sizeof(struct set_ht2040_mode);
	pMsg = qdf_mem_malloc(len);
	if (!pMsg)
		status = QDF_STATUS_E_NOMEM;
	else
		status = QDF_STATUS_SUCCESS;
	if (QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_zero(pMsg, sizeof(struct set_ht2040_mode));
		pMsg->messageType = eWNI_SME_SET_HT_2040_MODE;
		pMsg->length = len;

		qdf_copy_macaddr(&pMsg->bssid, &pSession->self_mac_addr);
		sme_debug(
			"CSR Attempting to set HT20/40 mode for Bssid= "
			 QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(pMsg->bssid.bytes));
		pMsg->sessionId = sessionId;
		sme_debug("  session %d HT20/40 mode %d",
			sessionId, cbMode);
		pMsg->cbMode = cbMode;
		pMsg->obssEnabled = obssEnabled;
		status = umac_send_mb_message_to_mac(pMsg);
	}
	return status;
}
#endif

QDF_STATUS csr_send_mb_deauth_req_msg(struct mac_context *mac,
				      uint32_t vdev_id,
				      tSirMacAddr bssId, uint16_t reasonCode)
{
	struct deauth_req *pMsg;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, vdev_id);
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	enum QDF_OPMODE opmode;
#endif

	if (!CSR_IS_SESSION_VALID(mac, vdev_id))
		return QDF_STATUS_E_FAILURE;

	pMsg = qdf_mem_malloc(sizeof(*pMsg));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_DEAUTH_REQ;
	pMsg->length = sizeof(*pMsg);
	pMsg->vdev_id = vdev_id;

	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	opmode = wlan_get_opmode_from_vdev_id(mac->pdev, vdev_id);
	if (opmode == QDF_SAP_MODE || opmode == QDF_P2P_GO_MODE) {
		qdf_mem_copy(&pMsg->bssid,
			     &pSession->self_mac_addr,
			     QDF_MAC_ADDR_SIZE);
	} else {
		qdf_mem_copy(&pMsg->bssid,
			     bssId, QDF_MAC_ADDR_SIZE);
	}
#else
	qdf_mem_copy(&pMsg->bssid, &pSession->self_mac_addr, QDF_MAC_ADDR_SIZE);
#endif
	/* Set the peer MAC address before sending the message to LIM */
	qdf_mem_copy(&pMsg->peer_macaddr.bytes, bssId, QDF_MAC_ADDR_SIZE);
	pMsg->reasonCode = reasonCode;

	/* Update the disconnect stats */
	pSession->disconnect_stats.disconnection_cnt++;
	pSession->disconnect_stats.disconnection_by_app++;

	return umac_send_mb_message_to_mac(pMsg);
}

QDF_STATUS csr_send_mb_disassoc_cnf_msg(struct mac_context *mac,
					struct disassoc_ind *pDisassocInd)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct disassoc_cnf *pMsg;

	do {
		pMsg = qdf_mem_malloc(sizeof(*pMsg));
		if (!pMsg)
			status = QDF_STATUS_E_NOMEM;
		else
			status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		pMsg->messageType = eWNI_SME_DISASSOC_CNF;
		pMsg->status_code = eSIR_SME_SUCCESS;
		pMsg->length = sizeof(*pMsg);
		pMsg->vdev_id = pDisassocInd->vdev_id;
		qdf_copy_macaddr(&pMsg->peer_macaddr,
				 &pDisassocInd->peer_macaddr);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}

		qdf_copy_macaddr(&pMsg->bssid, &pDisassocInd->bssid);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}

		status = umac_send_mb_message_to_mac(pMsg);
	} while (0);
	return status;
}

QDF_STATUS csr_send_mb_deauth_cnf_msg(struct mac_context *mac,
				      struct deauth_ind *pDeauthInd)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct deauth_cnf *pMsg;

	do {
		pMsg = qdf_mem_malloc(sizeof(*pMsg));
		if (!pMsg)
			status = QDF_STATUS_E_NOMEM;
		else
			status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		pMsg->messageType = eWNI_SME_DEAUTH_CNF;
		pMsg->status_code = eSIR_SME_SUCCESS;
		pMsg->length = sizeof(*pMsg);
		pMsg->vdev_id = pDeauthInd->vdev_id;
		qdf_copy_macaddr(&pMsg->bssid, &pDeauthInd->bssid);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}
		qdf_copy_macaddr(&pMsg->peer_macaddr,
				 &pDeauthInd->peer_macaddr);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}
		status = umac_send_mb_message_to_mac(pMsg);
	} while (0);
	return status;
}

QDF_STATUS csr_send_assoc_cnf_msg(struct mac_context *mac,
				  struct assoc_ind *pAssocInd,
				  QDF_STATUS Halstatus,
				  enum wlan_status_code mac_status_code)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct assoc_cnf *pMsg;
	struct scheduler_msg msg = { 0 };

	sme_debug("HalStatus: %d, mac_status_code %d",
		  Halstatus, mac_status_code);
	do {
		pMsg = qdf_mem_malloc(sizeof(*pMsg));
		if (!pMsg)
			return QDF_STATUS_E_NOMEM;
		pMsg->messageType = eWNI_SME_ASSOC_CNF;
		pMsg->length = sizeof(*pMsg);
		if (QDF_IS_STATUS_SUCCESS(Halstatus)) {
			pMsg->status_code = eSIR_SME_SUCCESS;
		} else {
			pMsg->status_code = eSIR_SME_ASSOC_REFUSED;
			pMsg->mac_status_code = mac_status_code;
		}
		/* bssId */
		qdf_mem_copy(pMsg->bssid.bytes, pAssocInd->bssId,
			     QDF_MAC_ADDR_SIZE);
		/* peerMacAddr */
		qdf_mem_copy(pMsg->peer_macaddr.bytes, pAssocInd->peerMacAddr,
			     QDF_MAC_ADDR_SIZE);
		/* aid */
		pMsg->aid = pAssocInd->aid;
		/* OWE IE */
		if (pAssocInd->owe_ie_len) {
			pMsg->owe_ie = qdf_mem_malloc(pAssocInd->owe_ie_len);
			if (!pMsg->owe_ie)
				return QDF_STATUS_E_NOMEM;
			qdf_mem_copy(pMsg->owe_ie, pAssocInd->owe_ie,
				     pAssocInd->owe_ie_len);
			pMsg->owe_ie_len = pAssocInd->owe_ie_len;
		}
		pMsg->need_assoc_rsp_tx_cb = pAssocInd->need_assoc_rsp_tx_cb;

		msg.type = pMsg->messageType;
		msg.bodyval = 0;
		msg.bodyptr = pMsg;
		/* pMsg is freed by umac_send_mb_message_to_mac in anycase*/
		status = scheduler_post_msg_by_priority(QDF_MODULE_ID_PE, &msg,
							true);
	} while (0);
	return status;
}

QDF_STATUS csr_send_mb_start_bss_req_msg(struct mac_context *mac, uint32_t
					sessionId, eCsrRoamBssType bssType,
					 struct csr_roamstart_bssparams *pParam,
					 struct bss_description *bss_desc)
{
	struct start_bss_req *pMsg;
	struct validate_bss_data candidate_info;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	pSession->joinFailStatusCode.status_code = eSIR_SME_SUCCESS;
	pSession->joinFailStatusCode.reasonCode = 0;
	pMsg = qdf_mem_malloc(sizeof(*pMsg));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_START_BSS_REQ;
	pMsg->vdev_id = sessionId;
	pMsg->length = sizeof(*pMsg);
	qdf_copy_macaddr(&pMsg->bssid, &pParam->bssid);
	/* self_mac_addr */
	qdf_copy_macaddr(&pMsg->self_macaddr, &pSession->self_mac_addr);
	/* beaconInterval */
	if (bss_desc && bss_desc->beaconInterval)
		candidate_info.beacon_interval = bss_desc->beaconInterval;
	else if (pParam->beaconInterval)
		candidate_info.beacon_interval = pParam->beaconInterval;
	else
		candidate_info.beacon_interval = MLME_CFG_BEACON_INTERVAL_DEF;

	candidate_info.chan_freq = pParam->operation_chan_freq;
	if_mgr_is_beacon_interval_valid(mac->pdev, sessionId,
					&candidate_info);

	/* Update the beacon Interval */
	pParam->beaconInterval = candidate_info.beacon_interval;
	pMsg->beaconInterval = candidate_info.beacon_interval;
	pMsg->dot11mode =
		csr_translate_to_wni_cfg_dot11_mode(mac,
						    pParam->uCfgDot11Mode);
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	pMsg->cc_switch_mode = mac->roam.configParam.cc_switch_mode;
#endif
	pMsg->bssType = csr_translate_bsstype_to_mac_type(bssType);
	qdf_mem_copy(&pMsg->ssId, &pParam->ssId, sizeof(pParam->ssId));
	pMsg->oper_ch_freq = pParam->operation_chan_freq;
	/* What should we really do for the cbmode. */
	pMsg->cbMode = (ePhyChanBondState) pParam->cbMode;
	pMsg->vht_channel_width = pParam->ch_params.ch_width;
	pMsg->center_freq_seg0 = pParam->ch_params.center_freq_seg0;
	pMsg->center_freq_seg1 = pParam->ch_params.center_freq_seg1;
	pMsg->sec_ch_offset = pParam->ch_params.sec_ch_offset;
	pMsg->privacy = pParam->privacy;
	pMsg->apUapsdEnable = pParam->ApUapsdEnable;
	pMsg->ssidHidden = pParam->ssidHidden;
	pMsg->fwdWPSPBCProbeReq = (uint8_t) pParam->fwdWPSPBCProbeReq;
	pMsg->protEnabled = (uint8_t) pParam->protEnabled;
	pMsg->obssProtEnabled = (uint8_t) pParam->obssProtEnabled;
	/* set cfg related to protection */
	pMsg->ht_capab = pParam->ht_protection;
	pMsg->authType = pParam->authType;
	pMsg->dtimPeriod = pParam->dtimPeriod;
	pMsg->wps_state = pParam->wps_state;
	pMsg->bssPersona = pParam->bssPersona;
	pMsg->txLdpcIniFeatureEnabled = mac->mlme_cfg->ht_caps.tx_ldpc_enable;

	pMsg->pmfCapable = pParam->mfpCapable;
	pMsg->pmfRequired = pParam->mfpRequired;

	if (pParam->nRSNIELength > sizeof(pMsg->rsnIE.rsnIEdata)) {
		qdf_mem_free(pMsg);
		return QDF_STATUS_E_INVAL;
	}
	pMsg->rsnIE.length = pParam->nRSNIELength;
	qdf_mem_copy(pMsg->rsnIE.rsnIEdata,
		     pParam->pRSNIE,
		     pParam->nRSNIELength);
	pMsg->nwType = (tSirNwType)pParam->sirNwType;
	qdf_mem_copy(&pMsg->operationalRateSet,
		     &pParam->operationalRateSet,
		     sizeof(tSirMacRateSet));
	qdf_mem_copy(&pMsg->extendedRateSet,
		     &pParam->extendedRateSet,
		     sizeof(tSirMacRateSet));

	pMsg->add_ie_params = pParam->add_ie_params;
	pMsg->obssEnabled = mac->roam.configParam.obssEnabled;
	pMsg->sap_dot11mc = pParam->sap_dot11mc;
	pMsg->vendor_vht_sap =
		mac->mlme_cfg->vht_caps.vht_cap_info.vendor_24ghz_band;
	pMsg->cac_duration_ms = pParam->cac_duration_ms;
	pMsg->dfs_regdomain = pParam->dfs_regdomain;
	pMsg->beacon_tx_rate = pParam->beacon_tx_rate;

	return umac_send_mb_message_to_mac(pMsg);
}

QDF_STATUS csr_send_mb_stop_bss_req_msg(struct mac_context *mac,
					uint32_t sessionId)
{
	struct stop_bss_req *pMsg;

	pMsg = qdf_mem_malloc(sizeof(*pMsg));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;
	pMsg->messageType = eWNI_SME_STOP_BSS_REQ;
	pMsg->sessionId = sessionId;
	pMsg->length = sizeof(*pMsg);
	pMsg->reasonCode = 0;
	wlan_mlme_get_bssid_vdev_id(mac->pdev, sessionId, &pMsg->bssid);

	return umac_send_mb_message_to_mac(pMsg);
}

#ifndef FEATURE_CM_ENABLE
QDF_STATUS csr_reassoc(struct mac_context *mac, uint32_t sessionId,
		       tCsrRoamModifyProfileFields *pModProfileFields,
		       uint32_t *pRoamId, bool fForce)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t roamId = 0;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if ((csr_is_conn_state_connected(mac, sessionId)) &&
	    (fForce || (qdf_mem_cmp(&pModProfileFields,
				     &pSession->connectedProfile.
				     modifyProfileFields,
				     sizeof(tCsrRoamModifyProfileFields))))) {
		roamId = GET_NEXT_ROAM_ID(&mac->roam);
		if (pRoamId)
			*pRoamId = roamId;

		status =
			csr_roam_issue_reassoc(mac, sessionId, NULL,
					       pModProfileFields,
					       eCsrSmeIssuedReassocToSameAP,
					       roamId, false);
	}
	return status;
}
#endif

/**
 * csr_store_oce_cfg_flags_in_vdev() - fill OCE flags from ini
 * @mac: mac_context.
 * @vdev: Pointer to pdev obj
 * @vdev_id: vdev_id
 *
 * This API will store the oce flags in vdev mlme priv object
 *
 * Return: none
 */
static void csr_store_oce_cfg_flags_in_vdev(struct mac_context *mac,
					    struct wlan_objmgr_pdev *pdev,
					    uint8_t vdev_id)
{
	uint8_t *vdev_dynamic_oce;
	struct wlan_objmgr_vdev *vdev =
	wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_LEGACY_MAC_ID);

	if (!vdev)
		return;

	vdev_dynamic_oce = mlme_get_dynamic_oce_flags(vdev);
	if (vdev_dynamic_oce)
		*vdev_dynamic_oce = mac->mlme_cfg->oce.feature_bitmap;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
}

static void csr_send_set_ie(uint8_t type, uint8_t sub_type,
			    uint8_t vdev_id)
{
	struct send_extcap_ie *msg;
	QDF_STATUS status;

	sme_debug("send SET IE msg to PE");

	if (!(type == WLAN_VDEV_MLME_TYPE_STA ||
	      (type == WLAN_VDEV_MLME_TYPE_AP &&
	      sub_type == WLAN_VDEV_MLME_SUBTYPE_P2P_DEVICE))) {
		sme_debug("Failed to send set IE req for vdev_%d", vdev_id);
		return;
	}

	msg = qdf_mem_malloc(sizeof(*msg));
	if (!msg)
		return;

	msg->msg_type = eWNI_SME_SET_IE_REQ;
	msg->session_id = vdev_id;
	msg->length = sizeof(*msg);
	status = umac_send_mb_message_to_mac(msg);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_debug("Failed to send set IE req for vdev_%d", vdev_id);
}

void csr_get_vdev_type_nss(enum QDF_OPMODE dev_mode, uint8_t *nss_2g,
			   uint8_t *nss_5g)
{
	struct mac_context *mac_ctx = sme_get_mac_context();

	if (!mac_ctx) {
		sme_err("Invalid MAC context");
		return;
	}

	switch (dev_mode) {
	case QDF_STA_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.sta;
		*nss_5g = mac_ctx->vdev_type_nss_5g.sta;
		break;
	case QDF_SAP_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.sap;
		*nss_5g = mac_ctx->vdev_type_nss_5g.sap;
		break;
	case QDF_P2P_CLIENT_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_cli;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_cli;
		break;
	case QDF_P2P_GO_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_go;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_go;
		break;
	case QDF_P2P_DEVICE_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_dev;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_dev;
		break;
	case QDF_IBSS_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.ibss;
		*nss_5g = mac_ctx->vdev_type_nss_5g.ibss;
		break;
	case QDF_OCB_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.ocb;
		*nss_5g = mac_ctx->vdev_type_nss_5g.ocb;
		break;
	case QDF_NAN_DISC_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.nan;
		*nss_5g = mac_ctx->vdev_type_nss_5g.nan;
		break;
	case QDF_NDI_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.ndi;
		*nss_5g = mac_ctx->vdev_type_nss_5g.ndi;
		break;
	default:
		*nss_2g = 1;
		*nss_5g = 1;
		sme_err("Unknown device mode");
		break;
	}
	sme_debug("mode - %d: nss_2g - %d, 5g - %d",
		  dev_mode, *nss_2g, *nss_5g);
}

QDF_STATUS csr_setup_vdev_session(struct vdev_mlme_obj *vdev_mlme)
{
	QDF_STATUS status;
	uint32_t existing_session_id;
	struct csr_roam_session *session;
	struct mlme_vht_capabilities_info *vht_cap_info;
	u8 vdev_id;
	struct qdf_mac_addr *mac_addr;
	mac_handle_t mac_handle;
	struct mac_context *mac_ctx;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_vht_config vht_config;
	struct wlan_ht_config ht_cap;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	mac_ctx = MAC_CONTEXT(mac_handle);
	if (!mac_ctx) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	if (!(mac_ctx->mlme_cfg)) {
		sme_err("invalid mlme cfg");
		return QDF_STATUS_E_FAILURE;
	}
	vht_cap_info = &mac_ctx->mlme_cfg->vht_caps.vht_cap_info;

	vdev = vdev_mlme->vdev;

	vdev_id = wlan_vdev_get_id(vdev);
	mac_addr = (struct qdf_mac_addr *)wlan_vdev_mlme_get_macaddr(vdev);

	/* check to see if the mac address already belongs to a session */
	status = csr_roam_get_session_id_from_bssid(mac_ctx, mac_addr,
						    &existing_session_id);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Session %d exists with mac address "QDF_MAC_ADDR_FMT,
			existing_session_id,
			QDF_MAC_ADDR_REF(mac_addr->bytes));
		return QDF_STATUS_E_FAILURE;
	}

	/* attempt to retrieve session for Id */
	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		sme_err("Session does not exist for interface %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	/* check to see if the session is already active */
	if (session->sessionActive) {
		sme_err("Cannot re-open active session with Id %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	session->sessionActive = true;
	session->sessionId = vdev_id;

	qdf_mem_copy(&session->self_mac_addr, mac_addr,
		     sizeof(struct qdf_mac_addr));
#ifndef FEATURE_CM_ENABLE
	/* Initialize FT related data structures only in STA mode */
	sme_ft_open(MAC_HANDLE(mac_ctx), session->sessionId);
	status = qdf_mc_timer_init(&session->hTimerRoaming,
				   QDF_TIMER_TYPE_SW,
				   csr_roam_roaming_timer_handler,
				   &session->roamingTimerInfo);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("cannot allocate memory for Roaming timer");
		return status;
	}
	status = qdf_mc_timer_init(&session->roaming_offload_timer,
				   QDF_TIMER_TYPE_SW,
				   csr_roam_roaming_offload_timeout_handler,
				   &session->roamingTimerInfo);

	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("timer init failed for join failure timer");
		return status;
	}
#endif

	ht_cap.caps = 0;
	vht_config.caps = 0;
	ht_cap.ht_caps = mac_ctx->mlme_cfg->ht_caps.ht_cap_info;
	vdev_mlme->proto.ht_info.ht_caps = ht_cap.caps;

	vht_config.max_mpdu_len = vht_cap_info->ampdu_len;
	vht_config.supported_channel_widthset =
			vht_cap_info->supp_chan_width;
	vht_config.ldpc_coding = vht_cap_info->ldpc_coding_cap;
	vht_config.shortgi80 = vht_cap_info->short_gi_80mhz;
	vht_config.shortgi160and80plus80 =
			vht_cap_info->short_gi_160mhz;
	vht_config.tx_stbc = vht_cap_info->tx_stbc;
	vht_config.rx_stbc = vht_cap_info->rx_stbc;
	vht_config.su_beam_former = vht_cap_info->su_bformer;
	vht_config.su_beam_formee = vht_cap_info->su_bformee;
	vht_config.csnof_beamformer_antSup =
			vht_cap_info->tx_bfee_ant_supp;
	vht_config.num_soundingdim = vht_cap_info->num_soundingdim;
	vht_config.mu_beam_former = vht_cap_info->mu_bformer;
	vht_config.mu_beam_formee = vht_cap_info->enable_mu_bformee;
	vht_config.vht_txops = vht_cap_info->txop_ps;
	vht_config.htc_vhtcap = vht_cap_info->htc_vhtc;
	vht_config.rx_antpattern = vht_cap_info->rx_antpattern;
	vht_config.tx_antpattern = vht_cap_info->tx_antpattern;

	vht_config.max_ampdu_lenexp =
			vht_cap_info->ampdu_len_exponent;
	vdev_mlme->proto.vht_info.caps = vht_config.caps;
	csr_update_session_he_cap(mac_ctx, session);
	csr_update_session_eht_cap(mac_ctx, session);

	csr_send_set_ie(vdev_mlme->mgmt.generic.type,
			vdev_mlme->mgmt.generic.subtype,
			wlan_vdev_get_id(vdev));

	if (vdev_mlme->mgmt.generic.type == WLAN_VDEV_MLME_TYPE_STA) {
		csr_store_oce_cfg_flags_in_vdev(mac_ctx, mac_ctx->pdev,
						wlan_vdev_get_id(vdev));
		wlan_mlme_update_oce_flags(mac_ctx->pdev);
	}

	return QDF_STATUS_SUCCESS;
}

void csr_cleanup_vdev_session(struct mac_context *mac, uint8_t vdev_id)
{
	if (CSR_IS_SESSION_VALID(mac, vdev_id)) {
		struct csr_roam_session *pSession = CSR_GET_SESSION(mac,
								vdev_id);

#ifndef FEATURE_CM_ENABLE
		csr_roam_stop_roaming_timer(mac, vdev_id);
		csr_free_connect_bss_desc(mac, vdev_id);
		/* Clean up FT related data structures */
		sme_ft_close(MAC_HANDLE(mac), vdev_id);
#endif
		csr_flush_roam_scan_chan_lists(mac, vdev_id);
		csr_roam_free_connect_profile(&pSession->connectedProfile);
		csr_roam_free_connected_info(mac, &pSession->connectedInfo);
#ifndef FEATURE_CM_ENABLE
		csr_roam_free_connected_info(mac,
					     &pSession->prev_assoc_ap_info);
		qdf_mc_timer_destroy(&pSession->hTimerRoaming);
		qdf_mc_timer_destroy(&pSession->roaming_offload_timer);
#endif
		csr_init_session(mac, vdev_id);
	}
}

QDF_STATUS csr_prepare_vdev_delete(struct mac_context *mac_ctx,
				   uint8_t vdev_id, bool cleanup)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session)
		return QDF_STATUS_E_INVAL;

	if (!CSR_IS_SESSION_VALID(mac_ctx, vdev_id))
		return QDF_STATUS_E_INVAL;

#ifndef FEATURE_CM_ENABLE
	/* Vdev going down stop roaming */
	session->fCancelRoaming = true;
#endif
	if (cleanup) {
		csr_cleanup_vdev_session(mac_ctx, vdev_id);
		return status;
	}

	if (CSR_IS_WAIT_FOR_KEY(mac_ctx, vdev_id)) {
		sme_debug("Stop Wait for key timer and change substate to eCSR_ROAM_SUBSTATE_NONE");
		cm_stop_wait_for_key_timer(mac_ctx->psoc, vdev_id);
		csr_roam_substate_change(mac_ctx, eCSR_ROAM_SUBSTATE_NONE,
					 vdev_id);
	}

	/* Flush all the commands for vdev */
	wlan_serialization_purge_all_cmd_by_vdev_id(mac_ctx->pdev, vdev_id);
	if (!mac_ctx->session_close_cb) {
		sme_err("no close session callback registered");
		return QDF_STATUS_E_FAILURE;
	}

	return status;
}

static void csr_init_session(struct mac_context *mac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession)
		return;

	pSession->sessionActive = false;
	pSession->sessionId = WLAN_UMAC_VDEV_ID_MAX;
	pSession->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	csr_saved_scan_cmd_free_fields(mac, pSession);
#endif
	csr_free_roam_profile(mac, sessionId);
	csr_roam_free_connect_profile(&pSession->connectedProfile);
	csr_roam_free_connected_info(mac, &pSession->connectedInfo);
	qdf_mem_zero(&pSession->self_mac_addr, sizeof(struct qdf_mac_addr));
#ifndef FEATURE_CM_ENABLE
	csr_roam_free_connected_info(mac,
				     &pSession->prev_assoc_ap_info);
	csr_free_connect_bss_desc(mac, sessionId);
	if (pSession->pWpaRsnReqIE) {
		qdf_mem_free(pSession->pWpaRsnReqIE);
		pSession->pWpaRsnReqIE = NULL;
	}
	pSession->nWpaRsnReqIeLength = 0;
#ifdef FEATURE_WLAN_WAPI
	if (pSession->pWapiReqIE) {
		qdf_mem_free(pSession->pWapiReqIE);
		pSession->pWapiReqIE = NULL;
	}
	pSession->nWapiReqIeLength = 0;
#endif /* FEATURE_WLAN_WAPI */
	if (pSession->pAddIEScan) {
		qdf_mem_free(pSession->pAddIEScan);
		pSession->pAddIEScan = NULL;
	}
	pSession->nAddIEScanLength = 0;
#endif
}

static void csr_get_vdev_id_from_bssid(struct wlan_objmgr_pdev *pdev,
				       void *object, void *arg)
{
	struct bssid_search_arg *bssid_arg = arg;
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	struct wlan_objmgr_peer *peer;

	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS)
		return;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_LEGACY_SME_ID);
	if (!peer)
		return;

	if (WLAN_ADDR_EQ(bssid_arg->peer_addr.bytes,
			 wlan_peer_get_macaddr(peer)) == QDF_STATUS_SUCCESS)
		bssid_arg->vdev_id = wlan_vdev_get_id(vdev);

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_SME_ID);
}

QDF_STATUS csr_roam_get_session_id_from_bssid(struct mac_context *mac,
					      struct qdf_mac_addr *bssid,
					      uint32_t *pSessionId)
{
	struct bssid_search_arg bssid_arg;

	qdf_copy_macaddr(&bssid_arg.peer_addr, bssid);
	bssid_arg.vdev_id = WLAN_MAX_VDEVS;
	wlan_objmgr_pdev_iterate_obj_list(mac->pdev, WLAN_VDEV_OP,
					  csr_get_vdev_id_from_bssid,
					  &bssid_arg, 0,
					  WLAN_LEGACY_SME_ID);
	if (bssid_arg.vdev_id >= WLAN_MAX_VDEVS) {
		*pSessionId = 0;
		return QDF_STATUS_E_FAILURE;
	}

	*pSessionId = bssid_arg.vdev_id;

	return QDF_STATUS_SUCCESS;
}

#ifndef FEATURE_CM_ENABLE
static void csr_roam_link_up(struct mac_context *mac, struct qdf_mac_addr bssid)
{
	uint32_t sessionId = 0;
	QDF_STATUS status;

	/*
	 * Update the current BSS info in ho control block based on connected
	 * profile info from pmac global structure
	 */

	sme_debug("WLAN link UP with AP= " QDF_MAC_ADDR_FMT,
		QDF_MAC_ADDR_REF(bssid.bytes));
	/* Indicate the neighbor roal algorithm about the connect indication */
	status = csr_roam_get_session_id_from_bssid(mac, &bssid, &sessionId);
	if (QDF_IS_STATUS_SUCCESS(status))
		csr_neighbor_roam_indicate_connect(mac, sessionId,
						   QDF_STATUS_SUCCESS);
}

static void csr_roam_link_down(struct mac_context *mac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	enum QDF_OPMODE opmode =
			wlan_get_opmode_from_vdev_id(mac->pdev, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return;
	}
	/* Only to handle the case for Handover on infra link */
	if (opmode != QDF_STA_MODE && opmode != QDF_P2P_CLIENT_MODE)
		return;
	/*
	 * Incase of station mode, immediately stop data transmission whenever
	 * link down is detected.
	 */
	if (csr_roam_is_sta_mode(mac, sessionId)
	    && !CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(mac, sessionId)
	    && !csr_roam_is11r_assoc(mac, sessionId)) {
		sme_debug("Inform Link lost for session %d",
			sessionId);
		csr_roam_call_callback(mac, sessionId, NULL, 0,
				       eCSR_ROAM_LOSTLINK,
				       eCSR_ROAM_RESULT_LOSTLINK);
	}
	/* Indicate the neighbor roal algorithm about the disconnect
	 * indication
	 */
	csr_neighbor_roam_indicate_disconnect(mac, sessionId);
}
#endif

QDF_STATUS csr_get_snr(struct mac_context *mac,
		       tCsrSnrCallback callback,
		       struct qdf_mac_addr bssId, void *pContext)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0};
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	tAniGetSnrReq *pMsg;

	pMsg = qdf_mem_malloc(sizeof(tAniGetSnrReq));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	status = csr_roam_get_session_id_from_bssid(mac, &bssId, &sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_free(pMsg);
		sme_err("Couldn't find session_id for given BSSID");
		return status;
	}

	pMsg->msgType = eWNI_SME_GET_SNR_REQ;
	pMsg->msgLen = (uint16_t) sizeof(tAniGetSnrReq);
	pMsg->sessionId = sessionId;
	pMsg->snrCallback = callback;
	pMsg->pDevContext = pContext;
	msg.type = eWNI_SME_GET_SNR_REQ;
	msg.bodyptr = pMsg;
	msg.reserved = 0;

	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 &msg)) {
		qdf_mem_free((void *)pMsg);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
QDF_STATUS csr_invoke_neighbor_report_request(
				uint8_t session_id,
				struct sRrmNeighborReq *neighbor_report_req,
				bool send_resp_to_host)
{
	struct wmi_invoke_neighbor_report_params *invoke_params;
	struct scheduler_msg msg = {0};

	if (!neighbor_report_req) {
		sme_err("Invalid params");
		return QDF_STATUS_E_INVAL;
	}

	invoke_params = qdf_mem_malloc(sizeof(*invoke_params));
	if (!invoke_params)
		return QDF_STATUS_E_NOMEM;

	invoke_params->vdev_id = session_id;
	invoke_params->send_resp_to_host = send_resp_to_host;

	if (!neighbor_report_req->no_ssid) {
		invoke_params->ssid.length = neighbor_report_req->ssid.length;
		qdf_mem_copy(invoke_params->ssid.ssid,
			     neighbor_report_req->ssid.ssId,
			     neighbor_report_req->ssid.length);
	} else {
		invoke_params->ssid.length = 0;
	}

	sme_debug("Sending SIR_HAL_INVOKE_NEIGHBOR_REPORT");

	msg.type = SIR_HAL_INVOKE_NEIGHBOR_REPORT;
	msg.reserved = 0;
	msg.bodyptr = invoke_params;

	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		sme_err("Not able to post message to WMA");
		qdf_mem_free(invoke_params);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_ESE
void wlan_cm_ese_populate_addtional_ies(struct wlan_objmgr_pdev *pdev,
			struct wlan_mlme_psoc_ext_obj *mlme_obj,
			uint8_t vdev_id,
			struct wlan_roam_scan_offload_params *rso_mode_cfg)
{
	uint8_t tspec_ie_hdr[SIR_MAC_OUI_WME_HDR_MIN]
			= { 0x00, 0x50, 0xf2, 0x02, 0x02, 0x01 };
	uint8_t tspec_ie_buf[DOT11F_IE_WMMTSPEC_MAX_LEN], j;
	ese_wmm_tspec_ie *tspec_ie;
	tESETspecInfo ese_tspec;
	struct mac_context *mac_ctx;
	struct csr_roam_session *session;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return;
	}

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		sme_err("session is null %d", vdev_id);
		return;
	}

	tspec_ie = (ese_wmm_tspec_ie *)(tspec_ie_buf + SIR_MAC_OUI_WME_HDR_MIN);
	if (csr_is_wmm_supported(mac_ctx) &&
	    mlme_obj->cfg.lfr.ese_enabled &&
	    wlan_cm_get_ese_assoc(pdev, session->sessionId)) {
		ese_tspec.numTspecs = sme_qos_ese_retrieve_tspec_info(
					mac_ctx, session->sessionId,
					(tTspecInfo *)&ese_tspec.tspec[0]);
		qdf_mem_copy(tspec_ie_buf, tspec_ie_hdr,
			     SIR_MAC_OUI_WME_HDR_MIN);
		for (j = 0; j < ese_tspec.numTspecs; j++) {
			/* Populate the tspec_ie */
			ese_populate_wmm_tspec(&ese_tspec.tspec[j].tspec,
					       tspec_ie);
			wlan_cm_append_assoc_ies(rso_mode_cfg,
						 WLAN_ELEMID_VENDOR,
						  DOT11F_IE_WMMTSPEC_MAX_LEN,
						 tspec_ie_buf);
		}
	}
}
#endif

uint8_t *wlan_cm_get_rrm_cap_ie_data(void)
{
	struct mac_context *mac_ctx;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return NULL;
	}

	return (uint8_t *)&mac_ctx->rrm.rrmPEContext.rrmEnabledCaps;
}

#ifndef FEATURE_CM_ENABLE
#if defined(WLAN_FEATURE_FILS_SK)
QDF_STATUS csr_update_fils_config(struct mac_context *mac, uint8_t session_id,
				  struct csr_roam_profile *src_profile)
{
	struct csr_roam_session *session = CSR_GET_SESSION(mac, session_id);
	struct csr_roam_profile *dst_profile = NULL;

	if (!session) {
		sme_err("session NULL");
		return QDF_STATUS_E_FAILURE;
	}

	dst_profile = session->pCurRoamProfile;

	if (!dst_profile) {
		sme_err("Current Roam profile of SME session NULL");
		return QDF_STATUS_E_FAILURE;
	}
	update_profile_fils_info(mac, dst_profile, src_profile,
				 session_id);
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_ADAPTIVE_11R
static bool
csr_is_adaptive_11r_roam_supported(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id)
{
	struct cm_roam_values_copy config;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	wlan_cm_roam_cfg_get_value(psoc, vdev_id,
				   ADAPTIVE_11R_CONNECTION,
				   &config);
	if (config.bool_value)
		return mlme_obj->cfg.lfr.tgt_adaptive_11r_cap;

	return true;
}
#else
static bool
csr_is_adaptive_11r_roam_supported(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id)

{
	return true;
}
#endif

QDF_STATUS
wlan_cm_roam_cmd_allowed(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			 uint8_t command, uint8_t reason)
{
	uint8_t *state = NULL;
	struct csr_roam_session *session;
	struct mac_context *mac_ctx;
	tpCsrNeighborRoamControlInfo roam_info;
	enum csr_akm_type roam_profile_akm = eCSR_AUTH_TYPE_UNKNOWN;
	uint32_t fw_akm_bitmap;
	bool p2p_disable_sta_roaming = 0, nan_disable_sta_roaming = 0;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	sme_debug("RSO Command %d, vdev %d, Reason %d",
		  command, vdev_id, reason);

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		mlme_err("session is null for vdev %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	roam_info = &mac_ctx->roam.neighborRoamInfo[vdev_id];

	if (roam_info->neighborRoamState !=
	    eCSR_NEIGHBOR_ROAM_STATE_CONNECTED &&
	    (command == ROAM_SCAN_OFFLOAD_UPDATE_CFG ||
	     command == ROAM_SCAN_OFFLOAD_START ||
	     command == ROAM_SCAN_OFFLOAD_RESTART)) {
		sme_debug("Session not in connected state, RSO not sent and state=%d ",
			  roam_info->neighborRoamState);
		return QDF_STATUS_E_FAILURE;
	}

	if (session->pCurRoamProfile)
		roam_profile_akm =
			session->pCurRoamProfile->AuthType.authType[0];
	else
		sme_info("Roam profile is NULL");

	if (CSR_IS_AKM_FILS(roam_profile_akm) &&
	    !mlme_obj->cfg.lfr.rso_user_config.is_fils_roaming_supported) {
		sme_info("FILS Roaming not suppprted by fw");
		return QDF_STATUS_E_NOSUPPORT;
	}

	if (!csr_is_adaptive_11r_roam_supported(mac_ctx->psoc, vdev_id)) {
		sme_info("Adaptive 11r Roaming not suppprted by fw");
		return QDF_STATUS_E_NOSUPPORT;
	}

	fw_akm_bitmap = mac_ctx->mlme_cfg->lfr.fw_akm_bitmap;
	/* Roaming is not supported currently for OWE akm */
	if (roam_profile_akm == eCSR_AUTH_TYPE_OWE &&
	    !CSR_IS_FW_OWE_ROAM_SUPPORTED(fw_akm_bitmap)) {
		sme_info("OWE Roaming not suppprted by fw");
		return QDF_STATUS_E_NOSUPPORT;
	}

	/* Roaming is not supported for SAE authentication */
	if (CSR_IS_AUTH_TYPE_SAE(roam_profile_akm) &&
	    !CSR_IS_FW_SAE_ROAM_SUPPORTED(fw_akm_bitmap)) {
		sme_info("Roaming not suppprted for SAE connection");
		return QDF_STATUS_E_NOSUPPORT;
	}

	if ((roam_profile_akm == eCSR_AUTH_TYPE_SUITEB_EAP_SHA256 ||
	     roam_profile_akm == eCSR_AUTH_TYPE_SUITEB_EAP_SHA384) &&
	     !CSR_IS_FW_SUITEB_ROAM_SUPPORTED(fw_akm_bitmap)) {
		sme_info("Roaming not supported for SUITEB connection");
		return QDF_STATUS_E_NOSUPPORT;
	}

	/*
	 * If fw doesn't advertise FT SAE, FT-FILS or FT-Suite-B capability,
	 * don't support roaming to that profile
	 */
	if (CSR_IS_AKM_FT_SAE(roam_profile_akm)) {
		if (!CSR_IS_FW_FT_SAE_SUPPORTED(fw_akm_bitmap)) {
			sme_info("Roaming not suppprted for FT SAE akm");
			return QDF_STATUS_E_NOSUPPORT;
		}
	}

	if (CSR_IS_AKM_FT_SUITEB_SHA384(roam_profile_akm)) {
		if (!CSR_IS_FW_FT_SUITEB_SUPPORTED(fw_akm_bitmap)) {
			sme_info("Roaming not suppprted for FT Suite-B akm");
			return QDF_STATUS_E_NOSUPPORT;
		}
	}

	if (CSR_IS_AKM_FT_FILS(roam_profile_akm)) {
		if (!CSR_IS_FW_FT_FILS_SUPPORTED(fw_akm_bitmap)) {
			sme_info("Roaming not suppprted for FT FILS akm");
			return QDF_STATUS_E_NOSUPPORT;
		}
	}

	p2p_disable_sta_roaming =
		(cfg_p2p_is_roam_config_disabled(psoc) &&
		(policy_mgr_mode_specific_connection_count(
					psoc, PM_P2P_CLIENT_MODE, NULL) ||
		policy_mgr_mode_specific_connection_count(
					psoc, PM_P2P_GO_MODE, NULL)));
	nan_disable_sta_roaming =
	    (cfg_nan_is_roam_config_disabled(psoc) &&
	    policy_mgr_mode_specific_connection_count(psoc, PM_NDI_MODE, NULL));

	if ((command == ROAM_SCAN_OFFLOAD_START ||
	     command == ROAM_SCAN_OFFLOAD_UPDATE_CFG) &&
	     (p2p_disable_sta_roaming || nan_disable_sta_roaming)) {
		sme_info("roaming not supported for active %s connection",
			 p2p_disable_sta_roaming ? "p2p" : "ndi");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * The Dynamic Config Items Update may happen even if the state is in
	 * INIT. It is important to ensure that the command is passed down to
	 * the FW only if the Infra Station is in a connected state. A connected
	 * station could also be in a PREAUTH or REASSOC states.
	 * 1) Block all CMDs that are not STOP in INIT State. For STOP always
	 *    inform firmware irrespective of state.
	 * 2) Block update cfg CMD if its for REASON_ROAM_SET_BLACKLIST_BSSID,
	 *    because we need to inform firmware of blacklisted AP for PNO in
	 *    all states.
	 */
	if ((roam_info->neighborRoamState == eCSR_NEIGHBOR_ROAM_STATE_INIT) &&
	    (command != ROAM_SCAN_OFFLOAD_STOP) &&
	    (reason != REASON_ROAM_SET_BLACKLIST_BSSID)) {
		state = mac_trace_get_neighbour_roam_state(
				roam_info->neighborRoamState);
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  FL("Scan Command not sent to FW state=%s and cmd=%d"),
			  state,  command);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_cm_roam_neighbor_proceed_with_handoff_req(uint8_t vdev_id)
{
	struct mac_context *mac_ctx;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return csr_neighbor_roam_proceed_with_handoff_req(mac_ctx, vdev_id);
}

QDF_STATUS
wlan_cm_roam_scan_offload_rsp(uint8_t vdev_id, uint8_t reason)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg cds_msg = {0};
	struct roam_offload_scan_rsp *scan_offload_rsp;

	if (reason == REASON_OS_REQUESTED_ROAMING_NOW) {
		scan_offload_rsp = qdf_mem_malloc(sizeof(*scan_offload_rsp));
		if (!scan_offload_rsp)
			return QDF_STATUS_E_NOMEM;

		cds_msg.type = eWNI_SME_ROAM_SCAN_OFFLOAD_RSP;
		scan_offload_rsp->sessionId = vdev_id;
		scan_offload_rsp->reason = reason;
		cds_msg.bodyptr = scan_offload_rsp;
		/*
		 * Since REASSOC request is processed in
		 * Roam_Scan_Offload_Rsp post a dummy rsp msg back to
		 * SME with proper reason code.
		 */
		status = scheduler_post_message(QDF_MODULE_ID_MLME,
						QDF_MODULE_ID_SME,
						QDF_MODULE_ID_SME,
						&cds_msg);
		if (QDF_IS_STATUS_ERROR(status))
			qdf_mem_free(scan_offload_rsp);
	}

	return status;
}

bool wlan_cm_is_sta_connected(uint8_t vdev_id)
{
	struct mac_context *mac_ctx;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return false;
	}

	return CSR_IS_ROAM_JOINED(mac_ctx, vdev_id);
}

QDF_STATUS
csr_roam_offload_scan_rsp_hdlr(struct mac_context *mac,
			       struct roam_offload_scan_rsp *scanOffloadRsp)
{
	switch (scanOffloadRsp->reason) {
	case 0:
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "Rsp for Roam Scan Offload with failure status");
		break;
	case REASON_OS_REQUESTED_ROAMING_NOW:
		csr_neighbor_roam_proceed_with_handoff_req(mac,
						scanOffloadRsp->sessionId);
		break;

	default:
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "Rsp for Roam Scan Offload with reason %d",
			  scanOffloadRsp->reason);
	}
	return QDF_STATUS_SUCCESS;
}
#endif
#endif

tSmeCmd *csr_get_command_buffer(struct mac_context *mac)
{
	tSmeCmd *pCmd = sme_get_command_buffer(mac);

	if (pCmd)
		mac->roam.sPendingCommands++;

	return pCmd;
}

static void csr_free_cmd_memory(struct mac_context *mac, tSmeCmd *pCommand)
{
	if (!pCommand) {
		sme_err("pCommand is NULL");
		return;
	}
	switch (pCommand->command) {
	case eSmeCommandRoam:
		csr_release_command_roam(mac, pCommand);
		break;
	case eSmeCommandWmStatusChange:
		csr_release_command_wm_status_change(mac, pCommand);
		break;
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	case e_sme_command_set_hw_mode:
		csr_release_command_set_hw_mode(mac, pCommand);
#endif
	default:
		break;
	}
}

void csr_release_command_buffer(struct mac_context *mac, tSmeCmd *pCommand)
{
	if (mac->roam.sPendingCommands > 0) {
		/*
		 * All command allocated through csr_get_command_buffer
		 * need to decrement the pending count when releasing
		 */
		mac->roam.sPendingCommands--;
		csr_free_cmd_memory(mac, pCommand);
		sme_release_command(mac, pCommand);
	} else {
		sme_err("no pending commands");
		QDF_ASSERT(0);
	}
}

void csr_release_command(struct mac_context *mac_ctx, tSmeCmd *sme_cmd)
{
	struct wlan_serialization_queued_cmd_info cmd_info;
	struct wlan_serialization_command cmd;
	struct wlan_objmgr_vdev *vdev;

	if (!sme_cmd) {
		sme_err("sme_cmd is NULL");
		return;
	}
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
			sme_cmd->vdev_id, WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Invalid vdev");
		return;
	}
	qdf_mem_zero(&cmd_info,
			sizeof(struct wlan_serialization_queued_cmd_info));
	cmd_info.cmd_id = sme_cmd->cmd_id;
	cmd_info.req_type = WLAN_SER_CANCEL_NON_SCAN_CMD;
	cmd_info.cmd_type = csr_get_cmd_type(sme_cmd);
	cmd_info.vdev = vdev;
	qdf_mem_zero(&cmd, sizeof(struct wlan_serialization_command));
	cmd.cmd_id = cmd_info.cmd_id;
	cmd.cmd_type = cmd_info.cmd_type;
	cmd.vdev = cmd_info.vdev;
	if (wlan_serialization_is_cmd_present_in_active_queue(
				mac_ctx->psoc, &cmd)) {
		cmd_info.queue_type = WLAN_SERIALIZATION_ACTIVE_QUEUE;
		wlan_serialization_remove_cmd(&cmd_info);
	} else if (wlan_serialization_is_cmd_present_in_pending_queue(
				mac_ctx->psoc, &cmd)) {
		cmd_info.queue_type = WLAN_SERIALIZATION_PENDING_QUEUE;
		wlan_serialization_cancel_request(&cmd_info);
	} else {
		sme_debug("can't find cmd_id %d cmd_type %d", cmd_info.cmd_id,
			  cmd_info.cmd_type);
	}
	if (cmd_info.vdev)
		wlan_objmgr_vdev_release_ref(cmd_info.vdev, WLAN_LEGACY_SME_ID);
}


static enum wlan_serialization_cmd_type csr_get_roam_cmd_type(
		tSmeCmd *sme_cmd)
{
	enum wlan_serialization_cmd_type cmd_type = WLAN_SER_CMD_MAX;

	switch (sme_cmd->u.roamCmd.roamReason) {
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	case eCsrForcedDisassoc:
	case eCsrForcedDeauth:
	case eCsrForcedDisassocMICFailure:
		cmd_type = WLAN_SER_CMD_VDEV_DISCONNECT;
		break;
#endif
	case eCsrHddIssued:
		if (CSR_IS_INFRA_AP(&sme_cmd->u.roamCmd.roamProfile) ||
		    CSR_IS_NDI(&sme_cmd->u.roamCmd.roamProfile))
			cmd_type = WLAN_SER_CMD_VDEV_START_BSS;
		else
			cmd_type = WLAN_SER_CMD_VDEV_CONNECT;
		break;
	/* This is temp ifdef will be removed in near future */
#ifndef FEATURE_CM_ENABLE
	case eCsrHddIssuedReassocToSameAP:
		cmd_type = WLAN_SER_CMD_HDD_ISSUE_REASSOC_SAME_AP;
		break;
	case eCsrSmeIssuedReassocToSameAP:
		cmd_type = WLAN_SER_CMD_SME_ISSUE_REASSOC_SAME_AP;
		break;
	case eCsrSmeIssuedDisassocForHandoff:
		cmd_type =
			WLAN_SER_CMD_SME_ISSUE_DISASSOC_FOR_HANDOFF;
		break;
	case eCsrSmeIssuedAssocToSimilarAP:
		cmd_type =
			WLAN_SER_CMD_SME_ISSUE_ASSOC_TO_SIMILAR_AP;
		break;
#endif
	case eCsrStopBss:
		cmd_type = WLAN_SER_CMD_VDEV_STOP_BSS;
		break;
#ifndef FEATURE_CM_ENABLE
	case eCsrSmeIssuedFTReassoc:
		cmd_type = WLAN_SER_CMD_SME_ISSUE_FT_REASSOC;
		break;
#endif
	case eCsrForcedDisassocSta:
		cmd_type = WLAN_SER_CMD_FORCE_DISASSOC_STA;
		break;
	case eCsrForcedDeauthSta:
		cmd_type = WLAN_SER_CMD_FORCE_DEAUTH_STA;
		break;
#ifndef FEATURE_CM_ENABLE
	case eCsrPerformPreauth:
		cmd_type = WLAN_SER_CMD_PERFORM_PRE_AUTH;
		break;
#endif
	default:
		break;
	}

	return cmd_type;
}

enum wlan_serialization_cmd_type csr_get_cmd_type(tSmeCmd *sme_cmd)
{
	enum wlan_serialization_cmd_type cmd_type = WLAN_SER_CMD_MAX;

	switch (sme_cmd->command) {
	case eSmeCommandRoam:
		cmd_type = csr_get_roam_cmd_type(sme_cmd);
		break;
	case eSmeCommandWmStatusChange:
		cmd_type = WLAN_SER_CMD_WM_STATUS_CHANGE;
		break;
	case eSmeCommandGetdisconnectStats:
		cmd_type =  WLAN_SER_CMD_GET_DISCONNECT_STATS;
		break;
	case eSmeCommandAddTs:
		cmd_type = WLAN_SER_CMD_ADDTS;
		break;
	case eSmeCommandDelTs:
		cmd_type = WLAN_SER_CMD_DELTS;
		break;
	case e_sme_command_set_hw_mode:
		cmd_type = WLAN_SER_CMD_SET_HW_MODE;
		break;
	case e_sme_command_nss_update:
		cmd_type = WLAN_SER_CMD_NSS_UPDATE;
		break;
	case e_sme_command_set_dual_mac_config:
		cmd_type = WLAN_SER_CMD_SET_DUAL_MAC_CONFIG;
		break;
	case e_sme_command_set_antenna_mode:
		cmd_type = WLAN_SER_CMD_SET_ANTENNA_MODE;
		break;
	default:
		break;
	}

	return cmd_type;
}

static uint32_t csr_get_monotonous_number(struct mac_context *mac_ctx)
{
	uint32_t cmd_id;
	uint32_t mask = 0x00FFFFFF, prefix = 0x0D000000;

	cmd_id = qdf_atomic_inc_return(&mac_ctx->global_cmd_id);
	cmd_id = (cmd_id & mask);
	cmd_id = (cmd_id | prefix);

	return cmd_id;
}

static void csr_fill_cmd_timeout(struct wlan_serialization_command *cmd)
{
	switch (cmd->cmd_type) {
#ifndef FEATURE_CM_ENABLE
	case WLAN_SER_CMD_VDEV_DISCONNECT:
#endif
	case WLAN_SER_CMD_WM_STATUS_CHANGE:
		cmd->cmd_timeout_duration = SME_CMD_VDEV_DISCONNECT_TIMEOUT;
		break;
	case WLAN_SER_CMD_VDEV_START_BSS:
		cmd->cmd_timeout_duration = SME_CMD_VDEV_START_BSS_TIMEOUT;
		break;
	case WLAN_SER_CMD_VDEV_STOP_BSS:
		cmd->cmd_timeout_duration = SME_CMD_STOP_BSS_CMD_TIMEOUT;
		break;
	case WLAN_SER_CMD_FORCE_DISASSOC_STA:
	case WLAN_SER_CMD_FORCE_DEAUTH_STA:
		cmd->cmd_timeout_duration = SME_CMD_PEER_DISCONNECT_TIMEOUT;
		break;
	case WLAN_SER_CMD_GET_DISCONNECT_STATS:
		cmd->cmd_timeout_duration =
					SME_CMD_GET_DISCONNECT_STATS_TIMEOUT;
		break;
#ifndef FEATURE_CM_ENABLE
	case WLAN_SER_CMD_HDD_ISSUE_REASSOC_SAME_AP:
	case WLAN_SER_CMD_SME_ISSUE_REASSOC_SAME_AP:
	case WLAN_SER_CMD_SME_ISSUE_DISASSOC_FOR_HANDOFF:
	case WLAN_SER_CMD_SME_ISSUE_ASSOC_TO_SIMILAR_AP:
	case WLAN_SER_CMD_SME_ISSUE_FT_REASSOC:
	case WLAN_SER_CMD_PERFORM_PRE_AUTH:
		cmd->cmd_timeout_duration = SME_CMD_ROAM_CMD_TIMEOUT;
		break;
#endif
	case WLAN_SER_CMD_ADDTS:
	case WLAN_SER_CMD_DELTS:
		cmd->cmd_timeout_duration = SME_CMD_ADD_DEL_TS_TIMEOUT;
		break;
	case WLAN_SER_CMD_SET_HW_MODE:
	case WLAN_SER_CMD_NSS_UPDATE:
	case WLAN_SER_CMD_SET_DUAL_MAC_CONFIG:
	case WLAN_SER_CMD_SET_ANTENNA_MODE:
		cmd->cmd_timeout_duration = SME_CMD_POLICY_MGR_CMD_TIMEOUT;
		break;
	case WLAN_SER_CMD_VDEV_CONNECT:
		/* fallthrough to use def MAX value */
	default:
		cmd->cmd_timeout_duration = SME_ACTIVE_LIST_CMD_TIMEOUT_VALUE;
		break;
	}
}

QDF_STATUS csr_set_serialization_params_to_cmd(struct mac_context *mac_ctx,
		tSmeCmd *sme_cmd, struct wlan_serialization_command *cmd,
		uint8_t high_priority)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!sme_cmd) {
		sme_err("Invalid sme_cmd");
		return status;
	}
	if (!cmd) {
		sme_err("Invalid serialization_cmd");
		return status;
	}

	/*
	 * no need to fill command id for non-scan as they will be
	 * zero always
	 */
	sme_cmd->cmd_id = csr_get_monotonous_number(mac_ctx);
	cmd->cmd_id = sme_cmd->cmd_id;

	cmd->cmd_type = csr_get_cmd_type(sme_cmd);
	if (cmd->cmd_type == WLAN_SER_CMD_MAX) {
		sme_err("serialization enum not found for sme_cmd type %d",
			sme_cmd->command);
		return status;
	}
	cmd->vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
				sme_cmd->vdev_id, WLAN_LEGACY_SME_ID);
	if (!cmd->vdev) {
		sme_err("vdev is NULL for vdev_id:%d", sme_cmd->vdev_id);
		return status;
	}
	cmd->umac_cmd = sme_cmd;

	csr_fill_cmd_timeout(cmd);

	cmd->source = WLAN_UMAC_COMP_MLME;
	cmd->cmd_cb = sme_ser_cmd_callback;
	cmd->is_high_priority = high_priority;
	cmd->is_blocking = true;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_queue_sme_command(struct mac_context *mac_ctx, tSmeCmd *sme_cmd,
				 bool high_priority)
{
	struct wlan_serialization_command cmd;
	struct wlan_objmgr_vdev *vdev = NULL;
	enum wlan_serialization_status ser_cmd_status;
	QDF_STATUS status;

	if (!SME_IS_START(mac_ctx)) {
		sme_err("Sme in stop state");
		QDF_ASSERT(0);
		goto error;
	}

#ifndef FEATURE_CM_ENABLE
	if (CSR_IS_WAIT_FOR_KEY(mac_ctx, sme_cmd->vdev_id)) {
		if (!CSR_IS_DISCONNECT_COMMAND(sme_cmd)) {
			sme_err("Can't process cmd(%d), waiting for key",
				sme_cmd->command);
			goto error;
		}
	}
#endif
	qdf_mem_zero(&cmd, sizeof(struct wlan_serialization_command));
	status = csr_set_serialization_params_to_cmd(mac_ctx, sme_cmd,
					&cmd, high_priority);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failed to set ser params");
		goto error;
	}

	vdev = cmd.vdev;
	ser_cmd_status = wlan_serialization_request(&cmd);

	switch (ser_cmd_status) {
	case WLAN_SER_CMD_PENDING:
	case WLAN_SER_CMD_ACTIVE:
		/* Command posted to active/pending list */
		status = QDF_STATUS_SUCCESS;
		break;
	default:
		sme_err("Failed to queue command %d with status:%d",
			  sme_cmd->command, ser_cmd_status);
		status = QDF_STATUS_E_FAILURE;
		goto error;
	}

	return status;

error:
	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	csr_release_command_buffer(mac_ctx, sme_cmd);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS csr_roam_update_config(struct mac_context *mac_ctx, uint8_t session_id,
				  uint16_t capab, uint32_t value)
{
	struct update_config *msg;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	sme_debug("update HT config requested");
	if (!session) {
		sme_err("Session does not exist for session id %d", session_id);
		return QDF_STATUS_E_FAILURE;
	}

	msg = qdf_mem_malloc(sizeof(*msg));
	if (!msg)
		return QDF_STATUS_E_NOMEM;

	msg->messageType = eWNI_SME_UPDATE_CONFIG;
	msg->vdev_id = session_id;
	msg->capab = capab;
	msg->value = value;
	msg->length = sizeof(*msg);
	status = umac_send_mb_message_to_mac(msg);

	return status;
}

#ifndef FEATURE_CM_ENABLE
/* Returns whether a session is in QDF_STA_MODE...or not */
bool csr_roam_is_sta_mode(struct mac_context *mac, uint8_t vdev_id)
{
	enum QDF_OPMODE opmode;

	if (!CSR_IS_SESSION_VALID(mac, vdev_id)) {
		sme_err("Inactive session_id: %d", vdev_id);
		return false;
	}
	opmode = wlan_get_opmode_from_vdev_id(mac->pdev, vdev_id);
	if (opmode == QDF_STA_MODE)
		return true;

	sme_debug("Wrong opmode %d", opmode);

	return false;
}

QDF_STATUS csr_handoff_request(struct mac_context *mac,
			       uint8_t sessionId,
			       tCsrHandoffRequest *pHandoffInfo)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0};

	tAniHandoffReq *pMsg;

	pMsg = qdf_mem_malloc(sizeof(tAniHandoffReq));
	if (!pMsg) {
		return QDF_STATUS_E_NOMEM;
	}
	pMsg->msgType = eWNI_SME_HANDOFF_REQ;
	pMsg->msgLen = (uint16_t) sizeof(tAniHandoffReq);
	pMsg->sessionId = sessionId;
	pMsg->ch_freq = pHandoffInfo->ch_freq;
	pMsg->handoff_src = pHandoffInfo->src;
	qdf_mem_copy(pMsg->bssid, pHandoffInfo->bssid.bytes, QDF_MAC_ADDR_SIZE);
	msg.type = eWNI_SME_HANDOFF_REQ;
	msg.bodyptr = pMsg;
	msg.reserved = 0;
	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 &msg)) {
		qdf_mem_free((void *)pMsg);
		status = QDF_STATUS_E_FAILURE;
	}
	return status;
}
#endif

/**
 * csr_roam_channel_change_req() - Post channel change request to LIM
 * @mac: mac context
 * @bssid: SAP bssid
 * @ch_params: channel information
 * @profile: CSR profile
 *
 * This API is primarily used to post Channel Change Req for SAP
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_roam_channel_change_req(struct mac_context *mac,
				       struct qdf_mac_addr bssid,
				       struct ch_params *ch_params,
				       struct csr_roam_profile *profile)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirChanChangeRequest *msg;
	struct csr_roamstart_bssparams param;
	bool skip_hostapd_rate = !profile->chan_switch_hostapd_rate_enabled;

	/*
	 * while changing the channel, use basic rates given by driver
	 * and not by hostapd as there is a chance that hostapd might
	 * give us rates based on original channel which may not be
	 * suitable for new channel
	 */
	qdf_mem_zero(&param, sizeof(struct csr_roamstart_bssparams));

	status = csr_roam_get_bss_start_parms(mac, profile, &param,
					      skip_hostapd_rate);

	if (status != QDF_STATUS_SUCCESS) {
		sme_err("Failed to get bss parameters");
		return status;
	}

	msg = qdf_mem_malloc(sizeof(tSirChanChangeRequest));
	if (!msg)
		return QDF_STATUS_E_NOMEM;

	msg->messageType = eWNI_SME_CHANNEL_CHANGE_REQ;
	msg->messageLen = sizeof(tSirChanChangeRequest);
	msg->target_chan_freq = profile->ChannelInfo.freq_list[0];
	msg->sec_ch_offset = ch_params->sec_ch_offset;
	msg->ch_width = profile->ch_params.ch_width;
	msg->dot11mode = csr_translate_to_wni_cfg_dot11_mode(mac,
					param.uCfgDot11Mode);
	if (WLAN_REG_IS_24GHZ_CH_FREQ(msg->target_chan_freq) &&
	    !mac->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band &&
	    (msg->dot11mode == MLME_DOT11_MODE_11AC ||
	    msg->dot11mode == MLME_DOT11_MODE_11AC_ONLY))
		msg->dot11mode = MLME_DOT11_MODE_11N;
	msg->nw_type = param.sirNwType;
	msg->center_freq_seg_0 = ch_params->center_freq_seg0;
	msg->center_freq_seg_1 = ch_params->center_freq_seg1;
	msg->cac_duration_ms = profile->cac_duration_ms;
	msg->dfs_regdomain = profile->dfs_regdomain;
	qdf_mem_copy(msg->bssid, bssid.bytes, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(&msg->operational_rateset,
		     &param.operationalRateSet,
		     sizeof(msg->operational_rateset));
	qdf_mem_copy(&msg->extended_rateset, &param.extendedRateSet,
		     sizeof(msg->extended_rateset));

	status = umac_send_mb_message_to_mac(msg);

	return status;
}

/*
 * Post Beacon Tx Start request to LIM
 * immediately after SAP CAC WAIT is
 * completed without any RADAR indications.
 */
QDF_STATUS csr_roam_start_beacon_req(struct mac_context *mac,
				     struct qdf_mac_addr bssid,
				     uint8_t dfsCacWaitStatus)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirStartBeaconIndication *pMsg;

	pMsg = qdf_mem_malloc(sizeof(tSirStartBeaconIndication));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_START_BEACON_REQ;
	pMsg->messageLen = sizeof(tSirStartBeaconIndication);
	pMsg->beaconStartStatus = dfsCacWaitStatus;
	qdf_mem_copy(pMsg->bssid, bssid.bytes, QDF_MAC_ADDR_SIZE);

	status = umac_send_mb_message_to_mac(pMsg);

	return status;
}

/*
 * csr_roam_modify_add_ies -
 * This function sends msg to modify the additional IE buffers in PE
 *
 * @mac: mac global structure
 * @pModifyIE: pointer to tSirModifyIE structure
 * @updateType: Type of buffer
 *
 *
 * Return: QDF_STATUS -  Success or failure
 */
QDF_STATUS
csr_roam_modify_add_ies(struct mac_context *mac,
			 tSirModifyIE *pModifyIE, eUpdateIEsType updateType)
{
	tpSirModifyIEsInd pModifyAddIEInd = NULL;
	uint8_t *pLocalBuffer = NULL;
	QDF_STATUS status;

	/* following buffer will be freed by consumer (PE) */
	pLocalBuffer = qdf_mem_malloc(pModifyIE->ieBufferlength);
	if (!pLocalBuffer)
		return QDF_STATUS_E_NOMEM;

	pModifyAddIEInd = qdf_mem_malloc(sizeof(tSirModifyIEsInd));
	if (!pModifyAddIEInd) {
		qdf_mem_free(pLocalBuffer);
		return QDF_STATUS_E_NOMEM;
	}

	/*copy the IE buffer */
	qdf_mem_copy(pLocalBuffer, pModifyIE->pIEBuffer,
		     pModifyIE->ieBufferlength);
	qdf_mem_zero(pModifyAddIEInd, sizeof(tSirModifyIEsInd));

	pModifyAddIEInd->msgType = eWNI_SME_MODIFY_ADDITIONAL_IES;
	pModifyAddIEInd->msgLen = sizeof(tSirModifyIEsInd);

	qdf_copy_macaddr(&pModifyAddIEInd->modifyIE.bssid, &pModifyIE->bssid);

	pModifyAddIEInd->modifyIE.vdev_id = pModifyIE->vdev_id;
	pModifyAddIEInd->modifyIE.notify = pModifyIE->notify;
	pModifyAddIEInd->modifyIE.ieID = pModifyIE->ieID;
	pModifyAddIEInd->modifyIE.ieIDLen = pModifyIE->ieIDLen;
	pModifyAddIEInd->modifyIE.pIEBuffer = pLocalBuffer;
	pModifyAddIEInd->modifyIE.ieBufferlength = pModifyIE->ieBufferlength;
	pModifyAddIEInd->modifyIE.oui_length = pModifyIE->oui_length;

	pModifyAddIEInd->updateType = updateType;

	status = umac_send_mb_message_to_mac(pModifyAddIEInd);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to send eWNI_SME_UPDATE_ADDTIONAL_IES msg status %d",
			status);
		qdf_mem_free(pLocalBuffer);
	}
	return status;
}

/*
 * csr_roam_update_add_ies -
 * This function sends msg to updates the additional IE buffers in PE
 *
 * @mac: mac global structure
 * @sessionId: SME session id
 * @bssid: BSSID
 * @additionIEBuffer: buffer containing addition IE from hostapd
 * @length: length of buffer
 * @updateType: Type of buffer
 * @append: append or replace completely
 *
 *
 * Return: QDF_STATUS -  Success or failure
 */
QDF_STATUS
csr_roam_update_add_ies(struct mac_context *mac,
			 tSirUpdateIE *pUpdateIE, eUpdateIEsType updateType)
{
	tpSirUpdateIEsInd pUpdateAddIEs = NULL;
	uint8_t *pLocalBuffer = NULL;
	QDF_STATUS status;

	if (pUpdateIE->ieBufferlength != 0) {
		/* Following buffer will be freed by consumer (PE) */
		pLocalBuffer = qdf_mem_malloc(pUpdateIE->ieBufferlength);
		if (!pLocalBuffer) {
			return QDF_STATUS_E_NOMEM;
		}
		qdf_mem_copy(pLocalBuffer, pUpdateIE->pAdditionIEBuffer,
			     pUpdateIE->ieBufferlength);
	}

	pUpdateAddIEs = qdf_mem_malloc(sizeof(tSirUpdateIEsInd));
	if (!pUpdateAddIEs) {
		qdf_mem_free(pLocalBuffer);
		return QDF_STATUS_E_NOMEM;
	}

	pUpdateAddIEs->msgType = eWNI_SME_UPDATE_ADDITIONAL_IES;
	pUpdateAddIEs->msgLen = sizeof(tSirUpdateIEsInd);

	qdf_copy_macaddr(&pUpdateAddIEs->updateIE.bssid, &pUpdateIE->bssid);

	pUpdateAddIEs->updateIE.vdev_id = pUpdateIE->vdev_id;
	pUpdateAddIEs->updateIE.append = pUpdateIE->append;
	pUpdateAddIEs->updateIE.notify = pUpdateIE->notify;
	pUpdateAddIEs->updateIE.ieBufferlength = pUpdateIE->ieBufferlength;
	pUpdateAddIEs->updateIE.pAdditionIEBuffer = pLocalBuffer;

	pUpdateAddIEs->updateType = updateType;

	status = umac_send_mb_message_to_mac(pUpdateAddIEs);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to send eWNI_SME_UPDATE_ADDTIONAL_IES msg status %d",
			status);
		qdf_mem_free(pLocalBuffer);
	}
	return status;
}

/**
 * csr_send_ext_change_freq()- function to post send ECSA
 * action frame to lim.
 * @mac_ctx: pointer to global mac structure
 * @ch_freq: new channel freq to switch
 * @session_id: senssion it should be sent on.
 *
 * This function is called to post ECSA frame to lim.
 *
 * Return: success if msg posted to LIM else return failure
 */
QDF_STATUS csr_send_ext_change_freq(struct mac_context *mac_ctx,
				    qdf_freq_t ch_freq, uint8_t session_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct sir_sme_ext_cng_chan_req *msg;

	msg = qdf_mem_malloc(sizeof(*msg));
	if (!msg)
		return QDF_STATUS_E_NOMEM;

	msg->message_type = eWNI_SME_EXT_CHANGE_CHANNEL;
	msg->length = sizeof(*msg);
	msg->new_ch_freq = ch_freq;
	msg->vdev_id = session_id;
	status = umac_send_mb_message_to_mac(msg);
	return status;
}

QDF_STATUS csr_csa_restart(struct mac_context *mac_ctx, uint8_t session_id)
{
	QDF_STATUS status;
	struct scheduler_msg message = {0};

	/* Serialize the req through MC thread */
	message.bodyval = session_id;
	message.type    = eWNI_SME_CSA_RESTART_REQ;
	status = scheduler_post_message(QDF_MODULE_ID_SME, QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("scheduler_post_msg failed!(err=%d)", status);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

/**
 * csr_roam_send_chan_sw_ie_request() - Request to transmit CSA IE
 * @mac_ctx:        Global MAC context
 * @bssid:          BSSID
 * @target_chan_freq: Channel frequency on which to send the IE
 * @csa_ie_reqd:    Include/Exclude CSA IE.
 * @ch_params:  operating Channel related information
 *
 * This function sends request to transmit channel switch announcement
 * IE to lower layers
 *
 * Return: success or failure
 **/
QDF_STATUS csr_roam_send_chan_sw_ie_request(struct mac_context *mac_ctx,
					    struct qdf_mac_addr bssid,
					    uint32_t target_chan_freq,
					    uint8_t csa_ie_reqd,
					    struct ch_params *ch_params)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirDfsCsaIeRequest *msg;

	msg = qdf_mem_malloc(sizeof(tSirDfsCsaIeRequest));
	if (!msg)
		return QDF_STATUS_E_NOMEM;

	msg->msgType = eWNI_SME_DFS_BEACON_CHAN_SW_IE_REQ;
	msg->msgLen = sizeof(tSirDfsCsaIeRequest);

	msg->target_chan_freq = target_chan_freq;
	msg->csaIeRequired = csa_ie_reqd;
	msg->ch_switch_beacon_cnt =
		 mac_ctx->sap.SapDfsInfo.sap_ch_switch_beacon_cnt;
	msg->ch_switch_mode = mac_ctx->sap.SapDfsInfo.sap_ch_switch_mode;
	msg->dfs_ch_switch_disable =
		mac_ctx->sap.SapDfsInfo.disable_dfs_ch_switch;
	qdf_mem_copy(msg->bssid, bssid.bytes, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(&msg->ch_params, ch_params, sizeof(struct ch_params));

	status = umac_send_mb_message_to_mac(msg);

	return status;
}

QDF_STATUS csr_sta_continue_csa(struct mac_context *mac_ctx, uint8_t vdev_id)
{
	QDF_STATUS status;
	struct scheduler_msg message = {0};

	/* Serialize the req through MC thread */
	message.bodyval = vdev_id;
	message.type    = eWNI_SME_STA_CSA_CONTINUE_REQ;
	status = scheduler_post_message(QDF_MODULE_ID_SME, QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("eWNI_SME_STA_CSA_CONTINUE_REQ failed!(err=%d)",
			status);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

#ifndef FEATURE_CM_ENABLE
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
/**
 * csr_roaming_report_diag_event() - Diag events for LFR3
 * @mac_ctx:              MAC context
 * @roam_synch_ind_ptr:   Roam Synch Indication Pointer
 * @reason:               Reason for this event to happen
 *
 * The major events in the host for LFR3 roaming such as
 * roam synch indication, roam synch completion and
 * roam synch handoff fail will be indicated to the
 * diag framework using this API.
 *
 * Return: None
 */
static void csr_roaming_report_diag_event(struct mac_context *mac_ctx,
		struct roam_offload_synch_ind *roam_synch_ind_ptr,
		enum diagwlan_status_eventreason reason)
{
	WLAN_HOST_DIAG_EVENT_DEF(roam_connection,
		host_event_wlan_status_payload_type);
	qdf_mem_zero(&roam_connection,
		sizeof(host_event_wlan_status_payload_type));
	switch (reason) {
	case DIAG_REASON_ROAM_SYNCH_IND:
		roam_connection.eventId = DIAG_WLAN_STATUS_CONNECT;
		if (roam_synch_ind_ptr) {
			roam_connection.rssi = roam_synch_ind_ptr->rssi;
			roam_connection.channel =
				cds_freq_to_chan(roam_synch_ind_ptr->chan_freq);
		}
		break;
	case DIAG_REASON_ROAM_SYNCH_CNF:
		roam_connection.eventId = DIAG_WLAN_STATUS_CONNECT;
		break;
	case DIAG_REASON_ROAM_HO_FAIL:
		roam_connection.eventId = DIAG_WLAN_STATUS_DISCONNECT;
		break;
	default:
		sme_err("LFR3: Unsupported reason %d", reason);
		return;
	}
	roam_connection.reason = reason;
	WLAN_HOST_DIAG_EVENT_REPORT(&roam_connection, EVENT_WLAN_STATUS_V2);
}
#endif

void csr_process_ho_fail_ind(struct mac_context *mac_ctx, void *msg_buf)
{
	struct handoff_failure_ind *pSmeHOFailInd = msg_buf;
	struct mlme_roam_after_data_stall *vdev_roam_params;
	struct wlan_objmgr_vdev *vdev;
	struct reject_ap_info ap_info;
	uint32_t sessionId;

	if (!pSmeHOFailInd) {
		sme_err("LFR3: Hand-Off Failure Ind is NULL");
		return;
	}

	sessionId = pSmeHOFailInd->vdev_id;
	ap_info.bssid = pSmeHOFailInd->bssid;
	ap_info.reject_ap_type = DRIVER_AVOID_TYPE;
	ap_info.reject_reason = REASON_ROAM_HO_FAILURE;
	ap_info.source = ADDED_BY_DRIVER;
	wlan_blm_add_bssid_to_reject_list(mac_ctx->pdev, &ap_info);

	/* Roaming is supported only on Infra STA Mode. */
	if (!csr_roam_is_sta_mode(mac_ctx, sessionId)) {
		sme_err("LFR3:HO Fail cannot be handled for session %d",
			sessionId);
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, sessionId,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("LFR3: vdev is NULL");
		return;
	}

	vdev_roam_params = mlme_get_roam_invoke_params(vdev);
	if (vdev_roam_params)
		vdev_roam_params->roam_invoke_in_progress = false;
	else
		sme_err("LFR3: Vdev roam params is NULL");

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	mac_ctx->sme.set_connection_info_cb(false);

	csr_roam_roaming_offload_timer_action(mac_ctx, 0, sessionId,
			ROAMING_OFFLOAD_TIMER_STOP);
	csr_roam_call_callback(mac_ctx, sessionId, NULL, 0,
			eCSR_ROAM_NAPI_OFF, eCSR_ROAM_RESULT_FAILURE);
	csr_roam_synch_clean_up(mac_ctx, sessionId);
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	csr_roaming_report_diag_event(mac_ctx, NULL,
			DIAG_REASON_ROAM_HO_FAIL);
#endif
	csr_roam_disconnect(mac_ctx, sessionId,
			eCSR_DISCONNECT_REASON_ROAM_HO_FAIL,
			REASON_FW_TRIGGERED_ROAM_FAILURE);
	if (mac_ctx->mlme_cfg->gen.fatal_event_trigger)
		cds_flush_logs(WLAN_LOG_TYPE_FATAL,
				WLAN_LOG_INDICATOR_HOST_DRIVER,
				WLAN_LOG_REASON_ROAM_HO_FAILURE,
				false, false);
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */
#endif

/**
 * csr_update_op_class_array() - update op class for each band
 * @mac_ctx:          mac global context
 * @op_classes:       out param, operating class array to update
 * @channel_info:     channel info
 * @ch_name:          channel band name to display in debug messages
 * @i:                out param, stores number of operating classes
 *
 * Return: void
 */
static void
csr_update_op_class_array(struct mac_context *mac_ctx,
			  uint8_t *op_classes,
			  struct csr_channel *channel_info,
			  char *ch_name,
			  uint8_t *i)
{
	uint8_t j = 0, idx = 0, class = 0;
	bool found = false;
	uint8_t num_channels = channel_info->numChannels;
	uint8_t ch_num;

	sme_debug("Num %s channels,  %d", ch_name, num_channels);

	for (idx = 0; idx < num_channels &&
		*i < (REG_MAX_SUPP_OPER_CLASSES - 1); idx++) {
		wlan_reg_freq_to_chan_op_class(
			mac_ctx->pdev, channel_info->channel_freq_list[idx],
			true, BIT(BEHAV_NONE), &class, &ch_num);

		found = false;
		for (j = 0; j < REG_MAX_SUPP_OPER_CLASSES - 1; j++) {
			if (op_classes[j] == class) {
				found = true;
				break;
			}
		}

		if (!found) {
			op_classes[*i] = class;
			*i = *i + 1;
		}
	}
}

/**
 * csr_init_operating_classes() - update op class for all bands
 * @mac: pointer to mac context.
 *
 * Return: void
 */
static void csr_init_operating_classes(struct mac_context *mac)
{
	uint8_t i = 0;
	uint8_t j = 0;
	uint8_t swap = 0;
	uint8_t numClasses = 0;
	uint8_t opClasses[REG_MAX_SUPP_OPER_CLASSES] = {0,};

	sme_debug("Current Country = %c%c",
		  mac->scan.countryCodeCurrent[0],
		  mac->scan.countryCodeCurrent[1]);

	csr_update_op_class_array(mac, opClasses,
				  &mac->scan.base_channels, "20MHz", &i);
	numClasses = i;

	/* As per spec the operating classes should be in ascending order.
	 * Bubble sort is fine since we don't have many classes
	 */
	for (i = 0; i < (numClasses - 1); i++) {
		for (j = 0; j < (numClasses - i - 1); j++) {
			/* For decreasing order use < */
			if (opClasses[j] > opClasses[j + 1]) {
				swap = opClasses[j];
				opClasses[j] = opClasses[j + 1];
				opClasses[j + 1] = swap;
			}
		}
	}

	/* Set the ordered list of op classes in regdomain
	 * for use by other modules
	 */
	wlan_reg_dmn_set_curr_opclasses(numClasses, &opClasses[0]);
}

#ifndef FEATURE_CM_ENABLE
/**
 * csr_find_session_by_type() - This function will find given session type from
 * all sessions.
 * @mac_ctx: pointer to mac context.
 * @type:    session type
 *
 * Return: session id for give session type.
 **/
static uint32_t
csr_find_session_by_type(struct mac_context *mac_ctx, enum QDF_OPMODE type)
{
	uint32_t i, session_id = WLAN_UMAC_VDEV_ID_MAX;
	struct csr_roam_session *session_ptr;

	for (i = 0; i < WLAN_MAX_VDEVS; i++) {
		if (!CSR_IS_SESSION_VALID(mac_ctx, i))
			continue;

		session_ptr = CSR_GET_SESSION(mac_ctx, i);
		if (type == session_ptr->bssParams.bssPersona) {
			session_id = i;
			break;
		}
	}
	return session_id;
}

/**
 * csr_is_conn_allow_2g_band() - This function will check if station's conn
 * is allowed in 2.4Ghz band.
 * @mac_ctx: pointer to mac context.
 * @chnl: station's channel.
 *
 * This function will check if station's connection is allowed in 5Ghz band
 * after comparing it with SAP's operating channel. If SAP's operating
 * channel and Station's channel is different than this function will return
 * false else true.
 *
 * Return: true or false.
 **/
static bool csr_is_conn_allow_2g_band(struct mac_context *mac_ctx,
				      uint32_t ch_freq)
{
	uint32_t sap_session_id;
	struct csr_roam_session *sap_session;

	if (0 == ch_freq) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("channel is zero, connection not allowed"));

		return false;
	}

	sap_session_id = csr_find_session_by_type(mac_ctx, QDF_SAP_MODE);
	if (WLAN_UMAC_VDEV_ID_MAX != sap_session_id) {
		sap_session = CSR_GET_SESSION(mac_ctx, sap_session_id);
		if (0 != sap_session->bssParams.operation_chan_freq &&
		    sap_session->bssParams.operation_chan_freq != ch_freq) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"Can't allow STA to connect, chnls not same");
			return false;
		}
	}
	return true;
}

/**
 * csr_is_conn_allow_5g_band() - This function will check if station's conn
 * is allowed in 5Ghz band.
 * @mac_ctx: pointer to mac context.
 * @chnl: station's channel.
 *
 * This function will check if station's connection is allowed in 5Ghz band
 * after comparing it with P2PGO's operating channel. If P2PGO's operating
 * channel and Station's channel is different than this function will return
 * false else true.
 *
 * Return: true or false.
 **/
static bool csr_is_conn_allow_5g_band(struct mac_context *mac_ctx,
				      uint32_t ch_freq)
{
	uint32_t p2pgo_session_id;
	struct csr_roam_session *p2pgo_session;

	if (0 == ch_freq) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("channel is zero, connection not allowed"));
		return false;
	}

	p2pgo_session_id = csr_find_session_by_type(mac_ctx, QDF_P2P_GO_MODE);
	if (WLAN_UMAC_VDEV_ID_MAX != p2pgo_session_id) {
		p2pgo_session = CSR_GET_SESSION(mac_ctx, p2pgo_session_id);
		if (0 != p2pgo_session->bssParams.operation_chan_freq &&
		    eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED !=
		    p2pgo_session->connectState &&
		    p2pgo_session->bssParams.operation_chan_freq != ch_freq) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"Can't allow STA to connect, chnls not same");
			return false;
		}
	}
	return true;
}
#endif

/**
 * csr_process_set_hw_mode() - Set HW mode command to PE
 * @mac: Globacl MAC pointer
 * @command: Command received from SME
 *
 * Posts the set HW mode command to PE. This message passing
 * through PE is required for PE's internal management
 *
 * Return: None
 */
void csr_process_set_hw_mode(struct mac_context *mac, tSmeCmd *command)
{
	uint32_t len;
	struct s_sir_set_hw_mode *cmd = NULL;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct sir_set_hw_mode_resp *param;
	enum policy_mgr_hw_mode_change hw_mode;
	enum policy_mgr_conc_next_action action;
	enum set_hw_mode_status hw_mode_change_status =
						SET_HW_MODE_STATUS_ECANCELED;

	/* Setting HW mode is for the entire system.
	 * So, no need to check session
	 */

	if (!command) {
		sme_err("Set HW mode param is NULL");
		goto fail;
	}

	len = sizeof(*cmd);
	cmd = qdf_mem_malloc(len);
	if (!cmd)
		/* Probably the fail response will also fail during malloc.
		 * Still proceeding to send response!
		 */
		goto fail;

	action = command->u.set_hw_mode_cmd.action;

#ifndef FEATURE_CM_ENABLE
	/* For hidden SSID case, if there is any scan command pending
	 * it needs to be cleared before issuing set HW mode
	 */
	if (command->u.set_hw_mode_cmd.reason ==
	    POLICY_MGR_UPDATE_REASON_HIDDEN_STA) {
		sme_err("clear any pending scan command");
		status = csr_scan_abort_mac_scan(mac,
			command->u.set_hw_mode_cmd.session_id, INVAL_SCAN_ID);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("Failed to clear scan cmd");
			goto fail;
		}
	}
#endif
	status = policy_mgr_validate_dbs_switch(mac->psoc, action);

	if (QDF_IS_STATUS_ERROR(status)) {
		sme_debug("Hw mode change not sent to FW status = %d", status);
		if (status == QDF_STATUS_E_ALREADY)
			hw_mode_change_status = SET_HW_MODE_STATUS_ALREADY;
		goto fail;
	}

	hw_mode = policy_mgr_get_hw_mode_change_from_hw_mode_index(
			mac->psoc, command->u.set_hw_mode_cmd.hw_mode_index);

	if (POLICY_MGR_HW_MODE_NOT_IN_PROGRESS == hw_mode) {
		sme_err("hw_mode %d, failing", hw_mode);
		goto fail;
	}

	policy_mgr_set_hw_mode_change_in_progress(mac->psoc, hw_mode);

	if ((POLICY_MGR_UPDATE_REASON_OPPORTUNISTIC ==
	     command->u.set_hw_mode_cmd.reason) &&
	    (true == mac->sme.get_connection_info_cb(NULL, NULL))) {
		sme_err("Set HW mode refused: conn in progress");
		policy_mgr_restart_opportunistic_timer(mac->psoc, false);
		goto reset_state;
	}

	if ((POLICY_MGR_UPDATE_REASON_OPPORTUNISTIC ==
	     command->u.set_hw_mode_cmd.reason) &&
	    (!command->u.set_hw_mode_cmd.hw_mode_index &&
	     !policy_mgr_need_opportunistic_upgrade(mac->psoc, NULL))) {
		sme_err("Set HW mode to SMM not needed anymore");
		goto reset_state;
	}

	cmd->messageType = eWNI_SME_SET_HW_MODE_REQ;
	cmd->length = len;
	cmd->set_hw.hw_mode_index = command->u.set_hw_mode_cmd.hw_mode_index;
	cmd->set_hw.reason = command->u.set_hw_mode_cmd.reason;
	/*
	 * Below callback and context info are not needed for PE as of now.
	 * Storing the passed value in the same s_sir_set_hw_mode format.
	 */
	cmd->set_hw.set_hw_mode_cb = command->u.set_hw_mode_cmd.set_hw_mode_cb;

	sme_debug(
		"Posting set hw mode req to PE session:%d reason:%d",
		command->u.set_hw_mode_cmd.session_id,
		command->u.set_hw_mode_cmd.reason);

	status = umac_send_mb_message_to_mac(cmd);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("Posting to PE failed");
		cmd = NULL;
		goto reset_state;
	}
	return;

reset_state:
	policy_mgr_set_hw_mode_change_in_progress(mac->psoc,
			POLICY_MGR_HW_MODE_NOT_IN_PROGRESS);
fail:
	if (cmd)
		qdf_mem_free(cmd);
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return;

	sme_debug("Sending set HW fail response to SME");
	param->status = hw_mode_change_status;
	param->cfgd_hw_mode_index = 0;
	param->num_vdev_mac_entries = 0;
	msg.type = eWNI_SME_SET_HW_MODE_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	sys_process_mmh_msg(mac, &msg);
}

/**
 * csr_process_set_dual_mac_config() - Set HW mode command to PE
 * @mac: Global MAC pointer
 * @command: Command received from SME
 *
 * Posts the set dual mac config command to PE.
 *
 * Return: None
 */
void csr_process_set_dual_mac_config(struct mac_context *mac, tSmeCmd *command)
{
	uint32_t len;
	struct sir_set_dual_mac_cfg *cmd;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct sir_dual_mac_config_resp *param;

	/* Setting MAC configuration is for the entire system.
	 * So, no need to check session
	 */

	if (!command) {
		sme_err("Set HW mode param is NULL");
		goto fail;
	}

	len = sizeof(*cmd);
	cmd = qdf_mem_malloc(len);
	if (!cmd)
		/* Probably the fail response will also fail during malloc.
		 * Still proceeding to send response!
		 */
		goto fail;

	cmd->message_type = eWNI_SME_SET_DUAL_MAC_CFG_REQ;
	cmd->length = len;
	cmd->set_dual_mac.scan_config = command->u.set_dual_mac_cmd.scan_config;
	cmd->set_dual_mac.fw_mode_config =
		command->u.set_dual_mac_cmd.fw_mode_config;
	/*
	 * Below callback and context info are not needed for PE as of now.
	 * Storing the passed value in the same sir_set_dual_mac_cfg format.
	 */
	cmd->set_dual_mac.set_dual_mac_cb =
		command->u.set_dual_mac_cmd.set_dual_mac_cb;

	sme_debug("Posting eWNI_SME_SET_DUAL_MAC_CFG_REQ to PE: %x %x",
		  cmd->set_dual_mac.scan_config,
		  cmd->set_dual_mac.fw_mode_config);

	status = umac_send_mb_message_to_mac(cmd);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Posting to PE failed");
		goto fail;
	}
	return;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return;

	sme_err("Sending set dual mac fail response to SME");
	param->status = SET_HW_MODE_STATUS_ECANCELED;
	msg.type = eWNI_SME_SET_DUAL_MAC_CFG_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	sys_process_mmh_msg(mac, &msg);
}

/**
 * csr_process_set_antenna_mode() - Set antenna mode command to
 * PE
 * @mac: Global MAC pointer
 * @command: Command received from SME
 *
 * Posts the set dual mac config command to PE.
 *
 * Return: None
 */
void csr_process_set_antenna_mode(struct mac_context *mac, tSmeCmd *command)
{
	uint32_t len;
	struct sir_set_antenna_mode *cmd;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct sir_antenna_mode_resp *param;

	/* Setting MAC configuration is for the entire system.
	 * So, no need to check session
	 */

	if (!command) {
		sme_err("Set antenna mode param is NULL");
		goto fail;
	}

	len = sizeof(*cmd);
	cmd = qdf_mem_malloc(len);
	if (!cmd)
		goto fail;

	cmd->message_type = eWNI_SME_SET_ANTENNA_MODE_REQ;
	cmd->length = len;
	cmd->set_antenna_mode = command->u.set_antenna_mode_cmd;

	sme_debug(
		"Posting eWNI_SME_SET_ANTENNA_MODE_REQ to PE: %d %d",
		cmd->set_antenna_mode.num_rx_chains,
		cmd->set_antenna_mode.num_tx_chains);

	status = umac_send_mb_message_to_mac(cmd);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("Posting to PE failed");
		/*
		 * umac_send_mb_message_to_mac would've released the mem
		 * allocated by cmd.
		 */
		goto fail;
	}

	return;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return;

	sme_err("Sending set dual mac fail response to SME");
	param->status = SET_ANTENNA_MODE_STATUS_ECANCELED;
	msg.type = eWNI_SME_SET_ANTENNA_MODE_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	sys_process_mmh_msg(mac, &msg);
}

/**
 * csr_process_nss_update_req() - Update nss command to PE
 * @mac: Globacl MAC pointer
 * @command: Command received from SME
 *
 * Posts the nss update command to PE. This message passing
 * through PE is required for PE's internal management
 *
 * Return: None
 */
void csr_process_nss_update_req(struct mac_context *mac, tSmeCmd *command)
{
	uint32_t len;
	struct sir_nss_update_request *msg;
	QDF_STATUS status;
	struct scheduler_msg msg_return = {0};
	struct sir_bcn_update_rsp *param;
	struct csr_roam_session *session;


	if (!CSR_IS_SESSION_VALID(mac, command->vdev_id)) {
		sme_err("Invalid session id %d", command->vdev_id);
		goto fail;
	}
	session = CSR_GET_SESSION(mac, command->vdev_id);

	len = sizeof(*msg);
	msg = qdf_mem_malloc(len);
	if (!msg)
		/* Probably the fail response is also fail during malloc.
		 * Still proceeding to send response!
		 */
		goto fail;

	msg->msgType = eWNI_SME_NSS_UPDATE_REQ;
	msg->msgLen = sizeof(*msg);

	msg->new_nss = command->u.nss_update_cmd.new_nss;
	msg->ch_width = command->u.nss_update_cmd.ch_width;
	msg->vdev_id = command->u.nss_update_cmd.session_id;

	sme_debug("Posting eWNI_SME_NSS_UPDATE_REQ to PE");

	status = umac_send_mb_message_to_mac(msg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return;

	sme_err("Posting to PE failed");
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return;

	sme_err("Sending nss update fail response to SME");
	param->status = QDF_STATUS_E_FAILURE;
	param->vdev_id = command->u.nss_update_cmd.session_id;
	param->reason = REASON_NSS_UPDATE;
	msg_return.type = eWNI_SME_NSS_UPDATE_RSP;
	msg_return.bodyptr = param;
	msg_return.bodyval = 0;
	sys_process_mmh_msg(mac, &msg_return);
}

#ifndef FEATURE_CM_ENABLE
#ifdef FEATURE_WLAN_TDLS
/**
 * csr_roam_fill_tdls_info() - Fill TDLS information
 * @roam_info: Roaming information buffer
 * @join_rsp: Join response which has TDLS info
 *
 * Return: None
 */
void csr_roam_fill_tdls_info(struct mac_context *mac_ctx,
			     struct csr_roam_info *roam_info,
			     struct wlan_objmgr_vdev *vdev)
{
	roam_info->tdls_prohibited = mlme_get_tdls_prohibited(vdev);
	roam_info->tdls_chan_swit_prohibited =
		mlme_get_tdls_chan_switch_prohibited(vdev);
	sme_debug(
		"tdls:prohibit: %d, chan_swit_prohibit: %d",
		roam_info->tdls_prohibited,
		roam_info->tdls_chan_swit_prohibited);
}
#endif

#if defined(WLAN_FEATURE_FILS_SK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
static void csr_copy_fils_join_rsp_roam_info(struct csr_roam_info *roam_info,
				      struct roam_offload_synch_ind *roam_synch_data)
{
	struct fils_join_rsp_params *roam_fils_info;

	roam_info->fils_join_rsp = qdf_mem_malloc(sizeof(*roam_fils_info));
	if (!roam_info->fils_join_rsp)
		return;

	roam_fils_info = roam_info->fils_join_rsp;
	cds_copy_hlp_info(&roam_synch_data->dst_mac,
			&roam_synch_data->src_mac,
			roam_synch_data->hlp_data_len,
			roam_synch_data->hlp_data,
			&roam_fils_info->dst_mac,
			&roam_fils_info->src_mac,
			&roam_fils_info->hlp_data_len,
			roam_fils_info->hlp_data);
}

/*
 * csr_update_fils_erp_seq_num() - Update the FILS erp sequence number in
 * roaming profile after roam complete
 * @roam_info: roam_info pointer
 * @erp_next_seq_num: next erp sequence number from firmware
 *
 * Return: NONE
 */
static
void csr_update_fils_erp_seq_num(struct csr_roam_profile *roam_profile,
				 uint16_t erp_next_seq_num)
{
	if (roam_profile->fils_con_info)
		roam_profile->fils_con_info->erp_sequence_number =
				erp_next_seq_num;
}
#else
static inline
void csr_copy_fils_join_rsp_roam_info(struct csr_roam_info *roam_info,
				      struct roam_offload_synch_ind *roam_synch_data)
{}

static inline
void csr_update_fils_erp_seq_num(struct csr_roam_profile *roam_profile,
				 uint16_t erp_next_seq_num)
{}
#endif
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#ifndef FEATURE_CM_ENABLE
QDF_STATUS csr_fast_reassoc(mac_handle_t mac_handle,
			    struct csr_roam_profile *profile,
			    const tSirMacAddr bssid, uint32_t ch_freq,
			    uint8_t vdev_id, const tSirMacAddr connected_bssid)
{
	QDF_STATUS status;
	struct roam_invoke_req *fastreassoc;
	struct scheduler_msg msg = {0};
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_roam_after_data_stall *vdev_roam_params;
	bool roam_control_bitmap;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		sme_err("session %d not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (!csr_is_conn_state_connected(mac_ctx, vdev_id)) {
		sme_debug("Not in connected state, Roam Invoke not sent");
		return QDF_STATUS_E_FAILURE;
	}

	roam_control_bitmap = mlme_get_operations_bitmap(mac_ctx->psoc,
							 vdev_id);
	if (roam_control_bitmap ||
	    !MLME_IS_ROAM_INITIALIZED(mac_ctx->psoc, vdev_id)) {
		sme_debug("ROAM: RSO Disabled internaly: vdev[%d] bitmap[0x%x]",
			  vdev_id, roam_control_bitmap);
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);

	if (!vdev) {
		sme_err("vdev is NULL, aborting roam invoke");
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev_roam_params = mlme_get_roam_invoke_params(vdev);

	if (!vdev_roam_params) {
		sme_err("Invalid vdev roam params, aborting roam invoke");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (vdev_roam_params->roam_invoke_in_progress) {
		sme_debug("Roaming already initiated by %d source",
			  vdev_roam_params->source);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return QDF_STATUS_E_FAILURE;
	}

	fastreassoc = qdf_mem_malloc(sizeof(*fastreassoc));
	if (!fastreassoc) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return QDF_STATUS_E_NOMEM;
	}
	/* if both are same then set the flag */
	if (!qdf_mem_cmp(connected_bssid, bssid, ETH_ALEN))
		fastreassoc->is_same_bssid = true;

	fastreassoc->vdev_id = vdev_id;
	fastreassoc->target_bssid.bytes[0] = bssid[0];
	fastreassoc->target_bssid.bytes[1] = bssid[1];
	fastreassoc->target_bssid.bytes[2] = bssid[2];
	fastreassoc->target_bssid.bytes[3] = bssid[3];
	fastreassoc->target_bssid.bytes[4] = bssid[4];
	fastreassoc->target_bssid.bytes[5] = bssid[5];

	status = sme_get_beacon_frm(mac_handle, profile, bssid,
				    &fastreassoc->frame_buf,
				    &fastreassoc->frame_len,
				    &ch_freq, vdev_id);

	if (!ch_freq) {
		sme_err("channel retrieval from BSS desc fails!");
		qdf_mem_free(fastreassoc->frame_buf);
		fastreassoc->frame_buf = NULL;
		fastreassoc->frame_len = 0;
		qdf_mem_free(fastreassoc);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return QDF_STATUS_E_FAULT;
	}

	fastreassoc->ch_freq = ch_freq;
	if (QDF_STATUS_SUCCESS != status) {
		sme_warn("sme_get_beacon_frm failed");
		qdf_mem_free(fastreassoc->frame_buf);
		fastreassoc->frame_buf = NULL;
		fastreassoc->frame_len = 0;
	}

	if (csr_is_auth_type_ese(mac_ctx->roam.roamSession[vdev_id].
				connectedProfile.AuthType)) {
		sme_debug("Beacon is not required for ESE");
		if (fastreassoc->frame_len) {
			qdf_mem_free(fastreassoc->frame_buf);
			fastreassoc->frame_buf = NULL;
			fastreassoc->frame_len = 0;
		}
	}

	msg.type = eWNI_SME_ROAM_INVOKE;
	msg.reserved = 0;
	msg.bodyptr = fastreassoc;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Not able to post ROAM_INVOKE_CMD message to PE");
		qdf_mem_free(fastreassoc->frame_buf);
		fastreassoc->frame_buf = NULL;
		fastreassoc->frame_len = 0;
		qdf_mem_free(fastreassoc);
	} else {
		vdev_roam_params->roam_invoke_in_progress = true;
		vdev_roam_params->source = USERSPACE_INITIATED;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	return status;
}

static QDF_STATUS
csr_process_roam_sync_callback(struct mac_context *mac_ctx,
			       struct roam_offload_synch_ind *roam_synch_data,
			       struct bss_description *bss_desc,
			       enum sir_roam_op_code reason)
{
	uint8_t session_id = roam_synch_data->roamed_vdev_id;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	tDot11fBeaconIEs *ies_local = NULL;
	struct csr_roam_info *roam_info;
	tCsrRoamConnectedProfile *conn_profile = NULL;
	sme_QosAssocInfo assoc_info;
	struct bss_params *add_bss_params;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint16_t len;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_roam_after_data_stall *vdev_roam_params;
	bool abort_host_scan_cap = false;
	wlan_scan_id scan_id;
	uint8_t ssid_offset;
	uint8_t mdie_present;
	struct cm_roam_values_copy config;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct mlme_legacy_priv *mlme_priv;
	struct qdf_mac_addr connected_bssid;

	mlme_obj = mlme_get_psoc_ext_obj(mac_ctx->psoc);
	if (!mlme_obj) {
		sme_err("mlme_obj is NULL, aborting roam invoke");
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, session_id,
						    WLAN_LEGACY_SME_ID);

	if (!vdev) {
		sme_err("vdev is NULL, aborting roam invoke");
		return QDF_STATUS_E_NULL_VALUE;
	}
	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		status = QDF_STATUS_E_NULL_VALUE;
		goto end;
	}

	vdev_roam_params = mlme_get_roam_invoke_params(vdev);

	if (!vdev_roam_params) {
		sme_err("Invalid vdev roam params, aborting roam invoke");
		status = QDF_STATUS_E_NULL_VALUE;
		goto end;
	}

	if (!session) {
		sme_err("LFR3: Session not found");
		status = QDF_STATUS_E_NULL_VALUE;
		goto end;
	}

	sme_debug("LFR3: reason: %d roam invoke in progress %d, source %d",
		  reason, vdev_roam_params->roam_invoke_in_progress,
		  vdev_roam_params->source);

	switch (reason) {
	case SIR_ROAMING_DEREGISTER_STA:
		/*
		 * Get old bssid as, new AP is not updated yet and do cleanup
		 * for old bssid.
		 */
		wlan_mlme_get_bssid_vdev_id(mac_ctx->pdev, session_id,
					    &connected_bssid);

		/* Update the BLM that the previous profile has disconnected */
		wlan_blm_update_bssid_connect_params(mac_ctx->pdev,
						     connected_bssid,
						     BLM_AP_DISCONNECTED);
		if (IS_ROAM_REASON_STA_KICKOUT(roam_synch_data->roam_reason)) {
			struct reject_ap_info ap_info;

			ap_info.bssid = connected_bssid;
			ap_info.reject_ap_type = DRIVER_AVOID_TYPE;
			ap_info.reject_reason = REASON_STA_KICKOUT;
			ap_info.source = ADDED_BY_DRIVER;
			wlan_blm_add_bssid_to_reject_list(mac_ctx->pdev, &ap_info);
		}
		/* use API similar to cm_update_scan_mlme_on_disconnect for CM */
		csr_update_scan_entry_associnfo(mac_ctx, session_id,
						SCAN_ENTRY_CON_STATE_NONE);
		/*
		 * The following is the first thing done in CSR
		 * after receiving RSI. Hence stopping the timer here.
		 */
		csr_roam_roaming_offload_timer_action(mac_ctx,
				0, session_id, ROAMING_OFFLOAD_TIMER_STOP);

		if (session->discon_in_progress) {
			sme_err("LFR3: vdev:%d Disconnect is in progress roam_synch is not allowed",
				session_id);
			status = QDF_STATUS_E_FAILURE;
			goto end;
		}
		status = wlan_cm_roam_state_change(mac_ctx->pdev, session_id,
						    WLAN_ROAM_SYNCH_IN_PROG,
						    REASON_ROAM_HANDOFF_DONE);
		if (QDF_IS_STATUS_ERROR(status))
			goto end;

		mlme_init_twt_context(mac_ctx->psoc, &connected_bssid,
				      TWT_ALL_SESSIONS_DIALOG_ID);
		csr_roam_call_callback(mac_ctx, session_id, NULL, 0,
				eCSR_ROAM_FT_START, eCSR_ROAM_RESULT_SUCCESS);
		goto end;
	case SIR_ROAMING_START:
		status = wlan_cm_roam_state_change(mac_ctx->pdev, session_id,
					WLAN_ROAMING_IN_PROG,
					REASON_ROAM_CANDIDATE_FOUND);
		if (QDF_IS_STATUS_ERROR(status))
			goto end;

		csr_roam_roaming_offload_timer_action(mac_ctx,
				CSR_ROAMING_OFFLOAD_TIMEOUT_PERIOD, session_id,
				ROAMING_OFFLOAD_TIMER_START);
		csr_roam_call_callback(mac_ctx, session_id, NULL, 0,
				eCSR_ROAM_START, eCSR_ROAM_RESULT_SUCCESS);
		/*
		 * For emergency deauth roaming, firmware sends ROAM start
		 * instead of ROAM scan start notification as data path queues
		 * will be stopped only during roam start notification.
		 * This is because, for deauth/disassoc triggered roam, the
		 * AP has sent deauth, and packets shouldn't be sent to AP
		 * after that. Since firmware is sending roam start directly
		 * host sends scan abort during roam scan, but in other
		 * triggers, the host receives roam start after candidate
		 * selection and roam scan is complete. So when host sends
		 * roam abort for emergency deauth roam trigger, the firmware
		 * roam scan is also aborted. This results in roaming failure.
		 * So send scan_id as CANCEL_HOST_SCAN_ID to scan module to
		 * abort only host triggered scans.
		 */
		abort_host_scan_cap =
			wlan_mlme_get_host_scan_abort_support(mac_ctx->psoc);
		if (abort_host_scan_cap)
			scan_id = CANCEL_HOST_SCAN_ID;
		else
			scan_id = INVAL_SCAN_ID;

		wlan_abort_scan(mac_ctx->pdev, INVAL_PDEV_ID,
				session_id, scan_id, false);
		goto end;
	case SIR_ROAMING_ABORT:
		/*
		 * Roaming abort is received after roaming is started
		 * in firmware(after candidate selection) but re-assoc to
		 * the candidate was not successful.
		 * Connection to the previous AP is still valid in this
		 * case. So move to RSO_ENABLED state.
		 */
		wlan_cm_roam_state_change(mac_ctx->pdev, session_id,
					   WLAN_ROAM_RSO_ENABLED,
					   REASON_ROAM_ABORT);
		csr_roam_roaming_offload_timer_action(mac_ctx,
				0, session_id, ROAMING_OFFLOAD_TIMER_STOP);
		csr_roam_call_callback(mac_ctx, session_id, NULL, 0,
				eCSR_ROAM_ABORT, eCSR_ROAM_RESULT_SUCCESS);
		vdev_roam_params->roam_invoke_in_progress = false;
		goto end;
	case SIR_ROAM_SYNCH_NAPI_OFF:
		csr_roam_call_callback(mac_ctx, session_id, NULL, 0,
				eCSR_ROAM_NAPI_OFF, eCSR_ROAM_RESULT_SUCCESS);
		goto end;
	case SIR_ROAMING_INVOKE_FAIL:
		sme_debug("Roaming triggered failed source %d nud behaviour %d",
			  vdev_roam_params->source, mac_ctx->nud_fail_behaviour);

		/* Userspace roam req fail, disconnect with AP */
		if (vdev_roam_params->source == USERSPACE_INITIATED ||
		    mac_ctx->nud_fail_behaviour == DISCONNECT_AFTER_ROAM_FAIL) {
			csr_roam_disconnect(mac_ctx, session_id,
				    eCSR_DISCONNECT_REASON_DEAUTH,
				    REASON_USER_TRIGGERED_ROAM_FAILURE);
		}

		vdev_roam_params->roam_invoke_in_progress = false;
		goto end;
	case SIR_ROAM_SYNCH_PROPAGATION:
		wlan_rec_conn_info(session_id, DEBUG_CONN_ROAMING,
				   bss_desc->bssId,
				   reason, session->connectState);
		break;
	case SIR_ROAM_SYNCH_COMPLETE:
		/* Handle one race condition that if candidate is already
		 *selected & FW has gone ahead with roaming or about to go
		 * ahead when set_band comes, it will be complicated for FW
		 * to stop the current roaming. Instead, host will check the
		 * roam sync to make sure the new AP is on 2G, or disconnect
		 * the AP.
		 */
		if (wlan_reg_is_disable_for_freq(mac_ctx->pdev,
						 roam_synch_data->chan_freq)) {
			csr_roam_disconnect(mac_ctx, session_id,
					    eCSR_DISCONNECT_REASON_DEAUTH,
					    REASON_OPER_CHANNEL_BAND_CHANGE);
			sme_debug("Roaming Failed for disabled channel or band");
			vdev_roam_params->roam_invoke_in_progress = false;
			goto end;
		}
		/*
		 * Following operations need to be done once roam sync
		 * completion is sent to FW, hence called here:
		 * 1) Firmware has already updated DBS policy. Update connection
		 *    table in the host driver.
		 * 2) Force SCC switch if needed
		 * 3) Set connection in progress = false
		 */
		/* first update connection info from wma interface */
		policy_mgr_update_connection_info(mac_ctx->psoc, session_id);
		/* then update remaining parameters from roam sync ctx */
		sme_debug("Update DBS hw mode");
		policy_mgr_hw_mode_transition_cb(
			roam_synch_data->hw_mode_trans_ind.old_hw_mode_index,
			roam_synch_data->hw_mode_trans_ind.new_hw_mode_index,
			roam_synch_data->hw_mode_trans_ind.num_vdev_mac_entries,
			roam_synch_data->hw_mode_trans_ind.vdev_mac_map,
			mac_ctx->psoc);
		mac_ctx->sme.set_connection_info_cb(false);

		cm_check_and_set_sae_single_pmk_cap(mac_ctx->psoc, session_id);

		if (ucfg_pkt_capture_get_pktcap_mode(mac_ctx->psoc))
			ucfg_pkt_capture_record_channel(vdev);

		if (WLAN_REG_IS_5GHZ_CH_FREQ(bss_desc->chan_freq)) {
			wlan_cm_set_disable_hi_rssi(mac_ctx->pdev,
						    session_id, true);
			sme_debug("Disabling HI_RSSI, AP freq=%d, rssi=%d",
				  bss_desc->chan_freq, bss_desc->rssi);
		} else {
			wlan_cm_set_disable_hi_rssi(mac_ctx->pdev,
						    session_id, false);
		}

		policy_mgr_check_n_start_opportunistic_timer(mac_ctx->psoc);
		policy_mgr_check_concurrent_intf_and_restart_sap(mac_ctx->psoc);
		vdev_roam_params->roam_invoke_in_progress = false;

		if (roam_synch_data->auth_status ==
		    ROAM_AUTH_STATUS_AUTHENTICATED) {
			wlan_cm_roam_state_change(mac_ctx->pdev, session_id,
						   WLAN_ROAM_RSO_ENABLED,
						   REASON_CONNECT);
		} else {
			/*
			 * STA is just in associated state here, RSO
			 * enable will be sent once EAP & EAPOL will be done by
			 * user-space and after set key response
			 * is received.
			 */
			wlan_cm_roam_state_change(mac_ctx->pdev, session_id,
						  WLAN_ROAM_INIT,
						  REASON_CONNECT);
		}
		goto end;
	case SIR_ROAMING_DEAUTH:
		csr_roam_roaming_offload_timer_action(
			mac_ctx, 0, session_id, ROAMING_OFFLOAD_TIMER_STOP);
		goto end;
	default:
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}
	session->roam_synch_data = roam_synch_data;
	status = csr_get_parsed_bss_description_ies(
			mac_ctx, bss_desc, &ies_local);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("LFR3: fail to parse IEs");
		goto end;
	}

	conn_profile = &session->connectedProfile;
	status = csr_roam_stop_network(mac_ctx, session_id,
				       session->pCurRoamProfile,
				       bss_desc, ies_local);
	if (!QDF_IS_STATUS_SUCCESS(status))
		goto end;

	session->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED;

	/* use cm_inform_bcn_probe for connection manager */
	csr_rso_save_ap_to_scan_cache(mac_ctx, roam_synch_data, bss_desc);
	/* For CM fill struct cm_vdev_join_rsp and struct wlan_cm_connect_resp
	 * instead of roam info */
	roam_info = qdf_mem_malloc(sizeof(struct csr_roam_info));
	if (!roam_info) {
		qdf_mem_free(ies_local);
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}
	roam_info->sessionId = session_id;

	qdf_mem_copy(&roam_info->bssid.bytes, &bss_desc->bssId,
			sizeof(struct qdf_mac_addr));
	/* for CM hande same way as cm_csr_connect_rsp and
	 * cm_csr_connect_done_ind */
	csr_roam_save_connected_information(mac_ctx, session_id,
			session->pCurRoamProfile,
			bss_desc,
			ies_local);
	/* For CM handle similar to cm_connect_complete to update */
	csr_update_scan_entry_associnfo(mac_ctx, session_id,
					SCAN_ENTRY_CON_STATE_ASSOC);

#ifdef FEATURE_WLAN_ESE
	roam_info->isESEAssoc = wlan_cm_get_ese_assoc(mac_ctx->pdev,
						      session_id);
#endif

	/*
	 * Encryption keys for new connection are obtained as follows:
	 * auth_status = CSR_ROAM_AUTH_STATUS_AUTHENTICATED
	 * Open - No keys required.
	 * Static WEP - Firmware copies keys from old AP to new AP.
	 * Fast roaming authentications e.g. PSK, FT, CCKM - firmware
	 *      supplicant obtains them through 4-way handshake.
	 *
	 * auth_status = CSR_ROAM_AUTH_STATUS_CONNECTED
	 * All other authentications - Host supplicant performs EAPOL
	 *      with AP after this point and sends new keys to the driver.
	 *      Driver starts wait_for_key timer for that purpose.
	 * Allow csr_lookup_pmkid_using_bssid() if akm is SAE/OWE since
	 * SAE/OWE roaming uses hybrid model and eapol is offloaded to
	 * supplicant unlike in WPA2 – 802.1x case, after 8 way handshake
	 * the __wlan_hdd_cfg80211_keymgmt_set_key ->sme_roam_set_psk_pmk()
	 * will get called after roam synch complete to update the
	 * session->psk_pmk, but in SAE/OWE roaming this sequence is not
	 * present and set_pmksa will come before roam synch indication &
	 * eapol. So the session->psk_pmk will be stale in PMKSA cached
	 * SAE/OWE roaming case.
	 */

	/* for CM handle this similar to cm_connect_complete_ind */
	if (roam_synch_data->auth_status == ROAM_AUTH_STATUS_AUTHENTICATED ||
	    session->pCurRoamProfile->negotiatedAuthType ==
	    eCSR_AUTH_TYPE_SAE ||
	    session->pCurRoamProfile->negotiatedAuthType ==
	    eCSR_AUTH_TYPE_OWE) {
		struct wlan_crypto_pmksa *pmkid_cache, *pmksa;

		cm_csr_set_ss_none(session_id);
		/*
		 * If auth_status is AUTHENTICATED, then we have done successful
		 * 4 way handshake in FW using the cached PMKID.
		 * However, the session->psk_pmk has the PMK of the older AP
		 * as set_key is not received from supplicant.
		 * When any RSO command is sent for the current AP, the older
		 * AP's PMK is sent to the FW which leads to incorrect PMK and
		 * leads to 4 way handshake failure when roaming happens to
		 * this AP again.
		 * Check if a PMK cache exists for the roamed AP and update
		 * it into the session pmk.
		 */
		pmkid_cache = qdf_mem_malloc(sizeof(*pmkid_cache));
		if (!pmkid_cache) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		wlan_vdev_get_bss_peer_mac(vdev, &pmkid_cache->bssid);
		sme_debug("Trying to find PMKID for " QDF_MAC_ADDR_FMT " AKM Type:%d",
			  QDF_MAC_ADDR_REF(pmkid_cache->bssid.bytes),
			  session->pCurRoamProfile->negotiatedAuthType);
		wlan_cm_roam_cfg_get_value(mac_ctx->psoc, session_id,
					   MOBILITY_DOMAIN, &config);
		mdie_present = config.bool_value;

		if (cm_lookup_pmkid_using_bssid(mac_ctx->psoc,
						 session->vdev_id,
						 pmkid_cache)) {
			/*
			 * Consider two APs: AP1, AP2
			 * Both APs configured with EAP 802.1x security mode
			 * and OKC is enabled in both APs by default. Initially
			 * DUT successfully associated with AP1, and generated
			 * PMK1 by performing full EAP and added an entry for
			 * AP1 in pmk table. At this stage, pmk table has only
			 * one entry for PMK1 (1. AP1-->PMK1). Now DUT try to
			 * roam to AP2 using PMK1 (as OKC is enabled) but
			 * session timeout happens on AP2 just before 4 way
			 * handshake completion in FW. At this point of time
			 * DUT not in authenticated state. Due to this DUT
			 * performs full EAP with AP2 and generates PMK2. As
			 * there is no previous entry of AP2 (AP2-->PMK1) in pmk
			 * table. When host gets pmk delete command for BSSID of
			 * AP2, the BSSID match fails. Hence host will not
			 * delete pmk entry of AP1 as well.
			 * At this point of time, the PMK table has two entry
			 * 1. AP1-->PMK1 and 2. AP2 --> PMK2.
			 * Ideally, if OKC is enabled then whenever timeout
			 * occurs in a mobility domain, then the driver should
			 * clear all APs cache entries related to that domain
			 * but as the BSSID doesn't exist yet in the driver
			 * cache there is no way of clearing the cache entries,
			 * without disturbing the legacy roaming.
			 * Now security profile for both APs changed to FT-RSN.
			 * DUT first disassociate with AP2 and successfully
			 * associated with AP2 and perform full EAP and
			 * generates PMK3. DUT first deletes PMK entry for AP2
			 * and then adds a new entry for AP2.
			 * At this point of time pmk table has two entry
			 * AP2--> PMK3 and AP1-->PMK1. Now DUT roamed to AP1
			 * using PMK3 but sends stale entry of AP1 (PMK1) to
			 * fw via RSO command. This override PMK for both APs
			 * with PMK1 (as FW uses mlme session PMK for both APs
			 * in case of FT roaming) and next time when FW try to
			 * roam to AP2 using PMK1, AP2 rejects PMK1 (As AP2 is
			 * expecting PMK3) and initiates full EAP with AP2,
			 * which is wrong.
			 * To address this issue update pmk table entry for
			 * roamed AP1 with PMK3 value comes to host via roam
			 * sync indication event. By this host override stale
			 * entry (if any) with the latest valid pmk for that AP
			 * at a point of time.
			 */
			if (roam_synch_data->pmk_len) {
				pmksa = qdf_mem_malloc(sizeof(*pmksa));
				if (!pmksa) {
					status = QDF_STATUS_E_NOMEM;
					qdf_mem_zero(pmkid_cache,
						     sizeof(*pmkid_cache));
					qdf_mem_free(pmkid_cache);
					goto end;
				}

				/*
				 * This pmksa buffer is to update the
				 * crypto table
				 */
				wlan_vdev_get_bss_peer_mac(vdev, &pmksa->bssid);
				qdf_mem_copy(pmksa->pmkid,
					     roam_synch_data->pmkid, PMKID_LEN);
				qdf_mem_copy(pmksa->pmk, roam_synch_data->pmk,
					     roam_synch_data->pmk_len);
				pmksa->pmk_len = roam_synch_data->pmk_len;
				status = wlan_crypto_set_del_pmksa(vdev,
								   pmksa, true);
				if (QDF_IS_STATUS_ERROR(status)) {
					qdf_mem_zero(pmksa, sizeof(*pmksa));
					qdf_mem_free(pmksa);
					pmksa = NULL;
				}

				/* update the pmkid_cache buffer to
				 * update the global session pmk
				 */
				qdf_mem_copy(pmkid_cache->pmkid,
					     roam_synch_data->pmkid, PMKID_LEN);
				qdf_mem_copy(pmkid_cache->pmk,
					     roam_synch_data->pmk,
					     roam_synch_data->pmk_len);
				pmkid_cache->pmk_len = roam_synch_data->pmk_len;
			}
			wlan_cm_set_psk_pmk(mac_ctx->pdev, session_id,
					    pmkid_cache->pmk,
					    pmkid_cache->pmk_len);
			sme_debug("pmkid found for " QDF_MAC_ADDR_FMT " len %d",
				  QDF_MAC_ADDR_REF(pmkid_cache->bssid.bytes),
				  pmkid_cache->pmk_len);
		} else {
			sme_debug("PMKID Not found in cache for " QDF_MAC_ADDR_FMT,
				  QDF_MAC_ADDR_REF(pmkid_cache->bssid.bytes));
			/*
			 * In FT roam when the CSR lookup fails then the PMK
			 * details from the roam sync indication will be
			 * updated to Session/PMK cache. This will result in
			 * having multiple PMK cache entries for the same MDID,
			 * So do not add the PMKSA cache entry in all the
			 * FT-Roam cases.
			 */
			if (!cm_is_auth_type_11r(mlme_obj, vdev,
						 mdie_present) &&
			    roam_synch_data->pmk_len) {
				/*
				 * This pmksa buffer is to update the
				 * crypto table
				 */
				pmksa = qdf_mem_malloc(sizeof(*pmksa));
				if (!pmksa) {
					status = QDF_STATUS_E_NOMEM;
					qdf_mem_zero(pmkid_cache,
						     sizeof(*pmkid_cache));
					qdf_mem_free(pmkid_cache);
					goto end;
				}
				wlan_cm_set_psk_pmk(pdev, vdev_id,
						    roam_synch_data->pmk,
						    roam_synch_data->pmk_len);
				wlan_vdev_get_bss_peer_mac(vdev,
							   &pmksa->bssid);
				qdf_mem_copy(pmksa->pmkid,
					     roam_synch_data->pmkid, PMKID_LEN);
				qdf_mem_copy(pmksa->pmk,
					     roam_synch_data->pmk,
					     roam_synch_data->pmk_len);
				pmksa->pmk_len = roam_synch_data->pmk_len;

				status = wlan_crypto_set_del_pmksa(vdev,
								   pmksa,
								   true);
				if (QDF_IS_STATUS_ERROR(status)) {
					qdf_mem_zero(pmksa, sizeof(*pmksa));
					qdf_mem_free(pmksa);
				}
			}
		}
		qdf_mem_zero(pmkid_cache, sizeof(*pmkid_cache));
		qdf_mem_free(pmkid_cache);
	}

	if (roam_synch_data->auth_status != ROAM_AUTH_STATUS_AUTHENTICATED) {
		roam_info->fAuthRequired = true;
		cm_update_wait_for_key_timer(vdev, session_id,
					     WAIT_FOR_KEY_TIMEOUT_PERIOD);
		csr_neighbor_roam_state_transition(mac_ctx,
				eCSR_NEIGHBOR_ROAM_STATE_INIT, session_id);
	}

	if (roam_synch_data->is_ft_im_roam) {
		ssid_offset = SIR_MAC_ASSOC_REQ_SSID_OFFSET;
	} else {
		ssid_offset = SIR_MAC_REASSOC_REQ_SSID_OFFSET;
	}

	roam_info->nBeaconLength = 0;
	roam_info->nAssocReqLength = roam_synch_data->reassoc_req_length -
		SIR_MAC_HDR_LEN_3A - ssid_offset;
	roam_info->nAssocRspLength = roam_synch_data->reassocRespLength -
		SIR_MAC_HDR_LEN_3A;
	roam_info->pbFrames = qdf_mem_malloc(roam_info->nBeaconLength +
		roam_info->nAssocReqLength + roam_info->nAssocRspLength);
	if (!roam_info->pbFrames) {
		if (roam_info)
			qdf_mem_free(roam_info);
		qdf_mem_free(ies_local);
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}
	qdf_mem_copy(roam_info->pbFrames,
			(uint8_t *)roam_synch_data +
			roam_synch_data->reassoc_req_offset +
			SIR_MAC_HDR_LEN_3A + ssid_offset,
			roam_info->nAssocReqLength);

	qdf_mem_copy(roam_info->pbFrames + roam_info->nAssocReqLength,
			(uint8_t *)roam_synch_data +
			roam_synch_data->reassocRespOffset +
			SIR_MAC_HDR_LEN_3A,
			roam_info->nAssocRspLength);

	sme_debug("LFR3:Clear Connected info");
	csr_roam_free_connected_info(mac_ctx, &session->connectedInfo);
	len = roam_synch_data->ric_data_len;

#ifdef FEATURE_WLAN_ESE
	len += roam_synch_data->tspec_len;
	sme_debug("LFR3: tspecLen %d", roam_synch_data->tspec_len);
#endif

	sme_debug("LFR3: RIC length - %d", roam_synch_data->ric_data_len);
	if (len) {
		session->connectedInfo.pbFrames =
			qdf_mem_malloc(len);
		if (session->connectedInfo.pbFrames) {
			qdf_mem_copy(session->connectedInfo.pbFrames,
				     roam_synch_data->ric_tspec_data,
				     len);
			session->connectedInfo.nRICRspLength =
				roam_synch_data->ric_data_len;

#ifdef FEATURE_WLAN_ESE
			session->connectedInfo.nTspecIeLength =
				roam_synch_data->tspec_len;
#endif
		}
	}
	add_bss_params = (struct bss_params *)roam_synch_data->add_bss_params;
	roam_info->timingMeasCap = mlme_priv->connect_info.timing_meas_cap;
	roam_info->chan_info.nss = roam_synch_data->nss;
	roam_info->chan_info.rate_flags = roam_synch_data->max_rate_flags;
	roam_info->chan_info.ch_width = roam_synch_data->chan_width;
	csr_roam_fill_tdls_info(mac_ctx, roam_info, vdev);
	assoc_info.bss_desc = bss_desc;
	roam_info->status_code = eSIR_SME_SUCCESS;
	roam_info->reasonCode = eSIR_SME_SUCCESS;
	wlan_cm_roam_cfg_get_value(mac_ctx->psoc, session_id, UAPSD_MASK,
				   &config);
	assoc_info.uapsd_mask = config.uint_value;
	mac_ctx->roam.roamSession[session_id].connectState =
		eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	/* for CM move all this sme qos API to cm_csr * API as in
	 * cm_csr_connect_rsp */
	sme_qos_csr_event_ind(mac_ctx, session_id,
		SME_QOS_CSR_HANDOFF_ASSOC_REQ, NULL);
	sme_qos_csr_event_ind(mac_ctx, session_id,
		SME_QOS_CSR_REASSOC_REQ, NULL);
	sme_qos_csr_event_ind(mac_ctx, session_id,
		SME_QOS_CSR_HANDOFF_COMPLETE, NULL);

	mac_ctx->roam.roamSession[session_id].connectState =
		eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED;
	/* for CM move all this sme qos API to cm_csr * API as in
	 * cm_csr_connect_rsp */
	sme_qos_csr_event_ind(mac_ctx, session_id,
		SME_QOS_CSR_REASSOC_COMPLETE, &assoc_info);
	roam_info->bss_desc = bss_desc;

	conn_profile->acm_mask = sme_qos_get_acm_mask(mac_ctx,
			bss_desc, NULL);
	if (conn_profile->modifyProfileFields.uapsd_mask) {
		sme_debug(
				" uapsd_mask (0x%X) set, request UAPSD now",
				conn_profile->modifyProfileFields.uapsd_mask);
		sme_ps_start_uapsd(MAC_HANDLE(mac_ctx), session_id);
	}
	roam_info->u.pConnectedProfile = conn_profile;

	sme_debug("staId %d nss %d rate_flag %d",
		roam_info->staId,
		roam_info->chan_info.nss,
		roam_info->chan_info.rate_flags);

	roam_info->roamSynchInProgress = true;
	roam_info->synchAuthStatus = roam_synch_data->auth_status;
	roam_info->kck_len = roam_synch_data->kck_len;
	roam_info->kek_len = roam_synch_data->kek_len;
	roam_info->pmk_len = roam_synch_data->pmk_len;
	qdf_mem_copy(roam_info->kck, roam_synch_data->kck, roam_info->kck_len);
	qdf_mem_copy(roam_info->kek, roam_synch_data->kek, roam_info->kek_len);

	if (roam_synch_data->pmk_len)
		qdf_mem_copy(roam_info->pmk, roam_synch_data->pmk,
			     roam_synch_data->pmk_len);

	qdf_mem_copy(roam_info->pmkid, roam_synch_data->pmkid, PMKID_LEN);
	roam_info->update_erp_next_seq_num =
			roam_synch_data->update_erp_next_seq_num;
	roam_info->next_erp_seq_num = roam_synch_data->next_erp_seq_num;
	/* for cm enable copy to reassoc/connect resp */
	/* for CM fill fils info in struct wlan_cm_connect_resp */
	csr_update_fils_erp_seq_num(session->pCurRoamProfile,
				    roam_info->next_erp_seq_num);

	sme_debug("Update ERP Seq Num : %d, Next ERP Seq Num : %d",
			roam_info->update_erp_next_seq_num,
			roam_info->next_erp_seq_num);
	qdf_mem_copy(roam_info->replay_ctr, roam_synch_data->replay_ctr,
			REPLAY_CTR_LEN);
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		FL("LFR3: Copy KCK, KEK(len %d) and Replay Ctr"),
		roam_info->kek_len);
	/* bit-4 and bit-5 indicate the subnet status */
	roam_info->subnet_change_status =
		CM_GET_SUBNET_STATUS(roam_synch_data->roam_reason);

	/* fetch 4 LSB to get roam reason */
	roam_info->roam_reason = roam_synch_data->roam_reason &
				 ROAM_REASON_MASK;
	sme_debug("Update roam reason : %d", roam_info->roam_reason);
	/* for cm enable copy to reassoc/connect resp */
	/* for CM fill fils info in struct wlan_cm_connect_resp */
	csr_copy_fils_join_rsp_roam_info(roam_info, roam_synch_data);
	csr_roam_call_callback(mac_ctx, session_id, roam_info, 0,
		eCSR_ROAM_ASSOCIATION_COMPLETION, eCSR_ROAM_RESULT_ASSOCIATED);

	/* for CM move all this sme qos API to cm_csr * API as in
	 * cm_csr_connect_rsp */
	if (IS_ROAM_REASON_DISCONNECTION(roam_synch_data->roam_reason))
		sme_qos_csr_event_ind(mac_ctx, session_id,
				      SME_QOS_CSR_DISCONNECT_ROAM_COMPLETE,
				      NULL);
	/* for CM move hanlde all roam specific handling in new func */
	if (!CSR_IS_WAIT_FOR_KEY(mac_ctx, session_id)) {
		wlan_mlme_get_bssid_vdev_id(mac_ctx->pdev, session_id,
					    &connected_bssid);
		QDF_TRACE(QDF_MODULE_ID_SME,
				QDF_TRACE_LEVEL_DEBUG,
				FL
				("NO CSR_IS_WAIT_FOR_KEY -> csr_roam_link_up"));
		csr_roam_link_up(mac_ctx, connected_bssid);
	}

	sme_free_join_rsp_fils_params(roam_info);
	qdf_mem_free(roam_info->pbFrames);
	qdf_mem_free(roam_info);
	qdf_mem_free(ies_local);

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	return status;
}

/**
 * csr_roam_synch_callback() - SME level callback for roam synch propagation
 * @mac_ctx: MAC Context
 * @roam_synch_data: Roam synch data buffer pointer
 * @bss_desc: BSS descriptor pointer
 * @reason: Reason for calling the callback
 *
 * This callback is registered with WMA and used after roaming happens in
 * firmware and the call to this routine completes the roam synch
 * propagation at both CSR and HDD levels. The HDD level propagation
 * is achieved through the already defined callback for assoc completion
 * handler.
 *
 * Return: Success or Failure.
 */
QDF_STATUS
csr_roam_synch_callback(struct mac_context *mac_ctx,
			struct roam_offload_synch_ind *roam_synch_data,
			struct bss_description *bss_desc,
			enum sir_roam_op_code reason)
{
	QDF_STATUS status;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	status = csr_process_roam_sync_callback(mac_ctx, roam_synch_data,
					    bss_desc, reason);

	sme_release_global_lock(&mac_ctx->sme);

	return status;
}
#endif
#ifdef WLAN_FEATURE_SAE
/**
 * csr_process_roam_auth_sae_callback() - API to trigger the
 * WPA3 pre-auth event for candidate AP received from firmware.
 * @mac_ctx: Global mac context pointer
 * @vdev_id: vdev id
 * @roam_bssid: Candidate BSSID to roam
 *
 * This function calls the hdd_sme_roam_callback with reason
 * eCSR_ROAM_SAE_COMPUTE to trigger SAE auth to supplicant.
 */
static QDF_STATUS
csr_process_roam_auth_sae_callback(struct mac_context *mac_ctx,
				   uint8_t vdev_id,
				   struct qdf_mac_addr roam_bssid)
{
	struct csr_roam_info *roam_info;
	struct sir_sae_info sae_info;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, vdev_id);

	if (!session) {
		sme_err("WPA3 Preauth event with invalid session id:%d",
			vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_FAILURE;

	sae_info.msg_len = sizeof(sae_info);
	sae_info.vdev_id = vdev_id;
	wlan_mlme_get_ssid_vdev_id(mac_ctx->pdev, vdev_id,
				   sae_info.ssid.ssId,
				   &sae_info.ssid.length);
	qdf_mem_copy(sae_info.peer_mac_addr.bytes,
		     roam_bssid.bytes, QDF_MAC_ADDR_SIZE);

	roam_info->sae_info = &sae_info;

	csr_roam_call_callback(mac_ctx, vdev_id, roam_info, 0,
			       eCSR_ROAM_SAE_COMPUTE, eCSR_ROAM_RESULT_NONE);

	qdf_mem_free(roam_info);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
csr_process_roam_auth_sae_callback(struct mac_context *mac_ctx,
				   uint8_t vdev_id,
				   struct qdf_mac_addr roam_bssid)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

QDF_STATUS
csr_roam_auth_offload_callback(struct mac_context *mac_ctx,
			       uint8_t vdev_id, struct qdf_mac_addr bssid)
{
	QDF_STATUS status;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	status = csr_process_roam_auth_sae_callback(mac_ctx, vdev_id, bssid);

	sme_release_global_lock(&mac_ctx->sme);

	return status;

}
#endif

QDF_STATUS csr_update_owe_info(struct mac_context *mac,
			       struct assoc_ind *assoc_ind)
{
	uint32_t session_id = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;

	status = csr_roam_get_session_id_from_bssid(mac,
					(struct qdf_mac_addr *)assoc_ind->bssId,
					&session_id);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Couldn't find session_id for given BSSID");
		return QDF_STATUS_E_FAILURE;
	}

	/* Send Association completion message to PE */
	if (assoc_ind->owe_status)
		status = QDF_STATUS_E_INVAL;
	status = csr_send_assoc_cnf_msg(mac, assoc_ind, status,
					assoc_ind->owe_status);
	/*
	 * send a message to CSR itself just to avoid the EAPOL frames
	 * going OTA before association response
	 */
	if (assoc_ind->owe_status == 0)
		status = csr_send_assoc_ind_to_upper_layer_cnf_msg(mac,
								   assoc_ind,
								   status,
								   session_id);

	return status;
}
