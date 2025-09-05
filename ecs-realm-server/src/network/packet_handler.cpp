#include "network/packet_handler.h"
#include "network/session.h"

#include <iostream>
#include <google/protobuf/message.h>

namespace mmorpg::network {

// [SEQUENCE: MVP1-16] Dispatches a received message to the appropriate registered handler.
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

// [SEQUENCE: MVP1-15] Registers a callback for a specific Protobuf message type.
void PacketHandler::RegisterHandler(const google::protobuf::Descriptor* descriptor, PacketHandlerCallback handler) {

    if (!m_handlers.try_emplace(descriptor, handler).second) {
        // TODO: Replace with a proper logging system
        std::cerr << "Warning: Handler for message type '" << descriptor->full_name() << "' was already registered. Overwriting.\n";
        m_handlers[descriptor] = handler;
    }
}

}