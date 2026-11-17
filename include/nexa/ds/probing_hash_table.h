#pragma once
#include <cstddef>
#include <utility>
#include "hash_table.h"   // reuses Hash<int> / Hash<std::string>

namespace nexa {

// Hash table with open addressing and linear probing (no chains, one flat array).
// Deleted slots become tombstones so later probe sequences are not cut short.
// Capacity is a power of two, so the slot index is hash & (cap - 1).
// Rehashes when (live + tombstones) exceeds 0.5 of capacity, which keeps probe
// sequences short. Search/Insert/Remove: O(1) avg, O(n) worst.
template <typename K, typename V, typename H = Hash<K>>
class ProbingHashTable {
    enum State : unsigned char { EMPTY, FULL, DELETED };
    struct Slot { K key; V val; State st = EMPTY; };

    Slot* s_;
    size_t cap_, size_, tomb_;
    H hash_;
    mutable size_t probes_ = 0;   // total slots inspected, for the analysis report

    size_t home(const K& k) const { return hash_(k) & (cap_ - 1); }

    // Returns the slot holding k, or cap_ if not present.
    size_t findSlot(const K& k) const {
        size_t i = home(k);
        for (size_t step = 0; step < cap_; step++, i = (i + 1) & (cap_ - 1)) {
            probes_++;
            if (s_[i].st == EMPTY) return cap_;
            if (s_[i].st == FULL && s_[i].key == k) return i;
        }
        return cap_;
    }

    void rehash(size_t nc) {
        Slot* old = s_;
        size_t oc = cap_;
        s_ = new Slot[nc];
        cap_ = nc; size_ = 0; tomb_ = 0;
        for (size_t i = 0; i < oc; i++)
            if (old[i].st == FULL) insertFresh(std::move(old[i].key), std::move(old[i].val));
        delete[] old;
    }
    // Insert a key known not to be present, into a table with free space.
    void insertFresh(K k, V v) {
        size_t i = home(k);
        while (s_[i].st == FULL) i = (i + 1) & (cap_ - 1);
        if (s_[i].st == DELETED) tomb_--;
        s_[i].key = std::move(k); s_[i].val = std::move(v); s_[i].st = FULL;
        size_++;
    }
    static size_t roundUpPow2(size_t n) { size_t p = 8; while (p < n) p <<= 1; return p; }

public:
    explicit ProbingHashTable(size_t cap = 16)
        : s_(new Slot[roundUpPow2(cap)]), cap_(roundUpPow2(cap)), size_(0), tomb_(0) {}
    ~ProbingHashTable() { delete[] s_; }
    ProbingHashTable(const ProbingHashTable&) = delete;
    ProbingHashTable& operator=(const ProbingHashTable&) = delete;

    void put(const K& k, const V& v) {          // insert or update
        size_t i = findSlot(k);
        if (i != cap_) { s_[i].val = v; return; }
        if ((size_ + tomb_ + 1) * 2 > cap_)
            rehash(size_ + 1 > cap_ / 4 ? cap_ * 2 : cap_);   // mostly tombstones: clean in place
        insertFresh(k, v);
    }
    V* get(const K& k) {
        size_t i = findSlot(k);
        return i == cap_ ? nullptr : &s_[i].val;
    }
    const V* get(const K& k) const { return const_cast<ProbingHashTable*>(this)->get(k); }
    bool contains(const K& k) const { return get(k) != nullptr; }
    bool remove(const K& k) {
        size_t i = findSlot(k);
        if (i == cap_) return false;
        s_[i].st = DELETED;
        s_[i].key = K(); s_[i].val = V();   // release strings etc.
        size_--; tomb_++;
        return true;
    }
    void clear() {
        for (size_t i = 0; i < cap_; i++) { s_[i] = Slot(); }
        size_ = 0; tomb_ = 0;
    }
    template <typename F> void for_each(F f) {
        for (size_t i = 0; i < cap_; i++)
            if (s_[i].st == FULL) f(s_[i].key, s_[i].val);
    }
    size_t size() const { return size_; }
    size_t capacity() const { return cap_; }
    size_t tombstones() const { return tomb_; }
    double loadFactor() const { return static_cast<double>(size_) / cap_; }
    size_t probeCount() const { return probes_; }
    void resetProbeCount() const { probes_ = 0; }
};

}  // namespace nexa
