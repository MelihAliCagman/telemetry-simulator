package com.skywatcher.telemetrysimulator;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.scheduling.annotation.EnableScheduling; // Bunu ekleyin!

@SpringBootApplication
@EnableScheduling // Zamanlanmış görevleri etkinleştirir.
public class TelemetrySimulatorApplication {

    public static void main(String[] args) {
        SpringApplication.run(TelemetrySimulatorApplication.class, args);
    }
}