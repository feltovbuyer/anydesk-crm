#include "FunnelService.h"
#include "TelegramService.h"
#include "LeadTagService.h"
#include "../db/Database.h"

#include <thread>
#include <chrono>

static void sendStep(
    int leadId,
    long long tgUserId,
    const TgBotConfig& bot,
    int stepId,
    const std::string& tagAfter
)
{
    auto msgs = Database::query(
        "SELECT message_type, text, file_path, delay_seconds "
        "FROM funnel_messages "
        "WHERE step_id=" + std::to_string(stepId) + " "
        "ORDER BY sort_order ASC, id ASC"
    );

    for (const auto& m : msgs) {
        std::string type = m.at("message_type");
        std::string text = m.at("text");
        std::string file = m.at("file_path");

        double delay = 2.5;

        try {
            delay = std::stod(m.at("delay_seconds"));
        }
        catch (...) {}

        if (type == "text") {
            TelegramService::sendText(bot.token, tgUserId, text);

            Database::exec(
                "INSERT INTO messages (lead_id, sender, text, media_type, media_id, created_at, is_read) "
                "VALUES (" + std::to_string(leadId) + ", 'admin', '" + Database::escape(text) + "', 'text', '', datetime('now'), 1)"
            );
        }
        else {
            TelegramService::sendFile(bot.token, tgUserId, type, file, text);

            std::string fileName = file;

            size_t p = fileName.find_last_of("/\\");
            if (p != std::string::npos) {
                fileName = fileName.substr(p + 1);
            }

            Database::exec(
                "INSERT INTO messages (lead_id, sender, text, media_type, media_id, created_at, is_read) "
                "VALUES (" + std::to_string(leadId) + ", 'admin', '" + Database::escape(text) + "', '" + Database::escape(type) + "', '" + Database::escape(fileName) + "', datetime('now'), 1)"
            );
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds((int)(delay * 1000))
        );
    }

    LeadTagService::setFunnelTag(leadId, tagAfter);

    Database::exec(
        "UPDATE lead_funnel_state SET "
        "current_step_id=" + std::to_string(stepId) + ", "
        "waiting_answer=1, "
        "updated_at=datetime('now') "
        "WHERE lead_id=" + std::to_string(leadId)
    );
}

void FunnelService::startFdForLead(
    int leadId,
    long long tgUserId,
    const TgBotConfig& bot
)
{
    if (bot.funnel.empty()) return;

    auto f = Database::query(
        "SELECT id FROM funnels "
        "WHERE name='" + Database::escape(bot.funnel) + "' "
        "AND type='fd' "
        "AND active=1 "
        "LIMIT 1"
    );

    if (f.empty()) return;

    std::string funnelId = f[0].at("id");

    auto existing = Database::query(
        "SELECT id, finished FROM lead_funnel_state "
        "WHERE lead_id=" + std::to_string(leadId) + " "
        "LIMIT 1"
    );

    if (!existing.empty()) {
        if (existing[0].at("finished") == "1") {
            return;
        }
    }
    else {
        Database::exec(
            "INSERT INTO lead_funnel_state "
            "(lead_id, funnel_id, current_step_id, waiting_answer, finished, started_at, updated_at) "
            "VALUES ("
            + std::to_string(leadId) + ", "
            + funnelId + ", "
            "0, 0, 0, datetime('now'), datetime('now'))"
        );
    }

    auto steps = Database::query(
        "SELECT id, tag_after "
        "FROM funnel_steps "
        "WHERE funnel_id=" + funnelId + " "
        "ORDER BY sort_order ASC, id ASC "
        "LIMIT 1"
    );

    if (steps.empty()) return;

    sendStep(
        leadId,
        tgUserId,
        bot,
        std::stoi(steps[0].at("id")),
        steps[0].at("tag_after")
    );
}

void FunnelService::handleLeadReply(
    int leadId,
    long long tgUserId,
    const TgBotConfig& bot,
    const std::string& replyText
)
{
    auto st = Database::query(
        "SELECT funnel_id, current_step_id, waiting_answer "
        "FROM lead_funnel_state "
        "WHERE lead_id=" + std::to_string(leadId) + " "
        "AND finished=0 "
        "LIMIT 1"
    );

    if (st.empty()) return;
    if (st[0].at("waiting_answer") != "1") return;

    std::string funnelId = st[0].at("funnel_id");
    std::string stepId = st[0].at("current_step_id");

    Database::exec(
        "INSERT INTO lead_funnel_answers "
        "(lead_id, funnel_id, step_id, answer_text, answer_type, created_at) "
        "VALUES ("
        + std::to_string(leadId) + ", "
        + funnelId + ", "
        + stepId + ", '"
        + Database::escape(replyText) + "', 'text', datetime('now'))"
    );

    Database::exec(
        "UPDATE lead_funnel_state SET waiting_answer=0 "
        "WHERE lead_id=" + std::to_string(leadId)
    );

    auto next = Database::query(
        "SELECT id, tag_after "
        "FROM funnel_steps "
        "WHERE funnel_id=" + funnelId + " "
        "AND sort_order > (SELECT sort_order FROM funnel_steps WHERE id=" + stepId + ") "
        "ORDER BY sort_order ASC "
        "LIMIT 1"
    );

    if (next.empty()) {
        LeadTagService::finishFd(leadId);

        Database::exec(
            "UPDATE lead_funnel_state SET "
            "finished=1, waiting_answer=0 "
            "WHERE lead_id=" + std::to_string(leadId)
        );

        return;
    }

    sendStep(
        leadId,
        tgUserId,
        bot,
        std::stoi(next[0].at("id")),
        next[0].at("tag_after")
    );
}