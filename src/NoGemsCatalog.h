#ifndef MOD_NO_GEMS_CATALOG_H
#define MOD_NO_GEMS_CATALOG_H

#include "Common.h"
#include "ItemTemplate.h"
#include <vector>
#include <map>

struct ReplacementStatComponent
{
    uint32 modType{0}; // ITEM_MOD_*
    int32 amount{0};
};

struct GemCatalogEntry
{
    uint32 itemId{0};
    uint32 subclass{0};
    uint32 colorMask{0};
    uint32 requiredLevel{0};
    uint32 itemLevel{0};
    uint32 quality{0};
    bool isProfessionExclusive{false};
    uint32 enchantId{0};
    std::vector<ReplacementStatComponent> statComponents;
};

class NoGemsCatalog
{
public:
    static NoGemsCatalog* instance();

    void Initialize();
    [[nodiscard]] bool IsInitialized() const { return _initialized; }

    [[nodiscard]] std::vector<GemCatalogEntry const*> GetCandidates(uint8 socketColor, uint32 itemLevel, uint32 requiredLevel, bool allowJC) const;

    [[nodiscard]] size_t GetTotalGemsIndexed() const { return _totalGems; }
    [[nodiscard]] size_t GetUsableCandidatesIndexed() const { return _usableCandidates; }
    [[nodiscard]] size_t GetExcludedProfessionGems() const { return _excludedProfessionGems; }
    [[nodiscard]] size_t GetExcludedSpecialGems() const { return _excludedSpecialGems; }

private:
    NoGemsCatalog() = default;

    bool _initialized{false};
    size_t _totalGems{0};
    size_t _usableCandidates{0};
    size_t _excludedProfessionGems{0};
    size_t _excludedSpecialGems{0};

    std::vector<GemCatalogEntry> _catalog;
};

#define sNoGemsCatalog NoGemsCatalog::instance()

#endif // MOD_NO_GEMS_CATALOG_H
