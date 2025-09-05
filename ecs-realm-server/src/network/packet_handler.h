#pragma once

#include <functional>
#include <atomic>
#include <memory>
#include <unordered_map>

// Forward declarations
namespace google::protobuf {
class Message;
class Descriptor;
}
namespace mmorpg::network {
class Session;
}

namespace mmorpg::network {

// [SEQUENCE: MVP1-13] Defines the callback type and interface for handling packets.
using PacketHandlerCallback = std::function<void(std::shared_ptr<Session>, const google::protobuf::Message&)>;

class IPacketHandler {
public:
    virtual ~IPacketHandler() = default;
    virtual void Handle(std::shared_ptr<Session> session, const google::protobuf::Message& message) = 0;
    virtual void RegisterHandler(const google::protobuf::Descriptor* descriptor, PacketHandlerCallback handler) = 0;
};

// [SEQUENCE: MVP1-14] A basic implementation of the packet handler interface.
class PacketHandler : public IPacketHandler {
public:
    void Handle(std::shared_ptr<Session> session, const google::protobuf::Message& message) override;
    void RegisterHandler(const google::protobuf::Descriptor* descriptor, PacketHandlerCallback handler) override;

private:
    std::unordered_map<const google::protobuf::Descriptor*, PacketHandlerCallback> m_handlers;
};

}
