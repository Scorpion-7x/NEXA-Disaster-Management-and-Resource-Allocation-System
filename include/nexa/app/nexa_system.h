#pragma once
#include <filesystem>
#include <string>
#include "nexa/ds/dynamic_array.h"
#include "nexa/ds/stack.h"
#include "nexa/ds/queue.h"
#include "nexa/ds/priority_queue.h"
#include "nexa/ds/hash_table.h"
#include "nexa/ds/probing_hash_table.h"
#include "nexa/ds/avl_tree.h"
#include "nexa/ds/kd_tree.h"
#include "nexa/ds/sparse_matrix.h"
#include "nexa/ds/multi_linked_list.h"
#include "nexa/algo/sort.h"
#include "nexa/algo/search.h"
#include "nexa/algo/graph_algo.h"
#include "nexa/map/city_map.h"
#include "nexa/io/store.h"
#include "nexa/io/logger.h"

namespace nexa {

// One deployment = one unit sent to one incident (plus the hospital a bed was reserved in).
struct Deployment {
    double km = 0;          // road distance unit -> incident
    long long at = 0;       // dispatch time (epoch minutes)
    int hospital = -1;      // hospital index, -1 if no bed was needed
    double hospitalKm = 0;  // road distance incident -> hospital
    bool diverted = false;  // the unit was taken away for a more critical incident before arriving
};

struct DispatchResult {
    int incident = -1;      // incident index, -1 if there was nothing open to dispatch
    int resource = -1;      // resource index, -1 if no suitable unit was available
    int hospital = -1;      // hospital index, -1 if not needed / none with free beds
    double km = 0, hospitalKm = 0;
    int candidates = 0;     // how many KD-tree candidates were compared by road
    int waiting = 0;        // more urgent incidents skipped because no unit of their type was free
    int bumped = -1;        // pre-emption only: incident index that lost its unit
    std::string neededType;
};

// How dispatchNext() chooses which OPEN incident to serve next.
struct DispatchPolicy {
    // Ageing: effective priority = severity + (minutes waited / agingMinutes), boost capped at maxBoost,
    // and a non-critical incident never rises above a critical one (it stays below criticalSeverity).
    // So ageing only reorders the non-critical incidents among themselves: long waiters move up,
    // but critical incidents are always served first.
    // Off: strictly severity first, then earliest report (the original behaviour).
    bool aging = false;
    double agingMinutes = 30;   // +1 priority level per 30 minutes of waiting
    double maxBoost = 3;        // a severity-3 incident can rise to at most 6
    int criticalSeverity = 8;   // 8-10 are critical and are never overtaken
};

// The whole NEXA workflow in one place. Every data structure has a job here:
//
//   DynamicArray      incidents, resources, hospitals (the records themselves)
//   Queue             intake: newly reported incidents wait here until triage
//   PriorityQueue     open incidents, most urgent first (lazy deletion of stale entries)
//   HashTable         incident id -> index, resource id -> index        (chaining)
//   ProbingHashTable  resource name -> index                             (open addressing)
//   AvlTree           report time -> incidents, for time-window queries
//   KDTree            nearest units to an incident by straight-line distance
//   Graph + Dijkstra  real road distances and routes (CityMap)
//   SparseMatrix      incident x resource road-distance table, only for compared candidates
//   MultiLinkedList   deployments, walkable per incident, per resource, or as full history
//   Stack             undo history of actions
//   merge sort / linear search / binary search  listings and lookups
class NexaSystem {
public:
    DynamicArray<Incident> incidents;
    DynamicArray<Resource> resources;
    DynamicArray<Hospital> hospitals;

private:
    struct Urgency { int sev; long long at; int idx; };
    struct MoreUrgent {
        bool operator()(const Urgency& a, const Urgency& b) const {
            if (a.sev != b.sev) return a.sev > b.sev;
            if (a.at != b.at) return a.at < b.at;
            return a.idx < b.idx;
        }
    };
    enum ActionType { ACT_REPORT, ACT_DISPATCH, ACT_RESOLVE };
    struct Action {
        ActionType type = ACT_REPORT;
        int incident = -1, resource = -1, hospital = -1;
        MultiLinkedList<Deployment>::Node* dep = nullptr;
        DynamicArray<int> freed;       // units released by a resolve
    };

    static constexpr double LON_SCALE = 0.878;   // cos(28.57 deg), equalises lat/lon degrees near Noida
    static constexpr double ROAD_FACTOR = 1.3;   // same as buildCityMap
    static constexpr int KD_CANDIDATES = 3;

    Logger log_;
    CityMap city_;
    DynamicArray<int> incNode_;          // incident index -> graph node
    DynamicArray<int> nodeIncident_;     // graph node -> incident index, or -1
    DynamicArray<double> nodeLat_, nodeLon_;
    KDTree kd_;
    AvlTree<long long, DynamicArray<int>> timeline_;
    HashTable<int, int> incById_, resById_;
    ProbingHashTable<std::string, int> resByName_;
    PriorityQueue<Urgency, MoreUrgent> urgent_;
    Queue<int> intake_;
    SparseMatrix<double> cost_;
    MultiLinkedList<Deployment> deployments_;
    Stack<Action> undo_;
    long long now_ = 0;
    int nextIncidentId_ = 1;
    DispatchPolicy policy_;

    double effectivePriority(int i) const {
        int sev = incidents[i].severity;
        if (!policy_.aging || sev >= policy_.criticalSeverity) return sev;
        double boost = (double)(now_ - incidents[i].reportedAt) / policy_.agingMinutes;
        if (boost > policy_.maxBoost) boost = policy_.maxBoost;
        if (boost < 0) boost = 0;
        double p = sev + boost;
        double ceiling = policy_.criticalSeverity - 0.5;   // stays below every critical incident
        return p > ceiling ? ceiling : p;
    }
    // Reserves the nearest hospital bed for casualty incident i; returns hospital index or -1.
    int reserveBed(int i, double& km) {
        km = 0;
        if (!needsHospital(incidents[i].type)) return -1;
        ShortestPaths hp;
        int hn = nearestTarget(city_.g, incNode_[i], [&](int v) {
            int h = hospitalOfNode(v);
            return h >= 0 && hospitals[h].freeBeds > 0;
        }, hp);
        if (hn < 0) return -1;
        int h = hospitalOfNode(hn);
        km = hp.dist[hn];
        hospitals[h].freeBeds--;
        return h;
    }

    int resourceOfNode(int v) const { return (v >= city_.resBase && v < city_.hosBase) ? v - city_.resBase : -1; }
    int hospitalOfNode(int v) const {
        return (v >= city_.hosBase && v < city_.hosBase + hospitals.size()) ? v - city_.hosBase : -1;
    }

    void pushUrgent(int i) { urgent_.push(Urgency{incidents[i].severity, incidents[i].reportedAt, i}); }

    static std::string lower(std::string s) {
        for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        return s;
    }

    // Joins a new graph node to its 4 nearest existing places, like buildCityMap does.
    void connectNewNode(int v) {
        int n = city_.g.nodeCount();
        DynamicArray<bool> picked;
        for (int i = 0; i < n; i++) picked.push_back(false);
        picked[v] = true;
        for (int pass = 0; pass < 4 && pass < n - 1; pass++) {
            int best = -1; double bd = 0;
            for (int u = 0; u < n; u++) {
                if (picked[u]) continue;
                double d = haversineKm(nodeLat_[v], nodeLon_[v], nodeLat_[u], nodeLon_[u]);
                if (best < 0 || d < bd) { best = u; bd = d; }
            }
            picked[best] = true;
            city_.g.addEdge(v, best, bd * ROAD_FACTOR);
        }
    }

    // Core allocation for one OPEN incident. Does not touch the priority queue.
    DispatchResult allocate(int i) {
        DispatchResult res;
        res.incident = i;
        Incident& x = incidents[i];
        res.neededType = unitFor(x.type);
        const std::string want = res.neededType;

        // 1. KD tree: a few straight-line-nearest free units of the right type
        int cand[KD_CANDIDATES];
        res.candidates = kd_.kNearest(x.lat, x.lon * LON_SCALE, KD_CANDIDATES, cand, [&](int r) {
            return resources[r].type == want && resources[r].status == "AVAILABLE";
        });
        if (res.candidates == 0) return res;

        // 2. Dijkstra: real road distance to each candidate, kept in the sparse cost matrix
        ShortestPaths sp = dijkstra(city_.g, incNode_[i]);
        for (int j = 0; j < res.candidates; j++) cost_.set(i, cand[j], sp.dist[city_.resourceNode(cand[j])]);

        // 3. cheapest candidate that is still free
        int r = cost_.argMinInRow(i, [&](int c) { return resources[c].type == want && resources[c].status == "AVAILABLE"; });
        if (r < 0) return res;
        res.resource = r;
        res.km = cost_.get(i, r);

        // 4. reserve a hospital bed for casualty incidents (nearest by road with a free bed)
        res.hospital = reserveBed(i, res.hospitalKm);

        resources[r].status = "BUSY";
        x.status = "DISPATCHED";
        Action a;
        a.type = ACT_DISPATCH; a.incident = i; a.resource = r; a.hospital = res.hospital;
        a.dep = deployments_.add(i, r, Deployment{res.km, now_, res.hospital, res.hospitalKm, false});
        undo_.push(a);
        log_.info("Dispatched " + resources[r].name + " to incident #" + std::to_string(x.id) +
                  (res.hospital >= 0 ? ", bed reserved at " + hospitals[res.hospital].name : ""));
        return res;
    }

public:
    explicit NexaSystem(const std::string& logPath = "logs/operations.log")
        : log_(logPath), cost_(0, 0) {}
    NexaSystem(const NexaSystem&) = delete;
    NexaSystem& operator=(const NexaSystem&) = delete;

    static std::string unitFor(const std::string& type) {
        if (type == "FIRE") return "FIRE_TRUCK";
        if (type == "FLOOD") return "BOAT";
        if (type == "COLLAPSE" || type == "EARTHQUAKE") return "RESCUE_TEAM";
        return "AMBULANCE";   // MEDICAL, ACCIDENT
    }
    static bool needsHospital(const std::string& type) {
        return type == "MEDICAL" || type == "ACCIDENT" || type == "COLLAPSE" || type == "EARTHQUAKE";
    }

    // Loads the three CSVs (and deployments.csv if present) from dir and builds every index.
    bool load(const std::string& dir, std::string& err) {
        if (!loadIncidents(dir + "/incidents.csv", incidents)) { err = "cannot open " + dir + "/incidents.csv"; return false; }
        if (!loadResources(dir + "/resources.csv", resources)) { err = "cannot open " + dir + "/resources.csv"; return false; }
        if (!loadHospitals(dir + "/hospitals.csv", hospitals)) { err = "cannot open " + dir + "/hospitals.csv"; return false; }

        for (int i = 0; i < incidents.size(); i++) {
            const Incident& x = incidents[i];
            incById_.put(x.id, i);
            timeline_.getOrInsert(x.reportedAt).push_back(i);
            if (x.reportedAt > now_) now_ = x.reportedAt;
            if (x.id >= nextIncidentId_) nextIncidentId_ = x.id + 1;
            if (x.status == "OPEN") pushUrgent(i);
        }
        DynamicArray<KDPoint> pts;
        for (int r = 0; r < resources.size(); r++) {
            resById_.put(resources[r].id, r);
            resByName_.put(resources[r].name, r);
            pts.push_back(KDPoint{resources[r].lat, resources[r].lon * LON_SCALE, r});
        }
        kd_.build(pts);

        buildCityMap(incidents, resources, hospitals, city_);
        for (const Incident& x : incidents) { nodeLat_.push_back(x.lat); nodeLon_.push_back(x.lon); }
        for (const Resource& x : resources) { nodeLat_.push_back(x.lat); nodeLon_.push_back(x.lon); }
        for (const Hospital& x : hospitals) { nodeLat_.push_back(x.lat); nodeLon_.push_back(x.lon); }
        for (int v = 0; v < city_.g.nodeCount(); v++) nodeIncident_.push_back(v < incidents.size() ? v : -1);
        for (int i = 0; i < incidents.size(); i++) incNode_.push_back(city_.incidentNode(i));

        cost_ = SparseMatrix<double>(incidents.size(), resources.size());

        // deployments.csv exists only in a folder written by save()
        CsvTable rows;
        if (readCsv(dir + "/deployments.csv", rows)) {
            for (int k = 0; k < rows.size(); k++) {
                const CsvRow& row = rows[k];
                if (row.size() < 5) continue;
                try {
                    int* i = incById_.get(std::stoi(row[0]));
                    int* r = resById_.get(std::stoi(row[1]));
                    if (!i || !r) continue;
                    int h = std::stoi(row[4]);
                    double hkm = row.size() >= 6 ? std::stod(row[5]) : 0;   // columns added later
                    bool div = row.size() >= 7 && row[6] == "1";
                    deployments_.add(*i, *r, Deployment{std::stod(row[2]), std::stoll(row[3]), h, hkm, div});
                } catch (...) {}
            }
        }
        // meta.csv (written by save) holds the clock, which may be later than the last report
        CsvTable meta;
        if (readCsv(dir + "/meta.csv", meta)) {
            for (int k = 0; k < meta.size(); k++) {
                if (meta[k].size() < 2 || meta[k][0] != "now") continue;
                try { long long t = std::stoll(meta[k][1]); if (t > now_) now_ = t; } catch (...) {}
            }
        }
        log_.info("System loaded from '" + dir + "': " + std::to_string(incidents.size()) + " incidents, " +
                  std::to_string(resources.size()) + " resources, " + std::to_string(hospitals.size()) +
                  " hospitals, " + std::to_string(deployments_.size()) + " deployments");
        return true;
    }

    // Writes everything (including deployments) to dir, creating it if needed.
    bool save(const std::string& dir) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        CsvTable rows;
        deployments_.forEachAll([&](MultiLinkedList<Deployment>::Node& d) {
            CsvRow row;
            row.push_back(std::to_string(incidents[d.row].id));
            row.push_back(std::to_string(resources[d.col].id));
            row.push_back(std::to_string(d.val.km));
            row.push_back(std::to_string(d.val.at));
            row.push_back(std::to_string(d.val.hospital));
            row.push_back(std::to_string(d.val.hospitalKm));
            row.push_back(d.val.diverted ? "1" : "0");
            rows.push_back(row);
        });
        bool ok = saveIncidents(dir + "/incidents.csv", incidents) &&
                  saveResources(dir + "/resources.csv", resources) &&
                  saveHospitals(dir + "/hospitals.csv", hospitals) &&
                  writeCsv(dir + "/deployments.csv", "incident_id,resource_id,km,at,hospital,hospital_km,diverted", rows);
        CsvTable meta;
        CsvRow clock;
        clock.push_back("now"); clock.push_back(std::to_string(now_));
        meta.push_back(clock);
        ok = ok && writeCsv(dir + "/meta.csv", "key,value", meta);
        if (ok) log_.info("State saved to '" + dir + "'");
        else log_.error("Saving to '" + dir + "' failed");
        return ok;
    }

    // Adds a new OPEN incident and puts it in the intake queue. Returns its index.
    // at = report time; by default one minute after the current time.
    int reportIncident(const std::string& type, int severity, double lat, double lon, const std::string& desc,
                       long long at = -1) {
        Incident x;
        x.id = nextIncidentId_++;
        x.type = type; x.severity = severity; x.lat = lat; x.lon = lon;
        if (at < 0) at = now_ + 1;
        if (at > now_) now_ = at;
        x.reportedAt = at;
        x.status = "OPEN"; x.description = desc;
        int i = incidents.size();
        incidents.push_back(x);
        incById_.put(x.id, i);
        timeline_.getOrInsert(x.reportedAt).push_back(i);

        int v = city_.g.addNode();                      // the new place joins the road network
        nodeLat_.push_back(lat); nodeLon_.push_back(lon);
        nodeIncident_.push_back(i);
        incNode_.push_back(v);
        connectNewNode(v);

        cost_.addRow();
        intake_.enqueue(i);
        Action a; a.type = ACT_REPORT; a.incident = i;
        undo_.push(a);
        log_.info("Reported incident #" + std::to_string(x.id) + " " + type + " sev " + std::to_string(severity));
        return i;
    }

    // Moves everything from the intake queue into the priority queue. Returns how many.
    int triage() {
        int n = 0;
        while (!intake_.empty()) {
            int i = intake_.dequeue();
            if (incidents[i].status == "OPEN") { pushUrgent(i); n++; }
        }
        return n;
    }

    // Dispatches the most urgent OPEN incident that has a free unit of its type.
    DispatchResult dispatchNext() {
        triage();
        if (policy_.aging) return dispatchNextAging();
        DynamicArray<Urgency> skipped;
        DispatchResult res;
        while (!urgent_.empty()) {
            Urgency u = urgent_.pop();
            const Incident& x = incidents[u.idx];
            if (x.status != "OPEN" || x.severity != u.sev) continue;      // stale entry
            res = allocate(u.idx);
            if (res.resource >= 0) break;
            skipped.push_back(u);                                         // no unit right now: keep it
            res = DispatchResult();
        }
        for (const Urgency& u : skipped) urgent_.push(u);
        res.waiting = skipped.size();
        return res;
    }

    // Ageing policy: priorities change as time passes, so a fixed heap order no longer holds.
    // Rank the OPEN incidents by their current effective priority (merge sort) and serve the first
    // one that has a free unit. The heap's entries simply go stale and are skipped later.
    DispatchResult dispatchNextAging() {
        DynamicArray<int> open = linearSearchAll(incidents, [](const Incident& x) { return x.status == "OPEN"; });
        mergeSort(open, [&](int a, int b) {
            double pa = effectivePriority(a), pb = effectivePriority(b);
            if (pa != pb) return pa > pb;
            return IncidentHigherPriority()(incidents[a], incidents[b]);
        });
        int skipped = 0;
        for (int i : open) {
            DispatchResult res = allocate(i);
            if (res.resource >= 0) { res.waiting = skipped; return res; }
            skipped++;
        }
        DispatchResult none;
        none.waiting = skipped;
        return none;
    }

    // Pre-emption: give critical incident `to` a unit that is busy elsewhere, chosen from the units
    // for which canTake(resource) is true (the simulator allows only units still driving to a
    // minor incident). The nearest such unit by road is diverted; the incident it was heading to
    // goes back to OPEN and its hospital bed is released. Returns resource = -1 if none qualifies.
    template <typename Pred>
    DispatchResult preemptFor(int to, Pred canTake) {
        DispatchResult res;
        res.incident = to;
        Incident& x = incidents[to];
        res.neededType = unitFor(x.type);
        if (x.status != "OPEN") return res;
        ShortestPaths sp = dijkstra(city_.g, incNode_[to]);
        int best = -1;
        for (int r = 0; r < resources.size(); r++) {
            if (resources[r].type != res.neededType || resources[r].status != "BUSY" || !canTake(r)) continue;
            if (activeIncidentOf(r) < 0) continue;
            double d = sp.dist[city_.resourceNode(r)];
            if (best < 0 || d < sp.dist[city_.resourceNode(best)]) best = r;
        }
        if (best < 0) return res;

        MultiLinkedList<Deployment>::Node* old = deployments_.colLast(best);
        int from = old->row;
        old->val.diverted = true;
        if (old->val.hospital >= 0) hospitals[old->val.hospital].freeBeds++;
        incidents[from].status = "OPEN";
        pushUrgent(from);

        res.resource = best;
        res.bumped = from;
        res.km = sp.dist[city_.resourceNode(best)];
        cost_.set(to, best, res.km);
        res.hospital = reserveBed(to, res.hospitalKm);
        x.status = "DISPATCHED";
        deployments_.add(to, best, Deployment{res.km, now_, res.hospital, res.hospitalKm, false});
        log_.info("Pre-empted " + resources[best].name + " from incident #" + std::to_string(incidents[from].id) +
                  " (sev " + std::to_string(incidents[from].severity) + ") to incident #" + std::to_string(x.id) +
                  " (sev " + std::to_string(x.severity) + ")");
        return res;
    }

    // Incident the unit is currently assigned to, or -1 (free, returning, or busy on an unknown job).
    int activeIncidentOf(int r) const {
        MultiLinkedList<Deployment>::Node* last = deployments_.colLast(r);
        if (!last || last->val.diverted || incidents[last->row].status != "DISPATCHED") return -1;
        return last->row;
    }

    void setPolicy(const DispatchPolicy& p) { policy_ = p; }
    const DispatchPolicy& policy() const { return policy_; }

    // Dispatches one specific incident by id (its old queue entry becomes stale and is skipped later).
    DispatchResult dispatchIncident(int id, std::string& err) {
        int* i = incById_.get(id);
        if (!i) { err = "no incident #" + std::to_string(id); return DispatchResult(); }
        if (incidents[*i].status != "OPEN") { err = "incident #" + std::to_string(id) + " is " + incidents[*i].status; return DispatchResult(); }
        return allocate(*i);
    }

    // Marks a DISPATCHED incident RESOLVED and frees its units. Returns how many units were freed, -1 on error.
    int resolveIncident(int id, std::string& err) {
        int* ip = incById_.get(id);
        if (!ip) { err = "no incident #" + std::to_string(id); return -1; }
        int i = *ip;
        if (incidents[i].status != "DISPATCHED") { err = "incident #" + std::to_string(id) + " is " + incidents[i].status; return -1; }
        Action a; a.type = ACT_RESOLVE; a.incident = i;
        deployments_.forEachInRow(i, [&](MultiLinkedList<Deployment>::Node& d) {
            if (!d.val.diverted && resources[d.col].status == "BUSY") { resources[d.col].status = "AVAILABLE"; a.freed.push_back(d.col); }
        });
        incidents[i].status = "RESOLVED";
        int freed = a.freed.size();
        undo_.push(a);
        log_.info("Resolved incident #" + std::to_string(id) + ", freed " + std::to_string(freed) + " unit(s)");
        return freed;
    }

    // Undoes the most recent report / dispatch / resolve. Returns a description, or "" if nothing to undo.
    std::string undo() {
        if (undo_.empty()) return "";
        Action a = undo_.pop();
        Incident& x = incidents[a.incident];
        std::string msg;
        if (a.type == ACT_REPORT) {
            x.status = "CANCELLED";                       // records stay, but it is never dispatched
            msg = "cancelled report of incident #" + std::to_string(x.id);
        } else if (a.type == ACT_DISPATCH) {
            deployments_.remove(a.dep);                   // O(1): unlinks from row, column and history lists
            resources[a.resource].status = "AVAILABLE";
            if (a.hospital >= 0) hospitals[a.hospital].freeBeds++;
            x.status = "OPEN";
            pushUrgent(a.incident);
            msg = "recalled " + resources[a.resource].name + " from incident #" + std::to_string(x.id);
        } else {
            for (int r : a.freed) resources[r].status = "BUSY";
            x.status = "DISPATCHED";
            msg = "re-opened incident #" + std::to_string(x.id) + " as DISPATCHED";
        }
        log_.info("Undo: " + msg);
        return msg;
    }

    // ---------- queries ----------

    int incidentIndex(int id) const { const int* p = incById_.get(id); return p ? *p : -1; }
    int resourceByName(const std::string& name) const { const int* p = resByName_.get(name); return p ? *p : -1; }

    // Case-insensitive substring match on resource names (linear search).
    DynamicArray<int> resourcesMatching(const std::string& text) const {
        std::string t = lower(text);
        return linearSearchAll(resources, [&](const Resource& r) { return lower(r.name).find(t) != std::string::npos; });
    }

    // Incident indices with the given status, most urgent first (merge sort, stable).
    DynamicArray<int> incidentsByPriority(const std::string& status) const {
        DynamicArray<int> idx = linearSearchAll(incidents, [&](const Incident& x) { return x.status == status; });
        mergeSort(idx, [&](int a, int b) { return IncidentHigherPriority()(incidents[a], incidents[b]); });
        return idx;
    }

    // Up to k nearest units to (lat, lon), nearest first. type "" = any type.
    int nearestResources(double lat, double lon, int k, const std::string& type, bool availableOnly, int* out) const {
        return kd_.kNearest(lat, lon * LON_SCALE, k, out, [&](int r) {
            return (type.empty() || resources[r].type == type) && (!availableOnly || resources[r].status == "AVAILABLE");
        });
    }

    // Incident indices reported in [from, to], in time order (AVL range query).
    DynamicArray<int> incidentsBetween(long long from, long long to) {
        DynamicArray<int> out;
        timeline_.rangeQuery(from, to, [&](const long long&, DynamicArray<int>& ids) {
            for (int i : ids) out.push_back(i);
        });
        return out;
    }

    // Shortest road route from incident i to the nearest hospital with a free bed.
    // Returns the hospital index (or -1); fills the node path and its length.
    int routeToHospital(int i, DynamicArray<int>& path, double& km) const {
        ShortestPaths sp;
        int hn = nearestTarget(city_.g, incNode_[i], [&](int v) {
            int h = hospitalOfNode(v);
            return h >= 0 && hospitals[h].freeBeds > 0;
        }, sp);
        if (hn < 0) return -1;
        path = pathTo(sp, hn);
        km = sp.dist[hn];
        return hospitalOfNode(hn);
    }

    std::string nodeLabel(int v) const {
        int r = resourceOfNode(v), h = hospitalOfNode(v);
        if (r >= 0) return resources[r].name;
        if (h >= 0) return hospitals[h].name;
        if (v >= 0 && v < nodeIncident_.size() && nodeIncident_[v] >= 0) return "Incident #" + std::to_string(incidents[nodeIncident_[v]].id);
        return "node " + std::to_string(v);
    }

    MultiLinkedList<Deployment>& deployments() { return deployments_; }
    const SparseMatrix<double>& costMatrix() const { return cost_; }
    const Graph& roads() const { return city_.g; }
    int intakeSize() const { return intake_.size(); }
    int undoDepth() const { return undo_.size(); }
    long long now() const { return now_; }
    int timelineHeight() const { return timeline_.height(); }
    size_t nameTableProbes() const { return resByName_.probeCount(); }

    // ---------- hooks for the simulator (include/nexa/app/simulator.h) ----------

    // Moves the clock forward (never backward).
    void advanceClock(long long t) { if (t > now_) now_ = t; }
    // The unit's crew has finished on scene: the incident is closed but the unit is still driving back.
    void markResolved(int i) {
        if (incidents[i].status != "DISPATCHED") return;
        incidents[i].status = "RESOLVED";
        log_.info("Incident #" + std::to_string(incidents[i].id) + " cleared at t=" + std::to_string(now_));
    }
    // The unit is back at base and can be dispatched again.
    void releaseUnit(int r) {
        if (resources[r].status == "AVAILABLE") return;
        resources[r].status = "AVAILABLE";
        log_.info(resources[r].name + " available again at t=" + std::to_string(now_));
    }
    // Once time has passed, old actions can no longer be undone safely.
    void clearUndo() { undo_.clear(); }
    void logInfo(const std::string& msg) { log_.info(msg); }
};

}  // namespace nexa
