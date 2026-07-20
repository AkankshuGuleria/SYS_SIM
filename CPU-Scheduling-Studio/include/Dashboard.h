#pragma once
#include <vector>
#include <string>
#include "Process.h"
#include "Scheduler.h"
#include "Colors.h"

// --- Renders the live animated console view during a simulation ---
class Dashboard {
public:
    Dashboard() = default;

    // Called every clock tick to redraw the screen
    void render(
        int                              time,
        const Process*                   cpuProcess,
        const std::vector<std::string>&  readyQueue,
        const std::vector<std::string>&  waitingQueue,
        const std::vector<Process>&      processes,
        const std::vector<GanttEntry>&   gantt,
        const std::vector<SchedulingLog>& log,
        const AppSettings&               settings);

    // Static helpers used by menus
    static void printBanner();
    static void printMainMenu(const AppSettings& settings);
    static void printSettingsMenu(const AppSettings& settings);
    static void printSectionHeader(const std::string& title);
    static void printSimulationComplete(const SimulationResult& result);

private:
    // Internal section renderers (each draws one panel)
    void drawSystemClock(int time) const;
    void drawCPUPanel(const Process* cpuProc, const std::vector<Process>& procs) const;
    void drawReadyQueue(const std::vector<std::string>& queue) const;
    void drawWaitingQueue(const std::vector<std::string>& queue) const;
    void drawProcessStates(const std::vector<Process>& procs) const;
    void drawLiveGantt(const std::vector<GanttEntry>& gantt) const;
    void drawLastLogEntry(const std::vector<SchedulingLog>& log) const;
    void drawCompletedList(const std::vector<Process>& procs) const;
};
