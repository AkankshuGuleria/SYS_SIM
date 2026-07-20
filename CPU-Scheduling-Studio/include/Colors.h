#pragma once
#include <string>
#include <iostream>

// --- Global flag: set false to disable all ANSI color output ---
extern bool g_colorsEnabled;

// --- Raw ANSI escape codes ---
namespace ANSI {
    constexpr const char* RESET          = "\033[0m";
    constexpr const char* RED            = "\033[31m";
    constexpr const char* GREEN          = "\033[32m";
    constexpr const char* YELLOW         = "\033[33m";
    constexpr const char* BLUE           = "\033[34m";
    constexpr const char* CYAN           = "\033[36m";
    constexpr const char* BRIGHT_BLACK   = "\033[90m";
    constexpr const char* BRIGHT_RED     = "\033[91m";
    constexpr const char* BRIGHT_GREEN   = "\033[92m";
    constexpr const char* BRIGHT_YELLOW  = "\033[93m";
    constexpr const char* BRIGHT_BLUE    = "\033[94m";
    constexpr const char* BRIGHT_CYAN    = "\033[96m";
    constexpr const char* BRIGHT_WHITE   = "\033[97m";
    constexpr const char* BOLD           = "\033[1m";
    constexpr const char* DIM            = "\033[2m";
    constexpr const char* CLEAR_SCREEN   = "\033[2J\033[H";
    constexpr const char* CURSOR_HOME    = "\033[H";
    constexpr const char* CURSOR_HIDE    = "\033[?25l";
    constexpr const char* CURSOR_SHOW    = "\033[?25h";
}

// --- Wrap text in a color code (no-op if colors disabled) ---
inline std::string colorize(const std::string& code, const std::string& text) {
    if (!g_colorsEnabled) return text;
    return code + text + ANSI::RESET;
}

// --- Semantic color helpers ---
inline std::string clrRunning  (const std::string& s){ return colorize(ANSI::BRIGHT_GREEN,  s); }
inline std::string clrReady    (const std::string& s){ return colorize(ANSI::BRIGHT_BLUE,   s); }
inline std::string clrWaiting  (const std::string& s){ return colorize(ANSI::BRIGHT_YELLOW, s); }
inline std::string clrCompleted(const std::string& s){ return colorize(ANSI::BRIGHT_BLACK,  s); }
inline std::string clrError    (const std::string& s){ return colorize(ANSI::BRIGHT_RED,    s); }
inline std::string clrMenu     (const std::string& s){ return colorize(ANSI::CYAN,          s); }
inline std::string clrBold     (const std::string& s){ return colorize(ANSI::BOLD,          s); }
inline std::string clrHeader   (const std::string& s){ return colorize(std::string(ANSI::BOLD) + ANSI::BRIGHT_CYAN, s); }
inline std::string clrDim      (const std::string& s){ return colorize(ANSI::DIM,           s); }

// --- Move cursor to row, col (1-indexed) ---
inline std::string moveCursor(int row, int col) {
    return "\033[" + std::to_string(row) + ";" + std::to_string(col) + "H";
}

// --- Enable ANSI VT processing on Windows (no-op on other platforms) ---
#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
inline void enableWindowsVT() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#else
inline void enableWindowsVT() {}
#endif
