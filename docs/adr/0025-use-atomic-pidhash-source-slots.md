---
status: accepted
---

# Use direct PIDHASH source slots with atomic metadata

Map a task directly to `g_health_slots[PIDHASH(pid)]` and validate that the corresponding PIDHASH entry contains the current TCB and full PID. Treat a mismatch as a Health Monitor self-check failure rather than a normal API error. This gives task operations and the central release hook the same O(1) identity without a registration search or TCB pointer in the source slot.

Store the control, checkpoint, timeout, full PID, app ID, and eight 32-bit task-name words atomically. Start writes metadata and checkpoint before release-publishing `ARMED+generation`; CPU0 reads control, the atomic fields, and control again, accepting only an unchanged armed generation. Atomic metadata avoids a C data race when stop and immediate restart overlap a CPU0 scan, while keepalive still mutates only its checkpoint.

Use a lock-free 64-bit membership epoch incremented by start, stop, and managed teardown, never keepalive. Do not wrap lifecycle generation or membership epoch: attempts to advance past `UINT32_MAX` or `UINT64_MAX` enter the self-check failure path.
