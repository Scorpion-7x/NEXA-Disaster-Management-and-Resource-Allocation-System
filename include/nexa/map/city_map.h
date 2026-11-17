#pragma once
#include <cmath>
#include "../ds/graph.h"
#include "../algo/graph_algo.h"
#include "../model/incident.h"
#include "../model/resource.h"
#include "../model/hospital.h"

namespace nexa {

// Great-circle distance between two lat/lon points, in km.
inline double haversineKm(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0, D = 3.14159265358979323846 / 180.0;
    double dlat = (lat2 - lat1) * D, dlon = (lon2 - lon1) * D;
    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(lat1 * D) * std::cos(lat2 * D) * std::sin(dlon / 2) * std::sin(dlon / 2);
    return 2 * R * std::asin(std::sqrt(a));
}

// Road network over every incident, resource and hospital.
// Node ids: incidents first, then resources, then hospitals:
//   incident i -> i,  resource r -> resBase + r,  hospital h -> hosBase + h.
struct CityMap {
    Graph g;
    int incCount = 0, resBase = 0, hosBase = 0;

    int incidentNode(int i) const { return i; }
    int resourceNode(int r) const { return resBase + r; }
    int hospitalNode(int h) const { return hosBase + h; }
    bool isResource(int v) const { return v >= resBase && v < hosBase; }
    bool isHospital(int v) const { return v >= hosBase; }
    int resourceIndex(int v) const { return v - resBase; }
    int hospitalIndex(int v) const { return v - hosBase; }
};

// Builds a synthetic road network (the dataset has locations but no roads):
// each place is joined to its k nearest places, and the edge weight is the
// straight-line distance times roadFactor (roads are rarely straight).
// Any separate clusters are then joined by their closest pair, so every
// place can reach every other. O(n^2) — fine for city-sized data.
inline void buildCityMap(const DynamicArray<Incident>& inc, const DynamicArray<Resource>& res,
                         const DynamicArray<Hospital>& hos, CityMap& m,
                         int k = 4, double roadFactor = 1.3) {
    DynamicArray<double> lat, lon;
    for (const Incident& x : inc) { lat.push_back(x.lat); lon.push_back(x.lon); }
    for (const Resource& x : res) { lat.push_back(x.lat); lon.push_back(x.lon); }
    for (const Hospital& x : hos) { lat.push_back(x.lat); lon.push_back(x.lon); }
    int n = lat.size();
    m.g = Graph(n);
    m.incCount = inc.size(); m.resBase = inc.size(); m.hosBase = inc.size() + res.size();
    auto dist = [&](int a, int b) { return haversineKm(lat[a], lon[a], lat[b], lon[b]); };

    // k nearest neighbours of each node (k passes of "closest not yet picked")
    DynamicArray<bool> picked;
    for (int i = 0; i < n; i++) picked.push_back(false);
    for (int u = 0; u < n; u++) {
        for (int i = 0; i < n; i++) picked[i] = false;
        for (int pass = 0; pass < k && pass < n - 1; pass++) {
            int best = -1; double bd = 0;
            for (int v = 0; v < n; v++) {
                if (v == u || picked[v]) continue;
                double d = dist(u, v);
                if (best == -1 || d < bd) { best = v; bd = d; }
            }
            picked[best] = true;
            if (!m.g.hasEdge(u, best)) m.g.addEdge(u, best, bd * roadFactor);
        }
    }

    // join clusters until the graph is connected
    DynamicArray<int> comp;
    while (n > 0 && connectedComponents(m.g, comp) > 1) {
        int bu = -1, bv = -1; double bd = 0;
        for (int u = 0; u < n; u++) {
            if (comp[u] != 0) continue;
            for (int v = 0; v < n; v++) {
                if (comp[v] == 0) continue;
                double d = dist(u, v);
                if (bu == -1 || d < bd) { bu = u; bv = v; bd = d; }
            }
        }
        m.g.addEdge(bu, bv, bd * roadFactor);
    }
}

}  // namespace nexa
