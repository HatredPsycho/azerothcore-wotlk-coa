-- Snowgrave (64627) shares the world boss kill rewards with Atal'zul, which skip players already bound permanently.
-- With CREATURE_FLAG_EXTRA_INSTANCE_BIND (flags_extra 1) the core binds every player as he dies, before the rewards
-- run, so nobody received the runes, tokens or treasure. The rewards bind the players themselves.
UPDATE `creature_template` SET `flags_extra` = `flags_extra` & ~1 WHERE `entry` = 64627;
