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

// [SEQUENCE: MVP1-51] Packet handler callback type
using PacketHandlerCallback = std::function<void(std::shared_ptr<Session>, const google::protobuf::Message&)>;

// [SEQUENCE: MVP1-52] Packet handler interface
class IPacketHandler {
public:
    virtual ~IPacketHandler() = default;
    virtual void Handle(std::shared_ptr<Session> session, const google::protobuf::Message& message) = 0;
    virtual void RegisterHandler(const google::protobuf::Descriptor* descriptor, PacketHandlerCallback handler) = 0;
};

// [SEQUENCE: MVP1-33] Basic packet handler implementation
class PacketHandler : public IPacketHandler {
public:
    // [SEQUENCE: MVP1-35] `PacketHandler::HandlePacket()`: 수신된 패킷을 처리하기 위해 등록된 핸들러를 호출합니다.
    void Handle(std::shared_ptr<Session> session, const google::protobuf::Message& message) override;
    // [SEQUENCE: MVP1-34] `PacketHandler::RegisterHandler()`: 패킷 타입에 대한 핸들러 함수를 등록합니다.
    void RegisterHandler(const google::protobuf::Descriptor* descriptor, PacketHandlerCallback handler) override;

private:
    std::unordered_map<const google::protobuf::Descriptor*, PacketHandlerCallback> m_handlers;
};

}