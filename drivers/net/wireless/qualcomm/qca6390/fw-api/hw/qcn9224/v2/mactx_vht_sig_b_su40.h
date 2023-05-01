
/* Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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

 
 
 
 
 
 
 


#ifndef _MACTX_VHT_SIG_B_SU40_H_
#define _MACTX_VHT_SIG_B_SU40_H_
#if !defined(__ASSEMBLER__)
#endif

#include "vht_sig_b_su40_info.h"
#define NUM_OF_DWORDS_MACTX_VHT_SIG_B_SU40 2

#define NUM_OF_QWORDS_MACTX_VHT_SIG_B_SU40 1


struct mactx_vht_sig_b_su40 {
#ifndef WIFI_BIT_ORDER_BIG_ENDIAN
             struct   vht_sig_b_su40_info                                       mactx_vht_sig_b_su40_info_details;
#else
             struct   vht_sig_b_su40_info                                       mactx_vht_sig_b_su40_info_details;
#endif
};


 


 

#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_LENGTH_OFFSET        0x0000000000000000
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_LENGTH_LSB           0
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_LENGTH_MSB           18
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_LENGTH_MASK          0x000000000007ffff


 

#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_VHTB_RESERVED_OFFSET 0x0000000000000000
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_VHTB_RESERVED_LSB    19
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_VHTB_RESERVED_MSB    20
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_VHTB_RESERVED_MASK   0x0000000000180000


 

#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_TAIL_OFFSET          0x0000000000000000
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_TAIL_LSB             21
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_TAIL_MSB             26
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_TAIL_MASK            0x0000000007e00000


 

#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RESERVED_OFFSET      0x0000000000000000
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RESERVED_LSB         27
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RESERVED_MSB         30
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RESERVED_MASK        0x0000000078000000


 

#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RX_NDP_OFFSET        0x0000000000000000
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RX_NDP_LSB           31
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RX_NDP_MSB           31
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RX_NDP_MASK          0x0000000080000000


 

#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_LENGTH_COPY_OFFSET   0x0000000000000000
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_LENGTH_COPY_LSB      32
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_LENGTH_COPY_MSB      50
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_LENGTH_COPY_MASK     0x0007ffff00000000


 

#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_VHTB_RESERVED_COPY_OFFSET 0x0000000000000000
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_VHTB_RESERVED_COPY_LSB 51
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_VHTB_RESERVED_COPY_MSB 52
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_VHTB_RESERVED_COPY_MASK 0x0018000000000000


 

#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_TAIL_COPY_OFFSET     0x0000000000000000
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_TAIL_COPY_LSB        53
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_TAIL_COPY_MSB        58
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_TAIL_COPY_MASK       0x07e0000000000000


 

#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RESERVED_COPY_OFFSET 0x0000000000000000
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RESERVED_COPY_LSB    59
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RESERVED_COPY_MSB    62
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RESERVED_COPY_MASK   0x7800000000000000


 

#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RX_NDP_COPY_OFFSET   0x0000000000000000
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RX_NDP_COPY_LSB      63
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RX_NDP_COPY_MSB      63
#define MACTX_VHT_SIG_B_SU40_MACTX_VHT_SIG_B_SU40_INFO_DETAILS_RX_NDP_COPY_MASK     0x8000000000000000



#endif    
