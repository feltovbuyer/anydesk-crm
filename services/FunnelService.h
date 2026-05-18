#pragma once

#include <string>

struct TgBotConfig;

class FunnelService {
public:
    static void startFdForLead(
        int leadId,
        long long tgUserId,
        const TgBotConfig& bot
    );

    static void handleLeadReply(
        int leadId,
        long long tgUserId,
        const TgBotConfig& bot,
        const std::string& replyText
    );
};