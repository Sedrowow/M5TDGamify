#include "TaskManager.h"
#include <cstring>

TaskManager::TaskManager() : task_count(0), next_task_id(1) {}

uint16_t TaskManager::addTask(const char* name, const char* details, uint8_t difficulty,
                               uint8_t urgency, uint8_t fear, uint8_t repetition,
                               uint32_t duration_seconds, uint32_t due_date, uint32_t reward_money,
                               uint16_t linked_skill_category_id, uint16_t linked_skill_id,
                               uint16_t reward_item_id, uint8_t reward_item_quantity) {
    if (task_count >= MAX_TASKS) {
        return 0;  // Max tasks reached
    }

    Task& new_task = tasks[task_count];
    strncpy(new_task.name, name, MAX_TASK_NAME_LEN - 1);
    new_task.name[MAX_TASK_NAME_LEN - 1] = '\0';
    
    if (details) {
        strncpy(new_task.details, details, MAX_TASK_DETAILS_LEN - 1);
        new_task.details[MAX_TASK_DETAILS_LEN - 1] = '\0';
    }

    new_task.difficulty = difficulty > 100 ? 100 : difficulty;
    new_task.urgency = urgency > 100 ? 100 : urgency;
    new_task.fear = fear > 100 ? 100 : fear;
    new_task.repetition = repetition > 0 ? repetition : 1;
    new_task.duration_seconds = duration_seconds;
    new_task.due_date = due_date;
    new_task.reward_money = reward_money;
    new_task.reward_item_id = reward_item_id;
    new_task.reward_item_quantity = reward_item_quantity;
    new_task.clearLinkedSkills();
    if (linked_skill_id > 0) {
        new_task.addLinkedSkill(linked_skill_category_id, linked_skill_id);
    }
    new_task.completed = false;
    new_task.creation_time = millis() / 1000;  // Unix-like timestamp
    new_task.id = next_task_id;

    task_count++;
    next_task_id++;
    
    return new_task.id;
}

bool TaskManager::updateTask(uint16_t task_id, const Task& updated_task) {
    Task* task = getTask(task_id);
    if (!task) {
        return false;
    }

    uint16_t preserved_id = task->id;
    uint32_t preserved_creation = task->creation_time;
    bool preserved_completed = task->completed;
    bool preserved_failed = task->failed;
    bool preserved_archived = task->archived;
    uint32_t preserved_completion = task->completion_time;

    *task = updated_task;
    task->id = preserved_id;
    task->creation_time = preserved_creation;
    task->completed = preserved_completed;
    task->failed = preserved_failed;
    task->archived = preserved_archived;
    task->completion_time = preserved_completion;

    return true;
}

bool TaskManager::removeTask(uint16_t task_id) {
    for (uint16_t i = 0; i < task_count; i++) {
        if (tasks[i].id == task_id) {
            // Shift remaining tasks
            for (uint16_t j = i; j < task_count - 1; j++) {
                tasks[j] = tasks[j + 1];
            }
            task_count--;
            return true;
        }
    }
    return false;
}

uint32_t TaskManager::completeTask(uint16_t task_id) {
    Task* task = getTask(task_id);
    if (!task || task->isTerminal()) {
        return 0;
    }
    
    task->completed = true;
    task->failed = false;
    task->completion_time = millis() / 1000;
    
    return task->calculateXP();
}

bool TaskManager::failTask(uint16_t task_id) {
    Task* task = getTask(task_id);
    if (!task || task->isTerminal()) {
        return false;
    }

    task->failed = true;
    task->completed = false;
    task->completion_time = millis() / 1000;
    return true;
}

bool TaskManager::archiveTask(uint16_t task_id) {
    Task* task = getTask(task_id);
    if (!task || !task->isTerminal() || task->archived) {
        return false;
    }

    task->archived = true;
    return true;
}

Task* TaskManager::getTask(uint16_t task_id) {
    for (uint16_t i = 0; i < task_count; i++) {
        if (tasks[i].id == task_id) {
            return &tasks[i];
        }
    }
    return nullptr;
}

Task* TaskManager::getAllTasks(uint16_t& count) const {
    count = task_count;
    return (Task*)tasks;
}

Task* TaskManager::getVisibleTaskByIndex(uint16_t index) const {
    uint16_t visible_index = 0;
    for (uint16_t i = 0; i < task_count; i++) {
        if (!tasks[i].isVisible()) continue;
        if (visible_index == index) return (Task*)&tasks[i];
        visible_index++;
    }
    return nullptr;
}

uint16_t TaskManager::getVisibleTaskCount() const {
    uint16_t visible_count = 0;
    for (uint16_t i = 0; i < task_count; i++) {
        if (tasks[i].isVisible()) visible_count++;
    }
    return visible_count;
}

Task* TaskManager::getActiveTasks(uint16_t& count) const {
    // Note: This returns pointer to original array, caller should filter by completed flag
    count = task_count;
    return (Task*)tasks;
}

uint32_t TaskManager::getTotalCompletedXP() const {
    uint32_t total_xp = 0;
    for (uint16_t i = 0; i < task_count; i++) {
        if (tasks[i].completed) {
            total_xp += tasks[i].calculateXP();
        }
    }
    return total_xp;
}

void TaskManager::clearAll() {
    task_count = 0;
    next_task_id = 1;
}

void TaskManager::fixNextId() {
    uint16_t max_id = 0;
    for (uint16_t i = 0; i < task_count; i++) {
        if (tasks[i].id > max_id) max_id = tasks[i].id;
    }
    next_task_id = max_id + 1;
}

void TaskManager::printTask(uint16_t task_id) const {
    const Task* task = nullptr;
    for (uint16_t i = 0; i < task_count; i++) {
        if (tasks[i].id == task_id) {
            task = &tasks[i];
            break;
        }
    }
    
    if (!task) {
        Serial.println("[TaskManager] Task not found");
        return;
    }

    Serial.printf("[Task #%d] %s\n", task->id, task->name);
    Serial.printf("  Details: %s\n", task->details);
    Serial.printf("  Difficulty: %d%%, Urgency: %d%%, Fear: %d\n", 
                  task->difficulty, task->urgency, task->fear);
    Serial.printf("  Repetitions: %d, Duration: %lu sec\n", task->repetition, (unsigned long)task->duration_seconds);
    Serial.printf("  Reward: $%d, XP: %lu\n", task->reward_money, task->calculateXP());
    if (task->linked_skill_count > 0) {
        Serial.printf("  Linked Skills: %d\n", task->linked_skill_count);
        for (uint8_t i = 0; i < task->linked_skill_count; i++) {
            Serial.printf("    - Category #%d, Skill #%d\n",
                          task->linked_skill_category_ids[i], task->linked_skill_ids[i]);
        }
    }
    if (task->reward_item_id > 0) {
        Serial.printf("  Item Reward: Item #%d x%d (money disabled)\n", task->reward_item_id, task->reward_item_quantity);
    }
    Serial.printf("  Completed: %s\n", task->completed ? "Yes" : "No");
}
