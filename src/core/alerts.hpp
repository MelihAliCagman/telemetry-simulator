#pragma once
#include <map>
#include <set>
#include <vector>

#include "core/geofence.hpp"
#include "core/types.hpp"

namespace uavgcs {

// Kenar tetiklemeli uyarı motoru: bir koşul ilk kez doğru olduğunda uyarı üretir,
// koşul sona erene kadar tekrar üretmez.
class AlertEngine {
public:
    static constexpr double kLowBatteryPct = 25.0;
    static constexpr double kCriticalBatteryPct = 10.0;
    static constexpr double kMinAltForGeofenceM = 5.0;

    // `return_energy_wh`: İHA'nın eve dönmesi için gereken tahmini enerji.
    std::vector<Alert> evaluate(const UavState& s, double return_energy_wh, const GeofenceSet& fences,
                                double sim_time_s);

    std::vector<AlertType> active(int uav_id) const;

    // İHA yeni görevine başlarken eski uyarı durumunu temizler.
    void reset(int uav_id);

private:
    Alert make(const UavState& s, AlertType type, Severity sev, std::string msg, double sim_time_s);

    std::map<int, std::set<AlertType>> active_;
    std::uint64_t next_seq_{1};
};

}  // namespace uavgcs
