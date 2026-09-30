-- The Sinister Lair is CoA client geometry. The 3.3.5a terrain the server's mmaps are built from has solid
-- ground at z 56 to 81 over the whole cave, where every Blizzard spawn in the area sits, so the lair's own
-- creatures at z -51 to 45 are below it. ".mmap loc" inside the cave reports an invalid poly although the
-- tile 13239.mmtile is loaded, and ".mmap path" from a Sinister Snake to a player returns PATHFIND_NOPATH,
-- which makes the creature evade after five seconds. CREATURE_FLAG_EXTRA_IGNORE_PATHFINDING sends them
-- straight at their target instead of over the navmesh.
UPDATE `creature_template` SET `flags_extra` = `flags_extra` | 0x20000000
    WHERE `entry` IN (161793, 161794, 161795, 161833);
