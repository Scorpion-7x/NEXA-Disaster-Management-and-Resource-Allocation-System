#include <iostream>
#include <string>
#include "nexa/ds/priority_queue.h"
#include "nexa/ds/probing_hash_table.h"
#include "nexa/ds/avl_tree.h"
#include "nexa/algo/sort.h"
#include "nexa/algo/search.h"
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

    // Tier-1 demo: hashing, sorting, searching
    ProbingHashTable<std::string, int> byName;          // resource name -> index
    for (int i = 0; i < resources.size(); i++) byName.put(resources[i].name, i);
    const char* lookFor = "Rescue Team 1";
    if (int* idx = byName.get(lookFor))
        std::cout << "\nLookup '" << lookFor << "': " << resources[*idx].type << ", " << resources[*idx].status << "\n";

    DynamicArray<Incident> bySeverity = incidents;
    countingSort(bySeverity, [](const Incident& x) { return x.severity; }, 1, 10, true);
    std::cout << "Counting sort by severity: first sev " << bySeverity[0].severity
              << ", last sev " << bySeverity.back().severity << "\n";

    DynamicArray<Incident> byTime = incidents;
    mergeSort(byTime, [](const Incident& a, const Incident& b) { return a.reportedAt < b.reportedAt; });
    long long from = 1000, to = 1100;
    int first = 0;
    int n = rangeSearch(byTime, [](const Incident& x) { return x.reportedAt; }, from, to, &first);
    std::cout << "Incidents reported in [" << from << ", " << to << "]: " << n << "\n";

    DynamicArray<Incident> byId = incidents;
    quickSort(byId, [](const Incident& a, const Incident& b) { return a.id < b.id; });
    int pos = binarySearch(byId, [](const Incident& x) { return x.id; }, 25);
    if (pos >= 0) std::cout << "Binary search id 25: " << byId[pos].type << " - " << byId[pos].description << "\n";

    log.info("Tier-1 demo ran");

    // AVL tree demo: incidents indexed by report time (several can share a minute)
    AvlTree<long long, DynamicArray<int>> timeline;
    for (int i = 0; i < incidents.size(); i++) timeline.getOrInsert(incidents[i].reportedAt).push_back(i);
    std::cout << "\nAVL timeline: " << timeline.size() << " distinct times, height " << timeline.height()
              << ", " << timeline.rotationCount() << " rotations\n";
    int inWindow = 0;
    timeline.rangeQuery(from, to, [&](const long long&, DynamicArray<int>& ids) { inWindow += ids.size(); });
    std::cout << "AVL range [" << from << ", " << to << "]: " << inWindow << " incidents (matches range search above)\n";
    if (const long long* t = timeline.ceilingKey(1250)) {
        const Incident& x = incidents[(*timeline.get(*t))[0]];
        std::cout << "First incident at or after t=1250: #" << x.id << " at t=" << *t << " (" << x.type << ")\n";
    }
    log.info("AVL demo ran");
    return 0;
}
