#include "LeadService.h"
#include "../db/Database.h"

#include <sstream>

static std::string esc2(const std::string& s)
{
    std::string out;
    for (char c : s) {
        if (c == '\'') out += "''";
        else out += c;
    }
    return out;
}

void LeadService::upsertFromTelegram(
    const std::string& tgUserId,
    const std::string& username,
    const std::string& fullName,
    const std::string& channel,
    const std::string& geo,
    const std::string& subid
)
{
    std::ostringstream sql;

    sql << "INSERT INTO leads "
        << "(tg_user_id, username, full_name, channel, geo, subid, tags, last_message_at) VALUES ("
        << "'" << esc2(tgUserId) << "',"
        << "'" << esc2(username) << "',"
        << "'" << esc2(fullName) << "',"
        << "'" << esc2(channel) << "',"
        << "'" << esc2(geo) << "',"
        << "'" << esc2(subid) << "',"
        << "'" << esc2(geo) << "',"
        << "CURRENT_TIMESTAMP"
        << ") "
        << "ON CONFLICT(tg_user_id) DO UPDATE SET "
        << "username=excluded.username,"
        << "full_name=excluded.full_name,"
        << "channel=excluded.channel,"
        << "geo=excluded.geo,"
        << "last_message_at=CURRENT_TIMESTAMP";

    if (!subid.empty()) {
        sql << ",subid=excluded.subid";
    }

    sql << ";";

    Database::exec(sql.str());
}

int LeadService::findLeadIdByTgUserId(const std::string& tgUserId)
{
    auto rows = Database::query(
        "SELECT id FROM leads WHERE tg_user_id='" + esc2(tgUserId) + "' LIMIT 1;"
    );

    if (rows.empty()) return 0;
    return std::stoi(rows[0]["id"]);
}

void LeadService::addMessage(
    int leadId,
    const std::string& sender,
    const std::string& text
)
{
    if (leadId <= 0 || text.empty()) return;

    std::ostringstream sql;

    sql << "INSERT INTO messages (lead_id, sender, text, is_read) VALUES ("
        << leadId << ","
        << "'" << esc2(sender) << "',"
        << "'" << esc2(text) << "',"
        << "0"
        << ");";

    Database::exec(sql.str());

    if (sender == "user") {
        Database::exec(
            "UPDATE leads SET last_message_at=CURRENT_TIMESTAMP WHERE id=" +
            std::to_string(leadId) + ";"
        );
    }
}

std::vector<LeadItem> LeadService::all(const std::string& folder)
{
    std::string where = "1=1";

    if (folder == "unread") {
        where =
            "EXISTS (SELECT 1 FROM messages m "
            "WHERE m.lead_id=leads.id AND m.sender='user' AND m.is_read=0)";
    }
    else if (folder == "fd") {
        where = "tags LIKE '%ФД%'";
    }
    else if (folder == "rd") {
        where = "tags LIKE '%РД%'";
    }
    else if (folder == "403") {
        where = "is_403=1 OR tags LIKE '%403%'";
    }

    auto rows = Database::query(
        "SELECT "
        "leads.*, "
        "(SELECT text FROM messages m WHERE m.lead_id=leads.id ORDER BY m.id DESC LIMIT 1) AS last_message, "
        "(SELECT COUNT(*) FROM messages m WHERE m.lead_id=leads.id AND m.sender='user' AND m.is_read=0) AS unread "
        "FROM leads "
        "WHERE " + where + " "
        "ORDER BY last_message_at DESC;"
    );

    std::vector<LeadItem> leads;

    for (auto& r : rows) {
        LeadItem l;
        l.id = std::stoi(r["id"]);
        l.tgUserId = r["tg_user_id"];
        l.username = r["username"];
        l.fullName = r["full_name"];
        l.channel = r["channel"];
        l.geo = r["geo"];
        l.subid = r["subid"];
        l.tags = r["tags"];
        l.comment = r["comment"];
        l.unread = r["unread"].empty() ? 0 : std::stoi(r["unread"]);
        l.createdAt = r["created_at"];
        l.lastMessageAt = r["last_message_at"];
        l.lastMessage = r["last_message"];
        leads.push_back(l);
    }

    return leads;
}

std::vector<MessageItem> LeadService::messages(int leadId)
{
    auto rows = Database::query(
        "SELECT id, lead_id, sender, text, media_type, media_id, created_at "
        "FROM messages "
        "WHERE lead_id=" + std::to_string(leadId) + " "
        "ORDER BY id ASC;"
    );

    std::vector<MessageItem> messages;

    for (auto& r : rows) {
        MessageItem m;
        m.id = std::stoi(r["id"]);
        m.leadId = std::stoi(r["lead_id"]);
        m.sender = r["sender"];
        m.text = r["text"];
        m.mediaType = r["media_type"];
        m.mediaId = r["media_id"];
        m.createdAt = r["created_at"];
        messages.push_back(m);
    }

    return messages;
}
LeadItem LeadService::findById(int leadId)
{
    LeadItem l{};
    l.id = 0;

    auto rows = Database::query(
        "SELECT * FROM leads WHERE id=" + std::to_string(leadId) + " LIMIT 1;"
    );

    if (rows.empty()) return l;

    auto& r = rows[0];

    l.id = std::stoi(r["id"]);
    l.tgUserId = r["tg_user_id"];
    l.username = r["username"];
    l.fullName = r["full_name"];
    l.channel = r["channel"];
    l.geo = r["geo"];
    l.subid = r["subid"];
    l.tags = r["tags"];
    l.comment = r["comment"];
    l.createdAt = r["created_at"];
    l.lastMessageAt = r["last_message_at"];

    return l;
}

void LeadService::markRead(int leadId)
{
    Database::exec(
        "UPDATE messages SET is_read=1 "
        "WHERE lead_id=" + std::to_string(leadId) + " AND sender='user';"
    );
}
static int countSql(const std::string& sql)
{
    auto rows = Database::query(sql);
    if (rows.empty()) return 0;
    return rows[0]["cnt"].empty() ? 0 : std::stoi(rows[0]["cnt"]);
}

FolderCounts LeadService::counts()
{
    FolderCounts c{};

    c.unread = countSql(
        "SELECT COUNT(*) AS cnt FROM leads "
        "WHERE EXISTS (SELECT 1 FROM messages m "
        "WHERE m.lead_id=leads.id AND m.sender='user' AND m.is_read=0);"
    );

    c.rd = countSql(
        "SELECT COUNT(*) AS cnt FROM leads "
        "WHERE tags LIKE '%РД%' AND is_403=0;"
    );

    c.fd = countSql(
        "SELECT COUNT(*) AS cnt FROM leads "
        "WHERE tags LIKE '%ФД%' AND is_403=0;"
    );

    c.all = countSql(
        "SELECT COUNT(*) AS cnt FROM leads "
        "WHERE is_403=0;"
    );

    c.bad403 = countSql(
        "SELECT COUNT(*) AS cnt FROM leads "
        "WHERE is_403=1 OR tags LIKE '%403%';"
    );

    return c;
}