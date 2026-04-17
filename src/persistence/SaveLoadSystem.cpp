#include "SaveLoadSystem.h"

#include <cstring>
#include <vector>

#define SD_SPI_SCK_PIN 40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN 12

const char* SaveLoadSystem::BASE_DIR = "/FiveITE";
const char* SaveLoadSystem::PROFILES_DIR = "/FiveITE/profiles";
const char* SaveLoadSystem::GLOBAL_FILE = "/FiveITE/global.cfg";
const char* SaveLoadSystem::INDEX_FILE = "/FiveITE/profiles/index.dat";
const char* SaveLoadSystem::SHARED_SHOP_ITEMS_FILE = "/FiveITE/shop_items_shared.dat";

SaveLoadSystem::SaveLoadSystem()
    : last_error(""),
      spiffs_ready(false),
      sd_ready(false),
      backend(STORAGE_BACKEND_SPIFFS),
      active_fs(nullptr),
      active_profile_name("default"),
      active_profile_description("Default profile") {}

bool SaveLoadSystem::ensureDirRecursive(const char* dir_path) {
    if (!active_fs) {
        last_error = "No active filesystem";
        return false;
    }

    String path(dir_path);
    if (!path.startsWith("/")) {
        path = "/" + path;
    }

    if (active_fs->exists(path.c_str())) {
        return true;
    }

    String current = "";
    for (size_t i = 0; i < path.length(); i++) {
        if (path[i] == '/') {
            if (current.length() == 0) {
                current = "/";
                continue;
            }

            if (!active_fs->exists(current.c_str()) && !active_fs->mkdir(current.c_str())) {
                last_error = "mkdir failed: " + current;
                return false;
            }
        } else {
            if (current != "/") {
                current += path[i];
            } else {
                current = "/";
                current += path[i];
            }
        }
    }

    if (!active_fs->exists(path.c_str()) && !active_fs->mkdir(path.c_str())) {
        last_error = "mkdir failed: " + path;
        return false;
    }

    return true;
}

bool SaveLoadSystem::writeTextFile(const String& path, const String& content) {
    if (!active_fs) {
        last_error = "No active filesystem";
        return false;
    }

    int slash = path.lastIndexOf('/');
    if (slash > 0) {
        String dir = path.substring(0, slash);
        if (!ensureDirRecursive(dir.c_str())) {
            return false;
        }
    }

    File file = active_fs->open(path.c_str(), FILE_WRITE);
    if (!file) {
        last_error = "Open for write failed: " + path;
        return false;
    }

    file.print(content);
    file.close();
    return true;
}

bool SaveLoadSystem::readTextFile(const String& path, String& content) const {
    if (!active_fs) {
        return false;
    }

    if (!active_fs->exists(path.c_str())) {
        return false;
    }

    File file = active_fs->open(path.c_str(), FILE_READ);
    if (!file) {
        return false;
    }

    content = "";
    while (file.available()) {
        content += (char)file.read();
    }

    file.close();
    return true;
}

String SaveLoadSystem::sanitizeProfileName(const String& name) const {
    String out = "";
    for (size_t i = 0; i < name.length(); i++) {
        char c = name[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-') {
            out += c;
        } else if (c == ' ') {
            out += '_';
        }
    }

    if (out.length() == 0) {
        out = "profile";
    }

    return out;
}

String SaveLoadSystem::profileDir(const String& profile_name) const {
    if (!sd_ready) {
        return String(PROFILES_DIR) + "/local";
    }
    return String(PROFILES_DIR) + "/" + sanitizeProfileName(profile_name);
}

String SaveLoadSystem::profileSkillsPath(const String& profile_name) const {
    return profileDir(profile_name) + "/skills.dat";
}

String SaveLoadSystem::profileStatePath(const String& profile_name) const {
    return profileDir(profile_name) + "/state.cfg";
}

String SaveLoadSystem::profileTasksPath(const String& profile_name) const {
    return profileDir(profile_name) + "/tasks.dat";
}

String SaveLoadSystem::profileShopStatePath(const String& profile_name) const {
    return profileDir(profile_name) + "/shop_state.dat";
}

String SaveLoadSystem::sharedShopCatalogPath() const {
    return String(BASE_DIR) + "/shop_catalog.dat";
}

bool SaveLoadSystem::parseKeyValueLine(const String& line, String& key, String& value) const {
    int eq = line.indexOf('=');
    if (eq <= 0) {
        return false;
    }

    key = line.substring(0, eq);
    value = line.substring(eq + 1);
    key.trim();
    value.trim();
    return true;
}

bool SaveLoadSystem::saveGlobalSettings() {
    String data = "active_profile=" + active_profile_name + "\n";
    data += "active_profile_description=" + active_profile_description + "\n";
    data += "backend=" + String((backend == STORAGE_BACKEND_SD) ? "sd" : "spiffs") + "\n";

    return writeTextFile(String(GLOBAL_FILE), data);
}

bool SaveLoadSystem::loadGlobalSettings() {
    String data;
    if (!readTextFile(String(GLOBAL_FILE), data)) {
        return false;
    }

    int start = 0;
    while (start < data.length()) {
        int end = data.indexOf('\n', start);
        if (end < 0) {
            end = data.length();
        }

        String line = data.substring(start, end);
        line.trim();

        String key, value;
        if (parseKeyValueLine(line, key, value)) {
            if (key == "active_profile") {
                active_profile_name = value;
            } else if (key == "active_profile_description") {
                active_profile_description = value;
            }
        }

        start = end + 1;
    }

    return true;
}

bool SaveLoadSystem::loadProfileIndex(ProfileInfo profiles[], uint8_t max_count, uint8_t& out_count) const {
    out_count = 0;
    if (!active_fs || !active_fs->exists(INDEX_FILE)) {
        return true;
    }

    File file = active_fs->open(INDEX_FILE, FILE_READ);
    if (!file) {
        return false;
    }

    while (file.available() && out_count < max_count) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) {
            continue;
        }

        int sep = line.indexOf('|');
        if (sep < 0) {
            profiles[out_count].name = line;
            profiles[out_count].description = "";
        } else {
            profiles[out_count].name = line.substring(0, sep);
            profiles[out_count].description = line.substring(sep + 1);
        }

        out_count++;
    }

    file.close();
    return true;
}

bool SaveLoadSystem::saveProfileIndex(const ProfileInfo profiles[], uint8_t count) {
    String content = "";
    for (uint8_t i = 0; i < count; i++) {
        content += profiles[i].name;
        content += "|";
        content += profiles[i].description;
        content += "\n";
    }

    return writeTextFile(String(INDEX_FILE), content);
}

bool SaveLoadSystem::ensureProfileInIndex(const String& name, const String& description) {
    ProfileInfo profiles[16];
    uint8_t count = 0;
    if (!loadProfileIndex(profiles, 16, count)) {
        last_error = "Profile index load failed";
        return false;
    }

    for (uint8_t i = 0; i < count; i++) {
        if (profiles[i].name == name) {
            profiles[i].description = description;
            return saveProfileIndex(profiles, count);
        }
    }

    if (count >= 16) {
        last_error = "Profile index full";
        return false;
    }

    profiles[count].name = name;
    profiles[count].description = description;
    count++;

    return saveProfileIndex(profiles, count);
}

bool SaveLoadSystem::begin() {
    spiffs_ready = SPIFFS.begin(true);

    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    sd_ready = SD.begin(SD_SPI_CS_PIN, SPI, 25000000);

    if (sd_ready) {
        backend = STORAGE_BACKEND_SD;
        active_fs = &SD;
    } else if (spiffs_ready) {
        backend = STORAGE_BACKEND_SPIFFS;
        active_fs = &SPIFFS;
    } else {
        last_error = "Both SD and SPIFFS unavailable";
        active_fs = nullptr;
        return false;
    }

    if (!ensureDirRecursive(BASE_DIR) || !ensureDirRecursive(PROFILES_DIR)) {
        return false;
    }

    if (!active_fs->exists(SHARED_SHOP_ITEMS_FILE)) {
        writeTextFile(String(SHARED_SHOP_ITEMS_FILE), "# shared shop items\n");
    }

    loadGlobalSettings();

    if (active_profile_name.length() == 0) {
        active_profile_name = (backend == STORAGE_BACKEND_SD) ? "default" : "local";
        active_profile_description = (backend == STORAGE_BACKEND_SD) ? "Default profile" : "Local profile (no SD)";
    }

    if (!setActiveProfile(active_profile_name, active_profile_description)) {
        return false;
    }

    return true;
}

const char* SaveLoadSystem::getBackendName() const {
    return (backend == STORAGE_BACKEND_SD) ? "SD" : "SPIFFS";
}

bool SaveLoadSystem::setActiveProfile(const String& profile_name, const String& description) {
    if (!active_fs) {
        last_error = "No active filesystem";
        return false;
    }

    String safe_name = sanitizeProfileName(profile_name);
    if (safe_name.length() == 0) {
        last_error = "Invalid profile name";
        return false;
    }

    String dir = profileDir(safe_name);
    if (!ensureDirRecursive(dir.c_str())) {
        return false;
    }

    active_profile_name = safe_name;
    if (description.length() > 0) {
        active_profile_description = description;
    } else if (active_profile_description.length() == 0) {
        active_profile_description = "Profile";
    }

    if (!ensureProfileInIndex(active_profile_name, active_profile_description)) {
        return false;
    }

    return saveGlobalSettings();
}

bool SaveLoadSystem::listProfiles(ProfileInfo profiles[], uint8_t max_count, uint8_t& out_count) const {
    if (!active_fs) {
        out_count = 0;
        return false;
    }

    if (!loadProfileIndex(profiles, max_count, out_count)) {
        return false;
    }

    if (!sd_ready && max_count > 0) {
        out_count = 1;
        profiles[0].name = active_profile_name;
        profiles[0].description = active_profile_description;
    }

    return true;
}

bool SaveLoadSystem::saveCurrentProfileState(const LevelSystem& level_system, const HealthSystem& health_system,
                                             int32_t money, int8_t timezone_offset) {
    String path = profileStatePath(active_profile_name);

    String data = "level=" + String(level_system.getLevel()) + "\n";
    data += "current_xp=" + String(level_system.getCurrentXP()) + "\n";
    data += "lifetime_xp=" + String(level_system.getLifetimeXP()) + "\n";
    data += "money=" + String(money) + "\n";
    data += "health_mode=" + String((int)health_system.getMode()) + "\n";
    data += "manual_health=" + String((int)health_system.getManualHealth()) + "\n";
    data += "start_hour=" + String((int)health_system.getTimeBasedStartHour()) + "\n";
    data += "end_hour=" + String((int)health_system.getTimeBasedEndHour()) + "\n";
    data += "timezone_offset=" + String((int)timezone_offset) + "\n";

    return writeTextFile(path, data);
}

bool SaveLoadSystem::loadCurrentProfileState(LevelSystem& level_system, HealthSystem& health_system,
                                             int32_t& money, int8_t& timezone_offset) {
    String path = profileStatePath(active_profile_name);
    String data;
    if (!readTextFile(path, data)) {
        last_error = "No state file for active profile";
        return false;
    }

    int loaded_level = 1;
    uint32_t loaded_current_xp = 0;
    uint32_t loaded_lifetime_xp = 0;
    int loaded_money = 0;
    int loaded_mode = 0;
    int loaded_manual_health = 100;
    int loaded_start_hour = 8;
    int loaded_end_hour = 22;
    int loaded_timezone = 0;

    int start = 0;
    while (start < data.length()) {
        int end = data.indexOf('\n', start);
        if (end < 0) {
            end = data.length();
        }

        String line = data.substring(start, end);
        line.trim();

        String key, value;
        if (parseKeyValueLine(line, key, value)) {
            if (key == "level") loaded_level = value.toInt();
            else if (key == "current_xp") loaded_current_xp = (uint32_t)value.toInt();
            else if (key == "lifetime_xp") loaded_lifetime_xp = (uint32_t)value.toInt();
            else if (key == "money") loaded_money = value.toInt();
            else if (key == "health_mode") loaded_mode = value.toInt();
            else if (key == "manual_health") loaded_manual_health = value.toInt();
            else if (key == "start_hour") loaded_start_hour = value.toInt();
            else if (key == "end_hour") loaded_end_hour = value.toInt();
            else if (key == "timezone_offset") loaded_timezone = value.toInt();
        }

        start = end + 1;
    }

    level_system.setLevel((uint16_t)loaded_level);
    level_system.setCurrentXP(loaded_current_xp);
    level_system.setLifetimeXP(loaded_lifetime_xp);

    money = loaded_money;

    if (loaded_mode == HEALTH_MANUAL) {
        health_system.setManualMode();
        health_system.setManualHealth((uint8_t)loaded_manual_health);
    } else {
        health_system.setTimeBasedMode();
        health_system.setTimeBasedCycle((uint8_t)loaded_start_hour, (uint8_t)loaded_end_hour);
    }

    timezone_offset = (int8_t)loaded_timezone;

    return true;
}

bool SaveLoadSystem::saveSharedShopItems(const String& shop_items_text) {
    return writeTextFile(String(SHARED_SHOP_ITEMS_FILE), shop_items_text);
}

bool SaveLoadSystem::loadSharedShopItems(String& shop_items_text) const {
    return readTextFile(String(SHARED_SHOP_ITEMS_FILE), shop_items_text);
}

bool SaveLoadSystem::saveTasks(const TaskManager& task_manager) {
    if (!active_fs) {
        last_error = "No active filesystem";
        return false;
    }

    String path = profileTasksPath(active_profile_name);
    File file = active_fs->open(path.c_str(), FILE_WRITE);
    if (!file) {
        last_error = "Failed to open tasks file for writing";
        return false;
    }

    uint16_t count = 0;
    Task* tasks = task_manager.getAllTasks(count);
    for (uint16_t i = 0; i < count; i++) {
        const Task& t = tasks[i];
        file.print("T|");
        file.print(t.id); file.print("|");
        file.print(t.name); file.print("|");
        file.print(t.details); file.print("|");
        file.print((int)t.difficulty); file.print("|");
        file.print((int)t.urgency); file.print("|");
        file.print((int)t.fear); file.print("|");
        file.print((int)t.repetition); file.print("|");
        file.print(t.duration_minutes); file.print("|");
        file.print(t.due_date); file.print("|");
        file.print(t.reward_money); file.print("|");
        file.print(t.reward_item_id); file.print("|");
        file.print((int)t.reward_item_quantity); file.print("|");
        file.print(t.linked_skill_category_id); file.print("|");
        file.print(t.linked_skill_id); file.print("|");
        file.print((int)t.completed); file.print("|");
        file.print(t.creation_time); file.print("|");
        file.println(t.completion_time);
    }

    file.close();
    return true;
}

bool SaveLoadSystem::loadTasks(TaskManager& task_manager) {
    if (!active_fs) {
        last_error = "No active filesystem";
        return false;
    }

    String path = profileTasksPath(active_profile_name);
    if (!active_fs->exists(path.c_str())) {
        last_error = "No tasks file";
        return false;
    }

    File file = active_fs->open(path.c_str(), FILE_READ);
    if (!file) {
        last_error = "Failed to open tasks file";
        return false;
    }

    task_manager.clearAll();

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (!line.startsWith("T|")) {
            continue;
        }

        // Parse pipe-separated record.
        String fields[19];
        uint8_t fcount = 0;
        int start = 0;
        while (fcount < 19) {
            int sep = line.indexOf('|', start);
            if (sep < 0) {
                fields[fcount++] = line.substring(start);
                break;
            }
            fields[fcount++] = line.substring(start, sep);
            start = sep + 1;
        }

        if (fcount < 19) {
            continue;
        }

        uint16_t task_id = (uint16_t)fields[1].toInt();
        const char* name = fields[2].c_str();
        const char* details = fields[3].c_str();
        uint8_t difficulty = (uint8_t)fields[4].toInt();
        uint8_t urgency = (uint8_t)fields[5].toInt();
        uint8_t fear = (uint8_t)fields[6].toInt();
        uint8_t repetition = (uint8_t)fields[7].toInt();
        uint16_t duration = (uint16_t)fields[8].toInt();
        uint32_t due_date = (uint32_t)fields[9].toInt();
        uint32_t reward_money = (uint32_t)fields[10].toInt();
        uint16_t reward_item_id = (uint16_t)fields[11].toInt();
        uint8_t reward_item_qty = (uint8_t)fields[12].toInt();
        uint16_t linked_cat = (uint16_t)fields[13].toInt();
        uint16_t linked_skill = (uint16_t)fields[14].toInt();
        bool completed = fields[15].toInt() != 0;
        uint32_t creation_time = (uint32_t)fields[16].toInt();
        uint32_t completion_time = (uint32_t)fields[17].toInt();

        uint16_t created_id = task_manager.addTask(name, details, difficulty, urgency, fear,
                                                   repetition, duration, due_date, reward_money,
                                                   linked_cat, linked_skill, reward_item_id, reward_item_qty);
        if (created_id == 0) {
            continue;
        }

        Task* task = task_manager.getTask(created_id);
        if (!task) {
            continue;
        }

        task->id = task_id;
        task->completed = completed;
        task->creation_time = creation_time;
        task->completion_time = completion_time;
    }

    file.close();
    return true;
}

bool SaveLoadSystem::saveSharedShopCatalog(const ShopSystem& shop_system) {
    if (!active_fs) {
        last_error = "No active filesystem";
        return false;
    }

    String path = sharedShopCatalogPath();
    File file = active_fs->open(path.c_str(), FILE_WRITE);
    if (!file) {
        last_error = "Failed to open shop catalog";
        return false;
    }

    const ShopItem* items = shop_system.itemsRaw();
    uint16_t item_count = shop_system.getItemCountRaw();
    for (uint16_t i = 0; i < item_count; i++) {
        const ShopItem& it = items[i];
        if (!it.active) continue;
        file.print("I|");
        file.print(it.id); file.print("|");
        file.print(it.name); file.print("|");
        file.print(it.description); file.print("|");
        file.print((int)it.collectible_only); file.print("|");
        file.print((int)it.effect_type); file.print("|");
        file.print(it.effect_value); file.print("|");
        file.print(it.base_price); file.print("|");
        file.print(it.buy_limit); file.print("|");
        file.print(it.cooldown_seconds); file.print("|");
        file.println(it.price_increase_per_purchase);
    }

    const CraftRecipe* recipes = shop_system.recipesRaw();
    uint16_t recipe_count = shop_system.getRecipeCountRaw();
    for (uint16_t i = 0; i < recipe_count; i++) {
        const CraftRecipe& r = recipes[i];
        if (!r.active) continue;
        file.print("R|");
        file.print(r.id); file.print("|");
        file.print((int)r.input_count); file.print("|");
        for (uint8_t k = 0; k < 3; k++) {
            file.print(r.inputs[k].item_id); file.print("|");
            file.print((int)r.inputs[k].quantity); file.print("|");
        }
        file.print(r.output_item_id); file.print("|");
        file.println((int)r.output_quantity);
    }

    file.close();
    return true;
}

bool SaveLoadSystem::loadSharedShopCatalog(ShopSystem& shop_system) {
    if (!active_fs) {
        last_error = "No active filesystem";
        return false;
    }

    String path = sharedShopCatalogPath();
    if (!active_fs->exists(path.c_str())) {
        last_error = "No shop catalog";
        return false;
    }

    File file = active_fs->open(path.c_str(), FILE_READ);
    if (!file) {
        last_error = "Open shop catalog failed";
        return false;
    }

    shop_system.resetAll();

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() < 3) continue;

        if (line.startsWith("I|")) {
            std::vector<String> f;
            int s = 0;
            while (true) {
                int p = line.indexOf('|', s);
                if (p < 0) {
                    f.push_back(line.substring(s));
                    break;
                }
                f.push_back(line.substring(s, p));
                s = p + 1;
            }
            if (f.size() < 11) continue;
            shop_system.addItem(f[2].c_str(), f[3].c_str(), f[4].toInt() != 0,
                                (ItemEffectType)f[5].toInt(), (uint32_t)f[6].toInt(),
                                f[7].toInt(), (uint16_t)f[8].toInt(),
                                (uint32_t)f[9].toInt(), f[10].toInt());
        }
    }

    file.seek(0, SeekSet);
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (!line.startsWith("R|")) continue;

        std::vector<String> f;
        int s = 0;
        while (true) {
            int p = line.indexOf('|', s);
            if (p < 0) {
                f.push_back(line.substring(s));
                break;
            }
            f.push_back(line.substring(s, p));
            s = p + 1;
        }
        if (f.size() < 11) continue;

        uint8_t input_count = (uint8_t)f[2].toInt();
        CraftIngredient inputs[3];
        for (uint8_t i = 0; i < 3; i++) {
            inputs[i].item_id = (uint16_t)f[3 + i * 2].toInt();
            inputs[i].quantity = (uint8_t)f[4 + i * 2].toInt();
        }

        uint16_t out_item = (uint16_t)f[9].toInt();
        uint8_t out_qty = (uint8_t)f[10].toInt();
        shop_system.addRecipe(inputs, input_count, out_item, out_qty);
    }

    file.close();
    return true;
}

bool SaveLoadSystem::saveProfileShopState(const ShopSystem& shop_system) {
    if (!active_fs) {
        last_error = "No active filesystem";
        return false;
    }

    String path = profileShopStatePath(active_profile_name);
    File file = active_fs->open(path.c_str(), FILE_WRITE);
    if (!file) {
        last_error = "Open profile shop state failed";
        return false;
    }

    const ShopItem* items = shop_system.itemsRaw();
    uint16_t item_count = shop_system.getItemCountRaw();
    for (uint16_t i = 0; i < item_count; i++) {
        const ShopItem& it = items[i];
        if (!it.active) continue;
        file.print("S|");
        file.print(it.id); file.print("|");
        file.print(it.current_price); file.print("|");
        file.print(it.times_purchased); file.print("|");
        file.println(it.last_purchase_time);
    }

    const InventoryEntry* inv = shop_system.inventoryRaw();
    for (uint16_t i = 0; i < MAX_INVENTORY_STACKS; i++) {
        if (!inv[i].active) continue;
        file.print("V|");
        file.print(inv[i].item_id); file.print("|");
        file.println(inv[i].quantity);
    }

    file.close();
    return true;
}

bool SaveLoadSystem::loadProfileShopState(ShopSystem& shop_system) {
    if (!active_fs) {
        last_error = "No active filesystem";
        return false;
    }

    String path = profileShopStatePath(active_profile_name);
    if (!active_fs->exists(path.c_str())) {
        last_error = "No profile shop state";
        return false;
    }

    File file = active_fs->open(path.c_str(), FILE_READ);
    if (!file) {
        last_error = "Open profile shop state failed";
        return false;
    }

    shop_system.resetRuntimeState();

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() < 3) continue;

        std::vector<String> f;
        int s = 0;
        while (true) {
            int p = line.indexOf('|', s);
            if (p < 0) {
                f.push_back(line.substring(s));
                break;
            }
            f.push_back(line.substring(s, p));
            s = p + 1;
        }

        if (f.size() < 3) continue;
        if (f[0] == "S" && f.size() >= 5) {
            uint16_t item_id = (uint16_t)f[1].toInt();
            ShopItem* item = shop_system.getItem(item_id);
            if (!item) continue;
            item->current_price = f[2].toInt();
            item->times_purchased = (uint16_t)f[3].toInt();
            item->last_purchase_time = (uint32_t)f[4].toInt();
        } else if (f[0] == "V" && f.size() >= 3) {
            uint16_t item_id = (uint16_t)f[1].toInt();
            uint16_t qty = (uint16_t)f[2].toInt();
            if (qty > 0) {
                shop_system.addInventoryItem(item_id, qty);
            }
        }
    }

    file.close();
    return true;
}

bool SaveLoadSystem::saveSkills(const SkillSystem& skill_system) {
    if (!active_fs) {
        last_error = "No active filesystem";
        return false;
    }

    String path = profileSkillsPath(active_profile_name);
    File file = active_fs->open(path.c_str(), FILE_WRITE);
    if (!file) {
        last_error = "Failed to open skills file for writing";
        return false;
    }

    uint16_t category_count = skill_system.getActiveCategoryCount();
    for (uint16_t i = 0; i < category_count; i++) {
        const SkillCategory* category = skill_system.getCategoryByActiveIndex(i);
        if (!category) {
            continue;
        }

        file.print("C|");
        file.print(category->name);
        file.print("|");
        file.println(category->description);
    }

    for (uint16_t i = 0; i < category_count; i++) {
        const SkillCategory* category = skill_system.getCategoryByActiveIndex(i);
        if (!category) {
            continue;
        }

        uint16_t skills_in_category = skill_system.getActiveSkillCountInCategory(category->id);
        for (uint16_t j = 0; j < skills_in_category; j++) {
            const Skill* skill = skill_system.getSkillInCategoryByActiveIndex(category->id, j);
            if (!skill) {
                continue;
            }

            file.print("S|");
            file.print(category->name);
            file.print("|");
            file.print(skill->name);
            file.print("|");
            file.print(skill->details);
            file.print("|");
            file.print(skill->level);
            file.print("|");
            file.print(skill->current_xp);
            file.print("|");
            file.println(skill->lifetime_xp);
        }
    }

    file.close();
    return true;
}

bool SaveLoadSystem::loadSkills(SkillSystem& skill_system) {
    if (!active_fs) {
        last_error = "No active filesystem";
        return false;
    }

    String path = profileSkillsPath(active_profile_name);
    if (!active_fs->exists(path.c_str())) {
        last_error = "No saved skills file";
        return false;
    }

    File file = active_fs->open(path.c_str(), FILE_READ);
    if (!file) {
        last_error = "Failed to open skills file for reading";
        return false;
    }

    skill_system.reset();

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() < 3) {
            continue;
        }

        int p1 = line.indexOf('|');
        if (p1 < 0) {
            continue;
        }

        char record_type = line.charAt(0);

        if (record_type == 'C') {
            int p2 = line.indexOf('|', p1 + 1);
            if (p2 < 0) {
                continue;
            }

            String name = line.substring(p1 + 1, p2);
            String description = line.substring(p2 + 1);
            if (name.length() > 0) {
                skill_system.addCategory(name.c_str(), description.c_str());
            }
        }
    }

    file.seek(0, SeekSet);
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() < 3) {
            continue;
        }

        int p1 = line.indexOf('|');
        if (p1 < 0) {
            continue;
        }

        char record_type = line.charAt(0);
        if (record_type != 'S') {
            continue;
        }

        int p2 = line.indexOf('|', p1 + 1);
        int p3 = (p2 >= 0) ? line.indexOf('|', p2 + 1) : -1;
        int p4 = (p3 >= 0) ? line.indexOf('|', p3 + 1) : -1;
        int p5 = (p4 >= 0) ? line.indexOf('|', p4 + 1) : -1;
        int p6 = (p5 >= 0) ? line.indexOf('|', p5 + 1) : -1;

        if (p2 < 0 || p3 < 0 || p4 < 0 || p5 < 0 || p6 < 0) {
            continue;
        }

        String category_name = line.substring(p1 + 1, p2);
        String skill_name = line.substring(p2 + 1, p3);
        String details = line.substring(p3 + 1, p4);
        uint16_t level = (uint16_t)line.substring(p4 + 1, p5).toInt();
        uint32_t current_xp = (uint32_t)line.substring(p5 + 1, p6).toInt();
        uint32_t lifetime_xp = (uint32_t)line.substring(p6 + 1).toInt();

        uint16_t category_id = skill_system.findCategoryByName(category_name.c_str());
        if (category_id == 0) {
            continue;
        }

        uint16_t skill_id = skill_system.addSkill(category_id, skill_name.c_str(), details.c_str());
        if (skill_id > 0) {
            skill_system.setSkillProgress(skill_id, level, current_xp, lifetime_xp);
        }
    }

    file.close();
    return true;
}

bool SaveLoadSystem::hasSavedSkills() const {
    if (!active_fs) {
        return false;
    }

    String path = profileSkillsPath(active_profile_name);
    return active_fs->exists(path.c_str());
}

bool SaveLoadSystem::clearSkillsSave() {
    if (!active_fs) {
        last_error = "No active filesystem";
        return false;
    }

    String path = profileSkillsPath(active_profile_name);
    if (!active_fs->exists(path.c_str())) {
        return true;
    }

    bool ok = active_fs->remove(path.c_str());
    if (!ok) {
        last_error = "Failed to delete skills file";
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Global device config (WiFi credentials, timezone, health schedule)
// ---------------------------------------------------------------------------

bool SaveLoadSystem::saveGlobalConfig(const GlobalSettings& cfg) {
    if (!active_fs) return false;
    String path = String(BASE_DIR) + "/global_config.txt";
    String data = "";
    data += "wifi_ssid=" + String(cfg.wifi_ssid) + "\n";
    data += "wifi_password=" + String(cfg.wifi_password) + "\n";
    data += "timezone=" + String((int)cfg.timezone_offset) + "\n";
    data += "health_time_based=" + String(cfg.health_time_based ? 1 : 0) + "\n";
    data += "health_wake=" + String((int)cfg.health_wake_hour) + "\n";
    data += "health_sleep=" + String((int)cfg.health_sleep_hour) + "\n";
    data += "visual_feedback=" + String(cfg.visual_feedback_enabled ? 1 : 0) + "\n";
    data += "audio_feedback=" + String(cfg.audio_feedback_enabled ? 1 : 0) + "\n";
    data += "audio_volume=" + String((int)cfg.audio_volume) + "\n";
    return writeTextFile(path, data);
}

bool SaveLoadSystem::loadGlobalConfig(GlobalSettings& cfg) {
    if (!active_fs) return false;
    String path = String(BASE_DIR) + "/global_config.txt";
    String data;
    if (!readTextFile(path, data)) return false;

    int start = 0;
    while (start < (int)data.length()) {
        int end = data.indexOf('\n', start);
        if (end < 0) end = data.length();
        String line = data.substring(start, end);
        line.trim();

        String key, value;
        if (parseKeyValueLine(line, key, value)) {
            if (key == "wifi_ssid") {
                strncpy(cfg.wifi_ssid, value.c_str(), MAX_SSID_LEN - 1);
                cfg.wifi_ssid[MAX_SSID_LEN - 1] = '\0';
            } else if (key == "wifi_password") {
                strncpy(cfg.wifi_password, value.c_str(), MAX_PASSWORD_LEN - 1);
                cfg.wifi_password[MAX_PASSWORD_LEN - 1] = '\0';
            } else if (key == "timezone") {
                cfg.timezone_offset = (int8_t)value.toInt();
            } else if (key == "health_time_based") {
                cfg.health_time_based = value.toInt() != 0;
            } else if (key == "health_wake") {
                cfg.health_wake_hour = (uint8_t)value.toInt();
            } else if (key == "health_sleep") {
                cfg.health_sleep_hour = (uint8_t)value.toInt();
            } else if (key == "visual_feedback") {
                cfg.visual_feedback_enabled = value.toInt() != 0;
            } else if (key == "audio_feedback") {
                cfg.audio_feedback_enabled = value.toInt() != 0;
            } else if (key == "audio_volume") {
                int v = value.toInt();
                if (v < 0) v = 0;
                if (v > 255) v = 255;
                cfg.audio_volume = (uint8_t)v;
            }
        }
        start = end + 1;
    }
    return true;
}
