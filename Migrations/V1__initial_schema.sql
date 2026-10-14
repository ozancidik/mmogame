-- Initial schema for MyProject MMORPG
-- Version: 1
-- Description: Initial database schema for characters, inventory, and progression

-- Enable UUID extension
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- Characters table
CREATE TABLE characters (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    account_id UUID NOT NULL,
    name VARCHAR(32) UNIQUE NOT NULL,
    class_type VARCHAR(16) NOT NULL DEFAULT 'Warrior',
    level INT NOT NULL DEFAULT 1,
    experience BIGINT NOT NULL DEFAULT 0,
    strength INT NOT NULL DEFAULT 12,
    dexterity INT NOT NULL DEFAULT 12,
    intelligence INT NOT NULL DEFAULT 12,
    vitality INT NOT NULL DEFAULT 12,
    unspent_stat_points INT NOT NULL DEFAULT 0,
    unspent_skill_points INT NOT NULL DEFAULT 0,
    gold BIGINT NOT NULL DEFAULT 0 CHECK (gold >= 0),
    map_name VARCHAR(64) NOT NULL DEFAULT 'Summer_scene',
    location_x FLOAT NOT NULL DEFAULT 0,
    location_y FLOAT NOT NULL DEFAULT 0,
    location_z FLOAT NOT NULL DEFAULT 0,
    rotation_pitch FLOAT NOT NULL DEFAULT 0,
    rotation_yaw FLOAT NOT NULL DEFAULT 0,
    rotation_roll FLOAT NOT NULL DEFAULT 0,
    health FLOAT NOT NULL DEFAULT 100,
    max_health FLOAT NOT NULL DEFAULT 100,
    mana FLOAT NOT NULL DEFAULT 50,
    max_mana FLOAT NOT NULL DEFAULT 50,
    is_dead BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_login_at TIMESTAMPTZ
);

CREATE INDEX idx_characters_account_id ON characters(account_id);
CREATE INDEX idx_characters_name ON characters(name);

-- Inventory items table
CREATE TABLE character_inventory (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    character_id UUID NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
    item_id VARCHAR(64) NOT NULL,
    count INT NOT NULL DEFAULT 1 CHECK (count >= 0),
    slot_index INT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_inventory_character_id ON character_inventory(character_id);
CREATE UNIQUE INDEX idx_inventory_unique_slot ON character_inventory(character_id, slot_index) WHERE slot_index IS NOT NULL;

-- Equipment table
CREATE TABLE character_equipment (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    character_id UUID NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
    slot VARCHAR(16) NOT NULL, -- Weapon, Armor, Helmet, Boots
    item_id VARCHAR(64) NOT NULL,
    instance_id INT NOT NULL,
    plus_level INT NOT NULL DEFAULT 0,
    rarity VARCHAR(16) NOT NULL DEFAULT 'Common',
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE UNIQUE INDEX idx_equipment_unique_slot ON character_equipment(character_id, slot);

-- Warehouse table
CREATE TABLE character_warehouse (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    character_id UUID NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
    item_id VARCHAR(64) NOT NULL,
    instance_id INT NOT NULL,
    plus_level INT NOT NULL DEFAULT 0,
    rarity VARCHAR(16) NOT NULL DEFAULT 'Common',
    stored_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW() -- needed by update_warehouse_updated_at trigger (NEW.updated_at)
);

CREATE INDEX idx_warehouse_character_id ON character_warehouse(character_id);

-- Skills table
CREATE TABLE character_skills (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    character_id UUID NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
    skill_id VARCHAR(64) NOT NULL,
    level INT NOT NULL DEFAULT 1,
    is_on_hotbar BOOLEAN NOT NULL DEFAULT FALSE,
    hotbar_slot INT,
    learned_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_skills_character_id ON character_skills(character_id);

-- Quests table
CREATE TABLE character_quests (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    character_id UUID NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
    quest_id VARCHAR(64) NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'Active', -- Active, Completed, TurnedIn
    progress JSONB NOT NULL DEFAULT '{}',
    accepted_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    completed_at TIMESTAMPTZ,
    turned_in_at TIMESTAMPTZ
);

CREATE INDEX idx_quests_character_id ON character_quests(character_id);
CREATE UNIQUE INDEX idx_quests_unique ON character_quests(character_id, quest_id) WHERE status != 'TurnedIn';

-- Party table
CREATE TABLE parties (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    leader_id UUID NOT NULL REFERENCES characters(id),
    name VARCHAR(64),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE party_members (
    party_id UUID NOT NULL REFERENCES parties(id) ON DELETE CASCADE,
    character_id UUID NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
    joined_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (party_id, character_id)
);

-- Clan/Guild table
CREATE TABLE clans (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    name VARCHAR(32) UNIQUE NOT NULL,
    leader_id UUID NOT NULL REFERENCES characters(id),
    description TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE clan_members (
    clan_id UUID NOT NULL REFERENCES clans(id) ON DELETE CASCADE,
    character_id UUID NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
    rank VARCHAR(16) NOT NULL DEFAULT 'Member', -- Leader, Officer, Member
    joined_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (clan_id, character_id)
);

-- Friends table
CREATE TABLE friends (
    character_id UUID NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
    friend_id UUID NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
    status VARCHAR(16) NOT NULL DEFAULT 'Pending', -- Pending, Accepted, Blocked
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (character_id, friend_id)
);

-- Mail system
CREATE TABLE mail (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    sender_id UUID REFERENCES characters(id) ON DELETE SET NULL,
    recipient_id UUID NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
    subject VARCHAR(128) NOT NULL,
    body TEXT NOT NULL,
    gold_attached BIGINT NOT NULL DEFAULT 0 CHECK (gold_attached >= 0),
    items_attached JSONB NOT NULL DEFAULT '[]',
    is_read BOOLEAN NOT NULL DEFAULT FALSE,
    sent_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMPTZ NOT NULL DEFAULT (NOW() + INTERVAL '30 days')
);

CREATE INDEX idx_mail_recipient ON mail(recipient_id, is_read);

-- Auction House
CREATE TABLE auction_house (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    seller_id UUID NOT NULL REFERENCES characters(id) ON DELETE CASCADE,
    item_id VARCHAR(64) NOT NULL,
    instance_id INT NOT NULL,
    plus_level INT NOT NULL DEFAULT 0,
    rarity VARCHAR(16) NOT NULL DEFAULT 'Common',
    price BIGINT NOT NULL CHECK (price >= 0),
    quantity INT NOT NULL DEFAULT 1 CHECK (quantity >= 0),
    listed_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMPTZ NOT NULL DEFAULT (NOW() + INTERVAL '48 hours'),
    buyer_id UUID REFERENCES characters(id) ON DELETE SET NULL,
    sold_at TIMESTAMPTZ
);

CREATE INDEX idx_auction_item ON auction_house(item_id, expires_at) WHERE buyer_id IS NULL;
CREATE INDEX idx_auction_seller ON auction_house(seller_id);

-- Game events/logs
CREATE TABLE game_events (
    id BIGSERIAL PRIMARY KEY,
    event_type VARCHAR(64) NOT NULL,
    character_id UUID REFERENCES characters(id) ON DELETE SET NULL,
    data JSONB NOT NULL DEFAULT '{}',
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_game_events_type ON game_events(event_type, created_at);
CREATE INDEX idx_game_events_character ON game_events(character_id, created_at);

-- Updated timestamp trigger function
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ language 'plpgsql';

-- Apply updated_at triggers
CREATE TRIGGER update_characters_updated_at BEFORE UPDATE ON characters
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_inventory_updated_at BEFORE UPDATE ON character_inventory
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_equipment_updated_at BEFORE UPDATE ON character_equipment
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

CREATE TRIGGER update_warehouse_updated_at BEFORE UPDATE ON character_warehouse
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();