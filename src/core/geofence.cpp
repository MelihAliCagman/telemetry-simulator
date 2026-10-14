#include "core/geofence.hpp"

#include <utility>

namespace uavgcs {

const char* to_string(ZoneKind k) {
    switch (k) {
        case ZoneKind::NoFly: return "NO_FLY";
        case ZoneKind::KeepIn: return "KEEP_IN";
    }
    return "UNKNOWN";
}

void GeofenceSet::add(Zone zone) { zones_.push_back(std::move(zone)); }

std::optional<std::string> GeofenceSet::violation(const LatLon& p) const {
    for (const auto& z : zones_) {
        const bool inside = geo::point_in_polygon(p, z.polygon);
        if ((z.kind == ZoneKind::NoFly && inside) || (z.kind == ZoneKind::KeepIn && !inside)) {
            return z.name;
        }
    }
    return std::nullopt;
}

}  // namespace uavgcs
