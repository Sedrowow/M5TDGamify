#ifndef SHOPSYSTEM_H
#define SHOPSYSTEM_H

#include <Arduino.h>
#include "../levelsystem/LevelSystem.h"

#define MAX_SHOP_ITEMS 96
#define MAX_RECIPES 96
#define MAX_INVENTORY_STACKS 128

#define MAX_SHOP_ITEM_NAME_LEN 48
#define MAX_SHOP_ITEM_DESC_LEN 128

enum ItemEffectType {
    ITEM_EFFECT_NONE = 0,
    ITEM_EFFECT_CONSUME_REMOVE = 1,
    ITEM_EFFECT_ADD_XP = 2,
    ITEM_EFFECT_RANDOM_XP = 3,
    ITEM_EFFECT_COUNTDOWN = 4
};

struct ShopItem {
    uint16_t id;
    char name[MAX_SHOP_ITEM_NAME_LEN];
    char description[MAX_SHOP_ITEM_DESC_LEN];

    bool collectible_only;
    ItemEffectType effect_type;
    uint32_t effect_value;

    int32_t base_price;
    int32_t current_price;
    int32_t price_increase_per_purchase;

    uint16_t buy_limit;       // 0 = unlimited
    uint16_t times_purchased;
    uint32_t cooldown_seconds;
    uint32_t last_purchase_time;

    bool active;

    ShopItem() : id(0), collectible_only(true), effect_type(ITEM_EFFECT_NONE), effect_value(0),
                 base_price(0), current_price(0), price_increase_per_purchase(0),
                 buy_limit(0), times_purchased(0), cooldown_seconds(0), last_purchase_time(0), active(false) {
        name[0] = '\0';
        description[0] = '\0';
    }
};

struct CraftIngredient {
    uint16_t item_id;
    uint8_t quantity;

    CraftIngredient() : item_id(0), quantity(0) {}
};

struct CraftRecipe {
    uint16_t id;
    CraftIngredient inputs[3];
    uint8_t input_count;
    uint16_t output_item_id;
    uint8_t output_quantity;
    bool active;

    CraftRecipe() : id(0), input_count(0), output_item_id(0), output_quantity(0), active(false) {}
};

struct InventoryEntry {
    uint16_t item_id;
    uint16_t quantity;
    bool active;

    InventoryEntry() : item_id(0), quantity(0), active(false) {}
};

class ShopSystem {
private:
    ShopItem items[MAX_SHOP_ITEMS];
    CraftRecipe recipes[MAX_RECIPES];
    InventoryEntry inventory[MAX_INVENTORY_STACKS];

    uint16_t item_count;
    uint16_t recipe_count;
    uint16_t next_item_id;
    uint16_t next_recipe_id;

    InventoryEntry* inventoryEntry(uint16_t item_id);
    const InventoryEntry* inventoryEntry(uint16_t item_id) const;

public:
    ShopSystem();

    void resetAll();
    void resetRuntimeState();

    uint16_t addItem(const char* name, const char* description, bool collectible_only,
                     ItemEffectType effect_type, uint32_t effect_value,
                     int32_t base_price, uint16_t buy_limit,
                     uint32_t cooldown_seconds, int32_t price_increase_per_purchase);
    bool removeItem(uint16_t item_id);
    ShopItem* getItem(uint16_t item_id);
    const ShopItem* getItem(uint16_t item_id) const;
    uint16_t getActiveItemCount() const;
    ShopItem* getItemByActiveIndex(uint16_t index);
    const ShopItem* getItemByActiveIndex(uint16_t index) const;

    uint16_t addRecipe(const CraftIngredient* inputs, uint8_t input_count,
                       uint16_t output_item_id, uint8_t output_quantity);
    bool removeRecipe(uint16_t recipe_id);
    CraftRecipe* getRecipe(uint16_t recipe_id);
    const CraftRecipe* getRecipe(uint16_t recipe_id) const;
    uint16_t getActiveRecipeCount() const;
    CraftRecipe* getRecipeByActiveIndex(uint16_t index);
    const CraftRecipe* getRecipeByActiveIndex(uint16_t index) const;

    bool addInventoryItem(uint16_t item_id, uint16_t quantity);
    bool consumeInventoryItem(uint16_t item_id, uint16_t quantity);
    uint16_t getInventoryQuantity(uint16_t item_id) const;

    bool canBuyItem(uint16_t item_id, int32_t money, uint32_t now_seconds) const;
    bool buyItem(uint16_t item_id, int32_t& money, uint32_t now_seconds);

    bool canCraftRecipe(uint16_t recipe_id) const;
    bool craftRecipe(uint16_t recipe_id);

    bool useItem(uint16_t item_id, LevelSystem& level_system, uint32_t now_seconds);

    uint16_t getItemCountRaw() const { return item_count; }
    uint16_t getRecipeCountRaw() const { return recipe_count; }
    ShopItem* itemsRaw() { return items; }
    const ShopItem* itemsRaw() const { return items; }
    CraftRecipe* recipesRaw() { return recipes; }
    const CraftRecipe* recipesRaw() const { return recipes; }
    InventoryEntry* inventoryRaw() { return inventory; }
    const InventoryEntry* inventoryRaw() const { return inventory; }

    void printOverview() const;
};

#endif
