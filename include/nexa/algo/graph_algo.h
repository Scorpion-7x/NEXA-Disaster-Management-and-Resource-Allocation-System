#pragma once
#include <limits>
#include "../ds/graph.h"
#include "../ds/queue.h"
#include "../ds/stack.h"
#include "../ds/priority_queue.h"

namespace nexa {

const double INF_DIST = std::numeric_limits<double>::infinity();

// Breadth-first search from src. Returns nodes in visit order; if hops is given,
// (*hops)[v] = fewest edges from src to v, or -1 if unreachable. O(V + E).
inline DynamicArray<int> bfs(const Graph& g, int src, DynamicArray<int>* hops = nullptr) {
    int n = g.nodeCount();
    DynamicArray<int> h, order;
    for (int i = 0; i < n; i++) h.push_back(-1);
    Queue<int> q;
    h[src] = 0; q.enqueue(src);
    while (!q.empty()) {
        int u = q.dequeue();
        order.push_back(u);
        for (const Edge& e : g.neighbors(u))
            if (h[e.to] == -1) { h[e.to] = h[u] + 1; q.enqueue(e.to); }
    }
    if (hops) *hops = h;
    return order;
}

// Depth-first search from src (iterative, with an explicit stack). Returns visit order. O(V + E).
inline DynamicArray<int> dfs(const Graph& g, int src) {
    int n = g.nodeCount();
    DynamicArray<bool> seen;
    for (int i = 0; i < n; i++) seen.push_back(false);
    DynamicArray<int> order;
    Stack<int> st;
    st.push(src);
    while (!st.empty()) {
        int u = st.pop();
        if (seen[u]) continue;
        seen[u] = true;
        order.push_back(u);
        const DynamicArray<Edge>& nb = g.neighbors(u);
        for (int i = nb.size() - 1; i >= 0; i--)          // reversed so the first neighbour is visited first
            if (!seen[nb[i].to]) st.push(nb[i].to);
    }
    return order;
}

// Labels each node with a component number 0..count-1 (undirected graphs). Returns count. O(V + E).
inline int connectedComponents(const Graph& g, DynamicArray<int>& comp) {
    int n = g.nodeCount(), count = 0;
    comp.clear();
    for (int i = 0; i < n; i++) comp.push_back(-1);
    Queue<int> q;
    for (int s = 0; s < n; s++) {
        if (comp[s] != -1) continue;
        comp[s] = count; q.enqueue(s);
        while (!q.empty()) {
            int u = q.dequeue();
            for (const Edge& e : g.neighbors(u))
                if (comp[e.to] == -1) { comp[e.to] = count; q.enqueue(e.to); }
        }
        count++;
    }
    return count;
}

struct ShortestPaths {
    int src = -1;
    DynamicArray<double> dist;   // INF_DIST if unreachable (or not reached before an early stop)
    DynamicArray<int> prev;      // previous node on the shortest path, -1 for src / unreachable
};

namespace detail {
struct DistItem { double d = 0; int v = 0; };
struct CloserFirst { bool operator()(const DistItem& a, const DistItem& b) const { return a.d < b.d; } };

// Dijkstra with a binary heap and lazy deletion: a node may be pushed several times,
// stale entries are skipped when popped. O((V + E) log V).
// Stops as soon as a node with isTarget(v) is settled and returns it (-1 if none).
template <typename Pred>
int dijkstraImpl(const Graph& g, int src, ShortestPaths& sp, Pred isTarget) {
    int n = g.nodeCount();
    sp.src = src; sp.dist.clear(); sp.prev.clear();
    for (int i = 0; i < n; i++) { sp.dist.push_back(INF_DIST); sp.prev.push_back(-1); }
    DynamicArray<bool> done;
    for (int i = 0; i < n; i++) done.push_back(false);
    PriorityQueue<DistItem, CloserFirst> pq;
    sp.dist[src] = 0; pq.push(DistItem{0, src});
    while (!pq.empty()) {
        DistItem it = pq.pop();
        int u = it.v;
        if (done[u]) continue;                   // stale entry
        done[u] = true;
        if (isTarget(u)) return u;               // dist[u] is final once u is popped
        for (const Edge& e : g.neighbors(u)) {
            double nd = sp.dist[u] + e.w;
            if (nd < sp.dist[e.to]) {            // relax
                sp.dist[e.to] = nd; sp.prev[e.to] = u;
                pq.push(DistItem{nd, e.to});
            }
        }
    }
    return -1;
}
}  // namespace detail

// Shortest distances from src to every node.
inline ShortestPaths dijkstra(const Graph& g, int src) {
    ShortestPaths sp;
    detail::dijkstraImpl(g, src, sp, [](int) { return false; });
    return sp;
}

// Closest node v (by shortest-path distance) with isTarget(v), or -1 if none is reachable.
// Stops early, so it is cheaper than a full dijkstra when a target is nearby.
// Example: nearest AVAILABLE ambulance to an incident.
template <typename Pred>
int nearestTarget(const Graph& g, int src, Pred isTarget, ShortestPaths& sp) {
    return detail::dijkstraImpl(g, src, sp, isTarget);
}

// Nodes on the shortest path src -> t (inclusive), or empty if t was not reached.
inline DynamicArray<int> pathTo(const ShortestPaths& sp, int t) {
    DynamicArray<int> rev, path;
    if (t < 0 || t >= sp.dist.size() || sp.dist[t] == INF_DIST) return path;
    for (int v = t; v != -1; v = sp.prev[v]) rev.push_back(v);
    for (int i = rev.size() - 1; i >= 0; i--) path.push_back(rev[i]);
    return path;
}

}  // namespace nexa
