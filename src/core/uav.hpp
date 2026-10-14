#pragma once
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "core/types.hpp"

namespace uavgcs {

// Tek bir sabit kanatlı İHA'nın nokta-kütle uçuş modeli ve görev durum makinesi.
//
// Basitleştirmeler (bilinçli): rüzgar yer hızına vektörel eklenir ama "crab" düzeltmesi yoktur,
// yaklaşma/dönüş hız profili sabit ivmelidir, atmosfer yoğunluğu değişimi modellenmez.
class Uav {
public:
    static constexpr double kTakeoffAltM = 30.0;
    static constexpr double kAcceptRadiusM = 80.0;   // dönüş yarıçapından (v/ω ≈ 57 m) büyük olmalı
    static constexpr double kMaxSubstepS = 0.5;
    static constexpr double kAccelMps2 = 3.0;

    Uav(int id, std::string callsign, UavSpec spec, LatLon home, std::vector<Waypoint> mission,
        std::uint32_t seed = 1, double initial_battery_pct = 100.0);

    // Görevi başlatır (Preflight veya Landed'dan). Landed durumunda batarya yenilenir.
    bool start(const std::string& sortie_id);
    // Eve dönüş komutu; havadaki ve RTL/iniş fazında olmayan İHA için geçerlidir.
    bool return_to_launch();
    void set_link_forced_down(bool down) { link_forced_down_ = down; }

    // `dt` simülasyon saniyesi; içeride en fazla kMaxSubstepS'lik adımlara bölünür.
    void step(double dt, const Wind& wind);

    UavState state() const;
    // Mevcut konumdan eve dönüş için tahmini enerji (Wh), rüzgarsız.
    double return_energy_wh() const;

    int id() const { return id_; }
    const std::string& callsign() const { return callsign_; }
    const UavSpec& spec() const { return spec_; }
    FlightPhase phase() const { return phase_; }

private:
    void step_once(double dt, const Wind& wind);
    void update_link(double dt);
    void enter_rtl(bool failsafe);
    void advance_waypoint();
    LatLon current_target() const;

    int id_;
    std::string callsign_;
    UavSpec spec_;
    LatLon home_;
    std::vector<Waypoint> mission_;
    std::mt19937 rng_;

    std::string sortie_id_;
    FlightPhase phase_{FlightPhase::Preflight};
    LatLon pos_;
    double alt_m_{0.0};
    double heading_deg_{0.0};
    double airspeed_mps_{0.0};
    double vz_mps_{0.0};
    double groundspeed_mps_{0.0};
    double battery_wh_{0.0};
    std::size_t wp_index_{0};
    double loiter_left_s_{0.0};

    bool link_forced_down_{false};
    bool link_ok_{true};
    double link_quality_{1.0};
    double link_down_s_{0.0};
    bool failsafe_{false};
};

}  // namespace uavgcs
