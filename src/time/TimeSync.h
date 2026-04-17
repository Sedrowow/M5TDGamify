#ifndef TIMESYNC_H
#define TIMESYNC_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <ctime>

/**
 * Time Synchronization System
 * 
 * Handles:
 * - WiFi-based NTP time synchronization
 * - Timezone offset management (UTC+/- format)
 * - Manual time setting
 * - Automatic timezone calculation
 */
class TimeSync {
private:
    int8_t timezone_offset;     // UTC offset in hours (-12 to +14)
    bool daylight_saving;       // Add +1h when enabled
    bool date_format_us;        // true MM/DD/YYYY, false DD/MM/YYYY
    time_t last_sync_time;      // When time was last synced
    bool wifi_connected;        // WiFi connection status

    /**
     * Helper to configure NTP with timezone
     */
    void configureNTP() const;

public:
    TimeSync();

    /**
     * Set timezone offset from UTC
     * @param offset Hours from UTC (-12 to +14)
     * 
     * Examples:
     * - UTC+0:     offset = 0
     * - UTC+1:     offset = 1
     * - UTC-5:     offset = -5
     */
    void setTimezoneOffset(int8_t offset);
    void setDaylightSavingEnabled(bool enabled);
    bool isDaylightSavingEnabled() const { return daylight_saving; }
    void setDateFormatUS(bool enabled);
    bool isDateFormatUS() const { return date_format_us; }

    /**
     * Get current timezone offset
     */
    int8_t getTimezoneOffset() const { return timezone_offset; }

    /**
     * Get timezone offset as string (e.g., "UTC+1", "UTC-5")
     */
    String getTimezoneString() const;

    /**
     * Attempt WiFi connection and NTP sync
     * @param ssid WiFi SSID
     * @param password WiFi password
     * @param timeout_ms How long to wait for WiFi connection
     * @return true if sync was successful
     */
    bool syncWithWiFi(const char* ssid, const char* password, uint32_t timeout_ms = 5000);

    /**
     * Manual time setting
     * @param hour Hour (0-23)
     * @param minute Minute (0-59)
     * @param second Second (0-59)
     * @param day Day of month (1-31)
     * @param month Month (1-12)
     * @param year Year (2000+)
     */
    void setManualTime(uint8_t hour, uint8_t minute, uint8_t second,
                       uint8_t day, uint8_t month, uint16_t year);

    /**
     * Get current time as Unix timestamp
     */
    time_t getCurrentTime() const;

    /**
     * Get current time as formatted string
     * Format: "DD/MM/YYYY HH:MM:SS"
     */
    String getCurrentTimeString() const;

    /**
     * Get time since last sync
     */
    time_t getTimeSinceSync() const;

    /**
     * Check if time has been set
     */
    bool isTimeSet() const;

    /**
     * Get WiFi connection status
     */
    bool isWiFiConnected() const { return wifi_connected; }

    /**
     * Disconnect WiFi
     */
    void disconnectWiFi();

    /**
     * Update WiFi connection status (call periodically)
     */
    void update();

    /**
     * Print time info for debugging
     */
    void printInfo() const;

    /**
     * Timezone helper: Calculate appropriate offset for a region
     * Common offsets: -5 (EST), 0 (GMT), +1 (CET), +8 (CST), +9 (JST)
     */
    static int8_t getRegionOffset(const char* region_name);
};

#endif
