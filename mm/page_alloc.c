/*
 *  linux/mm/page_alloc.c
 *
 *  Manages the free list, the system allocates free pages here.
 *  Note that kmalloc() lives in slab.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *  Swap reorganised 29.12.95, Stephen Tweedie
 *  Support of BIGMEM added by Gerhard Wichert, Siemens AG, July 1999
 *  Reshaped it to be a zoned allocator, Ingo Molnar, Red Hat, 1999
 *  Discontiguous memory support, Kanoj Sarcar, SGI, Nov 1999
 *  Zone balancing, Kanoj Sarcar, SGI, Jan 2000
 *  Per cpu hot/cold page lists, bulk allocation, Martin J. Bligh, Sept 2002
 *          (lots of bits borrowed from Ingo Molnar & Andrew Morton)
 */

#include <linux/stddef.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/swap.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <linux/jiffies.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/kasan.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/pagevec.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/oom.h>
#include <linux/topology.h>
#include <linux/sysctl.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/memory_hotplug.h>
#include <linux/nodemask.h>
#include <linux/vmalloc.h>
#include <linux/vmstat.h>
#include <linux/mempolicy.h>
#include <linux/memremap.h>
#include <linux/stop_machine.h>
#include <linux/sort.h>
#include <linux/pfn.h>
#include <linux/backing-dev.h>
#include <linux/fault-inject.h>
#include <linux/page-isolation.h>
#include <linux/page_ext.h>
#include <linux/debugobjects.h>
#include <linux/kmemleak.h>
#include <linux/compaction.h>
#include <trace/events/kmem.h>
#include <trace/events/oom.h>
#include <linux/prefetch.h>
#include <linux/mm_inline.h>
#include <linux/migrate.h>
#include <linux/hugetlb.h>
#include <linux/sched/rt.h>
#include <linux/sched/mm.h>
#include <linux/page_owner.h>
#include <linux/kthread.h>
#include <linux/memcontrol.h>
#include <linux/show_mem_notifier.h>
#include <linux/ftrace.h>
#include <linux/lockdep.h>
#include <linux/nmi.h>
#include <linux/khugepaged.h>
#include <linux/psi.h>
#include <linux/sched/cputime.h>

#include <asm/sections.h>
#include <asm/tlbflush.h>
#include <asm/div64.h>
#include "internal.h"

/* prevent >1 _updater_ of zone percpu pageset ->high and ->batch fields */
static DEFINE_MUTEX(pcp_batch_high_lock);
#define MIN_PERCPU_PAGELIST_FRACTION	(8)

#ifdef CONFIG_USE_PERCPU_NUMA_NODE_ID
DEFINE_PER_CPU(int, numa_node);
EXPORT_PER_CPU_SYMBOL(numa_node);
#endif

DEFINE_STATIC_KEY_TRUE(vm_numa_stat_key);

#ifdef CONFIG_HAVE_MEMORYLESS_NODES
/*
 * N.B., Do NOT reference the '_numa_mem_' per cpu variable directly.
 * It will not be defined when CONFIG_HAVE_MEMORYLESS_NODES is not defined.
 * Use the accessor functions set_numa_mem(), numa_mem_id() and cpu_to_mem()
 * defined in <linux/topology.h>.
 */
DEFINE_PER_CPU(int, _numa_mem_);		/* Kernel "local memory" node */
EXPORT_PER_CPU_SYMBOL(_numa_mem_);
int _node_numa_mem_[MAX_NUMNODES];
#endif

/* work_structs for global per-cpu drains */
DEFINE_MUTEX(pcpu_drain_mutex);
DEFINE_PER_CPU(struct work_struct, pcpu_drain);

#ifdef CONFIG_GCC_PLUGIN_LATENT_ENTROPY
volatile unsigned long latent_entropy __latent_entropy;
EXPORT_SYMBOL(latent_entropy);
#endif

/*
 * Array of node states.
 */
nodemask_t node_states[NR_NODE_STATES] __read_mostly = {
	[N_POSSIBLE] = NODE_MASK_ALL,
	[N_ONLINE] = { { [0] = 1UL } },
#ifndef CONFIG_NUMA
	[N_NORMAL_MEMORY] = { { [0] = 1UL } },
#ifdef CONFIG_HIGHMEM
	[N_HIGH_MEMORY] = { { [0] = 1UL } },
#endif
	[N_MEMORY] = { { [0] = 1UL } },
	[N_CPU] = { { [0] = 1UL } },
#endif	/* NUMA */
};
EXPORT_SYMBOL(node_states);

/* Protect totalram_pages and zone->managed_pages */
static DEFINE_SPINLOCK(managed_page_count_lock);

atomic_long_t _totalram_pages __read_mostly;
EXPORT_SYMBOL(_totalram_pages);
unsigned long totalreserve_pages __read_mostly;
unsigned long totalcma_pages __read_mostly;

int percpu_pagelist_fraction;
gfp_t gfp_allowed_mask __read_mostly = GFP_BOOT_MASK;
#ifdef CONFIG_INIT_ON_ALLOC_DEFAULT_ON
DEFINE_STATIC_KEY_TRUE(init_on_alloc);
#else
DEFINE_STATIC_KEY_FALSE(init_on_alloc);
#endif
EXPORT_SYMBOL(init_on_alloc);

#ifdef CONFIG_INIT_ON_FREE_DEFAULT_ON
DEFINE_STATIC_KEY_TRUE(init_on_free);
#else
DEFINE_STATIC_KEY_FALSE(init_on_free);
#endif
EXPORT_SYMBOL(init_on_free);

static int __init early_init_on_alloc(char *buf)
{
	int ret;
	bool bool_result;

	if (!buf)
		return -EINVAL;
	ret = kstrtobool(buf, &bool_result);
	if (bool_result && page_poisoning_enabled())
		pr_info("mem auto-init: CONFIG_PAGE_POISONING is on, will take precedence over init_on_alloc\n");
	if (bool_result)
		static_branch_enable(&init_on_alloc);
	else
		static_branch_disable(&init_on_alloc);
	return ret;
}
early_param("init_on_alloc", early_init_on_alloc);

static int __init early_init_on_free(char *buf)
{
	int ret;
	bool bool_result;

	if (!buf)
		return -EINVAL;
	ret = kstrtobool(buf, &bool_result);
	if (bool_result && page_poisoning_enabled())
		pr_info("mem auto-init: CONFIG_PAGE_POISONING is on, will take precedence over init_on_free\n");
	if (bool_result)
		static_branch_enable(&init_on_free);
	else
		static_branch_disable(&init_on_free);
	return ret;
}
early_param("init_on_free", early_init_on_free);

/*
 * A cached value of the page's pageblock's migratetype, used when the page is
 * put on a pcplist. Used to avoid the pageblock migratetype lookup when
 * freeing from pcplists in most cases, at the cost of possibly becoming stale.
 * Also the migratetype set in the page does not necessarily match the pcplist
 * index, e.g. page might have MIGRATE_CMA set but be on a pcplist with any
 * other index - this ensures that it will be put on the correct CMA freelist.
 */
static inline int get_pcppage_migratetype(struct page *page)
{
	return page->index;
}

static inline void set_pcppage_migratetype(struct page *page, int migratetype)
{
	page->index = migratetype;
}

#ifdef CONFIG_PM_SLEEP
/*
 * The following functions are used by the suspend/hibernate code to temporarily
 * change gfp_allowed_mask in order to avoid using I/O during memory allocations
 * while devices are suspended.  To avoid races with the suspend/hibernate code,
 * they should always be called with system_transition_mutex held
 * (gfp_allowed_mask also should only be modified with system_transition_mutex
 * held, unless the suspend/hibernate code is guaranteed not to run in parallel
 * with that modification).
 */

static gfp_t saved_gfp_mask;

void pm_restore_gfp_mask(void)
{
	WARN_ON(!mutex_is_locked(&system_transition_mutex));
	if (saved_gfp_mask) {
		gfp_allowed_mask = saved_gfp_mask;
		saved_gfp_mask = 0;
	}
}

void pm_restrict_gfp_mask(void)
{
	WARN_ON(!mutex_is_locked(&system_transition_mutex));
	WARN_ON(saved_gfp_mask);
	saved_gfp_mask = gfp_allowed_mask;
	gfp_allowed_mask &= ~(__GFP_IO | __GFP_FS);
}

bool pm_suspended_storage(void)
{
	if ((gfp_allowed_mask & (__GFP_IO | __GFP_FS)) == (__GFP_IO | __GFP_FS))
		return false;
	return true;
}
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_HUGETLB_PAGE_SIZE_VARIABLE
unsigned int pageblock_order __read_mostly;
#endif

static void __free_pages_ok(struct page *page, unsigned int order);

/*
 * results with 256, 32 in the lowmem_reserve sysctl:
 *	1G machine -> (16M dma, 800M-16M normal, 1G-800M high)
 *	1G machine -> (16M dma, 784M normal, 224M high)
 *	NORMAL allocation will leave 784M/256 of ram reserved in the ZONE_DMA
 *	HIGHMEM allocation will leave 224M/32 of ram reserved in ZONE_NORMAL
 *	HIGHMEM allocation will leave (224M+784M)/256 of ram reserved in ZONE_DMA
 *
 * TBD: should special case ZONE_DMA32 machines here - in those we normally
 * don't need any ZONE_NORMAL reservation
 */
int sysctl_lowmem_reserve_ratio[MAX_NR_ZONES] = {
#ifdef CONFIG_ZONE_DMA
	[ZONE_DMA] = 256,
#endif
#ifdef CONFIG_ZONE_DMA32
	[ZONE_DMA32] = 256,
#endif
	[ZONE_NORMAL] = 32,
#ifdef CONFIG_HIGHMEM
	[ZONE_HIGHMEM] = 0,
#endif
	[ZONE_MOVABLE] = 0,
};

EXPORT_SYMBOL(totalram_pages);

static char * const zone_names[MAX_NR_ZONES] = {
#ifdef CONFIG_ZONE_DMA
	 "DMA",
#endif
#ifdef CONFIG_ZONE_DMA32
	 "DMA32",
#endif
	 "Normal",
#ifdef CONFIG_HIGHMEM
	 "HighMem",
#endif
	 "Movable",
#ifdef CONFIG_ZONE_DEVICE
	 "Device",
#endif
};

char * const migratetype_names[MIGRATE_TYPES] = {
	"Unmovable",
	"Movable",
	"Reclaimable",
#ifdef CONFIG_CMA
	"CMA",
#endif
	"HighAtomic",
#ifdef CONFIG_MEMORY_ISOLATION
	"Isolate",
#endif
};

compound_page_dtor * const compound_page_dtors[] = {
	NULL,
	free_compound_page,
#ifdef CONFIG_HUGETLB_PAGE
	free_huge_page,
#endif
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	free_transhuge_page,
#endif
};

/*
 * Try to keep at least this much lowmem free.  Do not allow normal
 * allocations below this point, only high priority ones. Automatically
 * tuned according to the amount of memory in the system.
 */
int min_free_kbytes = 32768;
int user_min_free_kbytes = -1;
#ifdef CONFIG_DISCONTIGMEM
/*
 * DiscontigMem defines memory ranges as separate pg_data_t even if the ranges
 * are not on separate NUMA nodes. Functionally this works but with
 * watermark_boost_factor, it can reclaim prematurely as the ranges can be
 * quite small. By default, do not boost watermarks on discontigmem as in
 * many cases very high-order allocations like THP are likely to be
 * unsupported and the premature reclaim offsets the advantage of long-term
 * fragmentation avoidance.
 */
int watermark_boost_factor __read_mostly;
#else
/* Set watermark_boost_factor 0 by default(disable) */
int watermark_boost_factor __read_mostly;
#endif
int watermark_scale_factor = 32;

/*
 * Extra memory for the system to try freeing. Used to temporarily
 * free memory, to make space for new workloads. Anyone can allocate
 * down to the min watermarks controlled by min_free_kbytes above.
 */
int extra_free_kbytes = 0;

static unsigned long nr_kernel_pages __meminitdata;
static unsigned long nr_all_pages __meminitdata;
static unsigned long dma_reserve __meminitdata;

#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
static unsigned long arch_zone_lowest_possible_pfn[MAX_NR_ZONES] __meminitdata;
static unsigned long arch_zone_highest_possible_pfn[MAX_NR_ZONES] __meminitdata;
static unsigned long required_kernelcore __initdata;
static unsigned long required_kernelcore_percent __initdata;
static unsigned long required_movablecore __initdata;
static unsigned long required_movablecore_percent __initdata;
static unsigned long zone_movable_pfn[MAX_NUMNODES] __meminitdata;
static bool mirrored_kernelcore __meminitdata;

/* movable_zone is the "real" zone pages in ZONE_MOVABLE are taken from */
int movable_zone;
EXPORT_SYMBOL(movable_zone);
#endif /* CONFIG_HAVE_MEMBLOCK_NODE_MAP */

#if MAX_NUMNODES > 1
int nr_node_ids __read_mostly = MAX_NUMNODES;
int nr_online_nodes __read_mostly = 1;
EXPORT_SYMBOL(nr_node_ids);
EXPORT_SYMBOL(nr_online_nodes);
#endif

int page_group_by_mobility_disabled __read_mostly;

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
/*
 * During boot we initialize deferred pages on-demand, as needed, but once
 * page_alloc_init_late() has finished, the deferred pages are all initialized,
 * and we can permanently disable that path.
 */
static DEFINE_STATIC_KEY_TRUE(deferred_pages);

/*
 * Calling kasan_free_pages() only after deferred memory initialization
 * has completed. Poisoning pages during deferred memory init will greatly
 * lengthen the process and cause problem in large memory systems as the
 * deferred pages initialization is done with interrupt disabled.
 *
 * Assuming that there will be no reference to those newly initialized
 * pages before they are ever allocated, this should have no effect on
 * KASAN memory tracking as the poison will be properly inserted at page
 * allocation time. The only corner case is when pages are allocated by
 * on-demand allocation and then freed again before the deferred pages
 * initialization is done, but this is not likely to happen.
 */
static inline void kasan_free_nondeferred_pages(struct page *page, int order)
{
	if (!static_branch_unlikely(&deferred_pages))
		kasan_free_pages(page, order);
}

/* Returns true if the struct page for the pfn is uninitialised */
static inline bool __meminit early_page_uninitialised(unsigned long pfn)
{
	int nid = early_pfn_to_nid(pfn);

	if (node_online(nid) && pfn >= NODE_DATA(nid)->first_deferred_pfn)
		return true;

	return false;
}

/*
 * Returns false when the remaining initialisation should be deferred until
 * later in the boot cycle when it can be parallelised.
 */
static inline bool update_defer_init(pg_data_t *pgdat,
				unsigned long pfn, unsigned long zone_end,
				unsigned long *nr_initialised)
{
	/* Always populate low zones for address-constrained allocations */
	if (zone_end < pgdat_end_pfn(pgdat))
		return true;
	(*nr_initialised)++;
	if ((*nr_initialised > pgdat->static_init_pgcnt) &&
	    (pfn & (PAGES_PER_SECTION - 1)) == 0) {
		pgdat->first_deferred_pfn = pfn;
		return false;
	}

	return true;
}
#else
#define kasan_free_nondeferred_pages(p, o)	kasan_free_pages(p, o)

static inline bool early_page_uninitialised(unsigned long pfn)
{
	return false;
}

static inline bool update_defer_init(pg_data_t *pgdat,
				unsigned long pfn, unsigned long zone_end,
				unsigned long *nr_initialised)
{
	return true;
}
#endif

/* Return a pointer to the bitmap storing bits affecting a block of pages */
static inline unsigned long *get_pageblock_bitmap(struct page *page,
							unsigned long pfn)
{
#ifdef CONFIG_SPARSEMEM
	return __pfn_to_section(pfn)->pageblock_flags;
#else
	return page_zone(page)->pageblock_flags;
#endif /* CONFIG_SPARSEMEM */
}

static inline int pfn_to_bitidx(struct page *page, unsigned long pfn)
{
#ifdef CONFIG_SPARSEMEM
	pfn &= (PAGES_PER_SECTION-1);
	return (pfn >> pageblock_order) * NR_PAGEBLOCK_BITS;
#else
	pfn = pfn - round_down(page_zone(page)->zone_start_pfn, pageblock_nr_pages);
	return (pfn >> pageblock_order) * NR_PAGEBLOCK_BITS;
#endif /* CONFIG_SPARSEMEM */
}

/**
 * get_pfnblock_flags_mask - Return the requested group of flags for the pageblock_nr_pages block of pages
 * @page: The page within the block of interest
 * @pfn: The target page frame number
 * @end_bitidx: The last bit of interest to retrieve
 * @mask: mask of bits that the caller is interested in
 *
 * Return: pageblock_bits flags
 */
static __always_inline unsigned long __get_pfnblock_flags_mask(struct page *page,
					unsigned long pfn,
					unsigned long end_bitidx,
					unsigned long mask)
{
	unsigned long *bitmap;
	unsigned long bitidx, word_bitidx;
	unsigned long word;

	bitmap = get_pageblock_bitmap(page, pfn);
	bitidx = pfn_to_bitidx(page, pfn);
	word_bitidx = bitidx / BITS_PER_LONG;
	bitidx &= (BITS_PER_LONG-1);

	/*
	 * This races, without locks, with set_pfnblock_flags_mask(). Ensure
	 * a consistent read of the memory array, so that results, even though
	 * racy, are not corrupted.
	 */
	word = READ_ONCE(bitmap[word_bitidx]);
	bitidx += end_bitidx;
	return (word >> (BITS_PER_LONG - bitidx - 1)) & mask;
}

unsigned long get_pfnblock_flags_mask(struct page *page, unsigned long pfn,
					unsigned long end_bitidx,
					unsigned long mask)
{
	return __get_pfnblock_flags_mask(page, pfn, end_bitidx, mask);
}

static __always_inline int get_pfnblock_migratetype(struct page *page, unsigned long pfn)
{
	return __get_pfnblock_flags_mask(page, pfn, PB_migrate_end, MIGRATETYPE_MASK);
}

/**
 * set_pfnblock_flags_mask - Set the requested group of flags for a pageblock_nr_pages block of pages
 * @page: The page within the block of interest
 * @flags: The flags to set
 * @pfn: The target page frame number
 * @end_bitidx: The last bit of interest
 * @mask: mask of bits that the caller is interested in
 */
void set_pfnblock_flags_mask(struct page *page, unsigned long flags,
					unsigned long pfn,
					unsigned long end_bitidx,
					unsigned long mask)
{
	unsigned long *bitmap;
	unsigned long bitidx, word_bitidx;
	unsigned long old_word, word;

	BUILD_BUG_ON(NR_PAGEBLOCK_BITS != 4);

	bitmap = get_pageblock_bitmap(page, pfn);
	bitidx = pfn_to_bitidx(page, pfn);
	word_bitidx = bitidx / BITS_PER_LONG;
	bitidx &= (BITS_PER_LONG-1);

	VM_BUG_ON_PAGE(!zone_spans_pfn(page_zone(page), pfn), page);

	bitidx += end_bitidx;
	mask <<= (BITS_PER_LONG - bitidx - 1);
	flags <<= (BITS_PER_LONG - bitidx - 1);

	word = READ_ONCE(bitmap[word_bitidx]);
	for (;;) {
		old_word = cmpxchg(&bitmap[word_bitidx], word, (word & ~mask) | flags);
		if (word == old_word)
			break;
		word = old_word;
	}
}

void set_pageblock_migratetype(struct page *page, int migratetype)
{
	if (unlikely(page_group_by_mobility_disabled &&
		     migratetype < MIGRATE_PCPTYPES))
		migratetype = MIGRATE_UNMOVABLE;

	set_pageblock_flags_group(page, (unsigned long)migratetype,
					PB_migrate, PB_migrate_end);
}

#ifdef CONFIG_DEBUG_VM
static int page_outside_zone_boundaries(struct zone *zone, struct page *page)
{
	int ret = 0;
	unsigned seq;
	unsigned long pfn = page_to_pfn(page);
	unsigned long sp, start_pfn;

	do {
		seq = zone_span_seqbegin(zone);
		start_pfn = zone->zone_start_pfn;
		sp = zone->spanned_pages;
		if (!zone_spans_pfn(zone, pfn))
			ret = 1;
	} while (zone_span_seqretry(zone, seq));

	if (ret)
		pr_err("page 0x%lx outside node %d zone %s [ 0x%lx - 0x%lx ]\n",
			pfn, zone_to_nid(zone), zone->name,
			start_pfn, start_pfn + sp);

	return ret;
}

static int page_is_consistent(struct zone *zone, struct page *page)
{
	if (!pfn_valid_within(page_to_pfn(page)))
		return 0;
	if (zone != page_zone(page))
		return 0;

	return 1;
}
/*
 * Temporary debugging check for pages not lying within a given zone.
 */
static int __maybe_unused bad_range(struct zone *zone, struct page *page)
{
	if (page_outside_zone_boundaries(zone, page))
		return 1;
	if (!page_is_consistent(zone, page))
		return 1;

	return 0;
}
#else
static inline int __maybe_unused bad_range(struct zone *zone, struct page *page)
{
	return 0;
}
#endif

static void bad_page(struct page *page, const char *reason,
		unsigned long bad_flags)
{
	static unsigned long resume;
	static unsigned long nr_shown;
	static unsigned long nr_unshown;

	/*
	 * Allow a burst of 60 reports, then keep quiet for that minute;
	 * or allow a steady drip of one report per second.
	 */
	if (nr_shown == 60) {
		if (time_before(jiffies, resume)) {
			nr_unshown++;
			goto out;
		}
		if (nr_unshown) {
			pr_alert(
			      "BUG: Bad page state: %lu messages suppressed\n",
				nr_unshown);
			nr_unshown = 0;
		}
		nr_shown = 0;
	}
	if (nr_shown++ == 0)
		resume = jiffies + 60 * HZ;

	pr_alert("BUG: Bad page state in process %s  pfn:%05lx\n",
		current->comm, page_to_pfn(page));
	__dump_page(page, reason);
	bad_flags &= page->flags;
	if (bad_flags)
		pr_alert("bad because of flags: %#lx(%pGp)\n",
						bad_flags, &bad_flags);
	dump_page_owner(page);

	print_modules();
	dump_stack();
out:
	/* Leave bad fields for debug, except PageBuddy could make trouble */
	page_mapcount_reset(page); /* remove PageBuddy */
	add_taint(TAINT_BAD_PAGE, LOCKDEP_NOW_UNRELIABLE);
}

/*
 * Higher-order pages are called "compound pages".  They are structured thusly:
 *
 * The first PAGE_SIZE page is called the "head page" and have PG_head set.
 *
 * The remaining PAGE_SIZE pages are called "tail pages". PageTail() is encoded
 * in bit 0 of page->compound_head. The rest of bits is pointer to head page.
 *
 * The first tail page's ->compound_dtor holds the offset in array of compound
 * page destructors. See compound_page_dtors.
 *
 * The first tail page's ->compound_order holds the order of allocation.
 * This usage means that zero-order pages may not be compound.
 */

void free_compound_page(struct page *page)
{
	__free_pages_ok(page, compound_order(page));
}

void prep_compound_page(struct page *page, unsigned int order)
{
	int i;
	int nr_pages = 1 << order;

	set_compound_page_dtor(page, COMPOUND_PAGE_DTOR);
	set_compound_order(page, order);
	__SetPageHead(page);
	for (i = 1; i < nr_pages; i++) {
		struct page *p = page + i;
		set_page_count(p, 0);
		p->mapping = TAIL_MAPPING;
		set_compound_head(p, page);
	}
	atomic_set(compound_mapcount_ptr(page), -1);
}

#ifdef CONFIG_DEBUG_PAGEALLOC
unsigned int _debug_guardpage_minorder;
bool _debug_pagealloc_enabled __read_mostly
			= IS_ENABLED(CONFIG_DEBUG_PAGEALLOC_ENABLE_DEFAULT);
EXPORT_SYMBOL(_debug_pagealloc_enabled);
bool _debug_guardpage_enabled __read_mostly;

static int __init early_debug_pagealloc(char *buf)
{
	if (!buf)
		return -EINVAL;
	return kstrtobool(buf, &_debug_pagealloc_enabled);
}
early_param("debug_pagealloc", early_debug_pagealloc);

static bool need_debug_guardpage(void)
{
	/* If we don't use debug_pagealloc, we don't need guard page */
	if (!debug_pagealloc_enabled())
		return false;

	if (!debug_guardpage_minorder())
		return false;

	return true;
}

static void init_debug_guardpage(void)
{
	if (!debug_pagealloc_enabled())
		return;

	if (!debug_guardpage_minorder())
		return;

	_debug_guardpage_enabled = true;
}

struct page_ext_operations debug_guardpage_ops = {
	.need = need_debug_guardpage,
	.init = init_debug_guardpage,
};

static int __init debug_guardpage_minorder_setup(char *buf)
{
	unsigned long res;

	if (kstrtoul(buf, 10, &res) < 0 ||  res > MAX_ORDER / 2) {
		pr_err("Bad debug_guardpage_minorder value\n");
		return 0;
	}
	_debug_guardpage_minorder = res;
	pr_info("Setting debug_guardpage_minorder to %lu\n", res);
	return 0;
}
early_param("debug_guardpage_minorder", debug_guardpage_minorder_setup);

static inline bool set_page_guard(struct zone *zone, struct page *page,
				unsigned int order, int migratetype)
{
	struct page_ext *page_ext;

	if (!debug_guardpage_enabled())
		return false;

	if (order >= debug_guardpage_minorder())
		return false;

	page_ext = lookup_page_ext(page);
	if (unlikely(!page_ext))
		return false;

	__set_bit(PAGE_EXT_DEBUG_GUARD, &page_ext->flags);

	INIT_LIST_HEAD(&page->lru);
	set_page_private(page, order);
	/* Guard pages are not available for any usage */
	__mod_zone_freepage_state(zone, -(1 << order), migratetype);

	return true;
}

static inline void clear_page_guard(struct zone *zone, struct page *page,
				unsigned int order, int migratetype)
{
	struct page_ext *page_ext;

	if (!debug_guardpage_enabled())
		return;

	page_ext = lookup_page_ext(page);
	if (unlikely(!page_ext))
		return;

	__clear_bit(PAGE_EXT_DEBUG_GUARD, &page_ext->flags);

	set_page_private(page, 0);
	if (!is_migrate_isolate(migratetype))
		__mod_zone_freepage_state(zone, (1 << order), migratetype);
}
#else
struct page_ext_operations debug_guardpage_ops;
static inline bool set_page_guard(struct zone *zone, struct page *page,
			unsigned int order, int migratetype) { return false; }
static inline void clear_page_guard(struct zone *zone, struct page *page,
				unsigned int order, int migratetype) {}
#endif

static inline void set_page_order(struct page *page, unsigned int order)
{
	set_page_private(page, order);
	__SetPageBuddy(page);
}

static inline void rmv_page_order(struct page *page)
{
	__ClearPageBuddy(page);
	set_page_private(page, 0);
}

/*
 * This function checks whether a page is free && is the buddy
 * we can coalesce a page and its buddy if
 * (a) the buddy is not in a hole (check before calling!) &&
 * (b) the buddy is in the buddy system &&
 * (c) a page and its buddy have the same order &&
 * (d) a page and its buddy are in the same zone.
 *
 * For recording whether a page is in the buddy system, we set PageBuddy.
 * Setting, clearing, and testing PageBuddy is serialized by zone->lock.
 *
 * For recording page's order, we use page_private(page).
 */
static inline bool page_is_buddy(struct page *page, struct page *buddy,
							unsigned int order)
{
	if (!page_is_guard(buddy) && !PageBuddy(buddy))
		return false;

	if (page_order(buddy) != order)
		return false;

	/*
	 * zone check is done late to avoid uselessly calculating
	 * zone/node ids for pages that could never merge.
	 */
	if (page_zone_id(page) != page_zone_id(buddy))
		return false;

	VM_BUG_ON_PAGE(page_count(buddy) != 0, buddy);

	return true;
}

#ifdef CONFIG_COMPACTION
static inline struct capture_control *task_capc(struct zone *zone)
{
	struct capture_control *capc = current->capture_control;

	return unlikely(capc) &&
		!(current->flags & PF_KTHREAD) &&
		!capc->page &&
		capc->cc->zone == zone ? capc : NULL;
}

static inline bool
compaction_capture(struct capture_control *capc, struct page *page,
		   int order, int migratetype)
{
	if (!capc || order != capc->cc->order)
		return false;

	/* Do not accidentally pollute CMA or isolated regions*/
	if (is_migrate_cma(migratetype) ||
	    is_migrate_isolate(migratetype))
		return false;

	/*
	 * Do not let lower order allocations polluate a movable pageblock.
	 * This might let an unmovable request use a reclaimable pageblock
	 * and vice-versa but no more than normal fallback logic which can
	 * have trouble finding a high-order free page.
	 */
	if (order < pageblock_order && migratetype == MIGRATE_MOVABLE)
		return false;

	capc->page = page;
	return true;
}

#else
static inline struct capture_control *task_capc(struct zone *zone)
{
	return NULL;
}

static inline bool
compaction_capture(struct capture_control *capc, struct page *page,
		   int order, int migratetype)
{
	return false;
}
#endif /* CONFIG_COMPACTION */

/*
 * Freeing function for a buddy system allocator.
 *
 * The concept of a buddy system is to maintain direct-mapped table
 * (containing bit values) for memory blocks of various "orders".
 * The bottom level table contains the map for the smallest allocatable
 * units of memory (here, pages), and each level above it describes
 * pairs of units from the levels below, hence, "buddies".
 * At a high level, all that happens here is marking the table entry
 * at the bottom level available, and propagating the changes upward
 * as necessary, plus some accounting needed to play nicely with other
 * parts of the VM system.
 * At each level, we keep a list of pages, which are heads of continuous
 * free pages of length of (1 << order) and marked with PageBuddy.
 * Page's order is recorded in page_private(page) field.
 * So when we are allocating or freeing one, we can derive the state of the
 * other.  That is, if we allocate a small block, and both were
 * free, the remainder of the region must be split into blocks.
 * If a block is freed, and its buddy is also free, then this
 * triggers coalescing into a block of larger size.
 *
 * -- nyc
 */

static inline void __free_one_page(struct page *page,
		unsigned long pfn,
		struct zone *zone, unsigned int order,
		int migratetype)
{
	unsigned long combined_pfn;
	unsigned long uninitialized_var(buddy_pfn);
	struct page *buddy;
	unsigned int max_order;
	struct capture_control *capc = task_capc(zone);

	max_order = min_t(unsigned int, MAX_ORDER - 1, pageblock_order);

	VM_BUG_ON(!zone_is_initialized(zone));
	VM_BUG_ON_PAGE(page->flags & PAGE_FLAGS_CHECK_AT_PREP, page);

	VM_BUG_ON(migratetype == -1);
	if (likely(!is_migrate_isolate(migratetype)))
		__mod_zone_freepage_state(zone, 1 << order, migratetype);

	VM_BUG_ON_PAGE(pfn & ((1 << order) - 1), page);
	VM_BUG_ON_PAGE(bad_range(zone, page), page);

continue_merging:
	while (order < max_order) {
		if (compaction_capture(capc, page, order, migratetype)) {
			__mod_zone_freepage_state(zone, -(1 << order),
								migratetype);
			return;
		}
		buddy_pfn = __find_buddy_pfn(pfn, order);
		buddy = page + (buddy_pfn - pfn);

		if (!pfn_valid_within(buddy_pfn))
			goto done_merging;
		if (!page_is_buddy(page, buddy, order))
			goto done_merging;
		/*
		 * Our buddy is free or it is CONFIG_DEBUG_PAGEALLOC guard page,
		 * merge with it and move up one order.
		 */
		if (page_is_guard(buddy)) {
			clear_page_guard(zone, buddy, order, migratetype);
		} else {
			list_del(&buddy->lru);
			zone->free_area[order].nr_free--;
			rmv_page_order(buddy);
		}
		combined_pfn = buddy_pfn & pfn;
		page = page + (combined_pfn - pfn);
		pfn = combined_pfn;
		order++;
	}
	if (order < MAX_ORDER - 1) {
		/* If we are here, it means order is >= pageblock_order.
		 * We want to prevent merge between freepages on isolate
		 * pageblock and normal pageblock. Without this, pageblock
		 * isolation could cause incorrect freepage or CMA accounting.
		 *
		 * We don't want to hit this code for the more frequent
		 * low-order merging.
		 */
		if (unlikely(has_isolate_pageblock(zone))) {
			int buddy_mt;

			buddy_pfn = __find_buddy_pfn(pfn, order);
			buddy = page + (buddy_pfn - pfn);
			buddy_mt = get_pageblock_migratetype(buddy);

			if (migratetype != buddy_mt
					&& (is_migrate_isolate(migratetype) ||
						is_migrate_isolate(buddy_mt)))
				goto done_merging;
		}
		max_order = order + 1;
		goto continue_merging;
	}

done_merging:
	set_page_order(page, order);

	/*
	 * If this is not the largest possible page, check if the buddy
	 * of the next-highest order is free. If it is, it's possible
	 * that pages are being freed that will coalesce soon. In case,
	 * that is happening, add the free page to the tail of the list
	 * so it's less likely to be used soon and more likely to be merged
	 * as a higher order page
	 */
	if ((order < MAX_ORDER-2) && pfn_valid_within(buddy_pfn)) {
		struct page *higher_page, *higher_buddy;
		combined_pfn = buddy_pfn & pfn;
		higher_page = page + (combined_pfn - pfn);
		buddy_pfn = __find_buddy_pfn(combined_pfn, order + 1);
		higher_buddy = higher_page + (buddy_pfn - combined_pfn);
		if (pfn_valid_within(buddy_pfn) &&
		    page_is_buddy(higher_page, higher_buddy, order + 1)) {
			list_add_tail(&page->lru,
				&zone->free_area[order].free_list[migratetype]);
			goto out;
		}
	}

	list_add(&page->lru, &zone->free_area[order].free_list[migratetype]);
out:
	zone->free_area[order].nr_free++;
}

/*
 * A bad page could be due to a number of fields. Instead of multiple branches,
 * try and check multiple fields with one check. The caller must do a detailed
 * check if necessary.
 */
static inline bool page_expected_state(struct page *page,
					unsigned long check_flags)
{
	if (unlikely(atomic_read(&page->_mapcount) != -1))
		return false;

	if (unlikely((unsigned long)page->mapping |
			page_ref_count(page) |
#ifdef CONFIG_MEMCG
			(unsigned long)page->mem_cgroup |
#endif
			(page->flags & check_flags)))
		return false;

	return true;
}

static void free_pages_check_bad(struct page *page)
{
	const char *bad_reason;
	unsigned long bad_flags;

	bad_reason = NULL;
	bad_flags = 0;

	if (unlikely(atomic_read(&page->_mapcount) != -1))
		bad_reason = "nonzero mapcount";
	if (unlikely(page->mapping != NULL))
		bad_reason = "non-NULL mapping";
	if (unlikely(page_ref_count(page) != 0))
		bad_reason = "nonzero _refcount";
	if (unlikely(page->flags & PAGE_FLAGS_CHECK_AT_FREE)) {
		bad_reason = "PAGE_FLAGS_CHECK_AT_FREE flag(s) set";
		bad_flags = PAGE_FLAGS_CHECK_AT_FREE;
	}
#ifdef CONFIG_MEMCG
	if (unlikely(page->mem_cgroup))
		bad_reason = "page still charged to cgroup";
#endif
	bad_page(page, bad_reason, bad_flags);
}

static inline int free_pages_check(struct page *page)
{
	if (likely(page_expected_state(page, PAGE_FLAGS_CHECK_AT_FREE)))
		return 0;

	/* Something has gone sideways, find it */
	free_pages_check_bad(page);
	return 1;
}

static int free_tail_pages_check(struct page *head_page, struct page *page)
{
	int ret = 1;

	/*
	 * We rely page->lru.next never has bit 0 set, unless the page
	 * is PageTail(). Let's make sure that's true even for poisoned ->lru.
	 */
	BUILD_BUG_ON((unsigned long)LIST_POISON1 & 1);

	if (!IS_ENABLED(CONFIG_DEBUG_VM)) {
		ret = 0;
		goto out;
	}
	switch (page - head_page) {
	case 1:
		/* the first tail page: ->mapping may be compound_mapcount() */
		if (unlikely(compound_mapcount(page))) {
			bad_page(page, "nonzero compound_mapcount", 0);
			goto out;
		}
		break;
	case 2:
		/*
		 * the second tail page: ->mapping is
		 * deferred_list.next -- ignore value.
		 */
		break;
	default:
		if (page->mapping != TAIL_MAPPING) {
			bad_page(page, "corrupted mapping in tail page", 0);
			goto out;
		}
		break;
	}
	if (unlikely(!PageTail(page))) {
		bad_page(page, "PageTail not set", 0);
		goto out;
	}
	if (unlikely(compound_head(page) != head_page)) {
		bad_page(page, "compound_head not consistent", 0);
		goto out;
	}
	ret = 0;
out:
	page->mapping = NULL;
	clear_compound_head(page);
	return ret;
}

static void kernel_init_free_pages(struct page *page, int numpages)
{
	int i;

	for (i = 0; i < numpages; i++)
		clear_highpage(page + i);
}

static __always_inline bool free_pages_prepare(struct page *page,
					unsigned int order, bool check_free)
{
	int bad = 0;

	VM_BUG_ON_PAGE(PageTail(page), page);

	trace_mm_page_free(page, order);

	/*
	 * Check tail pages before head page information is cleared to
	 * avoid checking PageCompound for order-0 pages.
	 */
	if (unlikely(order)) {
		bool compound = PageCompound(page);
		int i;

		VM_BUG_ON_PAGE(compound && compound_order(page) != order, page);

		if (compound)
			ClearPageDoubleMap(page);
		for (i = 1; i < (1 << order); i++) {
			if (compound)
				bad += free_tail_pages_check(page, page + i);
			if (unlikely(free_pages_check(page + i))) {
				bad++;
				continue;
			}
			(page + i)->flags &= ~PAGE_FLAGS_CHECK_AT_PREP;
		}
	}
	if (PageMappingFlags(page))
		page->mapping = NULL;
	if (memcg_kmem_enabled() && PageKmemcg(page))
		memcg_kmem_uncharge(page, order);
	if (check_free)
		bad += free_pages_check(page);
	if (bad)
		return false;

	page_cpupid_reset_last(page);
	page->flags &= ~PAGE_FLAGS_CHECK_AT_PREP;
	reset_page_owner(page, order);

	if (!PageHighMem(page)) {
		debug_check_no_locks_freed(page_address(page),
					   PAGE_SIZE << order);
		debug_check_no_obj_freed(page_address(page),
					   PAGE_SIZE << order);
	}
	arch_free_page(page, order);
	if (want_init_on_free())
		kernel_init_free_pages(page, 1 << order);

	kernel_poison_pages(page, 1 << order, 0);
	kernel_map_pages(page, 1 << order, 0);
	kasan_free_nondeferred_pages(page, order);

	return true;
}

#ifdef CONFIG_DEBUG_VM
static inline bool free_pcp_prepare(struct page *page)
{
	return free_pages_prepare(page, 0, true);
}

static inline bool bulkfree_pcp_prepare(struct page *page)
{
	return false;
}
#else
static bool free_pcp_prepare(struct page *page)
{
	return free_pages_prepare(page, 0, false);
}

static bool bulkfree_pcp_prepare(struct page *page)
{
	return free_pages_check(page);
}
#endif /* CONFIG_DEBUG_VM */

static inline void prefetch_buddy(struct page *page)
{
	unsigned long pfn = page_to_pfn(page);
	unsigned long buddy_pfn = __find_buddy_pfn(pfn, 0);
	struct page *buddy = page + (buddy_pfn - pfn);

	prefetch(buddy);
}

/*
 * Frees a number of pages which have been collected from the pcp lists.
 * Assumes all pages on list are in same zone, and of same order.
 * count is the number of pages to free.
 *
 * If the zone was previously in an "all pages pinned" state then look to
 * see if this freeing clears that state.
 *
 * And clear the zone's pages_scanned counter, to hold off the "all pages are
 * pinned" detection logic.
 */
static void free_pcppages_bulk(struct zone *zone, struct list_head *head,
			       bool zone_retry)
{
	bool isolated_pageblocks;
	struct page *page, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&zone->lock, flags);
	isolated_pageblocks = has_isolate_pageblock(zone);

	/*
	 * Use safe version since after __free_one_page(),
	 * page->lru.next will not point to original list.
	 */
	list_for_each_entry_safe(page, tmp, head, lru) {
		int mt = get_pcppage_migratetype(page);

		if (page_zone(page) != zone) {
			/*
			 * free_unref_page_list() sorts pages by zone. If we end
			 * up with pages from a different NUMA nodes belonging
			 * to the same ZONE index then we need to redo with the
			 * correct ZONE pointer. Skip the page for now, redo it
			 * on the next iteration.
			 */
			WARN_ON_ONCE(zone_retry == false);
			if (zone_retry)
				continue;
		}

		/* MIGRATE_ISOLATE page should not go to pcplists */
		VM_BUG_ON_PAGE(is_migrate_isolate(mt), page);
		/* Pageblock could have been isolated meanwhile */
		if (unlikely(isolated_pageblocks))
			mt = get_pageblock_migratetype(page);

		list_del(&page->lru);
		__free_one_page(page, page_to_pfn(page), zone, 0, mt);
		trace_mm_page_pcpu_drain(page, 0, mt);
	}
	spin_unlock_irqrestore(&zone->lock, flags);
}

static void isolate_pcp_pages(int count, struct per_cpu_pages *pcp,
			      struct list_head *dst)

{
	int migratetype = 0;
	int batch_free = 0;
	int prefetch_nr = 0;
	struct page *page;

	/*
	 * Ensure proper count is passed which otherwise would stuck in the
	 * below while (list_empty(list)) loop.
	 */
	count = min(pcp->count, count);
	while (count) {
		struct list_head *list;

		/*
		 * Remove pages from lists in a round-robin fashion. A
		 * batch_free count is maintained that is incremented when an
		 * empty list is encountered.  This is so more pages are freed
		 * off fuller lists instead of spinning excessively around empty
		 * lists
		 */
		do {
			batch_free++;
			if (++migratetype == MIGRATE_PCPTYPES)
				migratetype = 0;
			list = &pcp->lists[migratetype];
		} while (list_empty(list));

		/* This is the only non-empty list. Free them all. */
		if (batch_free == MIGRATE_PCPTYPES)
			batch_free = count;

		do {
			page = list_last_entry(list, struct page, lru);
			/* must delete to avoid corrupting pcp list */
			list_del(&page->lru);
			pcp->count--;

			if (bulkfree_pcp_prepare(page))
				continue;

			list_add_tail(&page->lru, dst);

			/*
			 * We are going to put the page back to the global
			 * pool, prefetch its buddy to speed up later access
			 * under zone->lock. It is believed the overhead of
			 * an additional test and calculating buddy_pfn here
			 * can be offset by reduced memory latency later. To
			 * avoid excessive prefetching due to large count, only
			 * prefetch buddy for the first pcp->batch nr of pages.
			 */
			if (prefetch_nr++ < pcp->batch)
				prefetch_buddy(page);
		} while (--count && --batch_free && !list_empty(list));
	}
}

static void free_one_page(struct zone *zone,
				struct page *page, unsigned long pfn,
				unsigned int order,
				int migratetype)
{
	spin_lock(&zone->lock);
	if (unlikely(has_isolate_pageblock(zone) ||
		is_migrate_isolate(migratetype))) {
		migratetype = get_pfnblock_migratetype(page, pfn);
	}
	__free_one_page(page, pfn, zone, order, migratetype);
	spin_unlock(&zone->lock);
}

static void __meminit __init_single_page(struct page *page, unsigned long pfn,
				unsigned long zone, int nid)
{
	mm_zero_struct_page(page);
	set_page_links(page, zone, nid, pfn);
	init_page_count(page);
	page_mapcount_reset(page);
	page_cpupid_reset_last(page);
	page_kasan_tag_reset(page);

	INIT_LIST_HEAD(&page->lru);
#ifdef WANT_PAGE_VIRTUAL
	/* The shift won't overflow because ZONE_NORMAL is below 4G. */
	if (!is_highmem_idx(zone))
		set_page_address(page, __va(pfn << PAGE_SHIFT));
#endif
}

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
static void __meminit init_reserved_page(unsigned long pfn)
{
	pg_data_t *pgdat;
	int nid, zid;

	if (!early_page_uninitialised(pfn))
		return;

	nid = early_pfn_to_nid(pfn);
	pgdat = NODE_DATA(nid);

	for (zid = 0; zid < MAX_NR_ZONES; zid++) {
		struct zone *zone = &pgdat->node_zones[zid];

		if (pfn >= zone->zone_start_pfn && pfn < zone_end_pfn(zone))
			break;
	}
	__init_single_page(pfn_to_page(pfn), pfn, zid, nid);
}
#else
static inline void init_reserved_page(unsigned long pfn)
{
}
#endif /* CONFIG_DEFERRED_STRUCT_PAGE_INIT */

/*
 * Initialised pages do not have PageReserved set. This function is
 * called for each range allocated by the bootmem allocator and
 * marks the pages PageReserved. The remaining valid pages are later
 * sent to the buddy page allocator.
 */
void __meminit reserve_bootmem_region(phys_addr_t start, phys_addr_t end)
{
	unsigned long start_pfn = PFN_DOWN(start);
	unsigned long end_pfn = PFN_UP(end);

	for (; start_pfn < end_pfn; start_pfn++) {
		if (pfn_valid(start_pfn)) {
			struct page *page = pfn_to_page(start_pfn);

			init_reserved_page(start_pfn);

			/* Avoid false-positive PageTail() */
			INIT_LIST_HEAD(&page->lru);

			SetPageReserved(page);
		}
	}
}

#ifdef CONFIG_HUGEPAGE_POOL
static void  __free_pages_ok(struct page *page, unsigned int order)
{
	___free_pages_ok(page, order, false);
}

void ___free_pages_ok(struct page *page, unsigned int order,
		      bool skip_hugepage_pool)
#else
static void __free_pages_ok(struct page *page, unsigned int order)
#endif
{
	unsigned long flags;
	int migratetype;
	unsigned long pfn = page_to_pfn(page);

#ifdef CONFIG_HUGEPAGE_POOL
	if (!skip_hugepage_pool && !free_pages_prepare(page, order, true))
		return;
#else
	if (!free_pages_prepare(page, order, true))
		return;
#endif

#ifdef CONFIG_HUGEPAGE_POOL
	if (!skip_hugepage_pool && order == HUGEPAGE_ORDER &&
	    insert_hugepage_pool(page, order))
		return;
#endif
	migratetype = get_pfnblock_migratetype(page, pfn);
	local_irq_save(flags);
	__count_vm_events(PGFREE, 1 << order);
	free_one_page(page_zone(page), page, pfn, order, migratetype);
	local_irq_restore(flags);
}

static void __free_pages_boot_core(struct page *page, unsigned int order)
{
	unsigned int nr_pages = 1 << order;
	struct page *p = page;
	unsigned int loop;

	prefetchw(p);
	for (loop = 0; loop < (nr_pages - 1); loop++, p++) {
		prefetchw(p + 1);
		__ClearPageReserved(p);
		set_page_count(p, 0);
	}
	__ClearPageReserved(p);
	set_page_count(p, 0);

	page_zone(page)->managed_pages += nr_pages;
	set_page_refcounted(page);
	__free_pages(page, order);
}

#if defined(CONFIG_HAVE_ARCH_EARLY_PFN_TO_NID) || \
	defined(CONFIG_HAVE_MEMBLOCK_NODE_MAP)

static struct mminit_pfnnid_cache early_pfnnid_cache __meminitdata;

int __meminit early_pfn_to_nid(unsigned long pfn)
{
	static DEFINE_SPINLOCK(early_pfn_lock);
	int nid;

	spin_lock(&early_pfn_lock);
	nid = __early_pfn_to_nid(pfn, &early_pfnnid_cache);
	if (nid < 0)
		nid = first_online_node;
	spin_unlock(&early_pfn_lock);

	return nid;
}
#endif

#ifdef CONFIG_NODES_SPAN_OTHER_NODES
static inline bool __meminit __maybe_unused
meminit_pfn_in_nid(unsigned long pfn, int node,
		   struct mminit_pfnnid_cache *state)
{
	int nid;

	nid = __early_pfn_to_nid(pfn, state);
	if (nid >= 0 && nid != node)
		return false;
	return true;
}

/* Only safe to use early in boot when initialisation is single-threaded */
static inline bool __meminit early_pfn_in_nid(unsigned long pfn, int node)
{
	return meminit_pfn_in_nid(pfn, node, &early_pfnnid_cache);
}

#else

static inline bool __meminit early_pfn_in_nid(unsigned long pfn, int node)
{
	return true;
}
static inline bool __meminit  __maybe_unused
meminit_pfn_in_nid(unsigned long pfn, int node,
		   struct mminit_pfnnid_cache *state)
{
	return true;
}
#endif


void __free_pages_bootmem(struct page *page, unsigned long pfn,
							unsigned int order)
{
	if (early_page_uninitialised(pfn))
		return;
	return __free_pages_boot_core(page, order);
}

/*
 * Check that the whole (or subset of) a pageblock given by the interval of
 * [start_pfn, end_pfn) is valid and within the same zone, before scanning it
 * with the migration of free compaction scanner. The scanners then need to
 * use only pfn_valid_within() check for arches that allow holes within
 * pageblocks.
 *
 * Return struct page pointer of start_pfn, or NULL if checks were not passed.
 *
 * It's possible on some configurations to have a setup like node0 node1 node0
 * i.e. it's possible that all pages within a zones range of pages do not
 * belong to a single zone. We assume that a border between node0 and node1
 * can occur within a single pageblock, but not a node0 node1 node0
 * interleaving within a single pageblock. It is therefore sufficient to check
 * the first and last page of a pageblock and avoid checking each individual
 * page in a pageblock.
 */
struct page *__pageblock_pfn_to_page(unsigned long start_pfn,
				     unsigned long end_pfn, struct zone *zone)
{
	struct page *start_page;
	struct page *end_page;

	/* end_pfn is one past the range we are checking */
	end_pfn--;

	if (!pfn_valid(start_pfn) || !pfn_valid(end_pfn))
		return NULL;

	start_page = pfn_to_online_page(start_pfn);
	if (!start_page)
		return NULL;

	if (page_zone(start_page) != zone)
		return NULL;

	end_page = pfn_to_page(end_pfn);

	/* This gives a shorter code than deriving page_zone(end_page) */
	if (page_zone_id(start_page) != page_zone_id(end_page))
		return NULL;

	return start_page;
}

void set_zone_contiguous(struct zone *zone)
{
	unsigned long block_start_pfn = zone->zone_start_pfn;
	unsigned long block_end_pfn;

	block_end_pfn = ALIGN(block_start_pfn + 1, pageblock_nr_pages);
	for (; block_start_pfn < zone_end_pfn(zone);
			block_start_pfn = block_end_pfn,
			 block_end_pfn += pageblock_nr_pages) {

		block_end_pfn = min(block_end_pfn, zone_end_pfn(zone));

		if (!__pageblock_pfn_to_page(block_start_pfn,
					     block_end_pfn, zone))
			return;
		cond_resched();
	}

	/* We confirm that there is no hole */
	zone->contiguous = true;
}

void clear_zone_contiguous(struct zone *zone)
{
	zone->contiguous = false;
}

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
static void __init deferred_free_range(unsigned long pfn,
				       unsigned long nr_pages)
{
	struct page *page;
	unsigned long i;

	if (!nr_pages)
		return;

	page = pfn_to_page(pfn);

	/* Free a large naturally-aligned chunk if possible */
	if (nr_pages == pageblock_nr_pages &&
	    (pfn & (pageblock_nr_pages - 1)) == 0) {
		set_pageblock_migratetype(page, MIGRATE_MOVABLE);
		__free_pages_boot_core(page, pageblock_order);
		return;
	}

	for (i = 0; i < nr_pages; i++, page++, pfn++) {
		if ((pfn & (pageblock_nr_pages - 1)) == 0)
			set_pageblock_migratetype(page, MIGRATE_MOVABLE);
		__free_pages_boot_core(page, 0);
	}
}

/* Completion tracking for deferred_init_memmap() threads */
static atomic_t pgdat_init_n_undone __initdata;
static __initdata DECLARE_COMPLETION(pgdat_init_all_done_comp);

static inline void __init pgdat_init_report_one_done(void)
{
	if (atomic_dec_and_test(&pgdat_init_n_undone))
		complete(&pgdat_init_all_done_comp);
}

/*
 * Returns true if page needs to be initialized or freed to buddy allocator.
 *
 * First we check if pfn is valid on architectures where it is possible to have
 * holes within pageblock_nr_pages. On systems where it is not possible, this
 * function is optimized out.
 *
 * Then, we check if a current large page is valid by only checking the validity
 * of the head pfn.
 *
 * Finally, meminit_pfn_in_nid is checked on systems where pfns can interleave
 * within a node: a pfn is between start and end of a node, but does not belong
 * to this memory node.
 */
static inline bool __init
deferred_pfn_valid(int nid, unsigned long pfn,
		   struct mminit_pfnnid_cache *nid_init_state)
{
	if (!pfn_valid_within(pfn))
		return false;
	if (!(pfn & (pageblock_nr_pages - 1)) && !pfn_valid(pfn))
		return false;
	if (!meminit_pfn_in_nid(pfn, nid, nid_init_state))
		return false;
	return true;
}

/*
 * Free pages to buddy allocator. Try to free aligned pages in
 * pageblock_nr_pages sizes.
 */
static void __init deferred_free_pages(int nid, int zid, unsigned long pfn,
				       unsigned long end_pfn)
{
	struct mminit_pfnnid_cache nid_init_state = { };
	unsigned long nr_pgmask = pageblock_nr_pages - 1;
	unsigned long nr_free = 0;

	for (; pfn < end_pfn; pfn++) {
		if (!deferred_pfn_valid(nid, pfn, &nid_init_state)) {
			deferred_free_range(pfn - nr_free, nr_free);
			nr_free = 0;
		} else if (!(pfn & nr_pgmask)) {
			deferred_free_range(pfn - nr_free, nr_free);
			nr_free = 1;
			touch_nmi_watchdog();
		} else {
			nr_free++;
		}
	}
	/* Free the last block of pages to allocator */
	deferred_free_range(pfn - nr_free, nr_free);
}

/*
 * Initialize struct pages.  We minimize pfn page lookups and scheduler checks
 * by performing it only once every pageblock_nr_pages.
 * Return number of pages initialized.
 */
static unsigned long  __init deferred_init_pages(int nid, int zid,
						 unsigned long pfn,
						 unsigned long end_pfn)
{
	struct mminit_pfnnid_cache nid_init_state = { };
	unsigned long nr_pgmask = pageblock_nr_pages - 1;
	unsigned long nr_pages = 0;
	struct page *page = NULL;

	for (; pfn < end_pfn; pfn++) {
		if (!deferred_pfn_valid(nid, pfn, &nid_init_state)) {
			page = NULL;
			continue;
		} else if (!page || !(pfn & nr_pgmask)) {
			page = pfn_to_page(pfn);
			touch_nmi_watchdog();
		} else {
			page++;
		}
		__init_single_page(page, pfn, zid, nid);
		nr_pages++;
	}
	return (nr_pages);
}

/* Initialise remaining memory on a node */
static int __init deferred_init_memmap(void *data)
{
	pg_data_t *pgdat = data;
	int nid = pgdat->node_id;
	unsigned long start = jiffies;
	unsigned long nr_pages = 0;
	unsigned long spfn, epfn, first_init_pfn, flags;
	phys_addr_t spa, epa;
	int zid;
	struct zone *zone;
	const struct cpumask *cpumask = cpumask_of_node(pgdat->node_id);
	u64 i;

	/* Bind memory initialisation thread to a local node if possible */
	if (!cpumask_empty(cpumask))
		set_cpus_allowed_ptr(current, cpumask);

	pgdat_resize_lock(pgdat, &flags);
	first_init_pfn = pgdat->first_deferred_pfn;
	if (first_init_pfn == ULONG_MAX) {
		pgdat_resize_unlock(pgdat, &flags);
		pgdat_init_report_one_done();
		return 0;
	}

	/* Sanity check boundaries */
	BUG_ON(pgdat->first_deferred_pfn < pgdat->node_start_pfn);
	BUG_ON(pgdat->first_deferred_pfn > pgdat_end_pfn(pgdat));
	pgdat->first_deferred_pfn = ULONG_MAX;

	/*
	 * Once we unlock here, the zone cannot be grown anymore, thus if an
	 * interrupt thread must allocate this early in boot, zone must be
	 * pre-grown prior to start of deferred page initialization.
	 */
	pgdat_resize_unlock(pgdat, &flags);

	/* Only the highest zone is deferred so find it */
	for (zid = 0; zid < MAX_NR_ZONES; zid++) {
		zone = pgdat->node_zones + zid;
		if (first_init_pfn < zone_end_pfn(zone))
			break;
	}
	first_init_pfn = max(zone->zone_start_pfn, first_init_pfn);

	/*
	 * Initialize and free pages. We do it in two loops: first we initialize
	 * struct page, than free to buddy allocator, because while we are
	 * freeing pages we can access pages that are ahead (computing buddy
	 * page in __free_one_page()).
	 */
	for_each_free_mem_range(i, nid, MEMBLOCK_NONE, &spa, &epa, NULL) {
		spfn = max_t(unsigned long, first_init_pfn, PFN_UP(spa));
		epfn = min_t(unsigned long, zone_end_pfn(zone), PFN_DOWN(epa));
		nr_pages += deferred_init_pages(nid, zid, spfn, epfn);
	}
	for_each_free_mem_range(i, nid, MEMBLOCK_NONE, &spa, &epa, NULL) {
		spfn = max_t(unsigned long, first_init_pfn, PFN_UP(spa));
		epfn = min_t(unsigned long, zone_end_pfn(zone), PFN_DOWN(epa));
		deferred_free_pages(nid, zid, spfn, epfn);
	}

	/* Sanity check that the next zone really is unpopulated */
	WARN_ON(++zid < MAX_NR_ZONES && populated_zone(++zone));

	pr_info("node %d initialised, %lu pages in %ums\n", nid, nr_pages,
					jiffies_to_msecs(jiffies - start));

	pgdat_init_report_one_done();
	return 0;
}

/*
 * If this zone has deferred pages, try to grow it by initializing enough
 * deferred pages to satisfy the allocation specified by order, rounded up to
 * the nearest PAGES_PER_SECTION boundary.  So we're adding memory in increments
 * of SECTION_SIZE bytes by initializing struct pages in increments of
 * PAGES_PER_SECTION * sizeof(struct page) bytes.
 *
 * Return true when zone was grown, otherwise return false. We return true even
 * when we grow less than requested, to let the caller decide if there are
 * enough pages to satisfy the allocation.
 *
 * Note: We use noinline because this function is needed only during boot, and
 * it is called from a __ref function _deferred_grow_zone. This way we are
 * making sure that it is not inlined into permanent text section.
 */
static noinline bool __init
deferred_grow_zone(struct zone *zone, unsigned int order)
{
	int zid = zone_idx(zone);
	int nid = zone_to_nid(zone);
	pg_data_t *pgdat = NODE_DATA(nid);
	unsigned long nr_pages_needed = ALIGN(1 << order, PAGES_PER_SECTION);
	unsigned long nr_pages = 0;
	unsigned long first_init_pfn, spfn, epfn, t, flags;
	unsigned long first_deferred_pfn = pgdat->first_deferred_pfn;
	phys_addr_t spa, epa;
	u64 i;

	/* Only the last zone may have deferred pages */
	if (zone_end_pfn(zone) != pgdat_end_pfn(pgdat))
		return false;

	pgdat_resize_lock(pgdat, &flags);

	/*
	 * If someone grew this zone while we were waiting for spinlock, return
	 * true, as there might be enough pages already.
	 */
	if (first_deferred_pfn != pgdat->first_deferred_pfn) {
		pgdat_resize_unlock(pgdat, &flags);
		return true;
	}

	first_init_pfn = max(zone->zone_start_pfn, first_deferred_pfn);

	if (first_init_pfn >= pgdat_end_pfn(pgdat)) {
		pgdat_resize_unlock(pgdat, &flags);
		return false;
	}

	for_each_free_mem_range(i, nid, MEMBLOCK_NONE, &spa, &epa, NULL) {
		spfn = max_t(unsigned long, first_init_pfn, PFN_UP(spa));
		epfn = min_t(unsigned long, zone_end_pfn(zone), PFN_DOWN(epa));

		while (spfn < epfn && nr_pages < nr_pages_needed) {
			t = ALIGN(spfn + PAGES_PER_SECTION, PAGES_PER_SECTION);
			first_deferred_pfn = min(t, epfn);
			nr_pages += deferred_init_pages(nid, zid, spfn,
							first_deferred_pfn);
			spfn = first_deferred_pfn;
		}

		if (nr_pages >= nr_pages_needed)
			break;
	}

	for_each_free_mem_range(i, nid, MEMBLOCK_NONE, &spa, &epa, NULL) {
		spfn = max_t(unsigned long, first_init_pfn, PFN_UP(spa));
		epfn = min_t(unsigned long, first_deferred_pfn, PFN_DOWN(epa));
		deferred_free_pages(nid, zid, spfn, epfn);

		if (first_deferred_pfn == epfn)
			break;
	}
	pgdat->first_deferred_pfn = first_deferred_pfn;
	pgdat_resize_unlock(pgdat, &flags);

	return nr_pages > 0;
}

/*
 * deferred_grow_zone() is __init, but it is called from
 * get_page_from_freelist() during early boot until deferred_pages permanently
 * disables this call. This is why we have refdata wrapper to avoid warning,
 * and to ensure that the function body gets unloaded.
 */
static bool __ref
_deferred_grow_zone(struct zone *zone, unsigned int order)
{
	return deferred_grow_zone(zone, order);
}

#endif /* CONFIG_DEFERRED_STRUCT_PAGE_INIT */

void __init page_alloc_init_late(void)
{
	struct zone *zone;

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
	int nid;

	/* There will be num_node_state(N_MEMORY) threads */
	atomic_set(&pgdat_init_n_undone, num_node_state(N_MEMORY));
	for_each_node_state(nid, N_MEMORY) {
		kthread_run(deferred_init_memmap, NODE_DATA(nid), "pgdatinit%d", nid);
	}

	/* Block until all are initialised */
	wait_for_completion(&pgdat_init_all_done_comp);

	/*
	 * The number of managed pages has changed due to the initialisation
	 * so the pcpu batch and high limits needs to be updated or the limits
	 * will be artificially small.
	 */
	for_each_populated_zone(zone)
		zone_pcp_update(zone);

	/*
	 * We initialized the rest of the deferred pages.  Permanently disable
	 * on-demand struct page initialization.
	 */
	static_branch_disable(&deferred_pages);

	/* Reinit limits that are based on free pages after the kernel is up */
	files_maxfiles_init();
#endif
#ifdef CONFIG_ARCH_DISCARD_MEMBLOCK
	/* Discard memblock private memory */
	memblock_discard();
#endif

	for_each_populated_zone(zone)
		set_zone_contiguous(zone);
}

#ifdef CONFIG_CMA
/* Free whole pageblock and set its migration type to MIGRATE_CMA. */
void __init init_cma_reserved_pageblock(struct page *page)
{
	unsigned i = pageblock_nr_pages;
	struct page *p = page;

	do {
		__ClearPageReserved(p);
		set_page_count(p, 0);
	} while (++p, --i);

	set_pageblock_migratetype(page, MIGRATE_CMA);

	if (pageblock_order >= MAX_ORDER) {
		i = pageblock_nr_pages;
		p = page;
		do {
			set_page_refcounted(p);
			__free_pages(p, MAX_ORDER - 1);
			p += MAX_ORDER_NR_PAGES;
		} while (i -= MAX_ORDER_NR_PAGES);
	} else {
		set_page_refcounted(page);
		__free_pages(page, pageblock_order);
	}

	adjust_managed_page_count(page, pageblock_nr_pages);
}
#endif

/*
 * The order of subdivision here is critical for the IO subsystem.
 * Please do not alter this order without good reasons and regression
 * testing. Specifically, as large blocks of memory are subdivided,
 * the order in which smaller blocks are delivered depends on the order
 * they're subdivided in this function. This is the primary factor
 * influencing the order in which pages are delivered to the IO
 * subsystem according to empirical testing, and this is also justified
 * by considering the behavior of a buddy system containing a single
 * large block of memory acted on by a series of small allocations.
 * This behavior is a critical factor in sglist merging's success.
 *
 * -- nyc
 */
static inline void expand(struct zone *zone, struct page *page,
	int low, int high, struct free_area *area,
	int migratetype)
{
	unsigned long size = 1 << high;

	while (high > low) {
		area--;
		high--;
		size >>= 1;
		VM_BUG_ON_PAGE(bad_range(zone, &page[size]), &page[size]);

		/*
		 * Mark as guard pages (or page), that will allow to
		 * merge back to allocator when buddy will be freed.
		 * Corresponding page table entries will not be touched,
		 * pages will stay not present in virtual address space
		 */
		if (set_page_guard(zone, &page[size], high, migratetype))
			continue;

		list_add(&page[size].lru, &area->free_list[migratetype]);
		area->nr_free++;
		set_page_order(&page[size], high);
	}
}

static void check_new_page_bad(struct page *page)
{
	const char *bad_reason = NULL;
	unsigned long bad_flags = 0;

	if (unlikely(atomic_read(&page->_mapcount) != -1))
		bad_reason = "nonzero mapcount";
	if (unlikely(page->mapping != NULL))
		bad_reason = "non-NULL mapping";
	if (unlikely(page_ref_count(page) != 0))
		bad_reason = "nonzero _count";
	if (unlikely(page->flags & __PG_HWPOISON)) {
		bad_reason = "HWPoisoned (hardware-corrupted)";
		bad_flags = __PG_HWPOISON;
		/* Don't complain about hwpoisoned pages */
		page_mapcount_reset(page); /* remove PageBuddy */
		return;
	}
	if (unlikely(page->flags & PAGE_FLAGS_CHECK_AT_PREP)) {
		bad_reason = "PAGE_FLAGS_CHECK_AT_PREP flag set";
		bad_flags = PAGE_FLAGS_CHECK_AT_PREP;
	}
#ifdef CONFIG_MEMCG
	if (unlikely(page->mem_cgroup))
		bad_reason = "page still charged to cgroup";
#endif
	bad_page(page, bad_reason, bad_flags);
}

/*
 * This page is about to be returned from the page allocator
 */
static inline int check_new_page(struct page *page)
{
	if (likely(page_expected_state(page,
				PAGE_FLAGS_CHECK_AT_PREP|__PG_HWPOISON)))
		return 0;

	check_new_page_bad(page);
	return 1;
}

static inline bool free_pages_prezeroed(void)
{
	return (IS_ENABLED(CONFIG_PAGE_POISONING_ZERO) &&
		page_poisoning_enabled()) || want_init_on_free();
}

static inline bool check_pcp_refill(struct page *page)
{
	return false;
}

static inline bool check_new_pcp(struct page *page)
{
	return IS_ENABLED(CONFIG_DEBUG_VM) ? check_new_page(page) : false;
}

static bool check_new_pages(struct page *page, unsigned int order)
{
	int i;
	for (i = 0; i < (1 << order); i++) {
		struct page *p = page + i;

		if (unlikely(check_new_page(p)))
			return true;
	}

	return false;
}

inline void post_alloc_hook(struct page *page, unsigned int order,
				gfp_t gfp_flags)
{
	set_page_private(page, 0);
	set_page_refcounted(page);

	arch_alloc_page(page, order);
	kasan_alloc_pages(page, order);
	kernel_map_pages(page, 1 << order, 1);
	kernel_poison_pages(page, 1 << order, 1);
	set_page_owner(page, order, gfp_flags);
}

#ifdef CONFIG_HUGEPAGE_POOL
void prep_new_page(struct page *page, unsigned int order, gfp_t gfp_flags,
							unsigned int alloc_flags)
#else
static void prep_new_page(struct page *page, unsigned int order, gfp_t gfp_flags,
							unsigned int alloc_flags)
#endif
{
	post_alloc_hook(page, order, gfp_flags);

	if (!free_pages_prezeroed() && want_init_on_alloc(gfp_flags))
		kernel_init_free_pages(page, 1 << order);

	if (order && (gfp_flags & __GFP_COMP))
		prep_compound_page(page, order);

	/*
	 * page is set pfmemalloc when ALLOC_NO_WATERMARKS was necessary to
	 * allocate the page. The expectation is that the caller is taking
	 * steps that will free more memory. The caller should avoid the page
	 * being used for !PFMEMALLOC purposes.
	 */
	if (alloc_flags & ALLOC_NO_WATERMARKS)
		set_page_pfmemalloc(page);
	else
		clear_page_pfmemalloc(page);
}

/*
 * Go through the free lists for the given migratetype and remove
 * the smallest available page from the freelists
 */
static __always_inline
struct page *__rmqueue_smallest(struct zone *zone, unsigned int order,
						int migratetype)
{
	unsigned int current_order;
	struct free_area *area;
	struct page *page;

	/* Find a page of the appropriate size in the preferred list */
	for (current_order = order; current_order < MAX_ORDER; ++current_order) {
		area = &(zone->free_area[current_order]);
		page = list_first_entry_or_null(&area->free_list[migratetype],
							struct page, lru);
		if (!page)
			continue;
		list_del(&page->lru);
		rmv_page_order(page);
		area->nr_free--;
		expand(zone, page, order, current_order, area, migratetype);
		set_pcppage_migratetype(page, migratetype);
		return page;
	}

	return NULL;
}


/*
 * This array describes the order lists are fallen back to when
 * the free lists for the desirable migrate type are depleted
 */
static int fallbacks[MIGRATE_TYPES][4] = {
	[MIGRATE_UNMOVABLE]   = { MIGRATE_RECLAIMABLE, MIGRATE_MOVABLE,   MIGRATE_TYPES },
	[MIGRATE_RECLAIMABLE] = { MIGRATE_UNMOVABLE,   MIGRATE_MOVABLE,   MIGRATE_TYPES },
	[MIGRATE_MOVABLE]     = { MIGRATE_RECLAIMABLE, MIGRATE_UNMOVABLE, MIGRATE_TYPES },
#ifdef CONFIG_CMA
	[MIGRATE_CMA]         = { MIGRATE_TYPES }, /* Never used */
#endif
#ifdef CONFIG_MEMORY_ISOLATION
	[MIGRATE_ISOLATE]     = { MIGRATE_TYPES }, /* Never used */
#endif
};

#ifdef CONFIG_CMA
static __always_inline struct page *__rmqueue_cma_fallback(struct zone *zone,
					unsigned int order)
{
	return __rmqueue_smallest(zone, order, MIGRATE_CMA);
}
#else
static inline struct page *__rmqueue_cma_fallback(struct zone *zone,
					unsigned int order) { return NULL; }
#endif

/*
 * Move the free pages in a range to the free lists of the requested type.
 * Note that start_page and end_pages are not aligned on a pageblock
 * boundary. If alignment is required, use move_freepages_block()
 */
static int move_freepages(struct zone *zone,
			  struct page *start_page, struct page *end_page,
			  int migratetype, int *num_movable)
{
	struct page *page;
	unsigned int order;
	int pages_moved = 0;

#ifndef CONFIG_HOLES_IN_ZONE
	/*
	 * page_zone is not safe to call in this context when
	 * CONFIG_HOLES_IN_ZONE is set. This bug check is probably redundant
	 * anyway as we check zone boundaries in move_freepages_block().
	 * Remove at a later date when no bug reports exist related to
	 * grouping pages by mobility
	 */
	VM_BUG_ON(pfn_valid(page_to_pfn(start_page)) &&
	          pfn_valid(page_to_pfn(end_page)) &&
	          page_zone(start_page) != page_zone(end_page));
#endif

	if (num_movable)
		*num_movable = 0;

	for (page = start_page; page <= end_page;) {
		if (!pfn_valid_within(page_to_pfn(page))) {
			page++;
			continue;
		}

		/* Make sure we are not inadvertently changing nodes */
		VM_BUG_ON_PAGE(page_to_nid(page) != zone_to_nid(zone), page);

		if (!PageBuddy(page)) {
			/*
			 * We assume that pages that could be isolated for
			 * migration are movable. But we don't actually try
			 * isolating, as that would be expensive.
			 */
			if (num_movable &&
					(PageLRU(page) || __PageMovable(page)))
				(*num_movable)++;

			page++;
			continue;
		}

		order = page_order(page);
		list_move(&page->lru,
			  &zone->free_area[order].free_list[migratetype]);
		page += 1 << order;
		pages_moved += 1 << order;
	}

	return pages_moved;
}

int move_freepages_block(struct zone *zone, struct page *page,
				int migratetype, int *num_movable)
{
	unsigned long start_pfn, end_pfn;
	struct page *start_page, *end_page;

	start_pfn = page_to_pfn(page);
	start_pfn = start_pfn & ~(pageblock_nr_pages-1);
	start_page = pfn_to_page(start_pfn);
	end_page = start_page + pageblock_nr_pages - 1;
	end_pfn = start_pfn + pageblock_nr_pages - 1;

	/* Do not cross zone boundaries */
	if (!zone_spans_pfn(zone, start_pfn))
		start_page = page;
	if (!zone_spans_pfn(zone, end_pfn))
		return 0;

	return move_freepages(zone, start_page, end_page, migratetype,
								num_movable);
}

static void change_pageblock_range(struct page *pageblock_page,
					int start_order, int migratetype)
{
	int nr_pageblocks = 1 << (start_order - pageblock_order);

	while (nr_pageblocks--) {
		set_pageblock_migratetype(pageblock_page, migratetype);
		pageblock_page += pageblock_nr_pages;
	}
}

/*
 * When we are falling back to another migratetype during allocation, try to
 * steal extra free pages from the same pageblocks to satisfy further
 * allocations, instead of polluting multiple pageblocks.
 *
 * If we are stealing a relatively large buddy page, it is likely there will
 * be more free pages in the pageblock, so try to steal them all. For
 * reclaimable and unmovable allocations, we steal regardless of page size,
 * as fragmentation caused by those allocations polluting movable pageblocks
 * is worse than movable allocations stealing from unmovable and reclaimable
 * pageblocks.
 */
static bool can_steal_fallback(unsigned int order, int start_mt, int fallback_type,
								unsigned int start_order)
{
	/*
	 * Leaving this order check is intended, although there is
	 * relaxed order check in next check. The reason is that
	 * we can actually steal whole pageblock if this condition met,
	 * but, below check doesn't guarantee it and that is just heuristic
	 * so could be changed anytime.
	 */
	if (order >= pageblock_order)
		return true;

	/* don't let unmovable allocations cause migrations simply because of free pages */
	if ((start_mt != MIGRATE_UNMOVABLE && order >= pageblock_order / 2) ||
	/* only steal reclaimable page blocks for unmovable allocations */
	(start_mt == MIGRATE_UNMOVABLE && fallback_type != MIGRATE_MOVABLE && order >= pageblock_order / 2) ||
	/* reclaimable can steal aggressively */
	start_mt == MIGRATE_RECLAIMABLE ||
	/* allow unmovable allocs up to 64K without migrating blocks */
	(start_mt == MIGRATE_UNMOVABLE && start_order >= 5) ||
	page_group_by_mobility_disabled)
		return true;

	return false;
}

static bool boost_eligible(struct zone *z)
{
	unsigned long high_wmark, threshold;
	unsigned long reclaim_eligible, free_pages;

	high_wmark = z->_watermark[WMARK_HIGH];
	reclaim_eligible = zone_page_state_snapshot(z, NR_ZONE_INACTIVE_FILE) +
			zone_page_state_snapshot(z, NR_ZONE_ACTIVE_FILE);
	free_pages = zone_page_state(z, NR_FREE_PAGES) -
			zone_page_state(z, NR_FREE_CMA_PAGES);
	threshold = high_wmark + (2 * mult_frac(high_wmark,
					watermark_boost_factor, 10000));

	/*
	 * Don't boost watermark If we are already low on memory where the
	 * boosting can simply put the watermarks at higher levels for a
	 * longer duration of time and thus the other users relied on the
	 * watermarks are forced to choose unintended decissions. If memory
	 * is so low, kswapd in normal mode should help.
	 */

	if (reclaim_eligible < threshold && free_pages < threshold)
		return false;

	return true;
}

static inline void boost_watermark(struct zone *zone)
{
	unsigned long max_boost;

	if (!watermark_boost_factor || !boost_eligible(zone))
		return;

	max_boost = mult_frac(zone->_watermark[WMARK_HIGH],
			watermark_boost_factor, 10000);

	/*
	 * high watermark may be uninitialised if fragmentation occurs
	 * very early in boot so do not boost. We do not fall
	 * through and boost by pageblock_nr_pages as failing
	 * allocations that early means that reclaim is not going
	 * to help and it may even be impossible to reclaim the
	 * boosted watermark resulting in a hang.
	 */
	if (!max_boost)
		return;

	max_boost = max(pageblock_nr_pages, max_boost);

	zone->watermark_boost = min(zone->watermark_boost + pageblock_nr_pages,
		max_boost);
}

/*
 * This function implements actual steal behaviour. If order is large enough,
 * we can steal whole pageblock. If not, we first move freepages in this
 * pageblock to our migratetype and determine how many already-allocated pages
 * are there in the pageblock with a compatible migratetype. If at least half
 * of pages are free or compatible, we can change migratetype of the pageblock
 * itself, so pages freed in the future will be put on the correct free list.
 */
static void steal_suitable_fallback(struct zone *zone, struct page *page,
		unsigned int alloc_flags, int start_type, bool whole_block)
{
	unsigned int current_order = page_order(page);
	struct free_area *area;
	int free_pages, movable_pages, alike_pages;
	int old_block_type;

	old_block_type = get_pageblock_migratetype(page);

	/*
	 * This can happen due to races and we want to prevent broken
	 * highatomic accounting.
	 */
	if (is_migrate_highatomic(old_block_type))
		goto single_page;

	/* Take ownership for orders >= pageblock_order */
	if (current_order >= pageblock_order) {
		change_pageblock_range(page, current_order, start_type);
		goto single_page;
	}

	/*
	 * Boost watermarks to increase reclaim pressure to reduce the
	 * likelihood of future fallbacks. Wake kswapd now as the node
	 * may be balanced overall and kswapd will not wake naturally.
	 */
	boost_watermark(zone);
	if (alloc_flags & ALLOC_KSWAPD)
		set_bit(ZONE_BOOSTED_WATERMARK, &zone->flags);

	/* We are not allowed to try stealing from the whole block */
	if (!whole_block)
		goto single_page;

	free_pages = move_freepages_block(zone, page, start_type,
						&movable_pages);
	/*
	 * Determine how many pages are compatible with our allocation.
	 * For movable allocation, it's the number of movable pages which
	 * we just obtained. For other types it's a bit more tricky.
	 */
	if (start_type == MIGRATE_MOVABLE) {
		alike_pages = movable_pages;
	} else {
		/*
		 * If we are falling back a RECLAIMABLE or UNMOVABLE allocation
		 * to MOVABLE pageblock, consider all non-movable pages as
		 * compatible. If it's UNMOVABLE falling back to RECLAIMABLE or
		 * vice versa, be conservative since we can't distinguish the
		 * exact migratetype of non-movable pages.
		 */
		if (old_block_type == MIGRATE_MOVABLE)
			alike_pages = pageblock_nr_pages
						- (free_pages + movable_pages);
		else
			alike_pages = 0;
	}

	/* moving whole block can fail due to zone boundary conditions */
	if (!free_pages)
		goto single_page;

	/*
	 * If a sufficient number of pages in the block are either free or of
	 * comparable migratability as our allocation, claim the whole block.
	 */
	if (free_pages + alike_pages >= (1 << (pageblock_order-1)) ||
			page_group_by_mobility_disabled)
		set_pageblock_migratetype(page, start_type);

	return;

single_page:
	area = &zone->free_area[current_order];
	list_move(&page->lru, &area->free_list[start_type]);
}

/*
 * Check whether there is a suitable fallback freepage with requested order.
 * If only_stealable is true, this function returns fallback_mt only if
 * we can steal other freepages all together. This would help to reduce
 * fragmentation due to mixed migratetype pages in one pageblock.
 */
int find_suitable_fallback(struct free_area *area, unsigned int order,
			int migratetype, bool only_stealable, bool *can_steal, unsigned int start_order)
{
	int i;
	int fallback_mt;

	if (area->nr_free == 0)
		return -1;

	*can_steal = false;
	for (i = 0;; i++) {
		fallback_mt = fallbacks[migratetype][i];
		if (fallback_mt == MIGRATE_TYPES)
			break;

		if (list_empty(&area->free_list[fallback_mt]))
			continue;

		if (can_steal_fallback(order, migratetype, fallback_mt, start_order))
			*can_steal = true;

		if (!only_stealable)
			return fallback_mt;

		if (*can_steal)
			return fallback_mt;
	}

	return -1;
}

/*
 * Reserve a pageblock for exclusive use of high-order atomic allocations if
 * there are no empty page blocks that contain a page with a suitable order
 */
static void reserve_highatomic_pageblock(struct page *page, struct zone *zone,
				unsigned int alloc_order)
{
	int mt;
	unsigned long max_managed, flags;

	/*
	 * Limit the number reserved to 1 pageblock or roughly 1% of a zone.
	 * Check is race-prone but harmless.
	 */
	max_managed = (zone->managed_pages / 100) + pageblock_nr_pages;
#if CONFIG_HIGHATOMIC_PAGEBLOCKS > 0
	max_managed = min_t(unsigned long, max_managed,
			pageblock_nr_pages * CONFIG_HIGHATOMIC_PAGEBLOCKS);
#endif
	if (zone->nr_reserved_highatomic >= max_managed)
		return;

	spin_lock_irqsave(&zone->lock, flags);

	/* Recheck the nr_reserved_highatomic limit under the lock */
	if (zone->nr_reserved_highatomic >= max_managed)
		goto out_unlock;

	/* Yoink! */
	mt = get_pageblock_migratetype(page);
	if (!is_migrate_highatomic(mt) && !is_migrate_isolate(mt)
	    && !is_migrate_cma(mt)) {
		zone->nr_reserved_highatomic += pageblock_nr_pages;
		set_pageblock_migratetype(page, MIGRATE_HIGHATOMIC);
		move_freepages_block(zone, page, MIGRATE_HIGHATOMIC, NULL);
	}

out_unlock:
	spin_unlock_irqrestore(&zone->lock, flags);
}

/*
 * Used when an allocation is about to fail under memory pressure. This
 * potentially hurts the reliability of high-order allocations when under
 * intense memory pressure but failed atomic allocations should be easier
 * to recover from than an OOM.
 *
 * If @force is true, try to unreserve a pageblock even though highatomic
 * pageblock is exhausted.
 */
static bool unreserve_highatomic_pageblock(const struct alloc_context *ac,
						bool force)
{
	struct zonelist *zonelist = ac->zonelist;
	unsigned long flags;
	struct zoneref *z;
	struct zone *zone;
	struct page *page;
	int order;
	bool ret;

	for_each_zone_zonelist_nodemask(zone, z, zonelist, ac->high_zoneidx,
								ac->nodemask) {
		/*
		 * Preserve at least one pageblock unless memory pressure
		 * is really high.
		 */
		if (!force && zone->nr_reserved_highatomic <=
					pageblock_nr_pages)
			continue;

		spin_lock_irqsave(&zone->lock, flags);
		for (order = 0; order < MAX_ORDER; order++) {
			struct free_area *area = &(zone->free_area[order]);

			page = list_first_entry_or_null(
					&area->free_list[MIGRATE_HIGHATOMIC],
					struct page, lru);
			if (!page)
				continue;

			/*
			 * In page freeing path, migratetype change is racy so
			 * we can counter several free pages in a pageblock
			 * in this loop althoug we changed the pageblock type
			 * from highatomic to ac->migratetype. So we should
			 * adjust the count once.
			 */
			if (is_migrate_highatomic_page(page)) {
				/*
				 * It should never happen but changes to
				 * locking could inadvertently allow a per-cpu
				 * drain to add pages to MIGRATE_HIGHATOMIC
				 * while unreserving so be safe and watch for
				 * underflows.
				 */
				zone->nr_reserved_highatomic -= min(
						pageblock_nr_pages,
						zone->nr_reserved_highatomic);
			}

			/*
			 * Convert to ac->migratetype and avoid the normal
			 * pageblock stealing heuristics. Minimally, the caller
			 * is doing the work and needs the pages. More
			 * importantly, if the block was always converted to
			 * MIGRATE_UNMOVABLE or another type then the number
			 * of pageblocks that cannot be completely freed
			 * may increase.
			 */
			set_pageblock_migratetype(page, ac->migratetype);
			ret = move_freepages_block(zone, page, ac->migratetype,
									NULL);
			if (ret) {
				spin_unlock_irqrestore(&zone->lock, flags);
				return ret;
			}
		}
		spin_unlock_irqrestore(&zone->lock, flags);
	}

	return false;
}

/*
 * Try finding a free buddy page on the fallback list and put it on the free
 * list of requested migratetype, possibly along with other pages from the same
 * block, depending on fragmentation avoidance heuristics. Returns true if
 * fallback was found so that __rmqueue_smallest() can grab it.
 *
 * The use of signed ints for order and current_order is a deliberate
 * deviation from the rest of this file, to make the for loop
 * condition simpler.
 */
static __always_inline bool
__rmqueue_fallback(struct zone *zone, int order, int start_migratetype,
						unsigned int alloc_flags)
{
	struct free_area *area;
	int current_order;
	int min_order = order;
	struct page *page;
	int fallback_mt;
	bool can_steal;

	/*
	 * Do not steal pages from freelists belonging to other pageblocks
	 * i.e. orders < pageblock_order. If there are no local zones free,
	 * the zonelists will be reiterated without ALLOC_NOFRAGMENT.
	 */
	if (alloc_flags & ALLOC_NOFRAGMENT)
		min_order = pageblock_order;

	/*
	 * Find the largest available free page in the other list. This roughly
	 * approximates finding the pageblock with the most free pages, which
	 * would be too costly to do exactly.
	 */
	for (current_order = MAX_ORDER - 1; current_order >= min_order;
				--current_order) {
		area = &(zone->free_area[current_order]);
		fallback_mt = find_suitable_fallback(area, current_order,
				start_migratetype, false, &can_steal, order);
		if (fallback_mt == -1)
			continue;

		/*
		 * We cannot steal all free pages from the pageblock and the
		 * requested migratetype is movable. In that case it's better to
		 * steal and split the smallest available page instead of the
		 * largest available page, because even if the next movable
		 * allocation falls back into a different pageblock than this
		 * one, it won't cause permanent fragmentation.
		 */
		if (!can_steal && start_migratetype == MIGRATE_MOVABLE
					&& current_order > order)
			goto find_smallest;

		goto do_steal;
	}

	return false;

find_smallest:
	for (current_order = order; current_order < MAX_ORDER;
							current_order++) {
		area = &(zone->free_area[current_order]);
		fallback_mt = find_suitable_fallback(area, current_order,
				start_migratetype, false, &can_steal, order);
		if (fallback_mt != -1)
			break;
	}

	/*
	 * This should not happen - we already found a suitable fallback
	 * when looking for the largest page.
	 */
	VM_BUG_ON(current_order == MAX_ORDER);

do_steal:
	page = list_first_entry(&area->free_list[fallback_mt],
							struct page, lru);

	steal_suitable_fallback(zone, page, alloc_flags, start_migratetype,
								can_steal);

	trace_mm_page_alloc_extfrag(page, order, current_order,
		start_migratetype, fallback_mt);

	return true;

}

/*
 * Do the hard work of removing an element from the buddy allocator.
 * Call me with the zone->lock already held.
 */
static __always_inline struct page *
__rmqueue(struct zone *zone, unsigned int order, int migratetype,
						unsigned int alloc_flags)
{
	struct page *page;

retry:
	page = __rmqueue_smallest(zone, order, migratetype);

	if (unlikely(!page) && __rmqueue_fallback(zone, order, migratetype,
						  alloc_flags))
		goto retry;

	trace_mm_page_alloc_zone_locked(page, order, migratetype);
	return page;
}

#ifdef CONFIG_CMA
static struct page *__rmqueue_cma(struct zone *zone, unsigned int order)
{
	struct page *page = 0;

	if (IS_ENABLED(CONFIG_CMA))
		if (!zone->cma_alloc)
			page = __rmqueue_cma_fallback(zone, order);
	trace_mm_page_alloc_zone_locked(page, order, MIGRATE_CMA);
	return page;
}
#else
static inline struct page *__rmqueue_cma(struct zone *zone, unsigned int order)
{
	return NULL;
}
#endif

/*
 * Obtain a specified number of elements from the buddy allocator, and relax the
 * zone lock when needed. Add them to the supplied list. Returns the number of
 * new pages which were placed at *list.
 */
static int rmqueue_bulk(struct zone *zone, unsigned int order,
			unsigned long count, struct list_head *list,
			int migratetype, unsigned int alloc_flags)
{
	const bool can_resched = !preempt_count() && !irqs_disabled();
	int i, alloced = 0, last_mod = 0;

	spin_lock(&zone->lock);
	for (i = 0; i < count; ++i) {
		struct page *page;

		/*
		 * If migrate type CMA is being requested only try to
		 * satisfy the request with CMA pages to try and increase
		 * CMA utlization.
		 */
		if (is_migrate_cma(migratetype))
			page = __rmqueue_cma(zone, order);
		else
			page = __rmqueue(zone, order, migratetype, alloc_flags);

		if (unlikely(page == NULL))
			break;

		/* Reschedule and ease the contention on the lock if needed */
		if (i + 1 < count && ((can_resched && need_resched()) ||
				      spin_needbreak(&zone->lock))) {
			__mod_zone_page_state(zone, NR_FREE_PAGES,
					      -((i + 1 - last_mod) << order));
			last_mod = i + 1;
			spin_unlock(&zone->lock);
			if (can_resched)
				cond_resched();
			spin_lock(&zone->lock);
		}

		if (unlikely(check_pcp_refill(page)))
			continue;

		/*
		 * Split buddy pages returned by expand() are received here in
		 * physical page order. The page is added to the tail of
		 * caller's list. From the callers perspective, the linked list
		 * is ordered by page number under some conditions. This is
		 * useful for IO devices that can forward direction from the
		 * head, thus also in the physical page order. This is useful
		 * for IO devices that can merge IO requests if the physical
		 * pages are ordered properly.
		 */
		list_add_tail(&page->lru, list);
		alloced++;
		if (is_migrate_cma(get_pcppage_migratetype(page)))
			__mod_zone_page_state(zone, NR_FREE_CMA_PAGES,
					      -(1 << order));
	}

	/*
	 * i pages were removed from the buddy list even if some leak due
	 * to check_pcp_refill failing so adjust NR_FREE_PAGES based
	 * on i. Do not confuse with 'alloced' which is the number of
	 * pages added to the pcp list.
	 */
	__mod_zone_page_state(zone, NR_FREE_PAGES, -((i - last_mod) << order));
	spin_unlock(&zone->lock);
	return alloced;
}

/*
 * Return the pcp list that corresponds to the migrate type if that list isn't
 * empty.
 * If the list is empty return NULL.
 */
static struct list_head *get_populated_pcp_list(struct zone *zone,
			unsigned int order, struct per_cpu_pages *pcp,
			int migratetype, unsigned int alloc_flags)
{
	struct list_head *list = &pcp->lists[migratetype];

	if (list_empty(list)) {
		pcp->count += rmqueue_bulk(zone, order,
				pcp->batch, list,
				migratetype, alloc_flags);

		if (list_empty(list))
			list = NULL;
	}
	return list;
}

#ifdef CONFIG_NUMA
/*
 * Called from the vmstat counter updater to drain pagesets of this
 * currently executing processor on remote nodes after they have
 * expired.
 *
 * Note that this function must be called with the thread pinned to
 * a single processor.
 */
void drain_zone_pages(struct zone *zone, struct per_cpu_pages *pcp)
{
	unsigned long flags;
	int to_drain, batch;
	LIST_HEAD(dst);

	local_irq_save(flags);
	batch = READ_ONCE(pcp->batch);
	to_drain = min(pcp->count, batch);
	if (to_drain > 0)
		isolate_pcp_pages(to_drain, pcp, &dst);

	local_irq_restore(flags);

	if (to_drain > 0)
		free_pcppages_bulk(zone, &dst, false);
}
#endif

/*
 * Drain pcplists of the indicated processor and zone.
 *
 * The processor must either be the current processor and the
 * thread pinned to the current processor or a processor that
 * is not online.
 */
static void drain_pages_zone(unsigned int cpu, struct zone *zone)
{
	unsigned long flags;
	struct per_cpu_pageset *pset;
	struct per_cpu_pages *pcp;
	LIST_HEAD(dst);
	int count;

	local_irq_save(flags);
	pset = per_cpu_ptr(zone->pageset, cpu);

	pcp = &pset->pcp;
	count = pcp->count;
	if (count)
		isolate_pcp_pages(count, pcp, &dst);

	local_irq_restore(flags);

	if (count)
		free_pcppages_bulk(zone, &dst, false);
}

/*
 * Drain pcplists of all zones on the indicated processor.
 *
 * The processor must either be the current processor and the
 * thread pinned to the current processor or a processor that
 * is not online.
 */
static void drain_pages(unsigned int cpu)
{
	struct zone *zone;

	for_each_populated_zone(zone) {
		drain_pages_zone(cpu, zone);
	}
}

/*
 * Spill all of this CPU's per-cpu pages back into the buddy allocator.
 *
 * The CPU has to be pinned. When zone parameter is non-NULL, spill just
 * the single zone's pages.
 */
void drain_local_pages(struct zone *zone)
{
	int cpu = smp_processor_id();

	if (zone)
		drain_pages_zone(cpu, zone);
	else
		drain_pages(cpu);
}

static void drain_local_pages_wq(struct work_struct *work)
{
	/*
	 * drain_all_pages doesn't use proper cpu hotplug protection so
	 * we can race with cpu offline when the WQ can move this from
	 * a cpu pinned worker to an unbound one. We can operate on a different
	 * cpu which is allright but we also have to make sure to not move to
	 * a different one.
	 */
	preempt_disable();
	drain_local_pages(NULL);
	preempt_enable();
}

/*
 * Spill all the per-cpu pages from all CPUs back into the buddy allocator.
 *
 * When zone parameter is non-NULL, spill just the single zone's pages.
 *
 * Note that this can be extremely slow as the draining happens in a workqueue.
 */
void drain_all_pages(struct zone *zone)
{
	int cpu;

	/*
	 * Allocate in the BSS so we wont require allocation in
	 * direct reclaim path for CONFIG_CPUMASK_OFFSTACK=y
	 */
	static cpumask_t cpus_with_pcps;

	/*
	 * Make sure nobody triggers this path before mm_percpu_wq is fully
	 * initialized.
	 */
	if (WARN_ON_ONCE(!mm_percpu_wq))
		return;

	/*
	 * Do not drain if one is already in progress unless it's specific to
	 * a zone. Such callers are primarily CMA and memory hotplug and need
	 * the drain to be complete when the call returns.
	 */
	if (unlikely(!mutex_trylock(&pcpu_drain_mutex))) {
		if (!zone)
			return;
		mutex_lock(&pcpu_drain_mutex);
	}

	/*
	 * We don't care about racing with CPU hotplug event
	 * as offline notification will cause the notified
	 * cpu to drain that CPU pcps and on_each_cpu_mask
	 * disables preemption as part of its processing
	 */
	for_each_online_cpu(cpu) {
		struct per_cpu_pageset *pcp;
		struct zone *z;
		bool has_pcps = false;

		if (zone) {
			pcp = per_cpu_ptr(zone->pageset, cpu);
			if (pcp->pcp.count)
				has_pcps = true;
		} else {
			for_each_populated_zone(z) {
				pcp = per_cpu_ptr(z->pageset, cpu);
				if (pcp->pcp.count) {
					has_pcps = true;
					break;
				}
			}
		}

		if (has_pcps)
			cpumask_set_cpu(cpu, &cpus_with_pcps);
		else
			cpumask_clear_cpu(cpu, &cpus_with_pcps);
	}

	for_each_cpu(cpu, &cpus_with_pcps) {
		struct work_struct *work = per_cpu_ptr(&pcpu_drain, cpu);
		INIT_WORK(work, drain_local_pages_wq);
		queue_work_on(cpu, mm_percpu_wq, work);
	}
	for_each_cpu(cpu, &cpus_with_pcps)
		flush_work(per_cpu_ptr(&pcpu_drain, cpu));

	mutex_unlock(&pcpu_drain_mutex);
}

#ifdef CONFIG_HIBERNATION

/*
 * Touch the watchdog for every WD_PAGE_COUNT pages.
 */
#define WD_PAGE_COUNT	(128*1024)

void mark_free_pages(struct zone *zone)
{
	unsigned long pfn, max_zone_pfn, page_count = WD_PAGE_COUNT;
	unsigned long flags;
	unsigned int order, t;
	struct page *page;

	if (zone_is_empty(zone))
		return;

	spin_lock_irqsave(&zone->lock, flags);

	max_zone_pfn = zone_end_pfn(zone);
	for (pfn = zone->zone_start_pfn; pfn < max_zone_pfn; pfn++)
		if (pfn_valid(pfn)) {
			page = pfn_to_page(pfn);

			if (!--page_count) {
				touch_nmi_watchdog();
				page_count = WD_PAGE_COUNT;
			}

			if (page_zone(page) != zone)
				continue;

			if (!swsusp_page_is_forbidden(page))
				swsusp_unset_page_free(page);
		}

	for_each_migratetype_order(order, t) {
		list_for_each_entry(page,
				&zone->free_area[order].free_list[t], lru) {
			unsigned long i;

			pfn = page_to_pfn(page);
			for (i = 0; i < (1UL << order); i++) {
				if (!--page_count) {
					touch_nmi_watchdog();
					page_count = WD_PAGE_COUNT;
				}
				swsusp_set_page_free(pfn_to_page(pfn + i));
			}
		}
	}
	spin_unlock_irqrestore(&zone->lock, flags);
}
#endif /* CONFIG_PM */

static bool free_unref_page_prepare(struct page *page, unsigned long pfn)
{
	int migratetype;

	if (!free_pcp_prepare(page))
		return false;

	migratetype = get_pfnblock_migratetype(page, pfn);
	set_pcppage_migratetype(page, migratetype);
	return true;
}

static void free_unref_page_commit(struct page *page, unsigned long pfn,
				   struct list_head *dst)
{
	struct zone *zone = page_zone(page);
	struct per_cpu_pages *pcp;
	int migratetype;

	migratetype = get_pcppage_migratetype(page);
	__count_vm_event(PGFREE);

	/*
	 * We only track unmovable, reclaimable and movable on pcp lists.
	 * Free ISOLATE pages back to the allocator because they are being
	 * offlined but treat HIGHATOMIC as movable pages so we can get those
	 * areas back if necessary. Otherwise, we may have to free
	 * excessively into the page allocator
	 */
	if (migratetype >= MIGRATE_PCPTYPES) {
		if (unlikely(is_migrate_isolate(migratetype))) {
			free_one_page(zone, page, pfn, 0, migratetype);
			return;
		}
		migratetype = MIGRATE_MOVABLE;
	}

	pcp = &this_cpu_ptr(zone->pageset)->pcp;
	list_add(&page->lru, &pcp->lists[migratetype]);
	pcp->count++;
	if (pcp->count >= pcp->high) {
		unsigned long batch = READ_ONCE(pcp->batch);

		isolate_pcp_pages(batch, pcp, dst);
	}
}

/*
 * Free a 0-order page
 */
void free_unref_page(struct page *page)
{
	unsigned long flags;
	unsigned long pfn = page_to_pfn(page);
	struct zone *zone = page_zone(page);
	LIST_HEAD(dst);

	if (!free_unref_page_prepare(page, pfn))
		return;

	local_irq_save(flags);
	free_unref_page_commit(page, pfn, &dst);
	local_irq_restore(flags);
	if (!list_empty(&dst))
		free_pcppages_bulk(zone, &dst, false);
}

/*
 * Free a list of 0-order pages
 */
void free_unref_page_list(struct list_head *list)
{
	struct page *page, *next;
	unsigned long flags, pfn;
	int batch_count = 0;
	struct list_head dsts[__MAX_NR_ZONES];
	int i;

	for (i = 0; i < __MAX_NR_ZONES; i++)
		INIT_LIST_HEAD(&dsts[i]);

	/* Prepare pages for freeing */
	list_for_each_entry_safe(page, next, list, lru) {
		pfn = page_to_pfn(page);
		if (!free_unref_page_prepare(page, pfn))
			list_del(&page->lru);
		set_page_private(page, pfn);
	}

	local_irq_save(flags);
	list_for_each_entry_safe(page, next, list, lru) {
		unsigned long pfn = page_private(page);
		enum zone_type type;

		set_page_private(page, 0);
		trace_mm_page_free_batched(page);
		type = page_zonenum(page);
		free_unref_page_commit(page, pfn, &dsts[type]);

		/*
		 * Guard against excessive IRQ disabled times when we get
		 * a large list of pages to free.
		 */
		if (++batch_count == SWAP_CLUSTER_MAX) {
			local_irq_restore(flags);
			batch_count = 0;
			local_irq_save(flags);
		}
	}
	local_irq_restore(flags);

	for (i = 0; i < __MAX_NR_ZONES; ) {
		struct page *page;
		struct zone *zone;

		if (list_empty(&dsts[i])) {
			i++;
			continue;
		}

		page = list_first_entry(&dsts[i], struct page, lru);
		zone = page_zone(page);

		free_pcppages_bulk(zone, &dsts[i], true);
	}
}

/*
 * split_page takes a non-compound higher-order page, and splits it into
 * n (1<<order) sub-pages: page[0..n]
 * Each sub-page must be freed individually.
 *
 * Note: this is probably too low level an operation for use in drivers.
 * Please consult with lkml before using this in your driver.
 */
void split_page(struct page *page, unsigned int order)
{
	int i;

	VM_BUG_ON_PAGE(PageCompound(page), page);
	VM_BUG_ON_PAGE(!page_count(page), page);

	for (i = 1; i < (1 << order); i++)
		set_page_refcounted(page + i);
	split_page_owner(page, order);
}
EXPORT_SYMBOL_GPL(split_page);

int __isolate_free_page(struct page *page, unsigned int order)
{
	unsigned long watermark;
	struct zone *zone;
	int mt;

	BUG_ON(!PageBuddy(page));

	zone = page_zone(page);
	mt = get_pageblock_migratetype(page);

	if (!is_migrate_isolate(mt)) {
		/*
		 * Obey watermarks as if the page was being allocated. We can
		 * emulate a high-order watermark check with a raised order-0
		 * watermark, because we already know our high-order page
		 * exists.
		 */
		watermark = zone->_watermark[WMARK_MIN] + (1UL << order);
		if (!zone_watermark_ok(zone, 0, watermark, 0, ALLOC_CMA))
			return 0;

		__mod_zone_freepage_state(zone, -(1UL << order), mt);
	}

	/* Remove page from free list */
	list_del(&page->lru);
	zone->free_area[order].nr_free--;
	rmv_page_order(page);

	/*
	 * Set the pageblock if the isolated page is at least half of a
	 * pageblock
	 */
	if (order >= pageblock_order - 1) {
		struct page *endpage = page + (1 << order) - 1;
		for (; page < endpage; page += pageblock_nr_pages) {
			int mt = get_pageblock_migratetype(page);
			if (!is_migrate_isolate(mt) && !is_migrate_cma(mt)
			    && !is_migrate_highatomic(mt))
				set_pageblock_migratetype(page,
							  MIGRATE_MOVABLE);
		}
	}


	return 1UL << order;
}

/*
 * Update NUMA hit/miss statistics
 *
 * Must be called with interrupts disabled.
 */
static inline void zone_statistics(struct zone *preferred_zone, struct zone *z)
{
#ifdef CONFIG_NUMA
	enum numa_stat_item local_stat = NUMA_LOCAL;

	/* skip numa counters update if numa stats is disabled */
	if (!static_branch_likely(&vm_numa_stat_key))
		return;

	if (zone_to_nid(z) != numa_node_id())
		local_stat = NUMA_OTHER;

	if (zone_to_nid(z) == zone_to_nid(preferred_zone))
		__inc_numa_state(z, NUMA_HIT);
	else {
		__inc_numa_state(z, NUMA_MISS);
		__inc_numa_state(preferred_zone, NUMA_FOREIGN);
	}
	__inc_numa_state(z, local_stat);
#endif
}

/* Remove page from the per-cpu list, caller must protect the list */
static struct page *__rmqueue_pcplist(struct zone *zone, int migratetype,
			unsigned int alloc_flags,
			struct per_cpu_pages *pcp,
			gfp_t gfp_flags)
{
	struct page *page = NULL;
	struct list_head *list = NULL;

	do {
		/* First try to get CMA pages */
		if (migratetype == MIGRATE_MOVABLE &&
				gfp_flags & __GFP_CMA) {
			list = get_populated_pcp_list(zone, 0, pcp,
					get_cma_migrate_type(), alloc_flags);
		}

		if (list == NULL) {
			/*
			 * Either CMA is not suitable or there are no
			 * free CMA pages.
			 */
			list = get_populated_pcp_list(zone, 0, pcp,
					migratetype, alloc_flags);
			if (unlikely(list == NULL) ||
					unlikely(list_empty(list)))
				return NULL;

		}

		page = list_first_entry(list, struct page, lru);
		list_del(&page->lru);
		pcp->count--;
	} while (check_new_pcp(page));

	return page;
}

/* Lock and remove page from the per-cpu list */
static struct page *rmqueue_pcplist(struct zone *preferred_zone,
			struct zone *zone, unsigned int order,
			gfp_t gfp_flags, int migratetype,
			unsigned int alloc_flags)
{
	struct per_cpu_pages *pcp;
	struct page *page;
	unsigned long flags;

	local_irq_save(flags);
	pcp = &this_cpu_ptr(zone->pageset)->pcp;
	page = __rmqueue_pcplist(zone,  migratetype, alloc_flags, pcp,
				 gfp_flags);
	if (page) {
		__count_zid_vm_events(PGALLOC, page_zonenum(page), 1 << order);
		zone_statistics(preferred_zone, zone);
	}
	local_irq_restore(flags);
	return page;
}

/*
 * Allocate a page from the given zone. Use pcplists for order-0 allocations.
 */
static inline
struct page *rmqueue(struct zone *preferred_zone,
			struct zone *zone, unsigned int order,
			gfp_t gfp_flags, unsigned int alloc_flags,
			int migratetype)
{
	unsigned long flags;
	struct page *page;

	if (likely(order == 0)) {
		page = rmqueue_pcplist(preferred_zone, zone, order,
				gfp_flags, migratetype, alloc_flags);
		goto out;
	}

	/*
	 * We most definitely don't want callers attempting to
	 * allocate greater than order-1 page units with __GFP_NOFAIL.
	 */
	WARN_ON_ONCE((gfp_flags & __GFP_NOFAIL) && (order > 1));
	spin_lock_irqsave(&zone->lock, flags);

	do {
		page = NULL;

		if (alloc_flags & ALLOC_HARDER) {
			page = __rmqueue_smallest(zone, order, MIGRATE_HIGHATOMIC);
			if (page)
				trace_mm_page_alloc_zone_locked(page, order, migratetype);
		}

		if (!page && migratetype == MIGRATE_MOVABLE &&
				gfp_flags & __GFP_CMA)
			page = __rmqueue_cma(zone, order);

		if (!page)
			page = __rmqueue(zone, order, migratetype, alloc_flags);
	} while (page && check_new_pages(page, order));

	spin_unlock(&zone->lock);
	if (!page)
		goto failed;
	__mod_zone_freepage_state(zone, -(1 << order),
				  get_pcppage_migratetype(page));

	__count_zid_vm_events(PGALLOC, page_zonenum(page), 1 << order);
	zone_statistics(preferred_zone, zone);
	local_irq_restore(flags);

out:
	/* Separate test+clear to avoid unnecessary atomics */
	if (test_bit(ZONE_BOOSTED_WATERMARK, &zone->flags)) {
		clear_bit(ZONE_BOOSTED_WATERMARK, &zone->flags);
		wakeup_kswapd(zone, 0, 0, zone_idx(zone));
	}

	VM_BUG_ON_PAGE(page && bad_range(zone, page), page);
	return page;

failed:
	local_irq_restore(flags);
	return NULL;
}

#ifdef CONFIG_FAIL_PAGE_ALLOC

static struct {
	struct fault_attr attr;

	bool ignore_gfp_highmem;
	bool ignore_gfp_reclaim;
	u32 min_order;
} fail_page_alloc = {
	.attr = FAULT_ATTR_INITIALIZER,
	.ignore_gfp_reclaim = true,
	.ignore_gfp_highmem = true,
	.min_order = 1,
};

static int __init setup_fail_page_alloc(char *str)
{
	return setup_fault_attr(&fail_page_alloc.attr, str);
}
__setup("fail_page_alloc=", setup_fail_page_alloc);

static bool should_fail_alloc_page(gfp_t gfp_mask, unsigned int order)
{
	if (order < fail_page_alloc.min_order)
		return false;
	if (gfp_mask & __GFP_NOFAIL)
		return false;
	if (fail_page_alloc.ignore_gfp_highmem && (gfp_mask & __GFP_HIGHMEM))
		return false;
	if (fail_page_alloc.ignore_gfp_reclaim &&
			(gfp_mask & __GFP_DIRECT_RECLAIM))
		return false;

	return should_fail(&fail_page_alloc.attr, 1 << order);
}

#ifdef CONFIG_FAULT_INJECTION_DEBUG_FS

static int __init fail_page_alloc_debugfs(void)
{
	umode_t mode = S_IFREG | 0600;
	struct dentry *dir;

	dir = fault_create_debugfs_attr("fail_page_alloc", NULL,
					&fail_page_alloc.attr);
	if (IS_ERR(dir))
		return PTR_ERR(dir);

	if (!debugfs_create_bool("ignore-gfp-wait", mode, dir,
				&fail_page_alloc.ignore_gfp_reclaim))
		goto fail;
	if (!debugfs_create_bool("ignore-gfp-highmem", mode, dir,
				&fail_page_alloc.ignore_gfp_highmem))
		goto fail;
	if (!debugfs_create_u32("min-order", mode, dir,
				&fail_page_alloc.min_order))
		goto fail;

	return 0;
fail:
	debugfs_remove_recursive(dir);

	return -ENOMEM;
}

late_initcall(fail_page_alloc_debugfs);

#endif /* CONFIG_FAULT_INJECTION_DEBUG_FS */

#else /* CONFIG_FAIL_PAGE_ALLOC */

static inline bool should_fail_alloc_page(gfp_t gfp_mask, unsigned int order)
{
	return false;
}

#endif /* CONFIG_FAIL_PAGE_ALLOC */

static inline long __zone_watermark_unusable_free(struct zone *z,
				unsigned int order, unsigned int alloc_flags)
{
	const bool alloc_harder = (alloc_flags & (ALLOC_HARDER|ALLOC_OOM));
	long unusable_free = (1 << order) - 1;

	/*
	 * If the caller does not have rights to ALLOC_HARDER then subtract
	 * the high-atomic reserves. This will over-estimate the size of the
	 * atomic reserve but it avoids a search.
	 */
	if (likely(!alloc_harder))
		unusable_free += z->nr_reserved_highatomic;

#ifdef CONFIG_CMA
	/* If allocation can't use CMA areas don't use free CMA pages */
	if (!(alloc_flags & ALLOC_CMA))
		unusable_free += zone_page_state(z, NR_FREE_CMA_PAGES);
#endif

	return unusable_free;
}

/*
 * Return true if free base pages are above 'mark'. For high-order checks it
 * will return true of the order-0 watermark is reached and there is at least
 * one free page of a suitable size. Checking now avoids taking the zone lock
 * to check in the allocation paths if no pages are free.
 */
bool __zone_watermark_ok(struct zone *z, unsigned int order, unsigned long mark,
			 int classzone_idx, unsigned int alloc_flags,
			 long free_pages)
{
	long min = mark;
	int o;
	const bool alloc_harder = (alloc_flags & (ALLOC_HARDER|ALLOC_OOM));

	/* free_pages may go negative - that's OK */
	free_pages -= __zone_watermark_unusable_free(z, order, alloc_flags);

	if (alloc_flags & ALLOC_HIGH)
		min -= min / 2;

	if (unlikely(alloc_harder)) {
		/*
		 * OOM victims can try even harder than normal ALLOC_HARDER
		 * users on the grounds that it's definitely going to be in
		 * the exit path shortly and free memory. Any allocation it
		 * makes during the free path will be small and short-lived.
		 */
		if (alloc_flags & ALLOC_OOM)
			min -= min / 2;
		else
			min -= min / 4;
	}

	/*
	 * Check watermarks for an order-0 allocation request. If these
	 * are not met, then a high-order request also cannot go ahead
	 * even if a suitable page happened to be free.
	 */
	if (free_pages <= min + z->lowmem_reserve[classzone_idx])
		return false;

	/* If this is an order-0 request then the watermark is fine */
	if (!order)
		return true;

	/* For a high-order request, check at least one suitable page is free */
	for (o = order; o < MAX_ORDER; o++) {
		struct free_area *area = &z->free_area[o];
		int mt;

		if (!area->nr_free)
			continue;

		for (mt = 0; mt < MIGRATE_PCPTYPES; mt++) {
#ifdef CONFIG_CMA
			/*
			 * Note that this check is needed only
			 * when MIGRATE_CMA < MIGRATE_PCPTYPES.
			 */
			if (mt == MIGRATE_CMA)
				continue;
#endif
			if (!list_empty(&area->free_list[mt]))
				return true;
		}

#ifdef CONFIG_CMA
		if ((alloc_flags & ALLOC_CMA) &&
		    !list_empty(&area->free_list[MIGRATE_CMA])) {
			return true;
		}
#endif
		if (alloc_harder &&
			!list_empty(&area->free_list[MIGRATE_HIGHATOMIC]))
			return true;
	}
	return false;
}

bool zone_watermark_ok(struct zone *z, unsigned int order, unsigned long mark,
		      int classzone_idx, unsigned int alloc_flags)
{
	return __zone_watermark_ok(z, order, mark, classzone_idx, alloc_flags,
					zone_page_state(z, NR_FREE_PAGES));
}

static inline bool zone_watermark_fast(struct zone *z, unsigned int order,
		unsigned long mark, int classzone_idx, unsigned int alloc_flags)
{
	long free_pages;

	free_pages = zone_page_state(z, NR_FREE_PAGES);

	/*
	 * Fast check for order-0 only. If this fails then the reserves
	 * need to be calculated.
	 */
	if (!order) {
		long fast_free;

		fast_free = free_pages;
		fast_free -= __zone_watermark_unusable_free(z, 0, alloc_flags);
		if (fast_free > mark + z->lowmem_reserve[classzone_idx])
			return true;
	}

	return __zone_watermark_ok(z, order, mark, classzone_idx, alloc_flags,
					free_pages);
}

bool zone_watermark_ok_safe(struct zone *z, unsigned int order,
			unsigned long mark, int classzone_idx)
{
	long free_pages = zone_page_state(z, NR_FREE_PAGES);

	if (z->percpu_drift_mark && free_pages < z->percpu_drift_mark)
		free_pages = zone_page_state_snapshot(z, NR_FREE_PAGES);

	return __zone_watermark_ok(z, order, mark, classzone_idx, 0,
								free_pages);
}

#ifdef CONFIG_NUMA
static bool zone_allows_reclaim(struct zone *local_zone, struct zone *zone)
{
	return node_distance(zone_to_nid(local_zone), zone_to_nid(zone)) <=
				RECLAIM_DISTANCE;
}
#else	/* CONFIG_NUMA */
static bool zone_allows_reclaim(struct zone *local_zone, struct zone *zone)
{
	return true;
}
#endif	/* CONFIG_NUMA */

/*
 * The restriction on ZONE_DMA32 as being a suitable zone to use to avoid
 * fragmentation is subtle. If the preferred zone was HIGHMEM then
 * premature use of a lower zone may cause lowmem pressure problems that
 * are worse than fragmentation. If the next zone is ZONE_DMA then it is
 * probably too small. It only makes sense to spread allocations to avoid
 * fragmentation between the Normal and DMA32 zones.
 */
static inline unsigned int
alloc_flags_nofragment(struct zone *zone, gfp_t gfp_mask)
{
	unsigned int alloc_flags = 0;

	if (gfp_mask & __GFP_KSWAPD_RECLAIM)
		alloc_flags |= ALLOC_KSWAPD;

#ifdef CONFIG_ZONE_DMA32
	if (!zone)
		return alloc_flags;

	if (zone_idx(zone) != ZONE_NORMAL)
		return alloc_flags;

	/*
	 * If ZONE_DMA32 exists, assume it is the one after ZONE_NORMAL and
	 * the pointer is within zone->zone_pgdat->node_zones[]. Also assume
	 * on UMA that if Normal is populated then so is DMA32.
	 */
	BUILD_BUG_ON(ZONE_NORMAL - ZONE_DMA32 != 1);
	if (nr_online_nodes > 1 && !populated_zone(--zone))
		return alloc_flags;

	alloc_flags |= ALLOC_NOFRAGMENT;
#endif /* CONFIG_ZONE_DMA32 */
	return alloc_flags;
}

/*
 * get_page_from_freelist goes through the zonelist trying to allocate
 * a page.
 */
static struct page *
get_page_from_freelist(gfp_t gfp_mask, unsigned int order, int alloc_flags,
						const struct alloc_context *ac)
{
	struct zoneref *z;
	struct zone *zone;
	struct pglist_data *last_pgdat_dirty_limit = NULL;
	bool no_fallback;

retry:
	/*
	 * Scan zonelist, looking for a zone with enough free.
	 * See also __cpuset_node_allowed() comment in kernel/cpuset.c.
	 */
	no_fallback = alloc_flags & ALLOC_NOFRAGMENT;
	z = ac->preferred_zoneref;
	for_next_zone_zonelist_nodemask(zone, z, ac->zonelist, ac->high_zoneidx,
								ac->nodemask) {
		struct page *page;
		unsigned long mark;

		if (cpusets_enabled() &&
			(alloc_flags & ALLOC_CPUSET) &&
			!__cpuset_zone_allowed(zone, gfp_mask))
				continue;
		/*
		 * When allocating a page cache page for writing, we
		 * want to get it from a node that is within its dirty
		 * limit, such that no single node holds more than its
		 * proportional share of globally allowed dirty pages.
		 * The dirty limits take into account the node's
		 * lowmem reserves and high watermark so that kswapd
		 * should be able to balance it without having to
		 * write pages from its LRU list.
		 *
		 * XXX: For now, allow allocations to potentially
		 * exceed the per-node dirty limit in the slowpath
		 * (spread_dirty_pages unset) before going into reclaim,
		 * which is important when on a NUMA setup the allowed
		 * nodes are together not big enough to reach the
		 * global limit.  The proper fix for these situations
		 * will require awareness of nodes in the
		 * dirty-throttling and the flusher threads.
		 */
		if (ac->spread_dirty_pages) {
			if (last_pgdat_dirty_limit == zone->zone_pgdat)
				continue;

			if (!node_dirty_ok(zone->zone_pgdat)) {
				last_pgdat_dirty_limit = zone->zone_pgdat;
				continue;
			}
		}

		if (no_fallback && nr_online_nodes > 1 &&
		    zone != ac->preferred_zoneref->zone) {
			int local_nid;

			/*
			 * If moving to a remote node, retry but allow
			 * fragmenting fallbacks. Locality is more important
			 * than fragmentation avoidance.
			 */
			local_nid = zone_to_nid(ac->preferred_zoneref->zone);
			if (zone_to_nid(zone) != local_nid) {
				alloc_flags &= ~ALLOC_NOFRAGMENT;
				goto retry;
			}
		}

		mark = wmark_pages(zone, alloc_flags & ALLOC_WMARK_MASK);
		/*
		 * Allow high, atomic, harder order-0 allocation requests
		 * to skip the ->watermark_boost for min watermark check.
		 * In doing so, check for:
		 *  1) ALLOC_WMARK_MIN - Allow to wake up kswapd in the
		 *			 slow path.
		 *  2) ALLOC_HIGH - Allow high priority requests.
		 *  3) ALLOC_HARDER - Allow (__GFP_ATOMIC && !__GFP_NOMEMALLOC),
		 *			of the others.
		 */
		if (unlikely(!order && !(alloc_flags & ALLOC_WMARK_MASK) &&
		     (alloc_flags & (ALLOC_HARDER | ALLOC_HIGH)))) {
			mark = zone->_watermark[WMARK_MIN];
		}
		if (!zone_watermark_fast(zone, order, mark,
				       ac_classzone_idx(ac), alloc_flags)) {
			int ret;

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
			/*
			 * Watermark failed for this zone, but see if we can
			 * grow this zone if it contains deferred pages.
			 */
			if (static_branch_unlikely(&deferred_pages)) {
				if (_deferred_grow_zone(zone, order))
					goto try_this_zone;
			}
#endif
			/* Checked here to keep the fast path fast */
			BUILD_BUG_ON(ALLOC_NO_WATERMARKS < NR_WMARK);
			if (alloc_flags & ALLOC_NO_WATERMARKS)
				goto try_this_zone;

			if (node_reclaim_mode == 0 ||
			    !zone_allows_reclaim(ac->preferred_zoneref->zone, zone))
				continue;

			ret = node_reclaim(zone->zone_pgdat, gfp_mask, order);
			switch (ret) {
			case NODE_RECLAIM_NOSCAN:
				/* did not scan */
				continue;
			case NODE_RECLAIM_FULL:
				/* scanned but unreclaimable */
				continue;
			default:
				/* did we reclaim enough */
				if (zone_watermark_ok(zone, order, mark,
						ac_classzone_idx(ac), alloc_flags))
					goto try_this_zone;

				continue;
			}
		}

try_this_zone:
		page = rmqueue(ac->preferred_zoneref->zone, zone, order,
				gfp_mask, alloc_flags, ac->migratetype);
		if (page) {
			prep_new_page(page, order, gfp_mask, alloc_flags);

			/*
			 * If this is a high-order atomic allocation then check
			 * if the pageblock should be reserved for the future
			 */
			if (unlikely(order && (alloc_flags & ALLOC_HARDER)))
				reserve_highatomic_pageblock(page, zone, order);

			return page;
		} else {
#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
			/* Try again if zone has deferred pages */
			if (static_branch_unlikely(&deferred_pages)) {
				if (_deferred_grow_zone(zone, order))
					goto try_this_zone;
			}
#endif
		}
	}

	/*
	 * It's possible on a UMA machine to get through all zones that are
	 * fragmented. If avoiding fragmentation, reset and try again.
	 */
	if (no_fallback) {
		alloc_flags &= ~ALLOC_NOFRAGMENT;
		goto retry;
	}

	return NULL;
}

/*
 * Large machines with many possible nodes should not always dump per-node
 * meminfo in irq context.
 */
static inline bool should_suppress_show_mem(void)
{
	bool ret = false;

#if NODES_SHIFT > 8
	ret = in_interrupt();
#endif
	return ret;
}

static void warn_alloc_show_mem(gfp_t gfp_mask, nodemask_t *nodemask)
{
	unsigned int filter = SHOW_MEM_FILTER_NODES;
	static DEFINE_RATELIMIT_STATE(show_mem_rs, HZ, 1);

	if (should_suppress_show_mem() || !__ratelimit(&show_mem_rs))
		return;

	/*
	 * This documents exceptions given to allocations in certain
	 * contexts that are allowed to allocate outside current's set
	 * of allowed nodes.
	 */
	if (!(gfp_mask & __GFP_NOMEMALLOC))
		if (tsk_is_oom_victim(current) ||
		    (current->flags & (PF_MEMALLOC | PF_EXITING)))
			filter &= ~SHOW_MEM_FILTER_NODES;
	if (in_interrupt() || !(gfp_mask & __GFP_DIRECT_RECLAIM))
		filter &= ~SHOW_MEM_FILTER_NODES;

	show_mem(filter, nodemask);
	show_mem_call_notifiers();
}

void warn_alloc(gfp_t gfp_mask, nodemask_t *nodemask, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	static DEFINE_RATELIMIT_STATE(nopage_rs, DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);

	if ((gfp_mask & __GFP_NOWARN) || !__ratelimit(&nopage_rs))
		return;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	pr_warn("%s: %pV, mode:%#x(%pGg), fatal_signal:%d, nodemask=%*pbl\n",
			current->comm, &vaf, gfp_mask, &gfp_mask,
			fatal_signal_pending(current) ? 1 : 0,
			nodemask_pr_args(nodemask));
	va_end(args);

	cpuset_print_current_mems_allowed();

	dump_stack();
	warn_alloc_show_mem(gfp_mask, nodemask);
}

static inline struct page *
__alloc_pages_cpuset_fallback(gfp_t gfp_mask, unsigned int order,
			      unsigned int alloc_flags,
			      const struct alloc_context *ac)
{
	struct page *page;

	page = get_page_from_freelist(gfp_mask, order,
			alloc_flags|ALLOC_CPUSET, ac);
	/*
	 * fallback to ignore cpuset restriction if our nodes
	 * are depleted
	 */
	if (!page)
		page = get_page_from_freelist(gfp_mask, order,
				alloc_flags, ac);

	return page;
}

static inline struct page *
__alloc_pages_may_oom(gfp_t gfp_mask, unsigned int order,
	const struct alloc_context *ac, unsigned long *did_some_progress)
{
	struct oom_control oc = {
		.zonelist = ac->zonelist,
		.nodemask = ac->nodemask,
		.memcg = NULL,
		.gfp_mask = gfp_mask,
		.order = order,
	};
	struct page *page;

	*did_some_progress = 0;

	/*
	 * Acquire the oom lock.  If that fails, somebody else is
	 * making progress for us.
	 */
	if (!mutex_trylock(&oom_lock)) {
		*did_some_progress = 1;
		schedule_timeout_uninterruptible(1);
		return NULL;
	}

	/*
	 * Go through the zonelist yet one more time, keep very high watermark
	 * here, this is only to catch a parallel oom killing, we must fail if
	 * we're still under heavy pressure. But make sure that this reclaim
	 * attempt shall not depend on __GFP_DIRECT_RECLAIM && !__GFP_NORETRY
	 * allocation which will never fail due to oom_lock already held.
	 */
	page = get_page_from_freelist((gfp_mask | __GFP_HARDWALL) &
				      ~__GFP_DIRECT_RECLAIM, order,
				      ALLOC_WMARK_HIGH|ALLOC_CPUSET, ac);
	if (page)
		goto out;

	/* Coredumps can quickly deplete all memory reserves */
	if (current->flags & PF_DUMPCORE)
		goto out;
	/* The OOM killer will not help higher order allocs */
	if (order > PAGE_ALLOC_COSTLY_ORDER)
		goto out;
	/*
	 * We have already exhausted all our reclaim opportunities without any
	 * success so it is time to admit defeat. We will skip the OOM killer
	 * because it is very likely that the caller has a more reasonable
	 * fallback than shooting a random task.
	 */
	if (gfp_mask & __GFP_RETRY_MAYFAIL)
		goto out;
	/* The OOM killer does not needlessly kill tasks for lowmem */
	if (ac->high_zoneidx < ZONE_NORMAL)
		goto out;
	if (pm_suspended_storage())
		goto out;
	/*
	 * XXX: GFP_NOFS allocations should rather fail than rely on
	 * other request to make a forward progress.
	 * We are in an unfortunate situation where out_of_memory cannot
	 * do much for this context but let's try it to at least get
	 * access to memory reserved if the current task is killed (see
	 * out_of_memory). Once filesystems are ready to handle allocation
	 * failures more gracefully we should just bail out here.
	 */

	/* The OOM killer may not free memory on a specific node */
	if (gfp_mask & __GFP_THISNODE)
		goto out;

	/* Exhausted what can be done so it's blame time */
	if (out_of_memory(&oc) || WARN_ON_ONCE(gfp_mask & __GFP_NOFAIL)) {
		*did_some_progress = 1;

		/*
		 * Help non-failing allocations by giving them access to memory
		 * reserves
		 */
		if (gfp_mask & __GFP_NOFAIL)
			page = __alloc_pages_cpuset_fallback(gfp_mask, order,
					ALLOC_NO_WATERMARKS, ac);
	}
out:
	mutex_unlock(&oom_lock);
	return page;
}

/*
 * Maximum number of compaction retries wit a progress before OOM
 * killer is consider as the only way to move forward.
 */
#define MAX_COMPACT_RETRIES 16

#ifdef CONFIG_COMPACTION
/* Try memory compaction for high-order allocations before reclaim */
static struct page *
__alloc_pages_direct_compact(gfp_t gfp_mask, unsigned int order,
		unsigned int alloc_flags, const struct alloc_context *ac,
		enum compact_priority prio, enum compact_result *compact_result)
{
	struct page *page = NULL;
	unsigned long pflags;
	unsigned int noreclaim_flag;

	if (!order)
		return NULL;

	psi_memstall_enter(&pflags);
	noreclaim_flag = memalloc_noreclaim_save();

	*compact_result = try_to_compact_pages(gfp_mask, order, alloc_flags, ac,
								prio, &page);

	memalloc_noreclaim_restore(noreclaim_flag);
	psi_memstall_leave(&pflags);

	/*
	 * At least in one zone compaction wasn't deferred or skipped, so let's
	 * count a compaction stall
	 */
	count_vm_event(COMPACTSTALL);

	/* Prep a captured page if available */
	if (page)
		prep_new_page(page, order, gfp_mask, alloc_flags);

	/* Try get a page from the freelist if available */
	if (!page)
		page = get_page_from_freelist(gfp_mask, order, alloc_flags, ac);

	if (page) {
		struct zone *zone = page_zone(page);

		zone->compact_blockskip_flush = false;
		compaction_defer_reset(zone, order, true);
		count_vm_event(COMPACTSUCCESS);
		return page;
	}

	/*
	 * It's bad if compaction run occurs and fails. The most likely reason
	 * is that pages exist, but not enough to satisfy watermarks.
	 */
	count_vm_event(COMPACTFAIL);

	cond_resched();

	return NULL;
}

static inline bool
should_compact_retry(struct alloc_context *ac, int order, int alloc_flags,
		     enum compact_result compact_result,
		     enum compact_priority *compact_priority,
		     int *compaction_retries)
{
	int max_retries = MAX_COMPACT_RETRIES;
	int min_priority;
	bool ret = false;
	int retries = *compaction_retries;
	enum compact_priority priority = *compact_priority;

	if (!order)
		return false;

	if (compaction_made_progress(compact_result))
		(*compaction_retries)++;

	/*
	 * compaction considers all the zone as desperately out of memory
	 * so it doesn't really make much sense to retry except when the
	 * failure could be caused by insufficient priority
	 */
	if (compaction_failed(compact_result))
		goto check_priority;

	/*
	 * make sure the compaction wasn't deferred or didn't bail out early
	 * due to locks contention before we declare that we should give up.
	 * But do not retry if the given zonelist is not suitable for
	 * compaction.
	 */
	if (compaction_withdrawn(compact_result)) {
		ret = compaction_zonelist_suitable(ac, order, alloc_flags);
		goto out;
	}

	/*
	 * !costly requests are much more important than __GFP_RETRY_MAYFAIL
	 * costly ones because they are de facto nofail and invoke OOM
	 * killer to move on while costly can fail and users are ready
	 * to cope with that. 1/4 retries is rather arbitrary but we
	 * would need much more detailed feedback from compaction to
	 * make a better decision.
	 */
	if (order > PAGE_ALLOC_COSTLY_ORDER)
		max_retries /= 4;
	if (*compaction_retries <= max_retries) {
		ret = true;
		goto out;
	}

	/*
	 * Make sure there are attempts at the highest priority if we exhausted
	 * all retries or failed at the lower priorities.
	 */
check_priority:
	min_priority = (order > PAGE_ALLOC_COSTLY_ORDER) ?
			MIN_COMPACT_COSTLY_PRIORITY : MIN_COMPACT_PRIORITY;

	if (*compact_priority > min_priority) {
		(*compact_priority)--;
		*compaction_retries = 0;
		ret = true;
	}
out:
	trace_compact_retry(order, priority, compact_result, retries, max_retries, ret);
	return ret;
}
#else
static inline struct page *
__alloc_pages_direct_compact(gfp_t gfp_mask, unsigned int order,
		unsigned int alloc_flags, const struct alloc_context *ac,
		enum compact_priority prio, enum compact_result *compact_result)
{
	*compact_result = COMPACT_SKIPPED;
	return NULL;
}

static inline bool
should_compact_retry(struct alloc_context *ac, unsigned int order, int alloc_flags,
		     enum compact_result compact_result,
		     enum compact_priority *compact_priority,
		     int *compaction_retries)
{
	struct zone *zone;
	struct zoneref *z;

	if (!order || order > PAGE_ALLOC_COSTLY_ORDER)
		return false;

	/*
	 * There are setups with compaction disabled which would prefer to loop
	 * inside the allocator rather than hit the oom killer prematurely.
	 * Let's give them a good hope and keep retrying while the order-0
	 * watermarks are OK.
	 */
	for_each_zone_zonelist_nodemask(zone, z, ac->zonelist, ac->high_zoneidx,
					ac->nodemask) {
		if (zone_watermark_ok(zone, 0, min_wmark_pages(zone),
					ac_classzone_idx(ac), alloc_flags))
			return true;
	}
	return false;
}
#endif /* CONFIG_COMPACTION */

#ifdef CONFIG_LOCKDEP
static struct lockdep_map __fs_reclaim_map =
	STATIC_LOCKDEP_MAP_INIT("fs_reclaim", &__fs_reclaim_map);

static bool __need_fs_reclaim(gfp_t gfp_mask)
{
	gfp_mask = current_gfp_context(gfp_mask);

	/* no reclaim without waiting on it */
	if (!(gfp_mask & __GFP_DIRECT_RECLAIM))
		return false;

	/* this guy won't enter reclaim */
	if (current->flags & PF_MEMALLOC)
		return false;

	/* We're only interested __GFP_FS allocations for now */
	if (!(gfp_mask & __GFP_FS))
		return false;

	if (gfp_mask & __GFP_NOLOCKDEP)
		return false;

	return true;
}

void __fs_reclaim_acquire(void)
{
	lock_map_acquire(&__fs_reclaim_map);
}

void __fs_reclaim_release(void)
{
	lock_map_release(&__fs_reclaim_map);
}

void fs_reclaim_acquire(gfp_t gfp_mask)
{
	if (__need_fs_reclaim(gfp_mask))
		__fs_reclaim_acquire();
}
EXPORT_SYMBOL_GPL(fs_reclaim_acquire);

void fs_reclaim_release(gfp_t gfp_mask)
{
	if (__need_fs_reclaim(gfp_mask))
		__fs_reclaim_release();
}
EXPORT_SYMBOL_GPL(fs_reclaim_release);
#endif

/*
 * Zonelists may change due to hotplug during allocation. Detect when zonelists
 * have been rebuilt so allocation retries. Reader side does not lock and
 * retries the allocation if zonelist changes. Writer side is protected by the
 * embedded spin_lock.
 */
static DEFINE_SEQLOCK(zonelist_update_seq);

static unsigned int zonelist_iter_begin(void)
{
	if (IS_ENABLED(CONFIG_MEMORY_HOTREMOVE))
		return read_seqbegin(&zonelist_update_seq);

	return 0;
}

static unsigned int check_retry_zonelist(unsigned int seq)
{
	if (IS_ENABLED(CONFIG_MEMORY_HOTREMOVE))
		return read_seqretry(&zonelist_update_seq, seq);

	return seq;
}

/* Perform direct synchronous page reclaim */
static int
__perform_reclaim(gfp_t gfp_mask, unsigned int order,
					const struct alloc_context *ac)
{
	struct reclaim_state reclaim_state = {};
	int progress;
	unsigned int noreclaim_flag;
	unsigned long pflags;

	cond_resched();

	/* We now go into synchronous reclaim */
	cpuset_memory_pressure_bump();
	psi_memstall_enter(&pflags);
	fs_reclaim_acquire(gfp_mask);
	noreclaim_flag = memalloc_noreclaim_save();
	reclaim_state.reclaimed_slab = 0;
	current->reclaim_state = &reclaim_state;

	progress = try_to_free_pages(ac->zonelist, order, gfp_mask,
								ac->nodemask);

	current->reclaim_state = NULL;
	memalloc_noreclaim_restore(noreclaim_flag);
	fs_reclaim_release(gfp_mask);
	psi_memstall_leave(&pflags);

	cond_resched();

	return progress;
}

/* The really slow allocator path where we enter direct reclaim */
static inline struct page *
__alloc_pages_direct_reclaim(gfp_t gfp_mask, unsigned int order,
		unsigned int alloc_flags, const struct alloc_context *ac,
		unsigned long *did_some_progress)
{
	struct page *page = NULL;
	bool drained = false;

	*did_some_progress = __perform_reclaim(gfp_mask, order, ac);
	if (unlikely(!(*did_some_progress)))
		return NULL;

retry:
	page = get_page_from_freelist(gfp_mask, order, alloc_flags, ac);

	/*
	 * If an allocation failed after direct reclaim, it could be because
	 * pages are pinned on the per-cpu lists or in high alloc reserves.
	 * Shrink them them and try again
	 */
	if (!page && !drained) {
		unreserve_highatomic_pageblock(ac, false);
		if (!need_memory_boosting(NULL))
			drain_all_pages(NULL);
		drained = true;
		goto retry;
	}

	return page;
}

static void wake_all_kswapds(unsigned int order, gfp_t gfp_mask,
			     const struct alloc_context *ac)
{
	struct zoneref *z;
	struct zone *zone;
	pg_data_t *last_pgdat = NULL;
	enum zone_type high_zoneidx = ac->high_zoneidx;

	for_each_zone_zonelist_nodemask(zone, z, ac->zonelist, high_zoneidx,
					ac->nodemask) {
		if (last_pgdat != zone->zone_pgdat)
			wakeup_kswapd(zone, gfp_mask, order, high_zoneidx);
		last_pgdat = zone->zone_pgdat;
	}
}

static inline unsigned int
gfp_to_alloc_flags(gfp_t gfp_mask)
{
	unsigned int alloc_flags = ALLOC_WMARK_MIN | ALLOC_CPUSET;

	/* __GFP_HIGH is assumed to be the same as ALLOC_HIGH to save a branch. */
	BUILD_BUG_ON(__GFP_HIGH != (__force gfp_t) ALLOC_HIGH);

	/*
	 * The caller may dip into page reserves a bit more if the caller
	 * cannot run direct reclaim, or if the caller has realtime scheduling
	 * policy or is asking for __GFP_HIGH memory.  GFP_ATOMIC requests will
	 * set both ALLOC_HARDER (__GFP_ATOMIC) and ALLOC_HIGH (__GFP_HIGH).
	 */
	alloc_flags |= (__force int) (gfp_mask & __GFP_HIGH);

	if (gfp_mask & __GFP_ATOMIC) {
		/*
		 * Not worth trying to allocate harder for __GFP_NOMEMALLOC even
		 * if it can't schedule.
		 */
		if (!(gfp_mask & __GFP_NOMEMALLOC))
			alloc_flags |= ALLOC_HARDER;
		/*
		 * Ignore cpuset mems for GFP_ATOMIC rather than fail, see the
		 * comment for __cpuset_node_allowed().
		 */
		alloc_flags &= ~ALLOC_CPUSET;
	} else if (unlikely(rt_task(current)) && !in_interrupt())
		alloc_flags |= ALLOC_HARDER;

	if (gfp_mask & __GFP_KSWAPD_RECLAIM)
		alloc_flags |= ALLOC_KSWAPD;

#ifdef CONFIG_CMA
	if ((gfpflags_to_migratetype(gfp_mask) == MIGRATE_MOVABLE) &&
				(gfp_mask & __GFP_CMA))
		alloc_flags |= ALLOC_CMA;
#endif
	return alloc_flags;
}

static bool oom_reserves_allowed(struct task_struct *tsk)
{
	if (!tsk_is_oom_victim(tsk))
		return false;

	/*
	 * !MMU doesn't have oom reaper so give access to memory reserves
	 * only to the thread with TIF_MEMDIE set
	 */
	if (!IS_ENABLED(CONFIG_MMU) && !test_thread_flag(TIF_MEMDIE))
		return false;

	return true;
}

/*
 * Distinguish requests which really need access to full memory
 * reserves from oom victims which can live with a portion of it
 */
static inline int __gfp_pfmemalloc_flags(gfp_t gfp_mask)
{
	if (unlikely(gfp_mask & __GFP_NOMEMALLOC))
		return 0;
	if (gfp_mask & __GFP_MEMALLOC)
		return ALLOC_NO_WATERMARKS;
	if (in_serving_softirq() && (current->flags & PF_MEMALLOC))
		return ALLOC_NO_WATERMARKS;
	if (!in_interrupt()) {
		if (current->flags & PF_MEMALLOC)
			return ALLOC_NO_WATERMARKS;
		else if (oom_reserves_allowed(current))
			return ALLOC_OOM;
	}

	return 0;
}

bool gfp_pfmemalloc_allowed(gfp_t gfp_mask)
{
	return !!__gfp_pfmemalloc_flags(gfp_mask);
}

/*
 * Checks whether it makes sense to retry the reclaim to make a forward progress
 * for the given allocation request.
 *
 * We give up when we either have tried MAX_RECLAIM_RETRIES in a row
 * without success, or when we couldn't even meet the watermark if we
 * reclaimed all remaining pages on the LRU lists.
 *
 * Returns true if a retry is viable or false to enter the oom path.
 */
static inline bool
should_reclaim_retry(gfp_t gfp_mask, unsigned order,
		     struct alloc_context *ac, int alloc_flags,
		     bool did_some_progress, int *no_progress_loops)
{
	struct zone *zone;
	struct zoneref *z;

	/*
	 * Costly allocations might have made a progress but this doesn't mean
	 * their order will become available due to high fragmentation so
	 * always increment the no progress counter for them
	 */
	if (did_some_progress && order <= PAGE_ALLOC_COSTLY_ORDER)
		*no_progress_loops = 0;
	else
		(*no_progress_loops)++;

	/*
	 * Make sure we converge to OOM if we cannot make any progress
	 * several times in the row.
	 */
	if (*no_progress_loops > MAX_RECLAIM_RETRIES) {
		/* Before OOM, exhaust highatomic_reserve */
		return unreserve_highatomic_pageblock(ac, true);
	}

	/*
	 * Keep reclaiming pages while there is a chance this will lead
	 * somewhere.  If none of the target zones can satisfy our allocation
	 * request even if all reclaimable pages are considered then we are
	 * screwed and have to go OOM.
	 */
	for_each_zone_zonelist_nodemask(zone, z, ac->zonelist, ac->high_zoneidx,
					ac->nodemask) {
		unsigned long available;
		unsigned long reclaimable;
		unsigned long min_wmark = min_wmark_pages(zone);
		bool wmark;

		available = reclaimable = zone_reclaimable_pages(zone);
		available += zone_page_state_snapshot(zone, NR_FREE_PAGES);

		/*
		 * Would the allocation succeed if we reclaimed all
		 * reclaimable pages?
		 */
		wmark = __zone_watermark_ok(zone, order, min_wmark,
				ac_classzone_idx(ac), alloc_flags, available);
		trace_reclaim_retry_zone(z, order, reclaimable,
				available, min_wmark, *no_progress_loops, wmark);
		if (wmark) {
			/*
			 * If we didn't make any progress and have a lot of
			 * dirty + writeback pages then we should wait for
			 * an IO to complete to slow down the reclaim and
			 * prevent from pre mature OOM
			 */
			if (!did_some_progress) {
				unsigned long write_pending;

				write_pending = zone_page_state_snapshot(zone,
							NR_ZONE_WRITE_PENDING);

				if (2 * write_pending > reclaimable) {
					congestion_wait(BLK_RW_ASYNC, HZ/10);
					return true;
				}
			}

			/*
			 * Memory allocation/reclaim might be called from a WQ
			 * context and the current implementation of the WQ
			 * concurrency control doesn't recognize that
			 * a particular WQ is congested if the worker thread is
			 * looping without ever sleeping. Therefore we have to
			 * do a short sleep here rather than calling
			 * cond_resched().
			 */
			if (current->flags & PF_WQ_WORKER)
				schedule_timeout_uninterruptible(1);
			else
				cond_resched();

			return true;
		}
	}

	return false;
}

static inline bool
check_retry_cpuset(int cpuset_mems_cookie, struct alloc_context *ac)
{
	/*
	 * It's possible that cpuset's mems_allowed and the nodemask from
	 * mempolicy don't intersect. This should be normally dealt with by
	 * policy_nodemask(), but it's possible to race with cpuset update in
	 * such a way the check therein was true, and then it became false
	 * before we got our cpuset_mems_cookie here.
	 * This assumes that for all allocations, ac->nodemask can come only
	 * from MPOL_BIND mempolicy (whose documented semantics is to be ignored
	 * when it does not intersect with the cpuset restrictions) or the
	 * caller can deal with a violated nodemask.
	 */
	if (cpusets_enabled() && ac->nodemask &&
			!cpuset_nodemask_valid_mems_allowed(ac->nodemask)) {
		ac->nodemask = NULL;
		return true;
	}

	/*
	 * When updating a task's mems_allowed or mempolicy nodemask, it is
	 * possible to race with parallel threads in such a way that our
	 * allocation can fail while the mask is being updated. If we are about
	 * to fail, check if the cpuset changed during allocation and if so,
	 * retry.
	 */
	if (read_mems_allowed_retry(cpuset_mems_cookie))
		return true;

	return false;
}

static inline struct page *
__alloc_pages_slowpath(gfp_t gfp_mask, unsigned int order,
						struct alloc_context *ac)
{
	bool can_direct_reclaim = gfp_mask & __GFP_DIRECT_RECLAIM;
	const bool costly_order = order > PAGE_ALLOC_COSTLY_ORDER;
	struct page *page = NULL;
	unsigned int alloc_flags;
	unsigned long did_some_progress = 0;
	enum compact_priority compact_priority;
	enum compact_result compact_result;
	int compaction_retries;
	int no_progress_loops;
	unsigned int cpuset_mems_cookie;
	unsigned int zonelist_iter_cookie;
	int reserve_flags;
	unsigned long pages_reclaimed = 0;
	int retry_loop_count = 0;
	unsigned long jiffies_s = jiffies;
	u64 utime, stime_s, stime_e, stime_d;

	task_cputime(current, &utime, &stime_s);

	/*
	 * We also sanity check to catch abuse of atomic reserves being used by
	 * callers that are not in atomic context.
	 */
	if (WARN_ON_ONCE((gfp_mask & (__GFP_ATOMIC|__GFP_DIRECT_RECLAIM)) ==
				(__GFP_ATOMIC|__GFP_DIRECT_RECLAIM)))
		gfp_mask &= ~__GFP_ATOMIC;

restart:
	compaction_retries = 0;
	no_progress_loops = 0;
	compact_priority = DEF_COMPACT_PRIORITY;
	cpuset_mems_cookie = read_mems_allowed_begin();
	zonelist_iter_cookie = zonelist_iter_begin();

	/*
	 * The fast path uses conservative alloc_flags to succeed only until
	 * kswapd needs to be woken up, and to avoid the cost of setting up
	 * alloc_flags precisely. So we do that now.
	 */
	alloc_flags = gfp_to_alloc_flags(gfp_mask);

	/*
	 * We need to recalculate the starting point for the zonelist iterator
	 * because we might have used different nodemask in the fast path, or
	 * there was a cpuset modification and we are retrying - otherwise we
	 * could end up iterating over non-eligible zones endlessly.
	 */
	ac->preferred_zoneref = first_zones_zonelist(ac->zonelist,
					ac->high_zoneidx, ac->nodemask);
	if (!ac->preferred_zoneref->zone)
		goto nopage;

	if (alloc_flags & ALLOC_KSWAPD)
		wake_all_kswapds(order, gfp_mask, ac);

	/*
	 * The adjusted alloc_flags might result in immediate success, so try
	 * that first
	 */
	page = get_page_from_freelist(gfp_mask, order, alloc_flags, ac);
	if (page)
		goto got_pg;

	/*
	 * For costly allocations, try direct compaction first, as it's likely
	 * that we have enough base pages and don't need to reclaim. For non-
	 * movable high-order allocations, do that as well, as compaction will
	 * try prevent permanent fragmentation by migrating from blocks of the
	 * same migratetype.
	 * Don't try this for allocations that are allowed to ignore
	 * watermarks, as the ALLOC_NO_WATERMARKS attempt didn't yet happen.
	 */
	if (can_direct_reclaim &&
			(costly_order ||
			   (order > 0 && ac->migratetype != MIGRATE_MOVABLE))
			&& !gfp_pfmemalloc_allowed(gfp_mask)) {
		page = __alloc_pages_direct_compact(gfp_mask, order,
						alloc_flags, ac,
						INIT_COMPACT_PRIORITY,
						&compact_result);
		if (page)
			goto got_pg;

		 if (order >= pageblock_order && (gfp_mask & __GFP_IO)) {
			/*
			 * If allocating entire pageblock(s) and compaction
			 * failed because all zones are below low watermarks
			 * or is prohibited because it recently failed at this
			 * order, fail immediately.
			 *
			 * Reclaim is
			 *  - potentially very expensive because zones are far
			 *    below their low watermarks or this is part of very
			 *    bursty high order allocations,
			 *  - not guaranteed to help because isolate_freepages()
			 *    may not iterate over freed pages as part of its
			 *    linear scan, and
			 *  - unlikely to make entire pageblocks free on its
			 *    own.
			 */
			if (compact_result == COMPACT_SKIPPED ||
			    compact_result == COMPACT_DEFERRED)
				goto nopage;
		}

		/*
		 * Checks for costly allocations with __GFP_NORETRY, which
		 * includes THP page fault allocations
		 */
		if (costly_order && (gfp_mask & __GFP_NORETRY)) {
			/*
			 * If compaction is deferred for high-order allocations,
			 * it is because sync compaction recently failed. If
			 * this is the case and the caller requested a THP
			 * allocation, we do not want to heavily disrupt the
			 * system, so we fail the allocation instead of entering
			 * direct reclaim.
			 */
			if (compact_result == COMPACT_DEFERRED)
				goto nopage;

			/*
			 * Looks like reclaim/compaction is worth trying, but
			 * sync compaction could be very expensive, so keep
			 * using async compaction.
			 */
			compact_priority = INIT_COMPACT_PRIORITY;
		}
	}

retry:
	retry_loop_count++;
	/* Ensure kswapd doesn't accidentally go to sleep as long as we loop */
	if (alloc_flags & ALLOC_KSWAPD)
		wake_all_kswapds(order, gfp_mask, ac);

	reserve_flags = __gfp_pfmemalloc_flags(gfp_mask);
	if (reserve_flags)
		alloc_flags = reserve_flags;

	/*
	 * Reset the nodemask and zonelist iterators if memory policies can be
	 * ignored. These allocations are high priority and system rather than
	 * user oriented.
	 */
	if (!(alloc_flags & ALLOC_CPUSET) || reserve_flags) {
		ac->nodemask = NULL;
		ac->preferred_zoneref = first_zones_zonelist(ac->zonelist,
					ac->high_zoneidx, ac->nodemask);
	}

	/* Attempt with potentially adjusted zonelist and alloc_flags */
	page = get_page_from_freelist(gfp_mask, order, alloc_flags, ac);
	if (page)
		goto got_pg;

	/* Caller is not willing to reclaim, we can't balance anything */
	if (!can_direct_reclaim)
		goto nopage;

	/* Avoid recursion of direct reclaim */
	if (current->flags & PF_MEMALLOC)
		goto nopage;

	if (fatal_signal_pending(current) && !(gfp_mask & __GFP_NOFAIL) &&
			(gfp_mask & __GFP_FS))
		goto nopage;

	/* Try direct reclaim and then allocating */
	page = __alloc_pages_direct_reclaim(gfp_mask, order, alloc_flags, ac,
							&did_some_progress);
	pages_reclaimed += did_some_progress;
	if (page)
		goto got_pg;

	/* Try direct compaction and then allocating */
	page = __alloc_pages_direct_compact(gfp_mask, order, alloc_flags, ac,
					compact_priority, &compact_result);
	if (page)
		goto got_pg;

	/* Do not loop if specifically requested */
	if (gfp_mask & __GFP_NORETRY)
		goto nopage;

	/*
	 * Do not retry costly high order allocations unless they are
	 * __GFP_RETRY_MAYFAIL
	 */
	if (costly_order && !(gfp_mask & __GFP_RETRY_MAYFAIL))
		goto nopage;

	if (should_reclaim_retry(gfp_mask, order, ac, alloc_flags,
				 did_some_progress > 0, &no_progress_loops))
		goto retry;

	/*
	 * It doesn't make any sense to retry for the compaction if the order-0
	 * reclaim is not able to make any progress because the current
	 * implementation of the compaction depends on the sufficient amount
	 * of free memory (see __compaction_suitable)
	 */
	if (did_some_progress > 0 &&
			should_compact_retry(ac, order, alloc_flags,
				compact_result, &compact_priority,
				&compaction_retries))
		goto retry;

	if (order <= PAGE_ALLOC_COSTLY_ORDER && should_ulmk_retry(gfp_mask))
		goto retry;

	/*
	 * Deal with possible cpuset update races or zonelist updates to avoid
	 * a unnecessary OOM kill.
	 */
	if (check_retry_cpuset(cpuset_mems_cookie, ac) ||
	    check_retry_zonelist(zonelist_iter_cookie))
		goto restart;

	/* Reclaim has failed us, start killing things */
	page = __alloc_pages_may_oom(gfp_mask, order, ac, &did_some_progress);
	if (page)
		goto got_pg;

	/* Avoid allocations with no watermarks from looping endlessly */
	if (tsk_is_oom_victim(current) &&
	    (alloc_flags == ALLOC_OOM ||
	     (gfp_mask & __GFP_NOMEMALLOC)))
		goto nopage;

	/* Retry as long as the OOM killer is making progress */
	if (did_some_progress) {
		no_progress_loops = 0;
		goto retry;
	}

nopage:
	/*
	 * Deal with possible cpuset update races or zonelist updates to avoid
	 * a unnecessary OOM kill.
	 */
	if (check_retry_cpuset(cpuset_mems_cookie, ac) ||
	    check_retry_zonelist(zonelist_iter_cookie))
		goto restart;

	/*
	 * Make sure that __GFP_NOFAIL request doesn't leak out and make sure
	 * we always retry
	 */
	if (gfp_mask & __GFP_NOFAIL) {
		/*
		 * All existing users of the __GFP_NOFAIL are blockable, so warn
		 * of any new users that actually require GFP_NOWAIT
		 */
		if (WARN_ON_ONCE(!can_direct_reclaim))
			goto fail;

		/*
		 * PF_MEMALLOC request from this context is rather bizarre
		 * because we cannot reclaim anything and only can loop waiting
		 * for somebody to do a work for us
		 */
		WARN_ON_ONCE(current->flags & PF_MEMALLOC);

		/*
		 * non failing costly orders are a hard requirement which we
		 * are not prepared for much so let's warn about these users
		 * so that we can identify them and convert them to something
		 * else.
		 */
		WARN_ON_ONCE(order > PAGE_ALLOC_COSTLY_ORDER);

		/*
		 * Help non-failing allocations by giving them access to memory
		 * reserves but do not use ALLOC_NO_WATERMARKS because this
		 * could deplete whole memory reserves which would just make
		 * the situation worse
		 */
		page = __alloc_pages_cpuset_fallback(gfp_mask, order, ALLOC_HARDER, ac);
		if (page)
			goto got_pg;

		cond_resched();
		goto retry;
	}
fail:
	warn_alloc(gfp_mask, ac->nodemask,
			"page allocation failure: order:%u", order);
got_pg:
	task_cputime(current, &utime, &stime_e);
	stime_d = stime_e - stime_s;
	if (stime_d / NSEC_PER_MSEC > 256) {
		pg_data_t *pgdat;

		unsigned long a_anon = 0;
		unsigned long in_anon = 0;
		unsigned long a_file = 0;
		unsigned long in_file = 0;
		for_each_online_pgdat(pgdat) {
			a_anon += node_page_state(pgdat, NR_ACTIVE_ANON);
			in_anon += node_page_state(pgdat, NR_INACTIVE_ANON);
			a_file += node_page_state(pgdat, NR_ACTIVE_FILE);
			in_file += node_page_state(pgdat, NR_INACTIVE_FILE);
		}
		pr_info("alloc stall: timeJS(ms):%u|%llu rec:%lu|%lu ret:%d o:%d gfp:%#x(%pGg) AaiFai:%lukB|%lukB|%lukB|%lukB\n",
			jiffies_to_msecs(jiffies - jiffies_s),
			stime_d / NSEC_PER_MSEC,
			did_some_progress, pages_reclaimed, retry_loop_count,
			order, gfp_mask, &gfp_mask,
			a_anon << (PAGE_SHIFT-10), in_anon << (PAGE_SHIFT-10),
			a_file << (PAGE_SHIFT-10), in_file << (PAGE_SHIFT-10));
	}
	return page;
}

static inline bool prepare_alloc_pages(gfp_t gfp_mask, unsigned int order,
		int preferred_nid, nodemask_t *nodemask,
		struct alloc_context *ac, gfp_t *alloc_mask,
		unsigned int *alloc_flags)
{
	ac->high_zoneidx = gfp_zone(gfp_mask);
	ac->zonelist = node_zonelist(preferred_nid, gfp_mask);
	ac->nodemask = nodemask;
	ac->migratetype = gfpflags_to_migratetype(gfp_mask);

	if (cpusets_enabled()) {
		*alloc_mask |= __GFP_HARDWALL;
		if (!ac->nodemask)
			ac->nodemask = &cpuset_current_mems_allowed;
		else
			*alloc_flags |= ALLOC_CPUSET;
	}

	fs_reclaim_acquire(gfp_mask);
	fs_reclaim_release(gfp_mask);

	might_sleep_if(gfp_mask & __GFP_DIRECT_RECLAIM);

	if (should_fail_alloc_page(gfp_mask, order))
		return false;

	if (IS_ENABLED(CONFIG_CMA) && ac->migratetype == MIGRATE_MOVABLE &&
			(gfp_mask & __GFP_CMA))
		*alloc_flags |= ALLOC_CMA;

	return true;
}

/* Determine whether to spread dirty pages and what the first usable zone */
static inline void finalise_ac(gfp_t gfp_mask, struct alloc_context *ac)
{
	/* Dirty zone balancing only done in the fast path */
	ac->spread_dirty_pages = (gfp_mask & __GFP_WRITE);

	/*
	 * The preferred zone is used for statistics but crucially it is
	 * also used as the starting point for the zonelist iterator. It
	 * may get reset for allocations that ignore memory policies.
	 */
	ac->preferred_zoneref = first_zones_zonelist(ac->zonelist,
					ac->high_zoneidx, ac->nodemask);
}

/*
 * This is the 'heart' of the zoned buddy allocator.
 */
struct page *
__alloc_pages_nodemask(gfp_t gfp_mask, unsigned int order, int preferred_nid,
							nodemask_t *nodemask)
{
	struct page *page;
	unsigned int alloc_flags = ALLOC_WMARK_LOW;
	gfp_t alloc_mask; /* The gfp_t that was actually used for allocation */
	struct alloc_context ac = { };

	/*
	 * There are several places where we assume that the order value is sane
	 * so bail out early if the request is out of bound.
	 */
	if (unlikely(order >= MAX_ORDER)) {
		WARN_ON_ONCE(!(gfp_mask & __GFP_NOWARN));
		return NULL;
	}

	gfp_mask &= gfp_allowed_mask;
	alloc_mask = gfp_mask;
	if (!prepare_alloc_pages(gfp_mask, order, preferred_nid, nodemask, &ac, &alloc_mask, &alloc_flags))
		return NULL;

	finalise_ac(gfp_mask, &ac);

	/*
	 * Forbid the first pass from falling back to types that fragment
	 * memory until all local zones are considered.
	 */
	alloc_flags |= alloc_flags_nofragment(ac.preferred_zoneref->zone, gfp_mask);

	/* First allocation attempt */
	page = get_page_from_freelist(alloc_mask, order, alloc_flags, &ac);
	if (likely(page))
		goto out;

	/*
	 * Apply scoped allocation constraints. This is mainly about GFP_NOFS
	 * resp. GFP_NOIO which has to be inherited for all allocation requests
	 * from a particular context which has been marked by
	 * memalloc_no{fs,io}_{save,restore}.
	 */
	alloc_mask = current_gfp_context(gfp_mask);
	ac.spread_dirty_pages = false;

	/*
	 * Restore the original nodemask if it was potentially replaced with
	 * &cpuset_current_mems_allowed to optimize the fast-path attempt.
	 */
	ac.nodemask = nodemask;

	page = __alloc_pages_slowpath(alloc_mask, order, &ac);

out:
	if (memcg_kmem_enabled() && (gfp_mask & __GFP_ACCOUNT) && page &&
	    unlikely(memcg_kmem_charge(page, gfp_mask, order) != 0)) {
		__free_pages(page, order);
		page = NULL;
	}

	trace_mm_page_alloc(page, order, alloc_mask, ac.migratetype);

	return page;
}
EXPORT_SYMBOL(__alloc_pages_nodemask);

/*
 * Common helper functions. Never use with __GFP_HIGHMEM because the returned
 * address cannot represent highmem pages. Use alloc_pages and then kmap if
 * you need to access high mem.
 */
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page;

	page = alloc_pages(gfp_mask & ~__GFP_HIGHMEM, order);
	if (!page)
		return 0;
	return (unsigned long) page_address(page);
}
EXPORT_SYMBOL(__get_free_pages);

unsigned long get_zeroed_page(gfp_t gfp_mask)
{
	return __get_free_pages(gfp_mask | __GFP_ZERO, 0);
}
EXPORT_SYMBOL(get_zeroed_page);

static inline void free_the_page(struct page *page, unsigned int order)
{
	if (order == 0)		/* Via pcp? */
		free_unref_page(page);
	else
		__free_pages_ok(page, order);
}

void __free_pages(struct page *page, unsigned int order)
{
	if (put_page_testzero(page))
		free_the_page(page, order);
}
EXPORT_SYMBOL(__free_pages);

void free_pages(unsigned long addr, unsigned int order)
{
	if (addr != 0) {
		VM_BUG_ON(!virt_addr_valid((void *)addr));
		__free_pages(virt_to_page((void *)addr), order);
	}
}

EXPORT_SYMBOL(free_pages);

/*
 * Page Fragment:
 *  An arbitrary-length arbitrary-offset area of memory which resides
 *  within a 0 or higher order page.  Multiple fragments within that page
 *  are individually refcounted, in the page's reference counter.
 *
 * The page_frag functions below provide a simple allocation framework for
 * page fragments.  This is used by the network stack and network device
 * drivers to provide a backing region of memory for use as either an
 * sk_buff->head, or to be used in the "frags" portion of skb_shared_info.
 */
static struct page *__page_frag_cache_refill(struct page_frag_cache *nc,
					     gfp_t gfp_mask)
{
	struct page *page = NULL;
	gfp_t gfp = gfp_mask;

#if (PAGE_SIZE < PAGE_FRAG_CACHE_MAX_SIZE)
	gfp_mask |= __GFP_COMP | __GFP_NOWARN | __GFP_NORETRY |
		    __GFP_NOMEMALLOC;
	page = alloc_pages_node(NUMA_NO_NODE, gfp_mask,
				PAGE_FRAG_CACHE_MAX_ORDER);
	nc->size = page ? PAGE_FRAG_CACHE_MAX_SIZE : PAGE_SIZE;
#endif
	if (unlikely(!page)) {
		gfp |= __GFP_KSWAPD_RECLAIM;
		page = alloc_pages_node(NUMA_NO_NODE, gfp, 0);
	}

	nc->va = page ? page_address(page) : NULL;

	return page;
}

void __page_frag_cache_drain(struct page *page, unsigned int count)
{
	VM_BUG_ON_PAGE(page_ref_count(page) == 0, page);

	if (page_ref_sub_and_test(page, count))
		free_the_page(page, compound_order(page));
}
EXPORT_SYMBOL(__page_frag_cache_drain);

void *page_frag_alloc(struct page_frag_cache *nc,
		      unsigned int fragsz, gfp_t gfp_mask)
{
	unsigned int size = PAGE_SIZE;
	struct page *page;
	int offset;

	if (unlikely(!nc->va)) {
refill:
		page = __page_frag_cache_refill(nc, gfp_mask);
		if (!page)
			return NULL;

#if (PAGE_SIZE < PAGE_FRAG_CACHE_MAX_SIZE)
		/* if size can vary use size else just use PAGE_SIZE */
		size = nc->size;
#endif
		/* Even if we own the page, we do not use atomic_set().
		 * This would break get_page_unless_zero() users.
		 */
		page_ref_add(page, PAGE_FRAG_CACHE_MAX_SIZE);

		/* reset page count bias and offset to start of new frag */
		nc->pfmemalloc = page_is_pfmemalloc(page);
		nc->pagecnt_bias = PAGE_FRAG_CACHE_MAX_SIZE + 1;
		nc->offset = size;
	}

	offset = nc->offset - fragsz;
	if (unlikely(offset < 0)) {
		page = virt_to_page(nc->va);

		if (!page_ref_sub_and_test(page, nc->pagecnt_bias))
			goto refill;

		if (unlikely(nc->pfmemalloc)) {
			free_the_page(page, compound_order(page));
			goto refill;
		}

#if (PAGE_SIZE < PAGE_FRAG_CACHE_MAX_SIZE)
		/* if size can vary use size else just use PAGE_SIZE */
		size = nc->size;
#endif
		/* OK, page count is 0, we can safely set it */
		set_page_count(page, PAGE_FRAG_CACHE_MAX_SIZE + 1);

		/* reset page count bias and offset to start of new frag */
		nc->pagecnt_bias = PAGE_FRAG_CACHE_MAX_SIZE + 1;
		offset = size - fragsz;
		if (unlikely(offset < 0)) {
			/*
			 * The caller is trying to allocate a fragment
			 * with fragsz > PAGE_SIZE but the cache isn't big
			 * enough to satisfy the request, this may
			 * happen in low memory conditions.
			 * We don't release the cache page because
			 * it could make memory pressure worse
			 * so we simply return NULL here.
			 */
			return NULL;
		}
	}

	nc->pagecnt_bias--;
	nc->offset = offset;

	return nc->va + offset;
}
EXPORT_SYMBOL(page_frag_alloc);

/*
 * Frees a page fragment allocated out of either a compound or order 0 page.
 */
void page_frag_free(void *addr)
{
	struct page *page = virt_to_head_page(addr);

	if (unlikely(put_page_testzero(page)))
		free_the_page(page, compound_order(page));
}
EXPORT_SYMBOL(page_frag_free);

static void *make_alloc_exact(unsigned long addr, unsigned int order,
		size_t size)
{
	if (addr) {
		unsigned long alloc_end = addr + (PAGE_SIZE << order);
		unsigned long used = addr + PAGE_ALIGN(size);

		split_page(virt_to_page((void *)addr), order);
		while (used < alloc_end) {
			free_page(used);
			used += PAGE_SIZE;
		}
	}
	return (void *)addr;
}

/**
 * alloc_pages_exact - allocate an exact number physically-contiguous pages.
 * @size: the number of bytes to allocate
 * @gfp_mask: GFP flags for the allocation
 *
 * This function is similar to alloc_pages(), except that it allocates the
 * minimum number of pages to satisfy the request.  alloc_pages() can only
 * allocate memory in power-of-two pages.
 *
 * This function is also limited by MAX_ORDER.
 *
 * Memory allocated by this function must be released by free_pages_exact().
 */
void *alloc_pages_exact(size_t size, gfp_t gfp_mask)
{
	unsigned int order = get_order(size);
	unsigned long addr;

	addr = __get_free_pages(gfp_mask, order);
	return make_alloc_exact(addr, order, size);
}
EXPORT_SYMBOL(alloc_pages_exact);

/**
 * alloc_pages_exact_nid - allocate an exact number of physically-contiguous
 *			   pages on a node.
 * @nid: the preferred node ID where memory should be allocated
 * @size: the number of bytes to allocate
 * @gfp_mask: GFP flags for the allocation
 *
 * Like alloc_pages_exact(), but try to allocate on node nid first before falling
 * back.
 */
void * __meminit alloc_pages_exact_nid(int nid, size_t size, gfp_t gfp_mask)
{
	unsigned int order = get_order(size);
	struct page *p = alloc_pages_node(nid, gfp_mask, order);
	if (!p)
		return NULL;
	return make_alloc_exact((unsigned long)page_address(p), order, size);
}

/**
 * free_pages_exact - release memory allocated via alloc_pages_exact()
 * @virt: the value returned by alloc_pages_exact.
 * @size: size of allocation, same value as passed to alloc_pages_exact().
 *
 * Release the memory allocated by a previous call to alloc_pages_exact.
 */
void free_pages_exact(void *virt, size_t size)
{
	unsigned long addr = (unsigned long)virt;
	unsigned long end = addr + PAGE_ALIGN(size);

	while (addr < end) {
		free_page(addr);
		addr += PAGE_SIZE;
	}
}
EXPORT_SYMBOL(free_pages_exact);

/**
 * nr_free_zone_pages - count number of pages beyond high watermark
 * @offset: The zone index of the highest zone
 *
 * nr_free_zone_pages() counts the number of counts pages which are beyond the
 * high watermark within all zones at or below a given zone index.  For each
 * zone, the number of pages is calculated as:
 *
 *     nr_free_zone_pages = managed_pages - high_pages
 */
static unsigned long nr_free_zone_pages(int offset)
{
	struct zoneref *z;
	struct zone *zone;

	/* Just pick one node, since fallback list is circular */
	unsigned long sum = 0;

	struct zonelist *zonelist = node_zonelist(numa_node_id(), GFP_KERNEL);

	for_each_zone_zonelist(zone, z, zonelist, offset) {
		unsigned long size = zone->managed_pages;
		unsigned long high = high_wmark_pages(zone);
		if (size > high)
			sum += size - high;
	}

	return sum;
}

/**
 * nr_free_buffer_pages - count number of pages beyond high watermark
 *
 * nr_free_buffer_pages() counts the number of pages which are beyond the high
 * watermark within ZONE_DMA and ZONE_NORMAL.
 */
unsigned long nr_free_buffer_pages(void)
{
	return nr_free_zone_pages(gfp_zone(GFP_USER));
}
EXPORT_SYMBOL_GPL(nr_free_buffer_pages);

/**
 * nr_free_pagecache_pages - count number of pages beyond high watermark
 *
 * nr_free_pagecache_pages() counts the number of pages which are beyond the
 * high watermark within all zones.
 */
unsigned long nr_free_pagecache_pages(void)
{
	return nr_free_zone_pages(gfp_zone(GFP_HIGHUSER_MOVABLE));
}

static inline void show_node(struct zone *zone)
{
	if (IS_ENABLED(CONFIG_NUMA))
		printk("Node %d ", zone_to_nid(zone));
}

long si_mem_available(void)
{
	long available;
	unsigned long pagecache;
	unsigned long wmark_low = 0;
	unsigned long pages[NR_LRU_LISTS];
	unsigned long reclaimable;
	struct zone *zone;
	int lru;

	for (lru = LRU_BASE; lru < NR_LRU_LISTS; lru++)
		pages[lru] = global_node_page_state(NR_LRU_BASE + lru);

	for_each_zone(zone)
		wmark_low += low_wmark_pages(zone);

	/*
	 * Estimate the amount of memory available for userspace allocations,
	 * without causing swapping.
	 */
	available = global_zone_page_state(NR_FREE_PAGES) - totalreserve_pages;

	/*
	 * Not all the page cache can be freed, otherwise the system will
	 * start swapping. Assume at least half of the page cache, or the
	 * low watermark worth of cache, needs to stay.
	 */
	pagecache = pages[LRU_ACTIVE_FILE] + pages[LRU_INACTIVE_FILE];
	pagecache -= min(pagecache / 2, wmark_low);
	available += pagecache;

	/*
	 * Part of the reclaimable slab and other kernel memory consists of
	 * items that are in use, and cannot be freed. Cap this estimate at the
	 * low watermark.
	 */
	reclaimable = global_node_page_state(NR_SLAB_RECLAIMABLE) +
			global_node_page_state(NR_KERNEL_MISC_RECLAIMABLE);
	available += reclaimable - min(reclaimable / 2, wmark_low);

#ifdef CONFIG_ION_RBIN_HEAP
	available += atomic_read(&rbin_cached_pages);
#endif
	if (available < 0)
		available = 0;
	return available;
}
EXPORT_SYMBOL_GPL(si_mem_available);

void si_meminfo(struct sysinfo *val)
{
	val->totalram = totalram_pages();
#ifdef CONFIG_ION_RBIN_HEAP
	val->totalram += totalrbin_pages;
#endif
	val->sharedram = global_node_page_state(NR_SHMEM);
	val->freeram = global_zone_page_state(NR_FREE_PAGES);
	val->bufferram = nr_blockdev_pages();
	val->totalhigh = totalhigh_pages();
	val->freehigh = nr_free_highpages();
	val->mem_unit = PAGE_SIZE;
}

EXPORT_SYMBOL(si_meminfo);

#ifdef CONFIG_NUMA
void si_meminfo_node(struct sysinfo *val, int nid)
{
	int zone_type;		/* needs to be signed */
	unsigned long managed_pages = 0;
	unsigned long managed_highpages = 0;
	unsigned long free_highpages = 0;
	pg_data_t *pgdat = NODE_DATA(nid);

	for (zone_type = 0; zone_type < MAX_NR_ZONES; zone_type++)
		managed_pages += pgdat->node_zones[zone_type].managed_pages;
	val->totalram = managed_pages;
	val->sharedram = node_page_state(pgdat, NR_SHMEM);
	val->freeram = sum_zone_node_page_state(nid, NR_FREE_PAGES);
#ifdef CONFIG_HIGHMEM
	for (zone_type = 0; zone_type < MAX_NR_ZONES; zone_type++) {
		struct zone *zone = &pgdat->node_zones[zone_type];

		if (is_highmem(zone)) {
			managed_highpages += zone->managed_pages;
			free_highpages += zone_page_state(zone, NR_FREE_PAGES);
		}
	}
	val->totalhigh = managed_highpages;
	val->freehigh = free_highpages;
#else
	val->totalhigh = managed_highpages;
	val->freehigh = free_highpages;
#endif
	val->mem_unit = PAGE_SIZE;
}
#endif

/*
 * Determine whether the node should be displayed or not, depending on whether
 * SHOW_MEM_FILTER_NODES was passed to show_free_areas().
 */
static bool show_mem_node_skip(unsigned int flags, int nid, nodemask_t *nodemask)
{
	if (!(flags & SHOW_MEM_FILTER_NODES))
		return false;

	/*
	 * no node mask - aka implicit memory numa policy. Do not bother with
	 * the synchronization - read_mems_allowed_begin - because we do not
	 * have to be precise here.
	 */
	if (!nodemask)
		nodemask = &cpuset_current_mems_allowed;

	return !node_isset(nid, *nodemask);
}

#define K(x) ((x) << (PAGE_SHIFT-10))

static void show_migration_types(unsigned char type)
{
	static const char types[MIGRATE_TYPES] = {
		[MIGRATE_UNMOVABLE]	= 'U',
		[MIGRATE_MOVABLE]	= 'M',
		[MIGRATE_RECLAIMABLE]	= 'E',
		[MIGRATE_HIGHATOMIC]	= 'H',
#ifdef CONFIG_CMA
		[MIGRATE_CMA]		= 'C',
#endif
#ifdef CONFIG_MEMORY_ISOLATION
		[MIGRATE_ISOLATE]	= 'I',
#endif
	};
	char tmp[MIGRATE_TYPES + 1];
	char *p = tmp;
	int i;

	for (i = 0; i < MIGRATE_TYPES; i++) {
		if (type & (1 << i))
			*p++ = types[i];
	}

	*p = '\0';
	printk(KERN_CONT "(%s) ", tmp);
}

/*
 * Show free area list (used inside shift_scroll-lock stuff)
 * We also calculate the percentage fragmentation. We do this by counting the
 * memory on each free list with the exception of the first item on the list.
 *
 * Bits in @filter:
 * SHOW_MEM_FILTER_NODES: suppress nodes that are not allowed by current's
 *   cpuset.
 */
void show_free_areas(unsigned int filter, nodemask_t *nodemask)
{
	unsigned long free_pcp = 0;
	int cpu;
	struct zone *zone;
	pg_data_t *pgdat;

	for_each_populated_zone(zone) {
		if (show_mem_node_skip(filter, zone_to_nid(zone), nodemask))
			continue;

		for_each_online_cpu(cpu)
			free_pcp += per_cpu_ptr(zone->pageset, cpu)->pcp.count;
	}

	printk("active_anon:%lu inactive_anon:%lu isolated_anon:%lu\n"
		" active_file:%lu inactive_file:%lu isolated_file:%lu\n"
		" unevictable:%lu dirty:%lu writeback:%lu unstable:%lu\n"
		" slab_reclaimable:%lu slab_unreclaimable:%lu\n"
		" mapped:%lu shmem:%lu pagetables:%lu bounce:%lu\n"
		" free:%lu free_pcp:%lu free_cma:%lu\n",
		global_node_page_state(NR_ACTIVE_ANON),
		global_node_page_state(NR_INACTIVE_ANON),
		global_node_page_state(NR_ISOLATED_ANON),
		global_node_page_state(NR_ACTIVE_FILE),
		global_node_page_state(NR_INACTIVE_FILE),
		global_node_page_state(NR_ISOLATED_FILE),
		global_node_page_state(NR_UNEVICTABLE),
		global_node_page_state(NR_FILE_DIRTY),
		global_node_page_state(NR_WRITEBACK),
		global_node_page_state(NR_UNSTABLE_NFS),
		global_node_page_state(NR_SLAB_RECLAIMABLE),
		global_node_page_state(NR_SLAB_UNRECLAIMABLE),
		global_node_page_state(NR_FILE_MAPPED),
		global_node_page_state(NR_SHMEM),
		global_zone_page_state(NR_PAGETABLE),
		global_zone_page_state(NR_BOUNCE),
		global_zone_page_state(NR_FREE_PAGES),
		free_pcp,
		global_zone_page_state(NR_FREE_CMA_PAGES));

	for_each_online_pgdat(pgdat) {
		if (show_mem_node_skip(filter, pgdat->node_id, nodemask))
			continue;

		printk("Node %d"
			" active_anon:%lukB"
			" inactive_anon:%lukB"
			" active_file:%lukB"
			" inactive_file:%lukB"
			" unevictable:%lukB"
			" isolated(anon):%lukB"
			" isolated(file):%lukB"
			" mapped:%lukB"
			" dirty:%lukB"
			" writeback:%lukB"
			" shmem:%lukB"
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
			" shmem_thp: %lukB"
			" shmem_pmdmapped: %lukB"
			" anon_thp: %lukB"
#endif
			" writeback_tmp:%lukB"
			" unstable:%lukB"
			" all_unreclaimable? %s"
			"\n",
			pgdat->node_id,
			K(node_page_state(pgdat, NR_ACTIVE_ANON)),
			K(node_page_state(pgdat, NR_INACTIVE_ANON)),
			K(node_page_state(pgdat, NR_ACTIVE_FILE)),
			K(node_page_state(pgdat, NR_INACTIVE_FILE)),
			K(node_page_state(pgdat, NR_UNEVICTABLE)),
			K(node_page_state(pgdat, NR_ISOLATED_ANON)),
			K(node_page_state(pgdat, NR_ISOLATED_FILE)),
			K(node_page_state(pgdat, NR_FILE_MAPPED)),
			K(node_page_state(pgdat, NR_FILE_DIRTY)),
			K(node_page_state(pgdat, NR_WRITEBACK)),
			K(node_page_state(pgdat, NR_SHMEM)),
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
			K(node_page_state(pgdat, NR_SHMEM_THPS) * HPAGE_PMD_NR),
			K(node_page_state(pgdat, NR_SHMEM_PMDMAPPED)
					* HPAGE_PMD_NR),
			K(node_page_state(pgdat, NR_ANON_THPS) * HPAGE_PMD_NR),
#endif
			K(node_page_state(pgdat, NR_WRITEBACK_TEMP)),
			K(node_page_state(pgdat, NR_UNSTABLE_NFS)),
			pgdat->kswapd_failures >= MAX_RECLAIM_RETRIES ?
				"yes" : "no");
	}

	for_each_populated_zone(zone) {
		int i;

		if (show_mem_node_skip(filter, zone_to_nid(zone), nodemask))
			continue;

		free_pcp = 0;
		for_each_online_cpu(cpu)
			free_pcp += per_cpu_ptr(zone->pageset, cpu)->pcp.count;

		show_node(zone);
		printk(KERN_CONT
			"%s"
			" free:%lukB"
			" min:%lukB"
			" low:%lukB"
			" high:%lukB"
			" active_anon:%lukB"
			" inactive_anon:%lukB"
			" active_file:%lukB"
			" inactive_file:%lukB"
			" unevictable:%lukB"
			" writepending:%lukB"
			" present:%lukB"
			" managed:%lukB"
			" mlocked:%lukB"
			" kernel_stack:%lukB"
#ifdef CONFIG_SHADOW_CALL_STACK
			" shadow_call_stack:%lukB"
#endif
			" pagetables:%lukB"
			" bounce:%lukB"
			" free_pcp:%lukB"
			" local_pcp:%ukB"
			" free_cma:%lukB"
			"\n",
			zone->name,
			K(zone_page_state(zone, NR_FREE_PAGES)),
			K(min_wmark_pages(zone)),
			K(low_wmark_pages(zone)),
			K(high_wmark_pages(zone)),
			K(zone_page_state(zone, NR_ZONE_ACTIVE_ANON)),
			K(zone_page_state(zone, NR_ZONE_INACTIVE_ANON)),
			K(zone_page_state(zone, NR_ZONE_ACTIVE_FILE)),
			K(zone_page_state(zone, NR_ZONE_INACTIVE_FILE)),
			K(zone_page_state(zone, NR_ZONE_UNEVICTABLE)),
			K(zone_page_state(zone, NR_ZONE_WRITE_PENDING)),
			K(zone->present_pages),
			K(zone->managed_pages),
			K(zone_page_state(zone, NR_MLOCK)),
			zone_page_state(zone, NR_KERNEL_STACK_KB),
#ifdef CONFIG_SHADOW_CALL_STACK
			zone_page_state(zone, NR_KERNEL_SCS_BYTES) / 1024,
#endif
			K(zone_page_state(zone, NR_PAGETABLE)),
			K(zone_page_state(zone, NR_BOUNCE)),
			K(free_pcp),
			K(this_cpu_read(zone->pageset->pcp.count)),
			K(zone_page_state(zone, NR_FREE_CMA_PAGES)));
		printk("lowmem_reserve[]:");
		for (i = 0; i < MAX_NR_ZONES; i++)
			printk(KERN_CONT " %ld", zone->lowmem_reserve[i]);
		printk(KERN_CONT "\n");
	}

	for_each_populated_zone(zone) {
		unsigned int order;
		unsigned long nr[MAX_ORDER], flags, total = 0;
		unsigned char types[MAX_ORDER];

		if (show_mem_node_skip(filter, zone_to_nid(zone), nodemask))
			continue;
		show_node(zone);
		printk(KERN_CONT "%s: ", zone->name);

		spin_lock_irqsave(&zone->lock, flags);
		for (order = 0; order < MAX_ORDER; order++) {
			struct free_area *area = &zone->free_area[order];
			int type;

			nr[order] = area->nr_free;
			total += nr[order] << order;

			types[order] = 0;
			for (type = 0; type < MIGRATE_TYPES; type++) {
				if (!list_empty(&area->free_list[type]))
					types[order] |= 1 << type;
			}
		}
		spin_unlock_irqrestore(&zone->lock, flags);
		for (order = 0; order < MAX_ORDER; order++) {
			printk(KERN_CONT "%lu*%lukB ",
			       nr[order], K(1UL) << order);
			if (nr[order])
				show_migration_types(types[order]);
		}
		printk(KERN_CONT "= %lukB\n", K(total));
	}

	hugetlb_show_meminfo();

	printk("%ld total pagecache pages\n", global_node_page_state(NR_FILE_PAGES));

	show_swap_cache_info();
}

static void zoneref_set_zone(struct zone *zone, struct zoneref *zoneref)
{
	zoneref->zone = zone;
	zoneref->zone_idx = zone_idx(zone);
}

/*
 * Builds allocation fallback zone lists.
 *
 * Add all populated zones of a node to the zonelist.
 */
static int build_zonerefs_node(pg_data_t *pgdat, struct zoneref *zonerefs)
{
	struct zone *zone;
	enum zone_type zone_type = MAX_NR_ZONES;
	int nr_zones = 0;

	do {
		zone_type--;
		zone = pgdat->node_zones + zone_type;
		if (populated_zone(zone)) {
			zoneref_set_zone(zone, &zonerefs[nr_zones++]);
			check_highest_zone(zone_type);
		}
	} while (zone_type);

	return nr_zones;
}

#ifdef CONFIG_NUMA

static int __parse_numa_zonelist_order(char *s)
{
	/*
	 * We used to support different zonlists modes but they turned
	 * out to be just not useful. Let's keep the warning in place
	 * if somebody still use the cmd line parameter so that we do
	 * not fail it silently
	 */
	if (!(*s == 'd' || *s == 'D' || *s == 'n' || *s == 'N')) {
		pr_warn("Ignoring unsupported numa_zonelist_order value:  %s\n", s);
		return -EINVAL;
	}
	return 0;
}

static __init int setup_numa_zonelist_order(char *s)
{
	if (!s)
		return 0;

	return __parse_numa_zonelist_order(s);
}
early_param("numa_zonelist_order", setup_numa_zonelist_order);

char numa_zonelist_order[] = "Node";

/*
 * sysctl handler for numa_zonelist_order
 */
int numa_zonelist_order_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *length,
		loff_t *ppos)
{
	char *str;
	int ret;

	if (!write)
		return proc_dostring(table, write, buffer, length, ppos);
	str = memdup_user_nul(buffer, 16);
	if (IS_ERR(str))
		return PTR_ERR(str);

	ret = __parse_numa_zonelist_order(str);
	kfree(str);
	return ret;
}


#define MAX_NODE_LOAD (nr_online_nodes)
static int node_load[MAX_NUMNODES];

/**
 * find_next_best_node - find the next node that should appear in a given node's fallback list
 * @node: node whose fallback list we're appending
 * @used_node_mask: nodemask_t of already used nodes
 *
 * We use a number of factors to determine which is the next node that should
 * appear on a given node's fallback list.  The node should not have appeared
 * already in @node's fallback list, and it should be the next closest node
 * according to the distance array (which contains arbitrary distance values
 * from each node to each node in the system), and should also prefer nodes
 * with no CPUs, since presumably they'll have very little allocation pressure
 * on them otherwise.
 * It returns -1 if no node is found.
 */
static int find_next_best_node(int node, nodemask_t *used_node_mask)
{
	int n, val;
	int min_val = INT_MAX;
	int best_node = NUMA_NO_NODE;
	const struct cpumask *tmp = cpumask_of_node(0);

	/* Use the local node if we haven't already */
	if (!node_isset(node, *used_node_mask)) {
		node_set(node, *used_node_mask);
		return node;
	}

	for_each_node_state(n, N_MEMORY) {

		/* Don't want a node to appear more than once */
		if (node_isset(n, *used_node_mask))
			continue;

		/* Use the distance array to find the distance */
		val = node_distance(node, n);

		/* Penalize nodes under us ("prefer the next node") */
		val += (n < node);

		/* Give preference to headless and unused nodes */
		tmp = cpumask_of_node(n);
		if (!cpumask_empty(tmp))
			val += PENALTY_FOR_NODE_WITH_CPUS;

		/* Slight preference for less loaded node */
		val *= (MAX_NODE_LOAD*MAX_NUMNODES);
		val += node_load[n];

		if (val < min_val) {
			min_val = val;
			best_node = n;
		}
	}

	if (best_node >= 0)
		node_set(best_node, *used_node_mask);

	return best_node;
}


/*
 * Build zonelists ordered by node and zones within node.
 * This results in maximum locality--normal zone overflows into local
 * DMA zone, if any--but risks exhausting DMA zone.
 */
static void build_zonelists_in_node_order(pg_data_t *pgdat, int *node_order,
		unsigned nr_nodes)
{
	struct zoneref *zonerefs;
	int i;

	zonerefs = pgdat->node_zonelists[ZONELIST_FALLBACK]._zonerefs;

	for (i = 0; i < nr_nodes; i++) {
		int nr_zones;

		pg_data_t *node = NODE_DATA(node_order[i]);

		nr_zones = build_zonerefs_node(node, zonerefs);
		zonerefs += nr_zones;
	}
	zonerefs->zone = NULL;
	zonerefs->zone_idx = 0;
}

/*
 * Build gfp_thisnode zonelists
 */
static void build_thisnode_zonelists(pg_data_t *pgdat)
{
	struct zoneref *zonerefs;
	int nr_zones;

	zonerefs = pgdat->node_zonelists[ZONELIST_NOFALLBACK]._zonerefs;
	nr_zones = build_zonerefs_node(pgdat, zonerefs);
	zonerefs += nr_zones;
	zonerefs->zone = NULL;
	zonerefs->zone_idx = 0;
}

/*
 * Build zonelists ordered by zone and nodes within zones.
 * This results in conserving DMA zone[s] until all Normal memory is
 * exhausted, but results in overflowing to remote node while memory
 * may still exist in local DMA zone.
 */

static void build_zonelists(pg_data_t *pgdat)
{
	static int node_order[MAX_NUMNODES];
	int node, load, nr_nodes = 0;
	nodemask_t used_mask;
	int local_node, prev_node;

	/* NUMA-aware ordering of nodes */
	local_node = pgdat->node_id;
	load = nr_online_nodes;
	prev_node = local_node;
	nodes_clear(used_mask);

	memset(node_order, 0, sizeof(node_order));
	while ((node = find_next_best_node(local_node, &used_mask)) >= 0) {
		/*
		 * We don't want to pressure a particular node.
		 * So adding penalty to the first node in same
		 * distance group to make it round-robin.
		 */
		if (node_distance(local_node, node) !=
		    node_distance(local_node, prev_node))
			node_load[node] = load;

		node_order[nr_nodes++] = node;
		prev_node = node;
		load--;
	}

	build_zonelists_in_node_order(pgdat, node_order, nr_nodes);
	build_thisnode_zonelists(pgdat);
}

#ifdef CONFIG_HAVE_MEMORYLESS_NODES
/*
 * Return node id of node used for "local" allocations.
 * I.e., first node id of first zone in arg node's generic zonelist.
 * Used for initializing percpu 'numa_mem', which is used primarily
 * for kernel allocations, so use GFP_KERNEL flags to locate zonelist.
 */
int local_memory_node(int node)
{
	struct zoneref *z;

	z = first_zones_zonelist(node_zonelist(node, GFP_KERNEL),
				   gfp_zone(GFP_KERNEL),
				   NULL);
	return zone_to_nid(z->zone);
}
#endif

static void setup_min_unmapped_ratio(void);
static void setup_min_slab_ratio(void);
#else	/* CONFIG_NUMA */

static void build_zonelists(pg_data_t *pgdat)
{
	int node, local_node;
	struct zoneref *zonerefs;
	int nr_zones;

	local_node = pgdat->node_id;

	zonerefs = pgdat->node_zonelists[ZONELIST_FALLBACK]._zonerefs;
	nr_zones = build_zonerefs_node(pgdat, zonerefs);
	zonerefs += nr_zones;

	/*
	 * Now we build the zonelist so that it contains the zones
	 * of all the other nodes.
	 * We don't want to pressure a particular node, so when
	 * building the zones for node N, we make sure that the
	 * zones coming right after the local ones are those from
	 * node N+1 (modulo N)
	 */
	for (node = local_node + 1; node < MAX_NUMNODES; node++) {
		if (!node_online(node))
			continue;
		nr_zones = build_zonerefs_node(NODE_DATA(node), zonerefs);
		zonerefs += nr_zones;
	}
	for (node = 0; node < local_node; node++) {
		if (!node_online(node))
			continue;
		nr_zones = build_zonerefs_node(NODE_DATA(node), zonerefs);
		zonerefs += nr_zones;
	}

	zonerefs->zone = NULL;
	zonerefs->zone_idx = 0;
}

#endif	/* CONFIG_NUMA */

/*
 * Boot pageset table. One per cpu which is going to be used for all
 * zones and all nodes. The parameters will be set in such a way
 * that an item put on a list will immediately be handed over to
 * the buddy list. This is safe since pageset manipulation is done
 * with interrupts disabled.
 *
 * The boot_pagesets must be kept even after bootup is complete for
 * unused processors and/or zones. They do play a role for bootstrapping
 * hotplugged processors.
 *
 * zoneinfo_show() and maybe other functions do
 * not check if the processor is online before following the pageset pointer.
 * Other parts of the kernel may not check if the zone is available.
 */
static void setup_pageset(struct per_cpu_pageset *p, unsigned long batch);
static DEFINE_PER_CPU(struct per_cpu_pageset, boot_pageset);
static DEFINE_PER_CPU(struct per_cpu_nodestat, boot_nodestats);

static void __build_all_zonelists(void *data)
{
	int nid;
	int __maybe_unused cpu;
	pg_data_t *self = data;
	unsigned long flags;

	/*
	 * Explicitly disable this CPU's interrupts before taking seqlock
	 * to prevent any IRQ handler from calling into the page allocator
	 * (e.g. GFP_ATOMIC) that could hit zonelist_iter_begin and livelock.
	 */
	local_irq_save(flags);
	/*
	 * Explicitly disable this CPU's synchronous printk() before taking
	 * seqlock to prevent any printk() from trying to hold port->lock, for
	 * tty_insert_flip_string_and_push_buffer() on other CPU might be
	 * calling kmalloc(GFP_ATOMIC | __GFP_NOWARN) with port->lock held.
	 */
	printk_deferred_enter();
	write_seqlock(&zonelist_update_seq);

#ifdef CONFIG_NUMA
	memset(node_load, 0, sizeof(node_load));
#endif

	/*
	 * This node is hotadded and no memory is yet present.   So just
	 * building zonelists is fine - no need to touch other nodes.
	 */
	if (self && !node_online(self->node_id)) {
		build_zonelists(self);
	} else {
		for_each_online_node(nid) {
			pg_data_t *pgdat = NODE_DATA(nid);

			build_zonelists(pgdat);
		}

#ifdef CONFIG_HAVE_MEMORYLESS_NODES
		/*
		 * We now know the "local memory node" for each node--
		 * i.e., the node of the first zone in the generic zonelist.
		 * Set up numa_mem percpu variable for on-line cpus.  During
		 * boot, only the boot cpu should be on-line;  we'll init the
		 * secondary cpus' numa_mem as they come on-line.  During
		 * node/memory hotplug, we'll fixup all on-line cpus.
		 */
		for_each_online_cpu(cpu)
			set_cpu_numa_mem(cpu, local_memory_node(cpu_to_node(cpu)));
#endif
	}

	write_sequnlock(&zonelist_update_seq);
	printk_deferred_exit();
	local_irq_restore(flags);
}

static noinline void __init
build_all_zonelists_init(void)
{
	int cpu;

	__build_all_zonelists(NULL);

	/*
	 * Initialize the boot_pagesets that are going to be used
	 * for bootstrapping processors. The real pagesets for
	 * each zone will be allocated later when the per cpu
	 * allocator is available.
	 *
	 * boot_pagesets are used also for bootstrapping offline
	 * cpus if the system is already booted because the pagesets
	 * are needed to initialize allocators on a specific cpu too.
	 * F.e. the percpu allocator needs the page allocator which
	 * needs the percpu allocator in order to allocate its pagesets
	 * (a chicken-egg dilemma).
	 */
	for_each_possible_cpu(cpu)
		setup_pageset(&per_cpu(boot_pageset, cpu), 0);

	mminit_verify_zonelist();
	cpuset_init_current_mems_allowed();
}

/*
 * unless system_state == SYSTEM_BOOTING.
 *
 * __ref due to call of __init annotated helper build_all_zonelists_init
 * [protected by SYSTEM_BOOTING].
 */
void __ref build_all_zonelists(pg_data_t *pgdat)
{
	if (system_state == SYSTEM_BOOTING) {
		build_all_zonelists_init();
	} else {
		__build_all_zonelists(pgdat);
		/* cpuset refresh routine should be here */
	}
	vm_total_pages = nr_free_pagecache_pages();
	/*
	 * Disable grouping by mobility if the number of pages in the
	 * system is too low to allow the mechanism to work. It would be
	 * more accurate, but expensive to check per-zone. This check is
	 * made on memory-hotadd so a system can start with mobility
	 * disabled and enable it later
	 */
	if (vm_total_pages < (pageblock_nr_pages * MIGRATE_TYPES))
		page_group_by_mobility_disabled = 1;
	else
		page_group_by_mobility_disabled = 0;

	pr_info("Built %i zonelists, mobility grouping %s.  Total pages: %ld\n",
		nr_online_nodes,
		page_group_by_mobility_disabled ? "off" : "on",
		vm_total_pages);
#ifdef CONFIG_NUMA
	pr_info("Policy zone: %s\n", zone_names[policy_zone]);
#endif
}

/*
 * Initially all pages are reserved - free ones are freed
 * up by free_all_bootmem() once the early boot process is
 * done. Non-atomic initialization, single-pass.
 */
void __meminit memmap_init_zone(unsigned long size, int nid, unsigned long zone,
		unsigned long start_pfn, enum meminit_context context,
		struct vmem_altmap *altmap)
{
	unsigned long end_pfn = start_pfn + size;
	pg_data_t *pgdat = NODE_DATA(nid);
	unsigned long pfn;
	unsigned long nr_initialised = 0;
	struct page *page;
#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
	struct memblock_region *r = NULL, *tmp;
#endif

	if (highest_memmap_pfn < end_pfn - 1)
		highest_memmap_pfn = end_pfn - 1;

	/*
	 * Honor reservation requested by the driver for this ZONE_DEVICE
	 * memory
	 */
	if (altmap && start_pfn == altmap->base_pfn)
		start_pfn += altmap->reserve;

	for (pfn = start_pfn; pfn < end_pfn; pfn++) {
		/*
		 * There can be holes in boot-time mem_map[]s handed to this
		 * function.  They do not exist on hotplugged memory.
		 */
		if (context != MEMINIT_EARLY)
			goto not_early;

		if (!early_pfn_valid(pfn))
			continue;
		if (!early_pfn_in_nid(pfn, nid))
			continue;
		if (!update_defer_init(pgdat, pfn, end_pfn, &nr_initialised))
			break;

#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
		/*
		 * Check given memblock attribute by firmware which can affect
		 * kernel memory layout.  If zone==ZONE_MOVABLE but memory is
		 * mirrored, it's an overlapped memmap init. skip it.
		 */
		if (mirrored_kernelcore && zone == ZONE_MOVABLE) {
			if (!r || pfn >= memblock_region_memory_end_pfn(r)) {
				for_each_memblock(memory, tmp)
					if (pfn < memblock_region_memory_end_pfn(tmp))
						break;
				r = tmp;
			}
			if (pfn >= memblock_region_memory_base_pfn(r) &&
			    memblock_is_mirror(r)) {
				/* already initialized as NORMAL */
				pfn = memblock_region_memory_end_pfn(r);
				continue;
			}
		}
#endif

not_early:
		page = pfn_to_page(pfn);
		__init_single_page(page, pfn, zone, nid);
		if (context == MEMINIT_HOTPLUG)
			SetPageReserved(page);

		/*
		 * Mark the block movable so that blocks are reserved for
		 * movable at startup. This will force kernel allocations
		 * to reserve their blocks rather than leaking throughout
		 * the address space during boot when many long-lived
		 * kernel allocations are made.
		 *
		 * bitmap is created for zone's valid pfn range. but memmap
		 * can be created for invalid pages (for alignment)
		 * check here not to call set_pageblock_migratetype() against
		 * pfn out of zone.
		 *
		 * Please note that MEMINIT_HOTPLUG path doesn't clear memmap
		 * because this is done early in sparse_add_one_section
		 */
		if (!(pfn & (pageblock_nr_pages - 1))) {
			set_pageblock_migratetype(page, MIGRATE_MOVABLE);
			cond_resched();
		}
	}
}

static void __meminit zone_init_free_lists(struct zone *zone)
{
	unsigned int order, t;
	for_each_migratetype_order(order, t) {
		INIT_LIST_HEAD(&zone->free_area[order].free_list[t]);
		zone->free_area[order].nr_free = 0;
	}
}

#ifndef __HAVE_ARCH_MEMMAP_INIT
#define memmap_init(size, nid, zone, start_pfn) \
	memmap_init_zone((size), (nid), (zone), (start_pfn), \
			 MEMINIT_EARLY, NULL)
#endif

static int zone_batchsize(struct zone *zone)
{
#ifdef CONFIG_MMU
	int batch;

	/*
	 * The per-cpu-pages pools are set to around 1000th of the
	 * size of the zone.
	 */
	batch = zone->managed_pages / 1024;
	/* But no more than a meg. */
	if (batch * PAGE_SIZE > 1024 * 1024)
		batch = (1024 * 1024) / PAGE_SIZE;
	batch /= 4;		/* We effectively *= 4 below */
	if (batch < 1)
		batch = 1;

	/*
	 * Clamp the batch to a 2^n - 1 value. Having a power
	 * of 2 value was found to be more likely to have
	 * suboptimal cache aliasing properties in some cases.
	 *
	 * For example if 2 tasks are alternately allocating
	 * batches of pages, one task can end up with a lot
	 * of pages of one half of the possible page colors
	 * and the other with pages of the other colors.
	 */
	batch = rounddown_pow_of_two(batch + batch/2) - 1;

	return batch;

#else
	/* The deferral and batching of frees should be suppressed under NOMMU
	 * conditions.
	 *
	 * The problem is that NOMMU needs to be able to allocate large chunks
	 * of contiguous memory as there's no hardware page translation to
	 * assemble apparent contiguous memory from discontiguous pages.
	 *
	 * Queueing large contiguous runs of pages for batching, however,
	 * causes the pages to actually be freed in smaller chunks.  As there
	 * can be a significant delay between the individual batches being
	 * recycled, this leads to the once large chunks of space being
	 * fragmented and becoming unavailable for high-order allocations.
	 */
	return 0;
#endif
}

/*
 * pcp->high and pcp->batch values are related and dependent on one another:
 * ->batch must never be higher then ->high.
 * The following function updates them in a safe manner without read side
 * locking.
 *
 * Any new users of pcp->batch and pcp->high should ensure they can cope with
 * those fields changing asynchronously (acording the the above rule).
 *
 * mutex_is_locked(&pcp_batch_high_lock) required when calling this function
 * outside of boot time (or some other assurance that no concurrent updaters
 * exist).
 */
static void pageset_update(struct per_cpu_pages *pcp, unsigned long high,
		unsigned long batch)
{
       /* start with a fail safe value for batch */
	pcp->batch = 1;
	smp_wmb();

       /* Update high, then batch, in order */
	pcp->high = high;
	smp_wmb();

	pcp->batch = batch;
}

/* a companion to pageset_set_high() */
static void pageset_set_batch(struct per_cpu_pageset *p, unsigned long batch)
{
	pageset_update(&p->pcp, 6 * batch, max(1UL, 1 * batch));
}

static void pageset_init(struct per_cpu_pageset *p)
{
	struct per_cpu_pages *pcp;
	int migratetype;

	memset(p, 0, sizeof(*p));

	pcp = &p->pcp;
	for (migratetype = 0; migratetype < MIGRATE_PCPTYPES; migratetype++)
		INIT_LIST_HEAD(&pcp->lists[migratetype]);
}

static void setup_pageset(struct per_cpu_pageset *p, unsigned long batch)
{
	pageset_init(p);
	pageset_set_batch(p, batch);
}

/*
 * pageset_set_high() sets the high water mark for hot per_cpu_pagelist
 * to the value high for the pageset p.
 */
static void pageset_set_high(struct per_cpu_pageset *p,
				unsigned long high)
{
	unsigned long batch = max(1UL, high / 4);
	if ((high / 4) > (PAGE_SHIFT * 8))
		batch = PAGE_SHIFT * 8;

	pageset_update(&p->pcp, high, batch);
}

static void pageset_set_high_and_batch(struct zone *zone,
				       struct per_cpu_pageset *pcp)
{
	if (percpu_pagelist_fraction)
		pageset_set_high(pcp,
			(zone->managed_pages /
				percpu_pagelist_fraction));
	else
		pageset_set_batch(pcp, zone_batchsize(zone));
}

static void __meminit zone_pageset_init(struct zone *zone, int cpu)
{
	struct per_cpu_pageset *pcp = per_cpu_ptr(zone->pageset, cpu);

	pageset_init(pcp);
	pageset_set_high_and_batch(zone, pcp);
}

void __meminit setup_zone_pageset(struct zone *zone)
{
	int cpu;
	zone->pageset = alloc_percpu(struct per_cpu_pageset);
	for_each_possible_cpu(cpu)
		zone_pageset_init(zone, cpu);
}

/*
 * Allocate per cpu pagesets and initialize them.
 * Before this call only boot pagesets were available.
 */
void __init setup_per_cpu_pageset(void)
{
	struct pglist_data *pgdat;
	struct zone *zone;

	for_each_populated_zone(zone)
		setup_zone_pageset(zone);

	for_each_online_pgdat(pgdat)
		pgdat->per_cpu_nodestats =
			alloc_percpu(struct per_cpu_nodestat);
}

static __meminit void zone_pcp_init(struct zone *zone)
{
	/*
	 * per cpu subsystem is not up at this point. The following code
	 * relies on the ability of the linker to provide the
	 * offset of a (static) per cpu variable into the per cpu area.
	 */
	zone->pageset = &boot_pageset;

	if (populated_zone(zone))
		printk(KERN_DEBUG "  %s zone: %lu pages, LIFO batch:%u\n",
			zone->name, zone->present_pages,
					 zone_batchsize(zone));
}

void __meminit init_currently_empty_zone(struct zone *zone,
					unsigned long zone_start_pfn,
					unsigned long size)
{
	struct pglist_data *pgdat = zone->zone_pgdat;
	int zone_idx = zone_idx(zone) + 1;

	if (zone_idx > pgdat->nr_zones)
		pgdat->nr_zones = zone_idx;

	zone->zone_start_pfn = zone_start_pfn;

	mminit_dprintk(MMINIT_TRACE, "memmap_init",
			"Initialising map node %d zone %lu pfns %lu -> %lu\n",
			pgdat->node_id,
			(unsigned long)zone_idx(zone),
			zone_start_pfn, (zone_start_pfn + size));

	zone_init_free_lists(zone);
	zone->initialized = 1;
}

#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
#ifndef CONFIG_HAVE_ARCH_EARLY_PFN_TO_NID

/*
 * Required by SPARSEMEM. Given a PFN, return what node the PFN is on.
 */
int __meminit __early_pfn_to_nid(unsigned long pfn,
					struct mminit_pfnnid_cache *state)
{
	unsigned long start_pfn, end_pfn;
	int nid;

	if (state->last_start <= pfn && pfn < state->last_end)
		return state->last_nid;

	nid = memblock_search_pfn_nid(pfn, &start_pfn, &end_pfn);
	if (nid != -1) {
		state->last_start = start_pfn;
		state->last_end = end_pfn;
		state->last_nid = nid;
	}

	return nid;
}
#endif /* CONFIG_HAVE_ARCH_EARLY_PFN_TO_NID */

/**
 * free_bootmem_with_active_regions - Call memblock_free_early_nid for each active range
 * @nid: The node to free memory on. If MAX_NUMNODES, all nodes are freed.
 * @max_low_pfn: The highest PFN that will be passed to memblock_free_early_nid
 *
 * If an architecture guarantees that all ranges registered contain no holes
 * and may be freed, this this function may be used instead of calling
 * memblock_free_early_nid() manually.
 */
void __init free_bootmem_with_active_regions(int nid, unsigned long max_low_pfn)
{
	unsigned long start_pfn, end_pfn;
	int i, this_nid;

	for_each_mem_pfn_range(i, nid, &start_pfn, &end_pfn, &this_nid) {
		start_pfn = min(start_pfn, max_low_pfn);
		end_pfn = min(end_pfn, max_low_pfn);

		if (start_pfn < end_pfn)
			memblock_free_early_nid(PFN_PHYS(start_pfn),
					(end_pfn - start_pfn) << PAGE_SHIFT,
					this_nid);
	}
}

/**
 * sparse_memory_present_with_active_regions - Call memory_present for each active range
 * @nid: The node to call memory_present for. If MAX_NUMNODES, all nodes will be used.
 *
 * If an architecture guarantees that all ranges registered contain no holes and may
 * be freed, this function may be used instead of calling memory_present() manually.
 */
void __init sparse_memory_present_with_active_regions(int nid)
{
	unsigned long start_pfn, end_pfn;
	int i, this_nid;

	for_each_mem_pfn_range(i, nid, &start_pfn, &end_pfn, &this_nid)
		memory_present(this_nid, start_pfn, end_pfn);
}

/**
 * get_pfn_range_for_nid - Return the start and end page frames for a node
 * @nid: The nid to return the range for. If MAX_NUMNODES, the min and max PFN are returned.
 * @start_pfn: Passed by reference. On return, it will have the node start_pfn.
 * @end_pfn: Passed by reference. On return, it will have the node end_pfn.
 *
 * It returns the start and end page frame of a node based on information
 * provided by memblock_set_node(). If called for a node
 * with no available memory, a warning is printed and the start and end
 * PFNs will be 0.
 */
void __meminit get_pfn_range_for_nid(unsigned int nid,
			unsigned long *start_pfn, unsigned long *end_pfn)
{
	unsigned long this_start_pfn, this_end_pfn;
	int i;

	*start_pfn = -1UL;
	*end_pfn = 0;

	for_each_mem_pfn_range(i, nid, &this_start_pfn, &this_end_pfn, NULL) {
		*start_pfn = min(*start_pfn, this_start_pfn);
		*end_pfn = max(*end_pfn, this_end_pfn);
	}

	if (*start_pfn == -1UL)
		*start_pfn = 0;
}

/*
 * This finds a zone that can be used for ZONE_MOVABLE pages. The
 * assumption is made that zones within a node are ordered in monotonic
 * increasing memory addresses so that the "highest" populated zone is used
 */
static void __init find_usable_zone_for_movable(void)
{
	int zone_index;
	for (zone_index = MAX_NR_ZONES - 1; zone_index >= 0; zone_index--) {
		if (zone_index == ZONE_MOVABLE)
			continue;

		if (arch_zone_highest_possible_pfn[zone_index] >
				arch_zone_lowest_possible_pfn[zone_index])
			break;
	}

	VM_BUG_ON(zone_index == -1);
	movable_zone = zone_index;
}

/*
 * The zone ranges provided by the architecture do not include ZONE_MOVABLE
 * because it is sized independent of architecture. Unlike the other zones,
 * the starting point for ZONE_MOVABLE is not fixed. It may be different
 * in each node depending on the size of each node and how evenly kernelcore
 * is distributed. This helper function adjusts the zone ranges
 * provided by the architecture for a given node by using the end of the
 * highest usable zone for ZONE_MOVABLE. This preserves the assumption that
 * zones within a node are in order of monotonic increases memory addresses
 */
static void __meminit adjust_zone_range_for_zone_movable(int nid,
					unsigned long zone_type,
					unsigned long node_start_pfn,
					unsigned long node_end_pfn,
					unsigned long *zone_start_pfn,
					unsigned long *zone_end_pfn)
{
	/* Only adjust if ZONE_MOVABLE is on this node */
	if (zone_movable_pfn[nid]) {
		/* Size ZONE_MOVABLE */
		if (zone_type == ZONE_MOVABLE) {
			*zone_start_pfn = zone_movable_pfn[nid];
			*zone_end_pfn = min(node_end_pfn,
				arch_zone_highest_possible_pfn[movable_zone]);

		/* Adjust for ZONE_MOVABLE starting within this range */
		} else if (!mirrored_kernelcore &&
			*zone_start_pfn < zone_movable_pfn[nid] &&
			*zone_end_pfn > zone_movable_pfn[nid]) {
			*zone_end_pfn = zone_movable_pfn[nid];

		/* Check if this whole range is within ZONE_MOVABLE */
		} else if (*zone_start_pfn >= zone_movable_pfn[nid])
			*zone_start_pfn = *zone_end_pfn;
	}
}

/*
 * Return the number of pages a zone spans in a node, including holes
 * present_pages = zone_spanned_pages_in_node() - zone_absent_pages_in_node()
 */
static unsigned long __meminit zone_spanned_pages_in_node(int nid,
					unsigned long zone_type,
					unsigned long node_start_pfn,
					unsigned long node_end_pfn,
					unsigned long *zone_start_pfn,
					unsigned long *zone_end_pfn,
					unsigned long *ignored)
{
	unsigned long zone_low = arch_zone_lowest_possible_pfn[zone_type];
	unsigned long zone_high = arch_zone_highest_possible_pfn[zone_type];
	/* When hotadd a new node from cpu_up(), the node should be empty */
	if (!node_start_pfn && !node_end_pfn)
		return 0;

	/* Get the start and end of the zone */
	*zone_start_pfn = clamp(node_start_pfn, zone_low, zone_high);
	*zone_end_pfn = clamp(node_end_pfn, zone_low, zone_high);
	adjust_zone_range_for_zone_movable(nid, zone_type,
				node_start_pfn, node_end_pfn,
				zone_start_pfn, zone_end_pfn);

	/* Check that this node has pages within the zone's required range */
	if (*zone_end_pfn < node_start_pfn || *zone_start_pfn > node_end_pfn)
		return 0;

	/* Move the zone boundaries inside the node if necessary */
	*zone_end_pfn = min(*zone_end_pfn, node_end_pfn);
	*zone_start_pfn = max(*zone_start_pfn, node_start_pfn);

	/* Return the spanned pages */
	return *zone_end_pfn - *zone_start_pfn;
}

/*
 * Return the number of holes in a range on a node. If nid is MAX_NUMNODES,
 * then all holes in the requested range will be accounted for.
 */
unsigned long __meminit __absent_pages_in_range(int nid,
				unsigned long range_start_pfn,
				unsigned long range_end_pfn)
{
	unsigned long nr_absent = range_end_pfn - range_start_pfn;
	unsigned long start_pfn, end_pfn;
	int i;

	for_each_mem_pfn_range(i, nid, &start_pfn, &end_pfn, NULL) {
		start_pfn = clamp(start_pfn, range_start_pfn, range_end_pfn);
		end_pfn = clamp(end_pfn, range_start_pfn, range_end_pfn);
		nr_absent -= end_pfn - start_pfn;
	}
	return nr_absent;
}

/**
 * absent_pages_in_range - Return number of page frames in holes within a range
 * @start_pfn: The start PFN to start searching for holes
 * @end_pfn: The end PFN to stop searching for holes
 *
 * It returns the number of pages frames in memory holes within a range.
 */
unsigned long __init absent_pages_in_range(unsigned long start_pfn,
							unsigned long end_pfn)
{
	return __absent_pages_in_range(MAX_NUMNODES, start_pfn, end_pfn);
}

/* Return the number of page frames in holes in a zone on a node */
static unsigned long __meminit zone_absent_pages_in_node(int nid,
					unsigned long zone_type,
					unsigned long node_start_pfn,
					unsigned long node_end_pfn,
					unsigned long *ignored)
{
	unsigned long zone_low = arch_zone_lowest_possible_pfn[zone_type];
	unsigned long zone_high = arch_zone_highest_possible_pfn[zone_type];
	unsigned long zone_start_pfn, zone_end_pfn;
	unsigned long nr_absent;

	/* When hotadd a new node from cpu_up(), the node should be empty */
	if (!node_start_pfn && !node_end_pfn)
		return 0;

	zone_start_pfn = clamp(node_start_pfn, zone_low, zone_high);
	zone_end_pfn = clamp(node_end_pfn, zone_low, zone_high);

	adjust_zone_range_for_zone_movable(nid, zone_type,
			node_start_pfn, node_end_pfn,
			&zone_start_pfn, &zone_end_pfn);
	nr_absent = __absent_pages_in_range(nid, zone_start_pfn, zone_end_pfn);

	/*
	 * ZONE_MOVABLE handling.
	 * Treat pages to be ZONE_MOVABLE in ZONE_NORMAL as absent pages
	 * and vice versa.
	 */
	if (mirrored_kernelcore && zone_movable_pfn[nid]) {
		unsigned long start_pfn, end_pfn;
		struct memblock_region *r;

		for_each_memblock(memory, r) {
			start_pfn = clamp(memblock_region_memory_base_pfn(r),
					  zone_start_pfn, zone_end_pfn);
			end_pfn = clamp(memblock_region_memory_end_pfn(r),
					zone_start_pfn, zone_end_pfn);

			if (zone_type == ZONE_MOVABLE &&
			    memblock_is_mirror(r))
				nr_absent += end_pfn - start_pfn;

			if (zone_type == ZONE_NORMAL &&
			    !memblock_is_mirror(r))
				nr_absent += end_pfn - start_pfn;
		}
	}

	return nr_absent;
}

#else /* CONFIG_HAVE_MEMBLOCK_NODE_MAP */
static inline unsigned long __meminit zone_spanned_pages_in_node(int nid,
					unsigned long zone_type,
					unsigned long node_start_pfn,
					unsigned long node_end_pfn,
					unsigned long *zone_start_pfn,
					unsigned long *zone_end_pfn,
					unsigned long *zones_size)
{
	unsigned int zone;

	*zone_start_pfn = node_start_pfn;
	for (zone = 0; zone < zone_type; zone++)
		*zone_start_pfn += zones_size[zone];

	*zone_end_pfn = *zone_start_pfn + zones_size[zone_type];

	return zones_size[zone_type];
}

static inline unsigned long __meminit zone_absent_pages_in_node(int nid,
						unsigned long zone_type,
						unsigned long node_start_pfn,
						unsigned long node_end_pfn,
						unsigned long *zholes_size)
{
	if (!zholes_size)
		return 0;

	return zholes_size[zone_type];
}

#endif /* CONFIG_HAVE_MEMBLOCK_NODE_MAP */

static void __meminit calculate_node_totalpages(struct pglist_data *pgdat,
						unsigned long node_start_pfn,
						unsigned long node_end_pfn,
						unsigned long *zones_size,
						unsigned long *zholes_size)
{
	unsigned long realtotalpages = 0, totalpages = 0;
	enum zone_type i;

	for (i = 0; i < MAX_NR_ZONES; i++) {
		struct zone *zone = pgdat->node_zones + i;
		unsigned long zone_start_pfn, zone_end_pfn;
		unsigned long size, real_size;

		size = zone_spanned_pages_in_node(pgdat->node_id, i,
						  node_start_pfn,
						  node_end_pfn,
						  &zone_start_pfn,
						  &zone_end_pfn,
						  zones_size);
		real_size = size - zone_absent_pages_in_node(pgdat->node_id, i,
						  node_start_pfn, node_end_pfn,
						  zholes_size);
		if (size)
			zone->zone_start_pfn = zone_start_pfn;
		else
			zone->zone_start_pfn = 0;
		zone->spanned_pages = size;
		zone->present_pages = real_size;

		totalpages += size;
		realtotalpages += real_size;
	}

	pgdat->node_spanned_pages = totalpages;
	pgdat->node_present_pages = realtotalpages;
	printk(KERN_DEBUG "On node %d totalpages: %lu\n", pgdat->node_id,
							realtotalpages);
}

#ifndef CONFIG_SPARSEMEM
/*
 * Calculate the size of the zone->blockflags rounded to an unsigned long
 * Start by making sure zonesize is a multiple of pageblock_order by rounding
 * up. Then use 1 NR_PAGEBLOCK_BITS worth of bits per pageblock, finally
 * round what is now in bits to nearest long in bits, then return it in
 * bytes.
 */
static unsigned long __init usemap_size(unsigned long zone_start_pfn, unsigned long zonesize)
{
	unsigned long usemapsize;

	zonesize += zone_start_pfn & (pageblock_nr_pages-1);
	usemapsize = roundup(zonesize, pageblock_nr_pages);
	usemapsize = usemapsize >> pageblock_order;
	usemapsize *= NR_PAGEBLOCK_BITS;
	usemapsize = roundup(usemapsize, 8 * sizeof(unsigned long));

	return usemapsize / 8;
}

static void __ref setup_usemap(struct pglist_data *pgdat,
				struct zone *zone,
				unsigned long zone_start_pfn,
				unsigned long zonesize)
{
	unsigned long usemapsize = usemap_size(zone_start_pfn, zonesize);
	zone->pageblock_flags = NULL;
	if (usemapsize)
		zone->pageblock_flags =
			memblock_virt_alloc_node_nopanic(usemapsize,
							 pgdat->node_id);
}
#else
static inline void setup_usemap(struct pglist_data *pgdat, struct zone *zone,
				unsigned long zone_start_pfn, unsigned long zonesize) {}
#endif /* CONFIG_SPARSEMEM */

#ifdef CONFIG_HUGETLB_PAGE_SIZE_VARIABLE

/* Initialise the number of pages represented by NR_PAGEBLOCK_BITS */
void __init set_pageblock_order(void)
{
	unsigned int order;

	/* Check that pageblock_nr_pages has not already been setup */
	if (pageblock_order)
		return;

	if (HPAGE_SHIFT > PAGE_SHIFT)
		order = HUGETLB_PAGE_ORDER;
	else
		order = MAX_ORDER - 1;

	/*
	 * Assume the largest contiguous order of interest is a huge page.
	 * This value may be variable depending on boot parameters on IA64 and
	 * powerpc.
	 */
	pageblock_order = order;
}
#else /* CONFIG_HUGETLB_PAGE_SIZE_VARIABLE */

/*
 * When CONFIG_HUGETLB_PAGE_SIZE_VARIABLE is not set, set_pageblock_order()
 * is unused as pageblock_order is set at compile-time. See
 * include/linux/pageblock-flags.h for the values of pageblock_order based on
 * the kernel config
 */
void __init set_pageblock_order(void)
{
}

#endif /* CONFIG_HUGETLB_PAGE_SIZE_VARIABLE */

static unsigned long __init calc_memmap_size(unsigned long spanned_pages,
						unsigned long present_pages)
{
	unsigned long pages = spanned_pages;

	/*
	 * Provide a more accurate estimation if there are holes within
	 * the zone and SPARSEMEM is in use. If there are holes within the
	 * zone, each populated memory region may cost us one or two extra
	 * memmap pages due to alignment because memmap pages for each
	 * populated regions may not be naturally aligned on page boundary.
	 * So the (present_pages >> 4) heuristic is a tradeoff for that.
	 */
	if (spanned_pages > present_pages + (present_pages >> 4) &&
	    IS_ENABLED(CONFIG_SPARSEMEM))
		pages = present_pages;

	return PAGE_ALIGN(pages * sizeof(struct page)) >> PAGE_SHIFT;
}

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
static void pgdat_init_split_queue(struct pglist_data *pgdat)
{
	spin_lock_init(&pgdat->split_queue_lock);
	INIT_LIST_HEAD(&pgdat->split_queue);
	pgdat->split_queue_len = 0;
}
#else
static void pgdat_init_split_queue(struct pglist_data *pgdat) {}
#endif

#ifdef CONFIG_COMPACTION
static void pgdat_init_kcompactd(struct pglist_data *pgdat)
{
	init_waitqueue_head(&pgdat->kcompactd_wait);
}
#else
static void pgdat_init_kcompactd(struct pglist_data *pgdat) {}
#endif

static void __meminit pgdat_init_internals(struct pglist_data *pgdat)
{
	pgdat_resize_init(pgdat);

	pgdat_init_split_queue(pgdat);
	pgdat_init_kcompactd(pgdat);

	init_waitqueue_head(&pgdat->kswapd_wait);
	init_waitqueue_head(&pgdat->pfmemalloc_wait);

	pgdat_page_ext_init(pgdat);
	spin_lock_init(&pgdat->lru_lock);
	lruvec_init(node_lruvec(pgdat));
}

static void __meminit zone_init_internals(struct zone *zone, enum zone_type idx, int nid,
							unsigned long remaining_pages)
{
	zone->managed_pages = remaining_pages;
	zone_set_nid(zone, nid);
	zone->name = zone_names[idx];
	zone->zone_pgdat = NODE_DATA(nid);
	spin_lock_init(&zone->lock);
	zone_seqlock_init(zone);
	zone_pcp_init(zone);
}

/*
 * Set up the zone data structures
 * - init pgdat internals
 * - init all zones belonging to this node
 *
 * NOTE: this function is only called during memory hotplug
 */
#ifdef CONFIG_MEMORY_HOTPLUG
void __ref free_area_init_core_hotplug(int nid)
{
	enum zone_type z;
	pg_data_t *pgdat = NODE_DATA(nid);

	pgdat_init_internals(pgdat);
	for (z = 0; z < MAX_NR_ZONES; z++)
		zone_init_internals(&pgdat->node_zones[z], z, nid, 0);
}
#endif

/*
 * Set up the zone data structures:
 *   - mark all pages reserved
 *   - mark all memory queues empty
 *   - clear the memory bitmaps
 *
 * NOTE: pgdat should get zeroed by caller.
 * NOTE: this function is only called during early init.
 */
static void __init free_area_init_core(struct pglist_data *pgdat)
{
	enum zone_type j;
	int nid = pgdat->node_id;

	pgdat_init_internals(pgdat);
	pgdat->per_cpu_nodestats = &boot_nodestats;

	for (j = 0; j < MAX_NR_ZONES; j++) {
		struct zone *zone = pgdat->node_zones + j;
		unsigned long size, freesize, memmap_pages;
		unsigned long zone_start_pfn = zone->zone_start_pfn;

		size = zone->spanned_pages;
		freesize = zone->present_pages;

		/*
		 * Adjust freesize so that it accounts for how much memory
		 * is used by this zone for memmap. This affects the watermark
		 * and per-cpu initialisations
		 */
		memmap_pages = calc_memmap_size(size, freesize);
		if (!is_highmem_idx(j)) {
			if (freesize >= memmap_pages) {
				freesize -= memmap_pages;
				if (memmap_pages)
					printk(KERN_DEBUG
					       "  %s zone: %lu pages used for memmap\n",
					       zone_names[j], memmap_pages);
			} else
				pr_warn("  %s zone: %lu pages exceeds freesize %lu\n",
					zone_names[j], memmap_pages, freesize);
		}

		/* Account for reserved pages */
		if (j == 0 && freesize > dma_reserve) {
			freesize -= dma_reserve;
			printk(KERN_DEBUG "  %s zone: %lu pages reserved\n",
					zone_names[0], dma_reserve);
		}

		if (!is_highmem_idx(j))
			nr_kernel_pages += freesize;
		/* Charge for highmem memmap if there are enough kernel pages */
		else if (nr_kernel_pages > memmap_pages * 2)
			nr_kernel_pages -= memmap_pages;
		nr_all_pages += freesize;

		/*
		 * Set an approximate value for lowmem here, it will be adjusted
		 * when the bootmem allocator frees pages into the buddy system.
		 * And all highmem pages will be managed by the buddy system.
		 */
		zone_init_internals(zone, j, nid, freesize);

		if (!size)
			continue;

		set_pageblock_order();
		setup_usemap(pgdat, zone, zone_start_pfn, size);
		init_currently_empty_zone(zone, zone_start_pfn, size);
		memmap_init(size, nid, j, zone_start_pfn);
	}
}

#ifdef CONFIG_FLAT_NODE_MEM_MAP
static void __ref alloc_node_mem_map(struct pglist_data *pgdat)
{
	unsigned long __maybe_unused start = 0;
	unsigned long __maybe_unused offset = 0;

	/* Skip empty nodes */
	if (!pgdat->node_spanned_pages)
		return;

	start = pgdat->node_start_pfn & ~(MAX_ORDER_NR_PAGES - 1);
	offset = pgdat->node_start_pfn - start;
	/* ia64 gets its own node_mem_map, before this, without bootmem */
	if (!pgdat->node_mem_map) {
		unsigned long size, end;
		struct page *map;

		/*
		 * The zone's endpoints aren't required to be MAX_ORDER
		 * aligned but the node_mem_map endpoints must be in order
		 * for the buddy allocator to function correctly.
		 */
		end = pgdat_end_pfn(pgdat);
		end = ALIGN(end, MAX_ORDER_NR_PAGES);
		size =  (end - start) * sizeof(struct page);
		map = memblock_virt_alloc_node_nopanic(size, pgdat->node_id);
		pgdat->node_mem_map = map + offset;
	}
	pr_debug("%s: node %d, pgdat %08lx, node_mem_map %08lx\n",
				__func__, pgdat->node_id, (unsigned long)pgdat,
				(unsigned long)pgdat->node_mem_map);
#ifndef CONFIG_NEED_MULTIPLE_NODES
	/*
	 * With no DISCONTIG, the global mem_map is just set as node 0's
	 */
	if (pgdat == NODE_DATA(0)) {
		mem_map = NODE_DATA(0)->node_mem_map;
#if defined(CONFIG_HAVE_MEMBLOCK_NODE_MAP) || defined(CONFIG_FLATMEM)
		if (page_to_pfn(mem_map) != pgdat->node_start_pfn)
			mem_map -= offset;
#endif /* CONFIG_HAVE_MEMBLOCK_NODE_MAP */
	}
#endif
}
#else
static void __ref alloc_node_mem_map(struct pglist_data *pgdat) { }
#endif /* CONFIG_FLAT_NODE_MEM_MAP */

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
static inline void pgdat_set_deferred_range(pg_data_t *pgdat)
{
	/*
	 * We start only with one section of pages, more pages are added as
	 * needed until the rest of deferred pages are initialized.
	 */
	pgdat->static_init_pgcnt = min_t(unsigned long, PAGES_PER_SECTION,
						pgdat->node_spanned_pages);
	pgdat->first_deferred_pfn = ULONG_MAX;
}
#else
static inline void pgdat_set_deferred_range(pg_data_t *pgdat) {}
#endif

void __init free_area_init_node(int nid, unsigned long *zones_size,
				   unsigned long node_start_pfn,
				   unsigned long *zholes_size)
{
	pg_data_t *pgdat = NODE_DATA(nid);
	unsigned long start_pfn = 0;
	unsigned long end_pfn = 0;

	/* pg_data_t should be reset to zero when it's allocated */
	WARN_ON(pgdat->nr_zones || pgdat->kswapd_classzone_idx);

	pgdat->node_id = nid;
	pgdat->node_start_pfn = node_start_pfn;
	pgdat->per_cpu_nodestats = NULL;
#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP
	get_pfn_range_for_nid(nid, &start_pfn, &end_pfn);
	pr_info("Initmem setup node %d [mem %#018Lx-%#018Lx]\n", nid,
		(u64)start_pfn << PAGE_SHIFT,
		end_pfn ? ((u64)end_pfn << PAGE_SHIFT) - 1 : 0);
#else
	start_pfn = node_start_pfn;
#endif
	calculate_node_totalpages(pgdat, start_pfn, end_pfn,
				  zones_size, zholes_size);

	alloc_node_mem_map(pgdat);
	pgdat_set_deferred_range(pgdat);

	free_area_init_core(pgdat);
}

#if defined(CONFIG_HAVE_MEMBLOCK) && !defined(CONFIG_FLAT_NODE_MEM_MAP)

/*
 * Zero all valid struct pages in range [spfn, epfn), return number of struct
 * pages zeroed
 */
static u64 zero_pfn_range(unsigned long spfn, unsigned long epfn)
{
	unsigned long pfn;
	u64 pgcnt = 0;

	for (pfn = spfn; pfn < epfn; pfn++) {
		if (!pfn_valid(ALIGN_DOWN(pfn, pageblock_nr_pages))) {
			pfn = ALIGN_DOWN(pfn, pageblock_nr_pages)
				+ pageblock_nr_pages - 1;
			continue;
		}
		mm_zero_struct_page(pfn_to_page(pfn));
		pgcnt++;
	}

	return pgcnt;
}

/*
 * Only struct pages that are backed by physical memory are zeroed and
 * initialized by going through __init_single_page(). But, there are some
 * struct pages which are reserved in memblock allocator and their fields
 * may be accessed (for example page_to_pfn() on some configuration accesses
 * flags). We must explicitly zero those struct pages.
 *
 * This function also addresses a similar issue where struct pages are left
 * uninitialized because the physical address range is not covered by
 * memblock.memory or memblock.reserved. That could happen when memblock
 * layout is manually configured via memmap=, or when the highest physical
 * address (max_pfn) does not end on a section boundary.
 */
void __init zero_resv_unavail(void)
{
	phys_addr_t start, end;
	u64 i, pgcnt;
	phys_addr_t next = 0;

	/*
	 * Loop through unavailable ranges not covered by memblock.memory.
	 */
	pgcnt = 0;
	for_each_mem_range(i, &memblock.memory, NULL,
			NUMA_NO_NODE, MEMBLOCK_NONE, &start, &end, NULL) {
		if (next < start)
			pgcnt += zero_pfn_range(PFN_DOWN(next), PFN_UP(start));
		next = end;
	}

	/*
	 * Early sections always have a fully populated memmap for the whole
	 * section - see pfn_valid(). If the last section has holes at the
	 * end and that section is marked "online", the memmap will be
	 * considered initialized. Make sure that memmap has a well defined
	 * state.
	 */
	pgcnt += zero_pfn_range(PFN_DOWN(next),
				round_up(max_pfn, PAGES_PER_SECTION));

	/*
	 * Struct pages that do not have backing memory. This could be because
	 * firmware is using some of this memory, or for some other reasons.
	 */
	if (pgcnt)
		pr_info("Zeroed struct page in unavailable ranges: %lld pages", pgcnt);
}
#endif /* CONFIG_HAVE_MEMBLOCK && !CONFIG_FLAT_NODE_MEM_MAP */

#ifdef CONFIG_HAVE_MEMBLOCK_NODE_MAP

#if MAX_NUMNODES > 1
/*
 * Figure out the number of possible node ids.
 */
void __init setup_nr_node_ids(void)
{
	unsigned int highest;

	highest = find_last_bit(node_possible_map.bits, MAX_NUMNODES);
	nr_node_ids = highest + 1;
}
#endif

/**
 * node_map_pfn_alignment - determine the maximum internode alignment
 *
 * This function should be called after node map is populated and sorted.
 * It calculates the maximum power of two alignment which can distinguish
 * all the nodes.
 *
 * For example, if all nodes are 1GiB and aligned to 1GiB, the return value
 * would indicate 1GiB alignment with (1 << (30 - PAGE_SHIFT)).  If the
 * nodes are shifted by 256MiB, 256MiB.  Note that if only the last node is
 * shifted, 1GiB is enough and this function will indicate so.
 *
 * This is used to test whether pfn -> nid mapping of the chosen memory
 * model has fine enough granularity to avoid incorrect mapping for the
 * populated node map.
 *
 * Returns the determined alignment in pfn's.  0 if there is no alignment
 * requirement (single node).
 */
unsigned long __init node_map_pfn_alignment(void)
{
	unsigned long accl_mask = 0, last_end = 0;
	unsigned long start, end, mask;
	int last_nid = -1;
	int i, nid;

	for_each_mem_pfn_range(i, MAX_NUMNODES, &start, &end, &nid) {
		if (!start || last_nid < 0 || last_nid == nid) {
			last_nid = nid;
			last_end = end;
			continue;
		}

		/*
		 * Start with a mask granular enough to pin-point to the
		 * start pfn and tick off bits one-by-one until it becomes
		 * too coarse to separate the current node from the last.
		 */
		mask = ~((1 << __ffs(start)) - 1);
		while (mask && last_end <= (start & (mask << 1)))
			mask <<= 1;

		/* accumulate all internode masks */
		accl_mask |= mask;
	}

	/* convert mask to number of pages */
	return ~accl_mask + 1;
}

/* Find the lowest pfn for a node */
static unsigned long __init find_min_pfn_for_node(int nid)
{
	unsigned long min_pfn = ULONG_MAX;
	unsigned long start_pfn;
	int i;

	for_each_mem_pfn_range(i, nid, &start_pfn, NULL, NULL)
		min_pfn = min(min_pfn, start_pfn);

	if (min_pfn == ULONG_MAX) {
		pr_warn("Could not find start_pfn for node %d\n", nid);
		return 0;
	}

	return min_pfn;
}

/**
 * find_min_pfn_with_active_regions - Find the minimum PFN registered
 *
 * It returns the minimum PFN based on information provided via
 * memblock_set_node().
 */
unsigned long __init find_min_pfn_with_active_regions(void)
{
	return find_min_pfn_for_node(MAX_NUMNODES);
}

/*
 * early_calculate_totalpages()
 * Sum pages in active regions for movable zone.
 * Populate N_MEMORY for calculating usable_nodes.
 */
static unsigned long __init early_calculate_totalpages(void)
{
	unsigned long totalpages = 0;
	unsigned long start_pfn, end_pfn;
	int i, nid;

	for_each_mem_pfn_range(i, MAX_NUMNODES, &start_pfn, &end_pfn, &nid) {
		unsigned long pages = end_pfn - start_pfn;

		totalpages += pages;
		if (pages)
			node_set_state(nid, N_MEMORY);
	}
	return totalpages;
}

/*
 * Find the PFN the Movable zone begins in each node. Kernel memory
 * is spread evenly between nodes as long as the nodes have enough
 * memory. When they don't, some nodes will have more kernelcore than
 * others
 */
static void __init find_zone_movable_pfns_for_nodes(void)
{
	int i, nid;
	unsigned long usable_startpfn;
	unsigned long kernelcore_node, kernelcore_remaining;
	/* save the state before borrow the nodemask */
	nodemask_t saved_node_state = node_states[N_MEMORY];
	unsigned long totalpages = early_calculate_totalpages();
	int usable_nodes = nodes_weight(node_states[N_MEMORY]);
	struct memblock_region *r;

	/* Need to find movable_zone earlier when movable_node is specified. */
	find_usable_zone_for_movable();

	/*
	 * If movable_node is specified, ignore kernelcore and movablecore
	 * options.
	 */
	if (movable_node_is_enabled()) {
		for_each_memblock(memory, r) {
			if (!memblock_is_hotpluggable(r))
				continue;

			nid = r->nid;

			usable_startpfn = PFN_DOWN(r->base);
			zone_movable_pfn[nid] = zone_movable_pfn[nid] ?
				min(usable_startpfn, zone_movable_pfn[nid]) :
				usable_startpfn;
		}

		goto out2;
	}

	/*
	 * If kernelcore=mirror is specified, ignore movablecore option
	 */
	if (mirrored_kernelcore) {
		bool mem_below_4gb_not_mirrored = false;

		for_each_memblock(memory, r) {
			if (memblock_is_mirror(r))
				continue;

			nid = r->nid;

			usable_startpfn = memblock_region_memory_base_pfn(r);

			if (usable_startpfn < 0x100000) {
				mem_below_4gb_not_mirrored = true;
				continue;
			}

			zone_movable_pfn[nid] = zone_movable_pfn[nid] ?
				min(usable_startpfn, zone_movable_pfn[nid]) :
				usable_startpfn;
		}

		if (mem_below_4gb_not_mirrored)
			pr_warn("This configuration results in unmirrored kernel memory.");

		goto out2;
	}

	/*
	 * If kernelcore=nn% or movablecore=nn% was specified, calculate the
	 * amount of necessary memory.
	 */
	if (required_kernelcore_percent)
		required_kernelcore = (totalpages * 100 * required_kernelcore_percent) /
				       10000UL;
	if (required_movablecore_percent)
		required_movablecore = (totalpages * 100 * required_movablecore_percent) /
					10000UL;

	/*
	 * If movablecore= was specified, calculate what size of
	 * kernelcore that corresponds so that memory usable for
	 * any allocation type is evenly spread. If both kernelcore
	 * and movablecore are specified, then the value of kernelcore
	 * will be used for required_kernelcore if it's greater than
	 * what movablecore would have allowed.
	 */
	if (required_movablecore) {
		unsigned long corepages;

		/*
		 * Round-up so that ZONE_MOVABLE is at least as large as what
		 * was requested by the user
		 */
		required_movablecore =
			roundup(required_movablecore, MAX_ORDER_NR_PAGES);
		required_movablecore = min(totalpages, required_movablecore);
		corepages = totalpages - required_movablecore;

		required_kernelcore = max(required_kernelcore, corepages);
	}

	/*
	 * If kernelcore was not specified or kernelcore size is larger
	 * than totalpages, there is no ZONE_MOVABLE.
	 */
	if (!required_kernelcore || required_kernelcore >= totalpages)
		goto out;

	/* usable_startpfn is the lowest possible pfn ZONE_MOVABLE can be at */
	usable_startpfn = arch_zone_lowest_possible_pfn[movable_zone];

restart:
	/* Spread kernelcore memory as evenly as possible throughout nodes */
	kernelcore_node = required_kernelcore / usable_nodes;
	for_each_node_state(nid, N_MEMORY) {
		unsigned long start_pfn, end_pfn;

		/*
		 * Recalculate kernelcore_node if the division per node
		 * now exceeds what is necessary to satisfy the requested
		 * amount of memory for the kernel
		 */
		if (required_kernelcore < kernelcore_node)
			kernelcore_node = required_kernelcore / usable_nodes;

		/*
		 * As the map is walked, we track how much memory is usable
		 * by the kernel using kernelcore_remaining. When it is
		 * 0, the rest of the node is usable by ZONE_MOVABLE
		 */
		kernelcore_remaining = kernelcore_node;

		/* Go through each range of PFNs within this node */
		for_each_mem_pfn_range(i, nid, &start_pfn, &end_pfn, NULL) {
			unsigned long size_pages;

			start_pfn = max(start_pfn, zone_movable_pfn[nid]);
			if (start_pfn >= end_pfn)
				continue;

			/* Account for what is only usable for kernelcore */
			if (start_pfn < usable_startpfn) {
				unsigned long kernel_pages;
				kernel_pages = min(end_pfn, usable_startpfn)
								- start_pfn;

				kernelcore_remaining -= min(kernel_pages,
							kernelcore_remaining);
				required_kernelcore -= min(kernel_pages,
							required_kernelcore);

				/* Continue if range is now fully accounted */
				if (end_pfn <= usable_startpfn) {

					/*
					 * Push zone_movable_pfn to the end so
					 * that if we have to rebalance
					 * kernelcore across nodes, we will
					 * not double account here
					 */
					zone_movable_pfn[nid] = end_pfn;
					continue;
				}
				start_pfn = usable_startpfn;
			}

			/*
			 * The usable PFN range for ZONE_MOVABLE is from
			 * start_pfn->end_pfn. Calculate size_pages as the
			 * number of pages used as kernelcore
			 */
			size_pages = end_pfn - start_pfn;
			if (size_pages > kernelcore_remaining)
				size_pages = kernelcore_remaining;
			zone_movable_pfn[nid] = start_pfn + size_pages;

			/*
			 * Some kernelcore has been met, update counts and
			 * break if the kernelcore for this node has been
			 * satisfied
			 */
			required_kernelcore -= min(required_kernelcore,
								size_pages);
			kernelcore_remaining -= size_pages;
			if (!kernelcore_remaining)
				break;
		}
	}

	/*
	 * If there is still required_kernelcore, we do another pass with one
	 * less node in the count. This will push zone_movable_pfn[nid] further
	 * along on the nodes that still have memory until kernelcore is
	 * satisfied
	 */
	usable_nodes--;
	if (usable_nodes && required_kernelcore > usable_nodes)
		goto restart;

out2:
	/* Align start of ZONE_MOVABLE on all nids to MAX_ORDER_NR_PAGES */
	for (nid = 0; nid < MAX_NUMNODES; nid++) {
		unsigned long start_pfn, end_pfn;

		zone_movable_pfn[nid] =
			roundup(zone_movable_pfn[nid], MAX_ORDER_NR_PAGES);

		get_pfn_range_for_nid(nid, &start_pfn, &end_pfn);
		if (zone_movable_pfn[nid] >= end_pfn)
			zone_movable_pfn[nid] = 0;
	}

out:
	/* restore the node_state */
	node_states[N_MEMORY] = saved_node_state;
}

/* Any regular or high memory on that node ? */
static void check_for_memory(pg_data_t *pgdat, int nid)
{
	enum zone_type zone_type;

	if (N_MEMORY == N_NORMAL_MEMORY)
		return;

	for (zone_type = 0; zone_type <= ZONE_MOVABLE - 1; zone_type++) {
		struct zone *zone = &pgdat->node_zones[zone_type];
		if (populated_zone(zone)) {
			node_set_state(nid, N_HIGH_MEMORY);
			if (N_NORMAL_MEMORY != N_HIGH_MEMORY &&
			    zone_type <= ZONE_NORMAL)
				node_set_state(nid, N_NORMAL_MEMORY);
			break;
		}
	}
}

/**
 * free_area_init_nodes - Initialise all pg_data_t and zone data
 * @max_zone_pfn: an array of max PFNs for each zone
 *
 * This will call free_area_init_node() for each active node in the system.
 * Using the page ranges provided by memblock_set_node(), the size of each
 * zone in each node and their holes is calculated. If the maximum PFN
 * between two adjacent zones match, it is assumed that the zone is empty.
 * For example, if arch_max_dma_pfn == arch_max_dma32_pfn, it is assumed
 * that arch_max_dma32_pfn has no pages. It is also assumed that a zone
 * starts where the previous one ended. For example, ZONE_DMA32 starts
 * at arch_max_dma_pfn.
 */
void __init free_area_init_nodes(unsigned long *max_zone_pfn)
{
	unsigned long start_pfn, end_pfn;
	int i, nid;

	/* Record where the zone boundaries are */
	memset(arch_zone_lowest_possible_pfn, 0,
				sizeof(arch_zone_lowest_possible_pfn));
	memset(arch_zone_highest_possible_pfn, 0,
				sizeof(arch_zone_highest_possible_pfn));

	start_pfn = find_min_pfn_with_active_regions();

	for (i = 0; i < MAX_NR_ZONES; i++) {
		if (i == ZONE_MOVABLE)
			continue;

		end_pfn = max(max_zone_pfn[i], start_pfn);
		arch_zone_lowest_possible_pfn[i] = start_pfn;
		arch_zone_highest_possible_pfn[i] = end_pfn;

		start_pfn = end_pfn;
	}

	/* Find the PFNs that ZONE_MOVABLE begins at in each node */
	memset(zone_movable_pfn, 0, sizeof(zone_movable_pfn));
	find_zone_movable_pfns_for_nodes();

	/* Print out the zone ranges */
	pr_info("Zone ranges:\n");
	for (i = 0; i < MAX_NR_ZONES; i++) {
		if (i == ZONE_MOVABLE)
			continue;
		pr_info("  %-8s ", zone_names[i]);
		if (arch_zone_lowest_possible_pfn[i] ==
				arch_zone_highest_possible_pfn[i])
			pr_cont("empty\n");
		else
			pr_cont("[mem %#018Lx-%#018Lx]\n",
				(u64)arch_zone_lowest_possible_pfn[i]
					<< PAGE_SHIFT,
				((u64)arch_zone_highest_possible_pfn[i]
					<< PAGE_SHIFT) - 1);
	}

	/* Print out the PFNs ZONE_MOVABLE begins at in each node */
	pr_info("Movable zone start for each node\n");
	for (i = 0; i < MAX_NUMNODES; i++) {
		if (zone_movable_pfn[i])
			pr_info("  Node %d: %#018Lx\n", i,
			       (u64)zone_movable_pfn[i] << PAGE_SHIFT);
	}

	/* Print out the early node map */
	pr_info("Early memory node ranges\n");
	for_each_mem_pfn_range(i, MAX_NUMNODES, &start_pfn, &end_pfn, &nid)
		pr_info("  node %3d: [mem %#018Lx-%#018Lx]\n", nid,
			(u64)start_pfn << PAGE_SHIFT,
			((u64)end_pfn << PAGE_SHIFT) - 1);

	/* Initialise every node */
	mminit_verify_pageflags_layout();
	setup_nr_node_ids();
	zero_resv_unavail();
	for_each_online_node(nid) {
		pg_data_t *pgdat = NODE_DATA(nid);
		free_area_init_node(nid, NULL,
				find_min_pfn_for_node(nid), NULL);

		/* Any memory on that node */
		if (pgdat->node_present_pages)
			node_set_state(nid, N_MEMORY);
		check_for_memory(pgdat, nid);
	}
}

static int __init cmdline_parse_core(char *p, unsigned long *core,
				     unsigned long *percent)
{
	unsigned long long coremem;
	char *endptr;

	if (!p)
		return -EINVAL;

	/* Value may be a percentage of total memory, otherwise bytes */
	coremem = simple_strtoull(p, &endptr, 0);
	if (*endptr == '%') {
		/* Paranoid check for percent values greater than 100 */
		WARN_ON(coremem > 100);

		*percent = coremem;
	} else {
		coremem = memparse(p, &p);
		/* Paranoid check that UL is enough for the coremem value */
		WARN_ON((coremem >> PAGE_SHIFT) > ULONG_MAX);

		*core = coremem >> PAGE_SHIFT;
		*percent = 0UL;
	}
	return 0;
}

/*
 * kernelcore=size sets the amount of memory for use for allocations that
 * cannot be reclaimed or migrated.
 */
static int __init cmdline_parse_kernelcore(char *p)
{
	/* parse kernelcore=mirror */
	if (parse_option_str(p, "mirror")) {
		mirrored_kernelcore = true;
		return 0;
	}

	return cmdline_parse_core(p, &required_kernelcore,
				  &required_kernelcore_percent);
}

/*
 * movablecore=size sets the amount of memory for use for allocations that
 * can be reclaimed or migrated.
 */
static int __init cmdline_parse_movablecore(char *p)
{
	return cmdline_parse_core(p, &required_movablecore,
				  &required_movablecore_percent);
}

early_param("kernelcore", cmdline_parse_kernelcore);
early_param("movablecore", cmdline_parse_movablecore);

#endif /* CONFIG_HAVE_MEMBLOCK_NODE_MAP */

void adjust_managed_page_count(struct page *page, long count)
{
	spin_lock(&managed_page_count_lock);
	page_zone(page)->managed_pages += count;
	totalram_pages_add(count);
#ifdef CONFIG_HIGHMEM
	if (PageHighMem(page))
		totalram_pages_add(count);
#endif
	spin_unlock(&managed_page_count_lock);
}
EXPORT_SYMBOL(adjust_managed_page_count);

unsigned long free_reserved_area(void *start, void *end, int poison, char *s)
{
	void *pos;
	unsigned long pages = 0;

	free_memsize_reserved(__pa(start), end - start);
	start = (void *)PAGE_ALIGN((unsigned long)start);
	end = (void *)((unsigned long)end & PAGE_MASK);
	for (pos = start; pos < end; pos += PAGE_SIZE, pages++) {
		struct page *page = virt_to_page(pos);
		void *direct_map_addr;

		/*
		 * 'direct_map_addr' might be different from 'pos'
		 * because some architectures' virt_to_page()
		 * work with aliases.  Getting the direct map
		 * address ensures that we get a _writeable_
		 * alias for the memset().
		 */
		direct_map_addr = page_address(page);
		if ((unsigned int)poison <= 0xFF)
			memset(direct_map_addr, poison, PAGE_SIZE);

		free_reserved_page(page);
	}

	if (pages && s)
		pr_info("Freeing %s memory: %ldK\n",
			s, pages << (PAGE_SHIFT - 10));

#ifdef CONFIG_HAVE_MEMBLOCK
		memblock_dbg("memblock_free: [%#016llx-%#016llx] %pS\n",
			(u64)__pa(start), (u64)__pa(end), (void *)_RET_IP_);
#endif

	return pages;
}
EXPORT_SYMBOL(free_reserved_area);

#ifdef	CONFIG_HIGHMEM
void free_highmem_page(struct page *page)
{
	__free_reserved_page(page);
	totalram_pages++;
	page_zone(page)->managed_pages++;
	totalhigh_pages++;
}
#endif


void __init mem_init_print_info(const char *str)
{
	unsigned long physpages, codesize, datasize, rosize, bss_size;
	unsigned long init_code_size, init_data_size;

	physpages = get_num_physpages();
	codesize = _etext - _stext;
	datasize = _edata - _sdata;
	rosize = __end_rodata - __start_rodata;
	bss_size = __bss_stop - __bss_start;
	init_data_size = __init_end - __init_begin;
	init_code_size = _einittext - _sinittext;

	/*
	 * Detect special cases and adjust section sizes accordingly:
	 * 1) .init.* may be embedded into .data sections
	 * 2) .init.text.* may be out of [__init_begin, __init_end],
	 *    please refer to arch/tile/kernel/vmlinux.lds.S.
	 * 3) .rodata.* may be embedded into .text or .data sections.
	 */
#define adj_init_size(start, end, size, pos, adj) \
	do { \
		if (&start[0] <= &pos[0] && &pos[0] < &end[0] && size > adj) \
			size -= adj; \
	} while (0)

	adj_init_size(__init_begin, __init_end, init_data_size,
		     _sinittext, init_code_size);
	adj_init_size(_stext, _etext, codesize, _sinittext, init_code_size);
	adj_init_size(_sdata, _edata, datasize, __init_begin, init_data_size);
	adj_init_size(_stext, _etext, codesize, __start_rodata, rosize);
	adj_init_size(_sdata, _edata, datasize, __start_rodata, rosize);

#undef	adj_init_size

	pr_info("Memory: %luK/%luK available (%luK kernel code, %luK rwdata, %luK rodata, %luK init, %luK bss, %luK reserved, %luK cma-reserved"
#ifdef	CONFIG_HIGHMEM
		", %luK highmem"
#endif
		"%s%s)\n",
		nr_free_pages() << (PAGE_SHIFT - 10),
		physpages << (PAGE_SHIFT - 10),
		codesize >> 10, datasize >> 10, rosize >> 10,
		(init_data_size + init_code_size) >> 10, bss_size >> 10,
		(physpages - totalram_pages() - totalcma_pages) << (PAGE_SHIFT - 10),
		totalcma_pages << (PAGE_SHIFT - 10),
#ifdef	CONFIG_HIGHMEM
		totalhigh_pages() << (PAGE_SHIFT - 10),
#endif
		str ? ", " : "", str ? str : "");
}

/**
 * set_dma_reserve - set the specified number of pages reserved in the first zone
 * @new_dma_reserve: The number of pages to mark reserved
 *
 * The per-cpu batchsize and zone watermarks are determined by managed_pages.
 * In the DMA zone, a significant percentage may be consumed by kernel image
 * and other unfreeable allocations which can skew the watermarks badly. This
 * function may optionally be used to account for unfreeable pages in the
 * first zone (e.g., ZONE_DMA). The effect will be lower watermarks and
 * smaller per-cpu batchsize.
 */
void __init set_dma_reserve(unsigned long new_dma_reserve)
{
	dma_reserve = new_dma_reserve;
}

void __init free_area_init(unsigned long *zones_size)
{
	zero_resv_unavail();
	free_area_init_node(0, zones_size,
			__pa(PAGE_OFFSET) >> PAGE_SHIFT, NULL);
}

static int page_alloc_cpu_dead(unsigned int cpu)
{

	lru_add_drain_cpu(cpu);
	drain_pages(cpu);

	/*
	 * Spill the event counters of the dead processor
	 * into the current processors event counters.
	 * This artificially elevates the count of the current
	 * processor.
	 */
	vm_events_fold_cpu(cpu);

	/*
	 * Zero the differential counters of the dead processor
	 * so that the vm statistics are consistent.
	 *
	 * This is only okay since the processor is dead and cannot
	 * race with what we are doing.
	 */
	cpu_vm_stats_fold(cpu);
	return 0;
}

void __init page_alloc_init(void)
{
	int ret;

	ret = cpuhp_setup_state_nocalls(CPUHP_PAGE_ALLOC_DEAD,
					"mm/page_alloc:dead", NULL,
					page_alloc_cpu_dead);
	WARN_ON(ret < 0);
}

/*
 * calculate_totalreserve_pages - called when sysctl_lowmem_reserve_ratio
 *	or min_free_kbytes changes.
 */
static void calculate_totalreserve_pages(void)
{
	struct pglist_data *pgdat;
	unsigned long reserve_pages = 0;
	enum zone_type i, j;

	for_each_online_pgdat(pgdat) {

		pgdat->totalreserve_pages = 0;

		for (i = 0; i < MAX_NR_ZONES; i++) {
			struct zone *zone = pgdat->node_zones + i;
			long max = 0;

			/* Find valid and maximum lowmem_reserve in the zone */
			for (j = i; j < MAX_NR_ZONES; j++) {
				if (zone->lowmem_reserve[j] > max)
					max = zone->lowmem_reserve[j];
			}

			/* we treat the high watermark as reserved pages. */
			max += high_wmark_pages(zone);

			if (max > zone->managed_pages)
				max = zone->managed_pages;

			pgdat->totalreserve_pages += max;

			reserve_pages += max;
		}
	}
	totalreserve_pages = reserve_pages;
}

/*
 * setup_per_zone_lowmem_reserve - called whenever
 *	sysctl_lowmem_reserve_ratio changes.  Ensures that each zone
 *	has a correct pages reserved value, so an adequate number of
 *	pages are left in the zone after a successful __alloc_pages().
 */
static void setup_per_zone_lowmem_reserve(void)
{
	struct pglist_data *pgdat;
	enum zone_type j, idx;

	for_each_online_pgdat(pgdat) {
		for (j = 0; j < MAX_NR_ZONES; j++) {
			struct zone *zone = pgdat->node_zones + j;
			unsigned long managed_pages = zone->managed_pages;

			zone->lowmem_reserve[j] = 0;

			idx = j;
			while (idx) {
				struct zone *lower_zone;

				idx--;
				lower_zone = pgdat->node_zones + idx;

				if (sysctl_lowmem_reserve_ratio[idx] < 1) {
					sysctl_lowmem_reserve_ratio[idx] = 0;
					lower_zone->lowmem_reserve[j] = 0;
				} else {
					lower_zone->lowmem_reserve[j] =
						managed_pages / sysctl_lowmem_reserve_ratio[idx];
				}
				managed_pages += lower_zone->managed_pages;
			}
		}
	}

	/* update totalreserve_pages */
	calculate_totalreserve_pages();
}

static void __setup_per_zone_wmarks(void)
{
	unsigned long pages_min = min_free_kbytes >> (PAGE_SHIFT - 10);
	unsigned long pages_low = extra_free_kbytes >> (PAGE_SHIFT - 10);
	unsigned long lowmem_pages = 0;
	struct zone *zone;
	unsigned long flags;

	/* Calculate total number of !ZONE_HIGHMEM pages */
	for_each_zone(zone) {
		if (!is_highmem(zone))
			lowmem_pages += zone->managed_pages;
	}

	for_each_zone(zone) {
		u64 min, low;

		spin_lock_irqsave(&zone->lock, flags);
		min = (u64)pages_min * zone->managed_pages;
		do_div(min, lowmem_pages);
		low = (u64)pages_low * zone->managed_pages;
		do_div(low, vm_total_pages);

		if (is_highmem(zone)) {
			/*
			 * __GFP_HIGH and PF_MEMALLOC allocations usually don't
			 * need highmem pages, so cap pages_min to a small
			 * value here.
			 *
			 * The WMARK_HIGH-WMARK_LOW and (WMARK_LOW-WMARK_MIN)
			 * deltas control async page reclaim, and so should
			 * not be capped for highmem.
			 */
			unsigned long min_pages;

			min_pages = zone->managed_pages / 1024;
			min_pages = clamp(min_pages, SWAP_CLUSTER_MAX, 128UL);
			zone->_watermark[WMARK_MIN] = min_pages;
		} else {
			/*
			 * If it's a lowmem zone, reserve a number of pages
			 * proportionate to the zone's size.
			 */
			zone->_watermark[WMARK_MIN] = min;
		}

		/*
		 * Set the kswapd watermarks distance according to the
		 * scale factor in proportion to available memory, but
		 * ensure a minimum size on small systems.
		 */
		min = max_t(u64, min >> 2,
			    mult_frac(zone->managed_pages,
				      watermark_scale_factor, 10000));

		zone->watermark_boost = 0;
		zone->_watermark[WMARK_LOW]  = min_wmark_pages(zone) +
					low + min;
		zone->_watermark[WMARK_HIGH] = min_wmark_pages(zone) +
					low + min * 2;

		spin_unlock_irqrestore(&zone->lock, flags);
	}

	/* update totalreserve_pages */
	calculate_totalreserve_pages();
}

/**
 * setup_per_zone_wmarks - called when min_free_kbytes changes
 * or when memory is hot-{added|removed}
 *
 * Ensures that the watermark[min,low,high] values for each zone are set
 * correctly with respect to min_free_kbytes.
 */
void setup_per_zone_wmarks(void)
{
	static DEFINE_SPINLOCK(lock);

	spin_lock(&lock);
	__setup_per_zone_wmarks();
	spin_unlock(&lock);
}

/*
 * Initialise min_free_kbytes.
 *
 * For small machines we want it small (128k min).  For large machines
 * we want it large (64MB max).  But it is not linear, because network
 * bandwidth does not increase linearly with machine size.  We use
 *
 *	min_free_kbytes = 4 * sqrt(lowmem_kbytes), for better accuracy:
 *	min_free_kbytes = sqrt(lowmem_kbytes * 16)
 *
 * which yields
 *
 * 16MB:	512k
 * 32MB:	724k
 * 64MB:	1024k
 * 128MB:	1448k
 * 256MB:	2048k
 * 512MB:	2896k
 * 1024MB:	4096k
 * 2048MB:	5792k
 * 4096MB:	8192k
 * 8192MB:	11584k
 * 16384MB:	16384k
 */
int __meminit init_per_zone_wmark_min(void)
{
	unsigned long lowmem_kbytes;
	int new_min_free_kbytes;

	lowmem_kbytes = nr_free_buffer_pages() * (PAGE_SIZE >> 10);
	new_min_free_kbytes = int_sqrt(lowmem_kbytes * 16);

	if (new_min_free_kbytes > user_min_free_kbytes) {
		min_free_kbytes = new_min_free_kbytes;
		if (min_free_kbytes < 32768)
			min_free_kbytes = 32768;
		if (min_free_kbytes > 65536)
			min_free_kbytes = 65536;
	} else {
		pr_warn("min_free_kbytes is not updated to %d because user defined value %d is preferred\n",
				new_min_free_kbytes, user_min_free_kbytes);
	}
	setup_per_zone_wmarks();
	refresh_zone_stat_thresholds();
	setup_per_zone_lowmem_reserve();

#ifdef CONFIG_NUMA
	setup_min_unmapped_ratio();
	setup_min_slab_ratio();
#endif

	khugepaged_min_free_kbytes_update();

	return 0;
}
postcore_initcall(init_per_zone_wmark_min)

/*
 * min_free_kbytes_sysctl_handler - just a wrapper around proc_dointvec() so
 *	that we can call two helper functions whenever min_free_kbytes
 *	or extra_free_kbytes changes.
 */
int min_free_kbytes_sysctl_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *length, loff_t *ppos)
{
	int rc;

	rc = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (rc)
		return rc;

	if (write) {
		user_min_free_kbytes = min_free_kbytes;
		setup_per_zone_wmarks();
	}
	return 0;
}

int watermark_boost_factor_sysctl_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *length, loff_t *ppos)
{
	int rc;

	rc = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (rc)
		return rc;

	return 0;
}

int kswapd_threads_sysctl_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *length, loff_t *ppos)
{
	int rc;

	rc = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (rc)
		return rc;

	if (write)
		update_kswapd_threads();

	return 0;
}

int watermark_scale_factor_sysctl_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *length, loff_t *ppos)
{
	int rc;

	rc = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (rc)
		return rc;

	if (write)
		setup_per_zone_wmarks();

	return 0;
}

#ifdef CONFIG_NUMA
static void setup_min_unmapped_ratio(void)
{
	pg_data_t *pgdat;
	struct zone *zone;

	for_each_online_pgdat(pgdat)
		pgdat->min_unmapped_pages = 0;

	for_each_zone(zone)
		zone->zone_pgdat->min_unmapped_pages += (zone->managed_pages *
				sysctl_min_unmapped_ratio) / 100;
}


int sysctl_min_unmapped_ratio_sysctl_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *length, loff_t *ppos)
{
	int rc;

	rc = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (rc)
		return rc;

	setup_min_unmapped_ratio();

	return 0;
}

static void setup_min_slab_ratio(void)
{
	pg_data_t *pgdat;
	struct zone *zone;

	for_each_online_pgdat(pgdat)
		pgdat->min_slab_pages = 0;

	for_each_zone(zone)
		zone->zone_pgdat->min_slab_pages += (zone->managed_pages *
				sysctl_min_slab_ratio) / 100;
}

int sysctl_min_slab_ratio_sysctl_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *length, loff_t *ppos)
{
	int rc;

	rc = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (rc)
		return rc;

	setup_min_slab_ratio();

	return 0;
}
#endif

/*
 * lowmem_reserve_ratio_sysctl_handler - just a wrapper around
 *	proc_dointvec() so that we can call setup_per_zone_lowmem_reserve()
 *	whenever sysctl_lowmem_reserve_ratio changes.
 *
 * The reserve ratio obviously has absolutely no relation with the
 * minimum watermarks. The lowmem reserve ratio can only make sense
 * if in function of the boot time zone sizes.
 */
int lowmem_reserve_ratio_sysctl_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *length, loff_t *ppos)
{
	proc_dointvec_minmax(table, write, buffer, length, ppos);
	setup_per_zone_lowmem_reserve();
	return 0;
}

/*
 * percpu_pagelist_fraction - changes the pcp->high for each zone on each
 * cpu.  It is the fraction of total pages in each zone that a hot per cpu
 * pagelist can have before it gets flushed back to buddy allocator.
 */
int percpu_pagelist_fraction_sysctl_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *length, loff_t *ppos)
{
	struct zone *zone;
	int old_percpu_pagelist_fraction;
	int ret;

	mutex_lock(&pcp_batch_high_lock);
	old_percpu_pagelist_fraction = percpu_pagelist_fraction;

	ret = proc_dointvec_minmax(table, write, buffer, length, ppos);
	if (!write || ret < 0)
		goto out;

	/* Sanity checking to avoid pcp imbalance */
	if (percpu_pagelist_fraction &&
	    percpu_pagelist_fraction < MIN_PERCPU_PAGELIST_FRACTION) {
		percpu_pagelist_fraction = old_percpu_pagelist_fraction;
		ret = -EINVAL;
		goto out;
	}

	/* No change? */
	if (percpu_pagelist_fraction == old_percpu_pagelist_fraction)
		goto out;

	for_each_populated_zone(zone) {
		unsigned int cpu;

		for_each_possible_cpu(cpu)
			pageset_set_high_and_batch(zone,
					per_cpu_ptr(zone->pageset, cpu));
	}
out:
	mutex_unlock(&pcp_batch_high_lock);
	return ret;
}

#ifdef CONFIG_NUMA
int hashdist = HASHDIST_DEFAULT;

static int __init set_hashdist(char *str)
{
	if (!str)
		return 0;
	hashdist = simple_strtoul(str, &str, 0);
	return 1;
}
__setup("hashdist=", set_hashdist);
#endif

#ifndef __HAVE_ARCH_RESERVED_KERNEL_PAGES
/*
 * Returns the number of pages that arch has reserved but
 * is not known to alloc_large_system_hash().
 */
static unsigned long __init arch_reserved_kernel_pages(void)
{
	return 0;
}
#endif

/*
 * Adaptive scale is meant to reduce sizes of hash tables on large memory
 * machines. As memory size is increased the scale is also increased but at
 * slower pace.  Starting from ADAPT_SCALE_BASE (64G), every time memory
 * quadruples the scale is increased by one, which means the size of hash table
 * only doubles, instead of quadrupling as well.
 * Because 32-bit systems cannot have large physical memory, where this scaling
 * makes sense, it is disabled on such platforms.
 */
#if __BITS_PER_LONG > 32
#define ADAPT_SCALE_BASE	(64ul << 30)
#define ADAPT_SCALE_SHIFT	2
#define ADAPT_SCALE_NPAGES	(ADAPT_SCALE_BASE >> PAGE_SHIFT)
#endif

/*
 * allocate a large system hash table from bootmem
 * - it is assumed that the hash table must contain an exact power-of-2
 *   quantity of entries
 * - limit is the number of hash buckets, not the total allocation size
 */
void *__init alloc_large_system_hash(const char *tablename,
				     unsigned long bucketsize,
				     unsigned long numentries,
				     int scale,
				     int flags,
				     unsigned int *_hash_shift,
				     unsigned int *_hash_mask,
				     unsigned long low_limit,
				     unsigned long high_limit)
{
	unsigned long long max = high_limit;
	unsigned long log2qty, size;
	void *table = NULL;
	gfp_t gfp_flags;

	/* allow the kernel cmdline to have a say */
	if (!numentries) {
		/* round applicable memory size up to nearest megabyte */
		numentries = nr_kernel_pages;
		numentries -= arch_reserved_kernel_pages();

		/* It isn't necessary when PAGE_SIZE >= 1MB */
		if (PAGE_SHIFT < 20)
			numentries = round_up(numentries, (1<<20)/PAGE_SIZE);

#if __BITS_PER_LONG > 32
		if (!high_limit) {
			unsigned long adapt;

			for (adapt = ADAPT_SCALE_NPAGES; adapt < numentries;
			     adapt <<= ADAPT_SCALE_SHIFT)
				scale++;
		}
#endif

		/* limit to 1 bucket per 2^scale bytes of low memory */
		if (scale > PAGE_SHIFT)
			numentries >>= (scale - PAGE_SHIFT);
		else
			numentries <<= (PAGE_SHIFT - scale);

		/* Make sure we've got at least a 0-order allocation.. */
		if (unlikely(flags & HASH_SMALL)) {
			/* Makes no sense without HASH_EARLY */
			WARN_ON(!(flags & HASH_EARLY));
			if (!(numentries >> *_hash_shift)) {
				numentries = 1UL << *_hash_shift;
				BUG_ON(!numentries);
			}
		} else if (unlikely((numentries * bucketsize) < PAGE_SIZE))
			numentries = PAGE_SIZE / bucketsize;
	}
	numentries = roundup_pow_of_two(numentries);

	/* limit allocation size to 1/16 total memory by default */
	if (max == 0) {
		max = ((unsigned long long)nr_all_pages << PAGE_SHIFT) >> 4;
		do_div(max, bucketsize);
	}
	max = min(max, 0x80000000ULL);

	if (numentries < low_limit)
		numentries = low_limit;
	if (numentries > max)
		numentries = max;

	log2qty = ilog2(numentries);

	gfp_flags = (flags & HASH_ZERO) ? GFP_ATOMIC | __GFP_ZERO : GFP_ATOMIC;
	do {
		size = bucketsize << log2qty;
		if (flags & HASH_EARLY) {
			if (flags & HASH_ZERO)
				table = memblock_virt_alloc_nopanic(size, 0);
			else
				table = memblock_virt_alloc_raw(size, 0);
		} else if (hashdist) {
			table = __vmalloc(size, gfp_flags, PAGE_KERNEL);
		} else {
			/*
			 * If bucketsize is not a power-of-two, we may free
			 * some pages at the end of hash table which
			 * alloc_pages_exact() automatically does
			 */
			if (get_order(size) < MAX_ORDER) {
				table = alloc_pages_exact(size, gfp_flags);
				kmemleak_alloc(table, size, 1, gfp_flags);
			}
		}
	} while (!table && size > PAGE_SIZE && --log2qty);

	if (!table)
		panic("Failed to allocate %s hash table\n", tablename);

	pr_info("%s hash table entries: %ld (order: %d, %lu bytes)\n",
		tablename, 1UL << log2qty, ilog2(size) - PAGE_SHIFT, size);

	if (_hash_shift)
		*_hash_shift = log2qty;
	if (_hash_mask)
		*_hash_mask = (1 << log2qty) - 1;

	return table;
}

/*
 * This function checks whether pageblock includes unmovable pages or not.
 * If @count is not zero, it is okay to include less @count unmovable pages
 *
 * PageLRU check without isolation or lru_lock could race so that
 * MIGRATE_MOVABLE block might include unmovable pages. And __PageMovable
 * check without lock_page also may miss some movable non-lru pages at
 * race condition. So you can't expect this function should be exact.
 */
bool has_unmovable_pages(struct zone *zone, struct page *page, int count,
			 int migratetype,
			 bool skip_hwpoisoned_pages)
{
	unsigned long pfn, iter, found;

	/*
	 * TODO we could make this much more efficient by not checking every
	 * page in the range if we know all of them are in MOVABLE_ZONE and
	 * that the movable zone guarantees that pages are migratable but
	 * the later is not the case right now unfortunatelly. E.g. movablecore
	 * can still lead to having bootmem allocations in zone_movable.
	 */

	/*
	 * CMA allocations (alloc_contig_range) really need to mark isolate
	 * CMA pageblocks even when they are not movable in fact so consider
	 * them movable here.
	 */
	if (is_migrate_cma(migratetype) &&
			is_migrate_cma(get_pageblock_migratetype(page)))
		return false;

	pfn = page_to_pfn(page);
	for (found = 0, iter = 0; iter < pageblock_nr_pages; iter++) {
		unsigned long check = pfn + iter;

		if (!pfn_valid_within(check))
			continue;

		page = pfn_to_page(check);

		if (PageReserved(page))
			goto unmovable;

		/*
		 * If the zone is movable and we have ruled out all reserved
		 * pages then it should be reasonably safe to assume the rest
		 * is movable.
		 */
		if (zone_idx(zone) == ZONE_MOVABLE)
			continue;

		/*
		 * Hugepages are not in LRU lists, but they're movable.
		 * We need not scan over tail pages because we don't
		 * handle each tail page individually in migration.
		 */
		if (PageHuge(page)) {
			struct page *head = compound_head(page);
			unsigned int skip_pages;

			if (!hugepage_migration_supported(page_hstate(head)))
				goto unmovable;

			skip_pages = (1 << compound_order(head)) - (page - head);
			iter += skip_pages - 1;
			continue;
		}

		/*
		 * We can't use page_count without pin a page
		 * because another CPU can free compound page.
		 * This check already skips compound tails of THP
		 * because their page->_refcount is zero at all time.
		 */
		if (!page_ref_count(page)) {
			if (PageBuddy(page))
				iter += (1 << page_order(page)) - 1;
			continue;
		}

		/*
		 * The HWPoisoned page may be not in buddy system, and
		 * page_count() is not 0.
		 */
		if (skip_hwpoisoned_pages && PageHWPoison(page))
			continue;

		if (__PageMovable(page))
			continue;

		if (!PageLRU(page))
			found++;
		/*
		 * If there are RECLAIMABLE pages, we need to check
		 * it.  But now, memory offline itself doesn't call
		 * shrink_node_slabs() and it still to be fixed.
		 */
		/*
		 * If the page is not RAM, page_count()should be 0.
		 * we don't need more check. This is an _used_ not-movable page.
		 *
		 * The problematic thing here is PG_reserved pages. PG_reserved
		 * is set to both of a memory hole page and a _used_ kernel
		 * page at boot.
		 */
		if (found > count)
			goto unmovable;
	}
	return false;
unmovable:
	WARN_ON_ONCE(zone_idx(zone) == ZONE_MOVABLE);
	return true;
}

#if (defined(CONFIG_MEMORY_ISOLATION) && defined(CONFIG_COMPACTION)) || defined(CONFIG_CMA)

static unsigned long pfn_max_align_down(unsigned long pfn)
{
	return pfn & ~(max_t(unsigned long, MAX_ORDER_NR_PAGES,
			     pageblock_nr_pages) - 1);
}

static unsigned long pfn_max_align_up(unsigned long pfn)
{
	return ALIGN(pfn, max_t(unsigned long, MAX_ORDER_NR_PAGES,
				pageblock_nr_pages));
}

/* [start, end) must belong to a single zone. */
static int __alloc_contig_migrate_range(struct compact_control *cc,
					unsigned long start, unsigned long end)
{
	/* This function is based on compact_zone() from compaction.c. */
	unsigned long nr_reclaimed;
	unsigned long pfn = start;
	unsigned int tries = 0;
	int ret = 0;

	migrate_prep();

	while (pfn < end || !list_empty(&cc->migratepages)) {
		if (fatal_signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		if (list_empty(&cc->migratepages)) {
			cc->nr_migratepages = 0;
			pfn = isolate_migratepages_range(cc, pfn, end);
			if (!pfn) {
				ret = -EINTR;
				break;
			}
			tries = 0;
		} else if (++tries == 5) {
			ret = ret < 0 ? ret : -EBUSY;
			break;
		}

		nr_reclaimed = reclaim_clean_pages_from_list(cc->zone,
							&cc->migratepages);
		cc->nr_migratepages -= nr_reclaimed;

		ret = migrate_pages(&cc->migratepages, alloc_migrate_target,
				    NULL, 0, cc->mode, MR_CONTIG_RANGE);
	}
	if (ret < 0) {
		putback_movable_pages(&cc->migratepages);
		return ret;
	}
	return 0;
}

/**
 * alloc_contig_range() -- tries to allocate given range of pages
 * @start:	start PFN to allocate
 * @end:	one-past-the-last PFN to allocate
 * @migratetype:	migratetype of the underlaying pageblocks (either
 *			#MIGRATE_MOVABLE or #MIGRATE_CMA).  All pageblocks
 *			in range must have the same migratetype and it must
 *			be either of the two.
 * @gfp_mask:	GFP mask to use during compaction
 *
 * The PFN range does not have to be pageblock or MAX_ORDER_NR_PAGES
 * aligned.  The PFN range must belong to a single zone.
 *
 * The first thing this routine does is attempt to MIGRATE_ISOLATE all
 * pageblocks in the range.  Once isolated, the pageblocks should not
 * be modified by others.
 *
 * Returns zero on success or negative error code.  On success all
 * pages which PFN is in [start, end) are allocated for the caller and
 * need to be freed with free_contig_range().
 */
int alloc_contig_range(unsigned long start, unsigned long end,
		       unsigned migratetype, gfp_t gfp_mask)
{
	unsigned long outer_start, outer_end;
	unsigned int order;
	int ret = 0;

	struct compact_control cc = {
		.nr_migratepages = 0,
		.order = -1,
		.zone = page_zone(pfn_to_page(start)),
		.mode = MIGRATE_SYNC,
		.ignore_skip_hint = true,
		.no_set_skip_hint = true,
		.gfp_mask = current_gfp_context(gfp_mask),
	};
	INIT_LIST_HEAD(&cc.migratepages);

	/*
	 * What we do here is we mark all pageblocks in range as
	 * MIGRATE_ISOLATE.  Because pageblock and max order pages may
	 * have different sizes, and due to the way page allocator
	 * work, we align the range to biggest of the two pages so
	 * that page allocator won't try to merge buddies from
	 * different pageblocks and change MIGRATE_ISOLATE to some
	 * other migration type.
	 *
	 * Once the pageblocks are marked as MIGRATE_ISOLATE, we
	 * migrate the pages from an unaligned range (ie. pages that
	 * we are interested in).  This will put all the pages in
	 * range back to page allocator as MIGRATE_ISOLATE.
	 *
	 * When this is done, we take the pages in range from page
	 * allocator removing them from the buddy system.  This way
	 * page allocator will never consider using them.
	 *
	 * This lets us mark the pageblocks back as
	 * MIGRATE_CMA/MIGRATE_MOVABLE so that free pages in the
	 * aligned range but not in the unaligned, original range are
	 * put back to page allocator so that buddy can use them.
	 */

	ret = start_isolate_page_range(pfn_max_align_down(start),
				       pfn_max_align_up(end), migratetype,
				       false);
	if (ret)
		return ret;

#ifdef CONFIG_CMA
	cc.zone->cma_alloc = 1;
#endif
	/*
	 * In case of -EBUSY, we'd like to know which page causes problem.
	 * So, just fall through. test_pages_isolated() has a tracepoint
	 * which will report the busy page.
	 *
	 * It is possible that busy pages could become available before
	 * the call to test_pages_isolated, and the range will actually be
	 * allocated.  So, if we fall through be sure to clear ret so that
	 * -EBUSY is not accidentally used or returned to caller.
	 */
	ret = __alloc_contig_migrate_range(&cc, start, end);
	if (ret && ret != -EBUSY)
		goto done;
	ret =0;

	/*
	 * Pages from [start, end) are within a MAX_ORDER_NR_PAGES
	 * aligned blocks that are marked as MIGRATE_ISOLATE.  What's
	 * more, all pages in [start, end) are free in page allocator.
	 * What we are going to do is to allocate all pages from
	 * [start, end) (that is remove them from page allocator).
	 *
	 * The only problem is that pages at the beginning and at the
	 * end of interesting range may be not aligned with pages that
	 * page allocator holds, ie. they can be part of higher order
	 * pages.  Because of this, we reserve the bigger range and
	 * once this is done free the pages we are not interested in.
	 *
	 * We don't have to hold zone->lock here because the pages are
	 * isolated thus they won't get removed from buddy.
	 */

	lru_add_drain_all();
	drain_all_pages(cc.zone);

	order = 0;
	outer_start = start;
	while (!PageBuddy(pfn_to_page(outer_start))) {
		if (++order >= MAX_ORDER) {
			outer_start = start;
			break;
		}
		outer_start &= ~0UL << order;
	}

	if (outer_start != start) {
		order = page_order(pfn_to_page(outer_start));

		/*
		 * outer_start page could be small order buddy page and
		 * it doesn't include start page. Adjust outer_start
		 * in this case to report failed page properly
		 * on tracepoint in test_pages_isolated()
		 */
		if (outer_start + (1UL << order) <= start)
			outer_start = start;
	}

	/* Make sure the range is really isolated. */
	if (test_pages_isolated(outer_start, end, false)) {
		pr_info_ratelimited("%s: [%lx, %lx) PFNs busy\n",
			__func__, outer_start, end);
		ret = -EBUSY;
		goto done;
	}

	/* Grab isolated pages from freelists. */
	outer_end = isolate_freepages_range(&cc, outer_start, end);
	if (!outer_end) {
		ret = -EBUSY;
		goto done;
	}

	/* Free head and tail (if any) */
	if (start != outer_start)
		free_contig_range(outer_start, start - outer_start);
	if (end != outer_end)
		free_contig_range(end, outer_end - end);

done:
	undo_isolate_page_range(pfn_max_align_down(start),
				pfn_max_align_up(end), migratetype);
#ifdef CONFIG_CMA
	cc.zone->cma_alloc = 0;
#endif
	return ret;
}

void free_contig_range(unsigned long pfn, unsigned nr_pages)
{
	unsigned int count = 0;

	for (; nr_pages--; pfn++) {
		struct page *page = pfn_to_page(pfn);

		count += page_count(page) != 1;
		__free_page(page);
	}
	WARN(count != 0, "%d pages are still in use!\n", count);
}
#endif

/*
 * The zone indicated has a new number of managed_pages; batch sizes and percpu
 * page high values need to be recalulated.
 */
void __meminit zone_pcp_update(struct zone *zone)
{
	unsigned cpu;
	mutex_lock(&pcp_batch_high_lock);
	for_each_possible_cpu(cpu)
		pageset_set_high_and_batch(zone,
				per_cpu_ptr(zone->pageset, cpu));
	mutex_unlock(&pcp_batch_high_lock);
}

void zone_pcp_reset(struct zone *zone)
{
	unsigned long flags;
	int cpu;
	struct per_cpu_pageset *pset;

	/* avoid races with drain_pages()  */
	local_irq_save(flags);
	if (zone->pageset != &boot_pageset) {
		for_each_online_cpu(cpu) {
			pset = per_cpu_ptr(zone->pageset, cpu);
			drain_zonestat(zone, pset);
		}
		free_percpu(zone->pageset);
		zone->pageset = &boot_pageset;
	}
	local_irq_restore(flags);
}

#ifdef CONFIG_MEMORY_HOTREMOVE
/*
 * All pages in the range must be in a single zone and isolated
 * before calling this.
 */
void
__offline_isolated_pages(unsigned long start_pfn, unsigned long end_pfn)
{
	struct page *page;
	struct zone *zone;
	unsigned int order, i;
	unsigned long pfn;
	unsigned long flags;
	/* find the first valid pfn */
	for (pfn = start_pfn; pfn < end_pfn; pfn++)
		if (pfn_valid(pfn))
			break;
	if (pfn == end_pfn)
		return;
	offline_mem_sections(pfn, end_pfn);
	zone = page_zone(pfn_to_page(pfn));
	spin_lock_irqsave(&zone->lock, flags);
	pfn = start_pfn;
	while (pfn < end_pfn) {
		if (!pfn_valid(pfn)) {
			pfn++;
			continue;
		}
		page = pfn_to_page(pfn);
		/*
		 * The HWPoisoned page may be not in buddy system, and
		 * page_count() is not 0.
		 */
		if (unlikely(!PageBuddy(page) && PageHWPoison(page))) {
			pfn++;
			SetPageReserved(page);
			continue;
		}

		BUG_ON(page_count(page));
		BUG_ON(!PageBuddy(page));
		order = page_order(page);
#ifdef CONFIG_DEBUG_VM
		pr_info("remove from free list %lx %d %lx\n",
			pfn, 1 << order, end_pfn);
#endif
		list_del(&page->lru);
		rmv_page_order(page);
		zone->free_area[order].nr_free--;
		for (i = 0; i < (1 << order); i++)
			SetPageReserved((page+i));
		pfn += (1 << order);
	}
	spin_unlock_irqrestore(&zone->lock, flags);
}
#endif

bool is_free_buddy_page(struct page *page)
{
	struct zone *zone = page_zone(page);
	unsigned long pfn = page_to_pfn(page);
	unsigned long flags;
	unsigned int order;

	spin_lock_irqsave(&zone->lock, flags);
	for (order = 0; order < MAX_ORDER; order++) {
		struct page *page_head = page - (pfn & ((1 << order) - 1));

		if (PageBuddy(page_head) && page_order(page_head) >= order)
			break;
	}
	spin_unlock_irqrestore(&zone->lock, flags);

	return order < MAX_ORDER;
}

#ifdef CONFIG_MEMORY_FAILURE
/*
 * Set PG_hwpoison flag if a given page is confirmed to be a free page.  This
 * test is performed under the zone lock to prevent a race against page
 * allocation.
 */
bool set_hwpoison_free_buddy_page(struct page *page)
{
	struct zone *zone = page_zone(page);
	unsigned long pfn = page_to_pfn(page);
	unsigned long flags;
	unsigned int order;
	bool hwpoisoned = false;

	spin_lock_irqsave(&zone->lock, flags);
	for (order = 0; order < MAX_ORDER; order++) {
		struct page *page_head = page - (pfn & ((1 << order) - 1));

		if (PageBuddy(page_head) && page_order(page_head) >= order) {
			if (!TestSetPageHWPoison(page))
				hwpoisoned = true;
			break;
		}
	}
	spin_unlock_irqrestore(&zone->lock, flags);

	return hwpoisoned;
}
#endif
