-- rev_20261024_00 republishes the Northshire class-chain spawns from their first authoring (rev_20261007_20) and so
-- undoes the corrections made since: the A Quiet Life visit spot on the bank (rev_20261017_40), the single Lost
-- Pendant on the Defias camp crate with the trail pendant removed (rev_20261012_41, rev_20261017_40), and Peace
-- Through Strength's miner standing (rev_20261020_14). Restore those after it.
DELETE FROM `gameobject` WHERE `guid` IN (7912102, 7912105, 7912106);
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `position_x`, `position_y`,
    `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`,
    `state`, `ScriptName`, `Comment`) VALUES
(7912102, 9301105, 0, 0, 0, 1, 1, -8739.86, -413.297, 82.6027, 2.97193, 0, 0, 0.996404, 0.0847297, 300, 100, 1, '',
    'CoA Northshire class chain: the bank in front of the Northshire falls, where a player on A Quiet Life sees the statue without the falling water across it'),
(7912105, 9301103, 0, 0, 0, 1, 1, -8956.38, -435.027, 65.3559, 0.00392888, -0.0725746, -0.031781, -0.000347103,
    0.996856, 60, 100, 1, '',
    'CoA Northshire class chain: on the crate at the Defias camp, where the chaplain fled and dropped it; the valley''s single Lost Pendant for 200058');

UPDATE `creature_addon` SET `bytes1` = 0 WHERE `guid` = 9003121 AND `path_id` = 0;
