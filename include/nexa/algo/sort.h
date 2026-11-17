#pragma once
#include <utility>
#include "../ds/dynamic_array.h"

namespace nexa {

// All sorts take a comparator less(a, b) that returns true if a must come BEFORE b,
// so the same IncidentHigherPriority used by the PriorityQueue also sorts by priority.
//
//                 best        average     worst       extra space  stable
// insertionSort   O(n)        O(n^2)      O(n^2)      O(1)         yes
// mergeSort       O(n log n)  O(n log n)  O(n log n)  O(n)         yes
// quickSort       O(n log n)  O(n log n)  O(n^2)      O(log n)     no
// heapSort        O(n log n)  O(n log n)  O(n log n)  O(1)         no
// countingSort    O(n + k)    O(n + k)    O(n + k)    O(n + k)     yes   (integer keys in [lo, hi])

namespace detail {

template <typename T, typename Less>
void insertionRange(T* a, int lo, int hi, Less less) {          // sorts a[lo..hi]
    for (int i = lo + 1; i <= hi; i++) {
        T x = std::move(a[i]);
        int j = i - 1;
        while (j >= lo && less(x, a[j])) { a[j + 1] = std::move(a[j]); j--; }
        a[j + 1] = std::move(x);
    }
}

template <typename T, typename Less>
void mergeRec(T* a, T* tmp, int lo, int hi, Less less) {        // sorts a[lo..hi]
    if (hi - lo < 16) { insertionRange(a, lo, hi, less); return; }
    int mid = lo + (hi - lo) / 2;
    mergeRec(a, tmp, lo, mid, less);
    mergeRec(a, tmp, mid + 1, hi, less);
    if (!less(a[mid + 1], a[mid])) return;                       // already in order
    int i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) tmp[k++] = less(a[j], a[i]) ? std::move(a[j++]) : std::move(a[i++]);
    while (i <= mid) tmp[k++] = std::move(a[i++]);
    while (j <= hi)  tmp[k++] = std::move(a[j++]);
    for (k = lo; k <= hi; k++) a[k] = std::move(tmp[k]);
}

template <typename T, typename Less>
void quickRec(T* a, int lo, int hi, Less less) {
    while (hi - lo >= 16) {
        // median-of-three pivot, moved to a[hi]
        int mid = lo + (hi - lo) / 2;
        if (less(a[mid], a[lo])) std::swap(a[mid], a[lo]);
        if (less(a[hi], a[lo]))  std::swap(a[hi], a[lo]);
        if (less(a[mid], a[hi])) std::swap(a[mid], a[hi]);
        // Hoare-style partition around pivot value a[hi]
        int i = lo - 1, j = hi;
        for (;;) {
            while (less(a[++i], a[hi])) {}
            while (j > lo && less(a[hi], a[--j])) {}
            if (i >= j) break;
            std::swap(a[i], a[j]);
        }
        std::swap(a[i], a[hi]);
        // recurse into the smaller side, loop on the larger: O(log n) stack
        if (i - lo < hi - i) { quickRec(a, lo, i - 1, less); lo = i + 1; }
        else                 { quickRec(a, i + 1, hi, less); hi = i - 1; }
    }
    insertionRange(a, lo, hi, less);
}

template <typename T, typename Less>
void siftDown(T* a, int i, int n, Less less) {                  // max-heap w.r.t. less
    for (;;) {
        int l = 2 * i + 1, r = l + 1, b = i;
        if (l < n && less(a[b], a[l])) b = l;
        if (r < n && less(a[b], a[r])) b = r;
        if (b == i) return;
        std::swap(a[i], a[b]);
        i = b;
    }
}

}  // namespace detail

template <typename T, typename Less>
void insertionSort(DynamicArray<T>& v, Less less) {
    if (v.size() > 1) detail::insertionRange(v.begin(), 0, v.size() - 1, less);
}

template <typename T, typename Less>
void mergeSort(DynamicArray<T>& v, Less less) {
    int n = v.size();
    if (n < 2) return;
    T* tmp = new T[n];
    detail::mergeRec(v.begin(), tmp, 0, n - 1, less);
    delete[] tmp;
}

template <typename T, typename Less>
void quickSort(DynamicArray<T>& v, Less less) {
    if (v.size() > 1) detail::quickRec(v.begin(), 0, v.size() - 1, less);
}

template <typename T, typename Less>
void heapSort(DynamicArray<T>& v, Less less) {
    T* a = v.begin();
    int n = v.size();
    for (int i = n / 2 - 1; i >= 0; i--) detail::siftDown(a, i, n, less);
    for (int end = n - 1; end > 0; end--) {
        std::swap(a[0], a[end]);               // largest goes to the back
        detail::siftDown(a, 0, end, less);
    }
}

// Stable counting sort on an integer key in [lo, hi], e.g. severity 1..10.
// Pass descending = true to put the highest key first. Keys outside the range go last.
template <typename T, typename KeyFn>
void countingSort(DynamicArray<T>& v, KeyFn key, int lo, int hi, bool descending = false) {
    int n = v.size(), k = hi - lo + 1;
    if (n < 2 || k <= 0) return;
    int* count = new int[k + 2]();             // slot k+1 collects out-of-range keys
    auto bucket = [&](const T& x) {
        int kv = key(x);
        if (kv < lo || kv > hi) return k + 1;
        return descending ? (hi - kv) + 1 : (kv - lo) + 1;
    };
    for (int i = 0; i < n; i++) count[bucket(v[i])]++;
    // count[b] becomes the start index for bucket b (bucket 1..k+1 -> positions)
    int run = 0;
    for (int b = 1; b <= k + 1; b++) { int c = count[b]; count[b] = run; run += c; }
    T* out = new T[n];
    for (int i = 0; i < n; i++) out[count[bucket(v[i])]++] = std::move(v[i]);
    for (int i = 0; i < n; i++) v[i] = std::move(out[i]);
    delete[] out;
    delete[] count;
}

template <typename T, typename Less>
bool isSorted(const DynamicArray<T>& v, Less less) {
    for (int i = 1; i < v.size(); i++) if (less(v[i], v[i - 1])) return false;
    return true;
}

}  // namespace nexa
