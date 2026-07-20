// ================================================================
//  Priority.cpp — Priority Scheduling (Non-Preemptive) with Aging
//
//  ALGORITHM:
//    Lower priority NUMBER = higher urgency (priority 1 runs before priority 5).
//    At each scheduling point, pick the highest-priority arrived process.
//    Run it to completion (non-preemptive).
//
//  PROBLEM: Low-priority processes can wait forever if high-priority
//           ones keep arriving. This is called "starvation."
//
//  FIX — AGING:
//    Every AGING_INTERVAL ticks, a waiting process's priority number
//    decreases by 1 (making it more urgent), until it eventually wins.
//
//  DSA USED:
//    - Class with Inheritance  (PriorityScheduler extends Scheduler)
//    - vector<Process>         (all processes)
//    - vector<int>             (shadow array for effective priorities)
//    - vector<GanttEntry>      (Gantt chart)
//    - vector<SchedulingLog>   (decision log)
//    - Linear Search           (to find highest effective priority)
// ================================================================
#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>
#include <vector>

// How many ticks a process must wait before getting a priority boost
static const int AGING_INTERVAL = 4;

class PriorityScheduler : public Scheduler {
public:
    PriorityScheduler() : Scheduler("Priority (Non-Preemptive + Aging)") {}

    SimulationResult run(
        std::vector<Process> procs,
        const AppSettings&   settings,
        TickCallback         onTick) override
    {
        SimulationResult result;
        result.algorithmName = algorithmName_;
        for (int i = 0; i < (int)procs.size(); i++) procs[i].resetForSimulation();

        std::vector<GanttEntry>    gantt;
        std::vector<SchedulingLog> log;

        int n         = (int)procs.size();
        int time      = 0;
        int completed = 0;

        // DSA: vector<int> — shadow array for effective priorities
        // We never modify the real Process::priority so the original values
        // are still shown in the final results table.
        std::vector<int> effPriority(n);
        std::vector<int> waitingSince(n, -1);  // tick when each process entered READY

        for (int i = 0; i < n; i++) effPriority[i] = procs[i].priority;

        while (completed < n) {

            // Step 1: Mark newly arrived processes as READY
            for (int i = 0; i < n; i++) {
                if (procs[i].arrivalTime <= time && procs[i].state == ProcessState::NEW) {
                    procs[i].state  = ProcessState::READY;
                    waitingSince[i] = time;
                }
            }

            // Step 2: Aging — boost priority of processes that have waited too long
            for (int i = 0; i < n; i++) {
                if (procs[i].state != ProcessState::READY) continue;

                int waitedFor = time - waitingSince[i];
                bool timeToBoost = (waitedFor > 0) && (waitedFor % AGING_INTERVAL == 0);

                if (timeToBoost) {
                    int before        = effPriority[i];
                    effPriority[i]    = std::max(1, effPriority[i] - 1);  // lower number = more urgent
                    bool actuallyBoosted = effPriority[i] < before;

                    if (actuallyBoosted)
                        log.push_back({ time, procs[i].pid,
                            "Aged: priority " + std::to_string(before) +
                            " -> " + std::to_string(effPriority[i]) });
                }
            }

            // Step 3: DSA: Linear Search — pick highest effective priority (lowest number)
            int bestIdx   = -1;
            int bestPri   = INT_MAX;
            int bestArriv = INT_MAX;

            for (int i = 0; i < n; i++) {
                if (procs[i].state != ProcessState::READY) continue;

                bool higherPriority = effPriority[i] < bestPri;
                bool samePriority   = effPriority[i] == bestPri;
                bool earlierArrival = (bestIdx != -1) && procs[i].arrivalTime < bestArriv;

                if (higherPriority || (samePriority && earlierArrival)) {
                    bestPri   = effPriority[i];
                    bestArriv = procs[i].arrivalTime;
                    bestIdx   = i;
                }
            }

            // CPU idle — jump to next arrival
            if (bestIdx == -1) {
                int nextArrival = INT_MAX;
                for (int i = 0; i < n; i++)
                    if (procs[i].state == ProcessState::NEW)
                        nextArrival = std::min(nextArrival, procs[i].arrivalTime);
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

            // Step 4: Run chosen process to completion (non-preemptive)
            Process& current  = procs[bestIdx];
            current.state     = ProcessState::RUNNING;
            current.startTime = time;
            log.push_back({ time, current.pid,
                "Effective priority = " + std::to_string(effPriority[bestIdx]) +
                " (original: " + std::to_string(current.priority) + ")" });

            int startT = time;
            while (current.remainingTime > 0) {
                // Mark processes arriving during this burst as READY
                for (int i = 0; i < n; i++) {
                    if (procs[i].arrivalTime <= time && procs[i].state == ProcessState::NEW) {
                        procs[i].state  = ProcessState::READY;
                        waitingSince[i] = time;
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
