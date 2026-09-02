---
status: accepted
---

# Enforce target performance and fault-injection gates

On rtl8730e, require worst observed keepalive time at or below 5us, the healthy CPU0 tick fast path at or below 10us, and a repair-budget overflow followed by a 256-slot scan and Floyd heapify at or below 250us. Verify from disassembly that runtime paths contain no semaphore, allocation, VFS, or libatomic calls.

Use deterministic fake-time/fake-watchdog tests for state including `PREPARING`, deadlines, PM compensation, lazy-heap repair, epoch churn, and first-fault behavior; SMP race stress for task publication and per-CPU exit snapshot ownership against CPU0 processing; and board fault injection for all reboot paths, managed teardown, disabled initialization, 30-second expiration, adapter-return-based ten-second feed cadence, and PM pause. Measure that no secondary CPU or task runs inside the resume compensation window. Reject a hardware configuration that resets earlier than the requested 30 seconds or cannot provide the required resume quiescence.
