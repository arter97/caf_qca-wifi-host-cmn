/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * This file sir_params.h contains the common parameter definitions, which
 * are not dependent on threadX API. These can be used by all Firmware
 * modules.
 *
 * Author:      Sandesh Goel
 * Date:        04/13/2002
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#ifndef __SIRPARAMS_H
#define __SIRPARAMS_H

#include "sir_types.h"

#define WAKELOCK_DURATION_RECOMMENDED	1000
#define WAKELOCK_DURATION_MAX		3000


#define SYSTEM_TIME_MSEC_TO_USEC      1000
#define SYSTEM_TIME_SEC_TO_MSEC       1000
#define SYSTEM_TIME_NSEC_TO_USEC      1000

/* defines for WPS config states */
#define       SAP_WPS_DISABLED             0
#define       SAP_WPS_ENABLED_UNCONFIGURED 1
#define       SAP_WPS_ENABLED_CONFIGURED   2


/* Firmware wide constants */

#define SIR_MAX_PACKET_SIZE     512
#define SIR_MAX_NUM_CHANNELS    64
#define SIR_MAX_NUM_STA_IN_IBSS 16
#define SIR_ESE_MAX_MEAS_IE_REQS   8

typedef enum {
	PHY_SINGLE_CHANNEL_CENTERED = 0,        /* 20MHz IF bandwidth centered on IF carrier */
	PHY_DOUBLE_CHANNEL_LOW_PRIMARY = 1,     /* 40MHz IF bandwidth with lower 20MHz supporting the primary channel */
	PHY_DOUBLE_CHANNEL_HIGH_PRIMARY = 3,    /* 40MHz IF bandwidth with higher 20MHz supporting the primary channel */
	PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED = 4,     /* 20/40MHZ offset LOW 40/80MHZ offset CENTERED */
	PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED = 5,        /* 20/40MHZ offset CENTERED 40/80MHZ offset CENTERED */
	PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED = 6,    /* 20/40MHZ offset HIGH 40/80MHZ offset CENTERED */
	PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW = 7,  /* 20/40MHZ offset LOW 40/80MHZ offset LOW */
	PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW = 8, /* 20/40MHZ offset HIGH 40/80MHZ offset LOW */
	PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH = 9, /* 20/40MHZ offset LOW 40/80MHZ offset HIGH */
	PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH = 10,       /* 20/40MHZ offset-HIGH 40/80MHZ offset HIGH */
	PHY_CHANNEL_BONDING_STATE_MAX = 11
} ePhyChanBondState;

#define MAX_BONDED_CHANNELS 8
/**
 * enum cap_bitmap - bit field for FW capability
 * MCC - indicate MCC
 * P2P - indicate P2P
 * DOT11AC - indicate 11AC
 * DOT11AC_OPMODE - indicate 11ac opmode
 * SAP32STA - indicate SAP32STA
 * TDLS - indicate TDLS
 * P2P_GO_NOA_DECOUPLE_INIT_SCAN - indicate P2P_GO_NOA_DECOUPLE_INIT_SCAN
 * WLANACTIVE_OFFLOAD - indicate active offload
 * EXTENDED_SCAN - indicate extended scan
 * PNO - indicate PNO
 * NAN - indicate NAN
 * RTT - indicate RTT
 * DOT11AX - indicate 11ax
 * DOT11BE - indicate 11be
 * SECURE_NAN - indicate NAN Pairing protocol
 * WOW - indicate WOW
 * WLAN_ROAM_SCAN_OFFLOAD - indicate Roam scan offload
 * WLAN_PERIODIC_TX_PTRN - indicate WLAN_PERIODIC_TX_PTRN
 * ADVANCE_TDLS - indicate advanced TDLS
 * TDLS_OFF_CHANNEL - indicate TDLS off channel
 *
 * This definition is independent of any other modules.
 * We can use any unused numbers.
 */
#define MAX_SUPPORTED_FEATURE 32
enum cap_bitmap {
	MCC = 0,
	P2P = 1,
	DOT11AC = 2,
	DOT11AC_OPMODE = 4,
	SAP32STA = 5,
	TDLS = 6,
	P2P_GO_NOA_DECOUPLE_INIT_SCAN = 7,
	WLANACTIVE_OFFLOAD = 8,
	EXTENDED_SCAN = 9,
#ifdef FEATURE_WLAN_SCAN_PNO
	PNO = 10,
#endif
#ifdef WLAN_FEATURE_NAN
	NAN = 11,
#endif
	RTT = 12,
	DOT11AX = 13,
#ifdef WLAN_FEATURE_11BE
	DOT11BE = 14,
#endif
#ifdef WLAN_FEATURE_NAN
	SECURE_NAN = 15,
#endif
	WOW = 22,
	WLAN_ROAM_SCAN_OFFLOAD = 23,
	WLAN_PERIODIC_TX_PTRN = 28,
#ifdef FEATURE_WLAN_TDLS
	ADVANCE_TDLS = 29,
	TDLS_OFF_CHANNEL = 30,
#endif
	VDEV_LATENCY_CONFIG = 31,

	/* MAX_FEATURE_SUPPORTED = 32 */
};

/* / Mailbox Message Structure Define */
typedef struct sSirMbMsg {
	uint16_t type;

	/**
	 * This length includes 4 bytes of header, that is,
	 * 2 bytes type + 2 bytes msgLen + n*4 bytes of data.
	 * This field is byte length.
	 */
	uint16_t msgLen;

	/**
	 * This is the first data word in the mailbox message.
	 * It is followed by n words of data.
	 * NOTE: data[1] is not a place holder to store data
	 * instead to dereference the message body.
	 */
	QDF_FLEX_ARRAY(uint32_t, data);
} tSirMbMsg, *tpSirMbMsg;

/**
 * struct sir_mgmt_msg - Structure used to send auth frame from CSR to LIM
 * @type: Message type
 * @msg_len: Message length
 * @vdev_id: vdev id
 * @data: Pointer to data tobe transmitted
 */
struct sir_mgmt_msg {
	uint16_t type;
	uint16_t msg_len;
	uint8_t vdev_id;
	uint8_t *data;
};

/**
 * struct sir_cfg_action_frm_tb_ppdu - cfg to set action frame in he tb ppdu
 * @type: Message type
 * @vdev_id: vdev id
 * @cfg: enable/disable cfg
 */
struct sir_cfg_action_frm_tb_ppdu {
	uint16_t type;
	uint8_t vdev_id;
	uint8_t cfg;
};

/* ******************************************* *
*                                             *
*         SIRIUS MESSAGE TYPES                *
*                                             *
* ******************************************* */

/*
 * The following message types have bounds defined for each module for
 * inter thread/module communications.
 * Each module will get 256 message types in total.
 * Note that message type definitions for mailbox messages for
 * communication with Host are in wni_api.h file.
 *
 * Any addition/deletion to this message list should also be
 * reflected in the halUtil_getMsgString() routine.
 */

/**
 * Module ID definitions.
 */
enum {
	SIR_HAL_MODULE_ID = 0x10,
	SIR_LIM_MODULE_ID = 0x13,
	SIR_SME_MODULE_ID,
};

#define SIR_WMA_MODULE_ID SIR_HAL_MODULE_ID

/* HAL message types */
enum halmsgtype {
	SIR_HAL_MSG_TYPES_BEGIN           = (SIR_HAL_MODULE_ID << 8),
	SIR_HAL_ITC_MSG_TYPES_BEGIN       = (SIR_HAL_MSG_TYPES_BEGIN + 0x20),
	SIR_HAL_ADD_STA_REQ               = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 1),
	SIR_HAL_ADD_STA_RSP               = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 2),
	SIR_HAL_DELETE_STA_REQ            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 3),
	SIR_HAL_DELETE_STA_RSP            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 4),
	SIR_HAL_ADD_BSS_REQ               = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 5),
	SIR_HAL_DELETE_BSS_REQ            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 7),
	SIR_HAL_DELETE_BSS_RSP            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 8),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 9 -> 16), are unused */
	SIR_HAL_SEND_BEACON_REQ           = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 17),

	SIR_HAL_SET_BSSKEY_RSP            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 19),
	SIR_HAL_SET_STAKEY_RSP            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 21),
	SIR_HAL_UPDATE_EDCA_PROFILE_IND   = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 22),

	SIR_HAL_UPDATE_BEACON_IND         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 23),
	SIR_HAL_ADD_TS_REQ                = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 26),
	SIR_HAL_DEL_TS_REQ                = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 27),

	SIR_HAL_MISSED_BEACON_IND         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 34),
	SIR_HAL_PWR_SAVE_CFG              = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 36),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 37 -> 44) are unused */
	SIR_HAL_SET_LINK_STATE            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 45),
	SIR_HAL_DELETE_BSS_HO_FAIL_REQ    = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 46),
	SIR_HAL_DELETE_BSS_HO_FAIL_RSP    = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 47),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 48 -> 58) */
	SIR_HAL_SET_STA_BCASTKEY_RSP      = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 59),
	SIR_HAL_ADD_TS_RSP                = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 60),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 66) is unused */
	SIR_HAL_SET_MIMOPS_REQ            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 67),
	SIR_HAL_SET_MIMOPS_RSP            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 68),
	SIR_HAL_SYS_READY_IND             = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 69),
	SIR_HAL_SET_TX_POWER_REQ          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 70),
	SIR_HAL_SET_TX_POWER_RSP          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 71),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 72 -> 93) are unused */
	SIR_HAL_SEND_PROBE_RSP_TMPL       = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 94),
	SIR_LIM_ADDR2_MISS_IND            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 95),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 96 -> 97) are unused */
	SIR_HAL_SET_MAX_TX_POWER_REQ      = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 98),
	SIR_HAL_SET_MAX_TX_POWER_RSP      = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 99),
	SIR_HAL_SET_HOST_OFFLOAD          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 100),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 101 -> 108) are unused */
	SIR_HAL_AGGR_QOS_REQ              = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 109),
	SIR_HAL_AGGR_QOS_RSP              = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 110),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 111) is unused */
	SIR_HAL_P2P_NOA_ATTR_IND          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 112),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 113 -> 116) are unused */
	SIR_HAL_SET_KEEP_ALIVE            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 117),

#ifdef WLAN_NS_OFFLOAD
	SIR_HAL_SET_NS_OFFLOAD            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 118),
#endif /* WLAN_NS_OFFLOAD */

	SIR_HAL_SOC_ANTENNA_MODE_REQ      = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 120),
	SIR_HAL_SOC_ANTENNA_MODE_RESP     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 121),

	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 122) is unused */

	SIR_HAL_8023_MULTICAST_LIST_REQ   = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 123),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 124 -> 131) are unused */
#ifdef FEATURE_WLAN_ESE
	SIR_HAL_TSM_STATS_REQ             = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 132),
	SIR_HAL_TSM_STATS_RSP             = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 133),
#endif

	SIR_HAL_SET_TM_LEVEL_REQ          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 134),

	SIR_HAL_UPDATE_OP_MODE            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 135),

	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 136 -> 138) are unused */

	SIR_HAL_ROAM_PRE_AUTH_STATUS_IND  = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 139),

	SIR_HAL_EXCLUDE_UNENCRYPTED_IND   = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 142),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 143 -> 145) are unused */

	SIR_HAL_STOP_SCAN_OFFLOAD_REQ     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 146),
	SIR_HAL_DHCP_START_IND            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 148),
	SIR_HAL_DHCP_STOP_IND             = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 149),
	SIR_HAL_ADD_PERIODIC_TX_PTRN_IND  = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 152),
	SIR_HAL_DEL_PERIODIC_TX_PTRN_IND  = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 153),
	SIR_HAL_PDEV_DUAL_MAC_CFG_REQ     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 154),
	SIR_HAL_PDEV_MAC_CFG_RESP         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 155),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 156 -> 158) are unused */
	SIR_HAL_RATE_UPDATE_IND           = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 159),

	SIR_HAL_FLUSH_LOG_TO_FW           = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 160),

	SIR_HAL_SET_PCL_TO_FW             = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 161),

#ifdef WLAN_MWS_INFO_DEBUGFS
	SIR_HAL_GET_MWS_COEX_INFO_REQ     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 162),
#endif /* WLAN_MWS_INFO_DEBUGFS */

	SIR_HAL_CLI_SET_CMD               = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 163),
#ifndef REMOVE_PKT_LOG
	SIR_HAL_PKTLOG_ENABLE_REQ         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 164),
#endif
	SIR_HAL_UPDATE_CHAN_LIST_REQ      = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 167),
	SIR_CSA_OFFLOAD_EVENT             = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 169),

	SIR_HAL_SET_MAX_TX_POWER_PER_BAND_REQ =
					(SIR_HAL_ITC_MSG_TYPES_BEGIN + 170),

	SIR_HAL_UPDATE_MEMBERSHIP         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 172),
	SIR_HAL_UPDATE_USERPOS            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 173),

#ifdef FEATURE_WLAN_TDLS
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 174) is not used */
	SIR_HAL_UPDATE_TDLS_PEER_STATE    = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 175),
#endif
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 176 -> 178) are not used */
	SIR_HAL_BEACON_TX_SUCCESS_IND     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 179),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 180 -> 184) are unused */
	SIR_HAL_INIT_THERMAL_INFO_CMD     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 185),
	SIR_HAL_SET_THERMAL_LEVEL         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 186),

#ifdef FEATURE_WLAN_ESE
	SIR_HAL_SET_PLM_REQ               = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 187),
#endif

	SIR_HAL_SET_TX_POWER_LIMIT        = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 188),
	SIR_HAL_SET_SAP_INTRABSS_DIS      = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 189),

	SIR_HAL_MODEM_POWER_STATE_IND     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 190),

	SIR_HAL_DISASSOC_TX_COMP          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 191),
	SIR_HAL_DEAUTH_TX_COMP            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 192),

	SIR_HAL_UPDATE_RX_NSS             = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 193),

#ifdef WLAN_FEATURE_STATS_EXT
	SIR_HAL_STATS_EXT_REQUEST         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 194),
#endif /* WLAN_FEATURE_STATS_EXT */
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 195 -> 197) are unused */
#ifdef FEATURE_WLAN_EXTSCAN
	SIR_HAL_EXTSCAN_GET_CAPABILITIES_REQ =
					(SIR_HAL_ITC_MSG_TYPES_BEGIN + 198),
	SIR_HAL_EXTSCAN_START_REQ         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 199),
	SIR_HAL_EXTSCAN_STOP_REQ          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 200),
	SIR_HAL_EXTSCAN_SET_BSS_HOTLIST_REQ =
					(SIR_HAL_ITC_MSG_TYPES_BEGIN + 201),
	SIR_HAL_EXTSCAN_RESET_BSS_HOTLIST_REQ =
					(SIR_HAL_ITC_MSG_TYPES_BEGIN + 202),
	SIR_HAL_EXTSCAN_SET_SIGNF_CHANGE_REQ =
					(SIR_HAL_ITC_MSG_TYPES_BEGIN + 203),
	SIR_HAL_EXTSCAN_RESET_SIGNF_CHANGE_REQ =
					(SIR_HAL_ITC_MSG_TYPES_BEGIN + 204),
	SIR_HAL_EXTSCAN_GET_CACHED_RESULTS_REQ =
					(SIR_HAL_ITC_MSG_TYPES_BEGIN + 205),
#endif /* FEATURE_WLAN_EXTSCAN */

#ifdef FEATURE_WLAN_CH_AVOID
	SIR_HAL_CH_AVOID_UPDATE_REQ       = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 206),
#endif

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
	SIR_HAL_LL_STATS_CLEAR_REQ        = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 207),
	SIR_HAL_LL_STATS_SET_REQ          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 208),
	SIR_HAL_LL_STATS_GET_REQ          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 209),
	SIR_HAL_LL_STATS_RESULTS_RSP      = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 210),
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 211 -> 212) are unused */
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
	SIR_HAL_SET_AUTO_SHUTDOWN_TIMER_REQ =
					(SIR_HAL_ITC_MSG_TYPES_BEGIN + 213),
#endif

	SIR_HAL_SET_BASE_MACADDR_IND      = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 214),

	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 215) is unused */

	SIR_HAL_LINK_STATUS_GET_REQ       = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 216),

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
	SIR_HAL_CONFIG_EXT_WOW            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 217),
	SIR_HAL_CONFIG_APP_TYPE1_PARAMS   = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 218),
	SIR_HAL_CONFIG_APP_TYPE2_PARAMS   = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 219),
#endif

	SIR_HAL_GET_TEMPERATURE_REQ       = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 220),
	SIR_HAL_SET_SCAN_MAC_OUI_REQ      = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 221),
#ifdef DHCP_SERVER_OFFLOAD
	SIR_HAL_SET_DHCP_SERVER_OFFLOAD   = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 222),
#endif /* DHCP_SERVER_OFFLOAD */
	SIR_HAL_LED_FLASHING_REQ          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 223),

	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 224 -> 228) are unused */

	SIR_HAL_SET_MAS                   = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 229),
	SIR_HAL_SET_MIRACAST              = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 230),
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	SIR_HAL_UPDATE_Q2Q_IE_IND         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 231),
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	SIR_HAL_CONFIG_STATS_FACTOR       = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 232),
	SIR_HAL_CONFIG_GUARD_TIME         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 233),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 234), is unused */

	SIR_HAL_ENTER_PS_REQ              = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 235),
	SIR_HAL_EXIT_PS_REQ               = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 236),
	SIR_HAL_ENABLE_UAPSD_REQ          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 237),
	SIR_HAL_DISABLE_UAPSD_REQ         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 238),
	SIR_HAL_GATEWAY_PARAM_UPDATE_REQ  = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 239),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 240 -> 312) are unused */
	SIR_HAL_SET_EPNO_LIST_REQ         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 313),
	SIR_HAL_SET_PASSPOINT_LIST_REQ    = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 316),
	SIR_HAL_RESET_PASSPOINT_LIST_REQ  = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 317),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 318 -> 327) are unused */
	SIR_HAL_START_STOP_LOGGING        = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 328),
	SIR_HAL_PDEV_SET_HW_MODE          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 329),
	SIR_HAL_PDEV_SET_HW_MODE_RESP     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 330),
	SIR_HAL_PDEV_HW_MODE_TRANS_IND    = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 331),

	SIR_HAL_BAD_PEER_TX_CTL_INI_CMD   = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 332),
	SIR_HAL_SET_RSSI_MONITOR_REQ      = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 333),
	SIR_HAL_SET_IE_INFO               = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 334),

	/* SIR_HAL_ITC_MSG_TYPES_BEGIN + 335 -> 336 are unused */
	SIR_HAL_HT40_OBSS_SCAN_IND        = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 337),

	SIR_HAL_TSF_GPIO_PIN_REQ          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 338),

	SIR_HAL_ADD_BCN_FILTER_CMDID      = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 339),
	SIR_HAL_REMOVE_BCN_FILTER_CMDID   = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 340),


	SIR_HAL_APF_GET_CAPABILITIES_REQ  = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 341),
	SIR_HAL_WMA_ROAM_SYNC_TIMEOUT     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 342),

	SIR_HAL_SET_WISA_PARAMS           = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 343),
	SIR_HAL_SET_ADAPT_DWELLTIME_PARAMS =
					(SIR_HAL_ITC_MSG_TYPES_BEGIN + 344),
	SIR_HAL_SET_PDEV_IE_REQ           = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 345),

	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 346 -> 359) are unused */

	SIR_HAL_SEND_FREQ_RANGE_CONTROL_IND =
					(SIR_HAL_ITC_MSG_TYPES_BEGIN + 360),
	SIR_HAL_POWER_DBG_CMD             = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 362),
	SIR_HAL_SET_DTIM_PERIOD           = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 363),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 364) is unused */
	SIR_HAL_SHORT_RETRY_LIMIT_CNT     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 365),
	SIR_HAL_LONG_RETRY_LIMIT_CNT      = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 366),
	SIR_HAL_UPDATE_TX_FAIL_CNT_TH     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 367),
	SIR_HAL_POWER_DEBUG_STATS_REQ     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 368),

	SIR_HAL_SET_WOW_PULSE_CMD         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 369),

	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 370 -> 371) are unused */
	SIR_HAL_RX_CHN_STATUS_EVENT       = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 372),

	SIR_HAL_GET_RCPI_REQ              = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 373),

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
	SIR_HAL_LL_STATS_EXT_SET_THRESHOLD =
					(SIR_HAL_ITC_MSG_TYPES_BEGIN + 378),
#endif
	SIR_HAL_SET_DBS_SCAN_SEL_PARAMS   = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 379),
	/* (SIR_HAL_ITC_MSG_TYPES_BEGIN + 380 -> 387) are unused */
	SIR_HAL_SET_ARP_STATS_REQ         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 388),
	SIR_HAL_GET_ARP_STATS_REQ         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 389),

	SIR_HAL_SET_LIMIT_OFF_CHAN        = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 390),

	SIR_HAL_SET_DEL_PMKID_CACHE       = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 391),
	SIR_HAL_HLP_IE_INFO               = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 392),
	SIR_HAL_OBSS_DETECTION_REQ        = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 393),
	SIR_HAL_OBSS_DETECTION_INFO       = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 394),
	SIR_HAL_INVOKE_NEIGHBOR_REPORT    = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 395),
	SIR_HAL_OBSS_COLOR_COLLISION_REQ  = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 396),
	SIR_HAL_OBSS_COLOR_COLLISION_INFO = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 397),

	SIR_HAL_SEND_ADDBA_REQ            = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 398),
	SIR_HAL_GET_ROAM_SCAN_STATS       = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 399),
	SIR_HAL_SEND_AP_VDEV_UP           = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 400),
	SIR_HAL_SEND_BCN_RSP              = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 401),
	SIR_HAL_CFG_VENDOR_ACTION_TB_PPDU = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 402),
	SIR_HAL_BEACON_DEBUG_STATS_REQ    = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 403),

#ifdef WLAN_FEATURE_MOTION_DETECTION
	SIR_HAL_SET_MOTION_DET_CONFIG     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 405),
	SIR_HAL_SET_MOTION_DET_ENABLE     = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 406),
	SIR_HAL_SET_MOTION_DET_BASE_LINE_CONFIG =
					(SIR_HAL_ITC_MSG_TYPES_BEGIN + 407),
	SIR_HAL_SET_MOTION_DET_BASE_LINE_ENABLE =
					(SIR_HAL_ITC_MSG_TYPES_BEGIN + 408),
#endif /* WLAN_FEATURE_MOTION_DETECTION */

#ifdef FW_THERMAL_THROTTLE_SUPPORT
	SIR_HAL_SET_THERMAL_THROTTLE_CFG  = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 409),
	SIR_HAL_SET_THERMAL_MGMT          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 410),
#endif /* FW_THERMAL_THROTTLE_SUPPORT */

	SIR_HAL_SEND_PEER_UNMAP_CONF      = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 411),

	SIR_HAL_GET_ISOLATION             = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 412),

	SIR_HAL_SET_ROAM_TRIGGERS         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 413),

	SIR_HAL_ROAM_SCAN_CH_REQ          = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 414),

	SIR_HAL_REQ_SEND_DELBA_REQ_IND    = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 415),
	SIR_HAL_SEND_MAX_TX_POWER         = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 416),

	SIR_HAL_TWT_ADD_DIALOG_REQUEST    = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 417),
	SIR_HAL_TWT_DEL_DIALOG_REQUEST    = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 418),
	SIR_HAL_TWT_PAUSE_DIALOG_REQUEST  = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 419),
	SIR_HAL_TWT_RESUME_DIALOG_REQUEST = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 420),
	SIR_HAL_PEER_CREATE_REQ           = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 421),
	SIR_HAL_TWT_NUDGE_DIALOG_REQUEST  = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 422),
	SIR_HAL_PASN_PEER_DELETE_REQUEST  = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 423),
	SIR_HAL_UPDATE_EDCA_PIFS_PARAM_IND = (SIR_HAL_ITC_MSG_TYPES_BEGIN + 424),

	SIR_HAL_MSG_TYPES_END               = (SIR_HAL_MSG_TYPES_BEGIN + 0x1FF),
};
/* LIM message types */
#define SIR_LIM_MSG_TYPES_BEGIN        (SIR_LIM_MODULE_ID << 8)
#define SIR_LIM_ITC_MSG_TYPES_BEGIN    (SIR_LIM_MSG_TYPES_BEGIN+0xB0)
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 1) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 2) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 3) */
/* Message from BB Transport */
#define SIR_BB_XPORT_MGMT_MSG          (SIR_LIM_ITC_MSG_TYPES_BEGIN + 4)
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 5) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 6) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 7) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 8) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 9) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0xA) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0xB) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0xC) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0xD) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0xE) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0xF) */
/* UNUSED                              (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0x10) */
/* Indication from HAL to delete Station context */
#define SIR_LIM_DELETE_STA_CONTEXT_IND (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0x11)
/* Indication from HAL to delete BA */
#define SIR_LIM_UPDATE_BEACON          (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0x13)
/* Indication from HAL to handle RX invalid peer */
#define SIR_LIM_RX_INVALID_PEER        (SIR_LIM_ITC_MSG_TYPES_BEGIN + 0x15)

/* LIM Timeout messages */
#define SIR_LIM_TIMEOUT_MSG_START      ((SIR_LIM_MODULE_ID << 8) + 0xD0)
#define SIR_LIM_JOIN_FAIL_TIMEOUT      (SIR_LIM_TIMEOUT_MSG_START + 2)
#define SIR_LIM_AUTH_FAIL_TIMEOUT      (SIR_LIM_TIMEOUT_MSG_START + 3)
#define SIR_LIM_AUTH_RSP_TIMEOUT       (SIR_LIM_TIMEOUT_MSG_START + 4)
#define SIR_LIM_ASSOC_FAIL_TIMEOUT     (SIR_LIM_TIMEOUT_MSG_START + 5)
#define SIR_LIM_REASSOC_FAIL_TIMEOUT   (SIR_LIM_TIMEOUT_MSG_START + 6)
#define SIR_LIM_HEART_BEAT_TIMEOUT     (SIR_LIM_TIMEOUT_MSG_START + 7)
/* currently unused                    SIR_LIM_TIMEOUT_MSG_START + 0x8 */
/* Link Monitoring Messages */
#define SIR_LIM_PROBE_HB_FAILURE_TIMEOUT (SIR_LIM_TIMEOUT_MSG_START + 0xB)
#define SIR_LIM_ADDTS_RSP_TIMEOUT        (SIR_LIM_TIMEOUT_MSG_START + 0xC)
#define SIR_LIM_LINK_TEST_DURATION_TIMEOUT (SIR_LIM_TIMEOUT_MSG_START + 0x13)
#define SIR_LIM_CNF_WAIT_TIMEOUT         (SIR_LIM_TIMEOUT_MSG_START + 0x17)
/* currently unused			(SIR_LIM_TIMEOUT_MSG_START + 0x18) */
#define SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT (SIR_LIM_TIMEOUT_MSG_START + 0x19)

#define SIR_LIM_WPS_OVERLAP_TIMEOUT      (SIR_LIM_TIMEOUT_MSG_START + 0x1D)
#define SIR_LIM_FT_PREAUTH_RSP_TIMEOUT   (SIR_LIM_TIMEOUT_MSG_START + 0x1E)

#define SIR_LIM_RRM_STA_STATS_RSP_TIMEOUT    (SIR_LIM_TIMEOUT_MSG_START + 0x24)
/* currently unused                     (SIR_LIM_TIMEOUT_MSG_START + 0x25) */

#define SIR_LIM_DISASSOC_ACK_TIMEOUT       (SIR_LIM_TIMEOUT_MSG_START + 0x26)
/*#define SIR_LIM_DEAUTH_ACK_TIMEOUT       (SIR_LIM_TIMEOUT_MSG_START + 0x27) */
#define SIR_LIM_PERIODIC_JOIN_PROBE_REQ_TIMEOUT \
					 (SIR_LIM_TIMEOUT_MSG_START + 0x28)

#define SIR_LIM_AUTH_RETRY_TIMEOUT     (SIR_LIM_TIMEOUT_MSG_START + 0x2D)
#define SIR_LIM_AUTH_SAE_TIMEOUT       (SIR_LIM_TIMEOUT_MSG_START + 0x2E)

#define SIR_LIM_PROCESS_DEFERRED_QUEUE (SIR_LIM_TIMEOUT_MSG_START + 0x2F)

#define SIR_LIM_MSG_TYPES_END            (SIR_LIM_MSG_TYPES_BEGIN+0xFF)

/* ****************************************** *
*                                            *
*         EVENT TYPE Definitions              *
*                                            *
* ****************************************** */

/* Param Change Bitmap sent to HAL */
#define PARAM_BCN_INTERVAL_CHANGED                      (1 << 0)
#define PARAM_SHORT_PREAMBLE_CHANGED                 (1 << 1)
#define PARAM_SHORT_SLOT_TIME_CHANGED                 (1 << 2)
#define PARAM_llACOEXIST_CHANGED                            (1 << 3)
#define PARAM_llBCOEXIST_CHANGED                            (1 << 4)
#define PARAM_llGCOEXIST_CHANGED                            (1 << 5)
#define PARAM_HT20MHZCOEXIST_CHANGED                  (1<<6)
#define PARAM_NON_GF_DEVICES_PRESENT_CHANGED (1<<7)
#define PARAM_RIFS_MODE_CHANGED                            (1<<8)
#define PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED   (1<<9)
#define PARAM_OBSS_MODE_CHANGED                               (1<<10)
#define PARAM_BSS_COLOR_CHANGED			(1 << 11)
#define PARAM_BEACON_UPDATE_MASK    (PARAM_BCN_INTERVAL_CHANGED | \
				     PARAM_SHORT_PREAMBLE_CHANGED | \
				     PARAM_SHORT_SLOT_TIME_CHANGED | \
				     PARAM_llACOEXIST_CHANGED | \
				     PARAM_llBCOEXIST_CHANGED | \
				     PARAM_llGCOEXIST_CHANGED | \
				     PARAM_HT20MHZCOEXIST_CHANGED | \
				     PARAM_NON_GF_DEVICES_PRESENT_CHANGED | \
				     PARAM_RIFS_MODE_CHANGED | \
				     PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED | \
				     PARAM_OBSS_MODE_CHANGED | \
				     PARAM_BSS_COLOR_CHANGED)

#endif
