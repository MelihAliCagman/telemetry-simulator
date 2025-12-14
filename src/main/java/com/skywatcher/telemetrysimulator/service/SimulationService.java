package com.skywatcher.telemetrysimulator.service;

import com.skywatcher.telemetrysimulator.model.TelemetryData;
import com.skywatcher.telemetrysimulator.repository.TelemetryRepository;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;

@Service
public class SimulationService {

    private final SimpMessagingTemplate messagingTemplate;
    private final TelemetryRepository telemetryRepository;

    // Uçağın Şu Anki Konumu (Başlangıç: Esenboğa civarı)
    private double currentLat = 40.1281;
    private double currentLon = 32.9951;

    // Gidilecek Hedef (Başlangıçta olduğu yerde kalsın)
    private double targetLat = 40.1281;
    private double targetLon = 32.9951;

    private double altitude = 1000;
    private double speed = 0;

    public SimulationService(SimpMessagingTemplate messagingTemplate, TelemetryRepository telemetryRepository) {
        this.messagingTemplate = messagingTemplate;
        this.telemetryRepository = telemetryRepository;
    }

    // Dışarıdan yeni hedef girmek için metot
    public void updateTarget(double lat, double lon) {
        this.targetLat = lat;
        this.targetLon = lon;
        System.out.println("Yeni Rota Oluşturuldu: " + lat + ", " + lon);
    }

    // YENİ METOT: Uçuş Planını Uygula
    public void startFlight(double startLat, double startLon, double endLat, double endLon) {
        // 1. Uçağı başlangıç noktasına ışınla
        this.currentLat = startLat;
        this.currentLon = startLon;

        // 2. Hedefi ayarla
        this.targetLat = endLat;
        this.targetLon = endLon;

        // 3. Motorları çalıştır
        this.speed = 0; // Kalkışta hız 0'dan başlar
        System.out.println("Uçuş Planı Yüklendi. Rota: " + startLat + "," + startLon + " -> " + endLat + "," + endLon);
    }

    @Scheduled(fixedRate = 100) // Saniyede 10 kare
    public void movePlane() {
        // Hedefe ne kadar yolumuz var?
        double distLat = targetLat - currentLat;
        double distLon = targetLon - currentLon;

        // Basit Pisagor ile mesafe tahmini (kare kök işlemi yorar, yaklaşık bakıyoruz)
        double distance = Math.sqrt(distLat*distLat + distLon*distLon);

        if (distance > 0.0001) { // Hedefe varmadıysak hareket et
            // Hızlan (Motor çalışıyor)
            speed = 250;

            // Hareketi yumuşatmak için mesafenin %5'i kadar ilerle (Easing)
            // Bu sayede yaklaştıkça yavaşlayan gerçekçi bir iniş yapar
            currentLat += distLat * 0.05;
            currentLon += distLon * 0.05;

            // Hareket halindeyken irtifa sabit kalsın veya yükselsin
            altitude = 1000;
        } else {
            // Hedefe vardıysak dur
            speed = 0;
            System.out.println("Hedefe Varıldı/Beklemede.");
        }

        // Veriyi Kaydet ve Gönder
        TelemetryData data = new TelemetryData(currentLat, currentLon, altitude, speed);
        telemetryRepository.save(data);
        messagingTemplate.convertAndSend("/topic/telemetry", data);
    }
}