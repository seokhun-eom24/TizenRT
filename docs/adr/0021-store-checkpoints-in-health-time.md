---
status: accepted
---

# Store checkpoints in Health Time

Store each start and keepalive checkpoint in Health Time, not raw global system time. Read the accumulated PM sleep ticks atomically, subtract them from `clock_systimer()`, and publish the result with one release atomic store. Otherwise all PM sleep before a checkpoint would incorrectly extend its next deadline. The extra atomic load remains part of the five-microsecond keepalive performance gate.
