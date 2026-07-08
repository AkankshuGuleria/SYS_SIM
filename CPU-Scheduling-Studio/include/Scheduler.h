// ============================================================
//  Scheduler.h
//  Abstract base class for all scheduling algorithms.
//  Also declares the global Settings struct used across modules.
// ============================================================
#pragma once

#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <chrono>
#include <numeric>
#include <climits>
#include "Process.h"

// ============================================================
//  Application-wide settings
//  (modified from the Settings menu)
// ============================================================
struct AppSettings {
    int  animationSpeedMs = 500;   ///< Delay between ticks (ms)
    bool darkTheme        = true;  ///< true=dark, false=light (visual hint only)
    bool colorsEnabled    = true;  ///< Mirror of g_colorsEnabled
    bool stepMode         = false; ///< Pause and wait for Enter each tick
    int  timeQuantum      = 2;     ///< Round-Robin quantum (configurable)
};

// ============================================================
//  Scheduler – Abstract base class
//
//  Every algorithm inherits from this class and overrides
//  `run()`.  The base class provides:
//    - a reference to the shared process list
//    - helper methods to compute final statistics
// ============================================================
class Scheduler {
public:
    explicit Scheduler(const std::string& name)
        : algorithmName_(name) {}

    virtual ~Scheduler() = default;

    // ---- Main interface ----------------------------------

    /// Execute the scheduling algorithm on `procs` and return
    /// a complete SimulationResult.
    /// `onTick` is called after each simulated clock tick so
    /// that the Dashboard can refresh the live view.
    virtual SimulationResult run(
        std::vector<Process> procs,
        const AppSettings&   settings,
        std::function<void(int /*time*/,
                           const std::vector<Process>& /*snapshot*/,
                           const std::vector<GanttEntry>& /*ganttSoFar*/,
                           const std::vector<SchedulingLog>& /*logSoFar*/)>
            onTick = nullptr) = 0;

    const std::string& name() const { return algorithmName_; }

protected:
    std::string algorithmName_;

    // ---- Shared post-processing --------------------------

    /// After simulation loop: compute WT, TAT, RT for each
    /// process and fill the aggregate fields of `result`.
    void finalizeResult(SimulationResult& result);

    /// Count context switches from a Gantt chart.
    int countContextSwitches(const std::vector<GanttEntry>& gantt);

    /// Compute total idle time from Gantt.
    int computeIdleTime(const std::vector<GanttEntry>& gantt);
};
