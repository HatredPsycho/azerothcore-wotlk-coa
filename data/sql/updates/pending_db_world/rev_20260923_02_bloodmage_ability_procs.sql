-- Two Bloodmage passives that name the abilities which should trigger them, but whose Spell.dbc
-- records carry ProcFlags 0 and an empty EffectSpellClassMask: SpellMgr::LoadSpellProcs skips them
-- ("Skip if no proc flags in DBC"), so the aura 42 effect never fires. Neither has a `spell_dbc`
-- override or a `spell_proc` row in the world database. The masks below are authored here, the way
-- the class audit authored the Screech of the Darkwing row.
--
-- 300588 Bloodshards -> 681074 (aura 3 bleed), ProcChance 100: "Damage dealt by Claw Sweep and
-- Sanguine Rupture". Family 26, SpellFamilyMask2 0x4000 | 0x80000000: a full Spell.dbc scan shows
-- those two bits belong to the eight Claw Sweep ranks and the eight Sanguine Rupture records and
-- to nothing else.
--
-- 532712 Easy Prey -> 801958 Call of the Shadow Pack, ProcChance 10: "Bloodmoon Blast now has a
-- $h% chance and Bloodfang Bite a $h% chance". Family 26, SpellFamilyMask1 0x2000 | 0x800000: the
-- first bit belongs to the ten Bloodmoon Blast ranks, the second to the ten Bloodfang Bite ranks
-- and to Crimson Maw (803388), which shares Bloodfang Bite's bit in this data set and therefore
-- also triggers the pack. Screech of the Darkwing (300260) already carries its own row with a one
-- second cooldown, so its echo is not repeated here.
--
-- ProcFlags 65552 is DONE_SPELL_MELEE_DMG_CLASS 0x10 | DONE_SPELL_MAGIC_DMG_CLASS_NEG 0x10000,
-- the two damage classes these four abilities use. Chance 0 defers to each record's own ProcChance.
DELETE FROM `spell_proc` WHERE `SpellId` IN (300588, 532712);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(300588, 0, 26, 0, 0, 2147500032, 65552, 1, 2, 0, 0, 0, 0, 0, 0, 0),
(532712, 0, 26, 0, 8396800, 0, 65552, 1, 2, 0, 0, 0, 0, 0, 0, 0);
