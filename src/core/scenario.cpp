#include "core/scenario.hpp"

namespace uavgcs {

void load_demo_scenario(Fleet& fleet) {
    const LatLon base{39.8000, 32.8000};  // Ankara güneyi, temsili ileri üs

    fleet.set_wind({6.0, 300.0, 1.5});  // 6 m/s kuzeybatıdan, hafif türbülans

    fleet.add_zone({"NFZ-1 (örnek yasak bölge)",
                    ZoneKind::NoFly,
                    {{39.83, 32.94}, {39.83, 33.00}, {39.89, 33.00}, {39.89, 32.94}}});

    // KARTAL-1: kuzey devriyesi, noktalarda bekleme. Göreve sorunsuz biter.
    fleet.add_uav("KARTAL-1", *spec_preset("MINI_RECON"), base,
                  {{{39.86, 32.88}, 300.0, 40.0}, {{39.92, 32.86}, 350.0, 0.0}, {{39.90, 32.76}, 300.0, 30.0}});

    // SAHIN-2: doğu rotası yasak bölgeyi keser; sınır ihlali ve otomatik RTL gösterir.
    fleet.add_uav("SAHIN-2", *spec_preset("MINI_RECON"), base,
                  {{{39.84, 32.90}, 280.0, 0.0}, {{39.86, 33.04}, 280.0, 0.0}});

    // BAYKUS-3: uzun menzilli gözetleme, %30 bataryayla başlar; yetersiz dönüş enerjisi uyarısı gösterir.
    fleet.add_uav("BAYKUS-3", *spec_preset("ENDURANCE"), base,
                  {{{40.10, 33.20}, 800.0, 0.0}, {{40.30, 33.60}, 800.0, 60.0}}, 30.0);
}

}  // namespace uavgcs
