/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
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

// ################ START SUMMARY #################
//
//	Dword	Fields
//	0-32	struct phyrx_pkt_end_info rx_pkt_end_details;
//
// ################ END SUMMARY #################

#define NUM_OF_DWORDS_PHYRX_PKT_END 33

struct phyrx_pkt_end {
    struct            phyrx_pkt_end_info                       rx_pkt_end_details;
};

/*

struct phyrx_pkt_end_info rx_pkt_end_details
			
			Overview of the final receive related parameters from
			the PHY RX
*/

#define PHYRX_PKT_END_0_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000000
#define PHYRX_PKT_END_0_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB    0
#define PHYRX_PKT_END_0_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK   0xffffffff
#define PHYRX_PKT_END_1_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000004
#define PHYRX_PKT_END_1_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB    0
#define PHYRX_PKT_END_1_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK   0xffffffff
#define PHYRX_PKT_END_2_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000008
#define PHYRX_PKT_END_2_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB    0
#define PHYRX_PKT_END_2_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK   0xffffffff
#define PHYRX_PKT_END_3_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x0000000c
#define PHYRX_PKT_END_3_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB    0
#define PHYRX_PKT_END_3_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK   0xffffffff
#define PHYRX_PKT_END_4_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000010
#define PHYRX_PKT_END_4_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB    0
#define PHYRX_PKT_END_4_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK   0xffffffff
#define PHYRX_PKT_END_5_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000014
#define PHYRX_PKT_END_5_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB    0
#define PHYRX_PKT_END_5_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK   0xffffffff
#define PHYRX_PKT_END_6_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000018
#define PHYRX_PKT_END_6_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB    0
#define PHYRX_PKT_END_6_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK   0xffffffff
#define PHYRX_PKT_END_7_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x0000001c
#define PHYRX_PKT_END_7_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB    0
#define PHYRX_PKT_END_7_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK   0xffffffff
#define PHYRX_PKT_END_8_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000020
#define PHYRX_PKT_END_8_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB    0
#define PHYRX_PKT_END_8_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK   0xffffffff
#define PHYRX_PKT_END_9_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000024
#define PHYRX_PKT_END_9_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB    0
#define PHYRX_PKT_END_9_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK   0xffffffff
#define PHYRX_PKT_END_10_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000028
#define PHYRX_PKT_END_10_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_10_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_11_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x0000002c
#define PHYRX_PKT_END_11_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_11_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_12_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000030
#define PHYRX_PKT_END_12_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_12_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_13_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000034
#define PHYRX_PKT_END_13_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_13_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_14_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000038
#define PHYRX_PKT_END_14_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_14_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_15_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x0000003c
#define PHYRX_PKT_END_15_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_15_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_16_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000040
#define PHYRX_PKT_END_16_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_16_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_17_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000044
#define PHYRX_PKT_END_17_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_17_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_18_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000048
#define PHYRX_PKT_END_18_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_18_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_19_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x0000004c
#define PHYRX_PKT_END_19_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_19_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_20_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000050
#define PHYRX_PKT_END_20_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_20_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_21_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000054
#define PHYRX_PKT_END_21_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_21_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_22_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000058
#define PHYRX_PKT_END_22_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_22_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_23_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x0000005c
#define PHYRX_PKT_END_23_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_23_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_24_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000060
#define PHYRX_PKT_END_24_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_24_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_25_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000064
#define PHYRX_PKT_END_25_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_25_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_26_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000068
#define PHYRX_PKT_END_26_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_26_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_27_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x0000006c
#define PHYRX_PKT_END_27_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_27_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_28_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000070
#define PHYRX_PKT_END_28_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_28_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_29_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000074
#define PHYRX_PKT_END_29_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_29_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_30_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000078
#define PHYRX_PKT_END_30_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_30_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_31_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x0000007c
#define PHYRX_PKT_END_31_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_31_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff
#define PHYRX_PKT_END_32_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_OFFSET 0x00000080
#define PHYRX_PKT_END_32_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_LSB   0
#define PHYRX_PKT_END_32_PHYRX_PKT_END_INFO_RX_PKT_END_DETAILS_MASK  0xffffffff


#endif // _PHYRX_PKT_END_H_
