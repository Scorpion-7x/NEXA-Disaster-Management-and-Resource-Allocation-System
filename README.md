# NEXA
Data Structures & Algorithms based disaster-response system in C++ (DS Lab, JIIT).

## Build (PowerShell, MSYS2 UCRT64 toolchain)
    cmake -S . -B build -G Ninja
    cmake --build build
    ctest --test-dir build --output-on-failure
    .\build\nexa.exe          # run from repo root so data/ is found

## Layout
- `include/nexa/ds/`    hand-written data structures (header-only templates)
- `include/nexa/algo/`  sorting, searching, graph algorithms (BFS, DFS, Dijkstra)
- `include/nexa/map/`   road network built from the dataset locations
- `include/nexa/model/` Incident, Resource, Hospital
- `include/nexa/io/`    CSV store, logger
- `tests/`              one test file per area
- `tools/gen_data.py`   regenerates the demo dataset
