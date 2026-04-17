#include "ShopSystem.h"
#include <cstring>

ShopSystem::ShopSystem() : item_count(0), recipe_count(0), next_item_id(1), next_recipe_id(1) {
    resetAll();
}

void ShopSystem::resetAll() {
    item_count = 0;
    recipe_count = 0;
    next_item_id = 1;
    next_recipe_id = 1;

    for (uint16_t i = 0; i < MAX_SHOP_ITEMS; i++) {
        items[i] = ShopItem();
    }
    for (uint16_t i = 0; i < MAX_RECIPES; i++) {
        recipes[i] = CraftRecipe();
    }
    for (uint16_t i = 0; i < MAX_INVENTORY_STACKS; i++) {
        inventory[i] = InventoryEntry();
    }
}

void ShopSystem::resetRuntimeState() {
    for (uint16_t i = 0; i < item_count; i++) {
        if (items[i].active) {
            items[i].current_price = items[i].base_price;
            items[i].times_purchased = 0;
            items[i].last_purchase_time = 0;
        }
    }

    for (uint16_t i = 0; i < MAX_INVENTORY_STACKS; i++) {
        inventory[i] = InventoryEntry();
    }
}

InventoryEntry* ShopSystem::inventoryEntry(uint16_t item_id) {
    for (uint16_t i = 0; i < MAX_INVENTORY_STACKS; i++) {
        if (inventory[i].active && inventory[i].item_id == item_id) {
            return &inventory[i];
        }
    }
    return nullptr;
}

const InventoryEntry* ShopSystem::inventoryEntry(uint16_t item_id) const {
    for (uint16_t i = 0; i < MAX_INVENTORY_STACKS; i++) {
        if (inventory[i].active && inventory[i].item_id == item_id) {
            return &inventory[i];
        }
    }
    return nullptr;
}

uint16_t ShopSystem::addItem(const char* name, const char* description, bool collectible_only,
                             ItemEffectType effect_type, uint32_t effect_value,
                             int32_t base_price, uint16_t buy_limit,
                             uint32_t cooldown_seconds, int32_t price_increase_per_purchase) {
    if (!name || name[0] == '\0' || item_count >= MAX_SHOP_ITEMS) {
        return 0;
    }

    ShopItem& item = items[item_count];
    item.id = next_item_id;
    strncpy(item.name, name, MAX_SHOP_ITEM_NAME_LEN - 1);
    item.name[MAX_SHOP_ITEM_NAME_LEN - 1] = '\0';

    if (description) {
        strncpy(item.description, description, MAX_SHOP_ITEM_DESC_LEN - 1);
        item.description[MAX_SHOP_ITEM_DESC_LEN - 1] = '\0';
    }

    item.collectible_only = collectible_only;
    item.effect_type = effect_type;
    item.effect_value = effect_value;
    item.base_price = base_price;
    item.current_price = base_price;
    item.price_increase_per_purchase = price_increase_per_purchase;
    item.buy_limit = buy_limit;
    item.times_purchased = 0;
    item.cooldown_seconds = cooldown_seconds;
    item.last_purchase_time = 0;
    item.active = true;

    item_count++;
    next_item_id++;
    return item.id;
}

bool ShopSystem::removeItem(uint16_t item_id) {
    ShopItem* item = getItem(item_id);
    if (!item) return false;
    item->active = false;
    return true;
}

ShopItem* ShopSystem::getItem(uint16_t item_id) {
    for (uint16_t i = 0; i < item_count; i++) {
        if (items[i].active && items[i].id == item_id) return &items[i];
    }
    return nullptr;
}

const ShopItem* ShopSystem::getItem(uint16_t item_id) const {
    for (uint16_t i = 0; i < item_count; i++) {
        if (items[i].active && items[i].id == item_id) return &items[i];
    }
    return nullptr;
}

uint16_t ShopSystem::getActiveItemCount() const {
    uint16_t count = 0;
    for (uint16_t i = 0; i < item_count; i++) {
        if (items[i].active) count++;
    }
    return count;
}

ShopItem* ShopSystem::getItemByActiveIndex(uint16_t index) {
    uint16_t seen = 0;
    for (uint16_t i = 0; i < item_count; i++) {
        if (!items[i].active) continue;
        if (seen == index) return &items[i];
        seen++;
    }
    return nullptr;
}

const ShopItem* ShopSystem::getItemByActiveIndex(uint16_t index) const {
    uint16_t seen = 0;
    for (uint16_t i = 0; i < item_count; i++) {
        if (!items[i].active) continue;
        if (seen == index) return &items[i];
        seen++;
    }
    return nullptr;
}

uint16_t ShopSystem::addRecipe(const CraftIngredient* inputs, uint8_t input_count,
                               uint16_t output_item_id, uint8_t output_quantity) {
    if (!inputs || input_count == 0 || input_count > 3 || output_item_id == 0 || output_quantity == 0 || recipe_count >= MAX_RECIPES) {
        return 0;
    }

    if (input_count == 1 && inputs[0].quantity < 2) {
        return 0;
    }

    for (uint8_t i = 0; i < input_count; i++) {
        if (inputs[i].item_id == 0 || inputs[i].quantity == 0 || !getItem(inputs[i].item_id)) return 0;
    }

    if (!getItem(output_item_id)) return 0;

    CraftRecipe& recipe = recipes[recipe_count];
    recipe.id = next_recipe_id;
    recipe.input_count = input_count;
    for (uint8_t i = 0; i < input_count; i++) recipe.inputs[i] = inputs[i];
    recipe.output_item_id = output_item_id;
    recipe.output_quantity = output_quantity;
    recipe.active = true;

    recipe_count++;
    next_recipe_id++;
    return recipe.id;
}

bool ShopSystem::removeRecipe(uint16_t recipe_id) {
    CraftRecipe* recipe = getRecipe(recipe_id);
    if (!recipe) return false;
    recipe->active = false;
    return true;
}

CraftRecipe* ShopSystem::getRecipe(uint16_t recipe_id) {
    for (uint16_t i = 0; i < recipe_count; i++) {
        if (recipes[i].active && recipes[i].id == recipe_id) return &recipes[i];
    }
    return nullptr;
}

const CraftRecipe* ShopSystem::getRecipe(uint16_t recipe_id) const {
    for (uint16_t i = 0; i < recipe_count; i++) {
        if (recipes[i].active && recipes[i].id == recipe_id) return &recipes[i];
    }
    return nullptr;
}

uint16_t ShopSystem::getActiveRecipeCount() const {
    uint16_t count = 0;
    for (uint16_t i = 0; i < recipe_count; i++) if (recipes[i].active) count++;
    return count;
}

CraftRecipe* ShopSystem::getRecipeByActiveIndex(uint16_t index) {
    uint16_t seen = 0;
    for (uint16_t i = 0; i < recipe_count; i++) {
        if (!recipes[i].active) continue;
        if (seen == index) return &recipes[i];
        seen++;
    }
    return nullptr;
}

const CraftRecipe* ShopSystem::getRecipeByActiveIndex(uint16_t index) const {
    uint16_t seen = 0;
    for (uint16_t i = 0; i < recipe_count; i++) {
        if (!recipes[i].active) continue;
        if (seen == index) return &recipes[i];
        seen++;
    }
    return nullptr;
}

bool ShopSystem::addInventoryItem(uint16_t item_id, uint16_t quantity) {
    if (item_id == 0 || quantity == 0 || !getItem(item_id)) return false;

    InventoryEntry* existing = inventoryEntry(item_id);
    if (existing) {
        existing->quantity += quantity;
        return true;
    }

    for (uint16_t i = 0; i < MAX_INVENTORY_STACKS; i++) {
        if (!inventory[i].active) {
            inventory[i].active = true;
            inventory[i].item_id = item_id;
            inventory[i].quantity = quantity;
            return true;
        }
    }

    return false;
}

bool ShopSystem::consumeInventoryItem(uint16_t item_id, uint16_t quantity) {
    if (quantity == 0) return true;
    InventoryEntry* existing = inventoryEntry(item_id);
    if (!existing || existing->quantity < quantity) return false;

    existing->quantity -= quantity;
    if (existing->quantity == 0) {
        existing->active = false;
        existing->item_id = 0;
    }
    return true;
}

uint16_t ShopSystem::getInventoryQuantity(uint16_t item_id) const {
    const InventoryEntry* e = inventoryEntry(item_id);
    return e ? e->quantity : 0;
}

bool ShopSystem::canBuyItem(uint16_t item_id, int32_t money, uint32_t now_seconds) const {
    const ShopItem* item = getItem(item_id);
    if (!item) return false;
    if (money < item->current_price) return false;
    if (item->buy_limit > 0 && item->times_purchased >= item->buy_limit) return false;
    if (item->cooldown_seconds > 0 && item->last_purchase_time > 0) {
        if ((now_seconds - item->last_purchase_time) < item->cooldown_seconds) return false;
    }
    return true;
}

bool ShopSystem::buyItem(uint16_t item_id, int32_t& money, uint32_t now_seconds) {
    ShopItem* item = getItem(item_id);
    if (!item) return false;
    if (!canBuyItem(item_id, money, now_seconds)) return false;

    money -= item->current_price;
    item->times_purchased++;
    item->last_purchase_time = now_seconds;
    item->current_price += item->price_increase_per_purchase;
    if (item->current_price < 0) item->current_price = 0;

    return addInventoryItem(item_id, 1);
}

bool ShopSystem::canCraftRecipe(uint16_t recipe_id) const {
    const CraftRecipe* recipe = getRecipe(recipe_id);
    if (!recipe) return false;

    for (uint8_t i = 0; i < recipe->input_count; i++) {
        if (getInventoryQuantity(recipe->inputs[i].item_id) < recipe->inputs[i].quantity) {
            return false;
        }
    }
    return true;
}

bool ShopSystem::craftRecipe(uint16_t recipe_id) {
    CraftRecipe* recipe = getRecipe(recipe_id);
    if (!recipe) return false;
    if (!canCraftRecipe(recipe_id)) return false;

    for (uint8_t i = 0; i < recipe->input_count; i++) {
        if (!consumeInventoryItem(recipe->inputs[i].item_id, recipe->inputs[i].quantity)) {
            return false;
        }
    }

    return addInventoryItem(recipe->output_item_id, recipe->output_quantity);
}

bool ShopSystem::useItem(uint16_t item_id, LevelSystem& level_system, uint32_t now_seconds) {
    ShopItem* item = getItem(item_id);
    if (!item) return false;
    if (getInventoryQuantity(item_id) == 0) return false;

    if (item->collectible_only || item->effect_type == ITEM_EFFECT_NONE) {
        return false;
    }

    bool ok = true;
    switch (item->effect_type) {
        case ITEM_EFFECT_CONSUME_REMOVE:
            ok = true;
            break;
        case ITEM_EFFECT_ADD_XP:
            level_system.addXP(item->effect_value);
            break;
        case ITEM_EFFECT_RANDOM_XP: {
            uint32_t max_xp = item->effect_value;
            if (max_xp == 0) max_xp = 1;
            uint32_t random_xp = (uint32_t)random(1, (long)max_xp + 1);
            level_system.addXP(random_xp);
            break;
        }
        case ITEM_EFFECT_COUNTDOWN:
            // Countdown effect is tracked by user semantics; consume item.
            (void)now_seconds;
            break;
        default:
            ok = false;
            break;
    }

    if (!ok) return false;
    return consumeInventoryItem(item_id, 1);
}

void ShopSystem::printOverview() const {
    Serial.println("=== Shop Overview ===");
    for (uint16_t i = 0; i < item_count; i++) {
        if (!items[i].active) continue;
        Serial.printf("Item #%d %s | price:%ld inv:%d\n", items[i].id, items[i].name, (long)items[i].current_price,
                      getInventoryQuantity(items[i].id));
    }

    Serial.println("=== Recipes ===");
    for (uint16_t i = 0; i < recipe_count; i++) {
        if (!recipes[i].active) continue;
        Serial.printf("Recipe #%d -> item #%d x%d\n", recipes[i].id, recipes[i].output_item_id, recipes[i].output_quantity);
    }
}
