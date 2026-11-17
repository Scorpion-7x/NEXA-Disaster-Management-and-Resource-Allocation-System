#pragma once
#include "dynamic_array.h"

namespace nexa {

// LIFO stack. Used for recent operational action history. Push/Pop/Peek O(1).
template <typename T>
class Stack {
    DynamicArray<T> a_;
public:
    void push(const T& v) { a_.push_back(v); }
    T pop() {
        if (a_.empty()) throw std::out_of_range("Stack::pop on empty");
        return a_.pop_back();
    }
    T& peek() {
        if (a_.empty()) throw std::out_of_range("Stack::peek on empty");
        return a_.back();
    }
    bool empty() const { return a_.empty(); }
    int size() const { return a_.size(); }
    void clear() { a_.clear(); }
};

}  // namespace nexa
