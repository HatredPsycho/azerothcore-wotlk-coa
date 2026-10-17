CREATE TABLE IF NOT EXISTS `mod_reagent_bank_account_deposit_exclusions_zz_custom` (
    `item_entry` INT UNSIGNED NOT NULL,
    `comment` VARCHAR(255) NULL DEFAULT NULL,
    PRIMARY KEY (`item_entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO `mod_reagent_bank_account_deposit_exclusions_zz_custom` (`item_entry`, `comment`) VALUES
(18232, 'Field Repair Bot 74A'),
(34113, 'Field Repair Bot 110G'),
(40769, 'Scrapbot Construction Kit'),
(49040, 'Jeeves')
ON DUPLICATE KEY UPDATE
    `comment` = VALUES(`comment`);
