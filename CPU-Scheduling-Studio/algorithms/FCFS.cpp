// ============================================================
//  FCFS.cpp
//  First Come First Serve Scheduling (Non-Preemptive)
// ============================================================
#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>

class FCFSScheduler : public Scheduler {
public:
    FCFSScheduler() : Scheduler("FCFS (First Come First Serve)") {}

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

        // Sort by arrival time (FCFS order)
        std::stable_sort(procs.begin(), procs.end(),
            [](const Process& a, const Process& b){
                return a.arrivalTime < b.arrivalTime;
            });

        std::vector<GanttEntry>    gantt;
        std::vector<SchedulingLog> log;

        int time    = 0;
        int procIdx = 0;
        int n       = (int)procs.size();

        while (procIdx < n) {
            const Process& next = procs[procIdx];

            // CPU idle gap: no process has arrived yet
            if (time < next.arrivalTime) {
                GanttEntry idle{ "Idle", time, next.arrivalTime };
                gantt.push_back(idle);

                for (int t = time; t < next.arrivalTime; t++) {
                    if (onTick) onTick(t, procs, gantt, log);
                    if (settings.stepMode) pressEnter();
                    else sleepMs(settings.animationSpeedMs);
                }
                time = next.arrivalTime;
            }

            // Select the front process
            Process& current  = procs[procIdx];
            current.state     = ProcessState::RUNNING;
            current.startTime = time;

            log.push_back({ time, current.pid,
                "Arrived earliest (AT=" + std::to_string(current.arrivalTime) + ")" });

            // Mark other arrived processes READY
            for (int j = procIdx + 1; j < n; j++)
                if (procs[j].arrivalTime <= time && procs[j].state == ProcessState::NEW)
                    procs[j].state = ProcessState::READY;

            int startT = time;
            while (current.remainingTime > 0) {
                if (onTick) onTick(time, procs, gantt, log);
                if (settings.stepMode) pressEnter();
                else sleepMs(settings.animationSpeedMs);

                current.remainingTime--;
                time++;
            }

            current.state          = ProcessState::TERMINATED;
            current.completionTime = time;
            gantt.push_back({ current.pid, startT, time });
            procIdx++;
        }

        if (onTick) onTick(time, procs, gantt, log);
        result.processes = procs;
        result.gantt     = gantt;
        result.log       = log;
        finalizeResult(result);
        return result;
    }
};

std::unique_ptr<Scheduler> makeFCFS() {
    return std::make_unique<FCFSScheduler>();
}
