#include "NoGemsRegistry.h"
#include "NoGemsConfig.h"
#include "NoGemsFormula.h"
#include "ObjectMgr.h"
#include "Log.h"

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
        backup.Description = proto.Description;

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
    backup.Description = proto->Description;

    _backupData[proto->ItemId] = backup;
    return NoGemsFormula::ApplyNoGems(proto);
}

