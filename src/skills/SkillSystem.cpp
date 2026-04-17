#include "SkillSystem.h"
#include <cstring>

SkillSystem::SkillSystem()
    : category_count(0), skill_count(0), next_category_id(1), next_skill_id(1) {}

uint32_t SkillSystem::calculateXPForSkillLevel(uint16_t level) const {
    if (level <= 1) {
        return SKILL_BASE_XP_REQUIREMENT;
    }

    float exponent = (float)(level - 1);
    uint32_t required_xp = (uint32_t)(SKILL_BASE_XP_REQUIREMENT * pow(SKILL_XP_GROWTH, exponent) + 0.5f);
    if (required_xp > 1000000000U) {
        required_xp = 1000000000U;
    }
    return required_xp;
}

uint16_t SkillSystem::addCategory(const char* name, const char* description) {
    if (!name || name[0] == '\0' || category_count >= MAX_SKILL_CATEGORIES) {
        return 0;
    }

    SkillCategory& category = categories[category_count];
    category.id = next_category_id;
    strncpy(category.name, name, MAX_SKILL_CATEGORY_NAME_LEN - 1);
    category.name[MAX_SKILL_CATEGORY_NAME_LEN - 1] = '\0';

    if (description) {
        strncpy(category.description, description, MAX_SKILL_CATEGORY_DESC_LEN - 1);
        category.description[MAX_SKILL_CATEGORY_DESC_LEN - 1] = '\0';
    } else {
        category.description[0] = '\0';
    }

    category.creation_time = millis() / 1000;
    category.active = true;

    category_count++;
    next_category_id++;
    return category.id;
}

SkillCategory* SkillSystem::getCategory(uint16_t category_id) {
    for (uint16_t i = 0; i < category_count; i++) {
        if (categories[i].id == category_id && categories[i].active) {
            return &categories[i];
        }
    }
    return nullptr;
}

const SkillCategory* SkillSystem::getCategory(uint16_t category_id) const {
    for (uint16_t i = 0; i < category_count; i++) {
        if (categories[i].id == category_id && categories[i].active) {
            return &categories[i];
        }
    }
    return nullptr;
}

bool SkillSystem::removeCategory(uint16_t category_id) {
    for (uint16_t i = 0; i < category_count; i++) {
        if (categories[i].id == category_id && categories[i].active) {
            // Also remove all skills in this category.
            for (uint16_t j = 0; j < skill_count; j++) {
                if (skills[j].active && skills[j].category_id == category_id) {
                    skills[j].active = false;
                }
            }
            categories[i].active = false;
            return true;
        }
    }
    return false;
}

uint16_t SkillSystem::getActiveCategoryCount() const {
    uint16_t active_count = 0;
    for (uint16_t i = 0; i < category_count; i++) {
        if (categories[i].active) {
            active_count++;
        }
    }
    return active_count;
}

const SkillCategory* SkillSystem::getCategoryByActiveIndex(uint16_t index) const {
    uint16_t current_index = 0;
    for (uint16_t i = 0; i < category_count; i++) {
        if (!categories[i].active) {
            continue;
        }
        if (current_index == index) {
            return &categories[i];
        }
        current_index++;
    }
    return nullptr;
}

uint16_t SkillSystem::findCategoryByName(const char* name) const {
    if (!name || name[0] == '\0') {
        return 0;
    }

    for (uint16_t i = 0; i < category_count; i++) {
        if (categories[i].active && strcmp(categories[i].name, name) == 0) {
            return categories[i].id;
        }
    }

    return 0;
}

uint16_t SkillSystem::addSkill(uint16_t category_id, const char* name, const char* details) {
    if (!name || name[0] == '\0' || skill_count >= MAX_SKILLS) {
        return 0;
    }

    if (!getCategory(category_id)) {
        return 0;
    }

    Skill& skill = skills[skill_count];
    skill.id = next_skill_id;
    skill.category_id = category_id;
    strncpy(skill.name, name, MAX_SKILL_NAME_LEN - 1);
    skill.name[MAX_SKILL_NAME_LEN - 1] = '\0';

    if (details) {
        strncpy(skill.details, details, MAX_SKILL_DETAILS_LEN - 1);
        skill.details[MAX_SKILL_DETAILS_LEN - 1] = '\0';
    } else {
        skill.details[0] = '\0';
    }

    skill.level = MIN_SKILL_LEVEL;
    skill.current_xp = 0;
    skill.lifetime_xp = 0;
    skill.creation_time = millis() / 1000;
    skill.active = true;

    skill_count++;
    next_skill_id++;
    return skill.id;
}

Skill* SkillSystem::getSkill(uint16_t skill_id) {
    for (uint16_t i = 0; i < skill_count; i++) {
        if (skills[i].id == skill_id && skills[i].active) {
            return &skills[i];
        }
    }
    return nullptr;
}

const Skill* SkillSystem::getSkill(uint16_t skill_id) const {
    for (uint16_t i = 0; i < skill_count; i++) {
        if (skills[i].id == skill_id && skills[i].active) {
            return &skills[i];
        }
    }
    return nullptr;
}

bool SkillSystem::removeSkill(uint16_t skill_id) {
    Skill* skill = getSkill(skill_id);
    if (!skill) {
        return false;
    }

    skill->active = false;
    return true;
}

uint16_t SkillSystem::getActiveSkillCount() const {
    uint16_t active_count = 0;
    for (uint16_t i = 0; i < skill_count; i++) {
        if (skills[i].active) {
            active_count++;
        }
    }
    return active_count;
}

uint16_t SkillSystem::getActiveSkillCountInCategory(uint16_t category_id) const {
    uint16_t active_count = 0;
    for (uint16_t i = 0; i < skill_count; i++) {
        if (skills[i].active && skills[i].category_id == category_id) {
            active_count++;
        }
    }
    return active_count;
}

const Skill* SkillSystem::getSkillByActiveIndex(uint16_t index) const {
    uint16_t current_index = 0;
    for (uint16_t i = 0; i < skill_count; i++) {
        if (!skills[i].active) {
            continue;
        }
        if (current_index == index) {
            return &skills[i];
        }
        current_index++;
    }
    return nullptr;
}

const Skill* SkillSystem::getSkillInCategoryByActiveIndex(uint16_t category_id, uint16_t index) const {
    uint16_t current_index = 0;
    for (uint16_t i = 0; i < skill_count; i++) {
        if (!skills[i].active || skills[i].category_id != category_id) {
            continue;
        }
        if (current_index == index) {
            return &skills[i];
        }
        current_index++;
    }
    return nullptr;
}

uint16_t SkillSystem::findSkillByName(uint16_t category_id, const char* name) const {
    if (!name || name[0] == '\0') {
        return 0;
    }

    for (uint16_t i = 0; i < skill_count; i++) {
        if (skills[i].active && skills[i].category_id == category_id && strcmp(skills[i].name, name) == 0) {
            return skills[i].id;
        }
    }

    return 0;
}

bool SkillSystem::setSkillProgress(uint16_t skill_id, uint16_t level, uint32_t current_xp, uint32_t lifetime_xp) {
    Skill* skill = getSkill(skill_id);
    if (!skill) {
        return false;
    }

    if (level < MIN_SKILL_LEVEL) {
        level = MIN_SKILL_LEVEL;
    }
    if (level > MAX_SKILL_LEVEL) {
        level = MAX_SKILL_LEVEL;
    }

    skill->level = level;
    skill->lifetime_xp = lifetime_xp;

    uint32_t max_xp_for_level = calculateXPForSkillLevel(level);
    if (current_xp >= max_xp_for_level && max_xp_for_level > 0) {
        current_xp = max_xp_for_level - 1;
    }
    skill->current_xp = current_xp;

    return true;
}

uint16_t SkillSystem::addXPToSkill(uint16_t skill_id, uint32_t xp_amount) {
    Skill* skill = getSkill(skill_id);
    if (!skill || xp_amount == 0) {
        return 0;
    }

    skill->current_xp += xp_amount;
    skill->lifetime_xp += xp_amount;

    uint16_t levels_gained = 0;
    while (skill->level < MAX_SKILL_LEVEL && skill->current_xp >= calculateXPForSkillLevel(skill->level)) {
        skill->current_xp -= calculateXPForSkillLevel(skill->level);
        skill->level++;
        levels_gained++;
    }

    return levels_gained;
}

uint16_t SkillSystem::addXPFromTask(uint16_t skill_id, uint32_t player_xp_awarded, uint32_t& skill_xp_awarded,
                                    float skill_xp_multiplier) {
    if (skill_xp_multiplier < 0.01f) {
        skill_xp_multiplier = 0.01f;
    }
    if (skill_xp_multiplier > 1.0f) {
        skill_xp_multiplier = 1.0f;
    }

    skill_xp_awarded = (uint32_t)(player_xp_awarded * skill_xp_multiplier + 0.5f);
    return addXPToSkill(skill_id, skill_xp_awarded);
}

uint32_t SkillSystem::getXPForNextSkillLevel(uint16_t skill_id) const {
    const Skill* skill = getSkill(skill_id);
    if (!skill || skill->level >= MAX_SKILL_LEVEL) {
        return 0;
    }
    return calculateXPForSkillLevel(skill->level);
}

uint8_t SkillSystem::getSkillXPProgress(uint16_t skill_id) const {
    const Skill* skill = getSkill(skill_id);
    if (!skill) {
        return 0;
    }

    uint32_t required = calculateXPForSkillLevel(skill->level);
    if (required == 0) {
        return 100;
    }
    return (uint8_t)((skill->current_xp * 100U) / required);
}

void SkillSystem::reset() {
    category_count = 0;
    skill_count = 0;
    next_category_id = 1;
    next_skill_id = 1;
}

void SkillSystem::printSkill(uint16_t skill_id) const {
    const Skill* skill = getSkill(skill_id);
    if (!skill) {
        Serial.println("[SkillSystem] Skill not found");
        return;
    }

    const SkillCategory* category = getCategory(skill->category_id);
    Serial.printf("[Skill #%d] %s\n", skill->id, skill->name);
    Serial.printf("  Category: %s\n", category ? category->name : "Unknown");
    Serial.printf("  Level: %d\n", skill->level);
    Serial.printf("  XP: %lu / %lu (%d%%)\n",
                  skill->current_xp,
                  calculateXPForSkillLevel(skill->level),
                  getSkillXPProgress(skill->id));
    Serial.printf("  Lifetime XP: %lu\n", skill->lifetime_xp);
}

void SkillSystem::printCategory(uint16_t category_id) const {
    const SkillCategory* category = getCategory(category_id);
    if (!category) {
        Serial.println("[SkillSystem] Category not found");
        return;
    }

    Serial.printf("[Category #%d] %s\n", category->id, category->name);
    Serial.printf("  %s\n", category->description);
    Serial.println("  Skills:");
    for (uint16_t i = 0; i < skill_count; i++) {
        if (skills[i].active && skills[i].category_id == category_id) {
            Serial.printf("    - #%d %s (Lv%d %d%%)\n",
                          skills[i].id,
                          skills[i].name,
                          skills[i].level,
                          getSkillXPProgress(skills[i].id));
        }
    }
}

void SkillSystem::printAll() const {
    Serial.println("=== Skill System ===");
    for (uint16_t i = 0; i < category_count; i++) {
        if (categories[i].active) {
            printCategory(categories[i].id);
        }
    }
    Serial.println("====================");
}

uint16_t SkillSystem::exportCategorySnapshot(char names[][MAX_SKILL_CATEGORY_NAME_LEN], float levels[], uint16_t max_items) const {
    uint16_t out_count = 0;
    for (uint16_t i = 0; i < category_count && out_count < max_items; i++) {
        if (!categories[i].active) {
            continue;
        }

        uint32_t total_level_scaled = 0;
        uint16_t total_skills = 0;
        for (uint16_t j = 0; j < skill_count; j++) {
            if (skills[j].active && skills[j].category_id == categories[i].id) {
                uint32_t scaled_level = (uint32_t)skills[j].level * 100U + getSkillXPProgress(skills[j].id);
                total_level_scaled += scaled_level;
                total_skills++;
            }
        }

        strncpy(names[out_count], categories[i].name, MAX_SKILL_CATEGORY_NAME_LEN - 1);
        names[out_count][MAX_SKILL_CATEGORY_NAME_LEN - 1] = '\0';

        if (total_skills == 0) {
            levels[out_count] = 0.0f;
        } else {
            levels[out_count] = (float)total_level_scaled / (100.0f * total_skills);
        }

        out_count++;
    }

    return out_count;
}

uint16_t SkillSystem::exportSkillsInCategorySnapshot(uint16_t category_id, char names[][MAX_SKILL_NAME_LEN], float levels[], uint16_t max_items) const {
    uint16_t out_count = 0;
    for (uint16_t i = 0; i < skill_count && out_count < max_items; i++) {
        if (!skills[i].active || skills[i].category_id != category_id) {
            continue;
        }

        strncpy(names[out_count], skills[i].name, MAX_SKILL_NAME_LEN - 1);
        names[out_count][MAX_SKILL_NAME_LEN - 1] = '\0';
        levels[out_count] = (float)skills[i].level + ((float)getSkillXPProgress(skills[i].id) / 100.0f);
        out_count++;
    }

    return out_count;
}
