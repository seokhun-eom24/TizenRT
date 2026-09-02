---
status: accepted
---

# Publish first faults to the CPU0 executor

Task-release hooks and mandatory kernel integrations publish a fixed 64-bit first-fault token with an atomic compare-and-swap. Allocate its bits as an 8-bit reason, 16-bit source slot, 32-bit lifecycle generation, and 8-bit checksum. Reserve a slot value for faults without an offender and reject `CONFIG_MAX_TASKS > 65535` at build time. Publishers do not dump or panic in their own context. The CPU0 tick ISR acquires the token, suppresses Health Monitor feeding, and exclusively executes the fatal diagnostic pipeline. A terminal source slot remains stable until that pipeline begins.

Compute the checksum as CRC-8/ATM (`poly=0x07`, `init=0`) over the lower seven bytes from least to most significant. Token zero is the only no-fault representation, and valid reason codes are nonzero. Do not maintain a separate mutable feed-enable flag: a hardware kick is eligible only while global state is `READY` and the token is zero.

CPU0 checks the token again immediately before a hardware kick. Do not add cross-CPU blocking solely to serialize a token publication against a register kick already in progress; once CPU0 observes the token, no later kick is allowed.

Because `sched_releasetcb()` runs with interrupts disabled and frees the TCB, each CPU owns one fixed exit snapshot. An unexpected active release first changes its matching slot from `ARMED` to `TERMINAL`. If the global token is already nonzero it does not touch a snapshot. Otherwise it copies a slot/generation/magic header and the common `struct tcb_s`, release-publishes the snapshot identity, and then attempts the global token CAS. Competing CPUs copy independently. Once a token wins, later hooks cannot overwrite any snapshot, and CPU0 consumes only the per-CPU snapshot whose identity matches the winning token. A missing or corrupt match is logged as unavailable without aborting the rest of the dump. Pointer fields are retained as diagnostic values but are never dereferenced after release.
