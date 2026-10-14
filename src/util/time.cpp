#include "util/time.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace uavgcs::util {

std::string iso_utc_now() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", tm.tm_year + 1900, tm.tm_mon + 1,
                  tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));
    return buf;
}

namespace {
std::mutex g_log_mutex;

void log(const char* level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::fprintf(stderr, "%s [%s] %s\n", iso_utc_now().c_str(), level, msg.c_str());
}
}  // namespace

void log_info(const std::string& msg) { log("INFO", msg); }
void log_warn(const std::string& msg) { log("WARN", msg); }
void log_error(const std::string& msg) { log("ERROR", msg); }

}  // namespace uavgcs::util
