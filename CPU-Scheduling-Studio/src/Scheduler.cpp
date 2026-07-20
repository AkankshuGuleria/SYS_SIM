// Scheduler.cpp – base class post-processing helpers
#include "../include/Scheduler.h"
#include "../include/Utilities.h"
#include <algorithm>
#include <climits>

// Compute per-process times and fill aggregate stats in result
void Scheduler::finalizeResult(SimulationResult& result) {
    for (auto& p : result.processes) {
        p.turnaroundTime = p.completionTime - p.arrivalTime;
        p.waitingTime    = std::max(0, p.turnaroundTime - p.burstTime);
        p.responseTime   = (p.startTime >= 0) ? std::max(0, p.startTime - p.arrivalTime) : 0;
    }
    if (!result.gantt.empty())
        result.totalTime = result.gantt.back().end;
    result.idleTime        = computeIdleTime(result.gantt);
    result.contextSwitches = countContextSwitches(result.gantt);
    computeStatistics(result);
}

// Count CPU switches between different non-idle processes
int Scheduler::countContextSwitches(const std::vector<GanttEntry>& gantt) {
    int count = 0;
    std::string prev;
    for (const auto& g : gantt) {
        if (g.pid != "Idle" && !prev.empty() && g.pid != prev) count++;
        if (g.pid != "Idle") prev = g.pid;
    }
    return count;
}

// Sum durations of all "Idle" Gantt entries
int Scheduler::computeIdleTime(const std::vector<GanttEntry>& gantt) {
    int idle = 0;
    for (const auto& g : gantt)
        if (g.pid == "Idle") idle += g.end - g.start;
    return idle;
}
