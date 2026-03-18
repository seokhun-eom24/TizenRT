# TizenRT API Documentation Guide

This document defines the documentation standards and guidelines for the TizenRT project using Doxygen.

## Table of Contents

1. [Overview](#overview)
2. [Group Hierarchy](#group-hierarchy)
3. [Comment Format](#comment-format)
4. [Documentation Priority](#documentation-priority)
5. [Best Practices](#best-practices)
6. [Building Documentation](#building-documentation)

---

## Overview

TizenRT uses Doxygen for API documentation. All public APIs should be documented following the conventions in this guide to ensure consistency and completeness.

### Key Principles

- **Consistency**: Follow the same format across all files
- **Completeness**: Document all public APIs with parameters, return values, and examples
- **Clarity**: Write clear, concise descriptions that help developers understand the API

---

## Group Hierarchy

APIs are organized into hierarchical groups using `@defgroup` and `@ingroup` tags.

### Top-Level Structure

```
TIZENRT (Root)
├── KERNEL
│   ├── SCHED_KERNEL      - Scheduler APIs (sched_lock, sched_yield, etc.)
│   ├── TASK_KERNEL       - Task management (task_create, task_delete, etc.)
│   ├── PTHREAD_KERNEL    - POSIX threads (pthread_create, pthread_join, etc.)
│   ├── SEMAPHORE_KERNEL  - Semaphores (sem_init, sem_wait, sem_post, etc.)
│   ├── MQUEUE_KERNEL     - Message queues (mq_open, mq_send, mq_receive, etc.)
│   ├── SIGNAL_KERNEL     - Signals (sigaction, kill, sigwait, etc.)
│   ├── TIME_KERNEL       - Time management (nanosleep, clock_gettime, etc.)
│   ├── CLOCK_KERNEL      - Clock functions (clock_settime, clock_getres, etc.)
│   └── IRQ_KERNEL        - Interrupt handling
│
├── FILESYSTEM
│   ├── VFS_KERNEL        - Virtual filesystem (open, close, read, write, etc.)
│   ├── FCNTL_KERNEL      - File control (fcntl, etc.)
│   ├── DIRENT_KERNEL     - Directory operations (opendir, readdir, closedir, etc.)
│   └── MOUNT_KERNEL      - Mount operations (mount, umount, etc.)
│
├── MEMORY
│   ├── MM_KERNEL         - Memory management (malloc, free, etc.)
│   └── SHM_KERNEL        - Shared memory (shmat, shmdt, shmget, etc.)
│
├── NETWORK
│   ├── SOCKET            - Socket APIs (socket, bind, listen, connect, etc.)
│   ├── NETMGR            - Network manager
│   └── DNS               - DNS APIs
│
├── DRIVERS
│   ├── SERIAL_DRIVER     - Serial/UART drivers
│   ├── SPI_DRIVER        - SPI drivers
│   ├── I2C_DRIVER        - I2C drivers
│   └── GPIO_DRIVER       - GPIO drivers
│
├── LIBC
│   ├── STDIO_LIBC        - Standard I/O (printf, scanf, fopen, etc.)
│   ├── STDLIB_LIBC       - Standard library (atoi, strtol, qsort, etc.)
│   ├── STRING_LIBC       - String functions (strcpy, strlen, memcpy, etc.)
│   └── MATH_LIBC         - Math functions (sin, cos, sqrt, etc.)
│
└── FRAMEWORK
    ├── WIFI_MANAGER      - WiFi management
    ├── BLE_MANAGER       - BLE management
    └── TASK_MANAGER      - Task lifecycle management
```

### Defining Groups

Groups are defined in header files using `@defgroup`:

```c
/**
 * @defgroup SCHED_KERNEL SCHED
 * @brief Scheduler and task switching APIs
 * @ingroup KERNEL
 * @{
 */

/* API declarations here */

/** @} */  /* End of SCHED_KERNEL */
```

### Using Groups

Individual functions use `@ingroup` to belong to a group:

```c
/**
 * @ingroup SCHED_KERNEL
 * @brief Disable context switching
 */
int sched_lock(void);
```

---

## Comment Format

### Header Files (Public API)

Use this template for public API documentation in header files:

```c
/**
 * @ingroup GROUP_NAME
 * @brief Brief description (verb phrase, capitalize first letter)
 *
 * @details
 * Detailed description of the function's behavior, use cases,
 * and important notes. Can span multiple lines.
 *
 * @param[in]     param1  Description of input parameter
 * @param[out]    param2  Description of output parameter
 * @param[in,out] param3  Description of input/output parameter
 *
 * @return Description of return value
 * @retval OK       Success
 * @retval -EINVAL  Invalid argument
 * @retval -ENOMEM  Out of memory
 * @retval -ENOENT  No such entry
 *
 * @par Example
 * @code
 * #include <header.h>
 *
 * int result = my_function(arg1, arg2);
 * if (result < 0) {
 *     // Handle error
 * }
 * @endcode
 *
 * @note Important notes or caveats (optional)
 * @warning Warnings about potential issues (optional)
 *
 * @see related_function1()
 * @see related_function2()
 *
 * @since TizenRT vX.X
 */
int my_function(int param1, int *param2, int *param3);
```

### Source Files (Implementation)

Use this template for source file documentation:

#### File Header

```c
/**
 * @file filename.c
 * @brief Brief description of the file (one line)
 *
 * Detailed description of what this file implements and any
 * important implementation notes. (optional)
 */
```

#### Function Documentation

```c
/**
 * @brief Brief description of the function
 *
 * Implementation-specific details that complement (not duplicate)
 * the header file documentation. Focus on internal behavior.
 *
 * @return Description of return value
 */
```

### Parameter Direction Tags

| Tag | Description |
|-----|-------------|
| `@param[in]` | Input parameter (read-only) |
| `@param[out]` | Output parameter (write-only) |
| `@param[in,out]` | Input/output parameter (read and write) |

### Common Return Values

| Return Value | Description |
|--------------|-------------|
| `OK` (0) | Success |
| `ERROR` (-1) | Generic error |
| `-EINVAL` | Invalid argument |
| `-ENOMEM` | Out of memory |
| `-ENOENT` | No such file or entry |
| `-EBUSY` | Resource busy |
| `-ETIMEDOUT` | Operation timed out |
| `-ENOSYS` | Function not implemented |
| `-EPERM` | Operation not permitted |

---

## Documentation Priority

Document APIs in the following order of priority:

### Priority 1: Public API Headers
- `os/include/*.h` - Standard POSIX-like APIs
- `os/include/sys/*.h` - System APIs
- `os/include/tinyara/*.h` - TizenRT-specific APIs

### Priority 2: System Calls
- Functions registered in `os/syscall/syscall.csv`
- User-space accessible kernel functions

### Priority 3: Framework APIs
- `framework/include/wifi_manager/*.h`
- `framework/include/ble_manager/*.h`
- `framework/include/task_manager/*.h`
- Other framework public APIs

### Priority 4: Internal Kernel Functions (Optional)
- Kernel internal functions in `os/kernel/`
- Only document complex or frequently referenced functions

---

## Best Practices

### Do's

1. **Start @brief with a verb**: "Disable", "Create", "Initialize", not "This function disables"
2. **Document all parameters**: Include direction (`[in]`, `[out]`, `[in,out]`)
3. **Document all return values**: Use `@retval` for specific values
4. **Include examples**: For complex APIs, show usage with `@par Example`
5. **Use @see for related functions**: Help users discover related APIs
6. **Keep header and source docs complementary**: Don't duplicate, extend

### Don'ts

1. **Don't state the obvious**: "param1 is the first parameter" is not helpful
2. **Don't duplicate code in description**: Explain what, not how
3. **Don't use implementation details in headers**: Keep headers abstract
4. **Don't leave stale documentation**: Update docs when code changes
5. **Don't use abbreviations without explanation**: First use should be spelled out

### Formatting Guidelines

- Use spaces after `*` in block comments
- Align parameter descriptions for readability
- Keep lines under 80 characters when possible
- Use consistent capitalization (sentence case for descriptions)

---

## Building Documentation

### Prerequisites

Install Doxygen:
```bash
# Ubuntu/Debian
sudo apt install doxygen

# macOS
brew install doxygen

# Optional: Install graphviz for diagrams
sudo apt install graphviz
```

### Generate Documentation

```bash
cd tools/doxygen
doxygen Doxyfile
```

### View Documentation

Open in browser:
```bash
# Linux
xdg-open ../../docs/api/html/index.html

# macOS
open ../../docs/api/html/index.html
```

### Output Location

Generated documentation is placed in:
```
docs/api/
├── html/           # HTML documentation
│   ├── index.html  # Main page
│   ├── modules.html # Group/module listing
│   └── ...
└── (other formats if enabled)
```

---

## Migration from Old Format

When updating existing documentation, convert from the old format:

### Old Format (NuttX-style)
```c
/************************************************************************
 * Name:  function_name
 *
 * Description:
 *   Description here
 *
 * Inputs
 *   param - description
 *
 * Return Value:
 *   OK on success
 *
 ************************************************************************/
```

### New Format (Doxygen-style)
```c
/**
 * @brief Brief description
 *
 * @details
 * Detailed description here.
 *
 * @param[in] param Description
 *
 * @return OK on success
 */
```

---

## Version History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | 2026-03-17 | Initial documentation guide |
