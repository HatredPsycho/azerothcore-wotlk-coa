-- Four Bloodmage tooltips describe what happens when an aura runs out, which no DBC field and no
-- `spell_proc` row can express. Their carriers are plain dummy records: Vampiric Hunger (802316)
-- and Aortic Aegis (704637) hold aura 4 with BasePoints -1 and nothing else, and Atherann's
-- Anguish (680680) and Infuse (681403) carry aura 4 with their burst record on
-- EffectTriggerSpell[0] (680681 and 681404), both SPELL_EFFECT_SCHOOL_DAMAGE with BasePoints -1/0
-- so the damage is supplied by the caller. The listeners live in src/server/coa.
--
-- Blood Veil 504263 and 572279 carry the absorb on EFFECT_0 (aura 69), Darkfallen Lament 680828 a
-- periodic trigger (aura 23); the scripts hook AfterEffectRemove on those effects and act only on
-- AURA_REMOVE_BY_EXPIRE. A dispelled plague mark pays nothing out but still drops its pool.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (504263, 572279)
    AND `ScriptName` = 'aura_ascension_bloodmage_blood_veil';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(504263, 'aura_ascension_bloodmage_blood_veil'),
(572279, 'aura_ascension_bloodmage_blood_veil');

DELETE FROM `spell_script_names` WHERE `spell_id` = 680828
    AND `ScriptName` = 'aura_ascension_bloodmage_darkfallen_lament';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(680828, 'aura_ascension_bloodmage_darkfallen_lament');

DELETE FROM `spell_script_names` WHERE `spell_id` IN (680680, 681403)
    AND `ScriptName` = 'aura_ascension_bloodmage_plague_mark';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(680680, 'aura_ascension_bloodmage_plague_mark'),
(681403, 'aura_ascension_bloodmage_plague_mark');
