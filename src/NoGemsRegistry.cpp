#include "NoGemsRegistry.h"
#include "NoGemsConfig.h"
#include "NoGemsFormula.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Player.h"
#include "Item.h"
#include "Bag.h"

NoGemsRegistry* NoGemsRegistry::instance()
{
    static NoGemsRegistry instance;
    return &instance;
}

void NoGemsRegistry::Initialize()
{
    if (_initialized || !sNoGemsConfig->Enable)
        return;

    ItemTemplateContainer const* itemTemplates = sObjectMgr->GetItemTemplateStore();
    if (!itemTemplates)
    {
        LOG_ERROR("server.loading", ">> [mod-no-gems] Failed to initialize: ItemTemplateStore is null!");
        return;
    }

    std::unique_lock<std::shared_mutex> lock(_lock);
    _backupData.reserve(3000);
    uint32 modifiedCount = 0;

    for (auto const& pair : *itemTemplates)
    {
        ItemTemplate& proto = const_cast<ItemTemplate&>(pair.second);

        if (!NoGemsFormula::HasSockets(&proto))
            continue;

        if (_backupData.find(proto.ItemId) != _backupData.end())
            continue;

        ItemSocketBackup backup;
        for (uint32 s = 0; s < MAX_ITEM_PROTO_SOCKETS; ++s)
            backup.Socket[s] = proto.Socket[s];
        backup.socketBonus = proto.socketBonus;
        backup.StatsCount = proto.StatsCount;
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
            backup.ItemStat[i] = proto.ItemStat[i];

        _backupData[proto.ItemId] = backup;

        if (NoGemsFormula::ApplyNoGems(&proto))
        {
            ++modifiedCount;
            if (sNoGemsConfig->Debug)
            {
                LOG_INFO("module", "[mod-no-gems] Modified item {} '{}' (sockets removed, stats merged into ItemStat)",
                    proto.ItemId, proto.Name1);
            }
        }
    }

    _initialized = true;
    LOG_INFO("server.loading", ">> [mod-no-gems] Successfully modified {} item templates in memory (0 sockets, synergistic affixes applied).",
        modifiedCount);
}

bool NoGemsRegistry::IsItemModified(uint32 entry) const
{
    std::shared_lock<std::shared_mutex> lock(_lock);
    return _backupData.find(entry) != _backupData.end();
}

uint32 NoGemsRegistry::GetVariantEntry(uint32 baseEntry) const
{
    return baseEntry;
}

uint32 NoGemsRegistry::GetBaseEntry(uint32 variantEntry) const
{
    return variantEntry;
}

bool NoGemsRegistry::IsVariant(uint32 /*entry*/) const
{
    return false;
}

bool NoGemsRegistry::CheckAndModifyDynamicTemplate(ItemTemplate* proto)
{
    if (!proto || !sNoGemsConfig->Enable || !NoGemsFormula::HasSockets(proto))
        return false;

    // Fast-path read check: avoid acquiring exclusive lock if template already converted
    {
        std::shared_lock<std::shared_mutex> readLock(_lock);
        if (_backupData.find(proto->ItemId) != _backupData.end())
            return false;
    }

    std::unique_lock<std::shared_mutex> lock(_lock);
    if (_backupData.find(proto->ItemId) != _backupData.end())
        return false;

    ItemSocketBackup backup;
    for (uint32 s = 0; s < MAX_ITEM_PROTO_SOCKETS; ++s)
        backup.Socket[s] = proto->Socket[s];
    backup.socketBonus = proto->socketBonus;
    backup.StatsCount = proto->StatsCount;
    for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        backup.ItemStat[i] = proto->ItemStat[i];

    _backupData[proto->ItemId] = backup;
    return NoGemsFormula::ApplyNoGems(proto);
}

bool NoGemsRegistry::CleanseItemGems(Player* player, Item* item)
{
    if (!item)
        return false;

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
        item->SendUpdateToPlayer(player);
    }

    return hadGems;
}

bool NoGemsRegistry::CleanseAllPlayerItems(Player* player)
{
    if (!player)
        return false;

    bool anyGemsCleansed = false;

    // 1. Equipped items
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (CleanseItemGems(player, player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot)))
            anyGemsCleansed = true;
    }

    // 2. Main bag
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        if (CleanseItemGems(player, player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot)))
            anyGemsCleansed = true;
    }

    // 3. Extra equipped bags & bag contents
    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        Bag* bag = player->GetBagByPos(bagSlot);
        if (bag)
        {
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
            {
                if (CleanseItemGems(player, bag->GetItemByPos(slot)))
                    anyGemsCleansed = true;
            }
        }
    }

    // 4. Bank items
    for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; ++slot)
    {
        if (CleanseItemGems(player, player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot)))
            anyGemsCleansed = true;
    }

    for (uint8 bagSlot = BANK_SLOT_BAG_START; bagSlot < BANK_SLOT_BAG_END; ++bagSlot)
    {
        Bag* bag = player->GetBagByPos(bagSlot);
        if (bag)
        {
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
            {
                if (CleanseItemGems(player, bag->GetItemByPos(slot)))
                    anyGemsCleansed = true;
            }
        }
    }

    if (anyGemsCleansed)
    {
        player->_RemoveAllItemMods();
        player->_ApplyAllItemMods();
    }

    return anyGemsCleansed;
}

