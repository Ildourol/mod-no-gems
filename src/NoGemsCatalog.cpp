#include "NoGemsCatalog.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Log.h"
#include "ObjectMgr.h"
#include <algorithm>

NoGemsCatalog* NoGemsCatalog::instance()
{
    static NoGemsCatalog instance;
    return &instance;
}

void NoGemsCatalog::Initialize()
{
    _catalog.clear();
    _totalGems = 0;
    _usableCandidates = 0;
    _excludedProfessionGems = 0;
    _excludedSpecialGems = 0;

    ItemTemplateContainer const* itemTemplates = sObjectMgr->GetItemTemplateStore();
    if (!itemTemplates)
    {
        LOG_ERROR("server.loading", "mod-no-gems: Failed to retrieve item template store!");
        return;
    }

    for (auto const& [itemId, pProto] : *itemTemplates)
    {
        if (pProto.Class != ITEM_CLASS_GEM)
            continue;

        ++_totalGems;

        if (!pProto.GemProperties)
            continue;

        GemPropertiesEntry const* gemProperty = sGemPropertiesStore.LookupEntry(pProto.GemProperties);
        if (!gemProperty)
            continue;

        uint32 enchantId = gemProperty->spellitemenchantement;
        if (!enchantId)
            continue;

        SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(enchantId);
        if (!enchantEntry)
            continue;

        bool hasSkillReq = (pProto.RequiredSkill > 0 || enchantEntry->requiredSkill > 0);
        if (hasSkillReq)
            ++_excludedProfessionGems;

        // Parse stat components from enchantment
        std::vector<ReplacementStatComponent> components;
        bool hasUnsupportedEffect = false;

        for (int s = 0; s < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++s)
        {
            uint32 type = enchantEntry->type[s];
            uint32 amount = enchantEntry->amount[s];
            uint32 spellid = enchantEntry->spellid[s];

            if (type == ITEM_ENCHANTMENT_TYPE_NONE)
                continue;

            if (type == ITEM_ENCHANTMENT_TYPE_STAT)
            {
                if (amount > 0)
                    components.push_back({ spellid, static_cast<int32>(amount) });
            }
            else if (type == ITEM_ENCHANTMENT_TYPE_RESISTANCE)
            {
                // Resistances can be handled or treated as special. We support flat stats / ratings primarily.
            }
            else
            {
                // Combat spells, procs, use spells, etc.
                hasUnsupportedEffect = true;
            }
        }

        // Meta gems frequently have procs/conditions. For safe replacement, we only include entries with pure stat components
        if (components.empty())
        {
            ++_excludedSpecialGems;
            continue;
        }

        GemCatalogEntry entry;
        entry.itemId = itemId;
        entry.subclass = pProto.SubClass;
        entry.colorMask = gemProperty->color;
        entry.requiredLevel = std::max(pProto.RequiredLevel, enchantEntry->requiredLevel);
        entry.itemLevel = pProto.ItemLevel;
        entry.quality = pProto.Quality;
        entry.isProfessionExclusive = hasSkillReq;
        entry.enchantId = enchantId;
        entry.statComponents = std::move(components);

        _catalog.push_back(std::move(entry));
        ++_usableCandidates;
    }

    _initialized = true;

    LOG_INFO("server.loading", "mod-no-gems: Catalog indexed {} gem templates ({} usable replacement candidates, {} profession-restricted, {} special/proc excluded).",
        _totalGems, _usableCandidates, _excludedProfessionGems, _excludedSpecialGems);
}

std::vector<GemCatalogEntry const*> NoGemsCatalog::GetCandidates(uint8 socketColor, uint32 itemLevel, uint32 requiredLevel, bool allowJC) const
{
    std::vector<GemCatalogEntry const*> result;

    for (GemCatalogEntry const& gem : _catalog)
    {
        if (!allowJC && gem.isProfessionExclusive)
            continue;

        // Check socket color compatibility
        // If socketColor is 0, it's considered prismatic
        if (socketColor != 0)
        {
            if (!(gem.colorMask & socketColor))
                continue;
        }
        else
        {
            // Prismatic socket accepts any normal non-meta gem
            if (gem.colorMask & SOCKET_COLOR_META)
                continue;
        }

        // Tier / progression matching:
        // Candidate gems should not require a level much higher than the item requires.
        if (gem.requiredLevel > 0 && requiredLevel > 0 && gem.requiredLevel > requiredLevel + 5)
            continue;

        // Compare itemLevel loosely to maintain sensible progression tiers (e.g. TBC gems vs Wrath gems)
        if (itemLevel > 0 && gem.itemLevel > 0)
        {
            if (itemLevel <= 60 && gem.itemLevel > 80)
                continue;
            if (itemLevel <= 115 && gem.itemLevel > 140)
                continue;
        }

        result.push_back(&gem);
    }

    // Fallback: if no candidates matched due to strict level filtering, loosen requiredLevel restriction
    if (result.empty())
    {
        for (GemCatalogEntry const& gem : _catalog)
        {
            if (!allowJC && gem.isProfessionExclusive)
                continue;

            if (socketColor != 0)
            {
                if (!(gem.colorMask & socketColor))
                    continue;
            }
            else
            {
                if (gem.colorMask & SOCKET_COLOR_META)
                    continue;
            }

            result.push_back(&gem);
        }
    }

    return result;
}
