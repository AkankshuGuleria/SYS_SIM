// ================================================================
//  PriorityPreemptive.cpp — Priority Scheduling (Preemptive)
//
//  ALGORITHM:
//    Lower priority NUMBER = higher urgency (same as non-preemptive).
//    Evaluated every single clock tick.
//    If a higher-priority process arrives while one is running,
//    the running process is PREEMPTED (paused) immediately.
//
//  DIFFERENCE from Non-Preemptive:
//    Non-Preemptive: once started, a process runs to completion.
//    Preemptive:     the CPU can be taken away at any tick.
//
//  DSA USED:
//    - Class with Inheritance  (PriorityPreemptiveScheduler extends Scheduler)
//    - vector<Process>         (all processes)
//    - vector<GanttEntry>      (Gantt chart, blocks merged per run)
//    - vector<SchedulingLog>   (decision log)
//    - Linear Search           (to find highest priority each tick)
//    - Two-variable tracking   (prevIdx + blockStart for Gantt merging)
// ================================================================
#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>

class PriorityPreemptiveScheduler : public Scheduler {
public:
    PriorityPreemptiveScheduler()
        : Scheduler("Priority Preemptive (Highest Priority First)") {}

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
        int completed = 0;

        // Start at the first arrival time
        int minArr = INT_MAX;
        for (int i = 0; i < n; i++) minArr = std::min(minArr, procs[i].arrivalTime);
        int time = minArr;

        // Track the current Gantt block
        int prevIdx    = -1;  // process index that ran last tick (-1 = none/idle)
        int blockStart = time;

        while (completed < n) {
            // Step 1: DSA: Linear Search — find highest-priority arrived process
            int bestIdx = -1;
            int bestPri = INT_MAX;

            for (int i = 0; i < n; i++) {
                Process& p = procs[i];
                bool hasArrived = p.arrivalTime <= time;
                bool notDone    = p.state != ProcessState::TERMINATED;
                bool hasWork    = p.remainingTime > 0;

                if (hasArrived && notDone && hasWork) {
                    bool higherPriority = p.priority < bestPri;
                    bool samePriSooner  = (p.priority == bestPri) && bestIdx != -1
                                          && p.arrivalTime < procs[bestIdx].arrivalTime;

                    if (higherPriority || samePriSooner) {
                        bestPri = p.priority;
                        bestIdx = i;
                    }
                }
            }

            // Step 2: CPU idle — no process has arrived yet
            if (bestIdx == -1) {
                // Close any open process block
                if (prevIdx != -1) {
                    gantt.push_back({ procs[prevIdx].pid, blockStart, time });
                    prevIdx    = -1;
                    blockStart = time;
                }
                // Extend or open an Idle block
                if (gantt.empty() || gantt.back().pid != "Idle")
                    gantt.push_back({ "Idle", time, time + 1 });
                else
                    gantt.back().end = time + 1;

                if (onTick) onTick(time, procs, gantt, log);
                if (settings.stepMode) pressEnter();
                else sleepMs(settings.animationSpeedMs);

                time++;
                blockStart = time;
                continue;
            }

            Process& current = procs[bestIdx];
            if (current.startTime == -1) current.startTime = time;  // first CPU access

            // Step 3: Detect context switch (process changed since last tick)
            bool contextSwitch = (bestIdx != prevIdx);
            if (contextSwitch) {
                // Close the previous Gantt block
                if (prevIdx != -1)
                    gantt.push_back({ procs[prevIdx].pid, blockStart, time });
                else if (blockStart < time)
                    gantt.push_back({ "Idle", blockStart, time });

                // Open a new block for the current process
                blockStart = time;
                prevIdx    = bestIdx;
                log.push_back({ time, current.pid,
                    "Higher Priority = " + std::to_string(current.priority) });
            }

            // Step 4: Update all arrived processes to READY
            for (int i = 0; i < n; i++) {
                if (procs[i].arrivalTime <= time && procs[i].state == ProcessState::NEW)
                    procs[i].state = ProcessState::READY;
            }
            current.state = ProcessState::RUNNING;

            if (onTick) onTick(time, procs, gantt, log);
            if (settings.stepMode) pressEnter();
            else sleepMs(settings.animationSpeedMs);

            // Step 5: Execute one tick
            current.remainingTime--;
            time++;

            if (current.remainingTime == 0) {
                // Process finished — close its Gantt block
                current.state          = ProcessState::TERMINATED;
                current.completionTime = time;
                gantt.push_back({ current.pid, blockStart, time });
                blockStart = time;
                prevIdx    = -1;
                completed++;
            } else {
                // Process was preempted or will be re-evaluated next tick
                current.state = ProcessState::READY;
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

std::unique_ptr<Scheduler> makePriorityPreemptive() {
    return std::make_unique<PriorityPreemptiveScheduler>();
}
