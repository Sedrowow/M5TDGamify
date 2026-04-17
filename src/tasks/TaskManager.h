#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include "Task.h"
#include <vector>

#define MAX_TASKS 128

class TaskManager {
private:
    Task tasks[MAX_TASKS];
    uint16_t task_count;
    uint16_t next_task_id;

public:
    TaskManager();

    /**
     * Add a new task
     * @return task ID if successful, 0 if max tasks reached
     */
    uint16_t addTask(const char* name, const char* details, uint8_t difficulty,
                     uint8_t urgency, uint8_t fear, uint8_t repetition,
                     uint32_t duration_seconds, uint32_t due_date, uint32_t reward_money,
                     uint16_t linked_skill_category_id = 0, uint16_t linked_skill_id = 0,
                     uint16_t reward_item_id = 0, uint8_t reward_item_quantity = 0);

    bool updateTask(uint16_t task_id, const Task& updated_task);

    /**
     * Remove a task by ID
     * @return true if task was removed, false if not found
     */
    bool removeTask(uint16_t task_id);

    /**
     * Mark task as completed
     * @return XP gained, 0 if task not found
     */
    uint32_t completeTask(uint16_t task_id);

    /**
     * Mark task as failed
     * @return true if state changed, false otherwise
     */
    bool failTask(uint16_t task_id);

    /**
     * Archive a terminal task so it no longer appears in normal UI
     */
    bool archiveTask(uint16_t task_id);

    /**
     * Get task by ID
     * @return pointer to task if found, nullptr otherwise
     */
    Task* getTask(uint16_t task_id);

    /**
     * Get all tasks (both completed and pending)
     * @return raw array of tasks and count
     */
    Task* getAllTasks(uint16_t& count) const;

    Task* getVisibleTaskByIndex(uint16_t index) const;
    uint16_t getVisibleTaskCount() const;

    /**
     * Get only active tasks
     * @return raw array of active tasks and count
     */
    Task* getActiveTasks(uint16_t& count) const;

    /**
     * Get task count
     */
    uint16_t getTaskCount() const { return task_count; }

    /**
     * Get total XP from completed tasks
     */
    uint32_t getTotalCompletedXP() const;

    /**
     * Clear all tasks
     */
    void clearAll();

    /**
     * Print task info for debugging
     */
    void printTask(uint16_t task_id) const;
};

#endif
