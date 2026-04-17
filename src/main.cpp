#include <M5Cardputer.h>
#include <ctype.h>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include "tasks/TaskManager.h"
#include "levelsystem/LevelSystem.h"
#include "health/HealthSystem.h"
#include "time/TimeSync.h"
#include "skills/SkillSystem.h"
#include "shop/ShopSystem.h"
#include "persistence/SaveLoadSystem.h"
#include "settings/SettingsSystem.h"

TaskManager task_manager;
LevelSystem level_system;
HealthSystem health_system;
TimeSync time_sync;
SkillSystem skill_system;
ShopSystem shop_system;
SaveLoadSystem save_load_system;
SettingsSystem settings_system;

// Forward declarations
bool saveAllProfileData();
void normalizeTaskSelection();
void normalizeSkillSelection();
void normalizeShopSelection();
void drawSkillsSpiderChart(int cx, int cy, int radius);

enum FeedbackEvent {
    FEEDBACK_TASK_COMPLETE = 0,
    FEEDBACK_TASK_FAILED = 1,
    FEEDBACK_LEVEL_UP = 2
};

enum UiScreen {
    UI_DASHBOARD = 0,
    UI_TASKS = 1,
    UI_SKILLS = 2,
    UI_SHOP = 3,
    UI_PROFILES = 4,
    UI_SAVE_LOAD = 5,
    UI_SETTINGS = 6,
    UI_HELP = 7
};

enum TextInputPurpose {
    INPUT_NONE = 0,
    INPUT_ADD_CATEGORY = 1,
    INPUT_ADD_SKILL = 2,
    INPUT_NEW_PROFILE = 3,
    INPUT_PROFILE_DESCRIPTION = 4,
    INPUT_NEW_TASK_NAME = 5,
    INPUT_EDIT_TASK_DETAILS = 6,
    INPUT_NEW_SHOP_ITEM_NAME = 7,
    INPUT_WIFI_PASSWORD = 8,
    INPUT_MANUAL_TIME = 9,
    INPUT_MANUAL_HEALTH = 10
};

enum NavCommand {
    NAV_NONE = 0,
    NAV_UP,
    NAV_DOWN,
    NAV_LEFT,
    NAV_RIGHT,
    NAV_SELECT,
    NAV_BACK
};

enum TaskEditorField {
    TASK_EDIT_DIFFICULTY = 0,
    TASK_EDIT_URGENCY = 1,
    TASK_EDIT_FEAR = 2,
    TASK_EDIT_REPETITION = 3,
    TASK_EDIT_REWARD_MONEY = 4,
    TASK_EDIT_LINKED_SKILL = 5,
    TASK_EDIT_REWARD_ITEM = 6,
    TASK_EDIT_REWARD_ITEM_QTY = 7
};

UiScreen current_screen = UI_DASHBOARD;
TextInputPurpose input_purpose = INPUT_NONE;
bool text_input_active = false;

char text_input_buffer[64];
uint8_t text_input_len = 0;
uint16_t pending_skill_category_id = 0;
uint16_t pending_task_id = 0;

uint16_t selected_task_index = 0;
TaskEditorField selected_task_field = TASK_EDIT_DIFFICULTY;
bool task_edit_mode = false;

uint16_t selected_category_index = 0;
uint16_t selected_skill_index = 0;

uint16_t selected_shop_item_index = 0;
uint16_t selected_shop_recipe_index = 0;
bool shop_setup_mode = false;
bool shop_focus_items = true;

CraftIngredient recipe_builder_inputs[3];
uint8_t recipe_builder_input_count = 1;
uint16_t recipe_builder_output_item_id = 0;
uint8_t recipe_builder_output_qty = 1;
uint8_t recipe_builder_slot = 0;

uint16_t selected_profile_index = 0;
ProfileInfo profile_list[12];
uint8_t profile_count = 0;

uint8_t save_menu_index = 0;

// Settings screen state
enum SettingsSection {
    SETT_WIFI = 0,
    SETT_TIMEZONE = 1,
    SETT_TIME = 2,
    SETT_HEALTH = 3,
    SETT_RAND_TASK = 4,
    SETT_FEEDBACK = 5,
    SETT_SECTION_COUNT = 6
};
SettingsSection sett_section = SETT_WIFI;
uint8_t sett_wifi_sel = 0;         // selected scanned network index
bool sett_wifi_scanning = false;

// Manual time editor fields: H M S d m Y
uint8_t sett_time_vals[6] = {9, 0, 0, 16, 4, 26};  // hh mm ss dd MM yy(2-digit)
uint8_t sett_time_field = 0;

int32_t player_money = 0;
bool storage_ready = false;

// Profile statistics
uint32_t profile_tasks_completed = 0;
uint32_t profile_tasks_failed = 0;
uint32_t profile_tasks_created = 0;
int32_t profile_money_gained = 0;
int32_t profile_money_spent = 0;

// Manual health mode state
bool manual_health_active = false;
uint32_t manual_health_last_input_time = 0;
uint8_t manual_health_input_percent = 100;

// UI control scrolling state
uint8_t control_scroll_offset = 0;
uint32_t control_scroll_last_update = 0;

// Screen selector state
bool screen_selector_active = false;
uint8_t screen_selector_index = 0;  // 0=DASH, 1=TASKS, 2=SKILLS, etc
const uint8_t SCREEN_SELECTOR_COUNT = 8;  // DASH, TASKS, SKILLS, SHOP, PROF, SAVE, SETTINGS, HELP

// Screensaver state
bool screensaver_active = false;
uint32_t screensaver_anim_time = 0;

// Screen brightness (0-255)
uint8_t screen_brightness = 255;

// Boot intro state
bool boot_complete = false;

char status_line[120] = "Ready";
uint32_t status_line_until = 0;
uint32_t last_ui_render_ms = 0;

M5Canvas ui_canvas(&M5Cardputer.Display);
bool ui_canvas_ready = false;

// HUD animation state
float hud_health_anim = 100.0f;
float hud_level_anim = 0.0f;  // 0..100

void triggerFeedback(FeedbackEvent ev);
void updateHudAnimation();

void setStatus(const char* message, uint32_t ms = 2500) {
    strncpy(status_line, message, sizeof(status_line) - 1);
    status_line[sizeof(status_line) - 1] = '\0';
    status_line_until = millis() + ms;
    Serial.println(status_line);
}

void triggerFeedback(FeedbackEvent ev) {
    GlobalSettings& cfg = settings_system.settings();

    if (cfg.visual_feedback_enabled) {
        if (ev == FEEDBACK_LEVEL_UP) {
            setStatus("LEVEL UP!", 2200);
        } else if (ev == FEEDBACK_TASK_COMPLETE) {
            setStatus("Task completed", 1600);
        } else {
            setStatus("Task marked failed", 1600);
        }
    }

    if (cfg.audio_feedback_enabled) {
        uint16_t freq = 1200;
        uint16_t dur = 90;
        if (ev == FEEDBACK_LEVEL_UP) {
            freq = 1700;
            dur = 160;
        } else if (ev == FEEDBACK_TASK_FAILED) {
            freq = 520;
            dur = 120;
        }

        M5Cardputer.Speaker.setVolume(cfg.audio_volume);
        M5Cardputer.Speaker.tone(freq, dur);
    }
}

void updateHudAnimation() {
    float target_health = (float)health_system.getHealth();
    float target_level = (float)level_system.getXPProgress();

    hud_health_anim += (target_health - hud_health_anim) * 0.18f;
    hud_level_anim += (target_level - hud_level_anim) * 0.20f;

    if (fabsf(target_health - hud_health_anim) < 0.2f) hud_health_anim = target_health;
    if (fabsf(target_level - hud_level_anim) < 0.2f) hud_level_anim = target_level;
}

void renderScreensaver() {
    if (!ui_canvas_ready) return;
    
    uint32_t anim_ms = millis() - screensaver_anim_time;
    float phase = (float)(anim_ms % 3000) / 3000.0f;
    
    ui_canvas.fillSprite(0x0000);
    
    // Draw animated dungeon-like pattern
    const uint16_t grid_color = 0x4208;
    const uint16_t accent = 0xD260;
    
    // Animated grid background
    for (int i = 0; i < 240; i += 20) {
        ui_canvas.drawLine(i, 0, i, 160, grid_color);
        ui_canvas.drawLine(0, i, 240, i, grid_color);
    }
    
    // Pulsing circles (magical effect)
    for (uint8_t j = 0; j < 3; j++) {
        float pulse = sinf(phase * 6.28f + j * 2.09f) * 0.5f + 0.5f;
        int radius = (int)(20 + pulse * 30);
        ui_canvas.drawCircle(120, 80, radius, accent);
    }
    
    // Draw "FiveITE" title
    ui_canvas.setTextColor(0xFFFF, 0x0000);
    ui_canvas.setTextSize(3);
    ui_canvas.setCursor(60, 35);
    ui_canvas.println("FiveITE");
    
    // Draw pulsing subtitle
    float pulse_text = sinf(phase * 6.28f) * 0.5f + 0.5f;
    uint16_t text_col = (0xFFFF * (uint16_t)pulse_text) & 0xFFFF;
    ui_canvas.setTextColor(text_col, 0x0000);
    ui_canvas.setTextSize(1);
    ui_canvas.setCursor(50, 120);
    ui_canvas.println("~ RPG Quest Gamifier ~");
    
    ui_canvas.pushSprite(0, 0);
}

void renderBootIntro() {
    if (!ui_canvas_ready) return;
    
    uint32_t boot_ms = millis();
    
    ui_canvas.fillSprite(0x0000);
    
    // Loading bar animation (0-3000ms)
    uint8_t progress = (boot_ms * 100) / 3000;
    if (progress > 100) progress = 100;
    
    ui_canvas.setTextColor(0xFFFF, 0x0000);
    ui_canvas.setTextSize(2);
    ui_canvas.setCursor(30, 20);
    ui_canvas.println("FIVEITE");
    
    ui_canvas.setTextSize(1);
    ui_canvas.setCursor(40, 50);
    ui_canvas.println("~ RPG Quest System ~");
    
    // Draw loading bar
    ui_canvas.drawRect(30, 90, 180, 20, 0xBDF7);
    if (progress > 0) {
        ui_canvas.fillRect(32, 92, (progress * 176) / 100, 16, 0xD260);
    }
    
    ui_canvas.setTextSize(1);
    ui_canvas.setCursor(100, 120);
    ui_canvas.printf("Loading... %d%%", progress);
    
    // Loading messages
    const char* messages[] = {
        "Initializing quests...",
        "Loading skill trees...",
        "Preparing adventures...",
        "Almost ready..."
    };
    
    uint8_t msg_idx = (progress / 25) % 4;
    ui_canvas.setCursor(20, 140);
    ui_canvas.println(messages[msg_idx]);
    
    ui_canvas.pushSprite(0, 0);
    
    if (boot_ms > 3000) {
        boot_complete = true;
    }
}

void drawSkillsSpiderChart(int cx, int cy, int radius) {
    uint16_t cat_count = skill_system.getActiveCategoryCount();
    uint8_t axes = (cat_count > 8) ? 8 : (uint8_t)cat_count;
    if (axes < 3) {
        ui_canvas.drawCircle(cx, cy, radius, 0x6B4D);
        ui_canvas.setTextColor(0xBDF7, 0x2124);
        ui_canvas.setCursor(cx - 32, cy - 4);
        ui_canvas.print("Need 3+ categories");
        return;
    }

    const float step = (2.0f * PI) / (float)axes;
    const float start = -PI / 2.0f;
    const uint16_t grid_col = 0x6B4D;
    const uint16_t spoke_col = 0x94B2;
    const uint16_t data_col = 0x07FF;

    // Grid rings.
    for (uint8_t ring = 1; ring <= 4; ring++) {
        int rr = (radius * ring) / 4;
        int prev_x = 0;
        int prev_y = 0;
        for (uint8_t i = 0; i <= axes; i++) {
            uint8_t a = (i == axes) ? 0 : i;
            float ang = start + step * a;
            int px = cx + (int)(cosf(ang) * rr);
            int py = cy + (int)(sinf(ang) * rr);
            if (i > 0) ui_canvas.drawLine(prev_x, prev_y, px, py, grid_col);
            prev_x = px;
            prev_y = py;
        }
    }

    // Spokes.
    for (uint8_t i = 0; i < axes; i++) {
        float ang = start + step * i;
        int px = cx + (int)(cosf(ang) * radius);
        int py = cy + (int)(sinf(ang) * radius);
        ui_canvas.drawLine(cx, cy, px, py, spoke_col);
    }

    // Data polygon.
    int data_x[8];
    int data_y[8];
    for (uint8_t i = 0; i < axes; i++) {
        const SkillCategory* c = skill_system.getCategoryByActiveIndex(i);
        float avg = 0.0f;
        uint16_t count = 0;
        if (c) {
            uint16_t sc = skill_system.getActiveSkillCountInCategory(c->id);
            for (uint16_t s = 0; s < sc; s++) {
                const Skill* sk = skill_system.getSkillInCategoryByActiveIndex(c->id, s);
                if (sk) {
                    avg += sk->level;
                    count++;
                }
            }
        }
        if (count > 0) avg /= (float)count;
        if (avg > 100.0f) avg = 100.0f;
        float rr = radius * (avg / 100.0f);
        float ang = start + step * i;
        data_x[i] = cx + (int)(cosf(ang) * rr);
        data_y[i] = cy + (int)(sinf(ang) * rr);
    }
    for (uint8_t i = 0; i < axes; i++) {
        uint8_t n = (i + 1) % axes;
        ui_canvas.drawLine(data_x[i], data_y[i], data_x[n], data_y[n], data_col);
        ui_canvas.fillCircle(data_x[i], data_y[i], 2, data_col);
    }
}

UiScreen indexToScreen(uint8_t idx) {
    static const UiScreen screens[] = {UI_DASHBOARD, UI_TASKS, UI_SKILLS, UI_SHOP, UI_PROFILES, UI_SAVE_LOAD, UI_SETTINGS, UI_HELP};
    if (idx < SCREEN_SELECTOR_COUNT) return screens[idx];
    return UI_DASHBOARD;
}

uint8_t screenToIndex(UiScreen screen) {
    static const UiScreen screens[] = {UI_DASHBOARD, UI_TASKS, UI_SKILLS, UI_SHOP, UI_PROFILES, UI_SAVE_LOAD, UI_SETTINGS, UI_HELP};
    for (uint8_t i = 0; i < SCREEN_SELECTOR_COUNT; i++) {
        if (screens[i] == screen) return i;
    }
    return 0;
}

const char* indexToScreenName(uint8_t idx) {
    static const char* names[] = {"DASH", "TASKS", "SKILLS", "SHOP", "PROF", "SAVE", "SETTINGS", "HELP"};
    if (idx < SCREEN_SELECTOR_COUNT) return names[idx];
    return "DASH";
}

void openScreenSelector() {
    screen_selector_active = true;
    screen_selector_index = screenToIndex(current_screen);
}

void closeScreenSelector() {
    screen_selector_active = false;
}

void selectCurrentScreen() {
    UiScreen new_screen = indexToScreen(screen_selector_index);
    current_screen = new_screen;
    closeScreenSelector();
    setStatus(indexToScreenName(screen_selector_index), 800);
}

const Task* selectedTask() {
    uint16_t count = 0;
    Task* tasks = task_manager.getAllTasks(count);
    if (count == 0) return nullptr;
    if (selected_task_index >= count) selected_task_index = count - 1;
    return &tasks[selected_task_index];
}

Task* selectedTaskMutable() {
    uint16_t count = 0;
    Task* tasks = task_manager.getAllTasks(count);
    if (count == 0) return nullptr;
    if (selected_task_index >= count) selected_task_index = count - 1;
    return &tasks[selected_task_index];
}

const SkillCategory* selectedCategory() {
    return skill_system.getCategoryByActiveIndex(selected_category_index);
}

const Skill* selectedSkillInSelectedCategory() {
    const SkillCategory* cat = selectedCategory();
    if (!cat) return nullptr;
    return skill_system.getSkillInCategoryByActiveIndex(cat->id, selected_skill_index);
}

const ShopItem* selectedShopItem() {
    return shop_system.getItemByActiveIndex(selected_shop_item_index);
}

const CraftRecipe* selectedRecipe() {
    return shop_system.getRecipeByActiveIndex(selected_shop_recipe_index);
}

void normalizeTaskSelection() {
    uint16_t count = 0;
    task_manager.getAllTasks(count);
    if (count == 0) {
        selected_task_index = 0;
        return;
    }
    if (selected_task_index >= count) selected_task_index = count - 1;
}

void normalizeSkillSelection() {
    uint16_t cat_count = skill_system.getActiveCategoryCount();
    if (cat_count == 0) {
        selected_category_index = 0;
        selected_skill_index = 0;
        return;
    }
    if (selected_category_index >= cat_count) selected_category_index = cat_count - 1;
    const SkillCategory* cat = selectedCategory();
    if (!cat) {
        selected_skill_index = 0;
        return;
    }
    uint16_t skill_count = skill_system.getActiveSkillCountInCategory(cat->id);
    if (skill_count == 0) {
        selected_skill_index = 0;
    } else if (selected_skill_index >= skill_count) {
        selected_skill_index = skill_count - 1;
    }
}

void normalizeShopSelection() {
    uint16_t item_count = shop_system.getActiveItemCount();
    if (item_count == 0) {
        selected_shop_item_index = 0;
    } else if (selected_shop_item_index >= item_count) {
        selected_shop_item_index = item_count - 1;
    }

    uint16_t recipe_count = shop_system.getActiveRecipeCount();
    if (recipe_count == 0) {
        selected_shop_recipe_index = 0;
    } else if (selected_shop_recipe_index >= recipe_count) {
        selected_shop_recipe_index = recipe_count - 1;
    }
}

void refreshProfiles() {
    profile_count = 0;
    if (!storage_ready) return;

    save_load_system.listProfiles(profile_list, 12, profile_count);
    if (profile_count == 0) {
        profile_count = 1;
        profile_list[0].name = save_load_system.getActiveProfileName();
        profile_list[0].description = save_load_system.getActiveProfileDescription();
    }

    if (selected_profile_index >= profile_count) {
        selected_profile_index = profile_count - 1;
    }
}

void createDefaultShopCatalog() {
    shop_system.addItem("XP Booster", "Adds XP when used", false, ITEM_EFFECT_ADD_XP, 5000, 1500, 0, 0, 250);
    shop_system.addItem("Mystery Box", "Random XP reward", false, ITEM_EFFECT_RANDOM_XP, 10000, 2500, 0, 0, 300);
    shop_system.addItem("Collector Badge", "Collectible", true, ITEM_EFFECT_NONE, 0, 1000, 1, 0, 0);

    const ShopItem* a = shop_system.getItemByActiveIndex(0);
    const ShopItem* b = shop_system.getItemByActiveIndex(2);
    if (a && b) {
        CraftIngredient inputs[3];
        inputs[0].item_id = a->id;
        inputs[0].quantity = 2;
        inputs[1].item_id = b->id;
        inputs[1].quantity = 1;
        shop_system.addRecipe(inputs, 2, a->id, 1);
    }
}

void generateRandomTask() {
    static const char* adjectives[] = {
        "Important", "Quick", "Complex", "Critical", "Routine",
        "Urgent", "Tricky", "Long", "Short", "Creative"
    };
    static const char* nouns[] = {
        "Meeting", "Report", "Email", "Review", "Cleanup",
        "Update", "Fix", "Design", "Analysis", "Planning"
    };
    static const uint8_t adj_count = 10;
    static const uint8_t noun_count = 10;

    char name[MAX_TASK_NAME_LEN];
    snprintf(name, sizeof(name), "%s %s",
             adjectives[esp_random() % adj_count],
             nouns[esp_random() % noun_count]);

    static const char* details_templates[] = {
        "Generated task - fill in details as needed.",
        "Auto-generated. Set context before starting.",
        "Random task. Review and adjust if necessary.",
        "No details yet. Edit to add context."
    };
    const char* details = details_templates[esp_random() % 4];

    uint8_t diff    = (uint8_t)(10 + (esp_random() % 91));
    uint8_t urgency = (uint8_t)(10 + (esp_random() % 91));
    uint8_t fear    = (uint8_t)(esp_random() % 101);

    task_manager.addTask(name, details, diff, urgency, fear, 1, 0, 0, 0, 0, 0, 0, 0);
    normalizeTaskSelection();
    selected_task_index = task_manager.getTaskCount() > 0 ? task_manager.getTaskCount() - 1 : 0;
    saveAllProfileData();
}

void createDemoTasks() {
    // No demo tasks - use the random generator in Settings instead.
}

bool saveSkills() {
    if (!storage_ready) return false;
    return save_load_system.saveSkills(skill_system);
}

bool loadSkills() {
    if (!storage_ready) return false;
    return save_load_system.loadSkills(skill_system);
}

bool saveShopData() {
    if (!storage_ready) return false;
    bool ok = true;
    ok = save_load_system.saveSharedShopCatalog(shop_system) && ok;
    ok = save_load_system.saveProfileShopState(shop_system) && ok;
    return ok;
}

bool loadShopData() {
    if (!storage_ready) return false;
    bool ok_catalog = save_load_system.loadSharedShopCatalog(shop_system);
    if (!ok_catalog) {
        shop_system.resetAll();
        createDefaultShopCatalog();
        save_load_system.saveSharedShopCatalog(shop_system);
    }

    bool ok_state = save_load_system.loadProfileShopState(shop_system);
    if (!ok_state) {
        shop_system.resetRuntimeState();
    }

    return ok_catalog || ok_state;
}

bool saveAllProfileData() {
    if (!storage_ready) return false;

    bool ok = true;
    ok = saveSkills() && ok;
    ok = save_load_system.saveTasks(task_manager) && ok;
    ok = save_load_system.saveCurrentProfileState(level_system, health_system, player_money,
                                                  time_sync.getTimezoneOffset()) && ok;
    ok = save_load_system.saveProfileShopState(shop_system) && ok;

    if (ok) setStatus("Profile saved");
    else setStatus("Profile save failed");
    return ok;
}

bool loadAllProfileData() {
    if (!storage_ready) return false;

    bool ok = true;

    skill_system.reset();
    task_manager.clearAll();

    if (save_load_system.hasSavedSkills()) {
        ok = loadSkills() && ok;
    }

    if (!save_load_system.loadTasks(task_manager)) {
        ok = false;
    }

    int8_t tz = time_sync.getTimezoneOffset();
    int32_t loaded_money = player_money;
    if (save_load_system.loadCurrentProfileState(level_system, health_system, loaded_money, tz)) {
        player_money = loaded_money;
        time_sync.setTimezoneOffset(tz);
    } else {
        ok = false;
    }

    loadShopData();
    normalizeTaskSelection();
    normalizeSkillSelection();
    normalizeShopSelection();

    if (ok) setStatus("Profile loaded");
    else setStatus("Partial load/fallback");
    return ok;
}

void beginTextInput(TextInputPurpose purpose, uint16_t arg = 0) {
    text_input_active = true;
    input_purpose = purpose;
    pending_skill_category_id = arg;
    pending_task_id = arg;
    text_input_len = 0;
    text_input_buffer[0] = '\0';
}

void cancelTextInput() {
    text_input_active = false;
    input_purpose = INPUT_NONE;
    pending_skill_category_id = 0;
    pending_task_id = 0;
    text_input_len = 0;
    text_input_buffer[0] = '\0';
    setStatus("Input cancelled", 1200);
}

void finishTextInput() {
    if (text_input_len == 0) {
        cancelTextInput();
        return;
    }

    if (input_purpose == INPUT_ADD_CATEGORY) {
        if (skill_system.addCategory(text_input_buffer, "Custom") > 0) {
            saveSkills();
            setStatus("Category added");
        } else {
            setStatus("Add category failed");
        }
    } else if (input_purpose == INPUT_ADD_SKILL) {
        if (pending_skill_category_id > 0 && skill_system.addSkill(pending_skill_category_id, text_input_buffer, "Custom") > 0) {
            saveSkills();
            setStatus("Skill added");
        } else {
            setStatus("Add skill failed");
        }
    } else if (input_purpose == INPUT_NEW_PROFILE) {
        if (!save_load_system.isSDReady()) {
            setStatus("SD required for multi profile");
        } else if (save_load_system.setActiveProfile(String(text_input_buffer), "Custom profile")) {
            refreshProfiles();
            skill_system.reset();
            task_manager.clearAll();
            level_system.reset();
            player_money = 0;
            shop_system.resetRuntimeState();
            saveAllProfileData();
            setStatus("Profile created");
        } else {
            setStatus("Create profile failed");
        }
    } else if (input_purpose == INPUT_PROFILE_DESCRIPTION) {
        if (save_load_system.setActiveProfile(save_load_system.getActiveProfileName(), String(text_input_buffer))) {
            refreshProfiles();
            setStatus("Profile description updated");
        } else {
            setStatus("Description update failed");
        }
    } else if (input_purpose == INPUT_NEW_TASK_NAME) {
        const Skill* skill = skill_system.getSkillByActiveIndex(0);
        task_manager.addTask(text_input_buffer, "", 10, 10, 0, 1, 0, 0, 100,
                             skill ? skill->category_id : 0,
                             skill ? skill->id : 0,
                             0, 0);
        normalizeTaskSelection();
        selected_task_index = task_manager.getTaskCount() > 0 ? task_manager.getTaskCount() - 1 : 0;
        saveAllProfileData();
        setStatus("Task created");
    } else if (input_purpose == INPUT_EDIT_TASK_DETAILS) {
        Task* t = selectedTaskMutable();
        if (t) {
            strncpy(t->details, text_input_buffer, MAX_TASK_DETAILS_LEN - 1);
            t->details[MAX_TASK_DETAILS_LEN - 1] = '\0';
            saveAllProfileData();
            setStatus("Task details updated");
        }
    } else if (input_purpose == INPUT_NEW_SHOP_ITEM_NAME) {
        if (shop_system.addItem(text_input_buffer, "Custom", true, ITEM_EFFECT_NONE, 0, 1000, 0, 0, 0) > 0) {
            saveShopData();
            setStatus("Shop item added");
        } else {
            setStatus("Add item failed");
        }
    } else if (input_purpose == INPUT_WIFI_PASSWORD) {
        // text_input_buffer holds the password; pending_skill_category_id holds the network index
        const WiFiNetwork* net = settings_system.getScannedNetwork(sett_wifi_sel);
        if (net) {
            setStatus("Connecting...", 8000);
            if (settings_system.connectWiFi(net->ssid, text_input_buffer, 8000)) {
                settings_system.setStoredCredentials(net->ssid, text_input_buffer);
                save_load_system.saveGlobalConfig(settings_system.settings());
                // Already connected above; avoid reconnect loop in TimeSync.
                if (time_sync.syncWithWiFi(net->ssid, text_input_buffer, 5000)) {
                    setStatus("WiFi+NTP OK");
                } else {
                    setStatus("WiFi OK, NTP fail");
                }
            } else {
                setStatus("WiFi connect failed");
            }
        }
    } else if (input_purpose == INPUT_MANUAL_TIME) {
        // time_input_buffer holds "HH:MM:SS DD/MM/YY" — already applied via sett_time_vals
        setStatus("Time set");
    }

    text_input_active = false;
    input_purpose = INPUT_NONE;
    normalizeSkillSelection();
    normalizeTaskSelection();
    normalizeShopSelection();
}

void handleTextInputChar(char key) {
    if (key == '\r' || key == '\n') {
        finishTextInput();
        return;
    }

    if (key == 8 || key == 127) {
        if (text_input_len > 0) {
            text_input_len--;
            text_input_buffer[text_input_len] = '\0';
        }
        return;
    }

    if (key == '`') {
        cancelTextInput();
        return;
    }

    if (isprint((unsigned char)key) && text_input_len < sizeof(text_input_buffer) - 1) {
        text_input_buffer[text_input_len++] = key;
        text_input_buffer[text_input_len] = '\0';
    }
}

void adjustTaskField(Task& t, bool increase) {
    int delta = increase ? 1 : -1;

    switch (selected_task_field) {
        case TASK_EDIT_DIFFICULTY: {
            int v = (int)t.difficulty + delta * 5;
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            t.difficulty = (uint8_t)v;
            break;
        }
        case TASK_EDIT_URGENCY: {
            int v = (int)t.urgency + delta * 5;
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            t.urgency = (uint8_t)v;
            break;
        }
        case TASK_EDIT_FEAR: {
            int v = (int)t.fear + delta * 5;
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            t.fear = (uint8_t)v;
            break;
        }
        case TASK_EDIT_REPETITION: {
            int v = (int)t.repetition + delta;
            if (v < 1) v = 1;
            if (v > 20) v = 20;
            t.repetition = (uint8_t)v;
            break;
        }
        case TASK_EDIT_REWARD_MONEY: {
            if (t.reward_item_id > 0) break;
            int32_t v = (int32_t)t.reward_money + delta * 100;
            if (v < 0) v = 0;
            t.reward_money = (uint32_t)v;
            break;
        }
        case TASK_EDIT_LINKED_SKILL: {
            uint16_t skill_count = skill_system.getActiveSkillCount();
            if (skill_count == 0) {
                t.linked_skill_id = 0;
                t.linked_skill_category_id = 0;
                break;
            }

            int current_idx = 0;
            bool found = false;
            for (uint16_t i = 0; i < skill_count; i++) {
                const Skill* s = skill_system.getSkillByActiveIndex(i);
                if (s && s->id == t.linked_skill_id) {
                    current_idx = i;
                    found = true;
                    break;
                }
            }
            if (!found) current_idx = 0;
            current_idx += delta;
            if (current_idx < 0) current_idx = skill_count - 1;
            if (current_idx >= skill_count) current_idx = 0;

            const Skill* s = skill_system.getSkillByActiveIndex((uint16_t)current_idx);
            if (s) {
                t.linked_skill_id = s->id;
                t.linked_skill_category_id = s->category_id;
            }
            break;
        }
        case TASK_EDIT_REWARD_ITEM: {
            uint16_t item_count = shop_system.getActiveItemCount();
            int current_idx = -1;
            if (t.reward_item_id > 0) {
                for (uint16_t i = 0; i < item_count; i++) {
                    const ShopItem* it = shop_system.getItemByActiveIndex(i);
                    if (it && it->id == t.reward_item_id) {
                        current_idx = i;
                        break;
                    }
                }
            }
            current_idx += delta;
            if (current_idx < -1) current_idx = item_count > 0 ? item_count - 1 : -1;
            if (current_idx >= (int)item_count) current_idx = -1;

            if (current_idx < 0) {
                t.reward_item_id = 0;
                t.reward_item_quantity = 0;
            } else {
                const ShopItem* it = shop_system.getItemByActiveIndex((uint16_t)current_idx);
                if (it) {
                    t.reward_item_id = it->id;
                    if (t.reward_item_quantity == 0) t.reward_item_quantity = 1;
                    t.reward_money = 0;
                }
            }
            break;
        }
        case TASK_EDIT_REWARD_ITEM_QTY: {
            if (t.reward_item_id == 0) break;
            int v = (int)t.reward_item_quantity + delta;
            if (v < 1) v = 1;
            if (v > 20) v = 20;
            t.reward_item_quantity = (uint8_t)v;
            break;
        }
    }
}

void completeTaskAtIndex(uint16_t task_index) {
    uint16_t count = 0;
    Task* tasks = task_manager.getAllTasks(count);
    if (task_index >= count) {
        setStatus("No task at slot");
        return;
    }

    Task* t = &tasks[task_index];
    uint32_t base_xp = task_manager.completeTask(t->id);
    if (base_xp == 0) {
        setStatus("Task already done", 1400);
        return;
    }

    float health_mul = health_system.getXPMultiplier();
    uint32_t player_xp = health_system.applyHealthMultiplier(base_xp);
    uint16_t player_levels = level_system.addXPWithMultiplier(base_xp, health_mul);

    uint32_t skill_xp = 0;
    uint16_t skill_levels = 0;
    if (t->linked_skill_id > 0) {
        skill_levels = skill_system.addXPFromTask(t->linked_skill_id, player_xp, skill_xp);
    }

    uint32_t money_gain = 0;
    if (t->reward_item_id > 0 && t->reward_item_quantity > 0) {
        shop_system.addInventoryItem(t->reward_item_id, t->reward_item_quantity);
    } else {
        money_gain = t->reward_money;
        if (t->difficulty >= 70 || t->urgency >= 70) {
            money_gain += t->calculateMoneyBonus();
        }
        player_money += (int32_t)money_gain;
    }

    saveAllProfileData();

    triggerFeedback(FEEDBACK_TASK_COMPLETE);
    if (player_levels > 0) {
        triggerFeedback(FEEDBACK_LEVEL_UP);
    }

    char msg[120];
    if (t->reward_item_id > 0) {
        snprintf(msg, sizeof(msg), "Task +%luXP item#%d x%d P+%d S+%d", (unsigned long)player_xp,
                 t->reward_item_id, t->reward_item_quantity, player_levels, skill_levels);
    } else {
        snprintf(msg, sizeof(msg), "Task +%luXP +$%lu P+%d S+%d", (unsigned long)player_xp,
                 (unsigned long)money_gain, player_levels, skill_levels);
    }
    setStatus(msg, 3000);
}

void cycleScreen(int8_t delta) {
    int8_t next = (int8_t)current_screen + delta;
    if (next < 0) next = UI_HELP;
    if (next > UI_HELP) next = UI_DASHBOARD;
    current_screen = (UiScreen)next;

    const char* name = "DASH";
    if (current_screen == UI_TASKS) name = "TASKS";
    else if (current_screen == UI_SKILLS) name = "SKILLS";
    else if (current_screen == UI_SHOP) name = "SHOP";
    else if (current_screen == UI_PROFILES) name = "PROF";
    else if (current_screen == UI_SAVE_LOAD) name = "SAVE";
    else if (current_screen == UI_SETTINGS) name = "SETTINGS";
    else if (current_screen == UI_HELP) name = "HELP";

    setStatus(name, 1000);
}

void handleNavCommand(NavCommand cmd) {
    // Screen selector mode (scrollable vertical list)
    if (screen_selector_active) {
        if (cmd == NAV_UP) {
            if (screen_selector_index > 0) screen_selector_index--;
            else screen_selector_index = SCREEN_SELECTOR_COUNT - 1;
        } else if (cmd == NAV_DOWN) {
            if (screen_selector_index + 1 < SCREEN_SELECTOR_COUNT) screen_selector_index++;
            else screen_selector_index = 0;
        } else if (cmd == NAV_LEFT || cmd == NAV_RIGHT) {
            // Also allow left/right for alternative navigation
            if (cmd == NAV_LEFT) {
                if (screen_selector_index > 0) screen_selector_index--;
                else screen_selector_index = SCREEN_SELECTOR_COUNT - 1;
            } else {
                if (screen_selector_index + 1 < SCREEN_SELECTOR_COUNT) screen_selector_index++;
                else screen_selector_index = 0;
            }
        } else if (cmd == NAV_SELECT) {
            selectCurrentScreen();
        } else if (cmd == NAV_BACK) {
            closeScreenSelector();
        }
        return;
    }

    if (text_input_active) {
        if (cmd == NAV_SELECT) handleTextInputChar('\n');
        else if (cmd == NAV_BACK) cancelTextInput();
        return;
    }

    switch (current_screen) {
        case UI_DASHBOARD:
            if (cmd == NAV_UP && selected_task_index > 0) selected_task_index--;
            else if (cmd == NAV_DOWN) {
                uint16_t count = 0;
                task_manager.getAllTasks(count);
                if (selected_task_index + 1 < count) selected_task_index++;
            } else if (cmd == NAV_SELECT) {
                completeTaskAtIndex(selected_task_index);
            }
            break;

        case UI_TASKS: {
            normalizeTaskSelection();
            Task* t = selectedTaskMutable();
            if (!task_edit_mode) {
                if (cmd == NAV_LEFT) cycleScreen(-1);
                else if (cmd == NAV_RIGHT) cycleScreen(1);
                else if (cmd == NAV_UP && selected_task_index > 0) selected_task_index--;
                else if (cmd == NAV_DOWN) {
                    uint16_t count = 0;
                    task_manager.getAllTasks(count);
                    if (selected_task_index + 1 < count) selected_task_index++;
                } else if (cmd == NAV_SELECT) {
                    completeTaskAtIndex(selected_task_index);
                } else if (cmd == NAV_BACK) {
                    cycleScreen(-1);
                }
            } else {
                if (cmd == NAV_LEFT) {
                    int f = (int)selected_task_field - 1;
                    if (f < 0) f = TASK_EDIT_REWARD_ITEM_QTY;
                    selected_task_field = (TaskEditorField)f;
                } else if (cmd == NAV_RIGHT) {
                    int f = (int)selected_task_field + 1;
                    if (f > TASK_EDIT_REWARD_ITEM_QTY) f = TASK_EDIT_DIFFICULTY;
                    selected_task_field = (TaskEditorField)f;
                } else if (cmd == NAV_UP) {
                    if (t) adjustTaskField(*t, true);
                } else if (cmd == NAV_DOWN) {
                    if (t) adjustTaskField(*t, false);
                } else if (cmd == NAV_SELECT) {
                    saveAllProfileData();
                    setStatus("Task updated");
                } else if (cmd == NAV_BACK) {
                    task_edit_mode = false;
                    setStatus("Task edit closed", 1000);
                }
            }
            break;
        }

        case UI_SKILLS: {
            normalizeSkillSelection();
            const SkillCategory* cat = selectedCategory();
            if (cmd == NAV_LEFT) {
                if (selected_category_index > 0) {
                    selected_category_index--;
                    selected_skill_index = 0;
                } else {
                    cycleScreen(-1);
                }
            } else if (cmd == NAV_RIGHT) {
                if (selected_category_index + 1 < skill_system.getActiveCategoryCount()) {
                    selected_category_index++;
                    selected_skill_index = 0;
                } else {
                    cycleScreen(1);
                }
            } else if (cmd == NAV_UP) {
                if (selected_skill_index > 0) selected_skill_index--;
            } else if (cmd == NAV_DOWN) {
                if (cat && selected_skill_index + 1 < skill_system.getActiveSkillCountInCategory(cat->id)) {
                    selected_skill_index++;
                }
            } else if (cmd == NAV_BACK) {
                cycleScreen(-1);
            }
            break;
        }

        case UI_SHOP:
            normalizeShopSelection();
            if (cmd == NAV_LEFT) {
                if (shop_focus_items) cycleScreen(-1);
                else shop_focus_items = true;
            } else if (cmd == NAV_RIGHT) {
                if (!shop_focus_items) cycleScreen(1);
                else shop_focus_items = false;
            } else if (cmd == NAV_UP) {
                if (shop_focus_items && selected_shop_item_index > 0) selected_shop_item_index--;
                if (!shop_focus_items && selected_shop_recipe_index > 0) selected_shop_recipe_index--;
            } else if (cmd == NAV_DOWN) {
                if (shop_focus_items) {
                    if (selected_shop_item_index + 1 < shop_system.getActiveItemCount()) selected_shop_item_index++;
                } else {
                    if (selected_shop_recipe_index + 1 < shop_system.getActiveRecipeCount()) selected_shop_recipe_index++;
                }
            } else if (cmd == NAV_SELECT) {
                uint32_t now_s = millis() / 1000;
                if (shop_focus_items) {
                    const ShopItem* it = selectedShopItem();
                    if (it && shop_system.buyItem(it->id, player_money, now_s)) {
                        saveAllProfileData();
                        setStatus("Item purchased");
                    } else {
                        setStatus("Cannot buy item");
                    }
                } else {
                    const CraftRecipe* r = selectedRecipe();
                    if (r && shop_system.craftRecipe(r->id)) {
                        saveAllProfileData();
                        setStatus("Craft success");
                    } else {
                        setStatus("Cannot craft");
                    }
                }
            } else if (cmd == NAV_BACK) {
                if (shop_focus_items) {
                    const ShopItem* it = selectedShopItem();
                    if (it && shop_system.useItem(it->id, level_system, millis() / 1000)) {
                        saveAllProfileData();
                        setStatus("Item used");
                    } else {
                        setStatus("Cannot use item");
                    }
                } else {
                    cycleScreen(-1);
                }
            }
            break;

        case UI_PROFILES:
            refreshProfiles();
            if (cmd == NAV_LEFT) cycleScreen(-1);
            else if (cmd == NAV_RIGHT) cycleScreen(1);
            else if (cmd == NAV_UP && selected_profile_index > 0) selected_profile_index--;
            else if (cmd == NAV_DOWN && selected_profile_index + 1 < profile_count) selected_profile_index++;
            else if (cmd == NAV_SELECT) {
                if (profile_count > 0 && save_load_system.setActiveProfile(profile_list[selected_profile_index].name,
                                                                            profile_list[selected_profile_index].description)) {
                    loadAllProfileData();
                    setStatus("Profile switched");
                }
            }
            break;

        case UI_SAVE_LOAD:
            if (cmd == NAV_LEFT) cycleScreen(-1);
            else if (cmd == NAV_RIGHT) cycleScreen(1);
            else if (cmd == NAV_UP && save_menu_index > 0) save_menu_index--;
            else if (cmd == NAV_DOWN && save_menu_index < 4) save_menu_index++;
            else if (cmd == NAV_SELECT) {
                if (save_menu_index == 0) saveAllProfileData();
                else if (save_menu_index == 1) loadAllProfileData();
                else if (save_menu_index == 2) {
                    save_load_system.saveSharedShopCatalog(shop_system);
                    setStatus("Shared catalog saved");
                } else if (save_menu_index == 3) {
                    if (!save_load_system.loadSharedShopCatalog(shop_system)) {
                        createDefaultShopCatalog();
                    }
                    save_load_system.loadProfileShopState(shop_system);
                    setStatus("Shared catalog loaded");
                } else if (save_menu_index == 4) {
                    save_load_system.clearSkillsSave();
                    setStatus("Skills save cleared");
                }
            }
            break;

        case UI_SETTINGS: {
            // Section switch: Left/Right
            if (cmd == NAV_LEFT) {
                int s = (int)sett_section - 1;
                if (s < 0) { cycleScreen(-1); break; }
                sett_section = (SettingsSection)s;
            } else if (cmd == NAV_RIGHT) {
                int s = (int)sett_section + 1;
                if (s >= SETT_SECTION_COUNT) { cycleScreen(1); break; }
                sett_section = (SettingsSection)s;
            } else if (sett_section == SETT_WIFI) {
                if (cmd == NAV_UP && sett_wifi_sel > 0) sett_wifi_sel--;
                else if (cmd == NAV_DOWN && sett_wifi_sel + 1 < settings_system.getScannedCount()) sett_wifi_sel++;
                else if (cmd == NAV_SELECT) {
                    // Connect to selected network
                    const WiFiNetwork* net = settings_system.getScannedNetwork(sett_wifi_sel);
                    if (net) {
                        if (net->has_password) {
                            text_input_len = 0;
                            text_input_buffer[0] = '\0';
                            // pre-fill with stored password if ssid matches
                            char stored_ssid[MAX_SSID_LEN];
                            char stored_pass[MAX_PASSWORD_LEN];
                            settings_system.getStoredCredentials(stored_ssid, stored_pass);
                            if (strcmp(stored_ssid, net->ssid) == 0 && stored_pass[0] != '\0') {
                                strncpy(text_input_buffer, stored_pass, sizeof(text_input_buffer) - 1);
                                text_input_len = strlen(text_input_buffer);
                            }
                            beginTextInput(INPUT_WIFI_PASSWORD, sett_wifi_sel);
                        } else {
                            setStatus("Connecting...", 8000);
                            if (settings_system.connectWiFi(net->ssid, "", 8000)) {
                                settings_system.setStoredCredentials(net->ssid, "");
                                save_load_system.saveGlobalConfig(settings_system.settings());
                                // Already connected above; avoid reconnect loop in TimeSync.
                                if (time_sync.syncWithWiFi(net->ssid, "", 5000)) {
                                    setStatus("WiFi+NTP OK");
                                } else {
                                    setStatus("WiFi OK, NTP fail");
                                }
                            } else {
                                setStatus("WiFi failed");
                            }
                        }
                    }
                } else if (cmd == NAV_BACK) {
                    // Trigger scan
                    setStatus("Scanning WiFi...", 3000);
                    settings_system.scanNetworks();
                    sett_wifi_sel = 0;
                    char buf[48];
                    snprintf(buf, sizeof(buf), "Found %d networks", settings_system.getScannedCount());
                    setStatus(buf, 2500);
                }
            } else if (sett_section == SETT_TIMEZONE) {
                GlobalSettings& cfg = settings_system.settings();
                if (cmd == NAV_UP) {
                    if (cfg.timezone_offset < 14) cfg.timezone_offset++;
                    time_sync.setTimezoneOffset(cfg.timezone_offset);
                    save_load_system.saveGlobalConfig(cfg);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "TZ UTC%+d", (int)cfg.timezone_offset);
                    setStatus(buf, 1500);
                } else if (cmd == NAV_DOWN) {
                    if (cfg.timezone_offset > -12) cfg.timezone_offset--;
                    time_sync.setTimezoneOffset(cfg.timezone_offset);
                    save_load_system.saveGlobalConfig(cfg);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "TZ UTC%+d", (int)cfg.timezone_offset);
                    setStatus(buf, 1500);
                } else if (cmd == NAV_SELECT) {
                    // Re-sync NTP with stored credentials
                    char ssid[MAX_SSID_LEN], pass[MAX_PASSWORD_LEN];
                    settings_system.getStoredCredentials(ssid, pass);
                    if (ssid[0] != '\0') {
                        setStatus("NTP sync...", 6000);
                        if (time_sync.syncWithWiFi(ssid, pass)) {
                            setStatus("NTP sync OK");
                        } else {
                            setStatus("NTP sync failed");
                        }
                    } else {
                        setStatus("No stored WiFi");
                    }
                }
            } else if (sett_section == SETT_TIME) {
                // Navigate fields and increment/decrement
                static const uint8_t field_max[6] = {23, 59, 59, 31, 12, 99};
                static const uint8_t field_min[6] = {0, 0, 0, 1, 1, 0};
                if (cmd == NAV_LEFT && sett_time_field > 0) sett_time_field--;
                else if (cmd == NAV_RIGHT && sett_time_field < 5) sett_time_field++;
                else if (cmd == NAV_UP) {
                    if (sett_time_vals[sett_time_field] < field_max[sett_time_field])
                        sett_time_vals[sett_time_field]++;
                    else
                        sett_time_vals[sett_time_field] = field_min[sett_time_field];
                } else if (cmd == NAV_DOWN) {
                    if (sett_time_vals[sett_time_field] > field_min[sett_time_field])
                        sett_time_vals[sett_time_field]--;
                    else
                        sett_time_vals[sett_time_field] = field_max[sett_time_field];
                } else if (cmd == NAV_SELECT) {
                    uint16_t year = 2000 + (uint16_t)sett_time_vals[5];
                    time_sync.setManualTime(sett_time_vals[0], sett_time_vals[1], sett_time_vals[2],
                                           sett_time_vals[3], sett_time_vals[4], year);
                    setStatus("Manual time applied");
                }
            } else if (sett_section == SETT_HEALTH) {
                GlobalSettings& cfg = settings_system.settings();
                if (cmd == NAV_SELECT) {
                    cfg.health_time_based = !cfg.health_time_based;
                    if (cfg.health_time_based) {
                        health_system.setTimeBasedMode();
                        health_system.setTimeBasedCycle(cfg.health_wake_hour, cfg.health_sleep_hour);
                        setStatus("Time-based health");
                    } else {
                        health_system.setManualMode();
                        setStatus("Manual health");
                    }
                    save_load_system.saveGlobalConfig(cfg);
                } else if (cmd == NAV_UP) {
                    if (cfg.health_wake_hour < 12) cfg.health_wake_hour++;
                    health_system.setTimeBasedCycle(cfg.health_wake_hour, cfg.health_sleep_hour);
                    save_load_system.saveGlobalConfig(cfg);
                    char buf[40]; snprintf(buf, sizeof(buf), "Wake %d:00", cfg.health_wake_hour);
                    setStatus(buf, 1200);
                } else if (cmd == NAV_DOWN) {
                    if (cfg.health_wake_hour > 0) cfg.health_wake_hour--;
                    health_system.setTimeBasedCycle(cfg.health_wake_hour, cfg.health_sleep_hour);
                    save_load_system.saveGlobalConfig(cfg);
                    char buf[40]; snprintf(buf, sizeof(buf), "Wake %d:00", cfg.health_wake_hour);
                    setStatus(buf, 1200);
                } else if (cmd == NAV_LEFT) {
                    if (cfg.health_sleep_hour > 12) cfg.health_sleep_hour--;
                    health_system.setTimeBasedCycle(cfg.health_wake_hour, cfg.health_sleep_hour);
                    save_load_system.saveGlobalConfig(cfg);
                    char buf[40]; snprintf(buf, sizeof(buf), "Sleep %d:00", cfg.health_sleep_hour);
                    setStatus(buf, 1200);
                } else if (cmd == NAV_RIGHT) {
                    if (cfg.health_sleep_hour < 23) cfg.health_sleep_hour++;
                    health_system.setTimeBasedCycle(cfg.health_wake_hour, cfg.health_sleep_hour);
                    save_load_system.saveGlobalConfig(cfg);
                    char buf[40]; snprintf(buf, sizeof(buf), "Sleep %d:00", cfg.health_sleep_hour);
                    setStatus(buf, 1200);
                }
            } else if (sett_section == SETT_RAND_TASK) {
                if (cmd == NAV_SELECT) {
                    generateRandomTask();
                    setStatus("Random task added");
                }
            } else if (sett_section == SETT_FEEDBACK) {
                GlobalSettings& cfg = settings_system.settings();
                if (cmd == NAV_SELECT) {
                    cfg.visual_feedback_enabled = !cfg.visual_feedback_enabled;
                    save_load_system.saveGlobalConfig(cfg);
                    setStatus(cfg.visual_feedback_enabled ? "Visual feedback ON" : "Visual feedback OFF", 1400);
                } else if (cmd == NAV_BACK) {
                    cfg.audio_feedback_enabled = !cfg.audio_feedback_enabled;
                    save_load_system.saveGlobalConfig(cfg);
                    setStatus(cfg.audio_feedback_enabled ? "Audio feedback ON" : "Audio feedback OFF", 1400);
                } else if (cmd == NAV_UP) {
                    if (cfg.audio_volume < 245) cfg.audio_volume += 10;
                    else cfg.audio_volume = 255;
                    save_load_system.saveGlobalConfig(cfg);
                    setStatus("Volume up", 900);
                } else if (cmd == NAV_DOWN) {
                    if (cfg.audio_volume > 10) cfg.audio_volume -= 10;
                    else cfg.audio_volume = 0;
                    save_load_system.saveGlobalConfig(cfg);
                    setStatus("Volume down", 900);
                }
            }
            break;
        }

        case UI_HELP:
            if (cmd == NAV_LEFT) cycleScreen(-1);
            else if (cmd == NAV_RIGHT) cycleScreen(1);
            break;
    }
}

void handleKeyInput(char key) {
    if (text_input_active) {
        handleTextInputChar(key);
        return;
    }

    // G0 button triggers screensaver
    if (key == 'g' || key == 'G') {
        if (!screensaver_active) {
            screensaver_active = true;
            screensaver_anim_time = millis();
            setStatus("Screensaver ON - Press any key to exit", 2000);
        }
        return;
    }

    // Brightness controls: [ for dim, ] for bright
    if (key == '[' || key == '{') {
        if (screen_brightness > 20) {
            screen_brightness -= 20;
            M5Cardputer.Display.setBrightness(screen_brightness);
            char msg[32];
            snprintf(msg, sizeof(msg), "Brightness: %d%%", (screen_brightness * 100) / 255);
            setStatus(msg, 800);
        }
        return;
    }
    if (key == ']' || key == '}') {
        if (screen_brightness < 235) {
            screen_brightness += 20;
            M5Cardputer.Display.setBrightness(screen_brightness);
            char msg[32];
            snprintf(msg, sizeof(msg), "Brightness: %d%%", (screen_brightness * 100) / 255);
            setStatus(msg, 800);
        }
        return;
    }

    // ESC globally opens screen selector (handled in loop before this function).

    // Global manual health shortcut, except in settings.
    if ((key == 'h' || key == 'H') && current_screen != UI_SETTINGS) {
        manual_health_active = true;
        manual_health_input_percent = health_system.getHealth();
        manual_health_last_input_time = millis();
        setStatus("Health input - 1:min 0:max or -/=", 3000);
        return;
    }

    switch (current_screen) {
        case UI_DASHBOARD:
            if (key == 'm' || key == 'M') {
                health_system.setManualMode();
                setStatus("Manual health mode");
            } else if (key == 'b' || key == 'B') {
                health_system.setTimeBasedMode();
                health_system.setTimeBasedCycle(8, 22);
                setStatus("Time health mode");
            } else if (key == 'f' || key == 'F') {
                triggerFeedback(FEEDBACK_TASK_FAILED);
            }
            break;

        case UI_TASKS:
            if (key == 'e' || key == 'E') {
                const Task* t = selectedTask();
                if (t) {
                    task_edit_mode = !task_edit_mode;
                    setStatus(task_edit_mode ? "Task edit mode" : "Task browse mode", 1200);
                }
            } else if (key == 'n' || key == 'N') {
                beginTextInput(INPUT_NEW_TASK_NAME);
            } else if (key == 'd' || key == 'D') {
                Task* t = selectedTaskMutable();
                if (t && task_manager.removeTask(t->id)) {
                    normalizeTaskSelection();
                    saveAllProfileData();
                    setStatus("Task deleted");
                }
            } else if (key == 't' || key == 'T') {
                Task* t = selectedTaskMutable();
                if (t) {
                    pending_task_id = t->id;
                    beginTextInput(INPUT_EDIT_TASK_DETAILS, t->id);
                }
            } else if (key == 'f' || key == 'F') {
                triggerFeedback(FEEDBACK_TASK_FAILED);
            }
            break;

        case UI_SKILLS:
            if (key == 'a' || key == 'A') {
                beginTextInput(INPUT_ADD_CATEGORY);
            } else if (key == 'k' || key == 'K') {
                const SkillCategory* cat = selectedCategory();
                if (cat) {
                    beginTextInput(INPUT_ADD_SKILL, cat->id);
                }
            } else if (key == 'd' || key == 'D') {
                const SkillCategory* cat = selectedCategory();
                if (cat && skill_system.removeCategory(cat->id)) {
                    normalizeSkillSelection();
                    saveSkills();
                    setStatus("Category removed");
                }
            } else if (key == 'x' || key == 'X') {
                const Skill* s = selectedSkillInSelectedCategory();
                if (s && skill_system.removeSkill(s->id)) {
                    saveSkills();
                    setStatus("Skill removed");
                }
            }
            break;

        case UI_SHOP:
            if (key == 'z' || key == 'Z') {
                shop_setup_mode = !shop_setup_mode;
                setStatus(shop_setup_mode ? "Shop setup mode" : "Shop normal mode");
            } else if (shop_setup_mode && (key == 'n' || key == 'N')) {
                beginTextInput(INPUT_NEW_SHOP_ITEM_NAME);
            } else if (shop_setup_mode && (key == 'x' || key == 'X')) {
                const ShopItem* it = selectedShopItem();
                if (it && shop_system.removeItem(it->id)) {
                    saveShopData();
                    normalizeShopSelection();
                    setStatus("Shop item removed");
                }
            } else if (shop_setup_mode && (key == 'r' || key == 'R')) {
                // Create recipe from builder.
                if (recipe_builder_output_item_id > 0 && recipe_builder_input_count >= 1) {
                    if (recipe_builder_input_count == 1 && recipe_builder_inputs[0].quantity < 2) {
                        setStatus("1-input recipe needs qty>=2");
                    } else if (shop_system.addRecipe(recipe_builder_inputs, recipe_builder_input_count,
                                                     recipe_builder_output_item_id, recipe_builder_output_qty) > 0) {
                        saveShopData();
                        setStatus("Recipe added");
                    } else {
                        setStatus("Recipe add failed");
                    }
                }
            } else if (shop_setup_mode && (key == '1' || key == '2' || key == '3')) {
                uint8_t slot = (uint8_t)(key - '1');
                const ShopItem* it = selectedShopItem();
                if (it) {
                    recipe_builder_inputs[slot].item_id = it->id;
                    if (recipe_builder_inputs[slot].quantity == 0) recipe_builder_inputs[slot].quantity = 1;
                    if (slot + 1 > recipe_builder_input_count) recipe_builder_input_count = slot + 1;
                    setStatus("Recipe input set");
                }
            } else if (shop_setup_mode && (key == 'o' || key == 'O')) {
                const ShopItem* it = selectedShopItem();
                if (it) {
                    recipe_builder_output_item_id = it->id;
                    setStatus("Recipe output set");
                }
            } else if (shop_setup_mode && (key == '+' || key == '=')) {
                if (recipe_builder_slot < 3) {
                    if (recipe_builder_inputs[recipe_builder_slot].quantity < 20) recipe_builder_inputs[recipe_builder_slot].quantity++;
                } else if (recipe_builder_slot == 4) {
                    if (recipe_builder_output_qty < 20) recipe_builder_output_qty++;
                }
            } else if (shop_setup_mode && key == '-') {
                if (recipe_builder_slot < 3) {
                    if (recipe_builder_inputs[recipe_builder_slot].quantity > 1) recipe_builder_inputs[recipe_builder_slot].quantity--;
                } else if (recipe_builder_slot == 4) {
                    if (recipe_builder_output_qty > 1) recipe_builder_output_qty--;
                }
            } else if (shop_setup_mode && (key == 'h' || key == 'H')) {
                recipe_builder_slot = (recipe_builder_slot + 1) % 5;
            }
            break;

        case UI_PROFILES:
            if (key == 'n' || key == 'N') {
                beginTextInput(INPUT_NEW_PROFILE);
            } else if (key == 'g' || key == 'G') {
                beginTextInput(INPUT_PROFILE_DESCRIPTION);
            }
            break;

        case UI_SAVE_LOAD:
            break;
    }
}

void renderUI() {
    if (!ui_canvas_ready) return;

    const uint16_t bg = 0x1082;
    const uint16_t panel = 0x2124;
    const uint16_t accent = 0xD260;  // Dark orange
    const uint16_t text = WHITE;
    const uint16_t muted = 0xBDF7;
    const uint16_t yellow = 0xFFE0;

    ui_canvas.fillSprite(bg);
    ui_canvas.setTextColor(text, bg);
    ui_canvas.setTextSize(1);

    // Header with screen name, LVL, HP
    const char* screen_name = "DASH";
    if (current_screen == UI_TASKS) screen_name = "TASKS";
    else if (current_screen == UI_SKILLS) screen_name = "SKILLS";
    else if (current_screen == UI_SHOP) screen_name = "SHOP";
    else if (current_screen == UI_PROFILES) screen_name = "PROF";
    else if (current_screen == UI_SAVE_LOAD) screen_name = "SAVE";
    else if (current_screen == UI_SETTINGS) screen_name = "SETT";
    else if (current_screen == UI_HELP) screen_name = "HELP";

    ui_canvas.fillRoundRect(0, 0, 240, 20, 4, accent);
    ui_canvas.setTextColor(BLACK, accent);
    ui_canvas.setCursor(6, 6);
    ui_canvas.printf("%s", screen_name);
    ui_canvas.setCursor(80, 6);
    ui_canvas.printf("LVL%d  HP%d", level_system.getLevel(), health_system.getHealth());

    // HUD bars - Level and Health
    updateHudAnimation();
    uint8_t lvlw = (uint8_t)((hud_level_anim / 100.0f) * 100.0f);
    uint8_t hpw = (uint8_t)((hud_health_anim / 100.0f) * 100.0f);
    ui_canvas.drawRoundRect(140, 2, 98, 8, 2, BLACK);
    ui_canvas.fillRoundRect(142, 4, (lvlw * 94) / 100, 4, 1, 0x07E0);
    ui_canvas.drawRoundRect(140, 11, 98, 8, 2, BLACK);
    ui_canvas.fillRoundRect(142, 13, (hpw * 94) / 100, 4, 1, 0xF800);
    
    // XP text on level bar (transparent background)
    ui_canvas.setTextColor(0xFFFF);
    ui_canvas.setTextSize(1);
    ui_canvas.setCursor(145, 3);
    uint32_t cur_xp = level_system.getCurrentXP();
    uint32_t next_xp = level_system.getXPForNextLevel();
    ui_canvas.printf("%lu/%lu", (unsigned long)cur_xp, (unsigned long)next_xp);
    
    // Health percentage when in manual health input mode (transparent)
    if (manual_health_active) {
        ui_canvas.setTextColor(0xF81F);  // Red/magenta for visibility
        ui_canvas.setCursor(145, 12);
        ui_canvas.printf("%d%%", manual_health_input_percent);
    }

    // Date/time source + battery.
    String ts = time_sync.getCurrentTimeString();
    String date_part = ts.length() >= 10 ? ts.substring(0, 10) : String("----/--/--");
    String time_part = ts.length() >= 16 ? ts.substring(11, 16) : String("--:--");
    int batt = 0;
    #if defined(ARDUINO)
    batt = M5Cardputer.Power.getBatteryLevel();
    #endif

    // Info bar below header: date + time + battery.
    ui_canvas.fillRoundRect(0, 21, 240, 12, 2, 0x18C3);
    ui_canvas.setTextColor(0xE71C, 0x18C3);
    ui_canvas.setTextSize(1);
    ui_canvas.setCursor(4, 24);
    ui_canvas.printf("%s %s", date_part.c_str(), time_part.c_str());
    ui_canvas.setCursor(188, 24);
    ui_canvas.printf("BAT %d%%", batt);

    // Bottom bar moved up for full visibility.
    ui_canvas.fillRoundRect(0, 124, 240, 14, 2, panel);

    // Right-half scrolling controls (dynamic per screen), starting after left quarter.
    if (millis() - control_scroll_last_update > 120) {
        control_scroll_offset += 2;
        control_scroll_last_update = millis();
    }
    String scroll_text = "Controls: ";
    if (screen_selector_active) {
        scroll_text += ";/.:Move  ENT:Select  `:Close  ESC:Menu";
    } else if (manual_health_active) {
        scroll_text += "1-0:Set  -=Down  =:Up  ENT:Apply  `:Cancel  ESC:Menu";
    } else if (text_input_active) {
        scroll_text += "Type text  DEL:Erase  ENT:Save  `:Cancel  ESC:Menu";
    } else if (current_screen == UI_DASHBOARD) {
        scroll_text += ";/.:Task  ENT:Done  ESC:Menu  G0:Saver  H:Health  [:Dim  ]:Bright";
    } else if (current_screen == UI_TASKS) {
        if (task_edit_mode) scroll_text += ",/:Field  ;/.:Value  ENT:Save  E:ExitEdit  ESC:Menu";
        else scroll_text += ";/.:Select  ENT:Done  E:Edit  N:New  D:Delete  T:Details  ESC:Menu";
    } else if (current_screen == UI_SKILLS) {
        scroll_text += ",/:Category  ;/.:Skill  A:AddCat  K:AddSkill  X:DelSkill  D:DelCat  ESC:Menu";
    } else if (current_screen == UI_SHOP) {
        scroll_text += ";/.:Select  ,/:Focus  ENT:BuyCraft  Z:Setup  N:Add  R:Recipe  ESC:Menu";
    } else if (current_screen == UI_PROFILES) {
        scroll_text += ";/.:Profile  ENT:Load  N:New  G:Desc  ESC:Menu";
    } else if (current_screen == UI_SAVE_LOAD) {
        scroll_text += ";/.:Option  ENT:Run  ESC:Menu";
    } else if (current_screen == UI_SETTINGS) {
        if (sett_section == SETT_WIFI) scroll_text += ";/.:Network  ENT:Connect  DEL:Scan  ESC:Menu";
        else scroll_text += ",/:Section  ;/.:Value  ENT:Apply  ESC:Menu";
    } else if (current_screen == UI_HELP) {
        scroll_text += "Read guide  ESC:Menu";
    }
    const int scroll_origin_x = 60;  // keep left quarter free for money
    int text_px = (int)scroll_text.length() * 6;
    if (text_px < 1) text_px = 1;
    int loop_px = text_px + 8;
    if (control_scroll_offset >= (uint32_t)loop_px) control_scroll_offset = 0;
    int cur_offset = control_scroll_offset;
    ui_canvas.setTextColor(muted, panel);
    ui_canvas.setCursor(scroll_origin_x - cur_offset, 127);
    ui_canvas.print(scroll_text.c_str());
    ui_canvas.setCursor(scroll_origin_x - cur_offset + loop_px, 127);
    ui_canvas.print(scroll_text.c_str());

    // Repaint reserved money zone after scrolling so text never overlaps money.
    ui_canvas.fillRect(0, 124, scroll_origin_x, 14, panel);
    ui_canvas.setTextColor(yellow, panel);
    ui_canvas.setCursor(4, 127);
    ui_canvas.setTextSize(1);
    ui_canvas.printf("$ %ld", (long)player_money);

    // Screen selector overlay if active (scrollable vertical list)
    if (screen_selector_active) {
        ui_canvas.fillRoundRect(30, 25, 180, 100, 4, panel);
        ui_canvas.setTextColor(text, panel);
        ui_canvas.setCursor(50, 30);
        ui_canvas.setTextSize(2);
        ui_canvas.println("SCREENS");
        
        ui_canvas.setTextSize(1);
        uint8_t visible_items = 6;
        uint8_t start_idx = (screen_selector_index > 2) ? (screen_selector_index - 2) : 0;
        
        for (uint8_t i = 0; i < visible_items && (start_idx + i) < SCREEN_SELECTOR_COUNT; i++) {
            uint8_t idx = start_idx + i;
            uint16_t col = (idx == screen_selector_index) ? yellow : muted;
            uint16_t bg_col = (idx == screen_selector_index) ? accent : panel;
            
            // Highlight bar for selected item
            if (idx == screen_selector_index) {
                ui_canvas.fillRoundRect(35, 42 + (i * 12), 170, 11, 2, bg_col);
            }
            
            ui_canvas.setTextColor(col, panel);
            ui_canvas.setCursor(40, 43 + (i * 12));
            ui_canvas.printf("  %s", indexToScreenName(idx));
        }
        
        ui_canvas.setTextColor(muted, panel);
        ui_canvas.setCursor(35, 107);
        ui_canvas.setTextSize(1);
        ui_canvas.println("Enter=select  `=close");
        
        ui_canvas.pushSprite(0, 0);
        return;
    }

    // Main content area
    ui_canvas.setTextColor(text, panel);
    ui_canvas.fillRoundRect(0, 34, 240, 88, 4, panel);
    ui_canvas.setCursor(6, 40);
    ui_canvas.setTextSize(1);

    if (manual_health_active) {
        // Manual health input display with larger text for readability
        ui_canvas.setTextSize(2);
        ui_canvas.printf("Health: %d%%\n", manual_health_input_percent);
        ui_canvas.setTextSize(1);
        ui_canvas.println("1-9: 10-90%   0: 100%");
        ui_canvas.println("-: down   =: up   `: cancel");
        ui_canvas.setTextColor(muted, panel);
        ui_canvas.setCursor(6, 104);
        ui_canvas.setTextSize(1);
        ui_canvas.println("Enter=confirm  `=cancel");
        ui_canvas.setTextColor(text, bg);
    } else if (text_input_active) {
        if (input_purpose == INPUT_NEW_TASK_NAME) ui_canvas.println("New Task Name:");
        else if (input_purpose == INPUT_EDIT_TASK_DETAILS) ui_canvas.println("Task Details:");
        else if (input_purpose == INPUT_ADD_CATEGORY) ui_canvas.println("New Category:");
        else if (input_purpose == INPUT_ADD_SKILL) ui_canvas.println("New Skill:");
        else if (input_purpose == INPUT_NEW_PROFILE) ui_canvas.println("New Profile Name:");
        else if (input_purpose == INPUT_PROFILE_DESCRIPTION) ui_canvas.println("Profile Description:");
        else if (input_purpose == INPUT_NEW_SHOP_ITEM_NAME) ui_canvas.println("New Shop Item Name:");
        else if (input_purpose == INPUT_WIFI_PASSWORD) {
            const WiFiNetwork* net = settings_system.getScannedNetwork(sett_wifi_sel);
            if (net) ui_canvas.printf("Password for %s:\n", net->ssid);
            else ui_canvas.println("WiFi Password:");
        }

        ui_canvas.fillRoundRect(6, 76, 228, 18, 3, bg);
        ui_canvas.setCursor(10, 80);
        ui_canvas.println(text_input_buffer);
        ui_canvas.setTextColor(muted, panel);
        ui_canvas.setCursor(6, 104);
        ui_canvas.println("Enter=save  Esc=cancel  Del=erase");
        ui_canvas.setTextColor(text, bg);
    } else if (current_screen == UI_DASHBOARD) {
        uint16_t count = 0;
        Task* tasks = task_manager.getAllTasks(count);
        ui_canvas.printf("Tasks:%d selected:%d\n", count, selected_task_index + 1);
        if (count > 0) {
            const Task* t = &tasks[selected_task_index < count ? selected_task_index : 0];
            ui_canvas.printf("%s\n", t->name);
            ui_canvas.printf("D:%d U:%d F:%d\n", t->difficulty, t->urgency, t->fear);
        }
    } else if (current_screen == UI_TASKS) {
        normalizeTaskSelection();
        const Task* t = selectedTask();
        if (!t) {
            ui_canvas.println("No tasks. Press N");
        } else {
            ui_canvas.printf("Task[%d]: %s\n", selected_task_index + 1, t->name);
            ui_canvas.printf("Mode:%s\n", task_edit_mode ? "EDIT" : "BROWSE");
            ui_canvas.printf("Field:%d\n", (int)selected_task_field + 1);
            ui_canvas.printf("D:%d U:%d F:%d R:%d\n", t->difficulty, t->urgency, t->fear, t->repetition);
            ui_canvas.printf("$:%lu skill:%d\n", (unsigned long)t->reward_money, t->linked_skill_id);
            ui_canvas.printf("item:%d x%d\n", t->reward_item_id, t->reward_item_quantity);
        }
        ui_canvas.println("Enter=done, E=toggle edit");
    } else if (current_screen == UI_SKILLS) {
        normalizeSkillSelection();
        const SkillCategory* cat = selectedCategory();
        const Skill* sk = selectedSkillInSelectedCategory();
        ui_canvas.printf("Cats:%d Skills:%d\n", skill_system.getActiveCategoryCount(), skill_system.getActiveSkillCount());
        if (cat) ui_canvas.printf("Cat:%s\n", cat->name);
        if (sk) ui_canvas.printf("Skill:%s Lv%d\n", sk->name, sk->level);
        drawSkillsSpiderChart(184, 84, 34);
    } else if (current_screen == UI_SHOP) {
        normalizeShopSelection();
        const ShopItem* it = selectedShopItem();
        const CraftRecipe* rc = selectedRecipe();
        ui_canvas.printf("Mode:%s Focus:%s\n", shop_setup_mode ? "setup" : "buy", shop_focus_items ? "items" : "recipes");
        if (it) {
            ui_canvas.printf("Item:%s\n", it->name);
            ui_canvas.printf("$%ld inv:%d\n", (long)it->current_price, shop_system.getInventoryQuantity(it->id));
        }
        if (rc) {
            ui_canvas.printf("Recipe#%d out:%d x%d\n", rc->id, rc->output_item_id, rc->output_quantity);
        }
        ui_canvas.println("Shop & crafting");
    } else if (current_screen == UI_PROFILES) {
        refreshProfiles();
        ui_canvas.printf("Active: %s\n", save_load_system.getActiveProfileName().c_str());
        if (profile_count > 0) {
            const ProfileInfo& prof = profile_list[selected_profile_index];
            ui_canvas.printf("Lvl:%d  HP:%d\n", level_system.getLevel(), health_system.getHealth());
            ui_canvas.printf("Tasks: %lu do, %lu fail\n", (unsigned long)prof.tasks_completed, (unsigned long)prof.tasks_failed);
            ui_canvas.printf("Created: %lu  Money: +%ld -%ld\n", (unsigned long)prof.tasks_created, 
                            (long)prof.money_gained, (long)prof.money_spent);
        }
        ui_canvas.println("Profile overview");
    } else if (current_screen == UI_SAVE_LOAD) {
        ui_canvas.printf("Backend:%s\n", save_load_system.getBackendName());
        ui_canvas.printf("Option:%d\n", save_menu_index + 1);
        ui_canvas.println("1 save profile");
        ui_canvas.println("2 load profile");
        ui_canvas.println("3 save shared shop");
        ui_canvas.println("4 load shared shop");
        ui_canvas.println("5 clear skills save");
    } else if (current_screen == UI_SETTINGS) {
        static const char* sett_names[] = {"WiFi", "Timezone", "Time", "Health", "RandTask", "Feedback"};
        ui_canvas.printf("SETTINGS [%s]\n", sett_names[(int)sett_section]);

        if (sett_section == SETT_WIFI) {
            uint8_t cnt = settings_system.getScannedCount();
            if (cnt == 0) {
                ui_canvas.println("No scan. Del=Scan");
            } else {
                for (uint8_t i = 0; i < cnt && i < 6; i++) {
                    const WiFiNetwork* n = settings_system.getScannedNetwork(i);
                    if (!n) break;
                    ui_canvas.printf("%c%s %ddBm %s\n",
                        i == sett_wifi_sel ? '>' : ' ',
                        n->ssid, (int)n->rssi,
                        n->has_password ? "*" : "");
                }
            }
            if (settings_system.isWiFiConnected()) {
                ui_canvas.printf("Connected: %s\n", settings_system.getConnectedSSID());
            } else {
                ui_canvas.println("Not connected");
            }
            ui_canvas.println("Enter=connect Del=scan");
        } else if (sett_section == SETT_TIMEZONE) {
            const GlobalSettings& cfg = settings_system.settings();
            ui_canvas.printf("UTC%+d\n", (int)cfg.timezone_offset);
            ui_canvas.println(";/. change");
            ui_canvas.println("Enter=NTP sync");
        } else if (sett_section == SETT_TIME) {
            static const char* field_labels[] = {"HH","MM","SS","DD","MO","YY"};
            ui_canvas.printf("%02d:%02d:%02d %02d/%02d/20%02d\n",
                sett_time_vals[0], sett_time_vals[1], sett_time_vals[2],
                sett_time_vals[3], sett_time_vals[4], sett_time_vals[5]);
            ui_canvas.printf("Field: %s\n", field_labels[sett_time_field]);
            ui_canvas.println(",/ field  ;/. val");
            ui_canvas.println("Enter=apply manual time");
        } else if (sett_section == SETT_HEALTH) {
            const GlobalSettings& cfg = settings_system.settings();
            ui_canvas.printf("Mode:%s\n", cfg.health_time_based ? "time-based" : "manual");
            ui_canvas.printf("Wake:%02d:00 Sleep:%02d:00\n", cfg.health_wake_hour, cfg.health_sleep_hour);
            ui_canvas.println("Enter toggle mode");
            ui_canvas.println(";/.: wake  ,/: sleep");
        } else if (sett_section == SETT_RAND_TASK) {
            ui_canvas.println("Generate a random task");
            ui_canvas.println("with random name/desc/");
            ui_canvas.println("difficulty/urgency/fear.");
            ui_canvas.println("Enter=generate");
        } else if (sett_section == SETT_FEEDBACK) {
            const GlobalSettings& cfg = settings_system.settings();
            ui_canvas.printf("Visual:%s\n", cfg.visual_feedback_enabled ? "ON" : "OFF");
            ui_canvas.printf("Audio:%s\n", cfg.audio_feedback_enabled ? "ON" : "OFF");
            ui_canvas.printf("Volume:%d\n", (int)cfg.audio_volume);
            ui_canvas.println("Enter toggle visual");
            ui_canvas.println("`: toggle audio ;/. vol");
            ui_canvas.println("Task fail event: key F");
        }
    } else if (current_screen == UI_HELP) {
        ui_canvas.println("FiveITE Help");
        ui_canvas.println("ESC = open selector");
        ui_canvas.println("Arrows , ; . / = nav");
        ui_canvas.println("Enter = select/confirm");
        ui_canvas.println("` = back/cancel");
        ui_canvas.println("H = manual health");
        ui_canvas.println("G = screensaver");
        ui_canvas.println("[ ] = dim/bright");
    }   // end UI_SETTINGS

    if (millis() < status_line_until) {
        // Draw status in a visible location (upper area)
        ui_canvas.fillRoundRect(2, 104, 236, 18, 3, accent);
        ui_canvas.setTextColor(0xFFFF, accent);
        ui_canvas.setCursor(6, 109);
        ui_canvas.setTextSize(1);
        ui_canvas.println(status_line);
    }

    ui_canvas.pushSprite(0, 0);
}

void setup() {
    M5Cardputer.begin();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.setTextSize(1);

    ui_canvas.setColorDepth(16);
    ui_canvas_ready = ui_canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());

    Serial.begin(115200);
    delay(300);

    storage_ready = save_load_system.begin();
    refreshProfiles();

    // Load global device config (WiFi creds, timezone, health schedule)
    GlobalSettings& cfg = settings_system.settings();
    if (storage_ready && save_load_system.loadGlobalConfig(cfg)) {
        time_sync.setTimezoneOffset(cfg.timezone_offset);
        if (cfg.health_time_based) {
            health_system.setTimeBasedMode();
            health_system.setTimeBasedCycle(cfg.health_wake_hour, cfg.health_sleep_hour);
        } else {
            health_system.setManualMode();
        }
        // Populate settings time editor with current values from saved TZ
        sett_time_vals[0] = 0; sett_time_vals[1] = 0; sett_time_vals[2] = 0;
        sett_time_vals[3] = 1; sett_time_vals[4] = 1; sett_time_vals[5] = 26;

        // Try auto WiFi reconnect with saved credentials
        if (cfg.wifi_ssid[0] != '\0') {
            if (settings_system.connectWiFi(cfg.wifi_ssid, cfg.wifi_password, 6000)) {
                time_sync.syncWithWiFi(cfg.wifi_ssid, cfg.wifi_password);
            }
        }
    } else {
        // Defaults
        health_system.setTimeBasedMode();
        health_system.setTimeBasedCycle(8, 22);
        cfg.health_time_based = true;
        cfg.health_wake_hour = 8;
        cfg.health_sleep_hour = 22;
        cfg.timezone_offset = 0;
    }

    bool loaded = false;
    if (storage_ready) {
        loadShopData();
        if (save_load_system.hasSavedSkills()) {
            loaded = loadAllProfileData();
        }
    }

    if (!loaded) {
        // Start clean — user creates own skills/tasks via on-device menus
        setStatus("First boot. Add skills/tasks in menus.");
    }

    if (shop_system.getActiveItemCount() == 0) {
        createDefaultShopCatalog();
        saveShopData();
    }

    setStatus("Ready");
}

void loop() {
    M5Cardputer.update();
    health_system.setCurrentTime(time_sync.getCurrentTime());

    // Auto-apply manual health after 4 seconds of no input
    if (manual_health_active && millis() - manual_health_last_input_time > 4000) {
        uint8_t new_health = (manual_health_input_percent * 100) / 100;
        health_system.setManualHealth(new_health);
        saveAllProfileData();
        manual_health_active = false;
        setStatus("Health auto-set", 1500);
    }

    if (M5Cardputer.Keyboard.isChange()) {
        M5Cardputer.Keyboard.updateKeysState();
        auto& ks = M5Cardputer.Keyboard.keysState();

        // ESC always opens screen selector (global behavior).
        bool esc_pressed = false;
        bool backtick_pressed = false;
        for (char ch : ks.word) {
            if (ch == 27) esc_pressed = true;
            if (ch == '`') backtick_pressed = true;
        }
        if (esc_pressed) {
            openScreenSelector();
            return;
        }

        // Manual health input handling
        if (manual_health_active) {
            bool handled = false;
            
            // Number keys 1-0 for quick health setting
            for (char ch : ks.word) {
                if (ch >= '1' && ch <= '9') {
                    // 1=10%, 2=20%, ... 9=90%
                    manual_health_input_percent = (ch - '0') * 10;
                    manual_health_last_input_time = millis();
                    handled = true;
                    char msg[32];
                    snprintf(msg, sizeof(msg), "Health: %d%%", manual_health_input_percent);
                    setStatus(msg, 1200);
                } else if (ch == '0') {
                    // 0 = 100%
                    manual_health_input_percent = 100;
                    manual_health_last_input_time = millis();
                    handled = true;
                    setStatus("Health: 100%", 1200);
                } else if (ch == '-') {
                    // Decrease by 1
                    if (manual_health_input_percent > 0) manual_health_input_percent--;
                    manual_health_last_input_time = millis();
                    handled = true;
                } else if (ch == '=' || ch == '+') {
                    // Increase by 1
                    if (manual_health_input_percent < 100) manual_health_input_percent++;
                    manual_health_last_input_time = millis();
                    handled = true;
                } else if (ch == '`') {
                    // Backtick cancels manual health.
                    manual_health_active = false;
                    setStatus("Health input cancelled", 1000);
                    handled = true;
                }
            }
            
            // Exit or confirm manual health
            if (ks.enter) {
                uint8_t new_health = (manual_health_input_percent * 100) / 100;
                health_system.setManualHealth(new_health);
                saveAllProfileData();
                manual_health_active = false;
                setStatus("Health set", 1000);
                handled = true;
            }
            
            if (handled) return;
        }

        bool nav_handled = false;

        for (char ch : ks.word) {
            if (ch == ';') {
                handleNavCommand(NAV_UP);
                nav_handled = true;
            } else if (ch == '.') {
                handleNavCommand(NAV_DOWN);
                nav_handled = true;
            } else if (ch == ',') {
                handleNavCommand(NAV_LEFT);
                nav_handled = true;
            } else if (ch == '/') {
                handleNavCommand(NAV_RIGHT);
                nav_handled = true;
            }
        }

        if (!nav_handled && ks.fn) {
            for (char ch : ks.word) {
                if (ch == 'i' || ch == 'I') {
                    handleNavCommand(NAV_UP);
                    nav_handled = true;
                } else if (ch == 'k' || ch == 'K') {
                    handleNavCommand(NAV_DOWN);
                    nav_handled = true;
                } else if (ch == 'j' || ch == 'J') {
                    handleNavCommand(NAV_LEFT);
                    nav_handled = true;
                } else if (ch == 'l' || ch == 'L') {
                    handleNavCommand(NAV_RIGHT);
                    nav_handled = true;
                }
            }
        }

        if (ks.enter) {
            handleNavCommand(NAV_SELECT);
            nav_handled = true;
        }

        // DEL scans WiFi only in WiFi settings section.
        if (!nav_handled && !text_input_active && ks.del && current_screen == UI_SETTINGS && sett_section == SETT_WIFI) {
            handleNavCommand(NAV_BACK);
            nav_handled = true;
        }

        // Backtick remains the generic back/cancel key.
        if (!nav_handled && backtick_pressed) {
            handleNavCommand(NAV_BACK);
            nav_handled = true;
        }

        // In text input mode, Delete should act like backspace instead of cancel.
        if (!nav_handled && text_input_active && ks.del) {
            handleTextInputChar(8);
            nav_handled = true;
        }

        if (!nav_handled) {
            for (char ch : ks.word) {
                handleKeyInput(ch);
            }
        }
    }

    if (millis() - last_ui_render_ms >= 66) {
        renderUI();
        last_ui_render_ms = millis();
    }

    delay(40);
}
