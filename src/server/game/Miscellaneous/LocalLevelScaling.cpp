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
}
