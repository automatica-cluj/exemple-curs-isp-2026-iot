#include <WiFi.h>
#include <PubSubClient.h>

// ---- UPDATE THESE ----
// Double check if you are usiung mobile hotspot if it is set to 2.4 GHz, otherways will not connect!
const char* ssid = "CHANGE";
const char* password = "CHANGE";
const char* mqtt_server = "control.aut.utcluj.ro";
const int mqtt_port = 11188;
// ----------------------

#define LED_PIN 8

WiFiClient espClient;
PubSubClient client(espClient);
String deviceId;
String topicPrefix;
bool ledOn = false;

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

  // Build unique device ID from MAC
  deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  topicPrefix = "devices/" + deviceId;
  Serial.println("Device ID: " + deviceId);
  Serial.println("Topic prefix: " + topicPrefix);

  // Connect to MQTT
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
    snprintf(buf, sizeof(buf), "%u", ESP.getFreeHeap());
    client.publish((topicPrefix + "/heap").c_str(), buf);

    // Wi-Fi channel
    snprintf(buf, sizeof(buf), "%d", WiFi.channel());
    client.publish((topicPrefix + "/wifi_channel").c_str(), buf);

    // IP address
    client.publish((topicPrefix + "/ip").c_str(), WiFi.localIP().toString().c_str());

    // MAC address
    client.publish((topicPrefix + "/mac").c_str(), WiFi.macAddress().c_str());

    //LED status
    client.publish((topicPrefix + "/led_state").c_str(), ledOn ? "on" : "off");

    Serial.printf("Temp: %.1f | RSSI: %d | Heap: %u | Uptime: %lus\n",
                  chipTemp, WiFi.RSSI(), ESP.getFreeHeap(), millis() / 1000);
  }

  delay(1);  // yield to RTOS — prevents CPU spin and reduces heat
}