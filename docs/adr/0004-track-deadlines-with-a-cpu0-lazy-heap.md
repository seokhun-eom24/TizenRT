---
status: accepted
---

# Track deadlines with a CPU0-owned bounded lazy heap

Use fixed external source slots as the registration source of truth and a fixed-capacity CPU0-owned lazy min-heap as the deadline index. A monitored task never mutates the heap: keepalive publishes one 64-bit checkpoint tick with a lock-free atomic store, while start and stop publish membership changes through an epoch. Because keepalive only extends an armed deadline, each heap snapshot remains a safe lower bound. CPU0 validates a due root against its source slot and repairs an extended root.

Limit stale-root repair to an initial budget of 16 per tick. If the budget is exceeded or membership changes, scan the bounded source array and rebuild with Floyd heapify in the same tick so deadline detection is not deferred. Do not periodically traverse the entire heap for self-audit; validate magic values, bounds, monotonic global-state relationships, lifecycle versions at use, and local invariants on mutated heap paths. This keeps task hot paths constant-time and free of scheduler-global synchronization while bounding timer-path work.

Attempt at most one membership rebuild per tick. Compare the membership epoch before and after the source scan; publish the rebuilt heap only if the epoch stayed stable. If it changed, use that scan for current expiration checks, discard the derived heap, and leave rebuild pending for the next tick. Continuous churn therefore degrades to one bounded full scan per tick instead of causing an unbounded ISR retry loop. Revalidate any expiration candidate with final acquire loads before claiming the fault; a same-tick keepalive or stop wins only when visible to that final check.

Each observed membership-epoch change causes one full source scan and heap rebuild, coalescing multiple changes in the same tick. Do not add a dirty bitmap or pending-minimum optimization. Integrations must not open and close Scoped Monitoring around packets, audio frames, or each high-frequency callback. The first workqueue port groups consecutive ready callbacks into a Busy Batch, starting once before the first callback, publishing a checkpoint after each completed callback, and stopping before idle wait.
