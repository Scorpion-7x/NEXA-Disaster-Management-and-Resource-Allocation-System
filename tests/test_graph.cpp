#include <cmath>
#include "test_util.h"
#include "nexa/ds/graph.h"
#include "nexa/algo/graph_algo.h"
#include "nexa/map/city_map.h"
#include "nexa/io/store.h"
using namespace nexa;

static unsigned g_seed = 4242;
static int rnd(int mod) { g_seed = g_seed * 1103515245u + 12345u; return (int)((g_seed >> 8) % (unsigned)mod); }
static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

int main() {
    { // Graph basics
        Graph g(3);
        g.addEdge(0, 1, 2.5);
        CHECK(g.nodeCount() == 3 && g.edgeCount() == 1 && g.hasEdge(0, 1) && g.hasEdge(1, 0) && !g.hasEdge(0, 2));
        g.addEdge(1, 2, 1, true);   // directed
        CHECK(g.hasEdge(1, 2) && !g.hasEdge(2, 1) && g.degree(1) == 2);
        bool threw = false; try { g.addEdge(0, 2, -1); } catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
        threw = false; try { g.addEdge(0, 7, 1); } catch (const std::out_of_range&) { threw = true; }
        CHECK(threw);
        CHECK(g.addNode() == 3 && g.nodeCount() == 4);
    }
    { // BFS hops and order on a small graph:  0-1-2-3 plus shortcut 0-3, 4 isolated
        Graph g(5);
        g.addEdge(0, 1, 1); g.addEdge(1, 2, 1); g.addEdge(2, 3, 1); g.addEdge(0, 3, 1);
        DynamicArray<int> hops;
        DynamicArray<int> order = bfs(g, 0, &hops);
        CHECK(order.size() == 4 && order[0] == 0);
        CHECK(hops[0] == 0 && hops[1] == 1 && hops[3] == 1 && hops[2] == 2 && hops[4] == -1);
        DynamicArray<int> d = dfs(g, 0);   // goes deep along 0-1-2-3 before backtracking
        CHECK(d.size() == 4 && d[0] == 0 && d[1] == 1 && d[2] == 2 && d[3] == 3);
        DynamicArray<int> comp;
        CHECK(connectedComponents(g, comp) == 2 && comp[0] == comp[3] && comp[4] != comp[0]);
    }
    { // Dijkstra picks the cheaper long way:  0->1 (10) vs 0->2->3->1 (1+1+1)
        Graph g(5);
        g.addEdge(0, 1, 10); g.addEdge(0, 2, 1); g.addEdge(2, 3, 1); g.addEdge(3, 1, 1);
        ShortestPaths sp = dijkstra(g, 0);
        CHECK(near(sp.dist[1], 3) && near(sp.dist[3], 2) && sp.dist[4] == INF_DIST);
        DynamicArray<int> p = pathTo(sp, 1);
        CHECK(p.size() == 4 && p[0] == 0 && p[1] == 2 && p[2] == 3 && p[3] == 1);
        CHECK(pathTo(sp, 4).size() == 0 && pathTo(sp, 0).size() == 1);
    }
    { // Dijkstra vs Floyd-Warshall on random graphs (directed and undirected)
        bool ok = true;
        for (int trial = 0; trial < 40; trial++) {
            int n = 2 + rnd(25), m = rnd(n * 3);
            bool directed = trial % 2;
            Graph g(n);
            static double fw[30][30];
            for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) fw[i][j] = (i == j) ? 0 : INF_DIST;
            for (int e = 0; e < m; e++) {
                int u = rnd(n), v = rnd(n); double w = rnd(100);
                g.addEdge(u, v, w, directed);
                if (w < fw[u][v]) fw[u][v] = w;
                if (!directed && w < fw[v][u]) fw[v][u] = w;
            }
            for (int k = 0; k < n; k++) for (int i = 0; i < n; i++) for (int j = 0; j < n; j++)
                if (fw[i][k] + fw[k][j] < fw[i][j]) fw[i][j] = fw[i][k] + fw[k][j];
            for (int s = 0; s < n; s++) {
                ShortestPaths sp = dijkstra(g, s);
                for (int t = 0; t < n; t++) {
                    if (!(sp.dist[t] == fw[s][t] || near(sp.dist[t], fw[s][t]))) ok = false;
                    // the reconstructed path must use real edges and add up to dist
                    DynamicArray<int> p = pathTo(sp, t);
                    if (sp.dist[t] != INF_DIST) {
                        double sum = 0;
                        for (int i = 0; i + 1 < p.size(); i++) {
                            double best = INF_DIST;
                            for (const Edge& e : g.neighbors(p[i])) if (e.to == p[i + 1] && e.w < best) best = e.w;
                            if (best == INF_DIST) ok = false; else sum += best;
                        }
                        if (p.size() == 0 || p[0] != s || p.back() != t || !near(sum, sp.dist[t])) ok = false;
                    }
                }
            }
        }
        CHECK(ok);
    }
    { // nearestTarget stops at the closest matching node
        Graph g(6);   // 0 -2- 1 -2- 2 ; 0 -1- 3 -5- 4 ; 5 isolated
        g.addEdge(0, 1, 2); g.addEdge(1, 2, 2); g.addEdge(0, 3, 1); g.addEdge(3, 4, 5);
        ShortestPaths sp;
        int t = nearestTarget(g, 0, [](int v) { return v == 2 || v == 4; }, sp);
        CHECK(t == 2 && near(sp.dist[2], 4));
        CHECK(nearestTarget(g, 0, [](int v) { return v == 5; }, sp) == -1);
        CHECK(nearestTarget(g, 0, [](int v) { return v == 0; }, sp) == 0);   // source itself
    }
    { // haversine: Noida Sector 18 to Connaught Place is about 13 km
        double d = haversineKm(28.5708, 77.3261, 28.6315, 77.2167);
        CHECK(d > 12 && d < 15 && near(haversineKm(28.5, 77.3, 28.5, 77.3), 0));
    }
    { // CityMap from the real dataset: connected, sensible degrees, node ids line up
        DynamicArray<Incident> inc; DynamicArray<Resource> res; DynamicArray<Hospital> hos;
        bool loaded = loadIncidents("../data/incidents.csv", inc) && loadResources("../data/resources.csv", res) &&
                      loadHospitals("../data/hospitals.csv", hos);
        if (!loaded) loaded = loadIncidents("data/incidents.csv", inc) && loadResources("data/resources.csv", res) &&
                              loadHospitals("data/hospitals.csv", hos);
        CHECK(loaded);
        CityMap m;
        buildCityMap(inc, res, hos, m);
        int n = inc.size() + res.size() + hos.size();
        DynamicArray<int> comp;
        CHECK(m.g.nodeCount() == n && connectedComponents(m.g, comp) == 1);
        bool degOk = true; for (int v = 0; v < n; v++) if (m.g.degree(v) < 4) degOk = false;
        CHECK(degOk);
        CHECK(m.isResource(m.resourceNode(0)) && m.isHospital(m.hospitalNode(0)) && !m.isResource(m.incidentNode(0)));
        CHECK(m.resourceIndex(m.resourceNode(3)) == 3 && m.hospitalIndex(m.hospitalNode(2)) == 2);
        // road distance is never shorter than the straight line
        ShortestPaths sp = dijkstra(m.g, m.incidentNode(0));
        bool geoOk = true;
        for (int h = 0; h < hos.size(); h++) {
            double straight = haversineKm(inc[0].lat, inc[0].lon, hos[h].lat, hos[h].lon);
            if (sp.dist[m.hospitalNode(h)] < straight - 1e-9) geoOk = false;
        }
        CHECK(geoOk);
    }
    { // CityMap joins far-apart clusters
        DynamicArray<Incident> inc; DynamicArray<Resource> res; DynamicArray<Hospital> hos;
        for (int i = 0; i < 6; i++) { Incident x; x.lat = 28.5 + i * 0.001; x.lon = 77.3; inc.push_back(x); }
        for (int i = 0; i < 6; i++) { Resource x; x.lat = 19.0 + i * 0.001; x.lon = 72.8; res.push_back(x); }   // Mumbai
        CityMap m;
        buildCityMap(inc, res, hos, m, 2);
        DynamicArray<int> comp;
        CHECK(connectedComponents(m.g, comp) == 1);
    }
    TEST_END();
}
