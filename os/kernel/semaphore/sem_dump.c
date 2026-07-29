/****************************************************************************
 *
 * Copyright 2026 Samsung Electronics All Rights Reserved.
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
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

#undef  CONFIG_DEBUG
#undef  CONFIG_DEBUG_ERROR
#define CONFIG_DEBUG 1
#define CONFIG_DEBUG_ERROR 1

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <semaphore.h>
#include <debug.h>
#include <sys/types.h>
#include <tinyara/arch.h>
#include <tinyara/sched.h>

#include "semaphore/semaphore.h"

/* NOTE : This file is for assert usage only.
 *
 * There is no global registry of semaphores in the kernel, so the set of
 * semaphores that can be reported is discovered indirectly, from the state
 * that the scheduler already keeps:
 *
 *   1. tcb->waitsem  - the semaphore each blocked task is waiting on
 *   2. tcb->holdsem  - the per-task list of semaphores whose counts the task
 *                      currently owns (priority inheritance builds only)
 *
 * Consequence: an idle, uncontended semaphore with no registered holder is
 * invisible to this dump.  That is acceptable for crash analysis, where the
 * interesting semaphores are by definition the contended ones.
 *
 * Everything here runs after the system is already broken, so the code is
 * written defensively: no dynamic allocation, no critical section (a dead or
 * spinning peer CPU must not be able to hang the dump), bounded traversal of
 * every list, and a plausibility check before dereferencing any pointer that
 * was recovered from a possibly corrupt TCB.
 */

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Bounds.  All storage is static or on the caller's stack; keep the stack
 * footprint small because the asserting task may itself be near overflow.
 */

#define SEMDUMP_MAX_SEMS            32	/* Max semaphores reported */
#define SEMDUMP_MAX_CHAIN           16	/* Max nodes walked in any one list */
#define SEMDUMP_HOLDER_TEXT_SIZE    40	/* Holder column scratch buffer */
#define SEMDUMP_HOLDER_COL          24	/* Holder column print width */

#if defined(CONFIG_PRIORITY_INHERITANCE) && CONFIG_SEM_PREALLOCHOLDERS > 0
#define SEMDUMP_HAVE_HOLDER_LIST    1
#endif

#ifdef CONFIG_PRIORITY_INHERITANCE
#define SEMDUMP_ALIGN_MASK          (sizeof(FAR void *) - 1)
#else
#define SEMDUMP_ALIGN_MASK          (sizeof(int16_t) - 1)
#endif

/* Reject obviously bogus pointers recovered from a corrupt TCB */

#define SEMDUMP_MIN_ADDR            0x100

#define SEMDUMP_ARGS_FORMAT "%-9s | %8s | %5s | %-*s | %s\n"
#define SEMDUMP_ARGS        "SEM ADDR", "SEMCOUNT", "FLAGS", \
	SEMDUMP_HOLDER_COL, "HOLDERS(PID/COUNTS)", "WAITERS(PID)"

#define SEMDUMP_ROW_FORMAT  "%8p | %8d | 0x%02x | %-*s | "

#define SEMDUMP_SEPARATOR \
	"--------------------------------------------------------------------\n"
#define SEMDUMP_SECTION \
	"===========================================================\n"

/****************************************************************************
 * Private Type Declarations
 ****************************************************************************/

struct semdump_inventory_s {
	FAR sem_t *sems[SEMDUMP_MAX_SEMS];
	unsigned int count;
	bool truncated;
};

struct semdump_waiters_s {
	FAR sem_t *sem;
	unsigned int count;
	bool truncated;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: semdump_plausible
 *
 * Description:
 *   Cheap sanity filter applied before any dereference.  This cannot prove
 *   that an address is mapped, but it removes the garbage patterns (NULL,
 *   small integers, misaligned words, poison values) that a corrupt TCB
 *   typically yields, which is enough to keep the dump itself from taking a
 *   nested fault in the common case.
 ****************************************************************************/

static bool semdump_plausible(FAR const void *ptr)
{
	uintptr_t addr = (uintptr_t)ptr;

	if (addr < SEMDUMP_MIN_ADDR) {
		return false;
	}

	if ((addr & SEMDUMP_ALIGN_MASK) != 0) {
		return false;
	}

	return true;
}

/****************************************************************************
 * Name: semdump_inventory_add
 ****************************************************************************/

static void semdump_inventory_add(FAR struct semdump_inventory_s *inventory, FAR sem_t *sem)
{
	unsigned int i;

	if (!semdump_plausible(sem)) {
		return;
	}

	for (i = 0; i < inventory->count; i++) {
		if (inventory->sems[i] == sem) {
			return;
		}
	}

	if (inventory->count >= SEMDUMP_MAX_SEMS) {
		inventory->truncated = true;
		return;
	}

	inventory->sems[inventory->count] = sem;
	inventory->count++;
}

/****************************************************************************
 * Name: semdump_inventory_tcb
 ****************************************************************************/

static void semdump_inventory_tcb(FAR struct tcb_s *tcb, FAR void *arg)
{
	FAR struct semdump_inventory_s *inventory = (FAR struct semdump_inventory_s *)arg;
#ifdef SEMDUMP_HAVE_HOLDER_LIST
	FAR struct semholder_s *holder;
	unsigned int walked;
#endif

	semdump_inventory_add(inventory, tcb->waitsem);

#ifdef SEMDUMP_HAVE_HOLDER_LIST
	holder = tcb->holdsem;
	for (walked = 0; walked < SEMDUMP_MAX_CHAIN; walked++) {
		if (!semdump_plausible(holder)) {
			break;
		}

		semdump_inventory_add(inventory, holder->sem);
		holder = holder->tlink;
	}

	if (walked >= SEMDUMP_MAX_CHAIN) {
		inventory->truncated = true;
	}
#endif
}

/****************************************************************************
 * Name: semdump_stale_holder
 *
 * Description:
 *   Return true if the holder's TCB back-pointer no longer agrees with the
 *   scheduler's PID table.  That means the semaphore is still crediting
 *   counts to a task that has exited (or to a TCB that has been reused),
 *   i.e. a phantom holder.  Such an entry corrupts the count bookkeeping on
 *   the next post/wait and is a strong candidate root cause whenever the
 *   SEMCOUNT and the waiter list disagree.
 ****************************************************************************/

#ifdef CONFIG_PRIORITY_INHERITANCE
static bool semdump_stale_holder(FAR struct semholder_s *holder)
{
	FAR struct tcb_s *htcb = (FAR struct tcb_s *)holder->htcb;

	if (!semdump_plausible(htcb)) {
		return true;
	}

	return (sched_gettcb(htcb->pid) != htcb);
}

/****************************************************************************
 * Name: semdump_holder_text
 *
 * Description:
 *   Format the holder list into a caller supplied buffer.  A '!' suffix marks
 *   a holder that failed semdump_stale_holder().  Returns the number of
 *   holders found and, via nsuspect, how many of them looked stale.
 ****************************************************************************/

static unsigned int semdump_holder_text(FAR sem_t *sem, FAR char *buf, size_t buflen,
					FAR int *counts, FAR unsigned int *nsuspect)
{
	unsigned int nholder = 0;
	size_t used = 0;
	int written;
	bool stale;
#if CONFIG_SEM_PREALLOCHOLDERS > 0
	FAR struct semholder_s *holder;
	unsigned int walked;
#endif

	*counts = 0;
	*nsuspect = 0;
	buf[0] = '\0';

#if CONFIG_SEM_PREALLOCHOLDERS > 0
	holder = sem->hhead;
	for (walked = 0; walked < SEMDUMP_MAX_CHAIN; walked++) {
		if (!semdump_plausible(holder)) {
			break;
		}

		stale = semdump_stale_holder(holder);
		if (stale) {
			(*nsuspect)++;
		}

		written = snprintf(&buf[used], buflen - used, "%s%d/%d%s",
				   (nholder > 0) ? "," : "",
				   semdump_plausible(holder->htcb) ?
					(int)((FAR struct tcb_s *)holder->htcb)->pid : -1,
				   (int)holder->counts, stale ? "!" : "");
		if (written < 0 || (size_t)written >= buflen - used) {
			/* Out of room: mark the column as elided and stop.  The
			 * counts below are still accumulated for the sanity checks.
			 */

			if (buflen >= 4) {
				used = buflen - 4;
				buf[used++] = '.';
				buf[used++] = '.';
				buf[used++] = '.';
				buf[used] = '\0';
			}

			buflen = used + 1;	/* Suppress further appends */
		} else {
			used += (size_t)written;
		}

		*counts += (int)holder->counts;
		nholder++;
		holder = holder->flink;
	}
#else
	if (semdump_plausible(sem->holder.htcb) || sem->holder.counts != 0) {
		stale = semdump_stale_holder(&sem->holder);
		if (stale) {
			(*nsuspect)++;
		}

		written = snprintf(buf, buflen, "%d/%d%s",
				   semdump_plausible(sem->holder.htcb) ?
					(int)((FAR struct tcb_s *)sem->holder.htcb)->pid : -1,
				   (int)sem->holder.counts, stale ? "!" : "");
		UNUSED(written);
		*counts += (int)sem->holder.counts;
		nholder++;
	}
#endif

	if (nholder == 0) {
		snprintf(buf, buflen, "none");
	}

	return nholder;
}
#endif							/* CONFIG_PRIORITY_INHERITANCE */

/****************************************************************************
 * Name: semdump_waiter
 ****************************************************************************/

static void semdump_waiter(FAR struct tcb_s *tcb, FAR void *arg)
{
	FAR struct semdump_waiters_s *ctx = (FAR struct semdump_waiters_s *)arg;

	if (tcb->waitsem != ctx->sem) {
		return;
	}

	if (ctx->count >= SEMDUMP_MAX_CHAIN) {
		ctx->truncated = true;
		return;
	}

	/* Printed incrementally rather than buffered: the waiter list is the last
	 * column, is unbounded in principle, and lldbg_noarg() writes straight to
	 * the low level console without line buffering.
	 */

	lldbg_noarg("%s%d", (ctx->count > 0) ? ", " : "", (int)tcb->pid);
	ctx->count++;
}

/****************************************************************************
 * Name: semdump_row
 *
 * Description:
 *   Emit one table row, followed by a warning line for each inconsistency
 *   detected, in the same spirit as the STACK OVERFLOW annotation in
 *   task_taskdump().
 ****************************************************************************/

static void semdump_row(FAR sem_t *sem)
{
	struct semdump_waiters_s waiters;
	char holders[SEMDUMP_HOLDER_TEXT_SIZE];
	unsigned int nholder = 0;
	unsigned int nsuspect = 0;
	int hcounts = 0;
	int semcount;
	int expected;

	semcount = (int)sem->semcount;
	expected = (semcount < 0) ? -semcount : 0;

#ifdef CONFIG_PRIORITY_INHERITANCE
	nholder = semdump_holder_text(sem, holders, sizeof(holders), &hcounts, &nsuspect);
#else
	snprintf(holders, sizeof(holders), "n/a");
#endif

	lldbg_noarg(SEMDUMP_ROW_FORMAT, sem, semcount, (unsigned int)sem->flags,
		    SEMDUMP_HOLDER_COL, holders);

	waiters.sem = sem;
	waiters.count = 0;
	waiters.truncated = false;
	sched_foreach(semdump_waiter, &waiters);

	if (waiters.count == 0) {
		lldbg_noarg("none");
	}

	if (waiters.truncated) {
		lldbg_noarg(", ...");
	}

	lldbg_noarg("\n");

	/* Consistency checks.
	 *
	 * A negative count is defined as the number of tasks blocked on the
	 * semaphore, so it must agree with the number of TCBs whose waitsem
	 * points here.  A mismatch means the count and the wait queue have
	 * diverged - either a post that failed to wake a waiter (lost signal) or
	 * a count that was consumed without a corresponding block.
	 */

	if (!waiters.truncated && expected != (int)waiters.count) {
		lldbg_noarg("  !!! SEM (%p) COUNT/WAITER MISMATCH : semcount %d implies %d waiter(s), found %u !!!\n",
			    sem, semcount, expected, waiters.count);
	}

	if (nsuspect > 0) {
		lldbg_noarg("  !!! SEM (%p) HAS %u PHANTOM HOLDER(S) (marked '!') !!!\n", sem, nsuspect);
	}

#ifdef CONFIG_PRIORITY_INHERITANCE
	/* Under priority inheritance every count checked out of the semaphore
	 * should be attributed to some holder.  Signaling semaphores and
	 * semaphores with priority inheritance disabled are excluded.
	 */

	if ((sem->flags & (PRIOINHERIT_FLAGS_DISABLE | FLAGS_SIGSEM)) == 0 &&
	    semcount <= 0 && nholder == 0) {
		lldbg_noarg("  !!! SEM (%p) IS PRIO-INHERIT AND EXHAUSTED BUT HAS NO HOLDER !!!\n", sem);
	}

	if (nholder > 0 && hcounts <= 0) {
		lldbg_noarg("  !!! SEM (%p) HOLDER COUNT SUM IS %d (expected > 0) !!!\n", sem, hcounts);
	}
#else
	UNUSED(nholder);
	UNUSED(hcounts);
#endif
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sem_show_seminfo
 *
 * Description:
 *   Dump one semaphore in detail.  Intended to be called with the asserting
 *   task's tcb->waitsem, as a companion to task_show_tcbinfo().
 *
 * NOTE : This function is for assert usage only.
 ****************************************************************************/

void sem_show_seminfo(FAR sem_t *sem)
{
	lldbg_noarg(SEMDUMP_SECTION);
	lldbg_noarg("Semaphore info \n");
	lldbg_noarg(SEMDUMP_SECTION);

	if (!semdump_plausible(sem)) {
		lldbg("Address     : %p (not a plausible semaphore)\n", sem);
		return;
	}

	lldbg("Address     : %p\n", sem);
	lldbg("Semcount    : %d\n", (int)sem->semcount);
	lldbg("Flags       : 0x%02x\n", (unsigned int)sem->flags);

	lldbg_noarg(SEMDUMP_ARGS_FORMAT, SEMDUMP_ARGS);
	lldbg_noarg(SEMDUMP_SEPARATOR);
	semdump_row(sem);
	lldbg_noarg(SEMDUMP_SEPARATOR);
}

/****************************************************************************
 * Name: sem_show_alivesem_list
 *
 * Description:
 *   Dump every semaphore reachable from the TCBs of the live tasks: the
 *   semaphores tasks are blocked on, plus the semaphores whose counts they
 *   hold.  Semaphores with neither a waiter nor a registered holder are not
 *   discoverable and are not listed.
 *
 * NOTE : This function is for assert usage only.
 ****************************************************************************/

void sem_show_alivesem_list(void)
{
	struct semdump_inventory_s inventory;
	unsigned int i;

	inventory.count = 0;
	inventory.truncated = false;
	sched_foreach(semdump_inventory_tcb, &inventory);

	lldbg_noarg(SEMDUMP_SECTION);
	lldbg_noarg("List of semaphores referenced by all tasks in the system\n");
	lldbg_noarg(SEMDUMP_SECTION);

	lldbg_noarg(SEMDUMP_ARGS_FORMAT, SEMDUMP_ARGS);
	lldbg_noarg(SEMDUMP_SEPARATOR);

	if (inventory.count == 0) {
		lldbg_noarg("  no semaphore is referenced by any task\n");
	}

	for (i = 0; i < inventory.count; i++) {
		semdump_row(inventory.sems[i]);
	}

	lldbg_noarg(SEMDUMP_SEPARATOR);

	if (inventory.truncated) {
		lldbg_noarg("  semaphore list truncated (limit %d, or a corrupt holder chain)\n",
			    SEMDUMP_MAX_SEMS);
	}
}
