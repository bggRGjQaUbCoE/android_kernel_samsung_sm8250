// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2008-2021, The Linux Foundation. All rights reserved.
 */

#include <uapi/linux/sched/types.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/dma-buf.h>
#include <linux/fdtable.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/msm-bus.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/pm_runtime.h>
#include <linux/security.h>
#include <linux/sort.h>
#include <asm/cacheflush.h>

#include "kgsl_compat.h"
#include "kgsl_debugfs.h"
#include "kgsl_device.h"
#include "kgsl_mmu.h"
#include "kgsl_reclaim.h"
#include "kgsl_sync.h"
#include "kgsl_trace.h"

#ifndef arch_mmap_check
#define arch_mmap_check(addr, len, flags)	(0)
#endif

#ifndef pgprot_writebackcache
#define pgprot_writebackcache(_prot)	(_prot)
#endif

#ifndef pgprot_writethroughcache
#define pgprot_writethroughcache(_prot)	(_prot)
#endif

#ifdef CONFIG_ARM_LPAE
#define KGSL_DMA_BIT_MASK	DMA_BIT_MASK(64)
#else
#define KGSL_DMA_BIT_MASK	DMA_BIT_MASK(32)
#endif

/* Mutex used for the IOMMU sync quirk */
DEFINE_MUTEX(kgsl_mmu_sync);
EXPORT_SYMBOL(kgsl_mmu_sync);

/* List of dmabufs mapped */
static LIST_HEAD(kgsl_dmabuf_list);
static DEFINE_SPINLOCK(kgsl_dmabuf_lock);

struct dmabuf_list_entry {
	struct page *firstpage;
	struct list_head node;
	struct list_head dmabuf_list;
};

struct kgsl_dma_buf_meta {
	struct kgsl_mem_entry *entry;
	struct dma_buf_attachment *attach;
	struct dma_buf *dmabuf;
	struct sg_table *table;
	struct dmabuf_list_entry *dle;
	struct list_head node;
};

static inline struct kgsl_pagetable *_get_memdesc_pagetable(
		struct kgsl_pagetable *pt, struct kgsl_mem_entry *entry)
{
	/* if a secured buffer, map it to secure global pagetable */
	if (kgsl_memdesc_is_secured(&entry->memdesc))
		return pt->mmu->securepagetable;

	return pt;
}

static void kgsl_mem_entry_detach_process(struct kgsl_mem_entry *entry);

static const struct vm_operations_struct kgsl_gpumem_vm_ops;

/*
 * The memfree list contains the last N blocks of memory that have been freed.
 * On a GPU fault we walk the list to see if the faulting address had been
 * recently freed and print out a message to that effect
 */

#define MEMFREE_ENTRIES 512

static DEFINE_SPINLOCK(memfree_lock);

struct memfree_entry {
	pid_t ptname;
	uint64_t gpuaddr;
	uint64_t size;
	pid_t pid;
	uint64_t flags;
};

static struct {
	struct memfree_entry *list;
	int head;
	int tail;
} memfree;

static inline bool match_memfree_addr(struct memfree_entry *entry,
		pid_t ptname, uint64_t gpuaddr)
{
	return ((entry->ptname == ptname) &&
		(entry->size > 0) &&
		(gpuaddr >= entry->gpuaddr &&
			 gpuaddr < (entry->gpuaddr + entry->size)));
}
int kgsl_memfree_find_entry(pid_t ptname, uint64_t *gpuaddr,
	uint64_t *size, uint64_t *flags, pid_t *pid)
{
	int ptr;

	if (memfree.list == NULL)
		return 0;

	spin_lock(&memfree_lock);

	ptr = memfree.head - 1;
	if (ptr < 0)
		ptr = MEMFREE_ENTRIES - 1;

	/* Walk backwards through the list looking for the last match  */
	while (ptr != memfree.tail) {
		struct memfree_entry *entry = &memfree.list[ptr];

		if (match_memfree_addr(entry, ptname, *gpuaddr)) {
			*gpuaddr = entry->gpuaddr;
			*flags = entry->flags;
			*size = entry->size;
			*pid = entry->pid;

			spin_unlock(&memfree_lock);
			return 1;
		}

		ptr = ptr - 1;

		if (ptr < 0)
			ptr = MEMFREE_ENTRIES - 1;
	}

	spin_unlock(&memfree_lock);
	return 0;
}

static void kgsl_memfree_purge(struct kgsl_pagetable *pagetable,
		uint64_t gpuaddr, uint64_t size)
{
	pid_t ptname = pagetable ? pagetable->name : 0;
	int i;

	if (memfree.list == NULL)
		return;

	spin_lock(&memfree_lock);

	for (i = 0; i < MEMFREE_ENTRIES; i++) {
		struct memfree_entry *entry = &memfree.list[i];

		if (entry->ptname != ptname || entry->size == 0)
			continue;

		if (gpuaddr > entry->gpuaddr &&
			gpuaddr < entry->gpuaddr + entry->size) {
			/* truncate the end of the entry */
			entry->size = gpuaddr - entry->gpuaddr;
		} else if (gpuaddr <= entry->gpuaddr) {
			if (gpuaddr + size > entry->gpuaddr &&
				gpuaddr + size < entry->gpuaddr + entry->size)
				/* Truncate the beginning of the entry */
				entry->gpuaddr = gpuaddr + size;
			else if (gpuaddr + size >= entry->gpuaddr + entry->size)
				/* Remove the entire entry */
				entry->size = 0;
		}
	}
	spin_unlock(&memfree_lock);
}

static void kgsl_memfree_add(pid_t pid, pid_t ptname, uint64_t gpuaddr,
		uint64_t size, uint64_t flags)

{
	struct memfree_entry *entry;

	if (memfree.list == NULL)
		return;

	spin_lock(&memfree_lock);

	entry = &memfree.list[memfree.head];

	entry->pid = pid;
	entry->ptname = ptname;
	entry->gpuaddr = gpuaddr;
	entry->size = size;
	entry->flags = flags;

	memfree.head = (memfree.head + 1) % MEMFREE_ENTRIES;

	if (memfree.head == memfree.tail)
		memfree.tail = (memfree.tail + 1) % MEMFREE_ENTRIES;

	spin_unlock(&memfree_lock);
}

int kgsl_readtimestamp(struct kgsl_device *device, void *priv,
		enum kgsl_timestamp_type type, unsigned int *timestamp)
{
	if (device)
		return device->ftbl->readtimestamp(device, priv, type,
			timestamp);
	return -EINVAL;

}
EXPORT_SYMBOL(kgsl_readtimestamp);

static struct kgsl_mem_entry *kgsl_mem_entry_create(void)
{
	struct kgsl_mem_entry *entry = kzalloc(sizeof(*entry), GFP_KERNEL);

	if (entry != NULL) {
		kref_init(&entry->refcount);
		/* put this ref in userspace memory alloc and map ioctls */
		kref_get(&entry->refcount);
		atomic_set(&entry->map_count, 0);
	}

	return entry;
}

static void add_dmabuf_list(struct kgsl_dma_buf_meta *meta)
{
	struct dmabuf_list_entry *dle;
	struct page *page;

	/*
	 * Get the first page. We will use it to identify the imported
	 * buffer, since the same buffer can be mapped as different
	 * mem entries.
	 */
	page = sg_page(meta->table->sgl);

	spin_lock(&kgsl_dmabuf_lock);

	/* Go through the list to see if we imported this buffer before */
	list_for_each_entry(dle, &kgsl_dmabuf_list, node) {
		if (dle->firstpage == page) {
			/* Add the dmabuf meta to the list for this dle */
			meta->dle = dle;
			list_add(&meta->node, &dle->dmabuf_list);
			spin_unlock(&kgsl_dmabuf_lock);
			return;
		}
	}

	/* This is a new buffer. Add a new entry for it */
	dle = kzalloc(sizeof(*dle), GFP_ATOMIC);
	if (dle) {
		dle->firstpage = page;
		INIT_LIST_HEAD(&dle->dmabuf_list);
		list_add(&dle->node, &kgsl_dmabuf_list);
		meta->dle = dle;
		list_add(&meta->node, &dle->dmabuf_list);
	}
	spin_unlock(&kgsl_dmabuf_lock);
}

static void remove_dmabuf_list(struct kgsl_dma_buf_meta *meta)
{
	struct dmabuf_list_entry *dle = meta->dle;

	if (!dle)
		return;

	spin_lock(&kgsl_dmabuf_lock);
	list_del(&meta->node);
	if (list_empty(&dle->dmabuf_list)) {
		list_del(&dle->node);
		kfree(dle);
	}
	spin_unlock(&kgsl_dmabuf_lock);
}

#ifdef CONFIG_DMA_SHARED_BUFFER
static void kgsl_destroy_ion(struct kgsl_dma_buf_meta *meta)
{
	if (meta != NULL) {
		remove_dmabuf_list(meta);
		dma_buf_unmap_attachment(meta->attach, meta->table,
			DMA_BIDIRECTIONAL);
		dma_buf_detach(meta->dmabuf, meta->attach);
		dma_buf_put(meta->dmabuf);
		kfree(meta);
	}
}
#else
static void kgsl_destroy_ion(struct kgsl_dma_buf_meta *meta)
{

}
#endif

static void mem_entry_destroy(struct kgsl_mem_entry *entry)
{
	unsigned int memtype;

	/* pull out the memtype before the flags get cleared */
	memtype = kgsl_memdesc_usermem_type(&entry->memdesc);

	if (!(entry->memdesc.flags & KGSL_MEMFLAGS_SPARSE_VIRT))
		kgsl_process_sub_stats(entry->priv, memtype,
			entry->memdesc.size);

	/* Detach from process list */
	kgsl_mem_entry_detach_process(entry);

	if (memtype != KGSL_MEM_ENTRY_KERNEL)
		atomic_long_sub(entry->memdesc.size,
			&kgsl_driver.stats.mapped);

	/*
	 * Ion takes care of freeing the sg_table for us so
	 * clear the sg table before freeing the sharedmem
	 * so kgsl_sharedmem_free doesn't try to free it again
	 */
	if (memtype == KGSL_MEM_ENTRY_ION)
		entry->memdesc.sgt = NULL;

	if ((memtype == KGSL_MEM_ENTRY_USER)
		&& !(entry->memdesc.flags & KGSL_MEMFLAGS_GPUREADONLY)) {
		int i = 0, j;
		struct scatterlist *sg;
		struct page *page;
		/*
		 * Mark all of pages in the scatterlist as dirty since they
		 * were writable by the GPU.
		 */
		for_each_sg(entry->memdesc.sgt->sgl, sg,
			    entry->memdesc.sgt->nents, i) {
			page = sg_page(sg);
			for (j = 0; j < (sg->length >> PAGE_SHIFT); j++)
				set_page_dirty_lock(nth_page(page, j));
		}
	}

	kgsl_sharedmem_free(&entry->memdesc);

	switch (memtype) {
	case KGSL_MEM_ENTRY_ION:
		kgsl_destroy_ion(entry->priv_data);
		break;
	default:
		break;
	}

	kfree(entry);
}

static void _deferred_destroy(struct work_struct *work)
{
	struct kgsl_mem_entry *entry =
		container_of(work, struct kgsl_mem_entry, work);

	mem_entry_destroy(entry);
}

void kgsl_mem_entry_destroy(struct kref *kref)
{
	struct kgsl_mem_entry *entry =
		container_of(kref, struct kgsl_mem_entry, refcount);

	mem_entry_destroy(entry);
}
EXPORT_SYMBOL(kgsl_mem_entry_destroy);

void kgsl_mem_entry_destroy_deferred(struct kref *kref)
{
	struct kgsl_mem_entry *entry =
		container_of(kref, struct kgsl_mem_entry, refcount);

	INIT_WORK(&entry->work, _deferred_destroy);
	queue_work(kgsl_driver.mem_workqueue, &entry->work);
}
EXPORT_SYMBOL(kgsl_mem_entry_destroy_deferred);

/* Allocate a IOVA for memory objects that don't use SVM */
static int kgsl_mem_entry_track_gpuaddr(struct kgsl_device *device,
		struct kgsl_process_private *process,
		struct kgsl_mem_entry *entry)
{
	struct kgsl_pagetable *pagetable;

	/*
	 * If SVM is enabled for this object then the address needs to be
	 * assigned elsewhere
	 * Also do not proceed further in case of NoMMU.
	 */
	if (kgsl_memdesc_use_cpu_map(&entry->memdesc) ||
		(kgsl_mmu_get_mmutype(device) == KGSL_MMU_TYPE_NONE))
		return 0;

	pagetable = kgsl_memdesc_is_secured(&entry->memdesc) ?
		device->mmu.securepagetable : process->pagetable;

	return kgsl_mmu_get_gpuaddr(pagetable, &entry->memdesc);
}

/* Commit the entry to the process so it can be accessed by other operations */
static void kgsl_mem_entry_commit_process(struct kgsl_mem_entry *entry)
{
	if (!entry)
		return;

	spin_lock(&entry->priv->mem_lock);
	idr_replace(&entry->priv->mem_idr, entry, entry->id);
	spin_unlock(&entry->priv->mem_lock);
}

/*
 * Attach the memory object to a process by (possibly) getting a GPU address and
 * (possibly) mapping it
 */
static int kgsl_mem_entry_attach_process(struct kgsl_device *device,
		struct kgsl_process_private *process,
		struct kgsl_mem_entry *entry)
{
	int id, ret;

	ret = kgsl_process_private_get(process);
	if (!ret)
		return -EBADF;

	ret = kgsl_mem_entry_track_gpuaddr(device, process, entry);
	if (ret) {
		kgsl_process_private_put(process);
		return ret;
	}

	idr_preload(GFP_KERNEL);
	spin_lock(&process->mem_lock);
	/* Allocate the ID but don't attach the pointer just yet */
	id = idr_alloc(&process->mem_idr, NULL, 1, 0, GFP_NOWAIT);
	spin_unlock(&process->mem_lock);
	idr_preload_end();

	if (id < 0) {
		if (!kgsl_memdesc_use_cpu_map(&entry->memdesc))
			kgsl_mmu_put_gpuaddr(&entry->memdesc);
		kgsl_process_private_put(process);
		return id;
	}

	entry->id = id;
	entry->priv = process;

	/*
	 * Map the memory if a GPU address is already assigned, either through
	 * kgsl_mem_entry_track_gpuaddr() or via some other SVM process
	 */
	if (entry->memdesc.gpuaddr) {
		if (entry->memdesc.flags & KGSL_MEMFLAGS_SPARSE_VIRT)
			ret = kgsl_mmu_sparse_dummy_map(
				entry->memdesc.pagetable,
				&entry->memdesc, 0,
				kgsl_memdesc_footprint(&entry->memdesc));
		else if (entry->memdesc.gpuaddr)
			ret = kgsl_mmu_map(entry->memdesc.pagetable,
					&entry->memdesc);

		if (ret)
			kgsl_mem_entry_detach_process(entry);
	}

	kgsl_memfree_purge(entry->memdesc.pagetable, entry->memdesc.gpuaddr,
		entry->memdesc.size);

	return ret;
}

/* Detach a memory entry from a process and unmap it from the MMU */
static void kgsl_mem_entry_detach_process(struct kgsl_mem_entry *entry)
{
	if (entry == NULL)
		return;

	/*
	 * First remove the entry from mem_idr list
	 * so that no one can operate on obsolete values
	 */
	spin_lock(&entry->priv->mem_lock);
	if (entry->id != 0)
		idr_remove(&entry->priv->mem_idr, entry->id);
	entry->id = 0;

	spin_unlock(&entry->priv->mem_lock);

	kgsl_mmu_put_gpuaddr(&entry->memdesc);

	atomic_sub(entry->memdesc.reclaimed_page_count,
			&entry->priv->reclaimed_page_count);


	kgsl_process_private_put(entry->priv);

	entry->priv = NULL;
}

/**
 * kgsl_context_dump() - dump information about a draw context
 * @device: KGSL device that owns the context
 * @context: KGSL context to dump information about
 *
 * Dump specific information about the context to the kernel log.  Used for
 * fence timeout callbacks
 */
void kgsl_context_dump(struct kgsl_context *context)
{
	struct kgsl_device *device;

	if (_kgsl_context_get(context) == 0)
		return;

	device = context->device;

	if (kgsl_context_detached(context)) {
		dev_err(device->dev, "  context[%u]: context detached\n",
			context->id);
	} else if (device->ftbl->drawctxt_dump != NULL)
		device->ftbl->drawctxt_dump(device, context);

	kgsl_context_put(context);
}
EXPORT_SYMBOL(kgsl_context_dump);

/* Allocate a new context ID */
static int _kgsl_get_context_id(struct kgsl_device *device)
{
	int id;

	idr_preload(GFP_KERNEL);
	write_lock(&device->context_lock);
	/* Allocate the slot but don't put a pointer in it yet */
	id = idr_alloc(&device->context_idr, NULL, 1,
		KGSL_MEMSTORE_MAX, GFP_NOWAIT);
	write_unlock(&device->context_lock);
	idr_preload_end();

	return id;
}

/**
 * kgsl_context_init() - helper to initialize kgsl_context members
 * @dev_priv: the owner of the context
 * @context: the newly created context struct, should be allocated by
 * the device specific drawctxt_create function.
 *
 * This is a helper function for the device specific drawctxt_create
 * function to initialize the common members of its context struct.
 * If this function succeeds, reference counting is active in the context
 * struct and the caller should kgsl_context_put() it on error.
 * If it fails, the caller should just free the context structure
 * it passed in.
 */
int kgsl_context_init(struct kgsl_device_private *dev_priv,
			struct kgsl_context *context)
{
	struct kgsl_device *device = dev_priv->device;
	int ret = 0, id;
	struct kgsl_process_private  *proc_priv = dev_priv->process_priv;

	/*
	 * Read and increment the context count under lock to make sure
	 * no process goes beyond the specified context limit.
	 */
	spin_lock(&proc_priv->ctxt_count_lock);
	if (atomic_read(&proc_priv->ctxt_count) > KGSL_MAX_CONTEXTS_PER_PROC) {
		dev_err(device->dev,
			     "Per process context limit reached for pid %u\n",
			     pid_nr(dev_priv->process_priv->pid));
		spin_unlock(&proc_priv->ctxt_count_lock);
		return -ENOSPC;
	}

	atomic_inc(&proc_priv->ctxt_count);
	spin_unlock(&proc_priv->ctxt_count_lock);

	id = _kgsl_get_context_id(device);
	if (id == -ENOSPC) {
		/*
		 * Before declaring that there are no contexts left try
		 * flushing the event workqueue just in case there are
		 * detached contexts waiting to finish
		 */

		flush_workqueue(device->events_wq);
		id = _kgsl_get_context_id(device);
	}

	if (id < 0) {
		if (id == -ENOSPC)
			dev_warn(device->dev,
				      "cannot have more than %zu contexts due to memstore limitation\n",
				      KGSL_MEMSTORE_MAX);
		atomic_dec(&proc_priv->ctxt_count);
		return id;
	}

	context->id = id;

	kref_init(&context->refcount);
	/*
	 * Get a refernce to the process private so its not destroyed, until
	 * the context is destroyed. This will also prevent the pagetable
	 * from being destroyed
	 */
	if (!kgsl_process_private_get(dev_priv->process_priv)) {
		ret = -EBADF;
		goto out;
	}
	context->device = dev_priv->device;
	context->dev_priv = dev_priv;
	context->proc_priv = dev_priv->process_priv;
	context->tid = task_pid_nr(current);
	proc_priv->tid = context->tid;

	ret = kgsl_sync_timeline_create(context);
	if (ret) {
		kgsl_process_private_put(dev_priv->process_priv);
		goto out;
	}

	kgsl_add_event_group(&context->events, context,
		kgsl_readtimestamp, context, "context-%d", id);

out:
	if (ret) {
		atomic_dec(&proc_priv->ctxt_count);
		write_lock(&device->context_lock);
		idr_remove(&dev_priv->device->context_idr, id);
		write_unlock(&device->context_lock);
	}

	return ret;
}
EXPORT_SYMBOL(kgsl_context_init);

/**
 * kgsl_context_detach() - Release the "master" context reference
 * @context: The context that will be detached
 *
 * This is called when a context becomes unusable, because userspace
 * has requested for it to be destroyed. The context itself may
 * exist a bit longer until its reference count goes to zero.
 * Other code referencing the context can detect that it has been
 * detached by checking the KGSL_CONTEXT_PRIV_DETACHED bit in
 * context->priv.
 */
void kgsl_context_detach(struct kgsl_context *context)
{
	struct kgsl_device *device;

	if (context == NULL)
		return;

	/*
	 * Mark the context as detached to keep others from using
	 * the context before it gets fully removed, and to make sure
	 * we don't try to detach twice.
	 */
	if (test_and_set_bit(KGSL_CONTEXT_PRIV_DETACHED, &context->priv))
		return;

	device = context->device;

	trace_kgsl_context_detach(device, context);

	context->device->ftbl->drawctxt_detach(context);

	/*
	 * Cancel all pending events after the device-specific context is
	 * detached, to avoid possibly freeing memory while it is still
	 * in use by the GPU.
	 */
	kgsl_cancel_events(device, &context->events);

	/* Remove the event group from the list */
	kgsl_del_event_group(&context->events);

	kgsl_sync_timeline_put(context->ktimeline);

	kgsl_context_put(context);
}

void
kgsl_context_destroy(struct kref *kref)
{
	struct kgsl_context *context = container_of(kref, struct kgsl_context,
						    refcount);
	struct kgsl_device *device = context->device;

	trace_kgsl_context_destroy(device, context);

	/*
	 * It's not safe to destroy the context if it's not detached as GPU
	 * may still be executing commands
	 */
	BUG_ON(!kgsl_context_detached(context));

	write_lock(&device->context_lock);
	if (context->id != KGSL_CONTEXT_INVALID) {

		/* Clear the timestamps in the memstore during destroy */
		kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp), 0);
		kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp), 0);

		/* clear device power constraint */
		if (context->id == device->pwrctrl.constraint.owner_id) {
			trace_kgsl_constraint(device,
				device->pwrctrl.constraint.type,
				device->pwrctrl.active_pwrlevel,
				0);
			device->pwrctrl.constraint.type = KGSL_CONSTRAINT_NONE;
		}

		atomic_dec(&context->proc_priv->ctxt_count);
		idr_remove(&device->context_idr, context->id);
		context->id = KGSL_CONTEXT_INVALID;
	}
	write_unlock(&device->context_lock);
	kgsl_sync_timeline_destroy(context);
	kgsl_process_private_put(context->proc_priv);

	device->ftbl->drawctxt_destroy(context);
}

struct kgsl_device *kgsl_get_device(int dev_idx)
{
	int i;
	struct kgsl_device *ret = NULL;

	mutex_lock(&kgsl_driver.devlock);

	for (i = 0; i < ARRAY_SIZE(kgsl_driver.devp); i++) {
		if (kgsl_driver.devp[i] && kgsl_driver.devp[i]->id == dev_idx) {
			ret = kgsl_driver.devp[i];
			break;
		}
	}

	mutex_unlock(&kgsl_driver.devlock);
	return ret;
}
EXPORT_SYMBOL(kgsl_get_device);

static struct kgsl_device *kgsl_get_minor(int minor)
{
	struct kgsl_device *ret = NULL;

	if (minor < 0 || minor >= ARRAY_SIZE(kgsl_driver.devp))
		return NULL;

	mutex_lock(&kgsl_driver.devlock);
	ret = kgsl_driver.devp[minor];
	mutex_unlock(&kgsl_driver.devlock);

	return ret;
}

/**
 * kgsl_check_timestamp() - return true if the specified timestamp is retired
 * @device: Pointer to the KGSL device to check
 * @context: Pointer to the context for the timestamp
 * @timestamp: The timestamp to compare
 */
int kgsl_check_timestamp(struct kgsl_device *device,
	struct kgsl_context *context, unsigned int timestamp)
{
	unsigned int ts_processed;

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED,
		&ts_processed);

	return (timestamp_cmp(ts_processed, timestamp) >= 0);
}
EXPORT_SYMBOL(kgsl_check_timestamp);

static int kgsl_suspend_device(struct kgsl_device *device, pm_message_t state)
{
	int status = -EINVAL;

	if (!device)
		return -EINVAL;

	mutex_lock(&device->mutex);
	status = kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);
	if (status == 0 && device->state == KGSL_STATE_SUSPEND)
		device->ftbl->dispatcher_halt(device);
	mutex_unlock(&device->mutex);

	return status;
}

static int kgsl_resume_device(struct kgsl_device *device)
{
	if (!device)
		return -EINVAL;

	mutex_lock(&device->mutex);
	if (device->state == KGSL_STATE_SUSPEND) {
		device->ftbl->dispatcher_unhalt(device);
		kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);
	} else if (device->state != KGSL_STATE_INIT) {
		/*
		 * This is an error situation,so wait for the device
		 * to idle and then put the device to SLUMBER state.
		 * This will put the device to the right state when
		 * we resume.
		 */
		if (device->state == KGSL_STATE_ACTIVE)
			device->ftbl->idle(device);
		kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);
		dev_err(device->dev,
			     "resume invoked without a suspend\n");
	}

	mutex_unlock(&device->mutex);
	return 0;
}

static int kgsl_suspend(struct device *dev)
{

	pm_message_t arg = {0};
	struct kgsl_device *device = dev_get_drvdata(dev);

	return kgsl_suspend_device(device, arg);
}

static int kgsl_resume(struct device *dev)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return kgsl_resume_device(device);
}

static int kgsl_runtime_suspend(struct device *dev)
{
	return 0;
}

static int kgsl_runtime_resume(struct device *dev)
{
	return 0;
}

const struct dev_pm_ops kgsl_pm_ops = {
	.suspend = kgsl_suspend,
	.resume = kgsl_resume,
	.runtime_suspend = kgsl_runtime_suspend,
	.runtime_resume = kgsl_runtime_resume,
};
EXPORT_SYMBOL(kgsl_pm_ops);

int kgsl_suspend_driver(struct platform_device *pdev,
					pm_message_t state)
{
	struct kgsl_device *device = dev_get_drvdata(&pdev->dev);

	return kgsl_suspend_device(device, state);
}
EXPORT_SYMBOL(kgsl_suspend_driver);

int kgsl_resume_driver(struct platform_device *pdev)
{
	struct kgsl_device *device = dev_get_drvdata(&pdev->dev);

	return kgsl_resume_device(device);
}
EXPORT_SYMBOL(kgsl_resume_driver);

/**
 * kgsl_destroy_process_private() - Cleanup function to free process private
 * @kref: - Pointer to object being destroyed's kref struct
 * Free struct object and all other resources attached to it.
 * Since the function can be used when not all resources inside process
 * private have been allocated, there is a check to (before each resource
 * cleanup) see if the struct member being cleaned is in fact allocated or not.
 * If the value is not NULL, resource is freed.
 */
static void kgsl_destroy_process_private(struct kref *kref)
{
	struct kgsl_process_private *private = container_of(kref,
			struct kgsl_process_private, refcount);

	put_pid(private->pid);
	idr_destroy(&private->mem_idr);
	idr_destroy(&private->syncsource_idr);

	/* When using global pagetables, do not detach global pagetable */
	if (private->pagetable->name != KGSL_MMU_GLOBAL_PT)
		kgsl_mmu_putpagetable(private->pagetable);

	kfree(private);
}

void
kgsl_process_private_put(struct kgsl_process_private *private)
{
	if (private)
		kref_put(&private->refcount, kgsl_destroy_process_private);
}

/**
 * kgsl_process_private_find() - Find the process associated with the specified
 * name
 * @name: pid_t of the process to search for
 * Return the process struct for the given ID.
 */
struct kgsl_process_private *kgsl_process_private_find(pid_t pid)
{
	struct kgsl_process_private *p, *private = NULL;

	spin_lock(&kgsl_driver.proclist_lock);
	list_for_each_entry(p, &kgsl_driver.process_list, list) {
		if (pid_nr(p->pid) == pid) {
			if (kgsl_process_private_get(p))
				private = p;
			break;
		}
	}
	spin_unlock(&kgsl_driver.proclist_lock);

	return private;
}

#if defined(CONFIG_DISPLAY_SAMSUNG)
extern void kgsl_svm_addr_hole_log(struct kgsl_device *device, pid_t pid, uint64_t memflags);

#define KGSL_PRCO_PATH "/sys/kernel/debug/kgsl/proc"
#define KGSL_PROC_PID_MEM_PATH "mem"

void kgsl_svm_addr_mapping_check(pid_t pid, unsigned long fault_addr)
{
	struct kgsl_process_private *private = NULL;
	struct kgsl_mem_entry *entry = NULL;
	struct kgsl_memdesc *m = NULL;
	int id = 0;
	int mapped = 0;

	private = kgsl_process_private_find(pid);
	if (IS_ERR_OR_NULL(private)) {
		pr_err("%s : smmu fault pid killed\n", __func__);
		return;
	}

	spin_lock(&private->mem_lock);
	for (entry = idr_get_next(&private->mem_idr, &id); entry;
		id++, entry = idr_get_next(&private->mem_idr, &id)) {
		m = &entry->memdesc;

		if ((fault_addr >= m->gpuaddr) &&
			(fault_addr < (m->gpuaddr + m->size))) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			pr_err("%s pid : %d fault_addr : 0x%lx m->gpuaddr : 0x%llx m->size : 0x%llx\n", __func__, pid, fault_addr,
					m->gpuaddr, m->size);
#endif
			mapped = 1;
			break;
		}
	}
	spin_unlock(&private->mem_lock);

	kgsl_process_private_put(private);

#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	pr_err("%s pid : %d mapped : %d\n", __func__, pid, mapped);
#else
	pr_err("%s pid : %d fault_addr : 0x%lx mapped : %d\n", __func__, pid, fault_addr, mapped);
#endif
}

#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
void kgsl_svm_addr_mapping_log(struct kgsl_device *device, pid_t pid)
{
	pr_debug("%s : nothing to do\n", __func__);
}
#else
static void kgsl_svm_addr_log_print(struct kgsl_process_private *private)
{
	struct kgsl_mem_entry *entry = NULL;
	struct kgsl_memdesc *m = NULL;
	char usage[16];
	int id = 0;

	pr_err("%s : %16s %16s %16s %5s %16s\n", __func__,
			"gpuaddr", "useraddr", "size", "id", "usage");

	spin_lock(&private->mem_lock);

	for (entry = idr_get_next(&private->mem_idr, &id); entry;
		id++, entry = idr_get_next(&private->mem_idr, &id)) {
		m = &entry->memdesc;
		kgsl_get_memory_usage(usage, sizeof(usage), m->flags);

		pr_err("%s : %p %p %16llu %5d %16s\n", __func__,
			(uint64_t *)(uintptr_t) m->gpuaddr, 0,
			m->size, entry->id, usage);
	}

	spin_unlock(&private->mem_lock);
}

void kgsl_svm_addr_mapping_log(struct kgsl_device *device, pid_t pid)
{
	struct file *fp;
	mm_segment_t old_fs;
	long nread;
	long buf_index, start_index, print_size;
	char *buf = NULL;
	char *print_buf = NULL;

	char dir_path[SZ_64] = {0, };

	struct kgsl_process_private *private = NULL;

	static DEFINE_RATELIMIT_STATE(_rs,
					DEFAULT_RATELIMIT_INTERVAL,
					DEFAULT_RATELIMIT_BURST);

	private = kgsl_process_private_find(pid);
	if (IS_ERR_OR_NULL(private)) {
		pr_err("%s : smmu fault pid killed\n", __func__);
		return;
	}

	buf = kmalloc(SZ_4K, GFP_KERNEL);
	if (IS_ERR_OR_NULL(buf)) {
		kgsl_process_private_put(private);
		pr_err("%s : buf allocation fail SZ_4K\n", __func__);
		return;
	}

	print_buf = kmalloc(SZ_256, GFP_KERNEL);
	if (IS_ERR_OR_NULL(print_buf)) {
		kfree(buf);
		kgsl_process_private_put(private);
		pr_err("%s : buf allocation fail SZ_256\n", __func__);
		return;
	}

	old_fs = get_fs();
	set_fs(get_ds());

	sprintf(dir_path, "%s/%d/%s", KGSL_PRCO_PATH, private->pid, KGSL_PROC_PID_MEM_PATH);

	fp = filp_open(dir_path, O_RDONLY, 0444);
	if (IS_ERR(fp)) {
		if (__ratelimit(&_rs)) {
			pr_err("%s %s open fail err : %ld\n", __func__, dir_path, PTR_ERR(fp));
			kgsl_svm_addr_log_print(private);
		}
		goto end;
	}

	pr_err("%s : %s \n", __func__, dir_path);

	nread = vfs_read(fp, (char __user *)buf, SZ_2K + SZ_1K, &fp->f_pos);
	while (nread > 0) {
		for (start_index = buf_index = 0; buf_index < nread;buf_index++) {
			/* 0x0A means LF(line feed) */
			if (buf[buf_index] == 0x0A) {
				print_size = buf_index - start_index;
				memcpy(print_buf, buf + start_index, print_size);
				start_index = buf_index + 1;
				print_buf[print_size] = '\0';

				pr_err("%s : %s \n", __func__, print_buf);

			}
		}

		print_size = buf_index - start_index;
		memcpy(print_buf, buf + start_index, print_size);
		print_buf[print_size] = '\0';

		pr_err("%s : %s \n", __func__, print_buf);

		nread = vfs_read(fp, (char __user *)buf, SZ_2K + SZ_1K, &fp->f_pos);
	}

	filp_close(fp, current->files);

end:
	set_fs(old_fs);

	kfree(buf);
	kfree(print_buf);
	kgsl_process_private_put(private);
}
#endif
#endif

static struct kgsl_process_private *kgsl_process_private_new(
		struct kgsl_device *device)
{
	struct kgsl_process_private *private;
	struct pid *cur_pid = get_task_pid(current->group_leader, PIDTYPE_PID);

	/*
	 * Flush mem_workqueue to make sure that any lingering
	 * structs (process pagetable etc) are released before
	 * starting over again.
	 */
	flush_workqueue(kgsl_driver.mem_workqueue);

	/* Search in the process list */
	list_for_each_entry(private, &kgsl_driver.process_list, list) {
		if (private->pid == cur_pid) {
			if (!kgsl_process_private_get(private)) {
				private = ERR_PTR(-EINVAL);
			}
			/*
			 * We need to hold only one reference to the PID for
			 * each process struct to avoid overflowing the
			 * reference counter which can lead to use-after-free.
			 */
			put_pid(cur_pid);
			return private;
		}
	}

	/* Create a new object */
	private = kzalloc(sizeof(struct kgsl_process_private), GFP_KERNEL);
	if (private == NULL) {
		put_pid(cur_pid);
		return ERR_PTR(-ENOMEM);
	}

	kref_init(&private->refcount);

	private->pid = cur_pid;
	get_task_comm(private->comm, current->group_leader);

	spin_lock_init(&private->mem_lock);
	spin_lock_init(&private->syncsource_lock);
	spin_lock_init(&private->ctxt_count_lock);

	idr_init(&private->mem_idr);
	idr_init(&private->syncsource_idr);

	kgsl_reclaim_proc_private_init(private);

	/* Allocate a pagetable for the new process object */
	private->pagetable = kgsl_mmu_getpagetable(&device->mmu,
							pid_nr(cur_pid));
	if (IS_ERR(private->pagetable)) {
		int err = PTR_ERR(private->pagetable);

		idr_destroy(&private->mem_idr);
		idr_destroy(&private->syncsource_idr);
		put_pid(private->pid);

		kfree(private);
		private = ERR_PTR(err);
	}

	return private;
}

static void process_release_memory(struct kgsl_process_private *private)
{
	struct kgsl_mem_entry *entry;
	int next = 0;

	while (1) {
		spin_lock(&private->mem_lock);
		entry = idr_get_next(&private->mem_idr, &next);
		if (entry == NULL) {
			spin_unlock(&private->mem_lock);
			break;
		}
		/*
		 * If the free pending flag is not set it means that user space
		 * did not free it's reference to this entry, in that case
		 * free a reference to this entry, other references are from
		 * within kgsl so they will be freed eventually by kgsl
		 */
		if (!entry->pending_free) {
			entry->pending_free = 1;
			spin_unlock(&private->mem_lock);
			kgsl_mem_entry_put_deferred(entry);
		} else {
			spin_unlock(&private->mem_lock);
		}
		next = next + 1;
	}
}

static void kgsl_process_private_close(struct kgsl_device_private *dev_priv,
		struct kgsl_process_private *private)
{
	mutex_lock(&kgsl_driver.process_mutex);

	if (--private->fd_count > 0) {
		mutex_unlock(&kgsl_driver.process_mutex);
		kgsl_process_private_put(private);
		return;
	}

	/*
	 * If this is the last file on the process take down the debug
	 * directories and garbage collect any outstanding resources
	 */

	kgsl_process_uninit_sysfs(private);

	/* Release all syncsource objects from process private */
	kgsl_syncsource_process_release_syncsources(private);

	/* When using global pagetables, do not detach global pagetable */
	if (private->pagetable->name != KGSL_MMU_GLOBAL_PT)
		kgsl_mmu_detach_pagetable(private->pagetable);

	/* Remove the process struct from the master list */
	spin_lock(&kgsl_driver.proclist_lock);
	list_del(&private->list);
	spin_unlock(&kgsl_driver.proclist_lock);

	/*
	 * Unlock the mutex before releasing the memory and the debugfs
	 * nodes - this prevents deadlocks with the IOMMU and debugfs
	 * locks.
	 */
	mutex_unlock(&kgsl_driver.process_mutex);

	process_release_memory(private);
	debugfs_remove_recursive(private->debug_root);

	kgsl_process_private_put(private);
}


static struct kgsl_process_private *kgsl_process_private_open(
		struct kgsl_device *device)
{
	struct kgsl_process_private *private;

	mutex_lock(&kgsl_driver.process_mutex);
	private = kgsl_process_private_new(device);

	if (IS_ERR(private))
		goto done;

	/*
	 * If this is a new process create the debug directories and add it to
	 * the process list
	 */

	if (private->fd_count++ == 0) {
		kgsl_process_init_sysfs(device, private);
		kgsl_process_init_debugfs(private);

		spin_lock(&kgsl_driver.proclist_lock);
		list_add(&private->list, &kgsl_driver.process_list);
		spin_unlock(&kgsl_driver.proclist_lock);
	}

done:
	mutex_unlock(&kgsl_driver.process_mutex);
	return private;
}

static int kgsl_close_device(struct kgsl_device *device)
{
	int result = 0;

	mutex_lock(&device->mutex);
	device->open_count--;
	if (device->open_count == 0) {

		/*
		 * Wait up to 1 second for the active count to go low
		 * and then start complaining about it
		 */
		if (kgsl_active_count_wait(device, 0)) {
			dev_err(device->dev,
				"Waiting for the active count to become 0\n");

			while (kgsl_active_count_wait(device, 0))
				dev_err(device->dev,
					"Still waiting for the active count\n");
		}

		result = kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);
	}
	mutex_unlock(&device->mutex);
	return result;

}

static void device_release_contexts(struct kgsl_device_private *dev_priv)
{
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	int next = 0;
	int result = 0;

	while (1) {
		read_lock(&device->context_lock);
		context = idr_get_next(&device->context_idr, &next);

		if (context == NULL) {
			read_unlock(&device->context_lock);
			break;
		} else if (context->dev_priv == dev_priv) {
			/*
			 * Hold a reference to the context in case somebody
			 * tries to put it while we are detaching
			 */
			result = _kgsl_context_get(context);
		}
		read_unlock(&device->context_lock);

		if (result) {
			kgsl_context_detach(context);
			kgsl_context_put(context);
			result = 0;
		}

		next = next + 1;
	}
}

static int kgsl_release(struct inode *inodep, struct file *filep)
{
	struct kgsl_device_private *dev_priv = filep->private_data;
	struct kgsl_device *device = dev_priv->device;
	int result;

	filep->private_data = NULL;

	/* Release the contexts for the file */
	device_release_contexts(dev_priv);

	/* Close down the process wide resources for the file */
	kgsl_process_private_close(dev_priv, dev_priv->process_priv);

	/* Destroy the device-specific structure */
	device->ftbl->device_private_destroy(dev_priv);

	result = kgsl_close_device(device);
	pm_runtime_put(&device->pdev->dev);

	return result;
}

static int kgsl_open_device(struct kgsl_device *device)
{
	int result = 0;

	mutex_lock(&device->mutex);
	if (device->open_count == 0) {
		/*
		 * active_cnt special case: we are starting up for the first
		 * time, so use this sequence instead of the kgsl_pwrctrl_wake()
		 * which will be called by kgsl_active_count_get().
		 */
		atomic_inc(&device->active_cnt);
		kgsl_sharedmem_set(device, &device->memstore, 0, 0,
				device->memstore.size);

		result = device->ftbl->init(device);
		if (result)
			goto err;

		result = device->ftbl->start(device, 0);
		if (result)
			goto err;
		/*
		 * Make sure the gates are open, so they don't block until
		 * we start suspend or FT.
		 */
		complete_all(&device->hwaccess_gate);
		kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);
		kgsl_active_count_put(device);
	}
	device->open_count++;
err:
	if (result) {
		kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);
		atomic_dec(&device->active_cnt);
	}

	mutex_unlock(&device->mutex);
	return result;
}

static int kgsl_open(struct inode *inodep, struct file *filep)
{
	int result;
	struct kgsl_device_private *dev_priv;
	struct kgsl_device *device;
	unsigned int minor = iminor(inodep);

	device = kgsl_get_minor(minor);
	if (device == NULL) {
		pr_err("kgsl: No device found\n");
		return -ENODEV;
	}

	result = pm_runtime_get_sync(&device->pdev->dev);
	if (result < 0) {
		dev_err(device->dev,
			     "Runtime PM: Unable to wake up the device, rc = %d\n",
			     result);
		return result;
	}
	result = 0;

	dev_priv = device->ftbl->device_private_create();
	if (dev_priv == NULL) {
		result = -ENOMEM;
		goto err;
	}

	dev_priv->device = device;
	filep->private_data = dev_priv;

	result = kgsl_open_device(device);
	if (result)
		goto err;

	/*
	 * Get file (per process) private struct. This must be done
	 * after the first start so that the global pagetable mappings
	 * are set up before we create the per-process pagetable.
	 */
	dev_priv->process_priv = kgsl_process_private_open(device);
	if (IS_ERR(dev_priv->process_priv)) {
		result = PTR_ERR(dev_priv->process_priv);
		kgsl_close_device(device);
		goto err;
	}

err:
	if (result) {
		filep->private_data = NULL;
		kfree(dev_priv);
		pm_runtime_put(&device->pdev->dev);
	}
	return result;
}

#define GPUADDR_IN_MEMDESC(_val, _memdesc) \
	(((_val) >= (_memdesc)->gpuaddr) && \
	 ((_val) < ((_memdesc)->gpuaddr + (_memdesc)->size)))

/**
 * kgsl_sharedmem_find() - Find a gpu memory allocation
 *
 * @private: private data for the process to check.
 * @gpuaddr: start address of the region
 *
 * Find a gpu allocation. Caller must kgsl_mem_entry_put()
 * the returned entry when finished using it.
 */
struct kgsl_mem_entry * __must_check
kgsl_sharedmem_find(struct kgsl_process_private *private, uint64_t gpuaddr)
{
	int ret = 0, id;
	struct kgsl_mem_entry *entry = NULL;

	if (!private)
		return NULL;

	if (!kgsl_mmu_gpuaddr_in_range(private->pagetable, gpuaddr))
		return NULL;

	spin_lock(&private->mem_lock);
	idr_for_each_entry(&private->mem_idr, entry, id) {
		if (GPUADDR_IN_MEMDESC(gpuaddr, &entry->memdesc)) {
			if (!entry->pending_free)
				ret = kgsl_mem_entry_get(entry);
			break;
		}
	}
	spin_unlock(&private->mem_lock);

	return (ret == 0) ? NULL : entry;
}
EXPORT_SYMBOL(kgsl_sharedmem_find);

struct kgsl_mem_entry * __must_check
kgsl_sharedmem_find_id_flags(struct kgsl_process_private *process,
		unsigned int id, uint64_t flags)
{
	int count = 0;
	struct kgsl_mem_entry *entry;

	spin_lock(&process->mem_lock);
	entry = idr_find(&process->mem_idr, id);
	if (entry)
		if (!entry->pending_free &&
				(flags & entry->memdesc.flags) == flags)
			count = kgsl_mem_entry_get(entry);
	spin_unlock(&process->mem_lock);

	return (count == 0) ? NULL : entry;
}

/**
 * kgsl_sharedmem_find_id() - find a memory entry by id
 * @process: the owning process
 * @id: id to find
 *
 * @returns - the mem_entry or NULL
 *
 * Caller must kgsl_mem_entry_put() the returned entry, when finished using
 * it.
 */
struct kgsl_mem_entry * __must_check
kgsl_sharedmem_find_id(struct kgsl_process_private *process, unsigned int id)
{
	return kgsl_sharedmem_find_id_flags(process, id, 0);
}

/**
 * kgsl_mem_entry_unset_pend() - Unset the pending free flag of an entry
 * @entry - The memory entry
 */
static inline void kgsl_mem_entry_unset_pend(struct kgsl_mem_entry *entry)
{
	if (entry == NULL)
		return;
	spin_lock(&entry->priv->mem_lock);
	entry->pending_free = 0;
	spin_unlock(&entry->priv->mem_lock);
}

struct msm_bus_scale_pdata *kgsl_get_bus_scale_table(struct kgsl_device *device)
{
	struct device_node *child = NULL, *parent;
	char str[24];

	parent = device->pdev->dev.of_node;

	snprintf(str, sizeof(str), "qcom,gpu-bus-table-ddr%d",
		of_fdt_get_ddrtype());

	child = of_find_compatible_node(parent, NULL, str);

	/* Go with the first bus table node */
	if (child == NULL)
		child = of_find_compatible_node(parent, NULL,
			"qcom,gpu-bus-table");

	if (child) {
		struct msm_bus_scale_pdata *data = msm_bus_pdata_from_node(
					device->pdev, child);
		of_node_put(child);
		return data;
	}

	return msm_bus_cl_get_pdata(device->pdev);
}

/**
 * kgsl_mem_entry_set_pend() - Set the pending free flag of a memory entry
 * @entry - The memory entry
 *
 * @returns - true if pending flag was 0 else false
 *
 * This function will set the pending free flag if it is previously unset. Used
 * to prevent race condition between ioctls calling free/freememontimestamp
 * on the same entry. Whichever thread set's the flag first will do the free.
 */
static inline bool kgsl_mem_entry_set_pend(struct kgsl_mem_entry *entry)
{
	bool ret = false;

	if (entry == NULL)
		return false;

	spin_lock(&entry->priv->mem_lock);
	if (!entry->pending_free) {
		entry->pending_free = 1;
		ret = true;
	}
	spin_unlock(&entry->priv->mem_lock);
	return ret;
}

static int kgsl_get_ctxt_fault_stats(struct kgsl_context *context,
		struct kgsl_context_property *ctxt_property)
{
	struct kgsl_context_property_fault fault_stats;
	size_t copy;

	/* Return the size of the subtype struct */
	if (ctxt_property->size == 0) {
		ctxt_property->size = sizeof(fault_stats);
		return 0;
	}

	memset(&fault_stats, 0, sizeof(fault_stats));

	copy = min_t(size_t, ctxt_property->size, sizeof(fault_stats));

	fault_stats.faults = context->total_fault_count;
	fault_stats.timestamp = context->last_faulted_cmd_ts;

	/*
	 * Copy the context fault stats to data which also serves as
	 * the out parameter.
	 */
	if (copy_to_user(u64_to_user_ptr(ctxt_property->data),
				&fault_stats, copy))
		return -EFAULT;

	return 0;
}

static long kgsl_get_ctxt_properties(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param)
{
	/* Return fault stats of given context */
	struct kgsl_context_property ctxt_property;
	struct kgsl_context *context;
	size_t copy;
	long ret;

	/*
	 * If sizebytes is zero, tell the user how big the
	 * ctxt_property struct should be.
	 */
	if (param->sizebytes == 0) {
		param->sizebytes = sizeof(ctxt_property);
		return 0;
	}

	memset(&ctxt_property, 0, sizeof(ctxt_property));

	copy = min_t(size_t, param->sizebytes, sizeof(ctxt_property));

	/* We expect the value passed in to contain the context id */
	if (copy_from_user(&ctxt_property, param->value, copy))
		return -EFAULT;

	/* ctxt type zero is not valid, as we consider it as uninitialized. */
	if (ctxt_property.type == 0)
		return -EINVAL;

	context = kgsl_context_get_owner(dev_priv,
			ctxt_property.contextid);
	if (!context)
		return -EINVAL;

	if (ctxt_property.type == KGSL_CONTEXT_PROP_FAULTS)
		ret = kgsl_get_ctxt_fault_stats(context, &ctxt_property);
	else
		ret = -EOPNOTSUPP;

	kgsl_context_put(context);

	return ret;
}

static long kgsl_prop_version(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param)
{
	struct kgsl_version version = {
		.drv_major = KGSL_VERSION_MAJOR,
		.drv_minor = KGSL_VERSION_MINOR,
		.dev_major = 3,
		.dev_minor = 1,
	};

	if (param->sizebytes != sizeof(version))
		return -EINVAL;

	if (copy_to_user(param->value, &version, sizeof(version)))
		return -EFAULT;

	return 0;
}

/* Return reset status of given context and clear it */
static long kgsl_prop_gpu_reset_stat(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param)
{
	u32 id;
	struct kgsl_context *context;

	if (param->sizebytes != sizeof(id))
		return -EINVAL;

	/* We expect the value passed in to contain the context id */
	if (copy_from_user(&id, param->value, sizeof(id)))
		return -EFAULT;

	context = kgsl_context_get_owner(dev_priv, id);
	if (!context)
		return -EINVAL;

	/*
	 * Copy the reset status to value which also serves as
	 * the out parameter
	 */
	id = context->reset_status;

	context->reset_status = KGSL_CTX_STAT_NO_ERROR;
	kgsl_context_put(context);

	if (copy_to_user(param->value, &id, sizeof(id)))
		return -EFAULT;

	return 0;
}

static long kgsl_prop_secure_buf_alignment(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param)
{
	u32 align;

	if (param->sizebytes != sizeof(align))
		return -EINVAL;

	/*
	 * XPUv2 impose the constraint of 1MB memory alignment,
	 * on the other hand Hypervisor does not have such
	 * constraints. So driver should fulfill such
	 * requirements when allocating secure memory.
	 */
	align = MMU_FEATURE(&dev_priv->device->mmu,
			KGSL_MMU_HYP_SECURE_ALLOC) ? PAGE_SIZE : SZ_1M;

	if (copy_to_user(param->value, &align, sizeof(align)))
		return -EFAULT;

	return 0;
}

static long kgsl_prop_secure_ctxt_support(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param)
{
	u32 secure;

	if (param->sizebytes != sizeof(secure))
		return -EINVAL;

	secure = dev_priv->device->mmu.secured ? 1 : 0;

	if (copy_to_user(param->value, &secure, sizeof(secure)))
		return -EFAULT;

	return 0;
}

static int kgsl_query_caps_properties(struct kgsl_device *device,
		struct kgsl_capabilities *caps)
{
	struct kgsl_capabilities_properties props;
	size_t copy;
	u32 count, *local;
	int ret;

	/* Return the size of the subtype struct */
	if (caps->size == 0) {
		caps->size = sizeof(props);
		return 0;
	}

	memset(&props, 0, sizeof(props));

	copy = min_t(size_t, caps->size, sizeof(props));

	if (copy_from_user(&props, u64_to_user_ptr(caps->data), copy))
		return -EFAULT;

	/* Get the number of properties */
	count = kgsl_query_property_list(device, NULL, 0);

	/*
	 * If the incoming user count is zero, they are querying the number of
	 * available properties. Set it and return.
	 */
	if (props.count == 0) {
		props.count = count;
		goto done;
	}

	/* Copy the lesser of the user or kernel property count */
	if (props.count < count)
		count = props.count;

	/* Create a local buffer to store the property list */
	local = kcalloc(count, sizeof(u32), GFP_KERNEL);
	if (!local)
		return -ENOMEM;

	/* Get the properties */
	props.count = kgsl_query_property_list(device, local, count);

	ret = copy_to_user(u64_to_user_ptr(props.list), local,
		props.count * sizeof(u32));

	kfree(local);

	if (ret)
		return -EFAULT;

done:
	if (copy_to_user(u64_to_user_ptr(caps->data), &props, copy))
		return -EFAULT;

	return 0;
}

static long kgsl_prop_query_capabilities(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param)
{
	struct kgsl_capabilities caps;
	long ret;
	size_t copy;

	/*
	 * If sizebytes is zero, tell the user how big the capabilities struct
	 * should be
	 */
	if (param->sizebytes == 0) {
		param->sizebytes = sizeof(caps);
		return 0;
	}

	memset(&caps, 0, sizeof(caps));

	copy = min_t(size_t, param->sizebytes, sizeof(caps));

	if (copy_from_user(&caps, param->value, copy))
		return -EFAULT;

	/* querytype must be non zero */
	if (caps.querytype == 0)
		return -EINVAL;

	if (caps.querytype == KGSL_QUERY_CAPS_PROPERTIES)
		ret = kgsl_query_caps_properties(dev_priv->device, &caps);
	else {
		/* Unsupported querytypes should return a unique return value */
		return -EOPNOTSUPP;
	}

	if (copy_to_user(param->value, &caps, copy))
		return -EFAULT;

	return ret;
}

static const struct {
	int type;
	long (*func)(struct kgsl_device_private *dev_priv,
		struct kgsl_device_getproperty *param);
} kgsl_property_funcs[] = {
	{ KGSL_PROP_VERSION, kgsl_prop_version },
	{ KGSL_PROP_GPU_RESET_STAT, kgsl_prop_gpu_reset_stat},
	{ KGSL_PROP_SECURE_BUFFER_ALIGNMENT, kgsl_prop_secure_buf_alignment },
	{ KGSL_PROP_SECURE_CTXT_SUPPORT, kgsl_prop_secure_ctxt_support },
	{ KGSL_PROP_QUERY_CAPABILITIES, kgsl_prop_query_capabilities },
	{ KGSL_PROP_CONTEXT_PROPERTY, kgsl_get_ctxt_properties },
};

/*call all ioctl sub functions with driver locked*/
long kgsl_ioctl_device_getproperty(struct kgsl_device_private *dev_priv,
					  unsigned int cmd, void *data)
{
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_device_getproperty *param = data;
	int i;

	for (i = 0; i < ARRAY_SIZE(kgsl_property_funcs); i++) {
		if (param->type == kgsl_property_funcs[i].type)
			return kgsl_property_funcs[i].func(dev_priv, param);
	}

	if (is_compat_task())
		return device->ftbl->getproperty_compat(device, param);

	return device->ftbl->getproperty(device, param);
}

int kgsl_query_property_list(struct kgsl_device *device, u32 *list, u32 count)
{
	int num = 0;

	if (!list) {
		num = ARRAY_SIZE(kgsl_property_funcs);

		if (device->ftbl->query_property_list)
			num += device->ftbl->query_property_list(device, list,
				count);

		return num;
	}

	for (; num < count && num < ARRAY_SIZE(kgsl_property_funcs); num++)
		list[num] = kgsl_property_funcs[num].type;

	if (device->ftbl->query_property_list)
		num += device->ftbl->query_property_list(device, &list[num],
			count - num);

	return num;
}

long kgsl_ioctl_device_setproperty(struct kgsl_device_private *dev_priv,
					  unsigned int cmd, void *data)
{
	int result = 0;
	/* The getproperty struct is reused for setproperty too */
	struct kgsl_device_getproperty *param = data;

	/* Reroute to compat version if coming from compat_ioctl */
	if (is_compat_task())
		result = dev_priv->device->ftbl->setproperty_compat(
			dev_priv, param->type, param->value,
			param->sizebytes);
	else if (dev_priv->device->ftbl->setproperty)
		result = dev_priv->device->ftbl->setproperty(
			dev_priv, param->type, param->value,
			param->sizebytes);

	return result;
}

long kgsl_ioctl_device_waittimestamp_ctxtid(
		struct kgsl_device_private *dev_priv, unsigned int cmd,
		void *data)
{
	struct kgsl_device_waittimestamp_ctxtid *param = data;
	struct kgsl_device *device = dev_priv->device;
	long result = -EINVAL;
	unsigned int temp_cur_ts = 0;
	struct kgsl_context *context;

	context = kgsl_context_get_owner(dev_priv, param->context_id);
	if (context == NULL)
		return result;

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED,
		&temp_cur_ts);

	trace_kgsl_waittimestamp_entry(device, context->id, temp_cur_ts,
		param->timestamp, param->timeout);

	result = device->ftbl->waittimestamp(device, context, param->timestamp,
		param->timeout);

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED,
		&temp_cur_ts);
	trace_kgsl_waittimestamp_exit(device, temp_cur_ts, result);

	kgsl_context_put(context);

	return result;
}

static inline bool _check_context_is_sparse(struct kgsl_context *context,
			uint64_t flags)
{
	if ((context->flags & KGSL_CONTEXT_SPARSE) ||
		(flags & KGSL_DRAWOBJ_SPARSE))
		return true;

	return false;
}


long kgsl_ioctl_rb_issueibcmds(struct kgsl_device_private *dev_priv,
				      unsigned int cmd, void *data)
{
	struct kgsl_ringbuffer_issueibcmds *param = data;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	struct kgsl_drawobj *drawobj;
	struct kgsl_drawobj_cmd *cmdobj;
	long result = -EINVAL;

	/* The legacy functions don't support synchronization commands */
	if ((param->flags & (KGSL_DRAWOBJ_SYNC | KGSL_DRAWOBJ_MARKER)))
		return -EINVAL;

	/* Sanity check the number of IBs */
	if (param->flags & KGSL_DRAWOBJ_SUBMIT_IB_LIST &&
			(param->numibs == 0 || param->numibs > KGSL_MAX_NUMIBS))
		return -EINVAL;

	/* Get the context */
	context = kgsl_context_get_owner(dev_priv, param->drawctxt_id);
	if (context == NULL)
		return -EINVAL;

	if (_check_context_is_sparse(context, param->flags)) {
		kgsl_context_put(context);
		return -EINVAL;
	}

	cmdobj = kgsl_drawobj_cmd_create(device, context, param->flags,
					CMDOBJ_TYPE);
	if (IS_ERR(cmdobj)) {
		kgsl_context_put(context);
		return PTR_ERR(cmdobj);
	}

	drawobj = DRAWOBJ(cmdobj);

	if (param->flags & KGSL_DRAWOBJ_SUBMIT_IB_LIST)
		result = kgsl_drawobj_cmd_add_ibdesc_list(device, cmdobj,
			(void __user *) param->ibdesc_addr,
			param->numibs);
	else {
		struct kgsl_ibdesc ibdesc;
		/* Ultra legacy path */

		ibdesc.gpuaddr = param->ibdesc_addr;
		ibdesc.sizedwords = param->numibs;
		ibdesc.ctrl = 0;

		result = kgsl_drawobj_cmd_add_ibdesc(device, cmdobj, &ibdesc);
	}

	if (result == 0)
		result = kgsl_reclaim_to_pinned_state(dev_priv->process_priv);

	if (result == 0)
		result = dev_priv->device->ftbl->queue_cmds(dev_priv, context,
				&drawobj, 1, &param->timestamp);

	/*
	 * -EPROTO is a "success" error - it just tells the user that the
	 * context had previously faulted
	 */
	if (result && result != -EPROTO)
		kgsl_drawobj_destroy(drawobj);

	kgsl_context_put(context);
	return result;
}

/* Returns 0 on failure.  Returns command type(s) on success */
static unsigned int _process_command_input(struct kgsl_device *device,
		unsigned int flags, unsigned int numcmds,
		unsigned int numobjs, unsigned int numsyncs)
{
	if (numcmds > KGSL_MAX_NUMIBS ||
			numobjs > KGSL_MAX_NUMIBS ||
			numsyncs > KGSL_MAX_SYNCPOINTS)
		return 0;

	/*
	 * The SYNC bit is supposed to identify a dummy sync object
	 * so warn the user if they specified any IBs with it.
	 * A MARKER command can either have IBs or not but if the
	 * command has 0 IBs it is automatically assumed to be a marker.
	 */

	/* If they specify the flag, go with what they say */
	if (flags & KGSL_DRAWOBJ_MARKER)
		return MARKEROBJ_TYPE;
	else if (flags & KGSL_DRAWOBJ_SYNC)
		return SYNCOBJ_TYPE;

	/* If not, deduce what they meant */
	if (numsyncs && numcmds)
		return SYNCOBJ_TYPE | CMDOBJ_TYPE;
	else if (numsyncs)
		return SYNCOBJ_TYPE;
	else if (numcmds)
		return CMDOBJ_TYPE;
	else if (numcmds == 0)
		return MARKEROBJ_TYPE;

	return 0;
}

long kgsl_ioctl_submit_commands(struct kgsl_device_private *dev_priv,
				      unsigned int cmd, void *data)
{
	struct kgsl_submit_commands *param = data;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	struct kgsl_drawobj *drawobj[2];
	unsigned int type;
	long result;
	unsigned int i = 0;

	type = _process_command_input(device, param->flags, param->numcmds, 0,
			param->numsyncs);
	if (!type)
		return -EINVAL;

	context = kgsl_context_get_owner(dev_priv, param->context_id);
	if (context == NULL)
		return -EINVAL;

	if (_check_context_is_sparse(context, param->flags)) {
		kgsl_context_put(context);
		return -EINVAL;
	}

	if (type & SYNCOBJ_TYPE) {
		struct kgsl_drawobj_sync *syncobj =
				kgsl_drawobj_sync_create(device, context);
		if (IS_ERR(syncobj)) {
			result = PTR_ERR(syncobj);
			goto done;
		}

		drawobj[i++] = DRAWOBJ(syncobj);

		result = kgsl_drawobj_sync_add_syncpoints(device, syncobj,
				param->synclist, param->numsyncs);
		if (result)
			goto done;
	}

	if (type & (CMDOBJ_TYPE | MARKEROBJ_TYPE)) {
		struct kgsl_drawobj_cmd *cmdobj =
				kgsl_drawobj_cmd_create(device,
					context, param->flags, type);
		if (IS_ERR(cmdobj)) {
			result = PTR_ERR(cmdobj);
			goto done;
		}

		drawobj[i++] = DRAWOBJ(cmdobj);

		result = kgsl_drawobj_cmd_add_ibdesc_list(device, cmdobj,
				param->cmdlist, param->numcmds);
		if (result)
			goto done;

		/* If no profiling buffer was specified, clear the flag */
		if (cmdobj->profiling_buf_entry == NULL)
			DRAWOBJ(cmdobj)->flags &=
				~(unsigned long)KGSL_DRAWOBJ_PROFILING;

		if (type & CMDOBJ_TYPE) {
			result = kgsl_reclaim_to_pinned_state(
					dev_priv->process_priv);
			if (result)
				goto done;
		}
	}

	result = device->ftbl->queue_cmds(dev_priv, context, drawobj,
			i, &param->timestamp);

done:
	/*
	 * -EPROTO is a "success" error - it just tells the user that the
	 * context had previously faulted
	 */
	if (result && result != -EPROTO)
		while (i--)
			kgsl_drawobj_destroy(drawobj[i]);


	kgsl_context_put(context);
	return result;
}

long kgsl_ioctl_gpu_command(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_gpu_command *param = data;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	struct kgsl_drawobj *drawobj[2];
	unsigned int type;
	long result;
	unsigned int i = 0;

	type = _process_command_input(device, param->flags, param->numcmds,
			param->numobjs, param->numsyncs);
	if (!type)
		return -EINVAL;

	context = kgsl_context_get_owner(dev_priv, param->context_id);
	if (context == NULL)
		return -EINVAL;

	if (_check_context_is_sparse(context, param->flags)) {
		kgsl_context_put(context);
		return -EINVAL;
	}

	if (type & SYNCOBJ_TYPE) {
		struct kgsl_drawobj_sync *syncobj =
				kgsl_drawobj_sync_create(device, context);

		if (IS_ERR(syncobj)) {
			result = PTR_ERR(syncobj);
			goto done;
		}

		drawobj[i++] = DRAWOBJ(syncobj);

		result = kgsl_drawobj_sync_add_synclist(device, syncobj,
				to_user_ptr(param->synclist),
				param->syncsize, param->numsyncs);
		if (result)
			goto done;
	}

	if (type & (CMDOBJ_TYPE | MARKEROBJ_TYPE)) {
		struct kgsl_drawobj_cmd *cmdobj =
				kgsl_drawobj_cmd_create(device,
					context, param->flags, type);

		if (IS_ERR(cmdobj)) {
			result = PTR_ERR(cmdobj);
			goto done;
		}

		drawobj[i++] = DRAWOBJ(cmdobj);

		result = kgsl_drawobj_cmd_add_cmdlist(device, cmdobj,
			to_user_ptr(param->cmdlist),
			param->cmdsize, param->numcmds);
		if (result)
			goto done;

		result = kgsl_drawobj_cmd_add_memlist(device, cmdobj,
			to_user_ptr(param->objlist),
			param->objsize, param->numobjs);
		if (result)
			goto done;

		/* If no profiling buffer was specified, clear the flag */
		if (cmdobj->profiling_buf_entry == NULL)
			DRAWOBJ(cmdobj)->flags &=
				~(unsigned long)KGSL_DRAWOBJ_PROFILING;

		if (type & CMDOBJ_TYPE) {
			result = kgsl_reclaim_to_pinned_state(
					dev_priv->process_priv);
			if (result)
				goto done;
		}
	}

	result = device->ftbl->queue_cmds(dev_priv, context, drawobj,
				i, &param->timestamp);

done:
	/*
	 * -EPROTO is a "success" error - it just tells the user that the
	 * context had previously faulted
	 */
	if (result && result != -EPROTO)
		while (i--)
			kgsl_drawobj_destroy(drawobj[i]);

	kgsl_context_put(context);
	return result;
}

long kgsl_ioctl_cmdstream_readtimestamp_ctxtid(struct kgsl_device_private
						*dev_priv, unsigned int cmd,
						void *data)
{
	struct kgsl_cmdstream_readtimestamp_ctxtid *param = data;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	long result = -EINVAL;

	mutex_lock(&device->mutex);
	context = kgsl_context_get_owner(dev_priv, param->context_id);

	if (context) {
		result = kgsl_readtimestamp(device, context,
			param->type, &param->timestamp);

		trace_kgsl_readtimestamp(device, context->id,
			param->type, param->timestamp);
	}

	kgsl_context_put(context);
	mutex_unlock(&device->mutex);
	return result;
}

long kgsl_ioctl_drawctxt_create(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	int result = 0;
	struct kgsl_drawctxt_create *param = data;
	struct kgsl_context *context = NULL;
	struct kgsl_device *device = dev_priv->device;

	context = device->ftbl->drawctxt_create(dev_priv, &param->flags);
	if (IS_ERR(context)) {
		result = PTR_ERR(context);
		goto done;
	}
	trace_kgsl_context_create(dev_priv->device, context, param->flags);

	/* Commit the pointer to the context in context_idr */
	write_lock(&device->context_lock);
	idr_replace(&device->context_idr, context, context->id);
	param->drawctxt_id = context->id;
	write_unlock(&device->context_lock);

done:
	return result;
}

long kgsl_ioctl_drawctxt_destroy(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_drawctxt_destroy *param = data;
	struct kgsl_context *context;

	context = kgsl_context_get_owner(dev_priv, param->drawctxt_id);
	if (context == NULL)
		return -EINVAL;

	kgsl_context_detach(context);
	kgsl_context_put(context);

	return 0;
}

long gpumem_free_entry(struct kgsl_mem_entry *entry)
{
	if (!kgsl_mem_entry_set_pend(entry))
		return -EBUSY;

	trace_kgsl_mem_free(entry);
	kgsl_memfree_add(pid_nr(entry->priv->pid),
			entry->memdesc.pagetable ?
				entry->memdesc.pagetable->name : 0,
			entry->memdesc.gpuaddr, entry->memdesc.size,
			entry->memdesc.flags);

	kgsl_mem_entry_put(entry);

	return 0;
}

static void gpumem_free_func(struct kgsl_device *device,
		struct kgsl_event_group *group, void *priv, int ret)
{
	struct kgsl_context *context = group->context;
	struct kgsl_mem_entry *entry = priv;
	unsigned int timestamp;

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED, &timestamp);

	/* Free the memory for all event types */
	trace_kgsl_mem_timestamp_free(device, entry, KGSL_CONTEXT_ID(context),
		timestamp, 0);
	kgsl_memfree_add(pid_nr(entry->priv->pid),
			entry->memdesc.pagetable ?
				entry->memdesc.pagetable->name : 0,
			entry->memdesc.gpuaddr, entry->memdesc.size,
			entry->memdesc.flags);

	kgsl_mem_entry_put(entry);
}

static long gpumem_free_entry_on_timestamp(struct kgsl_device *device,
		struct kgsl_mem_entry *entry,
		struct kgsl_context *context, unsigned int timestamp)
{
	int ret;
	unsigned int temp;

	if (!kgsl_mem_entry_set_pend(entry))
		return -EBUSY;

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED, &temp);
	trace_kgsl_mem_timestamp_queue(device, entry, context->id, temp,
		timestamp);
	ret = kgsl_add_event(device, &context->events,
		timestamp, gpumem_free_func, entry);

	if (ret)
		kgsl_mem_entry_unset_pend(entry);

	return ret;
}

long kgsl_ioctl_sharedmem_free(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_sharedmem_free *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry;
	long ret;

	entry = kgsl_sharedmem_find(private, (uint64_t) param->gpuaddr);
	if (entry == NULL)
		return -EINVAL;

	ret = gpumem_free_entry(entry);
	kgsl_mem_entry_put_deferred(entry);

	return ret;
}

long kgsl_ioctl_gpumem_free_id(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data)
{
	struct kgsl_gpumem_free_id *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry;
	long ret;

	entry = kgsl_sharedmem_find_id(private, param->id);
	if (entry == NULL)
		return -EINVAL;

	ret = gpumem_free_entry(entry);
	kgsl_mem_entry_put_deferred(entry);

	return ret;
}

static long gpuobj_free_on_timestamp(struct kgsl_device_private *dev_priv,
		struct kgsl_mem_entry *entry, struct kgsl_gpuobj_free *param)
{
	struct kgsl_gpu_event_timestamp event;
	struct kgsl_context *context;
	long ret;

	memset(&event, 0, sizeof(event));

	ret = kgsl_copy_from_user(&event, to_user_ptr(param->priv),
		sizeof(event), param->len);
	if (ret)
		return ret;

	if (event.context_id == 0)
		return -EINVAL;

	context = kgsl_context_get_owner(dev_priv, event.context_id);
	if (context == NULL)
		return -EINVAL;

	ret = gpumem_free_entry_on_timestamp(dev_priv->device, entry, context,
		event.timestamp);

	kgsl_context_put(context);
	return ret;
}

static bool gpuobj_free_fence_func(void *priv)
{
	struct kgsl_mem_entry *entry = priv;

	trace_kgsl_mem_free(entry);
	kgsl_memfree_add(pid_nr(entry->priv->pid),
			entry->memdesc.pagetable ?
				entry->memdesc.pagetable->name : 0,
			entry->memdesc.gpuaddr, entry->memdesc.size,
			entry->memdesc.flags);

	kgsl_mem_entry_put_deferred(entry);
	return true;
}

static long gpuobj_free_on_fence(struct kgsl_device_private *dev_priv,
		struct kgsl_mem_entry *entry, struct kgsl_gpuobj_free *param)
{
	struct kgsl_sync_fence_cb *handle;
	struct kgsl_gpu_event_fence event;
	long ret;

	if (!kgsl_mem_entry_set_pend(entry))
		return -EBUSY;

	memset(&event, 0, sizeof(event));

	ret = kgsl_copy_from_user(&event, to_user_ptr(param->priv),
		sizeof(event), param->len);
	if (ret) {
		kgsl_mem_entry_unset_pend(entry);
		return ret;
	}

	if (event.fd < 0) {
		kgsl_mem_entry_unset_pend(entry);
		return -EINVAL;
	}

	handle = kgsl_sync_fence_async_wait(event.fd,
		gpuobj_free_fence_func, entry, NULL);

	if (IS_ERR(handle)) {
		kgsl_mem_entry_unset_pend(entry);
		return PTR_ERR(handle);
	}

	/* if handle is NULL the fence has already signaled */
	if (handle == NULL)
		gpuobj_free_fence_func(entry);

	return 0;
}

long kgsl_ioctl_gpuobj_free(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_gpuobj_free *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry;
	long ret;

	entry = kgsl_sharedmem_find_id(private, param->id);
	if (entry == NULL)
		return -EINVAL;

	/* If no event is specified then free immediately */
	if (!(param->flags & KGSL_GPUOBJ_FREE_ON_EVENT))
		ret = gpumem_free_entry(entry);
	else if (param->type == KGSL_GPU_EVENT_TIMESTAMP)
		ret = gpuobj_free_on_timestamp(dev_priv, entry, param);
	else if (param->type == KGSL_GPU_EVENT_FENCE)
		ret = gpuobj_free_on_fence(dev_priv, entry, param);
	else
		ret = -EINVAL;

	kgsl_mem_entry_put_deferred(entry);
	return ret;
}

long kgsl_ioctl_cmdstream_freememontimestamp_ctxtid(
		struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_cmdstream_freememontimestamp_ctxtid *param = data;
	struct kgsl_context *context = NULL;
	struct kgsl_mem_entry *entry;
	long ret = -EINVAL;

	if (param->type != KGSL_TIMESTAMP_RETIRED)
		return -EINVAL;

	context = kgsl_context_get_owner(dev_priv, param->context_id);
	if (context == NULL)
		return -EINVAL;

	entry = kgsl_sharedmem_find(dev_priv->process_priv,
		(uint64_t) param->gpuaddr);
	if (entry == NULL) {
		kgsl_context_put(context);
		return -EINVAL;
	}

	ret = gpumem_free_entry_on_timestamp(dev_priv->device, entry,
		context, param->timestamp);

	kgsl_mem_entry_put_deferred(entry);
	kgsl_context_put(context);

	return ret;
}

static int check_vma_flags(struct vm_area_struct *vma,
		unsigned int flags)
{
	unsigned long flags_requested = (VM_READ | VM_WRITE);

	if (flags & KGSL_MEMFLAGS_GPUREADONLY)
		flags_requested &= ~(unsigned long)VM_WRITE;

	if ((vma->vm_flags & flags_requested) == flags_requested)
		return 0;

	return -EFAULT;
}

static int check_vma(unsigned long hostptr, u64 size)
{
	struct vm_area_struct *vma;
	unsigned long cur = hostptr;

	while (cur < (hostptr + size)) {
		vma = find_vma(current->mm, cur);
		if (!vma)
			return false;

		/* Don't remap memory that we already own */
		if (vma->vm_file && vma->vm_ops == &kgsl_gpumem_vm_ops)
			return false;

		cur = vma->vm_end;
	}

	return true;
}

static int memdesc_sg_virt(struct kgsl_memdesc *memdesc, unsigned long useraddr)
{
	int ret = 0;
	long npages = 0, i;
	size_t sglen = (size_t) (memdesc->size / PAGE_SIZE);
	struct page **pages = NULL;
	int write = ((memdesc->flags & KGSL_MEMFLAGS_GPUREADONLY) ? 0 :
								FOLL_WRITE);

	if (sglen == 0 || sglen >= LONG_MAX)
		return -EINVAL;

	pages = kvcalloc(sglen, sizeof(*pages), GFP_KERNEL);
	if (pages == NULL)
		return -ENOMEM;

	memdesc->sgt = kmalloc(sizeof(*memdesc->sgt), GFP_KERNEL);
	if (memdesc->sgt == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	down_read(&current->mm->mmap_sem);
	if (!check_vma(useraddr, memdesc->size)) {
		up_read(&current->mm->mmap_sem);
		ret = -EFAULT;
		goto out;
	}

	npages = get_user_pages(useraddr, sglen, write, pages, NULL);
	up_read(&current->mm->mmap_sem);

	ret = (npages < 0) ? (int)npages : 0;
	if (ret)
		goto out;

	if ((unsigned long) npages != sglen) {
		ret = -EINVAL;
		goto out;
	}

	ret = sg_alloc_table_from_pages(memdesc->sgt, pages, npages,
					0, memdesc->size, GFP_KERNEL);
out:
	if (ret) {
		for (i = 0; i < npages; i++)
			put_page(pages[i]);

		kfree(memdesc->sgt);
		memdesc->sgt = NULL;
	}
	kvfree(pages);
	return ret;
}

static int kgsl_setup_anon_useraddr(struct kgsl_pagetable *pagetable,
	struct kgsl_mem_entry *entry, unsigned long hostptr,
	size_t offset, size_t size)
{
	/* Map an anonymous memory chunk */
	int ret;

	if (size == 0 || offset != 0 ||
		!IS_ALIGNED(size, PAGE_SIZE))
		return -EINVAL;

	entry->memdesc.pagetable = pagetable;
	entry->memdesc.size = (uint64_t) size;
	entry->memdesc.flags |= (uint64_t)KGSL_MEMFLAGS_USERMEM_ADDR;

	if (kgsl_memdesc_use_cpu_map(&entry->memdesc)) {
		/* Register the address in the database */
		ret = kgsl_mmu_set_svm_region(pagetable,
			(uint64_t) hostptr, (uint64_t) size);

		if (ret)
			return ret;

		entry->memdesc.gpuaddr = (uint64_t) hostptr;
	}

	ret = memdesc_sg_virt(&entry->memdesc, hostptr);

	if (ret && kgsl_memdesc_use_cpu_map(&entry->memdesc))
		kgsl_mmu_put_gpuaddr(&entry->memdesc);

	return ret;
}

#ifdef CONFIG_DMA_SHARED_BUFFER
static int match_file(const void *p, struct file *file, unsigned int fd)
{
	/*
	 * We must return fd + 1 because iterate_fd stops searching on
	 * non-zero return, but 0 is a valid fd.
	 */
	return (p == file) ? (fd + 1) : 0;
}

static void _setup_cache_mode(struct kgsl_mem_entry *entry,
		struct vm_area_struct *vma)
{
	uint64_t mode;
	pgprot_t pgprot = vma->vm_page_prot;

	if (pgprot_val(pgprot) == pgprot_val(pgprot_noncached(pgprot)))
		mode = KGSL_CACHEMODE_UNCACHED;
	else if (pgprot_val(pgprot) == pgprot_val(pgprot_writecombine(pgprot)))
		mode = KGSL_CACHEMODE_WRITECOMBINE;
	else
		mode = KGSL_CACHEMODE_WRITEBACK;

	entry->memdesc.flags |= (mode << KGSL_CACHEMODE_SHIFT);
}

static int kgsl_setup_dma_buf(struct kgsl_device *device,
				struct kgsl_pagetable *pagetable,
				struct kgsl_mem_entry *entry,
				struct dma_buf *dmabuf);

static int kgsl_setup_dmabuf_useraddr(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry, unsigned long hostptr)
{
	struct vm_area_struct *vma;
	struct dma_buf *dmabuf = NULL;
	int ret;

	/*
	 * Find the VMA containing this pointer and figure out if it
	 * is a dma-buf.
	 */
	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, hostptr);

	if (vma && vma->vm_file) {
		int fd;

		ret = check_vma_flags(vma, entry->memdesc.flags);
		if (ret) {
			up_read(&current->mm->mmap_sem);
			return ret;
		}

		/*
		 * Check to see that this isn't our own memory that we have
		 * already mapped
		 */
		if (vma->vm_ops == &kgsl_gpumem_vm_ops) {
			up_read(&current->mm->mmap_sem);
			return -EFAULT;
		}

		/* Look for the fd that matches this the vma file */
		fd = iterate_fd(current->files, 0, match_file, vma->vm_file);
		if (fd != 0)
			dmabuf = dma_buf_get(fd - 1);
	}

	if (IS_ERR_OR_NULL(dmabuf)) {
		up_read(&current->mm->mmap_sem);
		return dmabuf ? PTR_ERR(dmabuf) : -ENODEV;
	}

	ret = kgsl_setup_dma_buf(device, pagetable, entry, dmabuf);
	if (ret) {
		dma_buf_put(dmabuf);
		up_read(&current->mm->mmap_sem);
		return ret;
	}

	/* Setup the cache mode for cache operations */
	_setup_cache_mode(entry, vma);

	if (MMU_FEATURE(&device->mmu, KGSL_MMU_IO_COHERENT))
		entry->memdesc.flags |= KGSL_MEMFLAGS_IOCOHERENT;
	else
		entry->memdesc.flags &= ~((u64) KGSL_MEMFLAGS_IOCOHERENT);

	up_read(&current->mm->mmap_sem);
	return 0;
}
#else
static int kgsl_setup_dmabuf_useraddr(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry, unsigned long hostptr)
{
	return -ENODEV;
}
#endif

static int kgsl_setup_useraddr(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry,
		unsigned long hostptr, size_t offset, size_t size)
{
	int ret;

	if (hostptr == 0 || !IS_ALIGNED(hostptr, PAGE_SIZE))
		return -EINVAL;

	/* Try to set up a dmabuf - if it returns -ENODEV assume anonymous */
	ret = kgsl_setup_dmabuf_useraddr(device, pagetable, entry, hostptr);
	if (ret != -ENODEV)
		return ret;

	/* Okay - lets go legacy */
	return kgsl_setup_anon_useraddr(pagetable, entry,
		hostptr, offset, size);
}

static long _gpuobj_map_useraddr(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry,
		struct kgsl_gpuobj_import *param)
{
	struct kgsl_gpuobj_import_useraddr useraddr = {0};
	int ret;

	param->flags &= KGSL_MEMFLAGS_GPUREADONLY
		| KGSL_CACHEMODE_MASK
		| KGSL_MEMFLAGS_USE_CPU_MAP
		| KGSL_MEMTYPE_MASK
		| KGSL_MEMFLAGS_FORCE_32BIT
		| KGSL_MEMFLAGS_IOCOHERENT;

	/* Specifying SECURE is an explicit error */
	if (param->flags & KGSL_MEMFLAGS_SECURE)
		return -ENOTSUPP;

	kgsl_memdesc_init(device, &entry->memdesc, param->flags);

	ret = kgsl_copy_from_user(&useraddr,
		to_user_ptr(param->priv), sizeof(useraddr),
		param->priv_len);
	if (ret)
		return ret;

	/* Verify that the virtaddr and len are within bounds */
	if (useraddr.virtaddr > ULONG_MAX)
		return -EINVAL;

	return kgsl_setup_useraddr(device, pagetable, entry,
		(unsigned long) useraddr.virtaddr, 0, param->priv_len);
}

#ifdef CONFIG_DMA_SHARED_BUFFER
static long _gpuobj_map_dma_buf(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry,
		struct kgsl_gpuobj_import *param,
		int *fd)
{
	bool iocoherent = (param->flags & KGSL_MEMFLAGS_IOCOHERENT);
	struct kgsl_gpuobj_import_dma_buf buf;
	struct dma_buf *dmabuf;
	unsigned long flags = 0;
	int ret;

	param->flags &= KGSL_MEMFLAGS_GPUREADONLY |
		KGSL_MEMTYPE_MASK |
		KGSL_MEMALIGN_MASK |
		KGSL_MEMFLAGS_SECURE |
		KGSL_MEMFLAGS_FORCE_32BIT;

	kgsl_memdesc_init(device, &entry->memdesc, param->flags);

	/*
	 * If content protection is not enabled and secure buffer
	 * is requested to be mapped return error.
	 */
	if (entry->memdesc.flags & KGSL_MEMFLAGS_SECURE) {
		if (!kgsl_mmu_is_secured(&device->mmu)) {
			dev_WARN_ONCE(device->dev, 1,
				"Secure buffer not supported");
			return -ENOTSUPP;
		}

		entry->memdesc.priv |= KGSL_MEMDESC_SECURE;
	}

	ret = kgsl_copy_from_user(&buf, to_user_ptr(param->priv),
			sizeof(buf), param->priv_len);
	if (ret)
		return ret;

	if (buf.fd < 0)
		return -EINVAL;

	*fd = buf.fd;
	dmabuf = dma_buf_get(buf.fd);

	if (IS_ERR_OR_NULL(dmabuf))
		return (dmabuf == NULL) ? -EINVAL : PTR_ERR(dmabuf);

	/*
	 * ION cache ops are routed through kgsl, so record if the dmabuf is
	 * cached or not in the memdesc. Assume uncached if dma_buf_get_flags
	 * fails.
	 */
	dma_buf_get_flags(dmabuf, &flags);
	if (flags & ION_FLAG_CACHED) {
		entry->memdesc.flags |=
			KGSL_CACHEMODE_WRITEBACK << KGSL_CACHEMODE_SHIFT;

		/*
		 * Enable I/O coherency if it is 1) a thing, and either
		 * 2) enabled by default or 3) enabled by the caller
		 */
		if (MMU_FEATURE(&device->mmu, KGSL_MMU_IO_COHERENT) &&
		    (IS_ENABLED(CONFIG_QCOM_KGSL_IOCOHERENCY_DEFAULT) ||
		     iocoherent))
			entry->memdesc.flags |= KGSL_MEMFLAGS_IOCOHERENT;
	}

	ret = kgsl_setup_dma_buf(device, pagetable, entry, dmabuf);
	if (ret)
		dma_buf_put(dmabuf);

	return ret;
}
#else
static long _gpuobj_map_dma_buf(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry,
		struct kgsl_gpuobj_import *param,
		int *fd)
{
	return -EINVAL;
}
#endif

long kgsl_ioctl_gpuobj_import(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpuobj_import *param = data;
	struct kgsl_mem_entry *entry;
	int ret, fd = -1;

	if (param->type != KGSL_USER_MEM_TYPE_ADDR &&
		param->type != KGSL_USER_MEM_TYPE_DMABUF)
		return -ENOTSUPP;

	entry = kgsl_mem_entry_create();
	if (entry == NULL)
		return -ENOMEM;

	if (param->type == KGSL_USER_MEM_TYPE_ADDR)
		ret = _gpuobj_map_useraddr(dev_priv->device, private->pagetable,
			entry, param);
	else
		ret = _gpuobj_map_dma_buf(dev_priv->device, private->pagetable,
			entry, param, &fd);

	if (ret)
		goto out;

	if (entry->memdesc.size >= SZ_1M)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_1M));
	else if (entry->memdesc.size >= SZ_64K)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_64K));

	param->flags = entry->memdesc.flags;

	ret = kgsl_mem_entry_attach_process(dev_priv->device, private, entry);
	if (ret)
		goto unmap;

	param->id = entry->id;

	KGSL_STATS_ADD(entry->memdesc.size, &kgsl_driver.stats.mapped,
		&kgsl_driver.stats.mapped_max);

	kgsl_process_add_stats(private,
		kgsl_memdesc_usermem_type(&entry->memdesc),
		entry->memdesc.size);

	trace_kgsl_mem_map(entry, fd);

	kgsl_mem_entry_commit_process(entry);

	/* Put the extra ref from kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);

	return 0;

unmap:
	if (kgsl_memdesc_usermem_type(&entry->memdesc) == KGSL_MEM_ENTRY_ION) {
		kgsl_destroy_ion(entry->priv_data);
		entry->memdesc.sgt = NULL;
	}

	kgsl_sharedmem_free(&entry->memdesc);

out:
	kfree(entry);
	return ret;
}

static long _map_usermem_addr(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable, struct kgsl_mem_entry *entry,
		unsigned long hostptr, size_t offset, size_t size)
{
	if (!MMU_FEATURE(&device->mmu, KGSL_MMU_PAGED))
		return -EINVAL;

	/* No CPU mapped buffer could ever be secure */
	if (entry->memdesc.flags & KGSL_MEMFLAGS_SECURE)
		return -EINVAL;

	return kgsl_setup_useraddr(device, pagetable, entry, hostptr,
		offset, size);
}

#ifdef CONFIG_DMA_SHARED_BUFFER
static int _map_usermem_dma_buf(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry,
		unsigned int fd)
{
	int ret;
	struct dma_buf *dmabuf;

	/*
	 * If content protection is not enabled and secure buffer
	 * is requested to be mapped return error.
	 */

	if (entry->memdesc.flags & KGSL_MEMFLAGS_SECURE) {
		if (!kgsl_mmu_is_secured(&device->mmu)) {
			dev_WARN_ONCE(device->dev, 1,
				"Secure buffer not supported");
			return -EINVAL;
		}

		entry->memdesc.priv |= KGSL_MEMDESC_SECURE;
	}

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		return ret ? ret : -EINVAL;
	}
	ret = kgsl_setup_dma_buf(device, pagetable, entry, dmabuf);
	if (ret)
		dma_buf_put(dmabuf);
	return ret;
}
#else
static int _map_usermem_dma_buf(struct kgsl_device *device,
		struct kgsl_pagetable *pagetable,
		struct kgsl_mem_entry *entry,
		unsigned int fd)
{
	return -EINVAL;
}
#endif

#ifdef CONFIG_DMA_SHARED_BUFFER
static int kgsl_setup_dma_buf(struct kgsl_device *device,
				struct kgsl_pagetable *pagetable,
				struct kgsl_mem_entry *entry,
				struct dma_buf *dmabuf)
{
	int ret = 0;
	struct scatterlist *s;
	struct sg_table *sg_table;
	struct dma_buf_attachment *attach = NULL;
	struct kgsl_dma_buf_meta *meta;

	meta = kzalloc(sizeof(*meta), GFP_KERNEL);
	if (!meta)
		return -ENOMEM;

	attach = dma_buf_attach(dmabuf, device->dev);

	if (IS_ERR(attach)) {
		ret = PTR_ERR(attach);
		goto out;
	}

	/*
	 * If dma buffer is marked IO coherent, skip sync at attach,
	 * which involves flushing the buffer on CPU.
	 * HW manages coherency for IO coherent buffers.
	 */
	if (entry->memdesc.flags & KGSL_MEMFLAGS_IOCOHERENT)
		attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	meta->dmabuf = dmabuf;
	meta->attach = attach;
	meta->entry = entry;

	entry->priv_data = meta;
	entry->memdesc.pagetable = pagetable;
	entry->memdesc.size = 0;
	/* USE_CPU_MAP is not impemented for ION. */
	entry->memdesc.flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);
	entry->memdesc.flags |= (uint64_t)KGSL_MEMFLAGS_USERMEM_ION;

	sg_table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);

	if (IS_ERR_OR_NULL(sg_table)) {
		ret = PTR_ERR(sg_table);
		goto out;
	}

	meta->table = sg_table;
	entry->priv_data = meta;
	entry->memdesc.sgt = sg_table;

	/* Calculate the size of the memdesc from the sglist */
	for (s = entry->memdesc.sgt->sgl; s != NULL; s = sg_next(s)) {
		int priv = (entry->memdesc.priv & KGSL_MEMDESC_SECURE) ? 1 : 0;

		/*
		 * Check that each chunk of of the sg table matches the secure
		 * flag.
		 */

		if (PagePrivate(sg_page(s)) != priv) {
			ret = -EPERM;
			goto out;
		}

		entry->memdesc.size += (uint64_t) s->length;
	}

	if (!entry->memdesc.size) {
		ret = -EINVAL;
		goto out;
	}

	add_dmabuf_list(meta);
	entry->memdesc.size = PAGE_ALIGN(entry->memdesc.size);

out:
	if (ret) {
		if (!IS_ERR_OR_NULL(attach))
			dma_buf_detach(dmabuf, attach);

		kfree(meta);
	}

	return ret;
}
#endif

#ifdef CONFIG_DMA_SHARED_BUFFER
void kgsl_get_egl_counts(struct kgsl_mem_entry *entry,
		int *egl_surface_count, int *egl_image_count)
{
	struct kgsl_dma_buf_meta *meta = entry->priv_data;
	struct dmabuf_list_entry *dle = meta->dle;
	struct kgsl_dma_buf_meta *scan_meta;
	struct kgsl_mem_entry *scan_mem_entry;

	if (!dle)
		return;

	spin_lock(&kgsl_dmabuf_lock);
	list_for_each_entry(scan_meta, &dle->dmabuf_list, node) {
		scan_mem_entry = scan_meta->entry;

		switch (kgsl_memdesc_get_memtype(&scan_mem_entry->memdesc)) {
		case KGSL_MEMTYPE_EGL_SURFACE:
			(*egl_surface_count)++;
			break;
		case KGSL_MEMTYPE_EGL_IMAGE:
			(*egl_image_count)++;
			break;
		}
	}
	spin_unlock(&kgsl_dmabuf_lock);
}
#else
void kgsl_get_egl_counts(struct kgsl_mem_entry *entry,
		int *egl_surface_count, int *egl_image_count)
{
}
#endif

long kgsl_ioctl_map_user_mem(struct kgsl_device_private *dev_priv,
				     unsigned int cmd, void *data)
{
	int result = -EINVAL;
	struct kgsl_map_user_mem *param = data;
	struct kgsl_mem_entry *entry = NULL;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mmu *mmu = &dev_priv->device->mmu;
	unsigned int memtype;
	uint64_t flags;

	/*
	 * If content protection is not enabled and secure buffer
	 * is requested to be mapped return error.
	 */

	if (param->flags & KGSL_MEMFLAGS_SECURE) {
		/* Log message and return if context protection isn't enabled */
		if (!kgsl_mmu_is_secured(mmu)) {
			dev_WARN_ONCE(dev_priv->device->dev, 1,
				"Secure buffer not supported");
			return -EOPNOTSUPP;
		}

		/* Can't use CPU map with secure buffers */
		if (param->flags & KGSL_MEMFLAGS_USE_CPU_MAP)
			return -EINVAL;
	}

	entry = kgsl_mem_entry_create();

	if (entry == NULL)
		return -ENOMEM;

	/*
	 * Convert from enum value to KGSL_MEM_ENTRY value, so that
	 * we can use the latter consistently everywhere.
	 */
	memtype = param->memtype + 1;

	/*
	 * Mask off unknown flags from userspace. This way the caller can
	 * check if a flag is supported by looking at the returned flags.
	 * Note: CACHEMODE is ignored for this call. Caching should be
	 * determined by type of allocation being mapped.
	 */
	flags = param->flags & (KGSL_MEMFLAGS_GPUREADONLY
				| KGSL_MEMTYPE_MASK
				| KGSL_MEMALIGN_MASK
				| KGSL_MEMFLAGS_USE_CPU_MAP
				| KGSL_MEMFLAGS_SECURE
				| KGSL_MEMFLAGS_IOCOHERENT);

	if (kgsl_is_compat_task())
		flags |= KGSL_MEMFLAGS_FORCE_32BIT;

	kgsl_memdesc_init(dev_priv->device, &entry->memdesc, flags);

	switch (memtype) {
	case KGSL_MEM_ENTRY_USER:
		result = _map_usermem_addr(dev_priv->device, private->pagetable,
			entry, param->hostptr, param->offset, param->len);
		break;
	case KGSL_MEM_ENTRY_ION:
		if (param->offset != 0)
			result = -EINVAL;
		else
			result = _map_usermem_dma_buf(dev_priv->device,
				private->pagetable, entry, param->fd);
		break;
	default:
		result = -EOPNOTSUPP;
		break;
	}

	if (result)
		goto error;

	if ((param->flags & KGSL_MEMFLAGS_SECURE) &&
		(entry->memdesc.size & mmu->secure_align_mask)) {
		result = -EINVAL;
		goto error_attach;
	}

	if (entry->memdesc.size >= SZ_2M)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_2M));
	else if (entry->memdesc.size >= SZ_1M)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_1M));
	else if (entry->memdesc.size >= SZ_64K)
		kgsl_memdesc_set_align(&entry->memdesc, ilog2(SZ_64));

	/* echo back flags */
	param->flags = (unsigned int) entry->memdesc.flags;

	result = kgsl_mem_entry_attach_process(dev_priv->device, private,
		entry);
	if (result)
		goto error_attach;

	/* Adjust the returned value for a non 4k aligned offset */
	param->gpuaddr = (unsigned long)
		entry->memdesc.gpuaddr + (param->offset & PAGE_MASK);

	KGSL_STATS_ADD(param->len, &kgsl_driver.stats.mapped,
		&kgsl_driver.stats.mapped_max);

	kgsl_process_add_stats(private,
			kgsl_memdesc_usermem_type(&entry->memdesc), param->len);

	trace_kgsl_mem_map(entry, param->fd);

	kgsl_mem_entry_commit_process(entry);

	/* Put the extra ref from kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);

	return result;

error_attach:
	switch (kgsl_memdesc_usermem_type(&entry->memdesc)) {
	case KGSL_MEM_ENTRY_ION:
		kgsl_destroy_ion(entry->priv_data);
		entry->memdesc.sgt = NULL;
		break;
	default:
		break;
	}
	kgsl_sharedmem_free(&entry->memdesc);
error:
	/* Clear gpuaddr here so userspace doesn't get any wrong ideas */
	param->gpuaddr = 0;

	kfree(entry);
	return result;
}

static int _kgsl_gpumem_sync_cache(struct kgsl_mem_entry *entry,
		uint64_t offset, uint64_t length, unsigned int op)
{
	int ret = 0;
	int cacheop;
	int mode;

	if (!entry)
		return 0;

	 /* Cache ops are not allowed on secure memory */
	if (entry->memdesc.flags & KGSL_MEMFLAGS_SECURE)
		return 0;

	/*
	 * Flush is defined as (clean | invalidate).  If both bits are set, then
	 * do a flush, otherwise check for the individual bits and clean or inv
	 * as requested
	 */

	if ((op & KGSL_GPUMEM_CACHE_FLUSH) == KGSL_GPUMEM_CACHE_FLUSH)
		cacheop = KGSL_CACHE_OP_FLUSH;
	else if (op & KGSL_GPUMEM_CACHE_CLEAN)
		cacheop = KGSL_CACHE_OP_CLEAN;
	else if (op & KGSL_GPUMEM_CACHE_INV)
		cacheop = KGSL_CACHE_OP_INV;
	else {
		ret = -EINVAL;
		goto done;
	}

	if (!(op & KGSL_GPUMEM_CACHE_RANGE)) {
		offset = 0;
		length = entry->memdesc.size;
	}

	mode = kgsl_memdesc_get_cachemode(&entry->memdesc);
	if (mode != KGSL_CACHEMODE_UNCACHED
		&& mode != KGSL_CACHEMODE_WRITECOMBINE) {
		trace_kgsl_mem_sync_cache(entry, offset, length, op);
		ret = kgsl_cache_range_op(&entry->memdesc, offset,
					length, cacheop);
	}

done:
	return ret;
}

/* New cache sync function - supports both directions (clean and invalidate) */

long kgsl_ioctl_gpumem_sync_cache(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data)
{
	struct kgsl_gpumem_sync_cache *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry = NULL;
	long ret;

	if (param->id != 0)
		entry = kgsl_sharedmem_find_id(private, param->id);
	else if (param->gpuaddr != 0)
		entry = kgsl_sharedmem_find(private, (uint64_t) param->gpuaddr);

	if (entry == NULL)
		return -EINVAL;

	ret = _kgsl_gpumem_sync_cache(entry, (uint64_t) param->offset,
					(uint64_t) param->length, param->op);
	kgsl_mem_entry_put(entry);
	return ret;
}

static int mem_id_cmp(const void *_a, const void *_b)
{
	const unsigned int *a = _a, *b = _b;

	if (*a == *b)
		return 0;
	return (*a > *b) ? 1 : -1;
}

#ifdef CONFIG_ARM64
/* Do not support full flush on ARM64 targets */
static inline bool check_full_flush(size_t size, int op)
{
	return false;
}
#else
/* Support full flush if the size is bigger than the threshold */
static inline bool check_full_flush(size_t size, int op)
{
	/* If we exceed the breakeven point, flush the entire cache */
	bool ret = (kgsl_driver.full_cache_threshold != 0) &&
		(size >= kgsl_driver.full_cache_threshold) &&
		(op == KGSL_GPUMEM_CACHE_FLUSH);
	if (ret)
		flush_cache_all();
	return ret;
}
#endif

long kgsl_ioctl_gpumem_sync_cache_bulk(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data)
{
	int i;
	struct kgsl_gpumem_sync_cache_bulk *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	unsigned int id, last_id = 0, *id_list = NULL, actual_count = 0;
	struct kgsl_mem_entry **entries = NULL;
	long ret = 0;
	uint64_t op_size = 0;
	bool full_flush = false;

	if (param->id_list == NULL || param->count == 0
			|| param->count > (PAGE_SIZE / sizeof(unsigned int)))
		return -EINVAL;

	id_list = kcalloc(param->count, sizeof(unsigned int), GFP_KERNEL);
	if (id_list == NULL)
		return -ENOMEM;

	entries = kcalloc(param->count, sizeof(*entries), GFP_KERNEL);
	if (entries == NULL) {
		ret = -ENOMEM;
		goto end;
	}

	if (copy_from_user(id_list, param->id_list,
				param->count * sizeof(unsigned int))) {
		ret = -EFAULT;
		goto end;
	}
	/* sort the ids so we can weed out duplicates */
	sort(id_list, param->count, sizeof(*id_list), mem_id_cmp, NULL);

	for (i = 0; i < param->count; i++) {
		unsigned int cachemode;
		struct kgsl_mem_entry *entry = NULL;

		id = id_list[i];
		/* skip 0 ids or duplicates */
		if (id == last_id)
			continue;

		entry = kgsl_sharedmem_find_id(private, id);
		if (entry == NULL)
			continue;

		/* skip uncached memory */
		cachemode = kgsl_memdesc_get_cachemode(&entry->memdesc);
		if (cachemode != KGSL_CACHEMODE_WRITETHROUGH &&
		    cachemode != KGSL_CACHEMODE_WRITEBACK) {
			kgsl_mem_entry_put(entry);
			continue;
		}

		op_size += entry->memdesc.size;
		entries[actual_count++] = entry;

		full_flush  = check_full_flush(op_size, param->op);
		if (full_flush) {
			trace_kgsl_mem_sync_full_cache(actual_count, op_size);
			break;
		}

		last_id = id;
	}

	param->op &= ~KGSL_GPUMEM_CACHE_RANGE;

	for (i = 0; i < actual_count; i++) {
		if (!full_flush)
			_kgsl_gpumem_sync_cache(entries[i], 0,
						entries[i]->memdesc.size,
						param->op);
		kgsl_mem_entry_put(entries[i]);
	}
end:
	kfree(entries);
	kfree(id_list);
	return ret;
}

/* Legacy cache function, does a flush (clean  + invalidate) */

long kgsl_ioctl_sharedmem_flush_cache(struct kgsl_device_private *dev_priv,
				 unsigned int cmd, void *data)
{
	struct kgsl_sharedmem_free *param = data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry = NULL;
	long ret;

	entry = kgsl_sharedmem_find(private, (uint64_t) param->gpuaddr);
	if (entry == NULL)
		return -EINVAL;

	ret = _kgsl_gpumem_sync_cache(entry, 0, entry->memdesc.size,
					KGSL_GPUMEM_CACHE_FLUSH);
	kgsl_mem_entry_put(entry);
	return ret;
}

long kgsl_ioctl_gpuobj_sync(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpuobj_sync *param = data;
	struct kgsl_gpuobj_sync_obj *objs;
	struct kgsl_mem_entry **entries;
	long ret = 0;
	uint64_t size = 0;
	int i;
	void __user *ptr;

	if (param->count == 0 || param->count > 128)
		return -EINVAL;

	objs = kcalloc(param->count, sizeof(*objs), GFP_KERNEL);
	if (objs == NULL)
		return -ENOMEM;

	entries = kcalloc(param->count, sizeof(*entries), GFP_KERNEL);
	if (entries == NULL) {
		kfree(objs);
		return -ENOMEM;
	}

	ptr = to_user_ptr(param->objs);

	for (i = 0; i < param->count; i++) {
		ret = kgsl_copy_from_user(&objs[i], ptr, sizeof(*objs),
			param->obj_len);
		if (ret)
			goto out;

		entries[i] = kgsl_sharedmem_find_id(private, objs[i].id);

		/* Not finding the ID is not a fatal failure - just skip it */
		if (entries[i] == NULL)
			continue;

		if (!(objs[i].op & KGSL_GPUMEM_CACHE_RANGE))
			size += entries[i]->memdesc.size;
		else if (objs[i].offset < entries[i]->memdesc.size)
			size += (entries[i]->memdesc.size - objs[i].offset);

		if (check_full_flush(size, objs[i].op)) {
			trace_kgsl_mem_sync_full_cache(i, size);
			goto out;
		}

		ptr += sizeof(*objs);
	}

	for (i = 0; !ret && i < param->count; i++)
		ret = _kgsl_gpumem_sync_cache(entries[i],
			objs[i].offset, objs[i].length, objs[i].op);

out:
	for (i = 0; i < param->count; i++)
		kgsl_mem_entry_put(entries[i]);

	kfree(entries);
	kfree(objs);

	return ret;
}

#ifdef CONFIG_ARM64
static uint64_t kgsl_filter_cachemode(uint64_t flags)
{
	/*
	 * WRITETHROUGH is not supported in arm64, so we tell the user that we
	 * use WRITEBACK which is the default caching policy.
	 */
	if ((flags & KGSL_CACHEMODE_MASK) >> KGSL_CACHEMODE_SHIFT ==
					KGSL_CACHEMODE_WRITETHROUGH) {
		flags &= ~((uint64_t) KGSL_CACHEMODE_MASK);
		flags |= (uint64_t)((KGSL_CACHEMODE_WRITEBACK <<
						KGSL_CACHEMODE_SHIFT) &
					KGSL_CACHEMODE_MASK);
	}
	return flags;
}
#else
static uint64_t kgsl_filter_cachemode(uint64_t flags)
{
	return flags;
}
#endif

/* The largest allowable alignment for a GPU object is 32MB */
#define KGSL_MAX_ALIGN (32 * SZ_1M)

struct kgsl_mem_entry *gpumem_alloc_entry(
		struct kgsl_device_private *dev_priv,
		uint64_t size, uint64_t flags)
{
	int ret;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry;
	struct kgsl_mmu *mmu = &dev_priv->device->mmu;
	unsigned int align;
	u32 cachemode;

	flags &= KGSL_MEMFLAGS_GPUREADONLY
		| KGSL_CACHEMODE_MASK
		| KGSL_MEMTYPE_MASK
		| KGSL_MEMALIGN_MASK
		| KGSL_MEMFLAGS_USE_CPU_MAP
		| KGSL_MEMFLAGS_SECURE
		| KGSL_MEMFLAGS_FORCE_32BIT
		| KGSL_MEMFLAGS_IOCOHERENT;

	/* Return not supported error if secure memory isn't enabled */
	if (!kgsl_mmu_is_secured(mmu) &&
			(flags & KGSL_MEMFLAGS_SECURE)) {
		dev_WARN_ONCE(dev_priv->device->dev, 1,
				"Secure memory not supported");
		return ERR_PTR(-EOPNOTSUPP);
	}

	/* Cap the alignment bits to the highest number we can handle */
	align = MEMFLAGS(flags, KGSL_MEMALIGN_MASK, KGSL_MEMALIGN_SHIFT);
	if (align >= ilog2(KGSL_MAX_ALIGN)) {
		dev_info(dev_priv->device->dev,
			"Alignment too large; restricting to %dK\n",
			KGSL_MAX_ALIGN >> 10);

		flags &= ~((uint64_t) KGSL_MEMALIGN_MASK);
		flags |= (uint64_t)((ilog2(KGSL_MAX_ALIGN) <<
						KGSL_MEMALIGN_SHIFT) &
					KGSL_MEMALIGN_MASK);
	}

	/* For now only allow allocations up to 4G */
	if (size == 0 || size > UINT_MAX)
		return ERR_PTR(-EINVAL);

	flags = kgsl_filter_cachemode(flags);

	entry = kgsl_mem_entry_create();
	if (entry == NULL)
		return ERR_PTR(-ENOMEM);

	ret = kgsl_allocate_user(dev_priv->device, &entry->memdesc,
		size, flags);
	if (ret != 0)
		goto err;

	ret = kgsl_mem_entry_attach_process(dev_priv->device, private, entry);
	if (ret != 0) {
		kgsl_sharedmem_free(&entry->memdesc);
		goto err;
	}

	cachemode = kgsl_memdesc_get_cachemode(&entry->memdesc);
	/*
	 * Secure buffers cannot be reclaimed. Avoid reclaim of cached buffers
	 * as we could get request for cache operations on these buffers when
	 * they are reclaimed.
	 */
	if (!(flags & KGSL_MEMFLAGS_SECURE) &&
			!(cachemode == KGSL_CACHEMODE_WRITEBACK) &&
			!(cachemode == KGSL_CACHEMODE_WRITETHROUGH))
		entry->memdesc.priv |= KGSL_MEMDESC_CAN_RECLAIM;

	kgsl_process_add_stats(private,
			kgsl_memdesc_usermem_type(&entry->memdesc),
			entry->memdesc.size);
	trace_kgsl_mem_alloc(entry);

	kgsl_mem_entry_commit_process(entry);
	return entry;
err:
	kfree(entry);
	return ERR_PTR(ret);
}

static void copy_metadata(struct kgsl_mem_entry *entry, uint64_t metadata,
		unsigned int len)
{
	unsigned int i, size;

	if (len == 0)
		return;

	size = min_t(unsigned int, len, sizeof(entry->metadata) - 1);

	if (copy_from_user(entry->metadata, to_user_ptr(metadata), size)) {
		memset(entry->metadata, 0, sizeof(entry->metadata));
		return;
	}

	/* Clean up non printable characters in the string */
	for (i = 0; i < size && entry->metadata[i] != 0; i++) {
		if (!isprint(entry->metadata[i]))
			entry->metadata[i] = '?';
	}
}

long kgsl_ioctl_gpuobj_alloc(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_gpuobj_alloc *param = data;
	struct kgsl_mem_entry *entry;
#if defined(CONFIG_DISPLAY_SAMSUNG)
	struct kgsl_process_private *private = dev_priv->process_priv;
	uint64_t debug_size;
	debug_size = param->size >> 10;

	if(debug_size > 200000) {
		pr_err("kgsl: huge memory %lldKB is requested from pid = %d comm = %s\n", debug_size, private->pid, private->comm);
	}
#endif
	entry = gpumem_alloc_entry(dev_priv, param->size, param->flags);

	if (IS_ERR(entry))
		return PTR_ERR(entry);

	copy_metadata(entry, param->metadata, param->metadata_len);

	param->size = entry->memdesc.size;
	param->flags = entry->memdesc.flags;
	param->mmapsize = kgsl_memdesc_footprint(&entry->memdesc);
	param->id = entry->id;

	/* Put the extra ref from kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);

	return 0;
}

long kgsl_ioctl_gpumem_alloc(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_gpumem_alloc *param = data;
	struct kgsl_mem_entry *entry;
	uint64_t flags = param->flags;

	/* Legacy functions doesn't support these advanced features */
	flags &= ~((uint64_t) KGSL_MEMFLAGS_USE_CPU_MAP);

	if (kgsl_is_compat_task())
		flags |= KGSL_MEMFLAGS_FORCE_32BIT;

	entry = gpumem_alloc_entry(dev_priv, (uint64_t) param->size, flags);

	if (IS_ERR(entry))
		return PTR_ERR(entry);

	param->gpuaddr = (unsigned long) entry->memdesc.gpuaddr;
	param->size = (size_t) entry->memdesc.size;
	param->flags = (unsigned int) entry->memdesc.flags;

	/* Put the extra ref from kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);

	return 0;
}

long kgsl_ioctl_gpumem_alloc_id(struct kgsl_device_private *dev_priv,
			unsigned int cmd, void *data)
{
	struct kgsl_gpumem_alloc_id *param = data;
	struct kgsl_mem_entry *entry;
	uint64_t flags = param->flags;

	if (kgsl_is_compat_task())
		flags |= KGSL_MEMFLAGS_FORCE_32BIT;

	entry = gpumem_alloc_entry(dev_priv, (uint64_t) param->size, flags);

	if (IS_ERR(entry))
		return PTR_ERR(entry);

	param->id = entry->id;
	param->flags = (unsigned int) entry->memdesc.flags;
	param->size = (size_t) entry->memdesc.size;
	param->mmapsize = (size_t) kgsl_memdesc_footprint(&entry->memdesc);
	param->gpuaddr = (unsigned long) entry->memdesc.gpuaddr;

	/* Put the extra ref from kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);

	return 0;
}

long kgsl_ioctl_gpumem_get_info(struct kgsl_device_private *dev_priv,
			unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpumem_get_info *param = data;
	struct kgsl_mem_entry *entry = NULL;
	int result = 0;

	if (param->id != 0)
		entry = kgsl_sharedmem_find_id(private, param->id);
	else if (param->gpuaddr != 0)
		entry = kgsl_sharedmem_find(private, (uint64_t) param->gpuaddr);

	if (entry == NULL)
		return -EINVAL;

	/*
	 * If any of the 64 bit address / sizes would end up being
	 * truncated, return -ERANGE.  That will signal the user that they
	 * should use a more modern API
	 */
	if (entry->memdesc.gpuaddr > ULONG_MAX)
		result = -ERANGE;

	param->gpuaddr = (unsigned long) entry->memdesc.gpuaddr;
	param->id = entry->id;
	param->flags = (unsigned int) entry->memdesc.flags;
	param->size = (size_t) entry->memdesc.size;
	param->mmapsize = (size_t) kgsl_memdesc_footprint(&entry->memdesc);
	/*
	 * Entries can have multiple user mappings so thre isn't any one address
	 * we can report. Plus, the user should already know their mappings, so
	 * there isn't any value in reporting it back to them.
	 */
	param->useraddr = 0;

	kgsl_mem_entry_put(entry);
	return result;
}

static inline int _sparse_alloc_param_sanity_check(uint64_t size,
		uint64_t pagesize)
{
	if (size == 0 || pagesize == 0)
		return -EINVAL;

	if (pagesize != PAGE_SIZE && pagesize != SZ_64K)
		return -EINVAL;

	if (pagesize > size || !IS_ALIGNED(size, pagesize))
		return -EINVAL;

	return 0;
}

long kgsl_ioctl_sparse_phys_alloc(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data)
{
	struct kgsl_process_private *process = dev_priv->process_priv;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_sparse_phys_alloc *param = data;
	struct kgsl_mem_entry *entry;
	uint64_t flags;
	int ret;
	int id;

	if (!(device->flags & KGSL_FLAG_SPARSE))
		return -ENOTSUPP;

	ret = _sparse_alloc_param_sanity_check(param->size, param->pagesize);
	if (ret)
		return ret;

	entry = kgsl_mem_entry_create();
	if (entry == NULL)
		return -ENOMEM;

	ret = kgsl_process_private_get(process);
	if (!ret) {
		ret = -EBADF;
		goto err_free_entry;
	}

	idr_preload(GFP_KERNEL);
	spin_lock(&process->mem_lock);
	/* Allocate the ID but don't attach the pointer just yet */
	id = idr_alloc(&process->mem_idr, NULL, 1, 0, GFP_NOWAIT);
	spin_unlock(&process->mem_lock);
	idr_preload_end();

	if (id < 0) {
		ret = id;
		goto err_put_proc_priv;
	}

	entry->id = id;
	entry->priv = process;

	flags = KGSL_MEMFLAGS_SPARSE_PHYS |
		((ilog2(param->pagesize) << KGSL_MEMALIGN_SHIFT) &
			KGSL_MEMALIGN_MASK);

	ret = kgsl_allocate_user(dev_priv->device, &entry->memdesc,
			param->size, flags);
	if (ret)
		goto err_remove_idr;

	/* Sanity check to verify we got correct pagesize */
	if (param->pagesize != PAGE_SIZE && entry->memdesc.sgt != NULL) {
		struct scatterlist *s;
		int i;

		for_each_sg(entry->memdesc.sgt->sgl, s,
				entry->memdesc.sgt->nents, i) {
			if (!IS_ALIGNED(s->length, param->pagesize))
				goto err_invalid_pages;
		}
	}

	param->id = entry->id;
	param->flags = entry->memdesc.flags;

	kgsl_process_add_stats(process,
			kgsl_memdesc_usermem_type(&entry->memdesc),
			entry->memdesc.size);

	trace_sparse_phys_alloc(entry->id, param->size, param->pagesize);
	kgsl_mem_entry_commit_process(entry);

	/* Put the extra ref from kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);

	return 0;

err_invalid_pages:
	kgsl_sharedmem_free(&entry->memdesc);
err_remove_idr:
	spin_lock(&process->mem_lock);
	idr_remove(&process->mem_idr, entry->id);
	spin_unlock(&process->mem_lock);
err_put_proc_priv:
	kgsl_process_private_put(process);
err_free_entry:
	kfree(entry);

	return ret;
}

long kgsl_ioctl_sparse_phys_free(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data)
{
	struct kgsl_process_private *process = dev_priv->process_priv;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_sparse_phys_free *param = data;
	struct kgsl_mem_entry *entry;

	if (!(device->flags & KGSL_FLAG_SPARSE))
		return -ENOTSUPP;

	entry = kgsl_sharedmem_find_id_flags(process, param->id,
			KGSL_MEMFLAGS_SPARSE_PHYS);
	if (entry == NULL)
		return -EINVAL;

	if (!kgsl_mem_entry_set_pend(entry)) {
		kgsl_mem_entry_put(entry);
		return -EBUSY;
	}

	if (entry->memdesc.cur_bindings != 0) {
		kgsl_mem_entry_unset_pend(entry);
		kgsl_mem_entry_put(entry);
		return -EINVAL;
	}

	trace_sparse_phys_free(entry->id);

	/* One put for find_id(), one put for the kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);
	kgsl_mem_entry_put_deferred(entry);

	return 0;
}

long kgsl_ioctl_sparse_virt_alloc(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_sparse_virt_alloc *param = data;
	struct kgsl_mem_entry *entry;
	int ret;

	if (!(device->flags & KGSL_FLAG_SPARSE))
		return -ENOTSUPP;

	ret = _sparse_alloc_param_sanity_check(param->size, param->pagesize);
	if (ret)
		return ret;

	entry = kgsl_mem_entry_create();
	if (entry == NULL)
		return -ENOMEM;

	kgsl_memdesc_init(dev_priv->device, &entry->memdesc,
			KGSL_MEMFLAGS_SPARSE_VIRT);
	entry->memdesc.size = param->size;
	entry->memdesc.cur_bindings = 0;
	kgsl_memdesc_set_align(&entry->memdesc, ilog2(param->pagesize));

	spin_lock_init(&entry->bind_lock);
	entry->bind_tree = RB_ROOT;

	ret = kgsl_mem_entry_attach_process(dev_priv->device, private, entry);
	if (ret) {
		kfree(entry);
		return ret;
	}

	param->id = entry->id;
	param->gpuaddr = entry->memdesc.gpuaddr;
	param->flags = entry->memdesc.flags;

	trace_sparse_virt_alloc(entry->id, param->size, param->pagesize);
	kgsl_mem_entry_commit_process(entry);

	/* Put the extra ref from kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);

	return 0;
}

long kgsl_ioctl_sparse_virt_free(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data)
{
	struct kgsl_process_private *process = dev_priv->process_priv;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_sparse_virt_free *param = data;
	struct kgsl_mem_entry *entry = NULL;

	if (!(device->flags & KGSL_FLAG_SPARSE))
		return -ENOTSUPP;

	entry = kgsl_sharedmem_find_id_flags(process, param->id,
			KGSL_MEMFLAGS_SPARSE_VIRT);
	if (entry == NULL)
		return -EINVAL;

	if (!kgsl_mem_entry_set_pend(entry)) {
		kgsl_mem_entry_put(entry);
		return -EBUSY;
	}

	if (entry->bind_tree.rb_node != NULL) {
		kgsl_mem_entry_unset_pend(entry);
		kgsl_mem_entry_put(entry);
		return -EINVAL;
	}

	trace_sparse_virt_free(entry->id);

	/* One put for find_id(), one put for the kgsl_mem_entry_create() */
	kgsl_mem_entry_put(entry);
	kgsl_mem_entry_put_deferred(entry);

	return 0;
}

/* entry->bind_lock must be held by the caller */
static int _sparse_add_to_bind_tree(struct kgsl_mem_entry *entry,
		uint64_t v_offset,
		struct kgsl_memdesc *memdesc,
		uint64_t p_offset,
		uint64_t size,
		uint64_t flags)
{
	struct sparse_bind_object *new;
	struct rb_node **node, *parent = NULL;

	new = kzalloc(sizeof(*new), GFP_ATOMIC);
	if (new == NULL)
		return -ENOMEM;

	new->v_off = v_offset;
	new->p_off = p_offset;
	new->p_memdesc = memdesc;
	new->size = size;
	new->flags = flags;

	node = &entry->bind_tree.rb_node;

	while (*node != NULL) {
		struct sparse_bind_object *this;

		parent = *node;
		this = rb_entry(parent, struct sparse_bind_object, node);

		if ((new->v_off < this->v_off) &&
			((new->v_off + new->size) <= this->v_off))
			node = &parent->rb_left;
		else if ((new->v_off > this->v_off) &&
			(new->v_off >= (this->v_off + this->size)))
			node = &parent->rb_right;
		else {
			kfree(new);
			return -EADDRINUSE;
		}
	}

	rb_link_node(&new->node, parent, node);
	rb_insert_color(&new->node, &entry->bind_tree);

	return 0;
}

static int _sparse_rm_from_bind_tree(struct kgsl_mem_entry *entry,
		struct sparse_bind_object *obj,
		uint64_t v_offset, uint64_t size)
{
	if (v_offset == obj->v_off && size >= obj->size) {
		/*
		 * We are all encompassing, remove the entry and free
		 * things up
		 */
		rb_erase(&obj->node, &entry->bind_tree);
		kfree(obj);
	} else if (v_offset == obj->v_off) {
		/*
		 * We are the front of the node, adjust the front of
		 * the node
		 */
		obj->v_off += size;
		obj->p_off += size;
		obj->size -= size;
	} else if ((v_offset + size) == (obj->v_off + obj->size)) {
		/*
		 * We are at the end of the obj, adjust the beginning
		 * points
		 */
		obj->size -= size;
	} else {
		/*
		 * We are in the middle of a node, split it up and
		 * create a new mini node. Adjust this node's bounds
		 * and add the new node to the list.
		 */
		uint64_t tmp_size = obj->size;
		int ret;

		obj->size = v_offset - obj->v_off;

		ret = _sparse_add_to_bind_tree(entry, v_offset + size,
				obj->p_memdesc,
				obj->p_off + (v_offset - obj->v_off) + size,
				tmp_size - (v_offset - obj->v_off) - size,
				obj->flags);

		return ret;
	}

	return 0;
}

/* entry->bind_lock must be held by the caller */
static struct sparse_bind_object *_find_containing_bind_obj(
		struct kgsl_mem_entry *entry,
		uint64_t offset, uint64_t size)
{
	struct sparse_bind_object *obj = NULL;
	struct rb_node *node = entry->bind_tree.rb_node;

	while (node != NULL) {
		obj = rb_entry(node, struct sparse_bind_object, node);

		if (offset == obj->v_off) {
			break;
		} else if (offset < obj->v_off) {
			if (offset + size > obj->v_off)
				break;
			node = node->rb_left;
			obj = NULL;
		} else if (offset > obj->v_off) {
			if (offset < obj->v_off + obj->size)
				break;
			node = node->rb_right;
			obj = NULL;
		}
	}

	return obj;
}

/* entry->bind_lock must be held by the caller */
static int _sparse_unbind(struct kgsl_mem_entry *entry,
		struct sparse_bind_object *bind_obj,
		uint64_t offset, uint64_t size)
{
	int ret;

	ret = _sparse_rm_from_bind_tree(entry, bind_obj, offset, size);
	if (ret == 0) {
		atomic_long_sub(size, &kgsl_driver.stats.mapped);
		trace_sparse_unbind(entry->id, offset, size);
	}

	return ret;
}

static long sparse_unbind_range(struct kgsl_sparse_binding_object *obj,
	struct kgsl_mem_entry *virt_entry)
{
	struct sparse_bind_object *bind_obj;
	struct kgsl_memdesc *memdesc;
	struct kgsl_pagetable *pt;
	int ret = 0;
	uint64_t size = obj->size;
	uint64_t tmp_size = obj->size;
	uint64_t offset = obj->virtoffset;

	while (size > 0 && ret == 0) {
		tmp_size = size;

		spin_lock(&virt_entry->bind_lock);
		bind_obj = _find_containing_bind_obj(virt_entry, offset, size);

		if (bind_obj == NULL) {
			spin_unlock(&virt_entry->bind_lock);
			return 0;
		}

		if (bind_obj->v_off > offset) {
			tmp_size = size - bind_obj->v_off - offset;
			if (tmp_size > bind_obj->size)
				tmp_size = bind_obj->size;
			offset = bind_obj->v_off;
		} else if (bind_obj->v_off < offset) {
			uint64_t diff = offset - bind_obj->v_off;

			if (diff + size > bind_obj->size)
				tmp_size = bind_obj->size - diff;
		} else {
			if (tmp_size > bind_obj->size)
				tmp_size = bind_obj->size;
		}

		memdesc = bind_obj->p_memdesc;
		pt = memdesc->pagetable;

		if (memdesc->cur_bindings < (tmp_size / PAGE_SIZE)) {
			spin_unlock(&virt_entry->bind_lock);
			return -EINVAL;
		}

		memdesc->cur_bindings -= tmp_size / PAGE_SIZE;

		ret = _sparse_unbind(virt_entry, bind_obj, offset, tmp_size);
		spin_unlock(&virt_entry->bind_lock);

		ret = kgsl_mmu_unmap_offset(pt, memdesc,
				virt_entry->memdesc.gpuaddr, offset, tmp_size);
		if (ret)
			return ret;

		ret = kgsl_mmu_sparse_dummy_map(pt, memdesc, offset, tmp_size);
		if (ret)
			return ret;

		if (ret == 0) {
			offset += tmp_size;
			size -= tmp_size;
		}
	}

	return ret;
}

static inline bool _is_phys_bindable(struct kgsl_mem_entry *phys_entry,
		uint64_t offset, uint64_t size, uint64_t flags)
{
	struct kgsl_memdesc *memdesc = &phys_entry->memdesc;

	if (!IS_ALIGNED(offset | size, kgsl_memdesc_get_pagesize(memdesc)))
		return false;

	if (offset + size < offset)
		return false;

	if (!(flags & KGSL_SPARSE_BIND_MULTIPLE_TO_PHYS) &&
			offset + size > memdesc->size)
		return false;

	return true;
}

static int _sparse_bind(struct kgsl_process_private *process,
		struct kgsl_mem_entry *virt_entry, uint64_t v_offset,
		struct kgsl_mem_entry *phys_entry, uint64_t p_offset,
		uint64_t size, uint64_t flags)
{
	int ret;
	struct kgsl_pagetable *pagetable;
	struct kgsl_memdesc *memdesc = &phys_entry->memdesc;

	/* map the memory after unlocking if gpuaddr has been assigned */
	if (memdesc->gpuaddr)
		return -EINVAL;

	pagetable = memdesc->pagetable;

	/* Clear out any mappings */
	ret = kgsl_mmu_unmap_offset(pagetable, &virt_entry->memdesc,
			virt_entry->memdesc.gpuaddr, v_offset, size);
	if (ret)
		return ret;

	ret = kgsl_mmu_map_offset(pagetable, virt_entry->memdesc.gpuaddr,
			v_offset, memdesc, p_offset, size, flags);
	if (ret) {
		/* Try to clean up, but not the end of the world */
		kgsl_mmu_sparse_dummy_map(pagetable, &virt_entry->memdesc,
				v_offset, size);
		return ret;
	}

	spin_lock(&virt_entry->bind_lock);
	ret = _sparse_add_to_bind_tree(virt_entry, v_offset, memdesc,
			p_offset, size, flags);
	spin_unlock(&virt_entry->bind_lock);

	if (ret == 0)
		memdesc->cur_bindings += size / PAGE_SIZE;

	return ret;
}

static long sparse_bind_range(struct kgsl_process_private *private,
		struct kgsl_sparse_binding_object *obj,
		struct kgsl_mem_entry *virt_entry)
{
	struct kgsl_mem_entry *phys_entry;
	int ret;

	phys_entry = kgsl_sharedmem_find_id_flags(private, obj->id,
			KGSL_MEMFLAGS_SPARSE_PHYS);
	if (phys_entry == NULL)
		return -EINVAL;

	if (!_is_phys_bindable(phys_entry, obj->physoffset, obj->size,
				obj->flags)) {
		kgsl_mem_entry_put(phys_entry);
		return -EINVAL;
	}

	if (kgsl_memdesc_get_align(&virt_entry->memdesc) !=
			kgsl_memdesc_get_align(&phys_entry->memdesc)) {
		kgsl_mem_entry_put(phys_entry);
		return -EINVAL;
	}

	ret = sparse_unbind_range(obj, virt_entry);
	if (ret) {
		kgsl_mem_entry_put(phys_entry);
		return -EINVAL;
	}

	ret = _sparse_bind(private, virt_entry, obj->virtoffset,
			phys_entry, obj->physoffset, obj->size,
			obj->flags & KGSL_SPARSE_BIND_MULTIPLE_TO_PHYS);
	if (ret == 0) {
		KGSL_STATS_ADD(obj->size, &kgsl_driver.stats.mapped,
				&kgsl_driver.stats.mapped_max);

		trace_sparse_bind(virt_entry->id, obj->virtoffset,
				phys_entry->id, obj->physoffset,
				obj->size, obj->flags);
	}

	kgsl_mem_entry_put(phys_entry);

	return ret;
}

long kgsl_ioctl_sparse_bind(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_sparse_bind *param = data;
	struct kgsl_sparse_binding_object obj;
	struct kgsl_mem_entry *virt_entry;
	int pg_sz;
	void __user *ptr;
	int ret = 0;
	int i = 0;

	if (!(device->flags & KGSL_FLAG_SPARSE))
		return -ENOTSUPP;

	ptr = (void __user *) (uintptr_t) param->list;

	if (param->size > sizeof(struct kgsl_sparse_binding_object) ||
		param->count == 0 || ptr == NULL)
		return -EINVAL;

	virt_entry = kgsl_sharedmem_find_id_flags(private, param->id,
			KGSL_MEMFLAGS_SPARSE_VIRT);
	if (virt_entry == NULL)
		return -EINVAL;

	pg_sz = kgsl_memdesc_get_pagesize(&virt_entry->memdesc);

	for (i = 0; i < param->count; i++) {
		memset(&obj, 0, sizeof(obj));
		ret = kgsl_copy_from_user(&obj, ptr, sizeof(obj), param->size);
		if (ret)
			break;

		/* Sanity check initial range */
		if (obj.size == 0 || obj.virtoffset + obj.size < obj.size ||
			obj.virtoffset + obj.size > virt_entry->memdesc.size ||
			!(IS_ALIGNED(obj.virtoffset | obj.size, pg_sz))) {
			ret = -EINVAL;
			break;
		}

		if (obj.flags & KGSL_SPARSE_BIND)
			ret = sparse_bind_range(private, &obj, virt_entry);
		else if (obj.flags & KGSL_SPARSE_UNBIND)
			ret = sparse_unbind_range(&obj, virt_entry);
		else
			ret = -EINVAL;
		if (ret)
			break;

		ptr += sizeof(obj);
	}

	kgsl_mem_entry_put(virt_entry);

	return ret;
}

long kgsl_ioctl_gpu_sparse_command(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_gpu_sparse_command *param = data;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_context *context;
	struct kgsl_drawobj *drawobj[2];
	struct kgsl_drawobj_sparse *sparseobj;
	long result;
	unsigned int i = 0;

	if (!(device->flags & KGSL_FLAG_SPARSE))
		return -ENOTSUPP;

	/* Make sure sparse and syncpoint count isn't too big */
	if (param->numsparse > KGSL_MAX_SPARSE ||
		param->numsyncs > KGSL_MAX_SYNCPOINTS)
		return -EINVAL;

	/* Make sure there is atleast one sparse or sync */
	if (param->numsparse == 0 && param->numsyncs == 0)
		return -EINVAL;

	/* Only Sparse commands are supported in this ioctl */
	if (!(param->flags & KGSL_DRAWOBJ_SPARSE) || (param->flags &
			(KGSL_DRAWOBJ_SUBMIT_IB_LIST | KGSL_DRAWOBJ_MARKER
			| KGSL_DRAWOBJ_SYNC)))
		return -EINVAL;

	context = kgsl_context_get_owner(dev_priv, param->context_id);
	if (context == NULL)
		return -EINVAL;

	/* Restrict bind commands to bind context */
	if (!(context->flags & KGSL_CONTEXT_SPARSE)) {
		kgsl_context_put(context);
		return -EINVAL;
	}

	if (param->numsyncs) {
		struct kgsl_drawobj_sync *syncobj = kgsl_drawobj_sync_create(
				device, context);
		if (IS_ERR(syncobj)) {
			result = PTR_ERR(syncobj);
			goto done;
		}

		drawobj[i++] = DRAWOBJ(syncobj);
		result = kgsl_drawobj_sync_add_synclist(device, syncobj,
				to_user_ptr(param->synclist),
				param->syncsize, param->numsyncs);
		if (result)
			goto done;
	}

	if (param->numsparse) {
		sparseobj = kgsl_drawobj_sparse_create(device, context,
					param->flags);
		if (IS_ERR(sparseobj)) {
			result = PTR_ERR(sparseobj);
			goto done;
		}

		sparseobj->id = param->id;
		drawobj[i++] = DRAWOBJ(sparseobj);
		result = kgsl_drawobj_sparse_add_sparselist(device, sparseobj,
				param->id, to_user_ptr(param->sparselist),
				param->sparsesize, param->numsparse);
		if (result)
			goto done;
	}

	result = dev_priv->device->ftbl->queue_cmds(dev_priv, context,
					drawobj, i, &param->timestamp);

done:
	/*
	 * -EPROTO is a "success" error - it just tells the user that the
	 * context had previously faulted
	 */
	if (result && result != -EPROTO)
		while (i--)
			kgsl_drawobj_destroy(drawobj[i]);

	kgsl_context_put(context);
	return result;
}

void kgsl_sparse_bind(struct kgsl_process_private *private,
		struct kgsl_drawobj_sparse *sparseobj)
{
	struct kgsl_sparseobj_node *sparse_node;
	struct kgsl_mem_entry *virt_entry = NULL;
	long ret = 0;
	char *name;

	virt_entry = kgsl_sharedmem_find_id_flags(private, sparseobj->id,
			KGSL_MEMFLAGS_SPARSE_VIRT);
	if (virt_entry == NULL)
		return;

	list_for_each_entry(sparse_node, &sparseobj->sparselist, node) {
		if (sparse_node->obj.flags & KGSL_SPARSE_BIND) {
			ret = sparse_bind_range(private, &sparse_node->obj,
					virt_entry);
			name = "bind";
		} else {
			ret = sparse_unbind_range(&sparse_node->obj,
					virt_entry);
			name = "unbind";
		}

		if (ret)
			pr_err("kgsl: unable to '%s' ret %ld virt_id %d,phys_id %d, virt_offset %16.16llX,phys_offset %16.16llX, size %16.16llX,flags %16.16llX\n",
					name, ret, sparse_node->virt_id,
					sparse_node->obj.id,
					sparse_node->obj.virtoffset,
					sparse_node->obj.physoffset,
					sparse_node->obj.size,
					sparse_node->obj.flags);
	}

	kgsl_mem_entry_put(virt_entry);
}
EXPORT_SYMBOL(kgsl_sparse_bind);

long kgsl_ioctl_gpuobj_info(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpuobj_info *param = data;
	struct kgsl_mem_entry *entry;

	if (param->id == 0)
		return -EINVAL;

	entry = kgsl_sharedmem_find_id(private, param->id);
	if (entry == NULL)
		return -EINVAL;

	param->id = entry->id;
	param->gpuaddr = entry->memdesc.gpuaddr;
	param->flags = entry->memdesc.flags;
	param->size = entry->memdesc.size;
	param->va_len = kgsl_memdesc_footprint(&entry->memdesc);
	/*
	 * Entries can have multiple user mappings so thre isn't any one address
	 * we can report. Plus, the user should already know their mappings, so
	 * there isn't any value in reporting it back to them.
	 */
	param->va_addr = 0;

	kgsl_mem_entry_put(entry);
	return 0;
}

long kgsl_ioctl_gpuobj_set_info(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_gpuobj_set_info *param = data;
	struct kgsl_mem_entry *entry;
	int ret = 0;

	if (param->id == 0)
		return -EINVAL;

	entry = kgsl_sharedmem_find_id(private, param->id);
	if (entry == NULL)
		return -EINVAL;

	if (param->flags & KGSL_GPUOBJ_SET_INFO_METADATA)
		copy_metadata(entry, param->metadata, param->metadata_len);

	if (param->flags & KGSL_GPUOBJ_SET_INFO_TYPE) {
		if (param->type <= (KGSL_MEMTYPE_MASK >> KGSL_MEMTYPE_SHIFT)) {
			entry->memdesc.flags &= ~((uint64_t) KGSL_MEMTYPE_MASK);
			entry->memdesc.flags |= (uint64_t)((param->type <<
				KGSL_MEMTYPE_SHIFT) & KGSL_MEMTYPE_MASK);
		} else
			ret = -EINVAL;
	}

	kgsl_mem_entry_put(entry);
	return ret;
}

/**
 * kgsl_ioctl_timestamp_event - Register a new timestamp event from userspace
 * @dev_priv - pointer to the private device structure
 * @cmd - the ioctl cmd passed from kgsl_ioctl
 * @data - the user data buffer from kgsl_ioctl
 * @returns 0 on success or error code on failure
 */

long kgsl_ioctl_timestamp_event(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data)
{
	struct kgsl_timestamp_event *param = data;
	int ret;

	switch (param->type) {
	case KGSL_TIMESTAMP_EVENT_FENCE:
		ret = kgsl_add_fence_event(dev_priv->device,
			param->context_id, param->timestamp, param->priv,
			param->len, dev_priv);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int
kgsl_mmap_memstore(struct kgsl_device *device, struct vm_area_struct *vma)
{
	struct kgsl_memdesc *memdesc = &device->memstore;
	int result;
	unsigned int vma_size = vma->vm_end - vma->vm_start;

	/* The memstore can only be mapped as read only */

	if (vma->vm_flags & VM_WRITE)
		return -EPERM;

	vma->vm_flags &= ~VM_MAYWRITE;

	if (memdesc->size  !=  vma_size) {
		dev_err(device->dev,
			     "memstore bad size: %d should be %llu\n",
			     vma_size, memdesc->size);
		return -EINVAL;
	}

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	result = remap_pfn_range(vma, vma->vm_start,
				device->memstore.physaddr >> PAGE_SHIFT,
				 vma_size, vma->vm_page_prot);
	if (result != 0)
		dev_err(device->dev, "remap_pfn_range failed: %d\n",
			     result);

	return result;
}

/*
 * kgsl_gpumem_vm_open is called whenever a vma region is copied or split.
 * Increase the refcount to make sure that the accounting stays correct
 */

static void kgsl_gpumem_vm_open(struct vm_area_struct *vma)
{
	struct kgsl_mem_entry *entry = vma->vm_private_data;

	if (kgsl_mem_entry_get(entry) == 0)
		vma->vm_private_data = NULL;

	atomic_inc(&entry->map_count);
}

static int
kgsl_gpumem_vm_fault(struct vm_fault *vmf)
{
	struct kgsl_mem_entry *entry = vmf->vma->vm_private_data;

	if (!entry)
		return VM_FAULT_SIGBUS;
	if (!entry->memdesc.ops || !entry->memdesc.ops->vmfault)
		return VM_FAULT_SIGBUS;

	return entry->memdesc.ops->vmfault(&entry->memdesc, vmf->vma, vmf);
}

static void
kgsl_gpumem_vm_close(struct vm_area_struct *vma)
{
	struct kgsl_mem_entry *entry  = vma->vm_private_data;

	if (!entry)
		return;

	/*
	 * Remove the memdesc from the mapped stat once all the mappings have
	 * gone away
	 */
	if (!atomic_dec_return(&entry->map_count))
		atomic_long_sub(entry->memdesc.size,
				&entry->priv->gpumem_mapped);

	kgsl_mem_entry_put_deferred(entry);
}

static const struct vm_operations_struct kgsl_gpumem_vm_ops = {
	.open  = kgsl_gpumem_vm_open,
	.fault = kgsl_gpumem_vm_fault,
	.close = kgsl_gpumem_vm_close,
};

static int
get_mmap_entry(struct kgsl_process_private *private,
		struct kgsl_mem_entry **out_entry, unsigned long pgoff,
		unsigned long len)
{
	int ret = 0;
	struct kgsl_mem_entry *entry;

	entry = kgsl_sharedmem_find_id(private, pgoff);
	if (entry == NULL)
		entry = kgsl_sharedmem_find(private, pgoff << PAGE_SHIFT);

	if (!entry)
		return -EINVAL;

	if (!entry->memdesc.ops ||
		!entry->memdesc.ops->vmflags ||
		!entry->memdesc.ops->vmfault) {
		ret = -EINVAL;
		goto err_put;
	}

	if (entry->memdesc.flags & KGSL_MEMFLAGS_SPARSE_PHYS) {
		if (len != entry->memdesc.size) {
			ret = -EINVAL;
			goto err_put;
		}
	}

	/* Don't allow ourselves to remap user memory */
	if (entry->memdesc.flags & KGSL_MEMFLAGS_USERMEM_ADDR) {
		ret = -EBUSY;
		goto err_put;
	}

	if (kgsl_memdesc_use_cpu_map(&entry->memdesc)) {
		if (len != kgsl_memdesc_footprint(&entry->memdesc)) {
			ret = -ERANGE;
			goto err_put;
		}
	} else if (len != kgsl_memdesc_footprint(&entry->memdesc) &&
		len != entry->memdesc.size) {
		/*
		 * If cpu_map != gpumap then user can map either the
		 * footprint or the entry size
		 */
		ret = -ERANGE;
		goto err_put;
	}

	*out_entry = entry;
	return 0;
err_put:
	kgsl_mem_entry_put(entry);
	return ret;
}

static unsigned long _gpu_set_svm_region(struct kgsl_process_private *private,
		struct kgsl_mem_entry *entry, unsigned long addr,
		unsigned long size)
{
	int ret;

	/*
	 * Protect access to the gpuaddr here to prevent multiple vmas from
	 * trying to map a SVM region at the same time
	 */
	spin_lock(&entry->memdesc.gpuaddr_lock);

	if (entry->memdesc.gpuaddr) {
		spin_unlock(&entry->memdesc.gpuaddr_lock);
		return (unsigned long) -EBUSY;
	}

	ret = kgsl_mmu_set_svm_region(private->pagetable, (uint64_t) addr,
		(uint64_t) size);

	if (ret != 0) {
		spin_unlock(&entry->memdesc.gpuaddr_lock);
		return (unsigned long) ret;
	}

	entry->memdesc.gpuaddr = (uint64_t) addr;
	spin_unlock(&entry->memdesc.gpuaddr_lock);

	entry->memdesc.pagetable = private->pagetable;

	ret = kgsl_mmu_map(private->pagetable, &entry->memdesc);
	if (ret) {
		kgsl_mmu_put_gpuaddr(&entry->memdesc);
		return (unsigned long) ret;
	}

	kgsl_memfree_purge(private->pagetable, entry->memdesc.gpuaddr,
		entry->memdesc.size);

	return addr;
}

static unsigned long _gpu_find_svm(struct kgsl_process_private *private,
		unsigned long start, unsigned long end, unsigned long len,
		unsigned int align)
{
	uint64_t addr = kgsl_mmu_find_svm_region(private->pagetable,
		(uint64_t) start, (uint64_t)end, (uint64_t) len, align);

	WARN(!IS_ERR_VALUE((unsigned long)addr) && (addr > ULONG_MAX),
		"Couldn't find range\n");

	return (unsigned long) addr;
}

/* Search top down in the CPU VM region for a free address */
static unsigned long _cpu_get_unmapped_area(unsigned long bottom,
		unsigned long top, unsigned long len, unsigned long align)
{
	struct vm_unmapped_area_info info;
	unsigned long addr, err;

	info.flags = VM_UNMAPPED_AREA_TOPDOWN;
	info.low_limit = bottom;
	info.high_limit = top;
	info.length = len;
	info.align_offset = 0;
	info.align_mask = align - 1;

	addr = vm_unmapped_area(&info);

	if (IS_ERR_VALUE(addr))
		return addr;

	err = security_mmap_addr(addr);
	return err ? err : addr;
}

static unsigned long _search_range(struct kgsl_process_private *private,
		struct kgsl_mem_entry *entry,
		unsigned long start, unsigned long end,
		unsigned long len, uint64_t align)
{
	unsigned long cpu, gpu = end, result = -ENOMEM;

	while (gpu > start) {
		/* find a new empty spot on the CPU below the last one */
		cpu = _cpu_get_unmapped_area(start, gpu, len,
			(unsigned long) align);
		if (IS_ERR_VALUE(cpu)) {
			result = cpu;
			break;
		}
		/* try to map it on the GPU */
		result = _gpu_set_svm_region(private, entry, cpu, len);
		if (!IS_ERR_VALUE(result))
			break;
		/*
		 * _gpu_set_svm_region will return -EBUSY if we tried to set up
		 * SVM on an object that already has a GPU address. If
		 * that happens don't bother walking the rest of the
		 * region
		 */
		if ((long) result == -EBUSY)
			return -EBUSY;

		trace_kgsl_mem_unmapped_area_collision(entry, cpu, len);

		if (cpu <= start) {
			result = -ENOMEM;
			break;
		}

		/* move downward to the next empty spot on the GPU */
		gpu = _gpu_find_svm(private, start, cpu, len, align);
		if (IS_ERR_VALUE(gpu)) {
			result = gpu;
			break;
		}

		/* Check that_gpu_find_svm doesn't put us in a loop */
		if (gpu >= cpu) {
			result = -ENOMEM;
			break;
		}

		/* Break if the recommended GPU address is out of range */
		if (gpu < start) {
			result = -ENOMEM;
			break;
		}

		/*
		 * Add the length of the chunk to the GPU address to yield the
		 * upper bound for the CPU search
		 */
		gpu += len;
	}
	return result;
}

static unsigned long _get_svm_area(struct kgsl_process_private *private,
		struct kgsl_mem_entry *entry, unsigned long hint,
		unsigned long len, unsigned long flags)
{
	uint64_t start, end;
	int align_shift = kgsl_memdesc_get_align(&entry->memdesc);
	uint64_t align;
	unsigned long result;
	unsigned long addr;

	if (align_shift >= ilog2(SZ_2M))
		align = SZ_2M;
	else if (align_shift >= ilog2(SZ_1M))
		align = SZ_1M;
	else if (align_shift >= ilog2(SZ_64K))
		align = SZ_64K;
	else
		align = SZ_4K;

	align = max_t(uint64_t, align, PAGE_SIZE);

	/* get the GPU pagetable's SVM range */
	if (kgsl_mmu_svm_range(private->pagetable, &start, &end,
				entry->memdesc.flags))
		return -ERANGE;

	/* now clamp the range based on the CPU's requirements */
	start = max_t(uint64_t, start, mmap_min_addr);
	end = min_t(uint64_t, end, current->mm->mmap_base);
	if (start >= end)
		return -ERANGE;

	if (flags & MAP_FIXED) {
		/* We must honor alignment requirements */
		if (!IS_ALIGNED(hint, align))
			return -EINVAL;

		/* we must use addr 'hint' or fail */
		return _gpu_set_svm_region(private, entry, hint, len);
	} else if (hint != 0) {
		struct vm_area_struct *vma;

		/*
		 * See if the hint is usable, if not we will use
		 * it as the start point for searching.
		 */
		addr = clamp_t(unsigned long, hint & ~(align - 1),
				start, (end - len) & ~(align - 1));

		vma = find_vma(current->mm, addr);

		if (vma == NULL || ((addr + len) <= vma->vm_start)) {
			result = _gpu_set_svm_region(private, entry, addr, len);

			/* On failure drop down to keep searching */
			if (!IS_ERR_VALUE(result))
				return result;
		}
	} else {
		/* no hint, start search at the top and work down */
		addr = end & ~(align - 1);
	}

	/*
	 * Search downwards from the hint first. If that fails we
	 * must try to search above it.
	 */
	result = _search_range(private, entry, start, addr, len, align);
	if (IS_ERR_VALUE(result) && hint != 0)
		result = _search_range(private, entry, addr, end, len, align);

	return result;
}

static unsigned long
kgsl_get_unmapped_area(struct file *file, unsigned long addr,
			unsigned long len, unsigned long pgoff,
			unsigned long flags)
{
	unsigned long val;
	unsigned long vma_offset = pgoff << PAGE_SHIFT;
	struct kgsl_device_private *dev_priv = file->private_data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_mem_entry *entry = NULL;

	if (vma_offset == (unsigned long) device->memstore.gpuaddr)
		return get_unmapped_area(NULL, addr, len, pgoff, flags);

	val = get_mmap_entry(private, &entry, pgoff, len);
	if (val)
		return val;

	/* Do not allow CPU mappings for secure buffers */
	if (kgsl_memdesc_is_secured(&entry->memdesc)) {
		val = -EPERM;
		goto put;
	}

	if (!kgsl_memdesc_use_cpu_map(&entry->memdesc)) {
		val = get_unmapped_area(NULL, addr, len, 0, flags);
		if (IS_ERR_VALUE(val))
			dev_err_ratelimited(device->dev,
					       "get_unmapped_area: pid %d addr %lx pgoff %lx len %ld failed error %d\n",
						pid_nr(private->pid), addr,
						pgoff, len, (int) val);
	} else {
		val = _get_svm_area(private, entry, addr, len, flags);
		if (IS_ERR_VALUE(val))
			dev_err_ratelimited(device->dev,
					       "_get_svm_area: pid %d mmap_base %lx addr %lx pgoff %lx len %ld failed error %d\n",
					       pid_nr(private->pid),
					       current->mm->mmap_base, addr,
					       pgoff, len, (int) val);

#if defined(CONFIG_DISPLAY_SAMSUNG)
		if (IS_ERR_VALUE(val)) {
			kgsl_svm_addr_mapping_log(device, pid_nr(private->pid));
			kgsl_svm_addr_hole_log(device, pid_nr(private->pid), entry->memdesc.flags);
		}
#endif
	}

put:
	kgsl_mem_entry_put(entry);
	return val;
}

static int kgsl_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned int ret, cache;
	unsigned long vma_offset = vma->vm_pgoff << PAGE_SHIFT;
	struct kgsl_device_private *dev_priv = file->private_data;
	struct kgsl_process_private *private = dev_priv->process_priv;
	struct kgsl_mem_entry *entry = NULL;
	struct kgsl_device *device = dev_priv->device;

	/* Handle leagacy behavior for memstore */

	if (vma_offset == (unsigned long) device->memstore.gpuaddr)
		return kgsl_mmap_memstore(device, vma);

	/*
	 * The reference count on the entry that we get from
	 * get_mmap_entry() will be held until kgsl_gpumem_vm_close().
	 */
	ret = get_mmap_entry(private, &entry, vma->vm_pgoff,
				vma->vm_end - vma->vm_start);
	if (ret)
		return ret;

	vma->vm_flags |= entry->memdesc.ops->vmflags;

	vma->vm_private_data = entry;

	/* Determine user-side caching policy */

	cache = kgsl_memdesc_get_cachemode(&entry->memdesc);

	switch (cache) {
	case KGSL_CACHEMODE_UNCACHED:
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		break;
	case KGSL_CACHEMODE_WRITETHROUGH:
		vma->vm_page_prot = pgprot_writethroughcache(vma->vm_page_prot);
		if (pgprot_val(vma->vm_page_prot) ==
			pgprot_val(pgprot_writebackcache(vma->vm_page_prot)))
			WARN_ONCE(1, "WRITETHROUGH is deprecated for arm64");
		break;
	case KGSL_CACHEMODE_WRITEBACK:
		vma->vm_page_prot = pgprot_writebackcache(vma->vm_page_prot);
		break;
	case KGSL_CACHEMODE_WRITECOMBINE:
	default:
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
		break;
	}

	vma->vm_ops = &kgsl_gpumem_vm_ops;

	if (cache == KGSL_CACHEMODE_WRITEBACK
		|| cache == KGSL_CACHEMODE_WRITETHROUGH) {
		int i;
		unsigned long addr = vma->vm_start;
		struct kgsl_memdesc *m = &entry->memdesc;

		for (i = 0; i < m->page_count; i++) {
			struct page *page = m->pages[i];

			vm_insert_page(vma, addr, page);
			addr += PAGE_SIZE;
		}
	}

	if (entry->memdesc.shmem_filp) {
		fput(vma->vm_file);
		vma->vm_file = get_file(entry->memdesc.shmem_filp);
	}

	/*
	 * kgsl gets the entry id or the gpu address through vm_pgoff.
	 * It is used during mmap and never needed again. But this vm_pgoff
	 * has different meaning at other parts of kernel. Not setting to
	 * zero will let way for wrong assumption when tried to unmap a page
	 * from this vma.
	 */
	vma->vm_pgoff = 0;

	if (atomic_inc_return(&entry->map_count) == 1)
		atomic_long_add(entry->memdesc.size,
				&entry->priv->gpumem_mapped);

	trace_kgsl_mem_mmap(entry, vma->vm_start);
	return 0;
}

static irqreturn_t kgsl_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;

	return device->ftbl->irq_handler(device);

}

#define KGSL_READ_MESSAGE "OH HAI GPU\n"

static ssize_t kgsl_read(struct file *filep, char __user *buf, size_t count,
		loff_t *pos)
{
	return simple_read_from_buffer(buf, count, pos,
			KGSL_READ_MESSAGE, strlen(KGSL_READ_MESSAGE) + 1);
}

static const struct file_operations kgsl_fops = {
	.owner = THIS_MODULE,
	.release = kgsl_release,
	.open = kgsl_open,
	.mmap = kgsl_mmap,
	.read = kgsl_read,
	.get_unmapped_area = kgsl_get_unmapped_area,
	.unlocked_ioctl = kgsl_ioctl,
	.compat_ioctl = kgsl_compat_ioctl,
};

struct kgsl_driver kgsl_driver  = {
	.process_mutex = __MUTEX_INITIALIZER(kgsl_driver.process_mutex),
	.proclist_lock = __SPIN_LOCK_UNLOCKED(kgsl_driver.proclist_lock),
	.ptlock = __SPIN_LOCK_UNLOCKED(kgsl_driver.ptlock),
	.devlock = __MUTEX_INITIALIZER(kgsl_driver.devlock),
	/*
	 * Full cache flushes are faster than line by line on at least
	 * 8064 and 8974 once the region to be flushed is > 16mb.
	 */
	.full_cache_threshold = SZ_16M,

	.stats.vmalloc = ATOMIC_LONG_INIT(0),
	.stats.vmalloc_max = ATOMIC_LONG_INIT(0),
	.stats.page_alloc = ATOMIC_LONG_INIT(0),
	.stats.page_alloc_max = ATOMIC_LONG_INIT(0),
	.stats.coherent = ATOMIC_LONG_INIT(0),
	.stats.coherent_max = ATOMIC_LONG_INIT(0),
	.stats.secure = ATOMIC_LONG_INIT(0),
	.stats.secure_max = ATOMIC_LONG_INIT(0),
	.stats.mapped = ATOMIC_LONG_INIT(0),
	.stats.mapped_max = ATOMIC_LONG_INIT(0),
};
EXPORT_SYMBOL(kgsl_driver);

static void _unregister_device(struct kgsl_device *device)
{
	int minor;

	mutex_lock(&kgsl_driver.devlock);
	for (minor = 0; minor < ARRAY_SIZE(kgsl_driver.devp); minor++) {
		if (device == kgsl_driver.devp[minor]) {
			device_destroy(kgsl_driver.class,
				MKDEV(MAJOR(kgsl_driver.major), minor));
			kgsl_driver.devp[minor] = NULL;
			break;
		}
	}
	mutex_unlock(&kgsl_driver.devlock);
}

static int _register_device(struct kgsl_device *device)
{
	static u64 dma_mask = DMA_BIT_MASK(64);
	int minor, ret;
	dev_t dev;

	/* Find a minor for the device */

	mutex_lock(&kgsl_driver.devlock);
	for (minor = 0; minor < ARRAY_SIZE(kgsl_driver.devp); minor++) {
		if (kgsl_driver.devp[minor] == NULL) {
			kgsl_driver.devp[minor] = device;
			break;
		}
	}
	mutex_unlock(&kgsl_driver.devlock);

	if (minor == ARRAY_SIZE(kgsl_driver.devp)) {
		pr_err("kgsl: minor devices exhausted\n");
		return -ENODEV;
	}

	/* Create the device */
	dev = MKDEV(MAJOR(kgsl_driver.major), minor);
	device->dev = device_create(kgsl_driver.class,
				    &device->pdev->dev,
				    dev, device,
				    "%s", device->name);

	if (IS_ERR(device->dev)) {
		mutex_lock(&kgsl_driver.devlock);
		kgsl_driver.devp[minor] = NULL;
		mutex_unlock(&kgsl_driver.devlock);
		ret = PTR_ERR(device->dev);
		pr_err("kgsl: device_create(%s): %d\n", device->name, ret);
		return ret;
	}

	device->dev->dma_mask = &dma_mask;
	arch_setup_dma_ops(device->dev, 0, 0, NULL, false);

	dev_set_drvdata(&device->pdev->dev, device);
	return 0;
}

int kgsl_request_irq(struct platform_device *pdev, const  char *name,
		irq_handler_t handler, void *data)
{
	int ret, num = platform_get_irq_byname(pdev, name);

	if (num < 0)
		return num;

	ret = devm_request_irq(&pdev->dev, num, handler, IRQF_TRIGGER_HIGH,
		name, data);

	if (ret)
		dev_err(&pdev->dev, "Unable to get interrupt %s: %d\n",
			name, ret);

	return ret ? ret : num;
}

int kgsl_of_property_read_ddrtype(struct device_node *node, const char *base,
		u32 *ptr)
{
	char str[32];
	int ddr = of_fdt_get_ddrtype();

	/* of_fdt_get_ddrtype returns error if the DDR type isn't determined */
	if (ddr >= 0) {
		int ret;

		/* Construct expanded string for the DDR type  */
		ret = snprintf(str, sizeof(str), "%s-ddr%d", base, ddr);

		/* WARN_ON() if the array size was too small for the string */
		if (WARN_ON(ret > sizeof(str)))
			return -ENOMEM;

		/* Read the expanded string */
		if (!of_property_read_u32(node, str, ptr))
			return 0;
	}

	/* Read the default string */
	return of_property_read_u32(node, base, ptr);
}

int kgsl_device_platform_probe(struct kgsl_device *device)
{
	int status = -EINVAL;
	int cpu;

	status = _register_device(device);
	if (status)
		return status;

	/* Disable the sparse ioctl invocation as they are not used */
	device->flags &= ~KGSL_FLAG_SPARSE;

	kgsl_device_debugfs_init(device);

	status = kgsl_pwrctrl_init(device);
	if (status)
		goto error;

	if (!devm_request_mem_region(device->dev, device->reg_phys,
				device->reg_len, device->name)) {
		dev_err(device->dev, "request_mem_region failed\n");
		status = -ENODEV;
		goto error_pwrctrl_close;
	}

	device->reg_virt = devm_ioremap(device->dev, device->reg_phys,
					device->reg_len);

	if (device->reg_virt == NULL) {
		dev_err(device->dev, "ioremap failed\n");
		status = -ENODEV;
		goto error_pwrctrl_close;
	}

	status = kgsl_request_irq(device->pdev, device->pwrctrl.irq_name,
		kgsl_irq_handler, device);
	if (status < 0)
		goto error_pwrctrl_close;

	device->pwrctrl.interrupt_num = status;
	disable_irq(device->pwrctrl.interrupt_num);

	rwlock_init(&device->context_lock);
	spin_lock_init(&device->submit_lock);

	timer_setup(&device->idle_timer, kgsl_timer, 0);

	status = kgsl_mmu_probe(device);
	if (status != 0)
		goto error_pwrctrl_close;

	/* Check to see if our device can perform DMA correctly */
	status = dma_set_coherent_mask(&device->pdev->dev, KGSL_DMA_BIT_MASK);
	if (status)
		goto error_close_mmu;

	/* Allocate memory for dma_parms and set the max_seg_size */
	device->dev->dma_parms =
		kzalloc(sizeof(*device->dev->dma_parms), GFP_KERNEL);

	dma_set_max_seg_size(device->dev, KGSL_DMA_BIT_MASK);

	/* Initialize the memory pools */
	kgsl_init_page_pools(device);

	status = kgsl_reclaim_init(device);
	if (status)
		goto error_close_mmu;

	/*
	 * The default request type PM_QOS_REQ_ALL_CORES is
	 * applicable to all CPU cores that are online and
	 * would have a power impact when there are more
	 * number of CPUs. PM_QOS_REQ_AFFINE_IRQ request
	 * type shall update/apply the vote only to that CPU to
	 * which IRQ's affinity is set to.
	 */
#ifdef CONFIG_SMP
#ifdef CONFIG_DISPLAY_SAMSUNG
	device->pwrctrl.pm_qos_req_dma.type = PM_QOS_REQ_AFFINE_CORES;
	cpumask_empty(&device->pwrctrl.pm_qos_req_dma.cpus_affine);
	for_each_possible_cpu(cpu) {
		if ((1 << cpu) & 0xf)
			cpumask_set_cpu(cpu, &device->pwrctrl.pm_qos_req_dma.cpus_affine);
	}
#else
	device->pwrctrl.pm_qos_req_dma.type = PM_QOS_REQ_AFFINE_IRQ;
	device->pwrctrl.pm_qos_req_dma.irq = device->pwrctrl.interrupt_num;
#endif

#endif
	pm_qos_add_request(&device->pwrctrl.pm_qos_req_dma,
				PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);

	if (device->pwrctrl.l2pc_cpus_mask) {
		struct pm_qos_request *qos = &device->pwrctrl.l2pc_cpus_qos;

		qos->type = PM_QOS_REQ_AFFINE_CORES;

		cpumask_empty(&qos->cpus_affine);
		for_each_possible_cpu(cpu) {
			if ((1 << cpu) & device->pwrctrl.l2pc_cpus_mask)
				cpumask_set_cpu(cpu, &qos->cpus_affine);
		}

		pm_qos_add_request(&device->pwrctrl.l2pc_cpus_qos,
				PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);
	}

	device->events_wq = alloc_workqueue("kgsl-events",
		WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS | WQ_HIGHPRI, 0);

	/* Initialize the snapshot engine */
	kgsl_device_snapshot_init(device);

	/* Initialize common sysfs entries */
	kgsl_pwrctrl_init_sysfs(device);

	return 0;

error_close_mmu:
	kgsl_mmu_close(device);
error_pwrctrl_close:
	kgsl_pwrctrl_close(device);
error:
	kgsl_device_debugfs_close(device);
	_unregister_device(device);
	return status;
}
EXPORT_SYMBOL(kgsl_device_platform_probe);

void kgsl_device_platform_remove(struct kgsl_device *device)
{
	destroy_workqueue(device->events_wq);

	kfree(device->dev->dma_parms);
	device->dev->dma_parms = NULL;

	kgsl_device_snapshot_close(device);

	kgsl_exit_page_pools();

	kgsl_pwrctrl_uninit_sysfs(device);

	pm_qos_remove_request(&device->pwrctrl.pm_qos_req_dma);
	if (device->pwrctrl.l2pc_cpus_mask)
		pm_qos_remove_request(&device->pwrctrl.l2pc_cpus_qos);

	idr_destroy(&device->context_idr);

	kgsl_mmu_close(device);

	kgsl_pwrctrl_close(device);

	kgsl_device_debugfs_close(device);
	_unregister_device(device);
}
EXPORT_SYMBOL(kgsl_device_platform_remove);

static void
_flush_mem_workqueue(struct work_struct *work)
{
	flush_workqueue(kgsl_driver.mem_workqueue);
}

static int kgsl_sharedmem_size_notifier(struct notifier_block *nb,
					unsigned long action, void *data)
{
	struct seq_file *s;

	s = (struct seq_file *)data;
	if (s != NULL)
		seq_printf(s, "KgslSharedmem:  %8lu kB\n",
			atomic_long_read(&kgsl_driver.stats.page_alloc) >> 10);
	else
		pr_cont("KgslSharedmem:%lukB ",
			atomic_long_read(&kgsl_driver.stats.page_alloc) >> 10);
	return 0;
}

static struct notifier_block kgsl_sharedmem_size_nb = {
	.notifier_call = kgsl_sharedmem_size_notifier,
};

static void kgsl_core_exit(void)
{
	kgsl_events_exit();
	kgsl_core_debugfs_close();

	kgsl_reclaim_close();

	/*
	 * We call kgsl_sharedmem_uninit_sysfs() and device_unregister()
	 * only if kgsl_driver.virtdev has been populated.
	 * We check at least one member of kgsl_driver.virtdev to
	 * see if it is not NULL (and thus, has been populated).
	 */
	if (kgsl_driver.virtdev.class) {
		kgsl_sharedmem_uninit_sysfs();
		device_unregister(&kgsl_driver.virtdev);
	}

	if (kgsl_driver.class) {
		class_destroy(kgsl_driver.class);
		kgsl_driver.class = NULL;
	}

	kgsl_drawobjs_cache_exit();

	kfree(memfree.list);
	memset(&memfree, 0, sizeof(memfree));
	show_mem_extra_notifier_unregister(&kgsl_sharedmem_size_nb);

	unregister_chrdev_region(kgsl_driver.major,
		ARRAY_SIZE(kgsl_driver.devp));
}

static int __init kgsl_core_init(void)
{
	int result = 0;
	struct sched_param param = { .sched_priority = 2 };

	/* alloc major and minor device numbers */
	result = alloc_chrdev_region(&kgsl_driver.major, 0,
		ARRAY_SIZE(kgsl_driver.devp), "kgsl");

	if (result < 0) {

		pr_err("kgsl: alloc_chrdev_region failed err = %d\n", result);
		goto err;
	}

	cdev_init(&kgsl_driver.cdev, &kgsl_fops);
	kgsl_driver.cdev.owner = THIS_MODULE;
	kgsl_driver.cdev.ops = &kgsl_fops;
	result = cdev_add(&kgsl_driver.cdev, MKDEV(MAJOR(kgsl_driver.major), 0),
		ARRAY_SIZE(kgsl_driver.devp));

	if (result) {
		pr_err("kgsl: cdev_add() failed, dev_num= %d,result= %d\n",
				kgsl_driver.major, result);
		goto err;
	}

	kgsl_driver.class = class_create(THIS_MODULE, "kgsl");

	if (IS_ERR(kgsl_driver.class)) {
		result = PTR_ERR(kgsl_driver.class);
		pr_err("kgsl: failed to create class for kgsl\n");
		goto err;
	}

	/*
	 * Make a virtual device for managing core related things
	 * in sysfs
	 */
	kgsl_driver.virtdev.class = kgsl_driver.class;
	dev_set_name(&kgsl_driver.virtdev, "kgsl");
	result = device_register(&kgsl_driver.virtdev);
	if (result) {
		pr_err("kgsl: driver_register failed\n");
		goto err;
	}

	/* Make kobjects in the virtual device for storing statistics */

	kgsl_driver.ptkobj =
	  kobject_create_and_add("pagetables",
				 &kgsl_driver.virtdev.kobj);

	kgsl_driver.prockobj =
		kobject_create_and_add("proc",
				       &kgsl_driver.virtdev.kobj);

	kgsl_core_debugfs_init();

	kgsl_sharedmem_init_sysfs();

	INIT_LIST_HEAD(&kgsl_driver.process_list);

	INIT_LIST_HEAD(&kgsl_driver.pagetable_list);

	kgsl_driver.workqueue = alloc_workqueue("kgsl-workqueue",
		WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS, 0);

	kgsl_driver.mem_workqueue = alloc_workqueue("kgsl-mementry",
		WQ_UNBOUND | WQ_MEM_RECLAIM, 0);

	INIT_WORK(&kgsl_driver.mem_work, _flush_mem_workqueue);

	kthread_init_worker(&kgsl_driver.worker);

	kgsl_driver.worker_thread = kthread_run(kthread_worker_fn,
		&kgsl_driver.worker, "kgsl_worker_thread");

	if (IS_ERR(kgsl_driver.worker_thread)) {
		pr_err("kgsl: unable to start kgsl thread\n");
		goto err;
	}

	sched_setscheduler(kgsl_driver.worker_thread, SCHED_FIFO, &param);

	kgsl_events_init();

	result = kgsl_drawobjs_cache_init();
	if (result)
		goto err;

	memfree.list = kcalloc(MEMFREE_ENTRIES, sizeof(struct memfree_entry),
		GFP_KERNEL);

	show_mem_extra_notifier_register(&kgsl_sharedmem_size_nb);

	return 0;

err:
	kgsl_core_exit();
	return result;
}

module_init(kgsl_core_init);
module_exit(kgsl_core_exit);

MODULE_DESCRIPTION("MSM GPU driver");
MODULE_LICENSE("GPL v2");
