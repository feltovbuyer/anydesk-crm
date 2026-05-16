#include "DuplicateLeadService.h"
#include "AssignmentService.h"
#include "../db/Database.h"

#include <string>
#include <vector>

int DuplicateLeadService::rootLeadId(int leadId)
{
    auto rows = Database::query(
        "SELECT id, duplicate_of "
        "FROM leads "
        "WHERE id=" + std::to_string(leadId) + " "
        "LIMIT 1"
    );

    if (rows.empty()) {
        return leadId;
    }

    int duplicateOf = 0;

    try {
        duplicateOf = std::stoi(rows[0].at("duplicate_of"));
    }
    catch (...) {
        duplicateOf = 0;
    }

    if (duplicateOf > 0) {
        return duplicateOf;
    }

    return leadId;
}

std::vector<int> DuplicateLeadService::groupLeadIds(int leadId)
{
    int rootId = rootLeadId(leadId);

    auto rows = Database::query(
        "SELECT id "
        "FROM leads "
        "WHERE id=" + std::to_string(rootId) + " "
        "OR duplicate_of=" + std::to_string(rootId) + " "
        "ORDER BY id ASC"
    );

    std::vector<int> ids;

    for (const auto& r : rows) {
        try {
            ids.push_back(std::stoi(r.at("id")));
        }
        catch (...) {
        }
    }

    if (ids.empty()) {
        ids.push_back(rootId);
    }

    return ids;
}

LeadGroupInfo DuplicateLeadService::groupInfo(int leadId)
{
    LeadGroupInfo info;
    info.rootLeadId = rootLeadId(leadId);
    info.leadIds = groupLeadIds(leadId);
    info.mainChannel = mainChannel(leadId);
    return info;
}

bool DuplicateLeadService::isDuplicate(int leadId)
{
    auto rows = Database::query(
        "SELECT is_duplicate "
        "FROM leads "
        "WHERE id=" + std::to_string(leadId) + " "
        "LIMIT 1"
    );

    if (rows.empty()) {
        return false;
    }

    return rows[0].at("is_duplicate") == "1";
}

std::string DuplicateLeadService::mainChannel(int leadId)
{
    int rootId = rootLeadId(leadId);

    auto rows = Database::query(
        "SELECT channel "
        "FROM leads "
        "WHERE id=" + std::to_string(rootId) + " "
        "LIMIT 1"
    );

    if (rows.empty()) {
        return "";
    }

    return rows[0].at("channel");
}

bool DuplicateLeadService::transferWholeGroupToManager(int leadId, const std::string& managerLogin)
{
    auto ids = groupLeadIds(leadId);

    bool ok = false;

    for (int id : ids) {
        if (AssignmentService::transferLeadToManager(id, managerLogin)) {
            ok = true;
        }
    }

    return ok;
}
int DuplicateLeadService::findLeadForChannel(long long tgUserId, const std::string& channel)
{
    auto rows = Database::query(
        "SELECT id FROM leads "
        "WHERE tg_user_id='" + std::to_string(tgUserId) + "' "
        "AND channel='" + Database::escape(channel) + "' "
        "LIMIT 1"
    );

    if (rows.empty()) return 0;

    try {
        return std::stoi(rows[0].at("id"));
    }
    catch (...) {
        return 0;
    }
}

int DuplicateLeadService::findRootLeadForUser(long long tgUserId)
{
    auto rows = Database::query(
        "SELECT id FROM leads "
        "WHERE tg_user_id='" + std::to_string(tgUserId) + "' "
        "AND is_duplicate=0 "
        "ORDER BY id ASC "
        "LIMIT 1"
    );

    if (rows.empty()) return 0;

    try {
        return std::stoi(rows[0].at("id"));
    }
    catch (...) {
        return 0;
    }
}

bool DuplicateLeadService::markAsDuplicate(int leadId, int rootLeadId)
{
    if (leadId <= 0 || rootLeadId <= 0 || leadId == rootLeadId) {
        return false;
    }

    std::string main = mainChannel(rootLeadId);

    Database::exec(
        "UPDATE leads SET "
        "is_duplicate=1, "
        "duplicate_of=" + std::to_string(rootLeadId) + ", "
        "automation_enabled=0, "
        "main_channel='" + Database::escape(main) + "' "
        "WHERE id=" + std::to_string(leadId)
    );

    return true;
}