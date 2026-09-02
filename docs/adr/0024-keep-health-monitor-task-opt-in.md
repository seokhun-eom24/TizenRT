---
status: accepted
---

# Keep Health Monitor task-opt-in without a CPU heartbeat

Use only explicitly registered tasks and their liveness checkpoints as runtime health contracts. If a secondary CPU stops scheduling or servicing interrupts and this prevents a registered task from reaching its checkpoint, CPU0 detects that task's timeout. If no registered task depends on the affected CPU while CPU0 and all other registered tasks remain healthy, Health Monitor does not detect the isolated CPU failure.

Do not add an implicit per-CPU heartbeat. Such a heartbeat would introduce a new global reboot trigger outside the accepted opt-in task boundary and partially restore the global monitoring behavior intentionally removed with `sched_lock` surveillance. Document the non-guarantee in the integration guide and require a task registration when a subsystem needs this coverage.
