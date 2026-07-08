// ============================================================
//  Utilities.h
//  General-purpose helper functions used across all modules.
//  Topics: string formatting, process table printing,
//          Gantt chart rendering, statistics calculation,
//          input validation, random process generation.
// ============================================================
#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <chrono>
#include <ctime>
#include <climits>
#include <numeric>
#include "Process.h"
#include "Colors.h"

// ============================================================
//  String helpers
// ============================================================

/// Pad string to exact width with spaces (left-aligned).
inline std::string padRight(const std::string& s, int width) {
    if ((int)s.size() >= width) return s.substr(0, width);
    return s + std::string(width - s.size(), ' ');
}

/// Pad string to exact width with spaces (right-aligned).
inline std::string padLeft(const std::string& s, int width) {
    if ((int)s.size() >= width) return s.substr(0, width);
    return std::string(width - s.size(), ' ') + s;
}

/// Center a string within a field of `width`.
inline std::string center(const std::string& s, int width) {
    if ((int)s.size() >= width) return s.substr(0, width);
    int pad  = width - (int)s.size();
    int left = pad / 2;
    int right= pad - left;
    return std::string(left, ' ') + s + std::string(right, ' ');
}

/// Repeat a character n times.
inline std::string repeat(char c, int n) {
    return (n > 0) ? std::string(n, c) : "";
}

/// Format a double to 2 decimal places.
inline std::string fmt2(double v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << v;
    return ss.str();
}

/// Get a current date-time string for report headers.
inline std::string currentDateTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return std::string(buf);
}

// ============================================================
//  Progress bar (for CPU burst visualisation)
//  filled: cells already consumed  total: full width in chars
// ============================================================
inline std::string progressBar(int remaining, int total, int barWidth = 20) {
    if (total <= 0) total = 1;
    int filled = barWidth - (int)((double)remaining / total * barWidth);
    filled = std::max(0, std::min(filled, barWidth));
    // Use unicode block characters: █ (filled) vs ░ (empty)
    std::string result = "[";
    for (int i = 0; i < barWidth; i++) {
        result += (i < filled) ? "\xE2\x96\x88" : "\xE2\x96\x91";
    }
    result += "]";
    return result;
}

// ============================================================
//  Statistics calculation
//  Fills aggregated fields of SimulationResult.
// ============================================================
inline void computeStatistics(SimulationResult& res) {
    if (res.processes.empty()) return;

    double sumWT = 0, sumTAT = 0, sumRT = 0;
    int maxWT = -1, minWT = INT32_MAX;
    std::string maxPID, minPID;

    for (const auto& p : res.processes) {
        sumWT  += p.waitingTime;
        sumTAT += p.turnaroundTime;
        sumRT  += p.responseTime;
        if (p.waitingTime > maxWT) { maxWT = p.waitingTime; maxPID = p.pid; }
        if (p.waitingTime < minWT) { minWT = p.waitingTime; minPID = p.pid; }
    }

    int n = (int)res.processes.size();
    res.avgWaitingTime    = sumWT  / n;
    res.avgTurnaroundTime = sumTAT / n;
    res.avgResponseTime   = sumRT  / n;

    // CPU utilization
    int busyTime = res.totalTime - res.idleTime;
    res.cpuUtilization = (res.totalTime > 0)
        ? 100.0 * busyTime / res.totalTime : 0.0;

    // Throughput = processes completed / total time
    res.throughput = (res.totalTime > 0)
        ? (double)n / res.totalTime : 0.0;

    // Jain's Fairness Index on turnaround times:
    //   J = (\u03a3 TAT_i)\u00b2 / (n \u00d7 \u03a3 TAT_i\u00b2)
    //   Range: [1/n, 1.0].  1.0 = perfectly fair (identical TAT for all).
    double sumTATsq = 0.0;
    for (const auto& p : res.processes) {
        double tat = static_cast<double>(p.turnaroundTime);
        sumTATsq += tat * tat;
    }
    res.jainFairnessIndex = (sumTATsq > 0.0)
        ? (sumTAT * sumTAT) / (n * sumTATsq)
        : 1.0;

    res.longestWaitingPID  = maxPID;
    res.shortestWaitingPID = minPID;
}

// ============================================================
//  Process table printer
// ============================================================
inline void printProcessTable(const std::vector<Process>& procs) {
    const int W = 80;
    std::string line = repeat('-', W);

    std::cout << clrHeader(line) << "\n";
    std::cout << clrHeader(
        " " + padRight("PID",4) + " | " +
        padRight("Arrival",8) + " | " +
        padRight("Burst",6)   + " | " +
        padRight("Priority",9) + " | " +
        padRight("Start",6)   + " | " +
        padRight("Finish",7)  + " | " +
        padRight("WT",5)      + " | " +
        padRight("TAT",5)     + " | " +
        padRight("RT",5)      + " | " +
        "State"
    ) << "\n";
    std::cout << clrHeader(line) << "\n";

    for (const auto& p : procs) {
        std::string row =
            " " + padRight(p.pid, 4)                       + " | " +
            padRight(std::to_string(p.arrivalTime),  8)    + " | " +
            padRight(std::to_string(p.burstTime),    6)    + " | " +
            padRight(std::to_string(p.priority),     9)    + " | " +
            padRight(std::to_string(p.startTime),    6)    + " | " +
            padRight(std::to_string(p.completionTime),7)   + " | " +
            padRight(std::to_string(p.waitingTime),  5)    + " | " +
            padRight(std::to_string(p.turnaroundTime),5)   + " | " +
            padRight(std::to_string(p.responseTime), 5)    + " | " +
            p.stateStr();

        // Colour by state
        if      (p.state == ProcessState::RUNNING)    std::cout << clrRunning(row);
        else if (p.state == ProcessState::READY)      std::cout << clrReady(row);
        else if (p.state == ProcessState::WAITING)    std::cout << clrWaiting(row);
        else if (p.state == ProcessState::TERMINATED) std::cout << clrCompleted(row);
        else                                           std::cout << row;
        std::cout << "\n";
    }
    std::cout << clrHeader(line) << "\n";
}

// ============================================================
//  Gantt chart renderer
// ============================================================
inline void printGanttChart(const std::vector<GanttEntry>& gantt) {
    if (gantt.empty()) { std::cout << "(empty)\n"; return; }

    std::cout << "\n";

    // Top border
    std::string top = "+";
    for (const auto& g : gantt) {
        int w = std::max(4, (int)g.pid.size() + 2);
        top += repeat('-', w) + "+";
    }
    std::cout << clrMenu(top) << "\n";

    // Process labels
    std::string labels = "|";
    for (const auto& g : gantt) {
        int w = std::max(4, (int)g.pid.size() + 2);
        std::string cell = center(g.pid, w);
        if      (g.pid == "Idle") labels += clrDim(cell) + "|";
        else                      labels += clrRunning(cell) + "|";
    }
    std::cout << labels << "\n";

    // Bottom border
    std::cout << clrMenu(top) << "\n";

    // Time markers
    std::string times;
    // First marker at the start time of the first block
    times += std::to_string(gantt.front().start);
    int prevLen = (int)std::to_string(gantt.front().start).size();
    for (const auto& g : gantt) {
        int cellW = std::max(4, (int)g.pid.size() + 2) + 1; // +1 for '|'
        std::string t = std::to_string(g.end);
        int spaces = cellW - prevLen - (int)t.size();
        if (spaces < 0) spaces = 0;
        times += std::string(spaces, ' ') + t;
        prevLen = (int)t.size();
    }
    std::cout << clrDim(times) << "\n\n";
}

// ============================================================
//  Statistics summary printer
// ============================================================
inline void printStatistics(const SimulationResult& res) {
    const int W = 50;
    std::string line = repeat('=', W);

    std::cout << clrHeader("\n" + line) << "\n";
    std::cout << clrHeader("  PERFORMANCE STATISTICS – " + res.algorithmName) << "\n";
    std::cout << clrHeader(line) << "\n";

    auto row = [&](const std::string& label, const std::string& val) {
        std::cout << "  " << clrMenu(padRight(label, 28)) << ": "
                  << clrBold(val) << "\n";
    };

    row("Avg Waiting Time",    fmt2(res.avgWaitingTime)    + " units");
    row("Avg Turnaround Time", fmt2(res.avgTurnaroundTime) + " units");
    row("Avg Response Time",   fmt2(res.avgResponseTime)   + " units");
    row("CPU Utilization",     fmt2(res.cpuUtilization)    + " %");
    row("CPU Idle Time",       std::to_string(res.idleTime)+ " units");
    row("Throughput",          fmt2(res.throughput)        + " proc/unit");
    row("Jain Fairness Index", fmt2(res.jainFairnessIndex)
                               + "  (1.0=fair, 1/n=unfair)");
    row("Context Switches",    std::to_string(res.contextSwitches));
    row("Total Execution Time",std::to_string(res.totalTime)+" units");
    row("Longest Waiting PID", res.longestWaitingPID);
    row("Shortest Waiting PID",res.shortestWaitingPID);

    std::cout << clrHeader(line) << "\n\n";
}

// ============================================================
//  Scheduling decision log printer
// ============================================================
inline void printSchedulingLog(const std::vector<SchedulingLog>& log) {
    std::cout << clrHeader("\n===== SCHEDULING DECISION LOG =====\n");
    for (const auto& entry : log) {
        std::cout << clrMenu("  Time " + std::to_string(entry.time))
                  << "\n  Selected: " << clrRunning(entry.pid)
                  << "\n  Reason:   " << entry.reason
                  << "\n  " << repeat('-', 34) << "\n";
    }
    std::cout << "\n";
}

// ============================================================
//  Random process generator
// ============================================================
inline std::vector<Process> generateRandomProcesses(int count) {
    // Seed from current time for true randomness each run
    std::mt19937 rng(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count()));

    std::uniform_int_distribution<int> arrDist(0, 10);
    std::uniform_int_distribution<int> burstDist(1, 15);
    std::uniform_int_distribution<int> priDist(1, 10);

    std::vector<Process> procs;
    procs.reserve(count);

    for (int i = 1; i <= count; i++) {
        Process p;
        p.pid           = "P" + std::to_string(i);
        p.arrivalTime   = arrDist(rng);
        p.burstTime     = burstDist(rng);
        p.remainingTime = p.burstTime;
        p.priority      = priDist(rng);
        p.state         = ProcessState::NEW;
        procs.push_back(p);
    }
    return procs;
}

// ============================================================
//  Input validation helpers
// ============================================================

/// Returns true if `pid` is unique among existing processes.
inline bool isPIDUnique(const std::string& pid,
                        const std::vector<Process>& procs) {
    for (const auto& p : procs) if (p.pid == pid) return false;
    return true;
}

/// Safely read an integer from stdin with a prompt and range check.
/// Returns false if the user enters non-numeric input.
inline bool readInt(const std::string& prompt, int& out,
                    int minVal = INT32_MIN, int maxVal = INT32_MAX) {
    std::cout << clrMenu(prompt);
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    try {
        std::size_t pos;
        int val = std::stoi(line, &pos);
        if (pos != line.size()) return false; // trailing garbage
        if (val < minVal || val > maxVal) return false;
        out = val;
        return true;
    } catch (...) {
        return false;
    }
}

/// Read a non-empty string (strip leading/trailing spaces).
inline bool readString(const std::string& prompt, std::string& out) {
    std::cout << clrMenu(prompt);
    if (!std::getline(std::cin, out)) return false;
    // trim
    size_t s = out.find_first_not_of(" \t");
    size_t e = out.find_last_not_of(" \t");
    if (s == std::string::npos) return false;
    out = out.substr(s, e - s + 1);
    return !out.empty();
}

/// Pause and wait for Enter key.
inline void pressEnter(const std::string& msg = "Press [Enter] to continue...") {
    std::cout << clrDim("\n  " + msg);
    std::string dummy;
    std::getline(std::cin, dummy);
}

/// Clear the terminal screen.
inline void clearScreen() {
    if (g_colorsEnabled)
        std::cout << ANSI::CLEAR_SCREEN << std::flush;
    else
        std::cout << "\n" << repeat('=', 60) << "\n";
}

// ============================================================
//  Portable sleep – works on MinGW 6.3 (no std::this_thread)
//  Uses Windows Sleep() on Windows, nanosleep on POSIX.
// ============================================================
inline void sleepMs(int ms) {
    if (ms <= 0) return;
#ifdef _WIN32
    Sleep(static_cast<DWORD>(ms));
#else
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, nullptr);
#endif
}
