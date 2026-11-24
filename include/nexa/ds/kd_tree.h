#pragma once
#include <stdexcept>
#include "nexa/ds/dynamic_array.h"
#include "nexa/ds/priority_queue.h"

namespace nexa {

// 2-D KD Tree for nearest-resource queries.
//
//   build (balanced, median split)   O(n log n)
//   insert (unbalanced)              O(log n) average
//   nearest / kNearest               O(log n) average, O(n) worst case
//   rangeSearch / withinRadius       O(sqrt(n) + m) for m results
//
// Points are plain (x, y, id). For geographic data use x = lat and
// y = lon * cos(lat of the city), so that one unit means the same distance
// in both directions (cos(28.57 deg) ~ 0.878 for Noida).
// Distances are compared squared, so there is no sqrt in the search loop.

struct KDPoint {
    double x = 0, y = 0;
    int id = -1;      // index into your resources array
};

class KDTree {
    struct Node {
        KDPoint p;
        int axis;     // 0 = split on x, 1 = split on y
        Node* left;
        Node* right;
    };
    struct Cand {     // candidate for kNearest
        double d2;
        int id;
    };
    struct FartherFirst {   // max-heap on distance: top() is the worst of the current k
        bool operator()(const Cand& a, const Cand& b) const { return a.d2 > b.d2; }
    };

    Node* root_;
    int size_;

    static double coord(const KDPoint& p, int axis) { return axis == 0 ? p.x : p.y; }
    static double dist2(const KDPoint& p, double x, double y) {
        double dx = p.x - x, dy = p.y - y;
        return dx * dx + dy * dy;
    }
    static void swapP(KDPoint& a, KDPoint& b) { KDPoint t = a; a = b; b = t; }

    // Quickselect on a[lo..hi] by one axis: afterwards a[k] is in its sorted position,
    // everything left of it is <= and everything right of it is >=.
    static void select(KDPoint* a, int lo, int hi, int k, int axis) {
        while (lo < hi) {
            double pivot = coord(a[lo + (hi - lo) / 2], axis);
            int lt = lo, i = lo, gt = hi;              // 3-way partition: < | == | >
            while (i <= gt) {
                double c = coord(a[i], axis);
                if (c < pivot)      swapP(a[lt++], a[i++]);
                else if (c > pivot) swapP(a[i], a[gt--]);
                else                i++;
            }
            if (k < lt) hi = lt - 1;
            else if (k > gt) lo = gt + 1;
            else return;
        }
    }

    Node* buildRec(KDPoint* a, int lo, int hi, int depth) {
        if (lo > hi) return nullptr;
        int axis = depth % 2;
        int mid = lo + (hi - lo) / 2;
        select(a, lo, hi, mid, axis);                  // median becomes the splitting node
        Node* n = new Node{a[mid], axis, nullptr, nullptr};
        n->left = buildRec(a, lo, mid - 1, depth + 1);
        n->right = buildRec(a, mid + 1, hi, depth + 1);
        return n;
    }

    static void destroy(Node* n) {
        if (!n) return;
        destroy(n->left);
        destroy(n->right);
        delete n;
    }

    static int heightRec(const Node* n) {
        if (!n) return 0;
        int l = heightRec(n->left), r = heightRec(n->right);
        return 1 + (l > r ? l : r);
    }

    template <typename Pred>
    static void nearestRec(const Node* n, double x, double y, Pred& ok,
                           const Node*& best, double& bestD2) {
        if (!n) return;
        if (ok(n->p.id)) {
            double d2 = dist2(n->p, x, y);
            if (!best || d2 < bestD2) { best = n; bestD2 = d2; }
        }
        double diff = (n->axis == 0 ? x : y) - coord(n->p, n->axis);
        const Node* nearSide = diff < 0 ? n->left : n->right;
        const Node* farSide  = diff < 0 ? n->right : n->left;
        nearestRec(nearSide, x, y, ok, best, bestD2);
        // Pruning: the far side can only hold something closer if the
        // splitting line itself is closer than the best distance so far.
        if (!best || diff * diff < bestD2)
            nearestRec(farSide, x, y, ok, best, bestD2);
    }

    template <typename Pred>
    static void kNearestRec(const Node* n, double x, double y, int k, Pred& ok,
                            PriorityQueue<Cand, FartherFirst>& heap) {
        if (!n) return;
        if (ok(n->p.id)) {
            double d2 = dist2(n->p, x, y);
            if (heap.size() < k) heap.push(Cand{d2, n->p.id});
            else if (d2 < heap.top().d2) { heap.pop(); heap.push(Cand{d2, n->p.id}); }
        }
        double diff = (n->axis == 0 ? x : y) - coord(n->p, n->axis);
        const Node* nearSide = diff < 0 ? n->left : n->right;
        const Node* farSide  = diff < 0 ? n->right : n->left;
        kNearestRec(nearSide, x, y, k, ok, heap);
        if (heap.size() < k || diff * diff < heap.top().d2)
            kNearestRec(farSide, x, y, k, ok, heap);
    }

    static void rangeRec(const Node* n, double xlo, double xhi, double ylo, double yhi,
                         DynamicArray<int>& out) {
        if (!n) return;
        if (n->p.x >= xlo && n->p.x <= xhi && n->p.y >= ylo && n->p.y <= yhi) out.push_back(n->p.id);
        double c = coord(n->p, n->axis);
        double lo = n->axis == 0 ? xlo : ylo;
        double hi = n->axis == 0 ? xhi : yhi;
        if (lo <= c) rangeRec(n->left, xlo, xhi, ylo, yhi, out);    // left side holds coords <= c
        if (hi >= c) rangeRec(n->right, xlo, xhi, ylo, yhi, out);   // right side holds coords >= c
    }

    static void radiusRec(const Node* n, double x, double y, double r2, DynamicArray<int>& out) {
        if (!n) return;
        if (dist2(n->p, x, y) <= r2) out.push_back(n->p.id);
        double diff = (n->axis == 0 ? x : y) - coord(n->p, n->axis);
        const Node* nearSide = diff < 0 ? n->left : n->right;
        const Node* farSide  = diff < 0 ? n->right : n->left;
        radiusRec(nearSide, x, y, r2, out);
        if (diff * diff <= r2) radiusRec(farSide, x, y, r2, out);
    }

    struct AcceptAll { bool operator()(int) const { return true; } };

public:
    KDTree() : root_(nullptr), size_(0) {}
    ~KDTree() { destroy(root_); }
    KDTree(const KDTree&) = delete;
    KDTree& operator=(const KDTree&) = delete;

    // Replaces the tree with a balanced tree over pts.
    void build(const DynamicArray<KDPoint>& pts) {
        destroy(root_);
        root_ = nullptr;
        size_ = pts.size();
        if (size_ == 0) return;
        KDPoint* a = new KDPoint[size_];               // working copy: select() reorders it
        for (int i = 0; i < size_; i++) a[i] = pts[i];
        root_ = buildRec(a, 0, size_ - 1, 0);
        delete[] a;
    }

    // Adds one point without rebalancing (fine for a few new resources;
    // call build() again after many inserts).
    void insert(const KDPoint& p) {
        Node** cur = &root_;
        int depth = 0;
        while (*cur) {
            Node* n = *cur;
            cur = coord(p, n->axis) < coord(n->p, n->axis) ? &n->left : &n->right;
            depth++;
        }
        *cur = new Node{p, depth % 2, nullptr, nullptr};
        size_++;
    }

    // id of the nearest point whose id passes ok(id), or -1 if none does.
    // outDist2 (optional) receives the squared distance.
    template <typename Pred>
    int nearest(double x, double y, Pred ok, double* outDist2 = nullptr) const {
        const Node* best = nullptr;
        double bestD2 = 0;
        nearestRec(root_, x, y, ok, best, bestD2);
        if (outDist2) *outDist2 = best ? bestD2 : -1;
        return best ? best->p.id : -1;
    }
    int nearest(double x, double y, double* outDist2 = nullptr) const {
        return nearest(x, y, AcceptAll(), outDist2);
    }

    // Up to k nearest ids that pass ok(id), written to outIds nearest-first.
    // outIds must hold k ints. Returns how many were found.
    template <typename Pred>
    int kNearest(double x, double y, int k, int* outIds, Pred ok) const {
        if (k <= 0) return 0;
        PriorityQueue<Cand, FartherFirst> heap;
        kNearestRec(root_, x, y, k, ok, heap);
        int m = heap.size();
        for (int i = m - 1; i >= 0; i--) outIds[i] = heap.pop().id;   // heap pops farthest first
        return m;
    }
    int kNearest(double x, double y, int k, int* outIds) const {
        return kNearest(x, y, k, outIds, AcceptAll());
    }

    // ids of all points inside the rectangle [xlo, xhi] x [ylo, yhi], appended to out.
    void rangeSearch(double xlo, double xhi, double ylo, double yhi, DynamicArray<int>& out) const {
        rangeRec(root_, xlo, xhi, ylo, yhi, out);
    }

    // ids of all points within distance r of (x, y), appended to out.
    void withinRadius(double x, double y, double r, DynamicArray<int>& out) const {
        radiusRec(root_, x, y, r * r, out);
    }

    int size() const { return size_; }
    bool empty() const { return size_ == 0; }
    int height() const { return heightRec(root_); }
};

}  // namespace nexa
