// Priority.cpp - Priority Scheduling, Non-Preemptive, with Aging
//
// Why aging?  The baseline non-preemptive priority algorithm is
// textbook-correct but has a well-known pathology: if high-priority
// processes keep arriving, a low-priority process can wait forever.
// The classic fix is aging: every `AGING_INTERVAL` idle ticks a
// waiting process's effective priority is incremented by one level
// so that it eventually wins the CPU regardless of new arrivals.
//
// Implementation detail: we shadow the real priority in a local
// `effPriority[]` array so the Process struct is never mutated and
// the original values appear unchanged in the final result table.
// The scheduling log records each aging event explicitly so the
// decision log shows *why* the priority changed.

#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>
#include <vector>

// How many ticks a process must wait before its effective priority
// improves by one level.  Four ticks keeps aging snappy on small
// test cases; you can make this configurable if needed.
static constexpr int AGING_INTERVAL = 4;

class PriorityScheduler : public Scheduler {
public:
    PriorityScheduler() : Scheduler("Priority (Non-Preemptive + Aging)") {}

    SimulationResult run(
        std::vector<Process> procs,
        const AppSettings&   settings,
        std::function<void(int, const std::vector<Process>&,
                           const std::vector<GanttEntry>&,
                           const std::vector<SchedulingLog>&)> onTick) override
    {
        SimulationResult result;
        result.algorithmName = algorithmName_;
        for (auto& p : procs) p.resetForSimulation();

        std::vector<GanttEntry>    gantt;
        std::vector<SchedulingLog> log;

        const int n   = (int)procs.size();
        int time      = 0;
        int completed = 0;

        // Effective priorities (lower = more urgent).  We start at
        // the user-supplied value and can only decrease (improve).
        std::vector<int> effPriority(n);
        std::vector<int> waitingSince(n, -1);   // tick when process entered READY
        for (int i = 0; i < n; i++) effPriority[i] = procs[i].priority;

        while (completed < n) {

            // --- Determine which processes are currently ready ---
            for (int i = 0; i < n; i++) {
                auto& p = procs[i];
                if (p.arrivalTime <= time && p.state == ProcessState::NEW) {
                    p.state        = ProcessState::READY;
                    waitingSince[i] = time;
                }
            }

            // --- Aging pass: boost effective priority of waiting procs ---
            for (int i = 0; i < n; i++) {
                if (procs[i].state != ProcessState::READY) continue;
                int waited = time - waitingSince[i];
                if (waited > 0 && waited % AGING_INTERVAL == 0) {
                    int before = effPriority[i];
                    effPriority[i] = std::max(1, effPriority[i] - 1);
                    if (effPriority[i] < before) {
                        log.push_back({ time, procs[i].pid,
                            "Aged: effective priority " + std::to_string(before)
                            + " -> " + std::to_string(effPriority[i])
                            + " (waited " + std::to_string(waited) + " ticks)" });
                    }
                }
            }

            // --- Pick highest-priority ready process (lowest number wins) ---
            int bestIdx   = -1;
            int bestPri   = INT_MAX;
            int bestArriv = INT_MAX;

            for (int i = 0; i < n; i++) {
                if (procs[i].state != ProcessState::READY) continue;
                if (effPriority[i] < bestPri ||
                    (effPriority[i] == bestPri &&
                     procs[i].arrivalTime < bestArriv))
                {
                    bestPri   = effPriority[i];
                    bestArriv = procs[i].arrivalTime;
                    bestIdx   = i;
                }
            }

            // --- CPU idle: jump forward to next arrival ---
            if (bestIdx == -1) {
                int nextArrival = INT_MAX;
                for (auto& p : procs)
                    if (p.state == ProcessState::NEW)
                        nextArrival = std::min(nextArrival, p.arrivalTime);
                if (nextArrival == INT_MAX) break;

                gantt.push_back({ "Idle", time, nextArrival });
                for (int t = time; t < nextArrival; t++) {
                    if (onTick) onTick(t, procs, gantt, log);
                    if (settings.stepMode) pressEnter();
                    else sleepMs(settings.animationSpeedMs);
                }
                time = nextArrival;
                continue;
            }

            // --- Run to completion (non-preemptive) ---
            Process& current  = procs[bestIdx];
            current.state     = ProcessState::RUNNING;
            current.startTime = time;

            log.push_back({ time, current.pid,
                "Highest effective priority = " +
                std::to_string(effPriority[bestIdx]) +
                " (original: " + std::to_string(current.priority) + ")"});

            int startT = time;
            while (current.remainingTime > 0) {
                // Newly arrived processes become READY during the burst
                for (int i = 0; i < n; i++) {
                    auto& p = procs[i];
                    if (p.arrivalTime <= time && p.state == ProcessState::NEW) {
                        p.state         = ProcessState::READY;
                        waitingSince[i]  = time;
                    }
                }

                if (onTick) onTick(time, procs, gantt, log);
                if (settings.stepMode) pressEnter();
                else sleepMs(settings.animationSpeedMs);

                current.remainingTime--;
                time++;
            }

            current.state          = ProcessState::TERMINATED;
            current.completionTime = time;
            gantt.push_back({ current.pid, startT, time });
            completed++;
        }

        if (onTick) onTick(time, procs, gantt, log);
        result.processes = procs;
        result.gantt     = gantt;
        result.log       = log;
        finalizeResult(result);
        return result;
    }
};

std::unique_ptr<Scheduler> makePriority() {
    return std::make_unique<PriorityScheduler>();
}
