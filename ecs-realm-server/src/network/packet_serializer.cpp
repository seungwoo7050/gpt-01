#include "network/packet_serializer.h"
#include "proto/packet.pb.h"
#include "proto/packet.pb.h"

#include <google/protobuf/message.h>
#include <arpa/inet.h> // For htonl, ntohl
#include <map>

namespace mmorpg::network {
namespace PacketSerializer {

std::map<std::string, mmorpg::proto::PacketType> type_name_to_packet_type_map = {
    {"mmorpg.proto.LoginRequest", mmorpg::proto::PACKET_LOGIN_REQUEST},
    {"mmorpg.proto.LoginResponse", mmorpg::proto::PACKET_LOGIN_RESPONSE},
    {"mmorpg.proto.LogoutRequest", mmorpg::proto::PACKET_LOGOUT_REQUEST},
    {"mmorpg.proto.LogoutResponse", mmorpg::proto::PACKET_LOGOUT_RESPONSE},
    {"mmorpg.proto.HeartbeatRequest", mmorpg::proto::PACKET_HEARTBEAT_REQUEST},
    {"mmorpg.proto.HeartbeatResponse", mmorpg::proto::PACKET_HEARTBEAT_RESPONSE},
    {"mmorpg.proto.EnterWorldRequest", mmorpg::proto::PACKET_ENTER_WORLD_REQUEST},
    {"mmorpg.proto.EnterWorldResponse", mmorpg::proto::PACKET_ENTER_WORLD_RESPONSE},
    {"mmorpg.proto.MovementUpdate", mmorpg::proto::PACKET_MOVEMENT_UPDATE},
    {"mmorpg.proto.EntityUpdate", mmorpg::proto::PACKET_ENTITY_UPDATE},
    {"mmorpg.proto.CombatAction", mmorpg::proto::PACKET_COMBAT_ACTION},
    {"mmorpg.proto.CombatResult", mmorpg::proto::PACKET_COMBAT_RESULT},
    {"mmorpg.proto.ChatMessage", mmorpg::proto::PACKET_CHAT_MESSAGE},
    {"mmorpg.proto.GuildCreateRequest", mmorpg::proto::PACKET_GUILD_CREATE_REQUEST},
    {"mmorpg.proto.GuildCreateResponse", mmorpg::proto::PACKET_GUILD_CREATE_RESPONSE},
    {"mmorpg.proto.GuildInviteRequest", mmorpg::proto::PACKET_GUILD_INVITE_REQUEST},
    {"mmorpg.proto.GuildInviteResponse", mmorpg::proto::PACKET_GUILD_INVITE_RESPONSE},
    {"mmorpg.proto.GuildWarRequest", mmorpg::proto::PACKET_GUILD_WAR_REQUEST},
    {"mmorpg.proto.GuildWarResponse", mmorpg::proto::PACKET_GUILD_WAR_RESPONSE},
};

// [SEQUENCE: MVP1-11] Serializes a message into a byte vector with a 4-byte size header.
std::vector<std::byte> Serialize(const google::protobuf::Message& message) {
    proto::Packet packet;
    auto* header = packet.mutable_header();

    auto it = type_name_to_packet_type_map.find(message.GetTypeName());
    if (it == type_name_to_packet_type_map.end()) {
        return {};
    }
    header->set_type(it->second);

    if (!message.SerializeToString(packet.mutable_payload())) {
        return {};
    }

    const size_t packet_size = packet.ByteSizeLong();
    std::vector<std::byte> buffer(4 + packet_size);

    uint32_t net_packet_size = htonl(static_cast<uint32_t>(packet_size));
    memcpy(buffer.data(), &net_packet_size, sizeof(net_packet_size));

    if (!packet.SerializeToArray(buffer.data() + 4, packet_size)) {
        return {};
    }

    return buffer;
}


// [SEQUENCE: MVP1-12] Deserializes a raw byte array back into a Packet message.
std::unique_ptr<proto::Packet> Deserialize(const std::byte* data, size_t size) {
    auto packet = std::make_unique<proto::Packet>();
    if (packet->ParseFromArray(data, size)) {
        return packet;
    }
    return nullptr;
}

} // namespace PacketSerializer
} // namespace mmorpg::network