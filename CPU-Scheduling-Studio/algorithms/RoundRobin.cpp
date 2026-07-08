// ============================================================
//  RoundRobin.cpp
//  Round Robin Scheduling (Preemptive, Fixed Time Quantum)
// ============================================================
#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>
#include <queue>

class RoundRobinScheduler : public Scheduler {
public:
    RoundRobinScheduler() : Scheduler("Round Robin") {}

    SimulationResult run(
        std::vector<Process> procs,
        const AppSettings&   settings,
        std::function<void(int, const std::vector<Process>&,
                           const std::vector<GanttEntry>&,
                           const std::vector<SchedulingLog>&)> onTick) override
    {
        SimulationResult result;
        result.algorithmName = algorithmName_
            + " (Q=" + std::to_string(settings.timeQuantum) + ")";
        for (auto& p : procs) p.resetForSimulation();

        std::vector<GanttEntry>    gantt;
        std::vector<SchedulingLog> log;

        int n         = (int)procs.size();
        int time      = 0;
        int completed = 0;
        int quantum   = settings.timeQuantum;

        // Order by arrival for insertion tracking
        std::vector<int> orderByArrival(n);
        std::iota(orderByArrival.begin(), orderByArrival.end(), 0);
        std::stable_sort(orderByArrival.begin(), orderByArrival.end(),
            [&](int a, int b){ return procs[a].arrivalTime < procs[b].arrivalTime; });

        std::queue<int>  readyQ;
        int nextCheck = 0;

        // Seed: processes arriving at or before time 0
        while (nextCheck < n &&
               procs[orderByArrival[nextCheck]].arrivalTime <= time) {
            int idx = orderByArrival[nextCheck++];
            readyQ.push(idx);
            procs[idx].state = ProcessState::READY;
        }

        while (completed < n) {
            // Idle if nothing ready
            if (readyQ.empty()) {
                if (nextCheck >= n) break;
                int nextArr = procs[orderByArrival[nextCheck]].arrivalTime;
                gantt.push_back({ "Idle", time, nextArr });
                for (int t = time; t < nextArr; t++) {
                    if (onTick) onTick(t, procs, gantt, log);
                    if (settings.stepMode) pressEnter();
                    else sleepMs(settings.animationSpeedMs);
                }
                time = nextArr;
                while (nextCheck < n &&
                       procs[orderByArrival[nextCheck]].arrivalTime <= time) {
                    int idx = orderByArrival[nextCheck++];
                    readyQ.push(idx);
                    procs[idx].state = ProcessState::READY;
                }
                continue;
            }

            int idx = readyQ.front(); readyQ.pop();
            Process& current = procs[idx];
            current.state = ProcessState::RUNNING;
            if (current.startTime == -1) current.startTime = time;

            log.push_back({ time, current.pid,
                "Round Robin - next in queue (Q=" + std::to_string(quantum) + ")" });

            int startT    = time;
            int ticksLeft = quantum;

            while (ticksLeft > 0 && current.remainingTime > 0) {
                if (onTick) onTick(time, procs, gantt, log);
                if (settings.stepMode) pressEnter();
                else sleepMs(settings.animationSpeedMs);

                current.remainingTime--;
                time++;
                ticksLeft--;

                // Enqueue newly arrived processes
                while (nextCheck < n &&
                       procs[orderByArrival[nextCheck]].arrivalTime <= time) {
                    int ni = orderByArrival[nextCheck++];
                    readyQ.push(ni);
                    procs[ni].state = ProcessState::READY;
                }
            }

            gantt.push_back({ current.pid, startT, time });

            if (current.remainingTime == 0) {
                current.state          = ProcessState::TERMINATED;
                current.completionTime = time;
                completed++;
            } else {
                current.state = ProcessState::READY;
                readyQ.push(idx);
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

std::unique_ptr<Scheduler> makeRoundRobin() {
    return std::make_unique<RoundRobinScheduler>();
}
