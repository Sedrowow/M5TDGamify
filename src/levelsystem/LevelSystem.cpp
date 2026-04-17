#include "LevelSystem.h"

LevelSystem::LevelSystem() : current_xp(0), current_level(1), lifetime_xp(0) {}

uint32_t LevelSystem::calculateXPForLevel(uint16_t level) const {
    if (level <= 1) return 0;
    
    // Exponential formula: BASE * (1.2 ^ (level - 2))
    // We use pow which should be available in Arduino
    float exponent = (float)(level - 2);
    uint32_t required_xp = (uint32_t)(BASE_XP_REQUIREMENT * pow(1.2f, exponent) + 0.5f);
    
    // Cap to prevent overflow
    if (required_xp > 1000000000U) {
        required_xp = 1000000000U;
    }
    
    return required_xp;
}

uint16_t LevelSystem::addXP(uint32_t xp_amount) {
    current_xp += xp_amount;
    lifetime_xp += xp_amount;
    
    uint16_t levels_gained = 0;
    
    // Level up loop
    while (current_xp >= getXPForNextLevel() && current_level < MAX_LEVEL) {
        current_xp -= getXPForNextLevel();
        current_level++;
        levels_gained++;
    }
    
    return levels_gained;
}

uint16_t LevelSystem::addXPWithMultiplier(uint32_t xp_amount, float health_multiplier) {
    // Apply health multiplier to XP
    uint32_t adjusted_xp = (uint32_t)(xp_amount * health_multiplier + 0.5f);
    
    // Add to level system
    current_xp += adjusted_xp;
    lifetime_xp += adjusted_xp;
    
    uint16_t levels_gained = 0;
    
    // Level up loop
    while (current_xp >= getXPForNextLevel() && current_level < MAX_LEVEL) {
        current_xp -= getXPForNextLevel();
        current_level++;
        levels_gained++;
    }
    
    return levels_gained;
}

uint32_t LevelSystem::getXPForNextLevel() const {
    return calculateXPForLevel(current_level + 1);
}

uint8_t LevelSystem::getXPProgress() const {
    uint32_t required = getXPForNextLevel();
    if (required == 0) {
        return 100;  // Max level
    }
    return (uint8_t)((current_xp * 100) / required);
}

void LevelSystem::setLevel(uint16_t new_level) {
    if (new_level < MIN_LEVEL) new_level = MIN_LEVEL;
    if (new_level > MAX_LEVEL) new_level = MAX_LEVEL;
    
    current_level = new_level;
    current_xp = 0;
}

void LevelSystem::setCurrentXP(uint32_t xp_amount) {
    current_xp = xp_amount;
    
    // Validate bounds against next level requirement
    uint32_t next_level_required = getXPForNextLevel();
    if (current_xp >= next_level_required) {
        current_xp = next_level_required - 1;  // Cap to just before level up
    }
}

void LevelSystem::setLifetimeXP(uint32_t xp_amount) {
    lifetime_xp = xp_amount;
}

void LevelSystem::reset() {
    current_level = 1;
    current_xp = 0;
    lifetime_xp = 0;
}

void LevelSystem::printInfo() const {
    Serial.printf("=== Level System Info ===\n");
    Serial.printf("Current Level: %d\n", current_level);
    Serial.printf("Current XP: %lu / %lu\n", current_xp, getXPForNextLevel());
    Serial.printf("XP Progress: %d%%\n", getXPProgress());
    Serial.printf("Lifetime XP: %lu\n", lifetime_xp);
    Serial.printf("========================\n");
}
