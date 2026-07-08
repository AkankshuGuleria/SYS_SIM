// ============================================================
//  Colors.h
//  ANSI escape codes for terminal color and style output.
//  Supports enabling / disabling colors via a global flag.
// ============================================================
#pragma once

#include <string>
#include <iostream>

// --------------- Global color-enable flag -------------------
// Controlled from Settings; checked by every color function.
// Use extern declaration here; definition is in Colors.cpp
extern bool g_colorsEnabled;

// ============================================================
//  Raw ANSI codes (always compiled in, applied conditionally)
// ============================================================
namespace ANSI {
    // Reset
    constexpr const char* RESET       = "\033[0m";

    // Regular foreground colours
    constexpr const char* BLACK       = "\033[30m";
    constexpr const char* RED         = "\033[31m";
    constexpr const char* GREEN       = "\033[32m";
    constexpr const char* YELLOW      = "\033[33m";
    constexpr const char* BLUE        = "\033[34m";
    constexpr const char* MAGENTA     = "\033[35m";
    constexpr const char* CYAN        = "\033[36m";
    constexpr const char* WHITE       = "\033[37m";

    // Bright foreground colours
    constexpr const char* BRIGHT_BLACK   = "\033[90m";
    constexpr const char* BRIGHT_RED     = "\033[91m";
    constexpr const char* BRIGHT_GREEN   = "\033[92m";
    constexpr const char* BRIGHT_YELLOW  = "\033[93m";
    constexpr const char* BRIGHT_BLUE    = "\033[94m";
    constexpr const char* BRIGHT_MAGENTA = "\033[95m";
    constexpr const char* BRIGHT_CYAN    = "\033[96m";
    constexpr const char* BRIGHT_WHITE   = "\033[97m";

    // Background colours
    constexpr const char* BG_BLACK    = "\033[40m";
    constexpr const char* BG_GREEN    = "\033[42m";
    constexpr const char* BG_YELLOW   = "\033[43m";
    constexpr const char* BG_BLUE     = "\033[44m";
    constexpr const char* BG_CYAN     = "\033[46m";
    constexpr const char* BG_WHITE    = "\033[47m";

    // Styles
    constexpr const char* BOLD        = "\033[1m";
    constexpr const char* DIM         = "\033[2m";
    constexpr const char* UNDERLINE   = "\033[4m";
    constexpr const char* BLINK       = "\033[5m";
    constexpr const char* REVERSE     = "\033[7m";

    // Cursor / screen control
    constexpr const char* CLEAR_SCREEN    = "\033[2J\033[H";
    constexpr const char* CLEAR_LINE      = "\033[2K";
    constexpr const char* CURSOR_HOME     = "\033[H";
    constexpr const char* CURSOR_HIDE     = "\033[?25l";
    constexpr const char* CURSOR_SHOW     = "\033[?25h";
    constexpr const char* SAVE_CURSOR     = "\033[s";
    constexpr const char* RESTORE_CURSOR  = "\033[u";
}

// ============================================================
//  Convenience helper – returns a coloured string only when
//  colors are enabled.
// ============================================================
inline std::string colorize(const std::string& code, const std::string& text) {
    if (!g_colorsEnabled) return text;
    return code + text + ANSI::RESET;
}

// ============================================================
//  Semantic colour helpers matching the spec:
//    Running  -> Green
//    Ready    -> Blue
//    Waiting  -> Yellow
//    Completed-> Gray (Bright Black)
//    Errors   -> Red
//    Menu     -> Cyan
// ============================================================
inline std::string clrRunning  (const std::string& s){ return colorize(ANSI::BRIGHT_GREEN,  s); }
inline std::string clrReady    (const std::string& s){ return colorize(ANSI::BRIGHT_BLUE,   s); }
inline std::string clrWaiting  (const std::string& s){ return colorize(ANSI::BRIGHT_YELLOW, s); }
inline std::string clrCompleted(const std::string& s){ return colorize(ANSI::BRIGHT_BLACK,  s); }
inline std::string clrError    (const std::string& s){ return colorize(ANSI::BRIGHT_RED,    s); }
inline std::string clrMenu     (const std::string& s){ return colorize(ANSI::CYAN,          s); }
inline std::string clrBold     (const std::string& s){ return colorize(ANSI::BOLD,          s); }
inline std::string clrHeader   (const std::string& s){ return colorize(std::string(ANSI::BOLD) + ANSI::BRIGHT_CYAN, s); }
inline std::string clrDim      (const std::string& s){ return colorize(ANSI::DIM,           s); }

// ============================================================
//  Move cursor to (row, col) – 1-indexed
// ============================================================
inline std::string moveCursor(int row, int col) {
    return "\033[" + std::to_string(row) + ";" + std::to_string(col) + "H";
}

// ============================================================
//  Enable Windows VT processing so ANSI codes work on modern
//  Windows consoles (Windows 10 1511+).
// ============================================================
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
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}
#else
inline void enableWindowsVT() {} // No-op on other platforms
#endif
