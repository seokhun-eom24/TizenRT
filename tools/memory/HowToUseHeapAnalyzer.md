# How to Use Heap Analyzer

`heap_analyzer.py` analyzes TizenRT heap logs captured from the serial
console and writes all reports under the fixed `./output/` directory.

It understands two kinds of events, and a single log may contain any number
of both:

- **alloc_fail** — the dump printed by `mm_manage_alloc_fail` when an
  allocation fails (fail header, heap dump table, summaries, assert info,
  task list, loading info).
- **heapinfo** — the output of the TASH `heapinfo` command in any variant
  (the `-a` variant additionally contains the per-block dump table).

## Requirements

- Python 3
- `openpyxl` (only for the Excel export; everything else works without it)
- `arm-none-eabi-addr2line` + ELF binaries (`tinyara.axf`, `common_dbg`,
  `app1_dbg`) for symbol resolution — skipped automatically when missing

## Usage

```bash
python3 heap_analyzer.py <logfile> [options]

python3 heap_analyzer.py alloc_fail_test.log
python3 heap_analyzer.py heapinfo_test.log --bin-dir ../../build/output/bin
python3 heap_analyzer.py serial.log --no-symbols --event 2
```

The output path cannot be changed through the command line. Reports are
always written to `./output/`, which is deleted and recreated on every run.

| Option | Default | Description |
|---|---|---|
| `--bin-dir DIR` | `build/output/bin` | Directory containing the ELF binaries |
| `--addr2line CMD` | `arm-none-eabi-addr2line` | addr2line command to use |
| `--no-symbols` | off | Skip addr2line symbol resolution |
| `--no-excel` | off | Skip `report.xlsx` |
| `--no-html` | off | Skip `memory_map.html` |
| `--event N` | all | Report only the N-th event |

The script also searches `<script dir>/../../build/output/bin` and
`./build/output/bin` for ELF files, so inside a TizenRT tree symbols usually
resolve without any option.

## Outputs (`output/`)

```text
output/
├── extracted/                     # stage-1 artifacts: cleaned per-event logs
│   ├── event_01_alloc_fail.log    # timestamps stripped, one file per event
│   └── event_02_heapinfo.log
├── report.txt                     # same content as the console output
├── report.xlsx                    # Events / Summary / PID / Owner /
│                                  # Histogram / Blocks / Threads / Tasks /
│                                  # Diagnostics sheets (E<N>_ prefix when
│                                  # the log holds multiple events)
├── memory_map.html                # interactive HTML memory map
└── analysis.json                  # full parsed data for further processing
```

## Reading the report

Per event you get the fail header (requested size, caller with symbol,
largest/total free and the concluded cause), followed by one section per
heap. Kernel and application heaps remain separate:

- **Summary / Details** — values reported by the target, plus values observed
  by summing the dump table. A difference may represent heap metadata or lost
  rows.
- **Fragmentation index** — `(1 - largest free / total free) * 100`.
  `0%` means all free memory is one contiguous run.
- **PID table** — per-PID allocation totals split into heap and stack usage,
  joined with task names from the alive-thread and task-list tables.
- **Owner table** — allocation call sites, with addr2line symbols when
  available.
- **Histogram** — allocated block sizes in doubling buckets.
- **Dead-thread allocations** — memory still owned by dead PIDs and therefore
  possible leak candidates.

The HTML map shows each REGION as a vertical address-ordered strip. Consecutive
heap, stack, and free blocks are merged into runs. Memory not covered by valid
parsed rows is displayed as `unknown`; these inferred areas are excluded from
allocation/free totals and histograms. Clicking a run shows its member blocks
with PID, owner, and symbol information.

## Log truncation and corruption

Serial logs are frequently damaged by log volume, interleaved streams, and
reboots. The analyzer detects and reports this instead of failing:

- `(!) TRUNCATED sections:` lists sections that were cut, such as
  `dump_table:<heap>`, `stack_dump`, and `task_list`.
- Numbers derived from a truncated dump are marked **LOWER BOUND**.
- `Malformed/corrupted rows skipped` counts damaged table rows.
- `Address continuity: N gap(s)/overlap(s)` reports holes or overlaps between
  adjacent parsed blocks.
- Block addresses must have exactly `0x` followed by eight hexadecimal digits.
  Invalid addresses are excluded from block totals and maps.

## Extending the parser

Section parsers live in `heap_analyzer.py` section `[S4]` and are registered in
`SECTION_REGISTRY` as `(header regex, parser function)`. To support a new log
section, add a regex, write a parser that consumes lines until the next
registered section header, and add it to the registry. Data models are plain
dataclasses in section `[S1]`.

## Tests

```bash
env TMPDIR=/dev/shm python3 -B -m unittest test_heap_analyzer -v
```

The suite covers event extraction, malformed and truncated rows, REGION
boundary validation, section parsers, analyzer metrics, symbol resolution
(mocked), reporters, CLI behavior, and end-to-end runs against the sample
logs when present.
