const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

const PORT = 3000;
const PUBLIC_DIR = path.join(__dirname, 'public');

// MIME types helper
const MIME_TYPES = {
    '.html': 'text/html',
    '.css': 'text/css',
    '.js': 'text/javascript',
    '.json': 'application/json',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.gif': 'image/gif',
    '.ico': 'image/x-icon',
    '.svg': 'image/svg+xml'
};

// Route static file request
function serveStaticFile(res, filePath) {
    const ext = path.extname(filePath).toLowerCase();
    const contentType = MIME_TYPES[ext] || 'application/octet-stream';

    fs.readFile(filePath, (err, content) => {
        if (err) {
            if (err.code === 'ENOENT') {
                res.writeHead(404, { 'Content-Type': 'text/plain' });
                res.end('404 Not Found');
            } else {
                res.writeHead(500, { 'Content-Type': 'text/plain' });
                res.end(`500 Internal Server Error: ${err.code}`);
            }
        } else {
            res.writeHead(200, { 'Content-Type': contentType });
            res.end(content, 'utf-8');
        }
    });
}

// Run simulation via the C++ executable
function handleSimulateAPI(req, res) {
    let body = '';
    req.on('data', chunk => {
        body += chunk;
    });

    req.on('end', () => {
        try {
            const data = JSON.parse(body);
            const { algorithm, quantum, processes } = data;

            if (!algorithm || !processes || !Array.isArray(processes)) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ error: 'Missing required parameters (algorithm, processes)' }));
                return;
            }

            // Path to C++ executable (located in parent directory CPU-Scheduling-Studio)
            const exePath = path.resolve(__dirname, '..', 'cpu_studio.exe');

            // Format inputs to send to child process stdin:
            // Line 1: Algorithm
            // Line 2: Quantum
            // Line 3: Number of processes
            // Next lines: pid arrival burst priority
            let inputStr = `${algorithm}\n${quantum || 2}\n${processes.length}\n`;
            processes.forEach(p => {
                inputStr += `${p.pid} ${p.arrivalTime} ${p.burstTime} ${p.priority}\n`;
            });

            // Start child process
            const child = spawn(exePath, ['--json']);

            let stdoutData = '';
            let stderrData = '';

            child.stdout.on('data', data => {
                stdoutData += data.toString();
            });

            child.stderr.on('data', data => {
                stderrData += data.toString();
            });

            child.on('close', code => {
                if (code !== 0) {
                    console.error(`C++ process exited with code ${code}. Stderr: ${stderrData}`);
                    res.writeHead(500, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({
                        error: 'Simulation execution failed in C++ backend',
                        details: stderrData,
                        code: code
                    }));
                    return;
                }

                try {
                    // C++ prints the raw JSON output to stdout. Parse it to verify validity.
                    const resultJson = JSON.parse(stdoutData);
                    res.writeHead(200, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify(resultJson));
                } catch (parseErr) {
                    console.error('Failed to parse C++ stdout as JSON:', stdoutData);
                    res.writeHead(500, { 'Content-Type': 'application/json' });
                    res.end(JSON.stringify({
                        error: 'C++ backend returned invalid JSON output',
                        rawOutput: stdoutData
                    }));
                }
            });

            // Write inputs to stdin and close it
            child.stdin.write(inputStr);
            child.stdin.end();

        } catch (err) {
            console.error('Error handling simulation request:', err);
            res.writeHead(500, { 'Content-Type': 'application/json' });
            res.end(JSON.stringify({ error: 'Server internal error', message: err.message }));
        }
    });
}

// Main HTTP Server
const server = http.createServer((req, res) => {
    // API endpoint
    if (req.method === 'POST' && req.url === '/api/simulate') {
        handleSimulateAPI(req, res);
        return;
    }

    // Default static file route
    let reqPath = req.url === '/' ? '/index.html' : req.url;
    // Strip query string if present
    reqPath = reqPath.split('?')[0];

    const safePath = path.normalize(reqPath).replace(/^(\.\.[\/\\])+/, '');
    const filePath = path.join(PUBLIC_DIR, safePath);

    // Verify it is inside the public folder for security
    if (!filePath.startsWith(PUBLIC_DIR)) {
        res.writeHead(403, { 'Content-Type': 'text/plain' });
        res.end('403 Forbidden');
        return;
    }

    serveStaticFile(res, filePath);
});

server.listen(PORT, () => {
    console.log(`=======================================================`);
    console.log(` CPU Scheduling Studio - Web Companion Server started!`);
    console.log(` Run local server on: http://localhost:${PORT}`);
    console.log(`=======================================================`);
});
