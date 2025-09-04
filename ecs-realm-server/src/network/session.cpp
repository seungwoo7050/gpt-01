#include "network/session.h"
#include "proto/packet.pb.h"
#include "network/packet_serializer.h"
#include "network/packet_handler.h"
#include "core/logger.h"

#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>

#include <iostream>
#include <arpa/inet.h>

namespace mmorpg::network {

// Factory to create message instances from a type name string.
std::unique_ptr<google::protobuf::Message> createMessage(const std::string& type_name) {
    const google::protobuf::Descriptor* descriptor = google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(type_name);
    if (descriptor) {
        const google::protobuf::Message* prototype = google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
        if (prototype) {
            return std::unique_ptr<google::protobuf::Message>(prototype->New());
        }
    }
    return nullptr;
}

// [SEQUENCE: MVP9-3] Modified Session constructor to take socket and context
Session::Session(tcp::socket socket, boost::asio::ssl::context& context, uint32_t session_id, std::shared_ptr<IPacketHandler> handler)
    : m_ssl_stream(std::move(socket), context),
      m_strand(boost::asio::make_strand(m_ssl_stream.get_executor())),
      m_state(SessionState::Connecting),
      m_sessionId(session_id),
      m_packetHandler(std::move(handler)),
      m_isAuthenticated(false) {}

Session::~Session() {
    LOG_INFO("Session {} destroyed.", m_sessionId);
}

// [SEQUENCE: MVP9-4] Modified Start() to initiate the SSL handshake.
void Session::Start() {
    DoHandshake();
}

void Session::Disconnect() {
    if (m_state == SessionState::Disconnected) return;
    m_state = SessionState::Disconnected;

    boost::asio::post(m_strand, [self = shared_from_this()]() {
        boost::system::error_code ec;
        self->m_ssl_stream.async_shutdown([self]([[maybe_unused]] const boost::system::error_code& shutdown_ec) {
            boost::system::error_code close_ec;
            self->GetSocket().shutdown(tcp::socket::shutdown_both, close_ec);
            self->GetSocket().close(close_ec);
        });
    });
}

void Session::Send(const google::protobuf::Message& message) {
    auto buffer = PacketSerializer::Serialize(message);
    if (buffer.empty()) return;

    boost::asio::post(m_strand, [this, buffer = std::move(buffer)]() {
        bool write_in_progress = !m_writeQueue.empty();
        m_writeQueue.push_back(std::move(buffer));
        if (!write_in_progress) {
            DoWrite();
        }
    });
}

std::string Session::GetRemoteAddress() const {
    boost::system::error_code ec;
    auto endpoint = m_ssl_stream.next_layer().remote_endpoint(ec);
    return ec ? "Unknown" : endpoint.address().to_string();
}

void Session::Authenticate() {
    SetAuthenticated(true);
}

void Session::SetUdpEndpoint(const udp::endpoint& endpoint) {
    m_udp_endpoint = endpoint;
}

std::optional<udp::endpoint> Session::GetUdpEndpoint() const {
    return m_udp_endpoint;
}

void Session::SetPlayerId(uint64_t player_id) {
    m_player_id = player_id;
}

// [SEQUENCE: MVP9-5] Added DoHandshake() to perform the SSL handshake.
void Session::DoHandshake() {
    m_state = SessionState::Handshake;
    m_ssl_stream.async_handshake(boost::asio::ssl::stream_base::server,
        boost::asio::bind_executor(m_strand,
            [self = shared_from_this()](const boost::system::error_code& ec) {
                if (!ec) {
                    self->m_state = SessionState::Connected;
                    LOG_INFO("Session {} handshake successful. Remote: {}", self->m_sessionId, self->GetRemoteAddress());
                    self->DoReadHeader();
                } else {
                    LOG_ERROR("Session {} handshake failed: {}", self->m_sessionId, ec.message());
                    self->HandleError(ec);
                }
            }));
}

void Session::DoReadHeader() {
    m_readBuffer.assign(4, std::byte{0});
    boost::asio::async_read(m_ssl_stream, boost::asio::buffer(m_readBuffer),
        boost::asio::bind_executor(m_strand,
            [self = shared_from_this()](const boost::system::error_code& ec, [[maybe_unused]] std::size_t length) {
                if (!ec) {
                    uint32_t body_size = 0;
                    memcpy(&body_size, self->m_readBuffer.data(), sizeof(uint32_t));
                    body_size = ntohl(body_size);
                    self->DoReadBody(body_size);
                } else {
                    self->HandleError(ec);
                }
            }));
}

void Session::DoReadBody(uint32_t body_size) {
    if (body_size == 0 || body_size > 65536) {
        HandleError(boost::asio::error::invalid_argument);
        return;
    }
    m_readBuffer.resize(body_size);
    boost::asio::async_read(m_ssl_stream, boost::asio::buffer(m_readBuffer),
        boost::asio::bind_executor(m_strand,
            [self = shared_from_this()](const boost::system::error_code& ec, [[maybe_unused]] std::size_t length) {
                if (!ec) {
                    self->ProcessPacket(std::move(self->m_readBuffer));
                    self->DoReadHeader();
                } else {
                    self->HandleError(ec);
                }
            }));
}

std::string getMessageTypeName(mmorpg::proto::PacketType packet_type); // Forward declaration

void Session::ProcessPacket(std::vector<std::byte>&& data) {
    auto packet = PacketSerializer::Deserialize(data.data(), data.size());
    if (!packet) {
        return;
    }

    std::string type_name = getMessageTypeName(packet->header().type());
    const auto* descriptor = google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(type_name);
    if (!descriptor) return;

    auto message = std::shared_ptr<google::protobuf::Message>(google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor)->New());
    if (!message->ParseFromString(packet->payload())) return;

    if (m_packetHandler) {
        m_packetHandler->Handle(shared_from_this(), *message);
    }
}

void Session::DoWrite() {
    boost::asio::async_write(m_ssl_stream, boost::asio::buffer(m_writeQueue.front()),
        boost::asio::bind_executor(m_strand,
            [self = shared_from_this()](const boost::system::error_code& ec, [[maybe_unused]] std::size_t length) {
                if (!ec) {
                    self->m_writeQueue.pop_front();
                    if (!self->m_writeQueue.empty()) {
                        self->DoWrite();
                    }
                } else {
                    self->HandleError(ec);
                }
            }));
}

void Session::HandleError(const boost::system::error_code& ec) {
    if (ec != boost::asio::error::operation_aborted) {
        LOG_ERROR("Session [{}] error: {}", m_sessionId, ec.message());
    }
    Disconnect();
}


std::string getMessageTypeName(mmorpg::proto::PacketType packet_type) {
    switch (packet_type) {
        case mmorpg::proto::PACKET_LOGIN_REQUEST:
            return "mmorpg.proto.LoginRequest";
        case mmorpg::proto::PACKET_LOGIN_RESPONSE:
            return "mmorpg.proto.LoginResponse";
        case mmorpg::proto::PACKET_LOGOUT_REQUEST:
            return "mmorpg.proto.LogoutRequest";
        case mmorpg::proto::PACKET_LOGOUT_RESPONSE:
            return "mmorpg.proto.LogoutResponse";
        case mmorpg::proto::PACKET_HEARTBEAT_REQUEST:
            return "mmorpg.proto.HeartbeatRequest";
        case mmorpg::proto::PACKET_HEARTBEAT_RESPONSE:
            return "mmorpg.proto.HeartbeatResponse";
        case mmorpg::proto::PACKET_ENTER_WORLD_REQUEST:
            return "mmorpg.proto.EnterWorldRequest";
        case mmorpg::proto::PACKET_ENTER_WORLD_RESPONSE:
            return "mmorpg.proto.EnterWorldResponse";
        case mmorpg::proto::PACKET_MOVEMENT_UPDATE:
            return "mmorpg.proto.MovementUpdate";
        case mmorpg::proto::PACKET_ENTITY_UPDATE:
            return "mmorpg.proto.EntityUpdate";
        case mmorpg::proto::PACKET_COMBAT_ACTION:
            return "mmorpg.proto.CombatAction";
        case mmorpg::proto::PACKET_COMBAT_RESULT:
            return "mmorpg.proto.CombatResult";
        case mmorpg::proto::PACKET_CHAT_MESSAGE:
            return "mmorpg.proto.ChatMessage";
        case mmorpg::proto::PACKET_GUILD_CREATE_REQUEST:
            return "mmorpg.proto.GuildCreateRequest";
        case mmorpg::proto::PACKET_GUILD_CREATE_RESPONSE:
            return "mmorpg.proto.GuildCreateResponse";
        case mmorpg::proto::PACKET_GUILD_INVITE_REQUEST:
            return "mmorpg.proto.GuildInviteRequest";
        case mmorpg::proto::PACKET_GUILD_INVITE_RESPONSE:
            return "mmorpg.proto.GuildInviteResponse";
        case mmorpg::proto::PACKET_GUILD_WAR_REQUEST:
            return "mmorpg.proto.GuildWarRequest";
        case mmorpg::proto::PACKET_GUILD_WAR_RESPONSE:
            return "mmorpg.proto.GuildWarResponse";
        default:
            return "";
    }
}

}
