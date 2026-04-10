package com.iotdashboard.dto;

import java.time.LocalDateTime;

public record DeviceResponse(
        Long id,
        String macAddress,
        String macFormatted,
        String ipAddress,
        Double temperature,
        Integer rssi,
        Long freeHeap,
        Long uptime,
        Integer wifiChannel,
        String ledState,
        String firmwareVersion,
        String otaStatus,
        String ssid,
        String powerMode,
        Integer telemetryInterval,
        LocalDateTime lastSeen,
        LocalDateTime otaLastAttempt
) {}
