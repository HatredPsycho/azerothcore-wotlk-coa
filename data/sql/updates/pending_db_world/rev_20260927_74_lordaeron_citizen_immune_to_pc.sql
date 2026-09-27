-- Lordaeron Citizen (3617) is invisible ambience: 32 spawns in the Ruins of Lordaeron carrying
-- 34426 Greater Invisibility, with no loot, quest, game event, reputation or script. It is the only
-- permanently invisible creature there and, unlike the Invisible Stalkers that share the location
-- (31576 and 31577, both unit_flags 256), it lacks UNIT_FLAG_IMMUNE_TO_PC. Area spells pass
-- ignoreStealth to Unit::_IsValidAttackTarget, which skips the invisibility check, so player and
-- pet area damage reached these citizens, gave them threat and let them kill characters who could
-- neither see nor fight back against them.
UPDATE `creature_template` SET `unit_flags` = `unit_flags` | 0x100 WHERE `entry` = 3617;
