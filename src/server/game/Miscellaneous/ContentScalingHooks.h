/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU GPL.
 */

#ifndef AC_CONTENT_SCALING_HOOKS_H
#define AC_CONTENT_SCALING_HOOKS_H

#include "Define.h"
#include <atomic>
#include <cstdint>

class Creature;
struct CreatureTemplate;
class Player;
class Quest;
class Unit;

/// Resolvers a content scaling module installs to map the realm's content onto other levels. The core
/// asks through them instead of reading the authored value; with nothing installed every answer is the
/// authored value, so the stock paths are unchanged. They sit beside LocalLevelScaling and share its
/// namespace, but in a file of their own, so upstream changes to LocalLevelScaling.h do not touch them.
namespace LocalLevelScaling
{
/// Set while a content scaling module is in charge, so systems that scale the same thing by themselves
/// stand down instead of applying a second answer on top of the module's.
inline std::atomic<bool> ContentScalingActive{false};

using QuestBaseLevelResolver = std::int32_t (*)(Quest const*);
inline std::atomic<QuestBaseLevelResolver> QuestBaseLevelOwner{nullptr};

/// The level a quest stands at on this realm: its authored level, or the one the installed module maps
/// it to. -1 (follows the character) stays -1.
AC_GAME_API std::int32_t GetEffectiveQuestBaseLevel(Quest const* quest);

using QuestMinLevelResolver = std::uint32_t (*)(Quest const*);
inline std::atomic<QuestMinLevelResolver> QuestMinLevelOwner{nullptr};

/// The level a character needs to take a quest on this realm.
AC_GAME_API std::uint32_t GetEffectiveQuestMinLevel(Quest const* quest);

using QuestMoneyMaxLevelResolver = std::uint32_t (*)(Quest const*, std::uint32_t);
inline std::atomic<QuestMoneyMaxLevelResolver> QuestMoneyMaxLevelOwner{nullptr};

/// The money a quest pays in place of experience at the level cap, given the core's own value.
AC_GAME_API std::uint32_t GetEffectiveQuestMoneyMaxLevel(Quest const* quest, std::uint32_t defaultRewardMoney);

using QuestRewardRateResolver = float (*)(Player const*, Quest const*, float);
inline std::atomic<QuestRewardRateResolver> QuestRewardRateOwner{nullptr};

/// The experience rate a quest pays a character, given the core's own rate.
AC_GAME_API float GetEffectiveQuestRewardRate(Player const* player, Quest const* quest, float defaultRate);

using CreatureBaseLevelResolver = std::uint8_t (*)(CreatureTemplate const*, Creature const*);
inline std::atomic<CreatureBaseLevelResolver> CreatureBaseLevelOwner{nullptr};

/// The level a creature stands at on this realm before any per-character view lifts it.
AC_GAME_API std::uint8_t GetEffectiveCreatureBaseLevel(CreatureTemplate const* cinfo,
    Creature const* creature = nullptr);

using CreatureArmorResolver = float (*)(CreatureTemplate const*, Creature const*, float);
inline std::atomic<CreatureArmorResolver> CreatureArmorOwner{nullptr};

/// The armor a creature wears. Creature::UpdateEntry regenerates armor after SelectLevel, so a value a module
/// set from the level hook would be overwritten; the core asks here with the generated value instead.
AC_GAME_API float GetEffectiveCreatureArmor(CreatureTemplate const* cinfo, Creature const* creature,
    float generatedArmor);

using KillContentLevelResolver = std::uint8_t (*)(Player const*, Unit const*, std::uint8_t);
inline std::atomic<KillContentLevelResolver> KillContentLevelOwner{nullptr};

/// The expansion level band a kill counts in for experience, given the core's own for the map and zone.
AC_GAME_API std::uint8_t GetEffectiveKillContentLevel(Player const* player, Unit const* victim,
    std::uint8_t defaultContentLevel);

using AreaContentLevelResolver = std::uint8_t (*)(std::uint32_t, std::uint32_t, std::uint8_t);
inline std::atomic<AreaContentLevelResolver> AreaContentLevelOwner{nullptr};

/// The level an area's content stands at on this realm, given the level it was written for.
inline std::uint8_t GetEffectiveAreaContentLevel(std::uint32_t areaId, std::uint32_t mapId,
    std::uint8_t authoredLevel)
{
    AreaContentLevelResolver const owner = AreaContentLevelOwner.load(std::memory_order_relaxed);
    return owner ? owner(areaId, mapId, authoredLevel) : authoredLevel;
}

using AuthenticMapResolver = bool (*)(std::uint32_t mapId, std::uint8_t difficulty);
inline std::atomic<AuthenticMapResolver> AuthenticMapOwner{nullptr};

/// Maps the installed module plays at their authored levels on a difficulty: no per-character view and no
/// loot lift applies there. With nothing installed no map is authentic.
inline bool IsAuthenticMap(std::uint32_t mapId, std::uint8_t difficulty)
{
    AuthenticMapResolver const owner = AuthenticMapOwner.load(std::memory_order_relaxed);
    return owner && owner(mapId, difficulty);
}
}

#endif
