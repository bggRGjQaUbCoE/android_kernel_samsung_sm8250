/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
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

/**
 * Generated file ... Do not hand edit ...
 */

#ifndef _TLV_TAG_DEF_
#define _TLV_TAG_DEF_

typedef enum {

  WIFIMACTX_CBF_START_E                    =   0 /* 0x0 */,
  WIFIPHYRX_DATA_E                         =   1 /* 0x1 */,
  WIFIPHYRX_CBF_DATA_RESP_E                =   2 /* 0x2 */,
  WIFIPHYRX_ABORT_REQUEST_E                =   3 /* 0x3 */,
  WIFIPHYRX_USER_ABORT_NOTIFICATION_E      =   4 /* 0x4 */,
  WIFIMACTX_DATA_RESP_E                    =   5 /* 0x5 */,
  WIFIMACTX_CBF_DATA_E                     =   6 /* 0x6 */,
  WIFIMACTX_CBF_DONE_E                     =   7 /* 0x7 */,
  WIFIMACRX_CBF_READ_REQUEST_E             =   8 /* 0x8 */,
  WIFIMACRX_CBF_DATA_REQUEST_E             =   9 /* 0x9 */,
  WIFIMACRX_EXPECT_NDP_RECEPTION_E         =  10 /* 0xa */,
  WIFIMACRX_FREEZE_CAPTURE_CHANNEL_E       =  11 /* 0xb */,
  WIFIMACRX_NDP_TIMEOUT_E                  =  12 /* 0xc */,
  WIFIMACRX_ABORT_ACK_E                    =  13 /* 0xd */,
  WIFIMACRX_REQ_IMPLICIT_FB_E              =  14 /* 0xe */,
  WIFIMACRX_CHAIN_MASK_E                   =  15 /* 0xf */,
  WIFIMACRX_NAP_USER_E                     =  16 /* 0x10 */,
  WIFIMACRX_ABORT_REQUEST_E                =  17 /* 0x11 */,
  WIFIPHYTX_OTHER_TRANSMIT_INFO16_E        =  18 /* 0x12 */,
  WIFIPHYTX_ABORT_ACK_E                    =  19 /* 0x13 */,
  WIFIPHYTX_ABORT_REQUEST_E                =  20 /* 0x14 */,
  WIFIPHYTX_PKT_END_E                      =  21 /* 0x15 */,
  WIFIPHYTX_PPDU_HEADER_INFO_REQUEST_E     =  22 /* 0x16 */,
  WIFIPHYTX_REQUEST_CTRL_INFO_E            =  23 /* 0x17 */,
  WIFIPHYTX_DATA_REQUEST_E                 =  24 /* 0x18 */,
  WIFIPHYTX_BF_CV_LOADING_DONE_E           =  25 /* 0x19 */,
  WIFIPHYTX_NAP_ACK_E                      =  26 /* 0x1a */,
  WIFIPHYTX_NAP_DONE_E                     =  27 /* 0x1b */,
  WIFIPHYTX_OFF_ACK_E                      =  28 /* 0x1c */,
  WIFIPHYTX_ON_ACK_E                       =  29 /* 0x1d */,
  WIFIPHYTX_SYNTH_OFF_ACK_E                =  30 /* 0x1e */,
  WIFIPHYTX_DEBUG16_E                      =  31 /* 0x1f */,
  WIFIMACTX_ABORT_REQUEST_E                =  32 /* 0x20 */,
  WIFIMACTX_ABORT_ACK_E                    =  33 /* 0x21 */,
  WIFIMACTX_PKT_END_E                      =  34 /* 0x22 */,
  WIFIMACTX_PRE_PHY_DESC_E                 =  35 /* 0x23 */,
  WIFIMACTX_BF_PARAMS_COMMON_E             =  36 /* 0x24 */,
  WIFIMACTX_BF_PARAMS_PER_USER_E           =  37 /* 0x25 */,
  WIFIMACTX_PREFETCH_CV_E                  =  38 /* 0x26 */,
  WIFIMACTX_USER_DESC_COMMON_E             =  39 /* 0x27 */,
  WIFIMACTX_USER_DESC_PER_USER_E           =  40 /* 0x28 */,
  WIFIEXAMPLE_USER_TLV_16_E                =  41 /* 0x29 */,
  WIFIEXAMPLE_TLV_16_E                     =  42 /* 0x2a */,
  WIFIMACTX_PHY_OFF_E                      =  43 /* 0x2b */,
  WIFIMACTX_PHY_ON_E                       =  44 /* 0x2c */,
  WIFIMACTX_SYNTH_OFF_E                    =  45 /* 0x2d */,
  WIFIMACTX_EXPECT_CBF_COMMON_E            =  46 /* 0x2e */,
  WIFIMACTX_EXPECT_CBF_PER_USER_E          =  47 /* 0x2f */,
  WIFIMACTX_PHY_DESC_E                     =  48 /* 0x30 */,
  WIFIMACTX_L_SIG_A_E                      =  49 /* 0x31 */,
  WIFIMACTX_L_SIG_B_E                      =  50 /* 0x32 */,
  WIFIMACTX_HT_SIG_E                       =  51 /* 0x33 */,
  WIFIMACTX_VHT_SIG_A_E                    =  52 /* 0x34 */,
  WIFIMACTX_VHT_SIG_B_SU20_E               =  53 /* 0x35 */,
  WIFIMACTX_VHT_SIG_B_SU40_E               =  54 /* 0x36 */,
  WIFIMACTX_VHT_SIG_B_SU80_E               =  55 /* 0x37 */,
  WIFIMACTX_VHT_SIG_B_SU160_E              =  56 /* 0x38 */,
  WIFIMACTX_VHT_SIG_B_MU20_E               =  57 /* 0x39 */,
  WIFIMACTX_VHT_SIG_B_MU40_E               =  58 /* 0x3a */,
  WIFIMACTX_VHT_SIG_B_MU80_E               =  59 /* 0x3b */,
  WIFIMACTX_VHT_SIG_B_MU160_E              =  60 /* 0x3c */,
  WIFIMACTX_SERVICE_E                      =  61 /* 0x3d */,
  WIFIMACTX_HE_SIG_A_SU_E                  =  62 /* 0x3e */,
  WIFIMACTX_HE_SIG_A_MU_DL_E               =  63 /* 0x3f */,
  WIFIMACTX_HE_SIG_A_MU_UL_E               =  64 /* 0x40 */,
  WIFIMACTX_HE_SIG_B1_MU_E                 =  65 /* 0x41 */,
  WIFIMACTX_HE_SIG_B2_MU_E                 =  66 /* 0x42 */,
  WIFIMACTX_HE_SIG_B2_OFDMA_E              =  67 /* 0x43 */,
  WIFIMACTX_DELETE_CV_E                    =  68 /* 0x44 */,
  WIFIMACTX_MU_UPLINK_COMMON_E             =  69 /* 0x45 */,
  WIFIMACTX_MU_UPLINK_USER_SETUP_E         =  70 /* 0x46 */,
  WIFIMACTX_OTHER_TRANSMIT_INFO_E          =  71 /* 0x47 */,
  WIFIMACTX_PHY_NAP_E                      =  72 /* 0x48 */,
  WIFIMACTX_DEBUG_E                        =  73 /* 0x49 */,
  WIFIPHYRX_ABORT_ACK_E                    =  74 /* 0x4a */,
  WIFIPHYRX_GENERATED_CBF_DETAILS_E        =  75 /* 0x4b */,
  WIFIPHYRX_RSSI_LEGACY_E                  =  76 /* 0x4c */,
  WIFIPHYRX_RSSI_HT_E                      =  77 /* 0x4d */,
  WIFIPHYRX_USER_INFO_E                    =  78 /* 0x4e */,
  WIFIPHYRX_PKT_END_E                      =  79 /* 0x4f */,
  WIFIPHYRX_DEBUG_E                        =  80 /* 0x50 */,
  WIFIPHYRX_CBF_TRANSFER_DONE_E            =  81 /* 0x51 */,
  WIFIPHYRX_CBF_TRANSFER_ABORT_E           =  82 /* 0x52 */,
  WIFIPHYRX_L_SIG_A_E                      =  83 /* 0x53 */,
  WIFIPHYRX_L_SIG_B_E                      =  84 /* 0x54 */,
  WIFIPHYRX_HT_SIG_E                       =  85 /* 0x55 */,
  WIFIPHYRX_VHT_SIG_A_E                    =  86 /* 0x56 */,
  WIFIPHYRX_VHT_SIG_B_SU20_E               =  87 /* 0x57 */,
  WIFIPHYRX_VHT_SIG_B_SU40_E               =  88 /* 0x58 */,
  WIFIPHYRX_VHT_SIG_B_SU80_E               =  89 /* 0x59 */,
  WIFIPHYRX_VHT_SIG_B_SU160_E              =  90 /* 0x5a */,
  WIFIPHYRX_VHT_SIG_B_MU20_E               =  91 /* 0x5b */,
  WIFIPHYRX_VHT_SIG_B_MU40_E               =  92 /* 0x5c */,
  WIFIPHYRX_VHT_SIG_B_MU80_E               =  93 /* 0x5d */,
  WIFIPHYRX_VHT_SIG_B_MU160_E              =  94 /* 0x5e */,
  WIFIPHYRX_HE_SIG_A_SU_E                  =  95 /* 0x5f */,
  WIFIPHYRX_HE_SIG_A_MU_DL_E               =  96 /* 0x60 */,
  WIFIPHYRX_HE_SIG_A_MU_UL_E               =  97 /* 0x61 */,
  WIFIPHYRX_HE_SIG_B1_MU_E                 =  98 /* 0x62 */,
  WIFIPHYRX_HE_SIG_B2_MU_E                 =  99 /* 0x63 */,
  WIFIPHYRX_HE_SIG_B2_OFDMA_E              = 100 /* 0x64 */,
  WIFIPHYRX_OTHER_RECEIVE_INFO_E           = 101 /* 0x65 */,
  WIFIPHYRX_COMMON_USER_INFO_E             = 102 /* 0x66 */,
  WIFIPHYRX_DATA_DONE_E                    = 103 /* 0x67 */,
  WIFIRECEIVE_RSSI_INFO_E                  = 104 /* 0x68 */,
  WIFIRECEIVE_USER_INFO_E                  = 105 /* 0x69 */,
  WIFIMIMO_CONTROL_INFO_E                  = 106 /* 0x6a */,
  WIFIRX_LOCATION_INFO_E                   = 107 /* 0x6b */,
  WIFICOEX_TX_REQ_E                        = 108 /* 0x6c */,
  WIFIDUMMY_E                              = 109 /* 0x6d */,
  WIFIRX_TIMING_OFFSET_INFO_E              = 110 /* 0x6e */,
  WIFIEXAMPLE_TLV_32_NAME_E                = 111 /* 0x6f */,
  WIFIMPDU_LIMIT_E                         = 112 /* 0x70 */,
  WIFINA_LENGTH_END_E                      = 113 /* 0x71 */,
  WIFIOLE_BUF_STATUS_E                     = 114 /* 0x72 */,
  WIFIPCU_PPDU_SETUP_DONE_E                = 115 /* 0x73 */,
  WIFIPCU_PPDU_SETUP_END_E                 = 116 /* 0x74 */,
  WIFIPCU_PPDU_SETUP_INIT_E                = 117 /* 0x75 */,
  WIFIPCU_PPDU_SETUP_START_E               = 118 /* 0x76 */,
  WIFIPDG_FES_SETUP_E                      = 119 /* 0x77 */,
  WIFIPDG_RESPONSE_E                       = 120 /* 0x78 */,
  WIFIPDG_TX_REQ_E                         = 121 /* 0x79 */,
  WIFISCH_WAIT_INSTR_E                     = 122 /* 0x7a */,
  WIFISCHEDULER_TLV_E                      = 123 /* 0x7b */,
  WIFITQM_FLOW_EMPTY_STATUS_E              = 124 /* 0x7c */,
  WIFITQM_FLOW_NOT_EMPTY_STATUS_E          = 125 /* 0x7d */,
  WIFITQM_GEN_MPDU_LENGTH_LIST_E           = 126 /* 0x7e */,
  WIFITQM_GEN_MPDU_LENGTH_LIST_STATUS_E    = 127 /* 0x7f */,
  WIFITQM_GEN_MPDUS_E                      = 128 /* 0x80 */,
  WIFITQM_GEN_MPDUS_STATUS_E               = 129 /* 0x81 */,
  WIFITQM_REMOVE_MPDU_E                    = 130 /* 0x82 */,
  WIFITQM_REMOVE_MPDU_STATUS_E             = 131 /* 0x83 */,
  WIFITQM_REMOVE_MSDU_E                    = 132 /* 0x84 */,
  WIFITQM_REMOVE_MSDU_STATUS_E             = 133 /* 0x85 */,
  WIFITQM_UPDATE_TX_MPDU_COUNT_E           = 134 /* 0x86 */,
  WIFITQM_WRITE_CMD_E                      = 135 /* 0x87 */,
  WIFIOFDMA_TRIGGER_DETAILS_E              = 136 /* 0x88 */,
  WIFITX_DATA_E                            = 137 /* 0x89 */,
  WIFITX_FES_SETUP_E                       = 138 /* 0x8a */,
  WIFIRX_PACKET_E                          = 139 /* 0x8b */,
  WIFIEXPECTED_RESPONSE_E                  = 140 /* 0x8c */,
  WIFITX_MPDU_END_E                        = 141 /* 0x8d */,
  WIFITX_MPDU_START_E                      = 142 /* 0x8e */,
  WIFITX_MSDU_END_E                        = 143 /* 0x8f */,
  WIFITX_MSDU_START_E                      = 144 /* 0x90 */,
  WIFITX_SW_MODE_SETUP_E                   = 145 /* 0x91 */,
  WIFITXPCU_BUFFER_STATUS_E                = 146 /* 0x92 */,
  WIFITXPCU_USER_BUFFER_STATUS_E           = 147 /* 0x93 */,
  WIFIDATA_TO_TIME_CONFIG_E                = 148 /* 0x94 */,
  WIFIEXAMPLE_USER_TLV_32_E                = 149 /* 0x95 */,
  WIFIMPDU_INFO_E                          = 150 /* 0x96 */,
  WIFIPDG_USER_SETUP_E                     = 151 /* 0x97 */,
  WIFITX_11AH_SETUP_E                      = 152 /* 0x98 */,
  WIFIREO_UPDATE_RX_REO_QUEUE_STATUS_E     = 153 /* 0x99 */,
  WIFITX_PEER_ENTRY_E                      = 154 /* 0x9a */,
  WIFITX_RAW_OR_NATIVE_FRAME_SETUP_E       = 155 /* 0x9b */,
  WIFIEXAMPLE_STRUCT_NAME_E                = 156 /* 0x9c */,
  WIFIPCU_PPDU_SETUP_END_INFO_E            = 157 /* 0x9d */,
  WIFIPPDU_RATE_SETTING_E                  = 158 /* 0x9e */,
  WIFIPROT_RATE_SETTING_E                  = 159 /* 0x9f */,
  WIFIRX_MPDU_DETAILS_E                    = 160 /* 0xa0 */,
  WIFIEXAMPLE_USER_TLV_42_E                = 161 /* 0xa1 */,
  WIFIRX_MSDU_LINK_E                       = 162 /* 0xa2 */,
  WIFIRX_REO_QUEUE_E                       = 163 /* 0xa3 */,
  WIFIADDR_SEARCH_ENTRY_E                  = 164 /* 0xa4 */,
  WIFISCHEDULER_CMD_E                      = 165 /* 0xa5 */,
  WIFITX_FLUSH_E                           = 166 /* 0xa6 */,
  WIFITQM_ENTRANCE_RING_E                  = 167 /* 0xa7 */,
  WIFITX_DATA_WORD_E                       = 168 /* 0xa8 */,
  WIFITX_MPDU_DETAILS_E                    = 169 /* 0xa9 */,
  WIFITX_MPDU_LINK_E                       = 170 /* 0xaa */,
  WIFITX_MPDU_LINK_PTR_E                   = 171 /* 0xab */,
  WIFITX_MPDU_QUEUE_HEAD_E                 = 172 /* 0xac */,
  WIFITX_MPDU_QUEUE_EXT_E                  = 173 /* 0xad */,
  WIFITX_MPDU_QUEUE_EXT_PTR_E              = 174 /* 0xae */,
  WIFITX_MSDU_DETAILS_E                    = 175 /* 0xaf */,
  WIFITX_MSDU_EXTENSION_E                  = 176 /* 0xb0 */,
  WIFITX_MSDU_FLOW_E                       = 177 /* 0xb1 */,
  WIFITX_MSDU_LINK_E                       = 178 /* 0xb2 */,
  WIFITX_MSDU_LINK_ENTRY_PTR_E             = 179 /* 0xb3 */,
  WIFIRESPONSE_RATE_SETTING_E              = 180 /* 0xb4 */,
  WIFITXPCU_BUFFER_BASICS_E                = 181 /* 0xb5 */,
  WIFIUNIFORM_DESCRIPTOR_HEADER_E          = 182 /* 0xb6 */,
  WIFIUNIFORM_TQM_CMD_HEADER_E             = 183 /* 0xb7 */,
  WIFIUNIFORM_TQM_STATUS_HEADER_E          = 184 /* 0xb8 */,
  WIFIUSER_RATE_SETTING_E                  = 185 /* 0xb9 */,
  WIFIWBM_BUFFER_RING_E                    = 186 /* 0xba */,
  WIFIWBM_LINK_DESCRIPTOR_RING_E           = 187 /* 0xbb */,
  WIFIWBM_RELEASE_RING_E                   = 188 /* 0xbc */,
  WIFITX_FLUSH_REQ_E                       = 189 /* 0xbd */,
  WIFIRX_MSDU_DETAILS_E                    = 190 /* 0xbe */,
  WIFITQM_WRITE_CMD_STATUS_E               = 191 /* 0xbf */,
  WIFITQM_GET_MPDU_QUEUE_STATS_E           = 192 /* 0xc0 */,
  WIFITQM_GET_MSDU_FLOW_STATS_E            = 193 /* 0xc1 */,
  WIFIEXAMPLE_USER_CTLV_32_E               = 194 /* 0xc2 */,
  WIFITX_FES_STATUS_START_E                = 195 /* 0xc3 */,
  WIFITX_FES_STATUS_USER_PPDU_E            = 196 /* 0xc4 */,
  WIFITX_FES_STATUS_USER_RESPONSE_E        = 197 /* 0xc5 */,
  WIFITX_FES_STATUS_END_E                  = 198 /* 0xc6 */,
  WIFIRX_TRIG_INFO_E                       = 199 /* 0xc7 */,
  WIFIRXPCU_TX_SETUP_CLEAR_E               = 200 /* 0xc8 */,
  WIFIRX_FRAME_BITMAP_REQ_E                = 201 /* 0xc9 */,
  WIFIRX_FRAME_BITMAP_ACK_E                = 202 /* 0xca */,
  WIFICOEX_RX_STATUS_E                     = 203 /* 0xcb */,
  WIFIRX_START_PARAM_E                     = 204 /* 0xcc */,
  WIFIRX_PPDU_START_E                      = 205 /* 0xcd */,
  WIFIRX_PPDU_END_E                        = 206 /* 0xce */,
  WIFIRX_MPDU_START_E                      = 207 /* 0xcf */,
  WIFIRX_MPDU_END_E                        = 208 /* 0xd0 */,
  WIFIRX_MSDU_START_E                      = 209 /* 0xd1 */,
  WIFIRX_MSDU_END_E                        = 210 /* 0xd2 */,
  WIFIRX_ATTENTION_E                       = 211 /* 0xd3 */,
  WIFIRECEIVED_RESPONSE_INFO_E             = 212 /* 0xd4 */,
  WIFIRX_PHY_SLEEP_E                       = 213 /* 0xd5 */,
  WIFIRX_HEADER_E                          = 214 /* 0xd6 */,
  WIFIRX_PEER_ENTRY_E                      = 215 /* 0xd7 */,
  WIFIRX_FLUSH_E                           = 216 /* 0xd8 */,
  WIFIRX_RESPONSE_REQUIRED_INFO_E          = 217 /* 0xd9 */,
  WIFIRX_FRAMELESS_BAR_DETAILS_E           = 218 /* 0xda */,
  WIFITQM_GET_MPDU_QUEUE_STATS_STATUS_E    = 219 /* 0xdb */,
  WIFITQM_GET_MSDU_FLOW_STATS_STATUS_E     = 220 /* 0xdc */,
  WIFITX_CBF_INFO_E                        = 221 /* 0xdd */,
  WIFIPCU_PPDU_SETUP_USER_E                = 222 /* 0xde */,
  WIFIRX_MPDU_PCU_START_E                  = 223 /* 0xdf */,
  WIFIRX_PM_INFO_E                         = 224 /* 0xe0 */,
  WIFIRX_USER_PPDU_END_E                   = 225 /* 0xe1 */,
  WIFIRX_PRE_PPDU_START_E                  = 226 /* 0xe2 */,
  WIFIRX_PREAMBLE_E                        = 227 /* 0xe3 */,
  WIFITX_FES_SETUP_COMPLETE_E              = 228 /* 0xe4 */,
  WIFITX_LAST_MPDU_FETCHED_E               = 229 /* 0xe5 */,
  WIFITXDMA_STOP_REQUEST_E                 = 230 /* 0xe6 */,
  WIFIRXPCU_SETUP_E                        = 231 /* 0xe7 */,
  WIFIRXPCU_USER_SETUP_E                   = 232 /* 0xe8 */,
  WIFITX_FES_STATUS_ACK_OR_BA_E            = 233 /* 0xe9 */,
  WIFITQM_ACKED_MPDU_E                     = 234 /* 0xea */,
  WIFICOEX_TX_RESP_E                       = 235 /* 0xeb */,
  WIFICOEX_TX_STATUS_E                     = 236 /* 0xec */,
  WIFIMACTX_COEX_PHY_CTRL_E                = 237 /* 0xed */,
  WIFICOEX_STATUS_BROADCAST_E              = 238 /* 0xee */,
  WIFIRESPONSE_START_STATUS_E              = 239 /* 0xef */,
  WIFIRESPONSE_END_STATUS_E                = 240 /* 0xf0 */,
  WIFICRYPTO_STATUS_E                      = 241 /* 0xf1 */,
  WIFIRECEIVED_TRIGGER_INFO_E              = 242 /* 0xf2 */,
  WIFIREO_ENTRANCE_RING_E                  = 243 /* 0xf3 */,
  WIFIRX_MPDU_LINK_E                       = 244 /* 0xf4 */,
  WIFICOEX_TX_STOP_CTRL_E                  = 245 /* 0xf5 */,
  WIFIRX_PPDU_ACK_REPORT_E                 = 246 /* 0xf6 */,
  WIFIRX_PPDU_NO_ACK_REPORT_E              = 247 /* 0xf7 */,
  WIFISCH_COEX_STATUS_E                    = 248 /* 0xf8 */,
  WIFISCHEDULER_COMMAND_STATUS_E           = 249 /* 0xf9 */,
  WIFISCHEDULER_RX_PPDU_NO_RESPONSE_STATUS_E = 250 /* 0xfa */,
  WIFITX_FES_STATUS_PROT_E                 = 251 /* 0xfb */,
  WIFITX_FES_STATUS_START_PPDU_E           = 252 /* 0xfc */,
  WIFITX_FES_STATUS_START_PROT_E           = 253 /* 0xfd */,
  WIFITXPCU_PHYTX_DEBUG32_E                = 254 /* 0xfe */,
  WIFITXPCU_PHYTX_OTHER_TRANSMIT_INFO32_E  = 255 /* 0xff */,
  WIFITX_MPDU_COUNT_TRANSFER_END_E         = 256 /* 0x100 */,
  WIFIWHO_ANCHOR_OFFSET_E                  = 257 /* 0x101 */,
  WIFIWHO_ANCHOR_VALUE_E                   = 258 /* 0x102 */,
  WIFIWHO_CCE_INFO_E                       = 259 /* 0x103 */,
  WIFIWHO_COMMIT_E                         = 260 /* 0x104 */,
  WIFIWHO_COMMIT_DONE_E                    = 261 /* 0x105 */,
  WIFIWHO_FLUSH_E                          = 262 /* 0x106 */,
  WIFIWHO_L2_LLC_E                         = 263 /* 0x107 */,
  WIFIWHO_L2_PAYLOAD_E                     = 264 /* 0x108 */,
  WIFIWHO_L3_CHECKSUM_E                    = 265 /* 0x109 */,
  WIFIWHO_L3_INFO_E                        = 266 /* 0x10a */,
  WIFIWHO_L4_CHECKSUM_E                    = 267 /* 0x10b */,
  WIFIWHO_L4_INFO_E                        = 268 /* 0x10c */,
  WIFIWHO_MSDU_E                           = 269 /* 0x10d */,
  WIFIWHO_MSDU_MISC_E                      = 270 /* 0x10e */,
  WIFIWHO_PACKET_DATA_E                    = 271 /* 0x10f */,
  WIFIWHO_PACKET_HDR_E                     = 272 /* 0x110 */,
  WIFIWHO_PPDU_END_E                       = 273 /* 0x111 */,
  WIFIWHO_PPDU_START_E                     = 274 /* 0x112 */,
  WIFIWHO_TSO_E                            = 275 /* 0x113 */,
  WIFIWHO_WMAC_HEADER_PV0_E                = 276 /* 0x114 */,
  WIFIWHO_WMAC_HEADER_PV1_E                = 277 /* 0x115 */,
  WIFIWHO_WMAC_IV_E                        = 278 /* 0x116 */,
  WIFIMPDU_INFO_END_E                      = 279 /* 0x117 */,
  WIFIMPDU_INFO_BITMAP_E                   = 280 /* 0x118 */,
  WIFITX_QUEUE_EXTENSION_E                 = 281 /* 0x119 */,
  WIFIRX_PEER_ENTRY_DETAILS_E              = 282 /* 0x11a */,
  WIFIRX_REO_QUEUE_REFERENCE_E             = 283 /* 0x11b */,
  WIFIRX_REO_QUEUE_EXT_E                   = 284 /* 0x11c */,
  WIFISCHEDULER_SELFGEN_RESPONSE_STATUS_E  = 285 /* 0x11d */,
  WIFITQM_UPDATE_TX_MPDU_COUNT_STATUS_E    = 286 /* 0x11e */,
  WIFITQM_ACKED_MPDU_STATUS_E              = 287 /* 0x11f */,
  WIFITQM_ADD_MSDU_STATUS_E                = 288 /* 0x120 */,
  WIFIRX_MPDU_LINK_PTR_E                   = 289 /* 0x121 */,
  WIFIREO_DESTINATION_RING_E               = 290 /* 0x122 */,
  WIFITQM_LIST_GEN_DONE_E                  = 291 /* 0x123 */,
  WIFIWHO_TERMINATE_E                      = 292 /* 0x124 */,
  WIFITX_LAST_MPDU_END_E                   = 293 /* 0x125 */,
  WIFITX_CV_DATA_E                         = 294 /* 0x126 */,
  WIFITCL_ENTRANCE_FROM_PPE_RING_E         = 295 /* 0x127 */,
  WIFIPPDU_TX_END_E                        = 296 /* 0x128 */,
  WIFIPROT_TX_END_E                        = 297 /* 0x129 */,
  WIFIPDG_RESPONSE_RATE_SETTING_E          = 298 /* 0x12a */,
  WIFIMPDU_INFO_GLOBAL_END_E               = 299 /* 0x12b */,
  WIFITQM_SCH_INSTR_GLOBAL_END_E           = 300 /* 0x12c */,
  WIFIRX_PPDU_END_USER_STATS_E             = 301 /* 0x12d */,
  WIFIRX_PPDU_END_USER_STATS_EXT_E         = 302 /* 0x12e */,
  WIFINO_ACK_REPORT_E                      = 303 /* 0x12f */,
  WIFIACK_REPORT_E                         = 304 /* 0x130 */,
  WIFIUNIFORM_REO_CMD_HEADER_E             = 305 /* 0x131 */,
  WIFIREO_GET_QUEUE_STATS_E                = 306 /* 0x132 */,
  WIFIREO_FLUSH_QUEUE_E                    = 307 /* 0x133 */,
  WIFIREO_FLUSH_CACHE_E                    = 308 /* 0x134 */,
  WIFIREO_UNBLOCK_CACHE_E                  = 309 /* 0x135 */,
  WIFIUNIFORM_REO_STATUS_HEADER_E          = 310 /* 0x136 */,
  WIFIREO_GET_QUEUE_STATS_STATUS_E         = 311 /* 0x137 */,
  WIFIREO_FLUSH_QUEUE_STATUS_E             = 312 /* 0x138 */,
  WIFIREO_FLUSH_CACHE_STATUS_E             = 313 /* 0x139 */,
  WIFIREO_UNBLOCK_CACHE_STATUS_E           = 314 /* 0x13a */,
  WIFITQM_FLUSH_CACHE_E                    = 315 /* 0x13b */,
  WIFITQM_UNBLOCK_CACHE_E                  = 316 /* 0x13c */,
  WIFITQM_FLUSH_CACHE_STATUS_E             = 317 /* 0x13d */,
  WIFITQM_UNBLOCK_CACHE_STATUS_E           = 318 /* 0x13e */,
  WIFIRX_PPDU_END_STATUS_DONE_E            = 319 /* 0x13f */,
  WIFIRX_STATUS_BUFFER_DONE_E              = 320 /* 0x140 */,
  WIFIBUFFER_ADDR_INFO_E                   = 321 /* 0x141 */,
  WIFIRX_MSDU_DESC_INFO_E                  = 322 /* 0x142 */,
  WIFIRX_MPDU_DESC_INFO_E                  = 323 /* 0x143 */,
  WIFITCL_DATA_CMD_E                       = 324 /* 0x144 */,
  WIFITCL_GSE_CMD_E                        = 325 /* 0x145 */,
  WIFITCL_EXIT_BASE_E                      = 326 /* 0x146 */,
  WIFITCL_COMPACT_EXIT_RING_E              = 327 /* 0x147 */,
  WIFITCL_REGULAR_EXIT_RING_E              = 328 /* 0x148 */,
  WIFITCL_EXTENDED_EXIT_RING_E             = 329 /* 0x149 */,
  WIFIUPLINK_COMMON_INFO_E                 = 330 /* 0x14a */,
  WIFIUPLINK_USER_SETUP_INFO_E             = 331 /* 0x14b */,
  WIFITX_DATA_SYNC_E                       = 332 /* 0x14c */,
  WIFIPHYRX_CBF_READ_REQUEST_ACK_E         = 333 /* 0x14d */,
  WIFITCL_STATUS_RING_E                    = 334 /* 0x14e */,
  WIFITQM_GET_MPDU_HEAD_INFO_E             = 335 /* 0x14f */,
  WIFITQM_SYNC_CMD_E                       = 336 /* 0x150 */,
  WIFITQM_GET_MPDU_HEAD_INFO_STATUS_E      = 337 /* 0x151 */,
  WIFITQM_SYNC_CMD_STATUS_E                = 338 /* 0x152 */,
  WIFITQM_THRESHOLD_DROP_NOTIFICATION_STATUS_E = 339 /* 0x153 */,
  WIFITQM_DESCRIPTOR_THRESHOLD_REACHED_STATUS_E = 340 /* 0x154 */,
  WIFIREO_FLUSH_TIMEOUT_LIST_E             = 341 /* 0x155 */,
  WIFIREO_FLUSH_TIMEOUT_LIST_STATUS_E      = 342 /* 0x156 */,
  WIFIREO_TO_PPE_RING_E                    = 343 /* 0x157 */,
  WIFIRX_MPDU_INFO_E                       = 344 /* 0x158 */,
  WIFIREO_DESCRIPTOR_THRESHOLD_REACHED_STATUS_E = 345 /* 0x159 */,
  WIFISCHEDULER_RX_SIFS_RESPONSE_TRIGGER_STATUS_E = 346 /* 0x15a */,
  WIFIEXAMPLE_USER_TLV_32_NAME_E           = 347 /* 0x15b */,
  WIFIRX_PPDU_START_USER_INFO_E            = 348 /* 0x15c */,
  WIFIRX_RXPCU_CLASSIFICATION_OVERVIEW_E   = 349 /* 0x15d */,
  WIFIRX_RING_MASK_E                       = 350 /* 0x15e */,
  WIFIWHO_CLASSIFY_INFO_E                  = 351 /* 0x15f */,
  WIFITXPT_CLASSIFY_INFO_E                 = 352 /* 0x160 */,
  WIFIRXPT_CLASSIFY_INFO_E                 = 353 /* 0x161 */,
  WIFITX_FLOW_SEARCH_ENTRY_E               = 354 /* 0x162 */,
  WIFIRX_FLOW_SEARCH_ENTRY_E               = 355 /* 0x163 */,
  WIFIRECEIVED_TRIGGER_INFO_DETAILS_E      = 356 /* 0x164 */,
  WIFICOEX_MAC_NAP_E                       = 357 /* 0x165 */,
  WIFIMACRX_ABORT_REQUEST_INFO_E           = 358 /* 0x166 */,
  WIFIMACTX_ABORT_REQUEST_INFO_E           = 359 /* 0x167 */,
  WIFIPHYRX_ABORT_REQUEST_INFO_E           = 360 /* 0x168 */,
  WIFIPHYTX_ABORT_REQUEST_INFO_E           = 361 /* 0x169 */,
  WIFIRXPCU_PPDU_END_INFO_E                = 362 /* 0x16a */,
  WIFIWHO_MESH_CONTROL_E                   = 363 /* 0x16b */,
  WIFIL_SIG_A_INFO_E                       = 364 /* 0x16c */,
  WIFIL_SIG_B_INFO_E                       = 365 /* 0x16d */,
  WIFIHT_SIG_INFO_E                        = 366 /* 0x16e */,
  WIFIVHT_SIG_A_INFO_E                     = 367 /* 0x16f */,
  WIFIVHT_SIG_B_SU20_INFO_E                = 368 /* 0x170 */,
  WIFIVHT_SIG_B_SU40_INFO_E                = 369 /* 0x171 */,
  WIFIVHT_SIG_B_SU80_INFO_E                = 370 /* 0x172 */,
  WIFIVHT_SIG_B_SU160_INFO_E               = 371 /* 0x173 */,
  WIFIVHT_SIG_B_MU20_INFO_E                = 372 /* 0x174 */,
  WIFIVHT_SIG_B_MU40_INFO_E                = 373 /* 0x175 */,
  WIFIVHT_SIG_B_MU80_INFO_E                = 374 /* 0x176 */,
  WIFIVHT_SIG_B_MU160_INFO_E               = 375 /* 0x177 */,
  WIFISERVICE_INFO_E                       = 376 /* 0x178 */,
  WIFIHE_SIG_A_SU_INFO_E                   = 377 /* 0x179 */,
  WIFIHE_SIG_A_MU_DL_INFO_E                = 378 /* 0x17a */,
  WIFIHE_SIG_A_MU_UL_INFO_E                = 379 /* 0x17b */,
  WIFIHE_SIG_B1_MU_INFO_E                  = 380 /* 0x17c */,
  WIFIHE_SIG_B2_MU_INFO_E                  = 381 /* 0x17d */,
  WIFIHE_SIG_B2_OFDMA_INFO_E               = 382 /* 0x17e */,
  WIFIPDG_SW_MODE_BW_START_E               = 383 /* 0x17f */,
  WIFIPDG_SW_MODE_BW_END_E                 = 384 /* 0x180 */,
  WIFIPDG_WAIT_FOR_MAC_REQUEST_E           = 385 /* 0x181 */,
  WIFIPDG_WAIT_FOR_PHY_REQUEST_E           = 386 /* 0x182 */,
  WIFISCHEDULER_END_E                      = 387 /* 0x183 */,
  WIFIPEER_TABLE_ENTRY_E                   = 388 /* 0x184 */,
  WIFISW_PEER_INFO_E                       = 389 /* 0x185 */,
  WIFIRXOLE_CCE_CLASSIFY_INFO_E            = 390 /* 0x186 */,
  WIFITCL_CCE_CLASSIFY_INFO_E              = 391 /* 0x187 */,
  WIFIRXOLE_CCE_INFO_E                     = 392 /* 0x188 */,
  WIFITCL_CCE_INFO_E                       = 393 /* 0x189 */,
  WIFITCL_CCE_SUPERRULE_E                  = 394 /* 0x18a */,
  WIFICCE_RULE_E                           = 395 /* 0x18b */,
  WIFIRX_PPDU_START_DROPPED_E              = 396 /* 0x18c */,
  WIFIRX_PPDU_END_DROPPED_E                = 397 /* 0x18d */,
  WIFIRX_PPDU_END_STATUS_DONE_DROPPED_E    = 398 /* 0x18e */,
  WIFIRX_MPDU_START_DROPPED_E              = 399 /* 0x18f */,
  WIFIRX_MSDU_START_DROPPED_E              = 400 /* 0x190 */,
  WIFIRX_MSDU_END_DROPPED_E                = 401 /* 0x191 */,
  WIFIRX_MPDU_END_DROPPED_E                = 402 /* 0x192 */,
  WIFIRX_ATTENTION_DROPPED_E               = 403 /* 0x193 */,
  WIFITXPCU_USER_SETUP_E                   = 404 /* 0x194 */,
  WIFIRXPCU_USER_SETUP_EXT_E               = 405 /* 0x195 */,
  WIFICE_SRC_DESC_E                        = 406 /* 0x196 */,
  WIFICE_STAT_DESC_E                       = 407 /* 0x197 */,
  WIFIRXOLE_CCE_SUPERRULE_E                = 408 /* 0x198 */,
  WIFITX_RATE_STATS_INFO_E                 = 409 /* 0x199 */,
  WIFICMD_PART_0_END_E                     = 410 /* 0x19a */,
  WIFIMACTX_SYNTH_ON_E                     = 411 /* 0x19b */,
  WIFISCH_CRITICAL_TLV_REFERENCE_E         = 412 /* 0x19c */,
  WIFITQM_MPDU_GLOBAL_START_E              = 413 /* 0x19d */,
  WIFIEXAMPLE_TLV_32_E                     = 414 /* 0x19e */,
  WIFITQM_UPDATE_TX_MSDU_FLOW_E            = 415 /* 0x19f */,
  WIFITQM_UPDATE_TX_MPDU_QUEUE_HEAD_E      = 416 /* 0x1a0 */,
  WIFITQM_UPDATE_TX_MSDU_FLOW_STATUS_E     = 417 /* 0x1a1 */,
  WIFITQM_UPDATE_TX_MPDU_QUEUE_HEAD_STATUS_E = 418 /* 0x1a2 */,
  WIFIREO_UPDATE_RX_REO_QUEUE_E            = 419 /* 0x1a3 */,
  WIFICE_DST_DESC_E                        = 420 /* 0x1a4 */,
  WIFITQM_MPDU_QUEUE_EMPTY_STATUS_E        = 421 /* 0x1a5 */,
  WIFITQM_2_SCH_MPDU_AVAILABLE_E           = 422 /* 0x1a6 */,
  WIFIPDG_TRIG_RESPONSE_E                  = 423 /* 0x1a7 */,
  WIFITRIGGER_RESPONSE_TX_DONE_E           = 424 /* 0x1a8 */,
  WIFIABORT_FROM_PHYRX_DETAILS_E           = 425 /* 0x1a9 */,
  WIFISCH_TQM_CMD_WRAPPER_E                = 426 /* 0x1aa */,
  WIFIMPDUS_AVAILABLE_E                    = 427 /* 0x1ab */,
  WIFIRECEIVED_RESPONSE_INFO_PART2_E       = 428 /* 0x1ac */,
  WIFIPHYRX_PKT_END_INFO_E                 = 429 /* 0x1ad */,
  WIFIPHYRX_TX_START_TIMING_E              = 430 /* 0x1ae */,
  WIFITXPCU_PREAMBLE_DONE_E                = 431 /* 0x1af */,
  WIFINDP_PREAMBLE_DONE_E                  = 432 /* 0x1b0 */,
  WIFISCH_TQM_CMD_WRAPPER_RBO_DROP_E       = 433 /* 0x1b1 */,
  WIFISCH_TQM_CMD_WRAPPER_CONT_DROP_E      = 434 /* 0x1b2 */,
  WIFIRX_PPDU_END_START_E                  = 435 /* 0x1b3 */,
  WIFIRX_PPDU_END_MIDDLE_E                 = 436 /* 0x1b4 */,
  WIFIRX_PPDU_END_LAST_E                   = 437 /* 0x1b5 */,
  WIFIMACTX_CLEAR_PREV_TX_INFO_E           = 438 /* 0x1b6 */,
  WIFITX_PUNCTURE_SETUP_E                  = 439 /* 0x1b7 */,
  WIFITX_PUNCTURE_PATTERN_E                = 440 /* 0x1b8 */,
  WIFIR2R_STATUS_END_E                     = 441 /* 0x1b9 */,
  WIFIMACTX_PREFETCH_CV_COMMON_E           = 442 /* 0x1ba */,
  WIFIEND_OF_FLUSH_MARKER_E                = 443 /* 0x1bb */,
  WIFIUPLINK_COMMON_INFO_PUNC_E            = 444 /* 0x1bc */,
  WIFIMACTX_MU_UPLINK_COMMON_PUNC_E        = 445 /* 0x1bd */,
  WIFIMACTX_MU_UPLINK_USER_SETUP_PUNC_E    = 446 /* 0x1be */,
  WIFIRECEIVED_RESPONSE_USER_7_0_E         = 447 /* 0x1bf */,
  WIFIRECEIVED_RESPONSE_USER_15_8_E        = 448 /* 0x1c0 */,
  WIFIRECEIVED_RESPONSE_USER_23_16_E       = 449 /* 0x1c1 */,
  WIFIRECEIVED_RESPONSE_USER_31_24_E       = 450 /* 0x1c2 */,
  WIFIRECEIVED_RESPONSE_USER_36_32_E       = 451 /* 0x1c3 */,
  WIFIRECEIVED_RESPONSE_USER_INFO_E        = 452 /* 0x1c4 */,
  WIFITX_LOOPBACK_SETUP_E                  = 453 /* 0x1c5 */,
  WIFITLV_BASE_E                           = 511 /* 0x1ff */

} tlv_tag_def__e; ///< tlv_tag_def Enum Type

#endif // _TLV_TAG_DEF_
