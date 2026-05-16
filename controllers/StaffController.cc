#include "StaffController.h"
#include "../db/Database.h"

#include <drogon/drogon.h>
#include <json/json.h>

static std::string escStaff(const std::string& s)
{
    return Database::escape(s);
}

void StaffController::registerRoutes()
{
    drogon::app().registerHandler(
        "/api/staff",
        [](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto rows = Database::query(
                "SELECT s.id, s.login, s.password, s.role, s.active, "
                "COALESCE(sd.manager_tag, '') AS manager_tag, "
                "COALESCE(sd.work_type, 'fd_rd') AS work_type, "
                "COALESCE(sd.percent, 0) AS percent "
                "FROM staff s "
                "LEFT JOIN staff_distribution sd ON sd.staff_login = s.login "
                "WHERE s.role='manager' "
                "ORDER BY s.id ASC"
            );

            Json::Value arr(Json::arrayValue);

            for (const auto& r : rows) {
                Json::Value item;
                item["id"] = r.at("id");
                item["login"] = r.at("login");
                item["password"] = r.at("password");
                item["role"] = r.at("role");
                item["active"] = r.at("active");
                item["manager_tag"] = r.at("manager_tag");
                item["work_type"] = r.at("work_type");
                item["percent"] = r.at("percent");
                arr.append(item);
            }

            Json::Value json;
            json["ok"] = true;
            json["staff"] = arr;

            callback(drogon::HttpResponse::newHttpJsonResponse(json));
        },
        { drogon::Get }
    );

    drogon::app().registerHandler(
        "/api/staff/toggle",
        [](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            std::string login = req->getParameter("login");

            Json::Value json;

            if (login.empty()) {
                json["ok"] = false;
                json["error"] = "login required";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            std::string safeLogin = Database::escape(login);

            auto rows = Database::query(
                "SELECT active FROM staff "
                "WHERE login='" + safeLogin + "' "
                "LIMIT 1"
            );

            if (rows.empty()) {
                json["ok"] = false;
                json["error"] = "staff not found";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            int newState = rows[0].at("active") == "1" ? 0 : 1;

            Database::exec(
                "UPDATE staff SET active=" + std::to_string(newState) + " "
                "WHERE login='" + safeLogin + "'"
            );

            json["ok"] = true;
            json["active"] = newState;
            callback(drogon::HttpResponse::newHttpJsonResponse(json));
        },
        { drogon::Post }
    );

    drogon::app().registerHandler(
        "/api/staff/save",
        [](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            std::string login = req->getParameter("login");
            std::string password = req->getParameter("password");
            std::string managerTag = req->getParameter("manager_tag");
            std::string workType = req->getParameter("work_type");
            std::string percentRaw = req->getParameter("percent");

            Json::Value json;

            if (login.empty() || password.empty() || managerTag.empty()) {
                json["ok"] = false;
                json["error"] = "login/password/manager_tag required";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            if (workType != "fd" && workType != "rd" && workType != "fd_rd") {
                workType = "fd_rd";
            }

            int percent = 0;
            try {
                percent = std::stoi(percentRaw);
            }
            catch (...) {
                percent = 0;
            }

            if (percent < 0) percent = 0;
            if (percent > 100) percent = 100;

            std::string safeLogin = escStaff(login);
            std::string safePassword = escStaff(password);
            std::string safeTag = escStaff(managerTag);
            std::string safeWorkType = escStaff(workType);

            Database::exec(
                "INSERT INTO staff (login, password, role, active) "
                "VALUES ('" + safeLogin + "', '" + safePassword + "', 'manager', 1) "
                "ON CONFLICT(login) DO UPDATE SET "
                "password=excluded.password, "
                "role='manager', "
                "active=1"
            );

            Database::exec(
                "INSERT INTO staff_distribution (staff_login, manager_tag, work_type, percent, active) "
                "VALUES ('" + safeLogin + "', '" + safeTag + "', '" + safeWorkType + "', " + std::to_string(percent) + ", 1) "
                "ON CONFLICT(staff_login) DO UPDATE SET "
                "manager_tag=excluded.manager_tag, "
                "work_type=excluded.work_type, "
                "percent=excluded.percent, "
                "active=1"
            );

            json["ok"] = true;
            callback(drogon::HttpResponse::newHttpJsonResponse(json));
        },
        { drogon::Post }
    );
}