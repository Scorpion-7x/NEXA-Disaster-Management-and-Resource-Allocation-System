#include <iostream>
#include <string>
#include "nexa/ds/priority_queue.h"
#include "nexa/io/logger.h"
#include "nexa/io/store.h"

using namespace nexa;

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "data";
    Logger log("logs/operations.log");

    DynamicArray<Incident> incidents;
    DynamicArray<Resource> resources;
    DynamicArray<Hospital> hospitals;
    if (!loadIncidents(dir + "/incidents.csv", incidents) ||
        !loadResources(dir + "/resources.csv", resources) ||
        !loadHospitals(dir + "/hospitals.csv", hospitals)) {
        std::cerr << "Could not load CSV files from '" << dir << "'. Run from the repo root.\n";
        return 1;
    }
    log.info("Loaded " + std::to_string(incidents.size()) + " incidents, " +
             std::to_string(resources.size()) + " resources, " +
             std::to_string(hospitals.size()) + " hospitals");

    // Phase-1 demo: process incidents in priority order
    PriorityQueue<Incident, IncidentHigherPriority> pq;
    for (const Incident& x : incidents) pq.push(x);

    std::cout << "NEXA - incidents by priority (top 10)\n";
    for (int i = 0; i < 10 && !pq.empty(); i++) {
        Incident x = pq.pop();
        std::cout << "  #" << x.id << "  sev " << x.severity << "  " << x.type << "  - " << x.description << "\n";
    }
    return 0;
}
