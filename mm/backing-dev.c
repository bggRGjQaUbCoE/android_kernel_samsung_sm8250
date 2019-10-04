
#include <linux/wait.h>
#include <linux/backing-dev.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/writeback.h>
#include <linux/device.h>
#include <trace/events/writeback.h>

struct backing_dev_info noop_backing_dev_info = {
	.name		= "noop",
	.capabilities	= BDI_CAP_NO_ACCT_AND_WRITEBACK,
};
EXPORT_SYMBOL_GPL(noop_backing_dev_info);

static struct class *bdi_class;
const char *bdi_unknown_name = "(unknown)";

/*
 * bdi_lock protects updates to bdi_list. bdi_list has RCU reader side
 * locking.
 */
DEFINE_SPINLOCK(bdi_lock);
LIST_HEAD(bdi_list);

/* bdi_wq serves all asynchronous writeback tasks */
struct workqueue_struct *bdi_wq;

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/seq_file.h>

static struct dentry *bdi_debug_root;

static void bdi_debug_init(void)
{
	bdi_debug_root = debugfs_create_dir("bdi", NULL);
}

static int bdi_debug_stats_show(struct seq_file *m, void *v)
{
	struct backing_dev_info *bdi = m->private;
	struct bdi_writeback *wb = &bdi->wb;
	unsigned long background_thresh;
	unsigned long dirty_thresh;
	unsigned long wb_thresh;
	unsigned long nr_dirty, nr_io, nr_more_io, nr_dirty_time;
	struct inode *inode;

	nr_dirty = nr_io = nr_more_io = nr_dirty_time = 0;
	spin_lock(&wb->list_lock);
	list_for_each_entry(inode, &wb->b_dirty, i_io_list)
		nr_dirty++;
	list_for_each_entry(inode, &wb->b_io, i_io_list)
		nr_io++;
	list_for_each_entry(inode, &wb->b_more_io, i_io_list)
		nr_more_io++;
	list_for_each_entry(inode, &wb->b_dirty_time, i_io_list)
		if (inode->i_state & I_DIRTY_TIME)
			nr_dirty_time++;
	spin_unlock(&wb->list_lock);

	global_dirty_limits(&background_thresh, &dirty_thresh);
	wb_thresh = wb_calc_thresh(wb, dirty_thresh);

#define K(x) ((x) << (PAGE_SHIFT - 10))
	seq_printf(m,
		   "BdiWriteback:       %10lu kB\n"
		   "BdiReclaimable:     %10lu kB\n"
		   "BdiDirtyThresh:     %10lu kB\n"
		   "DirtyThresh:        %10lu kB\n"
		   "BackgroundThresh:   %10lu kB\n"
		   "BdiDirtied:         %10lu kB\n"
		   "BdiWritten:         %10lu kB\n"
		   "BdiWriteBandwidth:  %10lu kBps\n"
		   "b_dirty:            %10lu\n"
		   "b_io:               %10lu\n"
		   "b_more_io:          %10lu\n"
		   "b_dirty_time:       %10lu\n"
		   "bdi_list:           %10u\n"
		   "state:              %10lx\n",
		   (unsigned long) K(wb_stat(wb, WB_WRITEBACK)),
		   (unsigned long) K(wb_stat(wb, WB_RECLAIMABLE)),
		   K(wb_thresh),
		   K(dirty_thresh),
		   K(background_thresh),
		   (unsigned long) K(wb_stat(wb, WB_DIRTIED)),
		   (unsigned long) K(wb_stat(wb, WB_WRITTEN)),
		   (unsigned long) K(wb->write_bandwidth),
		   nr_dirty,
		   nr_io,
		   nr_more_io,
		   nr_dirty_time,
		   !list_empty(&bdi->bdi_list), bdi->wb.state);
#undef K

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(bdi_debug_stats);

static int bdi_debug_register(struct backing_dev_info *bdi, const char *name)
{
	if (!bdi_debug_root)
		return -ENOMEM;

	bdi->debug_dir = debugfs_create_dir(name, bdi_debug_root);
	if (!bdi->debug_dir)
		return -ENOMEM;

	bdi->debug_stats = debugfs_create_file("stats", 0444, bdi->debug_dir,
					       bdi, &bdi_debug_stats_fops);
	if (!bdi->debug_stats) {
		debugfs_remove(bdi->debug_dir);
		bdi->debug_dir = NULL;
		return -ENOMEM;
	}

	return 0;
}

static void bdi_debug_unregister(struct backing_dev_info *bdi)
{
	debugfs_remove(bdi->debug_stats);
	debugfs_remove(bdi->debug_dir);
}
#else
static inline void bdi_debug_init(void)
{
}
static inline int bdi_debug_register(struct backing_dev_info *bdi,
				      const char *name)
{
	return 0;
}
static inline void bdi_debug_unregister(struct backing_dev_info *bdi)
{
}
#endif

static ssize_t read_ahead_kb_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct backing_dev_info *bdi = dev_get_drvdata(dev);
	unsigned long read_ahead_kb;
	ssize_t ret;
	static const char temp[] = "temporary ";

	if (strncmp(buf, temp, sizeof(temp) - 1) != 0)
		return count;

	buf += sizeof(temp) - 1;

	ret = kstrtoul(buf, 10, &read_ahead_kb);

	if (ret < 0)
		return ret;

	bdi->ra_pages = read_ahead_kb >> (PAGE_SHIFT - 10);

	return count;
}

#define K(pages) ((pages) << (PAGE_SHIFT - 10))

#define BDI_SHOW(name, expr)						\
static ssize_t name##_show(struct device *dev,				\
			   struct device_attribute *attr, char *page)	\
{									\
	struct backing_dev_info *bdi = dev_get_drvdata(dev);		\
									\
	return snprintf(page, PAGE_SIZE-1, "%lld\n", (long long)expr);	\
}									\
static DEVICE_ATTR_RW(name);

BDI_SHOW(read_ahead_kb, K(bdi->ra_pages))

static ssize_t min_ratio_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct backing_dev_info *bdi = dev_get_drvdata(dev);
	unsigned int ratio;
	ssize_t ret;

	ret = kstrtouint(buf, 10, &ratio);
	if (ret < 0)
		return ret;

	ret = bdi_set_min_ratio(bdi, ratio);
	if (!ret)
		ret = count;

	return ret;
}
BDI_SHOW(min_ratio, bdi->min_ratio)

static ssize_t max_ratio_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct backing_dev_info *bdi = dev_get_drvdata(dev);
	unsigned int ratio;
	ssize_t ret;

	ret = kstrtouint(buf, 10, &ratio);
	if (ret < 0)
		return ret;

	ret = bdi_set_max_ratio(bdi, ratio);
	if (!ret)
		ret = count;

	return ret;
}
BDI_SHOW(max_ratio, bdi->max_ratio)

static ssize_t stable_pages_required_show(struct device *dev,
					  struct device_attribute *attr,
					  char *page)
{
	struct backing_dev_info *bdi = dev_get_drvdata(dev);

	return snprintf(page, PAGE_SIZE-1, "%d\n",
			bdi_cap_stable_pages_required(bdi) ? 1 : 0);
}
static DEVICE_ATTR_RO(stable_pages_required);

static ssize_t bdp_debug_show(struct device *dev,
					  struct device_attribute *attr,
					  char *page)
{
	struct backing_dev_info *bdi = dev_get_drvdata(dev);
	struct sec_backing_dev_info *sec_bdi = SEC_BDI(bdi);
	int len = 0, i;

	len += snprintf(page + len, PAGE_SIZE-len-1,
		"start_time, elapsed_ms, g_thresh, g_dirty, wb_thresh, wb_dirty"
		", avg_bw, timelist_dirty, timelist_inodes\n");

	spin_lock(&sec_bdi->bdp_debug.lock);
	for (i = 0; i < BDI_BDP_DEBUG_ENTRY && i < sec_bdi->bdp_debug.total; i++) {
		struct bdi_sec_bdp_entry *entry = sec_bdi->bdp_debug.entry + i;

		len += snprintf(page + len, PAGE_SIZE-len-1,
			"%lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu\n",
			entry->start_time,
			entry->elapsed_ms,
			entry->global_thresh,
			entry->global_dirty,
			entry->wb_thresh,
			entry->wb_dirty,
			entry->wb_avg_write_bandwidth,
			entry->wb_timelist_dirty,
			entry->wb_timelist_inodes);
	}
	spin_unlock(&sec_bdi->bdp_debug.lock);

	return len;
}
static DEVICE_ATTR_RO(bdp_debug);

static ssize_t max_bdp_debug_show(struct device *dev,
					  struct device_attribute *attr,
					  char *page)
{
	struct backing_dev_info *bdi = dev_get_drvdata(dev);
	struct sec_backing_dev_info *sec_bdi = SEC_BDI(bdi);
	int len = 0;
	struct bdi_sec_bdp_entry *entry = &sec_bdi->bdp_debug.max_entry;

	len += snprintf(page + len, PAGE_SIZE-len-1,
		"start_time, elapsed_ms, g_thresh, g_dirty, wb_thresh, wb_dirty"
		", avg_bw, timelist_dirty, timelist_inodes\n");

	spin_lock(&sec_bdi->bdp_debug.lock);
	len += snprintf(page + len, PAGE_SIZE-len-1,
			"%lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu\n",
			entry->start_time,
			entry->elapsed_ms,
			entry->global_thresh,
			entry->global_dirty,
			entry->wb_thresh,
			entry->wb_dirty,
			entry->wb_avg_write_bandwidth,
			entry->wb_timelist_dirty,
			entry->wb_timelist_inodes);
	spin_unlock(&sec_bdi->bdp_debug.lock);

	return len;
}
static DEVICE_ATTR_RO(max_bdp_debug);

static struct attribute *bdi_dev_attrs[] = {
	&dev_attr_read_ahead_kb.attr,
	&dev_attr_min_ratio.attr,
	&dev_attr_max_ratio.attr,
	&dev_attr_stable_pages_required.attr,
	&dev_attr_bdp_debug.attr,
	&dev_attr_max_bdp_debug.attr,
	NULL,
};
ATTRIBUTE_GROUPS(bdi_dev);

static __init int bdi_class_init(void)
{
	bdi_class = class_create(THIS_MODULE, "bdi");
	if (IS_ERR(bdi_class))
		return PTR_ERR(bdi_class);

	bdi_class->dev_groups = bdi_dev_groups;
	bdi_debug_init();

	return 0;
}
postcore_initcall(bdi_class_init);

static int bdi_init(struct backing_dev_info *bdi);

static int __init default_bdi_init(void)
{
	int err;

	bdi_wq = alloc_workqueue("writeback", WQ_MEM_RECLAIM | WQ_UNBOUND |
				 WQ_SYSFS, 0);
	if (!bdi_wq)
		return -ENOMEM;

	err = bdi_init(&noop_backing_dev_info);

	return err;
}
subsys_initcall(default_bdi_init);

/*
 * This function is used when the first inode for this wb is marked dirty. It
 * wakes-up the corresponding bdi thread which should then take care of the
 * periodic background write-out of dirty inodes. Since the write-out would
 * starts only 'dirty_writeback_interval' centisecs from now anyway, we just
 * set up a timer which wakes the bdi thread up later.
 *
 * Note, we wouldn't bother setting up the timer, but this function is on the
 * fast-path (used by '__mark_inode_dirty()'), so we save few context switches
 * by delaying the wake-up.
 *
 * We have to be careful not to postpone flush work if it is scheduled for
 * earlier. Thus we use queue_delayed_work().
 */
void wb_wakeup_delayed(struct bdi_writeback *wb)
{
	unsigned long timeout;

	timeout = msecs_to_jiffies(dirty_writeback_interval * 10);
	spin_lock_bh(&wb->work_lock);
	if (test_bit(WB_registered, &wb->state))
		queue_delayed_work(bdi_wq, &wb->dwork, timeout);
	spin_unlock_bh(&wb->work_lock);
}

/*
 * Initial write bandwidth: 100 MB/s
 */
#define INIT_BW		(100 << (20 - PAGE_SHIFT))

static int wb_init(struct bdi_writeback *wb, struct backing_dev_info *bdi,
		   int blkcg_id, gfp_t gfp)
{
	int i, err;

	memset(wb, 0, sizeof(*wb));

	if (wb != &bdi->wb)
		bdi_get(bdi);
	wb->bdi = bdi;
	wb->last_old_flush = jiffies;
	INIT_LIST_HEAD(&wb->b_dirty);
	INIT_LIST_HEAD(&wb->b_io);
	INIT_LIST_HEAD(&wb->b_more_io);
	INIT_LIST_HEAD(&wb->b_dirty_time);
	spin_lock_init(&wb->list_lock);

	wb->bw_time_stamp = jiffies;
	wb->balanced_dirty_ratelimit = INIT_BW;
	wb->dirty_ratelimit = INIT_BW;
	wb->write_bandwidth = INIT_BW;
	wb->avg_write_bandwidth = INIT_BW;

	spin_lock_init(&wb->work_lock);
	INIT_LIST_HEAD(&wb->work_list);
	INIT_DELAYED_WORK(&wb->dwork, wb_workfn);
	wb->dirty_sleep = jiffies;

	wb->congested = wb_congested_get_create(bdi, blkcg_id, gfp);
	if (!wb->congested) {
		err = -ENOMEM;
		goto out_put_bdi;
	}

	err = fprop_local_init_percpu(&wb->completions, gfp);
	if (err)
		goto out_put_cong;

	for (i = 0; i < NR_WB_STAT_ITEMS; i++) {
		err = percpu_counter_init(&wb->stat[i], 0, gfp);
		if (err)
			goto out_destroy_stat;
	}

	return 0;

out_destroy_stat:
	while (i--)
		percpu_counter_destroy(&wb->stat[i]);
	fprop_local_destroy_percpu(&wb->completions);
out_put_cong:
	wb_congested_put(wb->congested);
out_put_bdi:
	if (wb != &bdi->wb)
		bdi_put(bdi);
	return err;
}

static void cgwb_remove_from_bdi_list(struct bdi_writeback *wb);

/*
 * Remove bdi from the global list and shutdown any threads we have running
 */
static void wb_shutdown(struct bdi_writeback *wb)
{
	/* Make sure nobody queues further work */
	spin_lock_bh(&wb->work_lock);
	if (!test_and_clear_bit(WB_registered, &wb->state)) {
		spin_unlock_bh(&wb->work_lock);
		return;
	}
	spin_unlock_bh(&wb->work_lock);

	cgwb_remove_from_bdi_list(wb);
	/*
	 * Drain work list and shutdown the delayed_work.  !WB_registered
	 * tells wb_workfn() that @wb is dying and its work_list needs to
	 * be drained no matter what.
	 */
	mod_delayed_work(bdi_wq, &wb->dwork, 0);
	flush_delayed_work(&wb->dwork);
	WARN_ON(!list_empty(&wb->work_list));
}

static void wb_exit(struct bdi_writeback *wb)
{
	int i;

	WARN_ON(delayed_work_pending(&wb->dwork));

	for (i = 0; i < NR_WB_STAT_ITEMS; i++)
		percpu_counter_destroy(&wb->stat[i]);

	fprop_local_destroy_percpu(&wb->completions);
	wb_congested_put(wb->congested);
	if (wb != &wb->bdi->wb)
		bdi_put(wb->bdi);
}

#ifdef CONFIG_CGROUP_WRITEBACK

#include <linux/memcontrol.h>

/*
 * cgwb_lock protects bdi->cgwb_tree, bdi->cgwb_congested_tree,
 * blkcg->cgwb_list, and memcg->cgwb_list.  bdi->cgwb_tree is also RCU
 * protected.
 */
static DEFINE_SPINLOCK(cgwb_lock);
static struct workqueue_struct *cgwb_release_wq;

/**
 * wb_congested_get_create - get or create a wb_congested
 * @bdi: associated bdi
 * @blkcg_id: ID of the associated blkcg
 * @gfp: allocation mask
 *
 * Look up the wb_congested for @blkcg_id on @bdi.  If missing, create one.
 * The returned wb_congested has its reference count incremented.  Returns
 * NULL on failure.
 */
struct bdi_writeback_congested *
wb_congested_get_create(struct backing_dev_info *bdi, int blkcg_id, gfp_t gfp)
{
	struct bdi_writeback_congested *new_congested = NULL, *congested;
	struct rb_node **node, *parent;
	unsigned long flags;
retry:
	spin_lock_irqsave(&cgwb_lock, flags);

	node = &bdi->cgwb_congested_tree.rb_node;
	parent = NULL;

	while (*node != NULL) {
		parent = *node;
		congested = rb_entry(parent, struct bdi_writeback_congested,
				     rb_node);
		if (congested->blkcg_id < blkcg_id)
			node = &parent->rb_left;
		else if (congested->blkcg_id > blkcg_id)
			node = &parent->rb_right;
		else
			goto found;
	}

	if (new_congested) {
		/* !found and storage for new one already allocated, insert */
		congested = new_congested;
		rb_link_node(&congested->rb_node, parent, node);
		rb_insert_color(&congested->rb_node, &bdi->cgwb_congested_tree);
		spin_unlock_irqrestore(&cgwb_lock, flags);
		return congested;
	}

	spin_unlock_irqrestore(&cgwb_lock, flags);

	/* allocate storage for new one and retry */
	new_congested = kzalloc(sizeof(*new_congested), gfp);
	if (!new_congested)
		return NULL;

	refcount_set(&new_congested->refcnt, 1);
	new_congested->__bdi = bdi;
	new_congested->blkcg_id = blkcg_id;
	goto retry;

found:
	refcount_inc(&congested->refcnt);
	spin_unlock_irqrestore(&cgwb_lock, flags);
	kfree(new_congested);
	return congested;
}

/**
 * wb_congested_put - put a wb_congested
 * @congested: wb_congested to put
 *
 * Put @congested and destroy it if the refcnt reaches zero.
 */
void wb_congested_put(struct bdi_writeback_congested *congested)
{
	unsigned long flags;

	if (!refcount_dec_and_lock_irqsave(&congested->refcnt, &cgwb_lock, &flags))
		return;

	/* bdi might already have been destroyed leaving @congested unlinked */
	if (congested->__bdi) {
		rb_erase(&congested->rb_node,
			 &congested->__bdi->cgwb_congested_tree);
		congested->__bdi = NULL;
	}

	spin_unlock_irqrestore(&cgwb_lock, flags);
	kfree(congested);
}

static void cgwb_release_workfn(struct work_struct *work)
{
	struct bdi_writeback *wb = container_of(work, struct bdi_writeback,
						release_work);
	struct blkcg *blkcg = css_to_blkcg(wb->blkcg_css);

	mutex_lock(&wb->bdi->cgwb_release_mutex);
	wb_shutdown(wb);

	css_put(wb->memcg_css);
	css_put(wb->blkcg_css);
	mutex_unlock(&wb->bdi->cgwb_release_mutex);

	/* triggers blkg destruction if cgwb_refcnt becomes zero */
	blkcg_cgwb_put(blkcg);

	fprop_local_destroy_percpu(&wb->memcg_completions);
	percpu_ref_exit(&wb->refcnt);
	wb_exit(wb);
	kfree_rcu(wb, rcu);
}

static void cgwb_release(struct percpu_ref *refcnt)
{
	struct bdi_writeback *wb = container_of(refcnt, struct bdi_writeback,
						refcnt);
	queue_work(cgwb_release_wq, &wb->release_work);
}

static void cgwb_kill(struct bdi_writeback *wb)
{
	lockdep_assert_held(&cgwb_lock);

	WARN_ON(!radix_tree_delete(&wb->bdi->cgwb_tree, wb->memcg_css->id));
	list_del(&wb->memcg_node);
	list_del(&wb->blkcg_node);
	percpu_ref_kill(&wb->refcnt);
}

static void cgwb_remove_from_bdi_list(struct bdi_writeback *wb)
{
	spin_lock_irq(&cgwb_lock);
	list_del_rcu(&wb->bdi_node);
	spin_unlock_irq(&cgwb_lock);
}

static int cgwb_create(struct backing_dev_info *bdi,
		       struct cgroup_subsys_state *memcg_css, gfp_t gfp)
{
	struct mem_cgroup *memcg;
	struct cgroup_subsys_state *blkcg_css;
	struct blkcg *blkcg;
	struct list_head *memcg_cgwb_list, *blkcg_cgwb_list;
	struct bdi_writeback *wb;
	unsigned long flags;
	int ret = 0;

	memcg = mem_cgroup_from_css(memcg_css);
	blkcg_css = cgroup_get_e_css(memcg_css->cgroup, &io_cgrp_subsys);
	blkcg = css_to_blkcg(blkcg_css);
	memcg_cgwb_list = &memcg->cgwb_list;
	blkcg_cgwb_list = &blkcg->cgwb_list;

	/* look up again under lock and discard on blkcg mismatch */
	spin_lock_irqsave(&cgwb_lock, flags);
	wb = radix_tree_lookup(&bdi->cgwb_tree, memcg_css->id);
	if (wb && wb->blkcg_css != blkcg_css) {
		cgwb_kill(wb);
		wb = NULL;
	}
	spin_unlock_irqrestore(&cgwb_lock, flags);
	if (wb)
		goto out_put;

	/* need to create a new one */
	wb = kmalloc(sizeof(*wb), gfp);
	if (!wb) {
		ret = -ENOMEM;
		goto out_put;
	}

	ret = wb_init(wb, bdi, blkcg_css->id, gfp);
	if (ret)
		goto err_free;

	ret = percpu_ref_init(&wb->refcnt, cgwb_release, 0, gfp);
	if (ret)
		goto err_wb_exit;

	ret = fprop_local_init_percpu(&wb->memcg_completions, gfp);
	if (ret)
		goto err_ref_exit;

	wb->memcg_css = memcg_css;
	wb->blkcg_css = blkcg_css;
	INIT_WORK(&wb->release_work, cgwb_release_workfn);
	set_bit(WB_registered, &wb->state);

	/*
	 * The root wb determines the registered state of the whole bdi and
	 * memcg_cgwb_list and blkcg_cgwb_list's next pointers indicate
	 * whether they're still online.  Don't link @wb if any is dead.
	 * See wb_memcg_offline() and wb_blkcg_offline().
	 */
	ret = -ENODEV;
	spin_lock_irqsave(&cgwb_lock, flags);
	if (test_bit(WB_registered, &bdi->wb.state) &&
	    blkcg_cgwb_list->next && memcg_cgwb_list->next) {
		/* we might have raced another instance of this function */
		ret = radix_tree_insert(&bdi->cgwb_tree, memcg_css->id, wb);
		if (!ret) {
			list_add_tail_rcu(&wb->bdi_node, &bdi->wb_list);
			list_add(&wb->memcg_node, memcg_cgwb_list);
			list_add(&wb->blkcg_node, blkcg_cgwb_list);
			blkcg_cgwb_get(blkcg);
			css_get(memcg_css);
			css_get(blkcg_css);
		}
	}
	spin_unlock_irqrestore(&cgwb_lock, flags);
	if (ret) {
		if (ret == -EEXIST)
			ret = 0;
		goto err_fprop_exit;
	}
	goto out_put;

err_fprop_exit:
	fprop_local_destroy_percpu(&wb->memcg_completions);
err_ref_exit:
	percpu_ref_exit(&wb->refcnt);
err_wb_exit:
	wb_exit(wb);
err_free:
	kfree(wb);
out_put:
	css_put(blkcg_css);
	return ret;
}

/**
 * wb_get_create - get wb for a given memcg, create if necessary
 * @bdi: target bdi
 * @memcg_css: cgroup_subsys_state of the target memcg (must have positive ref)
 * @gfp: allocation mask to use
 *
 * Try to get the wb for @memcg_css on @bdi.  If it doesn't exist, try to
 * create one.  The returned wb has its refcount incremented.
 *
 * This function uses css_get() on @memcg_css and thus expects its refcnt
 * to be positive on invocation.  IOW, rcu_read_lock() protection on
 * @memcg_css isn't enough.  try_get it before calling this function.
 *
 * A wb is keyed by its associated memcg.  As blkcg implicitly enables
 * memcg on the default hierarchy, memcg association is guaranteed to be
 * more specific (equal or descendant to the associated blkcg) and thus can
 * identify both the memcg and blkcg associations.
 *
 * Because the blkcg associated with a memcg may change as blkcg is enabled
 * and disabled closer to root in the hierarchy, each wb keeps track of
 * both the memcg and blkcg associated with it and verifies the blkcg on
 * each lookup.  On mismatch, the existing wb is discarded and a new one is
 * created.
 */
struct bdi_writeback *wb_get_create(struct backing_dev_info *bdi,
				    struct cgroup_subsys_state *memcg_css,
				    gfp_t gfp)
{
	struct bdi_writeback *wb;

	might_sleep_if(gfpflags_allow_blocking(gfp));

	if (!memcg_css->parent)
		return &bdi->wb;

	do {
		rcu_read_lock();
		wb = radix_tree_lookup(&bdi->cgwb_tree, memcg_css->id);
		if (wb) {
			struct cgroup_subsys_state *blkcg_css;

			/* see whether the blkcg association has changed */
			blkcg_css = cgroup_get_e_css(memcg_css->cgroup,
						     &io_cgrp_subsys);
			if (unlikely(wb->blkcg_css != blkcg_css ||
				     !wb_tryget(wb)))
				wb = NULL;
			css_put(blkcg_css);
		}
		rcu_read_unlock();
	} while (!wb && !cgwb_create(bdi, memcg_css, gfp));

	return wb;
}

static int cgwb_bdi_init(struct backing_dev_info *bdi)
{
	int ret;

	INIT_RADIX_TREE(&bdi->cgwb_tree, GFP_ATOMIC);
	bdi->cgwb_congested_tree = RB_ROOT;
	mutex_init(&bdi->cgwb_release_mutex);
	init_rwsem(&bdi->wb_switch_rwsem);

	ret = wb_init(&bdi->wb, bdi, 1, GFP_KERNEL);
	if (!ret) {
		bdi->wb.memcg_css = &root_mem_cgroup->css;
		bdi->wb.blkcg_css = blkcg_root_css;
	}
	return ret;
}

static void cgwb_bdi_unregister(struct backing_dev_info *bdi)
{
	struct radix_tree_iter iter;
	void **slot;
	struct bdi_writeback *wb;

	WARN_ON(test_bit(WB_registered, &bdi->wb.state));

	spin_lock_irq(&cgwb_lock);
	radix_tree_for_each_slot(slot, &bdi->cgwb_tree, &iter, 0)
		cgwb_kill(*slot);
	spin_unlock_irq(&cgwb_lock);

	mutex_lock(&bdi->cgwb_release_mutex);
	spin_lock_irq(&cgwb_lock);
	while (!list_empty(&bdi->wb_list)) {
		wb = list_first_entry(&bdi->wb_list, struct bdi_writeback,
				      bdi_node);
		spin_unlock_irq(&cgwb_lock);
		wb_shutdown(wb);
		spin_lock_irq(&cgwb_lock);
	}
	spin_unlock_irq(&cgwb_lock);
	mutex_unlock(&bdi->cgwb_release_mutex);
}

/**
 * wb_memcg_offline - kill all wb's associated with a memcg being offlined
 * @memcg: memcg being offlined
 *
 * Also prevents creation of any new wb's associated with @memcg.
 */
void wb_memcg_offline(struct mem_cgroup *memcg)
{
	struct list_head *memcg_cgwb_list = &memcg->cgwb_list;
	struct bdi_writeback *wb, *next;

	spin_lock_irq(&cgwb_lock);
	list_for_each_entry_safe(wb, next, memcg_cgwb_list, memcg_node)
		cgwb_kill(wb);
	memcg_cgwb_list->next = NULL;	/* prevent new wb's */
	spin_unlock_irq(&cgwb_lock);
}

/**
 * wb_blkcg_offline - kill all wb's associated with a blkcg being offlined
 * @blkcg: blkcg being offlined
 *
 * Also prevents creation of any new wb's associated with @blkcg.
 */
void wb_blkcg_offline(struct blkcg *blkcg)
{
	struct bdi_writeback *wb, *next;

	spin_lock_irq(&cgwb_lock);
	list_for_each_entry_safe(wb, next, &blkcg->cgwb_list, blkcg_node)
		cgwb_kill(wb);
	blkcg->cgwb_list.next = NULL;	/* prevent new wb's */
	spin_unlock_irq(&cgwb_lock);
}

static void cgwb_bdi_exit(struct backing_dev_info *bdi)
{
	struct rb_node *rbn;

	spin_lock_irq(&cgwb_lock);
	while ((rbn = rb_first(&bdi->cgwb_congested_tree))) {
		struct bdi_writeback_congested *congested =
			rb_entry(rbn, struct bdi_writeback_congested, rb_node);

		rb_erase(rbn, &bdi->cgwb_congested_tree);
		congested->__bdi = NULL;	/* mark @congested unlinked */
	}
	spin_unlock_irq(&cgwb_lock);
}

static void cgwb_bdi_register(struct backing_dev_info *bdi)
{
	spin_lock_irq(&cgwb_lock);
	list_add_tail_rcu(&bdi->wb.bdi_node, &bdi->wb_list);
	spin_unlock_irq(&cgwb_lock);
}

static int __init cgwb_init(void)
{
	/*
	 * There can be many concurrent release work items overwhelming
	 * system_wq.  Put them in a separate wq and limit concurrency.
	 * There's no point in executing many of these in parallel.
	 */
	cgwb_release_wq = alloc_workqueue("cgwb_release", 0, 1);
	if (!cgwb_release_wq)
		return -ENOMEM;

	return 0;
}
subsys_initcall(cgwb_init);

#else	/* CONFIG_CGROUP_WRITEBACK */

static int cgwb_bdi_init(struct backing_dev_info *bdi)
{
	int err;

	bdi->wb_congested = kzalloc(sizeof(*bdi->wb_congested), GFP_KERNEL);
	if (!bdi->wb_congested)
		return -ENOMEM;

	refcount_set(&bdi->wb_congested->refcnt, 1);

	err = wb_init(&bdi->wb, bdi, 1, GFP_KERNEL);
	if (err) {
		wb_congested_put(bdi->wb_congested);
		return err;
	}
	return 0;
}

static void cgwb_bdi_unregister(struct backing_dev_info *bdi) { }

static void cgwb_bdi_exit(struct backing_dev_info *bdi)
{
	wb_congested_put(bdi->wb_congested);
}

static void cgwb_bdi_register(struct backing_dev_info *bdi)
{
	list_add_tail_rcu(&bdi->wb.bdi_node, &bdi->wb_list);
}

static void cgwb_remove_from_bdi_list(struct bdi_writeback *wb)
{
	list_del_rcu(&wb->bdi_node);
}

#endif	/* CONFIG_CGROUP_WRITEBACK */

static int bdi_init(struct backing_dev_info *bdi)
{
	int ret;

	bdi->dev = NULL;

	kref_init(&bdi->refcnt);
	bdi->min_ratio = 0;
	bdi->max_ratio = 100;
	bdi->max_prop_frac = FPROP_FRAC_BASE;
	INIT_LIST_HEAD(&bdi->bdi_list);
	INIT_LIST_HEAD(&bdi->wb_list);
	init_waitqueue_head(&bdi->wb_waitq);

	bdi->last_thresh = 0;
	bdi->last_nr_dirty = 0;
	bdi->paused_total = 0;

	ret = cgwb_bdi_init(bdi);

	return ret;
}

struct backing_dev_info *bdi_alloc_node(gfp_t gfp_mask, int node_id)
{
	struct backing_dev_info *bdi;

	bdi = kmalloc_node(sizeof(struct backing_dev_info),
			   gfp_mask | __GFP_ZERO, node_id);
	if (!bdi)
		return NULL;

	if (bdi_init(bdi)) {
		kfree(bdi);
		return NULL;
	}
	return bdi;
}
EXPORT_SYMBOL(bdi_alloc_node);

struct backing_dev_info *sec_bdi_alloc_node(gfp_t gfp_mask, int node_id)
{
	struct sec_backing_dev_info *sec_bdi;

	sec_bdi = kmalloc_node(sizeof(struct sec_backing_dev_info),
			   gfp_mask | __GFP_ZERO, node_id);
	if (!sec_bdi)
		return NULL;

	if (bdi_init(&sec_bdi->bdi)) {
		kfree(sec_bdi);
		return NULL;
	}
	spin_lock_init(&sec_bdi->bdp_debug.lock);

	return (struct backing_dev_info *)sec_bdi;
}
EXPORT_SYMBOL(sec_bdi_alloc_node);

int bdi_register_va(struct backing_dev_info *bdi, const char *fmt, va_list args)
{
	struct device *dev;

	if (bdi->dev)	/* The driver needs to use separate queues per device */
		return 0;

	dev = device_create_vargs(bdi_class, NULL, MKDEV(0, 0), bdi, fmt, args);
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	cgwb_bdi_register(bdi);
	bdi->dev = dev;

	bdi_debug_register(bdi, dev_name(dev));
	set_bit(WB_registered, &bdi->wb.state);

	spin_lock_bh(&bdi_lock);
	list_add_tail_rcu(&bdi->bdi_list, &bdi_list);
	spin_unlock_bh(&bdi_lock);

	trace_writeback_bdi_register(bdi);
	return 0;
}
EXPORT_SYMBOL(bdi_register_va);

int bdi_register(struct backing_dev_info *bdi, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = bdi_register_va(bdi, fmt, args);
	va_end(args);
	return ret;
}
EXPORT_SYMBOL(bdi_register);

int bdi_register_owner(struct backing_dev_info *bdi, struct device *owner)
{
	int rc;

	rc = bdi_register(bdi, "%u:%u", MAJOR(owner->devt), MINOR(owner->devt));
	if (rc)
		return rc;
	/* Leaking owner reference... */
	WARN_ON(bdi->owner);
	bdi->owner = owner;
	get_device(owner);
	return 0;
}
EXPORT_SYMBOL(bdi_register_owner);

/*
 * Remove bdi from bdi_list, and ensure that it is no longer visible
 */
static void bdi_remove_from_list(struct backing_dev_info *bdi)
{
	spin_lock_bh(&bdi_lock);
	list_del_rcu(&bdi->bdi_list);
	spin_unlock_bh(&bdi_lock);

	synchronize_rcu_expedited();
}

void bdi_unregister(struct backing_dev_info *bdi)
{
	/* make sure nobody finds us on the bdi_list anymore */
	bdi_remove_from_list(bdi);
	wb_shutdown(&bdi->wb);
	cgwb_bdi_unregister(bdi);

	if (bdi->dev) {
		bdi_debug_unregister(bdi);
		device_unregister(bdi->dev);
		bdi->dev = NULL;
	}

	if (bdi->owner) {
		put_device(bdi->owner);
		bdi->owner = NULL;
	}
}

static void release_bdi(struct kref *ref)
{
	struct backing_dev_info *bdi =
			container_of(ref, struct backing_dev_info, refcnt);

	if (test_bit(WB_registered, &bdi->wb.state))
		bdi_unregister(bdi);
	WARN_ON_ONCE(bdi->dev);
	wb_exit(&bdi->wb);
	cgwb_bdi_exit(bdi);

	if (bdi->capabilities & BDI_CAP_SEC_DEBUG) {
		struct sec_backing_dev_info *sec_bdi = SEC_BDI(bdi);

		kfree(sec_bdi);
	} else {
		kfree(bdi);
	}
}

void bdi_put(struct backing_dev_info *bdi)
{
	kref_put(&bdi->refcnt, release_bdi);
}
EXPORT_SYMBOL(bdi_put);

static wait_queue_head_t congestion_wqh[2] = {
		__WAIT_QUEUE_HEAD_INITIALIZER(congestion_wqh[0]),
		__WAIT_QUEUE_HEAD_INITIALIZER(congestion_wqh[1])
	};
static atomic_t nr_wb_congested[2];

void clear_wb_congested(struct bdi_writeback_congested *congested, int sync)
{
	wait_queue_head_t *wqh = &congestion_wqh[sync];
	enum wb_congested_state bit;

	bit = sync ? WB_sync_congested : WB_async_congested;
	if (test_and_clear_bit(bit, &congested->state))
		atomic_dec(&nr_wb_congested[sync]);
	smp_mb__after_atomic();
	if (waitqueue_active(wqh))
		wake_up(wqh);
}
EXPORT_SYMBOL(clear_wb_congested);

void set_wb_congested(struct bdi_writeback_congested *congested, int sync)
{
	enum wb_congested_state bit;

	bit = sync ? WB_sync_congested : WB_async_congested;
	if (!test_and_set_bit(bit, &congested->state))
		atomic_inc(&nr_wb_congested[sync]);
}
EXPORT_SYMBOL(set_wb_congested);

/**
 * congestion_wait - wait for a backing_dev to become uncongested
 * @sync: SYNC or ASYNC IO
 * @timeout: timeout in jiffies
 *
 * Waits for up to @timeout jiffies for a backing_dev (any backing_dev) to exit
 * write congestion.  If no backing_devs are congested then just wait for the
 * next write to be completed.
 */
long congestion_wait(int sync, long timeout)
{
	long ret;
	unsigned long start = jiffies;
	DEFINE_WAIT(wait);
	wait_queue_head_t *wqh = &congestion_wqh[sync];

	prepare_to_wait(wqh, &wait, TASK_UNINTERRUPTIBLE);
	ret = io_schedule_timeout(timeout);
	finish_wait(wqh, &wait);

	trace_writeback_congestion_wait(jiffies_to_usecs(timeout),
					jiffies_to_usecs(jiffies - start));

	return ret;
}
EXPORT_SYMBOL(congestion_wait);

/**
 * wait_iff_congested - Conditionally wait for a backing_dev to become uncongested or a pgdat to complete writes
 * @sync: SYNC or ASYNC IO
 * @timeout: timeout in jiffies
 *
 * In the event of a congested backing_dev (any backing_dev) this waits
 * for up to @timeout jiffies for either a BDI to exit congestion of the
 * given @sync queue or a write to complete.
 *
 * The return value is 0 if the sleep is for the full timeout. Otherwise,
 * it is the number of jiffies that were still remaining when the function
 * returned. return_value == timeout implies the function did not sleep.
 */
long wait_iff_congested(int sync, long timeout)
{
	long ret;
	unsigned long start = jiffies;
	DEFINE_WAIT(wait);
	wait_queue_head_t *wqh = &congestion_wqh[sync];

	/*
	 * If there is no congestion, yield if necessary instead
	 * of sleeping on the congestion queue
	 */
	if (atomic_read(&nr_wb_congested[sync]) == 0) {
		cond_resched();

		/* In case we scheduled, work out time remaining */
		ret = timeout - (jiffies - start);
		if (ret < 0)
			ret = 0;

		goto out;
	}

	/* Sleep until uncongested or a write happens */
	prepare_to_wait(wqh, &wait, TASK_UNINTERRUPTIBLE);
	ret = io_schedule_timeout(timeout);
	finish_wait(wqh, &wait);

out:
	trace_writeback_wait_iff_congested(jiffies_to_usecs(timeout),
					jiffies_to_usecs(jiffies - start));

	return ret;
}
EXPORT_SYMBOL(wait_iff_congested);
