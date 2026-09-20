#ifndef _NO_GEMS_PLAYER_SCRIPT_H
#define _NO_GEMS_PLAYER_SCRIPT_H

#include "ScriptMgr.h"

class Player;
class Item;

class NoGemsPlayerScript : public PlayerScript
{
public:
    NoGemsPlayerScript();

    void OnPlayerLogin(Player* player) override;
    void OnPlayerEquip(Player* player, Item* item, uint8 bag, uint8 slot, bool update) override;
    bool OnPlayerCanUseItem(Player* player, ItemTemplate const* proto, InventoryResult& result) override;

private:
    void CleanseItemGems(Player* player, Item* item);
};

void AddNoGemsPlayerScripts();

#endif // _NO_GEMS_PLAYER_SCRIPT_H
