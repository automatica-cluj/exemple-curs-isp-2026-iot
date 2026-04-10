package com.iotdashboard.service;

import jakarta.annotation.PostConstruct;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.core.io.FileSystemResource;
import org.springframework.core.io.Resource;
import org.springframework.stereotype.Service;
import org.springframework.web.multipart.MultipartFile;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.time.LocalDateTime;

@Service
public class FirmwareService {

    private static final Logger log = LoggerFactory.getLogger(FirmwareService.class);

    @Value("${firmware.storage-dir:./firmware}")
    private String storageDir;

    @Value("${firmware.download-url:http://localhost:8080/api/firmware/latest}")
    private String downloadUrl;

    private Path firmwareDir;
    private String currentFilename;
    private long currentFileSize;
    private LocalDateTime uploadedAt;

    @PostConstruct
    public void init() throws IOException {
        firmwareDir = Path.of(storageDir);
        Files.createDirectories(firmwareDir);

        // Check if a firmware file already exists from a previous run
        Path existing = firmwareDir.resolve("firmware.bin");
        if (Files.exists(existing)) {
            currentFilename = "firmware.bin";
            currentFileSize = Files.size(existing);
            uploadedAt = LocalDateTime.now();
            log.info("Found existing firmware: {} ({} bytes)", currentFilename, currentFileSize);
        }
    }

    public void store(MultipartFile file) throws IOException {
        String originalName = file.getOriginalFilename();
        if (originalName == null || !originalName.endsWith(".bin")) {
            throw new IllegalArgumentException("File must have a .bin extension");
        }

        Path target = firmwareDir.resolve("firmware.bin");
        Files.copy(file.getInputStream(), target, StandardCopyOption.REPLACE_EXISTING);

        currentFilename = originalName;
        currentFileSize = file.getSize();
        uploadedAt = LocalDateTime.now();
        log.info("Stored firmware: {} ({} bytes)", currentFilename, currentFileSize);
    }

    public Resource loadFirmware() {
        Path path = firmwareDir.resolve("firmware.bin");
        if (!Files.exists(path)) {
            return null;
        }
        return new FileSystemResource(path);
    }

    public String getDownloadUrl() {
        return downloadUrl;
    }

    public boolean hasFirmware() {
        return currentFilename != null && Files.exists(firmwareDir.resolve("firmware.bin"));
    }

    public String getFilename() {
        return currentFilename;
    }

    public long getFileSize() {
        return currentFileSize;
    }

    public LocalDateTime getUploadedAt() {
        return uploadedAt;
    }
}
