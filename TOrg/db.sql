CREATE TABLE `devicesTable` (
    `id` int NOT NULL AUTO_INCREMENT,
    `device_id` varchar(16) NOT NULL,
    `is_trust_org` tinyint(1) NOT NULL DEFAULT '0',
    `created_on` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_on` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY `device_id` (`device_id`)
) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE `secretKeysTable` (
    `id` int unsigned NOT NULL AUTO_INCREMENT,
    `hashed_tag_uid` varchar(32) NOT NULL,
    `secret_key` varchar(12) NOT NULL,
    `created_on` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_on` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY `hashed_tag_uid` (`hashed_tag_uid`)
) ENGINE=InnoDB AUTO_INCREMENT=25 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE `rollingPasswordTable` (
    `id` int unsigned NOT NULL AUTO_INCREMENT,
    `hashed_blockdata` varchar(32) DEFAULT NULL,
    `rolling_pass` varchar(64) DEFAULT NULL,
    `created_on` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_on` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    `secret_key_id` int unsigned NOT NULL,
    PRIMARY KEY (`id`),
    KEY `fk_constraint` (`secret_key_id`),
    KEY `created_on` (`created_on`),
    CONSTRAINT `fk_constraint` FOREIGN KEY (`secret_key_id`) REFERENCES `secretKeysTable` (`id`) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB AUTO_INCREMENT=57 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

ALTER TABLE `secretKeysTable` AUTO_INCREMENT=1;
ALTER TABLE `rollingPasswordTable` AUTO_INCREMENT=1;