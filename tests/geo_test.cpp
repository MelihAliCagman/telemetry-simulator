#include <gtest/gtest.h>

#include <cmath>

#include "core/geo.hpp"

using namespace uavgcs::geo;

TEST(Geo, HaversineAnkaraIstanbul) {
    const LatLon esenboga{40.1281, 32.9951};
    const LatLon ist{41.2811, 28.7519};
    EXPECT_NEAR(distance_m(esenboga, ist), 379913.0, 500.0);
}

TEST(Geo, OneDegreeOfLongitudeAtEquator) {
    EXPECT_NEAR(distance_m({0, 0}, {0, 1}), 111195.0, 5.0);
}

TEST(Geo, DistanceIsSymmetricAndZeroForSamePoint) {
    const LatLon a{39.0, 32.0}, b{40.0, 33.0};
    EXPECT_DOUBLE_EQ(distance_m(a, a), 0.0);
    EXPECT_NEAR(distance_m(a, b), distance_m(b, a), 1e-6);
}

TEST(Geo, BearingCardinalDirections) {
    const LatLon o{39.0, 32.0};
    EXPECT_NEAR(bearing_deg(o, {40.0, 32.0}), 0.0, 1e-6);
    EXPECT_NEAR(bearing_deg(o, {39.0, 33.0}), 90.0, 0.5);
    EXPECT_NEAR(bearing_deg(o, {38.0, 32.0}), 180.0, 1e-6);
    EXPECT_NEAR(bearing_deg(o, {39.0, 31.0}), 270.0, 0.5);
}

TEST(Geo, NormalizeAndSignedDiff) {
    EXPECT_DOUBLE_EQ(normalize_deg(-90.0), 270.0);
    EXPECT_DOUBLE_EQ(normalize_deg(720.0), 0.0);
    EXPECT_DOUBLE_EQ(signed_angle_diff_deg(350.0, 10.0), 20.0);
    EXPECT_DOUBLE_EQ(signed_angle_diff_deg(10.0, 350.0), -20.0);
}

TEST(Geo, OffsetRoundTrip) {
    const LatLon p{39.8, 32.8};
    const LatLon q = offset_m(p, 1000.0, 2000.0);
    EXPECT_NEAR(distance_m(p, q), std::hypot(1000.0, 2000.0), 2.0);
    EXPECT_GT(q.lat, p.lat);
    EXPECT_GT(q.lon, p.lon);
}

TEST(Geo, PointInPolygon) {
    const std::vector<LatLon> square{{39.0, 32.0}, {39.0, 33.0}, {40.0, 33.0}, {40.0, 32.0}};
    EXPECT_TRUE(point_in_polygon({39.5, 32.5}, square));
    EXPECT_FALSE(point_in_polygon({38.5, 32.5}, square));
    EXPECT_FALSE(point_in_polygon({39.5, 33.5}, square));
    EXPECT_FALSE(point_in_polygon({39.5, 32.5}, {{39.0, 32.0}, {40.0, 33.0}}));  // < 3 köşe
}

TEST(Geo, PointInConcavePolygon) {
    // "L" biçimli çokgen
    const std::vector<LatLon> l{{0, 0}, {0, 2}, {1, 2}, {1, 1}, {2, 1}, {2, 0}};
    EXPECT_TRUE(point_in_polygon({0.5, 1.5}, l));
    EXPECT_TRUE(point_in_polygon({1.5, 0.5}, l));
    EXPECT_FALSE(point_in_polygon({1.5, 1.5}, l));
}
