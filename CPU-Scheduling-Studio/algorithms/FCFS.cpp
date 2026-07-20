// ================================================================
//  FCFS.cpp — First Come First Serve (Non-Preemptive)
//
//  ALGORITHM:
//    Sort processes by arrival time.
//    Run each one fully before starting the next.
//    No interruption once a process starts.
//
//  DSA USED:
//    - Class with Inheritance  (FCFSScheduler extends Scheduler)
//    - vector<Process>         (stores all processes)
//    - vector<GanttEntry>      (timeline of who ran when)
//    - vector<SchedulingLog>   (log of each scheduling decision)
//    - stable_sort()           (sorts by arrival time, preserves order of ties)
// ================================================================
#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>

// DSA: Class Inheritance — FCFSScheduler IS-A Scheduler
class FCFSScheduler : public Scheduler {
public:
    // Constructor calls the parent Scheduler constructor with the name
    FCFSScheduler() : Scheduler("FCFS (First Come First Serve)") {}

    // Override the pure virtual run() from the base class
    SimulationResult run(
        std::vector<Process> procs,
        const AppSettings&   settings,
        TickCallback         onTick) override
    {
        SimulationResult result;
        result.algorithmName = algorithmName_;
        for (int i = 0; i < (int)procs.size(); i++) procs[i].resetForSimulation();

        // DSA: vector — store Gantt chart blocks and decision log
        std::vector<GanttEntry>    gantt;
        std::vector<SchedulingLog> log;

        int n = (int)procs.size();
        int time    = 0;
        int procIdx = 0;  // index of the next process to run

        // DSA: stable_sort — sort by arrival time (FCFS order)
        std::stable_sort(procs.begin(), procs.end(),
            [](const Process& a, const Process& b) {
                return a.arrivalTime < b.arrivalTime;
            });

        while (procIdx < n) {
            Process& nextProc = procs[procIdx];

            // If CPU has nothing to do, insert an "Idle" gap
            if (time < nextProc.arrivalTime) {
                GanttEntry idleBlock;
                idleBlock.pid   = "Idle";
                idleBlock.start = time;
                idleBlock.end   = nextProc.arrivalTime;
                gantt.push_back(idleBlock);

                // Animate idle period tick by tick
                for (int t = time; t < nextProc.arrivalTime; t++) {
                    if (onTick) onTick(t, procs, gantt, log);
                    if (settings.stepMode) pressEnter();
                    else sleepMs(settings.animationSpeedMs);
                }
                time = nextProc.arrivalTime;
            }

            // Pick next process and mark it RUNNING
            Process& current  = procs[procIdx];
            current.state     = ProcessState::RUNNING;
            current.startTime = time;

            // Log why this process was chosen
            log.push_back({ time, current.pid,
                "Arrived earliest (AT=" + std::to_string(current.arrivalTime) + ")" });

            // Mark all other already-arrived processes as READY
            for (int j = procIdx + 1; j < n; j++) {
                if (procs[j].arrivalTime <= time && procs[j].state == ProcessState::NEW)
                    procs[j].state = ProcessState::READY;
            }

            // Run process until it finishes (non-preemptive — no interruption)
            int startT = time;
            while (current.remainingTime > 0) {
                if (onTick) onTick(time, procs, gantt, log);
                if (settings.stepMode) pressEnter();
                else sleepMs(settings.animationSpeedMs);

                current.remainingTime--;
                time++;
            }

            // Mark as done and record in Gantt
            current.state          = ProcessState::TERMINATED;
            current.completionTime = time;
            gantt.push_back({ current.pid, startT, time });

            procIdx++;  // move to next process
        }

        // Final tick and pack results
        if (onTick) onTick(time, procs, gantt, log);
        result.processes = procs;
        result.gantt     = gantt;
        result.log       = log;
        finalizeResult(result);  // compute WT, TAT, RT, statistics
        return result;
    }
};

// Factory function — creates and returns an FCFSScheduler as a base pointer
std::unique_ptr<Scheduler> makeFCFS() {
    return std::make_unique<FCFSScheduler>();
}
