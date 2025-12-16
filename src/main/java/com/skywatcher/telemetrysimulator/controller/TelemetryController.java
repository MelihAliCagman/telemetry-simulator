package com.skywatcher.telemetrysimulator.controller;

import com.skywatcher.telemetrysimulator.model.TelemetryData;
import com.skywatcher.telemetrysimulator.repository.TelemetryRepository;
import com.skywatcher.telemetrysimulator.service.AirportService; // YENİ
import com.skywatcher.telemetrysimulator.service.SimulationService;
import org.springframework.web.bind.annotation.*;
import java.util.HashMap;
import java.util.Map;

import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/api/telemetry")
@CrossOrigin(origins = "*")
public class TelemetryController {

    private final TelemetryRepository telemetryRepository;
    private final SimulationService simulationService;
    private final AirportService airportService; // YENİ

    // Constructor'a AirportService'i de ekledik
    public TelemetryController(TelemetryRepository telemetryRepository,
                               SimulationService simulationService,
                               AirportService airportService) {
        this.telemetryRepository = telemetryRepository;
        this.simulationService = simulationService;
        this.airportService = airportService;
    }

    @GetMapping("/all")
    public List<TelemetryData> getAllTelemetry() {
        return telemetryRepository.findAll();
    }

    // YENİ: Havalimanı listesini React'a gönder
    @GetMapping("/airports")
    public Map<String, double[]> getAirports() {
        return airportService.getAllAirports();
    }

    // YENİ: İki nokta arasında uçuş başlat
    @PostMapping("/start-flight")
    public String startFlight(@RequestParam double startLat, @RequestParam double startLon,
                              @RequestParam double endLat, @RequestParam double endLon) {
        simulationService.startFlight(startLat, startLon, endLat, endLon);
        return "Uçuş Başlatıldı!";
    }

    // YENİ ÖZELLİK: Uçuş Verilerini Hesapla (Mesafe ve Yakıt)
    // Adres: http://localhost:8080/api/telemetry/calculate?startLat=...&endLat=...
    @GetMapping("/calculate")
    public Map<String, Object> calculateFlight(@RequestParam double startLat, @RequestParam double startLon,
                                               @RequestParam double endLat, @RequestParam double endLon) {

        // 1. Haversine Formülü ile Gerçek Mesafe Hesabı (KM cinsinden)
        double earthRadius = 6371; // Dünya'nın yarıçapı (km)
        double dLat = Math.toRadians(endLat - startLat);
        double dLon = Math.toRadians(endLon - startLon);

        double a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
                Math.cos(Math.toRadians(startLat)) * Math.cos(Math.toRadians(endLat)) *
                        Math.sin(dLon / 2) * Math.sin(dLon / 2);

        double c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
        double distanceKm = earthRadius * c;

        // 2. Yakıt Hesabı (Senaryo: Uçağımız km başına 5 Litre yakıyor olsun)
        double fuelConsumptionPerKm = 5.0;
        double estimatedFuel = distanceKm * fuelConsumptionPerKm;

        // 3. Sonuçları Paketle
        Map<String, Object> result = new HashMap<>();
        result.put("distanceKm", Math.round(distanceKm * 100.0) / 100.0); // Virgülden sonra 2 hane
        result.put("estimatedFuel", Math.round(estimatedFuel)); // Yuvarlanmış yakıt

        return result;
    }

    // YENİ: Sadece Uçuş Kodlarını Listele
    @GetMapping("/flights")
    public List<String> getFlightIds() {
        return telemetryRepository.findUniqueFlightIds();
    }

    // YENİ: Seçilen uçuşun detaylarını getir
    @GetMapping("/history/{flightId}")
    public List<TelemetryData> getFlightHistory(@PathVariable String flightId) {
        return telemetryRepository.findByFlightId(flightId);
    }
}