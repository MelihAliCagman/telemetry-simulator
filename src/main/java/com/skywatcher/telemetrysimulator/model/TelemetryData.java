package com.skywatcher.telemetrysimulator.model; // Paket ismine dikkat

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity // Bu sınıfın bir Veritabanı Tablosu olduğunu söyler
@Table(name = "telemetry_data") // Tablonun adı bu olacak
public class TelemetryData {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id; // Her kayıt için eşsiz numara (1, 2, 3...)

    private double latitude;
    private double longitude;
    private double altitude;
    private double speed;

    private LocalDateTime timestamp; // Verinin ne zaman geldiği

    // Boş Constructor (JPA için şart)
    public TelemetryData() {
    }

    // Veri doldurmak için kolay Constructor
    public TelemetryData(double latitude, double longitude, double altitude, double speed) {
        this.latitude = latitude;
        this.longitude = longitude;
        this.altitude = altitude;
        this.speed = speed;
        this.timestamp = LocalDateTime.now(); // Şu anki saati otomatik atar
    }

    // --- Getter ve Setter Metotları ---
    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }

    public double getLatitude() { return latitude; }
    public void setLatitude(double latitude) { this.latitude = latitude; }

    public double getLongitude() { return longitude; }
    public void setLongitude(double longitude) { this.longitude = longitude; }

    public double getAltitude() { return altitude; }
    public void setAltitude(double altitude) { this.altitude = altitude; }

    public double getSpeed() { return speed; }
    public void setSpeed(double speed) { this.speed = speed; }

    public LocalDateTime getTimestamp() { return timestamp; }
    public void setTimestamp(LocalDateTime timestamp) { this.timestamp = timestamp; }
}