#include <gtest/gtest.h>

#include <set>

#include "core/uav.hpp"

using namespace uavgcs;

namespace {

const LatLon kHome{39.80, 32.80};

Uav make_uav(std::vector<Waypoint> mission, double battery_pct = 100.0, std::uint32_t seed = 1) {
    return Uav(1, "TEST-1", *spec_preset("MINI_RECON"), kHome, std::move(mission), seed, battery_pct);
}

// Simülasyonu en fazla `max_s` saniye veya bitiş fazına kadar ilerletir; görülen fazları toplar.
std::set<FlightPhase> run(Uav& u, double max_s, const Wind& wind = {}, double dt = 0.5) {
    std::set<FlightPhase> seen{u.phase()};
    for (double t = 0; t < max_s && !is_terminal(u.phase()); t += dt) {
        u.step(dt, wind);
        seen.insert(u.phase());
    }
    return seen;
}

}  // namespace

TEST(Uav, StaysOnGroundUntilStarted) {
    auto u = make_uav({{{39.85, 32.85}, 300.0, 0.0}});
    u.step(10.0, {});
    const auto s = u.state();
    EXPECT_EQ(s.phase, FlightPhase::Preflight);
    EXPECT_DOUBLE_EQ(s.alt_m, 0.0);
    EXPECT_DOUBLE_EQ(s.groundspeed_mps, 0.0);
    EXPECT_DOUBLE_EQ(s.battery_pct, 100.0);
}

TEST(Uav, StartOnlyFromPreflightOrLanded) {
    auto u = make_uav({{{39.85, 32.85}, 300.0, 0.0}});
    EXPECT_TRUE(u.start("SRT-1"));
    EXPECT_FALSE(u.start("SRT-2"));
    EXPECT_EQ(u.state().sortie_id, "SRT-1");
}

TEST(Uav, FullMissionVisitsAllPhasesAndLands) {
    auto u = make_uav({{{39.83, 32.83}, 200.0, 20.0}, {{39.85, 32.80}, 200.0, 0.0}});
    ASSERT_TRUE(u.start("SRT-1"));
    const auto seen = run(u, 3000.0);

    EXPECT_EQ(u.phase(), FlightPhase::Landed);
    for (auto p : {FlightPhase::Takeoff, FlightPhase::Climb, FlightPhase::Cruise, FlightPhase::Loiter,
                   FlightPhase::ReturnToLaunch, FlightPhase::Landing, FlightPhase::Landed}) {
        EXPECT_TRUE(seen.count(p)) << "faz görülmedi: " << to_string(p);
    }
    EXPECT_EQ(u.state().waypoint_index, 2u);
    EXPECT_NEAR(u.state().alt_m, 0.0, 1e-9);
    EXPECT_LT(u.state().distance_to_home_m, 1000.0);
}

TEST(Uav, BatteryNeverIncreasesWhileFlying) {
    auto u = make_uav({{{39.85, 32.85}, 300.0, 10.0}});
    ASSERT_TRUE(u.start("SRT-1"));
    double prev = u.state().battery_wh;
    for (int i = 0; i < 4000 && !is_terminal(u.phase()); ++i) {
        u.step(0.5, {});
        const double now = u.state().battery_wh;
        ASSERT_LE(now, prev);
        ASSERT_GE(now, 0.0);
        prev = now;
    }
    EXPECT_LT(prev, u.spec().battery_wh);
}

TEST(Uav, AltitudeFollowsMissionAndStaysNonNegative) {
    auto u = make_uav({{{39.90, 32.90}, 500.0, 0.0}});
    ASSERT_TRUE(u.start("SRT-1"));
    double max_alt = 0.0;
    for (int i = 0; i < 2000 && !is_terminal(u.phase()); ++i) {
        u.step(0.5, {});
        max_alt = std::max(max_alt, u.state().alt_m);
        ASSERT_GE(u.state().alt_m, 0.0);
    }
    EXPECT_NEAR(max_alt, 500.0, 5.0);
}

TEST(Uav, DepletedBatteryCrashes) {
    auto u = make_uav({{{39.90, 32.90}, 300.0, 0.0}}, 0.5);
    ASSERT_TRUE(u.start("SRT-1"));
    run(u, 3000.0);
    EXPECT_EQ(u.phase(), FlightPhase::Crashed);
    EXPECT_DOUBLE_EQ(u.state().alt_m, 0.0);
    EXPECT_DOUBLE_EQ(u.state().battery_wh, 0.0);
}

TEST(Uav, LinkLossTriggersFailsafeRtlAfterTimeout) {
    auto u = make_uav({{{39.95, 32.95}, 300.0, 0.0}});
    ASSERT_TRUE(u.start("SRT-1"));
    run(u, 120.0);  // havada, görev devam ediyor
    ASSERT_TRUE(u.phase() == FlightPhase::Cruise || u.phase() == FlightPhase::Climb);

    u.set_link_forced_down(true);
    u.step(u.spec().link_loss_timeout_s - 2.0, {});
    EXPECT_NE(u.phase(), FlightPhase::ReturnToLaunch);  // süre dolmadı
    EXPECT_FALSE(u.state().link_ok);

    u.step(4.0, {});
    EXPECT_EQ(u.phase(), FlightPhase::ReturnToLaunch);
    EXPECT_TRUE(u.state().failsafe_active);
}

TEST(Uav, LinkRecoveringBeforeTimeoutAvoidsFailsafe) {
    auto u = make_uav({{{39.95, 32.95}, 300.0, 0.0}});
    ASSERT_TRUE(u.start("SRT-1"));
    run(u, 60.0);
    u.set_link_forced_down(true);
    u.step(5.0, {});
    u.set_link_forced_down(false);
    u.step(30.0, {});
    EXPECT_NE(u.phase(), FlightPhase::ReturnToLaunch);
    EXPECT_FALSE(u.state().failsafe_active);
}

TEST(Uav, OutOfRangeLosesLink) {
    auto spec = *spec_preset("MINI_RECON");
    spec.link_range_m = 3000.0;
    Uav u(1, "TEST-1", spec, kHome, {{{40.00, 33.00}, 300.0, 0.0}}, 1);
    ASSERT_TRUE(u.start("SRT-1"));
    run(u, 1200.0);
    EXPECT_TRUE(u.state().failsafe_active);
}

TEST(Uav, ManualRtlOnlyWhenAirborneAndNotAlreadyReturning) {
    auto u = make_uav({{{39.90, 32.90}, 300.0, 0.0}});
    EXPECT_FALSE(u.return_to_launch());  // yerde
    ASSERT_TRUE(u.start("SRT-1"));
    run(u, 60.0);
    EXPECT_TRUE(u.return_to_launch());
    EXPECT_FALSE(u.return_to_launch());  // zaten RTL
}

TEST(Uav, TailwindIncreasesGroundspeedHeadwindDecreasesIt) {
    // Doğuya giden rota: rüzgar batıdan esiyorsa (from=270) arkadan, doğudan esiyorsa (from=90) karşıdan gelir.
    auto groundspeed_with = [](double from_deg) {
        Uav u(1, "T", *spec_preset("MINI_RECON"), kHome, {{{39.80, 33.30}, 300.0, 0.0}}, 1);
        u.start("S");
        const Wind w{8.0, from_deg, 0.0};
        for (int i = 0; i < 400; ++i) u.step(0.5, w);  // seyir hızına ulaş
        return u.state().groundspeed_mps;
    };
    const double tail = groundspeed_with(270.0);
    const double head = groundspeed_with(90.0);
    EXPECT_GT(tail, head + 10.0);
}

TEST(Uav, SameSeedGivesIdenticalTrajectoryWithGusts) {
    auto final_pos = [](std::uint32_t seed) {
        Uav u(1, "T", *spec_preset("MINI_RECON"), kHome, {{{39.90, 32.90}, 300.0, 0.0}}, seed);
        u.start("S");
        for (int i = 0; i < 300; ++i) u.step(0.5, Wind{5.0, 45.0, 3.0});
        return u.state().pos;
    };
    const auto a = final_pos(7), b = final_pos(7), c = final_pos(8);
    EXPECT_DOUBLE_EQ(a.lat, b.lat);
    EXPECT_DOUBLE_EQ(a.lon, b.lon);
    EXPECT_NE(a.lat, c.lat);
}

TEST(Uav, RestartAfterLandingRechargesBattery) {
    auto u = make_uav({{{39.82, 32.82}, 150.0, 0.0}});
    ASSERT_TRUE(u.start("SRT-1"));
    run(u, 3000.0);
    ASSERT_EQ(u.phase(), FlightPhase::Landed);
    EXPECT_LT(u.state().battery_pct, 100.0);

    ASSERT_TRUE(u.start("SRT-2"));
    EXPECT_DOUBLE_EQ(u.state().battery_pct, 100.0);
    EXPECT_EQ(u.state().sortie_id, "SRT-2");
}

TEST(Uav, ReturnEnergyGrowsWithDistanceFromHome) {
    auto u = make_uav({{{40.00, 33.00}, 300.0, 0.0}});
    const double at_home = u.return_energy_wh();
    ASSERT_TRUE(u.start("SRT-1"));
    run(u, 300.0);
    EXPECT_GT(u.return_energy_wh(), at_home);
}
