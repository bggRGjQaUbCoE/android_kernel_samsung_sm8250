// SPDX-License-Identifier: GPL-2.0
/* kernel/rwsem.c: R/W semaphores, public implementation
 *
 * Written by David Howells (dhowells@redhat.com).
 * Derived from asm-i386/semaphore.h
 *
 * Writer lock-stealing by Alex Shi <alex.shi@intel.com>
 * and Michel Lespinasse <walken@google.com>
 *
 * Optimistic spinning by Tim Chen <tim.c.chen@intel.com>
 * and Davidlohr Bueso <davidlohr@hp.com>. Based on mutexes.
 *
 * Rwsem count bit fields re-definition and rwsem rearchitecture by
 * Waiman Long <longman@redhat.com> and
 * Peter Zijlstra <peterz@infradead.org>.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/sched/task.h>
#include <linux/sched/debug.h>
#include <linux/sched/wake_q.h>
#include <linux/sched/signal.h>
#include <linux/sched/clock.h>
#include <linux/export.h>
#include <linux/rwsem.h>
#include <linux/atomic.h>

#include "rwsem.h"

/*
 * The least significant 3 bits of the owner value has the following
 * meanings when set.
 *  - Bit 0: RWSEM_READER_OWNED - The rwsem is owned by readers
 *  - Bit 1: RWSEM_RD_NONSPINNABLE - Readers cannot spin on this lock.
 *  - Bit 2: RWSEM_WR_NONSPINNABLE - Writers cannot spin on this lock.
 *
 * When the rwsem is either owned by an anonymous writer, or it is
 * reader-owned, but a spinning writer has timed out, both nonspinnable
 * bits will be set to disable optimistic spinning by readers and writers.
 * In the later case, the last unlocking reader should then check the
 * writer nonspinnable bit and clear it only to give writers preference
 * to acquire the lock via optimistic spinning, but not readers. Similar
 * action is also done in the reader slowpath.

 * When a writer acquires a rwsem, it puts its task_struct pointer
 * into the owner field. It is cleared after an unlock.
 *
 * When a reader acquires a rwsem, it will also puts its task_struct
 * pointer into the owner field with the RWSEM_READER_OWNED bit set.
 * On unlock, the owner field will largely be left untouched. So
 * for a free or reader-owned rwsem, the owner value may contain
 * information about the last reader that acquires the rwsem.
 *
 * That information may be helpful in debugging cases where the system
 * seems to hang on a reader owned rwsem especially if only one reader
 * is involved. Ideally we would like to track all the readers that own
 * a rwsem, but the overhead is simply too big.
 */
#define RWSEM_READER_OWNED	(1UL << 0)
#define RWSEM_RD_NONSPINNABLE	(1UL << 1)
#define RWSEM_WR_NONSPINNABLE	(1UL << 2)
#define RWSEM_NONSPINNABLE	(RWSEM_RD_NONSPINNABLE | RWSEM_WR_NONSPINNABLE)
#define RWSEM_OWNER_FLAGS_MASK	(RWSEM_READER_OWNED | RWSEM_NONSPINNABLE)

enum rwsem_waiter_type {
	RWSEM_WAITING_FOR_WRITE,
	RWSEM_WAITING_FOR_READ
};

struct rwsem_waiter {
	struct list_head list;
	struct task_struct *task;
	enum rwsem_waiter_type type;
	unsigned long timeout;
};
#define rwsem_first_waiter(sem) \
	list_first_entry(&sem->wait_list, struct rwsem_waiter, list)

#ifdef CONFIG_DEBUG_RWSEMS
# define DEBUG_RWSEMS_WARN_ON(c, sem)	do {			\
	if (!debug_locks_silent &&				\
	    WARN_ONCE(c, "DEBUG_RWSEMS_WARN_ON(%s): count = 0x%lx, owner = 0x%lx, curr 0x%lx, list %sempty\n",\
		#c, atomic_long_read(&(sem)->count),		\
		atomic_long_read(&(sem)->owner), (long)current,	\
		list_empty(&(sem)->wait_list) ? "" : "not "))	\
			debug_locks_off();			\
	} while (0)
#else
# define DEBUG_RWSEMS_WARN_ON(c, sem)
#endif

/*
 * The definition of the atomic counter in the semaphore:
 *
 * Bit  0   - writer locked bit
 * Bit  1   - waiters present bit
 * Bit  2   - lock handoff bit
 * Bits 3-7 - reserved
 * Bits 8-X - 24-bit (32-bit) or 56-bit reader count
 *
 * atomic_long_fetch_add() is used to obtain reader lock, whereas
 * atomic_long_cmpxchg() will be used to obtain writer lock.
 *
 * There are three places where the lock handoff bit may be set or cleared.
 * 1) rwsem_mark_wake() for readers.
 * 2) rwsem_try_write_lock() for writers.
 * 3) Error path of rwsem_down_write_slowpath().
 *
 * For all the above cases, wait_lock will be held. A writer must also
 * be the first one in the wait_list to be eligible for setting the handoff
 * bit. So concurrent setting/clearing of handoff bit is not possible.
 */
#define RWSEM_WRITER_LOCKED	(1UL << 0)
#define RWSEM_FLAG_WAITERS	(1UL << 1)
#define RWSEM_FLAG_HANDOFF	(1UL << 2)

#define RWSEM_READER_SHIFT	8
#define RWSEM_READER_BIAS	(1UL << RWSEM_READER_SHIFT)
#define RWSEM_READER_MASK	(~(RWSEM_READER_BIAS - 1))
#define RWSEM_WRITER_MASK	RWSEM_WRITER_LOCKED
#define RWSEM_LOCK_MASK		(RWSEM_WRITER_MASK|RWSEM_READER_MASK)
#define RWSEM_READ_FAILED_MASK	(RWSEM_WRITER_MASK|RWSEM_FLAG_WAITERS|\
				 RWSEM_FLAG_HANDOFF)

#ifdef CONFIG_RWSEM_SPIN_ON_OWNER
/*
 * All writes to owner are protected by WRITE_ONCE() to make sure that
 * store tearing can't happen as optimistic spinners may read and use
 * the owner value concurrently without lock. Read from owner, however,
 * may not need READ_ONCE() as long as the pointer value is only used
 * for comparison and isn't being dereferenced.
 */
static inline void rwsem_set_owner(struct rw_semaphore *sem)
{
	atomic_long_set(&sem->owner, (long)current);
}

static inline void rwsem_clear_owner(struct rw_semaphore *sem)
{
	atomic_long_set(&sem->owner, 0);
}

/*
 * Test the flags in the owner field.
 */
static inline bool rwsem_test_oflags(struct rw_semaphore *sem, long flags)
{
	return atomic_long_read(&sem->owner) & flags;
}

/*
 * The task_struct pointer of the last owning reader will be left in
 * the owner field.
 *
 * Note that the owner value just indicates the task has owned the rwsem
 * previously, it may not be the real owner or one of the real owners
 * anymore when that field is examined, so take it with a grain of salt.
 */
static inline void __rwsem_set_reader_owned(struct rw_semaphore *sem,
					    struct task_struct *owner)
{
	unsigned long val = (unsigned long)owner | RWSEM_READER_OWNED;

	atomic_long_set(&sem->owner, val);
}

static inline void rwsem_set_reader_owned(struct rw_semaphore *sem)
{
	__rwsem_set_reader_owned(sem, current);
}

/*
 * Return true if the rwsem is owned by a reader.
 */
static inline bool is_rwsem_reader_owned(struct rw_semaphore *sem)
{
#ifdef CONFIG_DEBUG_RWSEMS
	/*
	 * Check the count to see if it is write-locked.
	 */
	long count = atomic_long_read(&sem->count);

	if (count & RWSEM_WRITER_MASK)
		return false;
#endif
	return rwsem_test_oflags(sem, RWSEM_READER_OWNED);
}

#ifdef CONFIG_DEBUG_RWSEMS
/*
 * With CONFIG_DEBUG_RWSEMS configured, it will make sure that if there
 * is a task pointer in owner of a reader-owned rwsem, it will be the
 * real owner or one of the real owners. The only exception is when the
 * unlock is done by up_read_non_owner().
 */
#define rwsem_clear_reader_owned rwsem_clear_reader_owned
static inline void rwsem_clear_reader_owned(struct rw_semaphore *sem)
{
	unsigned long val = atomic_long_read(&sem->owner);

	while ((val & ~RWSEM_OWNER_FLAGS_MASK) == (unsigned long)current) {
		if (atomic_long_try_cmpxchg(&sem->owner, &val,
					    val & RWSEM_OWNER_FLAGS_MASK))
			return;
	}
}
#endif

#else
static inline void rwsem_set_owner(struct rw_semaphore *sem)
{
}

static inline void rwsem_clear_owner(struct rw_semaphore *sem)
{
}

static inline void __rwsem_set_reader_owned(struct rw_semaphore *sem,
					   struct task_struct *owner)
{
}

static inline void rwsem_set_reader_owned(struct rw_semaphore *sem)
{
}
#endif

#ifndef rwsem_clear_reader_owned
static inline void rwsem_clear_reader_owned(struct rw_semaphore *sem)
{
}
#endif

#ifdef CONFIG_RWSEM_PRIO_AWARE

#define RWSEM_MAX_PREEMPT_ALLOWED 3000

/*
 * Return true if current waiter is added in the front of the rwsem wait list.
 */
static inline bool rwsem_list_add_per_prio(struct rwsem_waiter *waiter_in,
				    struct rw_semaphore *sem)
{
	struct list_head *pos;
	struct list_head *head;
	struct rwsem_waiter *waiter = NULL;

	pos = head = &sem->wait_list;
	/*
	 * Rules for task prio aware rwsem wait list queueing:
	 * 1:	Only try to preempt waiters with which task priority
	 *	which is higher than DEFAULT_PRIO.
	 * 2:	To avoid starvation, add count to record
	 *	how many high priority waiters preempt to queue in wait
	 *	list.
	 *	If preempt count is exceed RWSEM_MAX_PREEMPT_ALLOWED,
	 *	use simple fifo until wait list is empty.
	 */
	if (list_empty(head)) {
		list_add_tail(&waiter_in->list, head);
		sem->m_count = 0;
		return true;
	}

	if (waiter_in->task->prio < DEFAULT_PRIO
		&& sem->m_count < RWSEM_MAX_PREEMPT_ALLOWED) {

		list_for_each(pos, head) {
			waiter = list_entry(pos, struct rwsem_waiter, list);
			if (waiter->task->prio > waiter_in->task->prio) {
				list_add(&waiter_in->list, pos->prev);
				sem->m_count++;
				return &waiter_in->list == head->next;
			}
		}
	}

	list_add_tail(&waiter_in->list, head);

	return false;
}
#else
static inline bool rwsem_list_add_per_prio(struct rwsem_waiter *waiter_in,
				    struct rw_semaphore *sem)
{
	list_add_tail(&waiter_in->list, &sem->wait_list);
	return false;
}
#endif

/*
 * Set the RWSEM_NONSPINNABLE bits if the RWSEM_READER_OWNED flag
 * remains set. Otherwise, the operation will be aborted.
 */
static inline void rwsem_set_nonspinnable(struct rw_semaphore *sem)
{
	unsigned long owner = atomic_long_read(&sem->owner);

	do {
		if (!(owner & RWSEM_READER_OWNED))
			break;
		if (owner & RWSEM_NONSPINNABLE)
			break;
	} while (!atomic_long_try_cmpxchg(&sem->owner, &owner,
					  owner | RWSEM_NONSPINNABLE));
}

/*
 * Return just the real task structure pointer of the owner
 */
static inline struct task_struct *rwsem_owner(struct rw_semaphore *sem)
{
	return (struct task_struct *)
		(atomic_long_read(&sem->owner) & ~RWSEM_OWNER_FLAGS_MASK);
}

/*
 * Return the real task structure pointer of the owner and the embedded
 * flags in the owner. pflags must be non-NULL.
 */
static inline struct task_struct *
rwsem_owner_flags(struct rw_semaphore *sem, unsigned long *pflags)
{
	unsigned long owner = atomic_long_read(&sem->owner);

	*pflags = owner & RWSEM_OWNER_FLAGS_MASK;
	return (struct task_struct *)(owner & ~RWSEM_OWNER_FLAGS_MASK);
}

/*
 * Guide to the rw_semaphore's count field.
 *
 * When the RWSEM_WRITER_LOCKED bit in count is set, the lock is owned
 * by a writer.
 *
 * The lock is owned by readers when
 * (1) the RWSEM_WRITER_LOCKED isn't set in count,
 * (2) some of the reader bits are set in count, and
 * (3) the owner field has RWSEM_READ_OWNED bit set.
 *
 * Having some reader bits set is not enough to guarantee a readers owned
 * lock as the readers may be in the process of backing out from the count
 * and a writer has just released the lock. So another writer may steal
 * the lock immediately after that.
 */

/*
 * Initialize an rwsem:
 */
void __init_rwsem(struct rw_semaphore *sem, const char *name,
		  struct lock_class_key *key)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/*
	 * Make sure we are not reinitializing a held semaphore:
	 */
	debug_check_no_locks_freed((void *)sem, sizeof(*sem));
	lockdep_init_map(&sem->dep_map, name, key, 0);
#endif
	atomic_long_set(&sem->count, RWSEM_UNLOCKED_VALUE);
	raw_spin_lock_init(&sem->wait_lock);
	INIT_LIST_HEAD(&sem->wait_list);
#ifdef CONFIG_RWSEM_SPIN_ON_OWNER
	atomic_long_set(&sem->owner, 0L);
	osq_lock_init(&sem->osq);
#endif
#ifdef CONFIG_RWSEM_PRIO_AWARE
	sem->m_count = 0;
#endif
}
EXPORT_SYMBOL(__init_rwsem);

enum rwsem_wake_type {
	RWSEM_WAKE_ANY,		/* Wake whatever's at head of wait list */
	RWSEM_WAKE_READERS,	/* Wake readers only */
	RWSEM_WAKE_READ_OWNED	/* Waker thread holds the read lock */
};

enum writer_wait_state {
	WRITER_NOT_FIRST,	/* Writer is not first in wait list */
	WRITER_FIRST,		/* Writer is first in wait list     */
	WRITER_HANDOFF		/* Writer is first & handoff needed */
};

/*
 * The typical HZ value is either 250 or 1000. So set the minimum waiting
 * time to at least 4ms or 1 jiffy (if it is higher than 4ms) in the wait
 * queue before initiating the handoff protocol.
 */
#define RWSEM_WAIT_TIMEOUT	DIV_ROUND_UP(HZ, 250)

/*
 * Magic number to batch-wakeup waiting readers, even when writers are
 * also present in the queue. This both limits the amount of work the
 * waking thread must do and also prevents any potential counter overflow,
 * however unlikely.
 */
#define MAX_READERS_WAKEUP	0x100

/*
 * handle the lock release when processes blocked on it that can now run
 * - if we come here from up_xxxx(), then the RWSEM_FLAG_WAITERS bit must
 *   have been set.
 * - there must be someone on the queue
 * - the wait_lock must be held by the caller
 * - tasks are marked for wakeup, the caller must later invoke wake_up_q()
 *   to actually wakeup the blocked task(s) and drop the reference count,
 *   preferably when the wait_lock is released
 * - woken process blocks are discarded from the list after having task zeroed
 * - writers are only marked woken if downgrading is false
 */
static void rwsem_mark_wake(struct rw_semaphore *sem,
			    enum rwsem_wake_type wake_type,
			    struct wake_q_head *wake_q)
{
	struct rwsem_waiter *waiter, *tmp;
	long oldcount, woken = 0, adjustment = 0;
	struct list_head wlist;

	lockdep_assert_held(&sem->wait_lock);

	/*
	 * Take a peek at the queue head waiter such that we can determine
	 * the wakeup(s) to perform.
	 */
	waiter = rwsem_first_waiter(sem);

	if (waiter->type == RWSEM_WAITING_FOR_WRITE) {
		if (wake_type == RWSEM_WAKE_ANY) {
			/*
			 * Mark writer at the front of the queue for wakeup.
			 * Until the task is actually later awoken later by
			 * the caller, other writers are able to steal it.
			 * Readers, on the other hand, will block as they
			 * will notice the queued writer.
			 */
			wake_q_add(wake_q, waiter->task);
		}

		return;
	}

	/*
	 * Writers might steal the lock before we grant it to the next reader.
	 * We prefer to do the first reader grant before counting readers
	 * so we can bail out early if a writer stole the lock.
	 */
	if (wake_type != RWSEM_WAKE_READ_OWNED) {
		adjustment = RWSEM_READER_BIAS;
		oldcount = atomic_long_fetch_add(adjustment, &sem->count);
		if (unlikely(oldcount & RWSEM_WRITER_MASK)) {
			/*
			 * When we've been waiting "too" long (for writers
			 * to give up the lock), request a HANDOFF to
			 * force the issue.
			 */
			if (!(oldcount & RWSEM_FLAG_HANDOFF) &&
			    time_after(jiffies, waiter->timeout)) {
				adjustment -= RWSEM_FLAG_HANDOFF;
			}

			atomic_long_add(-adjustment, &sem->count);
			return;
		}
		/*
		 * Set it to reader-owned to give spinners an early
		 * indication that readers now have the lock.
		 */
		__rwsem_set_reader_owned(sem, waiter->task);
	}

	/*
	 * Grant up to MAX_READERS_WAKEUP read locks to all the readers in the
	 * queue. We know that the woken will be at least 1 as we accounted
	 * for above. Note we increment the 'active part' of the count by the
	 * number of readers before waking any processes up.
	 *
	 * This is an adaptation of the phase-fair R/W locks where at the
	 * reader phase (first waiter is a reader), all readers are eligible
	 * to acquire the lock at the same time irrespective of their order
	 * in the queue. The writers acquire the lock according to their
	 * order in the queue.
	 *
	 * We have to do wakeup in 2 passes to prevent the possibility that
	 * the reader count may be decremented before it is incremented. It
	 * is because the to-be-woken waiter may not have slept yet. So it
	 * may see waiter->task got cleared, finish its critical section and
	 * do an unlock before the reader count increment.
	 *
	 * 1) Collect the read-waiters in a separate list, count them and
	 *    fully increment the reader count in rwsem.
	 * 2) For each waiters in the new list, clear waiter->task and
	 *    put them into wake_q to be woken up later.
	 */
	INIT_LIST_HEAD(&wlist);
	list_for_each_entry_safe(waiter, tmp, &sem->wait_list, list) {
		if (waiter->type == RWSEM_WAITING_FOR_WRITE)
			continue;

		woken++;
		list_move_tail(&waiter->list, &wlist);

		/*
		 * Limit # of readers that can be woken up per wakeup call.
		 */
		if (woken >= MAX_READERS_WAKEUP)
			break;
	}

	adjustment = woken * RWSEM_READER_BIAS - adjustment;
	if (list_empty(&sem->wait_list)) {
		/* hit end of list above */
		adjustment -= RWSEM_FLAG_WAITERS;
	}

	/*
	 * When we've woken a reader, we no longer need to force writers
	 * to give up the lock and we can clear HANDOFF.
	 */
	if (woken && (atomic_long_read(&sem->count) & RWSEM_FLAG_HANDOFF))
		adjustment -= RWSEM_FLAG_HANDOFF;

	if (adjustment)
		atomic_long_add(adjustment, &sem->count);

	/* 2nd pass */
	list_for_each_entry_safe(waiter, tmp, &wlist, list) {
		struct task_struct *tsk;

		tsk = waiter->task;
		get_task_struct(tsk);

		/*
		 * Ensure calling get_task_struct() before setting the reader
		 * waiter to nil such that rwsem_down_read_slowpath() cannot
		 * race with do_exit() by always holding a reference count
		 * to the task to wakeup.
		 */
		smp_store_release(&waiter->task, NULL);
		/*
		 * Ensure issuing the wakeup (either by us or someone else)
		 * after setting the reader waiter to nil.
		 */
		wake_q_add(wake_q, tsk);
		/* wake_q_add() already take the task ref */
		put_task_struct(tsk);
	}
}

/*
 * This function must be called with the sem->wait_lock held to prevent
 * race conditions between checking the rwsem wait list and setting the
 * sem->count accordingly.
 *
 * If wstate is WRITER_HANDOFF, it will make sure that either the handoff
 * bit is set or the lock is acquired with handoff bit cleared.
 */
static inline bool rwsem_try_write_lock(struct rw_semaphore *sem,
					enum writer_wait_state wstate)
{
	long count, new;

	lockdep_assert_held(&sem->wait_lock);

	count = atomic_long_read(&sem->count);
	do {
		bool has_handoff = !!(count & RWSEM_FLAG_HANDOFF);

		if (has_handoff && wstate == WRITER_NOT_FIRST)
			return false;

		new = count;

		if (count & RWSEM_LOCK_MASK) {
			if (has_handoff || (wstate != WRITER_HANDOFF))
				return false;

			new |= RWSEM_FLAG_HANDOFF;
		} else {
			new |= RWSEM_WRITER_LOCKED;
			new &= ~RWSEM_FLAG_HANDOFF;

			if (list_is_singular(&sem->wait_list))
				new &= ~RWSEM_FLAG_WAITERS;
		}
	} while (!atomic_long_try_cmpxchg_acquire(&sem->count, &count, new));

	/*
	 * We have either acquired the lock with handoff bit cleared or
	 * set the handoff bit.
	 */
	if (new & RWSEM_FLAG_HANDOFF)
		return false;

	rwsem_set_owner(sem);
	return true;
}

#ifdef CONFIG_RWSEM_SPIN_ON_OWNER
/*
 * Try to acquire read lock before the reader is put on wait queue.
 * Lock acquisition isn't allowed if the rwsem is locked or a writer handoff
 * is ongoing.
 */
static inline bool rwsem_try_read_lock_unqueued(struct rw_semaphore *sem)
{
	long count = atomic_long_read(&sem->count);

	if (count & (RWSEM_WRITER_MASK | RWSEM_FLAG_HANDOFF))
		return false;

	count = atomic_long_fetch_add_acquire(RWSEM_READER_BIAS, &sem->count);
	if (!(count & (RWSEM_WRITER_MASK | RWSEM_FLAG_HANDOFF))) {
		rwsem_set_reader_owned(sem);
		return true;
	}

	/* Back out the change */
	atomic_long_add(-RWSEM_READER_BIAS, &sem->count);
	return false;
}

/*
 * Try to acquire write lock before the writer has been put on wait queue.
 */
static inline bool rwsem_try_write_lock_unqueued(struct rw_semaphore *sem)
{
	long count = atomic_long_read(&sem->count);

	while (!(count & (RWSEM_LOCK_MASK|RWSEM_FLAG_HANDOFF))) {
		if (atomic_long_try_cmpxchg_acquire(&sem->count, &count,
					count | RWSEM_WRITER_LOCKED)) {
			rwsem_set_owner(sem);
			return true;
		}
	}
	return false;
}

static inline bool owner_on_cpu(struct task_struct *owner)
{
	/*
	 * As lock holder preemption issue, we both skip spinning if
	 * task is not on cpu or its cpu is preempted
	 */
	return owner->on_cpu && !vcpu_is_preempted(task_cpu(owner));
}

static inline bool rwsem_can_spin_on_owner(struct rw_semaphore *sem,
					   unsigned long nonspinnable)
{
	struct task_struct *owner;
	unsigned long flags;
	bool ret = true;

	BUILD_BUG_ON(!(RWSEM_OWNER_UNKNOWN & RWSEM_NONSPINNABLE));

	if (need_resched())
		return false;

	preempt_disable();
	rcu_read_lock();
	owner = rwsem_owner_flags(sem, &flags);
	if ((flags & nonspinnable) || (owner && !owner_on_cpu(owner)))
		ret = false;
	rcu_read_unlock();
	preempt_enable();

	return ret;
}

/*
 * The rwsem_spin_on_owner() function returns the folowing 4 values
 * depending on the lock owner state.
 *   OWNER_NULL  : owner is currently NULL
 *   OWNER_WRITER: when owner changes and is a writer
 *   OWNER_READER: when owner changes and the new owner may be a reader.
 *   OWNER_NONSPINNABLE:
 *		   when optimistic spinning has to stop because either the
 *		   owner stops running, is unknown, or its timeslice has
 *		   been used up.
 */
enum owner_state {
	OWNER_NULL		= 1 << 0,
	OWNER_WRITER		= 1 << 1,
	OWNER_READER		= 1 << 2,
	OWNER_NONSPINNABLE	= 1 << 3,
};
#define OWNER_SPINNABLE		(OWNER_NULL | OWNER_WRITER | OWNER_READER)

static inline enum owner_state
rwsem_owner_state(struct task_struct *owner, unsigned long flags, unsigned long nonspinnable)
{
	if (flags & nonspinnable)
		return OWNER_NONSPINNABLE;

	if (flags & RWSEM_READER_OWNED)
		return OWNER_READER;

	return owner ? OWNER_WRITER : OWNER_NULL;
}

static noinline enum owner_state
rwsem_spin_on_owner(struct rw_semaphore *sem, unsigned long nonspinnable)
{
	struct task_struct *new, *owner;
	unsigned long flags, new_flags;
	enum owner_state state;

	owner = rwsem_owner_flags(sem, &flags);
	state = rwsem_owner_state(owner, flags, nonspinnable);
	if (state != OWNER_WRITER)
		return state;

	rcu_read_lock();
	for (;;) {
		if (atomic_long_read(&sem->count) & RWSEM_FLAG_HANDOFF) {
			state = OWNER_NONSPINNABLE;
			break;
		}

		new = rwsem_owner_flags(sem, &new_flags);
		if ((new != owner) || (new_flags != flags)) {
			state = rwsem_owner_state(new, new_flags, nonspinnable);
			break;
		}

		/*
		 * Ensure we emit the owner->on_cpu, dereference _after_
		 * checking sem->owner still matches owner, if that fails,
		 * owner might point to free()d memory, if it still matches,
		 * the rcu_read_lock() ensures the memory stays valid.
		 */
		barrier();

		if (need_resched() || !owner_on_cpu(owner)) {
			state = OWNER_NONSPINNABLE;
			break;
		}

		cpu_relax();
	}
	rcu_read_unlock();

	return state;
}

/*
 * Calculate reader-owned rwsem spinning threshold for writer
 *
 * The more readers own the rwsem, the longer it will take for them to
 * wind down and free the rwsem. So the empirical formula used to
 * determine the actual spinning time limit here is:
 *
 *   Spinning threshold = (10 + nr_readers/2)us
 *
 * The limit is capped to a maximum of 25us (30 readers). This is just
 * a heuristic and is subjected to change in the future.
 */
static inline u64 rwsem_rspin_threshold(struct rw_semaphore *sem)
{
	long count = atomic_long_read(&sem->count);
	int readers = count >> RWSEM_READER_SHIFT;
	u64 delta;

	if (readers > 30)
		readers = 30;
	delta = (20 + readers) * NSEC_PER_USEC / 2;

	return sched_clock() + delta;
}

static bool rwsem_optimistic_spin(struct rw_semaphore *sem, bool wlock)
{
	bool taken = false;
	int prev_owner_state = OWNER_NULL;
	int loop = 0;
	u64 rspin_threshold = 0;
	unsigned long nonspinnable = wlock ? RWSEM_WR_NONSPINNABLE
					   : RWSEM_RD_NONSPINNABLE;

	preempt_disable();

	/* sem->wait_lock should not be held when doing optimistic spinning */
	if (!osq_lock(&sem->osq))
		goto done;

	/*
	 * Optimistically spin on the owner field and attempt to acquire the
	 * lock whenever the owner changes. Spinning will be stopped when:
	 *  1) the owning writer isn't running; or
	 *  2) readers own the lock and spinning time has exceeded limit.
	 */
	for (;;) {
		enum owner_state owner_state;

		owner_state = rwsem_spin_on_owner(sem, nonspinnable);
		if (!(owner_state & OWNER_SPINNABLE))
			break;

		/*
		 * Try to acquire the lock
		 */
		taken = wlock ? rwsem_try_write_lock_unqueued(sem)
			      : rwsem_try_read_lock_unqueued(sem);

		if (taken)
			break;

		/*
		 * Time-based reader-owned rwsem optimistic spinning
		 */
		if (wlock && (owner_state == OWNER_READER)) {
			/*
			 * Re-initialize rspin_threshold every time when
			 * the owner state changes from non-reader to reader.
			 * This allows a writer to steal the lock in between
			 * 2 reader phases and have the threshold reset at
			 * the beginning of the 2nd reader phase.
			 */
			if (prev_owner_state != OWNER_READER) {
				if (rwsem_test_oflags(sem, nonspinnable))
					break;
				rspin_threshold = rwsem_rspin_threshold(sem);
				loop = 0;
			}

			/*
			 * Check time threshold once every 16 iterations to
			 * avoid calling sched_clock() too frequently so
			 * as to reduce the average latency between the times
			 * when the lock becomes free and when the spinner
			 * is ready to do a trylock.
			 */
			else if (!(++loop & 0xf) && (sched_clock() > rspin_threshold)) {
				rwsem_set_nonspinnable(sem);
				break;
			}
		}

		/*
		 * An RT task cannot do optimistic spinning if it cannot
		 * be sure the lock holder is running or live-lock may
		 * happen if the current task and the lock holder happen
		 * to run in the same CPU. However, aborting optimistic
		 * spinning while a NULL owner is detected may miss some
		 * opportunity where spinning can continue without causing
		 * problem.
		 *
		 * There are 2 possible cases where an RT task may be able
		 * to continue spinning.
		 *
		 * 1) The lock owner is in the process of releasing the
		 *    lock, sem->owner is cleared but the lock has not
		 *    been released yet.
		 * 2) The lock was free and owner cleared, but another
		 *    task just comes in and acquire the lock before
		 *    we try to get it. The new owner may be a spinnable
		 *    writer.
		 *
		 * To take advantage of two scenarios listed agove, the RT
		 * task is made to retry one more time to see if it can
		 * acquire the lock or continue spinning on the new owning
		 * writer. Of course, if the time lag is long enough or the
		 * new owner is not a writer or spinnable, the RT task will
		 * quit spinning.
		 *
		 * If the owner is a writer, the need_resched() check is
		 * done inside rwsem_spin_on_owner(). If the owner is not
		 * a writer, need_resched() check needs to be done here.
		 */
		if (owner_state != OWNER_WRITER) {
			if (need_resched())
				break;
			if (rt_task(current) &&
			   (prev_owner_state != OWNER_WRITER))
				break;
		}
		prev_owner_state = owner_state;

		/*
		 * The cpu_relax() call is a compiler barrier which forces
		 * everything in this loop to be re-loaded. We don't need
		 * memory barriers as we'll eventually observe the right
		 * values at the cost of a few extra spins.
		 */
		cpu_relax();
	}
	osq_unlock(&sem->osq);
done:
	preempt_enable();
	return taken;
}

/*
 * Clear the owner's RWSEM_WR_NONSPINNABLE bit if it is set. This should
 * only be called when the reader count reaches 0.
 *
 * This give writers better chance to acquire the rwsem first before
 * readers when the rwsem was being held by readers for a relatively long
 * period of time. Race can happen that an optimistic spinner may have
 * just stolen the rwsem and set the owner, but just clearing the
 * RWSEM_WR_NONSPINNABLE bit will do no harm anyway.
 */
static inline void clear_wr_nonspinnable(struct rw_semaphore *sem)
{
	if (rwsem_test_oflags(sem, RWSEM_WR_NONSPINNABLE))
		atomic_long_andnot(RWSEM_WR_NONSPINNABLE, &sem->owner);
}
#else
static inline bool rwsem_can_spin_on_owner(struct rw_semaphore *sem,
					   unsigned long nonspinnable)
{
	return false;
}

static inline bool rwsem_optimistic_spin(struct rw_semaphore *sem, bool wlock)
{
	return false;
}

static inline void clear_wr_nonspinnable(struct rw_semaphore *sem) { }
#endif

/*
 * Wait for the read lock to be granted
 */
static struct rw_semaphore __sched *
rwsem_down_read_slowpath(struct rw_semaphore *sem, int state)
{
	long count, adjustment = -RWSEM_READER_BIAS;
	bool wake = false;
	struct rwsem_waiter waiter;
	DEFINE_WAKE_Q(wake_q);
	bool is_first_waiter = false;

	if (!rwsem_can_spin_on_owner(sem, RWSEM_RD_NONSPINNABLE))
		goto queue;

	/*
	 * Undo read bias from down_read() and do optimistic spinning.
	 */
	atomic_long_add(-RWSEM_READER_BIAS, &sem->count);
	adjustment = 0;
	if (rwsem_optimistic_spin(sem, false)) {
		/*
		 * Wake up other readers in the wait list if the front
		 * waiter is a reader.
		 */
		if ((atomic_long_read(&sem->count) & RWSEM_FLAG_WAITERS)) {
			raw_spin_lock_irq(&sem->wait_lock);
			if (!list_empty(&sem->wait_list))
				rwsem_mark_wake(sem, RWSEM_WAKE_READ_OWNED,
						&wake_q);
			raw_spin_unlock_irq(&sem->wait_lock);
			wake_up_q(&wake_q);
		}
		return sem;
	}

queue:
	waiter.task = current;
	waiter.type = RWSEM_WAITING_FOR_READ;
	waiter.timeout = jiffies + RWSEM_WAIT_TIMEOUT;

	raw_spin_lock_irq(&sem->wait_lock);
	if (list_empty(&sem->wait_list)) {
		/*
		 * In case the wait queue is empty and the lock isn't owned
		 * by a writer or has the handoff bit set, this reader can
		 * exit the slowpath and return immediately as its
		 * RWSEM_READER_BIAS has already been set in the count.
		 */
		if (adjustment && !(atomic_long_read(&sem->count) &
		     (RWSEM_WRITER_MASK | RWSEM_FLAG_HANDOFF))) {
			raw_spin_unlock_irq(&sem->wait_lock);
			rwsem_set_reader_owned(sem);
			return sem;
		}
		adjustment += RWSEM_FLAG_WAITERS;
	}

	/* is_first_waiter == true means we are first in the queue */
	is_first_waiter = rwsem_list_add_per_prio(&waiter, sem);

	/* we're now waiting on the lock, but no longer actively locking */
	if (adjustment)
		count = atomic_long_add_return(adjustment, &sem->count);
	else
		count = atomic_long_read(&sem->count);

	/*
	 * If there are no active locks, wake the front queued process(es).
	 *
	 * If there are no writers and we are first in the queue,
	 * wake our own waiter to join the existing active readers !
	 */
	if (!(count & RWSEM_LOCK_MASK)) {
		clear_wr_nonspinnable(sem);
		wake = true;
	}
	if (wake || (!(count & RWSEM_WRITER_MASK) &&
		    (adjustment & RWSEM_FLAG_WAITERS || is_first_waiter)))
		rwsem_mark_wake(sem, RWSEM_WAKE_ANY, &wake_q);

	raw_spin_unlock_irq(&sem->wait_lock);
	wake_up_q(&wake_q);

	/* wait to be given the lock */
	while (true) {
		set_current_state(state);
		if (!waiter.task)
			break;
		if (signal_pending_state(state, current)) {
			raw_spin_lock_irq(&sem->wait_lock);
			if (waiter.task)
				goto out_nolock;
			raw_spin_unlock_irq(&sem->wait_lock);
			break;
		}
		schedule();
	}

	__set_current_state(TASK_RUNNING);
	return sem;
out_nolock:
	list_del(&waiter.list);
	if (list_empty(&sem->wait_list)) {
		atomic_long_andnot(RWSEM_FLAG_WAITERS|RWSEM_FLAG_HANDOFF,
				   &sem->count);
	}
	raw_spin_unlock_irq(&sem->wait_lock);
	__set_current_state(TASK_RUNNING);
	return ERR_PTR(-EINTR);
}

/*
 * Wait until we successfully acquire the write lock
 */
static struct rw_semaphore *
rwsem_down_write_slowpath(struct rw_semaphore *sem, int state)
{
	long count;
	enum writer_wait_state wstate;
	struct rwsem_waiter waiter;
	struct rw_semaphore *ret = sem;
	DEFINE_WAKE_Q(wake_q);
	bool is_first_waiter = false;

	/* do optimistic spinning and steal lock if possible */
	if (rwsem_can_spin_on_owner(sem, RWSEM_WR_NONSPINNABLE) &&
	    rwsem_optimistic_spin(sem, true))
		return sem;

	/*
	 * Optimistic spinning failed, proceed to the slowpath
	 * and block until we can acquire the sem.
	 */
	waiter.task = current;
	waiter.type = RWSEM_WAITING_FOR_WRITE;
	waiter.timeout = jiffies + RWSEM_WAIT_TIMEOUT;

	raw_spin_lock_irq(&sem->wait_lock);

	/* account for this before adding a new element to the list */
	wstate = list_empty(&sem->wait_list) ? WRITER_FIRST : WRITER_NOT_FIRST;

	/*
	 * is_first_waiter == true means we are first in the queue,
	 * so there is no read locks that were queued ahead of us.
	 */
	is_first_waiter = rwsem_list_add_per_prio(&waiter, sem);

	/* we're now waiting on the lock */
	if (wstate == WRITER_NOT_FIRST) {
		count = atomic_long_read(&sem->count);

		/*
		 * If there were already threads queued before us and:
		 *  1) there are no no active locks, wake the front
		 *     queued process(es) as the handoff bit might be set.
		 *  2) there are no active writers and some readers, the lock
		 *     must be read owned; so we try to wake any read lock
		 *     waiters that were queued ahead of us.
		 */
		if (is_first_waiter || count & RWSEM_WRITER_MASK)
			goto wait;

		rwsem_mark_wake(sem, (count & RWSEM_READER_MASK)
					? RWSEM_WAKE_READERS
					: RWSEM_WAKE_ANY, &wake_q);

		if (!wake_q_empty(&wake_q)) {
			/*
			 * We want to minimize wait_lock hold time especially
			 * when a large number of readers are to be woken up.
			 */
			raw_spin_unlock_irq(&sem->wait_lock);
			wake_up_q(&wake_q);
			wake_q_init(&wake_q);	/* Used again, reinit */
			raw_spin_lock_irq(&sem->wait_lock);
		}
	} else {
		atomic_long_or(RWSEM_FLAG_WAITERS, &sem->count);
	}

wait:
	/* wait until we successfully acquire the lock */
	set_current_state(state);
	while (true) {
		if (rwsem_try_write_lock(sem, wstate))
			break;

		raw_spin_unlock_irq(&sem->wait_lock);

		/* Block until there are no active lockers. */
		for (;;) {
			if (signal_pending_state(state, current))
				goto out_nolock;

			schedule();
			set_current_state(state);
			/*
			 * If HANDOFF bit is set, unconditionally do
			 * a trylock.
			 */
			if (wstate == WRITER_HANDOFF)
				break;

			if ((wstate == WRITER_NOT_FIRST) &&
			    (rwsem_first_waiter(sem) == &waiter))
				wstate = WRITER_FIRST;

			count = atomic_long_read(&sem->count);
			if (!(count & RWSEM_LOCK_MASK))
				break;

			/*
			 * The setting of the handoff bit is deferred
			 * until rwsem_try_write_lock() is called.
			 */
			if ((wstate == WRITER_FIRST) && (rt_task(current) ||
			    time_after(jiffies, waiter.timeout))) {
				wstate = WRITER_HANDOFF;
				break;
			}
		}

		raw_spin_lock_irq(&sem->wait_lock);
	}
	__set_current_state(TASK_RUNNING);
	list_del(&waiter.list);
	raw_spin_unlock_irq(&sem->wait_lock);

	return ret;

out_nolock:
	__set_current_state(TASK_RUNNING);
	raw_spin_lock_irq(&sem->wait_lock);
	list_del(&waiter.list);

	if (unlikely(wstate == WRITER_HANDOFF))
		atomic_long_add(-RWSEM_FLAG_HANDOFF,  &sem->count);

	if (list_empty(&sem->wait_list))
		atomic_long_andnot(RWSEM_FLAG_WAITERS, &sem->count);
	else
		rwsem_mark_wake(sem, RWSEM_WAKE_ANY, &wake_q);
	raw_spin_unlock_irq(&sem->wait_lock);
	wake_up_q(&wake_q);

	return ERR_PTR(-EINTR);
}

/*
 * handle waking up a waiter on the semaphore
 * - up_read/up_write has decremented the active part of count if we come here
 */
static struct rw_semaphore *rwsem_wake(struct rw_semaphore *sem, long count)
{
	unsigned long flags;
	DEFINE_WAKE_Q(wake_q);

	raw_spin_lock_irqsave(&sem->wait_lock, flags);

	if (!list_empty(&sem->wait_list))
		rwsem_mark_wake(sem, RWSEM_WAKE_ANY, &wake_q);

	raw_spin_unlock_irqrestore(&sem->wait_lock, flags);
	wake_up_q(&wake_q);

	return sem;
}

/*
 * downgrade a write lock into a read lock
 * - caller incremented waiting part of count and discovered it still negative
 * - just wake up any readers at the front of the queue
 */
static struct rw_semaphore *rwsem_downgrade_wake(struct rw_semaphore *sem)
{
	unsigned long flags;
	DEFINE_WAKE_Q(wake_q);

	raw_spin_lock_irqsave(&sem->wait_lock, flags);

	if (!list_empty(&sem->wait_list))
		rwsem_mark_wake(sem, RWSEM_WAKE_READ_OWNED, &wake_q);

	raw_spin_unlock_irqrestore(&sem->wait_lock, flags);
	wake_up_q(&wake_q);

	return sem;
}

/*
 * lock for reading
 */
inline void __down_read(struct rw_semaphore *sem)
{
	if (unlikely(atomic_long_fetch_add_acquire(RWSEM_READER_BIAS,
			&sem->count) & RWSEM_READ_FAILED_MASK)) {
		rwsem_down_read_slowpath(sem, TASK_UNINTERRUPTIBLE);
		DEBUG_RWSEMS_WARN_ON(!is_rwsem_reader_owned(sem), sem);
	} else {
		rwsem_set_reader_owned(sem);
	}
}

static inline int __down_read_killable(struct rw_semaphore *sem)
{
	if (unlikely(atomic_long_fetch_add_acquire(RWSEM_READER_BIAS,
			&sem->count) & RWSEM_READ_FAILED_MASK)) {
		if (IS_ERR(rwsem_down_read_slowpath(sem, TASK_KILLABLE)))
			return -EINTR;
		DEBUG_RWSEMS_WARN_ON(!is_rwsem_reader_owned(sem), sem);
	} else {
		rwsem_set_reader_owned(sem);
	}
	return 0;
}

static inline int __down_read_trylock(struct rw_semaphore *sem)
{
	/*
	 * Optimize for the case when the rwsem is not locked at all.
	 */
	long tmp = RWSEM_UNLOCKED_VALUE;

	do {
		if (atomic_long_try_cmpxchg_acquire(&sem->count, &tmp,
					tmp + RWSEM_READER_BIAS)) {
			rwsem_set_reader_owned(sem);
			return 1;
		}
	} while (!(tmp & RWSEM_READ_FAILED_MASK));
	return 0;
}

/*
 * lock for writing
 */
static inline void __down_write(struct rw_semaphore *sem)
{
	long tmp = RWSEM_UNLOCKED_VALUE;

	if (unlikely(!atomic_long_try_cmpxchg_acquire(&sem->count, &tmp,
						      RWSEM_WRITER_LOCKED)))
		rwsem_down_write_slowpath(sem, TASK_UNINTERRUPTIBLE);
	rwsem_set_owner(sem);
}

static inline int __down_write_killable(struct rw_semaphore *sem)
{
	long tmp = RWSEM_UNLOCKED_VALUE;

	if (unlikely(!atomic_long_try_cmpxchg_acquire(&sem->count, &tmp,
						      RWSEM_WRITER_LOCKED))) {
		if (IS_ERR(rwsem_down_write_slowpath(sem, TASK_KILLABLE)))
			return -EINTR;
	}
	rwsem_set_owner(sem);
	return 0;
}

static inline int __down_write_trylock(struct rw_semaphore *sem)
{
	long tmp = RWSEM_UNLOCKED_VALUE;

	if (atomic_long_try_cmpxchg_acquire(&sem->count, &tmp,
					    RWSEM_WRITER_LOCKED)) {
		rwsem_set_owner(sem);
		return true;
	}
	return false;
}

/*
 * unlock after reading
 */
inline void __up_read(struct rw_semaphore *sem)
{
	long tmp;

	DEBUG_RWSEMS_WARN_ON(!is_rwsem_reader_owned(sem), sem);
	rwsem_clear_reader_owned(sem);
	tmp = atomic_long_add_return_release(-RWSEM_READER_BIAS, &sem->count);
	if (unlikely((tmp & (RWSEM_LOCK_MASK|RWSEM_FLAG_WAITERS)) ==
		      RWSEM_FLAG_WAITERS)) {
		clear_wr_nonspinnable(sem);
		rwsem_wake(sem, tmp);
	}
}

/*
 * unlock after writing
 */
static inline void __up_write(struct rw_semaphore *sem)
{
	long tmp;

	/*
	 * sem->owner may differ from current if the ownership is transferred
	 * to an anonymous writer by setting the RWSEM_NONSPINNABLE bits.
	 */
	DEBUG_RWSEMS_WARN_ON((rwsem_owner(sem) != current) &&
			    !rwsem_test_oflags(sem, RWSEM_NONSPINNABLE), sem);
	rwsem_clear_owner(sem);
	tmp = atomic_long_fetch_add_release(-RWSEM_WRITER_LOCKED, &sem->count);
	if (unlikely(tmp & RWSEM_FLAG_WAITERS))
		rwsem_wake(sem, tmp);
}

/*
 * downgrade write lock to read lock
 */
static inline void __downgrade_write(struct rw_semaphore *sem)
{
	long tmp;

	/*
	 * When downgrading from exclusive to shared ownership,
	 * anything inside the write-locked region cannot leak
	 * into the read side. In contrast, anything in the
	 * read-locked region is ok to be re-ordered into the
	 * write side. As such, rely on RELEASE semantics.
	 */
	DEBUG_RWSEMS_WARN_ON(rwsem_owner(sem) != current, sem);
	tmp = atomic_long_fetch_add_release(
		-RWSEM_WRITER_LOCKED+RWSEM_READER_BIAS, &sem->count);
	rwsem_set_reader_owned(sem);
	if (tmp & RWSEM_FLAG_WAITERS)
		rwsem_downgrade_wake(sem);
}

/*
 * lock for reading
 */
void __sched down_read(struct rw_semaphore *sem)
{
	might_sleep();
	rwsem_acquire_read(&sem->dep_map, 0, 0, _RET_IP_);

	LOCK_CONTENDED(sem, __down_read_trylock, __down_read);
}
EXPORT_SYMBOL(down_read);

int __sched down_read_killable(struct rw_semaphore *sem)
{
	might_sleep();
	rwsem_acquire_read(&sem->dep_map, 0, 0, _RET_IP_);

	if (LOCK_CONTENDED_RETURN(sem, __down_read_trylock, __down_read_killable)) {
		rwsem_release(&sem->dep_map, 1, _RET_IP_);
		return -EINTR;
	}

	return 0;
}
EXPORT_SYMBOL(down_read_killable);

/*
 * trylock for reading -- returns 1 if successful, 0 if contention
 */
int down_read_trylock(struct rw_semaphore *sem)
{
	int ret = __down_read_trylock(sem);

	if (ret == 1)
		rwsem_acquire_read(&sem->dep_map, 0, 1, _RET_IP_);
	return ret;
}
EXPORT_SYMBOL(down_read_trylock);

/*
 * lock for writing
 */
void __sched down_write(struct rw_semaphore *sem)
{
	might_sleep();
	rwsem_acquire(&sem->dep_map, 0, 0, _RET_IP_);
	LOCK_CONTENDED(sem, __down_write_trylock, __down_write);
}
EXPORT_SYMBOL(down_write);

/*
 * lock for writing
 */
int __sched down_write_killable(struct rw_semaphore *sem)
{
	might_sleep();
	rwsem_acquire(&sem->dep_map, 0, 0, _RET_IP_);

	if (LOCK_CONTENDED_RETURN(sem, __down_write_trylock,
				  __down_write_killable)) {
		rwsem_release(&sem->dep_map, 1, _RET_IP_);
		return -EINTR;
	}

	return 0;
}
EXPORT_SYMBOL(down_write_killable);

/*
 * trylock for writing -- returns 1 if successful, 0 if contention
 */
int down_write_trylock(struct rw_semaphore *sem)
{
	int ret = __down_write_trylock(sem);

	if (ret == 1)
		rwsem_acquire(&sem->dep_map, 0, 1, _RET_IP_);

	return ret;
}
EXPORT_SYMBOL(down_write_trylock);

/*
 * release a read lock
 */
void up_read(struct rw_semaphore *sem)
{
	rwsem_release(&sem->dep_map, 1, _RET_IP_);
	__up_read(sem);
}
EXPORT_SYMBOL(up_read);

/*
 * release a write lock
 */
void up_write(struct rw_semaphore *sem)
{
	rwsem_release(&sem->dep_map, 1, _RET_IP_);
	__up_write(sem);
}
EXPORT_SYMBOL(up_write);

/*
 * downgrade write lock to read lock
 */
void downgrade_write(struct rw_semaphore *sem)
{
	lock_downgrade(&sem->dep_map, _RET_IP_);
	__downgrade_write(sem);
}
EXPORT_SYMBOL(downgrade_write);

#ifdef CONFIG_DEBUG_LOCK_ALLOC

void down_read_nested(struct rw_semaphore *sem, int subclass)
{
	might_sleep();
	rwsem_acquire_read(&sem->dep_map, subclass, 0, _RET_IP_);
	LOCK_CONTENDED(sem, __down_read_trylock, __down_read);
}
EXPORT_SYMBOL(down_read_nested);

void _down_write_nest_lock(struct rw_semaphore *sem, struct lockdep_map *nest)
{
	might_sleep();
	rwsem_acquire_nest(&sem->dep_map, 0, 0, nest, _RET_IP_);
	LOCK_CONTENDED(sem, __down_write_trylock, __down_write);
}
EXPORT_SYMBOL(_down_write_nest_lock);

void down_read_non_owner(struct rw_semaphore *sem)
{
	might_sleep();
	__down_read(sem);
	__rwsem_set_reader_owned(sem, NULL);
}
EXPORT_SYMBOL(down_read_non_owner);

void down_write_nested(struct rw_semaphore *sem, int subclass)
{
	might_sleep();
	rwsem_acquire(&sem->dep_map, subclass, 0, _RET_IP_);
	LOCK_CONTENDED(sem, __down_write_trylock, __down_write);
}
EXPORT_SYMBOL(down_write_nested);

int __sched down_write_killable_nested(struct rw_semaphore *sem, int subclass)
{
	might_sleep();
	rwsem_acquire(&sem->dep_map, subclass, 0, _RET_IP_);

	if (LOCK_CONTENDED_RETURN(sem, __down_write_trylock,
				  __down_write_killable)) {
		rwsem_release(&sem->dep_map, 1, _RET_IP_);
		return -EINTR;
	}

	return 0;
}
EXPORT_SYMBOL(down_write_killable_nested);

void up_read_non_owner(struct rw_semaphore *sem)
{
	DEBUG_RWSEMS_WARN_ON(!is_rwsem_reader_owned(sem), sem);
	__up_read(sem);
}
EXPORT_SYMBOL(up_read_non_owner);

#endif
