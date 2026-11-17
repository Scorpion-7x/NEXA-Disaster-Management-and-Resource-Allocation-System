#pragma once
#include "../ds/dynamic_array.h"

namespace nexa {

// Linear search: first index where pred(x) is true, or -1. O(n), works on unsorted data.
template <typename T, typename Pred>
int linearSearch(const DynamicArray<T>& v, Pred pred) {
    for (int i = 0; i < v.size(); i++) if (pred(v[i])) return i;
    return -1;
}

// Collects the indices of every match. O(n).
template <typename T, typename Pred>
DynamicArray<int> linearSearchAll(const DynamicArray<T>& v, Pred pred) {
    DynamicArray<int> out;
    for (int i = 0; i < v.size(); i++) if (pred(v[i])) out.push_back(i);
    return out;
}

// The binary searches below need v sorted ascending by key(x). O(log n).
// key is a function x -> comparable value, e.g. [](const Incident& x) { return x.id; }

// First index whose key is >= target (v.size() if none).
template <typename T, typename KeyFn, typename K>
int lowerBound(const DynamicArray<T>& v, KeyFn key, const K& target) {
    int lo = 0, hi = v.size();                 // answer lies in [lo, hi]
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (key(v[mid]) < target) lo = mid + 1; else hi = mid;
    }
    return lo;
}

// First index whose key is > target (v.size() if none).
template <typename T, typename KeyFn, typename K>
int upperBound(const DynamicArray<T>& v, KeyFn key, const K& target) {
    int lo = 0, hi = v.size();
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (target < key(v[mid])) hi = mid; else lo = mid + 1;
    }
    return lo;
}

// Index of an element whose key equals target, or -1.
template <typename T, typename KeyFn, typename K>
int binarySearch(const DynamicArray<T>& v, KeyFn key, const K& target) {
    int i = lowerBound(v, key, target);
    return (i < v.size() && !(target < key(v[i]))) ? i : -1;
}

// Range query on sorted data: number of elements with lo <= key <= hi,
// first one at index *first. E.g. incidents reported between two times.
template <typename T, typename KeyFn, typename K>
int rangeSearch(const DynamicArray<T>& v, KeyFn key, const K& lo, const K& hi, int* first = nullptr) {
    int a = lowerBound(v, key, lo), b = upperBound(v, key, hi);
    if (first) *first = a;
    return b > a ? b - a : 0;
}

}  // namespace nexa
