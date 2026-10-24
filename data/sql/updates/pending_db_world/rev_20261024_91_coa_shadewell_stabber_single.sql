-- Worldforged Shadewell Stabber (item 824384) at Shadewell Spring, Elwynn Forest (COA-6829).
-- rev_20261023_13 restored the original Murloc Tool pickup (guid 6900733) after rev_20261023_12 had left the spring
-- without one; rev_20261024_90 now places upstream's own pickup there (guid 6975024, 3 yd away). Keep only upstream's.
DELETE FROM `gameobject` WHERE `guid` = 6900733 AND `id` = 520066;
