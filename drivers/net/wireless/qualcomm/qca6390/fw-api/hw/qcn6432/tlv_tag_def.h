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

#ifndef _TLV_TAG_DEF_
#define _TLV_TAG_DEF_

typedef enum tlv_tag_def{
  WIFIMACTX_CBF_START_E                                    = 0 /* 0x0 */,
  WIFIPHYRX_DATA_E                                         = 1 /* 0x1 */,
  WIFIPHYRX_CBF_DATA_RESP_E                                = 2 /* 0x2 */,
  WIFIPHYRX_ABORT_REQUEST_E                                = 3 /* 0x3 */,
  WIFIPHYRX_USER_ABORT_NOTIFICATION_E                      = 4 /* 0x4 */,
  WIFIMACTX_DATA_RESP_E                                    = 5 /* 0x5 */,
  WIFIMACTX_CBF_DATA_E                                     = 6 /* 0x6 */,
  WIFIMACTX_CBF_DONE_E                                     = 7 /* 0x7 */,
  WIFIPHYRX_LMR_DATA_RESP_E                                = 8 /* 0x8 */,
  WIFIRXPCU_TO_UCODE_START_E                               = 9 /* 0x9 */,
  WIFIRXPCU_TO_UCODE_DELIMITER_FOR_FULL_MPDU_E             = 10 /* 0xa */,
  WIFIRXPCU_TO_UCODE_FULL_MPDU_DATA_E                      = 11 /* 0xb */,
  WIFIRXPCU_TO_UCODE_FCS_STATUS_E                          = 12 /* 0xc */,
  WIFIRXPCU_TO_UCODE_MPDU_DELIMITER_E                      = 13 /* 0xd */,
  WIFIRXPCU_TO_UCODE_DELIMITER_FOR_MPDU_HEADER_E           = 14 /* 0xe */,
  WIFIRXPCU_TO_UCODE_MPDU_HEADER_DATA_E                    = 15 /* 0xf */,
  WIFIRXPCU_TO_UCODE_END_E                                 = 16 /* 0x10 */,
  WIFIMACRX_CBF_READ_REQUEST_E                             = 32 /* 0x20 */,
  WIFIMACRX_CBF_DATA_REQUEST_E                             = 33 /* 0x21 */,
  WIFIMACRX_EXPECT_NDP_RECEPTION_E                         = 34 /* 0x22 */,
  WIFIMACRX_FREEZE_CAPTURE_CHANNEL_E                       = 35 /* 0x23 */,
  WIFIMACRX_NDP_TIMEOUT_E                                  = 36 /* 0x24 */,
  WIFIMACRX_ABORT_ACK_E                                    = 37 /* 0x25 */,
  WIFIMACRX_REQ_IMPLICIT_FB_E                              = 38 /* 0x26 */,
  WIFIMACRX_CHAIN_MASK_E                                   = 39 /* 0x27 */,
  WIFIMACRX_NAP_USER_E                                     = 40 /* 0x28 */,
  WIFIMACRX_ABORT_REQUEST_E                                = 41 /* 0x29 */,
  WIFIPHYTX_OTHER_TRANSMIT_INFO16_E                        = 42 /* 0x2a */,
  WIFIPHYTX_ABORT_ACK_E                                    = 43 /* 0x2b */,
  WIFIPHYTX_ABORT_REQUEST_E                                = 44 /* 0x2c */,
  WIFIPHYTX_PKT_END_E                                      = 45 /* 0x2d */,
  WIFIPHYTX_PPDU_HEADER_INFO_REQUEST_E                     = 46 /* 0x2e */,
  WIFIPHYTX_REQUEST_CTRL_INFO_E                            = 47 /* 0x2f */,
  WIFIPHYTX_DATA_REQUEST_E                                 = 48 /* 0x30 */,
  WIFIPHYTX_BF_CV_LOADING_DONE_E                           = 49 /* 0x31 */,
  WIFIPHYTX_NAP_ACK_E                                      = 50 /* 0x32 */,
  WIFIPHYTX_NAP_DONE_E                                     = 51 /* 0x33 */,
  WIFIPHYTX_OFF_ACK_E                                      = 52 /* 0x34 */,
  WIFIPHYTX_ON_ACK_E                                       = 53 /* 0x35 */,
  WIFIPHYTX_SYNTH_OFF_ACK_E                                = 54 /* 0x36 */,
  WIFIPHYTX_DEBUG16_E                                      = 55 /* 0x37 */,
  WIFIMACTX_ABORT_REQUEST_E                                = 56 /* 0x38 */,
  WIFIMACTX_ABORT_ACK_E                                    = 57 /* 0x39 */,
  WIFIMACTX_PKT_END_E                                      = 58 /* 0x3a */,
  WIFIMACTX_PRE_PHY_DESC_E                                 = 59 /* 0x3b */,
  WIFIMACTX_BF_PARAMS_COMMON_E                             = 60 /* 0x3c */,
  WIFIMACTX_BF_PARAMS_PER_USER_E                           = 61 /* 0x3d */,
  WIFIMACTX_PREFETCH_CV_E                                  = 62 /* 0x3e */,
  WIFIMACTX_USER_DESC_COMMON_E                             = 63 /* 0x3f */,
  WIFIMACTX_USER_DESC_PER_USER_E                           = 64 /* 0x40 */,
  WIFIEXAMPLE_USER_TLV_16_E                                = 65 /* 0x41 */,
  WIFIEXAMPLE_TLV_16_E                                     = 66 /* 0x42 */,
  WIFIMACTX_PHY_OFF_E                                      = 67 /* 0x43 */,
  WIFIMACTX_PHY_ON_E                                       = 68 /* 0x44 */,
  WIFIMACTX_SYNTH_OFF_E                                    = 69 /* 0x45 */,
  WIFIMACTX_EXPECT_CBF_COMMON_E                            = 70 /* 0x46 */,
  WIFIMACTX_EXPECT_CBF_PER_USER_E                          = 71 /* 0x47 */,
  WIFIMACTX_PHY_DESC_E                                     = 72 /* 0x48 */,
  WIFIMACTX_L_SIG_A_E                                      = 73 /* 0x49 */,
  WIFIMACTX_L_SIG_B_E                                      = 74 /* 0x4a */,
  WIFIMACTX_HT_SIG_E                                       = 75 /* 0x4b */,
  WIFIMACTX_VHT_SIG_A_E                                    = 76 /* 0x4c */,
  WIFIMACTX_VHT_SIG_B_SU20_E                               = 77 /* 0x4d */,
  WIFIMACTX_VHT_SIG_B_SU40_E                               = 78 /* 0x4e */,
  WIFIMACTX_VHT_SIG_B_SU80_E                               = 79 /* 0x4f */,
  WIFIMACTX_VHT_SIG_B_SU160_E                              = 80 /* 0x50 */,
  WIFIMACTX_VHT_SIG_B_MU20_E                               = 81 /* 0x51 */,
  WIFIMACTX_VHT_SIG_B_MU40_E                               = 82 /* 0x52 */,
  WIFIMACTX_VHT_SIG_B_MU80_E                               = 83 /* 0x53 */,
  WIFIMACTX_VHT_SIG_B_MU160_E                              = 84 /* 0x54 */,
  WIFIMACTX_SERVICE_E                                      = 85 /* 0x55 */,
  WIFIMACTX_HE_SIG_A_SU_E                                  = 86 /* 0x56 */,
  WIFIMACTX_HE_SIG_A_MU_DL_E                               = 87 /* 0x57 */,
  WIFIMACTX_HE_SIG_A_MU_UL_E                               = 88 /* 0x58 */,
  WIFIMACTX_HE_SIG_B1_MU_E                                 = 89 /* 0x59 */,
  WIFIMACTX_HE_SIG_B2_MU_E                                 = 90 /* 0x5a */,
  WIFIMACTX_HE_SIG_B2_OFDMA_E                              = 91 /* 0x5b */,
  WIFIMACTX_DELETE_CV_E                                    = 92 /* 0x5c */,
  WIFIMACTX_MU_UPLINK_COMMON_E                             = 93 /* 0x5d */,
  WIFIMACTX_MU_UPLINK_USER_SETUP_E                         = 94 /* 0x5e */,
  WIFIMACTX_OTHER_TRANSMIT_INFO_E                          = 95 /* 0x5f */,
  WIFIMACTX_PHY_NAP_E                                      = 96 /* 0x60 */,
  WIFIMACTX_DEBUG_E                                        = 97 /* 0x61 */,
  WIFIPHYRX_ABORT_ACK_E                                    = 98 /* 0x62 */,
  WIFIPHYRX_GENERATED_CBF_DETAILS_E                        = 99 /* 0x63 */,
  WIFIPHYRX_RSSI_LEGACY_E                                  = 100 /* 0x64 */,
  WIFIPHYRX_RSSI_HT_E                                      = 101 /* 0x65 */,
  WIFIPHYRX_USER_INFO_E                                    = 102 /* 0x66 */,
  WIFIPHYRX_PKT_END_E                                      = 103 /* 0x67 */,
  WIFIPHYRX_DEBUG_E                                        = 104 /* 0x68 */,
  WIFIPHYRX_CBF_TRANSFER_DONE_E                            = 105 /* 0x69 */,
  WIFIPHYRX_CBF_TRANSFER_ABORT_E                           = 106 /* 0x6a */,
  WIFIPHYRX_L_SIG_A_E                                      = 107 /* 0x6b */,
  WIFIPHYRX_L_SIG_B_E                                      = 108 /* 0x6c */,
  WIFIPHYRX_HT_SIG_E                                       = 109 /* 0x6d */,
  WIFIPHYRX_VHT_SIG_A_E                                    = 110 /* 0x6e */,
  WIFIPHYRX_VHT_SIG_B_SU20_E                               = 111 /* 0x6f */,
  WIFIPHYRX_VHT_SIG_B_SU40_E                               = 112 /* 0x70 */,
  WIFIPHYRX_VHT_SIG_B_SU80_E                               = 113 /* 0x71 */,
  WIFIPHYRX_VHT_SIG_B_SU160_E                              = 114 /* 0x72 */,
  WIFIPHYRX_VHT_SIG_B_MU20_E                               = 115 /* 0x73 */,
  WIFIPHYRX_VHT_SIG_B_MU40_E                               = 116 /* 0x74 */,
  WIFIPHYRX_VHT_SIG_B_MU80_E                               = 117 /* 0x75 */,
  WIFIPHYRX_VHT_SIG_B_MU160_E                              = 118 /* 0x76 */,
  WIFIPHYRX_HE_SIG_A_SU_E                                  = 119 /* 0x77 */,
  WIFIPHYRX_HE_SIG_A_MU_DL_E                               = 120 /* 0x78 */,
  WIFIPHYRX_HE_SIG_A_MU_UL_E                               = 121 /* 0x79 */,
  WIFIPHYRX_HE_SIG_B1_MU_E                                 = 122 /* 0x7a */,
  WIFIPHYRX_HE_SIG_B2_MU_E                                 = 123 /* 0x7b */,
  WIFIPHYRX_HE_SIG_B2_OFDMA_E                              = 124 /* 0x7c */,
  WIFIPHYRX_OTHER_RECEIVE_INFO_E                           = 125 /* 0x7d */,
  WIFIPHYRX_COMMON_USER_INFO_E                             = 126 /* 0x7e */,
  WIFIPHYRX_DATA_DONE_E                                    = 127 /* 0x7f */,
  WIFICOEX_TX_REQ_E                                        = 128 /* 0x80 */,
  WIFIDUMMY_E                                              = 129 /* 0x81 */,
  WIFIEXAMPLE_TLV_32_NAME_E                                = 130 /* 0x82 */,
  WIFIMPDU_LIMIT_E                                         = 131 /* 0x83 */,
  WIFINA_LENGTH_END_E                                      = 132 /* 0x84 */,
  WIFIOLE_BUF_STATUS_E                                     = 133 /* 0x85 */,
  WIFIPCU_PPDU_SETUP_DONE_E                                = 134 /* 0x86 */,
  WIFIPCU_PPDU_SETUP_END_E                                 = 135 /* 0x87 */,
  WIFIPCU_PPDU_SETUP_INIT_E                                = 136 /* 0x88 */,
  WIFIPCU_PPDU_SETUP_START_E                               = 137 /* 0x89 */,
  WIFIPDG_FES_SETUP_E                                      = 138 /* 0x8a */,
  WIFIPDG_RESPONSE_E                                       = 139 /* 0x8b */,
  WIFIPDG_TX_REQ_E                                         = 140 /* 0x8c */,
  WIFISCH_WAIT_INSTR_E                                     = 141 /* 0x8d */,
  WIFITQM_FLOW_EMPTY_STATUS_E                              = 143 /* 0x8f */,
  WIFITQM_FLOW_NOT_EMPTY_STATUS_E                          = 144 /* 0x90 */,
  WIFITQM_GEN_MPDU_LENGTH_LIST_E                           = 145 /* 0x91 */,
  WIFITQM_GEN_MPDU_LENGTH_LIST_STATUS_E                    = 146 /* 0x92 */,
  WIFITQM_GEN_MPDUS_E                                      = 147 /* 0x93 */,
  WIFITQM_GEN_MPDUS_STATUS_E                               = 148 /* 0x94 */,
  WIFITQM_REMOVE_MPDU_E                                    = 149 /* 0x95 */,
  WIFITQM_REMOVE_MPDU_STATUS_E                             = 150 /* 0x96 */,
  WIFITQM_REMOVE_MSDU_E                                    = 151 /* 0x97 */,
  WIFITQM_REMOVE_MSDU_STATUS_E                             = 152 /* 0x98 */,
  WIFITQM_UPDATE_TX_MPDU_COUNT_E                           = 153 /* 0x99 */,
  WIFITQM_WRITE_CMD_E                                      = 154 /* 0x9a */,
  WIFIOFDMA_TRIGGER_DETAILS_E                              = 155 /* 0x9b */,
  WIFITX_DATA_E                                            = 156 /* 0x9c */,
  WIFITX_FES_SETUP_E                                       = 157 /* 0x9d */,
  WIFIRX_PACKET_E                                          = 158 /* 0x9e */,
  WIFIEXPECTED_RESPONSE_E                                  = 159 /* 0x9f */,
  WIFITX_MPDU_END_E                                        = 160 /* 0xa0 */,
  WIFITX_MPDU_START_E                                      = 161 /* 0xa1 */,
  WIFITX_MSDU_END_E                                        = 162 /* 0xa2 */,
  WIFITX_MSDU_START_E                                      = 163 /* 0xa3 */,
  WIFITX_SW_MODE_SETUP_E                                   = 164 /* 0xa4 */,
  WIFITXPCU_BUFFER_STATUS_E                                = 165 /* 0xa5 */,
  WIFITXPCU_USER_BUFFER_STATUS_E                           = 166 /* 0xa6 */,
  WIFIDATA_TO_TIME_CONFIG_E                                = 167 /* 0xa7 */,
  WIFIEXAMPLE_USER_TLV_32_E                                = 168 /* 0xa8 */,
  WIFIMPDU_INFO_E                                          = 169 /* 0xa9 */,
  WIFIPDG_USER_SETUP_E                                     = 170 /* 0xaa */,
  WIFITX_11AH_SETUP_E                                      = 171 /* 0xab */,
  WIFIREO_UPDATE_RX_REO_QUEUE_STATUS_E                     = 172 /* 0xac */,
  WIFITX_PEER_ENTRY_E                                      = 173 /* 0xad */,
  WIFITX_RAW_OR_NATIVE_FRAME_SETUP_E                       = 174 /* 0xae */,
  WIFIEXAMPLE_USER_TLV_44_E                                = 175 /* 0xaf */,
  WIFITX_FLUSH_E                                           = 176 /* 0xb0 */,
  WIFITX_FLUSH_REQ_E                                       = 177 /* 0xb1 */,
  WIFITQM_WRITE_CMD_STATUS_E                               = 178 /* 0xb2 */,
  WIFITQM_GET_MPDU_QUEUE_STATS_E                           = 179 /* 0xb3 */,
  WIFITQM_GET_MSDU_FLOW_STATS_E                            = 180 /* 0xb4 */,
  WIFIEXAMPLE_USER_CTLV_44_E                               = 181 /* 0xb5 */,
  WIFITX_FES_STATUS_START_E                                = 182 /* 0xb6 */,
  WIFITX_FES_STATUS_USER_PPDU_E                            = 183 /* 0xb7 */,
  WIFITX_FES_STATUS_USER_RESPONSE_E                        = 184 /* 0xb8 */,
  WIFITX_FES_STATUS_END_E                                  = 185 /* 0xb9 */,
  WIFIRX_TRIG_INFO_E                                       = 186 /* 0xba */,
  WIFIRXPCU_TX_SETUP_CLEAR_E                               = 187 /* 0xbb */,
  WIFIRX_FRAME_BITMAP_REQ_E                                = 188 /* 0xbc */,
  WIFIRX_FRAME_BITMAP_ACK_E                                = 189 /* 0xbd */,
  WIFICOEX_RX_STATUS_E                                     = 190 /* 0xbe */,
  WIFIRX_START_PARAM_E                                     = 191 /* 0xbf */,
  WIFIRX_PPDU_START_E                                      = 192 /* 0xc0 */,
  WIFIRX_PPDU_END_E                                        = 193 /* 0xc1 */,
  WIFIRX_MPDU_START_E                                      = 194 /* 0xc2 */,
  WIFIRX_MPDU_END_E                                        = 195 /* 0xc3 */,
  WIFIRX_MSDU_START_E                                      = 196 /* 0xc4 */,
  WIFIRX_MSDU_END_E                                        = 197 /* 0xc5 */,
  WIFIRX_ATTENTION_E                                       = 198 /* 0xc6 */,
  WIFIRECEIVED_RESPONSE_INFO_E                             = 199 /* 0xc7 */,
  WIFIRX_PHY_SLEEP_E                                       = 200 /* 0xc8 */,
  WIFIRX_HEADER_E                                          = 201 /* 0xc9 */,
  WIFIRX_PEER_ENTRY_E                                      = 202 /* 0xca */,
  WIFIRX_FLUSH_E                                           = 203 /* 0xcb */,
  WIFIRX_RESPONSE_REQUIRED_INFO_E                          = 204 /* 0xcc */,
  WIFIRX_FRAMELESS_BAR_DETAILS_E                           = 205 /* 0xcd */,
  WIFITQM_GET_MPDU_QUEUE_STATS_STATUS_E                    = 206 /* 0xce */,
  WIFITQM_GET_MSDU_FLOW_STATS_STATUS_E                     = 207 /* 0xcf */,
  WIFITX_CBF_INFO_E                                        = 208 /* 0xd0 */,
  WIFIPCU_PPDU_SETUP_USER_E                                = 209 /* 0xd1 */,
  WIFIRX_MPDU_PCU_START_E                                  = 210 /* 0xd2 */,
  WIFIRX_PM_INFO_E                                         = 211 /* 0xd3 */,
  WIFIRX_USER_PPDU_END_E                                   = 212 /* 0xd4 */,
  WIFIRX_PRE_PPDU_START_E                                  = 213 /* 0xd5 */,
  WIFIRX_PREAMBLE_E                                        = 214 /* 0xd6 */,
  WIFITX_FES_SETUP_COMPLETE_E                              = 215 /* 0xd7 */,
  WIFITX_LAST_MPDU_FETCHED_E                               = 216 /* 0xd8 */,
  WIFITXDMA_STOP_REQUEST_E                                 = 217 /* 0xd9 */,
  WIFIRXPCU_SETUP_E                                        = 218 /* 0xda */,
  WIFIRXPCU_USER_SETUP_E                                   = 219 /* 0xdb */,
  WIFITX_FES_STATUS_ACK_OR_BA_E                            = 220 /* 0xdc */,
  WIFITQM_ACKED_MPDU_E                                     = 221 /* 0xdd */,
  WIFICOEX_TX_RESP_E                                       = 222 /* 0xde */,
  WIFICOEX_TX_STATUS_E                                     = 223 /* 0xdf */,
  WIFIMACTX_COEX_PHY_CTRL_E                                = 224 /* 0xe0 */,
  WIFICOEX_STATUS_BROADCAST_E                              = 225 /* 0xe1 */,
  WIFIRESPONSE_START_STATUS_E                              = 226 /* 0xe2 */,
  WIFIRESPONSE_END_STATUS_E                                = 227 /* 0xe3 */,
  WIFICRYPTO_STATUS_E                                      = 228 /* 0xe4 */,
  WIFIRECEIVED_TRIGGER_INFO_E                              = 229 /* 0xe5 */,
  WIFICOEX_TX_STOP_CTRL_E                                  = 230 /* 0xe6 */,
  WIFIRX_PPDU_ACK_REPORT_E                                 = 231 /* 0xe7 */,
  WIFIRX_PPDU_NO_ACK_REPORT_E                              = 232 /* 0xe8 */,
  WIFISCH_COEX_STATUS_E                                    = 233 /* 0xe9 */,
  WIFISCHEDULER_COMMAND_STATUS_E                           = 234 /* 0xea */,
  WIFISCHEDULER_RX_PPDU_NO_RESPONSE_STATUS_E               = 235 /* 0xeb */,
  WIFITX_FES_STATUS_PROT_E                                 = 236 /* 0xec */,
  WIFITX_FES_STATUS_START_PPDU_E                           = 237 /* 0xed */,
  WIFITX_FES_STATUS_START_PROT_E                           = 238 /* 0xee */,
  WIFITXPCU_PHYTX_DEBUG32_E                                = 239 /* 0xef */,
  WIFITXPCU_PHYTX_OTHER_TRANSMIT_INFO32_E                  = 240 /* 0xf0 */,
  WIFITX_MPDU_COUNT_TRANSFER_END_E                         = 241 /* 0xf1 */,
  WIFIWHO_ANCHOR_OFFSET_E                                  = 242 /* 0xf2 */,
  WIFIWHO_ANCHOR_VALUE_E                                   = 243 /* 0xf3 */,
  WIFIWHO_CCE_INFO_E                                       = 244 /* 0xf4 */,
  WIFIWHO_COMMIT_E                                         = 245 /* 0xf5 */,
  WIFIWHO_COMMIT_DONE_E                                    = 246 /* 0xf6 */,
  WIFIWHO_FLUSH_E                                          = 247 /* 0xf7 */,
  WIFIWHO_L2_LLC_E                                         = 248 /* 0xf8 */,
  WIFIWHO_L2_PAYLOAD_E                                     = 249 /* 0xf9 */,
  WIFIWHO_L3_CHECKSUM_E                                    = 250 /* 0xfa */,
  WIFIWHO_L3_INFO_E                                        = 251 /* 0xfb */,
  WIFIWHO_L4_CHECKSUM_E                                    = 252 /* 0xfc */,
  WIFIWHO_L4_INFO_E                                        = 253 /* 0xfd */,
  WIFIWHO_MSDU_E                                           = 254 /* 0xfe */,
  WIFIWHO_MSDU_MISC_E                                      = 255 /* 0xff */,
  WIFIWHO_PACKET_DATA_E                                    = 256 /* 0x100 */,
  WIFIWHO_PACKET_HDR_E                                     = 257 /* 0x101 */,
  WIFIWHO_PPDU_END_E                                       = 258 /* 0x102 */,
  WIFIWHO_PPDU_START_E                                     = 259 /* 0x103 */,
  WIFIWHO_TSO_E                                            = 260 /* 0x104 */,
  WIFIWHO_WMAC_HEADER_PV0_E                                = 261 /* 0x105 */,
  WIFIWHO_WMAC_HEADER_PV1_E                                = 262 /* 0x106 */,
  WIFIWHO_WMAC_IV_E                                        = 263 /* 0x107 */,
  WIFIMPDU_INFO_END_E                                      = 264 /* 0x108 */,
  WIFIMPDU_INFO_BITMAP_E                                   = 265 /* 0x109 */,
  WIFITX_QUEUE_EXTENSION_E                                 = 266 /* 0x10a */,
  WIFISCHEDULER_SELFGEN_RESPONSE_STATUS_E                  = 267 /* 0x10b */,
  WIFITQM_UPDATE_TX_MPDU_COUNT_STATUS_E                    = 268 /* 0x10c */,
  WIFITQM_ACKED_MPDU_STATUS_E                              = 269 /* 0x10d */,
  WIFITQM_ADD_MSDU_STATUS_E                                = 270 /* 0x10e */,
  WIFITQM_LIST_GEN_DONE_E                                  = 271 /* 0x10f */,
  WIFIWHO_TERMINATE_E                                      = 272 /* 0x110 */,
  WIFITX_LAST_MPDU_END_E                                   = 273 /* 0x111 */,
  WIFITX_CV_DATA_E                                         = 274 /* 0x112 */,
  WIFIPPDU_TX_END_E                                        = 275 /* 0x113 */,
  WIFIPROT_TX_END_E                                        = 276 /* 0x114 */,
  WIFIMPDU_INFO_GLOBAL_END_E                               = 277 /* 0x115 */,
  WIFITQM_SCH_INSTR_GLOBAL_END_E                           = 278 /* 0x116 */,
  WIFIRX_PPDU_END_USER_STATS_E                             = 279 /* 0x117 */,
  WIFIRX_PPDU_END_USER_STATS_EXT_E                         = 280 /* 0x118 */,
  WIFIREO_GET_QUEUE_STATS_E                                = 281 /* 0x119 */,
  WIFIREO_FLUSH_QUEUE_E                                    = 282 /* 0x11a */,
  WIFIREO_FLUSH_CACHE_E                                    = 283 /* 0x11b */,
  WIFIREO_UNBLOCK_CACHE_E                                  = 284 /* 0x11c */,
  WIFIREO_GET_QUEUE_STATS_STATUS_E                         = 285 /* 0x11d */,
  WIFIREO_FLUSH_QUEUE_STATUS_E                             = 286 /* 0x11e */,
  WIFIREO_FLUSH_CACHE_STATUS_E                             = 287 /* 0x11f */,
  WIFIREO_UNBLOCK_CACHE_STATUS_E                           = 288 /* 0x120 */,
  WIFITQM_FLUSH_CACHE_E                                    = 289 /* 0x121 */,
  WIFITQM_UNBLOCK_CACHE_E                                  = 290 /* 0x122 */,
  WIFITQM_FLUSH_CACHE_STATUS_E                             = 291 /* 0x123 */,
  WIFITQM_UNBLOCK_CACHE_STATUS_E                           = 292 /* 0x124 */,
  WIFIRX_PPDU_END_STATUS_DONE_E                            = 293 /* 0x125 */,
  WIFIRX_STATUS_BUFFER_DONE_E                              = 294 /* 0x126 */,
  WIFISCHEDULER_MLO_SW_MSG_STATUS_E                        = 295 /* 0x127 */,
  WIFISCHEDULER_TXOP_DURATION_TRIGGER_E                    = 296 /* 0x128 */,
  WIFITX_DATA_SYNC_E                                       = 297 /* 0x129 */,
  WIFIPHYRX_CBF_READ_REQUEST_ACK_E                         = 298 /* 0x12a */,
  WIFITQM_GET_MPDU_HEAD_INFO_E                             = 299 /* 0x12b */,
  WIFITQM_SYNC_CMD_E                                       = 300 /* 0x12c */,
  WIFITQM_GET_MPDU_HEAD_INFO_STATUS_E                      = 301 /* 0x12d */,
  WIFITQM_SYNC_CMD_STATUS_E                                = 302 /* 0x12e */,
  WIFITQM_THRESHOLD_DROP_NOTIFICATION_STATUS_E             = 303 /* 0x12f */,
  WIFITQM_DESCRIPTOR_THRESHOLD_REACHED_STATUS_E            = 304 /* 0x130 */,
  WIFIREO_FLUSH_TIMEOUT_LIST_E                             = 305 /* 0x131 */,
  WIFIREO_FLUSH_TIMEOUT_LIST_STATUS_E                      = 306 /* 0x132 */,
  WIFIREO_DESCRIPTOR_THRESHOLD_REACHED_STATUS_E            = 307 /* 0x133 */,
  WIFISCHEDULER_RX_SIFS_RESPONSE_TRIGGER_STATUS_E          = 308 /* 0x134 */,
  WIFIEXAMPLE_USER_TLV_32_NAME_E                           = 309 /* 0x135 */,
  WIFIRX_PPDU_START_USER_INFO_E                            = 310 /* 0x136 */,
  WIFIRX_RING_MASK_E                                       = 311 /* 0x137 */,
  WIFICOEX_MAC_NAP_E                                       = 312 /* 0x138 */,
  WIFIRXPCU_PPDU_END_INFO_E                                = 313 /* 0x139 */,
  WIFIWHO_MESH_CONTROL_E                                   = 314 /* 0x13a */,
  WIFIPDG_SW_MODE_BW_START_E                               = 315 /* 0x13b */,
  WIFIPDG_SW_MODE_BW_END_E                                 = 316 /* 0x13c */,
  WIFIPDG_WAIT_FOR_MAC_REQUEST_E                           = 317 /* 0x13d */,
  WIFIPDG_WAIT_FOR_PHY_REQUEST_E                           = 318 /* 0x13e */,
  WIFISCHEDULER_END_E                                      = 319 /* 0x13f */,
  WIFIRX_PPDU_START_DROPPED_E                              = 320 /* 0x140 */,
  WIFIRX_PPDU_END_DROPPED_E                                = 321 /* 0x141 */,
  WIFIRX_PPDU_END_STATUS_DONE_DROPPED_E                    = 322 /* 0x142 */,
  WIFIRX_MPDU_START_DROPPED_E                              = 323 /* 0x143 */,
  WIFIRX_MSDU_START_DROPPED_E                              = 324 /* 0x144 */,
  WIFIRX_MSDU_END_DROPPED_E                                = 325 /* 0x145 */,
  WIFIRX_MPDU_END_DROPPED_E                                = 326 /* 0x146 */,
  WIFIRX_ATTENTION_DROPPED_E                               = 327 /* 0x147 */,
  WIFITXPCU_USER_SETUP_E                                   = 328 /* 0x148 */,
  WIFIRXPCU_USER_SETUP_EXT_E                               = 329 /* 0x149 */,
  WIFICMD_PART_0_END_E                                     = 330 /* 0x14a */,
  WIFIMACTX_SYNTH_ON_E                                     = 331 /* 0x14b */,
  WIFISCH_CRITICAL_TLV_REFERENCE_E                         = 332 /* 0x14c */,
  WIFITQM_MPDU_GLOBAL_START_E                              = 333 /* 0x14d */,
  WIFIEXAMPLE_TLV_32_E                                     = 334 /* 0x14e */,
  WIFITQM_UPDATE_TX_MSDU_FLOW_E                            = 335 /* 0x14f */,
  WIFITQM_UPDATE_TX_MPDU_QUEUE_HEAD_E                      = 336 /* 0x150 */,
  WIFITQM_UPDATE_TX_MSDU_FLOW_STATUS_E                     = 337 /* 0x151 */,
  WIFITQM_UPDATE_TX_MPDU_QUEUE_HEAD_STATUS_E               = 338 /* 0x152 */,
  WIFIREO_UPDATE_RX_REO_QUEUE_E                            = 339 /* 0x153 */,
  WIFITQM_MPDU_QUEUE_EMPTY_STATUS_E                        = 340 /* 0x154 */,
  WIFITQM_2_SCH_MPDU_AVAILABLE_E                           = 341 /* 0x155 */,
  WIFIPDG_TRIG_RESPONSE_E                                  = 342 /* 0x156 */,
  WIFITRIGGER_RESPONSE_TX_DONE_E                           = 343 /* 0x157 */,
  WIFIABORT_FROM_PHYRX_DETAILS_E                           = 344 /* 0x158 */,
  WIFISCH_TQM_CMD_WRAPPER_E                                = 345 /* 0x159 */,
  WIFIMPDUS_AVAILABLE_E                                    = 346 /* 0x15a */,
  WIFIRECEIVED_RESPONSE_INFO_PART2_E                       = 347 /* 0x15b */,
  WIFIPHYRX_TX_START_TIMING_E                              = 348 /* 0x15c */,
  WIFITXPCU_PREAMBLE_DONE_E                                = 349 /* 0x15d */,
  WIFINDP_PREAMBLE_DONE_E                                  = 350 /* 0x15e */,
  WIFISCH_TQM_CMD_WRAPPER_RBO_DROP_E                       = 351 /* 0x15f */,
  WIFISCH_TQM_CMD_WRAPPER_CONT_DROP_E                      = 352 /* 0x160 */,
  WIFIMACTX_CLEAR_PREV_TX_INFO_E                           = 353 /* 0x161 */,
  WIFITX_PUNCTURE_SETUP_E                                  = 354 /* 0x162 */,
  WIFIR2R_STATUS_END_E                                     = 355 /* 0x163 */,
  WIFIMACTX_PREFETCH_CV_COMMON_E                           = 356 /* 0x164 */,
  WIFIEND_OF_FLUSH_MARKER_E                                = 357 /* 0x165 */,
  WIFIMACTX_MU_UPLINK_COMMON_PUNC_E                        = 358 /* 0x166 */,
  WIFIMACTX_MU_UPLINK_USER_SETUP_PUNC_E                    = 359 /* 0x167 */,
  WIFIRECEIVED_RESPONSE_USER_7_0_E                         = 360 /* 0x168 */,
  WIFIRECEIVED_RESPONSE_USER_15_8_E                        = 361 /* 0x169 */,
  WIFIRECEIVED_RESPONSE_USER_23_16_E                       = 362 /* 0x16a */,
  WIFIRECEIVED_RESPONSE_USER_31_24_E                       = 363 /* 0x16b */,
  WIFIRECEIVED_RESPONSE_USER_36_32_E                       = 364 /* 0x16c */,
  WIFITX_LOOPBACK_SETUP_E                                  = 365 /* 0x16d */,
  WIFIPHYRX_OTHER_RECEIVE_INFO_RU_DETAILS_E                = 366 /* 0x16e */,
  WIFISCH_WAIT_INSTR_TX_PATH_E                             = 367 /* 0x16f */,
  WIFIMACTX_OTHER_TRANSMIT_INFO_TX2TX_E                    = 368 /* 0x170 */,
  WIFIMACTX_OTHER_TRANSMIT_INFO_EMUPHY_SETUP_E             = 369 /* 0x171 */,
  WIFIPHYRX_OTHER_RECEIVE_INFO_EVM_DETAILS_E               = 370 /* 0x172 */,
  WIFITX_WUR_DATA_E                                        = 371 /* 0x173 */,
  WIFIRX_PPDU_END_START_E                                  = 372 /* 0x174 */,
  WIFIRX_PPDU_END_MIDDLE_E                                 = 373 /* 0x175 */,
  WIFIRX_PPDU_END_LAST_E                                   = 374 /* 0x176 */,
  WIFIMACTX_BACKOFF_BASED_TRANSMISSION_E                   = 375 /* 0x177 */,
  WIFIMACTX_OTHER_TRANSMIT_INFO_DL_OFDMA_TX_E              = 376 /* 0x178 */,
  WIFISRP_INFO_E                                           = 377 /* 0x179 */,
  WIFIOBSS_SR_INFO_E                                       = 378 /* 0x17a */,
  WIFISCHEDULER_SW_MSG_STATUS_E                            = 379 /* 0x17b */,
  WIFIHWSCH_RXPCU_MAC_INFO_ANNOUNCEMENT_E                  = 380 /* 0x17c */,
  WIFIRXPCU_SETUP_COMPLETE_E                               = 381 /* 0x17d */,
  WIFISNOOP_PPDU_START_E                                   = 382 /* 0x17e */,
  WIFISNOOP_MPDU_USR_DBG_INFO_E                            = 383 /* 0x17f */,
  WIFISNOOP_MSDU_USR_DBG_INFO_E                            = 384 /* 0x180 */,
  WIFISNOOP_MSDU_USR_DATA_E                                = 385 /* 0x181 */,
  WIFISNOOP_MPDU_USR_STAT_INFO_E                           = 386 /* 0x182 */,
  WIFISNOOP_PPDU_END_E                                     = 387 /* 0x183 */,
  WIFISNOOP_SPARE_E                                        = 388 /* 0x184 */,
  WIFILMR_TX_END_E                                         = 389 /* 0x185 */,
  WIFIPHYRX_OTHER_RECEIVE_INFO_MU_RSSI_COMMON_E            = 390 /* 0x186 */,
  WIFIPHYRX_OTHER_RECEIVE_INFO_MU_RSSI_USER_E              = 391 /* 0x187 */,
  WIFIMACTX_OTHER_TRANSMIT_INFO_SCH_DETAILS_E              = 392 /* 0x188 */,
  WIFIPHYRX_OTHER_RECEIVE_INFO_108P_EVM_DETAILS_E          = 393 /* 0x189 */,
  WIFISCH_TLV_WRAPPER_E                                    = 394 /* 0x18a */,
  WIFISCHEDULER_STATUS_WRAPPER_E                           = 395 /* 0x18b */,
  WIFIMPDU_INFO_6X_E                                       = 396 /* 0x18c */,
  WIFIMACTX_11AZ_USER_DESC_PER_USER_E                      = 397 /* 0x18d */,
  WIFIMACTX_U_SIG_EHT_SU_MU_E                              = 398 /* 0x18e */,
  WIFIMACTX_U_SIG_EHT_TB_E                                 = 399 /* 0x18f */,
  WIFICOEX_TLV_ACC_TLV_TAG0_CFG_E                          = 400 /* 0x190 */,
  WIFICOEX_TLV_ACC_TLV_TAG1_CFG_E                          = 401 /* 0x191 */,
  WIFICOEX_TLV_ACC_TLV_TAG2_CFG_E                          = 402 /* 0x192 */,
  WIFIPHYRX_U_SIG_EHT_SU_MU_E                              = 403 /* 0x193 */,
  WIFIPHYRX_U_SIG_EHT_TB_E                                 = 404 /* 0x194 */,
  WIFICOEX_TLV_ACC_TLV_TAG3_CFG_E                          = 405 /* 0x195 */,
  WIFICOEX_TLV_ACC_TLV_TAG_CGIM_CFG_E                      = 406 /* 0x196 */,
  WIFITX_PUNCTURE_6PATTERNS_SETUP_E                        = 407 /* 0x197 */,
  WIFIMACRX_LMR_READ_REQUEST_E                             = 408 /* 0x198 */,
  WIFIMACRX_LMR_DATA_REQUEST_E                             = 409 /* 0x199 */,
  WIFIPHYRX_LMR_TRANSFER_DONE_E                            = 410 /* 0x19a */,
  WIFIPHYRX_LMR_TRANSFER_ABORT_E                           = 411 /* 0x19b */,
  WIFIPHYRX_LMR_READ_REQUEST_ACK_E                         = 412 /* 0x19c */,
  WIFIMACRX_SECURE_LTF_SEQ_PTR_E                           = 413 /* 0x19d */,
  WIFIPHYRX_USER_INFO_MU_UL_E                              = 414 /* 0x19e */,
  WIFIMPDU_QUEUE_OVERVIEW_E                                = 415 /* 0x19f */,
  WIFISCHEDULER_NAV_INFO_E                                 = 416 /* 0x1a0 */,
  WIFILMR_PEER_ENTRY_E                                     = 418 /* 0x1a2 */,
  WIFILMR_MPDU_START_E                                     = 419 /* 0x1a3 */,
  WIFILMR_DATA_E                                           = 420 /* 0x1a4 */,
  WIFILMR_MPDU_END_E                                       = 421 /* 0x1a5 */,
  WIFIREO_GET_QUEUE_1K_STATS_STATUS_E                      = 422 /* 0x1a6 */,
  WIFIRX_FRAME_1K_BITMAP_ACK_E                             = 423 /* 0x1a7 */,
  WIFITX_FES_STATUS_1K_BA_E                                = 424 /* 0x1a8 */,
  WIFITQM_ACKED_1K_MPDU_E                                  = 425 /* 0x1a9 */,
  WIFIMACRX_INBSS_OBSS_IND_E                               = 426 /* 0x1aa */,
  WIFIPHYRX_LOCATION_E                                     = 427 /* 0x1ab */,
  WIFIMLO_TX_NOTIFICATION_SU_E                             = 428 /* 0x1ac */,
  WIFIMLO_TX_NOTIFICATION_MU_E                             = 429 /* 0x1ad */,
  WIFIMLO_TX_REQ_SU_E                                      = 430 /* 0x1ae */,
  WIFIMLO_TX_REQ_MU_E                                      = 431 /* 0x1af */,
  WIFIMLO_TX_RESP_E                                        = 432 /* 0x1b0 */,
  WIFIMLO_RX_NOTIFICATION_E                                = 433 /* 0x1b1 */,
  WIFIMLO_BKOFF_TRUNC_REQ_E                                = 434 /* 0x1b2 */,
  WIFIMLO_TBTT_NOTIFICATION_E                              = 435 /* 0x1b3 */,
  WIFIMLO_MESSAGE_E                                        = 436 /* 0x1b4 */,
  WIFIMLO_TS_SYNC_MSG_E                                    = 437 /* 0x1b5 */,
  WIFIMLO_FES_SETUP_E                                      = 438 /* 0x1b6 */,
  WIFIMLO_PDG_FES_SETUP_SU_E                               = 439 /* 0x1b7 */,
  WIFIMLO_PDG_FES_SETUP_MU_E                               = 440 /* 0x1b8 */,
  WIFIMPDU_INFO_1K_BITMAP_E                                = 441 /* 0x1b9 */,
  WIFIMON_BUFFER_ADDR_E                                    = 442 /* 0x1ba */,
  WIFITX_FRAG_STATE_E                                      = 443 /* 0x1bb */,
  WIFIMACTX_OTHER_TRANSMIT_INFO_PHY_CV_RESET_E             = 444 /* 0x1bc */,
  WIFIMACTX_OTHER_TRANSMIT_INFO_SW_PEER_IDS_E              = 445 /* 0x1bd */,
  WIFIMACTX_EHT_SIG_USR_OFDMA_E                            = 446 /* 0x1be */,
  WIFIPHYRX_EHT_SIG_CMN_PUNC_E                             = 448 /* 0x1c0 */,
  WIFIPHYRX_EHT_SIG_CMN_OFDMA_E                            = 450 /* 0x1c2 */,
  WIFIPHYRX_EHT_SIG_USR_OFDMA_E                            = 454 /* 0x1c6 */,
  WIFIPHYRX_PKT_END_PART1_E                                = 456 /* 0x1c8 */,
  WIFIMACTX_EXPECT_NDP_RECEPTION_E                         = 457 /* 0x1c9 */,
  WIFIMACTX_SECURE_LTF_SEQ_PTR_E                           = 458 /* 0x1ca */,
  WIFIMLO_PDG_BKOFF_TRUNC_NOTIFY_E                         = 460 /* 0x1cc */,
  WIFIPHYRX_11AZ_INTEGRITY_DATA_E                          = 461 /* 0x1cd */,
  WIFIPHYTX_LOCATION_E                                     = 462 /* 0x1ce */,
  WIFIPHYTX_11AZ_INTEGRITY_DATA_E                          = 463 /* 0x1cf */,
  WIFIMACTX_EHT_SIG_USR_SU_E                               = 466 /* 0x1d2 */,
  WIFIMACTX_EHT_SIG_USR_MU_MIMO_E                          = 467 /* 0x1d3 */,
  WIFIPHYRX_EHT_SIG_USR_SU_E                               = 468 /* 0x1d4 */,
  WIFIPHYRX_EHT_SIG_USR_MU_MIMO_E                          = 469 /* 0x1d5 */,
  WIFIPHYRX_GENERIC_U_SIG_E                                = 470 /* 0x1d6 */,
  WIFIPHYRX_GENERIC_EHT_SIG_E                              = 471 /* 0x1d7 */,
  WIFIOVERWRITE_RESP_START_E                               = 472 /* 0x1d8 */,
  WIFIOVERWRITE_RESP_PREAMBLE_INFO_E                       = 473 /* 0x1d9 */,
  WIFIOVERWRITE_RESP_FRAME_INFO_E                          = 474 /* 0x1da */,
  WIFIOVERWRITE_RESP_END_E                                 = 475 /* 0x1db */,
  WIFIRXPCU_EARLY_RX_INDICATION_E                          = 476 /* 0x1dc */,
  WIFIMON_DROP_E                                           = 477 /* 0x1dd */,
  WIFIMACRX_MU_UPLINK_COMMON_SNIFF_E                       = 478 /* 0x1de */,
  WIFIMACRX_MU_UPLINK_USER_SETUP_SNIFF_E                   = 479 /* 0x1df */,
  WIFIMACRX_MU_UPLINK_USER_SEL_SNIFF_E                     = 480 /* 0x1e0 */,
  WIFIMACRX_MU_UPLINK_FCS_STATUS_SNIFF_E                   = 481 /* 0x1e1 */,
  WIFIMACTX_PREFETCH_CV_DMA_E                              = 482 /* 0x1e2 */,
  WIFIMACTX_PREFETCH_CV_PER_USER_E                         = 483 /* 0x1e3 */,
  WIFIPHYRX_OTHER_RECEIVE_INFO_ALL_SIGB_DETAILS_E          = 484 /* 0x1e4 */,
  WIFIMACTX_BF_PARAMS_UPDATE_COMMON_E                      = 485 /* 0x1e5 */,
  WIFIMACTX_BF_PARAMS_UPDATE_PER_USER_E                    = 486 /* 0x1e6 */,
  WIFIRANGING_USER_DETAILS_E                               = 487 /* 0x1e7 */,
  WIFIPHYTX_CV_CORR_STATUS_E                               = 488 /* 0x1e8 */,
  WIFIPHYTX_CV_CORR_COMMON_E                               = 489 /* 0x1e9 */,
  WIFIPHYTX_CV_CORR_USER_E                                 = 490 /* 0x1ea */,
  WIFIMACTX_CV_CORR_COMMON_E                               = 491 /* 0x1eb */,
  WIFIMACTX_CV_CORR_MAC_INFO_GROUP_E                       = 492 /* 0x1ec */,
  WIFIBW_PUNCTURE_EVAL_WRAPPER_E                           = 493 /* 0x1ed */,
  WIFIMACTX_RX_NOTIFICATION_FOR_PHY_E                      = 494 /* 0x1ee */,
  WIFIMACTX_TX_NOTIFICATION_FOR_PHY_E                      = 495 /* 0x1ef */,
  WIFIMACTX_MU_UPLINK_COMMON_PER_BW_E                      = 496 /* 0x1f0 */,
  WIFIMACTX_MU_UPLINK_USER_SETUP_PER_BW_E                  = 497 /* 0x1f1 */,
  WIFIRX_PPDU_END_USER_STATS_EXT2_E                        = 498 /* 0x1f2 */,
  WIFIFW2SW_MON_E                                          = 499 /* 0x1f3 */,
  WIFIWSI_DIRECT_MESSAGE_E                                 = 500 /* 0x1f4 */,
  WIFIMACTX_EMLSR_PRE_SWITCH_E                             = 501 /* 0x1f5 */,
  WIFIMACTX_EMLSR_SWITCH_E                                 = 502 /* 0x1f6 */,
  WIFIMACTX_EMLSR_SWITCH_BACK_E                            = 503 /* 0x1f7 */,
  WIFIPHYTX_EMLSR_SWITCH_ACK_E                             = 504 /* 0x1f8 */,
  WIFIPHYTX_EMLSR_SWITCH_BACK_ACK_E                        = 505 /* 0x1f9 */,
  WIFISPARE_REUSE_TAG_0_E                                  = 506 /* 0x1fa */,
  WIFISPARE_REUSE_TAG_1_E                                  = 507 /* 0x1fb */,
  WIFISPARE_REUSE_TAG_2_E                                  = 508 /* 0x1fc */,
  WIFISPARE_REUSE_TAG_3_E                                  = 509 /* 0x1fd */
} tlv_tag_def__e;


#endif
