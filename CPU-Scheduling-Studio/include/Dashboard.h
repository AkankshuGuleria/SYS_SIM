// ============================================================
//  Dashboard.h
//  Declares the Dashboard class that renders the live
//  animated console view of the simulation.
// ============================================================
#pragma once

#include <vector>
#include <string>
#include "Process.h"
#include "Scheduler.h"
#include "Colors.h"

// ============================================================
//  Dashboard
//
//  Responsible for ALL console output during a live simulation.
//  Uses ANSI cursor-control to redraw the screen in place,
//  giving a smooth animation effect rather than scrolling.
// ============================================================
class Dashboard {
public:
    Dashboard() = default;

    // ---- Live render ------------------------------------

    /// Full redraw called every clock tick.
    /// @param time         Current simulation clock
    /// @param cpuProcess   Pointer to the running process (nullptr = idle)
    /// @param readyQueue   PIDs currently in the ready queue
    /// @param waitingQueue PIDs currently waiting
    /// @param processes    Full snapshot of all processes
    /// @param gantt        Gantt chart accumulated so far
    /// @param log          Decision log entries so far
    /// @param settings     Current application settings
    void render(
        int                             time,
        const Process*                  cpuProcess,
        const std::vector<std::string>& readyQueue,
        const std::vector<std::string>& waitingQueue,
        const std::vector<Process>&     processes,
        const std::vector<GanttEntry>&  gantt,
        const std::vector<SchedulingLog>& log,
        const AppSettings&              settings);

    // ---- Static helpers (also called from menus) --------

    /// Print the decorative application banner.
    static void printBanner();

    /// Print the main menu.
    static void printMainMenu(const AppSettings& settings);

    /// Print the settings menu.
    static void printSettingsMenu(const AppSettings& settings);

    /// Print a section header box.
    static void printSectionHeader(const std::string& title);

    /// Print a completion summary card at the end of simulation.
    static void printSimulationComplete(const SimulationResult& result);

private:
    // ---- Internal section renderers ----------------------

    void drawSystemClock(int time) const;
    void drawCPUPanel(const Process* cpuProc, const std::vector<Process>& procs) const;
    void drawReadyQueue(const std::vector<std::string>& queue) const;
    void drawWaitingQueue(const std::vector<std::string>& queue) const;
    void drawProcessStates(const std::vector<Process>& procs) const;
    void drawLiveGantt(const std::vector<GanttEntry>& gantt) const;
    void drawLastLogEntry(const std::vector<SchedulingLog>& log) const;
    void drawCompletedList(const std::vector<Process>& procs) const;
};
