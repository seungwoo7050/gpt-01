#include "pvp_handler.h"
#include "game/systems/pvp_manager.h"
#include "network/session.h"
#include <google/protobuf/message.h>

namespace mmorpg::network {

using namespace mmorpg::game::systems;

// [SEQUENCE: MVP5-97] Implements the PvpHandler.
PvpHandler::PvpHandler() {
    // TODO: Register specific pvp packet handlers
}

void PvpHandler::Handle(std::shared_ptr<Session> session, const google::protobuf::Message& message) {
    auto it = m_handlers.find(message.GetDescriptor());
    if (it != m_handlers.end()) {
        it->second(session, message);
    }
}

void PvpHandler::RegisterHandler(const google::protobuf::Descriptor* descriptor, PacketHandlerCallback handler) {
    m_handlers[descriptor] = handler;
}

void PvpHandler::HandleDuelRequest([[maybe_unused]] std::shared_ptr<Session> session, [[maybe_unused]] const google::protobuf::Message& message) {
    // TODO: Process DuelRequest packet and call PvpManager
}

}