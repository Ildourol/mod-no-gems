--[[
    No Gems Tooltip Addon (WoW 3.3.5a)
    Listens for MOD_NOGEMS addon messages from the server and renders
    the randomized socket replacement affixes cleanly onto the item tooltips.
]]

local ADDON_PREFIX = "MOD_NOGEMS"
NoGemsTooltipCache = NoGemsTooltipCache or {} -- [itemGuid] = { "+20 Strength", "+30 Stamina", ... }

-- Frame for event listening
local frame = CreateFrame("Frame")
frame:RegisterEvent("CHAT_MSG_ADDON")

local function ParseItemGuidFromLink(link)
    if not link then return nil end
    -- Standard 3.3.5a item link: |cff...|Hitem:itemId:enchantId:gem1:gem2:gem3:gem4:suffixId:uniqueId:linkLevel:reforgeId|h[Name]|h|r
    -- In 3.3.5a, uniqueId is field 8 (1-indexed from item:...)
    local itemString = link:match("item:([%-?%d:]+)")
    if not itemString then return nil end
    local parts = { strsplit(":", itemString) }
    local uniqueId = tonumber(parts[8])
    return uniqueId
end

-- Process incoming server addon messages
frame:SetScript("OnEvent", function(self, event, ...)
    if event == "CHAT_MSG_ADDON" then
        local prefix, message, channel, sender = ...
        if prefix == ADDON_PREFIX and message then
            -- Payload format: AFFIX:<itemGuid>:<itemEntry>:<line1>|<line2>|...
            local tag, guidStr, entryStr, linesStr = message:match("^(%w+):(%d+):(%d+):(.*)$")
            if tag == "AFFIX" and guidStr then
                local guid = tonumber(guidStr)
                if guid then
                    local lines = {}
                    for line in string.gmatch(linesStr, "([^|]+)") do
                        table.insert(lines, line)
                    end
                    NoGemsTooltipCache[guid] = lines

                    -- If GameTooltip is currently showing this item, refresh it
                    if GameTooltip:IsShown() then
                        local _, curLink = GameTooltip:GetItem()
                        if curLink and ParseItemGuidFromLink(curLink) == guid then
                            GameTooltip:Show()
                        end
                    end
                end
            end
        end
    end
end)

-- Hook GameTooltip to append affix lines
local function AttachAffixesToTooltip(tooltip)
    local name, link = tooltip:GetItem()
    if not link then return end

    local guid = ParseItemGuidFromLink(link)
    if not guid or guid == 0 then return end

    local affixes = NoGemsTooltipCache[guid]
    if affixes and #affixes > 0 then
        tooltip:AddLine(" ")
        tooltip:AddLine("Socket Replacements:", 1.0, 0.82, 0.0) -- Gold header
        for _, text in ipairs(affixes) do
            tooltip:AddLine("  " .. text, 0.1, 1.0, 0.1) -- Green text
        end
        tooltip:Show()
    else
        -- Request affix data from server if not cached yet
        SendAddonMessage(ADDON_PREFIX, "REQ:" .. guid, "WHISPER", UnitName("player"))
    end
end

GameTooltip:HookScript("OnTooltipSetItem", AttachAffixesToTooltip)
ItemRefTooltip:HookScript("OnTooltipSetItem", AttachAffixesToTooltip)
