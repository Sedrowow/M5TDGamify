#include <M5Cardputer.h>
#include <ctype.h>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include "tasks/TaskManager.h"
#include "levelsystem/LevelSystem.h"
#include "health/HealthSystem.h"
#include "time/TimeSync.h"
#include "skills/SkillSystem.h"
#include "shop/ShopSystem.h"
#include "persistence/SaveLoadSystem.h"
#include "settings/SettingsSystem.h"
#include "alarms/AlarmSystem.h"

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
void failTaskAtIndex(uint16_t task_index);

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
    UI_INVENTORY = 5,
    UI_SAVE_LOAD = 6,
    UI_SETTINGS = 7,
    UI_HELP = 8,
    UI_ALARMS = 9
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
    INPUT_MANUAL_HEALTH = 10,
    INPUT_SHOP_ITEM_NAME = 11,
    INPUT_SHOP_ITEM_DESC = 12,
    INPUT_RENAME_CATEGORY = 13,
    INPUT_RENAME_SKILL = 14,
    INPUT_EDIT_SKILL_DETAILS = 15,
    INPUT_ALARM_NAME = 16,
    INPUT_TIMER_NAME = 17,
    INPUT_MATH_QUIZ_ANSWER = 18
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
    TASK_EDIT_DURATION = 4,
    TASK_EDIT_DUE_DATE = 5,
    TASK_EDIT_REWARD_MONEY = 6,
    TASK_EDIT_LINKED_SKILLS = 7,
    TASK_EDIT_REWARD_ITEM = 8,
    TASK_EDIT_REWARD_ITEM_QTY = 9
};

enum OverlayMode {
    OVERLAY_NONE = 0,
    OVERLAY_TASK_DURATION = 1,
    OVERLAY_TASK_DUE_DATE = 2,
    OVERLAY_TASK_MONEY = 3,
    OVERLAY_TASK_SKILLS = 4,
    OVERLAY_PROFILE_DELETE = 5,
    // Shop edit overlays
    OVERLAY_SHOP_FIELD_MENU = 6,    // spacebar popup: pick which field to edit
    OVERLAY_SHOP_EFFECT_PICK = 7,   // pick ItemEffectType
    OVERLAY_SHOP_PRICE_EDIT = 8,    // stepper for price / buy_limit / cooldown
    OVERLAY_SHOP_RECIPE_ITEM_PICK = 9, // pick item for a recipe ingredient slot / output
    OVERLAY_SHOP_CONFIRM_DELETE = 10,  // Y/N confirmation before removing item/recipe
    // Skill edit overlays
    OVERLAY_SKILL_FIELD_MENU = 11,      // field picker for category or skill
    OVERLAY_SKILL_DELETE_CONFIRM = 12,  // Y/N confirmation before removing cat/skill
    // Skill quick-action menu (non-edit mode spacebar)
    OVERLAY_SKILL_QUICK_MENU = 13,
    // Skill XP edit stepper
    OVERLAY_SKILL_XP_EDIT = 14,
    // Main XP edit (guarded by math quiz)
    OVERLAY_MATH_QUIZ = 15,
    OVERLAY_MAIN_XP_EDIT = 16,
    // Alarm/Timer detailed setup
    OVERLAY_ALARM_SETUP = 17,
    OVERLAY_TIMER_SETUP = 18,
    // Alarm/timer actively ringing — covers full screen until dismissed
    OVERLAY_ALARM_RINGING = 19
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
OverlayMode overlay_mode = OVERLAY_NONE;
uint8_t task_duration_vals[3] = {0, 0, 0};
uint8_t task_duration_field = 0;
uint8_t task_due_vals[6] = {0, 0, 0, 1, 1, 26};
bool task_due_time_stage = false;
uint8_t task_due_time_field = 0;
int32_t task_money_edit = 0;
uint8_t task_money_step_index = 2;
uint16_t task_skill_menu_category_index = 0;
uint16_t task_skill_menu_skill_index = 0;
bool task_skill_menu_focus_skills = false;
uint32_t profile_delete_confirm_until = 0;

uint16_t selected_category_index = 0;
uint16_t selected_skill_index = 0;
bool skill_edit_mode = false;           // E toggles skill edit mode
bool skill_edit_is_category = false;    // true = editing category, false = editing skill
uint8_t skill_field_menu_index = 0;     // highlighted entry in skill field picker
bool skill_delete_is_category = false;  // whether the pending delete is a category
// Spider chart mode: 0=cat-level, 1=cat-totalXP, 2=skill-level, 3=skill-totalXP
uint8_t skill_spider_mode = 0;
// Skill quick-action menu (non-edit spacebar)
uint8_t skill_quick_menu_index = 0;
// Skill XP stepper state
int32_t skill_xp_edit_value = 0;
uint8_t skill_xp_edit_step_index = 2;
uint16_t skill_xp_edit_skill_id = 0;   // 0 = main player XP
// Math quiz state (guard for main XP edit)
uint8_t math_quiz_question = 0;     // current question index 0-2
int32_t math_quiz_a = 0;
int32_t math_quiz_b = 0;
uint8_t math_quiz_op = 0;           // 0=+, 1=-, 2=*
int64_t math_quiz_answer = 0;
// Alarm/timer detail setup overlay state
uint8_t alarm_setup_entry_index = 0;
uint8_t alarm_setup_field = 0;      // 0=name,1=hour,2=minute for alarm; 0=name,1=H,2=M,3=S for timer

// Help overlay (H key toggles per-screen help)
bool help_overlay_active = false;

bool shop_edit_mode = false;            // E toggles item/recipe edit mode
bool shop_edit_is_recipe = false;       // whether editing a recipe (vs item)
// Field picker popup state
uint8_t shop_field_menu_index = 0;     // currently highlighted entry in field picker
// Effect-type picker state
uint8_t shop_effect_pick_index = 0;
// Price/numeric stepper state (reused for price, buy_limit, cooldown, output qty, ingredient qty)
int32_t shop_num_edit_value = 0;
uint8_t shop_num_edit_step_index = 2;  // index into steps[]
uint8_t shop_num_edit_target = 0;      // see SHOP_NUM_* constants
static const uint8_t SHOP_NUM_BASE_PRICE     = 0;
static const uint8_t SHOP_NUM_BUY_LIMIT      = 1;
static const uint8_t SHOP_NUM_COOLDOWN       = 2;
static const uint8_t SHOP_NUM_RECIPE_IN_QTY  = 3;
static const uint8_t SHOP_NUM_RECIPE_OUT_QTY = 4;
static const uint8_t SHOP_NUM_EFFECT_VALUE   = 5;
// Recipe item-picker state
uint8_t shop_recipe_pick_slot = 0;       // 0-2 = ingredient slot, 3 = output
uint16_t shop_recipe_pick_item_index = 0;// browse index into active items
// Delete confirmation
bool shop_confirm_is_recipe = false;

uint16_t selected_inventory_item_index = 0;  // for UI_INVENTORY browse

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
    SETT_SKILL_XP = 6,
    SETT_SECTION_COUNT = 7
};
SettingsSection sett_section = SETT_WIFI;
uint8_t sett_wifi_sel = 0;         // selected scanned network index
bool sett_wifi_scanning = false;
bool sett_wifi_focus_saved = false; // false=scanned list, true=saved list
uint8_t sett_wifi_saved_sel = 0;   // selected saved credential index

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
const uint8_t SCREEN_SELECTOR_COUNT = 10;  // DASH, TASKS, SKILLS, SHOP, PROF, INV, SAVE, SETTINGS, HELP, ALARMS
bool screen_selector_sub_active = false;
uint8_t screen_selector_sub_index = 0;

// Alarm & timer system
AlarmEntry alarm_entries[MAX_ALARMS];
uint8_t alarm_count = 0;
TimerEntry timer_entries[MAX_TIMERS];
uint8_t timer_count = 0;
// UI state for alarms screen
bool alarm_focus_timers = false;   // false=alarm pane, true=timer pane
uint8_t alarm_sel_index = 0;
uint8_t timer_sel_index = 0;
// Pending add state
uint8_t alarm_add_hour = 7;
uint8_t alarm_add_minute = 0;
uint32_t alarm_add_duration_seconds = 60;   // default timer: 1 minute
uint8_t alarm_edit_index = 0xFF;            // 0xFF = adding new, else editing existing

// Alarm/timer ringing state
bool alarm_ringing = false;
bool alarm_ringing_is_timer = false;
uint8_t alarm_ringing_index = 0;
uint8_t alarm_dismiss_key = 0;     // 0=OPT, 1=ALT, 2=CTRL, 3=FN
uint32_t alarm_snooze_until_unix = 0;  // non-zero = snoozed; re-ring at this unix time
uint32_t alarm_last_buzz_ms = 0;

// Screensaver state
bool screensaver_active = false;
uint32_t screensaver_anim_time = 0;

// Screen brightness (0-255)
uint8_t screen_brightness = 255;
bool screen_off = false;   // G0 physical button toggles display off

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
uint16_t hud_display_level = 1;
uint16_t hud_levelup_pending = 0;
bool hud_levelup_fast = false;
uint32_t hud_levelup_last_step_ms = 0;
uint16_t hud_levelup_step_ms = 220;
uint16_t hud_levelup_sound_ms = 140;
uint32_t hud_bar_sound_last_ms = 0;

void triggerFeedback(FeedbackEvent ev);
void updateHudAnimation();
void playHealthBarBuzz();
void playXPBarBuzz();
void playLevelUpBuzz(uint16_t duration_ms);
TaskEditorField nextTaskField(const Task* task, TaskEditorField current, int delta);
String formatDurationValue(uint32_t total_seconds);
String formatDueDateValue(uint32_t due_date);
String truncateUiText(const char* text, size_t max_len);
String taskRewardItemLabel(const Task& task);
String taskLinkedSkillsLabel(const Task& task);
void openTaskDurationOverlay(const Task& task);
void openTaskDueDateOverlay(const Task& task);
void openTaskMoneyOverlay(const Task& task);
void openTaskSkillsOverlay(const Task& task);
void clampTaskDueEditor();
uint8_t daysInMonth(uint16_t year, uint8_t month);
time_t buildTaskDueTimestamp();
bool taskCategoryFullySelected(const Task& task, uint16_t category_id);
void toggleTaskCategorySelection(Task& task, uint16_t category_id);
void toggleTaskSkillSelection(Task& task, uint16_t category_id, uint16_t skill_id);

void setStatus(const char* message, uint32_t ms = 2500) {
    strncpy(status_line, message, sizeof(status_line) - 1);
    status_line[sizeof(status_line) - 1] = '\0';
    status_line_until = millis() + ms;
    Serial.println(status_line);
}

// Generate a new math quiz question (up to 8-digit operands)
void generateMathQuizQuestion() {
    // op: 0=+, 1=-, 2=* (avoid multiplication for very large numbers)
    math_quiz_op = (uint8_t)(esp_random() % 3);
    if (math_quiz_op == 2) {
        // multiplication: keep smaller to be solvable (up to 4 digits each)
        math_quiz_a = (int32_t)(esp_random() % 9999) + 1;
        math_quiz_b = (int32_t)(esp_random() % 9999) + 1;
        math_quiz_answer = (int64_t)math_quiz_a * math_quiz_b;
    } else {
        // addition / subtraction: up to 8 digits
        math_quiz_a = (int32_t)(esp_random() % 99999999) + 1;
        math_quiz_b = (int32_t)(esp_random() % 99999999) + 1;
        if (math_quiz_op == 0) {
            math_quiz_answer = (int64_t)math_quiz_a + math_quiz_b;
        } else {
            if (math_quiz_b > math_quiz_a) {
                int32_t tmp = math_quiz_a;
                math_quiz_a = math_quiz_b;
                math_quiz_b = tmp;
            }
            math_quiz_answer = (int64_t)math_quiz_a - math_quiz_b;
        }
    }
    char msg[64];
    snprintf(msg, sizeof(msg), "Quiz %d/3", (int)math_quiz_question + 1);
    setStatus(msg, 1000);
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

        // Map volume 0-100 to 0-255 for speaker
        uint8_t spk_vol = (uint8_t)((uint16_t)cfg.audio_volume * 255 / 100);
        M5Cardputer.Speaker.setVolume(spk_vol);
        M5Cardputer.Speaker.tone(freq, dur);
    }
}

void playHealthBarBuzz() {
    const GlobalSettings& cfg = settings_system.settings();
    if (!cfg.audio_feedback_enabled) return;
    uint8_t spk_vol = (uint8_t)((uint16_t)cfg.audio_volume * 255 / 100);
    M5Cardputer.Speaker.setVolume(spk_vol);
    // Low triangle-like buzz: up then down.
    M5Cardputer.Speaker.tone(170, 28);
    M5Cardputer.Speaker.tone(220, 24);
    M5Cardputer.Speaker.tone(170, 28);
}

void playXPBarBuzz() {
    const GlobalSettings& cfg = settings_system.settings();
    if (!cfg.audio_feedback_enabled) return;
    uint8_t spk_vol = (uint8_t)((uint16_t)cfg.audio_volume * 255 / 100);
    M5Cardputer.Speaker.setVolume(spk_vol);
    // Saw-like buzz: rising steps.
    M5Cardputer.Speaker.tone(380, 20);
    M5Cardputer.Speaker.tone(520, 20);
    M5Cardputer.Speaker.tone(680, 20);
}

void playLevelUpBuzz(uint16_t duration_ms) {
    const GlobalSettings& cfg = settings_system.settings();
    if (!cfg.audio_feedback_enabled) return;
    uint8_t spk_vol = (uint8_t)((uint16_t)cfg.audio_volume * 255 / 100);
    M5Cardputer.Speaker.setVolume(spk_vol);
    M5Cardputer.Speaker.tone(1700, duration_ms);
}

void updateHudAnimation() {
    float target_health = (float)health_system.getHealth();
    if (hud_display_level == 0) hud_display_level = level_system.getLevel();

    float target_level = (hud_levelup_pending > 0) ? 100.0f : (float)level_system.getXPProgress();

    hud_health_anim += (target_health - hud_health_anim) * 0.18f;
    hud_level_anim += (target_level - hud_level_anim) * (hud_levelup_pending > 0 ? 0.35f : 0.20f);

    if (fabsf(target_health - hud_health_anim) < 0.2f) hud_health_anim = target_health;
    if (fabsf(target_level - hud_level_anim) < 0.2f) hud_level_anim = target_level;

    // Bar-change buzz while bar is animating.
    if (millis() - hud_bar_sound_last_ms > 120) {
        if (fabsf(target_health - hud_health_anim) > 0.8f) {
            playHealthBarBuzz();
            hud_bar_sound_last_ms = millis();
        } else if (fabsf(target_level - hud_level_anim) > 0.8f) {
            playXPBarBuzz();
            hud_bar_sound_last_ms = millis();
        }
    }

    // Per-level XP animation and sound (instead of instant jump).
    if (hud_levelup_pending > 0 && hud_level_anim >= 99.0f && millis() - hud_levelup_last_step_ms >= hud_levelup_step_ms) {
        hud_levelup_last_step_ms = millis();
        hud_levelup_pending--;
        if (hud_display_level < level_system.getLevel()) hud_display_level++;

        playLevelUpBuzz(hud_levelup_sound_ms);

        if (settings_system.settings().visual_feedback_enabled) {
            if (!hud_levelup_fast || hud_levelup_pending == 0) {
                setStatus("LEVEL UP!", hud_levelup_fast ? 700 : 1200);
            }
        }

        if (hud_levelup_pending > 0) {
            hud_level_anim = 0.0f;
        } else {
            hud_display_level = level_system.getLevel();
            hud_level_anim = (float)level_system.getXPProgress();
        }
    }
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
    
    // Draw "M5TDG" title
    ui_canvas.setTextColor(0xFFFF, 0x0000);
    ui_canvas.setTextSize(3);
    ui_canvas.setCursor(60, 35);
    ui_canvas.println("M5TDG");
    
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
    ui_canvas.println("M5TDG");
    
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

// Spider chart for skills within a single category; max_level caps the scale.
void drawSkillsSpiderChartCategory(int cx, int cy, int radius, uint16_t category_id) {
    uint16_t sk_count = skill_system.getActiveSkillCountInCategory(category_id);
    uint8_t axes = (sk_count > 8) ? 8 : (uint8_t)sk_count;
    if (axes < 3) {
        ui_canvas.drawCircle(cx, cy, radius, 0x6B4D);
        ui_canvas.setTextColor(0xBDF7, 0x2124);
        ui_canvas.setCursor(cx - 28, cy - 4);
        ui_canvas.print("Need 3+ skills");
        return;
    }

    const float step  = (2.0f * PI) / (float)axes;
    const float start = -PI / 2.0f;
    const uint16_t grid_col = 0x6B4D;
    const uint16_t spoke_col = 0x94B2;
    const uint16_t data_col  = 0xFD20;   // orange — distinct from category chart

    // Find max level in category for scale.
    uint16_t max_lvl = 1;
    for (uint8_t i = 0; i < axes; i++) {
        const Skill* sk = skill_system.getSkillInCategoryByActiveIndex(category_id, i);
        if (sk && sk->level > max_lvl) max_lvl = sk->level;
    }

    // Grid rings.
    for (uint8_t ring = 1; ring <= 4; ring++) {
        int rr = (radius * ring) / 4;
        int prev_x = 0, prev_y = 0;
        for (uint8_t i = 0; i <= axes; i++) {
            uint8_t a = (i == axes) ? 0 : i;
            float ang = start + step * a;
            int px = cx + (int)(cosf(ang) * rr);
            int py = cy + (int)(sinf(ang) * rr);
            if (i > 0) ui_canvas.drawLine(prev_x, prev_y, px, py, grid_col);
            prev_x = px; prev_y = py;
        }
    }
    // Spokes.
    for (uint8_t i = 0; i < axes; i++) {
        float ang = start + step * i;
        ui_canvas.drawLine(cx, cy,
            cx + (int)(cosf(ang) * radius),
            cy + (int)(sinf(ang) * radius), spoke_col);
    }
    // Data polygon — scaled to max_lvl.
    int data_x[8], data_y[8];
    for (uint8_t i = 0; i < axes; i++) {
        const Skill* sk = skill_system.getSkillInCategoryByActiveIndex(category_id, i);
        float lvl = sk ? (float)sk->level : 0.0f;
        float rr = radius * (lvl / (float)max_lvl);
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

// Spider chart comparing total lifetime XP across categories.
void drawSpiderCategoryTotalXP(int cx, int cy, int radius) {
    char names[8][MAX_SKILL_CATEGORY_NAME_LEN];
    float values[8];
    uint16_t axes = (uint16_t)skill_system.exportCategoryTotalXP(names, values, 8);
    if (axes < 3) {
        ui_canvas.drawCircle(cx, cy, radius, 0x6B4D);
        ui_canvas.setTextColor(0xBDF7, 0x2124);
        ui_canvas.setCursor(cx - 28, cy - 4);
        ui_canvas.print("Need 3+ cats");
        return;
    }
    uint8_t ax = axes > 8 ? 8 : (uint8_t)axes;
    const float step  = (2.0f * PI) / (float)ax;
    const float start = -PI / 2.0f;
    const uint16_t grid_col = 0x6B4D;
    const uint16_t spoke_col = 0x94B2;
    const uint16_t data_col  = 0x07FF;  // cyan — distinct
    for (uint8_t ring = 1; ring <= 4; ring++) {
        int rr = (radius * ring) / 4;
        int prev_x = 0, prev_y = 0;
        for (uint8_t i = 0; i <= ax; i++) {
            uint8_t a = (i == ax) ? 0 : i;
            float ang = start + step * a;
            int px = cx + (int)(cosf(ang) * rr);
            int py = cy + (int)(sinf(ang) * rr);
            if (i > 0) ui_canvas.drawLine(prev_x, prev_y, px, py, grid_col);
            prev_x = px; prev_y = py;
        }
    }
    for (uint8_t i = 0; i < ax; i++) {
        float ang = start + step * i;
        ui_canvas.drawLine(cx, cy, cx + (int)(cosf(ang) * radius), cy + (int)(sinf(ang) * radius), spoke_col);
    }
    int data_x[8], data_y[8];
    for (uint8_t i = 0; i < ax; i++) {
        float rr = radius * (values[i] / 100.0f);
        float ang = start + step * i;
        data_x[i] = cx + (int)(cosf(ang) * rr);
        data_y[i] = cy + (int)(sinf(ang) * rr);
    }
    for (uint8_t i = 0; i < ax; i++) {
        uint8_t n = (i + 1) % ax;
        ui_canvas.drawLine(data_x[i], data_y[i], data_x[n], data_y[n], data_col);
        ui_canvas.fillCircle(data_x[i], data_y[i], 2, data_col);
    }
}

// Spider chart comparing total lifetime XP for skills in a category.
void drawSpiderSkillsTotalXP(int cx, int cy, int radius, uint16_t category_id) {
    char names[8][MAX_SKILL_NAME_LEN];
    float values[8];
    uint16_t axes = (uint16_t)skill_system.exportSkillsInCategoryTotalXP(category_id, names, values, 8);
    if (axes < 3) {
        ui_canvas.drawCircle(cx, cy, radius, 0x6B4D);
        ui_canvas.setTextColor(0xBDF7, 0x2124);
        ui_canvas.setCursor(cx - 28, cy - 4);
        ui_canvas.print("Need 3+ skills");
        return;
    }
    uint8_t ax = axes > 8 ? 8 : (uint8_t)axes;
    const float step  = (2.0f * PI) / (float)ax;
    const float start = -PI / 2.0f;
    const uint16_t grid_col = 0x6B4D;
    const uint16_t spoke_col = 0x94B2;
    const uint16_t data_col  = 0xF81F;  // magenta — distinct
    for (uint8_t ring = 1; ring <= 4; ring++) {
        int rr = (radius * ring) / 4;
        int prev_x = 0, prev_y = 0;
        for (uint8_t i = 0; i <= ax; i++) {
            uint8_t a = (i == ax) ? 0 : i;
            float ang = start + step * a;
            int px = cx + (int)(cosf(ang) * rr);
            int py = cy + (int)(sinf(ang) * rr);
            if (i > 0) ui_canvas.drawLine(prev_x, prev_y, px, py, grid_col);
            prev_x = px; prev_y = py;
        }
    }
    for (uint8_t i = 0; i < ax; i++) {
        float ang = start + step * i;
        ui_canvas.drawLine(cx, cy, cx + (int)(cosf(ang) * radius), cy + (int)(sinf(ang) * radius), spoke_col);
    }
    int data_x[8], data_y[8];
    for (uint8_t i = 0; i < ax; i++) {
        float rr = radius * (values[i] / 100.0f);
        float ang = start + step * i;
        data_x[i] = cx + (int)(cosf(ang) * rr);
        data_y[i] = cy + (int)(sinf(ang) * rr);
    }
    for (uint8_t i = 0; i < ax; i++) {
        uint8_t n = (i + 1) % ax;
        ui_canvas.drawLine(data_x[i], data_y[i], data_x[n], data_y[n], data_col);
        ui_canvas.fillCircle(data_x[i], data_y[i], 2, data_col);
    }
}

UiScreen indexToScreen(uint8_t idx) {
    // Order: DASH, TASKS, SKILLS, SHOP, ALARMS, PROF, INV, SAVE, SETTINGS, HELP
    static const UiScreen screens[] = {UI_DASHBOARD, UI_TASKS, UI_SKILLS, UI_SHOP, UI_ALARMS, UI_PROFILES, UI_INVENTORY, UI_SAVE_LOAD, UI_SETTINGS, UI_HELP};
    if (idx < SCREEN_SELECTOR_COUNT) return screens[idx];
    return UI_DASHBOARD;
}

uint8_t screenToIndex(UiScreen screen) {
    static const UiScreen screens[] = {UI_DASHBOARD, UI_TASKS, UI_SKILLS, UI_SHOP, UI_ALARMS, UI_PROFILES, UI_INVENTORY, UI_SAVE_LOAD, UI_SETTINGS, UI_HELP};
    for (uint8_t i = 0; i < SCREEN_SELECTOR_COUNT; i++) {
        if (screens[i] == screen) return i;
    }
    return 0;
}

const char* indexToScreenName(uint8_t idx) {
    static const char* names[] = {"DASH", "TASKS", "SKILLS", "SHOP", "ALARMS", "PROF", "INV", "SAVE", "SETTINGS", "HELP"};
    if (idx < SCREEN_SELECTOR_COUNT) return names[idx];
    return "DASH";
}

static const char* sett_sub_names[7] = {"WiFi", "Timezone", "Time", "Health", "Rand Task", "Feedback", "Skill XP"};

uint8_t getSelectorSubCount(uint8_t screen_idx) {
    if (screen_idx == 2) return (uint8_t)skill_system.getActiveCategoryCount(); // SKILLS
    if (screen_idx == 8) return 7; // SETTINGS (index 8 in new order)
    return 0;
}

const char* getSelectorSubName(uint8_t screen_idx, uint8_t sub_idx) {
    if (screen_idx == 2) {
        const SkillCategory* cat = skill_system.getCategoryByActiveIndex(sub_idx);
        return cat ? cat->name : "?";
    }
    if (screen_idx == 8 && sub_idx < 7) return sett_sub_names[sub_idx];
    return "?";
}

void openScreenSelector() {
    screen_selector_active = true;
    screen_selector_sub_active = false;
    screen_selector_sub_index = 0;
    screen_selector_index = screenToIndex(current_screen);
}

void closeScreenSelector() {
    screen_selector_active = false;
    screen_selector_sub_active = false;
    screen_selector_sub_index = 0;
}

void selectCurrentScreen() {
    UiScreen new_screen = indexToScreen(screen_selector_index);
    current_screen = new_screen;
    closeScreenSelector();
    setStatus(indexToScreenName(screen_selector_index), 800);
}

const Task* selectedTask() {
    uint16_t count = task_manager.getVisibleTaskCount();
    if (count == 0) return nullptr;
    if (selected_task_index >= count) selected_task_index = count - 1;
    return task_manager.getVisibleTaskByIndex(selected_task_index);
}

Task* selectedTaskMutable() {
    uint16_t count = task_manager.getVisibleTaskCount();
    if (count == 0) return nullptr;
    if (selected_task_index >= count) selected_task_index = count - 1;
    return task_manager.getVisibleTaskByIndex(selected_task_index);
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
    uint16_t count = task_manager.getVisibleTaskCount();
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
    profile_tasks_created++;
    normalizeTaskSelection();
    selected_task_index = task_manager.getVisibleTaskCount() > 0 ? task_manager.getVisibleTaskCount() - 1 : 0;
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
                                                  time_sync.getTimezoneOffset(),
                                                  profile_tasks_completed, profile_tasks_failed,
                                                  profile_tasks_created, profile_money_gained,
                                                  profile_money_spent) && ok;
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
    profile_tasks_completed = 0;
    profile_tasks_failed = 0;
    profile_tasks_created = 0;
    profile_money_gained = 0;
    profile_money_spent = 0;
    if (save_load_system.loadCurrentProfileState(level_system, health_system, loaded_money, tz,
                                                 profile_tasks_completed, profile_tasks_failed,
                                                 profile_tasks_created, profile_money_gained,
                                                 profile_money_spent)) {
        player_money = loaded_money;
        time_sync.setTimezoneOffset(tz);
    } else {
        ok = false;
    }

    if (profile_tasks_completed == 0 && profile_tasks_failed == 0 && profile_tasks_created == 0) {
        uint16_t raw_count = 0;
        Task* raw_tasks = task_manager.getAllTasks(raw_count);
        profile_tasks_created = raw_count;
        for (uint16_t i = 0; i < raw_count; i++) {
            if (raw_tasks[i].completed) profile_tasks_completed++;
            if (raw_tasks[i].failed) profile_tasks_failed++;
        }
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
    // If cancelled during math quiz, also close the overlay
    if (overlay_mode == OVERLAY_MATH_QUIZ) overlay_mode = OVERLAY_NONE;
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
            profile_tasks_completed = 0;
            profile_tasks_failed = 0;
            profile_tasks_created = 0;
            profile_money_gained = 0;
            profile_money_spent = 0;
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
        profile_tasks_created++;
        normalizeTaskSelection();
        selected_task_index = task_manager.getVisibleTaskCount() > 0 ? task_manager.getVisibleTaskCount() - 1 : 0;
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
    } else if (input_purpose == INPUT_SHOP_ITEM_NAME) {
        ShopItem* it = shop_system.getItem(pending_task_id);
        if (it) {
            strncpy(it->name, text_input_buffer, MAX_SHOP_ITEM_NAME_LEN - 1);
            it->name[MAX_SHOP_ITEM_NAME_LEN - 1] = '\0';
            saveShopData();
            setStatus("Item name updated");
        }
    } else if (input_purpose == INPUT_SHOP_ITEM_DESC) {
        ShopItem* it = shop_system.getItem(pending_task_id);
        if (it) {
            strncpy(it->description, text_input_buffer, MAX_SHOP_ITEM_DESC_LEN - 1);
            it->description[MAX_SHOP_ITEM_DESC_LEN - 1] = '\0';
            saveShopData();
            setStatus("Item desc updated");
        }
    } else if (input_purpose == INPUT_WIFI_PASSWORD) {
        // text_input_buffer holds the password; pending_skill_category_id holds the network index
        const WiFiNetwork* net = settings_system.getScannedNetwork(sett_wifi_sel);
        if (net) {
            setStatus("Connecting...", 8000);
            if (settings_system.connectWiFi(net->ssid, text_input_buffer, 8000)) {
                save_load_system.saveGlobalConfig(settings_system.settings());
                if (time_sync.syncWithWiFi(net->ssid, text_input_buffer, 5000)) {
                    // Save time to RTC after successful NTP sync
                    #if defined(ARDUINO)
                    time_t now_ntp = time_sync.getCurrentTime();
                    struct tm ti_ntp;
                    localtime_r(&now_ntp, &ti_ntp);
                    m5::rtc_datetime_t dt_ntp;
                    dt_ntp.date.year = ti_ntp.tm_year + 1900;
                    dt_ntp.date.month = ti_ntp.tm_mon + 1;
                    dt_ntp.date.date = ti_ntp.tm_mday;
                    dt_ntp.time.hours = ti_ntp.tm_hour;
                    dt_ntp.time.minutes = ti_ntp.tm_min;
                    dt_ntp.time.seconds = ti_ntp.tm_sec;
                    M5.Rtc.setDateTime(dt_ntp);
                    #endif
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
    } else if (input_purpose == INPUT_RENAME_CATEGORY) {
        SkillCategory* cat = skill_system.getCategory(pending_skill_category_id);
        if (cat) {
            strncpy(cat->name, text_input_buffer, MAX_SKILL_CATEGORY_NAME_LEN - 1);
            cat->name[MAX_SKILL_CATEGORY_NAME_LEN - 1] = '\0';
            saveSkills();
            setStatus("Category renamed");
        }
    } else if (input_purpose == INPUT_RENAME_SKILL) {
        Skill* sk = skill_system.getSkill(pending_task_id);
        if (sk) {
            strncpy(sk->name, text_input_buffer, MAX_SKILL_NAME_LEN - 1);
            sk->name[MAX_SKILL_NAME_LEN - 1] = '\0';
            saveSkills();
            setStatus("Skill renamed");
        }
    } else if (input_purpose == INPUT_EDIT_SKILL_DETAILS) {
        Skill* sk = skill_system.getSkill(pending_task_id);
        if (sk) {
            strncpy(sk->details, text_input_buffer, MAX_SKILL_DETAILS_LEN - 1);
            sk->details[MAX_SKILL_DETAILS_LEN - 1] = '\0';
            saveSkills();
            setStatus("Skill details updated");
        }
    } else if (input_purpose == INPUT_MATH_QUIZ_ANSWER) {
        // Parse user answer, requiring full-string consumption to avoid "abc"→0 false-pass
        char* endptr = nullptr;
        int64_t user_ans = (int64_t)strtoll(text_input_buffer, &endptr, 10);
        bool valid_input = (endptr && endptr != text_input_buffer && *endptr == '\0');
        text_input_active = false;
        input_purpose = INPUT_NONE;
        if (valid_input && user_ans == math_quiz_answer) {
            math_quiz_question++;
            if (math_quiz_question >= 3) {
                // Passed all 3 questions
                setStatus("Quiz passed! Edit main XP.", 1500);
                skill_xp_edit_value = 0;
                skill_xp_edit_step_index = 2;
                skill_xp_edit_skill_id = 0;
                overlay_mode = OVERLAY_MAIN_XP_EDIT;
            } else {
                // Next question — restart input
                generateMathQuizQuestion();
                beginTextInput(INPUT_MATH_QUIZ_ANSWER);  // re-enables text_input_active
                overlay_mode = OVERLAY_MATH_QUIZ;
            }
        } else {
            setStatus("Wrong! Access denied.", 2000);
            overlay_mode = OVERLAY_NONE;
        }
        normalizeSkillSelection();
        normalizeTaskSelection();
        normalizeShopSelection();
        return;  // early return to avoid resetting overlay_mode below
    } else if (input_purpose == INPUT_ALARM_NAME) {
        // pending_task_id holds alarm slot index
        uint8_t idx = (uint8_t)pending_task_id;
        if (idx < alarm_count) {
            strncpy(alarm_entries[idx].name, text_input_buffer, ALARM_NAME_LEN - 1);
            alarm_entries[idx].name[ALARM_NAME_LEN - 1] = '\0';
            save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
            setStatus("Alarm name saved");
        }
        // Reopen alarm setup overlay if we were editing from it
        if (alarm_setup_entry_index == idx) {
            text_input_active = false;
            input_purpose = INPUT_NONE;
            overlay_mode = OVERLAY_ALARM_SETUP;
            normalizeSkillSelection(); normalizeTaskSelection(); normalizeShopSelection();
            return;
        }
    } else if (input_purpose == INPUT_TIMER_NAME) {
        uint8_t idx = (uint8_t)pending_task_id;
        if (idx < timer_count) {
            strncpy(timer_entries[idx].name, text_input_buffer, ALARM_NAME_LEN - 1);
            timer_entries[idx].name[ALARM_NAME_LEN - 1] = '\0';
            save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
            setStatus("Timer name saved");
        }
        // Reopen timer setup overlay if we were editing from it
        if (alarm_setup_entry_index == idx) {
            text_input_active = false;
            input_purpose = INPUT_NONE;
            overlay_mode = OVERLAY_TIMER_SETUP;
            normalizeSkillSelection(); normalizeTaskSelection(); normalizeShopSelection();
            return;
        }
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

TaskEditorField nextTaskField(const Task* task, TaskEditorField current, int delta) {
    int idx = (int)current;
    const int max_field = TASK_EDIT_REWARD_ITEM_QTY;
    for (int attempt = 0; attempt <= max_field; attempt++) {
        idx += delta;
        if (idx < 0) idx = max_field;
        if (idx > max_field) idx = 0;
        if (!task || idx != TASK_EDIT_REWARD_ITEM_QTY || task->reward_item_id > 0) {
            return (TaskEditorField)idx;
        }
    }
    return current;
}

String formatDurationValue(uint32_t total_seconds) {
    uint32_t hours = total_seconds / 3600U;
    uint32_t minutes = (total_seconds / 60U) % 60U;
    uint32_t seconds = total_seconds % 60U;
    char buf[24];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (unsigned long)hours,
             (unsigned long)minutes, (unsigned long)seconds);
    return String(buf);
}

String formatDueDateValue(uint32_t due_date) {
    if (due_date == 0) return String("No due date");
    time_t ts = (time_t)due_date;
    struct tm info;
    localtime_r(&ts, &info);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d", info.tm_mday, info.tm_mon + 1,
             info.tm_year + 1900, info.tm_hour, info.tm_min);
    return String(buf);
}

String truncateUiText(const char* text, size_t max_len) {
    if (!text || text[0] == '\0') return String("-");
    String out = String(text);
    if (out.length() <= (int)max_len) return out;
    return out.substring(0, max_len > 3 ? (int)max_len - 3 : 0) + "...";
}

String taskRewardItemLabel(const Task& task) {
    if (task.reward_item_id == 0) return String("No item");
    const ShopItem* item = shop_system.getItem(task.reward_item_id);
    if (!item) return String("Unknown item");
    return truncateUiText(item->name, 16);
}

String taskLinkedSkillsLabel(const Task& task) {
    if (task.linked_skill_count == 0) return String("None");
    if (task.linked_skill_count == 1) {
        const Skill* skill = skill_system.getSkill(task.linked_skill_ids[0]);
        if (skill) return truncateUiText(skill->name, 16);
    }
    char buf[20];
    snprintf(buf, sizeof(buf), "%d selected", task.linked_skill_count);
    return String(buf);
}

String taskStateLabel(const Task& task) {
    if (task.failed) return String("FAILED");
    if (task.completed) return String("DONE");
    return String("OPEN");
}

void drawTaskStateStamp(const Task& task, int x, int y) {
    if (!task.isTerminal()) return;
    uint16_t stamp_bg = task.failed ? 0xD8A7 : 0x3686;
    const char* label = task.failed ? "FAILED" : "DONE";
    ui_canvas.fillRoundRect(x, y, 54, 14, 3, stamp_bg);
    ui_canvas.setTextColor(WHITE, stamp_bg);
    ui_canvas.setCursor(x + 7, y + 4);
    ui_canvas.print(label);
}

void openTaskDurationOverlay(const Task& task) {
    overlay_mode = OVERLAY_TASK_DURATION;
    uint32_t total_seconds = task.duration_seconds;
    task_duration_vals[0] = (uint8_t)((total_seconds / 3600U) % 100U);
    task_duration_vals[1] = (uint8_t)((total_seconds / 60U) % 60U);
    task_duration_vals[2] = (uint8_t)(total_seconds % 60U);
    task_duration_field = 0;
}

uint8_t daysInMonth(uint16_t year, uint8_t month) {
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1 || month > 12) return 31;
    if (month != 2) return days[month - 1];
    bool leap = ((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U);
    return leap ? 29 : 28;
}

void clampTaskDueEditor() {
    uint16_t year = 2000U + task_due_vals[5];
    if (task_due_vals[4] < 1) task_due_vals[4] = 1;
    if (task_due_vals[4] > 12) task_due_vals[4] = 12;
    uint8_t max_day = daysInMonth(year, task_due_vals[4]);
    if (task_due_vals[3] < 1) task_due_vals[3] = 1;
    if (task_due_vals[3] > max_day) task_due_vals[3] = max_day;
    if (task_due_vals[0] > 23) task_due_vals[0] = 23;
    if (task_due_vals[1] > 59) task_due_vals[1] = 59;
    if (task_due_vals[2] > 59) task_due_vals[2] = 59;
}

void openTaskDueDateOverlay(const Task& task) {
    overlay_mode = OVERLAY_TASK_DUE_DATE;
    time_t ts = task.due_date > 0 ? (time_t)task.due_date : time_sync.getCurrentTime();
    struct tm info;
    localtime_r(&ts, &info);
    task_due_vals[0] = (uint8_t)info.tm_hour;
    task_due_vals[1] = (uint8_t)info.tm_min;
    task_due_vals[2] = (uint8_t)info.tm_sec;
    task_due_vals[3] = (uint8_t)info.tm_mday;
    task_due_vals[4] = (uint8_t)(info.tm_mon + 1);
    task_due_vals[5] = (uint8_t)((info.tm_year + 1900) - 2000);
    task_due_time_stage = false;
    task_due_time_field = 0;
    clampTaskDueEditor();
}

time_t buildTaskDueTimestamp() {
    struct tm info = {};
    info.tm_hour = task_due_vals[0];
    info.tm_min = task_due_vals[1];
    info.tm_sec = task_due_vals[2];
    info.tm_mday = task_due_vals[3];
    info.tm_mon = task_due_vals[4] - 1;
    info.tm_year = 100 + task_due_vals[5];
    return mktime(&info);
}

void openTaskMoneyOverlay(const Task& task) {
    overlay_mode = OVERLAY_TASK_MONEY;
    task_money_edit = (int32_t)task.reward_money;
    task_money_step_index = 2;
}

void openTaskSkillsOverlay(const Task& task) {
    overlay_mode = OVERLAY_TASK_SKILLS;
    task_skill_menu_category_index = selected_category_index;
    task_skill_menu_skill_index = 0;
    task_skill_menu_focus_skills = false;
    if (task.linked_skill_count > 0) {
        const Skill* first_skill = skill_system.getSkill(task.linked_skill_ids[0]);
        if (first_skill) {
            uint16_t cat_count = skill_system.getActiveCategoryCount();
            for (uint16_t i = 0; i < cat_count; i++) {
                const SkillCategory* cat = skill_system.getCategoryByActiveIndex(i);
                if (cat && cat->id == first_skill->category_id) {
                    task_skill_menu_category_index = i;
                    break;
                }
            }
        }
    }
}

bool taskCategoryFullySelected(const Task& task, uint16_t category_id) {
    uint16_t count = skill_system.getActiveSkillCountInCategory(category_id);
    if (count == 0) return false;
    for (uint16_t i = 0; i < count; i++) {
        const Skill* skill = skill_system.getSkillInCategoryByActiveIndex(category_id, i);
        if (!skill || !task.hasLinkedSkill(skill->id)) return false;
    }
    return true;
}

void toggleTaskCategorySelection(Task& task, uint16_t category_id) {
    bool all_selected = taskCategoryFullySelected(task, category_id);
    uint16_t count = skill_system.getActiveSkillCountInCategory(category_id);
    for (uint16_t i = 0; i < count; i++) {
        const Skill* skill = skill_system.getSkillInCategoryByActiveIndex(category_id, i);
        if (!skill) continue;
        if (all_selected) task.removeLinkedSkill(skill->id);
        else task.addLinkedSkill(category_id, skill->id);
    }
    task.syncPrimaryLinkedSkill();
}

void toggleTaskSkillSelection(Task& task, uint16_t category_id, uint16_t skill_id) {
    if (task.hasLinkedSkill(skill_id)) task.removeLinkedSkill(skill_id);
    else task.addLinkedSkill(category_id, skill_id);
    task.syncPrimaryLinkedSkill();
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
        case TASK_EDIT_DURATION:
        case TASK_EDIT_DUE_DATE:
        case TASK_EDIT_LINKED_SKILLS:
            break;
        case TASK_EDIT_REWARD_MONEY: {
            if (t.reward_item_id > 0) break;
            int32_t v = (int32_t)t.reward_money + delta * 100;
            if (v < 0) v = 0;
            t.reward_money = (uint32_t)v;
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
    uint16_t count = task_manager.getVisibleTaskCount();
    if (task_index >= count) {
        setStatus("No task at slot");
        return;
    }

    Task* t = task_manager.getVisibleTaskByIndex(task_index);
    if (!t) {
        setStatus("No task at slot");
        return;
    }
    uint32_t base_xp = task_manager.completeTask(t->id);
    if (base_xp == 0) {
        setStatus(t->failed ? "Task already failed" : "Task already done", 1400);
        return;
    }

    uint16_t start_level = level_system.getLevel();
    float health_mul = health_system.getXPMultiplier();
    uint32_t player_xp = health_system.applyHealthMultiplier(base_xp);
    uint16_t player_levels = level_system.addXPWithMultiplier(base_xp, health_mul);

    uint32_t skill_xp = 0;
    uint16_t skill_levels = 0;
    if (t->linked_skill_count > 0) {
        const GlobalSettings& gcfg = settings_system.settings();
        uint8_t ratio = gcfg.skill_xp_ratio;
        bool split = gcfg.skill_xp_split;
        // Compute per-skill XP based on ratio and optional split
        uint32_t per_skill_xp = (player_xp * ratio) / 100;
        if (split && t->linked_skill_count > 1) {
            per_skill_xp /= t->linked_skill_count;
        }
        for (uint8_t i = 0; i < t->linked_skill_count; i++) {
            uint32_t granted_xp = 0;
            // Use the ratio-adjusted XP instead of the default multiplier
            skill_levels += skill_system.addXPFromTask(t->linked_skill_ids[i], per_skill_xp, granted_xp, 1.0f);
            skill_xp += granted_xp;
        }
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

    profile_tasks_completed++;
    profile_money_gained += (int32_t)money_gain;

    saveAllProfileData();

    triggerFeedback(FEEDBACK_TASK_COMPLETE);
    if (player_levels > 0) {
        hud_display_level = start_level;
        hud_levelup_pending = player_levels;
        hud_levelup_fast = player_levels > 10;
        hud_levelup_step_ms = hud_levelup_fast ? 90 : 220;
        hud_levelup_sound_ms = hud_levelup_fast ? 55 : 140;
        hud_levelup_last_step_ms = millis();
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

void failTaskAtIndex(uint16_t task_index) {
    uint16_t count = task_manager.getVisibleTaskCount();
    if (task_index >= count) {
        setStatus("No task at slot");
        return;
    }

    Task* t = task_manager.getVisibleTaskByIndex(task_index);
    if (!t) {
        setStatus("No task at slot");
        return;
    }
    if (!task_manager.failTask(t->id)) {
        setStatus(t->completed ? "Task already done" : "Task already failed", 1400);
        return;
    }

    profile_tasks_failed++;
    saveAllProfileData();
    triggerFeedback(FEEDBACK_TASK_FAILED);
    setStatus("Task failed", 1600);
}

void cycleScreen(int8_t delta) {
    int8_t next = (int8_t)current_screen + delta;
    if (next < 0) next = UI_ALARMS;
    if (next > UI_ALARMS) next = UI_DASHBOARD;
    current_screen = (UiScreen)next;

    const char* name = "DASH";
    if (current_screen == UI_TASKS) name = "TASKS";
    else if (current_screen == UI_SKILLS) name = "SKILLS";
    else if (current_screen == UI_SHOP) name = "SHOP";
    else if (current_screen == UI_PROFILES) name = "PROF";
    else if (current_screen == UI_SAVE_LOAD) name = "SAVE";
    else if (current_screen == UI_SETTINGS) name = "SETTINGS";
    else if (current_screen == UI_HELP) name = "HELP";
    else if (current_screen == UI_ALARMS) name = "ALARMS";

    setStatus(name, 1000);
}

void handleNavCommand(NavCommand cmd) {
    // Block nav input while text input is active (e.g. during math quiz)
    if (text_input_active && overlay_mode == OVERLAY_MATH_QUIZ) return;

    // Screen selector mode (scrollable vertical list)
    if (screen_selector_active) {
        if (screen_selector_sub_active) {
            uint8_t sub_count = getSelectorSubCount(screen_selector_index);
            if (cmd == NAV_UP) {
                if (screen_selector_sub_index > 0) screen_selector_sub_index--;
                else if (sub_count > 0) screen_selector_sub_index = sub_count - 1;
            } else if (cmd == NAV_DOWN) {
                if (sub_count > 0 && screen_selector_sub_index + 1 < sub_count) screen_selector_sub_index++;
                else screen_selector_sub_index = 0;
            } else if (cmd == NAV_SELECT) {
                UiScreen new_screen = indexToScreen(screen_selector_index);
                current_screen = new_screen;
                if (screen_selector_index == 2 && sub_count > 0) { // SKILLS
                    selected_category_index = screen_selector_sub_index;
                    selected_skill_index = 0;
                } else if (screen_selector_index == 8) { // SETTINGS (index 8 in new order)
                    sett_section = (SettingsSection)screen_selector_sub_index;
                }
                const char* sub_name = getSelectorSubName(screen_selector_index, screen_selector_sub_index);
                closeScreenSelector();
                setStatus(sub_name, 800);
            } else if (cmd == NAV_BACK) {
                screen_selector_sub_active = false;
            }
        } else {
            if (cmd == NAV_UP) {
                if (screen_selector_index > 0) screen_selector_index--;
                else screen_selector_index = SCREEN_SELECTOR_COUNT - 1;
            } else if (cmd == NAV_DOWN) {
                if (screen_selector_index + 1 < SCREEN_SELECTOR_COUNT) screen_selector_index++;
                else screen_selector_index = 0;
            } else if (cmd == NAV_SELECT) {
                uint8_t sub_count = getSelectorSubCount(screen_selector_index);
                if (sub_count > 0) {
                    screen_selector_sub_active = true;
                    screen_selector_sub_index = 0;
                } else {
                    selectCurrentScreen();
                }
            } else if (cmd == NAV_BACK) {
                closeScreenSelector();
            }
        }
        return;
    }

    if (overlay_mode == OVERLAY_PROFILE_DELETE) {
        if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
            setStatus("Profile delete cancelled", 1000);
        }
        return;
    }

    // ---- Skill overlays ----
    if (overlay_mode == OVERLAY_SKILL_FIELD_MENU) {
        // Category fields: 0=Rename Cat, 1=Add Skill, 2=Delete Cat
        // Skill fields:    0=Rename Skill, 1=Edit Details, 2=Add Skill, 3=Delete Skill
        uint8_t field_count = skill_edit_is_category ? 3 : 4;
        if (cmd == NAV_UP && skill_field_menu_index > 0) skill_field_menu_index--;
        else if (cmd == NAV_DOWN && skill_field_menu_index + 1 < field_count) skill_field_menu_index++;
        else if (cmd == NAV_SELECT) {
            overlay_mode = OVERLAY_NONE;
            const SkillCategory* cat = selectedCategory();
            const Skill* sk = selectedSkillInSelectedCategory();
            if (skill_edit_is_category) {
                if (skill_field_menu_index == 0 && cat) {
                    beginTextInput(INPUT_RENAME_CATEGORY, cat->id);
                } else if (skill_field_menu_index == 1 && cat) {
                    beginTextInput(INPUT_ADD_SKILL, cat->id);
                } else if (skill_field_menu_index == 2 && cat) {
                    skill_delete_is_category = true;
                    profile_delete_confirm_until = millis() + 5000;
                    overlay_mode = OVERLAY_SKILL_DELETE_CONFIRM;
                }
            } else {
                if (skill_field_menu_index == 0 && sk) {
                    beginTextInput(INPUT_RENAME_SKILL, sk->id);
                } else if (skill_field_menu_index == 1 && sk) {
                    beginTextInput(INPUT_EDIT_SKILL_DETAILS, sk->id);
                } else if (skill_field_menu_index == 2 && cat) {
                    beginTextInput(INPUT_ADD_SKILL, cat->id);
                } else if (skill_field_menu_index == 3 && sk) {
                    skill_delete_is_category = false;
                    profile_delete_confirm_until = millis() + 5000;
                    overlay_mode = OVERLAY_SKILL_DELETE_CONFIRM;
                }
            }
        } else if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
        }
        return;
    }

    if (overlay_mode == OVERLAY_SKILL_FIELD_MENU) {
        // Category fields: 0=Rename Cat, 1=Add Category, 2=Add Skill, 3=Delete Cat
        // Skill fields:    0=Rename Skill, 1=Edit Details, 2=Add Skill, 3=Delete Skill
        // Both contexts have 4 items
        uint8_t field_count = 4;
        if (cmd == NAV_UP && skill_field_menu_index > 0) skill_field_menu_index--;
        else if (cmd == NAV_DOWN && skill_field_menu_index + 1 < field_count) skill_field_menu_index++;
        else if (cmd == NAV_SELECT) {
            overlay_mode = OVERLAY_NONE;
            const SkillCategory* cat = selectedCategory();
            const Skill* sk = selectedSkillInSelectedCategory();
            if (skill_edit_is_category) {
                if (skill_field_menu_index == 0 && cat) {
                    beginTextInput(INPUT_RENAME_CATEGORY, cat->id);
                } else if (skill_field_menu_index == 1) {
                    beginTextInput(INPUT_ADD_CATEGORY);
                } else if (skill_field_menu_index == 2 && cat) {
                    beginTextInput(INPUT_ADD_SKILL, cat->id);
                } else if (skill_field_menu_index == 3 && cat) {
                    skill_delete_is_category = true;
                    profile_delete_confirm_until = millis() + 5000;
                    overlay_mode = OVERLAY_SKILL_DELETE_CONFIRM;
                }
            } else {
                if (skill_field_menu_index == 0 && sk) {
                    beginTextInput(INPUT_RENAME_SKILL, sk->id);
                } else if (skill_field_menu_index == 1 && sk) {
                    beginTextInput(INPUT_EDIT_SKILL_DETAILS, sk->id);
                } else if (skill_field_menu_index == 2 && cat) {
                    beginTextInput(INPUT_ADD_SKILL, cat->id);
                } else if (skill_field_menu_index == 3 && sk) {
                    skill_delete_is_category = false;
                    profile_delete_confirm_until = millis() + 5000;
                    overlay_mode = OVERLAY_SKILL_DELETE_CONFIRM;
                }
            }
        } else if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
        }
        return;
    }

    if (overlay_mode == OVERLAY_SKILL_DELETE_CONFIRM) {
        if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
            setStatus("Delete cancelled", 1000);
        }
        return;
    }

    if (overlay_mode == OVERLAY_SKILL_QUICK_MENU) {
        // Edit-mode actions: 0=Add Category, 1=Add Skill (if cat), 2=Edit Skill XP (if skill), 3=Edit Main XP
        const SkillCategory* cat = selectedCategory();
        const Skill* sk = selectedSkillInSelectedCategory();
        uint8_t item_count = 4;
        if (cmd == NAV_UP && skill_quick_menu_index > 0) skill_quick_menu_index--;
        else if (cmd == NAV_DOWN && skill_quick_menu_index + 1 < item_count) skill_quick_menu_index++;
        else if (cmd == NAV_SELECT) {
            overlay_mode = OVERLAY_NONE;
            if (skill_quick_menu_index == 0) {
                beginTextInput(INPUT_ADD_CATEGORY);
            } else if (skill_quick_menu_index == 1) {
                if (cat) beginTextInput(INPUT_ADD_SKILL, cat->id);
                else setStatus("No category selected");
            } else if (skill_quick_menu_index == 2) {
                if (sk) {
                    skill_xp_edit_skill_id = sk->id;
                    skill_xp_edit_value = 0;
                    skill_xp_edit_step_index = 2;
                    overlay_mode = OVERLAY_SKILL_XP_EDIT;
                } else {
                    setStatus("No skill selected");
                }
            } else if (skill_quick_menu_index == 3) {
                // Start math quiz
                math_quiz_question = 0;
                generateMathQuizQuestion();
                beginTextInput(INPUT_MATH_QUIZ_ANSWER);
                overlay_mode = OVERLAY_MATH_QUIZ;
            }
        } else if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
        }
        return;
    }

    if (overlay_mode == OVERLAY_SKILL_XP_EDIT) {
        static const int32_t xp_steps[5] = {1, 10, 100, 1000, 10000};
        int32_t step = xp_steps[skill_xp_edit_step_index];
        if (cmd == NAV_UP) skill_xp_edit_value += step;
        else if (cmd == NAV_DOWN) skill_xp_edit_value -= step;
        else if (cmd == NAV_LEFT && skill_xp_edit_step_index > 0) skill_xp_edit_step_index--;
        else if (cmd == NAV_RIGHT && skill_xp_edit_step_index < 4) skill_xp_edit_step_index++;
        else if (cmd == NAV_SELECT) {
            if (skill_xp_edit_skill_id > 0) {
                Skill* sk = skill_system.getSkill(skill_xp_edit_skill_id);
                if (sk) {
                    if (skill_xp_edit_value >= 0) {
                        skill_system.addXPToSkill(sk->id, (uint32_t)skill_xp_edit_value);
                    } else {
                        uint32_t remove_amt = (uint32_t)(-skill_xp_edit_value);
                        sk->current_xp = (sk->current_xp >= remove_amt) ? sk->current_xp - remove_amt : 0;
                        // Recompute level from remaining current_xp; preserve lifetime_xp (removal must not inflate it)
                        uint32_t saved_lifetime = sk->lifetime_xp;
                        uint32_t remaining = sk->current_xp;
                        sk->level = 1;
                        sk->current_xp = 0;
                        if (remaining > 0) skill_system.addXPToSkill(sk->id, remaining);
                        sk->lifetime_xp = saved_lifetime;
                    }
                    saveSkills();
                    setStatus("Skill XP updated");
                }
            }
            overlay_mode = OVERLAY_NONE;
        } else if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
        }
        return;
    }

    if (overlay_mode == OVERLAY_MAIN_XP_EDIT) {
        static const int32_t mxp_steps[5] = {1, 10, 100, 1000, 10000};
        int32_t step = mxp_steps[skill_xp_edit_step_index];
        if (cmd == NAV_UP) skill_xp_edit_value += step;
        else if (cmd == NAV_DOWN) skill_xp_edit_value -= step;
        else if (cmd == NAV_LEFT && skill_xp_edit_step_index > 0) skill_xp_edit_step_index--;
        else if (cmd == NAV_RIGHT && skill_xp_edit_step_index < 4) skill_xp_edit_step_index++;
        else if (cmd == NAV_SELECT) {
            if (skill_xp_edit_value >= 0) {
                level_system.addXP((uint32_t)skill_xp_edit_value);
            } else {
                uint32_t remove_amt = (uint32_t)(-skill_xp_edit_value);
                uint32_t cur_xp = level_system.getCurrentXP();
                level_system.setCurrentXP(cur_xp >= remove_amt ? cur_xp - remove_amt : 0);
            }
            hud_levelup_pending = 0;
            hud_display_level = level_system.getLevel();
            hud_level_anim = (float)level_system.getXPProgress();
            saveAllProfileData();
            setStatus("Main XP updated");
            overlay_mode = OVERLAY_NONE;
        } else if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
        }
        return;
    }

    if (overlay_mode == OVERLAY_MATH_QUIZ) {
        // handled in handleKeyInput/finishTextInput
        return;
    }

    if (overlay_mode == OVERLAY_ALARM_SETUP) {
        if (alarm_setup_entry_index >= alarm_count) { overlay_mode = OVERLAY_NONE; return; }
        AlarmEntry& ae = alarm_entries[alarm_setup_entry_index];
        // Fields: 0=Name, 1=Hour, 2=Minute, 3=Enabled
        if (cmd == NAV_UP) {
            if (alarm_setup_field == 1) ae.hour = (ae.hour + 23) % 24;
            else if (alarm_setup_field == 2) ae.minute = (ae.minute + 59) % 60;
            else if (alarm_setup_field == 3) ae.enabled = !ae.enabled;
        } else if (cmd == NAV_DOWN) {
            if (alarm_setup_field == 1) ae.hour = (ae.hour + 1) % 24;
            else if (alarm_setup_field == 2) ae.minute = (ae.minute + 1) % 60;
            else if (alarm_setup_field == 3) ae.enabled = !ae.enabled;
        } else if (cmd == NAV_LEFT && alarm_setup_field > 0) {
            alarm_setup_field--;
        } else if (cmd == NAV_RIGHT && alarm_setup_field < 3) {
            alarm_setup_field++;
        } else if (cmd == NAV_SELECT) {
            if (alarm_setup_field == 0) {
                beginTextInput(INPUT_ALARM_NAME, alarm_setup_entry_index);
                overlay_mode = OVERLAY_ALARM_SETUP;  // reopen after input
            } else {
                save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
                setStatus("Alarm saved", 800);
                overlay_mode = OVERLAY_NONE;
            }
        } else if (cmd == NAV_BACK) {
            save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
            overlay_mode = OVERLAY_NONE;
        }
        return;
    }

    if (overlay_mode == OVERLAY_TIMER_SETUP) {
        if (alarm_setup_entry_index >= timer_count) { overlay_mode = OVERLAY_NONE; return; }
        TimerEntry& te = timer_entries[alarm_setup_entry_index];
        uint32_t h = te.duration_seconds / 3600;
        uint32_t m = (te.duration_seconds % 3600) / 60;
        uint32_t s = te.duration_seconds % 60;
        // Fields: 0=Name, 1=Hours, 2=Minutes, 3=Seconds
        bool changed = false;
        if (cmd == NAV_UP) {
            if (alarm_setup_field == 1) { if (h < 99) { h++; changed = true; } }
            else if (alarm_setup_field == 2) { if (m < 59) { m++; changed = true; } else if (h < 99) { m = 0; h++; changed = true; } }
            else if (alarm_setup_field == 3) { if (s < 59) { s++; changed = true; } else if (m < 59) { s = 0; m++; changed = true; } else if (h < 99) { s = 0; m = 0; h++; changed = true; } }
        } else if (cmd == NAV_DOWN) {
            if (alarm_setup_field == 1) { if (h > 0) { h--; changed = true; } }
            else if (alarm_setup_field == 2) { if (m > 0) { m--; changed = true; } else if (h > 0) { m = 59; h--; changed = true; } }
            else if (alarm_setup_field == 3) { if (s > 0) { s--; changed = true; } else if (m > 0) { s = 59; m--; changed = true; } else if (h > 0) { s = 59; m = 59; h--; changed = true; } }
        } else if (cmd == NAV_LEFT && alarm_setup_field > 0) {
            alarm_setup_field--;
        } else if (cmd == NAV_RIGHT && alarm_setup_field < 3) {
            alarm_setup_field++;
        } else if (cmd == NAV_SELECT) {
            if (alarm_setup_field == 0) {
                beginTextInput(INPUT_TIMER_NAME, alarm_setup_entry_index);
                overlay_mode = OVERLAY_TIMER_SETUP;
            } else {
                te.duration_seconds = h * 3600 + m * 60 + s;
                save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
                setStatus("Timer saved", 800);
                overlay_mode = OVERLAY_NONE;
            }
            return;
        } else if (cmd == NAV_BACK) {
            te.duration_seconds = h * 3600 + m * 60 + s;
            save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
            overlay_mode = OVERLAY_NONE;
            return;
        }
        if (changed) te.duration_seconds = h * 3600 + m * 60 + s;
        return;
    }

    // ---- Shop overlays ----
    if (overlay_mode == OVERLAY_SHOP_FIELD_MENU) {
        // Build field count dynamically
        uint8_t field_count = shop_edit_is_recipe ? 10 : 9;
        if (cmd == NAV_UP && shop_field_menu_index > 0) shop_field_menu_index--;
        else if (cmd == NAV_DOWN && shop_field_menu_index + 1 < field_count) shop_field_menu_index++;
        else if (cmd == NAV_SELECT) {
            uint8_t sel = shop_field_menu_index;
            overlay_mode = OVERLAY_NONE;
            if (!shop_edit_is_recipe) {
                // Item fields: 0=Name, 1=Description, 2=Effect Type, 3=Effect Value, 4=BasePrice, 5=BuyLimit, 6=Cooldown, 7=AddNew, 8=Remove
                ShopItem* it = shop_system.getItemByActiveIndex(selected_shop_item_index);
                if (sel == 0 && it) { beginTextInput(INPUT_SHOP_ITEM_NAME, it->id); }
                else if (sel == 1 && it) { beginTextInput(INPUT_SHOP_ITEM_DESC, it->id); }
                else if (sel == 2 && it) {
                    shop_effect_pick_index = (uint8_t)it->effect_type;
                    overlay_mode = OVERLAY_SHOP_EFFECT_PICK;
                } else if (sel == 3 && it) {
                    shop_num_edit_value = (int32_t)it->effect_value;
                    shop_num_edit_step_index = 2;
                    shop_num_edit_target = SHOP_NUM_EFFECT_VALUE;
                    overlay_mode = OVERLAY_SHOP_PRICE_EDIT;
                } else if (sel == 4 && it) {
                    shop_num_edit_value = it->base_price;
                    shop_num_edit_step_index = 2;
                    shop_num_edit_target = SHOP_NUM_BASE_PRICE;
                    overlay_mode = OVERLAY_SHOP_PRICE_EDIT;
                } else if (sel == 5 && it) {
                    shop_num_edit_value = it->buy_limit;
                    shop_num_edit_step_index = 0;
                    shop_num_edit_target = SHOP_NUM_BUY_LIMIT;
                    overlay_mode = OVERLAY_SHOP_PRICE_EDIT;
                } else if (sel == 6 && it) {
                    shop_num_edit_value = (int32_t)it->cooldown_seconds;
                    shop_num_edit_step_index = 1;
                    shop_num_edit_target = SHOP_NUM_COOLDOWN;
                    overlay_mode = OVERLAY_SHOP_PRICE_EDIT;
                } else if (sel == 7) {
                    beginTextInput(INPUT_NEW_SHOP_ITEM_NAME);
                } else if (sel == 8 && it) {
                    shop_confirm_is_recipe = false;
                    overlay_mode = OVERLAY_SHOP_CONFIRM_DELETE;
                    profile_delete_confirm_until = millis() + 5000;
                }
            } else {
                // Recipe fields: 0=Add,1=Remove,2=In1 item,3=In1 qty,4=In2 item,5=In2 qty,6=In3 item,7=In3 qty,8=Out item,9=Out qty
                CraftRecipe* rc = shop_system.getRecipeByActiveIndex(selected_shop_recipe_index);
                if (sel == 0) {
                    // Add recipe directly from edit mode.
                    uint16_t active_items = shop_system.getActiveItemCount();
                    if (active_items < 2) {
                        setStatus("Need at least 2 items");
                        return;
                    }
                    const ShopItem* input_item = selectedShopItem();
                    if (!input_item) input_item = shop_system.getItemByActiveIndex(0);
                    const ShopItem* output_item = shop_system.getItemByActiveIndex(0);
                    for (uint16_t i = 0; i < active_items; i++) {
                        const ShopItem* cand = shop_system.getItemByActiveIndex(i);
                        if (cand && input_item && cand->id != input_item->id) {
                            output_item = cand;
                            break;
                        }
                    }
                    if (!input_item || !output_item || input_item->id == output_item->id) {
                        setStatus("Recipe add failed");
                        return;
                    }
                    CraftIngredient in[1];
                    in[0].item_id = input_item->id;
                    in[0].quantity = 2;  // valid 1-input recipe
                    if (shop_system.addRecipe(in, 1, output_item->id, 1) > 0) {
                        normalizeShopSelection();
                        uint16_t rc_count = shop_system.getActiveRecipeCount();
                        if (rc_count > 0) selected_shop_recipe_index = rc_count - 1;
                        saveShopData();
                        setStatus("Recipe added");
                    } else {
                        setStatus("Recipe add failed");
                    }
                    return;
                }
                if (!rc) {
                    setStatus("No recipe. Add first");
                    return;
                }
                if (sel == 1) {
                    shop_confirm_is_recipe = true;
                    overlay_mode = OVERLAY_SHOP_CONFIRM_DELETE;
                    profile_delete_confirm_until = millis() + 5000;
                    return;
                }
                if (sel >= 2 && sel <= 7 && sel % 2 == 0) {  // slot item: 2,4,6
                    uint8_t slot = (sel - 2) / 2;
                    shop_recipe_pick_slot = slot;
                    shop_recipe_pick_item_index = 0;
                    overlay_mode = OVERLAY_SHOP_RECIPE_ITEM_PICK;
                } else if (sel >= 3 && sel <= 7 && sel % 2 == 1) {  // slot qty: 3,5,7
                    uint8_t slot = (sel - 3) / 2;
                    shop_num_edit_value = rc->inputs[slot].quantity;
                    shop_num_edit_step_index = 0;
                    shop_num_edit_target = SHOP_NUM_RECIPE_IN_QTY;
                    shop_recipe_pick_slot = slot;
                    overlay_mode = OVERLAY_SHOP_PRICE_EDIT;
                } else if (sel == 8) {
                    shop_recipe_pick_slot = 3; // output
                    shop_recipe_pick_item_index = 0;
                    overlay_mode = OVERLAY_SHOP_RECIPE_ITEM_PICK;
                } else if (sel == 9) {
                    shop_num_edit_value = rc->output_quantity;
                    shop_num_edit_step_index = 0;
                    shop_num_edit_target = SHOP_NUM_RECIPE_OUT_QTY;
                    overlay_mode = OVERLAY_SHOP_PRICE_EDIT;
                }
            }
        } else if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
        }
        return;
    }

    if (overlay_mode == OVERLAY_SHOP_EFFECT_PICK) {
        static const uint8_t EFFECT_COUNT = 5;
        if (cmd == NAV_UP && shop_effect_pick_index > 0) shop_effect_pick_index--;
        else if (cmd == NAV_DOWN && shop_effect_pick_index + 1 < EFFECT_COUNT) shop_effect_pick_index++;
        else if (cmd == NAV_SELECT) {
            ShopItem* it = shop_system.getItemByActiveIndex(selected_shop_item_index);
            if (it) {
                it->effect_type = (ItemEffectType)shop_effect_pick_index;
                if (it->effect_type == ITEM_EFFECT_NONE || it->effect_type == ITEM_EFFECT_CONSUME_REMOVE) {
                    it->collectible_only = (it->effect_type == ITEM_EFFECT_NONE);
                } else {
                    it->collectible_only = false;
                }
                saveShopData();
                setStatus("Effect updated");
            }
            overlay_mode = OVERLAY_NONE;
        } else if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
        }
        return;
    }

    if (overlay_mode == OVERLAY_SHOP_PRICE_EDIT) {
        static const int32_t price_steps[5] = {1, 10, 100, 1000, 10000};
        int32_t step = price_steps[shop_num_edit_step_index];
        if (cmd == NAV_UP) shop_num_edit_value += step;
        else if (cmd == NAV_DOWN) { shop_num_edit_value -= step; if (shop_num_edit_value < 0) shop_num_edit_value = 0; }
        else if (cmd == NAV_LEFT && shop_num_edit_step_index > 0) shop_num_edit_step_index--;
        else if (cmd == NAV_RIGHT && shop_num_edit_step_index < 4) shop_num_edit_step_index++;
        else if (cmd == NAV_SELECT) {
            if (shop_num_edit_target == SHOP_NUM_EFFECT_VALUE) {
                ShopItem* it = shop_system.getItemByActiveIndex(selected_shop_item_index);
                if (it) it->effect_value = (uint32_t)shop_num_edit_value;
            } else if (shop_num_edit_target == SHOP_NUM_BASE_PRICE) {
                ShopItem* it = shop_system.getItemByActiveIndex(selected_shop_item_index);
                if (it) { it->base_price = shop_num_edit_value; it->current_price = shop_num_edit_value; }
            } else if (shop_num_edit_target == SHOP_NUM_BUY_LIMIT) {
                ShopItem* it = shop_system.getItemByActiveIndex(selected_shop_item_index);
                if (it) it->buy_limit = (uint16_t)shop_num_edit_value;
            } else if (shop_num_edit_target == SHOP_NUM_COOLDOWN) {
                ShopItem* it = shop_system.getItemByActiveIndex(selected_shop_item_index);
                if (it) it->cooldown_seconds = (uint32_t)shop_num_edit_value;
            } else if (shop_num_edit_target == SHOP_NUM_RECIPE_IN_QTY) {
                CraftRecipe* rc = shop_system.getRecipeByActiveIndex(selected_shop_recipe_index);
                if (rc && shop_recipe_pick_slot < 3) rc->inputs[shop_recipe_pick_slot].quantity = (uint8_t)shop_num_edit_value;
            } else if (shop_num_edit_target == SHOP_NUM_RECIPE_OUT_QTY) {
                CraftRecipe* rc = shop_system.getRecipeByActiveIndex(selected_shop_recipe_index);
                if (rc) rc->output_quantity = (uint8_t)shop_num_edit_value;
            }
            saveShopData();
            setStatus("Value updated");
            overlay_mode = OVERLAY_NONE;
        } else if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
        }
        return;
    }

    if (overlay_mode == OVERLAY_SHOP_RECIPE_ITEM_PICK) {
        uint16_t item_count = shop_system.getActiveItemCount();
        if (cmd == NAV_UP && shop_recipe_pick_item_index > 0) shop_recipe_pick_item_index--;
        else if (cmd == NAV_DOWN && shop_recipe_pick_item_index + 1 < item_count) shop_recipe_pick_item_index++;
        else if (cmd == NAV_SELECT) {
            const ShopItem* picked = shop_system.getItemByActiveIndex(shop_recipe_pick_item_index);
            if (picked) {
                CraftRecipe* rc = shop_system.getRecipeByActiveIndex(selected_shop_recipe_index);
                if (rc) {
                    // Always allow selection; check for conflict after setting
                    if (shop_recipe_pick_slot == 3) {
                        rc->output_item_id = picked->id;
                    } else {
                        rc->inputs[shop_recipe_pick_slot].item_id = picked->id;
                        if (rc->inputs[shop_recipe_pick_slot].quantity == 0)
                            rc->inputs[shop_recipe_pick_slot].quantity = 1;
                        if (shop_recipe_pick_slot + 1 > rc->input_count)
                            rc->input_count = shop_recipe_pick_slot + 1;
                    }
                    // Check for conflict: any input matches output
                    bool conflict = false;
                    if (rc->output_item_id != 0) {
                        for (uint8_t s = 0; s < rc->input_count; s++) {
                            if (rc->inputs[s].item_id == rc->output_item_id) { conflict = true; break; }
                        }
                    }
                    if (conflict) {
                        setStatus("Conflict: input=output (fix before saving)", 2000);
                        // Do not save while conflict exists
                    } else {
                        saveShopData();
                        setStatus(shop_recipe_pick_slot == 3 ? "Output item set" : "Ingredient set");
                    }
                }
            }
            overlay_mode = OVERLAY_NONE;
        } else if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
        }
        return;
    }

    if (overlay_mode == OVERLAY_SHOP_CONFIRM_DELETE) {
        if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
            setStatus("Delete cancelled", 1000);
        }
        return;
    }
    // ---- End shop overlays ----

    if (overlay_mode == OVERLAY_TASK_DURATION) {
        Task* t = selectedTaskMutable();
        if (!t) {
            overlay_mode = OVERLAY_NONE;
            return;
        }
        if (cmd == NAV_LEFT && task_duration_field > 0) task_duration_field--;
        else if (cmd == NAV_RIGHT && task_duration_field < 2) task_duration_field++;
        else if (cmd == NAV_UP) {
            if (task_duration_field == 0 && task_duration_vals[0] < 99) task_duration_vals[0]++;
            else if (task_duration_field > 0 && task_duration_vals[task_duration_field] < 59) task_duration_vals[task_duration_field]++;
        } else if (cmd == NAV_DOWN) {
            if (task_duration_vals[task_duration_field] > 0) task_duration_vals[task_duration_field]--;
        } else if (cmd == NAV_SELECT) {
            t->duration_seconds = (uint32_t)task_duration_vals[0] * 3600U + (uint32_t)task_duration_vals[1] * 60U + task_duration_vals[2];
            saveAllProfileData();
            overlay_mode = OVERLAY_NONE;
            setStatus("Task duration set", 1200);
        } else if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
            setStatus("Duration edit cancelled", 1000);
        }
        return;
    }

    if (overlay_mode == OVERLAY_TASK_MONEY) {
        Task* t = selectedTaskMutable();
        static const int32_t steps[5] = {1, 10, 100, 1000, 10000};
        if (!t) {
            overlay_mode = OVERLAY_NONE;
            return;
        }
        if (cmd == NAV_LEFT && task_money_step_index > 0) task_money_step_index--;
        else if (cmd == NAV_RIGHT && task_money_step_index < 4) task_money_step_index++;
        else if (cmd == NAV_UP) task_money_edit += steps[task_money_step_index];
        else if (cmd == NAV_DOWN) {
            task_money_edit -= steps[task_money_step_index];
            if (task_money_edit < 0) task_money_edit = 0;
        } else if (cmd == NAV_SELECT) {
            t->reward_money = (uint32_t)task_money_edit;
            if (t->reward_item_id == 0) {
                saveAllProfileData();
                setStatus("Reward money set", 1200);
            }
            overlay_mode = OVERLAY_NONE;
        } else if (cmd == NAV_BACK) {
            overlay_mode = OVERLAY_NONE;
            setStatus("Money edit cancelled", 1000);
        }
        return;
    }

    if (overlay_mode == OVERLAY_TASK_DUE_DATE) {
        Task* t = selectedTaskMutable();
        if (!t) {
            overlay_mode = OVERLAY_NONE;
            return;
        }
        if (!task_due_time_stage) {
            time_t ts = buildTaskDueTimestamp();
            if (cmd == NAV_LEFT) ts -= 86400;
            else if (cmd == NAV_RIGHT) ts += 86400;
            else if (cmd == NAV_UP) ts -= 7 * 86400;
            else if (cmd == NAV_DOWN) ts += 7 * 86400;
            else if (cmd == NAV_SELECT) {
                task_due_time_stage = true;
                return;
            } else if (cmd == NAV_BACK) {
                overlay_mode = OVERLAY_NONE;
                setStatus("Due date edit cancelled", 1000);
                return;
            }
            struct tm info;
            localtime_r(&ts, &info);
            task_due_vals[3] = (uint8_t)info.tm_mday;
            task_due_vals[4] = (uint8_t)(info.tm_mon + 1);
            task_due_vals[5] = (uint8_t)((info.tm_year + 1900) - 2000);
        } else {
            if (cmd == NAV_LEFT && task_due_time_field > 0) task_due_time_field--;
            else if (cmd == NAV_RIGHT && task_due_time_field < 2) task_due_time_field++;
            else if (cmd == NAV_UP) {
                uint8_t limits[3] = {23, 59, 59};
                if (task_due_vals[task_due_time_field] < limits[task_due_time_field]) task_due_vals[task_due_time_field]++;
                else task_due_vals[task_due_time_field] = 0;
            } else if (cmd == NAV_DOWN) {
                uint8_t limits[3] = {23, 59, 59};
                if (task_due_vals[task_due_time_field] > 0) task_due_vals[task_due_time_field]--;
                else task_due_vals[task_due_time_field] = limits[task_due_time_field];
            } else if (cmd == NAV_SELECT) {
                t->due_date = (uint32_t)buildTaskDueTimestamp();
                saveAllProfileData();
                overlay_mode = OVERLAY_NONE;
                setStatus("Task due date set", 1200);
            } else if (cmd == NAV_BACK) {
                task_due_time_stage = false;
            }
        }
        clampTaskDueEditor();
        return;
    }

    if (overlay_mode == OVERLAY_TASK_SKILLS) {
        Task* t = selectedTaskMutable();
        if (!t) {
            overlay_mode = OVERLAY_NONE;
            return;
        }
        uint16_t cat_count = skill_system.getActiveCategoryCount();
        const SkillCategory* cat = skill_system.getCategoryByActiveIndex(task_skill_menu_category_index);
        uint16_t skill_count = cat ? skill_system.getActiveSkillCountInCategory(cat->id) : 0;
        if (cmd == NAV_LEFT) task_skill_menu_focus_skills = false;
        else if (cmd == NAV_RIGHT && cat) task_skill_menu_focus_skills = true;
        else if (cmd == NAV_UP) {
            if (!task_skill_menu_focus_skills) {
                if (task_skill_menu_category_index > 0) task_skill_menu_category_index--;
                else if (cat_count > 0) task_skill_menu_category_index = cat_count - 1;
                task_skill_menu_skill_index = 0;
            } else if (task_skill_menu_skill_index > 0) {
                task_skill_menu_skill_index--;
            }
        } else if (cmd == NAV_DOWN) {
            if (!task_skill_menu_focus_skills) {
                if (task_skill_menu_category_index + 1 < cat_count) task_skill_menu_category_index++;
                else task_skill_menu_category_index = 0;
                task_skill_menu_skill_index = 0;
            } else if (task_skill_menu_skill_index + 1 < skill_count) {
                task_skill_menu_skill_index++;
            }
        } else if (cmd == NAV_SELECT) {
            cat = skill_system.getCategoryByActiveIndex(task_skill_menu_category_index);
            if (cat) {
                if (!task_skill_menu_focus_skills) {
                    toggleTaskCategorySelection(*t, cat->id);
                } else {
                    const Skill* skill = skill_system.getSkillInCategoryByActiveIndex(cat->id, task_skill_menu_skill_index);
                    if (skill) toggleTaskSkillSelection(*t, cat->id, skill->id);
                }
            }
        } else if (cmd == NAV_BACK) {
            saveAllProfileData();
            overlay_mode = OVERLAY_NONE;
            setStatus("Task skills updated", 1200);
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
                uint16_t count = task_manager.getVisibleTaskCount();
                if (selected_task_index + 1 < count) selected_task_index++;
            } else if (cmd == NAV_SELECT) {
                completeTaskAtIndex(selected_task_index);
            }
            break;

        case UI_TASKS: {
            normalizeTaskSelection();
            Task* t = selectedTaskMutable();
            if (!task_edit_mode) {
                if (cmd == NAV_UP && selected_task_index > 0) selected_task_index--;
                else if (cmd == NAV_DOWN) {
                    uint16_t count = task_manager.getVisibleTaskCount();
                    if (selected_task_index + 1 < count) selected_task_index++;
                } else if (cmd == NAV_SELECT) {
                    completeTaskAtIndex(selected_task_index);
                }
            } else {
                if (cmd == NAV_LEFT) {
                    selected_task_field = nextTaskField(t, selected_task_field, -1);
                } else if (cmd == NAV_RIGHT) {
                    selected_task_field = nextTaskField(t, selected_task_field, 1);
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
            if (skill_edit_mode) {
                if (cmd == NAV_LEFT) {
                    if (selected_category_index > 0) { selected_category_index--; selected_skill_index = 0; }
                } else if (cmd == NAV_RIGHT) {
                    if (selected_category_index + 1 < skill_system.getActiveCategoryCount()) { selected_category_index++; selected_skill_index = 0; }
                } else if (cmd == NAV_UP) {
                    if (selected_skill_index > 0) selected_skill_index--;
                } else if (cmd == NAV_DOWN) {
                    if (cat && selected_skill_index + 1 < skill_system.getActiveSkillCountInCategory(cat->id)) selected_skill_index++;
                } else if (cmd == NAV_SELECT) {
                    skill_edit_is_category = (skill_system.getActiveSkillCountInCategory(cat ? cat->id : 0) == 0 || selected_skill_index == 0xFFFF);
                    skill_field_menu_index = 0;
                    overlay_mode = OVERLAY_SKILL_FIELD_MENU;
                }
            } else {
                if (cmd == NAV_LEFT) {
                    if (selected_category_index > 0) { selected_category_index--; selected_skill_index = 0; }
                } else if (cmd == NAV_RIGHT) {
                    if (selected_category_index + 1 < skill_system.getActiveCategoryCount()) { selected_category_index++; selected_skill_index = 0; }
                } else if (cmd == NAV_UP) {
                    if (selected_skill_index > 0) selected_skill_index--;
                } else if (cmd == NAV_DOWN) {
                    if (cat && selected_skill_index + 1 < skill_system.getActiveSkillCountInCategory(cat->id)) selected_skill_index++;
                }
            }
            break;
        }

        case UI_SHOP:
            normalizeShopSelection();
            if (cmd == NAV_LEFT) {
                if (!shop_focus_items) shop_focus_items = true;
            } else if (cmd == NAV_RIGHT) {
                if (shop_focus_items) shop_focus_items = false;
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
                if (shop_edit_mode) {
                    shop_edit_is_recipe = !shop_focus_items;
                    shop_field_menu_index = 0;
                    overlay_mode = OVERLAY_SHOP_FIELD_MENU;
                    return;
                }
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
                if (shop_edit_mode) {
                    shop_edit_mode = false;
                    setStatus("Edit mode OFF", 1000);
                }
            }
            break;

        case UI_PROFILES:
            refreshProfiles();
            if (cmd == NAV_UP && selected_profile_index > 0) selected_profile_index--;
            else if (cmd == NAV_DOWN && selected_profile_index + 1 < profile_count) selected_profile_index++;
            else if (cmd == NAV_SELECT) {
                if (profile_count > 0 && save_load_system.setActiveProfile(profile_list[selected_profile_index].name,
                                                                            profile_list[selected_profile_index].description)) {
                    loadAllProfileData();
                    setStatus("Profile switched");
                }
            }
            break;

        case UI_INVENTORY: {
            uint16_t inv_count = 0;
            for (uint16_t i = 0; i < shop_system.getItemCountRaw(); i++) {
                const ShopItem* it = shop_system.itemsRaw() + i;
                if (it->active && shop_system.getInventoryQuantity(it->id) > 0) inv_count++;
            }
            if (cmd == NAV_UP && selected_inventory_item_index > 0) selected_inventory_item_index--;
            else if (cmd == NAV_DOWN && selected_inventory_item_index + 1 < inv_count) selected_inventory_item_index++;
            else if (cmd == NAV_SELECT) {
                // Use the selected inventory item
                uint16_t found = 0;
                for (uint16_t i = 0; i < shop_system.getItemCountRaw(); i++) {
                    const ShopItem* it = shop_system.itemsRaw() + i;
                    if (!it->active || shop_system.getInventoryQuantity(it->id) == 0) continue;
                    if (found == selected_inventory_item_index) {
                        uint32_t xp_before = level_system.getLifetimeXP();
                        if (shop_system.useItem(it->id, level_system, millis() / 1000)) {
                            saveAllProfileData();
                            uint32_t xp_gained = level_system.getLifetimeXP() - xp_before;
                            if (xp_gained > 0) {
                                char msg[40];
                                snprintf(msg, sizeof(msg), "Item used +%luXP", (unsigned long)xp_gained);
                                setStatus(msg, 2000);
                            } else {
                                setStatus("Item used");
                            }
                        } else {
                            setStatus("Cannot use item");
                        }
                        break;
                    }
                    found++;
                }
            }
            break;
        }

        case UI_SAVE_LOAD:
            if (cmd == NAV_UP && save_menu_index > 0) save_menu_index--;
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
            // WiFi section handles LEFT/RIGHT for its own focus toggle vs section nav
            if (sett_section == SETT_WIFI && (cmd == NAV_LEFT || cmd == NAV_UP || cmd == NAV_DOWN || cmd == NAV_SELECT || cmd == NAV_BACK)) {
                if (cmd == NAV_LEFT) {
                    sett_wifi_focus_saved = !sett_wifi_focus_saved;
                    sett_wifi_saved_sel = 0;
                    sett_wifi_sel = 0;
                } else if (cmd == NAV_UP) {
                    if (sett_wifi_focus_saved) {
                        if (sett_wifi_saved_sel > 0) sett_wifi_saved_sel--;
                    } else {
                        if (sett_wifi_sel > 0) sett_wifi_sel--;
                    }
                } else if (cmd == NAV_DOWN) {
                    if (sett_wifi_focus_saved) {
                        uint8_t cnt = settings_system.getSavedWiFiCount();
                        if (sett_wifi_saved_sel + 1 < cnt) sett_wifi_saved_sel++;
                    } else {
                        if (sett_wifi_sel + 1 < settings_system.getScannedCount()) sett_wifi_sel++;
                    }
                } else if (cmd == NAV_SELECT) {
                    if (sett_wifi_focus_saved) {
                        const SavedWiFiCred* cred = settings_system.getSavedWiFi(sett_wifi_saved_sel);
                        if (cred) {
                            setStatus("Connecting...", 8000);
                            if (settings_system.connectWiFi(cred->ssid, cred->password, 8000)) {
                                save_load_system.saveGlobalConfig(settings_system.settings());
                                if (time_sync.syncWithWiFi(cred->ssid, cred->password, 5000)) {
                                    #if defined(ARDUINO)
                                    time_t now = time_sync.getCurrentTime();
                                    struct tm ti; localtime_r(&now, &ti);
                                    m5::rtc_datetime_t dt;
                                    dt.date.year = ti.tm_year + 1900; dt.date.month = ti.tm_mon + 1; dt.date.date = ti.tm_mday;
                                    dt.time.hours = ti.tm_hour; dt.time.minutes = ti.tm_min; dt.time.seconds = ti.tm_sec;
                                    M5.Rtc.setDateTime(dt);
                                    #endif
                                    setStatus("WiFi+NTP OK");
                                } else { setStatus("WiFi OK, NTP fail"); }
                            } else { setStatus("WiFi failed"); }
                        }
                    } else {
                        const WiFiNetwork* net = settings_system.getScannedNetwork(sett_wifi_sel);
                        if (net) {
                            if (net->has_password) {
                                text_input_len = 0; text_input_buffer[0] = '\0';
                                for (uint8_t i = 0; i < settings_system.getSavedWiFiCount(); i++) {
                                    const SavedWiFiCred* c = settings_system.getSavedWiFi(i);
                                    if (c && strcmp(c->ssid, net->ssid) == 0 && c->password[0] != '\0') {
                                        strncpy(text_input_buffer, c->password, sizeof(text_input_buffer) - 1);
                                        text_input_len = strlen(text_input_buffer); break;
                                    }
                                }
                                beginTextInput(INPUT_WIFI_PASSWORD, sett_wifi_sel);
                            } else {
                                setStatus("Connecting...", 8000);
                                if (settings_system.connectWiFi(net->ssid, "", 8000)) {
                                    save_load_system.saveGlobalConfig(settings_system.settings());
                                    if (time_sync.syncWithWiFi(net->ssid, "", 5000)) {
                                        #if defined(ARDUINO)
                                        time_t now2 = time_sync.getCurrentTime();
                                        struct tm ti2; localtime_r(&now2, &ti2);
                                        m5::rtc_datetime_t dt2;
                                        dt2.date.year = ti2.tm_year + 1900; dt2.date.month = ti2.tm_mon + 1; dt2.date.date = ti2.tm_mday;
                                        dt2.time.hours = ti2.tm_hour; dt2.time.minutes = ti2.tm_min; dt2.time.seconds = ti2.tm_sec;
                                        M5.Rtc.setDateTime(dt2);
                                        #endif
                                        setStatus("WiFi+NTP OK");
                                    } else { setStatus("WiFi OK, NTP fail"); }
                                } else { setStatus("WiFi failed"); }
                            }
                        }
                    }
                } else if (cmd == NAV_BACK) {
                    if (sett_wifi_focus_saved) {
                        if (settings_system.removeSavedWiFi(sett_wifi_saved_sel)) {
                            save_load_system.saveGlobalConfig(settings_system.settings());
                            if (sett_wifi_saved_sel > 0 && sett_wifi_saved_sel >= settings_system.getSavedWiFiCount())
                                sett_wifi_saved_sel--;
                            setStatus("Saved WiFi removed", 1500);
                        }
                    } else {
                        setStatus("Scanning WiFi...", 3000);
                        settings_system.scanNetworks();
                        sett_wifi_sel = 0;
                        char buf[48]; snprintf(buf, sizeof(buf), "Found %d networks", settings_system.getScannedCount());
                        setStatus(buf, 2500);
                    }
                }
            } else if (cmd == NAV_LEFT) {
                // Section switch left
                int s = (int)sett_section - 1;
                if (s >= 0) sett_section = (SettingsSection)s;
            } else if (cmd == NAV_RIGHT) {
                int s = (int)sett_section + 1;
                if (s < SETT_SECTION_COUNT) sett_section = (SettingsSection)s;
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
                } else if (cmd == NAV_BACK) {
                    cfg.date_format_us = !cfg.date_format_us;
                    time_sync.setDateFormatUS(cfg.date_format_us);
                    save_load_system.saveGlobalConfig(cfg);
                    setStatus(cfg.date_format_us ? "Date MM/DD" : "Date DD/MM", 1200);
                } else if (cmd == NAV_SELECT) {
                    // Re-sync NTP using currently connected WiFi or first saved credential
                    const char* ssid_s = settings_system.isWiFiConnected() ?
                        settings_system.getConnectedSSID() : nullptr;
                    const char* pass_s = "";
                    if (ssid_s) {
                        for (uint8_t i = 0; i < settings_system.getSavedWiFiCount(); i++) {
                            const SavedWiFiCred* c = settings_system.getSavedWiFi(i);
                            if (c && strcmp(c->ssid, ssid_s) == 0) { pass_s = c->password; break; }
                        }
                    } else if (settings_system.getSavedWiFiCount() > 0) {
                        const SavedWiFiCred* c = settings_system.getSavedWiFi(0);
                        if (c) { ssid_s = c->ssid; pass_s = c->password; }
                    }
                    if (ssid_s && ssid_s[0] != '\0') {
                        setStatus("NTP sync...", 6000);
                        if (time_sync.syncWithWiFi(ssid_s, pass_s)) {
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
                    if (cfg.audio_volume < MAX_AUDIO_VOLUME) {
                        cfg.audio_volume += 5;
                        if (cfg.audio_volume > MAX_AUDIO_VOLUME) cfg.audio_volume = MAX_AUDIO_VOLUME;
                    }
                    save_load_system.saveGlobalConfig(cfg);
                    char buf[32]; snprintf(buf, sizeof(buf), "Volume %d%%", cfg.audio_volume);
                    setStatus(buf, 900);
                } else if (cmd == NAV_DOWN) {
                    if (cfg.audio_volume > 5) cfg.audio_volume -= 5;
                    else cfg.audio_volume = 0;
                    save_load_system.saveGlobalConfig(cfg);
                    char buf[32]; snprintf(buf, sizeof(buf), "Volume %d%%", cfg.audio_volume);
                    setStatus(buf, 900);
                }
            } else if (sett_section == SETT_SKILL_XP) {
                GlobalSettings& cfg = settings_system.settings();
                if (cmd == NAV_UP) {
                    if (cfg.skill_xp_ratio < 100) cfg.skill_xp_ratio += 5;
                    if (cfg.skill_xp_ratio > 100) cfg.skill_xp_ratio = 100;
                    save_load_system.saveGlobalConfig(cfg);
                    char buf[40]; snprintf(buf, sizeof(buf), "Skill XP ratio: %d%%", cfg.skill_xp_ratio);
                    setStatus(buf, 1000);
                } else if (cmd == NAV_DOWN) {
                    if (cfg.skill_xp_ratio >= 5) cfg.skill_xp_ratio -= 5;
                    else cfg.skill_xp_ratio = 0;
                    save_load_system.saveGlobalConfig(cfg);
                    char buf[40]; snprintf(buf, sizeof(buf), "Skill XP ratio: %d%%", cfg.skill_xp_ratio);
                    setStatus(buf, 1000);
                } else if (cmd == NAV_SELECT) {
                    cfg.skill_xp_split = !cfg.skill_xp_split;
                    save_load_system.saveGlobalConfig(cfg);
                    setStatus(cfg.skill_xp_split ? "Skill XP split ON" : "Skill XP split OFF", 1200);
                }
            }
            break;
        }

        case UI_HELP:
            break;

        case UI_ALARMS: {
            if (cmd == NAV_LEFT) {
                alarm_focus_timers = !alarm_focus_timers;
            } else if (!alarm_focus_timers) {
                // Alarms pane
                if (cmd == NAV_UP && alarm_sel_index > 0) alarm_sel_index--;
                else if (cmd == NAV_DOWN && alarm_sel_index + 1 < alarm_count) alarm_sel_index++;
                else if (cmd == NAV_SELECT && alarm_sel_index < alarm_count) {
                    alarm_entries[alarm_sel_index].enabled = !alarm_entries[alarm_sel_index].enabled;
                    setStatus(alarm_entries[alarm_sel_index].enabled ? "Alarm enabled" : "Alarm disabled", 1000);
                    save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
                } else if (cmd == NAV_BACK && alarm_sel_index < alarm_count) {
                    // Delete selected alarm
                    for (uint8_t i = alarm_sel_index; i < alarm_count - 1; i++)
                        alarm_entries[i] = alarm_entries[i + 1];
                    alarm_count--;
                    if (alarm_sel_index >= alarm_count && alarm_count > 0) alarm_sel_index = alarm_count - 1;
                    save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
                    setStatus("Alarm removed", 1000);
                }
            } else {
                // Timers pane
                if (cmd == NAV_UP && timer_sel_index > 0) timer_sel_index--;
                else if (cmd == NAV_DOWN && timer_sel_index + 1 < timer_count) timer_sel_index++;
                else if (cmd == NAV_SELECT && timer_sel_index < timer_count) {
                    TimerEntry& te = timer_entries[timer_sel_index];
                    te.running = !te.running;
                    if (te.running) {
                        // Record start as unix timestamp
                        te.start_unix_time = (uint32_t)time_sync.getCurrentTime();
                    }
                    setStatus(te.running ? "Timer started" : "Timer paused", 1000);
                    save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
                } else if (cmd == NAV_BACK && timer_sel_index < timer_count) {
                    // Delete selected timer
                    for (uint8_t i = timer_sel_index; i < timer_count - 1; i++)
                        timer_entries[i] = timer_entries[i + 1];
                    timer_count--;
                    if (timer_sel_index >= timer_count && timer_count > 0) timer_sel_index = timer_count - 1;
                    save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
                    setStatus("Timer removed", 1000);
                }
            }
            break;
        }
    }
}

void handleKeyInput(char key) {
    if (text_input_active) {
        handleTextInputChar(key);
        return;
    }

    if (overlay_mode == OVERLAY_PROFILE_DELETE) {
        if (key == 'y' || key == 'Y') {
            refreshProfiles();
            if (profile_count <= 1) {
                overlay_mode = OVERLAY_NONE;
                setStatus("Cannot delete last profile", 1500);
                return;
            }
            String delete_name = profile_list[selected_profile_index].name;
            String delete_desc = profile_list[selected_profile_index].description;
            uint16_t fallback_idx = selected_profile_index > 0 ? selected_profile_index - 1 : 1;
            if (fallback_idx >= profile_count) fallback_idx = 0;
            String fallback_name = profile_list[fallback_idx].name;
            String fallback_desc = profile_list[fallback_idx].description;
            if (save_load_system.deleteProfile(delete_name, fallback_name, fallback_desc)) {
                overlay_mode = OVERLAY_NONE;
                refreshProfiles();
                loadAllProfileData();
                setStatus("Profile deleted", 1500);
            } else {
                overlay_mode = OVERLAY_NONE;
                setStatus("Profile delete failed", 1500);
            }
        } else if (key == 'n' || key == 'N' || key == '`') {
            overlay_mode = OVERLAY_NONE;
            setStatus("Profile delete cancelled", 1000);
        }
        return;
    }

    if (overlay_mode == OVERLAY_TASK_SKILLS && key == ' ') {
        handleNavCommand(NAV_SELECT);
        return;
    }

    if (overlay_mode == OVERLAY_SHOP_CONFIRM_DELETE) {
        if (key == 'y' || key == 'Y') {
            overlay_mode = OVERLAY_NONE;
            if (!shop_confirm_is_recipe) {
                ShopItem* it = shop_system.getItemByActiveIndex(selected_shop_item_index);
                if (it && shop_system.removeItem(it->id)) {
                    normalizeShopSelection();
                    saveShopData();
                    setStatus("Item removed");
                } else { setStatus("Remove failed"); }
            } else {
                CraftRecipe* rc = shop_system.getRecipeByActiveIndex(selected_shop_recipe_index);
                if (rc && shop_system.removeRecipe(rc->id)) {
                    normalizeShopSelection();
                    saveShopData();
                    setStatus("Recipe removed");
                } else { setStatus("Remove failed"); }
            }
        } else if (key == 'n' || key == 'N' || key == '`') {
            overlay_mode = OVERLAY_NONE;
            setStatus("Delete cancelled", 1000);
        }
        return;
    }

    if (overlay_mode == OVERLAY_SKILL_DELETE_CONFIRM) {
        if (key == 'y' || key == 'Y') {
            overlay_mode = OVERLAY_NONE;
            if (skill_delete_is_category) {
                const SkillCategory* cat = selectedCategory();
                if (cat && skill_system.removeCategory(cat->id)) {
                    normalizeSkillSelection();
                    saveSkills();
                    setStatus("Category deleted");
                } else { setStatus("Delete failed"); }
            } else {
                const Skill* sk = selectedSkillInSelectedCategory();
                if (sk && skill_system.removeSkill(sk->id)) {
                    normalizeSkillSelection();
                    saveSkills();
                    setStatus("Skill deleted");
                } else { setStatus("Delete failed"); }
            }
        } else if (key == 'n' || key == 'N' || key == '`') {
            overlay_mode = OVERLAY_NONE;
            setStatus("Delete cancelled", 1000);
        }
        return;
    }

    // G0 keyboard key triggers screensaver
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

    // TAB globally opens screen selector (handled in loop before this function).

    // 'H' key toggles per-screen help overlay. When open, closes it; otherwise opens it.
    // In shop setup mode, 'H' cycles recipe builder slot instead.
    if (key == 'h' || key == 'H') {
        if (help_overlay_active) {
            help_overlay_active = false;
            return;
        }
        if (current_screen == UI_SHOP && shop_setup_mode) {
            recipe_builder_slot = (recipe_builder_slot + 1) % 5;
            return;
        }
        help_overlay_active = true;
        return;
    }

    // 'L' key activates manual health input mode (globally, except in text input)
    if (key == 'l' || key == 'L') {
        overlay_mode = OVERLAY_NONE;
        help_overlay_active = false;
        manual_health_active = true;
        manual_health_input_percent = (uint8_t)health_system.getHealth();
        manual_health_last_input_time = millis();
        setStatus("Health mode: 1-0 set%, -/= adjust, ENT confirm", 2000);
        return;
    }

    switch (current_screen) {
        case UI_DASHBOARD:
            if (key == 'f' || key == 'F') {
                failTaskAtIndex(selected_task_index);
            }
            break;

        case UI_TASKS:
            if (key == 'e' || key == 'E') {
                const Task* t = selectedTask();
                if (t) {
                    task_edit_mode = !task_edit_mode;
                    setStatus(task_edit_mode ? "Task edit mode" : "Task browse mode", 1200);
                }
            } else if (task_edit_mode && key == ' ') {
                const Task* t = selectedTask();
                if (!t) break;
                if (selected_task_field == TASK_EDIT_DURATION) {
                    openTaskDurationOverlay(*t);
                } else if (selected_task_field == TASK_EDIT_DUE_DATE) {
                    openTaskDueDateOverlay(*t);
                } else if (selected_task_field == TASK_EDIT_REWARD_MONEY) {
                    openTaskMoneyOverlay(*t);
                } else if (selected_task_field == TASK_EDIT_LINKED_SKILLS) {
                    openTaskSkillsOverlay(*t);
                }
            } else if (key == 'n' || key == 'N') {
                beginTextInput(INPUT_NEW_TASK_NAME);
            } else if (key == 'd' || key == 'D') {
                Task* t = selectedTaskMutable();
                if (t && t->isTerminal()) {
                    if (task_manager.archiveTask(t->id)) {
                        normalizeTaskSelection();
                        saveAllProfileData();
                        setStatus("Task archived");
                    }
                } else if (t && task_manager.removeTask(t->id)) {
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
                failTaskAtIndex(selected_task_index);
            }
            break;

        case UI_SKILLS:
            if (key == 'e' || key == 'E') {
                skill_edit_mode = !skill_edit_mode;
                setStatus(skill_edit_mode ? "Skill edit ON (SPC=edit menu)" : "Skill edit OFF (SPC=cycle chart)");
            } else if (skill_edit_mode && key == ' ') {
                // In edit mode: open action menu (add/remove/edit skills and XP)
                skill_quick_menu_index = 0;
                overlay_mode = OVERLAY_SKILL_QUICK_MENU;
            } else if (!skill_edit_mode && key == ' ') {
                // Without edit mode: cycle spider chart only
                skill_spider_mode = (skill_spider_mode + 1) % 4;
                static const char* chart_names[] = {
                    "Chart: cat level", "Chart: cat total XP",
                    "Chart: skill level", "Chart: skill total XP"
                };
                setStatus(chart_names[skill_spider_mode], 1000);
            }
            break;

        case UI_SHOP:
            if (key == 'e' || key == 'E') {
                shop_edit_mode = !shop_edit_mode;
                shop_edit_is_recipe = !shop_focus_items;
                setStatus(shop_edit_mode ? "Edit mode ON (SPC=field menu)" : "Edit mode OFF");
            } else if (shop_edit_mode && key == ' ') {
                // Open field picker popup for current item or recipe
                shop_edit_is_recipe = !shop_focus_items;
                shop_field_menu_index = 0;
                overlay_mode = OVERLAY_SHOP_FIELD_MENU;
            } else if (key == 'z' || key == 'Z') {
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
            }
            break;

        case UI_PROFILES:
            if (key == 'n' || key == 'N') {
                beginTextInput(INPUT_NEW_PROFILE);
            } else if (key == 'g' || key == 'G') {
                beginTextInput(INPUT_PROFILE_DESCRIPTION);
            }
            break;

        case UI_SETTINGS:
            if (sett_section == SETT_TIMEZONE) {
                GlobalSettings& cfg = settings_system.settings();
                if (key == 'd' || key == 'D') {
                    cfg.timezone_dst = !cfg.timezone_dst;
                    time_sync.setDaylightSavingEnabled(cfg.timezone_dst);
                    save_load_system.saveGlobalConfig(cfg);
                    setStatus(cfg.timezone_dst ? "DST ON" : "DST OFF", 1200);
                } else if (key == 'f' || key == 'F') {
                    cfg.date_format_us = !cfg.date_format_us;
                    time_sync.setDateFormatUS(cfg.date_format_us);
                    save_load_system.saveGlobalConfig(cfg);
                    setStatus(cfg.date_format_us ? "Date MM/DD" : "Date DD/MM", 1200);
                }
            }
            break;

        case UI_SAVE_LOAD:
            break;

        case UI_ALARMS: {
            if (!alarm_focus_timers) {
                // Alarm pane key handling
                if (key == 'a' || key == 'A') {
                    // Add new alarm
                    if (alarm_count < MAX_ALARMS) {
                        alarm_edit_index = alarm_count;
                        memset(&alarm_entries[alarm_count], 0, sizeof(AlarmEntry));
                        alarm_entries[alarm_count].active = true;
                        alarm_entries[alarm_count].enabled = true;
                        alarm_entries[alarm_count].hour = alarm_add_hour;
                        alarm_entries[alarm_count].minute = alarm_add_minute;
                        alarm_count++;
                        save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
                        // Open detailed setup overlay for the new alarm
                        alarm_setup_entry_index = alarm_count - 1;
                        alarm_setup_field = 0;
                        overlay_mode = OVERLAY_ALARM_SETUP;
                    } else {
                        setStatus("Max alarms reached");
                    }
                } else if (key == ' ' && alarm_sel_index < alarm_count) {
                    // Open detailed alarm setup screen
                    alarm_setup_entry_index = alarm_sel_index;
                    alarm_setup_field = 0;
                    overlay_mode = OVERLAY_ALARM_SETUP;
                } else if ((key == '+' || key == '=') && alarm_sel_index < alarm_count) {
                    alarm_entries[alarm_sel_index].minute++;
                    if (alarm_entries[alarm_sel_index].minute >= 60) { alarm_entries[alarm_sel_index].minute = 0; alarm_entries[alarm_sel_index].hour = (alarm_entries[alarm_sel_index].hour + 1) % 24; }
                    save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
                } else if (key == '-' && alarm_sel_index < alarm_count) {
                    if (alarm_entries[alarm_sel_index].minute > 0) alarm_entries[alarm_sel_index].minute--;
                    else { alarm_entries[alarm_sel_index].minute = 59; alarm_entries[alarm_sel_index].hour = (alarm_entries[alarm_sel_index].hour + 23) % 24; }
                    save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
                }
            } else {
                // Timer pane key handling
                if (key == 't' || key == 'T') {
                    // Add new countdown timer
                    if (timer_count < MAX_TIMERS) {
                        memset(&timer_entries[timer_count], 0, sizeof(TimerEntry));
                        timer_entries[timer_count].active = true;
                        timer_entries[timer_count].duration_seconds = alarm_add_duration_seconds;
                        alarm_edit_index = timer_count;
                        timer_count++;
                        save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
                        // Open detailed setup overlay for the new timer
                        alarm_setup_entry_index = timer_count - 1;
                        alarm_setup_field = 0;
                        overlay_mode = OVERLAY_TIMER_SETUP;
                    } else {
                        setStatus("Max timers reached");
                    }
                } else if (key == ' ' && timer_sel_index < timer_count) {
                    // Open detailed timer setup screen
                    alarm_setup_entry_index = timer_sel_index;
                    alarm_setup_field = 0;
                    overlay_mode = OVERLAY_TIMER_SETUP;
                } else if ((key == '+' || key == '=') && timer_sel_index < timer_count) {
                    timer_entries[timer_sel_index].duration_seconds += 60;
                    save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
                } else if (key == '-' && timer_sel_index < timer_count) {
                    if (timer_entries[timer_sel_index].duration_seconds >= 60)
                        timer_entries[timer_sel_index].duration_seconds -= 60;
                    save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
                }
            }
            break;
        }
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
    else if (current_screen == UI_INVENTORY) screen_name = "INV";
    else if (current_screen == UI_SAVE_LOAD) screen_name = "SAVE";
    else if (current_screen == UI_SETTINGS) screen_name = "SETT";
    else if (current_screen == UI_HELP) screen_name = "HELP";
    else if (current_screen == UI_ALARMS) screen_name = "ALARMS";

    ui_canvas.fillRoundRect(0, 0, 240, 20, 4, accent);
    ui_canvas.setTextColor(BLACK, accent);
    ui_canvas.setCursor(6, 6);
    ui_canvas.printf("%s", screen_name);
    ui_canvas.setCursor(80, 6);
    uint16_t shown_level = (hud_levelup_pending > 0) ? hud_display_level : level_system.getLevel();
    ui_canvas.printf("LVL%d  HP%d", shown_level, health_system.getHealth());

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
    ui_canvas.printf("XP %3d%%", (int)hud_level_anim);
    
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

    // Info bar below header: date + time + [timer bar] + battery.
    ui_canvas.fillRoundRect(0, 21, 240, 12, 2, 0x18C3);
    ui_canvas.setTextColor(0xE71C, 0x18C3);
    ui_canvas.setTextSize(1);
    ui_canvas.setCursor(4, 24);
    ui_canvas.printf("%s %s", date_part.c_str(), time_part.c_str());
    ui_canvas.setCursor(188, 24);
    ui_canvas.printf("BAT %d%%", batt);

    // Timer mini-bar: find longest-running (earliest start) timer
    {
        TimerEntry* rt = nullptr;
        uint32_t earliest = 0xFFFFFFFFUL;
        time_t cur_unix = time_sync.getCurrentTime();
        for (uint8_t i = 0; i < timer_count; i++) {
            if (timer_entries[i].active && timer_entries[i].running && timer_entries[i].start_unix_time != 0) {
                if (timer_entries[i].start_unix_time < earliest) {
                    earliest = timer_entries[i].start_unix_time;
                    rt = &timer_entries[i];
                }
            }
        }
        if (rt) {
            uint32_t elapsed = (cur_unix > (time_t)rt->start_unix_time) ?
                               (uint32_t)(cur_unix - rt->start_unix_time) : 0;
            if (elapsed > rt->duration_seconds) elapsed = rt->duration_seconds;
            uint32_t remaining = rt->duration_seconds - elapsed;
            float progress = (rt->duration_seconds > 0) ?
                             (1.0f - (float)elapsed / (float)rt->duration_seconds) : 0.0f;
            // Draw mini bar at x=103
            int bar_x = 103, bar_y = 23, bar_w = 28, bar_h = 6;
            ui_canvas.fillRect(bar_x, bar_y, bar_w, bar_h, 0x0841);
            ui_canvas.fillRect(bar_x, bar_y, (int)(bar_w * progress), bar_h, 0x07FF);
            // Timer name/label
            char label[8];
            if (rt->linked_item_id > 0) {
                const ShopItem* it = shop_system.getItem(rt->linked_item_id);
                if (it) {
                    snprintf(label, sizeof(label), "%.4s", it->name);
                } else {
                    snprintf(label, sizeof(label), "#%d", rt->linked_item_id);
                }
            } else if (rt->name[0]) {
                snprintf(label, sizeof(label), "%.4s", rt->name);
            } else {
                snprintf(label, sizeof(label), "T%d", (int)(rt - timer_entries) + 1);
            }
            char tstr[12];
            uint32_t h = remaining / 3600, m = (remaining % 3600) / 60, s = remaining % 60;
            if (h > 0) snprintf(tstr, sizeof(tstr), "%u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
            else snprintf(tstr, sizeof(tstr), "%u:%02u", (unsigned)m, (unsigned)s);
            ui_canvas.setTextColor(0xE71C, 0x18C3);
            ui_canvas.setCursor(bar_x + bar_w + 2, 24);
            ui_canvas.printf("%s %s", label, tstr);
        }
    }

    // Bottom bar moved up for full visibility.
    ui_canvas.fillRoundRect(0, 124, 240, 14, 2, panel);

    // Right-half scrolling controls (dynamic per screen), starting after left quarter.
    if (millis() - control_scroll_last_update > 120) {
        control_scroll_offset += 2;
        control_scroll_last_update = millis();
    }
    String scroll_text = "Controls: ";
    if (screen_selector_active) {
        scroll_text += ";/.:Move  ENT:Select  `:Close  TAB:Menu";
    } else if (manual_health_active) {
        scroll_text += "1-0:Set  -=Down  =:Up  ENT:Apply  `:Cancel  TAB:Menu";
    } else if (text_input_active) {
        scroll_text += "Type text  DEL:Erase  ENT:Save  `:Cancel  TAB:Menu";
    } else if (current_screen == UI_DASHBOARD) {
        scroll_text += ";/.:Task  ENT:Done  TAB:Menu  G0:Screen  G:Saver  L:Health  [:Dim  ]:Bright";
    } else if (current_screen == UI_TASKS) {
        if (task_edit_mode) scroll_text += ",/:Field  ;/.:Value  SPC:Detail  ENT:Save  E:ExitEdit  TAB:Menu";
        else scroll_text += ";/.:Select  ENT:Done  E:Edit  N:New  D:Delete  T:Details  TAB:Menu";
    } else if (current_screen == UI_SKILLS) {
        if (skill_edit_mode) scroll_text += ",/:Category  ;/.:Skill  E:ExitEdit  SPC:EditMenu(AddCat/AddSkill/XP)  TAB:Menu";
        else scroll_text += ",/:Category  ;/.:Skill  E:EditMode  SPC:CycleChart  TAB:Menu";
    } else if (current_screen == UI_SHOP) {
        scroll_text += ";/.:Select  ,/:Focus  ENT:BuyCraft  Z:Setup  N:Add  R:Recipe  TAB:Menu";
    } else if (current_screen == UI_PROFILES) {
        scroll_text += ";/.:Profile  ENT:Load  N:New  G:Desc  DEL:Delete  TAB:Menu";
    } else if (current_screen == UI_SAVE_LOAD) {
        scroll_text += ";/.:Option  ENT:Run  TAB:Menu";
    } else if (current_screen == UI_SETTINGS) {
        if (sett_section == SETT_WIFI) scroll_text += "LEFT:Toggle Saved/Scan  ;/.:Select  ENT:Connect  DEL:Scan/Remove  TAB:Menu";
        else scroll_text += ",/:Section  ;/.:Value  ENT:Apply  TAB:Menu";
    } else if (current_screen == UI_HELP) {
        scroll_text += "Read guide  H:Close help overlay  TAB:Menu";
    } else if (current_screen == UI_ALARMS) {
        scroll_text += "LEFT:Alarms/Timers  ;/.:Select  ENT:Toggle  DEL:Remove  A:Add  T:Add timer  SPC:Setup  TAB:Menu";
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
        if (screen_selector_sub_active) {
            uint8_t sub_count = getSelectorSubCount(screen_selector_index);
            ui_canvas.fillRoundRect(40, 30, 160, 95, 4, panel);
            ui_canvas.setTextColor(yellow, panel);
            ui_canvas.setCursor(48, 33);
            ui_canvas.printf("< %s >", indexToScreenName(screen_selector_index));
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(42, 43);
            ui_canvas.println("-- sub-sections --");
            uint8_t sub_visible = 6;
            uint8_t sub_start = (screen_selector_sub_index > 2) ? screen_selector_sub_index - 2 : 0;
            for (uint8_t i = 0; i < sub_visible && (sub_start + i) < sub_count; i++) {
                uint8_t idx = sub_start + i;
                uint16_t col2 = (idx == screen_selector_sub_index) ? yellow : muted;
                if (idx == screen_selector_sub_index) {
                    ui_canvas.fillRoundRect(44, 52 + (i * 11), 152, 10, 2, accent);
                }
                ui_canvas.setTextColor(col2, panel);
                ui_canvas.setCursor(48, 53 + (i * 11));
                ui_canvas.print(getSelectorSubName(screen_selector_index, idx));
            }
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(42, 117);
            ui_canvas.println("Enter=go   `=back");
        } else {
            uint8_t visible_items = 6;
            uint8_t start_idx = (screen_selector_index > 2) ? (screen_selector_index - 2) : 0;
            for (uint8_t i = 0; i < visible_items && (start_idx + i) < SCREEN_SELECTOR_COUNT; i++) {
                uint8_t idx = start_idx + i;
                uint16_t col2 = (idx == screen_selector_index) ? yellow : muted;
                if (idx == screen_selector_index) {
                    ui_canvas.fillRoundRect(35, 42 + (i * 12), 170, 11, 2, accent);
                }
                ui_canvas.setTextColor(col2, panel);
                ui_canvas.setCursor(40, 43 + (i * 12));
                const char* sub_hint = getSelectorSubCount(idx) > 0 ? " >" : "";
                ui_canvas.printf("  %s%s", indexToScreenName(idx), sub_hint);
            }
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(35, 107);
            ui_canvas.println("Enter=select  `=close");
        }
        ui_canvas.pushSprite(0, 0);
        return;
    }

    if (overlay_mode != OVERLAY_NONE) {
        ui_canvas.fillRoundRect(16, 36, 208, 82, 4, panel);
        ui_canvas.setTextColor(text, panel);
        ui_canvas.setTextSize(1);
        if (overlay_mode == OVERLAY_PROFILE_DELETE) {
            refreshProfiles();
            String prof_name = (profile_count > 0) ? profile_list[selected_profile_index].name : String("?");
            uint32_t remain = (profile_delete_confirm_until > millis()) ? ((profile_delete_confirm_until - millis() + 999) / 1000) : 0;
            ui_canvas.setCursor(24, 42);
            ui_canvas.println("DELETE PROFILE");
            ui_canvas.setTextColor(yellow, panel);
            ui_canvas.setCursor(24, 58);
            ui_canvas.println(truncateUiText(prof_name.c_str(), 22));
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(24, 78);
            ui_canvas.printf("Y = yes   N = no (%lus)", (unsigned long)remain);
        } else if (overlay_mode == OVERLAY_TASK_DURATION) {
            ui_canvas.setCursor(24, 42);
            ui_canvas.println("TASK DURATION");
            static const int box_x[3] = {44, 96, 148};
            for (uint8_t i = 0; i < 3; i++) {
                uint16_t bg_col = (i == task_duration_field) ? accent : bg;
                ui_canvas.fillRoundRect(box_x[i], 66, 32, 18, 3, bg_col);
                ui_canvas.setTextColor((i == task_duration_field) ? BLACK : text, bg_col);
                ui_canvas.setCursor(box_x[i] + 7, 72);
                ui_canvas.printf("%02d", task_duration_vals[i]);
            }
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(56, 88);
            ui_canvas.println("HH   MM   SS");
            ui_canvas.setCursor(28, 104);
            ui_canvas.println(";/.:change  ,/:field  ENT:save");
        } else if (overlay_mode == OVERLAY_TASK_MONEY) {
            static const int32_t steps[5] = {1, 10, 100, 1000, 10000};
            ui_canvas.setCursor(24, 42);
            ui_canvas.println("REWARD MONEY");
            ui_canvas.setTextColor(yellow, panel);
            ui_canvas.setTextSize(2);
            ui_canvas.setCursor(32, 64);
            ui_canvas.printf("$%ld", (long)task_money_edit);
            ui_canvas.setTextSize(1);
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(32, 92);
            ui_canvas.printf("Step: %ld", (long)steps[task_money_step_index]);
            ui_canvas.setCursor(24, 104);
            ui_canvas.println(";/.:amount  ,/:step  ENT:save");
        } else if (overlay_mode == OVERLAY_TASK_DUE_DATE) {
            uint16_t year = 2000U + task_due_vals[5];
            static const char* months[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
            struct tm first = {};
            first.tm_year = (int)year - 1900;
            first.tm_mon = task_due_vals[4] - 1;
            first.tm_mday = 1;
            mktime(&first);
            uint8_t first_wday = (uint8_t)((first.tm_wday + 6) % 7);
            uint8_t max_day = daysInMonth(year, task_due_vals[4]);
            ui_canvas.setCursor(24, 40);
            ui_canvas.printf("DUE DATE %s %u", months[task_due_vals[4] - 1], year);
            const char* wdays = "MTWTFSS";
            for (uint8_t i = 0; i < 7; i++) {
                ui_canvas.setCursor(30 + i * 25, 52);
                ui_canvas.printf("%c", wdays[i]);
            }
            for (uint8_t day = 1; day <= max_day; day++) {
                uint8_t slot = first_wday + day - 1;
                uint8_t row = slot / 7;
                uint8_t col = slot % 7;
                int x = 26 + col * 25;
                int y = 62 + row * 9;
                uint16_t bg_col = (day == task_due_vals[3] && !task_due_time_stage) ? accent : panel;
                ui_canvas.fillRoundRect(x, y, 18, 8, 2, bg_col);
                ui_canvas.setTextColor((day == task_due_vals[3] && !task_due_time_stage) ? BLACK : text, bg_col);
                ui_canvas.setCursor(x + 3, y + 1);
                ui_canvas.printf("%d", day);
            }
            ui_canvas.setTextColor(task_due_time_stage ? yellow : muted, panel);
            ui_canvas.setCursor(40, 109);
            ui_canvas.printf("%02d:%02d:%02d", task_due_vals[0], task_due_vals[1], task_due_vals[2]);
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(100, 109);
            ui_canvas.println(task_due_time_stage ? "ENT save" : "ENT time");
        } else if (overlay_mode == OVERLAY_TASK_SKILLS) {
            uint16_t cat_count = skill_system.getActiveCategoryCount();
            const SkillCategory* cat = skill_system.getCategoryByActiveIndex(task_skill_menu_category_index);
            ui_canvas.setCursor(22, 40);
            ui_canvas.println("TASK SKILLS");
            for (uint8_t i = 0; i < 4 && i < cat_count; i++) {
                uint16_t idx = (task_skill_menu_category_index > 1 ? task_skill_menu_category_index - 1 : 0) + i;
                if (idx >= cat_count) break;
                const SkillCategory* c = skill_system.getCategoryByActiveIndex(idx);
                if (!c) continue;
                uint16_t bg_col = (!task_skill_menu_focus_skills && idx == task_skill_menu_category_index) ? accent : bg;
                ui_canvas.fillRoundRect(22, 54 + i * 12, 84, 10, 2, bg_col);
                ui_canvas.setTextColor((!task_skill_menu_focus_skills && idx == task_skill_menu_category_index) ? BLACK : text, bg_col);
                ui_canvas.setCursor(24, 56 + i * 12);
                ui_canvas.printf("%c %s", taskCategoryFullySelected(*selectedTask(), c->id) ? '+' : '-', truncateUiText(c->name, 10).c_str());
            }
            if (cat) {
                uint16_t skill_count = skill_system.getActiveSkillCountInCategory(cat->id);
                for (uint8_t i = 0; i < 4 && i < skill_count; i++) {
                    uint16_t idx = (task_skill_menu_skill_index > 1 ? task_skill_menu_skill_index - 1 : 0) + i;
                    if (idx >= skill_count) break;
                    const Skill* skill = skill_system.getSkillInCategoryByActiveIndex(cat->id, idx);
                    if (!skill) continue;
                    uint16_t bg_col = (task_skill_menu_focus_skills && idx == task_skill_menu_skill_index) ? accent : bg;
                    ui_canvas.fillRoundRect(116, 54 + i * 12, 96, 10, 2, bg_col);
                    ui_canvas.setTextColor((task_skill_menu_focus_skills && idx == task_skill_menu_skill_index) ? BLACK : text, bg_col);
                    ui_canvas.setCursor(118, 56 + i * 12);
                    ui_canvas.printf("%c %s", selectedTask()->hasLinkedSkill(skill->id) ? 'x' : ' ', truncateUiText(skill->name, 12).c_str());
                }
            }
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(24, 104);
            ui_canvas.println(",/:pane ;/.:move SPC/ENT:toggle");
        } else if (overlay_mode == OVERLAY_SHOP_FIELD_MENU) {
            static const char* item_fields[9] = {"Name", "Description", "Effect Type", "Effect Value", "Base Price", "Buy Limit", "Cooldown(s)", "Add New Item", "Remove Item"};
            static const char* recipe_fields[10] = {"Add Recipe", "Remove Recipe", "Slot1 Item", "Slot1 Qty", "Slot2 Item", "Slot2 Qty", "Slot3 Item", "Slot3 Qty", "Output Item", "Output Qty"};
            const char** fields = shop_edit_is_recipe ? recipe_fields : item_fields;
            uint8_t field_count = shop_edit_is_recipe ? 10 : 9;
            uint8_t window = 7;
            uint8_t start_idx = (shop_field_menu_index > 3) ? (uint8_t)(shop_field_menu_index - 3) : 0;
            if (start_idx + window > field_count) start_idx = field_count > window ? (uint8_t)(field_count - window) : 0;
            ui_canvas.setCursor(24, 40);
            ui_canvas.println(shop_edit_is_recipe ? "EDIT RECIPE" : "EDIT ITEM");
            for (uint8_t i = 0; i < window; i++) {
                uint8_t idx = start_idx + i;
                if (idx >= field_count) break;
                uint16_t row_bg = (idx == shop_field_menu_index) ? accent : bg;
                uint16_t row_fg = (idx == shop_field_menu_index) ? BLACK : text;
                ui_canvas.fillRoundRect(24, 52 + i * 10, 192, 9, 2, row_bg);
                ui_canvas.setTextColor(row_fg, row_bg);
                ui_canvas.setCursor(28, 53 + i * 10);
                ui_canvas.print(fields[idx]);
            }
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(24, 106);
            ui_canvas.println(";/.:move  ENT:select  `:back");
        } else if (overlay_mode == OVERLAY_SHOP_EFFECT_PICK) {
            static const char* effect_names[5] = {"None/Collectible", "Consume & Remove", "Add XP", "Random XP", "Countdown"};
            ui_canvas.setCursor(24, 40);
            ui_canvas.println("EFFECT TYPE");
            for (uint8_t i = 0; i < 5; i++) {
                uint16_t row_bg = (i == shop_effect_pick_index) ? accent : bg;
                uint16_t row_fg = (i == shop_effect_pick_index) ? BLACK : text;
                ui_canvas.fillRoundRect(24, 52 + i * 12, 192, 10, 2, row_bg);
                ui_canvas.setTextColor(row_fg, row_bg);
                ui_canvas.setCursor(28, 53 + i * 12);
                ui_canvas.print(effect_names[i]);
            }
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(24, 106);
            ui_canvas.println(";/.:move  ENT:select  `:back");
        } else if (overlay_mode == OVERLAY_SHOP_PRICE_EDIT) {
            static const int32_t p_steps[5] = {1, 10, 100, 1000, 10000};
            static const char* num_labels[6] = {"Base Price", "Buy Limit", "Cooldown (s)", "Ingr. Qty", "Output Qty", "Effect Value"};
            ui_canvas.setCursor(24, 40);
            ui_canvas.println(num_labels[shop_num_edit_target]);
            ui_canvas.setTextColor(yellow, panel);
            ui_canvas.setTextSize(2);
            ui_canvas.setCursor(32, 64);
            ui_canvas.printf("%ld", (long)shop_num_edit_value);
            ui_canvas.setTextSize(1);
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(32, 92);
            ui_canvas.printf("Step: %ld", (long)p_steps[shop_num_edit_step_index]);
            ui_canvas.setCursor(24, 104);
            ui_canvas.println(";/.:amount  ,/:step  ENT:save");
        } else if (overlay_mode == OVERLAY_SHOP_RECIPE_ITEM_PICK) {
            uint16_t item_count = shop_system.getActiveItemCount();
            ui_canvas.setCursor(24, 40);
            ui_canvas.printf("%s", shop_recipe_pick_slot == 3 ? "OUTPUT ITEM" : "INGREDIENT ITEM");
            for (uint8_t i = 0; i < 5 && (shop_recipe_pick_item_index - (shop_recipe_pick_item_index > 2 ? shop_recipe_pick_item_index - 2 : 0) + i) < item_count; i++) {
                uint16_t idx = (shop_recipe_pick_item_index > 2 ? shop_recipe_pick_item_index - 2 : 0) + i;
                if (idx >= item_count) break;
                const ShopItem* browse_it = shop_system.getItemByActiveIndex(idx);
                if (!browse_it) continue;
                uint16_t row_bg = (idx == shop_recipe_pick_item_index) ? accent : bg;
                uint16_t row_fg = (idx == shop_recipe_pick_item_index) ? BLACK : text;
                ui_canvas.fillRoundRect(24, 52 + i * 12, 192, 10, 2, row_bg);
                ui_canvas.setTextColor(row_fg, row_bg);
                ui_canvas.setCursor(28, 53 + i * 12);
                ui_canvas.printf("(%d) %s", browse_it->id, truncateUiText(browse_it->name, 16).c_str());
            }
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(24, 106);
            ui_canvas.println(";/.:move  ENT:select  `:back");
        } else if (overlay_mode == OVERLAY_SHOP_CONFIRM_DELETE) {
            String del_name = "?";
            if (!shop_confirm_is_recipe) {
                const ShopItem* it = shop_system.getItemByActiveIndex(selected_shop_item_index);
                if (it) del_name = String(it->name);
            } else {
                const CraftRecipe* rc = shop_system.getRecipeByActiveIndex(selected_shop_recipe_index);
                if (rc) del_name = String("Recipe #") + String(rc->id);
            }
            uint32_t remain = (profile_delete_confirm_until > millis()) ? ((profile_delete_confirm_until - millis() + 999) / 1000) : 0;
            ui_canvas.setCursor(24, 42);
            ui_canvas.println(shop_confirm_is_recipe ? "DELETE RECIPE" : "DELETE ITEM");
            ui_canvas.setTextColor(yellow, panel);
            ui_canvas.setCursor(24, 58);
            ui_canvas.println(truncateUiText(del_name.c_str(), 22).c_str());
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(24, 78);
            ui_canvas.printf("Y = yes   N = no (%lus)", (unsigned long)remain);
        } else if (overlay_mode == OVERLAY_SKILL_FIELD_MENU) {
            // Category fields: Rename Cat / Add Category / Add Skill / Delete Cat
            // Skill fields:    Rename Skill / Edit Details / Add Skill / Delete Skill
            static const char* cat_fields[4]  = {"Rename Category", "Add Category", "Add Skill", "Delete Category"};
            static const char* sk_fields[4]   = {"Rename Skill", "Edit Details", "Add Skill", "Delete Skill"};
            const char** fields = skill_edit_is_category ? cat_fields : sk_fields;
            uint8_t field_count = 4;
            ui_canvas.setCursor(24, 40);
            ui_canvas.println(skill_edit_is_category ? "CATEGORY EDIT" : "SKILL EDIT");
            for (uint8_t i = 0; i < field_count; i++) {
                uint16_t row_bg = (i == skill_field_menu_index) ? accent : bg;
                uint16_t row_fg = (i == skill_field_menu_index) ? BLACK : text;
                ui_canvas.fillRoundRect(24, 52 + i * 12, 192, 10, 2, row_bg);
                ui_canvas.setTextColor(row_fg, row_bg);
                ui_canvas.setCursor(28, 53 + i * 12);
                ui_canvas.print(fields[i]);
            }
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(24, 106);
            ui_canvas.println(";/.:move  ENT:select  `:back");
        } else if (overlay_mode == OVERLAY_SKILL_DELETE_CONFIRM) {
            const char* del_what = skill_delete_is_category ? "CATEGORY" : "SKILL";
            String del_name = "?";
            if (skill_delete_is_category) {
                const SkillCategory* cat = selectedCategory();
                if (cat) del_name = String(cat->name);
            } else {
                const Skill* sk = selectedSkillInSelectedCategory();
                if (sk) del_name = String(sk->name);
            }
            uint32_t remain = (profile_delete_confirm_until > millis()) ? ((profile_delete_confirm_until - millis() + 999) / 1000) : 0;
            ui_canvas.setCursor(24, 42);
            ui_canvas.printf("DELETE %s", del_what);
            ui_canvas.setTextColor(yellow, panel);
            ui_canvas.setCursor(24, 58);
            ui_canvas.println(truncateUiText(del_name.c_str(), 22).c_str());
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(24, 78);
            ui_canvas.printf("Y = yes   N = no (%lus)", (unsigned long)remain);
        } else if (overlay_mode == OVERLAY_SKILL_QUICK_MENU) {
            const SkillCategory* qcat = selectedCategory();
            const Skill* qsk = selectedSkillInSelectedCategory();
            static const char* qitems[4] = {"Add Category", "Add Skill", "Edit Skill XP", "Edit Main XP"};
            ui_canvas.setCursor(24, 40);
            ui_canvas.println("SKILLS EDIT MENU");
            for (uint8_t i = 0; i < 4; i++) {
                bool disabled = (i == 1 && !qcat) || (i == 2 && !qsk);
                bool selected = (i == skill_quick_menu_index);
                // Selected+disabled: dim highlight; Selected+enabled: accent; Not selected: bg
                uint16_t row_bg = selected ? (disabled ? 0x4208 : accent) : bg;
                uint16_t row_fg = disabled ? muted : (selected ? BLACK : text);
                ui_canvas.fillRoundRect(24, 52 + i * 11, 192, 10, 2, row_bg);
                ui_canvas.setTextColor(row_fg, row_bg);
                ui_canvas.setCursor(28, 53 + i * 11);
                ui_canvas.print(qitems[i]);
            }
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(24, 102);
            ui_canvas.println(";/.:move  ENT:select  `:back");
        } else if (overlay_mode == OVERLAY_SKILL_XP_EDIT) {
            const Skill* xpsk = skill_system.getSkill(skill_xp_edit_skill_id);
            static const int32_t xp_steps[5] = {1, 10, 100, 1000, 10000};
            ui_canvas.setCursor(24, 40);
            ui_canvas.printf("EDIT SKILL XP: %s", xpsk ? truncateUiText(xpsk->name, 14).c_str() : "?");
            ui_canvas.setTextColor(skill_xp_edit_value >= 0 ? 0x07E0 : 0xF800, panel);
            ui_canvas.setTextSize(2);
            ui_canvas.setCursor(32, 62);
            ui_canvas.printf("%+ld", (long)skill_xp_edit_value);
            ui_canvas.setTextSize(1);
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(32, 90);
            ui_canvas.printf("Step: %ld", (long)xp_steps[skill_xp_edit_step_index]);
            if (xpsk) { ui_canvas.setCursor(32, 100); ui_canvas.printf("Current: %lu XP Lv%d", (unsigned long)xpsk->current_xp, xpsk->level); }
            ui_canvas.setCursor(24, 110);
            ui_canvas.println(";/.:amount  ,/:step  ENT:apply");
        } else if (overlay_mode == OVERLAY_MAIN_XP_EDIT) {
            static const int32_t mxp_steps[5] = {1, 10, 100, 1000, 10000};
            ui_canvas.setCursor(24, 40);
            ui_canvas.println("EDIT MAIN XP");
            ui_canvas.setTextColor(skill_xp_edit_value >= 0 ? 0x07E0 : 0xF800, panel);
            ui_canvas.setTextSize(2);
            ui_canvas.setCursor(32, 62);
            ui_canvas.printf("%+ld", (long)skill_xp_edit_value);
            ui_canvas.setTextSize(1);
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(32, 90);
            ui_canvas.printf("Step: %ld", (long)mxp_steps[skill_xp_edit_step_index]);
            ui_canvas.setCursor(32, 100);
            ui_canvas.printf("Cur XP: %lu  Lv%d", (unsigned long)level_system.getCurrentXP(), level_system.getLevel());
            ui_canvas.setCursor(24, 110);
            ui_canvas.println(";/.:amount  ,/:step  ENT:apply");
        } else if (overlay_mode == OVERLAY_MATH_QUIZ) {
            static const char* op_chars[3] = {"+", "-", "*"};
            ui_canvas.setCursor(24, 36);
            ui_canvas.setTextColor(yellow, panel);
            ui_canvas.printf("MATH QUIZ %d/3 - Type the answer:", (int)math_quiz_question + 1);
            ui_canvas.setTextColor(text, panel);
            ui_canvas.setTextSize(2);
            ui_canvas.setCursor(28, 56);
            ui_canvas.printf("%ld %s %ld = ?", (long)math_quiz_a, op_chars[math_quiz_op], (long)math_quiz_b);
            ui_canvas.setTextSize(1);
            ui_canvas.fillRoundRect(28, 84, 184, 14, 3, bg);
            ui_canvas.setCursor(32, 87);
            ui_canvas.setTextColor(text, bg);
            ui_canvas.println(text_input_buffer);
            ui_canvas.setTextColor(muted, panel);
            ui_canvas.setCursor(24, 106);
            ui_canvas.println("Type answer  ENT:confirm  `:cancel");
        } else if (overlay_mode == OVERLAY_ALARM_SETUP) {
            if (alarm_setup_entry_index < alarm_count) {
                AlarmEntry& ae = alarm_entries[alarm_setup_entry_index];
                ui_canvas.setCursor(24, 40);
                ui_canvas.printf("ALARM SETUP (#%d)", alarm_setup_entry_index + 1);
                static const char* af_names[4] = {"Name", "Hour", "Minute", "Enabled"};
                static const char* af_hints[4] = {"ENT:edit name", ";/.:change", ";/.:change", ";/.:toggle"};
                ui_canvas.setTextColor(text, panel);
                for (uint8_t i = 0; i < 4; i++) {
                    uint16_t rbg = (i == alarm_setup_field) ? accent : bg;
                    uint16_t rfg = (i == alarm_setup_field) ? BLACK : text;
                    ui_canvas.fillRoundRect(24, 54 + i * 14, 192, 12, 2, rbg);
                    ui_canvas.setTextColor(rfg, rbg);
                    ui_canvas.setCursor(28, 56 + i * 14);
                    if (i == 0) ui_canvas.printf("Name: %.16s", ae.name[0] ? ae.name : "(none)");
                    else if (i == 1) ui_canvas.printf("Hour: %02d", ae.hour);
                    else if (i == 2) ui_canvas.printf("Minute: %02d", ae.minute);
                    else ui_canvas.printf("Enabled: %s", ae.enabled ? "YES" : "NO");
                }
                ui_canvas.setTextColor(muted, panel);
                ui_canvas.setCursor(24, 110);
                ui_canvas.printf("%s  ,/:field  `:save", af_hints[alarm_setup_field]);
            }
        } else if (overlay_mode == OVERLAY_TIMER_SETUP) {
            if (alarm_setup_entry_index < timer_count) {
                TimerEntry& te = timer_entries[alarm_setup_entry_index];
                uint32_t th = te.duration_seconds / 3600;
                uint32_t tm2 = (te.duration_seconds % 3600) / 60;
                uint32_t ts2 = te.duration_seconds % 60;
                ui_canvas.setCursor(24, 40);
                ui_canvas.printf("TIMER SETUP (#%d)", alarm_setup_entry_index + 1);
                static const char* tf_names[4] = {"Name", "Hours", "Minutes", "Seconds"};
                static const char* tf_hints[4] = {"ENT:edit name", ";/.:change", ";/.:change", ";/.:change"};
                for (uint8_t i = 0; i < 4; i++) {
                    uint16_t rbg = (i == alarm_setup_field) ? accent : bg;
                    uint16_t rfg = (i == alarm_setup_field) ? BLACK : text;
                    ui_canvas.fillRoundRect(24, 54 + i * 14, 192, 12, 2, rbg);
                    ui_canvas.setTextColor(rfg, rbg);
                    ui_canvas.setCursor(28, 56 + i * 14);
                    if (i == 0) ui_canvas.printf("Name: %.16s", te.name[0] ? te.name : "(none)");
                    else if (i == 1) ui_canvas.printf("Hours: %02lu", (unsigned long)th);
                    else if (i == 2) ui_canvas.printf("Minutes: %02lu", (unsigned long)tm2);
                    else ui_canvas.printf("Seconds: %02lu", (unsigned long)ts2);
                }
                ui_canvas.setTextColor(muted, panel);
                ui_canvas.setCursor(24, 110);
                ui_canvas.printf("%s  ,/:field  `:save", tf_hints[alarm_setup_field]);
            }
        }
        ui_canvas.pushSprite(0, 0);
        return;
    }

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
        else if (input_purpose == INPUT_SHOP_ITEM_NAME) ui_canvas.println("Edit Item Name:");
        else if (input_purpose == INPUT_SHOP_ITEM_DESC) ui_canvas.println("Edit Item Description:");
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
        uint16_t count = task_manager.getVisibleTaskCount();
        ui_canvas.printf("Tasks:%d selected:%d\n", count, selected_task_index + 1);
        if (count > 0) {
            const Task* t = task_manager.getVisibleTaskByIndex(selected_task_index < count ? selected_task_index : 0);
            ui_canvas.printf("%s\n", t->name);
            ui_canvas.printf("D:%d U:%d F:%d\n", t->difficulty, t->urgency, t->fear);
            ui_canvas.printf("State:%s\n", taskStateLabel(*t).c_str());
            drawTaskStateStamp(*t, 176, 42);
        }
    } else if (current_screen == UI_TASKS) {
        normalizeTaskSelection();
        const Task* t = selectedTask();
        if (!t) {
            ui_canvas.println("No tasks. Press N");
        } else {
            ui_canvas.printf("Task[%d]: %s\n", selected_task_index + 1, truncateUiText(t->name, 22).c_str());
            if (!task_edit_mode) {
                drawTaskStateStamp(*t, 180, 40);
                ui_canvas.printf("XP:%lu  Repeat:%d\n", (unsigned long)t->calculateXP(), t->repetition);
                ui_canvas.printf("D:%d U:%d F:%d\n", t->difficulty, t->urgency, t->fear);
                ui_canvas.printf("State:%s\n", taskStateLabel(*t).c_str());
                ui_canvas.printf("Dur:%s\n", formatDurationValue(t->duration_seconds).c_str());
                ui_canvas.printf("Due:%s\n", truncateUiText(formatDueDateValue(t->due_date).c_str(), 20).c_str());
                ui_canvas.printf("Skills:%s\n", taskLinkedSkillsLabel(*t).c_str());
                ui_canvas.printf("Reward:%s\n", taskRewardItemLabel(*t).c_str());
            } else {
                static const char* field_names[] = {
                    "Difficulty", "Urgency", "Fear", "Repeat", "Duration",
                    "Due Date", "Money", "Skills", "Item", "Item Qty"
                };
                String values[10];
                values[TASK_EDIT_DIFFICULTY] = String(t->difficulty) + "%";
                values[TASK_EDIT_URGENCY] = String(t->urgency) + "%";
                values[TASK_EDIT_FEAR] = String(t->fear) + "%";
                values[TASK_EDIT_REPETITION] = String(t->repetition);
                values[TASK_EDIT_DURATION] = formatDurationValue(t->duration_seconds);
                values[TASK_EDIT_DUE_DATE] = formatDueDateValue(t->due_date);
                values[TASK_EDIT_REWARD_MONEY] = String("$") + String((unsigned long)t->reward_money);
                values[TASK_EDIT_LINKED_SKILLS] = taskLinkedSkillsLabel(*t);
                values[TASK_EDIT_REWARD_ITEM] = taskRewardItemLabel(*t);
                values[TASK_EDIT_REWARD_ITEM_QTY] = (t->reward_item_id > 0) ? String(t->reward_item_quantity) : String("-");
                uint8_t start_field = ((int)selected_task_field > 2) ? (uint8_t)selected_task_field - 2 : 0;
                uint8_t drawn = 0;
                for (uint8_t idx = start_field; idx <= TASK_EDIT_REWARD_ITEM_QTY && drawn < 6; idx++) {
                    if (idx == TASK_EDIT_REWARD_ITEM_QTY && t->reward_item_id == 0) continue;
                    uint16_t bg_col = (idx == selected_task_field) ? accent : panel;
                    uint16_t fg_col = (idx == selected_task_field) ? BLACK : text;
                    int y = 54 + drawn * 11;
                    ui_canvas.fillRoundRect(4, y - 1, 232, 10, 2, bg_col);
                    ui_canvas.setTextColor(fg_col, bg_col);
                    ui_canvas.setCursor(8, y + 1);
                    ui_canvas.printf("%s", field_names[idx]);
                    ui_canvas.setCursor(96, y + 1);
                    ui_canvas.print(truncateUiText(values[idx].c_str(), 20));
                    drawn++;
                }
            }
        }
        ui_canvas.println(task_edit_mode ? "Space=open editor" : "Enter=done, E=toggle edit");
    } else if (current_screen == UI_SKILLS) {
        normalizeSkillSelection();
        const SkillCategory* cat = selectedCategory();
        const Skill* sk = selectedSkillInSelectedCategory();
        if (skill_edit_mode) {
            ui_canvas.setTextColor(accent, bg);
            ui_canvas.println("EDIT MODE  SPC=field menu");
            ui_canvas.setTextColor(text, bg);
        }
        // Header: category name + skill count
        if (cat) {
            ui_canvas.printf("Cat[%d/%d]: %s\n",
                selected_category_index + 1,
                skill_system.getActiveCategoryCount(),
                truncateUiText(cat->name, 18).c_str());
        } else {
            ui_canvas.println("No categories. Press A");
        }
        // Skill list — scrollable window of 4 rows
        if (cat) {
            uint16_t sk_count = skill_system.getActiveSkillCountInCategory(cat->id);
            uint8_t window = 4;
            uint8_t start_sk = (selected_skill_index > 1) ? (uint8_t)(selected_skill_index - 1) : 0;
            for (uint8_t i = 0; i < window; i++) {
                uint16_t idx = start_sk + i;
                if (idx >= sk_count) break;
                const Skill* s = skill_system.getSkillInCategoryByActiveIndex(cat->id, idx);
                if (!s) continue;
                uint16_t row_bg = (idx == selected_skill_index) ? accent : panel;
                uint16_t row_fg = (idx == selected_skill_index) ? BLACK : text;
                int y = 54 + i * 11;
                ui_canvas.fillRoundRect(4, y - 1, 154, 10, 2, row_bg);
                ui_canvas.setTextColor(row_fg, row_bg);
                ui_canvas.setCursor(8, y + 1);
                uint8_t xp_pct = skill_system.getSkillXPProgress(s->id);
                ui_canvas.printf("%s Lv%d (%d%%)", truncateUiText(s->name, 13).c_str(), s->level, xp_pct);
            }
            ui_canvas.setTextColor(text, bg);
            // XP bar for selected skill
            if (sk) {
                uint8_t xp_pct = skill_system.getSkillXPProgress(sk->id);
                uint32_t xp_need = skill_system.getXPForNextSkillLevel(sk->id);
                int bar_y = 102;
                ui_canvas.fillRect(4, bar_y, 154, 6, panel);
                ui_canvas.fillRect(4, bar_y, (int)(154 * xp_pct / 100), 6, accent);
                ui_canvas.setTextColor(muted, bg);
                ui_canvas.setCursor(4, bar_y + 8);
                ui_canvas.printf("XP %lu/%lu", (unsigned long)sk->current_xp, (unsigned long)xp_need);
            }
        }
        ui_canvas.setTextColor(muted, bg);
        ui_canvas.setCursor(4, 124);
        ui_canvas.print(skill_edit_mode ? "E:exit edit  SPC:field menu" : "E:edit  SPC:quick menu");
        ui_canvas.setTextColor(text, bg);
        // Spider chart on the right side — 4 modes
        static const char* chart_labels[] = {"Lvl", "TXP", "SLv", "STX"};
        ui_canvas.setTextColor(muted, bg);
        ui_canvas.setCursor(166, 34);
        ui_canvas.printf("[%s]", chart_labels[skill_spider_mode]);
        if (skill_spider_mode == 0) {
            drawSkillsSpiderChart(200, 75, 30);
        } else if (skill_spider_mode == 1) {
            drawSpiderCategoryTotalXP(200, 75, 30);
        } else if (skill_spider_mode == 2 && cat) {
            drawSkillsSpiderChartCategory(200, 75, 30, cat->id);
        } else if (skill_spider_mode == 3 && cat) {
            drawSpiderSkillsTotalXP(200, 75, 30, cat->id);
        } else {
            drawSkillsSpiderChart(200, 75, 30);
        }
    } else if (current_screen == UI_SHOP) {
        normalizeShopSelection();
        const ShopItem* it = selectedShopItem();
        const CraftRecipe* rc = selectedRecipe();
        if (shop_edit_mode) {
            ui_canvas.setTextColor(accent, bg);
            ui_canvas.println("EDIT MODE  SPC=field menu");
            ui_canvas.setTextColor(text, bg);
        } else {
            ui_canvas.printf("Focus:%s\n", shop_focus_items ? "items" : "recipes");
        }
        if (shop_focus_items && it) {
            ui_canvas.printf("Item: %s\n", truncateUiText(it->name, 20).c_str());
            ui_canvas.printf("$%ld  buy_limit:%d\n", (long)it->current_price, (int)it->buy_limit);
            ui_canvas.printf("Effect:%d val:%lu\n", (int)it->effect_type, (unsigned long)it->effect_value);
            ui_canvas.printf("Desc: %s\n", truncateUiText(it->description, 22).c_str());
        } else if (!shop_focus_items && rc) {
            const ShopItem* out_it = shop_system.getItem(rc->output_item_id);
            ui_canvas.printf("Recipe -> %s x%d\n", out_it ? truncateUiText(out_it->name, 14).c_str() : "?", rc->output_quantity);
            for (uint8_t s = 0; s < rc->input_count && s < 3; s++) {
                const ShopItem* in_it = shop_system.getItem(rc->inputs[s].item_id);
                ui_canvas.printf("  In%d: %s x%d\n", s + 1,
                    in_it ? truncateUiText(in_it->name, 12).c_str() : "?",
                    rc->inputs[s].quantity);
            }
        }
        if (!shop_edit_mode) {
            ui_canvas.setTextColor(muted, bg);
            ui_canvas.println(shop_focus_items ? "ENT=buy `=use E=edit" : "ENT=craft E=edit");
        }
    } else if (current_screen == UI_PROFILES) {
        refreshProfiles();
        String active_name = save_load_system.getActiveProfileName();

        // Profile list on left (up to 5 rows)
        ui_canvas.setTextColor(muted, bg);
        ui_canvas.setCursor(4, 40);
        ui_canvas.printf("PROFILES (%d)", profile_count);

        uint8_t list_window = 5;
        uint8_t list_start = (selected_profile_index > 2) ? (uint8_t)(selected_profile_index - 2) : 0;
        for (uint8_t i = 0; i < list_window && (list_start + i) < profile_count; i++) {
            uint8_t idx = list_start + i;
            bool is_selected = (idx == selected_profile_index);
            bool is_active = (profile_list[idx].name == active_name);
            uint16_t row_bg = is_selected ? accent : panel;
            uint16_t row_fg = is_selected ? BLACK : (is_active ? yellow : text);
            int y = 50 + i * 12;
            ui_canvas.fillRoundRect(2, y, 150, 11, 2, row_bg);
            ui_canvas.setTextColor(row_fg, row_bg);
            ui_canvas.setCursor(6, y + 2);
            ui_canvas.printf("%c %.16s", is_active ? '*' : ' ', profile_list[idx].name.c_str());
        }

        // Right side: details for selected profile
        if (profile_count > 0 && selected_profile_index < profile_count) {
            const ProfileInfo& psel = profile_list[selected_profile_index];
            bool sel_is_active = (psel.name == active_name);
            ui_canvas.setTextColor(yellow, bg);
            ui_canvas.setCursor(158, 40);
            ui_canvas.printf("%.14s", truncateUiText(psel.name.c_str(), 12).c_str());
            ui_canvas.setTextColor(sel_is_active ? 0x07E0 : muted, bg);
            ui_canvas.setCursor(158, 50);
            ui_canvas.println(sel_is_active ? "ACTIVE" : "inactive");
            ui_canvas.setTextColor(text, bg);
            if (sel_is_active) {
                ui_canvas.setCursor(158, 62);
                ui_canvas.printf("Lv%d HP%d%%\n", level_system.getLevel(), health_system.getHealth());
            }
            ui_canvas.setCursor(158, sel_is_active ? 74 : 62);
            ui_canvas.setTextColor(muted, bg);
            ui_canvas.printf("Done:%lu\n", (unsigned long)psel.tasks_completed);
            ui_canvas.printf("Fail:%lu\n", (unsigned long)psel.tasks_failed);
            ui_canvas.printf("+$%ld\n", (long)psel.money_gained);
            ui_canvas.printf("-$%ld\n", (long)psel.money_spent);
        }

        ui_canvas.setTextColor(muted, bg);
        ui_canvas.setCursor(4, 117);
        ui_canvas.print("N=new  ENT=load  DEL=delete");
    } else if (current_screen == UI_INVENTORY) {
        // Large money display at top
        ui_canvas.setTextSize(2);
        ui_canvas.setTextColor(yellow, bg);
        ui_canvas.printf("$%ld\n", (long)player_money);
        ui_canvas.setTextSize(1);
        ui_canvas.setTextColor(text, bg);
        // Build owned item list
        uint16_t inv_display_count = 0;
        uint16_t inv_found = 0;
        for (uint16_t i = 0; i < shop_system.getItemCountRaw(); i++) {
            const ShopItem* inv_it = shop_system.itemsRaw() + i;
            if (!inv_it->active) continue;
            uint16_t qty = shop_system.getInventoryQuantity(inv_it->id);
            if (qty == 0) continue;
            if (inv_found >= selected_inventory_item_index && inv_display_count < 4) {
                uint16_t row_bg = (inv_found == selected_inventory_item_index) ? accent : panel;
                uint16_t row_fg = (inv_found == selected_inventory_item_index) ? BLACK : text;
                int y = 50 + inv_display_count * 12;
                ui_canvas.fillRoundRect(4, y, 232, 11, 2, row_bg);
                ui_canvas.setTextColor(row_fg, row_bg);
                ui_canvas.setCursor(8, y + 2);
                ui_canvas.printf("%s", truncateUiText(inv_it->name, 20).c_str());
                ui_canvas.setCursor(190, y + 2);
                ui_canvas.printf("x%d", qty);
                inv_display_count++;
            }
            inv_found++;
        }
        if (inv_found == 0) {
            ui_canvas.setTextColor(muted, bg);
            ui_canvas.println("No items in inventory.");
        }
        ui_canvas.setTextColor(muted, bg);
        ui_canvas.setCursor(6, 106);
        ui_canvas.println("ENT=use item");
    } else if (current_screen == UI_SAVE_LOAD) {
        ui_canvas.printf("Backend:%s\n", save_load_system.getBackendName());
        ui_canvas.printf("Option:%d\n", save_menu_index + 1);
        ui_canvas.println("1 save profile");
        ui_canvas.println("2 load profile");
        ui_canvas.println("3 save shared shop");
        ui_canvas.println("4 load shared shop");
        ui_canvas.println("5 clear skills save");
    } else if (current_screen == UI_SETTINGS) {
        static const char* sett_names[] = {"WiFi", "Timezone", "Time", "Health", "RandTask", "Feedback", "SkillXP"};
        ui_canvas.printf("SETTINGS [%s]\n", sett_names[(int)sett_section]);

        if (sett_section == SETT_WIFI) {
            // Show two sub-lists: saved (left/top) and scanned (right/bottom)
            // Top half: saved networks
            {
                uint16_t hcol = sett_wifi_focus_saved ? accent : panel;
                uint16_t htxt = sett_wifi_focus_saved ? BLACK : muted;
                ui_canvas.fillRoundRect(2, 46, 236, 10, 2, hcol);
                ui_canvas.setTextColor(htxt, hcol);
                ui_canvas.setCursor(6, 48);
                uint8_t svc = settings_system.getSavedWiFiCount();
                ui_canvas.printf("Saved (%d)  LEFT=switch", (int)svc);
            }
            uint8_t svc = settings_system.getSavedWiFiCount();
            uint8_t sv_start = (sett_wifi_saved_sel > 1) ? sett_wifi_saved_sel - 1 : 0;
            uint8_t drawn_sv = 0;
            for (uint8_t i = sv_start; i < svc && drawn_sv < 2; i++, drawn_sv++) {
                const SavedWiFiCred* cred = settings_system.getSavedWiFi(i);
                if (!cred) continue;
                bool sel = (sett_wifi_focus_saved && i == sett_wifi_saved_sel);
                uint16_t rb = sel ? accent : panel;
                uint16_t rt = sel ? BLACK : text;
                int y = 57 + drawn_sv * 10;
                ui_canvas.fillRoundRect(2, y - 1, 236, 9, 2, rb);
                ui_canvas.setTextColor(rt, rb);
                ui_canvas.setCursor(6, y + 1);
                bool connected = settings_system.isWiFiConnected() &&
                    strcmp(settings_system.getConnectedSSID(), cred->ssid) == 0;
                ui_canvas.printf("%s%s", connected ? "*" : " ", truncateUiText(cred->ssid, 19).c_str());
            }
            if (svc == 0) {
                ui_canvas.setTextColor(muted, bg);
                ui_canvas.setCursor(6, 58);
                ui_canvas.print("(none saved)");
            }
            // Middle divider: status
            {
                uint16_t statcol = settings_system.isWiFiConnected() ? 0x07E0 : 0xF800;
                ui_canvas.fillRoundRect(2, 78, 236, 9, 2, 0x1082);
                ui_canvas.setTextColor(statcol, 0x1082);
                ui_canvas.setCursor(6, 80);
                if (settings_system.isWiFiConnected())
                    ui_canvas.printf("Connected: %.22s", settings_system.getConnectedSSID());
                else
                    ui_canvas.print("Not connected");
            }
            // Bottom half: scanned networks
            {
                uint16_t hcol = !sett_wifi_focus_saved ? accent : panel;
                uint16_t htxt = !sett_wifi_focus_saved ? BLACK : muted;
                ui_canvas.fillRoundRect(2, 88, 236, 9, 2, hcol);
                ui_canvas.setTextColor(htxt, hcol);
                ui_canvas.setCursor(6, 90);
                uint8_t cnt = settings_system.getScannedCount();
                ui_canvas.printf("Available (%d)  DEL=scan", (int)cnt);
            }
            uint8_t cnt = settings_system.getScannedCount();
            uint8_t sc_start = (sett_wifi_sel > 0) ? sett_wifi_sel - 0 : 0;
            if (cnt == 0) {
                ui_canvas.setTextColor(muted, bg);
                ui_canvas.setCursor(6, 99);
                ui_canvas.print("(none) DEL to scan");
            } else {
                uint8_t drawn_sc = 0;
                for (uint8_t i = sc_start; i < cnt && drawn_sc < 2; i++, drawn_sc++) {
                    const WiFiNetwork* n = settings_system.getScannedNetwork(i);
                    if (!n) continue;
                    bool sel = (!sett_wifi_focus_saved && i == sett_wifi_sel);
                    uint16_t rb = sel ? accent : panel;
                    uint16_t rt = sel ? BLACK : text;
                    int y = 98 + drawn_sc * 10;
                    ui_canvas.fillRoundRect(2, y - 1, 236, 9, 2, rb);
                    ui_canvas.setTextColor(rt, rb);
                    ui_canvas.setCursor(6, y + 1);
                    ui_canvas.printf("%s %ddBm%s",
                        truncateUiText(n->ssid, 15).c_str(),
                        (int)n->rssi,
                        n->has_password ? " *" : "");
                }
            }
        } else if (sett_section == SETT_TIMEZONE) {
            const GlobalSettings& cfg = settings_system.settings();
            ui_canvas.printf("UTC%+d DST:%s\n", (int)cfg.timezone_offset, cfg.timezone_dst ? "ON" : "OFF");
            ui_canvas.printf("Date:%s\n", cfg.date_format_us ? "MM/DD/YYYY" : "DD/MM/YYYY");
            ui_canvas.println(";/. change");
            ui_canvas.println("Enter=NTP  D:DST  F:fmt");
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
            ui_canvas.printf("Volume:%d%%\n", (int)cfg.audio_volume);
            ui_canvas.println("Enter toggle visual");
            ui_canvas.println("`: toggle audio  ;/. vol (0-100)");
        } else if (sett_section == SETT_SKILL_XP) {
            const GlobalSettings& cfg = settings_system.settings();
            // Draw ratio bar
            ui_canvas.setTextColor(text, bg);
            ui_canvas.printf("XP Ratio: %d%%\n", (int)cfg.skill_xp_ratio);
            int bar_y = 66;
            ui_canvas.fillRect(4, bar_y, 200, 8, panel);
            ui_canvas.fillRect(4, bar_y, (cfg.skill_xp_ratio * 200) / 100, 8, accent);
            ui_canvas.setTextColor(muted, bg);
            ui_canvas.setCursor(4, bar_y + 10);
            ui_canvas.printf("Split: %s\n", cfg.skill_xp_split ? "ON (div among skills)" : "OFF (each skill full)");
            ui_canvas.println(";/. ratio  Enter=toggle split");
            ui_canvas.println("Example: 500XP @50% = 250/skill");
            if (cfg.skill_xp_split)
                ui_canvas.printf("With split %d skills: %d each\n",
                    2, (int)(500 * cfg.skill_xp_ratio / 100 / 2));
        }
    } else if (current_screen == UI_HELP) {
        ui_canvas.setTextColor(text, bg);
        ui_canvas.println("M5TDGamify - Quick Guide");
        ui_canvas.println("TAB: screen menu  H: help overlay");
        ui_canvas.println("L: manual health  DASH: F=fail task");
        ui_canvas.println("TASKS: manage, E=edit, N=new");
        ui_canvas.println("SKILLS: SPC=cycle chart / E+SPC=edit menu");
        ui_canvas.println("SHOP: buy/craft items");
        ui_canvas.println("ALARMS: A=add alarm, T=add timer");
        ui_canvas.println("  SPC=detailed setup for selected");
    } else if (current_screen == UI_ALARMS) {
        // Left pane: alarms; right pane: timers
        // Divider
        ui_canvas.drawFastVLine(120, 35, 88, 0x4208);
        // Left pane header
        {
            uint16_t hcol = alarm_focus_timers ? panel : accent;
            uint16_t htxt = alarm_focus_timers ? muted : BLACK;
            ui_canvas.fillRoundRect(2, 35, 116, 10, 2, hcol);
            ui_canvas.setTextColor(htxt, hcol);
            ui_canvas.setCursor(6, 37);
            ui_canvas.printf("ALARMS (%d/%d)", alarm_count, MAX_ALARMS);
        }
        // Alarm rows
        time_t cur_t = time_sync.getCurrentTime();
        struct tm ti_cur; localtime_r(&cur_t, &ti_cur);
        for (uint8_t i = 0; i < alarm_count && i < 6; i++) {
            bool sel = (!alarm_focus_timers && i == alarm_sel_index);
            uint16_t rowbg = sel ? accent : panel;
            uint16_t rowtxt = sel ? BLACK : (alarm_entries[i].enabled ? text : muted);
            int y = 47 + i * 11;
            ui_canvas.fillRoundRect(2, y - 1, 116, 10, 2, rowbg);
            ui_canvas.setTextColor(rowtxt, rowbg);
            ui_canvas.setCursor(6, y + 1);
            ui_canvas.printf("%02d:%02d %s %s", alarm_entries[i].hour, alarm_entries[i].minute,
                             alarm_entries[i].enabled ? "ON " : "OFF",
                             alarm_entries[i].name[0] ? alarm_entries[i].name : "Alarm");
        }
        if (alarm_count == 0) {
            ui_canvas.setTextColor(muted, bg);
            ui_canvas.setCursor(6, 50);
            ui_canvas.print("No alarms. Press A");
        }
        // Right pane header
        {
            uint16_t hcol = alarm_focus_timers ? accent : panel;
            uint16_t htxt = alarm_focus_timers ? BLACK : muted;
            ui_canvas.fillRoundRect(122, 35, 116, 10, 2, hcol);
            ui_canvas.setTextColor(htxt, hcol);
            ui_canvas.setCursor(126, 37);
            ui_canvas.printf("TIMERS (%d/%d)", timer_count, MAX_TIMERS);
        }
        // Timer rows
        for (uint8_t i = 0; i < timer_count && i < 6; i++) {
            bool sel = (alarm_focus_timers && i == timer_sel_index);
            uint16_t rowbg = sel ? accent : panel;
            uint16_t rowtxt = sel ? BLACK : (timer_entries[i].running ? text : muted);
            int y = 47 + i * 11;
            ui_canvas.fillRoundRect(122, y - 1, 116, 10, 2, rowbg);
            ui_canvas.setTextColor(rowtxt, rowbg);
            ui_canvas.setCursor(126, y + 1);
            // Compute remaining (state changes handled in game loop, not here)
            uint32_t remaining = timer_entries[i].duration_seconds;
            if (timer_entries[i].running && timer_entries[i].start_unix_time != 0) {
                uint32_t elapsed = (cur_t > (time_t)timer_entries[i].start_unix_time) ?
                                   (uint32_t)(cur_t - timer_entries[i].start_unix_time) : 0;
                remaining = (elapsed < timer_entries[i].duration_seconds) ?
                            (timer_entries[i].duration_seconds - elapsed) : 0;
            }
            uint32_t rh = remaining / 3600, rm = (remaining % 3600) / 60, rs = remaining % 60;
            char tstr[16];
            if (rh > 0) snprintf(tstr, sizeof(tstr), "%u:%02u:%02u", (unsigned)rh, (unsigned)rm, (unsigned)rs);
            else snprintf(tstr, sizeof(tstr), "%u:%02u", (unsigned)rm, (unsigned)rs);
            const char* tname = timer_entries[i].name[0] ? timer_entries[i].name : "Timer";
            ui_canvas.printf("%.5s %s %s", tname, tstr, timer_entries[i].running ? ">" : "|");
        }
        if (timer_count == 0) {
            ui_canvas.setTextColor(muted, bg);
            ui_canvas.setCursor(126, 50);
            ui_canvas.print("No timers. Press T");
        }
        ui_canvas.setTextColor(muted, bg);
        ui_canvas.setCursor(2, 116);
        ui_canvas.print("LEFT:switch  ENT:toggle  DEL:del");
    }

    if (millis() < status_line_until) {
        // Draw status in a visible location (upper area)
        ui_canvas.fillRoundRect(2, 104, 236, 18, 3, accent);
        ui_canvas.setTextColor(0xFFFF, accent);
        ui_canvas.setCursor(6, 109);
        ui_canvas.setTextSize(1);
        ui_canvas.println(status_line);
    }

    // Help overlay — drawn on top of everything else
    if (help_overlay_active) {
        ui_canvas.fillRoundRect(4, 22, 232, 100, 4, 0x0841);
        ui_canvas.drawRoundRect(4, 22, 232, 100, 4, accent);
        ui_canvas.setTextColor(accent, 0x0841);
        ui_canvas.setCursor(8, 26);
        ui_canvas.print("HELP  [H to close]");
        ui_canvas.setTextColor(text, 0x0841);
        ui_canvas.setCursor(8, 36);
        if (current_screen == UI_DASHBOARD) {
            ui_canvas.println("DASHBOARD: overview & health");
            ui_canvas.println("F=fail task  L=set health");
            ui_canvas.println("TAB=menu  [/]=brightness  G=saver");
        } else if (current_screen == UI_TASKS) {
            ui_canvas.println("TASKS: view & edit your tasks");
            ui_canvas.println("N=new  E=toggle edit  D=delete");
            ui_canvas.println("ENT=done  SPC=open editor");
            ui_canvas.println("F=fail task  TAB=screen menu");
        } else if (current_screen == UI_SKILLS) {
            ui_canvas.println("SKILLS: track skill progress");
            ui_canvas.println("SPC (no edit)=cycle spider chart");
            ui_canvas.println("E=edit mode  SPC (edit)=edit menu");
            ui_canvas.println("Edit menu: add/remove/edit/XP");
        } else if (current_screen == UI_SHOP) {
            ui_canvas.println("SHOP: buy & craft items");
            ui_canvas.println(",/=focus  Z=setup mode");
            ui_canvas.println("N=add item  R=add recipe");
            ui_canvas.println("ENT=buy/craft  E=edit mode");
        } else if (current_screen == UI_SETTINGS) {
            ui_canvas.println("SETTINGS sections:");
            ui_canvas.println("WiFi: LEFT=saved/scan, ENT=connect");
            ui_canvas.println("Skill XP: ratio% + split option");
            ui_canvas.println(",/=prev/next section");
        } else if (current_screen == UI_ALARMS) {
            ui_canvas.println("ALARMS & TIMERS");
            ui_canvas.println("LEFT=switch pane  A=add alarm");
            ui_canvas.println("T=add timer  SPC=setup selected");
            ui_canvas.println("ENT=toggle  DEL=remove");
        } else if (current_screen == UI_PROFILES) {
            ui_canvas.println("PROFILES: manage game profiles");
            ui_canvas.println("N=new  ENT=load  DEL=delete");
        } else if (current_screen == UI_INVENTORY) {
            ui_canvas.println("INVENTORY: view owned items");
        } else if (current_screen == UI_SAVE_LOAD) {
            ui_canvas.println("SAVE/LOAD: manual data control");
        } else {
            ui_canvas.println("Press H to close this overlay");
            ui_canvas.println("Available on every screen.");
        }
    }

    // Alarm/timer ringing overlay — covers full display until dismissed
    if (alarm_ringing) {
        static const char* key_names[] = {"OPT", "ALT", "CTRL", "FN"};
        const uint16_t ring_bg = 0x6000;  // dark red
        ui_canvas.fillRoundRect(4, 20, 232, 118, 6, ring_bg);
        ui_canvas.drawRoundRect(4, 20, 232, 118, 6, 0xF800);
        ui_canvas.setTextColor(0xFFFF, ring_bg);
        ui_canvas.setTextSize(2);
        ui_canvas.setCursor(10, 28);
        if (alarm_ringing_is_timer) {
            const char* tname = (alarm_ringing_index < timer_count && timer_entries[alarm_ringing_index].name[0])
                                ? timer_entries[alarm_ringing_index].name : "Timer";
            ui_canvas.printf("TIMER: %.9s", tname);
        } else {
            const char* aname = (alarm_ringing_index < alarm_count && alarm_entries[alarm_ringing_index].name[0])
                                ? alarm_entries[alarm_ringing_index].name : "Alarm";
            ui_canvas.printf("ALARM: %.9s", aname);
        }
        ui_canvas.setTextSize(1);
        ui_canvas.setTextColor(0xFFE0, ring_bg);
        ui_canvas.setCursor(10, 58);
        ui_canvas.printf("Press  [%s]  to dismiss", key_names[alarm_dismiss_key]);
        if (!alarm_ringing_is_timer) {
            ui_canvas.setTextColor(0xBDF7, ring_bg);
            ui_canvas.setCursor(10, 74);
            ui_canvas.println("Any other key = 5 min snooze");
        }
        ui_canvas.setTextColor(0xF800, ring_bg);
        ui_canvas.setTextSize(2);
        // Pulsing indicator using millis
        uint32_t pulse = (millis() / 400) % 2;
        ui_canvas.setCursor(10, 94);
        ui_canvas.print(pulse ? "** RINGING **" : "             ");
        ui_canvas.setTextSize(1);
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
        time_sync.setDaylightSavingEnabled(cfg.timezone_dst);
        time_sync.setDateFormatUS(cfg.date_format_us);
        if (cfg.health_time_based) {
            health_system.setTimeBasedMode();
            health_system.setTimeBasedCycle(cfg.health_wake_hour, cfg.health_sleep_hour);
        } else {
            health_system.setManualMode();
        }
        // Populate settings time editor with current values from saved TZ
        sett_time_vals[0] = 0; sett_time_vals[1] = 0; sett_time_vals[2] = 0;
        sett_time_vals[3] = 1; sett_time_vals[4] = 1; sett_time_vals[5] = 26;

        // Try to restore time from RTC first (no network needed)
        #if defined(ARDUINO)
        auto rtc_dt = M5.Rtc.getDateTime();
        if (rtc_dt.date.year >= 2024) {
            struct tm tm_rtc = {};
            tm_rtc.tm_year = rtc_dt.date.year - 1900;
            tm_rtc.tm_mon  = rtc_dt.date.month - 1;
            tm_rtc.tm_mday = rtc_dt.date.date;
            tm_rtc.tm_hour = rtc_dt.time.hours;
            tm_rtc.tm_min  = rtc_dt.time.minutes;
            tm_rtc.tm_sec  = rtc_dt.time.seconds;
            time_t rtc_time = mktime(&tm_rtc);
            struct timeval tv_rtc = { rtc_time, 0 };
            settimeofday(&tv_rtc, nullptr);
        }
        #endif

        // Try auto WiFi reconnect — use best available network from saved list
        bool wifi_ok = settings_system.connectToBestAvailableWiFi(6000);
        if (!wifi_ok && cfg.wifi_ssid[0] != '\0') {
            // Fall back to legacy single credential
            wifi_ok = settings_system.connectWiFi(cfg.wifi_ssid, cfg.wifi_password, 6000);
        }
        if (wifi_ok) {
            // Sync NTP and write back to RTC
            const char* ssid = settings_system.getConnectedSSID();
            const SavedWiFiCred* cred = nullptr;
            for (uint8_t i = 0; i < settings_system.getSavedWiFiCount(); i++) {
                const SavedWiFiCred* c = settings_system.getSavedWiFi(i);
                if (c && strcmp(c->ssid, ssid) == 0) { cred = c; break; }
            }
            const char* pass = cred ? cred->password : cfg.wifi_password;
            if (time_sync.syncWithWiFi(ssid, pass)) {
                #if defined(ARDUINO)
                time_t now_sync = time_sync.getCurrentTime();
                struct tm ti_sync;
                localtime_r(&now_sync, &ti_sync);
                m5::rtc_datetime_t dt_sync;
                dt_sync.date.year = ti_sync.tm_year + 1900;
                dt_sync.date.month = ti_sync.tm_mon + 1;
                dt_sync.date.date = ti_sync.tm_mday;
                dt_sync.time.hours = ti_sync.tm_hour;
                dt_sync.time.minutes = ti_sync.tm_min;
                dt_sync.time.seconds = ti_sync.tm_sec;
                M5.Rtc.setDateTime(dt_sync);
                #endif
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
        cfg.timezone_dst = false;
        cfg.date_format_us = false;
        time_sync.setTimezoneOffset(cfg.timezone_offset);
        time_sync.setDaylightSavingEnabled(cfg.timezone_dst);
        time_sync.setDateFormatUS(cfg.date_format_us);
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

    // Load alarms and timers
    if (storage_ready) {
        save_load_system.loadAlarmsAndTimers(alarm_entries, alarm_count, MAX_ALARMS,
                                             timer_entries, timer_count, MAX_TIMERS);
    }

    setStatus("Ready");
}

void loop() {
    M5Cardputer.update();
    health_system.setCurrentTime(time_sync.getCurrentTime());

    // Check alarms — fires ringing overlay instead of single-shot beep
    {
        time_t now_alarm = time_sync.getCurrentTime();
        struct tm ta; localtime_r(&now_alarm, &ta);
        bool alarm_dirty = false;
        for (uint8_t i = 0; i < alarm_count; i++) {
            if (!alarm_entries[i].active || !alarm_entries[i].enabled) continue;
            if (ta.tm_hour == alarm_entries[i].hour && ta.tm_min == alarm_entries[i].minute) {
                if (!alarm_entries[i].triggered_today) {
                    alarm_entries[i].triggered_today = true;
                    alarm_dirty = true;
                    if (!alarm_ringing && alarm_snooze_until_unix == 0) {
                        alarm_ringing = true;
                        alarm_ringing_is_timer = false;
                        alarm_ringing_index = i;
                        alarm_dismiss_key = (uint8_t)(esp_random() % 4);
                        alarm_last_buzz_ms = 0;
                        overlay_mode = OVERLAY_ALARM_RINGING;
                        screensaver_active = false;
                        screen_off = false;
                        M5Cardputer.Display.setBrightness(screen_brightness);
                    }
                }
            } else {
                if (alarm_entries[i].triggered_today) {
                    alarm_entries[i].triggered_today = false;
                    alarm_dirty = true;
                }
            }
        }
        if (alarm_dirty && storage_ready) {
            save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
        }
    }

    // Check timer expiry — start ringing when a running timer reaches zero
    {
        time_t now_t = time_sync.getCurrentTime();
        bool timer_dirty = false;
        for (uint8_t i = 0; i < timer_count; i++) {
            if (!timer_entries[i].active || !timer_entries[i].running) continue;
            if (timer_entries[i].start_unix_time == 0) continue;
            uint32_t elapsed = (now_t > (time_t)timer_entries[i].start_unix_time)
                               ? (uint32_t)(now_t - timer_entries[i].start_unix_time) : 0;
            if (elapsed >= timer_entries[i].duration_seconds) {
                timer_entries[i].running = false;
                timer_dirty = true;
                if (!alarm_ringing) {
                    alarm_ringing = true;
                    alarm_ringing_is_timer = true;
                    alarm_ringing_index = i;
                    alarm_dismiss_key = (uint8_t)(esp_random() % 4);
                    alarm_last_buzz_ms = 0;
                    overlay_mode = OVERLAY_ALARM_RINGING;
                    screensaver_active = false;
                    screen_off = false;
                    M5Cardputer.Display.setBrightness(screen_brightness);
                }
            }
        }
        if (timer_dirty && storage_ready) {
            save_load_system.saveAlarmsAndTimers(alarm_entries, alarm_count, timer_entries, timer_count);
        }
    }

    // Check snooze expiry — re-ring alarm after 5-minute snooze
    if (alarm_snooze_until_unix > 0) {
        time_t now_snooze = time_sync.getCurrentTime();
        if (now_snooze >= (time_t)alarm_snooze_until_unix) {
            alarm_snooze_until_unix = 0;
            alarm_ringing = true;
            alarm_dismiss_key = (uint8_t)(esp_random() % 4);
            alarm_last_buzz_ms = 0;
            overlay_mode = OVERLAY_ALARM_RINGING;
            screensaver_active = false;
            screen_off = false;
            M5Cardputer.Display.setBrightness(screen_brightness);
        }
    }

    // Buzz periodically while ringing (every ~1 second)
    if (alarm_ringing && millis() - alarm_last_buzz_ms >= 1000) {
        alarm_last_buzz_ms = millis();
        const GlobalSettings& acfg = settings_system.settings();
        if (acfg.audio_feedback_enabled) {
            uint8_t spk_vol = (uint8_t)((uint16_t)acfg.audio_volume * 255 / 100);
            M5Cardputer.Speaker.setVolume(spk_vol);
            M5Cardputer.Speaker.tone(880, 200);
            delay(250);
            M5Cardputer.Speaker.tone(880, 200);
        }
    }

    // G0 physical button: toggle screen on/off.
    if (M5Cardputer.BtnA.wasClicked()) {
        screen_off = !screen_off;
        if (screen_off) {
            M5Cardputer.Display.setBrightness(0);
        } else {
            M5Cardputer.Display.setBrightness(screen_brightness);
        }
    }
    if (screen_off) {
        delay(50);
        return;
    }

    // Screensaver mode: render continuously, any key exits.
    if (screensaver_active) {
        if (M5Cardputer.Keyboard.isChange()) {
            M5Cardputer.Keyboard.updateKeysState();
            auto& sk = M5Cardputer.Keyboard.keysState();
            if (sk.enter || sk.del || sk.word.size() > 0) {
                screensaver_active = false;
                setStatus("Screensaver OFF", 900);
            }
        }
        if (screensaver_active && millis() - last_ui_render_ms >= 33) {
            renderScreensaver();
            last_ui_render_ms = millis();
        }
        delay(20);
        return;
    }

    // Auto-apply manual health after 4 seconds of no input
    if (manual_health_active && millis() - manual_health_last_input_time > 4000) {
        uint8_t new_health = (manual_health_input_percent * 100) / 100;
        health_system.setManualHealth(new_health);
        saveAllProfileData();
        manual_health_active = false;
        setStatus("Health auto-set", 1500);
    }

    if ((overlay_mode == OVERLAY_PROFILE_DELETE || overlay_mode == OVERLAY_SHOP_CONFIRM_DELETE || overlay_mode == OVERLAY_SKILL_DELETE_CONFIRM) && millis() >= profile_delete_confirm_until) {
        overlay_mode = OVERLAY_NONE;
        setStatus("Delete cancelled", 1000);
    }

    if (M5Cardputer.Keyboard.isChange()) {
        M5Cardputer.Keyboard.updateKeysState();
        auto& ks = M5Cardputer.Keyboard.keysState();

        // TAB opens screen selector globally.
        bool tab_pressed = M5Cardputer.Keyboard.isKeyPressed(KEY_TAB);
        bool del_pressed = M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE);
        bool backtick_pressed = false;
        for (char ch : ks.word) {
            if (ch == '\t' || ch == 9) tab_pressed = true;
            if (ch == '`') backtick_pressed = true;
        }

        // Alarm/timer ringing: consume all key events until dismissed or snoozed
        if (alarm_ringing) {
            static const char* key_names[] = {"OPT", "ALT", "CTRL", "FN"};
            bool correct_dismiss =
                (alarm_dismiss_key == 0 && ks.opt) ||
                (alarm_dismiss_key == 1 && ks.alt) ||
                (alarm_dismiss_key == 2 && ks.ctrl) ||
                (alarm_dismiss_key == 3 && ks.fn);
            bool any_key = ks.enter || del_pressed || tab_pressed ||
                           ks.fn || ks.opt || ks.alt || ks.ctrl ||
                           ks.word.size() > 0;
            if (any_key) {
                if (correct_dismiss || alarm_ringing_is_timer) {
                    alarm_ringing = false;
                    overlay_mode = OVERLAY_NONE;
                    setStatus(alarm_ringing_is_timer ? "Timer dismissed" : "Alarm dismissed", 2000);
                } else {
                    // Wrong key for alarm → snooze 5 minutes, re-ring with new dismiss key
                    alarm_ringing = false;
                    overlay_mode = OVERLAY_NONE;
                    alarm_snooze_until_unix = (uint32_t)(time_sync.getCurrentTime() + 5 * 60);
                    char msg[56];
                    snprintf(msg, sizeof(msg), "Snoozed 5min. Next: press [%s]",
                             key_names[alarm_dismiss_key]);
                    setStatus(msg, 4000);
                }
            }
            return;
        }

        if (tab_pressed) {
            if (!screen_selector_active) openScreenSelector();
            return;
        }
        if (backtick_pressed) {
            // backtick alone = go back one level
            if (screen_selector_sub_active) {
                screen_selector_sub_active = false;
            } else if (screen_selector_active) {
                closeScreenSelector();
            } else {
                handleNavCommand(NAV_BACK);
            }
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

        // DEL scans WiFi / removes saved WiFi in WiFi settings section.
        if (!nav_handled && !text_input_active && del_pressed && current_screen == UI_SETTINGS && sett_section == SETT_WIFI) {
            handleNavCommand(NAV_BACK);
            nav_handled = true;
        }

        // DEL removes selected alarm or timer in alarms screen.
        if (!nav_handled && !text_input_active && del_pressed && current_screen == UI_ALARMS) {
            handleNavCommand(NAV_BACK);
            nav_handled = true;
        }

        if (!nav_handled && !text_input_active && del_pressed && current_screen == UI_PROFILES && overlay_mode == OVERLAY_NONE) {
            refreshProfiles();
            if (profile_count > 1) {
                overlay_mode = OVERLAY_PROFILE_DELETE;
                profile_delete_confirm_until = millis() + 5000;
                setStatus("Delete profile? Y yes / N no", 1200);
            } else {
                setStatus("Cannot delete last profile", 1500);
            }
            nav_handled = true;
        }

        // In text input mode, Delete should act like backspace instead of cancel.
        if (!nav_handled && text_input_active && del_pressed) {
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
