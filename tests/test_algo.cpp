#include <string>
#include "test_util.h"
#include "nexa/ds/probing_hash_table.h"
#include "nexa/ds/hash_table.h"
#include "nexa/algo/sort.h"
#include "nexa/algo/search.h"
#include "nexa/model/incident.h"
using namespace nexa;

struct IntLess { bool operator()(int a, int b) const { return a < b; } };

static unsigned g_seed = 12345;
static int rnd(int mod) { g_seed = g_seed * 1103515245u + 12345u; return (int)((g_seed >> 8) % (unsigned)mod); }

// Runs one sort function over several tricky inputs and checks the result against a known-good sort.
template <typename SortFn>
static void checkIntSort(SortFn sortFn, const char* name) {
    const int sizes[] = {0, 1, 2, 3, 15, 16, 17, 100, 1000, 5000};
    for (int n : sizes) {
        for (int pattern = 0; pattern < 5; pattern++) {
            DynamicArray<int> v;
            for (int i = 0; i < n; i++) {
                switch (pattern) {
                    case 0: v.push_back(rnd(1000000)); break;   // random
                    case 1: v.push_back(i); break;              // already sorted
                    case 2: v.push_back(n - i); break;          // reversed
                    case 3: v.push_back(7); break;              // all equal
                    case 4: v.push_back(rnd(5)); break;         // many duplicates
                }
            }
            long long sumBefore = 0; for (int x : v) sumBefore += x;
            sortFn(v);
            long long sumAfter = 0; for (int x : v) sumAfter += x;
            bool ok = v.size() == n && isSorted(v, IntLess()) && sumBefore == sumAfter;
            if (!ok) std::printf("  %s failed: n=%d pattern=%d\n", name, n, pattern);
            CHECK(ok);
        }
    }
}

static Incident mk(int id, int sev, long long t) {
    Incident x; x.id = id; x.severity = sev; x.reportedAt = t; return x;
}

int main() {
    { // ProbingHashTable: ints, growth, update, remove
        ProbingHashTable<int, int> h;
        for (int i = 0; i < 2000; i++) h.put(i, i * 2);
        CHECK(h.size() == 2000 && h.loadFactor() <= 0.5);
        bool ok = true; for (int i = 0; i < 2000; i++) { int* v = h.get(i); if (!v || *v != i * 2) ok = false; }
        CHECK(ok);
        h.put(5, 999); CHECK(*h.get(5) == 999 && h.size() == 2000);
        CHECK(h.remove(5) && !h.contains(5) && !h.remove(5) && h.size() == 1999);
        CHECK(h.get(-3) == nullptr);
        h.put(5, 1); CHECK(*h.get(5) == 1 && h.size() == 2000);   // reuse after delete
    }
    { // ProbingHashTable: tombstones must not break lookups of keys further along the probe chain
        ProbingHashTable<int, int> h(16);
        // fill enough to create clusters, then delete every other key
        for (int i = 0; i < 200; i++) h.put(i, i);
        for (int i = 0; i < 200; i += 2) h.remove(i);
        bool ok = true;
        for (int i = 0; i < 200; i++) { bool want = (i % 2 == 1); if (h.contains(i) != want) ok = false; }
        CHECK(ok && h.size() == 100);
        // churn: repeated insert/delete must not fill the table with tombstones forever
        for (int round = 0; round < 50; round++) {
            for (int i = 1000; i < 1100; i++) h.put(i, i);
            for (int i = 1000; i < 1100; i++) h.remove(i);
        }
        CHECK(h.size() == 100 && h.capacity() <= 1024);
        ok = true; for (int i = 1; i < 200; i += 2) if (!h.get(i) || *h.get(i) != i) ok = false;
        CHECK(ok);
    }
    { // ProbingHashTable: strings + for_each + clear
        ProbingHashTable<std::string, int> h;
        h.put("AMB-01", 1); h.put("FIRE-07", 2); h.put("BOAT-3", 3);
        CHECK(*h.get("AMB-01") == 1 && *h.get("FIRE-07") == 2 && !h.contains("X"));
        h.remove("FIRE-07");
        int n = 0, sum = 0; h.for_each([&](const std::string&, int& v) { n++; sum += v; });
        CHECK(n == 2 && sum == 4);
        h.clear(); CHECK(h.size() == 0 && !h.contains("AMB-01"));
        h.put("AMB-01", 9); CHECK(*h.get("AMB-01") == 9);
    }
    { // Probe counter: a successful lookup at load <= 0.5 should average well under 3 probes
        ProbingHashTable<int, int> h;
        for (int i = 0; i < 10000; i++) h.put(i * 7919, i);
        h.resetProbeCount();
        for (int i = 0; i < 10000; i++) h.get(i * 7919);
        double avg = (double)h.probeCount() / 10000;
        CHECK(avg >= 1.0 && avg < 3.0);
    }

    // Sorting
    checkIntSort([](DynamicArray<int>& v) { insertionSort(v, IntLess()); }, "insertionSort");
    checkIntSort([](DynamicArray<int>& v) { mergeSort(v, IntLess()); }, "mergeSort");
    checkIntSort([](DynamicArray<int>& v) { quickSort(v, IntLess()); }, "quickSort");
    checkIntSort([](DynamicArray<int>& v) { heapSort(v, IntLess()); }, "heapSort");

    { // Incidents by priority: every sort must agree with IncidentHigherPriority
        DynamicArray<Incident> base;
        for (int i = 0; i < 500; i++) base.push_back(mk(i, 1 + rnd(10), rnd(5000)));
        DynamicArray<Incident> a = base, b = base, c = base;
        mergeSort(a, IncidentHigherPriority());
        quickSort(b, IncidentHigherPriority());
        heapSort(c, IncidentHigherPriority());
        CHECK(isSorted(a, IncidentHigherPriority()));
        CHECK(isSorted(b, IncidentHigherPriority()));
        CHECK(isSorted(c, IncidentHigherPriority()));
        CHECK(a[0].severity == 10 && a[499].severity == 1);
    }
    { // Stability: merge and counting sort keep equal keys in input order
        DynamicArray<Incident> v;
        for (int i = 0; i < 300; i++) v.push_back(mk(i, 1 + rnd(10), 0));   // id = input order
        DynamicArray<Incident> m = v, cs = v, csd = v;
        auto bySev = [](const Incident& x, const Incident& y) { return x.severity < y.severity; };
        mergeSort(m, bySev);
        countingSort(cs, [](const Incident& x) { return x.severity; }, 1, 10);
        countingSort(csd, [](const Incident& x) { return x.severity; }, 1, 10, true);
        bool stable = true, same = true, desc = true;
        for (int i = 1; i < 300; i++) {
            if (m[i].severity == m[i - 1].severity && m[i].id < m[i - 1].id) stable = false;
            if (cs[i].id != m[i].id) same = false;
            if (csd[i].severity > csd[i - 1].severity) desc = false;
            if (csd[i].severity == csd[i - 1].severity && csd[i].id < csd[i - 1].id) desc = false;
        }
        CHECK(stable && same && desc);
    }
    { // countingSort: out-of-range keys go last, small inputs
        DynamicArray<int> v; int vals[] = {3, 99, 1, -5, 2, 3};
        for (int x : vals) v.push_back(x);
        countingSort(v, [](int x) { return x; }, 1, 10);
        CHECK(v[0] == 1 && v[1] == 2 && v[2] == 3 && v[3] == 3 && v[4] == 99 && v[5] == -5);
        DynamicArray<int> e; countingSort(e, [](int x) { return x; }, 1, 10); CHECK(e.size() == 0);
    }

    // Searching
    { // linear search on unsorted data
        DynamicArray<Incident> v;
        v.push_back(mk(10, 3, 0)); v.push_back(mk(4, 9, 0)); v.push_back(mk(7, 9, 0));
        CHECK(linearSearch(v, [](const Incident& x) { return x.severity == 9; }) == 1);
        CHECK(linearSearch(v, [](const Incident& x) { return x.id == 99; }) == -1);
        DynamicArray<int> all = linearSearchAll(v, [](const Incident& x) { return x.severity == 9; });
        CHECK(all.size() == 2 && all[0] == 1 && all[1] == 2);
    }
    { // binary search, bounds and range on sorted ids
        DynamicArray<Incident> v;
        for (int i = 0; i < 100; i++) v.push_back(mk(i * 2, 1, i * 10));   // ids 0,2,...,198
        auto id = [](const Incident& x) { return x.id; };
        CHECK(binarySearch(v, id, 0) == 0 && binarySearch(v, id, 198) == 99 && binarySearch(v, id, 50) == 25);
        CHECK(binarySearch(v, id, 51) == -1 && binarySearch(v, id, -1) == -1 && binarySearch(v, id, 500) == -1);
        CHECK(lowerBound(v, id, 51) == 26 && upperBound(v, id, 50) == 26 && lowerBound(v, id, 999) == 100);
        auto t = [](const Incident& x) { return x.reportedAt; };
        int first = -1;
        CHECK(rangeSearch(v, t, 95LL, 300LL, &first) == 21 && first == 10);   // times 100..300
        CHECK(rangeSearch(v, t, 2000LL, 3000LL) == 0);
        DynamicArray<Incident> empty;
        CHECK(binarySearch(empty, id, 1) == -1 && lowerBound(empty, id, 1) == 0);
    }
    { // duplicates: lowerBound/upperBound bracket the run
        DynamicArray<int> v; int vals[] = {1, 2, 2, 2, 5, 8};
        for (int x : vals) v.push_back(x);
        auto k = [](int x) { return x; };
        CHECK(lowerBound(v, k, 2) == 1 && upperBound(v, k, 2) == 4 && rangeSearch(v, k, 2, 2) == 3);
        int i = binarySearch(v, k, 2); CHECK(i >= 1 && i <= 3);
    }
    { // ProbingHashTable and HashTable give the same answers on the same workload
        HashTable<int, int> a; ProbingHashTable<int, int> b;
        bool same = true;
        for (int i = 0; i < 5000; i++) {
            int k = rnd(3000), op = rnd(3);
            if (op == 0) { a.put(k, i); b.put(k, i); }
            else if (op == 1) { if (a.remove(k) != b.remove(k)) same = false; }
            else { int* x = a.get(k); int* y = b.get(k); if ((!x) != (!y) || (x && *x != *y)) same = false; }
        }
        CHECK(same && a.size() == b.size());
    }
    TEST_END();
}
