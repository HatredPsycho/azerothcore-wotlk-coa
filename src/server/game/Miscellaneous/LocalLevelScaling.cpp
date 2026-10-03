/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU GPL.
 */

#include "LocalLevelScaling.h"
#include "Creature.h"
#include "CreatureData.h"
#include "QuestDef.h"

namespace LocalLevelScaling
{
    std::int32_t GetEffectiveQuestBaseLevel(Quest const* quest)
    {
        if (!quest)
            return 0;

        QuestBaseLevelResolver const owner = QuestBaseLevelOwner.load(std::memory_order_relaxed);
        if (owner)
            return owner(quest);

        return quest->GetQuestLevel();
    }

    std::uint32_t GetEffectiveQuestMinLevel(Quest const* quest)
    {
        if (!quest)
            return 0;

        QuestMinLevelResolver const owner = QuestMinLevelOwner.load(std::memory_order_relaxed);
        if (owner)
            return owner(quest);

        return quest->GetMinLevel();
    }

    std::uint8_t GetEffectiveCreatureBaseLevel(CreatureTemplate const* cinfo, Creature const* creature)
    {
        if (!cinfo)
            return 1;

        CreatureBaseLevelResolver const owner = CreatureBaseLevelOwner.load(std::memory_order_relaxed);
        if (owner)
            return owner(cinfo, creature);

        return cinfo->maxlevel;
    }

    float GetEffectiveCreatureArmor(CreatureTemplate const* cinfo, Creature const* creature, float generatedArmor)
    {
        if (!cinfo)
            return generatedArmor;

        CreatureArmorResolver const owner = CreatureArmorOwner.load(std::memory_order_relaxed);
        if (owner)
            return owner(cinfo, creature, generatedArmor);

        return generatedArmor;
    }

    std::uint32_t GetEffectiveQuestMoneyMaxLevel(Quest const* quest, std::uint32_t defaultRewardMoney)
    {
        if (!quest)
            return defaultRewardMoney;

        QuestMoneyMaxLevelResolver const owner = QuestMoneyMaxLevelOwner.load(std::memory_order_relaxed);
        if (owner)
            return owner(quest, defaultRewardMoney);

        return defaultRewardMoney;
    }

    std::uint8_t GetEffectiveKillContentLevel(Player const* player, Unit const* victim,
        std::uint8_t defaultContentLevel)
    {
        KillContentLevelResolver const owner = KillContentLevelOwner.load(std::memory_order_relaxed);
        if (owner)
            return owner(player, victim, defaultContentLevel);

        return defaultContentLevel;
    }

    float GetEffectiveQuestRewardRate(Player const* player, Quest const* quest, float defaultRate)
    {
        QuestRewardRateResolver const owner = QuestRewardRateOwner.load(std::memory_order_relaxed);
        if (owner)
            return owner(player, quest, defaultRate);

        return defaultRate;
    }

    std::int32_t GetEffectiveItemEffectValue(std::uint32_t itemEntry, std::int32_t value)
    {
        if (!itemEntry || !value)
            return value;

        ItemEffectValueResolver const owner = ItemEffectValueOwner.load(std::memory_order_relaxed);
        if (owner)
            return owner(itemEntry, value);

        return value;
    }
}
