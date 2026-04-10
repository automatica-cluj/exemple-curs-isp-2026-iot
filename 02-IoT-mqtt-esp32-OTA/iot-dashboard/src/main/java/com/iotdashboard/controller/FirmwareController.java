package com.iotdashboard.controller;

import com.iotdashboard.dto.DeviceResponse;
import com.iotdashboard.service.DeviceService;
import com.iotdashboard.service.FirmwareService;
import com.iotdashboard.service.MqttService;
import org.springframework.core.io.Resource;
import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;

import java.io.IOException;
import java.util.Map;

@RestController
@RequestMapping("/api/firmware")
public class FirmwareController {

    private final FirmwareService firmwareService;
    private final DeviceService deviceService;
    private final MqttService mqttService;

    public FirmwareController(FirmwareService firmwareService,
                              DeviceService deviceService,
                              MqttService mqttService) {
        this.firmwareService = firmwareService;
        this.deviceService = deviceService;
        this.mqttService = mqttService;
    }

    @PostMapping("/upload")
    public ResponseEntity<Map<String, Object>> uploadFirmware(@RequestParam("file") MultipartFile file)
            throws IOException {
        firmwareService.store(file);
        return ResponseEntity.ok(Map.of(
                "filename", firmwareService.getFilename(),
                "size", firmwareService.getFileSize()
        ));
    }

    @GetMapping("/latest")
    public ResponseEntity<Resource> downloadLatestFirmware() {
        Resource firmware = firmwareService.loadFirmware();
        if (firmware == null) {
            return ResponseEntity.notFound().build();
        }
        return ResponseEntity.ok()
                .contentType(MediaType.APPLICATION_OCTET_STREAM)
                .header(HttpHeaders.CONTENT_DISPOSITION, "attachment; filename=\"firmware.bin\"")
                .body(firmware);
    }

    @GetMapping("/info")
    public ResponseEntity<Map<String, Object>> getFirmwareInfo() {
        if (!firmwareService.hasFirmware()) {
            return ResponseEntity.ok(Map.of("available", false));
        }
        return ResponseEntity.ok(Map.of(
                "available", true,
                "filename", firmwareService.getFilename(),
                "size", firmwareService.getFileSize(),
                "uploadedAt", firmwareService.getUploadedAt().toString()
        ));
    }

    @PostMapping("/deploy/{deviceId}")
    public ResponseEntity<Void> deployToDevice(@PathVariable Long deviceId) {
        DeviceResponse device = deviceService.findById(deviceId);
        String topic = "devices/" + device.macAddress() + "/ota";
        mqttService.publish(topic, firmwareService.getDownloadUrl());
        deviceService.setOtaLastAttempt(deviceId);
        return ResponseEntity.ok().build();
    }
}
