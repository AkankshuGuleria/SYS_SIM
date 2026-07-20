#pragma once
#include <vector>
#include <string>
#include <functional>
#include "Process.h"

// ================================================================
//  DSA CONCEPT: struct (Plain data bundle — like a record/node)
//  Stores user-configurable simulation settings.
// ================================================================
struct AppSettings {
    int  animationSpeedMs = 500;  // ms delay per clock tick
    bool darkTheme        = true;
    bool colorsEnabled    = true;
    bool stepMode         = false; // step-by-step mode (waits for Enter)
    int  timeQuantum      = 2;    // time slice for Round Robin / MLFQ
};

// ================================================================
//  DSA CONCEPT: typedef (gives a complex type a short readable name)
//
//  TickCallback is a function pointer type.
//  Every algorithm calls this after each clock tick so the
//  Dashboard can redraw the live screen.
//
//  Parameters passed to it each tick:
//    int                      — current simulation time
//    vector<Process>          — snapshot of all processes
//    vector<GanttEntry>       — Gantt chart so far
//    vector<SchedulingLog>    — decision log so far
// ================================================================
typedef std::function<void(
    int,
    const std::vector<Process>&,
    const std::vector<GanttEntry>&,
    const std::vector<SchedulingLog>&
)> TickCallback;

// ================================================================
//  DSA CONCEPT: class with Inheritance (OOP)
//
//  Scheduler is an ABSTRACT BASE CLASS.
//  All 7 scheduling algorithms inherit from it and override run().
//  This is the "polymorphism" pattern — one pointer type can hold
//  any algorithm at runtime.
// ================================================================
class Scheduler {
public:
    // Constructor: stores the algorithm name
    explicit Scheduler(const std::string& name) : algorithmName_(name) {}

    // Virtual destructor — required when using inheritance
    virtual ~Scheduler() = default;

    // Pure virtual function — every subclass MUST provide its own run()
    virtual SimulationResult run(
        std::vector<Process> procs,
        const AppSettings&   settings,
        TickCallback         onTick = nullptr) = 0;

    // Getter for the algorithm name
    const std::string& name() const { return algorithmName_; }

protected:
    std::string algorithmName_;  // e.g. "FCFS", "SJF", etc.

    // Shared helper: compute WT, TAT, RT for each process + aggregate stats
    void finalizeResult(SimulationResult& result);

    // Count CPU switches between different processes
    int countContextSwitches(const std::vector<GanttEntry>& gantt);

    // Sum durations of all "Idle" Gantt blocks
    int computeIdleTime(const std::vector<GanttEntry>& gantt);
};
