-- Atherann's Anguish (680680) and Infuse (681403) each detonate through their own aura script
-- (aura_ascension_bloodmage_atheranns_anguish, aura_ascension_bloodmage_infuse). The earlier generic
-- plague mark bound to both spells cast the same burst a second time on expiry, and for Infuse paid
-- out the whole banked damage instead of the stored tenth; its script is gone.
DELETE FROM `spell_script_names` WHERE `spell_id` IN (680680, 681403)
    AND `ScriptName` = 'aura_ascension_bloodmage_plague_mark';
