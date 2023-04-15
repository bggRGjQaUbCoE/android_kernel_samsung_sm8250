/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DT_BINDINGS_CLK_MSM_GCC_COBALT_H
#define _DT_BINDINGS_CLK_MSM_GCC_COBALT_H

#define BLSP1_QUP1_I2C_APPS_CLK_SRC				0
#define BLSP1_QUP1_SPI_APPS_CLK_SRC				1
#define BLSP1_QUP2_I2C_APPS_CLK_SRC				2
#define BLSP1_QUP2_SPI_APPS_CLK_SRC				3
#define BLSP1_QUP3_I2C_APPS_CLK_SRC				4
#define BLSP1_QUP3_SPI_APPS_CLK_SRC				5
#define BLSP1_QUP4_I2C_APPS_CLK_SRC				6
#define BLSP1_QUP4_SPI_APPS_CLK_SRC				7
#define BLSP1_QUP5_I2C_APPS_CLK_SRC				8
#define BLSP1_QUP5_SPI_APPS_CLK_SRC				9
#define BLSP1_QUP6_I2C_APPS_CLK_SRC				10
#define BLSP1_QUP6_SPI_APPS_CLK_SRC				11
#define BLSP1_UART1_APPS_CLK_SRC				12
#define BLSP1_UART2_APPS_CLK_SRC				13
#define BLSP1_UART3_APPS_CLK_SRC				14
#define BLSP2_QUP1_I2C_APPS_CLK_SRC				15
#define BLSP2_QUP1_SPI_APPS_CLK_SRC				16
#define BLSP2_QUP2_I2C_APPS_CLK_SRC				17
#define BLSP2_QUP2_SPI_APPS_CLK_SRC				18
#define BLSP2_QUP3_I2C_APPS_CLK_SRC				19
#define BLSP2_QUP3_SPI_APPS_CLK_SRC				20
#define BLSP2_QUP4_I2C_APPS_CLK_SRC				21
#define BLSP2_QUP4_SPI_APPS_CLK_SRC				22
#define BLSP2_QUP5_I2C_APPS_CLK_SRC				23
#define BLSP2_QUP5_SPI_APPS_CLK_SRC				24
#define BLSP2_QUP6_I2C_APPS_CLK_SRC				25
#define BLSP2_QUP6_SPI_APPS_CLK_SRC				26
#define BLSP2_UART1_APPS_CLK_SRC				27
#define BLSP2_UART2_APPS_CLK_SRC				28
#define BLSP2_UART3_APPS_CLK_SRC				29
#define GCC_AGGRE1_NOC_XO_CLK					30
#define GCC_AGGRE1_UFS_AXI_CLK					31
#define GCC_AGGRE1_USB3_AXI_CLK					32
#define GCC_APSS_QDSS_TSCTR_DIV2_CLK				33
#define GCC_APSS_QDSS_TSCTR_DIV8_CLK				34
#define GCC_BIMC_HMSS_AXI_CLK					35
#define GCC_BIMC_MSS_Q6_AXI_CLK					36
#define GCC_BLSP1_AHB_CLK					37
#define GCC_BLSP1_QUP1_I2C_APPS_CLK				38
#define GCC_BLSP1_QUP1_SPI_APPS_CLK				39
#define GCC_BLSP1_QUP2_I2C_APPS_CLK				40
#define GCC_BLSP1_QUP2_SPI_APPS_CLK				41
#define GCC_BLSP1_QUP3_I2C_APPS_CLK				42
#define GCC_BLSP1_QUP3_SPI_APPS_CLK				43
#define GCC_BLSP1_QUP4_I2C_APPS_CLK				44
#define GCC_BLSP1_QUP4_SPI_APPS_CLK				45
#define GCC_BLSP1_QUP5_I2C_APPS_CLK				46
#define GCC_BLSP1_QUP5_SPI_APPS_CLK				47
#define GCC_BLSP1_QUP6_I2C_APPS_CLK				48
#define GCC_BLSP1_QUP6_SPI_APPS_CLK				49
#define GCC_BLSP1_SLEEP_CLK					50
#define GCC_BLSP1_UART1_APPS_CLK				51
#define GCC_BLSP1_UART2_APPS_CLK				52
#define GCC_BLSP1_UART3_APPS_CLK				53
#define GCC_BLSP2_AHB_CLK					54
#define GCC_BLSP2_QUP1_I2C_APPS_CLK				55
#define GCC_BLSP2_QUP1_SPI_APPS_CLK				56
#define GCC_BLSP2_QUP2_I2C_APPS_CLK				57
#define GCC_BLSP2_QUP2_SPI_APPS_CLK				58
#define GCC_BLSP2_QUP3_I2C_APPS_CLK				59
#define GCC_BLSP2_QUP3_SPI_APPS_CLK				60
#define GCC_BLSP2_QUP4_I2C_APPS_CLK				61
#define GCC_BLSP2_QUP4_SPI_APPS_CLK				62
#define GCC_BLSP2_QUP5_I2C_APPS_CLK				63
#define GCC_BLSP2_QUP5_SPI_APPS_CLK				64
#define GCC_BLSP2_QUP6_I2C_APPS_CLK				65
#define GCC_BLSP2_QUP6_SPI_APPS_CLK				66
#define GCC_BLSP2_SLEEP_CLK					67
#define GCC_BLSP2_UART1_APPS_CLK				68
#define GCC_BLSP2_UART2_APPS_CLK				69
#define GCC_BLSP2_UART3_APPS_CLK				70
#define GCC_CFG_NOC_USB3_AXI_CLK				71
#define GCC_GP1_CLK						72
#define GCC_GP2_CLK						73
#define GCC_GP3_CLK						74
#define GCC_GPU_BIMC_GFX_CLK					75
#define GCC_GPU_BIMC_GFX_SRC_CLK				76
#define GCC_GPU_CFG_AHB_CLK					77
#define GCC_GPU_SNOC_DVM_GFX_CLK				78
#define GCC_HMSS_AHB_CLK					79
#define GCC_HMSS_AT_CLK						80
#define GCC_HMSS_DVM_BUS_CLK					81
#define GCC_HMSS_RBCPR_CLK					82
#define GCC_HMSS_TRIG_CLK					83
#define GCC_LPASS_AT_CLK					84
#define GCC_LPASS_TRIG_CLK					85
#define GCC_MMSS_NOC_CFG_AHB_CLK				86
#define GCC_MMSS_QM_AHB_CLK					87
#define GCC_MMSS_QM_CORE_CLK					88
#define GCC_MMSS_SYS_NOC_AXI_CLK				89
#define GCC_MSS_AT_CLK						90
#define GCC_PCIE_0_AUX_CLK					91
#define GCC_PCIE_0_CFG_AHB_CLK					92
#define GCC_PCIE_0_MSTR_AXI_CLK					93
#define GCC_PCIE_0_PIPE_CLK					94
#define GCC_PCIE_0_SLV_AXI_CLK					95
#define GCC_PCIE_PHY_AUX_CLK					96
#define GCC_PDM2_CLK						97
#define GCC_PDM_AHB_CLK						98
#define GCC_PDM_XO4_CLK						99
#define GCC_PRNG_AHB_CLK					100
#define GCC_SDCC2_AHB_CLK					101
#define GCC_SDCC2_APPS_CLK					102
#define GCC_SDCC4_AHB_CLK					103
#define GCC_SDCC4_APPS_CLK					104
#define GCC_TSIF_AHB_CLK					105
#define GCC_TSIF_INACTIVITY_TIMERS_CLK				106
#define GCC_TSIF_REF_CLK					107
#define GCC_UFS_AHB_CLK						108
#define GCC_UFS_AXI_CLK						109
#define GCC_UFS_ICE_CORE_CLK					110
#define GCC_UFS_PHY_AUX_CLK					111
#define GCC_UFS_RX_SYMBOL_0_CLK					112
#define GCC_UFS_RX_SYMBOL_1_CLK					113
#define GCC_UFS_TX_SYMBOL_0_CLK					114
#define GCC_UFS_UNIPRO_CORE_CLK					115
#define GCC_USB30_MASTER_CLK					116
#define GCC_USB30_MOCK_UTMI_CLK					117
#define GCC_USB30_SLEEP_CLK					118
#define GCC_USB3_PHY_AUX_CLK					119
#define GCC_USB3_PHY_PIPE_CLK					120
#define GCC_USB_PHY_CFG_AHB2PHY_CLK				121
#define GP1_CLK_SRC						122
#define GP2_CLK_SRC						123
#define GP3_CLK_SRC						124
#define GPLL0							125
#define GPLL0_OUT_EVEN						126
#define GPLL0_OUT_MAIN						127
#define GPLL0_OUT_ODD						128
#define GPLL0_OUT_TEST						129
#define GPLL1							130
#define GPLL1_OUT_EVEN						131
#define GPLL1_OUT_MAIN						132
#define GPLL1_OUT_ODD						133
#define GPLL1_OUT_TEST						134
#define GPLL2							135
#define GPLL2_OUT_EVEN						136
#define GPLL2_OUT_MAIN						137
#define GPLL2_OUT_ODD						138
#define GPLL2_OUT_TEST						139
#define GPLL3							140
#define GPLL3_OUT_EVEN						141
#define GPLL3_OUT_MAIN						142
#define GPLL3_OUT_ODD						143
#define GPLL3_OUT_TEST						144
#define GPLL4							145
#define GPLL4_OUT_EVEN						146
#define GPLL4_OUT_MAIN						147
#define GPLL4_OUT_ODD						148
#define GPLL4_OUT_TEST						149
#define GPLL6							150
#define GPLL6_OUT_EVEN						151
#define GPLL6_OUT_MAIN						152
#define GPLL6_OUT_ODD						153
#define GPLL6_OUT_TEST						154
#define HMSS_AHB_CLK_SRC					155
#define HMSS_RBCPR_CLK_SRC					156
#define PCIE_AUX_CLK_SRC					157
#define PDM2_CLK_SRC						158
#define SDCC2_APPS_CLK_SRC					159
#define SDCC4_APPS_CLK_SRC					160
#define TSIF_REF_CLK_SRC					161
#define UFS_AXI_CLK_SRC						162
#define USB30_MASTER_CLK_SRC					163
#define USB30_MOCK_UTMI_CLK_SRC					164
#define USB3_PHY_AUX_CLK_SRC					165

#define PCIE_0_GDSC						0
#define UFS_GDSC						1
#define USB_30_GDSC						2

#define GCC_BLSP1_QUP1_BCR					0
#define GCC_BLSP1_QUP2_BCR					1
#define GCC_BLSP1_QUP3_BCR					2
#define GCC_BLSP1_QUP4_BCR					3
#define GCC_BLSP1_QUP5_BCR					4
#define GCC_BLSP1_QUP6_BCR					5
#define GCC_BLSP2_QUP1_BCR					6
#define GCC_BLSP2_QUP2_BCR					7
#define GCC_BLSP2_QUP3_BCR					8
#define GCC_BLSP2_QUP4_BCR					9
#define GCC_BLSP2_QUP5_BCR					10
#define GCC_BLSP2_QUP6_BCR					11
#define GCC_PCIE_0_BCR						12
#define GCC_PDM_BCR						13
#define GCC_SDCC2_BCR						14
#define GCC_SDCC4_BCR						15
#define GCC_TSIF_BCR						16
#define GCC_UFS_BCR						17
#define GCC_USB_30_BCR						18

#endif
