cmake_minimum_required(VERSION 3.16)
project(CPUSchedulingStudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Treat warnings as errors in Debug builds so regressions are caught early.
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(ALGO_SOURCES
    algorithms/FCFS.cpp
    algorithms/SJF.cpp
    algorithms/SRTF.cpp
    algorithms/Priority.cpp
    algorithms/PriorityPreemptive.cpp
    algorithms/RoundRobin.cpp
    algorithms/MLFQ.cpp
)

set(SRC_SOURCES
    src/Colors.cpp
    src/JsonMode.cpp
    src/Scheduler.cpp
    src/Dashboard.cpp
    src/Process.cpp
    src/Utilities.cpp
)

add_executable(cpu_studio
    main.cpp
    ${SRC_SOURCES}
    ${ALGO_SOURCES}
)

target_include_directories(cpu_studio PRIVATE include)

if(WIN32)
    target_link_libraries(cpu_studio PRIVATE kernel32)
endif()

# A lightweight assert-based harness; no external test framework needed.

add_executable(run_tests
    tests/test_scheduling.cpp
    ${SRC_SOURCES}
    ${ALGO_SOURCES}
)

target_include_directories(run_tests PRIVATE include)

if(WIN32)
    target_link_libraries(run_tests PRIVATE kernel32)
endif()

enable_testing()
add_test(NAME SchedulingTests COMMAND run_tests)
