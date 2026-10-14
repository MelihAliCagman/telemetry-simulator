#pragma once
#include <string>

namespace uavgcs::util {

// Şimdiki zamanı ISO-8601 UTC olarak döndürür, örn. 2026-10-14T09:30:15.123Z
std::string iso_utc_now();

// Basit, iş parçacığı güvenli günlükleme (stderr).
void log_info(const std::string& msg);
void log_warn(const std::string& msg);
void log_error(const std::string& msg);

}  // namespace uavgcs::util
