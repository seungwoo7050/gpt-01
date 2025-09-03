#include "network/session.h"
#include "proto/packet.pb.h"
#include "network/packet_serializer.h"
#include "network/packet_handler.h"
#include "proto/packet.pb.h"

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

Session::Session(std::shared_ptr<boost::asio::ssl::stream<tcp::socket>> ssl_stream, uint32_t session_id, std::shared_ptr<IPacketHandler> handler)
    : m_ssl_stream(std::move(*ssl_stream)),
      m_strand(boost::asio::make_strand(m_ssl_stream.get_executor())),
      m_state(SessionState::Handshake),
      m_sessionId(session_id),
      m_packetHandler(std::move(handler)),
      m_isAuthenticated(false) {}

Session::~Session() {}

// [SEQUENCE: MVP1-14] `Session::Start()`: 세션을 시작하고 클라이언트로부터 데이터 수신을 대기합니다.
void Session::Start() {
    m_state = SessionState::Connected;
    DoReadHeader();
}

// [SEQUENCE: MVP1-15] `Session::Stop()`: 세션을 종료하고 소켓을 닫습니다.
void Session::Disconnect() {
    if (m_state == SessionState::Disconnected) return;
    m_state = SessionState::Disconnected;

    // Gracefully shut down the SSL stream.
    m_ssl_stream.async_shutdown([self = shared_from_this()]([[maybe_unused]] const boost::system::error_code& ec) {
        boost::system::error_code socket_ec;
        self->GetSocket().shutdown(tcp::socket::shutdown_both, socket_ec);
        self->GetSocket().close(socket_ec);
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

// [SEQUENCE: MVP1-16] `Session::DoReadHeader()`: 패킷의 고정 크기 헤더를 비동기적으로 읽습니다.
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

// [SEQUENCE: MVP1-17] `Session::DoReadBody()`: 헤더에서 파악한 크기만큼 패킷의 본문을 비동기적으로 읽습니다.
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

// [SEQUENCE: MVP1-18] `Session::DoWrite()`: 클라이언트에게 패킷을 비동기적으로 전송합니다.
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
        // std::cerr << "Session [" << m_sessionId << "] error: " << ec.message() << std::endl;
    }
    Disconnect();
}

}