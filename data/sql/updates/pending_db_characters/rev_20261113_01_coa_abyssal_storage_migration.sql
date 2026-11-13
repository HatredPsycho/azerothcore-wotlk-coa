-- mod-abyssal-storage-plus replaces mod-reagent-bank-account: the stored reagents move into the account vault.
-- Account-wide rows keep their account, character-wide rows go to the character's account; amounts of one item
-- add up. The old tables stay in place untouched.
CREATE TABLE IF NOT EXISTS `abyssal_storage` (
  `account_id` INT UNSIGNED NOT NULL,
  `item_entry` INT UNSIGNED NOT NULL,
  `count` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`account_id`, `item_entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

SELECT IF(
    EXISTS(SELECT 1 FROM `information_schema`.`tables` WHERE `table_schema` = DATABASE()
        AND `table_name` = 'mod_reagent_bank_account'),
    'INSERT INTO `abyssal_storage` (`account_id`, `item_entry`, `count`)
     SELECT IF(`bank`.`account_id` <> 0, `bank`.`account_id`, `characters`.`account`), `bank`.`item_entry`,
            `bank`.`amount`
     FROM `mod_reagent_bank_account` AS `bank`
     LEFT JOIN `characters` ON `characters`.`guid` = `bank`.`guid` AND `bank`.`account_id` = 0
     WHERE `bank`.`amount` > 0 AND IF(`bank`.`account_id` <> 0, `bank`.`account_id`, `characters`.`account`) <> 0
     ON DUPLICATE KEY UPDATE `count` = `abyssal_storage`.`count` + VALUES(`count`)',
    'DO 0') INTO @coa_abyssal_migration_sql;
PREPARE coa_abyssal_migration FROM @coa_abyssal_migration_sql;
EXECUTE coa_abyssal_migration;
DEALLOCATE PREPARE coa_abyssal_migration;
