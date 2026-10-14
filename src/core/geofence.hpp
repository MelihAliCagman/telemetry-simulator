#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/types.hpp"

namespace uavgcs {

enum class ZoneKind {
    NoFly,   // içine girmek yasak
    KeepIn   // dışına çıkmak yasak
};

const char* to_string(ZoneKind k);

struct Zone {
    std::string name;
    ZoneKind kind{ZoneKind::NoFly};
    std::vector<LatLon> polygon;
};

class GeofenceSet {
public:
    void add(Zone zone);
    const std::vector<Zone>& zones() const { return zones_; }

    // Konum herhangi bir bölgeyi ihlal ediyorsa ilk ihlal edilen bölgenin adını döndürür.
    std::optional<std::string> violation(const LatLon& p) const;

private:
    std::vector<Zone> zones_;
};

}  // namespace uavgcs
