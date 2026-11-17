#pragma once
#include <string>

namespace nexa {

struct Hospital {
    int id = 0;
    std::string name;
    double lat = 0, lon = 0;
    int totalBeds = 0, freeBeds = 0;
};

}  // namespace nexa
