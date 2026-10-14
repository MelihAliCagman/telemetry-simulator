#pragma once
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/types.hpp"

namespace pqxx {
class connection;
}

namespace uavgcs {

struct TelemetrySample {
    std::string sortie_id;
    int uav_id{0};
    std::string callsign;
    std::string uav_type;
    std::string recorded_at;  // ISO-8601 UTC
    double sim_time_s{0.0};
    LatLon pos;
    double alt_m{0.0};
    double groundspeed_mps{0.0};
    double heading_deg{0.0};
    double battery_pct{0.0};
    double link_quality{0.0};
    FlightPhase phase{FlightPhase::Preflight};
};

struct SortieInfo {
    std::string id;
    int uav_id{0};
    std::string callsign;
    std::string uav_type;
    std::string started_at;
    std::string ended_at;      // boş olabilir
    std::string final_phase;   // boş olabilir
    long long sample_count{0};
};

struct TrackPoint {
    std::string recorded_at;
    double lat{0}, lon{0}, alt_m{0}, groundspeed_mps{0}, battery_pct{0};
    std::string phase;
};

// PostgreSQL'e telemetri ve uyarı yazan/okuyan katman.
//
// Yazmalar bir kuyruğa alınır; arka plan iş parçacığı bunları periyodik olarak tek işlemde (COPY ile)
// toplu yazar, böylece simülasyon döngüsü veritabanı gecikmesinden etkilenmez.
class TelemetryStore {
public:
    // Bağlanır ve `migrations_dir` içindeki bekleyen SQL göçlerini uygular. Başarısızlıkta std::exception fırlatır.
    static std::unique_ptr<TelemetryStore> open(const std::string& conninfo, const std::string& migrations_dir);

    ~TelemetryStore();
    TelemetryStore(const TelemetryStore&) = delete;
    TelemetryStore& operator=(const TelemetryStore&) = delete;

    void enqueue(TelemetrySample sample);
    void enqueue(const Alert& alert, const std::string& sortie_id);

    std::vector<SortieInfo> list_sorties(std::size_t limit) const;
    bool sortie_exists(const std::string& id) const;
    std::vector<TrackPoint> track(const std::string& sortie_id, std::size_t max_points) const;

    // Kuyruktaki her şeyi yazar (test ve kapanış için).
    void flush();

private:
    struct AlertRow {
        Alert alert;
        std::string sortie_id;
    };

    TelemetryStore(const std::string& conninfo);
    void writer_loop();
    void write_batch(std::vector<TelemetrySample>& samples, std::vector<AlertRow>& alerts);

    std::string conninfo_;
    std::unique_ptr<pqxx::connection> writer_conn_;
    std::unique_ptr<pqxx::connection> reader_conn_;
    mutable std::mutex reader_mutex_;
    std::mutex write_mutex_;  // writer_conn_ erişimini (arka plan + flush) sıralar

    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::vector<TelemetrySample> samples_;
    std::vector<AlertRow> alerts_;
    bool stop_{false};
    std::thread writer_;
};

}  // namespace uavgcs
