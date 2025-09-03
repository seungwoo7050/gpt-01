#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <mutex>
#include <spdlog/spdlog.h>

namespace mmorpg::game::social {

// [SEQUENCE: 1662] Mail system for asynchronous player communication
// 메일 시스템으로 비동기 플레이어 커뮤니케이션 관리

// [SEQUENCE: 1663] Mail type
enum class MailType {
    PLAYER,         // Player to player mail
    SYSTEM,         // System generated mail
    GM,             // Game Master mail
    AUCTION,        // Auction house notifications
    QUEST,          // Quest reward mail
    ACHIEVEMENT,    // Achievement reward mail
    EVENT           // Event reward mail
};

// [SEQUENCE: 1664] Mail flags
enum class MailFlag : uint32_t {
    NONE = 0,
    UNREAD = 1 << 0,
    COD = 1 << 1,              // Cash on Delivery
    RETURNED = 1 << 2,
    DELETED = 1 << 3,
    GM_MAIL = 1 << 4,
    ITEM_ATTACHED = 1 << 5,
    GOLD_ATTACHED = 1 << 6
};

// [SEQUENCE: 1665] Mail attachment
struct MailAttachment {
    enum class Type {
        ITEM,
        GOLD
    };
    
    Type type;
    uint64_t item_instance_id = 0;  // For items
    uint32_t item_id = 0;           // Item template
    uint32_t quantity = 1;
    uint64_t gold_amount = 0;       // For gold
    
    bool is_taken = false;
};

// [SEQUENCE: 1666] Mail message
struct Mail {
    uint64_t mail_id;
    MailType type = MailType::PLAYER;
    
    // Sender info
    uint64_t sender_id = 0;
    std::string sender_name;
    
    // Recipient info
    uint64_t recipient_id;
    std::string recipient_name;
    
    // Content
    std::string subject;
    std::string body;
    
    // Attachments
    std::vector<MailAttachment> attachments;
    uint64_t cod_amount = 0;  // Cash on Delivery
    
    // Timestamps
    std::chrono::system_clock::time_point send_time;
    std::chrono::system_clock::time_point expire_time;
    std::optional<std::chrono::system_clock::time_point> read_time;
    std::optional<std::chrono::system_clock::time_point> deleted_time;
    
    // Flags
    uint32_t flags = static_cast<uint32_t>(MailFlag::UNREAD);
    
    // [SEQUENCE: 1667] Check if mail has flag
    bool HasFlag(MailFlag flag) const {
        return (flags & static_cast<uint32_t>(flag)) != 0;
    }
    
    // [SEQUENCE: 1668] Set flag
    void SetFlag(MailFlag flag) {
        flags |= static_cast<uint32_t>(flag);
    }
    
    // [SEQUENCE: 1669] Remove flag
    void RemoveFlag(MailFlag flag) {
        flags &= ~static_cast<uint32_t>(flag);
    }
    
    // [SEQUENCE: 1670] Check if expired
    bool IsExpired() const {
        return std::chrono::system_clock::now() > expire_time;
    }
    
    // [SEQUENCE: 1671] Has attachments
    bool HasAttachments() const {
        return !attachments.empty() || cod_amount > 0;
    }
    
    // [SEQUENCE: 1672] Can be deleted
    bool CanBeDeleted() const {
        // Can't delete if has untaken attachments
        for (const auto& attachment : attachments) {
            if (!attachment.is_taken) {
                return false;
            }
        }
        return true;
    }
};

// [SEQUENCE: 1673] Mailbox for a player
class Mailbox {
public:
    static constexpr size_t MAX_MAILS = 100;
    static constexpr size_t MAX_MAILS_PER_DAY = 50;
    
    Mailbox(uint64_t owner_id) : owner_id_(owner_id) {}
    
    // [SEQUENCE: 1674] Add mail
    bool AddMail(const Mail& mail) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (mails_.size() >= MAX_MAILS) {
            spdlog::warn("Mailbox full for player {}", owner_id_);
            return false;
        }
        
        // Check daily limit
        if (GetMailsReceivedToday() >= MAX_MAILS_PER_DAY) {
            spdlog::warn("Daily mail limit reached for player {}", owner_id_);
            return false;
        }
        
        mails_[mail.mail_id] = mail;
        
        if (mail.HasFlag(MailFlag::UNREAD)) {
            unread_count_++;
        }
        
        return true;
    }
    
    // [SEQUENCE: 1675] Get mail
    Mail* GetMail(uint64_t mail_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = mails_.find(mail_id);
        if (it == mails_.end()) {
            return nullptr;
        }
        
        return &it->second;
    }
    
    // [SEQUENCE: 1676] Read mail
    bool ReadMail(uint64_t mail_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = mails_.find(mail_id);
        if (it == mails_.end()) {
            return false;
        }
        
        if (it->second.HasFlag(MailFlag::UNREAD)) {
            it->second.RemoveFlag(MailFlag::UNREAD);
            it->second.read_time = std::chrono::system_clock::now();
            unread_count_--;
        }
        
        return true;
    }
    
    // [SEQUENCE: 1677] Delete mail
    bool DeleteMail(uint64_t mail_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = mails_.find(mail_id);
        if (it == mails_.end()) {
            return false;
        }
        
        if (!it->second.CanBeDeleted()) {
            spdlog::warn("Cannot delete mail {} - has untaken attachments", mail_id);
            return false;
        }
        
        it->second.SetFlag(MailFlag::DELETED);
        it->second.deleted_time = std::chrono::system_clock::now();
        
        // Move to deleted mails
        deleted_mails_[mail_id] = it->second;
        mails_.erase(it);
        
        return true;
    }
    
    // [SEQUENCE: 1678] Take attachment
    bool TakeAttachment(uint64_t mail_id, size_t attachment_index) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = mails_.find(mail_id);
        if (it == mails_.end()) {
            return false;
        }
        
        if (attachment_index >= it->second.attachments.size()) {
            return false;
        }
        
        auto& attachment = it->second.attachments[attachment_index];
        if (attachment.is_taken) {
            return false;
        }
        
        // TODO: Add item/gold to player inventory
        attachment.is_taken = true;
        
        return true;
    }
    
    // [SEQUENCE: 1679] Pay COD
    bool PayCOD(uint64_t mail_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = mails_.find(mail_id);
        if (it == mails_.end()) {
            return false;
        }
        
        if (!it->second.HasFlag(MailFlag::COD)) {
            return false;
        }
        
        // TODO: Check player has enough gold
        // TODO: Deduct gold from player
        // TODO: Send gold to original sender
        
        it->second.RemoveFlag(MailFlag::COD);
        it->second.cod_amount = 0;
        
        return true;
    }
    
    // [SEQUENCE: 1680] Get all mails
    std::vector<Mail> GetMails(bool include_deleted = false) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<Mail> result;
        
        for (const auto& [id, mail] : mails_) {
            result.push_back(mail);
        }
        
        if (include_deleted) {
            for (const auto& [id, mail] : deleted_mails_) {
                result.push_back(mail);
            }
        }
        
        // Sort by send time (newest first)
        std::sort(result.begin(), result.end(),
            [](const Mail& a, const Mail& b) {
                return a.send_time > b.send_time;
            });
        
        return result;
    }
    
    // [SEQUENCE: 1681] Clean expired mails
    void CleanExpiredMails() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<uint64_t> expired_ids;
        
        for (const auto& [id, mail] : mails_) {
            if (mail.IsExpired() && mail.CanBeDeleted()) {
                expired_ids.push_back(id);
            }
        }
        
        for (uint64_t id : expired_ids) {
            auto it = mails_.find(id);
            if (it != mails_.end()) {
                if (it->second.HasFlag(MailFlag::UNREAD)) {
                    unread_count_--;
                }
                
                // Return attachments if any
                if (it->second.HasAttachments()) {
                    ReturnMail(it->second);
                }
                
                mails_.erase(it);
            }
        }
        
        // Clean old deleted mails
        auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24);
        std::erase_if(deleted_mails_, [cutoff_time](const auto& pair) {
            return pair.second.deleted_time.value_or(cutoff_time) < cutoff_time;
        });
    }
    
    // Getters
    size_t GetMailCount() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return mails_.size(); 
    }
    
    size_t GetUnreadCount() const { 
        return unread_count_; 
    }
    
    bool IsFull() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return mails_.size() >= MAX_MAILS; 
    }
    
private:
    uint64_t owner_id_;
    mutable std::mutex mutex_;
    
    std::unordered_map<uint64_t, Mail> mails_;
    std::unordered_map<uint64_t, Mail> deleted_mails_;
    
    std::atomic<size_t> unread_count_{0};
    
    // [SEQUENCE: 1682] Get mails received today
    size_t GetMailsReceivedToday() const {
        auto today_start = std::chrono::system_clock::now();
        today_start -= std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::hours(std::chrono::duration_cast<std::chrono::hours>(
                today_start.time_since_epoch()).count() % 24));
        
        size_t count = 0;
        for (const auto& [id, mail] : mails_) {
            if (mail.send_time >= today_start) {
                count++;
            }
        }
        
        return count;
    }
    
    // [SEQUENCE: 1683] Return mail to sender
    void ReturnMail(const Mail& original_mail);  // Implemented by MailManager
};

// [SEQUENCE: 1684] Mail manager
class MailManager {
public:
    static MailManager& Instance() {
        static MailManager instance;
        return instance;
    }
    
    // [SEQUENCE: 1685] Send mail
    bool SendMail(uint64_t sender_id, const std::string& sender_name,
                  uint64_t recipient_id, const std::string& recipient_name,
                  const std::string& subject, const std::string& body,
                  const std::vector<MailAttachment>& attachments = {},
                  uint64_t cod_amount = 0) {
        
        // Validate
        if (!ValidateMail(sender_id, recipient_id, attachments)) {
            return false;
        }
        
        // Create mail
        Mail mail;
        mail.mail_id = GenerateMailId();
        mail.type = MailType::PLAYER;
        mail.sender_id = sender_id;
        mail.sender_name = sender_name;
        mail.recipient_id = recipient_id;
        mail.recipient_name = recipient_name;
        mail.subject = subject;
        mail.body = body;
        mail.attachments = attachments;
        mail.cod_amount = cod_amount;
        mail.send_time = std::chrono::system_clock::now();
        mail.expire_time = mail.send_time + std::chrono::hours(30 * 24);  // 30 days
        
        if (!attachments.empty()) {
            mail.SetFlag(MailFlag::ITEM_ATTACHED);
        }
        if (cod_amount > 0) {
            mail.SetFlag(MailFlag::COD);
        }
        
        // Get recipient mailbox
        auto mailbox = GetOrCreateMailbox(recipient_id);
        if (!mailbox->AddMail(mail)) {
            return false;
        }
        
        // Log mail
        LogMail(mail);
        
        // TODO: Notify recipient if online
        
        spdlog::info("Mail {} sent from {} to {}", 
                    mail.mail_id, sender_id, recipient_id);
        
        return true;
    }
    
    // [SEQUENCE: 1686] Send system mail
    bool SendSystemMail(uint64_t recipient_id, const std::string& recipient_name,
                       const std::string& subject, const std::string& body,
                       const std::vector<MailAttachment>& attachments = {}) {
        
        Mail mail;
        mail.mail_id = GenerateMailId();
        mail.type = MailType::SYSTEM;
        mail.sender_id = 0;
        mail.sender_name = "System";
        mail.recipient_id = recipient_id;
        mail.recipient_name = recipient_name;
        mail.subject = subject;
        mail.body = body;
        mail.attachments = attachments;
        mail.send_time = std::chrono::system_clock::now();
        mail.expire_time = mail.send_time + std::chrono::hours(90 * 24);  // 90 days for system mail
        
        if (!attachments.empty()) {
            mail.SetFlag(MailFlag::ITEM_ATTACHED);
        }
        
        auto mailbox = GetOrCreateMailbox(recipient_id);
        return mailbox->AddMail(mail);
    }
    
    // [SEQUENCE: 1687] Return mail
    bool ReturnMail(uint64_t mail_id, uint64_t owner_id) {
        auto mailbox = GetMailbox(owner_id);
        if (!mailbox) {
            return false;
        }
        
        auto* mail = mailbox->GetMail(mail_id);
        if (!mail || mail->type != MailType::PLAYER) {
            return false;
        }
        
        // Create return mail
        Mail return_mail;
        return_mail.mail_id = GenerateMailId();
        return_mail.type = MailType::PLAYER;
        return_mail.sender_id = mail->recipient_id;
        return_mail.sender_name = mail->recipient_name;
        return_mail.recipient_id = mail->sender_id;
        return_mail.recipient_name = mail->sender_name;
        return_mail.subject = "Returned: " + mail->subject;
        return_mail.body = "Your mail has been returned.\n\nOriginal message:\n" + mail->body;
        return_mail.send_time = std::chrono::system_clock::now();
        return_mail.expire_time = return_mail.send_time + std::chrono::hours(30 * 24);
        return_mail.SetFlag(MailFlag::RETURNED);
        
        // Return untaken attachments
        for (const auto& attachment : mail->attachments) {
            if (!attachment.is_taken) {
                return_mail.attachments.push_back(attachment);
            }
        }
        
        // Get sender's mailbox
        auto sender_mailbox = GetOrCreateMailbox(mail->sender_id);
        if (!sender_mailbox->AddMail(return_mail)) {
            return false;
        }
        
        // Delete original mail
        mailbox->DeleteMail(mail_id);
        
        return true;
    }
    
    // [SEQUENCE: 1688] Get player mailbox
    std::shared_ptr<Mailbox> GetMailbox(uint64_t player_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = mailboxes_.find(player_id);
        if (it == mailboxes_.end()) {
            return nullptr;
        }
        
        return it->second;
    }
    
    // [SEQUENCE: 1689] Get or create mailbox
    std::shared_ptr<Mailbox> GetOrCreateMailbox(uint64_t player_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = mailboxes_.find(player_id);
        if (it == mailboxes_.end()) {
            auto mailbox = std::make_shared<Mailbox>(player_id);
            mailboxes_[player_id] = mailbox;
            return mailbox;
        }
        
        return it->second;
    }
    
    // [SEQUENCE: 1690] Clean all expired mails
    void CleanupExpiredMails() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [player_id, mailbox] : mailboxes_) {
            mailbox->CleanExpiredMails();
        }
        
        spdlog::info("Cleaned up expired mails");
    }
    
    // [SEQUENCE: 1691] Mass mail (GM function)
    void SendMassMail(const std::string& subject, const std::string& body,
                     const std::vector<MailAttachment>& attachments = {}) {
        // TODO: Get all active players and send mail to each
        spdlog::info("Sending mass mail: {}", subject);
    }
    
private:
    MailManager() = default;
    
    std::mutex mutex_;
    std::unordered_map<uint64_t, std::shared_ptr<Mailbox>> mailboxes_;
    
    std::atomic<uint64_t> next_mail_id_{1};
    
    // Mail history for logging
    struct MailLog {
        uint64_t mail_id;
        uint64_t sender_id;
        uint64_t recipient_id;
        MailType type;
        std::chrono::system_clock::time_point timestamp;
        bool has_attachments;
    };
    std::deque<MailLog> mail_history_;
    
    // [SEQUENCE: 1692] Generate mail ID
    uint64_t GenerateMailId() {
        return next_mail_id_++;
    }
    
    // [SEQUENCE: 1693] Validate mail
    bool ValidateMail(uint64_t sender_id, uint64_t recipient_id,
                     const std::vector<MailAttachment>& attachments) {
        // Can't mail to self
        if (sender_id == recipient_id) {
            spdlog::warn("Cannot send mail to self");
            return false;
        }
        
        // Validate attachments
        if (attachments.size() > 12) {  // Max 12 item slots
            spdlog::warn("Too many attachments");
            return false;
        }
        
        // TODO: Validate sender owns items
        // TODO: Validate items can be mailed
        // TODO: Check sender/recipient not banned from mail
        
        return true;
    }
    
    // [SEQUENCE: 1694] Log mail
    void LogMail(const Mail& mail) {
        MailLog log;
        log.mail_id = mail.mail_id;
        log.sender_id = mail.sender_id;
        log.recipient_id = mail.recipient_id;
        log.type = mail.type;
        log.timestamp = mail.send_time;
        log.has_attachments = !mail.attachments.empty();
        
        mail_history_.push_back(log);
        
        // Keep only recent history
        while (mail_history_.size() > 10000) {
            mail_history_.pop_front();
        }
    }
};

// [SEQUENCE: 1695] Mail templates for common system mails
class MailTemplates {
public:
    // Quest completion mail
    static void SendQuestRewardMail(uint64_t player_id, const std::string& player_name,
                                   const std::string& quest_name,
                                   const std::vector<MailAttachment>& rewards) {
        std::string subject = "Quest Reward: " + quest_name;
        std::string body = "Congratulations on completing " + quest_name + 
                          "!\n\nYour rewards are attached.";
        
        Mail mail;
        mail.type = MailType::QUEST;
        
        MailManager::Instance().SendSystemMail(player_id, player_name, 
                                             subject, body, rewards);
    }
    
    // Auction sold mail
    static void SendAuctionSoldMail(uint64_t seller_id, const std::string& seller_name,
                                   const std::string& item_name, uint64_t sale_price) {
        std::string subject = "Auction Successful: " + item_name;
        std::string body = "Your " + item_name + " has been sold!\n\n" +
                          "Sale price: " + std::to_string(sale_price) + " gold";
        
        MailAttachment gold_attachment;
        gold_attachment.type = MailAttachment::Type::GOLD;
        gold_attachment.gold_amount = sale_price;
        
        Mail mail;
        mail.type = MailType::AUCTION;
        
        MailManager::Instance().SendSystemMail(seller_id, seller_name,
                                             subject, body, {gold_attachment});
    }
    
    // Achievement reward mail
    static void SendAchievementMail(uint64_t player_id, const std::string& player_name,
                                   const std::string& achievement_name,
                                   const std::vector<MailAttachment>& rewards) {
        std::string subject = "Achievement Earned: " + achievement_name;
        std::string body = "Congratulations on earning " + achievement_name + "!";
        
        Mail mail;
        mail.type = MailType::ACHIEVEMENT;
        
        MailManager::Instance().SendSystemMail(player_id, player_name,
                                             subject, body, rewards);
    }
};

} // namespace mmorpg::game::social