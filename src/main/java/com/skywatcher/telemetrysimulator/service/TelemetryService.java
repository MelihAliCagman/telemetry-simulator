package com.skywatcher.telemetrysimulator.service;

import com.skywatcher.telemetrysimulator.model.UavTelemetry;
import org.springframework.messaging.simp.SimpMessagingTemplate; // YENİ EKLENDİ
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;
import jakarta.annotation.PostConstruct;
import java.util.Random;

@Service
public class TelemetryService {

    private final SimpMessagingTemplate messagingTemplate; // YENİ: Mesaj gönderme aracı
    private UavTelemetry currentTelemetry;
    private final Random random = new Random();

    // YENİ: Constructor Injection (Spring bu aracı bize otomatik verecek)
    public TelemetryService(SimpMessagingTemplate messagingTemplate) {
        this.messagingTemplate = messagingTemplate;
    }

    private static final double START_LAT = 40.1000;
    private static final double START_LON = 32.9900;

    @PostConstruct
    public void initialize() {
        currentTelemetry = new UavTelemetry(START_LAT, START_LON, 5000, 700, 100);
    }

    // SÜREYİ HIZLANDIRALIM: 5000 yerine 1000 (1 saniye) yapalım ki hareketi daha net görelim.
    @Scheduled(fixedRate = 1000)
    public void generateNewTelemetry() {

        // --- HESAPLAMA KISMI AYNI ---
        double newLat = currentTelemetry.getLatitude() + (random.nextDouble() - 0.5) * 0.0002;
        double newLon = currentTelemetry.getLongitude() + (random.nextDouble() - 0.5) * 0.0002;
        double newAlt = currentTelemetry.getAltitudeMeters() + (random.nextDouble() - 0.5) * 5;
        int newBat = Math.max(0, currentTelemetry.getBatteryPercentage() - (random.nextInt(2) == 0 ? 0 : 1)); // Pil daha yavaş bitsin

        currentTelemetry = new UavTelemetry(
                newLat,
                newLon,
                newAlt,
                currentTelemetry.getSpeedKmh(),
                newBat
        );

        // --- DEĞİŞEN KISIM BURASI ---

        // 1. Yine de konsola yazalım (debug için, çalıştığını görelim)
        System.out.println("Veri Gönderiliyor -> Lat: " + currentTelemetry.getLatitude() + " Bat: " + currentTelemetry.getBatteryPercentage());

        // 2. WEBSOCKET YAYINI: "/topic/telemetry" kanalına veriyi basıyoruz.
        messagingTemplate.convertAndSend("/topic/telemetry", currentTelemetry);
    }
}