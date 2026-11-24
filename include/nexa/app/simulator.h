#pragma once
#include <random>
#include <string>
#include "nexa/app/nexa_system.h"

namespace nexa {

// Discrete-event simulation on top of NexaSystem.
//
// Time jumps from one event to the next (no ticking minute by minute). Events live in a
// priority queue ordered by time; processing an event can schedule new ones:
//
//   ARRIVAL   a new random incident is reported           -> schedules the next ARRIVAL
//   ON_SCENE  the unit reaches the incident               -> schedules CLEARED (after on-scene work)
//   CLEARED   work done, incident RESOLVED                -> schedules BACK (drive to hospital if a
//                                                           casualty needed one, then back to base)
//   BACK      unit is at base and AVAILABLE again
//
// After every event, every OPEN incident that now has a free unit is dispatched.
// Each run rebuilds its events from the system's current state, so runs can be repeated,
// mixed with manual menu actions, or continued after save/load.
struct SimConfig {
    int hours = 8;                  // how long to simulate
    double arrivalsPerHour = 4;     // new incidents per hour (0 = no new incidents, just let time pass)
    unsigned seed = 2026;           // same seed -> same run
    double speedKmh = 30;           // average city driving speed
    int turnoutMin = 2;             // time from dispatch to wheels rolling
    int handoverMin = 10;           // time at the hospital when a casualty is taken there
};

struct SimReport {
    long long start = 0, end = 0;
    int arrivals = 0;               // incidents generated during the run
    int dispatched = 0;             // dispatches made during the run (any incident)
    int resolved = 0;               // incidents cleared during the run
    int stillOpen = 0;              // OPEN at the end
    int peakOpen = 0;               // most incidents waiting for a unit at one time
    int answered = 0;               // new incidents whose unit reached the scene
    double avgResponse = 0, medianResponse = 0, p90Response = 0, maxResponse = 0;   // minutes, report -> on scene
    double avgWait = 0;             // minutes, report -> dispatch (time spent waiting for a free unit)
    double utilization = 0;         // fraction of unit-time spent busy
    // per unit type: AMBULANCE, FIRE_TRUCK, RESCUE_TEAM, BOAT
    int typeAnswered[4] = {0, 0, 0, 0};
    int typeOpen[4] = {0, 0, 0, 0};
    double typeAvgResponse[4] = {0, 0, 0, 0};
};

class Simulator {
public:
    static constexpr int TYPE_COUNT = 4;
    static const char* typeName(int t) {
        static const char* names[TYPE_COUNT] = {"AMBULANCE", "FIRE_TRUCK", "RESCUE_TEAM", "BOAT"};
        return names[t];
    }

private:
    enum Kind { ARRIVAL, ON_SCENE, CLEARED, BACK };
    struct Event {
        long long t = 0;
        long long seq = 0;          // tie-break: events at the same minute run in scheduling order
        Kind kind = ARRIVAL;
        int incident = -1, resource = -1;
    };
    struct EarlierFirst {
        bool operator()(const Event& a, const Event& b) const { return a.t != b.t ? a.t < b.t : a.seq < b.seq; }
    };

    NexaSystem& sys_;
    SimConfig cfg_;
    std::mt19937 rng_;
    PriorityQueue<Event, EarlierFirst> events_;
    long long seq_ = 0;

    // per-run bookkeeping
    DynamicArray<long long> dispatchedAt_;   // by incident index, -1 if not dispatched in this run
    DynamicArray<bool> newThisRun_;          // incident reported during this run
    DynamicArray<long long> busySince_;      // by resource index, -1 if free
    double busyMinutes_ = 0;
    SimReport rep_;
    DynamicArray<double> responses_;
    double waitSum_ = 0;
    int waitCount_ = 0;
    double typeRespSum_[TYPE_COUNT] = {0, 0, 0, 0};

    void schedule(long long t, Kind k, int i = -1, int r = -1) { events_.push(Event{t, seq_++, k, i, r}); }

    long long travelMin(double km) const {
        long long m = (long long)(km / cfg_.speedKmh * 60.0 + 0.5);
        return m < 1 ? 1 : m;
    }
    static long long workMin(int severity) { return 15 + 5LL * severity; }   // 20 min (sev 1) .. 65 min (sev 10)

    static int typeIndex(const std::string& unitType) {
        for (int t = 0; t < TYPE_COUNT; t++) if (unitType == typeName(t)) return t;
        return 0;
    }

    void growTables() {
        while (dispatchedAt_.size() < sys_.incidents.size()) { dispatchedAt_.push_back(-1); newThisRun_.push_back(false); }
    }

    void markBusy(int r, long long t) { if (busySince_[r] < 0) busySince_[r] = t; }
    void markFree(int r, long long t) {
        if (busySince_[r] >= 0) { busyMinutes_ += (double)(t - busySince_[r]); busySince_[r] = -1; }
    }

    // Random incident: type by realistic share, mostly low severity, location inside the city's data bounds.
    void generateIncident(long long t, double latLo, double latHi, double lonLo, double lonHi) {
        static const char* types[6] = {"MEDICAL", "ACCIDENT", "FIRE", "FLOOD", "COLLAPSE", "EARTHQUAKE"};
        static const int weights[6] = {30, 25, 20, 10, 10, 5};
        int pick = std::uniform_int_distribution<int>(0, 99)(rng_), ti = 0;
        while (pick >= weights[ti]) { pick -= weights[ti]; ti++; }
        double u = std::uniform_real_distribution<double>(0, 1)(rng_);
        int sev = 1 + (int)(9.999 * u * u);                                   // skewed towards minor incidents
        double lat = std::uniform_real_distribution<double>(latLo, latHi)(rng_);
        double lon = std::uniform_real_distribution<double>(lonLo, lonHi)(rng_);
        int i = sys_.reportIncident(types[ti], sev, lat, lon, std::string("Simulated ") + types[ti], t);
        growTables();
        newThisRun_[i] = true;
        rep_.arrivals++;
    }

    // Dispatch everything that can be dispatched right now.
    void dispatchAll(long long t) {
        for (;;) {
            DispatchResult d = sys_.dispatchNext();
            if (d.incident < 0 || d.resource < 0) break;
            growTables();
            rep_.dispatched++;
            dispatchedAt_[d.incident] = t;
            markBusy(d.resource, t);
            const Incident& x = sys_.incidents[d.incident];
            if (newThisRun_[d.incident]) { waitSum_ += (double)(t - x.reportedAt); waitCount_++; }
            schedule(t + cfg_.turnoutMin + travelMin(d.km), ON_SCENE, d.incident, d.resource);
        }
        int open = 0;
        for (const Incident& x : sys_.incidents) if (x.status == "OPEN") open++;
        if (open > rep_.peakOpen) rep_.peakOpen = open;
    }

    // Recreates pending events for jobs that were already under way when this run started.
    void resumeJobs(long long now) {
        MultiLinkedList<Deployment>& dep = sys_.deployments();
        std::uniform_int_distribution<int> externalReturn(30, 90);
        for (int r = 0; r < sys_.resources.size(); r++) {
            if (sys_.resources[r].status != "BUSY") continue;
            markBusy(r, now);
            MultiLinkedList<Deployment>::Node* last = dep.colLast(r);
            bool active = last && sys_.incidents[last->row].status == "DISPATCHED";
            if (!active) {
                // busy on a job NEXA has no record of (e.g. BUSY in the input data): assume it returns soon
                schedule(now + externalReturn(rng_), BACK, -1, r);
                continue;
            }
            const Deployment& d = last->val;
            long long arrive = d.at + cfg_.turnoutMin + travelMin(d.km);
            long long cleared = arrive + workMin(sys_.incidents[last->row].severity);
            if (now < arrive) schedule(arrive, ON_SCENE, last->row, r);
            else if (now < cleared) schedule(cleared, CLEARED, last->row, r);
            else schedule(now, CLEARED, last->row, r);
        }
    }

    long long returnMin(int incident, int r) {
        MultiLinkedList<Deployment>::Node* n = sys_.deployments().find(incident, r);
        if (!n) return 30;
        const Deployment& d = n->val;
        if (d.hospital >= 0)   // to the hospital, hand over, then roughly back the way it came
            return travelMin(d.hospitalKm) + cfg_.handoverMin + travelMin(d.hospitalKm + d.km);
        return travelMin(d.km);
    }

    static double percentile(const DynamicArray<double>& sorted, double p) {
        if (sorted.size() == 0) return 0;
        int idx = (int)(p * (sorted.size() - 1) + 0.5);
        return sorted[idx];
    }

public:
    explicit Simulator(NexaSystem& sys) : sys_(sys) {}

    SimReport run(const SimConfig& cfg) {
        cfg_ = cfg;
        rng_.seed(cfg.seed);
        while (!events_.empty()) events_.pop();
        seq_ = 0;
        rep_ = SimReport();
        responses_.clear();
        waitSum_ = 0; waitCount_ = 0; busyMinutes_ = 0;
        for (int t = 0; t < TYPE_COUNT; t++) typeRespSum_[t] = 0;
        dispatchedAt_.clear(); newThisRun_.clear(); busySince_.clear();
        growTables();
        for (int r = 0; r < sys_.resources.size(); r++) busySince_.push_back(-1);

        const long long start = sys_.now(), end = start + 60LL * cfg.hours;
        rep_.start = start; rep_.end = end;
        sys_.clearUndo();
        sys_.logInfo("Simulation started at t=" + std::to_string(start) + " for " + std::to_string(cfg.hours) +
                     " h, " + std::to_string(cfg.arrivalsPerHour) + " incidents/h, seed " + std::to_string(cfg.seed));

        // city bounds from the data, for placing new incidents
        double latLo = 90, latHi = -90, lonLo = 180, lonHi = -180;
        for (const Incident& x : sys_.incidents) {
            if (x.lat < latLo) latLo = x.lat;
            if (x.lat > latHi) latHi = x.lat;
            if (x.lon < lonLo) lonLo = x.lon;
            if (x.lon > lonHi) lonHi = x.lon;
        }
        std::exponential_distribution<double> gap(cfg.arrivalsPerHour > 0 ? cfg.arrivalsPerHour / 60.0 : 1.0);
        auto nextArrival = [&](long long from) { return from + 1 + (long long)gap(rng_); };   // Poisson process

        resumeJobs(start);
        if (cfg.arrivalsPerHour > 0) schedule(nextArrival(start), ARRIVAL);
        dispatchAll(start);

        while (!events_.empty() && events_.top().t <= end) {
            Event e = events_.pop();
            sys_.advanceClock(e.t);
            if (e.kind == ARRIVAL) {
                generateIncident(e.t, latLo, latHi, lonLo, lonHi);
                schedule(nextArrival(e.t), ARRIVAL);
            } else if (e.kind == ON_SCENE) {
                const Incident& x = sys_.incidents[e.incident];
                if (newThisRun_[e.incident]) {
                    double resp = (double)(e.t - x.reportedAt);
                    responses_.push_back(resp);
                    int ti = typeIndex(sys_.resources[e.resource].type);
                    rep_.typeAnswered[ti]++;
                    typeRespSum_[ti] += resp;
                }
                schedule(e.t + workMin(x.severity), CLEARED, e.incident, e.resource);
            } else if (e.kind == CLEARED) {
                if (sys_.incidents[e.incident].status == "DISPATCHED") rep_.resolved++;
                sys_.markResolved(e.incident);
                schedule(e.t + returnMin(e.incident, e.resource), BACK, e.incident, e.resource);
            } else {   // BACK
                sys_.releaseUnit(e.resource);
                markFree(e.resource, e.t);
            }
            dispatchAll(e.t);
        }
        sys_.advanceClock(end);
        sys_.clearUndo();   // dispatches made by the simulation are not user actions to undo

        // ---- report ----
        for (int r = 0; r < sys_.resources.size(); r++) markFree(r, end);
        double unitMinutes = (double)sys_.resources.size() * (double)(end - start);
        rep_.utilization = unitMinutes > 0 ? busyMinutes_ / unitMinutes : 0;
        for (int i = 0; i < sys_.incidents.size(); i++) {
            const Incident& x = sys_.incidents[i];
            if (x.status != "OPEN") continue;
            rep_.stillOpen++;
            rep_.typeOpen[typeIndex(NexaSystem::unitFor(x.type))]++;
        }
        rep_.answered = responses_.size();
        if (rep_.answered > 0) {
            double sum = 0;
            for (double v : responses_) sum += v;
            mergeSort(responses_, [](double a, double b) { return a < b; });
            rep_.avgResponse = sum / rep_.answered;
            rep_.medianResponse = percentile(responses_, 0.5);
            rep_.p90Response = percentile(responses_, 0.9);
            rep_.maxResponse = responses_[responses_.size() - 1];
        }
        rep_.avgWait = waitCount_ ? waitSum_ / waitCount_ : 0;
        for (int t = 0; t < TYPE_COUNT; t++)
            rep_.typeAvgResponse[t] = rep_.typeAnswered[t] ? typeRespSum_[t] / rep_.typeAnswered[t] : 0;

        sys_.logInfo("Simulation ended at t=" + std::to_string(end) + ": " + std::to_string(rep_.arrivals) +
                     " new incidents, " + std::to_string(rep_.resolved) + " resolved, " +
                     std::to_string(rep_.stillOpen) + " still open");
        return rep_;
    }
};

}  // namespace nexa
