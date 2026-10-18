-- Revanchion (Scourge Invasion boss in Dire Maul) has smart_scripts rows for Shadow Bolt Volley and Frost Nova but an empty AIName, so they never ran
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` = 14690;
