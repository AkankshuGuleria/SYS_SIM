// ============================================================
//  Scheduler.cpp
//  Base-class implementation: post-processing helpers used by
//  every scheduling algorithm after its simulation loop ends.
// ============================================================
#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>
#include <climits>

// ============================================================
//  finalizeResult
//  After the main simulation loop completes, this function:
//    1. Computes per-process WT, TAT, RT
//    2. Fills the aggregate statistics in `result`
// ============================================================
void Scheduler::finalizeResult(SimulationResult& result) {
    for (auto& p : result.processes) {
        // Turnaround = completion - arrival
        p.turnaroundTime = p.completionTime - p.arrivalTime;

        // Waiting = TAT - burst
        p.waitingTime = p.turnaroundTime - p.burstTime;
        if (p.waitingTime < 0) p.waitingTime = 0;

        // Response = first CPU access - arrival
        p.responseTime = (p.startTime >= 0)
            ? p.startTime - p.arrivalTime
            : 0;
        if (p.responseTime < 0) p.responseTime = 0;
    }

    // Determine total time from last Gantt block
    if (!result.gantt.empty())
        result.totalTime = result.gantt.back().end;

    result.idleTime        = computeIdleTime(result.gantt);
    result.contextSwitches = countContextSwitches(result.gantt);

    computeStatistics(result); // from Utilities.h
}

// ============================================================
//  countContextSwitches
//  A context switch occurs whenever the CPU switches from one
//  non-idle process to another non-idle process.
// ============================================================
int Scheduler::countContextSwitches(const std::vector<GanttEntry>& gantt) {
    int count = 0;
    std::string prev;
    for (const auto& g : gantt) {
        if (g.pid != "Idle" && !prev.empty() && g.pid != prev)
            count++;
        if (g.pid != "Idle") prev = g.pid;
    }
    return count;
}

// ============================================================
//  computeIdleTime
//  Sum durations of all "Idle" Gantt entries.
// ============================================================
int Scheduler::computeIdleTime(const std::vector<GanttEntry>& gantt) {
    int idle = 0;
    for (const auto& g : gantt)
        if (g.pid == "Idle")
            idle += g.end - g.start;
    return idle;
}
