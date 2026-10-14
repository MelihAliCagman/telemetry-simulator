#include "api/serialize.hpp"

#include <cmath>

namespace uavgcs::api {

namespace {
// JSON'a yazılırken gereksiz uzun ondalıkları kırp (konum için ~1 cm, diğerleri için 2 hane).
double round_to(double v, int digits) {
    const double f = std::pow(10.0, digits);
    return std::round(v * f) / f;
}
}  // namespace

json to_json(const UavState& s) {
    json alerts = json::array();
    for (auto t : s.active_alerts) alerts.push_back(to_string(t));
    return {
        {"id", s.id},
        {"callsign", s.callsign},
        {"type", s.type},
        {"sortie_id", s.sortie_id},
        {"lat", round_to(s.pos.lat, 7)},
        {"lon", round_to(s.pos.lon, 7)},
        {"home", {{"lat", s.home.lat}, {"lon", s.home.lon}}},
        {"alt_m", round_to(s.alt_m, 1)},
        {"groundspeed_mps", round_to(s.groundspeed_mps, 1)},
        {"airspeed_mps", round_to(s.airspeed_mps, 1)},
        {"vertical_speed_mps", round_to(s.vertical_speed_mps, 1)},
        {"heading_deg", round_to(s.heading_deg, 1)},
        {"battery_pct", round_to(s.battery_pct, 1)},
        {"battery_wh", round_to(s.battery_wh, 1)},
        {"link_quality", round_to(s.link_quality, 2)},
        {"link_ok", s.link_ok},
        {"failsafe_active", s.failsafe_active},
        {"phase", to_string(s.phase)},
        {"waypoint_index", s.waypoint_index},
        {"waypoint_count", s.waypoint_count},
        {"distance_to_home_m", round_to(s.distance_to_home_m, 0)},
        {"active_alerts", std::move(alerts)},
    };
}

json to_json(const Alert& a) {
    return {
        {"seq", a.seq},
        {"uav_id", a.uav_id},
        {"callsign", a.callsign},
        {"type", to_string(a.type)},
        {"severity", to_string(a.severity)},
        {"message", a.message},
        {"sim_time_s", round_to(a.sim_time_s, 1)},
        {"raised_at", a.raised_at},
    };
}

json to_json(const Zone& z) {
    json poly = json::array();
    for (const auto& p : z.polygon) poly.push_back({{"lat", p.lat}, {"lon", p.lon}});
    return {{"name", z.name}, {"kind", to_string(z.kind)}, {"polygon", std::move(poly)}};
}

json to_json(const Wind& w) {
    return {{"speed_mps", w.speed_mps}, {"from_deg", w.from_deg}, {"gust_mps", w.gust_mps}};
}

json to_json(const SortieInfo& s) {
    return {
        {"id", s.id},
        {"uav_id", s.uav_id},
        {"callsign", s.callsign},
        {"uav_type", s.uav_type},
        {"started_at", s.started_at},
        {"ended_at", s.ended_at.empty() ? json(nullptr) : json(s.ended_at)},
        {"final_phase", s.final_phase.empty() ? json(nullptr) : json(s.final_phase)},
        {"sample_count", s.sample_count},
    };
}

json to_json(const TrackPoint& p) {
    return {
        {"recorded_at", p.recorded_at},
        {"lat", round_to(p.lat, 7)},
        {"lon", round_to(p.lon, 7)},
        {"alt_m", round_to(p.alt_m, 1)},
        {"groundspeed_mps", round_to(p.groundspeed_mps, 1)},
        {"battery_pct", round_to(p.battery_pct, 1)},
        {"phase", p.phase},
    };
}

json telemetry_message(const TickResult& tick, double time_scale) {
    json uavs = json::array();
    for (const auto& s : tick.states) uavs.push_back(to_json(s));
    return {{"type", "telemetry"}, {"sim_time_s", round_to(tick.sim_time_s, 1)}, {"time_scale", time_scale},
            {"uavs", std::move(uavs)}};
}

json alert_message(const Alert& a) { return {{"type", "alert"}, {"alert", to_json(a)}}; }

}  // namespace uavgcs::api
