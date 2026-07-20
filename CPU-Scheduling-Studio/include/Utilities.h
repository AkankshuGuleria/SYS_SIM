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

// --- String padding helpers ---
inline std::string padRight(const std::string& s, int w) {
    if ((int)s.size() >= w) return s.substr(0, w);
    return s + std::string(w - s.size(), ' ');
}
inline std::string padLeft(const std::string& s, int w) {
    if ((int)s.size() >= w) return s.substr(0, w);
    return std::string(w - s.size(), ' ') + s;
}
inline std::string center(const std::string& s, int w) {
    if ((int)s.size() >= w) return s.substr(0, w);
    int pad = w - (int)s.size(), left = pad / 2;
    return std::string(left, ' ') + s + std::string(pad - left, ' ');
}
inline std::string repeat(char c, int n) { return (n > 0) ? std::string(n, c) : ""; }
inline std::string fmt2(double v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << v;
    return ss.str();
}
inline std::string currentDateTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return std::string(buf);
}

// --- Progress bar: shows how much burst time has been consumed ---
inline std::string progressBar(int remaining, int total, int barWidth = 20) {
    if (total <= 0) total = 1;
    int filled = barWidth - (int)((double)remaining / total * barWidth);
    filled = std::max(0, std::min(filled, barWidth));
    std::string r = "[";
    for (int i = 0; i < barWidth; i++)
        r += (i < filled) ? "\xE2\x96\x88" : "\xE2\x96\x91";
    return r + "]";
}

// --- Compute aggregate statistics after simulation ends ---
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
    res.cpuUtilization    = (res.totalTime > 0) ? 100.0 * (res.totalTime - res.idleTime) / res.totalTime : 0.0;
    res.throughput        = (res.totalTime > 0) ? (double)n / res.totalTime : 0.0;

    // Jain's Fairness Index: J = (sum TAT)^2 / (n * sum TAT^2), range [1/n, 1.0]
    double sumTATsq = 0.0;
    for (const auto& p : res.processes) { double tat = p.turnaroundTime; sumTATsq += tat * tat; }
    res.jainFairnessIndex = (sumTATsq > 0.0) ? (sumTAT * sumTAT) / (n * sumTATsq) : 1.0;

    res.longestWaitingPID  = maxPID;
    res.shortestWaitingPID = minPID;
}

// --- Print a colour-coded process table ---
inline void printProcessTable(const std::vector<Process>& procs) {
    const int W = 80;
    std::cout << clrHeader(repeat('-', W)) << "\n";
    std::cout << clrHeader(
        " " + padRight("PID",4) + " | " + padRight("Arrival",8) + " | " +
        padRight("Burst",6) + " | " + padRight("Priority",9) + " | " +
        padRight("Start",6) + " | " + padRight("Finish",7) + " | " +
        padRight("WT",5) + " | " + padRight("TAT",5) + " | " +
        padRight("RT",5) + " | " + "State") << "\n";
    std::cout << clrHeader(repeat('-', W)) << "\n";

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

        if      (p.state == ProcessState::RUNNING)    std::cout << clrRunning(row);
        else if (p.state == ProcessState::READY)      std::cout << clrReady(row);
        else if (p.state == ProcessState::WAITING)    std::cout << clrWaiting(row);
        else if (p.state == ProcessState::TERMINATED) std::cout << clrCompleted(row);
        else                                           std::cout << row;
        std::cout << "\n";
    }
    std::cout << clrHeader(repeat('-', W)) << "\n";
}

// --- Print the Gantt chart with time markers ---
inline void printGanttChart(const std::vector<GanttEntry>& gantt) {
    if (gantt.empty()) { std::cout << "(empty)\n"; return; }
    std::string top = "+";
    for (const auto& g : gantt) {
        int w = std::max(4, (int)g.pid.size() + 2);
        top += repeat('-', w) + "+";
    }
    std::cout << "\n" << clrMenu(top) << "\n";

    std::string labels = "|";
    for (const auto& g : gantt) {
        int w = std::max(4, (int)g.pid.size() + 2);
        std::string cell = center(g.pid, w);
        labels += (g.pid == "Idle" ? clrDim(cell) : clrRunning(cell)) + "|";
    }
    std::cout << labels << "\n" << clrMenu(top) << "\n";

    // Time markers below each block
    std::string times = std::to_string(gantt.front().start);
    int prevLen = (int)times.size();
    for (const auto& g : gantt) {
        int cellW = std::max(4, (int)g.pid.size() + 2) + 1;
        std::string t = std::to_string(g.end);
        int spaces = std::max(0, cellW - prevLen - (int)t.size());
        times += std::string(spaces, ' ') + t;
        prevLen = (int)t.size();
    }
    std::cout << clrDim(times) << "\n\n";
}

// --- Print aggregated performance statistics ---
inline void printStatistics(const SimulationResult& res) {
    const int W = 50;
    std::cout << clrHeader("\n" + repeat('=', W)) << "\n";
    std::cout << clrHeader("  PERFORMANCE STATISTICS – " + res.algorithmName) << "\n";
    std::cout << clrHeader(repeat('=', W)) << "\n";

    auto row = [&](const std::string& label, const std::string& val) {
        std::cout << "  " << clrMenu(padRight(label, 28)) << ": " << clrBold(val) << "\n";
    };
    row("Avg Waiting Time",    fmt2(res.avgWaitingTime)    + " units");
    row("Avg Turnaround Time", fmt2(res.avgTurnaroundTime) + " units");
    row("Avg Response Time",   fmt2(res.avgResponseTime)   + " units");
    row("CPU Utilization",     fmt2(res.cpuUtilization)    + " %");
    row("CPU Idle Time",       std::to_string(res.idleTime)+ " units");
    row("Throughput",          fmt2(res.throughput)        + " proc/unit");
    row("Jain Fairness Index", fmt2(res.jainFairnessIndex) + "  (1.0=fair)");
    row("Context Switches",    std::to_string(res.contextSwitches));
    row("Total Execution Time",std::to_string(res.totalTime) + " units");
    row("Longest Waiting PID", res.longestWaitingPID);
    row("Shortest Waiting PID",res.shortestWaitingPID);
    std::cout << clrHeader(repeat('=', W)) << "\n\n";
}

// --- Print the scheduling decision log ---
inline void printSchedulingLog(const std::vector<SchedulingLog>& log) {
    std::cout << clrHeader("\n===== SCHEDULING DECISION LOG =====\n");
    for (const auto& e : log)
        std::cout << clrMenu("  Time " + std::to_string(e.time))
                  << "\n  Selected: " << clrRunning(e.pid)
                  << "\n  Reason:   " << e.reason
                  << "\n  " << repeat('-', 34) << "\n";
    std::cout << "\n";
}

// --- Generate random processes for quick testing ---
inline std::vector<Process> generateRandomProcesses(int count) {
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

// --- Input helpers ---
inline bool isPIDUnique(const std::string& pid, const std::vector<Process>& procs) {
    for (const auto& p : procs) if (p.pid == pid) return false;
    return true;
}
inline bool readInt(const std::string& prompt, int& out,
                    int minVal = INT32_MIN, int maxVal = INT32_MAX) {
    std::cout << clrMenu(prompt);
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    try {
        std::size_t pos;
        int val = std::stoi(line, &pos);
        if (pos != line.size() || val < minVal || val > maxVal) return false;
        out = val; return true;
    } catch (...) { return false; }
}
inline bool readString(const std::string& prompt, std::string& out) {
    std::cout << clrMenu(prompt);
    if (!std::getline(std::cin, out)) return false;
    size_t s = out.find_first_not_of(" \t"), e = out.find_last_not_of(" \t");
    if (s == std::string::npos) return false;
    out = out.substr(s, e - s + 1);
    return !out.empty();
}
inline void pressEnter(const std::string& msg = "Press [Enter] to continue...") {
    std::cout << clrDim("\n  " + msg);
    std::string dummy; std::getline(std::cin, dummy);
}
inline void clearScreen() {
    if (g_colorsEnabled) std::cout << ANSI::CLEAR_SCREEN << std::flush;
    else                 std::cout << "\n" << repeat('=', 60) << "\n";
}

// --- Cross-platform sleep ---
inline void sleepMs(int ms) {
    if (ms <= 0) return;
#ifdef _WIN32
    Sleep(static_cast<DWORD>(ms));
#else
    struct timespec ts{ ms / 1000, (ms % 1000) * 1000000 };
    nanosleep(&ts, nullptr);
#endif
}
