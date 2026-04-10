# IoT MQTT ESP32 Demo — Deep Sleep

End-to-end IoT demo: an ESP32 microcontroller wakes from deep sleep, publishes sensor data over MQTT, and goes back to sleep. A Spring Boot dashboard receives, stores, and visualizes the telemetry.

**What's new in this example:** Uses **ESP32 deep sleep** for maximum power efficiency. The device does not stay connected or respond to commands — it wakes on a timer, reports telemetry in a single burst, and sleeps again. The sleep interval is configurable from the dashboard via a **retained MQTT message**. Compared to [Example 03 (Low Power)](../03-IoT-mqtt-esp32-LowPower/) which uses modem sleep (~20–30 mA) and stays responsive, this example achieves ~5 µA during sleep — roughly **250x less power** at a 5-minute reporting interval.

## Documentation Index

| Document | Description |
|----------|-------------|
| **This file** (`README.md`) | Project overview, architecture, how to run and test everything |
| [simulate-device.sh](simulate-device.sh) | Shell script to simulate an ESP32 device without hardware |

---

## Hardware Required

### ESP32-C3 Super Mini

This demo uses the **ESP32-C3 Super Mini** development board — a tiny, low-cost microcontroller with built-in WiFi and Bluetooth.

**Key specs:**
- RISC-V single-core processor at 160 MHz
- WiFi 802.11 b/g/n
- Built-in LED on GPIO 8
- USB-C connector (no external serial driver needed)

**Where to buy:** [ESP32-C3 Super Mini — Ardushop.ro](https://ardushop.ro/ro/plci-de-dezvoltare/2224-placa-de-dezvoltare-esp32-c3-super-mini-6427854034298.html)

**You also need:** a USB-C data cable (not charge-only) to connect the board to your computer for programming and serial monitoring.

> **No hardware?** You can test the full system without a physical device using the `simulate-device.sh` script described below.

> **Important — before uploading to the ESP32:** Open `sketch_deep_sleep.ino` and update the WiFi credentials to match your network:
> ```cpp
> const char* ssid = "YOUR_WIFI_NAME";
> const char* password = "YOUR_WIFI_PASSWORD";
> ```
> The ESP32-C3 only supports **2.4 GHz WiFi** — it will not connect to 5 GHz networks. Make sure your access point has a 2.4 GHz band available.

---

## Project Structure

```
04-IoT-mqtt-esp32-DeepSleep/
├── iot-esp32-app/                  # ESP32 Arduino firmware
│   └── sketch_deep_sleep/
│       └── sketch_deep_sleep.ino  # Arduino sketch (deep sleep + telemetry)
│
├── iot-dashboard/                  # Spring Boot web application
│   ├── pom.xml                    # Maven dependencies (Spring Boot, JPA, Paho MQTT)
│   ├── Dockerfile                 # Multi-stage Docker build
│   ├── compose.yml                # Docker Compose (PostgreSQL + app)
│   └── src/main/
│       ├── java/com/iotdashboard/
│       │   ├── IoTDashboardApplication.java
│       │   ├── model/             # JPA entities (Device, SensorData)
│       │   ├── repository/        # Spring Data repositories
│       │   ├── dto/               # Request/Response records
│       │   ├── service/           # DeviceService + MqttService
│       │   └── controller/        # REST API endpoints (DeviceController)
│       └── resources/
│           ├── application.yml            # Default config (H2 in-memory)
│           ├── application-docker.yml     # Docker config (PostgreSQL)
│           └── static/                    # Web dashboard (HTML/CSS/JS)
│
└── simulate-device.sh             # Simulates an ESP32 without hardware
```

## How It Works

### Deep Sleep Wake Cycle

Unlike the previous examples where the ESP32 stays connected and runs a continuous `loop()`, this example puts all logic in `setup()`. The device never reaches `loop()` — it sleeps before that happens.

```
          ┌──────────────────────────────────────────────────────┐
          │                    DEEP SLEEP                        │
          │                  (~5 µA draw)                        │
          │              Timer: N seconds                        │
          └──────────────┬───────────────────────────────────────┘
                         │ Timer fires
                         ▼
          ┌──────────────────────────────────────────────────────┐
          │  1. Wake up (setup() runs from the top)              │
          │  2. Connect WiFi          (timeout: 10s)             │
          │  3. Connect MQTT          (timeout: 10s)             │
          │  4. Subscribe to config/interval topic               │
          │  5. Wait 2s for retained message (new interval?)     │
          │  6. Publish telemetry (temp, rssi, heap, etc.)       │
          │  7. Disconnect WiFi + MQTT                           │
          │  8. Enter deep sleep again                           │
          └──────────────┬───────────────────────────────────────┘
                         │
                         ▼
                   Back to sleep
```

**Key points:**
- After deep sleep, the ESP32 **reboots completely** — `setup()` runs again as if freshly powered on
- WiFi and MQTT must reconnect on every wake cycle (~2–4 seconds overhead)
- If WiFi or MQTT connection fails (timeout), the device goes back to sleep and retries next cycle
- `RTC_DATA_ATTR` variables survive deep sleep — used to persist `bootCount` and `sleepIntervalSec`

### Configurable Sleep Interval

The sleep interval is controlled via a **retained MQTT message**:

1. The dashboard publishes a retained message to `devices/{MAC}/config/interval` with a value in seconds (e.g., `"300"`)
2. The broker **stores** retained messages and delivers them immediately to any new subscriber
3. On each wake, the ESP32 subscribes to its config topic and waits up to 2 seconds
4. If a retained message is waiting, the new interval is stored in RTC memory and takes effect on the next sleep
5. If no message is received, the device uses the last known interval (default: 300 seconds)

Valid range: **10–3600 seconds** (10 seconds to 1 hour).

### RTC Memory

ESP32 deep sleep powers off almost everything, but **RTC (Real-Time Clock) memory** survives. This example stores two values there:

```cpp
RTC_DATA_ATTR uint32_t bootCount = 0;            // How many times the device has woken up
RTC_DATA_ATTR uint32_t sleepIntervalSec = 300;    // Current sleep interval in seconds
```

These values reset only on power cycle or hard reset — not on deep sleep wake.

### MQTT Topics

Each device uses its MAC address (without colons) as identifier:

| Topic                              | Direction       | Payload              | Retained? |
|------------------------------------|-----------------|----------------------|-----------|
| `devices/{MAC}/temperature`        | ESP32 → Broker  | `35.2`               | No        |
| `devices/{MAC}/rssi`               | ESP32 → Broker  | `-52`                | No        |
| `devices/{MAC}/heap`               | ESP32 → Broker  | `280000`             | No        |
| `devices/{MAC}/wifi_channel`       | ESP32 → Broker  | `6`                  | No        |
| `devices/{MAC}/ip`                 | ESP32 → Broker  | `192.168.1.45`       | No        |
| `devices/{MAC}/mac`                | ESP32 → Broker  | `AA:BB:CC:DD:EE:FF`  | No        |
| `devices/{MAC}/ssid`               | ESP32 → Broker  | `MyWiFi`             | No        |
| `devices/{MAC}/firmware_version`   | ESP32 → Broker  | `1.0.0`              | No        |
| `devices/{MAC}/boot_count`         | ESP32 → Broker  | `42`                 | No        |
| `devices/{MAC}/sleep_interval`     | ESP32 → Broker  | `300`                | No        |
| `devices/{MAC}/config/interval`    | Broker → ESP32  | `300` (seconds)      | **Yes**   |

Note that unlike Examples 01–03, there are **no command topics** for LED or power mode — this device is report-only.

### REST API

| Method | Endpoint                     | Description                          |
|--------|------------------------------|--------------------------------------|
| GET    | `/api/devices`               | List all devices                     |
| GET    | `/api/devices/{id}`          | Get single device                    |
| GET    | `/api/devices/{id}/history`  | Last 100 sensor readings             |
| PUT    | `/api/devices/{id}/interval` | Set sleep interval (`{"seconds":300}`) — publishes retained MQTT message |

---

## Running the ESP32 Firmware

1. Open `sketch_deep_sleep.ino` in Arduino IDE
2. Update WiFi credentials (`ssid` and `password`)
3. Select board: **ESP32C3 Dev Module**
4. Upload to the ESP32-C3 board
5. Open Serial Monitor (115200 baud) to see the wake cycle

Expected serial output:
```
================================
Deep Sleep Sensor — Boot #1
Current interval: 300 seconds
================================
Connecting to WiFi... OK (IP: 192.168.1.45)
Connecting to MQTT... OK
Subscribed to: devices/A1B2C3D4E5F6/config/interval
No config message received, using current interval
Published: temp=34.2  rssi=-48  heap=280000  boots=1  interval=300s
Sleeping for 300 seconds...
```

---

## Running the Spring Boot Dashboard

### Option 1: Local Development (H2 in-memory database)

```bash
cd iot-dashboard
mvn spring-boot:run
```

Open http://localhost:8080

### Option 2: Docker Compose (PostgreSQL)

```bash
cd iot-dashboard
docker compose up --build
```

Open http://localhost:8080

To stop: `docker compose down` (add `-v` to also remove the database volume).

### H2 Console (local dev only)

Available at http://localhost:8080/h2-console with:
- JDBC URL: `jdbc:h2:mem:iotdb`
- User: `sa`
- Password: *(empty)*

### Dashboard Features

The dashboard shows a device card for each ESP32 that has reported in, with:
- **Online/offline indicator** — green dot if the device was seen within 2x its sleep interval + 30 seconds; gray otherwise
- **Sensor readings** — temperature, WiFi RSSI, free heap, WiFi channel
- **Boot count** — how many wake cycles since last power-on
- **Sleep interval selector** — dropdown to set the interval (10s, 30s, 1m, 5m, 10m, 30m, 1h), applied on next wake

---

## Testing Without Hardware

Use the simulator script to publish fake sensor data to the MQTT broker:

```bash
# Simulate one device (default MAC, default 300s interval)
./simulate-device.sh

# Simulate with a custom MAC and 30-second interval
./simulate-device.sh A1B2C3D4E5F6 30

# Simulate multiple devices (run in separate terminals)
./simulate-device.sh DEVICE000001 60
./simulate-device.sh DEVICE000002 120
```

Requires `mosquitto_pub`:
- macOS: `brew install mosquitto`
- Linux: `sudo apt install mosquitto-clients`

### Manual MQTT Testing

```bash
# Subscribe to all device messages
mosquitto_sub -h control.aut.utcluj.ro -p 11188 -t "devices/#" -v

# Publish a single temperature reading
mosquitto_pub -h control.aut.utcluj.ro -p 11188 -t "devices/TEST123/temperature" -m "25.5"

# Set a device's sleep interval to 60 seconds (retained message)
mosquitto_pub -h control.aut.utcluj.ro -p 11188 -t "devices/TEST123/config/interval" -m "60" -r
```

---

## Deep Sleep Explained

### Why deep sleep?

In Examples 01–03, the ESP32 stays powered on and connected at all times. This is fine when plugged into USB, but impractical for battery-powered deployments. Deep sleep dramatically reduces power consumption by shutting down almost everything — CPU, WiFi radio, peripherals — keeping only the RTC timer alive to trigger the next wake.

### Power comparison across examples

| Mode | Current Draw | Used In | Can Receive Commands? |
|------|-------------|---------|----------------------|
| Always-on | ~130–180 mA | Examples 01, 02 | Yes, instantly |
| Modem sleep | ~20–30 mA | Example 03 | Yes, slight delay |
| **Deep sleep** | **~5–10 µA** | **Example 04** | **No (report-only)** |

### Average power at different intervals

During each wake cycle, the ESP32 draws ~130 mA for approximately 3–4 seconds (WiFi connect + MQTT + publish). The rest of the time it draws ~5 µA. The average current depends on the reporting interval:

| Interval | Awake Time | Average Current | Battery Life (2000 mAh) |
|----------|-----------|-----------------|------------------------|
| 30 seconds | ~4s | ~17 mA | ~5 days |
| 1 minute | ~4s | ~9 mA | ~9 days |
| 5 minutes | ~4s | ~2 mA | ~42 days |
| 30 minutes | ~4s | ~0.3 mA | ~280 days |

*Estimates assume ideal conditions. Real-world battery life depends on battery chemistry, self-discharge, WiFi reconnection time, and ambient temperature.*

### The trade-off

Deep sleep gives you orders-of-magnitude power savings, but at a cost:

- **No real-time commands** — the device cannot receive LED commands, OTA updates, or power mode changes while sleeping. Configuration changes (like sleep interval) only take effect on the next wake.
- **Reconnection overhead** — WiFi and MQTT must reconnect on every wake (~2–4 seconds). At very short intervals (< 30 seconds), this overhead becomes significant relative to the sleep time.
- **Data gaps** — the dashboard only receives data once per interval. For applications needing sub-second updates, deep sleep is not suitable.

This makes deep sleep ideal for **environmental monitoring** (temperature, humidity, air quality), **asset tracking**, and other scenarios where periodic reports are sufficient and battery life is critical.

---

## MQTT Broker

This demo uses a shared MQTT broker — no installation required:

| Setting        | Value                     |
|----------------|---------------------------|
| Host           | `control.aut.utcluj.ro`   |
| MQTT Port      | `11188`                   |
| WebSocket Port | `11190`                   |
| Authentication | Anonymous (none required) |
