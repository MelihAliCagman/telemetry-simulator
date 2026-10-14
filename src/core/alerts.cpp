#include "core/alerts.hpp"

#include <cstdio>
#include <utility>

#include "util/time.hpp"

namespace uavgcs {

Alert AlertEngine::make(const UavState& s, AlertType type, Severity sev, std::string msg, double sim_time_s) {
    Alert a;
    a.seq = next_seq_++;
    a.uav_id = s.id;
    a.callsign = s.callsign;
    a.type = type;
    a.severity = sev;
    a.message = std::move(msg);
    a.sim_time_s = sim_time_s;
    a.raised_at = util::iso_utc_now();
    return a;
}

std::vector<Alert> AlertEngine::evaluate(const UavState& s, double return_energy_wh, const GeofenceSet& fences,
                                         double sim_time_s) {
    std::vector<Alert> raised;
    auto& active = active_[s.id];

    // Her koşulu bir kez değerlendir; yeni doğru olanlar için uyarı üret, bitenleri temizle.
    auto update = [&](AlertType type, bool condition, Severity sev, const std::string& msg) {
        const bool was_active = active.count(type) > 0;
        if (condition && !was_active) {
            active.insert(type);
            raised.push_back(make(s, type, sev, msg, sim_time_s));
        } else if (!condition && was_active) {
            active.erase(type);
        }
    };

    const bool airborne = is_airborne(s.phase);
    char buf[160];

    std::snprintf(buf, sizeof(buf), "Düşük batarya: %%%.0f", s.battery_pct);
    update(AlertType::LowBattery, airborne && s.battery_pct < kLowBatteryPct, Severity::Warning, buf);

    std::snprintf(buf, sizeof(buf), "Kritik batarya: %%%.0f", s.battery_pct);
    update(AlertType::CriticalBattery, airborne && s.battery_pct < kCriticalBatteryPct, Severity::Critical, buf);

    update(AlertType::LinkLost, airborne && !s.link_ok, Severity::Critical, "Veri bağı kaybı");

    std::optional<std::string> zone;
    if (airborne && s.alt_m > kMinAltForGeofenceM) zone = fences.violation(s.pos);
    update(AlertType::GeofenceBreach, zone.has_value(), Severity::Critical,
           "Coğrafi sınır ihlali: " + zone.value_or(""));

    // Gereken enerjiye %10 emniyet payı eklenir.
    const double needed = return_energy_wh * 1.10;
    const bool short_of_energy = airborne && s.phase != FlightPhase::Landing && s.battery_wh < needed;
    std::snprintf(buf, sizeof(buf), "Eve dönüş için yetersiz enerji (kalan %.1f Wh, gereken %.1f Wh)", s.battery_wh,
                  needed);
    update(AlertType::InsufficientReturnEnergy, short_of_energy, Severity::Critical, buf);

    return raised;
}

std::vector<AlertType> AlertEngine::active(int uav_id) const {
    auto it = active_.find(uav_id);
    if (it == active_.end()) return {};
    return {it->second.begin(), it->second.end()};
}

void AlertEngine::reset(int uav_id) { active_.erase(uav_id); }

}  // namespace uavgcs
