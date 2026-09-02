---
status: accepted
---

# Disable Health Monitor on pre-start initialization failure

Prepare static state and hooks, register `/dev/health_monitor`, and complete all core provisioning before invoking the watchdog adapter. Make the adapter's fail-atomic `initialize(30000)` the last fallible operation; after it starts the irreversible HW watchdog, only a non-failing atomic `READY` publication remains. If any earlier step or adapter initialization fails, emit a low-level boot diagnostic, leave Health Monitor in `DISABLED`, and continue boot without starting the watchdog. Calls made in that state return `-ENODEV`; they do not silently succeed. Mandatory first-port integrations skip monitoring when boot established the disabled state, while errors after `READY` remain fatal integration failures.

The rtl8730e watchdog cannot be stopped by software after it starts. Consequently, failure after a successful start cannot use this fail-open policy and is handled as a Health Failure. Hardware timeout representation and PM pause behavior remain bring-up gates because the target HAL does not report programming success.

The `DISABLED` state is sticky for the rest of that boot. Do not provide runtime retry or late activation; a reboot is required to attempt initialization again.

Initialize Health Monitor at the beginning of `os_bringup()`, before the paging and workqueue threads. Remove the rtl8730e board-initialization call that configures the watchdog for five seconds. This guarantees that every first-port monitored thread starts only after global state is `READY` or sticky `DISABLED`.
