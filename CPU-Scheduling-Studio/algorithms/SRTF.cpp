// ================================================================
//  SRTF.cpp — Shortest Remaining Time First (Preemptive SJF)
//
//  ALGORITHM:
//    Every clock tick, look at all arrived processes.
//    Pick the one with the LEAST remaining burst time.
//    If a shorter job arrives while another is running, preempt it.
//    This is essentially SJF but re-evaluated every tick.
//
//  DSA USED:
//    - Class with Inheritance  (SRTFScheduler extends Scheduler)
//    - vector<Process>         (all processes)
//    - vector<GanttEntry>      (Gantt chart, merged into blocks)
//    - vector<SchedulingLog>   (decision log)
//    - Linear Search           (to find min remaining time each tick)
//    - Two-pointer tracking    (prevIdx + blockStart to merge Gantt blocks)
// ================================================================
#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>

class SRTFScheduler : public Scheduler {
public:
    SRTFScheduler() : Scheduler("SRTF (Shortest Remaining Time First - Preemptive)") {}

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

        // Start simulation at the earliest arrival time
        int minArrival = INT_MAX;
        for (int i = 0; i < n; i++) minArrival = std::min(minArrival, procs[i].arrivalTime);
        int time = minArrival;

        // Track current Gantt block: who ran last and when the block started
        int prevIdx    = -1;  // index of process in previous tick (-1 = none)
        int blockStart = time;

        while (completed < n) {
            // DSA: Linear Search — find arrived process with smallest remaining time
            int bestIdx = -1;
            int bestRem = INT_MAX;

            for (int i = 0; i < n; i++) {
                Process& p = procs[i];
                bool hasArrived = p.arrivalTime <= time;
                bool notDone    = p.state != ProcessState::TERMINATED;
                bool hasWork    = p.remainingTime > 0;

                if (hasArrived && notDone && hasWork) {
                    bool isShorter   = p.remainingTime < bestRem;
                    bool isTieSooner = (p.remainingTime == bestRem) && bestIdx != -1
                                       && p.arrivalTime < procs[bestIdx].arrivalTime;

                    if (isShorter || isTieSooner) {
                        bestRem = p.remainingTime;
                        bestIdx = i;
                    }
                }
            }

            // CPU idle — advance one tick, extend or create an Idle Gantt block
            if (bestIdx == -1) {
                // Close any open process block first
                if (prevIdx != -1) {
                    gantt.push_back({ procs[prevIdx].pid, blockStart, time });
                    prevIdx    = -1;
                    blockStart = time;
                }
                // Extend or create Idle block
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
            if (current.startTime == -1) current.startTime = time;  // first time on CPU

            // Detect context switch: process changed since last tick
            bool contextSwitch = (bestIdx != prevIdx);
            if (contextSwitch) {
                // Close previous block
                if (prevIdx != -1)
                    gantt.push_back({ procs[prevIdx].pid, blockStart, time });
                else if (blockStart < time)
                    gantt.push_back({ "Idle", blockStart, time });

                // Start new block
                blockStart = time;
                prevIdx    = bestIdx;
                log.push_back({ time, current.pid,
                    "Shortest Remaining = " + std::to_string(current.remainingTime) });
            }

            // Update all arrived processes to READY state
            for (int i = 0; i < n; i++) {
                if (procs[i].arrivalTime <= time
                    && procs[i].state == ProcessState::NEW
                    && procs[i].remainingTime > 0)
                    procs[i].state = ProcessState::READY;
            }
            current.state = ProcessState::RUNNING;

            if (onTick) onTick(time, procs, gantt, log);
            if (settings.stepMode) pressEnter();
            else sleepMs(settings.animationSpeedMs);

            // Advance one tick
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
            } else if (current.state == ProcessState::RUNNING) {
                // Still has work, mark back as READY for re-evaluation next tick
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

std::unique_ptr<Scheduler> makeSRTF() {
    return std::make_unique<SRTFScheduler>();
}
