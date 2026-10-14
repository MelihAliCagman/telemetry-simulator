#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "api/api.hpp"
#include "core/scenario.hpp"

using namespace uavgcs;
using nlohmann::json;

namespace {

struct Harness {
    Fleet fleet{1};
    api::Api api{fleet, nullptr, 10.0};

    Harness() { load_demo_scenario(fleet); }

    HttpResponse call(const std::string& method, const std::string& path, const std::string& body = "",
                      std::map<std::string, std::string> query = {}) {
        HttpRequest r;
        r.method = method;
        r.path = path;
        r.body = body;
        r.query = std::move(query);
        return api.handle(r);
    }
};

const char* kValidUav = R"({"callsign":"YENI-4","type":"MINI_RECON","home":{"lat":39.8,"lon":32.8},
  "waypoints":[{"lat":39.85,"lon":32.85,"alt_m":300,"loiter_s":10}]})";

}  // namespace

TEST(Api, HealthReportsStateAndNoDatabase) {
    Harness h;
    const auto r = h.call("GET", "/api/health");
    EXPECT_EQ(r.status, 200);
    const auto j = json::parse(r.body);
    EXPECT_EQ(j["status"], "ok");
    EXPECT_EQ(j["uav_count"], 3);
    EXPECT_EQ(j["database"], false);
}

TEST(Api, ListsDemoUavs) {
    Harness h;
    const auto j = json::parse(h.call("GET", "/api/uavs").body);
    ASSERT_EQ(j.size(), 3u);
    EXPECT_EQ(j[0]["callsign"], "KARTAL-1");
    EXPECT_EQ(j[0]["phase"], "PREFLIGHT");
}

TEST(Api, UnknownRoutesAndMethods) {
    Harness h;
    EXPECT_EQ(h.call("GET", "/api/nope").status, 404);
    EXPECT_EQ(h.call("GET", "/foo").status, 404);
    EXPECT_EQ(h.call("DELETE", "/api/uavs").status, 405);
    EXPECT_EQ(h.call("POST", "/api/health").status, 405);
}

TEST(Api, GetSingleUavValidatesId) {
    Harness h;
    EXPECT_EQ(h.call("GET", "/api/uavs/1").status, 200);
    EXPECT_EQ(h.call("GET", "/api/uavs/99").status, 404);
    EXPECT_EQ(h.call("GET", "/api/uavs/abc").status, 400);
    EXPECT_EQ(h.call("GET", "/api/uavs/-1").status, 400);
}

TEST(Api, StartThenConflictOnSecondStart) {
    Harness h;
    const auto first = h.call("POST", "/api/uavs/1/start");
    EXPECT_EQ(first.status, 200);
    EXPECT_EQ(json::parse(first.body)["phase"], "TAKEOFF");
    EXPECT_EQ(h.call("POST", "/api/uavs/1/start").status, 409);
}

TEST(Api, RtlRequiresFlight) {
    Harness h;
    EXPECT_EQ(h.call("POST", "/api/uavs/1/rtl").status, 409);
    h.call("POST", "/api/uavs/1/start");
    for (int i = 0; i < 100; ++i) h.fleet.tick(1.0);
    EXPECT_EQ(h.call("POST", "/api/uavs/1/rtl").status, 200);
}

TEST(Api, LinkToggleValidatesBody) {
    Harness h;
    EXPECT_EQ(h.call("POST", "/api/uavs/1/link", "{\"up\":false}").status, 200);
    EXPECT_EQ(h.call("POST", "/api/uavs/1/link", "{\"up\":\"no\"}").status, 400);
    EXPECT_EQ(h.call("POST", "/api/uavs/1/link", "not json").status, 400);
}

TEST(Api, CreateUavSuccess) {
    Harness h;
    const auto r = h.call("POST", "/api/uavs", kValidUav);
    ASSERT_EQ(r.status, 201) << r.body;
    const auto j = json::parse(r.body);
    EXPECT_EQ(j["id"], 4);
    EXPECT_EQ(j["callsign"], "YENI-4");
    EXPECT_EQ(j["waypoint_count"], 1);
}

TEST(Api, CreateUavRejectsBadInput) {
    Harness h;
    auto bad = [&](json j) { return h.call("POST", "/api/uavs", j.dump()).status; };
    auto base = [] { return json::parse(kValidUav); };

    EXPECT_EQ(h.call("POST", "/api/uavs", "{").status, 400);
    EXPECT_EQ(h.call("POST", "/api/uavs", "[]").status, 400);

    auto j = base(); j["callsign"] = "has space";           EXPECT_EQ(bad(j), 400);
    j = base();      j["callsign"] = std::string(30, 'A'); EXPECT_EQ(bad(j), 400);
    j = base();      j["callsign"] = 5;                    EXPECT_EQ(bad(j), 400);
    j = base();      j["type"] = "TANK";                   EXPECT_EQ(bad(j), 400);
    j = base();      j["home"]["lat"] = 123.0;             EXPECT_EQ(bad(j), 400);
    j = base();      j["waypoints"] = json::array();       EXPECT_EQ(bad(j), 400);
    j = base();      j["waypoints"][0]["alt_m"] = 99999;   EXPECT_EQ(bad(j), 400);
    j = base();      j["waypoints"][0]["lon"] = "x";       EXPECT_EQ(bad(j), 400);
    j = base();      j["battery_pct"] = 0;                 EXPECT_EQ(bad(j), 400);
    j = base();      j.erase("home");                      EXPECT_EQ(bad(j), 400);

    EXPECT_EQ(json::parse(h.call("GET", "/api/health").body)["uav_count"], 3);  // hiçbiri eklenmedi
}

TEST(Api, CreateUavCanAutoStart) {
    Harness h;
    auto j = json::parse(kValidUav);
    j["start"] = true;
    const auto r = h.call("POST", "/api/uavs", j.dump());
    ASSERT_EQ(r.status, 201);
    EXPECT_EQ(json::parse(r.body)["phase"], "TAKEOFF");
}

TEST(Api, ZonesAndAlerts) {
    Harness h;
    const auto zones = json::parse(h.call("GET", "/api/zones").body);
    ASSERT_EQ(zones.size(), 1u);
    EXPECT_EQ(zones[0]["kind"], "NO_FLY");
    EXPECT_EQ(zones[0]["polygon"].size(), 4u);
    EXPECT_TRUE(json::parse(h.call("GET", "/api/alerts").body).empty());
}

TEST(Api, WindRoundTripAndValidation) {
    Harness h;
    EXPECT_EQ(h.call("POST", "/api/wind", R"({"speed_mps":12,"from_deg":90,"gust_mps":2})").status, 200);
    const auto w = json::parse(h.call("GET", "/api/wind").body);
    EXPECT_EQ(w["speed_mps"], 12.0);
    EXPECT_EQ(w["from_deg"], 90.0);
    EXPECT_EQ(h.call("POST", "/api/wind", R"({"speed_mps":-1,"from_deg":90})").status, 400);
    EXPECT_EQ(h.call("POST", "/api/wind", R"({"speed_mps":500,"from_deg":90})").status, 400);
    EXPECT_EQ(h.call("POST", "/api/wind", R"({"from_deg":90})").status, 400);
}

TEST(Api, HistoryEndpointsReturn503WithoutDatabase) {
    Harness h;
    EXPECT_EQ(h.call("GET", "/api/sorties").status, 503);
    EXPECT_EQ(h.call("GET", "/api/sorties/SRT-1/track").status, 503);
}

TEST(Api, AlertsEndpointReflectsFleetAlerts) {
    Harness h;
    h.fleet.set_wind({});
    h.call("POST", "/api/uavs/2/start");  // SAHIN-2: yasak bölgeyi keser
    for (int i = 0; i < 3000; ++i) h.fleet.tick(1.0);
    const auto alerts = json::parse(h.call("GET", "/api/alerts", "", {{"limit", "10"}}).body);
    bool found = false;
    for (const auto& a : alerts) found |= a["type"] == "GEOFENCE_BREACH";
    EXPECT_TRUE(found);
}
