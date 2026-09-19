--[[
    mod-no-gems: Server-Side Lua Module for AzerothCore (mod-ale / Eluna)
    
    Hardcoded Server Rules:
    1. Zero configuration file: all rules, budgets, and policies are hardcoded.
    2. Gem insertion is completely blocked via CMSG_SOCKET_GEMS (0x347) interception.
    3. Existing socketed gems and socket bonuses remain dormant with zero gameplay effect.
    4. Equipment with sockets receives a one-time randomized replacement affix per socket.
    5. Affixes are permanently persisted to the database (mod_no_gems_item_affix / mod_no_gems_item_affix_effect)
       keyed by item GUID, surviving restarts, trades, mail, and ownership changes.
    6. Affixes are applied to players on equip and removed on unequip.
    7. Affix details are transmitted via Addon Messages (MOD_NOGEMS) for client tooltip display.
    8. Chat inspection command: .nogems item
]]

local MODULE_NAME = "mod-no-gems"
local MODULE_VERSION = "1.0.0"

-- Hardcoded Module Configuration
local CONFIG = {
    Enable                 = true,  -- Master switch
    BlockSocketing         = true,  -- Reject CMSG_SOCKET_GEMS packets
    AffixesEnabled         = true,  -- Enable randomized socket replacement affixes
    RollVersion            = 1,     -- Roll algorithm version
    AddonPrefix            = "MOD_NOGEMS",
    ChatCommand            = "nogems",
}

-- Socket Color Mask Constants (WoW 3.3.5a)
local SOCKET_COLOR_META     = 1
local SOCKET_COLOR_RED      = 2
local SOCKET_COLOR_YELLOW   = 4
local SOCKET_COLOR_BLUE     = 8

-- Stat Modifier Type Identifiers
local MOD_TYPE_STRENGTH     = 1
local MOD_TYPE_AGILITY      = 2
local MOD_TYPE_STAMINA      = 3
local MOD_TYPE_INTELLECT    = 4
local MOD_TYPE_SPIRIT       = 5
local MOD_TYPE_ATTACK_POWER = 6
local MOD_TYPE_SPELL_POWER  = 7
local MOD_TYPE_CRIT_RATING  = 8
local MOD_TYPE_HASTE_RATING = 9
local MOD_TYPE_HIT_RATING   = 10
local MOD_TYPE_DEFENSE      = 11
local MOD_TYPE_RESILIENCE   = 12
local MOD_TYPE_ARMOR_PEN    = 13
local MOD_TYPE_EXPERTISE    = 14
local MOD_TYPE_DODGE        = 15
local MOD_TYPE_PARRY        = 16

-- Stat Display Names
local STAT_NAMES = {
    [MOD_TYPE_STRENGTH]     = "Strength",
    [MOD_TYPE_AGILITY]      = "Agility",
    [MOD_TYPE_STAMINA]      = "Stamina",
    [MOD_TYPE_INTELLECT]    = "Intellect",
    [MOD_TYPE_SPIRIT]       = "Spirit",
    [MOD_TYPE_ATTACK_POWER] = "Attack Power",
    [MOD_TYPE_SPELL_POWER]  = "Spell Power",
    [MOD_TYPE_CRIT_RATING]  = "Critical Strike Rating",
    [MOD_TYPE_HASTE_RATING] = "Haste Rating",
    [MOD_TYPE_HIT_RATING]   = "Hit Rating",
    [MOD_TYPE_DEFENSE]      = "Defense Rating",
    [MOD_TYPE_RESILIENCE]   = "Resilience Rating",
    [MOD_TYPE_ARMOR_PEN]    = "Armor Penetration Rating",
    [MOD_TYPE_EXPERTISE]    = "Expertise Rating",
    [MOD_TYPE_DODGE]        = "Dodge Rating",
    [MOD_TYPE_PARRY]        = "Parry Rating",
}

-- Combat Rating Enum Mappings for player:ApplyRatingMod(ratingId, value, apply)
local CR_DEFENSE_SKILL      = 1
local CR_DODGE              = 2
local CR_PARRY              = 3
local CR_HIT_MELEE          = 5
local CR_HIT_RANGED         = 6
local CR_HIT_SPELL          = 7
local CR_CRIT_MELEE         = 8
local CR_CRIT_RANGED        = 9
local CR_CRIT_SPELL         = 10
local CR_CRIT_TAKEN_MELEE   = 14
local CR_CRIT_TAKEN_RANGED  = 15
local CR_CRIT_TAKEN_SPELL   = 16
local CR_HASTE_MELEE        = 17
local CR_HASTE_RANGED       = 18
local CR_HASTE_SPELL        = 19
local CR_EXPERTISE          = 23
local CR_ARMOR_PENETRATION  = 24

-- Unit Modifier Flat Stat Indices for unit:HandleStatFlatModifier(stat, type, value, apply)
-- type: 1 = TOTAL_VALUE
local UNIT_MOD_STRENGTH     = 0
local UNIT_MOD_AGILITY      = 1
local UNIT_MOD_STAMINA      = 2
local UNIT_MOD_INTELLECT    = 3
local UNIT_MOD_SPIRIT       = 4
local UNIT_MOD_ATTACK_POWER = 20
local UNIT_MOD_AP_RANGED    = 21

-- Progression Tiers (Derived from Blizzard TBC & WotLK Gem Itemization)
-- Tier 1: iLvl < 100 (TBC Leveling / Normal Dungeons)
-- Tier 2: iLvl 100 - 159 (TBC Endgame / Heroics / T4-T6)
-- Tier 3: iLvl 160 - 199 (WotLK Leveling / Normal Dungeons)
-- Tier 4: iLvl 200 - 231 (WotLK Entry Raids / Heroic Dungeons)
-- Tier 5: iLvl >= 232 (WotLK Endgame Raids / ICC / RS)
local STAT_BUDGETS = {
    [1] = { primary = 8,  stamina = 12, ap = 16, sp = 9,  rating = 8,  meta_primary = 12, meta_stamina = 18, meta_ap = 24, meta_sp = 14, meta_rating = 12 },
    [2] = { primary = 10, stamina = 15, ap = 20, sp = 12, rating = 10, meta_primary = 14, meta_stamina = 22, meta_ap = 30, meta_sp = 18, meta_rating = 14 },
    [3] = { primary = 12, stamina = 18, ap = 24, sp = 14, rating = 12, meta_primary = 18, meta_stamina = 25, meta_ap = 42, meta_sp = 25, meta_rating = 18 },
    [4] = { primary = 16, stamina = 24, ap = 32, sp = 19, rating = 16, meta_primary = 21, meta_stamina = 30, meta_ap = 50, meta_sp = 30, meta_rating = 21 },
    [5] = { primary = 20, stamina = 30, ap = 40, sp = 23, rating = 20, meta_primary = 25, meta_stamina = 45, meta_ap = 84, meta_sp = 42, meta_rating = 25 },
}

-- Candidate Stat Pools by Socket Color (Authentic Gem Themes)
local POOL_RED = {
    MOD_TYPE_STRENGTH,
    MOD_TYPE_AGILITY,
    MOD_TYPE_ATTACK_POWER,
    MOD_TYPE_SPELL_POWER,
    MOD_TYPE_ARMOR_PEN,
    MOD_TYPE_EXPERTISE,
}

local POOL_YELLOW = {
    MOD_TYPE_CRIT_RATING,
    MOD_TYPE_HASTE_RATING,
    MOD_TYPE_HIT_RATING,
    MOD_TYPE_DEFENSE,
    MOD_TYPE_RESILIENCE,
    MOD_TYPE_INTELLECT,
}

local POOL_BLUE = {
    MOD_TYPE_STAMINA,
    MOD_TYPE_SPIRIT,
    MOD_TYPE_DODGE,
    MOD_TYPE_PARRY,
}

local POOL_META = {
    MOD_TYPE_STRENGTH,
    MOD_TYPE_AGILITY,
    MOD_TYPE_STAMINA,
    MOD_TYPE_INTELLECT,
    MOD_TYPE_ATTACK_POWER,
    MOD_TYPE_SPELL_POWER,
    MOD_TYPE_CRIT_RATING,
}

-- Memory Caches
local itemTemplateSocketCache = {}  -- [entry] = { count = N, sockets = { color1, color2, color3 }, signature = "R-Y-B" }
local itemAffixCache          = {}  -- [itemGuid] = { signature = "...", effects = { { socketIndex, socketType, modType, amount }, ... } }
local playerEquippedAffixes   = {}  -- [playerGuid] = { [itemGuid] = { effects = { ... } } }

----------------------------------------------------------------------------------------------------
-- Database Schema Initialization
----------------------------------------------------------------------------------------------------
local function InitDatabase()
    CharDBExecute([[
        CREATE TABLE IF NOT EXISTS `mod_no_gems_item_affix` (
            `item_guid` INT UNSIGNED NOT NULL,
            `item_entry` INT UNSIGNED NOT NULL,
            `roll_version` INT UNSIGNED NOT NULL DEFAULT 1,
            `socket_signature` VARCHAR(32) NOT NULL DEFAULT '',
            `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (`item_guid`)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
    ]])

    CharDBExecute([[
        CREATE TABLE IF NOT EXISTS `mod_no_gems_item_affix_effect` (
            `item_guid` INT UNSIGNED NOT NULL,
            `socket_index` TINYINT UNSIGNED NOT NULL,
            `socket_type` TINYINT UNSIGNED NOT NULL,
            `source_gem_entry` INT UNSIGNED NOT NULL DEFAULT 0,
            `source_enchant_id` INT UNSIGNED NOT NULL DEFAULT 0,
            `mod_type` INT UNSIGNED NOT NULL DEFAULT 0,
            `amount` INT NOT NULL DEFAULT 0,
            PRIMARY KEY (`item_guid`, `socket_index`),
            CONSTRAINT `fk_mod_no_gems_item_affix` FOREIGN KEY (`item_guid`) REFERENCES `mod_no_gems_item_affix` (`item_guid`) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
    ]])
    print(string.format("[%s] Database tables verified.", MODULE_NAME))
end

----------------------------------------------------------------------------------------------------
-- Helper: Determine Item Progression Tier
----------------------------------------------------------------------------------------------------
local function GetItemProgressionTier(itemLevel)
    if not itemLevel or itemLevel < 100 then
        return 1
    elseif itemLevel < 160 then
        return 2
    elseif itemLevel < 200 then
        return 3
    elseif itemLevel < 232 then
        return 4
    else
        return 5
    end
end

----------------------------------------------------------------------------------------------------
-- Helper: Calculate Stat Amount for a Given Mod Type and Item Level
----------------------------------------------------------------------------------------------------
local function GetStatAmount(modType, itemLevel, isMeta)
    local tier = GetItemProgressionTier(itemLevel)
    local budget = STAT_BUDGETS[tier]

    if isMeta then
        if modType == MOD_TYPE_STAMINA then
            return budget.meta_stamina
        elseif modType == MOD_TYPE_ATTACK_POWER then
            return budget.meta_ap
        elseif modType == MOD_TYPE_SPELL_POWER then
            return budget.meta_sp
        elseif modType == MOD_TYPE_CRIT_RATING then
            return budget.meta_rating
        else
            return budget.meta_primary
        end
    end

    if modType == MOD_TYPE_STAMINA then
        return budget.stamina
    elseif modType == MOD_TYPE_ATTACK_POWER then
        return budget.ap
    elseif modType == MOD_TYPE_SPELL_POWER then
        return budget.sp
    elseif modType == MOD_TYPE_STRENGTH or modType == MOD_TYPE_AGILITY or modType == MOD_TYPE_INTELLECT or modType == MOD_TYPE_SPIRIT then
        return budget.primary
    else
        return budget.rating
    end
end

----------------------------------------------------------------------------------------------------
-- Helper: Get Socket Color Letter
----------------------------------------------------------------------------------------------------
local function GetColorLetter(colorMask)
    if colorMask == SOCKET_COLOR_META then
        return "M"
    elseif colorMask == SOCKET_COLOR_RED then
        return "R"
    elseif colorMask == SOCKET_COLOR_YELLOW then
        return "Y"
    elseif colorMask == SOCKET_COLOR_BLUE then
        return "B"
    elseif bit.band(colorMask, SOCKET_COLOR_RED) ~= 0 and bit.band(colorMask, SOCKET_COLOR_YELLOW) ~= 0 then
        return "O" -- Orange
    elseif bit.band(colorMask, SOCKET_COLOR_RED) ~= 0 and bit.band(colorMask, SOCKET_COLOR_BLUE) ~= 0 then
        return "P" -- Purple
    elseif bit.band(colorMask, SOCKET_COLOR_YELLOW) ~= 0 and bit.band(colorMask, SOCKET_COLOR_BLUE) ~= 0 then
        return "G" -- Green
    else
        return "S" -- Prismatic / Special
    end
end

----------------------------------------------------------------------------------------------------
-- Helper: Query and Cache Item Template Sockets
----------------------------------------------------------------------------------------------------
local function GetItemTemplateSockets(itemEntry)
    if itemTemplateSocketCache[itemEntry] then
        return itemTemplateSocketCache[itemEntry]
    end

    local query = WorldDBQuery(string.format("SELECT socketColor_1, socketColor_2, socketColor_3, socketBonus FROM item_template WHERE entry = %u", itemEntry))
    if not query then
        itemTemplateSocketCache[itemEntry] = { count = 0, sockets = {}, signature = "" }
        return itemTemplateSocketCache[itemEntry]
    end

    local c1 = query:GetUInt32(0)
    local c2 = query:GetUInt32(1)
    local c3 = query:GetUInt32(2)

    local sockets = {}
    local sigParts = {}

    if c1 and c1 > 0 then
        table.insert(sockets, c1)
        table.insert(sigParts, GetColorLetter(c1))
    end
    if c2 and c2 > 0 then
        table.insert(sockets, c2)
        table.insert(sigParts, GetColorLetter(c2))
    end
    if c3 and c3 > 0 then
        table.insert(sockets, c3)
        table.insert(sigParts, GetColorLetter(c3))
    end

    local result = {
        count = #sockets,
        sockets = sockets,
        signature = table.concat(sigParts, "-")
    }

    itemTemplateSocketCache[itemEntry] = result
    return result
end

----------------------------------------------------------------------------------------------------
-- Helper: Select Random Mod Type from Socket Color
----------------------------------------------------------------------------------------------------
local function SelectModTypeForSocket(colorMask)
    if colorMask == SOCKET_COLOR_META then
        return POOL_META[math.random(#POOL_META)]
    elseif colorMask == SOCKET_COLOR_RED then
        return POOL_RED[math.random(#POOL_RED)]
    elseif colorMask == SOCKET_COLOR_YELLOW then
        return POOL_YELLOW[math.random(#POOL_YELLOW)]
    elseif colorMask == SOCKET_COLOR_BLUE then
        return POOL_BLUE[math.random(#POOL_BLUE)]
    elseif bit.band(colorMask, SOCKET_COLOR_RED) ~= 0 and bit.band(colorMask, SOCKET_COLOR_YELLOW) ~= 0 then
        -- Orange
        local pool = math.random(2) == 1 and POOL_RED or POOL_YELLOW
        return pool[math.random(#pool)]
    elseif bit.band(colorMask, SOCKET_COLOR_RED) ~= 0 and bit.band(colorMask, SOCKET_COLOR_BLUE) ~= 0 then
        -- Purple
        local pool = math.random(2) == 1 and POOL_RED or POOL_BLUE
        return pool[math.random(#pool)]
    elseif bit.band(colorMask, SOCKET_COLOR_YELLOW) ~= 0 and bit.band(colorMask, SOCKET_COLOR_BLUE) ~= 0 then
        -- Green
        local pool = math.random(2) == 1 and POOL_YELLOW or POOL_BLUE
        return pool[math.random(#pool)]
    else
        -- Prismatic / Any non-meta
        local r = math.random(3)
        if r == 1 then
            return POOL_RED[math.random(#POOL_RED)]
        elseif r == 2 then
            return POOL_YELLOW[math.random(#POOL_YELLOW)]
        else
            return POOL_BLUE[math.random(#POOL_BLUE)]
        end
    end
end

----------------------------------------------------------------------------------------------------
-- Helper: Load or Generate Affixes for an Item GUID
----------------------------------------------------------------------------------------------------
local function EnsureItemAffixes(item)
    if not item or not CONFIG.AffixesEnabled then
        return nil
    end

    local itemGuid = item:GetGUIDLow()
    if itemAffixCache[itemGuid] then
        return itemAffixCache[itemGuid]
    end

    local itemEntry = item:GetEntry()
    local templateData = GetItemTemplateSockets(itemEntry)
    if templateData.count == 0 then
        return nil
    end

    -- Check database for existing roll
    local affixQuery = CharDBQuery(string.format("SELECT socket_signature, roll_version FROM mod_no_gems_item_affix WHERE item_guid = %u", itemGuid))
    if affixQuery then
        local signature = affixQuery:GetString(0)
        local effects = {}

        local effectsQuery = CharDBQuery(string.format("SELECT socket_index, socket_type, mod_type, amount FROM mod_no_gems_item_affix_effect WHERE item_guid = %u ORDER BY socket_index ASC", itemGuid))
        if effectsQuery then
            repeat
                table.insert(effects, {
                    socketIndex = effectsQuery:GetUInt8(0),
                    socketType  = effectsQuery:GetUInt8(1),
                    modType     = effectsQuery:GetUInt32(2),
                    amount      = effectsQuery:GetInt32(3),
                })
            until not effectsQuery:NextRow()
        end

        local data = { signature = signature, effects = effects, itemEntry = itemEntry }
        itemAffixCache[itemGuid] = data
        return data
    end

    -- Generate new randomized roll (one roll per original socket)
    local itemLevel = item:GetItemLevel()
    local effects = {}

    for i = 1, templateData.count do
        local socketColor = templateData.sockets[i]
        local isMeta = (socketColor == SOCKET_COLOR_META)
        local modType = SelectModTypeForSocket(socketColor)
        local amount = GetStatAmount(modType, itemLevel, isMeta)

        table.insert(effects, {
            socketIndex = i,
            socketType  = socketColor,
            modType     = modType,
            amount      = amount,
        })
    end

    -- Persist immediately to characters database
    CharDBExecute(string.format(
        "INSERT IGNORE INTO mod_no_gems_item_affix (item_guid, item_entry, roll_version, socket_signature) VALUES (%u, %u, %u, '%s')",
        itemGuid, itemEntry, CONFIG.RollVersion, templateData.signature
    ))

    for _, eff in ipairs(effects) do
        CharDBExecute(string.format(
            "INSERT IGNORE INTO mod_no_gems_item_affix_effect (item_guid, socket_index, socket_type, mod_type, amount) VALUES (%u, %u, %u, %u, %d)",
            itemGuid, eff.socketIndex, eff.socketType, eff.modType, eff.amount
        ))
    end

    local data = { signature = templateData.signature, effects = effects, itemEntry = itemEntry }
    itemAffixCache[itemGuid] = data
    return data
end

----------------------------------------------------------------------------------------------------
-- Helper: Apply or Remove Affix Stats on Player
----------------------------------------------------------------------------------------------------
local function ApplyAffixEffect(player, modType, amount, apply)
    if not player or amount == 0 then
        return
    end

    if modType == MOD_TYPE_STRENGTH then
        player:HandleStatFlatModifier(UNIT_MOD_STRENGTH, 1, amount, apply)
    elseif modType == MOD_TYPE_AGILITY then
        player:HandleStatFlatModifier(UNIT_MOD_AGILITY, 1, amount, apply)
    elseif modType == MOD_TYPE_STAMINA then
        player:HandleStatFlatModifier(UNIT_MOD_STAMINA, 1, amount, apply)
    elseif modType == MOD_TYPE_INTELLECT then
        player:HandleStatFlatModifier(UNIT_MOD_INTELLECT, 1, amount, apply)
    elseif modType == MOD_TYPE_SPIRIT then
        player:HandleStatFlatModifier(UNIT_MOD_SPIRIT, 1, amount, apply)
    elseif modType == MOD_TYPE_ATTACK_POWER then
        player:HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER, 1, amount, apply)
        player:HandleStatFlatModifier(UNIT_MOD_AP_RANGED, 1, amount, apply)
    elseif modType == MOD_TYPE_SPELL_POWER then
        player:SetSpellPower(amount, apply)
    elseif modType == MOD_TYPE_CRIT_RATING then
        player:ApplyRatingMod(CR_CRIT_MELEE, amount, apply)
        player:ApplyRatingMod(CR_CRIT_RANGED, amount, apply)
        player:ApplyRatingMod(CR_CRIT_SPELL, amount, apply)
    elseif modType == MOD_TYPE_HASTE_RATING then
        player:ApplyRatingMod(CR_HASTE_MELEE, amount, apply)
        player:ApplyRatingMod(CR_HASTE_RANGED, amount, apply)
        player:ApplyRatingMod(CR_HASTE_SPELL, amount, apply)
    elseif modType == MOD_TYPE_HIT_RATING then
        player:ApplyRatingMod(CR_HIT_MELEE, amount, apply)
        player:ApplyRatingMod(CR_HIT_RANGED, amount, apply)
        player:ApplyRatingMod(CR_HIT_SPELL, amount, apply)
    elseif modType == MOD_TYPE_DEFENSE then
        player:ApplyRatingMod(CR_DEFENSE_SKILL, amount, apply)
    elseif modType == MOD_TYPE_RESILIENCE then
        player:ApplyRatingMod(CR_CRIT_TAKEN_MELEE, amount, apply)
        player:ApplyRatingMod(CR_CRIT_TAKEN_RANGED, amount, apply)
        player:ApplyRatingMod(CR_CRIT_TAKEN_SPELL, amount, apply)
    elseif modType == MOD_TYPE_ARMOR_PEN then
        player:ApplyRatingMod(CR_ARMOR_PENETRATION, amount, apply)
    elseif modType == MOD_TYPE_EXPERTISE then
        player:ApplyRatingMod(CR_EXPERTISE, amount, apply)
    elseif modType == MOD_TYPE_DODGE then
        player:ApplyRatingMod(CR_DODGE, amount, apply)
    elseif modType == MOD_TYPE_PARRY then
        player:ApplyRatingMod(CR_PARRY, amount, apply)
    end
end

local function ApplyItemAffixStats(player, itemGuid, affixData, apply)
    if not player or not affixData or not affixData.effects then
        return
    end

    for _, eff in ipairs(affixData.effects) do
        ApplyAffixEffect(player, eff.modType, eff.amount, apply)
    end
end

----------------------------------------------------------------------------------------------------
-- Helper: Serialize Affix for Client Tooltip Addon
----------------------------------------------------------------------------------------------------
local function SerializeAffixPayload(itemGuid, itemEntry, affixData)
    if not affixData or not affixData.effects then
        return nil
    end

    local lines = {}
    for _, eff in ipairs(affixData.effects) do
        local statName = STAT_NAMES[eff.modType] or "Stat"
        table.insert(lines, string.format("+%d %s", eff.amount, statName))
    end

    -- Format: AFFIX:<itemGuid>:<itemEntry>:<line1>|<line2>|...
    return string.format("AFFIX:%u:%u:%s", itemGuid, itemEntry, table.concat(lines, "|"))
end

local function SendItemAffixToAddon(player, itemGuid, itemEntry, affixData)
    if not player or not affixData then
        return
    end

    local payload = SerializeAffixPayload(itemGuid, itemEntry, affixData)
    if payload then
        player:SendAddonMessage(CONFIG.AddonPrefix, payload, 0, player)
    end
end

----------------------------------------------------------------------------------------------------
-- Equipment Reconciliation: Scans Equipped Gear and Syncs Affix Stats
----------------------------------------------------------------------------------------------------
local function ReconcilePlayerEquipment(player)
    if not player or not CONFIG.Enable or not CONFIG.AffixesEnabled then
        return
    end

    local playerGuid = player:GetGUIDLow()
    if not playerEquippedAffixes[playerGuid] then
        playerEquippedAffixes[playerGuid] = {}
    end

    local currentEquipped = {}

    -- Equipment slots 0 to 18 (INVENTORY_SLOT_BAG_0 = 255)
    for slot = 0, 18 do
        local item = player:GetItemByPos(255, slot)
        if item then
            local itemGuid = item:GetGUIDLow()
            local itemEntry = item:GetEntry()
            local templateData = GetItemTemplateSockets(itemEntry)

            if templateData.count > 0 then
                currentEquipped[itemGuid] = item
            end
        end
    end

    -- Remove affixes from items that were unequipped or swapped out
    for itemGuid, oldData in pairs(playerEquippedAffixes[playerGuid]) do
        if not currentEquipped[itemGuid] then
            ApplyItemAffixStats(player, itemGuid, oldData, false)
            playerEquippedAffixes[playerGuid][itemGuid] = nil
        end
    end

    -- Apply affixes to items that are currently equipped
    for itemGuid, item in pairs(currentEquipped) do
        if not playerEquippedAffixes[playerGuid][itemGuid] then
            local affixData = EnsureItemAffixes(item)
            if affixData then
                ApplyItemAffixStats(player, itemGuid, affixData, true)
                playerEquippedAffixes[playerGuid][itemGuid] = affixData
                SendItemAffixToAddon(player, itemGuid, item:GetEntry(), affixData)
            end
        end
    end
end

----------------------------------------------------------------------------------------------------
-- Packet Interception: CMSG_SOCKET_GEMS (Opcode 0x347 = 839)
----------------------------------------------------------------------------------------------------
local function OnSocketGemsPacket(event, packet, player)
    if not CONFIG.Enable or not CONFIG.BlockSocketing then
        return true
    end

    if player then
        player:SendBroadcastMessage("|cFFFF0000[No Gems]|r Socketing gems is disabled on this realm.")
    end

    -- Returning false suppresses the packet so socketing never occurs
    return false
end

----------------------------------------------------------------------------------------------------
-- Player Lifecycle Hooks
----------------------------------------------------------------------------------------------------
local function OnPlayerLogin(event, player)
    ReconcilePlayerEquipment(player)
end

local function OnPlayerLogout(event, player)
    local playerGuid = player:GetGUIDLow()
    playerEquippedAffixes[playerGuid] = nil
end

local function OnPlayerEquip(event, player, item, bag, slot)
    -- Small deferral or direct reconciliation ensures item is fully in equipment slot
    ReconcilePlayerEquipment(player)
end

local function OnItemAcquisition(event, player, item, count)
    if not item or not CONFIG.AffixesEnabled then
        return
    end

    local itemEntry = item:GetEntry()
    local templateData = GetItemTemplateSockets(itemEntry)
    if templateData.count > 0 then
        -- Ensure affix is rolled and persisted immediately upon acquiring the item
        local affixData = EnsureItemAffixes(item)
        if affixData and player then
            SendItemAffixToAddon(player, item:GetGUIDLow(), itemEntry, affixData)
        end
    end
end

----------------------------------------------------------------------------------------------------
-- Addon Message Handler: Handles Requests from Client Tooltip Addon
----------------------------------------------------------------------------------------------------
local function OnPlayerChat(event, player, msg, msgType, lang)
    -- CHAT_MSG_ADDON = 15
    if msgType == 15 and msg and player then
        -- Client request format: REQ:<itemGuid>
        local prefix, reqGuidStr = msg:match("^(%w+):(%d+)$")
        if prefix == "REQ" and reqGuidStr then
            local reqGuid = tonumber(reqGuidStr)
            if reqGuid and itemAffixCache[reqGuid] then
                local data = itemAffixCache[reqGuid]
                SendItemAffixToAddon(player, reqGuid, data.itemEntry or 0, data)
            else
                -- Try loading from DB
                local q = CharDBQuery(string.format("SELECT item_entry, socket_signature FROM mod_no_gems_item_affix WHERE item_guid = %u", reqGuid))
                if q then
                    local entry = q:GetUInt32(0)
                    local sig = q:GetString(1)
                    local effects = {}
                    local eq = CharDBQuery(string.format("SELECT socket_index, socket_type, mod_type, amount FROM mod_no_gems_item_affix_effect WHERE item_guid = %u ORDER BY socket_index ASC", reqGuid))
                    if eq then
                        repeat
                            table.insert(effects, {
                                socketIndex = eq:GetUInt8(0),
                                socketType  = eq:GetUInt8(1),
                                modType     = eq:GetUInt32(2),
                                amount      = eq:GetInt32(3),
                            })
                        until not eq:NextRow()
                    end
                    local data = { signature = sig, effects = effects, itemEntry = entry }
                    itemAffixCache[reqGuid] = data
                    SendItemAffixToAddon(player, reqGuid, entry, data)
                end
            end
        end
    end
end

----------------------------------------------------------------------------------------------------
-- Chat Inspection Command: .nogems item
----------------------------------------------------------------------------------------------------
local function OnPlayerCommand(event, player, command, chatHandler)
    if not player or not command then
        return
    end

    local cmd = command:lower()
    if cmd == CONFIG.ChatCommand or cmd:find("^" .. CONFIG.ChatCommand .. "%s") then
        local subCmd = cmd:match("^" .. CONFIG.ChatCommand .. "%s*(.*)$")
        subCmd = subCmd and subCmd:gsub("^%s*(.-)%s*$", "%1") or ""

        if subCmd == "" or subCmd == "item" then
            player:SendBroadcastMessage("|cFF00FF00=== [No Gems] Equipped Socket Affixes ===|r")
            local foundAny = false

            for slot = 0, 18 do
                local item = player:GetItemByPos(255, slot)
                if item then
                    local itemGuid = item:GetGUIDLow()
                    local itemEntry = item:GetEntry()
                    local templateData = GetItemTemplateSockets(itemEntry)

                    if templateData.count > 0 then
                        foundAny = true
                        local affixData = EnsureItemAffixes(item)
                        player:SendBroadcastMessage(string.format("|cFFFFD700[%s]|r (Sockets: %s)", item:GetItemLink(), templateData.signature))

                        if affixData and affixData.effects then
                            for _, eff in ipairs(affixData.effects) do
                                local statName = STAT_NAMES[eff.modType] or "Unknown Stat"
                                player:SendBroadcastMessage(string.format("  |cFF00FF00Socket %d [%s]:|r +%d %s", eff.socketIndex, GetColorLetter(eff.socketType), eff.amount, statName))
                            end
                        else
                            player:SendBroadcastMessage("  |cFF888888No replacement affixes generated.|r")
                        end
                    end
                end
            end

            if not foundAny then
                player:SendBroadcastMessage("|cFF888888No socketed items currently equipped.|r")
            end
            return false
        else
            player:SendBroadcastMessage("|cFF00FF00[No Gems]|r Usage: .nogems item")
            return false
        end
    end
end

----------------------------------------------------------------------------------------------------
-- Registration & Initialization
----------------------------------------------------------------------------------------------------
InitDatabase()

-- Register Packet Interception: CMSG_SOCKET_GEMS (0x347 = 839)
-- PACKET_EVENT_ON_PACKET_RECEIVE = 5
RegisterPacketEvent(0x347, 5, OnSocketGemsPacket)

-- Register Player Lifecycle Events
RegisterPlayerEvent(3,  OnPlayerLogin)              -- PLAYER_EVENT_ON_LOGIN
RegisterPlayerEvent(4,  OnPlayerLogout)             -- PLAYER_EVENT_ON_LOGOUT
RegisterPlayerEvent(29, OnPlayerEquip)              -- PLAYER_EVENT_ON_EQUIP
RegisterPlayerEvent(18, OnPlayerChat)               -- PLAYER_EVENT_ON_CHAT
RegisterPlayerEvent(42, OnPlayerCommand)            -- PLAYER_EVENT_ON_COMMAND
RegisterPlayerEvent(32, OnItemAcquisition)          -- PLAYER_EVENT_ON_LOOT_ITEM
RegisterPlayerEvent(51, OnItemAcquisition)          -- PLAYER_EVENT_ON_QUEST_REWARD_ITEM
RegisterPlayerEvent(52, OnItemAcquisition)          -- PLAYER_EVENT_ON_CREATE_ITEM
RegisterPlayerEvent(53, OnItemAcquisition)          -- PLAYER_EVENT_ON_STORE_NEW_ITEM

print(string.format("[%s] Loaded successfully (Version %s). Socketing disabled, persistent affixes active.", MODULE_NAME, MODULE_VERSION))
