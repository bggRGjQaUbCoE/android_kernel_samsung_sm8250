
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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



#ifndef _MACTX_HE_SIG_A_SU_H_
#define _MACTX_HE_SIG_A_SU_H_
#if !defined(__ASSEMBLER__)
#endif

#include "he_sig_a_su_info.h"
#define NUM_OF_DWORDS_MACTX_HE_SIG_A_SU 2

#define NUM_OF_QWORDS_MACTX_HE_SIG_A_SU 1

struct mactx_he_sig_a_su {
#ifndef WIFI_BIT_ORDER_BIG_ENDIAN
             struct   he_sig_a_su_info                                          mactx_he_sig_a_su_info_details;
#else
             struct   he_sig_a_su_info                                          mactx_he_sig_a_su_info_details;
#endif
};

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_FORMAT_INDICATION_OFFSET   0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_FORMAT_INDICATION_LSB      0
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_FORMAT_INDICATION_MSB      0
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_FORMAT_INDICATION_MASK     0x0000000000000001

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_BEAM_CHANGE_OFFSET         0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_BEAM_CHANGE_LSB            1
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_BEAM_CHANGE_MSB            1
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_BEAM_CHANGE_MASK           0x0000000000000002

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DL_UL_FLAG_OFFSET          0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DL_UL_FLAG_LSB             2
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DL_UL_FLAG_MSB             2
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DL_UL_FLAG_MASK            0x0000000000000004

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TRANSMIT_MCS_OFFSET        0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TRANSMIT_MCS_LSB           3
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TRANSMIT_MCS_MSB           6
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TRANSMIT_MCS_MASK          0x0000000000000078

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DCM_OFFSET                 0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DCM_LSB                    7
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DCM_MSB                    7
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DCM_MASK                   0x0000000000000080

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_BSS_COLOR_ID_OFFSET        0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_BSS_COLOR_ID_LSB           8
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_BSS_COLOR_ID_MSB           13
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_BSS_COLOR_ID_MASK          0x0000000000003f00

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RESERVED_0A_OFFSET         0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RESERVED_0A_LSB            14
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RESERVED_0A_MSB            14
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RESERVED_0A_MASK           0x0000000000004000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_SPATIAL_REUSE_OFFSET       0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_SPATIAL_REUSE_LSB          15
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_SPATIAL_REUSE_MSB          18
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_SPATIAL_REUSE_MASK         0x0000000000078000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TRANSMIT_BW_OFFSET         0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TRANSMIT_BW_LSB            19
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TRANSMIT_BW_MSB            20
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TRANSMIT_BW_MASK           0x0000000000180000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_CP_LTF_SIZE_OFFSET         0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_CP_LTF_SIZE_LSB            21
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_CP_LTF_SIZE_MSB            22
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_CP_LTF_SIZE_MASK           0x0000000000600000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_NSTS_OFFSET                0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_NSTS_LSB                   23
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_NSTS_MSB                   25
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_NSTS_MASK                  0x0000000003800000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RESERVED_0B_OFFSET         0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RESERVED_0B_LSB            26
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RESERVED_0B_MSB            31
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RESERVED_0B_MASK           0x00000000fc000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TXOP_DURATION_OFFSET       0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TXOP_DURATION_LSB          32
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TXOP_DURATION_MSB          38
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TXOP_DURATION_MASK         0x0000007f00000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_CODING_OFFSET              0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_CODING_LSB                 39
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_CODING_MSB                 39
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_CODING_MASK                0x0000008000000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_LDPC_EXTRA_SYMBOL_OFFSET   0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_LDPC_EXTRA_SYMBOL_LSB      40
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_LDPC_EXTRA_SYMBOL_MSB      40
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_LDPC_EXTRA_SYMBOL_MASK     0x0000010000000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_STBC_OFFSET                0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_STBC_LSB                   41
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_STBC_MSB                   41
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_STBC_MASK                  0x0000020000000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TXBF_OFFSET                0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TXBF_LSB                   42
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TXBF_MSB                   42
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TXBF_MASK                  0x0000040000000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_PACKET_EXTENSION_A_FACTOR_OFFSET 0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_PACKET_EXTENSION_A_FACTOR_LSB 43
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_PACKET_EXTENSION_A_FACTOR_MSB 44
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_PACKET_EXTENSION_A_FACTOR_MASK 0x0000180000000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_PACKET_EXTENSION_PE_DISAMBIGUITY_OFFSET 0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_PACKET_EXTENSION_PE_DISAMBIGUITY_LSB 45
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_PACKET_EXTENSION_PE_DISAMBIGUITY_MSB 45
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_PACKET_EXTENSION_PE_DISAMBIGUITY_MASK 0x0000200000000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RESERVED_1A_OFFSET         0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RESERVED_1A_LSB            46
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RESERVED_1A_MSB            46
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RESERVED_1A_MASK           0x0000400000000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DOPPLER_INDICATION_OFFSET  0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DOPPLER_INDICATION_LSB     47
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DOPPLER_INDICATION_MSB     47
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DOPPLER_INDICATION_MASK    0x0000800000000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_CRC_OFFSET                 0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_CRC_LSB                    48
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_CRC_MSB                    51
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_CRC_MASK                   0x000f000000000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TAIL_OFFSET                0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TAIL_LSB                   52
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TAIL_MSB                   57
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_TAIL_MASK                  0x03f0000000000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DOT11AX_SU_EXTENDED_OFFSET 0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DOT11AX_SU_EXTENDED_LSB    58
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DOT11AX_SU_EXTENDED_MSB    58
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DOT11AX_SU_EXTENDED_MASK   0x0400000000000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DOT11AX_EXT_RU_SIZE_OFFSET 0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DOT11AX_EXT_RU_SIZE_LSB    59
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DOT11AX_EXT_RU_SIZE_MSB    61
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_DOT11AX_EXT_RU_SIZE_MASK   0x3800000000000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RX_NDP_OFFSET              0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RX_NDP_LSB                 62
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RX_NDP_MSB                 62
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RX_NDP_MASK                0x4000000000000000

#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RX_INTEGRITY_CHECK_PASSED_OFFSET 0x0000000000000000
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RX_INTEGRITY_CHECK_PASSED_LSB 63
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RX_INTEGRITY_CHECK_PASSED_MSB 63
#define MACTX_HE_SIG_A_SU_MACTX_HE_SIG_A_SU_INFO_DETAILS_RX_INTEGRITY_CHECK_PASSED_MASK 0x8000000000000000

#endif
