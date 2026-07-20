// ================================================================
//  MLFQ.cpp — Multi-Level Feedback Queue (3 Levels)
//
//  ALGORITHM:
//    Three queues, each with different priority and time quantum:
//      Q0 (highest priority): quantum = timeQuantum          (e.g. 2)
//      Q1 (medium priority):  quantum = timeQuantum * 2      (e.g. 4)
//      Q2 (lowest priority):  runs to completion (FCFS)
//
//    Every new process enters Q0.
//    DEMOTION: if a process exhausts its quantum without finishing,
//              it drops one level (Q0->Q1->Q2).
//    PROMOTION (Aging): processes waiting in Q1/Q2 gain age ticks.
//              After agingThresh ticks, they are promoted one level.
//              This prevents starvation.
//
//  DSA USED:
//    - Class with Inheritance  (MLFQScheduler extends Scheduler)
//    - vector<Process>         (all processes)
//    - deque<int>[3]           (three FIFO queues holding process indices)
//    - vector<int>             (queueLevel: which queue each process is in)
//    - vector<int>             (ageTicks: ticks spent waiting in Q1/Q2)
//    - vector<GanttEntry>      (Gantt chart)
//    - vector<SchedulingLog>   (decision log)
// ================================================================
#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>
#include <deque>    // DSA: deque — double-ended queue (used as FIFO here)

class MLFQScheduler : public Scheduler {
public:
    MLFQScheduler() : Scheduler("MLFQ (3-level Feedback + Aging)") {}

    SimulationResult run(
        std::vector<Process> procs,
        const AppSettings&   settings,
        TickCallback         onTick) override
    {
        SimulationResult result;
        result.algorithmName = algorithmName_
            + " [Q=" + std::to_string(settings.timeQuantum) + "]";
        for (int i = 0; i < (int)procs.size(); i++) procs[i].resetForSimulation();

        std::vector<GanttEntry>    gantt;
        std::vector<SchedulingLog> log;

        int n             = (int)procs.size();
        int time          = 0;
        int completed     = 0;
        int Q0            = settings.timeQuantum;       // quantum for level 0
        int Q1            = settings.timeQuantum * 2;   // quantum for level 1
        int agingThresh   = settings.timeQuantum * 4;   // ticks before promotion

        // DSA: vector<int> — which queue (level) each process is currently in
        std::vector<int> queueLevel(n, 0);

        // DSA: vector<int> — how many ticks each process has waited in Q1/Q2
        std::vector<int> ageTicks(n, 0);

        // DSA: deque<int>[3] — three separate FIFO ready queues
        // Each stores indices into procs[]
        std::deque<int> q[3];

        // Sort process indices by arrival time
        std::vector<int> byArrival(n);
        for (int i = 0; i < n; i++) byArrival[i] = i;
        std::stable_sort(byArrival.begin(), byArrival.end(),
            [&](int a, int b) { return procs[a].arrivalTime < procs[b].arrivalTime; });
        int nextCheck = 0;

        // Helper: add all newly arrived processes to Q0
        auto enqueueArrivals = [&]() {
            while (nextCheck < n && procs[byArrival[nextCheck]].arrivalTime <= time) {
                int idx = byArrival[nextCheck++];
                procs[idx].state = ProcessState::READY;
                q[0].push_back(idx);  // all new processes start at Q0 (highest priority)
            }
        };

        // Helper: promote processes that have been waiting too long in Q1/Q2
        auto applyAging = [&]() {
            for (int level = 1; level <= 2; level++) {
                std::deque<int> kept;  // processes that stay at this level

                for (int idx : q[level]) {
                    ageTicks[idx]++;

                    if (ageTicks[idx] >= agingThresh) {
                        // Promote one level
                        ageTicks[idx]   = 0;
                        queueLevel[idx] = level - 1;
                        q[level - 1].push_back(idx);
                        log.push_back({ time, procs[idx].pid,
                            "Aged: promoted Q" + std::to_string(level) +
                            " -> Q" + std::to_string(level - 1) });
                    } else {
                        kept.push_back(idx);  // stays here
                    }
                }
                q[level] = kept;
            }
        };

        enqueueArrivals();  // seed with processes arriving at time 0

        while (completed < n) {
            // Find the highest-priority non-empty queue (Q0 first, then Q1, Q2)
            int chosen_q = -1;
            for (int lv = 0; lv <= 2; lv++) {
                if (!q[lv].empty()) { chosen_q = lv; break; }
            }

            // All queues empty — CPU idle, jump to next arrival
            if (chosen_q == -1) {
                if (nextCheck >= n) break;
                int nextArr = procs[byArrival[nextCheck]].arrivalTime;
                gantt.push_back({ "Idle", time, nextArr });

                for (int t = time; t < nextArr; t++) {
                    if (onTick) onTick(t, procs, gantt, log);
                    if (settings.stepMode) pressEnter();
                    else sleepMs(settings.animationSpeedMs);
                }
                time = nextArr;
                enqueueArrivals();
                continue;
            }

            // Dequeue from the chosen level (FIFO within each level)
            int idx = q[chosen_q].front();
            q[chosen_q].pop_front();

            Process& cur = procs[idx];
            cur.state = ProcessState::RUNNING;
            if (cur.startTime == -1) cur.startTime = time;
            ageTicks[idx] = 0;  // reset age counter when process gets CPU

            // Determine quantum for this level (Q2 = no limit, run to completion)
            int quantum = (chosen_q == 0) ? Q0
                        : (chosen_q == 1) ? Q1
                        :                   cur.remainingTime;  // Q2: FCFS

            log.push_back({ time, cur.pid,
                "Q" + std::to_string(chosen_q) +
                " (quantum=" + std::to_string(quantum) +
                ", remaining=" + std::to_string(cur.remainingTime) + ")" });

            int startT    = time;
            int ticksDone = 0;

            // Run for up to quantum ticks
            while (ticksDone < quantum && cur.remainingTime > 0) {
                if (onTick) onTick(time, procs, gantt, log);
                if (settings.stepMode) pressEnter();
                else sleepMs(settings.animationSpeedMs);

                cur.remainingTime--;
                time++;
                ticksDone++;

                enqueueArrivals();  // check for new arrivals each tick
                applyAging();       // update age counters for waiting processes
            }

            gantt.push_back({ cur.pid, startT, time });

            if (cur.remainingTime == 0) {
                // Finished
                cur.state          = ProcessState::TERMINATED;
                cur.completionTime = time;
                completed++;
            } else {
                // Demote: quantum exhausted, process goes one level lower
                if (chosen_q < 2) {
                    queueLevel[idx] = chosen_q + 1;
                    log.push_back({ time, cur.pid,
                        "Quantum exhausted: demoted Q" + std::to_string(chosen_q) +
                        " -> Q" + std::to_string(chosen_q + 1) });
                }
                cur.state = ProcessState::READY;
                q[queueLevel[idx]].push_back(idx);  // add to back of its queue
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

std::unique_ptr<Scheduler> makeMLFQ() {
    return std::make_unique<MLFQScheduler>();
}
