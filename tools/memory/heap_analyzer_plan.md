# heap_analyzer.py 재구현 계획

기존 `heapfailanalyzer.py`와 별개의 새 스크립트. **단일 파일** `heap_analyzer.py` +
`test_heap_analyzer.py` 로 구성하되, 파일 내부를 역할별 계층(섹션)으로 명확히 나눠
파서 추가/교체가 쉬운 구조로 만든다.

---

## 0. 설계 원칙

1. **파이프라인 5단계 분리** — 각 단계는 입력/출력 자료형이 명확한 독립 계층.
   단계 사이는 dataclass로만 통신 (전역 상태 없음).
2. **잘림(truncation)은 1급 개념** — 모든 파싱 결과에 `truncated`/`parse_status`를
   달고, 최종 리포트까지 전파해서 "이 수치는 부분 데이터 기반"임을 표시.
3. **파서는 registry 패턴** — 섹션 헤더 정규식 → 파서 함수 매핑 테이블 하나만
   고치면 새 섹션 지원 가능.
4. **모든 산출물은 `output/` 아래로** — 중간 산출물(추출된 이벤트 로그)도 보존.

---

## 1. 전체 파이프라인

```
사용자 입력
   │
   ▼
┌─────────────────────────────────────────────────────────────────┐
│ [Stage 0] CLI (argparse)                                        │
│   logfile, --bin-dir, --addr2line, --no-symbols,                │
│   --no-excel, --no-html, --event N(특정 이벤트만)                │
└──────────────┬──────────────────────────────────────────────────┘
               ▼
┌─────────────────────────────────────────────────────────────────┐
│ [Stage 0.5] OutputManager                                       │
│   output/ 전체 삭제 → 재생성 (output/extracted/ 포함)            │
└──────────────┬──────────────────────────────────────────────────┘
               ▼
┌─────────────────────────────────────────────────────────────────┐
│ [Stage 1] LogExtractor  ── "로그 정리"                           │
│   raw log ──▶ 타임스탬프 제거 ──▶ 이벤트 경계 감지 ──▶            │
│   이벤트별 클린 로그 파일 저장 (여러 개면 여러 파일)               │
│     output/extracted/event_01_alloc_fail.log                    │
│     output/extracted/event_02_heapinfo.log ...                  │
└──────────────┬──────────────────────────────────────────────────┘
               ▼  [ExtractedEvent, ...]
┌─────────────────────────────────────────────────────────────────┐
│ [Stage 2] SectionScanner + Parser Registry ── "부분별 파싱"      │
│   클린 로그를 순차로 읽으며 섹션 헤더 감지 → 해당 파서 호출        │
│   (fail 정보 / 요약 / 디테일 / 덤프 / 스레드 / assert / ...)      │
└──────────────┬──────────────────────────────────────────────────┘
               ▼  Event (구조화된 dataclass 트리)
┌─────────────────────────────────────────────────────────────────┐
│ [Stage 3] Analyzer ── 파생 지표 계산                             │
│   PID/Owner 집계, top5, 히스토그램, 단편화 지수, gap 추론,        │
│   observed vs reported 교차검증, PID↔태스크명 결합               │
└──────────────┬──────────────────────────────────────────────────┘
               ▼
┌─────────────────────────────────────────────────────────────────┐
│ [Stage 4] SymbolResolver ── addr2line (바이너리 없으면 스킵)      │
└──────────────┬──────────────────────────────────────────────────┘
               ▼  AnalysisResult
┌─────────────────────────────────────────────────────────────────┐
│ [Stage 5] Reporters                                             │
│   ConsoleReporter ──▶ stdout + output/report.txt                │
│   ExcelReporter   ──▶ output/report.xlsx                        │
│   HtmlReporter    ──▶ output/memory_map.html                    │
│   JsonDumper      ──▶ output/analysis.json (raw 결과 보존)       │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. 파일 내부 구성 (단일 파일)

```
heap_analyzer.py
│ #=== [S0] Constants & Regex ===        모든 섹션 마커/행 정규식 한곳에
│ #=== [S1] Data Models ===              dataclass 정의 (아래 4장)
│ #=== [S2] Output Manager ===           output/ 삭제·생성, 경로 헬퍼
│ #=== [S3] Log Extractor ===            Stage 1
│ #=== [S4] Section Parsers ===          Stage 2 (파서 함수들 + REGISTRY)
│ #=== [S5] Analyzer ===                 Stage 3
│ #=== [S6] Symbol Resolver ===          Stage 4
│ #=== [S7] Reporters ===                Stage 5 (console/excel/html/json)
│ #=== [S8] CLI & main ===               argparse + 파이프라인 조립
└─ (약 1500~2000줄 예상)

test_heap_analyzer.py                    pytest, 계층별 단위 + e2e
```

교체 용이성: main()은 각 단계를 함수 호출로만 연결하므로, 예를 들어
HTML 리포터를 바꾸려면 `#=== [S7]`의 해당 함수만 교체하면 된다.

---

## 3. Stage 1 — 이벤트 감지·추출 (로그 정리)

### 3-1. 타임스탬프 처리

```
[2026-07-22 16:10:46.014] mm_malloc: ...     ← 프리픽스 제거
mm_malloc: ...                                ← 프리픽스 없어도 동작
```

- `^\[\d{4}-\d{2}-\d{2} [\d:.]+\]\s?` 제거. 단, **이벤트의 시작/끝 타임스탬프는
  메타데이터로 보존** (리포트에 "언제 발생한 이벤트"인지 표기).

### 3-2. 이벤트 종류와 경계

```
                    시작 마커                          종료 판정
┌─────────────┬──────────────────────────────┬────────────────────────────┐
│ ALLOC_FAIL  │ "mm_manage_alloc_fail…:      │ 아래 중 먼저 오는 것:        │
│             │  Allocation failed from      │  · 다음 이벤트 시작 마커     │
│             │  (user|kernel) heap."        │  · 리부트 마커(boardctl/    │
│             │  (mm_malloc 실패 줄부터 포함) │    ROM:[V…]/Autoboot)       │
│             │                              │  · EOF                     │
├─────────────┼──────────────────────────────┼────────────────────────────┤
│ HEAPINFO    │ "TASH>>heapinfo…" 커맨드 줄  │  · 마지막 알려진 섹션 종료   │
│             │ 또는 "KERNEL HEAP            │    후 비섹션 라인 연속       │
│             │  INFORMATION" 배너           │  · 다음 이벤트 시작 마커     │
│             │ (커맨드 옵션 -a 등 기록)      │  · EOF                     │
└─────────────┴──────────────────────────────┴────────────────────────────┘
```

- ALLOC_FAIL은 힙 덤프 + 요약 + (커널 힙 요약) + **assert 정보 + task list +
  로딩 정보**까지 한 이벤트로 포함 (alloc_fail_test.log 전체가 한 이벤트).
- 이벤트가 여러 개면 발견 순서대로 번호를 붙여 **각각 별도 파일로 저장**:
  `output/extracted/event_<NN>_<kind>.log`
- 각 이벤트에 원본 라인 범위(시작~끝)를 기록 → 리포트/디버깅에서 원본 추적 가능.

### 3-3. 실로그에서 확인된 오염 패턴 (반드시 처리)

실제 로그 3종에서 관찰된 사례 — extractor/파서가 모두 견뎌야 함:

```
① 줄 붙음: 이벤트 시작이 이전 줄 중간에 붙음
   "…Try to free dead tasmm_manage_alloc_fail_dump: Allocation failed…"
   → 시작 마커를 줄 중간에서도 search, 마커 위치부터 절단

② 덤프 행 잘림 (alloc_fail_real.log:5079)
   "0x605c6810 |     "   ← 행이 중간에서 끊기고 다음 섹션 시작
   → 부분 행은 malformed+truncated로 기록, 테이블 파싱 종료

③ 스택덤프 잘림+헤더 붙음 (alloc_fail_real.log:5134)
   "…604c0930 604==========="   ← 덤프가 끊기며 다음 구분선과 한 줄로
   → 줄 중간의 "===========" / 섹션 헤더도 경계로 인식

④ task list 잘림 (alloc_fail_real.log:5170)
   행 중간에서 끊기고 "boardctl: Board will Reboot now" 로 이어짐
   → 리부트 마커를 이벤트 종료로 처리, task list는 truncated 표시

⑤ 덤프 행 중간 유실 (alloc_fail_real.log:1002)
   "0  A    | 0x e1733c9 |  16   |"   ← 행 앞부분이 통째로 사라짐
   → 파이프 2개 이상이면 깨진 행으로 간주(malformed), 테이블 파싱은 계속
```

---

## 4. 데이터 모델 (dataclass 트리)

```
AnalysisResult
├─ source: SourceInfo            파일명, 크기, 총 이벤트 수, 스캔 범위
└─ events: [Event]

Event
├─ index: int                    1부터, 추출 파일 번호와 일치
├─ kind: "alloc_fail" | "heapinfo"
├─ command: str|None             "heapinfo -a" 등 (HEAPINFO만)
├─ time_range: (str|None, str|None)   원본 타임스탬프 시작/끝
├─ source_lines: (int, int)      원본 라인 범위
├─ extracted_path: str           output/extracted/… 경로
├─ fail_info: FailInfo|None      ← ALLOC_FAIL만
├─ heaps: [HeapSnapshot]         ← 커널/앱 힙별로 하나씩
├─ assertion: AssertionInfo|None
├─ task_list: TaskList|None      (entries + truncated)
├─ loading_info: [BinTextAddr]   [common]/[app1] Text Addr/Size
└─ diagnostics: Diagnostics      잘린 섹션 목록, malformed 수, 연속성 gap

FailInfo:      requested_size, caller_address, largest_free, total_free,
               heap_kind("user"/"kernel"), fail_line(원문)
HeapSnapshot
├─ name: "kernel"|"app1"|…       배너에서 추출 → 유저/커널 구분의 기준
├─ heap_range: (start, end)      mm_check_heap_corruption 줄에서
├─ corruption: bool|None
├─ regions: [Region]             number, start, end, size
├─ blocks: [Block]               ← 덤프 테이블 (없을 수 있음: heapinfo 기본)
├─ blocks_truncated: bool        ② 패턴 감지 시 True
├─ summary: HeapSummary|None     total, alloc_current, alloc_peak, free, reserved
├─ details: HeapDetails|None     free_node_count, largest_free_node,
│                                dead_thread_alloc, stack_sum, curr_heap_sum
├─ nodelist: [NodeBucket]        range_min, range_max, num, size
├─ dead_threads: [(pid, size)]
├─ alive_threads: [ThreadEntry]  pid, ppid, stack, curr_heap, peak_heap, bin, name
└─ derived: DerivedMetrics|None  ← Stage 3에서 채움

Block:         region, address, size, status(A/F/G), owner, pid, is_stack,
               category(heap/stack/free/gap), symbol(Stage 4에서)
DerivedMetrics: by_pid, by_owner, top_pids, top_owners, histogram,
               fragmentation_index, total_allocated, observed_free,
               largest_allocation, continuity_gaps, partial(bool ← 잘림 기반)
AssertionInfo: file, line, task, pid, pc, stack(base/size/used, truncated), tcb{}
```

**유저/커널 구분**은 `HeapSnapshot.name`으로 일원화:
- `Summary of current app (app1) heap…` → app1
- `Summary of Kernel heap…` / `KERNEL HEAP INFORMATION` → kernel
- `app1 HEAP INFORMATION` → app1
리포트의 모든 표는 힙(=스냅샷) 단위로 나눠 출력한다.

---

## 5. Stage 2 — 섹션 스캐너 + 파서 registry

### 5-1. 동작 방식

```
클린 로그 (한 이벤트)
   │  한 줄씩
   ▼
┌────────────────────────────────────────────────┐
│ for line in lines:                             │
│   for marker_re, parser in SECTION_REGISTRY:   │
│     if marker_re.search(line):                 │
│       parser(iterator, event, context)  ──────┼──▶ 파서가 자기 섹션 끝까지
│       break                                    │    소비하고 반환
└────────────────────────────────────────────────┘
   context: 현재 어느 HeapSnapshot에 속하는지 (배너 파서가 갱신)
```

- 파서는 **다음 섹션 헤더를 만나면 그 줄을 되돌려주고 반환** (lookahead 1줄
  버퍼). 알 수 없는 줄은 스킵하되 카운트.
- 반환값은 항상 `(data, status)` — status ∈ {OK, TRUNCATED, MALFORMED_PARTIAL}.

### 5-2. 파서 목록 (SECTION_REGISTRY)

```
 #  섹션 마커 (요지)                          파서              채우는 필드
──  ─────────────────────────────────────    ───────────────   ─────────────────────
 1  "Allocation failed from … heap."         parse_fail_info   event.fail_info
 2  "Summary of current app (…)"/            parse_heap_banner 새 HeapSnapshot 시작
    "Summary of Kernel heap…"/                                 (context 전환)
    "… HEAP INFORMATION"
 3  "mm_check_heap_corruption: Heap start"   parse_corruption  heap.heap_range 등
 4  "REGION #n Start=…"                      parse_region      heap.regions
 5  "MemAddr | Size | Status |…"             parse_dump_table  heap.blocks (+잘림)
 6  "Summary of Heap Usages"                 parse_summary     heap.summary
 7  "Details of Heap Usages"                 parse_details     heap.details
 8  "Available fragmented memory segments"   parse_nodelist    heap.nodelist
 9  "< by Dead Threads >"                    parse_dead        heap.dead_threads
10  "< by Alive Threads >"                   parse_alive       heap.alive_threads
11  "Assertion details"                      parse_assertion   event.assertion
12  "Asserted task's stack details"          parse_stack       assertion.stack
13  "Asserted task's TCB info"               parse_tcb         assertion.tcb
14  "List of all tasks…" 또는                parse_task_list   event.task_list
    "NAME | PID | PRI | …" 헤더
15  "Loading location information"           parse_loading     event.loading_info
```

- 덤프 테이블 행 정규식·검증 규칙(A인데 owner/pid 없음 → malformed 등)은
  기존 heapfailanalyzer.py 로직 승계.
- `0x    dead`(공백 hex), `0x0x` 프리픽스, `19(S)` 스택 표기 처리도 승계.

### 5-3. 잘림 감지 규칙 (덤프 테이블 기준)

```
행 판독 결과                           처리
───────────────────────────────────   ─────────────────────────────────
완전한 행 (BLOCK_RE fullmatch)         blocks에 추가
"0x… |" 로 시작하나 필드 부족/끊김      malformed += 1, blocks_truncated=True
다음 섹션 헤더 등장                     테이블 종료 (정상)
terminator("** PID(S)…") 없이 종료     blocks_truncated=True
```

잘림 플래그는 `DerivedMetrics.partial`로 전파 → 리포트에서
"⚠ dump truncated — 아래 수치는 하한값" 배너 출력.

---

## 6. Stage 3 — Analyzer (파생 지표)

힙 스냅샷마다 (기존 기능 승계 + 신규):

```
승계 (heapfailanalyzer.py 로직 재사용)
├─ by_pid / by_owner 집계, top 5
├─ 크기 히스토그램 (1-16, 17-32, … 지수 버킷)
├─ 단편화 지수 = (1 - largest_free/total_free) × 100
├─ gap 추론(G 블록) — HTML 맵용
├─ 주소 연속성 검사 (gap/overlap)
└─ observed(테이블 합산) vs reported(요약 수치) 교차검증

신규
├─ PID ↔ 태스크명 결합: alive_threads/task_list에서 이름·BIN을 찾아
│   PID 요약에 "19 app1(app1)" 처럼 표기 (유저/커널 BIN 구분 포함)
├─ heap/stack 용도 구분 집계: (S) 블록 vs 일반 블록 + alive_threads의
│   STACK/CURR_HEAP/PEAK_HEAP 컬럼과 대조
├─ dead threads 누수 후보: dead_threads 표 + dump의 dead pid 블록 매칭
├─ nodelist 기반 free 분포 (heapinfo 로그의 실제 free 조각 분포)
└─ Peak vs Current 사용률 (summary의 Peak 활용)
```

---

## 7. Stage 4 — 심볼 해석 (addr2line)

```
수집 대상: 모든 블록 owner + fail caller + assert PC
ELF 후보: --bin-dir, ../../build/output/bin, ./build/output/bin
          × (tinyara.axf, common_dbg, app1_dbg)
실행:     ELF당 1회 배치 호출 (addr2line -f -C -e ELF addr…)
병합:     resolved 결과 우선 (커널 ELF ??인 주소를 앱 ELF가 해석 가능)
스킵:     ELF 파일이 하나도 없거나 --no-symbols → 전부 "??" + 리포트에
          "symbols skipped (no binary)" 명시
활용:     loading_info의 Text Addr([common] 0xe261010 등)을 리포트에 함께
          출력해 어느 바이너리 주소 대역인지 판단 근거 제공
```

---

## 8. Stage 5 — 결과물

### 8-1. output 디렉토리 (시작 시 전체 삭제 후 재생성)

```
output/
├── extracted/
│   ├── event_01_alloc_fail.log      ← Stage 1 산출물 (클린 로그)
│   └── event_02_heapinfo.log
├── report.txt                       ← 콘솔 출력과 동일 내용
├── report.xlsx
├── memory_map.html
└── analysis.json                    ← 전체 파싱 결과 (기계 판독/후처리용)
```

### 8-2. 콘솔/텍스트 리포트 구성

```
[이벤트 목록]  #, 종류, 시각, 잘림 여부, 원본 라인 범위
[이벤트별]
  ├─ (fail만) FAIL 정보: 요청 크기, caller(+심볼), largest/total free
  ├─ ⚠ 잘림/진단 배너 (truncated 섹션, malformed 수, 연속성 gap)
  ├─ 힙별(커널/앱 구분):
  │   ├─ 전체 지표: Total/Alloc(Curr/Peak)/Free/Reserved,
  │   │            observed vs reported, 단편화 지수
  │   ├─ PID별 표 (태스크명·BIN 포함, heap/stack 분리, %)
  │   ├─ Owner별 표 (심볼, PID 목록, top 5 강조)
  │   ├─ 크기 히스토그램 / nodelist 분포
  │   └─ 최대 할당, dead threads
  └─ (fail만) assert 요약: 파일:라인, task, PC(+심볼)
```

### 8-3. Excel (이벤트×힙 단위 시트)

```
시트: Events | E1_Summary | E1_kernel_PID | E1_kernel_Owner | E1_app1_… |
      Histogram | Blocks(심볼 포함) | Threads | TaskList | Diagnostics
(이벤트가 1개면 접두사 생략해 단순화)
```

### 8-4. HTML 메모리 맵 (기존 canvas 방식 승계 + 확장)

```
┌───────────────────────────────────────────────┐
│ [이벤트 선택 ▼] [힙 선택: kernel/app1 ▼]        │
│ ☑Heap ☑Stack ☑Free ☑Gap   Zoom──── Pan────    │
│ ┌───────────────────────────────────────────┐ │
│ │ REGION #0  ▓▓▓░░▓▓▓▓░░░░▓░░░░░░░░░░░░░░░  │ │  hover → 주소/크기/PID/
│ └───────────────────────────────────────────┘ │  태스크명/owner/심볼
│ ⚠ dump truncated (마지막 유효 주소 이후 미상)   │
└───────────────────────────────────────────────┘
데이터는 JSON 임베드 1개로 통합(이벤트→힙→블록), 잘린 구간은 표시
```

---

## 9. CLI 설계

```
python3 heap_analyzer.py <logfile>
                          출력은 고정된 ./output에 생성 (시작 시 내용 전체 삭제!)
    [--bin-dir DIR]       기본 build/output/bin
    [--addr2line CMD]     기본 arm-none-eabi-addr2line
    [--no-symbols]        addr2line 스킵
    [--no-excel] [--no-html]
    [--event N]           N번째 이벤트만 상세 리포트 (기본: 전부)
```

---

## 10. 테스트 계획 (test_heap_analyzer.py, pytest)

```
fixtures (테스트 파일 내 문자열 상수 + 실로그 활용)
├─ 실로그 3종 존재 시 e2e: alloc_fail_test / alloc_fail_real / heapinfo_test
└─ 합성 케이스: 타임스탬프 유/무, 줄 붙음(①), 행 잘림(②),
   스택덤프 잘림(③), 다중 이벤트, 빈 로그, 이벤트 없음

단위 테스트 (계층별)
├─ extractor: 이벤트 수/종류/경계, 타임스탬프 제거, 파일 저장
├─ parsers: 섹션별 정상 파싱 + 잘림 상태 반환 (표 15종 각각 최소 1케이스)
├─ analyzer: 집계 수치, 단편화 지수, PID↔이름 결합, partial 플래그
├─ symbols: addr2line 모킹(subprocess 대체), ELF 없음 → 스킵
└─ reporters: output 파일 생성 여부, 잘림 경고 문구 포함 여부

e2e 스모크
└─ 실로그 → output/ 산출물 5종 생성 + 핵심 수치 스냅샷 비교
   (예: alloc_fail_test.log → 이벤트 1개, requested 70135168,
        blocks 잘림 없음 / alloc_fail_real.log → blocks_truncated=True)
```

---

## 11. 구현 순서 (마일스톤)

```
M1  뼈대: CLI + OutputManager + Models
    └ 테스트: output 삭제/생성
M2  Extractor (이벤트 감지·분할·클린 로그 저장)
    └ 테스트: 오염 패턴 ①~④, 다중 이벤트
M3  핵심 파서: fail_info, 배너/힙 전환, dump_table(잘림), summary
    └ 이 시점에 alloc_fail 로그 최소 분석 가능
M4  나머지 파서: details, nodelist, threads, assertion, task_list, loading
M5  Analyzer + Console/JSON 리포터  ← 최소 유용 버전(MVP) 완성
M6  SymbolResolver + Excel + HTML 리포터
M7  실로그 3종 검증, HowToUseHeapAnalyzer.md 작성, 마무리
```

## 12. 기존 대비 주요 개선점 요약

| 항목 | 기존 heapfailanalyzer.py | 새 heap_analyzer.py |
|---|---|---|
| 대상 로그 | alloc fail만 | alloc fail + heapinfo(옵션별) |
| 다중 발생 | 마지막 complete 1개만 상세 | 이벤트별 분리 추출·분석 |
| 중간 산출물 | 없음 | output/extracted/ 클린 로그 |
| 잘림 처리 | complete 여부만 | 섹션별 truncated 추적·경고 |
| 커널/유저 | 사실상 유저 힙만 | HeapSnapshot 단위 구분 |
| assert/task | 미파싱 | 파싱 + PID↔태스크명 결합 |
| 산출물 위치 | CWD에 개별 지정 | output/ 일원화(+json) |
| 테스트 | 일부 | 계층별 단위 + e2e |
```
