-- Worldforged Shadewell Stabber (item 824384) at Shadewell Spring, Elwynn Forest (COA-6829).
-- rev_20261023_12 keeps only the original Murloc Tool pickup (520066, guid 6900733), but
-- mod-worldforged-pickups 2026_09_23_05_worldforged_map.sql already wiped guids 6900001-6939999 after
-- 2026_09_16_00_worldforged_pickups.sql had placed it, so the spring was left with no pickup at all.
-- Restore the original pickup as 2026_09_16_00 placed it.
DELETE FROM `gameobject` WHERE `guid` = 6900733;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `position_x`, `position_y`,
    `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`,
    `state`, `ScriptName`, `Comment`) VALUES
(6900733, 520066, 0, 0, 0, 1, 1, -8545.820, -514.623, 146.444, 5.21315, 0, 0, 0.509857, -0.860259, 0, 0, 1,
    'worldforged_pickup', 'AscensionWorldforged Murloc Tool | Elwynn | Shadewell Spring');
