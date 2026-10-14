#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include "core/fleet.hpp"
#include "core/scenario.hpp"

using namespace uavgcs;

namespace {
const LatLon kBase{39.80, 32.80};
}

TEST(Fleet, AddsUavsWithSequentialIdsAndRespectsLimit) {
    Fleet f(1);
    for (std::size_t i = 0; i < Fleet::kMaxUavs; ++i) {
        const auto id = f.add_uav("U", *spec_preset("MINI_RECON"), kBase, {{{39.9, 32.9}, 300.0, 0.0}});
        ASSERT_TRUE(id.has_value());
        EXPECT_EQ(*id, static_cast<int>(i) + 1);
    }
    EXPECT_FALSE(f.add_uav("U", *spec_preset("MINI_RECON"), kBase, {}).has_value());
}

TEST(Fleet, UnknownIdsAreRejected) {
    Fleet f(1);
    EXPECT_FALSE(f.start(42));
    EXPECT_FALSE(f.return_to_launch(42));
    EXPECT_FALSE(f.set_link(42, false));
    EXPECT_FALSE(f.get(42).has_value());
}

TEST(Fleet, StartAssignsSortieIdOnce) {
    Fleet f(1);
    const int id = *f.add_uav("U", *spec_preset("MINI_RECON"), kBase, {{{39.9, 32.9}, 300.0, 0.0}});
    EXPECT_TRUE(f.get(id)->sortie_id.empty());
    ASSERT_TRUE(f.start(id));
    const auto sortie = f.get(id)->sortie_id;
    EXPECT_EQ(sortie.rfind("SRT-", 0), 0u);
    EXPECT_FALSE(f.start(id));
    EXPECT_EQ(f.get(id)->sortie_id, sortie);
}

TEST(Fleet, TickAdvancesSimTimeAndReportsAllUavs) {
    Fleet f(1);
    f.add_uav("A", *spec_preset("MINI_RECON"), kBase, {{{39.9, 32.9}, 300.0, 0.0}});
    f.add_uav("B", *spec_preset("MINI_RECON"), kBase, {{{39.9, 32.9}, 300.0, 0.0}});
    const auto r = f.tick(1.0);
    EXPECT_DOUBLE_EQ(r.sim_time_s, 1.0);
    EXPECT_EQ(r.states.size(), 2u);
    EXPECT_DOUBLE_EQ(f.sim_time_s(), 1.0);
}

TEST(Fleet, GeofenceBreachRaisesAlertAndForcesRtl) {
    Fleet f(1);
    f.set_wind({});
    f.add_zone({"NFZ", ZoneKind::NoFly, {{39.83, 32.94}, {39.83, 33.00}, {39.89, 33.00}, {39.89, 32.94}}});
    const int id = *f.add_uav("U", *spec_preset("MINI_RECON"), kBase,
                              {{{39.84, 32.90}, 280.0, 0.0}, {{39.86, 33.04}, 280.0, 0.0}});
    ASSERT_TRUE(f.start(id));

    bool breach_seen = false;
    int breach_count = 0;
    for (int i = 0; i < 3000 && !is_terminal(f.get(id)->phase); ++i) {
        for (const auto& a : f.tick(1.0).new_alerts) {
            if (a.type == AlertType::GeofenceBreach) {
                breach_seen = true;
                ++breach_count;
                EXPECT_EQ(a.callsign, "U");
            }
        }
    }
    EXPECT_TRUE(breach_seen);
    EXPECT_EQ(breach_count, 1);  // kenar tetiklemeli: tek uyarı
    EXPECT_EQ(f.get(id)->phase, FlightPhase::Landed);
    EXPECT_FALSE(f.recent_alerts(10).empty());
}

TEST(Fleet, LowEnergyStartForcesEarlyReturn) {
    Fleet f(1);
    f.set_wind({});
    // %20 batarya ile uzak bir hedef: dönüş enerjisi yetmeyecek, İHA hedefe varmadan RTL'e geçmeli.
    auto spec = *spec_preset("MINI_RECON");
    spec.link_range_m = 200000.0;  // bağ menzili devre dışı kalmasın, yalnız enerji kuralı sınansın
    const int id = *f.add_uav("U", spec, kBase, {{{40.20, 33.40}, 300.0, 0.0}}, 20.0);
    ASSERT_TRUE(f.start(id));
    bool energy_alert = false;
    for (int i = 0; i < 4000 && !is_terminal(f.get(id)->phase); ++i) {
        for (const auto& a : f.tick(1.0).new_alerts) {
            energy_alert |= a.type == AlertType::InsufficientReturnEnergy;
        }
    }
    EXPECT_TRUE(energy_alert);
    EXPECT_EQ(f.get(id)->phase, FlightPhase::Landed);
    EXPECT_EQ(f.get(id)->waypoint_index, 0u);
}

TEST(Fleet, RecentAlertsAreCapped) {
    Fleet f(1);
    const int id = *f.add_uav("U", *spec_preset("MINI_RECON"), kBase, {{{39.9, 32.9}, 300.0, 0.0}});
    f.start(id);
    for (int i = 0; i < 200; ++i) f.tick(1.0);
    EXPECT_LE(f.recent_alerts(1000).size(), Fleet::kMaxRecentAlerts);
}

TEST(Fleet, ConcurrentTickAndReadsAreSafe) {
    Fleet f(1);
    load_demo_scenario(f);
    for (int id = 1; id <= 3; ++id) f.start(id);

    std::atomic<bool> stop{false};
    std::vector<std::thread> readers;
    for (int i = 0; i < 3; ++i) {
        readers.emplace_back([&] {
            while (!stop) {
                auto s = f.snapshot();
                (void)f.recent_alerts(10);
                (void)f.zones();
                f.set_link(1, true);
                EXPECT_EQ(s.size(), 3u);
            }
        });
    }
    for (int i = 0; i < 2000; ++i) f.tick(0.5);
    stop = true;
    for (auto& t : readers) t.join();
    SUCCEED();
}

TEST(Scenario, DemoScenarioHasThreeUavsAndOneZone) {
    Fleet f(1);
    load_demo_scenario(f);
    EXPECT_EQ(f.snapshot().size(), 3u);
    EXPECT_EQ(f.zones().size(), 1u);
}
