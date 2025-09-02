# MMORPG Server API Documentation

## Overview

This document provides comprehensive documentation for all network protocols, API endpoints, and message formats used in the MMORPG server engine.

## Table of Contents

1. [Network Protocol](#network-protocol)
2. [Protocol Buffer Messages](#protocol-buffer-messages)
3. [REST API Endpoints](#rest-api-endpoints)
4. [WebSocket Events](#websocket-events)
5. [Error Codes](#error-codes)
6. [Rate Limiting](#rate-limiting)

---

## Network Protocol

### Connection Overview

```
Client <--TCP/UDP--> Game Server (Port 8080)
Client <--TCP-----> Login Server (Port 8081)
Client <--TCP-----> Chat Server (Port 8082)
```

### Protocol Selection

- **TCP**: Used for reliable messages (login, chat, transactions)
- **UDP**: Used for real-time updates (movement, combat)
- **WebSocket**: Alternative for web clients

### Message Format

All messages use Protocol Buffers v3 with the following envelope:

```protobuf
message Envelope {
  uint32 message_type = 1;
  uint32 sequence_number = 2;
  uint64 timestamp = 3;
  bytes payload = 4;
  optional bytes signature = 5;  // For anti-tampering
}
```

---

## Protocol Buffer Messages

### Authentication Messages

#### LoginRequest
```protobuf
message LoginRequest {
  string username = 1;
  string password_hash = 2;  // SHA256
  string client_version = 3;
  string device_id = 4;
  optional string auth_token = 5;  // For reconnection
}
```

#### LoginResponse
```protobuf
message LoginResponse {
  enum Status {
    SUCCESS = 0;
    INVALID_CREDENTIALS = 1;
    BANNED = 2;
    SERVER_FULL = 3;
    VERSION_MISMATCH = 4;
  }
  
  Status status = 1;
  string auth_token = 2;
  string session_id = 3;
  repeated ServerInfo game_servers = 4;
  optional string error_message = 5;
}
```

### Character Management

#### CharacterListRequest
```protobuf
message CharacterListRequest {
  string session_id = 1;
}
```

#### CharacterListResponse
```protobuf
message CharacterListResponse {
  repeated CharacterInfo characters = 1;
  uint32 max_characters = 2;
}

message CharacterInfo {
  uint64 character_id = 1;
  string name = 2;
  uint32 level = 3;
  string class_type = 4;
  uint64 last_login = 5;
  string zone_name = 6;
}
```

#### CreateCharacterRequest
```protobuf
message CreateCharacterRequest {
  string name = 1;
  string class_type = 2;
  uint32 appearance_id = 3;
  CharacterCustomization customization = 4;
}

message CharacterCustomization {
  uint32 hair_style = 1;
  uint32 hair_color = 2;
  uint32 face_type = 3;
  uint32 skin_tone = 4;
  float height = 5;
  float body_type = 6;
}
```

### Movement & Position

#### MovementUpdate
```protobuf
message MovementUpdate {
  uint64 entity_id = 1;
  Vector3 position = 2;
  Vector3 velocity = 3;
  float rotation_y = 4;
  uint32 movement_flags = 5;  // Walking, Running, Jumping, etc.
  uint64 timestamp = 6;
  uint32 sequence = 7;  // For client prediction
}

message Vector3 {
  float x = 1;
  float y = 2;
  float z = 3;
}
```

#### PositionCorrection
```protobuf
message PositionCorrection {
  uint64 entity_id = 1;
  Vector3 server_position = 2;
  Vector3 server_velocity = 3;
  uint32 acknowledge_sequence = 4;
  uint64 server_timestamp = 5;
}
```

### Combat Messages

#### AttackRequest
```protobuf
message AttackRequest {
  uint64 attacker_id = 1;
  uint64 target_id = 2;
  uint32 skill_id = 3;
  Vector3 cast_position = 4;
  float cast_rotation = 5;
  uint64 client_timestamp = 6;
}
```

#### CombatEvent
```protobuf
message CombatEvent {
  enum EventType {
    DAMAGE = 0;
    HEAL = 1;
    MISS = 2;
    DODGE = 3;
    BLOCK = 4;
    CRITICAL = 5;
    DEATH = 6;
  }
  
  EventType type = 1;
  uint64 source_id = 2;
  uint64 target_id = 3;
  int32 value = 4;
  uint32 skill_id = 5;
  repeated StatusEffect effects = 6;
  uint64 timestamp = 7;
}
```

### Chat Messages

#### ChatMessage
```protobuf
message ChatMessage {
  enum Channel {
    GENERAL = 0;
    PARTY = 1;
    GUILD = 2;
    WHISPER = 3;
    TRADE = 4;
    ZONE = 5;
    SYSTEM = 6;
  }
  
  Channel channel = 1;
  uint64 sender_id = 2;
  string sender_name = 3;
  string message = 4;
  uint64 timestamp = 5;
  optional uint64 recipient_id = 6;  // For whisper
  optional string guild_name = 7;
}
```

### Guild Operations

#### CreateGuildRequest
```protobuf
message CreateGuildRequest {
  string guild_name = 1;
  string guild_tag = 2;  // 3-5 characters
  uint32 emblem_id = 3;
  string description = 4;
}
```

#### GuildWarDeclaration
```protobuf
message GuildWarDeclaration {
  uint64 declaring_guild_id = 1;
  uint64 target_guild_id = 2;
  WarTerms terms = 3;
  uint64 start_time = 4;
  uint64 duration = 5;
}

message WarTerms {
  enum WarType {
    TOTAL_WAR = 0;
    OBJECTIVE_BASED = 1;
    SCHEDULED_BATTLE = 2;
  }
  
  WarType type = 1;
  repeated Objective objectives = 2;
  uint32 max_participants = 3;
  repeated string allowed_zones = 4;
}
```

### Trading

#### TradeRequest
```protobuf
message TradeRequest {
  uint64 initiator_id = 1;
  uint64 target_id = 2;
}

message TradeUpdate {
  uint64 trade_id = 1;
  uint64 player_id = 2;
  repeated TradeItem items = 3;
  uint64 gold_amount = 4;
  bool locked = 5;
}

message TradeItem {
  uint64 item_id = 1;
  uint32 quantity = 2;
  uint32 slot_index = 3;
}
```

### World Events

#### ZoneEventNotification
```protobuf
message ZoneEventNotification {
  enum EventType {
    WORLD_BOSS = 0;
    TREASURE_HUNT = 1;
    PVP_TOURNAMENT = 2;
    INVASION = 3;
    WEATHER_CHANGE = 4;
  }
  
  EventType type = 1;
  string zone_name = 2;
  string event_name = 3;
  string description = 4;
  uint64 start_time = 5;
  uint64 end_time = 6;
  map<string, string> event_data = 7;
}
```

---

## REST API Endpoints

### Health & Monitoring

#### GET /health
Health check endpoint for load balancers.

**Response:**
```json
{
  "status": "healthy",
  "version": "1.2.0",
  "uptime": 3600,
  "connections": 4523,
  "server_time": 1643723400
}
```

#### GET /metrics
Prometheus-compatible metrics endpoint.

**Response (Prometheus format):**
```
# HELP game_server_connections Current number of connections
# TYPE game_server_connections gauge
game_server_connections 4523

# HELP game_server_requests_total Total number of requests
# TYPE game_server_requests_total counter
game_server_requests_total{method="login"} 12543
```

### Admin API

#### GET /admin/players
Get online player statistics.

**Headers:**
```
Authorization: Bearer <admin_token>
```

**Response:**
```json
{
  "total_online": 4523,
  "by_zone": {
    "starter_zone": 523,
    "capital_city": 1245,
    "pvp_arena": 234
  },
  "by_level": {
    "1-10": 456,
    "11-20": 789,
    "21-30": 654
  }
}
```

#### POST /admin/broadcast
Send server-wide message.

**Request:**
```json
{
  "message": "Server maintenance in 30 minutes",
  "type": "warning",
  "duration": 1800
}
```

### Game Data API

#### GET /api/v1/items/{item_id}
Get item information.

**Response:**
```json
{
  "item_id": 12345,
  "name": "Legendary Sword",
  "type": "weapon",
  "rarity": "legendary",
  "stats": {
    "damage": 150,
    "critical_chance": 0.15,
    "attack_speed": 1.2
  },
  "requirements": {
    "level": 50,
    "class": ["warrior", "paladin"]
  }
}
```

#### GET /api/v1/leaderboard/{category}
Get leaderboard data.

**Categories:** `level`, `pvp`, `guild`, `achievement`

**Response:**
```json
{
  "category": "pvp",
  "updated_at": 1643723400,
  "entries": [
    {
      "rank": 1,
      "player_name": "DragonSlayer",
      "score": 2450,
      "wins": 523,
      "losses": 45
    }
  ]
}
```

---

## WebSocket Events

### Connection
```javascript
// Client -> Server
{
  "event": "connect",
  "data": {
    "session_id": "abc123",
    "character_id": 12345
  }
}

// Server -> Client
{
  "event": "connected",
  "data": {
    "server_time": 1643723400,
    "tick_rate": 30
  }
}
```

### Real-time Updates
```javascript
// Server -> Client (Batch Update)
{
  "event": "world_update",
  "data": {
    "entities": [
      {
        "id": 12345,
        "position": {"x": 100, "y": 0, "z": 50},
        "health": 85
      }
    ],
    "removed_entities": [67890],
    "tick": 18293847
  }
}
```

---

## Error Codes

### General Errors (1000-1999)
- `1001`: Invalid request format
- `1002`: Missing required field
- `1003`: Invalid session
- `1004`: Rate limit exceeded
- `1005`: Server maintenance

### Authentication Errors (2000-2999)
- `2001`: Invalid credentials
- `2002`: Account banned
- `2003`: Account locked
- `2004`: Session expired
- `2005`: Invalid token

### Game Errors (3000-3999)
- `3001`: Character not found
- `3002`: Invalid target
- `3003`: Out of range
- `3004`: On cooldown
- `3005`: Insufficient resources
- `3006`: Inventory full
- `3007`: Invalid item
- `3008`: Cannot trade

### Combat Errors (4000-4999)
- `4001`: Target immune
- `4002`: Invalid skill
- `4003`: Not enough mana
- `4004`: Target dead
- `4005`: In safe zone

---

## Rate Limiting

### Limits by Message Type

| Message Type | Limit | Window |
|-------------|-------|---------|
| Movement | 30/sec | 1 second |
| Chat | 5/sec | 10 seconds |
| Combat | 10/sec | 1 second |
| Trade | 1/sec | 5 seconds |
| Guild | 1/min | 60 seconds |

### Rate Limit Headers

```
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 45
X-RateLimit-Reset: 1643723460
```

### Exceeding Limits

When rate limit is exceeded:
```json
{
  "error": {
    "code": 1004,
    "message": "Rate limit exceeded",
    "retry_after": 5
  }
}
```

---

## Compression

### Message Compression

Messages larger than 1KB are automatically compressed using zstd.

**Compression Header:**
```protobuf
message CompressedEnvelope {
  uint32 compression_type = 1;  // 1=zstd, 2=lz4
  uint32 uncompressed_size = 2;
  bytes compressed_data = 3;
}
```

---

## Security

### Message Signing

Critical messages (trades, guild operations) include HMAC-SHA256 signatures:

```
signature = HMAC-SHA256(session_key, message_bytes)
```

### Encryption

All TCP connections use TLS 1.3. UDP packets for sensitive data use AES-128-GCM.

---

## Example Client Implementation

### Python Example
```python
import asyncio
import struct
from game_protocol_pb2 import *

class GameClient:
    async def connect(self, host, port):
        self.reader, self.writer = await asyncio.open_connection(host, port)
        
    async def login(self, username, password):
        request = LoginRequest()
        request.username = username
        request.password_hash = hashlib.sha256(password.encode()).hexdigest()
        request.client_version = "1.2.0"
        
        await self.send_message(MessageType.LOGIN_REQUEST, request)
        response = await self.receive_message()
        
        return response
        
    async def send_message(self, msg_type, message):
        data = message.SerializeToString()
        envelope = Envelope()
        envelope.message_type = msg_type
        envelope.sequence_number = self.next_sequence()
        envelope.timestamp = int(time.time() * 1000)
        envelope.payload = data
        
        envelope_data = envelope.SerializeToString()
        length = struct.pack('!I', len(envelope_data))
        
        self.writer.write(length + envelope_data)
        await self.writer.drain()
```

### JavaScript/WebSocket Example
```javascript
class GameClient {
  constructor() {
    this.ws = new WebSocket('wss://game.example.com/ws');
    this.ws.binaryType = 'arraybuffer';
  }
  
  login(username, password) {
    const request = {
      username: username,
      passwordHash: sha256(password),
      clientVersion: '1.2.0'
    };
    
    this.send('login', request);
  }
  
  send(event, data) {
    const message = {
      event: event,
      data: data,
      timestamp: Date.now()
    };
    
    this.ws.send(JSON.stringify(message));
  }
}
```

---

## Performance Considerations

### Batching
Multiple updates are batched into single packets:
- Movement updates: 100ms window
- Combat events: 50ms window
- Entity updates: 33ms window (30 FPS)

### Delta Compression
Only changed fields are sent for entity updates:
```protobuf
message EntityDelta {
  uint64 entity_id = 1;
  optional Vector3 position = 2;
  optional float health = 3;
  optional uint32 state_flags = 4;
}
```

### Interest Management
Clients only receive updates for entities within:
- View distance: 150 units
- Extended distance for party/guild: 300 units
- Global events: Zone-wide

---

## Backwards Compatibility

### Version Negotiation
```protobuf
message VersionInfo {
  uint32 protocol_version = 1;  // Current: 3
  repeated uint32 supported_versions = 2;
  map<string, string> feature_flags = 3;
}
```

### Deprecated Fields
Fields marked with `[deprecated = true]` will be removed in next major version.

---

## Contact

For API questions or issues:
- GitHub Issues: https://github.com/username/mmorpg-server/issues
- API Status: https://status.gameserver.com