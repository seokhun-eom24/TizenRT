---
status: accepted
---

# Replace task_monitor with Health Monitor

Remove the existing `task_monitor` feature and replace it with Health Monitor rather than extending it. `task_monitor` depends on blocking primitives, treats a context restore as proof of health, and silently unregisters a terminating task, which conflicts with the explicit Health Contract and disappearance detection.

Remove its Kconfig and defconfig entries, TCB fields, lifecycle hooks, implementation files, public `PR_MONITOR_REGISTER`/`PR_MONITOR_UPDATE` names, and `task_prctl.c` cases. Leave enum values 8 and 9 unused with a former-monitor comment and assign the following prctl member explicit value 10, so old binaries receive default `EINVAL` and later values do not shift. Do not reuse those positions for Health Monitor.
