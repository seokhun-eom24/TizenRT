---
status: accepted
---

# Trust Health Monitor device callers

Treat protected and loadable binaries in the first port as trusted code and add no separate authorization model to `/dev/health_monitor`. The interface is self-only, but a caller can deliberately arm a short timeout and stop making progress to force a system reset. Document that capability as part of the trusted-binary boundary.
