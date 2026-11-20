-- mod-chat-transmitter: the first kill of each boss on this realm, per difficulty (realm first kills).
CREATE TABLE IF NOT EXISTS `chat_transmitter_boss_kills` (
  `entry` INT UNSIGNED NOT NULL COMMENT 'creature_template.entry',
  `difficulty` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `map` INT UNSIGNED NOT NULL DEFAULT 0,
  `killed_at` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'unix time',
  `killers` VARCHAR(255) NOT NULL DEFAULT '' COMMENT 'character names, comma separated',
  `guild_name` VARCHAR(24) NOT NULL DEFAULT '',
  PRIMARY KEY (`entry`, `difficulty`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- mod-chat-transmitter: messages for Discord from any module or tool. The worldserver sends each row to the bot
-- as a notification (the bot picks the channel by "source") and then deletes it.
-- Example: INSERT INTO chat_transmitter_event_queue (source, message) VALUES ('events', 'The Darkmoon Faire is here!');
CREATE TABLE IF NOT EXISTS `chat_transmitter_event_queue` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `source` VARCHAR(50) NOT NULL DEFAULT 'default',
  `message` TEXT NOT NULL,
  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
