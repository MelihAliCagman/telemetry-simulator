#include "api/api.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <sstream>

#include "api/serialize.hpp"
#include "util/time.hpp"

namespace uavgcs::api {

namespace {

constexpr std::size_t kMaxWaypoints = 50;

HttpResponse reply(int status, const json& body) {
    HttpResponse r;
    r.status = status;
    r.body = body.dump();
    return r;
}

HttpResponse error(int status, const std::string& message) { return reply(status, {{"error", message}}); }

std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string item;
    while (std::getline(ss, item, '/')) {
        if (!item.empty()) parts.push_back(item);
    }
    return parts;
}

std::optional<int> parse_id(const std::string& s) {
    if (s.empty() || s.size() > 9 || !std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); }))
        return std::nullopt;
    return std::stoi(s);
}

std::size_t query_size(const HttpRequest& req, const char* key, std::size_t def, std::size_t max) {
    auto it = req.query.find(key);
    if (it == req.query.end()) return def;
    auto v = parse_id(it->second);
    if (!v || *v < 1) return def;
    return std::min<std::size_t>(static_cast<std::size_t>(*v), max);
}

// Sayı alanını okur; yoksa/yanlış tipteyse/aralık dışındaysa hata mesajı döner.
std::optional<std::string> read_number(const json& j, const char* key, double min, double max, double& out) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_number()) return std::string("'") + key + "' sayı olmalı";
    out = it->get<double>();
    if (!std::isfinite(out) || out < min || out > max) {
        std::ostringstream m;
        m << "'" << key << "' " << min << " ile " << max << " arasında olmalı";
        return m.str();
    }
    return std::nullopt;
}

std::optional<std::string> read_latlon(const json& j, LatLon& out) {
    if (!j.is_object()) return std::string("konum nesne olmalı");
    if (auto e = read_number(j, "lat", -90, 90, out.lat)) return e;
    return read_number(j, "lon", -180, 180, out.lon);
}

bool valid_callsign(const std::string& s) {
    return !s.empty() && s.size() <= 24 &&
           std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_'; });
}

}  // namespace

Api::Api(Fleet& fleet, TelemetryStore* store, double time_scale)
    : fleet_(fleet), store_(store), time_scale_(time_scale) {}

HttpResponse Api::handle(const HttpRequest& req) const {
    try {
        const auto parts = split_path(req.path);
        const bool get = req.method == "GET";
        const bool post = req.method == "POST";

        if (parts.size() < 2 || parts[0] != "api") return error(404, "Bulunamadı");
        const std::string& res = parts[1];

        if (res == "health" && parts.size() == 2) {
            if (!get) return error(405, "İzin verilmeyen yöntem");
            return reply(200, {{"status", "ok"},
                               {"uav_count", fleet_.snapshot().size()},
                               {"database", store_ != nullptr},
                               {"sim_time_s", fleet_.sim_time_s()},
                               {"time_scale", time_scale_}});
        }

        if (res == "uavs") {
            if (parts.size() == 2) {
                if (get) {
                    json arr = json::array();
                    for (const auto& s : fleet_.snapshot()) arr.push_back(to_json(s));
                    return reply(200, arr);
                }
                if (post) {
                    const json body = json::parse(req.body, nullptr, false);
                    if (body.is_discarded() || !body.is_object()) return error(400, "Geçersiz JSON gövdesi");

                    const std::string callsign = body.value("callsign", std::string());
                    if (!valid_callsign(callsign)) return error(400, "'callsign' 1-24 karakter, harf/rakam/-/_ olmalı");

                    auto spec = spec_preset(body.value("type", std::string("MINI_RECON")));
                    if (!spec) return error(400, "Bilinmeyen İHA türü (MINI_RECON veya ENDURANCE)");

                    LatLon home;
                    if (!body.contains("home")) return error(400, "'home' gerekli");
                    if (auto e = read_latlon(body["home"], home)) return error(400, "home: " + *e);

                    if (!body.contains("waypoints") || !body["waypoints"].is_array() || body["waypoints"].empty() ||
                        body["waypoints"].size() > kMaxWaypoints) {
                        return error(400, "'waypoints' 1-50 elemanlı dizi olmalı");
                    }
                    std::vector<Waypoint> mission;
                    for (const auto& w : body["waypoints"]) {
                        Waypoint wp;
                        if (auto e = read_latlon(w, wp.pos)) return error(400, "waypoint: " + *e);
                        if (auto e = read_number(w, "alt_m", 30.0, spec->max_alt_m, wp.alt_m)) return error(400, "waypoint: " + *e);
                        if (w.contains("loiter_s") && !w["loiter_s"].is_null()) {
                            if (auto e = read_number(w, "loiter_s", 0.0, 3600.0, wp.loiter_s)) return error(400, "waypoint: " + *e);
                        }
                        mission.push_back(wp);
                    }

                    double battery = 100.0;
                    if (body.contains("battery_pct")) {
                        if (auto e = read_number(body, "battery_pct", 1.0, 100.0, battery)) return error(400, *e);
                    }

                    const auto id = fleet_.add_uav(callsign, *spec, home, std::move(mission), battery);
                    if (!id) return error(409, "Filo kapasitesi dolu");
                    if (body.value("start", false)) fleet_.start(*id);
                    return reply(201, to_json(*fleet_.get(*id)));
                }
                return error(405, "İzin verilmeyen yöntem");
            }

            const auto id = parse_id(parts[2]);
            if (!id) return error(400, "Geçersiz İHA kimliği");
            const auto state = fleet_.get(*id);
            if (!state) return error(404, "İHA bulunamadı");

            if (parts.size() == 3) {
                if (!get) return error(405, "İzin verilmeyen yöntem");
                return reply(200, to_json(*state));
            }
            if (parts.size() == 4 && post) {
                const std::string& action = parts[3];
                if (action == "start") {
                    if (!fleet_.start(*id)) return error(409, "İHA şu an göreve başlayamaz (faz: " + std::string(to_string(state->phase)) + ")");
                    return reply(200, to_json(*fleet_.get(*id)));
                }
                if (action == "rtl") {
                    if (!fleet_.return_to_launch(*id)) return error(409, "Eve dönüş şu an verilemez (faz: " + std::string(to_string(state->phase)) + ")");
                    return reply(200, to_json(*fleet_.get(*id)));
                }
                if (action == "link") {
                    const json body = json::parse(req.body, nullptr, false);
                    if (body.is_discarded() || !body.is_object() || !body.contains("up") || !body["up"].is_boolean())
                        return error(400, "Gövde {\"up\": true|false} olmalı");
                    fleet_.set_link(*id, body["up"].get<bool>());
                    return reply(200, to_json(*fleet_.get(*id)));
                }
            }
            return error(404, "Bulunamadı");
        }

        if (res == "zones" && parts.size() == 2) {
            if (!get) return error(405, "İzin verilmeyen yöntem");
            json arr = json::array();
            for (const auto& z : fleet_.zones()) arr.push_back(to_json(z));
            return reply(200, arr);
        }

        if (res == "alerts" && parts.size() == 2) {
            if (!get) return error(405, "İzin verilmeyen yöntem");
            json arr = json::array();
            for (const auto& a : fleet_.recent_alerts(query_size(req, "limit", 50, 200))) arr.push_back(to_json(a));
            return reply(200, arr);
        }

        if (res == "wind" && parts.size() == 2) {
            if (get) return reply(200, to_json(fleet_.wind()));
            if (post) {
                const json body = json::parse(req.body, nullptr, false);
                if (body.is_discarded() || !body.is_object()) return error(400, "Geçersiz JSON gövdesi");
                Wind w;
                if (auto e = read_number(body, "speed_mps", 0, 40, w.speed_mps)) return error(400, *e);
                if (auto e = read_number(body, "from_deg", 0, 360, w.from_deg)) return error(400, *e);
                w.gust_mps = 0.0;
                if (body.contains("gust_mps")) {
                    if (auto e = read_number(body, "gust_mps", 0, 15, w.gust_mps)) return error(400, *e);
                }
                fleet_.set_wind(w);
                return reply(200, to_json(w));
            }
            return error(405, "İzin verilmeyen yöntem");
        }

        if (res == "sorties") {
            if (!get) return error(405, "İzin verilmeyen yöntem");
            if (!store_) return error(503, "Veritabanı devre dışı");

            if (parts.size() == 2) {
                json arr = json::array();
                for (const auto& s : store_->list_sorties(query_size(req, "limit", 50, 200))) arr.push_back(to_json(s));
                return reply(200, arr);
            }
            if (parts.size() == 4 && parts[3] == "track") {
                if (!store_->sortie_exists(parts[2])) return error(404, "Sortie bulunamadı");
                json arr = json::array();
                for (const auto& p : store_->track(parts[2], query_size(req, "max_points", 2000, 20000))) arr.push_back(to_json(p));
                return reply(200, arr);
            }
        }

        return error(404, "Bulunamadı");
    } catch (const json::exception&) {
        return error(400, "Geçersiz alan tipi");
    } catch (const std::exception& e) {
        util::log_error(std::string("istek işlenirken hata: ") + e.what());
        return error(500, "Sunucu hatası");
    }
}

}  // namespace uavgcs::api
