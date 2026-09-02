# TizenRT

TizenRT의 커널 및 시스템 기능에서 사용하는 프로젝트 고유 용어를 정의한다.

## Health Monitoring

**Health Monitor**:
등록한 태스크가 정해진 시간 안에 생존 신호를 갱신해야 한다는 계약과 그 위반을 관찰하는 커널 기능.
_Avoid_: sdog, task_monitor

**Health Contract**:
등록 태스크가 제한시간 안에 생존 신호를 갱신하고 정상 종료 전에 감시를 명시적으로 해제한다는 약속.

**Liveness Checkpoint**:
등록 태스크의 개발자가 의미 있는 진행이 완료됐다고 선언한 코드 지점. 단순히 CPU를 배정받거나 context restore가 일어난 것은 Liveness Checkpoint가 아니다.

**Health Failure**:
Health Contract가 깨지거나, mandatory integration이 Health Monitor API 계약을 지키지 못하거나, 그 위반을 감시하는 실행 기반이 멈춘 상태. 관측된 대기, lock 보유, CPU 독점은 장애 원인을 설명하는 증거이지 그 자체로 Health Failure를 뜻하지 않는다.

**Fatal Diagnostic Phase**:
Health Monitor가 최초 Health Failure를 확정하고 HW watchdog feed를 중단한 뒤, reset 전에 최대한 많은 장애 정보를 출력하는 단계. 정상 감시 경로의 non-blocking 제약은 더 이상 적용하지 않으며, blocking 진단이 멈추면 HW watchdog이 reset한다.

**Minimum Fault Header**:
Fatal Diagnostic Phase에서 lock이나 CPU pause를 시도하기 전에 direct low-level UART로 출력하는 최대 두 줄의 최소 장애 기록. raw token과 시간 상태, offending registration을 포함하고 출력 직후 persistent reboot reason을 기록한다.

**Health Time**:
Health Monitor가 deadline과 HW watchdog feed cadence에 사용하는 64-bit monotonic awake-time. global system tick에서 누적 PM sleep tick을 제외하므로 정상 PM sleep 중에는 Health Contract 시간이 흐르지 않는다.

**Managed Binary Teardown**:
Binary Manager가 update, unload 또는 fault recovery를 위해 loadable binary의 태스크를 의도적으로 제거하는 수명주기 동작. 이 경로의 태스크 소멸은 Health Failure가 아니다.
_Avoid_: Binary Unload (fault recovery까지 함께 뜻할 때)

**Lifetime Monitoring**:
태스크가 시작한 뒤 cooperative exit 직전까지 하나의 Health Contract를 유지하는 감시 범위. 정상 idle 상태도 태스크가 주기적으로 Liveness Checkpoint를 통과해야 한다.

**Scoped Monitoring**:
메시지 처리나 callback 실행처럼 경계가 있는 작업 동안만 Health Contract를 유지하는 감시 범위. 감시 범위 밖에서 태스크가 사라지는 것은 검출하지 않는다.

**Busy Batch**:
workqueue에서 첫 ready callback을 실행하기 직전부터 연속된 ready callback을 모두 처리하고 idle wait로 들어가기 직전까지의 Scoped Monitoring 구간. callback 완료는 Liveness Checkpoint이며 idle 상태는 감시하지 않는다.

**Lazy Deadline Heap**:
Health Monitor의 source slot에서 읽은 deadline snapshot을 정렬하는 CPU0 전용 min-heap. 감시 대상 태스크는 heap을 직접 수정하지 않고 checkpoint만 publish하며, CPU0가 root에 도달했을 때 연장된 deadline을 lazy repair한다.
_Avoid_: writer-mutated watchdog queue, task-side deadline heap
