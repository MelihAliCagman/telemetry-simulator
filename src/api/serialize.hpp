#pragma once
#include <nlohmann/json.hpp>
#include <vector>

#include "core/fleet.hpp"
#include "storage/telemetry_store.hpp"

namespace uavgcs::api {

using json = nlohmann::json;

json to_json(const UavState& s);
json to_json(const Alert& a);
json to_json(const Zone& z);
json to_json(const Wind& w);
json to_json(const SortieInfo& s);
json to_json(const TrackPoint& p);

// WebSocket yayın mesajları
json telemetry_message(const TickResult& tick, double time_scale);
json alert_message(const Alert& a);

}  // namespace uavgcs::api
