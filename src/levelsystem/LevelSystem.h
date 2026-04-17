#ifndef LEVELSYSTEM_H
#define LEVELSYSTEM_H

#include <Arduino.h>
#include <cmath>

#define MIN_LEVEL 1
#define MAX_LEVEL 999
#define BASE_XP_REQUIREMENT 1500  // XP needed for level 2

/**
 * Level System for RPG-style progression
 * 
 * XP Requirements grow exponentially to create satisfying long-term progression.
 * Formula: required_xp(level) = BASE_XP_REQUIREMENT * (1.2 ^ (level - 1))
 * 
 * Example progression:
 * Level 1->2:  1,500 XP
 * Level 2->3:  1,800 XP
 * Level 3->4:  2,160 XP
 * ...
 * Level 10->11: 2,593 XP
 * Level 50->51: 117,390 XP
 */
class LevelSystem {
private:
    uint32_t current_xp;      // Current XP in this level (0 to xp_for_next_level)
    uint16_t current_level;   // Current player level (1-999)
    uint32_t lifetime_xp;     // Total XP ever earned

    /**
     * Calculate XP requirement for a specific level
     * @param level The target level
     * @return XP needed to reach that level from level-1
     */
    uint32_t calculateXPForLevel(uint16_t level) const;

public:
    LevelSystem();

    /**
     * Add XP to the player
     * @param xp_amount XP to add
     * @return number of levels gained (0 if no level up)
     */
    uint16_t addXP(uint32_t xp_amount);

    /**
     * Add XP to the player with health multiplier
     * @param xp_amount Base XP to add
     * @param health_multiplier Multiplier from health system (0.25 to 1.0)
     * @return number of levels gained (0 if no level up)
     */
    uint16_t addXPWithMultiplier(uint32_t xp_amount, float health_multiplier);

    /**
     * Get current level
     */
    uint16_t getLevel() const { return current_level; }

    /**
     * Get current XP in this level
     */
    uint32_t getCurrentXP() const { return current_xp; }

    /**
     * Get total lifetime XP earned
     */
    uint32_t getLifetimeXP() const { return lifetime_xp; }

    /**
     * Get XP required for next level
     */
    uint32_t getXPForNextLevel() const;

    /**
     * Get XP progress to next level (0-100%)
     */
    uint8_t getXPProgress() const;

    /**
     * Manually set the player level (for loading saved data)
     * Sets XP to 0 for the new level
     */
    void setLevel(uint16_t new_level);

    /**
     * Manually set current XP (for loading saved data)
     */
    void setCurrentXP(uint32_t xp_amount);

    /**
     * Manually set lifetime XP (for loading saved data)
     */
    void setLifetimeXP(uint32_t xp_amount);

    /**
     * Reset to level 1, 0 XP
     */
    void reset();

    /**
     * Print level info for debugging
     */
    void printInfo() const;
};

#endif
