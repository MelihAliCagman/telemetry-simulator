#include <gtest/gtest.h>

#include "core/alerts.hpp"
#include "core/fleet.hpp"

using namespace uavgcs;

namespace {

UavState flying_state() {
    UavState s;
    s.id = 1;
    s.callsign = "T-1";
    s.phase = FlightPhase::Cruise;
    s.alt_m = 300.0;
    s.battery_pct = 80.0;
    s.battery_wh = 320.0;
    s.pos = {39.8, 32.8};
    s.link_ok = true;
    return s;
}

bool has(const std::vector<Alert>& v, AlertType t) {
    for (const auto& a : v) {
        if (a.type == t) return true;
    }
    return false;
}

}  // namespace

TEST(AlertEngine, NoAlertsInNominalFlight) {
    AlertEngine e;
    EXPECT_TRUE(e.evaluate(flying_state(), 5.0, GeofenceSet{}, 1.0).empty());
}

TEST(AlertEngine, LowBatteryIsEdgeTriggered) {
    AlertEngine e;
    auto s = flying_state();
    s.battery_pct = 20.0;
    s.battery_wh = 80.0;
    EXPECT_TRUE(has(e.evaluate(s, 5.0, {}, 1.0), AlertType::LowBattery));
    EXPECT_TRUE(e.evaluate(s, 5.0, {}, 2.0).empty());  // tekrar üretmez
    s.battery_pct = 50.0;
    s.battery_wh = 200.0;
    e.evaluate(s, 5.0, {}, 3.0);                       // koşul bitti
    s.battery_pct = 20.0;
    EXPECT_TRUE(has(e.evaluate(s, 5.0, {}, 4.0), AlertType::LowBattery));  // yeniden tetiklenir
}

TEST(AlertEngine, CriticalBatteryAndSeverity) {
    AlertEngine e;
    auto s = flying_state();
    s.battery_pct = 5.0;
    s.battery_wh = 20.0;
    const auto a = e.evaluate(s, 5.0, {}, 1.0);
    EXPECT_TRUE(has(a, AlertType::CriticalBattery));
    for (const auto& x : a) {
        if (x.type == AlertType::CriticalBattery) {
            EXPECT_EQ(x.severity, Severity::Critical);
        }
    }
}

TEST(AlertEngine, NoAlertsOnGround) {
    AlertEngine e;
    auto s = flying_state();
    s.phase = FlightPhase::Preflight;
    s.battery_pct = 1.0;
    s.link_ok = false;
    EXPECT_TRUE(e.evaluate(s, 5.0, {}, 1.0).empty());
}

TEST(AlertEngine, LinkLost) {
    AlertEngine e;
    auto s = flying_state();
    s.link_ok = false;
    EXPECT_TRUE(has(e.evaluate(s, 5.0, {}, 1.0), AlertType::LinkLost));
}

TEST(AlertEngine, InsufficientReturnEnergy) {
    AlertEngine e;
    auto s = flying_state();
    s.battery_wh = 10.0;
    EXPECT_TRUE(has(e.evaluate(s, 20.0, {}, 1.0), AlertType::InsufficientReturnEnergy));
}

TEST(AlertEngine, NoFlyZoneBreach) {
    GeofenceSet fences;
    fences.add({"NFZ", ZoneKind::NoFly, {{39.7, 32.7}, {39.7, 32.9}, {39.9, 32.9}, {39.9, 32.7}}});
    AlertEngine e;
    auto s = flying_state();  // bölgenin içinde
    const auto a = e.evaluate(s, 5.0, fences, 1.0);
    ASSERT_TRUE(has(a, AlertType::GeofenceBreach));
    EXPECT_NE(a[0].message.find("NFZ"), std::string::npos);

    s.pos = {40.5, 33.5};
    EXPECT_TRUE(e.active(1).size() == 1);
    e.evaluate(s, 5.0, fences, 2.0);
    EXPECT_TRUE(e.active(1).empty());
}

TEST(AlertEngine, KeepInZoneBreachWhenOutside) {
    GeofenceSet fences;
    fences.add({"AO", ZoneKind::KeepIn, {{39.0, 32.0}, {39.0, 33.0}, {40.0, 33.0}, {40.0, 32.0}}});
    AlertEngine e;
    auto s = flying_state();
    s.pos = {39.5, 32.5};
    EXPECT_FALSE(has(e.evaluate(s, 5.0, fences, 1.0), AlertType::GeofenceBreach));
    s.pos = {41.0, 32.5};
    EXPECT_TRUE(has(e.evaluate(s, 5.0, fences, 2.0), AlertType::GeofenceBreach));
}

TEST(AlertEngine, GeofenceIgnoredNearGround) {
    GeofenceSet fences;
    fences.add({"NFZ", ZoneKind::NoFly, {{39.7, 32.7}, {39.7, 32.9}, {39.9, 32.9}, {39.9, 32.7}}});
    AlertEngine e;
    auto s = flying_state();
    s.alt_m = 2.0;
    EXPECT_FALSE(has(e.evaluate(s, 5.0, fences, 1.0), AlertType::GeofenceBreach));
}

TEST(AlertEngine, SequenceNumbersAreMonotonic) {
    AlertEngine e;
    auto s = flying_state();
    s.link_ok = false;
    s.battery_pct = 5.0;
    s.battery_wh = 20.0;
    const auto a = e.evaluate(s, 5.0, {}, 1.0);
    ASSERT_GE(a.size(), 2u);
    for (std::size_t i = 1; i < a.size(); ++i) EXPECT_GT(a[i].seq, a[i - 1].seq);
}
