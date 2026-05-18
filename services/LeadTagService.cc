#include "LeadTagService.h"
#include "../db/Database.h"

#include <sstream>
#include <vector>
#include <set>

static std::vector<std::string> splitTags(const std::string& tags)
{
    std::vector<std::string> out;
    std::stringstream ss(tags);
    std::string item;

    while (std::getline(ss, item, ',')) {
        while (!item.empty() && item.front() == ' ') item.erase(item.begin());
        while (!item.empty() && item.back() == ' ') item.pop_back();

        if (!item.empty()) {
            out.push_back(item);
        }
    }

    return out;
}

static std::string joinTags(const std::vector<std::string>& tags)
{
    std::string out;

    for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) out += ", ";
        out += tags[i];
    }

    return out;
}

static bool isFunnelTag(const std::string& tag)
{
    return tag == "1 шаг"
        || tag == "2 шаг"
        || tag == "3 шаг"
        || tag == "Прошел воронку";
}

std::string LeadTagService::normalizeTags(const std::string& tags)
{
    auto list = splitTags(tags);

    std::vector<std::string> out;
    std::set<std::string> seen;

    for (const auto& t : list) {
        if (seen.count(t)) continue;
        seen.insert(t);
        out.push_back(t);
    }

    return joinTags(out);
}

std::string LeadTagService::removeFunnelTags(const std::string& tags)
{
    auto list = splitTags(tags);

    std::vector<std::string> out;

    for (const auto& t : list) {
        if (isFunnelTag(t)) continue;
        out.push_back(t);
    }

    return joinTags(out);
}

void LeadTagService::setFunnelTag(int leadId, const std::string& tag)
{
    auto rows = Database::query(
        "SELECT tags FROM leads WHERE id=" + std::to_string(leadId) + " LIMIT 1"
    );

    if (rows.empty()) return;

    std::string tags = removeFunnelTags(rows[0].at("tags"));

    if (!tags.empty()) {
        tags += ", ";
    }

    tags += tag;
    tags = normalizeTags(tags);

    Database::exec(
        "UPDATE leads SET tags='" + Database::escape(tags) + "' "
        "WHERE id=" + std::to_string(leadId)
    );
}

void LeadTagService::finishFd(int leadId)
{
    setFunnelTag(leadId, "Прошел воронку");
}