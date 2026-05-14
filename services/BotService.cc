#include "BotService.h"
#include "../db/Database.h"

#include <sstream>

static std::string esc(const std::string& s)
{
    std::string out;
    for (char c : s) {
        if (c == '\'') out += "''";
        else out += c;
    }
    return out;
}

bool BotService::create(
    const std::string& name,
    const std::string& token,
    const std::string& geo,
    const std::string& funnel
)
{
    if (name.empty() || token.empty()) {
        return false;
    }

    std::ostringstream sql;
    sql << "INSERT INTO bots (name, token, geo, funnel, active) VALUES ("
        << "'" << esc(name) << "',"
        << "'" << esc(token) << "',"
        << "'" << esc(geo) << "',"
        << "'" << esc(funnel) << "',"
        << "1"
        << ");";

    return Database::exec(sql.str());
}

std::vector<BotItem> BotService::all()
{
    std::vector<BotItem> bots;

    auto rows = Database::query(
        "SELECT id, name, token, geo, funnel, active, created_at "
        "FROM bots ORDER BY id DESC;"
    );

    for (auto& r : rows) {
        BotItem b;
        b.id = std::stoi(r["id"]);
        b.name = r["name"];
        b.token = r["token"];
        b.geo = r["geo"];
        b.funnel = r["funnel"];
        b.active = std::stoi(r["active"]);
        b.createdAt = r["created_at"];
        bots.push_back(b);
    }

    return bots;
}
std::string BotService::tokenByChannel(const std::string& channel)
{
    auto rows = Database::query(
        "SELECT token FROM bots WHERE name='" + esc(channel) + "' AND active=1 LIMIT 1;"
    );

    if (rows.empty()) return "";

    return rows[0]["token"];
}