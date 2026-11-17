#include <string>
#include "test_util.h"
#include "nexa/ds/dynamic_array.h"
#include "nexa/ds/stack.h"
#include "nexa/ds/queue.h"
#include "nexa/ds/linked_list.h"
#include "nexa/ds/hash_table.h"
#include "nexa/ds/priority_queue.h"
using namespace nexa;

struct MaxCmp { bool operator()(int a, int b) const { return a > b; } };

int main() {
    { // DynamicArray
        DynamicArray<int> a;
        for (int i = 0; i < 100; i++) a.push_back(i);
        CHECK(a.size() == 100 && a[99] == 99);
        DynamicArray<int> b = a;            // copy
        b[0] = -1;
        CHECK(a[0] == 0 && b[0] == -1);
        DynamicArray<int> c = std::move(b); // move
        CHECK(c.size() == 100 && b.size() == 0);
        b.push_back(5);                     // moved-from still usable
        CHECK(b.size() == 1 && b[0] == 5);
        CHECK(a.pop_back() == 99 && a.size() == 99);
        bool threw = false; try { a[500]; } catch (const std::out_of_range&) { threw = true; }
        CHECK(threw);
        int sum = 0; for (int x : c) sum += x; CHECK(sum == -1 + (99 * 100 / 2));
    }
    { // Stack
        Stack<int> s;
        CHECK(s.empty());
        s.push(1); s.push(2); s.push(3);
        CHECK(s.peek() == 3 && s.pop() == 3 && s.pop() == 2 && s.size() == 1);
        s.pop();
        bool threw = false; try { s.pop(); } catch (const std::out_of_range&) { threw = true; }
        CHECK(threw);
    }
    { // Queue (forces wrap-around and growth)
        Queue<int> q;
        for (int i = 0; i < 6; i++) q.enqueue(i);
        for (int i = 0; i < 4; i++) CHECK(q.dequeue() == i);
        for (int i = 6; i < 30; i++) q.enqueue(i);   // wraps, then grows
        for (int i = 4; i < 30; i++) CHECK(q.dequeue() == i);
        CHECK(q.empty());
    }
    { // LinkedList
        LinkedList<int> l;
        l.push_back(2); l.push_back(3); l.push_front(1);
        CHECK(l.size() == 3 && l.head()->val == 1);
        CHECK(l.find([](int v) { return v == 3; }) != nullptr);
        CHECK(l.find([](int v) { return v == 9; }) == nullptr);
        CHECK(l.remove_first([](int v) { return v == 3; }));   // remove tail
        l.push_back(4);                                         // tail still valid
        CHECK(l.head()->next->next->val == 4);
        CHECK(l.pop_front() == 1 && l.pop_front() == 2 && l.pop_front() == 4 && l.empty());
        l.push_back(7); CHECK(l.head()->val == 7);              // reuse after emptying
    }
    { // HashTable: ints + rehash
        HashTable<int, int> h;
        for (int i = 0; i < 2000; i++) h.put(i, i * 2);
        CHECK(h.size() == 2000 && h.loadFactor() <= 0.75);
        bool ok = true; for (int i = 0; i < 2000; i++) { int* v = h.get(i); if (!v || *v != i * 2) ok = false; }
        CHECK(ok);
        h.put(5, 999); CHECK(*h.get(5) == 999 && h.size() == 2000);   // update, not duplicate
        CHECK(h.remove(5) && !h.contains(5) && !h.remove(5) && h.size() == 1999);
        CHECK(h.get(-3) == nullptr);
    }
    { // HashTable: strings
        HashTable<std::string, int> h;
        h.put("AMB-01", 1); h.put("FIRE-07", 2);
        CHECK(*h.get("AMB-01") == 1 && *h.get("FIRE-07") == 2 && !h.contains("X"));
        int n = 0; h.for_each([&](const std::string&, int&) { n++; }); CHECK(n == 2);
    }
    { // PriorityQueue
        PriorityQueue<int, MaxCmp> pq;
        int vals[] = {5, 1, 9, 3, 7, 9, 2, 8, 6, 4, 0};
        for (int v : vals) pq.push(v);
        CHECK(pq.top() == 9);
        int prev = 1000; bool sorted = true;
        while (!pq.empty()) { int v = pq.pop(); if (v > prev) sorted = false; prev = v; }
        CHECK(sorted);
        PriorityQueue<int, MaxCmp> big;
        for (int i = 0; i < 500; i++) big.push((i * 37) % 101);   // forces growth
        CHECK(big.size() == 500 && big.pop() == 100);
    }
    TEST_END();
}
