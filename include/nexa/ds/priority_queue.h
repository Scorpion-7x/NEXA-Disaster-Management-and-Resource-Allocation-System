#pragma once
#include <stdexcept>
#include <utility>

namespace nexa {

// Binary-heap priority queue. Cmp(a,b) must return true if a has HIGHER priority than b.
// Insert / Extract: O(log n).
template <typename T, typename Cmp>
class PriorityQueue {
    T* data_;
    int cap_, n_;
    Cmp cmp_;

    void grow() {
        T* nd = new T[cap_ * 2];
        for (int i = 0; i < n_; i++) nd[i] = std::move(data_[i]);
        delete[] data_;
        data_ = nd; cap_ *= 2;
    }
    void up(int i) {
        while (i > 0 && cmp_(data_[i], data_[(i - 1) / 2])) {
            std::swap(data_[i], data_[(i - 1) / 2]);
            i = (i - 1) / 2;
        }
    }
    void down(int i) {
        for (;;) {
            int l = 2 * i + 1, r = l + 1, b = i;
            if (l < n_ && cmp_(data_[l], data_[b])) b = l;
            if (r < n_ && cmp_(data_[r], data_[b])) b = r;
            if (b == i) return;
            std::swap(data_[i], data_[b]);
            i = b;
        }
    }
public:
    PriorityQueue() : data_(new T[16]), cap_(16), n_(0) {}
    ~PriorityQueue() { delete[] data_; }
    PriorityQueue(const PriorityQueue&) = delete;
    PriorityQueue& operator=(const PriorityQueue&) = delete;

    void push(const T& v) { if (n_ == cap_) grow(); data_[n_] = v; up(n_++); }
    T pop() {
        if (!n_) throw std::out_of_range("PriorityQueue::pop on empty");
        T top = std::move(data_[0]);
        data_[0] = std::move(data_[--n_]);
        if (n_) down(0);
        return top;
    }
    const T& top() const {
        if (!n_) throw std::out_of_range("PriorityQueue::top on empty");
        return data_[0];
    }
    bool empty() const { return n_ == 0; }
    int size() const { return n_; }
};

}  // namespace nexa
