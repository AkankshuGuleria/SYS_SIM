#pragma once
#include <string>
#include <vector>

// ================================================================
//  DSA CONCEPT: enum class (named constants for process states)
//  Represents where a process is in its lifecycle.
// ================================================================
enum class ProcessState {
    NEW,        // just created, hasn't entered any queue yet
    READY,      // in the ready queue, waiting for CPU
    RUNNING,    // currently executing on the CPU
    WAITING,    // blocked (e.g. waiting for I/O) — reserved for future use
    TERMINATED  // finished execution
};

// ================================================================
//  DSA CONCEPT: struct (simple data bundle)
//  Represents one coloured block on the Gantt chart.
// ================================================================
struct GanttEntry {
    std::string pid;   // process ID, e.g. "P1" or "Idle"
    int start;         // time when this block started
    int end;           // time when this block ended
};

// ================================================================
//  DSA CONCEPT: struct
//  Represents one line in the scheduling decision log.
// ================================================================
struct SchedulingLog {
    int time;           // clock tick when the decision was made
    std::string pid;    // process selected (or "Idle")
    std::string reason; // why this process was chosen
};

// ================================================================
//  DSA CONCEPT: struct (the main data object)
//
//  Process holds everything about one process.
//  Input fields (set by user):   arrivalTime, burstTime, priority
//  Output fields (set by sim):   waitingTime, turnaroundTime, etc.
// ================================================================
struct Process {
    std::string pid;           // unique identifier, e.g. "P1"

    // --- Input fields (given by the user) ---
    int arrivalTime   = 0;     // time unit when this process arrives
    int burstTime     = 0;     // total CPU time it needs
    int priority      = 0;     // lower number = higher urgency

    // --- Runtime fields (updated tick-by-tick during simulation) ---
    int remainingTime = 0;     // burst time still left
    int startTime     = -1;    // first time it got the CPU (-1 = not yet)
    int completionTime= 0;     // time it finished
    int waitingTime   = 0;     // time spent in ready queue
    int turnaroundTime= 0;     // completionTime - arrivalTime
    int responseTime  = 0;     // startTime - arrivalTime

    ProcessState state = ProcessState::NEW;

    // --- Member function: reset before each simulation run ---
    void resetForSimulation() {
        remainingTime  = burstTime;
        startTime      = -1;
        completionTime = 0;
        waitingTime    = 0;
        turnaroundTime = 0;
        responseTime   = 0;
        state          = ProcessState::NEW;
    }

    // --- Member function: return state as a printable string ---
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

// ================================================================
//  DSA CONCEPT: struct (aggregate result object)
//  Returned by every scheduling algorithm after it finishes.
//  DSA used inside: vector (dynamic array) to hold processes,
//  Gantt entries, and log entries.
// ================================================================
struct SimulationResult {
    std::string              algorithmName;

    // DSA: vector<Process> — dynamic array of all processes
    std::vector<Process>     processes;

    // DSA: vector<GanttEntry> — dynamic array of Gantt chart blocks
    std::vector<GanttEntry>  gantt;

    // DSA: vector<SchedulingLog> — dynamic array of log entries
    std::vector<SchedulingLog> log;

    // --- Computed statistics ---
    double avgWaitingTime    = 0.0;
    double avgTurnaroundTime = 0.0;
    double avgResponseTime   = 0.0;
    double cpuUtilization    = 0.0;
    double throughput        = 0.0;
    double jainFairnessIndex = 0.0;
    int    contextSwitches   = 0;
    int    totalTime         = 0;
    int    idleTime          = 0;

    std::string longestWaitingPID;
    std::string shortestWaitingPID;
};
