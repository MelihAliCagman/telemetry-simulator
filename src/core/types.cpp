#include "core/types.hpp"

namespace uavgcs {

const char* to_string(FlightPhase p) {
    switch (p) {
        case FlightPhase::Preflight: return "PREFLIGHT";
        case FlightPhase::Takeoff: return "TAKEOFF";
        case FlightPhase::Climb: return "CLIMB";
        case FlightPhase::Cruise: return "CRUISE";
        case FlightPhase::Loiter: return "LOITER";
        case FlightPhase::ReturnToLaunch: return "RTL";
        case FlightPhase::Landing: return "LANDING";
        case FlightPhase::Landed: return "LANDED";
        case FlightPhase::Crashed: return "CRASHED";
    }
    return "UNKNOWN";
}

bool is_airborne(FlightPhase p) {
    switch (p) {
        case FlightPhase::Takeoff:
        case FlightPhase::Climb:
        case FlightPhase::Cruise:
        case FlightPhase::Loiter:
        case FlightPhase::ReturnToLaunch:
        case FlightPhase::Landing:
            return true;
        default:
            return false;
    }
}

bool is_terminal(FlightPhase p) { return p == FlightPhase::Landed || p == FlightPhase::Crashed; }

const char* to_string(AlertType t) {
    switch (t) {
        case AlertType::LowBattery: return "LOW_BATTERY";
        case AlertType::CriticalBattery: return "CRITICAL_BATTERY";
        case AlertType::GeofenceBreach: return "GEOFENCE_BREACH";
        case AlertType::LinkLost: return "LINK_LOST";
        case AlertType::InsufficientReturnEnergy: return "INSUFFICIENT_RETURN_ENERGY";
    }
    return "UNKNOWN";
}

const char* to_string(Severity s) {
    switch (s) {
        case Severity::Info: return "INFO";
        case Severity::Warning: return "WARNING";
        case Severity::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

std::optional<UavSpec> spec_preset(std::string_view type) {
    UavSpec s;
    if (type == "MINI_RECON") {
        s.type = "MINI_RECON";
        return s;
    }
    if (type == "ENDURANCE") {
        s.type = "ENDURANCE";
        s.cruise_speed_mps = 30.0;
        s.climb_rate_mps = 5.0;
        s.max_alt_m = 6000.0;
        s.rtl_alt_m = 400.0;
        s.max_turn_rate_deg_s = 12.0;
        s.loiter_radius_m = 400.0;
        s.battery_wh = 1500.0;
        s.base_power_w = 180.0;
        s.drag_coeff = 0.004;
        s.climb_power_w_per_mps = 40.0;
        s.link_range_m = 80000.0;
        return s;
    }
    return std::nullopt;
}

}  // namespace uavgcs
