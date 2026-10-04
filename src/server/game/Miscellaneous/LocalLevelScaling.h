/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU GPL.
 */

#ifndef AC_LOCAL_LEVEL_SCALING_H
#define AC_LOCAL_LEVEL_SCALING_H

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <optional>

class Creature;
class Unit;
class CreatureTemplate;
class Player;
class Quest;
struct ItemTemplate;

namespace LocalLevelScaling
{
/// A module that owns the level a quest or a creature is authored at installs itself here, and the
/// core asks through these instead of reading the template directly. Nothing is installed by default,
/// so both answers are the authored value.
using QuestBaseLevelResolver = std::int32_t (*)(Quest const*);
inline std::atomic<QuestBaseLevelResolver> QuestBaseLevelOwner{nullptr};

std::int32_t GetEffectiveQuestBaseLevel(Quest const* quest);

using QuestMinLevelResolver = std::uint32_t (*)(Quest const*);
inline std::atomic<QuestMinLevelResolver> QuestMinLevelOwner{nullptr};

std::uint32_t GetEffectiveQuestMinLevel(Quest const* quest);

/// The three questions below belong to whoever owns a realm's progression. This core answers them
/// itself - a quest is lifted to the character's level and its reward follows the game's own
/// tables, discounted by distance - and stands down entirely while a content scaling module is in
/// charge, the same way the raid difficulty module stands down over boss health. Two answers to
/// one question cannot both be right, and they would otherwise be applied one after the other.
using QuestMoneyMaxLevelResolver = std::uint32_t (*)(Quest const*, std::uint32_t);
inline std::atomic<QuestMoneyMaxLevelResolver> QuestMoneyMaxLevelOwner{nullptr};

std::uint32_t GetEffectiveQuestMoneyMaxLevel(Quest const* quest, std::uint32_t defaultRewardMoney);

using KillContentLevelResolver = std::uint8_t (*)(Player const*, Unit const*, std::uint8_t);
inline std::atomic<KillContentLevelResolver> KillContentLevelOwner{nullptr};

std::uint8_t GetEffectiveKillContentLevel(Player const* player, Unit const* victim,
    std::uint8_t defaultContentLevel);

using QuestRewardRateResolver = float (*)(Player const*, Quest const*, float);
inline std::atomic<QuestRewardRateResolver> QuestRewardRateOwner{nullptr};

float GetEffectiveQuestRewardRate(Player const* player, Quest const* quest, float defaultRate);

/// The flat part of what a spell cast from an item does - the damage or healing written into the
/// spell before any coefficient. A module that rewrites an item's statistics for this realm's level
/// band leaves its spells alone, because they belong to the spell and not to the item, and a weapon
/// whose stats were cut to a third keeps a proc written for the level it came from.
///
/// Only the flat part is asked about. What a coefficient adds is already in proportion: it is
/// computed from attack power or spell power, which come from the statistics that were cut. Asking
/// after the coefficient would cut the same thing twice.
using ItemEffectValueResolver = std::int32_t (*)(std::uint32_t, std::int32_t);
inline std::atomic<ItemEffectValueResolver> ItemEffectValueOwner{nullptr};

std::int32_t GetEffectiveItemEffectValue(std::uint32_t itemEntry, std::int32_t value);

/// The flat amount an enchantment, a gem or a socket bonus adds to the item it sits on.
///
/// An enchantment belongs to no item of its own: the same one goes on anything that will take it,
/// so the item it was put on is what decides. That also makes a socket bonus, which is written in
/// the host's own template, answer the same way as the gem that unlocks it.
///
/// Amounts an enchantment's spell carries are not asked about here; those travel with the item
/// already and are cut where every other spell value is.
using ItemEnchantmentAmountResolver = std::uint32_t (*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
inline std::atomic<ItemEnchantmentAmountResolver> ItemEnchantmentAmountOwner{nullptr};

std::uint32_t GetEffectiveItemEnchantmentAmount(std::uint32_t hostItemEntry, std::uint32_t enchantmentType,
    std::uint32_t statType, std::uint32_t amount);

/// The level an enchantment asks of the character wearing it.
///
/// Rewriting an item moves what it asks of its wearer, and an enchantment put into it has to move
/// with it or it stops applying: a gem written for level 80 in a weapon now asking for 60 is read as
/// out of reach and silently contributes nothing, while its own line still promises it.
using EnchantmentRequiredLevelResolver = std::uint32_t (*)(std::uint32_t);
inline std::atomic<EnchantmentRequiredLevelResolver> EnchantmentRequiredLevelOwner{nullptr};

std::uint32_t GetEffectiveEnchantmentRequiredLevel(std::uint32_t requiredLevel);

/// The level a rank of an ability asks of the character who learns it, whether that rank is sold by
/// the Book of Ascension, granted by progression, taught by an item or picked as a talent.
///
/// A rank is written against the level its own progression put it at, the same way a quest or a
/// creature is, so on a realm that maps content onto other levels it has to move with it. Left
/// alone, the top of every chain falls off the realm: the book lists the row and refuses the
/// purchase, progression skips it, and the character's line ends at whichever rank was authored at
/// or below the cap. The authored level stands with nothing installed, and an owner that cannot
/// place a rank answers with the authored level rather than putting it further out of reach.
using AbilityRequiredLevelResolver = std::uint8_t (*)(std::uint8_t);
inline std::atomic<AbilityRequiredLevelResolver> AbilityRequiredLevelOwner{nullptr};

inline std::uint8_t GetEffectiveAbilityRequiredLevel(std::uint8_t requiredLevel)
{
    AbilityRequiredLevelResolver const owner = AbilityRequiredLevelOwner.load(std::memory_order_relaxed);
    return owner ? owner(requiredLevel) : requiredLevel;
}

using CreatureBaseLevelResolver = std::uint8_t (*)(CreatureTemplate const*, Creature const*);
inline std::atomic<CreatureBaseLevelResolver> CreatureBaseLevelOwner{nullptr};

std::uint8_t GetEffectiveCreatureBaseLevel(CreatureTemplate const* cinfo, Creature const* creature = nullptr);

/// The armor a creature wears. `Creature::UpdateEntry` regenerates armor from the template *after*
/// `SelectLevel` has run, so a module that set it from the creature hook loses it again a few lines
/// later. The core asks here instead of writing the generated value straight back; with nothing
/// installed that generated value is the answer, so the stock path is unchanged.
using CreatureArmorResolver = float (*)(CreatureTemplate const*, Creature const*, float);
inline std::atomic<CreatureArmorResolver> CreatureArmorOwner{nullptr};

float GetEffectiveCreatureArmor(CreatureTemplate const* cinfo, Creature const* creature, float generatedArmor);

inline std::atomic<bool> QuestEnabled{false};
/// Set by a content scaling module while it is in charge, so systems that scale the same thing by
/// themselves can stand down.
inline std::atomic<bool> ContentScalingActive{false};
inline std::atomic<std::uint8_t> CreatureOffset{3};

/// How much of the level-scaled reward a quest keeps when it is lifted from its own level to the
/// player's, at the extreme end of the range: the rest is lost in proportion to how much of that
/// range the quest spans, so a quest at the player's own level always keeps all of it. Both default
/// to 100, which is no discount at all, and both are set from a module's config.
///
/// Money carries a discount by default: the reward class (the tier) says nothing about level - the
/// median tier is the same in every level band - so paid at full strength a rich quest from a
/// starting zone would pay what a rich level-appropriate one pays while being trivial to complete.
/// A discount keeps the old zones worth playing without letting the easy content become the
/// profitable content, and because the discount only ever scales *down* from the price the class
/// pays at the player's level, old content can never out-pay level-appropriate content.
inline std::atomic<std::uint32_t> QuestMoneyKeepSharePercent{100};

/// Experience is left whole by default. The promise scaling makes is that scaled content always
/// awards experience, and levelling through the old zones is exactly what the system is for - a
/// discount here would put the dead zones back. Lower it to bound how much of a character's
/// progress can come from content far below them.
inline std::atomic<std::uint32_t> QuestXpKeepSharePercent{100};

/// The realm switches above are the default. A character can have a choice of their own, and
/// whichever module owns that choice installs a resolver here; the realm switches keep the final
/// say, because the resolver is only asked while they are on. With no resolver installed, or when
/// it has no opinion about the character, the realm-wide behaviour stands - which is what a
/// resolver returning true means.
using QuestScalingResolver = bool (*)(Player const*);
inline std::atomic<QuestScalingResolver> QuestScalingOwner{nullptr};

/// The character's own answer, with no realm switch in front of it: whether this character asked
/// for open-world scaling. The realm switches ask *whether* a character's choice is consulted at
/// all; this asks *what* it is, and is what the per-viewer combat paths resolve.
inline bool ScalingChoiceEnabled(Player const* player)
{
    QuestScalingResolver const owner = QuestScalingOwner.load(std::memory_order_relaxed);
    return owner && owner(player);
}

inline bool QuestScalingEnabled(Player const* player)
{
    if (!QuestEnabled.load(std::memory_order_relaxed))
        return false;

    QuestScalingResolver const owner = QuestScalingOwner.load(std::memory_order_relaxed);
    return !owner || owner(player);
}

/// One character's version of one creature, asked for by the few places in the core that compute a
/// fight and cannot see the viewer's object fields: the armor a blow lands against, today. Returns
/// the armor the viewer's version of the creature wears, or nullopt when the creature is already their
/// version of it.
///
/// The implementation stays with whichever module owns the character's choice, so the level, the
/// pool, the armor and the damage all come out of one place and cannot disagree.
using CreatureViewArmorResolver = std::optional<std::uint32_t> (*)(Player const*, Creature const*);
inline std::atomic<CreatureViewArmorResolver> CreatureViewArmorOwner{nullptr};

inline std::optional<std::uint32_t> ViewArmorFor(Player const* viewer, Creature const* creature)
{
    CreatureViewArmorResolver const owner = CreatureViewArmorOwner.load(std::memory_order_relaxed);
    return owner ? owner(viewer, creature) : std::nullopt;
}

/// The level one character's version of one creature stands at, or zero when that character sees
/// the authored creature.
///
/// This is the lever every level-derived term in a fight hangs off. `Unit::getLevelForTarget` is
/// the function the core asks "how high is this unit, *relative to me*" - spell hit and resistance
/// tables, weapon and defence skill (`GetMaxSkillValueForLevel`, `GetUnitMeleeSkill`, and therefore
/// `GetWeaponSkillValue` / `GetDefenseSkillValue` for a creature), the glancing and crushing tables,
/// stealth detection, aggro radius, and kill experience all read it. It is already virtual and
/// already overridden to give world bosses a target-relative level, which is exactly the shape a
/// per-viewer level needs: with a view installed, one character rolls against the level they are
/// fighting while everybody else rolls against the authored creature, from one definition.
///
/// Zero means "no view", so an unset owner, a character who never chose scaling, or a creature that
/// already is their version of it all keep the stock behaviour.
using CreatureViewLevelResolver = std::uint8_t (*)(Player const*, Creature const*);
inline std::atomic<CreatureViewLevelResolver> CreatureViewLevelOwner{nullptr};

inline std::uint8_t ViewLevelFor(Player const* viewer, Creature const* creature)
{
    CreatureViewLevelResolver const owner = CreatureViewLevelOwner.load(std::memory_order_relaxed);
    return owner ? owner(viewer, creature) : 0;
}

/// The max health one character's version of one creature has, or zero when that character sees the
/// authored creature. For effects worded as a share of "the creature's health", which the character
/// reads off the health bar they are shown.
using CreatureViewMaxHealthResolver = std::uint32_t (*)(Player const*, Creature const*);
inline std::atomic<CreatureViewMaxHealthResolver> CreatureViewMaxHealthOwner{nullptr};

inline std::uint32_t ViewMaxHealthFor(Player const* viewer, Creature const* creature)
{
    CreatureViewMaxHealthResolver const owner = CreatureViewMaxHealthOwner.load(std::memory_order_relaxed);
    return owner ? owner(viewer, creature) : 0;
}

/// The level a *viewer* is shown for a creature: never lowered, and lifted to the viewer's level minus
/// the offset. A view is told to nobody else, so it has no ceiling: the creature in front of a
/// character comes all the way up to that character's band, which is the whole point of the feature
/// (a starting-zone creature stays relevant to the character standing in front of it).
inline std::uint8_t ScaleCreatureLevelForViewer(std::uint8_t originalLevel, std::uint8_t playerLevel,
    std::uint8_t offset = 3)
{
    std::uint8_t floor = playerLevel > offset ? playerLevel - offset : 1;
    return std::max(originalLevel, floor);
}

/// The viewer's rule inside a normal five-player dungeon, which also brings a creature down.
///
/// The dungeon finder admits a group to a classic dungeon from well below its authored level
/// (Scarlet Monastery - Cathedral from 20 against creatures of 36-40), so a dungeon creature is held
/// inside the viewer's band on both sides.
inline std::uint8_t ScaleDungeonCreatureLevelForViewer(std::uint8_t originalLevel, std::uint8_t playerLevel,
    std::uint8_t offset = 3)
{
    std::uint32_t const ceiling = std::uint32_t(playerLevel) + offset;
    std::uint8_t const lifted = ScaleCreatureLevelForViewer(originalLevel, playerLevel, offset);
    return static_cast<std::uint8_t>(std::min<std::uint32_t>(lifted, ceiling));
}

inline std::uint8_t ScaleQuestLevel(std::int32_t originalLevel, std::uint8_t playerLevel)
{
    if (originalLevel <= 0)
        return playerLevel;
    std::uint8_t questLevel = static_cast<std::uint8_t>(std::min<std::int32_t>(originalLevel, UINT8_MAX));
    return std::max(questLevel, playerLevel);
}

/// The share of a scaled reward a quest keeps: `floorPercent` at the far end of the level range,
/// growing to 100% at the player's own level, in proportion to how much of that range the quest
/// spans. A quest at or above the player's level - the case where scaling changes nothing - always
/// keeps the whole reward.
inline std::uint32_t RewardKeepPercent(std::uint32_t floorPercent, std::int32_t questLevel,
    std::uint8_t effectiveLevel)
{
    if (floorPercent >= 100 || questLevel <= 0 || effectiveLevel == 0 ||
        std::uint32_t(questLevel) >= effectiveLevel)
        return 100;

    std::uint32_t const sharePercent = std::uint32_t(questLevel) * 100 / effectiveLevel;
    return floorPercent + (100 - floorPercent) * sharePercent / 100;
}

/// Item templates that are not in the world database: lifted copies of authored items, made when
/// scaled content drops or rewards an item for a character above the content's level. Items already
/// handed out keep pointing at their copy, so the owner serves every copy it ever made even while new
/// lifts are switched off - an inventory whose template went missing would be deleted on login.
using ScaledItemTemplateResolver = ItemTemplate const* (*)(std::uint32_t entry);
inline std::atomic<ScaledItemTemplateResolver> ScaledItemTemplateOwner{nullptr};

inline ItemTemplate const* ScaledItemTemplateFor(std::uint32_t entry)
{
    ScaledItemTemplateResolver const owner = ScaledItemTemplateOwner.load(std::memory_order_relaxed);
    return owner ? owner(entry) : nullptr;
}

/// The item one character is offered and given for a quest's reward slot: the authored item, or a
/// copy lifted by as many levels as the quest itself is lifted for that character. The offer, the
/// query response and the reward all ask here, so what is shown is what is received.
using QuestRewardItemResolver = std::uint32_t (*)(Player const*, std::uint32_t itemId, std::int32_t questLevel);
inline std::atomic<QuestRewardItemResolver> QuestRewardItemOwner{nullptr};

inline std::uint32_t QuestRewardItemFor(Player const* player, std::uint32_t itemId, std::int32_t questLevel)
{
    QuestRewardItemResolver const owner = QuestRewardItemOwner.load(std::memory_order_relaxed);
    return owner && itemId ? owner(player, itemId, questLevel) : itemId;
}
}

#endif
