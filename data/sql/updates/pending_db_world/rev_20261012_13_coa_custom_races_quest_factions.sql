-- CoA Custom races: Thin Human (32, race mask bit 31) takes the quests of the Human, which the package never gave
-- it. Pandaren keep only the quests of their own faction: the package put both Pandaren (20 Horde, 29 Alliance) into
-- the quests of both factions. Illidari Night Elf (61) shares the bit of race 29 and follows it to the Alliance.
UPDATE `quest_template` SET `AllowableRaces` = `AllowableRaces` | 2147483648 WHERE (`AllowableRaces` & 1) <> 0;
UPDATE `quest_template` SET `AllowableRaces` = `AllowableRaces` & ~268435456
WHERE (`AllowableRaces` & 1101) = 0 AND (`AllowableRaces` & 690) <> 0;
UPDATE `quest_template` SET `AllowableRaces` = `AllowableRaces` & ~524288
WHERE (`AllowableRaces` & 690) = 0 AND (`AllowableRaces` & 1101) <> 0;
