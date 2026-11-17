#include <filesystem>
#include <fstream>
#include <string>
#include "test_util.h"
#include "nexa/io/logger.h"
#include "nexa/io/store.h"
#include "nexa/ds/priority_queue.h"
using namespace nexa;

int main() {
    std::filesystem::create_directories("test_tmp");
    { // CSV quoting
        CsvRow r = splitCsvLine("1,\"a, b\",\"say \"\"hi\"\"\",end");
        CHECK(r.size() == 4 && r[1] == "a, b" && r[2] == "say \"hi\"" && r[3] == "end");
        CHECK(escapeCsv("plain") == "plain" && escapeCsv("a,b") == "\"a,b\"");
    }
    { // Incident round-trip (description has comma + quote)
        DynamicArray<Incident> v;
        Incident a; a.id = 1; a.type = "FIRE"; a.severity = 9; a.lat = 28.55; a.lon = 77.33;
        a.reportedAt = 1000; a.description = "Warehouse fire, \"Block C\"";
        Incident b; b.id = 2; b.type = "FLOOD"; b.severity = 4; b.reportedAt = 500; b.description = "Low-lying area";
        v.push_back(a); v.push_back(b);
        CHECK(saveIncidents("test_tmp/inc.csv", v));
        DynamicArray<Incident> w;
        CHECK(loadIncidents("test_tmp/inc.csv", w));
        CHECK(w.size() == 2 && w[0].description == a.description && w[0].severity == 9);
        CHECK(w[0].lat > 28.549 && w[0].lat < 28.551 && w[1].type == "FLOOD");
    }
    { // Malformed rows skipped, missing file reported
        std::ofstream("test_tmp/bad.csv") << "id,type,severity,lat,lon,reported_at,status,description\n"
            << "1,FIRE,5,28.5,77.3,10,OPEN,ok\n"
            << "oops,FIRE\n"
            << "x,FIRE,notnum,28.5,77.3,10,OPEN,bad\n";
        DynamicArray<Incident> w;
        CHECK(loadIncidents("test_tmp/bad.csv", w) && w.size() == 1);
        CHECK(!loadIncidents("test_tmp/nope.csv", w));
    }
    { // Resource + Hospital round-trip
        DynamicArray<Resource> r; Resource x; x.id = 3; x.name = "Medic 3"; x.type = "AMBULANCE"; x.capability = "ICU"; x.availTo = 99;
        r.push_back(x);
        DynamicArray<Resource> r2;
        CHECK(saveResources("test_tmp/res.csv", r) && loadResources("test_tmp/res.csv", r2) && r2.size() == 1 && r2[0].capability == "ICU" && r2[0].availTo == 99);
        DynamicArray<Hospital> h; Hospital y; y.id = 1; y.name = "City Hospital"; y.totalBeds = 100; y.freeBeds = 12; h.push_back(y);
        DynamicArray<Hospital> h2;
        CHECK(saveHospitals("test_tmp/hos.csv", h) && loadHospitals("test_tmp/hos.csv", h2) && h2.size() == 1 && h2[0].freeBeds == 12);
    }
    { // Incident priority ordering: severity desc, then earliest first
        PriorityQueue<Incident, IncidentHigherPriority> pq;
        Incident a; a.id = 1; a.severity = 5; a.reportedAt = 10;
        Incident b; b.id = 2; b.severity = 9; b.reportedAt = 50;
        Incident c; c.id = 3; c.severity = 9; c.reportedAt = 20;
        pq.push(a); pq.push(b); pq.push(c);
        CHECK(pq.pop().id == 3 && pq.pop().id == 2 && pq.pop().id == 1);
    }
    { // Logger
        std::filesystem::remove("test_tmp/log/ops.log");
        Logger lg("test_tmp/log/ops.log");
        lg.info("hello"); lg.warn("careful");
        std::ifstream in("test_tmp/log/ops.log"); std::string l1, l2;
        std::getline(in, l1); std::getline(in, l2);
        CHECK(l1.find("[INFO] hello") != std::string::npos && l2.find("[WARN] careful") != std::string::npos);
    }
    std::filesystem::remove_all("test_tmp");
    TEST_END();
}
