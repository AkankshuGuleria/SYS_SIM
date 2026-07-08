# CPU Scheduling Studio

A tick-by-tick CPU scheduling simulator written in C++17.  Implements seven
algorithms — including MLFQ — with a live ANSI dashboard, Gantt chart
generation, a scheduling decision log, and Jain's Fairness Index alongside
the standard metrics.

---

## Why this exists

I built this for my Operating Systems course, but I also wanted to understand
*why* each algorithm fails before I understood why the next one was invented.
FCFS teaches you about the convoy effect.  SJF teaches you that knowing burst
length up-front is a fantasy.  SRTF gets you optimal average wait but punishes
long jobs.  Priority scheduling is clinically starvation-prone — which is why
I added the aging mechanism directly to the non-preemptive version rather than
leaving it in a "future work" list.  MLFQ is where those lessons converge: it
approximates SJF without requiring burst-time knowledge, and aging prevents
the starvation that would otherwise haunt lower queues.

### Design decisions

**Tick-by-tick vs event-driven.**  A discrete-clock loop made the animated
dashboard straightforward: every tick I call `onTick()` and the UI reads the
live process states.  An event-driven approach would be faster for large
simulations but would complicate the per-tick callback interface.  For an
educational tool with at most ~50 processes the tradeoff favors readability.

**Configurable RR quantum.**  The right quantum depends entirely on the
workload.  Hard-coding it would hide that trade-off.  Exposing it in Settings
lets you observe how smaller quanta improve responsiveness at the cost of
more context switches, which is the actual lesson.

**MLFQ queue count = 3.**  Two queues blur the demotion effect; four queues
add complexity without new insight for the burst lengths typical in test cases
(1–15 ticks).  Three (Q0=quantum, Q1=2×quantum, Q2=FCFS) is the structure
described in most OS textbooks and mirrors real-world scheduler designs like
the early Unix BSD scheduler.

**Aging threshold = 4×quantum.**  Aging must kick in before a process waits
longer than the quantum of its current queue, otherwise demotion and promotion
cycle each other with no net effect.  Four quanta gives enough headroom for
short bursts to clear out naturally before long-waits are rescued.

---

## Algorithms

| # | Algorithm | Preemptive | Starvation risk | Notes |
|---|---|---|---|---|
| 1 | FCFS | No | No | Convoy effect on long bursts |
| 2 | SJF | No | Yes | Optimal avg WT among NP algorithms |
| 3 | SRTF | Yes | Yes | Optimal avg WT overall; penalises long jobs |
| 4 | Priority (NP) | No | Partially mitigated | Aging raises effective priority every 4 ticks |
| 5 | Priority (P) | Yes | Yes | Immediate preemption on higher-priority arrival |
| 6 | Round Robin | Yes | No | Fairness depends heavily on quantum choice |
| 7 | MLFQ | Yes | No | Adaptive; aging prevents lower-queue stagnation |

---

## Metrics

| Metric | Formula |
|---|---|
| Completion Time (CT) | Time process finishes |
| Turnaround Time (TAT) | CT − Arrival |
| Waiting Time (WT) | TAT − Burst |
| Response Time (RT) | First CPU − Arrival |
| CPU Utilisation | (Total − Idle) / Total × 100 |
| Throughput | Processes / Total Time |
| **Jain's Fairness Index** | (ΣTAT)² / (n × ΣTAT²) — range [1/n, 1.0] |
| Context Switches | Gantt task-changes (Idle excluded) |

Jain's Index is the canonical equity metric from Raj Jain's 1991 paper.
A value of 1.0 means every process got an identical turnaround time; 1/n
means one process took all the benefit.  It lets you see, for example, that
SRTF often scores lower fairness than Round Robin even when its average
waiting time is smaller.

---

## Build

**CMake (recommended):**

```bash
# From the CPU-Scheduling-Studio/ directory
cmake -B build -G "MinGW Makefiles"
cmake --build build
./build/cpu_studio.exe
```

**Manual g++ (fallback):**

```bash
g++ -std=c++17 -O2 -o cpu_studio \
    main.cpp \
    src/Scheduler.cpp src/Dashboard.cpp \
    src/Process.cpp   src/Utilities.cpp \
    algorithms/FCFS.cpp  algorithms/SJF.cpp \
    algorithms/SRTF.cpp  algorithms/Priority.cpp \
    algorithms/PriorityPreemptive.cpp \
    algorithms/RoundRobin.cpp algorithms/MLFQ.cpp
```

> Requires: g++ ≥ 9 with C++17.  Run in Windows Terminal or VS Code's
> integrated terminal for full ANSI colour support.

---

## Tests

```bash
cmake --build build --target run_tests
cd build && ctest -V
```

Seven assert-based tests in `tests/test_scheduling.cpp`.  Each expected value
was worked out by hand from the algorithm definition and stated in comments
alongside the working, so you can verify the maths without running anything.
The SRTF test uses the exact 4-process example from Silberschatz *OS Concepts*
Table 6.4.

---

## Project structure

```
CPU-Scheduling-Studio/
├── main.cpp                  # Entry point, menu, compare, export
├── CMakeLists.txt
├── include/
│   ├── Colors.h              # ANSI codes, colour helpers
│   ├── Process.h             # Process, GanttEntry, SimulationResult
│   ├── Scheduler.h           # Abstract base, AppSettings
│   ├── Utilities.h           # Formatting, stats (incl. Jain's index), I/O
│   └── Dashboard.h           # Live panel declarations
├── src/
│   ├── Scheduler.cpp         # finalizeResult, countContextSwitches
│   ├── Dashboard.cpp         # Animated panel renderers
│   ├── Process.cpp
│   └── Utilities.cpp
├── algorithms/
│   ├── FCFS.cpp
│   ├── SJF.cpp
│   ├── SRTF.cpp
│   ├── Priority.cpp          # Non-preemptive + aging
│   ├── PriorityPreemptive.cpp
│   ├── RoundRobin.cpp
│   └── MLFQ.cpp              # 3-level feedback + aging
├── tests/
│   └── test_scheduling.cpp   # Hand-verified assert tests
└── reports/
    └── report.txt            # Auto-generated on export
```

---

## Usage

1. Start the program. Main menu lists all options.
2. Load processes via option 1 (manual) or 2 (random: 5/10/20/50).
3. Run any algorithm (options 4–10). Live dashboard updates each tick.
4. After simulation: full Gantt chart, per-process table, and stats including
   the fairness index are printed. Optionally view the scheduling decision log.
5. Option 11 compares all seven algorithms on the same process set side-by-side.
6. Option 12 exports a full text report to `reports/report.txt`.
7. Option 13 adjusts animation speed, step mode, and RR quantum.

---

Author: Operating Systems project, C++17, 2026
