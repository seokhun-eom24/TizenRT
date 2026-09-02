---
status: accepted
---

# Self-check global and touched local invariants

Continuously check global magic/inverse, monotonic global state, first-fault checksum, fatal/kick-eligibility/dump relationships, Health Time monotonicity, nondecreasing membership epoch, heap bounds, valid-heap epoch equality, and the local ordering paths touched by heap operations. A kick is eligible only for `READY` with a zero first-fault token. Validate PIDHASH, PID, and lifecycle relationships for each ARMED slot that CPU0 examines. Do not add a periodic full heap-to-source audit.

If a nonzero first-fault token has an invalid checksum, do not trust its encoded offender. Use a CPU0-local emergency record and enter the self-check failure path.
