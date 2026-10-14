-- Zone Levels, the client half of mod-world-journey, carried by mod-coa-content-scaling for its journey layout.
-- The server tells this addon at login what the journey looks like on this realm:
--   B                     a new set begins
--   Z<key>=<lo>-<hi>;...  the levels of zones, as the world map knows them (WORLD_MAP_LEVELS)
--   E<before>~<after>     a line of a gem or an enchantment: what it said, what it gives now
--   L<id>=<min>-<max>-<recommended>;...   the levels of the Dungeon Finder's dungeons (LFGDungeons.dbc of the
--                         client still holds the original ones, and the Dungeon Finder hides what it thinks is too high)
--   D                     the set is complete
-- The last complete set is kept per realm, so the map is right even before the server has spoken, and a realm
-- that does not play the journey never shows another realm's levels.

local PREFIX = "ZoneLevels"
local pending
local frame = CreateFrame("Frame")

-- This realm's set, or nil.
local function Current()
    local db = ZoneLevelsDB
    return type(db) == "table" and type(db.realms) == "table" and db.realms[GetRealmName()] or nil
end

-- The client opens the Dungeon Finder at SHOW_LFD_LEVEL (15); on the journey it opens with the lowest dungeon.
local function UnlockDungeonFinder(db)
    local lowest
    for _, levels in pairs(db.dungeons or {}) do
        if not lowest or levels[1] < lowest then lowest = levels[1] end
    end
    if not lowest or type(SHOW_LFD_LEVEL) ~= "number" or lowest >= SHOW_LFD_LEVEL then return end
    SHOW_LFD_LEVEL = lowest
    if LFDMicroButton and type(SHOW_PVP_LEVEL) == "number" then
        LFDMicroButton.minLevel = math.min(SHOW_PVP_LEVEL, lowest)
    end
    if type(UpdateMicroButtons) == "function" then pcall(UpdateMicroButtons) end
end

local function Apply(db)
    if not db then return end
    if type(WORLD_MAP_LEVELS) == "table" then
        for key, levels in pairs(db.zones or {}) do
            WORLD_MAP_LEVELS[key] = { levels[1], levels[2] }
        end
    end
    UnlockDungeonFinder(db)
end

-- The Dungeon Finder: every place that reads the levels of a dungeon gets the ones of the journey.
-- Fields of GetLFGDungeonInfo: 3 min level, 4 max level, 5 recommended, 6 lowest recommended, 7 highest recommended.
local function DungeonLevels(id)
    local db = Current()
    local all = db and db.dungeons
    return all and all[id]
end

local function PatchDungeon(id, info)
    local l = DungeonLevels(id)
    if not l or type(info) ~= "table" then return end
    info[3], info[4], info[5] = l[1], l[2], l[3]
    info[6], info[7] = l[1], math.min(l[2], l[3] + 3)
end

local function pack(...) return { n = select("#", ...), ... } end

if type(GetLFGDungeonInfo) == "function" then
    local original = GetLFGDungeonInfo
    GetLFGDungeonInfo = function(id, ...)
        if not DungeonLevels(id) then return original(id, ...) end
        local info = pack(original(id, ...))
        PatchDungeon(id, info)
        return unpack(info, 1, info.n)
    end
end

if type(GetLFDChoiceInfo) == "function" then
    local original = GetLFDChoiceInfo
    GetLFDChoiceInfo = function(...)
        local all = original(...)
        if type(all) == "table" then
            for id, info in pairs(all) do PatchDungeon(id, info) end
        end
        return all
    end
end

-- Data that came after the Dungeon Finder read its list: patch what it holds and let it list again.
local function RefreshDungeonFinder()
    if type(LFGDungeonInfo) == "table" then
        for id, info in pairs(LFGDungeonInfo) do PatchDungeon(id, info) end
    end
    if type(LFDQueueFrame_Update) == "function" then pcall(LFDQueueFrame_Update) end
end

local function Retext(tooltip)
    local db = Current()
    local texts = db and db.texts
    if not texts or not next(texts) then return end
    local name = tooltip:GetName()
    for i = 2, tooltip:NumLines() do
        local line = _G[name .. "TextLeft" .. i]
        local text = line and line:GetText()
        if text then
            local now = texts[text]
            if not now then
                local head, rest = text:match("^(.-:%s*)(.+)$")      -- "Socket Bonus: ..." and the like
                if head and texts[rest] then now = head .. texts[rest] end
            end
            if now then line:SetText(now) end
        end
    end
end

for _, tooltip in ipairs({ GameTooltip, ItemRefTooltip, ShoppingTooltip1, ShoppingTooltip2, ShoppingTooltip3 }) do
    if tooltip and tooltip.HookScript then
        tooltip:HookScript("OnTooltipSetItem", Retext)
    end
end

frame:RegisterEvent("CHAT_MSG_ADDON")
frame:RegisterEvent("ADDON_LOADED")
frame:RegisterEvent("PLAYER_ENTERING_WORLD")
frame:SetScript("OnEvent", function(self, event, prefix, message, channel, sender)
    if event == "ADDON_LOADED" then
        if prefix == PREFIX then Apply(Current()) end
        return
    end
    if event == "PLAYER_ENTERING_WORLD" then
        -- Ask once per session: what the server sent at login may have arrived before this addon was loaded.
        self:UnregisterEvent("PLAYER_ENTERING_WORLD")
        SendAddonMessage(PREFIX, "?", "WHISPER", UnitName("player"))
        return
    end
    if prefix ~= PREFIX or type(message) ~= "string" then return end
    local kind, body = message:sub(1, 1), message:sub(2)
    if kind == "B" then
        pending = { zones = {}, texts = {}, dungeons = {} }
    elseif not pending then
        return
    elseif kind == "Z" then
        for key, lo, hi in body:gmatch("([%w_]+)=(%d+)%-(%d+);") do
            pending.zones[key] = { tonumber(lo), tonumber(hi) }
        end
    elseif kind == "L" then
        for id, lo, hi, rec in body:gmatch("(%d+)=(%d+)%-(%d+)%-(%d+);") do
            pending.dungeons[tonumber(id)] = { tonumber(lo), tonumber(hi), tonumber(rec) }
        end
    elseif kind == "E" then
        local before, after = body:match("^(.-)~(.*)$")
        if before and before ~= "" then pending.texts[before] = after end
    elseif kind == "D" then
        if type(ZoneLevelsDB) ~= "table" or type(ZoneLevelsDB.realms) ~= "table" then
            ZoneLevelsDB = { realms = {} }
        end
        ZoneLevelsDB.realms[GetRealmName()] = pending
        pending = nil
        Apply(Current())
        RefreshDungeonFinder()
    end
end)
