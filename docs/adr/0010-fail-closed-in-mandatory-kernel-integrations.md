---
status: accepted
---

# Fail closed in mandatory kernel integrations

Treat any nonzero return from `health_monitor_start()`, `health_monitor_keepalive()`, or `health_monitor_stop()` in the first-port HPWORK, LPWORK, `log_dump`, and `binary_manager` integrations as `HEALTH_MONITOR_API_FAILURE` after Health Monitor has reached `READY`. Enter the same first-fault diagnostic pipeline instead of silently continuing without the intended health contract.

The one exception is the first mandatory `start()` observing `-ENODEV` after boot established sticky `DISABLED`, which means watchdog initialization failed before hardware start. That integration skips monitoring for the remainder of that boot. It must not reinterpret any later error as the disabled case.

Each mandatory thread keeps a boot-lifetime `monitoring_available` flag. Only that first `-ENODEV` changes it from true to false; the thread then skips all later Health Monitor calls without repeated probing or logging. Once start has succeeded, or for any other first-start error such as `-EAGAIN`, `-ENOENT`, or `-EALREADY`, fail closed.

This fail-closed rule applies to those trusted mandatory kernel integrations. A malformed or state-invalid `/dev/health_monitor` ioctl from a general caller returns its documented errno and does not itself reboot the system.

The internal `health_monitor_report_api_failure(op, error)` is noreturn and is called only from task context with IRQs enabled. It publishes the token and spins with only a compiler barrier, regardless of whether its CAS wins. It does not enter PM, idle, or WFI and does not alter unknown interrupt or critical-section nesting. CPU0's tick completes the fatal pipeline; if the precondition was already violated or the tick is dead, the missing Health Monitor kick leaves the 30-second hardware watchdog as the externally bounded termination.
