#pragma once

#include "network/packet_handler.h"

namespace mmorpg::network {

// [SEQUENCE: MVP5-94] Handles all guild-related packets.
class GuildHandler : public IPacketHandler {
public:
    GuildHandler();

    void Handle(std::shared_ptr<Session> session, const google::protobuf::Message& message) override;
    void RegisterHandler(const google::protobuf::Descriptor* descriptor, PacketHandlerCallback handler) override;

private:
    void HandleCreateGuild(std::shared_ptr<Session> session, const google::protobuf::Message& message);
    void HandleInviteToGuild(std::shared_ptr<Session> session, const google::protobuf::Message& message);
    // ... other guild packet handlers

    std::unordered_map<const google::protobuf::Descriptor*, PacketHandlerCallback> m_handlers;
};

}
