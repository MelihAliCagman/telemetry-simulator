#pragma once
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "core/alerts.hpp"
#include "core/geofence.hpp"
#include "core/types.hpp"
#include "core/uav.hpp"

namespace uavgcs {

struct TickResult {
    double sim_time_s{0.0};
    std::vector<UavState> states;
    std::vector<Alert> new_alerts;
};

// Birden fazla İHA'yı, ortak rüzgar ve coğrafi sınırlarla yöneten iş parçacığı güvenli filo yöneticisi.
class Fleet {
public:
    static constexpr std::size_t kMaxUavs = 50;
    static constexpr std::size_t kMaxRecentAlerts = 200;

    explicit Fleet(std::uint32_t seed = std::random_device{}());

    // Filoya İHA ekler ve atanan kimliği döndürür; filo doluysa std::nullopt.
    std::optional<int> add_uav(const std::string& callsign, UavSpec spec, LatLon home,
                               std::vector<Waypoint> mission, double initial_battery_pct = 100.0);

    bool start(int id);
    bool return_to_launch(int id);
    bool set_link(int id, bool up);

    void set_wind(const Wind& wind);
    Wind wind() const;

    void add_zone(Zone zone);
    std::vector<Zone> zones() const;

    // Simülasyonu `dt_s` saniye ilerletir. Kilit sadece bu çağrı boyunca tutulur.
    TickResult tick(double dt_s);

    std::vector<UavState> snapshot() const;
    std::optional<UavState> get(int id) const;
    std::vector<Alert> recent_alerts(std::size_t max_count) const;
    double sim_time_s() const;

private:
    UavState snapshot_locked(const Uav& u) const;
    Uav* find_locked(int id);
    std::string make_sortie_id();

    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<Uav>> uavs_;
    GeofenceSet fences_;
    AlertEngine alerts_;
    std::deque<Alert> recent_;
    Wind wind_;
    double sim_time_s_{0.0};
    std::mt19937 rng_;
    std::uint32_t seed_;
};

}  // namespace uavgcs
