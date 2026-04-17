#ifndef SKILLSYSTEM_H
#define SKILLSYSTEM_H

#include <Arduino.h>
#include <cmath>

#define MAX_SKILL_CATEGORIES 24
#define MAX_SKILLS 128

#define MAX_SKILL_CATEGORY_NAME_LEN 32
#define MAX_SKILL_CATEGORY_DESC_LEN 96
#define MAX_SKILL_NAME_LEN 48
#define MAX_SKILL_DETAILS_LEN 128

#define MIN_SKILL_LEVEL 1
#define MAX_SKILL_LEVEL 999
#define SKILL_BASE_XP_REQUIREMENT 2200U
#define SKILL_XP_GROWTH 1.18f
#define DEFAULT_SKILL_XP_MULTIPLIER 0.25f

struct SkillCategory {
    uint16_t id;
    char name[MAX_SKILL_CATEGORY_NAME_LEN];
    char description[MAX_SKILL_CATEGORY_DESC_LEN];
    uint32_t creation_time;
    bool active;

    SkillCategory() : id(0), creation_time(0), active(false) {
        name[0] = '\0';
        description[0] = '\0';
    }
};

struct Skill {
    uint16_t id;
    uint16_t category_id;
    char name[MAX_SKILL_NAME_LEN];
    char details[MAX_SKILL_DETAILS_LEN];

    uint16_t level;
    uint32_t current_xp;
    uint32_t lifetime_xp;

    uint32_t creation_time;
    bool active;

    Skill() : id(0), category_id(0), level(1), current_xp(0), lifetime_xp(0), creation_time(0), active(false) {
        name[0] = '\0';
        details[0] = '\0';
    }
};

class SkillSystem {
private:
    SkillCategory categories[MAX_SKILL_CATEGORIES];
    Skill skills[MAX_SKILLS];

    uint16_t category_count;
    uint16_t skill_count;
    uint16_t next_category_id;
    uint16_t next_skill_id;

    uint32_t calculateXPForSkillLevel(uint16_t level) const;

public:
    SkillSystem();

    uint16_t addCategory(const char* name, const char* description = nullptr);
    SkillCategory* getCategory(uint16_t category_id);
    const SkillCategory* getCategory(uint16_t category_id) const;
    bool removeCategory(uint16_t category_id);
    uint16_t getCategoryCount() const { return category_count; }
    uint16_t getActiveCategoryCount() const;
    const SkillCategory* getCategoryByActiveIndex(uint16_t index) const;
    uint16_t findCategoryByName(const char* name) const;

    uint16_t addSkill(uint16_t category_id, const char* name, const char* details = nullptr);
    Skill* getSkill(uint16_t skill_id);
    const Skill* getSkill(uint16_t skill_id) const;
    bool removeSkill(uint16_t skill_id);
    uint16_t getSkillCount() const { return skill_count; }
    uint16_t getActiveSkillCount() const;
    uint16_t getActiveSkillCountInCategory(uint16_t category_id) const;
    const Skill* getSkillByActiveIndex(uint16_t index) const;
    const Skill* getSkillInCategoryByActiveIndex(uint16_t category_id, uint16_t index) const;
    uint16_t findSkillByName(uint16_t category_id, const char* name) const;
    bool setSkillProgress(uint16_t skill_id, uint16_t level, uint32_t current_xp, uint32_t lifetime_xp);

    uint16_t addXPToSkill(uint16_t skill_id, uint32_t xp_amount);
    uint16_t addXPFromTask(uint16_t skill_id, uint32_t player_xp_awarded, uint32_t& skill_xp_awarded,
                           float skill_xp_multiplier = DEFAULT_SKILL_XP_MULTIPLIER);

    uint32_t getXPForNextSkillLevel(uint16_t skill_id) const;
    uint8_t getSkillXPProgress(uint16_t skill_id) const;

    void reset();
    void printSkill(uint16_t skill_id) const;
    void printCategory(uint16_t category_id) const;
    void printAll() const;

    // Chart helpers for future UI web-chart/radar view.
    uint16_t exportCategorySnapshot(char names[][MAX_SKILL_CATEGORY_NAME_LEN], float levels[], uint16_t max_items) const;
    uint16_t exportSkillsInCategorySnapshot(uint16_t category_id, char names[][MAX_SKILL_NAME_LEN], float levels[], uint16_t max_items) const;
};

#endif
