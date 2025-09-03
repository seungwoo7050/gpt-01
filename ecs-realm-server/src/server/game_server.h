#pragma once

#include "core/concurrent/lock_free_queue.h"
#include "core/ecs/world.h"
#include "network/packet_handler.h"
#include "network/tcp_server.h"
#include "game/social/guild_system.h"
#include "game/pvp/pvp_system.h"
#include <memory>
#include <atomic>

// [SEQUENCE: MVP1-81] The main game server class that orchestrates all major systems.
namespace mmorpg::server {

class GameServer {
public:
    GameServer();
    ~GameServer();

    void Start();
    void Stop();

    // [SEQUENCE: MVP1-83] The main `Run()` method containing the core game loop.
    void Run();

private:
    // [SEQUENCE: MVP1-85] `ProcessIncomingPackets()` method to dequeue and handle packets.
    void ProcessIncomingPackets();

    std::atomic<bool> running_;
    
    // [SEQUENCE: MVP1-86] Integration with the ECS `World` for system updates.
    std::unique_ptr<core::ecs::World> world_;
    std::shared_ptr<core::network::PacketHandler> packet_handler_;
    std::unique_ptr<core::network::TcpServer> tcp_server_;
    std::shared_ptr<core::monitoring::ServerMonitor> monitor_;
    std::shared_ptr<database::DatabaseManager> db_manager_;
    std::shared_ptr<cache::SessionManager> session_manager_;
    std::shared_ptr<auth::AuthService> auth_service_;

    // [SEQUENCE: MVP1-84] A lock-free MPSC queue (`MpscLfQueue`) to receive packets from network threads.
    core::concurrent::MpscLfQueue<std::pair<uint64_t, std::string>> packet_queue_;

    // [SEQUENCE: MVP1-87] Tick-rate management to ensure a consistent simulation speed.
    static constexpr int TICKS_PER_SECOND = 30;
    static constexpr std::chrono::milliseconds TICK_INTERVAL{1000 / TICKS_PER_SECOND};
};

} // namespace mmorpg::server
