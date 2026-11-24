#pragma once
// Hand-written sorting algorithms. All work in place on a raw array T* a of length n.
//
//   Algorithm      Time (avg / worst)        Extra space   Stable   Key type
//   insertionSort  O(n^2)   / O(n^2)         O(1)          yes      comparator
//   mergeSort      O(n lg n)/ O(n lg n)      O(n)          yes      comparator
//   quickSort      O(n lg n)/ O(n^2)         O(lg n)       no       comparator
//   countingSort   O(n + k) / O(n + k)       O(n + k)      yes      small int range (k = max-min+1)
//   radixSort      O(d(n+256))               O(n)          yes      64-bit integer, d = bytes of range
//   bucketSort     O(n)     / O(n^2)         O(n)          yes      double, roughly uniform
//
// Comparator-based sorts take less(a, b) == "a must come before b".
// Key-based sorts take a key function key(const T&) -> int / long long / double.
// T must be default-constructible and copy-assignable.

#include <stdexcept>

namespace nexa {

namespace detail {
template <typename T>
inline void swapT(T& x, T& y) { T t = x; x = y; y = t; }

template <typename T, typename Less>
void mergeRec(T* a, T* buf, int lo, int hi, Less& less) {   // [lo, hi)
    if (hi - lo <= 16) {                                     // small runs: insertion sort
        for (int i = lo + 1; i < hi; i++) {
            T v = a[i]; int j = i - 1;
            while (j >= lo && less(v, a[j])) { a[j + 1] = a[j]; j--; }
            a[j + 1] = v;
        }
        return;
    }
    int mid = lo + (hi - lo) / 2;
    mergeRec(a, buf, lo, mid, less);
    mergeRec(a, buf, mid, hi, less);
    if (!less(a[mid], a[mid - 1])) return;                   // already in order
    int i = lo, j = mid, k = lo;
    while (i < mid && j < hi) buf[k++] = less(a[j], a[i]) ? a[j++] : a[i++];  // ties take left: stable
    while (i < mid) buf[k++] = a[i++];
    while (j < hi) buf[k++] = a[j++];
    for (k = lo; k < hi; k++) a[k] = buf[k];
}

template <typename T, typename Less>
void quickRec(T* a, int lo, int hi, Less& less) {            // [lo, hi]
    while (hi - lo > 16) {
        int mid = lo + (hi - lo) / 2;                        // median of three
        if (less(a[mid], a[lo])) swapT(a[mid], a[lo]);
        if (less(a[hi], a[lo])) swapT(a[hi], a[lo]);
        if (less(a[hi], a[mid])) swapT(a[hi], a[mid]);
        T p = a[mid];
        int i = lo, j = hi;                                  // Hoare partition
        while (i <= j) {
            while (less(a[i], p)) i++;
            while (less(p, a[j])) j--;
            if (i <= j) { swapT(a[i], a[j]); i++; j--; }
        }
        // recurse on the smaller side, loop on the larger: stack depth O(lg n)
        if (j - lo < hi - i) { quickRec(a, lo, j, less); lo = i; }
        else                 { quickRec(a, i, hi, less); hi = j; }
    }
    for (int x = lo + 1; x <= hi; x++) {
        T v = a[x]; int y = x - 1;
        while (y >= lo && less(v, a[y])) { a[y + 1] = a[y]; y--; }
        a[y + 1] = v;
    }
}
} // namespace detail

// ---------- comparison sorts ----------

template <typename T, typename Less>
void insertionSort(T* a, int n, Less less) {
    for (int i = 1; i < n; i++) {
        T v = a[i]; int j = i - 1;
        while (j >= 0 && less(v, a[j])) { a[j + 1] = a[j]; j--; }
        a[j + 1] = v;
    }
}

template <typename T, typename Less>
void mergeSort(T* a, int n, Less less) {
    if (n < 2) return;
    T* buf = new T[n];
    detail::mergeRec(a, buf, 0, n, less);
    delete[] buf;
}

template <typename T, typename Less>
void quickSort(T* a, int n, Less less) {
    if (n < 2) return;
    detail::quickRec(a, 0, n - 1, less);
}

// ---------- non-comparison sorts ----------

// Stable counting sort for integer keys in [minKey, maxKey].
// descending = true puts the largest key first (e.g. severity 10 before 1), still stable.
template <typename T, typename KeyFn>
void countingSort(T* a, int n, KeyFn key, int minKey, int maxKey, bool descending = false) {
    if (n < 2) return;
    if (maxKey < minKey) throw std::invalid_argument("countingSort: maxKey < minKey");
    int range = maxKey - minKey + 1;
    int* count = new int[range + 1];
    for (int i = 0; i <= range; i++) count[i] = 0;
    int* slot = new int[n];
    for (int i = 0; i < n; i++) {
        int k = key(a[i]);
        if (k < minKey || k > maxKey) {
            delete[] count; delete[] slot;
            throw std::out_of_range("countingSort: key outside [minKey, maxKey]");
        }
        slot[i] = descending ? (maxKey - k) : (k - minKey);
        count[slot[i] + 1]++;
    }
    for (int i = 0; i < range; i++) count[i + 1] += count[i];   // count[s] = start of bucket s
    T* out = new T[n];
    for (int i = 0; i < n; i++) out[count[slot[i]]++] = a[i];  // forward pass keeps it stable
    for (int i = 0; i < n; i++) a[i] = out[i];
    delete[] out; delete[] slot; delete[] count;
}

// Stable LSD radix sort, base 256, for any 64-bit integer key (negatives allowed).
// Only as many passes as the key range (max - min) needs bytes.
template <typename T, typename KeyFn>
void radixSort(T* a, int n, KeyFn key) {
    if (n < 2) return;
    unsigned long long* k = new unsigned long long[n];
    long long mn = (long long)key(a[0]), mx = mn;
    for (int i = 0; i < n; i++) {
        long long v = (long long)key(a[i]);
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        k[i] = (unsigned long long)v;
    }
    for (int i = 0; i < n; i++) k[i] -= (unsigned long long)mn;  // now 0..(mx-mn), order preserved
    unsigned long long range = (unsigned long long)mx - (unsigned long long)mn;
    int passes = 0;
    while (range) { passes++; range >>= 8; }

    T* ta = new T[n];
    unsigned long long* tk = new unsigned long long[n];
    int count[257];
    for (int p = 0; p < passes; p++) {
        int shift = 8 * p;
        for (int i = 0; i < 257; i++) count[i] = 0;
        for (int i = 0; i < n; i++) count[((k[i] >> shift) & 0xFF) + 1]++;
        for (int i = 0; i < 256; i++) count[i + 1] += count[i];
        for (int i = 0; i < n; i++) {
            int d = (int)((k[i] >> shift) & 0xFF);
            int pos = count[d]++;
            ta[pos] = a[i]; tk[pos] = k[i];
        }
        for (int i = 0; i < n; i++) { a[i] = ta[i]; k[i] = tk[i]; }
    }
    delete[] ta; delete[] tk; delete[] k;
}

// Stable bucket sort for floating-point keys (distances, response times...).
// Uses n buckets over [min, max]; fast when keys are roughly uniform.
template <typename T, typename KeyFn>
void bucketSort(T* a, int n, KeyFn key) {
    if (n < 2) return;
    double* k = new double[n];
    double mn = key(a[0]), mx = mn;
    for (int i = 0; i < n; i++) {
        k[i] = key(a[i]);
        if (k[i] < mn) mn = k[i];
        if (k[i] > mx) mx = k[i];
    }
    int B = n;
    int* b = new int[n];
    int* start = new int[B + 1];
    for (int i = 0; i <= B; i++) start[i] = 0;
    double span = mx - mn;
    for (int i = 0; i < n; i++) {
        int idx = span > 0 ? (int)((k[i] - mn) / span * B) : 0;
        if (idx < 0) idx = 0;
        if (idx >= B) idx = B - 1;                    // the max key lands exactly on B
        b[i] = idx;
        start[idx + 1]++;
    }
    for (int i = 0; i < B; i++) start[i + 1] += start[i];
    T* ta = new T[n];
    double* tk = new double[n];
    int* fill = new int[B];
    for (int i = 0; i < B; i++) fill[i] = start[i];
    for (int i = 0; i < n; i++) { int pos = fill[b[i]]++; ta[pos] = a[i]; tk[pos] = k[i]; }

    // insertion sort each bucket by key (stable, strict <)
    for (int s = 0; s < B; s++) {
        for (int i = start[s] + 1; i < start[s + 1]; i++) {
            T v = ta[i]; double kv = tk[i]; int j = i - 1;
            while (j >= start[s] && kv < tk[j]) { ta[j + 1] = ta[j]; tk[j + 1] = tk[j]; j--; }
            ta[j + 1] = v; tk[j + 1] = kv;
        }
    }
    for (int i = 0; i < n; i++) a[i] = ta[i];
    delete[] fill; delete[] tk; delete[] ta; delete[] start; delete[] b; delete[] k;
}

} // namespace nexa
