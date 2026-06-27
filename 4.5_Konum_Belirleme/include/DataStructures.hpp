/**
 * @file DataStructures.hpp
 * @brief Core data structures for Geolocation system
 * @details Defines structures for station info, LOB, geolocation results
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * References:
 * - 4.5_Konum_Belirleme.txt: Technical specifications
 * - ED_pipeline.txt: Section [9] (Cross-Fixing Geolocation)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef GEOLOCATION_DATA_STRUCTURES_HPP
#define GEOLOCATION_DATA_STRUCTURES_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <cmath>

namespace CognitiveRF {
namespace Geolocation {

/**
 * @struct StationInfo
 * @brief ED İstasyonu Bilgileri
 * @details Coğrafi konum ve tanımlayıcı bilgiler
 */
struct StationInfo {
    uint32_t station_id;                ///< İstasyon ID (1, 2, ...)
    double latitude_deg;                ///< Enlem (derece) WGS84
    double longitude_deg;               ///< Boylam (derece) WGS84
    float altitude_meters;              ///< Yükseklik (metre) - opsiyonel
    std::string station_name;           ///< İstasyon adı
    
    StationInfo()
        : station_id(0)
        , latitude_deg(0.0)
        , longitude_deg(0.0)
        , altitude_meters(0.0f)
        , station_name("Unknown")
    {}
    
    StationInfo(uint32_t id, double lat, double lon, const std::string& name = "")
        : station_id(id)
        , latitude_deg(lat)
        , longitude_deg(lon)
        , altitude_meters(0.0f)
        , station_name(name.empty() ? "Station_" + std::to_string(id) : name)
    {}
};

/**
 * @struct AoAMeasurement
 * @brief Yön Bulma (AoA) Ölçümü
 * @details 4.4 Yön Bulma modülünden gelen ölçüm verisi
 */
struct AoAMeasurement {
    StationInfo station;                ///< Ölçümü yapan istasyon
    float angle_degrees;                ///< Geliş açısı (derece) - True North referans
    float confidence;                   ///< Güven skoru [0.0, 1.0]
    float rms_error_degrees;            ///< RMS hata (±10°)
    uint64_t timestamp_ms;              ///< GPS zaman etiketi (milisaniye)
    double center_frequency_hz;         ///< Tespit edilen sinyal frekansı
    
    AoAMeasurement()
        : angle_degrees(0.0f)
        , confidence(0.0f)
        , rms_error_degrees(10.0f)
        , timestamp_ms(0)
        , center_frequency_hz(0.0)
    {}
};

/**
 * @struct LineOfBearing
 * @brief Kerteriz Hattı (LOB) - Line of Bearing
 * @details İstasyondan hedef yönünde uzanan doğru
 */
struct LineOfBearing {
    StationInfo station;                ///< Başlangıç istasyonu
    float bearing_degrees;              ///< Azimut açısı (derece) - True North
    uint64_t timestamp_ms;              ///< Zaman etiketi
    float confidence;                   ///< Güven skoru
    
    // Parametrik doğru denklemi: P = origin + t * direction
    struct ParametricLine {
        double origin_x;                ///< Başlangıç X (metre, lokal UTM)
        double origin_y;                ///< Başlangıç Y (metre, lokal UTM)
        double direction_x;             ///< Yön vektörü X (normalize)
        double direction_y;             ///< Yön vektörü Y (normalize)
    };
    
    ParametricLine parametric;          ///< Kartezyen parametrik form
    
    LineOfBearing()
        : bearing_degrees(0.0f)
        , timestamp_ms(0)
        , confidence(0.0f)
    {
        parametric.origin_x = 0.0;
        parametric.origin_y = 0.0;
        parametric.direction_x = 0.0;
        parametric.direction_y = 1.0;
    }
};

/**
 * @struct GeoCoordinate
 * @brief Coğrafi Koordinat (WGS84)
 * @details Hedefin kestirilen konumu
 */
struct GeoCoordinate {
    double latitude_deg;                ///< Enlem (derece)
    double longitude_deg;               ///< Boylam (derece)
    float altitude_meters;              ///< Yükseklik (metre) - opsiyonel
    float accuracy_meters;              ///< Konum doğruluğu (metre CEP)
    
    GeoCoordinate()
        : latitude_deg(0.0)
        , longitude_deg(0.0)
        , altitude_meters(0.0f)
        , accuracy_meters(0.0f)
    {}
    
    GeoCoordinate(double lat, double lon, float acc = 0.0f)
        : latitude_deg(lat)
        , longitude_deg(lon)
        , altitude_meters(0.0f)
        , accuracy_meters(acc)
    {}
    
    /**
     * @brief Koordinatın geçerli olup olmadığını kontrol et
     */
    bool isValid() const {
        return (latitude_deg >= -90.0 && latitude_deg <= 90.0) &&
               (longitude_deg >= -180.0 && longitude_deg <= 180.0);
    }
};

/**
 * @struct UncertaintyEllipse
 * @brief Konum Belirsizlik Elipsi
 * @details Hata kovaryans matrisinden türetilen elips parametreleri
 */
struct UncertaintyEllipse {
    float semi_major_axis_meters;       ///< Büyük eksen (metre)
    float semi_minor_axis_meters;       ///< Küçük eksen (metre)
    float orientation_degrees;          ///< Oryantasyon (derece, True North)
    
    UncertaintyEllipse()
        : semi_major_axis_meters(0.0f)
        , semi_minor_axis_meters(0.0f)
        , orientation_degrees(0.0f)
    {}
};

/**
 * @struct GeolocationResult
 * @brief Konum Belirleme Sonucu
 * @details Cross-fixing + Least Squares çıktısı
 */
struct GeolocationResult {
    GeoCoordinate target_position;      ///< Hedef konumu
    UncertaintyEllipse uncertainty;     ///< Hata elipsi
    
    std::vector<LineOfBearing> lobs;    ///< Kullanılan LOB doğruları
    uint32_t num_stations_used;         ///< Kullanılan istasyon sayısı
    
    float confidence;                   ///< Genel güven skoru [0.0, 1.0]
    float gdop;                         ///< Geometric Dilution of Precision
    float residual_rms_meters;          ///< RMS artık (metre)
    
    uint64_t timestamp_ms;              ///< Sonuç zaman etiketi
    uint32_t iteration_count;           ///< Optimizasyon iterasyon sayısı
    bool converged;                     ///< Yakınsama başarısı
    bool is_valid;                      ///< Sonuç geçerliliği
    
    std::string validation_notes;       ///< Uyarı/hata mesajları
    
    GeolocationResult()
        : num_stations_used(0)
        , confidence(0.0f)
        , gdop(0.0f)
        , residual_rms_meters(0.0f)
        , timestamp_ms(0)
        , iteration_count(0)
        , converged(false)
        , is_valid(false)
    {}
};

/**
 * @struct GeolocationConfig
 * @brief Konum Belirleme Konfigürasyonu
 * @details Algoritma parametreleri
 */
struct GeolocationConfig {
    float max_station_distance_km;          ///< Maksimum istasyon mesafesi (km)
    float convergence_tolerance_meters;     ///< Yakınsama toleransı (metre)
    uint32_t max_iterations;                ///< Maksimum iterasyon sayısı
    float max_timestamp_diff_ms;            ///< Maksimum zaman farkı (ms)
    float min_intersection_angle_deg;       ///< Minimum kesişim açısı (derece)
    float max_residual_rms_meters;          ///< Maksimum RMS artık (metre)
    bool enable_outlier_rejection;          ///< Outlier reddetme
    
    GeolocationConfig()
        : max_station_distance_km(100.0f)       // 100 km
        , convergence_tolerance_meters(1.0f)    // 1 metre
        , max_iterations(50)
        , max_timestamp_diff_ms(100.0f)         // 100 ms
        , min_intersection_angle_deg(30.0f)     // 30 derece
        , max_residual_rms_meters(500.0f)       // 500 metre
        , enable_outlier_rejection(false)       // İki istasyon için kapalı
    {}
};

/**
 * @struct GeolocationStatistics
 * @brief Performans İstatistikleri
 * @details Sistem performans metrikleri
 */
struct GeolocationStatistics {
    uint64_t total_computations;            ///< Toplam hesaplama sayısı
    uint64_t successful_fixes;              ///< Başarılı konum sayısı
    uint64_t failed_fixes;                  ///< Başarısız konum sayısı
    
    double avg_processing_time_ms;          ///< Ortalama işlem süresi (ms)
    double min_processing_time_ms;          ///< Minimum işlem süresi (ms)
    double max_processing_time_ms;          ///< Maksimum işlem süresi (ms)
    
    float avg_residual_rms_meters;          ///< Ortalama RMS artık
    float avg_confidence;                   ///< Ortalama güven skoru
    float avg_gdop;                         ///< Ortalama GDOP
    
    GeolocationStatistics()
        : total_computations(0)
        , successful_fixes(0)
        , failed_fixes(0)
        , avg_processing_time_ms(0.0)
        , min_processing_time_ms(1e9)
        , max_processing_time_ms(0.0)
        , avg_residual_rms_meters(0.0f)
        , avg_confidence(0.0f)
        , avg_gdop(0.0f)
    {}
    
    void reset() {
        total_computations = 0;
        successful_fixes = 0;
        failed_fixes = 0;
        avg_processing_time_ms = 0.0;
        min_processing_time_ms = 1e9;
        max_processing_time_ms = 0.0;
        avg_residual_rms_meters = 0.0f;
        avg_confidence = 0.0f;
        avg_gdop = 0.0f;
    }
};

} // namespace Geolocation
} // namespace CognitiveRF

#endif // GEOLOCATION_DATA_STRUCTURES_HPP
