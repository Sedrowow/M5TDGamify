# Fiveite - Gamified Task Manager for M5Cardputer

Fiveite is an on-device task manager with RPG progression, configurable health, skills, profiles, a shop and inventory, alarms, timers, and Wi-Fi time synchronization. It runs on the M5Cardputer using the Arduino framework and PlatformIO.

## Current Features

- **Tasks:** Create, complete, fail, archive, and edit task properties, descriptions, due dates, durations, rewards, and linked skills. Up to 128 task records are stored per profile.
- **Progression:** Player levels range from 1 to 999. Task XP accounts for difficulty, urgency, fear, repetition, duration, due dates, and late completion. Health applies an XP multiplier.
- **Health:** Health can follow a daily time schedule or be set manually as a percentage or exact point value. Optional level-based maximum-health growth supports fixed HP or percentage growth. Shop items can increase or decrease maximum HP.
- **Skills:** Create and edit categories and skills, save descriptions, view detailed descriptions, and track skill XP and levels. Tasks can award XP to linked skills.
- **Shop and inventory:** Buy items, craft recipes, use items, and edit the catalog. Effects include XP, random XP, countdowns, and maximum HP changes.
- **Profiles and persistence:** Profile-specific tasks, skills, levels, health, money, and shop state are saved to SD when available. SPIFFS provides local storage when SD is unavailable; multi-profile management requires SD.
- **Alarms and timers:** Configure alarms and countdown timers with on-device notifications.
- **Time and settings:** Wi-Fi/NTP sync, manual time, timezone and date format settings, health schedule, audio/visual feedback, and skill XP distribution.
- **Help and battery status:** In-device help includes controls, battery information, changelog, and credits. Battery percentage is smoothed; the Cardputer may report charging state as unavailable because its battery sensing is ADC-based.

## Build and Upload

Prerequisites: PlatformIO, an M5Cardputer, and a USB cable.

```sh
pio run -e m5stack-cardputer
pio run -e m5stack-cardputer -t upload
pio run -e m5stack-cardputer -t monitor
```

The serial monitor uses 115200 baud.

## Controls

`TAB` opens the screen selector. `;` and `.` move up/down, `,` and `/` move left/right, `ENTER` selects, and backtick goes back. `H` opens contextual help. The bottom status bar scrolls screen-specific hints.

| Screen | Main controls |
|---|---|
| Dashboard | Up/down selects a task, `ENTER` completes it, `F` fails it, `L` opens manual health input. |
| Tasks | `N` creates a task, `E` toggles editing, `ENTER` completes, `D` deletes or archives, `T` edits its description. In edit mode use left/right to select a field, up/down to change it, and `SPACE` for detailed field editors. |
| Skills | Left/right selects categories, up/down selects skills, `ENTER` opens skill details, `SPACE` cycles charts, `E` enables editing, then `SPACE` opens the edit menu. |
| Shop | Up/down selects an entry, left/right changes item/recipe focus, `ENTER` buys or crafts, `E` and `SPACE` open editing, `Z` enables setup mode. |
| Inventory | Up/down selects an item, `ENTER` uses it, `SPACE` opens its description. |
| Profiles | Up/down selects a profile, `ENTER` switches, `N` creates, `G` edits its description, `DEL` deletes. Creating and managing multiple profiles requires SD. |
| Alarms | Left switches alarm/timer panes. `A` adds an alarm, `T` adds a timer in the timer pane, `SPACE` opens setup, `ENTER` toggles, and `DEL` removes. |
| Help | Left/right switches Overview, Controls, Battery Info, Changelog, and Credits pages. |

### Health Controls

Press `L` to open manual health input. In percentage mode, `1`-`9` set 10%-90%, `0` sets 100%, and `-`/`+` adjust by one percentage point. `SPACE` switches between percentage and numerical HP. In numerical mode, `,`/`/` decrease/increase the step, `;` increases HP by that step, and `.` decreases it. `ENTER` applies the value; numerical mode stays open until confirmed.

In **Settings > Health**, `P` switches the HUD display between percentage and points, `G` toggles level-based maximum HP growth, `M` switches growth between fixed points and percent, `+`/`-` change its amount, and `J` opens the wake/sleep time editor. Time-based health calculations remain percentage-based.

## Task XP and Player Levels

Task base XP is calculated from its properties: 100 base XP, plus difficulty × 12, urgency × 12, fear × 9, repetition bonuses, duration bonuses, and a due-date bonus for near deadlines. Completing after the due date reduces XP by 15%. Health then applies the player XP multiplier: 1.00x at 75%-100%, 0.75x at 50%-74%, 0.50x at 25%-49%, and 0.25x below 25%.

Player XP requirements start at 1,500 XP for level 2 and grow by a factor of 1.2 per level. Skill progression has its own XP curve and configurable task XP ratio and split behavior.

## Storage

The firmware uses SD storage when available and falls back to SPIFFS for local profile data. Global device settings are stored separately from profile data. Shop catalog data is shared while inventory and shop runtime state are profile-specific. The Save/Load screen provides manual profile save/load and shared shop catalog operations.

## Source Layout

```text
src/
  alarms/       Alarm and timer data
  health/       Health modes, HP points, and XP multipliers
  levelsystem/  Player XP and level progression
  persistence/  Profile, settings, task, skill, shop, and alarm storage
  settings/     Wi-Fi and global device settings
  shop/         Catalog, crafting, effects, and inventory
  skills/       Skill categories and progression
  tasks/        Task model and task manager
  time/         RTC, timezone, and NTP synchronization
  main.cpp      Firmware setup, input handling, and UI
```

## Credits

- Contributors: Sedrowow and GitHub Copilot
- Hardware/UI libraries: M5Cardputer, M5Unified, and M5GFX
- Runtime and build tooling: Arduino-ESP32 and PlatformIO
