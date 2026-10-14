#pragma once
// Coğrafi yardımcılar: küresel Dünya modeli (WGS-84 ortalama yarıçap).
#include <vector>

namespace uavgcs::geo {

constexpr double kEarthRadiusM = 6371008.8;
constexpr double kPi = 3.14159265358979323846;

struct LatLon {
    double lat{0.0};  // derece, +kuzey
    double lon{0.0};  // derece, +doğu
};

double deg2rad(double deg);
double rad2deg(double rad);

// Açıyı [0, 360) aralığına getirir.
double normalize_deg(double deg);

// from -> to dönüşünde en kısa işaretli fark, (-180, 180].
double signed_angle_diff_deg(double from_deg, double to_deg);

// Haversine büyük daire mesafesi (metre).
double distance_m(const LatLon& a, const LatLon& b);

// a noktasından b noktasına başlangıç kerterizi (derece, 0 = kuzey, saat yönü).
double bearing_deg(const LatLon& a, const LatLon& b);

// Küçük mesafeler için düz yüzey yaklaşımıyla kuzey/doğu öteleme (metre).
LatLon offset_m(const LatLon& p, double north_m, double east_m);

// Işın atma yöntemi; çokgen en az 3 köşe içermelidir. Kenar üzerindeki noktalar için sonuç tanımsızdır.
bool point_in_polygon(const LatLon& p, const std::vector<LatLon>& polygon);

}  // namespace uavgcs::geo
