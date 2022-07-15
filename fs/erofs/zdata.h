/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2018 HUAWEI, Inc.
 *             https://www.huawei.com/
 */
#ifndef __EROFS_FS_ZDATA_H
#define __EROFS_FS_ZDATA_H

#include "internal.h"
#include "tagptr.h"

#define Z_EROFS_PCLUSTER_MAX_PAGES	(Z_EROFS_PCLUSTER_MAX_SIZE / PAGE_SIZE)
#define Z_EROFS_INLINE_BVECS		2

#define Z_EROFS_PCLUSTER_FULL_LENGTH    0x00000001
#define Z_EROFS_PCLUSTER_LENGTH_BIT     1

/*
 * let's leave a type here in case of introducing
 * another tagged pointer later.
 */
typedef void *z_erofs_next_pcluster_t;

struct z_erofs_bvec {
	struct page *page;
	int offset;
	unsigned int end;
};

#define __Z_EROFS_BVSET(name, total) \
struct name { \
	/* point to the next page which contains the following bvecs */ \
	struct page *nextpage; \
	struct z_erofs_bvec bvec[total]; \
}
__Z_EROFS_BVSET(z_erofs_bvset,);
__Z_EROFS_BVSET(z_erofs_bvset_inline, Z_EROFS_INLINE_BVECS);

/*
 * Structure fields follow one of the following exclusion rules.
 *
 * I: Modifiable by initialization/destruction paths and read-only
 *    for everyone else;
 *
 * L: Field should be protected by the pcluster lock;
 *
 * A: Field should be accessed / updated in atomic for parallelized code.
 */
struct z_erofs_pcluster {
	struct erofs_workgroup obj;
	struct mutex lock;

	/* A: point to next chained pcluster or TAILs */
	z_erofs_next_pcluster_t next;

	/* A: lower limit of decompressed length and if full length or not */
	unsigned int length;

	/* L: total number of bvecs */
	unsigned int vcnt;

	/* I: page offset of start position of decompression */
	unsigned short pageofs_out;

	/* I: page offset of inline compressed data */
	unsigned short pageofs_in;

	/* L: maximum relative page index in bvecs */
	unsigned short nr_pages;

	union {
		/* L: inline a certain number of bvec for bootstrap */
		struct z_erofs_bvset_inline bvset;

		/* I: can be used to free the pcluster by RCU. */
		struct rcu_head rcu;
	};

	union {
		/* I: physical cluster size in pages */
		unsigned short pclusterpages;

		/* I: tailpacking inline compressed size */
		unsigned short tailpacking_size;
	};

	/* I: compression algorithm format */
	unsigned char algorithmformat;

	/* A: compressed bvecs (can be cached or inplaced pages) */
	struct z_erofs_bvec compressed_bvecs[];
};

/* let's avoid the valid 32-bit kernel addresses */

/* the chained workgroup has't submitted io (still open) */
#define Z_EROFS_PCLUSTER_TAIL           ((void *)0x5F0ECAFE)
/* the chained workgroup has already submitted io */
#define Z_EROFS_PCLUSTER_TAIL_CLOSED    ((void *)0x5F0EDEAD)

#define Z_EROFS_PCLUSTER_NIL            (NULL)

struct z_erofs_decompressqueue {
	struct super_block *sb;
	atomic_t pending_bios;
	z_erofs_next_pcluster_t head;

	union {
		struct completion done;
		struct work_struct work;
	} u;
};

static inline bool z_erofs_is_inline_pcluster(struct z_erofs_pcluster *pcl)
{
	return !pcl->obj.index;
}

static inline unsigned int z_erofs_pclusterpages(struct z_erofs_pcluster *pcl)
{
	if (z_erofs_is_inline_pcluster(pcl))
		return 1;
	return pcl->pclusterpages;
}

#define Z_EROFS_ONLINEPAGE_COUNT_BITS   2
#define Z_EROFS_ONLINEPAGE_COUNT_MASK   ((1 << Z_EROFS_ONLINEPAGE_COUNT_BITS) - 1)
#define Z_EROFS_ONLINEPAGE_INDEX_SHIFT  (Z_EROFS_ONLINEPAGE_COUNT_BITS)

/*
 * waiters (aka. ongoing_packs): # to unlock the page
 * sub-index: 0 - for partial page, >= 1 full page sub-index
 */
typedef atomic_t z_erofs_onlinepage_t;

/* type punning */
union z_erofs_onlinepage_converter {
	z_erofs_onlinepage_t *o;
	unsigned long *v;
};

static inline unsigned int z_erofs_onlinepage_index(struct page *page)
{
	union z_erofs_onlinepage_converter u;

	DBG_BUGON(!PagePrivate(page));
	u.v = &page_private(page);

	return atomic_read(u.o) >> Z_EROFS_ONLINEPAGE_INDEX_SHIFT;
}

static inline void z_erofs_onlinepage_init(struct page *page)
{
	union {
		z_erofs_onlinepage_t o;
		unsigned long v;
	/* keep from being unlocked in advance */
	} u = { .o = ATOMIC_INIT(1) };

	set_page_private(page, u.v);
	smp_wmb();
	SetPagePrivate(page);
}

static inline void z_erofs_onlinepage_fixup(struct page *page,
	uintptr_t index, bool down)
{
	union z_erofs_onlinepage_converter u = { .v = &page_private(page) };
	int orig, orig_index, val;

repeat:
	orig = atomic_read(u.o);
	orig_index = orig >> Z_EROFS_ONLINEPAGE_INDEX_SHIFT;
	if (orig_index) {
		if (!index)
			return;

		DBG_BUGON(orig_index != index);
	}

	val = (index << Z_EROFS_ONLINEPAGE_INDEX_SHIFT) |
		((orig & Z_EROFS_ONLINEPAGE_COUNT_MASK) + (unsigned int)down);
	if (atomic_cmpxchg(u.o, orig, val) != orig)
		goto repeat;
}

static inline void z_erofs_onlinepage_endio(struct page *page)
{
	union z_erofs_onlinepage_converter u;
	unsigned int v;

	DBG_BUGON(!PagePrivate(page));
	u.v = &page_private(page);

	v = atomic_dec_return(u.o);
	if (!(v & Z_EROFS_ONLINEPAGE_COUNT_MASK)) {
		set_page_private(page, 0);
		ClearPagePrivate(page);
		if (!PageError(page))
			SetPageUptodate(page);
		unlock_page(page);
	}
	erofs_dbg("%s, page %p value %x", __func__, page, atomic_read(u.o));
}

#define Z_EROFS_VMAP_ONSTACK_PAGES	\
	min_t(unsigned int, THREAD_SIZE / 8 / sizeof(struct page *), 96U)
#define Z_EROFS_VMAP_GLOBAL_PAGES	2048

/**
 * attach_page_private - Attach private data to a page.
 * @page: Page to attach data to.
 * @data: Data to attach to page.
 *
 * Attaching private data to a page increments the page's reference count.
 * The data must be detached before the page will be freed.
 */
static inline void attach_page_private(struct page *page, void *data)
{
	get_page(page);
	set_page_private(page, (unsigned long)data);
	SetPagePrivate(page);
}

/**
 * detach_page_private - Detach private data from a page.
 * @page: Page to detach data from.
 *
 * Removes the data that was previously attached to the page and decrements
 * the refcount on the page.
 *
 * Return: Data that was attached to the page.
 */
static inline void *detach_page_private(struct page *page)
{
	void *data = (void *)page_private(page);

	if (!PagePrivate(page))
		return NULL;
	ClearPagePrivate(page);
	set_page_private(page, 0);
	put_page(page);

	return data;
}

#endif
