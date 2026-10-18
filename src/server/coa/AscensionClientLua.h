/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */
#ifndef ASCENSION_CLIENT_LUA_H
#define ASCENSION_CLIENT_LUA_H

#include "Define.h"
#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class Player;

struct AscensionClientLuaChunk
{
    uint32 RequirementTypeId = 0;
    uint32 RequirementId = 0;
    std::string Token;
    std::string Lua;
    std::string TargetAddon;
};

enum class AscensionClientLuaRoute : uint8
{
    Formatter,
    HotPatch,
    Wait,
};

using AscensionChallengeStateSync = void (*)(Player* player);
using AscensionClientUISettings = std::map<std::string, std::string, std::less<>>;
using AscensionClientLuaProvider = std::function<void(Player* player, AscensionClientUISettings const& settings,
    std::vector<AscensionClientLuaChunk>& chunks)>;

using AscensionClientLuaDeferral = void (*)(Player* player, AscensionClientLuaChunk const& chunk);
using AscensionClientLuaClock = std::chrono::steady_clock;

constexpr uint16 AscensionClientLuaHotPatchOpcode = 0x8F0;
constexpr uint16 AscensionClientLuaCapabilityOpcode = 0x8F1;
constexpr uint16 AscensionClientLuaResultOpcode = 0x8F2;

void RegisterAscensionChallengeStateSync(AscensionChallengeStateSync sync);
void RegisterAscensionClientLuaDeferral(AscensionClientLuaDeferral deferral);
void ResetAscensionClientLuaCapability(uint32 accountId, AscensionClientLuaClock::time_point now);
void SetAscensionClientLuaCapability(uint32 accountId, uint32 protocolVersion, uint8 mode);
void ForgetAscensionClientLuaCapability(uint32 accountId);
AscensionClientLuaRoute GetAscensionClientLuaRoute(uint32 accountId, AscensionClientLuaClock::time_point now);
std::string BuildAscensionClientLuaFormatter(std::string_view token, std::string_view lua);
void SendAscensionClientLuaFormatter(Player* player, uint32 requirementTypeId, std::string_view token, std::string_view lua);
void RunAscensionClientLua(Player* player, AscensionClientLuaChunk const& chunk);

void RegisterAscensionClientLuaProvider(AscensionClientLuaProvider provider);
std::string AscensionClientUILua(std::string_view name, AscensionClientUISettings const& settings);

#endif
