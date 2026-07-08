// ============================================================
//  Process.h
//  Defines the Process data structure and related enumerations.
//  Every scheduling algorithm operates on a vector of Process
//  objects, keeping the data model independent of the UI.
// ============================================================
#pragma once

#include <string>
#include <vector>

// ============================================================
//  Process lifecycle states (maps to the "State" column in
//  the dashboard).
// ============================================================
enum class ProcessState {
    NEW,        ///< Just created – not yet in any queue
    READY,      ///< In the ready queue, waiting for CPU
    RUNNING,    ///< Currently executing on the CPU
    WAITING,    ///< Blocked / waiting for I/O (reserved for future use)
    TERMINATED  ///< Finished execution
};

// ============================================================
//  GanttEntry – one coloured block in the Gantt chart.
// ============================================================
struct GanttEntry {
    std::string pid;    ///< "P1" … "Idle"
    int         start;  ///< Inclusive start time
    int         end;    ///< Exclusive end time
};

// ============================================================
//  SchedulingLog – one line in the decision log.
// ============================================================
struct SchedulingLog {
    int         time;     ///< Clock tick when decision was made
    std::string pid;      ///< Process selected (or "Idle")
    std::string reason;   ///< Human-readable explanation
};

// ============================================================
//  Process – central data object
//
//  Immutable fields (set at creation):
//    pid, arrivalTime, burstTime, priority
//
//  Mutable fields (updated by the scheduler):
//    remainingTime, state, completionTime, waitingTime,
//    turnaroundTime, responseTime, startTime
// ============================================================
struct Process {
    // ---- Identity ----------------------------------------
    std::string pid;          ///< Unique process identifier e.g. "P1"

    // ---- Inputs (provided by user / random generator) ----
    int arrivalTime   = 0;    ///< Time unit at which process arrives
    int burstTime     = 0;    ///< Total CPU time required
    int priority      = 0;    ///< Lower number = Higher priority (Priority scheduling)

    // ---- Runtime fields (modified during simulation) -----
    int remainingTime = 0;    ///< Burst time left
    int startTime     = -1;   ///< First time process got CPU (-1 = not started)
    int completionTime= 0;    ///< Time unit when process finished
    int waitingTime   = 0;    ///< Total time spent waiting in ready queue
    int turnaroundTime= 0;    ///< completionTime - arrivalTime
    int responseTime  = 0;    ///< startTime - arrivalTime

    // ---- State -------------------------------------------
    ProcessState state = ProcessState::NEW;

    // ---- Convenience: reset runtime fields for re-use ----
    void resetForSimulation() {
        remainingTime  = burstTime;
        startTime      = -1;
        completionTime = 0;
        waitingTime    = 0;
        turnaroundTime = 0;
        responseTime   = 0;
        state          = ProcessState::NEW;
    }

    // ---- State helper ------------------------------------
    std::string stateStr() const {
        switch (state) {
            case ProcessState::NEW:        return "NEW";
            case ProcessState::READY:      return "READY";
            case ProcessState::RUNNING:    return "RUNNING";
            case ProcessState::WAITING:    return "WAITING";
            case ProcessState::TERMINATED: return "TERMINATED";
        }
        return "UNKNOWN";
    }
};

// ============================================================
//  SimulationResult
//  Returned by every scheduling algorithm after completion.
// ============================================================
struct SimulationResult {
    std::string            algorithmName;
    std::vector<Process>   processes;       ///< Final state of each process
    std::vector<GanttEntry> gantt;          ///< Gantt chart blocks
    std::vector<SchedulingLog> log;         ///< Decision log entries

    // Aggregated statistics
    double avgWaitingTime    = 0.0;
    double avgTurnaroundTime = 0.0;
    double avgResponseTime   = 0.0;
    double cpuUtilization    = 0.0;   ///< Percentage
    double throughput        = 0.0;   ///< Processes / time unit
    double jainFairnessIndex = 0.0;  ///< Jain's Fairness Index on TAT [1/n … 1.0]
    int    contextSwitches   = 0;
    int    totalTime         = 0;     ///< Last completion time
    int    idleTime          = 0;

    // Helpers for finding extremes
    std::string longestWaitingPID;
    std::string shortestWaitingPID;
};
