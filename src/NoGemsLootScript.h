#ifndef _NO_GEMS_LOOT_SCRIPT_H
#define _NO_GEMS_LOOT_SCRIPT_H

#include "ScriptMgr.h"

class NoGemsLootScript : public MiscScript
{
public:
    NoGemsLootScript();

    void OnAfterLootTemplateProcess(Loot* loot, LootTemplate const* tab, LootStore const& store, Player* lootOwner, bool personal, bool noEmptyError, uint16 lootMode) override;
};

void AddNoGemsLootScripts();

#endif // _NO_GEMS_LOOT_SCRIPT_H
