// ============================================================
//  CPU Scheduling Studio — Single File DSA Project
//
//  DSA CONCEPTS USED IN THIS FILE:
//    struct        — data bundles (Process, GanttEntry, etc.)
//    enum class    — named constants for process states
//    vector<>      — dynamic array (STL)
//    queue<int>    — FIFO queue for Round Robin (STL)
//    deque<int>    — double-ended queue for MLFQ (STL)
//    class         — OOP: base class + 7 subclasses (inheritance)
//    virtual func  — polymorphism (each algorithm overrides run())
//    stable_sort   — sorting algorithm (STL)
//    linear search — find min burst / min priority each tick
//
//  BUILD:
//    g++ -std=c++17 -O2 -o scheduler.exe main.cpp
// ============================================================

#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <deque>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>
#include <climits>
#include <memory>
#include <numeric>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================
//  SECTION 1: ANSI COLORS
//  Using escape codes to print colored text in the terminal.
// ============================================================
bool g_colors = true;   // global flag to toggle colors on/off

// Wrap text in an ANSI color code; strip if colors are off
std::string colorize(const std::string& code, const std::string& text) {
    if (!g_colors) return text;
    return code + text + "\033[0m";
}
std::string clrGreen (const std::string& s) { return colorize("\033[92m", s); }
std::string clrBlue  (const std::string& s) { return colorize("\033[94m", s); }
std::string clrYellow(const std::string& s) { return colorize("\033[93m", s); }
std::string clrGray  (const std::string& s) { return colorize("\033[90m", s); }
std::string clrRed   (const std::string& s) { return colorize("\033[91m", s); }
std::string clrCyan  (const std::string& s) { return colorize("\033[36m",  s); }
std::string clrBold  (const std::string& s) { return colorize("\033[1m",   s); }
std::string clrHeader(const std::string& s) { return colorize("\033[1m\033[96m", s); }
std::string clrDim   (const std::string& s) { return colorize("\033[2m",   s); }

// Enable ANSI colors on Windows terminal
void enableColors() {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | 0x0004);
#endif
}

// ============================================================
//  SECTION 2: DATA STRUCTURES
//  These structs define the core data model for the simulation.
// ============================================================

// Enum: the 5 possible states of a process in its lifecycle
enum class State { NEW, READY, RUNNING, WAITING, TERMINATED };

// Struct: one block on the Gantt chart (who ran, from when to when)
struct GanttEntry {
    std::string pid;    // "P1", "P2", or "Idle"
    int start, end;     // time range
};

// Struct: one line in the scheduling decision log
struct LogEntry {
    int time;
    std::string pid;
    std::string reason;
};

// Struct: the main process data object
// Input fields set by the user; output fields computed by the simulation
struct Process {
    std::string pid;

    // --- inputs (given by user) ---
    int arrivalTime    = 0;   // time when process arrives
    int burstTime      = 0;   // total CPU time needed
    int priority       = 0;   // lower number = higher urgency

    // --- runtime (updated during simulation) ---
    int remainingTime  = 0;   // burst time still left
    int startTime      = -1;  // first tick on CPU (-1 = not started)
    int completionTime = 0;   // tick when process finished

    // --- results (computed after simulation) ---
    int waitingTime    = 0;   // time spent in ready queue
    int turnaroundTime = 0;   // completionTime - arrivalTime
    int responseTime   = 0;   // startTime - arrivalTime

    State state = State::NEW;

    // Reset before each simulation run
    void reset() {
        remainingTime  = burstTime;
        startTime      = -1;
        completionTime = 0;
        waitingTime    = 0;
        turnaroundTime = 0;
        responseTime   = 0;
        state          = State::NEW;
    }

    // Convert state enum to a readable string
    std::string stateStr() const {
        switch (state) {
            case State::NEW:        return "NEW";
            case State::READY:      return "READY";
            case State::RUNNING:    return "RUNNING";
            case State::WAITING:    return "WAITING";
            case State::TERMINATED: return "TERMINATED";
        }
        return "?";
    }
};

// Struct: result returned by every algorithm after it runs
struct Result {
    std::string              algorithmName;
    std::vector<Process>     processes;       // final state of each process
    std::vector<GanttEntry>  gantt;           // timeline of execution
    std::vector<LogEntry>    log;             // scheduling decisions made

    // Aggregate statistics
    double avgWT = 0, avgTAT = 0, avgRT = 0;
    double cpuUtil = 0, throughput = 0;
    int    contextSwitches = 0, totalTime = 0, idleTime = 0;
};

// Struct: user-configurable settings
struct Settings {
    int  timeQuantum = 2;   // for Round Robin and MLFQ
    bool showLog     = false;
};

// ============================================================
//  SECTION 3: UTILITY FUNCTIONS
//  String formatting and console output helpers.
// ============================================================

// Pad string to a fixed width (left-align)
std::string padR(const std::string& s, int w) {
    if ((int)s.size() >= w) return s.substr(0, w);
    return s + std::string(w - s.size(), ' ');
}
// Pad string to a fixed width (right-align)
std::string padL(const std::string& s, int w) {
    if ((int)s.size() >= w) return s.substr(0, w);
    return std::string(w - s.size(), ' ') + s;
}
// Center a string in a field of given width
std::string center(const std::string& s, int w) {
    if ((int)s.size() >= w) return s.substr(0, w);
    int pad = w - (int)s.size(), left = pad / 2;
    return std::string(left, ' ') + s + std::string(pad - left, ' ');
}
// Repeat a character n times
std::string rep(char c, int n) { return (n > 0) ? std::string(n, c) : ""; }
// Format a double to 2 decimal places
std::string f2(double v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << v;
    return ss.str();
}

// Wait for user to press Enter
void pressEnter() {
    std::cout << clrDim("\n  Press [Enter] to continue...");
    std::string dummy; std::getline(std::cin, dummy);
}
// Clear the terminal screen
void clrScreen() { std::cout << "\033[2J\033[H" << std::flush; }

// Safely read an integer from the user within a valid range
bool readInt(const std::string& prompt, int& out, int lo = INT_MIN, int hi = INT_MAX) {
    std::cout << clrCyan(prompt);
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    try {
        std::size_t pos;
        int v = std::stoi(line, &pos);
        if (pos != line.size() || v < lo || v > hi) return false;
        out = v; return true;
    } catch (...) { return false; }
}
// Read a non-empty string from the user
bool readStr(const std::string& prompt, std::string& out) {
    std::cout << clrCyan(prompt);
    if (!std::getline(std::cin, out)) return false;
    size_t s = out.find_first_not_of(" \t"), e = out.find_last_not_of(" \t");
    if (s == std::string::npos) return false;
    out = out.substr(s, e - s + 1);
    return !out.empty();
}
// Check if a PID is not already used
bool pidUnique(const std::string& pid, const std::vector<Process>& procs) {
    for (int i = 0; i < (int)procs.size(); i++)
        if (procs[i].pid == pid) return false;
    return true;
}

// Generate random processes for testing
std::vector<Process> generateRandom(int count) {
    std::mt19937 rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> arr(0, 10), burst(1, 15), pri(1, 10);
    std::vector<Process> procs;
    for (int i = 1; i <= count; i++) {
        Process p;
        p.pid           = "P" + std::to_string(i);
        p.arrivalTime   = arr(rng);
        p.burstTime     = burst(rng);
        p.remainingTime = p.burstTime;
        p.priority      = pri(rng);
        p.state         = State::NEW;
        procs.push_back(p);
    }
    return procs;
}

// ============================================================
//  SECTION 4: COMPUTE STATISTICS
//  Called after the algorithm finishes to fill result fields.
// ============================================================
void computeStats(Result& res) {
    if (res.processes.empty()) return;
    double sumWT = 0, sumTAT = 0, sumRT = 0, sumTATsq = 0;
    int maxWT = -1, minWT = INT_MAX;

    for (int i = 0; i < (int)res.processes.size(); i++) {
        Process& p = res.processes[i];
        sumWT  += p.waitingTime;
        sumTAT += p.turnaroundTime;
        sumRT  += p.responseTime;
        sumTATsq += (double)p.turnaroundTime * p.turnaroundTime;
        if (p.waitingTime > maxWT) maxWT = p.waitingTime;
        if (p.waitingTime < minWT) minWT = p.waitingTime;
    }

    int n = (int)res.processes.size();
    res.avgWT  = sumWT  / n;
    res.avgTAT = sumTAT / n;
    res.avgRT  = sumRT  / n;
    res.cpuUtil     = (res.totalTime > 0) ? 100.0 * (res.totalTime - res.idleTime) / res.totalTime : 0;
    res.throughput  = (res.totalTime > 0) ? (double)n / res.totalTime : 0;

    // Count context switches: non-idle process changes in Gantt
    int ctx = 0;
    std::string prev = "";
    for (int i = 0; i < (int)res.gantt.size(); i++) {
        if (res.gantt[i].pid != "Idle" && !prev.empty() && res.gantt[i].pid != prev) ctx++;
        if (res.gantt[i].pid != "Idle") prev = res.gantt[i].pid;
    }
    res.contextSwitches = ctx;

    // Sum idle time from Gantt
    int idle = 0;
    for (int i = 0; i < (int)res.gantt.size(); i++)
        if (res.gantt[i].pid == "Idle") idle += res.gantt[i].end - res.gantt[i].start;
    res.idleTime = idle;
}

// Finalize per-process output fields (WT, TAT, RT) and call computeStats
void finalize(Result& res) {
    for (int i = 0; i < (int)res.processes.size(); i++) {
        Process& p = res.processes[i];
        p.turnaroundTime = p.completionTime - p.arrivalTime;
        p.waitingTime    = std::max(0, p.turnaroundTime - p.burstTime);
        p.responseTime   = (p.startTime >= 0) ? std::max(0, p.startTime - p.arrivalTime) : 0;
    }
    if (!res.gantt.empty()) res.totalTime = res.gantt.back().end;
    computeStats(res);
}

// ============================================================
//  SECTION 5: PRINT RESULTS
//  Display process table, Gantt chart, and statistics.
// ============================================================

// Print colour-coded process table
void printTable(const std::vector<Process>& procs) {
    std::cout << clrHeader(rep('-', 78)) << "\n";
    std::cout << clrHeader(" " + padR("PID",4) + " | " + padR("Arr",4) + " | " +
        padR("Burst",5) + " | " + padR("Pri",4) + " | " + padR("Start",5) + " | " +
        padR("Done",5) + " | " + padR("WT",4) + " | " + padR("TAT",4) + " | " +
        padR("RT",4) + " | State") << "\n";
    std::cout << clrHeader(rep('-', 78)) << "\n";

    for (int i = 0; i < (int)procs.size(); i++) {
        const Process& p = procs[i];
        std::string row =
            " " + padR(p.pid, 4)                           + " | " +
            padR(std::to_string(p.arrivalTime),  4)        + " | " +
            padR(std::to_string(p.burstTime),    5)        + " | " +
            padR(std::to_string(p.priority),     4)        + " | " +
            padR(std::to_string(p.startTime),    5)        + " | " +
            padR(std::to_string(p.completionTime),5)       + " | " +
            padR(std::to_string(p.waitingTime),  4)        + " | " +
            padR(std::to_string(p.turnaroundTime),4)       + " | " +
            padR(std::to_string(p.responseTime), 4)        + " | " +
            p.stateStr();

        if      (p.state == State::RUNNING)    std::cout << clrGreen(row);
        else if (p.state == State::READY)      std::cout << clrBlue(row);
        else if (p.state == State::WAITING)    std::cout << clrYellow(row);
        else if (p.state == State::TERMINATED) std::cout << clrGray(row);
        else                                    std::cout << row;
        std::cout << "\n";
    }
    std::cout << clrHeader(rep('-', 78)) << "\n";
}

// Print Gantt chart with time markers
void printGantt(const std::vector<GanttEntry>& gantt) {
    if (gantt.empty()) { std::cout << "(empty)\n"; return; }

    // Top border
    std::string top = "+";
    for (int i = 0; i < (int)gantt.size(); i++) {
        int w = std::max(4, (int)gantt[i].pid.size() + 2);
        top += rep('-', w) + "+";
    }
    std::cout << "\n" << clrCyan(top) << "\n";

    // Process labels
    std::string labels = "|";
    for (int i = 0; i < (int)gantt.size(); i++) {
        int w = std::max(4, (int)gantt[i].pid.size() + 2);
        std::string cell = center(gantt[i].pid, w);
        labels += (gantt[i].pid == "Idle" ? clrGray(cell) : clrGreen(cell)) + "|";
    }
    std::cout << labels << "\n" << clrCyan(top) << "\n";

    // Time markers below each block
    std::string times = std::to_string(gantt.front().start);
    int prevLen = (int)times.size();
    for (int i = 0; i < (int)gantt.size(); i++) {
        int cellW = std::max(4, (int)gantt[i].pid.size() + 2) + 1;
        std::string t = std::to_string(gantt[i].end);
        int spaces = std::max(0, cellW - prevLen - (int)t.size());
        times += std::string(spaces, ' ') + t;
        prevLen = (int)t.size();
    }
    std::cout << clrGray(times) << "\n\n";
}

// Print aggregate statistics
void printStats(const Result& res) {
    std::cout << clrHeader("\n" + rep('=', 50)) << "\n";
    std::cout << clrHeader("  STATISTICS – " + res.algorithmName) << "\n";
    std::cout << clrHeader(rep('=', 50)) << "\n";
    auto row = [&](const std::string& label, const std::string& val) {
        std::cout << "  " << clrCyan(padR(label, 28)) << ": " << clrBold(val) << "\n";
    };
    row("Avg Waiting Time",    f2(res.avgWT)    + " units");
    row("Avg Turnaround Time", f2(res.avgTAT)   + " units");
    row("Avg Response Time",   f2(res.avgRT)    + " units");
    row("CPU Utilization",     f2(res.cpuUtil)  + " %");
    row("Context Switches",    std::to_string(res.contextSwitches));
    row("Total Time",          std::to_string(res.totalTime) + " units");
    std::cout << clrHeader(rep('=', 50)) << "\n\n";
}

// Print the scheduling decision log
void printLog(const std::vector<LogEntry>& log) {
    std::cout << clrHeader("\n===== SCHEDULING DECISION LOG =====\n");
    for (int i = 0; i < (int)log.size(); i++)
        std::cout << clrCyan("  t=" + std::to_string(log[i].time))
                  << "  " << clrGreen(log[i].pid)
                  << "  | " << clrGray(log[i].reason) << "\n";
    std::cout << "\n";
}

// ============================================================
//  SECTION 6: ABSTRACT BASE CLASS — Scheduler
//
//  DSA: Inheritance + Polymorphism
//  All 7 algorithms extend this class and override run().
// ============================================================
class Scheduler {
public:
    explicit Scheduler(const std::string& name) : name_(name) {}
    virtual ~Scheduler() = default;

    // Pure virtual: every subclass must provide its own run()
    virtual Result run(std::vector<Process> procs, const Settings& s) = 0;

    const std::string& name() const { return name_; }

protected:
    std::string name_;
};

// ============================================================
//  SECTION 7: ALGORITHM — FCFS (First Come First Serve)
//
//  Rule: serve processes in arrival order, no interruption.
//  DSA: vector, stable_sort
// ============================================================
class FCFS : public Scheduler {
public:
    FCFS() : Scheduler("FCFS (First Come First Serve)") {}

    Result run(std::vector<Process> procs, const Settings& s) override {
        (void)s; // Settings not used by FCFS
        Result res;
        res.algorithmName = name_;
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        // Sort by arrival time
        std::stable_sort(procs.begin(), procs.end(),
            [](const Process& a, const Process& b){ return a.arrivalTime < b.arrivalTime; });

        int time = 0, n = (int)procs.size();

        for (int i = 0; i < n; i++) {
            // Insert idle gap if CPU has to wait
            if (time < procs[i].arrivalTime) {
                res.gantt.push_back({ "Idle", time, procs[i].arrivalTime });
                time = procs[i].arrivalTime;
            }
            // Mark other arrived processes READY
            for (int j = i + 1; j < n; j++)
                if (procs[j].arrivalTime <= time && procs[j].state == State::NEW)
                    procs[j].state = State::READY;

            // Run this process to completion
            Process& p = procs[i];
            p.state     = State::RUNNING;
            p.startTime = time;
            res.log.push_back({ time, p.pid, "Arrived earliest (AT=" + std::to_string(p.arrivalTime) + ")" });
            res.gantt.push_back({ p.pid, time, time + p.burstTime });
            time               += p.burstTime;
            p.completionTime    = time;
            p.state             = State::TERMINATED;
        }

        res.processes = procs;
        finalize(res);
        return res;
    }
};

// ============================================================
//  SECTION 8: ALGORITHM — SJF (Shortest Job First, Non-Preemptive)
//
//  Rule: at each scheduling point, pick the arrived process
//        with the smallest burst time.
//  DSA: vector, linear search for minimum
// ============================================================
class SJF : public Scheduler {
public:
    SJF() : Scheduler("SJF (Shortest Job First - Non-Preemptive)") {}

    Result run(std::vector<Process> procs, const Settings& s) override {
        (void)s; // Settings not used by SJF
        Result res;
        res.algorithmName = name_;
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        int n = (int)procs.size(), time = 0, completed = 0;

        while (completed < n) {
            // Linear search: find arrived process with smallest burst
            int bestIdx = -1, bestBurst = INT_MAX;
            for (int i = 0; i < n; i++) {
                if (procs[i].arrivalTime <= time && procs[i].state != State::TERMINATED)
                    if (procs[i].burstTime < bestBurst) {
                        bestBurst = procs[i].burstTime;
                        bestIdx   = i;
                    }
            }
            // CPU idle: jump to next arrival
            if (bestIdx == -1) {
                int next = INT_MAX;
                for (int i = 0; i < n; i++)
                    if (procs[i].state == State::NEW) next = std::min(next, procs[i].arrivalTime);
                if (next == INT_MAX) break;
                res.gantt.push_back({ "Idle", time, next });
                time = next;
                continue;
            }
            // Mark others READY
            for (int i = 0; i < n; i++)
                if (i != bestIdx && procs[i].arrivalTime <= time && procs[i].state == State::NEW)
                    procs[i].state = State::READY;

            Process& p = procs[bestIdx];
            p.state = State::RUNNING; p.startTime = time;
            res.log.push_back({ time, p.pid, "Shortest Burst = " + std::to_string(p.burstTime) });
            res.gantt.push_back({ p.pid, time, time + p.remainingTime });
            time             += p.remainingTime;
            p.completionTime  = time;
            p.remainingTime   = 0;
            p.state           = State::TERMINATED;
            completed++;
        }

        res.processes = procs;
        finalize(res);
        return res;
    }
};

// ============================================================
//  SECTION 9: ALGORITHM — SRTF (Shortest Remaining Time First)
//
//  Rule: preemptive SJF — every tick, pick the process with
//        the least remaining burst. Preempts if shorter arrives.
//  DSA: vector, linear search, two-variable Gantt block tracking
// ============================================================
class SRTF : public Scheduler {
public:
    SRTF() : Scheduler("SRTF (Shortest Remaining Time First - Preemptive)") {}

    Result run(std::vector<Process> procs, const Settings& s) override {
        (void)s; // Settings not used by SRTF
        Result res;
        res.algorithmName = name_;
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        int n = (int)procs.size(), completed = 0;
        // Start at earliest arrival
        int time = INT_MAX;
        for (int i = 0; i < n; i++) time = std::min(time, procs[i].arrivalTime);

        int prevIdx = -1, blockStart = time;  // track current Gantt block

        while (completed < n) {
            // Linear search: find arrived process with minimum remaining time
            int bestIdx = -1, bestRem = INT_MAX;
            for (int i = 0; i < n; i++) {
                bool arrived = procs[i].arrivalTime <= time;
                bool notDone = procs[i].state != State::TERMINATED;
                bool hasWork = procs[i].remainingTime > 0;
                if (arrived && notDone && hasWork && procs[i].remainingTime < bestRem) {
                    bestRem = procs[i].remainingTime;
                    bestIdx = i;
                }
            }
            // CPU idle
            if (bestIdx == -1) {
                if (prevIdx != -1) {
                    res.gantt.push_back({ procs[prevIdx].pid, blockStart, time });
                    prevIdx = -1; blockStart = time;
                }
                int next = INT_MAX;
                for (int i = 0; i < n; i++)
                    if (procs[i].state != State::TERMINATED && procs[i].arrivalTime > time)
                        next = std::min(next, procs[i].arrivalTime);
                if (next == INT_MAX) break;
                if (res.gantt.empty() || res.gantt.back().pid != "Idle")
                    res.gantt.push_back({ "Idle", time, time + 1 });
                else res.gantt.back().end = time + 1;
                time++; blockStart = time;
                continue;
            }

            Process& cur = procs[bestIdx];
            if (cur.startTime == -1) cur.startTime = time;

            // Detect context switch — close old block, open new one
            if (bestIdx != prevIdx) {
                if (prevIdx != -1) res.gantt.push_back({ procs[prevIdx].pid, blockStart, time });
                else if (blockStart < time) res.gantt.push_back({ "Idle", blockStart, time });
                blockStart = time; prevIdx = bestIdx;
                res.log.push_back({ time, cur.pid, "Shortest Remaining = " + std::to_string(cur.remainingTime) });
            }

            for (int i = 0; i < n; i++)
                if (procs[i].arrivalTime <= time && procs[i].state == State::NEW && procs[i].remainingTime > 0)
                    procs[i].state = State::READY;
            cur.state = State::RUNNING;

            cur.remainingTime--; time++;

            if (cur.remainingTime == 0) {
                cur.state = State::TERMINATED; cur.completionTime = time;
                res.gantt.push_back({ cur.pid, blockStart, time });
                blockStart = time; prevIdx = -1; completed++;
            } else { cur.state = State::READY; }
        }

        res.processes = procs;
        finalize(res);
        return res;
    }
};

// ============================================================
//  SECTION 10: ALGORITHM — Priority Non-Preemptive (+ Aging)
//
//  Rule: pick the highest-priority arrived process (lowest number).
//        Run to completion (non-preemptive).
//  Aging: every AGING_INTERVAL ticks, boost priority of waiting
//         processes so they eventually run (prevents starvation).
//  DSA: vector<int> for shadow priorities, linear search
// ============================================================
class PriorityNP : public Scheduler {
public:
    PriorityNP() : Scheduler("Priority Non-Preemptive (+ Aging)") {}

    static const int AGING_INTERVAL = 4;

    Result run(std::vector<Process> procs, const Settings& s) override {
        (void)s; // Settings not used by Priority NP
        Result res;
        res.algorithmName = name_;
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        int n = (int)procs.size(), time = 0, completed = 0;

        // Shadow priority array — keeps original priorities unchanged in output
        std::vector<int> eff(n);
        std::vector<int> waitSince(n, -1);
        for (int i = 0; i < n; i++) eff[i] = procs[i].priority;

        while (completed < n) {
            // Mark arrived processes READY
            for (int i = 0; i < n; i++)
                if (procs[i].arrivalTime <= time && procs[i].state == State::NEW) {
                    procs[i].state = State::READY; waitSince[i] = time;
                }

            // Aging pass: boost priority of long-waiting processes
            for (int i = 0; i < n; i++) {
                if (procs[i].state != State::READY) continue;
                int waited = time - waitSince[i];
                if (waited > 0 && waited % AGING_INTERVAL == 0) {
                    int before = eff[i];
                    eff[i] = std::max(1, eff[i] - 1);
                    if (eff[i] < before)
                        res.log.push_back({ time, procs[i].pid,
                            "Aged: priority " + std::to_string(before) + " -> " + std::to_string(eff[i]) });
                }
            }

            // Linear search: find READY process with best (lowest) effective priority
            int bestIdx = -1, bestPri = INT_MAX;
            for (int i = 0; i < n; i++) {
                if (procs[i].state == State::READY && eff[i] < bestPri) {
                    bestPri = eff[i]; bestIdx = i;
                }
            }
            // CPU idle
            if (bestIdx == -1) {
                int next = INT_MAX;
                for (int i = 0; i < n; i++)
                    if (procs[i].state == State::NEW) next = std::min(next, procs[i].arrivalTime);
                if (next == INT_MAX) break;
                res.gantt.push_back({ "Idle", time, next });
                time = next; continue;
            }

            // Run to completion
            Process& p = procs[bestIdx];
            p.state = State::RUNNING; p.startTime = time;
            res.log.push_back({ time, p.pid, "Priority = " + std::to_string(eff[bestIdx]) });
            int startT = time;
            while (p.remainingTime > 0) {
                for (int i = 0; i < n; i++)
                    if (procs[i].arrivalTime <= time && procs[i].state == State::NEW) {
                        procs[i].state = State::READY; waitSince[i] = time;
                    }
                p.remainingTime--; time++;
            }
            p.state = State::TERMINATED; p.completionTime = time;
            res.gantt.push_back({ p.pid, startT, time });
            completed++;
        }

        res.processes = procs;
        finalize(res);
        return res;
    }
};

// ============================================================
//  SECTION 11: ALGORITHM — Priority Preemptive
//
//  Rule: same as Priority NP but re-evaluated every tick.
//        A higher-priority arrival preempts the running process.
//  DSA: vector, linear search, two-variable Gantt block tracking
// ============================================================
class PriorityP : public Scheduler {
public:
    PriorityP() : Scheduler("Priority Preemptive (Highest Priority First)") {}

    Result run(std::vector<Process> procs, const Settings& s) override {
        (void)s; // Settings not used by Priority Preemptive
        Result res;
        res.algorithmName = name_;
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        int n = (int)procs.size(), completed = 0;
        int time = INT_MAX;
        for (int i = 0; i < n; i++) time = std::min(time, procs[i].arrivalTime);
        int prevIdx = -1, blockStart = time;

        while (completed < n) {
            // Linear search: find arrived process with highest priority (lowest number)
            int bestIdx = -1, bestPri = INT_MAX;
            for (int i = 0; i < n; i++) {
                bool arrived = procs[i].arrivalTime <= time;
                bool notDone = procs[i].state != State::TERMINATED;
                bool hasWork = procs[i].remainingTime > 0;
                if (arrived && notDone && hasWork && procs[i].priority < bestPri) {
                    bestPri = procs[i].priority; bestIdx = i;
                }
            }
            // CPU idle
            if (bestIdx == -1) {
                if (prevIdx != -1) {
                    res.gantt.push_back({ procs[prevIdx].pid, blockStart, time });
                    prevIdx = -1; blockStart = time;
                }
                int next = INT_MAX;
                for (int i = 0; i < n; i++)
                    if (procs[i].state != State::TERMINATED && procs[i].arrivalTime > time)
                        next = std::min(next, procs[i].arrivalTime);
                if (next == INT_MAX) break;
                if (res.gantt.empty() || res.gantt.back().pid != "Idle")
                    res.gantt.push_back({ "Idle", time, time + 1 });
                else res.gantt.back().end = time + 1;
                time++; blockStart = time; continue;
            }

            Process& cur = procs[bestIdx];
            if (cur.startTime == -1) cur.startTime = time;

            // Context switch detected — close old block, open new
            if (bestIdx != prevIdx) {
                if (prevIdx != -1) res.gantt.push_back({ procs[prevIdx].pid, blockStart, time });
                else if (blockStart < time) res.gantt.push_back({ "Idle", blockStart, time });
                blockStart = time; prevIdx = bestIdx;
                res.log.push_back({ time, cur.pid, "Higher Priority = " + std::to_string(cur.priority) });
            }

            for (int i = 0; i < n; i++)
                if (procs[i].arrivalTime <= time && procs[i].state == State::NEW)
                    procs[i].state = State::READY;
            cur.state = State::RUNNING;

            cur.remainingTime--; time++;

            if (cur.remainingTime == 0) {
                cur.state = State::TERMINATED; cur.completionTime = time;
                res.gantt.push_back({ cur.pid, blockStart, time });
                blockStart = time; prevIdx = -1; completed++;
            } else { cur.state = State::READY; }
        }

        res.processes = procs;
        finalize(res);
        return res;
    }
};

// ============================================================
//  SECTION 12: ALGORITHM — Round Robin
//
//  Rule: each process gets a fixed time slice (quantum).
//        Processes take turns; if not done, go back to end of queue.
//  DSA: queue<int> (FIFO ready queue), vector, stable_sort
// ============================================================
class RoundRobin : public Scheduler {
public:
    RoundRobin() : Scheduler("Round Robin") {}

    Result run(std::vector<Process> procs, const Settings& s) override {
        Result res;
        res.algorithmName = name_ + " (Q=" + std::to_string(s.timeQuantum) + ")";
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        int n = (int)procs.size(), time = 0, completed = 0;
        int quantum = s.timeQuantum;

        // Sort indices by arrival time for tracking new arrivals
        std::vector<int> order(n);
        for (int i = 0; i < n; i++) order[i] = i;
        std::stable_sort(order.begin(), order.end(),
            [&](int a, int b){ return procs[a].arrivalTime < procs[b].arrivalTime; });

        // DSA: queue<int> — FIFO ready queue (stores process indices)
        std::queue<int> readyQ;
        int nextCheck = 0;

        // Seed queue with processes arriving at time 0
        while (nextCheck < n && procs[order[nextCheck]].arrivalTime <= time) {
            int idx = order[nextCheck++];
            readyQ.push(idx); procs[idx].state = State::READY;
        }

        while (completed < n) {
            // Queue empty — CPU idle, jump to next arrival
            if (readyQ.empty()) {
                if (nextCheck >= n) break;
                int nextArr = procs[order[nextCheck]].arrivalTime;
                res.gantt.push_back({ "Idle", time, nextArr });
                time = nextArr;
                while (nextCheck < n && procs[order[nextCheck]].arrivalTime <= time) {
                    int idx = order[nextCheck++];
                    readyQ.push(idx); procs[idx].state = State::READY;
                }
                continue;
            }

            // Dequeue the next process (FIFO)
            int idx = readyQ.front(); readyQ.pop();
            Process& cur = procs[idx];
            cur.state = State::RUNNING;
            if (cur.startTime == -1) cur.startTime = time;
            res.log.push_back({ time, cur.pid, "Round Robin turn (Q=" + std::to_string(quantum) + ")" });

            int startT = time, ticks = quantum;

            // Run for up to quantum ticks
            while (ticks > 0 && cur.remainingTime > 0) {
                cur.remainingTime--; time++; ticks--;
                // Enqueue any process that arrived during this quantum
                while (nextCheck < n && procs[order[nextCheck]].arrivalTime <= time) {
                    int ni = order[nextCheck++];
                    readyQ.push(ni); procs[ni].state = State::READY;
                }
            }
            res.gantt.push_back({ cur.pid, startT, time });

            if (cur.remainingTime == 0) {
                cur.state = State::TERMINATED; cur.completionTime = time; completed++;
            } else {
                // Not done — go back to end of queue
                cur.state = State::READY; readyQ.push(idx);
            }
        }

        res.processes = procs;
        finalize(res);
        return res;
    }
};

// ============================================================
//  SECTION 13: ALGORITHM — MLFQ (Multi-Level Feedback Queue)
//
//  Three queues with different priorities and time quanta:
//    Q0 (highest): quantum = Q0 ticks
//    Q1 (medium):  quantum = Q0 * 2 ticks
//    Q2 (lowest):  FCFS (runs to completion)
//  Demotion:  exhaust quantum → drop to next queue
//  Promotion: wait too long in Q1/Q2 → boost one level (aging)
//  DSA: deque<int>[3] (3 FIFO queues), vector<int> (level + age)
// ============================================================
class MLFQ : public Scheduler {
public:
    MLFQ() : Scheduler("MLFQ (3-level Feedback + Aging)") {}

    Result run(std::vector<Process> procs, const Settings& s) override {
        Result res;
        res.algorithmName = name_ + " [Q=" + std::to_string(s.timeQuantum) + "]";
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        int n = (int)procs.size(), time = 0, completed = 0;
        int Q0 = s.timeQuantum;
        int Q1 = s.timeQuantum * 2;
        int agingThresh = s.timeQuantum * 4;

        // DSA: vector<int> — which queue level each process is in (0, 1, or 2)
        std::vector<int> level(n, 0);
        // DSA: vector<int> — age ticks accumulated waiting in Q1/Q2
        std::vector<int> age(n, 0);
        // DSA: deque<int>[3] — three FIFO queues storing process indices
        std::deque<int> q[3];

        // Sort by arrival time for tracking new arrivals
        std::vector<int> order(n);
        for (int i = 0; i < n; i++) order[i] = i;
        std::stable_sort(order.begin(), order.end(),
            [&](int a, int b){ return procs[a].arrivalTime < procs[b].arrivalTime; });
        int nextCheck = 0;

        // Add newly arrived processes to Q0
        auto enqueue = [&]() {
            while (nextCheck < n && procs[order[nextCheck]].arrivalTime <= time) {
                int idx = order[nextCheck++];
                procs[idx].state = State::READY;
                q[0].push_back(idx);  // all new processes start in Q0
            }
        };

        // Promote processes that have waited too long in Q1/Q2
        auto doAging = [&]() {
            for (int lv = 1; lv <= 2; lv++) {
                std::deque<int> kept;
                for (int idx : q[lv]) {
                    age[idx]++;
                    if (age[idx] >= agingThresh) {
                        age[idx] = 0; level[idx] = lv - 1;
                        q[lv - 1].push_back(idx);   // promote one level
                        res.log.push_back({ time, procs[idx].pid,
                            "Aged: Q" + std::to_string(lv) + " -> Q" + std::to_string(lv-1) });
                    } else { kept.push_back(idx); }
                }
                q[lv] = kept;
            }
        };

        enqueue();

        while (completed < n) {
            // Find highest-priority non-empty queue
            int chosen = -1;
            for (int lv = 0; lv <= 2; lv++) if (!q[lv].empty()) { chosen = lv; break; }

            // All queues empty — CPU idle
            if (chosen == -1) {
                if (nextCheck >= n) break;
                int nextArr = procs[order[nextCheck]].arrivalTime;
                res.gantt.push_back({ "Idle", time, nextArr });
                time = nextArr; enqueue(); continue;
            }

            // Dequeue from chosen level
            int idx = q[chosen].front(); q[chosen].pop_front();
            Process& cur = procs[idx];
            cur.state = State::RUNNING;
            if (cur.startTime == -1) cur.startTime = time;
            age[idx] = 0;  // reset age when on CPU

            // Q2 gets unlimited time (FCFS); others get their quantum
            int quantum = (chosen == 0) ? Q0 : (chosen == 1) ? Q1 : cur.remainingTime;
            res.log.push_back({ time, cur.pid,
                "Q" + std::to_string(chosen) + " quantum=" + std::to_string(quantum) });

            int startT = time, done = 0;
            while (done < quantum && cur.remainingTime > 0) {
                cur.remainingTime--; time++; done++;
                enqueue(); doAging();
            }
            res.gantt.push_back({ cur.pid, startT, time });

            if (cur.remainingTime == 0) {
                cur.state = State::TERMINATED; cur.completionTime = time; completed++;
            } else {
                // Demote if quantum exhausted
                if (chosen < 2) {
                    level[idx] = chosen + 1;
                    res.log.push_back({ time, cur.pid,
                        "Demoted Q" + std::to_string(chosen) + " -> Q" + std::to_string(chosen+1) });
                }
                cur.state = State::READY;
                q[level[idx]].push_back(idx);
            }
        }

        res.processes = procs;
        finalize(res);
        return res;
    }
};

// ============================================================
//  SECTION 14: MENU / MAIN PROGRAM
//  User interface: add processes, pick an algorithm, see results.
// ============================================================

// Add a process manually from user input
void addProcess(std::vector<Process>& procs) {
    Process p;
    while (true) {
        if (!readStr("  PID (e.g. P1): ", p.pid)) { std::cout << clrRed("  Invalid.\n"); continue; }
        if (!pidUnique(p.pid, procs)) { std::cout << clrRed("  PID already exists.\n"); continue; }
        break;
    }
    while (!readInt("  Arrival Time (0-1000): ", p.arrivalTime, 0, 1000))
        std::cout << clrRed("  Must be 0-1000.\n");
    while (!readInt("  Burst Time (1-1000): ", p.burstTime, 1, 1000))
        std::cout << clrRed("  Must be 1-1000.\n");
    while (!readInt("  Priority (1-100, lower=higher urgency): ", p.priority, 1, 100))
        std::cout << clrRed("  Must be 1-100.\n");
    p.remainingTime = p.burstTime;
    p.state         = State::NEW;
    procs.push_back(p);
    std::cout << clrGreen("  ✓ Process " + p.pid + " added.\n");
    pressEnter();
}

// Run a chosen algorithm and display results
void runAlgorithm(std::vector<Process>& procs, Scheduler* algo, const Settings& s) {
    if (procs.empty()) {
        std::cout << clrRed("\n  No processes. Add or generate some first.\n");
        pressEnter(); return;
    }

    std::cout << clrHeader("\n  Running " + algo->name() + "...\n");
    Result res = algo->run(procs, s);

    std::cout << "\n";
    printTable(res.processes);
    printGantt(res.gantt);
    printStats(res);

    std::cout << clrCyan("  Show decision log? [y/N]: ");
    std::string ans; std::getline(std::cin, ans);
    if (!ans.empty() && (ans[0] == 'y' || ans[0] == 'Y'))
        printLog(res.log);

    pressEnter();
}

int main() {
    enableColors();

    // Application state
    std::vector<Process> processes;
    Settings settings;

    // ---- Main loop ----
    while (true) {
        clrScreen();
        std::cout << clrHeader(rep('=', 50)) << "\n";
        std::cout << clrHeader("   CPU SCHEDULING STUDIO — DSA Project") << "\n";
        std::cout << clrHeader(rep('=', 50)) << "\n";
        std::cout << "  Processes loaded: " << clrBold(std::to_string(processes.size())) << "\n";
        std::cout << "  Time Quantum (RR/MLFQ): " << clrBold(std::to_string(settings.timeQuantum)) << "\n\n";

        std::cout << clrCyan("  --- Input ---\n");
        std::cout << "   1. Add Process Manually\n";
        std::cout << "   2. Generate  5 Random Processes\n";
        std::cout << "   3. Generate 10 Random Processes\n";
        std::cout << "   4. Clear All Processes\n";
        std::cout << "   5. View Process Table\n\n";

        std::cout << clrCyan("  --- Run Algorithm ---\n");
        std::cout << "   6. FCFS (First Come First Serve)\n";
        std::cout << "   7. SJF  (Shortest Job First)\n";
        std::cout << "   8. SRTF (Shortest Remaining Time First)\n";
        std::cout << "   9. Priority Non-Preemptive (+ Aging)\n";
        std::cout << "  10. Priority Preemptive\n";
        std::cout << "  11. Round Robin\n";
        std::cout << "  12. MLFQ (Multi-Level Feedback Queue)\n\n";

        std::cout << clrCyan("  --- Other ---\n");
        std::cout << "  13. Change Time Quantum\n";
        std::cout << "   0. Exit\n\n";
        std::cout << clrCyan("  Choice: ");

        std::string choice; std::getline(std::cin, choice);

        if      (choice == "0") { std::cout << clrHeader("\n  Goodbye!\n\n"); break; }
        else if (choice == "1") { clrScreen(); addProcess(processes); }
        else if (choice == "2") { processes = generateRandom(5);
            std::cout << clrGreen("  ✓ 5 random processes generated.\n"); pressEnter(); }
        else if (choice == "3") { processes = generateRandom(10);
            std::cout << clrGreen("  ✓ 10 random processes generated.\n"); pressEnter(); }
        else if (choice == "4") { processes.clear();
            std::cout << clrGreen("  ✓ Cleared.\n"); pressEnter(); }
        else if (choice == "5") {
            clrScreen();
            if (processes.empty()) std::cout << clrGray("  (No processes)\n");
            else printTable(processes);
            pressEnter();
        }
        else if (choice == "6")  { FCFS      a; runAlgorithm(processes, &a, settings); }
        else if (choice == "7")  { SJF       a; runAlgorithm(processes, &a, settings); }
        else if (choice == "8")  { SRTF      a; runAlgorithm(processes, &a, settings); }
        else if (choice == "9")  { PriorityNP a; runAlgorithm(processes, &a, settings); }
        else if (choice == "10") { PriorityP  a; runAlgorithm(processes, &a, settings); }
        else if (choice == "11") { RoundRobin a; runAlgorithm(processes, &a, settings); }
        else if (choice == "12") { MLFQ       a; runAlgorithm(processes, &a, settings); }
        else if (choice == "13") {
            while (!readInt("  New quantum (1-100): ", settings.timeQuantum, 1, 100))
                std::cout << clrRed("  Invalid.\n");
            pressEnter();
        }
        else { std::cout << clrRed("  Invalid choice.\n"); pressEnter(); }
    }

    return 0;
}
