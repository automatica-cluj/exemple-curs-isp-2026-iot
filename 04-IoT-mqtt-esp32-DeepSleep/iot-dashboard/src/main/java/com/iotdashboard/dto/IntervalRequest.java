package com.iotdashboard.dto;

import jakarta.validation.constraints.Max;
import jakarta.validation.constraints.Min;

public record IntervalRequest(
        @Min(value = 10, message = "Interval must be at least 10 seconds")
        @Max(value = 3600, message = "Interval must be at most 3600 seconds (1 hour)")
        int seconds
) {}
