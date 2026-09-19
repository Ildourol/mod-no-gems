#include "NoGemsConfig.h"
#include "NoGemsCatalog.h"
#include "NoGemsAffixMgr.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Item.h"
#include "Player.h"
#include "PlayerScript.h"
#include "RBAC.h"
#include "ServerScript.h"
#include "WorldPacket.h"
#include "WorldScript.h"
#include "Opcodes.h"

//
// WorldScript: startup, DBC / catalog loading, database initialization
//
class NoGemsWorldScript : public WorldScript
{
public:
    NoGemsWorldScript() : WorldScript("NoGemsWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sNoGemsConfig->LoadConfig();
    }

    void OnStartup() override
    {
        sNoGemsAffixMgr->InitializeDatabase();
        sNoGemsCatalog->Initialize();
    }
};

//
// ServerScript: block CMSG_SOCKET_GEMS packet at network boundary
//
class NoGemsServerScript : public ServerScript
{
public:
    NoGemsServerScript() : ServerScript("NoGemsServerScript") { }

    bool CanPacketReceive(WorldSession* session, WorldPacket const& packet) override
    {
        if (sNoGemsConfig->IsBlockSocketing() && packet.GetOpcode() == CMSG_SOCKET_GEMS)
        {
            if (session && session->GetPlayer())
            {
                ChatHandler(session).SendNotification("Socketing gems is currently disabled on this realm.");
            }
            return false;
        }

        return true;
    }
};

//
// PlayerScript: gem enchantment suppression, socket bonus suppression, affix equip/unequip/login
//
class NoGemsPlayerScript : public PlayerScript
{
public:
    NoGemsPlayerScript() : PlayerScript("NoGemsPlayerScript") { }

    // Intercept enchantment application
    bool OnPlayerCanApplyEnchantment(Player* /*player*/, Item* item, EnchantmentSlot slot, bool apply, bool /*apply_dur*/, bool /*ignore_condition*/) override
    {
        if (!sNoGemsConfig->IsEnabled())
            return true;

        // Gem suppression: slots SOCK_ENCHANTMENT_SLOT, SOCK_ENCHANTMENT_SLOT_2, SOCK_ENCHANTMENT_SLOT_3
        if (sNoGemsConfig->IsDisableGemEffects())
        {
            if (slot == SOCK_ENCHANTMENT_SLOT || slot == SOCK_ENCHANTMENT_SLOT_2 || slot == SOCK_ENCHANTMENT_SLOT_3)
            {
                // When unapplying during cleanup, allow it so stats don't remain stuck
                if (!apply)
                    return true;

                // When applying, block gem stat/aura/proc application
                return false;
            }
        }

        // Socket bonus suppression: BONUS_ENCHANTMENT_SLOT
        if (sNoGemsConfig->IsDisableSocketBonuses())
        {
            if (slot == BONUS_ENCHANTMENT_SLOT)
            {
                if (!apply)
                    return true;

                // If this is the item's socket bonus, block application
                if (item && item->GetTemplate() && item->GetTemplate()->socketBonus != 0)
                {
                    uint32 enchantId = item->GetEnchantmentId(slot);
                    if (enchantId == item->GetTemplate()->socketBonus)
                        return false;
                }
            }
        }

        return true;
    }

    // Apply replacement affixes on item equip
    void OnPlayerEquip(Player* player, Item* item, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override
    {
        if (!sNoGemsConfig->IsReplacementEnabled() || !player || !item)
            return;

        if (NoGemsAffixMgr::HasSockets(item))
            sNoGemsAffixMgr->ApplyAffix(player, item, true);
    }

    // Remove replacement affixes on item unequip
    void OnPlayerUnequip(Player* player, Item* item) override
    {
        if (!sNoGemsConfig->IsReplacementEnabled() || !player || !item)
            return;

        if (NoGemsAffixMgr::HasSockets(item))
            sNoGemsAffixMgr->ApplyAffix(player, item, false);
    }

    // Reconcile replacement affixes on player login for all equipped socketed items
    void OnPlayerLogin(Player* player) override
    {
        if (!sNoGemsConfig->IsReplacementEnabled() || !player)
            return;

        for (uint8 i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (!item)
                continue;

            if (NoGemsAffixMgr::HasSockets(item))
                sNoGemsAffixMgr->ApplyAffix(player, item, true);
        }
    }

    // Ensure affix generation on item acquisition
    void OnPlayerStoreNewItem(Player* /*player*/, Item* item, uint32 /*count*/) override
    {
        if (!sNoGemsConfig->IsReplacementEnabled() || !item)
            return;

        if (NoGemsAffixMgr::HasSockets(item))
            sNoGemsAffixMgr->EnsureAffix(item);
    }

    void OnPlayerLootItem(Player* /*player*/, Item* item, uint32 /*count*/, ObjectGuid /*lootguid*/) override
    {
        if (!sNoGemsConfig->IsReplacementEnabled() || !item)
            return;

        if (NoGemsAffixMgr::HasSockets(item))
            sNoGemsAffixMgr->EnsureAffix(item);
    }

    void OnPlayerCreateItem(Player* /*player*/, Item* item, uint32 /*count*/) override
    {
        if (!sNoGemsConfig->IsReplacementEnabled() || !item)
            return;

        if (NoGemsAffixMgr::HasSockets(item))
            sNoGemsAffixMgr->EnsureAffix(item);
    }

    void OnPlayerQuestRewardItem(Player* /*player*/, Item* item, uint32 /*count*/) override
    {
        if (!sNoGemsConfig->IsReplacementEnabled() || !item)
            return;

        if (NoGemsAffixMgr::HasSockets(item))
            sNoGemsAffixMgr->EnsureAffix(item);
    }
};

//
// CommandScript: inspection/debug command `.nogems item`
//
using namespace Acore::ChatCommands;

class NoGemsCommandScript : public CommandScript
{
public:
    NoGemsCommandScript() : CommandScript("NoGemsCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable nogemsSubCommandTable =
        {
            { "item", HandleNoGemsItemCommand, rbac::RBAC_PERM_COMMAND_GEAR_STATS, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "nogems", nogemsSubCommandTable }
        };

        return commandTable;
    }

    static bool HandleNoGemsItemCommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        handler->PSendSysMessage("|cff00ffff=== mod-no-gems Item Inspection ===|r");

        bool foundAny = false;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item || !NoGemsAffixMgr::HasSockets(item))
                continue;

            foundAny = true;
            ItemTemplate const* proto = item->GetTemplate();
            std::string itemName = proto ? proto->Name1 : "Unknown Item";

            ItemAffixRecord const* affix = sNoGemsAffixMgr->GetOrLoadAffix(item);
            if (affix && !affix->effects.empty())
            {
                handler->PSendSysMessage("|cffffff00Item:|r {} (GUID {})", itemName, item->GetGUID().GetCounter());
                std::string desc = NoGemsAffixMgr::FormatAffix(*affix);
                handler->PSendSysMessage("{}", desc);
            }
            else
            {
                handler->PSendSysMessage("|cffffff00Item:|r {} (GUID {}) - No replacement affix generated.", itemName, item->GetGUID().GetCounter());
            }
        }

        if (!foundAny)
        {
            handler->PSendSysMessage("You do not have any equipped items with sockets.");
        }

        return true;
    }
};

void AddNoGemsScripts()
{
    new NoGemsWorldScript();
    new NoGemsServerScript();
    new NoGemsPlayerScript();
    new NoGemsCommandScript();
}
