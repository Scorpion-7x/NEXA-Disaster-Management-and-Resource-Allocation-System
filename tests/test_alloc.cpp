#include "test_util.h"
#include "nexa/ds/multi_linked_list.h"
#include "nexa/ds/sparse_matrix.h"
using namespace nexa;

static unsigned g_seed = 99;
static int rnd(int mod) { g_seed = g_seed * 1103515245u + 12345u; return (int)((g_seed >> 8) % (unsigned)mod); }

struct Dep { double km = 0; long long at = 0; };

// Walks every list in all directions and checks the links agree with the counts.
static bool consistent(MultiLinkedList<int>& m, int rows, int cols) {
    int total = 0;
    for (int r = 0; r < rows; r++) {
        int fwd = 0;
        MultiLinkedList<int>::Node* prev = nullptr;
        for (auto* n = m.rowFirst(r); n; n = n->nextInRow) {
            if (n->row != r || n->prevInRow != prev) return false;
            prev = n; fwd++;
        }
        if (prev != m.rowLast(r) || fwd != m.rowSize(r)) return false;
        total += fwd;
    }
    if (total != m.size()) return false;
    total = 0;
    for (int c = 0; c < cols; c++) {
        int fwd = 0;
        MultiLinkedList<int>::Node* prev = nullptr;
        for (auto* n = m.colFirst(c); n; n = n->nextInCol) {
            if (n->col != c || n->prevInCol != prev) return false;
            prev = n; fwd++;
        }
        if (prev != m.colLast(c) || fwd != m.colSize(c)) return false;
        total += fwd;
    }
    if (total != m.size()) return false;
    int all = 0;
    MultiLinkedList<int>::Node* prev = nullptr;
    for (auto* n = m.first(); n; n = n->nextAll) { if (n->prevAll != prev) return false; prev = n; all++; }
    return prev == m.last() && all == m.size();
}

int main() {
    { // multi-linked list: empty
        MultiLinkedList<Dep> m;
        CHECK(m.empty() && m.size() == 0 && m.first() == nullptr && m.find(0, 0) == nullptr);
        CHECK(m.rowSize(5) == 0 && m.colSize(5) == 0 && m.rowFirst(3) == nullptr);
        int calls = 0;
        m.forEachInRow(2, [&](MultiLinkedList<Dep>::Node&) { calls++; });
        m.forEachAll([&](MultiLinkedList<Dep>::Node&) { calls++; });
        CHECK(calls == 0);
        bool threw = false;
        try { m.add(-1, 0, Dep{}); } catch (const std::out_of_range&) { threw = true; }
        CHECK(threw);
    }
    { // multi-linked list: one record sits on three lists
        MultiLinkedList<Dep> m;
        auto* a = m.add(0, 2, Dep{1.5, 100});   // incident 0 <- resource 2
        auto* b = m.add(1, 2, Dep{2.0, 105});   // incident 1 <- resource 2
        auto* c = m.add(0, 4, Dep{0.7, 110});   // incident 0 <- resource 4
        CHECK(m.size() == 3 && m.rowSize(0) == 2 && m.rowSize(1) == 1);
        CHECK(m.colSize(2) == 2 && m.colSize(4) == 1 && m.colSize(3) == 0);
        CHECK(m.rowFirst(0) == a && a->nextInRow == c && m.rowLast(0) == c);
        CHECK(m.colFirst(2) == a && a->nextInCol == b);
        CHECK(m.first() == a && a->nextAll == b && b->nextAll == c && m.last() == c);
        CHECK(m.find(0, 4) == c && m.find(1, 4) == nullptr && m.find(9, 9) == nullptr);
        double sum = 0;
        m.forEachInCol(2, [&](MultiLinkedList<Dep>::Node& n) { sum += n.val.km; });
        CHECK(sum == 3.5);
        long long order[3]; int k = 0;
        m.forEachAll([&](MultiLinkedList<Dep>::Node& n) { order[k++] = n.val.at; });
        CHECK(k == 3 && order[0] == 100 && order[1] == 105 && order[2] == 110);

        m.remove(a);                              // middle-of-history, head of row 0 and col 2
        CHECK(m.size() == 2 && m.rowFirst(0) == c && m.colFirst(2) == b && m.first() == b);
        CHECK(c->prevInRow == nullptr && b->prevInCol == nullptr && b->prevAll == nullptr);
        m.remove(c);
        CHECK(m.rowSize(0) == 0 && m.rowFirst(0) == nullptr && m.rowLast(0) == nullptr && m.last() == b);
        m.remove(b);
        CHECK(m.empty() && m.first() == nullptr && m.colFirst(2) == nullptr);
        m.add(0, 0, Dep{9, 1});                   // reusable after emptying
        CHECK(m.size() == 1 && m.find(0, 0)->val.km == 9);
    }
    { // multi-linked list: random adds and removes keep every list consistent
        MultiLinkedList<int> m;
        const int R = 40, C = 25;
        MultiLinkedList<int>::Node* live[4000];
        int nLive = 0;
        int count[R][C] = {};
        bool ok = true;
        for (int step = 0; step < 4000; step++) {
            if (nLive == 0 || rnd(3) != 0) {
                int r = rnd(R), c = rnd(C);
                live[nLive++] = m.add(r, c, step);
                count[r][c]++;
            } else {
                int i = rnd(nLive);
                count[live[i]->row][live[i]->col]--;
                m.remove(live[i]);
                live[i] = live[--nLive];
            }
            if (step % 500 == 0 && !consistent(m, R, C)) ok = false;
        }
        CHECK(ok);
        CHECK(consistent(m, R, C));
        CHECK(m.size() == nLive);
        bool findOk = true;
        for (int r = 0; r < R; r++)
            for (int c = 0; c < C; c++) {
                auto* n = m.find(r, c);
                if ((count[r][c] > 0) != (n != nullptr)) findOk = false;
                if (n && (n->row != r || n->col != c)) findOk = false;
            }
        CHECK(findOk);
        m.clear();
        CHECK(m.empty() && consistent(m, R, C));
    }

    { // sparse matrix: basics
        SparseMatrix<double> s(50, 30, -1.0);       // -1 = "no entry"
        CHECK(s.rows() == 50 && s.cols() == 30 && s.nnz() == 0 && s.density() == 0);
        CHECK(s.get(3, 4) == -1.0 && !s.contains(3, 4) && s.find(3, 4) == nullptr);
        s.set(3, 4, 2.5); s.set(3, 1, 0.0); s.set(3, 29, 7.0); s.set(49, 0, 1.0);
        CHECK(s.nnz() == 4 && s.rowNnz(3) == 3 && s.rowNnz(0) == 0);
        CHECK(s.get(3, 1) == 0.0 && s.contains(3, 1));  // a stored 0 is not "absent"
        s.set(3, 4, 1.25);                               // overwrite, no new cell
        CHECK(s.nnz() == 4 && s.get(3, 4) == 1.25);
        int cols[3], k = 0;
        s.forEachInRow(3, [&](int c, double&) { cols[k++] = c; });
        CHECK(k == 3 && cols[0] == 1 && cols[1] == 4 && cols[2] == 29);   // sorted by column
        CHECK(s.argMinInRow(3) == 1);
        CHECK(s.argMinInRow(3, [](int c) { return c != 1; }) == 4);
        CHECK(s.argMinInRow(3, [](int) { return false; }) == -1 && s.argMinInRow(0) == -1);
        CHECK(s.erase(3, 4) && !s.erase(3, 4) && s.nnz() == 3 && s.get(3, 4) == -1.0);
        int colHits = 0;
        s.forEachInCol(0, [&](int r, double& v) { colHits++; CHECK(r == 49 && v == 1.0); });
        CHECK(colHits == 1);
        bool threw = false;
        try { s.set(50, 0, 1); } catch (const std::out_of_range&) { threw = true; }
        CHECK(threw);
        threw = false;
        try { s.get(0, 30); } catch (const std::out_of_range&) { threw = true; }
        CHECK(threw);
    }
    { // sparse matrix vs a dense array, random operations
        const int R = 37, C = 23;
        SparseMatrix<int> s(R, C);
        int dense[R][C]; bool has[R][C];
        for (int r = 0; r < R; r++) for (int c = 0; c < C; c++) { dense[r][c] = 0; has[r][c] = false; }
        int expectNnz = 0;
        bool eraseOk = true;
        for (int step = 0; step < 5000; step++) {
            int r = rnd(R), c = rnd(C);
            if (rnd(4) == 0) { if (s.erase(r, c) != has[r][c]) eraseOk = false; if (has[r][c]) expectNnz--; has[r][c] = false; }
            else { int v = rnd(1000) - 500; s.set(r, c, v); if (!has[r][c]) expectNnz++; has[r][c] = true; dense[r][c] = v; }
        }
        CHECK(eraseOk && s.nnz() == expectNnz);
        bool same = true, sorted = true, rowCounts = true;
        for (int r = 0; r < R; r++) {
            int cnt = 0;
            for (int c = 0; c < C; c++) {
                if (s.contains(r, c) != has[r][c]) same = false;
                if (has[r][c] && s.get(r, c) != dense[r][c]) same = false;
                if (has[r][c]) cnt++;
            }
            if (cnt != s.rowNnz(r)) rowCounts = false;
            int last = -1;
            s.forEachInRow(r, [&](int c, int&) { if (c <= last) sorted = false; last = c; });
        }
        CHECK(same && sorted && rowCounts);
        // argMin agrees with a scan
        bool minOk = true;
        for (int r = 0; r < R; r++) {
            int best = -1;
            for (int c = 0; c < C; c++) if (has[r][c] && (best < 0 || dense[r][c] < dense[r][best])) best = c;
            if (s.argMinInRow(r) != best) minOk = false;
        }
        CHECK(minOk);
        // transpose
        SparseMatrix<int> t = s.transpose();
        CHECK(t.rows() == C && t.cols() == R && t.nnz() == s.nnz());
        bool tOk = true;
        for (int r = 0; r < R; r++) for (int c = 0; c < C; c++)
            if (t.contains(c, r) != has[r][c] || (has[r][c] && t.get(c, r) != dense[r][c])) tOk = false;
        for (int c = 0; c < C; c++) {
            int last = -1;
            t.forEachInRow(c, [&](int r, int&) { if (r <= last) tOk = false; last = r; });
        }
        CHECK(tOk);
        // column walk agrees with dense
        bool colOk = true;
        for (int c = 0; c < C; c++) {
            int cnt = 0, want = 0;
            s.forEachInCol(c, [&](int r, int& v) { cnt++; if (!has[r][c] || v != dense[r][c]) colOk = false; });
            for (int r = 0; r < R; r++) if (has[r][c]) want++;
            if (cnt != want) colOk = false;
        }
        CHECK(colOk);
        int visited = 0;
        s.forEach([&](int, int, int&) { visited++; });
        CHECK(visited == s.nnz());
        s.clear();
        CHECK(s.nnz() == 0 && !s.contains(0, 0) && s.rowNnz(0) == 0);
        s.set(0, 0, 5);
        CHECK(s.get(0, 0) == 5 && s.nnz() == 1);
    }
    { // empty matrix edge case
        SparseMatrix<int> z(0, 0);
        CHECK(z.nnz() == 0 && z.density() == 0);
        SparseMatrix<int> zt = z.transpose();
        CHECK(zt.rows() == 0);
    }
    TEST_END();
}
