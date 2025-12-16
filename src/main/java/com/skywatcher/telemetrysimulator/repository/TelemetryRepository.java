package com.skywatcher.telemetrysimulator.repository;

import com.skywatcher.telemetrysimulator.model.TelemetryData;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.stereotype.Repository;
import java.util.List;

@Repository
public interface TelemetryRepository extends JpaRepository<TelemetryData, Long> {

    // 1. Bütün uçuş kodlarını getir (Her kodu sadece 1 kere getir - DISTINCT)
    @Query("SELECT DISTINCT t.flightId FROM TelemetryData t ORDER BY t.flightId DESC")
    List<String> findUniqueFlightIds();

    // 2. Sadece belirli bir uçuşun verilerini getir
    List<TelemetryData> findByFlightId(String flightId);
}