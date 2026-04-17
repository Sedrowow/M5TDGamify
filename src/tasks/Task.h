#ifndef TASK_H
#define TASK_H

#include <Arduino.h>
#include <string>

#define MAX_TASK_NAME_LEN 64
#define MAX_TASK_DETAILS_LEN 256
#define MAX_TASK_LINKED_SKILLS 6

struct Task {
    char name[MAX_TASK_NAME_LEN];
    char details[MAX_TASK_DETAILS_LEN];
    uint8_t difficulty;          // 0-100%
    uint8_t urgency;             // 0-100%
    uint8_t fear;                // 0-100
    uint8_t repetition;          // times to repeat (default 1)
    uint32_t duration_seconds;   // duration in seconds (0 = none)
    uint32_t due_date;           // Unix timestamp (0 = no due date)
    uint32_t reward_money;       // reward in dollars/cents
    uint16_t reward_item_id;     // optional item reward ID (0 = none, disables money reward)
    uint8_t reward_item_quantity; // optional item reward quantity
    uint16_t linked_skill_category_id;  // optional linked skill category (0 = none)
    uint16_t linked_skill_id;           // optional linked skill (0 = none)
    uint8_t linked_skill_count;
    uint16_t linked_skill_category_ids[MAX_TASK_LINKED_SKILLS];
    uint16_t linked_skill_ids[MAX_TASK_LINKED_SKILLS];
    bool completed;
    bool failed;
    bool archived;
    uint32_t creation_time;
    uint32_t completion_time;
    uint16_t id;                 // unique task ID

    Task() : difficulty(0), urgency(0), fear(0), repetition(1), 
             duration_seconds(0), due_date(0), reward_money(0),
             reward_item_id(0), reward_item_quantity(0),
             linked_skill_category_id(0), linked_skill_id(0), linked_skill_count(0),
             completed(false), failed(false), archived(false), creation_time(0), completion_time(0), id(0) {
        name[0] = '\0';
        details[0] = '\0';
        for (uint8_t i = 0; i < MAX_TASK_LINKED_SKILLS; i++) {
            linked_skill_category_ids[i] = 0;
            linked_skill_ids[i] = 0;
        }
    }

    void syncPrimaryLinkedSkill() {
        if (linked_skill_count > 0) {
            linked_skill_category_id = linked_skill_category_ids[0];
            linked_skill_id = linked_skill_ids[0];
        } else {
            linked_skill_category_id = 0;
            linked_skill_id = 0;
        }
    }

    bool hasLinkedSkill(uint16_t skill_id) const {
        for (uint8_t i = 0; i < linked_skill_count; i++) {
            if (linked_skill_ids[i] == skill_id) return true;
        }
        return false;
    }

    bool addLinkedSkill(uint16_t category_id, uint16_t skill_id) {
        if (skill_id == 0 || hasLinkedSkill(skill_id) || linked_skill_count >= MAX_TASK_LINKED_SKILLS) {
            return false;
        }
        linked_skill_category_ids[linked_skill_count] = category_id;
        linked_skill_ids[linked_skill_count] = skill_id;
        linked_skill_count++;
        syncPrimaryLinkedSkill();
        return true;
    }

    bool removeLinkedSkill(uint16_t skill_id) {
        for (uint8_t i = 0; i < linked_skill_count; i++) {
            if (linked_skill_ids[i] != skill_id) continue;
            for (uint8_t j = i; j + 1 < linked_skill_count; j++) {
                linked_skill_category_ids[j] = linked_skill_category_ids[j + 1];
                linked_skill_ids[j] = linked_skill_ids[j + 1];
            }
            if (linked_skill_count > 0) {
                linked_skill_count--;
                linked_skill_category_ids[linked_skill_count] = 0;
                linked_skill_ids[linked_skill_count] = 0;
            }
            syncPrimaryLinkedSkill();
            return true;
        }
        return false;
    }

    void clearLinkedSkills() {
        linked_skill_count = 0;
        for (uint8_t i = 0; i < MAX_TASK_LINKED_SKILLS; i++) {
            linked_skill_category_ids[i] = 0;
            linked_skill_ids[i] = 0;
        }
        syncPrimaryLinkedSkill();
    }

    bool isTerminal() const {
        return completed || failed;
    }

    bool isVisible() const {
        return !archived;
    }

    uint32_t calculateMoneyBonus() const {
        // Bonus excludes fear by design.
        // Max bonus is capped and derived from difficulty + urgency intensity.
        uint16_t bonus_factor = (uint16_t)difficulty + (uint16_t)urgency; // 0-200
        return (uint32_t)bonus_factor * 10U;
    }

    uint32_t calculateXP() const {
        uint32_t base_xp = 100U;
        base_xp += (uint32_t)difficulty * 12U;
        base_xp += (uint32_t)urgency * 12U;
        base_xp += (uint32_t)fear * 9U;
        base_xp += (uint32_t)(repetition > 0 ? (repetition - 1) : 0) * 40U;
        base_xp += ((duration_seconds / 60U) > 480U ? 480U : (duration_seconds / 60U)) * 3U;

        if (due_date > 0 && creation_time > 0 && due_date > creation_time) {
            uint32_t due_window = due_date - creation_time;
            if (due_window <= 86400U) base_xp += 250U;
            else if (due_window <= 3U * 86400U) base_xp += 140U;
            else if (due_window <= 7U * 86400U) base_xp += 80U;
        }

        if (completed && due_date > 0 && completion_time > due_date) {
            base_xp = (base_xp * 85U) / 100U;
        }

        return base_xp;
    }
};

#endif
