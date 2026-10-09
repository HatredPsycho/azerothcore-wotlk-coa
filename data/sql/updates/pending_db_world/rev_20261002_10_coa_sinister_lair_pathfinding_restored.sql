-- Takes back rev_20260930_93. The Sinister Lair creatures were given
-- CREATURE_FLAG_EXTRA_IGNORE_PATHFINDING because the server's mmaps had solid ground at z 56 to 81
-- over the whole cave: the lair is CoA client geometry, and the extractors read only Blizzard's
-- archives, so every extraction produced stock terrain. ".mmap loc" inside the cave reported an
-- invalid poly and ".mmap path" returned PATHFIND_NOPATH, which evaded the creature after five
-- seconds.
-- The extractors now read the client's lettered patches (#6124), and mmaps built from that terrain
-- carry the cave: ".mmap loc" there reports Dt [9,23], the tile it should. With the navmesh in place
-- the flag only costs what it always cost - the creatures walk straight at their target instead of
-- around the pillars - so it goes.
UPDATE `creature_template` SET `flags_extra` = `flags_extra` & ~0x20000000
    WHERE `entry` IN (161793, 161794, 161795, 161833);
