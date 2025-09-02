#include <gtest/gtest.h>
#include "server/game_server.h"
#include "server/login_server.h"
#include "game/systems/player_system.h"
#include "game/systems/world_system.h"
#include "proto/auth.pb.h"
#include "proto/game.pb.h"
#include <thread>
#include <future>

using namespace mmorpg;
using namespace std::chrono_literals;

// [SEQUENCE: MVP4-56] Full game flow integration tests
class GameFlowTest : public ::testing::Test {
protected:
    std::unique_ptr<LoginServer> login_server;
    std::unique_ptr<GameServer> game_server;
    std::thread login_thread;
    std::thread game_thread;
    
    void SetUp() override {
        // Start login server
        login_server = std::make_unique<LoginServer>(8080);
        login_thread = std::thread([this]() {
            login_server->Start();
        });
        
        // Start game server
        game_server = std::make_unique<GameServer>(8081);
        game_thread = std::thread([this]() {
            game_server->Start();
        });
        
        // Wait for servers to start
        std::this_thread::sleep_for(500ms);
    }
    
    void TearDown() override {
        if (login_server) {
            login_server->Stop();
        }
        if (game_server) {
            game_server->Stop();
        }
        
        if (login_thread.joinable()) {
            login_thread.join();
        }
        if (game_thread.joinable()) {
            game_thread.join();
        }
    }
    
    std::string LoginAndGetToken(const std::string& username, const std::string& password) {
        // Connect to login server
        boost::asio::io_context io;
        boost::asio::ip::tcp::socket socket(io);
        boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::address::from_string("127.0.0.1"), 
            8080
        );
        
        socket.connect(endpoint);
        
        // Send login request
        LoginRequest request;
        request.set_username(username);
        request.set_password(password);
        
        Packet packet;
        packet.set_type(PacketType::LOGIN_REQUEST);
        packet.mutable_body()->PackFrom(request);
        
        SendPacket(socket, packet);
        
        // Receive response
        Packet response = ReceivePacket(socket);
        
        LoginResponse login_response;
        response.body().UnpackTo(&login_response);
        
        socket.close();
        
        return login_response.session_token();
    }
    
    void SendPacket(boost::asio::ip::tcp::socket& socket, const Packet& packet) {
        std::string serialized = packet.SerializeAsString();
        uint32_t size = htonl(serialized.size());
        
        boost::asio::write(socket, boost::asio::buffer(&size, sizeof(size)));
        boost::asio::write(socket, boost::asio::buffer(serialized));
    }
    
    Packet ReceivePacket(boost::asio::ip::tcp::socket& socket) {
        uint32_t size;
        boost::asio::read(socket, boost::asio::buffer(&size, sizeof(size)));
        size = ntohl(size);
        
        std::vector<char> buffer(size);
        boost::asio::read(socket, boost::asio::buffer(buffer));
        
        Packet packet;
        packet.ParseFromArray(buffer.data(), size);
        return packet;
    }
};

// [SEQUENCE: MVP4-57] Test complete login and character creation flow
TEST_F(GameFlowTest, LoginAndCharacterCreation) {
    // Step 1: Login
    std::string token = LoginAndGetToken("testuser", "testpass");
    EXPECT_FALSE(token.empty());
    
    // Step 2: Connect to game server
    boost::asio::io_context io;
    boost::asio::ip::tcp::socket game_socket(io);
    boost::asio::ip::tcp::endpoint game_endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 
        8081
    );
    
    game_socket.connect(game_endpoint);
    
    // Step 3: Authenticate with game server
    GameAuthRequest auth_req;
    auth_req.set_session_token(token);
    
    Packet auth_packet;
    auth_packet.set_type(PacketType::GAME_AUTH);
    auth_packet.mutable_body()->PackFrom(auth_req);
    
    SendPacket(game_socket, auth_packet);
    
    Packet auth_response = ReceivePacket(game_socket);
    EXPECT_EQ(auth_response.type(), PacketType::GAME_AUTH_RESPONSE);
    
    // Step 4: Create character
    CreateCharacterRequest create_req;
    create_req.set_name("TestHero");
    create_req.set_class_type(CharacterClass::WARRIOR);
    create_req.set_gender(Gender::MALE);
    
    Packet create_packet;
    create_packet.set_type(PacketType::CREATE_CHARACTER);
    create_packet.mutable_body()->PackFrom(create_req);
    
    SendPacket(game_socket, create_packet);
    
    Packet create_response = ReceivePacket(game_socket);
    EXPECT_EQ(create_response.type(), PacketType::CREATE_CHARACTER_RESPONSE);
    
    CreateCharacterResponse char_response;
    create_response.body().UnpackTo(&char_response);
    EXPECT_TRUE(char_response.success());
    EXPECT_GT(char_response.character_id(), 0);
    
    game_socket.close();
}

// [SEQUENCE: MVP4-58] Test world entry and movement
TEST_F(GameFlowTest, WorldEntryAndMovement) {
    // Login and create character first
    std::string token = LoginAndGetToken("movetest", "testpass");
    
    boost::asio::io_context io;
    boost::asio::ip::tcp::socket socket(io);
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 
        8081
    );
    
    socket.connect(endpoint);
    
    // Auth
    GameAuthRequest auth_req;
    auth_req.set_session_token(token);
    
    Packet auth_packet;
    auth_packet.set_type(PacketType::GAME_AUTH);
    auth_packet.mutable_body()->PackFrom(auth_req);
    
    SendPacket(socket, auth_packet);
    ReceivePacket(socket); // Auth response
    
    // Enter world
    EnterWorldRequest enter_req;
    enter_req.set_character_id(1);
    
    Packet enter_packet;
    enter_packet.set_type(PacketType::ENTER_WORLD);
    enter_packet.mutable_body()->PackFrom(enter_req);
    
    SendPacket(socket, enter_packet);
    
    Packet enter_response = ReceivePacket(socket);
    EXPECT_EQ(enter_response.type(), PacketType::ENTER_WORLD_RESPONSE);
    
    EnterWorldResponse world_response;
    enter_response.body().UnpackTo(&world_response);
    EXPECT_TRUE(world_response.success());
    
    // Test movement
    MovementRequest move_req;
    move_req.mutable_position()->set_x(100.0f);
    move_req.mutable_position()->set_y(0.0f);
    move_req.mutable_position()->set_z(100.0f);
    move_req.mutable_velocity()->set_x(1.0f);
    move_req.mutable_velocity()->set_y(0.0f);
    move_req.mutable_velocity()->set_z(0.0f);
    
    Packet move_packet;
    move_packet.set_type(PacketType::MOVEMENT);
    move_packet.mutable_body()->PackFrom(move_req);
    
    SendPacket(socket, move_packet);
    
    // Should receive movement broadcast
    Packet broadcast = ReceivePacket(socket);
    EXPECT_EQ(broadcast.type(), PacketType::MOVEMENT_BROADCAST);
    
    socket.close();
}

// [SEQUENCE: MVP4-59] Test combat between players
TEST_F(GameFlowTest, PlayerVsPlayerCombat) {
    // Create two players
    std::string token1 = LoginAndGetToken("fighter1", "testpass");
    std::string token2 = LoginAndGetToken("fighter2", "testpass");
    
    boost::asio::io_context io1, io2;
    boost::asio::ip::tcp::socket socket1(io1), socket2(io2);
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 
        8081
    );
    
    // Connect both players
    socket1.connect(endpoint);
    socket2.connect(endpoint);
    
    // Auth and enter world for both
    auto setup_player = [this](boost::asio::ip::tcp::socket& socket, const std::string& token, uint64_t char_id) {
        GameAuthRequest auth_req;
        auth_req.set_session_token(token);
        
        Packet auth_packet;
        auth_packet.set_type(PacketType::GAME_AUTH);
        auth_packet.mutable_body()->PackFrom(auth_req);
        
        SendPacket(socket, auth_packet);
        ReceivePacket(socket); // Auth response
        
        EnterWorldRequest enter_req;
        enter_req.set_character_id(char_id);
        
        Packet enter_packet;
        enter_packet.set_type(PacketType::ENTER_WORLD);
        enter_packet.mutable_body()->PackFrom(enter_req);
        
        SendPacket(socket, enter_packet);
        ReceivePacket(socket); // Enter response
    };
    
    setup_player(socket1, token1, 1);
    setup_player(socket2, token2, 2);
    
    // Player 1 attacks Player 2
    AttackRequest attack_req;
    attack_req.set_target_id(2);
    attack_req.set_skill_id(1); // Basic attack
    
    Packet attack_packet;
    attack_packet.set_type(PacketType::ATTACK);
    attack_packet.mutable_body()->PackFrom(attack_req);
    
    SendPacket(socket1, attack_packet);
    
    // Both players should receive combat update
    Packet combat_update1 = ReceivePacket(socket1);
    Packet combat_update2 = ReceivePacket(socket2);
    
    EXPECT_EQ(combat_update1.type(), PacketType::COMBAT_UPDATE);
    EXPECT_EQ(combat_update2.type(), PacketType::COMBAT_UPDATE);
    
    CombatUpdate update;
    combat_update2.body().UnpackTo(&update);
    EXPECT_EQ(update.attacker_id(), 1);
    EXPECT_EQ(update.target_id(), 2);
    EXPECT_GT(update.damage(), 0);
    
    socket1.close();
    socket2.close();
}

// [SEQUENCE: MVP4-60] Test guild system
TEST_F(GameFlowTest, GuildCreationAndManagement) {
    std::string token = LoginAndGetToken("guildmaster", "testpass");
    
    boost::asio::io_context io;
    boost::asio::ip::tcp::socket socket(io);
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 
        8081
    );
    
    socket.connect(endpoint);
    
    // Auth and setup
    GameAuthRequest auth_req;
    auth_req.set_session_token(token);
    
    Packet auth_packet;
    auth_packet.set_type(PacketType::GAME_AUTH);
    auth_packet.mutable_body()->PackFrom(auth_req);
    
    SendPacket(socket, auth_packet);
    ReceivePacket(socket);
    
    // Create guild
    CreateGuildRequest guild_req;
    guild_req.set_guild_name("TestGuild");
    guild_req.set_guild_tag("TEST");
    
    Packet guild_packet;
    guild_packet.set_type(PacketType::CREATE_GUILD);
    guild_packet.mutable_body()->PackFrom(guild_req);
    
    SendPacket(socket, guild_packet);
    
    Packet guild_response = ReceivePacket(socket);
    EXPECT_EQ(guild_response.type(), PacketType::CREATE_GUILD_RESPONSE);
    
    CreateGuildResponse response;
    guild_response.body().UnpackTo(&response);
    EXPECT_TRUE(response.success());
    EXPECT_GT(response.guild_id(), 0);
    
    socket.close();
}

// [SEQUENCE: MVP4-61] Test chat system
TEST_F(GameFlowTest, ChatSystem) {
    // Setup two players
    std::string token1 = LoginAndGetToken("chatter1", "testpass");
    std::string token2 = LoginAndGetToken("chatter2", "testpass");
    
    boost::asio::io_context io1, io2;
    boost::asio::ip::tcp::socket socket1(io1), socket2(io2);
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 
        8081
    );
    
    socket1.connect(endpoint);
    socket2.connect(endpoint);
    
    // Auth both
    auto auth_player = [this](boost::asio::ip::tcp::socket& socket, const std::string& token) {
        GameAuthRequest auth_req;
        auth_req.set_session_token(token);
        
        Packet auth_packet;
        auth_packet.set_type(PacketType::GAME_AUTH);
        auth_packet.mutable_body()->PackFrom(auth_req);
        
        SendPacket(socket, auth_packet);
        ReceivePacket(socket);
    };
    
    auth_player(socket1, token1);
    auth_player(socket2, token2);
    
    // Player 1 sends global chat
    ChatMessage chat_msg;
    chat_msg.set_channel(ChatChannel::GLOBAL);
    chat_msg.set_message("Hello World!");
    
    Packet chat_packet;
    chat_packet.set_type(PacketType::CHAT_MESSAGE);
    chat_packet.mutable_body()->PackFrom(chat_msg);
    
    SendPacket(socket1, chat_packet);
    
    // Both players should receive the message
    Packet recv1 = ReceivePacket(socket1);
    Packet recv2 = ReceivePacket(socket2);
    
    EXPECT_EQ(recv1.type(), PacketType::CHAT_BROADCAST);
    EXPECT_EQ(recv2.type(), PacketType::CHAT_BROADCAST);
    
    ChatBroadcast broadcast;
    recv2.body().UnpackTo(&broadcast);
    EXPECT_EQ(broadcast.sender_name(), "chatter1");
    EXPECT_EQ(broadcast.message(), "Hello World!");
    EXPECT_EQ(broadcast.channel(), ChatChannel::GLOBAL);
    
    socket1.close();
    socket2.close();
}

// [SEQUENCE: MVP4-62] Test trading system
TEST_F(GameFlowTest, ItemTrading) {
    std::string token1 = LoginAndGetToken("trader1", "testpass");
    std::string token2 = LoginAndGetToken("trader2", "testpass");
    
    boost::asio::io_context io1, io2;
    boost::asio::ip::tcp::socket socket1(io1), socket2(io2);
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 
        8081
    );
    
    socket1.connect(endpoint);
    socket2.connect(endpoint);
    
    // Setup both players
    auto setup_player = [this](boost::asio::ip::tcp::socket& socket, const std::string& token) {
        GameAuthRequest auth_req;
        auth_req.set_session_token(token);
        
        Packet auth_packet;
        auth_packet.set_type(PacketType::GAME_AUTH);
        auth_packet.mutable_body()->PackFrom(auth_req);
        
        SendPacket(socket, auth_packet);
        ReceivePacket(socket);
    };
    
    setup_player(socket1, token1);
    setup_player(socket2, token2);
    
    // Player 1 initiates trade
    TradeRequest trade_req;
    trade_req.set_target_player_id(2);
    
    Packet trade_packet;
    trade_packet.set_type(PacketType::TRADE_REQUEST);
    trade_packet.mutable_body()->PackFrom(trade_req);
    
    SendPacket(socket1, trade_packet);
    
    // Player 2 receives trade request
    Packet trade_invite = ReceivePacket(socket2);
    EXPECT_EQ(trade_invite.type(), PacketType::TRADE_INVITE);
    
    // Player 2 accepts
    TradeResponse trade_resp;
    trade_resp.set_accept(true);
    trade_resp.set_trade_id(1);
    
    Packet response_packet;
    response_packet.set_type(PacketType::TRADE_RESPONSE);
    response_packet.mutable_body()->PackFrom(trade_resp);
    
    SendPacket(socket2, response_packet);
    
    // Both players receive trade window open
    Packet window1 = ReceivePacket(socket1);
    Packet window2 = ReceivePacket(socket2);
    
    EXPECT_EQ(window1.type(), PacketType::TRADE_WINDOW_OPEN);
    EXPECT_EQ(window2.type(), PacketType::TRADE_WINDOW_OPEN);
    
    socket1.close();
    socket2.close();
}

// [SEQUENCE: MVP4-63] Test disconnection handling
TEST_F(GameFlowTest, DisconnectionHandling) {
    std::string token = LoginAndGetToken("disconnect_test", "testpass");
    
    boost::asio::io_context io;
    boost::asio::ip::tcp::socket socket(io);
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 
        8081
    );
    
    socket.connect(endpoint);
    
    // Auth
    GameAuthRequest auth_req;
    auth_req.set_session_token(token);
    
    Packet auth_packet;
    auth_packet.set_type(PacketType::GAME_AUTH);
    auth_packet.mutable_body()->PackFrom(auth_req);
    
    SendPacket(socket, auth_packet);
    ReceivePacket(socket);
    
    // Get player count before disconnect
    auto player_count_before = game_server->GetPlayerCount();
    
    // Abruptly close connection
    socket.close();
    
    // Wait for server to detect disconnection
    std::this_thread::sleep_for(1s);
    
    // Player count should decrease
    auto player_count_after = game_server->GetPlayerCount();
    EXPECT_LT(player_count_after, player_count_before);
}