-- Abyssal Storage Plus options panel (Blizzard Interface Options)

AbyssalStoragePlusDB = AbyssalStoragePlusDB or {}

local SETTING_ROWS = {
    { key = "loot",   label = "Auto-vault materials on Loot",                   default = true },
    { key = "craft",  label = "Auto-vault materials on Craft",                  default = false },
    { key = "mail",   label = "Auto-vault materials on Opening Mail",           default = false },
    { key = "quest",  label = "Auto-vault materials on Quest Reward",           default = false },
    { key = "vendor", label = "Auto-vault materials on Vendor Buys",            default = false },
    { key = "accept", label = "Auto-withdraw quest materials on Quest Accept",  default = true },
}

local function GetSettings()
    AbyssalStoragePlusDB.settings = AbyssalStoragePlusDB.settings or {}
    for _, row in ipairs(SETTING_ROWS) do
        if AbyssalStoragePlusDB.settings[row.key] == nil then
            AbyssalStoragePlusDB.settings[row.key] = row.default
        end
    end
    return AbyssalStoragePlusDB.settings
end

local function SendSettings()
    local settings = GetSettings()
    local values = {}
    for _, row in ipairs(SETTING_ROWS) do
        values[#values + 1] = settings[row.key] and 1 or 0
    end
    AbyssalStoragePlus:SendCommand("asp settings " .. table.concat(values, " "))
end

local panel = CreateFrame("Frame", "AbyssalStoragePlusOptions", InterfaceOptionsFramePanelContainer)
panel.name = "Abyssal Storage Plus"
panel:Hide()

local title = panel:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
title:SetPoint("TOPLEFT", 16, -16)
title:SetText("Abyssal Storage Plus")

local note = panel:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
note:SetPoint("TOPLEFT", 16, -40)
note:SetText("Settings apply account-wide and take effect immediately.")

panel.refresh = function()
    local settings = GetSettings()
    for _, row in ipairs(SETTING_ROWS) do
        row.check:SetChecked(settings[row.key])
    end
end

local offsetY = -66
for _, row in ipairs(SETTING_ROWS) do
    local check = CreateFrame("CheckButton", "AbyssalStoragePlusOptions" .. row.key, panel, "InterfaceOptionsCheckButtonTemplate")
    check:SetPoint("TOPLEFT", 16, offsetY)
    _G[check:GetName() .. "Text"]:SetText(row.label)
    check.key = row.key
    check:SetScript("OnClick", function(self)
        -- GetChecked() returns nil when unchecked in 3.3.5
        GetSettings()[self.key] = self:GetChecked() and true or false
        SendSettings()
    end)
    row.check = check
    offsetY = offsetY - 26
end

InterfaceOptions_AddCategory(panel)

-- Push the stored settings once the character is in the world
local loginFrame = CreateFrame("Frame")
loginFrame:RegisterEvent("PLAYER_LOGIN")
loginFrame:SetScript("OnEvent", function()
    SendSettings()
end)
