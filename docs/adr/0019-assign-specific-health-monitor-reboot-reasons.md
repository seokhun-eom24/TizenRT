---
status: accepted
---

# Assign specific Health Monitor reboot reasons

Assign system reboot reasons 62 through 65 to Health Monitor timeout, unexpected task exit, self-check failure, and mandatory integration API failure. Write the first-fault reason after the Minimum Fault Header and before blocking diagnostics. Preserve it across the later panic path. A stopped tick ISR cannot write a software reason, so a pure hardware-watchdog reset remains the existing reason 54.
