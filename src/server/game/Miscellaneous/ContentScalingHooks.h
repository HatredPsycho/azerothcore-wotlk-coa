/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU GPL.
 */

#ifndef AC_CONTENT_SCALING_HOOKS_H
#define AC_CONTENT_SCALING_HOOKS_H

#include "Define.h"
#include "LocalLevelScaling.h"
#include <atomic>
#include <cstdint>
#include <utility>
#include <vector>

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

using ItemEffectValueResolver = std::int32_t (*)(std::uint32_t, std::int32_t);
inline std::atomic<ItemEffectValueResolver> ItemEffectValueOwner{nullptr};

/// The flat value a spell cast from an item carries, given the value written into the spell.
inline std::int32_t GetEffectiveItemEffectValue(std::uint32_t itemEntry, std::int32_t value)
{
    ItemEffectValueResolver const owner = ItemEffectValueOwner.load(std::memory_order_relaxed);
    return owner && itemEntry && value ? owner(itemEntry, value) : value;
}

using ItemMarketValueResolver = std::uint32_t (*)(std::uint32_t, std::uint32_t);
inline std::atomic<ItemMarketValueResolver> ItemMarketValueOwner{nullptr};

/// What an item is worth on this realm, given its vendor price; vendors themselves keep the template's.
inline std::uint32_t GetEffectiveItemMarketValue(std::uint32_t itemEntry, std::uint32_t value)
{
    ItemMarketValueResolver const owner = ItemMarketValueOwner.load(std::memory_order_relaxed);
    return owner ? owner(itemEntry, value) : value;
}

using AuthoredItemLevelResolver = std::uint32_t (*)(std::uint32_t, std::uint32_t);
inline std::atomic<AuthoredItemLevelResolver> AuthoredItemLevelOwner{nullptr};

/// The item level an item was written with, given the one its template has on this realm.
inline std::uint32_t GetAuthoredItemLevel(std::uint32_t itemEntry, std::uint32_t itemLevel)
{
    AuthoredItemLevelResolver const owner = AuthoredItemLevelOwner.load(std::memory_order_relaxed);
    return owner ? owner(itemEntry, itemLevel) : itemLevel;
}

using ScaledItemBaseResolver = std::uint32_t (*)(std::uint32_t entry);
inline std::atomic<ScaledItemBaseResolver> ScaledItemBaseOwner{nullptr};

/// The authored item a lifted copy was made from; any other entry answers itself.
inline std::uint32_t BaseItemEntry(std::uint32_t entry)
{
    ScaledItemBaseResolver const owner = ScaledItemBaseOwner.load(std::memory_order_relaxed);
    return owner ? owner(entry) : entry;
}

using ItemEnchantmentAmountResolver = std::uint32_t (*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
inline std::atomic<ItemEnchantmentAmountResolver> ItemEnchantmentAmountOwner{nullptr};

/// The flat amount an enchantment, a gem or a socket bonus adds to the item it sits on.
AC_GAME_API std::uint32_t GetEffectiveItemEnchantmentAmount(std::uint32_t hostItemEntry, std::uint32_t enchantmentType,
    std::uint32_t statType, std::uint32_t amount);

using EnchantmentRequiredLevelResolver = std::uint32_t (*)(std::uint32_t);
inline std::atomic<EnchantmentRequiredLevelResolver> EnchantmentRequiredLevelOwner{nullptr};

/// The level an enchantment asks of the character wearing it.
AC_GAME_API std::uint32_t GetEffectiveEnchantmentRequiredLevel(std::uint32_t requiredLevel);

using AbilityRequiredLevelResolver = std::uint8_t (*)(std::uint8_t);
inline std::atomic<AbilityRequiredLevelResolver> AbilityRequiredLevelOwner{nullptr};

/// The level a rank of an ability asks of the character who learns it.
inline std::uint8_t GetEffectiveAbilityRequiredLevel(std::uint8_t requiredLevel)
{
    AbilityRequiredLevelResolver const owner = AbilityRequiredLevelOwner.load(std::memory_order_relaxed);
    return owner ? owner(requiredLevel) : requiredLevel;
}

using SummonedGuardianLevelResolver = std::uint8_t (*)(Unit const* summoner, Creature const* guardian,
    std::uint8_t summonLevel);
inline std::atomic<SummonedGuardianLevelResolver> SummonedGuardianLevelOwner{nullptr};

/// The level a summoned guardian enters the world at, given the summoner's level the core would give it.
inline std::uint8_t GetEffectiveSummonedGuardianLevel(Unit const* summoner, Creature const* guardian,
    std::uint8_t summonLevel)
{
    SummonedGuardianLevelResolver const owner = SummonedGuardianLevelOwner.load(std::memory_order_relaxed);
    return owner ? owner(summoner, guardian, summonLevel) : summonLevel;
}

using CreatureWindowResolver = std::uint8_t (*)(Player const*, Creature const*, std::uint8_t ownLevel);
inline std::atomic<CreatureWindowResolver> CreatureWindowOwner{nullptr};

/// Where a lifted open-world creature stands for a viewer: its place in its zone, within a window around the
/// viewer, instead of the viewer's level minus the offset. 0 when nothing is installed or it has no opinion.
inline std::uint8_t GetCreatureWindowLevel(Player const* viewer, Creature const* creature, std::uint8_t ownLevel)
{
    CreatureWindowResolver const owner = CreatureWindowOwner.load(std::memory_order_relaxed);
    return owner ? owner(viewer, creature, ownLevel) : 0;
}

using ScalingScopeResolver = void (*)(std::vector<std::uint32_t>& worldMaps,
                                      std::vector<std::pair<std::uint32_t, LevelBand>>& dungeonBands);
inline std::atomic<ScalingScopeResolver> ScalingScopeOwner{nullptr};

/// The open-world maps and dungeon bands the installed module adds to where creatures scale for a viewer,
/// asked whenever the scaling scope is built. A band given here replaces the one the dungeon finder names.
inline void AddContentScalingScope(std::vector<std::uint32_t>& worldMaps,
                                   std::vector<std::pair<std::uint32_t, LevelBand>>& dungeonBands)
{
    if (ScalingScopeResolver const owner = ScalingScopeOwner.load(std::memory_order_relaxed))
        owner(worldMaps, dungeonBands);
}

/// What the random bots take from a content scaling module that moves the world's levels: the level they stop
/// at, the levels they ride and fly from, and the level bracket of each zone they hunt in.
struct BotLevelPlan
{
    std::uint8_t maxLevel{0};
    std::uint8_t groundMountLevel{0};
    std::uint8_t fastGroundMountLevel{0};
    std::uint8_t flyMountLevel{0};
    std::uint8_t fastFlyMountLevel{0};
    std::vector<std::pair<std::uint32_t, std::pair<std::uint8_t, std::uint8_t>>> zoneBrackets;
};

using BotLevelPlanResolver = bool (*)(BotLevelPlan&);
inline std::atomic<BotLevelPlanResolver> BotLevelPlanOwner{nullptr};

/// The installed module's plan for the random bots; false when nothing is installed or it has none.
inline bool GetContentScalingBotLevelPlan(BotLevelPlan& plan)
{
    BotLevelPlanResolver const owner = BotLevelPlanOwner.load(std::memory_order_relaxed);
    return owner && owner(plan);
}

/// Item entries whose level is decided by which entry they are (one item per level), so nothing rewrites them.
AC_GAME_API void ReserveLevelResolvedItems(std::uint32_t firstEntry, std::uint32_t lastEntry);
[[nodiscard]] AC_GAME_API bool IsLevelResolvedItem(std::uint32_t entry);
}

#endif
