# IoT Dashboard — Spring Boot Application

A Spring Boot web application that connects to an MQTT broker, receives sensor data from ESP32 devices, stores it in a database, and provides a real-time web dashboard.

## Application Structure

```
src/main/java/com/iotdashboard/
├── IoTDashboardApplication.java        # Entry point
├── model/
│   ├── Device.java                     # JPA entity — device identity + latest readings
│   └── SensorData.java                 # JPA entity — time-series sensor snapshots
├── repository/
│   ├── DeviceRepository.java           # findByMacAddress()
│   └── SensorDataRepository.java       # findTop100ByDeviceOrderByTimestampDesc()
├── dto/
│   ├── DeviceResponse.java             # API response record
│   ├── SensorDataResponse.java         # Sensor history response record
│   ├── LedCommandRequest.java          # LED command request with validation
│   └── PowerModeRequest.java           # Power mode command request with validation
├── service/
│   ├── MqttService.java                # MQTT client — subscribes to broker, publishes commands
│   ├── DeviceService.java              # Business logic — CRUD, field updates, sensor snapshots
│   └── FirmwareService.java            # OTA firmware storage — upload, serve, metadata
└── controller/
    ├── DeviceController.java           # Device REST API endpoints
    ├── FirmwareController.java         # Firmware upload, download, OTA deploy endpoints
    └── GlobalExceptionHandler.java     # Centralized error handling
```

## Key Components

### MqttService

Connects to the MQTT broker using the Eclipse Paho client library. On startup (`@PostConstruct`), it subscribes to `devices/#` to receive all device messages. When a message arrives on a topic like `devices/AABBCCDDEEFF/temperature`, it parses the topic into MAC address + field name and delegates to `DeviceService.updateDeviceField()`.

Also exposes a `publish()` method used by the controllers to send LED, power mode, and OTA commands back to devices.

### DeviceService

Handles all database operations. The `updateDeviceField()` method is the core dispatch — it receives a MAC address, field name, and raw string value, then updates the matching field on the Device entity. When the field is `temperature`, it also saves a `SensorData` snapshot for the time-series history.

Devices are created automatically the first time a message is received from a new MAC address.

### Device Entity

Stores both the device identity (MAC address, IP) and the latest sensor values (temperature, RSSI, heap, uptime, etc.) directly on the entity. Also tracks OTA-related state (`firmwareVersion`, `otaStatus`, `otaLastAttempt`) and power management state (`powerMode`, `telemetryInterval`). This avoids joins when the dashboard fetches all devices with their current readings.

### SensorData Entity

Stores periodic sensor snapshots (one per ESP32 publish cycle). Linked to a Device via `@ManyToOne`. Used by the `/history` endpoint to show trends over time.

### FirmwareService

Manages firmware binary storage on disk. Handles uploading `.bin` files (replacing any previously stored firmware), serving the binary to ESP32 devices via HTTP, and exposing metadata (filename, size, upload time). Configured via `firmware.storage-dir` and `firmware.download-url` in `application.yml`.

### FirmwareController

Provides endpoints for firmware upload, download, info, and OTA deployment. The deploy endpoint looks up the target device's MAC address, then publishes the firmware download URL to `devices/{MAC}/ota` via MQTT, which triggers the ESP32 to download and flash the new firmware.

## REST API

| Method | Endpoint                    | Description                         |
|--------|-----------------------------|-------------------------------------|
| GET    | `/api/devices`              | List all devices with latest values |
| GET    | `/api/devices/{id}`         | Single device details               |
| GET    | `/api/devices/{id}/history` | Last 100 sensor snapshots           |
| POST   | `/api/devices/{id}/led`     | Send LED command `{"state":"on"}`   |
| POST   | `/api/devices/{id}/power_mode` | Send power mode command `{"mode":"low"}` or `{"mode":"normal"}` |
| POST   | `/api/firmware/upload`      | Upload a `.bin` firmware file        |
| GET    | `/api/firmware/latest`      | Download the stored firmware binary  |
| GET    | `/api/firmware/info`        | Get info about the uploaded firmware |
| POST   | `/api/firmware/deploy/{id}` | Trigger OTA update on a device       |

## Configuration

### Default Profile — H2 (local development)

`application.yml` — in-memory database, no setup needed:

```yaml
spring.datasource.url: jdbc:h2:mem:iotdb
spring.jpa.hibernate.ddl-auto: create-drop
mqtt.broker: control.aut.utcluj.ro
mqtt.port: 11188
```

### Admin Password

Firmware upload and OTA deploy actions are password-protected. The dashboard prompts for a password before allowing these operations.

```yaml
admin:
  password: changeme123    # change this before deploying
```

Override at runtime without editing the file:

```bash
java -jar iot-dashboard.jar --admin.password=mysecretpass
# or via environment variable:
ADMIN_PASSWORD=mysecretpass java -jar iot-dashboard.jar
```

### Docker Profile — PostgreSQL

`application-docker.yml` — activated via `SPRING_PROFILES_ACTIVE=docker`:

```yaml
spring.datasource.url: jdbc:postgresql://postgres:5432/iotdb
spring.jpa.hibernate.ddl-auto: update
```

> **Important:** For OTA updates to work, the dashboard must be reachable by the ESP32 over the network. The `firmware.download-url` must point to the server's **LAN IP or public IP** (e.g., `http://192.168.1.100:8080/api/firmware/latest`), not `localhost` or `127.0.0.1`. The ESP32 downloads the firmware binary via HTTP from this URL — if it can't reach the server, the OTA update will fail.

## How to Run

### Local (requires Java 17 + Maven)

```bash
mvn spring-boot:run
```

- Dashboard: http://localhost:8080
- H2 Console: http://localhost:8080/h2-console (JDBC URL: `jdbc:h2:mem:iotdb`, user: `sa`, no password)

### Docker (requires Docker)

```bash
docker compose up --build
```

- Dashboard: http://localhost:8080
- Stop: `docker compose down` (add `-v` to delete database volume)

## Dependencies

| Dependency                    | Purpose                         |
|-------------------------------|---------------------------------|
| `spring-boot-starter-web`    | REST API + static file serving  |
| `spring-boot-starter-data-jpa` | JPA / Hibernate ORM           |
| `spring-boot-starter-validation` | Request validation (`@Valid`) |
| `h2`                         | In-memory database (dev)        |
| `postgresql`                 | PostgreSQL driver (docker)      |
| `org.eclipse.paho.client.mqttv3` | MQTT client library          |

## Frontend

Located in `src/main/resources/static/`:

| File            | Purpose                                          |
|-----------------|--------------------------------------------------|
| `index.html`    | Single-page dashboard layout                     |
| `css/style.css` | Card grid layout, RSSI color coding, LED buttons, power mode controls |
| `js/app.js`     | Polls `GET /api/devices` every 5s, renders device cards, LED commands, power mode commands, firmware upload, OTA deploy |

No build tools or frameworks — plain HTML, CSS, and vanilla JavaScript.
