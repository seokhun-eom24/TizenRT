---
status: accepted
---

# Use ceiling timeout conversion

Convert the full public range `1..UINT32_MAX` milliseconds to ticks with 64-bit ceiling division, compute the deadline from Health Time, and declare expiration when `health_now >= deadline`. This never expires a contract before its requested duration. Treat overflow of `health_now + timeout_ticks` as a self-check failure rather than wrapping. Report `QUERY_SELF.remaining_ms` with the same ceiling conversion, saturate to `UINT32_MAX`, and return zero at or after the deadline.
