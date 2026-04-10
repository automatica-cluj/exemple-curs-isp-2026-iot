# IoT Course Examples

This folder contains end-to-end IoT examples that complement the ISP 2026 course material. Each subfolder is an **independent, runnable project** combining an ESP32 Arduino sketch with a Spring Boot dashboard.

The examples follow a progressive structure, building on each other to introduce new concepts incrementally. All dashboards use **Java 17+**, **Maven**, and **Spring Boot 3**. All firmware targets the **ESP32-C3 Super Mini** (RISC-V, WiFi, USB-C).

| Folder | Description | Key Topics |
|--------|-------------|------------|
| [`01-IoT-mqtt-esp32-demo`](01-IoT-mqtt-esp32-demo/) | ESP32 publishes sensor data over MQTT; Spring Boot dashboard visualizes and controls the LED in real time | MQTT, Arduino, PubSubClient, Eclipse Paho, REST API, Docker Compose |
| [`02-IoT-mqtt-esp32-OTA`](02-IoT-mqtt-esp32-OTA/) | Extends the demo with wireless firmware updates via the dashboard | OTA, HTTPUpdate, dual-partition flashing, firmware management endpoints |
| [`03-IoT-mqtt-esp32-LowPower`](03-IoT-mqtt-esp32-LowPower/) | Adds low power mode — reduces telemetry frequency and enables WiFi modem sleep on command | Modem sleep, DTIM beacon duty-cycling, dynamic telemetry interval, ~20–30 mA |
| [`04-IoT-mqtt-esp32-DeepSleep`](04-IoT-mqtt-esp32-DeepSleep/) | Report-only sensor using deep sleep — wakes on timer, publishes telemetry, sleeps again | Deep sleep (~5 µA), RTC memory, retained MQTT for config, battery-powered operation |

---

## Architecture

All four examples share the same high-level architecture:

```
┌──────────┐       MQTT        ┌──────────────┐       HTTP       ┌─────────┐
│  ESP32   │ ──── publish ───▶ │  MQTT Broker  │ ◀── subscribe ── │ Spring  │
│  Device  │ ◀── subscribe ─── │              │                   │  Boot   │
└──────────┘                   └──────────────┘                   │Dashboard│
                                                                  └─────────┘
                                                                       │
                                                                  Browser UI
```

- **ESP32** reads sensors and publishes to `devices/{MAC}/*` topics
- **MQTT broker** (shared, no setup needed) routes messages
- **Spring Boot** subscribes to `devices/#`, stores data in H2/PostgreSQL, serves a web dashboard

---

## Power Consumption Progression

| Example | Mode | Current Draw | Receives Commands? |
|---------|------|-------------|-------------------|
| 01 & 02 | Always-on | ~130–180 mA | Yes, instantly |
| 03 | Modem sleep | ~20–30 mA | Yes, slight delay |
| 04 | Deep sleep | ~5 µA (sleeping) | No (report-only) |

---

## Running Without Hardware

Every example includes a `simulate-device.sh` script that publishes fake sensor data to the MQTT broker, so you can test the full dashboard without a physical ESP32 board.

```bash
# Install mosquitto tools first
# macOS: brew install mosquitto
# Linux: sudo apt install mosquitto-clients

# Run the simulator (from any example folder)
./simulate-device.sh
```

---

## MQTT Broker

All examples use a shared MQTT broker — no installation required:

| Setting        | Value                     |
|----------------|---------------------------|
| Host           | `control.aut.utcluj.ro`   |
| MQTT Port      | `11188`                   |
| WebSocket Port | `11190`                   |
| Authentication | Anonymous (none required) |
