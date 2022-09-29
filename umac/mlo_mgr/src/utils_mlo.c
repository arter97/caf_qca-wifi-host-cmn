/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * DOC: contains MLO manager util api's
 */
#include <wlan_cmn.h>
#include <wlan_mlo_mgr_sta.h>
#include <wlan_cm_public_struct.h>
#include <wlan_mlo_mgr_main.h>
#include <wlan_cm_api.h>
#include "wlan_scan_api.h"
#include "qdf_types.h"
#include "utils_mlo.h"
#include "wlan_mlo_mgr_cmn.h"
#include "wlan_utility.h"

#ifdef WLAN_FEATURE_11BE_MLO

static uint8_t *util_find_eid(uint8_t eid, uint8_t *frame, qdf_size_t len)
{
	if (!frame)
		return NULL;

	while (len >= MIN_IE_LEN && len >= frame[TAG_LEN_POS] + MIN_IE_LEN) {
		if (frame[ID_POS] == eid)
			return frame;

		len -= frame[TAG_LEN_POS] + MIN_IE_LEN;
		frame += frame[TAG_LEN_POS] + MIN_IE_LEN;
	}

	return NULL;
}

static
uint8_t *util_find_extn_eid(uint8_t eid, uint8_t extn_eid,
			    uint8_t *frame, qdf_size_t len)
{
	if (!frame)
		return NULL;

	while (len > MIN_IE_LEN && len >= frame[TAG_LEN_POS] + MIN_IE_LEN) {
		if ((frame[ID_POS] == eid) &&
		    (frame[ELEM_ID_EXTN_POS] == extn_eid))
			return frame;

		len -= frame[TAG_LEN_POS] + MIN_IE_LEN;
		frame += frame[TAG_LEN_POS] + MIN_IE_LEN;
	}
	return NULL;
}

static
uint8_t *util_parse_multi_link_ctrl_ie(uint8_t *subelement,
				       qdf_size_t len,
				       qdf_size_t *link_info_ie_len)
{
	qdf_size_t sub_ie_len = 0;

	if (!subelement || !len)
		return NULL;

	/* sta prof len = ML IE len + EID extn(1) + Multi Lnk ctrl(2) */
	sub_ie_len += TAG_LEN_POS + MIN_IE_LEN + 2;

	/* check if MLD MAC address present */
	if (qdf_test_bit(WLAN_ML_CTRL_PBM_IDX |
			 WLAN_ML_BV_CTRL_PBM_MLDMACADDR_P,
			 (unsigned long *)&subelement[MULTI_LINK_CTRL_1]))
		sub_ie_len = sub_ie_len + QDF_MAC_ADDR_SIZE;

	/* check if Link ID info */
	if (qdf_test_bit(WLAN_ML_CTRL_PBM_IDX |
			 WLAN_ML_BV_CTRL_PBM_LINKIDINFO_P,
			 (unsigned long *)&subelement[MULTI_LINK_CTRL_1]))
		sub_ie_len = sub_ie_len + TAG_LEN_POS;

	/* check if BSS parameter change count */
	if (qdf_test_bit(WLAN_ML_CTRL_PBM_IDX |
			 WLAN_ML_BV_CTRL_PBM_BSSPARAMCHANGECNT_P,
			 (unsigned long *)&subelement[MULTI_LINK_CTRL_1]))
		sub_ie_len = sub_ie_len + TAG_LEN_POS;

	/* check if Medium Sync Delay Info present */
	if (qdf_test_bit(WLAN_ML_CTRL_PBM_IDX |
			 WLAN_ML_BV_CTRL_PBM_MEDIUMSYNCDELAYINFO_P,
			 (unsigned long *)&subelement[MULTI_LINK_CTRL_1]))
		sub_ie_len = sub_ie_len + PAYLOAD_START_POS;

	/* check if EML cap present */
	if (qdf_test_bit(WLAN_ML_CTRL_PBM_IDX |
			 WLAN_ML_BV_CTRL_PBM_EMLCAP_P,
			 (unsigned long *)&subelement[MULTI_LINK_CTRL_2]))
		sub_ie_len = sub_ie_len + SUBELEMENT_START_POS;

	/* check if MLD cap present */
	if (qdf_test_bit(WLAN_ML_CTRL_PBM_IDX |
			 WLAN_ML_BV_CTRL_PBM_MLDCAP_P,
			 (unsigned long *)&subelement[MULTI_LINK_CTRL_2]))
		sub_ie_len = sub_ie_len + PAYLOAD_START_POS;

	if (link_info_ie_len) {
		*link_info_ie_len = len - sub_ie_len;
		mlo_debug("link_info_ie_len:%zu, sub_ie_len:%zu",
			  *link_info_ie_len, sub_ie_len);
	}

	return &subelement[sub_ie_len];
}

static
uint8_t *util_parse_sta_profile_ie(uint8_t *subelement,
				   qdf_size_t len,
				   qdf_size_t *per_sta_prof_ie_len,
				   struct qdf_mac_addr *bssid)
{
	qdf_size_t tmp_len = 0;
	uint8_t *tmp = NULL;

	if (!subelement || !len)
		return NULL;

	if (subelement[0] == 0)
		tmp = subelement;
	if (!tmp)
		return NULL;

	/* tmp_len = 2 (sta ctrl) + 1 (sub EID) + 1 (len) */
	tmp_len = PAYLOAD_START_POS + 2;

	/* check DTIM info present bit */
	if (qdf_test_bit(WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_DTIMINFOP_IDX,
			 (unsigned long *)&subelement[STA_CTRL_1]))
		tmp_len = tmp_len + MIN_IE_LEN;

	/* check Beacon interval present bit */
	if (qdf_test_bit(WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_BCNINTP_IDX,
			 (unsigned long *)&subelement[STA_CTRL_1]))
		tmp_len = tmp_len + TAG_LEN_POS;

	/* check STA link mac addr info present bit */
	if (qdf_test_bit(WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_MACADDRP_IDX,
			 (unsigned long *)&subelement[STA_CTRL_1])) {
		tmp_len = tmp_len + QDF_MAC_ADDR_SIZE;
		mlo_debug("copied mac addr: " QDF_MAC_ADDR_FMT, &tmp[4]);
		qdf_mem_copy(&bssid->bytes, &tmp[4], QDF_MAC_ADDR_SIZE);
	}

	/* Add Link ID offset,as it will always be present in assoc rsp mlo ie */
	tmp_len = tmp_len + TAG_LEN_POS;

	/* check NTSR Link pair present bit */
	if (qdf_test_bit(WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRLINKPRP_IDX % 8,
			 (unsigned long *)&subelement[STA_CTRL_2]))
		tmp_len = tmp_len + MIN_IE_LEN;

	/* check NTSR bitmap size present bit */
	if (qdf_test_bit(WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_IDX % 8,
			 (unsigned long *)&subelement[STA_CTRL_2]))
		tmp_len = tmp_len + TAG_LEN_POS;

	if (len <= tmp_len) {
		mlo_err("len %zu <= tmp_len %zu, return", len, tmp_len);
		return NULL;
	}
	*per_sta_prof_ie_len = len - tmp_len;

	return &tmp[tmp_len];
}

static
uint8_t *util_get_successorfrag(uint8_t *currie, uint8_t *frame, qdf_size_t len)
{
	uint8_t *nextie;

	if (!currie || !frame || !len)
		return NULL;

	if ((currie + MIN_IE_LEN) > (frame + len))
		return NULL;

	/* Check whether there is sufficient space in the frame for the current
	 * IE, plus at least another MIN_IE_LEN bytes for the IE header of a
	 * fragment (if present) that would come just after the current IE.
	 */
	if ((currie + MIN_IE_LEN + currie[TAG_LEN_POS] + MIN_IE_LEN) >
			(frame + len))
		return NULL;

	nextie = currie + currie[TAG_LEN_POS] + MIN_IE_LEN;

	if (nextie[ID_POS] != WLAN_ELEMID_FRAGMENT)
		return NULL;

	return nextie;
}

QDF_STATUS util_gen_link_assoc_rsp(uint8_t *frame, qdf_size_t len,
				   struct qdf_mac_addr link_addr,
				   uint8_t *assoc_link_frame)
{
	uint8_t *tmp = NULL;
	const uint8_t *tmp_old, *rsn_ie;
	qdf_size_t sub_len, tmp_rem_len;
	qdf_size_t link_info_len, per_sta_prof_len = 0;
	uint8_t *subelement;
	uint8_t *pos;
	uint8_t *sub_copy, *orig_copy;
	struct qdf_mac_addr bssid;
	struct wlan_frame_hdr *hdr;

	if (!frame || !len)
		return QDF_STATUS_E_NULL_VALUE;

	pos = assoc_link_frame;
	hdr = (struct wlan_frame_hdr *)pos;
	pos = pos + WLAN_MAC_HDR_LEN_3A;

	/* Assoc resp Capability(2) + AID(2) + Status Code(2) */
	qdf_mem_copy(pos, frame, WLAN_ASSOC_RSP_IES_OFFSET);
	pos = pos + WLAN_ASSOC_RSP_IES_OFFSET;

	rsn_ie = wlan_get_ie_ptr_from_eid(WLAN_ELEMID_RSN, frame, len);
	if (rsn_ie) {
		qdf_mem_copy(pos, rsn_ie, rsn_ie[1]);
		pos = pos + rsn_ie[1];
	}
	/* find MLO IE */
	subelement = util_find_extn_eid(WLAN_ELEMID_EXTN_ELEM,
					WLAN_EXTN_ELEMID_MULTI_LINK,
					frame,
					len);
	if (!subelement)
		return QDF_STATUS_E_FAILURE;

	/*  EID(1) + len (1) = 2 */
	sub_len = subelement[TAG_LEN_POS] + 2;
	sub_copy = qdf_mem_malloc(sub_len);
	if (!sub_copy)
		return QDF_STATUS_E_NOMEM;
	orig_copy = sub_copy;
	qdf_mem_copy(sub_copy, subelement, sub_len);

	/* parse ml ie */
	sub_copy = util_parse_multi_link_ctrl_ie(sub_copy,
						 sub_len,
						 &link_info_len);

	if (!sub_copy)
		return QDF_STATUS_E_NULL_VALUE;

	mlo_debug("dumping hex after parsing multi link ctrl");
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_MLO, QDF_TRACE_LEVEL_DEBUG,
			   sub_copy, link_info_len);

	/* parse sta profile ie */
	sub_copy = util_parse_sta_profile_ie(sub_copy,
					     link_info_len,
					     &per_sta_prof_len,
					     &bssid);

	if (!sub_copy) {
		qdf_mem_copy(pos, frame, len);
		pos += len - WLAN_MAC_HDR_LEN_3A;
		goto update_header;
	}

	/* go through IEs in frame and subelement,
	 * merge them into new_ie
	 */
	tmp_old = util_find_eid(WLAN_ELEMID_SSID, frame, len);
	tmp_old = (tmp_old) ? tmp_old + tmp_old[TAG_LEN_POS] + MIN_IE_LEN : frame;

	while (((tmp_old + tmp_old[TAG_LEN_POS] + MIN_IE_LEN) - frame) <= len) {
		tmp = (uint8_t *)util_find_eid(tmp_old[0],
					       sub_copy,
					       per_sta_prof_len);
		if (!tmp) {
			/* ie in old ie but not in subelement */
			if (tmp_old[2] != WLAN_EXTN_ELEMID_MULTI_LINK) {
				if ((pos + tmp_old[TAG_LEN_POS] + MIN_IE_LEN) <=
					(assoc_link_frame + len)) {
					qdf_mem_copy(pos, tmp_old,
						     (tmp_old[TAG_LEN_POS] +
						     MIN_IE_LEN));
					pos += tmp_old[TAG_LEN_POS] + MIN_IE_LEN;
				}
			}
		} else {
			/* ie in transmitting ie also in subelement,
			 * copy from subelement and flag the ie in subelement
			 * as copied (by setting eid field to 0xff). For
			 * vendor ie, compare OUI + type + subType to
			 * determine if they are the same ie.
			 */
			tmp_rem_len = per_sta_prof_len - (tmp - sub_copy);
			if (tmp_old[0] == WLAN_ELEMID_VENDOR &&
			    tmp_rem_len >= MIN_VENDOR_TAG_LEN) {
				if (!qdf_mem_cmp(tmp_old + PAYLOAD_START_POS,
						 tmp + PAYLOAD_START_POS,
						 OUI_LEN)) {
					/* same vendor ie, copy from
					 * subelement
					 */
					if ((pos + tmp[TAG_LEN_POS] + MIN_IE_LEN) <=
						(assoc_link_frame + len)) {
						qdf_mem_copy(pos, tmp,
							     tmp[TAG_LEN_POS] +
							     MIN_IE_LEN);
						pos += tmp[TAG_LEN_POS] + MIN_IE_LEN;
						tmp[0] = 0;
					}
				} else {
					if ((pos + tmp_old[TAG_LEN_POS] +
						 MIN_IE_LEN) <=
						(assoc_link_frame + len)) {
						qdf_mem_copy(pos, tmp_old,
							     tmp_old[TAG_LEN_POS] +
							     MIN_IE_LEN);
						pos += tmp_old[TAG_LEN_POS] +
							MIN_IE_LEN;
					}
				}
			} else if (tmp_old[0] == WLAN_ELEMID_EXTN_ELEM) {
				if (tmp_old[PAYLOAD_START_POS] ==
					tmp[PAYLOAD_START_POS]) {
					/* same ie, copy from subelement */
					if ((pos + tmp[TAG_LEN_POS] + MIN_IE_LEN) <=
						(assoc_link_frame + len)) {
						qdf_mem_copy(pos, tmp,
							     tmp[TAG_LEN_POS] +
							     MIN_IE_LEN);
						pos += tmp[TAG_LEN_POS] + MIN_IE_LEN;
						tmp[0] = 0;
					}
				} else {
					if ((pos + tmp_old[TAG_LEN_POS] + MIN_IE_LEN) <=
						(assoc_link_frame + len)) {
						qdf_mem_copy(pos, tmp_old,
							     tmp_old[TAG_LEN_POS] +
							     MIN_IE_LEN);
						pos += tmp_old[TAG_LEN_POS] +
							MIN_IE_LEN;
					}
				}
			} else {
				/* copy ie from subelement into new ie */
				if ((pos + tmp[TAG_LEN_POS] + MIN_IE_LEN) <=
					(assoc_link_frame + len)) {
					qdf_mem_copy(pos, tmp,
						     tmp[TAG_LEN_POS] + MIN_IE_LEN);
					pos += tmp[TAG_LEN_POS] + MIN_IE_LEN;
					tmp[0] = 0;
				}
			}
		}

		if (((tmp_old + tmp_old[TAG_LEN_POS] + MIN_IE_LEN) - frame) >= len)
			break;

		tmp_old += tmp_old[TAG_LEN_POS] + MIN_IE_LEN;
	}

update_header:
	/* Copy the link mac addr */
	qdf_mem_copy(hdr->i_addr3, bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(hdr->i_addr2, bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(hdr->i_addr1, &link_addr,
		     QDF_MAC_ADDR_SIZE);
	hdr->i_fc[0] = FC0_IEEE_MGMT_FRM;
	hdr->i_fc[1] = FC1_IEEE_MGMT_FRM;
	/* seq num not used so not populated */
	qdf_mem_free(orig_copy);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_find_mlie(uint8_t *buf, qdf_size_t buflen, uint8_t **mlieseq,
	       qdf_size_t *mlieseqlen)
{
	uint8_t *bufboundary;
	uint8_t *ieseq;
	qdf_size_t ieseqlen;
	uint8_t *currie;
	uint8_t *successorfrag;

	if (!buf || !buflen || !mlieseq || !mlieseqlen)
		return QDF_STATUS_E_NULL_VALUE;

	*mlieseq = NULL;
	*mlieseqlen = 0;

	/* Find Multi-Link element. In case a fragment sequence is present,
	 * this element will be the leading fragment.
	 */
	ieseq = util_find_extn_eid(WLAN_ELEMID_EXTN_ELEM,
				   WLAN_EXTN_ELEMID_MULTI_LINK, buf,
				   buflen);

	/* Even if the element is not found, we have successfully examined the
	 * buffer. The caller will be provided a NULL value for the starting of
	 * the Multi-Link element. Hence, we return success.
	 */
	if (!ieseq)
		return QDF_STATUS_SUCCESS;

	bufboundary = buf + buflen;

	if ((ieseq + MIN_IE_LEN) > bufboundary)
		return QDF_STATUS_E_INVAL;

	ieseqlen = MIN_IE_LEN + ieseq[TAG_LEN_POS];

	if (ieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_PROTO;

	if ((ieseq + ieseqlen) > bufboundary)
		return QDF_STATUS_E_INVAL;

	/* In the next sequence of checks, if there is no space in the buffer
	 * for another element after the Multi-Link element/element fragment
	 * sequence, it could indicate an issue since non-MLO EHT elements
	 * would be expected to follow the Multi-Link element/element fragment
	 * sequence. However, this is outside of the purview of this function,
	 * hence we ignore it.
	 */

	currie = ieseq;
	successorfrag = util_get_successorfrag(currie, buf, buflen);

	/* Fragmentation definitions as of IEEE802.11be D1.0 and
	 * IEEE802.11REVme D0.2 are applied. Only the case where Multi-Link
	 * element is present in a buffer from the core frame is considered.
	 * Future changes to fragmentation, cases where the Multi-Link element
	 * is present in a subelement, etc. to be reflected here if applicable
	 * as and when the rules evolve.
	 */
	while (successorfrag) {
		/* We should not be seeing a successor fragment if the length
		 * of the current IE is lesser than the max.
		 */
		if (currie[TAG_LEN_POS] != WLAN_MAX_IE_LEN)
			return QDF_STATUS_E_PROTO;

		if (successorfrag[TAG_LEN_POS] == 0)
			return QDF_STATUS_E_PROTO;

		ieseqlen +=  (MIN_IE_LEN + successorfrag[TAG_LEN_POS]);

		currie = successorfrag;
		successorfrag = util_get_successorfrag(currie, buf, buflen);
	}

	*mlieseq = ieseq;
	*mlieseqlen = ieseqlen;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_mlie_variant(uint8_t *mlieseq, qdf_size_t mlieseqlen,
		      int *variant)
{
	struct wlan_ie_multilink *mlie_fixed;
	enum wlan_ml_variant var;
	uint16_t mlcontrol;

	if (!mlieseq || !mlieseqlen || !variant)
		return QDF_STATUS_E_NULL_VALUE;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if ((mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM) ||
	    (mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK))
		return QDF_STATUS_E_INVAL;

	mlcontrol = le16toh(mlie_fixed->mlcontrol);
	var = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			   WLAN_ML_CTRL_TYPE_BITS);

	if (var >= WLAN_ML_VARIANT_INVALIDSTART)
		return QDF_STATUS_E_PROTO;

	*variant = var;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_bvmlie_mldmacaddr(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			   bool *mldmacaddrfound,
			   struct qdf_mac_addr *mldmacaddr)
{
	struct wlan_ie_multilink *mlie_fixed;
	enum wlan_ml_variant variant;
	uint16_t mlcontrol;
	uint16_t presencebitmap;

	if (!mlieseq || !mlieseqlen || !mldmacaddrfound || !mldmacaddr)
		return QDF_STATUS_E_NULL_VALUE;

	*mldmacaddrfound = false;
	qdf_mem_zero(mldmacaddr, sizeof(*mldmacaddr));

	if (mlieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if ((mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM) ||
	    (mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK))
		return QDF_STATUS_E_INVAL;

	mlcontrol = le16toh(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_BASIC)
		return QDF_STATUS_E_INVAL;

	presencebitmap = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				      WLAN_ML_CTRL_PBM_BITS);

	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_MLDMACADDR_P) {
		/* Common Info starts at mlieseq + sizeof(struct
		 * wlan_ie_multilink). Check if there is sufficient space in
		 * Common Info for the MLD MAC address.
		 */
		if ((sizeof(struct wlan_ie_multilink) + QDF_MAC_ADDR_SIZE) >
				mlieseqlen)
			return QDF_STATUS_E_PROTO;

		*mldmacaddrfound = true;
		qdf_mem_copy(mldmacaddr->bytes,
			     mlieseq + sizeof(struct wlan_ie_multilink),
			     QDF_MAC_ADDR_SIZE);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_bvmlie_primary_linkid(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			       bool *linkidfound, uint8_t *linkid)
{
	struct wlan_ie_multilink *mlie_fixed;
	enum wlan_ml_variant variant;
	uint16_t mlcontrol;
	uint16_t presencebitmap;
	uint8_t *commoninfo;
	qdf_size_t commoninfolen;
	uint8_t *linkidinfo;

	if (!mlieseq || !mlieseqlen || !linkidfound || !linkid)
		return QDF_STATUS_E_NULL_VALUE;

	*linkidfound = false;
	*linkid = 0;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if ((mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM) ||
	    (mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK))
		return QDF_STATUS_E_INVAL;

	mlcontrol = le16toh(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_BASIC)
		return QDF_STATUS_E_INVAL;

	presencebitmap = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				      WLAN_ML_CTRL_PBM_BITS);

	commoninfo = mlieseq + sizeof(struct wlan_ie_multilink);
	commoninfolen = 0;

	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_MLDMACADDR_P) {
		commoninfolen += QDF_MAC_ADDR_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + commoninfolen) >
				mlieseqlen)
			return QDF_STATUS_E_PROTO;
	}

	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_LINKIDINFO_P) {
		linkidinfo = commoninfo + commoninfolen;
		commoninfolen += WLAN_ML_BV_CINFO_LINKIDINFO_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + commoninfolen) >
				mlieseqlen)
			return QDF_STATUS_E_PROTO;

		*linkidfound = true;
		*linkid = QDF_GET_BITS(linkidinfo[0],
				       WLAN_ML_BV_CINFO_LINKIDINFO_LINKID_IDX,
				       WLAN_ML_BV_CINFO_LINKIDINFO_LINKID_BITS);
	}

	return QDF_STATUS_SUCCESS;
}
#endif
