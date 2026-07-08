// main.cpp – CPU Scheduling Studio – Entry Point
//
// Build (from CPU-Scheduling-Studio/ directory):
//   g++ -std=c++17 -O2 -o cpu_studio.exe main.cpp ^
//       src/Colors.cpp src/Scheduler.cpp src/Dashboard.cpp ^
//       src/Process.cpp src/Utilities.cpp ^
//       algorithms/FCFS.cpp algorithms/SJF.cpp ^
//       algorithms/SRTF.cpp algorithms/Priority.cpp ^
//       algorithms/PriorityPreemptive.cpp algorithms/RoundRobin.cpp

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>
#include <iomanip>
#include <numeric>

#include "include/Colors.h"
#include "include/Process.h"
#include "include/Scheduler.h"
#include "include/Utilities.h"
#include "include/Dashboard.h"
#include "include/JsonMode.h"

// ============================================================
//  Forward declarations of factory functions (one per .cpp)
// ============================================================
std::unique_ptr<Scheduler> makeFCFS();
std::unique_ptr<Scheduler> makeSJF();
std::unique_ptr<Scheduler> makeSRTF();
std::unique_ptr<Scheduler> makePriority();
std::unique_ptr<Scheduler> makePriorityPreemptive();
std::unique_ptr<Scheduler> makeRoundRobin();
std::unique_ptr<Scheduler> makeMLFQ();


struct AppState {
    std::vector<Process>        processes;
    SimulationResult            lastResult;
    AppSettings                 settings;
    Dashboard                   dash;
    bool                        hasResult = false;
};

// Deep-copy processes and reset runtime fields for a fresh simulation run.
static std::vector<Process> cloneAndReset(const std::vector<Process>& src) {
    std::vector<Process> copy = src;
    for (auto& p : copy) p.resetForSimulation();
    return copy;
}

// runAlgorithm: launches the scheduler, wires up the Dashboard callback,
// and presents results after simulation ends.
static SimulationResult runAlgorithm(
    AppState& app,
    std::unique_ptr<Scheduler> sched)
{
    if (app.processes.empty()) {
        std::cout << clrError("\n  [ERROR] No processes loaded. "
                               "Please add or generate processes first.\n");
        pressEnter();
        return {};
    }

    clearScreen();
    Dashboard::printSectionHeader("SIMULATION – " + sched->name());

    // Hide cursor during animation
    if (g_colorsEnabled) std::cout << ANSI::CURSOR_HIDE;

    // Build a snapshot for the tick callback
    // We need mutable references to ready/waiting queues per tick.
    // The callback reconstructs them from process states each tick.

    auto onTick = [&](int time,
                      const std::vector<Process>& snapshot,
                      const std::vector<GanttEntry>& gantt,
                      const std::vector<SchedulingLog>& log)
    {
        // Build ready and waiting queue labels
        std::vector<std::string> rq, wq;
        const Process* cpuProc = nullptr;

        for (const auto& p : snapshot) {
            if      (p.state == ProcessState::RUNNING) cpuProc = &p;
            else if (p.state == ProcessState::READY)   rq.push_back(p.pid);
            else if (p.state == ProcessState::WAITING) wq.push_back(p.pid);
        }

        app.dash.render(time, cpuProc, rq, wq, snapshot, gantt, log, app.settings);
    };

    SimulationResult result = sched->run(
        cloneAndReset(app.processes),
        app.settings,
        onTick);

    // Restore cursor
    if (g_colorsEnabled) std::cout << ANSI::CURSOR_SHOW;

    Dashboard::printSimulationComplete(result);

    // Print full results
    printProcessTable(result.processes);
    printGanttChart(result.gantt);
    printStatistics(result);

    // Ask if user wants decision log
    std::cout << clrMenu("\n  Show Scheduling Decision Log? [y/N]: ");
    std::string ans;
    std::getline(std::cin, ans);
    if (!ans.empty() && (ans[0] == 'y' || ans[0] == 'Y'))
        printSchedulingLog(result.log);

    pressEnter();
    return result;
}

// --- Process Management ---

static void addProcess(AppState& app) {
    clearScreen();
    Dashboard::printSectionHeader("ADD PROCESS");

    Process p;

    // PID
    while (true) {
        if (!readString("  PID (e.g. P1): ", p.pid)) {
            std::cout << clrError("  Invalid PID.\n");
            continue;
        }
        if (!isPIDUnique(p.pid, app.processes)) {
            std::cout << clrError("  PID already exists. Choose another.\n");
            continue;
        }
        break;
    }

    // Arrival
    while (!readInt("  Arrival Time (>= 0): ", p.arrivalTime, 0, 1000)) {
        std::cout << clrError("  Invalid value. Must be 0-1000.\n");
    }

    // Burst
    while (!readInt("  Burst Time (>= 1): ", p.burstTime, 1, 1000)) {
        std::cout << clrError("  Invalid value. Must be 1-1000.\n");
    }

    // Priority
    while (!readInt("  Priority (1-10, lower = higher priority): ",
                    p.priority, 1, 100)) {
        std::cout << clrError("  Invalid value. Must be 1-100.\n");
    }

    p.remainingTime = p.burstTime;
    p.state         = ProcessState::NEW;

    app.processes.push_back(p);
    std::cout << clrRunning("\n  ✓ Process " + p.pid + " added successfully.\n");
    pressEnter();
}

static void deleteProcess(AppState& app) {
    clearScreen();
    Dashboard::printSectionHeader("DELETE PROCESS");
    printProcessTable(app.processes);

    std::string pid;
    if (!readString("  Enter PID to delete: ", pid)) return;

    auto it = std::find_if(app.processes.begin(), app.processes.end(),
        [&](const Process& p){ return p.pid == pid; });

    if (it == app.processes.end()) {
        std::cout << clrError("  PID not found.\n");
    } else {
        app.processes.erase(it);
        std::cout << clrRunning("  ✓ Process " + pid + " deleted.\n");
    }
    pressEnter();
}

static void editProcess(AppState& app) {
    clearScreen();
    Dashboard::printSectionHeader("EDIT PROCESS");
    printProcessTable(app.processes);

    std::string pid;
    if (!readString("  Enter PID to edit: ", pid)) return;

    auto it = std::find_if(app.processes.begin(), app.processes.end(),
        [&](const Process& p){ return p.pid == pid; });

    if (it == app.processes.end()) {
        std::cout << clrError("  PID not found.\n");
        pressEnter();
        return;
    }

    Process& p = *it;
    std::cout << "\n  Leave field blank and press Enter to keep current value.\n\n";

    // Arrival
    std::cout << clrMenu("  Arrival Time [" + std::to_string(p.arrivalTime) + "]: ");
    std::string line;
    std::getline(std::cin, line);
    if (!line.empty()) {
        try { p.arrivalTime = std::max(0, std::stoi(line)); } catch(...) {}
    }

    // Burst
    std::cout << clrMenu("  Burst Time [" + std::to_string(p.burstTime) + "]: ");
    std::getline(std::cin, line);
    if (!line.empty()) {
        try {
            int b = std::stoi(line);
            if (b >= 1) { p.burstTime = b; p.remainingTime = b; }
        } catch(...) {}
    }

    // Priority
    std::cout << clrMenu("  Priority [" + std::to_string(p.priority) + "]: ");
    std::getline(std::cin, line);
    if (!line.empty()) {
        try {
            int pr = std::stoi(line);
            if (pr >= 1) p.priority = pr;
        } catch(...) {}
    }

    std::cout << clrRunning("\n  ✓ Process " + pid + " updated.\n");
    pressEnter();
}

static void manageProcesses(AppState& app) {
    while (true) {
        clearScreen();
        Dashboard::printSectionHeader("PROCESS MANAGEMENT");
        printProcessTable(app.processes);

        std::cout << clrMenu("\n  1. Add Process\n"
                             "  2. Delete Process\n"
                             "  3. Edit Process\n"
                             "  4. Clear All\n"
                             "  0. Back\n\n"
                             "  Choice: ");
        std::string choice;
        std::getline(std::cin, choice);

        if      (choice == "1") addProcess(app);
        else if (choice == "2") deleteProcess(app);
        else if (choice == "3") editProcess(app);
        else if (choice == "4") {
            app.processes.clear();
            std::cout << clrRunning("  All processes cleared.\n");
            pressEnter();
        }
        else if (choice == "0") break;
        else std::cout << clrError("  Invalid choice.\n"), pressEnter();
    }
}

// --- Random Process Generation ---

static void generateRandom(AppState& app) {
    clearScreen();
    Dashboard::printSectionHeader("RANDOM PROCESS GENERATOR");

    std::cout << clrMenu("  1. Generate  5 processes\n"
                         "  2. Generate 10 processes\n"
                         "  3. Generate 20 processes\n"
                         "  4. Generate 50 processes\n"
                         "  0. Cancel\n\n"
                         "  Choice: ");
    std::string choice;
    std::getline(std::cin, choice);

    int count = 0;
    if      (choice == "1") count =  5;
    else if (choice == "2") count = 10;
    else if (choice == "3") count = 20;
    else if (choice == "4") count = 50;
    else return;

    app.processes = generateRandomProcesses(count);
    std::cout << clrRunning("\n  ✓ Generated " + std::to_string(count) + " random processes.\n");
    printProcessTable(app.processes);
    pressEnter();
}

// ============================================================
//  Compare All Algorithms
// ============================================================

static void compareAll(AppState& app) {
    if (app.processes.empty()) {
        std::cout << clrError("\n  [ERROR] No processes loaded.\n");
        pressEnter();
        return;
    }

    clearScreen();
    Dashboard::printSectionHeader("ALGORITHM COMPARISON");
    std::cout << clrDim("  Running all 6 algorithms on the same process set…\n\n");

    // Run all algorithms without live animation (speed 0)
    AppSettings fastSettings = app.settings;
    fastSettings.animationSpeedMs = 0;
    fastSettings.stepMode         = false;

    struct AlgoResult {
        std::string name;
        double avgWT, avgTAT, avgRT, cpuUtil, throughput;
        int    ctxSwitches;
    };

    std::vector<AlgoResult> results;

    auto collect = [&](std::unique_ptr<Scheduler> s) {
        auto r = s->run(cloneAndReset(app.processes), fastSettings, nullptr);
        results.push_back({
            r.algorithmName,
            r.avgWaitingTime, r.avgTurnaroundTime, r.avgResponseTime,
            r.cpuUtilization, r.throughput, r.contextSwitches
        });
    };

    collect(makeFCFS());
    collect(makeSJF());
    collect(makeSRTF());
    collect(makePriority());
    collect(makePriorityPreemptive());
    collect(makeRoundRobin());
    collect(makeMLFQ());

    // Find best (lowest avg WT)
    int bestIdx = 0;
    for (int i = 1; i < (int)results.size(); i++)
        if (results[i].avgWT < results[bestIdx].avgWT)
            bestIdx = i;

    // Table header
    const int NW = 42, W = 9;
    std::string hdr =
        padRight("Algorithm", NW) + " | " +
        padLeft("AvgWT", W)       + " | " +
        padLeft("AvgTAT", W)      + " | " +
        padLeft("AvgRT", W)       + " | " +
        padLeft("CPU%", W)        + " | " +
        padLeft("Thruput", W)     + " | " +
        padLeft("CtxSW", W);

    std::cout << clrHeader(repeat('-', (int)hdr.size())) << "\n";
    std::cout << clrHeader(hdr) << "\n";
    std::cout << clrHeader(repeat('-', (int)hdr.size())) << "\n";

    for (int i = 0; i < (int)results.size(); i++) {
        const auto& r = results[i];
        std::string row =
            padRight(r.name, NW)          + " | " +
            padLeft(fmt2(r.avgWT),   W)   + " | " +
            padLeft(fmt2(r.avgTAT),  W)   + " | " +
            padLeft(fmt2(r.avgRT),   W)   + " | " +
            padLeft(fmt2(r.cpuUtil), W)   + " | " +
            padLeft(fmt2(r.throughput),W) + " | " +
            padLeft(std::to_string(r.ctxSwitches), W);

        if (i == bestIdx)
            std::cout << clrRunning("★ " + row) << "\n";
        else
            std::cout << "  " << row << "\n";
    }
    std::cout << clrHeader(repeat('-', (int)hdr.size())) << "\n";
    std::cout << clrRunning("\n  ★ Best Algorithm (Lowest Avg WT): "
                            + results[bestIdx].name + "\n");
    pressEnter();
}

static void exportReport(AppState& app) {
    clearScreen();
    Dashboard::printSectionHeader("EXPORT REPORT");

    std::string path = "reports/report.txt";
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cout << clrError("  [ERROR] Cannot open reports/report.txt for writing.\n");
        pressEnter();
        return;
    }

    out << "CPU SCHEDULING STUDIO – PERFORMANCE REPORT\n";
    out << "Generated: " << currentDateTime() << "\n";
    out << repeat('=', 70) << "\n\n";

    // Input processes
    out << "INPUT PROCESSES\n" << repeat('-', 60) << "\n";
    out << padRight("PID",5) << padRight("Arrival",10) << padRight("Burst",8)
        << padRight("Priority",10) << "\n";
    for (const auto& p : app.processes)
        out << padRight(p.pid,5) << padRight(std::to_string(p.arrivalTime),10)
            << padRight(std::to_string(p.burstTime),8)
            << padRight(std::to_string(p.priority),10) << "\n";
    out << "\n";

    if (app.hasResult) {
        const auto& r = app.lastResult;

        // Algorithm
        out << "ALGORITHM: " << r.algorithmName << "\n\n";

        // Process table
        out << "PROCESS TABLE\n" << repeat('-', 60) << "\n";
        out << padRight("PID",5) << padRight("WT",6) << padRight("TAT",6)
            << padRight("RT",6) << padRight("CT",6) << "\n";
        for (const auto& p : r.processes)
            out << padRight(p.pid,5) << padRight(std::to_string(p.waitingTime),6)
                << padRight(std::to_string(p.turnaroundTime),6)
                << padRight(std::to_string(p.responseTime),6)
                << padRight(std::to_string(p.completionTime),6) << "\n";
        out << "\n";

        // Gantt chart (text)
        out << "GANTT CHART\n" << repeat('-', 60) << "\n";
        out << "| ";
        for (const auto& g : r.gantt) out << g.pid << " | ";
        out << "\n";
        out << r.gantt.front().start;
        for (const auto& g : r.gantt) out << "  " << g.end;
        out << "\n\n";

        // Statistics
        out << "STATISTICS\n" << repeat('-', 60) << "\n";
        out << "Avg Waiting Time     : " << fmt2(r.avgWaitingTime)    << "\n";
        out << "Avg Turnaround Time  : " << fmt2(r.avgTurnaroundTime) << "\n";
        out << "Avg Response Time    : " << fmt2(r.avgResponseTime)   << "\n";
        out << "CPU Utilization      : " << fmt2(r.cpuUtilization) << "%\n";
        out << "Throughput           : " << fmt2(r.throughput) << " proc/unit\n";
        out << "Context Switches     : " << r.contextSwitches << "\n";
        out << "Total Time           : " << r.totalTime << "\n\n";

        // Scheduling log
        out << "SCHEDULING LOG\n" << repeat('-', 60) << "\n";
        for (const auto& e : r.log)
            out << "t=" << e.time << " -> " << e.pid << " | " << e.reason << "\n";
    }

    // Comparison table
    out << "\nALGORITHM COMPARISON\n" << repeat('-', 60) << "\n";
    AppSettings fastSettings = app.settings;
    fastSettings.animationSpeedMs = 0;
    fastSettings.stepMode = false;

    auto runAndWrite = [&](std::unique_ptr<Scheduler> s) {
        auto r = s->run(cloneAndReset(app.processes), fastSettings, nullptr);
        out << padRight(r.algorithmName, 40)
            << " WT=" << fmt2(r.avgWaitingTime)
            << " TAT=" << fmt2(r.avgTurnaroundTime)
            << " CPU=" << fmt2(r.cpuUtilization) << "%\n";
    };

    runAndWrite(makeFCFS());
    runAndWrite(makeSJF());
    runAndWrite(makeSRTF());
    runAndWrite(makePriority());
    runAndWrite(makePriorityPreemptive());
    runAndWrite(makeRoundRobin());
    runAndWrite(makeMLFQ());

    out.close();
    std::cout << clrRunning("  ✓ Report saved to: " + path + "\n");
    pressEnter();
}

// ============================================================
//  Settings Menu
// ============================================================

static void settingsMenu(AppState& app) {
    while (true) {
        clearScreen();
        Dashboard::printSettingsMenu(app.settings);

        std::string choice;
        std::getline(std::cin, choice);

        if (choice == "0") break;

        else if (choice == "1") {
            int ms;
            if (readInt("  New animation speed (ms, 0-2000): ", ms, 0, 2000))
                app.settings.animationSpeedMs = ms;
            else std::cout << clrError("  Invalid value.\n"), pressEnter();
        }
        else if (choice == "2") {
            app.settings.darkTheme = !app.settings.darkTheme;
            std::cout << clrRunning("  Theme: " +
                std::string(app.settings.darkTheme ? "Dark" : "Light") + "\n");
            pressEnter();
        }
        else if (choice == "3") {
            app.settings.colorsEnabled = !app.settings.colorsEnabled;
            g_colorsEnabled = app.settings.colorsEnabled;
            std::cout << "  Colors: " <<
                (app.settings.colorsEnabled ? "Enabled" : "Disabled") << "\n";
            pressEnter();
        }
        else if (choice == "4") {
            app.settings.stepMode = !app.settings.stepMode;
            std::cout << clrRunning("  Step Mode: " +
                std::string(app.settings.stepMode ? "ON" : "OFF") + "\n");
            pressEnter();
        }
        else if (choice == "5") {
            int q;
            if (readInt("  New time quantum (1-100): ", q, 1, 100))
                app.settings.timeQuantum = q;
            else std::cout << clrError("  Invalid value.\n"), pressEnter();
        }
        else {
            std::cout << clrError("  Invalid choice.\n");
            pressEnter();
        }
    }
}

// ============================================================
//  main
// ============================================================
int main(int argc, char* argv[]) {
    // If running in JSON Mode for the web app backend
    if (argc > 1 && std::string(argv[1]) == "--json") {
        return runJsonMode();
    }

    // Enable ANSI VT sequences on Windows
    enableWindowsVT();

    AppState app;

    // Show startup banner
    Dashboard::printBanner();
    pressEnter("Press [Enter] to enter the main menu...");

    // ---- Main loop ---------------------------------------
    while (true) {
        clearScreen();
        Dashboard::printBanner();
        Dashboard::printMainMenu(app.settings);

        std::string choice;
        std::getline(std::cin, choice);

        // ---- Process info helpers ---
        auto requireProcesses = [&]() -> bool {
            if (app.processes.empty()) {
                std::cout << clrError(
                    "\n  [ERROR] No processes. Add or generate first.\n");
                pressEnter();
                return false;
            }
            return true;
        };

        auto dispatchAndSave = [&](std::unique_ptr<Scheduler> s) {
            if (!requireProcesses()) return;
            auto result = runAlgorithm(app, std::move(s));
            if (!result.algorithmName.empty()) {
                app.lastResult = result;
                app.hasResult  = true;
            }
        };

        if      (choice == "0") {
            clearScreen();
            std::cout << clrHeader("\n  Goodbye! Thank you for using CPU Scheduling Studio.\n\n");
            break;
        }
        else if (choice == "1")  manageProcesses(app);
        else if (choice == "2")  generateRandom(app);
        else if (choice == "3") {
            clearScreen();
            Dashboard::printSectionHeader("PROCESS TABLE");
            if (app.processes.empty())
                std::cout << clrDim("  (No processes loaded)\n");
            else
                printProcessTable(app.processes);
            pressEnter();
        }
        else if (choice == "4")  dispatchAndSave(makeFCFS());
        else if (choice == "5")  dispatchAndSave(makeSJF());
        else if (choice == "6")  dispatchAndSave(makeSRTF());
        else if (choice == "7")  dispatchAndSave(makePriority());
        else if (choice == "8")  dispatchAndSave(makePriorityPreemptive());
        else if (choice == "9")  dispatchAndSave(makeRoundRobin());
        else if (choice == "10") dispatchAndSave(makeMLFQ());
        else if (choice == "11") {
            if (requireProcesses()) compareAll(app);
        }
        else if (choice == "12") exportReport(app);
        else if (choice == "13") settingsMenu(app);
        else {
            std::cout << clrError("\n  [ERROR] Invalid menu choice.\n");
            pressEnter();
        }
    }

    return 0;
}
