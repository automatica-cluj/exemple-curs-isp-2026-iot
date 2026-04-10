#include <WiFi.h>
#include <PubSubClient.h>

// ---- UPDATE THESE ----
// Double check if you are using mobile hotspot if it is set to 2.4 GHz, otherwise will not connect!
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "control.aut.utcluj.ro";
const int mqtt_port = 11188;
// ----------------------

const char* FIRMWARE_VERSION = "1.0.0";

// Default sleep interval: 5 minutes (300 seconds)
#define DEFAULT_SLEEP_INTERVAL_SEC 300
#define WIFI_TIMEOUT_MS 10000
#define MQTT_TIMEOUT_MS 10000
#define CONFIG_WAIT_MS 2000

// RTC memory survives deep sleep (but not power-off or reset)
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR uint32_t sleepIntervalSec = DEFAULT_SLEEP_INTERVAL_SEC;

WiFiClient espClient;
PubSubClient client(espClient);
String deviceId;
String topicPrefix;
volatile bool configReceived = false;

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  String t = String(topic);
  if (t.endsWith("/config/interval")) {
    int newInterval = message.toInt();
    if (newInterval >= 10 && newInterval <= 3600) {
      sleepIntervalSec = (uint32_t)newInterval;
      Serial.printf("Config received: interval = %d seconds\n", newInterval);
    } else {
      Serial.printf("Config ignored: invalid interval %d (must be 10-3600)\n", newInterval);
    }
    configReceived = true;
  }
}

bool connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT_MS) {
      Serial.println(" TIMEOUT");
      return false;
    }
    delay(250);
    Serial.print(".");
  }
  Serial.printf(" OK (IP: %s)\n", WiFi.localIP().toString().c_str());
  return true;
}

bool connectMQTT() {
  Serial.print("Connecting to MQTT");
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  unsigned long start = millis();
  while (!client.connected()) {
    if (millis() - start > MQTT_TIMEOUT_MS) {
      Serial.println(" TIMEOUT");
      return false;
    }
    if (client.connect(deviceId.c_str())) {
      Serial.println(" OK");
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  return true;
}

void publishTelemetry() {
  char buf[32];

  // Boot count (acts as "uptime" for deep-sleep devices)
  snprintf(buf, sizeof(buf), "%lu", bootCount);
  client.publish((topicPrefix + "/boot_count").c_str(), buf);

  // Chip temperature
  float chipTemp = temperatureRead();
  snprintf(buf, sizeof(buf), "%.1f", chipTemp);
  client.publish((topicPrefix + "/temperature").c_str(), buf);

  // WiFi RSSI
  snprintf(buf, sizeof(buf), "%d", WiFi.RSSI());
  client.publish((topicPrefix + "/rssi").c_str(), buf);

  // Free heap memory
  snprintf(buf, sizeof(buf), "%u", ESP.getFreeHeap());
  client.publish((topicPrefix + "/heap").c_str(), buf);

  // WiFi channel
  snprintf(buf, sizeof(buf), "%d", WiFi.channel());
  client.publish((topicPrefix + "/wifi_channel").c_str(), buf);

  // IP address
  client.publish((topicPrefix + "/ip").c_str(), WiFi.localIP().toString().c_str());

  // WiFi SSID
  client.publish((topicPrefix + "/ssid").c_str(), ssid);

  // MAC address (formatted)
  client.publish((topicPrefix + "/mac").c_str(), WiFi.macAddress().c_str());

  // Firmware version
  client.publish((topicPrefix + "/firmware_version").c_str(), FIRMWARE_VERSION);

  // Current sleep interval
  snprintf(buf, sizeof(buf), "%lu", sleepIntervalSec);
  client.publish((topicPrefix + "/sleep_interval").c_str(), buf);

  Serial.printf("Published: temp=%.1f  rssi=%d  heap=%u  boots=%lu  interval=%lus\n",
                chipTemp, WiFi.RSSI(), ESP.getFreeHeap(), bootCount, sleepIntervalSec);
}

void goToSleep() {
  Serial.printf("Sleeping for %lu seconds...\n\n", sleepIntervalSec);
  esp_sleep_enable_timer_wakeup((uint64_t)sleepIntervalSec * 1000000ULL);
  esp_deep_sleep_start();
  // Execution stops here. On wake, setup() runs again from the top.
}

void setup() {
  Serial.begin(115200);
  bootCount++;

  Serial.println("================================");
  Serial.printf("Deep Sleep Sensor — Boot #%lu\n", bootCount);
  Serial.printf("Current interval: %lu seconds\n", sleepIntervalSec);
  Serial.println("================================");

  // Build unique device ID from MAC
  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  topicPrefix = "devices/" + deviceId;

  // Step 1: Connect WiFi
  if (!connectWiFi()) {
    goToSleep();  // Can't connect — try again next cycle
    return;
  }

  // Step 2: Connect MQTT
  if (!connectMQTT()) {
    goToSleep();
    return;
  }

  // Step 3: Subscribe to config topic and wait for retained message
  String configTopic = topicPrefix + "/config/interval";
  client.subscribe(configTopic.c_str());
  Serial.printf("Subscribed to: %s\n", configTopic.c_str());

  unsigned long waitStart = millis();
  while (millis() - waitStart < CONFIG_WAIT_MS) {
    client.loop();
    if (configReceived) break;
    delay(10);
  }

  if (!configReceived) {
    Serial.println("No config message received, using current interval");
  }

  // Step 4: Publish telemetry
  publishTelemetry();

  // Give MQTT time to send all messages
  unsigned long flushStart = millis();
  while (millis() - flushStart < 500) {
    client.loop();
    delay(10);
  }

  // Step 5: Disconnect cleanly and sleep
  client.disconnect();
  WiFi.disconnect(true);
  goToSleep();
}

void loop() {
  // Never reached — device sleeps after setup()
}
