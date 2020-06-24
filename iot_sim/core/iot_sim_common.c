/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#include "iot_sim_cmn_api_i.h"
#include "iot_sim_defs_i.h"
#include "wlan_iot_sim_tgt_api.h"
#include <qdf_mem.h>
#include <qdf_types.h>
#include <qdf_util.h>
#include <wmi_unified_param.h>

/*
 * iot_sim_convert_offset_to_hex_str - function to convert offset into binary
 * @offset: user provided offset value while action frame rule deletion
 * @hex: buffer to store converted value
 * @count: size of hex buffer
 *
 * Return: string pointer
 */
QDF_STATUS
iot_sim_convert_offset_to_hex_str(uint16_t offset, uint8_t *hex, int8_t count)
{
	uint8_t temp[5];
	int ret;

	snprintf(temp, sizeof(temp), "%04u", offset);

	ret = qdf_hex_str_to_binary(hex, temp, count);
	if (ret == -1) {
		iot_sim_err("offset to hex conversion failed");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/*
 * iot_sim_validate_content - function to validate frame content. User provided
 *			      content must be either full frame or full frame
 *			      body or valid TLV formatted data.
 * @buf: pointer to frame content in binary format
 * @total_len: length of content
 * @offset: offset provided by user
 *
 * Return: QDF_STATUS_SUCCESS on success
 *	   QDF_STATUS_E_FAULT on failure
 */
static QDF_STATUS
iot_sim_validate_content(uint8_t *buf,
			 uint16_t total_len,
			 uint16_t offset)
{
	char *ie = buf;
	uint32_t len = 0, i = 0;
	uint32_t fb = sizeof(struct ieee80211_frame);

	if (offset == 0 || offset == fb) {
		/* Replace the entire content set by user */
		return QDF_STATUS_SUCCESS;
	}

	/* Check for malformed IEs and proper IE
	 * boundaries in user content
	 */
	for (i = 0; i < total_len;) {
		/* TLV: T(1) + L(1) + V(L)*/
		len = (1 + 1 + ie[1]);
		i += len;
		ie += len;
	}

	if (i == total_len)
		return QDF_STATUS_SUCCESS;

	iot_sim_err("iot_sim: cnt(bin) len:%u IE Parsed len:%u",
		    total_len,
		    i);

	return QDF_STATUS_E_INVAL;
}

/*
 * iot_sim_handle_frame_content - function to process frame content provided
 *				  by user. This function will convert the ascii
 *				  string into binary string and validate the
 *				  content.
 * @isc: iot sim context
 * @pos: position to the frame content in the user buffer
 * @storage: storage to store frame content after processing
 * @offset: user provided offset
 * @len: length of the user provided content in bytes
 *
 * Return: QDF_STATUS_SUCCESS on success
 *	   QDF_STATUS_E_FAULT on hex str to binary conversion failure
 *	   QDF_STATUS_E_NOMEM on memory allocation failure
 */
QDF_STATUS
iot_sim_handle_frame_content(struct iot_sim_context *isc,
			     const char *pos,
			     uint8_t **storage,
			     uint16_t offset,
			     uint16_t len)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	int ret;

	*storage = qdf_mem_malloc(len);
	if (!*storage) {
		iot_sim_err("iot_sim:storage allocation failed");
		return QDF_STATUS_E_NOMEM;
	}

	ret = qdf_hex_str_to_binary(*storage, pos, len);
	if (ret == -1) {
		iot_sim_err("iot_sim:hex2bin conversion failed");
		status = QDF_STATUS_E_FAULT;
		goto error2;
	}

	status = iot_sim_validate_content(*storage, len, offset);
	if (QDF_IS_STATUS_ERROR(status)) {
		iot_sim_err("iot_sim:User Content Invalid");
		goto error2;
	}

	return QDF_STATUS_SUCCESS;

error2:
	qdf_mem_free(*storage);
	*storage = NULL;
	return status;
}

/*
 * iot_sim_parse_user_input_content_change - function to parse user input into
 *					     predefined format for content
 *					     change operation. All arguments
 *					     passed will be filled upon success
 * @isc: iot sim context
 * @userbuf: local copy of user input
 * @count: length of userbuf
 * @t_st: address of type variable
 * @seq: address of seq variable
 * @offset: address of offset variable
 * @length: address of length variable
 * @content: double pointer to storage to store frame content after processing
 * @addr: pointer to mac address
 *
 * @storage: storage to store frame content after processing
 * @offset: user provided offset
 * @len: length of the user provided content in bytes
 *
 * Return: QDF_STATUS_SUCCESS on success
 *	   QDF_STATUS_E_FAILURE otherwise
 */
QDF_STATUS
iot_sim_parse_user_input_content_change(struct iot_sim_context *isc,
					char *userbuf, ssize_t count,
					uint8_t *t_st, uint16_t *seq,
					uint16_t *offset, uint16_t *length,
					uint8_t **content,
					struct qdf_mac_addr *addr)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	char *argv[6], *delim = " ", *substr;
	int argc = -1, ret = 0;

	qdf_mem_zero(argv, sizeof(argv));
	userbuf = strim(userbuf);

	while ((substr = strsep(&userbuf, delim)) != NULL) {
		if (!isspace(*substr) && *substr != '\0')
			argv[++argc] = substr;
		if (argc >= 5)
			break;
	}

	if (argc < 3) {
		iot_sim_err("Invalid argument count %d", (argc + 1));
		return status;
	}

	if (!argv[0] || !argv[1] || !argv[2] || !argv[3]) {
		iot_sim_err("One or more arguments are null");
		return status;
	}

	ret = kstrtou8(argv[0], 16, t_st);
	if (ret)
		goto err;
	ret = kstrtou16(argv[1], 10, seq);
	if (ret)
		goto err;
	ret = kstrtou16(argv[2], 10, offset);
	if (ret)
		goto err;
	ret = kstrtou16(argv[3], 10, length);
	if (ret)
		goto err;
	/*
	 * User can send content change data in following format:
	 * 1. Add rule for specific peer
	 *	<t_st> <seq> <offset> <length> <content> <MAC>
	 * 2. Add rule for broadcast peer
	 *	<t_st> <seq> <offset> <length> <content>
	 * 3. Remove rule for specific peer
	 *	<t_st> <seq> <offset> <length> <MAC>
	 * 4. Remove rule for broadcast peer
	 *	<t_st> <seq> <offset> <length>
	 */

	/*
	 * If length is 0, this implies remove the rule
	 */
	if (!*length) {
		/*
		 * 1. Ignore the frame content
		 * 2. argv[4] is not null, then it must be a valid mac
		 *    If argv[4] is null, then set 'addr' as null
		 */
		*content = NULL;
		if (argv[4]) {
			status = qdf_mac_parse(argv[4], addr);
			if (QDF_IS_STATUS_ERROR(status))
				iot_sim_err("iot_sim: argv4 is invalid mac for 0 len");
		} else {
			qdf_mem_zero(addr, QDF_MAC_ADDR_SIZE);
			status = QDF_STATUS_SUCCESS;
		}
		/*
		 * No need to parse further just return.
		 */
		return status;
	}

	/*
	 * If argv[4] is valid, this implies frame content
	 */
	if (argv[4]) {
		status = iot_sim_handle_frame_content(isc, argv[4],
						      content, *offset,
						      *length);
		if (QDF_IS_STATUS_ERROR(status))
			return status;
	}

	/*
	 * If argv[5] is valid, this must be mac address
	 */
	if (argv[5])
		status = qdf_mac_parse(argv[5], addr);

	return status;
err:
	iot_sim_err("kstrtoXX failed: %d", ret);
	return status;
}

QDF_STATUS
iot_sim_get_index_for_action_frm(uint8_t *frm, uint8_t *cat,
				 uint8_t *act)
{
	uint8_t category, action;

	category = ((struct ieee80211_action *)(frm))->ia_category;
	action = ((struct ieee80211_action *)(frm))->ia_action;

	iot_sim_info("category %x action %x", category, action);

	switch (category) {
	case IEEE80211_ACTION_CAT_BA:
		switch (action) {
		case IEEE80211_ACTION_BA_ADDBA_REQUEST:
		case IEEE80211_ACTION_BA_ADDBA_RESPONSE:
		case IEEE80211_ACTION_BA_DELBA:
			*cat = CAT_BA;
			*act = action;
			break;
		default:
			return QDF_STATUS_E_FAULT;
		}
		break;
	case IEEE80211_ACTION_CAT_SA_QUERY:
		switch (action) {
		case IEEE80211_ACTION_SA_QUERY_REQUEST:
		case IEEE80211_ACTION_SA_QUERY_RESPONSE:
			*cat = CAT_SA_QUERY;
			*act = action;
			break;
		default:
			return QDF_STATUS_E_FAULT;
		}
		break;
	default:
		return QDF_STATUS_E_FAULT;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
iot_sim_del_cnt_cng_rule_peer_action(struct iot_sim_rule_per_peer *peer,
				     uint16_t seq, enum iot_sim_operations oper,
				     uint8_t cat, uint8_t act)
{
	/* seq entries and frame entries */
	struct iot_sim_rule_per_seq *s_e;
	struct iot_sim_rule *f_e;
	uint8_t bm;

	qdf_spin_lock(&peer->iot_sim_lock);
	s_e = peer->rule_per_seq[seq];
	if (!s_e) {
		qdf_spin_unlock(&peer->iot_sim_lock);
		iot_sim_err("s_e is null");
		return QDF_STATUS_SUCCESS;
	}

	f_e = s_e->rule_per_action_frm[cat][act];
	if (f_e && f_e->frm_content) {
		qdf_mem_free(f_e->frm_content);
		f_e->frm_content = NULL;
		bm = f_e->rule_bitmap;
		s_e->use_count--;
		if (IOT_SIM_CLEAR_OP_BIT(bm, oper)) {
			qdf_mem_free(f_e);
			s_e->rule_per_action_frm[cat][act] = NULL;
		}
	}
	if (s_e->use_count == 0) {
		qdf_mem_free(s_e);
		peer->rule_per_seq[seq] = NULL;
	}
	qdf_spin_unlock(&peer->iot_sim_lock);

	return QDF_STATUS_SUCCESS;
}

/*
 * iot_sim_del_cnt_cng_rule_peer - function to delete content change rule
 * @peer: iot sim peer
 * @type: 802.11 frame type
 * @subtype: 802.11 frame subtype
 * @seq: authentication sequence number, mostly 0 for non-authentication frame
 * @oper: iot sim operation
 * @cat: action category code
 * @act: action code
 * @action: boolean to indicate action frame
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS
iot_sim_del_cnt_cng_rule_peer(struct iot_sim_rule_per_peer *peer,
			      uint8_t type, uint8_t subtype,
			      uint16_t seq, enum iot_sim_operations oper,
			      uint8_t cat, uint8_t act, bool action)
{
	/* seq entries and frame entries */
	struct iot_sim_rule_per_seq *s_e;
	struct iot_sim_rule *f_e;
	uint8_t bm;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (action) {
		status = iot_sim_del_cnt_cng_rule_peer_action(peer, seq, oper,
							      cat, act);
		if (QDF_IS_STATUS_ERROR(status))
			iot_sim_err("Rule deletion for action frame failed");

		return status;
	}

	qdf_spin_lock(&peer->iot_sim_lock);
	s_e = peer->rule_per_seq[seq];
	if (!s_e) {
		qdf_spin_unlock(&peer->iot_sim_lock);
		iot_sim_info("s_e is null");
		return QDF_STATUS_SUCCESS;
	}

	f_e = s_e->rule_per_type[type][subtype];
	if (f_e && f_e->frm_content) {
		qdf_mem_free(f_e->frm_content);
		f_e->frm_content = NULL;
		bm = f_e->rule_bitmap;
		s_e->use_count--;
		if (IOT_SIM_CLEAR_OP_BIT(bm, oper)) {
			qdf_mem_free(f_e);
			s_e->rule_per_type[type][subtype] = NULL;
		}
		if (s_e->use_count == 0) {
			qdf_mem_free(s_e);
			peer->rule_per_seq[seq] = NULL;
		}
	}
	qdf_spin_unlock(&peer->iot_sim_lock);

	return QDF_STATUS_SUCCESS;
}

/*
 * iot_sim_frame_supported_by_fw - function to find if frame is supported by fw
 * @type: 802.11 frame type
 * @subtype: 802.11 frame subtype
 *
 * Return: true if supported else false
 */
bool
iot_sim_frame_supported_by_fw(uint8_t type, uint8_t subtype)
{
	switch (type << IEEE80211_FC0_TYPE_SHIFT) {
	case IEEE80211_FC0_TYPE_MGT:
		switch (subtype << IEEE80211_FC0_SUBTYPE_SHIFT) {
		case IEEE80211_FC0_SUBTYPE_BEACON:
				return true;
		default:
				return false;
		}
	case IEEE80211_FC0_TYPE_CTL:
		switch (subtype << IEEE80211_FC0_SUBTYPE_SHIFT) {
		case IEEE80211_FC0_SUBTYPE_TRIGGER:
		case IEEE80211_FC0_SUBTYPE_BAR:
		case IEEE80211_FC0_SUBTYPE_CTS:
		case IEEE80211_FC0_SUBTYPE_ACK:
		case IEEE80211_FC0_SUBTYPE_NDPA:
			return true;
		default:
			return false;
		}
	case IEEE80211_FC0_TYPE_DATA:
		switch (subtype << IEEE80211_FC0_SUBTYPE_SHIFT) {
		default:
			return true;
		}
	default:
		return false;
	}
}

/*
 * iot_sim_delete_rule_for_mac - function to delete content change rule
 *				 for given peer mac
 * @isc: iot sim context
 * @oper: iot sim operation
 * @seq: authentication sequence number, mostly 0 for non-authentication frame
 * @type: 802.11 frame type
 * @subtype: 802.11 frame subtype
 * @mac: peer mac address
 * @cat: action category code
 * @act: action code
 * @action: boolean to indicate action frame
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE otherwise
 */
QDF_STATUS
iot_sim_delete_rule_for_mac(struct iot_sim_context *isc,
			    enum iot_sim_operations oper,
			    uint16_t seq, uint8_t type,
			    uint8_t subtype, struct qdf_mac_addr *mac,
			    uint8_t cat, uint8_t act, bool action)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	struct simulation_test_params param;

	if (qdf_is_macaddr_zero(mac))
		iot_sim_info("Rule deletion for all peers");
	else
		iot_sim_info("Rule deletion for " QDF_MAC_ADDR_STR,
			     QDF_MAC_ADDR_ARRAY(mac->bytes));
	if (action)
		iot_sim_info("oper:%u cat type:%hu act type:%hu seq:%hu",
			     oper, cat, act, seq);
	else
		iot_sim_info("oper:%u type:%hu subtype:%hu seq:%hu", oper, type,
			     subtype, seq);

	if (!isc) {
		iot_sim_err("iot_sim: isc is null");
		return QDF_STATUS_E_FAILURE;
	}

	if (oper == CONTENT_CHANGE) {
		if (qdf_is_macaddr_broadcast(mac)) {
			/*
			 * Clear rules for bcast peer
			 */
			ret = iot_sim_del_cnt_cng_rule_peer(&isc->bcast_peer,
							    type, subtype,
							    seq, oper, cat, act,
							    action);
		} else if (qdf_is_macaddr_zero(mac)) {
			/*
			 * Zero mac address, rules will be deleted
			 * for all peer and including for bcast peer
			 */

			/*
			 * Clear rules for bcast peer
			 */
			ret = iot_sim_del_cnt_cng_rule_peer(&isc->bcast_peer,
							    type, subtype,
							    seq, oper, cat, act,
							    action);
			/* Clear rules for individual peer
			 * TBD
			 */
		} else {
			/* TBD: clear the rules for peer with address 'mac'*/
		}
	} else if (oper == DROP) {
		/* TBD */
	} else if (oper == DELAY) {
		/* TBD */
	}

	if (iot_sim_frame_supported_by_fw(type, subtype)) {
		qdf_mem_zero(&param, sizeof(struct simulation_test_params));
		param.pdev_id = wlan_objmgr_pdev_get_pdev_id(isc->pdev_obj);
		qdf_mem_copy(param.peer_mac, mac, QDF_MAC_ADDR_SIZE);
		param.test_cmd_type = oper;
		/* subtype_cmd: Rule:- set:0 clear:1 */
		param.test_subcmd_type = 1;
		param.frame_type = type;
		param.frame_subtype = subtype;
		param.seq = seq;
		param.bufp = NULL;
		if (QDF_IS_STATUS_ERROR(tgt_send_simulation_cmd(isc->pdev_obj,
								&param)))
			iot_sim_info("Sending del rule to fw failed!");
	}

	return ret;
}

/*
 * iot_sim_add_cnt_cng_rule_peer_action - function to add content change rule
 *					  for action frame
 * @isc: iot sim context
 * @peer: iot sim peer
 * @seq: authentication sequence number, mostly 0 for non-authentication frame
 * @frm: frame content to store
 * @offset: offset value
 * @len: length of frame content
 * @oper: iot sim operation
 * @cat: iot sim category code for action frame
 * @act: iot sim action code for action frame
 *
 * Return: QDF_STATUS_SUCCESS on success, failure otherwise
 */
QDF_STATUS
iot_sim_add_cnt_cng_rule_peer_action(struct iot_sim_context *isc,
				     struct iot_sim_rule_per_peer *peer,
				     uint16_t seq, uint8_t *frm,
				     uint16_t offset, uint16_t len,
				     enum iot_sim_operations oper,
				     uint8_t cat, uint8_t act)
{
	struct iot_sim_rule_per_seq *s_e = NULL;
	struct iot_sim_rule *f_e = NULL;

	qdf_spin_lock(&peer->iot_sim_lock);
	s_e = peer->rule_per_seq[seq];
	if (s_e) {
		f_e = s_e->rule_per_action_frm[cat][act];
		if (!f_e) {
			f_e = qdf_mem_malloc(sizeof(struct iot_sim_rule));
			if (!f_e) {
				iot_sim_err("can't allocate f_e for action frm");
				qdf_spin_unlock(&peer->iot_sim_lock);
				return QDF_STATUS_E_NOMEM;
			}
			s_e->rule_per_action_frm[cat][act] = f_e;
		}
	} else {
		s_e = qdf_mem_malloc(sizeof(struct iot_sim_rule_per_seq));
		if (!s_e) {
			qdf_spin_unlock(&peer->iot_sim_lock);
			iot_sim_err("can't allocate s_e for action");
			return QDF_STATUS_E_NOMEM;
		}

		f_e = qdf_mem_malloc(sizeof(struct iot_sim_rule));
		if (!f_e) {
			qdf_mem_free(s_e);
			peer->rule_per_seq[seq] = NULL;
			qdf_spin_unlock(&peer->iot_sim_lock);
			iot_sim_err("iot_sim: f_e is null");
			return QDF_STATUS_E_NOMEM;
		}
		s_e->rule_per_action_frm[cat][act] = f_e;
		peer->rule_per_seq[seq] = s_e;
	}

	s_e->use_count++;
	f_e->frm_content = qdf_mem_malloc(len);
	qdf_mem_copy(f_e->frm_content, frm, len);
	f_e->len = len;
	f_e->offset = offset;
	IOT_SIM_SET_OP_BIT(f_e->rule_bitmap, oper);
	qdf_spin_unlock(&peer->iot_sim_lock);

	iot_sim_info("iot_sim: Rule added for oper:%u", oper);
	iot_sim_info("cat:%x act:%x sq:%hu off:%hu len:%hu",
		     cat, act, seq, offset, len);

	return QDF_STATUS_SUCCESS;
}

/*
 * iot_sim_add_cnt_cng_rule_peer - function to add content change rule on peer
 * @peer: iot sim peer
 * @type: 802.11 frame type
 * @subtype: 802.11 frame subtype
 * @seq: authentication sequence number, mostly 0 for non-authentication frame
 * @frm: frame content to store
 * @offset: offset value
 * @len: length of frame content
 * @oper: iot sim operation
 * @cat: action category code
 * @act: action code
 * @action: boolean to indicate action frame
 *
 * Return: QDF_STATUS_SUCCESS on success, failure otherwise
 */
QDF_STATUS
iot_sim_add_cnt_cng_rule_peer(struct iot_sim_context *isc,
			      struct iot_sim_rule_per_peer *peer,
			      uint8_t type, uint8_t subtype,
			      uint16_t seq, uint8_t *frm,
			      uint16_t offset, uint16_t len,
			      enum iot_sim_operations oper,
			      uint8_t cat, uint8_t act, bool is_action)
{
	struct iot_sim_rule_per_seq *s_e = NULL;
	struct iot_sim_rule *f_e = NULL;
	struct simulation_test_params param;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (iot_sim_frame_supported_by_fw(type, subtype)) {
		qdf_mem_zero(&param, sizeof(struct simulation_test_params));
		param.pdev_id = wlan_objmgr_pdev_get_pdev_id(isc->pdev_obj);
		qdf_mem_copy(param.peer_mac, &peer->addr, QDF_MAC_ADDR_SIZE);
		param.test_cmd_type = oper;
		/* subtype_cmd: Rule:- set:0 clear:1 */
		param.test_subcmd_type = 0;
		param.frame_type = type;
		param.frame_subtype = subtype;
		param.seq = seq;
		param.offset = offset;
		param.frame_length = len;
		param.buf_len = len;
		param.bufp = frm;
		if (QDF_IS_STATUS_ERROR(tgt_send_simulation_cmd(isc->pdev_obj,
								&param)))
			iot_sim_info("Sending del rule to fw failed!");

		if (!FRAME_TYPE_IS_BEACON(type, subtype))
			return status;
	}

	if (is_action) {
		status = iot_sim_add_cnt_cng_rule_peer_action(isc, peer,
							      seq, frm,
							      offset, len,
							      oper, cat, act);
		if (QDF_IS_STATUS_ERROR(status))
			iot_sim_err("Add action rule failed!");

		return status;
	}

	qdf_spin_lock(&peer->iot_sim_lock);
	s_e = peer->rule_per_seq[seq];
	if (s_e) {
		f_e = s_e->rule_per_type[type][subtype];
		if (!f_e) {
			f_e = qdf_mem_malloc(sizeof(struct iot_sim_rule));
			if (!f_e) {
				iot_sim_err("can't allocate f_e");
				qdf_spin_unlock(&peer->iot_sim_lock);
				return QDF_STATUS_E_NOMEM;
			}
			s_e->rule_per_type[type][subtype] = f_e;
		}
	} else {
		s_e = qdf_mem_malloc(sizeof(struct iot_sim_rule_per_seq));
		if (!s_e) {
			qdf_spin_unlock(&peer->iot_sim_lock);
			iot_sim_err("iot_sim: s_e is null");
			return QDF_STATUS_E_NOMEM;
		}

		f_e = qdf_mem_malloc(sizeof(struct iot_sim_rule));
		if (!f_e) {
			qdf_mem_free(s_e);
			peer->rule_per_seq[seq] = NULL;
			qdf_spin_unlock(&peer->iot_sim_lock);
			iot_sim_err("iot_sim: f_e is null");
			return QDF_STATUS_E_NOMEM;
		}

		s_e->rule_per_type[type][subtype] = f_e;
		peer->rule_per_seq[seq] = s_e;
	}

	f_e->frm_content = qdf_mem_malloc(len);
	qdf_mem_copy(f_e->frm_content, frm, len);
	f_e->len = len;
	f_e->offset = offset;
	s_e->use_count++;
	IOT_SIM_SET_OP_BIT(f_e->rule_bitmap, oper);
	qdf_spin_unlock(&peer->iot_sim_lock);

	iot_sim_info("iot_sim:Rule added for mac: " QDF_MAC_ADDR_STR " oper:%u",
		     QDF_MAC_ADDR_ARRAY(peer->addr.bytes), oper);
	iot_sim_info("type:%x stype:%x sq:%hu off:%hu len:%hu",
		     type, subtype, seq, offset, len);

	return QDF_STATUS_SUCCESS;
}

/*
 * iot_sim_add_rule_for_mac - function to add content change rule
 *			      for given peer mac
 * @isc: iot sim context
 * @oper: iot sim operation
 * @mac: peer mac address
 * @type: 802.11 frame type
 * @subtype: 802.11 frame subtype
 * @seq: authentication sequence number, mostly 0 for non-authentication frame
 * @offset: user provided offset
 * @frm: user provided frame content
 * @length: length of frm
 * @cat: action category code
 * @act: action code
 * @action: boolean to indicate action frame
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE otherwise
 */
QDF_STATUS
iot_sim_add_rule_for_mac(struct iot_sim_context *isc,
			 enum iot_sim_operations oper,
			 struct qdf_mac_addr *mac,
			 uint8_t type, uint8_t subtype,
			 uint16_t seq, uint16_t offset,
			 uint8_t *frm, uint16_t len,
			 uint8_t cat, uint8_t act, bool is_action)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!isc) {
		iot_sim_err("iot_sim: isc is null");
		return status;
	}

	if (oper == CONTENT_CHANGE) {
		if (!frm)
			return status;

		status = iot_sim_delete_rule_for_mac(isc, oper, seq,
						     type, subtype, mac,
						     cat, act, is_action);
		if (status == QDF_STATUS_E_FAILURE) {
			iot_sim_err("iot_sim: Rule removed - Fail");
			return status;
		}

		if (qdf_is_macaddr_broadcast(mac)) {
			iot_sim_add_cnt_cng_rule_peer(isc, &isc->bcast_peer,
						      type, subtype, seq, frm,
						      offset, len, oper,
						      cat, act, is_action);
		} else {
			/*
			 * Add entry to peer with MAC 'mac'
			 */
			/* TBD */
		}
	} else if (oper == DROP) {
		/* TBD */
	} else if (oper == DELAY) {
		/* TBD */
	}

	return QDF_STATUS_SUCCESS;
}

/*
 *                   IOT SIM User Command Format
 *
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * | FrmType/subtype |  Seq  | Offset | Length | content | Mac Addr |
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * |     1Byte       | 2Byte | 2Bytes | 2Bytes | Length  | 6 Bytes  |
 *
 */

/*
 * iot_sim_debug_content_change_write - Write Handler for content change
 *					operation
 * @file: debugfs file pointer
 * @buf: buf of user input
 * @count: buf count
 * @ppos: offset on file
 *
 * Return: character read on success, failure otherwise
 */
static ssize_t
iot_sim_debug_content_change_write(struct file *file,
				   const char __user *buf,
				   size_t count, loff_t *ppos)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	unsigned char t_st, type, subtype, *content = NULL;
	uint16_t offset = 0, length = 0, seq = 0;
	char *locbuf, *ptr, hex[2];
	enum iot_sim_operations oper = CONTENT_CHANGE;
	struct qdf_mac_addr mac_addr = QDF_MAC_ADDR_BCAST_INIT;
	struct iot_sim_context *isc =
			((struct seq_file *)file->private_data)->private;
	uint8_t act_t = 0, cat_t = 0;
	bool is_action_frm = 0;

	if ((!buf) || (count > USER_BUF_LEN) || (count < 7))
		return -EFAULT;

	locbuf = qdf_mem_malloc(USER_BUF_LEN + 1);
	if (!locbuf)
		return -ENOMEM;

	if (copy_from_user(locbuf, buf, count)) {
		qdf_mem_free(locbuf);
		return -EFAULT;
	}

	status = iot_sim_parse_user_input_content_change(isc, locbuf, count,
							 &t_st, &seq, &offset,
							 &length, &content,
							 &mac_addr);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(locbuf);
		return count;
	}

	type = (t_st & IEEE80211_FC0_TYPE_MASK) >> IEEE80211_FC0_TYPE_SHIFT;
	subtype = (t_st & IEEE80211_FC0_SUBTYPE_MASK);
	subtype >>= IEEE80211_FC0_SUBTYPE_SHIFT;

	if (type > N_FRAME_TYPE || subtype > N_FRAME_SUBTYPE || seq > MAX_SEQ) {
		qdf_mem_free(locbuf);
		return -EFAULT;
	}

	if (FRAME_TYPE_IS_ACTION(type, subtype)) {
		if (!length) {
			status = iot_sim_convert_offset_to_hex_str(offset,
								   hex,
								   sizeof(hex));
			if (QDF_IS_STATUS_ERROR(status)) {
				qdf_mem_free(locbuf);
				return count;
			}

			/* Offset represet category type and action type */
			status = iot_sim_get_index_for_action_frm(hex,
								  &cat_t,
								  &act_t);
			if (status == QDF_STATUS_E_FAULT) {
				iot_sim_err("Get indices for action failed");
				goto free;
			}
		} else if (length && content) {
			/* if offset is zero, move ptr post header */
			if (offset == 0) {
				ptr = content + sizeof(struct ieee80211_frame);
			} else if (offset == sizeof(struct ieee80211_frame)) {
				ptr = content;
			} else {
				iot_sim_err("wrong offset for action frm cnt");
				goto free;
			}
			status = iot_sim_get_index_for_action_frm(ptr,
								  &cat_t,
								  &act_t);
		}
		is_action_frm = 1;
	}

	/* check for rule removal */
	if (!length || !content) {
		status = iot_sim_delete_rule_for_mac(isc, oper, seq,
						     type, subtype,
						     &mac_addr,
						     cat_t, act_t,
						     is_action_frm);
		if (QDF_IS_STATUS_ERROR(status))
			iot_sim_err("iot_sim: Rule removed - Fail");
		else
			iot_sim_err("iot_sim: Rule removed - success");

		qdf_mem_free(locbuf);
		return count;
	}

	status = iot_sim_add_rule_for_mac(isc, oper, &mac_addr, type, subtype,
					  seq, offset, content, length,
					  cat_t, act_t, is_action_frm);

	if (QDF_IS_STATUS_ERROR(status))
		iot_sim_err("iot_sim: Rule Add - Fail");
free:
	qdf_mem_free(content);
	qdf_mem_free(locbuf);
	return count;
}

/*
 * iot_sim_debug_delay_write - Write Handler for delay operation
 * @file: debugfs file pointer
 * @buf: buf of user input
 * @count: buf count
 * @ppos: offset on file
 *
 * Return: character read
 */
static ssize_t
iot_sim_debug_delay_write(struct file *file,
			  const char __user *buf,
			  size_t count, loff_t *ppos)
{
	iot_sim_err("delay iot sim ops called");
	return count;
}

/*
 * iot_sim_debug_drop_write - Write Handler for drop operation
 * @file: debugfs file pointer
 * @buf: buf of user input
 * @count: buf count
 * @ppos: offset on file
 *
 * Return: character read
 */
static ssize_t
iot_sim_debug_drop_write(struct file *file,
			 const char __user *buf,
			 size_t count, loff_t *ppos)
{
	iot_sim_err("drop iot sim ops called");
	return count;
}

/*
 * debug_iot_sim_##func_base##_show() - debugfs functions to display content
 *                                      dummy function
 * Return: success
 */
#define GENERATE_IOT_SIM_DEBUG_SHOW_FUNCS(func_base)			\
	static int iot_sim_debug_##func_base##_show(struct seq_file *m,	\
						    void *v)		\
{									\
	return qdf_status_to_os_return(QDF_STATUS_SUCCESS);		\
}

GENERATE_IOT_SIM_DEBUG_SHOW_FUNCS(content_change);
GENERATE_IOT_SIM_DEBUG_SHOW_FUNCS(delay);
GENERATE_IOT_SIM_DEBUG_SHOW_FUNCS(drop);

/*
 * debug_##func_base##_open() - Open debugfs entry for respective command
 * and event buffer.
 *
 * @inode: node for debug dir entry
 * @file: file handler
 *
 * Return: open status
 */
#define GENERATE_DEBUG_IOT_SIM_STRUCTS(func_base)			\
	static int debug_##func_base##_open(struct inode *inode,	\
					    struct file *file)		\
{									\
	return single_open(file, iot_sim_debug_##func_base##_show,	\
			   inode->i_private);				\
}									\
									\
static const struct file_operations debug_##func_base##_ops = {		\
	.open           = debug_##func_base##_open,			\
	.read           = seq_read,					\
	.llseek         = seq_lseek,					\
	.write          = iot_sim_debug_##func_base##_write,		\
	.release        = single_release,				\
}

GENERATE_DEBUG_IOT_SIM_STRUCTS(content_change);
GENERATE_DEBUG_IOT_SIM_STRUCTS(drop);
GENERATE_DEBUG_IOT_SIM_STRUCTS(delay);

/* Structure to maintain debug information */
struct iot_sim_dbgfs_file {
	const char *name;
	const struct file_operations *ops;
};

#define DEBUG_IOT_SIM(func_base) {	.name = #func_base,		\
					.ops = &debug_##func_base##_ops	\
}

struct iot_sim_dbgfs_file iot_sim_dbgfs_files[IOT_SIM_DEBUGFS_FILE_NUM] = {
	DEBUG_IOT_SIM(content_change),
	DEBUG_IOT_SIM(drop),
	DEBUG_IOT_SIM(delay),
};

QDF_STATUS
iot_sim_control_cmn(struct wlan_objmgr_pdev *pdev, wbuf_t wbuf)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct iot_sim_context *isc;

	if (!pdev) {
		iot_sim_err("PDEV is NULL!");
		goto bad;
	}

	isc = iot_sim_get_ctx_from_pdev(pdev);
	if (!isc) {
		iot_sim_err("iot_sim context is NULL!");
		goto bad;
	}

	status = QDF_STATUS_SUCCESS;
bad:
	return status;
}

/**
 * iot_sim_ctx_deinit() - De-initialize function pointers from iot_sim context
 * @sc - Reference to iot_sim_context object
 *
 * Return: None
 */
static void
iot_sim_ctx_deinit(struct iot_sim_context *isc)
{
	if (isc)
		isc->iot_sim_operation_handler = NULL;
}

/**
 * iot_sim_debugfs_deinit() - Deinit functions to remove debugfs directory and
 *			      it's file enteries.
 * @isc: iot_sim context
 *
 * Return: init status
 */
static QDF_STATUS
iot_sim_debugfs_deinit(struct iot_sim_context *isc)
{
	debugfs_remove_recursive(isc->iot_sim_dbgfs_ctx.iot_sim_dir_de);

	return QDF_STATUS_SUCCESS;
}

/*
 * iot_sim_remove_all_oper_rules - Function to remove all configured rules
 *				   for given operation
 *
 * @isc: iot sim context
 * @oper: iot sim operation
 *
 * Return: void
 */
static void
iot_sim_remove_all_oper_rules(struct iot_sim_context *isc,
			      enum iot_sim_operations oper)
{
	uint16_t seq;
	uint8_t type, subtype, cat = 0, act = 0;
	struct qdf_mac_addr zero_mac_addr = QDF_MAC_ADDR_ZERO_INIT;

	/* Remove rules for non-action type frames */
	for (seq = 0; seq < MAX_SEQ; seq++)
		for (type = 0; type < N_FRAME_TYPE; type++)
			for (subtype = 0; subtype < N_FRAME_SUBTYPE; subtype++)
				iot_sim_delete_rule_for_mac(isc, oper, seq,
							    type, subtype,
							    &zero_mac_addr,
							    cat, act, 0);
	/* Remove rules for action frames */
	for (seq = 0; seq < MAX_SEQ; seq++)
		for (cat = 0; cat < IOT_SIM_MAX_CAT; cat++)
			for (act = 0; act < MAX_ACTION; act++)
				iot_sim_delete_rule_for_mac(isc, oper, seq,
							    type, subtype,
							    &zero_mac_addr,
							    cat, act, 1);
}

/*
 * iot_sim_remove_all_rules - Function to remove all configured rules
 *
 * @isc: iot sim context
 *
 * Return: void
 */
static void
iot_sim_remove_all_rules(struct iot_sim_context *isc)
{
	enum iot_sim_operations oper;

	if (!isc)
		return;

	for (oper = CONTENT_CHANGE; oper < IOT_SIM_MAX_OPERATION; oper++)
		iot_sim_remove_all_oper_rules(isc, oper);
}

/**
 * iot_sim_debugfs_init() - debugfs functions to create debugfs directory and to
 *			    create debugfs enteries.
 * @isc: iot_sim context
 *
 * Return: init status
 */
static QDF_STATUS
iot_sim_debugfs_init(struct iot_sim_context *isc)
{
	struct dentry *dbgfs_dir = NULL;
	struct dentry *de = NULL;
	uint8_t i, pdev_id;
	char buf[32];

	if (!isc)
		return QDF_STATUS_E_FAILURE;

	pdev_id = wlan_objmgr_pdev_get_pdev_id(isc->pdev_obj);

	qdf_mem_zero(buf, sizeof(buf));
	snprintf(buf, sizeof(buf), "iot_sim_pdev%u", pdev_id);

	dbgfs_dir = debugfs_create_dir(buf, NULL);
	isc->iot_sim_dbgfs_ctx.iot_sim_dir_de = dbgfs_dir;

	if (!isc->iot_sim_dbgfs_ctx.iot_sim_dir_de) {
		iot_sim_err("dbgfs dir creation failed for pdev%u", pdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < IOT_SIM_DEBUGFS_FILE_NUM; ++i) {
		de = debugfs_create_file(iot_sim_dbgfs_files[i].name,
					 0644,
					 dbgfs_dir, isc,
					 iot_sim_dbgfs_files[i].ops);

		if (!de) {
			iot_sim_err("dbgfs file creation failed for pdev%u",
				    pdev_id);
			goto out;
		}
		isc->iot_sim_dbgfs_ctx.iot_sim_file_de[i] = de;
	}

	return QDF_STATUS_SUCCESS;

out:
	debugfs_remove_recursive(dbgfs_dir);
	qdf_mem_set(isc->iot_sim_dbgfs_ctx.iot_sim_file_de,
		    sizeof(isc->iot_sim_dbgfs_ctx.iot_sim_file_de), 0);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wlan_iot_sim_pdev_obj_create_handler(struct wlan_objmgr_pdev *pdev, void *arg)
{
	struct iot_sim_context *isc = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!pdev) {
		iot_sim_err("pdev is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	isc = qdf_mem_malloc(sizeof(struct iot_sim_context));
	if (!isc)
		return QDF_STATUS_E_NOMEM;

	isc->pdev_obj = pdev;

	if (QDF_IS_STATUS_ERROR(iot_sim_debugfs_init(isc))) {
		qdf_mem_free(isc);
		iot_sim_info("iot_sim debugfs file creation failed");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_set_macaddr_broadcast(&isc->bcast_peer.addr);
	qdf_spinlock_create(&isc->bcast_peer.iot_sim_lock);
	qdf_list_create(&isc->bcast_peer.p_list, 0);

	wlan_objmgr_pdev_component_obj_attach(pdev, WLAN_IOT_SIM_COMP,
					      (void *)isc, QDF_STATUS_SUCCESS);

	iot_sim_err("iot_sim component pdev object created");

	return status;
}

QDF_STATUS
wlan_iot_sim_pdev_obj_destroy_handler(struct wlan_objmgr_pdev *pdev,
				      void *arg)
{
	struct iot_sim_context *isc = NULL;

	if (!pdev) {
		iot_sim_err("pdev is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	isc = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						    WLAN_IOT_SIM_COMP);
	if (isc) {
		wlan_objmgr_pdev_component_obj_detach(pdev,
						      WLAN_IOT_SIM_COMP,
						      (void *)isc);
		/* Deinitilise function pointers from iot_sim context */
		iot_sim_ctx_deinit(isc);
		iot_sim_debugfs_deinit(isc);
		iot_sim_remove_all_rules(isc);
		qdf_spinlock_destroy(&isc->bcast_peer.iot_sim_lock);
		qdf_mem_free(isc);
	}
	iot_sim_err("iot_sim component pdev object destroyed");

	return QDF_STATUS_SUCCESS;
}
