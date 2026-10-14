#pragma once
#include "core/fleet.hpp"
#include "net/http_types.hpp"
#include "storage/telemetry_store.hpp"

namespace uavgcs::api {

// REST yönlendirmesi: ağ katmanından bağımsız, doğrudan test edilebilir.
//
//   GET  /api/health
//   GET  /api/uavs                 POST /api/uavs
//   GET  /api/uavs/{id}
//   POST /api/uavs/{id}/start      POST /api/uavs/{id}/rtl      POST /api/uavs/{id}/link
//   GET  /api/zones
//   GET  /api/alerts?limit=
//   GET  /api/wind                 POST /api/wind
//   GET  /api/sorties?limit=       GET  /api/sorties/{id}/track?max_points=
class Api {
public:
    // `store` null olabilir (veritabanı kapalı); geçmiş uç noktaları 503 döner.
    Api(Fleet& fleet, TelemetryStore* store, double time_scale);

    HttpResponse handle(const HttpRequest& req) const;

private:
    Fleet& fleet_;
    TelemetryStore* store_;
    double time_scale_;
};

}  // namespace uavgcs::api
