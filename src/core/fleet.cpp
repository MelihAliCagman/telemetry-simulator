#include "core/fleet.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>

namespace uavgcs {

Fleet::Fleet(std::uint32_t seed) : rng_(seed), seed_(seed) {}

Uav* Fleet::find_locked(int id) {
    auto it = std::find_if(uavs_.begin(), uavs_.end(), [id](const auto& u) { return u->id() == id; });
    return it == uavs_.end() ? nullptr : it->get();
}

std::string Fleet::make_sortie_id() {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "SRT-%08X", static_cast<unsigned>(rng_()));
    return buf;
}

std::optional<int> Fleet::add_uav(const std::string& callsign, UavSpec spec, LatLon home,
                                  std::vector<Waypoint> mission, double initial_battery_pct) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (uavs_.size() >= kMaxUavs) return std::nullopt;
    const int id = static_cast<int>(uavs_.size()) + 1;
    uavs_.push_back(std::make_unique<Uav>(id, callsign, std::move(spec), home, std::move(mission),
                                          seed_ + static_cast<std::uint32_t>(id), initial_battery_pct));
    return id;
}

bool Fleet::start(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    Uav* u = find_locked(id);
    if (!u) return false;
    const std::string sortie = make_sortie_id();
    if (!u->start(sortie)) return false;
    alerts_.reset(id);
    return true;
}

bool Fleet::return_to_launch(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    Uav* u = find_locked(id);
    return u && u->return_to_launch();
}

bool Fleet::set_link(int id, bool up) {
    std::lock_guard<std::mutex> lock(mutex_);
    Uav* u = find_locked(id);
    if (!u) return false;
    u->set_link_forced_down(!up);
    return true;
}

void Fleet::set_wind(const Wind& wind) {
    std::lock_guard<std::mutex> lock(mutex_);
    wind_ = wind;
}

Wind Fleet::wind() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return wind_;
}

void Fleet::add_zone(Zone zone) {
    std::lock_guard<std::mutex> lock(mutex_);
    fences_.add(std::move(zone));
}

std::vector<Zone> Fleet::zones() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return fences_.zones();
}

UavState Fleet::snapshot_locked(const Uav& u) const {
    UavState s = u.state();
    s.active_alerts = alerts_.active(s.id);
    return s;
}

TickResult Fleet::tick(double dt_s) {
    std::lock_guard<std::mutex> lock(mutex_);
    sim_time_s_ += dt_s;

    TickResult result;
    result.sim_time_s = sim_time_s_;
    result.states.reserve(uavs_.size());

    for (auto& u : uavs_) {
        u->step(dt_s, wind_);

        const auto raised = alerts_.evaluate(u->state(), u->return_energy_wh(), fences_, sim_time_s_);
        for (const auto& a : raised) {
            // Otomatik arıza önlemleri: sınır ihlali, kritik batarya veya yetersiz enerji -> eve dön.
            if (a.type == AlertType::GeofenceBreach || a.type == AlertType::CriticalBattery ||
                a.type == AlertType::InsufficientReturnEnergy) {
                u->return_to_launch();
            }
            recent_.push_back(a);
            if (recent_.size() > kMaxRecentAlerts) recent_.pop_front();
            result.new_alerts.push_back(a);
        }
        result.states.push_back(snapshot_locked(*u));
    }
    return result;
}

std::vector<UavState> Fleet::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UavState> out;
    out.reserve(uavs_.size());
    for (const auto& u : uavs_) out.push_back(snapshot_locked(*u));
    return out;
}

std::optional<UavState> Fleet::get(int id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& u : uavs_) {
        if (u->id() == id) return snapshot_locked(*u);
    }
    return std::nullopt;
}

std::vector<Alert> Fleet::recent_alerts(std::size_t max_count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::size_t n = std::min(max_count, recent_.size());
    return {recent_.end() - static_cast<std::ptrdiff_t>(n), recent_.end()};
}

double Fleet::sim_time_s() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sim_time_s_;
}

}  // namespace uavgcs
