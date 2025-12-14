package com.skywatcher.telemetrysimulator.model;

public class UavTelemetry {

    // İHA Kimliği (Tek bir İHA'mız var, ID: 1)
    private final int uavId = 1;

    // Coğrafi Konum
    private double latitude;
    private double longitude;

    // Uçuş Verileri
    private double altitudeMeters;
    private int speedKmh;

    // Sistem Durumu
    private int batteryPercentage;

    // Constructor (Yapıcı Metot)
    public UavTelemetry(double latitude, double longitude, double altitudeMeters, int speedKmh, int batteryPercentage) {
        this.latitude = latitude;
        this.longitude = longitude;
        this.altitudeMeters = altitudeMeters;
        this.speedKmh = speedKmh;
        this.batteryPercentage = batteryPercentage;
    }

    // Getter Metotları (IntelliJ'de Alt+Insert veya Cmd+N ile kolayca oluşturulabilir)

    public int getUavId() { return uavId; }
    public double getLatitude() { return latitude; }
    public double getLongitude() { return longitude; }
    public double getAltitudeMeters() { return altitudeMeters; }
    public int getSpeedKmh() { return speedKmh; }
    public int getBatteryPercentage() { return batteryPercentage; }
}