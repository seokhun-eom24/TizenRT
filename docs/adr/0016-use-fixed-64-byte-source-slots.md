---
status: accepted
---

# Use fixed 64-byte source slots

Use one aligned 64-byte source slot for each `CONFIG_MAX_TASKS` entry. It contains a packed atomic state/lifecycle control word, an aligned atomic checkpoint tick, timeout milliseconds, PID, app ID, and the 32-byte first-port task name. Do not retain a TCB pointer. Use 16-byte heap nodes containing only the derived deadline identity.

At 256 tasks this costs 16KiB for source slots and 4KiB for the heap. Including per-CPU TCB exit snapshots and global state, target approximately 21KiB of static memory and verify the actual size with build-time assertions and the linker map.

Require build-time assertions that the public status structure is 12 bytes, a source slot is 64 bytes with at least 8-byte alignment, a heap node is 16 bytes, and all 32/64-bit atomics used by the target are lock-free.
