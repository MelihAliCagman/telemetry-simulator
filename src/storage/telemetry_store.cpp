#include "storage/telemetry_store.hpp"

#include <pqxx/pqxx>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

#include "util/time.hpp"

namespace fs = std::filesystem;

// exec_params libpqxx 7.10'da "deprecated" ama 7.8 (Ubuntu 24.04) sürümünde exec(zview, params) yok;
// iki sürümde de derlenebilmesi için uyarıyı bu dosyada bilerek kapatıyoruz.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace uavgcs {

namespace {

constexpr std::size_t kMaxQueued = 50000;
constexpr auto kFlushInterval = std::chrono::milliseconds(500);

const char* kIso = "'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"'";

void run_migrations(pqxx::connection& conn, const std::string& dir) {
    {
        pqxx::work tx{conn};
        tx.exec(
            "CREATE TABLE IF NOT EXISTS schema_migrations ("
            " version TEXT PRIMARY KEY,"
            " applied_at TIMESTAMPTZ NOT NULL DEFAULT now())");
        tx.commit();
    }

    if (!fs::is_directory(dir)) throw std::runtime_error("göç dizini bulunamadı: " + dir);

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".sql") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files) {
        const std::string version = file.filename().string();
        pqxx::work tx{conn};
        if (!tx.exec_params("SELECT 1 FROM schema_migrations WHERE version = $1", version).empty()) continue;

        std::ifstream in(file, std::ios::binary);
        std::ostringstream sql;
        sql << in.rdbuf();
        tx.exec(sql.str());
        tx.exec_params("INSERT INTO schema_migrations (version) VALUES ($1)", version);
        tx.commit();
        util::log_info("veritabanı göçü uygulandı: " + version);
    }
}

}  // namespace

TelemetryStore::TelemetryStore(const std::string& conninfo) : conninfo_(conninfo) {}

std::unique_ptr<TelemetryStore> TelemetryStore::open(const std::string& conninfo, const std::string& migrations_dir) {
    std::unique_ptr<TelemetryStore> store(new TelemetryStore(conninfo));
    store->writer_conn_ = std::make_unique<pqxx::connection>(conninfo);
    store->reader_conn_ = std::make_unique<pqxx::connection>(conninfo);
    run_migrations(*store->writer_conn_, migrations_dir);
    store->writer_ = std::thread([s = store.get()] { s->writer_loop(); });
    return store;
}

TelemetryStore::~TelemetryStore() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    queue_cv_.notify_all();
    if (writer_.joinable()) writer_.join();
    try {
        flush();
    } catch (const std::exception& e) {
        util::log_error(std::string("kapanışta son yazma başarısız: ") + e.what());
    }
}

void TelemetryStore::enqueue(TelemetrySample sample) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (samples_.size() >= kMaxQueued) samples_.erase(samples_.begin(), samples_.begin() + kMaxQueued / 10);
    samples_.push_back(std::move(sample));
}

void TelemetryStore::enqueue(const Alert& alert, const std::string& sortie_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (alerts_.size() >= kMaxQueued) alerts_.erase(alerts_.begin(), alerts_.begin() + kMaxQueued / 10);
    alerts_.push_back({alert, sortie_id});
}

void TelemetryStore::writer_loop() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    while (!stop_) {
        queue_cv_.wait_for(lock, kFlushInterval);
        if (stop_) break;
        lock.unlock();
        try {
            flush();
        } catch (const std::exception& e) {
            util::log_error(std::string("telemetri yazılamadı: ") + e.what());
        }
        lock.lock();
    }
}

void TelemetryStore::flush() {
    std::lock_guard<std::mutex> write_lock(write_mutex_);
    std::vector<TelemetrySample> samples;
    std::vector<AlertRow> alerts;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        samples.swap(samples_);
        alerts.swap(alerts_);
    }
    if (samples.empty() && alerts.empty()) return;

    try {
        write_batch(samples, alerts);
    } catch (const pqxx::broken_connection&) {
        // Bağlantı koptu: bir kez yeniden bağlanıp aynı kümeyi tekrar dene.
        writer_conn_ = std::make_unique<pqxx::connection>(conninfo_);
        write_batch(samples, alerts);
    }
}

void TelemetryStore::write_batch(std::vector<TelemetrySample>& samples, std::vector<AlertRow>& alerts) {
    pqxx::work tx{*writer_conn_};

    std::set<std::string> known;
    for (const auto& s : samples) {
        if (!known.insert(s.sortie_id).second) continue;
        tx.exec_params(
            "INSERT INTO sortie (id, uav_id, callsign, uav_type, started_at) VALUES ($1, $2, $3, $4, $5) "
            "ON CONFLICT (id) DO NOTHING",
            s.sortie_id, s.uav_id, s.callsign, s.uav_type, s.recorded_at);
    }
    for (const auto& s : samples) {
        if (is_terminal(s.phase)) {
            tx.exec_params("UPDATE sortie SET ended_at = $2, final_phase = $3 WHERE id = $1", s.sortie_id,
                           s.recorded_at, std::string(to_string(s.phase)));
        }
    }

    if (!samples.empty()) {
        auto stream = pqxx::stream_to::table(
            tx, {"telemetry"},
            {"sortie_id", "recorded_at", "sim_time_s", "latitude", "longitude", "altitude_m", "groundspeed_mps",
             "heading_deg", "battery_pct", "link_quality", "phase"});
        for (const auto& s : samples) {
            stream.write_values(s.sortie_id, s.recorded_at, s.sim_time_s, s.pos.lat, s.pos.lon, s.alt_m,
                                s.groundspeed_mps, s.heading_deg, s.battery_pct, s.link_quality,
                                std::string(to_string(s.phase)));
        }
        stream.complete();
    }

    for (const auto& row : alerts) {
        const auto& a = row.alert;
        if (row.sortie_id.empty()) {
            tx.exec_params(
                "INSERT INTO alert (sortie_id, uav_id, callsign, type, severity, message, sim_time_s, raised_at) "
                "VALUES (NULL, $1, $2, $3, $4, $5, $6, $7)",
                a.uav_id, a.callsign, std::string(to_string(a.type)), std::string(to_string(a.severity)), a.message,
                a.sim_time_s, a.raised_at);
        } else {
            tx.exec_params(
                "INSERT INTO alert (sortie_id, uav_id, callsign, type, severity, message, sim_time_s, raised_at) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)",
                row.sortie_id, a.uav_id, a.callsign, std::string(to_string(a.type)),
                std::string(to_string(a.severity)), a.message, a.sim_time_s, a.raised_at);
        }
    }
    tx.commit();
}

std::vector<SortieInfo> TelemetryStore::list_sorties(std::size_t limit) const {
    std::lock_guard<std::mutex> lock(reader_mutex_);
    pqxx::read_transaction tx{*reader_conn_};
    const std::string sql =
        std::string("SELECT s.id, s.uav_id, s.callsign, s.uav_type, "
                    "to_char(s.started_at AT TIME ZONE 'UTC', ") + kIso + "), "
        "COALESCE(to_char(s.ended_at AT TIME ZONE 'UTC', " + kIso + "), ''), "
        "COALESCE(s.final_phase, ''), "
        "(SELECT count(*) FROM telemetry t WHERE t.sortie_id = s.id) "
        "FROM sortie s ORDER BY s.started_at DESC LIMIT $1";
    std::vector<SortieInfo> out;
    for (const auto& row : tx.exec_params(sql, static_cast<long long>(limit))) {
        SortieInfo i;
        i.id = row[0].as<std::string>();
        i.uav_id = row[1].as<int>();
        i.callsign = row[2].as<std::string>();
        i.uav_type = row[3].as<std::string>();
        i.started_at = row[4].as<std::string>();
        i.ended_at = row[5].as<std::string>();
        i.final_phase = row[6].as<std::string>();
        i.sample_count = row[7].as<long long>();
        out.push_back(std::move(i));
    }
    return out;
}

bool TelemetryStore::sortie_exists(const std::string& id) const {
    std::lock_guard<std::mutex> lock(reader_mutex_);
    pqxx::read_transaction tx{*reader_conn_};
    return !tx.exec_params("SELECT 1 FROM sortie WHERE id = $1", id).empty();
}

std::vector<TrackPoint> TelemetryStore::track(const std::string& sortie_id, std::size_t max_points) const {
    std::lock_guard<std::mutex> lock(reader_mutex_);
    pqxx::read_transaction tx{*reader_conn_};
    const std::string sql =
        std::string("SELECT to_char(recorded_at AT TIME ZONE 'UTC', ") + kIso +
        "), latitude, longitude, altitude_m, groundspeed_mps, battery_pct, phase "
        "FROM telemetry WHERE sortie_id = $1 ORDER BY recorded_at, id LIMIT 200000";
    std::vector<TrackPoint> all;
    for (const auto& row : tx.exec_params(sql, sortie_id)) {
        TrackPoint p;
        p.recorded_at = row[0].as<std::string>();
        p.lat = row[1].as<double>();
        p.lon = row[2].as<double>();
        p.alt_m = row[3].as<double>();
        p.groundspeed_mps = row[4].as<double>();
        p.battery_pct = row[5].as<double>();
        p.phase = row[6].as<std::string>();
        all.push_back(std::move(p));
    }
    if (max_points == 0 || all.size() <= max_points) return all;

    // Eşit aralıklı seyreltme; ilk ve son nokta korunur.
    std::vector<TrackPoint> out;
    out.reserve(max_points);
    const double step = static_cast<double>(all.size() - 1) / static_cast<double>(max_points - 1);
    for (std::size_t i = 0; i < max_points; ++i) out.push_back(all[static_cast<std::size_t>(i * step + 0.5)]);
    return out;
}

}  // namespace uavgcs
