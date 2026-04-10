# OTA Firmware Update Guide

This guide explains how Over-The-Air (OTA) firmware updates work in this IoT demo — from the underlying mechanism to a step-by-step deployment walkthrough.

---

## 1. What Is OTA?

OTA (Over-The-Air) update means replacing the firmware on a microcontroller **without physically connecting a USB cable**. Instead, the device downloads a new firmware binary over the network and flashes itself.

This is essential for deployed IoT devices that may be installed in hard-to-reach locations or managed in large numbers.

---

## 2. How It Works in This Demo

This demo uses an **MQTT-triggered, HTTP-downloaded** OTA pattern. The dashboard acts as both the command center and the firmware file server.

<p align="center">
  <img src="ota-flow.drawio.png" alt="OTA Firmware Update Flow Diagram">
</p>

### Sequence of Events

```
1. User uploads a compiled .bin firmware file to the dashboard
2. User clicks "Update FW" on a device card
3. Dashboard publishes an MQTT message to devices/{MAC}/ota
   containing the HTTP URL where the firmware can be downloaded
4. ESP32 receives the MQTT message
5. ESP32 publishes ota_status = "downloading"
6. ESP32 makes an HTTP GET request to the dashboard to download the .bin
7. ESP32 writes the new firmware to the inactive flash partition
8. ESP32 verifies the checksum and reboots into the new firmware
9. ESP32 reconnects to MQTT and publishes its new firmware_version
10. Dashboard sees the updated version on the next poll cycle
```

### Why MQTT + HTTP (Not Just MQTT)?

MQTT messages have a practical size limit (~256 KB), and a firmware binary is typically 1–2 MB. Sending the entire binary over MQTT would require fragmentation and reassembly logic. Instead, we use MQTT only as a **command channel** ("go download from this URL") and HTTP as the **data channel** (the actual binary transfer). This keeps both sides simple.

---

## 3. MQTT Topics

| Topic | Direction | Payload | Purpose |
|---|---|---|---|
| `devices/{MAC}/ota` | Dashboard → ESP32 | `http://192.168.1.100:8080/api/firmware/latest` | Command to start OTA |
| `devices/{MAC}/ota_status` | ESP32 → Dashboard | `downloading` / `failed: <reason>` | Progress reporting |
| `devices/{MAC}/firmware_version` | ESP32 → Dashboard | `1.0.0` | Current version (sent every 5s with telemetry) |

**Note on success detection:** When the OTA flash succeeds, the ESP32 reboots immediately — so it never gets a chance to publish a "success" message. The dashboard detects a successful update by observing a **new firmware version** and a **reset uptime** on the next telemetry cycle.

---

## 4. ESP32 Flash Partitions (A/B Scheme)

The ESP32-C3 uses a dual-partition (A/B) scheme for safe OTA updates:

```
┌──────────────────────────┐
│       Bootloader         │
├──────────────────────────┤
│  Partition Table         │
├──────────────────────────┤
│  OTA Data (which is      │
│  active: A or B)         │
├──────────────────────────┤
│  App Partition A (OTA0)  │  ← currently running firmware
├──────────────────────────┤
│  App Partition B (OTA1)  │  ← new firmware gets written here
├──────────────────────────┤
│  SPIFFS (190 KB)         │
└──────────────────────────┘
```

**How it works:**
1. The new firmware is written to the **inactive** partition (e.g., OTA1)
2. After the write completes and the checksum is verified, the OTA data is updated to mark OTA1 as the active partition
3. The ESP32 reboots into the new firmware from OTA1
4. The next OTA update will write to OTA0 (the now-inactive partition), and so on

**Why this is safe:** The running firmware is never overwritten. If the download fails or the binary is corrupted, the device simply continues running from the old partition. If power is lost during the write, the bootloader still boots from the last known-good partition.

**Required Arduino IDE setting:**
- **Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)**
- This gives ~1.9 MB per app partition — enough for the sketch with all libraries

---

## 5. ESP32 Code Walkthrough

### Library

```cpp
#include <HTTPUpdate.h>   // Built into ESP32 Arduino core — no extra install needed
```

### Firmware Version

```cpp
const char* FIRMWARE_VERSION = "1.0.0";  // Bump this before each build
```

This string is published to `devices/{MAC}/firmware_version` every 5 seconds. After a successful OTA, the device reboots and starts publishing the new version — this is how the dashboard confirms the update worked.

### OTA Handler (in callback)

When a message arrives on the `devices/{MAC}/ota` topic, the callback:

1. Publishes `"downloading"` to `ota_status` so the dashboard shows progress
2. Creates a dedicated `WiFiClient` for the HTTP download
3. Calls `httpUpdate.update(otaClient, url)` — this is a **blocking call** that downloads the binary, writes it to flash, verifies it, and reboots
4. If it fails, publishes the error message to `ota_status`

```cpp
if (t.endsWith("/ota")) {
    String url = message;
    client.publish((topicPrefix + "/ota_status").c_str(), "downloading");

    WiFiClient otaClient;
    t_httpUpdate_return ret = httpUpdate.update(otaClient, url);

    switch (ret) {
      case HTTP_UPDATE_FAILED:
        // Publish error, device keeps running old firmware
        break;
      case HTTP_UPDATE_OK:
        // Never reached — ESP reboots on success
        break;
    }
}
```

### Buffer Size

```cpp
client.setBufferSize(512);  // In setup(), before setServer()
```

The default PubSubClient buffer is 256 bytes. A firmware URL like `http://192.168.1.100:8080/api/firmware/latest` could get truncated at 256 bytes. Setting the buffer to 512 ensures the full URL is received.

---

## 6. Dashboard (Spring Boot) Code Walkthrough

### FirmwareService

Manages a single firmware file on disk:
- `store(MultipartFile)` — saves the uploaded `.bin` to `{storage-dir}/firmware.bin`, replacing any previous file
- `loadFirmware()` — returns the file as a Spring `Resource` for the download endpoint
- `getDownloadUrl()` — returns the configured URL that ESP32 devices will use

### FirmwareController Endpoints

| Endpoint | Purpose |
|---|---|
| `POST /api/firmware/upload` | Accepts a multipart `.bin` file, stores it via FirmwareService |
| `GET /api/firmware/latest` | Serves the stored binary as `application/octet-stream` — this is what the ESP32 downloads |
| `GET /api/firmware/info` | Returns JSON with filename, size, and upload time |
| `POST /api/firmware/deploy/{deviceId}` | Looks up the device MAC, publishes the download URL to `devices/{MAC}/ota` |

### Content-Length Header

The `GET /api/firmware/latest` endpoint returns a `FileSystemResource`. Spring automatically sets the `Content-Length` header from the file size. This header is **required** by the ESP32 `HTTPUpdate` library — without it, the download will fail.

### Network Requirement

**The dashboard must be reachable by the ESP32 over the network.** During OTA, the ESP32 makes an HTTP GET request to the dashboard to download the firmware binary. This means:

- The server must have a **LAN IP** (if ESP32 is on the same network) or a **public IP** (if the ESP32 is remote)
- `localhost` or `127.0.0.1` will **not work** — those resolve to the ESP32 itself, not your server
- The server's firewall must allow inbound connections on port 8080

### Configuration

In `application.yml`:

```yaml
firmware:
  storage-dir: ./firmware                                    # Where .bin files are stored
  download-url: http://YOUR_LAN_IP:8080/api/firmware/latest  # URL the ESP32 will use

admin:
  password: changeme123                                      # Password for upload & OTA deploy

spring.servlet.multipart:
  max-file-size: 2MB
  max-request-size: 2MB
```

> **Note:** The firmware upload and OTA deploy actions require a password. The dashboard will prompt for it when you click "Upload" or "Update FW". Change the default password before deploying.

---

## 7. Step-by-Step Deployment Walkthrough

### Prerequisites

- The ESP32 sketch with OTA support has been flashed via USB at least once
- The partition scheme was set to "Minimal SPIFFS" when uploading
- The Spring Boot dashboard is running and reachable from the ESP32's network
- `firmware.download-url` in `application.yml` is set to the server's **LAN IP** (not `localhost`)

### Step 1 — Build the New Firmware

1. Open `sketch_apr2a.ino` in Arduino IDE
2. Make your changes (e.g., add a new sensor, change the publish interval)
3. **Bump the version:** change `FIRMWARE_VERSION` from `"1.0.0"` to `"1.0.1"` (or whatever your new version is)
4. Go to **Sketch → Export Compiled Binary** (or press Ctrl+Alt+S / Cmd+Alt+S)
5. This creates a `.bin` file in the sketch folder (look in `sketch_apr2a/build/`)

### Step 2 — Upload to Dashboard

1. Open the dashboard at `http://YOUR_SERVER_IP:8080`
2. In the **Firmware** section at the top of the page, click the file picker and select the `.bin` file
3. Click **Upload**
4. The firmware info should update to show the filename, size, and upload time

### Step 3 — Deploy to a Device

1. Find the target device card on the dashboard
2. Check the current firmware version displayed (e.g., `FW: 1.0.0`)
3. Click **Update FW**
4. Confirm the dialog
5. The OTA status should change to `commanded`, then `downloading`

### Step 4 — Verify

1. If you have the ESP32's Serial Monitor open, you'll see:
   ```
   OTA update requested from: http://192.168.1.100:8080/api/firmware/latest
   ```
   followed by a reboot
2. After reboot, the device reconnects to MQTT and starts publishing telemetry
3. On the dashboard, within 5–10 seconds you should see:
   - **FW version** updated to `1.0.1`
   - **Uptime** reset to a small number
   - **OTA status** may show the last status or reset after the device reconnects

---

## 8. Failure Scenarios

| Scenario | What Happens | Recovery |
|---|---|---|
| **Wrong URL / server unreachable** | `HTTPUpdate` returns `HTTP_UPDATE_FAILED`, device publishes error to `ota_status` | Device keeps running old firmware. Fix the URL in `application.yml` and retry |
| **Corrupted binary** | Checksum verification fails before the partition swap | Old firmware remains active. Upload a valid `.bin` and retry |
| **Power loss during flash** | Write was to the inactive partition; bootloader loads the old partition on next boot | Device boots normally with old firmware. Retry the OTA |
| **Binary too large** | Exceeds the partition size (~1.9 MB) | `HTTPUpdate` reports an error. Reduce the sketch size or use a different partition scheme |
| **Device doesn't receive MQTT command** | Dashboard shows "commanded" but status never changes | Check that the device is connected to the broker. Retry after confirming connectivity |

---

## 9. Troubleshooting

**"OTA command sent" but nothing happens on the device:**
- Check that the device is actually connected to the MQTT broker (look at the serial monitor or check uptime updates on the dashboard)
- Verify the device is subscribed to the `/ota` topic (serial monitor should show `Subscribed to: devices/XXXX/ota` on connect)

**Device reports "failed: HTTP error":**
- The `firmware.download-url` is likely wrong. It must be reachable from the ESP32, not from your browser. Use the server's LAN IP, not `localhost` or `127.0.0.1`
- Test by opening the URL in a browser — you should get a binary file download

**Device reports "failed: Not Enough Space":**
- The partition scheme doesn't have OTA partitions. Re-flash via USB with **Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** selected

**Upload fails with "413 Request Entity Too Large":**
- The `.bin` file exceeds the 2 MB limit configured in `application.yml`. Increase `spring.servlet.multipart.max-file-size` if needed

---

## 10. Security Considerations

This demo uses **no authentication** for the firmware download endpoint. In a production system you would want:

- **Signed firmware** — the ESP32 verifies a cryptographic signature before flashing (ESP-IDF supports this natively via secure boot)
- **Authenticated download** — require a token or certificate to access the firmware endpoint
- **HTTPS** — encrypt the firmware transfer to prevent tampering in transit
- **Version rollback protection** — prevent downgrading to older, vulnerable firmware versions

These are out of scope for this classroom demo but important for any real-world deployment.
