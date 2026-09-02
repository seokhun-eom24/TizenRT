---
status: accepted
---

# Feed the hardware watchdog every ten seconds

Health Monitor owns board HW watchdog lower-half registration, initialization, 30,000ms configuration, start, and periodic kick. Move the existing board boot 5,000ms initialization into Health Monitor provisioning. Feed every 10,000ms only after the CPU0 Health Monitor check succeeds. Initialization may use blocking driver operations, but the CPU0 tick ISR uses an architecture-specific direct lower-half hook because the generic ioctl takes a semaphore. Keep both the generic `/dev/watchdog0` ABI and the rtl8730e lower-half's bounded register-busy polling in `watchdog_refresh()`.

When `CONFIG_HEALTH_MONITOR` is enabled, integration guidance forbids every other module from using `/dev/watchdog0`; no enforcement is added in the first port. This preserves the existing driver while making Health Monitor the operational owner of timeout, mode, and kick.

Health Time immediately after the watchdog adapter returns successfully is the feed cadence origin, so the first runtime kick occurs ten seconds after that return rather than immediately. Initialization time before the irreversible start does not consume the first interval. The first-port board stops the HW watchdog counter while PM sleep is active and resumes it on wake, so Health Monitor does not schedule a periodic PM wake solely to feed it. PM sleep is excluded from the ten-second feed interval. Bring-up must verify the 30-second representation and the pause/resume behavior. After a Health Failure, Health Monitor never feeds again, but unchanged generic driver users are not forcibly blocked. Keep the generic panic crashdump source unchanged; on rtl8730e it is not configured and the running watchdog cannot be stopped by software, so it remains a reset fallback through `PANIC()`.
