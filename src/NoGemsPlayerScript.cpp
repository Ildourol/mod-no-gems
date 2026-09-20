#include "NoGemsPlayerScript.h"
#include "NoGemsConfig.h"
#include "Player.h"
#include "Item.h"
#include "Bag.h"

NoGemsPlayerScript::NoGemsPlayerScript()
    : PlayerScript("NoGemsPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_EQUIP,
        PLAYERHOOK_CAN_USE_ITEM
    })
{
}

void NoGemsPlayerScript::CleanseItemGems(Player* player, Item* item)
{
    if (!item)
        return;

    bool hadGems = false;
    for (uint32 s = SOCK_ENCHANTMENT_SLOT; s <= PRISMATIC_ENCHANTMENT_SLOT; ++s)
    {
        if (item->GetEnchantmentId(EnchantmentSlot(s)) != 0)
        {
            item->ClearEnchantment(EnchantmentSlot(s));
            hadGems = true;
        }
    }

    if (hadGems && player)
    {
        item->SetState(ITEM_CHANGED, player);
        if (item->IsEquipped())
        {
            player->_ApplyItemMods(item, item->GetSlot(), false);
            player->_ApplyItemMods(item, item->GetSlot(), true);
        }
    }
}

void NoGemsPlayerScript::OnPlayerLogin(Player* player)
{
    if (!sNoGemsConfig->Enable || !player)
        return;

    if (sNoGemsConfig->ConvertLegacyGearOnLogin)
    {
        // 1. Equipped items
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            CleanseItemGems(player, player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
        }

        // 2. Main bag
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            CleanseItemGems(player, player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
        }

        // 3. Extra equipped bags & bag contents
        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        {
            Bag* bag = player->GetBagByPos(bagSlot);
            if (bag)
            {
                for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                {
                    CleanseItemGems(player, bag->GetItemByPos(slot));
                }
            }
        }

        // 4. Bank items
        for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; ++slot)
        {
            CleanseItemGems(player, player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
        }

        for (uint8 bagSlot = BANK_SLOT_BAG_START; bagSlot < BANK_SLOT_BAG_END; ++bagSlot)
        {
            Bag* bag = player->GetBagByPos(bagSlot);
            if (bag)
            {
                for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                {
                    CleanseItemGems(player, bag->GetItemByPos(slot));
                }
            }
        }
    }

    // Force full recalculation of character sheet stats and combat ratings with the modified item templates
    player->_RemoveAllItemMods();
    player->_ApplyAllItemMods();
}

void NoGemsPlayerScript::OnPlayerEquip(Player* player, Item* item, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/)
{
    if (!sNoGemsConfig->Enable || !player || !item)
        return;

    CleanseItemGems(player, item);
}

bool NoGemsPlayerScript::OnPlayerCanUseItem(Player* /*player*/, ItemTemplate const* proto, InventoryResult& result)
{
    if (sNoGemsConfig->Enable && sNoGemsConfig->BlockGemUse && proto)
    {
        if (proto->Class == ITEM_CLASS_GEM || proto->ItemId == 41611)
        {
            result = EQUIP_ERR_CANT_DO_RIGHT_NOW;
            return false;
        }
    }
    return true;
}

void AddNoGemsPlayerScripts()
{
    new NoGemsPlayerScript();
}
