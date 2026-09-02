---
status: accepted
---

# Use monotonic Health Monitor state machines

Global state moves from `UNINITIALIZED` to either sticky `DISABLED` or `READY`, and only from `READY` to `FATAL`. Calls in those non-ready states return `-EAGAIN`, `-ENODEV`, and `-ESHUTDOWN`, respectively. Do not add a public availability query; mandatory integrations interpret `-ENODEV` on their first start as the boot-disabled condition.

Each source slot moves from `INACTIVE` through an internal `PREPARING` claim to `ARMED`, then back to `INACTIVE` through stop or managed teardown, or to terminal state through unexpected release. A release during `PREPARING` clears the slot without an unexpected-exit failure because the contract is not yet armed. Nested start observes `-EALREADY`; keepalive and stop observe `-ENOENT`. The API is not promised to be async-signal-safe.

Successful `ARMED` publication increments the slot's 32-bit lifecycle generation. Reserve zero as invalid and, rather than reusing a generation after `UINT32_MAX`, enter the self-check failure path when that slot is started again. This fail-stop rule prevents a stale heap node from acquiring a valid identity through wraparound. The release-store of `ARMED`, checkpoint atomic store, and compare-and-swap to `INACTIVE` are the start, keepalive, and stop linearization points.
