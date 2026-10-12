-- CoA Custom races, stage 2: the Esteria appearance of Highmountain Tauren and Earthen (a sixth appearance byte) and of
-- the Haranir (a uint64), which the client's EsteriaAppearance.dll sends at creation and reads back from the enum.
ALTER TABLE `characters` ADD COLUMN `extraAppearance` BIGINT UNSIGNED NOT NULL DEFAULT 0;
