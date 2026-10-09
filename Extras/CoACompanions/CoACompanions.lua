--[[
    CoA Companions

    Every summoned Necromancer minion casts an "occupancy" aura on itself with
    SPELL_EFFECT_APPLY_AREA_AURA_OWNER, so the aura radiates onto its owner. The spell also
    carries SPELL_ATTR3_STACK_FOR_DIFF_CASTERS, which is why the player ends up with one
    separate aura instance per living minion rather than one stacked buff.

    That gives this addon everything it needs without a server change:

      * the list of live companions, by counting the player's occupancy auras,
      * a release channel, because the server intercepts CMSG_CANCEL_AURA and despawns the
        minion whose occupancy aura was cancelled (necromancer_minion_dismiss),
      * the Life Force budget, from aura 805011 (capacity) and 525004 (free slots).

    What it cannot do: show health for a companion the client has no unit token for. In 3.3.5a
    only pet, target, focus, mouseover, targettarget and pettarget exist, so health is read when
    a companion happens to occupy one of those and is remembered (dimmed) afterwards. Left
    clicking a row targets that companion, which is also the way to refresh its health.

    Rows are secure action buttons. Their attributes and positions are fixed at load, because
    neither may be changed while in combat, and companions are summoned mid-fight. Unused rows
    are therefore made invisible rather than hidden, and the list stays sparse instead of
    reflowing.
]]

local ADDON_NAME = ...

local COMPANIONS = {
    { spell = 805017, entry = 50068,  aura = "Abomination",              unit = "Abomination" },
    { spell = 805022, entry = 50115,  aura = "Decaying Colossus",        unit = "Decaying Colossus" },
    { spell = 807927, entry = 51065,  aura = "Greater Skeletal Warrior", unit = "Greater Skeletal Warrior" },
    { spell = 805016, entry = 50065,  aura = "Lesser Skeletal Warrior",  unit = "Lesser Skeletal Warrior" },
    { spell = 805019, entry = 50073,  aura = "Ghoul",                    unit = "Ghoul" },
    { spell = 805020, entry = 50075,  aura = "Command: Skeletal Mage",   unit = "Skeletal Mage" },
    { spell = 805021, entry = 50078,  aura = "Skeletal Rogue",           unit = "Skeletal Rogue" },
    { spell = 800034, entry = 50323,  aura = "Crypt Fiend",              unit = "Crypt Fiend" },
    { spell = 805028, entry = 50067,  aura = "Gargoyle",                 unit = "Raised Gargoyle" },
    { spell = 807840, entry = 500650, aura = "Banshee",                  unit = "Banshee" },
}

-- mod-coa-companions whispers "CoACompanions\t<entry>:<current>:<max>;..." to the player in
-- LANG_ADDON. It is optional: without it the addon falls back to whichever unit token happens to
-- hold a companion, which in practice means only while it is targeted or fought.
local ADDON_PREFIX = "CoACompanions"
local FEED_STALE_AFTER = 5

local CAPACITY_AURA = 805011
local FREE_AURA = 525004
local HEALTH_TOKENS = { "pet", "target", "focus", "mouseover", "targettarget", "pettarget" }

local ROW_HEIGHT = 26
local ROW_WIDTH = 260
local UPDATE_INTERVAL = 0.15

local bySpell = {}
local byEntry = {}
for index, entry in ipairs(COMPANIONS) do
    entry.index = index
    bySpell[entry.spell] = entry
    byEntry[entry.entry] = entry
end

-- Server feed, keyed by creature entry: { count, current, maximum, lowest } and when it arrived.
local feed = {}
local feedTime = 0

local rows = {}
local lastHealth = {}
local elapsedSinceUpdate = 0

local defaults = {
    point = "CENTER",
    relativePoint = "CENTER",
    x = -260,
    y = 0,
    scale = 1.0,
    locked = false,
}

local function Option(key)
    if CoACompanionsDB and CoACompanionsDB[key] ~= nil then
        return CoACompanionsDB[key]
    end
    return defaults[key]
end

local function Say(message)
    DEFAULT_CHAT_FRAME:AddMessage("|cff8fce00CoA Companions|r " .. message)
end

-- ---------------------------------------------------------------------------------------------
-- Reading the player's auras
-- ---------------------------------------------------------------------------------------------

local function ScanAuras()
    local active = {}
    local capacity, free = 0, 0

    for i = 1, 40 do
        local name, _, icon, count, _, _, _, caster, _, _, spellId = UnitBuff("player", i)
        if not name then
            break
        end

        if spellId == CAPACITY_AURA then
            capacity = count or 1
        elseif spellId == FREE_AURA then
            free = count or 0
        elseif bySpell[spellId] then
            local live = active[spellId]
            if live then
                live.count = live.count + 1
                live.caster = live.caster or caster
            else
                active[spellId] = { count = 1, icon = icon, name = name, caster = caster }
            end
        end
    end

    return active, capacity, free
end

-- A companion is only readable while it occupies one of the client's unit tokens. Name matching
-- can in principle pick up another player's minion of the same name; targeting the row first is
-- the reliable reading.
-- The container is protected through its secure children, so every call that would resize, move,
-- rescale or hide it has to wait for combat to end. The wanted state is remembered and applied on
-- PLAYER_REGEN_ENABLED. Declared here so the deferring helpers below close over the real frame.
local container
local wantedHeight = 0

local wantedSlot = {}

local function ApplyHeight()
    if wantedHeight <= 0 or InCombatLockdown() then
        return
    end
    if math.abs(container:GetHeight() - wantedHeight) > 0.5 then
        container:SetHeight(wantedHeight)
    end
end

-- Live companions are packed to the top so the list has no holes. Rows are secure and may not be
-- moved in combat, so a companion summoned or lost mid-fight can leave a gap until it ends; the
-- inactive rows are parked in order behind the live ones, which is where a new one belongs anyway.
local function ApplyLayout()
    if InCombatLockdown() then
        return
    end
    for spell, slot in pairs(wantedSlot) do
        local row = rows[spell]
        if row and row.slot ~= slot then
            row:ClearAllPoints()
            row:SetPoint("TOPLEFT", container, "TOPLEFT", 6, -(24 + (slot - 1) * ROW_HEIGHT))
            row.slot = slot
        end
    end
end

local function ParseFeed(body)
    local parsed = {}

    for chunk in string.gmatch(body or "", "[^;]+") do
        local entry, current, maximum = string.match(chunk, "^(%d+):(%d+):(%d+)$")
        if entry then
            entry = tonumber(entry)
            current = tonumber(current)
            maximum = tonumber(maximum)
            local row = parsed[entry]
            if row then
                row.current = row.current + current
                row.maximum = row.maximum + maximum
            else
                parsed[entry] = { current = current, maximum = maximum }
            end
        end
    end

    return parsed
end

local function FeedIsFresh()
    return feedTime > 0 and (GetTime() - feedTime) < FEED_STALE_AFTER
end

local function ReadHealth(token, unitName)
    if not token or not UnitExists(token) or UnitIsPlayer(token) then
        return nil
    end
    if unitName and UnitName(token) ~= unitName then
        return nil
    end
    local maximum = UnitHealthMax(token)
    if maximum and maximum > 0 then
        return UnitHealth(token), maximum
    end
end

local function HealthFor(unitName, caster)
    -- UnitBuff hands back the aura's caster as a token whenever the client has one for it, which
    -- is the only reading that cannot pick up somebody else's minion of the same name.
    local health, maximum = ReadHealth(caster, nil)
    if health then
        return health, maximum
    end

    for _, token in ipairs(HEALTH_TOKENS) do
        health, maximum = ReadHealth(token, unitName)
        if health then
            return health, maximum
        end
    end
end

-- ---------------------------------------------------------------------------------------------
-- Frames
-- ---------------------------------------------------------------------------------------------

container = CreateFrame("Frame", "CoACompanionsFrame", UIParent)
container:SetWidth(ROW_WIDTH + 12)
container:SetHeight(ROW_HEIGHT * #COMPANIONS + 30)
container:SetMovable(true)
container:EnableMouse(true)
container:RegisterForDrag("LeftButton")
container:SetClampedToScreen(true)
container:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 16, edgeSize = 12,
    insets = { left = 3, right = 3, top = 3, bottom = 3 },
})
container:SetBackdropColor(0, 0, 0, 0.55)

container:SetScript("OnDragStart", function(self)
    if not Option("locked") and not InCombatLockdown() then
        self:StartMoving()
        self.moving = true
    end
end)

container:SetScript("OnDragStop", function(self)
    if not self.moving then
        return
    end
    self.moving = nil
    self:StopMovingOrSizing()
    local point, _, relativePoint, x, y = self:GetPoint()
    CoACompanionsDB.point = point
    CoACompanionsDB.relativePoint = relativePoint
    CoACompanionsDB.x = x
    CoACompanionsDB.y = y
end)

local header = container:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
header:SetPoint("TOPLEFT", container, "TOPLEFT", 8, -8)
header:SetPoint("TOPRIGHT", container, "TOPRIGHT", -8, -8)
header:SetJustifyH("LEFT")
header:SetText("Companions")

local function CreateRow(entry)
    local row = CreateFrame("Button", "CoACompanionsRow" .. entry.index, container,
        "SecureActionButtonTemplate")
    row:SetWidth(ROW_WIDTH)
    row:SetHeight(ROW_HEIGHT - 2)
    row:SetPoint("TOPLEFT", container, "TOPLEFT", 6, -(24 + (entry.index - 1) * ROW_HEIGHT))
    row.slot = entry.index
    row:RegisterForClicks("AnyUp")

    row.icon = row:CreateTexture(nil, "ARTWORK")
    row.icon:SetWidth(ROW_HEIGHT - 6)
    row.icon:SetHeight(ROW_HEIGHT - 6)
    row.icon:SetPoint("LEFT", row, "LEFT", 0, 0)
    row.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)

    row.health = CreateFrame("StatusBar", nil, row)
    row.health:SetPoint("LEFT", row.icon, "RIGHT", 4, 0)
    row.health:SetPoint("RIGHT", row, "RIGHT", 0, 0)
    row.health:SetHeight(ROW_HEIGHT - 6)
    row.health:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
    row.health:SetMinMaxValues(0, 1)
    row.health:SetValue(1)

    row.healthBackground = row.health:CreateTexture(nil, "BACKGROUND")
    row.healthBackground:SetAllPoints(row.health)
    row.healthBackground:SetTexture(0, 0, 0, 0.6)

    row.value = row.health:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    row.value:SetPoint("RIGHT", row.health, "RIGHT", -4, 0)
    row.value:SetJustifyH("RIGHT")

    -- The name ends where the health value begins; a name too long for the space left is cut
    -- short with an ellipsis instead of running under the value.
    row.label = row.health:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    row.label:SetPoint("LEFT", row.health, "LEFT", 4, 0)
    row.label:SetPoint("RIGHT", row.value, "LEFT", -6, 0)
    row.label:SetHeight(12)
    row.label:SetJustifyH("LEFT")

    local targetSlash = SLASH_TARGET_EXACT1 or SLASH_TARGET1 or "/target"
    local cancelSlash = SLASH_CANCELAURA1 or "/cancelaura"

    row:SetAttribute("type1", "macro")
    row:SetAttribute("macrotext1", targetSlash .. " " .. entry.unit)
    row:SetAttribute("type2", "macro")
    row:SetAttribute("macrotext2", cancelSlash .. " " .. entry.aura)

    row:SetScript("OnEnter", function(self)
        if not self.active then
            return
        end
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:AddLine(entry.aura, 1, 0.82, 0)
        GameTooltip:AddLine("Left click to target, right click to release.", 1, 1, 1)
        GameTooltip:AddLine("A * behind the health means the client currently has no unit for "
            .. "this companion and the bar is the last reading. Targeting it refreshes it.",
            0.72, 0.72, 0.72, true)
        GameTooltip:Show()
    end)
    row:SetScript("OnLeave", function() GameTooltip:Hide() end)

    return row
end

for _, entry in ipairs(COMPANIONS) do
    rows[entry.spell] = CreateRow(entry)
end

-- ---------------------------------------------------------------------------------------------
-- Refresh
-- ---------------------------------------------------------------------------------------------

local function Refresh()
    local active, capacity, free = ScanAuras()
    local used = capacity - free

    if capacity > 0 then
        header:SetText(string.format("Life Force  |cffffffff%d|r / %d", used, capacity))
    else
        header:SetText("Companions")
    end

    local liveCount = 0
    local parked = 0

    for _, entry in ipairs(COMPANIONS) do
        local row = rows[entry.spell]
        local live = active[entry.spell]

        if not live then
            row.active = false
            row:SetAlpha(0)
            lastHealth[entry.spell] = nil
        else
            liveCount = liveCount + 1
            wantedSlot[entry.spell] = liveCount
            row.active = true
            row:SetAlpha(1)
            row.icon:SetTexture(live.icon)

            -- The count stands with the health value, so a long name cut short never takes it along.
            row.label:SetText(live.name)
            local count = live.count > 1 and string.format("|cffffd100x%d|r  ", live.count) or ""

            -- A companion that has just been summoned is at full health, so start there rather
            -- than with an empty bar the client cannot fill until the unit is targeted.
            if lastHealth[entry.spell] == nil then
                lastHealth[entry.spell] = 1
            end

            -- The server feed knows every companion; the unit tokens only know the one the
            -- client happens to hold, so the feed wins whenever it is current.
            local health, maximum
            local pushed = FeedIsFresh() and feed[entry.entry] or nil
            if pushed and pushed.maximum > 0 then
                health, maximum = pushed.current, pushed.maximum
            else
                health, maximum = HealthFor(entry.unit, live.caster)
            end

            if health then
                local fraction = health / maximum
                lastHealth[entry.spell] = fraction
                row.health:SetValue(fraction)
                row.health:SetStatusBarColor(0.15, 0.70, 0.15)
                row.value:SetText(count .. string.format("%d%%", math.floor(fraction * 100 + 0.5)))
                row.value:SetTextColor(1, 1, 1)
            else
                -- Dimmed green, not grey: the reading is stale rather than the companion being
                -- gone or out of range. The client has no unit token for it right now, and
                -- targeting it - a left click on this row - is what refreshes the bar.
                local remembered = lastHealth[entry.spell]
                row.health:SetValue(remembered)
                row.health:SetStatusBarColor(0.16, 0.38, 0.16)
                row.value:SetText(count .. string.format("%d%% *", math.floor(remembered * 100 + 0.5)))
                row.value:SetTextColor(0.72, 0.72, 0.72)
            end
        end
    end

    -- Visibility goes through alpha rather than Show/Hide: the container parents secure buttons
    -- and inherits their protection, so its size, position, scale and visibility may not be
    -- touched in combat - and companions are summoned mid-fight. Alpha is never protected.
    container:SetAlpha((capacity == 0 and liveCount == 0) and 0 or 1)

    -- The rows with nothing to show wait in order behind the live ones, so a companion summoned
    -- during a fight already sits where it belongs.
    for _, entry in ipairs(COMPANIONS) do
        if not active[entry.spell] then
            parked = parked + 1
            wantedSlot[entry.spell] = liveCount + parked
        end
    end

    wantedHeight = 30 + liveCount * ROW_HEIGHT
    ApplyLayout()
    ApplyHeight()
end

-- The container hides itself when there is nothing to show, and a hidden frame stops receiving
-- OnUpdate, so the timer lives on a separate frame that is always shown.
local driver = CreateFrame("Frame")

driver:SetScript("OnUpdate", function(self, elapsed)
    elapsedSinceUpdate = elapsedSinceUpdate + elapsed
    if elapsedSinceUpdate < UPDATE_INTERVAL then
        return
    end
    elapsedSinceUpdate = 0
    Refresh()
end)

-- ---------------------------------------------------------------------------------------------
-- Events and settings
-- ---------------------------------------------------------------------------------------------

local events = CreateFrame("Frame")
events:RegisterEvent("ADDON_LOADED")
events:RegisterEvent("PLAYER_LOGIN")
events:RegisterEvent("UNIT_AURA")
events:RegisterEvent("PLAYER_TARGET_CHANGED")
events:RegisterEvent("UPDATE_MOUSEOVER_UNIT")
events:RegisterEvent("CHAT_MSG_ADDON")
events:RegisterEvent("PLAYER_REGEN_ENABLED")

events:SetScript("OnEvent", function(self, event, arg1, arg2)
    if event == "CHAT_MSG_ADDON" then
        if arg1 == ADDON_PREFIX then
            feed = ParseFeed(arg2)
            feedTime = GetTime()
            Refresh()
        end
        return
    end

    if event == "ADDON_LOADED" and arg1 == ADDON_NAME then
        CoACompanionsDB = CoACompanionsDB or {}
        for key, value in pairs(defaults) do
            if CoACompanionsDB[key] == nil then
                CoACompanionsDB[key] = value
            end
        end
        container:ClearAllPoints()
        container:SetPoint(Option("point"), UIParent, Option("relativePoint"), Option("x"), Option("y"))
        container:SetScale(Option("scale"))
    elseif event == "PLAYER_LOGIN" then
        Refresh()
    elseif event == "PLAYER_REGEN_ENABLED" then
        -- Whatever the fight made the list want, the frame may finally be given.
        ApplyLayout()
        ApplyHeight()
    elseif event == "UNIT_AURA" then
        if arg1 == "player" then
            Refresh()
        end
    else
        Refresh()
    end
end)

SLASH_COACOMPANIONS1 = "/coacomp"
SLASH_COACOMPANIONS2 = "/coacompanions"

SlashCmdList["COACOMPANIONS"] = function(input)
    local command, argument = string.match(string.lower(input or ""), "^(%a*)%s*(.*)$")

    -- Scale and position are protected through the secure rows; the client refuses them in combat.
    if (command == "scale" or command == "reset") and InCombatLockdown() then
        Say("the frame cannot be moved or rescaled in combat.")
        return
    end

    if command == "lock" then
        CoACompanionsDB.locked = true
        Say("frame locked.")
    elseif command == "unlock" then
        CoACompanionsDB.locked = false
        Say("frame unlocked, drag it with the left mouse button.")
    elseif command == "scale" then
        local scale = tonumber(argument)
        if scale and scale >= 0.5 and scale <= 2.0 then
            CoACompanionsDB.scale = scale
            container:SetScale(scale)
            Say("scale set to " .. scale .. ".")
        else
            Say("scale takes a number between 0.5 and 2.0.")
        end
    elseif command == "reset" then
        CoACompanionsDB.point = defaults.point
        CoACompanionsDB.relativePoint = defaults.relativePoint
        CoACompanionsDB.x = defaults.x
        CoACompanionsDB.y = defaults.y
        CoACompanionsDB.scale = defaults.scale
        container:ClearAllPoints()
        container:SetPoint(defaults.point, UIParent, defaults.relativePoint, defaults.x, defaults.y)
        container:SetScale(defaults.scale)
        Say("position and scale reset.")
    else
        Say("commands: lock, unlock, scale <0.5-2.0>, reset.")
        Say("left click a row to target it, right click to release it.")
    end
end
