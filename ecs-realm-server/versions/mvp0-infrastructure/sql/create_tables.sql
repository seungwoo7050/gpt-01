-- [SEQUENCE: 265] Create database and tables for MMORPG server
CREATE DATABASE IF NOT EXISTS mmorpg CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mmorpg;

-- [SEQUENCE: 266] Players table (account information)
CREATE TABLE IF NOT EXISTS players (
    player_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    salt VARCHAR(32) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP NULL,
    is_banned BOOLEAN DEFAULT FALSE,
    ban_reason TEXT NULL,
    ban_until TIMESTAMP NULL,
    INDEX idx_username (username),
    INDEX idx_email (email),
    INDEX idx_last_login (last_login)
) ENGINE=InnoDB;

-- [SEQUENCE: 267] Characters table
CREATE TABLE IF NOT EXISTS characters (
    character_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    player_id BIGINT NOT NULL,
    character_name VARCHAR(50) UNIQUE NOT NULL,
    class_type ENUM('warrior', 'mage', 'archer', 'priest') NOT NULL,
    level INT DEFAULT 1,
    experience BIGINT DEFAULT 0,
    stat_points INT DEFAULT 0,
    skill_points INT DEFAULT 0,
    current_hp INT NOT NULL,
    current_mp INT NOT NULL,
    max_hp INT NOT NULL,
    max_mp INT NOT NULL,
    strength INT DEFAULT 10,
    intelligence INT DEFAULT 10,
    dexterity INT DEFAULT 10,
    vitality INT DEFAULT 10,
    current_map_id INT DEFAULT 1,
    position_x FLOAT DEFAULT 0,
    position_y FLOAT DEFAULT 0,
    position_z FLOAT DEFAULT 0,
    gold BIGINT DEFAULT 1000,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_played TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    is_deleted BOOLEAN DEFAULT FALSE,
    FOREIGN KEY (player_id) REFERENCES players(player_id),
    INDEX idx_player (player_id),
    INDEX idx_name (character_name),
    INDEX idx_map (current_map_id)
) ENGINE=InnoDB;

-- [SEQUENCE: 268] Inventory table
CREATE TABLE IF NOT EXISTS inventory (
    inventory_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    character_id BIGINT NOT NULL,
    item_id INT NOT NULL,
    slot_index INT NOT NULL,
    quantity INT DEFAULT 1,
    enhancement_level INT DEFAULT 0,
    durability INT DEFAULT 100,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (character_id) REFERENCES characters(character_id) ON DELETE CASCADE,
    UNIQUE KEY unique_slot (character_id, slot_index),
    INDEX idx_character (character_id)
) ENGINE=InnoDB;

-- [SEQUENCE: 269] Equipped items table
CREATE TABLE IF NOT EXISTS equipped_items (
    character_id BIGINT NOT NULL,
    equipment_slot ENUM('weapon', 'armor', 'helmet', 'gloves', 'boots', 'accessory1', 'accessory2') NOT NULL,
    item_id INT NOT NULL,
    enhancement_level INT DEFAULT 0,
    PRIMARY KEY (character_id, equipment_slot),
    FOREIGN KEY (character_id) REFERENCES characters(character_id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- [SEQUENCE: 270] Guilds table
CREATE TABLE IF NOT EXISTS guilds (
    guild_id INT PRIMARY KEY AUTO_INCREMENT,
    guild_name VARCHAR(100) UNIQUE NOT NULL,
    leader_id BIGINT NOT NULL,
    level INT DEFAULT 1,
    experience BIGINT DEFAULT 0,
    member_count INT DEFAULT 1,
    max_members INT DEFAULT 50,
    guild_notice TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (leader_id) REFERENCES characters(character_id),
    INDEX idx_name (guild_name),
    INDEX idx_leader (leader_id)
) ENGINE=InnoDB;

-- [SEQUENCE: 271] Guild members table
CREATE TABLE IF NOT EXISTS guild_members (
    guild_id INT NOT NULL,
    character_id BIGINT NOT NULL,
    rank ENUM('leader', 'officer', 'member') DEFAULT 'member',
    joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    contribution_points INT DEFAULT 0,
    PRIMARY KEY (character_id),
    FOREIGN KEY (guild_id) REFERENCES guilds(guild_id) ON DELETE CASCADE,
    FOREIGN KEY (character_id) REFERENCES characters(character_id) ON DELETE CASCADE,
    INDEX idx_guild (guild_id)
) ENGINE=InnoDB;

-- [SEQUENCE: 272] Friends table
CREATE TABLE IF NOT EXISTS friends (
    character_id BIGINT NOT NULL,
    friend_character_id BIGINT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (character_id, friend_character_id),
    FOREIGN KEY (character_id) REFERENCES characters(character_id) ON DELETE CASCADE,
    FOREIGN KEY (friend_character_id) REFERENCES characters(character_id) ON DELETE CASCADE,
    INDEX idx_character (character_id)
) ENGINE=InnoDB;

-- [SEQUENCE: 273] Quest progress table
CREATE TABLE IF NOT EXISTS quest_progress (
    character_id BIGINT NOT NULL,
    quest_id INT NOT NULL,
    status ENUM('active', 'completed', 'failed', 'abandoned') DEFAULT 'active',
    progress JSON,
    started_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    completed_at TIMESTAMP NULL,
    PRIMARY KEY (character_id, quest_id),
    FOREIGN KEY (character_id) REFERENCES characters(character_id) ON DELETE CASCADE,
    INDEX idx_character_status (character_id, status)
) ENGINE=InnoDB;

-- [SEQUENCE: 274] Login sessions table (for security)
CREATE TABLE IF NOT EXISTS login_sessions (
    session_id VARCHAR(128) PRIMARY KEY,
    player_id BIGINT NOT NULL,
    character_id BIGINT NULL,
    ip_address VARCHAR(45) NOT NULL,
    user_agent VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_activity TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NOT NULL,
    is_active BOOLEAN DEFAULT TRUE,
    FOREIGN KEY (player_id) REFERENCES players(player_id),
    FOREIGN KEY (character_id) REFERENCES characters(character_id),
    INDEX idx_player (player_id),
    INDEX idx_expires (expires_at),
    INDEX idx_active (is_active)
) ENGINE=InnoDB;

-- [SEQUENCE: 275] Audit log table
CREATE TABLE IF NOT EXISTS audit_log (
    log_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    player_id BIGINT,
    character_id BIGINT,
    action_type VARCHAR(50) NOT NULL,
    details JSON,
    ip_address VARCHAR(45),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_player (player_id),
    INDEX idx_character (character_id),
    INDEX idx_action (action_type),
    INDEX idx_created (created_at)
) ENGINE=InnoDB;

-- [SEQUENCE: 276] Create initial test data
INSERT INTO players (username, password_hash, email, salt) VALUES
    ('testuser1', 'dummy_hash_1', 'test1@example.com', 'salt1'),
    ('testuser2', 'dummy_hash_2', 'test2@example.com', 'salt2');

-- [SEQUENCE: 277] Create stored procedures
DELIMITER $$

CREATE PROCEDURE CreateCharacter(
    IN p_player_id BIGINT,
    IN p_character_name VARCHAR(50),
    IN p_class_type VARCHAR(20),
    OUT p_character_id BIGINT
)
BEGIN
    DECLARE v_hp INT DEFAULT 100;
    DECLARE v_mp INT DEFAULT 50;
    
    -- [SEQUENCE: 278] Set base stats by class
    IF p_class_type = 'warrior' THEN
        SET v_hp = 150;
        SET v_mp = 30;
    ELSEIF p_class_type = 'mage' THEN
        SET v_hp = 80;
        SET v_mp = 120;
    END IF;
    
    INSERT INTO characters (player_id, character_name, class_type, current_hp, current_mp, max_hp, max_mp)
    VALUES (p_player_id, p_character_name, p_class_type, v_hp, v_mp, v_hp, v_mp);
    
    SET p_character_id = LAST_INSERT_ID();
END$$

DELIMITER ;