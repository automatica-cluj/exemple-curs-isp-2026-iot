package com.iotdashboard.config;

import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;
import org.springframework.web.servlet.HandlerInterceptor;

@Component
public class AdminPasswordInterceptor implements HandlerInterceptor {

    @Value("${admin.password}")
    private String adminPassword;

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler)
            throws Exception {
        // Only intercept POST requests
        if (!"POST".equalsIgnoreCase(request.getMethod())) {
            return true;
        }

        String provided = request.getHeader("X-Admin-Password");
        if (provided == null || !provided.equals(adminPassword)) {
            response.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
            response.setContentType("application/json");
            response.getWriter().write("{\"error\": \"Invalid or missing admin password\"}");
            return false;
        }
        return true;
    }
}
