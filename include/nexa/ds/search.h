#pragma once
// Hand-written searching algorithms on raw arrays.
//
//   linearSearch / linearFilter   O(n)            unsorted data, any predicate
//   binarySearch / lower / upper  O(lg n)         data sorted by the same key
//   selectKth  (median search)    O(n) worst case median-of-medians selection
//   median                        O(n)            lower median, input left untouched
//
// "Median search" here is order-statistic selection: find the k-th smallest
// element without fully sorting. Used for e.g. the median response time.

#include <stdexcept>

namespace nexa {

// ---------- linear ----------

// index of the first element with pred(a[i]) true, or -1
template <typename T, typename Pred>
int linearSearch(const T* a, int n, Pred pred) {
    for (int i = 0; i < n; i++) if (pred(a[i])) return i;
    return -1;
}

// writes the indices of all matches into outIdx (must hold n ints); returns how many
template <typename T, typename Pred>
int linearFilter(const T* a, int n, Pred pred, int* outIdx) {
    int m = 0;
    for (int i = 0; i < n; i++) if (pred(a[i])) outIdx[m++] = i;
    return m;
}

// ---------- binary (array sorted ascending by key(a[i])) ----------

// first index i with key(a[i]) >= k, or n
template <typename T, typename K, typename KeyFn>
int lowerBound(const T* a, int n, const K& k, KeyFn key) {
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (key(a[mid]) < k) lo = mid + 1; else hi = mid;
    }
    return lo;
}

// first index i with key(a[i]) > k, or n
template <typename T, typename K, typename KeyFn>
int upperBound(const T* a, int n, const K& k, KeyFn key) {
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (k < key(a[mid])) hi = mid; else lo = mid + 1;
    }
    return lo;
}

// index of the first element whose key equals k, or -1
template <typename T, typename K, typename KeyFn>
int binarySearch(const T* a, int n, const K& k, KeyFn key) {
    int i = lowerBound(a, n, k, key);
    return (i < n && !(k < key(a[i]))) ? i : -1;
}

// ---------- selection / median ----------

namespace detail {
template <typename T>
inline void swapS(T& x, T& y) { T t = x; x = y; y = t; }

template <typename T, typename Less>
void insertionRange(T* a, int lo, int hi, Less& less) {     // [lo, hi]
    for (int i = lo + 1; i <= hi; i++) {
        T v = a[i]; int j = i - 1;
        while (j >= lo && less(v, a[j])) { a[j + 1] = a[j]; j--; }
        a[j + 1] = v;
    }
}

template <typename T, typename Less>
void selectRange(T* a, int lo, int hi, int k, Less& less);

// Median of medians: returns an index in [lo, hi] holding a good pivot.
template <typename T, typename Less>
int pivotMoM(T* a, int lo, int hi, Less& less) {
    if (hi - lo < 5) { insertionRange(a, lo, hi, less); return lo + (hi - lo) / 2; }
    int g = 0;
    for (int s = lo; s <= hi; s += 5) {
        int e = s + 4 > hi ? hi : s + 4;
        insertionRange(a, s, e, less);
        swapS(a[lo + g], a[s + (e - s) / 2]);             // gather group medians at the front
        g++;
    }
    int mid = lo + (g - 1) / 2;
    selectRange(a, lo, lo + g - 1, mid, less);
    return mid;
}

// Rearranges a[lo..hi] so that a[k] is the element that would be there if sorted.
template <typename T, typename Less>
void selectRange(T* a, int lo, int hi, int k, Less& less) {
    while (true) {
        if (lo >= hi) return;
        if (hi - lo < 5) { insertionRange(a, lo, hi, less); return; }
        T p = a[pivotMoM(a, lo, hi, less)];
        int lt = lo, i = lo, gt = hi;                     // 3-way partition: < p | == p | > p
        while (i <= gt) {
            if (less(a[i], p))      swapS(a[lt++], a[i++]);
            else if (less(p, a[i])) swapS(a[i], a[gt--]);
            else                    i++;
        }
        if (k < lt) hi = lt - 1;
        else if (k > gt) lo = gt + 1;
        else return;                                      // k falls among the pivot copies
    }
}
} // namespace detail

// k-th smallest (0-based). Reorders a: afterwards a[k] holds the answer,
// everything before it is <= a[k], everything after is >= a[k].
template <typename T, typename Less>
T selectKth(T* a, int n, int k, Less less) {
    if (k < 0 || k >= n) throw std::out_of_range("selectKth: k out of range");
    detail::selectRange(a, 0, n - 1, k, less);
    return a[k];
}

// Lower median ((n-1)/2-th smallest) without modifying the input.
template <typename T, typename Less>
T median(const T* a, int n, Less less) {
    if (n <= 0) throw std::invalid_argument("median: empty input");
    T* tmp = new T[n];
    for (int i = 0; i < n; i++) tmp[i] = a[i];
    T m;
    try { m = selectKth(tmp, n, (n - 1) / 2, less); }
    catch (...) { delete[] tmp; throw; }
    delete[] tmp;
    return m;
}

} // namespace nexa
