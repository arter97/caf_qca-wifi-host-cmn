/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <osdep.h>
#include "wmi.h"
#include "wmi_unified_priv.h"
#include "wmi_unified_wifi_radar_api.h"

static QDF_STATUS
extract_wifi_radar_cal_status_param_tlv
		(wmi_unified_t wmi_handle,
		 void *evt_buf,
		 struct wmi_wifi_radar_cal_status_param *param)
{
	WMI_PDEV_WIFI_RADAR_CAL_COMPLETION_STATUS_EVENTID_param_tlvs *ev_buf;
	wmi_pdev_wifi_radar_cal_completion_status_event_param *ev_param;

	ev_buf = (WMI_PDEV_WIFI_RADAR_CAL_COMPLETION_STATUS_EVENTID_param_tlvs *)evt_buf;
	if (!ev_buf) {
		wmi_err("Invalid wifi radar cal status event buffer");
		return QDF_STATUS_E_INVAL;
	}

	ev_param = ev_buf->cal_completion_status_event_param;

	param->pdev_id = ev_param->pdev_id;
	param->wifi_radar_pkt_bw = ev_param->wifi_radar_pkt_bw;
	param->channel_bw = ev_param->channel_bw;
	param->band_center_freq = ev_param->band_center_freq;
	param->num_ltf_tx = ev_param->num_ltf_tx;
	param->num_skip_ltf_rx = ev_param->num_skip_ltf_rx;
	param->num_ltf_accumulation = ev_param->num_ltf_accumulation;
	qdf_mem_copy(param->per_chain_cal_status,
		     ev_param->per_chain_cal_status,
		     QDF_MIN(sizeof(param->per_chain_cal_status),
			     sizeof(ev_param->per_chain_cal_status)));
	return QDF_STATUS_SUCCESS;
}

void wmi_wifi_radar_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->extract_wifi_radar_cal_status_param =
		extract_wifi_radar_cal_status_param_tlv;
}
