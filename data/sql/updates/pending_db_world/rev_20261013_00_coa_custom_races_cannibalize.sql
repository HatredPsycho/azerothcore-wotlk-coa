-- CoA Custom races: Cannibalize (race 51), Make Camp (Vulpera) and Bone Gnaw (Gnoll) copy Cannibalize 20577,
-- whose corpse check and 20578 channel are spell_gen_cannibalize (CoA Custom 1.6).
DELETE FROM `spell_script_names`
WHERE `spell_id` IN (14000512, 14000742, 14000832) AND `ScriptName` = 'spell_gen_cannibalize';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(14000512, 'spell_gen_cannibalize'),
(14000742, 'spell_gen_cannibalize'),
(14000832, 'spell_gen_cannibalize');
