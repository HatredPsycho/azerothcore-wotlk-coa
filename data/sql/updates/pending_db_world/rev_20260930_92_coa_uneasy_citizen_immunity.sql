-- "Those Who Fell" (1660018) has the player use the Unremarkable Stone on an Uneasy Citizen, which casts
-- Distilling Spiritual Unrest (256708). With UNIT_FLAG_IMMUNE_TO_PC (0x100) set, every cast failed with
-- "Invalid target" on the realm; clearing the flag on a live citizen made the same cast succeed, and setting
-- it again brought the failure back. UNIT_FLAG_IMMUNE_TO_NPC (0x200) stays, so the citizens keep out of
-- creature combat.
UPDATE `creature_template` SET `unit_flags` = 512 WHERE `entry` = 161791;
