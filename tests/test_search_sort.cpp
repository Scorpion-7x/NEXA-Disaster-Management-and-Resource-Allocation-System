// Tests for include/nexa/ds/sort.h and include/nexa/ds/search.h.
// The STL is used here ONLY as a reference oracle to check our results.
#include "nexa/ds/sort.h"
#include "nexa/ds/search.h"

#include <algorithm>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

static int failures = 0, checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
    std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

struct Rec {             // stand-in for Incident: key fields + original position
    int severity = 0;
    long long reportedAt = 0;
    double distance = 0;
    int id = 0;
};

static std::mt19937 rng(2026);

static std::vector<Rec> makeRecs(int n, int sevMax = 10) {
    std::vector<Rec> v(n);
    std::uniform_int_distribution<int> sev(1, sevMax);
    std::uniform_int_distribution<long long> t(-5000000000LL, 5000000000LL);
    std::uniform_real_distribution<double> d(0.0, 50.0);
    for (int i = 0; i < n; i++) v[i] = {sev(rng), t(rng), d(rng), i};
    return v;
}

// The shapes that break naive sorts
static std::vector<std::vector<int>> intCases() {
    std::vector<std::vector<int>> cs;
    cs.push_back({});
    cs.push_back({7});
    cs.push_back({2, 1});
    cs.push_back(std::vector<int>(100, 5));                    // all equal
    std::vector<int> asc(1000), desc(1000), organ(1000);
    for (int i = 0; i < 1000; i++) { asc[i] = i; desc[i] = 1000 - i; organ[i] = i < 500 ? i : 1000 - i; }
    cs.push_back(asc); cs.push_back(desc); cs.push_back(organ);
    for (int n : {17, 33, 257, 5000}) {                        // just above cutoffs + big
        std::vector<int> r(n);
        std::uniform_int_distribution<int> u(-1000, 1000);
        for (int& x : r) x = u(rng);
        cs.push_back(r);
        std::vector<int> fewDistinct(n);
        std::uniform_int_distribution<int> f(0, 3);
        for (int& x : fewDistinct) x = f(rng);
        cs.push_back(fewDistinct);
    }
    return cs;
}

static void testComparisonSorts() {
    auto less = [](int a, int b) { return a < b; };
    for (auto c : intCases()) {
        auto want = c; std::sort(want.begin(), want.end());
        auto m = c; nexa::mergeSort(m.data(), (int)m.size(), less);     CHECK(m == want);
        auto q = c; nexa::quickSort(q.data(), (int)q.size(), less);     CHECK(q == want);
        if (c.size() <= 1000) {
            auto s = c; nexa::insertionSort(s.data(), (int)s.size(), less); CHECK(s == want);
        }
    }
    // descending via comparator, on strings
    std::vector<std::string> w = {"flood", "fire", "quake", "cyclone", "fire", "landslide"};
    auto want = w; std::sort(want.begin(), want.end(), std::greater<std::string>());
    nexa::quickSort(w.data(), (int)w.size(), [](const std::string& a, const std::string& b) { return a > b; });
    CHECK(w == want);
}

static bool sameOrder(const std::vector<Rec>& a, const std::vector<Rec>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) if (a[i].id != b[i].id) return false;
    return true;
}

static void testStableSorts() {
    for (int n : {0, 1, 2, 50, 1000, 20000}) {
        auto base = makeRecs(n);

        // merge sort stability: compare to std::stable_sort
        auto want = base;
        std::stable_sort(want.begin(), want.end(), [](const Rec& a, const Rec& b) { return a.severity < b.severity; });
        auto m = base;
        nexa::mergeSort(m.data(), n, [](const Rec& a, const Rec& b) { return a.severity < b.severity; });
        CHECK(sameOrder(m, want));

        // counting sort, ascending and descending by severity
        auto c = base;
        nexa::countingSort(c.data(), n, [](const Rec& r) { return r.severity; }, 1, 10);
        CHECK(sameOrder(c, want));
        auto wantDesc = base;
        std::stable_sort(wantDesc.begin(), wantDesc.end(), [](const Rec& a, const Rec& b) { return a.severity > b.severity; });
        auto cd = base;
        nexa::countingSort(cd.data(), n, [](const Rec& r) { return r.severity; }, 1, 10, true);
        CHECK(sameOrder(cd, wantDesc));

        // radix sort on signed 64-bit timestamps
        auto wantT = base;
        std::stable_sort(wantT.begin(), wantT.end(), [](const Rec& a, const Rec& b) { return a.reportedAt < b.reportedAt; });
        auto r = base;
        nexa::radixSort(r.data(), n, [](const Rec& x) { return x.reportedAt; });
        CHECK(sameOrder(r, wantT));

        // radix sort with heavy duplicates (stability matters)
        auto wantS = base;
        std::stable_sort(wantS.begin(), wantS.end(), [](const Rec& a, const Rec& b) { return a.severity < b.severity; });
        auto rs = base;
        nexa::radixSort(rs.data(), n, [](const Rec& x) { return (long long)x.severity; });
        CHECK(sameOrder(rs, wantS));

        // bucket sort on distances
        auto wantD = base;
        std::stable_sort(wantD.begin(), wantD.end(), [](const Rec& a, const Rec& b) { return a.distance < b.distance; });
        auto b = base;
        nexa::bucketSort(b.data(), n, [](const Rec& x) { return x.distance; });
        CHECK(sameOrder(b, wantD));
    }

    // bucket sort: skewed keys, all-equal keys, negatives
    std::vector<double> sk = {1e-9, 1e9, 3, 3, 2, -7.5, 0, 1e9, 4.25};
    auto want = sk; std::sort(want.begin(), want.end());
    nexa::bucketSort(sk.data(), (int)sk.size(), [](double x) { return x; });
    CHECK(sk == want);
    std::vector<double> eq(64, 2.5);
    nexa::bucketSort(eq.data(), 64, [](double x) { return x; });
    CHECK(eq == std::vector<double>(64, 2.5));

    // radix sort: extremes of long long
    std::vector<long long> ex = {0, -1, 1, (long long)9e18, -(long long)9e18, 42, -42};
    auto wex = ex; std::sort(wex.begin(), wex.end());
    nexa::radixSort(ex.data(), (int)ex.size(), [](long long x) { return x; });
    CHECK(ex == wex);

    // counting sort rejects out-of-range keys and leaves the array intact
    std::vector<int> bad = {3, 1, 99, 2};
    bool threw = false;
    try { nexa::countingSort(bad.data(), 4, [](int x) { return x; }, 1, 10); }
    catch (const std::out_of_range&) { threw = true; }
    CHECK(threw);
    CHECK((bad == std::vector<int>{3, 1, 99, 2}));
}

static void testLinearAndBinary() {
    auto recs = makeRecs(200);
    int i = nexa::linearSearch(recs.data(), 200, [](const Rec& r) { return r.severity == 10; });
    int want = -1;
    for (int j = 0; j < 200; j++) if (recs[j].severity == 10) { want = j; break; }
    CHECK(i == want);
    CHECK(nexa::linearSearch(recs.data(), 200, [](const Rec& r) { return r.severity > 10; }) == -1);
    CHECK(nexa::linearSearch(recs.data(), 0, [](const Rec&) { return true; }) == -1);

    std::vector<int> idx(200);
    int m = nexa::linearFilter(recs.data(), 200, [](const Rec& r) { return r.severity >= 8; }, idx.data());
    int cnt = 0;
    for (auto& r : recs) if (r.severity >= 8) cnt++;
    CHECK(m == cnt);
    bool ok = true;
    for (int k = 0; k < m; k++) ok = ok && recs[idx[k]].severity >= 8 && (k == 0 || idx[k] > idx[k - 1]);
    CHECK(ok);

    // binary search with duplicates, absent keys, ends
    std::vector<int> a = {1, 3, 3, 3, 5, 8, 8, 13};
    auto id = [](int x) { return x; };
    int n = (int)a.size();
    for (int k = 0; k <= 14; k++) {
        CHECK(nexa::lowerBound(a.data(), n, k, id) == (int)(std::lower_bound(a.begin(), a.end(), k) - a.begin()));
        CHECK(nexa::upperBound(a.data(), n, k, id) == (int)(std::upper_bound(a.begin(), a.end(), k) - a.begin()));
        int b = nexa::binarySearch(a.data(), n, k, id);
        bool present = std::binary_search(a.begin(), a.end(), k);
        CHECK(present ? (b >= 0 && a[b] == k && (b == 0 || a[b - 1] < k)) : b == -1);
    }
    CHECK(nexa::binarySearch(a.data(), 0, 3, id) == -1);

    // binary search over records sorted by a key: "first incident with severity >= 7"
    nexa::countingSort(recs.data(), 200, [](const Rec& r) { return r.severity; }, 1, 10);
    int first7 = nexa::lowerBound(recs.data(), 200, 7, [](const Rec& r) { return r.severity; });
    CHECK(first7 == 200 || recs[first7].severity >= 7);
    CHECK(first7 == 0 || recs[first7 - 1].severity < 7);
}

static void testSelect() {
    auto less = [](int a, int b) { return a < b; };
    for (auto c : intCases()) {
        if (c.empty()) continue;
        auto sorted = c; std::sort(sorted.begin(), sorted.end());
        int n = (int)c.size();
        std::vector<int> ks = {0, n - 1, (n - 1) / 2, n / 3};
        for (int k : ks) {
            auto w = c;
            int got = nexa::selectKth(w.data(), n, k, less);
            CHECK(got == sorted[k]);
            bool part = true;                                 // partition property
            for (int j = 0; j < k; j++) part = part && w[j] <= got;
            for (int j = k + 1; j < n; j++) part = part && w[j] >= got;
            CHECK(part);
            auto ws = w; std::sort(ws.begin(), ws.end());
            CHECK(ws == sorted);                              // still a permutation
        }
        auto before = c;
        CHECK(nexa::median(c.data(), n, less) == sorted[(n - 1) / 2]);
        CHECK(c == before);                                   // median() leaves input alone
    }
    // every k on a small random array
    std::vector<int> v(97);
    std::uniform_int_distribution<int> u(0, 30);
    for (int& x : v) x = u(rng);
    auto s = v; std::sort(s.begin(), s.end());
    bool all = true;
    for (int k = 0; k < 97; k++) { auto w = v; all = all && nexa::selectKth(w.data(), 97, k, less) == s[k]; }
    CHECK(all);

    // median distance of records
    auto recs = makeRecs(501);
    std::vector<double> d; for (auto& r : recs) d.push_back(r.distance);
    std::sort(d.begin(), d.end());
    Rec med = nexa::median(recs.data(), 501, [](const Rec& a, const Rec& b) { return a.distance < b.distance; });
    CHECK(med.distance == d[250]);

    bool threw = false;
    try { nexa::selectKth(v.data(), 97, 97, less); } catch (const std::out_of_range&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { nexa::median(v.data(), 0, less); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}

int main() {
    testComparisonSorts();
    testStableSorts();
    testLinearAndBinary();
    testSelect();
    if (failures) { std::printf("%d of %d checks FAILED\n", failures, checks); return 1; }
    std::printf("test_search_sort: %d checks, ALL PASSED\n", checks);
    return 0;
}
