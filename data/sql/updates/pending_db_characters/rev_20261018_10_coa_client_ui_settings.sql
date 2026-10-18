CREATE TABLE IF NOT EXISTS `coa_client_ui_settings` (
    `guid` INT UNSIGNED NOT NULL,
    `name` VARCHAR(32) NOT NULL,
    `data` VARCHAR(255) NOT NULL DEFAULT '',
    PRIMARY KEY (`guid`, `name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
