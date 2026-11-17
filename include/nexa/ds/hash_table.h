#pragma once
#include <cstddef>
#include <string>

namespace nexa {

template <typename K> struct Hash;  // undefined for unsupported key types

template <> struct Hash<int> {
    size_t operator()(int k) const {
        size_t x = static_cast<size_t>(k);
        x *= 11400714819323198485ull;   // Fibonacci hashing
        return x ^ (x >> 32);
    }
};
template <> struct Hash<std::string> {
    size_t operator()(const std::string& s) const {   // FNV-1a
        size_t h = 1469598103934665603ull;
        for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
        return h;
    }
};

// Hash table with separate chaining. Search/Insert/Remove: O(1) avg, O(n) worst.
// Rehashes when load factor exceeds 0.75.
template <typename K, typename V, typename H = Hash<K>>
class HashTable {
    struct Node {
        K key; V val; Node* next;
        Node(const K& k, const V& v, Node* n) : key(k), val(v), next(n) {}
    };
    Node** b_;
    size_t cap_, size_;
    H hash_;

    void rehash(size_t nc) {
        Node** nb = new Node*[nc]();
        for (size_t i = 0; i < cap_; i++) {
            Node* n = b_[i];
            while (n) {
                Node* nx = n->next;
                size_t j = hash_(n->key) % nc;
                n->next = nb[j]; nb[j] = n;
                n = nx;
            }
        }
        delete[] b_;
        b_ = nb; cap_ = nc;
    }
public:
    explicit HashTable(size_t cap = 17) : b_(new Node*[cap]()), cap_(cap), size_(0) {}
    ~HashTable() { clear(); delete[] b_; }
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    void put(const K& k, const V& v) {          // insert or update
        size_t i = hash_(k) % cap_;
        for (Node* n = b_[i]; n; n = n->next)
            if (n->key == k) { n->val = v; return; }
        b_[i] = new Node(k, v, b_[i]);
        size_++;
        if (size_ * 4 > cap_ * 3) rehash(cap_ * 2 + 1);
    }
    V* get(const K& k) {
        size_t i = hash_(k) % cap_;
        for (Node* n = b_[i]; n; n = n->next) if (n->key == k) return &n->val;
        return nullptr;
    }
    const V* get(const K& k) const { return const_cast<HashTable*>(this)->get(k); }
    bool contains(const K& k) const { return get(k) != nullptr; }
    bool remove(const K& k) {
        size_t i = hash_(k) % cap_;
        Node* prev = nullptr;
        for (Node* n = b_[i]; n; prev = n, n = n->next) {
            if (n->key == k) {
                if (prev) prev->next = n->next; else b_[i] = n->next;
                delete n; size_--;
                return true;
            }
        }
        return false;
    }
    void clear() {
        for (size_t i = 0; i < cap_; i++) {
            Node* n = b_[i];
            while (n) { Node* nx = n->next; delete n; n = nx; }
            b_[i] = nullptr;
        }
        size_ = 0;
    }
    template <typename F> void for_each(F f) {
        for (size_t i = 0; i < cap_; i++)
            for (Node* n = b_[i]; n; n = n->next) f(n->key, n->val);
    }
    size_t size() const { return size_; }
    size_t bucketCount() const { return cap_; }
    double loadFactor() const { return static_cast<double>(size_) / cap_; }
};

}  // namespace nexa
