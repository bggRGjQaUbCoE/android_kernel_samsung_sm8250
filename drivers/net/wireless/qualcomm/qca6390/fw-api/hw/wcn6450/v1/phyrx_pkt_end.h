
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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



#ifndef _PHYRX_PKT_END_H_
#define _PHYRX_PKT_END_H_
#if !defined(__ASSEMBLER__)
#endif

#include "phyrx_pkt_end_info.h"

#define NUM_OF_DWORDS_PHYRX_PKT_END 33

struct phyrx_pkt_end {
    struct            phyrx_pkt_end_info                       rx_pkt_end_details;
};

#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_LOCATION_INFO_VALID_OFFSET 0x00000000
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_LOCATION_INFO_VALID_LSB   1
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_LOCATION_INFO_VALID_MASK  0x00000002

#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_TIMING_INFO_VALID_OFFSET  0x00000000
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_TIMING_INFO_VALID_LSB     2
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_TIMING_INFO_VALID_MASK    0x00000004

#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_RSSI_INFO_VALID_OFFSET    0x00000000
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_RSSI_INFO_VALID_LSB       3
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_RSSI_INFO_VALID_MASK      0x00000008

#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_RX_FRAME_CORRECTION_NEEDED_OFFSET 0x00000000
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_RX_FRAME_CORRECTION_NEEDED_LSB 4
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_RX_FRAME_CORRECTION_NEEDED_MASK 0x00000010

#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_FRAMELESS_FRAME_RECEIVED_OFFSET 0x00000000
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_FRAMELESS_FRAME_RECEIVED_LSB 5
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_FRAMELESS_FRAME_RECEIVED_MASK 0x00000020

#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_RESERVED_0A_OFFSET        0x00000000
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_RESERVED_0A_LSB           6
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_RESERVED_0A_MASK          0x00000fc0

#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_DL_OFDMA_INFO_VALID_OFFSET 0x00000000
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_DL_OFDMA_INFO_VALID_LSB   12
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_DL_OFDMA_INFO_VALID_MASK  0x00001000

#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_DL_OFDMA_RU_START_INDEX_OFFSET 0x00000000
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_DL_OFDMA_RU_START_INDEX_LSB 13
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_DL_OFDMA_RU_START_INDEX_MASK 0x000fe000

#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_DL_OFDMA_RU_WIDTH_OFFSET  0x00000000
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_DL_OFDMA_RU_WIDTH_LSB     20
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_DL_OFDMA_RU_WIDTH_MASK    0x07f00000

#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_RESERVED_0B_OFFSET        0x00000000
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_RESERVED_0B_LSB           27
#define PHYRX_PKT_END_0_RX_PKT_END_DETAILS_RESERVED_0B_MASK          0xf8000000

#define PHYRX_PKT_END_1_RX_PKT_END_DETAILS_PHY_TIMESTAMP_1_LOWER_32_OFFSET 0x00000004
#define PHYRX_PKT_END_1_RX_PKT_END_DETAILS_PHY_TIMESTAMP_1_LOWER_32_LSB 0
#define PHYRX_PKT_END_1_RX_PKT_END_DETAILS_PHY_TIMESTAMP_1_LOWER_32_MASK 0xffffffff

#define PHYRX_PKT_END_2_RX_PKT_END_DETAILS_PHY_TIMESTAMP_1_UPPER_32_OFFSET 0x00000008
#define PHYRX_PKT_END_2_RX_PKT_END_DETAILS_PHY_TIMESTAMP_1_UPPER_32_LSB 0
#define PHYRX_PKT_END_2_RX_PKT_END_DETAILS_PHY_TIMESTAMP_1_UPPER_32_MASK 0xffffffff

#define PHYRX_PKT_END_3_RX_PKT_END_DETAILS_PHY_TIMESTAMP_2_LOWER_32_OFFSET 0x0000000c
#define PHYRX_PKT_END_3_RX_PKT_END_DETAILS_PHY_TIMESTAMP_2_LOWER_32_LSB 0
#define PHYRX_PKT_END_3_RX_PKT_END_DETAILS_PHY_TIMESTAMP_2_LOWER_32_MASK 0xffffffff

#define PHYRX_PKT_END_4_RX_PKT_END_DETAILS_PHY_TIMESTAMP_2_UPPER_32_OFFSET 0x00000010
#define PHYRX_PKT_END_4_RX_PKT_END_DETAILS_PHY_TIMESTAMP_2_UPPER_32_LSB 0
#define PHYRX_PKT_END_4_RX_PKT_END_DETAILS_PHY_TIMESTAMP_2_UPPER_32_MASK 0xffffffff

#define PHYRX_PKT_END_5_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_LEGACY_OFFSET 0x00000014
#define PHYRX_PKT_END_5_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_LEGACY_LSB 0
#define PHYRX_PKT_END_5_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_LEGACY_MASK 0x0000ffff

#define PHYRX_PKT_END_5_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_LEGACY_EXT80_OFFSET 0x00000014
#define PHYRX_PKT_END_5_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_LEGACY_EXT80_LSB 16
#define PHYRX_PKT_END_5_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_LEGACY_EXT80_MASK 0xffff0000

#define PHYRX_PKT_END_6_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_VHT_OFFSET 0x00000018
#define PHYRX_PKT_END_6_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_VHT_LSB 0
#define PHYRX_PKT_END_6_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_VHT_MASK 0x0000ffff

#define PHYRX_PKT_END_6_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_VHT_EXT80_OFFSET 0x00000018
#define PHYRX_PKT_END_6_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_VHT_EXT80_LSB 16
#define PHYRX_PKT_END_6_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_VHT_EXT80_MASK 0xffff0000

#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_LEGACY_STATUS_OFFSET 0x0000001c
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_LEGACY_STATUS_LSB 0
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_LEGACY_STATUS_MASK 0x00000001

#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_LEGACY_EXT80_STATUS_OFFSET 0x0000001c
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_LEGACY_EXT80_STATUS_LSB 1
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_LEGACY_EXT80_STATUS_MASK 0x00000002

#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_VHT_STATUS_OFFSET 0x0000001c
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_VHT_STATUS_LSB 2
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_VHT_STATUS_MASK 0x00000004

#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_VHT_EXT80_STATUS_OFFSET 0x0000001c
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_VHT_EXT80_STATUS_LSB 3
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_VHT_EXT80_STATUS_MASK 0x00000008

#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_SIFS_OFFSET 0x0000001c
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_SIFS_LSB 4
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_SIFS_MASK 0x0000fff0

#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_SIFS_STATUS_OFFSET 0x0000001c
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_SIFS_STATUS_LSB 16
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_FAC_SIFS_STATUS_MASK 0x00030000

#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CFR_STATUS_OFFSET 0x0000001c
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CFR_STATUS_LSB 18
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CFR_STATUS_MASK 0x00040000

#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CIR_STATUS_OFFSET 0x0000001c
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CIR_STATUS_LSB 19
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CIR_STATUS_MASK 0x00080000

#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CHANNEL_DUMP_SIZE_OFFSET 0x0000001c
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CHANNEL_DUMP_SIZE_LSB 20
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CHANNEL_DUMP_SIZE_MASK 0x7ff00000

#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_HW_IFFT_MODE_OFFSET 0x0000001c
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_HW_IFFT_MODE_LSB 31
#define PHYRX_PKT_END_7_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_HW_IFFT_MODE_MASK 0x80000000

#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_BTCF_STATUS_OFFSET 0x00000020
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_BTCF_STATUS_LSB 0
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_BTCF_STATUS_MASK 0x00000001

#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_PREAMBLE_TYPE_OFFSET 0x00000020
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_PREAMBLE_TYPE_LSB 1
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_PREAMBLE_TYPE_MASK 0x0000003e

#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_PKT_BW_LEG_OFFSET 0x00000020
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_PKT_BW_LEG_LSB 6
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_PKT_BW_LEG_MASK 0x000000c0

#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_PKT_BW_VHT_OFFSET 0x00000020
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_PKT_BW_VHT_LSB 8
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_PKT_BW_VHT_MASK 0x00000300

#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_GI_TYPE_OFFSET 0x00000020
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_GI_TYPE_LSB 10
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_GI_TYPE_MASK 0x00000c00

#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_MCS_RATE_OFFSET 0x00000020
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_MCS_RATE_LSB 12
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_MCS_RATE_MASK 0x0001f000

#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_STRONGEST_CHAIN_OFFSET 0x00000020
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_STRONGEST_CHAIN_LSB 17
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_STRONGEST_CHAIN_MASK 0x000e0000

#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_STRONGEST_CHAIN_EXT80_OFFSET 0x00000020
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_STRONGEST_CHAIN_EXT80_LSB 20
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_STRONGEST_CHAIN_EXT80_MASK 0x00700000

#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_RX_CHAIN_MASK_OFFSET 0x00000020
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_RX_CHAIN_MASK_LSB 23
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_RX_CHAIN_MASK_MASK 0x7f800000

#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RESERVED_3_OFFSET 0x00000020
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RESERVED_3_LSB 31
#define PHYRX_PKT_END_8_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RESERVED_3_MASK 0x80000000

#define PHYRX_PKT_END_9_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RX_START_TS_OFFSET 0x00000024
#define PHYRX_PKT_END_9_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RX_START_TS_LSB 0
#define PHYRX_PKT_END_9_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RX_START_TS_MASK 0xffffffff

#define PHYRX_PKT_END_10_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RX_END_TS_OFFSET 0x00000028
#define PHYRX_PKT_END_10_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RX_END_TS_LSB 0
#define PHYRX_PKT_END_10_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RX_END_TS_MASK 0xffffffff

#define PHYRX_PKT_END_11_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_SFO_PHASE_PKT_START_OFFSET 0x0000002c
#define PHYRX_PKT_END_11_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_SFO_PHASE_PKT_START_LSB 0
#define PHYRX_PKT_END_11_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_SFO_PHASE_PKT_START_MASK 0x00000fff

#define PHYRX_PKT_END_11_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_SFO_PHASE_PKT_END_OFFSET 0x0000002c
#define PHYRX_PKT_END_11_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_SFO_PHASE_PKT_END_LSB 12
#define PHYRX_PKT_END_11_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_SFO_PHASE_PKT_END_MASK 0x00fff000

#define PHYRX_PKT_END_11_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CHE_BUFFER_POINTER_HIGH8_OFFSET 0x0000002c
#define PHYRX_PKT_END_11_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CHE_BUFFER_POINTER_HIGH8_LSB 24
#define PHYRX_PKT_END_11_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CHE_BUFFER_POINTER_HIGH8_MASK 0xff000000

#define PHYRX_PKT_END_12_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CHE_BUFFER_POINTER_LOW32_OFFSET 0x00000030
#define PHYRX_PKT_END_12_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CHE_BUFFER_POINTER_LOW32_LSB 0
#define PHYRX_PKT_END_12_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CHE_BUFFER_POINTER_LOW32_MASK 0xffffffff

#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CFO_MEASUREMENT_OFFSET 0x00000034
#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CFO_MEASUREMENT_LSB 0
#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CFO_MEASUREMENT_MASK 0x00003fff

#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CHAN_SPREAD_OFFSET 0x00000034
#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CHAN_SPREAD_LSB 14
#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_CHAN_SPREAD_MASK 0x003fc000

#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_TIMING_BACKOFF_SEL_OFFSET 0x00000034
#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_TIMING_BACKOFF_SEL_LSB 22
#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RTT_TIMING_BACKOFF_SEL_MASK 0x00c00000

#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RESERVED_8_OFFSET 0x00000034
#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RESERVED_8_LSB 24
#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RESERVED_8_MASK 0x7f000000

#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RX_LOCATION_INFO_VALID_OFFSET 0x00000034
#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RX_LOCATION_INFO_VALID_LSB 31
#define PHYRX_PKT_END_13_RX_PKT_END_DETAILS_RX_LOCATION_INFO_DETAILS_RX_LOCATION_INFO_VALID_MASK 0x80000000

#define PHYRX_PKT_END_14_RX_PKT_END_DETAILS_RX_TIMING_OFFSET_INFO_DETAILS_RESIDUAL_PHASE_OFFSET_OFFSET 0x00000038
#define PHYRX_PKT_END_14_RX_PKT_END_DETAILS_RX_TIMING_OFFSET_INFO_DETAILS_RESIDUAL_PHASE_OFFSET_LSB 0
#define PHYRX_PKT_END_14_RX_PKT_END_DETAILS_RX_TIMING_OFFSET_INFO_DETAILS_RESIDUAL_PHASE_OFFSET_MASK 0x00000fff

#define PHYRX_PKT_END_14_RX_PKT_END_DETAILS_RX_TIMING_OFFSET_INFO_DETAILS_RESERVED_OFFSET 0x00000038
#define PHYRX_PKT_END_14_RX_PKT_END_DETAILS_RX_TIMING_OFFSET_INFO_DETAILS_RESERVED_LSB 12
#define PHYRX_PKT_END_14_RX_PKT_END_DETAILS_RX_TIMING_OFFSET_INFO_DETAILS_RESERVED_MASK 0xfffff000

#define PHYRX_PKT_END_15_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN0_OFFSET 0x0000003c
#define PHYRX_PKT_END_15_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN0_LSB 0
#define PHYRX_PKT_END_15_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN0_MASK 0x000000ff

#define PHYRX_PKT_END_15_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN0_OFFSET 0x0000003c
#define PHYRX_PKT_END_15_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN0_LSB 8
#define PHYRX_PKT_END_15_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN0_MASK 0x0000ff00

#define PHYRX_PKT_END_15_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN0_OFFSET 0x0000003c
#define PHYRX_PKT_END_15_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN0_LSB 16
#define PHYRX_PKT_END_15_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN0_MASK 0x00ff0000

#define PHYRX_PKT_END_15_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN0_OFFSET 0x0000003c
#define PHYRX_PKT_END_15_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN0_LSB 24
#define PHYRX_PKT_END_15_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN0_MASK 0xff000000

#define PHYRX_PKT_END_16_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN0_OFFSET 0x00000040
#define PHYRX_PKT_END_16_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN0_LSB 0
#define PHYRX_PKT_END_16_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN0_MASK 0x000000ff

#define PHYRX_PKT_END_16_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN0_OFFSET 0x00000040
#define PHYRX_PKT_END_16_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN0_LSB 8
#define PHYRX_PKT_END_16_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN0_MASK 0x0000ff00

#define PHYRX_PKT_END_16_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN0_OFFSET 0x00000040
#define PHYRX_PKT_END_16_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN0_LSB 16
#define PHYRX_PKT_END_16_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN0_MASK 0x00ff0000

#define PHYRX_PKT_END_16_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN0_OFFSET 0x00000040
#define PHYRX_PKT_END_16_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN0_LSB 24
#define PHYRX_PKT_END_16_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN0_MASK 0xff000000

#define PHYRX_PKT_END_17_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN1_OFFSET 0x00000044
#define PHYRX_PKT_END_17_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN1_LSB 0
#define PHYRX_PKT_END_17_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN1_MASK 0x000000ff

#define PHYRX_PKT_END_17_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN1_OFFSET 0x00000044
#define PHYRX_PKT_END_17_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN1_LSB 8
#define PHYRX_PKT_END_17_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN1_MASK 0x0000ff00

#define PHYRX_PKT_END_17_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN1_OFFSET 0x00000044
#define PHYRX_PKT_END_17_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN1_LSB 16
#define PHYRX_PKT_END_17_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN1_MASK 0x00ff0000

#define PHYRX_PKT_END_17_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN1_OFFSET 0x00000044
#define PHYRX_PKT_END_17_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN1_LSB 24
#define PHYRX_PKT_END_17_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN1_MASK 0xff000000

#define PHYRX_PKT_END_18_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN1_OFFSET 0x00000048
#define PHYRX_PKT_END_18_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN1_LSB 0
#define PHYRX_PKT_END_18_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN1_MASK 0x000000ff

#define PHYRX_PKT_END_18_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN1_OFFSET 0x00000048
#define PHYRX_PKT_END_18_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN1_LSB 8
#define PHYRX_PKT_END_18_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN1_MASK 0x0000ff00

#define PHYRX_PKT_END_18_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN1_OFFSET 0x00000048
#define PHYRX_PKT_END_18_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN1_LSB 16
#define PHYRX_PKT_END_18_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN1_MASK 0x00ff0000

#define PHYRX_PKT_END_18_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN1_OFFSET 0x00000048
#define PHYRX_PKT_END_18_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN1_LSB 24
#define PHYRX_PKT_END_18_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN1_MASK 0xff000000

#define PHYRX_PKT_END_19_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN2_OFFSET 0x0000004c
#define PHYRX_PKT_END_19_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN2_LSB 0
#define PHYRX_PKT_END_19_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN2_MASK 0x000000ff

#define PHYRX_PKT_END_19_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN2_OFFSET 0x0000004c
#define PHYRX_PKT_END_19_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN2_LSB 8
#define PHYRX_PKT_END_19_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN2_MASK 0x0000ff00

#define PHYRX_PKT_END_19_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN2_OFFSET 0x0000004c
#define PHYRX_PKT_END_19_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN2_LSB 16
#define PHYRX_PKT_END_19_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN2_MASK 0x00ff0000

#define PHYRX_PKT_END_19_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN2_OFFSET 0x0000004c
#define PHYRX_PKT_END_19_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN2_LSB 24
#define PHYRX_PKT_END_19_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN2_MASK 0xff000000

#define PHYRX_PKT_END_20_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN2_OFFSET 0x00000050
#define PHYRX_PKT_END_20_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN2_LSB 0
#define PHYRX_PKT_END_20_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN2_MASK 0x000000ff

#define PHYRX_PKT_END_20_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN2_OFFSET 0x00000050
#define PHYRX_PKT_END_20_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN2_LSB 8
#define PHYRX_PKT_END_20_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN2_MASK 0x0000ff00

#define PHYRX_PKT_END_20_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN2_OFFSET 0x00000050
#define PHYRX_PKT_END_20_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN2_LSB 16
#define PHYRX_PKT_END_20_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN2_MASK 0x00ff0000

#define PHYRX_PKT_END_20_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN2_OFFSET 0x00000050
#define PHYRX_PKT_END_20_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN2_LSB 24
#define PHYRX_PKT_END_20_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN2_MASK 0xff000000

#define PHYRX_PKT_END_21_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN3_OFFSET 0x00000054
#define PHYRX_PKT_END_21_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN3_LSB 0
#define PHYRX_PKT_END_21_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN3_MASK 0x000000ff

#define PHYRX_PKT_END_21_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN3_OFFSET 0x00000054
#define PHYRX_PKT_END_21_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN3_LSB 8
#define PHYRX_PKT_END_21_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN3_MASK 0x0000ff00

#define PHYRX_PKT_END_21_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN3_OFFSET 0x00000054
#define PHYRX_PKT_END_21_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN3_LSB 16
#define PHYRX_PKT_END_21_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN3_MASK 0x00ff0000

#define PHYRX_PKT_END_21_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN3_OFFSET 0x00000054
#define PHYRX_PKT_END_21_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN3_LSB 24
#define PHYRX_PKT_END_21_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN3_MASK 0xff000000

#define PHYRX_PKT_END_22_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN3_OFFSET 0x00000058
#define PHYRX_PKT_END_22_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN3_LSB 0
#define PHYRX_PKT_END_22_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN3_MASK 0x000000ff

#define PHYRX_PKT_END_22_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN3_OFFSET 0x00000058
#define PHYRX_PKT_END_22_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN3_LSB 8
#define PHYRX_PKT_END_22_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN3_MASK 0x0000ff00

#define PHYRX_PKT_END_22_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN3_OFFSET 0x00000058
#define PHYRX_PKT_END_22_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN3_LSB 16
#define PHYRX_PKT_END_22_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN3_MASK 0x00ff0000

#define PHYRX_PKT_END_22_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN3_OFFSET 0x00000058
#define PHYRX_PKT_END_22_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN3_LSB 24
#define PHYRX_PKT_END_22_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN3_MASK 0xff000000

#define PHYRX_PKT_END_23_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN4_OFFSET 0x0000005c
#define PHYRX_PKT_END_23_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN4_LSB 0
#define PHYRX_PKT_END_23_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN4_MASK 0x000000ff

#define PHYRX_PKT_END_23_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN4_OFFSET 0x0000005c
#define PHYRX_PKT_END_23_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN4_LSB 8
#define PHYRX_PKT_END_23_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN4_MASK 0x0000ff00

#define PHYRX_PKT_END_23_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN4_OFFSET 0x0000005c
#define PHYRX_PKT_END_23_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN4_LSB 16
#define PHYRX_PKT_END_23_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN4_MASK 0x00ff0000

#define PHYRX_PKT_END_23_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN4_OFFSET 0x0000005c
#define PHYRX_PKT_END_23_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN4_LSB 24
#define PHYRX_PKT_END_23_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN4_MASK 0xff000000

#define PHYRX_PKT_END_24_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN4_OFFSET 0x00000060
#define PHYRX_PKT_END_24_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN4_LSB 0
#define PHYRX_PKT_END_24_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN4_MASK 0x000000ff

#define PHYRX_PKT_END_24_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN4_OFFSET 0x00000060
#define PHYRX_PKT_END_24_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN4_LSB 8
#define PHYRX_PKT_END_24_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN4_MASK 0x0000ff00

#define PHYRX_PKT_END_24_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN4_OFFSET 0x00000060
#define PHYRX_PKT_END_24_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN4_LSB 16
#define PHYRX_PKT_END_24_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN4_MASK 0x00ff0000

#define PHYRX_PKT_END_24_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN4_OFFSET 0x00000060
#define PHYRX_PKT_END_24_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN4_LSB 24
#define PHYRX_PKT_END_24_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN4_MASK 0xff000000

#define PHYRX_PKT_END_25_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN5_OFFSET 0x00000064
#define PHYRX_PKT_END_25_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN5_LSB 0
#define PHYRX_PKT_END_25_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN5_MASK 0x000000ff

#define PHYRX_PKT_END_25_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN5_OFFSET 0x00000064
#define PHYRX_PKT_END_25_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN5_LSB 8
#define PHYRX_PKT_END_25_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN5_MASK 0x0000ff00

#define PHYRX_PKT_END_25_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN5_OFFSET 0x00000064
#define PHYRX_PKT_END_25_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN5_LSB 16
#define PHYRX_PKT_END_25_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN5_MASK 0x00ff0000

#define PHYRX_PKT_END_25_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN5_OFFSET 0x00000064
#define PHYRX_PKT_END_25_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN5_LSB 24
#define PHYRX_PKT_END_25_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN5_MASK 0xff000000

#define PHYRX_PKT_END_26_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN5_OFFSET 0x00000068
#define PHYRX_PKT_END_26_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN5_LSB 0
#define PHYRX_PKT_END_26_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN5_MASK 0x000000ff

#define PHYRX_PKT_END_26_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN5_OFFSET 0x00000068
#define PHYRX_PKT_END_26_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN5_LSB 8
#define PHYRX_PKT_END_26_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN5_MASK 0x0000ff00

#define PHYRX_PKT_END_26_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN5_OFFSET 0x00000068
#define PHYRX_PKT_END_26_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN5_LSB 16
#define PHYRX_PKT_END_26_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN5_MASK 0x00ff0000

#define PHYRX_PKT_END_26_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN5_OFFSET 0x00000068
#define PHYRX_PKT_END_26_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN5_LSB 24
#define PHYRX_PKT_END_26_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN5_MASK 0xff000000

#define PHYRX_PKT_END_27_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN6_OFFSET 0x0000006c
#define PHYRX_PKT_END_27_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN6_LSB 0
#define PHYRX_PKT_END_27_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN6_MASK 0x000000ff

#define PHYRX_PKT_END_27_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN6_OFFSET 0x0000006c
#define PHYRX_PKT_END_27_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN6_LSB 8
#define PHYRX_PKT_END_27_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN6_MASK 0x0000ff00

#define PHYRX_PKT_END_27_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN6_OFFSET 0x0000006c
#define PHYRX_PKT_END_27_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN6_LSB 16
#define PHYRX_PKT_END_27_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN6_MASK 0x00ff0000

#define PHYRX_PKT_END_27_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN6_OFFSET 0x0000006c
#define PHYRX_PKT_END_27_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN6_LSB 24
#define PHYRX_PKT_END_27_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN6_MASK 0xff000000

#define PHYRX_PKT_END_28_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN6_OFFSET 0x00000070
#define PHYRX_PKT_END_28_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN6_LSB 0
#define PHYRX_PKT_END_28_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN6_MASK 0x000000ff

#define PHYRX_PKT_END_28_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN6_OFFSET 0x00000070
#define PHYRX_PKT_END_28_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN6_LSB 8
#define PHYRX_PKT_END_28_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN6_MASK 0x0000ff00

#define PHYRX_PKT_END_28_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN6_OFFSET 0x00000070
#define PHYRX_PKT_END_28_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN6_LSB 16
#define PHYRX_PKT_END_28_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN6_MASK 0x00ff0000

#define PHYRX_PKT_END_28_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN6_OFFSET 0x00000070
#define PHYRX_PKT_END_28_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN6_LSB 24
#define PHYRX_PKT_END_28_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN6_MASK 0xff000000

#define PHYRX_PKT_END_29_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN7_OFFSET 0x00000074
#define PHYRX_PKT_END_29_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN7_LSB 0
#define PHYRX_PKT_END_29_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_PRI20_CHAIN7_MASK 0x000000ff

#define PHYRX_PKT_END_29_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN7_OFFSET 0x00000074
#define PHYRX_PKT_END_29_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN7_LSB 8
#define PHYRX_PKT_END_29_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT20_CHAIN7_MASK 0x0000ff00

#define PHYRX_PKT_END_29_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN7_OFFSET 0x00000074
#define PHYRX_PKT_END_29_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN7_LSB 16
#define PHYRX_PKT_END_29_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_LOW20_CHAIN7_MASK 0x00ff0000

#define PHYRX_PKT_END_29_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN7_OFFSET 0x00000074
#define PHYRX_PKT_END_29_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN7_LSB 24
#define PHYRX_PKT_END_29_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT40_HIGH20_CHAIN7_MASK 0xff000000

#define PHYRX_PKT_END_30_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN7_OFFSET 0x00000078
#define PHYRX_PKT_END_30_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN7_LSB 0
#define PHYRX_PKT_END_30_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW20_CHAIN7_MASK 0x000000ff

#define PHYRX_PKT_END_30_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN7_OFFSET 0x00000078
#define PHYRX_PKT_END_30_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN7_LSB 8
#define PHYRX_PKT_END_30_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_LOW_HIGH20_CHAIN7_MASK 0x0000ff00

#define PHYRX_PKT_END_30_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN7_OFFSET 0x00000078
#define PHYRX_PKT_END_30_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN7_LSB 16
#define PHYRX_PKT_END_30_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH_LOW20_CHAIN7_MASK 0x00ff0000

#define PHYRX_PKT_END_30_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN7_OFFSET 0x00000078
#define PHYRX_PKT_END_30_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN7_LSB 24
#define PHYRX_PKT_END_30_RX_PKT_END_DETAILS_POST_RSSI_INFO_DETAILS_RSSI_EXT80_HIGH20_CHAIN7_MASK 0xff000000

#define PHYRX_PKT_END_31_RX_PKT_END_DETAILS_PHY_SW_STATUS_31_0_OFFSET 0x0000007c
#define PHYRX_PKT_END_31_RX_PKT_END_DETAILS_PHY_SW_STATUS_31_0_LSB   0
#define PHYRX_PKT_END_31_RX_PKT_END_DETAILS_PHY_SW_STATUS_31_0_MASK  0xffffffff

#define PHYRX_PKT_END_32_RX_PKT_END_DETAILS_PHY_SW_STATUS_63_32_OFFSET 0x00000080
#define PHYRX_PKT_END_32_RX_PKT_END_DETAILS_PHY_SW_STATUS_63_32_LSB  0
#define PHYRX_PKT_END_32_RX_PKT_END_DETAILS_PHY_SW_STATUS_63_32_MASK 0xffffffff

#endif
