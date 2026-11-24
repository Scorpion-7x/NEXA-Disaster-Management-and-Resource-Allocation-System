#include <string>
#include "test_util.h"
#include "nexa/app/nexa_system.h"
using namespace nexa;

// ctest runs from build/, a manual run usually from the repo root
static std::string dataDir() {
    DynamicArray<Incident> probe;
    return loadIncidents("../data/incidents.csv", probe) ? "../data" : "data";
}

static int countStatus(const NexaSystem& s, const std::string& st) {
    int n = 0;
    for (const Incident& x : s.incidents) if (x.status == st) n++;
    return n;
}
static int availableUnits(const NexaSystem& s) {
    int n = 0;
    for (const Resource& r : s.resources) if (r.status == "AVAILABLE") n++;
    return n;
}
static int freeBeds(const NexaSystem& s) {
    int n = 0;
    for (const Hospital& h : s.hospitals) n += h.freeBeds;
    return n;
}

int main() {
    std::string err;
    { // loading
        NexaSystem s("test_tmp/system.log");
        CHECK(!s.load("no_such_folder", err) && !err.empty());
    }
    NexaSystem s("test_tmp/system.log");
    CHECK(s.load(dataDir(), err));
    CHECK(s.incidents.size() == 50 && s.resources.size() == 30 && s.hospitals.size() == 15);
    const int open0 = countStatus(s, "OPEN"), avail0 = availableUnits(s), beds0 = freeBeds(s);
    const int nodes0 = s.roads().nodeCount();

    { // dispatchNext takes the most urgent incident that has a unit of its type
        DynamicArray<int> byPri = s.incidentsByPriority("OPEN");
        CHECK(byPri.size() == open0);
        bool ordered = true;
        for (int k = 1; k < byPri.size(); k++)
            if (IncidentHigherPriority()(s.incidents[byPri[k]], s.incidents[byPri[k - 1]])) ordered = false;
        CHECK(ordered);

        DispatchResult d = s.dispatchNext();
        CHECK(d.incident >= 0 && d.resource >= 0);
        CHECK(d.incident == byPri[d.waiting]);                          // first one that could be served
        const Incident& x = s.incidents[d.incident];
        CHECK(x.status == "DISPATCHED");
        CHECK(s.resources[d.resource].status == "BUSY");
        CHECK(s.resources[d.resource].type == NexaSystem::unitFor(x.type));
        CHECK(d.km > 0 && d.candidates >= 1 && d.candidates <= 3);
        CHECK(s.deployments().size() == 1 && s.deployments().find(d.incident, d.resource) != nullptr);
        CHECK(s.costMatrix().contains(d.incident, d.resource));
        CHECK(NexaSystem::needsHospital(x.type) ? (d.hospital >= 0 && freeBeds(s) == beds0 - 1) : freeBeds(s) == beds0);

        // the chosen unit is the road-cheapest of the candidates compared
        bool cheapest = true;
        const SparseMatrix<double>& cm = s.costMatrix();
        for (int r = 0; r < s.resources.size(); r++)
            if (cm.contains(d.incident, r) && cm.get(d.incident, r) < d.km) cheapest = false;
        CHECK(cheapest);

        // undo puts everything back
        std::string what = s.undo();
        CHECK(!what.empty());
        CHECK(countStatus(s, "OPEN") == open0 && availableUnits(s) == avail0 && freeBeds(s) == beds0);
        CHECK(s.deployments().size() == 0);
        CHECK(s.undo().empty());                                        // nothing left to undo

        // and the same incident comes out first again
        DispatchResult d2 = s.dispatchNext();
        CHECK(d2.incident == d.incident && d2.resource == d.resource);
    }
    { // dispatch until nothing more can be served: never double-books a unit
        int served = 1;
        for (;;) {
            DispatchResult d = s.dispatchNext();
            if (d.incident < 0) break;
            served++;
        }
        CHECK(served == avail0 || countStatus(s, "OPEN") > 0);
        CHECK(s.deployments().size() == served);
        bool oneEach = true;
        for (int r = 0; r < s.resources.size(); r++) if (s.deployments().colSize(r) > 1) oneEach = false;
        CHECK(oneEach);
        CHECK(countStatus(s, "DISPATCHED") == served);
        // every remaining OPEN incident really has no free unit of its type
        bool stuck = true;
        for (const Incident& x : s.incidents) {
            if (x.status != "OPEN") continue;
            for (const Resource& r : s.resources)
                if (r.type == NexaSystem::unitFor(x.type) && r.status == "AVAILABLE") stuck = false;
        }
        CHECK(stuck);
    }
    { // resolve frees units, undo of resolve re-books them
        DynamicArray<int> disp = s.incidentsByPriority("DISPATCHED");
        int id = s.incidents[disp[0]].id;
        int before = availableUnits(s);
        CHECK(s.resolveIncident(id, err) == 1);
        CHECK(s.incidents[disp[0]].status == "RESOLVED" && availableUnits(s) == before + 1);
        CHECK(s.resolveIncident(id, err) == -1 && !err.empty());      // can't resolve twice
        CHECK(s.resolveIncident(99999, err) == -1);
        s.undo();
        CHECK(s.incidents[disp[0]].status == "DISPATCHED" && availableUnits(s) == before);
        CHECK(s.resolveIncident(id, err) == 1);
    }
    { // a newly reported incident joins the road network and can be dispatched
        int i = s.reportIncident("FIRE", 10, 28.57, 77.36, "test fire");
        CHECK(s.incidents[i].status == "OPEN" && s.incidents[i].id == 51);
        CHECK(s.roads().nodeCount() == nodes0 + 1 && s.intakeSize() == 1);
        CHECK(s.incidentIndex(51) == i && s.costMatrix().rows() == 51);
        DynamicArray<int> inWindow = s.incidentsBetween(s.now(), s.now());
        CHECK(inWindow.size() == 1 && inWindow[0] == i);
        DynamicArray<int> path; double km = 0;
        int h = s.routeToHospital(i, path, km);
        CHECK(h >= 0 && path.size() >= 2 && km > 0);
        CHECK(s.nodeLabel(path[0]) == "Incident #51" && s.nodeLabel(path[path.size() - 1]) == s.hospitals[h].name);
        DispatchResult d = s.dispatchIncident(51, err);
        if (d.resource >= 0) CHECK(s.resources[d.resource].type == "FIRE_TRUCK");
        else CHECK(d.incident == i && s.incidents[i].status == "OPEN");
        if (d.resource >= 0) s.undo();                                 // undo the dispatch
        s.undo();                                                      // undo the report
        CHECK(s.incidents[i].status == "CANCELLED");
        CHECK(s.dispatchIncident(51, err).incident == -1 && !err.empty());
    }
    { // lookups
        CHECK(s.resourceByName(s.resources[5].name) == 5);
        CHECK(s.resourceByName("Nonexistent Unit") == -1);
        DynamicArray<int> m = s.resourcesMatching("ambul");
        int ambulances = 0;
        for (const Resource& r : s.resources) if (r.type == "AMBULANCE") ambulances++;
        CHECK(m.size() == ambulances);
        int ids[5];
        int n = s.nearestResources(28.57, 77.36, 5, "", false, ids);
        CHECK(n == 5);
        bool sortedByDist = true;
        for (int k = 1; k < n; k++) {
            auto d = [&](int r) { double a = s.resources[r].lat - 28.57, b = (s.resources[r].lon - 77.36) * 0.878; return a * a + b * b; };
            if (d(ids[k]) < d(ids[k - 1])) sortedByDist = false;
        }
        CHECK(sortedByDist);
    }
    { // save and load back, including deployments
        CHECK(s.save("test_tmp/saved_state"));
        NexaSystem t("test_tmp/system.log");
        CHECK(t.load("test_tmp/saved_state", err));
        CHECK(t.incidents.size() == s.incidents.size());
        CHECK(t.deployments().size() == s.deployments().size());
        CHECK(countStatus(t, "DISPATCHED") == countStatus(s, "DISPATCHED"));
        CHECK(countStatus(t, "CANCELLED") == 1 && availableUnits(t) == availableUnits(s) && freeBeds(t) == freeBeds(s));
        bool sameDeps = true;
        auto* a = s.deployments().first();
        auto* b = t.deployments().first();
        while (a && b) {
            if (s.incidents[a->row].id != t.incidents[b->row].id || s.resources[a->col].id != t.resources[b->col].id) sameDeps = false;
            a = a->nextAll; b = b->nextAll;
        }
        CHECK(sameDeps && !a && !b);
        // the next reported incident keeps counting ids from where we left off
        CHECK(t.incidents[t.reportIncident("MEDICAL", 5, 28.6, 77.4, "x")].id == 52);
    }
    TEST_END();
}
