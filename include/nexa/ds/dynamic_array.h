#pragma once
#include <stdexcept>
#include <utility>

namespace nexa {

// Resizable array (amortized O(1) push_back, O(1) access). Hand-written, no STL.
template <typename T>
class DynamicArray {
    T* data_;
    int size_, cap_;

    void grow() {
        int nc = cap_ < 8 ? 8 : cap_ * 2;
        T* nd = new T[nc];
        for (int i = 0; i < size_; i++) nd[i] = std::move(data_[i]);
        delete[] data_;
        data_ = nd;
        cap_ = nc;
    }

public:
    DynamicArray() : data_(new T[8]), size_(0), cap_(8) {}
    DynamicArray(const DynamicArray& o) : data_(new T[o.cap_ > 0 ? o.cap_ : 1]), size_(o.size_), cap_(o.cap_ > 0 ? o.cap_ : 1) {
        for (int i = 0; i < size_; i++) data_[i] = o.data_[i];
    }
    DynamicArray(DynamicArray&& o) noexcept : data_(o.data_), size_(o.size_), cap_(o.cap_) {
        o.data_ = nullptr; o.size_ = 0; o.cap_ = 0;
    }
    DynamicArray& operator=(DynamicArray o) {  // copy-and-swap (handles copy + move)
        std::swap(data_, o.data_); std::swap(size_, o.size_); std::swap(cap_, o.cap_);
        return *this;
    }
    ~DynamicArray() { delete[] data_; }

    void push_back(const T& v) { if (size_ == cap_) grow(); data_[size_++] = v; }
    void push_back(T&& v)      { if (size_ == cap_) grow(); data_[size_++] = std::move(v); }
    T pop_back() {
        if (size_ == 0) throw std::out_of_range("DynamicArray::pop_back on empty");
        return std::move(data_[--size_]);
    }
    T& operator[](int i) {
        if (i < 0 || i >= size_) throw std::out_of_range("DynamicArray index");
        return data_[i];
    }
    const T& operator[](int i) const {
        if (i < 0 || i >= size_) throw std::out_of_range("DynamicArray index");
        return data_[i];
    }
    T& back() { return (*this)[size_ - 1]; }
    int size() const { return size_; }
    bool empty() const { return size_ == 0; }
    void clear() { size_ = 0; }

    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }
};

}  // namespace nexa
