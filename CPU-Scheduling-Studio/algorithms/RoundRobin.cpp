// ================================================================
//  RoundRobin.cpp — Round Robin Scheduling (Preemptive)
//
//  ALGORITHM:
//    Give each process a fixed time slice called a "quantum".
//    Processes take turns in a circular queue.
//    If a process doesn't finish in its quantum, it goes back
//    to the end of the queue — no one holds the CPU too long.
//
//  DSA USED:
//    - Class with Inheritance  (RoundRobinScheduler extends Scheduler)
//    - vector<Process>         (all processes)
//    - queue<int>              (FIFO ready queue — process indices)
//    - vector<GanttEntry>      (Gantt chart)
//    - vector<SchedulingLog>   (decision log)
//    - stable_sort()           (sort by arrival to track new arrivals)
// ================================================================
#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>
#include <queue>    // DSA: queue (FIFO) for the ready queue

class RoundRobinScheduler : public Scheduler {
public:
    RoundRobinScheduler() : Scheduler("Round Robin") {}

    SimulationResult run(
        std::vector<Process> procs,
        const AppSettings&   settings,
        TickCallback         onTick) override
    {
        SimulationResult result;
        result.algorithmName = algorithmName_
            + " (Q=" + std::to_string(settings.timeQuantum) + ")";
        for (int i = 0; i < (int)procs.size(); i++) procs[i].resetForSimulation();

        std::vector<GanttEntry>    gantt;
        std::vector<SchedulingLog> log;

        int n         = (int)procs.size();
        int time      = 0;
        int completed = 0;
        int quantum   = settings.timeQuantum;

        // Sort process indices by arrival time — used to add arrivals to the queue
        std::vector<int> orderByArrival(n);
        for (int i = 0; i < n; i++) orderByArrival[i] = i;
        std::stable_sort(orderByArrival.begin(), orderByArrival.end(),
            [&](int a, int b) {
                return procs[a].arrivalTime < procs[b].arrivalTime;
            });

        // DSA: queue<int> — FIFO ready queue storing process indices
        std::queue<int> readyQ;
        int nextCheck = 0;  // pointer into orderByArrival for tracking new arrivals

        // Seed: add processes that arrive at time 0
        while (nextCheck < n && procs[orderByArrival[nextCheck]].arrivalTime <= time) {
            int idx = orderByArrival[nextCheck++];
            readyQ.push(idx);
            procs[idx].state = ProcessState::READY;
        }

        while (completed < n) {
            // Queue is empty — CPU idle, jump to next arrival
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

                // Add all processes that have now arrived
                while (nextCheck < n && procs[orderByArrival[nextCheck]].arrivalTime <= time) {
                    int idx = orderByArrival[nextCheck++];
                    readyQ.push(idx);
                    procs[idx].state = ProcessState::READY;
                }
                continue;
            }

            // Dequeue the next process (FIFO)
            int idx = readyQ.front();
            readyQ.pop();

            Process& current = procs[idx];
            current.state = ProcessState::RUNNING;
            if (current.startTime == -1) current.startTime = time;

            log.push_back({ time, current.pid,
                "Round Robin turn (Q=" + std::to_string(quantum) + ")" });

            int startT    = time;
            int ticksLeft = quantum;  // how many ticks remain in this time slice

            // Run for up to quantum ticks
            while (ticksLeft > 0 && current.remainingTime > 0) {
                if (onTick) onTick(time, procs, gantt, log);
                if (settings.stepMode) pressEnter();
                else sleepMs(settings.animationSpeedMs);

                current.remainingTime--;
                time++;
                ticksLeft--;

                // Check if any process arrived during this quantum — add to queue
                while (nextCheck < n && procs[orderByArrival[nextCheck]].arrivalTime <= time) {
                    int ni = orderByArrival[nextCheck++];
                    readyQ.push(ni);
                    procs[ni].state = ProcessState::READY;
                }
            }

            gantt.push_back({ current.pid, startT, time });

            if (current.remainingTime == 0) {
                // Process finished in this quantum
                current.state          = ProcessState::TERMINATED;
                current.completionTime = time;
                completed++;
            } else {
                // Not finished — put back at end of queue (round robin)
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
