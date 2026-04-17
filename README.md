# Fiveite - Gamification Todo List for M5Cardputer

A complete gamification system for the M5Cardputer with RPG-style progression, task management, health mechanics, and reward systems.

## Project Status

### ✅ Completed Systems
1. **Task System** - Full task management with comprehensive properties
   - Task properties: name, details, difficulty, urgency, fear, repetition, duration, due date, reward
   - XP calculation based on difficulty, urgency, and fear (max 1.4M XP per task)
   - Task creation, removal, completion tracking
   - TaskManager for managing up to 128 tasks

2. **Level System** - RPG-style progression
   - Exponential XP scaling (base 1000 XP, 1.1x multiplier per level)
   - Levels 1-999 support
   - XP tracking (current, lifetime)
   - Level progression, XP progress tracking
   - Automatic level up calculation

3. **Health System** - Dynamic health mechanics affecting XP gain
   - **Manual Mode**: Player directly sets health (0-100%)
   - **Time-Based Mode**: Health cycles throughout the day (e.g., 100% at 8 AM → 0% at 10 PM)
   - XP multipliers based on health quarters:
     - Quarter 1 (100-75%): 1.00x multiplier
     - Quarter 2 (74-50%):  0.75x multiplier
     - Quarter 3 (49-25%):  0.50x multiplier
     - Quarter 4 (24-0%):   0.25x multiplier

4. **Time Sync System** - WiFi and timezone management
   - WiFi-based NTP time synchronization
   - Manual time setting
   - UTC timezone offset support (-12 to +14)
   - Timezone helper with common regions (PST, EST, GMT, CET, IST, JST, etc.)
   - Time display formatting

5. **Skill System** - Player-defined categories and skills
   - Custom skill categories can be created by the player
   - Custom skills can be created inside each category
   - Tasks can be linked to a selected skill
   - Skills level up slower than player level (higher XP requirement + lower XP gain)
   - Chart-ready snapshot APIs for future web-chart/radar UI

6. **Advanced UI/Navigation** - Multi-screen keyboard UI on device
   - Screen navigation with Dashboard, Skills, and Save/Load screens
   - Skill management UI for adding/removing categories and skills
   - On-screen status feedback for actions and errors

7. **Save/Load System** - SPIFFS persistence for skills
   - Save current skill/category data to SPIFFS
   - Load saved skill/category data at startup or on command
   - Clear save file option from Save/Load screen

### 🔄 In Progress
- Shop System design

### 📋 Todo Systems
- Shop System
- Money System (basic reward tracking ready)
- Full task editor UI (create/edit/delete tasks on device)

## Building the Project

### Prerequisites
- PlatformIO VS Code Extension
- M5Cardputer and USB Cable
- Driver for M5Cardputer (CH340 or similar)

### Build & Upload
```bash
# Build for M5Cardputer
pio run -e m5stack-cardputer

# Upload to device
pio run -e m5stack-cardputer -t upload

# Monitor serial output
pio run -e m5stack-cardputer -t monitor
```

## Project Structure

```
Fiveite/
├── platformio.ini                  # PlatformIO configuration
├── src/
│   ├── main.cpp                    # Main entry point with demo
│   ├── tasks/
│   │   ├── Task.h                  # Task data structure
│   │   └── TaskManager.cpp         # Task management
│   ├── levelsystem/
│   │   ├── LevelSystem.h           # Level system header
│   │   └── LevelSystem.cpp         # Level system implementation
│   ├── health/
│   │   ├── HealthSystem.h          # Health system header
│   │   └── HealthSystem.cpp        # Health system implementation
│   ├── skills/
│   │   ├── SkillSystem.h           # Skill categories + skills + progression
│   │   └── SkillSystem.cpp         # Skill progression implementation
│   ├── persistence/
│   │   ├── SaveLoadSystem.h        # SPIFFS save/load interface
│   │   └── SaveLoadSystem.cpp      # SPIFFS serialization logic
│   └── time/
│       ├── TimeSync.h              # Time sync header
│       └── TimeSync.cpp            # Time sync implementation
├── lib/                            # External libraries
└── README.md                       # This file
```

## Task System Details

### Task Properties
- **name**: Required task name (max 64 chars)
- **details**: Optional task description (max 256 chars)
- **difficulty**: 0-100% - How hard is the task?
- **urgency**: 0-100% - How urgent is it?
- **fear**: 0-100 - How scary/challenging is it emotionally?
- **repetition**: Number of times to repeat (default: 1)
- **duration**: Minutes to complete (0 = no duration)
- **due_date**: Unix timestamp for due date (0 = no deadline)
- **reward**: Dollar amount reward upon completion
- **completed**: Track completion status

### XP Calculation Formula
```
Total XP = (Difficulty × 4000) + (Urgency × 4000) + (Fear × 3400)
Maximum XP = 1,340,000 per task (when all are 100)
```

## Level System Details

### XP Scaling Formula
```
XP for level N = 1000 × (1.1 ^ (N - 2))

Example progression:
Level 1→2:   1,000 XP
Level 2→3:   1,100 XP
Level 3→4:   1,210 XP
Level 10→11: 2,593 XP
Level 50→51: 117,390 XP
```

### Max Levels: 999

## Keyboard Commands (Legacy Demo)

- **1** - Complete first task
- **2** - Complete second task
- **s** - Show all tasks
- **l** - Show level info
- **r** - Reset demo

## Health System Details

### Health Modes

**Manual Mode**
- Player directly sets health percentage (0-100%)
- Useful for tracking subjective wellness
- Example: Set to 80% if feeling good, 40% if tired

**Time-Based Mode**
- Health automatically cycles based on time of day
- Configurable start and end times (in 24-hour format)
- Health interpolates linearly between start (100%) and end (0%)
- Example: 8 AM (100%) → 10 PM (0%)
- Before start time: 100% health
- After end time: 0% health

### XP Multipliers by Health Quarter

| Health Range | Quarter | XP Multiplier | Effect |
|--------------|---------|---------------|--------|
| 100-75%      | 1       | 1.00x         | Full XP gain |
| 74-50%       | 2       | 0.75x         | 75% of XP |
| 49-25%       | 3       | 0.50x         | 50% of XP |
| 24-0%        | 4       | 0.25x         | 25% of XP |

**Example**: A 1,000 XP task completed at different health levels:
- At 100% health: 1,000 XP gained
- At 60% health: 750 XP gained (Quarter 2)
- At 40% health: 500 XP gained (Quarter 3)
- At 10% health: 250 XP gained (Quarter 4)

## Time Sync System Details

### Features
- **WiFi NTP Synchronization**: Automatically sync time from internet
- **Manual Time Setting**: Set time manually if WiFi unavailable
- **Timezone Support**: UTC offset from -12 to +14 hours
- **Region Presets**: Built-in support for common timezones
   - PST (UTC-8), MST (UTC-7), CST (UTC-6), EST (UTC-5)
   - GMT (UTC+0), CET (UTC+1), EET (UTC+2), IST (UTC+5)
   - JST (UTC+9), AEST (UTC+10), NZST (UTC+12)

### Time Format
- Internal: Unix timestamp
- Display: DD/MM/YYYY HH:MM:SS
- 24-hour clock

### Timezone Setting Examples
```cpp
time_sync.setTimezoneOffset(1);    // UTC+1 (Central European Time)
time_sync.setTimezoneOffset(-5);   // UTC-5 (Eastern Standard Time)
time_sync.setTimezoneOffset(9);    // UTC+9 (Japan Standard Time)
```

## Keyboard Commands (Demo)

- **q / e** - Switch between Dashboard, Skills, Save/Load screens
- **Dashboard**:
   - **1 / 2** - Complete demo task 1 or 2
   - **m** - Manual health mode
   - **b** - Time-based health mode
   - **r** - Reset demo tasks
- **Skills Screen**:
   - **[ / ]** - Select previous/next category
   - **{ / }** - Select previous/next skill in current category
   - **a** - Add category (text input)
   - **k** - Add skill in selected category (text input)
   - **d** - Delete selected category
   - **x** - Delete selected skill
   - **p** - Save skills to SPIFFS
   - **l** - Load skills from SPIFFS
- **Save/Load Screen**:
   - **s** - Save skills/categories
   - **l** - Load skills/categories
   - **c** - Clear saved file
- **Text Input Mode**:
   - **Enter** - Confirm
   - **Backspace** - Delete character
   - **Esc** - Cancel

## Usage Example

### Setup with Time Sync
```cpp
// Initialize time sync with WiFi
time_sync.setTimezoneOffset(1);  // UTC+1
time_sync.syncWithWiFi("MY_SSID", "MY_PASSWORD");

// Initialize health system to time-based mode
health_system.setTimeBasedMode();
health_system.setTimeBasedCycle(8, 22);  // 8 AM = 100%, 10 PM = 0%
health_system.setCurrentTime(time_sync.getCurrentTime());
```

### Completing a Task with Health Multiplier
```cpp
// Complete a task
uint32_t base_xp = task_manager.completeTask(task_id);

// Apply health multiplier
float multiplier = health_system.getXPMultiplier();
uint16_t levels_gained = level_system.addXPWithMultiplier(base_xp, multiplier);
```

## Next Steps

1. Implement Shop System
2. Add on-device Task editor screens (create/edit/delete tasks)
3. Save/load for tasks, level, health, and time settings
4. Add audio feedback (beeps/buzzes)
5. Add final chart screen for skill/category progression comparison
6. Create final .bin file

## Memory Requirements

- Task array: ~128 tasks × ~400 bytes = ~51 KB
- Level system: ~16 bytes
- Health system: ~32 bytes
- Time system: ~16 bytes
- Display buffer: Variable

M5Cardputer has 8MB PSRAM, so plenty of room for expansion!

## Serial Debugging

Open the serial monitor in PlatformIO to see:
- Task creation logs
- XP gains with health multipliers
- Level ups
- Health state changes
- Time synchronization status
- System debug info

Set baud rate to **115200**
