-- creature_template.family holds a CreatureFamily id, and the core reads it as uint32
-- (CreatureData.h). The column is tinyint, so it stops at 127 while Ascension numbers its own
-- families far above that: the Hero-learnable summons of rev_20261004_20 carry 301 and 304.
-- On a server in strict mode the whole insert is refused, and on one without it the values are
-- silently truncated to 127, which puts those creatures in the wrong family instead.
ALTER TABLE `creature_template` MODIFY `family` int unsigned NOT NULL DEFAULT 0;

-- A realm that already ran rev_20261004_20 without strict mode wrote 127 for those two, and its
-- updater will not offer that file again. Put them back where they belong; on a realm that has not
-- seen it yet, or that applied it after this file, no row matches.
UPDATE `creature_template` SET `family` = 304 WHERE `entry` = 300182 AND `family` = 127;
UPDATE `creature_template` SET `family` = 301 WHERE `entry` = 300183 AND `family` = 127;
