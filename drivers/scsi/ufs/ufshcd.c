/*
 * Universal Flash Storage Host controller driver Core
 *
 * This code is based on drivers/scsi/ufs/ufshcd.c
 * Copyright (C) 2011-2013 Samsung India Software Operations
 * Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
 *
 * Authors:
 *	Santosh Yaraganavi <santosh.sy@samsung.com>
 *	Vinayak Holikatti <h.vinayak@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * See the COPYING file in the top-level directory or visit
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program is provided "AS IS" and "WITH ALL FAULTS" and
 * without warranty of any kind. You are solely responsible for
 * determining the appropriateness of using and distributing
 * the program and assume all risks associated with your exercise
 * of rights with respect to the program, including but not limited
 * to infringement of third party rights, the risks and costs of
 * program errors, damage to or loss of data, programs or equipment,
 * and unavailability or interruption of operations. Under no
 * circumstances will the contributor of this Program be liable for
 * any damages of any kind arising from your use or distribution of
 * this program.
 *
 * The Linux Foundation chooses to take subject only to the GPLv2
 * license terms, and distributes only under these terms.
 */

#include <linux/async.h>
#include <scsi/ufs/ioctl.h>
#include <linux/nls.h>
#include <linux/of.h>
#include <linux/bitfield.h>
#include <linux/blkdev.h>
#include <linux/suspend.h>
#include "ufshcd.h"
#include "ufs_quirks.h"
#include "unipro.h"
#include "ufs-sysfs.h"
#include "ufs-debugfs.h"
#include "ufs-qcom.h"

#if defined(CONFIG_SEC_USER_RESET_DEBUG)
#include <linux/sec_debug.h>
#endif

#if defined(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#ifdef CONFIG_QCOM_WB
static bool ufshcd_wb_sup(struct ufs_hba *hba);
static int ufshcd_wb_ctrl(struct ufs_hba *hba, bool enable);
static int ufshcd_wb_buf_flush_enable(struct ufs_hba *hba);
static int ufshcd_wb_buf_flush_disable(struct ufs_hba *hba);
static bool ufshcd_wb_is_buf_flush_needed(struct ufs_hba *hba);
static int ufshcd_wb_toggle_flush_during_h8(struct ufs_hba *hba, bool set);
#endif

#ifdef CONFIG_DEBUG_FS

static int ufshcd_tag_req_type(struct request *rq)
{
	int rq_type = TS_WRITE;

	if (!rq)
		rq_type = TS_NOT_SUPPORTED;
	else if (rq->cmd_flags & REQ_PREFLUSH)
		rq_type = TS_FLUSH;
	else if (rq_data_dir(rq) == READ)
		rq_type = (rq->cmd_flags & REQ_URGENT) ?
			TS_URGENT_READ : TS_READ;
	else if (rq->cmd_flags & REQ_URGENT)
		rq_type = TS_URGENT_WRITE;

	return rq_type;
}

static void ufshcd_update_error_stats(struct ufs_hba *hba, int type)
{
	ufsdbg_set_err_state(hba);
	if (type < UFS_ERR_MAX)
		hba->ufs_stats.err_stats[type]++;
}

static void ufshcd_update_tag_stats(struct ufs_hba *hba, int tag)
{
	struct request *rq =
		hba->lrb[tag].cmd ? hba->lrb[tag].cmd->request : NULL;
	u64 **tag_stats = hba->ufs_stats.tag_stats;
	int rq_type;

	if (!hba->ufs_stats.enabled)
		return;

	tag_stats[tag][TS_TAG]++;
	if (!rq)
		return;

	WARN_ON(hba->ufs_stats.q_depth > hba->nutrs);
	rq_type = ufshcd_tag_req_type(rq);
	if (!(rq_type < 0 || rq_type > TS_NUM_STATS))
		tag_stats[hba->ufs_stats.q_depth++][rq_type]++;
}

static void ufshcd_update_tag_stats_completion(struct ufs_hba *hba,
		struct scsi_cmnd *cmd)
{
	struct request *rq = cmd ? cmd->request : NULL;

	if (rq)
		hba->ufs_stats.q_depth--;
}

static void update_req_stats(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	int rq_type;
	struct request *rq = lrbp->cmd ? lrbp->cmd->request : NULL;
	s64 delta = ktime_us_delta(lrbp->compl_time_stamp,
		lrbp->issue_time_stamp);

	/* update general request statistics */
	if (hba->ufs_stats.req_stats[TS_TAG].count == 0)
		hba->ufs_stats.req_stats[TS_TAG].min = delta;
	hba->ufs_stats.req_stats[TS_TAG].count++;
	hba->ufs_stats.req_stats[TS_TAG].sum += delta;
	if (delta > hba->ufs_stats.req_stats[TS_TAG].max)
		hba->ufs_stats.req_stats[TS_TAG].max = delta;
	if (delta < hba->ufs_stats.req_stats[TS_TAG].min)
		hba->ufs_stats.req_stats[TS_TAG].min = delta;

	rq_type = ufshcd_tag_req_type(rq);
	if (rq_type == TS_NOT_SUPPORTED)
		return;

	/* update request type specific statistics */
	if (hba->ufs_stats.req_stats[rq_type].count == 0)
		hba->ufs_stats.req_stats[rq_type].min = delta;
	hba->ufs_stats.req_stats[rq_type].count++;
	hba->ufs_stats.req_stats[rq_type].sum += delta;
	if (delta > hba->ufs_stats.req_stats[rq_type].max)
		hba->ufs_stats.req_stats[rq_type].max = delta;
	if (delta < hba->ufs_stats.req_stats[rq_type].min)
			hba->ufs_stats.req_stats[rq_type].min = delta;
}

static void
ufshcd_update_query_stats(struct ufs_hba *hba, enum query_opcode opcode, u8 idn)
{
	if (opcode < UPIU_QUERY_OPCODE_MAX && idn < MAX_QUERY_IDN)
		hba->ufs_stats.query_stats_arr[opcode][idn]++;
}

#else
static inline void ufshcd_update_tag_stats(struct ufs_hba *hba, int tag)
{
}

static inline void ufshcd_update_tag_stats_completion(struct ufs_hba *hba,
		struct scsi_cmnd *cmd)
{
}

static inline void ufshcd_update_error_stats(struct ufs_hba *hba, int type)
{
}

static inline
void update_req_stats(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
}

static inline
void ufshcd_update_query_stats(struct ufs_hba *hba,
			       enum query_opcode opcode, u8 idn)
{
}
#endif

static void ufshcd_update_uic_error_cnt(struct ufs_hba *hba, u32 reg, int type)
{
	unsigned long err_bits;
	int ec;

	switch (type) {
	case UFS_UIC_ERROR_PA:
		err_bits = reg & UIC_PHY_ADAPTER_LAYER_ERROR_CODE_MASK;
		for_each_set_bit(ec, &err_bits, UFS_EC_PA_MAX) {
			hba->ufs_stats.pa_err_cnt[ec]++;
			hba->ufs_stats.pa_err_cnt_total++;
		}
		break;
	case UFS_UIC_ERROR_DL:
		err_bits = reg & UIC_DATA_LINK_LAYER_ERROR_CODE_MASK;
		for_each_set_bit(ec, &err_bits, UFS_EC_DL_MAX) {
			hba->ufs_stats.dl_err_cnt[ec]++;
			hba->ufs_stats.dl_err_cnt_total++;
		}
		break;
	case UFS_UIC_ERROR_DME:
		hba->ufs_stats.dme_err_cnt++;
	default:
		break;
	}
}
#include "ufshcd-crypto.h"

#define CREATE_TRACE_POINTS
#include <trace/events/ufs.h>

#define PWR_INFO_MASK	0xF
#define PWR_RX_OFFSET	4

#define UFSHCD_REQ_SENSE_SIZE	18

#define UFSHCD_ENABLE_INTRS	(UTP_TRANSFER_REQ_COMPL |\
				 UTP_TASK_REQ_COMPL |\
				 UFSHCD_ERROR_MASK)
/* UIC command timeout, unit: ms */
#define UIC_CMD_TIMEOUT	500

/* NOP OUT retries waiting for NOP IN response */
#define NOP_OUT_RETRIES    10
/* Timeout after 30 msecs if NOP OUT hangs without response */
#define NOP_OUT_TIMEOUT    30 /* msecs */

/* Device initialization completion timeout, unit: ms */
#define DEV_INIT_COMPL_TIMEOUT  1500

/* Query request retries */
#define QUERY_REQ_RETRIES 2
/* Query request timeout */
#define QUERY_REQ_TIMEOUT 1500 /* 1.5 seconds */

/* Task management command timeout */
#define TM_CMD_TIMEOUT	300 /* msecs */

/* maximum number of retries for a general UIC command  */
#define UFS_UIC_COMMAND_RETRIES 3

/* maximum number of link-startup retries */
#define DME_LINKSTARTUP_RETRIES 3

/* Maximum retries for Hibern8 enter */
#define UIC_HIBERN8_ENTER_RETRIES 3

/* maximum number of reset retries before giving up */
#define MAX_HOST_RESET_RETRIES 5

/* maximum retries for UFS link setup */
#define UFS_LINK_SETUP_RETRIES	5

/* Expose the flag value from utp_upiu_query.value */
#define MASK_QUERY_UPIU_FLAG_LOC 0xFF

/* Interrupt aggregation default timeout, unit: 40us */
#define INT_AGGR_DEF_TO	0x02

/* default value of auto suspend is 3 seconds */
#define UFSHCD_AUTO_SUSPEND_DELAY_MS 3000 /* millisecs */

/* default value of ref clock gating wait time is 100 micro seconds */
#define UFSHCD_REF_CLK_GATING_WAIT_US 100 /* microsecs */

#define UFSHCD_CLK_GATING_DELAY_MS_PWR_SAVE	30
#define UFSHCD_CLK_GATING_DELAY_MS_PERF		50

/* IOCTL opcode for command - ufs set device read only */
#define UFS_IOCTL_BLKROSET      BLKROSET

#ifdef CONFIG_BLK_TURBO_WRITE
#ifdef CONFIG_SCSI_UFS_SUPPORT_TW_MAN_GC
#define UFS_TW_MANUAL_FLUSH_THRESHOLD   5
#endif
#define UFS_TW_DISABLE_THRESHOLD	7

#define SEC_UFS_TW_INFO_DIFF(t, n, o, member) ({		\
		(t)->member = (n)->member - (o)->member;	\
		(o)->member = (n)->member; })

static void SEC_UFS_TW_info_get_diff(struct SEC_UFS_TW_info *target,
		struct SEC_UFS_TW_info *new, struct SEC_UFS_TW_info *old)
{
		SEC_UFS_TW_INFO_DIFF(target, new, old, tw_enable_count);
		SEC_UFS_TW_INFO_DIFF(target, new, old, tw_disable_count);
		SEC_UFS_TW_INFO_DIFF(target, new, old, tw_amount_W_kb);
		SEC_UFS_TW_INFO_DIFF(target, new, old, hibern8_enter_count);
		SEC_UFS_TW_INFO_DIFF(target, new, old, hibern8_enter_count_100ms);
		SEC_UFS_TW_INFO_DIFF(target, new, old, hibern8_amount_ms);
		SEC_UFS_TW_INFO_DIFF(target, new, old, hibern8_amount_ms_100ms);

		target->hibern8_max_ms = new->hibern8_max_ms;

		// initialize min, max h8 time values.
		new->hibern8_max_ms = 0;
		get_monotonic_boottime(&(old->timestamp));	/* update timestamp */
}
#endif	/* CONFIG_BLK_TURBO_WRITE */

#define ufshcd_toggle_vreg(_dev, _vreg, _on)				\
	({                                                              \
		int _ret;                                               \
		if (_on)                                                \
			_ret = ufshcd_enable_vreg(_dev, _vreg);         \
		else                                                    \
			_ret = ufshcd_disable_vreg(_dev, _vreg);        \
		_ret;                                                   \
	})

/* Called by FS */
extern void (*ufs_debug_func)(void *);

static void ufshcd_hex_dump(struct ufs_hba *hba, const char * const str,
			    const void *buf, size_t len)

{
	/*
	 * device name is expected to take up ~20 characters and "str" passed
	 * to this function is expected to be of ~10 character so we would need
	 * ~30 characters string to hold the concatenation of these 2 strings.
	 */
	#define MAX_PREFIX_STR_SIZE 50
	char prefix_str[MAX_PREFIX_STR_SIZE] = {0};

	/* concatenate the device name and "str" */
	snprintf(prefix_str, MAX_PREFIX_STR_SIZE, "%s %s: ",
		 dev_name(hba->dev), str);
	print_hex_dump(KERN_ERR, prefix_str, DUMP_PREFIX_OFFSET,
		       16, 4, buf, len, false);
}

enum {
	UFSHCD_MAX_CHANNEL	= 0,
	UFSHCD_MAX_ID		= 1,
	UFSHCD_CMD_PER_LUN	= 32,
	UFSHCD_CAN_QUEUE	= 32,
};

/* UFSHCD states */
enum {
	UFSHCD_STATE_RESET,
	UFSHCD_STATE_ERROR,
	UFSHCD_STATE_OPERATIONAL,
	UFSHCD_STATE_EH_SCHEDULED,
};

/* UFSHCD error handling flags */
enum {
	UFSHCD_EH_IN_PROGRESS = (1 << 0),
};

/* UFSHCD UIC layer error flags */
enum {
	UFSHCD_UIC_DL_PA_INIT_ERROR = (1 << 0), /* Data link layer error */
	UFSHCD_UIC_DL_NAC_RECEIVED_ERROR = (1 << 1), /* Data link layer error */
	UFSHCD_UIC_DL_TCx_REPLAY_ERROR = (1 << 2), /* Data link layer error */
	UFSHCD_UIC_NL_ERROR = (1 << 3), /* Network layer error */
	UFSHCD_UIC_TL_ERROR = (1 << 4), /* Transport Layer error */
	UFSHCD_UIC_DME_ERROR = (1 << 5), /* DME error */
};

#define DEFAULT_UFSHCD_DBG_PRINT_EN	UFSHCD_DBG_PRINT_ALL

#define ufshcd_set_eh_in_progress(h) \
	((h)->eh_flags |= UFSHCD_EH_IN_PROGRESS)
#define ufshcd_eh_in_progress(h) \
	((h)->eh_flags & UFSHCD_EH_IN_PROGRESS)
#define ufshcd_clear_eh_in_progress(h) \
	((h)->eh_flags &= ~UFSHCD_EH_IN_PROGRESS)

#define ufshcd_set_ufs_dev_active(h) \
	((h)->curr_dev_pwr_mode = UFS_ACTIVE_PWR_MODE)
#define ufshcd_set_ufs_dev_sleep(h) \
	((h)->curr_dev_pwr_mode = UFS_SLEEP_PWR_MODE)
#define ufshcd_set_ufs_dev_poweroff(h) \
	((h)->curr_dev_pwr_mode = UFS_POWERDOWN_PWR_MODE)
#define ufshcd_is_ufs_dev_active(h) \
	((h)->curr_dev_pwr_mode == UFS_ACTIVE_PWR_MODE)
#define ufshcd_is_ufs_dev_sleep(h) \
	((h)->curr_dev_pwr_mode == UFS_SLEEP_PWR_MODE)
#define ufshcd_is_ufs_dev_poweroff(h) \
	((h)->curr_dev_pwr_mode == UFS_POWERDOWN_PWR_MODE)

struct ufs_pm_lvl_states ufs_pm_lvl_states[] = {
	{UFS_ACTIVE_PWR_MODE, UIC_LINK_ACTIVE_STATE},
	{UFS_ACTIVE_PWR_MODE, UIC_LINK_HIBERN8_STATE},
	{UFS_SLEEP_PWR_MODE, UIC_LINK_ACTIVE_STATE},
	{UFS_SLEEP_PWR_MODE, UIC_LINK_HIBERN8_STATE},
	{UFS_POWERDOWN_PWR_MODE, UIC_LINK_HIBERN8_STATE},
	{UFS_POWERDOWN_PWR_MODE, UIC_LINK_OFF_STATE},
};

#if defined(CONFIG_UFS_DATA_LOG)
#if defined(CONFIG_UFS_DATA_LOG_MAGIC_CODE)
#define	UFS_DATA_BUF_SIZE	8
#endif
#define UFS_CMDQ_DEPTH_MAX	32
#define	UFS_DATA_LOG_MAX	1024

static int queuing_req[UFS_CMDQ_DEPTH_MAX];

struct ufs_data_log_summary {
	u64 start_time;
	u64	end_time;
	sector_t	sector;
	int	segments_cnt;
	int done;
#if defined(CONFIG_UFS_DATA_LOG_MAGIC_CODE)
	u64 *virt_addr;
	char	datbuf[UFS_DATA_BUF_SIZE];
#endif

};

static struct ufs_data_log_summary ufs_data_log[UFS_DATA_LOG_MAX] __cacheline_aligned;
#endif

static inline enum ufs_dev_pwr_mode
ufs_get_pm_lvl_to_dev_pwr_mode(enum ufs_pm_level lvl)
{
	return ufs_pm_lvl_states[lvl].dev_state;
}

static inline enum uic_link_state
ufs_get_pm_lvl_to_link_pwr_state(enum ufs_pm_level lvl)
{
	return ufs_pm_lvl_states[lvl].link_state;
}

#ifdef CONFIG_QCOM_WB
static inline void ufshcd_wb_toggle_flush(struct ufs_hba *hba)
{
	/*
	 * Query dAvailableWriteBoosterBufferSize attribute and enable
	 * the Write BoosterBuffer Flush if only 30% Write Booster
	 * Buffer is available.
	 * In reduction case, flush only if 10% is available
	 */
	if (ufshcd_wb_is_buf_flush_needed(hba))
		ufshcd_wb_buf_flush_enable(hba);
	else
		ufshcd_wb_buf_flush_disable(hba);
}

static inline void ufshcd_wb_config(struct ufs_hba *hba)
{
	int ret;

	if (!ufshcd_wb_sup(hba))
		return;

	ret = ufshcd_wb_ctrl(hba, true);
	if (ret)
		dev_err(hba->dev, "%s: Enable WB failed: %d\n", __func__, ret);
	else
		dev_info(hba->dev, "%s: Write Booster Configured\n", __func__);
	ret = ufshcd_wb_toggle_flush_during_h8(hba, true);
	if (ret)
		dev_err(hba->dev, "%s: En WB flush during H8: failed: %d\n",
			__func__, ret);
}
#endif

static inline enum ufs_pm_level
ufs_get_desired_pm_lvl_for_dev_link_state(enum ufs_dev_pwr_mode dev_state,
					enum uic_link_state link_state)
{
	enum ufs_pm_level lvl;

	for (lvl = UFS_PM_LVL_0; lvl < UFS_PM_LVL_MAX; lvl++) {
		if ((ufs_pm_lvl_states[lvl].dev_state == dev_state) &&
			(ufs_pm_lvl_states[lvl].link_state == link_state))
			return lvl;
	}

	/* if no match found, return the level 0 */
	return UFS_PM_LVL_0;
}

static inline bool ufshcd_is_valid_pm_lvl(int lvl)
{
	if (lvl >= 0 && lvl < ARRAY_SIZE(ufs_pm_lvl_states))
		return true;
	else
		return false;
}

static struct ufs_dev_fix ufs_fixups[] = {
	/* UFS cards deviations table */
	UFS_FIX(UFS_VENDOR_MICRON, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_NO_FASTAUTO),
	UFS_FIX(UFS_VENDOR_MICRON, UFS_ANY_MODEL,
		UFS_DEVICE_NO_FASTAUTO),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_WDC, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_SKHYNIX, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL, UFS_DEVICE_NO_VCCQ),
	UFS_FIX(UFS_VENDOR_TOSHIBA, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM),
	UFS_FIX(UFS_VENDOR_TOSHIBA, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_NO_LINK_OFF),
	UFS_FIX(UFS_VENDOR_MICRON, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_NO_LINK_OFF),
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGLF2G9C8KBADG",
		UFS_DEVICE_QUIRK_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGLF2G9D8KBADG",
		UFS_DEVICE_QUIRK_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_TOSHIBA, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_SUPPORT_QUERY_FATAL_MODE),
	UFS_FIX(UFS_VENDOR_SKHYNIX, UFS_ANY_MODEL, UFS_DEVICE_NO_VCCQ),
	UFS_FIX(UFS_VENDOR_SKHYNIX, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_SAVECONFIGTIME),
	UFS_FIX(UFS_VENDOR_SKHYNIX, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_WAIT_AFTER_REF_CLK_UNGATE),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "hB8aL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "hC8aL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "hD8aL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "hC8aM1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "h08aM1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "hC8GL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "hC8HL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_SAMSUNG, "KLUDG4UHDB-B2D1",
		UFS_DEVICE_QUIRK_PA_HIBER8TIME),
	UFS_FIX(UFS_VENDOR_SAMSUNG, "KLUEG8UHDB-C2D1",
		UFS_DEVICE_QUIRK_PA_HIBER8TIME),
	UFS_FIX(UFS_VENDOR_SAMSUNG, "KLUFG8RHDA-B2D1",
		UFS_DEVICE_QUIRK_PA_HIBER8TIME),
	UFS_FIX(UFS_VENDOR_SAMSUNG, "KLUGGARHDA-B0D1",
		UFS_DEVICE_QUIRK_PA_HIBER8TIME),
	END_FIX
};

static irqreturn_t ufshcd_intr(int irq, void *__hba);
static irqreturn_t ufshcd_tmc_handler(struct ufs_hba *hba);
static void ufshcd_async_scan(void *data, async_cookie_t cookie);
static int ufshcd_reset_and_restore(struct ufs_hba *hba);
static int ufshcd_eh_host_reset_handler(struct scsi_cmnd *cmd);
static int ufshcd_clear_tm_cmd(struct ufs_hba *hba, int tag);
static void ufshcd_hba_exit(struct ufs_hba *hba);
static int ufshcd_probe_hba(struct ufs_hba *hba);
static int ufshcd_enable_clocks(struct ufs_hba *hba);
static int ufshcd_disable_clocks(struct ufs_hba *hba,
				 bool is_gating_context);
static int ufshcd_disable_clocks_keep_link_active(struct ufs_hba *hba,
					      bool is_gating_context);
#if defined(CONFIG_UFSFEATURE)
void ufshcd_hold_all(struct ufs_hba *hba);
#else
static void ufshcd_hold_all(struct ufs_hba *hba);
#endif
#if defined(CONFIG_UFSFEATURE)
void ufshcd_release_all(struct ufs_hba *hba);
#else
static void ufshcd_release_all(struct ufs_hba *hba);
#endif
static int ufshcd_set_vccq_rail_unused(struct ufs_hba *hba, bool unused);
static inline void ufshcd_add_delay_before_dme_cmd(struct ufs_hba *hba);
static inline void ufshcd_save_tstamp_of_last_dme_cmd(struct ufs_hba *hba);
static int ufshcd_host_reset_and_restore(struct ufs_hba *hba);
static void ufshcd_resume_clkscaling(struct ufs_hba *hba);
static void ufshcd_suspend_clkscaling(struct ufs_hba *hba);
static void __ufshcd_suspend_clkscaling(struct ufs_hba *hba);
static void ufshcd_hba_vreg_set_lpm(struct ufs_hba *hba);
static void ufshcd_hba_vreg_set_hpm(struct ufs_hba *hba);
static int ufshcd_devfreq_target(struct device *dev,
				unsigned long *freq, u32 flags);
static int ufshcd_devfreq_get_dev_status(struct device *dev,
		struct devfreq_dev_status *stat);
static void __ufshcd_shutdown_clkscaling(struct ufs_hba *hba);
static int ufshcd_set_dev_pwr_mode(struct ufs_hba *hba,
				enum ufs_dev_pwr_mode pwr_mode);
static int ufshcd_config_vreg(struct device *dev,
				struct ufs_vreg *vreg, bool on);
static int ufshcd_enable_vreg(struct device *dev, struct ufs_vreg *vreg);
static int ufshcd_disable_vreg(struct device *dev, struct ufs_vreg *vreg);

#if defined(SEC_UFS_ERROR_COUNT)
#include <scsi/scsi_proto.h>

#define MAX_U8_VALUE	0xff

static void SEC_ufs_operation_check(struct ufs_hba *hba, u32 command)
{
	struct SEC_UFS_counting *err_info = &(hba->SEC_err_info);
	struct SEC_UFS_op_count *op_cnt = &(err_info->op_count);

	switch (command) {
	case SEC_UFS_HW_RESET:
		op_cnt->HW_RESET_count++;
		dev_err(hba->dev, "UFS HW_RESET %d\n", op_cnt->HW_RESET_count);
#if defined(CONFIG_SEC_ABC)
		if ((op_cnt->HW_RESET_count % 10) == 0)
			sec_abc_send_event("MODULE=storage@ERROR=ufs_hwreset_err");
#endif
		break;
	case UIC_CMD_DME_LINK_STARTUP:
		op_cnt->link_startup_count++;
		break;
	case UIC_CMD_DME_HIBER_ENTER:
		op_cnt->Hibern8_enter_count++;
		break;
	case UIC_CMD_DME_HIBER_EXIT:
		op_cnt->Hibern8_exit_count++;
		break;
	default:
		break;
	}
	op_cnt->op_err++;
}

static void SEC_ufs_uic_error_check(struct ufs_hba *hba, bool cmd_count, bool fatal_error)
{
	struct SEC_UFS_counting *err_info = &(hba->SEC_err_info);

	if (cmd_count) {
		struct uic_command *uic_cmd = hba->active_uic_cmd;	// uic CMD error count logging
		struct SEC_UFS_UIC_cmd_count *uic_cmd_cnt = &(err_info->UIC_cmd_count);
		struct SEC_UFS_UIC_err_count *uic_err_cnt = &(err_info->UIC_err_count);
		u32 uic_error = hba->uic_error;

		if (!uic_cmd)	/* No UIC CMD error */
			goto passed_uic_cmd_err;

		switch (uic_cmd->command & COMMAND_OPCODE_MASK) {
		case UIC_CMD_DME_GET:
			if (uic_cmd_cnt->DME_GET_err < MAX_U8_VALUE)
				uic_cmd_cnt->DME_GET_err++;
			uic_cmd_cnt->UIC_cmd_err++;
			break;
		case UIC_CMD_DME_SET:
			if (uic_cmd_cnt->DME_SET_err < MAX_U8_VALUE)
				uic_cmd_cnt->DME_SET_err++;
			uic_cmd_cnt->UIC_cmd_err++;
			break;
		case UIC_CMD_DME_PEER_GET:
			if (uic_cmd_cnt->DME_PEER_GET_err < MAX_U8_VALUE)
				uic_cmd_cnt->DME_PEER_GET_err++;
			uic_cmd_cnt->UIC_cmd_err++;
			break;
		case UIC_CMD_DME_PEER_SET:
			if (uic_cmd_cnt->DME_PEER_SET_err < MAX_U8_VALUE)
				uic_cmd_cnt->DME_PEER_SET_err++;
			uic_cmd_cnt->UIC_cmd_err++;
			break;
		case UIC_CMD_DME_POWERON:
			if (uic_cmd_cnt->DME_POWERON_err < MAX_U8_VALUE)
				uic_cmd_cnt->DME_POWERON_err++;
			uic_cmd_cnt->UIC_cmd_err++;
			break;
		case UIC_CMD_DME_POWEROFF:
			if (uic_cmd_cnt->DME_POWEROFF_err < MAX_U8_VALUE)
				uic_cmd_cnt->DME_POWEROFF_err++;
			uic_cmd_cnt->UIC_cmd_err++;
			break;
		case UIC_CMD_DME_ENABLE:
			if (uic_cmd_cnt->DME_ENABLE_err < MAX_U8_VALUE)
				uic_cmd_cnt->DME_ENABLE_err++;
			uic_cmd_cnt->UIC_cmd_err++;
			break;
		case UIC_CMD_DME_RESET:
			if (uic_cmd_cnt->DME_RESET_err < MAX_U8_VALUE)
				uic_cmd_cnt->DME_RESET_err++;
			uic_cmd_cnt->UIC_cmd_err++;
			break;
		case UIC_CMD_DME_END_PT_RST:
			if (uic_cmd_cnt->DME_END_PT_RST_err < MAX_U8_VALUE)
				uic_cmd_cnt->DME_END_PT_RST_err++;
			uic_cmd_cnt->UIC_cmd_err++;
			break;
		case UIC_CMD_DME_LINK_STARTUP:
			if (uic_cmd_cnt->DME_LINK_STARTUP_err < MAX_U8_VALUE)
				uic_cmd_cnt->DME_LINK_STARTUP_err++;
			uic_cmd_cnt->UIC_cmd_err++;
			break;
		case UIC_CMD_DME_HIBER_ENTER:
			if (uic_cmd_cnt->DME_HIBER_ENTER_err < MAX_U8_VALUE)
				uic_cmd_cnt->DME_HIBER_ENTER_err++;
			uic_cmd_cnt->UIC_cmd_err++;
			break;
		case UIC_CMD_DME_HIBER_EXIT:
			if (uic_cmd_cnt->DME_HIBER_EXIT_err < MAX_U8_VALUE)
				uic_cmd_cnt->DME_HIBER_EXIT_err++;
			uic_cmd_cnt->UIC_cmd_err++;
			break;
		case UIC_CMD_DME_TEST_MODE:
			if (uic_cmd_cnt->DME_TEST_MODE_err < MAX_U8_VALUE)
				uic_cmd_cnt->DME_TEST_MODE_err++;
			uic_cmd_cnt->UIC_cmd_err++;
			break;
		default:
			break;
		}
 passed_uic_cmd_err:
		// hba->uic_error; // uic error info
		if (hba->full_init_linereset) {
			if (uic_err_cnt->PA_ERR_cnt < MAX_U8_VALUE)
				uic_err_cnt->PA_ERR_cnt++;
			uic_err_cnt->UIC_err++;
		}
		if (uic_error & UFSHCD_UIC_DL_PA_INIT_ERROR) {
			if (uic_err_cnt->DL_PA_INIT_ERROR_cnt < MAX_U8_VALUE)
				uic_err_cnt->DL_PA_INIT_ERROR_cnt++;
			uic_err_cnt->UIC_err++;
		}
		if (uic_error & UFSHCD_UIC_DL_NAC_RECEIVED_ERROR) {
			if (uic_err_cnt->DL_NAC_RECEIVED_ERROR_cnt < MAX_U8_VALUE)
				uic_err_cnt->DL_NAC_RECEIVED_ERROR_cnt++;
			uic_err_cnt->UIC_err++;
		}
		if (uic_error & UFSHCD_UIC_DL_TCx_REPLAY_ERROR) {
			if (uic_err_cnt->DL_TC_REPLAY_ERROR_cnt < MAX_U8_VALUE)
				uic_err_cnt->DL_TC_REPLAY_ERROR_cnt++;
			uic_err_cnt->UIC_err++;
		}
		if (uic_error & UFSHCD_UIC_NL_ERROR) {
			if (uic_err_cnt->NL_ERROR_cnt < MAX_U8_VALUE)
				uic_err_cnt->NL_ERROR_cnt++;
			uic_err_cnt->UIC_err++;
		}
		if (uic_error & UFSHCD_UIC_TL_ERROR) {
			if (uic_err_cnt->TL_ERROR_cnt < MAX_U8_VALUE)
				uic_err_cnt->TL_ERROR_cnt++;
			uic_err_cnt->UIC_err++;
		}
		if (uic_error & UFSHCD_UIC_DME_ERROR) {
			if (uic_err_cnt->DME_ERROR_cnt < MAX_U8_VALUE)
				uic_err_cnt->DME_ERROR_cnt++;
			uic_err_cnt->UIC_err++;
		}

	} else if (fatal_error) {
		struct SEC_UFS_Fatal_err_count *fatal_err_cnt = &(err_info->Fatal_err_count);
		// hba->error;  // each bit check and count
		if (hba->errors & DEVICE_FATAL_ERROR) {
			if (fatal_err_cnt->DFE < MAX_U8_VALUE)
				fatal_err_cnt->DFE++;
		}
		if (hba->errors & CONTROLLER_FATAL_ERROR) {
			if (fatal_err_cnt->CFE < MAX_U8_VALUE)
				fatal_err_cnt->CFE++;
		}
		if (hba->errors & SYSTEM_BUS_FATAL_ERROR) {
			if (fatal_err_cnt->SBFE < MAX_U8_VALUE)
				fatal_err_cnt->SBFE++;
		}
		if (hba->errors & CRYPTO_ENGINE_FATAL_ERROR) {
			if (fatal_err_cnt->CEFE < MAX_U8_VALUE)
				fatal_err_cnt->CEFE++;
		}
		if (hba->errors & UIC_LINK_LOST) {
			if (fatal_err_cnt->LLE < MAX_U8_VALUE)
				fatal_err_cnt->LLE++;
		}
		if (hba->errors & INT_FATAL_ERRORS)
			fatal_err_cnt->Fatal_err++;
	}
}

static void SEC_ufs_utp_error_check(struct ufs_hba *hba, struct scsi_cmnd *cmd, bool utmr_request, u8 tm_cmd)
{
	struct SEC_UFS_counting *err_info = &(hba->SEC_err_info);
	struct SEC_UFS_UTP_count *utp_err = &(err_info->UTP_count);

	if (utmr_request) {
		if (tm_cmd == UFS_QUERY_TASK) {
			if (utp_err->UTMR_query_task_count < MAX_U8_VALUE)
				utp_err->UTMR_query_task_count++;
			utp_err->UTP_err++;
		} else if (tm_cmd == UFS_ABORT_TASK) {
			if (utp_err->UTMR_abort_task_count < MAX_U8_VALUE)
				utp_err->UTMR_abort_task_count++;
#if defined(CONFIG_SEC_ABC)
			if ((utp_err->UTMR_abort_task_count == 1) &&
					(hba->dev_info.w_manufacturer_id == UFS_VENDOR_TOSHIBA)) {
				sec_abc_send_event("MODULE=storage@ERROR=ufs_hwreset_err");
			}
#endif
			utp_err->UTP_err++;
		}
	} else {
		// cmd logging
		int opcode = cmd->cmnd[0];

		if (opcode == WRITE_10) {
			if (utp_err->UTR_write_err < MAX_U8_VALUE)
				utp_err->UTR_write_err++;
			utp_err->UTP_err++;
		} else if (opcode == READ_10) {
			if (utp_err->UTR_read_err < MAX_U8_VALUE)
				utp_err->UTR_read_err++;
			utp_err->UTP_err++;
		} else if (opcode == SYNCHRONIZE_CACHE) {
			if (utp_err->UTR_sync_cache_err < MAX_U8_VALUE)
				utp_err->UTR_sync_cache_err++;
			utp_err->UTP_err++;
		} else if (opcode == UNMAP) {
			if (utp_err->UTR_unmap_err < MAX_U8_VALUE)
				utp_err->UTR_unmap_err++;
			utp_err->UTP_err++;
		} else {
			if (utp_err->UTR_etc_err < MAX_U8_VALUE)
				utp_err->UTR_etc_err++;
			utp_err->UTP_err++;
		}
	}
}

static void SEC_ufs_query_error_check(struct ufs_hba *hba, enum dev_cmd_type cmd_type)
{
	struct SEC_UFS_counting *err_info = &(hba->SEC_err_info);
	struct SEC_UFS_QUERY_count *query_cnt = &(err_info->query_count);
	struct ufs_query_req *request = &hba->dev_cmd.query.request;
	enum query_opcode opcode = request->upiu_req.opcode;

	if (cmd_type == DEV_CMD_TYPE_NOP) {
		if (query_cnt->NOP_err < MAX_U8_VALUE)
			query_cnt->NOP_err++;
		query_cnt->Query_err++;
	} else if (opcode > 0) {
		switch (opcode) {
		case UPIU_QUERY_OPCODE_READ_DESC:
			if (query_cnt->R_Desc_err < MAX_U8_VALUE)
				query_cnt->R_Desc_err++;
			break;
		case UPIU_QUERY_OPCODE_WRITE_DESC:
			if (query_cnt->W_Desc_err < MAX_U8_VALUE)
				query_cnt->W_Desc_err++;
			break;
		case UPIU_QUERY_OPCODE_READ_ATTR:
			if (query_cnt->R_Attr_err < MAX_U8_VALUE)
				query_cnt->R_Attr_err++;
			break;
		case UPIU_QUERY_OPCODE_WRITE_ATTR:
			if (query_cnt->W_Attr_err < MAX_U8_VALUE)
				query_cnt->W_Attr_err++;
			break;
		case UPIU_QUERY_OPCODE_READ_FLAG:
			if (query_cnt->R_Flag_err < MAX_U8_VALUE)
				query_cnt->R_Flag_err++;
			break;
		case UPIU_QUERY_OPCODE_SET_FLAG:
			if (query_cnt->Set_Flag_err < MAX_U8_VALUE)
				query_cnt->Set_Flag_err++;
			break;
		case UPIU_QUERY_OPCODE_CLEAR_FLAG:
			if (query_cnt->Clear_Flag_err < MAX_U8_VALUE)
				query_cnt->Clear_Flag_err++;
			break;
		case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
			if (query_cnt->Toggle_Flag_err < MAX_U8_VALUE)
				query_cnt->Toggle_Flag_err++;
			break;
		default:
			break;
		}
		if (opcode && opcode < UPIU_QUERY_OPCODE_MAX)
			query_cnt->Query_err++;
	}

}
#endif	// SEC_UFS_ERROR_COUNT

#ifdef CONFIG_BLK_TURBO_WRITE
static void SEC_ufs_update_tw_info(struct ufs_hba *hba, int write_transfer_len)
{
	struct SEC_UFS_TW_info *tw_info = &(hba->SEC_tw_info);
	enum ufs_tw_state tw_state = hba->ufs_tw_state;

	if (tw_info->tw_info_disable)
		return;

	if (write_transfer_len) {
		/*
		 * write_transfer_len : Byte
		 * tw_info->tw_amount_W_kb : KB
		 */
		tw_info->tw_amount_W_kb += (unsigned long)(write_transfer_len >> 10);
		if (unlikely((s64)tw_info->tw_amount_W_kb < 0))
			goto disable_tw_info;
		return;
	}

	switch (tw_state) {
	case UFS_TW_OFF_STATE:
		tw_info->tw_enable_ms += jiffies_to_msecs(jiffies - tw_info->tw_state_ts);
		tw_info->tw_state_ts = jiffies;
		tw_info->tw_disable_count++;
		if (unlikely(((s64)tw_info->tw_enable_ms < 0) || ((s64)tw_info->tw_disable_count < 0)))
			goto disable_tw_info;
		break;
	case UFS_TW_ON_STATE:
		tw_info->tw_disable_ms += jiffies_to_msecs(jiffies - tw_info->tw_state_ts);
		tw_info->tw_state_ts = jiffies;
		tw_info->tw_enable_count++;
		if (unlikely(((s64)tw_info->tw_disable_ms < 0) || ((s64)tw_info->tw_enable_count < 0)))
			goto disable_tw_info;
		break;
	case UFS_TW_ERR_STATE:
		tw_info->tw_setflag_error_count++;
		if (unlikely((s64)tw_info->tw_setflag_error_count < 0))
			goto disable_tw_info;
		break;
	default:
		break;
	}
	return;

disable_tw_info:
	/* disable tw_info updating when MSB is set */
	tw_info->tw_info_disable = true;
	return;
}

static void SEC_ufs_update_h8_info(struct ufs_hba *hba, bool hibern8_enter)
{
	struct SEC_UFS_TW_info *tw_info = &(hba->SEC_tw_info);
	u64 calc_h8_time_ms = 0;

	/*
	 * I/O end in UFS_ACTIVE_PWR_MODE :
	 *   -> 10ms : Auto-H8
	 *   -> 30~50ms : call hibern8_enter in clk gating
	 *   -> 3s : runtime PM suspend
	 *      -> ufshcd_suspend : run H8 exit ->  SSU : UFS_SLEEP_PWR_MODE -> H8 enter
	 * runtime PM resume : H8 exit -> SSU : UFS_ACTIVE_PWR_MODE -> I/O
	 *
	 * system PM suspend :
	 *   if runtime suspended -> runtime PM resume
	 *   ufshcd_suspend : run H8 exit(if h8 state)
	 *      ->  SSU : UFS_SLEEP_PWR_MODE -> H8 enter
	 */
	if (!ufshcd_is_ufs_dev_active(hba))
		return;

	if (unlikely(((s64)tw_info->hibern8_enter_count < 0) || ((s64)tw_info->hibern8_amount_ms < 0)))
		return;

	if (hibern8_enter) {
		tw_info->hibern8_enter_ts = ktime_get();
	} else {
		calc_h8_time_ms = (u64)ktime_ms_delta(ktime_get(), tw_info->hibern8_enter_ts);
#if defined(CONFIG_SCSI_UFS_QCOM)
		if (ufshcd_is_auto_hibern8_supported(hba)) {
			calc_h8_time_ms += (u64)(hba->clk_gating.delay_ms - hba->hibern8_on_idle.delay_ms);
		}
#endif

		if (calc_h8_time_ms > 99) {
			tw_info->hibern8_enter_count_100ms++;
			tw_info->hibern8_amount_ms_100ms += calc_h8_time_ms;
		}

		if (tw_info->hibern8_max_ms < calc_h8_time_ms)
			tw_info->hibern8_max_ms = calc_h8_time_ms;

		tw_info->hibern8_amount_ms += calc_h8_time_ms;
		tw_info->hibern8_enter_count++;
	}
}
#endif

#if defined(CONFIG_UFSFEATURE) && defined(CONFIG_UFSHPB)
static void ufshcd_add_hpb_info_sysfs_node(struct ufs_hba *hba);

static void SEC_ufs_update_hpb_info(struct ufs_hba *hba, int read_transfer_len)
{
	struct SEC_UFS_HPB_info *hpb_info = &(hba->SEC_hpb_info);

	if (hpb_info->hpb_info_disable)
		return;

	/*
	 * read_transfer_len : Byte
	 * hpb_info->hpb_amount_R_kb : KB
	 */
	hpb_info->hpb_amount_R_kb += (unsigned long)(read_transfer_len >> 10);
	if (unlikely((s64)hpb_info->hpb_amount_R_kb < 0))
		goto disable_hpb_info;
	return;

disable_hpb_info:
	hpb_info->hpb_info_disable = true;
	return;
}

#define SEC_UFS_HPB_ERR_check(hpb_info, member) ({		\
		(hpb_info)->member++;				\
		if ((hpb_info)->member == UINT_MAX) 		\
			(hpb_info)->hpb_err_count_disable = true; })

static void SEC_ufs_hpb_error_check(struct ufs_hba *hba, struct scsi_cmnd *cmd)
{
	struct SEC_UFS_HPB_info *hpb_info = &(hba->SEC_hpb_info);

	u8 cmd_opcode = cmd->cmnd[0];
	u8 cmd_id = cmd->cmnd[1];

	if (hpb_info->hpb_err_count_disable)
		return;

	switch (cmd_opcode) {
	case READ_16:
		SEC_UFS_HPB_ERR_check(hpb_info, hpb_read_err_count);
		break;
	case UFSHPB_READ_BUFFER:
		if (cmd_id == UFSHPB_RB_ID_READ)
			SEC_UFS_HPB_ERR_check(hpb_info, hpb_RB_ID_READ_err_count);
		else if (cmd_id == UFSHPB_RB_ID_SET_RT)
			SEC_UFS_HPB_ERR_check(hpb_info, hpb_RB_ID_SET_RT_err_count);
		break;
	case UFSHPB_WRITE_BUFFER:
		if (cmd_id == UFSHPB_WB_ID_PREFETCH)
			SEC_UFS_HPB_ERR_check(hpb_info, hpb_WB_ID_PREFETCH_err_count);
		else if (cmd_id == UFSHPB_WB_ID_UNSET_RT)
			SEC_UFS_HPB_ERR_check(hpb_info, hpb_WB_ID_UNSET_RT_err_count);
		else if (cmd_id == UFSHPB_WB_ID_UNSET_RT_ALL)
			SEC_UFS_HPB_ERR_check(hpb_info, hpb_WB_ID_UNSET_RT_ALL_err_count);
		break;
	default:
		break;
	}

	return;
}

void SEC_ufs_hpb_rb_count(struct ufs_hba *hba, struct ufshpb_region *rgn)
{
       if (rgn->rgn_state == HPBREGION_PINNED ||
	   rgn->rgn_state == HPBREGION_RT_PINNED) {
	       atomic64_inc(&(hba->SEC_hpb_info.hpb_pinned_rb_cnt));
       } else if (rgn->rgn_state == HPBREGION_ACTIVE) {
	       atomic64_inc(&(hba->SEC_hpb_info.hpb_active_rb_cnt));
       }

       return;
}
#endif

#if IS_ENABLED(CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND)
static struct devfreq_simple_ondemand_data ufshcd_ondemand_data = {
	.upthreshold = 70,
	.downdifferential = 65,
	.simple_scaling = 1,
};

static void *gov_data = &ufshcd_ondemand_data;
#else
static void *gov_data;
#endif

static inline bool ufshcd_valid_tag(struct ufs_hba *hba, int tag)
{
	return tag >= 0 && tag < hba->nutrs;
}

static inline void ufshcd_enable_irq(struct ufs_hba *hba)
{
	if (!hba->is_irq_enabled) {
		enable_irq(hba->irq);
		hba->is_irq_enabled = true;
	}
}

static inline void ufshcd_disable_irq(struct ufs_hba *hba)
{
	if (hba->is_irq_enabled) {
		disable_irq(hba->irq);
		hba->is_irq_enabled = false;
	}
}

void ufshcd_scsi_unblock_requests(struct ufs_hba *hba)
{
	if (atomic_dec_and_test(&hba->scsi_block_reqs_cnt))
		scsi_unblock_requests(hba->host);
}
EXPORT_SYMBOL(ufshcd_scsi_unblock_requests);

void ufshcd_scsi_block_requests(struct ufs_hba *hba)
{
	if (atomic_inc_return(&hba->scsi_block_reqs_cnt) == 1)
		scsi_block_requests(hba->host);
}
EXPORT_SYMBOL(ufshcd_scsi_block_requests);

static int ufshcd_device_reset_ctrl(struct ufs_hba *hba, bool ctrl)
{
	int ret = 0;

	if (!hba->pctrl)
		return 0;

	/* Assert reset if ctrl == true */
	if (ctrl)
		ret = pinctrl_select_state(hba->pctrl,
			pinctrl_lookup_state(hba->pctrl, "dev-reset-assert"));
	else
		ret = pinctrl_select_state(hba->pctrl,
			pinctrl_lookup_state(hba->pctrl, "dev-reset-deassert"));

	if (ret < 0)
		dev_err(hba->dev, "%s: %s failed with err %d\n",
			__func__, ctrl ? "Assert" : "Deassert", ret);
	else
		dev_err(hba->dev, "UFS %s is done\n", ctrl ? "Assert" : "Deassert");

	return ret;
}

static inline int ufshcd_assert_device_reset(struct ufs_hba *hba)
{
	return ufshcd_device_reset_ctrl(hba, true);
}

static inline int ufshcd_deassert_device_reset(struct ufs_hba *hba)
{
	return ufshcd_device_reset_ctrl(hba, false);
}

static int ufshcd_reset_device(struct ufs_hba *hba)
{
	int ret;

#if defined(SEC_UFS_ERROR_COUNT)
	SEC_ufs_operation_check(hba, SEC_UFS_HW_RESET);
#endif
	/* reset the connected UFS device */
	ret = ufshcd_assert_device_reset(hba);
	if (ret)
		goto out;
	/*
	 * The reset signal is active low.
	 * The UFS device shall detect more than or equal to 1us of positive
	 * or negative RST_n pulse width.
	 * To be on safe side, keep the reset low for atleast 10us.
	 */
	usleep_range(10, 15);

	ret = ufshcd_deassert_device_reset(hba);
	if (ret)
		goto out;
	/* same as assert, wait for atleast 10us after deassert */
	usleep_range(10, 15);
out:
	return ret;
}

/* replace non-printable or non-ASCII characters with spaces */
static inline void ufshcd_remove_non_printable(char *val)
{
	if (!val || !*val)
		return;

	if (*val < 0x20 || *val > 0x7e)
		*val = ' ';
}

static void ufshcd_add_cmd_upiu_trace(struct ufs_hba *hba, unsigned int tag,
		const char *str)
{
	struct utp_upiu_req *rq = hba->lrb[tag].ucd_req_ptr;

	trace_ufshcd_upiu(dev_name(hba->dev), str, &rq->header, &rq->sc.cdb);
}

static void ufshcd_add_query_upiu_trace(struct ufs_hba *hba, unsigned int tag,
		const char *str)
{
	struct utp_upiu_req *rq = hba->lrb[tag].ucd_req_ptr;

	trace_ufshcd_upiu(dev_name(hba->dev), str, &rq->header, &rq->qr);
}

static void ufshcd_add_tm_upiu_trace(struct ufs_hba *hba, unsigned int tag,
		const char *str)
{
	struct utp_task_req_desc *descp;
	struct utp_upiu_task_req *task_req;
	int off = (int)tag - hba->nutrs;

	descp = &hba->utmrdl_base_addr[off];
	task_req = (struct utp_upiu_task_req *)descp->task_req_upiu;
	trace_ufshcd_upiu(dev_name(hba->dev), str, &task_req->header,
			&task_req->input_param1);
}

#define UFSHCD_MAX_CMD_LOGGING	200

#ifdef CONFIG_TRACEPOINTS
static inline void ufshcd_add_command_trace(struct ufs_hba *hba,
			struct ufshcd_cmd_log_entry *entry)
{
	if (trace_ufshcd_command_enabled()) {
		u32 intr = ufshcd_readl(hba, REG_INTERRUPT_STATUS);

		trace_ufshcd_command(dev_name(hba->dev), entry->str, entry->tag,
				     entry->doorbell, entry->transfer_len, intr,
				     entry->lba, entry->cmd_id);
	}
}
#else
static inline void ufshcd_add_command_trace(struct ufs_hba *hba,
			struct ufshcd_cmd_log_entry *entry)
{
}
#endif

#ifdef CONFIG_SCSI_UFSHCD_CMD_LOGGING
static void ufshcd_cmd_log_init(struct ufs_hba *hba)
{
	/* Allocate log entries */
	if (!hba->cmd_log.entries) {
		hba->cmd_log.entries = kcalloc(UFSHCD_MAX_CMD_LOGGING,
			sizeof(struct ufshcd_cmd_log_entry), GFP_KERNEL);
		if (!hba->cmd_log.entries)
			return;
		dev_dbg(hba->dev, "%s: cmd_log.entries initialized\n",
				__func__);
	}
}

static void __ufshcd_cmd_log(struct ufs_hba *hba, char *str, char *cmd_type,
			     unsigned int tag, u8 cmd_id, u8 idn, u8 lun,
			     sector_t lba, int transfer_len)
{
	struct ufshcd_cmd_log_entry *entry;

	if (!hba->cmd_log.entries)
		return;

	entry = &hba->cmd_log.entries[hba->cmd_log.pos];
	entry->lun = lun;
	entry->str = str;
	entry->cmd_type = cmd_type;
	entry->cmd_id = cmd_id;
	entry->lba = lba;
	entry->transfer_len = transfer_len;
	entry->idn = idn;
	entry->doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	entry->tag = tag;
	entry->tstamp = ktime_get();
	entry->outstanding_reqs = hba->outstanding_reqs;
	entry->seq_num = hba->cmd_log.seq_num;
	hba->cmd_log.seq_num++;
	hba->cmd_log.pos =
			(hba->cmd_log.pos + 1) % UFSHCD_MAX_CMD_LOGGING;

	ufshcd_add_command_trace(hba, entry);
}

static void ufshcd_cmd_log(struct ufs_hba *hba, char *str, char *cmd_type,
	unsigned int tag, u8 cmd_id, u8 idn)
{
	__ufshcd_cmd_log(hba, str, cmd_type, tag, cmd_id, idn, 0, 0, 0);
}

static void ufshcd_dme_cmd_log(struct ufs_hba *hba, char *str, u8 cmd_id)
{
	ufshcd_cmd_log(hba, str, "dme", 0, cmd_id, 0);
}

static void ufshcd_custom_cmd_log(struct ufs_hba *hba, char *str)
{
	ufshcd_cmd_log(hba, str, "custom", 0, 0, 0);
}

static void ufshcd_print_cmd_log(struct ufs_hba *hba)
{
	int i;
	int pos;
	struct ufshcd_cmd_log_entry *p;

	if (!hba->cmd_log.entries)
		return;

	pos = hba->cmd_log.pos;
	for (i = 0; i < UFSHCD_MAX_CMD_LOGGING; i++) {
		p = &hba->cmd_log.entries[pos];
		pos = (pos + 1) % UFSHCD_MAX_CMD_LOGGING;

		if (ktime_to_us(p->tstamp)) {
			pr_err("%s: %s: seq_no=%u lun=0x%x cmd_id=0x%02x lba=0x%llx txfer_len=%d tag=%u, doorbell=0x%x outstanding=0x%x idn=%d time=%lld us\n",
				p->cmd_type, p->str, p->seq_num,
				p->lun, p->cmd_id, (unsigned long long)p->lba,
				p->transfer_len, p->tag, p->doorbell,
				p->outstanding_reqs, p->idn,
				ktime_to_us(p->tstamp));
				usleep_range(1000, 1100);
		}
	}
}
#else
static void ufshcd_cmd_log_init(struct ufs_hba *hba)
{
}

static void __ufshcd_cmd_log(struct ufs_hba *hba, char *str, char *cmd_type,
			     unsigned int tag, u8 cmd_id, u8 idn, u8 lun,
			     sector_t lba, int transfer_len)
{
	struct ufshcd_cmd_log_entry entry;

	entry.str = str;
	entry.lba = lba;
	entry.cmd_id = cmd_id;
	entry.transfer_len = transfer_len;
	entry.doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	entry.tag = tag;

	ufshcd_add_command_trace(hba, &entry);
}

static void ufshcd_dme_cmd_log(struct ufs_hba *hba, char *str, u8 cmd_id)
{
}

static void ufshcd_custom_cmd_log(struct ufs_hba *hba, char *str)
{
}

static void ufshcd_print_cmd_log(struct ufs_hba *hba)
{
}
#endif

#ifdef CONFIG_TRACEPOINTS
static inline void ufshcd_cond_add_cmd_trace(struct ufs_hba *hba,
					unsigned int tag, const char *str)
{
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	char *cmd_type = NULL;
	u8 opcode = 0;
	u8 cmd_id = 0, idn = 0;
	sector_t lba = 0;
	int transfer_len = 0;

	if (lrbp->cmd) { /* data phase exists */
		/* trace UPIU also */
		ufshcd_add_cmd_upiu_trace(hba, tag, str);
		opcode = (u8)(*lrbp->cmd->cmnd);
		if ((opcode == READ_10) || (opcode == WRITE_10)) {
			/*
			 * Currently we only fully trace read(10) and write(10)
			 * commands
			 */
			if (lrbp->cmd->request && lrbp->cmd->request->bio)
				lba =
				lrbp->cmd->request->bio->bi_iter.bi_sector;
			transfer_len = be32_to_cpu(
				lrbp->ucd_req_ptr->sc.exp_data_transfer_len);
		}
	}

	if (lrbp->cmd && ((lrbp->command_type == UTP_CMD_TYPE_SCSI) ||
			  (lrbp->command_type == UTP_CMD_TYPE_UFS_STORAGE))) {
		cmd_type = "scsi";
		cmd_id = (u8)(*lrbp->cmd->cmnd);
	} else if (lrbp->command_type == UTP_CMD_TYPE_DEV_MANAGE) {
		if (hba->dev_cmd.type == DEV_CMD_TYPE_NOP) {
			cmd_type = "nop";
			cmd_id = 0;
		} else if (hba->dev_cmd.type == DEV_CMD_TYPE_QUERY) {
			cmd_type = "query";
			cmd_id = hba->dev_cmd.query.request.upiu_req.opcode;
			idn = hba->dev_cmd.query.request.upiu_req.idn;
		}
	}

	__ufshcd_cmd_log(hba, (char *) str, cmd_type, tag, cmd_id, idn,
			 lrbp->lun, lba, transfer_len);
}
#else
static inline void ufshcd_cond_add_cmd_trace(struct ufs_hba *hba,
					unsigned int tag, const char *str)
{
}
#endif

static void ufshcd_print_clk_freqs(struct ufs_hba *hba)
{
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;

	if (!(hba->ufshcd_dbg_print & UFSHCD_DBG_PRINT_CLK_FREQ_EN))
		return;

	if (list_empty(head))
		return;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk) && clki->min_freq &&
				clki->max_freq)
			dev_err(hba->dev, "clk: %s, rate: %u\n",
					clki->name, clki->curr_freq);
	}
}

static void ufshcd_print_uic_err_hist(struct ufs_hba *hba,
		struct ufs_uic_err_reg_hist *err_hist, char *err_name)
{
	int i;

	if (!(hba->ufshcd_dbg_print & UFSHCD_DBG_PRINT_UIC_ERR_HIST_EN))
		return;

	for (i = 0; i < UIC_ERR_REG_HIST_LENGTH; i++) {
		int p = (i + err_hist->pos - 1) % UIC_ERR_REG_HIST_LENGTH;

		if (err_hist->reg[p] == 0)
			continue;
		dev_err(hba->dev, "%s[%d] = 0x%x at %lld us\n", err_name, i,
			err_hist->reg[p], ktime_to_us(err_hist->tstamp[p]));
	}
}

static inline void __ufshcd_print_host_regs(struct ufs_hba *hba, bool no_sleep)
{
	if (!(hba->ufshcd_dbg_print & UFSHCD_DBG_PRINT_HOST_REGS_EN))
		return;

	ufshcd_hex_dump(hba, "host regs", hba->mmio_base,
			UFSHCI_REG_SPACE_SIZE);
	dev_err(hba->dev, "hba->ufs_version = 0x%x, hba->capabilities = 0x%x\n",
		hba->ufs_version, hba->capabilities);
	dev_err(hba->dev,
		"hba->outstanding_reqs = 0x%x, hba->outstanding_tasks = 0x%x\n",
		(u32)hba->outstanding_reqs, (u32)hba->outstanding_tasks);
	dev_err(hba->dev,
		"last_hibern8_exit_tstamp at %lld us, hibern8_exit_cnt = %d\n",
		ktime_to_us(hba->ufs_stats.last_hibern8_exit_tstamp),
		hba->ufs_stats.hibern8_exit_cnt);

	ufshcd_print_uic_err_hist(hba, &hba->ufs_stats.pa_err, "pa_err");
	ufshcd_print_uic_err_hist(hba, &hba->ufs_stats.dl_err, "dl_err");
	ufshcd_print_uic_err_hist(hba, &hba->ufs_stats.nl_err, "nl_err");
	ufshcd_print_uic_err_hist(hba, &hba->ufs_stats.tl_err, "tl_err");
	ufshcd_print_uic_err_hist(hba, &hba->ufs_stats.dme_err, "dme_err");

	ufshcd_print_clk_freqs(hba);

	ufshcd_vops_dbg_register_dump(hba, no_sleep);
}

static void ufshcd_print_host_regs(struct ufs_hba *hba)
{
	__ufshcd_print_host_regs(hba, false);

	ufshcd_crypto_debug(hba);
}

static
void ufshcd_print_trs(struct ufs_hba *hba, unsigned long bitmap, bool pr_prdt)
{
	struct ufshcd_lrb *lrbp;
	int prdt_length;
	int tag;

	if (!(hba->ufshcd_dbg_print & UFSHCD_DBG_PRINT_TRS_EN))
		return;

	for_each_set_bit(tag, &bitmap, hba->nutrs) {
		lrbp = &hba->lrb[tag];

		dev_err(hba->dev, "UPIU[%d] - issue time %lld us\n",
				tag, ktime_to_us(lrbp->issue_time_stamp));
		dev_err(hba->dev, "UPIU[%d] - complete time %lld us\n",
				tag, ktime_to_us(lrbp->compl_time_stamp));
		dev_err(hba->dev,
			"UPIU[%d] - Transfer Request Descriptor phys@0x%llx\n",
			tag, (u64)lrbp->utrd_dma_addr);

		ufshcd_hex_dump(hba, "UPIU TRD", lrbp->utr_descriptor_ptr,
				sizeof(struct utp_transfer_req_desc));
		dev_err(hba->dev, "UPIU[%d] - Request UPIU phys@0x%llx\n", tag,
			(u64)lrbp->ucd_req_dma_addr);
		ufshcd_hex_dump(hba, "UPIU REQ", lrbp->ucd_req_ptr,
				sizeof(struct utp_upiu_req));
		dev_err(hba->dev, "UPIU[%d] - Response UPIU phys@0x%llx\n", tag,
			(u64)lrbp->ucd_rsp_dma_addr);
		ufshcd_hex_dump(hba, "UPIU RSP", lrbp->ucd_rsp_ptr,
				sizeof(struct utp_upiu_rsp));

		prdt_length =
			le16_to_cpu(lrbp->utr_descriptor_ptr->prd_table_length);
		if (hba->quirks & UFSHCD_QUIRK_PRDT_BYTE_GRAN)
			prdt_length /= hba->sg_entry_size;

		dev_err(hba->dev,
			"UPIU[%d] - PRDT - %d entries  phys@0x%llx\n",
			tag, prdt_length,
			(u64)lrbp->ucd_prdt_dma_addr);

		if (pr_prdt)
			ufshcd_hex_dump(hba, "UPIU PRDT: ", lrbp->ucd_prdt_ptr,
					hba->sg_entry_size * prdt_length);
	}
}

static void ufshcd_print_tmrs(struct ufs_hba *hba, unsigned long bitmap)
{
	struct utp_task_req_desc *tmrdp;
	int tag;

	if (!(hba->ufshcd_dbg_print & UFSHCD_DBG_PRINT_TMRS_EN))
		return;

	for_each_set_bit(tag, &bitmap, hba->nutmrs) {
		tmrdp = &hba->utmrdl_base_addr[tag];
		dev_err(hba->dev, "TM[%d] - Task Management Header\n", tag);
		ufshcd_hex_dump(hba, "TM TRD", &tmrdp->header,
				sizeof(struct request_desc_header));
		dev_err(hba->dev, "TM[%d] - Task Management Request UPIU\n",
				tag);
		ufshcd_hex_dump(hba, "TM REQ", tmrdp->task_req_upiu,
				sizeof(struct utp_upiu_req));
		dev_err(hba->dev, "TM[%d] - Task Management Response UPIU\n",
				tag);
		ufshcd_hex_dump(hba, "TM RSP", tmrdp->task_rsp_upiu,
				sizeof(struct utp_task_req_desc));
	}
}

static void ufshcd_print_fsm_state(struct ufs_hba *hba)
{
	int err = 0, tx_fsm_val = 0, rx_fsm_val = 0;

	err = ufshcd_dme_get(hba,
			UIC_ARG_MIB_SEL(MPHY_TX_FSM_STATE,
			UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
			&tx_fsm_val);
	dev_err(hba->dev, "%s: TX_FSM_STATE = %u, err = %d\n", __func__,
			tx_fsm_val, err);
	err = ufshcd_dme_get(hba,
			UIC_ARG_MIB_SEL(MPHY_RX_FSM_STATE,
			UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)),
			&rx_fsm_val);
	dev_err(hba->dev, "%s: RX_FSM_STATE = %u, err = %d\n", __func__,
			rx_fsm_val, err);
}

static void ufshcd_print_host_state(struct ufs_hba *hba)
{
	if (!(hba->ufshcd_dbg_print & UFSHCD_DBG_PRINT_HOST_STATE_EN))
		return;

	dev_err(hba->dev, "UFS Host state=%d\n", hba->ufshcd_state);
	dev_err(hba->dev, "lrb in use=0x%lx, outstanding reqs=0x%lx tasks=0x%lx\n",
		hba->lrb_in_use, hba->outstanding_reqs, hba->outstanding_tasks);
	dev_err(hba->dev, "saved_err=0x%x, saved_uic_err=0x%x, saved_ce_err=0x%x\n",
		hba->saved_err, hba->saved_uic_err, hba->saved_ce_err);
	dev_err(hba->dev, "Device power mode=%d, UIC link state=%d\n",
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	dev_err(hba->dev, "PM in progress=%d, sys. suspended=%d\n",
		hba->pm_op_in_progress, hba->is_sys_suspended);
	dev_err(hba->dev, "Auto BKOPS=%d, Host self-block=%d\n",
		hba->auto_bkops_enabled, hba->host->host_self_blocked);
	dev_err(hba->dev, "Clk gate=%d, hibern8 on idle=%d\n",
		hba->clk_gating.state, hba->hibern8_on_idle.state);
	dev_err(hba->dev, "error handling flags=0x%x, req. abort count=%d\n",
		hba->eh_flags, hba->req_abort_count);
	dev_err(hba->dev, "Host capabilities=0x%x, caps=0x%x\n",
		hba->capabilities, hba->caps);
	dev_err(hba->dev, "quirks=0x%x, dev. quirks=0x%x\n", hba->quirks,
		hba->dev_info.quirks);
	dev_err(hba->dev, "pa_err_cnt_total=%d, pa_lane_0_err_cnt=%d, pa_lane_1_err_cnt=%d, pa_line_reset_err_cnt=%d\n",
		hba->ufs_stats.pa_err_cnt_total,
		hba->ufs_stats.pa_err_cnt[UFS_EC_PA_LANE_0],
		hba->ufs_stats.pa_err_cnt[UFS_EC_PA_LANE_1],
		hba->ufs_stats.pa_err_cnt[UFS_EC_PA_LINE_RESET]);
	dev_err(hba->dev, "dl_err_cnt_total=%d, dl_nac_received_err_cnt=%d, dl_tcx_replay_timer_expired_err_cnt=%d\n",
		hba->ufs_stats.dl_err_cnt_total,
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_NAC_RECEIVED],
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_TCx_REPLAY_TIMER_EXPIRED]);
	dev_err(hba->dev, "dl_afcx_request_timer_expired_err_cnt=%d, dl_fcx_protection_timer_expired_err_cnt=%d, dl_crc_err_cnt=%d\n",
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_AFCx_REQUEST_TIMER_EXPIRED],
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_FCx_PROTECT_TIMER_EXPIRED],
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_CRC_ERROR]);
	dev_err(hba->dev, "dll_rx_buffer_overflow_err_cnt=%d, dl_max_frame_length_exceeded_err_cnt=%d, dl_wrong_sequence_number_err_cnt=%d\n",
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_RX_BUFFER_OVERFLOW],
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_MAX_FRAME_LENGTH_EXCEEDED],
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_WRONG_SEQUENCE_NUMBER]);
	dev_err(hba->dev, "dl_afc_frame_syntax_err_cnt=%d, dl_nac_frame_syntax_err_cnt=%d, dl_eof_syntax_err_cnt=%d\n",
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_AFC_FRAME_SYNTAX_ERROR],
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_NAC_FRAME_SYNTAX_ERROR],
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_EOF_SYNTAX_ERROR]);
	dev_err(hba->dev, "dl_frame_syntax_err_cnt=%d, dl_bad_ctrl_symbol_type_err_cnt=%d, dl_pa_init_err_cnt=%d, dl_pa_error_ind_received=%d\n",
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_FRAME_SYNTAX_ERROR],
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_BAD_CTRL_SYMBOL_TYPE],
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_PA_INIT_ERROR],
		hba->ufs_stats.dl_err_cnt[UFS_EC_DL_PA_ERROR_IND_RECEIVED]);
	dev_err(hba->dev, "dme_err_cnt=%d\n", hba->ufs_stats.dme_err_cnt);
}

/**
 * ufshcd_print_pwr_info - print power params as saved in hba
 * power info
 * @hba: per-adapter instance
 */
static void ufshcd_print_pwr_info(struct ufs_hba *hba)
{
	static const char * const names[] = {
		"INVALID MODE",
		"FAST MODE",
		"SLOW_MODE",
		"INVALID MODE",
		"FASTAUTO_MODE",
		"SLOWAUTO_MODE",
		"INVALID MODE",
	};

	if (!(hba->ufshcd_dbg_print & UFSHCD_DBG_PRINT_PWR_EN))
		return;

	dev_err(hba->dev, "%s:[RX, TX]: gear=[%d, %d], lane[%d, %d], pwr[%s, %s], rate = %d\n",
		 __func__,
		 hba->pwr_info.gear_rx, hba->pwr_info.gear_tx,
		 hba->pwr_info.lane_rx, hba->pwr_info.lane_tx,
		 names[hba->pwr_info.pwr_rx],
		 names[hba->pwr_info.pwr_tx],
		 hba->pwr_info.hs_rate);
}

/*
 * ufshcd_wait_for_register - wait for register value to change
 * @hba - per-adapter interface
 * @reg - mmio register offset
 * @mask - mask to apply to read register value
 * @val - wait condition
 * @interval_us - polling interval in microsecs
 * @timeout_ms - timeout in millisecs
 * @can_sleep - perform sleep or just spin
 *
 * Returns -ETIMEDOUT on error, zero on success
 */
int ufshcd_wait_for_register(struct ufs_hba *hba, u32 reg, u32 mask,
				u32 val, unsigned long interval_us,
				unsigned long timeout_ms, bool can_sleep)
{
	int err = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);

	/* ignore bits that we don't intend to wait on */
	val = val & mask;

	while ((ufshcd_readl(hba, reg) & mask) != val) {
		if (can_sleep)
			usleep_range(interval_us, interval_us + 50);
		else
			udelay(interval_us);
		if (time_after(jiffies, timeout)) {
			if ((ufshcd_readl(hba, reg) & mask) != val)
				err = -ETIMEDOUT;
			break;
		}
	}

	return err;
}

/**
 * ufshcd_get_intr_mask - Get the interrupt bit mask
 * @hba: Pointer to adapter instance
 *
 * Returns interrupt bit mask per version
 */
static inline u32 ufshcd_get_intr_mask(struct ufs_hba *hba)
{
	u32 intr_mask = 0;

	switch (hba->ufs_version) {
	case UFSHCI_VERSION_10:
		intr_mask = INTERRUPT_MASK_ALL_VER_10;
		break;
	case UFSHCI_VERSION_11:
	case UFSHCI_VERSION_20:
		intr_mask = INTERRUPT_MASK_ALL_VER_11;
		break;
	case UFSHCI_VERSION_21:
	default:
		intr_mask = INTERRUPT_MASK_ALL_VER_21;
		break;
	}

	if (!ufshcd_is_crypto_supported(hba))
		intr_mask &= ~CRYPTO_ENGINE_FATAL_ERROR;

	return intr_mask;
}

/**
 * ufshcd_get_ufs_version - Get the UFS version supported by the HBA
 * @hba: Pointer to adapter instance
 *
 * Returns UFSHCI version supported by the controller
 */
static inline u32 ufshcd_get_ufs_version(struct ufs_hba *hba)
{
	if (hba->quirks & UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION)
		return ufshcd_vops_get_ufs_hci_version(hba);

	return ufshcd_readl(hba, REG_UFS_VERSION);
}

/**
 * ufshcd_is_device_present - Check if any device connected to
 *			      the host controller
 * @hba: pointer to adapter instance
 *
 * Returns true if device present, false if no device detected
 */
static inline bool ufshcd_is_device_present(struct ufs_hba *hba)
{
	return (ufshcd_readl(hba, REG_CONTROLLER_STATUS) &
						DEVICE_PRESENT) ? true : false;
}

/**
 * ufshcd_get_tr_ocs - Get the UTRD Overall Command Status
 * @lrbp: pointer to local command reference block
 *
 * This function is used to get the OCS field from UTRD
 * Returns the OCS field in the UTRD
 */
static inline int ufshcd_get_tr_ocs(struct ufshcd_lrb *lrbp)
{
	return le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_2) & MASK_OCS;
}

/**
 * ufshcd_get_tmr_ocs - Get the UTMRD Overall Command Status
 * @task_req_descp: pointer to utp_task_req_desc structure
 *
 * This function is used to get the OCS field from UTMRD
 * Returns the OCS field in the UTMRD
 */
static inline int
ufshcd_get_tmr_ocs(struct utp_task_req_desc *task_req_descp)
{
	return le32_to_cpu(task_req_descp->header.dword_2) & MASK_OCS;
}

/**
 * ufshcd_get_tm_free_slot - get a free slot for task management request
 * @hba: per adapter instance
 * @free_slot: pointer to variable with available slot value
 *
 * Get a free tag and lock it until ufshcd_put_tm_slot() is called.
 * Returns 0 if free slot is not available, else return 1 with tag value
 * in @free_slot.
 */
static bool ufshcd_get_tm_free_slot(struct ufs_hba *hba, int *free_slot)
{
	int tag;
	bool ret = false;

	if (!free_slot)
		goto out;

	do {
		tag = find_first_zero_bit(&hba->tm_slots_in_use, hba->nutmrs);
		if (tag >= hba->nutmrs)
			goto out;
	} while (test_and_set_bit_lock(tag, &hba->tm_slots_in_use));

	*free_slot = tag;
	ret = true;
out:
	return ret;
}

static inline void ufshcd_put_tm_slot(struct ufs_hba *hba, int slot)
{
	clear_bit_unlock(slot, &hba->tm_slots_in_use);
}

/**
 * ufshcd_utrl_clear - Clear a bit in UTRLCLR register
 * @hba: per adapter instance
 * @pos: position of the bit to be cleared
 */
static inline void ufshcd_utrl_clear(struct ufs_hba *hba, u32 pos)
{
	if (hba->quirks & UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR)
		ufshcd_writel(hba, (1 << pos), REG_UTP_TRANSFER_REQ_LIST_CLEAR);
	else
		ufshcd_writel(hba, ~(1 << pos),
				REG_UTP_TRANSFER_REQ_LIST_CLEAR);
}

/**
 * ufshcd_utmrl_clear - Clear a bit in UTRMLCLR register
 * @hba: per adapter instance
 * @pos: position of the bit to be cleared
 */
static inline void ufshcd_utmrl_clear(struct ufs_hba *hba, u32 pos)
{
	if (hba->quirks & UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR)
		ufshcd_writel(hba, (1 << pos), REG_UTP_TASK_REQ_LIST_CLEAR);
	else
		ufshcd_writel(hba, ~(1 << pos), REG_UTP_TASK_REQ_LIST_CLEAR);
}

/**
 * ufshcd_outstanding_req_clear - Clear a bit in outstanding request field
 * @hba: per adapter instance
 * @tag: position of the bit to be cleared
 */
static inline void ufshcd_outstanding_req_clear(struct ufs_hba *hba, int tag)
{
	__clear_bit(tag, &hba->outstanding_reqs);
}

/**
 * ufshcd_get_lists_status - Check UCRDY, UTRLRDY and UTMRLRDY
 * @reg: Register value of host controller status
 *
 * Returns integer, 0 on Success and positive value if failed
 */
static inline int ufshcd_get_lists_status(u32 reg)
{
	return !((reg & UFSHCD_STATUS_READY) == UFSHCD_STATUS_READY);
}

/**
 * ufshcd_get_uic_cmd_result - Get the UIC command result
 * @hba: Pointer to adapter instance
 *
 * This function gets the result of UIC command completion
 * Returns 0 on success, non zero value on error
 */
static inline int ufshcd_get_uic_cmd_result(struct ufs_hba *hba)
{
	return ufshcd_readl(hba, REG_UIC_COMMAND_ARG_2) &
	       MASK_UIC_COMMAND_RESULT;
}

/**
 * ufshcd_get_dme_attr_val - Get the value of attribute returned by UIC command
 * @hba: Pointer to adapter instance
 *
 * This function gets UIC command argument3
 * Returns 0 on success, non zero value on error
 */
static inline u32 ufshcd_get_dme_attr_val(struct ufs_hba *hba)
{
	return ufshcd_readl(hba, REG_UIC_COMMAND_ARG_3);
}

/**
 * ufshcd_get_req_rsp - returns the TR response transaction type
 * @ucd_rsp_ptr: pointer to response UPIU
 */
static inline int
ufshcd_get_req_rsp(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return be32_to_cpu(ucd_rsp_ptr->header.dword_0) >> 24;
}

/**
 * ufshcd_get_rsp_upiu_result - Get the result from response UPIU
 * @ucd_rsp_ptr: pointer to response UPIU
 *
 * This function gets the response status and scsi_status from response UPIU
 * Returns the response result code.
 */
static inline int
ufshcd_get_rsp_upiu_result(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return be32_to_cpu(ucd_rsp_ptr->header.dword_1) & MASK_RSP_UPIU_RESULT;
}

/*
 * ufshcd_get_rsp_upiu_data_seg_len - Get the data segment length
 *				from response UPIU
 * @ucd_rsp_ptr: pointer to response UPIU
 *
 * Return the data segment length.
 */
static inline unsigned int
ufshcd_get_rsp_upiu_data_seg_len(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return be32_to_cpu(ucd_rsp_ptr->header.dword_2) &
		MASK_RSP_UPIU_DATA_SEG_LEN;
}

/**
 * ufshcd_is_exception_event - Check if the device raised an exception event
 * @ucd_rsp_ptr: pointer to response UPIU
 *
 * The function checks if the device raised an exception event indicated in
 * the Device Information field of response UPIU.
 *
 * Returns true if exception is raised, false otherwise.
 */
static inline bool ufshcd_is_exception_event(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return be32_to_cpu(ucd_rsp_ptr->header.dword_2) &
			MASK_RSP_EXCEPTION_EVENT ? true : false;
}

/**
 * ufshcd_reset_intr_aggr - Reset interrupt aggregation values.
 * @hba: per adapter instance
 */
static inline void
ufshcd_reset_intr_aggr(struct ufs_hba *hba)
{
	ufshcd_writel(hba, INT_AGGR_ENABLE |
		      INT_AGGR_COUNTER_AND_TIMER_RESET,
		      REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL);
}

/**
 * ufshcd_config_intr_aggr - Configure interrupt aggregation values.
 * @hba: per adapter instance
 * @cnt: Interrupt aggregation counter threshold
 * @tmout: Interrupt aggregation timeout value
 */
static inline void
ufshcd_config_intr_aggr(struct ufs_hba *hba, u8 cnt, u8 tmout)
{
	ufshcd_writel(hba, INT_AGGR_ENABLE | INT_AGGR_PARAM_WRITE |
		      INT_AGGR_COUNTER_THLD_VAL(cnt) |
		      INT_AGGR_TIMEOUT_VAL(tmout),
		      REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL);
}

/**
 * ufshcd_disable_intr_aggr - Disables interrupt aggregation.
 * @hba: per adapter instance
 */
static inline void ufshcd_disable_intr_aggr(struct ufs_hba *hba)
{
	ufshcd_writel(hba, 0, REG_UTP_TRANSFER_REQ_INT_AGG_CONTROL);
}

/**
 * ufshcd_enable_run_stop_reg - Enable run-stop registers,
 *			When run-stop registers are set to 1, it indicates the
 *			host controller that it can process the requests
 * @hba: per adapter instance
 */
static void ufshcd_enable_run_stop_reg(struct ufs_hba *hba)
{
	ufshcd_writel(hba, UTP_TASK_REQ_LIST_RUN_STOP_BIT,
		      REG_UTP_TASK_REQ_LIST_RUN_STOP);
	ufshcd_writel(hba, UTP_TRANSFER_REQ_LIST_RUN_STOP_BIT,
		      REG_UTP_TRANSFER_REQ_LIST_RUN_STOP);
}

/**
 * ufshcd_hba_start - Start controller initialization sequence
 * @hba: per adapter instance
 */
static inline void ufshcd_hba_start(struct ufs_hba *hba)
{
	u32 val = CONTROLLER_ENABLE;

	if (ufshcd_hba_is_crypto_supported(hba)) {
		ufshcd_crypto_enable(hba);
		val |= CRYPTO_GENERAL_ENABLE;
	}

	ufshcd_writel(hba, val, REG_CONTROLLER_ENABLE);
}

/**
 * ufshcd_is_hba_active - Get controller state
 * @hba: per adapter instance
 *
 * Returns false if controller is active, true otherwise
 */
static inline bool ufshcd_is_hba_active(struct ufs_hba *hba)
{
	return (ufshcd_readl(hba, REG_CONTROLLER_ENABLE) & CONTROLLER_ENABLE)
		? false : true;
}

u32 ufshcd_get_local_unipro_ver(struct ufs_hba *hba)
{
	/* HCI version 1.0 and 1.1 supports UniPro 1.41 */
	if ((hba->ufs_version == UFSHCI_VERSION_10) ||
	    (hba->ufs_version == UFSHCI_VERSION_11))
		return UFS_UNIPRO_VER_1_41;
	else
		return UFS_UNIPRO_VER_1_6;
}
EXPORT_SYMBOL(ufshcd_get_local_unipro_ver);

static bool ufshcd_is_unipro_pa_params_tuning_req(struct ufs_hba *hba)
{
	/*
	 * If both host and device support UniPro ver1.6 or later, PA layer
	 * parameters tuning happens during link startup itself.
	 *
	 * We can manually tune PA layer parameters if either host or device
	 * doesn't support UniPro ver 1.6 or later. But to keep manual tuning
	 * logic simple, we will only do manual tuning if local unipro version
	 * doesn't support ver1.6 or later.
	 */
	if (ufshcd_get_local_unipro_ver(hba) < UFS_UNIPRO_VER_1_6)
		return true;
	else
		return false;
}

/**
 * ufshcd_set_clk_freq - set UFS controller clock frequencies
 * @hba: per adapter instance
 * @scale_up: If True, set max possible frequency othewise set low frequency
 *
 * Returns 0 if successful
 * Returns < 0 for any other errors
 */
static int ufshcd_set_clk_freq(struct ufs_hba *hba, bool scale_up)
{
	int ret = 0;
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;

	if (list_empty(head))
		goto out;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk)) {
			if (scale_up && clki->max_freq) {
				if ((clki->curr_freq == clki->max_freq) ||
				   (!strcmp(clki->name, "core_clk_ice_hw_ctl")))
					continue;

				ret = clk_set_rate(clki->clk, clki->max_freq);
				if (ret) {
					dev_err(hba->dev, "%s: %s clk set rate(%dHz) failed, %d\n",
						__func__, clki->name,
						clki->max_freq, ret);
					break;
				}
				trace_ufshcd_clk_scaling(dev_name(hba->dev),
						"scaled up", clki->name,
						clki->curr_freq,
						clki->max_freq);

				clki->curr_freq = clki->max_freq;

			} else if (!scale_up && clki->min_freq) {
				if ((clki->curr_freq == clki->min_freq) ||
				   (!strcmp(clki->name, "core_clk_ice_hw_ctl")))
					continue;

				ret = clk_set_rate(clki->clk, clki->min_freq);
				if (ret) {
					dev_err(hba->dev, "%s: %s clk set rate(%dHz) failed, %d\n",
						__func__, clki->name,
						clki->min_freq, ret);
					break;
				}
				trace_ufshcd_clk_scaling(dev_name(hba->dev),
						"scaled down", clki->name,
						clki->curr_freq,
						clki->min_freq);
				clki->curr_freq = clki->min_freq;
			}
		}
		dev_dbg(hba->dev, "%s: clk: %s, rate: %lu\n", __func__,
				clki->name, clk_get_rate(clki->clk));
	}

out:
	return ret;
}

/**
 * ufshcd_scale_clks - scale up or scale down UFS controller clocks
 * @hba: per adapter instance
 * @scale_up: True if scaling up and false if scaling down
 *
 * Returns 0 if successful
 * Returns < 0 for any other errors
 */
int ufshcd_scale_clks(struct ufs_hba *hba, bool scale_up)
{
	int ret = 0;

	ret = ufshcd_vops_clk_scale_notify(hba, scale_up, PRE_CHANGE);
	if (ret)
		return ret;

	ret = ufshcd_set_clk_freq(hba, scale_up);
	if (ret)
		return ret;

	ret = ufshcd_vops_clk_scale_notify(hba, scale_up, POST_CHANGE);
	if (ret) {
		ufshcd_set_clk_freq(hba, !scale_up);
		return ret;
	}

	return ret;
}

static inline void ufshcd_cancel_gate_work(struct ufs_hba *hba)
{
	hrtimer_cancel(&hba->clk_gating.gate_hrtimer);
	cancel_work_sync(&hba->clk_gating.gate_work);
}

/**
 * ufshcd_is_devfreq_scaling_required - check if scaling is required or not
 * @hba: per adapter instance
 * @scale_up: True if scaling up and false if scaling down
 *
 * Returns true if scaling is required, false otherwise.
 */
static bool ufshcd_is_devfreq_scaling_required(struct ufs_hba *hba,
					       bool scale_up)
{
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;

	if (list_empty(head))
		return false;

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk)) {
			if (scale_up && clki->max_freq) {
				if (clki->curr_freq == clki->max_freq)
					continue;
				return true;
			} else if (!scale_up && clki->min_freq) {
				if (clki->curr_freq == clki->min_freq)
					continue;
				return true;
			}
		}
	}

	return false;
}

int ufshcd_wait_for_doorbell_clr(struct ufs_hba *hba,
					u64 wait_timeout_us)
{
	unsigned long flags;
	int ret = 0;
	u32 tm_doorbell;
	u32 tr_doorbell;
	bool timeout = false, do_last_check = false;
	ktime_t start;

	ufshcd_hold_all(hba);
	spin_lock_irqsave(hba->host->host_lock, flags);
	/*
	 * Wait for all the outstanding tasks/transfer requests.
	 * Verify by checking the doorbell registers are clear.
	 */
	start = ktime_get();
	do {
		if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL) {
			ret = -EBUSY;
			goto out;
		}

		tm_doorbell = ufshcd_readl(hba, REG_UTP_TASK_REQ_DOOR_BELL);
		tr_doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
		if (!tm_doorbell && !tr_doorbell) {
			timeout = false;
			break;
		} else if (do_last_check) {
			break;
		}

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		schedule();
		if (ktime_to_us(ktime_sub(ktime_get(), start)) >
		    wait_timeout_us) {
			timeout = true;
			/*
			 * We might have scheduled out for long time so make
			 * sure to check if doorbells are cleared by this time
			 * or not.
			 */
			do_last_check = true;
		}
		spin_lock_irqsave(hba->host->host_lock, flags);
	} while (tm_doorbell || tr_doorbell);

	if (timeout) {
		dev_err(hba->dev,
			"%s: timedout waiting for doorbell to clear (tm=0x%x, tr=0x%x)\n",
			__func__, tm_doorbell, tr_doorbell);
		ret = -EBUSY;
	}
out:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufshcd_release_all(hba);
	return ret;
}

/**
 * ufshcd_scale_gear - scale up/down UFS gear
 * @hba: per adapter instance
 * @scale_up: True for scaling up gear and false for scaling down
 *
 * Returns 0 for success,
 * Returns -EBUSY if scaling can't happen at this time
 * Returns non-zero for any other errors
 */
static int ufshcd_scale_gear(struct ufs_hba *hba, bool scale_up)
{
	int ret = 0;
	struct ufs_pa_layer_attr new_pwr_info;
	u32 scale_down_gear = ufshcd_vops_get_scale_down_gear(hba);

	WARN_ON(!hba->clk_scaling.saved_pwr_info.is_valid);

	if (scale_up) {
		memcpy(&new_pwr_info, &hba->clk_scaling.saved_pwr_info.info,
		       sizeof(struct ufs_pa_layer_attr));
		/*
		 * Some UFS devices may stop responding after switching from
		 * HS-G1 to HS-G3. Also, it is found that these devices work
		 * fine if we do 2 steps switch: HS-G1 to HS-G2 followed by
		 * HS-G2 to HS-G3. If UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH
		 * quirk is enabled for such devices, this 2 steps gear switch
		 * workaround will be applied.
		 */
		if ((hba->dev_info.quirks &
		     UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH)
		    && (hba->pwr_info.gear_tx == UFS_HS_G1)
		    && (new_pwr_info.gear_tx == UFS_HS_G3)) {
			/* scale up to G2 first */
			new_pwr_info.gear_tx = UFS_HS_G2;
			new_pwr_info.gear_rx = UFS_HS_G2;
			ret = ufshcd_change_power_mode(hba, &new_pwr_info);
			if (ret)
				goto out;

			/* scale up to G3 now */
			new_pwr_info.gear_tx = UFS_HS_G3;
			new_pwr_info.gear_rx = UFS_HS_G3;
			/* now, fall through to set the HS-G3 */
		}
		ret = ufshcd_change_power_mode(hba, &new_pwr_info);
		if (ret)
			goto out;
	} else {
		memcpy(&new_pwr_info, &hba->pwr_info,
		       sizeof(struct ufs_pa_layer_attr));

		if (hba->pwr_info.gear_tx > scale_down_gear
		    || hba->pwr_info.gear_rx > scale_down_gear) {
			/* save the current power mode */
			memcpy(&hba->clk_scaling.saved_pwr_info.info,
				&hba->pwr_info,
				sizeof(struct ufs_pa_layer_attr));

			/* scale down gear */
			new_pwr_info.gear_tx = scale_down_gear;
			new_pwr_info.gear_rx = scale_down_gear;
			if (!(hba->dev_info.quirks & UFS_DEVICE_NO_FASTAUTO)) {
				new_pwr_info.pwr_tx = FASTAUTO_MODE;
				new_pwr_info.pwr_rx = FASTAUTO_MODE;
			}
		}
		ret = ufshcd_change_power_mode(hba, &new_pwr_info);
	}

out:
	if (ret)
		dev_err(hba->dev, "%s: failed err %d, old gear: (tx %d rx %d), new gear: (tx %d rx %d), scale_up = %d\n",
			__func__, ret,
			hba->pwr_info.gear_tx, hba->pwr_info.gear_rx,
			new_pwr_info.gear_tx, new_pwr_info.gear_rx,
			scale_up);

	return ret;
}

static int ufshcd_clock_scaling_prepare(struct ufs_hba *hba)
{
	#define DOORBELL_CLR_TOUT_US		(1000 * 1000) /* 1 sec */
	int ret = 0;
	/*
	 * make sure that there are no outstanding requests when
	 * clock scaling is in progress
	 */
	down_write(&hba->lock);
	ufshcd_scsi_block_requests(hba);
	if (ufshcd_wait_for_doorbell_clr(hba, DOORBELL_CLR_TOUT_US)) {
		ret = -EBUSY;
		up_write(&hba->lock);
		ufshcd_scsi_unblock_requests(hba);
	}

	return ret;
}

static void ufshcd_clock_scaling_unprepare(struct ufs_hba *hba)
{
	up_write(&hba->lock);
	ufshcd_scsi_unblock_requests(hba);
}

/**
 * ufshcd_devfreq_scale - scale up/down UFS clocks and gear
 * @hba: per adapter instance
 * @scale_up: True for scaling up and false for scalin down
 *
 * Returns 0 for success,
 * Returns -EBUSY if scaling can't happen at this time
 * Returns non-zero for any other errors
 */
static int ufshcd_devfreq_scale(struct ufs_hba *hba, bool scale_up)
{
	int ret = 0;

	/* let's not get into low power until clock scaling is completed */
	hba->ufs_stats.clk_hold.ctx = CLK_SCALE_WORK;
	ufshcd_hold_all(hba);

	ret = ufshcd_clock_scaling_prepare(hba);
	if (ret)
		goto out;

	ufshcd_custom_cmd_log(hba, "waited-for-DB-clear");

	/* scale down the gear before scaling down clocks */
	if (!scale_up) {
		ret = ufshcd_scale_gear(hba, false);
		if (ret)
			goto clk_scaling_unprepare;
		ufshcd_custom_cmd_log(hba, "Gear-scaled-down");
	}

	/* Enter hibern8 before scaling clocks */
	ret = ufshcd_uic_hibern8_enter(hba);
	if (ret)
		/* link will be bad state so no need to scale_up_gear */
		goto clk_scaling_unprepare;
	ufshcd_custom_cmd_log(hba, "Hibern8-entered");

	ret = ufshcd_scale_clks(hba, scale_up);
	if (ret) {
		dev_err(hba->dev, "%s: scaling %s clks failed %d\n", __func__,
			scale_up ? "up" : "down", ret);
		goto scale_up_gear;
	}
	ufshcd_custom_cmd_log(hba, "Clk-freq-switched");

	/* Exit hibern8 after scaling clocks */
	ret = ufshcd_uic_hibern8_exit(hba);
	if (ret)
		/* link will be bad state so no need to scale_up_gear */
		goto clk_scaling_unprepare;
	ufshcd_custom_cmd_log(hba, "Hibern8-Exited");

	/* scale up the gear after scaling up clocks */
	if (scale_up) {
		ret = ufshcd_scale_gear(hba, true);
		if (ret) {
			ufshcd_scale_clks(hba, false);
			goto clk_scaling_unprepare;
		}
		ufshcd_custom_cmd_log(hba, "Gear-scaled-up");
	}

	if (!ret) {
		hba->clk_scaling.is_scaled_up = scale_up;
		if (scale_up)
			hba->clk_gating.delay_ms =
				hba->clk_gating.delay_ms_perf;
		else
			hba->clk_gating.delay_ms =
				hba->clk_gating.delay_ms_pwr_save;
	}

#ifdef CONFIG_QCOM_WB
	/* Enable Write Booster if we have scaled up else disable it */
	up_write(&hba->lock);
	ufshcd_wb_ctrl(hba, scale_up);
	down_write(&hba->lock);
#endif
	goto clk_scaling_unprepare;

scale_up_gear:
	if (ufshcd_uic_hibern8_exit(hba))
		goto clk_scaling_unprepare;
	if (!scale_up)
		ufshcd_scale_gear(hba, true);
clk_scaling_unprepare:
	ufshcd_clock_scaling_unprepare(hba);
out:
	hba->ufs_stats.clk_rel.ctx = CLK_SCALE_WORK;
	ufshcd_release_all(hba);
	return ret;
}

static void ufshcd_clk_scaling_suspend_work(struct work_struct *work)
{
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
					   clk_scaling.suspend_work);
	unsigned long irq_flags;

	spin_lock_irqsave(hba->host->host_lock, irq_flags);
	if (hba->clk_scaling.active_reqs || hba->clk_scaling.is_suspended) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		return;
	}
	hba->clk_scaling.is_suspended = true;
	spin_unlock_irqrestore(hba->host->host_lock, irq_flags);

	__ufshcd_suspend_clkscaling(hba);
}

static void ufshcd_clk_scaling_resume_work(struct work_struct *work)
{
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
					   clk_scaling.resume_work);
	unsigned long irq_flags;

	spin_lock_irqsave(hba->host->host_lock, irq_flags);
	if (!hba->clk_scaling.is_suspended) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		return;
	}
	hba->clk_scaling.is_suspended = false;
	spin_unlock_irqrestore(hba->host->host_lock, irq_flags);

	devfreq_resume_device(hba->devfreq);
}

static int ufshcd_devfreq_target(struct device *dev,
				unsigned long *freq, u32 flags)
{
	int ret = 0;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	ktime_t start;
	bool scale_up, sched_clk_scaling_suspend_work = false;
	struct list_head *clk_list = &hba->clk_list_head;
	struct ufs_clk_info *clki;
	unsigned long irq_flags;
	int scaling_required = 0;

	if (!ufshcd_is_clkscaling_supported(hba))
		return -EINVAL;

	spin_lock_irqsave(hba->host->host_lock, irq_flags);
	if (ufshcd_eh_in_progress(hba)) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		return 0;
	}

	if (!hba->clk_scaling.active_reqs)
		sched_clk_scaling_suspend_work = true;

	if (list_empty(clk_list)) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		goto out;
	}

	clki = list_first_entry(&hba->clk_list_head, struct ufs_clk_info, list);
	scale_up = (*freq == clki->max_freq) ? true : false;
	if (!ufshcd_is_devfreq_scaling_required(hba, scale_up)) {
		spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
		ret = 0;
		goto out; /* no state change required */
	}
	spin_unlock_irqrestore(hba->host->host_lock, irq_flags);

	start = ktime_get();
	if (!(ufshcd_is_ufs_dev_active(hba) || ufshcd_is_link_active(hba))) {
		pm_runtime_get_sync(hba->dev);
		scaling_required = 1;
	}
	ret = ufshcd_devfreq_scale(hba, scale_up);
	if (scaling_required)
		pm_runtime_put(hba->dev);
	trace_ufshcd_profile_clk_scaling(dev_name(hba->dev),
		(scale_up ? "up" : "down"),
		ktime_to_us(ktime_sub(ktime_get(), start)), ret);

out:
	if (sched_clk_scaling_suspend_work)
		queue_work(hba->clk_scaling.workq,
			   &hba->clk_scaling.suspend_work);

	return ret;
}


static int ufshcd_devfreq_get_dev_status(struct device *dev,
		struct devfreq_dev_status *stat)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_clk_scaling *scaling = &hba->clk_scaling;
	unsigned long flags;

	if (!ufshcd_is_clkscaling_supported(hba))
		return -EINVAL;

	memset(stat, 0, sizeof(*stat));

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (!scaling->window_start_t)
		goto start_window;

	if (scaling->is_busy_started)
		scaling->tot_busy_t += ktime_to_us(ktime_sub(ktime_get(),
					scaling->busy_start_t));

	stat->total_time = jiffies_to_usecs((long)jiffies -
				(long)scaling->window_start_t);
	stat->busy_time = scaling->tot_busy_t;
start_window:
	scaling->window_start_t = jiffies;
	scaling->tot_busy_t = 0;

	if (hba->outstanding_reqs) {
		scaling->busy_start_t = ktime_get();
		scaling->is_busy_started = true;
	} else {
		scaling->busy_start_t = 0;
		scaling->is_busy_started = false;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return 0;
}

static int ufshcd_devfreq_init(struct ufs_hba *hba)
{
	struct list_head *clk_list = &hba->clk_list_head;
	struct ufs_clk_info *clki;
	struct devfreq *devfreq;
	struct ufs_clk_scaling *scaling = &hba->clk_scaling;
	int ret;

	/* Skip devfreq if we don't have any clocks in the list */
	if (list_empty(clk_list))
		return 0;

	clki = list_first_entry(clk_list, struct ufs_clk_info, list);
	dev_pm_opp_add(hba->dev, clki->min_freq, 0);
	dev_pm_opp_add(hba->dev, clki->max_freq, 0);

	scaling->profile.polling_ms = 60;
	scaling->profile.target = ufshcd_devfreq_target;
	scaling->profile.get_dev_status = ufshcd_devfreq_get_dev_status;

	devfreq = devfreq_add_device(hba->dev,
			&scaling->profile,
			DEVFREQ_GOV_SIMPLE_ONDEMAND,
			gov_data);
	if (IS_ERR(devfreq)) {
		ret = PTR_ERR(devfreq);
		dev_err(hba->dev, "Unable to register with devfreq %d\n", ret);

		dev_pm_opp_remove(hba->dev, clki->min_freq);
		dev_pm_opp_remove(hba->dev, clki->max_freq);
		return ret;
	}

	hba->devfreq = devfreq;

	return 0;
}

static void ufshcd_devfreq_remove(struct ufs_hba *hba)
{
	struct list_head *clk_list = &hba->clk_list_head;
	struct ufs_clk_info *clki;

	if (!hba->devfreq)
		return;

	devfreq_remove_device(hba->devfreq);
	hba->devfreq = NULL;

	clki = list_first_entry(clk_list, struct ufs_clk_info, list);
	dev_pm_opp_remove(hba->dev, clki->min_freq);
	dev_pm_opp_remove(hba->dev, clki->max_freq);
}

static void __ufshcd_suspend_clkscaling(struct ufs_hba *hba)
{
	unsigned long flags;

	devfreq_suspend_device(hba->devfreq);
	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->clk_scaling.window_start_t = 0;
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

static void ufshcd_suspend_clkscaling(struct ufs_hba *hba)
{
	unsigned long flags;
	bool suspend = false;

	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (!hba->clk_scaling.is_suspended) {
		suspend = true;
		hba->clk_scaling.is_suspended = true;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (suspend)
		__ufshcd_suspend_clkscaling(hba);
}

static void ufshcd_resume_clkscaling(struct ufs_hba *hba)
{
	unsigned long flags;
	bool resume = false;

	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->clk_scaling.is_suspended) {
		resume = true;
		hba->clk_scaling.is_suspended = false;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (resume)
		devfreq_resume_device(hba->devfreq);
}

static ssize_t ufshcd_clkscale_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", hba->clk_scaling.is_allowed);
}

static ssize_t ufshcd_clkscale_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 value;
	int err;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	value = !!value;
	if (value == hba->clk_scaling.is_allowed)
		goto out;

	pm_runtime_get_sync(hba->dev);
	ufshcd_hold(hba, false);

	cancel_work_sync(&hba->clk_scaling.suspend_work);
	cancel_work_sync(&hba->clk_scaling.resume_work);

	hba->clk_scaling.is_allowed = value;

	if (value) {
		ufshcd_resume_clkscaling(hba);
	} else {
		ufshcd_suspend_clkscaling(hba);
		err = ufshcd_devfreq_scale(hba, true);
		if (err)
			dev_err(hba->dev, "%s: failed to scale clocks up %d\n",
					__func__, err);
	}

	ufshcd_release(hba, false);
	pm_runtime_put_sync(hba->dev);
out:
	return count;
}

static void ufshcd_clkscaling_init_sysfs(struct ufs_hba *hba)
{
	hba->clk_scaling.enable_attr.show = ufshcd_clkscale_enable_show;
	hba->clk_scaling.enable_attr.store = ufshcd_clkscale_enable_store;
	sysfs_attr_init(&hba->clk_scaling.enable_attr.attr);
	hba->clk_scaling.enable_attr.attr.name = "clkscale_enable";
	hba->clk_scaling.enable_attr.attr.mode = 0644;
	if (device_create_file(hba->dev, &hba->clk_scaling.enable_attr))
		dev_err(hba->dev, "Failed to create sysfs for clkscale_enable\n");
}

static void ufshcd_ungate_work(struct work_struct *work)
{
	int ret;
	unsigned long flags;
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
			clk_gating.ungate_work);

	ufshcd_cancel_gate_work(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->clk_gating.state == CLKS_ON) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		goto unblock_reqs;
	}

	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufshcd_hba_vreg_set_hpm(hba);
	ufshcd_enable_clocks(hba);

	/* Exit from hibern8 */
	if (ufshcd_can_hibern8_during_gating(hba)) {
		/* Prevent gating in this path */
		hba->clk_gating.is_suspended = true;
		if (ufshcd_is_link_hibern8(hba)) {
			ret = ufshcd_uic_hibern8_exit(hba);
			if (ret)
				dev_err(hba->dev, "%s: hibern8 exit failed %d\n",
					__func__, ret);
			else
				ufshcd_set_link_active(hba);
		}
		hba->clk_gating.is_suspended = false;
	}
unblock_reqs:
	ufshcd_scsi_unblock_requests(hba);
}

/**
 * ufshcd_hold - Enable clocks that were gated earlier due to ufshcd_release.
 * Also, exit from hibern8 mode and set the link as active.
 * @hba: per adapter instance
 * @async: This indicates whether caller should ungate clocks asynchronously.
 */
int ufshcd_hold(struct ufs_hba *hba, bool async)
{
	int rc = 0;
	bool flush_result;
	unsigned long flags;

	if (!ufshcd_is_clkgating_allowed(hba))
		goto out;
	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->clk_gating.active_reqs++;

	if (ufshcd_eh_in_progress(hba)) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		return 0;
	}

start:
	switch (hba->clk_gating.state) {
	case CLKS_ON:
		/*
		 * Wait for the ungate work to complete if in progress.
		 * Though the clocks may be in ON state, the link could
		 * still be in hibner8 state if hibern8 is allowed
		 * during clock gating.
		 * Make sure we exit hibern8 state also in addition to
		 * clocks being ON.
		 */
		if (ufshcd_can_hibern8_during_gating(hba) &&
		    ufshcd_is_link_hibern8(hba)) {
			if (async) {
				rc = -EAGAIN;
				hba->clk_gating.active_reqs--;
				break;
			}

			spin_unlock_irqrestore(hba->host->host_lock, flags);
			flush_result = flush_work(&hba->clk_gating.ungate_work);
			if (hba->clk_gating.is_suspended && !flush_result)
				goto out;
			spin_lock_irqsave(hba->host->host_lock, flags);
			if (hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL)
				goto start;
		}
		break;
	case REQ_CLKS_OFF:
		/*
		 * If the timer was active but the callback was not running
		 * we have nothing to do, just change state and return.
		 */
		if (hrtimer_try_to_cancel(&hba->clk_gating.gate_hrtimer) == 1) {
			hba->clk_gating.state = CLKS_ON;
			trace_ufshcd_clk_gating(dev_name(hba->dev),
						hba->clk_gating.state);
			break;
		}
		/*
		 * If we are here, it means gating work is either done or
		 * currently running. Hence, fall through to cancel gating
		 * work and to enable clocks.
		 */
	case CLKS_OFF:
		hba->clk_gating.state = REQ_CLKS_ON;
		trace_ufshcd_clk_gating(dev_name(hba->dev),
					hba->clk_gating.state);
		if (queue_work(hba->clk_gating.clk_gating_workq,
			       &hba->clk_gating.ungate_work))
			ufshcd_scsi_block_requests(hba);
		/*
		 * fall through to check if we should wait for this
		 * work to be done or not.
		 */
	case REQ_CLKS_ON:
		if (async) {
			rc = -EAGAIN;
			hba->clk_gating.active_reqs--;
			break;
		}

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		flush_work(&hba->clk_gating.ungate_work);
		/* Make sure state is CLKS_ON before returning */
		spin_lock_irqsave(hba->host->host_lock, flags);
		goto start;
	default:
		dev_err(hba->dev, "%s: clk gating is in invalid state %d\n",
				__func__, hba->clk_gating.state);
		break;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
out:
	hba->ufs_stats.clk_hold.ts = ktime_get();
	return rc;
}
EXPORT_SYMBOL_GPL(ufshcd_hold);

static void ufshcd_gate_work(struct work_struct *work)
{
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
						clk_gating.gate_work);
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->clk_gating.state == CLKS_OFF)
		goto rel_lock;
	/*
	 * In case you are here to cancel this work the gating state
	 * would be marked as REQ_CLKS_ON. In this case save time by
	 * skipping the gating work and exit after changing the clock
	 * state to CLKS_ON.
	 */
	if (hba->clk_gating.is_suspended ||
		(hba->clk_gating.state != REQ_CLKS_OFF)) {
		hba->clk_gating.state = CLKS_ON;
		trace_ufshcd_clk_gating(dev_name(hba->dev),
					hba->clk_gating.state);
		goto rel_lock;
	}

	if (hba->clk_gating.active_reqs
		|| hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL
		|| hba->lrb_in_use || hba->outstanding_tasks
		|| hba->active_uic_cmd || hba->uic_async_done)
		goto rel_lock;

	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ufshcd_is_hibern8_on_idle_allowed(hba) &&
	    hba->hibern8_on_idle.is_enabled)
		/*
		 * Hibern8 enter work (on Idle) needs clocks to be ON hence
		 * make sure that it is flushed before turning off the clocks.
		 */
		flush_delayed_work(&hba->hibern8_on_idle.enter_work);

	/* put the link into hibern8 mode before turning off clocks */
	if (ufshcd_can_hibern8_during_gating(hba)) {
		if (ufshcd_uic_hibern8_enter(hba)) {
			hba->clk_gating.state = CLKS_ON;
			trace_ufshcd_clk_gating(dev_name(hba->dev),
						hba->clk_gating.state);
			goto out;
		}
		ufshcd_set_link_hibern8(hba);
	}

	/*
	 * If auto hibern8 is enabled then the link will already
	 * be in hibern8 state and the ref clock can be gated.
	 */
	if ((ufshcd_is_auto_hibern8_enabled(hba) ||
	     !ufshcd_is_link_active(hba)) && !hba->no_ref_clk_gating)
		ufshcd_disable_clocks(hba, true);
	else
		/* If link is active, device ref_clk can't be switched off */
		ufshcd_disable_clocks_keep_link_active(hba, true);

	/* Put the host controller in low power mode if possible */
	ufshcd_hba_vreg_set_lpm(hba);

	/*
	 * In case you are here to cancel this work the gating state
	 * would be marked as REQ_CLKS_ON. In this case keep the state
	 * as REQ_CLKS_ON which would anyway imply that clocks are off
	 * and a request to turn them on is pending. By doing this way,
	 * we keep the state machine in tact and this would ultimately
	 * prevent from doing cancel work multiple times when there are
	 * new requests arriving before the current cancel work is done.
	 */
	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->clk_gating.state == REQ_CLKS_OFF) {
		hba->clk_gating.state = CLKS_OFF;
		trace_ufshcd_clk_gating(dev_name(hba->dev),
					hba->clk_gating.state);
	}
rel_lock:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
out:
	return;
}

/* host lock must be held before calling this variant */
static void __ufshcd_release(struct ufs_hba *hba, bool no_sched)
{
	if (!ufshcd_is_clkgating_allowed(hba))
		return;

	hba->clk_gating.active_reqs--;

	if (hba->clk_gating.active_reqs || hba->clk_gating.is_suspended
		|| hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL
		|| hba->lrb_in_use || hba->outstanding_tasks
		|| hba->active_uic_cmd || hba->uic_async_done
		|| ufshcd_eh_in_progress(hba) || no_sched)
		return;

	hba->clk_gating.state = REQ_CLKS_OFF;
	trace_ufshcd_clk_gating(dev_name(hba->dev), hba->clk_gating.state);
	hba->ufs_stats.clk_rel.ts = ktime_get();

	hrtimer_start(&hba->clk_gating.gate_hrtimer,
			ms_to_ktime(hba->clk_gating.delay_ms),
			HRTIMER_MODE_REL);
}

void ufshcd_release(struct ufs_hba *hba, bool no_sched)
{
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	__ufshcd_release(hba, no_sched);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}
EXPORT_SYMBOL_GPL(ufshcd_release);

static ssize_t ufshcd_clkgate_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lu\n", hba->clk_gating.delay_ms);
}

static ssize_t ufshcd_clkgate_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned long flags, value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->clk_gating.delay_ms = value;
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return count;
}

static ssize_t ufshcd_clkgate_delay_pwr_save_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lu\n",
			hba->clk_gating.delay_ms_pwr_save);
}

static ssize_t ufshcd_clkgate_delay_pwr_save_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned long flags, value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	spin_lock_irqsave(hba->host->host_lock, flags);

	hba->clk_gating.delay_ms_pwr_save = value;
	if (ufshcd_is_clkscaling_supported(hba) &&
	    !hba->clk_scaling.is_scaled_up)
		hba->clk_gating.delay_ms = hba->clk_gating.delay_ms_pwr_save;

	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return count;
}

static ssize_t ufshcd_clkgate_delay_perf_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lu\n", hba->clk_gating.delay_ms_perf);
}

static ssize_t ufshcd_clkgate_delay_perf_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned long flags, value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	spin_lock_irqsave(hba->host->host_lock, flags);

	hba->clk_gating.delay_ms_perf = value;
	if (ufshcd_is_clkscaling_supported(hba) &&
	    hba->clk_scaling.is_scaled_up)
		hba->clk_gating.delay_ms = hba->clk_gating.delay_ms_perf;

	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return count;
}

static ssize_t ufshcd_clkgate_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", hba->clk_gating.is_enabled);
}

static ssize_t ufshcd_clkgate_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned long flags;
	u32 value;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	value = !!value;
	if (value == hba->clk_gating.is_enabled)
		goto out;

	if (value) {
		ufshcd_release(hba, false);
	} else {
		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->clk_gating.active_reqs++;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}

	hba->clk_gating.is_enabled = value;
out:
	return count;
}

static enum hrtimer_restart ufshcd_clkgate_hrtimer_handler(
					struct hrtimer *timer)
{
	struct ufs_hba *hba = container_of(timer, struct ufs_hba,
					   clk_gating.gate_hrtimer);

	queue_work(hba->clk_gating.clk_gating_workq,
				&hba->clk_gating.gate_work);

	return HRTIMER_NORESTART;
}

static void ufshcd_init_clk_scaling(struct ufs_hba *hba)
{
	char wq_name[sizeof("ufs_clkscaling_00")];

	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	INIT_WORK(&hba->clk_scaling.suspend_work,
		  ufshcd_clk_scaling_suspend_work);
	INIT_WORK(&hba->clk_scaling.resume_work,
		  ufshcd_clk_scaling_resume_work);

	snprintf(wq_name, sizeof(wq_name), "ufs_clkscaling_%d",
		 hba->host->host_no);
	hba->clk_scaling.workq = create_singlethread_workqueue(wq_name);

	ufshcd_clkscaling_init_sysfs(hba);
}

static void ufshcd_exit_clk_scaling(struct ufs_hba *hba)
{
	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	destroy_workqueue(hba->clk_scaling.workq);
	ufshcd_devfreq_remove(hba);
}

static void ufshcd_init_clk_gating(struct ufs_hba *hba)
{
	struct ufs_clk_gating *gating = &hba->clk_gating;
	char wq_name[sizeof("ufs_clk_gating_00")];

	hba->clk_gating.state = CLKS_ON;

	if (!ufshcd_is_clkgating_allowed(hba))
		return;

	INIT_WORK(&gating->gate_work, ufshcd_gate_work);
	INIT_WORK(&gating->ungate_work, ufshcd_ungate_work);
	/*
	 * Clock gating work must be executed only after auto hibern8
	 * timeout has expired in the hardware or after aggressive
	 * hibern8 on idle software timeout. Using jiffy based low
	 * resolution delayed work is not reliable to guarantee this,
	 * hence use a high resolution timer to make sure we schedule
	 * the gate work precisely more than hibern8 timeout.
	 *
	 * Always make sure gating->delay_ms > hibern8_on_idle->delay_ms
	 */
	hrtimer_init(&gating->gate_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gating->gate_hrtimer.function = ufshcd_clkgate_hrtimer_handler;

	snprintf(wq_name, ARRAY_SIZE(wq_name), "ufs_clk_gating_%d",
			hba->host->host_no);
	hba->clk_gating.clk_gating_workq = alloc_ordered_workqueue("%s",
							   WQ_MEM_RECLAIM, wq_name);

	gating->is_enabled = true;

	gating->delay_ms_pwr_save = UFSHCD_CLK_GATING_DELAY_MS_PWR_SAVE;
	gating->delay_ms_perf = UFSHCD_CLK_GATING_DELAY_MS_PERF;

	/* start with performance mode */
	gating->delay_ms = gating->delay_ms_perf;

	if (!ufshcd_is_clkscaling_supported(hba))
		goto scaling_not_supported;

	gating->delay_pwr_save_attr.show = ufshcd_clkgate_delay_pwr_save_show;
	gating->delay_pwr_save_attr.store = ufshcd_clkgate_delay_pwr_save_store;
	sysfs_attr_init(&gating->delay_pwr_save_attr.attr);
	gating->delay_pwr_save_attr.attr.name = "clkgate_delay_ms_pwr_save";
	gating->delay_pwr_save_attr.attr.mode = 0644;
	if (device_create_file(hba->dev, &gating->delay_pwr_save_attr))
		dev_err(hba->dev, "Failed to create sysfs for clkgate_delay_ms_pwr_save\n");

	gating->delay_perf_attr.show = ufshcd_clkgate_delay_perf_show;
	gating->delay_perf_attr.store = ufshcd_clkgate_delay_perf_store;
	sysfs_attr_init(&gating->delay_perf_attr.attr);
	gating->delay_perf_attr.attr.name = "clkgate_delay_ms_perf";
	gating->delay_perf_attr.attr.mode = 0644;
	if (device_create_file(hba->dev, &gating->delay_perf_attr))
		dev_err(hba->dev, "Failed to create sysfs for clkgate_delay_ms_perf\n");

	goto add_clkgate_enable;

scaling_not_supported:
	hba->clk_gating.delay_attr.show = ufshcd_clkgate_delay_show;
	hba->clk_gating.delay_attr.store = ufshcd_clkgate_delay_store;
	sysfs_attr_init(&hba->clk_gating.delay_attr.attr);
	hba->clk_gating.delay_attr.attr.name = "clkgate_delay_ms";
	hba->clk_gating.delay_attr.attr.mode = 0644;
	if (device_create_file(hba->dev, &hba->clk_gating.delay_attr))
		dev_err(hba->dev, "Failed to create sysfs for clkgate_delay\n");

add_clkgate_enable:
	gating->enable_attr.show = ufshcd_clkgate_enable_show;
	gating->enable_attr.store = ufshcd_clkgate_enable_store;
	sysfs_attr_init(&gating->enable_attr.attr);
	gating->enable_attr.attr.name = "clkgate_enable";
	gating->enable_attr.attr.mode = 0644;
	if (device_create_file(hba->dev, &gating->enable_attr))
		dev_err(hba->dev, "Failed to create sysfs for clkgate_enable\n");
}

static void ufshcd_exit_clk_gating(struct ufs_hba *hba)
{
	if (!ufshcd_is_clkgating_allowed(hba))
		return;
	if (ufshcd_is_clkscaling_supported(hba)) {
		device_remove_file(hba->dev,
				   &hba->clk_gating.delay_pwr_save_attr);
		device_remove_file(hba->dev, &hba->clk_gating.delay_perf_attr);
	} else {
		device_remove_file(hba->dev, &hba->clk_gating.delay_attr);
	}
	device_remove_file(hba->dev, &hba->clk_gating.enable_attr);
	ufshcd_cancel_gate_work(hba);
	cancel_work_sync(&hba->clk_gating.ungate_work);
	destroy_workqueue(hba->clk_gating.clk_gating_workq);
}

/**
 * ufshcd_hibern8_hold - Make sure that link is not in hibern8.
 *
 * @hba: per adapter instance
 * @async: This indicates whether caller wants to exit hibern8 asynchronously.
 *
 * Exit from hibern8 mode and set the link as active.
 *
 * Return 0 on success, non-zero on failure.
 */
#if defined(CONFIG_UFSFEATURE)
int ufshcd_hibern8_hold(struct ufs_hba *hba, bool async)
#else
static int ufshcd_hibern8_hold(struct ufs_hba *hba, bool async)
#endif
{
	int rc = 0;
	unsigned long flags;

	if (!ufshcd_is_hibern8_on_idle_allowed(hba))
		goto out;

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->hibern8_on_idle.active_reqs++;

	if (ufshcd_eh_in_progress(hba)) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		return 0;
	}

start:
	switch (hba->hibern8_on_idle.state) {
	case HIBERN8_EXITED:
		break;
	case REQ_HIBERN8_ENTER:
		if (cancel_delayed_work(&hba->hibern8_on_idle.enter_work)) {
			hba->hibern8_on_idle.state = HIBERN8_EXITED;
			trace_ufshcd_hibern8_on_idle(dev_name(hba->dev),
				hba->hibern8_on_idle.state);
			break;
		}
		/*
		 * If we here, it means Hibern8 enter work is either done or
		 * currently running. Hence, fall through to cancel hibern8
		 * work and exit hibern8.
		 */
	case HIBERN8_ENTERED:
		ufshcd_scsi_block_requests(hba);
		hba->hibern8_on_idle.state = REQ_HIBERN8_EXIT;
		trace_ufshcd_hibern8_on_idle(dev_name(hba->dev),
			hba->hibern8_on_idle.state);
		schedule_work(&hba->hibern8_on_idle.exit_work);
		/*
		 * fall through to check if we should wait for this
		 * work to be done or not.
		 */
	case REQ_HIBERN8_EXIT:
		if (async) {
			rc = -EAGAIN;
			hba->hibern8_on_idle.active_reqs--;
			break;
		} else {
			spin_unlock_irqrestore(hba->host->host_lock, flags);
			flush_work(&hba->hibern8_on_idle.exit_work);
			/* Make sure state is HIBERN8_EXITED before returning */
			spin_lock_irqsave(hba->host->host_lock, flags);
			goto start;
		}
	default:
		dev_err(hba->dev, "%s: H8 is in invalid state %d\n",
				__func__, hba->hibern8_on_idle.state);
		break;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
out:
	return rc;
}

/* host lock must be held before calling this variant */
static void __ufshcd_hibern8_release(struct ufs_hba *hba, bool no_sched)
{
	unsigned long delay_in_jiffies;

	if (!ufshcd_is_hibern8_on_idle_allowed(hba))
		return;

	hba->hibern8_on_idle.active_reqs--;
	BUG_ON(hba->hibern8_on_idle.active_reqs < 0);

	if (hba->hibern8_on_idle.active_reqs
		|| hba->hibern8_on_idle.is_suspended
		|| hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL
		|| hba->lrb_in_use || hba->outstanding_tasks
		|| hba->active_uic_cmd || hba->uic_async_done
		|| ufshcd_eh_in_progress(hba) || no_sched)
		return;

	hba->hibern8_on_idle.state = REQ_HIBERN8_ENTER;
	trace_ufshcd_hibern8_on_idle(dev_name(hba->dev),
		hba->hibern8_on_idle.state);
	/*
	 * Scheduling the delayed work after 1 jiffies will make the work to
	 * get schedule any time from 0ms to 1000/HZ ms which is not desirable
	 * for hibern8 enter work as it may impact the performance if it gets
	 * scheduled almost immediately. Hence make sure that hibern8 enter
	 * work gets scheduled atleast after 2 jiffies (any time between
	 * 1000/HZ ms to 2000/HZ ms).
	 */
	delay_in_jiffies = msecs_to_jiffies(hba->hibern8_on_idle.delay_ms);
	if (delay_in_jiffies == 1)
		delay_in_jiffies++;

	schedule_delayed_work(&hba->hibern8_on_idle.enter_work,
			      delay_in_jiffies);
}

static void ufshcd_hibern8_release(struct ufs_hba *hba, bool no_sched)
{
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	__ufshcd_hibern8_release(hba, no_sched);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

static void ufshcd_hibern8_enter_work(struct work_struct *work)
{
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
					   hibern8_on_idle.enter_work.work);
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->hibern8_on_idle.is_suspended) {
		hba->hibern8_on_idle.state = HIBERN8_EXITED;
		trace_ufshcd_hibern8_on_idle(dev_name(hba->dev),
			hba->hibern8_on_idle.state);
		goto rel_lock;
	}

	if (hba->hibern8_on_idle.active_reqs
		|| hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL
		|| hba->lrb_in_use || hba->outstanding_tasks
		|| hba->active_uic_cmd || hba->uic_async_done)
		goto rel_lock;

	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ufshcd_is_link_active(hba) && ufshcd_uic_hibern8_enter(hba)) {
		/* Enter failed */
		hba->hibern8_on_idle.state = HIBERN8_EXITED;
		trace_ufshcd_hibern8_on_idle(dev_name(hba->dev),
			hba->hibern8_on_idle.state);
		goto out;
	}
	ufshcd_set_link_hibern8(hba);

	/*
	 * In case you are here to cancel this work the hibern8_on_idle.state
	 * would be marked as REQ_HIBERN8_EXIT. In this case keep the state
	 * as REQ_HIBERN8_EXIT which would anyway imply that we are in hibern8
	 * and a request to exit from it is pending. By doing this way,
	 * we keep the state machine in tact and this would ultimately
	 * prevent from doing cancel work multiple times when there are
	 * new requests arriving before the current cancel work is done.
	 */
	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->hibern8_on_idle.state == REQ_HIBERN8_ENTER) {
		hba->hibern8_on_idle.state = HIBERN8_ENTERED;
		trace_ufshcd_hibern8_on_idle(dev_name(hba->dev),
			hba->hibern8_on_idle.state);
	}
rel_lock:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
out:
	return;
}

static void ufshcd_hibern8_exit_work(struct work_struct *work)
{
	int ret;
	unsigned long flags;
	struct ufs_hba *hba = container_of(work, struct ufs_hba,
					   hibern8_on_idle.exit_work);

	cancel_delayed_work_sync(&hba->hibern8_on_idle.enter_work);

	spin_lock_irqsave(hba->host->host_lock, flags);
	if ((hba->hibern8_on_idle.state == HIBERN8_EXITED)
	     || ufshcd_is_link_active(hba)) {
		hba->hibern8_on_idle.state = HIBERN8_EXITED;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		goto unblock_reqs;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/* Exit from hibern8 */
	if (ufshcd_is_link_hibern8(hba)) {
		hba->ufs_stats.clk_hold.ctx = H8_EXIT_WORK;
		ufshcd_hold(hba, false);
		ret = ufshcd_uic_hibern8_exit(hba);
		hba->ufs_stats.clk_rel.ctx = H8_EXIT_WORK;
		ufshcd_release(hba, false);
		if (!ret) {
			spin_lock_irqsave(hba->host->host_lock, flags);
			ufshcd_set_link_active(hba);
			hba->hibern8_on_idle.state = HIBERN8_EXITED;
			trace_ufshcd_hibern8_on_idle(dev_name(hba->dev),
				hba->hibern8_on_idle.state);
			spin_unlock_irqrestore(hba->host->host_lock, flags);
		}
	}
unblock_reqs:
	ufshcd_scsi_unblock_requests(hba);
}

static ssize_t ufshcd_hibern8_on_idle_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lu\n", hba->hibern8_on_idle.delay_ms);
}

static ssize_t ufshcd_hibern8_on_idle_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned long flags, value;
	bool change = true;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->hibern8_on_idle.delay_ms == value)
		change = false;

	if (value >= hba->clk_gating.delay_ms_pwr_save ||
	    value >= hba->clk_gating.delay_ms_perf) {
		dev_err(hba->dev, "hibern8_on_idle_delay (%lu) can not be >= to clkgate_delay_ms_pwr_save (%lu) and clkgate_delay_ms_perf (%lu)\n",
			value, hba->clk_gating.delay_ms_pwr_save,
			hba->clk_gating.delay_ms_perf);
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		return -EINVAL;
	}

	hba->hibern8_on_idle.delay_ms = value;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return count;
}

static ssize_t ufshcd_hibern8_on_idle_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			hba->hibern8_on_idle.is_enabled);
}

static ssize_t ufshcd_hibern8_on_idle_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned long flags;
	u32 value;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	value = !!value;
	if (value == hba->hibern8_on_idle.is_enabled)
		goto out;

	if (value) {
		/*
		 * As clock gating work would wait for the hibern8 enter work
		 * to finish, clocks would remain on during hibern8 enter work.
		 */
		ufshcd_hold(hba, false);
		ufshcd_release_all(hba);
	} else {
		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->hibern8_on_idle.active_reqs++;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}

	hba->hibern8_on_idle.is_enabled = value;
out:
	return count;
}

static void ufshcd_init_hibern8(struct ufs_hba *hba)
{
	struct ufs_hibern8_on_idle *h8 = &hba->hibern8_on_idle;

	/* initialize the state variable here */
	h8->state = HIBERN8_EXITED;

	if (!ufshcd_is_hibern8_on_idle_allowed(hba) &&
	    !ufshcd_is_auto_hibern8_supported(hba))
		return;

	if (ufshcd_is_auto_hibern8_supported(hba)) {
		/* Set the default auto-hiberate idle timer value to 10 ms */
		hba->ahit = FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 10) |
			    FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 3);
		h8->state = AUTO_HIBERN8;
		/*
		 * Disable SW hibern8 enter on idle in case
		 * auto hibern8 is supported
		 */
		hba->caps &= ~UFSHCD_CAP_HIBERN8_ENTER_ON_IDLE;
	} else {
		h8->delay_ms = 10;
		INIT_DELAYED_WORK(&hba->hibern8_on_idle.enter_work,
				  ufshcd_hibern8_enter_work);
		INIT_WORK(&hba->hibern8_on_idle.exit_work,
			  ufshcd_hibern8_exit_work);
		h8->is_enabled = true;

		h8->delay_attr.show = ufshcd_hibern8_on_idle_delay_show;
		h8->delay_attr.store = ufshcd_hibern8_on_idle_delay_store;
		sysfs_attr_init(&h8->delay_attr.attr);
		h8->delay_attr.attr.name = "hibern8_on_idle_delay_ms";
		h8->delay_attr.attr.mode = 0644;
		if (device_create_file(hba->dev, &h8->delay_attr))
			dev_err(hba->dev, "Failed to create sysfs for hibern8_on_idle_delay\n");

		h8->enable_attr.show = ufshcd_hibern8_on_idle_enable_show;
		h8->enable_attr.store = ufshcd_hibern8_on_idle_enable_store;
		sysfs_attr_init(&h8->enable_attr.attr);
		h8->enable_attr.attr.name = "hibern8_on_idle_enable";
		h8->enable_attr.attr.mode = 0644;
		if (device_create_file(hba->dev, &h8->enable_attr))
			dev_err(hba->dev, "Failed to create sysfs for hibern8_on_idle_enable\n");

	}
}

static void ufshcd_exit_hibern8_on_idle(struct ufs_hba *hba)
{
	if (!ufshcd_is_hibern8_on_idle_allowed(hba) &&
	    !ufshcd_is_auto_hibern8_supported(hba))
		return;
	device_remove_file(hba->dev, &hba->hibern8_on_idle.delay_attr);
	device_remove_file(hba->dev, &hba->hibern8_on_idle.enable_attr);
}

#if defined (CONFIG_UFSFEATURE)
void ufshcd_hold_all(struct ufs_hba *hba)
#else
static void ufshcd_hold_all(struct ufs_hba *hba)
#endif
{
	ufshcd_hold(hba, false);
	ufshcd_hibern8_hold(hba, false);
}

#if defined (CONFIG_UFSFEATURE)
void ufshcd_release_all(struct ufs_hba *hba)
#else
static void ufshcd_release_all(struct ufs_hba *hba)
#endif
{
	ufshcd_hibern8_release(hba, false);
	ufshcd_release(hba, false);
}

/* Must be called with host lock acquired */
static void ufshcd_clk_scaling_start_busy(struct ufs_hba *hba)
{
	bool queue_resume_work = false;

	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	if (!hba->clk_scaling.active_reqs++)
		queue_resume_work = true;

	if (!hba->clk_scaling.is_allowed || hba->pm_op_in_progress)
		return;

	if (queue_resume_work)
		queue_work(hba->clk_scaling.workq,
			   &hba->clk_scaling.resume_work);

	if (!hba->clk_scaling.window_start_t) {
		hba->clk_scaling.window_start_t = jiffies;
		hba->clk_scaling.tot_busy_t = 0;
		hba->clk_scaling.is_busy_started = false;
	}

	if (!hba->clk_scaling.is_busy_started) {
		hba->clk_scaling.busy_start_t = ktime_get();
		hba->clk_scaling.is_busy_started = true;
	}
}

static void ufshcd_clk_scaling_update_busy(struct ufs_hba *hba)
{
	struct ufs_clk_scaling *scaling = &hba->clk_scaling;

	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	if (!hba->outstanding_reqs && scaling->is_busy_started) {
		scaling->tot_busy_t += ktime_to_us(ktime_sub(ktime_get(),
					scaling->busy_start_t));
		scaling->busy_start_t = 0;
		scaling->is_busy_started = false;
	}
}
/**
 * ufshcd_send_command - Send SCSI or device management commands
 * @hba: per adapter instance
 * @task_tag: Task tag of the command
 */
static inline
int ufshcd_send_command(struct ufs_hba *hba, unsigned int task_tag)
{
	hba->lrb[task_tag].issue_time_stamp = ktime_get();
	hba->lrb[task_tag].compl_time_stamp = ktime_set(0, 0);
	ufshcd_clk_scaling_start_busy(hba);
	__set_bit(task_tag, &hba->outstanding_reqs);
	ufshcd_writel(hba, 1 << task_tag, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	/* Make sure that doorbell is committed immediately */
	wmb();
	ufshcd_cond_add_cmd_trace(hba, task_tag,
			hba->lrb[task_tag].cmd ? "scsi_send" : "dev_cmd_send");
	ufshcd_update_tag_stats(hba, task_tag);
	return 0;
}

/**
 * ufshcd_copy_sense_data - Copy sense data in case of check condition
 * @lrbp: pointer to local reference block
 */
static inline void ufshcd_copy_sense_data(struct ufshcd_lrb *lrbp)
{
	int len;
	if (lrbp->sense_buffer &&
	    ufshcd_get_rsp_upiu_data_seg_len(lrbp->ucd_rsp_ptr)) {
		int len_to_copy;

		len = be16_to_cpu(lrbp->ucd_rsp_ptr->sr.sense_data_len);
		len_to_copy = min_t(int, RESPONSE_UPIU_SENSE_DATA_LENGTH, len);

		memcpy(lrbp->sense_buffer,
			lrbp->ucd_rsp_ptr->sr.sense_data,
			min_t(int, len_to_copy, UFSHCD_REQ_SENSE_SIZE));
	}
}

/**
 * ufshcd_copy_query_response() - Copy the Query Response and the data
 * descriptor
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block
 */
static
int ufshcd_copy_query_response(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_query_res *query_res = &hba->dev_cmd.query.response;

	memcpy(&query_res->upiu_res, &lrbp->ucd_rsp_ptr->qr, QUERY_OSF_SIZE);

	/* Get the descriptor */
	if (hba->dev_cmd.query.descriptor &&
	    lrbp->ucd_rsp_ptr->qr.opcode == UPIU_QUERY_OPCODE_READ_DESC) {
		u8 *descp = (u8 *)lrbp->ucd_rsp_ptr +
				GENERAL_UPIU_REQUEST_SIZE;
		u16 resp_len;
		u16 buf_len;

		/* data segment length */
		resp_len = be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2) &
						MASK_QUERY_DATA_SEG_LEN;
		buf_len = be16_to_cpu(
				hba->dev_cmd.query.request.upiu_req.length);
		if (likely(buf_len >= resp_len)) {
			memcpy(hba->dev_cmd.query.descriptor, descp, resp_len);
		} else {
			dev_warn(hba->dev,
				"%s: Response size is bigger than buffer",
				__func__);
			return -EINVAL;
		}
	}

	return 0;
}

/**
 * ufshcd_hba_capabilities - Read controller capabilities
 * @hba: per adapter instance
 */
static inline void ufshcd_hba_capabilities(struct ufs_hba *hba)
{
	hba->capabilities = ufshcd_readl(hba, REG_CONTROLLER_CAPABILITIES);

	/* nutrs and nutmrs are 0 based values */
	hba->nutrs = (hba->capabilities & MASK_TRANSFER_REQUESTS_SLOTS) + 1;
	hba->nutmrs =
	((hba->capabilities & MASK_TASK_MANAGEMENT_REQUEST_SLOTS) >> 16) + 1;
}

/**
 * ufshcd_ready_for_uic_cmd - Check if controller is ready
 *                            to accept UIC commands
 * @hba: per adapter instance
 * Return true on success, else false
 */
static inline bool ufshcd_ready_for_uic_cmd(struct ufs_hba *hba)
{
	if (ufshcd_readl(hba, REG_CONTROLLER_STATUS) & UIC_COMMAND_READY)
		return true;
	else
		return false;
}

/**
 * ufshcd_get_upmcrs - Get the power mode change request status
 * @hba: Pointer to adapter instance
 *
 * This function gets the UPMCRS field of HCS register
 * Returns value of UPMCRS field
 */
static inline u8 ufshcd_get_upmcrs(struct ufs_hba *hba)
{
	return (ufshcd_readl(hba, REG_CONTROLLER_STATUS) >> 8) & 0x7;
}

/**
 * ufshcd_dispatch_uic_cmd - Dispatch UIC commands to unipro layers
 * @hba: per adapter instance
 * @uic_cmd: UIC command
 *
 * Mutex must be held.
 */
static inline void
ufshcd_dispatch_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	WARN_ON(hba->active_uic_cmd);

	hba->active_uic_cmd = uic_cmd;

	ufshcd_dme_cmd_log(hba, "dme_send", hba->active_uic_cmd->command);
	/* Write Args */
	ufshcd_writel(hba, uic_cmd->argument1, REG_UIC_COMMAND_ARG_1);
	ufshcd_writel(hba, uic_cmd->argument2, REG_UIC_COMMAND_ARG_2);
	ufshcd_writel(hba, uic_cmd->argument3, REG_UIC_COMMAND_ARG_3);

	/* Write UIC Cmd */
	ufshcd_writel(hba, uic_cmd->command & COMMAND_OPCODE_MASK,
		      REG_UIC_COMMAND);
	/* Make sure that UIC command is committed immediately */
	wmb();
}

/**
 * ufshcd_wait_for_uic_cmd - Wait complectioin of UIC command
 * @hba: per adapter instance
 * @uic_cmd: UIC command
 *
 * Must be called with mutex held.
 * Returns 0 only if success.
 */
static int
ufshcd_wait_for_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	int ret;
	unsigned long flags;

	if (wait_for_completion_timeout(&uic_cmd->done,
					msecs_to_jiffies(UIC_CMD_TIMEOUT)))
		ret = uic_cmd->argument2 & MASK_UIC_COMMAND_RESULT;
	else
		ret = -ETIMEDOUT;

	if (ret)
		ufsdbg_set_err_state(hba);

	ufshcd_dme_cmd_log(hba, "dme_cmpl_1", hba->active_uic_cmd->command);
#if defined(SEC_UFS_ERROR_COUNT)
	if (ret)
		SEC_ufs_uic_error_check(hba, true, false);
#endif

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->active_uic_cmd = NULL;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return ret;
}

/**
 * __ufshcd_send_uic_cmd - Send UIC commands and retrieve the result
 * @hba: per adapter instance
 * @uic_cmd: UIC command
 * @completion: initialize the completion only if this is set to true
 *
 * Identical to ufshcd_send_uic_cmd() expect mutex. Must be called
 * with mutex held and host_lock locked.
 * Returns 0 only if success.
 */
static int
__ufshcd_send_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd,
		      bool completion)
{
	if (!ufshcd_ready_for_uic_cmd(hba)) {
		dev_err(hba->dev,
			"Controller not ready to accept UIC commands\n");
		return -EIO;
	}

	if (completion)
		init_completion(&uic_cmd->done);

	ufshcd_dispatch_uic_cmd(hba, uic_cmd);

	return 0;
}

/**
 * ufshcd_send_uic_cmd - Send UIC commands and retrieve the result
 * @hba: per adapter instance
 * @uic_cmd: UIC command
 *
 * Returns 0 only if success.
 */
static int
ufshcd_send_uic_cmd(struct ufs_hba *hba, struct uic_command *uic_cmd)
{
	int ret;
	unsigned long flags;

	hba->ufs_stats.clk_hold.ctx = UIC_CMD_SEND;
	ufshcd_hold_all(hba);
	mutex_lock(&hba->uic_cmd_mutex);
	ufshcd_add_delay_before_dme_cmd(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = __ufshcd_send_uic_cmd(hba, uic_cmd, true);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (!ret)
		ret = ufshcd_wait_for_uic_cmd(hba, uic_cmd);

	ufshcd_save_tstamp_of_last_dme_cmd(hba);
	mutex_unlock(&hba->uic_cmd_mutex);
	ufshcd_release_all(hba);
	hba->ufs_stats.clk_rel.ctx = UIC_CMD_SEND;

	ufsdbg_error_inject_dispatcher(hba,
		ERR_INJECT_UIC, 0, &ret);

	return ret;
}

/**
 * ufshcd_map_sg - Map scatter-gather list to prdt
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block
 *
 * Returns 0 in case of success, non-zero value in case of failure
 */
#if defined(CONFIG_UFSFEATURE)
int ufshcd_map_sg(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
#else
static int ufshcd_map_sg(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
#endif
{
	struct ufshcd_sg_entry *prd;
	struct scatterlist *sg;
	struct scsi_cmnd *cmd;
	int sg_segments;
	int i;
#if defined(CONFIG_UFS_DATA_LOG)
	unsigned int dump_index;
	int cpu = raw_smp_processor_id();
	int id = 0;

#if defined(CONFIG_UFS_DATA_LOG_MAGIC_CODE)
	unsigned long  magicword[1];
	void  *virt_sg_addr;
	/*
	 * 0x1F5E3A7069245CBE
	 * 0x4977C72608FCE51F
	 * 0x524E74A9005F0D1B
	 * 0x05D3D11A26CF3142
	 */
	magicword[0] = 0x1F5E3A7069245CBE;
#endif
#endif

	cmd = lrbp->cmd;
#if defined(CONFIG_UFS_DATA_LOG)
	if (cmd->request && hba->host->ufs_sys_log_en){
		/*condition of data log*/
		/*read request of system area for debugging dm verity issue*/
		if (rq_data_dir(cmd->request) == READ &&
				cmd->request->__sector >= hba->host->ufs_system_start &&
				cmd->request->__sector < hba->host->ufs_system_end) {

			for (id = 0; id < UFS_DATA_LOG_MAX; id++){
				dump_index = atomic_inc_return(&hba->log_count) & (UFS_DATA_LOG_MAX - 1);
				if (ufs_data_log[dump_index].done != 0xA)
					break;
			}

			if (id == UFS_DATA_LOG_MAX)
				queuing_req[cmd->request->tag] = UFS_DATA_LOG_MAX - 1;
			else
				queuing_req[cmd->request->tag] = dump_index;

			ufs_data_log[dump_index].done = 0xA;
			ufs_data_log[dump_index].start_time = cpu_clock(cpu);
			ufs_data_log[dump_index].end_time = 0;
			ufs_data_log[dump_index].sector = cmd->request->__sector;

#if defined(CONFIG_UFS_DATA_LOG_MAGIC_CODE)
			sg_segments = scsi_sg_count(cmd);
			scsi_for_each_sg(cmd, sg, sg_segments, i) {
				/*
				 * Write magicword each memory location pointed  by SG element
				 */
				virt_sg_addr = sg_virt(sg);
				ufs_data_log[dump_index].virt_addr = virt_sg_addr;
				memcpy(virt_sg_addr, magicword, UFS_DATA_BUF_SIZE);
			}
#endif
		}
	}
#endif

	sg_segments = scsi_dma_map(cmd);
	if (sg_segments < 0)
		return sg_segments;

	if (sg_segments) {
		if (hba->quirks & UFSHCD_QUIRK_PRDT_BYTE_GRAN)
			lrbp->utr_descriptor_ptr->prd_table_length =
				cpu_to_le16((u16)(sg_segments *
						  hba->sg_entry_size));
		else
			lrbp->utr_descriptor_ptr->prd_table_length =
				cpu_to_le16((u16) (sg_segments));

		prd = (struct ufshcd_sg_entry *)lrbp->ucd_prdt_ptr;

		scsi_for_each_sg(cmd, sg, sg_segments, i) {
			prd->size =
				cpu_to_le32(((u32) sg_dma_len(sg))-1);
			prd->base_addr =
				cpu_to_le32(lower_32_bits(sg->dma_address));
			prd->upper_addr =
				cpu_to_le32(upper_32_bits(sg->dma_address));
			prd->reserved = 0;
			hba->transferred_sector += prd->size;
			prd = (void *)prd + hba->sg_entry_size;
		}
	} else {
		lrbp->utr_descriptor_ptr->prd_table_length = 0;
	}

	return ufshcd_map_sg_crypto(hba, lrbp);
}

/**
 * ufshcd_enable_intr - enable interrupts
 * @hba: per adapter instance
 * @intrs: interrupt bits
 */
static void ufshcd_enable_intr(struct ufs_hba *hba, u32 intrs)
{
	u32 set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	if (hba->ufs_version == UFSHCI_VERSION_10) {
		u32 rw;
		rw = set & INTERRUPT_MASK_RW_VER_10;
		set = rw | ((set ^ intrs) & intrs);
	} else {
		set |= intrs;
	}

	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
}

/**
 * ufshcd_disable_intr - disable interrupts
 * @hba: per adapter instance
 * @intrs: interrupt bits
 */
static void ufshcd_disable_intr(struct ufs_hba *hba, u32 intrs)
{
	u32 set = ufshcd_readl(hba, REG_INTERRUPT_ENABLE);

	if (hba->ufs_version == UFSHCI_VERSION_10) {
		u32 rw;
		rw = (set & INTERRUPT_MASK_RW_VER_10) &
			~(intrs & INTERRUPT_MASK_RW_VER_10);
		set = rw | ((set & intrs) & ~INTERRUPT_MASK_RW_VER_10);

	} else {
		set &= ~intrs;
	}

	ufshcd_writel(hba, set, REG_INTERRUPT_ENABLE);
}

/* IOPP-upiu_flags-v1.2.k5.4 */
static void set_customized_upiu_flags(struct ufshcd_lrb *lrbp, u32 *upiu_flags)
{
	if (!lrbp->cmd || !lrbp->cmd->request)
		return;

	switch (req_op(lrbp->cmd->request)) {
	case REQ_OP_READ:
		*upiu_flags |= UPIU_CMD_PRIO_HIGH;
		break;
	case REQ_OP_WRITE:
		if (lrbp->cmd->request->cmd_flags & REQ_SYNC)
			*upiu_flags |= UPIU_CMD_PRIO_HIGH;
		break;
	case REQ_OP_FLUSH:
		*upiu_flags |= UPIU_TASK_ATTR_HEADQ;
		break;
	case REQ_OP_DISCARD:
		*upiu_flags |= UPIU_TASK_ATTR_ORDERED;
		break;
	}
}

/**
 * ufshcd_prepare_req_desc_hdr() - Fills the requests header
 * descriptor according to request
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block
 * @upiu_flags: flags required in the header
 * @cmd_dir: requests data direction
 */
static int ufshcd_prepare_req_desc_hdr(struct ufs_hba *hba,
	struct ufshcd_lrb *lrbp, u32 *upiu_flags,
	enum dma_data_direction cmd_dir)
{
	struct utp_transfer_req_desc *req_desc = lrbp->utr_descriptor_ptr;
	u32 data_direction;
	u32 dword_0;

	if (cmd_dir == DMA_FROM_DEVICE) {
		data_direction = UTP_DEVICE_TO_HOST;
		*upiu_flags = UPIU_CMD_FLAGS_READ;
	} else if (cmd_dir == DMA_TO_DEVICE) {
		data_direction = UTP_HOST_TO_DEVICE;
		*upiu_flags = UPIU_CMD_FLAGS_WRITE;
	} else {
		data_direction = UTP_NO_DATA_TRANSFER;
		*upiu_flags = UPIU_CMD_FLAGS_NONE;
	}

	set_customized_upiu_flags(lrbp, upiu_flags);

	dword_0 = data_direction | (lrbp->command_type
				<< UPIU_COMMAND_TYPE_OFFSET);
	if (lrbp->intr_cmd)
		dword_0 |= UTP_REQ_DESC_INT_CMD;

	/* Transfer request descriptor header fields */
	if (ufshcd_lrbp_crypto_enabled(lrbp)) {
#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
		dword_0 |= UTP_REQ_DESC_CRYPTO_ENABLE_CMD;
		dword_0 |= lrbp->crypto_key_slot;
		req_desc->header.dword_1 =
			cpu_to_le32(lower_32_bits(lrbp->data_unit_num));
		req_desc->header.dword_3 =
			cpu_to_le32(upper_32_bits(lrbp->data_unit_num));
#endif /* CONFIG_SCSI_UFS_CRYPTO */
	} else {
		/* dword_1 and dword_3 are reserved, hence they are set to 0 */
		req_desc->header.dword_1 = 0;
		req_desc->header.dword_3 = 0;
	}

	req_desc->header.dword_0 = cpu_to_le32(dword_0);

	/*
	 * assigning invalid value for command status. Controller
	 * updates OCS on command completion, with the command
	 * status
	 */
	req_desc->header.dword_2 =
		cpu_to_le32(OCS_INVALID_COMMAND_STATUS);

	req_desc->prd_table_length = 0;

	return 0;
}

/**
 * ufshcd_prepare_utp_scsi_cmd_upiu() - fills the utp_transfer_req_desc,
 * for scsi commands
 * @lrbp: local reference block pointer
 * @upiu_flags: flags
 */
static
void ufshcd_prepare_utp_scsi_cmd_upiu(struct ufshcd_lrb *lrbp, u32 upiu_flags)
{
	struct utp_upiu_req *ucd_req_ptr = lrbp->ucd_req_ptr;
	unsigned short cdb_len;

	/* command descriptor fields */
	ucd_req_ptr->header.dword_0 = UPIU_HEADER_DWORD(
				UPIU_TRANSACTION_COMMAND, upiu_flags,
				lrbp->lun, lrbp->task_tag);
	ucd_req_ptr->header.dword_1 = UPIU_HEADER_DWORD(
				UPIU_COMMAND_SET_TYPE_SCSI, 0, 0, 0);

	/* Total EHS length and Data segment length will be zero */
	ucd_req_ptr->header.dword_2 = 0;

	ucd_req_ptr->sc.exp_data_transfer_len =
		cpu_to_be32(lrbp->cmd->sdb.length);

	cdb_len = min_t(unsigned short, lrbp->cmd->cmd_len, MAX_CDB_SIZE);
	memcpy(ucd_req_ptr->sc.cdb, lrbp->cmd->cmnd, cdb_len);
	if (cdb_len < MAX_CDB_SIZE)
		memset(ucd_req_ptr->sc.cdb + cdb_len, 0,
			(MAX_CDB_SIZE - cdb_len));
	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));
}

/**
 * ufshcd_prepare_utp_query_req_upiu() - fills the utp_transfer_req_desc,
 * for query requsts
 * @hba: UFS hba
 * @lrbp: local reference block pointer
 * @upiu_flags: flags
 */
static void ufshcd_prepare_utp_query_req_upiu(struct ufs_hba *hba,
				struct ufshcd_lrb *lrbp, u32 upiu_flags)
{
	struct utp_upiu_req *ucd_req_ptr = lrbp->ucd_req_ptr;
	struct ufs_query *query = &hba->dev_cmd.query;
	u16 len = be16_to_cpu(query->request.upiu_req.length);
	u8 *descp = (u8 *)lrbp->ucd_req_ptr + GENERAL_UPIU_REQUEST_SIZE;

	/* Query request header */
	ucd_req_ptr->header.dword_0 = UPIU_HEADER_DWORD(
			UPIU_TRANSACTION_QUERY_REQ, upiu_flags,
			lrbp->lun, lrbp->task_tag);
	ucd_req_ptr->header.dword_1 = UPIU_HEADER_DWORD(
			0, query->request.query_func, 0, 0);

	/* Data segment length only need for WRITE_DESC */
	if (query->request.upiu_req.opcode == UPIU_QUERY_OPCODE_WRITE_DESC)
		ucd_req_ptr->header.dword_2 =
			UPIU_HEADER_DWORD(0, 0, (len >> 8), (u8)len);
	else
		ucd_req_ptr->header.dword_2 = 0;

	/* Copy the Query Request buffer as is */
	memcpy(&ucd_req_ptr->qr, &query->request.upiu_req,
			QUERY_OSF_SIZE);

	/* Copy the Descriptor */
	if (query->request.upiu_req.opcode == UPIU_QUERY_OPCODE_WRITE_DESC)
		memcpy(descp, query->descriptor, len);

	if (query->request.query_func == UPIU_QUERY_FUNC_VENDOR_TOSHIBA_FATALMODE) {
		ucd_req_ptr->header.dword_2 =
			UPIU_HEADER_DWORD(0, 0, (len >> 8), (u8)len);
		memcpy(descp, query->descriptor, len);
	}

	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));
}

static inline void ufshcd_prepare_utp_nop_upiu(struct ufshcd_lrb *lrbp)
{
	struct utp_upiu_req *ucd_req_ptr = lrbp->ucd_req_ptr;

	memset(ucd_req_ptr, 0, sizeof(struct utp_upiu_req));

	/* command descriptor fields */
	ucd_req_ptr->header.dword_0 =
		UPIU_HEADER_DWORD(
			UPIU_TRANSACTION_NOP_OUT, 0, 0, lrbp->task_tag);
	/* clear rest of the fields of basic header */
	ucd_req_ptr->header.dword_1 = 0;
	ucd_req_ptr->header.dword_2 = 0;

	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));
}

/**
 * ufshcd_comp_devman_upiu - UFS Protocol Information Unit(UPIU)
 *			     for Device Management Purposes
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block
 */
static int ufshcd_comp_devman_upiu(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	u32 upiu_flags;
	int ret = 0;

	if ((hba->ufs_version == UFSHCI_VERSION_10) ||
	    (hba->ufs_version == UFSHCI_VERSION_11))
		lrbp->command_type = UTP_CMD_TYPE_DEV_MANAGE;
	else
		lrbp->command_type = UTP_CMD_TYPE_UFS_STORAGE;

	ret = ufshcd_prepare_req_desc_hdr(hba, lrbp, &upiu_flags,
			DMA_NONE);
	if (hba->dev_cmd.type == DEV_CMD_TYPE_QUERY)
		ufshcd_prepare_utp_query_req_upiu(hba, lrbp, upiu_flags);
	else if (hba->dev_cmd.type == DEV_CMD_TYPE_NOP)
		ufshcd_prepare_utp_nop_upiu(lrbp);
	else
		ret = -EINVAL;

	return ret;
}

/**
 * ufshcd_comp_scsi_upiu - UFS Protocol Information Unit(UPIU)
 *			   for SCSI Purposes
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block
 */
#if defined(CONFIG_UFSFEATURE)
int ufshcd_comp_scsi_upiu(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
#else
static int ufshcd_comp_scsi_upiu(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
#endif
{
	u32 upiu_flags;
	int ret = 0;

	if ((hba->ufs_version == UFSHCI_VERSION_10) ||
	    (hba->ufs_version == UFSHCI_VERSION_11))
		lrbp->command_type = UTP_CMD_TYPE_SCSI;
	else
		lrbp->command_type = UTP_CMD_TYPE_UFS_STORAGE;

	if (likely(lrbp->cmd)) {
#if defined(CONFIG_UFSFEATURE)
		ufsf_hpb_change_lun(&hba->ufsf, lrbp);
		ufsf_hpb_prep_fn(&hba->ufsf, lrbp);
#endif
		ret = ufshcd_prepare_req_desc_hdr(hba, lrbp,
				&upiu_flags, lrbp->cmd->sc_data_direction);
		ufshcd_prepare_utp_scsi_cmd_upiu(lrbp, upiu_flags);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

/**
 * ufshcd_upiu_wlun_to_scsi_wlun - maps UPIU W-LUN id to SCSI W-LUN ID
 * @upiu_wlun_id: UPIU W-LUN id
 *
 * Returns SCSI W-LUN id
 */
static inline u16 ufshcd_upiu_wlun_to_scsi_wlun(u8 upiu_wlun_id)
{
	return (upiu_wlun_id & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE;
}

/**
 * ufshcd_get_write_lock - synchronize between shutdown, scaling &
 * arrival of requests
 * @hba: ufs host
 *
 * Lock is predominantly held by shutdown context thus, ensuring
 * that no requests from any other context may sneak through.
 */
static inline void ufshcd_get_write_lock(struct ufs_hba *hba)
{
	down_write(&hba->lock);
}

/**
 * ufshcd_get_read_lock - synchronize between shutdown, scaling &
 * arrival of requests
 * @hba: ufs host
 *
 * Returns 1 if acquired, < 0 on contention
 *
 * After shutdown's initiated, allow requests only directed to the
 * well known device lun. The sync between scaling & issue is maintained
 * as is and this restructuring syncs shutdown with these too.
 */
static int ufshcd_get_read_lock(struct ufs_hba *hba, u64 lun)
{
	int err = 0;

	err = down_read_trylock(&hba->lock);
	if (err > 0)
		goto out;
	/* let requests for well known device lun to go through */
	if (ufshcd_scsi_to_upiu_lun(lun) == UFS_UPIU_UFS_DEVICE_WLUN)
		return 0;
	else if (!ufshcd_is_shutdown_ongoing(hba))
		return -EAGAIN;
	else
		return -EPERM;

out:
	return err;
}

/**
 * ufshcd_put_read_lock - synchronize between shutdown, scaling &
 * arrival of requests
 * @hba: ufs host
 *
 * Returns none
 */
static inline void ufshcd_put_read_lock(struct ufs_hba *hba)
{
	up_read(&hba->lock);
}

/**
 * ufshcd_queuecommand - main entry point for SCSI requests
 * @host: SCSI host pointer
 * @cmd: command from SCSI Midlayer
 *
 * Returns 0 for success, non-zero in case of failure
 */
static int ufshcd_queuecommand(struct Scsi_Host *host, struct scsi_cmnd *cmd)
{
	struct ufshcd_lrb *lrbp;
	struct ufs_hba *hba;
	unsigned long flags;
	int tag;
	int err = 0;
	bool has_read_lock = false;
#if defined(CONFIG_UFSFEATURE) && defined(CONFIG_UFSHPB)
	struct scsi_cmnd *pre_cmd;
	struct ufshcd_lrb *add_lrbp;
	int add_tag;
	int pre_req_err = -EBUSY;
	int lun = 0;
	
	if (cmd && cmd->device)
		lun = ufshcd_scsi_to_upiu_lun(cmd->device->lun);
#endif

	hba = shost_priv(host);

	if (!cmd || !cmd->request || !hba)
		return -EINVAL;

	tag = cmd->request->tag;
	if (!ufshcd_valid_tag(hba, tag)) {
		dev_err(hba->dev,
			"%s: invalid command tag %d: cmd=0x%pK, cmd->request=0x%pK\n",
			__func__, tag, cmd, cmd->request);
		BUG_ON(1);
	}

	err = ufshcd_get_read_lock(hba, cmd->device->lun);
	if (unlikely(err < 0)) {
		if (err == -EPERM) {
			set_host_byte(cmd, DID_ERROR);
			cmd->scsi_done(cmd);
			return 0;
		}
		if (err == -EAGAIN) {
			hba->ufs_stats.scsi_blk_reqs.ts = ktime_get();
			hba->ufs_stats.scsi_blk_reqs.busy_ctx = SCALING_BUSY;
			return SCSI_MLQUEUE_HOST_BUSY;
		}
	} else if (err == 1) {
		has_read_lock = true;
	}

	/*
	 * err might be non-zero here but logic later in this function
	 * assumes that err is set to 0.
	 */
	err = 0;

	spin_lock_irqsave(hba->host->host_lock, flags);

	/* if error handling is in progress, return host busy */
	if (ufshcd_eh_in_progress(hba)) {
		err = SCSI_MLQUEUE_HOST_BUSY;
		hba->ufs_stats.scsi_blk_reqs.ts = ktime_get();
		hba->ufs_stats.scsi_blk_reqs.busy_ctx = EH_IN_PROGRESS;
		goto out_unlock;
	}

	switch (hba->ufshcd_state) {
	case UFSHCD_STATE_OPERATIONAL:
		break;
	case UFSHCD_STATE_EH_SCHEDULED:
	case UFSHCD_STATE_RESET:
		err = SCSI_MLQUEUE_HOST_BUSY;
		hba->ufs_stats.scsi_blk_reqs.ts = ktime_get();
		hba->ufs_stats.scsi_blk_reqs.busy_ctx =
			UFS_RESET_OR_EH_SCHEDULED;
		goto out_unlock;
	case UFSHCD_STATE_ERROR:
		set_host_byte(cmd, DID_ERROR);
		cmd->scsi_done(cmd);
		goto out_unlock;
	default:
		dev_WARN_ONCE(hba->dev, 1, "%s: invalid state %d\n",
				__func__, hba->ufshcd_state);
		set_host_byte(cmd, DID_BAD_TARGET);
		cmd->scsi_done(cmd);
		goto out_unlock;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	hba->req_abort_count = 0;

	/* acquire the tag to make sure device cmds don't use it */
	if (test_and_set_bit_lock(tag, &hba->lrb_in_use)) {
		/*
		 * Dev manage command in progress, requeue the command.
		 * Requeuing the command helps in cases where the request *may*
		 * find different tag instead of waiting for dev manage command
		 * completion.
		 */
		err = SCSI_MLQUEUE_HOST_BUSY;
		hba->ufs_stats.scsi_blk_reqs.ts = ktime_get();
		hba->ufs_stats.scsi_blk_reqs.busy_ctx = LRB_IN_USE;
		goto out;
	}

	hba->ufs_stats.clk_hold.ctx = QUEUE_CMD;
	err = ufshcd_hold(hba, true);
	if (err) {
		err = SCSI_MLQUEUE_HOST_BUSY;
		hba->ufs_stats.scsi_blk_reqs.ts = ktime_get();
		hba->ufs_stats.scsi_blk_reqs.busy_ctx = UFSHCD_HOLD;
		clear_bit_unlock(tag, &hba->lrb_in_use);
		goto out;
	}
	if (ufshcd_is_clkgating_allowed(hba))
		WARN_ON(hba->clk_gating.state != CLKS_ON);

	err = ufshcd_hibern8_hold(hba, true);
	if (err) {
		clear_bit_unlock(tag, &hba->lrb_in_use);
		err = SCSI_MLQUEUE_HOST_BUSY;
		hba->ufs_stats.scsi_blk_reqs.ts = ktime_get();
		hba->ufs_stats.scsi_blk_reqs.busy_ctx = UFSHCD_HIBERN8_HOLD;
		hba->ufs_stats.clk_rel.ctx = QUEUE_CMD;
		ufshcd_release(hba, true);
		goto out;
	}
	if (ufshcd_is_hibern8_on_idle_allowed(hba))
		WARN_ON(hba->hibern8_on_idle.state != HIBERN8_EXITED);

#if defined(CONFIG_UFSFEATURE) && defined(CONFIG_UFSHPB)
	add_tag = ufsf_hpb_prepare_pre_req(&hba->ufsf, cmd, lun);
	if (add_tag < 0) {
		hba->lrb[tag].hpb_ctx_id = MAX_HPB_CONTEXT_ID;
		goto send_orig_cmd;
	}

	add_lrbp = &hba->lrb[add_tag];

	pre_req_err = ufsf_hpb_prepare_add_lrbp(&hba->ufsf, add_tag);
	if (pre_req_err)
		hba->lrb[tag].hpb_ctx_id = MAX_HPB_CONTEXT_ID;
send_orig_cmd:
#endif
	/* Vote PM QoS for the request */
	ufshcd_vops_pm_qos_req_start(hba, cmd->request);

	WARN_ON(hba->clk_gating.state != CLKS_ON);

	lrbp = &hba->lrb[tag];

	WARN_ON(lrbp->cmd);
	lrbp->cmd = cmd;
	lrbp->sense_bufflen = UFSHCD_REQ_SENSE_SIZE;
	lrbp->sense_buffer = cmd->sense_buffer;
	lrbp->task_tag = tag;
	lrbp->lun = ufshcd_scsi_to_upiu_lun(cmd->device->lun);
	lrbp->intr_cmd = !ufshcd_is_intr_aggr_allowed(hba) ? true : false;

	err = ufshcd_prepare_lrbp_crypto(hba, cmd, lrbp);
	if (err) {
		lrbp->cmd = NULL;
		clear_bit_unlock(tag, &hba->lrb_in_use);
		goto out;
	}
	lrbp->req_abort_skip = false;

	err = ufshcd_comp_scsi_upiu(hba, lrbp);
	if (err) {
		if (err != -EAGAIN)
			dev_err(hba->dev,
				"%s: failed to compose upiu %d\n",
				__func__, err);

		lrbp->cmd = NULL;
		clear_bit_unlock(tag, &hba->lrb_in_use);
		ufshcd_release_all(hba);
		ufshcd_vops_pm_qos_req_end(hba, cmd->request, true);
		goto out;
	}

	err = ufshcd_map_sg(hba, lrbp);
	if (err) {
		lrbp->cmd = NULL;
		clear_bit_unlock(tag, &hba->lrb_in_use);
		ufshcd_release_all(hba);
		ufshcd_vops_pm_qos_req_end(hba, cmd->request, true);
		goto out;
	}


	/* Make sure descriptors are ready before ringing the doorbell */
	wmb();

	/* issue command to the controller */
	spin_lock_irqsave(hba->host->host_lock, flags);
#if defined(CONFIG_UFSFEATURE) && defined(CONFIG_UFSHPB)
	if (!pre_req_err) {
		ufshcd_vops_setup_xfer_req(hba, add_tag, (add_lrbp->cmd ? true : false));
		ufshcd_send_command(hba, add_tag);
		pre_req_err = -EBUSY;
		atomic64_inc(&hba->ufsf.ufshpb_lup[add_lrbp->lun]->pre_req_cnt);
	}
#endif
	ufshcd_vops_setup_xfer_req(hba, tag, (lrbp->cmd ? true : false));

	err = ufshcd_send_command(hba, tag);
	if (err) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		scsi_dma_unmap(lrbp->cmd);
		lrbp->cmd = NULL;
		clear_bit_unlock(tag, &hba->lrb_in_use);
		ufshcd_release_all(hba);
		ufshcd_vops_pm_qos_req_end(hba, cmd->request, true);
		dev_err(hba->dev, "%s: failed sending command, %d\n",
							__func__, err);
		err = DID_ERROR;
		goto out;
	}

out_unlock:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
out:
#if defined(CONFIG_UFSFEATURE) && defined(CONFIG_UFSHPB)
	if (!pre_req_err) {
		pre_cmd = add_lrbp->cmd;
		scsi_dma_unmap(pre_cmd);
		add_lrbp->cmd = NULL;
		clear_bit_unlock(add_tag, &hba->lrb_in_use);
                ufshcd_release_all(hba);
		ufshcd_vops_pm_qos_req_end(hba, pre_cmd->request, true);
		ufsf_hpb_end_pre_req(&hba->ufsf, pre_cmd->request);
	}
#endif
	if (has_read_lock)
		ufshcd_put_read_lock(hba);
	return err;
}

static int ufshcd_compose_dev_cmd(struct ufs_hba *hba,
		struct ufshcd_lrb *lrbp, enum dev_cmd_type cmd_type, int tag)
{
	lrbp->cmd = NULL;
	lrbp->sense_bufflen = 0;
	lrbp->sense_buffer = NULL;
	lrbp->task_tag = tag;
	lrbp->lun = 0; /* device management cmd is not specific to any LUN */
	lrbp->intr_cmd = true; /* No interrupt aggregation */
#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO)
	lrbp->crypto_enable = false; /* No crypto operations */
#endif
	hba->dev_cmd.type = cmd_type;

	return ufshcd_comp_devman_upiu(hba, lrbp);
}

static int
ufshcd_clear_cmd(struct ufs_hba *hba, int tag)
{
	int err = 0;
	unsigned long flags;
	u32 mask = 1 << tag;

	/* clear outstanding transaction before retry */
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_utrl_clear(hba, tag);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/*
	 * wait for for h/w to clear corresponding bit in door-bell.
	 * max. wait is 1 sec.
	 */
	err = ufshcd_wait_for_register(hba,
			REG_UTP_TRANSFER_REQ_DOOR_BELL,
			mask, ~mask, 1000, 1000, true);

	return err;
}

static int
ufshcd_check_query_response(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_query_res *query_res = &hba->dev_cmd.query.response;

	/* Get the UPIU response */
	query_res->response = ufshcd_get_rsp_upiu_result(lrbp->ucd_rsp_ptr) >>
				UPIU_RSP_CODE_OFFSET;
	return query_res->response;
}

/**
 * ufshcd_dev_cmd_completion() - handles device management command responses
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block
 */
static int
ufshcd_dev_cmd_completion(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	int resp;
	int err = 0;

	hba->ufs_stats.last_hibern8_exit_tstamp = ktime_set(0, 0);
	resp = ufshcd_get_req_rsp(lrbp->ucd_rsp_ptr);

	switch (resp) {
	case UPIU_TRANSACTION_NOP_IN:
		if (hba->dev_cmd.type != DEV_CMD_TYPE_NOP) {
			err = -EINVAL;
			dev_err(hba->dev, "%s: unexpected response %x\n",
					__func__, resp);
		}
		break;
	case UPIU_TRANSACTION_QUERY_RSP:
		err = ufshcd_check_query_response(hba, lrbp);
		if (!err)
			err = ufshcd_copy_query_response(hba, lrbp);
		break;
	case UPIU_TRANSACTION_REJECT_UPIU:
		/* TODO: handle Reject UPIU Response */
		err = -EPERM;
		dev_err(hba->dev, "%s: Reject UPIU not fully implemented\n",
				__func__);
		break;
	default:
		err = -EINVAL;
		dev_err(hba->dev, "%s: Invalid device management cmd response: %x\n",
				__func__, resp);
		break;
	}

	return err;
}

static int ufshcd_wait_for_dev_cmd(struct ufs_hba *hba,
		struct ufshcd_lrb *lrbp, int max_timeout)
{
	int err = 0;
	unsigned long time_left;
	unsigned long flags;

	time_left = wait_for_completion_timeout(hba->dev_cmd.complete,
			msecs_to_jiffies(max_timeout));

	/* Make sure descriptors are ready before ringing the doorbell */
	wmb();
	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->dev_cmd.complete = NULL;
	if (likely(time_left)) {
		err = ufshcd_get_tr_ocs(lrbp);
		if (!err)
			err = ufshcd_dev_cmd_completion(hba, lrbp);
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (!time_left) {
		err = -ETIMEDOUT;
		dev_dbg(hba->dev, "%s: dev_cmd request timedout, tag %d\n",
			__func__, lrbp->task_tag);
		if (!ufshcd_clear_cmd(hba, lrbp->task_tag))
			/* successfully cleared the command, retry if needed */
			err = -EAGAIN;
		/*
		 * in case of an error, after clearing the doorbell,
		 * we also need to clear the outstanding_request
		 * field in hba
		 */
		ufshcd_outstanding_req_clear(hba, lrbp->task_tag);
	}

	if (err)
		ufsdbg_set_err_state(hba);

	return err;
}

/**
 * ufshcd_get_dev_cmd_tag - Get device management command tag
 * @hba: per-adapter instance
 * @tag_out: pointer to variable with available slot value
 *
 * Get a free slot and lock it until device management command
 * completes.
 *
 * Returns false if free slot is unavailable for locking, else
 * return true with tag value in @tag.
 */
static bool ufshcd_get_dev_cmd_tag(struct ufs_hba *hba, int *tag_out)
{
	int tag;
	bool ret = false;
	unsigned long tmp;

	if (!tag_out)
		goto out;

	do {
		tmp = ~hba->lrb_in_use;
		tag = find_last_bit(&tmp, hba->nutrs);
		if (tag >= hba->nutrs)
			goto out;
	} while (test_and_set_bit_lock(tag, &hba->lrb_in_use));

	*tag_out = tag;
	ret = true;
out:
	return ret;
}

static inline void ufshcd_put_dev_cmd_tag(struct ufs_hba *hba, int tag)
{
	clear_bit_unlock(tag, &hba->lrb_in_use);
}

/**
 * ufshcd_exec_dev_cmd - API for sending device management requests
 * @hba: UFS hba
 * @cmd_type: specifies the type (NOP, Query...)
 * @timeout: time in seconds
 *
 * NOTE: Since there is only one available tag for device management commands,
 * it is expected you hold the hba->dev_cmd.lock mutex.
 */
#if defined(CONFIG_UFSFEATURE)
int ufshcd_exec_dev_cmd(struct ufs_hba *hba,
		enum dev_cmd_type cmd_type, int timeout)
#else
static int ufshcd_exec_dev_cmd(struct ufs_hba *hba,
		enum dev_cmd_type cmd_type, int timeout)
#endif
{
	struct ufshcd_lrb *lrbp;
	int err;
	int tag;
	struct completion wait;
	unsigned long flags;

	/*
	 * Get free slot, sleep if slots are unavailable.
	 * Even though we use wait_event() which sleeps indefinitely,
	 * the maximum wait time is bounded by SCSI request timeout.
	 */
	wait_event(hba->dev_cmd.tag_wq, ufshcd_get_dev_cmd_tag(hba, &tag));

	init_completion(&wait);
	lrbp = &hba->lrb[tag];
	WARN_ON(lrbp->cmd);
	err = ufshcd_compose_dev_cmd(hba, lrbp, cmd_type, tag);
	if (unlikely(err))
		goto out_put_tag;

	hba->dev_cmd.complete = &wait;

	ufshcd_add_query_upiu_trace(hba, tag, "query_send");
	/* Make sure descriptors are ready before ringing the doorbell */
	wmb();
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_vops_setup_xfer_req(hba, tag, (lrbp->cmd ? true : false));
	err = ufshcd_send_command(hba, tag);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (err) {
		dev_err(hba->dev, "%s: failed sending command, %d\n",
							__func__, err);
		goto out_put_tag;
	}
	err = ufshcd_wait_for_dev_cmd(hba, lrbp, timeout);

	ufshcd_add_query_upiu_trace(hba, tag,
			err ? "query_complete_err" : "query_complete");

out_put_tag:
	ufshcd_put_dev_cmd_tag(hba, tag);
	wake_up(&hba->dev_cmd.tag_wq);
#if defined(SEC_UFS_ERROR_COUNT)
	if (err && (cmd_type != DEV_CMD_TYPE_NOP))
		SEC_ufs_query_error_check(hba, cmd_type);
#endif
	return err;
}

/**
 * ufshcd_init_query() - init the query response and request parameters
 * @hba: per-adapter instance
 * @request: address of the request pointer to be initialized
 * @response: address of the response pointer to be initialized
 * @opcode: operation to perform
 * @idn: flag idn to access
 * @index: LU number to access
 * @selector: query/flag/descriptor further identification
 */
static inline void ufshcd_init_query(struct ufs_hba *hba,
		struct ufs_query_req **request, struct ufs_query_res **response,
		enum query_opcode opcode, u8 idn, u8 index, u8 selector)
{
	int idn_t = (int)idn;

	ufsdbg_error_inject_dispatcher(hba,
		ERR_INJECT_QUERY, idn_t, (int *)&idn_t);
	idn = idn_t;

	*request = &hba->dev_cmd.query.request;
	*response = &hba->dev_cmd.query.response;
	memset(*request, 0, sizeof(struct ufs_query_req));
	memset(*response, 0, sizeof(struct ufs_query_res));
	(*request)->upiu_req.opcode = opcode;
	(*request)->upiu_req.idn = idn;
	(*request)->upiu_req.index = index;
	(*request)->upiu_req.selector = selector;

	ufshcd_update_query_stats(hba, opcode, idn);
}

#if defined(CONFIG_UFSFEATURE)
int ufshcd_query_flag_retry(struct ufs_hba *hba, enum query_opcode opcode,
                            enum flag_idn idn, bool *flag_res)
#else
static int ufshcd_query_flag_retry(struct ufs_hba *hba,
	enum query_opcode opcode, enum flag_idn idn, bool *flag_res)
#endif
{
	int ret;
	int retries;

	for (retries = 0; retries < QUERY_REQ_RETRIES; retries++) {
		ret = ufshcd_query_flag(hba, opcode, idn, flag_res);
		if (ret)
			dev_dbg(hba->dev,
				"%s: failed with error %d, retries %d\n",
				__func__, ret, retries);
		else
			break;
	}

	if (ret)
		dev_err(hba->dev,
			"%s: query attribute, opcode %d, idn %d, failed with error %d after %d retires\n",
			__func__, opcode, idn, ret, retries);
	return ret;
}

/**
 * ufshcd_query_flag() - API function for sending flag query requests
 * @hba: per-adapter instance
 * @opcode: flag query to perform
 * @idn: flag idn to access
 * @flag_res: the flag value after the query request completes
 *
 * Returns 0 for success, non-zero in case of failure
 */
int ufshcd_query_flag(struct ufs_hba *hba, enum query_opcode opcode,
			enum flag_idn idn, bool *flag_res)
{
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	int err, index = 0, selector = 0;
	int timeout = QUERY_REQ_TIMEOUT;
	bool has_read_lock = false;

	BUG_ON(!hba);

	ufshcd_hold_all(hba);
	if (!ufshcd_is_shutdown_ongoing(hba) && !ufshcd_eh_in_progress(hba)) {
		down_read(&hba->lock);
		has_read_lock = true;
	}
	mutex_lock(&hba->dev_cmd.lock);
	ufshcd_init_query(hba, &request, &response, opcode, idn, index,
			selector);

	switch (opcode) {
	case UPIU_QUERY_OPCODE_SET_FLAG:
	case UPIU_QUERY_OPCODE_CLEAR_FLAG:
	case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_WRITE_REQUEST;
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_READ_REQUEST;
		if (!flag_res) {
			/* No dummy reads */
			dev_err(hba->dev, "%s: Invalid argument for read request\n",
					__func__);
			err = -EINVAL;
			goto out_unlock;
		}
		break;
	default:
		dev_err(hba->dev,
			"%s: Expected query flag opcode but got = %d\n",
			__func__, opcode);
		err = -EINVAL;
		goto out_unlock;
	}

	err = ufshcd_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, timeout);

	if (err) {
		dev_err(hba->dev,
			"%s: Sending flag query for idn %d failed, err = %d\n",
			__func__, request->upiu_req.idn, err);
		goto out_unlock;
	}

	if (flag_res)
		*flag_res = (be32_to_cpu(response->upiu_res.value) &
				MASK_QUERY_UPIU_FLAG_LOC) & 0x1;

out_unlock:
	mutex_unlock(&hba->dev_cmd.lock);
	if (has_read_lock)
		up_read(&hba->lock);
	ufshcd_release_all(hba);
	return err;
}

/**
 * ufshcd_query_attr - API function for sending attribute requests
 * @hba: per-adapter instance
 * @opcode: attribute opcode
 * @idn: attribute idn to access
 * @index: index field
 * @selector: selector field
 * @attr_val: the attribute value after the query request completes
 *
 * Returns 0 for success, non-zero in case of failure
*/
int ufshcd_query_attr(struct ufs_hba *hba, enum query_opcode opcode,
		      enum attr_idn idn, u8 index, u8 selector, u32 *attr_val)
{
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	int err;
	bool has_read_lock = false;

	BUG_ON(!hba);

	ufshcd_hold_all(hba);
	if (!attr_val) {
		dev_err(hba->dev, "%s: attribute value required for opcode 0x%x\n",
				__func__, opcode);
		err = -EINVAL;
		goto out;
	}

	/*
	 * May get invoked from shutdown and IOCTL contexts.
	 * In shutdown context, it comes in with lock acquired.
	 * In error recovery context, it may come with lock acquired.
	 */
	if (!ufshcd_is_shutdown_ongoing(hba) && !ufshcd_eh_in_progress(hba)) {
		down_read(&hba->lock);
		has_read_lock = true;
	}
	mutex_lock(&hba->dev_cmd.lock);
	ufshcd_init_query(hba, &request, &response, opcode, idn, index,
			selector);

	switch (opcode) {
	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_WRITE_REQUEST;
		request->upiu_req.value = cpu_to_be32(*attr_val);
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_READ_REQUEST;
		break;
	default:
		dev_err(hba->dev, "%s: Expected query attr opcode but got = 0x%.2x\n",
				__func__, opcode);
		err = -EINVAL;
		goto out_unlock;
	}

	err = ufshcd_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, QUERY_REQ_TIMEOUT);

	if (err) {
		dev_err(hba->dev, "%s: opcode 0x%.2x for idn %d failed, index %d, err = %d\n",
				__func__, opcode,
				request->upiu_req.idn, index, err);
		goto out_unlock;
	}

	*attr_val = be32_to_cpu(response->upiu_res.value);

out_unlock:
	mutex_unlock(&hba->dev_cmd.lock);
	if (has_read_lock)
		up_read(&hba->lock);
out:
	ufshcd_release_all(hba);
	return err;
}

/**
 * ufshcd_query_attr_retry() - API function for sending query
 * attribute with retries
 * @hba: per-adapter instance
 * @opcode: attribute opcode
 * @idn: attribute idn to access
 * @index: index field
 * @selector: selector field
 * @attr_val: the attribute value after the query request
 * completes
 *
 * Returns 0 for success, non-zero in case of failure
*/
static int ufshcd_query_attr_retry(struct ufs_hba *hba,
	enum query_opcode opcode, enum attr_idn idn, u8 index, u8 selector,
	u32 *attr_val)
{
	int ret = 0;
	u32 retries;

	 for (retries = QUERY_REQ_RETRIES; retries > 0; retries--) {
		ret = ufshcd_query_attr(hba, opcode, idn, index,
						selector, attr_val);
		if (ret)
			dev_dbg(hba->dev, "%s: failed with error %d, retries %d\n",
				__func__, ret, retries);
		else
			break;
	}

	if (ret)
		dev_err(hba->dev,
			"%s: query attribute, idn %d, failed with error %d after %d retires\n",
			__func__, idn, ret, retries);
	return ret;
}

static int __ufshcd_query_descriptor(struct ufs_hba *hba,
			enum query_opcode opcode, enum desc_idn idn, u8 index,
			u8 selector, u8 *desc_buf, int *buf_len)
{
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	int err;
	bool has_read_lock = false;

	BUG_ON(!hba);

	ufshcd_hold_all(hba);
	if (!desc_buf) {
		dev_err(hba->dev, "%s: descriptor buffer required for opcode 0x%x\n",
				__func__, opcode);
		err = -EINVAL;
		goto out;
	}

	if (*buf_len < QUERY_DESC_MIN_SIZE || *buf_len > QUERY_DESC_MAX_SIZE) {
		dev_err(hba->dev, "%s: descriptor buffer size (%d) is out of range\n",
				__func__, *buf_len);
		err = -EINVAL;
		goto out;
	}

	if (!ufshcd_is_shutdown_ongoing(hba) && !ufshcd_eh_in_progress(hba)) {
		down_read(&hba->lock);
		has_read_lock = true;
	}
	mutex_lock(&hba->dev_cmd.lock);
	ufshcd_init_query(hba, &request, &response, opcode, idn, index,
			selector);
	hba->dev_cmd.query.descriptor = desc_buf;
	request->upiu_req.length = cpu_to_be16(*buf_len);

	switch (opcode) {
	case UPIU_QUERY_OPCODE_WRITE_DESC:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_WRITE_REQUEST;
		break;
	case UPIU_QUERY_OPCODE_READ_DESC:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_READ_REQUEST;
		break;
	default:
		dev_err(hba->dev,
				"%s: Expected query descriptor opcode but got = 0x%.2x\n",
				__func__, opcode);
		err = -EINVAL;
		goto out_unlock;
	}

	err = ufshcd_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, QUERY_REQ_TIMEOUT);

	if (err) {
		dev_err(hba->dev, "%s: opcode 0x%.2x for idn %d failed, index %d, err = %d\n",
				__func__, opcode,
				request->upiu_req.idn, index, err);
		goto out_unlock;
	}

	*buf_len = be16_to_cpu(response->upiu_res.length);

out_unlock:
	hba->dev_cmd.query.descriptor = NULL;
	mutex_unlock(&hba->dev_cmd.lock);
	if (has_read_lock)
		up_read(&hba->lock);
out:
	ufshcd_release_all(hba);
	return err;
}

/**
 * ufshcd_query_descriptor_retry - API function for sending descriptor requests
 * @hba: per-adapter instance
 * @opcode: attribute opcode
 * @idn: attribute idn to access
 * @index: index field
 * @selector: selector field
 * @desc_buf: the buffer that contains the descriptor
 * @buf_len: length parameter passed to the device
 *
 * Returns 0 for success, non-zero in case of failure.
 * The buf_len parameter will contain, on return, the length parameter
 * received on the response.
 */
int ufshcd_query_descriptor_retry(struct ufs_hba *hba,
				  enum query_opcode opcode,
				  enum desc_idn idn, u8 index,
				  u8 selector,
				  u8 *desc_buf, int *buf_len)
{
	int err;
	int retries;

	for (retries = QUERY_REQ_RETRIES; retries > 0; retries--) {
		err = __ufshcd_query_descriptor(hba, opcode, idn, index,
						selector, desc_buf, buf_len);
		if (!err || err == -EINVAL)
			break;
	}

	return err;
}
EXPORT_SYMBOL(ufshcd_query_descriptor_retry);

/**
 * ufshcd_read_desc_length - read the specified descriptor length from header
 * @hba: Pointer to adapter instance
 * @desc_id: descriptor idn value
 * @desc_index: descriptor index
 * @desc_length: pointer to variable to read the length of descriptor
 *
 * Return 0 in case of success, non-zero otherwise
 */
static int ufshcd_read_desc_length(struct ufs_hba *hba,
	enum desc_idn desc_id,
	int desc_index,
	int *desc_length)
{
	int ret;
	u8 header[QUERY_DESC_HDR_SIZE];
	int header_len = QUERY_DESC_HDR_SIZE;

	if (desc_id >= QUERY_DESC_IDN_MAX)
		return -EINVAL;

	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					desc_id, desc_index, 0, header,
					&header_len);

	if (ret) {
		dev_err(hba->dev, "%s: Failed to get descriptor header id %d",
			__func__, desc_id);
		return ret;
	} else if (desc_id != header[QUERY_DESC_DESC_TYPE_OFFSET]) {
		dev_warn(hba->dev, "%s: descriptor header id %d and desc_id %d mismatch",
			__func__, header[QUERY_DESC_DESC_TYPE_OFFSET],
			desc_id);
		ret = -EINVAL;
	}

	*desc_length = header[QUERY_DESC_LENGTH_OFFSET];
	return ret;

}

/**
 * ufshcd_map_desc_id_to_length - map descriptor IDN to its length
 * @hba: Pointer to adapter instance
 * @desc_id: descriptor idn value
 * @desc_len: mapped desc length (out)
 *
 * Return 0 in case of success, non-zero otherwise
 */
int ufshcd_map_desc_id_to_length(struct ufs_hba *hba,
	enum desc_idn desc_id, int *desc_len)
{
	switch (desc_id) {
	case QUERY_DESC_IDN_DEVICE:
		*desc_len = hba->desc_size.dev_desc;
		break;
	case QUERY_DESC_IDN_POWER:
		*desc_len = hba->desc_size.pwr_desc;
		break;
	case QUERY_DESC_IDN_GEOMETRY:
		*desc_len = hba->desc_size.geom_desc;
		break;
	case QUERY_DESC_IDN_CONFIGURATION:
		*desc_len = hba->desc_size.conf_desc;
		break;
	case QUERY_DESC_IDN_UNIT:
		*desc_len = hba->desc_size.unit_desc;
		break;
	case QUERY_DESC_IDN_INTERCONNECT:
		*desc_len = hba->desc_size.interc_desc;
		break;
	case QUERY_DESC_IDN_STRING:
		*desc_len = QUERY_DESC_MAX_SIZE;
		break;
	case QUERY_DESC_IDN_HEALTH:
		*desc_len = hba->desc_size.hlth_desc;
		break;
	case QUERY_DESC_IDN_RFU_0:
	case QUERY_DESC_IDN_RFU_1:
		*desc_len = 0;
		break;
	default:
		*desc_len = 0;
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(ufshcd_map_desc_id_to_length);

/**
 * ufshcd_read_desc_param - read the specified descriptor parameter
 * @hba: Pointer to adapter instance
 * @desc_id: descriptor idn value
 * @desc_index: descriptor index
 * @param_offset: offset of the parameter to read
 * @param_read_buf: pointer to buffer where parameter would be read
 * @param_size: sizeof(param_read_buf)
 *
 * Return 0 in case of success, non-zero otherwise
 */
int ufshcd_read_desc_param(struct ufs_hba *hba,
			   enum desc_idn desc_id,
			   int desc_index,
			   u8 param_offset,
			   u8 *param_read_buf,
			   u8 param_size)
{
	int ret;
	u8 *desc_buf;
	int buff_len;
	bool is_kmalloc = true;

	/* Safety check */
	if (desc_id >= QUERY_DESC_IDN_MAX || !param_size)
		return -EINVAL;

	/* Get the max length of descriptor from structure filled up at probe
	 * time.
	 */
	ret = ufshcd_map_desc_id_to_length(hba, desc_id, &buff_len);

	/* Sanity checks */
	if (ret || !buff_len) {
		dev_err(hba->dev, "%s: Failed to get full descriptor length",
			__func__);
		return ret;
	}

	/* Check param offset and size what we need is less than buff_len */
	if ((param_offset + param_size) > buff_len) {
		dev_err(hba->dev, "%s: Invalid offset 0x%x(size 0x%x) in descriptor IDN 0x%x, length 0x%x\n",
				__func__, param_offset, param_size, desc_id, buff_len);
		return -EINVAL;
	}

	/* Check whether we need temp memory */
	if (param_offset != 0 || param_size < buff_len) {
		desc_buf = kmalloc(buff_len, GFP_KERNEL);
		if (!desc_buf)
			return -ENOMEM;
	} else {
		desc_buf = param_read_buf;
		is_kmalloc = false;
	}

	/* Request for full descriptor */
	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					desc_id, desc_index, 0,
					desc_buf, &buff_len);

	if (ret) {
		dev_err(hba->dev, "%s: Failed reading descriptor. desc_id %d, desc_index %d, param_offset %d, ret %d",
			__func__, desc_id, desc_index, param_offset, ret);
		goto out;
	}

	/* Sanity check */
	if (desc_buf[QUERY_DESC_DESC_TYPE_OFFSET] != desc_id) {
		dev_err(hba->dev, "%s: invalid desc_id %d in descriptor header",
			__func__, desc_buf[QUERY_DESC_DESC_TYPE_OFFSET]);
		ret = -EINVAL;
		goto out;
	}

	/* Check wherher we will not copy more data, than available */
	if (is_kmalloc && param_size > buff_len)
		param_size = buff_len;

	if (is_kmalloc)
		memcpy(param_read_buf, &desc_buf[param_offset], param_size);
out:
	if (is_kmalloc)
		kfree(desc_buf);
	return ret;
}

static inline int ufshcd_read_desc(struct ufs_hba *hba,
				   enum desc_idn desc_id,
				   int desc_index,
				   u8 *buf,
				   u32 size)
{
	return ufshcd_read_desc_param(hba, desc_id, desc_index, 0, buf, size);
}

static inline int ufshcd_read_power_desc(struct ufs_hba *hba,
					 u8 *buf,
					 u32 size)
{
	return ufshcd_read_desc(hba, QUERY_DESC_IDN_POWER, 0, buf, size);
}

int ufshcd_read_device_desc(struct ufs_hba *hba, u8 *buf, u32 size)
{
	return ufshcd_read_desc(hba, QUERY_DESC_IDN_DEVICE, 0, buf, size);
}

/**
 * ufshcd_read_string_desc - read string descriptor
 * @hba: pointer to adapter instance
 * @desc_index: descriptor index
 * @buf: pointer to buffer where descriptor would be read
 * @size: size of buf
 * @ascii: if true convert from unicode to ascii characters
 *
 * Return 0 in case of success, non-zero otherwise
 */
int ufshcd_read_string_desc(struct ufs_hba *hba, int desc_index,
			    u8 *buf, u32 size, bool ascii)
{
	int err = 0;

	err = ufshcd_read_desc(hba,
				QUERY_DESC_IDN_STRING, desc_index, buf, size);

	if (err) {
		dev_err(hba->dev, "%s: reading String Desc failed after %d retries. err = %d\n",
			__func__, QUERY_REQ_RETRIES, err);
		goto out;
	}

	if (ascii) {
		int desc_len;
		int ascii_len;
		int i;
		char *buff_ascii;

		desc_len = buf[0];
		/* remove header and divide by 2 to move from UTF16 to UTF8 */
		ascii_len = (desc_len - QUERY_DESC_HDR_SIZE) / 2 + 1;
		if (size < ascii_len + QUERY_DESC_HDR_SIZE) {
			dev_err(hba->dev, "%s: buffer allocated size is too small\n",
					__func__);
			err = -ENOMEM;
			goto out;
		}

		buff_ascii = kzalloc(ascii_len, GFP_KERNEL);
		if (!buff_ascii) {
			err = -ENOMEM;
			goto out;
		}

		/*
		 * the descriptor contains string in UTF16 format
		 * we need to convert to utf-8 so it can be displayed
		 */
		utf16s_to_utf8s((wchar_t *)&buf[QUERY_DESC_HDR_SIZE],
				desc_len - QUERY_DESC_HDR_SIZE,
				UTF16_BIG_ENDIAN, buff_ascii, ascii_len);

		/* replace non-printable or non-ASCII characters with spaces */
		for (i = 0; i < ascii_len; i++)
			ufshcd_remove_non_printable(&buff_ascii[i]);

		memset(buf + QUERY_DESC_HDR_SIZE, 0,
				size - QUERY_DESC_HDR_SIZE);
		memcpy(buf + QUERY_DESC_HDR_SIZE, buff_ascii, ascii_len);
		buf[QUERY_DESC_LENGTH_OFFSET] = ascii_len + QUERY_DESC_HDR_SIZE;
		kfree(buff_ascii);
	}
out:
	return err;
}

/**
 * ufshcd_read_unit_desc_param - read the specified unit descriptor parameter
 * @hba: Pointer to adapter instance
 * @lun: lun id
 * @param_offset: offset of the parameter to read
 * @param_read_buf: pointer to buffer where parameter would be read
 * @param_size: sizeof(param_read_buf)
 *
 * Return 0 in case of success, non-zero otherwise
 */
static inline int ufshcd_read_unit_desc_param(struct ufs_hba *hba,
					      int lun,
					      enum unit_desc_param param_offset,
					      u8 *param_read_buf,
					      u32 param_size)
{
	/*
	 * Unit descriptors are only available for general purpose LUs (LUN id
	 * from 0 to 7) and RPMB Well known LU.
	 */
	if (!ufs_is_valid_unit_desc_lun(lun))
		return -EOPNOTSUPP;

	return ufshcd_read_desc_param(hba, QUERY_DESC_IDN_UNIT, lun,
				      param_offset, param_read_buf, param_size);
}

static int __ufshcd_query_vendor_func(struct ufs_hba *hba,
		u8 query_fn, enum query_opcode opcode, enum desc_idn idn, u8 index,
		u8 selector, u8 *desc_buf, int *buf_len)
{
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	int err;

	BUG_ON(!hba);

	pm_runtime_get_sync(hba->dev);
	ufshcd_hold_all(hba);
	if (!desc_buf) {
		dev_err(hba->dev, "%s: descriptor buffer required for opcode 0x%x\n",
				__func__, opcode);
		err = -EINVAL;
		goto out;
	}

	mutex_lock(&hba->dev_cmd.lock);

	request = &hba->dev_cmd.query.request;
	response = &hba->dev_cmd.query.response;
	memset(request, 0, sizeof(struct ufs_query_req));
	memset(response, 0, sizeof(struct ufs_query_res));

	request->query_func = query_fn;

	request->upiu_req.opcode = opcode;
	request->upiu_req.idn = idn;
	request->upiu_req.index = index;
	request->upiu_req.selector = selector;

	hba->dev_cmd.query.descriptor = desc_buf;
	request->upiu_req.length = cpu_to_be16(*buf_len);

	err = ufshcd_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, QUERY_REQ_TIMEOUT);

	if (err) {
		dev_err(hba->dev, "%s: opcode 0x%.2x for idn %d failed, index %d, err = %d\n",
				__func__, opcode,
				request->upiu_req.idn, index, err);
		goto out_unlock;
	}

	hba->dev_cmd.query.descriptor = NULL;
	*buf_len = be16_to_cpu(response->upiu_res.length);

out_unlock:
	mutex_unlock(&hba->dev_cmd.lock);
out:
	ufshcd_release_all(hba);
	pm_runtime_put_sync(hba->dev);
	return err;
}

/**
 * ufshcd_vendor_specific_func - Vendor Specific Read/Write Functions
 *
 * @hba: pointer to adapter instance
 * @buf: pointer to buffer where read/write function
 * @size: size of buf
 *
 * Return 0 in case of success, non-zero otherwise
 */
int ufshcd_vendor_specific_func(struct ufs_hba *hba, u8 query_fn,
		enum query_opcode opcode, enum desc_idn idn, u8 index, u8 selector,
		u8 *buf, u32 *size)
{
	int err = 0;
	int retries;

	for (retries = QUERY_REQ_RETRIES; retries > 0; retries--) {
		err = __ufshcd_query_vendor_func(hba, query_fn, opcode, idn, index,
				selector, buf, size);
		if (!err || err == -EINVAL)
			break;

		dev_dbg(hba->dev, "%s: error %d retrying\n", __func__, err);
	}

	if (err)
		dev_err(hba->dev,
				"%s is failed!! Query Function=0x%x err = %d\n",
				__func__, query_fn, err);
	else
		dev_err(hba->dev,
				"%s is OK!! Query Function=0x%x err = %d\n",
				__func__, query_fn, err);

	return err;
}

static int UFS_Toshiba_query_fatal_mode(struct ufs_hba *hba)
{
	u8 dbuf[512] = {0, };
	int dsize = sizeof(dbuf);
	int result = 0;

	/*enter fatal mode*/
	dbuf[0] = 0x0C, dbuf[4] = 0xA2, dbuf[5] = 0xA0, dbuf[6] = 0x6A, dbuf[7] = 0x04;
	result = ufshcd_vendor_specific_func(hba,
			UPIU_QUERY_FUNC_VENDOR_TOSHIBA_FATALMODE,
			0, 0, 0, 0, dbuf, &dsize);
	if (result)
		dev_err(hba->dev, "%s: failed to enter Toshiba K2 fatal mode. result = %d\n",
				__func__, result);
	else
		hba->UFS_fatal_mode_done = true;

	return result;
}

#ifdef CONFIG_BLK_TURBO_WRITE
/* call back for block layer */
void ufshcd_tw_ctrl(struct scsi_device *sdev, int en)
{
	int err;
	struct ufs_hba *hba;
	bool has_lock = false;

	hba = shost_priv(sdev->host);

	mutex_lock(&hba->tw_ctrl_mutex);
	has_lock = true;

	pr_err("UFS: try TW %s.\n", en ? "On" : "Off");

	if (!hba->support_tw)
		goto out;

	if (hba->pm_op_in_progress) {
		dev_err(hba->dev, "%s: tw ctrl during pm operation is not allowed.\n",
				__func__);
		goto out;
	}

	if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL) {
		dev_err(hba->dev, "%s: ufs host is not available.\n",
				__func__);
		goto out;
	}

	if (hba->tw_state_not_allowed) {
		dev_err(hba->dev, "%s: tw ctrl is not allowed(e.g. ufs driver is suspended).\n",
				__func__);
		goto out;
	}

	if (ufshcd_is_tw_err(hba))
		dev_err(hba->dev, "%s: previous turbo write control was failed.\n",
				__func__);

	if (en) {
		if (ufshcd_is_tw_on(hba)) {
			dev_err(hba->dev, "%s: turbo write already enabled. tw_state = %d\n",
					__func__, hba->ufs_tw_state);
			goto out;
		}
		pm_runtime_get_sync(hba->dev);
		if (hba->tw_state_not_allowed) {	// check again
			dev_err(hba->dev, "%s: tw ctrl %s is not allowed(e.g. ufs driver is suspended)\n",
					__func__, en ? "On" : "Off");
			goto out_rpm_put;
		}

		err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG,
				QUERY_FLAG_IDN_WB_EN, NULL);
		if (err) {
			ufshcd_set_tw_err(hba);
			dev_err(hba->dev, "%s: enable turbo write failed. err = %d\n",
					__func__, err);
		} else {
			ufshcd_set_tw_on(hba);
			dev_info(hba->dev, "%s: ufs turbo write enabled \n", __func__);
		}
	} else {
		if (ufshcd_is_tw_off(hba)) {
			dev_err(hba->dev, "%s: turbo write already disabled. tw_state = %d\n",
					__func__, hba->ufs_tw_state);
			goto out;
		}
		pm_runtime_get_sync(hba->dev);
		if (hba->tw_state_not_allowed) {	// check again
			dev_err(hba->dev, "%s: tw ctrl %s is not allowed(e.g. ufs driver is suspended)\n",
					__func__, en ? "On" : "Off");
			goto out_rpm_put;
		}

		err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG,
				QUERY_FLAG_IDN_WB_EN, NULL);
		if (err) {
			ufshcd_set_tw_err(hba);
			dev_err(hba->dev, "%s: disable turbo write failed. err = %d\n",
					__func__, err);
		} else {
			ufshcd_set_tw_off(hba);
			dev_info(hba->dev, "%s: ufs turbo write disabled \n", __func__);
		}
	}
	SEC_ufs_update_tw_info(hba, 0);
out_rpm_put:
	/*
	 * tw_ctrl_mutex must be unlocked before the pm_runtime_put_sync() is called
	 * to prevent deadlock issue.
	 *
	 * ufshcd_suspend() calls mutex_lock(&hba->tw_ctrl_mutex)
	 */
	mutex_unlock(&hba->tw_ctrl_mutex);
	has_lock = false;
	pm_runtime_put_sync(hba->dev);
out:
	if (has_lock)
		mutex_unlock(&hba->tw_ctrl_mutex);
}

static void ufshcd_reset_tw(struct ufs_hba *hba, bool force)
{
	int err = 0;

	if (!hba->support_tw)
		return;

	if (ufshcd_is_tw_off(hba)) {
		dev_info(hba->dev, "%s: turbo write already disabled. tw_state = %d\n",
				__func__, hba->ufs_tw_state);
		return;
	}

	if (ufshcd_is_tw_err(hba))
		dev_err(hba->dev, "%s: previous turbo write control was failed.\n",
				__func__);

	if (force)
		err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG,
				QUERY_FLAG_IDN_WB_EN, NULL);

	if (err) {
		ufshcd_set_tw_err(hba);
		dev_err(hba->dev, "%s: disable turbo write failed. err = %d\n",
				__func__, err);
	} else {
		ufshcd_set_tw_off(hba);
		dev_info(hba->dev, "%s: ufs turbo write disabled \n", __func__);
	}

	SEC_ufs_update_tw_info(hba, 0);
#ifdef CONFIG_BLK_TURBO_WRITE
	scsi_reset_tw_state(hba->host);
#endif
}

#ifdef CONFIG_SCSI_UFS_SUPPORT_TW_MAN_GC
static int ufshcd_get_tw_buf_status(struct ufs_hba *hba, u32 *status)
{
	return ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_AVAIL_WB_BUFF_SIZE, 0, 0, status);
}

static int ufshcd_tw_manual_flush_ctrl(struct ufs_hba *hba, int en)
{
	int err = 0;

	dev_info(hba->dev, "%s: %sable turbo write manual flush\n",
			__func__, en ? "en" : "dis");
	if (en) {
		err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG,
				QUERY_FLAG_IDN_WB_BUFF_FLUSH_EN, NULL);
		if (err)
			dev_err(hba->dev, "%s: enable turbo write failed. err = %d\n",
					__func__, err);
	} else {
		err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG,
				QUERY_FLAG_IDN_WB_BUFF_FLUSH_EN, NULL);
		if (err)
			dev_err(hba->dev, "%s: disable turbo write failed. err = %d\n",
					__func__, err);
	}

	return err;
}

static int ufshcd_tw_flush_ctrl(struct ufs_hba *hba)
{
	int err = 0;
	u32 curr_status = 0;

	err = ufshcd_get_tw_buf_status(hba, &curr_status);

	if (!err && (curr_status <= UFS_TW_MANUAL_FLUSH_THRESHOLD)) {
		dev_info(hba->dev, "%s: enable tw manual flush, buf status : %d\n",
				__func__, curr_status);
		ufshcd_scsi_block_requests(hba->host);
		err = ufshcd_tw_manual_flush_ctrl(hba, 1);
		if (!err) {
			mdelay(100);
			err = ufshcd_tw_manual_flush_ctrl(hba, 0);
			if (err)
				dev_err(hba->dev, "%s: disable tw manual flush failed. err = %d\n",
						__func__, err);
		} else
			dev_err(hba->dev, "%s: enable tw manual flush failed. err = %d\n",
					__func__, err);
		ufshcd_scsi_unblock_requests(hba->host);
	}
	return err;
}
#endif
#endif // CONFIG_BLK_TURBO_WRITE

/**
 * ufshcd_memory_alloc - allocate memory for host memory space data structures
 * @hba: per adapter instance
 *
 * 1. Allocate DMA memory for Command Descriptor array
 *	Each command descriptor consist of Command UPIU, Response UPIU and PRDT
 * 2. Allocate DMA memory for UTP Transfer Request Descriptor List (UTRDL).
 * 3. Allocate DMA memory for UTP Task Management Request Descriptor List
 *	(UTMRDL)
 * 4. Allocate memory for local reference block(lrb).
 *
 * Returns 0 for success, non-zero in case of failure
 */
static int ufshcd_memory_alloc(struct ufs_hba *hba)
{
	size_t utmrdl_size, utrdl_size, ucdl_size;

	/* Allocate memory for UTP command descriptors */
	ucdl_size = (sizeof_utp_transfer_cmd_desc(hba) * hba->nutrs);
	hba->ucdl_base_addr = dmam_alloc_coherent(hba->dev,
						  ucdl_size,
						  &hba->ucdl_dma_addr,
						  GFP_KERNEL);

	/*
	 * UFSHCI requires UTP command descriptor to be 128 byte aligned.
	 * make sure hba->ucdl_dma_addr is aligned to PAGE_SIZE
	 * if hba->ucdl_dma_addr is aligned to PAGE_SIZE, then it will
	 * be aligned to 128 bytes as well
	 */
	if (!hba->ucdl_base_addr ||
	    WARN_ON(hba->ucdl_dma_addr & (PAGE_SIZE - 1))) {
		dev_err(hba->dev,
			"Command Descriptor Memory allocation failed\n");
		goto out;
	}

	/*
	 * Allocate memory for UTP Transfer descriptors
	 * UFSHCI requires 1024 byte alignment of UTRD
	 */
	utrdl_size = (sizeof(struct utp_transfer_req_desc) * hba->nutrs);
	hba->utrdl_base_addr = dmam_alloc_coherent(hba->dev,
						   utrdl_size,
						   &hba->utrdl_dma_addr,
						   GFP_KERNEL);
	if (!hba->utrdl_base_addr ||
	    WARN_ON(hba->utrdl_dma_addr & (PAGE_SIZE - 1))) {
		dev_err(hba->dev,
			"Transfer Descriptor Memory allocation failed\n");
		goto out;
	}

	/*
	 * Allocate memory for UTP Task Management descriptors
	 * UFSHCI requires 1024 byte alignment of UTMRD
	 */
	utmrdl_size = sizeof(struct utp_task_req_desc) * hba->nutmrs;
	hba->utmrdl_base_addr = dmam_alloc_coherent(hba->dev,
						    utmrdl_size,
						    &hba->utmrdl_dma_addr,
						    GFP_KERNEL);
	if (!hba->utmrdl_base_addr ||
	    WARN_ON(hba->utmrdl_dma_addr & (PAGE_SIZE - 1))) {
		dev_err(hba->dev,
		"Task Management Descriptor Memory allocation failed\n");
		goto out;
	}

	/* Allocate memory for local reference block */
	hba->lrb = devm_kcalloc(hba->dev,
				hba->nutrs, sizeof(struct ufshcd_lrb),
				GFP_KERNEL);
	if (!hba->lrb) {
		dev_err(hba->dev, "LRB Memory allocation failed\n");
		goto out;
	}
	return 0;
out:
	return -ENOMEM;
}

/**
 * ufshcd_host_memory_configure - configure local reference block with
 *				memory offsets
 * @hba: per adapter instance
 *
 * Configure Host memory space
 * 1. Update Corresponding UTRD.UCDBA and UTRD.UCDBAU with UCD DMA
 * address.
 * 2. Update each UTRD with Response UPIU offset, Response UPIU length
 * and PRDT offset.
 * 3. Save the corresponding addresses of UTRD, UCD.CMD, UCD.RSP and UCD.PRDT
 * into local reference block.
 */
static void ufshcd_host_memory_configure(struct ufs_hba *hba)
{
	struct utp_transfer_cmd_desc *cmd_descp;
	struct utp_transfer_req_desc *utrdlp;
	dma_addr_t cmd_desc_dma_addr;
	dma_addr_t cmd_desc_element_addr;
	u16 response_offset;
	u16 prdt_offset;
	int cmd_desc_size;
	int i;

	utrdlp = hba->utrdl_base_addr;
	cmd_descp = hba->ucdl_base_addr;

	response_offset =
		offsetof(struct utp_transfer_cmd_desc, response_upiu);
	prdt_offset =
		offsetof(struct utp_transfer_cmd_desc, prd_table);

	cmd_desc_size = sizeof_utp_transfer_cmd_desc(hba);
	cmd_desc_dma_addr = hba->ucdl_dma_addr;

	for (i = 0; i < hba->nutrs; i++) {
		/* Configure UTRD with command descriptor base address */
		cmd_desc_element_addr =
				(cmd_desc_dma_addr + (cmd_desc_size * i));
		utrdlp[i].command_desc_base_addr_lo =
				cpu_to_le32(lower_32_bits(cmd_desc_element_addr));
		utrdlp[i].command_desc_base_addr_hi =
				cpu_to_le32(upper_32_bits(cmd_desc_element_addr));

		/* Response upiu and prdt offset should be in double words */
		if (hba->quirks & UFSHCD_QUIRK_PRDT_BYTE_GRAN) {
			utrdlp[i].response_upiu_offset =
				cpu_to_le16(response_offset);
			utrdlp[i].prd_table_offset =
				cpu_to_le16(prdt_offset);
			utrdlp[i].response_upiu_length =
				cpu_to_le16(ALIGNED_UPIU_SIZE);
		} else {
			utrdlp[i].response_upiu_offset =
				cpu_to_le16((response_offset >> 2));
			utrdlp[i].prd_table_offset =
				cpu_to_le16((prdt_offset >> 2));
			utrdlp[i].response_upiu_length =
				cpu_to_le16(ALIGNED_UPIU_SIZE >> 2);
		}

		hba->lrb[i].utr_descriptor_ptr = (utrdlp + i);
		hba->lrb[i].utrd_dma_addr = hba->utrdl_dma_addr +
				(i * sizeof(struct utp_transfer_req_desc));
		hba->lrb[i].ucd_req_ptr = (struct utp_upiu_req *)cmd_descp;
		hba->lrb[i].ucd_req_dma_addr = cmd_desc_element_addr;
		hba->lrb[i].ucd_rsp_ptr =
			(struct utp_upiu_rsp *)cmd_descp->response_upiu;
		hba->lrb[i].ucd_rsp_dma_addr = cmd_desc_element_addr +
				response_offset;
		hba->lrb[i].ucd_prdt_ptr =
			(struct ufshcd_sg_entry *)cmd_descp->prd_table;
		hba->lrb[i].ucd_prdt_dma_addr = cmd_desc_element_addr +
				prdt_offset;
		cmd_descp = (void *)cmd_descp + cmd_desc_size;
	}
}

/**
 * ufshcd_dme_link_startup - Notify Unipro to perform link startup
 * @hba: per adapter instance
 *
 * UIC_CMD_DME_LINK_STARTUP command must be issued to Unipro layer,
 * in order to initialize the Unipro link startup procedure.
 * Once the Unipro links are up, the device connected to the controller
 * is detected.
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_dme_link_startup(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {0};
	int ret;

	uic_cmd.command = UIC_CMD_DME_LINK_STARTUP;

	ret = ufshcd_send_uic_cmd(hba, &uic_cmd);
	if (ret)
		dev_dbg(hba->dev,
			"dme-link-startup: error code %d\n", ret);
	return ret;
}
/**
 * ufshcd_dme_reset - UIC command for DME_RESET
 * @hba: per adapter instance
 *
 * DME_RESET command is issued in order to reset UniPro stack.
 * This function now deal with cold reset.
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_dme_reset(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {0};
	int ret;

	uic_cmd.command = UIC_CMD_DME_RESET;

	ret = ufshcd_send_uic_cmd(hba, &uic_cmd);
	if (ret)
		dev_err(hba->dev,
			"dme-reset: error code %d\n", ret);

	return ret;
}

/**
 * ufshcd_dme_enable - UIC command for DME_ENABLE
 * @hba: per adapter instance
 *
 * DME_ENABLE command is issued in order to enable UniPro stack.
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_dme_enable(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {0};
	int ret;

	uic_cmd.command = UIC_CMD_DME_ENABLE;

	ret = ufshcd_send_uic_cmd(hba, &uic_cmd);
	if (ret)
		dev_err(hba->dev,
			"dme-reset: error code %d\n", ret);

	return ret;
}

static inline void ufshcd_add_delay_before_dme_cmd(struct ufs_hba *hba)
{
	#define MIN_DELAY_BEFORE_DME_CMDS_US	1000
	unsigned long min_sleep_time_us;

	if (!(hba->quirks & UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS))
		return;

	/*
	 * last_dme_cmd_tstamp will be 0 only for 1st call to
	 * this function
	 */
	if (unlikely(!ktime_to_us(hba->last_dme_cmd_tstamp))) {
		min_sleep_time_us = MIN_DELAY_BEFORE_DME_CMDS_US;
	} else {
		unsigned long delta =
			(unsigned long) ktime_to_us(
				ktime_sub(ktime_get(),
				hba->last_dme_cmd_tstamp));

		if (delta < MIN_DELAY_BEFORE_DME_CMDS_US)
			min_sleep_time_us =
				MIN_DELAY_BEFORE_DME_CMDS_US - delta;
		else
			return; /* no more delay required */
	}

	/* allow sleep for extra 50us if needed */
	usleep_range(min_sleep_time_us, min_sleep_time_us + 50);
}

static inline void ufshcd_save_tstamp_of_last_dme_cmd(
			struct ufs_hba *hba)
{
	if (hba->quirks & UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS)
		hba->last_dme_cmd_tstamp = ktime_get();
}

/**
 * ufshcd_dme_set_attr - UIC command for DME_SET, DME_PEER_SET
 * @hba: per adapter instance
 * @attr_sel: uic command argument1
 * @attr_set: attribute set type as uic command argument2
 * @mib_val: setting value as uic command argument3
 * @peer: indicate whether peer or local
 *
 * Returns 0 on success, non-zero value on failure
 */
int ufshcd_dme_set_attr(struct ufs_hba *hba, u32 attr_sel,
			u8 attr_set, u32 mib_val, u8 peer)
{
	struct uic_command uic_cmd = {0};
	static const char *const action[] = {
		"dme-set",
		"dme-peer-set"
	};
	const char *set = action[!!peer];
	int ret;
	int retries = UFS_UIC_COMMAND_RETRIES;

	ufsdbg_error_inject_dispatcher(hba,
		ERR_INJECT_DME_ATTR, attr_sel, &attr_sel);

	uic_cmd.command = peer ?
		UIC_CMD_DME_PEER_SET : UIC_CMD_DME_SET;
	uic_cmd.argument1 = attr_sel;
	uic_cmd.argument2 = UIC_ARG_ATTR_TYPE(attr_set);
	uic_cmd.argument3 = mib_val;

	do {
		/* for peer attributes we retry upon failure */
		ret = ufshcd_send_uic_cmd(hba, &uic_cmd);
		if (ret)
			dev_dbg(hba->dev, "%s: attr-id 0x%x val 0x%x error code %d\n",
				set, UIC_GET_ATTR_ID(attr_sel), mib_val, ret);
	} while (ret && peer && --retries);

	if (ret)
		dev_err(hba->dev, "%s: attr-id 0x%x val 0x%x failed %d retries\n",
			set, UIC_GET_ATTR_ID(attr_sel), mib_val,
			UFS_UIC_COMMAND_RETRIES - retries);

	return ret;
}
EXPORT_SYMBOL_GPL(ufshcd_dme_set_attr);

/**
 * ufshcd_dme_get_attr - UIC command for DME_GET, DME_PEER_GET
 * @hba: per adapter instance
 * @attr_sel: uic command argument1
 * @mib_val: the value of the attribute as returned by the UIC command
 * @peer: indicate whether peer or local
 *
 * Returns 0 on success, non-zero value on failure
 */
int ufshcd_dme_get_attr(struct ufs_hba *hba, u32 attr_sel,
			u32 *mib_val, u8 peer)
{
	struct uic_command uic_cmd = {0};
	static const char *const action[] = {
		"dme-get",
		"dme-peer-get"
	};
	const char *get = action[!!peer];
	int ret;
	int retries = UFS_UIC_COMMAND_RETRIES;
	struct ufs_pa_layer_attr orig_pwr_info;
	struct ufs_pa_layer_attr temp_pwr_info;
	bool pwr_mode_change = false;

	if (peer && (hba->quirks & UFSHCD_QUIRK_DME_PEER_ACCESS_AUTO_MODE)) {
		orig_pwr_info = hba->pwr_info;
		temp_pwr_info = orig_pwr_info;

		if (orig_pwr_info.pwr_tx == FAST_MODE ||
		    orig_pwr_info.pwr_rx == FAST_MODE) {
			temp_pwr_info.pwr_tx = FASTAUTO_MODE;
			temp_pwr_info.pwr_rx = FASTAUTO_MODE;
			pwr_mode_change = true;
		} else if (orig_pwr_info.pwr_tx == SLOW_MODE ||
		    orig_pwr_info.pwr_rx == SLOW_MODE) {
			temp_pwr_info.pwr_tx = SLOWAUTO_MODE;
			temp_pwr_info.pwr_rx = SLOWAUTO_MODE;
			pwr_mode_change = true;
		}
		if (pwr_mode_change) {
			ret = ufshcd_change_power_mode(hba, &temp_pwr_info);
			if (ret)
				goto out;
		}
	}

	uic_cmd.command = peer ?
		UIC_CMD_DME_PEER_GET : UIC_CMD_DME_GET;

	ufsdbg_error_inject_dispatcher(hba,
		ERR_INJECT_DME_ATTR, attr_sel, &attr_sel);

	uic_cmd.argument1 = attr_sel;

	do {
		/* for peer attributes we retry upon failure */
		ret = ufshcd_send_uic_cmd(hba, &uic_cmd);
		if (ret)
			dev_dbg(hba->dev, "%s: attr-id 0x%x error code %d\n",
				get, UIC_GET_ATTR_ID(attr_sel), ret);
	} while (ret && peer && --retries);

	if (ret)
		dev_err(hba->dev, "%s: attr-id 0x%x failed %d retries\n",
			get, UIC_GET_ATTR_ID(attr_sel),
			UFS_UIC_COMMAND_RETRIES - retries);

	if (mib_val && !ret)
		*mib_val = uic_cmd.argument3;

	if (peer && (hba->quirks & UFSHCD_QUIRK_DME_PEER_ACCESS_AUTO_MODE)
	    && pwr_mode_change)
		ufshcd_change_power_mode(hba, &orig_pwr_info);
out:
	return ret;
}
EXPORT_SYMBOL_GPL(ufshcd_dme_get_attr);

/**
 * ufshcd_uic_pwr_ctrl - executes UIC commands (which affects the link power
 * state) and waits for it to take effect.
 *
 * @hba: per adapter instance
 * @cmd: UIC command to execute
 *
 * DME operations like DME_SET(PA_PWRMODE), DME_HIBERNATE_ENTER &
 * DME_HIBERNATE_EXIT commands take some time to take its effect on both host
 * and device UniPro link and hence it's final completion would be indicated by
 * dedicated status bits in Interrupt Status register (UPMS, UHES, UHXS) in
 * addition to normal UIC command completion Status (UCCS). This function only
 * returns after the relevant status bits indicate the completion.
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_uic_pwr_ctrl(struct ufs_hba *hba, struct uic_command *cmd)
{
	struct completion uic_async_done;
	unsigned long flags;
	u8 status;
	int ret;
	bool reenable_intr = false;
	int wait_retries = 6; /* Allows 3secs max wait time */

	mutex_lock(&hba->uic_cmd_mutex);
	init_completion(&uic_async_done);
	ufshcd_add_delay_before_dme_cmd(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->uic_async_done = &uic_async_done;

	if (ufshcd_readl(hba, REG_INTERRUPT_ENABLE) & UIC_COMMAND_COMPL) {
		ufshcd_disable_intr(hba, UIC_COMMAND_COMPL);
		/*
		 * Make sure UIC command completion interrupt is disabled before
		 * issuing UIC command.
		 */
		wmb();
		reenable_intr = true;
	}
	ret = __ufshcd_send_uic_cmd(hba, cmd, false);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (ret) {
		dev_err(hba->dev,
			"pwr ctrl cmd 0x%x with mode 0x%x uic error %d\n",
			cmd->command, cmd->argument3, ret);
		goto out;
	}

more_wait:
	if (!wait_for_completion_timeout(hba->uic_async_done,
					 msecs_to_jiffies(UIC_CMD_TIMEOUT))) {
		u32 intr_status = 0;
		s64 ts_since_last_intr;

		dev_err(hba->dev,
			"pwr ctrl cmd 0x%x with mode 0x%x completion timeout\n",
			cmd->command, cmd->argument3);
		/*
		 * The controller must have triggered interrupt but ISR couldn't
		 * run due to interrupt starvation.
		 * Or ISR must have executed just after the timeout
		 * (which clears IS registers)
		 * If either of these two cases is true, then
		 * wait for little more time for completion.
		 */
		intr_status = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
		ts_since_last_intr = ktime_ms_delta(ktime_get(),
						hba->ufs_stats.last_intr_ts);

		if ((intr_status & UFSHCD_UIC_PWR_MASK) ||
		    ((hba->ufs_stats.last_intr_status & UFSHCD_UIC_PWR_MASK) &&
		     (ts_since_last_intr < (s64)UIC_CMD_TIMEOUT))) {
			dev_info(hba->dev, "IS:0x%08x last_intr_sts:0x%08x last_intr_ts:%lld, retry-cnt:%d\n",
				intr_status, hba->ufs_stats.last_intr_status,
				hba->ufs_stats.last_intr_ts, wait_retries);
			if (wait_retries--)
				goto more_wait;

			/*
			 * If same state continues event after more wait time,
			 * something must be hogging CPU.
			 */
			BUG_ON(hba->crash_on_err);
		}
		ret = -ETIMEDOUT;
		goto out;
	}

	status = ufshcd_get_upmcrs(hba);
	if (status != PWR_LOCAL) {
		dev_err(hba->dev,
			"pwr ctrl cmd 0x%x failed, host upmcrs:0x%x\n",
			cmd->command, status);
		ret = (status != PWR_OK) ? status : -1;
	}
	ufshcd_dme_cmd_log(hba, "dme_cmpl_2", hba->active_uic_cmd->command);

out:
	if (ret) {
#if defined(SEC_UFS_ERROR_COUNT)
		SEC_ufs_uic_error_check(hba, true, false);
#endif
		ufsdbg_set_err_state(hba);
		ufshcd_print_host_state(hba);
		ufshcd_print_pwr_info(hba);
		ufshcd_print_host_regs(hba);
		ufshcd_print_cmd_log(hba);
		BUG_ON(hba->crash_on_err);
	}

	ufshcd_save_tstamp_of_last_dme_cmd(hba);
	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->active_uic_cmd = NULL;
	hba->uic_async_done = NULL;
	if (reenable_intr)
		ufshcd_enable_intr(hba, UIC_COMMAND_COMPL);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	mutex_unlock(&hba->uic_cmd_mutex);
	return ret;
}

/**
 * ufshcd_uic_change_pwr_mode - Perform the UIC power mode chage
 *				using DME_SET primitives.
 * @hba: per adapter instance
 * @mode: powr mode value
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_uic_change_pwr_mode(struct ufs_hba *hba, u8 mode)
{
	struct uic_command uic_cmd = {0};
	int ret;

	if (hba->quirks & UFSHCD_QUIRK_BROKEN_PA_RXHSUNTERMCAP) {
		ret = ufshcd_dme_set(hba,
				UIC_ARG_MIB_SEL(PA_RXHSUNTERMCAP, 0), 1);
		if (ret) {
			dev_err(hba->dev, "%s: failed to enable PA_RXHSUNTERMCAP ret %d\n",
						__func__, ret);
			goto out;
		}
	}

	uic_cmd.command = UIC_CMD_DME_SET;
	uic_cmd.argument1 = UIC_ARG_MIB(PA_PWRMODE);
	uic_cmd.argument3 = mode;
	hba->ufs_stats.clk_hold.ctx = PWRCTL_CMD_SEND;
	ufshcd_hold_all(hba);
	ret = ufshcd_uic_pwr_ctrl(hba, &uic_cmd);
	hba->ufs_stats.clk_rel.ctx = PWRCTL_CMD_SEND;
	ufshcd_release_all(hba);
out:
	return ret;
}

static int ufshcd_link_recovery(struct ufs_hba *hba)
{
	int ret = 0;
	unsigned long flags;

	ufshcd_enable_irq(hba);

	/*
	 * Check if there is any race with fatal error handling.
	 * If so, wait for it to complete. Even though fatal error
	 * handling does reset and restore in some cases, don't assume
	 * anything out of it. We are just avoiding race here.
	 */
	do {
		spin_lock_irqsave(hba->host->host_lock, flags);
		if (!(work_pending(&hba->eh_work) ||
				hba->ufshcd_state == UFSHCD_STATE_RESET))
			break;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		dev_dbg(hba->dev, "%s: reset in progress 1\n", __func__);
		flush_work(&hba->eh_work);
	} while (1);


	/*
	 * we don't know if previous reset had really reset the host controller
	 * or not. So let's force reset here to be sure.
	 */
	hba->ufshcd_state = UFSHCD_STATE_ERROR;
	hba->force_host_reset = true;
	ufshcd_set_eh_in_progress(hba);
	queue_work(hba->recovery_wq, &hba->eh_work);

	/* wait for the reset work to finish */
	do {
		if (!(work_pending(&hba->eh_work) ||
				hba->ufshcd_state == UFSHCD_STATE_RESET))
			break;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		dev_dbg(hba->dev, "%s: reset in progress 2\n", __func__);
		flush_work(&hba->eh_work);
		spin_lock_irqsave(hba->host->host_lock, flags);
	} while (1);

	if (!((hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL) &&
	      ufshcd_is_link_active(hba)))
		ret = -ENOLINK;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return ret;
}

static int __ufshcd_uic_hibern8_enter(struct ufs_hba *hba)
{
	int ret;
	struct uic_command uic_cmd = {0};
	ktime_t start = ktime_get();

	ufshcd_vops_hibern8_notify(hba, UIC_CMD_DME_HIBER_ENTER, PRE_CHANGE);

	uic_cmd.command = UIC_CMD_DME_HIBER_ENTER;
	ret = ufshcd_uic_pwr_ctrl(hba, &uic_cmd);
	trace_ufshcd_profile_hibern8(dev_name(hba->dev), "enter",
			     ktime_to_us(ktime_sub(ktime_get(), start)), ret);

	ufsdbg_error_inject_dispatcher(hba, ERR_INJECT_HIBERN8_ENTER, 0, &ret);

	/*
	 * Do full reinit if enter failed or if LINERESET was detected during
	 * Hibern8 operation. After LINERESET, link moves to default PWM-G1
	 * mode hence full reinit is required to move link to HS speeds.
	 */
	if (ret || hba->full_init_linereset) {
		int err;

		hba->full_init_linereset = false;
		ufshcd_update_error_stats(hba, UFS_ERR_HIBERN8_ENTER);
		dev_err(hba->dev, "%s: hibern8 enter failed. ret = %d\n",
			__func__, ret);
#if defined(SEC_UFS_ERROR_COUNT)
		SEC_ufs_operation_check(hba, UIC_CMD_DME_HIBER_ENTER);
#endif

		/*
		 * If link recovery fails then return error code (-ENOLINK)
		 * returned ufshcd_link_recovery().
		 * If link recovery succeeds then return -EAGAIN to attempt
		 * hibern8 enter retry again.
		 */
		err = ufshcd_link_recovery(hba);
		if (err) {
			dev_err(hba->dev, "%s: link recovery failed\n",
				__func__);
			ret = err;
		} else {
			ret = -EAGAIN;
		}
	} else {
		ufshcd_vops_hibern8_notify(hba, UIC_CMD_DME_HIBER_ENTER,
								POST_CHANGE);
		dev_dbg(hba->dev, "%s: Hibern8 Enter at %lld us\n", __func__,
			ktime_to_us(ktime_get()));
#ifdef CONFIG_BLK_TURBO_WRITE
		SEC_ufs_update_h8_info(hba, true);
#endif
	}

	return ret;
}

int ufshcd_uic_hibern8_enter(struct ufs_hba *hba)
{
	int ret = 0, retries;

	for (retries = UIC_HIBERN8_ENTER_RETRIES; retries > 0; retries--) {
		ret = __ufshcd_uic_hibern8_enter(hba);
		if (!ret)
			goto out;
		else if (ret != -EAGAIN)
			/* Unable to recover the link, so no point proceeding */
			BUG_ON(1);
	}
out:
	return ret;
}

int ufshcd_uic_hibern8_exit(struct ufs_hba *hba)
{
	struct uic_command uic_cmd = {0};
	int ret;
	ktime_t start = ktime_get();

	ufshcd_vops_hibern8_notify(hba, UIC_CMD_DME_HIBER_EXIT, PRE_CHANGE);

	uic_cmd.command = UIC_CMD_DME_HIBER_EXIT;
	ret = ufshcd_uic_pwr_ctrl(hba, &uic_cmd);
	trace_ufshcd_profile_hibern8(dev_name(hba->dev), "exit",
			     ktime_to_us(ktime_sub(ktime_get(), start)), ret);

	ufsdbg_error_inject_dispatcher(hba, ERR_INJECT_HIBERN8_EXIT, 0, &ret);

	/* Do full reinit if exit failed */
	if (ret) {
		ufshcd_update_error_stats(hba, UFS_ERR_HIBERN8_EXIT);
		dev_err(hba->dev, "%s: hibern8 exit failed. ret = %d\n",
			__func__, ret);
#if defined(SEC_UFS_ERROR_COUNT)
		SEC_ufs_operation_check(hba, UIC_CMD_DME_HIBER_EXIT);
#endif
		ret = ufshcd_link_recovery(hba);
		/* Unable to recover the link, so no point proceeding */
		if (ret)
			BUG_ON(1);
	} else {
		ufshcd_vops_hibern8_notify(hba, UIC_CMD_DME_HIBER_EXIT,
								POST_CHANGE);
		dev_dbg(hba->dev, "%s: Hibern8 Exit at %lld us", __func__,
			ktime_to_us(ktime_get()));
		hba->ufs_stats.last_hibern8_exit_tstamp = ktime_get();
		hba->ufs_stats.hibern8_exit_cnt++;
#ifdef CONFIG_BLK_TURBO_WRITE
		SEC_ufs_update_h8_info(hba, false);
#endif
	}

	return ret;
}

static void ufshcd_set_auto_hibern8_timer(struct ufs_hba *hba)
{
	unsigned long flags;

	if (!ufshcd_is_auto_hibern8_supported(hba))
		return;

	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_writel(hba, hba->ahit, REG_AUTO_HIBERNATE_IDLE_TIMER);
	/* Make sure the timer gets applied before further operations */
	mb();
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

 /**
 * ufshcd_init_pwr_info - setting the POR (power on reset)
 * values in hba power info
 * @hba: per-adapter instance
 */
static void ufshcd_init_pwr_info(struct ufs_hba *hba)
{
	hba->pwr_info.gear_rx = UFS_PWM_G1;
	hba->pwr_info.gear_tx = UFS_PWM_G1;
	hba->pwr_info.lane_rx = 1;
	hba->pwr_info.lane_tx = 1;
	hba->pwr_info.pwr_rx = SLOWAUTO_MODE;
	hba->pwr_info.pwr_tx = SLOWAUTO_MODE;
	hba->pwr_info.hs_rate = 0;
}

/**
 * ufshcd_get_max_pwr_mode - reads the max power mode negotiated with device
 * @hba: per-adapter instance
 */
static int ufshcd_get_max_pwr_mode(struct ufs_hba *hba)
{
	struct ufs_pa_layer_attr *pwr_info = &hba->max_pwr_info.info;

	if (hba->max_pwr_info.is_valid)
		return 0;

	pwr_info->pwr_tx = FAST_MODE;
	pwr_info->pwr_rx = FAST_MODE;
	pwr_info->hs_rate = PA_HS_MODE_B;

	/* Get the connected lane count */
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDRXDATALANES),
			&pwr_info->lane_rx);
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
			&pwr_info->lane_tx);

	if (!pwr_info->lane_rx || !pwr_info->lane_tx) {
		dev_err(hba->dev, "%s: invalid connected lanes value. rx=%d, tx=%d\n",
				__func__,
				pwr_info->lane_rx,
				pwr_info->lane_tx);
		return -EINVAL;
	}

	/*
	 * First, get the maximum gears of HS speed.
	 * If a zero value, it means there is no HSGEAR capability.
	 * Then, get the maximum gears of PWM speed.
	 */
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR), &pwr_info->gear_rx);
	if (!pwr_info->gear_rx) {
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&pwr_info->gear_rx);
		if (!pwr_info->gear_rx) {
			dev_err(hba->dev, "%s: invalid max pwm rx gear read = %d\n",
				__func__, pwr_info->gear_rx);
			return -EINVAL;
		} else {
			if (hba->limit_rx_pwm_gear > 0 &&
			    (hba->limit_rx_pwm_gear < pwr_info->gear_rx))
				pwr_info->gear_rx = hba->limit_rx_pwm_gear;
		}
		pwr_info->pwr_rx = SLOW_MODE;
	} else {
		if (hba->limit_rx_hs_gear > 0 &&
		    (hba->limit_rx_hs_gear < pwr_info->gear_rx))
			pwr_info->gear_rx = hba->limit_rx_hs_gear;
	}

	ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR),
			&pwr_info->gear_tx);
	if (!pwr_info->gear_tx) {
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&pwr_info->gear_tx);
		if (!pwr_info->gear_tx) {
			dev_err(hba->dev, "%s: invalid max pwm tx gear read = %d\n",
				__func__, pwr_info->gear_tx);
			return -EINVAL;
		} else {
			if (hba->limit_tx_pwm_gear > 0 &&
			    (hba->limit_tx_pwm_gear < pwr_info->gear_tx))
				pwr_info->gear_tx = hba->limit_tx_pwm_gear;
		}
		pwr_info->pwr_tx = SLOW_MODE;
	} else {
		if (hba->limit_tx_hs_gear > 0 &&
		    (hba->limit_tx_hs_gear < pwr_info->gear_tx))
			pwr_info->gear_tx = hba->limit_tx_hs_gear;
	}

	hba->max_pwr_info.is_valid = true;
	return 0;
}

int ufshcd_change_power_mode(struct ufs_hba *hba,
			     struct ufs_pa_layer_attr *pwr_mode)
{
	int ret = 0;
	u32 peer_rx_hs_adapt_initial_cap;

	/* if already configured to the requested pwr_mode */
	if (!hba->restore_needed &&
		pwr_mode->gear_rx == hba->pwr_info.gear_rx &&
		pwr_mode->gear_tx == hba->pwr_info.gear_tx &&
	    pwr_mode->lane_rx == hba->pwr_info.lane_rx &&
	    pwr_mode->lane_tx == hba->pwr_info.lane_tx &&
	    pwr_mode->pwr_rx == hba->pwr_info.pwr_rx &&
	    pwr_mode->pwr_tx == hba->pwr_info.pwr_tx &&
	    pwr_mode->hs_rate == hba->pwr_info.hs_rate) {
		dev_dbg(hba->dev, "%s: power already configured\n", __func__);
		return 0;
	}

	ufsdbg_error_inject_dispatcher(hba, ERR_INJECT_PWR_CHANGE, 0, &ret);
	if (ret)
		return ret;

	/*
	 * Configure attributes for power mode change with below.
	 * - PA_RXGEAR, PA_ACTIVERXDATALANES, PA_RXTERMINATION,
	 * - PA_TXGEAR, PA_ACTIVETXDATALANES, PA_TXTERMINATION,
	 * - PA_HSSERIES
	 */
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_RXGEAR), pwr_mode->gear_rx);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_ACTIVERXDATALANES),
			pwr_mode->lane_rx);
	if (pwr_mode->pwr_rx == FASTAUTO_MODE ||
			pwr_mode->pwr_rx == FAST_MODE)
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_RXTERMINATION), TRUE);
	else
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_RXTERMINATION), FALSE);

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXGEAR), pwr_mode->gear_tx);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_ACTIVETXDATALANES),
			pwr_mode->lane_tx);
	if (pwr_mode->pwr_tx == FASTAUTO_MODE ||
			pwr_mode->pwr_tx == FAST_MODE)
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXTERMINATION), TRUE);
	else
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXTERMINATION), FALSE);

	if (pwr_mode->pwr_rx == FASTAUTO_MODE ||
	    pwr_mode->pwr_tx == FASTAUTO_MODE ||
	    pwr_mode->pwr_rx == FAST_MODE ||
	    pwr_mode->pwr_tx == FAST_MODE)
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_HSSERIES),
						pwr_mode->hs_rate);

	if (pwr_mode->gear_tx == UFS_HS_G4) {
		ret = ufshcd_dme_peer_get(hba,
				UIC_ARG_MIB_SEL(RX_HS_ADAPT_INITIAL_CAPABILITY,
				UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)),
				&peer_rx_hs_adapt_initial_cap);
		if (ret) {
			dev_err(hba->dev,
				"%s: RX_HS_ADAPT_INITIAL_CAP get failed %d\n",
				__func__, ret);
			peer_rx_hs_adapt_initial_cap =
				PA_PEERRXHSADAPTINITIAL_Default;
		}
		ret = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PEERRXHSADAPTINITIAL),
			peer_rx_hs_adapt_initial_cap);
		/* INITIAL ADAPT */
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXHSADAPTTYPE),
			PA_INITIAL_ADAPT);
	} else {
		/* NO ADAPT */
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXHSADAPTTYPE), PA_NO_ADAPT);
	}

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA0),
			DL_FC0ProtectionTimeOutVal_Default);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA1),
			DL_TC0ReplayTimeOutVal_Default);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA2),
			DL_AFC0ReqTimeOutVal_Default);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA3),
			DL_FC1ProtectionTimeOutVal_Default);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA4),
			DL_TC1ReplayTimeOutVal_Default);
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PWRMODEUSERDATA5),
			DL_AFC1ReqTimeOutVal_Default);

	ufshcd_dme_set(hba, UIC_ARG_MIB(DME_LocalFC0ProtectionTimeOutVal),
			DL_FC0ProtectionTimeOutVal_Default);
	ufshcd_dme_set(hba, UIC_ARG_MIB(DME_LocalTC0ReplayTimeOutVal),
			DL_TC0ReplayTimeOutVal_Default);
	ufshcd_dme_set(hba, UIC_ARG_MIB(DME_LocalAFC0ReqTimeOutVal),
			DL_AFC0ReqTimeOutVal_Default);

	ret = ufshcd_uic_change_pwr_mode(hba, pwr_mode->pwr_rx << 4
			| pwr_mode->pwr_tx);

	if (ret) {
		ufshcd_update_error_stats(hba, UFS_ERR_POWER_MODE_CHANGE);
		dev_err(hba->dev,
			"%s: power mode change failed %d\n", __func__, ret);
	} else {
		ufshcd_vops_pwr_change_notify(hba, POST_CHANGE, NULL,
								pwr_mode);

		memcpy(&hba->pwr_info, pwr_mode,
			sizeof(struct ufs_pa_layer_attr));
		hba->ufs_stats.power_mode_change_cnt++;
	}

	return ret;
}

/**
 * ufshcd_config_pwr_mode - configure a new power mode
 * @hba: per-adapter instance
 * @desired_pwr_mode: desired power configuration
 */
int ufshcd_config_pwr_mode(struct ufs_hba *hba,
		struct ufs_pa_layer_attr *desired_pwr_mode)
{
	struct ufs_pa_layer_attr final_params = { 0 };
	int ret;

	ret = ufshcd_vops_pwr_change_notify(hba, PRE_CHANGE,
					desired_pwr_mode, &final_params);

	if (ret)
		memcpy(&final_params, desired_pwr_mode, sizeof(final_params));

	ret = ufshcd_change_power_mode(hba, &final_params);
	if (!ret)
		ufshcd_print_pwr_info(hba);

	return ret;
}
EXPORT_SYMBOL_GPL(ufshcd_config_pwr_mode);

/**
 * ufshcd_complete_dev_init() - checks device readiness
 * @hba: per-adapter instance
 *
 * Set fDeviceInit flag and poll until device toggles it.
 */
static int ufshcd_complete_dev_init(struct ufs_hba *hba)
{
	int err;
	bool flag_res = 1;
	unsigned long timeout;

	err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG,
		QUERY_FLAG_IDN_FDEVICEINIT, NULL);
	if (err) {
		dev_err(hba->dev,
			"%s setting fDeviceInit flag failed with error %d\n",
			__func__, err);
		goto out;
	}

	/* Poll fDeviceInit flag to be cleared */
	timeout = jiffies + msecs_to_jiffies(DEV_INIT_COMPL_TIMEOUT);

	do {
		err = ufshcd_query_flag(hba, UPIU_QUERY_OPCODE_READ_FLAG,
				QUERY_FLAG_IDN_FDEVICEINIT, &flag_res);
		if (!flag_res)
			break;
		usleep_range(1000, 1000);
	} while (time_before(jiffies, timeout));

	if (err)
		dev_err(hba->dev,
			"%s reading fDeviceInit flag failed with error %d\n",
			__func__, err);
	else if (flag_res)
		dev_err(hba->dev,
			"%s fDeviceInit was not cleared by the device\n",
			__func__);

out:
	return err;
}

/**
 * ufshcd_make_hba_operational - Make UFS controller operational
 * @hba: per adapter instance
 *
 * To bring UFS host controller to operational state,
 * 1. Enable required interrupts
 * 2. Configure interrupt aggregation
 * 3. Program UTRL and UTMRL base address
 * 4. Configure run-stop-registers
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_make_hba_operational(struct ufs_hba *hba)
{
	int err = 0;
	u32 reg;

	/* Enable required interrupts */
	ufshcd_enable_intr(hba, UFSHCD_ENABLE_INTRS);

	/* Configure interrupt aggregation */
	if (ufshcd_is_intr_aggr_allowed(hba))
		ufshcd_config_intr_aggr(hba, hba->nutrs - 1, INT_AGGR_DEF_TO);
	else
		ufshcd_disable_intr_aggr(hba);

	/* Configure UTRL and UTMRL base address registers */
	ufshcd_writel(hba, lower_32_bits(hba->utrdl_dma_addr),
			REG_UTP_TRANSFER_REQ_LIST_BASE_L);
	ufshcd_writel(hba, upper_32_bits(hba->utrdl_dma_addr),
			REG_UTP_TRANSFER_REQ_LIST_BASE_H);
	ufshcd_writel(hba, lower_32_bits(hba->utmrdl_dma_addr),
			REG_UTP_TASK_REQ_LIST_BASE_L);
	ufshcd_writel(hba, upper_32_bits(hba->utmrdl_dma_addr),
			REG_UTP_TASK_REQ_LIST_BASE_H);

	/*
	 * Make sure base address and interrupt setup are updated before
	 * enabling the run/stop registers below.
	 */
	wmb();

	/*
	 * UCRDY, UTMRLDY and UTRLRDY bits must be 1
	 */
	reg = ufshcd_readl(hba, REG_CONTROLLER_STATUS);
	if (!(ufshcd_get_lists_status(reg))) {
		ufshcd_enable_run_stop_reg(hba);
	} else {
		dev_err(hba->dev,
			"Host controller not ready to process requests");
		err = -EIO;
		goto out;
	}

out:
	return err;
}

/**
 * ufshcd_hba_stop - Send controller to reset state
 * @hba: per adapter instance
 * @can_sleep: perform sleep or just spin
 */
static inline void ufshcd_hba_stop(struct ufs_hba *hba, bool can_sleep)
{
	int err;

	ufshcd_crypto_disable(hba);

	ufshcd_writel(hba, CONTROLLER_DISABLE,  REG_CONTROLLER_ENABLE);
	err = ufshcd_wait_for_register(hba, REG_CONTROLLER_ENABLE,
					CONTROLLER_ENABLE, CONTROLLER_DISABLE,
					10, 1, can_sleep);
	if (err)
		dev_err(hba->dev, "%s: Controller disable failed\n", __func__);
}

/**
 * ufshcd_hba_execute_hce - initialize the controller
 * @hba: per adapter instance
 *
 * The controller resets itself and controller firmware initialization
 * sequence kicks off. When controller is ready it will set
 * the Host Controller Enable bit to 1.
 *
 * Returns 0 on success, non-zero value on failure
 */
static int ufshcd_hba_execute_hce(struct ufs_hba *hba)
{
	int retry;

	/*
	 * msleep of 1 and 5 used in this function might result in msleep(20),
	 * but it was necessary to send the UFS FPGA to reset mode during
	 * development and testing of this driver. msleep can be changed to
	 * mdelay and retry count can be reduced based on the controller.
	 */
	if (!ufshcd_is_hba_active(hba))
		/* change controller state to "reset state" */
		ufshcd_hba_stop(hba, true);

	/* UniPro link is disabled at this point */
	ufshcd_set_link_off(hba);

	ufshcd_vops_hce_enable_notify(hba, PRE_CHANGE);

	/* start controller initialization sequence */
	ufshcd_hba_start(hba);

	/*
	 * To initialize a UFS host controller HCE bit must be set to 1.
	 * During initialization the HCE bit value changes from 1->0->1.
	 * When the host controller completes initialization sequence
	 * it sets the value of HCE bit to 1. The same HCE bit is read back
	 * to check if the controller has completed initialization sequence.
	 * So without this delay the value HCE = 1, set in the previous
	 * instruction might be read back.
	 * This delay can be changed based on the controller.
	 */
	msleep(1);

	/* wait for the host controller to complete initialization */
	retry = 10;
	while (ufshcd_is_hba_active(hba)) {
		if (retry) {
			retry--;
		} else {
			dev_err(hba->dev,
				"Controller enable failed\n");
			return -EIO;
		}
		msleep(5);
	}

	/* enable UIC related interrupts */
	ufshcd_enable_intr(hba, UFSHCD_UIC_MASK);

	ufshcd_vops_hce_enable_notify(hba, POST_CHANGE);

	return 0;
}

static int ufshcd_hba_enable(struct ufs_hba *hba)
{
	int ret;

	if (hba->quirks & UFSHCI_QUIRK_BROKEN_HCE) {
		ufshcd_set_link_off(hba);
		ufshcd_vops_hce_enable_notify(hba, PRE_CHANGE);

		/* enable UIC related interrupts */
		ufshcd_enable_intr(hba, UFSHCD_UIC_MASK);
		ret = ufshcd_dme_reset(hba);
		if (!ret) {
			ret = ufshcd_dme_enable(hba);
			if (!ret)
				ufshcd_vops_hce_enable_notify(hba, POST_CHANGE);
			if (ret)
				dev_err(hba->dev,
					"Host controller enable failed with non-hce\n");
		}
	} else {
		ret = ufshcd_hba_execute_hce(hba);
	}

	return ret;
}
static int ufshcd_disable_tx_lcc(struct ufs_hba *hba, bool peer)
{
	int tx_lanes, i, err = 0;

	if (!peer)
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
			       &tx_lanes);
	else
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
				    &tx_lanes);
	for (i = 0; i < tx_lanes; i++) {
		if (!peer)
			err = ufshcd_dme_set(hba,
				UIC_ARG_MIB_SEL(TX_LCC_ENABLE,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(i)),
					0);
		else
			err = ufshcd_dme_peer_set(hba,
				UIC_ARG_MIB_SEL(TX_LCC_ENABLE,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(i)),
					0);
		if (err) {
			dev_err(hba->dev, "%s: TX LCC Disable failed, peer = %d, lane = %d, err = %d",
				__func__, peer, i, err);
			break;
		}
	}

	return err;
}

static inline int ufshcd_disable_host_tx_lcc(struct ufs_hba *hba)
{
	return ufshcd_disable_tx_lcc(hba, false);
}

static inline int ufshcd_disable_device_tx_lcc(struct ufs_hba *hba)
{
	return ufshcd_disable_tx_lcc(hba, true);
}

/**
 * ufshcd_link_startup - Initialize unipro link startup
 * @hba: per adapter instance
 *
 * Returns 0 for success, non-zero in case of failure
 */
static int ufshcd_link_startup(struct ufs_hba *hba)
{
	int ret;
	int retries = DME_LINKSTARTUP_RETRIES;
	bool link_startup_again = false;

	/*
	 * If UFS device isn't active then we will have to issue link startup
	 * 2 times to make sure the device state move to active.
	 */
	if (!ufshcd_is_ufs_dev_active(hba))
		link_startup_again = true;

link_startup:
	do {
		ufshcd_vops_link_startup_notify(hba, PRE_CHANGE);

		ret = ufshcd_dme_link_startup(hba);
		if (ret)
			ufshcd_update_error_stats(hba, UFS_ERR_LINKSTARTUP);

		/* check if device is detected by inter-connect layer */
		if (!ret && !ufshcd_is_device_present(hba)) {
			ufshcd_update_error_stats(hba, UFS_ERR_LINKSTARTUP);
			dev_err(hba->dev, "%s: Device not present\n", __func__);
			ret = -ENXIO;
			goto out;
		}

		/*
		 * DME link lost indication is only received when link is up,
		 * but we can't be sure if the link is up until link startup
		 * succeeds. So reset the local Uni-Pro and try again.
		 */
		if ((ret && !retries && !link_startup_again) || (ret && ufshcd_hba_enable(hba)))
			goto out;
	} while (ret && retries--);

	if (ret && link_startup_again) {
		link_startup_again = false;

		ret = ufshcd_reset_device(hba);
		if (ret)
			dev_warn(hba->dev, "%s: device reset failed. err %d\n",
				 __func__, ret);

		retries = DME_LINKSTARTUP_RETRIES;
		goto link_startup;
	}

	if (ret)
		/* failed to get the link up... retire */
		goto out;

	/* Mark that link is up in PWM-G1, 1-lane, SLOW-AUTO mode */
	ufshcd_init_pwr_info(hba);
	ufshcd_print_pwr_info(hba);

	if (hba->quirks & UFSHCD_QUIRK_BROKEN_LCC) {
		ret = ufshcd_disable_device_tx_lcc(hba);
		if (ret)
			goto out;
	}

	if (hba->dev_info.quirks & UFS_DEVICE_QUIRK_BROKEN_LCC) {
		ret = ufshcd_disable_host_tx_lcc(hba);
		if (ret)
			goto out;
	}

	/* Include any host controller configuration via UIC commands */
	ret = ufshcd_vops_link_startup_notify(hba, POST_CHANGE);
	if (ret)
		goto out;

	ret = ufshcd_make_hba_operational(hba);
out:
	if (ret)
		dev_err(hba->dev, "link startup failed %d\n", ret);

#if defined(SEC_UFS_ERROR_COUNT)
	if (ret)
		SEC_ufs_operation_check(hba, UIC_CMD_DME_LINK_STARTUP);
#endif
	return ret;
}

/**
 * ufshcd_verify_dev_init() - Verify device initialization
 * @hba: per-adapter instance
 *
 * Send NOP OUT UPIU and wait for NOP IN response to check whether the
 * device Transport Protocol (UTP) layer is ready after a reset.
 * If the UTP layer at the device side is not initialized, it may
 * not respond with NOP IN UPIU within timeout of %NOP_OUT_TIMEOUT
 * and we retry sending NOP OUT for %NOP_OUT_RETRIES iterations.
 */
static int ufshcd_verify_dev_init(struct ufs_hba *hba)
{
	int err = 0;
	int retries;
	bool has_read_lock = false;

	ufshcd_hold_all(hba);
	if (!ufshcd_is_shutdown_ongoing(hba) && !ufshcd_eh_in_progress(hba)) {
		down_read(&hba->lock);
		has_read_lock = true;
	}
	mutex_lock(&hba->dev_cmd.lock);
	for (retries = NOP_OUT_RETRIES; retries > 0; retries--) {
		err = ufshcd_exec_dev_cmd(hba, DEV_CMD_TYPE_NOP,
					       NOP_OUT_TIMEOUT);

		if (!err || err == -ETIMEDOUT)
			break;

		dev_dbg(hba->dev, "%s: error %d retrying\n", __func__, err);
	}
	mutex_unlock(&hba->dev_cmd.lock);
	if (has_read_lock)
		up_read(&hba->lock);
	ufshcd_release_all(hba);

	if (err)
		dev_err(hba->dev, "%s: NOP OUT failed %d\n", __func__, err);

#if defined(SEC_UFS_ERROR_COUNT)
	if (err)
		SEC_ufs_query_error_check(hba, DEV_CMD_TYPE_NOP);
#endif
	return err;
}

/**
 * ufshcd_set_queue_depth - set lun queue depth
 * @sdev: pointer to SCSI device
 *
 * Read bLUQueueDepth value and activate scsi tagged command
 * queueing. For WLUN, queue depth is set to 1. For best-effort
 * cases (bLUQueueDepth = 0) the queue depth is set to a maximum
 * value that host can queue.
 */
static void ufshcd_set_queue_depth(struct scsi_device *sdev)
{
	int ret = 0;
	u8 lun_qdepth;
	struct ufs_hba *hba;

	hba = shost_priv(sdev->host);

	lun_qdepth = hba->nutrs;
	ret = ufshcd_read_unit_desc_param(hba,
			  ufshcd_scsi_to_upiu_lun(sdev->lun),
			  UNIT_DESC_PARAM_LU_Q_DEPTH,
			  &lun_qdepth,
			  sizeof(lun_qdepth));

	/* Some WLUN doesn't support unit descriptor */
	if (ret == -EOPNOTSUPP)
		lun_qdepth = 1;
	else if (!lun_qdepth)
		/* eventually, we can figure out the real queue depth */
		lun_qdepth = hba->nutrs;
	else
		lun_qdepth = min_t(int, lun_qdepth, hba->nutrs);

	dev_dbg(hba->dev, "%s: activate tcq with queue depth %d\n",
			__func__, lun_qdepth);
	scsi_change_queue_depth(sdev, lun_qdepth);
}

/**
 * ufshcd_get_bootlunID - get boot lun
 * @sdev: pointer to SCSI device
 *
 * Read bBootLunID in UNIT Descriptor to find boot LUN
 */
static void ufshcd_get_bootlunID(struct scsi_device *sdev)
{
	int ret = 0;
	u8 bBootLunID = 0;
	struct ufs_hba *hba;

	hba = shost_priv(sdev->host);
	ret = ufshcd_read_unit_desc_param(hba,
					  ufshcd_scsi_to_upiu_lun(sdev->lun),
					  UNIT_DESC_PARAM_BOOT_LUN_ID,
					  &bBootLunID,
					  sizeof(bBootLunID));

	/* Some WLUN doesn't support unit descriptor */
	if (ret == -EOPNOTSUPP)
		bBootLunID = 0;
	else
		sdev->bootlunID = bBootLunID;
}

#ifdef CONFIG_BLK_TURBO_WRITE
/**
 * ufshcd_get_twbuf_unit - get tw buffer alloc units
 * @sdev: pointer to SCSI device
 *
 * Read dLUNumTurboWriteBufferAllocUnits in UNIT Descriptor
 * to check if LU supports turbo write feature
 */
static void ufshcd_get_twbuf_unit(struct scsi_device *sdev)
{
	int ret = 0;
	u32 dLUNumTurboWriteBufferAllocUnits = 0;
	u8 desc_buf[4];
	struct ufs_hba *hba;

	hba = shost_priv(sdev->host);
	if (!hba->support_tw)
		return;

	ret = ufshcd_read_unit_desc_param(hba,
			ufshcd_scsi_to_upiu_lun(sdev->lun),
			UNIT_DESC_PARAM_WB_BUF_ALLOC_UNITS,
			desc_buf,
			sizeof(dLUNumTurboWriteBufferAllocUnits));

	/* Some WLUN doesn't support unit descriptor */
	if ((ret == -EOPNOTSUPP) || scsi_is_wlun(sdev->lun)) {
		sdev->support_tw_lu = false;
		return;
	}

	dLUNumTurboWriteBufferAllocUnits = ((desc_buf[0] << 24)|
			(desc_buf[1] << 16) |
			(desc_buf[2] << 8) |
			desc_buf[3]);

	if (dLUNumTurboWriteBufferAllocUnits) {
		sdev->support_tw_lu = true;
		dev_info(hba->dev, "%s: LU %d supports tw, twbuf unit : 0x%x\n",
				__func__, (int)sdev->lun, dLUNumTurboWriteBufferAllocUnits);
	} else
		sdev->support_tw_lu = false;
}
#endif

/*
 * ufshcd_get_lu_wp - returns the "b_lu_write_protect" from UNIT DESCRIPTOR
 * @hba: per-adapter instance
 * @lun: UFS device lun id
 * @b_lu_write_protect: pointer to buffer to hold the LU's write protect info
 *
 * Returns 0 in case of success and b_lu_write_protect status would be returned
 * @b_lu_write_protect parameter.
 * Returns -ENOTSUPP if reading b_lu_write_protect is not supported.
 * Returns -EINVAL in case of invalid parameters passed to this function.
 */
static int ufshcd_get_lu_wp(struct ufs_hba *hba,
			    u8 lun,
			    u8 *b_lu_write_protect)
{
	int ret;

	if (!b_lu_write_protect)
		ret = -EINVAL;
	/*
	 * According to UFS device spec, RPMB LU can't be write
	 * protected so skip reading bLUWriteProtect parameter for
	 * it. For other W-LUs, UNIT DESCRIPTOR is not available.
	 */
	else if (lun >= UFS_UPIU_MAX_GENERAL_LUN)
		ret = -ENOTSUPP;
	else
		ret = ufshcd_read_unit_desc_param(hba,
					  lun,
					  UNIT_DESC_PARAM_LU_WR_PROTECT,
					  b_lu_write_protect,
					  sizeof(*b_lu_write_protect));
	return ret;
}

#ifdef CONFIG_QCOM_WB
/*
 * ufshcd_get_wb_alloc_units - returns "dLUNumWriteBoosterBufferAllocUnits"
 * @hba: per-adapter instance
 * @lun: UFS device lun id
 * @d_lun_wbb_au: pointer to buffer to hold the LU's alloc units info
 *
 * Returns 0 in case of success and d_lun_wbb_au would be returned
 * Returns -ENOTSUPP if reading d_lun_wbb_au is not supported.
 * Returns -EINVAL in case of invalid parameters passed to this function.
 */
static int ufshcd_get_wb_alloc_units(struct ufs_hba *hba,
			    u8 lun,
			    u8 *d_lun_wbb_au)
{
	int ret;

	if (!d_lun_wbb_au)
		ret = -EINVAL;

	/* WB can be supported only from LU0..LU7 */
	else if (lun >= UFS_UPIU_MAX_GENERAL_LUN)
		ret = -ENOTSUPP;
	else
		ret = ufshcd_read_unit_desc_param(hba,
					  lun,
					  UNIT_DESC_PARAM_WB_BUF_ALLOC_UNITS,
					  d_lun_wbb_au,
					  sizeof(*d_lun_wbb_au));
	return ret;
}
#endif

/**
 * ufshcd_get_lu_power_on_wp_status - get LU's power on write protect
 * status
 * @hba: per-adapter instance
 * @sdev: pointer to SCSI device
 *
 */
static inline void ufshcd_get_lu_power_on_wp_status(struct ufs_hba *hba,
						    struct scsi_device *sdev)
{
	if (hba->dev_info.f_power_on_wp_en &&
	    !hba->dev_info.is_lu_power_on_wp) {
		u8 b_lu_write_protect;

		if (!ufshcd_get_lu_wp(hba, ufshcd_scsi_to_upiu_lun(sdev->lun),
				      &b_lu_write_protect) &&
		    (b_lu_write_protect == UFS_LU_POWER_ON_WP))
			hba->dev_info.is_lu_power_on_wp = true;
	}
}

/**
 * ufshcd_slave_alloc - handle initial SCSI device configurations
 * @sdev: pointer to SCSI device
 *
 * Returns success
 */
static int ufshcd_slave_alloc(struct scsi_device *sdev)
{
	struct ufs_hba *hba;

	hba = shost_priv(sdev->host);

	/* Mode sense(6) is not supported by UFS, so use Mode sense(10) */
	sdev->use_10_for_ms = 1;

	/* allow SCSI layer to restart the device in case of errors */
	sdev->allow_restart = 1;

	/* REPORT SUPPORTED OPERATION CODES is not supported */
	sdev->no_report_opcodes = 1;

	/* WRITE_SAME command is not supported*/
	sdev->no_write_same = 1;

	ufshcd_set_queue_depth(sdev);

	ufshcd_get_lu_power_on_wp_status(hba, sdev);

	ufshcd_get_bootlunID(sdev);
#ifdef CONFIG_BLK_TURBO_WRITE
	ufshcd_get_twbuf_unit(sdev);
#endif
	return 0;
}

/**
 * ufshcd_change_queue_depth - change queue depth
 * @sdev: pointer to SCSI device
 * @depth: required depth to set
 *
 * Change queue depth and make sure the max. limits are not crossed.
 */
static int ufshcd_change_queue_depth(struct scsi_device *sdev, int depth)
{
	struct ufs_hba *hba = shost_priv(sdev->host);

	if (depth > hba->nutrs)
		depth = hba->nutrs;
	return scsi_change_queue_depth(sdev, depth);
}

/**
 * ufshcd_slave_configure - adjust SCSI device configurations
 * @sdev: pointer to SCSI device
 */
static int ufshcd_slave_configure(struct scsi_device *sdev)
{
	struct ufs_hba *hba = shost_priv(sdev->host);
	struct request_queue *q = sdev->request_queue;

#if defined(CONFIG_UFSFEATURE)
	struct ufsf_feature *ufsf = &hba->ufsf;

	if (ufsf_is_valid_lun(sdev->lun)) {
		ufsf->sdev_ufs_lu[sdev->lun] = sdev;
		ufsf->slave_conf_cnt++;
		printk(KERN_ERR "%s: ufsfeature set lun %d sdev %p q %p\n",
				__func__, (int)sdev->lun, sdev, sdev->request_queue);
	}
#endif
	blk_queue_update_dma_pad(q, PRDT_DATA_BYTE_COUNT_PAD - 1);
	blk_queue_max_segment_size(q, PRDT_DATA_BYTE_COUNT_MAX);

	if (hba->scsi_cmd_timeout) {
		blk_queue_rq_timeout(q, hba->scsi_cmd_timeout * HZ);
		scsi_set_cmd_timeout_override(sdev, hba->scsi_cmd_timeout * HZ);
	}

	sdev->autosuspend_delay = UFSHCD_AUTO_SUSPEND_DELAY_MS;
	sdev->use_rpm_auto = 1;

	ufshcd_crypto_setup_rq_keyslot_manager(hba, q);

	return 0;
}

/**
 * ufshcd_slave_destroy - remove SCSI device configurations
 * @sdev: pointer to SCSI device
 */
static void ufshcd_slave_destroy(struct scsi_device *sdev)
{
	struct ufs_hba *hba;
	struct request_queue *q = sdev->request_queue;

	hba = shost_priv(sdev->host);
	/* Drop the reference as it won't be needed anymore */
	if (ufshcd_scsi_to_upiu_lun(sdev->lun) == UFS_UPIU_UFS_DEVICE_WLUN) {
		unsigned long flags;

		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->sdev_ufs_device = NULL;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}

	ufshcd_crypto_destroy_rq_keyslot_manager(hba, q);
}

/**
 * ufshcd_task_req_compl - handle task management request completion
 * @hba: per adapter instance
 * @index: index of the completed request
 * @resp: task management service response
 *
 * Returns non-zero value on error, zero on success
 */
static int ufshcd_task_req_compl(struct ufs_hba *hba, u32 index, u8 *resp)
{
	struct utp_task_req_desc *task_req_descp;
	struct utp_upiu_task_rsp *task_rsp_upiup;
	unsigned long flags;
	int ocs_value;
	int task_result;

	spin_lock_irqsave(hba->host->host_lock, flags);

	/* Clear completed tasks from outstanding_tasks */
	__clear_bit(index, &hba->outstanding_tasks);

	task_req_descp = hba->utmrdl_base_addr;
	ocs_value = ufshcd_get_tmr_ocs(&task_req_descp[index]);

	if (ocs_value == OCS_SUCCESS) {
		task_rsp_upiup = (struct utp_upiu_task_rsp *)
				task_req_descp[index].task_rsp_upiu;
		task_result = be32_to_cpu(task_rsp_upiup->output_param1);
		task_result = task_result & MASK_TM_SERVICE_RESP;
		if (resp)
			*resp = (u8)task_result;
	} else {
		dev_err(hba->dev, "%s: failed, ocs = 0x%x\n",
				__func__, ocs_value);
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return ocs_value;
}

/**
 * ufshcd_scsi_cmd_status - Update SCSI command result based on SCSI status
 * @lrbp: pointer to local reference block of completed command
 * @scsi_status: SCSI command status
 *
 * Returns value base on SCSI command status
 */
static inline int
ufshcd_scsi_cmd_status(struct ufshcd_lrb *lrbp, int scsi_status)
{
	int result = 0;

	switch (scsi_status) {
	case SAM_STAT_CHECK_CONDITION:
		ufshcd_copy_sense_data(lrbp);
	case SAM_STAT_GOOD:
		result |= DID_OK << 16 |
			  COMMAND_COMPLETE << 8 |
			  scsi_status;
		break;
	case SAM_STAT_TASK_SET_FULL:
	case SAM_STAT_BUSY:
	case SAM_STAT_TASK_ABORTED:
		ufshcd_copy_sense_data(lrbp);
		result |= scsi_status;
		break;
	default:
		result |= DID_ERROR << 16;
		break;
	} /* end of switch */

	return result;
}

/**
 * ufshcd_transfer_rsp_status - Get overall status of the response
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block of completed command
 *
 * Returns result of the command to notify SCSI midlayer
 */
static inline int
ufshcd_transfer_rsp_status(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	int result = 0;
	int scsi_status;
	int ocs;
	bool print_prdt;

	/* overall command status of utrd */
	ocs = ufshcd_get_tr_ocs(lrbp);

	switch (ocs) {
	case OCS_SUCCESS:
		result = ufshcd_get_req_rsp(lrbp->ucd_rsp_ptr);
		hba->ufs_stats.last_hibern8_exit_tstamp = ktime_set(0, 0);
		switch (result) {
		case UPIU_TRANSACTION_RESPONSE:
			/*
			 * get the response UPIU result to extract
			 * the SCSI command status
			 */
			result = ufshcd_get_rsp_upiu_result(lrbp->ucd_rsp_ptr);

			/*
			 * get the result based on SCSI status response
			 * to notify the SCSI midlayer of the command status
			 */
			scsi_status = result & MASK_SCSI_STATUS;
			result = ufshcd_scsi_cmd_status(lrbp, scsi_status);

			if (hba->dev_info.quirks & UFS_DEVICE_QUIRK_SUPPORT_QUERY_FATAL_MODE) {
				if (scsi_status == SAM_STAT_CHECK_CONDITION) {
					int sense_key = (0x0F & lrbp->sense_buffer[2]);

					dev_err(hba->dev, "%s: CHECK CONDITION sense key = %d",
							__func__, sense_key);
					if ((sense_key == MEDIUM_ERROR) && !hba->UFS_fatal_mode_done)
						schedule_work(&hba->fatal_mode_work);
				}
			}

			/*
			 * Currently we are only supporting BKOPs exception
			 * events hence we can ignore BKOPs exception event
			 * during power management callbacks. BKOPs exception
			 * event is not expected to be raised in runtime suspend
			 * callback as it allows the urgent bkops.
			 * During system suspend, we are anyway forcefully
			 * disabling the bkops and if urgent bkops is needed
			 * it will be enabled on system resume. Long term
			 * solution could be to abort the system suspend if
			 * UFS device needs urgent BKOPs.
			 */
			if (!hba->pm_op_in_progress &&
			    ufshcd_is_exception_event(lrbp->ucd_rsp_ptr) &&
			    scsi_host_in_recovery(hba->host)) {
				/*
				 * Prevent suspend once eeh_work is scheduled
				 * to avoid deadlock between ufshcd_suspend
				 * and exception event handler.
				 */
				if (queue_work(hba->recovery_wq,
							&hba->eeh_work))
					pm_runtime_get_noresume(hba->dev);
				dev_err(hba->dev, "execption event reported\n");
			}
#if defined(CONFIG_UFSFEATURE)
			if (scsi_status == SAM_STAT_GOOD)
				ufsf_hpb_noti_rb(&hba->ufsf, lrbp);
#endif
			break;
		case UPIU_TRANSACTION_REJECT_UPIU:
			/* TODO: handle Reject UPIU Response */
			result = DID_ERROR << 16;
			dev_err(hba->dev,
				"Reject UPIU not fully implemented\n");
			break;
		default:
			result = DID_ERROR << 16;
			dev_err(hba->dev,
				"Unexpected request response code = %x\n",
				result);
			break;
		}
		break;
	case OCS_ABORTED:
		result |= DID_ABORT << 16;
		break;
	case OCS_INVALID_COMMAND_STATUS:
		result |= DID_REQUEUE << 16;
		break;
	case OCS_INVALID_CMD_TABLE_ATTR:
	case OCS_INVALID_PRDT_ATTR:
	case OCS_MISMATCH_DATA_BUF_SIZE:
	case OCS_MISMATCH_RESP_UPIU_SIZE:
	case OCS_PEER_COMM_FAILURE:
	case OCS_FATAL_ERROR:
	case OCS_DEVICE_FATAL_ERROR:
	case OCS_INVALID_CRYPTO_CONFIG:
	case OCS_GENERAL_CRYPTO_ERROR:
	default:
		result |= DID_ERROR << 16;
		dev_err(hba->dev,
				"OCS error from controller = %x for tag %d\n",
				ocs, lrbp->task_tag);
		/*
		 * This is called in interrupt context, hence avoid sleep
		 * while printing debug registers. Also print only the minimum
		 * debug registers needed to debug OCS failure.
		 */
		__ufshcd_print_host_regs(hba, true);
		ufshcd_print_host_state(hba);
		break;
	} /* end of switch */

	if ((host_byte(result) != DID_OK) && !hba->silence_err_logs) {
		print_prdt = (ocs == OCS_INVALID_PRDT_ATTR ||
			ocs == OCS_MISMATCH_DATA_BUF_SIZE);
		ufshcd_print_trs(hba, 1 << lrbp->task_tag, print_prdt);
	}

	if ((host_byte(result) == DID_ERROR) ||
	    (host_byte(result) == DID_ABORT))
		ufsdbg_set_err_state(hba);

	return result;
}

/**
 * ufshcd_uic_cmd_compl - handle completion of uic command
 * @hba: per adapter instance
 * @intr_status: interrupt status generated by the controller
 *
 * Returns
 *  IRQ_HANDLED - If interrupt is valid
 *  IRQ_NONE    - If invalid interrupt
 */
static irqreturn_t ufshcd_uic_cmd_compl(struct ufs_hba *hba, u32 intr_status)
{
	irqreturn_t retval = IRQ_NONE;

	if ((intr_status & UIC_COMMAND_COMPL) && hba->active_uic_cmd) {
		hba->active_uic_cmd->argument2 |=
			ufshcd_get_uic_cmd_result(hba);
		hba->active_uic_cmd->argument3 =
			ufshcd_get_dme_attr_val(hba);
		complete(&hba->active_uic_cmd->done);
		retval = IRQ_HANDLED;
	}

	if (intr_status & UFSHCD_UIC_PWR_MASK) {
		if (hba->uic_async_done) {
			complete(hba->uic_async_done);
			retval = IRQ_HANDLED;
		} else if (ufshcd_is_auto_hibern8_enabled(hba)) {
			/*
			 * If uic_async_done flag is not set then this
			 * is an Auto hibern8 err interrupt.
			 * Perform a host reset followed by a full
			 * link recovery.
			 */
			hba->ufshcd_state = UFSHCD_STATE_ERROR;
			hba->force_host_reset = true;
			dev_err(hba->dev, "%s: Auto Hibern8 %s failed - status: 0x%08x, upmcrs: 0x%08x\n",
				__func__, (intr_status & UIC_HIBERNATE_ENTER) ?
				"Enter" : "Exit",
				intr_status, ufshcd_get_upmcrs(hba));
			/*
			 * It is possible to see auto-h8 errors during card
			 * removal, so set this flag and let the error handler
			 * decide if this error is seen while card was present
			 * or due to card removal.
			 * If error is seen during card removal, we don't want
			 * to printout the debug messages.
			 */
			hba->auto_h8_err = true;
			queue_work(hba->recovery_wq, &hba->eh_work);
			retval = IRQ_HANDLED;
		}
	}
	return retval;
}

/**
 * __ufshcd_transfer_req_compl - handle SCSI and query command completion
 * @hba: per adapter instance
 * @completed_reqs: requests to complete
 */
static void __ufshcd_transfer_req_compl(struct ufs_hba *hba,
					unsigned long completed_reqs)
{
	struct ufshcd_lrb *lrbp;
	struct scsi_cmnd *cmd;
	int result;
	int index;
#if defined(CONFIG_UFS_DATA_LOG)
#if defined(CONFIG_UFS_DATA_LOG_MAGIC_CODE)
	struct scatterlist *sg;
	int sg_segments;
	unsigned long magicword_rb = 0;
#endif
	int i = 0;
	int cpu = raw_smp_processor_id();
	unsigned int dump_index;
#endif

	for_each_set_bit(index, &completed_reqs, hba->nutrs) {
		lrbp = &hba->lrb[index];
		cmd = lrbp->cmd;
		if (cmd) {
			ufshcd_cond_add_cmd_trace(hba, index, "scsi_cmpl");
			ufshcd_update_tag_stats_completion(hba, cmd);
			result = ufshcd_transfer_rsp_status(hba, lrbp);
			scsi_dma_unmap(cmd);

#ifdef CONFIG_BLK_TURBO_WRITE
			if (ufshcd_is_tw_on(hba) && (rq_data_dir(cmd->request) == WRITE)) {
				int transfer_len = 0;
				transfer_len = be32_to_cpu(lrbp->ucd_req_ptr->sc.exp_data_transfer_len);
				SEC_ufs_update_tw_info(hba, transfer_len);
			}
#endif
#if defined(CONFIG_UFSFEATURE) && defined(CONFIG_UFSHPB)
			if (hba->ufsf.hpb_dev_info.hpb_device && (cmd->cmnd[0] == READ_16)) {
				int transfer_len = 0;
				transfer_len = be32_to_cpu(lrbp->ucd_req_ptr->sc.exp_data_transfer_len);
				SEC_ufs_update_hpb_info(hba, transfer_len);
			}
#endif
#if defined(CONFIG_UFS_DATA_LOG)
			if (cmd->request && hba->host->ufs_sys_log_en){
				/*condition of data log*/
				if (rq_data_dir(cmd->request) == READ &&
						cmd->request->__sector >= hba->host->ufs_system_start &&
						cmd->request->__sector < hba->host->ufs_system_end) {
					dump_index = queuing_req[cmd->request->tag];
					queuing_req[cmd->request->tag] = 0;
					ufs_data_log[dump_index].end_time = cpu_clock(cpu);
					sg_segments = scsi_sg_count(cmd);
					ufs_data_log[dump_index].segments_cnt = sg_segments;
					ufs_data_log[dump_index].done = 0xF;

#if defined(CONFIG_UFS_DATA_LOG_MAGIC_CODE)
					scsi_for_each_sg(cmd, sg, sg_segments, i) {
						/*
						 * Attempt to SG element for check magic word.
						 * If magic word should have been overwritten in read case,
						 * memory location will not be updated.
						 */

						ufs_data_log[dump_index].virt_addr = sg_virt(sg);
						memcpy(&ufs_data_log[dump_index].datbuf, sg_virt(sg), UFS_DATA_BUF_SIZE);
						memcpy(&magicword_rb,  sg_virt(sg), UFS_DATA_BUF_SIZE);
						if (magicword_rb == 0x1F5E3A7069245CBE) {
							printk(KERN_ALERT "tag%d:sg[%d]:%p: page_link=%lx, offset=%d, length=%d\n",
									index, i, sg, sg->page_link, sg->offset, sg->length);
						}
					}
#endif
				}
			}
#endif

			cmd->result = result;
			lrbp->compl_time_stamp = ktime_get();
			update_req_stats(hba, lrbp);
			ufshcd_complete_lrbp_crypto(hba, cmd, lrbp);
			/* Mark completed command as NULL in LRB */
			lrbp->cmd = NULL;
			hba->ufs_stats.clk_rel.ctx = XFR_REQ_COMPL;
			if (cmd->request) {
				/*
				 * As we are accessing the "request" structure,
				 * this must be called before calling
				 * ->scsi_done() callback.
				 */
				ufshcd_vops_pm_qos_req_end(hba, cmd->request,
					false);
			}

			clear_bit_unlock(index, &hba->lrb_in_use);
			/*
			 *__ufshcd_release and __ufshcd_hibern8_release is
			 * called after clear_bit_unlock so that
			 * these function can be called with updated state of
			 * lrb_in_use flag so that for last transfer req
			 * completion, gate and hibernate work function would
			 * be called to gate the clock and put the link in
			 * hibern8 state.
			 */
			__ufshcd_release(hba, false);
			__ufshcd_hibern8_release(hba, false);

			/* Do not touch lrbp after scsi done */
			cmd->scsi_done(cmd);
		} else if (lrbp->command_type == UTP_CMD_TYPE_DEV_MANAGE ||
			lrbp->command_type == UTP_CMD_TYPE_UFS_STORAGE) {
			lrbp->compl_time_stamp = ktime_get();
			if (hba->dev_cmd.complete) {
				ufshcd_cond_add_cmd_trace(hba, index,
						"dev_cmd_cmpl");
				complete(hba->dev_cmd.complete);
			}
		}
		if (ufshcd_is_clkscaling_supported(hba))
			hba->clk_scaling.active_reqs--;
	}

	/* clear corresponding bits of completed commands */
	hba->outstanding_reqs ^= completed_reqs;

	ufshcd_clk_scaling_update_busy(hba);

	/* we might have free'd some tags above */
	wake_up(&hba->dev_cmd.tag_wq);
}

/**
 * ufshcd_abort_outstanding_requests - abort all outstanding transfer requests.
 * @hba: per adapter instance
 * @result: error result to inform scsi layer about
 */
void ufshcd_abort_outstanding_transfer_requests(struct ufs_hba *hba, int result)
{
	u8 index;
	struct ufshcd_lrb *lrbp;
	struct scsi_cmnd *cmd;

	if (!hba->outstanding_reqs)
		return;

	for_each_set_bit(index, &hba->outstanding_reqs, hba->nutrs) {
		lrbp = &hba->lrb[index];
		cmd = lrbp->cmd;
		if (cmd) {
			ufshcd_cond_add_cmd_trace(hba, index, "scsi_failed");
			ufshcd_update_error_stats(hba,
					UFS_ERR_INT_FATAL_ERRORS);
			scsi_dma_unmap(cmd);
			cmd->result = result;
			/* Clear pending transfer requests */
			ufshcd_clear_cmd(hba, index);
			ufshcd_outstanding_req_clear(hba, index);
			lrbp->compl_time_stamp = ktime_get();
			update_req_stats(hba, lrbp);
			/* Mark completed command as NULL in LRB */
			lrbp->cmd = NULL;
			if (cmd->request) {
				/*
				 * As we are accessing the "request" structure,
				 * this must be called before calling
				 * ->scsi_done() callback.
				 */
				ufshcd_vops_pm_qos_req_end(hba, cmd->request,
					true);
			}
			clear_bit_unlock(index, &hba->lrb_in_use);

			/*
			 * ufshcd_release_all is called after clear_bit_unlock
			 * so that the function can be called with updated state
			 * of lrb_in_use flag so that for last abort req
			 * completion, gate and hibernate work function would
			 * be called to gate the clock and put the link in
			 * hibern8 state.
			 */
			 ufshcd_release_all(hba);

			/* Do not touch lrbp after scsi done */
			cmd->scsi_done(cmd);
		} else if (lrbp->command_type == UTP_CMD_TYPE_DEV_MANAGE) {
			lrbp->compl_time_stamp = ktime_get();
			if (hba->dev_cmd.complete) {
				ufshcd_cond_add_cmd_trace(hba, index,
							"dev_cmd_failed");
				ufshcd_outstanding_req_clear(hba, index);
				complete(hba->dev_cmd.complete);
			}
		}
		if (ufshcd_is_clkscaling_supported(hba))
			hba->clk_scaling.active_reqs--;
	}
}

/**
 * ufshcd_transfer_req_compl - handle SCSI and query command completion
 * @hba: per adapter instance
 *
 * Returns
 *  IRQ_HANDLED - If interrupt is valid
 *  IRQ_NONE    - If invalid interrupt
 */
static irqreturn_t ufshcd_transfer_req_compl(struct ufs_hba *hba)
{
	unsigned long completed_reqs;
	u32 tr_doorbell;

	/* Resetting interrupt aggregation counters first and reading the
	 * DOOR_BELL afterward allows us to handle all the completed requests.
	 * In order to prevent other interrupts starvation the DB is read once
	 * after reset. The down side of this solution is the possibility of
	 * false interrupt if device completes another request after resetting
	 * aggregation and before reading the DB.
	 */
	if (ufshcd_is_intr_aggr_allowed(hba) &&
	    !(hba->quirks & UFSHCI_QUIRK_SKIP_RESET_INTR_AGGR))
		ufshcd_reset_intr_aggr(hba);

	tr_doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	completed_reqs = tr_doorbell ^ hba->outstanding_reqs;

	if (completed_reqs) {
		__ufshcd_transfer_req_compl(hba, completed_reqs);
#if defined(CONFIG_UFSFEATURE)
		ufsf_hpb_wakeup_worker_on_idle(&hba->ufsf);
#endif
		return IRQ_HANDLED;
	} else {
		return IRQ_NONE;
	}
}

/**
 * ufshcd_disable_ee - disable exception event
 * @hba: per-adapter instance
 * @mask: exception event to disable
 *
 * Disables exception event in the device so that the EVENT_ALERT
 * bit is not set.
 *
 * Returns zero on success, non-zero error value on failure.
 */
static int ufshcd_disable_ee(struct ufs_hba *hba, u16 mask)
{
	int err = 0;
	u32 val;

	if (!(hba->ee_ctrl_mask & mask))
		goto out;

	val = hba->ee_ctrl_mask & ~mask;
	val &= MASK_EE_STATUS;
	err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
			QUERY_ATTR_IDN_EE_CONTROL, 0, 0, &val);
	if (!err)
		hba->ee_ctrl_mask &= ~mask;
out:
	return err;
}

/**
 * ufshcd_enable_ee - enable exception event
 * @hba: per-adapter instance
 * @mask: exception event to enable
 *
 * Enable corresponding exception event in the device to allow
 * device to alert host in critical scenarios.
 *
 * Returns zero on success, non-zero error value on failure.
 */
static int ufshcd_enable_ee(struct ufs_hba *hba, u16 mask)
{
	int err = 0;
	u32 val;

	if (hba->ee_ctrl_mask & mask)
		goto out;

	val = hba->ee_ctrl_mask | mask;
	val &= MASK_EE_STATUS;
	err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
			QUERY_ATTR_IDN_EE_CONTROL, 0, 0, &val);
	if (!err)
		hba->ee_ctrl_mask |= mask;
out:
	return err;
}

/**
 * ufshcd_enable_auto_bkops - Allow device managed BKOPS
 * @hba: per-adapter instance
 *
 * Allow device to manage background operations on its own. Enabling
 * this might lead to inconsistent latencies during normal data transfers
 * as the device is allowed to manage its own way of handling background
 * operations.
 *
 * Returns zero on success, non-zero on failure.
 */
static int ufshcd_enable_auto_bkops(struct ufs_hba *hba)
{
	int err = 0;

	if (hba->auto_bkops_enabled)
		goto out;

	err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG,
			QUERY_FLAG_IDN_BKOPS_EN, NULL);
	if (err) {
		dev_err(hba->dev, "%s: failed to enable bkops %d\n",
				__func__, err);
		goto out;
	}

	hba->auto_bkops_enabled = true;
	trace_ufshcd_auto_bkops_state(dev_name(hba->dev), 1);

	/* No need of URGENT_BKOPS exception from the device */
	err = ufshcd_disable_ee(hba, MASK_EE_URGENT_BKOPS);
	if (err)
		dev_err(hba->dev, "%s: failed to disable exception event %d\n",
				__func__, err);
out:
	return err;
}

/**
 * ufshcd_disable_auto_bkops - block device in doing background operations
 * @hba: per-adapter instance
 *
 * Disabling background operations improves command response latency but
 * has drawback of device moving into critical state where the device is
 * not-operable. Make sure to call ufshcd_enable_auto_bkops() whenever the
 * host is idle so that BKOPS are managed effectively without any negative
 * impacts.
 *
 * Returns zero on success, non-zero on failure.
 */
static int ufshcd_disable_auto_bkops(struct ufs_hba *hba)
{
	int err = 0;

	if (!hba->auto_bkops_enabled)
		goto out;

	/*
	 * If host assisted BKOPs is to be enabled, make sure
	 * urgent bkops exception is allowed.
	 */
	err = ufshcd_enable_ee(hba, MASK_EE_URGENT_BKOPS);
	if (err) {
		dev_err(hba->dev, "%s: failed to enable exception event %d\n",
				__func__, err);
		goto out;
	}

	err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG,
			QUERY_FLAG_IDN_BKOPS_EN, NULL);
	if (err) {
		dev_err(hba->dev, "%s: failed to disable bkops %d\n",
				__func__, err);
		ufshcd_disable_ee(hba, MASK_EE_URGENT_BKOPS);
		goto out;
	}

	hba->auto_bkops_enabled = false;
	trace_ufshcd_auto_bkops_state(dev_name(hba->dev), 0);
out:
	return err;
}

/**
 * ufshcd_force_reset_auto_bkops - force reset auto bkops state
 * @hba: per adapter instance
 *
 * After a device reset the device may toggle the BKOPS_EN flag
 * to default value. The s/w tracking variables should be updated
 * as well. This function would change the auto-bkops state based on
 * UFSHCD_CAP_KEEP_AUTO_BKOPS_ENABLED_EXCEPT_SUSPEND.
 */
static void ufshcd_force_reset_auto_bkops(struct ufs_hba *hba)
{
	if (ufshcd_keep_autobkops_enabled_except_suspend(hba)) {
		hba->auto_bkops_enabled = false;
		hba->ee_ctrl_mask |= MASK_EE_URGENT_BKOPS;
		ufshcd_enable_auto_bkops(hba);
	} else {
		hba->auto_bkops_enabled = true;
		hba->ee_ctrl_mask &= ~MASK_EE_URGENT_BKOPS;
		ufshcd_disable_auto_bkops(hba);
	}
}

static inline int ufshcd_get_bkops_status(struct ufs_hba *hba, u32 *status)
{
	return ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_BKOPS_STATUS, 0, 0, status);
}

/**
 * ufshcd_bkops_ctrl - control the auto bkops based on current bkops status
 * @hba: per-adapter instance
 * @status: bkops_status value
 *
 * Read the bkops_status from the UFS device and Enable fBackgroundOpsEn
 * flag in the device to permit background operations if the device
 * bkops_status is greater than or equal to "status" argument passed to
 * this function, disable otherwise.
 *
 * Returns 0 for success, non-zero in case of failure.
 *
 * NOTE: Caller of this function can check the "hba->auto_bkops_enabled" flag
 * to know whether auto bkops is enabled or disabled after this function
 * returns control to it.
 */
static int ufshcd_bkops_ctrl(struct ufs_hba *hba,
			     enum bkops_status status)
{
	int err;
	u32 curr_status = 0;

	err = ufshcd_get_bkops_status(hba, &curr_status);
	if (err) {
		dev_err(hba->dev, "%s: failed to get BKOPS status %d\n",
				__func__, err);
		goto out;
	} else if (curr_status > BKOPS_STATUS_MAX) {
		dev_err(hba->dev, "%s: invalid BKOPS status %d\n",
				__func__, curr_status);
		err = -EINVAL;
		goto out;
	}

	if (curr_status >= status) {
		err = ufshcd_enable_auto_bkops(hba);
		dev_info(hba->dev, "%s: auto_bkops %sabled, ret %d, status : %d\n",
				__func__, (curr_status >= status) ? "en" : "dis",
				err, curr_status);
	} else
		err = ufshcd_disable_auto_bkops(hba);
out:
	return err;
}

/**
 * ufshcd_urgent_bkops - handle urgent bkops exception event
 * @hba: per-adapter instance
 *
 * Enable fBackgroundOpsEn flag in the device to permit background
 * operations.
 *
 * If BKOPs is enabled, this function returns 0, 1 if the bkops in not enabled
 * and negative error value for any other failure.
 */
static int ufshcd_urgent_bkops(struct ufs_hba *hba)
{
	return ufshcd_bkops_ctrl(hba, hba->urgent_bkops_lvl);
}

static inline int ufshcd_get_ee_status(struct ufs_hba *hba, u32 *status)
{
	return ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_EE_STATUS, 0, 0, status);
}

static void ufshcd_bkops_exception_event_handler(struct ufs_hba *hba)
{
	int err;
	u32 curr_status = 0;

	if (hba->is_urgent_bkops_lvl_checked)
		goto enable_auto_bkops;

	err = ufshcd_get_bkops_status(hba, &curr_status);
	if (err) {
		dev_err(hba->dev, "%s: failed to get BKOPS status %d\n",
				__func__, err);
		goto out;
	}
	else
		dev_info(hba->dev, "%s: urgent bkops(status:%d)\n",
				__func__, curr_status);

	/*
	 * We are seeing that some devices are raising the urgent bkops
	 * exception events even when BKOPS status doesn't indicate performace
	 * impacted or critical. Handle these device by determining their urgent
	 * bkops status at runtime.
	 */
	if (curr_status < BKOPS_STATUS_PERF_IMPACT) {
		dev_err(hba->dev, "%s: device raised urgent BKOPS exception for bkops status %d\n",
				__func__, curr_status);
		/* update the current status as the urgent bkops level */
		//hba->urgent_bkops_lvl = curr_status;
		//hba->is_urgent_bkops_lvl_checked = true;

		/* SEC does not follow this policy that BKOPS is enabled for these events */
		goto out;
	}

enable_auto_bkops:
	err = ufshcd_enable_auto_bkops(hba);
	if (!err)
		dev_info(hba->dev, "%s: auto bkops is enabled\n", __func__);
out:
	if (err < 0)
		dev_err(hba->dev, "%s: failed to handle urgent bkops %d\n",
				__func__, err);
}

#ifdef CONFIG_QCOM_WB
static bool ufshcd_wb_sup(struct ufs_hba *hba)
{
	return ((hba->dev_info.d_ext_ufs_feature_sup &
		   UFS_DEV_WRITE_BOOSTER_SUP) &&
		  (hba->dev_info.b_wb_buffer_type
		   || hba->dev_info.wb_config_lun));
}

static int ufshcd_wb_ctrl(struct ufs_hba *hba, bool enable)
{
	int ret;
	enum query_opcode opcode;

	if (!ufshcd_wb_sup(hba))
		return 0;

	if (enable)
		opcode = UPIU_QUERY_OPCODE_SET_FLAG;
	else
		opcode = UPIU_QUERY_OPCODE_CLEAR_FLAG;

	ret = ufshcd_query_flag_retry(hba, opcode,
				      QUERY_FLAG_IDN_WB_EN, NULL);
	if (ret) {
		dev_err(hba->dev, "%s write booster %s failed %d\n",
			__func__, enable ? "enable" : "disable", ret);
		return ret;
	}

	hba->wb_enabled = enable;
	dev_dbg(hba->dev, "%s write booster %s %d\n",
			__func__, enable ? "enable" : "disable", ret);

	return ret;
}

static int ufshcd_wb_toggle_flush_during_h8(struct ufs_hba *hba, bool set)
{
	int val;

	if (set)
		val =  UPIU_QUERY_OPCODE_SET_FLAG;
	else
		val = UPIU_QUERY_OPCODE_CLEAR_FLAG;

	return ufshcd_query_flag_retry(hba, val,
			       QUERY_FLAG_IDN_WB_BUFF_FLUSH_DURING_HIBERN8,
				       NULL);
}

static int ufshcd_wb_buf_flush_enable(struct ufs_hba *hba)
{
	int ret;

	if (!ufshcd_wb_sup(hba) || hba->wb_buf_flush_enabled)
		return 0;

	ret = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG,
				      QUERY_FLAG_IDN_WB_BUFF_FLUSH_EN, NULL);
	if (ret)
		dev_err(hba->dev, "%s WB - buf flush enable failed %d\n",
			__func__, ret);
	else
		hba->wb_buf_flush_enabled = true;

	dev_dbg(hba->dev, "WB - Flush enabled: %d\n", ret);
	return ret;
}

static int ufshcd_wb_buf_flush_disable(struct ufs_hba *hba)
{
	int ret;

	if (!ufshcd_wb_sup(hba) || !hba->wb_buf_flush_enabled)
		return 0;

	ret = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG,
				      QUERY_FLAG_IDN_WB_BUFF_FLUSH_EN, NULL);
	if (ret) {
		dev_warn(hba->dev, "%s: WB - buf flush disable failed %d\n",
			__func__, ret);
	} else {
		hba->wb_buf_flush_enabled = false;
		dev_dbg(hba->dev, "WB - Flush disabled: %d\n", ret);
	}

	return ret;
}

static bool ufshcd_wb_is_buf_flush_needed(struct ufs_hba *hba)
{
	int ret;
	u32 cur_buf, status, avail_buf;

	if (!ufshcd_wb_sup(hba))
		return false;

	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
				      QUERY_ATTR_IDN_AVAIL_WB_BUFF_SIZE,
				      0, 0, &avail_buf);
	if (ret) {
		dev_warn(hba->dev, "%s dAvailableWriteBoosterBufferSize read failed %d\n",
			 __func__, ret);
		return false;
	}

	ret = ufshcd_vops_get_user_cap_mode(hba);
	if (ret <= 0) {
		dev_dbg(hba->dev, "Get user-cap reduction mode: failed: %d\n",
			ret);
		/* Most commonly used */
		ret = UFS_WB_BUFF_PRESERVE_USER_SPACE;
	}

	hba->dev_info.keep_vcc_on = false;
	if (ret == UFS_WB_BUFF_USER_SPACE_RED_EN) {
		if (avail_buf <= UFS_WB_10_PERCENT_BUF_REMAIN) {
			hba->dev_info.keep_vcc_on = true;
			return true;
		}
		return false;
	} else if (ret == UFS_WB_BUFF_PRESERVE_USER_SPACE) {
		ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
					      QUERY_ATTR_IDN_CURR_WB_BUFF_SIZE,
					      0, 0, &cur_buf);
		if (ret) {
			dev_err(hba->dev, "%s dCurWriteBoosterBufferSize read failed %d\n",
				 __func__, ret);
			return false;
		}

		if (!cur_buf) {
			dev_info(hba->dev, "dCurWBBuf: %d WB disabled until free-space is available\n",
				 cur_buf);
			return false;
		}

		ret = ufshcd_get_ee_status(hba, &status);
		if (ret) {
			dev_err(hba->dev, "%s: failed to get exception status %d\n",
				__func__, ret);
			if (avail_buf < UFS_WB_40_PERCENT_BUF_REMAIN) {
				hba->dev_info.keep_vcc_on = true;
				return true;
			}
			return false;
		}

		status &= hba->ee_ctrl_mask;

		if ((status & MASK_EE_URGENT_BKOPS) ||
		    (avail_buf < UFS_WB_40_PERCENT_BUF_REMAIN)) {
			hba->dev_info.keep_vcc_on = true;
			return true;
		}
	}
	return false;
}
#endif

/**
 * ufshcd_exception_event_handler - handle exceptions raised by device
 * @work: pointer to work data
 *
 * Read bExceptionEventStatus attribute from the device and handle the
 * exception event accordingly.
 */
static void ufshcd_exception_event_handler(struct work_struct *work)
{
	struct ufs_hba *hba;
	int err;
	u32 status = 0;
	hba = container_of(work, struct ufs_hba, eeh_work);

	pm_runtime_get_sync(hba->dev);
	ufshcd_scsi_block_requests(hba);
	err = ufshcd_get_ee_status(hba, &status);
	if (err) {
		dev_err(hba->dev, "%s: failed to get exception status %d\n",
				__func__, err);
		goto out;
	}

	status &= hba->ee_ctrl_mask;

	if (status & MASK_EE_URGENT_BKOPS)
		ufshcd_bkops_exception_event_handler(hba);

out:
	ufshcd_scsi_unblock_requests(hba);
	/*
	 * pm_runtime_get_noresume is called while scheduling
	 * eeh_work to avoid suspend racing with exception work.
	 * Hence decrement usage counter using pm_runtime_put_noidle
	 * to allow suspend on completion of exception event handler.
	 */
	pm_runtime_put_noidle(hba->dev);
	pm_runtime_put(hba->dev);
	return;
}

/* Complete requests that have door-bell cleared */
static void ufshcd_complete_requests(struct ufs_hba *hba)
{
	ufshcd_transfer_req_compl(hba);
	ufshcd_tmc_handler(hba);
}

/**
 * ufshcd_quirk_dl_nac_errors - This function checks if error handling is
 *				to recover from the DL NAC errors or not.
 * @hba: per-adapter instance
 *
 * Returns true if error handling is required, false otherwise
 */
static bool ufshcd_quirk_dl_nac_errors(struct ufs_hba *hba)
{
	unsigned long flags;
	bool err_handling = true;

	spin_lock_irqsave(hba->host->host_lock, flags);
	/*
	 * UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS only workaround the
	 * device fatal error and/or DL NAC & REPLAY timeout errors.
	 */
	if (hba->saved_err & (CONTROLLER_FATAL_ERROR | SYSTEM_BUS_FATAL_ERROR))
		goto out;

	if ((hba->saved_err & DEVICE_FATAL_ERROR) ||
	    ((hba->saved_err & UIC_ERROR) &&
	     (hba->saved_uic_err & UFSHCD_UIC_DL_TCx_REPLAY_ERROR))) {
		/*
		 * we have to do error recovery but atleast silence the error
		 * logs.
		 */
		hba->silence_err_logs = true;
		goto out;
	}

	if ((hba->saved_err & UIC_ERROR) &&
	    (hba->saved_uic_err & UFSHCD_UIC_DL_NAC_RECEIVED_ERROR)) {
		int err;
		/*
		 * wait for 50ms to see if we can get any other errors or not.
		 */
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		msleep(50);
		spin_lock_irqsave(hba->host->host_lock, flags);

		/*
		 * now check if we have got any other severe errors other than
		 * DL NAC error?
		 */
		if ((hba->saved_err & INT_FATAL_ERRORS) ||
		    ((hba->saved_err & UIC_ERROR) &&
		    (hba->saved_uic_err & ~UFSHCD_UIC_DL_NAC_RECEIVED_ERROR))) {
			if (((hba->saved_err & INT_FATAL_ERRORS) ==
				DEVICE_FATAL_ERROR) || (hba->saved_uic_err &
					~UFSHCD_UIC_DL_NAC_RECEIVED_ERROR))
				hba->silence_err_logs = true;
			goto out;
		}

		/*
		 * As DL NAC is the only error received so far, send out NOP
		 * command to confirm if link is still active or not.
		 *   - If we don't get any response then do error recovery.
		 *   - If we get response then clear the DL NAC error bit.
		 */

		/* silence the error logs from NOP command */
		hba->silence_err_logs = true;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		err = ufshcd_verify_dev_init(hba);
		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->silence_err_logs = false;

		if (err) {
			hba->silence_err_logs = true;
			goto out;
		}

		/* Link seems to be alive hence ignore the DL NAC errors */
		if (hba->saved_uic_err == UFSHCD_UIC_DL_NAC_RECEIVED_ERROR)
			hba->saved_err &= ~UIC_ERROR;
		/* clear NAC error */
		hba->saved_uic_err &= ~UFSHCD_UIC_DL_NAC_RECEIVED_ERROR;
		if (!hba->saved_uic_err) {
			err_handling = false;
			goto out;
		}
		/*
		 * there seems to be some errors other than NAC, so do error
		 * recovery
		 */
		hba->silence_err_logs = true;
	}
out:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	return err_handling;
}

/**
 * ufshcd_err_handler - handle UFS errors that require s/w attention
 * @work: pointer to work structure
 */
static void ufshcd_err_handler(struct work_struct *work)
{
	struct ufs_hba *hba;
	unsigned long flags;
	bool err_xfer = false, err_tm = false;
	int err = 0;
	int tag;
	bool needs_reset = false;
	bool clks_enabled = false;

	hba = container_of(work, struct ufs_hba, eh_work);

	spin_lock_irqsave(hba->host->host_lock, flags);
	ufsdbg_set_err_state(hba);

	if (hba->ufshcd_state == UFSHCD_STATE_RESET)
		goto out;

	/*
	 * Make sure the clocks are ON before we proceed with err
	 * handling. For the majority of cases err handler would be
	 * run with clocks ON. There is a possibility that the err
	 * handler was scheduled due to auto hibern8 error interrupt,
	 * in which case the clocks could be gated or be in the
	 * process of gating when the err handler runs.
	 */
	if (unlikely((hba->clk_gating.state != CLKS_ON) &&
	    (hba->clk_gating.state == REQ_CLKS_OFF &&
	     ufshcd_is_link_hibern8(hba)) &&
	     ufshcd_is_auto_hibern8_enabled(hba))) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		hba->ufs_stats.clk_hold.ctx = ERR_HNDLR_WORK;
		ufshcd_hold(hba, false);
		spin_lock_irqsave(hba->host->host_lock, flags);
		clks_enabled = true;
	}

	hba->ufshcd_state = UFSHCD_STATE_RESET;
	ufshcd_set_eh_in_progress(hba);

	/* Complete requests that have door-bell cleared by h/w */
	ufshcd_complete_requests(hba);

	if (hba->dev_info.quirks &
	    UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS) {
		bool ret;

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		/* release the lock as ufshcd_quirk_dl_nac_errors() may sleep */
		ret = ufshcd_quirk_dl_nac_errors(hba);
		spin_lock_irqsave(hba->host->host_lock, flags);
		if (!ret)
			goto skip_err_handling;
	}

	/*
	 * Dump controller state before resetting. Transfer requests state
	 * will be dump as part of the request completion.
	 */
	if ((hba->saved_err & (INT_FATAL_ERRORS | UIC_ERROR | UIC_LINK_LOST)) ||
	    hba->auto_h8_err) {
		dev_err(hba->dev, "%s: saved_err 0x%x saved_uic_err 0x%x\n",
			__func__, hba->saved_err, hba->saved_uic_err);
		if (!hba->silence_err_logs) {
			/* release lock as print host regs sleeps */
			spin_unlock_irqrestore(hba->host->host_lock, flags);
			ufshcd_print_host_regs(hba);
			ufshcd_print_host_state(hba);
			ufshcd_print_pwr_info(hba);
			ufshcd_print_tmrs(hba, hba->outstanding_tasks);
			ufshcd_print_cmd_log(hba);
			spin_lock_irqsave(hba->host->host_lock, flags);
		}
		hba->auto_h8_err = false;
	} else {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		ufshcd_print_cmd_log(hba);
		spin_lock_irqsave(hba->host->host_lock, flags);
	}

	if ((hba->saved_err & (INT_FATAL_ERRORS | UIC_LINK_LOST))
	    || hba->saved_ce_err || hba->force_host_reset ||
	    ((hba->saved_err & UIC_ERROR) &&
	    (hba->saved_uic_err & (UFSHCD_UIC_DL_PA_INIT_ERROR |
				   UFSHCD_UIC_DL_NAC_RECEIVED_ERROR |
				   UFSHCD_UIC_DL_TCx_REPLAY_ERROR))))
		needs_reset = true;

	/*
	 * if host reset is required then skip clearing the pending
	 * transfers forcefully because they will get cleared during
	 * host reset and restore
	 */
	if (needs_reset)
		goto skip_pending_xfer_clear;

	/* release lock as clear command might sleep */
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	/* Clear pending transfer requests */
	for_each_set_bit(tag, &hba->outstanding_reqs, hba->nutrs) {
		if (ufshcd_clear_cmd(hba, tag)) {
			err_xfer = true;
			goto lock_skip_pending_xfer_clear;
		}
	}

	/* Clear pending task management requests */
	for_each_set_bit(tag, &hba->outstanding_tasks, hba->nutmrs) {
		if (ufshcd_clear_tm_cmd(hba, tag)) {
			err_tm = true;
			goto lock_skip_pending_xfer_clear;
		}
	}

lock_skip_pending_xfer_clear:
	spin_lock_irqsave(hba->host->host_lock, flags);

	/* Complete the requests that are cleared by s/w */
	ufshcd_complete_requests(hba);

	if (err_xfer || err_tm)
		needs_reset = true;

skip_pending_xfer_clear:
	/* Fatal errors need reset */
	if (needs_reset) {
		unsigned long max_doorbells = (1UL << hba->nutrs) - 1;

		if (hba->saved_err & INT_FATAL_ERRORS)
			ufshcd_update_error_stats(hba,
						  UFS_ERR_INT_FATAL_ERRORS);
		if (hba->saved_ce_err)
			ufshcd_update_error_stats(hba, UFS_ERR_CRYPTO_ENGINE);

		if (hba->saved_err & UIC_ERROR)
			ufshcd_update_error_stats(hba,
						  UFS_ERR_INT_UIC_ERROR);

		if (err_xfer || err_tm)
			ufshcd_update_error_stats(hba,
						  UFS_ERR_CLEAR_PEND_XFER_TM);

		/*
		 * ufshcd_reset_and_restore() does the link reinitialization
		 * which will need atleast one empty doorbell slot to send the
		 * device management commands (NOP and query commands).
		 * If there is no slot empty at this moment then free up last
		 * slot forcefully.
		 */
		if (hba->outstanding_reqs == max_doorbells)
			__ufshcd_transfer_req_compl(hba,
						    (1UL << (hba->nutrs - 1)));

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		err = ufshcd_reset_and_restore(hba);
		spin_lock_irqsave(hba->host->host_lock, flags);
		if (err) {
			dev_err(hba->dev, "%s: reset and restore failed\n",
					__func__);
			hba->ufshcd_state = UFSHCD_STATE_ERROR;
		}
		/*
		 * Inform scsi mid-layer that we did reset and allow to handle
		 * Unit Attention properly.
		 */
		scsi_report_bus_reset(hba->host, 0);
		hba->saved_err = 0;
		hba->saved_uic_err = 0;
		hba->saved_ce_err = 0;
		hba->force_host_reset = false;
	}

skip_err_handling:
	if (!needs_reset) {
		hba->ufshcd_state = UFSHCD_STATE_OPERATIONAL;
		if (hba->saved_err || hba->saved_uic_err)
			dev_err_ratelimited(hba->dev, "%s: exit: saved_err 0x%x saved_uic_err 0x%x",
			    __func__, hba->saved_err, hba->saved_uic_err);
	}

	hba->silence_err_logs = false;

	if (clks_enabled) {
		__ufshcd_release(hba, false);
		hba->ufs_stats.clk_rel.ctx = ERR_HNDLR_WORK;
	}
out:
	ufshcd_clear_eh_in_progress(hba);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

static void ufshcd_update_uic_reg_hist(struct ufs_uic_err_reg_hist *reg_hist,
		u32 reg)
{
	reg_hist->reg[reg_hist->pos] = reg;
	reg_hist->tstamp[reg_hist->pos] = ktime_get();
	reg_hist->pos = (reg_hist->pos + 1) % UIC_ERR_REG_HIST_LENGTH;
}

static void ufshcd_rls_handler(struct work_struct *work)
{
	struct ufs_hba *hba;
	int ret = 0;
	u32 mode;

	hba = container_of(work, struct ufs_hba, rls_work);

	pm_runtime_get_sync(hba->dev);
	down_write(&hba->lock);
	ufshcd_scsi_block_requests(hba);
	if (ufshcd_is_shutdown_ongoing(hba))
		goto out;
	ret = ufshcd_wait_for_doorbell_clr(hba, U64_MAX);
	if (ret) {
		dev_err(hba->dev,
			"Timed out (%d) waiting for DB to clear\n",
			ret);
		goto out;
	}

	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_PWRMODE), &mode);
	if (hba->pwr_info.pwr_rx != ((mode >> PWR_RX_OFFSET) & PWR_INFO_MASK))
		hba->restore_needed = true;

	if (hba->pwr_info.pwr_tx != (mode & PWR_INFO_MASK))
		hba->restore_needed = true;

	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_RXGEAR), &mode);
	if (hba->pwr_info.gear_rx != mode)
		hba->restore_needed = true;

	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_TXGEAR), &mode);
	if (hba->pwr_info.gear_tx != mode)
		hba->restore_needed = true;

	if (hba->restore_needed)
		ret = ufshcd_config_pwr_mode(hba, &(hba->pwr_info));

	if (ret)
		dev_err(hba->dev, "%s: Failed setting power mode, err = %d\n",
			__func__, ret);
	else
		hba->restore_needed = false;

out:
	up_write(&hba->lock);
	ufshcd_scsi_unblock_requests(hba);
	pm_runtime_put_sync(hba->dev);
}

static void ufshcd_fatal_mode_handler(struct work_struct *work)
{
	struct ufs_hba *hba;

	hba = container_of(work, struct ufs_hba, fatal_mode_work);

	ufshcd_scsi_block_requests(hba);

	dev_err(hba->dev, "fatal mode %04x.\n", hba->dev_info.w_manufacturer_id);
	if (hba->dev_info.w_manufacturer_id == UFS_VENDOR_TOSHIBA)
		UFS_Toshiba_query_fatal_mode(hba);

	ufshcd_scsi_unblock_requests(hba);
	return;
}

/**
 * ufshcd_update_uic_error - check and set fatal UIC error flags.
 * @hba: per-adapter instance
 *
 * Returns
 *  IRQ_HANDLED - If interrupt is valid
 *  IRQ_NONE    - If invalid interrupt
 */
static irqreturn_t ufshcd_update_uic_error(struct ufs_hba *hba)
{
	u32 reg;
	irqreturn_t retval = IRQ_NONE;

	/* PHY layer lane error */
	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_PHY_ADAPTER_LAYER);
	if ((reg & UIC_PHY_ADAPTER_LAYER_ERROR) &&
	    (reg & UIC_PHY_ADAPTER_LAYER_ERROR_CODE_MASK)) {
		/*
		 * To know whether this error is fatal or not, DB timeout
		 * must be checked but this error is handled separately.
		 */
		dev_dbg(hba->dev, "%s: UIC Lane error reported, reg 0x%x\n",
				__func__, reg);
		ufshcd_update_uic_error_cnt(hba, reg, UFS_UIC_ERROR_PA);
		ufshcd_update_uic_reg_hist(&hba->ufs_stats.pa_err, reg);

		/*
		 * Don't ignore LINERESET indication during hibern8
		 * enter operation.
		 */
		if (reg & UIC_PHY_ADAPTER_LAYER_GENERIC_ERROR) {
			struct uic_command *cmd = hba->active_uic_cmd;

			if (cmd) {
				if (cmd->command == UIC_CMD_DME_HIBER_ENTER) {
					dev_err(hba->dev, "%s: LINERESET during hibern8 enter, reg 0x%x\n",
						__func__, reg);
					hba->full_init_linereset = true;
				}
			}
			if (!hba->full_init_linereset)
				queue_work(hba->recovery_wq, &hba->rls_work);
		}
		retval |= IRQ_HANDLED;
	}

	/* PA_INIT_ERROR is fatal and needs UIC reset */
	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_DATA_LINK_LAYER);
	if ((reg & UIC_DATA_LINK_LAYER_ERROR) &&
	    (reg & UIC_DATA_LINK_LAYER_ERROR_CODE_MASK)) {
		ufshcd_update_uic_error_cnt(hba, reg, UFS_UIC_ERROR_DL);
		ufshcd_update_uic_reg_hist(&hba->ufs_stats.dl_err, reg);

		if (reg & UIC_DATA_LINK_LAYER_ERROR_PA_INIT) {
			hba->uic_error |= UFSHCD_UIC_DL_PA_INIT_ERROR;
		} else if (hba->dev_info.quirks &
			   UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS) {
			if (reg & UIC_DATA_LINK_LAYER_ERROR_NAC_RECEIVED)
				hba->uic_error |=
					UFSHCD_UIC_DL_NAC_RECEIVED_ERROR;
			else if (reg &
				 UIC_DATA_LINK_LAYER_ERROR_TCx_REPLAY_TIMEOUT)
				hba->uic_error |=
					UFSHCD_UIC_DL_TCx_REPLAY_ERROR;
		}
		retval |= IRQ_HANDLED;
	}

	/* UIC NL/TL/DME errors needs software retry */
	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_NETWORK_LAYER);
	if ((reg & UIC_NETWORK_LAYER_ERROR) &&
	    (reg & UIC_NETWORK_LAYER_ERROR_CODE_MASK)) {
		ufshcd_update_uic_reg_hist(&hba->ufs_stats.nl_err, reg);
		hba->uic_error |= UFSHCD_UIC_NL_ERROR;
		retval |= IRQ_HANDLED;
	}

	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_TRANSPORT_LAYER);
	if ((reg & UIC_TRANSPORT_LAYER_ERROR) &&
	    (reg & UIC_TRANSPORT_LAYER_ERROR_CODE_MASK)) {
		ufshcd_update_uic_reg_hist(&hba->ufs_stats.tl_err, reg);
		hba->uic_error |= UFSHCD_UIC_TL_ERROR;
		retval |= IRQ_HANDLED;
	}

	reg = ufshcd_readl(hba, REG_UIC_ERROR_CODE_DME);
	if ((reg & UIC_DME_ERROR) &&
	    (reg & UIC_DME_ERROR_CODE_MASK)) {
		ufshcd_update_uic_error_cnt(hba, reg, UFS_UIC_ERROR_DME);
		ufshcd_update_uic_reg_hist(&hba->ufs_stats.dme_err, reg);
		hba->uic_error |= UFSHCD_UIC_DME_ERROR;
		retval |= IRQ_HANDLED;
	}

	dev_dbg(hba->dev, "%s: UIC error flags = 0x%08x\n",
			__func__, hba->uic_error);
#if defined(SEC_UFS_ERROR_COUNT)
	if (hba->uic_error || hba->full_init_linereset)
		SEC_ufs_uic_error_check(hba, true, false);
#endif

	return retval;
}

/**
 * ufshcd_check_errors - Check for errors that need s/w attention
 * @hba: per-adapter instance
 *
 * Returns
 *  IRQ_HANDLED - If interrupt is valid
 *  IRQ_NONE    - If invalid interrupt
 */
static irqreturn_t ufshcd_check_errors(struct ufs_hba *hba)
{
	bool queue_eh_work = false;
	irqreturn_t retval = IRQ_NONE;

	if (hba->errors & INT_FATAL_ERRORS || hba->ce_error)
		queue_eh_work = true;

	if (hba->errors & UIC_LINK_LOST) {
		dev_err(hba->dev, "%s: UIC_LINK_LOST received, errors 0x%x\n",
					__func__, hba->errors);
		queue_eh_work = true;
	}

#if defined(SEC_UFS_ERROR_COUNT)
	if (queue_eh_work)
		SEC_ufs_uic_error_check(hba, false, true);
#endif

	if (hba->errors & UIC_ERROR) {
		hba->uic_error = 0;
		retval = ufshcd_update_uic_error(hba);
		if (hba->uic_error)
			queue_eh_work = true;
	}

	if (queue_eh_work) {
		/*
		 * update the transfer error masks to sticky bits, let's do this
		 * irrespective of current ufshcd_state.
		 */
		hba->saved_err |= hba->errors;
		hba->saved_uic_err |= hba->uic_error;
		hba->saved_ce_err |= hba->ce_error;

		/* handle fatal errors only when link is functional */
		if (hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL) {
			/*
			 * Set error handling in progress flag early so that we
			 * don't issue new requests any more.
			 */
			ufshcd_set_eh_in_progress(hba);

			hba->ufshcd_state = UFSHCD_STATE_EH_SCHEDULED;

			queue_work(hba->recovery_wq, &hba->eh_work);
		}
		retval |= IRQ_HANDLED;
	}
	/*
	 * if (!queue_eh_work) -
	 * Other errors are either non-fatal where host recovers
	 * itself without s/w intervention or errors that will be
	 * handled by the SCSI core layer.
	 */
	return retval;
}

/**
 * ufshcd_tmc_handler - handle task management function completion
 * @hba: per adapter instance
 *
 * Returns
 *  IRQ_HANDLED - If interrupt is valid
 *  IRQ_NONE    - If invalid interrupt
 */
static irqreturn_t ufshcd_tmc_handler(struct ufs_hba *hba)
{
	u32 tm_doorbell;

	tm_doorbell = ufshcd_readl(hba, REG_UTP_TASK_REQ_DOOR_BELL);
	hba->tm_condition = tm_doorbell ^ hba->outstanding_tasks;
	if (hba->tm_condition) {
		wake_up(&hba->tm_wq);
		return IRQ_HANDLED;
	} else {
		return IRQ_NONE;
	}
}

/**
 * ufshcd_sl_intr - Interrupt service routine
 * @hba: per adapter instance
 * @intr_status: contains interrupts generated by the controller
 *
 * Returns
 *  IRQ_HANDLED - If interrupt is valid
 *  IRQ_NONE    - If invalid interrupt
 */
static irqreturn_t ufshcd_sl_intr(struct ufs_hba *hba, u32 intr_status)
{
	irqreturn_t retval = IRQ_NONE;

	ufsdbg_error_inject_dispatcher(hba,
		ERR_INJECT_INTR, intr_status, &intr_status);

	hba->errors = UFSHCD_ERROR_MASK & intr_status;
	if (hba->errors || hba->ce_error)
		retval |= ufshcd_check_errors(hba);

	if (intr_status & UFSHCD_UIC_MASK)
		retval |= ufshcd_uic_cmd_compl(hba, intr_status);

	if (intr_status & UTP_TASK_REQ_COMPL)
		retval |= ufshcd_tmc_handler(hba);

	if (intr_status & UTP_TRANSFER_REQ_COMPL)
		retval |= ufshcd_transfer_req_compl(hba);

	return retval;
}

/**
 * ufshcd_intr - Main interrupt service routine
 * @irq: irq number
 * @__hba: pointer to adapter instance
 *
 * Returns IRQ_HANDLED - If interrupt is valid
 *		IRQ_NONE - If invalid interrupt
 */
static irqreturn_t ufshcd_intr(int irq, void *__hba)
{
	u32 intr_status, enabled_intr_status;
	irqreturn_t retval = IRQ_NONE;
	struct ufs_hba *hba = __hba;
	int retries = hba->nutrs;

	spin_lock(hba->host->host_lock);
	intr_status = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
	hba->ufs_stats.last_intr_status = intr_status;
	hba->ufs_stats.last_intr_ts = ktime_get();

	/*
	 * There could be max of hba->nutrs reqs in flight and in worst case
	 * if the reqs get finished 1 by 1 after the interrupt status is
	 * read, make sure we handle them by checking the interrupt status
	 * again in a loop until we process all of the reqs before returning.
	 */
	do {
		enabled_intr_status =
			intr_status & ufshcd_readl(hba, REG_INTERRUPT_ENABLE);
		if (intr_status)
			ufshcd_writel(hba, intr_status, REG_INTERRUPT_STATUS);
		if (enabled_intr_status) {
			ufshcd_sl_intr(hba, enabled_intr_status);
			retval = IRQ_HANDLED;
		}

		intr_status = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
	} while (intr_status && --retries);

	if (retval == IRQ_NONE) {
		dev_err(hba->dev, "%s: Unhandled interrupt 0x%08x\n",
					__func__, intr_status);
		ufshcd_hex_dump(hba, "host regs: ", hba->mmio_base,
					UFSHCI_REG_SPACE_SIZE);
	}

	spin_unlock(hba->host->host_lock);
	return retval;
}

static int ufshcd_clear_tm_cmd(struct ufs_hba *hba, int tag)
{
	int err = 0;
	u32 mask = 1 << tag;
	unsigned long flags;

	if (!test_bit(tag, &hba->outstanding_tasks))
		goto out;

	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_utmrl_clear(hba, tag);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/* poll for max. 1 sec to clear door bell register by h/w */
	err = ufshcd_wait_for_register(hba,
			REG_UTP_TASK_REQ_DOOR_BELL,
			mask, 0, 1000, 1000, true);
out:
	return err;
}

/**
 * ufshcd_issue_tm_cmd - issues task management commands to controller
 * @hba: per adapter instance
 * @lun_id: LUN ID to which TM command is sent
 * @task_id: task ID to which the TM command is applicable
 * @tm_function: task management function opcode
 * @tm_response: task management service response return value
 *
 * Returns non-zero value on error, zero on success.
 */
static int ufshcd_issue_tm_cmd(struct ufs_hba *hba, int lun_id, int task_id,
		u8 tm_function, u8 *tm_response)
{
	struct utp_task_req_desc *task_req_descp;
	struct utp_upiu_task_req *task_req_upiup;
	struct Scsi_Host *host;
	unsigned long flags;
	int free_slot;
	int err;
	int task_tag;

	host = hba->host;

	/*
	 * Get free slot, sleep if slots are unavailable.
	 * Even though we use wait_event() which sleeps indefinitely,
	 * the maximum wait time is bounded by %TM_CMD_TIMEOUT.
	 */
	wait_event(hba->tm_tag_wq, ufshcd_get_tm_free_slot(hba, &free_slot));
	hba->ufs_stats.clk_hold.ctx = TM_CMD_SEND;
	ufshcd_hold_all(hba);

	spin_lock_irqsave(host->host_lock, flags);
	task_req_descp = hba->utmrdl_base_addr;
	task_req_descp += free_slot;

	/* Configure task request descriptor */
	task_req_descp->header.dword_0 = cpu_to_le32(UTP_REQ_DESC_INT_CMD);
	task_req_descp->header.dword_2 =
			cpu_to_le32(OCS_INVALID_COMMAND_STATUS);

	/* Configure task request UPIU */
	task_req_upiup =
		(struct utp_upiu_task_req *) task_req_descp->task_req_upiu;
	task_tag = hba->nutrs + free_slot;
	task_req_upiup->header.dword_0 =
		UPIU_HEADER_DWORD(UPIU_TRANSACTION_TASK_REQ, 0,
					      lun_id, task_tag);
	task_req_upiup->header.dword_1 =
		UPIU_HEADER_DWORD(0, tm_function, 0, 0);
	/*
	 * The host shall provide the same value for LUN field in the basic
	 * header and for Input Parameter.
	 */
	task_req_upiup->input_param1 = cpu_to_be32(lun_id);
	task_req_upiup->input_param2 = cpu_to_be32(task_id);

	ufshcd_vops_setup_task_mgmt(hba, free_slot, tm_function);

	/* send command to the controller */
	__set_bit(free_slot, &hba->outstanding_tasks);

	/* Make sure descriptors are ready before ringing the task doorbell */
	wmb();

	ufshcd_writel(hba, 1 << free_slot, REG_UTP_TASK_REQ_DOOR_BELL);
	/* Make sure that doorbell is committed immediately */
	wmb();

	spin_unlock_irqrestore(host->host_lock, flags);

	ufshcd_add_tm_upiu_trace(hba, task_tag, "tm_send");

	/* wait until the task management command is completed */
	err = wait_event_timeout(hba->tm_wq,
			test_bit(free_slot, &hba->tm_condition),
			msecs_to_jiffies(TM_CMD_TIMEOUT));
	if (!err) {
		ufshcd_add_tm_upiu_trace(hba, task_tag, "tm_complete_err");
		dev_err(hba->dev, "%s: task management cmd 0x%.2x timed-out\n",
				__func__, tm_function);
		if (ufshcd_clear_tm_cmd(hba, free_slot))
			dev_WARN(hba->dev, "%s: unable clear tm cmd (slot %d) after timeout\n",
					__func__, free_slot);
		spin_lock_irqsave(host->host_lock, flags);
		__clear_bit(free_slot, &hba->outstanding_tasks);
		spin_unlock_irqrestore(host->host_lock, flags);
		err = -ETIMEDOUT;
	} else {
		err = ufshcd_task_req_compl(hba, free_slot, tm_response);
		ufshcd_add_tm_upiu_trace(hba, task_tag, "tm_complete");
	}

	clear_bit(free_slot, &hba->tm_condition);
	ufshcd_put_tm_slot(hba, free_slot);
	wake_up(&hba->tm_tag_wq);
	hba->ufs_stats.clk_rel.ctx = TM_CMD_SEND;

	ufshcd_release_all(hba);
	return err;
}

/**
 * ufshcd_eh_device_reset_handler - device reset handler registered to
 *                                    scsi layer.
 * @cmd: SCSI command pointer
 *
 * Returns SUCCESS/FAILED
 */
static int ufshcd_eh_device_reset_handler(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	unsigned int tag;
	u32 pos;
	int err;
	u8 resp = 0xF;
	struct ufshcd_lrb *lrbp;
	unsigned long flags;

	host = cmd->device->host;
	hba = shost_priv(host);
	tag = cmd->request->tag;

	lrbp = &hba->lrb[tag];
	err = ufshcd_issue_tm_cmd(hba, lrbp->lun, 0, UFS_LOGICAL_RESET, &resp);
	if (err || resp != UPIU_TASK_MANAGEMENT_FUNC_COMPL) {
		if (!err)
			err = resp;
		goto out;
	}

	/* clear the commands that were pending for corresponding LUN */
	for_each_set_bit(pos, &hba->outstanding_reqs, hba->nutrs) {
		if (hba->lrb[pos].lun == lrbp->lun) {
			err = ufshcd_clear_cmd(hba, pos);
			if (err)
				break;
		}
	}
	spin_lock_irqsave(host->host_lock, flags);
	ufshcd_transfer_req_compl(hba);
	spin_unlock_irqrestore(host->host_lock, flags);

out:
	hba->req_abort_count = 0;
	if (!err) {
#if defined(CONFIG_UFSFEATURE)
		ufsf_hpb_reset_lu(&hba->ufsf);
#endif
		err = SUCCESS;
	} else {
		dev_err(hba->dev, "%s: failed with err %d\n", __func__, err);
		err = FAILED;
	}
	return err;
}

static void ufshcd_set_req_abort_skip(struct ufs_hba *hba, unsigned long bitmap)
{
	struct ufshcd_lrb *lrbp;
	int tag;

	for_each_set_bit(tag, &bitmap, hba->nutrs) {
		lrbp = &hba->lrb[tag];
		lrbp->req_abort_skip = true;
	}
}

/**
 * ufshcd_abort - abort a specific command
 * @cmd: SCSI command pointer
 *
 * Abort the pending command in device by sending UFS_ABORT_TASK task management
 * command, and in host controller by clearing the door-bell register. There can
 * be race between controller sending the command to the device while abort is
 * issued. To avoid that, first issue UFS_QUERY_TASK to check if the command is
 * really issued and then try to abort it.
 *
 * Returns SUCCESS/FAILED
 */
static int ufshcd_abort(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	unsigned long flags;
	unsigned int tag;
	int err = 0;
	int poll_cnt;
	u8 resp = 0xF;
	struct ufshcd_lrb *lrbp;
	u32 reg;

	host = cmd->device->host;
	hba = shost_priv(host);
	tag = cmd->request->tag;
	if (!ufshcd_valid_tag(hba, tag)) {
		dev_err(hba->dev,
			"%s: invalid command tag %d: cmd=0x%pK, cmd->request=0x%pK\n",
			__func__, tag, cmd, cmd->request);
		BUG_ON(1);
	}

	if (cmd->cmnd[0] == READ_10 || cmd->cmnd[0] == WRITE_10) {
		unsigned long lba = (cmd->cmnd[2] << 24) |
			(cmd->cmnd[3] << 16) |
			(cmd->cmnd[4] << 8) |
			(cmd->cmnd[5] << 0);
		unsigned int sct = (cmd->cmnd[7] << 8) |
			(cmd->cmnd[8] << 0);
		sector_t lba_bi_sector = 0;

		if (cmd->request->bio)
			lba_bi_sector = cmd->request->bio->bi_iter.bi_sector;

		dev_err(hba->dev, "%s: tag:%d, cmd:0x%x, "
				"lba:0x%08lx(0x%llx), sct:0x%04x, retries %d\n",
				__func__, tag, cmd->cmnd[0], lba,
				(unsigned long long)lba_bi_sector, sct, cmd->allowed);
	} else {
		dev_err(hba->dev, "%s: tag:%d, cmd:0x%x, retries %d\n",
				__func__, tag, cmd->cmnd[0], cmd->allowed);
	}

	lrbp = &hba->lrb[tag];

	ufshcd_update_error_stats(hba, UFS_ERR_TASK_ABORT);

	if (!ufshcd_valid_tag(hba, tag)) {
		dev_err(hba->dev,
			"%s: invalid command tag %d: cmd=0x%pK, cmd->request=0x%pK\n",
			__func__, tag, cmd, cmd->request);
		BUG_ON(1);
	}

	/*
	 * Task abort to the device W-LUN is illegal. When this command
	 * will fail, due to spec violation, scsi err handling next step
	 * will be to send LU reset which, again, is a spec violation.
	 * To avoid these unnecessary/illegal step we skip to the last error
	 * handling stage: reset and restore.
	 */
	if (lrbp->lun == UFS_UPIU_UFS_DEVICE_WLUN)
		return ufshcd_eh_host_reset_handler(cmd);

#if defined(SEC_UFS_ERROR_COUNT)
	SEC_ufs_utp_error_check(hba, cmd, false, 0);
#if defined(CONFIG_UFSFEATURE) && defined(CONFIG_UFSHPB)
	if (hba->ufsf.hpb_dev_info.hpb_device &&
			((cmd->cmnd[0] == READ_16) || (((cmd->cmnd[0] >> 4) & 0xF) == 0xF))) {
		SEC_ufs_hpb_error_check(hba, cmd);
	}
#endif
#endif

	ufshcd_hold_all(hba);
	reg = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	/* If command is already aborted/completed, return SUCCESS */
	if (!(test_bit(tag, &hba->outstanding_reqs))) {
		dev_err(hba->dev,
			"%s: cmd at tag %d already completed, outstanding=0x%lx, doorbell=0x%x\n",
			__func__, tag, hba->outstanding_reqs, reg);
		goto out;
	}

	if (!(reg & (1 << tag))) {
		dev_err(hba->dev,
		"%s: cmd was completed, but without a notifying intr, tag = %d",
		__func__, tag);
	}

	/* Print Transfer Request of aborted task */
	dev_err(hba->dev, "%s: Device abort task at tag %d\n", __func__, tag);

	/*
	 * Print detailed info about aborted request.
	 * As more than one request might get aborted at the same time,
	 * print full information only for the first aborted request in order
	 * to reduce repeated printouts. For other aborted requests only print
	 * basic details.
	 */
	scsi_print_command(cmd);
	if (!hba->req_abort_count) {
		ufshcd_print_fsm_state(hba);
		ufshcd_print_host_regs(hba);
		ufshcd_print_host_state(hba);
		ufshcd_print_pwr_info(hba);
		ufshcd_print_trs(hba, 1 << tag, true);
		/* crash the system upon setting this debugfs. */
		BUG_ON(hba->crash_on_err);
	} else {
		ufshcd_print_trs(hba, 1 << tag, false);
	}
	hba->req_abort_count++;

	/* Skip task abort in case previous aborts failed and report failure */
	if (lrbp->req_abort_skip) {
		err = -EIO;
		goto out;
	}

	for (poll_cnt = 100; poll_cnt; poll_cnt--) {
		err = ufshcd_issue_tm_cmd(hba, lrbp->lun, lrbp->task_tag,
				UFS_QUERY_TASK, &resp);
		if (!err && resp == UPIU_TASK_MANAGEMENT_FUNC_SUCCEEDED) {
			/* cmd pending in the device */
			dev_err(hba->dev, "%s: cmd pending in the device. tag = %d\n",
				__func__, tag);
			break;
		} else if (!err && resp == UPIU_TASK_MANAGEMENT_FUNC_COMPL) {
			/*
			 * cmd not pending in the device, check if it is
			 * in transition.
			 */
			dev_err(hba->dev, "%s: cmd at tag %d not pending in the device.\n",
				__func__, tag);
			reg = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
			if (reg & (1 << tag)) {
				/* sleep for max. 200us to stabilize */
				usleep_range(100, 200);
				continue;
			}
			/* command completed already */
			dev_err(hba->dev, "%s: cmd at tag %d successfully cleared from DB.\n",
				__func__, tag);
			goto out;
		} else {
			dev_err(hba->dev,
				"%s: no response from device. tag = %d, err %d\n",
				__func__, tag, err);
			if (!err)
				err = resp; /* service response error */
#if defined(SEC_UFS_ERROR_COUNT)
			SEC_ufs_utp_error_check(hba, NULL, true, UFS_QUERY_TASK);
#endif
			goto out;
		}
	}

	if (!poll_cnt) {
		err = -EBUSY;
		goto out;
	}

	err = ufshcd_issue_tm_cmd(hba, lrbp->lun, lrbp->task_tag,
			UFS_ABORT_TASK, &resp);
	if (err || resp != UPIU_TASK_MANAGEMENT_FUNC_COMPL) {
		if (!err) {
			err = resp; /* service response error */
			dev_err(hba->dev, "%s: issued. tag = %d, err %d\n",
				__func__, tag, err);
		}
#if defined(SEC_UFS_ERROR_COUNT)
		SEC_ufs_utp_error_check(hba, NULL, true, UFS_ABORT_TASK);
#endif
		goto out;
	}

	err = ufshcd_clear_cmd(hba, tag);
	if (err) {
		dev_err(hba->dev, "%s: Failed clearing cmd at tag %d, err %d\n",
			__func__, tag, err);
		goto out;
	}

	scsi_dma_unmap(cmd);

	spin_lock_irqsave(host->host_lock, flags);
	ufshcd_outstanding_req_clear(hba, tag);
	hba->lrb[tag].cmd = NULL;
	spin_unlock_irqrestore(host->host_lock, flags);

	clear_bit_unlock(tag, &hba->lrb_in_use);
	wake_up(&hba->dev_cmd.tag_wq);

out:
	if (!err) {
		err = SUCCESS;
		if ((hba->dev_info.quirks & UFS_DEVICE_QUIRK_SUPPORT_QUERY_FATAL_MODE) &&
				!hba->UFS_fatal_mode_done) {
			unsigned long max_doorbells = (1UL << hba->nutrs) - 1;
			if (hba->outstanding_reqs == max_doorbells)
				__ufshcd_transfer_req_compl(hba,
						(1UL << (hba->nutrs - 1)));
			schedule_work(&hba->fatal_mode_work);
		}
	} else {
		dev_err(hba->dev, "%s: failed with err %d\n", __func__, err);
		ufshcd_set_req_abort_skip(hba, hba->outstanding_reqs);
		err = FAILED;
	}

	/*
	 * This ufshcd_release_all() corresponds to the original scsi cmd that
	 * got aborted here (as we won't get any IRQ for it).
	 */
	ufshcd_release_all(hba);
	return err;
}

/**
 * ufshcd_host_reset_and_restore - reset and restore host controller
 * @hba: per-adapter instance
 *
 * Note that host controller reset may issue DME_RESET to
 * local and remote (device) Uni-Pro stack and the attributes
 * are reset to default state.
 *
 * Returns zero on success, non-zero on failure
 */
static int ufshcd_host_reset_and_restore(struct ufs_hba *hba)
{
	int err;
	unsigned long flags;

	/*
	 * Stop the host controller and complete the requests
	 * cleared by h/w
	 */
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufshcd_hba_stop(hba, false);
#if defined(CONFIG_UFSFEATURE)
	ufsf_hpb_reset_host(&hba->ufsf);
#endif
	hba->silence_err_logs = true;
	ufshcd_complete_requests(hba);
	hba->silence_err_logs = false;
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/* scale up clocks to max frequency before full reinitialization */
	ufshcd_set_clk_freq(hba, true);

	err = ufshcd_hba_enable(hba);
	if (err)
		goto out;

	/* Establish the link again and restore the device */
	err = ufshcd_probe_hba(hba);

	if (!err && (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL)) {
		err = -EIO;
		goto out;
	}


out:
	if (err)
		dev_err(hba->dev, "%s: Host init failed %d\n", __func__, err);

	return err;
}

static int ufshcd_detect_device(struct ufs_hba *hba)
{
	int err = 0;

	err = ufshcd_vops_full_reset(hba);
	if (err)
		dev_warn(hba->dev, "%s: full reset returned %d\n",
			 __func__, err);

	err = ufshcd_reset_device(hba);
	if (err)
		dev_warn(hba->dev, "%s: device reset failed. err %d\n",
			 __func__, err);

	return ufshcd_host_reset_and_restore(hba);
}

/**
 * ufshcd_reset_and_restore - reset and re-initialize host/device
 * @hba: per-adapter instance
 *
 * Reset and recover device, host and re-establish link. This
 * is helpful to recover the communication in fatal error conditions.
 *
 * Returns zero on success, non-zero on failure
 */
static int ufshcd_reset_and_restore(struct ufs_hba *hba)
{
	int err = 0;
	int retries = MAX_HOST_RESET_RETRIES;

#ifdef CONFIG_BLK_TURBO_WRITE
	ufshcd_reset_tw(hba, false);
#endif
	ssleep(2);
	ufshcd_enable_irq(hba);

	do {
		err = ufshcd_detect_device(hba);
	} while (err && --retries);

	/*
	 * There is no point proceeding even after failing
	 * to recover after multiple retries.
	 */
	BUG_ON(err && ufshcd_is_embedded_dev(hba));

	return err;
}

/**
 * ufshcd_eh_host_reset_handler - host reset handler registered to scsi layer
 * @cmd: SCSI command pointer
 *
 * Returns SUCCESS/FAILED
 */
static int ufshcd_eh_host_reset_handler(struct scsi_cmnd *cmd)
{
	int err = SUCCESS;
	unsigned long flags;
	struct ufs_hba *hba;

	hba = shost_priv(cmd->device->host);

	/*
	 * Check if there is any race with fatal error handling.
	 * If so, wait for it to complete. Even though fatal error
	 * handling does reset and restore in some cases, don't assume
	 * anything out of it. We are just avoiding race here.
	 */
	do {
		spin_lock_irqsave(hba->host->host_lock, flags);
		if (!(work_pending(&hba->eh_work) ||
			    hba->ufshcd_state == UFSHCD_STATE_RESET ||
			    hba->ufshcd_state == UFSHCD_STATE_EH_SCHEDULED))
			break;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		dev_err(hba->dev, "%s: reset in progress - 1\n", __func__);
		flush_work(&hba->eh_work);
	} while (1);

	/*
	 * we don't know if previous reset had really reset the host controller
	 * or not. So let's force reset here to be sure.
	 */
	hba->ufshcd_state = UFSHCD_STATE_ERROR;
	hba->force_host_reset = true;
	queue_work(hba->recovery_wq, &hba->eh_work);

	/* wait for the reset work to finish */
	do {
		if (!(work_pending(&hba->eh_work) ||
				hba->ufshcd_state == UFSHCD_STATE_RESET))
			break;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		dev_err(hba->dev, "%s: reset in progress - 2\n", __func__);
		flush_work(&hba->eh_work);
		spin_lock_irqsave(hba->host->host_lock, flags);
	} while (1);

	if (!((hba->ufshcd_state == UFSHCD_STATE_OPERATIONAL) &&
	      ufshcd_is_link_active(hba))) {
		err = FAILED;
		hba->ufshcd_state = UFSHCD_STATE_ERROR;
	}

	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return err;
}

/**
 * ufshcd_get_max_icc_level - calculate the ICC level
 * @sup_curr_uA: max. current supported by the regulator
 * @start_scan: row at the desc table to start scan from
 * @buff: power descriptor buffer
 *
 * Returns calculated max ICC level for specific regulator
 */
static u32 ufshcd_get_max_icc_level(int sup_curr_uA, u32 start_scan, char *buff)
{
	int i;
	int curr_uA;
	u16 data;
	u16 unit;

	for (i = start_scan; i >= 0; i--) {
		data = be16_to_cpup((__be16 *)&buff[2 * i]);
		unit = (data & ATTR_ICC_LVL_UNIT_MASK) >>
						ATTR_ICC_LVL_UNIT_OFFSET;
		curr_uA = data & ATTR_ICC_LVL_VALUE_MASK;
		switch (unit) {
		case UFSHCD_NANO_AMP:
			curr_uA = curr_uA / 1000;
			break;
		case UFSHCD_MILI_AMP:
			curr_uA = curr_uA * 1000;
			break;
		case UFSHCD_AMP:
			curr_uA = curr_uA * 1000 * 1000;
			break;
		case UFSHCD_MICRO_AMP:
		default:
			break;
		}
		if (sup_curr_uA >= curr_uA)
			break;
	}
	if (i < 0) {
		i = 0;
		pr_err("%s: Couldn't find valid icc_level = %d\n", __func__, i);
	}

	return (u32)i;
}

/**
 * ufshcd_find_max_sup_active_icc_level - find the max ICC level
 * In case regulators are not initialized we'll return 0
 * @hba: per-adapter instance
 * @desc_buf: power descriptor buffer to extract ICC levels from.
 * @len: length of desc_buff
 *
 * Returns calculated max ICC level
 */
static u32 ufshcd_find_max_sup_active_icc_level(struct ufs_hba *hba,
							u8 *desc_buf, int len)
{
	u32 icc_level = 0;

	/*
	 * VCCQ rail is optional for removable UFS card and also most of the
	 * vendors don't use this rail for embedded UFS devices as well. So
	 * it is normal that VCCQ rail may not be provided for given platform.
	 */
	if (!hba->vreg_info.vcc || !hba->vreg_info.vccq2) {
		dev_err(hba->dev, "%s: Regulator capability was not set, bActiveICCLevel=%d\n",
			__func__, icc_level);
		goto out;
	}

	if (hba->vreg_info.vcc && hba->vreg_info.vcc->max_uA)
		icc_level = ufshcd_get_max_icc_level(
				hba->vreg_info.vcc->max_uA,
				POWER_DESC_MAX_ACTV_ICC_LVLS - 1,
				&desc_buf[PWR_DESC_ACTIVE_LVLS_VCC_0]);

	if (hba->vreg_info.vccq && hba->vreg_info.vccq->max_uA)
		icc_level = ufshcd_get_max_icc_level(
				hba->vreg_info.vccq->max_uA,
				icc_level,
				&desc_buf[PWR_DESC_ACTIVE_LVLS_VCCQ_0]);

	if (hba->vreg_info.vccq2 && hba->vreg_info.vccq2->max_uA)
		icc_level = ufshcd_get_max_icc_level(
				hba->vreg_info.vccq2->max_uA,
				icc_level,
				&desc_buf[PWR_DESC_ACTIVE_LVLS_VCCQ2_0]);
out:
	return icc_level;
}

static void ufshcd_set_active_icc_lvl(struct ufs_hba *hba)
{
	int ret;
	int buff_len = hba->desc_size.pwr_desc;
	u8 *desc_buf = NULL;
	u32 icc_level;

	if (buff_len) {
		desc_buf = kmalloc(buff_len, GFP_KERNEL);
		if (!desc_buf)
			return;
	}

	ret = ufshcd_read_power_desc(hba, desc_buf, buff_len);
	if (ret) {
		dev_err(hba->dev,
			"%s: Failed reading power descriptor.len = %d ret = %d",
			__func__, buff_len, ret);
		goto out;
	}

	icc_level = ufshcd_find_max_sup_active_icc_level(hba, desc_buf,
							 buff_len);
	dev_dbg(hba->dev, "%s: setting icc_level 0x%x", __func__, icc_level);

	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
		QUERY_ATTR_IDN_ACTIVE_ICC_LVL, 0, 0, &icc_level);

	if (ret)
		dev_err(hba->dev,
			"%s: Failed configuring bActiveICCLevel = %d ret = %d",
			__func__, icc_level, ret);

out:
	kfree(desc_buf);
}

static int ufshcd_set_low_vcc_level(struct ufs_hba *hba,
					struct ufs_dev_desc *dev_desc)
{
	int ret;
	struct ufs_vreg *vreg = hba->vreg_info.vcc;

	/* Check if device supports the low voltage VCC feature */
	if (dev_desc->wspecversion < 0x300)
		return 0;

	/*
	 * Check if host has support for low VCC voltage?
	 * In addition, also check if we have already set the low VCC level
	 * or not?
	 */
	if (!vreg->low_voltage_sup || vreg->low_voltage_active)
		return 0;

	/* Put the device in sleep before lowering VCC level */
	ret = ufshcd_set_dev_pwr_mode(hba, UFS_SLEEP_PWR_MODE);

	/* Switch off VCC before switching it ON at 2.5v */
	ret = ufshcd_disable_vreg(hba->dev, vreg);
	/* add ~2ms delay before renabling VCC at lower voltage */
	usleep_range(2000, 2100);
	/* Now turn back VCC ON at low voltage */
	vreg->low_voltage_active = true;
	ret = ufshcd_enable_vreg(hba->dev, vreg);

	/* Bring the device in active now */
	ret = ufshcd_set_dev_pwr_mode(hba, UFS_ACTIVE_PWR_MODE);

	return ret;
}

/**
 * ufshcd_scsi_add_wlus - Adds required W-LUs
 * @hba: per-adapter instance
 *
 * UFS device specification requires the UFS devices to support 4 well known
 * logical units:
 *	"REPORT_LUNS" (address: 01h)
 *	"UFS Device" (address: 50h)
 *	"RPMB" (address: 44h)
 *	"BOOT" (address: 30h)
 * UFS device's power management needs to be controlled by "POWER CONDITION"
 * field of SSU (START STOP UNIT) command. But this "power condition" field
 * will take effect only when its sent to "UFS device" well known logical unit
 * hence we require the scsi_device instance to represent this logical unit in
 * order for the UFS host driver to send the SSU command for power management.
 *
 * We also require the scsi_device instance for "RPMB" (Replay Protected Memory
 * Block) LU so user space process can control this LU. User space may also
 * want to have access to BOOT LU.
 *
 * This function adds scsi device instances for each of all well known LUs
 * (except "REPORT LUNS" LU).
 *
 * Returns zero on success (all required W-LUs are added successfully),
 * non-zero error value on failure (if failed to add any of the required W-LU).
 */
static int ufshcd_scsi_add_wlus(struct ufs_hba *hba)
{
	int ret = 0;
	struct scsi_device *sdev_rpmb = NULL;
	struct scsi_device *sdev_boot = NULL;

	hba->sdev_ufs_device = __scsi_add_device(hba->host, 0, 0,
		ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_UFS_DEVICE_WLUN), NULL);
	if (IS_ERR(hba->sdev_ufs_device)) {
		ret = PTR_ERR(hba->sdev_ufs_device);
		hba->sdev_ufs_device = NULL;
		goto out;
	}
	scsi_device_put(hba->sdev_ufs_device);

	sdev_rpmb = __scsi_add_device(hba->host, 0, 0,
		ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_RPMB_WLUN), NULL);
	if (IS_ERR(sdev_rpmb)) {
		ret = PTR_ERR(sdev_rpmb);
		goto remove_sdev_ufs_device;
	}
	scsi_device_put(sdev_rpmb);

	sdev_boot = __scsi_add_device(hba->host, 0, 0,
		ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_BOOT_WLUN), NULL);
	if (IS_ERR(sdev_boot))
		dev_err(hba->dev, "%s: BOOT WLUN not found\n", __func__);
	else
		scsi_device_put(sdev_boot);
	goto out;

remove_sdev_ufs_device:
	scsi_remove_device(hba->sdev_ufs_device);
out:
	return ret;
}

static int ufs_get_device_desc(struct ufs_hba *hba,
			       struct ufs_dev_desc *dev_desc)
{
	int err;
	size_t buff_len;
	u8 model_index;
#ifdef CONFIG_QCOM_WB
	u8 *desc_buf, wb_buf[4];
	u32 lun, res;
#else
	u8 *desc_buf;
#endif

	buff_len = max_t(size_t, hba->desc_size.dev_desc,
			 QUERY_DESC_MAX_SIZE + 1);
	desc_buf = kmalloc(buff_len, GFP_KERNEL);
	if (!desc_buf)
		return -ENOMEM;

	err = ufshcd_read_device_desc(hba, desc_buf, hba->desc_size.dev_desc);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading Device Desc. err = %d\n",
			__func__, err);
		goto out;
	}

	/*
	 * getting vendor (manufacturerID) and Bank Index in big endian
	 * format
	 */
	dev_desc->wmanufacturerid = desc_buf[DEVICE_DESC_PARAM_MANF_ID] << 8 |
				     desc_buf[DEVICE_DESC_PARAM_MANF_ID + 1];

	dev_desc->wspecversion = desc_buf[DEVICE_DESC_PARAM_SPEC_VER] << 8 |
				  desc_buf[DEVICE_DESC_PARAM_SPEC_VER + 1];

	dev_err(hba->dev, "%s: UFS spec 0x%04x.\n", __func__, dev_desc->wspecversion);

	model_index = desc_buf[DEVICE_DESC_PARAM_PRDCT_NAME];

#ifdef CONFIG_BLK_TURBO_WRITE
	/* the device desc size of ufs 3.1 is different with the one of prev ver. */
	hba->support_tw = false;
	if (hba->desc_size.dev_desc < (DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP + 3)) {
		dev_err(hba->dev, "%s: desc_size.dev_desc = %d.\n", __func__,
				hba->desc_size.dev_desc);
		goto get_model_string;
	}

	dev_desc->dextfeatsupport = ((desc_buf[DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP] << 24)|
			(desc_buf[DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP + 1] << 16) |
			(desc_buf[DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP + 2] << 8) |
			desc_buf[DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP + 3]);

	if (dev_desc->dextfeatsupport & 0x100) {
		dev_info(hba->dev,"%s: ufs device supports turbo write\n", __func__);
		if (hba->dev_info.i_lt < UFS_TW_DISABLE_THRESHOLD) {
			/* if device supports tw, enable hibern flush */
			err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG,
					QUERY_FLAG_IDN_WB_BUFF_FLUSH_DURING_HIBERN8, NULL);
			if (err) {
				dev_err(hba->dev, "%s: Failed to enable tw hibern flush. err = %d\n",
						__func__, err);
				goto get_model_string;
			}
			hba->support_tw = true;
			dev_info(hba->dev,"%s: ufs turbo write is enabled\n", __func__);
		}
	}

get_model_string:
#endif

#ifdef CONFIG_QCOM_WB
	/* Enable WB only for UFS-3.1 or UFS-2.2 OR if desc len >= 0x59 */
	if ((dev_desc->wspecversion >= 0x310) ||
	    (dev_desc->wspecversion == 0x220) ||
	    (dev_desc->wmanufacturerid == UFS_VENDOR_TOSHIBA &&
	     dev_desc->wspecversion >= 0x300 &&
	     hba->desc_size.dev_desc >= 0x59)) {
		hba->dev_info.d_ext_ufs_feature_sup =
			desc_buf[DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP]
								<< 24 |
			desc_buf[DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP + 1]
								<< 16 |
			desc_buf[DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP + 2]
								<< 8 |
			desc_buf[DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP + 3];
		hba->dev_info.b_wb_buffer_type =
			desc_buf[DEVICE_DESC_PARAM_WB_TYPE];

		if (hba->dev_info.b_wb_buffer_type)
			goto skip_unit_desc;

		hba->dev_info.wb_config_lun = false;
		for (lun = 0; lun < UFS_UPIU_MAX_GENERAL_LUN; lun++) {
			memset(wb_buf, 0, sizeof(wb_buf));
			err = ufshcd_get_wb_alloc_units(hba, lun, wb_buf);
			if (err)
				break;

			res = wb_buf[0] << 24 | wb_buf[1] << 16 |
				wb_buf[2] << 8 | wb_buf[3];
			if (res) {
				hba->dev_info.wb_config_lun = true;
				break;
			}
		}
	}

skip_unit_desc:
#endif
	/* Zero-pad entire buffer for string termination. */
	memset(desc_buf, 0, buff_len);

	err = ufshcd_read_string_desc(hba, model_index, desc_buf,
				      QUERY_DESC_MAX_SIZE, true/*ASCII*/);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading Product Name. err = %d\n",
			__func__, err);
		goto out;
	}

	desc_buf[QUERY_DESC_MAX_SIZE] = '\0';
	strlcpy(dev_desc->model, (desc_buf + QUERY_DESC_HDR_SIZE),
		min_t(u8, desc_buf[QUERY_DESC_LENGTH_OFFSET],
		      MAX_MODEL_LEN));

	/* Null terminate the model string */
	dev_desc->model[MAX_MODEL_LEN] = '\0';

out:
	kfree(desc_buf);
	return err;
}

static void ufs_fixup_device_setup(struct ufs_hba *hba,
				   struct ufs_dev_desc *dev_desc)
{
	struct ufs_dev_fix *f;

	for (f = ufs_fixups; f->quirk; f++) {
		if ((f->w_manufacturer_id == dev_desc->wmanufacturerid ||
		     f->w_manufacturer_id == UFS_ANY_VENDOR) &&
		    (STR_PRFX_EQUAL(f->model, dev_desc->model) ||
		     !strcmp(f->model, UFS_ANY_MODEL)))
			hba->dev_info.quirks |= f->quirk;
	}
}

/**
 * ufshcd_tune_pa_tactivate - Tunes PA_TActivate of local UniPro
 * @hba: per-adapter instance
 *
 * PA_TActivate parameter can be tuned manually if UniPro version is less than
 * 1.61. PA_TActivate needs to be greater than or equal to peerM-PHY's
 * RX_MIN_ACTIVATETIME_CAPABILITY attribute. This optimal value can help reduce
 * the hibern8 exit latency.
 *
 * Returns zero on success, non-zero error value on failure.
 */
static int ufshcd_tune_pa_tactivate(struct ufs_hba *hba)
{
	int ret = 0;
	u32 peer_rx_min_activatetime = 0, tuned_pa_tactivate;

	if (!ufshcd_is_unipro_pa_params_tuning_req(hba))
		return 0;

	ret = ufshcd_dme_peer_get(hba,
				  UIC_ARG_MIB_SEL(
					RX_MIN_ACTIVATETIME_CAPABILITY,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)),
				  &peer_rx_min_activatetime);
	if (ret)
		goto out;

	/* make sure proper unit conversion is applied */
	tuned_pa_tactivate =
		((peer_rx_min_activatetime * RX_MIN_ACTIVATETIME_UNIT_US)
		 / PA_TACTIVATE_TIME_UNIT_US);
	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE),
			     tuned_pa_tactivate);

out:
	return ret;
}

/**
 * ufshcd_tune_pa_hibern8time - Tunes PA_Hibern8Time of local UniPro
 * @hba: per-adapter instance
 *
 * PA_Hibern8Time parameter can be tuned manually if UniPro version is less than
 * 1.61. PA_Hibern8Time needs to be maximum of local M-PHY's
 * TX_HIBERN8TIME_CAPABILITY & peer M-PHY's RX_HIBERN8TIME_CAPABILITY.
 * This optimal value can help reduce the hibern8 exit latency.
 *
 * Returns zero on success, non-zero error value on failure.
 */
static int ufshcd_tune_pa_hibern8time(struct ufs_hba *hba)
{
	int ret = 0;
	u32 local_tx_hibern8_time_cap = 0, peer_rx_hibern8_time_cap = 0;
	u32 max_hibern8_time, tuned_pa_hibern8time;
	u32 pa_hibern8time_quirk_enabled = hba->dev_info.quirks &
		UFS_DEVICE_QUIRK_PA_HIBER8TIME;

	if (!ufshcd_is_unipro_pa_params_tuning_req(hba) &&
	    !pa_hibern8time_quirk_enabled) {
		return 0;
	}

	ret = ufshcd_dme_get(hba,
			     UIC_ARG_MIB_SEL(TX_HIBERN8TIME_CAPABILITY,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
				  &local_tx_hibern8_time_cap);
	if (ret)
		goto out;

	ret = ufshcd_dme_peer_get(hba,
				  UIC_ARG_MIB_SEL(RX_HIBERN8TIME_CAPABILITY,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)),
				  &peer_rx_hibern8_time_cap);
	if (ret)
		goto out;

	max_hibern8_time = max(local_tx_hibern8_time_cap,
			       peer_rx_hibern8_time_cap);
	/* make sure proper unit conversion is applied */
	tuned_pa_hibern8time = ((max_hibern8_time * HIBERN8TIME_UNIT_US)
				/ PA_HIBERN8_TIME_UNIT_US);
	/* PA_HIBERN8TIME is product of tuned_pa_hibern8time * granularity,
	 * setting tuned_pa_hibern8time as 3 and since granularity is 100us
	 * for both host and device side, 3 *100us = 300us is set as
	 * PA_HIBERN8TIME if UFS_DEVICE_QUIRK_PA_HIBER8TIME quirk is enabled
	 */
	if (pa_hibern8time_quirk_enabled)
		tuned_pa_hibern8time = 3; /* 3 *100us =300us */
	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_HIBERN8TIME),
			     tuned_pa_hibern8time);
out:
	return ret;
}

/**
 * ufshcd_quirk_tune_host_pa_tactivate - Ensures that host PA_TACTIVATE is
 * less than device PA_TACTIVATE time.
 * @hba: per-adapter instance
 *
 * Some UFS devices require host PA_TACTIVATE to be lower than device
 * PA_TACTIVATE, we need to enable UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE quirk
 * for such devices.
 *
 * Returns zero on success, non-zero error value on failure.
 */
static int ufshcd_quirk_tune_host_pa_tactivate(struct ufs_hba *hba)
{
	int ret = 0;
	u32 granularity, peer_granularity;
	u32 pa_tactivate, peer_pa_tactivate;
	u32 pa_tactivate_us, peer_pa_tactivate_us;
	u8 gran_to_us_table[] = {1, 4, 8, 16, 32, 100};

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_GRANULARITY),
				  &granularity);
	if (ret)
		goto out;

	ret = ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_GRANULARITY),
				  &peer_granularity);
	if (ret)
		goto out;

	if ((granularity < PA_GRANULARITY_MIN_VAL) ||
	    (granularity > PA_GRANULARITY_MAX_VAL)) {
		dev_err(hba->dev, "%s: invalid host PA_GRANULARITY %d",
			__func__, granularity);
		return -EINVAL;
	}

	if ((peer_granularity < PA_GRANULARITY_MIN_VAL) ||
	    (peer_granularity > PA_GRANULARITY_MAX_VAL)) {
		dev_err(hba->dev, "%s: invalid device PA_GRANULARITY %d",
			__func__, peer_granularity);
		return -EINVAL;
	}

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_TACTIVATE), &pa_tactivate);
	if (ret)
		goto out;

	ret = ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_TACTIVATE),
				  &peer_pa_tactivate);
	if (ret)
		goto out;

	pa_tactivate_us = pa_tactivate * gran_to_us_table[granularity - 1];
	peer_pa_tactivate_us = peer_pa_tactivate *
			     gran_to_us_table[peer_granularity - 1];

	if (pa_tactivate_us > peer_pa_tactivate_us) {
		u32 new_peer_pa_tactivate;

		new_peer_pa_tactivate = pa_tactivate_us /
				      gran_to_us_table[peer_granularity - 1];
		new_peer_pa_tactivate++;
		ret = ufshcd_dme_peer_set(hba, UIC_ARG_MIB(PA_TACTIVATE),
					  new_peer_pa_tactivate);
	}

out:
	return ret;
}

static void ufshcd_tune_unipro_params(struct ufs_hba *hba)
{
	if (ufshcd_is_unipro_pa_params_tuning_req(hba))
		ufshcd_tune_pa_tactivate(hba);

	ufshcd_tune_pa_hibern8time(hba);
	if (hba->dev_info.quirks & UFS_DEVICE_QUIRK_PA_TACTIVATE)
		/* set 1ms timeout for PA_TACTIVATE */
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE), 10);

	if (hba->dev_info.quirks & UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE)
		ufshcd_quirk_tune_host_pa_tactivate(hba);

	ufshcd_vops_apply_dev_quirks(hba);
}

static void ufshcd_clear_dbg_ufs_stats(struct ufs_hba *hba)
{
	int err_reg_hist_size = sizeof(struct ufs_uic_err_reg_hist);

	memset(&hba->ufs_stats.pa_err, 0, err_reg_hist_size);
	memset(&hba->ufs_stats.dl_err, 0, err_reg_hist_size);
	memset(&hba->ufs_stats.nl_err, 0, err_reg_hist_size);
	memset(&hba->ufs_stats.tl_err, 0, err_reg_hist_size);
	memset(&hba->ufs_stats.dme_err, 0, err_reg_hist_size);

	hba->req_abort_count = 0;
}

static void ufshcd_init_desc_sizes(struct ufs_hba *hba)
{
	int err;

	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_DEVICE, 0,
		&hba->desc_size.dev_desc);
	if (err)
		hba->desc_size.dev_desc = QUERY_DESC_DEVICE_DEF_SIZE;

	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_POWER, 0,
		&hba->desc_size.pwr_desc);
	if (err)
		hba->desc_size.pwr_desc = QUERY_DESC_POWER_DEF_SIZE;

	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_INTERCONNECT, 0,
		&hba->desc_size.interc_desc);
	if (err)
		hba->desc_size.interc_desc = QUERY_DESC_INTERCONNECT_DEF_SIZE;

	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_CONFIGURATION, 0,
		&hba->desc_size.conf_desc);
	if (err)
		hba->desc_size.conf_desc = QUERY_DESC_CONFIGURATION_DEF_SIZE;

	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_UNIT, 0,
		&hba->desc_size.unit_desc);
	if (err)
		hba->desc_size.unit_desc = QUERY_DESC_UNIT_DEF_SIZE;

	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_GEOMETRY, 0,
		&hba->desc_size.geom_desc);
	if (err)
		hba->desc_size.geom_desc = QUERY_DESC_GEOMETRY_DEF_SIZE;

	err = ufshcd_read_desc_length(hba, QUERY_DESC_IDN_HEALTH, 0,
		&hba->desc_size.hlth_desc);
	if (err)
		hba->desc_size.hlth_desc = QUERY_DESC_HEALTH_DEF_SIZE;
}

static void ufshcd_def_desc_sizes(struct ufs_hba *hba)
{
	hba->desc_size.dev_desc = QUERY_DESC_DEVICE_DEF_SIZE;
	hba->desc_size.pwr_desc = QUERY_DESC_POWER_DEF_SIZE;
	hba->desc_size.interc_desc = QUERY_DESC_INTERCONNECT_DEF_SIZE;
	hba->desc_size.conf_desc = QUERY_DESC_CONFIGURATION_DEF_SIZE;
	hba->desc_size.unit_desc = QUERY_DESC_UNIT_DEF_SIZE;
	hba->desc_size.geom_desc = QUERY_DESC_GEOMETRY_DEF_SIZE;
	hba->desc_size.str_desc = QUERY_DESC_STRING_DEF_SIZE;
	hba->desc_size.hlth_desc = QUERY_DESC_HEALTH_DEF_SIZE;
}

void ufshcd_apply_pm_quirks(struct ufs_hba *hba)
{
	if (hba->dev_info.quirks & UFS_DEVICE_QUIRK_NO_LINK_OFF) {
		if (ufs_get_pm_lvl_to_link_pwr_state(hba->rpm_lvl) ==
		    UIC_LINK_OFF_STATE) {
			hba->rpm_lvl =
				ufs_get_desired_pm_lvl_for_dev_link_state(
						UFS_SLEEP_PWR_MODE,
						UIC_LINK_HIBERN8_STATE);
			dev_info(hba->dev, "UFS_DEVICE_QUIRK_NO_LINK_OFF enabled, changed rpm_lvl to %d\n",
				hba->rpm_lvl);
		}
		if (ufs_get_pm_lvl_to_link_pwr_state(hba->spm_lvl) ==
		    UIC_LINK_OFF_STATE) {
			hba->spm_lvl =
				ufs_get_desired_pm_lvl_for_dev_link_state(
						UFS_SLEEP_PWR_MODE,
						UIC_LINK_HIBERN8_STATE);
			dev_info(hba->dev, "UFS_DEVICE_QUIRK_NO_LINK_OFF enabled, changed spm_lvl to %d\n",
				hba->spm_lvl);
		}
	}
}
EXPORT_SYMBOL(ufshcd_apply_pm_quirks);

/**
 * ufshcd_set_dev_ref_clk - set the device bRefClkFreq
 * @hba: per-adapter instance
 *
 * Read the current value of the bRefClkFreq attribute from device and update it
 * if host is supplying different reference clock frequency than one mentioned
 * in bRefClkFreq attribute.
 *
 * Returns zero on success, non-zero error value on failure.
 */
static int ufshcd_set_dev_ref_clk(struct ufs_hba *hba)
{
	int err = 0;
	int ref_clk = -1;
	static const char * const ref_clk_freqs[] = {"19.2 MHz", "26 MHz",
						     "38.4 MHz", "52 MHz"};

	err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_REF_CLK_FREQ, 0, 0, &ref_clk);

	if (err) {
		dev_err(hba->dev, "%s: failed reading bRefClkFreq. err = %d\n",
			 __func__, err);
		goto out;
	}

	if ((ref_clk < 0) || (ref_clk > REF_CLK_FREQ_52_MHZ)) {
		dev_err(hba->dev, "%s: invalid ref_clk setting = %d\n",
			 __func__, ref_clk);
		err = -EINVAL;
		goto out;
	}

	if (ref_clk == hba->dev_ref_clk_freq)
		goto out; /* nothing to update */

	err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
			QUERY_ATTR_IDN_REF_CLK_FREQ, 0, 0,
			&hba->dev_ref_clk_freq);

	if (err)
		dev_err(hba->dev, "%s: bRefClkFreq setting to %s failed\n",
			__func__, ref_clk_freqs[hba->dev_ref_clk_freq]);
	else
		/*
		 * It is good to print this out here to debug any later failures
		 * related to gear switch.
		 */
		dev_info(hba->dev, "%s: bRefClkFreq setting to %s succeeded\n",
			__func__, ref_clk_freqs[hba->dev_ref_clk_freq]);

out:
	return err;
}

static int ufshcd_get_dev_ref_clk_gating_wait(struct ufs_hba *hba,
					struct ufs_dev_desc *dev_desc)
{
	int err = 0;
	u32 gating_wait = UFSHCD_REF_CLK_GATING_WAIT_US;

	if (dev_desc->wspecversion >= 0x300) {
		err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
				QUERY_ATTR_IDN_REF_CLK_GATING_WAIT_TIME, 0, 0,
				&gating_wait);

		if (err)
			dev_err(hba->dev, "failed reading bRefClkGatingWait. err = %d, use default %uus\n",
					err, gating_wait);

		if (gating_wait == 0) {
			gating_wait = UFSHCD_REF_CLK_GATING_WAIT_US;
			dev_err(hba->dev, "undefined ref clk gating wait time, use default %uus\n",
					gating_wait);
		}
	}

	hba->dev_ref_clk_gating_wait = gating_wait;
	return err;
}

/*
 * UN policy
 *   20 digits : manid + mandate + serial number
 *               (sec, hynix : 7byte hex, toshiba : 6byte + 00, ascii)
 */
static void ufs_set_sec_unique_number(struct ufs_hba *hba, u8 *str_desc_buf, u8 *desc_buf)
{
	u8 manid;
	u8 snum_buf[UFS_UN_MAX_DIGITS + 1];

	manid = hba->dev_info.w_manufacturer_id & 0xFF;
	memset(hba->unique_number, 0, sizeof(hba->unique_number));
	memset(snum_buf, 0, sizeof(snum_buf));

	memcpy(snum_buf, str_desc_buf + QUERY_DESC_HDR_SIZE, SERIAL_NUM_SIZE);

#if defined(CONFIG_UFS_UN_20DIGITS)
	sprintf(hba->unique_number, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		manid,
		desc_buf[DEVICE_DESC_PARAM_MANF_DATE], desc_buf[DEVICE_DESC_PARAM_MANF_DATE+1],
		snum_buf[0], snum_buf[1], snum_buf[2], snum_buf[3], snum_buf[4], snum_buf[5], snum_buf[6]);
#endif

	/* Null terminate the unique number string */
	hba->unique_number[UFS_UN_MAX_DIGITS] = '\0';

	hba->dev_info.w_manufacturer_date =
		desc_buf[DEVICE_DESC_PARAM_MANF_DATE] << 8 | desc_buf[DEVICE_DESC_PARAM_MANF_DATE+1];
}

static int ufs_read_device_desc_data(struct ufs_hba *hba)
{
	int err = 0;
	u8 *desc_buf = NULL;
	u8 *str_desc_buf = NULL;
	u8 *health_buf = NULL;
	u8 serial_num_index;

	if (hba->desc_size.dev_desc) {
		desc_buf = kmalloc(hba->desc_size.dev_desc, GFP_KERNEL);
		if (!desc_buf) {
			dev_err(hba->dev,
				"%s: Failed to allocate desc_buf\n", __func__);
			return -ENOMEM;
		}
	}
	err = ufshcd_read_device_desc(hba, desc_buf, hba->desc_size.dev_desc);
	if (err)
		goto out;

	/*
	 * getting vendor (manufacturerID) and Bank Index in big endian
	 * format
	 */
	hba->dev_info.w_manufacturer_id =
		desc_buf[DEVICE_DESC_PARAM_MANF_ID] << 8 |
		desc_buf[DEVICE_DESC_PARAM_MANF_ID + 1];
	hba->dev_info.b_device_sub_class =
		desc_buf[DEVICE_DESC_PARAM_DEVICE_SUB_CLASS];
	hba->dev_info.i_product_name = desc_buf[DEVICE_DESC_PARAM_PRDCT_NAME];
	hba->dev_info.w_spec_version =
		desc_buf[DEVICE_DESC_PARAM_SPEC_VER] << 8 |
		desc_buf[DEVICE_DESC_PARAM_SPEC_VER + 1];
	serial_num_index = desc_buf[DEVICE_DESC_PARAM_SN];

	if (hba->desc_size.str_desc) {
		str_desc_buf = kmalloc(hba->desc_size.str_desc + 1, GFP_KERNEL);
		if (!str_desc_buf) {
			err = -ENOMEM;
			dev_err(hba->dev,
				"%s: Failed to allocate str_desc_buf\n", __func__);
			goto out;
		}
	}
	memset(str_desc_buf, 0, hba->desc_size.str_desc + 1);
	err = ufshcd_read_string_desc(hba, serial_num_index,
			str_desc_buf, hba->desc_size.str_desc, UTF16_STD);
	if (err)
		goto out;

	str_desc_buf[hba->desc_size.str_desc] = '\0';

	ufs_set_sec_unique_number(hba, str_desc_buf, desc_buf);

	if (hba->desc_size.hlth_desc) {
		health_buf = kmalloc(hba->desc_size.hlth_desc, GFP_KERNEL);
		if (!health_buf) {
			err = -ENOMEM;
			dev_err(hba->dev,
				"%s: Failed to allocate health_buf\n", __func__);
			goto out;
		}
	}
	err = ufshcd_read_desc(hba, QUERY_DESC_IDN_HEALTH, 0, health_buf,
			hba->desc_size.hlth_desc);
	if (err) {
		hba->dev_info.i_lt = 0x0;
		dev_err(hba->dev, "%s: HEALTH desc read fail, err = %d\n",
				__func__, err);
	} else {
		hba->dev_info.i_lt = health_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_A];
		dev_info(hba->dev, "LT: 0x%01x%01x\n", health_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_A],
				health_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_B]);
	}

out:
	if (health_buf)
		kfree(health_buf);
	if (str_desc_buf)
		kfree(str_desc_buf);
	if (desc_buf)
		kfree(desc_buf);

	return err;
}

static inline bool ufshcd_needs_reinit(struct ufs_hba *hba)
{
	bool reinit = false;

	if (hba->dev_info.w_spec_version < 0x300 && hba->phy_init_g4) {
		dev_warn(hba->dev, "%s: Using force-g4 setting for a non-g4 device, re-init\n",
				  __func__);
		hba->phy_init_g4 = false;
		reinit = true;
	} else if (hba->dev_info.w_spec_version >= 0x300 && !hba->phy_init_g4) {
		dev_warn(hba->dev, "%s: Re-init UFS host to use proper PHY settings for the UFS device. This can be avoided by setting the force-g4 in DT\n",
				  __func__);
		hba->phy_init_g4 = true;
		reinit = true;
	}

	return reinit;
}

static int ufshcd_reset_all_before_retry_setup_link(struct ufs_hba *hba)
{
	int err = 0;

	err = ufshcd_vops_full_reset(hba);
	if (err)
		dev_warn(hba->dev, "%s: full reset returned %d\n",
				__func__, err);

	err = ufshcd_reset_device(hba);
	if (err)
		dev_warn(hba->dev, "%s: device reset failed. err %d\n",
				__func__, err);

	ufshcd_hba_stop(hba, false);

	/* scale up clocks to max frequency before full reinitialization */
	ufshcd_set_clk_freq(hba, true);

	err = ufshcd_hba_enable(hba);
	if (err)
		dev_warn(hba->dev, "%s: hba_enable failed. err %d\n",
				__func__, err);

	return err;
}

/**
 * ufshcd_probe_hba - probe hba to detect device and initialize
 * @hba: per-adapter instance
 *
 * Execute link-startup and verify device initialization
 */
static int ufshcd_probe_hba(struct ufs_hba *hba)
{
	struct ufs_dev_desc card = {0};
	int ret;
	ktime_t start = ktime_get();
	int link_retry_count = 0;
	unsigned long flags;

reinit:
	ret = ufshcd_link_startup(hba);
	if (ret)
		goto out;

	/* Debug counters initialization */
	ufshcd_clear_dbg_ufs_stats(hba);
	/* set the default level for urgent bkops */
	hba->urgent_bkops_lvl = BKOPS_STATUS_PERF_IMPACT;
	hba->is_urgent_bkops_lvl_checked = false;

	/* UniPro link is active now */
	ufshcd_set_link_active(hba);

	ret = ufshcd_verify_dev_init(hba);
	if (ret)
		goto out;

	ret = ufshcd_complete_dev_init(hba);
	if (ret)
		goto out;

	/* clear any previous UFS device information */
	memset(&hba->dev_info, 0, sizeof(hba->dev_info));

	/* cache important parameters from device descriptor for later use */
	ret = ufs_read_device_desc_data(hba);
	if (ret)
		goto out;

	/* Init check for device descriptor sizes */
	ufshcd_init_desc_sizes(hba);

	ret = ufs_get_device_desc(hba, &card);
	if (ret) {
		dev_err(hba->dev, "%s: Failed getting device info. err = %d\n",
			__func__, ret);
		goto out;
	}

	if (ufshcd_needs_reinit(hba)) {
		unsigned long flags;
		int err;
		struct ufs_qcom_host *host = ufshcd_get_variant(hba);

		err = ufshcd_vops_full_reset(hba);
		if (err)
			dev_warn(hba->dev, "%s: full reset returned %d\n",
				 __func__, err);

		err = ufshcd_reset_device(hba);
		if (err)
			dev_warn(hba->dev, "%s: device reset failed. err %d\n",
				 __func__, err);

		/* reset the SEC error info */
		memset(&(hba->SEC_err_info), 0, sizeof(struct SEC_UFS_counting));
		/* decrease UFS hw reset count due to reset for re-init */
		host->hw_reset_count--;

		/* Reset the host controller */
		spin_lock_irqsave(hba->host->host_lock, flags);
		ufshcd_hba_stop(hba, false);
		spin_unlock_irqrestore(hba->host->host_lock, flags);

		err = ufshcd_hba_enable(hba);
		if (err)
			goto out;

		goto reinit;
	}

	ufs_fixup_device_setup(hba, &card);
	ufshcd_tune_unipro_params(hba);

	ufshcd_apply_pm_quirks(hba);
	if (card.wspecversion < 0x300) {
		ret = ufshcd_set_vccq_rail_unused(hba,
			(hba->dev_info.quirks & UFS_DEVICE_NO_VCCQ) ?
			true : false);
		if (ret)
			goto out;
	}

	/*
	 * On 4.19 kernel, the controlling of Vccq requlator got changed.
	 * Its relying on sys_suspend_pwr_off regulator dt flag instead of spm
	 * level.
	 * Updated logic is not honoring spm level specified and the device
	 * specific quirks for the desired link state.
	 * Below change is to fix above listed issue without distrubing the
	 * present logic.
	 */
	if (hba->spm_lvl == ufs_get_desired_pm_lvl_for_dev_link_state(
				UFS_POWERDOWN_PWR_MODE,
				UIC_LINK_OFF_STATE)) {
		if ((hba->dev_info.w_spec_version >= 0x300 &&
		     hba->vreg_info.vccq &&
		     !hba->vreg_info.vccq->sys_suspend_pwr_off))
			hba->vreg_info.vccq->sys_suspend_pwr_off = true;

		if ((hba->dev_info.w_spec_version < 0x300 &&
		     !hba->vreg_info.vccq2->sys_suspend_pwr_off))
			hba->vreg_info.vccq2->sys_suspend_pwr_off = true;
	}

	/* UFS device is also active now */
	ufshcd_set_ufs_dev_active(hba);
	ufshcd_force_reset_auto_bkops(hba);

	if (ufshcd_get_max_pwr_mode(hba)) {
		dev_err(hba->dev,
			"%s: Failed getting max supported power mode\n",
			__func__);
	} else {
		ufshcd_get_dev_ref_clk_gating_wait(hba, &card);

		/*
		 * Set the right value to bRefClkFreq before attempting to
		 * switch to HS gears.
		 */
		ufshcd_set_dev_ref_clk(hba);
		ret = ufshcd_config_pwr_mode(hba, &hba->max_pwr_info.info);
		if (ret) {
			dev_err(hba->dev, "%s: Failed setting power mode, err = %d\n",
					__func__, ret);
			goto out;
		}
	}

	/*
	 * bActiveICCLevel is volatile for UFS device (as per latest v2.1 spec)
	 * and for removable UFS card as well, hence always set the parameter.
	 * Note: Error handler may issue the device reset hence resetting
	 *       bActiveICCLevel as well so it is always safe to set this here.
	 */
	ufshcd_set_active_icc_lvl(hba);

	/* set the state as operational after switching to desired gear */
	hba->ufshcd_state = UFSHCD_STATE_OPERATIONAL;

#ifdef CONFIG_BLK_TURBO_WRITE
	if (hba->support_tw) {
		if (!ufshcd_eh_in_progress(hba) && !hba->pm_op_in_progress) {
			hba->SEC_tw_info.tw_state_ts = jiffies;
			get_monotonic_boottime(&(hba->SEC_tw_info_old.timestamp));
		}
	}
#endif
#ifdef CONFIG_QCOM_WB
	ufshcd_wb_config(hba);
#endif

	/*
	 * Enable auto hibern8 if supported, after full host and
	 * device initialization.
	 */
	ufshcd_set_auto_hibern8_timer(hba);

	/*
	 * If we are in error handling context or in power management callbacks
	 * context, no need to scan the host
	 */
	if (!ufshcd_eh_in_progress(hba) && !hba->pm_op_in_progress) {
		bool flag;

		if (!ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_READ_FLAG,
				QUERY_FLAG_IDN_PWR_ON_WPE, &flag))
			hba->dev_info.f_power_on_wp_en = flag;

		/* Add required well known logical units to scsi mid layer */
		ret = ufshcd_scsi_add_wlus(hba);
		if (ret)
			goto out;

		/* lower VCC voltage level */
		ufshcd_set_low_vcc_level(hba, &card);

		/* Initialize devfreq after UFS device is detected */
		if (ufshcd_is_clkscaling_supported(hba)) {
			memcpy(&hba->clk_scaling.saved_pwr_info.info,
				&hba->pwr_info,
				sizeof(struct ufs_pa_layer_attr));
			hba->clk_scaling.saved_pwr_info.is_valid = true;
			hba->clk_scaling.is_scaled_up = true;
			if (!hba->devfreq) {
				ret = ufshcd_devfreq_init(hba);
				if (ret)
					goto out;
			}
			hba->clk_scaling.is_allowed = true;
			hba->clk_scaling.is_suspended = false;
		}

		scsi_scan_host(hba->host);
#if defined(CONFIG_UFSFEATURE)
		ufsf_device_check(hba);
		ufsf_hpb_init(&hba->ufsf);
		if (hba->ufsf.hpb_dev_info.hpb_device) {
			ufshcd_add_hpb_info_sysfs_node(hba);
			get_monotonic_boottime(&(hba->SEC_hpb_info.timestamp_old));
		}
#endif
		pm_runtime_put_sync(hba->dev);
	}

out:
	if (ret && link_retry_count++ < UFS_LINK_SETUP_RETRIES) {
		dev_err(hba->dev, "%s: error with %d, and will be reset.(%d)\n",
				__func__, ret, link_retry_count);
#if defined(CONFIG_SCSI_UFS_TEST_MODE)
		ufshcd_vops_dbg_register_dump(hba, false);
#endif
		if (!ufshcd_reset_all_before_retry_setup_link(hba))
			goto reinit;
	}

	if (ret) {
		dev_err(hba->dev, "%s: UFS link setup is failed with %d.\n",
		       __func__, ret);

		ufsdbg_set_err_state(hba);
		ufshcd_print_host_state(hba);
		ufshcd_print_pwr_info(hba);
		ufshcd_print_host_regs(hba);
		ufshcd_print_cmd_log(hba);

		ufshcd_set_ufs_dev_poweroff(hba);
		ufshcd_set_link_off(hba);

		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->ufshcd_state = UFSHCD_STATE_ERROR;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}

	/*
	 * If we failed to initialize the device or the device is not
	 * present, turn off the power/clocks etc.
	 */
	if (ret && !ufshcd_eh_in_progress(hba) && !hba->pm_op_in_progress) {
		pm_runtime_put_sync(hba->dev);
		ufshcd_exit_clk_scaling(hba);
		ufshcd_hba_exit(hba);
	}

#if defined(CONFIG_UFSFEATURE)
	ufsf_hpb_reset(&hba->ufsf);
#endif

	trace_ufshcd_init(dev_name(hba->dev), ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	return ret;
}

/**
 * ufshcd_async_scan - asynchronous execution for probing hba
 * @data: data pointer to pass to this function
 * @cookie: cookie data
 */
static void ufshcd_async_scan(void *data, async_cookie_t cookie)
{
	struct ufs_hba *hba = (struct ufs_hba *)data;

	/*
	 * Don't allow clock gating and hibern8 enter for faster device
	 * detection.
	 */
	ufshcd_hold_all(hba);
	ufshcd_probe_hba(hba);
	ufshcd_release_all(hba);
}

static enum blk_eh_timer_return ufshcd_eh_timed_out(struct scsi_cmnd *scmd)
{
	unsigned long flags;
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	int index;
	bool found = false;

	if (!scmd || !scmd->device || !scmd->device->host)
		return BLK_EH_DONE;

	host = scmd->device->host;
	hba = shost_priv(host);
	if (!hba)
		return BLK_EH_DONE;

	spin_lock_irqsave(host->host_lock, flags);

	for_each_set_bit(index, &hba->outstanding_reqs, hba->nutrs) {
		if (hba->lrb[index].cmd == scmd) {
			found = true;
			break;
		}
	}

	spin_unlock_irqrestore(host->host_lock, flags);

	/*
	 * Bypass SCSI error handling and reset the block layer timer if this
	 * SCSI command was not actually dispatched to UFS driver, otherwise
	 * let SCSI layer handle the error as usual.
	 */
	return found ? BLK_EH_DONE : BLK_EH_RESET_TIMER;
}

/**
 * ufshcd_query_ioctl - perform user read queries
 * @hba: per-adapter instance
 * @lun: used for lun specific queries
 * @buffer: user space buffer for reading and submitting query data and params
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct ufs_ioctl_query_data.
 * It will read the opcode, idn and buf_length parameters, and, put the
 * response in the buffer field while updating the used size in buf_length.
 */
static int ufshcd_query_ioctl(struct ufs_hba *hba, u8 lun, void __user *buffer)
{
	struct ufs_ioctl_query_data *ioctl_data;
	int err = 0;
	int length = 0;
	void *data_ptr;
	bool flag;
	u32 att;
	u8 index;
	u8 *desc = NULL;

	ioctl_data = kzalloc(sizeof(struct ufs_ioctl_query_data), GFP_KERNEL);
	if (!ioctl_data) {
		err = -ENOMEM;
		goto out;
	}

	/* extract params from user buffer */
	err = copy_from_user(ioctl_data, buffer,
			sizeof(struct ufs_ioctl_query_data));
	if (err) {
		dev_err(hba->dev,
			"%s: Failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}

#if defined(CONFIG_UFSFEATURE)
	if (ufsf_check_query(ioctl_data->opcode)) {
		err = ufsf_query_ioctl(&hba->ufsf, lun, buffer, ioctl_data,
				UFSFEATURE_SELECTOR);
		goto out_release_mem;
	}
#endif

	/* verify legal parameters & send query */
	switch (ioctl_data->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		switch (ioctl_data->idn) {
		case QUERY_DESC_IDN_DEVICE:
		case QUERY_DESC_IDN_CONFIGURATION:
		case QUERY_DESC_IDN_INTERCONNECT:
		case QUERY_DESC_IDN_GEOMETRY:
		case QUERY_DESC_IDN_POWER:
			index = 0;
			break;
		case QUERY_DESC_IDN_HEALTH:
			index = 0;
			break;
		case QUERY_DESC_IDN_STRING:
			index = 0;
			if (ioctl_data->buf_size > 0) {
				/* extract params from user buffer */
				err = copy_from_user(&index,
						buffer + sizeof(struct ufs_ioctl_query_data),
						sizeof(u8));
				if (err) {
					dev_err(hba->dev,
						"%s: Failed copying buffer from user, err %d\n",
						__func__, err);
					goto out_release_mem;
				}
			}
			break;
		case QUERY_DESC_IDN_UNIT:
			if (!ufs_is_valid_unit_desc_lun(lun)) {
				dev_err(hba->dev,
					"%s: No unit descriptor for lun 0x%x\n",
					__func__, lun);
				err = -EINVAL;
				goto out_release_mem;
			}
			index = lun;
			break;
		default:
			goto out_einval;
		}
		length = min_t(int, QUERY_DESC_MAX_SIZE,
				ioctl_data->buf_size);
		desc = kzalloc(length, GFP_KERNEL);
		if (!desc) {
			dev_err(hba->dev, "%s: Failed allocating %d bytes\n",
					__func__, length);
			err = -ENOMEM;
			goto out_release_mem;
		}
		err = ufshcd_query_descriptor_retry(hba, ioctl_data->opcode,
				ioctl_data->idn, index, 0, desc, &length);
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		switch (ioctl_data->idn) {
		case QUERY_ATTR_IDN_BOOT_LU_EN:
		case QUERY_ATTR_IDN_POWER_MODE:
		case QUERY_ATTR_IDN_ACTIVE_ICC_LVL:
		case QUERY_ATTR_IDN_OOO_DATA_EN:
		case QUERY_ATTR_IDN_BKOPS_STATUS:
		case QUERY_ATTR_IDN_PURGE_STATUS:
		case QUERY_ATTR_IDN_MAX_DATA_IN:
		case QUERY_ATTR_IDN_MAX_DATA_OUT:
		case QUERY_ATTR_IDN_REF_CLK_FREQ:
		case QUERY_ATTR_IDN_CONF_DESC_LOCK:
		case QUERY_ATTR_IDN_MAX_NUM_OF_RTT:
		case QUERY_ATTR_IDN_EE_CONTROL:
		case QUERY_ATTR_IDN_EE_STATUS:
		case QUERY_ATTR_IDN_SECONDS_PASSED:
			index = 0;
			break;
		case QUERY_ATTR_IDN_DYN_CAP_NEEDED:
		case QUERY_ATTR_IDN_CORR_PRG_BLK_NUM:
			index = lun;
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_attr(hba, ioctl_data->opcode,
					ioctl_data->idn, index, 0, &att);
		break;

	case UPIU_QUERY_OPCODE_READ_FLAG:
		switch (ioctl_data->idn) {
		case QUERY_FLAG_IDN_FDEVICEINIT:
		case QUERY_FLAG_IDN_PERMANENT_WPE:
		case QUERY_FLAG_IDN_PWR_ON_WPE:
		case QUERY_FLAG_IDN_BKOPS_EN:
		case QUERY_FLAG_IDN_PURGE_ENABLE:
		case QUERY_FLAG_IDN_FPHYRESOURCEREMOVAL:
		case QUERY_FLAG_IDN_BUSY_RTC:
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_flag_retry(hba, ioctl_data->opcode,
			ioctl_data->idn, &flag);
		break;
	default:
		goto out_einval;
	}

	if (err) {
		dev_err(hba->dev, "%s: Query for idn %d failed\n", __func__,
				ioctl_data->idn);
		goto out_release_mem;
	}

	/*
	 * copy response data
	 * As we might end up reading less data then what is specified in
	 * "ioctl_data->buf_size". So we are updating "ioctl_data->
	 * buf_size" to what exactly we have read.
	 */
	switch (ioctl_data->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		ioctl_data->buf_size = min_t(int, ioctl_data->buf_size, length);
		data_ptr = desc;
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		ioctl_data->buf_size = sizeof(u32);
		data_ptr = &att;
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		ioctl_data->buf_size = 1;
		data_ptr = &flag;
		break;
	default:
		goto out_einval;
	}

	/* copy to user */
	err = copy_to_user(buffer, ioctl_data,
			sizeof(struct ufs_ioctl_query_data));
	if (err)
		dev_err(hba->dev, "%s: Failed copying back to user.\n",
			__func__);
	err = copy_to_user(buffer + sizeof(struct ufs_ioctl_query_data),
			data_ptr, ioctl_data->buf_size);
	if (err)
		dev_err(hba->dev, "%s: err %d copying back to user.\n",
				__func__, err);
	goto out_release_mem;

out_einval:
	dev_err(hba->dev,
		"%s: illegal ufs query ioctl data, opcode 0x%x, idn 0x%x\n",
		__func__, ioctl_data->opcode, (unsigned int)ioctl_data->idn);
	err = -EINVAL;
out_release_mem:
	kfree(ioctl_data);
	kfree(desc);
out:
	return err;
}

/**
 * ufshcd_ioctl - ufs ioctl callback registered in scsi_host
 * @dev: scsi device required for per LUN queries
 * @cmd: command opcode
 * @buffer: user space buffer for transferring data
 *
 * Supported commands:
 * UFS_IOCTL_QUERY
 */
static int ufshcd_ioctl(struct scsi_device *dev, int cmd, void __user *buffer)
{
	struct ufs_hba *hba = shost_priv(dev->host);
	int err = 0;

	BUG_ON(!hba);
	if (!buffer) {
		dev_err(hba->dev, "%s: User buffer is NULL!\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case UFS_IOCTL_QUERY:
		pm_runtime_get_sync(hba->dev);
		err = ufshcd_query_ioctl(hba, ufshcd_scsi_to_upiu_lun(dev->lun),
				buffer);
		pm_runtime_put_sync(hba->dev);
		break;
	default:
		err = -ENOIOCTLCMD;
		dev_dbg(hba->dev, "%s: Unsupported ioctl cmd %d\n", __func__,
			cmd);
		break;
	}

	return err;
}

static const struct attribute_group *ufshcd_driver_groups[] = {
	&ufs_sysfs_unit_descriptor_group,
	&ufs_sysfs_lun_attributes_group,
	NULL,
};

static struct scsi_host_template ufshcd_driver_template = {
	.module			= THIS_MODULE,
	.name			= UFSHCD,
	.proc_name		= UFSHCD,
	.queuecommand		= ufshcd_queuecommand,
	.slave_alloc		= ufshcd_slave_alloc,
	.slave_configure	= ufshcd_slave_configure,
	.slave_destroy		= ufshcd_slave_destroy,
	.change_queue_depth	= ufshcd_change_queue_depth,
	.eh_abort_handler	= ufshcd_abort,
	.eh_device_reset_handler = ufshcd_eh_device_reset_handler,
	.eh_host_reset_handler   = ufshcd_eh_host_reset_handler,
	.eh_timed_out		= ufshcd_eh_timed_out,
#ifdef CONFIG_BLK_TURBO_WRITE
	.tw_ctrl                = ufshcd_tw_ctrl,
#endif
	.ioctl			= ufshcd_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= ufshcd_ioctl,
#endif
	.this_id		= -1,
	.sg_tablesize		= SG_ALL,
	.cmd_per_lun		= UFSHCD_CMD_PER_LUN,
	.can_queue		= UFSHCD_CAN_QUEUE,
	.max_host_blocked	= 1,
	.track_queue_depth	= 1,
	.sdev_groups		= ufshcd_driver_groups,
};

static int ufshcd_config_vreg_load(struct device *dev, struct ufs_vreg *vreg,
				   int ua)
{
	int ret;

	if (!vreg)
		return 0;

	/*
	 * "set_load" operation shall be required on those regulators
	 * which specifically configured current limitation. Otherwise
	 * zero max_uA may cause unexpected behavior when regulator is
	 * enabled or set as high power mode.
	 */
	if (!vreg->max_uA)
		return 0;

	ret = regulator_set_load(vreg->reg, ua);
	if (ret < 0) {
		dev_err(dev, "%s: %s set load (ua=%d) failed, err=%d\n",
				__func__, vreg->name, ua, ret);
	}

	return ret;
}

static inline int ufshcd_config_vreg_lpm(struct ufs_hba *hba,
					 struct ufs_vreg *vreg)
{
	if (!vreg)
		return 0;
	else if (vreg->unused)
		return 0;
	else
		return ufshcd_config_vreg_load(hba->dev, vreg, vreg->min_uA);
}

static inline int ufshcd_config_vreg_hpm(struct ufs_hba *hba,
					 struct ufs_vreg *vreg)
{
	if (!vreg)
		return 0;
	else if (vreg->unused)
		return 0;
	else
		return ufshcd_config_vreg_load(hba->dev, vreg, vreg->max_uA);
}

static int ufshcd_config_vreg(struct device *dev,
		struct ufs_vreg *vreg, bool on)
{
	int ret = 0;
	struct regulator *reg;
	const char *name;
	int min_uV, uA_load;

	BUG_ON(!vreg);

	reg = vreg->reg;
	name = vreg->name;

	if (regulator_count_voltages(reg) > 0) {
		uA_load = on ? vreg->max_uA : 0;
		ret = ufshcd_config_vreg_load(dev, vreg, uA_load);
		if (ret)
			goto out;

		if (vreg->min_uV && vreg->max_uV) {
			min_uV = on ? vreg->min_uV : 0;
			if (vreg->low_voltage_sup && !vreg->low_voltage_active)
				min_uV = vreg->max_uV;

			ret = regulator_set_voltage(reg, min_uV, vreg->max_uV);
			if (ret) {
				dev_err(dev,
					"%s: %s set voltage failed, err=%d\n",
					__func__, name, ret);
				goto out;
			}
		}
	}
out:
	return ret;
}

static int ufshcd_enable_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg)
		goto out;
	else if (vreg->enabled || vreg->unused)
		goto out;

	ret = ufshcd_config_vreg(dev, vreg, true);
	if (!ret)
		ret = regulator_enable(vreg->reg);

	if (!ret)
		vreg->enabled = true;
	else
		dev_err(dev, "%s: %s enable failed, err=%d\n",
				__func__, vreg->name, ret);
out:
	return ret;
}

static int ufshcd_disable_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg)
		goto out;
	else if (!vreg->enabled || vreg->unused)
		goto out;

	ret = regulator_disable(vreg->reg);

	if (!ret) {
		/* ignore errors on applying disable config */
		ufshcd_config_vreg(dev, vreg, false);
		vreg->enabled = false;
	} else {
		dev_err(dev, "%s: %s disable failed, err=%d\n",
				__func__, vreg->name, ret);
	}
out:
	return ret;
}

static int ufshcd_setup_vreg(struct ufs_hba *hba, bool on)
{
	int ret = 0;
	struct device *dev = hba->dev;
	struct ufs_vreg_info *info = &hba->vreg_info;

	if (!info)
		goto out;

	ret = ufshcd_toggle_vreg(dev, info->vcc, on);
	if (ret)
		goto out;

	ret = ufshcd_toggle_vreg(dev, info->vccq, on);
	if (ret)
		goto out;

	ret = ufshcd_toggle_vreg(dev, info->vccq2, on);
	if (ret)
		goto out;

out:
	if (ret) {
		ufshcd_toggle_vreg(dev, info->vccq2, false);
		ufshcd_toggle_vreg(dev, info->vccq, false);
		ufshcd_toggle_vreg(dev, info->vcc, false);
	}
	return ret;
}

static int ufshcd_setup_hba_vreg(struct ufs_hba *hba, bool on)
{
	struct ufs_vreg_info *info = &hba->vreg_info;
	int ret = 0;

	if (info->vdd_hba) {
		ret = ufshcd_toggle_vreg(hba->dev, info->vdd_hba, on);

		if (!ret)
			ufshcd_vops_update_sec_cfg(hba, on);
	}

	return ret;
}

static int ufshcd_get_vreg(struct device *dev, struct ufs_vreg *vreg)
{
	int ret = 0;

	if (!vreg)
		goto out;

	vreg->reg = devm_regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		ret = PTR_ERR(vreg->reg);
		dev_err(dev, "%s: %s get failed, err=%d\n",
				__func__, vreg->name, ret);
	}
out:
	return ret;
}

static int ufshcd_init_vreg(struct ufs_hba *hba)
{
	int ret = 0;
	struct device *dev = hba->dev;
	struct ufs_vreg_info *info = &hba->vreg_info;

	if (!info)
		goto out;

	ret = ufshcd_get_vreg(dev, info->vcc);
	if (ret)
		goto out;

	ret = ufshcd_get_vreg(dev, info->vccq);
	if (ret)
		goto out;

	ret = ufshcd_get_vreg(dev, info->vccq2);
out:
	return ret;
}

static int ufshcd_init_hba_vreg(struct ufs_hba *hba)
{
	struct ufs_vreg_info *info = &hba->vreg_info;

	if (info)
		return ufshcd_get_vreg(hba->dev, info->vdd_hba);

	return 0;
}

static int ufshcd_set_vccq_rail_unused(struct ufs_hba *hba, bool unused)
{
	int ret = 0;
	struct ufs_vreg_info *info = &hba->vreg_info;

	if (!info)
		goto out;
	else if (!info->vccq)
		goto out;

	if (unused) {
		/* shut off the rail here */
		ret = ufshcd_toggle_vreg(hba->dev, info->vccq, false);
		/*
		 * Mark this rail as no longer used, so it doesn't get enabled
		 * later by mistake
		 */
		if (!ret)
			info->vccq->unused = true;
	} else {
		/*
		 * rail should have been already enabled hence just make sure
		 * that unused flag is cleared.
		 */
		info->vccq->unused = false;
	}
out:
	return ret;
}

static int ufshcd_setup_clocks(struct ufs_hba *hba, bool on,
			       bool keep_link_active, bool is_gating_context)
{
	int ret = 0;
	struct ufs_clk_info *clki;
	struct list_head *head = &hba->clk_list_head;
	unsigned long flags;
	ktime_t start = ktime_get();
	bool clk_state_changed = false;

	if (list_empty(head))
		goto out;

	/* call vendor specific bus vote before enabling the clocks */
	if (on) {
		ret = ufshcd_vops_set_bus_vote(hba, on);
		if (ret)
			return ret;
	}

	/*
	 * vendor specific setup_clocks ops may depend on clocks managed by
	 * this standard driver hence call the vendor specific setup_clocks
	 * before disabling the clocks managed here.
	 */
	if (!on) {
		ret = ufshcd_vops_setup_clocks(hba, on, PRE_CHANGE);
		if (ret)
			return ret;
	}

	list_for_each_entry(clki, head, list) {
		if (!IS_ERR_OR_NULL(clki->clk)) {
			/*
			 * To keep link active, both device ref clock and unipro
			 * clock should be kept ON.
			 */
			if (keep_link_active &&
			    (!strcmp(clki->name, "ref_clk") ||
			     !strcmp(clki->name, "core_clk_unipro")))
				continue;

			clk_state_changed = on ^ clki->enabled;
			if (on && !clki->enabled) {
				ret = clk_prepare_enable(clki->clk);
				if (ret) {
					dev_err(hba->dev, "%s: %s prepare enable failed, %d\n",
						__func__, clki->name, ret);
					goto out;
				}
			} else if (!on && clki->enabled) {
				clk_disable_unprepare(clki->clk);
			}
			clki->enabled = on;
			dev_dbg(hba->dev, "%s: clk: %s %sabled\n", __func__,
					clki->name, on ? "en" : "dis");
		}
	}

	/*
	 * vendor specific setup_clocks ops may depend on clocks managed by
	 * this standard driver hence call the vendor specific setup_clocks
	 * after enabling the clocks managed here.
	 */
	if (on) {
		ret = ufshcd_vops_setup_clocks(hba, on, POST_CHANGE);
		if (ret)
			return ret;
	}

	/*
	 * call vendor specific bus vote to remove the vote after
	 * disabling the clocks.
	 */
	if (!on)
		ret = ufshcd_vops_set_bus_vote(hba, on);

out:
	if (ret) {
		if (on)
			/* Can't do much if this fails */
			(void) ufshcd_vops_set_bus_vote(hba, false);
		list_for_each_entry(clki, head, list) {
			if (!IS_ERR_OR_NULL(clki->clk) && clki->enabled)
				clk_disable_unprepare(clki->clk);
		}
	} else if (!ret && on) {
		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->clk_gating.state = CLKS_ON;
		trace_ufshcd_clk_gating(dev_name(hba->dev),
					hba->clk_gating.state);
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		/* restore the secure configuration as clocks are enabled */
		ufshcd_vops_update_sec_cfg(hba, true);
	}

	if (clk_state_changed)
		trace_ufshcd_profile_clk_gating(dev_name(hba->dev),
			(on ? "on" : "off"),
			ktime_to_us(ktime_sub(ktime_get(), start)), ret);
	return ret;
}

static int ufshcd_enable_clocks(struct ufs_hba *hba)
{
	return  ufshcd_setup_clocks(hba, true, false, false);
}

static int ufshcd_disable_clocks(struct ufs_hba *hba,
				 bool is_gating_context)
{
	return  ufshcd_setup_clocks(hba, false, false, is_gating_context);
}

static int ufshcd_disable_clocks_keep_link_active(struct ufs_hba *hba,
					      bool is_gating_context)
{
	return  ufshcd_setup_clocks(hba, false, true, is_gating_context);
}

static int ufshcd_init_clocks(struct ufs_hba *hba)
{
	int ret = 0;
	struct ufs_clk_info *clki;
	struct device *dev = hba->dev;
	struct list_head *head = &hba->clk_list_head;

	if (list_empty(head))
		goto out;

	list_for_each_entry(clki, head, list) {
		if ((!clki->name) ||
		   (!strcmp(clki->name, "core_clk_ice_hw_ctl")))
			continue;

		clki->clk = devm_clk_get(dev, clki->name);
		if (IS_ERR(clki->clk)) {
			ret = PTR_ERR(clki->clk);
			dev_err(dev, "%s: %s clk get failed, %d\n",
					__func__, clki->name, ret);
			goto out;
		}

		if (clki->max_freq) {
			ret = clk_set_rate(clki->clk, clki->max_freq);
			if (ret) {
				dev_err(hba->dev, "%s: %s clk set rate(%dHz) failed, %d\n",
					__func__, clki->name,
					clki->max_freq, ret);
				goto out;
			}
			clki->curr_freq = clki->max_freq;
		}
		dev_dbg(dev, "%s: clk: %s, rate: %lu\n", __func__,
				clki->name, clk_get_rate(clki->clk));
	}
out:
	return ret;
}

static int ufshcd_variant_hba_init(struct ufs_hba *hba)
{
	int err = 0;

	if (!hba->var || !hba->var->vops)
		goto out;

	err = ufshcd_vops_init(hba);
	if (err)
		dev_err(hba->dev, "%s: variant %s init failed err %d\n",
			__func__, ufshcd_get_var_name(hba), err);
out:
	return err;
}

static void ufshcd_variant_hba_exit(struct ufs_hba *hba)
{
	if (!hba->var || !hba->var->vops)
		return;

	ufshcd_vops_exit(hba);
}

static int ufshcd_hba_init(struct ufs_hba *hba)
{
	int err;

	/*
	 * Handle host controller power separately from the UFS device power
	 * rails as it will help controlling the UFS host controller power
	 * collapse easily which is different than UFS device power collapse.
	 * Also, enable the host controller power before we go ahead with rest
	 * of the initialization here.
	 */
	err = ufshcd_init_hba_vreg(hba);
	if (err)
		goto out;

	err = ufshcd_setup_hba_vreg(hba, true);
	if (err)
		goto out;

	err = ufshcd_init_clocks(hba);
	if (err)
		goto out_disable_hba_vreg;

	err = ufshcd_enable_clocks(hba);
	if (err)
		goto out_disable_hba_vreg;

	err = ufshcd_init_vreg(hba);
	if (err)
		goto out_disable_clks;

	err = ufshcd_setup_vreg(hba, true);
	if (err)
		goto out_disable_clks;

	err = ufshcd_variant_hba_init(hba);
	if (err)
		goto out_disable_vreg;

	hba->is_powered = true;
	goto out;

out_disable_vreg:
	ufshcd_setup_vreg(hba, false);
out_disable_clks:
	ufshcd_disable_clocks(hba, false);
out_disable_hba_vreg:
	ufshcd_setup_hba_vreg(hba, false);
out:
	return err;
}

static void ufshcd_hba_exit(struct ufs_hba *hba)
{
	if (hba->is_powered) {
		ufshcd_variant_hba_exit(hba);
		ufshcd_setup_vreg(hba, false);
		if (ufshcd_is_clkscaling_supported(hba))
			if (hba->devfreq)
				ufshcd_suspend_clkscaling(hba);

		if (hba->recovery_wq)
			destroy_workqueue(hba->recovery_wq);
		ufshcd_disable_clocks(hba, false);
		ufshcd_setup_hba_vreg(hba, false);
		hba->is_powered = false;
	}
}

static int
ufshcd_send_request_sense(struct ufs_hba *hba, struct scsi_device *sdp)
{
	unsigned char cmd[6] = {REQUEST_SENSE,
				0,
				0,
				0,
				UFSHCD_REQ_SENSE_SIZE,
				0};
	char *buffer;
	int ret;

	buffer = kzalloc(UFSHCD_REQ_SENSE_SIZE, GFP_KERNEL);
	if (!buffer) {
		ret = -ENOMEM;
		goto out;
	}

	ret = scsi_execute(sdp, cmd, DMA_FROM_DEVICE, buffer,
			UFSHCD_REQ_SENSE_SIZE, NULL, NULL,
			msecs_to_jiffies(1000), 3, 0, RQF_PM, NULL);
	if (ret)
		pr_err("%s: failed with err %d\n", __func__, ret);

	kfree(buffer);
out:
	return ret;
}

/**
 * ufshcd_set_dev_pwr_mode - sends START STOP UNIT command to set device
 *			     power mode
 * @hba: per adapter instance
 * @pwr_mode: device power mode to set
 *
 * Returns 0 if requested power mode is set successfully
 * Returns non-zero if failed to set the requested power mode
 */
static int ufshcd_set_dev_pwr_mode(struct ufs_hba *hba,
				     enum ufs_dev_pwr_mode pwr_mode)
{
	unsigned char cmd[6] = { START_STOP };
	struct scsi_sense_hdr sshdr;
	struct scsi_device *sdp;
	unsigned long flags;
	int ret;
	int retries = 0;

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdp = hba->sdev_ufs_device;
	if (sdp) {
		ret = scsi_device_get(sdp);
		if (!ret && !scsi_device_online(sdp)) {
			ret = -ENODEV;
			scsi_device_put(sdp);
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	/*
	 * If scsi commands fail, the scsi mid-layer schedules scsi error-
	 * handling, which would wait for host to be resumed. Since we know
	 * we are functional while we are here, skip host resume in error
	 * handling context.
	 */
	hba->host->eh_noresume = 1;
	if (!hba->dev_info.is_ufs_dev_wlun_ua_cleared) {
		ret = ufshcd_send_request_sense(hba, sdp);
		if (ret)
			goto out;
		/* Unit attention condition is cleared now */
		hba->dev_info.is_ufs_dev_wlun_ua_cleared = 1;
	}

	cmd[4] = pwr_mode << 4;

	for (retries = 0; retries < 3; retries++) {
		/*
		 * Current function would be generally called from the power management
		 * callbacks hence set the RQF_PM flag so that it doesn't resume the
		 * already suspended childs.
		 */
		ret = scsi_execute(sdp, cmd, DMA_NONE, NULL, 0, NULL, &sshdr,
				UFS_START_STOP_TIMEOUT, 2, 0, RQF_PM, NULL);
		if (ret) {
			sdev_printk(KERN_WARNING, sdp,
					"START_STOP failed for power mode: %d, result %x, retries : %d\n",
					pwr_mode, ret, retries);
			if (driver_byte(ret) & DRIVER_SENSE)
				scsi_print_sense_hdr(sdp, NULL, &sshdr);
		} else {
			break;
		}
	}

	if (!ret)
		hba->curr_dev_pwr_mode = pwr_mode;
out:
	scsi_device_put(sdp);
	hba->host->eh_noresume = 0;
	return ret;
}

static int ufshcd_link_state_transition(struct ufs_hba *hba,
					enum uic_link_state req_link_state,
					int check_for_bkops)
{
	int ret = 0;

	if (req_link_state == hba->uic_link_state)
		return 0;

	if (req_link_state == UIC_LINK_HIBERN8_STATE) {
		ret = ufshcd_uic_hibern8_enter(hba);
		if (!ret)
			ufshcd_set_link_hibern8(hba);
		else
			goto out;
	}
	/*
	 * If autobkops is enabled, link can't be turned off because
	 * turning off the link would also turn off the device.
	 */
	else if ((req_link_state == UIC_LINK_OFF_STATE) &&
		   (!check_for_bkops || (check_for_bkops &&
		    !hba->auto_bkops_enabled))) {
		/*
		 * Let's make sure that link is in low power mode, we are doing
		 * this currently by putting the link in Hibern8. Otherway to
		 * put the link in low power mode is to send the DME end point
		 * to device and then send the DME reset command to local
		 * unipro. But putting the link in hibern8 is much faster.
		 */
		ret = ufshcd_uic_hibern8_enter(hba);
		if (ret)
			goto out;
		/*
		 * Change controller state to "reset state" which
		 * should also put the link in off/reset state
		 */
		ufshcd_hba_stop(hba, true);
		/*
		 * TODO: Check if we need any delay to make sure that
		 * controller is reset
		 */
		ufshcd_set_link_off(hba);
	}

out:
	return ret;
}

static void ufshcd_vreg_set_lpm(struct ufs_hba *hba)
{
	/*
	 * It seems some UFS devices may keep drawing more than sleep current
	 * (atleast for 500us) from UFS rails (especially from VCCQ rail).
	 * To avoid this situation, add 2ms delay before putting these UFS
	 * rails in LPM mode.
	 */
	if (!ufshcd_is_link_active(hba))
		usleep_range(2000, 2100);

	/*
	 * If UFS device is either in UFS_Sleep turn off VCC rail to save some
	 * power.
	 *
	 * If UFS device and link is in OFF state, all power supplies (VCC,
	 * VCCQ, VCCQ2) can be turned off if power on write protect is not
	 * required. If UFS link is inactive (Hibern8 or OFF state) and device
	 * is in sleep state, put VCCQ & VCCQ2 rails in LPM mode.
	 *
	 * Ignore the error returned by ufshcd_toggle_vreg() as device is anyway
	 * in low power state which would save some power.
	 *
	 * If Write Booster is enabled and the device needs to flush the WB
	 * buffer OR if bkops status is urgent for WB, keep Vcc on.
	 */
	if (ufshcd_is_ufs_dev_poweroff(hba) && ufshcd_is_link_off(hba) &&
	    !hba->dev_info.is_lu_power_on_wp) {
		ufshcd_toggle_vreg(hba->dev, hba->vreg_info.vcc, false);
		if (hba->dev_info.w_spec_version >= 0x300 &&
			hba->vreg_info.vccq &&
			hba->vreg_info.vccq->sys_suspend_pwr_off)
			ufshcd_toggle_vreg(hba->dev,
				hba->vreg_info.vccq, false);
		else
			ufshcd_config_vreg_lpm(hba, hba->vreg_info.vccq);

		if (hba->dev_info.w_spec_version < 0x300 &&
			hba->vreg_info.vccq2->sys_suspend_pwr_off)
			ufshcd_toggle_vreg(hba->dev,
				hba->vreg_info.vccq2, false);
		else
			ufshcd_config_vreg_lpm(hba, hba->vreg_info.vccq2);
	} else if (!ufshcd_is_ufs_dev_active(hba)) {
#ifdef CONFIG_QCOM_WB
		if (!hba->dev_info.keep_vcc_on)
			ufshcd_toggle_vreg(hba->dev, hba->vreg_info.vcc, false);
#endif
#ifdef CONFIG_BLK_TURBO_WRITE
		ufshcd_toggle_vreg(hba->dev, hba->vreg_info.vcc, false);
#endif
		if (!ufshcd_is_link_active(hba)) {
			ufshcd_config_vreg_lpm(hba, hba->vreg_info.vccq);
			ufshcd_config_vreg_lpm(hba, hba->vreg_info.vccq2);
		}
	}
}

static int ufshcd_vreg_set_hpm(struct ufs_hba *hba)
{
	int ret = 0;

	if (ufshcd_is_ufs_dev_poweroff(hba) && ufshcd_is_link_off(hba) &&
		!hba->dev_info.is_lu_power_on_wp) {
		if (hba->dev_info.w_spec_version < 0x300 &&
			hba->vreg_info.vccq2->sys_suspend_pwr_off)
			ret = ufshcd_toggle_vreg(hba->dev,
				hba->vreg_info.vccq2, true);
		else
			ret = ufshcd_config_vreg_hpm(hba, hba->vreg_info.vccq2);
		if (ret)
			goto vcc_disable;

		if (hba->dev_info.w_spec_version >= 0x300 &&
			hba->vreg_info.vccq &&
			hba->vreg_info.vccq->sys_suspend_pwr_off)
			ret = ufshcd_toggle_vreg(hba->dev,
				hba->vreg_info.vccq, true);
		else
			ret = ufshcd_config_vreg_hpm(hba, hba->vreg_info.vccq);
		if (ret)
			goto vccq2_lpm;
		ret = ufshcd_toggle_vreg(hba->dev, hba->vreg_info.vcc, true);
	} else if (!ufshcd_is_ufs_dev_active(hba)) {
		if (!ufshcd_is_link_active(hba)) {
			ret = ufshcd_config_vreg_hpm(hba, hba->vreg_info.vccq2);
			if (ret)
				goto vcc_disable;
			ret = ufshcd_config_vreg_hpm(hba, hba->vreg_info.vccq);
			if (ret)
				goto vccq2_lpm;
		}
		ret = ufshcd_toggle_vreg(hba->dev, hba->vreg_info.vcc, true);
	}
	goto out;

vccq2_lpm:
	ufshcd_config_vreg_lpm(hba, hba->vreg_info.vccq2);
vcc_disable:
	ufshcd_toggle_vreg(hba->dev, hba->vreg_info.vcc, false);
out:
	return ret;
}

static void ufshcd_hba_vreg_set_lpm(struct ufs_hba *hba)
{
	if (ufshcd_is_link_off(hba) ||
	    (ufshcd_is_link_hibern8(hba)
	     && ufshcd_is_power_collapse_during_hibern8_allowed(hba)))
		ufshcd_setup_hba_vreg(hba, false);
}

static void ufshcd_hba_vreg_set_hpm(struct ufs_hba *hba)
{
	int ret;
	struct ufs_vreg_info *info = &hba->vreg_info;

	if (ufshcd_is_link_off(hba) ||
	    (ufshcd_is_link_hibern8(hba)
	     && ufshcd_is_power_collapse_during_hibern8_allowed(hba))) {
		ret = ufshcd_setup_hba_vreg(hba, true);
		if (ret && (info->vdd_hba->enabled == false)) {
			dev_err(hba->dev, "vdd_hba is not enabled\n");
			BUG_ON(1);
		}
	}
}

/**
 * ufshcd_suspend - helper function for suspend operations
 * @hba: per adapter instance
 * @pm_op: desired low power operation type
 *
 * This function will try to put the UFS device and link into low power
 * mode based on the "rpm_lvl" (Runtime PM level) or "spm_lvl"
 * (System PM level).
 *
 * If this function is called during shutdown, it will make sure that
 * both UFS device and UFS link is powered off.
 *
 * NOTE: UFS device & link must be active before we enter in this function.
 *
 * Returns 0 for success and non-zero for failure
 */
static int ufshcd_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	int ret = 0;
	enum ufs_pm_level pm_lvl;
	enum ufs_dev_pwr_mode req_dev_pwr_mode;
	enum uic_link_state req_link_state;

#ifdef CONFIG_BLK_TURBO_WRITE
	if (!mutex_trylock(&hba->tw_ctrl_mutex)) {
		dev_err(hba->dev, "%s has failed %d.\n", __func__, -EBUSY);
		return -EBUSY;
	}
#endif
	hba->pm_op_in_progress = 1;

	if (!ufshcd_is_shutdown_pm(pm_op)) {
		pm_lvl = ufshcd_is_runtime_pm(pm_op) ?
			 hba->rpm_lvl : hba->spm_lvl;
		req_dev_pwr_mode = ufs_get_pm_lvl_to_dev_pwr_mode(pm_lvl);
		req_link_state = ufs_get_pm_lvl_to_link_pwr_state(pm_lvl);
	} else {
		req_dev_pwr_mode = UFS_POWERDOWN_PWR_MODE;
		req_link_state = UIC_LINK_OFF_STATE;
	}

	ret = ufshcd_crypto_suspend(hba, pm_op);
	if (ret)
		goto out;

#if defined(CONFIG_UFSFEATURE)
	ufsf_hpb_suspend(&hba->ufsf);
#endif

	/*
	 * If we can't transition into any of the low power modes
	 * just gate the clocks.
	 */
	WARN_ON(hba->hibern8_on_idle.is_enabled &&
		hba->hibern8_on_idle.active_reqs);
	ufshcd_hold_all(hba);
	hba->clk_gating.is_suspended = true;
	hba->hibern8_on_idle.is_suspended = true;

	if (hba->clk_scaling.is_allowed) {
		cancel_work_sync(&hba->clk_scaling.suspend_work);
		cancel_work_sync(&hba->clk_scaling.resume_work);
		ufshcd_suspend_clkscaling(hba);
	}

	if (req_dev_pwr_mode == UFS_ACTIVE_PWR_MODE &&
			req_link_state == UIC_LINK_ACTIVE_STATE) {
		goto disable_clks;
	}

	if ((req_dev_pwr_mode == hba->curr_dev_pwr_mode) &&
	    (req_link_state == hba->uic_link_state))
		goto enable_gating;

	/* UFS device & link must be active before we enter in this function */
	if (!ufshcd_is_ufs_dev_active(hba) || !ufshcd_is_link_active(hba))
		goto disable_clks;

	if (ufshcd_is_runtime_pm(pm_op)) {
		if (ufshcd_can_autobkops_during_suspend(hba)) {
			/*
			 * The device is idle with no requests in the queue,
			 * allow background operations if bkops status shows
			 * that performance might be impacted.
			 */
			ret = ufshcd_urgent_bkops(hba);
			if (ret)
				goto enable_gating;
		} else {
			/* make sure that auto bkops is disabled */
			ufshcd_disable_auto_bkops(hba);
		}
#ifdef CONFIG_QCOM_WB
		ufshcd_wb_toggle_flush(hba);
	} else if (!ufshcd_is_runtime_pm(pm_op)) {
		ufshcd_wb_buf_flush_disable(hba);
		hba->dev_info.keep_vcc_on = false;
#endif
	}

#ifdef CONFIG_BLK_TURBO_WRITE
#ifdef CONFIG_SCSI_UFS_SUPPORT_TW_MAN_GC
	if (!ufshcd_is_shutdown_pm(pm_op) && hba->support_tw && ufshcd_is_system_pm(pm_op))
		ufshcd_tw_flush_ctrl(hba);
#endif

	if (ufshcd_is_system_pm(pm_op))
		ufshcd_reset_tw(hba, true);
#endif

#ifdef CONFIG_QCOM_WB
	if ((req_dev_pwr_mode != hba->curr_dev_pwr_mode) &&
	    ((ufshcd_is_runtime_pm(pm_op) && (!hba->auto_bkops_enabled)
	      && !hba->wb_buf_flush_enabled) ||
	     !ufshcd_is_runtime_pm(pm_op))) {
#else
	if ((req_dev_pwr_mode != hba->curr_dev_pwr_mode) &&
	     ((ufshcd_is_runtime_pm(pm_op) && !hba->auto_bkops_enabled) ||
	       !ufshcd_is_runtime_pm(pm_op))) {
#endif
		/* ensure that bkops is disabled */
		ufshcd_disable_auto_bkops(hba);
		ret = ufshcd_set_dev_pwr_mode(hba, req_dev_pwr_mode);
		if (ret)
			goto enable_gating;
	}

	flush_work(&hba->eeh_work);
	ret = ufshcd_link_state_transition(hba, req_link_state, 1);
	if (ret)
		goto set_dev_active;

	if (ufshcd_is_link_hibern8(hba) &&
	    ufshcd_is_hibern8_on_idle_allowed(hba))
		hba->hibern8_on_idle.state = HIBERN8_ENTERED;

disable_clks:
	/*
	 * Flush pending works before clock is disabled
	 */
	if (cancel_work_sync(&hba->eeh_work))
		pm_runtime_put_noidle(hba->dev);

	/*
	 * Call vendor specific suspend callback. As these callbacks may access
	 * vendor specific host controller register space call them before the
	 * host clocks are ON.
	 */
	ret = ufshcd_vops_suspend(hba, pm_op);
	if (ret)
		goto set_link_active;

	/*
	 * Disable the host irq as host controller as there won't be any
	 * host controller transaction expected till resume.
	 */
	ufshcd_disable_irq(hba);

	/* reset the connected UFS device when shutdown */
	if (ufshcd_is_shutdown_pm(pm_op)) {
		ret = ufshcd_assert_device_reset(hba);
		if (ret)
			goto set_link_active;
	}

	if (!ufshcd_is_link_active(hba))
		ret = ufshcd_disable_clocks(hba, false);
	else
		/*
		 * If link is active, device ref_clk and unipro clock can't be
		 * switched off.
		 */
		ret = ufshcd_disable_clocks_keep_link_active(hba, false);
	if (ret)
		goto set_link_active;

	if (ufshcd_is_clkgating_allowed(hba)) {
		hba->clk_gating.state = CLKS_OFF;
		trace_ufshcd_clk_gating(dev_name(hba->dev),
					hba->clk_gating.state);
	}

	/* Put the host controller in low power mode if possible */
	ufshcd_hba_vreg_set_lpm(hba);

	if (!hba->auto_bkops_enabled)
		ufshcd_vreg_set_lpm(hba);

	goto out;

set_link_active:
	ufshcd_enable_irq(hba);
	if (hba->clk_scaling.is_allowed)
		ufshcd_resume_clkscaling(hba);
	ufshcd_vreg_set_hpm(hba);
	if (ufshcd_is_link_hibern8(hba) && !ufshcd_uic_hibern8_exit(hba)) {
		ufshcd_set_link_active(hba);
	} else if (ufshcd_is_link_off(hba)) {
		ufshcd_update_error_stats(hba, UFS_ERR_VOPS_SUSPEND);
		ufshcd_host_reset_and_restore(hba);
	}
set_dev_active:
	if (!ufshcd_set_dev_pwr_mode(hba, UFS_ACTIVE_PWR_MODE))
		ufshcd_disable_auto_bkops(hba);
enable_gating:
	if (hba->clk_scaling.is_allowed)
		ufshcd_resume_clkscaling(hba);
	hba->hibern8_on_idle.is_suspended = false;
	hba->clk_gating.is_suspended = false;
	ufshcd_release_all(hba);
	ufshcd_crypto_resume(hba, pm_op);

#if defined(CONFIG_UFSFEATURE)
	ufsf_hpb_resume(&hba->ufsf);
#endif
out:
#ifdef CONFIG_BLK_TURBO_WRITE
	if (!ret && ufshcd_is_system_pm(pm_op))
		hba->tw_state_not_allowed = true;
#endif

	hba->pm_op_in_progress = 0;

#ifdef CONFIG_BLK_TURBO_WRITE
	mutex_unlock(&hba->tw_ctrl_mutex);
#endif

	if (ret)
		ufshcd_update_error_stats(hba, UFS_ERR_SUSPEND);

	return ret;
}

/**
 * ufshcd_resume - helper function for resume operations
 * @hba: per adapter instance
 * @pm_op: runtime PM or system PM
 *
 * This function basically brings the UFS device, UniPro link and controller
 * to active state.
 *
 * Returns 0 for success and non-zero for failure
 */
static int ufshcd_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	int ret;
	enum uic_link_state old_link_state;
	enum ufs_dev_pwr_mode old_pwr_mode;

	hba->pm_op_in_progress = 1;
	old_link_state = hba->uic_link_state;
	old_pwr_mode = hba->curr_dev_pwr_mode;

	ufshcd_hba_vreg_set_hpm(hba);
	/* Make sure clocks are enabled before accessing controller */
	ret = ufshcd_enable_clocks(hba);
	if (ret)
		goto out;

	/* enable the host irq as host controller would be active soon */
	ufshcd_enable_irq(hba);

	ret = ufshcd_vreg_set_hpm(hba);
	if (ret)
		goto disable_irq_and_vops_clks;

	/*
	 * Call vendor specific resume callback. As these callbacks may access
	 * vendor specific host controller register space call them when the
	 * host clocks are ON.
	 */
	ret = ufshcd_vops_resume(hba, pm_op);
	if (ret)
		goto disable_vreg;

	if (ufshcd_is_link_hibern8(hba)) {
		ret = ufshcd_uic_hibern8_exit(hba);
		if (!ret) {
			ufshcd_set_link_active(hba);
			if (ufshcd_is_hibern8_on_idle_allowed(hba))
				hba->hibern8_on_idle.state = HIBERN8_EXITED;
		} else {
			goto vendor_suspend;
		}
	} else if (ufshcd_is_link_off(hba)) {
		/*
		 * A full initialization of the host and the device is required
		 * since the link was put to off during suspend.
		 */
		ret = ufshcd_reset_and_restore(hba);
		/*
		 * ufshcd_reset_and_restore() should have already
		 * set the link state as active
		 */
		if (ret || !ufshcd_is_link_active(hba))
			goto vendor_suspend;
		/* mark link state as hibern8 exited */
		if (ufshcd_is_hibern8_on_idle_allowed(hba))
			hba->hibern8_on_idle.state = HIBERN8_EXITED;
	}

#ifdef CONFIG_QCOM_WB
	ufshcd_wb_buf_flush_disable(hba);
#endif
	if (!ufshcd_is_ufs_dev_active(hba)) {
		ret = ufshcd_set_dev_pwr_mode(hba, UFS_ACTIVE_PWR_MODE);
		if (ret) {
			/*
			 * In the case of SSU timeout, err_handler must have
			 * recovered the uic link and dev state to active so
			 * we can proceed after checking the link and
			 * dev state.
			 */
			if (ufshcd_is_ufs_dev_active(hba) &&
			    ufshcd_is_link_active(hba))
				ret = 0;

			else if ((work_pending(&hba->eh_work)) ||
				ufshcd_eh_in_progress(hba)) {
				flush_work(&hba->eh_work);
				ret = 0;
				dev_info(hba->dev, "dev pwr mode=%d, UIC link state=%d\n",
					hba->curr_dev_pwr_mode,
					hba->uic_link_state);
				}
			if (ret)
				goto set_old_link_state;
		}
	}

	ret = ufshcd_crypto_resume(hba, pm_op);
	if (ret)
		goto set_old_dev_pwr_mode;

	if (ufshcd_keep_autobkops_enabled_except_suspend(hba))
		ufshcd_enable_auto_bkops(hba);
	else
		/*
		 * If BKOPs operations are urgently needed at this moment then
		 * keep auto-bkops enabled or else disable it.
		 */
		ufshcd_urgent_bkops(hba);

	hba->clk_gating.is_suspended = false;
	hba->hibern8_on_idle.is_suspended = false;

	if (hba->clk_scaling.is_allowed)
		ufshcd_resume_clkscaling(hba);

#if defined(CONFIG_UFSFEATURE)
	ufsf_hpb_resume(&hba->ufsf);
#endif
	/* Set Auto-Hibernate timer if supported */
	ufshcd_set_auto_hibern8_timer(hba);

	/* Schedule clock gating in case of no access to UFS device yet */
	ufshcd_release_all(hba);
	goto out;

set_old_dev_pwr_mode:
	if (old_pwr_mode != hba->curr_dev_pwr_mode)
		ufshcd_set_dev_pwr_mode(hba, old_pwr_mode);
set_old_link_state:
	ufshcd_link_state_transition(hba, old_link_state, 0);
	if (ufshcd_is_link_hibern8(hba) &&
	    ufshcd_is_hibern8_on_idle_allowed(hba))
		hba->hibern8_on_idle.state = HIBERN8_ENTERED;
vendor_suspend:
	ufshcd_vops_suspend(hba, pm_op);
disable_vreg:
	ufshcd_vreg_set_lpm(hba);
disable_irq_and_vops_clks:
	ufshcd_disable_irq(hba);
	if (hba->clk_scaling.is_allowed)
		ufshcd_suspend_clkscaling(hba);
	ufshcd_disable_clocks(hba, false);
	if (ufshcd_is_clkgating_allowed(hba))
		hba->clk_gating.state = CLKS_OFF;
out:
	hba->pm_op_in_progress = 0;

#ifdef CONFIG_BLK_TURBO_WRITE
	if (hba->tw_state_not_allowed)
		hba->tw_state_not_allowed = false;
#endif

	if (ret)
		ufshcd_update_error_stats(hba, UFS_ERR_RESUME);

	return ret;
}

/**
 * ufshcd_system_suspend - system suspend routine
 * @hba: per adapter instance
 *
 * Check the description of ufshcd_suspend() function for more details.
 *
 * Returns 0 for success and non-zero for failure
 */
int ufshcd_system_suspend(struct ufs_hba *hba)
{
	int ret = 0;
	ktime_t start = ktime_get();

	if (!hba || !hba->is_powered)
		return 0;

	if ((ufs_get_pm_lvl_to_dev_pwr_mode(hba->spm_lvl) ==
	     hba->curr_dev_pwr_mode) &&
	    (ufs_get_pm_lvl_to_link_pwr_state(hba->spm_lvl) ==
	     hba->uic_link_state))
		goto out;

	if (pm_runtime_suspended(hba->dev)) {
		/*
		 * UFS device and/or UFS link low power states during runtime
		 * suspend seems to be different than what is expected during
		 * system suspend. Hence runtime resume the devic & link and
		 * let the system suspend low power states to take effect.
		 * TODO: If resume takes longer time, we might have optimize
		 * it in future by not resuming everything if possible.
		 */
		ret = ufshcd_runtime_resume(hba);
		if (ret)
			goto out;
	}

	ret = ufshcd_suspend(hba, UFS_SYSTEM_PM);
out:
	trace_ufshcd_system_suspend(dev_name(hba->dev), ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	if (!ret)
		hba->is_sys_suspended = true;
	return ret;
}
EXPORT_SYMBOL(ufshcd_system_suspend);

/**
 * ufshcd_system_resume - system resume routine
 * @hba: per adapter instance
 *
 * Returns 0 for success and non-zero for failure
 */

int ufshcd_system_resume(struct ufs_hba *hba)
{
	int ret = 0;
	ktime_t start = ktime_get();

	if (!hba)
		return -EINVAL;

	if (!hba->is_powered || pm_runtime_suspended(hba->dev))
		/*
		 * Let the runtime resume take care of resuming
		 * if runtime suspended.
		 */
		goto out;
	else
		ret = ufshcd_resume(hba, UFS_SYSTEM_PM);

out:
	trace_ufshcd_system_resume(dev_name(hba->dev), ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	if (!ret)
		hba->is_sys_suspended = false;
	return ret;
}
EXPORT_SYMBOL(ufshcd_system_resume);

/**
 * ufshcd_runtime_suspend - runtime suspend routine
 * @hba: per adapter instance
 *
 * Check the description of ufshcd_suspend() function for more details.
 *
 * Returns 0 for success and non-zero for failure
 */
int ufshcd_runtime_suspend(struct ufs_hba *hba)
{
	int ret = 0;
	ktime_t start = ktime_get();

	if (!hba)
		return -EINVAL;

	if (!hba->is_powered)
		goto out;
	else
		ret = ufshcd_suspend(hba, UFS_RUNTIME_PM);
out:
	trace_ufshcd_runtime_suspend(dev_name(hba->dev), ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	return ret;
}
EXPORT_SYMBOL(ufshcd_runtime_suspend);

/**
 * ufshcd_runtime_resume - runtime resume routine
 * @hba: per adapter instance
 *
 * This function basically brings the UFS device, UniPro link and controller
 * to active state. Following operations are done in this function:
 *
 * 1. Turn on all the controller related clocks
 * 2. Bring the UniPro link out of Hibernate state
 * 3. If UFS device is in sleep state, turn ON VCC rail and bring the UFS device
 *    to active state.
 * 4. If auto-bkops is enabled on the device, disable it.
 *
 * So following would be the possible power state after this function return
 * successfully:
 *	S1: UFS device in Active state with VCC rail ON
 *	    UniPro link in Active state
 *	    All the UFS/UniPro controller clocks are ON
 *
 * Returns 0 for success and non-zero for failure
 */
int ufshcd_runtime_resume(struct ufs_hba *hba)
{
	int ret = 0;
	ktime_t start = ktime_get();

	if (!hba)
		return -EINVAL;

	if (!hba->is_powered)
		goto out;
	else
		ret = ufshcd_resume(hba, UFS_RUNTIME_PM);
out:
	trace_ufshcd_runtime_resume(dev_name(hba->dev), ret,
		ktime_to_us(ktime_sub(ktime_get(), start)),
		hba->curr_dev_pwr_mode, hba->uic_link_state);
	return ret;
}
EXPORT_SYMBOL(ufshcd_runtime_resume);

int ufshcd_runtime_idle(struct ufs_hba *hba)
{
	return 0;
}
EXPORT_SYMBOL(ufshcd_runtime_idle);

#if defined(CONFIG_SCSI_UFS_QCOM)
#include "ufs-qcom.h"

static ssize_t ufshcd_hw_reset_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = container_of(dev, struct Scsi_Host, shost_dev);
	struct ufs_hba *hba = shost_priv(shost);
	struct ufs_qcom_host *host = ufshcd_get_variant(hba);
	int curr_len;
	int i = 0;

	curr_len = snprintf(buf, PAGE_SIZE, "rst_cnt:%d.\n", host->hw_reset_count);
	if (host->hw_reset_count == 0)
		return curr_len;

	curr_len += snprintf(buf + curr_len, PAGE_SIZE, "last_time:%lu.\n", host->last_hw_reset);
	curr_len += snprintf(buf + curr_len, PAGE_SIZE, "saved:0x%x, saved_uic:0x%x.\n",
			host->hw_reset_saved_err, host->hw_reset_saved_uic_err);
	curr_len += snprintf(buf + curr_len, PAGE_SIZE, "outstanding task:0x%lx, req:0x%lx.\n",
			host->hw_reset_outstanding_tasks, host->hw_reset_outstanding_reqs);

#ifdef CONFIG_DEBUG_FS
	curr_len += snprintf(buf + curr_len, PAGE_SIZE, "ufs_stats\n err_stats :");
	for (i = 0; i < UFS_ERR_MAX; i++)
		curr_len += snprintf(buf + curr_len, PAGE_SIZE, " %02d:%d", i,
				host->hw_reset_ufs_stats.err_stats[i]);
	curr_len += snprintf(buf + curr_len, PAGE_SIZE, "\n");
#endif

	curr_len += snprintf(buf + curr_len, PAGE_SIZE, "ufs_stats\n pa_stats :\n");
	for (i = 0; i < UIC_ERR_REG_HIST_LENGTH; i++) {
		int p = (i + host->hw_reset_ufs_stats.pa_err.pos - 1) % UIC_ERR_REG_HIST_LENGTH;

		if (host->hw_reset_ufs_stats.pa_err.reg[p] == 0)
			continue;
		curr_len += snprintf(buf + curr_len, PAGE_SIZE, "%02d:0x%x at %lld us\n", i,
			host->hw_reset_ufs_stats.pa_err.reg[p],
			ktime_to_us(host->hw_reset_ufs_stats.pa_err.tstamp[p]));
	}

	curr_len += snprintf(buf + curr_len, PAGE_SIZE, " dl_stats :\n");
	for (i = 0; i < UIC_ERR_REG_HIST_LENGTH; i++) {
		int p = (i + host->hw_reset_ufs_stats.dl_err.pos - 1) % UIC_ERR_REG_HIST_LENGTH;

		if (host->hw_reset_ufs_stats.dl_err.reg[p] == 0)
			continue;
		curr_len += snprintf(buf + curr_len, PAGE_SIZE, "%02d:0x%x at %lld us\n", i,
			host->hw_reset_ufs_stats.dl_err.reg[p],
			ktime_to_us(host->hw_reset_ufs_stats.dl_err.tstamp[p]));
	}

	curr_len += snprintf(buf + curr_len, PAGE_SIZE, " nl_stats :\n");
	for (i = 0; i < UIC_ERR_REG_HIST_LENGTH; i++) {
		int p = (i + host->hw_reset_ufs_stats.nl_err.pos - 1) % UIC_ERR_REG_HIST_LENGTH;

		if (host->hw_reset_ufs_stats.nl_err.reg[p] == 0)
			continue;
		curr_len += snprintf(buf + curr_len, PAGE_SIZE, "%02d:0x%x at %lld us\n", i,
			host->hw_reset_ufs_stats.nl_err.reg[p],
			ktime_to_us(host->hw_reset_ufs_stats.nl_err.tstamp[p]));
	}

	curr_len += snprintf(buf + curr_len, PAGE_SIZE, " tl_stats :\n");
	for (i = 0; i < UIC_ERR_REG_HIST_LENGTH; i++) {
		int p = (i + host->hw_reset_ufs_stats.tl_err.pos - 1) % UIC_ERR_REG_HIST_LENGTH;

		if (host->hw_reset_ufs_stats.tl_err.reg[p] == 0)
			continue;
		curr_len += snprintf(buf + curr_len, PAGE_SIZE, "%02d:0x%x at %lld us\n", i,
			host->hw_reset_ufs_stats.tl_err.reg[p],
			ktime_to_us(host->hw_reset_ufs_stats.tl_err.tstamp[p]));
	}

	curr_len += snprintf(buf + curr_len, PAGE_SIZE, " dme_stats :\n");
	for (i = 0; i < UIC_ERR_REG_HIST_LENGTH; i++) {
		int p = (i + host->hw_reset_ufs_stats.dme_err.pos - 1) % UIC_ERR_REG_HIST_LENGTH;

		if (host->hw_reset_ufs_stats.dme_err.reg[p] == 0)
			continue;
		curr_len += snprintf(buf + curr_len, PAGE_SIZE, "%02d:0x%x at %lld us\n", i,
			host->hw_reset_ufs_stats.dme_err.reg[p],
			ktime_to_us(host->hw_reset_ufs_stats.dme_err.tstamp[p]));
	}

	return curr_len;
}

static void ufshcd_add_hw_reset_info_sysfs_nodes(struct ufs_hba *hba)
{
	struct device *dev = &(hba->host->shost_dev);

	hba->hw_reset_info_attr.show = ufshcd_hw_reset_info_show;
	hba->hw_reset_info_attr.store = NULL;
	sysfs_attr_init(&hba->hw_reset_info_attr.attr);
	hba->hw_reset_info_attr.attr.name = "hw_rst_info";
	hba->hw_reset_info_attr.attr.mode = 0444;
	if (device_create_file(dev, &hba->hw_reset_info_attr))
		dev_err(hba->dev, "Failed to create sysfs for hw_reset_info\n");
}
#endif

#if defined(SEC_UFS_ERROR_COUNT)
#define SEC_UFS_DATA_ATTR(name, fmt, args...)								\
static ssize_t SEC_UFS_##name##_show(struct device *dev, struct device_attribute *attr, char *buf)	\
{													\
	struct Scsi_Host *Shost = container_of(dev, struct Scsi_Host, shost_dev);			\
	struct ufs_hba *hba = shost_priv(Shost);							\
	struct SEC_UFS_counting *err_info = &(hba->SEC_err_info);					\
	return sprintf(buf, fmt, args);									\
}													\
static DEVICE_ATTR(name, 0444, SEC_UFS_##name##_show, NULL)

SEC_UFS_DATA_ATTR(SEC_UFS_op_cnt, "\"HWRESET\":\"%u\",\"LINKFAIL\":\"%u\",\"H8ENTERFAIL\":\"%u\""
		",\"H8EXITFAIL\":\"%u\"\n",
		err_info->op_count.HW_RESET_count, err_info->op_count.link_startup_count,
		err_info->op_count.Hibern8_enter_count, err_info->op_count.Hibern8_exit_count);

SEC_UFS_DATA_ATTR(SEC_UFS_uic_cmd_cnt, "\"TESTMODE\":\"%d\",\"DME_GET\":\"%d\",\"DME_SET\":\"%d\""
		",\"DME_PGET\":\"%d\",\"DME_PSET\":\"%d\",\"PWRON\":\"%d\",\"PWROFF\":\"%d\""
		",\"DME_EN\":\"%d\",\"DME_RST\":\"%d\",\"EPRST\":\"%d\",\"LINKSTARTUP\":\"%d\""
		",\"H8ENTER\":\"%d\",\"H8EXIT\":\"%d\"\n",
		err_info->UIC_cmd_count.DME_TEST_MODE_err,	// TEST_MODE
		err_info->UIC_cmd_count.DME_GET_err,		// DME_GET
		err_info->UIC_cmd_count.DME_SET_err,		// DME_SET
		err_info->UIC_cmd_count.DME_PEER_GET_err,	// DME_PEER_GET
		err_info->UIC_cmd_count.DME_PEER_SET_err,	// DME_PEER_SET
		err_info->UIC_cmd_count.DME_POWERON_err,	// DME_POWERON
		err_info->UIC_cmd_count.DME_POWEROFF_err,	// DME_POWEROFF
		err_info->UIC_cmd_count.DME_ENABLE_err,		// DME_ENABLE
		err_info->UIC_cmd_count.DME_RESET_err,		// DME_RESET
		err_info->UIC_cmd_count.DME_END_PT_RST_err,	// DME_END_PT_RST
		err_info->UIC_cmd_count.DME_LINK_STARTUP_err,	// DME_LINK_STARTUP
		err_info->UIC_cmd_count.DME_HIBER_ENTER_err,	// DME_HIBERN8_ENTER
		err_info->UIC_cmd_count.DME_HIBER_EXIT_err);	// DME_HIBERN8_EXIT

SEC_UFS_DATA_ATTR(SEC_UFS_uic_err_cnt, "\"PAERR\":\"%d\",\"DLPAINITERROR\":\"%d\",\"DLNAC\":\"%d\""
		",\"DLTCREPLAY\":\"%d\",\"NLERR\":\"%d\",\"TLERR\":\"%d\",\"DMEERR\":\"%d\"\n",
		err_info->UIC_err_count.PA_ERR_cnt,			// PA_ERR
		err_info->UIC_err_count.DL_PA_INIT_ERROR_cnt,		// DL_PA_INIT_ERROR
		err_info->UIC_err_count.DL_NAC_RECEIVED_ERROR_cnt,	// DL_NAC_RECEIVED
		err_info->UIC_err_count.DL_TC_REPLAY_ERROR_cnt,		// DL_TCx_REPLAY_ERROR
		err_info->UIC_err_count.NL_ERROR_cnt,			// NL_ERROR
		err_info->UIC_err_count.TL_ERROR_cnt,			// TL_ERROR
		err_info->UIC_err_count.DME_ERROR_cnt);			// DME_ERROR

SEC_UFS_DATA_ATTR(SEC_UFS_fatal_cnt, "\"DFE\":\"%d\",\"CFE\":\"%d\",\"SBFE\":\"%d\""
		",\"CEFE\":\"%d\",\"LLE\":\"%d\"\n",
		err_info->Fatal_err_count.DFE,		// Device_Fatal
		err_info->Fatal_err_count.CFE,		// Controller_Fatal
		err_info->Fatal_err_count.SBFE,		// System_Bus_Fatal
		err_info->Fatal_err_count.CEFE,		// Crypto_Engine_Fatal
		err_info->Fatal_err_count.LLE);		// Link_Lost

SEC_UFS_DATA_ATTR(SEC_UFS_utp_cnt, "\"UTMRQTASK\":\"%d\",\"UTMRATASK\":\"%d\",\"UTRR\":\"%d\""
		",\"UTRW\":\"%d\",\"UTRSYNCCACHE\":\"%d\",\"UTRUNMAP\":\"%d\",\"UTRETC\":\"%d\"\n",
		err_info->UTP_count.UTMR_query_task_count,	// QUERY_TASK
		err_info->UTP_count.UTMR_abort_task_count,	// ABORT_TASK
		err_info->UTP_count.UTR_read_err,		// READ_10
		err_info->UTP_count.UTR_write_err,		// WRITE_10
		err_info->UTP_count.UTR_sync_cache_err,		// SYNC_CACHE
		err_info->UTP_count.UTR_unmap_err,		// UNMAP
		err_info->UTP_count.UTR_etc_err);		// etc

SEC_UFS_DATA_ATTR(SEC_UFS_query_cnt, "\"NOPERR\":\"%d\",\"R_DESC\":\"%d\",\"W_DESC\":\"%d\""
		",\"R_ATTR\":\"%d\",\"W_ATTR\":\"%d\",\"R_FLAG\":\"%d\",\"S_FLAG\":\"%d\""
		",\"C_FLAG\":\"%d\",\"T_FLAG\":\"%d\"\n",
		err_info->query_count.NOP_err,
		err_info->query_count.R_Desc_err,	// R_Desc
		err_info->query_count.W_Desc_err,	// W_Desc
		err_info->query_count.R_Attr_err,	// R_Attr
		err_info->query_count.W_Attr_err,	// W_Attr
		err_info->query_count.R_Flag_err,	// R_Flag
		err_info->query_count.Set_Flag_err,	// Set_Flag
		err_info->query_count.Clear_Flag_err,	// Clear_Flag
		err_info->query_count.Toggle_Flag_err);	// Toggle_Flag

SEC_UFS_DATA_ATTR(SEC_UFS_err_sum, "\"OPERR\":\"%d\",\"UICCMD\":\"%d\",\"UICERR\":\"%d\""
		",\"FATALERR\":\"%d\",\"UTPERR\":\"%d\",\"QUERYERR\":\"%d\"\n",
		err_info->op_count.op_err,
		err_info->UIC_cmd_count.UIC_cmd_err,
		err_info->UIC_err_count.UIC_err,
		err_info->Fatal_err_count.Fatal_err,
		err_info->UTP_count.UTP_err,
		err_info->query_count.Query_err);
#endif

#ifdef CONFIG_BLK_TURBO_WRITE
static ssize_t SEC_UFS_TW_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *Shost = container_of(dev, struct Scsi_Host, shost_dev);
	struct ufs_hba *hba = shost_priv(Shost);
	struct SEC_UFS_TW_info tw_info;
	struct SEC_UFS_TW_info *tw_info_old = &(hba->SEC_tw_info_old);
	struct SEC_UFS_TW_info *tw_info_new = &(hba->SEC_tw_info);
	long hours = 0;

	get_monotonic_boottime(&(tw_info_new->timestamp));
	hours = (tw_info_new->timestamp.tv_sec - tw_info_old->timestamp.tv_sec) / 60;	/* min */
	hours = (hours + 30) / 60;	/* round up to hours */

	SEC_UFS_TW_info_get_diff(&tw_info, tw_info_new, tw_info_old);

	return sprintf(buf, "\"TWCTRLCNT\":\"%llu\","
			"\"TWCTRLERRCNT\":\"%llu\","
			"\"TWDAILYMB\":\"%llu\","
			"\"TWTOTALMB\":\"%llu\","
			"\"H8CNT\":\"%llu\",\"H8MS\":\"%llu\","
			"\"H8CNT100MS\":\"%llu\",\"H8MS100MS\":\"%llu\","
			"\"H8MAXMS\":\"%llu\","
			"\"TWhours\":\"%ld\"\n",
			(tw_info.tw_enable_count + tw_info.tw_disable_count),
			tw_info_new->tw_setflag_error_count,    /* total error count */
			(tw_info.tw_amount_W_kb >> 10),         /* TW write daily : MB */
			(tw_info_new->tw_amount_W_kb >> 10),    /* TW write total : MB */
			tw_info.hibern8_enter_count, tw_info.hibern8_amount_ms,
			tw_info.hibern8_enter_count_100ms,
			tw_info.hibern8_amount_ms_100ms,
			tw_info.hibern8_max_ms,
			hours);
}
static DEVICE_ATTR(SEC_UFS_TW_info, 0444, SEC_UFS_TW_info_show, NULL);
#endif

#if defined(CONFIG_UFSFEATURE) && defined(CONFIG_UFSHPB)
static ssize_t SEC_UFS_HPB_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *Shost = container_of(dev, struct Scsi_Host, shost_dev);
	struct ufs_hba *hba = shost_priv(Shost);
	struct ufsf_feature *ufsf = &hba->ufsf;
	struct ufshpb_lu *hpb = ufsf->ufshpb_lup[0];
	struct SEC_UFS_HPB_info *hpb_info = &(hba->SEC_hpb_info);

	long long hit_cnt;
	long long miss_cnt;
	long long set_rt_cnt, unset_rt_cnt;

	int rt_pin_cnt = 0, act_cnt = 0;
	int rgn_idx;
	enum HPBREGION_STATE state;

	long long pinned_rb_cnt, active_rb_cnt;
	long long hpb_amount_R_kb_diff;
	long hours = 0;

	if (!hpb || !(ufsf->hpb_dev_info.hpb_device))
		return 0;

	if ((ufsf->ufshpb_state == HPB_FAILED) || (ufsf->ufshpb_state == HPB_NEED_INIT))
		return 0;

	hit_cnt = atomic64_read(&hpb->hit);
	miss_cnt = atomic64_read(&hpb->miss);
	set_rt_cnt = atomic64_read(&hpb->set_rt_req_cnt);
	unset_rt_cnt = atomic64_read(&hpb->unset_rt_req_cnt);

	for (rgn_idx = 0; rgn_idx < hpb->rgns_per_lu; rgn_idx++) {
		state = hpb->rgn_tbl[rgn_idx].rgn_state;
		if (state == HPBREGION_RT_PINNED)
			rt_pin_cnt++;
		else if (state == HPBREGION_ACTIVE)
			act_cnt++;
	}

	pinned_rb_cnt = atomic64_read(&(hpb_info->hpb_pinned_rb_cnt));
	active_rb_cnt = atomic64_read(&(hpb_info->hpb_active_rb_cnt));

	hpb_amount_R_kb_diff = hpb_info->hpb_amount_R_kb - hpb_info->hpb_amount_R_kb_old;
	hpb_info->hpb_amount_R_kb_old = hpb_info->hpb_amount_R_kb;

	get_monotonic_boottime(&(hpb_info->timestamp_new));
	hours = (hpb_info->timestamp_new.tv_sec - hpb_info->timestamp_old.tv_sec) / 60;	/* min */
	hours = (hours + 30) / 60;	/* round up to hours */
	get_monotonic_boottime(&(hpb_info->timestamp_old));	/* update timestamp */

	return sprintf(buf, "\"HCNT\":\"%llu\","
			"\"MCNT\":\"%llu\","
			"\"SRTCNT\":\"%llu\","
			"\"USRTCNT\":\"%llu\","
			"\"RTPCNT\":\"%d\","
			"\"ACTCNT\":\"%d\","
			"\"PINRBCNT\":\"%llu\","
			"\"ACTRBCNT\":\"%llu\","
			"\"HPBDAYMB\":\"%llu\","
			"\"HPBhours\":\"%ld\"\n",
			hit_cnt,
			miss_cnt,
			set_rt_cnt,
			unset_rt_cnt,
			rt_pin_cnt,
			act_cnt,
			pinned_rb_cnt,
			active_rb_cnt,
			(hpb_amount_R_kb_diff >> 10),
			hours);
}
static DEVICE_ATTR(SEC_UFS_HPB_info, 0444, SEC_UFS_HPB_info_show, NULL);

static ssize_t SEC_UFS_HPB_error_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *Shost = container_of(dev, struct Scsi_Host, shost_dev);
	struct ufs_hba *hba = shost_priv(Shost);
	struct SEC_UFS_HPB_info *hpb_info = &(hba->SEC_hpb_info);

	if (!(hba->ufsf.hpb_dev_info.hpb_device))
		return 0;

	return sprintf(buf, "\"HPBRERR\":\"%u\","
			"\"RBRERR\":\"%u\","
			"\"RBSRTERR\":\"%u\","
			"\"WBPFERR\":\"%u\","
			"\"WBUSRTERR\":\"%u\","
			"\"WBUSRTAERR\":\"%u\"\n",
			hpb_info->hpb_read_err_count,
			hpb_info->hpb_RB_ID_READ_err_count,
			hpb_info->hpb_RB_ID_SET_RT_err_count,
			hpb_info->hpb_WB_ID_PREFETCH_err_count,
			hpb_info->hpb_WB_ID_UNSET_RT_err_count,
			hpb_info->hpb_WB_ID_UNSET_RT_ALL_err_count);
}
static DEVICE_ATTR(SEC_UFS_HPB_err_info, 0444, SEC_UFS_HPB_error_show, NULL);
#endif

UFS_DEV_ATTR(capabilities, "%08x\n", hba->capabilities);
UFS_DEV_ATTR(gear, "%01x\n", hba->pwr_info.gear_rx);
UFS_DEV_ATTR(man_id, "%04x\n", hba->dev_info.w_manufacturer_id);
UFS_DEV_ATTR(man_date, "%04X\n", hba->dev_info.w_manufacturer_date);
UFS_DEV_ATTR(sense_err_count, "\"MEDIUM\":\"%d\",\"HWERR\":\"%d\"\n",
		hba->host->medium_err_cnt, hba->host->hw_err_cnt);
UFS_DEV_ATTR(sense_err_logging, "\"LBA0\":\"%lx\",\"LBA1\":\"%lx\",\"LBA2\":\"%lx\""
		",\"LBA3\":\"%lx\",\"LBA4\":\"%lx\",\"LBA5\":\"%lx\""
		",\"LBA6\":\"%lx\",\"LBA7\":\"%lx\",\"LBA8\":\"%lx\",\"LBA9\":\"%lx\""
		",\"REGIONMAP\":\"%016llx\"\n",
		hba->host->issue_LBA_list[0], hba->host->issue_LBA_list[1]
		, hba->host->issue_LBA_list[2], hba->host->issue_LBA_list[3]
		, hba->host->issue_LBA_list[4], hba->host->issue_LBA_list[5]
		, hba->host->issue_LBA_list[6], hba->host->issue_LBA_list[7]
		, hba->host->issue_LBA_list[8], hba->host->issue_LBA_list[9]
		, hba->host->issue_region_map);

static ssize_t ufs_unique_number_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *host = container_of(dev, struct Scsi_Host, shost_dev);
	struct ufs_hba *hba = shost_priv(host);
	return sprintf(buf, "%s\n", hba->unique_number);
}
static DEVICE_ATTR(unique_number, 0440, ufs_unique_number_show, NULL);

static ssize_t ufs_lc_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *host = container_of(dev, struct Scsi_Host, shost_dev);
	struct ufs_hba *hba = shost_priv(host);
	return sprintf(buf, "%u\n", hba->lc_info);
}

static ssize_t ufs_lc_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct Scsi_Host *host = container_of(dev, struct Scsi_Host, shost_dev);
	struct ufs_hba *hba = shost_priv(host);
	unsigned int value;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	hba->lc_info = value;

	return count;
}

static DEVICE_ATTR(lc, 0664, ufs_lc_info_show, ufs_lc_info_store);

static ssize_t ufs_lt_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *host = container_of(dev, struct Scsi_Host, shost_dev);
	struct ufs_hba *hba = shost_priv(host);
	int err = 0;
	u8 *health_buf = NULL;

	if (hba->desc_size.hlth_desc) {
		health_buf = kmalloc(hba->desc_size.hlth_desc, GFP_KERNEL);
		if (!health_buf) {
			err = -ENOMEM;
			dev_err(hba->dev,
					"%s: Failed to allocate health_buf\n", __func__);
			goto out_invalid_lt;
		}
	}

	pm_runtime_get_sync(hba->dev);
	err = ufshcd_read_desc(hba, QUERY_DESC_IDN_HEALTH, 0, health_buf,
			hba->desc_size.hlth_desc);
	pm_runtime_put(hba->dev);

out_invalid_lt:
	if (err) {
		hba->dev_info.i_lt = 0x0;
		dev_err(hba->dev, "%s: HEALTH desc read fail, err = %d\n",
				__func__, err);
	} else {
		hba->dev_info.i_lt = health_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_A];
		dev_info(hba->dev, "LT: 0x%01x%01x\n", health_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_A],
				health_buf[HEALTH_DESC_PARAM_LIFE_TIME_EST_B]);
	}

	if (health_buf)
		kfree(health_buf);

	return sprintf(buf, "%u\n", hba->dev_info.i_lt);
}

static DEVICE_ATTR(lt, 0444, ufs_lt_info_show, NULL);

static struct attribute *ufs_attributes[] = {
	&dev_attr_capabilities.attr,
	&dev_attr_gear.attr,
	&dev_attr_man_id.attr,
	&dev_attr_man_date.attr,
	&dev_attr_unique_number.attr,
	&dev_attr_lc.attr,
	&dev_attr_lt.attr,
	&dev_attr_sense_err_count.attr,
	&dev_attr_sense_err_logging.attr,
#if defined(SEC_UFS_ERROR_COUNT)
	&dev_attr_SEC_UFS_op_cnt.attr,
	&dev_attr_SEC_UFS_uic_cmd_cnt.attr,
	&dev_attr_SEC_UFS_uic_err_cnt.attr,
	&dev_attr_SEC_UFS_fatal_cnt.attr,
	&dev_attr_SEC_UFS_utp_cnt.attr,
	&dev_attr_SEC_UFS_query_cnt.attr,
	&dev_attr_SEC_UFS_err_sum.attr,
#endif
#ifdef CONFIG_BLK_TURBO_WRITE
	&dev_attr_SEC_UFS_TW_info.attr,
#endif
	NULL
};

static struct attribute_group ufs_attribute_group = {
	.attrs  = ufs_attributes,
};

static void ufshcd_add_info_sysfs_node(struct ufs_hba *hba)
{
	int err  = -ENOMEM;
	struct device *dev = &(hba->host->shost_dev);

	err = sysfs_create_group(&dev->kobj, &ufs_attribute_group);

	if (err)
		dev_err(hba->dev, "cannot create sysfs group err: %d\n", err);
}

static inline void ufshcd_add_sysfs_nodes(struct ufs_hba *hba)
{
	ufshcd_add_info_sysfs_node(hba);
#if defined(CONFIG_SCSI_UFS_QCOM)
	ufshcd_add_hw_reset_info_sysfs_nodes(hba);
#endif
}

#if defined(CONFIG_UFSFEATURE) && defined(CONFIG_UFSHPB)
static struct attribute *ufs_hpb_attributes[] = {
	&dev_attr_SEC_UFS_HPB_info.attr,
	&dev_attr_SEC_UFS_HPB_err_info.attr,
	NULL
};

static struct attribute_group ufs_hpb_attribute_group = {
	.attrs  = ufs_hpb_attributes,
};

static void ufshcd_add_hpb_info_sysfs_node(struct ufs_hba *hba)
{
	int err  = -ENOMEM;
	struct device *dev = &(hba->host->shost_dev);

	err = sysfs_create_group(&dev->kobj, &ufs_hpb_attribute_group);

	if (err)
		dev_err(hba->dev, "cannot create hpb sysfs group err: %d\n", err);
}
#endif

static void __ufshcd_shutdown_clkscaling(struct ufs_hba *hba)
{
	bool suspend = false;
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->clk_scaling.is_allowed) {
		hba->clk_scaling.is_allowed = false;
		suspend = true;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/**
	 * Scaling may be scheduled before, hence make sure it
	 * doesn't race with shutdown
	 */
	if (ufshcd_is_clkscaling_supported(hba)) {
		cancel_work_sync(&hba->clk_scaling.suspend_work);
		cancel_work_sync(&hba->clk_scaling.resume_work);
		if (suspend)
			ufshcd_suspend_clkscaling(hba);
	}

	/* Unregister so that devfreq_monitor can't race with shutdown */
	if (hba->devfreq)
		ufshcd_devfreq_remove(hba);
}

static void ufshcd_shutdown_clkscaling(struct ufs_hba *hba)
{
	if (!ufshcd_is_clkscaling_supported(hba))
		return;
	__ufshcd_shutdown_clkscaling(hba);
	device_remove_file(hba->dev, &hba->clk_scaling.enable_attr);
}

/**
 * ufshcd_shutdown - shutdown routine
 * @hba: per adapter instance
 *
 * This function would power off both UFS device and UFS link.
 *
 * Returns 0 always to allow force shutdown even in case of errors.
 */
int ufshcd_shutdown(struct ufs_hba *hba)
{
	int ret = 0;
#if defined(SEC_UFS_ERROR_COUNT)
	struct SEC_UFS_counting *err_info = &(hba->SEC_err_info);
	struct SEC_UFS_op_count *op_cnt = &(err_info->op_count);
	struct SEC_UFS_UIC_err_count *uic_err_cnt = &(err_info->UIC_err_count);
	struct SEC_UFS_UTP_count *utp_err = &(err_info->UTP_count);
	struct SEC_UFS_QUERY_count *query_cnt = &(err_info->query_count);
#endif

	if (!hba->is_powered)
		goto out;

	if (ufshcd_is_ufs_dev_poweroff(hba) && ufshcd_is_link_off(hba))
		goto out;

	pm_runtime_get_sync(hba->dev);
	ufshcd_hold_all(hba);
	ufshcd_mark_shutdown_ongoing(hba);
	ufshcd_shutdown_clkscaling(hba);
	/**
	 * (1) Acquire the lock to stop any more requests
	 * (2) Wait for all issued requests to complete
	 */
	ufshcd_get_write_lock(hba);
	ufshcd_scsi_block_requests(hba);
	ret = ufshcd_wait_for_doorbell_clr(hba, U64_MAX);
	if (ret)
		dev_err(hba->dev, "%s: waiting for DB clear: failed: %d\n",
			__func__, ret);
	/* Requests may have errored out above, let it be handled */
	flush_work(&hba->eh_work);
	/* reqs issued from contexts other than shutdown will fail from now */
	ufshcd_scsi_unblock_requests(hba);
	ufshcd_release_all(hba);
	ret = ufshcd_suspend(hba, UFS_SHUTDOWN_PM);
out:
	if (ret)
		dev_err(hba->dev, "%s failed, err %d\n", __func__, ret);

#if defined(SEC_UFS_ERROR_COUNT)
	dev_err(hba->dev, "Count: %d UIC: %d  UTP:%d QUERY: %d\n",
			op_cnt->HW_RESET_count,
			uic_err_cnt->UIC_err,
			utp_err->UTP_err,
			query_cnt->Query_err);
#endif

	/* allow force shutdown even in case of errors */
	return 0;
}
EXPORT_SYMBOL(ufshcd_shutdown);

/**
 * ufshcd_remove - de-allocate SCSI host and host memory space
 *		data structure memory
 * @hba: per adapter instance
 */
void ufshcd_remove(struct ufs_hba *hba)
{
#if defined(CONFIG_UFSFEATURE)
	ufsf_hpb_release(&hba->ufsf);
#endif
	ufs_sysfs_remove_nodes(hba->dev);
	scsi_remove_host(hba->host);
	/* disable interrupts */
	ufshcd_disable_intr(hba, hba->intr_mask);
	ufshcd_hba_stop(hba, true);

	ufshcd_exit_clk_scaling(hba);
	ufshcd_exit_clk_gating(hba);
	ufshcd_exit_hibern8_on_idle(hba);
	if (ufshcd_is_clkscaling_supported(hba)) {
		device_remove_file(hba->dev, &hba->clk_scaling.enable_attr);
		if (hba->devfreq)
			devfreq_remove_device(hba->devfreq);
	}
	ufshcd_hba_exit(hba);
	ufsdbg_remove_debugfs(hba);
}
EXPORT_SYMBOL_GPL(ufshcd_remove);

/**
 * ufshcd_dealloc_host - deallocate Host Bus Adapter (HBA)
 * @hba: pointer to Host Bus Adapter (HBA)
 */
void ufshcd_dealloc_host(struct ufs_hba *hba)
{
	scsi_host_put(hba->host);
}
EXPORT_SYMBOL_GPL(ufshcd_dealloc_host);

/**
 * ufshcd_set_dma_mask - Set dma mask based on the controller
 *			 addressing capability
 * @hba: per adapter instance
 *
 * Returns 0 for success, non-zero for failure
 */
static int ufshcd_set_dma_mask(struct ufs_hba *hba)
{
	if (hba->capabilities & MASK_64_ADDRESSING_SUPPORT) {
		if (!dma_set_mask_and_coherent(hba->dev, DMA_BIT_MASK(64)))
			return 0;
	}
	return dma_set_mask_and_coherent(hba->dev, DMA_BIT_MASK(32));
}

/**
 * ufshcd_alloc_host - allocate Host Bus Adapter (HBA)
 * @dev: pointer to device handle
 * @hba_handle: driver private handle
 * Returns 0 on success, non-zero value on failure
 */
int ufshcd_alloc_host(struct device *dev, struct ufs_hba **hba_handle)
{
	struct Scsi_Host *host;
	struct ufs_hba *hba;
	int err = 0;

	if (!dev) {
		dev_err(dev,
		"Invalid memory reference for dev is NULL\n");
		err = -ENODEV;
		goto out_error;
	}

	host = scsi_host_alloc(&ufshcd_driver_template,
				sizeof(struct ufs_hba));
	if (!host) {
		dev_err(dev, "scsi_host_alloc failed\n");
		err = -ENOMEM;
		goto out_error;
	}

	/*
	 * Do not use blk-mq at this time because blk-mq does not support
	 * runtime pm.
	 */
	host->use_blk_mq = false;

	hba = shost_priv(host);
	hba->host = host;
	hba->dev = dev;
	*hba_handle = hba;
	hba->sg_entry_size = sizeof(struct ufshcd_sg_entry);

	INIT_LIST_HEAD(&hba->clk_list_head);

out_error:
	return err;
}
EXPORT_SYMBOL(ufshcd_alloc_host);

#if defined(SEC_UFS_ERROR_COUNT)
/**
 * ufs_sec_send_errinfo - Send UFS Error Information to AP
 * Format : U0H0L0X0Q0R0W0F0
 * U : UTP cmd ERRor count
 * H : HWRESET count
 * L : Link startup failure count
 * X : Link Lost Error count
 * Q : UTMR QUERY_TASK error count
 * R : READ error count
 * W : WRITE error count
 * F : Device Fatal Error count
 **/
static void ufs_sec_send_errinfo(void *data)
{
	static struct ufs_hba *hba;
	struct SEC_UFS_counting *err_info;
	char buf[23];

	if (data) {
		hba = (struct ufs_hba *)data;
		return;
	}
	if (!hba) {
		pr_err("%s: hba is not initialized\n", __func__);
		return;
	}
	if (&(hba->SEC_err_info)) {
		err_info = &(hba->SEC_err_info);
		sprintf(buf, "U%1dH%1dL%1dX%1dQ%1dR%1dW%1dF%1dSM%1dSH%1d",
				(err_info->UTP_count.UTP_err > 9)		/* UTP Error */
				? 9 : err_info->UTP_count.UTP_err,
				(err_info->op_count.HW_RESET_count > 9)		/* HW Reset */
				? 9 : err_info->op_count.HW_RESET_count,
				(err_info->op_count.link_startup_count > 9)	/* Link Startup Fail */
				? 9 : err_info->op_count.link_startup_count,
				(err_info->Fatal_err_count.LLE > 9)		/* Link Lost */
				? 9 : err_info->Fatal_err_count.LLE,
				(err_info->UTP_count.UTMR_query_task_count > 9)	/* Query task */
				? 9 : err_info->UTP_count.UTMR_query_task_count,
				(err_info->UTP_count.UTR_read_err > 9)		/* UTRR */
				? 9 : err_info->UTP_count.UTR_read_err,
				(err_info->UTP_count.UTR_write_err > 9)		/* UTRW */
				? 9 : err_info->UTP_count.UTR_write_err,
				(err_info->Fatal_err_count.DFE > 9)		/* Device Fatal Error */
				? 9 : err_info->Fatal_err_count.DFE,
				(hba->host->medium_err_cnt > 9)
				? 9 : hba->host->medium_err_cnt,
				(hba->host->hw_err_cnt > 9)
				? 9 : hba->host->hw_err_cnt);
		pr_err("%s: Send UFS information to AP : %s\n", __func__, buf);
#if defined(CONFIG_SEC_USER_RESET_DEBUG)
		sec_debug_store_additional_dbg(DBG_1_UFS_ERR, 0, "%s", buf);
#endif
	}
}
#else
static void ufs_sec_send_errinfo(void *data)
{
	char buf[23];
	sprintf(buf, "UFSErrCount is disable");
	pr_err("%s: Send UFS information to AP : %s\n", __func__, buf);
#if defined(CONFIG_SEC_USER_RESET_DEBUG)
	sec_debug_store_additional_dbg(DBG_1_UFS_ERR, 0, "%s", buf);
#endif
}
#endif

/**
 * ufshcd_init - Driver initialization routine
 * @hba: per-adapter instance
 * @mmio_base: base register address
 * @irq: Interrupt line of device
 * Returns 0 on success, non-zero value on failure
 */
int ufshcd_init(struct ufs_hba *hba, void __iomem *mmio_base, unsigned int irq)
{
	int err;
	struct Scsi_Host *host = hba->host;
	struct device *dev = hba->dev;
	char recovery_wq_name[sizeof("ufs_recovery_00")];

	if (!mmio_base) {
		dev_err(hba->dev,
		"Invalid memory reference for mmio_base is NULL\n");
		err = -ENODEV;
		goto out_error;
	}

	hba->mmio_base = mmio_base;
	hba->irq = irq;

	/* Set descriptor lengths to specification defaults */
	ufshcd_def_desc_sizes(hba);

	err = ufshcd_hba_init(hba);
	if (err)
		goto out_error;

	/* Read capabilities registers */
	ufshcd_hba_capabilities(hba);

	/* Get UFS version supported by the controller */
	hba->ufs_version = ufshcd_get_ufs_version(hba);

	/* print error message if ufs_version is not valid */
	if ((hba->ufs_version != UFSHCI_VERSION_10) &&
	    (hba->ufs_version != UFSHCI_VERSION_11) &&
	    (hba->ufs_version != UFSHCI_VERSION_20) &&
	    (hba->ufs_version != UFSHCI_VERSION_21) &&
	    (hba->ufs_version != UFSHCI_VERSION_30))
		dev_warn(hba->dev, "invalid UFS version 0x%x\n",
			hba->ufs_version);

	/* Get Interrupt bit mask per version */
	hba->intr_mask = ufshcd_get_intr_mask(hba);

	/* Enable debug prints */
	hba->ufshcd_dbg_print = DEFAULT_UFSHCD_DBG_PRINT_EN;

	err = ufshcd_set_dma_mask(hba);
	if (err) {
		dev_err(hba->dev, "set dma mask failed\n");
		goto out_disable;
	}

	/* Allocate memory for host memory space */
	err = ufshcd_memory_alloc(hba);
	if (err) {
		dev_err(hba->dev, "Memory allocation failed\n");
		goto out_disable;
	}

	/* Configure LRB */
	ufshcd_host_memory_configure(hba);

	host->can_queue = hba->nutrs;
	host->cmd_per_lun = hba->nutrs;
	host->max_id = UFSHCD_MAX_ID;
	host->max_lun = UFS_MAX_LUNS;
	host->max_channel = UFSHCD_MAX_CHANNEL;
	host->unique_id = host->host_no;
	host->max_cmd_len = MAX_CDB_SIZE;
	host->set_dbd_for_caching = 1;
	host->by_ufs = 1;

	hba->max_pwr_info.is_valid = false;

	/* Initailize wait queue for task management */
	init_waitqueue_head(&hba->tm_wq);
	init_waitqueue_head(&hba->tm_tag_wq);

	/* Initialize work queues */
	snprintf(recovery_wq_name, ARRAY_SIZE(recovery_wq_name), "%s_%d",
				"ufs_recovery_wq", host->host_no);
	hba->recovery_wq = alloc_workqueue("%s",
			WQ_MEM_RECLAIM|WQ_UNBOUND|WQ_HIGHPRI, 0,
			recovery_wq_name);
	if (!hba->recovery_wq) {
		dev_err(hba->dev, "%s: failed to create the workqueue\n",
				__func__);
		err = -ENOMEM;
		goto out_disable;
	}

	INIT_WORK(&hba->eh_work, ufshcd_err_handler);
	INIT_WORK(&hba->eeh_work, ufshcd_exception_event_handler);
	INIT_WORK(&hba->rls_work, ufshcd_rls_handler);
	INIT_WORK(&hba->fatal_mode_work, ufshcd_fatal_mode_handler);

	/* Initialize UIC command mutex */
	mutex_init(&hba->uic_cmd_mutex);

	/* Initialize mutex for device management commands */
	mutex_init(&hba->dev_cmd.lock);

#ifdef CONFIG_BLK_TURBO_WRITE
	/* Initialize TW ctrl mutex */
	mutex_init(&hba->tw_ctrl_mutex);
#endif

	init_rwsem(&hba->lock);

	/* Initialize device management tag acquire wait queue */
	init_waitqueue_head(&hba->dev_cmd.tag_wq);

	ufshcd_init_clk_gating(hba);
	ufshcd_init_hibern8(hba);

	ufshcd_init_clk_scaling(hba);

	/*
	 * In order to avoid any spurious interrupt immediately after
	 * registering UFS controller interrupt handler, clear any pending UFS
	 * interrupt status and disable all the UFS interrupts.
	 */
	ufshcd_writel(hba, ufshcd_readl(hba, REG_INTERRUPT_STATUS),
		      REG_INTERRUPT_STATUS);
	ufshcd_writel(hba, 0, REG_INTERRUPT_ENABLE);
	/*
	 * Make sure that UFS interrupts are disabled and any pending interrupt
	 * status is cleared before registering UFS interrupt handler.
	 */
	mb();

	/* IRQ registration */
	err = devm_request_irq(dev, irq, ufshcd_intr, IRQF_SHARED,
				dev_name(dev), hba);
	if (err) {
		dev_err(hba->dev, "request irq failed\n");
		goto exit_gating;
	} else {
		hba->is_irq_enabled = true;
	}

	err = scsi_add_host(host, hba->dev);
	if (err) {
		dev_err(hba->dev, "scsi_add_host failed\n");
		goto exit_gating;
	}

	/* Reset controller to power on reset (POR) state */
	ufshcd_vops_full_reset(hba);

	/* reset connected UFS device */
	err = ufshcd_reset_device(hba);
	if (err)
		dev_warn(hba->dev, "%s: device reset failed. err %d\n",
			 __func__, err);

	/* reset the SEC error info */
	memset(&(hba->SEC_err_info), 0, sizeof(struct SEC_UFS_counting));

	if (hba->force_g4)
		hba->phy_init_g4 = true;
	/* Init crypto */
	err = ufshcd_hba_init_crypto(hba);
	if (err) {
		dev_err(hba->dev, "crypto setup failed\n");
		goto out_remove_scsi_host;
	}

	/* Host controller enable */
	err = ufshcd_hba_enable(hba);
	if (err) {
		dev_err(hba->dev, "Host controller enable failed\n");
		ufshcd_print_host_regs(hba);
		ufshcd_print_host_state(hba);
		goto out_remove_scsi_host;
	}

	/*
	 * If rpm_lvl and and spm_lvl are not already set to valid levels,
	 * set the default power management level for UFS runtime and system
	 * suspend. Default power saving mode selected is keeping UFS link in
	 * Hibern8 state and UFS device in sleep state.
	 */
	if (!ufshcd_is_valid_pm_lvl(hba->rpm_lvl))
		hba->rpm_lvl = ufs_get_desired_pm_lvl_for_dev_link_state(
							UFS_SLEEP_PWR_MODE,
							UIC_LINK_HIBERN8_STATE);
	if (!ufshcd_is_valid_pm_lvl(hba->spm_lvl))
		hba->spm_lvl = ufs_get_desired_pm_lvl_for_dev_link_state(
							UFS_SLEEP_PWR_MODE,
							UIC_LINK_HIBERN8_STATE);

#if defined(CONFIG_SCSI_UFS_TEST_MODE)
	dev_info(hba->dev, "UFS test mode enabled\n");
#endif

	/* init ufs_sec_debug function */
	ufs_sec_send_errinfo(hba);
	ufs_debug_func = ufs_sec_send_errinfo;

	/* Hold auto suspend until async scan completes */
	pm_runtime_get_sync(dev);
	atomic_set(&hba->scsi_block_reqs_cnt, 0);

	/*
	 * The device-initialize-sequence hasn't been invoked yet.
	 * Set the device to power-off state
	 */
	ufshcd_set_ufs_dev_poweroff(hba);

	ufshcd_cmd_log_init(hba);

#if defined(CONFIG_UFSFEATURE)
	 ufsf_hpb_set_init_state(&hba->ufsf);
#if defined(CONFIG_UFSHPB)
	atomic64_set(&(hba->SEC_hpb_info.hpb_pinned_rb_cnt), 0);
	atomic64_set(&(hba->SEC_hpb_info.hpb_active_rb_cnt), 0);
#endif
#endif
	async_schedule(ufshcd_async_scan, hba);

	ufsdbg_add_debugfs(hba);

	ufs_sysfs_add_nodes(hba->dev);

	ufshcd_add_sysfs_nodes(hba);

	return 0;

out_remove_scsi_host:
	scsi_remove_host(hba->host);
exit_gating:
	ufshcd_exit_clk_scaling(hba);
	ufshcd_exit_clk_gating(hba);
out_disable:
	hba->is_irq_enabled = false;
	ufshcd_hba_exit(hba);
out_error:
	return err;
}
EXPORT_SYMBOL_GPL(ufshcd_init);

MODULE_AUTHOR("Santosh Yaragnavi <santosh.sy@samsung.com>");
MODULE_AUTHOR("Vinayak Holikatti <h.vinayak@samsung.com>");
MODULE_DESCRIPTION("Generic UFS host controller driver Core");
MODULE_LICENSE("GPL");
MODULE_VERSION(UFSHCD_DRIVER_VERSION);
