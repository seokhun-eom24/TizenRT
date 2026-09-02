---
status: accepted
---

# Bound Health Failure diagnostics

After the Minimum Fault Header and CPU freeze, print the offender and first-fault state, CPU register/current-task state, a compact PIDHASH-based summary of every live TCB, each reachable wait/held-semaphore and lock relation, the complete Health Monitor source array and deadline heap, and finally the existing `PANIC()` dump.

Do the primary detailed dump by reading validated raw structures without acquiring scheduler, semaphore, allocator, or VFS locks that a paused CPU may hold. Only after the core raw output is complete may existing locking dump routines run best-effort. Use whatever semaphore information the build already provides, including the first target's wait and held chains, 16 preallocated holders, and Binary Manager global semaphore list. Do not add a diagnostic Kconfig; print missing compile-time capabilities as `unavailable(config)`.

Validate kernel pointer range and alignment before dereference and cap every scheduler, held-semaphore, and holder linked-chain traversal independently at `CONFIG_MAX_TASKS`. Do not allocate a visited set: validate every next pointer and terminate by iteration count. On cap exhaustion, print the last valid pointer as `truncated` and continue with the next TCB or diagnostic stage. Always print a protected task's user-space semaphore address, but read through it only when the currently active mapping can be proven kernel-readable; do not switch address spaces or risk a page fault for diagnostics. Mark unsafe mappings as `unavailable` and corrupt pointers as `invalid`. Print register context and stack bounds for every live task, but restrict raw stack contents to the offender and each CPU's current task so the higher-value system-wide state has a chance to reach the UART before the 30-second reset.
