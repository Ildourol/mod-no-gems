#include "NoGemsAffixMgr.h"
#include "NoGemsConfig.h"
#include "NoGemsCatalog.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Random.h"
#include "StringFormat.h"

NoGemsAffixMgr* NoGemsAffixMgr::instance()
{
    static NoGemsAffixMgr instance;
    return &instance;
}

void NoGemsAffixMgr::InitializeDatabase()
{
    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `mod_no_gems_item_affix` ("
        "  `item_guid` INT UNSIGNED NOT NULL,"
        "  `item_entry` INT UNSIGNED NOT NULL,"
        "  `roll_version` INT UNSIGNED NOT NULL DEFAULT 1,"
        "  `socket_signature` VARCHAR(32) NOT NULL DEFAULT '',"
        "  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (`item_guid`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;"
    );

    CharacterDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `mod_no_gems_item_affix_effect` ("
        "  `item_guid` INT UNSIGNED NOT NULL,"
        "  `socket_index` TINYINT UNSIGNED NOT NULL,"
        "  `socket_type` TINYINT UNSIGNED NOT NULL,"
        "  `source_gem_entry` INT UNSIGNED NOT NULL DEFAULT 0,"
        "  `source_enchant_id` INT UNSIGNED NOT NULL DEFAULT 0,"
        "  `mod_type` INT UNSIGNED NOT NULL DEFAULT 0,"
        "  `amount` INT NOT NULL DEFAULT 0,"
        "  PRIMARY KEY (`item_guid`, `socket_index`),"
        "  KEY `fk_mod_no_gems_item_affix` (`item_guid`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;"
    );

    LOG_INFO("server.loading", "mod-no-gems: Database tables verified/initialized.");
}

bool NoGemsAffixMgr::HasSockets(Item const* item)
{
    if (!item)
        return false;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return false;

    if (proto->Socket[0].Color != 0)
        return true;

    if (item->GetEnchantmentId(PRISMATIC_ENCHANTMENT_SLOT))
        return true;

    return false;
}

std::string NoGemsAffixMgr::ComputeSocketSignature(Item const* item, std::vector<uint8>& socketColors)
{
    socketColors.clear();
    if (!item)
        return "";

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return "";

    std::string sig;
    for (int i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
    {
        uint8 color = proto->Socket[i].Color;
        if (!color)
            continue;

        socketColors.push_back(color);
        if (!sig.empty())
            sig += "-";

        if (color & SOCKET_COLOR_META)
            sig += "M";
        else if (color & SOCKET_COLOR_RED)
            sig += "R";
        else if (color & SOCKET_COLOR_YELLOW)
            sig += "Y";
        else if (color & SOCKET_COLOR_BLUE)
            sig += "B";
        else
            sig += "P";
    }

    if (item->GetEnchantmentId(PRISMATIC_ENCHANTMENT_SLOT))
    {
        socketColors.push_back(0); // 0 indicates prismatic added socket
        if (!sig.empty())
            sig += "-";
        sig += "PRISM";
    }

    return sig;
}

ItemAffixRecord const* NoGemsAffixMgr::GetOrLoadAffix(Item* item)
{
    if (!item)
        return nullptr;

    uint32 lowGuid = item->GetGUID().GetCounter();
    auto it = _affixCache.find(lowGuid);
    if (it != _affixCache.end())
        return &it->second;

    QueryResult result = CharacterDatabase.Query(
        "SELECT item_entry, roll_version, socket_signature FROM mod_no_gems_item_affix WHERE item_guid = {}",
        lowGuid);

    if (!result)
        return nullptr;

    Field* fields = result->Fetch();
    ItemAffixRecord record;
    record.itemGuid = lowGuid;
    record.itemEntry = fields[0].Get<uint32>();
    record.rollVersion = fields[1].Get<uint32>();
    record.socketSignature = fields[2].Get<std::string>();

    QueryResult effectsResult = CharacterDatabase.Query(
        "SELECT socket_index, socket_type, source_gem_entry, source_enchant_id, mod_type, amount "
        "FROM mod_no_gems_item_affix_effect WHERE item_guid = {} ORDER BY socket_index ASC",
        lowGuid);

    if (effectsResult)
    {
        do
        {
            Field* effFields = effectsResult->Fetch();
            ItemAffixEffectRecord eff;
            eff.socketIndex = effFields[0].Get<uint8>();
            eff.socketType = effFields[1].Get<uint8>();
            eff.sourceGemEntry = effFields[2].Get<uint32>();
            eff.sourceEnchantId = effFields[3].Get<uint32>();
            eff.modType = effFields[4].Get<uint32>();
            eff.amount = effFields[5].Get<int32>();
            record.effects.push_back(eff);
        } while (effectsResult->NextRow());
    }

    auto inserted = _affixCache.emplace(lowGuid, std::move(record));
    return &inserted.first->second;
}

ItemAffixRecord const* NoGemsAffixMgr::EnsureAffix(Item* item)
{
    if (!item)
        return nullptr;

    if (!sNoGemsConfig->IsReplacementEnabled())
        return nullptr;

    ItemAffixRecord const* existing = GetOrLoadAffix(item);
    if (existing)
        return existing;

    return GenerateAffix(item);
}

ItemAffixRecord const* NoGemsAffixMgr::GenerateAffix(Item* item)
{
    if (!item)
        return nullptr;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return nullptr;

    // Check config eligibility filters
    if (proto->Quality < sNoGemsConfig->GetMinItemQuality())
        return nullptr;

    if (sNoGemsConfig->GetMinItemLevel() > 0 && proto->ItemLevel < sNoGemsConfig->GetMinItemLevel())
        return nullptr;

    if (sNoGemsConfig->GetMaxItemLevel() > 0 && proto->ItemLevel > sNoGemsConfig->GetMaxItemLevel())
        return nullptr;

    std::vector<uint8> socketColors;
    std::string signature = ComputeSocketSignature(item, socketColors);
    if (socketColors.empty())
        return nullptr;

    uint32 lowGuid = item->GetGUID().GetCounter();

    ItemAffixRecord record;
    record.itemGuid = lowGuid;
    record.itemEntry = proto->ItemId;
    record.rollVersion = 1;
    record.socketSignature = signature;

    bool allowJC = sNoGemsConfig->IsAllowProfessionExclusive();

    for (size_t i = 0; i < socketColors.size(); ++i)
    {
        uint8 sColor = socketColors[i];
        std::vector<GemCatalogEntry const*> candidates = sNoGemsCatalog->GetCandidates(sColor, proto->ItemLevel, proto->RequiredLevel, allowJC);
        if (candidates.empty())
            continue;

        uint32 randIdx = urand(0, static_cast<uint32>(candidates.size() - 1));
        GemCatalogEntry const* chosen = candidates[randIdx];

        if (chosen->statComponents.empty())
            continue;

        // Choose a random stat component from the chosen gem
        uint32 compIdx = urand(0, static_cast<uint32>(chosen->statComponents.size() - 1));
        ReplacementStatComponent const& comp = chosen->statComponents[compIdx];

        ItemAffixEffectRecord eff;
        eff.socketIndex = static_cast<uint8>(i);
        eff.socketType = sColor;
        eff.sourceGemEntry = chosen->itemId;
        eff.sourceEnchantId = chosen->enchantId;
        eff.modType = comp.modType;
        eff.amount = comp.amount;

        record.effects.push_back(eff);
    }

    if (record.effects.empty())
        return nullptr;

    SaveAffixToDB(record);

    auto inserted = _affixCache.emplace(lowGuid, std::move(record));
    if (sNoGemsConfig->IsDebugLogging())
    {
        LOG_INFO("server.loading", "mod-no-gems: Generated affix for item GUID {} (Entry {}): {} effects.",
            lowGuid, proto->ItemId, inserted.first->second.effects.size());
    }

    return &inserted.first->second;
}

void NoGemsAffixMgr::SaveAffixToDB(ItemAffixRecord const& affix)
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    trans->Append(
        "INSERT INTO `mod_no_gems_item_affix` (`item_guid`, `item_entry`, `roll_version`, `socket_signature`) "
        "VALUES ({}, {}, {}, '{}') "
        "ON DUPLICATE KEY UPDATE `roll_version` = `roll_version`",
        affix.itemGuid, affix.itemEntry, affix.rollVersion, affix.socketSignature);

    for (ItemAffixEffectRecord const& eff : affix.effects)
    {
        trans->Append(
            "INSERT INTO `mod_no_gems_item_affix_effect` (`item_guid`, `socket_index`, `socket_type`, `source_gem_entry`, `source_enchant_id`, `mod_type`, `amount`) "
            "VALUES ({}, {}, {}, {}, {}, {}, {}) "
            "ON DUPLICATE KEY UPDATE `amount` = VALUES(`amount`)",
            affix.itemGuid, eff.socketIndex, eff.socketType, eff.sourceGemEntry, eff.sourceEnchantId, eff.modType, eff.amount);
    }

    CharacterDatabase.CommitTransaction(trans);
}

void NoGemsAffixMgr::ApplyAffix(Player* player, Item* item, bool apply)
{
    if (!player || !item)
        return;

    if (!sNoGemsConfig->IsReplacementEnabled())
        return;

    ItemAffixRecord const* affix = EnsureAffix(item);
    if (!affix || affix->effects.empty())
        return;

    for (ItemAffixEffectRecord const& eff : affix->effects)
    {
        int32 amount = eff.amount;
        if (!amount)
            continue;

        switch (eff.modType)
        {
            case ITEM_MOD_MANA:
                player->HandleStatFlatModifier(UNIT_MOD_MANA, BASE_VALUE, float(amount), apply);
                break;
            case ITEM_MOD_HEALTH:
                player->HandleStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(amount), apply);
                break;
            case ITEM_MOD_AGILITY:
                player->HandleStatFlatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, float(amount), apply);
                player->UpdateStatBuffMod(STAT_AGILITY);
                break;
            case ITEM_MOD_STRENGTH:
                player->HandleStatFlatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, float(amount), apply);
                player->UpdateStatBuffMod(STAT_STRENGTH);
                break;
            case ITEM_MOD_INTELLECT:
                player->HandleStatFlatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, float(amount), apply);
                player->UpdateStatBuffMod(STAT_INTELLECT);
                break;
            case ITEM_MOD_SPIRIT:
                player->HandleStatFlatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, float(amount), apply);
                player->UpdateStatBuffMod(STAT_SPIRIT);
                break;
            case ITEM_MOD_STAMINA:
                player->HandleStatFlatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, float(amount), apply);
                player->UpdateStatBuffMod(STAT_STAMINA);
                break;
            case ITEM_MOD_DEFENSE_SKILL_RATING:
                player->ApplyRatingMod(CR_DEFENSE_SKILL, amount, apply);
                break;
            case ITEM_MOD_DODGE_RATING:
                player->ApplyRatingMod(CR_DODGE, amount, apply);
                break;
            case ITEM_MOD_PARRY_RATING:
                player->ApplyRatingMod(CR_PARRY, amount, apply);
                break;
            case ITEM_MOD_BLOCK_RATING:
                player->ApplyRatingMod(CR_BLOCK, amount, apply);
                break;
            case ITEM_MOD_HIT_MELEE_RATING:
                player->ApplyRatingMod(CR_HIT_MELEE, amount, apply);
                break;
            case ITEM_MOD_HIT_RANGED_RATING:
                player->ApplyRatingMod(CR_HIT_RANGED, amount, apply);
                break;
            case ITEM_MOD_HIT_SPELL_RATING:
                player->ApplyRatingMod(CR_HIT_SPELL, amount, apply);
                break;
            case ITEM_MOD_CRIT_MELEE_RATING:
                player->ApplyRatingMod(CR_CRIT_MELEE, amount, apply);
                break;
            case ITEM_MOD_CRIT_RANGED_RATING:
                player->ApplyRatingMod(CR_CRIT_RANGED, amount, apply);
                break;
            case ITEM_MOD_CRIT_SPELL_RATING:
                player->ApplyRatingMod(CR_CRIT_SPELL, amount, apply);
                break;
            case ITEM_MOD_HASTE_RANGED_RATING:
                player->ApplyRatingMod(CR_HASTE_RANGED, amount, apply);
                break;
            case ITEM_MOD_HASTE_SPELL_RATING:
                player->ApplyRatingMod(CR_HASTE_SPELL, amount, apply);
                break;
            case ITEM_MOD_HIT_RATING:
                player->ApplyRatingMod(CR_HIT_MELEE, amount, apply);
                player->ApplyRatingMod(CR_HIT_RANGED, amount, apply);
                player->ApplyRatingMod(CR_HIT_SPELL, amount, apply);
                break;
            case ITEM_MOD_CRIT_RATING:
                player->ApplyRatingMod(CR_CRIT_MELEE, amount, apply);
                player->ApplyRatingMod(CR_CRIT_RANGED, amount, apply);
                player->ApplyRatingMod(CR_CRIT_SPELL, amount, apply);
                break;
            case ITEM_MOD_RESILIENCE_RATING:
                player->ApplyRatingMod(CR_CRIT_TAKEN_MELEE, amount, apply);
                player->ApplyRatingMod(CR_CRIT_TAKEN_RANGED, amount, apply);
                player->ApplyRatingMod(CR_CRIT_TAKEN_SPELL, amount, apply);
                break;
            case ITEM_MOD_HASTE_RATING:
                player->ApplyRatingMod(CR_HASTE_MELEE, amount, apply);
                player->ApplyRatingMod(CR_HASTE_RANGED, amount, apply);
                player->ApplyRatingMod(CR_HASTE_SPELL, amount, apply);
                break;
            case ITEM_MOD_EXPERTISE_RATING:
                player->ApplyRatingMod(CR_EXPERTISE, amount, apply);
                break;
            case ITEM_MOD_ATTACK_POWER:
                player->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(amount), apply);
                player->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(amount), apply);
                break;
            case ITEM_MOD_RANGED_ATTACK_POWER:
                player->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(amount), apply);
                break;
            case ITEM_MOD_MANA_REGENERATION:
                player->ApplyManaRegenBonus(amount, apply);
                break;
            case ITEM_MOD_ARMOR_PENETRATION_RATING:
                player->ApplyRatingMod(CR_ARMOR_PENETRATION, amount, apply);
                break;
            case ITEM_MOD_SPELL_POWER:
                player->ApplySpellPowerBonus(amount, apply);
                break;
            case ITEM_MOD_HEALTH_REGEN:
                player->ApplyHealthRegenBonus(amount, apply);
                break;
            case ITEM_MOD_SPELL_PENETRATION:
                player->ApplySpellPenetrationBonus(amount, apply);
                break;
            case ITEM_MOD_BLOCK_VALUE:
                player->HandleBaseModFlatValue(SHIELD_BLOCK_VALUE, float(amount), apply);
                break;
            default:
                break;
        }
    }
}

void NoGemsAffixMgr::RemoveFromCache(uint32 itemGuid)
{
    _affixCache.erase(itemGuid);
}

static char const* GetModTypeName(uint32 modType)
{
    switch (modType)
    {
        case ITEM_MOD_MANA: return "Mana";
        case ITEM_MOD_HEALTH: return "Health";
        case ITEM_MOD_AGILITY: return "Agility";
        case ITEM_MOD_STRENGTH: return "Strength";
        case ITEM_MOD_INTELLECT: return "Intellect";
        case ITEM_MOD_SPIRIT: return "Spirit";
        case ITEM_MOD_STAMINA: return "Stamina";
        case ITEM_MOD_DEFENSE_SKILL_RATING: return "Defense Rating";
        case ITEM_MOD_DODGE_RATING: return "Dodge Rating";
        case ITEM_MOD_PARRY_RATING: return "Parry Rating";
        case ITEM_MOD_BLOCK_RATING: return "Block Rating";
        case ITEM_MOD_HIT_MELEE_RATING: return "Melee Hit Rating";
        case ITEM_MOD_HIT_RANGED_RATING: return "Ranged Hit Rating";
        case ITEM_MOD_HIT_SPELL_RATING: return "Spell Hit Rating";
        case ITEM_MOD_CRIT_MELEE_RATING: return "Melee Crit Rating";
        case ITEM_MOD_CRIT_RANGED_RATING: return "Ranged Crit Rating";
        case ITEM_MOD_CRIT_SPELL_RATING: return "Spell Crit Rating";
        case ITEM_MOD_HASTE_RANGED_RATING: return "Ranged Haste Rating";
        case ITEM_MOD_HASTE_SPELL_RATING: return "Spell Haste Rating";
        case ITEM_MOD_HIT_RATING: return "Hit Rating";
        case ITEM_MOD_CRIT_RATING: return "Critical Strike Rating";
        case ITEM_MOD_RESILIENCE_RATING: return "Resilience Rating";
        case ITEM_MOD_HASTE_RATING: return "Haste Rating";
        case ITEM_MOD_EXPERTISE_RATING: return "Expertise Rating";
        case ITEM_MOD_ATTACK_POWER: return "Attack Power";
        case ITEM_MOD_RANGED_ATTACK_POWER: return "Ranged Attack Power";
        case ITEM_MOD_MANA_REGENERATION: return "Mana per 5 sec.";
        case ITEM_MOD_ARMOR_PENETRATION_RATING: return "Armor Penetration Rating";
        case ITEM_MOD_SPELL_POWER: return "Spell Power";
        case ITEM_MOD_HEALTH_REGEN: return "Health per 5 sec.";
        case ITEM_MOD_SPELL_PENETRATION: return "Spell Penetration";
        case ITEM_MOD_BLOCK_VALUE: return "Block Value";
        default: return "Unknown Stat";
    }
}

static char const* GetSocketColorName(uint8 socketType)
{
    if (socketType == 0) return "Prismatic";
    if (socketType & SOCKET_COLOR_META) return "Meta";
    if (socketType & SOCKET_COLOR_RED) return "Red";
    if (socketType & SOCKET_COLOR_YELLOW) return "Yellow";
    if (socketType & SOCKET_COLOR_BLUE) return "Blue";
    return "Socket";
}

std::string NoGemsAffixMgr::FormatAffix(ItemAffixRecord const& affix)
{
    std::string out = Acore::StringFormat("Roll version: {} | Signature: {}\n", affix.rollVersion, affix.socketSignature);
    for (ItemAffixEffectRecord const& eff : affix.effects)
    {
        out += Acore::StringFormat("  Socket {} [{}]: +{} {}\n",
            eff.socketIndex + 1,
            GetSocketColorName(eff.socketType),
            eff.amount,
            GetModTypeName(eff.modType));
    }
    return out;
}
