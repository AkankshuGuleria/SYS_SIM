// ============================================================
//  Colors.cpp
//  Defines the global g_colorsEnabled variable declared as
//  extern in Colors.h.  Only ONE translation unit must define
//  it; all others use the extern declaration.
// ============================================================
#include "../include/Colors.h"

// Global definition (all other TUs get the extern declaration)
bool g_colorsEnabled = true;
