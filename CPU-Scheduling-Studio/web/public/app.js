let processesList = [];
let simulationData = null;
let currentTickIndex = -1;
let animationTimer = null;
let isSimRunning = false;
let isPaused = false;

const PRESETS = {
    simple: [
        { pid: 'P1', arrivalTime: 0, burstTime: 4, priority: 3 },
        { pid: 'P2', arrivalTime: 1, burstTime: 3, priority: 2 },
        { pid: 'P3', arrivalTime: 2, burstTime: 1, priority: 1 },
        { pid: 'P4', arrivalTime: 3, burstTime: 2, priority: 4 }
    ],
    preemptive: [
        { pid: 'P1', arrivalTime: 0, burstTime: 8, priority: 3 },
        { pid: 'P2', arrivalTime: 2, burstTime: 4, priority: 1 },
        { pid: 'P3', arrivalTime: 4, burstTime: 2, priority: 2 },
        { pid: 'P4', arrivalTime: 5, burstTime: 6, priority: 4 }
    ],
    starve: [
        { pid: 'P1', arrivalTime: 0, burstTime: 10, priority: 4 },
        { pid: 'P2', arrivalTime: 1, burstTime: 2, priority: 1 },
        { pid: 'P3', arrivalTime: 2, burstTime: 2, priority: 1 },
        { pid: 'P4', arrivalTime: 3, burstTime: 2, priority: 1 }
    ]
};

const SCENARIOS = {
    convoy: {
        title: "Convoy Effect (FCFS vs. SJF)",
        desc: "A massive process P1 (Burst=15) arrives first at t=0, followed by short processes P2, P3, P4 (Burst=2). In FCFS scheduling, the short processes are blocked waiting for P1 to finish. This creates a 'convoy' behind P1, dragging down Average Waiting Time.",
        tip: "Observe how P2, P3, and P4 remain stuck in the Ready Queue until t=15. Once done, switch the scheduling algorithm to SJF (Shortest Job First) and rerun the simulation to see how the Average Waiting Time drops from 12.75 to 3.75 ticks!",
        algo: "FCFS",
        quantum: 2,
        processes: [
            { pid: 'P1', arrivalTime: 0, burstTime: 15, priority: 3 },
            { pid: 'P2', arrivalTime: 1, burstTime: 2, priority: 2 },
            { pid: 'P3', arrivalTime: 2, burstTime: 2, priority: 1 },
            { pid: 'P4', arrivalTime: 3, burstTime: 2, priority: 4 }
        ]
    },
    preempt: {
        title: "Preemption Interruption (SJF vs. SRTF)",
        desc: "A longer process P1 (Burst=10) starts executing at t=0. At t=2, a short process P2 (Burst=2) arrives, followed by P3 (Burst=1) at t=3. Non-Preemptive SJF will ignore them and complete P1. Preemptive SRTF will immediately interrupt P1 to execute the shorter jobs.",
        tip: "Run standard SJF first: P1 runs to completion uninterrupted. Then select SRTF (Shortest Remaining Time First) and watch how P1 gets kicked out of the CPU at t=2 so P2 and P3 can run, reducing overall waiting times!",
        algo: "SJF",
        quantum: 2,
        processes: [
            { pid: 'P1', arrivalTime: 0, burstTime: 10, priority: 3 },
            { pid: 'P2', arrivalTime: 2, burstTime: 2, priority: 2 },
            { pid: 'P3', arrivalTime: 3, burstTime: 1, priority: 1 }
        ]
    },
    timeshare: {
        title: "Response Time vs. Fairness (FCFS vs. Round Robin)",
        desc: "Three identical long-running processes (P1, P2, P3 with Burst=8) arrive at the exact same time (t=0). FCFS runs each to completion sequentially. Round Robin (RR) with a small Quantum (Q=2) continuously rotates execution, offering fairer sharing.",
        tip: "Run Round Robin (Q=2): Every process gets scheduled immediately, so Response Time is very low (0, 2, 4). But this introduces multiple context switches. If you run FCFS, P3 doesn't get CPU time until t=16. Compare the average Response Times!",
        algo: "RR",
        quantum: 2,
        processes: [
            { pid: 'P1', arrivalTime: 0, burstTime: 8, priority: 3 },
            { pid: 'P2', arrivalTime: 0, burstTime: 8, priority: 2 },
            { pid: 'P3', arrivalTime: 0, burstTime: 8, priority: 1 }
        ]
    },
    priority: {
        title: "Priority Preemption (NP vs. Preemptive)",
        desc: "Process P1 (Priority=4, lowest priority) starts running at t=0. At t=2, P2 (Priority=1, highest priority) arrives, and at t=3, P3 (Priority=2) arrives. Non-preemptive priority lets P1 finish, whereas preemptive priority interrupts it immediately.",
        tip: "Run Priority (Non-Preemptive) first: P1 runs to completion despite P2 having a higher priority. Then select Priority Preemptive and watch P1 get preempted immediately at t=2. Notice how high-priority tasks get instant CPU response!",
        algo: "PRIORITY",
        quantum: 2,
        processes: [
            { pid: 'P1', arrivalTime: 0, burstTime: 10, priority: 4 },
            { pid: 'P2', arrivalTime: 2, burstTime: 3, priority: 1 },
            { pid: 'P3', arrivalTime: 3, burstTime: 3, priority: 2 }
        ]
    }
};

document.addEventListener('DOMContentLoaded', () => {
    initDepthCanvas();
    initPresets();
    initProcessEditor();
    initControls();
    initScenarios();
    
    loadPreset('simple');
});

function initDepthCanvas() {
    const canvas = document.getElementById('system-depth-canvas');
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    const points = Array.from({ length: 70 }, (_, i) => ({
        x: Math.random() * 2 - 1,
        y: Math.random() * 2 - 1,
        z: Math.random() * 1.2 + 0.2,
        phase: i * 0.37
    }));

    function resize() {
        canvas.width = window.innerWidth * window.devicePixelRatio;
        canvas.height = window.innerHeight * window.devicePixelRatio;
        canvas.style.width = `${window.innerWidth}px`;
        canvas.style.height = `${window.innerHeight}px`;
        ctx.setTransform(window.devicePixelRatio, 0, 0, window.devicePixelRatio, 0, 0);
    }

    function draw(time) {
        const width = window.innerWidth;
        const height = window.innerHeight;
        const centerX = width * 0.68;
        const centerY = height * 0.26;

        ctx.clearRect(0, 0, width, height);
        ctx.lineWidth = 1;

        const projected = points.map(point => {
            const drift = time * 0.00012;
            const z = point.z + Math.sin(time * 0.001 + point.phase) * 0.08;
            const scale = 190 / (z + 1.25);
            return {
                x: centerX + Math.cos(point.phase + drift) * point.x * scale + point.x * width * 0.18,
                y: centerY + Math.sin(point.phase - drift) * point.y * scale + point.y * height * 0.16,
                z
            };
        });

        projected.forEach((a, i) => {
            for (let j = i + 1; j < projected.length; j++) {
                const b = projected[j];
                const distance = Math.hypot(a.x - b.x, a.y - b.y);
                if (distance < 120) {
                    ctx.strokeStyle = `rgba(14, 165, 233, ${0.22 - distance / 700})`;
                    ctx.beginPath();
                    ctx.moveTo(a.x, a.y);
                    ctx.lineTo(b.x, b.y);
                    ctx.stroke();
                }
            }
        });

        projected.forEach(point => {
            const radius = Math.max(1.2, 3.4 - point.z);
            ctx.fillStyle = 'rgba(45, 212, 191, 0.58)';
            ctx.beginPath();
            ctx.arc(point.x, point.y, radius, 0, Math.PI * 2);
            ctx.fill();
        });

        requestAnimationFrame(draw);
    }

    resize();
    window.addEventListener('resize', resize);
    requestAnimationFrame(draw);
}

function initPresets() {
    document.getElementById('preset-simple').addEventListener('click', () => loadPreset('simple'));
    document.getElementById('preset-preemptive').addEventListener('click', () => loadPreset('preemptive'));
    document.getElementById('preset-starve').addEventListener('click', () => loadPreset('starve'));
}

function loadPreset(key) {
    if (isSimRunning) resetSimulation();
    
    processesList = JSON.parse(JSON.stringify(PRESETS[key]));
    renderProcessTable();
}

// Scenarios/Case Studies Setup
function initScenarios() {
    const scenarioSelect = document.getElementById('scenario-select');
    const guideCard = document.getElementById('scenario-guide-card');
    const titleSpan = document.getElementById('scenario-title');
    const descP = document.getElementById('scenario-description');
    const tipSpan = document.getElementById('scenario-tip');
    
    const algoSelect = document.getElementById('algorithm-select');
    const quantumInput = document.getElementById('time-quantum');
    const quantumGroup = document.getElementById('quantum-group');

    scenarioSelect.addEventListener('change', () => {
        const val = scenarioSelect.value;
        if (isSimRunning) resetSimulation();

        if (val === 'custom') {
            guideCard.classList.add('hidden');
            return;
        }

        const sc = SCENARIOS[val];
        if (!sc) return;

        // 1. Show guide card & fill content
        titleSpan.textContent = sc.title;
        descP.textContent = sc.desc;
        tipSpan.textContent = sc.tip;
        guideCard.classList.remove('hidden');

        // 2. Load processes
        processesList = JSON.parse(JSON.stringify(sc.processes));
        renderProcessTable();

        // 3. Set Algorithm and Time Quantum
        algoSelect.value = sc.algo;
        quantumInput.value = sc.quantum;

        // Toggle time quantum group view manually
        if (sc.algo === 'RR' || sc.algo === 'MLFQ') {
            quantumGroup.style.display = 'block';
        } else {
            quantumGroup.style.display = 'none';
        }

        // Trigger visual badging update
        document.getElementById('current-algo-badge').textContent = algoSelect.options[algoSelect.selectedIndex].text.split(' ')[0];
    });
}

// ============================================================
//  Process Editor Operations
// ============================================================
function initProcessEditor() {
    const btnAdd = document.getElementById('btn-add-process');
    const dialog = document.getElementById('add-process-dialog');
    const btnCancel = document.getElementById('btn-dialog-cancel');
    const form = document.getElementById('add-process-form');
    
    btnAdd.addEventListener('click', () => {
        // Auto-increment PID suggestion
        const nextNum = processesList.length + 1;
        document.getElementById('new-pid').value = `P${nextNum}`;
        dialog.classList.add('open');
    });

    btnCancel.addEventListener('click', () => {
        dialog.classList.remove('open');
    });

    form.addEventListener('submit', (e) => {
        e.preventDefault();
        
        const pid = document.getElementById('new-pid').value.trim();
        const arrival = parseInt(document.getElementById('new-arrival').value);
        const burst = parseInt(document.getElementById('new-burst').value);
        const priority = parseInt(document.getElementById('new-priority').value);
        
        // Validate PID unique
        if (processesList.some(p => p.pid.toLowerCase() === pid.toLowerCase())) {
            alert(`Process ID "${pid}" already exists. Please choose a unique name.`);
            return;
        }

        processesList.push({
            pid: pid,
            arrivalTime: arrival,
            burstTime: burst,
            priority: priority
        });

        dialog.classList.remove('open');
        renderProcessTable();
        
        // Reset form inputs for next time
        document.getElementById('new-arrival').value = 0;
        document.getElementById('new-burst').value = 5;
        document.getElementById('new-priority').value = 3;
    });

    document.getElementById('btn-clear-processes').addEventListener('click', () => {
        if (isSimRunning) resetSimulation();
        processesList = [];
        renderProcessTable();
    });

    document.getElementById('btn-reset-original').addEventListener('click', () => {
        loadPreset('simple');
    });

    // Hide quantum spinner unless RR is selected
    const algoSelect = document.getElementById('algorithm-select');
    const quantumGroup = document.getElementById('quantum-group');
    
    algoSelect.addEventListener('change', () => {
        if (algoSelect.value === 'RR' || algoSelect.value === 'MLFQ') {
            quantumGroup.style.display = 'block';
        } else {
            quantumGroup.style.display = 'none';
        }
        
        // Update badging on the screen
        document.getElementById('current-algo-badge').textContent = algoSelect.options[algoSelect.selectedIndex].text.split(' ')[0];
    });
}

function renderProcessTable() {
    const tbody = document.getElementById('process-list-body');
    tbody.innerHTML = '';
    
    if (processesList.length === 0) {
        tbody.innerHTML = `<tr><td colspan="5" style="text-align: center; color: var(--text-muted); font-style: italic;">No processes configured. Click + Add to add a process.</td></tr>`;
        return;
    }

    processesList.forEach((proc, idx) => {
        const tr = document.createElement('tr');
        
        tr.innerHTML = `
            <td><strong>${proc.pid}</strong></td>
            <td><input type="number" min="0" value="${proc.arrivalTime}" data-index="${idx}" data-field="arrivalTime" class="table-cell-input"></td>
            <td><input type="number" min="1" value="${proc.burstTime}" data-index="${idx}" data-field="burstTime" class="table-cell-input"></td>
            <td><input type="number" min="1" value="${proc.priority}" data-index="${idx}" data-field="priority" class="table-cell-input"></td>
            <td style="text-align: center;"><button class="btn-delete-row" data-index="${idx}" aria-label="Delete process">Delete</button></td>
        `;
        
        tbody.appendChild(tr);
    });

    // Attach listeners to input changes
    tbody.querySelectorAll('.table-cell-input').forEach(input => {
        input.addEventListener('change', (e) => {
            const idx = parseInt(e.target.getAttribute('data-index'));
            const field = e.target.getAttribute('data-field');
            const val = parseInt(e.target.value);
            
            if (field === 'arrivalTime') {
                processesList[idx].arrivalTime = Math.max(0, val);
            } else if (field === 'burstTime') {
                processesList[idx].burstTime = Math.max(1, val);
            } else if (field === 'priority') {
                processesList[idx].priority = Math.max(1, val);
            }
        });
    });

    // Attach listeners to deletes
    tbody.querySelectorAll('.btn-delete-row').forEach(btn => {
        btn.addEventListener('click', (e) => {
            const idx = parseInt(btn.getAttribute('data-index'));
            processesList.splice(idx, 1);
            renderProcessTable();
        });
    });
}

// ============================================================
//  Simulation Controls & Timer Loop
// ============================================================
function initControls() {
    const btnRun = document.getElementById('btn-run-sim');
    const btnPause = document.getElementById('btn-pause-sim');
    const btnStep = document.getElementById('btn-step-sim');
    const btnReset = document.getElementById('btn-reset-sim');
    const speedSlider = document.getElementById('speed-slider');
    const speedLabel = document.getElementById('speed-label');

    speedSlider.addEventListener('input', (e) => {
        speedLabel.textContent = e.target.value;
        if (isSimRunning && !isPaused) {
            // Restart interval with new speed
            clearInterval(animationTimer);
            animationTimer = setInterval(animateTickStep, parseInt(e.target.value));
        }
    });

    btnRun.addEventListener('click', () => {
        if (processesList.length === 0) {
            alert('Cannot run simulation: Ready queue is empty! Please add processes.');
            return;
        }

        if (isPaused && simulationData) {
            // Resume
            isPaused = false;
            btnRun.disabled = true;
            btnPause.disabled = false;
            btnStep.disabled = true;
            animationTimer = setInterval(animateTickStep, parseInt(speedSlider.value));
            return;
        }

        // Fresh Start - Fetch simulation from C++ server
        const algorithm = document.getElementById('algorithm-select').value;
        const quantum = parseInt(document.getElementById('time-quantum').value);
        
        btnRun.textContent = "Loading...";
        btnRun.disabled = true;

        fetch('/api/simulate', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                algorithm: algorithm,
                quantum: quantum,
                processes: processesList
            })
        })
        .then(res => {
            if (!res.ok) throw new Error('API server returned error');
            return res.json();
        })
        .then(data => {
            simulationData = data;
            currentTickIndex = 0;
            isSimRunning = true;
            isPaused = false;

            btnRun.textContent = "Run";
            btnPause.disabled = false;
            btnStep.disabled = true;
            btnReset.disabled = false;
            
            // Hide previous results
            document.getElementById('performance-results-section').classList.add('hidden');
            
            // Render first tick immediately
            renderTick(0);
            
            // Start clock ticks
            animationTimer = setInterval(animateTickStep, parseInt(speedSlider.value));
        })
        .catch(err => {
            console.error(err);
            alert('Failed to connect to the C++ Backend. Please verify that server.js is running and cpu_studio.exe is compiled.');
            btnRun.textContent = "Run";
            btnRun.disabled = false;
        });
    });

    btnPause.addEventListener('click', () => {
        if (!isSimRunning || isPaused) return;
        
        clearInterval(animationTimer);
        isPaused = true;
        
        btnRun.disabled = false;
        btnPause.disabled = true;
        btnStep.disabled = false;
    });

    btnStep.addEventListener('click', () => {
        if (!isSimRunning || !isPaused) return;
        animateTickStep();
    });

    btnReset.addEventListener('click', () => {
        resetSimulation();
    });
}

function animateTickStep() {
    if (!simulationData || currentTickIndex === -1) return;
    
    currentTickIndex++;
    if (currentTickIndex >= simulationData.ticks.length) {
        // Simulation completed
        clearInterval(animationTimer);
        isSimRunning = false;
        isPaused = false;
        
        document.getElementById('btn-run-sim').disabled = true;
        document.getElementById('btn-pause-sim').disabled = true;
        document.getElementById('btn-step-sim').disabled = true;
        
        // Show final results block
        renderFinalResults();
        return;
    }

    renderTick(currentTickIndex);
}

function resetSimulation() {
    clearInterval(animationTimer);
    simulationData = null;
    currentTickIndex = -1;
    isSimRunning = false;
    isPaused = false;

    // Reset buttons
    document.getElementById('btn-run-sim').disabled = false;
    document.getElementById('btn-pause-sim').disabled = true;
    document.getElementById('btn-step-sim').disabled = true;
    
    // Clear Visual Panel Elements
    document.getElementById('clock-ticks').textContent = 't = 0';
    
    const cpuBox = document.getElementById('cpu-box');
    cpuBox.className = 'cpu-display-box';
    cpuBox.innerHTML = `<div class="cpu-idle-label">CPU IDLE</div>`;
    
    document.getElementById('ready-queue-chain').innerHTML = `<span class="empty-placeholder">(empty)</span>`;
    document.getElementById('waiting-queue-chain').innerHTML = `<span class="empty-placeholder">(empty)</span>`;
    document.getElementById('states-grid').innerHTML = '';
    document.getElementById('gantt-chart-row').innerHTML = `<span class="empty-placeholder">Gantt chart will build here tick-by-tick.</span>`;
    
    const decisionBox = document.getElementById('decision-log-box');
    decisionBox.className = 'decision-box';
    decisionBox.innerHTML = `<div class="decision-reason">No simulation active. Press Run to start.</div>`;

    document.getElementById('performance-results-section').classList.add('hidden');
}

// ============================================================
//  Tick Rendering Engine
// ============================================================
function renderTick(tickIndex) {
    const tick = simulationData.ticks[tickIndex];
    if (!tick) return;

    // 1. Clock Time
    document.getElementById('clock-ticks').textContent = `t = ${tick.time}`;

    // 2. CPU Panel
    const cpuBox = document.getElementById('cpu-box');
    if (tick.cpuPid) {
        cpuBox.className = 'cpu-display-box busy';
        
        // Get process specs to compute remaining progress
        const activeProc = tick.processes.find(p => p.pid === tick.cpuPid);
        const burstTime = activeProc ? activeProc.burstTime : 1;
        const remTime = activeProc ? activeProc.remainingTime : 0;
        const percent = ((burstTime - remTime) / burstTime) * 100;

        cpuBox.innerHTML = `
            <div class="cpu-pid-label">${tick.cpuPid}</div>
            <div class="cpu-progress-bar-container">
                <div class="cpu-progress-bar" style="width: ${percent}%;"></div>
            </div>
            <div class="cpu-time-remaining">Remaining: ${remTime} / ${burstTime} ticks</div>
        `;
    } else {
        cpuBox.className = 'cpu-display-box';
        cpuBox.innerHTML = `<div class="cpu-idle-label">CPU IDLE</div>`;
    }

    // 3. Ready Queue Chain
    const rqChain = document.getElementById('ready-queue-chain');
    rqChain.innerHTML = '';
    if (tick.readyQueue.length === 0) {
        rqChain.innerHTML = `<span class="empty-placeholder">(empty)</span>`;
    } else {
        tick.readyQueue.forEach(pid => {
            const node = document.createElement('span');
            node.className = 'queue-process-node';
            node.textContent = pid;
            rqChain.appendChild(node);
        });
    }

    // 4. Waiting Queue Chain
    const wqChain = document.getElementById('waiting-queue-chain');
    wqChain.innerHTML = '';
    if (tick.waitingQueue.length === 0) {
        wqChain.innerHTML = `<span class="empty-placeholder">(empty)</span>`;
    } else {
        tick.waitingQueue.forEach(pid => {
            const node = document.createElement('span');
            node.className = 'queue-process-node';
            node.textContent = pid;
            wqChain.appendChild(node);
        });
    }

    // 5. Process States Grid
    const statesGrid = document.getElementById('states-grid');
    statesGrid.innerHTML = '';
    
    // Sort processes by PID alphabetically
    const sortedProcs = [...tick.processes].sort((a, b) => a.pid.localeCompare(b.pid));
    
    sortedProcs.forEach(p => {
        const percent = ((p.burstTime - p.remainingTime) / p.burstTime) * 100;
        const stateClass = `state-${p.state.toLowerCase()}`;
        
        const card = document.createElement('div');
        card.className = `state-bar-card ${stateClass}`;
        
        card.innerHTML = `
            <div class="state-bar-header">
                <span class="state-bar-pid">${p.pid}</span>
                <span class="state-pill ${p.state.toLowerCase()}">${p.state}</span>
            </div>
            <div class="state-progress">
                <div class="state-progress-fill" style="width: ${percent}%;"></div>
            </div>
            <span class="state-numeric">Burst: ${p.burstTime - p.remainingTime}/${p.burstTime}</span>
        `;
        statesGrid.appendChild(card);
    });

    // 6. Last Decision Log
    const decisionBox = document.getElementById('decision-log-box');
    
    // Find if there is a decision log entry made at or just before this time tick
    const relevantLog = [...tick.log].reverse().find(l => l.time <= tick.time);
    if (relevantLog) {
        decisionBox.className = 'decision-box decision-box-active';
        decisionBox.innerHTML = `<div class="decision-reason"><strong>t=${relevantLog.time} &rarr;</strong> Selected process <strong>${relevantLog.pid}</strong><br>${relevantLog.reason}</div>`;
    } else {
        decisionBox.className = 'decision-box';
        decisionBox.innerHTML = `<div class="decision-reason">No changes. Executing scheduler selection.</div>`;
    }

    // 7. Live Gantt Chart
    const ganttRow = document.getElementById('gantt-chart-row');
    ganttRow.innerHTML = '';
    
    if (tick.gantt.length === 0) {
        ganttRow.innerHTML = `<span class="empty-placeholder">Gantt chart will build here tick-by-tick.</span>`;
    } else {
        tick.gantt.forEach(block => {
            const div = document.createElement('div');
            div.className = 'gantt-block';
            div.setAttribute('data-pid', block.pid);
            
            // Flex scaling based on block width (burst duration)
            const duration = block.end - block.start;
            div.style.flexGrow = duration;
            div.style.minWidth = `${Math.max(40, duration * 18)}px`;

            div.innerHTML = `
                <span class="gantt-pid">${block.pid}</span>
                <div class="gantt-time-labels">
                    <span>${block.start}</span>
                    <span>${block.end}</span>
                </div>
            `;
            ganttRow.appendChild(div);
        });
    }
}

// ============================================================
//  Final Performance Results Rendering
// ============================================================
function renderFinalResults() {
    const res = simulationData.results;
    if (!res) return;

    // Show panel
    document.getElementById('performance-results-section').classList.remove('hidden');

    // 1. Stats overview cards
    document.getElementById('stat-avg-wt').textContent = res.avgWaitingTime.toFixed(2);
    document.getElementById('stat-avg-tat').textContent = res.avgTurnaroundTime.toFixed(2);
    document.getElementById('stat-avg-rt').textContent = res.avgResponseTime.toFixed(2);
    document.getElementById('stat-cpu-util').textContent = `${res.cpuUtilization.toFixed(1)}%`;
    document.getElementById('stat-jain-fairness').textContent = res.jainFairnessIndex.toFixed(2);
    document.getElementById('stat-throughput').textContent = res.throughput.toFixed(3);
    document.getElementById('stat-ctx-sw').textContent = res.contextSwitches;

    // 2. Table calculations
    const tbody = document.getElementById('results-table-body');
    tbody.innerHTML = '';

    res.processes.sort((a,b) => a.pid.localeCompare(b.pid)).forEach(p => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td><strong>${p.pid}</strong></td>
            <td>${p.arrivalTime}</td>
            <td>${p.burstTime}</td>
            <td>${p.priority}</td>
            <td>${p.startTime}</td>
            <td>${p.completionTime}</td>
            <td><strong>${p.waitingTime}</strong></td>
            <td><strong>${p.turnaroundTime}</strong></td>
            <td><strong>${p.responseTime}</strong></td>
        `;
        tbody.appendChild(tr);
    });

    // Auto-scroll to results card
    document.getElementById('performance-results-section').scrollIntoView({ behavior: 'smooth' });
}
