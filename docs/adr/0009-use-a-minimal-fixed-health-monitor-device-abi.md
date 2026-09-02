---
status: accepted
---

# Use a minimal fixed Health Monitor device ABI

Expose `START`, `KEEPALIVE`, `STOP`, and `QUERY_SELF` through `/dev/health_monitor`. `START` takes a scalar 32-bit millisecond timeout. The fixed v1 query result contains only `state`, `timeout_ms`, and `remaining_ms`, all as `uint32_t`. It contains no `version`, caller-buffer `size`, reserved space, PID, lifecycle version, or raw tick.

Allocate ioctl type base `0x2c00` and command numbers 1 through 4 in that order. Require zero arguments for keepalive and stop and a writable status pointer for query. Return `-ENOTTY` for unknown types or commands, `-EFAULT` for null or unverifiable user pointers, and `-EINVAL` for value-format errors such as a zero timeout or nonzero unused argument. Validate command and argument form before global Health Monitor state. Expose only `INACTIVE=0` and `ARMED=1`; an internal `PREPARING` observation returns `-EAGAIN` rather than becoming public ABI.

A caller-buffer size would allow one query command to evolve across independently updated kernel and loadable-binary versions, but it is not required for the current contract. Keep the ABI minimal and immutable instead. If a future query needs more fields, allocate a new ioctl command and leave the v1 structure unchanged.

Register the stateless driver with mode `0666` under the accepted trusted-caller boundary. Open and close neither create nor remove a registration, and there is no file-private owner state: an ioctl through a shared descriptor always operates on the calling current task. Do not implement read, write, poll, or mmap operations.
