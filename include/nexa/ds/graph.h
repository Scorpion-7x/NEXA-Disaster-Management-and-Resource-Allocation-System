#pragma once
#include <stdexcept>
#include "dynamic_array.h"

namespace nexa {

struct Edge {
    int to = 0;
    double w = 0;      // weight, e.g. road distance in km (must be >= 0)
};

// Weighted graph stored as adjacency lists: node u's edges are in adj_[u].
// Space O(V + E). addNode / addEdge: O(1) amortized. hasEdge: O(deg(u)).
// Undirected by default: addEdge(u, v) stores u->v and v->u.
class Graph {
    DynamicArray<DynamicArray<Edge>> adj_;
    int edges_ = 0;

    void checkNode(int u) const {
        if (u < 0 || u >= adj_.size()) throw std::out_of_range("Graph: bad node id");
    }
public:
    explicit Graph(int n = 0) { for (int i = 0; i < n; i++) addNode(); }

    int addNode() { adj_.push_back(DynamicArray<Edge>()); return adj_.size() - 1; }

    void addEdge(int u, int v, double w, bool directed = false) {
        checkNode(u); checkNode(v);
        if (w < 0) throw std::invalid_argument("Graph: negative edge weight");
        adj_[u].push_back(Edge{v, w});
        if (!directed && u != v) adj_[v].push_back(Edge{u, w});
        edges_++;
    }
    bool hasEdge(int u, int v) const {
        checkNode(u); checkNode(v);
        for (const Edge& e : adj_[u]) if (e.to == v) return true;
        return false;
    }
    const DynamicArray<Edge>& neighbors(int u) const { checkNode(u); return adj_[u]; }
    int degree(int u) const { return neighbors(u).size(); }
    int nodeCount() const { return adj_.size(); }
    int edgeCount() const { return edges_; }   // each undirected edge counted once
};

}  // namespace nexa
