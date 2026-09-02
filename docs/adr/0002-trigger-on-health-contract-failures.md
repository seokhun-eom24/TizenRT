---
status: accepted
---

# Trigger only on Health Contract failures

Health Monitor treats a missed survival signal, termination without an explicit stop, monitor self-check failure, and tick ISR stoppage as failures. Semaphore waits, long scheduler locks, and CPU monopolization are diagnostic evidence collected after a failure rather than independent global reboot triggers, avoiding false resets while registered kernel tasks still expose system-wide starvation through missed signals.
