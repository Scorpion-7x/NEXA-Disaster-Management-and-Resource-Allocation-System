// Tests for include/nexa/ds/probing_hash_table.h.
// std::unordered_map is used ONLY as a reference oracle.
#include "nexa/ds/probing_hash_table.h"

#include <cstdio>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>

static int failures = 0, checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
    std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void testBasics() {
    nexa::ProbingHashTable<int, std::string> t;
    CHECK(t.empty());
    CHECK(t.find(1) == nullptr);
    CHECK(!t.remove(1));
    CHECK(t.insert(101, "Ambulance A1"));
    CHECK(t.insert(102, "Fire Truck F3"));
    CHECK(!t.insert(101, "Ambulance A1 (updated)"));         // overwrite returns false
    CHECK(t.size() == 2);
    CHECK(*t.find(101) == "Ambulance A1 (updated)");
    CHECK(t.get(102) == "Fire Truck F3");
    CHECK(t.contains(102) && !t.contains(103));
    bool threw = false;
    try { t.get(999); } catch (const std::out_of_range&) { threw = true; }
    CHECK(threw);
    CHECK(t.remove(101));
    CHECK(!t.contains(101) && t.size() == 1);
    CHECK(t.tombstones() == 1);
    CHECK(t.insert(101, "back"));                             // reinsert after delete
    CHECK(t.get(101) == "back" && t.size() == 2);
}

// Keys that all share a home slot: probe chains through tombstones must still work.
struct ZeroHash { unsigned long long operator()(int) const { return 0; } };

static void testCollisionsAndTombstones() {
    nexa::ProbingHashTable<int, int, ZeroHash> t;
    for (int i = 0; i < 40; i++) t.insert(i, i * 10);
    bool ok = true;
    for (int i = 0; i < 40; i++) ok = ok && t.find(i) && *t.find(i) == i * 10;
    CHECK(ok);
    for (int i = 0; i < 40; i += 2) CHECK(t.remove(i));       // punch holes in the chain
    ok = true;
    for (int i = 1; i < 40; i += 2) ok = ok && t.find(i) && *t.find(i) == i * 10;   // still reachable past tombstones
    for (int i = 0; i < 40; i += 2) ok = ok && !t.contains(i);
    CHECK(ok);
    CHECK(t.size() == 20);
    // inserting an existing key must update, not duplicate, even with a tombstone earlier in the chain
    CHECK(!t.insert(39, -1));
    CHECK(t.get(39) == -1 && t.size() == 20);
    int seen = 0;
    t.forEach([&](const int&, int&) { seen++; });
    CHECK(seen == 20);
}

static void testChurnAgainstOracle() {
    nexa::ProbingHashTable<int, int> t;
    std::unordered_map<int, int> ref;
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> key(0, 3000), op(0, 9);
    for (int step = 0; step < 200000; step++) {
        int k = key(rng), o = op(rng);
        if (o < 5) { bool fresh = ref.find(k) == ref.end(); ref[k] = step; CHECK(t.insert(k, step) == fresh); }
        else if (o < 8) { CHECK(t.remove(k) == (ref.erase(k) == 1)); }
        else {
            auto it = ref.find(k);
            const int* v = t.find(k);
            CHECK(it == ref.end() ? v == nullptr : (v && *v == it->second));
        }
    }
    CHECK(t.size() == (int)ref.size());
    CHECK(t.loadFactor() <= 0.5);
    // steady churn must not let tombstones grow the table without bound
    CHECK(t.capacity() <= 16384);
    bool ok = true;
    for (auto& kv : ref) ok = ok && t.find(kv.first) && *t.find(kv.first) == kv.second;
    CHECK(ok);
}

static void testStringKeysAndGrowth() {
    nexa::ProbingHashTable<std::string, int> t(4);
    for (int i = 0; i < 5000; i++) t.insert("RES-" + std::to_string(i), i);
    CHECK(t.size() == 5000);
    CHECK(t.loadFactor() <= 0.5);
    bool ok = true;
    for (int i = 0; i < 5000; i++) { const int* v = t.find("RES-" + std::to_string(i)); ok = ok && v && *v == i; }
    CHECK(ok);
    CHECK(!t.contains("RES-5000") && !t.contains(""));
    t.insert("", -1);                                         // empty string is a valid key
    CHECK(t.get("") == -1);
}

static void testMoveAndClear() {
    nexa::ProbingHashTable<int, int> a;
    for (int i = 0; i < 100; i++) a.insert(i, i);
    nexa::ProbingHashTable<int, int> b(std::move(a));
    CHECK(b.size() == 100 && b.get(42) == 42);
    CHECK(a.size() == 0 && !a.contains(42));
    CHECK(a.insert(7, 70) && a.get(7) == 70);                 // moved-from object is reusable
    nexa::ProbingHashTable<int, int> c;
    c.insert(1, 1);
    c = std::move(b);
    CHECK(c.size() == 100 && !c.contains(-5) && c.get(99) == 99);
    c.clear();
    CHECK(c.empty() && !c.contains(1));
    CHECK(c.insert(1, 2) && c.get(1) == 2);
}

int main() {
    testBasics();
    testCollisionsAndTombstones();
    testChurnAgainstOracle();
    testStringKeysAndGrowth();
    testMoveAndClear();
    if (failures) { std::printf("%d of %d checks FAILED\n", failures, checks); return 1; }
    std::printf("test_probing_hash: %d checks, ALL PASSED\n", checks);
    return 0;
}
