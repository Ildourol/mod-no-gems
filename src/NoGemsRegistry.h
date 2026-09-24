#ifndef _NO_GEMS_REGISTRY_H
#define _NO_GEMS_REGISTRY_H

#include "Common.h"
#include "ItemTemplate.h"
#include <unordered_map>
#include <shared_mutex>
#include <string>

class Player;
class Item;

struct ItemSocketBackup
{
    _Socket Socket[MAX_ITEM_PROTO_SOCKETS];
    uint32 socketBonus{0};
    uint32 StatsCount{0};
    _ItemStat ItemStat[MAX_ITEM_PROTO_STATS];
};

class NoGemsRegistry
{
public:
    static NoGemsRegistry* instance();

    void Initialize();
    bool IsInitialized() const { return _initialized; }
    bool IsItemModified(uint32 entry) const;

    uint32 GetVariantEntry(uint32 baseEntry) const;
    uint32 GetBaseEntry(uint32 variantEntry) const;
    bool IsVariant(uint32 entry) const;

    size_t GetModifiedCount() const
    {
        std::shared_lock<std::shared_mutex> lock(_lock);
        return _backupData.size();
    }

    ItemSocketBackup const* GetBackupData(uint32 entry) const
    {
        std::shared_lock<std::shared_mutex> lock(_lock);
        auto it = _backupData.find(entry);
        if (it != _backupData.end())
            return &it->second;
        return nullptr;
    }

    bool CheckAndModifyDynamicTemplate(ItemTemplate* proto);

    static bool CleanseItemGems(Player* player, Item* item);
    static bool CleanseAllPlayerItems(Player* player);

private:
    NoGemsRegistry() = default;

    mutable std::shared_mutex _lock;
    std::unordered_map<uint32, ItemSocketBackup> _backupData;
    bool _initialized{false};
};

#define sNoGemsRegistry NoGemsRegistry::instance()

#endif // _NO_GEMS_REGISTRY_H
