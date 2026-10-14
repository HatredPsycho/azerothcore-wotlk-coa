/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU GPL.
 */

#ifndef AC_ENCOUNTER_MECHANIC_H
#define AC_ENCOUNTER_MECHANIC_H

#include <cstdint>

/// What kind of value an encounter is asking about, when it asks whether the number it was written
/// with is still the right one for the group in front of it.
///
/// A boss script is written for the group size the encounter was designed for. Where that number is
/// load-bearing - how many targets a mechanic picks, how many adds a wave brings, how many players
/// must stand on something - a smaller group meets a wall that has nothing to do with how hard the
/// fight is. The script asks instead of assuming, and whoever owns scaling on this realm answers.
///
/// The values are part of the question: a module answering them keeps its own copy and must not
/// renumber. Append at the end.
enum class EncounterMechanic : std::uint8_t
{
    TargetCount         = 0,
    AddCount            = 1,
    RequiredPlayers     = 2,
    RequiredInteractors = 3,
    ObjectiveCount      = 4,
    VehicleCount        = 5,
    WaveSize            = 6,
    StackThreshold      = 7,
    SplitDivisor        = 8,
    TimerMs             = 9,
    FailThreshold       = 10,
    ProximityDistance   = 11,
    HealingContribution = 12
};

#endif
