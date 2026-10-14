#include "core/geo.hpp"

#include <algorithm>
#include <cmath>

namespace uavgcs::geo {

double deg2rad(double deg) { return deg * kPi / 180.0; }
double rad2deg(double rad) { return rad * 180.0 / kPi; }

double normalize_deg(double deg) {
    double r = std::fmod(deg, 360.0);
    if (r < 0.0) r += 360.0;
    return r;
}

double signed_angle_diff_deg(double from_deg, double to_deg) {
    double d = normalize_deg(to_deg - from_deg);
    if (d > 180.0) d -= 360.0;
    return d;
}

double distance_m(const LatLon& a, const LatLon& b) {
    const double lat1 = deg2rad(a.lat);
    const double lat2 = deg2rad(b.lat);
    const double dlat = lat2 - lat1;
    const double dlon = deg2rad(b.lon - a.lon);
    const double s1 = std::sin(dlat / 2.0);
    const double s2 = std::sin(dlon / 2.0);
    const double h = s1 * s1 + std::cos(lat1) * std::cos(lat2) * s2 * s2;
    return 2.0 * kEarthRadiusM * std::asin(std::min(1.0, std::sqrt(h)));
}

double bearing_deg(const LatLon& a, const LatLon& b) {
    const double lat1 = deg2rad(a.lat);
    const double lat2 = deg2rad(b.lat);
    const double dlon = deg2rad(b.lon - a.lon);
    const double y = std::sin(dlon) * std::cos(lat2);
    const double x = std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(dlon);
    return normalize_deg(rad2deg(std::atan2(y, x)));
}

LatLon offset_m(const LatLon& p, double north_m, double east_m) {
    const double dlat = north_m / kEarthRadiusM;
    const double dlon = east_m / (kEarthRadiusM * std::cos(deg2rad(p.lat)));
    return {p.lat + rad2deg(dlat), p.lon + rad2deg(dlon)};
}

bool point_in_polygon(const LatLon& p, const std::vector<LatLon>& polygon) {
    const std::size_t n = polygon.size();
    if (n < 3) return false;
    bool inside = false;
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const double xi = polygon[i].lon, yi = polygon[i].lat;
        const double xj = polygon[j].lon, yj = polygon[j].lat;
        if ((yi > p.lat) != (yj > p.lat) &&
            p.lon < (xj - xi) * (p.lat - yi) / (yj - yi) + xi) {
            inside = !inside;
        }
    }
    return inside;
}

}  // namespace uavgcs::geo
