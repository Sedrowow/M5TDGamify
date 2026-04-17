#ifndef HEALTHSYSTEM_H
#define HEALTHSYSTEM_H

#include <Arduino.h>
#include <ctime>

#define HEALTH_MIN 0
#define HEALTH_MAX 100

/**
 * Health Mode determines how health is calculated
 */
enum HealthMode {
    HEALTH_MANUAL = 0,      // Player manually sets health level
    HEALTH_TIME_BASED = 1   // Health follows daily time cycle
};

/**
 * Health System for RPG-style health management
 * 
 * Supports two modes:
 * 1. Manual: Player directly sets health (0-100%)
 * 2. Time-Based: Health cycles throughout the day (100% at start_time, 0% at end_time)
 * 
 * Health affects XP gain in quarters:
 * - Quarter 1 (100-75%): 100% XP multiplier
 * - Quarter 2 (74-50%):  75% XP multiplier
 * - Quarter 3 (49-25%):  50% XP multiplier
 * - Quarter 4 (24-0%):   25% XP multiplier
 */
class HealthSystem {
private:
    HealthMode mode;
    uint8_t manual_health;              // Manual health level (0-100)
    time_t time_based_start_hour;       // Start of day (hour 0-23)
    time_t time_based_end_hour;         // End of day (hour 0-23)
    time_t current_time;                // Current Unix timestamp

    /**
     * Calculate health based on current time and time-based cycle
     * Health smoothly interpolates from 100% at start_time to 0% at end_time
     */
    uint8_t calculateTimeBasedHealth() const;

public:
    HealthSystem();

    /**
     * Get current health (0-100%)
     */
    uint8_t getHealth() const;

    /**
     * Get XP multiplier based on health quarter
     * Quarter 1 (100-75%): 1.00x
     * Quarter 2 (74-50%):  0.75x
     * Quarter 3 (49-25%):  0.50x
     * Quarter 4 (24-0%):   0.25x
     */
    float getXPMultiplier() const;

    /**
     * Get health quarter (1-4)
     * Quarter 1: 100-75%
     * Quarter 2: 74-50%
     * Quarter 3: 49-25%
     * Quarter 4: 24-0%
     */
    uint8_t getHealthQuarter() const;

    /**
     * Apply XP multiplier to XP amount
     */
    uint32_t applyHealthMultiplier(uint32_t base_xp) const;

    // ===== Manual Mode =====

    /**
     * Set health system to manual mode
     */
    void setManualMode();

    /**
     * Set health manually (0-100)
     */
    void setManualHealth(uint8_t health);

    /**
     * Get manual health setting
     */
    uint8_t getManualHealth() const { return manual_health; }

    // ===== Time-Based Mode =====

    /**
     * Set health system to time-based mode
     */
    void setTimeBasedMode();

    /**
     * Set time-based health cycle
     * @param start_hour Hour of day when health is 100% (0-23)
     * @param end_hour Hour of day when health is 0% (0-23)
     * 
     * Example: start_hour=8 (8 AM = 100%), end_hour=22 (10 PM = 0%)
     */
    void setTimeBasedCycle(uint8_t start_hour, uint8_t end_hour);

    /**
     * Get start hour of time-based cycle
     */
    uint8_t getTimeBasedStartHour() const { return (uint8_t)time_based_start_hour; }

    /**
     * Get end hour of time-based cycle
     */
    uint8_t getTimeBasedEndHour() const { return (uint8_t)time_based_end_hour; }

    // ===== Time Management =====

    /**
     * Update current time (should be called regularly or when time is synced)
     * @param unix_timestamp Current Unix timestamp
     */
    void setCurrentTime(time_t unix_timestamp);

    /**
     * Get current time
     */
    time_t getCurrentTime() const { return current_time; }

    /**
     * Get current hour of day (0-23)
     */
    uint8_t getCurrentHour() const;

    /**
     * Get current minute of hour (0-59)
     */
    uint8_t getCurrentMinute() const;

    /**
     * Get current mode
     */
    HealthMode getMode() const { return mode; }

    /**
     * Is in manual mode?
     */
    bool isManualMode() const { return mode == HEALTH_MANUAL; }

    /**
     * Is in time-based mode?
     */
    bool isTimeBasedMode() const { return mode == HEALTH_TIME_BASED; }

    /**
     * Reset to default (manual mode, 100% health)
     */
    void reset();

    /**
     * Print health info for debugging
     */
    void printInfo() const;
};

#endif
