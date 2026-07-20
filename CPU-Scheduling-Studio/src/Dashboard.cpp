// Dashboard.cpp – live animated console view
#include "../include/Dashboard.h"
#include "../include/Utilities.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

// --- render: called every clock tick; moves cursor home and redraws all panels ---
void Dashboard::render(
    int time, const Process* cpuProcess,
    const std::vector<std::string>& readyQueue,
    const std::vector<std::string>& waitingQueue,
    const std::vector<Process>& processes,
    const std::vector<GanttEntry>& gantt,
    const std::vector<SchedulingLog>& log,
    const AppSettings& settings)
{
    std::cout << ANSI::CURSOR_HOME << std::flush;
    std::cout << clrHeader(repeat('=', 70)) << "\n";
    std::cout << clrHeader(center("  CPU SCHEDULING STUDIO  –  LIVE SIMULATION", 70)) << "\n";
    std::cout << clrHeader(repeat('=', 70)) << "\n\n";

    drawSystemClock(time);
    drawCPUPanel(cpuProcess, processes);
    drawReadyQueue(readyQueue);
    drawWaitingQueue(waitingQueue);
    drawProcessStates(processes);
    drawLastLogEntry(log);
    drawCompletedList(processes);
    drawLiveGantt(gantt);

    std::cout << std::flush;
}

// --- System clock panel ---
void Dashboard::drawSystemClock(int time) const {
    std::cout << clrMenu(repeat('-', 40)) << "\n";
    std::cout << clrBold(clrMenu("  SYSTEM CLOCK:  t = " + std::to_string(time) + "  ")) << "\n";
    std::cout << clrMenu(repeat('-', 40)) << "\n\n";
}

// --- CPU panel: shows current running process and progress bar ---
void Dashboard::drawCPUPanel(const Process* cpuProc,
                              const std::vector<Process>& procs) const {
    std::cout << clrMenu("+---------------------------+") << "\n";
    std::cout << clrMenu("|") << clrBold("       CURRENT CPU         ") << clrMenu("|") << "\n";
    std::cout << clrMenu("+---------------------------+") << "\n";

    if (cpuProc == nullptr) {
        std::cout << clrMenu("|") << clrDim("          CPU IDLE         ") << clrMenu("|") << "\n";
        std::cout << clrMenu("|") << "                           " << clrMenu("|") << "\n";
        std::cout << clrMenu("|") << clrDim("  [                    ]   ") << clrMenu("|") << "\n";
    } else {
        std::string bar    = progressBar(cpuProc->remainingTime, cpuProc->burstTime, 20);
        std::string remStr = "  Rem: " + std::to_string(cpuProc->remainingTime)
                           + "/" + std::to_string(cpuProc->burstTime) + "          ";
        std::cout << clrMenu("|") << clrRunning(center(cpuProc->pid, 27))    << clrMenu("|") << "\n";
        std::cout << clrMenu("|") << "  " << clrRunning(bar) << "   "        << clrMenu("|") << "\n";
        std::cout << clrMenu("|") << padRight(remStr, 27)                    << clrMenu("|") << "\n";
    }
    std::cout << clrMenu("+---------------------------+") << "\n\n";
}

// --- Ready queue: processes waiting for CPU ---
void Dashboard::drawReadyQueue(const std::vector<std::string>& queue) const {
    std::cout << clrReady("  READY QUEUE") << "\n  " << clrDim("Front: ");
    if (queue.empty()) { std::cout << clrDim("(empty)"); }
    else {
        for (size_t i = 0; i < queue.size(); i++) {
            std::cout << clrReady(queue[i]);
            if (i + 1 < queue.size()) std::cout << clrDim(" -> ");
        }
    }
    std::cout << clrDim(" :Rear") << "\n\n";
}

// --- Waiting queue: blocked processes ---
void Dashboard::drawWaitingQueue(const std::vector<std::string>& queue) const {
    std::cout << clrWaiting("  WAITING QUEUE") << "\n  ";
    if (queue.empty()) { std::cout << clrDim("(empty)"); }
    else {
        for (size_t i = 0; i < queue.size(); i++) {
            std::cout << clrWaiting(queue[i]);
            if (i + 1 < queue.size()) std::cout << clrDim(", ");
        }
    }
    std::cout << "\n\n";
}

// --- Process state list: colour-coded by state ---
void Dashboard::drawProcessStates(const std::vector<Process>& procs) const {
    std::cout << clrMenu("  PROCESS STATES") << "\n";
    for (const auto& p : procs) {
        std::string entry = "  " + padRight(p.pid, 5) + "[" + padRight(p.stateStr(), 10) + "]";
        if      (p.state == ProcessState::RUNNING)    std::cout << clrRunning(entry);
        else if (p.state == ProcessState::READY)      std::cout << clrReady(entry);
        else if (p.state == ProcessState::WAITING)    std::cout << clrWaiting(entry);
        else if (p.state == ProcessState::TERMINATED) std::cout << clrCompleted(entry);
        else                                           std::cout << clrDim(entry);
        if (p.state == ProcessState::RUNNING)
            std::cout << clrRunning("  Remaining: " + std::to_string(p.remainingTime));
        std::cout << "\n";
    }
    std::cout << "\n";
}

// --- Last scheduling decision ---
void Dashboard::drawLastLogEntry(const std::vector<SchedulingLog>& log) const {
    if (log.empty()) return;
    const auto& last = log.back();
    std::cout << clrMenu("  LAST DECISION") << "\n";
    std::cout << "  t=" << last.time << "  -> " << clrRunning(last.pid)
              << "  | " << clrDim(last.reason) << "\n\n";
}

// --- List of completed processes ---
void Dashboard::drawCompletedList(const std::vector<Process>& procs) const {
    std::cout << clrCompleted("  COMPLETED PROCESSES") << "\n  ";
    bool any = false;
    for (const auto& p : procs)
        if (p.state == ProcessState::TERMINATED) { std::cout << clrCompleted(p.pid + " "); any = true; }
    if (!any) std::cout << clrDim("(none yet)");
    std::cout << "\n\n";
}

// --- Live Gantt chart: rolling view of last 15 blocks ---
void Dashboard::drawLiveGantt(const std::vector<GanttEntry>& gantt) const {
    if (gantt.empty()) return;
    std::cout << clrMenu("  LIVE GANTT (last 15 blocks)") << "\n  ";
    size_t start = (gantt.size() > 15) ? gantt.size() - 15 : 0;
    for (size_t i = start; i < gantt.size(); i++) {
        std::string cell = "[" + gantt[i].pid + "]";
        std::cout << (gantt[i].pid == "Idle" ? clrDim(cell) : clrRunning(cell));
    }
    std::cout << "\n  " << std::to_string(gantt[start].start);
    for (size_t i = start; i < gantt.size(); i++) {
        std::string cell = "[" + gantt[i].pid + "]";
        int w = (int)cell.size() - (int)std::to_string(gantt[i].end).size();
        if (w > 0) std::cout << std::string(w, ' ');
        std::cout << gantt[i].end;
    }
    std::cout << "\n\n";
}

// --- Application banner ---
void Dashboard::printBanner() {
    clearScreen();
    std::cout << clrHeader(repeat('*', 70)) << "\n";
    std::cout << clrHeader(center("  CPU SCHEDULING STUDIO", 70)) << "\n";
    std::cout << clrHeader(center("Advanced CPU Scheduling & Performance Analysis", 70)) << "\n";
    std::cout << clrHeader(repeat('*', 70)) << "\n";
    std::cout << clrDim(center("C++17  |  ANSI Console  |  7 Algorithms  |  Real-time Simulation", 70)) << "\n\n";
}

// --- Main menu ---
void Dashboard::printMainMenu(const AppSettings& settings) {
    std::cout << clrHeader(repeat('=', 44)) << "\n";
    std::cout << clrHeader(center("  CPU SCHEDULING STUDIO", 44)) << "\n";
    std::cout << clrHeader(repeat('=', 44)) << "\n";

    auto item = [&](const std::string& num, const std::string& label) {
        std::cout << "  " << clrMenu(padRight(num, 4)) << label << "\n";
    };
    item("1.",  "Enter Processes Manually");
    item("2.",  "Generate Random Processes");
    item("3.",  "View Process Table");
    item("4.",  "Run FCFS");
    item("5.",  "Run SJF (Non-Preemptive)");
    item("6.",  "Run SRTF (Preemptive)");
    item("7.",  "Run Priority (Non-Preemptive)");
    item("8.",  "Run Priority (Preemptive)");
    item("9.",  "Run Round Robin  [Quantum=" + std::to_string(settings.timeQuantum) + "]");
    item("10.", "Run MLFQ");
    item("11.", "Compare All Algorithms");
    item("12.", "Export Report");
    item("13.", "Settings");
    item("0.",  "Exit");

    std::cout << clrHeader(repeat('=', 44)) << "\n";
    std::cout << clrMenu("  Choice: ");
}

// --- Settings menu ---
void Dashboard::printSettingsMenu(const AppSettings& settings) {
    std::cout << clrHeader(repeat('=', 44)) << "\n";
    std::cout << clrHeader(center("  SETTINGS", 44)) << "\n";
    std::cout << clrHeader(repeat('=', 44)) << "\n";

    auto item = [&](const std::string& num, const std::string& label, const std::string& val) {
        std::cout << "  " << clrMenu(padRight(num, 4)) << padRight(label, 26) << clrBold(val) << "\n";
    };
    item("1.", "Animation Speed (ms)",  std::to_string(settings.animationSpeedMs));
    item("2.", "Theme",                 settings.darkTheme ? "Dark" : "Light");
    item("3.", "Colors",                settings.colorsEnabled ? "Enabled" : "Disabled");
    item("4.", "Step Mode",             settings.stepMode ? "ON" : "OFF");
    item("5.", "Round-Robin Quantum",   std::to_string(settings.timeQuantum));
    item("0.", "Back to Main Menu",     "");

    std::cout << clrHeader(repeat('=', 44)) << "\n";
    std::cout << clrMenu("  Choice: ");
}

// --- Section header box ---
void Dashboard::printSectionHeader(const std::string& title) {
    std::cout << "\n" << clrHeader(repeat('=', 60)) << "\n";
    std::cout << clrHeader("  " + title) << "\n";
    std::cout << clrHeader(repeat('=', 60)) << "\n\n";
}

// --- Shown after simulation ends ---
void Dashboard::printSimulationComplete(const SimulationResult& result) {
    std::cout << "\n" << clrHeader(repeat('=', 60)) << "\n";
    std::cout << clrHeader(center("  SIMULATION COMPLETE  –  " + result.algorithmName, 60)) << "\n";
    std::cout << clrHeader(repeat('=', 60)) << "\n";
}
