package com.skywatcher.telemetrysimulator.service;

import com.skywatcher.telemetrysimulator.model.TelemetryData;
import com.skywatcher.telemetrysimulator.repository.TelemetryRepository;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;
import java.util.UUID;
import java.util.Random;

@Service
public class SimulationService {

    private final SimpMessagingTemplate messagingTemplate;
    private final TelemetryRepository telemetryRepository;

    private String currentFlightId = "WAITING";
    private double currentLat = 40.1281;
    private double currentLon = 32.9951;
    private double targetLat = 40.1281;
    private double targetLon = 32.9951;

    private double altitude = 0;
    private double speed = 0;
    private double currentFuel = 0;
    private boolean isFlying = false;
    private String currentStatus = "WAITING";

    // Hava Durumu
    private String currentWeather = "GÜNEŞLİ ☀️";

    public SimulationService(SimpMessagingTemplate messagingTemplate, TelemetryRepository telemetryRepository) {
        this.messagingTemplate = messagingTemplate;
        this.telemetryRepository = telemetryRepository;
    }

    public void startFlight(double startLat, double startLon, double endLat, double endLon) {
        this.currentLat = startLat;
        this.currentLon = startLon;
        this.targetLat = endLat;
        this.targetLon = endLon;
        this.currentFlightId = "FLIGHT-" + UUID.randomUUID().toString().substring(0, 8).toUpperCase();

        // 1. Rastgele Hava Durumu Seç
        String[] weathers = {"GÜNEŞLİ ☀️", "YAĞMURLU 🌧️", "FIRTINALI ⛈️", "KARLI ❄️", "RÜZGARLI 💨"};
        this.currentWeather = weathers[new Random().nextInt(weathers.length)];

        // 2. YAKIT HESABI (NORMALE DÖNDÜRDÜK)
        double dist = Math.sqrt(Math.pow(endLat-startLat, 2) + Math.pow(endLon-startLon, 2)) * 10000;

        // Formül: Mesafe * 0.5 + 500 Litre Yedek (Güvenli Uçuş)
        this.currentFuel = dist * 0.5 + 500;

        this.speed = 0;
        this.altitude = 0;
        this.isFlying = true;
        this.currentStatus = "FLYING";

        System.out.println("Uçuş Başladı! ID: " + currentFlightId + " Hava: " + currentWeather);
    }

    @Scheduled(fixedRate = 100)
    public void movePlane() {
        if (!isFlying && !currentStatus.equals("FLYING")) return;

        double distLat = targetLat - currentLat;
        double distLon = targetLon - currentLon;
        double distance = Math.sqrt(distLat*distLat + distLon*distLon);

        // İniş Kontrolü
        if (distance < 0.05) {
            this.isFlying = false;
            this.currentStatus = "LANDED";
            this.speed = 0;
            this.altitude = 0;
        }
        // Düşüş Kontrolü
        else if (currentFuel <= 0) {
            this.isFlying = false;
            this.currentStatus = "CRASHED";
            this.speed = 0;
            this.altitude = 0;
        }
        else {
            this.currentStatus = "FLYING";

            // --- HAVA KOŞULLARI MANTIĞI (Final) ---
            double consumptionRate = 0.8; // Normal Tüketim
            double targetSpeed = 800;     // Normal Hız

            if (currentWeather.contains("FIRTINALI")) {
                consumptionRate = 2.0; // Fırtınada 2.5 katı yakıt harca!
                targetSpeed = 600;     // Hız düşer
                this.altitude = 10000 + (Math.random() * 600 - 300); // Şiddetli Türbülans
            }
            else if (currentWeather.contains("KARLI")) {
                consumptionRate = 1.5;
                targetSpeed = 700;
                this.altitude = 10000;
            }
            else if (currentWeather.contains("RÜZGARLI")) {
                targetSpeed = 900; // Arkadan rüzgar alırsa hızlanır
                consumptionRate = 0.7; // Daha az yakar
                this.altitude = 10000 + (Math.random() * 100 - 50); // Hafif sallantı
            }
            else { // GÜNEŞLİ
                this.altitude = 10000; // Stabil uçuş
            }

            // Hızı ve Yakıtı Uygula
            this.speed = targetSpeed;
            this.currentFuel -= consumptionRate;

            this.currentLat += distLat * 0.01;
            this.currentLon += distLon * 0.01;
        }

        TelemetryData data = new TelemetryData(currentFlightId, currentLat, currentLon, altitude, speed, currentFuel, currentStatus, currentWeather);

        telemetryRepository.save(data);
        messagingTemplate.convertAndSend("/topic/telemetry", data);
    }

    public void updateTarget(double lat, double lon) { }
}