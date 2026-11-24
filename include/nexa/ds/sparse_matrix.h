#pragma once
#include <stdexcept>
#include <utility>
#include "dynamic_array.h"

namespace nexa {

// Sparse matrix: stores only the cells that have a value.
// Each row keeps a singly linked list of (col, value) entries sorted by column.
// A dense rows x cols matrix needs rows*cols cells; this needs rows + nnz
// (nnz = number of stored cells).
//
//   get / set / erase on (r, c)   O(entries in row r)
//   walk a row                    O(entries in that row)
//   walk a column, column min     O(rows + nnz)
//   transpose                     O(rows + cols + nnz)
//
// Absent cells read as `fallback` (default T()). A stored value is kept even if it
// equals the fallback, so "distance 0 km" and "no entry" stay different.
template <typename T>
class SparseMatrix {
    struct Entry {
        int col;
        T val;
        Entry* next;
    };
    DynamicArray<Entry*> rows_;
    DynamicArray<int> rowNnz_;
    int nrows_, ncols_, nnz_ = 0;
    T fallback_;

    void check(int r, int c) const {
        if (r < 0 || r >= nrows_ || c < 0 || c >= ncols_) throw std::out_of_range("SparseMatrix: bad cell");
    }
    void freeAll() {
        for (int r = 0; r < rows_.size(); r++) {
            Entry* e = rows_[r];
            while (e) { Entry* nx = e->next; delete e; e = nx; }
            rows_[r] = nullptr;
        }
    }

public:
    SparseMatrix(int rows, int cols, const T& fallback = T())
        : nrows_(rows), ncols_(cols), fallback_(fallback) {
        if (rows < 0 || cols < 0) throw std::invalid_argument("SparseMatrix: negative size");
        for (int r = 0; r < rows; r++) { rows_.push_back(nullptr); rowNnz_.push_back(0); }
    }
    ~SparseMatrix() { freeAll(); }
    SparseMatrix(const SparseMatrix&) = delete;
    SparseMatrix& operator=(const SparseMatrix&) = delete;
    SparseMatrix(SparseMatrix&& o) noexcept
        : rows_(std::move(o.rows_)), rowNnz_(std::move(o.rowNnz_)),
          nrows_(o.nrows_), ncols_(o.ncols_), nnz_(o.nnz_), fallback_(o.fallback_) {
        o.nrows_ = o.ncols_ = o.nnz_ = 0;
    }

    // Insert or overwrite cell (r, c).
    void set(int r, int c, const T& v) {
        check(r, c);
        Entry** slot = &rows_[r];
        while (*slot && (*slot)->col < c) slot = &(*slot)->next;   // keep the row sorted by column
        if (*slot && (*slot)->col == c) { (*slot)->val = v; return; }
        *slot = new Entry{c, v, *slot};
        rowNnz_[r]++; nnz_++;
    }

    // Pointer to the stored value, or nullptr if the cell is empty.
    T* find(int r, int c) {
        check(r, c);
        for (Entry* e = rows_[r]; e && e->col <= c; e = e->next) if (e->col == c) return &e->val;
        return nullptr;
    }
    const T* find(int r, int c) const { return const_cast<SparseMatrix*>(this)->find(r, c); }
    T get(int r, int c) const { const T* p = find(r, c); return p ? *p : fallback_; }
    bool contains(int r, int c) const { return find(r, c) != nullptr; }

    bool erase(int r, int c) {
        check(r, c);
        Entry** slot = &rows_[r];
        while (*slot && (*slot)->col < c) slot = &(*slot)->next;
        if (!*slot || (*slot)->col != c) return false;
        Entry* dead = *slot;
        *slot = dead->next;
        delete dead;
        rowNnz_[r]--; nnz_--;
        return true;
    }

    // f(col, value&) for each stored cell in row r, in column order.
    template <typename F> void forEachInRow(int r, F f) {
        if (r < 0 || r >= nrows_) throw std::out_of_range("SparseMatrix: bad row");
        for (Entry* e = rows_[r]; e; e = e->next) f(e->col, e->val);
    }
    // f(row, value&) for each stored cell in column c, in row order.
    template <typename F> void forEachInCol(int c, F f) {
        if (c < 0 || c >= ncols_) throw std::out_of_range("SparseMatrix: bad column");
        for (int r = 0; r < nrows_; r++)
            for (Entry* e = rows_[r]; e && e->col <= c; e = e->next)
                if (e->col == c) f(r, e->val);
    }
    // f(row, col, value&) for every stored cell, row by row.
    template <typename F> void forEach(F f) {
        for (int r = 0; r < nrows_; r++)
            for (Entry* e = rows_[r]; e; e = e->next) f(r, e->col, e->val);
    }

    // Column with the smallest stored value in row r among cells where ok(col) is true,
    // or -1. Used to pick the cheapest available resource for an incident.
    template <typename Pred>
    int argMinInRow(int r, Pred ok) const {
        if (r < 0 || r >= nrows_) throw std::out_of_range("SparseMatrix: bad row");
        int best = -1; const T* bv = nullptr;
        for (Entry* e = rows_[r]; e; e = e->next)
            if (ok(e->col) && (!bv || e->val < *bv)) { best = e->col; bv = &e->val; }
        return best;
    }
    int argMinInRow(int r) const { return argMinInRow(r, [](int) { return true; }); }

    SparseMatrix transpose() const {
        SparseMatrix t(ncols_, nrows_, fallback_);
        // Walk rows from the bottom up and push to the FRONT of each new row,
        // so every transposed row comes out already sorted.
        for (int r = nrows_ - 1; r >= 0; r--)
            for (Entry* e = rows_[r]; e; e = e->next) {
                t.rows_[e->col] = new Entry{r, e->val, t.rows_[e->col]};
                t.rowNnz_[e->col]++; t.nnz_++;
            }
        return t;
    }

    void clear() {
        freeAll();
        for (int r = 0; r < rowNnz_.size(); r++) rowNnz_[r] = 0;
        nnz_ = 0;
    }

    int rows() const { return nrows_; }
    int cols() const { return ncols_; }
    int nnz() const { return nnz_; }
    int rowNnz(int r) const { return (r >= 0 && r < nrows_) ? rowNnz_[r] : 0; }
    double density() const { return nrows_ && ncols_ ? (double)nnz_ / ((double)nrows_ * ncols_) : 0.0; }
};

}  // namespace nexa
