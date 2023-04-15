/* SPDX-License-Identifier: GPL-2.0 */
/******************************************************************************
 *
 * Copyright(c) 2016  Realtek Corporation.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

/* *******************************************
 * The following is for 8822B 1ANT BT Co-exist definition
 * ********************************************/
#define BT_INFO_8822B_1ANT_B_FTP	BIT(7)
#define BT_INFO_8822B_1ANT_B_A2DP	BIT(6)
#define BT_INFO_8822B_1ANT_B_HID	BIT(5)
#define BT_INFO_8822B_1ANT_B_SCO_BUSY	BIT(4)
#define BT_INFO_8822B_1ANT_B_ACL_BUSY	BIT(3)
#define BT_INFO_8822B_1ANT_B_INQ_PAGE	BIT(2)
#define BT_INFO_8822B_1ANT_B_SCO_ESCO	BIT(1)
#define BT_INFO_8822B_1ANT_B_CONNECTION	BIT(0)

#define BT_INFO_8822B_1ANT_A2DP_BASIC_RATE(_BT_INFO_EXT_)                      \
	(((_BT_INFO_EXT_ & BIT(0))) ? true : false)

#define BTC_RSSI_COEX_THRESH_TOL_8822B_1ANT	2

#define BT_8822B_1ANT_WIFI_NOISY_THRESH	150 /* max: 255 */
#define BT_8822B_1ANT_DEFAULT_ISOLATION	15 /*  unit: dB */

/* for Antenna detection */
#define BT_8822B_1ANT_ANTDET_PSDTHRES_BACKGROUND	50
#define BT_8822B_1ANT_ANTDET_PSDTHRES_2ANT_BADISOLATION	70
#define BT_8822B_1ANT_ANTDET_PSDTHRES_2ANT_GOODISOLATION	55
#define BT_8822B_1ANT_ANTDET_PSDTHRES_1ANT	35
#define BT_8822B_1ANT_ANTDET_RETRY_INTERVAL                                    \
	10 /* retry timer if ant det is fail, unit: second */
#define BT_8822B_1ANT_ANTDET_ENABLE	0
#define BT_8822B_1ANT_ANTDET_COEXMECHANISMSWITCH_ENABLE	0

#define BT_8822B_1ANT_LTECOEX_INDIRECTREG_ACCESS_TIMEOUT	30000

enum bt_8822b_1ant_signal_state {
	BT_8822B_1ANT_SIG_STA_SET_TO_LOW	= 0x0,
	BT_8822B_1ANT_SIG_STA_SET_BY_HW	= 0x0,
	BT_8822B_1ANT_SIG_STA_SET_TO_HIGH	= 0x1,
	BT_8822B_1ANT_SIG_STA_MAX
};

enum bt_8822b_1ant_path_ctrl_owner {
	BT_8822B_1ANT_PCO_BTSIDE	= 0x0,
	BT_8822B_1ANT_PCO_WLSIDE	= 0x1,
	BT_8822B_1ANT_PCO_MAX
};

enum bt_8822b_1ant_gnt_ctrl_type {
	BT_8822B_1ANT_GNT_CTRL_BY_PTA	= 0x0,
	BT_8822B_1ANT_GNT_CTRL_BY_SW	= 0x1,
	BT_8822B_1ANT_GNT_CTRL_MAX
};

enum bt_8822b_1ant_gnt_ctrl_block {
	BT_8822B_1ANT_GNT_BLOCK_RFC_BB	= 0x0,
	BT_8822B_1ANT_GNT_BLOCK_RFC	= 0x1,
	BT_8822B_1ANT_GNT_BLOCK_BB	= 0x2,
	BT_8822B_1ANT_GNT_BLOCK_MAX
};

enum bt_8822b_1ant_lte_coex_table_type {
	BT_8822B_1ANT_CTT_WL_VS_LTE	= 0x0,
	BT_8822B_1ANT_CTT_BT_VS_LTE	= 0x1,
	BT_8822B_1ANT_CTT_MAX
};

enum bt_8822b_1ant_lte_break_table_type {
	BT_8822B_1ANT_LBTT_WL_BREAK_LTE	= 0x0,
	BT_8822B_1ANT_LBTT_BT_BREAK_LTE	= 0x1,
	BT_8822B_1ANT_LBTT_LTE_BREAK_WL	= 0x2,
	BT_8822B_1ANT_LBTT_LTE_BREAK_BT	= 0x3,
	BT_8822B_1ANT_LBTT_MAX
};

enum bt_info_src_8822b_1ant {
	BT_INFO_SRC_8822B_1ANT_WIFI_FW	= 0x0,
	BT_INFO_SRC_8822B_1ANT_BT_RSP	= 0x1,
	BT_INFO_SRC_8822B_1ANT_BT_ACTIVE_SEND	= 0x2,
	BT_INFO_SRC_8822B_1ANT_MAX
};

enum bt_8822b_1ant_bt_status {
	BT_8822B_1ANT_BT_STATUS_NON_CONNECTED_IDLE	= 0x0,
	BT_8822B_1ANT_BT_STATUS_CONNECTED_IDLE	= 0x1,
	BT_8822B_1ANT_BT_STATUS_INQ_PAGE	= 0x2,
	BT_8822B_1ANT_BT_STATUS_ACL_BUSY	= 0x3,
	BT_8822B_1ANT_BT_STATUS_SCO_BUSY	= 0x4,
	BT_8822B_1ANT_BT_STATUS_ACL_SCO_BUSY	= 0x5,
	BT_8822B_1ANT_BT_STATUS_MAX
};

enum bt_8822b_1ant_wifi_status {
	BT_8822B_1ANT_WIFI_STATUS_NON_CONNECTED_IDLE	= 0x0,
	BT_8822B_1ANT_WIFI_STATUS_NON_CONNECTED_ASSO_AUTH_SCAN	= 0x1,
	BT_8822B_1ANT_WIFI_STATUS_CONNECTED_SCAN	= 0x2,
	BT_8822B_1ANT_WIFI_STATUS_CONNECTED_SPECIFIC_PKT	= 0x3,
	BT_8822B_1ANT_WIFI_STATUS_CONNECTED_IDLE	= 0x4,
	BT_8822B_1ANT_WIFI_STATUS_CONNECTED_BUSY	= 0x5,
	BT_8822B_1ANT_WIFI_STATUS_MAX
};

enum bt_8822b_1ant_coex_algo {
	BT_8822B_1ANT_COEX_ALGO_UNDEFINED	= 0x0,
	BT_8822B_1ANT_COEX_ALGO_SCO	= 0x1,
	BT_8822B_1ANT_COEX_ALGO_HID	= 0x2,
	BT_8822B_1ANT_COEX_ALGO_A2DP	= 0x3,
	BT_8822B_1ANT_COEX_ALGO_A2DP_PANHS	= 0x4,
	BT_8822B_1ANT_COEX_ALGO_PANEDR	= 0x5,
	BT_8822B_1ANT_COEX_ALGO_PANHS	= 0x6,
	BT_8822B_1ANT_COEX_ALGO_PANEDR_A2DP	= 0x7,
	BT_8822B_1ANT_COEX_ALGO_PANEDR_HID	= 0x8,
	BT_8822B_1ANT_COEX_ALGO_HID_A2DP_PANEDR	= 0x9,
	BT_8822B_1ANT_COEX_ALGO_HID_A2DP	= 0xa,
	BT_8822B_1ANT_COEX_ALGO_MAX	= 0xb,
};

enum bt_8822b_1ant_ext_ant_switch_type {
	BT_8822B_1ANT_EXT_ANT_SWITCH_USE_SPDT	= 0x0,
	BT_8822B_1ANT_EXT_ANT_SWITCH_USE_SP3T	= 0x1,
	BT_8822B_1ANT_EXT_ANT_SWITCH_MAX
};

enum bt_8822b_1ant_ext_ant_switch_ctrl_type {
	BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_BBSW	= 0x0,
	BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_PTA	= 0x1,
	BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_ANTDIV	= 0x2,
	BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_MAC	= 0x3,
	BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_BY_BT	= 0x4,
	BT_8822B_1ANT_EXT_ANT_SWITCH_CTRL_MAX
};

enum bt_8822b_1ant_ext_ant_switch_pos_type {
	BT_8822B_1ANT_EXT_ANT_SWITCH_TO_BT	= 0x0,
	BT_8822B_1ANT_EXT_ANT_SWITCH_TO_WLG	= 0x1,
	BT_8822B_1ANT_EXT_ANT_SWITCH_TO_WLA	= 0x2,
	BT_8822B_1ANT_EXT_ANT_SWITCH_TO_NOCARE	= 0x3,
	BT_8822B_1ANT_EXT_ANT_SWITCH_TO_MAX
};

enum bt_8822b_1ant_phase {
	BT_8822B_1ANT_PHASE_COEX_INIT	= 0x0,
	BT_8822B_1ANT_PHASE_WLANONLY_INIT	= 0x1,
	BT_8822B_1ANT_PHASE_WLAN_OFF	= 0x2,
	BT_8822B_1ANT_PHASE_2G_RUNTIME	= 0x3,
	BT_8822B_1ANT_PHASE_5G_RUNTIME	= 0x4,
	BT_8822B_1ANT_PHASE_BTMPMODE	= 0x5,
	BT_8822B_1ANT_PHASE_MAX
};

/*ADD SCOREBOARD TO FIX BT LPS 32K ISSUE WHILE WL BUSY*/
enum bt_8822b_1ant_scoreboard {
	BT_8822B_1ANT_SCOREBOARD_ACTIVE	= BIT(0),
	BT_8822B_1ANT_SCOREBOARD_ONOFF	= BIT(1),
	BT_8822B_1ANT_SCOREBOARD_SCAN	= BIT(2),
	BT_8822B_1ANT_SCOREBOARD_UNDERTEST	= BIT(3),
	BT_8822B_1ANT_SCOREBOARD_WLBUSY = BIT(6)
};

struct coex_dm_8822b_1ant {
	/* hw setting */
	u32	pre_ant_pos_type;
	u32	cur_ant_pos_type;
	/* fw mechanism */
	bool	cur_ignore_wlan_act;
	bool	pre_ignore_wlan_act;
	u8	pre_ps_tdma;
	u8	cur_ps_tdma;
	u8	ps_tdma_para[5];
	u8	ps_tdma_du_adj_type;
	bool	auto_tdma_adjust;
	bool	pre_ps_tdma_on;
	bool	cur_ps_tdma_on;
	bool	pre_bt_auto_report;
	bool	cur_bt_auto_report;
	u8	pre_lps;
	u8	cur_lps;
	u8	pre_rpwm;
	u8	cur_rpwm;

	/* sw mechanism */
	bool	pre_low_penalty_ra;
	bool	cur_low_penalty_ra;
	u32	pre_val0x6c0;
	u32	cur_val0x6c0;
	u32	pre_val0x6c4;
	u32	cur_val0x6c4;
	u32	pre_val0x6c8;
	u32	cur_val0x6c8;
	u8	pre_val0x6cc;
	u8	cur_val0x6cc;
	bool	limited_dig;

	u32	backup_arfr_cnt1; /* Auto Rate Fallback Retry cnt */
	u32	backup_arfr_cnt2; /* Auto Rate Fallback Retry cnt */
	u16	backup_retry_limit;
	u8	backup_ampdu_max_time;

	/* algorithm related */
	u8	pre_algorithm;
	u8	cur_algorithm;
	u8	bt_status;
	u8	wifi_chnl_info[3];

	u32	pre_ra_mask;
	u32	cur_ra_mask;
	u8	pre_arfr_type;
	u8	cur_arfr_type;
	u8	pre_retry_limit_type;
	u8	cur_retry_limit_type;
	u8	pre_ampdu_time_type;
	u8	cur_ampdu_time_type;
	u32	arp_cnt;

	u32	pre_ext_ant_switch_status;
	u32	cur_ext_ant_switch_status;

	u8	error_condition;
};

struct coex_sta_8822b_1ant {
	bool	bt_disabled;
	bool	bt_link_exist;
	bool	sco_exist;
	bool	a2dp_exist;
	bool	hid_exist;
	bool	pan_exist;
	u8	num_of_profile;

	bool	under_lps;
	bool	under_ips;
	u32	specific_pkt_period_cnt;
	u32	high_priority_tx;
	u32	high_priority_rx;
	u32	low_priority_tx;
	u32	low_priority_rx;
	bool	is_hi_pri_rx_overhead;
	s8	bt_rssi;
	u8	pre_bt_rssi_state;
	u8	pre_wifi_rssi_state[4];
	u8	bt_info_c2h[BT_INFO_SRC_8822B_1ANT_MAX][10];
	u32	bt_info_c2h_cnt[BT_INFO_SRC_8822B_1ANT_MAX];
	bool	bt_whck_test;
	bool	c2h_bt_inquiry_page;
	bool	c2h_bt_remote_name_req;
	bool	c2h_bt_page; /* Add for win8.1 page out issue */
	bool	wifi_is_high_pri_task; /* Add for win8.1 page out issue */

	u8	bt_info_ext;
	u8	bt_info_ext2;
	u32	pop_event_cnt;
	u8	scan_ap_num;
	u8	bt_retry_cnt;

	u32	crc_ok_cck;
	u32	crc_ok_11g;
	u32	crc_ok_11n;
	u32	crc_ok_11n_vht;

	u32	crc_err_cck;
	u32	crc_err_11g;
	u32	crc_err_11n;
	u32	crc_err_11n_vht;

	bool	cck_lock;
	bool	pre_ccklock;
	bool	cck_ever_lock;
	u8	coex_table_type;

	bool	force_lps_ctrl;

	bool	concurrent_rx_mode_on;

	u16	score_board;
	u8	isolation_btween_wb; /* 0~ 50 */

	u8	a2dp_bit_pool;
	u8	cut_version;
	bool	acl_busy;
	bool	bt_create_connection;

	u32	bt_coex_supported_feature;
	u32	bt_coex_supported_version;

	u8	bt_ble_scan_type;
	u32	bt_ble_scan_para[3];

	bool	run_time_state;
	bool	freeze_coexrun_by_btinfo;

	bool	is_A2DP_3M;
	bool	voice_over_HOGP;
	u8	bt_info;
	bool	is_autoslot;
	u8	forbidden_slot;
	u8	hid_busy_num;
	u8	hid_pair_cnt;

	u32	cnt_remote_name_req;
	u32	cnt_setup_link;
	u32	cnt_reinit;
	u32	cnt_ign_wlan_act;
	u32	cnt_page;
	u32	cnt_role_switch;

	u16	bt_reg_vendor_ac;
	u16	bt_reg_vendor_ae;

	bool	is_setup_link;
	u8	wl_noisy_level;
	u32	gnt_error_cnt;
	u8	bt_afh_map[10];
	u8	bt_relink_downcount;
	bool	is_tdma_btautoslot;
	bool	is_tdma_btautoslot_hang;

	u8	switch_band_notify_to;
	bool	is_rf_state_off;

	bool	is_hid_low_pri_tx_overhead;
	bool	is_bt_multi_link;
	bool	is_bt_a2dp_sink;
	bool	rf4ce_enabled;

	bool	is_set_ps_state_fail;
	u8	cnt_set_ps_state_fail;
};

struct rfe_type_8822b_1ant {
	u8	rfe_module_type;
	bool	ext_ant_switch_exist;
	u8	ext_ant_switch_type;
	/*  iF 0: ANTSW(rfe_sel9)=0, ANTSWB(rfe_sel8)=1 =>  Ant to BT/5G */
	u8	ext_ant_switch_ctrl_polarity;
};

#define BT_8822B_1ANT_ANTDET_PSD_POINTS	256 /* MAX:1024 */
#define BT_8822B_1ANT_ANTDET_PSD_AVGNUM	1 /* MAX:3 */
#define BT_8822B_1ANT_ANTDET_BUF_LEN	16

struct psdscan_sta_8822b_1ant {
	u32	ant_det_bt_le_channel; /* BT LE Channel ex:2412 */
	u32	ant_det_bt_tx_time;
	u32	ant_det_pre_psdscan_peak_val;
	bool	ant_det_is_ant_det_available;
	u32	ant_det_psd_scan_peak_val;
	bool	ant_det_is_btreply_available;
	u32	ant_det_psd_scan_peak_freq;

	u8	ant_det_result;
	u8	ant_det_peak_val[BT_8822B_1ANT_ANTDET_BUF_LEN];
	u8	ant_det_peak_freq[BT_8822B_1ANT_ANTDET_BUF_LEN];
	u32	ant_det_try_count;
	u32	ant_det_fail_count;
	u32	ant_det_inteval_count;
	u32	ant_det_thres_offset;

	u32	real_cent_freq;
	s32	real_offset;
	u32	real_span;

	u32	psd_band_width; /* unit: Hz */
	u32	psd_point; /* 128/256/512/1024 */
	u32	psd_report[1024]; /* unit:dB (20logx), 0~255 */
	u32	psd_report_max_hold[1024]; /* unit:dB (20logx), 0~255 */
	u32	psd_start_point;
	u32	psd_stop_point;
	u32	psd_max_value_point;
	u32	psd_max_value;
	u32	psd_start_base;
	u32	psd_avg_num; /* 1/8/16/32 */
	u32	psd_gen_count;
	bool	is_psd_running;
	bool	is_psd_show_max_only;
	bool	is_ant_det_running;
};

/* *******************************************
 * The following is interface which will notify coex module.
 * ********************************************/
void ex_btc8822b1ant_power_on_setting(struct btc_coexist *btcoexist);
void ex_btc8822b1ant_pre_load_firmware(struct btc_coexist *btcoexist);
void ex_btc8822b1ant_init_hw_config(struct btc_coexist *btcoexist,
				    bool wifi_only);
void ex_btc8822b1ant_init_coex_dm(struct btc_coexist *btcoexist);
void ex_btc8822b1ant_ips_notify(struct btc_coexist *btcoexist, u8 type);
void ex_btc8822b1ant_lps_notify(struct btc_coexist *btcoexist, u8 type);
void ex_btc8822b1ant_scan_notify(struct btc_coexist *btcoexist, u8 type);
void ex_btc8822b1ant_scan_notify_without_bt(struct btc_coexist *btcoexist,
					    u8 type);
void ex_btc8822b1ant_switchband_notify(struct btc_coexist *btcoexist, u8 type);
void ex_btc8822b1ant_switchband_notify_without_bt(struct btc_coexist *btcoexist,
						  u8 type);
void ex_btc8822b1ant_connect_notify(struct btc_coexist *btcoexist, u8 type);
void ex_btc8822b1ant_media_status_notify(struct btc_coexist *btcoexist,
					 u8 type);
void ex_btc8822b1ant_specific_packet_notify(struct btc_coexist *btcoexist,
					    u8 type);
void ex_btc8822b1ant_bt_info_notify(struct btc_coexist *btcoexist, u8 *tmp_buf,
				    u8 length);
void ex_btc8822b1ant_rf_status_notify(struct btc_coexist *btcoexist, u8 type);
void ex_btc8822b1ant_halt_notify(struct btc_coexist *btcoexist);
void ex_btc8822b1ant_pnp_notify(struct btc_coexist *btcoexist, u8 pnp_state);
void ex_halbtc8822b1ant_score_board_status_notify(struct btc_coexist *btcoexist,
						  u8 *tmp_buf, u8 length);
void ex_btc8822b1ant_coex_dm_reset(struct btc_coexist *btcoexist);
void ex_btc8822b1ant_periodical(struct btc_coexist *btcoexist);
void ex_btc8822b1ant_display_coex_info(struct btc_coexist *btcoexist,
				       struct seq_file *m);
void ex_btc8822b1ant_antenna_detection(struct btc_coexist *btcoexist,
				       u32 cent_freq, u32 offset, u32 span,
				       u32 seconds);
void ex_btc8822b1ant_antenna_isolation(struct btc_coexist *btcoexist,
				       u32 cent_freq, u32 offset, u32 span,
				       u32 seconds);

void ex_btc8822b1ant_psd_scan(struct btc_coexist *btcoexist, u32 cent_freq,
			      u32 offset, u32 span, u32 seconds);
void ex_btc8822b1ant_display_ant_detection(struct btc_coexist *btcoexist);

void ex_btc8822b1ant_dbg_control(struct btc_coexist *btcoexist, u8 op_code,
				 u8 op_len, u8 *pdata);
