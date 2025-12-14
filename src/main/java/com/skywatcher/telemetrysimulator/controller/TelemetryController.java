package com.skywatcher.telemetrysimulator.controller;

import com.skywatcher.telemetrysimulator.model.TelemetryData;
import com.skywatcher.telemetrysimulator.repository.TelemetryRepository;
import com.skywatcher.telemetrysimulator.service.AirportService; // YENİ
import com.skywatcher.telemetrysimulator.service.SimulationService;
import org.springframework.web.bind.annotation.*;

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
}