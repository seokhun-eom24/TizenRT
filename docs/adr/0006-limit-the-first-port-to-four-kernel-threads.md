---
status: accepted
---

# Limit the first port to four kernel threads

Integrate only HPWORK, LPWORK, `log_dump`, and `binary_manager` in the first `rtl8730e/loadable_ext_ddr_st7785` port. HPWORK and LPWORK use Scoped Monitoring over a Busy Batch, with 5,000ms and 1,000ms timeouts respectively; idle wait is outside the contract. `log_dump` scopes actual compression-request processing after `mq_receive()`, and `binary_manager` scopes one dispatched request after `mq_receive()`; their module owners choose their timeout values.

Allow only one active contract per task. The outer HPWORK/LPWORK Busy Batch owns the worker task's slot, so callbacks executed inside it must not call Health Monitor directly. The outer `log_dump` and `binary_manager` request loops likewise are their task's only slot owners. Do not add nested scope stacks or owner identifiers.

Do not integrate uwork, LWIP, NDP health checking, binary loader children, networking, Wi-Fi, BLE, media, audio, or application threads in this first port. Health Monitor supplies no central timeout default or per-integration Kconfig: every module developer owns the value passed to `health_monitor_start(timeout_ms)` and the semantic placement of its Liveness Checkpoints.
