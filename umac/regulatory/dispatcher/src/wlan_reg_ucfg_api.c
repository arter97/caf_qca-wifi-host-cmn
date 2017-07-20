/*
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
 *
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
 * @file wlan_req_ucfg_api.c
 * @brief contains regulatory user config interface definations
 */

#include <wlan_reg_ucfg_api.h>
#include "../../core/src/reg_services.h"

QDF_STATUS ucfg_reg_register_event_handler(uint8_t vdev_id, reg_event_cb cb,
		void *arg)
{
	/* Register a event cb handler */
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_reg_unregister_event_handler(uint8_t vdev_id, reg_event_cb cb,
		void *arg)
{
	/* unregister a event cb handler */
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_reg_init_handler(uint8_t pdev_id)
{
	/* regulatory initialization handler */
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_reg_get_current_chan_list(struct wlan_objmgr_pdev *pdev,
					  struct regulatory_channel *chan_list)
{
	return reg_get_current_chan_list(pdev, chan_list);
}

QDF_STATUS ucfg_reg_set_config_vars(struct wlan_objmgr_psoc *psoc,
				 struct reg_config_vars config_vars)
{
	return reg_set_config_vars(psoc, config_vars);
}

bool ucfg_reg_is_regdb_offloaded(struct wlan_objmgr_psoc *psoc)
{
	return reg_is_regdb_offloaded(psoc);
}

void ucfg_reg_program_mas_chan_list(struct wlan_objmgr_psoc *psoc,
				    struct regulatory_channel *reg_channels,
				    uint8_t *alpha2,
				    enum dfs_reg dfs_region)
{
	reg_program_mas_chan_list(psoc, reg_channels, alpha2, dfs_region);
}

QDF_STATUS ucfg_reg_program_default_cc(struct wlan_objmgr_pdev *pdev,
				       uint16_t regdmn)
{
	return reg_program_default_cc(pdev, regdmn);
}

QDF_STATUS ucfg_reg_program_cc(struct wlan_objmgr_pdev *pdev,
			       struct cc_regdmn_s *rd)
{
	return reg_program_chan_list(pdev, rd);
}

QDF_STATUS ucfg_reg_get_current_cc(struct wlan_objmgr_pdev *pdev,
				   struct cc_regdmn_s *rd)
{
	return reg_get_current_cc(pdev, rd);
}

/**
 * ucfg_reg_set_band() - Sets the band information for the PDEV
 * @pdev: The physical pdev to set the band for
 * @band: The set band parameter to configure for the pysical device
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_set_band(struct wlan_objmgr_pdev *pdev,
			     enum band_info band)
{
	return reg_set_band(pdev, band);
}

/**
 * ucfg_reg_set_fcc_constraint() - apply fcc constraints on channels 12/13
 * @pdev: The physical pdev to reduce tx power for
 *
 * This function adjusts the transmit power on channels 12 and 13, to comply
 * with FCC regulations in the USA.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_set_fcc_constraint(struct wlan_objmgr_pdev *pdev,
				       bool fcc_constraint)
{
	return reg_set_fcc_constraint(pdev, fcc_constraint);
}


/**
 * ucfg_reg_get_default_country() - Get the default regulatory country
 * @psoc: The physical SoC to get default country from
 * @country_code: the buffer to populate the country code into
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_get_default_country(struct wlan_objmgr_psoc *psoc,
					       uint8_t *country_code)
{
	return reg_read_default_country(psoc, country_code);
}

/**
 * ucfg_reg_set_default_country() - Set the default regulatory country
 * @psoc: The physical SoC to set default country for
 * @country: The country information to configure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_set_default_country(struct wlan_objmgr_psoc *psoc,
					uint8_t *country)
{
	return reg_set_default_country(psoc, country);
}

/**
 * ucfg_reg_set_country() - Set the current regulatory country
 * @pdev: The physical dev to set current country for
 * @country: The country information to configure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_set_country(struct wlan_objmgr_pdev *pdev,
				uint8_t *country)
{
	return reg_set_country(pdev, country);
}

/**
 * ucfg_reg_reset_country() - Reset the regulatory country to default
 * @psoc: The physical SoC to reset country for
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_reset_country(struct wlan_objmgr_psoc *psoc)
{
	return reg_reset_country(psoc);
}

/**
 * ucfg_reg_enable_dfs_channels() - Enable the use of DFS channels
 * @pdev: The physical dev to enable DFS channels for
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_reg_enable_dfs_channels(struct wlan_objmgr_pdev *pdev,
					bool dfs_enable)
{
	return reg_enable_dfs_channels(pdev, dfs_enable);
}

QDF_STATUS ucfg_reg_get_curr_band(struct wlan_objmgr_pdev *pdev,
				  enum band_info *band)
{
	return reg_get_curr_band(pdev, band);

}

void ucfg_reg_register_chan_change_callback(struct wlan_objmgr_psoc *psoc,
					    reg_chan_change_callback cbk,
					    void *arg)
{
	reg_register_chan_change_callback(psoc, cbk, arg);
}

void ucfg_reg_unregister_chan_change_callback(struct wlan_objmgr_psoc *psoc,
					      reg_chan_change_callback cbk)
{
	reg_unregister_chan_change_callback(psoc, cbk);
}

enum country_src ucfg_reg_get_cc_and_src(struct wlan_objmgr_psoc *psoc,
					 uint8_t *alpha2)
{
	return reg_get_cc_and_src(psoc, alpha2);
}

QDF_STATUS ucfg_reg_11d_vdev_delete_update(struct wlan_objmgr_vdev *vdev)
{
	return reg_11d_vdev_delete_update(vdev);
}

QDF_STATUS ucfg_reg_11d_vdev_created_update(struct wlan_objmgr_vdev *vdev)
{
	return reg_11d_vdev_created_update(vdev);
}
