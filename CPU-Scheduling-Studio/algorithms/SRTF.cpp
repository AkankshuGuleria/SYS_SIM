// ============================================================
//  SRTF.cpp
//  Shortest Remaining Time First - Preemptive SJF
// ============================================================
#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>

class SRTFScheduler : public Scheduler {
public:
    SRTFScheduler() : Scheduler("SRTF (Shortest Remaining Time First - Preemptive)") {}

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

        int n         = (int)procs.size();
        int time      = 0;
        int completed = 0;

        // Jump to first arrival
        int minArrival = INT_MAX;
        for (auto& p : procs) minArrival = std::min(minArrival, p.arrivalTime);
        time = minArrival;

        int prevIdx    = -1;
        int blockStart = time;

        while (completed < n) {
            // Pick ready process with minimum remaining time
            int bestIdx = -1;
            int bestRem = INT_MAX;
            for (int i = 0; i < n; i++) {
                auto& p = procs[i];
                if (p.arrivalTime <= time &&
                    p.state != ProcessState::TERMINATED &&
                    p.remainingTime > 0) {
                    if (p.remainingTime < bestRem ||
                        (p.remainingTime == bestRem && bestIdx != -1 &&
                         p.arrivalTime < procs[bestIdx].arrivalTime)) {
                        bestRem = p.remainingTime;
                        bestIdx = i;
                    }
                }
            }

            // CPU idle
            if (bestIdx == -1) {
                if (prevIdx != -1) {
                    gantt.push_back({ procs[prevIdx].pid, blockStart, time });
                    prevIdx    = -1;
                    blockStart = time;
                }
                int nextArrival = INT_MAX;
                for (auto& p : procs)
                    if (p.state != ProcessState::TERMINATED && p.arrivalTime > time)
                        nextArrival = std::min(nextArrival, p.arrivalTime);
                if (nextArrival == INT_MAX) break;

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
            if (current.startTime == -1) current.startTime = time;

            // Detect context switch / new block
            if (bestIdx != prevIdx) {
                if (prevIdx != -1)
                    gantt.push_back({ procs[prevIdx].pid, blockStart, time });
                else if (blockStart < time)
                    gantt.push_back({ "Idle", blockStart, time });
                blockStart = time;
                prevIdx    = bestIdx;

                std::string reason = "Shortest Remaining Time = "
                    + std::to_string(current.remainingTime);
                log.push_back({ time, current.pid, reason });
            }

            // Update states
            for (int i = 0; i < n; i++) {
                auto& p = procs[i];
                if (p.arrivalTime <= time && p.state == ProcessState::NEW &&
                    p.remainingTime > 0)
                    p.state = ProcessState::READY;
            }
            current.state = ProcessState::RUNNING;

            if (onTick) onTick(time, procs, gantt, log);
            if (settings.stepMode) pressEnter();
            else sleepMs(settings.animationSpeedMs);

            current.remainingTime--;
            time++;

            if (current.remainingTime == 0) {
                current.state          = ProcessState::TERMINATED;
                current.completionTime = time;
                gantt.push_back({ current.pid, blockStart, time });
                blockStart = time;
                prevIdx    = -1;
                completed++;
            } else if (current.state == ProcessState::RUNNING) {
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
