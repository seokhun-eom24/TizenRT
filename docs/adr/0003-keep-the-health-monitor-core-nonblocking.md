---
status: accepted
---

# Keep the Health Monitor core non-blocking

Health Monitor runtime control, timer processing, task-release handling, failure confirmation, fatal latching, and HW watchdog feed suppression must not allocate memory, wait on synchronization primitives, perform file I/O, sleep, or acquire a scheduler-global lock. One-time `health_monitor_initialize()` provisioning may allocate memory, register the watchdog driver, and block. The strict rule starts after initialization. Health Monitor owns HW watchdog initialization, 30-second configuration, start, and periodic feed.

The board lower-half's existing bounded register-busy polling during HW watchdog refresh is an explicit normal-path exception; Health Monitor does not introduce a separate try-refresh operation.

Once the first failure is latched and Health Monitor feeding has stopped, the Fatal Diagnostic Phase may use blocking operations to maximize diagnostic information. It emits a Minimum Fault Header before acquiring locks, then may enter a critical section, pause other CPUs, traverse scheduler state, run existing dumps, and finally invoke the panic/reset path. The watchdog remains a fallback unless another unchanged `/dev/watchdog0` user feeds it. Keep the generic panic crashdump source unchanged; on the first target that crashdump is disabled in the configuration and the hardware cannot be stopped by software after start, so the watchdog remains active through `PANIC()`.
