package com.skywatcher.telemetrysimulator.service;

import org.springframework.stereotype.Service;
import java.util.HashMap;
import java.util.Map;

@Service
public class AirportService {

    // Havalimanı Adı -> Koordinat Bilgileri
    private final Map<String, double[]> airportDatabase = new HashMap<>();

    public AirportService() {
        // Türkiye
        airportDatabase.put("LTAC - Ankara Esenboğa", new double[]{40.1281, 32.9951});
        airportDatabase.put("LTFM - İstanbul Havalimanı", new double[]{41.2811, 28.7519});
        airportDatabase.put("LTBJ - İzmir Adnan Menderes", new double[]{38.2924, 27.1570});
        airportDatabase.put("LTAI - Antalya Havalimanı", new double[]{36.8987, 30.8005});

        // Dünya
        airportDatabase.put("KJFK - New York JFK", new double[]{40.6413, -73.7781});
        airportDatabase.put("EGLL - London Heathrow", new double[]{51.4700, -0.4543});
        airportDatabase.put("RJTT - Tokyo Haneda", new double[]{35.5494, 139.7798});
        airportDatabase.put("OMDB - Dubai Int.", new double[]{25.2532, 55.3657});
    }

    public Map<String, double[]> getAllAirports() {
        return airportDatabase;
    }
}