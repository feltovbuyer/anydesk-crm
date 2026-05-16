#pragma once
#include <string>
#include <vector>

struct LeadItem
{
    int id;
    std::string tgUserId;
    std::string username;
    std::string fullName;
    std::string channel;
    std::string geo;
    std::string subid;
    std::string tags;
    std::string comment;
    int unread;
    std::string createdAt;
    std::string lastMessageAt;
    std::string lastMessage;
};

struct MessageItem
{
    int id;
    int leadId;
    std::string sender;
    std::string text;
    std::string createdAt;
    std::string mediaType;
    std::string mediaId;
};

struct FolderCounts
{
    int unread;
    int rd;
    int fd;
    int all;
    int bad403;
};

class LeadService
{
public:
    static void upsertFromTelegram(
        const std::string& tgUserId,
        const std::string& username,
        const std::string& fullName,
        const std::string& channel,
        const std::string& geo,
        const std::string& subid
    );

    static int findLeadIdByTgUserId(const std::string& tgUserId);

    static void addMessage(
        int leadId,
        const std::string& sender,
        const std::string& text
    );

    static std::vector<LeadItem> all(
        const std::string& folder,
        const std::string& role = "admin",
        const std::string& login = ""
    );
    static std::vector<MessageItem> messages(int leadId);

    static LeadItem findById(int leadId);
    static void markRead(int leadId);

    static FolderCounts counts(
        const std::string& role = "admin",
        const std::string& login = ""
    );
};