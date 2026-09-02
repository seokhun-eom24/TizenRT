---
status: accepted
---

# Exclude PM sleep from Health Time

Do not count intentional PM sleep toward task Health Contract deadlines or the HW watchdog feed interval. Define Health Time as the 64-bit global system tick minus accumulated PM sleep ticks. The PM resume path publishes its compensated sleep duration through an internal Health Monitor hook adjacent to system-tick compensation; this is not a public task API. Ordinary awake-time tick loss, scheduler starvation, and IRQ masking remain part of Health Time and are not subtracted.

On resume, keep tick interrupts, all secondary CPUs, and ordinary tasks quiesced while calculating sleep ticks, compensating the global system tick, adding exactly the same value to Health Monitor's accumulator, and validating the resulting Health Time relationship. Only then resume tick delivery, secondary CPUs, and ordinary scheduling. This prevents keepalive from observing the compensated global tick before the sleep accumulator. If the first target cannot guarantee this system-wide quiescent window, its Health Monitor bring-up fails. Duplicate resume publication, an accumulator larger than the global tick, or regressing Health Time is a self-check failure.
