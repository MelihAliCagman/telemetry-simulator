cmake_minimum_required(VERSION 3.20)
project(uav_gcs_simulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED CONFIG)
find_package(nlohmann_json 3.2 REQUIRED)

find_package(libpqxx CONFIG QUIET)
if(NOT TARGET libpqxx::pqxx)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)
  add_library(libpqxx::pqxx ALIAS PkgConfig::PQXX)
endif()

# --- Çekirdek: saf simülasyon, G/Ç yok -------------------------------------
add_library(uavgcs_core STATIC
  src/core/geo.cpp
  src/core/types.cpp
  src/core/geofence.cpp
  src/core/alerts.cpp
  src/core/uav.cpp
  src/core/fleet.cpp
  src/core/scenario.cpp
  src/util/time.cpp)
target_include_directories(uavgcs_core PUBLIC src)
target_link_libraries(uavgcs_core PUBLIC Threads::Threads)

# --- Depolama: PostgreSQL ---------------------------------------------------
add_library(uavgcs_storage STATIC src/storage/telemetry_store.cpp)
target_link_libraries(uavgcs_storage PUBLIC uavgcs_core libpqxx::pqxx)

# --- API: yönlendirme + JSON -------------------------------------------------
add_library(uavgcs_api STATIC src/api/serialize.cpp src/api/api.cpp)
target_link_libraries(uavgcs_api PUBLIC uavgcs_core uavgcs_storage nlohmann_json::nlohmann_json)

# --- Ağ: Boost.Beast HTTP + WebSocket --------------------------------------
add_library(uavgcs_net STATIC src/net/server.cpp)
target_link_libraries(uavgcs_net PUBLIC uavgcs_core Boost::headers)
if(WIN32)
  target_compile_definitions(uavgcs_net PUBLIC _WIN32_WINNT=0x0A00)
  target_link_libraries(uavgcs_net PUBLIC ws2_32 mswsock)
endif()

add_executable(uav_gcs_server src/app/main.cpp)
target_link_libraries(uav_gcs_server PRIVATE uavgcs_api uavgcs_net)

# --- Testler ----------------------------------------------------------------
option(UAVGCS_BUILD_TESTS "Birim testlerini derle" ON)
if(UAVGCS_BUILD_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
  add_executable(uavgcs_tests
    tests/geo_test.cpp
    tests/uav_test.cpp
    tests/alerts_test.cpp
    tests/fleet_test.cpp
    tests/api_test.cpp)
  target_link_libraries(uavgcs_tests PRIVATE uavgcs_api GTest::gtest GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(uavgcs_tests)
endif()
