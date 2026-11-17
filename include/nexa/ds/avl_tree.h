#pragma once
#include <utility>

namespace nexa {

template <typename K> struct DefaultLess {
    bool operator()(const K& a, const K& b) const { return a < b; }
};

// Self-balancing binary search tree (AVL), used as an ordered map K -> V.
// Every node keeps |height(left) - height(right)| <= 1, so height stays O(log n).
// put / get / remove / floor / ceiling: O(log n).  rangeQuery: O(log n + m) for m results.
// In-order traversal visits keys in ascending order.
template <typename K, typename V, typename Less = DefaultLess<K>>
class AvlTree {
    struct Node {
        K key; V val;
        Node *left = nullptr, *right = nullptr;
        int h = 1;                          // height of the subtree rooted here (leaf = 1)
        Node(const K& k, const V& v) : key(k), val(v) {}
    };
    Node* root_ = nullptr;
    int size_ = 0;
    long long rotations_ = 0;               // for the analysis report
    Less less_;

    static int h(Node* n) { return n ? n->h : 0; }
    static int balance(Node* n) { return h(n->left) - h(n->right); }
    static void fix(Node* n) { int a = h(n->left), b = h(n->right); n->h = 1 + (a > b ? a : b); }
    bool eq(const K& a, const K& b) const { return !less_(a, b) && !less_(b, a); }

    // Right rotation at y (left-left case); rotLeft is the mirror image.
    //      y            x
    //     / \          / \      .
    //    x   C  -->   A   y
    //   / \              / \    .
    //  A   B            B   C
    Node* rotRight(Node* y) {
        Node* x = y->left;
        y->left = x->right; x->right = y;
        fix(y); fix(x);
        rotations_++;
        return x;
    }
    Node* rotLeft(Node* x) {
        Node* y = x->right;
        x->right = y->left; y->left = x;
        fix(x); fix(y);
        rotations_++;
        return y;
    }
    // Restores the AVL property at n after an insert or delete below it.
    Node* rebalance(Node* n) {
        fix(n);
        int b = balance(n);
        if (b > 1) {                                        // left heavy
            if (balance(n->left) < 0) n->left = rotLeft(n->left);       // left-right case
            return rotRight(n);                                         // left-left case
        }
        if (b < -1) {                                       // right heavy
            if (balance(n->right) > 0) n->right = rotRight(n->right);   // right-left case
            return rotLeft(n);                                          // right-right case
        }
        return n;
    }

    // Inserts k if missing. *slot is set to the node holding k either way.
    Node* insert(Node* n, const K& k, const V& v, Node** slot, bool* added) {
        if (!n) { *added = true; size_++; return *slot = new Node(k, v); }
        if (less_(k, n->key))      n->left = insert(n->left, k, v, slot, added);
        else if (less_(n->key, k)) n->right = insert(n->right, k, v, slot, added);
        else { *slot = n; return n; }
        return rebalance(n);
    }
    Node* erase(Node* n, const K& k, bool* removed) {
        if (!n) return nullptr;
        if (less_(k, n->key))      n->left = erase(n->left, k, removed);
        else if (less_(n->key, k)) n->right = erase(n->right, k, removed);
        else {
            *removed = true;
            if (!n->left || !n->right) {                    // 0 or 1 child: splice out
                Node* c = n->left ? n->left : n->right;
                delete n; size_--;
                return c;
            }
            // 2 children: move in the in-order successor, then delete it from the right subtree
            Node* s = n->right;
            while (s->left) s = s->left;
            n->key = s->key; n->val = std::move(s->val);
            bool dummy = false;
            n->right = erase(n->right, n->key, &dummy);
        }
        return rebalance(n);
    }
    void destroy(Node* n) { if (n) { destroy(n->left); destroy(n->right); delete n; } }

    template <typename F> void inorder(Node* n, F& f) {
        if (!n) return;
        inorder(n->left, f); f(n->key, n->val); inorder(n->right, f);
    }
    template <typename F> void range(Node* n, const K& lo, const K& hi, F& f) {
        if (!n) return;
        if (less_(lo, n->key)) range(n->left, lo, hi, f);          // left subtree may hold keys >= lo
        if (!less_(n->key, lo) && !less_(hi, n->key)) f(n->key, n->val);
        if (less_(n->key, hi)) range(n->right, lo, hi, f);         // right subtree may hold keys <= hi
    }
    // Returns height if the subtree is a valid AVL tree with keys in (lo, hi), else -1.
    int check(Node* n, const K* lo, const K* hi) const {
        if (!n) return 0;
        if ((lo && !less_(*lo, n->key)) || (hi && !less_(n->key, *hi))) return -1;
        int a = check(n->left, lo, &n->key), b = check(n->right, &n->key, hi);
        if (a < 0 || b < 0 || a - b > 1 || b - a > 1) return -1;
        int ht = 1 + (a > b ? a : b);
        return ht == n->h ? ht : -1;
    }

public:
    AvlTree() = default;
    ~AvlTree() { destroy(root_); }
    AvlTree(const AvlTree&) = delete;
    AvlTree& operator=(const AvlTree&) = delete;

    void put(const K& k, const V& v) {                     // insert or update
        Node* slot = nullptr; bool added = false;
        root_ = insert(root_, k, v, &slot, &added);
        if (!added) slot->val = v;
    }
    // Returns the value for k, inserting a default V first if k is missing.
    // Handy for grouping, e.g. tree.getOrInsert(time).push_back(id).
    V& getOrInsert(const K& k) {
        Node* slot = nullptr; bool added = false;
        root_ = insert(root_, k, V(), &slot, &added);
        return slot->val;
    }
    V* get(const K& k) {
        Node* n = root_;
        while (n) {
            if (less_(k, n->key)) n = n->left;
            else if (less_(n->key, k)) n = n->right;
            else return &n->val;
        }
        return nullptr;
    }
    const V* get(const K& k) const { return const_cast<AvlTree*>(this)->get(k); }
    bool contains(const K& k) const { return get(k) != nullptr; }
    bool remove(const K& k) {
        bool removed = false;
        root_ = erase(root_, k, &removed);
        return removed;
    }
    void clear() { destroy(root_); root_ = nullptr; size_ = 0; }

    // Smallest / largest key. Return nullptr when the tree is empty.
    const K* minKey() const { Node* n = root_; if (!n) return nullptr; while (n->left) n = n->left; return &n->key; }
    const K* maxKey() const { Node* n = root_; if (!n) return nullptr; while (n->right) n = n->right; return &n->key; }

    // Largest key <= k (floor) and smallest key >= k (ceiling). nullptr if none.
    const K* floorKey(const K& k) const {
        Node* n = root_; const K* best = nullptr;
        while (n) {
            if (less_(k, n->key)) n = n->left;
            else { best = &n->key; if (eq(n->key, k)) break; n = n->right; }
        }
        return best;
    }
    const K* ceilingKey(const K& k) const {
        Node* n = root_; const K* best = nullptr;
        while (n) {
            if (less_(n->key, k)) n = n->right;
            else { best = &n->key; if (eq(n->key, k)) break; n = n->left; }
        }
        return best;
    }

    // Calls f(key, value) for every key in [lo, hi], in ascending order.
    template <typename F> void rangeQuery(const K& lo, const K& hi, F f) { range(root_, lo, hi, f); }
    // Calls f(key, value) for every entry in ascending key order.
    template <typename F> void for_each(F f) { inorder(root_, f); }

    int size() const { return size_; }
    bool empty() const { return size_ == 0; }
    int height() const { return h(root_); }
    long long rotationCount() const { return rotations_; }
    bool isValid() const { return check(root_, nullptr, nullptr) >= 0; }   // for tests
};

}  // namespace nexa
