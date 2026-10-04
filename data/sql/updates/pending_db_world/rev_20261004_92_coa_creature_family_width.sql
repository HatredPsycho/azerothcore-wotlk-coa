-- creature_template.family holds a CreatureFamily id, and the core reads it as uint32
-- (CreatureData.h). The column is tinyint, so it stops at 127 while Ascension numbers its own
-- families far above that: the Hero-learnable summons of rev_20261004_20 carry 301, 304 and 80486.
-- On a server in strict mode the whole insert is refused, and on one without it the values are
-- silently truncated to 127, which puts those creatures in the wrong family instead.
ALTER TABLE `creature_template` MODIFY `family` int unsigned NOT NULL DEFAULT 0;
