CREATE TABLE IF NOT EXISTS `mod_no_gems_item_affix` (
  `item_guid` INT UNSIGNED NOT NULL,
  `item_entry` INT UNSIGNED NOT NULL,
  `roll_version` INT UNSIGNED NOT NULL DEFAULT 1,
  `socket_signature` VARCHAR(32) NOT NULL DEFAULT '',
  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`item_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `mod_no_gems_item_affix_effect` (
  `item_guid` INT UNSIGNED NOT NULL,
  `socket_index` TINYINT UNSIGNED NOT NULL,
  `socket_type` TINYINT UNSIGNED NOT NULL,
  `source_gem_entry` INT UNSIGNED NOT NULL DEFAULT 0,
  `source_enchant_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `mod_type` INT UNSIGNED NOT NULL DEFAULT 0,
  `amount` INT NOT NULL DEFAULT 0,
  PRIMARY KEY (`item_guid`, `socket_index`),
  CONSTRAINT `fk_mod_no_gems_item_affix` FOREIGN KEY (`item_guid`) REFERENCES `mod_no_gems_item_affix` (`item_guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
