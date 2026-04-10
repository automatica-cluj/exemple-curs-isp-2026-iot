package com.iotdashboard.service;

import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import org.eclipse.paho.client.mqttv3.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.util.UUID;

@Service
public class MqttService {

    private static final Logger log = LoggerFactory.getLogger(MqttService.class);
    private static final String TOPIC_FILTER = "devices/#";

    private final DeviceService deviceService;
    private MqttClient mqttClient;

    @Value("${mqtt.broker}")
    private String broker;

    @Value("${mqtt.port}")
    private int port;

    public MqttService(DeviceService deviceService) {
        this.deviceService = deviceService;
    }

    @PostConstruct
    public void connect() {
        try {
            String serverUri = "tcp://" + broker + ":" + port;
            String clientId = "iot-dashboard-" + UUID.randomUUID().toString().substring(0, 8);

            mqttClient = new MqttClient(serverUri, clientId);

            MqttConnectOptions options = new MqttConnectOptions();
            options.setCleanSession(true);
            options.setAutomaticReconnect(true);
            options.setConnectionTimeout(10);

            mqttClient.setCallback(new MqttCallbackExtended() {

                @Override
                public void connectComplete(boolean reconnect, String serverURI) {
                    log.info("Connected to MQTT broker: {} (reconnect: {})", serverURI, reconnect);
                    try {
                        mqttClient.subscribe(TOPIC_FILTER);
                        log.info("Subscribed to: {}", TOPIC_FILTER);
                    } catch (MqttException e) {
                        log.error("Failed to subscribe to {}", TOPIC_FILTER, e);
                    }
                }

                @Override
                public void connectionLost(Throwable cause) {
                    log.warn("MQTT connection lost: {}", cause.getMessage());
                }

                @Override
                public void messageArrived(String topic, MqttMessage message) {
                    String payload = new String(message.getPayload());
                    handleMessage(topic, payload);
                }

                @Override
                public void deliveryComplete(IMqttDeliveryToken token) {
                }
            });

            mqttClient.connect(options);

        } catch (MqttException e) {
            log.error("Failed to connect to MQTT broker", e);
        }
    }

    /**
     * Parse topic "devices/{macAddress}/{field}" and update the device.
     */
    private void handleMessage(String topic, String payload) {
        String[] parts = topic.split("/");
        if (parts.length < 3) {
            return;
        }

        String macAddress = parts[1];
        // Support nested topics like "devices/{MAC}/config/interval"
        String field = String.join("/", java.util.Arrays.copyOfRange(parts, 2, parts.length));

        try {
            deviceService.updateDeviceField(macAddress, field, payload);
        } catch (Exception e) {
            log.error("Error processing message on topic {}: {}", topic, e.getMessage());
        }
    }

    /**
     * Publish a retained message to an MQTT topic (used for config commands).
     * Retained messages are stored by the broker and delivered to new subscribers.
     */
    public void publishRetained(String topic, String payload) {
        try {
            if (mqttClient != null && mqttClient.isConnected()) {
                MqttMessage message = new MqttMessage(payload.getBytes());
                message.setRetained(true);
                mqttClient.publish(topic, message);
                log.info("Published (retained) to {}: {}", topic, payload);
            } else {
                log.warn("Cannot publish — MQTT client is not connected");
            }
        } catch (MqttException e) {
            log.error("Failed to publish to {}", topic, e);
        }
    }

    @PreDestroy
    public void disconnect() {
        try {
            if (mqttClient != null && mqttClient.isConnected()) {
                mqttClient.disconnect();
                log.info("Disconnected from MQTT broker");
            }
        } catch (MqttException e) {
            log.error("Error disconnecting from MQTT broker", e);
        }
    }
}
