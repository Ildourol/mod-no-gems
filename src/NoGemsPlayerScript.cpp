#include "NoGemsPlayerScript.h"
#include "NoGemsConfig.h"
#include "NoGemsRegistry.h"
#include "Player.h"
#include "Item.h"

NoGemsPlayerScript::NoGemsPlayerScript()
    : PlayerScript("NoGemsPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_EQUIP,
        PLAYERHOOK_CAN_USE_ITEM
    })
{
}

void NoGemsPlayerScript::OnPlayerLogin(Player* player)
{
    if (!sNoGemsConfig->Enable || !player)
        return;

    if (sNoGemsConfig->ConvertLegacyGearOnLogin)
    {
        NoGemsRegistry::CleanseAllPlayerItems(player);
    }
}

void NoGemsPlayerScript::OnPlayerEquip(Player* player, Item* item, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/)
{
    if (!sNoGemsConfig->Enable || !player || !item)
        return;

    NoGemsRegistry::CleanseItemGems(player, item);
}

bool NoGemsPlayerScript::OnPlayerCanUseItem(Player* /*player*/, ItemTemplate const* proto, InventoryResult& result)
{
    if (sNoGemsConfig->Enable && sNoGemsConfig->BlockGemUse && proto)
    {
        // Block all gems (ITEM_CLASS_GEM) and socket creation items:
        // 41611: Eternal Belt Buckle
        // 42614: Titanium Weapon Chain (if socket related)
        // 44883: Titanium Plating
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
