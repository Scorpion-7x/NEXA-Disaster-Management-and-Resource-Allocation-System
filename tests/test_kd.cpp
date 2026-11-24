#include <cstdio>
#include "test_util.h"
#include "nexa/ds/kd_tree.h"
#include "nexa/ds/dynamic_array.h"
using namespace nexa;

static unsigned g_seed = 4242;
static int rnd(int mod) { g_seed = g_seed * 1103515245u + 12345u; return (int)((g_seed >> 8) % (unsigned)mod); }
static double rndD(double lo, double hi) { return lo + (hi - lo) * rnd(1000000) / 1000000.0; }

static double d2(const KDPoint& p, double x, double y) { return (p.x - x) * (p.x - x) + (p.y - y) * (p.y - y); }

// Equal up to floating-point rounding. Optimising compilers may compute the same distance with a
// different last bit in two places (e.g. fused multiply-add), so exact == is too strict for doubles.
static bool close(double a, double b) {
    double m = a > b ? a : b;
    if (m < 1e-300) return true;
    double diff = a > b ? a - b : b - a;
    return diff <= 1e-9 * m;
}

// Brute force reference: nearest id passing ok, or -1
template <typename Pred>
static int bruteNearest(const DynamicArray<KDPoint>& pts, double x, double y, Pred ok, double& best) {
    int id = -1; best = -1;
    for (int i = 0; i < pts.size(); i++) {
        if (!ok(pts[i].id)) continue;
        double d = d2(pts[i], x, y);
        if (id < 0 || d < best) { id = pts[i].id; best = d; }
    }
    return id;
}

static bool contains(const DynamicArray<int>& v, int x) {
    for (int i = 0; i < v.size(); i++) if (v[i] == x) return true;
    return false;
}

int main() {
    { // empty and single point
        KDTree t;
        CHECK(t.empty() && t.size() == 0 && t.height() == 0);
        CHECK(t.nearest(1, 1) == -1);
        int ids[3];
        CHECK(t.kNearest(0, 0, 3, ids) == 0);
        DynamicArray<int> out;
        t.withinRadius(0, 0, 100, out);
        CHECK(out.size() == 0);
        t.insert(KDPoint{5, 5, 42});
        double dd;
        CHECK(t.nearest(0, 0, &dd) == 42 && dd == 50 && t.size() == 1);
    }
    { // small hand-checked case
        DynamicArray<KDPoint> pts;
        pts.push_back({2, 3, 0}); pts.push_back({5, 4, 1}); pts.push_back({9, 6, 2});
        pts.push_back({4, 7, 3}); pts.push_back({8, 1, 4}); pts.push_back({7, 2, 5});
        KDTree t; t.build(pts);
        CHECK(t.size() == 6 && t.height() == 3);
        CHECK(t.nearest(9, 2) == 4);               // (8,1) at d2 = 2 beats (7,2) at d2 = 4
        CHECK(t.nearest(3, 4.5) == 0 || t.nearest(3, 4.5) == 1);
        CHECK(t.nearest(4, 7) == 3);               // exact hit
        CHECK(t.nearest(9, 2, [](int id) { return id != 4; }) == 5);   // skip the closest
        CHECK(t.nearest(0, 0, [](int) { return false; }) == -1);
        int ids[6];
        CHECK(t.kNearest(9, 2, 3, ids) == 3 && ids[0] == 4 && ids[1] == 5);
        CHECK(t.kNearest(9, 2, 10, ids) == 6);      // k larger than tree
        DynamicArray<int> box;
        t.rangeSearch(4, 8, 1, 4, box);             // (5,4) (8,1) (7,2)
        CHECK(box.size() == 3 && contains(box, 1) && contains(box, 4) && contains(box, 5));
    }
    { // 2000 random points (lat/lon around Noida) vs brute force
        DynamicArray<KDPoint> pts;
        for (int i = 0; i < 2000; i++) pts.push_back({rndD(28.40, 28.70), rndD(77.20, 77.55), i});
        KDTree t; t.build(pts);
        CHECK(t.size() == 2000);
        CHECK(t.height() <= 12);                    // balanced: ceil(log2(2001)) = 11
        bool same = true, sameFiltered = true, sameK = true, sameR = true, sameBox = true;
        auto even = [](int id) { return id % 2 == 0; };
        for (int q = 0; q < 300; q++) {
            double x = rndD(28.35, 28.75), y = rndD(77.15, 77.60);
            double bd, td;
            bruteNearest(pts, x, y, [](int) { return true; }, bd);
            t.nearest(x, y, &td);
            if (!close(td, bd)) same = false;       // compare distance: ties may pick different ids
            bruteNearest(pts, x, y, even, bd);
            int id = t.nearest(x, y, even, &td);
            if (!close(td, bd) || id % 2 != 0) sameFiltered = false;

            // kNearest: the K distances returned must be the K smallest distances overall, nearest first.
            // (Compare distances, not ids: points at equal distance may come back in either order.)
            const int K = 7;
            int ids[K] = {0};
            int m = t.kNearest(x, y, K, ids);
            double kBest[K];                         // brute force: K smallest distances, kept sorted
            int have = 0;
            for (int i = 0; i < pts.size(); i++) {
                double d = d2(pts[i], x, y);
                if (have == K && d >= kBest[K - 1]) continue;
                int j = have < K ? have++ : K - 1;
                while (j > 0 && kBest[j - 1] > d) { kBest[j] = kBest[j - 1]; j--; }
                kBest[j] = d;
            }
            if (m != K) { sameK = false; std::printf("  kNearest returned %d of %d\n", m, K); }
            for (int j = 0; j < m; j++) {
                double got = d2(pts[ids[j]], x, y);
                if (!close(got, kBest[j])) {
                    if (sameK) std::printf("  kNearest query %d (%.6f, %.6f): result %d has d2 %.17g, expected %.17g\n",
                                           q, x, y, j, got, kBest[j]);
                    sameK = false;
                }
            }

            // withinRadius and rangeSearch vs brute force counts
            double r = 0.02;
            DynamicArray<int> got; t.withinRadius(x, y, r, got);
            int want = 0;
            for (int i = 0; i < pts.size(); i++) if (d2(pts[i], x, y) <= r * r) want++;
            if (got.size() != want) sameR = false;
            for (int i = 0; i < got.size(); i++) if (d2(pts[got[i]], x, y) > r * r) sameR = false;

            DynamicArray<int> inBox; t.rangeSearch(x - 0.03, x + 0.03, y - 0.02, y + 0.02, inBox);
            int wantBox = 0;
            for (int i = 0; i < pts.size(); i++)
                if (pts[i].x >= x - 0.03 && pts[i].x <= x + 0.03 && pts[i].y >= y - 0.02 && pts[i].y <= y + 0.02) wantBox++;
            if (inBox.size() != wantBox) sameBox = false;
        }
        CHECK(same);
        CHECK(sameFiltered);
        CHECK(sameK);
        CHECK(sameR);
        CHECK(sameBox);
    }
    { // many duplicate coordinates, plus inserts after build
        DynamicArray<KDPoint> pts;
        for (int i = 0; i < 300; i++) pts.push_back({(double)rnd(4), (double)rnd(4), i});
        KDTree t; t.build(pts);
        for (int i = 300; i < 400; i++) { KDPoint p{(double)rnd(4), (double)rnd(4), i}; pts.push_back(p); t.insert(p); }
        CHECK(t.size() == 400);
        bool ok = true;
        for (int q = 0; q < 100; q++) {
            double x = rndD(-1, 5), y = rndD(-1, 5), bd, td;
            bruteNearest(pts, x, y, [](int id) { return id >= 300; }, bd);
            t.nearest(x, y, [](int id) { return id >= 300; }, &td);
            if (td != bd) ok = false;
        }
        CHECK(ok);
        DynamicArray<int> all; t.rangeSearch(-10, 10, -10, 10, all);
        CHECK(all.size() == 400);
    }
    { // rebuild replaces the old tree
        DynamicArray<KDPoint> a, b;
        a.push_back({0, 0, 1});
        b.push_back({10, 10, 2}); b.push_back({11, 11, 3});
        KDTree t; t.build(a); t.build(b);
        CHECK(t.size() == 2 && t.nearest(0, 0) == 2);
        DynamicArray<KDPoint> none; t.build(none);
        CHECK(t.empty() && t.nearest(0, 0) == -1);
    }
    TEST_END();
}
