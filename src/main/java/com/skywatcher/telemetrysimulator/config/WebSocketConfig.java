package com.skywatcher.telemetrysimulator.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.messaging.simp.config.MessageBrokerRegistry;
import org.springframework.web.socket.config.annotation.EnableWebSocketMessageBroker;
import org.springframework.web.socket.config.annotation.StompEndpointRegistry;
import org.springframework.web.socket.config.annotation.WebSocketMessageBrokerConfigurer;

@Configuration
@EnableWebSocketMessageBroker // WebSocket sunucusunu aktif eder
public class WebSocketConfig implements WebSocketMessageBrokerConfigurer {

    @Override
    public void configureMessageBroker(MessageBrokerRegistry config) {
        // İstemcilerin (Frontend) dinleyeceği kanalın ön eki: "/topic"
        // Yani radyomuz "/topic/..." frekansından yayın yapacak.
        config.enableSimpleBroker("/topic");

        // Frontend'den backend'e veri gelirse (ilerde komut göndermek için) "/app" ile başlayacak.
        config.setApplicationDestinationPrefixes("/app");
    }

    @Override
    public void registerStompEndpoints(StompEndpointRegistry registry) {
        // Frontend'in bağlanacağı "ana kapı" (Handshake noktası)
        registry.addEndpoint("/ws-telemetry")
                .setAllowedOriginPatterns("*") // Tüm sitelerden (localhost:3000 vb.) erişime izin ver (CORS)
                .withSockJS(); // Bağlantı koparsa veya tarayıcı desteklemezse alternatif yollar dener.
    }
}