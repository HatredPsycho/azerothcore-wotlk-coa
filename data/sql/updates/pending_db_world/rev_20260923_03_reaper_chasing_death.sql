-- Chasing Death (707455) never fired. Its Spell.dbc record is a SPELL_AURA_PROC_TRIGGER_SPELL on
-- 807418 with ProcFlags 0 and ProcChance 100, so SpellMgr::LoadSpellProcs skips it ("Skip if no proc
-- flags in DBC") and no event ever reaches the aura. The world database holds neither a `spell_proc`
-- row nor a `spell_dbc` override for it.
--
-- Its tooltip carries two conditions: "Deathchaser used against a target below 20% health will now
-- reapply itself". Both already exist as scripts, and every DoCheckProc of a spell has to pass, so
-- binding both is the whole condition and no new code is needed:
--   * spell_ascension_reaper_talent_proc asks whether the spell was one the talent names. The rule
--     for 707455 lists the ten Deathchaser records of family 36; the three further records carrying
--     that name are helpers (the proc aura 560352 and the resets 681033 and 712288), not casts.
--   * aura_ascension_jailers_call is a DoCheckProc with no side effects: the victim must be alive,
--     another unit and below 20% health.
--
-- ProcFlags 69652 is DONE_MELEE_AUTO_ATTACK 0x4 | DONE_SPELL_MELEE_DMG_CLASS 0x10 |
-- DONE_SPELL_NONE_DMG_CLASS_NEG 0x1000 | DONE_SPELL_MAGIC_DMG_CLASS_NEG 0x10000, the flags the other
-- "damage dealt" talents of this class use. The trigger is written for an enemy and a done-damage
-- proc hands it the victim.
DELETE FROM `spell_proc` WHERE `SpellId` = 707455;
INSERT INTO `spell_proc` (`SpellId`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `AttributesMask`, `DisableEffectsMask`, `ProcsPerMinute`, `Chance`, `Cooldown`, `Charges`) VALUES
(707455, 0, 0, 0, 0, 0, 69652, 1, 2, 0, 0, 0, 0, 100, 0, 0);

DELETE FROM `spell_script_names` WHERE `spell_id` = 707455
    AND `ScriptName` IN ('spell_ascension_reaper_talent_proc', 'aura_ascension_jailers_call');
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(707455, 'spell_ascension_reaper_talent_proc'),
(707455, 'aura_ascension_jailers_call');
