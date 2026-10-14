#include "core/uav.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace uavgcs {

using geo::deg2rad;
using geo::distance_m;
using geo::normalize_deg;

Uav::Uav(int id, std::string callsign, UavSpec spec, LatLon home, std::vector<Waypoint> mission,
         std::uint32_t seed, double initial_battery_pct)
    : id_(id),
      callsign_(std::move(callsign)),
      spec_(std::move(spec)),
      home_(home),
      mission_(std::move(mission)),
      rng_(seed),
      pos_(home),
      battery_wh_(spec_.battery_wh * std::clamp(initial_battery_pct, 0.0, 100.0) / 100.0) {}

bool Uav::start(const std::string& sortie_id) {
    if (phase_ != FlightPhase::Preflight && phase_ != FlightPhase::Landed) return false;
    if (phase_ == FlightPhase::Landed) battery_wh_ = spec_.battery_wh;  // batarya değişimi
    sortie_id_ = sortie_id;
    wp_index_ = 0;
    loiter_left_s_ = 0.0;
    failsafe_ = false;
    link_down_s_ = 0.0;
    heading_deg_ = mission_.empty() ? 0.0 : geo::bearing_deg(pos_, mission_.front().pos);
    phase_ = FlightPhase::Takeoff;
    return true;
}

bool Uav::return_to_launch() {
    switch (phase_) {
        case FlightPhase::Takeoff:
        case FlightPhase::Climb:
        case FlightPhase::Cruise:
        case FlightPhase::Loiter:
            enter_rtl(false);
            return true;
        default:
            return false;
    }
}

void Uav::enter_rtl(bool failsafe) {
    phase_ = FlightPhase::ReturnToLaunch;
    loiter_left_s_ = 0.0;
    if (failsafe) failsafe_ = true;
}

LatLon Uav::current_target() const {
    return wp_index_ < mission_.size() ? mission_[wp_index_].pos : home_;
}

void Uav::advance_waypoint() {
    ++wp_index_;
    if (wp_index_ >= mission_.size()) {
        enter_rtl(false);
    } else {
        phase_ = FlightPhase::Cruise;
    }
}

void Uav::update_link(double dt) {
    const double d = distance_m(pos_, home_);
    const bool out_of_range = d > spec_.link_range_m;
    link_ok_ = !link_forced_down_ && !out_of_range;
    if (link_forced_down_) {
        link_quality_ = 0.0;
    } else {
        const double r = std::min(1.0, d / spec_.link_range_m);
        link_quality_ = std::clamp(1.0 - r * r * r, 0.0, 1.0);
    }

    link_down_s_ = link_ok_ ? 0.0 : link_down_s_ + dt;

    const bool can_failsafe = phase_ == FlightPhase::Takeoff || phase_ == FlightPhase::Climb ||
                              phase_ == FlightPhase::Cruise || phase_ == FlightPhase::Loiter;
    if (!link_ok_ && can_failsafe && link_down_s_ >= spec_.link_loss_timeout_s) enter_rtl(true);
}

void Uav::step(double dt, const Wind& wind) {
    double remaining = dt;
    while (remaining > 1e-9) {
        const double h = std::min(remaining, kMaxSubstepS);
        step_once(h, wind);
        remaining -= h;
    }
}

void Uav::step_once(double dt, const Wind& wind) {
    if (!is_airborne(phase_)) {
        airspeed_mps_ = 0.0;
        groundspeed_mps_ = 0.0;
        vz_mps_ = 0.0;
        return;
    }

    update_link(dt);

    // --- Rehberlik: fazın hedef konumu, irtifası ve hızı ---
    LatLon target = current_target();
    double target_alt = alt_m_;
    double target_speed = spec_.cruise_speed_mps;
    bool steer = true;
    bool orbit = false;

    switch (phase_) {
        case FlightPhase::Takeoff:
            target_alt = kTakeoffAltM;
            target_speed = 0.6 * spec_.cruise_speed_mps;
            steer = false;  // pist yönünde kalkış
            if (alt_m_ >= kTakeoffAltM - 0.5) phase_ = FlightPhase::Climb;
            break;
        case FlightPhase::Climb:
            if (wp_index_ >= mission_.size()) {
                enter_rtl(false);
                break;
            }
            target_alt = mission_[wp_index_].alt_m;
            if (alt_m_ >= target_alt - 2.0) phase_ = FlightPhase::Cruise;
            break;
        case FlightPhase::Cruise:
            target_alt = mission_[wp_index_].alt_m;
            if (distance_m(pos_, target) < kAcceptRadiusM) {
                if (mission_[wp_index_].loiter_s > 0.0) {
                    loiter_left_s_ = mission_[wp_index_].loiter_s;
                    phase_ = FlightPhase::Loiter;
                } else {
                    advance_waypoint();
                }
            }
            break;
        case FlightPhase::Loiter:
            target_alt = mission_[wp_index_].alt_m;
            orbit = true;
            loiter_left_s_ -= dt;
            if (loiter_left_s_ <= 0.0) advance_waypoint();
            break;
        case FlightPhase::ReturnToLaunch:
            target = home_;
            target_alt = spec_.rtl_alt_m;
            if (distance_m(pos_, home_) < kAcceptRadiusM) phase_ = FlightPhase::Landing;
            break;
        case FlightPhase::Landing:
            target = home_;
            target_alt = 0.0;
            target_speed = 0.5 * spec_.cruise_speed_mps;
            break;
        default:
            break;
    }

    // --- Kinematik ---
    const double dv = target_speed - airspeed_mps_;
    airspeed_mps_ += std::clamp(dv, -kAccelMps2 * dt, kAccelMps2 * dt);

    if (orbit) {
        heading_deg_ = normalize_deg(heading_deg_ + geo::rad2deg(airspeed_mps_ / spec_.loiter_radius_m) * dt);
    } else if (steer) {
        const double diff = geo::signed_angle_diff_deg(heading_deg_, geo::bearing_deg(pos_, target));
        const double max_turn = spec_.max_turn_rate_deg_s * dt;
        heading_deg_ = normalize_deg(heading_deg_ + std::clamp(diff, -max_turn, max_turn));
    }

    const double desired_vz = (target_alt - alt_m_) / dt;
    vz_mps_ = std::clamp(desired_vz, -spec_.descent_rate_mps, spec_.climb_rate_mps);
    alt_m_ = std::max(0.0, alt_m_ + vz_mps_ * dt);

    double wind_speed = wind.speed_mps;
    if (wind.gust_mps > 0.0) {
        std::normal_distribution<double> gust(0.0, wind.gust_mps);
        wind_speed = std::max(0.0, wind_speed + gust(rng_));
    }
    // Rüzgar "from" yönünden eser; hareket yönü from+180'dir.
    const double wr = deg2rad(wind.from_deg);
    const double wn = -wind_speed * std::cos(wr);
    const double we = -wind_speed * std::sin(wr);

    const double hr = deg2rad(heading_deg_);
    const double vn = airspeed_mps_ * std::cos(hr) + wn;
    const double ve = airspeed_mps_ * std::sin(hr) + we;
    groundspeed_mps_ = std::hypot(vn, ve);
    pos_ = geo::offset_m(pos_, vn * dt, ve * dt);

    // --- Enerji ---
    const double power_w = spec_.base_power_w + spec_.drag_coeff * std::pow(airspeed_mps_, 3.0) +
                           spec_.climb_power_w_per_mps * std::max(0.0, vz_mps_);
    battery_wh_ = std::max(0.0, battery_wh_ - power_w * dt / 3600.0);

    if (battery_wh_ <= 0.0) {
        phase_ = FlightPhase::Crashed;
        alt_m_ = 0.0;
        airspeed_mps_ = groundspeed_mps_ = vz_mps_ = 0.0;
        return;
    }

    if (phase_ == FlightPhase::Landing && alt_m_ <= 0.0) {
        phase_ = FlightPhase::Landed;
        airspeed_mps_ = groundspeed_mps_ = vz_mps_ = 0.0;
    }
}

double Uav::return_energy_wh() const {
    const double v = spec_.cruise_speed_mps;
    const double cruise_power_w = spec_.base_power_w + spec_.drag_coeff * v * v * v;
    const double fly_s = distance_m(pos_, home_) / v;
    const double descent_s = alt_m_ / spec_.descent_rate_mps;
    // Konumdan eve seyir + alçalma sırasındaki taban güç; %25 emniyet çarpanı.
    return 1.25 * (cruise_power_w * fly_s + spec_.base_power_w * descent_s) / 3600.0;
}

UavState Uav::state() const {
    UavState s;
    s.id = id_;
    s.callsign = callsign_;
    s.type = spec_.type;
    s.sortie_id = sortie_id_;
    s.pos = pos_;
    s.home = home_;
    s.alt_m = alt_m_;
    s.groundspeed_mps = groundspeed_mps_;
    s.airspeed_mps = airspeed_mps_;
    s.vertical_speed_mps = vz_mps_;
    s.heading_deg = heading_deg_;
    s.battery_wh = battery_wh_;
    s.battery_pct = spec_.battery_wh > 0.0 ? 100.0 * battery_wh_ / spec_.battery_wh : 0.0;
    s.link_quality = link_quality_;
    s.link_ok = link_ok_;
    s.failsafe_active = failsafe_;
    s.phase = phase_;
    s.waypoint_index = std::min(wp_index_, mission_.size());
    s.waypoint_count = mission_.size();
    s.distance_to_home_m = distance_m(pos_, home_);
    return s;
}

}  // namespace uavgcs
