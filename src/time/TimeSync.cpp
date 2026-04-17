#include "TimeSync.h"
#include <esp_sntp.h>

// NTP Server configuration
#define NTP_SERVER_0 "pool.ntp.org"
#define NTP_SERVER_1 "time.nist.gov"
#define NTP_UPDATE_INTERVAL 3600  // Resync every hour

TimeSync::TimeSync() 
    : timezone_offset(0), last_sync_time(0), wifi_connected(false) {}

void TimeSync::configureNTP() const {
    // Configure SNTP
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, NTP_SERVER_0);
    sntp_setservername(1, NTP_SERVER_1);
    sntp_init();
}

void TimeSync::setTimezoneOffset(int8_t offset) {
    if (offset < -12) offset = -12;
    if (offset > 14) offset = 14;
    
    timezone_offset = offset;
    
    // Set timezone environment variable
    char tz_env[32];
    if (offset >= 0) {
        snprintf(tz_env, sizeof(tz_env), "UTC-%d", offset);
    } else {
        snprintf(tz_env, sizeof(tz_env), "UTC+%d", -offset);
    }
    
    setenv("TZ", tz_env, 1);
    tzset();
}

String TimeSync::getTimezoneString() const {
    char tz_str[16];
    if (timezone_offset >= 0) {
        snprintf(tz_str, sizeof(tz_str), "UTC+%d", timezone_offset);
    } else {
        snprintf(tz_str, sizeof(tz_str), "UTC%d", timezone_offset);  // Already negative
    }
    return String(tz_str);
}

bool TimeSync::syncWithWiFi(const char* ssid, const char* password, uint32_t timeout_ms) {
    // Reuse existing connection if already connected.
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, (password && password[0] != '\0') ? password : nullptr);

        Serial.printf("[TimeSync] Connecting to WiFi: %s\n", ssid);

        uint32_t start_time = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start_time) < timeout_ms) {
            delay(300);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[TimeSync] WiFi connection failed!");
            return false;
        }
    }

    Serial.println("[TimeSync] WiFi connected!");
    wifi_connected = true;

    // Configure NTP
    configureNTP();

    // Wait for NTP sync
    Serial.print("[TimeSync] Syncing time from NTP");
    time_t now = time(nullptr);
    while (now < 24 * 3600) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println();

    last_sync_time = now;
    Serial.printf("[TimeSync] Time synced! Current: %s\n", getCurrentTimeString().c_str());

    return true;
}

void TimeSync::setManualTime(uint8_t hour, uint8_t minute, uint8_t second,
                              uint8_t day, uint8_t month, uint16_t year) {
    struct tm timeinfo = {};
    
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    timeinfo.tm_mday = day;
    timeinfo.tm_mon = month - 1;  // months are 0-indexed in struct tm
    timeinfo.tm_year = year - 1900;  // years since 1900

    time_t new_time = mktime(&timeinfo);
    
    // Set system time
    struct timeval tv;
    tv.tv_sec = new_time;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    last_sync_time = new_time;
    Serial.printf("[TimeSync] Manual time set to: %s\n", getCurrentTimeString().c_str());
}

time_t TimeSync::getCurrentTime() const {
    return time(nullptr);
}

String TimeSync::getCurrentTimeString() const {
    time_t now = getCurrentTime();
    struct tm* timeinfo = localtime(&now);
    
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", timeinfo);
    
    return String(buffer);
}

time_t TimeSync::getTimeSinceSync() const {
    if (last_sync_time == 0) return 0;
    return getCurrentTime() - last_sync_time;
}

bool TimeSync::isTimeSet() const {
    // If time is before year 2000, it hasn't been set
    time_t now = getCurrentTime();
    return now > 946684800;  // Unix timestamp for 2000-01-01
}

void TimeSync::disconnectWiFi() {
    WiFi.disconnect(true);  // true = turn off WiFi radio
    wifi_connected = false;
    Serial.println("[TimeSync] WiFi disconnected");
}

void TimeSync::update() {
    // Update WiFi status
    bool previously_connected = wifi_connected;
    wifi_connected = (WiFi.status() == WL_CONNECTED);

    // Print status change if connection state changed
    if (wifi_connected && !previously_connected) {
        Serial.println("[TimeSync] WiFi connected");
    } else if (!wifi_connected && previously_connected) {
        Serial.println("[TimeSync] WiFi disconnected");
    }
}

void TimeSync::printInfo() const {
    Serial.printf("=== Time Sync Info ===\n");
    Serial.printf("Timezone: %s\n", getTimezoneString().c_str());
    Serial.printf("Current Time: %s\n", getCurrentTimeString().c_str());
    Serial.printf("Time Set: %s\n", isTimeSet() ? "Yes" : "No");
    Serial.printf("WiFi Connected: %s\n", wifi_connected ? "Yes" : "No");
    
    if (last_sync_time > 0) {
        uint32_t seconds_since_sync = (uint32_t)getTimeSinceSync();
        uint32_t minutes = seconds_since_sync / 60;
        uint32_t seconds = seconds_since_sync % 60;
        Serial.printf("Last Sync: %lu minutes, %lu seconds ago\n", minutes, seconds);
    } else {
        Serial.printf("Last Sync: Never\n");
    }
    
    Serial.printf("======================\n");
}

int8_t TimeSync::getRegionOffset(const char* region_name) {
    // Common timezone offsets
    if (strcmp(region_name, "PST") == 0) return -8;   // Pacific Standard Time
    if (strcmp(region_name, "MST") == 0) return -7;   // Mountain Standard Time
    if (strcmp(region_name, "CST") == 0) return -6;   // Central Standard Time
    if (strcmp(region_name, "EST") == 0) return -5;   // Eastern Standard Time
    if (strcmp(region_name, "GMT") == 0) return 0;    // Greenwich Mean Time
    if (strcmp(region_name, "CET") == 0) return 1;    // Central European Time
    if (strcmp(region_name, "EET") == 0) return 2;    // Eastern European Time
    if (strcmp(region_name, "IST") == 0) return 5;    // Indian Standard Time (India)
    if (strcmp(region_name, "JST") == 0) return 9;    // Japan Standard Time
    if (strcmp(region_name, "AEST") == 0) return 10;  // Australian Eastern Standard Time
    if (strcmp(region_name, "NZST") == 0) return 12;  // New Zealand Standard Time
    
    return 0;  // Default to UTC
}
