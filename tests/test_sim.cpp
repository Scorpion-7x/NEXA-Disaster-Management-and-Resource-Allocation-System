#include <string>
#include "test_util.h"
#include "nexa/app/simulator.h"
using namespace nexa;

static std::string dataDir() {
    DynamicArray<Incident> probe;
    return loadIncidents("../data/incidents.csv", probe) ? "../data" : "data";
}
static int count(const NexaSystem& s, const std::string& st) {
    int n = 0;
    for (const Incident& x : s.incidents) if (x.status == st) n++;
    return n;
}
static int busy(const NexaSystem& s) {
    int n = 0;
    for (const Resource& r : s.resources) if (r.status == "BUSY") n++;
    return n;
}
static bool bedsOk(const NexaSystem& s) {
    for (const Hospital& h : s.hospitals) if (h.freeBeds < 0 || h.freeBeds > h.totalBeds) return false;
    return true;
}
// Every BUSY unit is on at most one live job, and no incident has two live units.
// (A diverted deployment stays in the history but is no longer live.)
static bool noDoubleBooking(NexaSystem& s) {
    for (int r = 0; r < s.resources.size(); r++) {
        int active = 0;
        s.deployments().forEachInCol(r, [&](MultiLinkedList<Deployment>::Node& d) {
            if (!d.val.diverted && s.incidents[d.row].status == "DISPATCHED") active++;
        });
        if (active > 1) return false;
        if (active == 1 && s.resources[r].status != "BUSY") return false;
    }
    for (int i = 0; i < s.incidents.size(); i++) {
        int live = 0;
        s.deployments().forEachInRow(i, [&](MultiLinkedList<Deployment>::Node& d) { if (!d.val.diverted) live++; });
        if (live > 1) return false;
    }
    return true;
}

int main() {
    std::string err;
    { // no arrivals, long enough: the whole backlog is cleared and every unit comes home
        NexaSystem s("test_tmp/sim.log");
        CHECK(s.load(dataDir(), err));
        long long t0 = s.now();
        Simulator sim(s);
        SimConfig cfg; cfg.hours = 72; cfg.arrivalsPerHour = 0;
        SimReport r = sim.run(cfg);
        CHECK(r.start == t0 && r.end == t0 + 72 * 60 && s.now() == r.end);
        CHECK(r.arrivals == 0 && r.answered == 0);
        CHECK(count(s, "OPEN") == 0 && count(s, "DISPATCHED") == 0 && count(s, "RESOLVED") == 50);
        CHECK(r.resolved == 50 && r.dispatched == 50 && r.stillOpen == 0);
        CHECK(busy(s) == 0);                                   // includes the 7 units BUSY in the input data
        CHECK(s.deployments().size() == 50 && s.undoDepth() == 0);
        CHECK(r.utilization > 0 && r.utilization < 1);
        CHECK(bedsOk(s));
    }
    { // with arrivals: invariants hold, numbers are sane
        NexaSystem s("test_tmp/sim.log");
        CHECK(s.load(dataDir(), err));
        Simulator sim(s);
        SimConfig cfg; cfg.hours = 12; cfg.arrivalsPerHour = 5; cfg.seed = 7;
        SimReport r = sim.run(cfg);
        CHECK(r.arrivals > 20 && r.arrivals < 120);            // ~60 expected
        CHECK(s.incidents.size() == 50 + r.arrivals);
        CHECK(r.answered > 0 && r.answered <= r.arrivals);
        CHECK(r.medianResponse >= cfg.turnoutMin + 1);         // can't beat turnout + one minute of driving
        CHECK(r.avgResponse > 0 && r.medianResponse <= r.p90Response && r.p90Response <= r.maxResponse);
        CHECK(r.stillOpen == count(s, "OPEN"));
        int typeSum = 0;
        for (int t = 0; t < Simulator::TYPE_COUNT; t++) typeSum += r.typeAnswered[t];
        CHECK(typeSum == r.answered);
        // severity bands cover every new incident exactly once
        int bandTotal = 0, bandReached = 0;
        for (int b = 0; b < 3; b++) {
            bandTotal += r.sevTotal[b]; bandReached += r.sevReached[b];
            CHECK(r.sevReached[b] <= r.sevTotal[b]);
        }
        CHECK(bandTotal == r.arrivals && bandReached == r.answered);
        // "all" includes the ones still waiting, so it has at least as many cases and a worst case at least as bad
        CHECK(r.maxAll >= r.maxResponse && r.medianAll <= r.p90All && r.p90All <= r.maxAll);
        CHECK(noDoubleBooking(s) && bedsOk(s));
        // new incidents carry the simulation's timestamps
        bool times = true;
        for (int i = 50; i < s.incidents.size(); i++)
            if (s.incidents[i].reportedAt < r.start || s.incidents[i].reportedAt > r.end) times = false;
        CHECK(times);
        // every remaining OPEN incident has no free unit of its type (nothing is left waiting needlessly)
        bool stuck = true;
        for (const Incident& x : s.incidents) {
            if (x.status != "OPEN") continue;
            for (const Resource& u : s.resources)
                if (u.type == NexaSystem::unitFor(x.type) && u.status == "AVAILABLE") stuck = false;
        }
        CHECK(stuck);

        // continue the same system: jobs under way are picked up, then everything drains
        SimConfig drain; drain.hours = 96; drain.arrivalsPerHour = 0;
        SimReport r2 = sim.run(drain);
        CHECK(r2.start == r.end);
        CHECK(count(s, "OPEN") == 0 && count(s, "DISPATCHED") == 0 && busy(s) == 0);
        CHECK(noDoubleBooking(s) && bedsOk(s));
    }
    { // light load: everyone gets reached, so "all" and "reached only" must agree exactly
        NexaSystem s("test_tmp/sim.log");
        CHECK(s.load(dataDir(), err));
        Simulator sim(s);
        SimConfig warm; warm.hours = 72; warm.arrivalsPerHour = 0;   // clear the backlog first
        sim.run(warm);
        SimConfig cfg; cfg.hours = 24; cfg.arrivalsPerHour = 0.5; cfg.seed = 5;
        SimReport r = sim.run(cfg);
        CHECK(r.arrivals > 0);
        if (r.answered == r.arrivals) {
            CHECK(r.avgAll == r.avgResponse && r.medianAll == r.medianResponse && r.maxAll == r.maxResponse);
        } else {
            CHECK(r.stillOpen > 0 || r.arrivals - r.answered <= 2);    // only the last few may be en route
        }
    }
    { // same seed -> identical run; different seed -> different run
        SimReport a, b, c;
        for (int k = 0; k < 3; k++) {
            NexaSystem s("test_tmp/sim.log");
            CHECK(s.load(dataDir(), err));
            Simulator sim(s);
            SimConfig cfg; cfg.hours = 10; cfg.arrivalsPerHour = 6; cfg.seed = k < 2 ? 11 : 12;
            SimReport r = sim.run(cfg);
            if (k == 0) a = r; else if (k == 1) b = r; else c = r;
        }
        CHECK(a.arrivals == b.arrivals && a.resolved == b.resolved && a.avgAll == b.avgAll && a.avgResponse == b.avgResponse && a.peakOpen == b.peakOpen);
        CHECK(a.arrivals != c.arrivals || a.avgResponse != c.avgResponse);
    }
    { // a manual dispatch before the run is carried through by the simulation
        NexaSystem s("test_tmp/sim.log");
        CHECK(s.load(dataDir(), err));
        DispatchResult d = s.dispatchNext();
        CHECK(d.resource >= 0);
        Simulator sim(s);
        SimConfig cfg; cfg.hours = 24; cfg.arrivalsPerHour = 0;
        sim.run(cfg);
        CHECK(s.incidents[d.incident].status == "RESOLVED" && s.resources[d.resource].status == "AVAILABLE");
    }
    { // save mid-simulation, load, continue: still drains cleanly
        NexaSystem s("test_tmp/sim.log");
        CHECK(s.load(dataDir(), err));
        Simulator sim(s);
        SimConfig cfg; cfg.hours = 3; cfg.arrivalsPerHour = 8; cfg.seed = 3;
        sim.run(cfg);
        CHECK(count(s, "DISPATCHED") > 0);                     // jobs are mid-flight
        CHECK(s.save("test_tmp/sim_state"));
        NexaSystem t("test_tmp/sim.log");
        CHECK(t.load("test_tmp/sim_state", err));
        CHECK(t.now() == s.now());                             // clock restored from meta.csv
        Simulator sim2(t);
        SimConfig drain; drain.hours = 96; drain.arrivalsPerHour = 0;
        sim2.run(drain);
        CHECK(count(t, "OPEN") == 0 && count(t, "DISPATCHED") == 0 && busy(t) == 0);
        CHECK(noDoubleBooking(t) && bedsOk(t));
    }
    { // pre-emption: only minor jobs lose their unit, only to critical incidents, and everything still drains
        int totalPreempt = 0;
        for (unsigned seed = 1; seed <= 6; seed++) {
            NexaSystem s("test_tmp/sim.log");
            CHECK(s.load(dataDir(), err));
            Simulator sim(s);
            SimConfig cfg; cfg.hours = 12; cfg.arrivalsPerHour = 6; cfg.seed = seed; cfg.preempt = true;
            SimReport r = sim.run(cfg);
            totalPreempt += r.preemptions;
            CHECK(noDoubleBooking(s) && bedsOk(s));
            int diverted = 0;
            bool rules = true;
            for (int u = 0; u < s.resources.size(); u++) {
                for (auto* n = s.deployments().colFirst(u); n; n = n->nextInCol) {
                    if (!n->val.diverted) continue;
                    diverted++;
                    if (s.incidents[n->row].severity > cfg.preemptMaxVictim) rules = false;   // victim was minor
                    if (!n->nextInCol || s.incidents[n->nextInCol->row].severity < cfg.preemptMinSeverity) rules = false;
                }
            }
            CHECK(rules);
            CHECK(diverted == r.preemptions);
            SimConfig drain; drain.hours = 120; drain.arrivalsPerHour = 0; drain.preempt = true;
            sim.run(drain);
            CHECK(count(s, "OPEN") == 0 && count(s, "DISPATCHED") == 0 && busy(s) == 0);   // bumped incidents got served
            CHECK(noDoubleBooking(s) && bedsOk(s));
        }
        CHECK(totalPreempt > 0);                                 // the scenario really exercises pre-emption
    }
    { // ageing never lets a non-critical incident be served while a critical one could be
        NexaSystem s("test_tmp/sim.log");
        CHECK(s.load(dataDir(), err));
        DispatchPolicy p; p.aging = true;
        s.setPolicy(p);
        s.advanceClock(s.now() + 600);                           // everything has waited 10 hours: maximum boost
        bool criticalFirst = true;
        for (;;) {
            DispatchResult d = s.dispatchNext();
            if (d.incident < 0) break;
            if (s.incidents[d.incident].severity >= p.criticalSeverity) continue;
            for (const Incident& x : s.incidents) {
                if (x.status != "OPEN" || x.severity < p.criticalSeverity) continue;
                for (const Resource& u : s.resources)
                    if (u.type == NexaSystem::unitFor(x.type) && u.status == "AVAILABLE") criticalFirst = false;
            }
        }
        CHECK(criticalFirst);
        // Among non-critical incidents ageing does change the order: an old minor fire beats a fresh,
        // slightly more serious one. Start from an empty backlog so only these two compete.
        auto clearBacklog = [&](NexaSystem& sys) {
            for (int round = 0; round < 20 && count(sys, "OPEN") > 0; round++) {
                while (sys.dispatchNext().incident >= 0) {}
                for (const Incident& x : sys.incidents) if (x.status == "DISPATCHED") sys.resolveIncident(x.id, err);
            }
        };
        NexaSystem a("test_tmp/sim.log"), b("test_tmp/sim.log");
        CHECK(a.load(dataDir(), err) && b.load(dataDir(), err));
        clearBacklog(a); clearBacklog(b);
        CHECK(count(a, "OPEN") == 0 && count(b, "OPEN") == 0);
        a.setPolicy(p);
        for (NexaSystem* sys : {&a, &b}) {
            long long t = sys->now() + 1;
            sys->reportIncident("FIRE", 3, 28.57, 77.36, "old minor fire", t);
            sys->reportIncident("FIRE", 5, 28.58, 77.37, "new serious fire", t + 120);   // 2 hours later
        }
        DispatchResult da = a.dispatchNext(), db = b.dispatchNext();
        CHECK(da.resource >= 0 && db.resource >= 0);
        CHECK(a.incidents[da.incident].severity == 3);           // ageing: 3 + 3 (waited 120 min) = 6 > 5
        CHECK(b.incidents[db.incident].severity == 5);           // basic: severity first
    }
    { // ageing in a full simulation keeps every invariant
        NexaSystem s("test_tmp/sim.log");
        CHECK(s.load(dataDir(), err));
        Simulator sim(s);
        SimConfig cfg; cfg.hours = 12; cfg.arrivalsPerHour = 6; cfg.seed = 4; cfg.policy.aging = true; cfg.preempt = true;
        sim.run(cfg);
        CHECK(noDoubleBooking(s) && bedsOk(s));
        CHECK(!s.policy().aging);                                // manual dispatch goes back to the default policy
    }
    TEST_END();
}
