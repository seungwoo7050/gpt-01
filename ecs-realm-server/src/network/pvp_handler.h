#pragma once

#include "network/packet_handler.h"

namespace mmorpg::network {

// [SEQUENCE: MVP5-96] Handles all PvP-related packets.
class PvpHandler : public IPacketHandler {
public:
    PvpHandler();

    void Handle(std::shared_ptr<Session> session, const google::protobuf::Message& message) override;
    void RegisterHandler(const google::protobuf::Descriptor* descriptor, PacketHandlerCallback handler) override;

private:
    void HandleDuelRequest(std::shared_ptr<Session> session, const google::protobuf::Message& message);
    // ... other pvp packet handlers

    std::unordered_map<const google::protobuf::Descriptor*, PacketHandlerCallback> m_handlers;
};

}
