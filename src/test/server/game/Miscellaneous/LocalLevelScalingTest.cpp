/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "LocalLevelScaling.h"
#include "gtest/gtest.h"

using LocalLevelScaling::ScaleCreatureLevelForViewer;
using LocalLevelScaling::ScaleDungeonCreatureLevelForViewer;

TEST(LocalLevelScalingTest, OpenWorldCreatureAboveViewerKeepsItsLevel)
{
    EXPECT_EQ(ScaleCreatureLevelForViewer(38, 20, 3), 38);
}

TEST(LocalLevelScalingTest, CathedralCreatureComesDownToLowLevelViewer)
{
    EXPECT_EQ(ScaleDungeonCreatureLevelForViewer(36, 20, 3), 23);
    EXPECT_EQ(ScaleDungeonCreatureLevelForViewer(38, 22, 3), 25);
    EXPECT_EQ(ScaleDungeonCreatureLevelForViewer(38, 33, 3), 36);
}

TEST(LocalLevelScalingTest, DungeonCreatureInsideViewerBandKeepsItsLevel)
{
    EXPECT_EQ(ScaleDungeonCreatureLevelForViewer(34, 33, 3), 34);
    EXPECT_EQ(ScaleDungeonCreatureLevelForViewer(30, 33, 3), 30);
}

TEST(LocalLevelScalingTest, DungeonCreatureBelowViewerIsStillLifted)
{
    EXPECT_EQ(ScaleDungeonCreatureLevelForViewer(18, 40, 3), 37);
}

TEST(LocalLevelScalingTest, DungeonCeilingFollowsTheOffset)
{
    EXPECT_EQ(ScaleDungeonCreatureLevelForViewer(40, 20, 0), 20);
    EXPECT_EQ(ScaleDungeonCreatureLevelForViewer(40, 20, 5), 25);
    EXPECT_EQ(ScaleDungeonCreatureLevelForViewer(63, 60, 3), 63);
}

TEST(LocalLevelScalingTest, AbilityRequirementKeepsItsAuthoredLevelWithoutAnOwner)
{
    LocalLevelScaling::AbilityRequiredLevelOwner.store(nullptr, std::memory_order_relaxed);
    EXPECT_EQ(LocalLevelScaling::GetEffectiveAbilityRequiredLevel(80), 80);
}

TEST(LocalLevelScalingTest, AbilityRequirementFollowsTheInstalledOwner)
{
    LocalLevelScaling::AbilityRequiredLevelOwner.store(
        [](std::uint8_t requiredLevel) -> std::uint8_t
        {
            return requiredLevel > 60 ? 60 : requiredLevel;
        }, std::memory_order_relaxed);

    EXPECT_EQ(LocalLevelScaling::GetEffectiveAbilityRequiredLevel(80), 60);
    EXPECT_EQ(LocalLevelScaling::GetEffectiveAbilityRequiredLevel(40), 40);

    LocalLevelScaling::AbilityRequiredLevelOwner.store(nullptr, std::memory_order_relaxed);
}

TEST(LocalLevelScalingTest, AreaContentKeepsItsAuthoredLevelWithoutAnOwner)
{
    LocalLevelScaling::AreaContentLevelOwner.store(nullptr, std::memory_order_relaxed);
    EXPECT_EQ(LocalLevelScaling::GetEffectiveAreaContentLevel(495, 571, 70), 70);
}

TEST(LocalLevelScalingTest, AreaContentAsksTheOwnerWithItsAreaAndMap)
{
    LocalLevelScaling::AreaContentLevelOwner.store(
        [](std::uint32_t /*areaId*/, std::uint32_t mapId, std::uint8_t authoredLevel) -> std::uint8_t
        {
            return mapId == 571 ? authoredLevel - 15 : authoredLevel;
        }, std::memory_order_relaxed);

    EXPECT_EQ(LocalLevelScaling::GetEffectiveAreaContentLevel(495, 571, 70), 55);
    EXPECT_EQ(LocalLevelScaling::GetEffectiveAreaContentLevel(12, 0, 10), 10);

    LocalLevelScaling::AreaContentLevelOwner.store(nullptr, std::memory_order_relaxed);
}

TEST(LocalLevelScalingTest, ReservedItemRangeIsRecognisedAndItsNeighboursAreNot)
{
    LocalLevelScaling::ReserveLevelResolvedItems(9700000, 9706399);

    EXPECT_TRUE(LocalLevelScaling::IsLevelResolvedItem(9700000));
    EXPECT_TRUE(LocalLevelScaling::IsLevelResolvedItem(9703160));
    EXPECT_TRUE(LocalLevelScaling::IsLevelResolvedItem(9706399));
    EXPECT_FALSE(LocalLevelScaling::IsLevelResolvedItem(9699999));
    EXPECT_FALSE(LocalLevelScaling::IsLevelResolvedItem(9706400));
    EXPECT_FALSE(LocalLevelScaling::IsLevelResolvedItem(0));
}

TEST(LocalLevelScalingTest, ReservingAnEmptyRangeReservesNothing)
{
    LocalLevelScaling::ReserveLevelResolvedItems(500, 499);
    LocalLevelScaling::ReserveLevelResolvedItems(0, 100);

    EXPECT_FALSE(LocalLevelScaling::IsLevelResolvedItem(500));
    EXPECT_FALSE(LocalLevelScaling::IsLevelResolvedItem(50));
}
