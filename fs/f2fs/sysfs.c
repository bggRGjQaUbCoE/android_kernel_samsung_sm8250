// SPDX-License-Identifier: GPL-2.0
/*
 * f2fs sysfs interface
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 * Copyright (c) 2017 Chao Yu <chao@kernel.org>
 */
#include <linux/compiler.h>
#include <linux/proc_fs.h>
#include <linux/f2fs_fs.h>
#include <linux/seq_file.h>
#include <linux/unicode.h>
#include <linux/ioprio.h>
#include <linux/sysfs.h>
#include <linux/statfs.h>
#include <linux/nls.h>

#include "f2fs.h"
#include "segment.h"
#include "gc.h"
#include "iostat.h"
#include <trace/events/f2fs.h>

#define SEC_BIGDATA_VERSION	(2)

static struct proc_dir_entry *f2fs_proc_root;

/* Sysfs support for f2fs */
enum {
	GC_THREAD,	/* struct f2fs_gc_thread */
	SM_INFO,	/* struct f2fs_sm_info */
	DCC_INFO,	/* struct discard_cmd_control */
	NM_INFO,	/* struct f2fs_nm_info */
	F2FS_SBI,	/* struct f2fs_sb_info */
#ifdef CONFIG_F2FS_STAT_FS
	STAT_INFO,	/* struct f2fs_stat_info */
#endif
#ifdef CONFIG_F2FS_FAULT_INJECTION
	FAULT_INFO_RATE,	/* struct f2fs_fault_info */
	FAULT_INFO_TYPE,	/* struct f2fs_fault_info */
#endif
	RESERVED_BLOCKS,	/* struct f2fs_sb_info */
	CPRC_INFO,	/* struct ckpt_req_control */
	ATGC_INFO,	/* struct atgc_management */
};

#ifdef CONFIG_F2FS_SEC_BLOCK_OPERATIONS_DEBUG
const char *sec_blkops_dbg_type_names[NR_F2FS_SEC_DBG_ENTRY] = {
	"DENTS",
	"IMETA",
	"NODES",
};
#endif

const char *sec_fua_mode_names[NR_F2FS_SEC_FUA_MODE] = {
	"NONE",
	"ROOT",
	"ALL",
};

struct f2fs_attr {
	struct attribute attr;
	ssize_t (*show)(struct f2fs_attr *, struct f2fs_sb_info *, char *);
	ssize_t (*store)(struct f2fs_attr *, struct f2fs_sb_info *,
			 const char *, size_t);
	int struct_type;
	int offset;
	int id;
};

static ssize_t f2fs_sbi_show(struct f2fs_attr *a,
			     struct f2fs_sb_info *sbi, char *buf);

static unsigned char *__struct_ptr(struct f2fs_sb_info *sbi, int struct_type)
{
	if (struct_type == GC_THREAD)
		return (unsigned char *)sbi->gc_thread;
	else if (struct_type == SM_INFO)
		return (unsigned char *)SM_I(sbi);
	else if (struct_type == DCC_INFO)
		return (unsigned char *)SM_I(sbi)->dcc_info;
	else if (struct_type == NM_INFO)
		return (unsigned char *)NM_I(sbi);
	else if (struct_type == F2FS_SBI || struct_type == RESERVED_BLOCKS)
		return (unsigned char *)sbi;
#ifdef CONFIG_F2FS_FAULT_INJECTION
	else if (struct_type == FAULT_INFO_RATE ||
					struct_type == FAULT_INFO_TYPE)
		return (unsigned char *)&F2FS_OPTION(sbi).fault_info;
#endif
#ifdef CONFIG_F2FS_STAT_FS
	else if (struct_type == STAT_INFO)
		return (unsigned char *)F2FS_STAT(sbi);
#endif
	else if (struct_type == CPRC_INFO)
		return (unsigned char *)&sbi->cprc_info;
	else if (struct_type == ATGC_INFO)
		return (unsigned char *)&sbi->am;
	return NULL;
}

static ssize_t dirty_segments_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
	return sprintf(buf, "%llu\n",
			(unsigned long long)(dirty_segments(sbi)));
}

static ssize_t free_segments_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
	return sprintf(buf, "%llu\n",
			(unsigned long long)(free_segments(sbi)));
}

static ssize_t ovp_segments_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
	return sprintf(buf, "%llu\n",
			(unsigned long long)(overprovision_segments(sbi)));
}

static ssize_t lifetime_write_kbytes_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
	return sprintf(buf, "%llu\n",
			(unsigned long long)(sbi->kbytes_written +
			((f2fs_get_sectors_written(sbi) -
				sbi->sectors_written_start) >> 1)));
}

static ssize_t sb_status_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
	return sprintf(buf, "%lx\n", sbi->s_flag);
}

static ssize_t pending_discard_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
	if (!SM_I(sbi)->dcc_info)
		return -EINVAL;
	return sprintf(buf, "%llu\n", (unsigned long long)atomic_read(
				&SM_I(sbi)->dcc_info->discard_cmd_cnt));
}

static ssize_t sec_fs_stat_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
	struct dentry *root = sbi->sb->s_root;
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	struct kstatfs statbuf;
	int ret;

	if (!root->d_sb->s_op->statfs)
		goto errout;

	ret = root->d_sb->s_op->statfs(root, &statbuf);
	if (ret)
		goto errout;

	return snprintf(buf, PAGE_SIZE, "\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%u\","
		"\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%u\",\"%s\":\"%u\","
		"\"%s\":\"%d\"\n",
		"F_BLOCKS", statbuf.f_blocks,
		"F_BFREE", statbuf.f_bfree,
		"F_SFREE", free_sections(sbi),
		"F_FILES", statbuf.f_files,
		"F_FFREE", statbuf.f_ffree,
		"F_FUSED", ckpt->valid_inode_count,
		"F_NUSED", ckpt->valid_node_count,
		"F_VER", SEC_BIGDATA_VERSION);

errout:
	return snprintf(buf, PAGE_SIZE, "\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\","
		"\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\","
		"\"%s\":\"%d\"\n",
		"F_BLOCKS", 0, "F_BFREE", 0, "F_SFREE", 0, "F_FILES", 0,
		"F_FFREE", 0, "F_FUSED", 0, "F_NUSED", 0, "F_VER", SEC_BIGDATA_VERSION);
}

static ssize_t features_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
	int len = 0;

	if (f2fs_sb_has_encrypt(sbi))
		len += scnprintf(buf, PAGE_SIZE - len, "%s",
						"encryption");
	if (f2fs_sb_has_blkzoned(sbi))
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "blkzoned");
	if (f2fs_sb_has_extra_attr(sbi))
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "extra_attr");
	if (f2fs_sb_has_project_quota(sbi))
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "projquota");
	if (f2fs_sb_has_inode_chksum(sbi))
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "inode_checksum");
	if (f2fs_sb_has_flexible_inline_xattr(sbi))
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "flexible_inline_xattr");
	if (f2fs_sb_has_quota_ino(sbi))
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "quota_ino");
	if (f2fs_sb_has_inode_crtime(sbi))
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "inode_crtime");
	if (f2fs_sb_has_lost_found(sbi))
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "lost_found");
	if (f2fs_sb_has_verity(sbi))
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "verity");
	if (f2fs_sb_has_sb_chksum(sbi))
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "sb_checksum");
	if (f2fs_sb_has_casefold(sbi))
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "casefold");
	if (f2fs_sb_has_readonly(sbi))
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "readonly");
	if (f2fs_sb_has_compression(sbi))
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "compression");
	len += scnprintf(buf + len, PAGE_SIZE - len, "%s%s",
				len ? ", " : "", "pin_file");
	len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
	return len;
}

static ssize_t current_reserved_blocks_show(struct f2fs_attr *a,
					struct f2fs_sb_info *sbi, char *buf)
{
	return sprintf(buf, "%u\n", sbi->current_reserved_blocks);
}

static ssize_t unusable_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
	block_t unusable;

	if (test_opt(sbi, DISABLE_CHECKPOINT))
		unusable = sbi->unusable_block_count;
	else
		unusable = f2fs_get_unusable_blocks(sbi);
	return sprintf(buf, "%llu\n", (unsigned long long)unusable);
}

static ssize_t encoding_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
#ifdef CONFIG_UNICODE
	struct super_block *sb = sbi->sb;

	if (f2fs_sb_has_casefold(sbi))
		return snprintf(buf, PAGE_SIZE, "%s (%d.%d.%d)\n",
			sb->s_encoding->charset,
			(sb->s_encoding->version >> 16) & 0xff,
			(sb->s_encoding->version >> 8) & 0xff,
			sb->s_encoding->version & 0xff);
#endif
	return sprintf(buf, "(none)");
}

static ssize_t mounted_time_sec_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
	return sprintf(buf, "%llu", SIT_I(sbi)->mounted_time);
}

#ifdef CONFIG_F2FS_STAT_FS
static ssize_t moved_blocks_foreground_show(struct f2fs_attr *a,
				struct f2fs_sb_info *sbi, char *buf)
{
	struct f2fs_stat_info *si = F2FS_STAT(sbi);

	return sprintf(buf, "%llu\n",
		(unsigned long long)(si->tot_blks -
			(si->bg_data_blks + si->bg_node_blks)));
}

static ssize_t moved_blocks_background_show(struct f2fs_attr *a,
				struct f2fs_sb_info *sbi, char *buf)
{
	struct f2fs_stat_info *si = F2FS_STAT(sbi);

	return sprintf(buf, "%llu\n",
		(unsigned long long)(si->bg_data_blks + si->bg_node_blks));
}

static ssize_t avg_vblocks_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
	struct f2fs_stat_info *si = F2FS_STAT(sbi);

	si->dirty_count = dirty_segments(sbi);
	f2fs_update_sit_info(sbi);
	return sprintf(buf, "%llu\n", (unsigned long long)(si->avg_vblocks));
}
#endif

#if defined(CONFIG_F2FS_SEC_BLOCK_OPERATIONS_DEBUG) && defined(CONFIG_F2FS_SEC_DEBUG_NODE)
static int f2fs_sec_blockops_dbg(struct f2fs_sb_info *sbi, char *buf, int src_len) {
	int len = src_len;
	int i, j;

	len += snprintf(buf + len, PAGE_SIZE - len, "\nblock_operations() DBG : %u, max : %llu\n",
			sbi->s_sec_blkops_total,
			sbi->s_sec_blkops_max_elapsed);
	for (i = 0; i < F2FS_SEC_BLKOPS_ENTRIES; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, " - [%u - %s(%d)] S: %llu, E: %llu [%llu]",
				sbi->s_sec_dbg_entries[i].entry_idx,
				sec_blkops_dbg_type_names[sbi->s_sec_dbg_entries[i].step],
				sbi->s_sec_dbg_entries[i].ret_val,
				sbi->s_sec_dbg_entries[i].start_time,
				sbi->s_sec_dbg_entries[i].end_time,
				(sbi->s_sec_dbg_entries[i].end_time - sbi->s_sec_dbg_entries[i].start_time));

		for(j = 0; j < NR_F2FS_SEC_DBG_ENTRY; j++) {
			len += snprintf(buf + len, PAGE_SIZE - len, ", %s: [%u] [%llu]",
					sec_blkops_dbg_type_names[j],
					sbi->s_sec_dbg_entries[i].entry[j].nr_ops,
					sbi->s_sec_dbg_entries[i].entry[j].cumulative_jiffies);
		}
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "\n - [MAX - %s(%d)] S: %llu, E: %llu [%llu]",
				sec_blkops_dbg_type_names[sbi->s_sec_dbg_max_entry.step],
				sbi->s_sec_dbg_max_entry.ret_val,
				sbi->s_sec_dbg_max_entry.start_time,
				sbi->s_sec_dbg_max_entry.end_time,
				(sbi->s_sec_dbg_max_entry.end_time - sbi->s_sec_dbg_max_entry.start_time));
	for(j = 0; j < NR_F2FS_SEC_DBG_ENTRY; j++) {
		len += snprintf(buf + len, PAGE_SIZE - len, ", %s: [%u] [%llu]",
				sec_blkops_dbg_type_names[j],
				sbi->s_sec_dbg_max_entry.entry[j].nr_ops,
				sbi->s_sec_dbg_max_entry.entry[j].cumulative_jiffies);
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return (len - src_len);
}
#endif

#ifdef CONFIG_F2FS_SEC_DEBUG_NODE
/* Copy from debug.c stat_show */
static ssize_t f2fs_sec_stats_show(struct f2fs_sb_info *sbi, char *buf)
{
	struct f2fs_stat_info *si = sbi->stat_info;
	int i = 0, len = 0;
	int j;

	f2fs_update_sec_stats(sbi);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"\n=====[ partition info(%pg). #%d, %s, CP: %s]=====\n",
			si->sbi->sb->s_bdev, i++,
			f2fs_readonly(si->sbi->sb) ? "RO": "RW",
			is_set_ckpt_flags(si->sbi, CP_DISABLED_FLAG) ?
			"Disabled": (f2fs_cp_error(si->sbi) ? "Error": "Good"));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"[SB: 1] [CP: 2] [SIT: %d] [NAT: %d] ",
			si->sit_area_segs, si->nat_area_segs);
	len += snprintf(buf + len, PAGE_SIZE - len, "[SSA: %d] [MAIN: %d",
			si->ssa_area_segs, si->main_area_segs);
	len += snprintf(buf + len, PAGE_SIZE - len, "(OverProv:%d Resv:%d)]\n\n",
			si->overp_segs, si->rsvd_segs);
	if (test_opt(si->sbi, DISCARD))
		len += snprintf(buf + len, PAGE_SIZE - len, "Utilization: %u%% (%u valid blocks, %u discard blocks)\n",
				si->utilization, si->valid_count, si->discard_blks);
	else
		len += snprintf(buf + len, PAGE_SIZE - len, "Utilization: %u%% (%u valid blocks)\n",
				si->utilization, si->valid_count);

	len += snprintf(buf + len, PAGE_SIZE - len, "  - Node: %u (Inode: %u, ",
			si->valid_node_count, si->valid_inode_count);
	len += snprintf(buf + len, PAGE_SIZE - len, "Other: %u)\n  - Data: %u\n",
			si->valid_node_count - si->valid_inode_count,
			si->valid_count - si->valid_node_count);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - Inline_xattr Inode: %u\n",
			si->inline_xattr);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - Inline_data Inode: %u\n",
			si->inline_inode);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - Inline_dentry Inode: %u\n",
			si->inline_dir);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - Orphan/Append/Update Inode: %u, %u, %u\n",
			si->orphans, si->append, si->update);
	len += snprintf(buf + len, PAGE_SIZE - len, "\nMain area: %d segs, %d secs %d zones\n",
			si->main_area_segs, si->main_area_sections,
			si->main_area_zones);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - COLD  data: %d, %d, %d\n",
			si->curseg[CURSEG_COLD_DATA],
			si->cursec[CURSEG_COLD_DATA],
			si->curzone[CURSEG_COLD_DATA]);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - WARM  data: %d, %d, %d\n",
			si->curseg[CURSEG_WARM_DATA],
			si->cursec[CURSEG_WARM_DATA],
			si->curzone[CURSEG_WARM_DATA]);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - HOT   data: %d, %d, %d\n",
			si->curseg[CURSEG_HOT_DATA],
			si->cursec[CURSEG_HOT_DATA],
			si->curzone[CURSEG_HOT_DATA]);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - Dir   dnode: %d, %d, %d\n",
			si->curseg[CURSEG_HOT_NODE],
			si->cursec[CURSEG_HOT_NODE],
			si->curzone[CURSEG_HOT_NODE]);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - File   dnode: %d, %d, %d\n",
			si->curseg[CURSEG_WARM_NODE],
			si->cursec[CURSEG_WARM_NODE],
			si->curzone[CURSEG_WARM_NODE]);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - Indir nodes: %d, %d, %d\n",
			si->curseg[CURSEG_COLD_NODE],
			si->cursec[CURSEG_COLD_NODE],
			si->curzone[CURSEG_COLD_NODE]);
	len += snprintf(buf + len, PAGE_SIZE - len, "\n  - Valid: %d\n  - Dirty: %d\n",
			si->main_area_segs - si->dirty_count -
			si->prefree_count - si->free_segs,
			si->dirty_count);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - Prefree: %d\n  - Free: %d (%d)\n\n",
			si->prefree_count, si->free_segs, si->free_secs);
	len += snprintf(buf + len, PAGE_SIZE - len, "CP calls: %d (BG: %d)\n",
			si->cp_count, si->bg_cp_count);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - cp blocks : %u\n", si->meta_count[META_CP]);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - sit blocks : %u\n",
			si->meta_count[META_SIT]);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - nat blocks : %u\n",
			si->meta_count[META_NAT]);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - ssa blocks : %u\n",
			si->meta_count[META_SSA]);
	len += snprintf(buf + len, PAGE_SIZE - len, "GC calls: %d (BG: %d)\n",
			si->call_count, si->bg_gc);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - data segments : %d (%d)\n",
			si->data_segs, si->bg_data_segs);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - node segments : %d (%d)\n",
			si->node_segs, si->bg_node_segs);
	len += snprintf(buf + len, PAGE_SIZE - len, "Try to move %d blocks (BG: %d)\n", si->tot_blks,
			si->bg_data_blks + si->bg_node_blks);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - data blocks : %d (%d)\n", si->data_blks,
			si->bg_data_blks);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - node blocks : %d (%d)\n", si->node_blks,
			si->bg_node_blks);
	len += snprintf(buf + len, PAGE_SIZE - len, "Skipped : atomic write %llu (%llu)\n",
			si->skipped_atomic_files[BG_GC] +
			si->skipped_atomic_files[FG_GC],
			si->skipped_atomic_files[BG_GC]);
	len += snprintf(buf + len, PAGE_SIZE - len, "BG skip : IO: %u, Other: %u\n",
			si->io_skip_bggc, si->other_skip_bggc);
	len += snprintf(buf + len, PAGE_SIZE - len, "\nExtent Cache:\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "  - Hit Count: L1-1:%llu L1-2:%llu L2:%llu\n",
			si->hit_largest, si->hit_cached,
			si->hit_rbtree);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - Hit Ratio: %llu%% (%llu / %llu)\n",
			!si->total_ext ? 0 :
			div64_u64(si->hit_total * 100, si->total_ext),
			si->hit_total, si->total_ext);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - Inner Struct Count: tree: %d(%d), node: %d\n",
			si->ext_tree, si->zombie_tree, si->ext_node);
	len += snprintf(buf + len, PAGE_SIZE - len, "\nBalancing F2FS Async:\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "  - IO_R (Data: %4d, Node: %4d, Meta: %4d\n",
			si->nr_rd_data, si->nr_rd_node, si->nr_rd_meta);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - IO_W (CP: %4d, Data: %4d, Flush: (%4d %4d %4d), "
			"Discard: (%4d %4d)) cmd: %4d undiscard:%4u\n",
			si->nr_wb_cp_data, si->nr_wb_data,
			si->nr_flushing, si->nr_flushed,
			si->flush_list_empty,
			si->nr_discarding, si->nr_discarded,
			si->nr_discard_cmd, si->undiscard_blks);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - inmem: %4d, atomic IO: %4d (Max. %4d), "
			"volatile IO: %4d (Max. %4d)\n",
			si->inmem_pages, si->aw_cnt, si->max_aw_cnt,
			si->vw_cnt, si->max_vw_cnt);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - nodes: %4d in %4d\n",
			si->ndirty_node, si->node_pages);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - dents: %4d in dirs:%4d (%4d)\n",
			si->ndirty_dent, si->ndirty_dirs, si->ndirty_all);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - datas: %4d in files:%4d\n",
			si->ndirty_data, si->ndirty_files);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - quota datas: %4d in quota files:%4d\n",
			si->ndirty_qdata, si->nquota_files);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - meta: %4d in %4d\n",
			si->ndirty_meta, si->meta_pages);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - imeta: %4d\n",
			si->ndirty_imeta);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - NATs: %9d/%9d\n  - SITs: %9d/%9d\n",
			si->dirty_nats, si->nats, si->dirty_sits, si->sits);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - free_nids: %9d/%9d\n  - alloc_nids: %9d\n",
			si->free_nids, si->avail_nids, si->alloc_nids);
	len += snprintf(buf + len, PAGE_SIZE - len, "\nDistribution of User Blocks:");
	len += snprintf(buf + len, PAGE_SIZE - len, " [ valid | invalid | free ]\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "  [");

	for (j = 0; j < si->util_valid; j++)
		len += snprintf(buf + len, PAGE_SIZE - len, "-");
	len += snprintf(buf + len, PAGE_SIZE - len, "|");

	for (j = 0; j < si->util_invalid; j++)
		len += snprintf(buf + len, PAGE_SIZE - len, "-");
	len += snprintf(buf + len, PAGE_SIZE - len, "|");

	for (j = 0; j < si->util_free; j++)
		len += snprintf(buf + len, PAGE_SIZE - len, "-");
	len += snprintf(buf + len, PAGE_SIZE - len, "]\n\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "IPU: %u blocks\n", si->inplace_count);
	len += snprintf(buf + len, PAGE_SIZE - len, "SSR: %u blocks in %u segments\n",
			si->block_count[SSR], si->segment_count[SSR]);
	len += snprintf(buf + len, PAGE_SIZE - len, "LFS: %u blocks in %u segments\n",
			si->block_count[LFS], si->segment_count[LFS]);

	/* segment usage info */
	len += snprintf(buf + len, PAGE_SIZE - len, "\nBDF: %u, avg. vblocks: %u\n",
			si->bimodal, si->avg_vblocks);

	/* memory footprint */
	len += snprintf(buf + len, PAGE_SIZE - len, "\nMemory: %llu KB\n",
			(si->base_mem + si->cache_mem + si->page_mem) >> 10);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - static: %llu KB\n",
			si->base_mem >> 10);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - cached: %llu KB\n",
			si->cache_mem >> 10);
	len += snprintf(buf + len, PAGE_SIZE - len, "  - paged : %llu KB\n",
			si->page_mem >> 10);

#if defined(CONFIG_F2FS_SEC_BLOCK_OPERATIONS_DEBUG) && defined(CONFIG_F2FS_STAT_FS)
	/* block_operations debug node */
	len += f2fs_sec_blockops_dbg(sbi, buf, len);
#endif
	return len;
}
#endif

static void __sec_bigdata_init_value(struct f2fs_sb_info *sbi,
		const char *attr_name)
{
	unsigned int i = 0;

	if (!strcmp(attr_name, "sec_gc_stat")) {
		sbi->sec_stat.gc_count[BG_GC] = 0;
		sbi->sec_stat.gc_count[FG_GC] = 0;
		sbi->sec_stat.gc_node_seg_count[BG_GC] = 0;
		sbi->sec_stat.gc_node_seg_count[FG_GC] = 0;
		sbi->sec_stat.gc_data_seg_count[BG_GC] = 0;
		sbi->sec_stat.gc_data_seg_count[FG_GC] = 0;
		sbi->sec_stat.gc_node_blk_count[BG_GC] = 0;
		sbi->sec_stat.gc_node_blk_count[FG_GC] = 0;
		sbi->sec_stat.gc_data_blk_count[BG_GC] = 0;
		sbi->sec_stat.gc_data_blk_count[FG_GC] = 0;
		sbi->sec_stat.gc_ttime[BG_GC] = 0;
		sbi->sec_stat.gc_ttime[FG_GC] = 0;
	} else if (!strcmp(attr_name, "sec_io_stat")) {
		sbi->sec_stat.cp_cnt[STAT_CP_ALL] = 0;
		sbi->sec_stat.cp_cnt[STAT_CP_BG] = 0;
		sbi->sec_stat.cp_cnt[STAT_CP_FSYNC] = 0;
		for (i = 0; i < NR_CP_REASON; i++)
			sbi->sec_stat.cpr_cnt[i] = 0;
		sbi->sec_stat.cp_max_interval = 0;
		sbi->sec_stat.alloc_seg_type[LFS] = 0;
		sbi->sec_stat.alloc_seg_type[SSR] = 0;
		sbi->sec_stat.alloc_blk_count[LFS] = 0;
		sbi->sec_stat.alloc_blk_count[SSR] = 0;
		atomic64_set(&sbi->sec_stat.inplace_count, 0);
		sbi->sec_stat.fsync_count = 0;
		sbi->sec_stat.fsync_dirty_pages = 0;
		sbi->sec_stat.hot_file_written_blocks = 0;
		sbi->sec_stat.cold_file_written_blocks = 0;
		sbi->sec_stat.warm_file_written_blocks = 0;
		sbi->sec_stat.max_inmem_pages = 0;
		sbi->sec_stat.drop_inmem_all = 0;
		sbi->sec_stat.drop_inmem_files = 0;
		if (sbi->sb->s_bdev->bd_part)
			sbi->sec_stat.kwritten_byte = BD_PART_WRITTEN(sbi);
		sbi->sec_stat.fs_por_error = 0;
		sbi->sec_stat.fs_error = 0;
		sbi->sec_stat.max_undiscard_blks = 0;
	} else if (!strcmp(attr_name, "sec_fsck_stat")) {
		sbi->sec_fsck_stat.fsck_read_bytes = 0;
		sbi->sec_fsck_stat.fsck_written_bytes = 0;
		sbi->sec_fsck_stat.fsck_elapsed_time = 0;
		sbi->sec_fsck_stat.fsck_exit_code = 0;
		sbi->sec_fsck_stat.valid_node_count = 0;
		sbi->sec_fsck_stat.valid_inode_count = 0;
	} else if (!strcmp(attr_name, "sec_defrag_stat")) {
		sbi->s_sec_part_best_extents = 0;
		sbi->s_sec_part_current_extents = 0;
		sbi->s_sec_part_score = 0;
		sbi->s_sec_defrag_writes_kb = 0;
		sbi->s_sec_num_apps = 0;
		sbi->s_sec_capacity_apps_kb = 0;
	}
}

static ssize_t main_blkaddr_show(struct f2fs_attr *a,
				struct f2fs_sb_info *sbi, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%llu\n",
			(unsigned long long)MAIN_BLKADDR(sbi));
}

static ssize_t f2fs_sbi_show(struct f2fs_attr *a,
			struct f2fs_sb_info *sbi, char *buf)
{
	unsigned char *ptr = NULL;
	unsigned int *ui;

	ptr = __struct_ptr(sbi, a->struct_type);
	if (!ptr)
		return -EINVAL;

	if (!strcmp(a->attr.name, "extension_list")) {
		__u8 (*extlist)[F2FS_EXTENSION_LEN] =
					sbi->raw_super->extension_list;
		int cold_count = le32_to_cpu(sbi->raw_super->extension_count);
		int hot_count = sbi->raw_super->hot_ext_count;
		int len = 0, i;

		len += scnprintf(buf + len, PAGE_SIZE - len,
						"cold file extension:\n");
		for (i = 0; i < cold_count; i++)
			len += scnprintf(buf + len, PAGE_SIZE - len, "%s\n",
								extlist[i]);

		len += scnprintf(buf + len, PAGE_SIZE - len,
						"hot file extension:\n");
		for (i = cold_count; i < cold_count + hot_count; i++)
			len += scnprintf(buf + len, PAGE_SIZE - len, "%s\n",
								extlist[i]);
		return len;
	} else if (!strcmp(a->attr.name, "sec_gc_stat")) {
		int len = 0;

		len = snprintf(buf, PAGE_SIZE, "\"%s\":\"%llu\",\"%s\":\"%llu\","
		"\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\","
		"\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\","
		"\"%s\":\"%llu\",\"%s\":\"%llu\"\n",
			"FGGC", sbi->sec_stat.gc_count[FG_GC],
			"FGGC_NSEG", sbi->sec_stat.gc_node_seg_count[FG_GC],
			"FGGC_NBLK", sbi->sec_stat.gc_node_blk_count[FG_GC],
			"FGGC_DSEG", sbi->sec_stat.gc_data_seg_count[FG_GC],
			"FGGC_DBLK", sbi->sec_stat.gc_data_blk_count[FG_GC],
			"FGGC_TTIME", sbi->sec_stat.gc_ttime[FG_GC],
			"BGGC", sbi->sec_stat.gc_count[BG_GC],
			"BGGC_NSEG", sbi->sec_stat.gc_node_seg_count[BG_GC],
			"BGGC_NBLK", sbi->sec_stat.gc_node_blk_count[BG_GC],
			"BGGC_DSEG", sbi->sec_stat.gc_data_seg_count[BG_GC],
			"BGGC_DBLK", sbi->sec_stat.gc_data_blk_count[BG_GC],
			"BGGC_TTIME", sbi->sec_stat.gc_ttime[BG_GC]);

		if (!sbi->sec_hqm_preserve)
			__sec_bigdata_init_value(sbi, a->attr.name);

		return len;
	} else if (!strcmp(a->attr.name, "sec_io_stat")) {
		u64 kbytes_written = 0;
		int len = 0;

		if (sbi->sb->s_bdev->bd_part)
			kbytes_written = BD_PART_WRITTEN(sbi) -
					 sbi->sec_stat.kwritten_byte;

		len = snprintf(buf, PAGE_SIZE, "\"%s\":\"%llu\",\"%s\":\"%llu\","
		"\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\","
		"\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\","
		"\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\","
		"\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\","
		"\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%u\","
		"\"%s\":\"%u\",\"%s\":\"%u\"\n",
			"CP",		sbi->sec_stat.cp_cnt[STAT_CP_ALL],
			"CPBG",		sbi->sec_stat.cp_cnt[STAT_CP_BG],
			"CPSYNC",	sbi->sec_stat.cp_cnt[STAT_CP_FSYNC],
			"CPNONRE",	sbi->sec_stat.cpr_cnt[CP_NON_REGULAR],
			"CPSBNEED",	sbi->sec_stat.cpr_cnt[CP_SB_NEED_CP],
			"CPWPINO",	sbi->sec_stat.cpr_cnt[CP_WRONG_PINO],
			"CP_MAX_INT",	sbi->sec_stat.cp_max_interval,
			"LFSSEG",	sbi->sec_stat.alloc_seg_type[LFS],
			"SSRSEG",	sbi->sec_stat.alloc_seg_type[SSR],
			"LFSBLK",	sbi->sec_stat.alloc_blk_count[LFS],
			"SSRBLK",	sbi->sec_stat.alloc_blk_count[SSR],
			"IPU",		(u64)atomic64_read(&sbi->sec_stat.inplace_count),
			"FSYNC",	sbi->sec_stat.fsync_count,
			"FSYNC_MB",	sbi->sec_stat.fsync_dirty_pages >> 8,
			"HOT_DATA",	sbi->sec_stat.hot_file_written_blocks >> 8,
			"COLD_DATA",	sbi->sec_stat.cold_file_written_blocks >> 8,
			"WARM_DATA",	sbi->sec_stat.warm_file_written_blocks >> 8,
			"MAX_INMEM",	sbi->sec_stat.max_inmem_pages,
			"DROP_INMEM",	sbi->sec_stat.drop_inmem_all,
			"DROP_INMEMF",	sbi->sec_stat.drop_inmem_files,
			"WRITE_MB",	(u64)(kbytes_written >> 10),
			"FS_PERROR",	sbi->sec_stat.fs_por_error,
			"FS_ERROR",	sbi->sec_stat.fs_error,
			"MAX_UNDSCD",	sbi->sec_stat.max_undiscard_blks);

		if (!sbi->sec_hqm_preserve)
			__sec_bigdata_init_value(sbi, a->attr.name);

		return len;
	} else if (!strcmp(a->attr.name, "sec_fsck_stat")) {
		int len = 0;

		len = snprintf(buf, PAGE_SIZE,
		"\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%llu\",\"%s\":\"%u\","
		"\"%s\":\"%u\",\"%s\":\"%u\"\n",
			"FSCK_RBYTES",	sbi->sec_fsck_stat.fsck_read_bytes,
			"FSCK_WBYTES",	sbi->sec_fsck_stat.fsck_written_bytes,
			"FSCK_TIME_MS",	sbi->sec_fsck_stat.fsck_elapsed_time,
			"FSCK_EXIT",	sbi->sec_fsck_stat.fsck_exit_code,
			"FSCK_VNODES",	sbi->sec_fsck_stat.valid_node_count,
			"FSCK_VINODES",	sbi->sec_fsck_stat.valid_inode_count);

		if (!sbi->sec_hqm_preserve)
			__sec_bigdata_init_value(sbi, a->attr.name);

		return len;
	} else if (!strcmp(a->attr.name, "sec_heimdallfs_stat")) {
		return snprintf(buf, PAGE_SIZE,
		"\"%s\":\"%u\",\"%s\":\"%llu\",\"%s\":\"%u\",\"%s\":\"%llu\",\"%s\":\"%llu\"\n",
			"NR_PKGS", sbi->sec_heimdallfs_stat.nr_pkgs,
			"NR_PKG_BLKS", sbi->sec_heimdallfs_stat.nr_pkg_blks,
			"NR_COMP_PKGS", sbi->sec_heimdallfs_stat.nr_comp_pkgs,
			"NR_COMP_PKG_BLKS", sbi->sec_heimdallfs_stat.nr_comp_pkg_blks,
			"NR_COMP_PKG_SAVED_BLKS", sbi->sec_heimdallfs_stat.nr_comp_saved_blks);
	} else if (!strcmp(a->attr.name, "sec_defrag_stat")) {
		int len = 0;

		len = snprintf(buf, PAGE_SIZE,
		"\"%s\":\"%u\",\"%s\":\"%u\",\"%s\":\"%u\",\"%s\":\"%u\",\"%s\":\"%u\",\"%s\":\"%u\"\n",
			"BESTEXT",  sbi->s_sec_part_best_extents,
			"CUREXT",   sbi->s_sec_part_current_extents,
			"DEFSCORE", sbi->s_sec_part_score,
			"DEFWRITE", sbi->s_sec_defrag_writes_kb,
			"NUMAPP",   sbi->s_sec_num_apps,
			"CAPAPP",   sbi->s_sec_capacity_apps_kb);

		if (!sbi->sec_hqm_preserve)
			__sec_bigdata_init_value(sbi, a->attr.name);

		return len;
	} else if (!strcmp(a->attr.name, "sec_fua_mode")) {
		int len = 0, i;

		for (i = 0; i < NR_F2FS_SEC_FUA_MODE; i++) {
			if (i == sbi->s_sec_cond_fua_mode)
				len += snprintf(buf, PAGE_SIZE, "[%s] ", 
						sec_fua_mode_names[i]);
			else
				len += snprintf(buf, PAGE_SIZE, "%s ", 
						sec_fua_mode_names[i]);
		}

		return len;
#ifdef CONFIG_F2FS_SEC_DEBUG_NODE
	} else if (!strcmp(a->attr.name, "sec_stats")) {
		return f2fs_sec_stats_show(sbi, buf);
#endif
	}

	if (!strcmp(a->attr.name, "ckpt_thread_ioprio")) {
		struct ckpt_req_control *cprc = &sbi->cprc_info;
		int len = 0;
		int class = IOPRIO_PRIO_CLASS(cprc->ckpt_thread_ioprio);
		int data = IOPRIO_PRIO_DATA(cprc->ckpt_thread_ioprio);

		if (class == IOPRIO_CLASS_RT)
			len += scnprintf(buf + len, PAGE_SIZE - len, "rt,");
		else if (class == IOPRIO_CLASS_BE)
			len += scnprintf(buf + len, PAGE_SIZE - len, "be,");
		else
			return -EINVAL;

		len += scnprintf(buf + len, PAGE_SIZE - len, "%d\n", data);
		return len;
	}

#ifdef CONFIG_F2FS_FS_COMPRESSION
	if (!strcmp(a->attr.name, "compr_written_block"))
		return snprintf(buf, PAGE_SIZE, "%llu\n",
						sbi->compr_written_block);

	if (!strcmp(a->attr.name, "compr_saved_block"))
		return snprintf(buf, PAGE_SIZE, "%llu\n",
						sbi->compr_saved_block);

	if (!strcmp(a->attr.name, "compr_new_inode"))
		return snprintf(buf, PAGE_SIZE, "%u\n",
						sbi->compr_new_inode);
#endif

	if (!strcmp(a->attr.name, "gc_segment_mode"))
		return snprintf(buf, PAGE_SIZE, "%u\n", sbi->gc_segment_mode);

	if (!strcmp(a->attr.name, "gc_reclaimed_segments")) {
		return snprintf(buf, PAGE_SIZE, "%u\n",
			sbi->gc_reclaimed_segs[sbi->gc_segment_mode]);
	}

	ui = (unsigned int *)(ptr + a->offset);

	return sprintf(buf, "%u\n", *ui);
}

static ssize_t __sbi_store(struct f2fs_attr *a,
			struct f2fs_sb_info *sbi,
			const char *buf, size_t count)
{
	unsigned char *ptr;
	unsigned long t;
	unsigned int *ui;
	ssize_t ret;

	ptr = __struct_ptr(sbi, a->struct_type);
	if (!ptr)
		return -EINVAL;

	if (!strcmp(a->attr.name, "extension_list")) {
		const char *name = strim((char *)buf);
		bool set = true, hot;

		if (!strncmp(name, "[h]", 3))
			hot = true;
		else if (!strncmp(name, "[c]", 3))
			hot = false;
		else
			return -EINVAL;

		name += 3;

		if (*name == '!') {
			name++;
			set = false;
		}

		if (!strlen(name) || strlen(name) >= F2FS_EXTENSION_LEN)
			return -EINVAL;

		f2fs_down_write(&sbi->sb_lock);

		ret = f2fs_update_extension_list(sbi, name, hot, set);
		if (ret)
			goto out;

		ret = f2fs_commit_super(sbi, false);
		if (ret)
			f2fs_update_extension_list(sbi, name, hot, !set);
out:
		f2fs_up_write(&sbi->sb_lock);
		return ret ? ret : count;
	} else if(!strcmp(a->attr.name, "sec_gc_stat")) {
		__sec_bigdata_init_value(sbi, a->attr.name);
		return count;
	} else if (!strcmp(a->attr.name, "sec_io_stat")) {
		__sec_bigdata_init_value(sbi, a->attr.name);
		return count;
	} else if (!strcmp(a->attr.name, "sec_fsck_stat")) {
		__sec_bigdata_init_value(sbi, a->attr.name);
		return count;
	} else if (!strcmp(a->attr.name, "sec_defrag_stat")) {
		__sec_bigdata_init_value(sbi, a->attr.name);
		return count;
	} else if (!strcmp(a->attr.name, "sec_fua_mode")) {
		const char *mode= strim((char *)buf);
		int idx;

		for (idx = 0; idx < NR_F2FS_SEC_FUA_MODE; idx++) {
			if(!strcmp(mode, sec_fua_mode_names[idx]))
				sbi->s_sec_cond_fua_mode = idx;
		}

		return count;
	}

	if (!strcmp(a->attr.name, "ckpt_thread_ioprio")) {
		const char *name = strim((char *)buf);
		struct ckpt_req_control *cprc = &sbi->cprc_info;
		int class;
		long data;
		int ret;

		if (!strncmp(name, "rt,", 3))
			class = IOPRIO_CLASS_RT;
		else if (!strncmp(name, "be,", 3))
			class = IOPRIO_CLASS_BE;
		else
			return -EINVAL;

		name += 3;
		ret = kstrtol(name, 10, &data);
		if (ret)
			return ret;
		if (data >= IOPRIO_BE_NR || data < 0)
			return -EINVAL;

		cprc->ckpt_thread_ioprio = IOPRIO_PRIO_VALUE(class, data);
		if (test_opt(sbi, MERGE_CHECKPOINT)) {
			ret = set_task_ioprio(cprc->f2fs_issue_ckpt,
					cprc->ckpt_thread_ioprio);
			if (ret)
				return ret;
		}

		return count;
	}

	ui = (unsigned int *)(ptr + a->offset);

	ret = kstrtoul(skip_spaces(buf), 0, &t);
	if (ret < 0)
		return ret;
#ifdef CONFIG_F2FS_FAULT_INJECTION
	if (a->struct_type == FAULT_INFO_TYPE && t >= (1 << FAULT_MAX))
		return -EINVAL;
	if (a->struct_type == FAULT_INFO_RATE && t >= UINT_MAX)
		return -EINVAL;
#endif
	if (a->struct_type == RESERVED_BLOCKS) {
		spin_lock(&sbi->stat_lock);
		if (t > (unsigned long)(sbi->user_block_count -
				F2FS_OPTION(sbi).root_reserved_blocks -
				sbi->blocks_per_seg *
				SM_I(sbi)->additional_reserved_segments)) {
			spin_unlock(&sbi->stat_lock);
			return -EINVAL;
		}
		*ui = t;
		sbi->current_reserved_blocks = min(sbi->reserved_blocks,
				sbi->user_block_count - valid_user_blocks(sbi));
		spin_unlock(&sbi->stat_lock);
		return count;
	}

	if (!strcmp(a->attr.name, "discard_granularity")) {
		if (t == 0 || t > MAX_PLIST_NUM)
			return -EINVAL;
		if (!f2fs_block_unit_discard(sbi))
			return -EINVAL;
		if (t == *ui)
			return count;
		*ui = t;
		return count;
	}

	if (!strcmp(a->attr.name, "migration_granularity")) {
		if (t == 0 || t > sbi->segs_per_sec)
			return -EINVAL;
	}

	if (!strcmp(a->attr.name, "trim_sections"))
		return -EINVAL;

	if (!strcmp(a->attr.name, "gc_urgent")) {
		if (t == 0) {
			sbi->gc_mode = GC_NORMAL;
		} else if (t == 1) {
			sbi->gc_mode = GC_URGENT_HIGH;
			if (sbi->gc_thread) {
				sbi->gc_thread->gc_wake = 1;
				wake_up_interruptible_all(
					&sbi->gc_thread->gc_wait_queue_head);
				wake_up_discard_thread(sbi, true);
			}
		} else if (t == 2) {
			sbi->gc_mode = GC_URGENT_LOW;
		} else {
			return -EINVAL;
		}
		return count;
	}
	if (!strcmp(a->attr.name, "gc_idle")) {
		if (t == GC_IDLE_CB) {
			sbi->gc_mode = GC_IDLE_CB;
		} else if (t == GC_IDLE_GREEDY) {
			sbi->gc_mode = GC_IDLE_GREEDY;
		} else if (t == GC_IDLE_AT) {
			if (!sbi->am.atgc_enabled)
				return -EINVAL;
			sbi->gc_mode = GC_AT;
		} else {
			sbi->gc_mode = GC_NORMAL;
		}
		return count;
	}

	if (!strcmp(a->attr.name, "gc_urgent_high_remaining")) {
		spin_lock(&sbi->gc_urgent_high_lock);
		sbi->gc_urgent_high_limited = t != 0;
		sbi->gc_urgent_high_remaining = t;
		spin_unlock(&sbi->gc_urgent_high_lock);

		return count;
	}

#ifdef CONFIG_F2FS_IOSTAT
	if (!strcmp(a->attr.name, "iostat_enable")) {
		sbi->iostat_enable = !!t;
		if (!sbi->iostat_enable)
			f2fs_reset_iostat(sbi);
		return count;
	}

	if (!strcmp(a->attr.name, "iostat_period_ms")) {
		if (t < MIN_IOSTAT_PERIOD_MS || t > MAX_IOSTAT_PERIOD_MS)
			return -EINVAL;
		spin_lock(&sbi->iostat_lock);
		sbi->iostat_period_ms = (unsigned int)t;
		spin_unlock(&sbi->iostat_lock);
		return count;
	}
#endif

#ifdef CONFIG_F2FS_FS_COMPRESSION
	if (!strcmp(a->attr.name, "compr_written_block") ||
		!strcmp(a->attr.name, "compr_saved_block")) {
		if (t != 0)
			return -EINVAL;
		sbi->compr_written_block = 0;
		sbi->compr_saved_block = 0;
		return count;
	}

	if (!strcmp(a->attr.name, "compr_new_inode")) {
		if (t != 0)
			return -EINVAL;
		sbi->compr_new_inode = 0;
		return count;
	}
#endif

	if (!strcmp(a->attr.name, "atgc_candidate_ratio")) {
		if (t > 100)
			return -EINVAL;
		sbi->am.candidate_ratio = t;
		return count;
	}

	if (!strcmp(a->attr.name, "atgc_age_weight")) {
		if (t > 100)
			return -EINVAL;
		sbi->am.age_weight = t;
		return count;
	}

	if (!strcmp(a->attr.name, "gc_segment_mode")) {
		if (t < MAX_GC_MODE)
			sbi->gc_segment_mode = t;
		else
			return -EINVAL;
		return count;
	}

	if (!strcmp(a->attr.name, "gc_reclaimed_segments")) {
		if (t != 0)
			return -EINVAL;
		sbi->gc_reclaimed_segs[sbi->gc_segment_mode] = 0;
		return count;
	}

	if (!strcmp(a->attr.name, "max_fragment_chunk")) {
		if (t >= MIN_FRAGMENT_SIZE && t <= MAX_FRAGMENT_SIZE)
			sbi->max_fragment_chunk = t;
		else
			return -EINVAL;
		return count;
	}

	if (!strcmp(a->attr.name, "max_fragment_hole")) {
		if (t >= MIN_FRAGMENT_SIZE && t <= MAX_FRAGMENT_SIZE)
			sbi->max_fragment_hole = t;
		else
			return -EINVAL;
		return count;
	}

	*ui = (unsigned int)t;

	return count;
}

static ssize_t f2fs_sbi_store(struct f2fs_attr *a,
			struct f2fs_sb_info *sbi,
			const char *buf, size_t count)
{
	ssize_t ret;
	bool gc_entry = (!strcmp(a->attr.name, "gc_urgent") ||
					a->struct_type == GC_THREAD);

	if (gc_entry) {
		if (!down_read_trylock(&sbi->sb->s_umount))
			return -EAGAIN;
	}
	ret = __sbi_store(a, sbi, buf, count);
	if (gc_entry)
		up_read(&sbi->sb->s_umount);

	return ret;
}

static ssize_t f2fs_attr_show(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	struct f2fs_sb_info *sbi = container_of(kobj, struct f2fs_sb_info,
								s_kobj);
	struct f2fs_attr *a = container_of(attr, struct f2fs_attr, attr);

	return a->show ? a->show(a, sbi, buf) : 0;
}

static ssize_t f2fs_attr_store(struct kobject *kobj, struct attribute *attr,
						const char *buf, size_t len)
{
	struct f2fs_sb_info *sbi = container_of(kobj, struct f2fs_sb_info,
									s_kobj);
	struct f2fs_attr *a = container_of(attr, struct f2fs_attr, attr);

	return a->store ? a->store(a, sbi, buf, len) : 0;
}

static void f2fs_sb_release(struct kobject *kobj)
{
	struct f2fs_sb_info *sbi = container_of(kobj, struct f2fs_sb_info,
								s_kobj);
	complete(&sbi->s_kobj_unregister);
}

/*
 * Note that there are three feature list entries:
 * 1) /sys/fs/f2fs/features
 *   : shows runtime features supported by in-kernel f2fs along with Kconfig.
 *     - ref. F2FS_FEATURE_RO_ATTR()
 *
 * 2) /sys/fs/f2fs/$s_id/features <deprecated>
 *   : shows on-disk features enabled by mkfs.f2fs, used for old kernels. This
 *     won't add new feature anymore, and thus, users should check entries in 3)
 *     instead of this 2).
 *
 * 3) /sys/fs/f2fs/$s_id/feature_list
 *   : shows on-disk features enabled by mkfs.f2fs per instance, which follows
 *     sysfs entry rule where each entry should expose single value.
 *     This list covers old feature list provided by 2) and beyond. Therefore,
 *     please add new on-disk feature in this list only.
 *     - ref. F2FS_SB_FEATURE_RO_ATTR()
 */
static ssize_t f2fs_feature_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
	return sprintf(buf, "supported\n");
}

#define F2FS_FEATURE_RO_ATTR(_name)				\
static struct f2fs_attr f2fs_attr_##_name = {			\
	.attr = {.name = __stringify(_name), .mode = 0444 },	\
	.show	= f2fs_feature_show,				\
}

static ssize_t f2fs_sb_feature_show(struct f2fs_attr *a,
		struct f2fs_sb_info *sbi, char *buf)
{
	if (F2FS_HAS_FEATURE(sbi, a->id))
		return sprintf(buf, "supported\n");
	return sprintf(buf, "unsupported\n");
}

#define F2FS_SB_FEATURE_RO_ATTR(_name, _feat)			\
static struct f2fs_attr f2fs_attr_sb_##_name = {		\
	.attr = {.name = __stringify(_name), .mode = 0444 },	\
	.show	= f2fs_sb_feature_show,				\
	.id	= F2FS_FEATURE_##_feat,				\
}

#define F2FS_ATTR_OFFSET(_struct_type, _name, _mode, _show, _store, _offset) \
static struct f2fs_attr f2fs_attr_##_name = {			\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.show	= _show,					\
	.store	= _store,					\
	.struct_type = _struct_type,				\
	.offset = _offset					\
}

#define F2FS_RW_ATTR(struct_type, struct_name, name, elname)	\
	F2FS_ATTR_OFFSET(struct_type, name, 0644,		\
		f2fs_sbi_show, f2fs_sbi_store,			\
		offsetof(struct struct_name, elname))

#define F2FS_RW_ATTR_640(struct_type, struct_name, name, elname)	\
	F2FS_ATTR_OFFSET(struct_type, name, 0640,		\
		f2fs_sbi_show, f2fs_sbi_store,			\
		offsetof(struct struct_name, elname))

#define F2FS_GENERAL_RO_ATTR(name) \
static struct f2fs_attr f2fs_attr_##name = __ATTR(name, 0444, name##_show, NULL)

#define F2FS_STAT_ATTR(_struct_type, _struct_name, _name, _elname)	\
static struct f2fs_attr f2fs_attr_##_name = {			\
	.attr = {.name = __stringify(_name), .mode = 0444 },	\
	.show = f2fs_sbi_show,					\
	.struct_type = _struct_type,				\
	.offset = offsetof(struct _struct_name, _elname),       \
}

F2FS_RW_ATTR(GC_THREAD, f2fs_gc_kthread, gc_urgent_sleep_time,
							urgent_sleep_time);
F2FS_RW_ATTR(GC_THREAD, f2fs_gc_kthread, gc_min_sleep_time, min_sleep_time);
F2FS_RW_ATTR(GC_THREAD, f2fs_gc_kthread, gc_max_sleep_time, max_sleep_time);
F2FS_RW_ATTR(GC_THREAD, f2fs_gc_kthread, gc_no_gc_sleep_time, no_gc_sleep_time);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, gc_idle, gc_mode);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, gc_urgent, gc_mode);
F2FS_RW_ATTR(SM_INFO, f2fs_sm_info, reclaim_segments, rec_prefree_segments);
F2FS_RW_ATTR(DCC_INFO, discard_cmd_control, max_small_discards, max_discards);
F2FS_RW_ATTR(DCC_INFO, discard_cmd_control, discard_granularity, discard_granularity);
F2FS_RW_ATTR(RESERVED_BLOCKS, f2fs_sb_info, reserved_blocks, reserved_blocks);
F2FS_RW_ATTR(SM_INFO, f2fs_sm_info, batched_trim_sections, trim_sections);
F2FS_RW_ATTR(SM_INFO, f2fs_sm_info, ipu_policy, ipu_policy);
F2FS_RW_ATTR(SM_INFO, f2fs_sm_info, min_ipu_util, min_ipu_util);
F2FS_RW_ATTR(SM_INFO, f2fs_sm_info, min_fsync_blocks, min_fsync_blocks);
F2FS_RW_ATTR(SM_INFO, f2fs_sm_info, min_seq_blocks, min_seq_blocks);
F2FS_RW_ATTR(SM_INFO, f2fs_sm_info, min_hot_blocks, min_hot_blocks);
F2FS_RW_ATTR(SM_INFO, f2fs_sm_info, min_ssr_sections, min_ssr_sections);
F2FS_RW_ATTR(NM_INFO, f2fs_nm_info, ram_thresh, ram_thresh);
F2FS_RW_ATTR(NM_INFO, f2fs_nm_info, ra_nid_pages, ra_nid_pages);
F2FS_RW_ATTR(NM_INFO, f2fs_nm_info, dirty_nats_ratio, dirty_nats_ratio);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, max_victim_search, max_victim_search);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, migration_granularity, migration_granularity);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, dir_level, dir_level);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, cp_interval, interval_time[CP_TIME]);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, idle_interval, interval_time[REQ_TIME]);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, discard_idle_interval,
					interval_time[DISCARD_TIME]);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, gc_idle_interval, interval_time[GC_TIME]);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info,
		umount_discard_timeout, interval_time[UMOUNT_DISCARD_TIMEOUT]);
#ifdef CONFIG_F2FS_IOSTAT
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, iostat_enable, iostat_enable);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, iostat_period_ms, iostat_period_ms);
#endif
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, readdir_ra, readdir_ra);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, max_io_bytes, max_io_bytes);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, gc_pin_file_thresh, gc_pin_file_threshold);
F2FS_RW_ATTR(F2FS_SBI, f2fs_super_block, extension_list, extension_list);
#ifdef CONFIG_F2FS_FAULT_INJECTION
F2FS_RW_ATTR(FAULT_INFO_RATE, f2fs_fault_info, inject_rate, inject_rate);
F2FS_RW_ATTR(FAULT_INFO_TYPE, f2fs_fault_info, inject_type, inject_type);
#endif
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, data_io_flag, data_io_flag);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, node_io_flag, node_io_flag);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, gc_urgent_high_remaining, gc_urgent_high_remaining);
F2FS_RW_ATTR(CPRC_INFO, ckpt_req_control, ckpt_thread_ioprio, ckpt_thread_ioprio);
F2FS_RW_ATTR_640(F2FS_SBI, f2fs_sb_info, sec_gc_stat, sec_stat);
F2FS_RW_ATTR_640(F2FS_SBI, f2fs_sb_info, sec_io_stat, sec_stat);
#ifdef CONFIG_F2FS_SEC_DEBUG_NODE
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, sec_stats, stat_info);
#endif
F2FS_RW_ATTR_640(F2FS_SBI, f2fs_sb_info, sec_fsck_stat, sec_fsck_stat);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, sec_heimdallfs_stat, sec_heimdallfs_stat);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, sec_part_best_extents, s_sec_part_best_extents);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, sec_part_current_extents, s_sec_part_current_extents);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, sec_part_score, s_sec_part_score);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, sec_defrag_writes_kb, s_sec_defrag_writes_kb);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, sec_num_apps, s_sec_num_apps);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, sec_capacity_apps_kb, s_sec_capacity_apps_kb);
F2FS_RW_ATTR_640(F2FS_SBI, f2fs_sb_info, sec_defrag_stat, s_sec_part_best_extents);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, sec_fua_mode, s_sec_cond_fua_mode);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, sec_hqm_preserve, sec_hqm_preserve);
F2FS_GENERAL_RO_ATTR(dirty_segments);
F2FS_GENERAL_RO_ATTR(free_segments);
F2FS_GENERAL_RO_ATTR(ovp_segments);
F2FS_GENERAL_RO_ATTR(lifetime_write_kbytes);
F2FS_GENERAL_RO_ATTR(sec_fs_stat);
F2FS_GENERAL_RO_ATTR(features);
F2FS_GENERAL_RO_ATTR(current_reserved_blocks);
F2FS_GENERAL_RO_ATTR(unusable);
F2FS_GENERAL_RO_ATTR(encoding);
F2FS_GENERAL_RO_ATTR(mounted_time_sec);
F2FS_GENERAL_RO_ATTR(main_blkaddr);
F2FS_GENERAL_RO_ATTR(pending_discard);
#ifdef CONFIG_F2FS_STAT_FS
F2FS_STAT_ATTR(STAT_INFO, f2fs_stat_info, cp_foreground_calls, cp_count);
F2FS_STAT_ATTR(STAT_INFO, f2fs_stat_info, cp_background_calls, bg_cp_count);
F2FS_STAT_ATTR(STAT_INFO, f2fs_stat_info, gc_foreground_calls, call_count);
F2FS_STAT_ATTR(STAT_INFO, f2fs_stat_info, gc_background_calls, bg_gc);
F2FS_GENERAL_RO_ATTR(moved_blocks_background);
F2FS_GENERAL_RO_ATTR(moved_blocks_foreground);
F2FS_GENERAL_RO_ATTR(avg_vblocks);
#endif

#ifdef CONFIG_FS_ENCRYPTION
F2FS_FEATURE_RO_ATTR(encryption);
F2FS_FEATURE_RO_ATTR(test_dummy_encryption_v2);
#ifdef CONFIG_UNICODE
F2FS_FEATURE_RO_ATTR(encrypted_casefold);
#endif
#endif /* CONFIG_FS_ENCRYPTION */
#ifdef CONFIG_BLK_DEV_ZONED
F2FS_FEATURE_RO_ATTR(block_zoned);
#endif
F2FS_FEATURE_RO_ATTR(atomic_write);
F2FS_FEATURE_RO_ATTR(extra_attr);
F2FS_FEATURE_RO_ATTR(project_quota);
F2FS_FEATURE_RO_ATTR(inode_checksum);
F2FS_FEATURE_RO_ATTR(flexible_inline_xattr);
F2FS_FEATURE_RO_ATTR(quota_ino);
F2FS_FEATURE_RO_ATTR(inode_crtime);
F2FS_FEATURE_RO_ATTR(lost_found);
#ifdef CONFIG_FS_VERITY
F2FS_FEATURE_RO_ATTR(verity);
#endif
F2FS_FEATURE_RO_ATTR(sb_checksum);
#ifdef CONFIG_UNICODE
F2FS_FEATURE_RO_ATTR(casefold);
#endif
F2FS_FEATURE_RO_ATTR(readonly);
#ifdef CONFIG_F2FS_FS_COMPRESSION
F2FS_FEATURE_RO_ATTR(compression);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, compr_written_block, compr_written_block);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, compr_saved_block, compr_saved_block);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, compr_new_inode, compr_new_inode);
#endif
F2FS_FEATURE_RO_ATTR(pin_file);

/* For ATGC */
F2FS_RW_ATTR(ATGC_INFO, atgc_management, atgc_candidate_ratio, candidate_ratio);
F2FS_RW_ATTR(ATGC_INFO, atgc_management, atgc_candidate_count, max_candidate_count);
F2FS_RW_ATTR(ATGC_INFO, atgc_management, atgc_age_weight, age_weight);
F2FS_RW_ATTR(ATGC_INFO, atgc_management, atgc_age_threshold, age_threshold);

F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, gc_segment_mode, gc_segment_mode);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, gc_reclaimed_segments, gc_reclaimed_segs);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, max_fragment_chunk, max_fragment_chunk);
F2FS_RW_ATTR(F2FS_SBI, f2fs_sb_info, max_fragment_hole, max_fragment_hole);

#define ATTR_LIST(name) (&f2fs_attr_##name.attr)
static struct attribute *f2fs_attrs[] = {
	ATTR_LIST(gc_urgent_sleep_time),
	ATTR_LIST(gc_min_sleep_time),
	ATTR_LIST(gc_max_sleep_time),
	ATTR_LIST(gc_no_gc_sleep_time),
	ATTR_LIST(gc_idle),
	ATTR_LIST(gc_urgent),
	ATTR_LIST(reclaim_segments),
	ATTR_LIST(main_blkaddr),
	ATTR_LIST(max_small_discards),
	ATTR_LIST(discard_granularity),
	ATTR_LIST(pending_discard),
	ATTR_LIST(batched_trim_sections),
	ATTR_LIST(ipu_policy),
	ATTR_LIST(min_ipu_util),
	ATTR_LIST(min_fsync_blocks),
	ATTR_LIST(min_seq_blocks),
	ATTR_LIST(min_hot_blocks),
	ATTR_LIST(min_ssr_sections),
	ATTR_LIST(max_victim_search),
	ATTR_LIST(migration_granularity),
	ATTR_LIST(dir_level),
	ATTR_LIST(ram_thresh),
	ATTR_LIST(ra_nid_pages),
	ATTR_LIST(dirty_nats_ratio),
	ATTR_LIST(cp_interval),
	ATTR_LIST(idle_interval),
	ATTR_LIST(discard_idle_interval),
	ATTR_LIST(gc_idle_interval),
	ATTR_LIST(umount_discard_timeout),
#ifdef CONFIG_F2FS_IOSTAT
	ATTR_LIST(iostat_enable),
	ATTR_LIST(iostat_period_ms),
#endif
	ATTR_LIST(readdir_ra),
	ATTR_LIST(max_io_bytes),
	ATTR_LIST(gc_pin_file_thresh),
	ATTR_LIST(extension_list),
	ATTR_LIST(sec_gc_stat),
	ATTR_LIST(sec_io_stat),
#ifdef CONFIG_F2FS_SEC_DEBUG_NODE
	ATTR_LIST(sec_stats),
#endif
	ATTR_LIST(sec_fsck_stat),
	ATTR_LIST(sec_heimdallfs_stat),
	ATTR_LIST(sec_part_best_extents),
	ATTR_LIST(sec_part_current_extents),
	ATTR_LIST(sec_part_score),
	ATTR_LIST(sec_defrag_writes_kb),
	ATTR_LIST(sec_num_apps),
	ATTR_LIST(sec_capacity_apps_kb),
	ATTR_LIST(sec_defrag_stat),
	ATTR_LIST(sec_fua_mode),
	ATTR_LIST(sec_hqm_preserve),
#ifdef CONFIG_F2FS_FAULT_INJECTION
	ATTR_LIST(inject_rate),
	ATTR_LIST(inject_type),
#endif
	ATTR_LIST(data_io_flag),
	ATTR_LIST(node_io_flag),
	ATTR_LIST(gc_urgent_high_remaining),
	ATTR_LIST(ckpt_thread_ioprio),
	ATTR_LIST(dirty_segments),
	ATTR_LIST(free_segments),
	ATTR_LIST(ovp_segments),
	ATTR_LIST(unusable),
	ATTR_LIST(lifetime_write_kbytes),
	ATTR_LIST(sec_fs_stat),
	ATTR_LIST(features),
	ATTR_LIST(reserved_blocks),
	ATTR_LIST(current_reserved_blocks),
	ATTR_LIST(encoding),
	ATTR_LIST(mounted_time_sec),
#ifdef CONFIG_F2FS_STAT_FS
	ATTR_LIST(cp_foreground_calls),
	ATTR_LIST(cp_background_calls),
	ATTR_LIST(gc_foreground_calls),
	ATTR_LIST(gc_background_calls),
	ATTR_LIST(moved_blocks_foreground),
	ATTR_LIST(moved_blocks_background),
	ATTR_LIST(avg_vblocks),
#endif
#ifdef CONFIG_F2FS_FS_COMPRESSION
	ATTR_LIST(compr_written_block),
	ATTR_LIST(compr_saved_block),
	ATTR_LIST(compr_new_inode),
#endif
	/* For ATGC */
	ATTR_LIST(atgc_candidate_ratio),
	ATTR_LIST(atgc_candidate_count),
	ATTR_LIST(atgc_age_weight),
	ATTR_LIST(atgc_age_threshold),
	ATTR_LIST(gc_segment_mode),
	ATTR_LIST(gc_reclaimed_segments),
	ATTR_LIST(max_fragment_chunk),
	ATTR_LIST(max_fragment_hole),
	NULL,
};

static struct attribute *f2fs_feat_attrs[] = {
#ifdef CONFIG_FS_ENCRYPTION
	ATTR_LIST(encryption),
	ATTR_LIST(test_dummy_encryption_v2),
#ifdef CONFIG_UNICODE
	ATTR_LIST(encrypted_casefold),
#endif
#endif /* CONFIG_FS_ENCRYPTION */
#ifdef CONFIG_BLK_DEV_ZONED
	ATTR_LIST(block_zoned),
#endif
	ATTR_LIST(atomic_write),
	ATTR_LIST(extra_attr),
	ATTR_LIST(project_quota),
	ATTR_LIST(inode_checksum),
	ATTR_LIST(flexible_inline_xattr),
	ATTR_LIST(quota_ino),
	ATTR_LIST(inode_crtime),
	ATTR_LIST(lost_found),
#ifdef CONFIG_FS_VERITY
	ATTR_LIST(verity),
#endif
	ATTR_LIST(sb_checksum),
#ifdef CONFIG_UNICODE
	ATTR_LIST(casefold),
#endif
	ATTR_LIST(readonly),
#ifdef CONFIG_F2FS_FS_COMPRESSION
	ATTR_LIST(compression),
#endif
	ATTR_LIST(pin_file),
	NULL,
};

F2FS_GENERAL_RO_ATTR(sb_status);
static struct attribute *f2fs_stat_attrs[] = {
	ATTR_LIST(sb_status),
	NULL,
};

F2FS_SB_FEATURE_RO_ATTR(encryption, ENCRYPT);
F2FS_SB_FEATURE_RO_ATTR(block_zoned, BLKZONED);
F2FS_SB_FEATURE_RO_ATTR(extra_attr, EXTRA_ATTR);
F2FS_SB_FEATURE_RO_ATTR(project_quota, PRJQUOTA);
F2FS_SB_FEATURE_RO_ATTR(inode_checksum, INODE_CHKSUM);
F2FS_SB_FEATURE_RO_ATTR(flexible_inline_xattr, FLEXIBLE_INLINE_XATTR);
F2FS_SB_FEATURE_RO_ATTR(quota_ino, QUOTA_INO);
F2FS_SB_FEATURE_RO_ATTR(inode_crtime, INODE_CRTIME);
F2FS_SB_FEATURE_RO_ATTR(lost_found, LOST_FOUND);
F2FS_SB_FEATURE_RO_ATTR(verity, VERITY);
F2FS_SB_FEATURE_RO_ATTR(sb_checksum, SB_CHKSUM);
F2FS_SB_FEATURE_RO_ATTR(casefold, CASEFOLD);
F2FS_SB_FEATURE_RO_ATTR(compression, COMPRESSION);
F2FS_SB_FEATURE_RO_ATTR(readonly, RO);

static struct attribute *f2fs_sb_feat_attrs[] = {
	ATTR_LIST(sb_encryption),
	ATTR_LIST(sb_block_zoned),
	ATTR_LIST(sb_extra_attr),
	ATTR_LIST(sb_project_quota),
	ATTR_LIST(sb_inode_checksum),
	ATTR_LIST(sb_flexible_inline_xattr),
	ATTR_LIST(sb_quota_ino),
	ATTR_LIST(sb_inode_crtime),
	ATTR_LIST(sb_lost_found),
	ATTR_LIST(sb_verity),
	ATTR_LIST(sb_sb_checksum),
	ATTR_LIST(sb_casefold),
	ATTR_LIST(sb_compression),
	ATTR_LIST(sb_readonly),
	NULL,
};

static const struct sysfs_ops f2fs_attr_ops = {
	.show	= f2fs_attr_show,
	.store	= f2fs_attr_store,
};

static struct kobj_type f2fs_sb_ktype = {
	.default_attrs	= f2fs_attrs,
	.sysfs_ops	= &f2fs_attr_ops,
	.release	= f2fs_sb_release,
};

static struct kobj_type f2fs_ktype = {
	.sysfs_ops	= &f2fs_attr_ops,
};

static struct kset f2fs_kset = {
	.kobj	= {.ktype = &f2fs_ktype},
};

static struct kobj_type f2fs_feat_ktype = {
	.default_attrs	= f2fs_feat_attrs,
	.sysfs_ops	= &f2fs_attr_ops,
};

static struct kobject f2fs_feat = {
	.kset	= &f2fs_kset,
};

static ssize_t f2fs_stat_attr_show(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	struct f2fs_sb_info *sbi = container_of(kobj, struct f2fs_sb_info,
								s_stat_kobj);
	struct f2fs_attr *a = container_of(attr, struct f2fs_attr, attr);

	return a->show ? a->show(a, sbi, buf) : 0;
}

static ssize_t f2fs_stat_attr_store(struct kobject *kobj, struct attribute *attr,
						const char *buf, size_t len)
{
	struct f2fs_sb_info *sbi = container_of(kobj, struct f2fs_sb_info,
								s_stat_kobj);
	struct f2fs_attr *a = container_of(attr, struct f2fs_attr, attr);

	return a->store ? a->store(a, sbi, buf, len) : 0;
}

static void f2fs_stat_kobj_release(struct kobject *kobj)
{
	struct f2fs_sb_info *sbi = container_of(kobj, struct f2fs_sb_info,
								s_stat_kobj);
	complete(&sbi->s_stat_kobj_unregister);
}

static const struct sysfs_ops f2fs_stat_attr_ops = {
	.show	= f2fs_stat_attr_show,
	.store	= f2fs_stat_attr_store,
};

static struct kobj_type f2fs_stat_ktype = {
	.default_attrs	= f2fs_stat_attrs,
	.sysfs_ops	= &f2fs_stat_attr_ops,
	.release	= f2fs_stat_kobj_release,
};

static ssize_t f2fs_sb_feat_attr_show(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	struct f2fs_sb_info *sbi = container_of(kobj, struct f2fs_sb_info,
							s_feature_list_kobj);
	struct f2fs_attr *a = container_of(attr, struct f2fs_attr, attr);

	return a->show ? a->show(a, sbi, buf) : 0;
}

static void f2fs_feature_list_kobj_release(struct kobject *kobj)
{
	struct f2fs_sb_info *sbi = container_of(kobj, struct f2fs_sb_info,
							s_feature_list_kobj);
	complete(&sbi->s_feature_list_kobj_unregister);
}

static const struct sysfs_ops f2fs_feature_list_attr_ops = {
	.show	= f2fs_sb_feat_attr_show,
};

static struct kobj_type f2fs_feature_list_ktype = {
	.default_attrs = f2fs_sb_feat_attrs,
	.sysfs_ops	= &f2fs_feature_list_attr_ops,
	.release	= f2fs_feature_list_kobj_release,
};

static int __maybe_unused segment_info_seq_show(struct seq_file *seq,
						void *offset)
{
	struct super_block *sb = seq->private;
	struct f2fs_sb_info *sbi = F2FS_SB(sb);
	unsigned int total_segs =
			le32_to_cpu(sbi->raw_super->segment_count_main);
	int i;

	seq_puts(seq, "format: segment_type|valid_blocks\n"
		"segment_type(0:HD, 1:WD, 2:CD, 3:HN, 4:WN, 5:CN)\n");

	for (i = 0; i < total_segs; i++) {
		struct seg_entry *se = get_seg_entry(sbi, i);

		if ((i % 10) == 0)
			seq_printf(seq, "%-10d", i);
		seq_printf(seq, "%d|%-3u", se->type, se->valid_blocks);
		if ((i % 10) == 9 || i == (total_segs - 1))
			seq_putc(seq, '\n');
		else
			seq_putc(seq, ' ');
	}

	return 0;
}

static int __maybe_unused segment_bits_seq_show(struct seq_file *seq,
						void *offset)
{
	struct super_block *sb = seq->private;
	struct f2fs_sb_info *sbi = F2FS_SB(sb);
	unsigned int total_segs =
			le32_to_cpu(sbi->raw_super->segment_count_main);
	int i, j;

	seq_puts(seq, "format: segment_type|valid_blocks|bitmaps\n"
		"segment_type(0:HD, 1:WD, 2:CD, 3:HN, 4:WN, 5:CN)\n");

	for (i = 0; i < total_segs; i++) {
		struct seg_entry *se = get_seg_entry(sbi, i);

		seq_printf(seq, "%-10d", i);
		seq_printf(seq, "%d|%-3u|", se->type, se->valid_blocks);
		for (j = 0; j < SIT_VBLOCK_MAP_SIZE; j++)
			seq_printf(seq, " %.2x", se->cur_valid_map[j]);
		seq_putc(seq, '\n');
	}
	return 0;
}

static int __maybe_unused victim_bits_seq_show(struct seq_file *seq,
						void *offset)
{
	struct super_block *sb = seq->private;
	struct f2fs_sb_info *sbi = F2FS_SB(sb);
	struct dirty_seglist_info *dirty_i = DIRTY_I(sbi);
	int i;

	seq_puts(seq, "format: victim_secmap bitmaps\n");

	for (i = 0; i < MAIN_SECS(sbi); i++) {
		if ((i % 10) == 0)
			seq_printf(seq, "%-10d", i);
		seq_printf(seq, "%d", test_bit(i, dirty_i->victim_secmap) ? 1 : 0);
		if ((i % 10) == 9 || i == (MAIN_SECS(sbi) - 1))
			seq_putc(seq, '\n');
		else
			seq_putc(seq, ' ');
	}
	return 0;
}

int __init f2fs_init_sysfs(void)
{
	int ret;

	kobject_set_name(&f2fs_kset.kobj, "f2fs");
	f2fs_kset.kobj.parent = fs_kobj;
	ret = kset_register(&f2fs_kset);
	if (ret)
		return ret;

	ret = kobject_init_and_add(&f2fs_feat, &f2fs_feat_ktype,
				   NULL, "features");
	if (ret) {
		kobject_put(&f2fs_feat);
		kset_unregister(&f2fs_kset);
	} else {
		f2fs_proc_root = proc_mkdir("fs/f2fs", NULL);
	}
	return ret;
}

void f2fs_exit_sysfs(void)
{
	kobject_put(&f2fs_feat);
	kset_unregister(&f2fs_kset);
	remove_proc_entry("fs/f2fs", NULL);
	f2fs_proc_root = NULL;
}

#define SEC_MAX_VOLUME_NAME	16
static bool __volume_is_userdata(struct f2fs_sb_info *sbi)
{
	char volume_name[SEC_MAX_VOLUME_NAME] = {0, };

	utf16s_to_utf8s(sbi->raw_super->volume_name, SEC_MAX_VOLUME_NAME,
			UTF16_LITTLE_ENDIAN, volume_name, SEC_MAX_VOLUME_NAME);
	volume_name[SEC_MAX_VOLUME_NAME - 1] = '\0';

	if (!strcmp(volume_name, "data"))
		return true;

	return false;
}

int f2fs_register_sysfs(struct f2fs_sb_info *sbi)
{
	struct super_block *sb = sbi->sb;
	int err;

	sbi->s_kobj.kset = &f2fs_kset;
	init_completion(&sbi->s_kobj_unregister);
	err = kobject_init_and_add(&sbi->s_kobj, &f2fs_sb_ktype, NULL,
				"%s", sb->s_id);
	if (err)
		goto put_sb_kobj;

	sbi->s_stat_kobj.kset = &f2fs_kset;
	init_completion(&sbi->s_stat_kobj_unregister);
	err = kobject_init_and_add(&sbi->s_stat_kobj, &f2fs_stat_ktype,
						&sbi->s_kobj, "stat");
	if (err)
		goto put_stat_kobj;

	sbi->s_feature_list_kobj.kset = &f2fs_kset;
	init_completion(&sbi->s_feature_list_kobj_unregister);
	err = kobject_init_and_add(&sbi->s_feature_list_kobj,
					&f2fs_feature_list_ktype,
					&sbi->s_kobj, "feature_list");
	if (err)
		goto put_feature_list_kobj;

	if (__volume_is_userdata(sbi)) {
		err = sysfs_create_link(&f2fs_kset.kobj, &sbi->s_kobj,
				"userdata");
		if (err)
			pr_err("Can not create sysfs link for userdata(%d)\n",
					err);
	}

	if (f2fs_proc_root)
		sbi->s_proc = proc_mkdir(sb->s_id, f2fs_proc_root);

	if (sbi->s_proc) {
		proc_create_single_data("segment_info", 0444, sbi->s_proc,
				segment_info_seq_show, sb);
		proc_create_single_data("segment_bits", 0444, sbi->s_proc,
				segment_bits_seq_show, sb);
#ifdef CONFIG_F2FS_IOSTAT
		proc_create_single_data("iostat_info", 0444, sbi->s_proc,
				iostat_info_seq_show, sb);
#endif
		proc_create_single_data("victim_bits", 0444, sbi->s_proc,
				victim_bits_seq_show, sb);
	}
	return 0;
put_feature_list_kobj:
	kobject_put(&sbi->s_feature_list_kobj);
	wait_for_completion(&sbi->s_feature_list_kobj_unregister);
put_stat_kobj:
	kobject_put(&sbi->s_stat_kobj);
	wait_for_completion(&sbi->s_stat_kobj_unregister);
put_sb_kobj:
	kobject_put(&sbi->s_kobj);
	wait_for_completion(&sbi->s_kobj_unregister);
	return err;
}

void f2fs_unregister_sysfs(struct f2fs_sb_info *sbi)
{
	if (sbi->s_proc) {
#ifdef CONFIG_F2FS_IOSTAT
		remove_proc_entry("iostat_info", sbi->s_proc);
#endif
		remove_proc_entry("segment_info", sbi->s_proc);
		remove_proc_entry("segment_bits", sbi->s_proc);
		remove_proc_entry("victim_bits", sbi->s_proc);
		remove_proc_entry(sbi->sb->s_id, f2fs_proc_root);
	}

	if (__volume_is_userdata(sbi))
		sysfs_delete_link(&f2fs_kset.kobj, &sbi->s_kobj, "userdata");

	kobject_del(&sbi->s_stat_kobj);
	kobject_put(&sbi->s_stat_kobj);
	wait_for_completion(&sbi->s_stat_kobj_unregister);
	kobject_del(&sbi->s_feature_list_kobj);
	kobject_put(&sbi->s_feature_list_kobj);
	wait_for_completion(&sbi->s_feature_list_kobj_unregister);

	kobject_del(&sbi->s_kobj);
	kobject_put(&sbi->s_kobj);
	wait_for_completion(&sbi->s_kobj_unregister);
}
