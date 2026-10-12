-- CoA Custom races: characters whose armor is hidden with .hidearmor.
CREATE TABLE IF NOT EXISTS `coa_hide_armor` (
  `guid` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
