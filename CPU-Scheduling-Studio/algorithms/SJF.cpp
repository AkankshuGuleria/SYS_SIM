// ================================================================
//  SJF.cpp — Shortest Job First (Non-Preemptive)
//
//  ALGORITHM:
//    At each scheduling point, look at all arrived processes.
//    Pick the one with the smallest burst time.
//    Run it to completion before picking the next.
//
//  DSA USED:
//    - Class with Inheritance  (SJFScheduler extends Scheduler)
//    - vector<Process>         (all processes)
//    - vector<GanttEntry>      (Gantt chart)
//    - vector<SchedulingLog>   (decision log)
//    - Linear search           (to find minimum burst time each turn)
// ================================================================
#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>

class SJFScheduler : public Scheduler {
public:
    SJFScheduler() : Scheduler("SJF (Shortest Job First - Non-Preemptive)") {}

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
        int completed = 0;  // how many processes have finished

        while (completed < n) {
            // DSA: Linear Search — find arrived process with smallest burst time
            int bestIdx   = -1;   // index of chosen process (-1 = none found yet)
            int bestBurst = INT_MAX;

            for (int i = 0; i < n; i++) {
                Process& p = procs[i];
                bool hasArrived    = p.arrivalTime <= time;
                bool notDone       = p.state != ProcessState::TERMINATED;

                if (hasArrived && notDone) {
                    bool isShorter  = p.burstTime < bestBurst;
                    bool isTieSooner = (p.burstTime == bestBurst) && bestIdx != -1
                                       && p.arrivalTime < procs[bestIdx].arrivalTime;

                    if (isShorter || isTieSooner) {
                        bestBurst = p.burstTime;
                        bestIdx   = i;
                    }
                }
            }

            // No process ready — CPU is idle, jump to next arrival
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

            // Mark all arrived processes as READY
            for (int i = 0; i < n; i++) {
                if (i != bestIdx && procs[i].arrivalTime <= time
                    && procs[i].state == ProcessState::NEW)
                    procs[i].state = ProcessState::READY;
            }

            // Run the chosen process to completion
            Process& current  = procs[bestIdx];
            current.state     = ProcessState::RUNNING;
            current.startTime = time;
            log.push_back({ time, current.pid,
                "Shortest Burst = " + std::to_string(current.burstTime) });

            int startT = time;
            while (current.remainingTime > 0) {
                // Mark newly arrived processes READY during the burst
                for (int i = 0; i < n; i++) {
                    if (i != bestIdx && procs[i].arrivalTime <= time
                        && procs[i].state == ProcessState::NEW)
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
