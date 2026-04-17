#ifndef TASK_H
#define TASK_H

#include <Arduino.h>
#include <string>

#define MAX_TASK_NAME_LEN 64
#define MAX_TASK_DETAILS_LEN 256

struct Task {
    char name[MAX_TASK_NAME_LEN];
    char details[MAX_TASK_DETAILS_LEN];
    uint8_t difficulty;          // 0-100%
    uint8_t urgency;             // 0-100%
    uint8_t fear;                // 0-100
    uint8_t repetition;          // times to repeat (default 1)
    uint16_t duration_minutes;   // duration in minutes (0 = none)
    uint32_t due_date;           // Unix timestamp (0 = no due date)
    uint32_t reward_money;       // reward in dollars/cents
    uint16_t reward_item_id;     // optional item reward ID (0 = none, disables money reward)
    uint8_t reward_item_quantity; // optional item reward quantity
    uint16_t linked_skill_category_id;  // optional linked skill category (0 = none)
    uint16_t linked_skill_id;           // optional linked skill (0 = none)
    bool completed;
    uint32_t creation_time;
    uint32_t completion_time;
    uint16_t id;                 // unique task ID

    Task() : difficulty(0), urgency(0), fear(0), repetition(1), 
             duration_minutes(0), due_date(0), reward_money(0),
             reward_item_id(0), reward_item_quantity(0),
             linked_skill_category_id(0), linked_skill_id(0),
             completed(false), creation_time(0), completion_time(0), id(0) {
        name[0] = '\0';
        details[0] = '\0';
    }

    uint32_t calculateMoneyBonus() const {
        // Bonus excludes fear by design.
        // Max bonus is capped and derived from difficulty + urgency intensity.
        uint16_t bonus_factor = (uint16_t)difficulty + (uint16_t)urgency; // 0-200
        return (uint32_t)bonus_factor * 10U;
    }

    /**
     * Calculate XP gain based on difficulty, urgency, and fear
     * Maximum XP: ~1.4 million (when all are maxed at 100)
     * Formula: (difficulty * 4000) + (urgency * 4000) + (fear * 3400) = 1,340,000 max
     */
    uint32_t calculateXP() const {
        uint32_t difficulty_xp = (uint32_t)difficulty * 4000;  // 0-400,000
        uint32_t urgency_xp = (uint32_t)urgency * 4000;        // 0-400,000
        uint32_t fear_xp = (uint32_t)fear * 3400;              // 0-340,000
        return difficulty_xp + urgency_xp + fear_xp;
    }
};

#endif
