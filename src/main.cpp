// NEXA - menu-driven disaster response console.
// main.cpp only reads input and prints; all the work happens in NexaSystem (include/nexa/app/nexa_system.h).
//
//   .\build\nexa.exe              load data/ and open the menu
//   .\build\nexa.exe saved        continue from a folder written by "Save"
//   .\build\nexa.exe --demo       run the original step-by-step data-structure demos
#include <iomanip>
#include <iostream>
#include <string>
#include "nexa/app/nexa_system.h"
#include "nexa/app/demos.h"
#include "nexa/app/simulator.h"

using namespace nexa;

// ---------- input helpers (return false on end of input, which exits the program) ----------

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
static std::string upper(std::string s) {
    for (char& c : s) if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    return s;
}
static bool readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt;
    if (!std::getline(std::cin, out)) return false;
    out = trim(out);
    return true;
}
static bool readLong(const std::string& prompt, long long& v, long long lo, long long hi) {
    std::string s;
    while (readLine(prompt, s)) {
        try {
            size_t used = 0;
            long long x = std::stoll(s, &used);
            if (used == s.size() && x >= lo && x <= hi) { v = x; return true; }
        } catch (...) {}
        std::cout << "  Enter a whole number from " << lo << " to " << hi << ".\n";
    }
    return false;
}
static bool readInt(const std::string& prompt, int& v, int lo, int hi) {
    long long x = 0;
    if (!readLong(prompt, x, lo, hi)) return false;
    v = (int)x;
    return true;
}
static bool readDouble(const std::string& prompt, double& v, double lo, double hi) {
    std::string s;
    while (readLine(prompt, s)) {
        try {
            size_t used = 0;
            double x = std::stod(s, &used);
            if (used == s.size() && x >= lo && x <= hi) { v = x; return true; }
        } catch (...) {}
        std::cout << "  Enter a number from " << lo << " to " << hi << ".\n";
    }
    return false;
}

// ---------- printing ----------

static void printIncident(const Incident& x) {
    std::cout << "  #" << std::left << std::setw(4) << x.id << " sev " << std::setw(3) << x.severity
              << std::setw(11) << x.type << std::setw(11) << x.status << "t=" << std::setw(6) << x.reportedAt
              << x.description << std::right << "\n";
}
static void printResource(const Resource& r) {
    std::cout << "  " << std::left << std::setw(16) << r.name << std::setw(12) << r.type << std::setw(10) << r.status
              << std::setw(20) << r.capability << std::right << "(" << r.lat << ", " << r.lon << ")\n";
}
static void printDispatch(const NexaSystem& sys, const DispatchResult& d) {
    const Incident& x = sys.incidents[d.incident];
    std::cout << "  Incident #" << x.id << " " << x.type << " (sev " << x.severity << ")\n";
    if (d.resource < 0) {
        std::cout << "  No available " << d.neededType << " - incident stays OPEN.\n";
        return;
    }
    std::cout << "  Sent " << sys.resources[d.resource].name << ": " << d.km << " km by road"
              << " (best of " << d.candidates << " nearest " << d.neededType << " candidates)\n";
    if (d.hospital >= 0)
        std::cout << "  Bed reserved at " << sys.hospitals[d.hospital].name << " (" << d.hospitalKm << " km)\n";
    else if (NexaSystem::needsHospital(x.type))
        std::cout << "  Warning: no hospital with free beds reachable.\n";
}

static void dashboard(NexaSystem& sys) {
    int open = 0, disp = 0, resolved = 0;
    for (const Incident& x : sys.incidents) {
        if (x.status == "OPEN") open++;
        else if (x.status == "DISPATCHED") disp++;
        else if (x.status == "RESOLVED") resolved++;
    }
    std::cout << "\n  Time t=" << sys.now() << "   Incidents: " << open << " open, " << disp << " dispatched, "
              << resolved << " resolved   (" << sys.intakeSize() << " waiting in intake queue)\n";
    const char* types[] = {"AMBULANCE", "FIRE_TRUCK", "RESCUE_TEAM", "BOAT"};
    std::cout << "  Units available:";
    for (const char* t : types) {
        int avail = 0, total = 0;
        for (const Resource& r : sys.resources) if (r.type == t) { total++; if (r.status == "AVAILABLE") avail++; }
        std::cout << "  " << t << " " << avail << "/" << total;
    }
    int beds = 0, total = 0;
    for (const Hospital& h : sys.hospitals) { beds += h.freeBeds; total += h.totalBeds; }
    std::cout << "\n  Hospital beds free: " << beds << "/" << total << "   Deployments: " << sys.deployments().size()
              << "   Undo history: " << sys.undoDepth() << " action(s)\n";
    std::cout << "  Road network: " << sys.roads().nodeCount() << " places, " << sys.roads().edgeCount()
              << " roads   Cost matrix: " << sys.costMatrix().nnz() << " cells stored\n";
}

static void menu() {
    std::cout << "\n=============== NEXA ===============\n"
                 "  1  Dashboard\n"
                 "  2  List incidents by priority\n"
                 "  3  Report a new incident\n"
                 "  4  Dispatch next most urgent incident\n"
                 "  5  Dispatch a specific incident\n"
                 "  6  Resolve an incident\n"
                 "  7  Undo last action\n"
                 "  8  Find a resource by name\n"
                 "  9  Nearest units to a location\n"
                 " 10  Incidents reported in a time window\n"
                 " 11  Route from an incident to a hospital\n"
                 " 12  Deployment history\n"
                 " 13  Save state\n"
                 " 14  Run data-structure demos\n"
                 " 15  Run time simulation\n"
                 "  0  Exit\n";
}

int main(int argc, char** argv) {
    std::string dir = "data";
    bool demo = false;
    for (int a = 1; a < argc; a++) {
        std::string s = argv[a];
        if (s == "--demo") demo = true; else dir = s;
    }
    if (demo) return runDemos(dir);

    NexaSystem sys;
    std::string err;
    if (!sys.load(dir, err)) {
        std::cerr << "Could not load data: " << err << ". Run from the repo root.\n";
        return 1;
    }
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "NEXA loaded " << sys.incidents.size() << " incidents, " << sys.resources.size() << " resources, "
              << sys.hospitals.size() << " hospitals from '" << dir << "'.\n";
    dashboard(sys);

    for (;;) {
        menu();
        int choice = 0;
        if (!readInt("Choose: ", choice, 0, 15)) break;
        std::cout << "\n";
        if (choice == 0) break;

        if (choice == 1) {
            dashboard(sys);

        } else if (choice == 2) {
            std::string st;
            if (!readLine("Status [OPEN/DISPATCHED/RESOLVED, Enter = OPEN]: ", st)) break;
            st = st.empty() ? "OPEN" : upper(st);
            DynamicArray<int> list = sys.incidentsByPriority(st);
            std::cout << "  " << list.size() << " " << st << " incident(s), most urgent first:\n";
            for (int k = 0; k < list.size() && k < 20; k++) printIncident(sys.incidents[list[k]]);
            if (list.size() > 20) std::cout << "  ... " << list.size() - 20 << " more\n";

        } else if (choice == 3) {
            const char* types[] = {"FLOOD", "FIRE", "EARTHQUAKE", "ACCIDENT", "MEDICAL", "COLLAPSE"};
            std::cout << "  Types: 1 FLOOD  2 FIRE  3 EARTHQUAKE  4 ACCIDENT  5 MEDICAL  6 COLLAPSE\n";
            int t = 1, sev = 1; double lat = 0, lon = 0; std::string desc;
            if (!readInt("Type (1-6): ", t, 1, 6) || !readInt("Severity (1-10): ", sev, 1, 10) ||
                !readDouble("Latitude (28.0-29.0): ", lat, 28.0, 29.0) ||
                !readDouble("Longitude (77.0-78.0): ", lon, 77.0, 78.0) ||
                !readLine("Description: ", desc)) break;
            if (desc.empty()) desc = "Reported via console";
            int i = sys.reportIncident(types[t - 1], sev, lat, lon, desc);
            std::cout << "  Reported incident #" << sys.incidents[i].id << " at t=" << sys.incidents[i].reportedAt
                      << ". It is in the intake queue and will be triaged on the next dispatch.\n";

        } else if (choice == 4) {
            DispatchResult d = sys.dispatchNext();
            if (d.incident < 0) std::cout << "  Nothing to dispatch: no open incident has a free unit of its type.\n";
            else printDispatch(sys, d);
            if (d.waiting > 0)
                std::cout << "  " << d.waiting << " more urgent incident(s) are waiting for a unit of their type.\n";

        } else if (choice == 5) {
            int id = 0;
            if (!readInt("Incident id: ", id, 1, 1000000)) break;
            DispatchResult d = sys.dispatchIncident(id, err);
            if (d.incident < 0) std::cout << "  " << err << "\n";
            else printDispatch(sys, d);

        } else if (choice == 6) {
            int id = 0;
            if (!readInt("Incident id: ", id, 1, 1000000)) break;
            int freed = sys.resolveIncident(id, err);
            if (freed < 0) std::cout << "  " << err << "\n";
            else std::cout << "  Incident #" << id << " resolved, " << freed << " unit(s) available again.\n";

        } else if (choice == 7) {
            std::string what = sys.undo();
            std::cout << "  " << (what.empty() ? "Nothing to undo." : "Undone: " + what) << "\n";

        } else if (choice == 8) {
            std::string name;
            if (!readLine("Resource name (e.g. Ambulance 6): ", name)) break;
            int r = sys.resourceByName(name);
            if (r >= 0) {
                std::cout << "  Exact match (hash table lookup):\n";
                printResource(sys.resources[r]);
            } else {
                DynamicArray<int> m = sys.resourcesMatching(name);
                std::cout << "  No exact match. " << m.size() << " name(s) containing \"" << name << "\" (linear search):\n";
                for (int k = 0; k < m.size() && k < 15; k++) printResource(sys.resources[m[k]]);
            }

        } else if (choice == 9) {
            double lat = 0, lon = 0; int k = 1; std::string type, avail;
            if (!readDouble("Latitude (28.0-29.0): ", lat, 28.0, 29.0) ||
                !readDouble("Longitude (77.0-78.0): ", lon, 77.0, 78.0) ||
                !readInt("How many (1-10): ", k, 1, 10) ||
                !readLine("Type [AMBULANCE/FIRE_TRUCK/RESCUE_TEAM/BOAT, Enter = any]: ", type) ||
                !readLine("Only available units? [Y/n]: ", avail)) break;
            int ids[10];
            int m = sys.nearestResources(lat, lon, k, upper(type), upper(avail) != "N", ids);
            std::cout << "  " << m << " nearest (KD tree, straight line):\n";
            for (int j = 0; j < m; j++) {
                const Resource& r = sys.resources[ids[j]];
                std::cout << "  " << std::setw(6) << haversineKm(lat, lon, r.lat, r.lon) << " km  ";
                printResource(r);
            }

        } else if (choice == 10) {
            long long from = 0, to = 0;
            if (!readLong("From time t: ", from, 0, 1000000000) || !readLong("To time t: ", to, from, 1000000000)) break;
            DynamicArray<int> list = sys.incidentsBetween(from, to);
            std::cout << "  " << list.size() << " incident(s) reported in [" << from << ", " << to << "] (AVL range query):\n";
            for (int k = 0; k < list.size() && k < 25; k++) printIncident(sys.incidents[list[k]]);

        } else if (choice == 11) {
            int id = 0;
            if (!readInt("Incident id: ", id, 1, 1000000)) break;
            int i = sys.incidentIndex(id);
            if (i < 0) { std::cout << "  No incident #" << id << "\n"; continue; }
            DynamicArray<int> path; double km = 0;
            int h = sys.routeToHospital(i, path, km);
            if (h < 0) { std::cout << "  No reachable hospital with free beds.\n"; continue; }
            std::cout << "  Nearest hospital with free beds: " << sys.hospitals[h].name << ", " << km
                      << " km by road (Dijkstra), " << path.size() - 1 << " road(s):\n  ";
            for (int k = 0; k < path.size(); k++) std::cout << (k ? " -> " : "") << sys.nodeLabel(path[k]);
            std::cout << "\n";

        } else if (choice == 12) {
            std::cout << "  1 All deployments   2 For one incident   3 For one resource\n";
            int v = 1;
            if (!readInt("View: ", v, 1, 3)) break;
            auto show = [&](MultiLinkedList<Deployment>::Node& d) {
                std::cout << "  t=" << std::setw(6) << d.val.at << "  #" << std::setw(4) << sys.incidents[d.row].id
                          << " " << std::left << std::setw(11) << sys.incidents[d.row].type << std::right << "<- "
                          << std::left << std::setw(16) << sys.resources[d.col].name << std::right << d.val.km << " km";
                if (d.val.hospital >= 0) std::cout << "  bed: " << sys.hospitals[d.val.hospital].name;
                std::cout << "\n";
            };
            MultiLinkedList<Deployment>& dep = sys.deployments();
            if (v == 1) {
                std::cout << "  " << dep.size() << " deployment(s), oldest first:\n";
                dep.forEachAll(show);
            } else if (v == 2) {
                int id = 0;
                if (!readInt("Incident id: ", id, 1, 1000000)) break;
                int i = sys.incidentIndex(id);
                if (i < 0) { std::cout << "  No incident #" << id << "\n"; continue; }
                std::cout << "  " << dep.rowSize(i) << " unit(s) sent to incident #" << id << ":\n";
                dep.forEachInRow(i, show);
            } else {
                std::string name;
                if (!readLine("Resource name: ", name)) break;
                int r = sys.resourceByName(name);
                if (r < 0) { std::cout << "  No resource named \"" << name << "\" (try option 8)\n"; continue; }
                std::cout << "  " << dep.colSize(r) << " job(s) for " << name << ":\n";
                dep.forEachInCol(r, show);
            }

        } else if (choice == 13) {
            std::string out;
            if (!readLine("Folder [Enter = saved]: ", out)) break;
            if (out.empty()) out = "saved";
            if (out == "data") { std::cout << "  Not overwriting data/ (the tests use it). Pick another folder.\n"; continue; }
            if (sys.save(out)) std::cout << "  Saved to " << out << "/. Continue later with: .\\build\\nexa.exe " << out << "\n";
            else std::cout << "  Could not save to " << out << "/\n";

        } else if (choice == 14) {
            runDemos(dir);

        } else if (choice == 15) {
            SimConfig cfg;
            int hours = 1, seed = 0; double rate = 0;
            std::cout << "  Units drive to incidents, work on scene, return to base, and new incidents keep arriving.\n";
            if (!readInt("Hours to simulate (1-168): ", hours, 1, 168) ||
                !readDouble("New incidents per hour (0-60, 0 = just let time pass): ", rate, 0, 60) ||
                !readInt("Random seed (any number, same seed = same run): ", seed, 0, 1000000)) break;
            cfg.hours = hours; cfg.arrivalsPerHour = rate; cfg.seed = (unsigned)seed;
            Simulator sim(sys);
            SimReport r = sim.run(cfg);
            std::cout << "\n  Simulated t=" << r.start << " to t=" << r.end << " (" << hours << " h)\n"
                      << "  Dispatches: " << r.dispatched << "   Resolved: " << r.resolved
                      << "   Still open: " << r.stillOpen << "   Peak waiting: " << r.peakOpen << "\n";
            if (r.arrivals > 0) {
                std::cout << "  New incidents: " << r.arrivals << ", reached by a unit: " << r.answered << " ("
                          << 100.0 * r.answered / r.arrivals << "%)\n"
                          << "  Response time in minutes, counting EVERY new incident\n"
                          << "  (one not reached yet counts as the time it has waited so far):\n"
                          << "    average " << r.avgAll << "   median " << r.medianAll << "   90% within " << r.p90All
                          << "   worst " << r.maxAll << "\n";
                if (r.answered > 0)
                    std::cout << "  Only the " << r.answered << " reached: average " << r.avgResponse << "   median "
                              << r.medianResponse << "   (flatters the result if many are still waiting)\n";
                std::cout << "  Average wait for a free unit: " << r.avgWait << " min\n"
                          << "  By severity            reached   median if reached   avg (all)   90% within (all)\n";
                const char* bands[3] = {"minor    (1-3)", "serious  (4-7)", "critical (8-10)"};
                for (int b = 2; b >= 0; b--) {
                    if (r.sevTotal[b] == 0) continue;
                    std::cout << "    " << std::left << std::setw(17) << bands[b] << std::right
                              << std::setw(4) << r.sevReached[b] << "/" << std::left << std::setw(4) << r.sevTotal[b] << std::right
                              << std::setw(14) << r.sevMedianReached[b] << std::setw(17) << r.sevAvgAll[b]
                              << std::setw(16) << r.sevP90All[b] << "\n";
                }
            }
            std::cout << "  Unit utilisation: " << r.utilization * 100 << "%\n  By unit type:\n";
            for (int t = 0; t < Simulator::TYPE_COUNT; t++) {
                std::cout << "    " << std::left << std::setw(12) << Simulator::typeName(t) << std::right
                          << std::setw(4) << r.typeAnswered[t] << " reached";
                if (r.typeAnswered[t]) std::cout << ", avg " << r.typeAvgResponse[t] << " min";
                if (r.typeOpen[t]) std::cout << ", " << r.typeOpen[t] << " still waiting";
                std::cout << "\n";
            }
            std::cout << "  (Undo history was cleared: past actions can't be undone once time has moved on.)\n";
        }
    }
    std::cout << "Goodbye.\n";
    return 0;
}
