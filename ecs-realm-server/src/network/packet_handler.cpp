#include "network/packet_handler.h"
#include "network/session.h"

#include <iostream>
#include <google/protobuf/message.h>

namespace mmorpg::network {

// [SEQUENCE: MVP1-35] `PacketHandler::HandlePacket()`: 수신된 패킷을 처리하기 위해 등록된 핸들러를 호출합니다.
void PacketHandler::Handle(std::shared_ptr<Session> session, const google::protobuf::Message& message) {
    const auto* descriptor = message.GetDescriptor();
    auto it = m_handlers.find(descriptor);
    if (it != m_handlers.end()) {
        it->second(session, message);
    } else {
        // TODO: Replace with a proper logging system like spdlog
        std::cerr << "Error: No handler registered for message type '" << descriptor->full_name() << "'.\n";
    }
}

// [SEQUENCE: MVP1-34] `PacketHandler::RegisterHandler()`: 패킷 타입에 대한 핸들러 함수를 등록합니다.
void PacketHandler::RegisterHandler(const google::protobuf::Descriptor* descriptor, PacketHandlerCallback handler) {

    if (!m_handlers.try_emplace(descriptor, handler).second) {
        // TODO: Replace with a proper logging system
        std::cerr << "Warning: Handler for message type '" << descriptor->full_name() << "' was already registered. Overwriting.\n";
        m_handlers[descriptor] = handler;
    }
}

}