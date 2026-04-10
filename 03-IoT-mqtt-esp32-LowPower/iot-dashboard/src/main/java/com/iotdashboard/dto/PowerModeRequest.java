package com.iotdashboard.dto;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Pattern;

public record PowerModeRequest(
        @NotBlank(message = "Mode is required")
        @Pattern(regexp = "low|normal", message = "Mode must be 'low' or 'normal'")
        String mode
) {}
