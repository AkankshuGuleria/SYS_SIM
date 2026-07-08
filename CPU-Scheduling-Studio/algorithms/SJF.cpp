// ============================================================
//  SJF.cpp
//  Shortest Job First - Non-Preemptive
// ============================================================
#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>

class SJFScheduler : public Scheduler {
public:
    SJFScheduler() : Scheduler("SJF (Shortest Job First - Non-Preemptive)") {}

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

        for (auto& p : procs) p.state = ProcessState::NEW;

        while (completed < n) {
            // Find arrived process with shortest burst
            int bestIdx   = -1;
            int bestBurst = INT_MAX;
            for (int i = 0; i < n; i++) {
                auto& p = procs[i];
                if (p.arrivalTime <= time && p.state != ProcessState::TERMINATED) {
                    if (p.burstTime < bestBurst ||
                        (p.burstTime == bestBurst && bestIdx != -1 &&
                         p.arrivalTime < procs[bestIdx].arrivalTime)) {
                        bestBurst = p.burstTime;
                        bestIdx   = i;
                    }
                }
            }

            // CPU idle
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

            // Mark arrived processes READY
            for (int i = 0; i < n; i++) {
                if (i != bestIdx && procs[i].arrivalTime <= time &&
                    procs[i].state == ProcessState::NEW)
                    procs[i].state = ProcessState::READY;
            }

            Process& current  = procs[bestIdx];
            current.state     = ProcessState::RUNNING;
            current.startTime = time;

            log.push_back({ time, current.pid,
                "Shortest Burst Time = " + std::to_string(current.burstTime) });

            int startT = time;
            while (current.remainingTime > 0) {
                for (int i = 0; i < n; i++) {
                    if (i != bestIdx && procs[i].arrivalTime <= time &&
                        procs[i].state == ProcessState::NEW)
                        procs[i].state = ProcessState::READY;
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

std::unique_ptr<Scheduler> makeSJF() {
    return std::make_unique<SJFScheduler>();
}
