// MLFQ.cpp - Multi-Level Feedback Queue Scheduling
//
// Design rationale (why this exists alongside the other six):
//
//   FCFS and SJF assume burst time is known upfront; MLFQ does not.
//   Priority NP suffers starvation; RR is fair but ignores burst length.
//   MLFQ approximates SJF adaptively: short jobs self-select into the
//   high-priority queue by finishing within the quantum; long jobs
//   progressively demote themselves.  A promotion (aging) rule then
//   prevents the starvation that haunts static-priority schemes.
//
// Queue structure (configurable via settings.timeQuantum as Q1 base):
//   Q0  highest priority, quantum = timeQuantum          (default 4)
//   Q1  medium  priority, quantum = timeQuantum * 2      (default 8)
//   Q2  lowest  priority, FCFS (runs until completion)
//
// Demotion: if a process exhausts its quantum at level k < 2, it drops
//           to level k+1 and goes to the back of that queue.
//
// Promotion / aging: every tick, any process waiting in Q1 or Q2 has
//           its `ageTicks` incremented.  Once ageTicks exceeds the
//           aging threshold (4 * timeQuantum), it is promoted one
//           level and ageTicks resets.  This guarantees no process
//           waits indefinitely regardless of the workload.

#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>
#include <deque>
#include <string>

class MLFQScheduler : public Scheduler {
public:
    MLFQScheduler() : Scheduler("MLFQ (3-level Feedback + Aging)") {}

    SimulationResult run(
        std::vector<Process> procs,
        const AppSettings&   settings,
        std::function<void(int, const std::vector<Process>&,
                           const std::vector<GanttEntry>&,
                           const std::vector<SchedulingLog>&)> onTick) override
    {
        SimulationResult result;
        result.algorithmName = algorithmName_
            + " [Q=" + std::to_string(settings.timeQuantum) + "]";
        for (auto& p : procs) p.resetForSimulation();

        std::vector<GanttEntry>    gantt;
        std::vector<SchedulingLog> log;

        const int n            = (int)procs.size();
        const int Q0           = settings.timeQuantum;      // e.g. 4
        const int Q1           = settings.timeQuantum * 2;  // e.g. 8
        const int agingThresh  = settings.timeQuantum * 4;  // e.g. 16

        // Per-process mutable metadata (not part of Process struct
        // so we don't pollute the shared data model)
        std::vector<int> queueLevel(n, 0);   // which queue (0,1,2)
        std::vector<int> ageTicks(n, 0);     // ticks spent waiting in Q1/Q2

        // Three FIFO ready queues holding indices into procs[]
        std::deque<int> q[3];

        int time      = 0;
        int completed = 0;

        // Sort arrival order for efficient new-arrival scanning
        std::vector<int> byArrival(n);
        for (int i = 0; i < n; i++) byArrival[i] = i;
        std::stable_sort(byArrival.begin(), byArrival.end(),
            [&](int a, int b){ return procs[a].arrivalTime < procs[b].arrivalTime; });

        int nextCheck = 0;

        auto enqueueArrivals = [&]() {
            while (nextCheck < n &&
                   procs[byArrival[nextCheck]].arrivalTime <= time) {
                int idx = byArrival[nextCheck++];
                procs[idx].state = ProcessState::READY;
                q[0].push_back(idx);   // all new processes enter Q0
            }
        };

        // Promotion pass: scan Q1 and Q2 for processes that have
        // aged past the threshold and move them up one level.
        auto applyAging = [&]() {
            for (int level = 1; level <= 2; level++) {
                std::deque<int> kept;
                for (int idx : q[level]) {
                    ageTicks[idx]++;
                    if (ageTicks[idx] >= agingThresh) {
                        ageTicks[idx]  = 0;
                        queueLevel[idx] = level - 1;
                        q[level - 1].push_back(idx);  // promote
                        log.push_back({ time, procs[idx].pid,
                            "Aged out of Q" + std::to_string(level)
                            + " -> promoted to Q" + std::to_string(level-1) });
                    } else {
                        kept.push_back(idx);
                    }
                }
                q[level] = kept;
            }
        };

        enqueueArrivals();

        while (completed < n) {
            // Find the highest-priority non-empty queue
            int chosen_q = -1;
            for (int lv = 0; lv <= 2; lv++) {
                if (!q[lv].empty()) { chosen_q = lv; break; }
            }

            // CPU idle
            if (chosen_q == -1) {
                if (nextCheck >= n) break;
                int nextArr = procs[byArrival[nextCheck]].arrivalTime;
                gantt.push_back({ "Idle", time, nextArr });
                for (int t = time; t < nextArr; t++) {
                    if (onTick) onTick(t, procs, gantt, log);
                    if (settings.stepMode) pressEnter();
                    else sleepMs(settings.animationSpeedMs);
                }
                time = nextArr;
                enqueueArrivals();
                continue;
            }

            // Dequeue from chosen level
            int idx      = q[chosen_q].front();
            q[chosen_q].pop_front();
            Process& cur = procs[idx];
            cur.state    = ProcessState::RUNNING;
            if (cur.startTime == -1) cur.startTime = time;
            ageTicks[idx] = 0;   // reset age counter when on CPU

            // Determine quantum for this level (Q2 = unlimited, so use remaining)
            int quantum = (chosen_q == 0) ? Q0
                        : (chosen_q == 1) ? Q1
                        :                   cur.remainingTime;

            log.push_back({ time, cur.pid,
                "Q" + std::to_string(chosen_q) + " (quantum=" +
                std::to_string(quantum) + ", rem=" +
                std::to_string(cur.remainingTime) + ")" });

            int startT    = time;
            int ticksDone = 0;

            while (ticksDone < quantum && cur.remainingTime > 0) {
                if (onTick) onTick(time, procs, gantt, log);
                if (settings.stepMode) pressEnter();
                else sleepMs(settings.animationSpeedMs);

                cur.remainingTime--;
                time++;
                ticksDone++;

                enqueueArrivals();
                applyAging();
            }

            gantt.push_back({ cur.pid, startT, time });

            if (cur.remainingTime == 0) {
                cur.state          = ProcessState::TERMINATED;
                cur.completionTime = time;
                completed++;
            } else {
                // Demote if quantum exhausted and not already at Q2
                if (chosen_q < 2) {
                    queueLevel[idx] = chosen_q + 1;
                    log.push_back({ time, cur.pid,
                        "Quantum exhausted -> demoted Q"
                        + std::to_string(chosen_q)
                        + " -> Q" + std::to_string(chosen_q + 1) });
                } else {
                    queueLevel[idx] = 2;   // stays in Q2
                }
                cur.state = ProcessState::READY;
                q[queueLevel[idx]].push_back(idx);
            }
        }

        if (onTick) onTick(time, procs, gantt, log);
        result.processes = procs;
        result.gantt     = gantt;
        result.log       = log;
        finalizeResult(result);
        return result;
    }
};

std::unique_ptr<Scheduler> makeMLFQ() {
    return std::make_unique<MLFQScheduler>();
}
