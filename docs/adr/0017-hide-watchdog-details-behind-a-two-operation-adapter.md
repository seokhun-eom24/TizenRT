---
status: accepted
---

# Hide watchdog details behind a two-operation adapter

The generic Health Monitor core depends on an internal watchdog adapter with only `initialize(timeout_ms)` and `kick()` operations. Initialization hides driver registration, timeout configuration, reset mode, and irreversible hardware start. A negative result guarantees the watchdog was not started; success guarantees it is running. Kick is bounded, nonblocking, and safe from the CPU0 timer path. Do not expose stop, pause, status, VFS, or lower-half details to the core.

Use an rtl8730e production adapter and a deterministic fake adapter in tests. This keeps the seam real while concentrating target-specific behavior behind a small interface.
