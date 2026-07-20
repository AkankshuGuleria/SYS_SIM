// ============================================================
//  CPU Scheduling Simulator — DSA Project
//
//  DSA CONCEPTS:
//    struct        — data bundles (Process, GanttEntry, etc.)
//    enum class    — process states (NEW, READY, RUNNING...)
//    vector<>      — dynamic array
//    queue<int>    — FIFO queue used by Round Robin
//    deque<int>    — double-ended queue used by MLFQ
//    class         — base class + 7 subclasses (inheritance)
//    virtual func  — polymorphism (each algo overrides run())
//    stable_sort   — sort by arrival time
//    linear search — find min burst / min priority each tick
//
//  BUILD:
//    g++ -std=c++17 -o scheduler.exe main.cpp
//  RUN:
//    ./scheduler.exe
// ============================================================

#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <deque>
#include <algorithm>
#include <iomanip>
#include <random>
#include <chrono>
#include <climits>
#include <memory>
#include <numeric>
#include <sstream>

using namespace std;

// ============================================================
//  SECTION 1: DATA STRUCTURES
// ============================================================

// Enum: the 5 possible states a process can be in
enum class State { NEW, READY, RUNNING, WAITING, TERMINATED };

// Struct: one block in the Gantt chart
struct GanttEntry {
    string pid;      // process ID or "Idle"
    int start, end;  // time range this block covers
};

// Struct: one scheduling decision recorded in the log
struct LogEntry {
    int    time;
    string pid;
    string reason;
};

// Struct: the main process data object
struct Process {
    string pid;

    // --- inputs (given by user) ---
    int arrivalTime  = 0;  // when the process arrives
    int burstTime    = 0;  // total CPU time it needs
    int priority     = 0;  // lower = higher urgency

    // --- updated during simulation ---
    int remainingTime  = 0;
    int startTime      = -1;  // -1 means not started yet
    int completionTime = 0;

    // --- computed after simulation ---
    int waitingTime    = 0;  // time spent in ready queue
    int turnaroundTime = 0;  // completionTime - arrivalTime
    int responseTime   = 0;  // startTime - arrivalTime

    State state = State::NEW;

    // Reset before each run
    void reset() {
        remainingTime  = burstTime;
        startTime      = -1;
        completionTime = 0;
        waitingTime    = 0;
        turnaroundTime = 0;
        responseTime   = 0;
        state          = State::NEW;
    }

    string stateStr() const {
        switch (state) {
            case State::NEW:        return "NEW";
            case State::READY:      return "READY";
            case State::RUNNING:    return "RUNNING";
            case State::WAITING:    return "WAITING";
            case State::TERMINATED: return "DONE";
        }
        return "?";
    }
};

// Struct: result returned by every algorithm
struct Result {
    string              algorithmName;
    vector<Process>     processes;
    vector<GanttEntry>  gantt;
    vector<LogEntry>    log;

    // Statistics
    double avgWT = 0, avgTAT = 0, avgRT = 0;
    double cpuUtil = 0;
    int    contextSwitches = 0, totalTime = 0, idleTime = 0;
};

// Struct: simulation settings
struct Settings {
    int timeQuantum = 2;  // used by Round Robin and MLFQ
};

// ============================================================
//  SECTION 2: COMPUTE STATISTICS
//  Called after the algorithm finishes to fill result fields.
// ============================================================
void finalize(Result& res) {
    // Compute per-process times
    for (int i = 0; i < (int)res.processes.size(); i++) {
        Process& p = res.processes[i];
        p.turnaroundTime = p.completionTime - p.arrivalTime;
        p.waitingTime    = max(0, p.turnaroundTime - p.burstTime);
        p.responseTime   = (p.startTime >= 0) ? max(0, p.startTime - p.arrivalTime) : 0;
    }

    if (!res.gantt.empty()) res.totalTime = res.gantt.back().end;

    // Count idle time from Gantt
    res.idleTime = 0;
    for (int i = 0; i < (int)res.gantt.size(); i++)
        if (res.gantt[i].pid == "Idle") res.idleTime += res.gantt[i].end - res.gantt[i].start;

    // Count context switches (non-idle process changes)
    res.contextSwitches = 0;
    string prev = "";
    for (int i = 0; i < (int)res.gantt.size(); i++) {
        if (res.gantt[i].pid != "Idle" && !prev.empty() && res.gantt[i].pid != prev)
            res.contextSwitches++;
        if (res.gantt[i].pid != "Idle") prev = res.gantt[i].pid;
    }

    // Compute averages
    double sumWT = 0, sumTAT = 0, sumRT = 0;
    int n = (int)res.processes.size();
    for (int i = 0; i < n; i++) {
        sumWT  += res.processes[i].waitingTime;
        sumTAT += res.processes[i].turnaroundTime;
        sumRT  += res.processes[i].responseTime;
    }
    res.avgWT  = sumWT  / n;
    res.avgTAT = sumTAT / n;
    res.avgRT  = sumRT  / n;
    res.cpuUtil = (res.totalTime > 0) ?
        100.0 * (res.totalTime - res.idleTime) / res.totalTime : 0;
}

// ============================================================
//  SECTION 3: PRINT RESULTS
// ============================================================

// Print the process table
void printTable(const vector<Process>& procs) {
    cout << "\n";
    cout << string(70, '-') << "\n";
    cout << left
         << setw(6)  << "PID"
         << setw(8)  << "Arr"
         << setw(7)  << "Burst"
         << setw(6)  << "Pri"
         << setw(7)  << "Start"
         << setw(7)  << "Done"
         << setw(6)  << "WT"
         << setw(6)  << "TAT"
         << setw(6)  << "RT"
         << "State" << "\n";
    cout << string(70, '-') << "\n";
    for (int i = 0; i < (int)procs.size(); i++) {
        const Process& p = procs[i];
        cout << left
             << setw(6)  << p.pid
             << setw(8)  << p.arrivalTime
             << setw(7)  << p.burstTime
             << setw(6)  << p.priority
             << setw(7)  << p.startTime
             << setw(7)  << p.completionTime
             << setw(6)  << p.waitingTime
             << setw(6)  << p.turnaroundTime
             << setw(6)  << p.responseTime
             << p.stateStr() << "\n";
    }
    cout << string(70, '-') << "\n";
}

// Print the Gantt chart
void printGantt(const vector<GanttEntry>& gantt) {
    if (gantt.empty()) { cout << "(empty)\n"; return; }
    cout << "\nGantt Chart:\n";

    // Top border
    for (int i = 0; i < (int)gantt.size(); i++) {
        int w = max(4, (int)gantt[i].pid.size() + 2);
        cout << "+" << string(w, '-');
    }
    cout << "+\n";

    // Process labels
    for (int i = 0; i < (int)gantt.size(); i++) {
        int w = max(4, (int)gantt[i].pid.size() + 2);
        int pad = w - (int)gantt[i].pid.size();
        int lpad = pad / 2;
        cout << "|" << string(lpad, ' ') << gantt[i].pid << string(pad - lpad, ' ');
    }
    cout << "|\n";

    // Bottom border
    for (int i = 0; i < (int)gantt.size(); i++) {
        int w = max(4, (int)gantt[i].pid.size() + 2);
        cout << "+" << string(w, '-');
    }
    cout << "+\n";

    // Time markers
    cout << gantt.front().start;
    int prevLen = (int)to_string(gantt.front().start).size();
    for (int i = 0; i < (int)gantt.size(); i++) {
        int w = max(4, (int)gantt[i].pid.size() + 2) + 1;
        string t = to_string(gantt[i].end);
        int spaces = max(0, w - prevLen - (int)t.size());
        cout << string(spaces, ' ') << t;
        prevLen = (int)t.size();
    }
    cout << "\n";
}

// Print statistics
void printStats(const Result& res) {
    cout << "\n--- Statistics: " << res.algorithmName << " ---\n";
    cout << "Avg Waiting Time   : " << fixed << setprecision(2) << res.avgWT    << " units\n";
    cout << "Avg Turnaround Time: " << res.avgTAT   << " units\n";
    cout << "Avg Response Time  : " << res.avgRT    << " units\n";
    cout << "CPU Utilization    : " << res.cpuUtil  << " %\n";
    cout << "Context Switches   : " << res.contextSwitches << "\n";
    cout << "Total Time         : " << res.totalTime << " units\n";
}

// Print scheduling decision log
void printLog(const vector<LogEntry>& log) {
    cout << "\n--- Scheduling Decision Log ---\n";
    for (int i = 0; i < (int)log.size(); i++)
        cout << "  t=" << log[i].time << "  " << log[i].pid << "  | " << log[i].reason << "\n";
}

// ============================================================
//  SECTION 4: UTILITY FUNCTIONS
// ============================================================

void pressEnter() {
    cout << "\nPress Enter to continue...";
    string dummy; getline(cin, dummy);
}

void clearScreen() {
    cout << string(50, '\n');
}

bool readInt(const string& prompt, int& out, int lo = INT_MIN, int hi = INT_MAX) {
    cout << prompt;
    string line;
    if (!getline(cin, line)) return false;
    try {
        size_t pos;
        int v = stoi(line, &pos);
        if (pos != line.size() || v < lo || v > hi) return false;
        out = v; return true;
    } catch (...) { return false; }
}

bool readStr(const string& prompt, string& out) {
    cout << prompt;
    if (!getline(cin, out)) return false;
    size_t s = out.find_first_not_of(" \t"), e = out.find_last_not_of(" \t");
    if (s == string::npos) return false;
    out = out.substr(s, e - s + 1);
    return !out.empty();
}

bool pidUnique(const string& pid, const vector<Process>& procs) {
    for (int i = 0; i < (int)procs.size(); i++)
        if (procs[i].pid == pid) return false;
    return true;
}

// Generate random processes for quick testing
vector<Process> generateRandom(int count) {
    mt19937 rng((unsigned)chrono::steady_clock::now().time_since_epoch().count());
    uniform_int_distribution<int> arr(0, 10), burst(1, 15), pri(1, 10);
    vector<Process> procs;
    for (int i = 1; i <= count; i++) {
        Process p;
        p.pid           = "P" + to_string(i);
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
//  SECTION 5: ABSTRACT BASE CLASS — Scheduler
//
//  DSA: Inheritance + Polymorphism
//  All 7 algorithms extend this class and override run().
// ============================================================
class Scheduler {
public:
    explicit Scheduler(const string& name) : name_(name) {}
    virtual ~Scheduler() = default;

    // Pure virtual: every subclass must provide its own run()
    virtual Result run(vector<Process> procs, const Settings& s) = 0;

    const string& name() const { return name_; }

protected:
    string name_;
};

// ============================================================
//  SECTION 6: ALGORITHM — FCFS (First Come First Serve)
//
//  Rule: serve in arrival order, no interruption once started.
//  DSA: vector, stable_sort
// ============================================================
class FCFS : public Scheduler {
public:
    FCFS() : Scheduler("FCFS (First Come First Serve)") {}

    Result run(vector<Process> procs, const Settings& s) override {
        (void)s;
        Result res;
        res.algorithmName = name_;
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        // Sort by arrival time
        stable_sort(procs.begin(), procs.end(),
            [](const Process& a, const Process& b){ return a.arrivalTime < b.arrivalTime; });

        int time = 0, n = (int)procs.size();

        for (int i = 0; i < n; i++) {
            // Idle gap if CPU has to wait for next process
            if (time < procs[i].arrivalTime) {
                res.gantt.push_back({ "Idle", time, procs[i].arrivalTime });
                time = procs[i].arrivalTime;
            }
            // Mark other arrived processes READY
            for (int j = i + 1; j < n; j++)
                if (procs[j].arrivalTime <= time && procs[j].state == State::NEW)
                    procs[j].state = State::READY;

            Process& p = procs[i];
            p.state     = State::RUNNING;
            p.startTime = time;
            res.log.push_back({ time, p.pid, "Arrived earliest (AT=" + to_string(p.arrivalTime) + ")" });
            res.gantt.push_back({ p.pid, time, time + p.burstTime });
            time            += p.burstTime;
            p.completionTime = time;
            p.state          = State::TERMINATED;
        }

        res.processes = procs;
        finalize(res);
        return res;
    }
};

// ============================================================
//  SECTION 7: ALGORITHM — SJF (Shortest Job First, Non-Preemptive)
//
//  Rule: pick arrived process with smallest burst time.
//  DSA: vector, linear search for minimum
// ============================================================
class SJF : public Scheduler {
public:
    SJF() : Scheduler("SJF (Shortest Job First - Non-Preemptive)") {}

    Result run(vector<Process> procs, const Settings& s) override {
        (void)s;
        Result res;
        res.algorithmName = name_;
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        int n = (int)procs.size(), time = 0, completed = 0;

        while (completed < n) {
            // Linear search: find arrived process with smallest burst
            int bestIdx = -1, bestBurst = INT_MAX;
            for (int i = 0; i < n; i++)
                if (procs[i].arrivalTime <= time && procs[i].state != State::TERMINATED)
                    if (procs[i].burstTime < bestBurst) {
                        bestBurst = procs[i].burstTime;
                        bestIdx   = i;
                    }

            // CPU idle: jump to next arrival
            if (bestIdx == -1) {
                int next = INT_MAX;
                for (int i = 0; i < n; i++)
                    if (procs[i].state == State::NEW) next = min(next, procs[i].arrivalTime);
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
            res.log.push_back({ time, p.pid, "Shortest Burst = " + to_string(p.burstTime) });
            res.gantt.push_back({ p.pid, time, time + p.remainingTime });
            time            += p.remainingTime;
            p.completionTime = time;
            p.remainingTime  = 0;
            p.state          = State::TERMINATED;
            completed++;
        }

        res.processes = procs;
        finalize(res);
        return res;
    }
};

// ============================================================
//  SECTION 8: ALGORITHM — SRTF (Shortest Remaining Time First)
//
//  Rule: preemptive SJF — every tick pick the process with
//        the least remaining time.
//  DSA: vector, linear search, two-variable Gantt tracking
// ============================================================
class SRTF : public Scheduler {
public:
    SRTF() : Scheduler("SRTF (Shortest Remaining Time First - Preemptive)") {}

    Result run(vector<Process> procs, const Settings& s) override {
        (void)s;
        Result res;
        res.algorithmName = name_;
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        int n = (int)procs.size(), completed = 0;
        int time = INT_MAX;
        for (int i = 0; i < n; i++) time = min(time, procs[i].arrivalTime);

        int prevIdx = -1, blockStart = time;

        while (completed < n) {
            // Linear search: find arrived process with minimum remaining time
            int bestIdx = -1, bestRem = INT_MAX;
            for (int i = 0; i < n; i++) {
                if (procs[i].arrivalTime <= time &&
                    procs[i].state != State::TERMINATED &&
                    procs[i].remainingTime > 0 &&
                    procs[i].remainingTime < bestRem) {
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
                        next = min(next, procs[i].arrivalTime);
                if (next == INT_MAX) break;
                if (res.gantt.empty() || res.gantt.back().pid != "Idle")
                    res.gantt.push_back({ "Idle", time, time + 1 });
                else res.gantt.back().end = time + 1;
                time++; blockStart = time;
                continue;
            }

            Process& cur = procs[bestIdx];
            if (cur.startTime == -1) cur.startTime = time;

            // Context switch: close old Gantt block, open new one
            if (bestIdx != prevIdx) {
                if (prevIdx != -1) res.gantt.push_back({ procs[prevIdx].pid, blockStart, time });
                else if (blockStart < time) res.gantt.push_back({ "Idle", blockStart, time });
                blockStart = time; prevIdx = bestIdx;
                res.log.push_back({ time, cur.pid, "Shortest Remaining = " + to_string(cur.remainingTime) });
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
//  SECTION 9: ALGORITHM — Priority Non-Preemptive (+ Aging)
//
//  Rule: pick highest-priority arrived process (lowest number).
//        Run to completion. Aging prevents starvation.
//  DSA: vector<int> for shadow priorities, linear search
// ============================================================
class PriorityNP : public Scheduler {
public:
    PriorityNP() : Scheduler("Priority Non-Preemptive (+ Aging)") {}
    static const int AGING_INTERVAL = 4;

    Result run(vector<Process> procs, const Settings& s) override {
        (void)s;
        Result res;
        res.algorithmName = name_;
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        int n = (int)procs.size(), time = 0, completed = 0;

        // Shadow priority array (keeps original values unchanged in output)
        vector<int> eff(n), waitSince(n, -1);
        for (int i = 0; i < n; i++) eff[i] = procs[i].priority;

        while (completed < n) {
            // Mark arrived processes READY
            for (int i = 0; i < n; i++)
                if (procs[i].arrivalTime <= time && procs[i].state == State::NEW) {
                    procs[i].state = State::READY; waitSince[i] = time;
                }

            // Aging: boost priority of processes waiting too long
            for (int i = 0; i < n; i++) {
                if (procs[i].state != State::READY) continue;
                int waited = time - waitSince[i];
                if (waited > 0 && waited % AGING_INTERVAL == 0) {
                    int before = eff[i];
                    eff[i] = max(1, eff[i] - 1);
                    if (eff[i] < before)
                        res.log.push_back({ time, procs[i].pid,
                            "Aged: " + to_string(before) + "->" + to_string(eff[i]) });
                }
            }

            // Linear search: find READY process with best effective priority
            int bestIdx = -1, bestPri = INT_MAX;
            for (int i = 0; i < n; i++)
                if (procs[i].state == State::READY && eff[i] < bestPri) {
                    bestPri = eff[i]; bestIdx = i;
                }

            // CPU idle
            if (bestIdx == -1) {
                int next = INT_MAX;
                for (int i = 0; i < n; i++)
                    if (procs[i].state == State::NEW) next = min(next, procs[i].arrivalTime);
                if (next == INT_MAX) break;
                res.gantt.push_back({ "Idle", time, next });
                time = next; continue;
            }

            // Run to completion
            Process& p = procs[bestIdx];
            p.state = State::RUNNING; p.startTime = time;
            res.log.push_back({ time, p.pid, "Priority = " + to_string(eff[bestIdx]) });
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
//  SECTION 10: ALGORITHM — Priority Preemptive
//
//  Rule: every tick pick highest-priority process.
//        Higher-priority arrival preempts current process.
//  DSA: vector, linear search, Gantt block tracking
// ============================================================
class PriorityP : public Scheduler {
public:
    PriorityP() : Scheduler("Priority Preemptive (Highest Priority First)") {}

    Result run(vector<Process> procs, const Settings& s) override {
        (void)s;
        Result res;
        res.algorithmName = name_;
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        int n = (int)procs.size(), completed = 0;
        int time = INT_MAX;
        for (int i = 0; i < n; i++) time = min(time, procs[i].arrivalTime);
        int prevIdx = -1, blockStart = time;

        while (completed < n) {
            // Linear search: find arrived process with highest priority (lowest number)
            int bestIdx = -1, bestPri = INT_MAX;
            for (int i = 0; i < n; i++)
                if (procs[i].arrivalTime <= time &&
                    procs[i].state != State::TERMINATED &&
                    procs[i].remainingTime > 0 &&
                    procs[i].priority < bestPri) {
                    bestPri = procs[i].priority; bestIdx = i;
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
                        next = min(next, procs[i].arrivalTime);
                if (next == INT_MAX) break;
                if (res.gantt.empty() || res.gantt.back().pid != "Idle")
                    res.gantt.push_back({ "Idle", time, time + 1 });
                else res.gantt.back().end = time + 1;
                time++; blockStart = time; continue;
            }

            Process& cur = procs[bestIdx];
            if (cur.startTime == -1) cur.startTime = time;

            // Context switch
            if (bestIdx != prevIdx) {
                if (prevIdx != -1) res.gantt.push_back({ procs[prevIdx].pid, blockStart, time });
                else if (blockStart < time) res.gantt.push_back({ "Idle", blockStart, time });
                blockStart = time; prevIdx = bestIdx;
                res.log.push_back({ time, cur.pid, "Higher Priority = " + to_string(cur.priority) });
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
//  SECTION 11: ALGORITHM — Round Robin
//
//  Rule: each process gets a fixed time slice (quantum).
//        If not done, goes back to end of queue.
//  DSA: queue<int> (FIFO), vector, stable_sort
// ============================================================
class RoundRobin : public Scheduler {
public:
    RoundRobin() : Scheduler("Round Robin") {}

    Result run(vector<Process> procs, const Settings& s) override {
        Result res;
        res.algorithmName = name_ + " (Q=" + to_string(s.timeQuantum) + ")";
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        int n = (int)procs.size(), time = 0, completed = 0;
        int quantum = s.timeQuantum;

        // Sort indices by arrival time
        vector<int> order(n);
        for (int i = 0; i < n; i++) order[i] = i;
        stable_sort(order.begin(), order.end(),
            [&](int a, int b){ return procs[a].arrivalTime < procs[b].arrivalTime; });

        // DSA: queue<int> — FIFO ready queue storing process indices
        queue<int> readyQ;
        int nextCheck = 0;

        // Seed with processes arriving at time 0
        while (nextCheck < n && procs[order[nextCheck]].arrivalTime <= time) {
            int idx = order[nextCheck++];
            readyQ.push(idx); procs[idx].state = State::READY;
        }

        while (completed < n) {
            // Queue empty — CPU idle
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

            // Dequeue next process (FIFO)
            int idx = readyQ.front(); readyQ.pop();
            Process& cur = procs[idx];
            cur.state = State::RUNNING;
            if (cur.startTime == -1) cur.startTime = time;
            res.log.push_back({ time, cur.pid, "Round Robin turn (Q=" + to_string(quantum) + ")" });

            int startT = time, ticks = quantum;

            // Run for up to quantum ticks
            while (ticks > 0 && cur.remainingTime > 0) {
                cur.remainingTime--; time++; ticks--;
                // Enqueue processes that arrived during this quantum
                while (nextCheck < n && procs[order[nextCheck]].arrivalTime <= time) {
                    int ni = order[nextCheck++];
                    readyQ.push(ni); procs[ni].state = State::READY;
                }
            }
            res.gantt.push_back({ cur.pid, startT, time });

            if (cur.remainingTime == 0) {
                cur.state = State::TERMINATED; cur.completionTime = time; completed++;
            } else {
                // Not done — go back to end of queue (round robin)
                cur.state = State::READY; readyQ.push(idx);
            }
        }

        res.processes = procs;
        finalize(res);
        return res;
    }
};

// ============================================================
//  SECTION 12: ALGORITHM — MLFQ (Multi-Level Feedback Queue)
//
//  Three queues:
//    Q0 (highest): quantum = Q0
//    Q1 (medium):  quantum = Q0*2
//    Q2 (lowest):  FCFS (runs to completion)
//  Demotion: exhaust quantum -> drop one level
//  Promotion (Aging): wait too long in Q1/Q2 -> boost one level
//  DSA: deque<int>[3] (3 FIFO queues), vector<int>
// ============================================================
class MLFQ : public Scheduler {
public:
    MLFQ() : Scheduler("MLFQ (3-level Feedback + Aging)") {}

    Result run(vector<Process> procs, const Settings& s) override {
        Result res;
        res.algorithmName = name_ + " [Q=" + to_string(s.timeQuantum) + "]";
        for (int i = 0; i < (int)procs.size(); i++) procs[i].reset();

        int n = (int)procs.size(), time = 0, completed = 0;
        int Q0 = s.timeQuantum, Q1 = Q0 * 2, agingThresh = Q0 * 4;

        // DSA: vector<int> — which queue level each process is in
        vector<int> level(n, 0);
        // DSA: vector<int> — age ticks accumulated in Q1/Q2
        vector<int> age(n, 0);
        // DSA: deque<int>[3] — three FIFO queues storing process indices
        deque<int> q[3];

        // Sort by arrival time
        vector<int> order(n);
        for (int i = 0; i < n; i++) order[i] = i;
        stable_sort(order.begin(), order.end(),
            [&](int a, int b){ return procs[a].arrivalTime < procs[b].arrivalTime; });
        int nextCheck = 0;

        // Add newly arrived processes to Q0
        auto enqueue = [&]() {
            while (nextCheck < n && procs[order[nextCheck]].arrivalTime <= time) {
                int idx = order[nextCheck++];
                procs[idx].state = State::READY;
                q[0].push_back(idx);
            }
        };

        // Promote processes that waited too long in Q1/Q2
        auto doAging = [&]() {
            for (int lv = 1; lv <= 2; lv++) {
                deque<int> kept;
                for (int idx : q[lv]) {
                    age[idx]++;
                    if (age[idx] >= agingThresh) {
                        age[idx] = 0; level[idx] = lv - 1;
                        q[lv - 1].push_back(idx);
                        res.log.push_back({ time, procs[idx].pid,
                            "Aged: Q" + to_string(lv) + "->Q" + to_string(lv-1) });
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

            int idx = q[chosen].front(); q[chosen].pop_front();
            Process& cur = procs[idx];
            cur.state = State::RUNNING;
            if (cur.startTime == -1) cur.startTime = time;
            age[idx] = 0;

            // Q2 gets unlimited time (FCFS), others get their quantum
            int quantum = (chosen == 0) ? Q0 : (chosen == 1) ? Q1 : cur.remainingTime;
            res.log.push_back({ time, cur.pid,
                "Q" + to_string(chosen) + " quantum=" + to_string(quantum) });

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
                        "Demoted Q" + to_string(chosen) + "->Q" + to_string(chosen+1) });
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
//  SECTION 13: MENU / MAIN
// ============================================================

void addProcess(vector<Process>& procs) {
    Process p;
    while (true) {
        if (!readStr("  PID (e.g. P1): ", p.pid)) { cout << "  Invalid.\n"; continue; }
        if (!pidUnique(p.pid, procs)) { cout << "  PID already exists.\n"; continue; }
        break;
    }
    while (!readInt("  Arrival Time (0-1000): ", p.arrivalTime, 0, 1000))
        cout << "  Must be 0-1000.\n";
    while (!readInt("  Burst Time (1-1000): ", p.burstTime, 1, 1000))
        cout << "  Must be 1-1000.\n";
    while (!readInt("  Priority (1-100, lower = higher urgency): ", p.priority, 1, 100))
        cout << "  Must be 1-100.\n";
    p.remainingTime = p.burstTime;
    p.state         = State::NEW;
    procs.push_back(p);
    cout << "  Process " << p.pid << " added.\n";
    pressEnter();
}

void runAlgorithm(vector<Process>& procs, Scheduler* algo, const Settings& s) {
    if (procs.empty()) {
        cout << "\n  No processes loaded. Add or generate some first.\n";
        pressEnter(); return;
    }
    cout << "\n  Running " << algo->name() << "...\n";
    Result res = algo->run(procs, s);

    printTable(res.processes);
    printGantt(res.gantt);
    printStats(res);

    cout << "\n  Show decision log? (y/n): ";
    string ans; getline(cin, ans);
    if (!ans.empty() && ans[0] == 'y') printLog(res.log);

    pressEnter();
}

int main() {
    vector<Process> processes;
    Settings settings;

    while (true) {
        clearScreen();
        cout << "============================================\n";
        cout << "   CPU Scheduling Simulator — DSA Project\n";
        cout << "============================================\n";
        cout << "  Processes: " << processes.size() << "\n";
        cout << "  Time Quantum (RR/MLFQ): " << settings.timeQuantum << "\n\n";

        cout << "  -- Input --\n";
        cout << "  1. Add Process Manually\n";
        cout << "  2. Generate 5 Random Processes\n";
        cout << "  3. Generate 10 Random Processes\n";
        cout << "  4. Clear All Processes\n";
        cout << "  5. View Process Table\n\n";

        cout << "  -- Run Algorithm --\n";
        cout << "  6.  FCFS\n";
        cout << "  7.  SJF (Shortest Job First)\n";
        cout << "  8.  SRTF (Shortest Remaining Time)\n";
        cout << "  9.  Priority Non-Preemptive\n";
        cout << "  10. Priority Preemptive\n";
        cout << "  11. Round Robin\n";
        cout << "  12. MLFQ\n\n";

        cout << "  13. Change Time Quantum\n";
        cout << "  0.  Exit\n\n";
        cout << "  Choice: ";

        string choice; getline(cin, choice);

        if      (choice == "0") { cout << "Goodbye!\n"; break; }
        else if (choice == "1") { addProcess(processes); }
        else if (choice == "2") { processes = generateRandom(5);  cout << "  5 processes generated.\n";  pressEnter(); }
        else if (choice == "3") { processes = generateRandom(10); cout << "  10 processes generated.\n"; pressEnter(); }
        else if (choice == "4") { processes.clear(); cout << "  Cleared.\n"; pressEnter(); }
        else if (choice == "5") {
            if (processes.empty()) cout << "  No processes.\n";
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
                cout << "  Invalid.\n";
            pressEnter();
        }
        else { cout << "  Invalid choice.\n"; pressEnter(); }
    }
    return 0;
}
