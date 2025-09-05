#pragma once

#include <vector>
#include <memory>
#include <cstddef>

#include "proto/packet.pb.h"

// Forward declarations
namespace google::protobuf {
class Message;
}

namespace mmorpg::network {

// [SEQUENCE: MVP1-10] PacketSerializer: A utility for serializing and deserializing Protobuf messages.
namespace PacketSerializer {

    // Serializes a Protobuf message into a byte vector with a 4-byte length prefix.
    std::vector<std::byte> Serialize(const google::protobuf::Message& message);

    // Deserializes a byte array into a Packet message.
    std::unique_ptr<mmorpg::proto::Packet> Deserialize(const std::byte* data, size_t size);

} // namespace PacketSerializer
} // namespace mmorpg::network