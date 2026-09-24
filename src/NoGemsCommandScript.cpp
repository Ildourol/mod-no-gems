#include "NoGemsCommandScript.h"
#include "ScriptMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "NoGemsConfig.h"
#include "NoGemsRegistry.h"
#include "Player.h"
#include "Item.h"

using namespace Acore::ChatCommands;

class NoGemsCommandScript : public CommandScript
{
public:
    NoGemsCommandScript() : CommandScript("NoGemsCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable noGemsSubCommands =
        {
            { "reload",  HandleReloadCommand,  SEC_ADMINISTRATOR, Console::Yes },
            { "stats",   HandleStatsCommand,   SEC_ADMINISTRATOR, Console::Yes },
            { "cleanse", HandleCleanseCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "info",    HandleInfoCommand,    SEC_ADMINISTRATOR, Console::Yes }
        };

        static ChatCommandTable commandTable =
        {
            { "nogems", noGemsSubCommands }
        };

        return commandTable;
    }

    static bool HandleReloadCommand(ChatHandler* handler)
    {
        sNoGemsConfig->Load();
        handler->SendSysMessage("mod-no-gems configuration reloaded.");
        return true;
    }

    static bool HandleStatsCommand(ChatHandler* handler)
    {
        handler->PSendSysMessage("mod-no-gems: %zu item templates modified in memory (0 sockets, stats merged).",
            sNoGemsRegistry->GetModifiedCount());
        return true;
    }

    static bool HandleCleanseCommand(ChatHandler* handler, Optional<PlayerIdentifier> target)
    {
        if (!target)
            target = PlayerIdentifier::FromTargetOrSelf(handler);

        if (!target)
        {
            handler->SendErrorMessage("No player selected or targeted.");
            return false;
        }

        Player* player = target->GetConnectedPlayer();
        if (!player)
        {
            handler->SendErrorMessage("Target player is not online.");
            return false;
        }

        bool cleansed = NoGemsRegistry::CleanseAllPlayerItems(player);
        if (cleansed)
            handler->PSendSysMessage("Cleanse completed and stats recalculated for player %s (items in gear, bags, and bank were updated).", player->GetName().c_str());
        else
            handler->PSendSysMessage("Player %s already has no gem enchantments.", player->GetName().c_str());

        return true;
    }

    static bool HandleInfoCommand(ChatHandler* handler, Optional<uint32> itemEntry)
    {
        uint32 entry = 0;
        if (itemEntry)
        {
            entry = *itemEntry;
        }
        else if (Player* player = handler->GetPlayer())
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            if (item)
                entry = item->GetEntry();
        }

        if (!entry)
        {
            handler->SendErrorMessage("Usage: .nogems info <itemId> or equip an item in main hand.");
            return false;
        }

        ItemSocketBackup const* backup = sNoGemsRegistry->GetBackupData(entry);
        if (!backup)
        {
            handler->PSendSysMessage("Item %u was not modified by mod-no-gems (has no sockets).", entry);
            return true;
        }

        uint32 socketCount = 0;
        for (uint32 s = 0; s < MAX_ITEM_PROTO_SOCKETS; ++s)
        {
            if (backup->Socket[s].Color != 0)
                socketCount++;
        }

        handler->PSendSysMessage(">> mod-no-gems Item Info for %u:", entry);
        handler->PSendSysMessage(" - Original Sockets: %u", socketCount);
        handler->PSendSysMessage(" - Original Socket Bonus ID: %u", backup->socketBonus);
        handler->PSendSysMessage(" - Original Stats Count: %u", backup->StatsCount);
        return true;
    }
};

void AddNoGemsCommandScripts()
{
    new NoGemsCommandScript();
}
