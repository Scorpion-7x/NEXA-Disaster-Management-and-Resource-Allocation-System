#include <string>
#include "test_util.h"
#include "nexa/ds/avl_tree.h"
#include "nexa/ds/dynamic_array.h"
#include "nexa/model/incident.h"
using namespace nexa;

static unsigned g_seed = 777;
static int rnd(int mod) { g_seed = g_seed * 1103515245u + 12345u; return (int)((g_seed >> 8) % (unsigned)mod); }

// Upper bound on AVL height for n nodes: about 1.44 * log2(n + 2).
static bool heightOk(int height, int n) {
    double lg = 0; for (int x = n + 2; x > 1; x >>= 1) lg += 1;
    return height <= 1.45 * lg + 1;
}

int main() {
    { // empty tree
        AvlTree<int, int> t;
        CHECK(t.empty() && t.height() == 0 && t.get(1) == nullptr && !t.remove(1));
        CHECK(t.minKey() == nullptr && t.maxKey() == nullptr && t.floorKey(5) == nullptr && t.isValid());
    }
    { // the four rotation cases, each on three keys
        int cases[4][3] = {{3, 2, 1}, {1, 2, 3}, {3, 1, 2}, {1, 3, 2}};   // LL, RR, LR, RL
        for (auto& c : cases) {
            AvlTree<int, int> t;
            for (int k : c) t.put(k, k);
            CHECK(t.height() == 2 && t.isValid() && t.size() == 3);
        }
    }
    { // sorted inserts (worst case for a plain BST) stay balanced
        AvlTree<int, int> t;
        for (int i = 0; i < 10000; i++) t.put(i, i * 3);
        CHECK(t.size() == 10000 && t.isValid() && heightOk(t.height(), 10000));
        CHECK(t.height() <= 15);   // perfectly balanced would be 14
        bool ok = true; for (int i = 0; i < 10000; i++) { int* v = t.get(i); if (!v || *v != i * 3) ok = false; }
        CHECK(ok && t.get(10000) == nullptr && t.get(-1) == nullptr);
        t.put(42, -1); CHECK(*t.get(42) == -1 && t.size() == 10000);     // update, not duplicate
        CHECK(*t.minKey() == 0 && *t.maxKey() == 9999);
    }
    { // in-order traversal is sorted
        AvlTree<int, int> t;
        for (int i = 0; i < 1000; i++) t.put(rnd(100000), i);
        int prev = -1, count = 0; bool sorted = true;
        t.for_each([&](const int& k, int&) { if (k <= prev) sorted = false; prev = k; count++; });
        CHECK(sorted && count == t.size());
    }
    { // removal: leaf, one child, two children, root, missing key; tree stays valid throughout
        AvlTree<int, int> t;
        for (int i = 1; i <= 15; i++) t.put(i, i);
        CHECK(t.remove(1) && t.isValid());        // leaf
        CHECK(t.remove(2) && t.isValid());        // node that may have one child
        CHECK(t.remove(8) && t.isValid());        // root / two children
        CHECK(!t.remove(8) && !t.remove(100));
        CHECK(t.size() == 12 && !t.contains(8) && t.contains(9) && *t.get(9) == 9);
        for (int i = 1; i <= 15; i++) t.remove(i);
        CHECK(t.empty() && t.height() == 0 && t.isValid());
        t.put(5, 5); CHECK(t.size() == 1 && *t.get(5) == 5);   // reusable after emptying
    }
    { // random insert/remove against a simple reference array
        AvlTree<int, int> t;
        const int N = 2000;
        int ref[N]; for (int i = 0; i < N; i++) ref[i] = -1;   // -1 = absent
        bool ok = true;
        for (int step = 0; step < 30000; step++) {
            int k = rnd(N), op = rnd(3);
            if (op < 2) { t.put(k, step); ref[k] = step; }
            else { bool had = ref[k] != -1; if (t.remove(k) != had) ok = false; ref[k] = -1; }
            if (step % 3000 == 0 && !t.isValid()) ok = false;
        }
        int live = 0;
        for (int k = 0; k < N; k++) {
            int* v = t.get(k);
            if (ref[k] == -1) { if (v) ok = false; }
            else { live++; if (!v || *v != ref[k]) ok = false; }
        }
        CHECK(ok && t.isValid() && t.size() == live && heightOk(t.height(), live));
        CHECK(t.rotationCount() > 0);
    }
    { // floor / ceiling
        AvlTree<int, int> t;
        for (int k = 10; k <= 100; k += 10) t.put(k, k);
        CHECK(*t.floorKey(55) == 50 && *t.ceilingKey(55) == 60);
        CHECK(*t.floorKey(50) == 50 && *t.ceilingKey(50) == 50);
        CHECK(t.floorKey(5) == nullptr && *t.ceilingKey(5) == 10);
        CHECK(*t.floorKey(500) == 100 && t.ceilingKey(500) == nullptr);
    }
    { // range query: inclusive bounds, ascending order, empty ranges
        AvlTree<int, int> t;
        for (int k = 0; k < 100; k += 2) t.put(k, k);   // 0,2,...,98
        DynamicArray<int> got;
        t.rangeQuery(10, 20, [&](const int& k, int&) { got.push_back(k); });
        CHECK(got.size() == 6 && got[0] == 10 && got[5] == 20);
        got.clear(); t.rangeQuery(11, 19, [&](const int& k, int&) { got.push_back(k); });
        CHECK(got.size() == 4 && got[0] == 12 && got[3] == 18);
        got.clear(); t.rangeQuery(-50, 1000, [&](const int& k, int&) { got.push_back(k); });
        CHECK(got.size() == 50);
        got.clear(); t.rangeQuery(41, 41, [&](const int& k, int&) { got.push_back(k); });
        CHECK(got.size() == 0);
        got.clear(); t.rangeQuery(60, 40, [&](const int& k, int&) { got.push_back(k); });   // lo > hi
        CHECK(got.size() == 0);
    }
    { // string keys, custom comparator (descending)
        AvlTree<std::string, int> s;
        s.put("FIRE", 1); s.put("AMBULANCE", 2); s.put("BOAT", 3);
        CHECK(*s.minKey() == "AMBULANCE" && *s.maxKey() == "FIRE" && *s.get("BOAT") == 3);
        struct Greater { bool operator()(int a, int b) const { return a > b; } };
        AvlTree<int, int, Greater> d;
        for (int i = 0; i < 50; i++) d.put(i, i);
        int first = -1; bool firstSet = false;
        d.for_each([&](const int& k, int&) { if (!firstSet) { first = k; firstSet = true; } });
        CHECK(first == 49 && d.isValid());
    }
    { // grouping incidents by report time with getOrInsert (several incidents can share a time)
        AvlTree<long long, DynamicArray<int>> byTime;
        long long times[] = {300, 100, 200, 100, 300, 300};
        for (int i = 0; i < 6; i++) byTime.getOrInsert(times[i]).push_back(i);
        CHECK(byTime.size() == 3);
        CHECK(byTime.get(100)->size() == 2 && byTime.get(300)->size() == 3);
        int n = 0;
        byTime.rangeQuery(150LL, 300LL, [&](const long long&, DynamicArray<int>& ids) { n += ids.size(); });
        CHECK(n == 4);
        CHECK(byTime.remove(300) && byTime.size() == 2 && byTime.isValid());
    }
    TEST_END();
}
