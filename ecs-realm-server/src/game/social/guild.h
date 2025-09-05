#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace mmorpg::game::social {

// [SEQUENCE: MVP5-90] Defines a member of a guild.
struct GuildMember {
    uint64_t character_id;
    std::string character_name;
    // TODO: Add rank, status, etc.
};

// [SEQUENCE: MVP5-91] Defines a Guild.
class Guild {
public:
    Guild(uint32_t id, std::string name, uint64_t leader_id, const std::vector<uint64_t>& signers) 
        : m_id(id), m_name(name) {
        // Add leader and signers as initial members
        m_members[leader_id] = {leader_id, "Leader"}; // Placeholder name
        for (auto signer_id : signers) {
            m_members[signer_id] = {signer_id, "Signer"}; // Placeholder name
        }
    }

    uint32_t GetId() const { return m_id; }
    const std::string& GetName() const { return m_name; }
    int GetMemberCount() const { return m_members.size(); }

    GuildMember* GetMember(uint64_t member_id) {
        auto it = m_members.find(member_id);
        if (it != m_members.end()) {
            return &it->second;
        }
        return nullptr;
    }

    void AddMember(uint64_t member_id, std::string name) {
        m_members[member_id] = {member_id, name};
    }

    void RemoveMember(uint64_t member_id) {
        m_members.erase(member_id);
    }

private:
    uint32_t m_id;
    std::string m_name;
    std::unordered_map<uint64_t, GuildMember> m_members;
};

}
