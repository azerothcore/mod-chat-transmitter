-- mod-chat-transmitter: game accounts linked to Discord users (".discord link" in game, "/link <code>" on Discord).
-- Only the worldserver writes here. The bot reads it with a read-only database user.
CREATE TABLE IF NOT EXISTS `chat_transmitter_discord_link` (
  `account_id` INT UNSIGNED NOT NULL COMMENT 'account.id',
  `discord_id` VARCHAR(20) NOT NULL COMMENT 'Discord user id (a number)',
  `linked_at` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'unix time',
  PRIMARY KEY (`account_id`),
  KEY `idx_discord_id` (`discord_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
