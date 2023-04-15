/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
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

//
// DO NOT EDIT!  This file is automatically generated
//               These definitions are tied to a particular hardware layout


#ifndef _RX_PPDU_START_USER_INFO_H_
#define _RX_PPDU_START_USER_INFO_H_
#if !defined(__ASSEMBLER__)
#endif

#include "receive_user_info.h"

// ################ START SUMMARY #################
//
//	Dword	Fields
//	0-2	struct receive_user_info receive_user_info_details;
//
// ################ END SUMMARY #################

#define NUM_OF_DWORDS_RX_PPDU_START_USER_INFO 3

struct rx_ppdu_start_user_info {
    struct            receive_user_info                       receive_user_info_details;
};

/*

struct receive_user_info receive_user_info_details
			
			Overview of receive parameters that the MAC needs to
			prepend to every received MSDU/MPDU.
*/


 /* EXTERNAL REFERENCE : struct receive_user_info receive_user_info_details */ 


/* Description		RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_PHY_PPDU_ID
			
			A ppdu counter value that PHY increments for every PPDU
			received. The counter value wraps around  
			
			<legal all>
*/
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_PHY_PPDU_ID_OFFSET 0x00000000
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_PHY_PPDU_ID_LSB 0
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_PHY_PPDU_ID_MASK 0x0000ffff

/* Description		RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_USER_RSSI
			
			RSSI for this user
			
			Frequency domain RSSI measurement for this user. Based
			on the channel estimate.  
			
			
			
			<legal all>
*/
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_USER_RSSI_OFFSET 0x00000000
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_USER_RSSI_LSB 16
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_USER_RSSI_MASK 0x00ff0000

/* Description		RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_PKT_TYPE
			
			Packet type:
			
			
			
			<enum 0 dot11a>802.11a PPDU type
			
			<enum 1 dot11b>802.11b PPDU type
			
			<enum 2 dot11n_mm>802.11n Mixed Mode PPDU type
			
			<enum 3 dot11ac>802.11ac PPDU type
			
			<enum 4 dot11ax>802.11ax PPDU type
			
			<enum 5 dot11ba>802.11ba (WUR) PPDU type
*/
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_PKT_TYPE_OFFSET 0x00000000
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_PKT_TYPE_LSB 24
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_PKT_TYPE_MASK 0x0f000000

/* Description		RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_STBC
			
			When set, use STBC transmission rates
*/
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_STBC_OFFSET 0x00000000
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_STBC_LSB 28
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_STBC_MASK 0x10000000

/* Description		RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_RECEPTION_TYPE
			
			Indicates what type of reception this is.
			
			<enum 0     reception_type_SU > Basic SU reception (not
			part of OFDMA or MU-MIMO)
			
			<enum 1     reception_type_MU_MIMO > This is related to
			DL type of reception
			
			<enum 2     reception_type_MU_OFDMA >  This is related
			to DL type of reception
			
			<enum 3     reception_type_MU_OFDMA_MIMO >  This is
			related to DL type of reception
			
			<enum 4     reception_type_UL_MU_MIMO > This is related
			to UL type of reception
			
			<enum 5     reception_type_UL_MU_OFDMA >  This is
			related to UL type of reception
			
			<enum 6     reception_type_UL_MU_OFDMA_MIMO >  This is
			related to UL type of reception
			
			
			
			<legal 0-6>
*/
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_RECEPTION_TYPE_OFFSET 0x00000000
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_RECEPTION_TYPE_LSB 29
#define RX_PPDU_START_USER_INFO_0_RECEIVE_USER_INFO_DETAILS_RECEPTION_TYPE_MASK 0xe0000000

/* Description		RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_RATE_MCS
			
			For details, refer to  MCS_TYPE description
			
			<legal all>
*/
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_RATE_MCS_OFFSET 0x00000004
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_RATE_MCS_LSB 0
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_RATE_MCS_MASK 0x0000000f

/* Description		RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_SGI
			
			Field only valid when pkt type is HT, VHT or HE.
			
			
			
			<enum 0     gi_0_8_us > Legacy normal GI.  Can also be
			used for HE
			
			<enum 1     gi_0_4_us > Legacy short GI.  Can also be
			used for HE
			
			<enum 2     gi_1_6_us > HE related GI
			
			<enum 3     gi_3_2_us > HE related GI
			
			<legal 0 - 3>
*/
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_SGI_OFFSET 0x00000004
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_SGI_LSB  4
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_SGI_MASK 0x00000030

/* Description		RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_RECEIVE_BANDWIDTH
			
			Full receive Bandwidth
			
			
			
			<enum 0     full_rx_bw_20_mhz>
			
			<enum 1      full_rx_bw_40_mhz>
			
			<enum 2      full_rx_bw_80_mhz>
			
			<enum 3      full_rx_bw_160_mhz> 
			
			
			
			<legal 0-3>
*/
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_RECEIVE_BANDWIDTH_OFFSET 0x00000004
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_RECEIVE_BANDWIDTH_LSB 6
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_RECEIVE_BANDWIDTH_MASK 0x000000c0

/* Description		RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_MIMO_SS_BITMAP
			
			Bitmap, with each bit indicating if the related spatial
			stream is used for this STA
			
			LSB related to SS 0
			
			
			
			0: spatial stream not used for this reception
			
			1: spatial stream used for this reception
			
			
			
			<legal all>
*/
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_MIMO_SS_BITMAP_OFFSET 0x00000004
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_MIMO_SS_BITMAP_LSB 8
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_MIMO_SS_BITMAP_MASK 0x0000ff00

/* Description		RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_OFDMA_RU_ALLOCATION
			
			Field only valid in case of OFDMA type receptions (DL
			and UL)
			
			
			
			Indicates the RU number associated with this user.
			
			
			
			In case of reception where the transmission was DL MU
			OFDMA, this field provides the RU pattern. Note that fields
			ofdma_user_index and ofdma_content_channel are needed to
			determine which RU (within a 40 MHz channel) was actually
			assigned to this user, but this does not give info on which
			40 MHz channel was assigned to this user. Please refer
			DL_ofdma_ru_* in PHYRX_PKT_END_INFO for complete RU info for
			this user.
			
			
			
			In case of reception where the transmission was UL MU
			OFDMA, PHY is recommended to insert the RU start index in
			this field. Note that PHY may insert the RU width in
			Reserved_2a[6:0].
			
			<legal all>
*/
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_OFDMA_RU_ALLOCATION_OFFSET 0x00000004
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_OFDMA_RU_ALLOCATION_LSB 16
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_OFDMA_RU_ALLOCATION_MASK 0x00ff0000

/* Description		RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_OFDMA_USER_INDEX
			
			Field only valid in the of DL MU OFDMA reception
			
			
			
			The user number within the RU_allocation.
			
			
			
			This is needed for SW to determine the exact RU position
			within the reception.
			
			<legal all>
*/
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_OFDMA_USER_INDEX_OFFSET 0x00000004
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_OFDMA_USER_INDEX_LSB 24
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_OFDMA_USER_INDEX_MASK 0x7f000000

/* Description		RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_OFDMA_CONTENT_CHANNEL
			
			Field only valid in the of DL MU OFDMA/MIMO reception
			
			
			
			In case of DL MU reception, this field indicates the
			content channel number where PHY found the RU information
			for this user
			
			
			
			This is needed for SW to determine the exact RU position
			within the reception.
			
			
			
			<enum 0      content_channel_1>
			
			<enum 1      content_channel_2> 
			
			
			
			<legal all>
*/
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_OFDMA_CONTENT_CHANNEL_OFFSET 0x00000004
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_OFDMA_CONTENT_CHANNEL_LSB 31
#define RX_PPDU_START_USER_INFO_1_RECEIVE_USER_INFO_DETAILS_OFDMA_CONTENT_CHANNEL_MASK 0x80000000

/* Description		RX_PPDU_START_USER_INFO_2_RECEIVE_USER_INFO_DETAILS_LDPC
			
			When set, use LDPC transmission rates were used.
			
			<legal all>
*/
#define RX_PPDU_START_USER_INFO_2_RECEIVE_USER_INFO_DETAILS_LDPC_OFFSET 0x00000008
#define RX_PPDU_START_USER_INFO_2_RECEIVE_USER_INFO_DETAILS_LDPC_LSB 0
#define RX_PPDU_START_USER_INFO_2_RECEIVE_USER_INFO_DETAILS_LDPC_MASK 0x00000001

/* Description		RX_PPDU_START_USER_INFO_2_RECEIVE_USER_INFO_DETAILS_RU_WIDTH
			
			In case of UL OFDMA reception, PHY is recommended to
			insert the RU width
			
			In Hastings80: was using Reserved_2a[6:0].
			
			<legal 1 - 74>
*/
#define RX_PPDU_START_USER_INFO_2_RECEIVE_USER_INFO_DETAILS_RU_WIDTH_OFFSET 0x00000008
#define RX_PPDU_START_USER_INFO_2_RECEIVE_USER_INFO_DETAILS_RU_WIDTH_LSB 1
#define RX_PPDU_START_USER_INFO_2_RECEIVE_USER_INFO_DETAILS_RU_WIDTH_MASK 0x000000fe

/* Description		RX_PPDU_START_USER_INFO_2_RECEIVE_USER_INFO_DETAILS_RESERVED_2A
			
			<legal 0>
*/
#define RX_PPDU_START_USER_INFO_2_RECEIVE_USER_INFO_DETAILS_RESERVED_2A_OFFSET 0x00000008
#define RX_PPDU_START_USER_INFO_2_RECEIVE_USER_INFO_DETAILS_RESERVED_2A_LSB 8
#define RX_PPDU_START_USER_INFO_2_RECEIVE_USER_INFO_DETAILS_RESERVED_2A_MASK 0xffffff00


#endif // _RX_PPDU_START_USER_INFO_H_
