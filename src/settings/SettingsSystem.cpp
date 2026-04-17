#include "SettingsSystem.h"
#include <cstring>

SettingsSystem::SettingsSystem() : scanned_count(0) {
    _settings.timezone_offset = 0;
    _settings.timezone_dst = false;
    _settings.date_format_us = false;
    _settings.wifi_ssid[0] = '\0';
    _settings.wifi_password[0] = '\0';
    _settings.health_time_based = true;
    _settings.health_wake_hour = 8;
    _settings.health_sleep_hour = 22;
    _settings.visual_feedback_enabled = true;
    _settings.audio_feedback_enabled = true;
    _settings.audio_volume = 140;
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
    // Store on success
    strncpy(_settings.wifi_ssid, ssid, MAX_SSID_LEN - 1);
    _settings.wifi_ssid[MAX_SSID_LEN - 1] = '\0';
    if (password) {
        strncpy(_settings.wifi_password, password, MAX_PASSWORD_LEN - 1);
        _settings.wifi_password[MAX_PASSWORD_LEN - 1] = '\0';
    } else {
        _settings.wifi_password[0] = '\0';
    }
    strncpy(_connected_ssid, _settings.wifi_ssid, MAX_SSID_LEN - 1);
    _connected_ssid[MAX_SSID_LEN - 1] = '\0';
    return true;
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

void SettingsSystem::getStoredCredentials(char* ssid_out, char* pass_out) const {
    if (ssid_out) strncpy(ssid_out, _settings.wifi_ssid, MAX_SSID_LEN);
    if (pass_out) strncpy(pass_out, _settings.wifi_password, MAX_PASSWORD_LEN);
}

void SettingsSystem::setStoredCredentials(const char* ssid, const char* pass) {
    if (ssid) {
        strncpy(_settings.wifi_ssid, ssid, MAX_SSID_LEN - 1);
        _settings.wifi_ssid[MAX_SSID_LEN - 1] = '\0';
    }
    if (pass) {
        strncpy(_settings.wifi_password, pass, MAX_PASSWORD_LEN - 1);
        _settings.wifi_password[MAX_PASSWORD_LEN - 1] = '\0';
    }
}
