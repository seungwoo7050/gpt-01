#include "guild_handler.h"
#include "game/social/guild_manager.h"
#include "network/session.h"
#include <google/protobuf/message.h>

namespace mmorpg::network {

using namespace mmorpg::game::social;

// [SEQUENCE: MVP5-95] Implements the GuildHandler.
GuildHandler::GuildHandler() {
    // TODO: Register specific guild packet handlers
    // e.g., RegisterHandler(CreateGuildPacket::descriptor(), 
    //      [this](auto session, auto& msg){ HandleCreateGuild(session, msg); });
}

void GuildHandler::Handle(std::shared_ptr<Session> session, const google::protobuf::Message& message) {
    auto it = m_handlers.find(message.GetDescriptor());
    if (it != m_handlers.end()) {
        it->second(session, message);
    }
}

void GuildHandler::RegisterHandler(const google::protobuf::Descriptor* descriptor, PacketHandlerCallback handler) {
    m_handlers[descriptor] = handler;
}

void GuildHandler::HandleCreateGuild([[maybe_unused]] std::shared_ptr<Session> session, [[maybe_unused]] const google::protobuf::Message& message) {
    // TODO: Process CreateGuild packet and call GuildManager
}

void GuildHandler::HandleInviteToGuild([[maybe_unused]] std::shared_ptr<Session> session, [[maybe_unused]] const google::protobuf::Message& message) {
    // TODO: Process InviteToGuild packet and call GuildManager
}

}