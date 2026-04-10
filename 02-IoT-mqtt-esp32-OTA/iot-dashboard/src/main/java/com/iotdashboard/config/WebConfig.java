package com.iotdashboard.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.web.servlet.config.annotation.InterceptorRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

@Configuration
public class WebConfig implements WebMvcConfigurer {

    private final AdminPasswordInterceptor adminPasswordInterceptor;

    public WebConfig(AdminPasswordInterceptor adminPasswordInterceptor) {
        this.adminPasswordInterceptor = adminPasswordInterceptor;
    }

    @Override
    public void addInterceptors(InterceptorRegistry registry) {
        registry.addInterceptor(adminPasswordInterceptor)
                .addPathPatterns("/api/firmware/upload", "/api/firmware/deploy/**");
    }
}
