
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

 
 
 
 
 
 
 


#ifndef _RX_START_PARAM_H_
#define _RX_START_PARAM_H_
#if !defined(__ASSEMBLER__)
#endif

#define NUM_OF_DWORDS_RX_START_PARAM 2

#define NUM_OF_QWORDS_RX_START_PARAM 1


struct rx_start_param {
#ifndef WIFI_BIT_ORDER_BIG_ENDIAN
             uint32_t pkt_type                                                :  4,  
                      reserved_0a                                             : 12,  
                      remaining_rx_time                                       : 16;  
             uint32_t tlv64_padding                                           : 32;  
#else
             uint32_t remaining_rx_time                                       : 16,  
                      reserved_0a                                             : 12,  
                      pkt_type                                                :  4;  
             uint32_t tlv64_padding                                           : 32;  
#endif
};


 

#define RX_START_PARAM_PKT_TYPE_OFFSET                                              0x0000000000000000
#define RX_START_PARAM_PKT_TYPE_LSB                                                 0
#define RX_START_PARAM_PKT_TYPE_MSB                                                 3
#define RX_START_PARAM_PKT_TYPE_MASK                                                0x000000000000000f


 

#define RX_START_PARAM_RESERVED_0A_OFFSET                                           0x0000000000000000
#define RX_START_PARAM_RESERVED_0A_LSB                                              4
#define RX_START_PARAM_RESERVED_0A_MSB                                              15
#define RX_START_PARAM_RESERVED_0A_MASK                                             0x000000000000fff0


 

#define RX_START_PARAM_REMAINING_RX_TIME_OFFSET                                     0x0000000000000000
#define RX_START_PARAM_REMAINING_RX_TIME_LSB                                        16
#define RX_START_PARAM_REMAINING_RX_TIME_MSB                                        31
#define RX_START_PARAM_REMAINING_RX_TIME_MASK                                       0x00000000ffff0000


 

#define RX_START_PARAM_TLV64_PADDING_OFFSET                                         0x0000000000000000
#define RX_START_PARAM_TLV64_PADDING_LSB                                            32
#define RX_START_PARAM_TLV64_PADDING_MSB                                            63
#define RX_START_PARAM_TLV64_PADDING_MASK                                           0xffffffff00000000



#endif    
