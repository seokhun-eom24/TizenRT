# Health Monitor Implementation Specification

Status: ready-for-agent

## Problem Statement

TizenRT의 기존 `task_monitor`는 blocking primitive에 의존하고, 단순 context restore를 생존 증거로 사용하며, 감시 중인 task가 사라질 때 이를 장애로 보존하지 못한다. 이 동작은 실제 작업 진행을 확인하는 명시적 Health Contract, 정상 runtime에서의 non-blocking 보장, task 소멸 감지, 풍부한 장애 진단, HW watchdog을 통한 최종 reset이라는 요구를 만족하지 못한다.

사용자는 `sdog`라는 기존 설계와 `task_monitor` 구현을 확장하는 대신 `health_monitor`라는 새로운 커널 기능을 원한다. 각 task가 스스로 timeout을 설정하고 의미 있는 Liveness Checkpoint에서 직접 keepalive해야 하며, timeout이나 비정상 종료가 발생하면 최초 장애를 보존하고 시스템 전체의 TCB, semaphore, lock, CPU 및 Health Monitor 상태를 최대한 출력한 후 reset해야 한다. 정상 감시 경로는 allocation, blocking synchronization, VFS 및 scheduler-global lock에 의존해서는 안 되고, 첫 포팅 보드에서는 Health Monitor가 30초 HW watchdog의 초기화와 10초 주기 kick까지 소유해야 한다.

## Solution

`task_monitor`를 완전히 제거하고 task-opt-in 방식의 Health Monitor를 도입한다. Kernel task는 세 개의 self-only C API로 Health Contract를 시작하고, 갱신하고, 중지한다. Protected task는 같은 동작을 stateless character device의 ioctl로 사용한다. 등록의 source of truth는 TCB 밖의 고정 source-slot 배열이며, task의 keepalive는 자기 checkpoint에 대한 lock-free atomic store 하나만 수행한다.

CPU0 tick 경로가 고정 용량 lazy min-heap을 소유하고 가장 가까운 deadline만 정상적으로 검사한다. Start, stop 및 managed teardown은 membership epoch를 갱신하며, CPU0가 membership 변경이나 repair budget 초과를 발견하면 같은 tick에서 bounded source scan과 Floyd heapify로 복구한다. Source slot이 유일한 진실이고 heap은 언제든 재구축 가능한 파생 index다.

Timeout, 감시 중 일반 task 소멸, Health Monitor 자기검증 실패 또는 mandatory integration API 실패가 발생하면 first-fault-wins token을 publish한다. CPU0 tick ISR은 더 이상 HW watchdog을 kick하지 않고, lock-free Minimum Fault Header를 출력하고, persistent reboot reason을 기록한 뒤, 다른 CPU를 멈추고 시스템 상태를 최대한 dump한다. 핵심 raw dump가 끝나면 기존 dump와 `PANIC()`을 실행한다. 어느 단계든 정지하면 이미 kick이 중단된 30초 HW watchdog이 reset한다. Tick ISR 자체가 멈춘 경우에는 software dump 없이 기존 watchdog reset reason만 남는다.

첫 포팅은 `rtl8730e/loadable_ext_ddr_st7785`이며 HPWORK, LPWORK, `log_dump`, `binary_manager` 네 kernel thread만 mandatory integration한다. Idle과 나머지 kernel/application thread는 이번 범위에서 제외한다.

## User Stories

1. As a kernel module developer, I want to start a Health Contract with a module-owned timeout, so that my module defines what timely progress means.
2. As a kernel module developer, I want to publish keepalive only at an explicit Liveness Checkpoint, so that scheduling activity is not mistaken for useful work.
3. As a kernel module developer, I want to stop a Health Contract when scoped work completes, so that intentional idle waits are not treated as failures.
4. As a kernel module developer, I want all task APIs to operate only on the calling task, so that another task cannot forge my liveness.
5. As a kernel module developer, I want duplicate start and inactive keepalive or stop to fail clearly, so that integration bugs are visible.
6. As a kernel module developer, I want stop followed by start to be supported, so that request-scoped monitoring can repeat.
7. As a kernel module developer, I want a contract to become active only after start publication completes, so that partially initialized state is never monitored.
8. As a kernel module developer, I want one active contract per task, so that ownership and timeout meaning remain unambiguous.
9. As a workqueue maintainer, I want HPWORK monitoring to cover an entire ready-work batch, so that idle waiting is excluded while slow callbacks are detected.
10. As a workqueue maintainer, I want HPWORK to use a 5,000ms timeout, so that long-running high-priority callbacks are caught.
11. As a workqueue maintainer, I want LPWORK to use a 1,000ms timeout, so that the accepted low-priority worker contract is enforced.
12. As a workqueue maintainer, I want a checkpoint after every completed callback, so that a long batch remains healthy when each callback progresses.
13. As a callback developer, I want guidance that the outer worker owns the Health Contract, so that nested registration does not reboot the system.
14. As a log-dump maintainer, I want monitoring to begin after the blocking receive and before compression work, so that idle queue waits are excluded.
15. As a Binary Manager maintainer, I want one request dispatch to form one scoped contract, so that request hangs are detected without monitoring idle receive.
16. As a module owner, I want to choose my own timeout in code rather than Kconfig, so that policy stays with the module that understands the operation.
17. As a protected-task developer, I want the same start, keepalive, stop, and self-query behavior through a device, so that protected code can participate without direct kernel calls.
18. As a protected-task developer, I want to reuse an already opened descriptor for runtime operations, so that keepalive does not open files or allocate.
19. As a protected-task developer, I want device operations to apply to the calling thread even when a descriptor is shared, so that liveness remains self-only.
20. As a protected-task developer, I want inactive and armed status with timeout and remaining time, so that I can inspect my own contract without learning kernel identities.
21. As an ABI maintainer, I want a minimal immutable query structure, so that the first ABI does not carry speculative version or reserved fields.
22. As an ABI maintainer, I want unknown commands, invalid pointers, and invalid values to have distinct errno results, so that callers can diagnose misuse.
23. As a security integrator, I want the trusted-caller assumption documented, so that the ability to provoke a reboot is understood.
24. As a scheduler maintainer, I want registration state outside the TCB, so that an exiting task cannot erase the only evidence that it was monitored.
25. As a scheduler maintainer, I want O(1) source-slot lookup from PIDHASH identity, so that runtime operations do not scan tasks.
26. As a scheduler maintainer, I want full PID and lifecycle generation validation, so that PID reuse cannot impersonate an old registration.
27. As a scheduler maintainer, I want generation wrap to fail safely, so that stale heap identities never become valid through ABA.
28. As a scheduler maintainer, I want task release during start preparation to remain non-fatal, so that a contract that never armed is not reported as abandoned.
29. As a reliability engineer, I want an armed task that returns, exits, is cancelled, or is deleted without stop to be a Health Failure, so that disappearing work is detected.
30. As a Binary Manager maintainer, I want update, unload, and fault-recovery termination through managed teardown to be normal, so that intentional binary removal does not reboot.
31. As a Binary Manager maintainer, I want teardown cause to be attached only to each actual target task, so that unrelated tasks are not exempted.
32. As a reliability engineer, I want first-fault-wins behavior, so that later failures do not overwrite the initiating cause.
33. As a reliability engineer, I want timeout and exit races resolved by final atomic validation, so that a same-deadline keepalive or stop has deterministic semantics.
34. As a performance engineer, I want keepalive to mutate only one atomic checkpoint, so that monitored hot paths remain cheap.
35. As a performance engineer, I want monitored tasks never to reorder the deadline heap, so that ordering cost stays off task execution paths.
36. As a performance engineer, I want CPU0 to check the nearest deadline first, so that healthy tick cost does not scale with all registered tasks.
37. As a performance engineer, I want lazy stale-root repair with a fixed budget, so that repeated keepalive does not cause unbounded ISR work.
38. As a reliability engineer, I want budget exhaustion to fall back to a same-tick bounded scan and heap rebuild, so that expiration is never deferred for performance.
39. As a reliability engineer, I want the source array to remain authoritative when the heap is stale or invalid, so that a derived index cannot hide failure.
40. As a platform engineer, I want all normal Health Monitor runtime paths free of allocation, semaphores, mutexes, file I/O, sleep, and scheduler-global locks, so that the monitor does not depend on facilities it diagnoses.
41. As a platform engineer, I want one-time initialization to be allowed to block, so that driver and device provisioning remains practical.
42. As a platform engineer, I want Health Monitor to own HW watchdog initialization, 30-second configuration, start, and kick, so that health decisions and reset progression share one owner.
43. As a platform engineer, I want the existing watchdog device ABI to remain available, so that unrelated driver compatibility is preserved.
44. As a platform engineer, I want Health Monitor runtime kick to bypass blocking watchdog ioctl synchronization, so that CPU0 tick remains non-blocking.
45. As a platform engineer, I want the first kick ten seconds after successful watchdog initialization returns, so that the 30-second watchdog has a clear cadence margin.
46. As a platform engineer, I want initialization failure before HW start to disable Health Monitor for that boot, so that boot can continue without a partially active reset mechanism.
47. As a mandatory-integration maintainer, I want the first disabled-state result to suppress later Health Monitor calls for that boot, so that initialization failure does not create repeated errors.
48. As a mandatory-integration maintainer, I want every other API error to fail closed, so that required monitoring never disappears silently.
49. As a field engineer, I want different persistent reboot reasons for timeout, unexpected exit, self-check, and mandatory API failure, so that fleet failures can be classified.
50. As a field engineer, I want tick-ISR loss to retain the existing HW watchdog reason, so that a failure the software could not observe is not misclassified.
51. As a field engineer, I want a Minimum Fault Header before locks or CPU pause, so that at least the initiating fault survives a diagnostic deadlock.
52. As a field engineer, I want all live TCB summaries, CPU contexts, wait semaphores, held semaphores, holder chains, and lock state when available, so that root-cause evidence is maximized.
53. As a field engineer, I want unavailable, invalid, and truncated data distinguished, so that missing diagnostics are not mistaken for valid zero values.
54. As a field engineer, I want raw stack contents limited to the offender and current tasks, so that higher-value system-wide information fits before reset.
55. As a field engineer, I want an exiting task's TCB copied before release, so that the first unexpected exit remains diagnosable after memory is freed.
56. As an SMP maintainer, I want per-CPU immutable exit snapshots selected by token identity, so that concurrent releases cannot overwrite the winner.
57. As an SMP maintainer, I want CPU0 alone to execute the fatal pipeline, so that multiple CPUs do not produce competing dumps or resets.
58. As an SMP maintainer, I want a final token check immediately before each HW kick, so that a newly published failure stops future feeding.
59. As a PM maintainer, I want intentional sleep excluded from Health Time and feed cadence, so that healthy sleep does not expire contracts.
60. As a PM maintainer, I want global tick and sleep compensation published while all CPUs and tasks are quiesced, so that no keepalive observes half-compensated time.
61. As a platform engineer, I want bring-up to fail when 30 seconds cannot be represented or PM quiescence cannot be guaranteed, so that unsupported hardware does not appear healthy.
62. As a reliability engineer, I want self-checks over magic, state monotonicity, token integrity, time relations, heap bounds, epoch relations, and touched heap paths, so that monitor corruption becomes a Health Failure.
63. As a reliability engineer, I want no periodic full heap audit, so that healthy runtime overhead stays bounded.
64. As a reliability engineer, I want an isolated secondary-CPU failure detected when it starves a registered task, so that opt-in subsystem coverage works across CPUs.
65. As a system architect, I want the absence of a per-CPU heartbeat documented, so that unregistered isolated-CPU failures are an explicit non-guarantee.
66. As a release engineer, I want measurable keepalive, healthy-tick, and fallback-rebuild budgets, so that performance regressions block release.
67. As a release engineer, I want deterministic fake-time and fake-watchdog coverage, so that boundary races can be reproduced without waiting in real time.
68. As a release engineer, I want physical fault injection for every reboot path, so that software assertions are backed by target evidence.
69. As a maintainer, I want the old task-monitor prctl names removed while their numeric positions remain unused, so that the feature is truly removed without shifting later ABI values.
70. As a maintainer, I want a single Health Monitor enable symbol and no timeout-policy Kconfig, so that configuration remains small and module policy remains local.

## Implementation Decisions

### Naming, replacement, and configuration

- The feature and all new public/internal naming use `health_monitor`; `sdog` and `task_monitor` are obsolete names.
- Remove the existing task-monitor implementation, build configuration, defconfig entry, public header, TCB activity field, bring-up thread, lifecycle integration, and utility wrappers.
- Remove the two public task-monitor prctl identifiers and their handlers. Keep numeric values 8 and 9 unused and assign the following prctl enum member explicit value 10 so later values do not shift. An old binary calling 8 or 9 receives the default invalid-option result.
- Add one feature-enable symbol only. Do not add configuration for integration timeouts, device path, watchdog expiration, feed period, heap capacity, repair budget, or diagnostic capability.
- Use the system maximum-task count as source-slot and heap capacity. Reject a configuration above 65,535 because one first-fault token slot value is reserved.
- When the feature is enabled, missing mandatory tick, lifecycle, PM, architecture watchdog, or low-level output hooks are build failures.

### Public task contract

- Expose only `health_monitor_start(timeout_ms)`, `health_monitor_keepalive()`, and `health_monitor_stop()` as public C operations. There is no public release operation.
- Operations are self-only and task-context-only. ISR calls fail with permission error.
- Start accepts every nonzero 32-bit millisecond timeout. Duplicate or nested start fails as already active. Keepalive and stop on a non-armed slot fail as not found.
- Stop permits a later restart. Each successful armed publication creates a new lifecycle generation.
- A task must call keepalive itself after passing a developer-selected Liveness Checkpoint. Context restore, CPU allocation, or another task's action is not evidence of health.
- One task has at most one active contract. Nested scopes, scope stacks, delegate keepalive, owner identifiers, and multiple independent deadlines per task are unsupported.
- Lifetime Monitoring and Scoped Monitoring are both supported; the integration owner documents which interval is covered and what is not guaranteed outside it.
- API error precedence is context, global state, argument, then slot state. Uninitialized, disabled, and fatal global states report retry-later, device-unavailable, and shutdown respectively.
- Public APIs are not promised to be async-signal-safe.

### State and publication model

- Global state is monotonic: uninitialized transitions once to ready or sticky disabled; ready transitions only to fatal.
- Slot state uses inactive, an internal preparing state, armed, and terminal. Start claims inactive as preparing, writes metadata and checkpoint, then release-publishes armed. Only armed publication begins the Health Contract.
- A nested start observing preparing reports already active; keepalive, stop, or query observing preparing reports inactive/retry according to their established contracts.
- Release during preparing clears the slot without an unexpected-exit failure because no contract was armed.
- Normal stop and managed teardown move armed to inactive. General release moves armed to terminal, which is not reused before reset.
- Start armed publication, checkpoint atomic store, and stop inactive compare-and-swap are the linearization points.
- Map each task directly to the source slot selected by its PID hash. Validate the corresponding live TCB and full PID; mismatch is a self-check failure, not caller misuse.
- Use a 32-bit lifecycle generation with zero reserved. Attempting to advance beyond the maximum value is a self-check failure rather than wraparound.
- Store control, checkpoint, timeout, full PID, app ID, and the fixed task name in lock-free atomic words. The fixed name occupies eight 32-bit atomic words.
- CPU0 accepts a source snapshot only when acquire-loaded control before and after atomic metadata reads is the same armed generation. A changed snapshot is discarded without unbounded retry.
- Start, stop, and managed teardown increment a lock-free 64-bit membership epoch; keepalive does not. Advancing beyond its maximum is a self-check failure.

### Time and deadline semantics

- Use Health Time: 64-bit global system tick minus accumulated intentional PM-sleep ticks.
- Store target checkpoints and CPU0 deadlines as 64-bit ticks. Require lock-free 64-bit atomic support on the first target. A future target may publish a 32-bit low checkpoint expanded by CPU0, but that fallback is not part of the first port.
- Keepalive reads global time and the PM-sleep accumulator, then performs one release atomic store to its checkpoint. The PM accumulator load does not change the single-mutation contract.
- Convert milliseconds to ticks with 64-bit ceiling division. Timeout zero is invalid; the full remaining 32-bit range is supported.
- Compute deadline as checkpoint plus timeout ticks. Addition overflow is a self-check failure.
- Expiration occurs when Health Time is greater than or equal to deadline. There is no grace tick.
- At the exact deadline, keepalive or stop wins only if CPU0's final acquire validation observes it before publishing the fault.
- Query converts remaining ticks to milliseconds with ceiling division, returns zero at or beyond deadline, and saturates at the maximum public value.

### Source storage and deadline index

- A fixed source-slot array is the sole source of truth. It does not retain a TCB pointer.
- Each source slot is 64 bytes with at least 8-byte alignment. Each derived heap node is 16 bytes and contains only slot, lifecycle identity, and a CPU0-observed deadline snapshot.
- The expected first-target static cost is 16KiB for 256 source slots, 4KiB for 256 heap nodes, plus per-CPU exit snapshots and small global state, targeting approximately 21KiB total.
- Use a bounded CPU0-owned lazy min-heap as the deadline index. Monitored tasks never insert, remove, or reorder heap nodes.
- Because armed keepalive only extends a deadline, a heap snapshot is a safe lower bound. A future root proves every indexed live deadline is future. A due root is revalidated against its source slot; an extended root is repaired with its latest deadline.
- Permit at most 16 consecutive stale-root repairs per tick initially. If the budget is exceeded, perform a bounded full source scan, expiration decision, and Floyd heapify in the same tick.
- A changed membership epoch forces CPU0 to scan the source array before root early-out and rebuild the heap. Multiple changes before one tick coalesce into one rebuild.
- Scan membership epoch before and after rebuild. If it changed during the scan, use the scan's stable observations for that tick's expiration decisions, discard the derived heap, and retry rebuilding on the next tick.
- Under continuous membership churn, degrade to one bounded maximum-task scan per tick. Do not spin or postpone a discovered expiration.
- Treat lifecycle-mismatched heap nodes as stale identities, never as a reused task registration.
- Do not perform a periodic full source-to-heap audit. Validate global invariants and only the local heap paths touched by operations.

### Protected-device ABI

- Provide a stateless character device with start, keepalive, stop, and query-self ioctls at type base `0x2c00`, command numbers 1 through 4.
- Start takes a scalar 32-bit timeout. Keepalive and stop require a zero unused argument. Query requires a writable pointer to the fixed status result.
- The public status contains exactly three 32-bit fields: state, timeout milliseconds, and remaining milliseconds. Its size is fixed at 12 bytes.
- Public status exposes inactive value 0 and armed value 1 only. It does not expose preparing, terminal, global state, PID, generation, raw ticks, version, size, or reserved fields.
- Unknown ioctl type or command reports not-a-tty; a null or unsafe user pointer reports bad address; invalid scalar form reports invalid argument. Validate command and argument shape before global state.
- First open may block. Runtime callers reuse the descriptor; ioctl execution after open must honor the non-blocking runtime contract.
- Open and close do not create, stop, or release a registration. There is no file-private owner state, so a shared descriptor always acts on the calling current task.
- Register the device with mode 0666 under the accepted trusted-caller boundary. Do not provide read, write, poll, or memory-map behavior.
- If future query data is needed, allocate a new command and leave the first status layout unchanged.

### Task lifecycle and managed teardown

- Store only an internal release-cause marker in the TCB, not Health Monitor registration state.
- Release cause is normal or managed binary teardown. It is consumed once in the central release path before PID identity is cleared.
- Mark a validated target immediately before removal through the managed-unloaded termination path. If removal fails and the same TCB/PID remains live, restore normal cause.
- Mark every removal target separately; do not inherit cause to children or all tasks in a binary implicitly.
- Update, unload, and fault recovery initiated by the managed-unloaded termination path are normal Managed Binary Teardown. Direct deletion outside that path is an unexpected exit.
- Return, process exit, pthread cancellation, and task deletion of an armed task without stop are unexpected exits.
- Managed teardown changes armed to inactive and increments membership epoch. Release during preparing clears inactive without an epoch change or failure.
- General armed release first changes the slot to terminal. If no first-fault token exists, it writes a per-CPU exit snapshot before token publication.
- Each per-CPU snapshot has magic and slot/generation identity followed by the common TCB copy. Publish snapshot identity before attempting the global token compare-and-swap.
- Once any first-fault token is nonzero, later release hooks do not write snapshots. CPU0 uses only the snapshot whose identity matches the winning unexpected-exit token.
- Snapshot pointer fields are printed as historical addresses only and are never dereferenced after TCB release.

### Failure model and first-fault execution

- Software-detected Health Failures are timeout, armed task disappearance, self-check failure, and mandatory integration API failure.
- Semaphore wait, held scheduler lock, and CPU occupation are diagnostic evidence, not independent reboot triggers.
- Tick ISR stoppage is not software-detectable; it produces the existing HW watchdog reset classification without the software fatal pipeline.
- Use one 64-bit atomic first-fault token with 8-bit reason, 16-bit source slot, 32-bit lifecycle generation, and 8-bit CRC.
- Reserve one slot encoding for source-less failures. Valid reason values are nonzero; token zero alone means no fault.
- Compute CRC-8/ATM over the lower seven bytes in fixed least-significant-byte order.
- Task-release and mandatory-integration reporters publish a token only; they do not dump or panic in their own context.
- CPU0 tick is the only fatal-pipeline executor. It acquire-loads the token, transitions ready to fatal, and never returns to normal feeding.
- Do not maintain a separate mutable feed-enable flag. A kick is eligible only when global state is ready and the token is zero.
- Recheck the token immediately before the HW register kick. Accept only a few-cycle overlap with a kick already in progress on another publication; no kick may begin after CPU0 observes the token.
- Preserve the first token and representative reboot reason. Later faults do not overwrite them or start another dump. Additional faults observed in the same stable scan may be included as secondary evidence.
- Mandatory API failure reporting is noreturn. The call site must be normal task context with IRQs enabled. The reporter publishes or loses the token CAS and spins with a compiler barrier without entering PM, idle, or WFI and without altering caller nesting.

### Minimum header and fatal diagnostics

- Immediately after recognizing a token, stop satisfying kick eligibility and print a maximum two-line Minimum Fault Header through direct low-level output without acquiring locks.
- The first line contains reason, raw token, CPU, Health Time, global tick, accumulated sleep, and last-kick time.
- The second line contains source slot, lifecycle generation, slot state, PID, app ID, sanitized fixed task name, timeout, checkpoint, and deadline. Print unavailable fields as `NA` and replace non-printable name bytes.
- Use a fixed-size integer formatter only. Do not allocate, traverse TCBs, or invoke dynamic logging before the header is complete.
- After the header, persist reason 62 for timeout, 63 for unexpected exit, 64 for self-check, or 65 for mandatory API failure. Do not let later panic handling overwrite it. Tick loss remains existing reason 54.
- Enter a critical section and use the existing SMP pause path after the persistent reason is written.
- Primary detailed diagnostics prioritize information volume and may use blocking facilities, but deliberately avoid acquiring scheduler, semaphore, allocator, or VFS locks that a paused CPU may own.
- Print the offender and token/snapshot, every CPU's registers and current task, every live PIDHASH TCB summary, per-TCB wait semaphore, held-semaphore chain, lock count, IRQ count and stack bounds, global semaphore list and holder chains when compiled, every Health Monitor source slot, and every heap node.
- Validate kernel pointer range and alignment before dereference. Print protected user semaphore addresses, but read them only when the current mapping is provably kernel-readable; never switch address spaces for diagnosis.
- Cap every scheduler, held-semaphore, and holder linked traversal independently at the maximum-task count. Do not allocate a visited set. Mark corrupt pointer, unavailable mapping/configuration, and cap exhaustion distinctly.
- Print registers and stack bounds for all live tasks, but raw stack contents only for the offender and each CPU's current task.
- After primary raw diagnostics, existing lock-taking dump routines may run best-effort, then invoke `PANIC()` once.
- Preserve the generic panic/crashdump source. On the first target its crashdump option is disabled and the watchdog cannot be stopped after start, so HW reset remains active through panic.

### Watchdog ownership and initialization

- Health Monitor owns lower-half registration, 30,000ms configuration, reset mode, irreversible start, and periodic kick. Remove the board's existing 5,000ms early initialization.
- Keep the existing user-visible watchdog device and ioctl ABI unchanged. Integration guidance forbids other users while Health Monitor is enabled, but the first port does not enforce exclusivity.
- Hide watchdog details behind an internal adapter with only initialize and kick responsibilities. Initialization hides driver registration, timeout programming, mode selection, and start. Kick is bounded, non-blocking, task/ISR-safe, and has no result.
- Initialization must be fail-atomic with respect to HW start: a negative result guarantees the watchdog did not start; success guarantees it is running and cannot be rolled back.
- Complete static/core/hook/device provisioning before invoking watchdog initialization as the last fallible step. After success, only capture the feed-origin Health Time and atomically publish ready.
- If any step before successful HW start fails, print one low-level boot diagnostic, publish sticky disabled, continue boot without the watchdog, and return device-unavailable from runtime operations. There is no runtime retry.
- If failure occurs after irreversible HW start, treat it as a Health Failure rather than disabling monitoring.
- Record feed origin immediately after adapter initialization returns. CPU0 kicks every 10,000ms of Health Time only after successful health checks. The first runtime kick is 10,000ms after that origin.
- Do not require Health Contract timeouts to be less than 30,000ms. Continue kick while all contracts remain healthy, regardless of their configured timeout length.
- Allow the existing bounded watchdog register busy-wait as the explicit normal-path exception. Do not call the semaphore-taking watchdog ioctl from CPU0 tick.
- Reject build or board bring-up when the lower half cannot represent at least the requested 30 seconds or resets earlier than requested.

### PM behavior

- The first target stops its HW watchdog counter during PM sleep, so Health Monitor does not schedule wakeups only to feed it.
- Exclude intentional PM sleep from task deadlines and 10-second feed cadence by adding the exact compensated sleep ticks to a 64-bit accumulator.
- During resume, keep tick IRQ, secondary CPUs, and ordinary task execution quiesced. Calculate slept ticks, compensate global tick, publish the same value once to Health Monitor, validate Health Time, then resume tick, CPUs, and scheduling.
- Duplicate resume publication, sleep accumulator greater than global tick, or decreasing Health Time is a self-check failure.
- Do not subtract ordinary awake-time tick loss, scheduler starvation, or IRQ masking.

### First-port integrations

- Initialize Health Monitor at the beginning of OS bring-up before paging worker and workqueue creation. All four mandatory threads start only after global state is ready or disabled.
- HPWORK uses Scoped Monitoring over a Busy Batch with 5,000ms timeout: start before the first ready callback, checkpoint after each callback, and stop before idle wait.
- LPWORK uses the same Busy Batch contract with 1,000ms timeout.
- `log_dump` begins its module-owned timeout after blocking message receive and immediately before actual compression-request work, then stops after that request completes.
- `binary_manager` begins its module-owned timeout after blocking message receive and immediately before one request dispatch, then stops after that request completes.
- Each mandatory thread owns a boot-lifetime `monitoring_available` flag. The first start returning disabled changes it to false and suppresses all later Health Monitor calls for that boot without repeated probing.
- Any other first-start error, or any nonzero result after start has succeeded, invokes the noreturn mandatory API-failure reporter.
- Outer worker batches and request loops are the only slot owners. Callback or nested request code must not call Health Monitor directly on the same task.

### Self-check, memory, and build invariants

- Maintain a global magic/inverse pair and validate monotonic global state.
- Validate nonzero token CRC, token/fatal/kick-eligibility relationships, dump/fatal relationship, sleep/global relationship, and nondecreasing Health Time.
- Validate heap size and slot indexes, valid-heap epoch equality, nondecreasing membership epoch, and local parent/child order on every touched sift path.
- Validate PIDHASH, full PID, and lifecycle generation for each armed source examined by CPU0.
- Any violation enters self-check first-fault handling. Invalid nonzero token uses a CPU0-local emergency record instead of trusting encoded identity.
- Use compile-time assertions for the 12-byte public status, 64-byte aligned source slot, 16-byte heap node, capacity limit, and lock-free 32/64-bit atomics.
- Do not add runtime full-heap/source self-audit. Header magic and cross-variable relationships plus touched-path validation are the accepted corruption coverage.

## Testing Decisions

- The primary functional test seam is the complete Health Monitor core exercised through its task operations and CPU0 process-tick entry with injected Health Time and a fake watchdog adapter. This is the highest stable seam and keeps state machine, source publication, deadline selection, fault latching, and feed cadence in one observable test boundary.
- Tests should assert externally visible behavior: API errno, query result, whether a fatal token/reboot reason is produced, whether a watchdog kick occurs, and whether reset/dump progression begins. They should not couple expectations to heap positions, private slot field offsets, or a specific sift implementation.
- Use deterministic fake time to cover start deadline, ceiling conversion, exact-deadline keepalive/stop ordering, long timeout, PM compensation, no grace tick, maximum timeout, deadline overflow, and remaining-time saturation.
- Exercise repeated keepalive through observable no-failure behavior until a stale root becomes due, then verify repair preserves the extended contract. Force more than 16 due stale candidates and verify the same tick still discovers any truly expired source.
- Exercise membership churn with concurrent start, stop, restart, and managed teardown. Verify no false expiration, missed expiration, unbounded retry, or stale PID/generation acceptance.
- Exercise inactive, preparing, armed, terminal, disabled, and fatal behavior through public results and final fault outcomes. Include nested start and release during preparing.
- Exercise lifecycle behavior with normal return, exit, pthread cancellation, task deletion, failed task deletion, managed update, managed unload, managed fault recovery, and direct deletion that bypasses managed teardown.
- Run SMP stress where CPU1 publishes checkpoint, stop, start, unexpected release, and API failure while CPU0 scans, repairs, rebuilds, and approaches a kick boundary.
- Inject concurrent exits on different CPUs and sequential exits on the same CPU. Verify the winning token selects an immutable matching exit snapshot and later releases cannot overwrite it.
- Test first-fault-wins across every pair of timeout, exit, self-check, and API failure. Verify later faults do not replace token or persistent reason.
- Corrupt test-build magic, token CRC, epoch relation, time relation, heap bound, touched heap order, and PID identity one at a time. Assert self-check outcome without asserting private recovery mechanics.
- Test the VFS adapter through actual open and ioctl behavior: scalar start, zero-only arguments, writable query, descriptor sharing between threads, close without stop, unsupported operations, and `-ENOTTY`, `-EFAULT`, `-EINVAL`, `-EAGAIN`, `-ENODEV`, and `-ESHUTDOWN` distinctions.
- Test mandatory wrappers at their external integration boundary: first disabled result makes the thread skip later calls; every other error is noreturn and produces API-failure classification.
- Test HPWORK and LPWORK with empty idle cycles, one callback, multiple callbacks, callback longer than timeout, exact-deadline completion, and callback attempts at nested registration.
- Test `log_dump` and `binary_manager` with idle receive, successful request, every normal error/early-exit branch, request timeout, and stop failure. Confirm every normal processing path completes its scoped contract.
- Test watchdog initialization with failure before start, successful irreversible start, no retry after disabled state, first kick ten seconds after adapter return, continued kicks with no active registrations, and no kick after a first-fault token.
- Test PM with multiple sleep/resume cycles and assert that task deadline and feed interval do not advance during sleep. Instrument the first target to prove no secondary CPU/task executes between global-tick compensation and sleep-accumulator publication.
- Validate Minimum Fault Header as fixed bounded output before persistent reason and before any lock/pause attempt. Test source-backed and source-less faults and sanitized task names.
- Fault-inject detailed diagnostics with corrupt lists, cycles, invalid kernel pointers, inaccessible protected pointers, missing compile-time semaphore capabilities, and traversal cap exhaustion. Verify later sections still run when safe.
- Use existing OS test patterns for watchdog timing, workqueue behavior, semaphore/priority inheritance, cancellation, pthread exit, signal handling, and SMP calls as prior art for harness style and cleanup.
- On rtl8730e hardware, require worst observed keepalive latency at or below 5us, healthy CPU0 tick at or below 10us, and 16 repairs plus a 256-source scan and Floyd heapify at or below 250us.
- Inspect release disassembly to ensure runtime paths do not call semaphore, allocation, VFS, or `libatomic` helpers.
- Measure actual watchdog timeout and clock tolerance. A 30,000ms request must never reset earlier; verify 10,000ms feed cadence and pause/resume behavior.
- Perform board fault injection for timeout, unexpected exit, self-check, mandatory API failure, tick ISR loss, initialization disable, stalled CPU pause, stalled detailed dump, and panic. Verify persistent reasons 62 through 65 and fallback reason 54.
- Verify linker-map and compile-time size gates for the public ABI, source array, heap, per-CPU snapshots, and total static-memory target.

## Out of Scope

- Monitoring idle tasks.
- First-port integration of uwork, LWIP TCP/IP, NDP health checking, binary-loader children, network manager, Wi-Fi, BLE, media/audio, or application threads.
- A per-CPU heartbeat or global CPU-health contract independent of registered tasks.
- Global sched-lock duration monitoring.
- Treating semaphore wait, lock ownership, or CPU occupation as an independent reboot trigger.
- Nested Health Contracts, multiple deadlines per task, delegated keepalive, or cross-task registration/control.
- Automatic timeout defaults, timeout Kconfig, or central policy for `log_dump`, `binary_manager`, or future modules.
- Authorization, capability tokens, per-fd ownership, or prevention of a trusted caller deliberately choosing a short timeout.
- Enforced exclusive access to the existing watchdog device by blocking other callers.
- Removing or changing the existing watchdog device ABI.
- Changing the generic panic crashdump watchdog-disable source.
- Treating update, unload, or fault recovery initiated through managed binary teardown as a Health Failure.
- Detecting a task that disappears outside an active Scoped Monitoring interval.
- Switching address spaces during fatal diagnosis to inspect protected user memory.
- Dumping the full raw stack of every task.
- Periodic full heap-to-source integrity scans during healthy operation.
- Runtime reinitialization after sticky disabled state.
- Software diagnosis when the CPU0 tick ISR itself is no longer executing.
- Supporting another board or implementing the future 32-bit checkpoint fallback in the first port.
- Renaming or rewriting historical `sdog` architecture, flow, and review evidence.

## Further Notes

- The authoritative accepted design is [health_monitor_design.md](/home/user/Dev/Public/TizenRT/health_monitor_design.md).
- The previous [sdog_design.md](/home/user/Dev/Public/TizenRT/sdog_design.md) is historical and contains a superseded notice.
- Domain vocabulary is defined in [CONTEXT.md](/home/user/Dev/Public/TizenRT/main/CONTEXT.md).
- Accepted architectural decisions are recorded under [docs/adr](/home/user/Dev/Public/TizenRT/main/docs/adr).
- The accepted lazy-heap choice was informed by the [deadline scheduler prototype](/home/user/Dev/Public/TizenRT/main/.scratch/health-monitor/deadline-scheduler-prototype.html). Prototype operation counts are design evidence, not target timing measurements.
- The primary core/fake-time/fake-watchdog test seam and the thin VFS, lifecycle, PM, tick, and board integration gates were already accepted during the design grill; no additional user interview is required.
