#include "SettingsSystem.h"
#include <cstring>

SettingsSystem::SettingsSystem() : scanned_count(0) {
    _settings.timezone_offset = 0;
    _settings.timezone_dst = false;
    _settings.date_format_us = false;
    _settings.saved_wifi_count = 0;
    memset(_settings.saved_wifi, 0, sizeof(_settings.saved_wifi));
    _settings.wifi_ssid[0] = '\0';
    _settings.wifi_password[0] = '\0';
    _settings.health_time_based = true;
    _settings.health_wake_hour = 8;
    _settings.health_sleep_hour = 22;
    _settings.visual_feedback_enabled = true;
    _settings.audio_feedback_enabled = true;
    _settings.audio_volume = 55;  // ~55/100 = ~55% of max volume
    _settings.skill_xp_ratio = 50;
    _settings.skill_xp_split = false;
    _connected_ssid[0] = '\0';
    memset(_networks, 0, sizeof(_networks));
}

uint8_t SettingsSystem::scanNetworks() {
    scanned_count = 0;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);

    int found = WiFi.scanNetworks(false /*async*/, false /*show_hidden*/);
    if (found <= 0) return 0;

    uint8_t lim = found < MAX_WIFI_NETWORKS ? (uint8_t)found : MAX_WIFI_NETWORKS;
    for (uint8_t i = 0; i < lim; i++) {
        strncpy(_networks[i].ssid, WiFi.SSID(i).c_str(), MAX_SSID_LEN - 1);
        _networks[i].ssid[MAX_SSID_LEN - 1] = '\0';
        _networks[i].rssi = (int8_t)WiFi.RSSI(i);
        _networks[i].has_password = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        _networks[i].channel = (uint8_t)WiFi.channel(i);
    }
    scanned_count = lim;
    WiFi.scanDelete();
    return scanned_count;
}

const WiFiNetwork* SettingsSystem::getScannedNetwork(uint8_t index) const {
    if (index >= scanned_count) return nullptr;
    return &_networks[index];
}

bool SettingsSystem::connectWiFi(const char* ssid, const char* password, uint32_t timeout_ms) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, (password && password[0] != '\0') ? password : nullptr);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeout_ms) {
            WiFi.disconnect(true);
            return false;
        }
        delay(200);
    }
    // Update legacy fields and add to saved list on success
    strncpy(_settings.wifi_ssid, ssid, MAX_SSID_LEN - 1);
    _settings.wifi_ssid[MAX_SSID_LEN - 1] = '\0';
    const char* pw = password ? password : "";
    strncpy(_settings.wifi_password, pw, MAX_PASSWORD_LEN - 1);
    _settings.wifi_password[MAX_PASSWORD_LEN - 1] = '\0';
    addSavedWiFi(ssid, pw);
    strncpy(_connected_ssid, ssid, MAX_SSID_LEN - 1);
    _connected_ssid[MAX_SSID_LEN - 1] = '\0';
    return true;
}

bool SettingsSystem::connectToBestAvailableWiFi(uint32_t timeout_ms) {
    if (_settings.saved_wifi_count == 0) return false;
    scanNetworks();
    if (scanned_count == 0) return false;

    // Find saved credential with best (highest) RSSI among visible networks
    int8_t best_rssi = -128;
    int best_saved = -1;
    for (uint8_t si = 0; si < _settings.saved_wifi_count; si++) {
        for (uint8_t ni = 0; ni < scanned_count; ni++) {
            if (strcmp(_settings.saved_wifi[si].ssid, _networks[ni].ssid) == 0) {
                if (_networks[ni].rssi > best_rssi) {
                    best_rssi = _networks[ni].rssi;
                    best_saved = si;
                }
            }
        }
    }
    if (best_saved < 0) return false;
    return connectWiFi(_settings.saved_wifi[best_saved].ssid,
                       _settings.saved_wifi[best_saved].password, timeout_ms);
}

void SettingsSystem::disconnectWiFi() {
    WiFi.disconnect(true);
    _connected_ssid[0] = '\0';
    delay(50);
}

bool SettingsSystem::isWiFiConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

const char* SettingsSystem::getConnectedSSID() const {
    if (!isWiFiConnected()) return "";
    return _connected_ssid[0] != '\0' ? _connected_ssid : _settings.wifi_ssid;
}

bool SettingsSystem::addSavedWiFi(const char* ssid, const char* password) {
    if (!ssid || ssid[0] == '\0') return false;
    // Update existing entry if SSID already saved
    for (uint8_t i = 0; i < _settings.saved_wifi_count; i++) {
        if (strcmp(_settings.saved_wifi[i].ssid, ssid) == 0) {
            if (password) {
                strncpy(_settings.saved_wifi[i].password, password, MAX_PASSWORD_LEN - 1);
                _settings.saved_wifi[i].password[MAX_PASSWORD_LEN - 1] = '\0';
            }
            return true;
        }
    }
    if (_settings.saved_wifi_count >= MAX_SAVED_WIFI) return false;
    strncpy(_settings.saved_wifi[_settings.saved_wifi_count].ssid, ssid, MAX_SSID_LEN - 1);
    _settings.saved_wifi[_settings.saved_wifi_count].ssid[MAX_SSID_LEN - 1] = '\0';
    const char* pw = password ? password : "";
    strncpy(_settings.saved_wifi[_settings.saved_wifi_count].password, pw, MAX_PASSWORD_LEN - 1);
    _settings.saved_wifi[_settings.saved_wifi_count].password[MAX_PASSWORD_LEN - 1] = '\0';
    _settings.saved_wifi_count++;
    return true;
}

bool SettingsSystem::removeSavedWiFi(uint8_t index) {
    if (index >= _settings.saved_wifi_count) return false;
    for (uint8_t i = index; i < (uint8_t)(_settings.saved_wifi_count - 1); i++) {
        _settings.saved_wifi[i] = _settings.saved_wifi[i + 1];
    }
    _settings.saved_wifi_count--;
    return true;
}

const SavedWiFiCred* SettingsSystem::getSavedWiFi(uint8_t index) const {
    if (index >= _settings.saved_wifi_count) return nullptr;
    return &_settings.saved_wifi[index];
}

void SettingsSystem::getStoredCredentials(char* ssid_out, char* pass_out) const {
    // Return first saved credential (legacy helper)
    if (_settings.saved_wifi_count > 0) {
        if (ssid_out) strncpy(ssid_out, _settings.saved_wifi[0].ssid, MAX_SSID_LEN);
        if (pass_out) strncpy(pass_out, _settings.saved_wifi[0].password, MAX_PASSWORD_LEN);
    } else {
        if (ssid_out) strncpy(ssid_out, _settings.wifi_ssid, MAX_SSID_LEN);
        if (pass_out) strncpy(pass_out, _settings.wifi_password, MAX_PASSWORD_LEN);
    }
}

void SettingsSystem::setStoredCredentials(const char* ssid, const char* pass) {
    if (ssid) {
        strncpy(_settings.wifi_ssid, ssid, MAX_SSID_LEN - 1);
        _settings.wifi_ssid[MAX_SSID_LEN - 1] = '\0';
        addSavedWiFi(ssid, pass);
    }
    if (pass) {
        strncpy(_settings.wifi_password, pass, MAX_PASSWORD_LEN - 1);
        _settings.wifi_password[MAX_PASSWORD_LEN - 1] = '\0';
    }
}
