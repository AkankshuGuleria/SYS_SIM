#pragma once

// Runs the scheduling simulator in JSON input/output mode for the web server.
// Reads input processes and settings from standard input, runs the algorithm,
// and outputs a complete simulation log and results as JSON to standard output.
int runJsonMode();
