#pragma once
#include <stdexcept>
#include "dynamic_array.h"

namespace nexa {

// Multi-linked list (orthogonal list). Every record lives on THREE linked lists at once:
//   - its row list    e.g. every deployment made for incident i
//   - its column list e.g. every deployment of resource r
//   - the global list every deployment, in the order it was added (history)
// One copy of each record, many ways to walk it. All lists are doubly linked,
// so a record can be unlinked from all three in O(1) once you hold its node.
//
//   add                      O(1) amortized
//   remove(node)             O(1)
//   find(row, col)           O(min(row length, column length))
//   walk a row / col / all   O(length of that list)
template <typename T>
class MultiLinkedList {
public:
    struct Node {
        int row, col;
        T val;
        Node *prevInRow = nullptr, *nextInRow = nullptr;
        Node *prevInCol = nullptr, *nextInCol = nullptr;
        Node *prevAll = nullptr, *nextAll = nullptr;
        Node(int r, int c, const T& v) : row(r), col(c), val(v) {}
    };

private:
    DynamicArray<Node*> rowHead_, rowTail_, colHead_, colTail_;
    DynamicArray<int> rowCount_, colCount_;
    Node *head_ = nullptr, *tail_ = nullptr;
    int size_ = 0;

    static void growTo(DynamicArray<Node*>& h, DynamicArray<Node*>& t, DynamicArray<int>& cnt, int i) {
        while (h.size() <= i) { h.push_back(nullptr); t.push_back(nullptr); cnt.push_back(0); }
    }

public:
    MultiLinkedList() = default;
    ~MultiLinkedList() { clear(); }
    MultiLinkedList(const MultiLinkedList&) = delete;
    MultiLinkedList& operator=(const MultiLinkedList&) = delete;

    // Appends a record to the end of its row list, its column list and the global list.
    Node* add(int row, int col, const T& val) {
        if (row < 0 || col < 0) throw std::out_of_range("MultiLinkedList: negative row/col");
        growTo(rowHead_, rowTail_, rowCount_, row);
        growTo(colHead_, colTail_, colCount_, col);
        Node* n = new Node(row, col, val);

        n->prevInRow = rowTail_[row];
        if (rowTail_[row]) rowTail_[row]->nextInRow = n; else rowHead_[row] = n;
        rowTail_[row] = n; rowCount_[row]++;

        n->prevInCol = colTail_[col];
        if (colTail_[col]) colTail_[col]->nextInCol = n; else colHead_[col] = n;
        colTail_[col] = n; colCount_[col]++;

        n->prevAll = tail_;
        if (tail_) tail_->nextAll = n; else head_ = n;
        tail_ = n;
        size_++;
        return n;
    }

    // Unlinks n from all three lists and frees it.
    void remove(Node* n) {
        if (!n) return;
        if (n->prevInRow) n->prevInRow->nextInRow = n->nextInRow; else rowHead_[n->row] = n->nextInRow;
        if (n->nextInRow) n->nextInRow->prevInRow = n->prevInRow; else rowTail_[n->row] = n->prevInRow;
        rowCount_[n->row]--;

        if (n->prevInCol) n->prevInCol->nextInCol = n->nextInCol; else colHead_[n->col] = n->nextInCol;
        if (n->nextInCol) n->nextInCol->prevInCol = n->prevInCol; else colTail_[n->col] = n->prevInCol;
        colCount_[n->col]--;

        if (n->prevAll) n->prevAll->nextAll = n->nextAll; else head_ = n->nextAll;
        if (n->nextAll) n->nextAll->prevAll = n->prevAll; else tail_ = n->prevAll;
        size_--;
        delete n;
    }

    // First record linking row and col, or nullptr. Walks whichever list is shorter.
    Node* find(int row, int col) const {
        if (row < 0 || col < 0 || row >= rowHead_.size() || col >= colHead_.size()) return nullptr;
        if (rowCount_[row] <= colCount_[col]) {
            for (Node* n = rowHead_[row]; n; n = n->nextInRow) if (n->col == col) return n;
        } else {
            for (Node* n = colHead_[col]; n; n = n->nextInCol) if (n->row == row) return n;
        }
        return nullptr;
    }

    // f(Node&) for every record in a row / column / the whole history, in insertion order.
    template <typename F> void forEachInRow(int row, F f) {
        if (row < 0 || row >= rowHead_.size()) return;
        for (Node* n = rowHead_[row]; n; n = n->nextInRow) f(*n);
    }
    template <typename F> void forEachInCol(int col, F f) {
        if (col < 0 || col >= colHead_.size()) return;
        for (Node* n = colHead_[col]; n; n = n->nextInCol) f(*n);
    }
    template <typename F> void forEachAll(F f) {
        for (Node* n = head_; n; n = n->nextAll) f(*n);
    }

    Node* rowFirst(int row) const { return row >= 0 && row < rowHead_.size() ? rowHead_[row] : nullptr; }
    Node* rowLast(int row) const { return row >= 0 && row < rowTail_.size() ? rowTail_[row] : nullptr; }
    Node* colFirst(int col) const { return col >= 0 && col < colHead_.size() ? colHead_[col] : nullptr; }
    Node* colLast(int col) const { return col >= 0 && col < colTail_.size() ? colTail_[col] : nullptr; }
    Node* first() const { return head_; }
    Node* last() const { return tail_; }

    int rowSize(int row) const { return row >= 0 && row < rowCount_.size() ? rowCount_[row] : 0; }
    int colSize(int col) const { return col >= 0 && col < colCount_.size() ? colCount_[col] : 0; }
    int size() const { return size_; }
    bool empty() const { return size_ == 0; }

    void clear() {
        Node* n = head_;
        while (n) { Node* nx = n->nextAll; delete n; n = nx; }
        head_ = tail_ = nullptr; size_ = 0;
        for (int i = 0; i < rowHead_.size(); i++) { rowHead_[i] = rowTail_[i] = nullptr; rowCount_[i] = 0; }
        for (int i = 0; i < colHead_.size(); i++) { colHead_[i] = colTail_[i] = nullptr; colCount_[i] = 0; }
    }
};

}  // namespace nexa
