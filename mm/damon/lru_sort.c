// SPDX-License-Identifier: GPL-2.0
/*
 * DAMON-based LRU-lists Sorting
 *
 * Author: SeongJae Park <sj@kernel.org>
 */

#define pr_fmt(fmt) "damon-lru-sort: " fmt

#include <linux/damon.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

#include "modules-common.h"

#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX "damon_lru_sort."

/*
 * Enable or disable DAMON_LRU_SORT.
 *
 * You can enable DAMON_LRU_SORT by setting the value of this parameter as
 * ``Y``.  Setting it as ``N`` disables DAMON_LRU_SORT.  Note that
 * DAMON_LRU_SORT could do no real monitoring and LRU-lists sorting due to the
 * watermarks-based activation condition.  Refer to below descriptions for the
 * watermarks parameter for this.
 */
static bool enabled __read_mostly;

/*
 * Make DAMON_LRU_SORT reads the input parameters again, except ``enabled``.
 *
 * Input parameters that updated while DAMON_LRU_SORT is running are not
 * applied by default.  Once this parameter is set as ``Y``, DAMON_LRU_SORT
 * reads values of parametrs except ``enabled`` again.  Once the re-reading is
 * done, this parameter is set as ``N``.  If invalid parameters are found while
 * the re-reading, DAMON_LRU_SORT will be disabled.
 */
static bool commit_inputs __read_mostly;
module_param(commit_inputs, bool, 0600);

/*
 * Access frequency threshold for hot memory regions identification in permil.
 *
 * If a memory region is accessed in frequency of this or higher,
 * DAMON_LRU_SORT identifies the region as hot, and mark it as accessed on the
 * LRU list, so that it could not be reclaimed under memory pressure.  50% by
 * default.
 */
static unsigned long hot_thres_access_freq = 500;
module_param(hot_thres_access_freq, ulong, 0600);

/*
 * Time threshold for cold memory regions identification in microseconds.
 *
 * If a memory region is not accessed for this or longer time, DAMON_LRU_SORT
 * identifies the region as cold, and mark it as unaccessed on the LRU list, so
 * that it could be reclaimed first under memory pressure.  120 seconds by
 * default.
 */
static unsigned long cold_min_age __read_mostly = 120000000;
module_param(cold_min_age, ulong, 0600);

/*
 * Limit of time for trying the LRU lists sorting in milliseconds.
 *
 * DAMON_LRU_SORT tries to use only up to this time within a time window
 * (quota_reset_interval_ms) for trying LRU lists sorting.  This can be used
 * for limiting CPU consumption of DAMON_LRU_SORT.  If the value is zero, the
 * limit is disabled.
 *
 * 10 ms by default.
 */
static unsigned long quota_ms __read_mostly = 10;
module_param(quota_ms, ulong, 0600);

/*
 * The time quota charge reset interval in milliseconds.
 *
 * The charge reset interval for the quota of time (quota_ms).  That is,
 * DAMON_LRU_SORT does not try LRU-lists sorting for more than quota_ms
 * milliseconds or quota_sz bytes within quota_reset_interval_ms milliseconds.
 *
 * 1 second by default.
 */
static unsigned long quota_reset_interval_ms __read_mostly = 1000;
module_param(quota_reset_interval_ms, ulong, 0600);

struct damos_watermarks damon_lru_sort_wmarks = {
	.metric = DAMOS_WMARK_FREE_MEM_RATE,
	.interval = 5000000,	/* 5 seconds */
	.high = 200,		/* 20 percent */
	.mid = 150,		/* 15 percent */
	.low = 50,		/* 5 percent */
};
DEFINE_DAMON_MODULES_WMARKS_PARAMS(damon_lru_sort_wmarks);

static struct damon_attrs damon_lru_sort_mon_attrs = {
	.sample_interval = 5000,	/* 5 ms */
	.aggr_interval = 100000,	/* 100 ms */
	.ops_update_interval = 0,
	.min_nr_regions = 10,
	.max_nr_regions = 1000,
};
DEFINE_DAMON_MODULES_MON_ATTRS_PARAMS(damon_lru_sort_mon_attrs);

/*
 * Start of the target memory region in physical address.
 *
 * The start physical address of memory region that DAMON_LRU_SORT will do work
 * against.  By default, biggest System RAM is used as the region.
 */
static unsigned long monitor_region_start __read_mostly;
module_param(monitor_region_start, ulong, 0600);

/*
 * End of the target memory region in physical address.
 *
 * The end physical address of memory region that DAMON_LRU_SORT will do work
 * against.  By default, biggest System RAM is used as the region.
 */
static unsigned long monitor_region_end __read_mostly;
module_param(monitor_region_end, ulong, 0600);

/*
 * PID of the DAMON thread
 *
 * If DAMON_LRU_SORT is enabled, this becomes the PID of the worker thread.
 * Else, -1.
 */
static int kdamond_pid __read_mostly = -1;
module_param(kdamond_pid, int, 0400);

static struct damos_stat damon_lru_sort_hot_stat;
DEFINE_DAMON_MODULES_DAMOS_STATS_PARAMS(damon_lru_sort_hot_stat,
		lru_sort_tried_hot_regions, lru_sorted_hot_regions,
		hot_quota_exceeds);

static struct damos_stat damon_lru_sort_cold_stat;
DEFINE_DAMON_MODULES_DAMOS_STATS_PARAMS(damon_lru_sort_cold_stat,
		lru_sort_tried_cold_regions, lru_sorted_cold_regions,
		cold_quota_exceeds);

static struct damon_ctx *ctx;
static struct damon_target *target;

/* Create a DAMON-based operation scheme for hot memory regions */
static struct damos *damon_lru_sort_new_hot_scheme(unsigned int hot_thres)
{
	struct damos_access_pattern pattern = {
		/* Find regions having PAGE_SIZE or larger size */
		.min_sz_region = PAGE_SIZE,
		.max_sz_region = ULONG_MAX,
		/* and accessed for more than the threshold */
		.min_nr_accesses = hot_thres,
		.max_nr_accesses = UINT_MAX,
		/* no matter its age */
		.min_age_region = 0,
		.max_age_region = UINT_MAX,
	};
	struct damos_quota quota = {
		/*
		 * Do not try LRU-lists sorting of hot pages for more than half
		 * of quota_ms milliseconds within quota_reset_interval_ms.
		 */
		.ms = quota_ms / 2,
		.sz = 0,
		.reset_interval = quota_reset_interval_ms,
		/* Within the quota, mark hotter regions accessed first. */
		.weight_sz = 0,
		.weight_nr_accesses = 1,
		.weight_age = 0,
	};

	return damon_new_scheme(
			&pattern,
			/* prioritize those on LRU lists, as soon as found */
			DAMOS_LRU_PRIO,
			/* under the quota. */
			&quota,
			/* (De)activate this according to the watermarks. */
			&damon_lru_sort_wmarks);
}

/* Create a DAMON-based operation scheme for cold memory regions */
static struct damos *damon_lru_sort_new_cold_scheme(unsigned int cold_thres)
{
	struct damos_access_pattern pattern = {
		/* Find regions having PAGE_SIZE or larger size */
		.min_sz_region = PAGE_SIZE,
		.max_sz_region = ULONG_MAX,
		/* and not accessed at all */
		.min_nr_accesses = 0,
		.max_nr_accesses = 0,
		/* for min_age or more micro-seconds */
		.min_age_region = cold_thres,
		.max_age_region = UINT_MAX,
	};
	struct damos_quota quota = {
		/*
		 * Do not try LRU-lists sorting of cold pages for more than
		 * half of quota_ms milliseconds within
		 * quota_reset_interval_ms.
		 */
		.ms = quota_ms / 2,
		.sz = 0,
		.reset_interval = quota_reset_interval_ms,
		/* Within the quota, mark colder regions not accessed first. */
		.weight_sz = 0,
		.weight_nr_accesses = 0,
		.weight_age = 1,
	};

	return damon_new_scheme(
			&pattern,
			/* mark those as not accessed, as soon as found */
			DAMOS_LRU_DEPRIO,
			/* under the quota. */
			&quota,
			/* (De)activate this according to the watermarks. */
			&damon_lru_sort_wmarks);
}

static int damon_lru_sort_apply_parameters(void)
{
	struct damos *scheme;
	struct damon_addr_range addr_range;
	unsigned int hot_thres, cold_thres;
	int err = 0;

	err = damon_set_attrs(ctx, &damon_lru_sort_mon_attrs);
	if (err)
		return err;

	/* aggr_interval / sample_interval is the maximum nr_accesses */
	hot_thres = damon_lru_sort_mon_attrs.aggr_interval /
		damon_lru_sort_mon_attrs.sample_interval *
		hot_thres_access_freq / 1000;
	scheme = damon_lru_sort_new_hot_scheme(hot_thres);
	if (!scheme)
		return -ENOMEM;
	err = damon_set_schemes(ctx, &scheme, 1);
	if (err)
		return err;

	cold_thres = cold_min_age / damon_lru_sort_mon_attrs.aggr_interval;
	scheme = damon_lru_sort_new_cold_scheme(cold_thres);
	if (!scheme)
		return -ENOMEM;
	damon_add_scheme(ctx, scheme);

	if (monitor_region_start > monitor_region_end)
		return -EINVAL;
	if (!monitor_region_start && !monitor_region_end &&
	    !damon_find_biggest_system_ram(&monitor_region_start,
					   &monitor_region_end))
		return -EINVAL;
	addr_range.start = monitor_region_start;
	addr_range.end = monitor_region_end;
	return damon_set_regions(target, &addr_range, 1);
}

static int damon_lru_sort_turn(bool on)
{
	int err;

	if (!on) {
		err = damon_stop(&ctx, 1);
		if (!err)
			kdamond_pid = -1;
		return err;
	}

	err = damon_lru_sort_apply_parameters();
	if (err)
		return err;

	err = damon_start(&ctx, 1, true);
	if (err)
		return err;
	kdamond_pid = ctx->kdamond->pid;
	return 0;
}

static struct delayed_work damon_lru_sort_timer;
static void damon_lru_sort_timer_fn(struct work_struct *work)
{
	static bool last_enabled;
	bool now_enabled;

	now_enabled = enabled;
	if (last_enabled != now_enabled) {
		if (!damon_lru_sort_turn(now_enabled))
			last_enabled = now_enabled;
		else
			enabled = last_enabled;
	}
}
static DECLARE_DELAYED_WORK(damon_lru_sort_timer, damon_lru_sort_timer_fn);

static bool damon_lru_sort_initialized;

static int damon_lru_sort_enabled_store(const char *val,
		const struct kernel_param *kp)
{
	int rc = param_set_bool(val, kp);

	if (rc < 0)
		return rc;

	if (!damon_lru_sort_initialized)
		return rc;

	schedule_delayed_work(&damon_lru_sort_timer, 0);

	return 0;
}

static const struct kernel_param_ops enabled_param_ops = {
	.set = damon_lru_sort_enabled_store,
	.get = param_get_bool,
};

module_param_cb(enabled, &enabled_param_ops, &enabled, 0600);
MODULE_PARM_DESC(enabled,
	"Enable or disable DAMON_LRU_SORT (default: disabled)");

static int damon_lru_sort_handle_commit_inputs(void)
{
	int err;

	if (!commit_inputs)
		return 0;

	err = damon_lru_sort_apply_parameters();
	commit_inputs = false;
	return err;
}

static int damon_lru_sort_after_aggregation(struct damon_ctx *c)
{
	struct damos *s;

	/* update the stats parameter */
	damon_for_each_scheme(s, c) {
		if (s->action == DAMOS_LRU_PRIO)
			damon_lru_sort_hot_stat = s->stat;
		else if (s->action == DAMOS_LRU_DEPRIO)
			damon_lru_sort_cold_stat = s->stat;
	}

	return damon_lru_sort_handle_commit_inputs();
}

static int damon_lru_sort_after_wmarks_check(struct damon_ctx *c)
{
	return damon_lru_sort_handle_commit_inputs();
}

static int __init damon_lru_sort_init(void)
{
	ctx = damon_new_ctx();
	if (!ctx)
		return -ENOMEM;

	if (damon_select_ops(ctx, DAMON_OPS_PADDR)) {
		damon_destroy_ctx(ctx);
		return -EINVAL;
	}

	ctx->callback.after_wmarks_check = damon_lru_sort_after_wmarks_check;
	ctx->callback.after_aggregation = damon_lru_sort_after_aggregation;

	target = damon_new_target();
	if (!target) {
		damon_destroy_ctx(ctx);
		return -ENOMEM;
	}
	damon_add_target(ctx, target);

	schedule_delayed_work(&damon_lru_sort_timer, 0);

	damon_lru_sort_initialized = true;
	return 0;
}

module_init(damon_lru_sort_init);
