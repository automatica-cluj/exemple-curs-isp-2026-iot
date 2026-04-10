package com.iotdashboard.model;

import jakarta.persistence.*;
import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.List;

@Entity
@Table(name = "devices")
public class Device {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false, unique = true)
    private String macAddress;          // "A1B2C3D4E5F6" (no colons, used in MQTT topics)

    private String macFormatted;        // "AA:BB:CC:DD:EE:FF" (with colons, from /mac topic)

    private String ipAddress;

    // Latest sensor readings (updated on each MQTT message)
    private Double temperature;
    private Integer rssi;
    private Long freeHeap;
    private Long uptime;
    private Integer wifiChannel;
    private String ledState;
    private String firmwareVersion;
    private String otaStatus;
    private String ssid;
    private String powerMode;
    private Integer telemetryInterval;

    private LocalDateTime lastSeen;
    private LocalDateTime otaLastAttempt;

    @OneToMany(mappedBy = "device", cascade = CascadeType.ALL)
    private List<SensorData> sensorHistory = new ArrayList<>();

    public Device() {
    }

    public Device(String macAddress) {
        this.macAddress = macAddress;
    }

    // --- Getters and Setters ---

    public Long getId() {
        return id;
    }

    public void setId(Long id) {
        this.id = id;
    }

    public String getMacAddress() {
        return macAddress;
    }

    public void setMacAddress(String macAddress) {
        this.macAddress = macAddress;
    }

    public String getMacFormatted() {
        return macFormatted;
    }

    public void setMacFormatted(String macFormatted) {
        this.macFormatted = macFormatted;
    }

    public String getIpAddress() {
        return ipAddress;
    }

    public void setIpAddress(String ipAddress) {
        this.ipAddress = ipAddress;
    }

    public Double getTemperature() {
        return temperature;
    }

    public void setTemperature(Double temperature) {
        this.temperature = temperature;
    }

    public Integer getRssi() {
        return rssi;
    }

    public void setRssi(Integer rssi) {
        this.rssi = rssi;
    }

    public Long getFreeHeap() {
        return freeHeap;
    }

    public void setFreeHeap(Long freeHeap) {
        this.freeHeap = freeHeap;
    }

    public Long getUptime() {
        return uptime;
    }

    public void setUptime(Long uptime) {
        this.uptime = uptime;
    }

    public Integer getWifiChannel() {
        return wifiChannel;
    }

    public void setWifiChannel(Integer wifiChannel) {
        this.wifiChannel = wifiChannel;
    }

    public String getLedState() {
        return ledState;
    }

    public void setLedState(String ledState) {
        this.ledState = ledState;
    }

    public String getFirmwareVersion() {
        return firmwareVersion;
    }

    public void setFirmwareVersion(String firmwareVersion) {
        this.firmwareVersion = firmwareVersion;
    }

    public String getOtaStatus() {
        return otaStatus;
    }

    public void setOtaStatus(String otaStatus) {
        this.otaStatus = otaStatus;
    }

    public String getSsid() {
        return ssid;
    }

    public void setSsid(String ssid) {
        this.ssid = ssid;
    }

    public String getPowerMode() {
        return powerMode;
    }

    public void setPowerMode(String powerMode) {
        this.powerMode = powerMode;
    }

    public Integer getTelemetryInterval() {
        return telemetryInterval;
    }

    public void setTelemetryInterval(Integer telemetryInterval) {
        this.telemetryInterval = telemetryInterval;
    }

    public LocalDateTime getLastSeen() {
        return lastSeen;
    }

    public void setLastSeen(LocalDateTime lastSeen) {
        this.lastSeen = lastSeen;
    }

    public LocalDateTime getOtaLastAttempt() {
        return otaLastAttempt;
    }

    public void setOtaLastAttempt(LocalDateTime otaLastAttempt) {
        this.otaLastAttempt = otaLastAttempt;
    }

    public List<SensorData> getSensorHistory() {
        return sensorHistory;
    }

    public void setSensorHistory(List<SensorData> sensorHistory) {
        this.sensorHistory = sensorHistory;
    }
}
