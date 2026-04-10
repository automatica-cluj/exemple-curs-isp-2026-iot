# ESP32-C3 Super Mini — Arduino & MQTT Quick Start Guide

## Introduction

This guide walks you through setting up an **ESP32-C3 Super Mini** development board using the **Arduino IDE** and connecting it to an **MQTT broker**. By the end, your board will publish sensor data (chip temperature, Wi-Fi signal strength, memory usage, and more) and respond to LED control commands over MQTT.

No prior experience with microcontrollers is required.

---

## 1. What You Need

**Hardware:**
- 1x ESP32-C3 Super Mini board
- 1x USB-C cable (data-capable, not charge-only)
- A computer (macOS, Windows, or Linux)
- A Wi-Fi network with internet access

**Software (all free):**
- Arduino IDE
- ESP32 board package for Arduino
- PubSubClient MQTT library

**MQTT Broker (already provided):**

| Setting        | Value                      |
|----------------|----------------------------|
| Host           | `control.aut.utcluj.ro`    |
| MQTT Port      | `11188`                    |
| WebSocket Port | `11190`                    |
| Authentication | Anonymous (none required)  |

> **Note:** You do not need to install your own MQTT broker. The server above is available for use. If you prefer to run your own broker (e.g., Mosquitto), you may do so, but it must be accessible on a **public IP address** so that the ESP32 can reach it from any network. A broker running on `localhost` is only reachable from the same machine.

---

## 2. About the ESP32-C3 Super Mini

The ESP32-C3 Super Mini is a tiny, affordable development board based on the Espressif ESP32-C3 chip. Key features:

- **RISC-V single-core** processor at 160 MHz
- **Wi-Fi 802.11 b/g/n** and **Bluetooth 5 (LE)**
- **USB-C** connector with **built-in USB** — no external serial driver needed in most cases
- **GPIO pins:** 0–10, 20, 21, plus 5V, 3.3V, and GND
- **BOOT** and **RST** buttons for manual flash mode
- **Built-in LED on GPIO 8** (active LOW — `LOW` turns it on, `HIGH` turns it off)

---

## 3. Install Arduino IDE

### macOS
1. Go to [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)
2. Download the macOS version (Apple Silicon or Intel depending on your Mac)
3. Open the `.dmg` file and drag **Arduino IDE** to your Applications folder
4. Launch Arduino IDE

### Windows
1. Go to [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)
2. Download the **Windows (MSI installer)** or the **ZIP** version
3. Run the installer and follow the prompts
4. Launch Arduino IDE

### Linux (Ubuntu/Debian)
1. Go to [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)
2. Download the **AppImage** for Linux
3. Make it executable: `chmod +x arduino-ide_*_Linux_64bit.AppImage`
4. Run it: `./arduino-ide_*_Linux_64bit.AppImage`
5. On Linux you may need to add your user to the `dialout` group for serial port access:
   ```bash
   sudo usermod -a -G dialout $USER
   ```
   Log out and back in for this to take effect.

---

## 4. Add ESP32 Board Support

This step is the same on all operating systems.

1. Open Arduino IDE
2. Go to **Arduino IDE → Settings** (macOS) or **File → Preferences** (Windows/Linux), or press `Cmd + ,` / `Ctrl + ,`
3. Find the field **"Additional boards manager URLs"**
4. Paste the following URL:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
5. Click **OK**
6. Go to **Tools → Board → Boards Manager** (or click the board icon in the left sidebar)
7. Search for **esp32**
8. Find **"esp32 by Espressif Systems"** and click **Install**
9. Wait for the installation to complete (this downloads the compiler and tools — it may take a few minutes)

---

## 5. Connect the Board and Configure Arduino IDE

### 5.1 — Plug In the Board

Connect the ESP32-C3 Super Mini to your computer using a USB-C cable.

> **Important:** Make sure you use a data-capable USB-C cable. Some cables are charge-only and will not show up as a device. If nothing appears, try a different cable.

The ESP32-C3 has a built-in USB interface, so **no additional driver should be needed** on most systems. If the board does not appear as a serial port:

- **macOS:** Check System Settings → Privacy & Security for any blocked drivers
- **Windows:** The built-in USB should work on Windows 10/11. If not, install the Espressif USB driver from [https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/establish-serial-connection.html](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/establish-serial-connection.html)
- **Linux:** Make sure your user is in the `dialout` group (see Step 3)

### 5.2 — Select the Board

Go to **Tools → Board → esp32** → select **"ESP32C3 Dev Module"**

> Do **not** select "ESP32 Dev Module" — the C3 has a different architecture (RISC-V) and requires its own board profile.

### 5.3 — Select the Port

Go to **Tools → Port** and select the port that appeared when you plugged in the board:

- **macOS:** `/dev/cu.usbmodem...`
- **Windows:** `COM3`, `COM4`, or similar
- **Linux:** `/dev/ttyACM0` or `/dev/ttyUSB0`

### 5.4 — Enable USB CDC

Go to **Tools → USB CDC On Boot** → set to **"Enabled"**

This is required for Serial Monitor to work over the built-in USB connection.

---

## 6. Test with a Blink Sketch

Before moving to MQTT, verify that your setup works by flashing a simple LED blink program.

Create a new sketch and paste the following code:

```cpp
#define LED_PIN 8  // built-in LED on ESP32-C3 Super Mini

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Hello from ESP32-C3!");
}

void loop() {
  digitalWrite(LED_PIN, LOW);   // LED ON (active LOW)
  delay(500);
  digitalWrite(LED_PIN, HIGH);  // LED OFF (active LOW)
  delay(500);
}
```

Click the **Upload** button (the arrow icon in the toolbar). Wait for compilation and upload to complete.

**If upload fails**, enter flash mode manually:
1. Hold the **BOOT** button
2. Press and release the **RST** button
3. Release the **BOOT** button
4. Click Upload again

Once uploaded, the onboard LED should blink every half second. Open **Tools → Serial Monitor** (set baud rate to **115200**) to see the "Hello from ESP32-C3!" message.

---

## 7. Install the MQTT Library

1. Go to **Sketch → Include Library → Manage Libraries**
2. Search for **PubSubClient**
3. Install the one by **Nick O'Leary**

---

## 8. The Complete MQTT Sketch

This sketch connects to Wi-Fi, connects to the MQTT broker, publishes telemetry data every 5 seconds, and listens for LED control commands. Each device is uniquely identified by its MAC address.

Create a new sketch and paste the following code. **Update the Wi-Fi credentials** at the top with your own network name and password:

```cpp
#include <WiFi.h>
#include <PubSubClient.h>

// ---- UPDATE THESE WITH YOUR WI-FI CREDENTIALS ----
const char* ssid = "YourWiFiName";
const char* password = "YourWiFiPassword";
// ---------------------------------------------------

// MQTT broker settings (no changes needed)
const char* mqtt_server = "control.aut.utcluj.ro";
const int mqtt_port = 11188;

#define LED_PIN 8

WiFiClient espClient;
PubSubClient client(espClient);
String deviceId;
String topicPrefix;

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Received on ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);

  // LED control (active LOW on Super Mini)
  String t = String(topic);
  if (t.endsWith("/led")) {
    if (message == "on") digitalWrite(LED_PIN, LOW);
    if (message == "off") digitalWrite(LED_PIN, HIGH);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect(deviceId.c_str())) {
      Serial.println("connected!");
      String ledTopic = topicPrefix + "/led";
      client.subscribe(ledTopic.c_str());
      Serial.println("Subscribed to: " + ledTopic);
    } else {
      Serial.print("failed (");
      Serial.print(client.state());
      Serial.println(") retrying in 2s...");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // LED off at start (active LOW)

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());

  // Build unique device ID from MAC address
  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  topicPrefix = "devices/" + deviceId;
  Serial.println("Device ID: " + deviceId);
  Serial.println("Topic prefix: " + topicPrefix);

  // Connect to MQTT broker
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 5000) {
    lastMsg = millis();

    char buf[32];

    // Uptime (seconds)
    snprintf(buf, sizeof(buf), "%lu", millis() / 1000);
    client.publish((topicPrefix + "/status").c_str(), buf);

    // Chip temperature (°C)
    float chipTemp = temperatureRead();
    snprintf(buf, sizeof(buf), "%.1f", chipTemp);
    client.publish((topicPrefix + "/temperature").c_str(), buf);

    // Wi-Fi signal strength (dBm)
    snprintf(buf, sizeof(buf), "%d", WiFi.RSSI());
    client.publish((topicPrefix + "/rssi").c_str(), buf);

    // Free heap memory (bytes)
    snprintf(buf, sizeof(buf), "%u", ESP.getFreeHeap());
    client.publish((topicPrefix + "/heap").c_str(), buf);

    // Wi-Fi channel
    snprintf(buf, sizeof(buf), "%d", WiFi.channel());
    client.publish((topicPrefix + "/wifi_channel").c_str(), buf);

    // IP address
    client.publish((topicPrefix + "/ip").c_str(),
                   WiFi.localIP().toString().c_str());

    // MAC address
    client.publish((topicPrefix + "/mac").c_str(),
                   WiFi.macAddress().c_str());

    Serial.printf("Temp: %.1f | RSSI: %d | Heap: %u | Uptime: %lus\n",
                  chipTemp, WiFi.RSSI(), ESP.getFreeHeap(), millis() / 1000);
  }
}
```

Upload the sketch and open Serial Monitor at **115200** baud. You should see the device connect to Wi-Fi, print its device ID, connect to the MQTT broker, and start publishing data.

---

## 9. Testing from Your Computer

To verify that messages are flowing, you can use the Mosquitto command-line tools to subscribe and publish from your computer.

### 9.1 — Install Mosquitto Client Tools

You only need the **client tools** (not the full broker):

- **macOS:** `brew install mosquitto`
- **Windows:** Download from [https://mosquitto.org/download/](https://mosquitto.org/download/) and install. Add the install directory to your PATH or navigate to it in Command Prompt.
- **Linux:** `sudo apt install mosquitto-clients`

### 9.2 — Subscribe to All Device Messages

Open a terminal and run:

```bash
mosquitto_sub -h control.aut.utcluj.ro -p 11188 -t "devices/#" -v
```

The `-v` flag prints the topic alongside the message. The `#` wildcard matches all sub-topics. You should see output like:

```
devices/A1B2C3D4E5F6/status 42
devices/A1B2C3D4E5F6/temperature 35.2
devices/A1B2C3D4E5F6/rssi -52
devices/A1B2C3D4E5F6/heap 280000
devices/A1B2C3D4E5F6/wifi_channel 6
devices/A1B2C3D4E5F6/ip 192.168.1.45
devices/A1B2C3D4E5F6/mac AA:BB:CC:DD:EE:FF
```

### 9.3 — Control the LED

Replace `A1B2C3D4E5F6` below with your actual device ID (shown in Serial Monitor at boot):

```bash
# Turn LED on
mosquitto_pub -h control.aut.utcluj.ro -p 11188 -t "devices/A1B2C3D4E5F6/led" -m "on"

# Turn LED off
mosquitto_pub -h control.aut.utcluj.ro -p 11188 -t "devices/A1B2C3D4E5F6/led" -m "off"
```

---

## 10. MQTT Topic Structure Reference

Each device publishes to its own unique topic tree based on its MAC address:

| Topic                              | Payload Example      | Description                          |
|------------------------------------|----------------------|--------------------------------------|
| `devices/{MAC}/status`             | `42`                 | Uptime in seconds                    |
| `devices/{MAC}/temperature`        | `35.2`               | Internal chip temperature in °C      |
| `devices/{MAC}/rssi`               | `-52`                | Wi-Fi signal strength in dBm         |
| `devices/{MAC}/heap`               | `280000`             | Free heap memory in bytes            |
| `devices/{MAC}/wifi_channel`       | `6`                  | Wi-Fi channel number                 |
| `devices/{MAC}/ip`                 | `192.168.1.45`       | Current IP address                   |
| `devices/{MAC}/mac`                | `AA:BB:CC:DD:EE:FF`  | MAC address                          |
| `devices/{MAC}/led` *(subscribe)*  | `on` / `off`         | LED control command                  |

**RSSI interpretation:** 0 to -30 dBm = excellent, -30 to -50 = very good, -50 to -70 = good, -70 to -90 = weak, below -90 = very poor.

---

## 11. Using Your Own MQTT Broker (Optional)

If you prefer to run your own broker instead of using `control.aut.utcluj.ro`, you can install **Mosquitto**:

- **macOS:** `brew install mosquitto && brew services start mosquitto`
- **Windows:** Download and install from [https://mosquitto.org/download/](https://mosquitto.org/download/)
- **Linux:** `sudo apt install mosquitto mosquitto-clients && sudo systemctl enable mosquitto`

**Important:** If your ESP32 is on a different network than your computer (e.g., university Wi-Fi vs. home network), your broker must be reachable over a **public IP address** or through **port forwarding** on your router. A broker running on `localhost` is only accessible from the same machine or local network.

Update the sketch to point to your broker:

```cpp
const char* mqtt_server = "your.public.ip.or.domain";
const int mqtt_port = 1883;  // default Mosquitto port
```

---

## 12. Troubleshooting

**Board not detected (no port appears):**
- Try a different USB-C cable — some cables are charge-only
- On Windows, check Device Manager for unrecognized devices
- On Linux, verify you are in the `dialout` group
- Try a different USB port on your computer

**Upload fails:**
- Enter flash mode manually: hold **BOOT**, press **RST**, release BOOT, then click Upload
- Make sure you selected **"ESP32C3 Dev Module"** as the board
- Make sure **USB CDC On Boot** is set to **Enabled**

**Wi-Fi does not connect:**
- Double-check SSID and password (case-sensitive)
- Make sure the network is 2.4 GHz — the ESP32-C3 does not support 5 GHz
- Move the board closer to the router

**MQTT does not connect:**
- Verify the broker is reachable: `mosquitto_pub -h control.aut.utcluj.ro -p 11188 -t "test" -m "hello"`
- Make sure port `11188` is not blocked by your network/firewall
- Check Serial Monitor for error codes

**Serial Monitor shows garbage characters:**
- Make sure baud rate in Serial Monitor matches the sketch: **115200**

**LED works in reverse (on/off swapped):**
- The built-in LED on the Super Mini is **active LOW**. The sketch already accounts for this. If you wire an external LED, you may need to swap `HIGH`/`LOW`.

---

## 13. Next Steps

Once you have the basic setup working, here are some ideas to explore:

- **Add external sensors** (DHT22 for temperature/humidity, BMP280 for pressure, HC-SR04 for distance)
- **Send JSON payloads** instead of plain text for richer data
- **Build a web dashboard** using Node-RED, Grafana, or a custom HTML page with MQTT over WebSocket (port 11190)
- **Add multiple boards** — each one gets its own topic tree automatically via the MAC address
- **Implement deep sleep** to run the board from a battery for days or weeks
- **Add OTA updates** so you can update firmware over Wi-Fi without plugging in the USB cable
