#ifndef MOD_NO_GEMS_AFFIX_MGR_H
#define MOD_NO_GEMS_AFFIX_MGR_H

#include "Common.h"
#include "Item.h"
#include "Player.h"
#include "NoGemsCatalog.h"
#include <unordered_map>
#include <vector>
#include <string>

struct ItemAffixEffectRecord
{
    uint8 socketIndex{0};
    uint8 socketType{0};
    uint32 sourceGemEntry{0};
    uint32 sourceEnchantId{0};
    uint32 modType{0}; // ITEM_MOD_*
    int32 amount{0};
};

struct ItemAffixRecord
{
    uint32 itemGuid{0};
    uint32 itemEntry{0};
    uint32 rollVersion{1};
    std::string socketSignature;
    std::vector<ItemAffixEffectRecord> effects;
};

class NoGemsAffixMgr
{
public:
    static NoGemsAffixMgr* instance();

    void InitializeDatabase();

    // Cache lookup or DB load
    ItemAffixRecord const* GetOrLoadAffix(Item* item);

    // Ensure item has an affix (loads from DB or generates a new one)
    ItemAffixRecord const* EnsureAffix(Item* item);

    // Apply or remove affix effects on the player
    void ApplyAffix(Player* player, Item* item, bool apply);

    // Check if item has any sockets (template sockets + prismatic socket enchantment)
    static bool HasSockets(Item const* item);

    // Compute socket signature, e.g. "R-Y-B" or "M-R"
    static std::string ComputeSocketSignature(Item const* item, std::vector<uint8>& socketColors);

    // Format affix description for chat/inspection
    static std::string FormatAffix(ItemAffixRecord const& affix);

    // Clean up cache when an item is permanently deleted
    void RemoveFromCache(uint32 itemGuid);

private:
    NoGemsAffixMgr() = default;

    ItemAffixRecord const* GenerateAffix(Item* item);
    void SaveAffixToDB(ItemAffixRecord const& affix);

    std::unordered_map<uint32, ItemAffixRecord> _affixCache;
};

#define sNoGemsAffixMgr NoGemsAffixMgr::instance()

#endif // MOD_NO_GEMS_AFFIX_MGR_H
