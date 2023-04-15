/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DSI_DEFS_H_
#define _DSI_DEFS_H_

#include <linux/types.h>
#include <drm/drm_mipi_dsi.h>
#include "msm_drv.h"

#define DSI_H_TOTAL(t) (((t)->h_active) + ((t)->h_back_porch) + \
			((t)->h_sync_width) + ((t)->h_front_porch))

#define DSI_V_TOTAL(t) (((t)->v_active) + ((t)->v_back_porch) + \
			((t)->v_sync_width) + ((t)->v_front_porch))

#define DSI_H_TOTAL_DSC(t) \
	({\
		u64 value;\
		if ((t)->dsc_enabled && (t)->dsc)\
			value = (t)->dsc->pclk_per_line;\
		else\
			value = (t)->h_active;\
		value = value + (t)->h_back_porch + (t)->h_sync_width +\
			(t)->h_front_porch;\
		value;\
	})

#define DSI_H_ACTIVE_DSC(t) \
	({\
		u64 value;\
		if ((t)->dsc_enabled && (t)->dsc)\
			value = (t)->dsc->pclk_per_line;\
		else\
			value = (t)->h_active;\
		value;\
	})


#define DSI_DEBUG_NAME_LEN		32
#define display_for_each_ctrl(index, display) \
	for (index = 0; (index < (display)->ctrl_count) &&\
			(index < MAX_DSI_CTRLS_PER_DISPLAY); index++)

#define DSI_WARN(fmt, ...)	DRM_WARN("[msm-dsi-warn]: "fmt, ##__VA_ARGS__)
#define DSI_ERR(fmt, ...)	DRM_DEV_ERROR(NULL, "[msm-dsi-error]: " fmt, \
								##__VA_ARGS__)
#define DSI_INFO(fmt, ...)	DRM_DEV_INFO(NULL, "[msm-dsi-info]: "fmt, \
								##__VA_ARGS__)
#define DSI_DEBUG(fmt, ...)	DRM_DEV_DEBUG(NULL, "[msm-dsi-debug]: "fmt, \
								##__VA_ARGS__)
/**
 * enum dsi_pixel_format - DSI pixel formats
 * @DSI_PIXEL_FORMAT_RGB565:
 * @DSI_PIXEL_FORMAT_RGB666:
 * @DSI_PIXEL_FORMAT_RGB666_LOOSE:
 * @DSI_PIXEL_FORMAT_RGB888:
 * @DSI_PIXEL_FORMAT_RGB111:
 * @DSI_PIXEL_FORMAT_RGB332:
 * @DSI_PIXEL_FORMAT_RGB444:
 * @DSI_PIXEL_FORMAT_MAX:
 */
enum dsi_pixel_format {
	DSI_PIXEL_FORMAT_RGB565 = 0,
	DSI_PIXEL_FORMAT_RGB666,
	DSI_PIXEL_FORMAT_RGB666_LOOSE,
	DSI_PIXEL_FORMAT_RGB888,
	DSI_PIXEL_FORMAT_RGB111,
	DSI_PIXEL_FORMAT_RGB332,
	DSI_PIXEL_FORMAT_RGB444,
	DSI_PIXEL_FORMAT_MAX
};

/**
 * enum dsi_op_mode - dsi operation mode
 * @DSI_OP_VIDEO_MODE: DSI video mode operation
 * @DSI_OP_CMD_MODE:   DSI Command mode operation
 * @DSI_OP_MODE_MAX:
 */
enum dsi_op_mode {
	DSI_OP_VIDEO_MODE = 0,
	DSI_OP_CMD_MODE,
	DSI_OP_MODE_MAX
};

/**
 * enum dsi_mode_flags - flags to signal other drm components via private flags
 * @DSI_MODE_FLAG_SEAMLESS:	Seamless transition requested by user
 * @DSI_MODE_FLAG_DFPS:		Seamless transition is DynamicFPS
 * @DSI_MODE_FLAG_VBLANK_PRE_MODESET:	Transition needs VBLANK before Modeset
 * @DSI_MODE_FLAG_DMS: Seamless transition is dynamic mode switch
 * @DSI_MODE_FLAG_VRR: Seamless transition is DynamicFPS.
 *                     New timing values are sent from DAL.
 * @DSI_MODE_FLAG_POMS:
 *         Seamless transition is dynamic panel operating mode switch
 * @DSI_MODE_FLAG_DYN_CLK: Seamless transition is dynamic clock change
 * @DSI_MODE_FLAG_DMS_FPS: Seamless fps only transition in Dynamic Mode Switch
 */
enum dsi_mode_flags {
	DSI_MODE_FLAG_SEAMLESS			= BIT(0),
	DSI_MODE_FLAG_DFPS			= BIT(1),
	DSI_MODE_FLAG_VBLANK_PRE_MODESET	= BIT(2),
	DSI_MODE_FLAG_DMS			= BIT(3),
	DSI_MODE_FLAG_VRR			= BIT(4),
	DSI_MODE_FLAG_POMS			= BIT(5),
	DSI_MODE_FLAG_DYN_CLK			= BIT(6),
	DSI_MODE_FLAG_DMS_FPS                   = BIT(7),
};

/**
 * enum dsi_logical_lane - dsi logical lanes
 * @DSI_LOGICAL_LANE_0:     Logical lane 0
 * @DSI_LOGICAL_LANE_1:     Logical lane 1
 * @DSI_LOGICAL_LANE_2:     Logical lane 2
 * @DSI_LOGICAL_LANE_3:     Logical lane 3
 * @DSI_LOGICAL_CLOCK_LANE: Clock lane
 * @DSI_LANE_MAX:           Maximum lanes supported
 */
enum dsi_logical_lane {
	DSI_LOGICAL_LANE_0 = 0,
	DSI_LOGICAL_LANE_1,
	DSI_LOGICAL_LANE_2,
	DSI_LOGICAL_LANE_3,
	DSI_LOGICAL_CLOCK_LANE,
	DSI_LANE_MAX
};

/**
 * enum dsi_data_lanes - BIT map for DSI data lanes
 * This is used to identify the active DSI data lanes for
 * various operations like DSI data lane enable/ULPS/clamp
 * configurations.
 * @DSI_DATA_LANE_0: BIT(DSI_LOGICAL_LANE_0)
 * @DSI_DATA_LANE_1: BIT(DSI_LOGICAL_LANE_1)
 * @DSI_DATA_LANE_2: BIT(DSI_LOGICAL_LANE_2)
 * @DSI_DATA_LANE_3: BIT(DSI_LOGICAL_LANE_3)
 * @DSI_CLOCK_LANE:  BIT(DSI_LOGICAL_CLOCK_LANE)
 */
enum dsi_data_lanes {
	DSI_DATA_LANE_0 = BIT(DSI_LOGICAL_LANE_0),
	DSI_DATA_LANE_1 = BIT(DSI_LOGICAL_LANE_1),
	DSI_DATA_LANE_2 = BIT(DSI_LOGICAL_LANE_2),
	DSI_DATA_LANE_3 = BIT(DSI_LOGICAL_LANE_3),
	DSI_CLOCK_LANE  = BIT(DSI_LOGICAL_CLOCK_LANE)
};

/**
 * enum dsi_phy_data_lanes - dsi physical lanes
 * used for DSI logical to physical lane mapping
 * @DSI_PHYSICAL_LANE_INVALID: Physical lane valid/invalid
 * @DSI_PHYSICAL_LANE_0: Physical lane 0
 * @DSI_PHYSICAL_LANE_1: Physical lane 1
 * @DSI_PHYSICAL_LANE_2: Physical lane 2
 * @DSI_PHYSICAL_LANE_3: Physical lane 3
 */
enum dsi_phy_data_lanes {
	DSI_PHYSICAL_LANE_INVALID = 0,
	DSI_PHYSICAL_LANE_0 = BIT(0),
	DSI_PHYSICAL_LANE_1 = BIT(1),
	DSI_PHYSICAL_LANE_2 = BIT(2),
	DSI_PHYSICAL_LANE_3  = BIT(3)
};

enum dsi_lane_map_type_v1 {
	DSI_LANE_MAP_0123,
	DSI_LANE_MAP_3012,
	DSI_LANE_MAP_2301,
	DSI_LANE_MAP_1230,
	DSI_LANE_MAP_0321,
	DSI_LANE_MAP_1032,
	DSI_LANE_MAP_2103,
	DSI_LANE_MAP_3210,
};

/**
 * lane_map: DSI logical <-> physical lane mapping
 * lane_map_v1: Lane mapping for DSI controllers < v2.0
 * lane_map_v2: Lane mapping for DSI controllers >= 2.0
 */
struct dsi_lane_map {
	enum dsi_lane_map_type_v1 lane_map_v1;
	u8 lane_map_v2[DSI_LANE_MAX - 1];
};

/**
 * enum dsi_trigger_type - dsi trigger type
 * @DSI_TRIGGER_NONE:     No trigger.
 * @DSI_TRIGGER_TE:       TE trigger.
 * @DSI_TRIGGER_SEOF:     Start or End of frame.
 * @DSI_TRIGGER_SW:       Software trigger.
 * @DSI_TRIGGER_SW_SEOF:  Software trigger and start/end of frame.
 * @DSI_TRIGGER_SW_TE:    Software and TE triggers.
 * @DSI_TRIGGER_MAX:      Max trigger values.
 */
enum dsi_trigger_type {
	DSI_TRIGGER_NONE = 0,
	DSI_TRIGGER_TE,
	DSI_TRIGGER_SEOF,
	DSI_TRIGGER_SW,
	DSI_TRIGGER_SW_SEOF,
	DSI_TRIGGER_SW_TE,
	DSI_TRIGGER_MAX
};

/**
 * enum dsi_color_swap_mode - color swap mode
 * @DSI_COLOR_SWAP_RGB:
 * @DSI_COLOR_SWAP_RBG:
 * @DSI_COLOR_SWAP_BGR:
 * @DSI_COLOR_SWAP_BRG:
 * @DSI_COLOR_SWAP_GRB:
 * @DSI_COLOR_SWAP_GBR:
 */
enum dsi_color_swap_mode {
	DSI_COLOR_SWAP_RGB = 0,
	DSI_COLOR_SWAP_RBG,
	DSI_COLOR_SWAP_BGR,
	DSI_COLOR_SWAP_BRG,
	DSI_COLOR_SWAP_GRB,
	DSI_COLOR_SWAP_GBR
};

/**
 * enum dsi_dfps_type - Dynamic FPS support type
 * @DSI_DFPS_NONE:           Dynamic FPS is not supported.
 * @DSI_DFPS_SUSPEND_RESUME:
 * @DSI_DFPS_IMMEDIATE_CLK:
 * @DSI_DFPS_IMMEDIATE_HFP:
 * @DSI_DFPS_IMMEDIATE_VFP:
 * @DSI_DPFS_MAX:
 */
enum dsi_dfps_type {
	DSI_DFPS_NONE = 0,
	DSI_DFPS_SUSPEND_RESUME,
	DSI_DFPS_IMMEDIATE_CLK,
	DSI_DFPS_IMMEDIATE_HFP,
	DSI_DFPS_IMMEDIATE_VFP,
	DSI_DFPS_MAX
};

/**
 * enum dsi_dyn_clk_feature_type - Dynamic clock feature support type
 * @DSI_DYN_CLK_TYPE_LEGACY:			Constant FPS is not supported
 * @DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_HFP:	Constant FPS supported with
 *						change in hfp
 * @DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_VFP:	Constant FPS supported with
 *						change in vfp
 * @DSI_DYN_CLK_TYPE_MAX:
 */
enum dsi_dyn_clk_feature_type {
	DSI_DYN_CLK_TYPE_LEGACY = 0,
	DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_HFP,
	DSI_DYN_CLK_TYPE_CONST_FPS_ADJUST_VFP,
	DSI_DYN_CLK_TYPE_MAX
};

/**
 * enum dsi_cmd_set_type  - DSI command set type
 * @DSI_CMD_SET_PRE_ON:	                   Panel pre on
 * @DSI_CMD_SET_ON:                        Panel on
 * @DSI_CMD_SET_POST_ON:                   Panel post on
 * @DSI_CMD_SET_PRE_OFF:                   Panel pre off
 * @DSI_CMD_SET_OFF:                       Panel off
 * @DSI_CMD_SET_POST_OFF:                  Panel post off
 * @DSI_CMD_SET_PRE_RES_SWITCH:            Pre resolution switch
 * @DSI_CMD_SET_RES_SWITCH:                Resolution switch
 * @DSI_CMD_SET_POST_RES_SWITCH:           Post resolution switch
 * @DSI_CMD_SET_CMD_TO_VID_SWITCH:         Cmd to video mode switch
 * @DSI_CMD_SET_POST_CMD_TO_VID_SWITCH:    Post cmd to vid switch
 * @DSI_CMD_SET_VID_TO_CMD_SWITCH:         Video to cmd mode switch
 * @DSI_CMD_SET_POST_VID_TO_CMD_SWITCH:    Post vid to cmd switch
 * @DSI_CMD_SET_PANEL_STATUS:              Panel status
 * @DSI_CMD_SET_LP1:                       Low power mode 1
 * @DSI_CMD_SET_LP2:                       Low power mode 2
 * @DSI_CMD_SET_NOLP:                      Low power mode disable
 * @DSI_CMD_SET_PPS:                       DSC PPS command
 * @DSI_CMD_SET_ROI:			   Panel ROI update
 * @DSI_CMD_SET_TIMING_SWITCH:             Timing switch
 * @DSI_CMD_SET_POST_TIMING_SWITCH:        Post timing switch
 * @DSI_CMD_SET_QSYNC_ON                   Enable qsync mode
 * @DSI_CMD_SET_QSYNC_OFF                  Disable qsync mode
 * @DSI_CMD_SET_MAX
 */
enum dsi_cmd_set_type {
	DSI_CMD_SET_PRE_ON = 0,
	DSI_CMD_SET_ON,
	DSI_CMD_SET_POST_ON,
	DSI_CMD_SET_PRE_OFF,
	DSI_CMD_SET_OFF,
	DSI_CMD_SET_POST_OFF,
	DSI_CMD_SET_PRE_RES_SWITCH,
	DSI_CMD_SET_RES_SWITCH,
	DSI_CMD_SET_POST_RES_SWITCH,
	DSI_CMD_SET_CMD_TO_VID_SWITCH,
	DSI_CMD_SET_POST_CMD_TO_VID_SWITCH,
	DSI_CMD_SET_VID_TO_CMD_SWITCH,
	DSI_CMD_SET_POST_VID_TO_CMD_SWITCH,
	DSI_CMD_SET_PANEL_STATUS,
	DSI_CMD_SET_LP1,
	DSI_CMD_SET_LP2,
	DSI_CMD_SET_NOLP,
	DSI_CMD_SET_PPS,
	DSI_CMD_SET_ROI,
	DSI_CMD_SET_TIMING_SWITCH,
	DSI_CMD_SET_POST_TIMING_SWITCH,
	DSI_CMD_SET_QSYNC_ON,
	DSI_CMD_SET_QSYNC_OFF,
	DSI_CMD_SET_MAX,

#if defined(CONFIG_DISPLAY_SAMSUNG)
	/* set type for samsung display driver
	 * Please add type in proper boundary (TX, RX)
	 */

	/* samsung CMD */
	SS_DSI_CMD_SET_START,

	/* cmd for each mode */
	SS_DSI_CMD_SET_FOR_EACH_MODE_START,
	TX_PANEL_LTPS,
	SS_DSI_CMD_SET_FOR_EACH_MODE_END,

	/* TX */
	TX_CMD_START,
	TX_MTP_WRITE_SYSFS,
	TX_TEMP_DSC,
	TX_DISPLAY_ON,
	TX_FIRST_DISPLAY_ON,
	TX_DISPLAY_OFF,
	TX_BRIGHT_CTRL,
	TX_MANUFACTURE_ID_READ_PRE,
	TX_LEVEL0_KEY_ENABLE,
	TX_LEVEL0_KEY_DISABLE,
	TX_LEVEL1_KEY_ENABLE,
	TX_LEVEL1_KEY_DISABLE,
	TX_LEVEL2_KEY_ENABLE,
	TX_LEVEL2_KEY_DISABLE,
	TX_POC_KEY_ENABLE,
	TX_POC_KEY_DISABLE,
	TX_MDNIE_ADB_TEST,
	TX_SELF_GRID_ON,
	TX_SELF_GRID_OFF,
	TX_LPM_ON_PRE,
	TX_LPM_ON,
	TX_LPM_OFF,
	TX_LPM_AOD_ON,
	TX_LPM_AOD_OFF,
	TX_LPM_2NIT_CMD,
	TX_LPM_10NIT_CMD,
	TX_LPM_30NIT_CMD,
	TX_LPM_40NIT_CMD,
	TX_LPM_60NIT_CMD,
	TX_ALPM_2NIT_CMD,
	TX_ALPM_10NIT_CMD,
	TX_ALPM_30NIT_CMD,
	TX_ALPM_40NIT_CMD,
	TX_ALPM_60NIT_CMD,
	TX_ALPM_OFF,
	TX_HLPM_2NIT_CMD,
	TX_HLPM_10NIT_CMD,
	TX_HLPM_30NIT_CMD,
	TX_HLPM_40NIT_CMD,
	TX_HLPM_60NIT_CMD,
	TX_HLPM_OFF,
	TX_LPM_BL_CMD,
	TX_PACKET_SIZE,
	TX_REG_READ_POS,
	TX_MDNIE_TUNE,
	TX_OSC_TE_FITTING,
	TX_AVC_ON,
	TX_LDI_FPS_CHANGE,
	TX_HMT_ENABLE,
	TX_HMT_DISABLE,
	TX_HMT_LOW_PERSISTENCE_OFF_BRIGHT,
	TX_HMT_REVERSE,
	TX_HMT_FORWARD,
	TX_FFC,
	TX_FFC_OFF,
	TX_DYNAMIC_FFC_PRE_SET,
	TX_DYNAMIC_FFC_SET,
	TX_CABC_ON,
	TX_CABC_OFF,
	TX_TFT_PWM,
	TX_GAMMA_MODE2_NORMAL,
	TX_GAMMA_MODE2_HBM,
	TX_GAMMA_MODE2_HBM_60HZ,
	TX_GAMMA_MODE2_HMT,
	TX_BLIC_DIMMING,
	TX_LDI_SET_VDD_OFFSET,
	TX_LDI_SET_VDDM_OFFSET,
	TX_HSYNC_ON,
	TX_CABC_ON_DUTY,
	TX_CABC_OFF_DUTY,
	TX_COPR_ENABLE,
	TX_COPR_DISABLE,
	TX_COLOR_WEAKNESS_ENABLE,
	TX_COLOR_WEAKNESS_DISABLE,
	TX_ESD_RECOVERY_1,
	TX_ESD_RECOVERY_2,
	TX_MCD_ON,
	TX_MCD_OFF,
	TX_MCD_READ_RESISTANCE_PRE, /* For read real MCD R/L resistance */
	TX_MCD_READ_RESISTANCE, /* For read real MCD R/L resistance */
	TX_MCD_READ_RESISTANCE_POST, /* For read real MCD R/L resistance */
	TX_BRIGHTDOT_ON,
	TX_BRIGHTDOT_OFF,
	TX_BRIGHTDOT_LF_ON,
	TX_BRIGHTDOT_LF_OFF,
	TX_GRADUAL_ACL,
	TX_HW_CURSOR,
	TX_DYNAMIC_HLPM_ENABLE,
	TX_DYNAMIC_HLPM_DISABLE,
	TX_MULTIRES_FHD_TO_WQHD,
	TX_MULTIRES_HD_TO_WQHD,
	TX_MULTIRES_FHD,
	TX_MULTIRES_HD,
	TX_COVER_CONTROL_ENABLE,
	TX_COVER_CONTROL_DISABLE,
	TX_HBM_GAMMA,
	TX_HBM_ETC,
	TX_HBM_IRC,
	TX_HBM_OFF,
	TX_AID,
	TX_AID_SUBDIVISION,
	TX_PAC_AID_SUBDIVISION,
	TX_TEST_AID,
	TX_ACL_ON,
	TX_ACL_OFF,
	TX_ELVSS,
	TX_ELVSS_HIGH,
	TX_ELVSS_MID,
	TX_ELVSS_LOW,
	TX_ELVSS_PRE,
	TX_GAMMA,
	TX_SMART_DIM_MTP,
	TX_HMT_ELVSS,
	TX_HMT_VINT,
	TX_HMT_IRC,
	TX_HMT_GAMMA,
	TX_HMT_AID,
	TX_ELVSS_LOWTEMP,
	TX_ELVSS_LOWTEMP2,
	TX_SMART_ACL_ELVSS,
	TX_SMART_ACL_ELVSS_LOWTEMP,
	TX_VINT,
	TX_IRC,
	TX_IRC_SUBDIVISION,
	TX_PAC_IRC_SUBDIVISION,
	TX_IRC_OFF,
	TX_SMOOTH_DIMMING_ON,
	TX_SMOOTH_DIMMING_OFF,
	TX_NORMAL_BRIGHTNESS_ETC,

	TX_MICRO_SHORT_TEST_ON,
	TX_MICRO_SHORT_TEST_OFF,
	TX_TCON_PE_ON,
	TX_TCON_PE_OFF,

	TX_POC_CMD_START, /* START POC CMDS */
	TX_POC_ENABLE,
	TX_POC_DISABLE,
	TX_POC_ERASE,			/* ERASE */
	TX_POC_ERASE1,
	TX_POC_PRE_ERASE_SECTOR,
	TX_POC_ERASE_SECTOR,
	TX_POC_POST_ERASE_SECTOR,
	TX_POC_PRE_WRITE,		/* WRITE */
	TX_POC_WRITE_LOOP_START,
	TX_POC_WRITE_LOOP_DATA_ADD,
	TX_POC_WRITE_LOOP_1BYTE,
	TX_POC_WRITE_LOOP_256BYTE,
	TX_POC_WRITE_LOOP_END,
	TX_POC_POST_WRITE,
	TX_POC_PRE_READ,		/* READ */
	TX_POC_PRE_READ2,		/* READ */
	TX_POC_READ,
	TX_POC_POST_READ,
	TX_POC_REG_READ_POS,
	TX_POC_CMD_END, /* END POC CMDS */
	TX_FW_UP_ERASE,
	TX_FW_UP_MTP_ID_ERASE,
	TX_FW_UP_WRITE,
	TX_FW_UP_MTP_ID_WRITE,
	TX_FW_UP_READ,
	TX_FLASH_CLEAR_BUFFER,
	TX_GCT_ENTER,
	TX_GCT_MID,
	TX_GCT_EXIT,
	TX_DDI_RAM_IMG_DATA,
	TX_GRAY_SPOT_TEST_ON,
	TX_GRAY_SPOT_TEST_OFF,
	TX_ISC_DEFECT_TEST_ON,
	TX_ISC_DEFECT_TEST_OFF,
	TX_PARTIAL_DISP_ON,
	TX_PARTIAL_DISP_OFF,
	TX_DIA_ON,
	TX_DIA_OFF,
	TX_MANUAL_DBV, /* For DIA */
	TX_SELF_IDLE_AOD_ENTER,
	TX_SELF_IDLE_AOD_EXIT,
	TX_SELF_IDLE_TIMER_ON,
	TX_SELF_IDLE_TIMER_OFF,
	TX_SELF_IDLE_MOVE_ON_PATTERN1,
	TX_SELF_IDLE_MOVE_ON_PATTERN2,
	TX_SELF_IDLE_MOVE_ON_PATTERN3,
	TX_SELF_IDLE_MOVE_ON_PATTERN4,
	TX_SELF_IDLE_MOVE_OFF,

	/* SELF DISPLAY */
	TX_SELF_DISP_CMD_START,
	TX_SELF_DISP_ON,
	TX_SELF_DISP_OFF,
	TX_SELF_TIME_SET,
	TX_SELF_MOVE_ON,
	TX_SELF_MOVE_ON_100,
	TX_SELF_MOVE_ON_200,
	TX_SELF_MOVE_ON_500,
	TX_SELF_MOVE_ON_1000,
	TX_SELF_MOVE_ON_DEBUG,
	TX_SELF_MOVE_RESET,
	TX_SELF_MOVE_OFF,
	TX_SELF_MOVE_2C_SYNC_OFF,
	TX_SELF_MASK_SET_PRE,
	TX_SELF_MASK_SET_POST,
	TX_SELF_MASK_SIDE_MEM_SET,
	TX_SELF_MASK_ON,
	TX_SELF_MASK_ON_FACTORY,
	TX_SELF_MASK_OFF,
	TX_SELF_MASK_IMAGE,
	TX_SELF_MASK_IMAGE_CRC,
	TX_SELF_ICON_SET_PRE,
	TX_SELF_ICON_SET_POST,
	TX_SELF_ICON_SIDE_MEM_SET,
	TX_SELF_ICON_GRID,
	TX_SELF_ICON_ON,
	TX_SELF_ICON_ON_GRID_ON,
	TX_SELF_ICON_ON_GRID_OFF,
	TX_SELF_ICON_OFF_GRID_ON,
	TX_SELF_ICON_OFF_GRID_OFF,
	TX_SELF_ICON_GRID_2C_SYNC_OFF,
	TX_SELF_ICON_OFF,
	TX_SELF_ICON_IMAGE,
	TX_SELF_BRIGHTNESS_ICON_ON,
	TX_SELF_BRIGHTNESS_ICON_OFF,
	TX_SELF_ACLOCK_SET_PRE,
	TX_SELF_ACLOCK_SET_POST,
	TX_SELF_ACLOCK_SIDE_MEM_SET,
	TX_SELF_ACLOCK_ON,
	TX_SELF_ACLOCK_TIME_UPDATE,
	TX_SELF_ACLOCK_ROTATION,
	TX_SELF_ACLOCK_OFF,
	TX_SELF_ACLOCK_HIDE,
	TX_SELF_ACLOCK_IMAGE,
	TX_SELF_DCLOCK_SET_PRE,
	TX_SELF_DCLOCK_SET_POST,
	TX_SELF_DCLOCK_SIDE_MEM_SET,
	TX_SELF_DCLOCK_ON,
	TX_SELF_DCLOCK_BLINKING_ON,
	TX_SELF_DCLOCK_BLINKING_OFF,
	TX_SELF_DCLOCK_TIME_UPDATE,
	TX_SELF_DCLOCK_OFF,
	TX_SELF_DCLOCK_HIDE,
	TX_SELF_DCLOCK_IMAGE,
	TX_SELF_CLOCK_2C_SYNC_OFF,
	TX_SELF_VIDEO_IMAGE,
	TX_SELF_VIDEO_SIDE_MEM_SET,
	TX_SELF_VIDEO_ON,
	TX_SELF_VIDEO_OFF,
	TX_SELF_PARTIAL_HLPM_SCAN_SET,
	RX_SELF_DISP_DEBUG,
	TX_SELF_MASK_CHECK_PRE1,
	TX_SELF_MASK_CHECK_PRE2,
	TX_SELF_MASK_CHECK_POST,
	RX_SELF_MASK_CHECK,
	TX_SELF_DISP_CMD_END,

	/* MAFPC */
	TX_MAFPC_CMD_START,
	TX_MAFPC_FLASH_SEL,
	TX_MAFPC_BRIGHTNESS_SCALE,
	RX_MAFPC_READ_1,
	RX_MAFPC_READ_2,
	RX_MAFPC_READ_3,
	TX_MAFPC_SET_PRE_FOR_INSTANT,
	TX_MAFPC_SET_PRE,
	TX_MAFPC_SET_POST,
	TX_MAFPC_SET_POST_FOR_INSTANT,
	TX_MAFPC_ON,
	TX_MAFPC_ON_FACTORY,
	TX_MAFPC_OFF,
	TX_MAFPC_TE_ON,
	TX_MAFPC_TE_OFF,
	TX_MAFPC_IMAGE,
	TX_MAFPC_CRC_CHECK_IMAGE,
	TX_MAFPC_CRC_CHECK_PRE1,
	TX_MAFPC_CRC_CHECK_PRE2,
	TX_MAFPC_CRC_CHECK_POST,
	RX_MAFPC_CRC_CHECK,
	TX_MAFPC_CMD_END,

	/* FLASH GAMMA */
	TX_FLASH_GAMMA_PRE1,
	TX_FLASH_GAMMA_PRE2,
	TX_FLASH_GAMMA,
	TX_FLASH_GAMMA_POST,

	TX_ON_PRE_SEQ,

	TX_ISC_DATA_THRESHOLD,
	TX_STM_ENABLE,
	TX_STM_DISABLE,
	TX_GAMMA_MODE1_INTERPOLATION,

	TX_SPI_IF_SEL_ON,
	TX_SPI_IF_SEL_OFF,

	TX_CCD_ON,
	TX_CCD_OFF,
	TX_POC_COMP,

	TX_FD_ON,
	TX_FD_OFF,

	TX_VRR,
	TX_VRR_GM2_GAMMA_COMP,

	TX_GREEN_WEIGHT_NORMAL,
	TX_GREEN_WEIGHT_80PERCENT,

	TX_DFPS,

	TX_ADJUST_TE,

	TX_FG_ERR,

	TX_CMD_END,

	/* RX */
	RX_CMD_START,
	RX_SMART_DIM_MTP,
	RX_CENTER_GAMMA_60HS,
	RX_CENTER_GAMMA_120HS,
	RX_MANUFACTURE_ID,
	RX_MANUFACTURE_ID0,
	RX_MANUFACTURE_ID1,
	RX_MANUFACTURE_ID2,
	RX_MODULE_INFO,
	RX_MANUFACTURE_DATE,
	RX_DDI_ID,
	RX_CELL_ID,
	RX_OCTA_ID,
	RX_OCTA_ID1,
	RX_OCTA_ID2,
	RX_OCTA_ID3,
	RX_OCTA_ID4,
	RX_OCTA_ID5,
	RX_RDDPM,
	RX_MTP_READ_SYSFS,
	RX_ELVSS,
	RX_IRC,
	RX_HBM,
	RX_HBM2,
	RX_HBM3,
	RX_MDNIE,
	RX_LDI_DEBUG0, /* 0x0A : RDDPM */
	RX_LDI_DEBUG1,
	RX_LDI_DEBUG2, /* 0xEE : ERR_FG */
	RX_LDI_DEBUG3, /* 0x0E : RDDSM */
	RX_LDI_DEBUG4, /* 0x05 : DSI_ERR */
	RX_LDI_DEBUG5, /* 0x0F : OTP loading error count */
	RX_LDI_DEBUG6, /* 0xE9 : MIPI protocol error  */
	RX_LDI_DEBUG_LOGBUF, /* 0x9C : command log buffer */
	RX_LDI_DEBUG_PPS1, /* 0xA2 : PPS data (0x00 ~ 0x2C) */
	RX_LDI_DEBUG_PPS2, /* 0xA2 : PPS data (0x2d ~ 0x58)*/
	RX_LDI_LOADING_DET,
	RX_LDI_FPS,
	RX_POC_READ,
	RX_POC_STATUS,
	RX_POC_CHECKSUM,
	RX_POC_MCA_CHECK,
	RX_FW_UP_READ,
	RX_FW_UP_MTP_ID_READ,
	RX_FW_UP_STATUS,
	RX_FW_UP_CHECK,
	RX_GCT_CHECKSUM,
	RX_MCD_READ_RESISTANCE,  /* For read real MCD R/L resistance */
	RX_FLASH_GAMMA,
	RX_CCD_STATE,
	RX_GRAYSPOT_RESTORE_VALUE,
	RX_VBIAS_MTP,	/* HOP display */
	RX_DDI_FW_ID,
	RX_ALPM_SET_VALUE,
	RX_CMD_END,

	SS_DSI_CMD_SET_MAX,
#endif
};

/**
 * enum dsi_cmd_set_state - command set state
 * @DSI_CMD_SET_STATE_LP:   dsi low power mode
 * @DSI_CMD_SET_STATE_HS:   dsi high speed mode
 * @DSI_CMD_SET_STATE_MAX
 */
enum dsi_cmd_set_state {
	DSI_CMD_SET_STATE_LP = 0,
	DSI_CMD_SET_STATE_HS,
	DSI_CMD_SET_STATE_MAX
};

/**
 * enum dsi_clk_gate_type - Type of clock to be gated.
 * @PIXEL_CLK:  DSI pixel clock.
 * @BYTE_CLK:   DSI byte clock.
 * @DSI_PHY:    DSI PHY.
 * @DSI_CLK_ALL: All available DSI clocks
 * @DSI_CLK_NONE: None of the clocks should be gated
 */
enum dsi_clk_gate_type {
	PIXEL_CLK = 1,
	BYTE_CLK = 2,
	DSI_PHY = 4,
	DSI_CLK_ALL = (PIXEL_CLK | BYTE_CLK | DSI_PHY),
	DSI_CLK_NONE = 8,
};

/**
 * enum dsi_phy_type - DSI phy types
 * @DSI_PHY_TYPE_DPHY:
 * @DSI_PHY_TYPE_CPHY:
 */
enum dsi_phy_type {
	DSI_PHY_TYPE_DPHY,
	DSI_PHY_TYPE_CPHY
};

/**
 * enum dsi_te_mode - dsi te source
 * @DSI_TE_ON_DATA_LINK:    TE read from DSI link
 * @DSI_TE_ON_EXT_PIN:      TE signal on an external GPIO
 */
enum dsi_te_mode {
	DSI_TE_ON_DATA_LINK = 0,
	DSI_TE_ON_EXT_PIN,
};

/**
 * enum dsi_video_traffic_mode - video mode pixel transmission type
 * @DSI_VIDEO_TRAFFIC_SYNC_PULSES:       Non-burst mode with sync pulses.
 * @DSI_VIDEO_TRAFFIC_SYNC_START_EVENTS: Non-burst mode with sync start events.
 * @DSI_VIDEO_TRAFFIC_BURST_MODE:        Burst mode using sync start events.
 */
enum dsi_video_traffic_mode {
	DSI_VIDEO_TRAFFIC_SYNC_PULSES = 0,
	DSI_VIDEO_TRAFFIC_SYNC_START_EVENTS,
	DSI_VIDEO_TRAFFIC_BURST_MODE,
};

/**
 * struct dsi_cmd_desc - description of a dsi command
 * @msg:		dsi mipi msg packet
 * @last_command:   indicates whether the cmd is the last one to send
 * @post_wait_ms:   post wait duration
 */
struct dsi_cmd_desc {
	struct mipi_dsi_msg msg;
	bool last_command;
	u32  post_wait_ms;
};

/**
 * struct dsi_panel_cmd_set - command set of the panel
 * @type:      type of the command
 * @state:     state of the command
 * @count:     number of cmds
 * @ctrl_idx:  index of the dsi control
 * @cmds:      arry of cmds
 */
struct dsi_panel_cmd_set {
	enum dsi_cmd_set_type type;
	enum dsi_cmd_set_state state;
	u32 count;
	u32 ctrl_idx;
	struct dsi_cmd_desc *cmds;

#if defined(CONFIG_DISPLAY_SAMSUNG)
#define SUPPORT_PANEL_REVISION	20
	int read_startoffset;
	char *name;
	int exclusive_pass;

	/* cmd_set_rev[panel_rev] is pointer to
	 * describe "struct dsi_panel_cmd_set *set" for each panel revision.
	 * If you want get cmd_set for panel revision A, get like below.
	 * struct dsi_panel_cmd_set *set = set->cmd_set_rev[panel_rev];
	 */
	void *cmd_set_rev[SUPPORT_PANEL_REVISION];

	void *self_disp_cmd_set_rev;
#endif
};

/**
 * struct dsi_mode_info - video mode information dsi frame
 * @h_active:         Active width of one frame in pixels.
 * @h_back_porch:     Horizontal back porch in pixels.
 * @h_sync_width:     HSYNC width in pixels.
 * @h_front_porch:    Horizontal fron porch in pixels.
 * @h_skew:
 * @h_sync_polarity:  Polarity of HSYNC (false is active low).
 * @v_active:         Active height of one frame in lines.
 * @v_back_porch:     Vertical back porch in lines.
 * @v_sync_width:     VSYNC width in lines.
 * @v_front_porch:    Vertical front porch in lines.
 * @v_sync_polarity:  Polarity of VSYNC (false is active low).
 * @refresh_rate:     Refresh rate in Hz.
 * @clk_rate_hz:      DSI bit clock rate per lane in Hz.
 * @min_dsi_clk_hz:   Min DSI bit clock to transfer in vsync time.
 * @mdp_transfer_time_us:   Specifies the mdp transfer time for command mode
 *                    panels in microseconds.
 * @dsi_transfer_time_us:   Specifies dsi transfer time for command mode.
 * @dsc_enabled:      DSC compression enabled.
 * @dsc:              DSC compression configuration.
 * @roi_caps:         Panel ROI capabilities.
 */
struct dsi_mode_info {
	u32 h_active;
	u32 h_back_porch;
	u32 h_sync_width;
	u32 h_front_porch;
	u32 h_skew;
	bool h_sync_polarity;

	u32 v_active;
	u32 v_back_porch;
	u32 v_sync_width;
	u32 v_front_porch;
	bool v_sync_polarity;

	u32 refresh_rate;
	u64 clk_rate_hz;
	u64 min_dsi_clk_hz;
	u32 mdp_transfer_time_us;
	u32 dsi_transfer_time_us;
	bool dsc_enabled;
	struct msm_display_dsc_info *dsc;
	struct msm_roi_caps roi_caps;
#if defined(CONFIG_DISPLAY_SAMSUNG)
	/* Identify VRR HS by drm_mode's name.
	 * drm_mode's name is defined by dsi_mode->timing.sot_hs_mode parsed
	 * from samsung,mdss-dsi-sot-hs-mode in panel dtsi file.
	 * ex) drm_mode->name is "1080x2316x60x193345cmdHS" for HS mode.
	 *     drm_mode->name is "1080x2316x60x193345cmdNS" for NS mode.
	 * To use this feature, declare different porch between HS and NS modes,
	 * in panel dtsi file.
	 * Refer to ss_is_sot_hs_from_drm_mode().
	 */
	bool sot_hs_mode;
#endif
};

/**
 * struct dsi_split_link_config - Split Link Configuration
 * @split_link_enabled:  Split Link Enabled.
 * @num_sublinks:     Number of sublinks.
 * @lanes_per_sublink:   Number of lanes per sublink.
 */
struct dsi_split_link_config {
	bool split_link_enabled;
	u32 num_sublinks;
	u32 lanes_per_sublink;
};

/**
 * struct dsi_host_common_cfg - Host configuration common to video and cmd mode
 * @dst_format:          Destination pixel format.
 * @data_lanes:          Physical data lanes to be enabled.
 * @num_data_lanes:      Number of physical data lanes.
 * @bpp:                 Number of bits per pixel.
 * @en_crc_check:        Enable CRC checks.
 * @en_ecc_check:        Enable ECC checks.
 * @te_mode:             Source for TE signalling.
 * @mdp_cmd_trigger:     MDP frame update trigger for command mode.
 * @dma_cmd_trigger:     Command DMA trigger.
 * @cmd_trigger_stream:  Command mode stream to trigger.
 * @swap_mode:           DSI color swap mode.
 * @bit_swap_read:       Is red color bit swapped.
 * @bit_swap_green:      Is green color bit swapped.
 * @bit_swap_blue:       Is blue color bit swapped.
 * @t_clk_post:          Number of byte clock cycles that the transmitter shall
 *                       continue sending after last data lane has transitioned
 *                       to LP mode.
 * @t_clk_pre:           Number of byte clock cycles that the high spped clock
 *                       shall be driven prior to data lane transitions from LP
 *                       to HS mode.
 * @t_clk_pre_extend:    Increment t_clk_pre counter by 2 byteclk if set to
 *                       true, otherwise increment by 1 byteclk.
 * @ignore_rx_eot:       Ignore Rx EOT packets if set to true.
 * @append_tx_eot:       Append EOT packets for forward transmissions if set to
 *                       true.
 * @ext_bridge_mode:     External bridge is connected.
 * @force_hs_clk_lane:   Send continuous clock to the panel.
 * @phy_type:            DPHY/CPHY is enabled for this panel.
 * @dsi_split_link_config:  Split Link Configuration.
 * @byte_intf_clk_div:   Determines the factor for calculating byte intf clock.
 */
struct dsi_host_common_cfg {
	enum dsi_pixel_format dst_format;
	enum dsi_data_lanes data_lanes;
	u8 num_data_lanes;
	u8 bpp;
	bool en_crc_check;
	bool en_ecc_check;
	enum dsi_te_mode te_mode;
	enum dsi_trigger_type mdp_cmd_trigger;
	enum dsi_trigger_type dma_cmd_trigger;
	u32 cmd_trigger_stream;
	enum dsi_color_swap_mode swap_mode;
	bool bit_swap_red;
	bool bit_swap_green;
	bool bit_swap_blue;
	u32 t_clk_post;
	u32 t_clk_pre;
	bool t_clk_pre_extend;
	bool ignore_rx_eot;
	bool append_tx_eot;
	bool ext_bridge_mode;
	bool force_hs_clk_lane;
	enum dsi_phy_type phy_type;
	struct dsi_split_link_config split_link;
	u32 byte_intf_clk_div;
};

/**
 * struct dsi_video_engine_cfg - DSI video engine configuration
 * @last_line_interleave_en:   Allow command mode op interleaved on last line of
 *                             video stream.
 * @pulse_mode_hsa_he:         Send HSA and HE following VS/VE packet if set to
 *                             true.
 * @hfp_lp11_en:               Enter low power stop mode (LP-11) during HFP.
 * @hbp_lp11_en:               Enter low power stop mode (LP-11) during HBP.
 * @hsa_lp11_en:               Enter low power stop mode (LP-11) during HSA.
 * @eof_bllp_lp11_en:          Enter low power stop mode (LP-11) during BLLP of
 *                             last line of a frame.
 * @bllp_lp11_en:              Enter low power stop mode (LP-11) during BLLP.
 * @traffic_mode:              Traffic mode for video stream.
 * @vc_id:                     Virtual channel identifier.
 * @dma_sched_line:         Line number, after vactive end, at which command dma
 *			       needs to be triggered.
 */
struct dsi_video_engine_cfg {
	bool last_line_interleave_en;
	bool pulse_mode_hsa_he;
	bool hfp_lp11_en;
	bool hbp_lp11_en;
	bool hsa_lp11_en;
	bool eof_bllp_lp11_en;
	bool bllp_lp11_en;
	enum dsi_video_traffic_mode traffic_mode;
	u32 vc_id;
	u32 dma_sched_line;
};

/**
 * struct dsi_cmd_engine_cfg - DSI command engine configuration
 * @max_cmd_packets_interleave     Maximum number of command mode RGB packets to
 *                                 send with in one horizontal blanking period
 *                                 of the video mode frame.
 * @wr_mem_start:                  DCS command for write_memory_start.
 * @wr_mem_continue:               DCS command for write_memory_continue.
 * @insert_dcs_command:            Insert DCS command as first byte of payload
 *                                 of the pixel data.
 */
struct dsi_cmd_engine_cfg {
	u32 max_cmd_packets_interleave;
	u32 wr_mem_start;
	u32 wr_mem_continue;
	bool insert_dcs_command;
};

/**
 * struct dsi_host_config - DSI host configuration parameters.
 * @panel_mode:            Operation mode for panel (video or cmd mode).
 * @common_config:         Host configuration common to both Video and Cmd mode.
 * @video_engine:          Video engine configuration if panel is in video mode.
 * @cmd_engine:            Cmd engine configuration if panel is in cmd mode.
 * @esc_clk_rate_hz:      Esc clock frequency in Hz.
 * @bit_clk_rate_hz:       Bit clock frequency in Hz.
 * @bit_clk_rate_hz_override: DSI bit clk rate override from dt/sysfs.
 * @video_timing:          Video timing information of a frame.
 * @lane_map:              Mapping between logical and physical lanes.
 */
struct dsi_host_config {
	enum dsi_op_mode panel_mode;
	struct dsi_host_common_cfg common_config;
	union {
		struct dsi_video_engine_cfg video_engine;
		struct dsi_cmd_engine_cfg cmd_engine;
	} u;
	u64 esc_clk_rate_hz;
	u64 bit_clk_rate_hz;
	u64 bit_clk_rate_hz_override;
	struct dsi_mode_info video_timing;
	struct dsi_lane_map lane_map;
};

/**
 * struct dsi_display_mode_priv_info - private mode info that will be attached
 *                             with each drm mode
 * @cmd_sets:		  Command sets of the mode
 * @phy_timing_val:       Phy timing values
 * @phy_timing_len:       Phy timing array length
 * @panel_jitter:         Panel jitter for RSC backoff
 * @panel_prefill_lines:  Panel prefill lines for RSC
 * @mdp_transfer_time_us:   Specifies the mdp transfer time for command mode
 *                          panels in microseconds.
 * @dsi_transfer_time_us: Specifies the dsi transfer time for cmd panels.
 * @clk_rate_hz:          DSI bit clock per lane in hz.
 * @min_dsi_clk_hz:       Min dsi clk per lane to transfer frame in vsync time.
 * @topology:             Topology selected for the panel
 * @dsc:                  DSC compression info
 * @dsc_enabled:          DSC compression enabled
 * @roi_caps:		  Panel ROI capabilities
 */
struct dsi_display_mode_priv_info {
#if !defined(CONFIG_DISPLAY_SAMSUNG)
	struct dsi_panel_cmd_set cmd_sets[DSI_CMD_SET_MAX];
#else
	struct dsi_panel_cmd_set cmd_sets[SS_DSI_CMD_SET_MAX];
#endif

	u32 *phy_timing_val;
	u32 phy_timing_len;

	u32 panel_jitter_numer;
	u32 panel_jitter_denom;
	u32 panel_prefill_lines;
	u32 mdp_transfer_time_us;
	u32 dsi_transfer_time_us;
	u64 clk_rate_hz;
	u64 min_dsi_clk_hz;

	struct msm_display_topology topology;
	struct msm_display_dsc_info dsc;
	bool dsc_enabled;
	struct msm_roi_caps roi_caps;
};

/**
 * struct dsi_display_mode - specifies mode for dsi display
 * @timing:         Timing parameters for the panel.
 * @pixel_clk_khz:  Pixel clock in Khz.
 * @dsi_mode_flags: Flags to signal other drm components via private flags
 * @panel_mode:      Panel mode
 * @is_preferred:   Is mode preferred
 * @priv_info:      Mode private info
 */
struct dsi_display_mode {
	struct dsi_mode_info timing;
	u32 pixel_clk_khz;
	u32 dsi_mode_flags;
	enum dsi_op_mode panel_mode;
	bool is_preferred;
	struct dsi_display_mode_priv_info *priv_info;
};

/**
 * struct dsi_rect - dsi rectangle representation
 * Note: sde_rect is also using u16, this must be maintained for memcpy
 */
struct dsi_rect {
	u16 x;
	u16 y;
	u16 w;
	u16 h;
};

/**
 * dsi_rect_intersect - intersect two rectangles
 * @r1: first rectangle
 * @r2: scissor rectangle
 * @result: result rectangle, all 0's on no intersection found
 */
void dsi_rect_intersect(const struct dsi_rect *r1,
		const struct dsi_rect *r2,
		struct dsi_rect *result);

/**
 * dsi_rect_is_equal - compares two rects
 * @r1: rect value to compare
 * @r2: rect value to compare
 *
 * Returns true if the rects are same
 */
static inline bool dsi_rect_is_equal(struct dsi_rect *r1,
		struct dsi_rect *r2)
{
	return r1->x == r2->x && r1->y == r2->y && r1->w == r2->w &&
			r1->h == r2->h;
}

struct dsi_event_cb_info {
	uint32_t event_idx;
	void *event_usr_ptr;

	int (*event_cb)(void *event_usr_ptr,
		uint32_t event_idx, uint32_t instance_idx,
		uint32_t data0, uint32_t data1,
		uint32_t data2, uint32_t data3);
};

/**
 * enum dsi_error_status - various dsi errors
 * @DSI_FIFO_OVERFLOW:     DSI FIFO Overflow error
 * @DSI_FIFO_UNDERFLOW:    DSI FIFO Underflow error
 * @DSI_LP_Rx_TIMEOUT:     DSI LP/RX Timeout error
 * @DSI_PLL_UNLOCK_ERR:	   DSI PLL unlock error
 */
enum dsi_error_status {
	DSI_FIFO_OVERFLOW = 1,
	DSI_FIFO_UNDERFLOW,
	DSI_LP_Rx_TIMEOUT,
	DSI_PLL_UNLOCK_ERR,
	DSI_ERR_INTR_ALL,
};

/* structure containing the delays required for dynamic clk */
struct dsi_dyn_clk_delay {
	u32 pipe_delay;
	u32 pipe_delay2;
	u32 pll_delay;
};

/* dynamic refresh control bits */
enum dsi_dyn_clk_control_bits {
	DYN_REFRESH_INTF_SEL = 1,
	DYN_REFRESH_SYNC_MODE,
	DYN_REFRESH_SW_TRIGGER,
	DYN_REFRESH_SWI_CTRL,
};

/* convert dsi pixel format into bits per pixel */
static inline int dsi_pixel_format_to_bpp(enum dsi_pixel_format fmt)
{
	switch (fmt) {
	case DSI_PIXEL_FORMAT_RGB888:
	case DSI_PIXEL_FORMAT_MAX:
		return 24;
	case DSI_PIXEL_FORMAT_RGB666:
	case DSI_PIXEL_FORMAT_RGB666_LOOSE:
		return 18;
	case DSI_PIXEL_FORMAT_RGB565:
		return 16;
	case DSI_PIXEL_FORMAT_RGB111:
		return 3;
	case DSI_PIXEL_FORMAT_RGB332:
		return 8;
	case DSI_PIXEL_FORMAT_RGB444:
		return 12;
	}
	return 24;
}
#endif /* _DSI_DEFS_H_ */
