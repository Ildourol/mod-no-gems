#include "NoGemsFormula.h"
#include "NoGemsConfig.h"
#include "DBCStructure.h"
#include "DBCStores.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include <algorithm>
#include <unordered_map>

namespace NoGemsFormula
{
    static StatTierBudget const STAT_BUDGETS[5] =
    {
        // Tier 1: iLvl < 100
        { 8,  12, 16, 9,  8,   8,  12, 16, 9,  8 },
        // Tier 2: iLvl 100 - 159
        { 10, 15, 20, 12, 10,  10, 15, 20, 12, 10 },
        // Tier 3: iLvl 160 - 199
        { 12, 18, 24, 14, 12,  12, 18, 24, 14, 12 },
        // Tier 4: iLvl 200 - 231
        { 16, 24, 32, 19, 16,  16, 24, 32, 19, 16 },
        // Tier 5: iLvl >= 232
        { 20, 30, 40, 23, 20,  20, 30, 40, 23, 20 }
    };

    bool HasSockets(ItemTemplate const* proto)
    {
        if (!proto)
            return false;

        for (uint32 s = 0; s < MAX_ITEM_PROTO_SOCKETS; ++s)
        {
            if (proto->Socket[s].Color != 0)
                return true;
        }
        return false;
    }

    char GetColorLetter(uint32 colorMask)
    {
        if (colorMask == NOGEMS_SOCKET_COLOR_META)
            return 'M';
        if (colorMask == NOGEMS_SOCKET_COLOR_RED)
            return 'R';
        if (colorMask == NOGEMS_SOCKET_COLOR_YELLOW)
            return 'Y';
        if (colorMask == NOGEMS_SOCKET_COLOR_BLUE)
            return 'B';
        if ((colorMask & NOGEMS_SOCKET_COLOR_RED) && (colorMask & NOGEMS_SOCKET_COLOR_YELLOW))
            return 'O';
        if ((colorMask & NOGEMS_SOCKET_COLOR_RED) && (colorMask & NOGEMS_SOCKET_COLOR_BLUE))
            return 'P';
        if ((colorMask & NOGEMS_SOCKET_COLOR_YELLOW) && (colorMask & NOGEMS_SOCKET_COLOR_BLUE))
            return 'G';
        return 'S';
    }

    std::string GetSocketSignature(ItemTemplate const* proto)
    {
        if (!proto)
            return "";

        std::string sig;
        for (uint32 s = 0; s < MAX_ITEM_PROTO_SOCKETS; ++s)
        {
            if (proto->Socket[s].Color != 0)
            {
                if (!sig.empty())
                    sig += "-";
                sig += GetColorLetter(proto->Socket[s].Color);
            }
        }
        return sig;
    }

    std::string GetItemModTypeName(uint32 modType)
    {
        switch (modType)
        {
            case ITEM_MOD_STRENGTH:                 return "Strength";
            case ITEM_MOD_AGILITY:                  return "Agility";
            case ITEM_MOD_STAMINA:                  return "Stamina";
            case ITEM_MOD_INTELLECT:                return "Intellect";
            case ITEM_MOD_SPIRIT:                   return "Spirit";
            case ITEM_MOD_ATTACK_POWER:             return "Attack Power";
            case ITEM_MOD_SPELL_POWER:              return "Spell Power";
            case ITEM_MOD_CRIT_RATING:              return "Critical Strike Rating";
            case ITEM_MOD_HASTE_RATING:             return "Haste Rating";
            case ITEM_MOD_HIT_RATING:               return "Hit Rating";
            case ITEM_MOD_DEFENSE_SKILL_RATING:     return "Defense Rating";
            case ITEM_MOD_RESILIENCE_RATING:        return "Resilience Rating";
            case ITEM_MOD_ARMOR_PENETRATION_RATING: return "Armor Penetration Rating";
            case ITEM_MOD_EXPERTISE_RATING:         return "Expertise Rating";
            case ITEM_MOD_DODGE_RATING:             return "Dodge Rating";
            case ITEM_MOD_PARRY_RATING:             return "Parry Rating";
            default:                                return "Stat";
        }
    }

    static uint8 GetItemTier(uint32 itemLevel)
    {
        if (itemLevel < 100)
            return 0;
        if (itemLevel < 160)
            return 1;
        if (itemLevel < 200)
            return 2;
        if (itemLevel < 232)
            return 3;
        return 4;
    }

    bool ItemHasStat(ItemTemplate const* proto, uint32 modType)
    {
        if (!proto)
            return false;

        // 1. Check ItemStat[]
        for (uint32 i = 0; i < proto->StatsCount; ++i)
        {
            if (proto->ItemStat[i].ItemStatType == modType && proto->ItemStat[i].ItemStatValue > 0)
                return true;
        }

        // 2. Check Spells[] for equip spells granting matching auras
        for (uint32 s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
        {
            if (proto->Spells[s].SpellId <= 0 || proto->Spells[s].SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP)
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(proto->Spells[s].SpellId);
            if (!spellInfo)
                continue;

            for (uint8 eff = 0; eff < MAX_SPELL_EFFECTS; ++eff)
            {
                if (spellInfo->Effects[eff].Effect != SPELL_EFFECT_APPLY_AURA)
                    continue;

                uint32 aura = spellInfo->Effects[eff].ApplyAuraName;
                uint32 misc = spellInfo->Effects[eff].MiscValue;

                if (aura == SPELL_AURA_MOD_RATING)
                {
                    if (modType == ITEM_MOD_CRIT_RATING && (misc & ((1 << CR_CRIT_MELEE) | (1 << CR_CRIT_RANGED) | (1 << CR_CRIT_SPELL))))
                        return true;
                    if (modType == ITEM_MOD_HIT_RATING && (misc & ((1 << CR_HIT_MELEE) | (1 << CR_HIT_RANGED) | (1 << CR_HIT_SPELL))))
                        return true;
                    if (modType == ITEM_MOD_HASTE_RATING && (misc & ((1 << CR_HASTE_MELEE) | (1 << CR_HASTE_RANGED) | (1 << CR_HASTE_SPELL))))
                        return true;
                    if (modType == ITEM_MOD_DEFENSE_SKILL_RATING && (misc & (1 << CR_DEFENSE_SKILL)))
                        return true;
                    if (modType == ITEM_MOD_DODGE_RATING && (misc & (1 << CR_DODGE)))
                        return true;
                    if (modType == ITEM_MOD_PARRY_RATING && (misc & (1 << CR_PARRY)))
                        return true;
                    if (modType == ITEM_MOD_RESILIENCE_RATING && (misc & ((1 << CR_CRIT_TAKEN_MELEE) | (1 << CR_CRIT_TAKEN_RANGED) | (1 << CR_CRIT_TAKEN_SPELL))))
                        return true;
                    if (modType == ITEM_MOD_ARMOR_PENETRATION_RATING && (misc & (1 << CR_ARMOR_PENETRATION)))
                        return true;
                }
                else if (aura == SPELL_AURA_MOD_ATTACK_POWER && modType == ITEM_MOD_ATTACK_POWER)
                    return true;
                else if ((aura == SPELL_AURA_MOD_DAMAGE_DONE_VERSUS || aura == SPELL_AURA_MOD_HEALING_DONE) && modType == ITEM_MOD_SPELL_POWER)
                    return true;
            }
        }

        return false;
    }

    uint32 SelectSynergisticModType(ItemTemplate const* proto, uint32 socketColor)
    {
        if (socketColor == NOGEMS_SOCKET_COLOR_META)
        {
            if (ItemHasStat(proto, ITEM_MOD_SPELL_POWER) || ItemHasStat(proto, ITEM_MOD_INTELLECT))
                return ITEM_MOD_SPELL_POWER;
            if (ItemHasStat(proto, ITEM_MOD_STRENGTH))
                return ITEM_MOD_STRENGTH;
            if (ItemHasStat(proto, ITEM_MOD_AGILITY))
                return ITEM_MOD_AGILITY;
            if (ItemHasStat(proto, ITEM_MOD_DEFENSE_SKILL_RATING) || ItemHasStat(proto, ITEM_MOD_DODGE_RATING))
                return ITEM_MOD_STAMINA;
            return ITEM_MOD_ATTACK_POWER;
        }

        if (socketColor == NOGEMS_SOCKET_COLOR_RED)
        {
            if (ItemHasStat(proto, ITEM_MOD_SPELL_POWER))
                return ITEM_MOD_SPELL_POWER;
            if (ItemHasStat(proto, ITEM_MOD_STRENGTH))
                return ITEM_MOD_STRENGTH;
            if (ItemHasStat(proto, ITEM_MOD_AGILITY))
                return ITEM_MOD_AGILITY;
            if (ItemHasStat(proto, ITEM_MOD_ARMOR_PENETRATION_RATING))
                return ITEM_MOD_ARMOR_PENETRATION_RATING;
            if (ItemHasStat(proto, ITEM_MOD_INTELLECT))
                return ITEM_MOD_SPELL_POWER;
            return ITEM_MOD_ATTACK_POWER;
        }

        if (socketColor == NOGEMS_SOCKET_COLOR_YELLOW)
        {
            if (ItemHasStat(proto, ITEM_MOD_CRIT_RATING))
                return ITEM_MOD_CRIT_RATING;
            if (ItemHasStat(proto, ITEM_MOD_HASTE_RATING))
                return ITEM_MOD_HASTE_RATING;
            if (ItemHasStat(proto, ITEM_MOD_HIT_RATING))
                return ITEM_MOD_HIT_RATING;
            if (ItemHasStat(proto, ITEM_MOD_DEFENSE_SKILL_RATING))
                return ITEM_MOD_DEFENSE_SKILL_RATING;
            if (ItemHasStat(proto, ITEM_MOD_RESILIENCE_RATING))
                return ITEM_MOD_RESILIENCE_RATING;
            if (ItemHasStat(proto, ITEM_MOD_INTELLECT))
                return ITEM_MOD_INTELLECT;
            return ITEM_MOD_CRIT_RATING;
        }

        if (socketColor == NOGEMS_SOCKET_COLOR_BLUE)
        {
            if (ItemHasStat(proto, ITEM_MOD_SPIRIT) && !ItemHasStat(proto, ITEM_MOD_DEFENSE_SKILL_RATING))
                return ITEM_MOD_SPIRIT;
            if (ItemHasStat(proto, ITEM_MOD_DODGE_RATING))
                return ITEM_MOD_DODGE_RATING;
            if (ItemHasStat(proto, ITEM_MOD_PARRY_RATING))
                return ITEM_MOD_PARRY_RATING;
            return ITEM_MOD_STAMINA;
        }

        // Mixed colors (Orange, Purple, Green, Prismatic)
        if ((socketColor & NOGEMS_SOCKET_COLOR_RED) && (socketColor & NOGEMS_SOCKET_COLOR_YELLOW))
            return SelectSynergisticModType(proto, NOGEMS_SOCKET_COLOR_RED);
        if ((socketColor & NOGEMS_SOCKET_COLOR_RED) && (socketColor & NOGEMS_SOCKET_COLOR_BLUE))
            return SelectSynergisticModType(proto, NOGEMS_SOCKET_COLOR_RED);
        if ((socketColor & NOGEMS_SOCKET_COLOR_YELLOW) && (socketColor & NOGEMS_SOCKET_COLOR_BLUE))
            return SelectSynergisticModType(proto, NOGEMS_SOCKET_COLOR_YELLOW);

        return ITEM_MOD_STAMINA;
    }

    int32 CalculateStatAmount(uint32 modType, uint32 itemLevel, bool isMeta)
    {
        uint8 tier = GetItemTier(itemLevel);
        StatTierBudget const& budget = STAT_BUDGETS[tier];
        float baseAmount = 0.0f;

        if (isMeta)
        {
            switch (modType)
            {
                case ITEM_MOD_STAMINA:      baseAmount = budget.meta_stamina; break;
                case ITEM_MOD_ATTACK_POWER: baseAmount = budget.meta_ap;      break;
                case ITEM_MOD_SPELL_POWER:  baseAmount = budget.meta_sp;      break;
                case ITEM_MOD_CRIT_RATING:  baseAmount = budget.meta_rating;  break;
                default:                    baseAmount = budget.meta_primary; break;
            }
        }
        else
        {
            switch (modType)
            {
                case ITEM_MOD_STAMINA:      baseAmount = budget.stamina; break;
                case ITEM_MOD_ATTACK_POWER: baseAmount = budget.ap;      break;
                case ITEM_MOD_SPELL_POWER:  baseAmount = budget.sp;      break;
                case ITEM_MOD_STRENGTH:
                case ITEM_MOD_AGILITY:
                case ITEM_MOD_INTELLECT:
                case ITEM_MOD_SPIRIT:       baseAmount = budget.primary; break;
                default:                    baseAmount = budget.rating;  break;
            }
        }

        float multiplier = sNoGemsConfig->StatMultiplier;
        if (isMeta)
            multiplier *= sNoGemsConfig->MetaMultiplier;

        if (multiplier <= 0.0f)
            return 0;

        int32 finalVal = static_cast<int32>(baseAmount * multiplier);
        return finalVal > 0 ? finalVal : 1;
    }

    bool ApplyNoGems(ItemTemplate* proto)
    {
        if (!proto || !HasSockets(proto))
            return false;

        // 1. Identify sockets and collect replacement affixes
        std::unordered_map<uint32, int32> aggregatedStats;
        for (uint32 s = 0; s < MAX_ITEM_PROTO_SOCKETS; ++s)
        {
            uint32 socketColor = proto->Socket[s].Color;
            if (socketColor == 0)
                continue;

            bool isMeta = (socketColor == NOGEMS_SOCKET_COLOR_META);
            uint32 modType = SelectSynergisticModType(proto, socketColor);
            int32 amount = CalculateStatAmount(modType, proto->ItemLevel, isMeta);

            if (amount > 0)
                aggregatedStats[modType] += amount;
        }

        // 2. Fold socket bonus if present and enabled
        if (proto->socketBonus != 0 && sNoGemsConfig->IncludeSocketBonus && sNoGemsConfig->SocketBonusMultiplier > 0.0f)
        {
            SpellItemEnchantmentEntry const* bonus = sSpellItemEnchantmentStore.LookupEntry(proto->socketBonus);
            if (bonus)
            {
                for (uint8 k = 0; k < MAX_SPELL_ITEM_ENCHANTMENT_EFFECTS; ++k)
                {
                    if (bonus->type[k] == ITEM_ENCHANTMENT_TYPE_STAT && bonus->amount[k] > 0)
                    {
                        int32 bonusAmt = static_cast<int32>(bonus->amount[k] * sNoGemsConfig->SocketBonusMultiplier);
                        if (bonusAmt > 0)
                            aggregatedStats[bonus->spellid[k]] += bonusAmt;
                    }
                }
            }
        }

        // 3. Zero out sockets completely
        for (uint32 s = 0; s < MAX_ITEM_PROTO_SOCKETS; ++s)
        {
            proto->Socket[s].Color = 0;
            proto->Socket[s].Content = 0;
        }
        proto->socketBonus = 0;

        // 4. Inject replacement stats into ItemStat[]
        // Either merge into an existing stat or append as a standard Blizzard stat
        for (auto const& [modType, amount] : aggregatedStats)
        {
            if (amount <= 0)
                continue;

            bool foundExisting = false;
            for (uint32 i = 0; i < proto->StatsCount; ++i)
            {
                if (proto->ItemStat[i].ItemStatType == modType)
                {
                    proto->ItemStat[i].ItemStatValue += amount;
                    foundExisting = true;
                    break;
                }
            }

            if (!foundExisting && proto->StatsCount < MAX_ITEM_PROTO_STATS)
            {
                proto->ItemStat[proto->StatsCount].ItemStatType = modType;
                proto->ItemStat[proto->StatsCount].ItemStatValue = amount;
                proto->StatsCount++;
            }
            else if (!foundExisting)
            {
                // Overflow protection: fold into first stat so budget is never lost
                if (proto->StatsCount > 0)
                {
                    proto->ItemStat[0].ItemStatValue += amount;
                }
                else if (MAX_ITEM_PROTO_STATS > 0)
                {
                    proto->ItemStat[0].ItemStatType = modType;
                    proto->ItemStat[0].ItemStatValue = amount;
                    proto->StatsCount = 1;
                }
            }
        }

        // 5. Group stats: all white base stats (types <= 7) first,
        // followed by all green equip rating lines (types >= 12)
        auto IsBaseStat = [](uint32 modType) -> bool {
            return (modType <= ITEM_MOD_STAMINA);
        };

        std::stable_sort(proto->ItemStat, proto->ItemStat + proto->StatsCount,
            [&](_ItemStat const& a, _ItemStat const& b) {
                bool aBase = IsBaseStat(a.ItemStatType);
                bool bBase = IsBaseStat(b.ItemStatType);
                if (aBase != bBase)
                    return aBase; // Base stats come first
                return false;     // Preserve relative order within each category
            });

        // Note: proto->Description is intentionally untouched.
        // No custom yellow flavor text will appear on the item.
        return true;
    }
}
