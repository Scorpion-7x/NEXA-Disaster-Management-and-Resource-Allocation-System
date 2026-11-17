#pragma once
#include <string>
#include "csv.h"
#include "../model/incident.h"
#include "../model/resource.h"
#include "../model/hospital.h"

namespace nexa {

// Malformed rows (too few columns / bad numbers) are skipped.
// load* appends to `out` and returns false only if the file can't be opened.

inline bool loadIncidents(const std::string& path, DynamicArray<Incident>& out) {
    CsvTable rows;
    if (!readCsv(path, rows)) return false;
    for (int i = 0; i < rows.size(); i++) {
        const CsvRow& r = rows[i];
        if (r.size() < 8) continue;
        try {
            Incident x;
            x.id = std::stoi(r[0]); x.type = r[1]; x.severity = std::stoi(r[2]);
            x.lat = std::stod(r[3]); x.lon = std::stod(r[4]);
            x.reportedAt = std::stoll(r[5]); x.status = r[6]; x.description = r[7];
            out.push_back(x);
        } catch (...) {}
    }
    return true;
}

inline bool saveIncidents(const std::string& path, const DynamicArray<Incident>& v) {
    CsvTable rows;
    for (const Incident& x : v) {
        CsvRow r;
        r.push_back(std::to_string(x.id)); r.push_back(x.type); r.push_back(std::to_string(x.severity));
        r.push_back(std::to_string(x.lat)); r.push_back(std::to_string(x.lon));
        r.push_back(std::to_string(x.reportedAt)); r.push_back(x.status); r.push_back(x.description);
        rows.push_back(r);
    }
    return writeCsv(path, "id,type,severity,lat,lon,reported_at,status,description", rows);
}

inline bool loadResources(const std::string& path, DynamicArray<Resource>& out) {
    CsvTable rows;
    if (!readCsv(path, rows)) return false;
    for (int i = 0; i < rows.size(); i++) {
        const CsvRow& r = rows[i];
        if (r.size() < 9) continue;
        try {
            Resource x;
            x.id = std::stoi(r[0]); x.name = r[1]; x.type = r[2]; x.capability = r[3];
            x.lat = std::stod(r[4]); x.lon = std::stod(r[5]); x.status = r[6];
            x.availFrom = std::stoll(r[7]); x.availTo = std::stoll(r[8]);
            out.push_back(x);
        } catch (...) {}
    }
    return true;
}

inline bool saveResources(const std::string& path, const DynamicArray<Resource>& v) {
    CsvTable rows;
    for (const Resource& x : v) {
        CsvRow r;
        r.push_back(std::to_string(x.id)); r.push_back(x.name); r.push_back(x.type); r.push_back(x.capability);
        r.push_back(std::to_string(x.lat)); r.push_back(std::to_string(x.lon)); r.push_back(x.status);
        r.push_back(std::to_string(x.availFrom)); r.push_back(std::to_string(x.availTo));
        rows.push_back(r);
    }
    return writeCsv(path, "id,name,type,capability,lat,lon,status,avail_from,avail_to", rows);
}

inline bool loadHospitals(const std::string& path, DynamicArray<Hospital>& out) {
    CsvTable rows;
    if (!readCsv(path, rows)) return false;
    for (int i = 0; i < rows.size(); i++) {
        const CsvRow& r = rows[i];
        if (r.size() < 6) continue;
        try {
            Hospital x;
            x.id = std::stoi(r[0]); x.name = r[1];
            x.lat = std::stod(r[2]); x.lon = std::stod(r[3]);
            x.totalBeds = std::stoi(r[4]); x.freeBeds = std::stoi(r[5]);
            out.push_back(x);
        } catch (...) {}
    }
    return true;
}

inline bool saveHospitals(const std::string& path, const DynamicArray<Hospital>& v) {
    CsvTable rows;
    for (const Hospital& x : v) {
        CsvRow r;
        r.push_back(std::to_string(x.id)); r.push_back(x.name);
        r.push_back(std::to_string(x.lat)); r.push_back(std::to_string(x.lon));
        r.push_back(std::to_string(x.totalBeds)); r.push_back(std::to_string(x.freeBeds));
        rows.push_back(r);
    }
    return writeCsv(path, "id,name,lat,lon,total_beds,free_beds", rows);
}

}  // namespace nexa
