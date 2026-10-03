-- CoA Scale Tooltips
--
-- A buff's text is built by the client from its own Spell.dbc. The aura packet carries no effect
-- amounts, so an item proc keeps showing the number it was authored with even after the realm cut
-- the item it came from down to a lower level. The server sends the applied amount over the
-- CoAScale addon channel; this file puts it into the tooltip where the authored one stood.
--
-- The authored number arrives with it, so the replacement is a substitution inside the finished
-- line. Nothing here has to know how the sentence is built or which language it is written in.

local PREFIX = "CoAScale"

local applied = {}
local items = {}
local asked = {}

local function RememberAura(body)
  local spellId, effectIndex, authored, effective =
    string.match(body, "^(%d+):(%d+):(%-?%d+):(%-?%d+)$")
  if not spellId then
    return
  end

  spellId = tonumber(spellId)
  local effects = applied[spellId]
  if not effects then
    effects = {}
    applied[spellId] = effects
  end

  effects[tonumber(effectIndex)] = { authored = tonumber(authored), effective = tonumber(effective) }
end

local function RememberItem(body)
  local itemId, pairs_ = string.match(body, "^(%d+):(.*)$")
  if not itemId then
    return
  end

  local amounts = {}
  for authoredMin, effectiveMin, authoredMax, effectiveMax in
      string.gmatch(pairs_ or "", "(%-?%d+),(%-?%d+),(%-?%d+),(%-?%d+)") do
    table.insert(amounts, { authored = tonumber(authoredMin), effective = tonumber(effectiveMin) })
    if authoredMax ~= authoredMin then
      table.insert(amounts, { authored = tonumber(authoredMax), effective = tonumber(effectiveMax) })
    end
  end

  items[tonumber(itemId)] = amounts
end

local function Remember(message)
  local kind, body = string.match(message, "^(%a):(.*)$")
  if kind == "A" then
    RememberAura(body)
  elseif kind == "I" then
    RememberItem(body)
  end
end

-- Bounded on both sides so that an authored 147 is not found inside an unrelated 1472. A negative
-- amount reaches this as a leading minus, which is a quantifier in a Lua pattern and has to be
-- escaped before it is searched for.
local function Substitute(text, authored, effective)
  local needle = tostring(authored)
  local pattern

  if string.sub(needle, 1, 1) == "-" then
    pattern = "%f[%-%d]%-" .. string.sub(needle, 2) .. "%f[%D]"
  else
    pattern = "%f[%d]" .. needle .. "%f[%D]"
  end

  return string.gsub(text, pattern, tostring(effective))
end

-- A stacking buff is printed with its amount already multiplied by the stack count, so from the
-- second stack onwards the authored figure is no longer in the text to be found. Both forms are
-- searched for: the one the client prints at a single stack and the one it prints at this many.
local function Rewrite(tooltip, amounts, stacks)
  if not amounts then
    return
  end

  local name = tooltip:GetName()
  if not name then
    return
  end

  local touched = false

  for i = 2, tooltip:NumLines() do
    local line = _G[name .. "TextLeft" .. i]
    local text = line and line:GetText()

    if text then
      local rewritten = text
      for _, amount in pairs(amounts) do
        if amount.authored ~= amount.effective then
          if stacks and stacks > 1 then
            rewritten = Substitute(rewritten, amount.authored * stacks, amount.effective * stacks)
          end
          rewritten = Substitute(rewritten, amount.authored, amount.effective)
        end
      end

      if rewritten ~= text then
        line:SetText(rewritten)
        touched = true
      end
    end
  end

  if touched then
    tooltip:Show()
  end
end

-- UnitAura and its two filtered forms return the stack count as their fourth value and the spell id
-- as their eleventh in 3.3.5a. An unstacked aura reports zero rather than one.
local function RewriteFromAura(tooltip, ...)
  local stacks = select(4, ...)
  local spellId = select(11, ...)
  Rewrite(tooltip, spellId and applied[spellId], stacks)
end

hooksecurefunc(GameTooltip, "SetUnitBuff", function(self, unit, index, filter)
  RewriteFromAura(self, UnitBuff(unit, index, filter))
end)

hooksecurefunc(GameTooltip, "SetUnitDebuff", function(self, unit, index, filter)
  RewriteFromAura(self, UnitDebuff(unit, index, filter))
end)

hooksecurefunc(GameTooltip, "SetUnitAura", function(self, unit, index, filter)
  RewriteFromAura(self, UnitAura(unit, index, filter))
end)

-- An item's own statistics already arrive scaled from the server; only the sentences the client
-- builds from its copy of the spell are wrong. Those cannot be pushed the way an applied aura can,
-- because the item may belong to a vendor or be a link in chat, so the server is asked for them.
local function Ask(itemId)
  if asked[itemId] then
    return
  end

  asked[itemId] = true
  SendAddonMessage(PREFIX, "Q:" .. itemId, "WHISPER", UnitName("player"))
end

local function RewriteItem(tooltip)
  local _, link = tooltip:GetItem()
  local itemId = link and tonumber(string.match(link, "item:(%d+)"))
  if not itemId then
    return
  end

  local amounts = items[itemId]
  if amounts then
    Rewrite(tooltip, amounts)
  else
    Ask(itemId)
  end
end

local watched = { GameTooltip, ItemRefTooltip, ShoppingTooltip1, ShoppingTooltip2 }

for _, tooltip in pairs(watched) do
  if tooltip then
    tooltip:HookScript("OnTooltipSetItem", RewriteItem)
  end
end

-- The first look at an item is what sends the question, so the answer arrives while the tooltip is
-- already on screen. Its lines are still there to be rewritten.
local function RefreshVisible()
  for _, tooltip in pairs(watched) do
    if tooltip and tooltip:IsShown() then
      RewriteItem(tooltip)
    end
  end
end

-- 3.3.5a has no RegisterAddonMessagePrefix; that only arrived in 4.x. The event fires regardless.
local listener = CreateFrame("Frame")
listener:RegisterEvent("CHAT_MSG_ADDON")
listener:SetScript("OnEvent", function(self, event, prefix, message)
  if prefix == PREFIX and message then
    Remember(message)
    RefreshVisible()
  end
end)
