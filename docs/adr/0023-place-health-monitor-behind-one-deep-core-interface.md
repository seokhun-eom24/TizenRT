---
status: accepted
---

# Place Health Monitor behind one deep core interface

Keep state machines, source slots, lazy deadline indexing, Health Time, and fatal diagnostics inside `os/kernel/health_monitor/`. Expose the public task and device contract from `os/include/tinyara/health_monitor.h`. Make `/dev/health_monitor`, scheduler tick, task lifecycle, PM, and rtl8730e watchdog code thin adapters or hooks into the core. Do not expose source-slot or heap-node types across the seam.

This concentrates policy and invariants in one deep module while retaining the necessary VFS and hardware adapters. Test through the same core interface with deterministic fake time and watchdog adapters.
