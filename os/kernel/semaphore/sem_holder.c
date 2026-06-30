/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
/****************************************************************************
 * kernel/semaphore/sem_holder.c
 *
 *   Copyright (C) 2009-2011, 2013 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

#include <semaphore.h>
#include <sched.h>
#include <assert.h>
#include <debug.h>
#include <tinyara/arch.h>

#include "sched/sched.h"
#include "semaphore/semaphore.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

#ifndef CONFIG_SEM_PREALLOCHOLDERS
#define CONFIG_SEM_PREALLOCHOLDERS 0
#endif

/****************************************************************************
 * Private Type Declarations
 ****************************************************************************/

typedef int (*holderhandler_t)(FAR struct semholder_s *pholder, FAR sem_t *sem, FAR void *arg);

/****************************************************************************
 * Global Variables
 ****************************************************************************/

/****************************************************************************
 * Private Variables
 ****************************************************************************/

/* Preallocated holder structures */

#if CONFIG_SEM_PREALLOCHOLDERS > 0
static struct semholder_s g_holderalloc[CONFIG_SEM_PREALLOCHOLDERS];
static FAR struct semholder_s *g_freeholders;
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sem_allocholder
 ****************************************************************************/

static inline FAR struct semholder_s *sem_allocholder(sem_t *sem)
{
	FAR struct semholder_s *pholder;

	/* Check if the "built-in" holder is being used.  We have this built-in
	 * holder to optimize for the simplest case where semaphores are only
	 * used to implement mutexes.
	 */

#if CONFIG_SEM_PREALLOCHOLDERS > 0
	pholder = g_freeholders;
	if (pholder) {
		/* Remove the holder from the free list an put it into the semaphore's
		 * holder list
		 */

		g_freeholders = pholder->flink;
		pholder->flink = sem->hhead;
		sem->hhead = pholder;

		/* Make sure the initial count is zero */

		pholder->counts = 0;
#if CONFIG_SEM_NNESTPRIO > 0
		pholder->ndemand = 0;
#endif
	}
#else
	if (!sem->holder.htcb) {
		pholder = &sem->holder;
		pholder->counts = 0;
	}
#endif
	else {
		sdbg("Insufficient pre-allocated holders\n");
		pholder = NULL;
	}

	return pholder;
}

/****************************************************************************
 * Name: sem_findorallocateholder
 ****************************************************************************/

static inline FAR struct semholder_s *sem_findorallocateholder(sem_t *sem, FAR struct tcb_s *htcb)
{
	FAR struct semholder_s *pholder = sem_findholder(sem, htcb);
	if (!pholder) {
		pholder = sem_allocholder(sem);
	}

	return pholder;
}

/****************************************************************************
 * Name: sem_freeholder
 ****************************************************************************/

void sem_freeholder(sem_t *sem, FAR struct semholder_s *pholder)
{
#if CONFIG_SEM_PREALLOCHOLDERS > 0
	FAR struct semholder_s *curr;
	FAR struct semholder_s *prev;
#endif

	/* Release the holder and counts */

	pholder->htcb = NULL;
	pholder->counts = 0;
#if CONFIG_SEM_PREALLOCHOLDERS > 0 && CONFIG_SEM_NNESTPRIO > 0
	pholder->ndemand = 0;
#endif

#if CONFIG_SEM_PREALLOCHOLDERS > 0
	/* Search the list for the matching holder */

	for (prev = NULL, curr = sem->hhead; curr && curr != pholder; prev = curr, curr = curr->flink) ;

	if (curr) {
		/* Remove the holder from the list */

		if (prev) {
			prev->flink = pholder->flink;
		} else {
			sem->hhead = pholder->flink;
		}

		/* And put it in the free list */

		pholder->flink = g_freeholders;
		g_freeholders = pholder;
	}
#endif
}

/****************************************************************************
 * Name: sem_clear_reprio_mirror
 *
 * Description:
 *   Discard the per-semaphore priority-inheritance demand mirror for every
 *   holder owned by the given task.  This must be called whenever the task's
 *   per-thread pend_reprios[] aggregate is cleared out-of-band (for example
 *   by sched_reprioritize() on an explicit user reprioritization, or when a
 *   task is restarted), so that the per-sem mirrors do not retain stale
 *   demand values that no longer exist in the aggregate.
 *
 ****************************************************************************/

#if CONFIG_SEM_PREALLOCHOLDERS > 0 && CONFIG_SEM_NNESTPRIO > 0
void sem_clear_reprio_mirror(FAR struct tcb_s *tcb)
{
	int i;

	for (i = 0; i < CONFIG_SEM_PREALLOCHOLDERS; i++) {
		if (g_holderalloc[i].htcb == tcb) {
			g_holderalloc[i].ndemand = 0;
		}
	}
}
#endif

/****************************************************************************
 * Name: sem_foreachholder
 ****************************************************************************/

static int sem_foreachholder(FAR sem_t *sem, holderhandler_t handler, FAR void *arg)
{
	FAR struct semholder_s *pholder;
#if CONFIG_SEM_PREALLOCHOLDERS > 0
	FAR struct semholder_s *next;
#endif
	int ret = 0;

#if CONFIG_SEM_PREALLOCHOLDERS > 0
	for (pholder = sem->hhead; pholder && ret == 0; pholder = next)
#else
	pholder = &sem->holder;
#endif
	{
#if CONFIG_SEM_PREALLOCHOLDERS > 0
		/* In case this holder gets deleted */

		next = pholder->flink;
#endif
		/* The initial "built-in" container may hold a NULL holder */

		if (pholder->htcb) {
			/* Call the handler */

			ret = handler(pholder, sem, arg);
		}
	}

	return ret;
}

/****************************************************************************
 * Name: sem_recoverholders
 ****************************************************************************/

#if CONFIG_SEM_PREALLOCHOLDERS > 0
static int sem_recoverholders(FAR struct semholder_s *pholder, FAR sem_t *sem, FAR void *arg)
{
	sem_freeholder(sem, pholder);
	return 0;
}
#endif

#ifdef CONFIG_PRIORITY_INHERITANCE

#if CONFIG_SEM_PREALLOCHOLDERS > 0 && CONFIG_SEM_NNESTPRIO > 0
/****************************************************************************
 * Priority-inheritance demand bookkeeping helpers
 *
 * The holder's per-thread pend_reprios[] array is an aggregate multiset of
 * all priority-inheritance demands placed on the holder by waiters across
 * ALL semaphores it holds.  Because that array carries no per-entry record
 * of which semaphore caused each demand, releasing one semaphore cannot tell
 * which entries to drop -- the root cause of the cross-semaphore contamination
 * bug.
 *
 * To fix this, each semholder_s keeps a "mirror" (demands[]) of exactly the
 * demand values that this (sem, holder) pair pushed into the aggregate.  On
 * release we remove just those values from the aggregate, then recompute the
 * holder priority from whatever demands remain (which belong to other still
 * held semaphores).
 *
 * Values are interchangeable within the multiset: if two semaphores both
 * contributed the same priority value, removing either occurrence is correct
 * because only the resulting maximum matters for the holder's priority.
 ****************************************************************************/

/* Record one demand into both the per-thread aggregate and the per-sem
 * mirror.  Returns false (and records nothing) if either store is full.
 */

static bool sem_demand_add(FAR struct tcb_s *htcb, FAR struct semholder_s *pholder, uint8_t prio)
{
	if (htcb->npend_reprio >= CONFIG_SEM_NNESTPRIO || pholder->ndemand >= CONFIG_SEM_NNESTPRIO) {
		return false;
	}

	htcb->pend_reprios[htcb->npend_reprio++] = prio;
	pholder->demands[pholder->ndemand++] = prio;
	return true;
}

/* Remove a single occurrence of value v from the per-thread aggregate. */

static void sem_aggregate_remove_one(FAR struct tcb_s *htcb, uint8_t v)
{
	int i;

	for (i = 0; i < htcb->npend_reprio; i++) {
		if (htcb->pend_reprios[i] == v) {
			htcb->npend_reprio--;
			htcb->pend_reprios[i] = htcb->pend_reprios[htcb->npend_reprio];
			return;
		}
	}
}

/* Highest demand at or above base in the aggregate (base if none remain). */

static uint8_t sem_aggregate_peak(FAR struct tcb_s *htcb)
{
	uint8_t hi = htcb->base_priority;
	int i;

	for (i = 0; i < htcb->npend_reprio; i++) {
		if (htcb->pend_reprios[i] > hi) {
			hi = htcb->pend_reprios[i];
		}
	}

	return hi;
}
#endif							/* CONFIG_SEM_PREALLOCHOLDERS > 0 && CONFIG_SEM_NNESTPRIO > 0 */

/****************************************************************************
 * Name: sem_boostholderprio
 ****************************************************************************/

static int sem_boostholderprio(FAR struct semholder_s *pholder, FAR sem_t *sem, FAR void *arg)
{
	FAR struct tcb_s *htcb = (FAR struct tcb_s *)pholder->htcb;
	FAR struct tcb_s *rtcb = (FAR struct tcb_s *)arg;

	/* Make sure that the holder thread is still active.  If it exited without
	 * releasing its counts, then that would be a bad thing.  But we can take
	 * no real action because we don't know know that the program is doing.
	 * Perhaps its plan is to kill a thread, then destroy the semaphore.
	 */

	if (!sched_verifytcb(htcb)) {
		sdbg("TCB 0x%08x is a stale handle, counts lost\n", htcb);
		sem_freeholder(sem, pholder);
	}
#if CONFIG_SEM_NNESTPRIO > 0

	/* If the priority of the thread that is waiting for a count is greater
	 * than the base priority of the thread holding a count, then we may need
	 * to adjust the holder's priority now or later to that priority.
	 */

	else if (rtcb->sched_priority > htcb->base_priority) {
#if CONFIG_SEM_PREALLOCHOLDERS > 0
		/* Record this waiter's demand into both the per-thread aggregate
		 * (pend_reprios[]) and this semaphore's per-holder mirror.  The
		 * holder's working priority is always the maximum of base and all
		 * recorded demands, so on release we can remove exactly this
		 * semaphore's demands and recompute correctly.
		 */

		if (!sem_demand_add(htcb, pholder, rtcb->sched_priority)) {
			sdbg("CONFIG_SEM_NNESTPRIO exceeded\n");
		}

		/* Raise the holder now only if this demand exceeds its current
		 * working priority.  This cannot cause a context switch because we
		 * have preemption disabled.
		 */

		if (rtcb->sched_priority > htcb->sched_priority) {
			(void)sched_setpriority(htcb, rtcb->sched_priority);
		}
#else
		/* Fallback (no per-sem mirror available because holders are not
		 * pre-allocated): original upstream nested-priority behavior.
		 */

		if (rtcb->sched_priority > htcb->sched_priority) {
			if (htcb->sched_priority > htcb->base_priority) {
				if (htcb->npend_reprio < CONFIG_SEM_NNESTPRIO) {
					htcb->pend_reprios[htcb->npend_reprio] = htcb->sched_priority;
					htcb->npend_reprio++;
				} else {
					sdbg("CONFIG_SEM_NNESTPRIO exceeded\n");
				}
			}

			(void)sched_setpriority(htcb, rtcb->sched_priority);
		} else {
			if (htcb->npend_reprio < CONFIG_SEM_NNESTPRIO) {
				htcb->pend_reprios[htcb->npend_reprio] = rtcb->sched_priority;
				htcb->npend_reprio++;
			} else {
				sdbg("Number of threads exceed CONFIG_SEM_NNESTPRIO\n");
			}
		}
#endif
	}
#else
	/* If the priority of the thread that is waiting for a count is less than
	 * of equal to the priority of the thread holding a count, then do nothing
	 * because the thread is already running at a sufficient priority.
	 */

	else if (rtcb->sched_priority > htcb->sched_priority) {
		/* Raise the priority of the holder of the semaphore.  This
		 * cannot cause a context switch because we have preemption
		 * disabled.  The task will be marked "pending" and the switch
		 * will occur during up_block_task() processing.
		 */

		(void)sched_setpriority(htcb, rtcb->sched_priority);
	}
#endif

	return 0;
}
#endif
/****************************************************************************
 * Name: sem_verifyholder
 ****************************************************************************/

#ifdef CONFIG_DEBUG
static int sem_verifyholder(FAR struct semholder_s *pholder, FAR sem_t *sem, FAR void *arg)
{
#if 0							// Need to revisit this, but these assumptions seem to be untrue -- OR there is a bug???
	FAR struct tcb_s *htcb = (FAR struct tcb_s *)pholder->htcb;

	/* Called after a semaphore has been released (incremented), the semaphore
	 * could is non-negative, and there is no thread waiting for the count.
	 * In this case, the priority of the holder should not be boosted.
	 */

#if CONFIG_SEM_NNESTPRIO > 0
	DEBUGASSERT(htcb->npend_reprio == 0);
#endif
	DEBUGASSERT(htcb->sched_priority == htcb->base_priority);
#endif
	return 0;
}
#endif

/****************************************************************************
 * Name: sem_dumpholder
 ****************************************************************************/

#if defined(CONFIG_DEBUG) && defined(CONFIG_SEM_PHDEBUG)
static int sem_dumpholder(FAR struct semholder_s *pholder, FAR sem_t *sem, FAR void *arg)
{
#if CONFIG_SEM_PREALLOCHOLDERS > 0
	vdbg("  %08x: %08x %08x %04x\n", pholder, pholder->flink, pholder->htcb, pholder->counts);
#else
	vdbg("  %08x: %08x %04x\n", pholder, pholder->htcb, pholder->counts);
#endif
	return 0;
}
#endif

#ifdef CONFIG_PRIORITY_INHERITANCE

/****************************************************************************
 * Name: sem_restoreholderprio
 ****************************************************************************/

static int sem_restoreholderprio(FAR struct semholder_s *pholder, FAR sem_t *sem, FAR void *arg)
{
	FAR struct tcb_s *htcb = (FAR struct tcb_s *)pholder->htcb;
#if CONFIG_SEM_NNESTPRIO > 0
#if CONFIG_SEM_PREALLOCHOLDERS > 0
	uint8_t newprio;
	int k;
	int best;
#else
	FAR struct tcb_s *stcb = (FAR struct tcb_s *)arg;
	int rpriority;
	int i;
	int j;
#endif
#endif

	/* Make sure that the holder thread is still active.  If it exited without
	 * releasing its counts, then that would be a bad thing.  But we can take
	 * no real action because we don't know know that the program is doing.
	 * Perhaps its plan is to kill a thread, then destroy the semaphore.
	 */

	if (!sched_verifytcb(htcb)) {
		sdbg("TCB 0x%08x is a stale handle, counts lost\n", htcb);
		sem_freeholder(sem, pholder);
	}

	/* Was the priority of the holder thread boosted? If so, then drop its
	 * priority back to the correct level.  What is the correct level?
	 */

	else if (htcb->sched_priority != htcb->base_priority) {
#if CONFIG_SEM_NNESTPRIO > 0
#if CONFIG_SEM_PREALLOCHOLDERS > 0
		/* Remove the priority-inheritance demands that THIS semaphore
		 * contributed to the holder's aggregate, then recompute the holder's
		 * priority from whatever demands remain (which belong to other
		 * semaphores the holder still holds).
		 *
		 * (void)arg: the woken task (stcb) is not needed; the holder's
		 * priority is derived purely from the remaining demand multiset.
		 */

		(void)arg;

		if (pholder->counts == 0) {
			/* The holder fully released this semaphore.  Any waiters still
			 * queued on it are now behind the new holder, so every demand
			 * this semaphore contributed must be removed from the holder.
			 */

			for (k = 0; k < pholder->ndemand; k++) {
				sem_aggregate_remove_one(htcb, pholder->demands[k]);
			}

			pholder->ndemand = 0;
		} else {
			/* The holder still holds counts on this semaphore.  Only the
			 * single demand satisfied by this post (the highest this
			 * semaphore contributed) is removed.
			 */

			best = -1;
			for (k = 0; k < pholder->ndemand; k++) {
				if (best < 0 || pholder->demands[k] > pholder->demands[best]) {
					best = k;
				}
			}

			if (best >= 0) {
				sem_aggregate_remove_one(htcb, pholder->demands[best]);
				pholder->ndemand--;
				pholder->demands[best] = pholder->demands[pholder->ndemand];
			}
		}

		/* Recompute: the holder's working priority is the highest remaining
		 * demand, or its base priority if none remain.  Demands are only ever
		 * recorded when strictly above base, so "peak == base" means no demand
		 * remains -- only then do we discard inheritance history via
		 * sched_reprioritize().  If demands from other semaphores remain,
		 * peak > base and they are preserved.
		 */

		newprio = sem_aggregate_peak(htcb);
		if (newprio <= htcb->base_priority) {
			sched_reprioritize(htcb, htcb->base_priority);
		} else {
			sched_setpriority(htcb, newprio);
		}
#else
		/* Fallback (no per-sem mirror): original upstream nested-priority
		 * restore based on the woken task's priority.
		 */

		if (htcb->npend_reprio < 1) {
			DEBUGASSERT(htcb->npend_reprio == 0);
			sched_reprioritize(htcb, htcb->base_priority);
		} else if (htcb->sched_priority <= stcb->sched_priority) {
			for (i = 1, j = 0; i < htcb->npend_reprio; i++) {
				if (htcb->pend_reprios[i] > htcb->pend_reprios[j]) {
					j = i;
				}
			}

			rpriority = htcb->pend_reprios[j];
			i = htcb->npend_reprio - 1;
			if (i > 0) {
				htcb->pend_reprios[j] = htcb->pend_reprios[i];
			}

			htcb->npend_reprio = i;
			sched_setpriority(htcb, rpriority);
		} else {
			for (i = 0; i < htcb->npend_reprio; i++) {
				if (htcb->pend_reprios[i] == stcb->sched_priority) {
					j = htcb->npend_reprio - 1;
					if (j > 0) {
						htcb->pend_reprios[i] = htcb->pend_reprios[j];
					}

					htcb->npend_reprio = j;
					break;
				}
			}
		}
#endif
#else
		/* There is no alternative restore priorities, drop the priority
		 * of the holder thread all the way back to the threads "base"
		 * priority.
		 */

		sched_reprioritize(htcb, htcb->base_priority);
#endif
	}

	return 0;
}

/****************************************************************************
 * Name: sem_restoreholderprioA
 *
 * Description:
 *   Reprioritize all holders except the currently executing task
 *
 ****************************************************************************/

static int sem_restoreholderprioA(FAR struct semholder_s *pholder, FAR sem_t *sem, FAR void *arg)
{
	FAR struct tcb_s *rtcb = this_task();
	if (pholder->htcb != rtcb) {
		return sem_restoreholderprio(pholder, sem, arg);
	}

	return 0;
}

/****************************************************************************
 * Name: sem_restoreholderprioB
 *
 * Description:
 *   Reprioritize only the currently executing task
 *
 ****************************************************************************/

static int sem_restoreholderprioB(FAR struct semholder_s *pholder, FAR sem_t *sem, FAR void *arg)
{
	FAR struct tcb_s *rtcb = this_task();
	if (pholder->htcb == rtcb) {
		(void)sem_restoreholderprio(pholder, sem, arg);
		return 1;
	}

	return 0;
}

/****************************************************************************
 * Name: sem_restorebaseprio_irq
 *
 * Description:
 *   This function is called after the an interrupt handler posts a count on
 *   the semaphore.  It will check if we need to drop the priority of any
 *   threads holding a count on the semaphore.  Their priority could have
 *   been boosted while they held the count.
 *
 * Parameters:
 *   stcb - The TCB of the task that was just started (if any).  If the
 *     post action caused a count to be given to another thread, then stcb
 *     is the TCB that received the count.  Note, just because stcb received
 *     the count, it does not mean that it it is higher priority than other
 *     threads.
 *   sem - A reference to the semaphore being posted.
 *     - If the semaphore count is <0 then there are still threads waiting
 *       for a count.  stcb should be non-null and will be higher priority
 *       than all of the other threads still waiting.
 *     - If it is ==0 then stcb refers to the thread that got the last count;
 *       no other threads are waiting.
 *     - If it is >0 then there should be no threads waiting for counts and
 *       stcb should be null.
 *
 * Return Value:
 *   None
 *
 * Assumptions:
 *   The scheduler is locked.
 *
 ****************************************************************************/

static inline void sem_restorebaseprio_irq(FAR struct tcb_s *stcb, FAR sem_t *sem)
{
	/* Perform the following actions only if a new thread was given a count.
	 * The thread that received the count should be the highest priority
	 * of all threads waiting for a count from the semaphore.  So in that
	 * case, the priority of all holder threads should be dropped to the
	 * next highest pending priority.
	 */

	if (stcb) {
		/* Drop the priority of all holder threads */

		(void)sem_foreachholder(sem, sem_restoreholderprio, stcb);
	}

	/* If there are no tasks waiting for available counts, then all holders
	 * should be at their base priority.
	 */

#ifdef CONFIG_DEBUG
	else {
		(void)sem_foreachholder(sem, sem_verifyholder, NULL);
	}
#endif
}

/****************************************************************************
 * Name: sem_restorebaseprio_task
 *
 * Description:
 *   This function is called after the current running task releases a
 *   count on the semaphore.  It will check if we need to drop the priority
 *   of any threads holding a count on the semaphore.  Their priority could
 *   have been boosted while they held the count.
 *
 * Parameters:
 *   stcb - The TCB of the task that was just started (if any).  If the
 *     post action caused a count to be given to another thread, then stcb
 *     is the TCB that received the count.  Note, just because stcb received
 *     the count, it does not mean that it it is higher priority than other
 *     threads.
 *   sem - A reference to the semaphore being posted.
 *     - If the semaphore count is <0 then there are still threads waiting
 *       for a count.  stcb should be non-null and will be higher priority
 *       than all of the other threads still waiting.
 *     - If it is ==0 then stcb refers to the thread that got the last count;
 *       no other threads are waiting.
 *     - If it is >0 then there should be no threads waiting for counts and
 *       stcb should be null.
 *
 * Return Value:
 *   None
 *
 * Assumptions:
 *   The scheduler is locked.
 *
 ****************************************************************************/

static inline void sem_restorebaseprio_task(FAR struct tcb_s *stcb, FAR struct tcb_s *htcb, FAR sem_t *sem)
{
	FAR struct semholder_s *pholder;

	/* Perform the following actions only if a new thread was given a count.
	 * The thread that received the count should be the highest priority
	 * of all threads waiting for a count from the semaphore.  So in that
	 * case, the priority of all holder threads should be dropped to the
	 * next highest pending priority.
	 */

	if (stcb) {
		/* The currently executed thread should be the lower priority
		 * thread that just posted the count and caused this action.
		 * However, we cannot drop the priority of the currently running
		 * thread -- because that will cause it to be suspended.
		 *
		 * So, do this in two passes.  First, reprioritizing all holders
		 * except for the running thread.
		 */

		(void)sem_foreachholder(sem, sem_restoreholderprioA, stcb);

		/* Now, find an reprioritize only the ready to run task */

		(void)sem_foreachholder(sem, sem_restoreholderprioB, stcb);
	}

	/* If there are no tasks waiting for available counts, then all holders
	 * should be at their base priority.
	 */

#ifdef CONFIG_DEBUG
	else {
		(void)sem_foreachholder(sem, sem_verifyholder, NULL);
	}
#endif

	/* In any case, the currently executing task should have an entry in the
	 * list.  Its counts were previously decremented; if it now holds no
	 * counts, then we need to remove it from the list of holders.
	 */

	pholder = sem_findholder(sem, htcb);
	if (pholder) {
		/* When no more counts are held, remove the holder from the list.  The
		 * count was decremented in sem_releaseholder.
		 */

		if (pholder->counts <= 0) {
			sem_freeholder(sem, pholder);
		}
	}
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sem_initholders
 *
 * Description:
 *   Called from sem_initialize() to set up semaphore holder information.
 *
 * Parameters:
 *   None
 *
 * Return Value:
 *   None
 *
 * Assumptions:
 *
 ****************************************************************************/

void sem_initholders(void)
{
#if CONFIG_SEM_PREALLOCHOLDERS > 0
	int i;

	/* Put all of the pre-allocated holder structures into the free list */

	g_freeholders = g_holderalloc;
	for (i = 0; i < (CONFIG_SEM_PREALLOCHOLDERS - 1); i++) {
		g_holderalloc[i].flink = &g_holderalloc[i + 1];
	}

	g_holderalloc[CONFIG_SEM_PREALLOCHOLDERS - 1].flink = NULL;
#endif
}

/****************************************************************************
 * Name: sem_destroyholder
 *
 * Description:
 *   Called from sem_destroyholder() to handle any holders of a semaphore
 *   when it is destroyed.
 *
 * Parameters:
 *   sem - A reference to the semaphore being destroyed
 *
 * Return Value:
 *   None
 *
 * Assumptions:
 *
 ****************************************************************************/

void sem_destroyholder(FAR sem_t *sem)
{
	/* It is an error if a semaphore is destroyed while there are any holders
	 * (except perhaps the thread release the semaphore itself).  Hmmm.. but
	 * we actually have to assume that the caller knows what it is doing because
	 * could have killed another thread that is the actual holder of the semaphore.
	 * We cannot make any assumptions about the state of the semaphore or the
	 * state of any of the holder threads.
	 *
	 * So just recover any stranded holders and hope the task knows what it is
	 * doing.
	 */

#if CONFIG_SEM_PREALLOCHOLDERS > 0
	if (sem->hhead) {
		sdbg("Semaphore destroyed with holders\n");
		(void)sem_foreachholder(sem, sem_recoverholders, NULL);
	}
#else
	if (sem->holder.htcb) {
		sdbg("Semaphore destroyed with holder\n");
	}

	sem->holder.htcb = NULL;
#endif
}

/****************************************************************************
 * Name: sem_findholder
 ****************************************************************************/

struct semholder_s *sem_findholder(sem_t *sem, FAR struct tcb_s *htcb)
{
	FAR struct semholder_s *pholder;

	/* Try to find the holder in the list of holders associated with this
	 * semaphore
	 */

#if CONFIG_SEM_PREALLOCHOLDERS > 0
	for (pholder = sem->hhead; pholder; pholder = pholder->flink)
#else
	pholder = &sem->holder;
#endif
	{
		if (pholder->htcb == htcb) {
			/* Got it! */

			return pholder;
		}
	}

	/* The holder does not appear in the list */

	return NULL;
}

/****************************************************************************
 * Name: sem_addholder_tcb
 *
 * Description:
 *   Called from sem_post() when the waiting thread obtains the semaphore.
 *
 * Parameters:
 *   htcb - TCB of the thread that just obtained the semaphore
 *   sem  - A reference to the incremented semaphore
 *
 * Return Value:
 *   None
 *
 * Assumptions:
 *   Interrupts are disabled.
 *
 ****************************************************************************/

void sem_addholder_tcb(FAR struct tcb_s *htcb, FAR sem_t *sem)
{
	FAR struct semholder_s *pholder;

	/* No needs to save holder for semaphore used for signaling */
	if ((sem->flags & FLAGS_SIGSEM) != 0) {
		return;
	}

#if defined(CONFIG_PRIORITY_INHERITANCE) && !defined(CONFIG_BINMGR_RECOVERY)
	/*
	 * If priority inheritance is disabled for this thread, then do not
	 * add the holder. If there are never holders of the semaphore,
	 * the priority inheritance is effectively disabled.
	 */
	if ((sem->flags & PRIOINHERIT_FLAGS_DISABLE) == 0) {
#endif
		/* Find or allocate a container for this new holder */
		pholder = sem_findorallocateholder(sem, htcb);
		if (pholder != NULL) {
			/*
			 * Then set the holder and increment the number of
			 * counts held by this holder
			 */
			pholder->htcb = htcb;
			pholder->counts++;
		}
#if defined(CONFIG_PRIORITY_INHERITANCE) && !defined(CONFIG_BINMGR_RECOVERY)
	}
#endif
}

/****************************************************************************
 * Name: sem_addholder
 *
 * Description:
 *   Called from sem_wait() or sem_trywait() when the calling thread
 *   obtains the semaphore
 *
 * Parameters:
 *   sem - A reference to the incremented semaphore
 *
 * Return Value:
 *   None
 *
 * Assumptions:
 *   Interrupts are disabled.
 *
 ****************************************************************************/
void sem_addholder(FAR sem_t *sem)
{
	FAR struct tcb_s *rtcb = this_task();
	sem_addholder_tcb(rtcb, sem);
}

/****************************************************************************
 * Name: sem_releaseholder
 *
 * Description:
 *   Called from sem_post() after a thread releases one count on the
 *   semaphore.
 *
 * Parameters:
 *   sem - A reference to the semaphore being posted
 *
 * Return Value:
 *   None
 *
 * Assumptions:
 *
 ****************************************************************************/

void sem_releaseholder(FAR sem_t *sem, FAR struct tcb_s *rtcb)
{
	FAR struct semholder_s *pholder;

	if ((sem->flags & FLAGS_SIGSEM) != 0) {
		/* No saved holder for semaphore used for signaling */
		return;
	}

	/* Find the container for this holder */

	pholder = sem_findholder(sem, rtcb);

	if (pholder && pholder->counts > 0) {
		/* Decrement the counts on this holder -- the holder will be freed
		 * later in sem_restorebaseprio.
		 */
		pholder->counts--;
	}
}

#ifdef CONFIG_PRIORITY_INHERITANCE
/****************************************************************************
 * Name: void sem_boostpriority(sem_t *sem)
 *
 * Description:
 *
 *
 * Parameters:
 *   None
 *
 * Return Value:
 *   None
 *
 * Assumptions:
 *
 ****************************************************************************/

void sem_boostpriority(FAR sem_t *sem)
{
	FAR struct tcb_s *rtcb = this_task();

	/* Boost the priority of every thread holding counts on this semaphore
	 * that are lower in priority than the new thread that is waiting for a
	 * count.
	 */

	(void)sem_foreachholder(sem, sem_boostholderprio, rtcb);
}

/****************************************************************************
 * Name: sem_restorebaseprio
 *
 * Description:
 *   This function is called after the current running task releases a
 *   count on the semaphore or an interrupt handler posts a new count.  It
 *   will check if we need to drop the priority of any threads holding a
 *   count on the semaphore.  Their priority could have been boosted while
 *   they held the count.
 *
 * Parameters:
 *   stcb - The TCB of the task that was just started (if any).  If the
 *     post action caused a count to be given to another thread, then stcb
 *     is the TCB that received the count.  Note, just because stcb received
 *     the count, it does not mean that it it is higher priority than other
 *     threads.
 *   htcb - The TCB of the task which already holds the semaphore.
 *   sem - A reference to the semaphore being posted.
 *     - If the semaphore count is <0 then there are still threads waiting
 *       for a count.  stcb should be non-null and will be higher priority
 *       than all of the other threads still waiting.
 *     - If it is ==0 then stcb refers to the thread that got the last count;
 *       no other threads are waiting.
 *     - If it is >0 then there should be no threads waiting for counts and
 *       stcb should be null.
 *
 * Return Value:
 *   None
 *
 * Assumptions:
 *   The scheduler is locked.
 *
 ****************************************************************************/

void sem_restorebaseprio(FAR struct tcb_s *stcb, FAR struct tcb_s *htcb, FAR sem_t *sem)
{
	/* Check our assumptions */

	DEBUGASSERT((sem->semcount > 0 && stcb == NULL) || (sem->semcount <= 0 && stcb != NULL));

	/* Handler semaphore counts posed from an interrupt handler differently
	 * from interrupts posted from threads.  The primary difference is that
	 * if the semaphore is posted from a thread, then the poster thread is
	 * a player in the priority inheritance scheme.  The interrupt handler
	 * externally injects the new count without otherwise participating
	 * itself.
	 */

	if (up_interrupt_context()) {
		sem_restorebaseprio_irq(stcb, sem);
	} else {
		sem_restorebaseprio_task(stcb, htcb, sem);
	}
}
#endif
/****************************************************************************
 * Name: sem_canceled
 *
 * Description:
 *   Called from sem_waitirq() after a thread that was waiting for a semaphore
 *   count was awakened because of a signal and the semaphore wait has been
 *   cancelled.  This function restores the correct thread priority of each
 *   holder of the semaphore.
 *
 * Parameters:
 *   sem - A reference to the semaphore no longer being waited for
 *
 * Return Value:
 *   None
 *
 * Assumptions:
 *
 ****************************************************************************/

#ifndef CONFIG_DISABLE_SIGNALS
void sem_canceled(FAR struct tcb_s *stcb, FAR sem_t *sem)
{
	/* Check our assumptions */

	DEBUGASSERT(sem->semcount <= 0);

	/* Adjust the priority of every holder as necessary */
#ifdef CONFIG_PRIORITY_INHERITANCE
	if ((sem->flags & PRIOINHERIT_FLAGS_DISABLE) == 0) {
		(void)sem_foreachholder(sem, sem_restoreholderprio, stcb);
	}
#endif
}
#endif

/****************************************************************************
 * Name: sem_enumholders
 *
 * Description:
 *   Show information about threads currently waiting on this semaphore
 *
 * Parameters:
 *   sem - A reference to the semaphore
 *
 * Return Value:
 *   None
 *
 * Assumptions:
 *
 ****************************************************************************/

#if defined(CONFIG_DEBUG) && defined(CONFIG_SEM_PHDEBUG)
void sem_enumholders(FAR sem_t *sem)
{
	(void)sem_foreachholder(sem, sem_dumpholder, NULL);
}
#endif

/****************************************************************************
 * Name: sem_nfreeholders
 *
 * Description:
 *   Return the number of available holder containers.  This is a good way
 *   to find out which threads are not calling sem_destroy.
 *
 * Parameters:
 *   sem - A reference to the semaphore
 *
 * Return Value:
 *   The number of available holder containers
 *
 * Assumptions:
 *
 ****************************************************************************/

#if defined(CONFIG_DEBUG) && defined(CONFIG_SEM_PHDEBUG)
int sem_nfreeholders(void)
{
#if CONFIG_SEM_PREALLOCHOLDERS > 0
	FAR struct semholder_s *pholder;
	int n;

	for (pholder = g_freeholders, n = 0; pholder; pholder = pholder->flink) {
		n++;
	}
	return n;
#else
	return 0;
#endif
}
#endif
