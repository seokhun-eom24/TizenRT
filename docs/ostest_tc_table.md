# ostest TC 목록

TC 열은 공개 진입 함수와 그 함수 안에서 분기되는 세부 케이스를 함께 표시한다. 성공 시 검증되는 동작을 적었다. 결과는 2026-08-18 제공 로그 기준이며 로그에 없는 케이스는 `미실행`으로 표시한다.

`기존 kernel_sample 대응`은 `apps/examples/kernel_sample`에 대응 TC가 있는지 표시한다. `있음`은 동일 기능의 TC, `부분`은 같은 기능 영역이지만 세부 케이스가 다른 TC, `없음`은 대응 TC 없음, `N/A`는 실행 TC가 아닌 공통·빌드 파일을 뜻한다. `wqueue.c`는 기존 `workqueue.c`와 비교했다.

실행 요약: `ostest_main: Exiting with status 0`으로 전체 실행은 정상 종료했다. 세부 케이스 기준 `PASS` 48개, `SKIPPED` 3개(Test 2·6·7), `미실행` 125개다.

원인 표기에서 `CONFIG_TESTING_OSTEST_*`는 테스트 선택만 꺼진 경우이고, 그 외 CONFIG는 해당 기능 또는 Kconfig 의존 조건이 꺼진 경우다. `CONFIG_BUILD_FLAT=n`은 현재 protected build에서 flat 전용 TC를 실행할 수 없다는 뜻이다.

| File | TC / 케이스 | 기존 kernel_sample 대응 | 성공 시 검증 내용 | 결과 | 원인 |
| --- | --- | --- | --- | --- | --- |
| Kconfig | 빌드 선택 | N/A | 선택한 ostest 파일만 빌드 대상에 포함되는지 검증 | N/A |  |
| Make.defs | 소스 등록 | N/A | 선택된 소스가 빌드 규칙에 등록되는지 검증 | N/A |  |
| aio.c | `aio_test` / case 1: polling | 있음 | 비동기 I/O 완료를 상태 polling으로 감지 | 미실행 | `CONFIG_FS_AIO` 미설정으로 의존 기능 비활성 |
| aio.c | `aio_test` / case 2: `LIO_WAIT` | 있음 | `lio_listio`의 대기 완료 경로 | 미실행 | `CONFIG_FS_AIO` 미설정으로 의존 기능 비활성 |
| aio.c | `aio_test` / case 3: `aio_suspend` | 있음 | 지정 AIO 요청의 suspend 대기 및 완료 | 미실행 | `CONFIG_FS_AIO` 미설정으로 의존 기능 비활성 |
| aio.c | `aio_test` / case 4: per-request signal | 있음 | 요청별 signal 통지와 완료 상태 | 미실행 | `CONFIG_FS_AIO` 미설정으로 의존 기능 비활성 |
| aio.c | `aio_test` / case 5: list complete signal | 있음 | `lio_listio` 완료 signal과 list I/O 완료 상태 | 미실행 | `CONFIG_FS_AIO` 미설정으로 의존 기능 비활성 |
| aio.c | `aio_test` / case 6: cancel by control block | 있음 | AIO control block을 이용한 취소 | 미실행 | `CONFIG_FS_AIO` 미설정으로 의존 기능 비활성 |
| aio.c | `aio_test` / case 7: cancel by file descriptor | 있음 | 파일 디스크립터 기반 AIO 취소 | 미실행 | `CONFIG_FS_AIO` 미설정으로 의존 기능 비활성 |
| barrier.c | `barrier_test` / 초기화·속성 | 있음 | barrier attribute와 barrier 생성 | PASS |  |
| barrier.c | `barrier_test` / 다중 스레드 대기 | 있음 | 모든 스레드가 barrier에서 동기화되고 한 스레드만 serial 반환 | PASS |  |
| barrier.c | `barrier_test` / 정리 | 있음 | 스레드 join과 barrier destroy | PASS |  |
| cancel.c | `cancel_test` / test 1a: normal cancellation | 있음 | cancellation point에서 `PTHREAD_CANCELED` 반환 | PASS |  |
| cancel.c | `cancel_test` / test 2: asynchronous cancellation | 있음 | asynchronous cancel 요청이 스레드를 종료 | SKIPPED | `CONFIG_CANCELLATION_POINTS` 미설정으로 `#else` skip |
| cancel.c | `cancel_test` / test 3: detached thread | 있음 | detached 스레드 cancel 후 join이 `ESRCH` | PASS |  |
| cancel.c | `cancel_test` / test 5: non-cancelable thread | 있음 | cancel 불가 상태에서 signal 후 정상적으로 cancellation point 처리 | PASS |  |
| cancel.c | `cancel_test` / test 6: message-queue wait | 있음 | `mq_receive` 대기 중 cancel과 정리 | SKIPPED | `CONFIG_CANCELLATION_POINTS` 미설정으로 `#else` skip |
| cancel.c | `cancel_test` / test 7: signal wait | 있음 | signal 대기 중 cancel과 `PTHREAD_CANCELED` | SKIPPED | `CONFIG_CANCELLATION_POINTS` 미설정으로 `#else` skip |
| cond.c | `cond_test` / waiter wait | 있음 | 조건이 없을 때 waiter가 mutex를 보유하지 않고 대기 | 미실행 | `CONFIG_PRIORITY_INHERITANCE=y`로 Kconfig 의존 조건 불충족 |
| cond.c | `cond_test` / signaler wakeup | 있음 | signal 후 waiter가 조건을 확인하고 진행 | 미실행 | `CONFIG_PRIORITY_INHERITANCE=y`로 Kconfig 의존 조건 불충족 |
| cond.c | `cond_test` / cancellation·통계 | 있음 | waiter cancel cleanup과 loop/error 카운터 | 미실행 | `CONFIG_PRIORITY_INHERITANCE=y`로 Kconfig 의존 조건 불충족 |
| dev_null.c | `dev_null_test` / read·write | 있음 | `/dev/null` open, read 0바이트, write 요청 길이, close | PASS |  |
| fork.c | `fork_test` / data 영역 분리 | 없음 | child의 data 변경이 parent에 보이지 않음 | 미실행 | `CONFIG_ARCH_HAVE_FORK` 미지원/미설정 |
| fork.c | `fork_test` / heap·stack 분리 | 없음 | child와 parent의 heap·stack 변경이 서로 독립 | 미실행 | `CONFIG_ARCH_HAVE_FORK` 미지원/미설정 |
| fork.c | `fork_test` / 종료·waitpid | 없음 | child 종료 상태를 parent가 회수 | 미실행 | `CONFIG_ARCH_HAVE_FORK` 미지원/미설정 |
| fpu.c | `fpu_test` / FPU task 1 | 없음 | 첫 번째 태스크의 FPU 레지스터 저장·복원 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 FPU TC 의존 조건 불충족 |
| fpu.c | `fpu_test` / FPU task 2 | 없음 | 두 번째 태스크의 FPU 레지스터 저장·복원 및 태스크 간 오염 방지 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 FPU TC 의존 조건 불충족 |
| getopt.c | `getopt_test` / `getopt` simple | 없음 | short option과 인자 순서·값 파싱 | 미실행 | `CONFIG_TESTING_OSTEST_GETOPT` 미설정 |
| getopt.c | `getopt_test` / invalid argument | 없음 | 잘못된 option 입력의 오류 반환 | 미실행 | `CONFIG_TESTING_OSTEST_GETOPT` 미설정 |
| getopt.c | `getopt_test` / missing optional argument | 없음 | optional argument 누락 처리 | 미실행 | `CONFIG_TESTING_OSTEST_GETOPT` 미설정 |
| getopt.c | `getopt_long` / simple | 없음 | long option 기본 파싱 | 미실행 | `CONFIG_TESTING_OSTEST_GETOPT` 미설정 |
| getopt.c | `getopt_long` / no short options | 없음 | short option 없이 long option 파싱 | 미실행 | `CONFIG_TESTING_OSTEST_GETOPT` 미설정 |
| getopt.c | `getopt_long` / `--option=argument` | 없음 | `=` 형식 long argument 파싱 | 미실행 | `CONFIG_TESTING_OSTEST_GETOPT` 미설정 |
| getopt.c | `getopt_long` / invalid long option | 없음 | 존재하지 않는 long option 오류 | 미실행 | `CONFIG_TESTING_OSTEST_GETOPT` 미설정 |
| getopt.c | `getopt_long` / mixed long·short | 없음 | long·short option 혼합 파싱 | 미실행 | `CONFIG_TESTING_OSTEST_GETOPT` 미설정 |
| getopt.c | `getopt_long` / invalid short option | 없음 | 존재하지 않는 short option 오류 | 미실행 | `CONFIG_TESTING_OSTEST_GETOPT` 미설정 |
| getopt.c | `getopt_long` / missing optional arguments | 없음 | long option optional argument 누락 처리 | 미실행 | `CONFIG_TESTING_OSTEST_GETOPT` 미설정 |
| getopt.c | `getopt_long_only` / mixed options | 없음 | long-only 모드의 혼합 option 처리 | 미실행 | `CONFIG_TESTING_OSTEST_GETOPT` 미설정 |
| getopt.c | `getopt_long_only` / single hyphen | 없음 | 단일 하이픈 long option 처리 | 미실행 | `CONFIG_TESTING_OSTEST_GETOPT` 미설정 |
| hrtimer.c | `hrtimer_test` / one-shot delays | 없음 | 0부터 10 ms까지 one-shot 만료 시각 | 미실행 | `CONFIG_BUILD_FLAT=n`, `CONFIG_HRTIMER` 미설정 |
| hrtimer.c | `hrtimer_test` / maximum delay | 없음 | `UINT64_MAX` 지연 설정과 남은 시간 | 미실행 | `CONFIG_BUILD_FLAT=n`, `CONFIG_HRTIMER` 미설정 |
| hrtimer.c | `hrtimer_test` / random delay | 없음 | random 지연의 만료 시각 | 미실행 | `CONFIG_BUILD_FLAT=n`, `CONFIG_HRTIMER` 미설정 |
| hrtimer.c | `hrtimer_test` / random cancel | 없음 | random 지연 timer 취소와 callback 미실행 조건 | 미실행 | `CONFIG_BUILD_FLAT=n`, `CONFIG_HRTIMER` 미설정 |
| hrtimer.c | `hrtimer_test` / periodic | 없음 | 주기 timer callback 횟수와 주기 오차 | 미실행 | `CONFIG_BUILD_FLAT=n`, `CONFIG_HRTIMER` 미설정 |
| hrtimer.c | `hrtimer_test` / cancel synchronization | 없음 | callback 실행 중 cancel의 동기화 | 미실행 | `CONFIG_BUILD_FLAT=n`, `CONFIG_HRTIMER` 미설정 |
| hrtimer.c | `hrtimer_test` / periodic cancel | 없음 | periodic timer cancel 후 callback 중복 방지 | 미실행 | `CONFIG_BUILD_FLAT=n`, `CONFIG_HRTIMER` 미설정 |
| libc_memmem.c | `memmem_test` / `hel` prefix | 없음 | haystack 시작 주소 반환 | 미실행 | `CONFIG_TESTING_OSTEST_LIBC_MEMMEM` 미설정 |
| libc_memmem.c | `memmem_test` / `lo` middle | 없음 | haystack 중간 위치 반환 | 미실행 | `CONFIG_TESTING_OSTEST_LIBC_MEMMEM` 미설정 |
| libc_memmem.c | `memmem_test` / exact match | 없음 | haystack과 동일한 needle 위치 반환 | 미실행 | `CONFIG_TESTING_OSTEST_LIBC_MEMMEM` 미설정 |
| libc_memmem.c | `memmem_test` / embedded NUL | 없음 | 지정 길이 안의 NUL 포함 비교 | 미실행 | `CONFIG_TESTING_OSTEST_LIBC_MEMMEM` 미설정 |
| libc_memmem.c | `memmem_test` / out-of-range match | 없음 | haystack 범위 밖 byte를 검색하지 않음 | 미실행 | `CONFIG_TESTING_OSTEST_LIBC_MEMMEM` 미설정 |
| libc_memmem.c | `memmem_test` / oversized needle | 없음 | haystack보다 긴 needle에 `NULL` 반환 | 미실행 | `CONFIG_TESTING_OSTEST_LIBC_MEMMEM` 미설정 |
| libc_memmem.c | `memmem_test` / empty needle | 없음 | 길이 0 needle에 haystack 시작 주소 반환 | 미실행 | `CONFIG_TESTING_OSTEST_LIBC_MEMMEM` 미설정 |
| mqueue.c | `mqueue_test` / sender | 있음 | sender가 메시지를 queue에 넣고 오류 없이 종료 | PASS |  |
| mqueue.c | `mqueue_test` / receiver | 있음 | receiver가 메시지를 받고 queue를 닫음 | PASS |  |
| mqueue.c | `mqueue_test` / signal·cancel cleanup | 있음 | receiver signal/cancel 후 join 결과와 mqd 정리 | PASS |  |
| multiuser.c | `multiuser_test` / effective UID·GID | 없음 | 유효 UID/GID 변경과 복원 | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| multiuser.c | `multiuser_test` / supplementary groups | 없음 | `setgroups`·`getgroups` 보조 그룹 목록 | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| multiuser.c | `multiuser_test` / setres drop ordering | 없음 | real/effective/saved ID drop 순서와 권한 제한 | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| multiuser.c | `multiuser_test` / getres·setre | 없음 | `getresuid/gid`, `setreuid/gid` 결과 | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| multiuser.c | `multiuser_test` / full setreuid child | 없음 | child에서 `setreuid(1000,1000)` 전체 대입 | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| multiuser.c | `multiuser_test` / saved ID child | 없음 | saved set-UID/GID 보존·복구 의미 | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| multiuser.c | `multiuser_test` / pseudoFS permissions | 없음 | pseudoFS의 chmod/chown/open 접근 권한 | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| multiuser.c | `multiuser_test` / tmpfs permissions | 없음 | tmpfs open 시 권한 검사 | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| multiuser.c | `multiuser_test` / message queue permissions | 없음 | message queue owner·mode에 따른 접근 제한 | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| multiuser.c | `multiuser_test` / named semaphore permissions | 없음 | named semaphore owner·mode에 따른 접근 제한 | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| multiuser.c | `multiuser_test` / shared memory permissions | 없음 | shared memory owner·mode에 따른 접근 제한 | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| multiuser.c | `multiuser_test` / FIFO permissions | 없음 | FIFO owner·mode에 따른 접근 제한 | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| multiuser.c | `multiuser_test` / passwd lookup | 없음 | credential drop 후 passwd 조회 | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| multiuser.c | `multiuser_test` / sudo exec | 없음 | hard credential drop 후 setuid sudo exec | 미실행 | `CONFIG_SCHED_USER_IDENTITY`, `CONFIG_LIBC_PASSWD_FILE` 미설정 |
| mutex.c | `mutex_test` / 기본 mutex | 있음 | 두 스레드의 lock/unlock 경쟁과 error count | PASS |  |
| mutex.c | `mutex_test` / moved mutex | 있음 | 스레드 간 이동된 mutex의 lock/unlock | PASS |  |
| nsem.c | `nsem_test` / semaphore 1 | 있음 | named semaphore create, peer post, wait, close, unlink | 미실행 | `CONFIG_FS_NAMED_SEMAPHORES` 미설정 |
| nsem.c | `nsem_test` / semaphore 2 | 있음 | peer가 post한 semaphore reopen, wait, close, unlink | 미실행 | `CONFIG_FS_NAMED_SEMAPHORES` 미설정 |
| nxevent.c | `nxevent_test` / task-local basic | 없음 | init, wait, trywait, tickwait의 기본 event mask | 미실행 | `CONFIG_SCHED_EVENTS` 및 `CONFIG_BUILD_FLAT` 미설정 |
| nxevent.c | `nxevent_test` / task notify rule 0 | 없음 | rule 0의 post·wait mask 조합 | 미실행 | `CONFIG_SCHED_EVENTS` 및 `CONFIG_BUILD_FLAT` 미설정 |
| nxevent.c | `nxevent_test` / task notify rule 1 | 없음 | rule 1의 post·wait mask 조합 | 미실행 | `CONFIG_SCHED_EVENTS` 및 `CONFIG_BUILD_FLAT` 미설정 |
| nxevent.c | `nxevent_test` / multi-task notify | 없음 | 두 waiter가 서로 다른 mask를 소비 | 미실행 | `CONFIG_SCHED_EVENTS` 및 `CONFIG_BUILD_FLAT` 미설정 |
| nxevent.c | `nxevent_test` / clear·getmask | 없음 | event clear와 현재 mask 조회 | 미실행 | `CONFIG_SCHED_EVENTS` 및 `CONFIG_BUILD_FLAT` 미설정 |
| ostest.h | 공통 선언 | N/A | 각 TC가 공통 prototype·assert/helper를 공유 | N/A |  |
| ostest_main.c | `ostest_main` / environment | 있음 | `putenv`와 `setenv` overwrite 옵션 의미 | PASS |  |
| ostest_main.c | `user_main` / argc·argv | 있음 | user task 인자 개수와 문자열 검증 | PASS |  |
| ostest_main.c | `user_main` / libc·stdio | 있음 | 활성화된 libc/stdio test 호출과 결과 집계 | PASS |  |
| ostest_main.c | `user_main` / OS test dispatch | 있음 | Kconfig로 활성화한 각 ostest TC 호출 | PASS |  |
| ostest_main.c | `stdio_test` / stdout·stderr | 있음 | `printf`와 `fprintf(stderr)` 출력 경로 | PASS |  |
| perf_gettime.c | `perf_gettime_test` / initial value | 없음 | performance time counter 초기값 조회 | 미실행 | `CONFIG_ARCH_HAVE_PERF_EVENTS`, `CONFIG_ARCH_PERF_EVENTS`, `CONFIG_BUILD_FLAT` 미설정 |
| perf_gettime.c | `perf_gettime_test` / monotonicity | 없음 | 반복 호출 시간이 감소하지 않음 | 미실행 | `CONFIG_ARCH_HAVE_PERF_EVENTS`, `CONFIG_ARCH_PERF_EVENTS`, `CONFIG_BUILD_FLAT` 미설정 |
| perf_gettime.c | `perf_gettime_test` / interval statistics | 없음 | 호출 간격 통계가 유효 범위 | 미실행 | `CONFIG_ARCH_HAVE_PERF_EVENTS`, `CONFIG_ARCH_PERF_EVENTS`, `CONFIG_BUILD_FLAT` 미설정 |
| perf_gettime.c | `perf_gettime_test` / frequency | 없음 | performance counter 주파수 조회 | 미실행 | `CONFIG_ARCH_HAVE_PERF_EVENTS`, `CONFIG_ARCH_PERF_EVENTS`, `CONFIG_BUILD_FLAT` 미설정 |
| perf_gettime.c | `perf_gettime_test` / rapid calls | 없음 | 연속 호출에서도 시간이 역행하지 않음 | 미실행 | `CONFIG_ARCH_HAVE_PERF_EVENTS`, `CONFIG_ARCH_PERF_EVENTS`, `CONFIG_BUILD_FLAT` 미설정 |
| posixtimer.c | `timer_test` / signal expiration | 있음 | POSIX timer 생성·시작·signal 대기·삭제와 signo, value, `SI_TIMER` | PASS |  |
| prioinherit.c | `priority_inheritance` / priority boost | 있음 | low owner가 high waiter에 의해 priority boost | 미실행 | `CONFIG_TESTING_OSTEST_PRIOINHERIT` 미설정 |
| prioinherit.c | `priority_inheritance` / restoration | 없음 | semaphore 해제 후 원래 priority 복원 | 미실행 | `CONFIG_TESTING_OSTEST_PRIOINHERIT` 미설정 |
| pthread_cleanup.c | `pthread_cleanup_test` / cancel cleanup | 없음 | cancel cleanup handler가 mutex를 unlock하고 join이 `PTHREAD_CANCELED` | 미실행 | `CONFIG_PTHREAD_CLEANUP` 미설정 |
| pthread_exit.c | `pthread_exit_test` / child thread | 없음 | child thread 생성·실행 | PASS |  |
| pthread_exit.c | `pthread_exit_test` / main thread exit | 없음 | `pthread_exit` 이후 함수가 계속 실행되지 않고 join 완료 | PASS |  |
| pthread_rwlock.c | `pthread_rwlock_test` / try lock states | 없음 | write/read trylock의 허용·거부 상태 | PASS |  |
| pthread_rwlock.c | `pthread_rwlock_test` / two-thread order | 없음 | 두 스레드의 read/write lock 순서와 unlock | PASS |  |
| pthread_rwlock.c | `pthread_rwlock_test` / timed write/read | 없음 | held lock의 timeout과 해제 후 timed read 성공 | PASS |  |
| pthread_rwlock_cancel.c | `pthread_rwlock_cancel_test` / canceled waiters | 없음 | read/write lock 대기 중 cancel 후 lock 상태 보존 | PASS |  |
| restart.c | `restart_test` / argv·environment | 있음 | restart된 task의 argc/argv와 환경변수 보존 | PASS |  |
| restart.c | `restart_test` / task restart | 있음 | `task_restart` 후 task가 재실행되고 semaphore 정리 | PASS |  |
| rmutex.c | `recursive_mutex_test` / recursive depth | 있음 | 같은 스레드의 중첩 lock과 trylock/unlock 횟수 | PASS |  |
| rmutex.c | `recursive_mutex_test` / cross-thread blocking | 있음 | 다른 스레드가 recursive mutex 해제까지 대기 | PASS |  |
| robust.c | `robust_test` / owner death | 없음 | mutex owner 종료 후 다음 lock이 `EOWNERDEAD` | 미실행 | `CONFIG_PTHREAD_MUTEX_UNSAFE=y`로 robust mutex 의존 조건 불충족 |
| robust.c | `robust_test` / consistent | 없음 | `pthread_mutex_consistent` 후 재잠금 가능 | 미실행 | `CONFIG_PTHREAD_MUTEX_UNSAFE=y`로 robust mutex 의존 조건 불충족 |
| robust.c | `robust_test` / cleanup | 없음 | join과 unlock/destroy가 오류 없이 완료 | 미실행 | `CONFIG_PTHREAD_MUTEX_UNSAFE=y`로 robust mutex 의존 조건 불충족 |
| roundrobin.c | `rr_test` / scheduling policy | 있음 | 두 thread가 `SCHED_RR`로 생성 | PASS |  |
| roundrobin.c | `rr_test` / alternation | 있음 | prime 계산 중 실행 순서가 교대되어 round-robin 동작 | PASS |  |
| sched_thread_local.c | `sched_thread_local_test` / initial TLS | 없음 | 각 thread의 TLS 초기값 | 미실행 | `CONFIG_SCHED_THREAD_LOCAL` 미설정 |
| sched_thread_local.c | `sched_thread_local_test` / isolation | 없음 | thread별 TLS 변경값이 서로 섞이지 않음 | 미실행 | `CONFIG_SCHED_THREAD_LOCAL` 미설정 |
| schedlock.c | `sched_lock_test` / preemption control | 없음 | scheduler lock 동안 preemption이 없고 unlock 후 재개 | PASS |  |
| sem.c | `sem_test` / waiter blocking | 있음 | 초기값 0 semaphore에서 두 waiter가 block | PASS |  |
| sem.c | `sem_test` / poster wakeup | 있음 | post가 waiter를 깨우고 semaphore count가 전이 | PASS |  |
| sem.c | `sem_test` / join·cleanup | 있음 | 모든 waiter/poster join과 semaphore 정리 | PASS |  |
| semtimed.c | `semtimed_test` / timeout | 있음 | post 없는 `sem_timedwait`가 `ETIMEDOUT` | PASS |  |
| semtimed.c | `semtimed_test` / posted before timeout | 있음 | timeout 전 post 시 `sem_timedwait` 성공 | PASS |  |
| setjmp.c | `setjmp_test` / longjmp value 123 | 없음 | `longjmp(...,123)` 후 setjmp 반환값 123 | 미실행 | `CONFIG_ARCH_SETJMP_H` 미설정 |
| setjmp.c | `setjmp_test` / longjmp value 0 | 없음 | `longjmp(...,0)`이 setjmp 반환값 1로 변환 | 미실행 | `CONFIG_ARCH_SETJMP_H` 미설정 |
| setvbuf.c | `setvbuf_test` / no buffering | 없음 | `_IONBF` 설정 | 미실행 | `CONFIG_TESTING_OSTEST_SETVBUF` 미설정 |
| setvbuf.c | `setvbuf_test` / default full buffering | 없음 | 기본 full buffering 설정 | 미실행 | `CONFIG_TESTING_OSTEST_SETVBUF` 미설정 |
| setvbuf.c | `setvbuf_test` / full 64-byte | 없음 | 지정 크기 full buffer 설정 | 미실행 | `CONFIG_TESTING_OSTEST_SETVBUF` 미설정 |
| setvbuf.c | `setvbuf_test` / caller buffer | 없음 | 사전 할당 buffer 사용 | 미실행 | `CONFIG_TESTING_OSTEST_SETVBUF` 미설정 |
| setvbuf.c | `setvbuf_test` / line buffering | 없음 | `_IOLBF` 설정 | 미실행 | `CONFIG_TESTING_OSTEST_SETVBUF` 미설정 |
| setvbuf.c | `setvbuf_test` / second full-buffer path | 없음 | 추가 full-buffer 호출 경로 | 미실행 | `CONFIG_TESTING_OSTEST_SETVBUF` 미설정 |
| sigev_thread.c | `sigev_thread_test` / timer callback | 없음 | `SIGEV_THREAD` callback 실행과 전달 value 검증 | 미실행 | `CONFIG_SIG_EVTHREAD` 미설정 |
| sighand.c | `sighand_test` / wakeup signal | 있음 | signal handler가 `sem_wait`를 깨우고 `EINTR` 반환 | 미실행 | `CONFIG_TESTING_OSTEST_SIGHAND` 미설정 |
| sighand.c | `sighand_test` / SIGCHLD | 있음 | child 종료 SIGCHLD handler 실행과 waiter 종료 | 미실행 | `CONFIG_TESTING_OSTEST_SIGHAND` 미설정 |
| sighelper.c | `sigset_isequal` / equal sets | N/A | 같은 sigset을 동일하다고 판정 | N/A |  |
| sighelper.c | `sigset_isequal` / different sets | N/A | 다른 sigset을 다르다고 판정 | N/A |  |
| signest.c | `signest_test` / simple case | 없음 | signal 전달·처리 수와 odd/even 집계 | 미실행 | `CONFIG_TESTING_OSTEST_SIGNEST` 미설정 |
| signest.c | `signest_test` / task locking | 없음 | task lock 중 signal 처리 집계와 nesting | 미실행 | `CONFIG_TESTING_OSTEST_SIGNEST` 미설정 |
| signest.c | `signest_test` / interfering thread | 없음 | 다른 task가 있어도 올바른 TID로 signal 전달, nested signal 없음 | 미실행 | `CONFIG_TESTING_OSTEST_SIGNEST` 미설정 |
| sigprocmask.c | `sigprocmask_test` / hold mask | 없음 | `sigaddset`·`sighold` 후 mask가 기대값 | PASS |  |
| sigprocmask.c | `sigprocmask_test` / fill and release | 없음 | `sigfillset`·`sigdelset`·`sigrelse` 결과 | PASS |  |
| sigprocmask.c | `sigprocmask_test` / restore | 없음 | 저장한 원래 signal mask 복원 | PASS |  |
| smp_call.c | `smp_call_test` / per-CPU nowait | 없음 | 각 CPU 대상 non-blocking call | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| smp_call.c | `smp_call_test` / per-CPU wait | 없음 | 각 CPU 대상 blocking call 완료 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| smp_call.c | `smp_call_test` / multi-CPU nowait | 없음 | 다중 CPU non-blocking call | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| smp_call.c | `smp_call_test` / interrupt wait | 없음 | interrupt context에서 wait call | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| smp_call.c | `smp_call_test` / multi-CPU wait | 없음 | 다중 CPU blocking call 완료 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| specific.c | `specific_test` / current thread | 없음 | key 생성, 현재 thread set/get | 미실행 | `CONFIG_TESTING_OSTEST_SPECIFIC` 미설정 |
| specific.c | `specific_test` / child isolation | 없음 | child set 값과 parent get 값의 thread별 분리 | 미실행 | `CONFIG_TESTING_OSTEST_SPECIFIC` 미설정 |
| specific.c | `specific_test` / key delete | 없음 | key delete 성공 | 미실행 | `CONFIG_TESTING_OSTEST_SPECIFIC` 미설정 |
| spinlock.c | `spinlock_test` / spinlock | 없음 | 일반 spinlock 경쟁 후 counter 일관성 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| spinlock.c | `spinlock_test` / rspinlock | 없음 | recursive spinlock 경쟁 후 counter 일관성 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| spinlock.c | `spinlock_test` / seqcount | 없음 | seqcount read/write 동기화 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| sporadic.c | `sporadic_test` / FIFO thread | 없음 | FIFO thread의 priority와 실행 결과 | 미실행 | `CONFIG_SCHED_SPORADIC` 미설정 |
| sporadic.c | `sporadic_test` / sporadic thread | 없음 | sporadic budget 소진·replenishment와 priority 변화 | 미실행 | `CONFIG_SCHED_SPORADIC` 미설정 |
| sporadic2.c | `sporadic2_test` / Sporadic 1 | 없음 | 첫 budget/replenishment 조합의 실행 시간 | 미실행 | `CONFIG_SCHED_SPORADIC` 미설정 |
| sporadic2.c | `sporadic2_test` / Sporadic 2 | 없음 | 두 번째 budget/replenishment 조합의 실행 시간 | 미실행 | `CONFIG_SCHED_SPORADIC` 미설정 |
| suspend.c | `suspend_test` / SIGSTOP | 없음 | victim task가 중지되어 출력이 멈춤 | 미실행 | `CONFIG_SIG_SIGSTOP_ACTION`, `CONFIG_SIG_SIGKILL_ACTION` 미설정 |
| suspend.c | `suspend_test` / SIGCONT | 없음 | 중지된 victim task가 재개 | 미실행 | `CONFIG_SIG_SIGSTOP_ACTION`, `CONFIG_SIG_SIGKILL_ACTION` 미설정 |
| suspend.c | `suspend_test` / SIGKILL | 없음 | victim 종료와 종료 후 kill 실패 | 미실행 | `CONFIG_SIG_SIGSTOP_ACTION`, `CONFIG_SIG_SIGKILL_ACTION` 미설정 |
| timedmqueue.c | `timedmqueue_test` / sender timeout | 있음 | full queue에서 마지막 `mq_timedsend`가 `ETIMEDOUT` | PASS |  |
| timedmqueue.c | `timedmqueue_test` / receiver timeout | 있음 | empty queue에서 마지막 `mq_timedreceive`가 `ETIMEDOUT` | PASS |  |
| timedmqueue.c | `timedmqueue_test` / message integrity | 있음 | 정상 메시지 길이와 payload 일치 | PASS |  |
| timedmutex.c | `timedmutex_test` / held mutex timeout | 없음 | mutex 보유 중 waiter가 `ETIMEDOUT`으로 종료 | 미실행 | `CONFIG_TESTING_OSTEST_TIMEDMUTEX` 미설정 |
| timedmutex.c | `timedmutex_test` / sole waiter regression | 없음 | 유일 waiter timeout 후 mutex가 다시 사용 가능 | 미실행 | `CONFIG_TESTING_OSTEST_TIMEDMUTEX` 미설정 |
| timedmutex.c | `timedmutex_test` / remaining waiter regression | 없음 | 짧은 waiter timeout 후 긴 waiter가 unlock을 받아 lock 획득 | 미실행 | `CONFIG_TESTING_OSTEST_TIMEDMUTEX` 미설정 |
| timedwait.c | `timedwait_test` / condition timeout | 있음 | `pthread_cond_timedwait`가 5초 후 `ETIMEDOUT` | PASS |  |
| timedwait.c | `timedwait_test` / mutex cleanup | 있음 | timeout 후 mutex unlock과 thread join | PASS |  |
| tls.c | `tls_test` / zero pattern | 없음 | TLS 전체 element에 0 저장·복원 | 미실행 | `CONFIG_TLS_NELEM` 미설정으로 TLS 기능 의존 조건 불충족 |
| tls.c | `tls_test` / `0xffffffff` pattern | 없음 | TLS 전체 element에 `0xffffffff` 저장·복원 | 미실행 | `CONFIG_TLS_NELEM` 미설정으로 TLS 기능 의존 조건 불충족 |
| tls.c | `tls_test` / `0x55555555` pattern | 없음 | TLS 전체 element에 `0x55555555` 저장·복원 | 미실행 | `CONFIG_TLS_NELEM` 미설정으로 TLS 기능 의존 조건 불충족 |
| tls.c | `tls_test` / `0xaaaaaaaa` pattern | 없음 | TLS 전체 element에 `0xaaaaaaaa` 저장·복원 | 미실행 | `CONFIG_TLS_NELEM` 미설정으로 TLS 기능 의존 조건 불충족 |
| vfork.c | `vfork_test` / child execution | 없음 | vfork child가 parent resume 전에 실행·종료 | 미실행 | `CONFIG_TESTING_OSTEST_VFORK` 미설정 (ARCH vfork 자체는 enabled) |
| vfork.c | `vfork_test` / status·waitpid | 없음 | child exit status와 parent waitpid 결과 | 미실행 | `CONFIG_TESTING_OSTEST_VFORK` 미설정 (ARCH vfork 자체는 enabled) |
| waitpid.c | `waitpid_test` / `waitpid` | 있음 | 지정 PID child 종료 상태 회수 | PASS |  |
| waitpid.c | `waitpid_test` / `waitid(P_PID)` | 있음 | 지정 PID에 대한 `siginfo` 회수 | PASS |  |
| waitpid.c | `waitpid_test` / `waitid(P_ALL)` | 있음 | 모든 child 중 종료한 child 선택·회수 | PASS |  |
| waitpid.c | `waitpid_test` / `wait` | 있음 | 임의 child 종료 상태 회수 | PASS |  |
| wdog.c | `wdog_test` / invalid arguments | 없음 | 잘못된 `wd_start`·`wd_cancel` 인자 거부 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| wdog.c | `wdog_test` / one-shot delays | 없음 | 여러 지연값의 callback 시각 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| wdog.c | `wdog_test` / maximum delay cancel | 없음 | 최대 지연 watchdog 조회·취소 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| wdog.c | `wdog_test` / recursive watchdog | 없음 | callback 재등록 횟수와 elapsed tick | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| wdog.c | `wdog_test` / random cancel | 없음 | random 지연 watchdog 만료·취소 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| wqueue.c | `wqueue_test` / HPWORK | 부분 | HPWORK에서 interval·priority 조합별 work 실행 횟수 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| wqueue.c | `wqueue_test` / LPWORK | 부분 | LPWORK에서 interval·priority 조합별 work 실행 횟수 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| wqueue.c | `wqueue_test` / test queue 1 | 부분 | 생성한 work queue의 work 실행 횟수 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
| wqueue.c | `wqueue_test` / test queue 2 | 부분 | 두 번째 생성 work queue의 work 실행 횟수 | 미실행 | `CONFIG_BUILD_FLAT=n`으로 TC 의존 조건 불충족 |
