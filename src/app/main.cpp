// İHA Yer Kontrol İstasyonu simülatörü: giriş noktası.
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "api/api.hpp"
#include "api/serialize.hpp"
#include "core/fleet.hpp"
#include "core/scenario.hpp"
#include "net/server.hpp"
#include "storage/telemetry_store.hpp"
#include "util/time.hpp"

using namespace uavgcs;

namespace {

std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop = true; }

std::string env_or(const char* name, const std::string& def) {
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : def;
}

struct Config {
    std::string bind_address = env_or("UAVGCS_BIND", "127.0.0.1");
    unsigned short port = static_cast<unsigned short>(std::stoi(env_or("UAVGCS_PORT", "8080")));
    double time_scale = std::stod(env_or("UAVGCS_TIME_SCALE", "10"));
    std::string cors_origin = env_or("UAVGCS_CORS_ORIGIN", "http://localhost:5173");
    std::string db_url = env_or("UAVGCS_DB_URL", "postgresql://postgres@localhost:5432/uavgcs");
    bool db_enabled = env_or("UAVGCS_DB", "on") != "off";
    std::string migrations_dir = env_or("UAVGCS_MIGRATIONS_DIR", "db/migrations");
};

constexpr auto kTick = std::chrono::milliseconds(100);       // simülasyon adımı (gerçek zaman)
constexpr auto kPersistEvery = std::chrono::milliseconds(1000);  // veritabanı örnekleme aralığı

TelemetrySample make_sample(const UavState& s, double sim_time_s) {
    TelemetrySample t;
    t.sortie_id = s.sortie_id;
    t.uav_id = s.id;
    t.callsign = s.callsign;
    t.uav_type = s.type;
    t.recorded_at = util::iso_utc_now();
    t.sim_time_s = sim_time_s;
    t.pos = s.pos;
    t.alt_m = s.alt_m;
    t.groundspeed_mps = s.groundspeed_mps;
    t.heading_deg = s.heading_deg;
    t.battery_pct = s.battery_pct;
    t.link_quality = s.link_quality;
    t.phase = s.phase;
    return t;
}

}  // namespace

int main() {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    try {
        const Config cfg;
        if (cfg.time_scale <= 0.0 || cfg.time_scale > 100.0) {
            std::cerr << "UAVGCS_TIME_SCALE 0 ile 100 arasında olmalı\n";
            return 2;
        }

        Fleet fleet;
        load_demo_scenario(fleet);

        std::unique_ptr<TelemetryStore> store;
        if (cfg.db_enabled) {
            try {
                store = TelemetryStore::open(cfg.db_url, cfg.migrations_dir);
                util::log_info("PostgreSQL bağlantısı hazır");
            } catch (const std::exception& e) {
                util::log_warn(std::string("veritabanı kullanılamıyor, kayıt yapılmayacak: ") + e.what());
            }
        }

        api::Api api(fleet, store.get(), cfg.time_scale);
        Server server(cfg.bind_address, cfg.port, [&api](const HttpRequest& r) { return api.handle(r); },
                      cfg.cors_origin);
        server.start();
        util::log_info("dinleniyor: http://" + cfg.bind_address + ":" + std::to_string(cfg.port) +
                       "  (WebSocket: /ws, zaman ölçeği x" + std::to_string(cfg.time_scale) + ")");

        const double dt_sim = std::chrono::duration<double>(kTick).count() * cfg.time_scale;
        auto next = std::chrono::steady_clock::now();
        auto last_persist = next;

        while (!g_stop) {
            next += kTick;

            const TickResult tick = fleet.tick(dt_sim);
            server.broadcast(api::telemetry_message(tick, cfg.time_scale).dump());
            for (const auto& a : tick.new_alerts) {
                server.broadcast(api::alert_message(a).dump());
                if (store) {
                    std::string sortie;
                    for (const auto& s : tick.states) {
                        if (s.id == a.uav_id) sortie = s.sortie_id;
                    }
                    store->enqueue(a, sortie);
                }
            }

            const auto now = std::chrono::steady_clock::now();
            if (store && now - last_persist >= kPersistEvery) {
                last_persist = now;
                for (const auto& s : tick.states) {
                    if (!s.sortie_id.empty() && (is_airborne(s.phase) || is_terminal(s.phase))) {
                        store->enqueue(make_sample(s, tick.sim_time_s));
                    }
                }
            }

            std::this_thread::sleep_until(next);
            // Gecikme biriktiyse (yavaş makine) yetişmeye çalışma, zaman çizgisini sıfırla.
            if (std::chrono::steady_clock::now() - next > std::chrono::seconds(1)) next = std::chrono::steady_clock::now();
        }

        util::log_info("kapatılıyor...");
        server.stop();
        return 0;
    } catch (const std::exception& e) {
        util::log_error(std::string("başlatılamadı: ") + e.what());
        return 1;
    }
}
