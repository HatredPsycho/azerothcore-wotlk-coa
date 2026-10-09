-- Adventure Mode's per-level "... SLS" spell (302054, 302622, ...) periodically triggers
-- 888071 "Adventurer Mode Random Aggro": SPELL_EFFECT_THREAT on TARGET_UNIT_SRC_AREA_ENEMY within
-- 25 yards. Unit::_IsValidAttackTarget passes ignoreStealth = SpellInfo::IsAffectingArea(), and
-- WorldObject::CanDetect skips the invisibility check under that flag, so area spells also reach
-- invisible units. Ambience creatures such as Lordaeron Citizen (3617, aura 34426) are therefore
-- pulled into combat and kill characters who can neither see nor fight back against them.
DELETE FROM `spell_script_names` WHERE `spell_id` = 888071
    AND `ScriptName` = 'spell_ascension_adventure_mode_random_aggro';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(888071, 'spell_ascension_adventure_mode_random_aggro');
