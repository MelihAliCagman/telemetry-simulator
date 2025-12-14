package com.skywatcher.telemetrysimulator.repository;

import com.skywatcher.telemetrysimulator.model.TelemetryData;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

@Repository
public interface TelemetryRepository extends JpaRepository<TelemetryData, Long> {

}