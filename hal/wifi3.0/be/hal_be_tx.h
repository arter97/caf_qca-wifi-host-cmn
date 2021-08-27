/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
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

#ifndef _HAL_BE_TX_H_
#define _HAL_BE_TX_H_

#include "hal_be_hw_headers.h"
#include "hal_tx.h"

enum hal_be_tx_ret_buf_manager {
	HAL_BE_WBM_SW0_BM_ID = 5,
	HAL_BE_WBM_SW1_BM_ID = 6,
	HAL_BE_WBM_SW2_BM_ID = 7,
	HAL_BE_WBM_SW3_BM_ID = 8,
	HAL_BE_WBM_SW4_BM_ID = 9,
	HAL_BE_WBM_SW5_BM_ID = 10,
	HAL_BE_WBM_SW6_BM_ID = 11,
};

/*---------------------------------------------------------------------------
 * Structures
 * ---------------------------------------------------------------------------
 */
/**
 * struct hal_tx_bank_config - SW config bank params
 * @epd: EPD indication flag
 * @encap_type: encapsulation type
 * @encrypt_type: encrypt type
 * @src_buffer_swap: big-endia switch for packet buffer
 * @link_meta_swap: big-endian switch for link metadata
 * @index_lookup_enable: Enabel index lookup
 * @addrx_en: Address-X search
 * @addry_en: Address-Y search
 * @mesh_enable:mesh enable flag
 * @vdev_id_check_en: vdev id check
 * @pmac_id: mac id
 * @mcast_pkt_ctrl: mulitcast packet control
 * @val: value representing bank config
 */
union hal_tx_bank_config {
	struct {
		uint32_t epd:1,
			 encap_type:2,
			 encrypt_type:4,
			 src_buffer_swap:1,
			 link_meta_swap:1,
			 index_lookup_enable:1,
			 addrx_en:1,
			 addry_en:1,
			 mesh_enable:2,
			 vdev_id_check_en:1,
			 pmac_id:2,
			 mcast_pkt_ctrl:2,
			 reserved:13;
	};
	uint32_t val;
};

/*---------------------------------------------------------------------------
 *  Function declarations and documentation
 * ---------------------------------------------------------------------------
 */

/*---------------------------------------------------------------------------
 *  TCL Descriptor accessor APIs
 *---------------------------------------------------------------------------
 */

/**
 * hal_tx_desc_set_buf_length - Set Data length in bytes in Tx Descriptor
 * @desc: Handle to Tx Descriptor
 * @data_length: MSDU length in case of direct descriptor.
 *              Length of link extension descriptor in case of Link extension
 *              descriptor.Includes the length of Metadata
 * Return: None
 */
static inline void  hal_tx_desc_set_buf_length(void *desc,
					       uint16_t data_length)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, DATA_LENGTH) |=
		HAL_TX_SM(TCL_DATA_CMD, DATA_LENGTH, data_length);
}

/**
 * hal_tx_desc_set_buf_offset - Sets Packet Offset field in Tx descriptor
 * @desc: Handle to Tx Descriptor
 * @offset: Packet offset from Metadata in case of direct buffer descriptor.
 *
 * Return: void
 */
static inline void hal_tx_desc_set_buf_offset(void *desc,
					      uint8_t offset)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, PACKET_OFFSET) |=
		HAL_TX_SM(TCL_DATA_CMD, PACKET_OFFSET, offset);
}

/**
 * hal_tx_desc_set_l4_checksum_en -  Set TCP/IP checksum enable flags
 * Tx Descriptor for MSDU_buffer type
 * @desc: Handle to Tx Descriptor
 * @en: UDP/TCP over ipv4/ipv6 checksum enable flags (5 bits)
 *
 * Return: void
 */
static inline void hal_tx_desc_set_l4_checksum_en(void *desc,
						  uint8_t en)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, IPV4_CHECKSUM_EN) |=
		(HAL_TX_SM(TCL_DATA_CMD, UDP_OVER_IPV4_CHECKSUM_EN, en) |
		 HAL_TX_SM(TCL_DATA_CMD, UDP_OVER_IPV6_CHECKSUM_EN, en) |
		 HAL_TX_SM(TCL_DATA_CMD, TCP_OVER_IPV4_CHECKSUM_EN, en) |
		 HAL_TX_SM(TCL_DATA_CMD, TCP_OVER_IPV6_CHECKSUM_EN, en));
}

/**
 * hal_tx_desc_set_l3_checksum_en -  Set IPv4 checksum enable flag in
 * Tx Descriptor for MSDU_buffer type
 * @desc: Handle to Tx Descriptor
 * @checksum_en_flags: ipv4 checksum enable flags
 *
 * Return: void
 */
static inline void hal_tx_desc_set_l3_checksum_en(void *desc,
						  uint8_t en)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, IPV4_CHECKSUM_EN) |=
		HAL_TX_SM(TCL_DATA_CMD, IPV4_CHECKSUM_EN, en);
}

/**
 * hal_tx_desc_set_fw_metadata- Sets the metadata that is part of TCL descriptor
 * @desc:Handle to Tx Descriptor
 * @metadata: Metadata to be sent to Firmware
 *
 * Return: void
 */
static inline void hal_tx_desc_set_fw_metadata(void *desc,
					       uint16_t metadata)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, TCL_CMD_NUMBER) |=
		HAL_TX_SM(TCL_DATA_CMD, TCL_CMD_NUMBER, metadata);
}

/**
 * hal_tx_desc_set_to_fw - Set To_FW bit in Tx Descriptor.
 * @desc:Handle to Tx Descriptor
 * @to_fw: if set, Forward packet to FW along with classification result
 *
 * Return: void
 */
static inline void hal_tx_desc_set_to_fw(void *desc, uint8_t to_fw)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, TO_FW) |=
		HAL_TX_SM(TCL_DATA_CMD, TO_FW, to_fw);
}

/**
 * hal_tx_desc_set_hlos_tid - Set the TID value (override DSCP/PCP fields in
 * frame) to be used for Tx Frame
 * @desc: Handle to Tx Descriptor
 * @hlos_tid: HLOS TID
 *
 * Return: void
 */
static inline void hal_tx_desc_set_hlos_tid(void *desc,
					    uint8_t hlos_tid)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, HLOS_TID) |=
		HAL_TX_SM(TCL_DATA_CMD, HLOS_TID, hlos_tid);

	HAL_SET_FLD(desc, TCL_DATA_CMD, HLOS_TID_OVERWRITE) |=
	   HAL_TX_SM(TCL_DATA_CMD, HLOS_TID_OVERWRITE, 1);
}

/**
 * hal_tx_desc_sync - Commit the descriptor to Hardware
 * @hal_tx_des_cached: Cached descriptor that software maintains
 * @hw_desc: Hardware descriptor to be updated
 */
static inline void hal_tx_desc_sync(void *hal_tx_desc_cached,
				    void *hw_desc)
{
	qdf_mem_copy(hw_desc, hal_tx_desc_cached, HAL_TX_DESC_LEN_BYTES);
}

static inline void hal_tx_desc_set_vdev_id(void *desc, uint8_t vdev_id)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, VDEV_ID) |=
		HAL_TX_SM(TCL_DATA_CMD, VDEV_ID, vdev_id);
}

static inline void hal_tx_desc_set_bank_id(void *desc, uint8_t bank_id)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, BANK_ID) |=
		HAL_TX_SM(TCL_DATA_CMD, BANK_ID, bank_id);
}

static inline void
hal_tx_desc_set_tcl_cmd_type(void *desc, uint8_t tcl_cmd_type)
{
	HAL_SET_FLD(desc, TCL_DATA_CMD, TCL_CMD_TYPE) |=
		HAL_TX_SM(TCL_DATA_CMD, TCL_CMD_TYPE, tcl_cmd_type);
}

/*---------------------------------------------------------------------------
 * WBM Descriptor accessor APIs for Tx completions
 * ---------------------------------------------------------------------------
 */

/**
 * hal_tx_get_wbm_sw0_bm_id() - Get the BM ID for first tx completion ring
 *
 * Return: BM ID for first tx completion ring
 */
static inline uint32_t hal_tx_get_wbm_sw0_bm_id(void)
{
	return HAL_BE_WBM_SW0_BM_ID;
}

/**
 * hal_tx_comp_get_desc_id() - Get TX descriptor id within comp descriptor
 * @hal_desc: completion ring descriptor pointer
 *
 * This function will tx descriptor id, cookie, within hardware completion
 * descriptor. For cases when cookie conversion is disabled, the sw_cookie
 * is present in the 2nd DWORD.
 *
 * Return: cookie
 */
static inline uint32_t hal_tx_comp_get_desc_id(void *hal_desc)
{
	uint32_t comp_desc =
		*(uint32_t *)(((uint8_t *)hal_desc) +
			       BUFFER_ADDR_INFO_SW_BUFFER_COOKIE_OFFSET);

	/* Cookie is placed on 2nd word */
	return (comp_desc & BUFFER_ADDR_INFO_SW_BUFFER_COOKIE_MASK) >>
		BUFFER_ADDR_INFO_SW_BUFFER_COOKIE_LSB;
}

/**
 * hal_tx_comp_get_paddr() - Get paddr within comp descriptor
 * @hal_desc: completion ring descriptor pointer
 *
 * This function will get buffer physical address within hardware completion
 * descriptor
 *
 * Return: Buffer physical address
 */
static inline qdf_dma_addr_t hal_tx_comp_get_paddr(void *hal_desc)
{
	uint32_t paddr_lo;
	uint32_t paddr_hi;

	paddr_lo = *(uint32_t *)(((uint8_t *)hal_desc) +
			BUFFER_ADDR_INFO_BUFFER_ADDR_31_0_OFFSET);

	paddr_hi = *(uint32_t *)(((uint8_t *)hal_desc) +
			BUFFER_ADDR_INFO_BUFFER_ADDR_39_32_OFFSET);

	paddr_hi = (paddr_hi & BUFFER_ADDR_INFO_BUFFER_ADDR_39_32_MASK) >>
		BUFFER_ADDR_INFO_BUFFER_ADDR_39_32_LSB;

	return (qdf_dma_addr_t)(paddr_lo | (((uint64_t)paddr_hi) << 32));
}

#ifdef DP_HW_COOKIE_CONVERT_EXCEPTION
/* HW set dowrd-2 bit30 to 1 if HW CC is done */
#define HAL_WBM2SW_COMPLETION_RING_TX_CC_DONE_OFFSET 0x8
#define HAL_WBM2SW_COMPLETION_RING_TX_CC_DONE_MASK 0x40000000
#define HAL_WBM2SW_COMPLETION_RING_TX_CC_DONE_LSB 0x1E
/**
 * hal_tx_comp_get_cookie_convert_done() - Get cookie conversion done flag
 * @hal_desc: completion ring descriptor pointer
 *
 * This function will get the bit value that indicate HW cookie
 * conversion done or not
 *
 * Return: 1 - HW cookie conversion done, 0 - not
 */
static inline uint8_t hal_tx_comp_get_cookie_convert_done(void *hal_desc)
{
	return HAL_TX_DESC_GET(hal_desc, HAL_WBM2SW_COMPLETION_RING_TX,
			       CC_DONE);
}
#endif

/**
 * hal_tx_comp_get_desc_va() - Get Desc virtual address within completion Desc
 * @hal_desc: completion ring descriptor pointer
 *
 * This function will get the TX Desc virtual address
 *
 * Return: TX desc virtual address
 */
static inline uintptr_t hal_tx_comp_get_desc_va(void *hal_desc)
{
	uint64_t va_from_desc;

	va_from_desc = HAL_TX_DESC_GET(hal_desc,
				       WBM2SW_COMPLETION_RING_TX,
				       BUFFER_VIRT_ADDR_31_0) |
			(((uint64_t)HAL_TX_DESC_GET(
					hal_desc,
					WBM2SW_COMPLETION_RING_TX,
					BUFFER_VIRT_ADDR_63_32)) << 32);

	return (uintptr_t)va_from_desc;
}

/*---------------------------------------------------------------------------
 * TX BANK register accessor APIs
 * ---------------------------------------------------------------------------
 */

/**
 * hal_tx_get_num_tcl_banks() - Get number of banks for target
 *
 * Return: None
 */
static inline uint8_t
hal_tx_get_num_tcl_banks(hal_soc_handle_t hal_soc_hdl)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;

	if (hal_soc->ops->hal_tx_get_num_tcl_banks)
		return hal_soc->ops->hal_tx_get_num_tcl_banks();

	return 0;
}

/**
 * hal_tx_populate_bank_register() - populate the bank register with
 *		the software configs.
 * @soc: HAL soc handle
 * @config: bank config
 * @bank_id: bank id to be configured
 *
 * Returns: None
 */
static inline void
hal_tx_populate_bank_register(hal_soc_handle_t hal_soc_hdl,
			      union hal_tx_bank_config *config,
			      uint8_t bank_id)
{
	struct hal_soc *hal_soc = (struct hal_soc *)hal_soc_hdl;
	uint32_t reg_addr, reg_val = 0;

	reg_addr = HWIO_TCL_R0_SW_CONFIG_BANK_n_ADDR(MAC_TCL_REG_REG_BASE,
						     bank_id);

	reg_val |= (config->epd << HWIO_TCL_R0_SW_CONFIG_BANK_n_EPD_SHFT);
	reg_val |= (config->encap_type <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_ENCAP_TYPE_SHFT);
	reg_val |= (config->encrypt_type <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_ENCRYPT_TYPE_SHFT);
	reg_val |= (config->src_buffer_swap <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_SRC_BUFFER_SWAP_SHFT);
	reg_val |= (config->link_meta_swap <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_LINK_META_SWAP_SHFT);
	reg_val |= (config->index_lookup_enable <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_INDEX_LOOKUP_ENABLE_SHFT);
	reg_val |= (config->addrx_en <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_ADDRX_EN_SHFT);
	reg_val |= (config->addry_en <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_ADDRY_EN_SHFT);
	reg_val |= (config->mesh_enable <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_MESH_ENABLE_SHFT);
	reg_val |= (config->vdev_id_check_en <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_VDEV_ID_CHECK_EN_SHFT);
	reg_val |= (config->pmac_id <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_PMAC_ID_SHFT);
	reg_val |= (config->mcast_pkt_ctrl <<
			HWIO_TCL_R0_SW_CONFIG_BANK_n_MCAST_PACKET_CTRL_SHFT);

	HAL_REG_WRITE(hal_soc, reg_addr, reg_val);
}

#endif /* _HAL_BE_TX_H_ */
