#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPUpdate.h>

// ---- UPDATE THESE ----
// Double check if you are usiung mobile hotspot if it is set to 2.4 GHz, otherways will not connect!
const char* ssid = "lks_dormitor_2";
const char* password = "opelastra";
const char* mqtt_server = "control.aut.utcluj.ro";
const int mqtt_port = 11188;
// ----------------------

const char* FIRMWARE_VERSION = "1.2.0";

#define LED_PIN 8

WiFiClient espClient;
PubSubClient client(espClient);
String deviceId;
String topicPrefix;
bool ledOn = false;
bool lowPowerMode = false;
unsigned long telemetryInterval = 5000;  // ms (5s normal, 30s low power)

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Received on ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);

  // Compare just the end of the topic
  String t = String(topic);
if (t.endsWith("/ota")) {
    String url = message;
    Serial.println("========== OTA UPDATE ==========");
    Serial.print("OTA update requested from: ");
    Serial.println(url);

    client.publish((topicPrefix + "/ota_status").c_str(), "downloading");
    Serial.println("Published ota_status: downloading");

    Serial.println("Starting HTTP download...");
    WiFiClient otaClient;
    t_httpUpdate_return ret = httpUpdate.update(otaClient, url);

    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("OTA FAILED: Error (%d): %s\n",
                       httpUpdate.getLastError(),
                       httpUpdate.getLastErrorString().c_str());
        {
          String errMsg = "failed: " + httpUpdate.getLastErrorString();
          client.publish((topicPrefix + "/ota_status").c_str(), errMsg.c_str());
        }
        break;
      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("OTA: No updates available");
        client.publish((topicPrefix + "/ota_status").c_str(), "no update");
        break;
      case HTTP_UPDATE_OK:
        Serial.println("OTA: Update OK — rebooting...");
        // Never reached — ESP reboots on success
        break;
    }
    Serial.println("================================");
    return;
  }

  if (t.endsWith("/led")) {
  if (message == "on") {
    digitalWrite(LED_PIN, LOW);
    ledOn = true;
  }
  if (message == "off") {
    digitalWrite(LED_PIN, HIGH);
    ledOn = false;
  }
  }

  if (t.endsWith("/power_mode")) {
    if (message == "low") {
      lowPowerMode = true;
      telemetryInterval = 30000;
      WiFi.setSleep(true);   // Enable modem sleep
      Serial.println("LOW POWER mode enabled (30s interval, modem sleep on)");
    } else if (message == "normal") {
      lowPowerMode = false;
      telemetryInterval = 5000;
      WiFi.setSleep(false);  // Disable modem sleep
      Serial.println("NORMAL power mode (5s interval, modem sleep off)");
    }
    client.publish((topicPrefix + "/power_mode_ack").c_str(), lowPowerMode ? "low" : "normal");
    client.publish((topicPrefix + "/telemetry_interval").c_str(), lowPowerMode ? "30" : "5");
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

      String otaTopic = topicPrefix + "/ota";
      client.subscribe(otaTopic.c_str());
      Serial.println("Subscribed to: " + otaTopic);

      String powerTopic = topicPrefix + "/power_mode";
      client.subscribe(powerTopic.c_str());
      Serial.println("Subscribed to: " + powerTopic);
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
  Serial.print("Connecting to Wi-Fi!");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());

  // Build unique device ID from MAC
  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  topicPrefix = "devices/" + deviceId;
  Serial.println("Device ID: " + deviceId);
  Serial.println("Topic prefix: " + topicPrefix);

  // Connect to MQTT
  client.setBufferSize(512);  // Default 256 is too small for OTA URLs
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > telemetryInterval) {
    lastMsg = millis();

    char buf[32];

    // Uptime
    snprintf(buf, sizeof(buf), "%lu", millis() / 1000);
    client.publish((topicPrefix + "/status").c_str(), buf);

    // Chip temperature
    float chipTemp = temperatureRead();
    snprintf(buf, sizeof(buf), "%.1f", chipTemp);
    client.publish((topicPrefix + "/temperature").c_str(), buf);

    // Wi-Fi RSSI
    snprintf(buf, sizeof(buf), "%d", WiFi.RSSI());
    client.publish((topicPrefix + "/rssi").c_str(), buf);

    // Free heap memory
    // snprintf(buf, sizeof(buf), "%u", ESP.getFreeHeap());
    // client.publish((topicPrefix + "/heap").c_str(), buf);

    // Wi-Fi channel
    snprintf(buf, sizeof(buf), "%d", WiFi.channel());
    client.publish((topicPrefix + "/wifi_channel").c_str(), buf);

    // IP address
    client.publish((topicPrefix + "/ip").c_str(), WiFi.localIP().toString().c_str());

    // Wi-Fi SSID
    client.publish((topicPrefix + "/ssid").c_str(), ssid);

    // MAC address
    client.publish((topicPrefix + "/mac").c_str(), WiFi.macAddress().c_str());

    //LED status
    client.publish((topicPrefix + "/led_state").c_str(), ledOn ? "on" : "off");

    // Firmware version
    client.publish((topicPrefix + "/firmware_version").c_str(), FIRMWARE_VERSION);

    // Power mode
    client.publish((topicPrefix + "/power_mode_status").c_str(), lowPowerMode ? "low" : "normal");
    snprintf(buf, sizeof(buf), "%lu", telemetryInterval / 1000);
    client.publish((topicPrefix + "/telemetry_interval").c_str(), buf);

    Serial.printf("Temp: %.1f | RSSI: %d | Heap: %u | Uptime: %lus | Power: %s\n",
                  chipTemp, WiFi.RSSI(), ESP.getFreeHeap(), millis() / 1000,
                  lowPowerMode ? "LOW" : "NORMAL");
  }

  delay(1);  // yield to RTOS — prevents CPU spin and reduces heat
}