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
        Integer wifiChannel,
        String ssid,
        String firmwareVersion,
        Long bootCount,
        Integer sleepInterval,
        LocalDateTime lastSeen
) {}
