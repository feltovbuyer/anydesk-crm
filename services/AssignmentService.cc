#include "AssignmentService.h"
#include "../db/Database.h"

#include <random>
#include <sstream>
#include <vector>

struct ManagerPick {
    std::string login;
    std::string tag;
};

static ManagerPick pickActiveFdManager()
{
    auto rows = Database::query(
        "SELECT sd.staff_login, sd.manager_tag, sd.percent "
        "FROM staff_distribution sd "
        "JOIN staff s ON s.login = sd.staff_login "
        "WHERE sd.active=1 "
        "AND s.active=1 "
        "AND sd.percent > 0 "
        "AND sd.work_type IN ('fd', 'fd_rd')"
    );

    int total = 0;
    for (const auto& r : rows) {
        total += std::stoi(r.at("percent"));
    }

    if (rows.empty() || total <= 0) {
        return {};
    }

    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(1, total);

    int roll = dist(rng);
    int acc = 0;

    for (const auto& r : rows) {
        acc += std::stoi(r.at("percent"));

        if (roll <= acc) {
            return { r.at("staff_login"), r.at("manager_tag") };
        }
    }

    return { rows[0].at("staff_login"), rows[0].at("manager_tag") };
}

static bool isManagerTag(const std::string& tag)
{
    auto rows = Database::query(
        "SELECT 1 FROM staff_distribution "
        "WHERE manager_tag='" + Database::escape(tag) + "' "
        "LIMIT 1"
    );

    return !rows.empty();
}

static std::string replaceManagerTag(const std::string& tags, const std::string& newTag)
{
    std::vector<std::string> keep;
    std::stringstream ss(tags);
    std::string part;

    while (std::getline(ss, part, ',')) {
        if (part.empty()) continue;

        if (!isManagerTag(part)) {
            keep.push_back(part);
        }
    }

    if (!newTag.empty()) {
        keep.push_back(newTag);
    }

    std::string result;
    for (size_t i = 0; i < keep.size(); ++i) {
        if (i > 0) result += ",";
        result += keep[i];
    }

    return result;
}

bool AssignmentService::transferLeadToManager(int leadId, const std::string& managerLogin)
{
    std::string safeLogin = Database::escape(managerLogin);

    auto managerRows = Database::query(
        "SELECT s.login, COALESCE(sd.manager_tag, s.login) AS manager_tag "
        "FROM staff s "
        "LEFT JOIN staff_distribution sd ON sd.staff_login = s.login "
        "WHERE s.login='" + safeLogin + "' "
        "AND s.role='manager' "
        "AND s.active=1 "
        "LIMIT 1"
    );

    if (managerRows.empty()) {
        return false;
    }

    auto leadRows = Database::query(
        "SELECT tags FROM leads "
        "WHERE id=" + std::to_string(leadId) + " "
        "LIMIT 1"
    );

    if (leadRows.empty()) {
        return false;
    }

    std::string newTag = managerRows[0].at("manager_tag");
    std::string finalTags = replaceManagerTag(leadRows[0].at("tags"), newTag);

    Database::exec(
        "UPDATE leads SET "
        "assigned_staff='" + safeLogin + "', "
        "tags='" + Database::escape(finalTags) + "' "
        "WHERE id=" + std::to_string(leadId)
    );

    return true;
}

void AssignmentService::reassignIfInactiveManager(int leadId)
{
    auto leadRows = Database::query(
        "SELECT assigned_staff FROM leads "
        "WHERE id=" + std::to_string(leadId) + " "
        "LIMIT 1"
    );

    if (leadRows.empty()) {
        return;
    }

    std::string assigned = leadRows[0].at("assigned_staff");

    if (assigned.empty()) {
        ManagerPick picked = pickActiveFdManager();
        if (!picked.login.empty()) {
            transferLeadToManager(leadId, picked.login);
        }
        return;
    }

    auto staffRows = Database::query(
        "SELECT active FROM staff "
        "WHERE login='" + Database::escape(assigned) + "' "
        "LIMIT 1"
    );

    if (!staffRows.empty() && staffRows[0].at("active") == "1") {
        return;
    }

    ManagerPick picked = pickActiveFdManager();

    if (picked.login.empty()) {
        return;
    }

    if (picked.login == assigned) {
        return;
    }

    transferLeadToManager(leadId, picked.login);
}