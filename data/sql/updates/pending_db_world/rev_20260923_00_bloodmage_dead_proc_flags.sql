-- Three Bloodmage passives whose trigger spell is wired correctly but which can never fire:
-- Spell.dbc gives each record ProcFlags 0, so SpellMgr::LoadSpellProcs skips it ("Skip if no proc
-- flags in DBC") and Aura::GetProcEffectMask returns a zero mask. Verified against the client
-- Spell.dbc of this fork and against the live world database, which holds no `spell_proc` row and
-- no `spell_dbc` override for any of the three.
--
-- 704639 Lingering Blood: aura 42 on 800988, ProcChance 30. "Your direct damage dealt has a $h%
-- chance to increase the chance for an enemy to be critically struck." ProcFlags 65556 is
-- DONE_MELEE_AUTO_ATTACK 0x4 | DONE_SPELL_MELEE_DMG_CLASS 0x10 | DONE_SPELL_MAGIC_DMG_CLASS_NEG
-- 0x10000, the three ways this class deals direct damage. Chance 0 defers to the record's own 30.
--
-- 704168 Hunger: aura 42 on 504189, ProcChance 100. "Dealing damage now increases the damage of
-- your next Bloodbolt or Bloodfang Bite ... Can only occur once per 0.5 sec", hence Cooldown 500
-- (milliseconds).
--
-- 705739 Insatiable Appetite: aura 42 on 520494, ProcChance 100. "Direct damaging critical strikes
-- now heal ...", so HitMask 2 (CRITICAL) restricts it to crits.
--
-- SpellPhaseMask 2 (HIT) matches the other Bloodmage rows added by the class audit.
-- SpellFamilyName and the masks stay 0 because no tooltip names a specific ability.
DELETE FROM `spell_proc` WHERE `SpellId` IN (704639, 704168, 705739);
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(704639, 0, 0, 0, 0, 0, 65556, 1, 2, 0, 0, 0, 0, 0, 0, 0),
(704168, 0, 0, 0, 0, 0, 65556, 1, 2, 0, 0, 0, 0, 0, 500, 0),
(705739, 0, 0, 0, 0, 0, 65556, 1, 2, 2, 0, 0, 0, 0, 0, 0);
