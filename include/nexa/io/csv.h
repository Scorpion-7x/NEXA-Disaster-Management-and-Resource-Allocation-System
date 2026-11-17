#pragma once
#include <fstream>
#include <string>
#include "../ds/dynamic_array.h"

namespace nexa {

using CsvRow = DynamicArray<std::string>;
using CsvTable = DynamicArray<CsvRow>;

// Splits one CSV line. Supports "quoted, fields" and "" escapes.
inline CsvRow splitCsvLine(const std::string& line) {
    CsvRow out;
    std::string cur;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; i++; }
                else inQuotes = false;
            } else cur += c;
        } else if (c == '"') inQuotes = true;
        else if (c == ',') { out.push_back(cur); cur.clear(); }
        else if (c != '\r') cur += c;
    }
    out.push_back(cur);
    return out;
}

inline std::string escapeCsv(const std::string& s) {
    if (s.find_first_of(",\"\n") == std::string::npos) return s;
    std::string r = "\"";
    for (char c : s) { if (c == '"') r += "\"\""; else r += c; }
    return r + "\"";
}

// Reads all data rows (header line skipped, blank lines ignored). false if file can't be opened.
inline bool readCsv(const std::string& path, CsvTable& rows) {
    std::ifstream in(path);
    if (!in) return false;
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (first) { first = false; continue; }
        if (line.empty() || line == "\r") continue;
        rows.push_back(splitCsvLine(line));
    }
    return true;
}

inline bool writeCsv(const std::string& path, const std::string& header, const CsvTable& rows) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << header << "\n";
    for (int i = 0; i < rows.size(); i++) {
        const CsvRow& r = rows[i];
        for (int j = 0; j < r.size(); j++) {
            if (j) out << ',';
            out << escapeCsv(r[j]);
        }
        out << "\n";
    }
    return true;
}

}  // namespace nexa
