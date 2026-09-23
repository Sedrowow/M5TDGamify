#include "HealthSystem.h"

HealthSystem::HealthSystem() 
    : mode(HEALTH_MANUAL), 
    manual_health(100),
    manual_health_points(HEALTH_DEFAULT_MAX_POINTS),
    max_health_points(HEALTH_DEFAULT_MAX_POINTS),
    base_max_health_points(HEALTH_DEFAULT_MAX_POINTS),
    player_level(1),
    level_scaling_enabled(false),
    level_scaling_percent(false),
    level_scaling_growth(10),
      time_based_start_hour(8),    // 8 AM
      time_based_end_hour(22),     // 10 PM
      current_time(0) {}

uint8_t HealthSystem::calculateTimeBasedHealth() const {
    if (current_time == 0) {
        return 100;  // No time set, assume full health
    }

    // Convert Unix timestamp to time struct
    time_t local_time = current_time;
    struct tm* time_info = localtime(&local_time);
    
    uint8_t current_hour = time_info->tm_hour;
    uint8_t current_min = time_info->tm_min;

    // Convert to minutes since midnight
    uint32_t current_minutes = (uint32_t)current_hour * 60 + current_min;
    uint32_t start_minutes = (uint32_t)time_based_start_hour * 60;
    uint32_t end_minutes = (uint32_t)time_based_end_hour * 60;

    // Before start time: 100% health
    if (current_minutes < start_minutes) {
        return 100;
    }

    // After end time: 0% health
    if (current_minutes >= end_minutes) {
        return 0;
    }

    // Between start and end: interpolate linearly
    uint32_t active_period = end_minutes - start_minutes;
    uint32_t elapsed = current_minutes - start_minutes;
    
    if (active_period == 0) {
        return 100;
    }

    // Calculate health as percentage
    uint8_t health = 100 - (uint8_t)((elapsed * 100) / active_period);
    
    return health;
}

uint8_t HealthSystem::getHealth() const {
    if (mode == HEALTH_MANUAL) {
        return (uint8_t)(((uint32_t)manual_health_points * 100U + max_health_points / 2U) / max_health_points);
    } else {
        return calculateTimeBasedHealth();
    }
}

uint16_t HealthSystem::getHealthPoints() const {
    return (uint16_t)(((uint32_t)getHealth() * max_health_points + 50U) / 100U);
}

float HealthSystem::getXPMultiplier() const {
    uint8_t quarter = getHealthQuarter();
    
    switch (quarter) {
        case 1: return 1.00f;   // 100-75%
        case 2: return 0.75f;   // 74-50%
        case 3: return 0.50f;   // 49-25%
        case 4: return 0.25f;   // 24-0%
        default: return 1.00f;
    }
}

uint8_t HealthSystem::getHealthQuarter() const {
    uint8_t health = getHealth();
    
    if (health >= 75) {
        return 1;  // 100-75%
    } else if (health >= 50) {
        return 2;  // 74-50%
    } else if (health >= 25) {
        return 3;  // 49-25%
    } else {
        return 4;  // 24-0%
    }
}

uint32_t HealthSystem::applyHealthMultiplier(uint32_t base_xp) const {
    float multiplier = getXPMultiplier();
    return (uint32_t)(base_xp * multiplier + 0.5f);
}

void HealthSystem::setManualMode() {
    mode = HEALTH_MANUAL;
}

void HealthSystem::setManualHealth(uint8_t health) {
    if (health > 100) health = 100;
    manual_health = health;
    manual_health_points = (uint16_t)(((uint32_t)health * max_health_points + 50U) / 100U);
}

void HealthSystem::setManualHealthPoints(uint16_t health_points) {
    if (health_points > max_health_points) health_points = max_health_points;
    manual_health_points = health_points;
    manual_health = (uint8_t)(((uint32_t)health_points * 100U + max_health_points / 2U) / max_health_points);
}

void HealthSystem::setMaxHealthPoints(uint16_t max_points) {
    if (max_points == 0) max_points = HEALTH_DEFAULT_MAX_POINTS;
    base_max_health_points = max_points;
    updateMaxHealthForLevel(player_level);
}

void HealthSystem::setBaseMaxHealthPoints(uint16_t base_points) {
    if (base_points == 0) base_points = HEALTH_DEFAULT_MAX_POINTS;
    base_max_health_points = base_points;
    updateMaxHealthForLevel(player_level);
}

void HealthSystem::configureLevelScaling(bool enabled, bool percent, uint16_t growth) {
    level_scaling_enabled = enabled;
    level_scaling_percent = percent;
    level_scaling_growth = growth;
    updateMaxHealthForLevel(player_level);
}

void HealthSystem::updateMaxHealthForLevel(uint16_t level) {
    if (level == 0) level = 1;
    player_level = level;
    uint16_t old_max = max_health_points;
    uint16_t old_points = manual_health_points;
    uint32_t calculated = base_max_health_points;
    if (level_scaling_enabled) {
        if (level_scaling_percent) {
            for (uint16_t current = 1; current < level && calculated < 65535U; current++) {
                calculated += (calculated * level_scaling_growth + 50U) / 100U;
            }
        } else {
            calculated += (uint32_t)(level - 1) * level_scaling_growth;
        }
    }
    max_health_points = calculated > 65535U ? 65535U : (uint16_t)calculated;
    if (old_max > 0 && old_points > 0 && old_max != max_health_points) {
        manual_health_points = (uint16_t)(((uint32_t)old_points * max_health_points + old_max / 2U) / old_max);
    }
    if (manual_health_points > max_health_points) manual_health_points = max_health_points;
    manual_health = (uint8_t)(((uint32_t)manual_health_points * 100U + max_health_points / 2U) / max_health_points);
}

void HealthSystem::modifyMaxHealth(int32_t delta) {
    int32_t updated = (int32_t)base_max_health_points + delta;
    if (updated < 1) updated = 1;
    if (updated > 65535) updated = 65535;
    base_max_health_points = (uint16_t)updated;
    updateMaxHealthForLevel(player_level);
}

void HealthSystem::setTimeBasedMode() {
    mode = HEALTH_TIME_BASED;
}

void HealthSystem::setTimeBasedCycle(uint8_t start_hour, uint8_t end_hour) {
    if (start_hour > 23) start_hour = 23;
    if (end_hour > 23) end_hour = 23;
    
    time_based_start_hour = start_hour;
    time_based_end_hour = end_hour;
}

void HealthSystem::setCurrentTime(time_t unix_timestamp) {
    current_time = unix_timestamp;
}

uint8_t HealthSystem::getCurrentHour() const {
    if (current_time == 0) return 0;
    
    time_t local_time = current_time;
    struct tm* time_info = localtime(&local_time);
    return time_info->tm_hour;
}

uint8_t HealthSystem::getCurrentMinute() const {
    if (current_time == 0) return 0;
    
    time_t local_time = current_time;
    struct tm* time_info = localtime(&local_time);
    return time_info->tm_min;
}

void HealthSystem::reset() {
    mode = HEALTH_MANUAL;
    manual_health = 100;
    manual_health_points = HEALTH_DEFAULT_MAX_POINTS;
    max_health_points = HEALTH_DEFAULT_MAX_POINTS;
    base_max_health_points = HEALTH_DEFAULT_MAX_POINTS;
    player_level = 1;
    level_scaling_enabled = false;
    level_scaling_percent = false;
    level_scaling_growth = 10;
    time_based_start_hour = 8;
    time_based_end_hour = 22;
}

void HealthSystem::printInfo() const {
    Serial.printf("=== Health System Info ===\n");
    
    if (mode == HEALTH_MANUAL) {
        Serial.printf("Mode: MANUAL\n");
        Serial.printf("Health: %d%%\n", manual_health);
    } else {
        Serial.printf("Mode: TIME-BASED\n");
        Serial.printf("Cycle: %02d:00 - %02d:00\n", 
                      (uint8_t)time_based_start_hour, (uint8_t)time_based_end_hour);
        Serial.printf("Current Health: %d%%\n", getHealth());
        Serial.printf("Current Time: %02d:%02d\n", getCurrentHour(), getCurrentMinute());
    }
    
    Serial.printf("Health Quarter: %d\n", getHealthQuarter());
    Serial.printf("XP Multiplier: %.2fx\n", getXPMultiplier());
    Serial.printf("===========================\n");
}
