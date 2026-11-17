#pragma once
#include <string>

namespace nexa {

struct Resource {
    int id = 0;
    std::string name;
    std::string type;          // AMBULANCE, FIRE_TRUCK, RESCUE_TEAM, BOAT
    std::string capability;    // e.g. ICU, HEAVY_LIFT, WATER_RESCUE
    double lat = 0, lon = 0;
    std::string status = "AVAILABLE";  // AVAILABLE, BUSY
    long long availFrom = 0, availTo = 0;  // epoch minutes
};

}  // namespace nexa
