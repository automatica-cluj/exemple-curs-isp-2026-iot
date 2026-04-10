# IoT ESP32 App — Arduino MQTT Sensor Firmware

Arduino sketch for the ESP32-C3 Super Mini that reads onboard sensors and publishes data to an MQTT broker. It also listens for LED control commands, supports Over-The-Air (OTA) firmware updates, and can switch between **normal** and **low power** modes on command.

For the full step-by-step setup guide (Arduino IDE installation, board configuration, library setup), see [esp32-c3-mqtt-guide.md](esp32-c3-mqtt-guide.md).

## What It Does

1. Connects to WiFi
2. Derives a unique device ID from the MAC address (e.g., `AABBCCDDEEFF`)
3. Connects to the MQTT broker
4. Publishes sensor readings at a configurable interval (5s normal, 30s low power)
5. Listens on `devices/{MAC}/led` for `on`/`off` commands to toggle the built-in LED
6. Listens on `devices/{MAC}/ota` for OTA update commands (receives a URL, downloads firmware, flashes and reboots)
7. Listens on `devices/{MAC}/power_mode` for `low`/`normal` commands to switch power modes

## File Structure

```
iot-esp32-app/
├── sketch_apr2a/
│   └── sketch_apr2a.ino       # Arduino sketch (main and only source file)
├── esp32-c3-mqtt-guide.md     # Full setup guide for beginners
└── README.md                  # This file
```

## Published MQTT Topics

All topics use the prefix `devices/{MAC}/` where `{MAC}` is the WiFi MAC address without colons.

| Topic              | Value                  | Example        |
|--------------------|------------------------|----------------|
| `.../status`       | Uptime in seconds      | `42`           |
| `.../temperature`  | Chip temperature (C)   | `35.2`         |
| `.../rssi`         | WiFi signal (dBm)      | `-52`          |
| `.../heap`         | Free heap memory (B)   | `280000`       |
| `.../wifi_channel` | WiFi channel number    | `6`            |
| `.../ip`           | Device IP address      | `192.168.1.45` |
| `.../mac`          | MAC with colons        | `AA:BB:CC:DD:EE:FF` |
| `.../ssid`           | WiFi network name      | `MyWiFi`       |
| `.../led_state`      | Current LED state      | `on` / `off`   |
| `.../firmware_version` | Firmware version     | `1.0.0`        |
| `.../ota_status`     | OTA progress           | `downloading` / `failed: reason` |
| `.../power_mode`     | Current power mode     | `low` / `normal` |
| `.../power_mode_ack` | Power mode acknowledgment | `low` / `normal` |
| `.../telemetry_interval` | Current interval (s) | `5` / `30` |

## Subscribed MQTT Topics

| Topic          | Expected Payload | Action                                   |
|----------------|------------------|------------------------------------------|
| `.../led`      | `on` or `off`    | Toggles built-in LED (GPIO 8, active LOW)|
| `.../ota`      | HTTP URL string  | Downloads firmware from URL, flashes, and reboots |
| `.../power_mode` | `low` or `normal` | Switches power mode: `low` = 30s interval + modem sleep, `normal` = 5s interval + radio always on |

## Configuration

Edit these lines at the top of `sketch_apr2a.ino` before uploading:

```cpp
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "control.aut.utcluj.ro";
const int mqtt_port = 11188;
```

## Hardware

- **Board:** ESP32-C3 Super Mini
- **LED:** Built-in on GPIO 8 (active LOW — `LOW` = on, `HIGH` = off)
- **USB:** USB-C data cable for programming and serial monitor

## Libraries Required

Install via Arduino IDE **Library Manager** (Sketch → Include Library → Manage Libraries):

| Library        | Author          | Purpose           |
|----------------|-----------------|-------------------|
| `PubSubClient` | Nick O'Leary    | MQTT client       |
| `WiFi`         | Built-in (ESP32)| WiFi connectivity |
| `HTTPUpdate`   | Built-in (ESP32)| OTA firmware updates |

## How to Upload

1. Open `sketch_apr2a.ino` in Arduino IDE
2. Select board: **Tools → Board → ESP32C3 Dev Module**
3. Select partition scheme: **Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)**
4. Select port: **Tools → Port → (your USB port)**
4. Update WiFi credentials
5. Click **Upload**
6. Open **Serial Monitor** at 115200 baud to verify

Expected serial output:

```
Connecting to Wi-Fi...
Connected! IP: 192.168.1.45
Device ID: AABBCCDDEEFF
Topic prefix: devices/AABBCCDDEEFF
Subscribed to: devices/AABBCCDDEEFF/led
Subscribed to: devices/AABBCCDDEEFF/ota
Subscribed to: devices/AABBCCDDEEFF/power_mode
Temp: 35.2 | RSSI: -52 | Heap: 280000 | Uptime: 42s | Power: NORMAL
```

## How the Code Works

The sketch has three main parts:

- **`setup()`** — Connects to WiFi, builds the device ID from MAC address, connects to the MQTT broker, sets up the message callback
- **`loop()`** — Maintains the MQTT connection and publishes all sensor readings at `telemetryInterval` (5s normal, 30s low power) using `millis()` for non-blocking timing
- **`callback()`** — Handles incoming MQTT messages; checks if the topic ends with `/led` (toggles GPIO 8), `/ota` (triggers firmware download and flash via `HTTPUpdate`), or `/power_mode` (switches between normal and low power modes)

### Low Power Mode

When the device receives `low` on the `/power_mode` topic:
- `telemetryInterval` changes from 5000 ms to 30000 ms (6x fewer publishes)
- `WiFi.setSleep(true)` enables modem sleep — the radio duty-cycles with the AP's DTIM beacon interval instead of staying always on
- The device remains reachable for commands (LED, OTA, power mode) with ~100–300 ms added latency
