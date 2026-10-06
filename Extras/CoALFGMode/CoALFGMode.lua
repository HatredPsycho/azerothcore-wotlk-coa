-- CoA LFG Mode: a row above the Dungeon Finder's queue button that sets how the server forms your group.
-- The server keeps the setting per character (the same one as .lfgmode / .lfgchallenge) and
-- answers every request with the resulting state, so the row only ever shows what the server holds.

local PREFIX = "CoALFG"

local MODES = {
    { key = "matchmaking", label = "Matchmaking", hint = "Wait for other players, like the normal Dungeon Finder." },
    { key = "bots", label = "Fill with bots", hint = "Missing group members are filled with bots when you enter." },
    { key = "party", label = "Start now", hint = "Enter at once with your current group or alone. No role check." },
}

local CHALLENGES = { 0, 1, 2, 3, 4, 5, 10, 15, 20, 25, 40 }

local state = { known = false, mode = "matchmaking", challenge = 0, scaling = false, botfill = false }
local announce = false
local waited = 0
local bar, modeLabel, modeDropDown, challengeLabel, challengeDropDown

local function Print(text)
    DEFAULT_CHAT_FRAME:AddMessage("|cff66ccffCoA LFG:|r " .. text)
end

local function Send(body)
    SendAddonMessage(PREFIX, body, "WHISPER", UnitName("player"))
end

local function FindMode(key)
    for _, mode in ipairs(MODES) do
        if mode.key == key then
            return mode
        end
    end
end

local function ChallengeLabel(size)
    if size == 0 then
        return "Adaptive"
    end
    return size == 1 and "1 player" or (size .. " players")
end

local function IsGroupMemberNotLeader()
    if GetNumRaidMembers() > 0 then
        return not IsRaidLeader()
    end
    if GetNumPartyMembers() > 0 then
        return not IsPartyLeader()
    end
    return false
end

local function Status()
    if not state.known then
        if waited > 3 then
            return "No answer from the server: it does not run content scaling.", true
        end
        return "Asking the server ...", false
    end
    if not state.scaling then
        return "Content scaling is off on this realm; the Dungeon Finder works as usual.", true
    end
    if IsGroupMemberNotLeader() then
        return "In a group only the leader's setting is used.", true
    end
    if not state.botfill then
        return "No bot provider on this realm, so bots cannot fill the group.", false
    end
    return "Applies the next time you join the queue.", false
end

local function ShowTooltip(owner)
    local mode = FindMode(state.mode)
    GameTooltip:SetOwner(owner, "ANCHOR_TOP")
    GameTooltip:SetText("How the Dungeon Finder forms your group", 1, 1, 1)
    for _, entry in ipairs(MODES) do
        local color = entry == mode and "|cffffd100" or "|cffaaaaaa"
        GameTooltip:AddLine(color .. entry.label .. ":|r " .. entry.hint, 1, 1, 1, true)
    end
    GameTooltip:AddLine("Bosses: the group size encounters are scaled for. Adaptive follows the real group.", 1, 1, 1, true)
    local text, warning = Status()
    GameTooltip:AddLine(" ")
    if warning then
        GameTooltip:AddLine(text, 1, 0.5, 0.25, true)
    else
        GameTooltip:AddLine(text, 0.6, 0.6, 0.6, true)
    end
    GameTooltip:Show()
end

local function Refresh()
    if not bar then
        return
    end

    local usable = state.known and state.scaling
    local mode = FindMode(state.mode)
    UIDropDownMenu_SetText(modeDropDown, mode and mode.label or state.mode)
    UIDropDownMenu_SetText(challengeDropDown, ChallengeLabel(state.challenge))
    if usable then
        UIDropDownMenu_EnableDropDown(modeDropDown)
        UIDropDownMenu_EnableDropDown(challengeDropDown)
    else
        UIDropDownMenu_DisableDropDown(modeDropDown)
        UIDropDownMenu_DisableDropDown(challengeDropDown)
    end

    local _, warning = Status()
    if warning then
        modeLabel:SetTextColor(1, 0.5, 0.25)
    else
        modeLabel:SetTextColor(1, 0.82, 0)
    end

    if GameTooltip:IsOwned(bar) then
        ShowTooltip(bar)
    end
end

local function ModeDropDown_Initialize()
    for _, mode in ipairs(MODES) do
        local info = UIDropDownMenu_CreateInfo()
        info.text = mode.label
        info.value = mode.key
        info.checked = state.mode == mode.key
        info.disabled = mode.key == "bots" and not state.botfill
        info.func = function()
            Send("MODE " .. mode.key)
        end
        UIDropDownMenu_AddButton(info)
    end
end

local function ChallengeDropDown_Initialize()
    for _, size in ipairs(CHALLENGES) do
        local info = UIDropDownMenu_CreateInfo()
        info.text = ChallengeLabel(size)
        info.value = size
        info.checked = state.challenge == size
        info.func = function()
            Send("CHALLENGE " .. size)
        end
        UIDropDownMenu_AddButton(info)
    end
end

local function HookTooltip(frame)
    frame:EnableMouse(true)
    frame:SetScript("OnEnter", function()
        ShowTooltip(bar)
    end)
    frame:SetScript("OnLeave", GameTooltip_Hide)
end

local function CreateBar(finder, queueButton)
    bar = CreateFrame("Frame", "CoALFGModeBar", finder)
    bar:SetSize(400, 30)
    bar:SetPoint("BOTTOM", queueButton, "TOP", 0, 6)
    bar:SetFrameLevel(queueButton:GetFrameLevel())

    modeLabel = bar:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    modeLabel:SetPoint("LEFT", bar, "LEFT", 0, 2)
    modeLabel:SetText("Group:")

    modeDropDown = CreateFrame("Frame", "CoALFGModeDropDown", bar, "UIDropDownMenuTemplate")
    modeDropDown:SetPoint("LEFT", modeLabel, "RIGHT", -12, -2)
    UIDropDownMenu_SetWidth(modeDropDown, 110)
    UIDropDownMenu_Initialize(modeDropDown, ModeDropDown_Initialize)

    challengeLabel = bar:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    challengeLabel:SetPoint("LEFT", modeDropDown, "RIGHT", -6, 2)
    challengeLabel:SetText("Bosses:")

    challengeDropDown = CreateFrame("Frame", "CoALFGModeChallengeDropDown", bar, "UIDropDownMenuTemplate")
    challengeDropDown:SetPoint("LEFT", challengeLabel, "RIGHT", -12, -2)
    UIDropDownMenu_SetWidth(challengeDropDown, 80)
    UIDropDownMenu_Initialize(challengeDropDown, ChallengeDropDown_Initialize)

    HookTooltip(bar)

    bar:SetScript("OnUpdate", function(_, elapsed)
        if state.known or waited > 3 then
            return
        end
        waited = waited + elapsed
        if waited > 3 then
            Refresh()
        end
    end)
    bar:SetScript("OnShow", function()
        waited = 0
        Refresh()
        Send("GET")
    end)
    Refresh()
end

local function AttachBar()
    if bar then
        return
    end
    local finder = AscensionPVEFrameLFDFrame or LFDQueueFrame
    if not finder then
        return
    end
    local queueButton = finder.QueueButton or _G[finder:GetName() .. "FindGroupButton"] or LFDQueueFrameFindGroupButton
    if queueButton then
        CreateBar(finder, queueButton)
    end
end

local function OnState(message)
    local mode, challenge, scaling, botfill = message:match("^STATE (%a+) (%d+) ([01]) ([01])$")
    if not mode then
        return
    end
    local changed = state.known and (mode ~= state.mode or tonumber(challenge) ~= state.challenge)
    state.known = true
    state.mode = mode
    state.challenge = tonumber(challenge)
    state.scaling = scaling == "1"
    state.botfill = botfill == "1"
    Refresh()

    if announce or changed then
        announce = false
        local entry = FindMode(state.mode)
        Print(format("%s, bosses scaled for %s.", entry and entry.label or state.mode,
            ChallengeLabel(state.challenge):lower()))
    end
end

local events = CreateFrame("Frame")
events:RegisterEvent("PLAYER_ENTERING_WORLD")
events:RegisterEvent("CHAT_MSG_ADDON")
events:RegisterEvent("PARTY_MEMBERS_CHANGED")
events:RegisterEvent("PARTY_LEADER_CHANGED")
events:SetScript("OnEvent", function(_, event, arg1, arg2, _, arg4)
    if event == "CHAT_MSG_ADDON" then
        if arg1 == PREFIX and arg4 == UnitName("player") then
            OnState(arg2)
        end
    elseif event == "PLAYER_ENTERING_WORLD" then
        AttachBar()
        Send("GET")
    else
        Refresh()
    end
end)

SLASH_COALFGMODE1 = "/coalfg"
SlashCmdList.COALFGMODE = function(input)
    local command, argument = (input or ""):lower():match("^%s*(%S*)%s*(%S*)")
    if command == "matchmaking" or command == "bots" or command == "party" then
        announce = true
        Send("MODE " .. command)
    elseif command == "challenge" and tonumber(argument) then
        announce = true
        Send("CHALLENGE " .. tonumber(argument))
    elseif command == "challenge" and argument == "adaptive" then
        announce = true
        Send("CHALLENGE 0")
    elseif command == "" then
        announce = true
        Send("GET")
    else
        Print("/coalfg [matchmaking | bots | party | challenge adaptive|1-40]")
    end
end
