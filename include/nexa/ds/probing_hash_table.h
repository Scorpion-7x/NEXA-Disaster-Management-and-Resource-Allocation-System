#pragma once
// Open-addressing hash table with linear probing.
// Companion to the chaining HashTable: same job, different collision strategy,
// so the two can be compared in the benchmark phase.
//
//   insert / find / remove : O(1) average, O(n) worst case
//   load factor kept <= 0.5 (counting tombstones), capacity is a power of two
//
// Deleted slots become tombstones so that probe chains stay intact.
// A rehash (triggered on insert) throws the tombstones away.

#include <stdexcept>
#include <string>

namespace nexa {

struct DefaultHash {
    static unsigned long long mix(unsigned long long x) {   // splitmix64 finalizer
        x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27; x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return x;
    }
    unsigned long long operator()(int k) const { return mix((unsigned long long)(long long)k); }
    unsigned long long operator()(long long k) const { return mix((unsigned long long)k); }
    unsigned long long operator()(unsigned k) const { return mix(k); }
    unsigned long long operator()(const std::string& s) const {   // FNV-1a
        unsigned long long h = 1469598103934665603ULL;
        for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
        return mix(h);
    }
};

template <typename K, typename V, typename H = DefaultHash>
class ProbingHashTable {
    enum : unsigned char { EMPTY = 0, FULL = 1, DELETED = 2 };

    K* keys_;
    V* vals_;
    unsigned char* state_;
    int cap_;      // always a power of two
    int n_;        // FULL slots
    int used_;     // FULL + DELETED slots
    H hash_;

    int home(const K& k) const { return (int)(hash_(k) & (unsigned long long)(cap_ - 1)); }

    void allocate(int cap) {
        cap_ = cap;
        keys_ = new K[cap];
        vals_ = new V[cap];
        state_ = new unsigned char[cap];
        for (int i = 0; i < cap; i++) state_[i] = EMPTY;
        n_ = used_ = 0;
    }
    void release() {
        delete[] keys_; delete[] vals_; delete[] state_;
        keys_ = nullptr; vals_ = nullptr; state_ = nullptr;
        cap_ = n_ = used_ = 0;
    }

    void rehash(int newCap) {
        K* ok = keys_; V* ov = vals_; unsigned char* os = state_; int oc = cap_;
        allocate(newCap);
        for (int i = 0; i < oc; i++) {
            if (os[i] != FULL) continue;
            int j = home(ok[i]);
            while (state_[j] == FULL) j = (j + 1) & (cap_ - 1);
            keys_[j] = ok[i]; vals_[j] = ov[i]; state_[j] = FULL;
            n_++; used_++;
        }
        delete[] ok; delete[] ov; delete[] os;
    }

    // index of k, or -1
    int locate(const K& k) const {
        if (cap_ == 0) return -1;
        int i = home(k);
        for (int step = 0; step < cap_; step++) {
            if (state_[i] == EMPTY) return -1;
            if (state_[i] == FULL && keys_[i] == k) return i;
            i = (i + 1) & (cap_ - 1);
        }
        return -1;
    }

public:
    explicit ProbingHashTable(int initialCap = 16) {
        int c = 8;
        while (c < initialCap) c <<= 1;
        allocate(c);
    }
    ~ProbingHashTable() { release(); }

    ProbingHashTable(const ProbingHashTable&) = delete;
    ProbingHashTable& operator=(const ProbingHashTable&) = delete;

    ProbingHashTable(ProbingHashTable&& o) noexcept
        : keys_(o.keys_), vals_(o.vals_), state_(o.state_),
          cap_(o.cap_), n_(o.n_), used_(o.used_), hash_(o.hash_) {
        o.keys_ = nullptr; o.vals_ = nullptr; o.state_ = nullptr;
        o.cap_ = o.n_ = o.used_ = 0;
    }
    ProbingHashTable& operator=(ProbingHashTable&& o) noexcept {
        if (this != &o) {
            release();
            keys_ = o.keys_; vals_ = o.vals_; state_ = o.state_;
            cap_ = o.cap_; n_ = o.n_; used_ = o.used_; hash_ = o.hash_;
            o.keys_ = nullptr; o.vals_ = nullptr; o.state_ = nullptr;
            o.cap_ = o.n_ = o.used_ = 0;
        }
        return *this;
    }

    // Returns true if k was new, false if an existing value was overwritten.
    bool insert(const K& k, const V& v) {
        if (cap_ == 0) allocate(8);                 // reuse after being moved from
        int existing = locate(k);
        if (existing >= 0) { vals_[existing] = v; return false; }

        if ((used_ + 1) * 2 > cap_) {
            // grow if live keys need it, otherwise same size just clears tombstones
            rehash((n_ + 1) * 4 > cap_ ? cap_ * 2 : cap_);
        }
        int i = home(k);
        while (state_[i] == FULL) i = (i + 1) & (cap_ - 1);
        if (state_[i] == EMPTY) used_++;            // reusing a tombstone keeps used_ the same
        keys_[i] = k; vals_[i] = v; state_[i] = FULL;
        n_++;
        return true;
    }

    V* find(const K& k) { int i = locate(k); return i < 0 ? nullptr : &vals_[i]; }
    const V* find(const K& k) const { int i = locate(k); return i < 0 ? nullptr : &vals_[i]; }
    bool contains(const K& k) const { return locate(k) >= 0; }

    V& get(const K& k) {
        int i = locate(k);
        if (i < 0) throw std::out_of_range("ProbingHashTable::get: key not found");
        return vals_[i];
    }

    bool remove(const K& k) {
        int i = locate(k);
        if (i < 0) return false;
        state_[i] = DELETED;
        keys_[i] = K(); vals_[i] = V();             // drop any heap memory the slot held
        n_--;
        return true;
    }

    void clear() { release(); allocate(8); }

    // fn(const K&, V&) for every entry, in slot order
    template <typename Fn>
    void forEach(Fn fn) {
        for (int i = 0; i < cap_; i++) if (state_[i] == FULL) fn(keys_[i], vals_[i]);
    }

    int size() const { return n_; }
    bool empty() const { return n_ == 0; }
    int capacity() const { return cap_; }
    int tombstones() const { return used_ - n_; }
    double loadFactor() const { return cap_ ? (double)used_ / cap_ : 0.0; }
};

} // namespace nexa
