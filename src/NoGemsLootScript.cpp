#include "NoGemsLootScript.h"
#include "NoGemsConfig.h"
#include "LootMgr.h"

NoGemsLootScript::NoGemsLootScript()
    : MiscScript("NoGemsLootScript", { MISCHOOK_ON_AFTER_LOOT_TEMPLATE_PROCESS })
{
}

void NoGemsLootScript::OnAfterLootTemplateProcess(Loot* /*loot*/, LootTemplate const* /*tab*/, LootStore const& /*store*/, Player* /*lootOwner*/, bool /*personal*/, bool /*noEmptyError*/, uint16 /*lootMode*/)
{
    // ItemTemplates are modified in-memory on startup; all loot drops natively inherit 0 sockets and upgraded affixes.
}

void AddNoGemsLootScripts()
{
    new NoGemsLootScript();
}
