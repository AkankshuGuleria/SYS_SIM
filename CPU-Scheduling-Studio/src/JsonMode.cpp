#include "../include/JsonMode.h"
#include "../include/Scheduler.h"
#include "../include/Process.h"
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <sstream>

// Forward declarations of factories
std::unique_ptr<Scheduler> makeFCFS();
std::unique_ptr<Scheduler> makeSJF();
std::unique_ptr<Scheduler> makeSRTF();
std::unique_ptr<Scheduler> makePriority();
std::unique_ptr<Scheduler> makePriorityPreemptive();
std::unique_ptr<Scheduler> makeRoundRobin();
std::unique_ptr<Scheduler> makeMLFQ();

// Struct representing a process state snapshot at a tick
struct JsonProcState {
    std::string pid;
    std::string state;
    int remainingTime;
    int arrivalTime;
    int burstTime;
    int priority;
    int startTime;
    int completionTime;
    int waitingTime;
    int turnaroundTime;
    int responseTime;
};

// Struct representing a simulation tick snapshot
struct JsonTick {
    int time;
    std::string cpuPid;
    std::vector<std::string> readyQueue;
    std::vector<std::string> waitingQueue;
    std::vector<JsonProcState> processes;
    std::vector<GanttEntry> gantt;
    std::vector<SchedulingLog> log;
};

// Simple utility to escape strings for JSON
std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

int runJsonMode() {
    // Read inputs:
    // Line 1: Algorithm Code (e.g. FCFS, SJF, SRTF, PRIORITY, PRIORITY_PREEMPTIVE, RR)
    // Line 2: Time Quantum (integer)
    // Line 3: Process Count (integer)
    // Next N lines: PID ArrivalTime BurstTime Priority
    std::string algoCode;
    if (!(std::cin >> algoCode)) return 1;

    int quantum = 2;
    if (!(std::cin >> quantum)) return 1;

    int procCount = 0;
    if (!(std::cin >> procCount)) return 1;

    std::vector<Process> processes;
    for (int i = 0; i < procCount; ++i) {
        Process p;
        if (!(std::cin >> p.pid >> p.arrivalTime >> p.burstTime >> p.priority)) {
            break;
        }
        p.remainingTime = p.burstTime;
        p.state = ProcessState::NEW;
        processes.push_back(p);
    }

    // Set settings
    AppSettings settings;
    settings.animationSpeedMs = 0;
    settings.stepMode = false;
    settings.colorsEnabled = false;
    settings.darkTheme = true;
    settings.timeQuantum = quantum;

    // Pick scheduler
    std::unique_ptr<Scheduler> scheduler;
    if (algoCode == "FCFS") scheduler = makeFCFS();
    else if (algoCode == "SJF") scheduler = makeSJF();
    else if (algoCode == "SRTF") scheduler = makeSRTF();
    else if (algoCode == "PRIORITY") scheduler = makePriority();
    else if (algoCode == "PRIORITY_PREEMPTIVE") scheduler = makePriorityPreemptive();
    else if (algoCode == "RR") scheduler = makeRoundRobin();
    else if (algoCode == "MLFQ") scheduler = makeMLFQ();
    else {
        std::cout << "{\"error\": \"Invalid algorithm code: " << jsonEscape(algoCode) << "\"}\n";
        return 1;
    }

    // Vector to collect all tick snapshots
    std::vector<JsonTick> ticks;

    // Tick callback
    auto onTick = [&](int time,
                      const std::vector<Process>& snapshot,
                      const std::vector<GanttEntry>& gantt,
                      const std::vector<SchedulingLog>& log)
    {
        JsonTick tick;
        tick.time = time;
        tick.cpuPid = "";
        
        for (const auto& p : snapshot) {
            if (p.state == ProcessState::RUNNING) {
                tick.cpuPid = p.pid;
            }
            if (p.state == ProcessState::READY) {
                tick.readyQueue.push_back(p.pid);
            }
            if (p.state == ProcessState::WAITING) {
                tick.waitingQueue.push_back(p.pid);
            }

            JsonProcState jps;
            jps.pid = p.pid;
            jps.state = p.stateStr();
            jps.remainingTime = p.remainingTime;
            jps.arrivalTime = p.arrivalTime;
            jps.burstTime = p.burstTime;
            jps.priority = p.priority;
            jps.startTime = p.startTime;
            jps.completionTime = p.completionTime;
            jps.waitingTime = p.waitingTime;
            jps.turnaroundTime = p.turnaroundTime;
            jps.responseTime = p.responseTime;
            tick.processes.push_back(jps);
        }
        tick.gantt = gantt;
        tick.log = log;

        ticks.push_back(tick);
    };

    // Run simulation
    SimulationResult result = scheduler->run(processes, settings, onTick);

    // Build the final JSON output
    std::ostringstream json;
    json << "{\n";
    json << "  \"algorithmName\": \"" << jsonEscape(result.algorithmName) << "\",\n";
    
    // Ticks array
    json << "  \"ticks\": [\n";
    for (size_t t = 0; t < ticks.size(); ++t) {
        const auto& tick = ticks[t];
        json << "    {\n";
        json << "      \"time\": " << tick.time << ",\n";
        json << "      \"cpuPid\": \"" << jsonEscape(tick.cpuPid) << "\",\n";
        
        // Ready Queue
        json << "      \"readyQueue\": [";
        for (size_t r = 0; r < tick.readyQueue.size(); ++r) {
            json << "\"" << jsonEscape(tick.readyQueue[r]) << "\"";
            if (r + 1 < tick.readyQueue.size()) json << ", ";
        }
        json << "],\n";

        // Waiting Queue
        json << "      \"waitingQueue\": [";
        for (size_t w = 0; w < tick.waitingQueue.size(); ++w) {
            json << "\"" << jsonEscape(tick.waitingQueue[w]) << "\"";
            if (w + 1 < tick.waitingQueue.size()) json << ", ";
        }
        json << "],\n";

        // Process states inside tick
        json << "      \"processes\": [\n";
        for (size_t p = 0; p < tick.processes.size(); ++p) {
            const auto& jp = tick.processes[p];
            json << "        {\n";
            json << "          \"pid\": \"" << jsonEscape(jp.pid) << "\",\n";
            json << "          \"state\": \"" << jsonEscape(jp.state) << "\",\n";
            json << "          \"remainingTime\": " << jp.remainingTime << ",\n";
            json << "          \"arrivalTime\": " << jp.arrivalTime << ",\n";
            json << "          \"burstTime\": " << jp.burstTime << ",\n";
            json << "          \"priority\": " << jp.priority << ",\n";
            json << "          \"startTime\": " << jp.startTime << ",\n";
            json << "          \"completionTime\": " << jp.completionTime << ",\n";
            json << "          \"waitingTime\": " << jp.waitingTime << ",\n";
            json << "          \"turnaroundTime\": " << jp.turnaroundTime << ",\n";
            json << "          \"responseTime\": " << jp.responseTime << "\n";
            json << "        }";
            if (p + 1 < tick.processes.size()) json << ",";
            json << "\n";
        }
        json << "      ],\n";

        // Gantt chart up to this tick
        json << "      \"gantt\": [\n";
        for (size_t g = 0; g < tick.gantt.size(); ++g) {
            const auto& ge = tick.gantt[g];
            json << "        {\n";
            json << "          \"pid\": \"" << jsonEscape(ge.pid) << "\",\n";
            json << "          \"start\": " << ge.start << ",\n";
            json << "          \"end\": " << ge.end << "\n";
            json << "        }";
            if (g + 1 < tick.gantt.size()) json << ",";
            json << "\n";
        }
        json << "      ],\n";

        // Scheduling Log up to this tick
        json << "      \"log\": [\n";
        for (size_t l = 0; l < tick.log.size(); ++l) {
            const auto& entry = tick.log[l];
            json << "        {\n";
            json << "          \"time\": " << entry.time << ",\n";
            json << "          \"pid\": \"" << jsonEscape(entry.pid) << "\",\n";
            json << "          \"reason\": \"" << jsonEscape(entry.reason) << "\"\n";
            json << "        }";
            if (l + 1 < tick.log.size()) json << ",";
            json << "\n";
        }
        json << "      ]\n";

        json << "    }";
        if (t + 1 < ticks.size()) json << ",";
        json << "\n";
    }
    json << "  ],\n";

    // Simulation Results
    json << "  \"results\": {\n";
    json << "    \"avgWaitingTime\": " << result.avgWaitingTime << ",\n";
    json << "    \"avgTurnaroundTime\": " << result.avgTurnaroundTime << ",\n";
    json << "    \"avgResponseTime\": " << result.avgResponseTime << ",\n";
    json << "    \"cpuUtilization\": " << result.cpuUtilization << ",\n";
    json << "    \"throughput\": " << result.throughput << ",\n";
    json << "    \"jainFairnessIndex\": " << result.jainFairnessIndex << ",\n";
    json << "    \"contextSwitches\": " << result.contextSwitches << ",\n";
    json << "    \"totalTime\": " << result.totalTime << ",\n";
    json << "    \"idleTime\": " << result.idleTime << ",\n";
    json << "    \"longestWaitingPID\": \"" << jsonEscape(result.longestWaitingPID) << "\",\n";
    json << "    \"shortestWaitingPID\": \"" << jsonEscape(result.shortestWaitingPID) << "\",\n";
    
    // Process results
    json << "    \"processes\": [\n";
    for (size_t p = 0; p < result.processes.size(); ++p) {
        const auto& pr = result.processes[p];
        json << "      {\n";
        json << "        \"pid\": \"" << jsonEscape(pr.pid) << "\",\n";
        json << "        \"arrivalTime\": " << pr.arrivalTime << ",\n";
        json << "        \"burstTime\": " << pr.burstTime << ",\n";
        json << "        \"priority\": " << pr.priority << ",\n";
        json << "        \"startTime\": " << pr.startTime << ",\n";
        json << "        \"completionTime\": " << pr.completionTime << ",\n";
        json << "        \"waitingTime\": " << pr.waitingTime << ",\n";
        json << "        \"turnaroundTime\": " << pr.turnaroundTime << ",\n";
        json << "        \"responseTime\": " << pr.responseTime << "\n";
        json << "      }";
        if (p + 1 < result.processes.size()) json << ",";
        json << "\n";
    }
    json << "    ],\n";

    // Final Gantt chart
    json << "    \"gantt\": [\n";
    for (size_t g = 0; g < result.gantt.size(); ++g) {
        const auto& ge = result.gantt[g];
        json << "      {\n";
        json << "        \"pid\": \"" << jsonEscape(ge.pid) << "\",\n";
        json << "        \"start\": " << ge.start << ",\n";
        json << "        \"end\": " << ge.end << "\n";
        json << "      }";
        if (g + 1 < result.gantt.size()) json << ",";
        json << "\n";
    }
    json << "    ]\n";
    json << "  }\n";
    json << "}\n";

    std::cout << json.str();
    return 0;
}
