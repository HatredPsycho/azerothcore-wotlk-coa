/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */
#include "AscensionClientLua.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "Opcodes.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ServerScript.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace
{
    constexpr uint32 LoginDelayMs = 5000;
    constexpr uint32 ReloadDelayMs = 1500;
    constexpr std::size_t MaxNameLength = 32;
    constexpr std::size_t MaxDataLength = 200;
    constexpr std::string_view SettingsPrefix = "COAUI\tSAVE\t";

    constexpr std::string_view UILua = R"lua(
CoAUI = CoAUI or { Settings = {} }
function CoAUI.Parse(text)
    local values = {}
    for key, value in string.gmatch(text or "", "([%w_]+)=([^;]*)") do
        values[key] = tonumber(value) or value
    end
    return values
end
function CoAUI.Save(name, values)
    local parts = {}
    for key, value in pairs(values) do
        if type(value) == "boolean" then
            value = value and 1 or 0
        elseif type(value) == "number" then
            value = string.format("%.2f", value):gsub("%.?0+$", "")
        end
        parts[#parts + 1] = key .. "=" .. tostring(value)
    end
    table.sort(parts)
    local text = table.concat(parts, ";")
    CoAUI.Settings[name] = CoAUI.Parse(text)
    SendAddonMessage("COAUI", "SAVE\t" .. name .. "\t" .. text, "WHISPER", UnitName("player"))
end
)lua";

    std::vector<AscensionClientLuaProvider>& Providers()
    {
        static std::vector<AscensionClientLuaProvider> providers;
        return providers;
    }

    std::mutex StateLock;
    std::unordered_set<ObjectGuid> Reloads;
    std::unordered_map<ObjectGuid, uint32> Pending;
    std::atomic<std::size_t> Watched = 0;

    void Recount()
    {
        Watched = Reloads.size() + Pending.size();
    }

    bool ValidName(std::string_view name)
    {
        return !name.empty() && name.size() <= MaxNameLength &&
            std::all_of(name.begin(), name.end(), [](char c) { return std::isalnum(uint8(c)) || c == '_'; });
    }

    bool ValidData(std::string_view data)
    {
        return data.size() <= MaxDataLength && std::all_of(data.begin(), data.end(), [](char c)
        {
            return std::isalnum(uint8(c)) || c == '_' || c == '.' || c == '-' || c == '=' || c == ';';
        });
    }

    bool Pushable(Player* player)
    {
        WorldSession* session = player ? player->GetSession() : nullptr;
        return session && !session->IsBot() && player->IsInWorld();
    }

    void Schedule(ObjectGuid guid, uint32 delay)
    {
        std::lock_guard lock(StateLock);
        auto [found, inserted] = Pending.emplace(guid, delay);
        if (!inserted)
            found->second = std::max(found->second, delay);
        Recount();
    }

    AscensionClientUISettings LoadSettings(Player* player)
    {
        AscensionClientUISettings settings;
        if (QueryResult result = CharacterDatabase.Query(
                "SELECT `name`, `data` FROM `coa_client_ui_settings` WHERE `guid` = {}",
                player->GetGUID().GetCounter()))
        {
            do
            {
                Field* fields = result->Fetch();
                settings.emplace(fields[0].Get<std::string>(), fields[1].Get<std::string>());
            } while (result->NextRow());
        }
        return settings;
    }

    void Push(Player* player)
    {
        if (!Pushable(player) || Providers().empty())
            return;

        AscensionClientUISettings const settings = LoadSettings(player);
        std::vector<AscensionClientLuaChunk> chunks;
        for (AscensionClientLuaProvider const& provider : Providers())
            provider(player, settings, chunks);
        for (AscensionClientLuaChunk const& chunk : chunks)
            RunAscensionClientLua(player, chunk);
    }

    class AscensionClientLuaPlayers final : public PlayerScript
    {
    public:
        AscensionClientLuaPlayers() : PlayerScript("ascension_client_lua_players", {PLAYERHOOK_ON_LOGIN,
            PLAYERHOOK_ON_UPDATE, PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_DELETE,
            PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT}) { }

        void OnPlayerLogin(Player* player) override
        {
            if (Pushable(player))
                Schedule(player->GetGUID(), LoginDelayMs);
        }

        void OnPlayerUpdate(Player* player, uint32 diff) override
        {
            if (!Watched)
                return;

            bool due = false;
            {
                std::lock_guard lock(StateLock);
                ObjectGuid const guid = player->GetGUID();
                if (Reloads.erase(guid))
                {
                    auto [found, inserted] = Pending.emplace(guid, ReloadDelayMs);
                    if (!inserted)
                        found->second = std::max(found->second, ReloadDelayMs);
                }
                auto found = Pending.find(guid);
                if (found != Pending.end())
                {
                    if (found->second <= diff)
                    {
                        Pending.erase(found);
                        due = true;
                    }
                    else
                        found->second -= diff;
                }
                Recount();
            }
            if (due)
                Push(player);
        }

        void OnPlayerLogout(Player* player) override
        {
            std::lock_guard lock(StateLock);
            Reloads.erase(player->GetGUID());
            Pending.erase(player->GetGUID());
            Recount();
        }

        void OnPlayerDelete(ObjectGuid guid, uint32) override
        {
            CharacterDatabase.Execute("DELETE FROM `coa_client_ui_settings` WHERE `guid` = {}", guid.GetCounter());
        }

        bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& message,
            Player* receiver) override
        {
            if (type != CHAT_MSG_WHISPER || language != LANG_ADDON || receiver != player ||
                !message.starts_with(SettingsPrefix))
                return true;

            std::string_view const body = std::string_view(message).substr(SettingsPrefix.size());
            std::size_t const split = body.find('\t');
            if (split == std::string_view::npos)
                return false;

            std::string_view const name = body.substr(0, split);
            std::string_view const data = body.substr(split + 1);
            if (ValidName(name) && ValidData(data))
                CharacterDatabase.Execute(
                    "REPLACE INTO `coa_client_ui_settings` (`guid`, `name`, `data`) VALUES ({}, '{}', '{}')",
                    player->GetGUID().GetCounter(), std::string(name), std::string(data));
            return false;
        }
    };

    class AscensionClientLuaReloads final : public ServerScript
    {
    public:
        AscensionClientLuaReloads() : ServerScript("ascension_client_lua_reloads", {SERVERHOOK_CAN_PACKET_RECEIVE}) { }

        bool CanPacketReceive(WorldSession* session, WorldPacket const& packet) override
        {
            if (packet.GetOpcode() != CMSG_SET_ACTIONBAR_TOGGLES || !session || session->IsBot())
                return true;

            if (Player* player = session->GetPlayer())
            {
                std::lock_guard lock(StateLock);
                Reloads.insert(player->GetGUID());
                Recount();
            }
            return true;
        }
    };
}

void RegisterAscensionClientLuaProvider(AscensionClientLuaProvider provider)
{
    Providers().push_back(std::move(provider));
}

std::string AscensionClientUILua(std::string_view name, AscensionClientUISettings const& settings)
{
    std::string lua(UILua);
    if (!ValidName(name))
        return lua;

    auto const found = settings.find(name);
    std::string_view const data = found != settings.end() && ValidData(found->second) ? found->second : "";
    lua.append("CoAUI.Settings[\"").append(name).append("\"] = CoAUI.Parse(\"").append(data).append("\")\n");
    return lua;
}

void AddSC_AscensionClientLuaLoader()
{
    new AscensionClientLuaPlayers();
    new AscensionClientLuaReloads();
}
