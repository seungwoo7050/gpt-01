#pragma once

#include <vector>
#include <memory>
#include <cstddef>

#include "proto/packet.pb.h"

#include "proto/packet.pb.h"

// Forward declarations
namespace google::protobuf {
class Message;
}

namespace mmorpg::network {

// [SEQUENCE: MVP1-30] Packet serialization utilities
namespace PacketSerializer {

    // [SEQUENCE: MVP1-31] `PacketSerializer::Serialize()`: 메시지를 직렬화하여 버퍼에 씁니다.
    // [SEQUENCE: MVP1-47] Serialize packet with 4-byte size header
    // [SEQUENCE: MVP1-48] Create a packet with header and payload
    std::vector<std::byte> Serialize(const google::protobuf::Message& message);

    // [SEQUENCE: MVP1-32] `PacketSerializer::Deserialize()`: 버퍼에서 메시지를 역직렬화합니다.
    // [SEQUENCE: MVP1-49] Extract message from packet payload
    std::unique_ptr<mmorpg::proto::Packet> Deserialize(const std::byte* data, size_t size);

} // namespace PacketSerializer
} // namespace mmorpg::network
