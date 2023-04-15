// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015, 2020 The Linux Foundation. All rights reserved.
 */

#include <linux/regmap.h>
#include <linux/device.h>
#include "wsa883x-registers.h"

const u8 wsa883x_reg_access[WSA883X_NUM_REGISTERS] = {
	[WSA883X_REG(WSA883X_REF_CTRL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TEST_CTL_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_BIAS_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OP_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_IREF_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ISENS_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CLK_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TEST_CTL_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_BIAS_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ADC_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DOUT_MSB)] = RD_REG,
	[WSA883X_REG(WSA883X_DOUT_LSB)] = RD_REG,
	[WSA883X_REG(WSA883X_VBAT_SNS)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ITRIM_CODE)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EN)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OVERRIDE1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OVERRIDE2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_VSENSE1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ISENSE1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ISENSE2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ISENSE_CAL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_MISC)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ADC_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ADC_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ADC_2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ADC_3)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ADC_4)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ADC_5)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ADC_6)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ADC_7)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_STATUS)] = RD_REG,
	[WSA883X_REG(WSA883X_DAC_CTRL_REG)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DAC_EN_DEBUG_REG)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DAC_OPAMP_BIAS1_REG)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DAC_OPAMP_BIAS2_REG)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DAC_VCM_CTRL_REG)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DAC_VOLTAGE_CTRL_REG)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ATEST1_REG)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ATEST2_REG)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_TOP_BIAS_REG1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_TOP_BIAS_REG2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_TOP_BIAS_REG3)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_TOP_BIAS_REG4)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_CLIP_DET_REG)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_DRV_LF_BLK_EN)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_DRV_LF_EN)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_DRV_LF_MASK_DCC_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_DRV_LF_MISC_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_DRV_LF_REG_GAIN)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_DRV_OS_CAL_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_DRV_OS_CAL_CTL1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_PWM_CLK_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_PDRV_HS_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_PDRV_LS_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_PWRSTG_DBG)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_OCP_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPKR_BBM_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PA_STATUS0)] = RD_REG,
	[WSA883X_REG(WSA883X_PA_STATUS1)] = RD_REG,
	[WSA883X_REG(WSA883X_PA_STATUS2)] = RD_REG,
	[WSA883X_REG(WSA883X_EN_CTRL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CURRENT_LIMIT)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_IBIAS1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_IBIAS2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_IBIAS3)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_LDO_PROG)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_STABILITY_CTRL1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_STABILITY_CTRL2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PWRSTAGE_CTRL1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PWRSTAGE_CTRL2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_BYPASS_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_BYPASS_2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ZX_CTRL_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ZX_CTRL_2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_MISC1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_MISC2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_GMAMP_SUP1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PWRSTAGE_CTRL3)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PWRSTAGE_CTRL4)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TEST1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPARE1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPARE2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PON_CTL_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PON_CLT_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PON_CTL_2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PON_CTL_3)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CKWD_CTL_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CKWD_CTL_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CKWD_CTL_2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CKSK_CTL_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PADSW_CTL_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TEST_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TEST_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_STATUS_0)] = RD_REG,
	[WSA883X_REG(WSA883X_STATUS_1)] = RD_REG,
	[WSA883X_REG(WSA883X_CHIP_ID0)] = RD_REG,
	[WSA883X_REG(WSA883X_CHIP_ID1)] = RD_REG,
	[WSA883X_REG(WSA883X_CHIP_ID2)] = RD_REG,
	[WSA883X_REG(WSA883X_CHIP_ID3)] = RD_REG,
	[WSA883X_REG(WSA883X_BUS_ID)] = RD_REG,
	[WSA883X_REG(WSA883X_CDC_RST_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TOP_CLK_CFG)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_PATH_MODE)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_CLK_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SWR_RESET_EN)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_RESET_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PA_FSM_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PA_FSM_TIMER0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PA_FSM_TIMER1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PA_FSM_STA)] = RD_REG,
	[WSA883X_REG(WSA883X_PA_FSM_ERR_COND)] = RD_REG,
	[WSA883X_REG(WSA883X_PA_FSM_MSK)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PA_FSM_BYP)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PA_FSM_DBG)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TADC_VALUE_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TEMP_DETECT_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TEMP_MSB)] = RD_REG,
	[WSA883X_REG(WSA883X_TEMP_LSB)] = RD_REG,
	[WSA883X_REG(WSA883X_TEMP_CONFIG0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TEMP_CONFIG1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_VBAT_ADC_FLT_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_VBAT_DIN_MSB)] = RD_REG,
	[WSA883X_REG(WSA883X_VBAT_DIN_LSB)] = RD_REG,
	[WSA883X_REG(WSA883X_VBAT_DOUT)] = RD_REG,
	[WSA883X_REG(WSA883X_SDM_PDM9_LSB)] = RD_REG,
	[WSA883X_REG(WSA883X_SDM_PDM9_MSB)] = RD_REG,
	[WSA883X_REG(WSA883X_CDC_RX_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_A1_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_A1_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_A2_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_A2_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_A3_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_A3_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_A4_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_A4_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_A5_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_A5_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_A6_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_A7_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_C_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_C_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_C_2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_C_3)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_R1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_R2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_R3)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_R4)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_R5)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_R6)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_DSM_R7)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_GAIN_PDM_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_GAIN_PDM_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CDC_SPK_GAIN_PDM_2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PDM_WD_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DEM_BYPASS_DATA0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DEM_BYPASS_DATA1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DEM_BYPASS_DATA2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DEM_BYPASS_DATA3)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_LRA_PER_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_LRA_PER_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_DELTA_THETA_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_DELTA_THETA_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_DIRECT_AMP_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_DIRECT_AMP_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP0_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP0_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP1_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP1_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP2_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP2_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP3_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP3_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP4_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP4_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP5_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP5_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP6_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP6_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP7_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PTRN_AMP7_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PER_0_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PER_2_3)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PER_4_5)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_PER_6_7)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_WAVG_STA)] = RD_REG,
	[WSA883X_REG(WSA883X_DRE_CTL_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DRE_CTL_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DRE_IDLE_DET_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CLSH_CTL_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CLSH_CTL_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CLSH_V_HD_PA)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CLSH_V_PA_MIN)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CLSH_OVRD_VAL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CLSH_HARD_MAX)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CLSH_SOFT_MAX)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_CLSH_SIG_DP)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TAGC_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TAGC_TIME)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TAGC_E2E_GAIN)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TAGC_FORCE_VAL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_VAGC_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_VAGC_TIME)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_VAGC_ATTN_LVL_1_2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_VAGC_ATTN_LVL_3)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_INTR_MODE)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_INTR_MASK0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_INTR_MASK1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_INTR_STATUS0)] = RD_REG,
	[WSA883X_REG(WSA883X_INTR_STATUS1)] = RD_REG,
	[WSA883X_REG(WSA883X_INTR_CLEAR0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_INTR_CLEAR1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_INTR_LEVEL0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_INTR_LEVEL1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_INTR_SET0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_INTR_SET1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_INTR_TEST0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_INTR_TEST1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_CTRL0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_CTRL1)] = RD_REG,
	[WSA883X_REG(WSA883X_HDRIVE_CTL_GROUP1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PIN_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PIN_CTL_OE)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PIN_WDATA_IOPAD)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PIN_STATUS)] = RD_REG,
	[WSA883X_REG(WSA883X_I2C_SLAVE_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_PDM_TEST_MODE)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ATE_TEST_MODE)] = RD_REG,
	[WSA883X_REG(WSA883X_DIG_DEBUG_MODE)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DIG_DEBUG_SEL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_DIG_DEBUG_EN)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SWR_HM_TEST0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SWR_HM_TEST1)] = RD_REG,
	[WSA883X_REG(WSA883X_SWR_PAD_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TADC_DETECT_DBG_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TADC_DEBUG_MSB)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TADC_DEBUG_LSB)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SAMPLE_EDGE_SEL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SWR_EDGE_SEL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_TEST_MODE_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_IOPAD_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ANA_CSR_DBG_ADD)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_ANA_CSR_DBG_CTL)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPARE_R)] = RD_REG,
	[WSA883X_REG(WSA883X_SPARE_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPARE_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SPARE_2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_SCODE)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_3)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_4)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_5)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_6)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_7)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_8)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_9)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_10)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_11)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_12)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_13)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_14)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_15)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_16)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_17)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_18)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_19)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_20)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_21)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_22)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_23)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_24)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_25)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_26)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_27)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_28)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_29)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_30)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_31)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_32)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_33)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_34)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_35)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_OTP_REG_63)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_0)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_1)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_2)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_3)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_4)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_5)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_6)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_7)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_8)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_9)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_10)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_11)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_12)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_13)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_14)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_15)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_16)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_17)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_18)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_19)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_20)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_21)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_22)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_23)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_24)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_25)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_26)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_27)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_28)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_29)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_30)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_31)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_32)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_33)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_34)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_35)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_36)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_37)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_38)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_39)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_40)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_41)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_42)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_43)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_44)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_45)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_46)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_47)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_48)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_49)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_50)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_51)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_52)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_53)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_54)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_55)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_56)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_57)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_58)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_59)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_60)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_61)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_62)] = RD_WR_REG,
	[WSA883X_REG(WSA883X_EMEM_63)] = RD_WR_REG,
};
