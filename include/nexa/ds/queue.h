#pragma once
#include <stdexcept>
#include <utility>

namespace nexa {

// FIFO queue on a circular buffer. Enqueue/Dequeue amortized O(1).
template <typename T>
class Queue {
    T* buf_;
    int cap_, head_, count_;

    void grow() {
        T* nb = new T[cap_ * 2];
        for (int i = 0; i < count_; i++) nb[i] = std::move(buf_[(head_ + i) % cap_]);
        delete[] buf_;
        buf_ = nb; head_ = 0; cap_ *= 2;
    }
public:
    Queue() : buf_(new T[8]), cap_(8), head_(0), count_(0) {}
    ~Queue() { delete[] buf_; }
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    void enqueue(const T& v) {
        if (count_ == cap_) grow();
        buf_[(head_ + count_) % cap_] = v;
        count_++;
    }
    T dequeue() {
        if (count_ == 0) throw std::out_of_range("Queue::dequeue on empty");
        T v = std::move(buf_[head_]);
        head_ = (head_ + 1) % cap_;
        count_--;
        return v;
    }
    T& front() {
        if (count_ == 0) throw std::out_of_range("Queue::front on empty");
        return buf_[head_];
    }
    bool empty() const { return count_ == 0; }
    int size() const { return count_; }
};

}  // namespace nexa
