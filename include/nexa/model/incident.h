#pragma once
#include <string>

namespace nexa {

struct Incident {
    int id = 0;
    std::string type;          // FLOOD, FIRE, EARTHQUAKE, ACCIDENT, MEDICAL, COLLAPSE
    int severity = 1;          // 1..10 (bounded, so counting sort works later)
    double lat = 0, lon = 0;
    long long reportedAt = 0;  // epoch minutes
    std::string status = "OPEN";   // OPEN, DISPATCHED, RESOLVED
    std::string description;
};

// Higher severity first; ties broken by earlier report time.
struct IncidentHigherPriority {
    bool operator()(const Incident& a, const Incident& b) const {
        if (a.severity != b.severity) return a.severity > b.severity;
        return a.reportedAt < b.reportedAt;
    }
};

}  // namespace nexa
