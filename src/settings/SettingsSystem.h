#ifndef SETTINGSSYSTEM_H
#define SETTINGSSYSTEM_H

#include <Arduino.h>
#include <WiFi.h>

#define MAX_WIFI_NETWORKS 16
#define MAX_SAVED_WIFI    8
#ifndef MAX_SSID_LEN
#define MAX_SSID_LEN      33
#endif
#ifndef MAX_PASSWORD_LEN
#define MAX_PASSWORD_LEN  64
#endif

struct WiFiNetwork {
    char ssid[MAX_SSID_LEN];
    int8_t rssi;
    bool has_password;
    uint8_t channel;
};

struct SavedWiFiCred {
    char ssid[MAX_SSID_LEN];
    char password[MAX_PASSWORD_LEN];
};

// Settings that are persisted globally (not per-profile)
struct GlobalSettings {
    int8_t timezone_offset;    // UTC offset in hours
    bool timezone_dst;         // +1h daylight saving
    bool date_format_us;       // true: MM/DD/YYYY, false: DD/MM/YYYY

    // Multi-credential WiFi list
    SavedWiFiCred saved_wifi[MAX_SAVED_WIFI];
    uint8_t saved_wifi_count;
    // Legacy single-credential (kept for backward-compat / quick reconnect)
    char wifi_ssid[MAX_SSID_LEN];
    char wifi_password[MAX_PASSWORD_LEN];

    bool health_time_based;
    uint8_t health_wake_hour;
    uint8_t health_sleep_hour;
    bool visual_feedback_enabled;
    bool audio_feedback_enabled;
    uint8_t audio_volume;      // 0-100 (displayed), mapped to 0-255 for speaker

    // Skill XP settings
    uint8_t skill_xp_ratio;   // % of player XP awarded to each linked skill (0-100, default 50)
    bool skill_xp_split;      // if true, total skill XP is split evenly among linked skills
};

class SettingsSystem {
public:
    SettingsSystem();

    // ---- WiFi ----
    // Scan visible networks (blocking, up to ~2 s). Returns count found.
    uint8_t scanNetworks();
    uint8_t getScannedCount() const { return scanned_count; }
    const WiFiNetwork* getScannedNetwork(uint8_t index) const;

    // Try connecting to a network. Returns true when connected.
    // On success, adds the credential to saved_wifi list automatically.
    bool connectWiFi(const char* ssid, const char* password, uint32_t timeout_ms = 8000);
    // Scan and connect to the saved network with best available signal.
    bool connectToBestAvailableWiFi(uint32_t timeout_ms = 8000);
    void disconnectWiFi();
    bool isWiFiConnected() const;
    const char* getConnectedSSID() const;

    // Saved-credential management
    bool addSavedWiFi(const char* ssid, const char* password);
    bool removeSavedWiFi(uint8_t index);
    uint8_t getSavedWiFiCount() const { return _settings.saved_wifi_count; }
    const SavedWiFiCred* getSavedWiFi(uint8_t index) const;

    // Legacy single-credential helpers (use first saved_wifi entry)
    void getStoredCredentials(char* ssid_out, char* pass_out) const;
    void setStoredCredentials(const char* ssid, const char* pass);

    // ---- Settings data ----
    GlobalSettings& settings() { return _settings; }
    const GlobalSettings& settings() const { return _settings; }
    void load(GlobalSettings& s) { _settings = s; }

private:
    WiFiNetwork _networks[MAX_WIFI_NETWORKS];
    uint8_t scanned_count;
    GlobalSettings _settings;
    char _connected_ssid[MAX_SSID_LEN];
};

#endif // SETTINGSSYSTEM_H
