# Modern C++ Containers and Utilities (C++17/20/23)

## Overview
Modern C++ has introduced several new container types and utility classes that provide safer, more expressive, and more efficient ways to work with data. This document covers the essential modern C++ containers and utilities specifically useful for multiplayer game server development.

## std::optional (C++17)

### Basic Usage
```cpp
#include <optional>

class Player {
public:
    std::optional<WeaponData> getEquippedWeapon() const {
        if (hasWeapon) {
            return weaponData;
        }
        return std::nullopt;  // No weapon equipped
    }
    
    void processPlayerAction(PlayerId id) {
        auto weapon = getEquippedWeapon();
        if (weapon) {  // Check if weapon exists
            weapon->fire();
        }
    }
    
private:
    bool hasWeapon = false;
    WeaponData weaponData;
};
```

### Network Protocol with Optional Values
```cpp
struct PlayerState {
    PlayerId id;
    Position position;
    std::optional<VehicleId> vehicle;  // Player may not be in a vehicle
    std::optional<TeamId> team;        // Player may not be on a team
    
    // Serialize with optional handling
    void serialize(BinaryWriter& writer) const {
        writer.write(id);
        writer.write(position);
        
        writer.write(vehicle.has_value());
        if (vehicle) {
            writer.write(*vehicle);
        }
        
        writer.write(team.has_value());
        if (team) {
            writer.write(*team);
        }
    }
};
```

### Game Configuration with Defaults
```cpp
class GameConfig {
public:
    std::optional<int> maxPlayers;
    std::optional<float> matchDuration;
    std::optional<std::string> mapName;
    
    // Get value with fallback
    int getMaxPlayers() const {
        return maxPlayers.value_or(32);  // Default to 32
    }
    
    float getMatchDuration() const {
        return matchDuration.value_or(600.0f);  // Default 10 minutes
    }
    
    std::string getMapName() const {
        return mapName.value_or("default_map");
    }
};
```

## std::variant (C++17)

### Game Event System
```cpp
#include <variant>

struct PlayerJoinEvent {
    PlayerId playerId;
    std::string playerName;
};

struct PlayerLeaveEvent {
    PlayerId playerId;
    std::string reason;
};

struct GameStartEvent {
    GameId gameId;
    std::vector<PlayerId> players;
};

struct ChatMessageEvent {
    PlayerId senderId;
    std::string message;
    ChatChannel channel;
};

// Variant to hold different event types
using GameEvent = std::variant<
    PlayerJoinEvent,
    PlayerLeaveEvent,
    GameStartEvent,
    ChatMessageEvent
>;

class EventProcessor {
public:
    void processEvent(const GameEvent& event) {
        std::visit([this](const auto& e) {
            handleEvent(e);
        }, event);
    }
    
private:
    void handleEvent(const PlayerJoinEvent& event) {
        logger.info("Player {} joined", event.playerName);
        // Handle player join logic
    }
    
    void handleEvent(const PlayerLeaveEvent& event) {
        logger.info("Player {} left: {}", event.playerId, event.reason);
        // Handle player leave logic
    }
    
    void handleEvent(const GameStartEvent& event) {
        logger.info("Game {} started with {} players", 
                   event.gameId, event.players.size());
        // Handle game start logic
    }
    
    void handleEvent(const ChatMessageEvent& event) {
        broadcastMessage(event.senderId, event.message, event.channel);
    }
};
```

### Network Message Types
```cpp
struct LoginMessage {
    std::string username;
    std::string password;
};

struct MoveMessage {
    PlayerId playerId;
    Position newPosition;
    float timestamp;
};

struct AttackMessage {
    PlayerId attackerId;
    PlayerId targetId;
    WeaponType weapon;
};

using NetworkMessage = std::variant<
    LoginMessage,
    MoveMessage,
    AttackMessage
>;

class MessageHandler {
public:
    void handleMessage(const NetworkMessage& msg) {
        std::visit(overload {
            [this](const LoginMessage& m) { handleLogin(m); },
            [this](const MoveMessage& m) { handleMove(m); },
            [this](const AttackMessage& m) { handleAttack(m); }
        }, msg);
    }
    
private:
    // Helper for visitor pattern
    template<class... Ts> 
    struct overload : Ts... { 
        using Ts::operator()...; 
    };
    template<class... Ts> 
    overload(Ts...) -> overload<Ts...>;
};
```

## std::string_view (C++17)

### Efficient String Processing
```cpp
#include <string_view>

class CommandProcessor {
public:
    // No string copying for read-only operations
    bool isValidCommand(std::string_view command) const {
        return command.starts_with("/") && command.length() > 1;
    }
    
    std::pair<std::string_view, std::string_view> 
    parseCommand(std::string_view fullCommand) const {
        if (!isValidCommand(fullCommand)) {
            return {"", ""};
        }
        
        auto spacePos = fullCommand.find(' ');
        if (spacePos == std::string_view::npos) {
            return {fullCommand.substr(1), ""};  // Remove '/' prefix
        }
        
        return {
            fullCommand.substr(1, spacePos - 1),     // Command
            fullCommand.substr(spacePos + 1)         // Arguments
        };
    }
    
    void processCommand(std::string_view command, std::string_view args) {
        if (command == "kick") {
            handleKickCommand(args);
        } else if (command == "ban") {
            handleBanCommand(args);
        } else if (command == "broadcast") {
            handleBroadcastCommand(args);
        }
    }
    
private:
    void handleKickCommand(std::string_view playerName) {
        // Process kick without string allocation
        logger.info("Kicking player: {}", playerName);
    }
};
```

### Configuration File Parser
```cpp
class ConfigParser {
public:
    std::unordered_map<std::string, std::string> 
    parseConfig(std::string_view configData) {
        std::unordered_map<std::string, std::string> config;
        
        size_t pos = 0;
        while (pos < configData.length()) {
            auto lineEnd = configData.find('\n', pos);
            if (lineEnd == std::string_view::npos) {
                lineEnd = configData.length();
            }
            
            auto line = configData.substr(pos, lineEnd - pos);
            parseLine(line, config);
            
            pos = lineEnd + 1;
        }
        
        return config;
    }
    
private:
    void parseLine(std::string_view line, 
                   std::unordered_map<std::string, std::string>& config) {
        // Skip empty lines and comments
        if (line.empty() || line.starts_with("#")) {
            return;
        }
        
        auto equalPos = line.find('=');
        if (equalPos == std::string_view::npos) {
            return;
        }
        
        auto key = trim(line.substr(0, equalPos));
        auto value = trim(line.substr(equalPos + 1));
        
        config[std::string(key)] = std::string(value);
    }
    
    std::string_view trim(std::string_view str) {
        auto start = str.find_first_not_of(" \t");
        if (start == std::string_view::npos) {
            return "";
        }
        
        auto end = str.find_last_not_of(" \t");
        return str.substr(start, end - start + 1);
    }
};
```

## std::span (C++20)

### Buffer Management
```cpp
#include <span>
#include <array>

class NetworkBuffer {
private:
    std::array<uint8_t, 4096> buffer;
    size_t dataSize = 0;
    
public:
    // Provide view of valid data without copying
    std::span<const uint8_t> getValidData() const {
        return std::span(buffer.data(), dataSize);
    }
    
    // Provide writable view for appending data
    std::span<uint8_t> getWritableSpace() {
        return std::span(buffer.data() + dataSize, buffer.size() - dataSize);
    }
    
    void appendData(std::span<const uint8_t> newData) {
        auto writeSpace = getWritableSpace();
        if (newData.size() > writeSpace.size()) {
            throw std::runtime_error("Not enough buffer space");
        }
        
        std::copy(newData.begin(), newData.end(), writeSpace.begin());
        dataSize += newData.size();
    }
};

class PacketProcessor {
public:
    // Process packet data without copying
    void processPacket(std::span<const uint8_t> packetData) {
        if (packetData.size() < sizeof(PacketHeader)) {
            return;  // Invalid packet
        }
        
        auto header = readHeader(packetData.first<sizeof(PacketHeader)>());
        auto payload = packetData.subspan(sizeof(PacketHeader));
        
        processPayload(header, payload);
    }
    
private:
    PacketHeader readHeader(std::span<const uint8_t, sizeof(PacketHeader)> headerData) {
        PacketHeader header;
        std::memcpy(&header, headerData.data(), sizeof(PacketHeader));
        return header;
    }
    
    void processPayload(const PacketHeader& header, std::span<const uint8_t> payload) {
        switch (header.type) {
            case PacketType::PLAYER_MOVE:
                processPlayerMove(payload);
                break;
            case PacketType::PLAYER_ATTACK:
                processPlayerAttack(payload);
                break;
        }
    }
};
```

## std::any (C++17)

### Plugin System with Dynamic Types
```cpp
#include <any>
#include <unordered_map>

class PluginRegistry {
private:
    std::unordered_map<std::string, std::any> plugins;
    
public:
    template<typename T>
    void registerPlugin(const std::string& name, T plugin) {
        plugins[name] = std::make_any<T>(std::move(plugin));
    }
    
    template<typename T>
    T* getPlugin(const std::string& name) {
        auto it = plugins.find(name);
        if (it == plugins.end()) {
            return nullptr;
        }
        
        try {
            return std::any_cast<T>(&it->second);
        } catch (const std::bad_any_cast&) {
            return nullptr;
        }
    }
    
    void executePlugin(const std::string& name, const std::string& method) {
        auto it = plugins.find(name);
        if (it == plugins.end()) {
            return;
        }
        
        // Try different plugin interfaces
        if (auto* gamePlugin = std::any_cast<GamePlugin>(&it->second)) {
            gamePlugin->execute(method);
        } else if (auto* networkPlugin = std::any_cast<NetworkPlugin>(&it->second)) {
            networkPlugin->handleNetwork(method);
        }
    }
};
```

## std::format (C++20)

### Logging and Message Formatting
```cpp
#include <format>

class Logger {
public:
    template<typename... Args>
    void info(const std::string& format, Args&&... args) {
        auto message = std::format("[INFO] {}: {}", 
                                 getCurrentTimestamp(),
                                 std::format(format, std::forward<Args>(args)...));
        writeToLog(message);
    }
    
    template<typename... Args>
    void error(const std::string& format, Args&&... args) {
        auto message = std::format("[ERROR] {}: {}", 
                                 getCurrentTimestamp(),
                                 std::format(format, std::forward<Args>(args)...));
        writeToLog(message);
    }
    
    // Game-specific formatted logging
    void logPlayerAction(PlayerId playerId, const std::string& action, 
                        Position pos, float timestamp) {
        auto message = std::format("Player {} performed {} at ({:.2f}, {:.2f}) @ {:.3f}",
                                 playerId, action, pos.x, pos.y, timestamp);
        info(message);
    }
    
    void logNetworkStats(size_t bytesIn, size_t bytesOut, 
                        double latency, size_t playerCount) {
        auto message = std::format("Network: In={}KB, Out={}KB, Latency={:.1f}ms, Players={}",
                                 bytesIn / 1024, bytesOut / 1024, latency, playerCount);
        info(message);
    }
};
```

## std::jthread (C++20)

### Cooperative Thread Management
```cpp
#include <thread>
#include <stop_token>

class NetworkThread {
private:
    std::jthread networkThread;
    std::atomic<bool> running{true};
    
public:
    void start() {
        networkThread = std::jthread([this](std::stop_token stopToken) {
            networkLoop(stopToken);
        });
    }
    
    void stop() {
        running = false;
        networkThread.request_stop();
        if (networkThread.joinable()) {
            networkThread.join();
        }
    }
    
private:
    void networkLoop(std::stop_token stopToken) {
        while (running && !stopToken.stop_requested()) {
            // Process network events
            processIncomingData();
            processSendQueue();
            
            // Cooperative cancellation point
            if (stopToken.stop_requested()) {
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        cleanup();
    }
    
    void processIncomingData() {
        // Handle incoming network data
    }
    
    void processSendQueue() {
        // Handle outgoing network data
    }
    
    void cleanup() {
        // Clean up network resources
    }
};
```

## Coroutines (C++20)

### Async Game Operations
```cpp
#include <coroutine>
#include <future>

template<typename T>
struct Task {
    struct promise_type {
        T value;
        
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        
        void return_value(T v) {
            value = std::move(v);
        }
        
        void unhandled_exception() {
            std::terminate();
        }
    };
    
    std::coroutine_handle<promise_type> coro;
    
    Task(std::coroutine_handle<promise_type> h) : coro(h) {}
    ~Task() { if (coro) coro.destroy(); }
    
    T get() {
        return coro.promise().value;
    }
};

class GameServer {
public:
    Task<bool> authenticatePlayerAsync(const std::string& token) {
        // Simulate async database lookup
        auto result = co_await databaseLookup(token);
        
        if (result.isValid) {
            co_await updateLastLogin(result.playerId);
            co_return true;
        }
        
        co_return false;
    }
    
    Task<PlayerStats> loadPlayerStatsAsync(PlayerId playerId) {
        auto basicStats = co_await loadBasicStats(playerId);
        auto achievements = co_await loadAchievements(playerId);
        auto inventory = co_await loadInventory(playerId);
        
        PlayerStats stats;
        stats.basic = basicStats;
        stats.achievements = std::move(achievements);
        stats.inventory = std::move(inventory);
        
        co_return stats;
    }
    
private:
    Task<DatabaseResult> databaseLookup(const std::string& token);
    Task<void> updateLastLogin(PlayerId playerId);
    Task<BasicStats> loadBasicStats(PlayerId playerId);
    Task<std::vector<Achievement>> loadAchievements(PlayerId playerId);
    Task<Inventory> loadInventory(PlayerId playerId);
};
```

## Concepts (C++20)

### Type Constraints for Game Components
```cpp
#include <concepts>
#include <type_traits>

// Define concepts for game components
template<typename T>
concept GameComponent = requires(T component) {
    { component.getId() } -> std::convertible_to<ComponentId>;
    { component.update(0.0f) } -> std::same_as<void>;
    { component.isActive() } -> std::convertible_to<bool>;
};

template<typename T>
concept NetworkSerializable = requires(T obj, BinaryWriter& writer, BinaryReader& reader) {
    { obj.serialize(writer) } -> std::same_as<void>;
    { T::deserialize(reader) } -> std::same_as<T>;
    { obj.getSerializedSize() } -> std::convertible_to<size_t>;
};

template<typename T>
concept GameEntity = requires(T entity) {
    { entity.getEntityId() } -> std::convertible_to<EntityId>;
    { entity.getPosition() } -> std::convertible_to<Position>;
    { entity.setPosition(Position{}) } -> std::same_as<void>;
} && GameComponent<T>;

// Use concepts in templates
template<GameComponent T>
class ComponentManager {
public:
    void addComponent(std::unique_ptr<T> component) {
        components.push_back(std::move(component));
    }
    
    void updateAll(float deltaTime) {
        for (auto& component : components) {
            if (component->isActive()) {
                component->update(deltaTime);
            }
        }
    }
    
private:
    std::vector<std::unique_ptr<T>> components;
};

template<NetworkSerializable T>
class NetworkManager {
public:
    void sendObject(const T& obj, ConnectionId connId) {
        BinaryWriter writer;
        obj.serialize(writer);
        sendData(writer.getData(), connId);
    }
    
    std::optional<T> receiveObject(ConnectionId connId) {
        auto data = receiveData(connId);
        if (data.empty()) {
            return std::nullopt;
        }
        
        BinaryReader reader(data);
        return T::deserialize(reader);
    }
};
```

## Ranges (C++20)

### Data Processing Pipelines
```cpp
#include <ranges>
#include <algorithm>

class PlayerManager {
private:
    std::vector<Player> players;
    
public:
    // Find active players with high score using ranges
    auto getTopActivePlayers(int count) {
        return players
            | std::views::filter([](const Player& p) { return p.isActive(); })
            | std::views::transform([](const Player& p) { return std::pair{p.getId(), p.getScore()}; })
            | std::views::take(count);
    }
    
    // Get players in specific area
    auto getPlayersInArea(const BoundingBox& area) {
        return players
            | std::views::filter([&area](const Player& p) {
                return area.contains(p.getPosition());
            })
            | std::views::transform([](const Player& p) -> PlayerId { return p.getId(); });
    }
    
    // Calculate team statistics
    std::map<TeamId, TeamStats> calculateTeamStats() {
        std::map<TeamId, TeamStats> teamStats;
        
        auto teamGroups = players
            | std::views::filter([](const Player& p) { return p.hasTeam(); })
            | std::views::chunk_by([](const Player& a, const Player& b) {
                return a.getTeamId() == b.getTeamId();
            });
        
        for (auto team : teamGroups) {
            if (team.empty()) continue;
            
            TeamId teamId = team.front().getTeamId();
            TeamStats& stats = teamStats[teamId];
            
            stats.playerCount = std::distance(team.begin(), team.end());
            stats.totalScore = std::ranges::fold_left(
                team | std::views::transform([](const Player& p) { return p.getScore(); }),
                0, std::plus<>{}
            );
            stats.averageScore = stats.totalScore / stats.playerCount;
        }
        
        return teamStats;
    }
};
```

## Best Practices and Performance Considerations

### 1. Use std::string_view for Read-Only String Operations
```cpp
// Good: No unnecessary string copies
void processCommand(std::string_view command);

// Avoid: Creates temporary strings
void processCommand(const std::string& command);
```

### 2. Prefer std::optional over Nullable Pointers
```cpp
// Good: Clear ownership and null-safety
std::optional<PlayerData> findPlayer(PlayerId id);

// Avoid: Unclear ownership
PlayerData* findPlayer(PlayerId id);  // Who owns this? Can it be null?
```

### 3. Use std::variant for Type-Safe Unions
```cpp
// Good: Type-safe, no undefined behavior
std::variant<int, float, std::string> ConfigValue;

// Avoid: Potential undefined behavior
union ConfigValue {
    int intVal;
    float floatVal;
    char stringVal[256];  // Fixed size, potential overflow
};
```

### 4. Leverage std::span for Safe Buffer Access
```cpp
// Good: Bounds checking, no array decay
void processBuffer(std::span<const uint8_t> buffer);

// Avoid: No size information, potential buffer overrun
void processBuffer(const uint8_t* buffer, size_t size);
```

### 5. Use Concepts for Clear Template Constraints
```cpp
// Good: Clear requirements and better error messages
template<NetworkSerializable T>
void sendMessage(const T& message);

// Avoid: Unclear requirements, cryptic error messages
template<typename T>
void sendMessage(const T& message);
```

## Integration with Game Server Architecture

### Entity Component System with Modern C++
```cpp
class ModernECS {
public:
    template<GameComponent T, typename... Args>
    ComponentId addComponent(EntityId entity, Args&&... args) {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        ComponentId id = component->getId();
        
        entityComponents[entity].emplace_back(std::move(component));
        return id;
    }
    
    template<GameComponent T>
    std::optional<T*> getComponent(EntityId entity) {
        auto it = entityComponents.find(entity);
        if (it == entityComponents.end()) {
            return std::nullopt;
        }
        
        for (auto& component : it->second) {
            if (auto* typed = dynamic_cast<T*>(component.get())) {
                return typed;
            }
        }
        
        return std::nullopt;
    }
    
    void updateSystems(float deltaTime) {
        // Update all active components
        for (auto& [entityId, components] : entityComponents) {
            for (auto& component : components) {
                if (component->isActive()) {
                    component->update(deltaTime);
                }
            }
        }
    }
    
private:
    std::unordered_map<EntityId, std::vector<std::unique_ptr<GameComponent>>> entityComponents;
};
```

This comprehensive coverage of modern C++ containers and utilities provides the foundation needed for efficient, safe, and expressive multiplayer game server development using the latest C++ features.