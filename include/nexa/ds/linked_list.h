#pragma once
#include <stdexcept>

namespace nexa {

// Singly linked list with tail pointer. Used for incident event timelines.
// push_front / push_back: O(1). find / remove: O(n).
template <typename T>
class LinkedList {
public:
    struct Node {
        T val;
        Node* next;
        explicit Node(const T& v) : val(v), next(nullptr) {}
    };
private:
    Node *head_, *tail_;
    int size_;
public:
    LinkedList() : head_(nullptr), tail_(nullptr), size_(0) {}
    ~LinkedList() { clear(); }
    LinkedList(const LinkedList&) = delete;
    LinkedList& operator=(const LinkedList&) = delete;

    void push_front(const T& v) {
        Node* n = new Node(v);
        n->next = head_; head_ = n;
        if (!tail_) tail_ = n;
        size_++;
    }
    void push_back(const T& v) {
        Node* n = new Node(v);
        if (tail_) tail_->next = n; else head_ = n;
        tail_ = n;
        size_++;
    }
    T pop_front() {
        if (!head_) throw std::out_of_range("LinkedList::pop_front on empty");
        Node* n = head_;
        T v = n->val;
        head_ = n->next;
        if (!head_) tail_ = nullptr;
        delete n; size_--;
        return v;
    }
    // Returns pointer to first element satisfying pred, or nullptr.
    template <typename Pred>
    T* find(Pred pred) {
        for (Node* n = head_; n; n = n->next) if (pred(n->val)) return &n->val;
        return nullptr;
    }
    // Removes first element satisfying pred. Returns true if removed.
    template <typename Pred>
    bool remove_first(Pred pred) {
        Node* prev = nullptr;
        for (Node* n = head_; n; prev = n, n = n->next) {
            if (pred(n->val)) {
                if (prev) prev->next = n->next; else head_ = n->next;
                if (n == tail_) tail_ = prev;
                delete n; size_--;
                return true;
            }
        }
        return false;
    }
    void clear() {
        while (head_) { Node* n = head_; head_ = n->next; delete n; }
        tail_ = nullptr; size_ = 0;
    }
    Node* head() const { return head_; }
    int size() const { return size_; }
    bool empty() const { return size_ == 0; }
};

}  // namespace nexa
