package com.skywatcher.telemetrysimulator.model;

import jakarta.persistence.*;
import java.time.LocalDateTime;

@Entity
@Table(name = "telemetry_data")
public class TelemetryData {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    private String flightId;
    private double latitude;
    private double longitude;
    private double altitude;
    private double speed;
    private double remainingFuel;
    private String status;
    private String weather;

    private LocalDateTime timestamp;

    public TelemetryData() {}

    // GÜNCELLENMİŞ YAPICI METOT (Constructor) - 8 Parametre
    public TelemetryData(String flightId, double latitude, double longitude, double altitude, double speed, double remainingFuel, String status, String weather) {
        this.flightId = flightId;
        this.latitude = latitude;
        this.longitude = longitude;
        this.altitude = altitude;
        this.speed = speed;
        this.remainingFuel = remainingFuel;
        this.status = status;
        this.weather = weather; // YENİ
        this.timestamp = LocalDateTime.now();
    }

    // --- Getter ve Setter Metotları ---
    public String getWeather() { return weather; }
    public void setWeather(String weather) { this.weather = weather; }

    public Long getId() { return id; }
    public void setId(Long id) { this.id = id; }
    public String getFlightId() { return flightId; }
    public void setFlightId(String flightId) { this.flightId = flightId; }
    public double getLatitude() { return latitude; }
    public void setLatitude(double latitude) { this.latitude = latitude; }
    public double getLongitude() { return longitude; }
    public void setLongitude(double longitude) { this.longitude = longitude; }
    public double getAltitude() { return altitude; }
    public void setAltitude(double altitude) { this.altitude = altitude; }
    public double getSpeed() { return speed; }
    public void setSpeed(double speed) { this.speed = speed; }
    public double getRemainingFuel() { return remainingFuel; }
    public void setRemainingFuel(double remainingFuel) { this.remainingFuel = remainingFuel; }
    public String getStatus() { return status; }
    public void setStatus(String status) { this.status = status; }
    public LocalDateTime getTimestamp() { return timestamp; }
    public void setTimestamp(LocalDateTime timestamp) { this.timestamp = timestamp; }
}