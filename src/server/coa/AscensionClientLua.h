/* Copyright (C) 2016+ AzerothCore, GNU AGPL v3. */
#ifndef ASCENSION_CLIENT_LUA_H
#define ASCENSION_CLIENT_LUA_H

#include "Define.h"
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
};

using AscensionChallengeStateSync = void (*)(Player* player);
using AscensionClientUISettings = std::map<std::string, std::string, std::less<>>;
using AscensionClientLuaProvider = std::function<void(Player* player, AscensionClientUISettings const& settings,
    std::vector<AscensionClientLuaChunk>& chunks)>;

void RegisterAscensionChallengeStateSync(AscensionChallengeStateSync sync);
std::string BuildAscensionClientLuaFormatter(std::string_view token, std::string_view lua);
void SendAscensionClientLuaFormatter(Player* player, uint32 requirementTypeId, std::string_view token, std::string_view lua);
void RunAscensionClientLua(Player* player, AscensionClientLuaChunk const& chunk);

void RegisterAscensionClientLuaProvider(AscensionClientLuaProvider provider);
std::string AscensionClientUILua(std::string_view name, AscensionClientUISettings const& settings);

#endif
