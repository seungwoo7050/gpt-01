# MMORPG Server Database Schema

## Overview

This document details the complete database schema for the MMORPG server, including table structures, relationships, indexing strategies, and partitioning schemes. The database is designed to handle 50,000+ concurrent players with optimal performance.

## Table of Contents

1. [Database Architecture](#database-architecture)
2. [Core Tables](#core-tables)
3. [Game Tables](#game-tables)
4. [Economy Tables](#economy-tables)
5. [Social Tables](#social-tables)
6. [Guild Tables](#guild-tables)
7. [Housing Tables](#housing-tables)
8. [Monitoring Tables](#monitoring-tables)
9. [Indexing Strategy](#indexing-strategy)
10. [Partitioning Strategy](#partitioning-strategy)
11. [Data Types & Conventions](#data-types--conventions)

---

## Database Architecture

### Infrastructure Overview

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Write Master  │────▶│  Read Replica 1 │     │  Read Replica 2 │
│   (Primary DB)  │     └─────────────────┘     └─────────────────┘
└────────┬────────┘              │                        │
         │                       ▼                        ▼
         │              ┌─────────────────┐     ┌─────────────────┐
         │              │   Redis Cache   │     │   Redis Cache   │
         │              └─────────────────┘     └─────────────────┘
         │
         ▼
┌─────────────────┐
│  Backup Server  │
└─────────────────┘
```

### Database Configuration

```sql
-- Character Set and Collation
CREATE DATABASE mmorpg_game
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

-- Storage Engine
SET default_storage_engine = InnoDB;

-- Time Zone
SET time_zone = '+00:00';  -- UTC

-- Connection Pool Settings
SET max_connections = 1000;
SET max_user_connections = 50;
```

---

## Core Tables

### users
Primary user account information.

```sql
CREATE TABLE users (
    user_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    username VARCHAR(32) NOT NULL,
    email VARCHAR(255) NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    salt VARCHAR(32) NOT NULL,
    
    -- Security
    mfa_secret VARCHAR(32),
    mfa_enabled BOOLEAN DEFAULT FALSE,
    last_password_change TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    failed_login_attempts INT DEFAULT 0,
    locked_until TIMESTAMP NULL,
    
    -- Account Status
    account_status ENUM('active', 'suspended', 'banned', 'deleted') DEFAULT 'active',
    ban_reason TEXT,
    ban_expires_at TIMESTAMP NULL,
    
    -- Metadata
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    last_login_at TIMESTAMP NULL,
    last_ip_address VARCHAR(45),  -- IPv6 support
    
    -- GDPR
    data_consent BOOLEAN DEFAULT TRUE,
    marketing_consent BOOLEAN DEFAULT FALSE,
    
    PRIMARY KEY (user_id),
    UNIQUE KEY idx_username (username),
    UNIQUE KEY idx_email (email),
    KEY idx_account_status (account_status),
    KEY idx_last_login (last_login_at)
) ENGINE=InnoDB;
```

### characters
Player character data.

```sql
CREATE TABLE characters (
    character_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id BIGINT UNSIGNED NOT NULL,
    
    -- Basic Info
    character_name VARCHAR(32) NOT NULL,
    class_id INT UNSIGNED NOT NULL,
    level INT UNSIGNED DEFAULT 1,
    experience BIGINT UNSIGNED DEFAULT 0,
    
    -- Location
    map_id INT UNSIGNED NOT NULL DEFAULT 1,
    position_x FLOAT NOT NULL DEFAULT 0,
    position_y FLOAT NOT NULL DEFAULT 0,
    position_z FLOAT NOT NULL DEFAULT 0,
    rotation FLOAT NOT NULL DEFAULT 0,
    
    -- Stats
    health_current INT UNSIGNED NOT NULL,
    health_max INT UNSIGNED NOT NULL,
    mana_current INT UNSIGNED NOT NULL,
    mana_max INT UNSIGNED NOT NULL,
    
    -- Combat Stats
    strength INT UNSIGNED DEFAULT 10,
    agility INT UNSIGNED DEFAULT 10,
    intelligence INT UNSIGNED DEFAULT 10,
    vitality INT UNSIGNED DEFAULT 10,
    stat_points INT UNSIGNED DEFAULT 0,
    
    -- Currency
    gold BIGINT UNSIGNED DEFAULT 0,
    premium_currency INT UNSIGNED DEFAULT 0,
    
    -- Status
    is_online BOOLEAN DEFAULT FALSE,
    is_deleted BOOLEAN DEFAULT FALSE,
    playtime_seconds BIGINT UNSIGNED DEFAULT 0,
    
    -- Timestamps
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_played TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    deleted_at TIMESTAMP NULL,
    
    PRIMARY KEY (character_id),
    UNIQUE KEY idx_character_name (character_name),
    KEY idx_user_id (user_id),
    KEY idx_level (level),
    KEY idx_map_location (map_id, position_x, position_z),
    KEY idx_online_status (is_online, map_id),
    
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB;
```

### sessions
Active login sessions for security and analytics.

```sql
CREATE TABLE sessions (
    session_id VARCHAR(128) NOT NULL,
    user_id BIGINT UNSIGNED NOT NULL,
    character_id BIGINT UNSIGNED,
    
    -- Session Data
    ip_address VARCHAR(45) NOT NULL,
    user_agent VARCHAR(255),
    server_id INT UNSIGNED NOT NULL,
    
    -- Security
    csrf_token VARCHAR(64),
    last_activity TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NOT NULL,
    
    -- Metadata
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    PRIMARY KEY (session_id),
    KEY idx_user_id (user_id),
    KEY idx_expires (expires_at),
    KEY idx_server (server_id),
    
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB;
```

---

## Game Tables

### inventory
Character inventory system with stacking support.

```sql
CREATE TABLE inventory (
    inventory_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    character_id BIGINT UNSIGNED NOT NULL,
    
    -- Item Info
    item_id INT UNSIGNED NOT NULL,
    quantity INT UNSIGNED DEFAULT 1,
    slot_position INT UNSIGNED NOT NULL,
    
    -- Item State
    durability INT UNSIGNED,
    enchantment_level INT UNSIGNED DEFAULT 0,
    is_bound BOOLEAN DEFAULT FALSE,
    bound_type ENUM('character', 'account') DEFAULT 'character',
    
    -- Metadata
    obtained_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NULL,
    
    PRIMARY KEY (inventory_id),
    UNIQUE KEY idx_character_slot (character_id, slot_position),
    KEY idx_item_id (item_id),
    KEY idx_expires (expires_at),
    
    FOREIGN KEY (character_id) REFERENCES characters(character_id) ON DELETE CASCADE
) ENGINE=InnoDB;
```

### equipment
Currently equipped items.

```sql
CREATE TABLE equipment (
    character_id BIGINT UNSIGNED NOT NULL,
    slot_type ENUM('head', 'chest', 'legs', 'feet', 'hands', 
                   'main_hand', 'off_hand', 'neck', 'ring1', 'ring2',
                   'earring1', 'earring2', 'belt', 'cape') NOT NULL,
    
    inventory_id BIGINT UNSIGNED NOT NULL,
    equipped_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    PRIMARY KEY (character_id, slot_type),
    KEY idx_inventory (inventory_id),
    
    FOREIGN KEY (character_id) REFERENCES characters(character_id) ON DELETE CASCADE,
    FOREIGN KEY (inventory_id) REFERENCES inventory(inventory_id) ON DELETE CASCADE
) ENGINE=InnoDB;
```

### skills
Character learned skills and cooldowns.

```sql
CREATE TABLE character_skills (
    character_id BIGINT UNSIGNED NOT NULL,
    skill_id INT UNSIGNED NOT NULL,
    
    -- Progression
    skill_level INT UNSIGNED DEFAULT 1,
    skill_exp INT UNSIGNED DEFAULT 0,
    
    -- Cooldown Tracking
    last_used_at TIMESTAMP NULL,
    cooldown_expires_at TIMESTAMP NULL,
    
    -- Hotbar
    hotbar_slot INT UNSIGNED,
    
    PRIMARY KEY (character_id, skill_id),
    KEY idx_cooldown (cooldown_expires_at),
    
    FOREIGN KEY (character_id) REFERENCES characters(character_id) ON DELETE CASCADE
) ENGINE=InnoDB;
```

### quests
Active and completed quests.

```sql
CREATE TABLE character_quests (
    character_id BIGINT UNSIGNED NOT NULL,
    quest_id INT UNSIGNED NOT NULL,
    
    -- Status
    status ENUM('active', 'completed', 'failed', 'abandoned') DEFAULT 'active',
    current_step INT UNSIGNED DEFAULT 0,
    
    -- Progress Tracking (JSON)
    progress JSON,
    /* Example:
    {
        "monsters_killed": {"goblin": 5, "orc": 2},
        "items_collected": {"herb": 10},
        "locations_visited": ["cave_entrance", "ancient_ruins"]
    }
    */
    
    -- Timestamps
    accepted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    completed_at TIMESTAMP NULL,
    
    PRIMARY KEY (character_id, quest_id),
    KEY idx_status (status),
    KEY idx_quest_id (quest_id),
    
    FOREIGN KEY (character_id) REFERENCES characters(character_id) ON DELETE CASCADE
) ENGINE=InnoDB;
```

### achievements
Player achievements and statistics.

```sql
CREATE TABLE character_achievements (
    character_id BIGINT UNSIGNED NOT NULL,
    achievement_id INT UNSIGNED NOT NULL,
    
    -- Progress
    progress INT UNSIGNED DEFAULT 0,
    max_progress INT UNSIGNED NOT NULL,
    
    -- Completion
    completed BOOLEAN DEFAULT FALSE,
    completed_at TIMESTAMP NULL,
    
    -- Rewards
    reward_claimed BOOLEAN DEFAULT FALSE,
    
    PRIMARY KEY (character_id, achievement_id),
    KEY idx_completed (completed),
    KEY idx_achievement (achievement_id),
    
    FOREIGN KEY (character_id) REFERENCES characters(character_id) ON DELETE CASCADE
) ENGINE=InnoDB;
```

---

## Economy Tables

### auction_house
Server-wide auction house listings.

```sql
CREATE TABLE auction_listings (
    listing_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    seller_character_id BIGINT UNSIGNED NOT NULL,
    
    -- Item Details
    item_id INT UNSIGNED NOT NULL,
    quantity INT UNSIGNED NOT NULL,
    enchantment_level INT UNSIGNED DEFAULT 0,
    
    -- Pricing
    price_per_unit BIGINT UNSIGNED NOT NULL,
    buyout_price BIGINT UNSIGNED,
    deposit_amount BIGINT UNSIGNED NOT NULL,
    
    -- Status
    status ENUM('active', 'sold', 'expired', 'cancelled') DEFAULT 'active',
    buyer_character_id BIGINT UNSIGNED,
    
    -- Timestamps
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NOT NULL,
    sold_at TIMESTAMP NULL,
    
    PRIMARY KEY (listing_id),
    KEY idx_item_price (item_id, price_per_unit),
    KEY idx_seller (seller_character_id),
    KEY idx_status_expires (status, expires_at),
    KEY idx_created (created_at),
    
    FOREIGN KEY (seller_character_id) REFERENCES characters(character_id)
) ENGINE=InnoDB;
```

### trade_history
Complete trade transaction history for auditing.

```sql
CREATE TABLE trade_history (
    trade_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    
    -- Participants
    character1_id BIGINT UNSIGNED NOT NULL,
    character2_id BIGINT UNSIGNED NOT NULL,
    
    -- Trade Contents (JSON)
    character1_items JSON,
    character2_items JSON,
    character1_gold BIGINT UNSIGNED DEFAULT 0,
    character2_gold BIGINT UNSIGNED DEFAULT 0,
    
    -- Status
    status ENUM('completed', 'cancelled', 'timeout') NOT NULL,
    
    -- Security
    ip_address1 VARCHAR(45),
    ip_address2 VARCHAR(45),
    
    -- Timestamps
    initiated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    completed_at TIMESTAMP NULL,
    
    PRIMARY KEY (trade_id),
    KEY idx_characters (character1_id, character2_id),
    KEY idx_timestamp (initiated_at),
    
    FOREIGN KEY (character1_id) REFERENCES characters(character_id),
    FOREIGN KEY (character2_id) REFERENCES characters(character_id)
) ENGINE=InnoDB;
```

### market_history
Price history for economic analysis.

```sql
CREATE TABLE market_history (
    history_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    item_id INT UNSIGNED NOT NULL,
    
    -- Price Data
    avg_price DECIMAL(15,2) NOT NULL,
    min_price DECIMAL(15,2) NOT NULL,
    max_price DECIMAL(15,2) NOT NULL,
    volume_traded INT UNSIGNED NOT NULL,
    
    -- Time Period
    period_start TIMESTAMP NOT NULL,
    period_end TIMESTAMP NOT NULL,
    
    PRIMARY KEY (history_id),
    UNIQUE KEY idx_item_period (item_id, period_start),
    KEY idx_period (period_start, period_end)
) ENGINE=InnoDB
PARTITION BY RANGE (UNIX_TIMESTAMP(period_start)) (
    PARTITION p_2024_01 VALUES LESS THAN (UNIX_TIMESTAMP('2024-02-01')),
    PARTITION p_2024_02 VALUES LESS THAN (UNIX_TIMESTAMP('2024-03-01')),
    PARTITION p_future VALUES LESS THAN MAXVALUE
);
```

---

## Social Tables

### friends
Friend relationships between characters.

```sql
CREATE TABLE friends (
    character_id BIGINT UNSIGNED NOT NULL,
    friend_character_id BIGINT UNSIGNED NOT NULL,
    
    -- Status
    status ENUM('pending', 'accepted', 'blocked') DEFAULT 'pending',
    
    -- Metadata
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    accepted_at TIMESTAMP NULL,
    
    PRIMARY KEY (character_id, friend_character_id),
    KEY idx_friend (friend_character_id),
    KEY idx_status (status),
    
    FOREIGN KEY (character_id) REFERENCES characters(character_id) ON DELETE CASCADE,
    FOREIGN KEY (friend_character_id) REFERENCES characters(character_id) ON DELETE CASCADE
) ENGINE=InnoDB;
```

### mail
In-game mail system with attachments.

```sql
CREATE TABLE mail (
    mail_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    
    -- Participants
    sender_character_id BIGINT UNSIGNED,  -- NULL for system mail
    recipient_character_id BIGINT UNSIGNED NOT NULL,
    
    -- Content
    subject VARCHAR(100) NOT NULL,
    body TEXT NOT NULL,
    
    -- Attachments
    attached_gold BIGINT UNSIGNED DEFAULT 0,
    attached_items JSON,  -- Array of {item_id, quantity}
    
    -- Status
    is_read BOOLEAN DEFAULT FALSE,
    is_deleted BOOLEAN DEFAULT FALSE,
    attachment_taken BOOLEAN DEFAULT FALSE,
    
    -- Timestamps
    sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NOT NULL,
    read_at TIMESTAMP NULL,
    
    PRIMARY KEY (mail_id),
    KEY idx_recipient (recipient_character_id, is_deleted, sent_at),
    KEY idx_expires (expires_at),
    
    FOREIGN KEY (sender_character_id) REFERENCES characters(character_id) ON DELETE SET NULL,
    FOREIGN KEY (recipient_character_id) REFERENCES characters(character_id) ON DELETE CASCADE
) ENGINE=InnoDB;
```

### chat_logs
Chat history for moderation and support.

```sql
CREATE TABLE chat_logs (
    log_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    
    -- Message Info
    character_id BIGINT UNSIGNED NOT NULL,
    channel_type ENUM('general', 'party', 'guild', 'whisper', 'trade', 'zone') NOT NULL,
    message TEXT NOT NULL,
    
    -- Context
    recipient_character_id BIGINT UNSIGNED,  -- For whispers
    zone_id INT UNSIGNED,  -- For zone chat
    party_id BIGINT UNSIGNED,  -- For party chat
    guild_id BIGINT UNSIGNED,  -- For guild chat
    
    -- Moderation
    is_flagged BOOLEAN DEFAULT FALSE,
    flagged_reason VARCHAR(255),
    moderated_by BIGINT UNSIGNED,
    
    -- Timestamp
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    PRIMARY KEY (log_id),
    KEY idx_character_time (character_id, created_at),
    KEY idx_channel_time (channel_type, created_at),
    KEY idx_flagged (is_flagged),
    
    FOREIGN KEY (character_id) REFERENCES characters(character_id)
) ENGINE=InnoDB
PARTITION BY RANGE (UNIX_TIMESTAMP(created_at)) (
    PARTITION p_logs_2024_01 VALUES LESS THAN (UNIX_TIMESTAMP('2024-02-01')),
    PARTITION p_logs_2024_02 VALUES LESS THAN (UNIX_TIMESTAMP('2024-03-01')),
    PARTITION p_logs_future VALUES LESS THAN MAXVALUE
);
```

---

## Guild Tables

### guilds
Guild information and settings.

```sql
CREATE TABLE guilds (
    guild_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    
    -- Basic Info
    guild_name VARCHAR(50) NOT NULL,
    guild_tag VARCHAR(5) NOT NULL,
    
    -- Leadership
    leader_character_id BIGINT UNSIGNED NOT NULL,
    
    -- Stats
    guild_level INT UNSIGNED DEFAULT 1,
    guild_exp BIGINT UNSIGNED DEFAULT 0,
    member_count INT UNSIGNED DEFAULT 1,
    max_members INT UNSIGNED DEFAULT 50,
    
    -- Resources
    guild_points INT UNSIGNED DEFAULT 0,
    guild_gold BIGINT UNSIGNED DEFAULT 0,
    
    -- Customization
    emblem_id INT UNSIGNED DEFAULT 0,
    description TEXT,
    recruitment_status ENUM('open', 'apply', 'closed') DEFAULT 'open',
    
    -- Timestamps
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    disbanded_at TIMESTAMP NULL,
    
    PRIMARY KEY (guild_id),
    UNIQUE KEY idx_guild_name (guild_name),
    UNIQUE KEY idx_guild_tag (guild_tag),
    KEY idx_leader (leader_character_id),
    KEY idx_level (guild_level),
    
    FOREIGN KEY (leader_character_id) REFERENCES characters(character_id)
) ENGINE=InnoDB;
```

### guild_members
Guild membership and ranks.

```sql
CREATE TABLE guild_members (
    guild_id BIGINT UNSIGNED NOT NULL,
    character_id BIGINT UNSIGNED NOT NULL,
    
    -- Rank System
    rank_id INT UNSIGNED NOT NULL DEFAULT 5,  -- 1=Leader, 2=Officer, etc.
    
    -- Contribution
    contribution_points BIGINT UNSIGNED DEFAULT 0,
    weekly_contribution INT UNSIGNED DEFAULT 0,
    
    -- Permissions (Bitfield)
    permissions BIGINT UNSIGNED DEFAULT 0,
    
    -- Activity
    last_active TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    -- Notes
    member_note TEXT,  -- Personal note
    officer_note TEXT,  -- Note visible to officers
    
    PRIMARY KEY (character_id),
    KEY idx_guild_rank (guild_id, rank_id),
    KEY idx_contribution (guild_id, contribution_points),
    
    FOREIGN KEY (guild_id) REFERENCES guilds(guild_id) ON DELETE CASCADE,
    FOREIGN KEY (character_id) REFERENCES characters(character_id) ON DELETE CASCADE
) ENGINE=InnoDB;
```

### guild_wars
Guild war declarations and results.

```sql
CREATE TABLE guild_wars (
    war_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    
    -- Participants
    attacker_guild_id BIGINT UNSIGNED NOT NULL,
    defender_guild_id BIGINT UNSIGNED NOT NULL,
    
    -- War Details
    war_type ENUM('total', 'objective', 'scheduled') NOT NULL,
    max_participants INT UNSIGNED DEFAULT 100,
    
    -- Score
    attacker_score INT UNSIGNED DEFAULT 0,
    defender_score INT UNSIGNED DEFAULT 0,
    
    -- Status
    status ENUM('pending', 'active', 'completed', 'cancelled') DEFAULT 'pending',
    winner_guild_id BIGINT UNSIGNED,
    
    -- Timestamps
    declared_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    starts_at TIMESTAMP NOT NULL,
    ends_at TIMESTAMP NOT NULL,
    completed_at TIMESTAMP NULL,
    
    PRIMARY KEY (war_id),
    KEY idx_guilds (attacker_guild_id, defender_guild_id),
    KEY idx_status_time (status, starts_at),
    
    FOREIGN KEY (attacker_guild_id) REFERENCES guilds(guild_id),
    FOREIGN KEY (defender_guild_id) REFERENCES guilds(guild_id)
) ENGINE=InnoDB;
```

---

## Housing Tables

### player_houses
Player housing ownership and customization.

```sql
CREATE TABLE player_houses (
    house_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    owner_character_id BIGINT UNSIGNED NOT NULL,
    
    -- Location
    neighborhood_id INT UNSIGNED NOT NULL,
    plot_number INT UNSIGNED NOT NULL,
    
    -- House Details
    house_template_id INT UNSIGNED NOT NULL,
    house_name VARCHAR(50),
    
    -- Upgrades
    size_tier INT UNSIGNED DEFAULT 1,  -- 1=Small, 2=Medium, 3=Large
    storage_slots INT UNSIGNED DEFAULT 50,
    decoration_limit INT UNSIGNED DEFAULT 100,
    
    -- Status
    is_public BOOLEAN DEFAULT FALSE,
    visitor_count INT UNSIGNED DEFAULT 0,
    
    -- Maintenance
    last_maintenance TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    maintenance_due TIMESTAMP NOT NULL,
    
    -- Timestamps
    purchased_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    PRIMARY KEY (house_id),
    UNIQUE KEY idx_location (neighborhood_id, plot_number),
    KEY idx_owner (owner_character_id),
    
    FOREIGN KEY (owner_character_id) REFERENCES characters(character_id)
) ENGINE=InnoDB;
```

### house_decorations
Placed decorations in houses.

```sql
CREATE TABLE house_decorations (
    decoration_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    house_id BIGINT UNSIGNED NOT NULL,
    
    -- Item Info
    item_id INT UNSIGNED NOT NULL,
    
    -- Placement
    position_x FLOAT NOT NULL,
    position_y FLOAT NOT NULL,
    position_z FLOAT NOT NULL,
    rotation_y FLOAT DEFAULT 0,
    scale FLOAT DEFAULT 1.0,
    
    -- State
    is_interactive BOOLEAN DEFAULT FALSE,
    custom_data JSON,  -- For interactive items
    
    -- Timestamps
    placed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    PRIMARY KEY (decoration_id),
    KEY idx_house (house_id),
    
    FOREIGN KEY (house_id) REFERENCES player_houses(house_id) ON DELETE CASCADE
) ENGINE=InnoDB;
```

---

## Monitoring Tables

### player_metrics
Real-time player activity metrics.

```sql
CREATE TABLE player_metrics (
    metric_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    character_id BIGINT UNSIGNED NOT NULL,
    
    -- Activity Metrics
    actions_per_minute FLOAT NOT NULL,
    distance_traveled FLOAT NOT NULL,
    damage_dealt BIGINT UNSIGNED DEFAULT 0,
    damage_taken BIGINT UNSIGNED DEFAULT 0,
    
    -- Economy Metrics
    gold_earned BIGINT UNSIGNED DEFAULT 0,
    gold_spent BIGINT UNSIGNED DEFAULT 0,
    items_looted INT UNSIGNED DEFAULT 0,
    
    -- Social Metrics
    messages_sent INT UNSIGNED DEFAULT 0,
    trades_completed INT UNSIGNED DEFAULT 0,
    
    -- Time Window
    period_start TIMESTAMP NOT NULL,
    period_end TIMESTAMP NOT NULL,
    
    PRIMARY KEY (metric_id),
    UNIQUE KEY idx_character_period (character_id, period_start),
    KEY idx_period (period_start, period_end)
) ENGINE=InnoDB
PARTITION BY RANGE (UNIX_TIMESTAMP(period_start)) (
    PARTITION p_metrics_2024_01 VALUES LESS THAN (UNIX_TIMESTAMP('2024-02-01')),
    PARTITION p_metrics_2024_02 VALUES LESS THAN (UNIX_TIMESTAMP('2024-03-01')),
    PARTITION p_metrics_future VALUES LESS THAN MAXVALUE
);
```

### security_logs
Security events and anti-cheat detections.

```sql
CREATE TABLE security_logs (
    log_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    
    -- Event Info
    event_type ENUM('speed_hack', 'teleport', 'damage_hack', 
                    'packet_flood', 'bot_behavior', 'exploit') NOT NULL,
    severity ENUM('low', 'medium', 'high', 'critical') NOT NULL,
    
    -- Context
    character_id BIGINT UNSIGNED,
    user_id BIGINT UNSIGNED,
    ip_address VARCHAR(45),
    
    -- Details
    event_data JSON NOT NULL,
    detection_confidence FLOAT NOT NULL,  -- 0.0 to 1.0
    
    -- Action Taken
    action_taken ENUM('logged', 'warned', 'kicked', 'banned') NOT NULL,
    
    -- Timestamp
    detected_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    PRIMARY KEY (log_id),
    KEY idx_character (character_id),
    KEY idx_type_time (event_type, detected_at),
    KEY idx_severity (severity),
    
    FOREIGN KEY (character_id) REFERENCES characters(character_id),
    FOREIGN KEY (user_id) REFERENCES users(user_id)
) ENGINE=InnoDB;
```

---

## Indexing Strategy

### Primary Indexing Rules

1. **Primary Keys**: Always use `BIGINT UNSIGNED` with `AUTO_INCREMENT`
2. **Foreign Keys**: Index all foreign key columns
3. **Query Patterns**: Index columns used in WHERE, ORDER BY, GROUP BY
4. **Composite Indexes**: Order by selectivity (most selective first)

### Index Examples

```sql
-- Spatial queries
CREATE SPATIAL INDEX idx_position ON characters(position_x, position_z);

-- Time-based queries
CREATE INDEX idx_timestamp ON auction_listings(expires_at, status);

-- Search queries
CREATE FULLTEXT INDEX idx_search ON guilds(guild_name, description);

-- Covering indexes
CREATE INDEX idx_covering ON inventory(character_id, item_id, quantity);
```

### Index Maintenance

```sql
-- Analyze table statistics
ANALYZE TABLE characters;

-- Optimize fragmented tables
OPTIMIZE TABLE inventory;

-- Monitor index usage
SELECT * FROM performance_schema.table_io_waits_summary_by_index_usage
WHERE object_schema = 'mmorpg_game';
```

---

## Partitioning Strategy

### Time-Based Partitioning

```sql
-- Chat logs partitioned by month
ALTER TABLE chat_logs
PARTITION BY RANGE (UNIX_TIMESTAMP(created_at)) (
    PARTITION p_2024_01 VALUES LESS THAN (UNIX_TIMESTAMP('2024-02-01')),
    PARTITION p_2024_02 VALUES LESS THAN (UNIX_TIMESTAMP('2024-03-01')),
    PARTITION p_2024_03 VALUES LESS THAN (UNIX_TIMESTAMP('2024-04-01')),
    PARTITION p_future VALUES LESS THAN MAXVALUE
);

-- Automatic partition management
CREATE EVENT partition_maintenance
ON SCHEDULE EVERY 1 MONTH
DO
BEGIN
    -- Add new partition
    ALTER TABLE chat_logs
    ADD PARTITION (
        PARTITION p_new VALUES LESS THAN (UNIX_TIMESTAMP(DATE_ADD(NOW(), INTERVAL 2 MONTH)))
    );
    
    -- Drop old partitions (keep 6 months)
    ALTER TABLE chat_logs
    DROP PARTITION p_old;
END;
```

### Hash Partitioning

```sql
-- Distribute characters across partitions
ALTER TABLE characters
PARTITION BY HASH(character_id)
PARTITIONS 16;

-- Distribute inventory for parallel access
ALTER TABLE inventory
PARTITION BY HASH(character_id)
PARTITIONS 16;
```

---

## Data Types & Conventions

### Naming Conventions

```sql
-- Tables: snake_case, plural
CREATE TABLE auction_listings (...);

-- Columns: snake_case
character_id BIGINT UNSIGNED

-- Indexes: idx_[column(s)]
CREATE INDEX idx_user_level ON characters(user_id, level);

-- Foreign Keys: fk_[table]_[column]
CONSTRAINT fk_inventory_character 
    FOREIGN KEY (character_id) 
    REFERENCES characters(character_id)
```

### Data Type Guidelines

```sql
-- IDs: BIGINT UNSIGNED (8 bytes, 0 to 18,446,744,073,709,551,615)
user_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT

-- Strings: VARCHAR with appropriate length
username VARCHAR(32)  -- Short, indexed strings
description TEXT      -- Long, unindexed strings

-- Money: BIGINT for game currency (avoid DECIMAL for performance)
gold BIGINT UNSIGNED  -- Store as smallest unit

-- Timestamps: TIMESTAMP for MySQL features
created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP

-- Booleans: BOOLEAN (stored as TINYINT)
is_online BOOLEAN DEFAULT FALSE

-- JSON: For flexible, structured data
progress JSON
```

### Performance Best Practices

1. **Minimize NULL columns**: Use NOT NULL with defaults when possible
2. **Choose appropriate data types**: Don't use BIGINT for small numbers
3. **Avoid TEXT/BLOB in frequently accessed tables**: Store separately if needed
4. **Use ENUM for fixed sets**: More efficient than VARCHAR for status fields
5. **Batch inserts**: Use multi-value INSERT for bulk operations

```sql
-- Efficient bulk insert
INSERT INTO inventory (character_id, item_id, quantity) VALUES
    (1001, 5001, 10),
    (1001, 5002, 5),
    (1001, 5003, 1);
```

---

## Database Maintenance

### Regular Tasks

```sql
-- Daily: Update statistics
ANALYZE TABLE characters, inventory, auction_listings;

-- Weekly: Optimize fragmented tables
OPTIMIZE TABLE inventory, equipment;

-- Monthly: Archive old data
INSERT INTO chat_logs_archive 
SELECT * FROM chat_logs 
WHERE created_at < DATE_SUB(NOW(), INTERVAL 6 MONTH);

DELETE FROM chat_logs 
WHERE created_at < DATE_SUB(NOW(), INTERVAL 6 MONTH);
```

### Monitoring Queries

```sql
-- Table sizes
SELECT 
    table_name,
    ROUND(data_length / 1024 / 1024, 2) AS data_mb,
    ROUND(index_length / 1024 / 1024, 2) AS index_mb,
    table_rows
FROM information_schema.tables
WHERE table_schema = 'mmorpg_game'
ORDER BY data_length DESC;

-- Slow queries
SELECT * FROM performance_schema.events_statements_summary_by_digest
ORDER BY sum_timer_wait DESC
LIMIT 10;

-- Connection status
SHOW STATUS LIKE 'Threads_connected';
SHOW STATUS LIKE 'Max_used_connections';
```

---

## Backup Strategy

### Backup Types

```bash
# Full backup (weekly)
mysqldump --single-transaction --routines --triggers --events \
    mmorpg_game > backup_full_$(date +%Y%m%d).sql

# Incremental backup (daily)
mysqlbinlog --start-datetime="2024-01-01 00:00:00" \
    mysql-bin.* > backup_inc_$(date +%Y%m%d).sql

# Hot backup with Percona XtraBackup
xtrabackup --backup --target-dir=/backup/full
```

### Recovery Procedures

```sql
-- Point-in-time recovery
mysqlbinlog --stop-datetime="2024-01-15 12:00:00" mysql-bin.* | mysql

-- Table recovery
mysql mmorpg_game < table_backup.sql

-- Verify after recovery
CHECK TABLE characters, inventory, guilds;
```

---

## Schema Versioning

### Migration Tracking

```sql
CREATE TABLE schema_migrations (
    version INT UNSIGNED NOT NULL,
    name VARCHAR(255) NOT NULL,
    executed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    PRIMARY KEY (version)
);

-- Example migration
INSERT INTO schema_migrations (version, name) VALUES 
    (001, 'create_initial_schema'),
    (002, 'add_player_housing'),
    (003, 'add_guild_wars');
```

---

## Performance Optimization

### Query Optimization Tips

```sql
-- Use EXPLAIN to analyze queries
EXPLAIN SELECT * FROM characters WHERE level > 50 AND map_id = 100;

-- Force index usage when needed
SELECT * FROM inventory USE INDEX (idx_character_id) 
WHERE character_id = 1001;

-- Avoid SELECT *
SELECT character_id, character_name, level 
FROM characters WHERE user_id = 100;

-- Use LIMIT for large results
SELECT * FROM auction_listings 
WHERE item_id = 5001 
ORDER BY price_per_unit 
LIMIT 10;
```

### Connection Pooling

```yaml
# Recommended pool settings
connection_pool:
  min_connections: 10
  max_connections: 100
  idle_timeout: 300
  validation_query: "SELECT 1"
  test_on_borrow: true
```

---

## Contact

For database-related questions:
- DBA Team: dba@gamecompany.com
- On-call: +1-555-DBA-HELP