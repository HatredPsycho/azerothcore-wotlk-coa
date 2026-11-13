-- Abyssal Storage Plus - Core Logic
AbyssalStoragePlus = AbyssalStoragePlus or {}
AbyssalStoragePlus.items = {} -- { [itemEntry] = count }
AbyssalStoragePlus.totals = {} -- { [itemEntry] = bagPlusVault }
AbyssalStoragePlus.totalsAt = {} -- { [itemEntry] = GetTime() }
AbyssalStoragePlus.vaultVersion = 0
AbyssalStoragePlus.PREFIX = "ABYS"
AbyssalStoragePlusDB = AbyssalStoragePlusDB or {}

BINDING_HEADER_ABYSSALSTORAGEPLUS = "Abyssal Storage Plus"
BINDING_NAME_ABYSSALSTORAGEPLUS_DEPOSITALL = "Deposit All"

local function ApplyDefaultDepositBinding()
    if AbyssalStoragePlusDB.depositBindingInitialized then return end

    if GetBindingKey("ABYSSALSTORAGEPLUS_DEPOSITALL") then
        AbyssalStoragePlusDB.depositBindingInitialized = true
        return
    end

    local applied = true
    if GetBindingAction("CTRL-B") == "" then
        if SetBinding("CTRL-B", "ABYSSALSTORAGEPLUS_DEPOSITALL") then
            SaveBindings(GetCurrentBindingSet())
        else
            applied = false
        end
    end

    if applied then
        AbyssalStoragePlusDB.depositBindingInitialized = true
    end
end

-- ============================================================================
-- Utility
-- ============================================================================

-- Simple timer for 3.3.5
local timerFrame = CreateFrame("Frame")
local activeTimers = {}
timerFrame:SetScript("OnUpdate", function(self, elapsed)
    local i = 1
    while i <= #activeTimers do
        local t = activeTimers[i]
        t.remaining = t.remaining - elapsed
        if t.remaining <= 0 then
            table.remove(activeTimers, i)
            local ok, err = pcall(t.callback)
            if not ok then
                DEFAULT_CHAT_FRAME:AddMessage("|cffff4444Abyssal Storage Plus timer error: " .. tostring(err) .. "|r")
            end
        else
            i = i + 1
        end
    end
    if #activeTimers == 0 then
        self:Hide()
    end
end)
timerFrame:Hide()

function AbyssalStoragePlus.SetTimer(delay, callback)
    table.insert(activeTimers, { remaining = delay, callback = callback })
    timerFrame:Show()
end

-- Coalesce bursty server updates into a single UI rebuild
function AbyssalStoragePlus:RequestUIUpdate()
    if not self.UpdateUI then return end
    if self.RefreshTradeSkill then self:RefreshTradeSkill() end
    if self._uiUpdatePending then return end
    self._uiUpdatePending = true
    AbyssalStoragePlus.SetTimer(0.1, function()
        if not AbyssalStoragePlus._uiUpdatePending then return end
        AbyssalStoragePlus._uiUpdatePending = false
        AbyssalStoragePlus:UpdateUI()
    end)
end

-- Extract item entry from an item link: |Hitem:12345:...|h[Name]|h
function AbyssalStoragePlus:EntryFromLink(link)
    if not link then return nil end
    local entry = link:match("item:(%d+)")
    return entry and tonumber(entry) or nil
end

function AbyssalStoragePlus:GetItemCount(entry)
    return self.items[entry] or 0
end

-- cursor trick reuses native item sounds; skips if busy
function AbyssalStoragePlus:PlayItemSound(entry, bag, slot)
    if GetCursorInfo() then return end

    if bag and slot then
        PickupContainerItem(bag, slot)
    else
        if not GetItemInfo(entry) then return end
        PickupItem(entry)
    end

    ClearCursor()
end

-- ============================================================================
-- Server Communication
-- ============================================================================

function AbyssalStoragePlus:SendCommand(cmd)
    SendChatMessage("." .. cmd, "SAY")
end

function AbyssalStoragePlus:Withdraw(entry, count)
    self:PlayItemSound(entry)
    self:SendCommand("asp withdraw " .. entry .. " " .. (count or 0))
end

function AbyssalStoragePlus:Deposit()
    self._depositAllPending = true
    self:SendCommand("asp deposit")
end

function AbyssalStoragePlus:DepositItem(entry, count, bag, slot)
    self:PlayItemSound(entry, bag, slot)
    local cmd = "asp deposit " .. entry .. " " .. (count or 0)
    if bag and slot then
        cmd = cmd .. " " .. bag .. " " .. slot
    end
    self:SendCommand(cmd)
end

function AbyssalStoragePlus:RequestSync()
    self:SendCommand("asp sync")
end

-- ============================================================================
-- Message Parsing
-- ============================================================================

-- msg is the body (arg2 from CHAT_MSG_ADDON), e.g. "SYNC:entry,count;..."
function AbyssalStoragePlus:HandleMessage(msg)
    if not msg then return end

    local cmd, payload = msg:match("^(%u+):(.*)$")
    if not cmd then
        cmd = msg:match("^(%u+)$")
    end

    if not cmd then return end

    if cmd == "SYNCB" then
        self.items = {}
        self.totals = {}
        self.totalsAt = {}
        self._syncActive = true
        self._syncGeneration = (self._syncGeneration or 0) + 1
        local gen = self._syncGeneration
        -- Force-close a sync that never receives its end marker
        AbyssalStoragePlus.SetTimer(2, function()
            if AbyssalStoragePlus._syncActive and AbyssalStoragePlus._syncGeneration == gen then
                AbyssalStoragePlus._syncActive = false
                AbyssalStoragePlus:RequestUIUpdate()
            end
        end)
    elseif cmd == "SYNCE" then
        self._syncActive = false
        self.vaultVersion = self.vaultVersion + 1
        self:RequestUIUpdate()
    elseif cmd == "SYNC" then
        self:HandleSync(payload)
    elseif cmd == "UPD" then
        self:HandleUpdate(payload)
    elseif cmd == "DEL" then
        self:HandleDelete(payload)
    elseif cmd == "ERR" then
        self:HandleError(payload)
    elseif cmd == "VAULT" then
        self:HandleVaultNotice(payload)
    elseif cmd == "VAULTN" then
        self:HandleVaultNotification(payload)
    end
end

function AbyssalStoragePlus:HandleSync(payload)
    -- Standalone SYNC (older server or missed begin marker) replaces first
    local standalone = not self._syncActive
    if standalone then
        self.items = {}
        self.totals = {}
        self.totalsAt = {}
        self._syncActive = true
    end

    if payload and payload ~= "" then
        for pair in payload:gmatch("[^;]+") do
            local entry, count = pair:match("(%d+),(%d+)")
            if entry and count then
                self.items[tonumber(entry)] = tonumber(count)
            end
        end
    end

    if standalone then
        self._syncActive = false
        self.vaultVersion = self.vaultVersion + 1
        self:RequestUIUpdate()
    end
end

function AbyssalStoragePlus:HandleUpdate(payload)
    if not payload then return end
    local entry, count, total = payload:match("(%d+),(%d+),(%d+)")
    if not entry then
        entry, count = payload:match("(%d+),(%d+)")
    end
    if entry and count then
        entry = tonumber(entry)
        count = tonumber(count)
        total = total and tonumber(total) or nil
        if count > 0 then
            self.items[entry] = count
        else
            self.items[entry] = nil
        end
        if total then
            self.totals[entry] = total
            self.totalsAt[entry] = GetTime()
        else
            self.totals[entry] = nil
            self.totalsAt[entry] = nil
        end
        self.vaultVersion = self.vaultVersion + 1
        -- Skip UI refresh during multi-packet SYNC to avoid flashing incomplete data
        if not self._syncActive then self:RequestUIUpdate() end
    end
end

function AbyssalStoragePlus:HandleDelete(payload)
    if not payload then return end
    local entry = tonumber(payload)
    if entry then
        self.items[entry] = nil
        self.totals[entry] = nil
        self.totalsAt[entry] = nil
        self.vaultVersion = self.vaultVersion + 1
        if not self._syncActive then self:RequestUIUpdate() end
    end
end

function AbyssalStoragePlus:HandleError(payload)
    if payload then
        DEFAULT_CHAT_FRAME:AddMessage("|cffff4444Abyssal Storage Plus: " .. payload .. "|r")
    end
end

-- ============================================================================
-- Vaulted Loot Notices
-- ============================================================================

local VAULT_ICON_PATH = "Interface\\AddOns\\AbyssalStoragePlus\\VoidPortal.blp"
local VAULT_ICON_TARGET = 22 -- on-screen px at a 12 pt chat font
local VAULT_ICON_BASE_FONT = 12
local VAULT_LABEL = "|cff00ccffYou vaulted material:|r"

-- Icon sized from the displaying chat frame's own font size
local function VaultIconString(frame)
    local size = VAULT_ICON_BASE_FONT
    local source = frame
    if not (source and source.GetFont) and ChatFrame1 then
        source = ChatFrame1
    end
    if source and source.GetFont then
        local _, fontSize = source:GetFont()
        if fontSize and fontSize > 0 then
            size = fontSize
        end
    end
    local iconSize = math.floor(size * VAULT_ICON_TARGET / VAULT_ICON_BASE_FONT + 0.5)
    return ("|T%s:%d:%d|t"):format(VAULT_ICON_PATH, iconSize, iconSize)
end

-- Server announces auto-vaulted items before the client prints them
local pendingVaults = {} -- [itemEntry] = { count = n, at = GetTime() }

function AbyssalStoragePlus:HandleVaultNotice(payload)
    local entry = payload and tonumber(payload:match("^(%d+)"))
    if not entry then return end
    local now = GetTime()
    for e, pending in pairs(pendingVaults) do
        if now - pending.at > 3 then
            pendingVaults[e] = nil
        end
    end
    local pending = pendingVaults[entry]
    if not pending then
        pending = { count = 0, at = now }
        pendingVaults[entry] = pending
    end
    pending.count = pending.count + 1
    pending.at = now
end

-- Standalone notice for vaults with no game chat line (mail, quest reward)
function AbyssalStoragePlus:HandleVaultNotification(payload)
    local entry = payload and tonumber(payload:match("^(%d+)"))
    if not entry then return end

    local link = select(2, GetItemInfo(entry))
    if not link then return end

    local count = tonumber(payload:match(",(%d+)")) or 1
    self:PlayItemSound(entry)

    local text = VaultIconString(DEFAULT_CHAT_FRAME) .. " " .. VAULT_LABEL .. " " .. link
    if count > 1 then
        text = text .. "|cffffffff x" .. count .. "|r"
    end
    DEFAULT_CHAT_FRAME:AddMessage(text .. ".")
end

-- Turn a global format string into a pattern with lazy link capture
local function ToLootPattern(fmt)
    if not fmt then return nil end
    local pat = fmt:gsub("%%s", "\1"):gsub("%%d", "\2")
    pat = pat:gsub("([%^%$%(%)%%%.%[%]%*%+%-%?])", "%%%1")
    pat = pat:gsub("\1", "(.-)"):gsub("\2", "(%d+)")
    return "^" .. pat .. "$"
end

local SELF_LOOT_PATTERNS = {
    ToLootPattern(LOOT_ITEM_SELF),
    ToLootPattern(LOOT_ITEM_SELF_MULTIPLE),
    ToLootPattern(LOOT_ITEM_CREATED_SELF),
    ToLootPattern(LOOT_ITEM_CREATED_SELF_MULTIPLE),
    ToLootPattern(LOOT_ITEM_PUSHED_SELF),
    ToLootPattern(LOOT_ITEM_PUSHED_SELF_MULTIPLE),
}

-- True only for messages about items this player received
local function IsSelfLootMessage(msg)
    for _, pat in pairs(SELF_LOOT_PATTERNS) do
        if pat and msg:find(pat) then
            return true
        end
    end
    return false
end

local function FindPendingEntry(msg)
    local entry = tonumber(msg:match("item:(%d+)"))
    if entry then return entry end

    local shown = msg:match("%[(.-)%]")
    if not shown then return nil end
    local now = GetTime()
    for e, pending in pairs(pendingVaults) do
        if now - pending.at <= 3 and GetItemInfo(e) == shown then
            return e
        end
    end
    return nil
end

local function VaultLootFilter(frame, _, msg, ...)
    if type(msg) ~= "string" then return false end
    if not IsSelfLootMessage(msg) then return false end

    local entry = FindPendingEntry(msg)
    local pending = entry and pendingVaults[entry]
    if not pending then return false end
    if GetTime() - pending.at > 3 then
        pendingVaults[entry] = nil
        return false
    end

    local count = tonumber(msg:match("x(%d+)%.$")) or 1
    pending.count = pending.count - 1
    if pending.count <= 0 then
        pendingVaults[entry] = nil
    end

    local link = msg:match("|c%x%x%x%x%x%x%x%x|Hitem:[^|]+|h%[.-%]|h|r")
        or msg:match("|Hitem:[^|]+|h%[.-%]|h|r")
        or msg:match("%[.-%]")
    if not link then return false end

    local text = VaultIconString(frame) .. " " .. VAULT_LABEL .. " " .. link
    if count > 1 then
        text = text .. "|cffffffff x" .. count .. "|r"
    end
    return false, text .. ".", ...
end

ChatFrame_AddMessageEventFilter("CHAT_MSG_LOOT", VaultLootFilter)

-- ============================================================================
-- Event Frame
-- ============================================================================

if RegisterAddonMessagePrefix then
    RegisterAddonMessagePrefix(AbyssalStoragePlus.PREFIX)
end

local eventFrame = CreateFrame("Frame", "AbyssalStoragePlusEventFrame", UIParent)
eventFrame:RegisterEvent("CHAT_MSG_ADDON")
eventFrame:RegisterEvent("CHAT_MSG_SYSTEM")
eventFrame:RegisterEvent("PLAYER_LOGIN")

eventFrame:SetScript("OnEvent", function(self, event, arg1, arg2, ...)
    if event == "CHAT_MSG_ADDON" then
        -- arg1 = prefix ("ABYS"), arg2 = message body
        if arg1 == AbyssalStoragePlus.PREFIX then
            AbyssalStoragePlus:HandleMessage(arg2)
        end
    elseif event == "CHAT_MSG_SYSTEM" then
        if AbyssalStoragePlus._depositAllPending then
            local count = arg1 and arg1:match("Deposited (%d+) item stacks")
            if count then
                AbyssalStoragePlus._depositAllPending = false
                if tonumber(count) > 0 then PlaySound("PutDownBag") end
            end
        end
    elseif event == "PLAYER_LOGIN" then
        AbyssalStoragePlus:RequestSync()
        ApplyDefaultDepositBinding()
    end
end)

-- ============================================================================
-- Slash Commands
-- ============================================================================

SLASH_ABYSSAL1 = "/asp"
SLASH_ABYSSAL2 = "/abyssal"
SlashCmdList["ABYSSAL"] = function(msg)
    if msg == "" or msg == "show" then
        if AbyssalStoragePlusFrame then
            if AbyssalStoragePlusFrame:IsShown() then
                AbyssalStoragePlusFrame:Hide()
            else
                AbyssalStoragePlusFrame:Show()
            end
        end
    elseif msg == "deposit" then
        AbyssalStoragePlus:Deposit()
    elseif msg == "sync" then
        AbyssalStoragePlus:RequestSync()
    else
        local cmd, rest = msg:match("^(%S+)%s*(.*)")
        if cmd == "deposit" then
            local entry, count = rest:match("(%d+)%s*(%d*)")
            if entry then
                AbyssalStoragePlus:DepositItem(tonumber(entry), tonumber(count) or 0)
            else
                AbyssalStoragePlus:Deposit()
            end
        elseif cmd == "withdraw" then
            local entry, count = rest:match("(%d+)%s*(%d*)")
            if entry then
                AbyssalStoragePlus:Withdraw(tonumber(entry), tonumber(count) or 0)
            else
                DEFAULT_CHAT_FRAME:AddMessage("|cffff4444Usage: /asp withdraw <itemEntry> [count]|r")
            end
        else
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ccffAbyssal Storage Plus Commands:|r")
            DEFAULT_CHAT_FRAME:AddMessage("  /asp - Toggle storage window")
            DEFAULT_CHAT_FRAME:AddMessage("  /asp deposit - Deposit all trade goods")
            DEFAULT_CHAT_FRAME:AddMessage("  Right-click bag materials - Deposit that stack")
            DEFAULT_CHAT_FRAME:AddMessage("  /asp sync - Re-sync from server")
            DEFAULT_CHAT_FRAME:AddMessage("  /asp withdraw <id> [count] - Withdraw items")
        end
    end
end
