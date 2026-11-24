# NEXA

![CI](https://github.com/Scorpion-7x/NEXA-Disaster-Management-and-Resource-Allocation-System/actions/workflows/ci.yml/badge.svg)

A disaster-response system in C++17: it prioritises emergency incidents, finds the nearest
suitable unit, routes it over a road network, reserves hospital beds, and simulates a city
over time to measure response times. The core data structures are hand-written.

## Build and run (PowerShell, MSYS2 UCRT64 toolchain)

Always run from the repo root, so the program finds `data/`.

    cmake -S . -B build -G Ninja                  # first time only
    cmake --build build                           # after every change
    ctest --test-dir build --output-on-failure    # run all tests
    .\build\nexa.exe                              # open the menu

Other ways to start it:

    .\build\nexa.exe saved      # continue from a folder written by menu option 13
    .\build\nexa.exe --demo     # the original step-by-step data-structure demos

## Using it

The menu covers the whole workflow: report incidents, dispatch the most urgent one,
resolve, undo, search units, find routes to hospitals, view deployment history, save.

**Option 15, time simulation**, is the best place to start. Units drive to incidents,
work on scene, return to base and become free again, while new random incidents keep
arriving. At the end you get response times (average, median, 90th percentile), how
long incidents waited for a free unit, utilisation, and a breakdown per unit type.
Same seed gives the same run, so you can change one thing and compare.

## Layout

- `include/nexa/ds/`     hand-written data structures (header-only templates)
- `include/nexa/algo/`   sorting, searching, graph algorithms (BFS, DFS, Dijkstra)
- `include/nexa/map/`    road network built from the dataset locations
- `include/nexa/app/`    `NexaSystem` (the dispatch workflow) and `Simulator`
- `include/nexa/model/`  Incident, Resource, Hospital
- `include/nexa/io/`     CSV store, logger
- `src/`                 `main.cpp` (menu), `demos.cpp`
- `tests/`               one test program per area, run by `ctest`
- `tools/gen_data.py`    regenerates the demo dataset
- `.github/workflows/`   CI: every push is built and tested on Linux (also with
                         memory checks) and on Windows with MSYS2

The log of every operation is written to `logs/operations.log`.
