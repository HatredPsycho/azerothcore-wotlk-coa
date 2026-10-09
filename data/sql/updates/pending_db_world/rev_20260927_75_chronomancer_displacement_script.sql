-- Chronomancer Displacement (806727): "Displaces a party or raid member and pulls them to you,
-- dispelling root and snare effects from them." chronomancer_movement_contracts rewrites effect 2
-- to SPELL_EFFECT_DUMMY so spell_ascension_displacement can carry out the reposition and clear the
-- movement impairing auras, but the script was never assigned, so that effect did nothing and the
-- worldserver reported "Script named 'spell_ascension_displacement' is not assigned in the
-- database." Every sibling script of the same source file is assigned (706973, 801294, 801277).
DELETE FROM `spell_script_names` WHERE `spell_id` = 806727
    AND `ScriptName` = 'spell_ascension_displacement';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(806727, 'spell_ascension_displacement');
