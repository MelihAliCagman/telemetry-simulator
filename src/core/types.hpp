#pragma once
// Alan modeli: İHA özellikleri, uçuş fazları, görev noktaları, uyarılar.
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/geo.hpp"

namespace uavgcs {

using geo::LatLon;

enum class FlightPhase {
    Preflight,       // yerde, görev bekliyor
    Takeoff,         // kalkış
    Climb,           // görev irtifasına tırmanış
    Cruise,          // görev noktaları arası seyir
    Loiter,          // nokta etrafında daire çizerek bekleme
    ReturnToLaunch,  // eve dönüş (RTL)
    Landing,         // iniş
    Landed,          // indi
    Crashed          // batarya bitti, kontrolsüz alçalma
};

const char* to_string(FlightPhase p);
bool is_airborne(FlightPhase p);   // Takeoff..Landing
bool is_terminal(FlightPhase p);   // Landed, Crashed

enum class AlertType {
    LowBattery,
    CriticalBattery,
    GeofenceBreach,
    LinkLost,
    InsufficientReturnEnergy
};

enum class Severity { Info, Warning, Critical };

const char* to_string(AlertType t);
const char* to_string(Severity s);

struct Waypoint {
    LatLon pos;
    double alt_m{300.0};     // yerden yükseklik (AGL)
    double loiter_s{0.0};    // > 0 ise noktada bu kadar saniye daire çizer
};

// Rüzgar: `from_deg` rüzgarın estiği yön (meteorolojik gösterim), `gust_mps` ani esinti standart sapması.
struct Wind {
    double speed_mps{0.0};
    double from_deg{0.0};
    double gust_mps{0.0};
};

// Hava aracı performans modeli. Değerler genel amaçlı küçük/orta sınıf İHA için temsilidir.
struct UavSpec {
    std::string type{"MINI_RECON"};
    double cruise_speed_mps{20.0};
    double climb_rate_mps{4.0};
    double descent_rate_mps{3.0};
    double max_alt_m{3000.0};
    double rtl_alt_m{150.0};
    double max_turn_rate_deg_s{20.0};
    double loiter_radius_m{150.0};
    double battery_wh{400.0};
    double base_power_w{50.0};              // aviyonik + yük + sürtünmesiz taban güç
    double drag_coeff{0.006};               // W / (m/s)^3
    double climb_power_w_per_mps{15.0};     // tırmanış için ek güç
    double link_range_m{20000.0};           // yer istasyonu veri bağı menzili
    double link_loss_timeout_s{15.0};       // bu süre bağ yoksa otomatik RTL
};

// "MINI_RECON" veya "ENDURANCE" ön ayarları; bilinmeyen tür için std::nullopt.
std::optional<UavSpec> spec_preset(std::string_view type);

struct Alert {
    std::uint64_t seq{0};
    int uav_id{0};
    std::string callsign;
    AlertType type{AlertType::LowBattery};
    Severity severity{Severity::Info};
    std::string message;
    double sim_time_s{0.0};
    std::string raised_at;  // ISO-8601 UTC
};

// Bir İHA'nın dışarıya açık anlık görüntüsü.
struct UavState {
    int id{0};
    std::string callsign;
    std::string type;
    std::string sortie_id;       // görev (sortie) kimliği, yerde bekliyorsa boş
    LatLon pos;
    LatLon home;
    double alt_m{0.0};
    double groundspeed_mps{0.0};
    double airspeed_mps{0.0};
    double vertical_speed_mps{0.0};
    double heading_deg{0.0};
    double battery_pct{100.0};
    double battery_wh{0.0};
    double link_quality{1.0};    // 0..1
    bool link_ok{true};
    bool failsafe_active{false};
    FlightPhase phase{FlightPhase::Preflight};
    std::size_t waypoint_index{0};
    std::size_t waypoint_count{0};
    double distance_to_home_m{0.0};
    std::vector<AlertType> active_alerts;
};

}  // namespace uavgcs
