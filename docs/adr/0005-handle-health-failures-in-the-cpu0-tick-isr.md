---
status: accepted
---

# Complete Health Failure handling in the CPU0 tick ISR

Detect and complete Health Failure handling in the CPU0 tick ISR rather than deferring it to a task that could be starved by the failure being diagnosed. After first-fault-wins latching, stop Health Monitor's HW watchdog feeding and emit a lock-free Minimum Fault Header as at most two direct-low-UART lines. The first contains reason, raw token, CPU, Health Time, global tick, accumulated sleep, and last-kick tick. The second contains source slot, generation, state, PID, app, sanitized 32-byte name, timeout, checkpoint, and deadline, using `NA` when there is no source. Use only a fixed-size integer formatter, with no traversal or dynamic formatting. Then enter the Fatal Diagnostic Phase, where blocking synchronization and existing SMP pause facilities are allowed so the system can stabilize and print as much TCB, semaphore, lock, register, Health Monitor slot, and heap information as possible. Invoke `PANIC()` after the detailed dump.

After the Minimum Fault Header and before entering blocking diagnostics, persist a Health Monitor-specific reboot reason. Use separate system reasons for timeout, unexpected exit, self-check failure, and mandatory integration API failure. The existing assert path only writes its generic reason when no reason is present, so it does not overwrite the first fault.

An unfed 30-second watchdog can reset a stalled critical-section, CPU-pause, dump, or panic path, although the unchanged generic watchdog driver means another caller could still feed it. Preserve the generic panic crashdump source unchanged. On rtl8730e the first-port configuration does not enable that crashdump and the hardware cannot be stopped after start, so the watchdog remains active through `PANIC()`. If the tick ISR itself stops, only the hardware watchdog reset path remains.
