#ifndef SAVELOADSYSTEM_H
#define SAVELOADSYSTEM_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <SPIFFS.h>
#include "../levelsystem/LevelSystem.h"
#include "../health/HealthSystem.h"
#include "../skills/SkillSystem.h"
#include "../tasks/TaskManager.h"
#include "../shop/ShopSystem.h"
#include "../settings/SettingsSystem.h"
#include "../alarms/AlarmSystem.h"

enum StorageBackend {
    STORAGE_BACKEND_SPIFFS = 0,
    STORAGE_BACKEND_SD = 1
};

struct ProfileInfo {
    String name;
    String description;
    uint32_t tasks_completed = 0;
    uint32_t tasks_failed = 0;
    uint32_t tasks_created = 0;
    int32_t money_gained = 0;
    int32_t money_spent = 0;
};

class SaveLoadSystem {
private:
    String last_error;
    bool spiffs_ready;
    bool sd_ready;
    StorageBackend backend;
    fs::FS* active_fs;

    String active_profile_name;
    String active_profile_description;

    static const char* BASE_DIR;
    static const char* PROFILES_DIR;
    static const char* GLOBAL_FILE;
    static const char* INDEX_FILE;
    static const char* SHARED_SHOP_ITEMS_FILE;

    bool ensureDirRecursive(const char* dir_path);
    bool writeTextFile(const String& path, const String& content);
    bool readTextFile(const String& path, String& content) const;

    String sanitizeProfileName(const String& name) const;
    String profileDir(const String& profile_name) const;
    String profileSkillsPath(const String& profile_name) const;
    String profileStatePath(const String& profile_name) const;
    String profileTasksPath(const String& profile_name) const;
    String profileShopStatePath(const String& profile_name) const;
    String sharedShopCatalogPath() const;

    bool saveGlobalSettings();
    bool loadGlobalSettings();

    bool loadProfileIndex(ProfileInfo profiles[], uint8_t max_count, uint8_t& out_count) const;
    bool saveProfileIndex(const ProfileInfo profiles[], uint8_t count);
    bool ensureProfileInIndex(const String& name, const String& description);

    bool parseKeyValueLine(const String& line, String& key, String& value) const;
    bool loadProfileStats(const String& profile_name, ProfileInfo& profile) const;

public:
    SaveLoadSystem();

    bool begin();

    bool isStorageReady() const { return active_fs != nullptr; }
    bool isSDReady() const { return sd_ready; }
    StorageBackend getBackend() const { return backend; }
    const char* getBackendName() const;

    bool setActiveProfile(const String& profile_name, const String& description);
    String getActiveProfileName() const { return active_profile_name; }
    String getActiveProfileDescription() const { return active_profile_description; }
    bool deleteProfile(const String& profile_name, const String& fallback_name = "",
                       const String& fallback_description = "");

    bool listProfiles(ProfileInfo profiles[], uint8_t max_count, uint8_t& out_count) const;

    bool saveCurrentProfileState(const LevelSystem& level_system, const HealthSystem& health_system,
                                 int32_t money, int8_t timezone_offset,
                                 uint32_t tasks_completed, uint32_t tasks_failed,
                                 uint32_t tasks_created, int32_t money_gained,
                                 int32_t money_spent);
    bool loadCurrentProfileState(LevelSystem& level_system, HealthSystem& health_system,
                                 int32_t& money, int8_t& timezone_offset,
                                 uint32_t& tasks_completed, uint32_t& tasks_failed,
                                 uint32_t& tasks_created, int32_t& money_gained,
                                 int32_t& money_spent);

    bool saveSharedShopItems(const String& shop_items_text);
    bool loadSharedShopItems(String& shop_items_text) const;

    bool saveTasks(const TaskManager& task_manager);
    bool loadTasks(TaskManager& task_manager);

    bool saveSharedShopCatalog(const ShopSystem& shop_system);
    bool loadSharedShopCatalog(ShopSystem& shop_system);
    bool saveProfileShopState(const ShopSystem& shop_system);
    bool loadProfileShopState(ShopSystem& shop_system);

    bool saveSkills(const SkillSystem& skill_system);
    bool loadSkills(SkillSystem& skill_system);

    bool hasSavedSkills() const;
    bool clearSkillsSave();

    bool saveGlobalConfig(const GlobalSettings& cfg);
    bool loadGlobalConfig(GlobalSettings& cfg);

    bool saveAlarmsAndTimers(const AlarmEntry alarms[], uint8_t alarm_count,
                             const TimerEntry timers[], uint8_t timer_count);
    bool loadAlarmsAndTimers(AlarmEntry alarms[], uint8_t& alarm_count, uint8_t max_alarms,
                             TimerEntry timers[], uint8_t& timer_count, uint8_t max_timers);

    String getLastError() const { return last_error; }
};

#endif
