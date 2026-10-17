local PREFIX = "COARotation"
local EMPTY_ICON = "Interface\\Icons\\INV_Misc_QuestionMark"
local DEFAULTS = { point = "CENTER", x = 0, y = -180, scale = 1, locked = false, hidden = false }

BINDING_HEADER_COAROTATION = "CoA Rotation"
BINDING_NAME_COAROTATION_PRESS = "Cast the next rotation spell"

local spellId = 0
local spellName
local elapsed = 0

local button = CreateFrame("Button", "CoARotationButton", UIParent)
button:SetWidth(48)
button:SetHeight(48)
button:SetMovable(true)
button:SetClampedToScreen(true)
button:RegisterForClicks("LeftButtonUp")
button:RegisterForDrag("LeftButton")
button:Hide()

local icon = button:CreateTexture(nil, "BACKGROUND")
icon:SetAllPoints()
icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)

local border = button:CreateTexture(nil, "OVERLAY")
border:SetTexture("Interface\\Buttons\\UI-Quickslot2")
border:SetPoint("CENTER")
border:SetWidth(84)
border:SetHeight(84)

button:SetHighlightTexture("Interface\\Buttons\\ButtonHilight-Square", "ADD")
button:SetPushedTexture("Interface\\Buttons\\UI-Quickslot-Depress")

local cooldown = CreateFrame("Cooldown", nil, button, "CooldownFrameTemplate")
cooldown:SetAllPoints()

local hotkey = button:CreateFontString(nil, "OVERLAY", "NumberFontNormalSmallGray")
hotkey:SetPoint("TOPRIGHT", -2, -3)

local function Settings()
    CoARotationDB = CoARotationDB or {}
    for key, value in pairs(DEFAULTS) do
        if CoARotationDB[key] == nil then
            CoARotationDB[key] = value
        end
    end
    return CoARotationDB
end

local function Send(message)
    SendAddonMessage(PREFIX, message, "WHISPER", UnitName("player"))
end

function CoARotation_Press()
    Send("PRESS")
end

local function Place()
    local settings = Settings()
    button:ClearAllPoints()
    button:SetPoint(settings.point, UIParent, settings.point, settings.x, settings.y)
    button:SetScale(settings.scale)
    if settings.hidden then
        button:Hide()
    else
        button:Show()
    end
end

local function UpdateHotkey()
    local key = GetBindingKey("COAROTATION_PRESS")
    hotkey:SetText(key and GetBindingText(key, "KEY_") or "")
end

local function UpdateCooldown()
    if not spellName then
        cooldown:Hide()
        return
    end
    local start, duration, enabled = GetSpellCooldown(spellName)
    if start then
        CooldownFrame_SetTimer(cooldown, start, duration, enabled)
    end
end

local function UpdateUsable()
    if not spellName then
        icon:SetVertexColor(1, 1, 1)
        return
    end
    local usable, noMana = IsUsableSpell(spellName)
    if IsSpellInRange(spellName, "target") == 0 then
        icon:SetVertexColor(0.8, 0.1, 0.1)
    elseif noMana then
        icon:SetVertexColor(0.3, 0.3, 1)
    elseif not usable then
        icon:SetVertexColor(0.4, 0.4, 0.4)
    else
        icon:SetVertexColor(1, 1, 1)
    end
end

local function ShowSpell(id)
    spellId = id
    local name, _, texture = GetSpellInfo(id)
    if id == 0 or not name then
        spellName = nil
        icon:SetTexture(EMPTY_ICON)
        icon:SetDesaturated(true)
        button:SetAlpha(0.5)
    else
        spellName = name
        icon:SetTexture(texture)
        icon:SetDesaturated(false)
        button:SetAlpha(1)
    end
    UpdateCooldown()
    UpdateUsable()
    if GameTooltip:IsOwned(button) then
        button:GetScript("OnEnter")(button)
    end
end

button:SetScript("OnClick", CoARotation_Press)

button:SetScript("OnDragStart", function(self)
    if not Settings().locked then
        self:StartMoving()
    end
end)

button:SetScript("OnDragStop", function(self)
    self:StopMovingOrSizing()
    local point, _, _, x, y = self:GetPoint()
    local settings = Settings()
    settings.point, settings.x, settings.y = point, x, y
end)

button:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
    if spellId ~= 0 and spellName then
        GameTooltip:SetHyperlink("spell:" .. spellId)
    else
        GameTooltip:SetText("CoA Rotation")
        GameTooltip:AddLine("No spell available for the current target.", 1, 1, 1, true)
    end
    GameTooltip:AddLine("Click or use its key binding to cast the next spell.", 0.6, 0.6, 0.6, true)
    GameTooltip:Show()
end)

button:SetScript("OnLeave", function()
    GameTooltip:Hide()
end)

button:SetScript("OnUpdate", function(_, delta)
    elapsed = elapsed + delta
    if elapsed >= 0.2 then
        elapsed = 0
        UpdateUsable()
    end
end)

local events = CreateFrame("Frame")
events:RegisterEvent("PLAYER_ENTERING_WORLD")
events:RegisterEvent("CHAT_MSG_ADDON")
events:RegisterEvent("SPELL_UPDATE_COOLDOWN")
events:RegisterEvent("UPDATE_BINDINGS")
events:SetScript("OnEvent", function(_, event, prefix, message, channel, sender)
    if event == "PLAYER_ENTERING_WORLD" then
        Place()
        UpdateHotkey()
        ShowSpell(0)
        if not Settings().hidden then
            Send("ON")
        end
    elseif event == "CHAT_MSG_ADDON" then
        if prefix == PREFIX and channel == "WHISPER" and sender == UnitName("player") then
            local id = tonumber(message:match("^N;(%d+)$"))
            if id then
                ShowSpell(id)
            end
        end
    elseif event == "SPELL_UPDATE_COOLDOWN" then
        UpdateCooldown()
    elseif event == "UPDATE_BINDINGS" then
        UpdateHotkey()
    end
end)

SLASH_COAROTATION1 = "/coarot"
SLASH_COAROTATION2 = "/coarotation"
SlashCmdList.COAROTATION = function(input)
    local command, argument = (input or ""):lower():match("^%s*(%S*)%s*(.-)%s*$")
    local settings = Settings()
    if command == "lock" or command == "unlock" then
        settings.locked = command == "lock"
    elseif command == "scale" and tonumber(argument) then
        settings.scale = math.min(2, math.max(0.5, tonumber(argument)))
        Place()
    elseif command == "reset" then
        CoARotationDB = nil
        Place()
    elseif command == "hide" then
        settings.hidden = true
        Send("OFF")
        Place()
    elseif command == "show" then
        settings.hidden = false
        Place()
        Send("ON")
    else
        print("|cff00ccffCoA Rotation|r: /coarot lock | unlock | scale <0.5-2> | reset | hide | show")
    end
end
