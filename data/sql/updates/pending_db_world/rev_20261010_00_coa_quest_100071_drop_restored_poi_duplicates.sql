-- CoA Goldshire: Defias Disruption (100071) shows the turn-in pin and one area per objective (ids 0-2, set by
-- rev_20261009_00_coa_quest_6977_goldshire_objective_markers). rev_20261009_30_coa_quest_poi_restore had also
-- given it one area per spawn cluster (ids 1-12); the Goldshire file replaced ids 1 and 2 only, so ids 3-12
-- stayed and the map drew ten more areas on top of the two objective areas.
DELETE FROM `quest_poi_points` WHERE `QuestID` = 100071 AND `Idx1` BETWEEN 3 AND 12;
DELETE FROM `quest_poi` WHERE `QuestID` = 100071 AND `id` BETWEEN 3 AND 12;
