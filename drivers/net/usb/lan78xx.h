/*
 * Copyright (C) 2015 Microchip Technology
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _LAN78XX_H
#define _LAN78XX_H

/* USB Vendor Requests */
#define USB_VENDOR_REQUEST_WRITE_REGISTER	0xA0
#define USB_VENDOR_REQUEST_READ_REGISTER	0xA1
#define USB_VENDOR_REQUEST_GET_STATS		0xA2

/* Interrupt Endpoint status word bitfields */
#define INT_ENP_EEE_START_TX_LPI_INT		BIT(26)
#define INT_ENP_EEE_STOP_TX_LPI_INT		BIT(25)
#define INT_ENP_EEE_RX_LPI_INT			BIT(24)
#define INT_ENP_RDFO_INT			BIT(22)
#define INT_ENP_TXE_INT				BIT(21)
#define INT_ENP_TX_DIS_INT			BIT(19)
#define INT_ENP_RX_DIS_INT			BIT(18)
#define INT_ENP_PHY_INT				BIT(17)
#define INT_ENP_DP_INT				BIT(16)
#define INT_ENP_MAC_ERR_INT			BIT(15)
#define INT_ENP_TDFU_INT			BIT(14)
#define INT_ENP_TDFO_INT			BIT(13)
#define INT_ENP_UTX_FP_INT			BIT(12)

#define TX_PKT_ALIGNMENT			4
#define RX_PKT_ALIGNMENT			4

/* Tx Command A */
#define TX_CMD_A_IGE_			(0x20000000)
#define TX_CMD_A_ICE_			(0x10000000)
#define TX_CMD_A_LSO_			(0x08000000)
#define TX_CMD_A_IPE_			(0x04000000)
#define TX_CMD_A_TPE_			(0x02000000)
#define TX_CMD_A_IVTG_			(0x01000000)
#define TX_CMD_A_RVTG_			(0x00800000)
#define TX_CMD_A_FCS_			(0x00400000)
#define TX_CMD_A_LEN_MASK_		(0x000FFFFF)

/* Tx Command B */
#define TX_CMD_B_MSS_SHIFT_		(16)
#define TX_CMD_B_MSS_MASK_		(0x3FFF0000)
#define TX_CMD_B_MSS_MIN_		((unsigned short)8)
#define TX_CMD_B_VTAG_MASK_		(0x0000FFFF)
#define TX_CMD_B_VTAG_PRI_MASK_		(0x0000E000)
#define TX_CMD_B_VTAG_CFI_MASK_		(0x00001000)
#define TX_CMD_B_VTAG_VID_MASK_		(0x00000FFF)

/* Rx Command A */
#define RX_CMD_A_ICE_			(0x80000000)
#define RX_CMD_A_TCE_			(0x40000000)
#define RX_CMD_A_CSE_MASK_		(0xC0000000)
#define RX_CMD_A_IPV_			(0x20000000)
#define RX_CMD_A_PID_MASK_		(0x18000000)
#define RX_CMD_A_PID_NONE_IP_		(0x00000000)
#define RX_CMD_A_PID_TCP_IP_		(0x08000000)
#define RX_CMD_A_PID_UDP_IP_		(0x10000000)
#define RX_CMD_A_PID_IP_		(0x18000000)
#define RX_CMD_A_PFF_			(0x04000000)
#define RX_CMD_A_BAM_			(0x02000000)
#define RX_CMD_A_MAM_			(0x01000000)
#define RX_CMD_A_FVTG_			(0x00800000)
#define RX_CMD_A_RED_			(0x00400000)
#define RX_CMD_A_RX_ERRS_MASK_		(0xC03F0000)
#define RX_CMD_A_RWT_			(0x00200000)
#define RX_CMD_A_RUNT_			(0x00100000)
#define RX_CMD_A_LONG_			(0x00080000)
#define RX_CMD_A_RXE_			(0x00040000)
#define RX_CMD_A_DRB_			(0x00020000)
#define RX_CMD_A_FCS_			(0x00010000)
#define RX_CMD_A_UAM_			(0x00008000)
#define RX_CMD_A_ICSM_			(0x00004000)
#define RX_CMD_A_LEN_MASK_		(0x00003FFF)

/* Rx Command B */
#define RX_CMD_B_CSUM_SHIFT_		(16)
#define RX_CMD_B_CSUM_MASK_		(0xFFFF0000)
#define RX_CMD_B_VTAG_MASK_		(0x0000FFFF)
#define RX_CMD_B_VTAG_PRI_MASK_		(0x0000E000)
#define RX_CMD_B_VTAG_CFI_MASK_		(0x00001000)
#define RX_CMD_B_VTAG_VID_MASK_		(0x00000FFF)

/* Rx Command C */
#define RX_CMD_C_WAKE_SHIFT_		(15)
#define RX_CMD_C_WAKE_			(0x8000)
#define RX_CMD_C_REF_FAIL_SHIFT_	(14)
#define RX_CMD_C_REF_FAIL_		(0x4000)

/* SCSRs */
#define NUMBER_OF_REGS			(193)

#define ID_REV				(0x00)
#define ID_REV_CHIP_ID_MASK_		(0xFFFF0000)
#define ID_REV_CHIP_REV_MASK_		(0x0000FFFF)
#define ID_REV_CHIP_ID_7800_		(0x7800)
#define ID_REV_CHIP_ID_7850_		(0x7850)
#define ID_REV_CHIP_ID_7801_		(0x7801)

#define FPGA_REV			(0x04)
#define FPGA_REV_MINOR_MASK_		(0x0000FF00)
#define FPGA_REV_MAJOR_MASK_		(0x000000FF)

#define INT_STS				(0x0C)
#define INT_STS_CLEAR_ALL_		(0xFFFFFFFF)
#define INT_STS_EEE_TX_LPI_STRT_	(0x04000000)
#define INT_STS_EEE_TX_LPI_STOP_	(0x02000000)
#define INT_STS_EEE_RX_LPI_		(0x01000000)
#define INT_STS_RDFO_			(0x00400000)
#define INT_STS_TXE_			(0x00200000)
#define INT_STS_TX_DIS_			(0x00080000)
#define INT_STS_RX_DIS_			(0x00040000)
#define INT_STS_PHY_INT_		(0x00020000)
#define INT_STS_DP_INT_			(0x00010000)
#define INT_STS_MAC_ERR_		(0x00008000)
#define INT_STS_TDFU_			(0x00004000)
#define INT_STS_TDFO_			(0x00002000)
#define INT_STS_UFX_FP_			(0x00001000)
#define INT_STS_GPIO_MASK_		(0x00000FFF)
#define INT_STS_GPIO11_			(0x00000800)
#define INT_STS_GPIO10_			(0x00000400)
#define INT_STS_GPIO9_			(0x00000200)
#define INT_STS_GPIO8_			(0x00000100)
#define INT_STS_GPIO7_			(0x00000080)
#define INT_STS_GPIO6_			(0x00000040)
#define INT_STS_GPIO5_			(0x00000020)
#define INT_STS_GPIO4_			(0x00000010)
#define INT_STS_GPIO3_			(0x00000008)
#define INT_STS_GPIO2_			(0x00000004)
#define INT_STS_GPIO1_			(0x00000002)
#define INT_STS_GPIO0_			(0x00000001)

#define HW_CFG				(0x010)
#define HW_CFG_CLK125_EN_		(0x02000000)
#define HW_CFG_REFCLK25_EN_		(0x01000000)
#define HW_CFG_LED3_EN_			(0x00800000)
#define HW_CFG_LED2_EN_			(0x00400000)
#define HW_CFG_LED1_EN_			(0x00200000)
#define HW_CFG_LED0_EN_			(0x00100000)
#define HW_CFG_EEE_PHY_LUSU_		(0x00020000)
#define HW_CFG_EEE_TSU_			(0x00010000)
#define HW_CFG_NETDET_STS_		(0x00008000)
#define HW_CFG_NETDET_EN_		(0x00004000)
#define HW_CFG_EEM_			(0x00002000)
#define HW_CFG_RST_PROTECT_		(0x00001000)
#define HW_CFG_CONNECT_BUF_		(0x00000400)
#define HW_CFG_CONNECT_EN_		(0x00000200)
#define HW_CFG_CONNECT_POL_		(0x00000100)
#define HW_CFG_SUSPEND_N_SEL_MASK_	(0x000000C0)
#define HW_CFG_SUSPEND_N_SEL_2		(0x00000000)
#define HW_CFG_SUSPEND_N_SEL_12N	(0x00000040)
#define HW_CFG_SUSPEND_N_SEL_012N	(0x00000080)
#define HW_CFG_SUSPEND_N_SEL_0123N	(0x000000C0)
#define HW_CFG_SUSPEND_N_POL_		(0x00000020)
#define HW_CFG_MEF_			(0x00000010)
#define HW_CFG_ETC_			(0x00000008)
#define HW_CFG_LRST_			(0x00000002)
#define HW_CFG_SRST_			(0x00000001)

#define PMT_CTL				(0x014)
#define PMT_CTL_EEE_WAKEUP_EN_		(0x00002000)
#define PMT_CTL_EEE_WUPS_		(0x00001000)
#define PMT_CTL_MAC_SRST_		(0x00000800)
#define PMT_CTL_PHY_PWRUP_		(0x00000400)
#define PMT_CTL_RES_CLR_WKP_MASK_	(0x00000300)
#define PMT_CTL_RES_CLR_WKP_STS_	(0x00000200)
#define PMT_CTL_RES_CLR_WKP_EN_		(0x00000100)
#define PMT_CTL_READY_			(0x00000080)
#define PMT_CTL_SUS_MODE_MASK_		(0x00000060)
#define PMT_CTL_SUS_MODE_0_		(0x00000000)
#define PMT_CTL_SUS_MODE_1_		(0x00000020)
#define PMT_CTL_SUS_MODE_2_		(0x00000040)
#define PMT_CTL_SUS_MODE_3_		(0x00000060)
#define PMT_CTL_PHY_RST_		(0x00000010)
#define PMT_CTL_WOL_EN_			(0x00000008)
#define PMT_CTL_PHY_WAKE_EN_		(0x00000004)
#define PMT_CTL_WUPS_MASK_		(0x00000003)
#define PMT_CTL_WUPS_MLT_		(0x00000003)
#define PMT_CTL_WUPS_MAC_		(0x00000002)
#define PMT_CTL_WUPS_PHY_		(0x00000001)

#define GPIO_CFG0			(0x018)
#define GPIO_CFG0_GPIOEN_MASK_		(0x0000F000)
#define GPIO_CFG0_GPIOEN3_		(0x00008000)
#define GPIO_CFG0_GPIOEN2_		(0x00004000)
#define GPIO_CFG0_GPIOEN1_		(0x00002000)
#define GPIO_CFG0_GPIOEN0_		(0x00001000)
#define GPIO_CFG0_GPIOBUF_MASK_		(0x00000F00)
#define GPIO_CFG0_GPIOBUF3_		(0x00000800)
#define GPIO_CFG0_GPIOBUF2_		(0x00000400)
#define GPIO_CFG0_GPIOBUF1_		(0x00000200)
#define GPIO_CFG0_GPIOBUF0_		(0x00000100)
#define GPIO_CFG0_GPIODIR_MASK_		(0x000000F0)
#define GPIO_CFG0_GPIODIR3_		(0x00000080)
#define GPIO_CFG0_GPIODIR2_		(0x00000040)
#define GPIO_CFG0_GPIODIR1_		(0x00000020)
#define GPIO_CFG0_GPIODIR0_		(0x00000010)
#define GPIO_CFG0_GPIOD_MASK_		(0x0000000F)
#define GPIO_CFG0_GPIOD3_		(0x00000008)
#define GPIO_CFG0_GPIOD2_		(0x00000004)
#define GPIO_CFG0_GPIOD1_		(0x00000002)
#define GPIO_CFG0_GPIOD0_		(0x00000001)

#define GPIO_CFG1			(0x01C)
#define GPIO_CFG1_GPIOEN_MASK_		(0xFF000000)
#define GPIO_CFG1_GPIOEN11_		(0x80000000)
#define GPIO_CFG1_GPIOEN10_		(0x40000000)
#define GPIO_CFG1_GPIOEN9_		(0x20000000)
#define GPIO_CFG1_GPIOEN8_		(0x10000000)
#define GPIO_CFG1_GPIOEN7_		(0x08000000)
#define GPIO_CFG1_GPIOEN6_		(0x04000000)
#define GPIO_CFG1_GPIOEN5_		(0x02000000)
#define GPIO_CFG1_GPIOEN4_		(0x01000000)
#define GPIO_CFG1_GPIOBUF_MASK_		(0x00FF0000)
#define GPIO_CFG1_GPIOBUF11_		(0x00800000)
#define GPIO_CFG1_GPIOBUF10_		(0x00400000)
#define GPIO_CFG1_GPIOBUF9_		(0x00200000)
#define GPIO_CFG1_GPIOBUF8_		(0x00100000)
#define GPIO_CFG1_GPIOBUF7_		(0x00080000)
#define GPIO_CFG1_GPIOBUF6_		(0x00040000)
#define GPIO_CFG1_GPIOBUF5_		(0x00020000)
#define GPIO_CFG1_GPIOBUF4_		(0x00010000)
#define GPIO_CFG1_GPIODIR_MASK_		(0x0000FF00)
#define GPIO_CFG1_GPIODIR11_		(0x00008000)
#define GPIO_CFG1_GPIODIR10_		(0x00004000)
#define GPIO_CFG1_GPIODIR9_		(0x00002000)
#define GPIO_CFG1_GPIODIR8_		(0x00001000)
#define GPIO_CFG1_GPIODIR7_		(0x00000800)
#define GPIO_CFG1_GPIODIR6_		(0x00000400)
#define GPIO_CFG1_GPIODIR5_		(0x00000200)
#define GPIO_CFG1_GPIODIR4_		(0x00000100)
#define GPIO_CFG1_GPIOD_MASK_		(0x000000FF)
#define GPIO_CFG1_GPIOD11_		(0x00000080)
#define GPIO_CFG1_GPIOD10_		(0x00000040)
#define GPIO_CFG1_GPIOD9_		(0x00000020)
#define GPIO_CFG1_GPIOD8_		(0x00000010)
#define GPIO_CFG1_GPIOD7_		(0x00000008)
#define GPIO_CFG1_GPIOD6_		(0x00000004)
#define GPIO_CFG1_GPIOD6_		(0x00000004)
#define GPIO_CFG1_GPIOD5_		(0x00000002)
#define GPIO_CFG1_GPIOD4_		(0x00000001)

#define GPIO_WAKE			(0x020)
#define GPIO_WAKE_GPIOPOL_MASK_		(0x0FFF0000)
#define GPIO_WAKE_GPIOPOL11_		(0x08000000)
#define GPIO_WAKE_GPIOPOL10_		(0x04000000)
#define GPIO_WAKE_GPIOPOL9_		(0x02000000)
#define GPIO_WAKE_GPIOPOL8_		(0x01000000)
#define GPIO_WAKE_GPIOPOL7_		(0x00800000)
#define GPIO_WAKE_GPIOPOL6_		(0x00400000)
#define GPIO_WAKE_GPIOPOL5_		(0x00200000)
#define GPIO_WAKE_GPIOPOL4_		(0x00100000)
#define GPIO_WAKE_GPIOPOL3_		(0x00080000)
#define GPIO_WAKE_GPIOPOL2_		(0x00040000)
#define GPIO_WAKE_GPIOPOL1_		(0x00020000)
#define GPIO_WAKE_GPIOPOL0_		(0x00010000)
#define GPIO_WAKE_GPIOWK_MASK_		(0x00000FFF)
#define GPIO_WAKE_GPIOWK11_		(0x00000800)
#define GPIO_WAKE_GPIOWK10_		(0x00000400)
#define GPIO_WAKE_GPIOWK9_		(0x00000200)
#define GPIO_WAKE_GPIOWK8_		(0x00000100)
#define GPIO_WAKE_GPIOWK7_		(0x00000080)
#define GPIO_WAKE_GPIOWK6_		(0x00000040)
#define GPIO_WAKE_GPIOWK5_		(0x00000020)
#define GPIO_WAKE_GPIOWK4_		(0x00000010)
#define GPIO_WAKE_GPIOWK3_		(0x00000008)
#define GPIO_WAKE_GPIOWK2_		(0x00000004)
#define GPIO_WAKE_GPIOWK1_		(0x00000002)
#define GPIO_WAKE_GPIOWK0_		(0x00000001)

#define DP_SEL				(0x024)
#define DP_SEL_DPRDY_			(0x80000000)
#define DP_SEL_RSEL_MASK_		(0x0000000F)
#define DP_SEL_RSEL_USB_PHY_CSRS_	(0x0000000F)
#define DP_SEL_RSEL_OTP_64BIT_		(0x00000009)
#define DP_SEL_RSEL_OTP_8BIT_		(0x00000008)
#define DP_SEL_RSEL_UTX_BUF_RAM_	(0x00000007)
#define DP_SEL_RSEL_DESC_RAM_		(0x00000005)
#define DP_SEL_RSEL_TXFIFO_		(0x00000004)
#define DP_SEL_RSEL_RXFIFO_		(0x00000003)
#define DP_SEL_RSEL_LSO_		(0x00000002)
#define DP_SEL_RSEL_VLAN_DA_		(0x00000001)
#define DP_SEL_RSEL_URXBUF_		(0x00000000)
#define DP_SEL_VHF_HASH_LEN		(16)
#define DP_SEL_VHF_VLAN_LEN		(128)

#define DP_CMD				(0x028)
#define DP_CMD_WRITE_			(0x00000001)
#define DP_CMD_READ_			(0x00000000)

#define DP_ADDR				(0x02C)
#define DP_ADDR_MASK_			(0x00003FFF)

#define DP_DATA				(0x030)

#define E2P_CMD				(0x040)
#define E2P_CMD_EPC_BUSY_		(0x80000000)
#define E2P_CMD_EPC_CMD_MASK_		(0x70000000)
#define E2P_CMD_EPC_CMD_RELOAD_		(0x70000000)
#define E2P_CMD_EPC_CMD_ERAL_		(0x60000000)
#define E2P_CMD_EPC_CMD_ERASE_		(0x50000000)
#define E2P_CMD_EPC_CMD_WRAL_		(0x40000000)
#define E2P_CMD_EPC_CMD_WRITE_		(0x30000000)
#define E2P_CMD_EPC_CMD_EWEN_		(0x20000000)
#define E2P_CMD_EPC_CMD_EWDS_		(0x10000000)
#define E2P_CMD_EPC_CMD_READ_		(0x00000000)
#define E2P_CMD_EPC_TIMEOUT_		(0x00000400)
#define E2P_CMD_EPC_DL_			(0x00000200)
#define E2P_CMD_EPC_ADDR_MASK_		(0x000001FF)

#define E2P_DATA			(0x044)
#define E2P_DATA_EEPROM_DATA_MASK_	(0x000000FF)

#define BOS_ATTR			(0x050)
#define BOS_ATTR_BLOCK_SIZE_MASK_	(0x000000FF)

#define SS_ATTR				(0x054)
#define SS_ATTR_POLL_INT_MASK_		(0x00FF0000)
#define SS_ATTR_DEV_DESC_SIZE_MASK_	(0x0000FF00)
#define SS_ATTR_CFG_BLK_SIZE_MASK_	(0x000000FF)

#define HS_ATTR				(0x058)
#define HS_ATTR_POLL_INT_MASK_		(0x00FF0000)
#define HS_ATTR_DEV_DESC_SIZE_MASK_	(0x0000FF00)
#define HS_ATTR_CFG_BLK_SIZE_MASK_	(0x000000FF)

#define FS_ATTR				(0x05C)
#define FS_ATTR_POLL_INT_MASK_		(0x00FF0000)
#define FS_ATTR_DEV_DESC_SIZE_MASK_	(0x0000FF00)
#define FS_ATTR_CFG_BLK_SIZE_MASK_	(0x000000FF)

#define STR_ATTR0			    (0x060)
#define STR_ATTR0_CFGSTR_DESC_SIZE_MASK_    (0xFF000000)
#define STR_ATTR0_SERSTR_DESC_SIZE_MASK_    (0x00FF0000)
#define STR_ATTR0_PRODSTR_DESC_SIZE_MASK_   (0x0000FF00)
#define STR_ATTR0_MANUF_DESC_SIZE_MASK_     (0x000000FF)

#define STR_ATTR1			    (0x064)
#define STR_ATTR1_INTSTR_DESC_SIZE_MASK_    (0x000000FF)

#define STR_FLAG_ATTR			    (0x068)
#define STR_FLAG_ATTR_PME_FLAGS_MASK_	    (0x000000FF)

#define USB_CFG0			(0x080)
#define USB_CFG_LPM_RESPONSE_		(0x80000000)
#define USB_CFG_LPM_CAPABILITY_		(0x40000000)
#define USB_CFG_LPM_ENBL_SLPM_		(0x20000000)
#define USB_CFG_HIRD_THR_MASK_		(0x1F000000)
#define USB_CFG_HIRD_THR_960_		(0x1C000000)
#define USB_CFG_HIRD_THR_885_		(0x1B000000)
#define USB_CFG_HIRD_THR_810_		(0x1A000000)
#define USB_CFG_HIRD_THR_735_		(0x19000000)
#define USB_CFG_HIRD_THR_660_		(0x18000000)
#define USB_CFG_HIRD_THR_585_		(0x17000000)
#define USB_CFG_HIRD_THR_510_		(0x16000000)
#define USB_CFG_HIRD_THR_435_		(0x15000000)
#define USB_CFG_HIRD_THR_360_		(0x14000000)
#define USB_CFG_HIRD_THR_285_		(0x13000000)
#define USB_CFG_HIRD_THR_210_		(0x12000000)
#define USB_CFG_HIRD_THR_135_		(0x11000000)
#define USB_CFG_HIRD_THR_60_		(0x10000000)
#define USB_CFG_MAX_BURST_BI_MASK_	(0x00F00000)
#define USB_CFG_MAX_BURST_BO_MASK_	(0x000F0000)
#define USB_CFG_MAX_DEV_SPEED_MASK_	(0x0000E000)
#define USB_CFG_MAX_DEV_SPEED_SS_	(0x00008000)
#define USB_CFG_MAX_DEV_SPEED_HS_	(0x00000000)
#define USB_CFG_MAX_DEV_SPEED_FS_	(0x00002000)
#define USB_CFG_PHY_BOOST_MASK_		(0x00000180)
#define USB_CFG_PHY_BOOST_PLUS_12_	(0x00000180)
#define USB_CFG_PHY_BOOST_PLUS_8_	(0x00000100)
#define USB_CFG_PHY_BOOST_PLUS_4_	(0x00000080)
#define USB_CFG_PHY_BOOST_NORMAL_	(0x00000000)
#define USB_CFG_BIR_			(0x00000040)
#define USB_CFG_BCE_			(0x00000020)
#define USB_CFG_PORT_SWAP_		(0x00000010)
#define USB_CFG_LPM_EN_			(0x00000008)
#define USB_CFG_RMT_WKP_		(0x00000004)
#define USB_CFG_PWR_SEL_		(0x00000002)
#define USB_CFG_STALL_BO_DIS_		(0x00000001)

#define USB_CFG1			(0x084)
#define USB_CFG1_U1_TIMEOUT_MASK_	(0xFF000000)
#define USB_CFG1_U2_TIMEOUT_MASK_	(0x00FF0000)
#define USB_CFG1_HS_TOUT_CAL_MASK_	(0x0000E000)
#define USB_CFG1_DEV_U2_INIT_EN_	(0x00001000)
#define USB_CFG1_DEV_U2_EN_		(0x00000800)
#define USB_CFG1_DEV_U1_INIT_EN_	(0x00000400)
#define USB_CFG1_DEV_U1_EN_		(0x00000200)
#define USB_CFG1_LTM_ENABLE_		(0x00000100)
#define USB_CFG1_FS_TOUT_CAL_MASK_	(0x00000070)
#define USB_CFG1_SCALE_DOWN_MASK_	(0x00000003)
#define USB_CFG1_SCALE_DOWN_MODE3_	(0x00000003)
#define USB_CFG1_SCALE_DOWN_MODE2_	(0x00000002)
#define USB_CFG1_SCALE_DOWN_MODE1_	(0x00000001)
#define USB_CFG1_SCALE_DOWN_MODE0_	(0x00000000)

#define USB_CFG2			    (0x088)
#define USB_CFG2_SS_DETACH_TIME_MASK_	    (0xFFFF0000)
#define USB_CFG2_HS_DETACH_TIME_MASK_	    (0x0000FFFF)

#define BURST_CAP			(0x090)
#define BURST_CAP_SIZE_MASK_		(0x000000FF)

#define BULK_IN_DLY			(0x094)
#define BULK_IN_DLY_MASK_		(0x0000FFFF)

#define INT_EP_CTL			(0x098)
#define INT_EP_INTEP_ON_		(0x80000000)
#define INT_STS_EEE_TX_LPI_STRT_EN_	(0x04000000)
#define INT_STS_EEE_TX_LPI_STOP_EN_	(0x02000000)
#define INT_STS_EEE_RX_LPI_EN_		(0x01000000)
#define INT_EP_RDFO_EN_			(0x00400000)
#define INT_EP_TXE_EN_			(0x00200000)
#define INT_EP_TX_DIS_EN_		(0x00080000)
#define INT_EP_RX_DIS_EN_		(0x00040000)
#define INT_EP_PHY_INT_EN_		(0x00020000)
#define INT_EP_DP_INT_EN_		(0x00010000)
#define INT_EP_MAC_ERR_EN_		(0x00008000)
#define INT_EP_TDFU_EN_			(0x00004000)
#define INT_EP_TDFO_EN_			(0x00002000)
#define INT_EP_UTX_FP_EN_		(0x00001000)
#define INT_EP_GPIO_EN_MASK_		(0x00000FFF)

#define PIPE_CTL			(0x09C)
#define PIPE_CTL_TXSWING_		(0x00000040)
#define PIPE_CTL_TXMARGIN_MASK_		(0x00000038)
#define PIPE_CTL_TXDEEMPHASIS_MASK_	(0x00000006)
#define PIPE_CTL_ELASTICITYBUFFERMODE_	(0x00000001)

#define U1_LATENCY			(0xA0)
#define U2_LATENCY			(0xA4)

#define USB_STATUS			(0x0A8)
#define USB_STATUS_REMOTE_WK_		(0x00100000)
#define USB_STATUS_FUNC_REMOTE_WK_	(0x00080000)
#define USB_STATUS_LTM_ENABLE_		(0x00040000)
#define USB_STATUS_U2_ENABLE_		(0x00020000)
#define USB_STATUS_U1_ENABLE_		(0x00010000)
#define USB_STATUS_SET_SEL_		(0x00000020)
#define USB_STATUS_REMOTE_WK_STS_	(0x00000010)
#define USB_STATUS_FUNC_REMOTE_WK_STS_	(0x00000008)
#define USB_STATUS_LTM_ENABLE_STS_	(0x00000004)
#define USB_STATUS_U2_ENABLE_STS_	(0x00000002)
#define USB_STATUS_U1_ENABLE_STS_	(0x00000001)

#define USB_CFG3			(0x0AC)
#define USB_CFG3_EN_U2_LTM_		(0x40000000)
#define USB_CFG3_BULK_OUT_NUMP_OVR_	(0x20000000)
#define USB_CFG3_DIS_FAST_U1_EXIT_	(0x10000000)
#define USB_CFG3_LPM_NYET_THR_		(0x0F000000)
#define USB_CFG3_RX_DET_2_POL_LFPS_	(0x00800000)
#define USB_CFG3_LFPS_FILT_		(0x00400000)
#define USB_CFG3_SKIP_RX_DET_		(0x00200000)
#define USB_CFG3_DELAY_P1P2P3_		(0x001C0000)
#define USB_CFG3_DELAY_PHY_PWR_CHG_	(0x00020000)
#define USB_CFG3_U1U2_EXIT_FR_		(0x00010000)
#define USB_CFG3_REQ_P1P2P3		(0x00008000)
#define USB_CFG3_HST_PRT_CMPL_		(0x00004000)
#define USB_CFG3_DIS_SCRAMB_		(0x00002000)
#define USB_CFG3_PWR_DN_SCALE_		(0x00001FFF)

#define RFE_CTL				(0x0B0)
#define RFE_CTL_IGMP_COE_		(0x00004000)
#define RFE_CTL_ICMP_COE_		(0x00002000)
#define RFE_CTL_TCPUDP_COE_		(0x00001000)
#define RFE_CTL_IP_COE_			(0x00000800)
#define RFE_CTL_BCAST_EN_		(0x00000400)
#define RFE_CTL_MCAST_EN_		(0x00000200)
#define RFE_CTL_UCAST_EN_		(0x00000100)
#define RFE_CTL_VLAN_STRIP_		(0x00000080)
#define RFE_CTL_DISCARD_UNTAGGED_	(0x00000040)
#define RFE_CTL_VLAN_FILTER_		(0x00000020)
#define RFE_CTL_SA_FILTER_		(0x00000010)
#define RFE_CTL_MCAST_HASH_		(0x00000008)
#define RFE_CTL_DA_HASH_		(0x00000004)
#define RFE_CTL_DA_PERFECT_		(0x00000002)
#define RFE_CTL_RST_			(0x00000001)

#define VLAN_TYPE			(0x0B4)
#define VLAN_TYPE_MASK_			(0x0000FFFF)

#define FCT_RX_CTL			(0x0C0)
#define FCT_RX_CTL_EN_			(0x80000000)
#define FCT_RX_CTL_RST_			(0x40000000)
#define FCT_RX_CTL_SBF_			(0x02000000)
#define FCT_RX_CTL_OVFL_		(0x01000000)
#define FCT_RX_CTL_DROP_		(0x00800000)
#define FCT_RX_CTL_NOT_EMPTY_		(0x00400000)
#define FCT_RX_CTL_EMPTY_		(0x00200000)
#define FCT_RX_CTL_DIS_			(0x00100000)
#define FCT_RX_CTL_USED_MASK_		(0x0000FFFF)

#define FCT_TX_CTL			(0x0C4)
#define FCT_TX_CTL_EN_			(0x80000000)
#define FCT_TX_CTL_RST_			(0x40000000)
#define FCT_TX_CTL_NOT_EMPTY_		(0x00400000)
#define FCT_TX_CTL_EMPTY_		(0x00200000)
#define FCT_TX_CTL_DIS_			(0x00100000)
#define FCT_TX_CTL_USED_MASK_		(0x0000FFFF)

#define FCT_RX_FIFO_END			(0x0C8)
#define FCT_RX_FIFO_END_MASK_		(0x0000007F)

#define FCT_TX_FIFO_END			(0x0CC)
#define FCT_TX_FIFO_END_MASK_		(0x0000003F)

#define FCT_FLOW			(0x0D0)
#define FCT_FLOW_OFF_MASK_		(0x00007F00)
#define FCT_FLOW_ON_MASK_		(0x0000007F)

#define RX_DP_STOR			(0x0D4)
#define RX_DP_STORE_TOT_RXUSED_MASK_	(0xFFFF0000)
#define RX_DP_STORE_UTX_RXUSED_MASK_	(0x0000FFFF)

#define TX_DP_STOR			(0x0D8)
#define TX_DP_STORE_TOT_TXUSED_MASK_	(0xFFFF0000)
#define TX_DP_STORE_URX_TXUSED_MASK_	(0x0000FFFF)

#define LTM_BELT_IDLE0			(0x0E0)
#define LTM_BELT_IDLE0_IDLE1000_	(0x0FFF0000)
#define LTM_BELT_IDLE0_IDLE100_		(0x00000FFF)

#define LTM_BELT_IDLE1			(0x0E4)
#define LTM_BELT_IDLE1_IDLE10_		(0x00000FFF)

#define LTM_BELT_ACT0			(0x0E8)
#define LTM_BELT_ACT0_ACT1000_		(0x0FFF0000)
#define LTM_BELT_ACT0_ACT100_		(0x00000FFF)

#define LTM_BELT_ACT1			(0x0EC)
#define LTM_BELT_ACT1_ACT10_		(0x00000FFF)

#define LTM_INACTIVE0			(0x0F0)
#define LTM_INACTIVE0_TIMER1000_	(0xFFFF0000)
#define LTM_INACTIVE0_TIMER100_		(0x0000FFFF)

#define LTM_INACTIVE1			(0x0F4)
#define LTM_INACTIVE1_TIMER10_		(0x0000FFFF)

#define MAC_CR				(0x100)
#define MAC_CR_GMII_EN_			(0x00080000)
#define MAC_CR_EEE_TX_CLK_STOP_EN_	(0x00040000)
#define MAC_CR_EEE_EN_			(0x00020000)
#define MAC_CR_EEE_TLAR_EN_		(0x00010000)
#define MAC_CR_ADP_			(0x00002000)
#define MAC_CR_AUTO_DUPLEX_		(0x00001000)
#define MAC_CR_AUTO_SPEED_		(0x00000800)
#define MAC_CR_LOOPBACK_		(0x00000400)
#define MAC_CR_BOLMT_MASK_		(0x000000C0)
#define MAC_CR_FULL_DUPLEX_		(0x00000008)
#define MAC_CR_SPEED_MASK_		(0x00000006)
#define MAC_CR_SPEED_1000_		(0x00000004)
#define MAC_CR_SPEED_100_		(0x00000002)
#define MAC_CR_SPEED_10_		(0x00000000)
#define MAC_CR_RST_			(0x00000001)

#define MAC_RX				(0x104)
#define MAC_RX_MAX_SIZE_SHIFT_		(16)
#define MAC_RX_MAX_SIZE_MASK_		(0x3FFF0000)
#define MAC_RX_FCS_STRIP_		(0x00000010)
#define MAC_RX_VLAN_FSE_		(0x00000004)
#define MAC_RX_RXD_			(0x00000002)
#define MAC_RX_RXEN_			(0x00000001)

#define MAC_TX				(0x108)
#define MAC_TX_BAD_FCS_			(0x00000004)
#define MAC_TX_TXD_			(0x00000002)
#define MAC_TX_TXEN_			(0x00000001)

#define FLOW				(0x10C)
#define FLOW_CR_FORCE_FC_		(0x80000000)
#define FLOW_CR_TX_FCEN_		(0x40000000)
#define FLOW_CR_RX_FCEN_		(0x20000000)
#define FLOW_CR_FPF_			(0x10000000)
#define FLOW_CR_FCPT_MASK_		(0x0000FFFF)

#define RAND_SEED			(0x110)
#define RAND_SEED_MASK_			(0x0000FFFF)

#define ERR_STS				(0x114)
#define ERR_STS_FERR_			(0x00000100)
#define ERR_STS_LERR_			(0x00000080)
#define ERR_STS_RFERR_			(0x00000040)
#define ERR_STS_ECERR_			(0x00000010)
#define ERR_STS_ALERR_			(0x00000008)
#define ERR_STS_URERR_			(0x00000004)

#define RX_ADDRH			(0x118)
#define RX_ADDRH_MASK_			(0x0000FFFF)

#define RX_ADDRL			(0x11C)
#define RX_ADDRL_MASK_			(0xFFFFFFFF)

#define MII_ACC				(0x120)
#define MII_ACC_PHY_ADDR_SHIFT_		(11)
#define MII_ACC_PHY_ADDR_MASK_		(0x0000F800)
#define MII_ACC_MIIRINDA_SHIFT_		(6)
#define MII_ACC_MIIRINDA_MASK_		(0x000007C0)
#define MII_ACC_MII_READ_		(0x00000000)
#define MII_ACC_MII_WRITE_		(0x00000002)
#define MII_ACC_MII_BUSY_		(0x00000001)

#define MII_DATA			(0x124)
#define MII_DATA_MASK_			(0x0000FFFF)

#define MAC_RGMII_ID			(0x128)
#define MAC_RGMII_ID_TXC_DELAY_EN_	(0x00000002)
#define MAC_RGMII_ID_RXC_DELAY_EN_	(0x00000001)

#define EEE_TX_LPI_REQ_DLY		(0x130)
#define EEE_TX_LPI_REQ_DLY_CNT_MASK_	(0xFFFFFFFF)

#define EEE_TW_TX_SYS			(0x134)
#define EEE_TW_TX_SYS_CNT1G_MASK_	(0xFFFF0000)
#define EEE_TW_TX_SYS_CNT100M_MASK_	(0x0000FFFF)

#define EEE_TX_LPI_REM_DLY		(0x138)
#define EEE_TX_LPI_REM_DLY_CNT_		(0x00FFFFFF)

#define WUCSR				(0x140)
#define WUCSR_TESTMODE_			(0x80000000)
#define WUCSR_RFE_WAKE_EN_		(0x00004000)
#define WUCSR_EEE_TX_WAKE_		(0x00002000)
#define WUCSR_EEE_TX_WAKE_EN_		(0x00001000)
#define WUCSR_EEE_RX_WAKE_		(0x00000800)
#define WUCSR_EEE_RX_WAKE_EN_		(0x00000400)
#define WUCSR_RFE_WAKE_FR_		(0x00000200)
#define WUCSR_STORE_WAKE_		(0x00000100)
#define WUCSR_PFDA_FR_			(0x00000080)
#define WUCSR_WUFR_			(0x00000040)
#define WUCSR_MPR_			(0x00000020)
#define WUCSR_BCST_FR_			(0x00000010)
#define WUCSR_PFDA_EN_			(0x00000008)
#define WUCSR_WAKE_EN_			(0x00000004)
#define WUCSR_MPEN_			(0x00000002)
#define WUCSR_BCST_EN_			(0x00000001)

#define WK_SRC				(0x144)
#define WK_SRC_GPIOX_INT_WK_SHIFT_	(20)
#define WK_SRC_GPIOX_INT_WK_MASK_	(0xFFF00000)
#define WK_SRC_IPV6_TCPSYN_RCD_WK_	(0x00010000)
#define WK_SRC_IPV4_TCPSYN_RCD_WK_	(0x00008000)
#define WK_SRC_EEE_TX_WK_		(0x00004000)
#define WK_SRC_EEE_RX_WK_		(0x00002000)
#define WK_SRC_GOOD_FR_WK_		(0x00001000)
#define WK_SRC_PFDA_FR_WK_		(0x00000800)
#define WK_SRC_MP_FR_WK_		(0x00000400)
#define WK_SRC_BCAST_FR_WK_		(0x00000200)
#define WK_SRC_WU_FR_WK_		(0x00000100)
#define WK_SRC_WUFF_MATCH_MASK_		(0x0000001F)

#define WUF_CFG0			(0x150)
#define NUM_OF_WUF_CFG			(32)
#define WUF_CFG_BEGIN			(WUF_CFG0)
#define WUF_CFG(index)			(WUF_CFG_BEGIN + (4 * (index)))
#define WUF_CFGX_EN_			(0x80000000)
#define WUF_CFGX_TYPE_MASK_		(0x03000000)
#define WUF_CFGX_TYPE_MCAST_		(0x02000000)
#define WUF_CFGX_TYPE_ALL_		(0x01000000)
#define WUF_CFGX_TYPE_UCAST_		(0x00000000)
#define WUF_CFGX_OFFSET_SHIFT_		(16)
#define WUF_CFGX_OFFSET_MASK_		(0x00FF0000)
#define WUF_CFGX_CRC16_MASK_		(0x0000FFFF)

#define WUF_MASK0_0			(0x200)
#define WUF_MASK0_1			(0x204)
#define WUF_MASK0_2			(0x208)
#define WUF_MASK0_3			(0x20C)
#define NUM_OF_WUF_MASK			(32)
#define WUF_MASK0_BEGIN			(WUF_MASK0_0)
#define WUF_MASK1_BEGIN			(WUF_MASK0_1)
#define WUF_MASK2_BEGIN			(WUF_MASK0_2)
#define WUF_MASK3_BEGIN			(WUF_MASK0_3)
#define WUF_MASK0(index)		(WUF_MASK0_BEGIN + (0x10 * (index)))
#define WUF_MASK1(index)		(WUF_MASK1_BEGIN + (0x10 * (index)))
#define WUF_MASK2(index)		(WUF_MASK2_BEGIN + (0x10 * (index)))
#define WUF_MASK3(index)		(WUF_MASK3_BEGIN + (0x10 * (index)))

#define MAF_BASE			(0x400)
#define MAF_HIX				(0x00)
#define MAF_LOX				(0x04)
#define NUM_OF_MAF			(33)
#define MAF_HI_BEGIN			(MAF_BASE + MAF_HIX)
#define MAF_LO_BEGIN			(MAF_BASE + MAF_LOX)
#define MAF_HI(index)			(MAF_BASE + (8 * (index)) + (MAF_HIX))
#define MAF_LO(index)			(MAF_BASE + (8 * (index)) + (MAF_LOX))
#define MAF_HI_VALID_			(0x80000000)
#define MAF_HI_TYPE_MASK_		(0x40000000)
#define MAF_HI_TYPE_SRC_		(0x40000000)
#define MAF_HI_TYPE_DST_		(0x00000000)
#define MAF_HI_ADDR_MASK		(0x0000FFFF)
#define MAF_LO_ADDR_MASK		(0xFFFFFFFF)

#define WUCSR2				(0x600)
#define WUCSR2_CSUM_DISABLE_		(0x80000000)
#define WUCSR2_NA_SA_SEL_		(0x00000100)
#define WUCSR2_NS_RCD_			(0x00000080)
#define WUCSR2_ARP_RCD_			(0x00000040)
#define WUCSR2_IPV6_TCPSYN_RCD_		(0x00000020)
#define WUCSR2_IPV4_TCPSYN_RCD_		(0x00000010)
#define WUCSR2_NS_OFFLOAD_EN_		(0x00000008)
#define WUCSR2_ARP_OFFLOAD_EN_		(0x00000004)
#define WUCSR2_IPV6_TCPSYN_WAKE_EN_	(0x00000002)
#define WUCSR2_IPV4_TCPSYN_WAKE_EN_	(0x00000001)

#define NS1_IPV6_ADDR_DEST0		(0x610)
#define NS1_IPV6_ADDR_DEST1		(0x614)
#define NS1_IPV6_ADDR_DEST2		(0x618)
#define NS1_IPV6_ADDR_DEST3		(0x61C)

#define NS1_IPV6_ADDR_SRC0		(0x620)
#define NS1_IPV6_ADDR_SRC1		(0x624)
#define NS1_IPV6_ADDR_SRC2		(0x628)
#define NS1_IPV6_ADDR_SRC3		(0x62C)

#define NS1_ICMPV6_ADDR0_0		(0x630)
#define NS1_ICMPV6_ADDR0_1		(0x634)
#define NS1_ICMPV6_ADDR0_2		(0x638)
#define NS1_ICMPV6_ADDR0_3		(0x63C)

#define NS1_ICMPV6_ADDR1_0		(0x640)
#define NS1_ICMPV6_ADDR1_1		(0x644)
#define NS1_ICMPV6_ADDR1_2		(0x648)
#define NS1_ICMPV6_ADDR1_3		(0x64C)

#define NS2_IPV6_ADDR_DEST0		(0x650)
#define NS2_IPV6_ADDR_DEST1		(0x654)
#define NS2_IPV6_ADDR_DEST2		(0x658)
#define NS2_IPV6_ADDR_DEST3		(0x65C)

#define NS2_IPV6_ADDR_SRC0		(0x660)
#define NS2_IPV6_ADDR_SRC1		(0x664)
#define NS2_IPV6_ADDR_SRC2		(0x668)
#define NS2_IPV6_ADDR_SRC3		(0x66C)

#define NS2_ICMPV6_ADDR0_0		(0x670)
#define NS2_ICMPV6_ADDR0_1		(0x674)
#define NS2_ICMPV6_ADDR0_2		(0x678)
#define NS2_ICMPV6_ADDR0_3		(0x67C)

#define NS2_ICMPV6_ADDR1_0		(0x680)
#define NS2_ICMPV6_ADDR1_1		(0x684)
#define NS2_ICMPV6_ADDR1_2		(0x688)
#define NS2_ICMPV6_ADDR1_3		(0x68C)

#define SYN_IPV4_ADDR_SRC		(0x690)
#define SYN_IPV4_ADDR_DEST		(0x694)
#define SYN_IPV4_TCP_PORTS		(0x698)
#define SYN_IPV4_TCP_PORTS_IPV4_DEST_PORT_SHIFT_    (16)
#define SYN_IPV4_TCP_PORTS_IPV4_DEST_PORT_MASK_     (0xFFFF0000)
#define SYN_IPV4_TCP_PORTS_IPV4_SRC_PORT_MASK_	    (0x0000FFFF)

#define SYN_IPV6_ADDR_SRC0		(0x69C)
#define SYN_IPV6_ADDR_SRC1		(0x6A0)
#define SYN_IPV6_ADDR_SRC2		(0x6A4)
#define SYN_IPV6_ADDR_SRC3		(0x6A8)

#define SYN_IPV6_ADDR_DEST0		(0x6AC)
#define SYN_IPV6_ADDR_DEST1		(0x6B0)
#define SYN_IPV6_ADDR_DEST2		(0x6B4)
#define SYN_IPV6_ADDR_DEST3		(0x6B8)

#define SYN_IPV6_TCP_PORTS		(0x6BC)
#define SYN_IPV6_TCP_PORTS_IPV6_DEST_PORT_SHIFT_    (16)
#define SYN_IPV6_TCP_PORTS_IPV6_DEST_PORT_MASK_     (0xFFFF0000)
#define SYN_IPV6_TCP_PORTS_IPV6_SRC_PORT_MASK_	    (0x0000FFFF)

#define ARP_SPA				(0x6C0)
#define ARP_TPA				(0x6C4)

#define PHY_DEV_ID			(0x700)
#define PHY_DEV_ID_REV_SHIFT_		(28)
#define PHY_DEV_ID_REV_SHIFT_		(28)
#define PHY_DEV_ID_REV_MASK_		(0xF0000000)
#define PHY_DEV_ID_MODEL_SHIFT_		(22)
#define PHY_DEV_ID_MODEL_MASK_		(0x0FC00000)
#define PHY_DEV_ID_OUI_MASK_		(0x003FFFFF)

#define RGMII_TX_BYP_DLL		(0x708)
#define RGMII_TX_BYP_DLL_TX_TUNE_ADJ_MASK_	(0x000FC00)
#define RGMII_TX_BYP_DLL_TX_TUNE_SEL_MASK_	(0x00003F0)
#define RGMII_TX_BYP_DLL_TX_DLL_RESET_		(0x0000002)
#define RGMII_TX_BYP_DLL_TX_DLL_BYPASS_		(0x0000001)

#define RGMII_RX_BYP_DLL		(0x70C)
#define RGMII_RX_BYP_DLL_RX_TUNE_ADJ_MASK_	(0x000FC00)
#define RGMII_RX_BYP_DLL_RX_TUNE_SEL_MASK_	(0x00003F0)
#define RGMII_RX_BYP_DLL_RX_DLL_RESET_		(0x0000002)
#define RGMII_RX_BYP_DLL_RX_DLL_BYPASS_		(0x0000001)

#define OTP_BASE_ADDR			(0x00001000)
#define OTP_ADDR_RANGE_			(0x1FF)

#define OTP_PWR_DN			(OTP_BASE_ADDR + 4 * 0x00)
#define OTP_PWR_DN_PWRDN_N_		(0x01)

#define OTP_ADDR1			(OTP_BASE_ADDR + 4 * 0x01)
#define OTP_ADDR1_15_11			(0x1F)

#define OTP_ADDR2			(OTP_BASE_ADDR + 4 * 0x02)
#define OTP_ADDR2_10_3			(0xFF)

#define OTP_ADDR3			(OTP_BASE_ADDR + 4 * 0x03)
#define OTP_ADDR3_2_0			(0x03)

#define OTP_PRGM_DATA			(OTP_BASE_ADDR + 4 * 0x04)

#define OTP_PRGM_MODE			(OTP_BASE_ADDR + 4 * 0x05)
#define OTP_PRGM_MODE_BYTE_		(0x01)

#define OTP_RD_DATA			(OTP_BASE_ADDR + 4 * 0x06)

#define OTP_FUNC_CMD			(OTP_BASE_ADDR + 4 * 0x08)
#define OTP_FUNC_CMD_RESET_		(0x04)
#define OTP_FUNC_CMD_PROGRAM_		(0x02)
#define OTP_FUNC_CMD_READ_		(0x01)

#define OTP_TST_CMD			(OTP_BASE_ADDR + 4 * 0x09)
#define OTP_TST_CMD_TEST_DEC_SEL_	(0x10)
#define OTP_TST_CMD_PRGVRFY_		(0x08)
#define OTP_TST_CMD_WRTEST_		(0x04)
#define OTP_TST_CMD_TESTDEC_		(0x02)
#define OTP_TST_CMD_BLANKCHECK_		(0x01)

#define OTP_CMD_GO			(OTP_BASE_ADDR + 4 * 0x0A)
#define OTP_CMD_GO_GO_			(0x01)

#define OTP_PASS_FAIL			(OTP_BASE_ADDR + 4 * 0x0B)
#define OTP_PASS_FAIL_PASS_		(0x02)
#define OTP_PASS_FAIL_FAIL_		(0x01)

#define OTP_STATUS			(OTP_BASE_ADDR + 4 * 0x0C)
#define OTP_STATUS_OTP_LOCK_		(0x10)
#define OTP_STATUS_WEB_			(0x08)
#define OTP_STATUS_PGMEN		(0x04)
#define OTP_STATUS_CPUMPEN_		(0x02)
#define OTP_STATUS_BUSY_		(0x01)

#define OTP_MAX_PRG			(OTP_BASE_ADDR + 4 * 0x0D)
#define OTP_MAX_PRG_MAX_PROG		(0x1F)

#define OTP_INTR_STATUS			(OTP_BASE_ADDR + 4 * 0x10)
#define OTP_INTR_STATUS_READY_		(0x01)

#define OTP_INTR_MASK			(OTP_BASE_ADDR + 4 * 0x11)
#define OTP_INTR_MASK_READY_		(0x01)

#define OTP_RSTB_PW1			(OTP_BASE_ADDR + 4 * 0x14)
#define OTP_RSTB_PW2			(OTP_BASE_ADDR + 4 * 0x15)
#define OTP_PGM_PW1			(OTP_BASE_ADDR + 4 * 0x18)
#define OTP_PGM_PW2			(OTP_BASE_ADDR + 4 * 0x19)
#define OTP_READ_PW1			(OTP_BASE_ADDR + 4 * 0x1C)
#define OTP_READ_PW2			(OTP_BASE_ADDR + 4 * 0x1D)
#define OTP_TCRST			(OTP_BASE_ADDR + 4 * 0x20)
#define OTP_RSRD			(OTP_BASE_ADDR + 4 * 0x21)
#define OTP_TREADEN_VAL			(OTP_BASE_ADDR + 4 * 0x22)
#define OTP_TDLES_VAL			(OTP_BASE_ADDR + 4 * 0x23)
#define OTP_TWWL_VAL			(OTP_BASE_ADDR + 4 * 0x24)
#define OTP_TDLEH_VAL			(OTP_BASE_ADDR + 4 * 0x25)
#define OTP_TWPED_VAL			(OTP_BASE_ADDR + 4 * 0x26)
#define OTP_TPES_VAL			(OTP_BASE_ADDR + 4 * 0x27)
#define OTP_TCPS_VAL			(OTP_BASE_ADDR + 4 * 0x28)
#define OTP_TCPH_VAL			(OTP_BASE_ADDR + 4 * 0x29)
#define OTP_TPGMVFY_VAL			(OTP_BASE_ADDR + 4 * 0x2A)
#define OTP_TPEH_VAL			(OTP_BASE_ADDR + 4 * 0x2B)
#define OTP_TPGRST_VAL			(OTP_BASE_ADDR + 4 * 0x2C)
#define OTP_TCLES_VAL			(OTP_BASE_ADDR + 4 * 0x2D)
#define OTP_TCLEH_VAL			(OTP_BASE_ADDR + 4 * 0x2E)
#define OTP_TRDES_VAL			(OTP_BASE_ADDR + 4 * 0x2F)
#define OTP_TBCACC_VAL			(OTP_BASE_ADDR + 4 * 0x30)
#define OTP_TAAC_VAL			(OTP_BASE_ADDR + 4 * 0x31)
#define OTP_TACCT_VAL			(OTP_BASE_ADDR + 4 * 0x32)
#define OTP_TRDEP_VAL			(OTP_BASE_ADDR + 4 * 0x38)
#define OTP_TPGSV_VAL			(OTP_BASE_ADDR + 4 * 0x39)
#define OTP_TPVSR_VAL			(OTP_BASE_ADDR + 4 * 0x3A)
#define OTP_TPVHR_VAL			(OTP_BASE_ADDR + 4 * 0x3B)
#define OTP_TPVSA_VAL			(OTP_BASE_ADDR + 4 * 0x3C)
#endif /* _LAN78XX_H */
