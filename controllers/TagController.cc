#include "TagController.h"
#include "../db/Database.h"

#include <drogon/drogon.h>
#include <string>

using namespace drogon;

void TagController::registerRoutes()
{
    app().registerHandler(
        "/api/tags",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto rows = Database::query(
                "SELECT id, name, bg_color, text_color, system_tag, active "
                "FROM tags_catalog "
                "WHERE active=1 "
                "ORDER BY system_tag DESC, name ASC"
            );

            Json::Value arr(Json::arrayValue);

            for (const auto& r : rows) {
                Json::Value item;
                item["id"] = r.at("id");
                item["name"] = r.at("name");
                item["bg_color"] = r.at("bg_color");
                item["text_color"] = r.at("text_color");
                item["system_tag"] = r.at("system_tag");
                item["active"] = r.at("active");
                arr.append(item);
            }

            callback(HttpResponse::newHttpJsonResponse(arr));
        },
        { Get }
    );

    app().registerHandler(
        "/api/tags/save",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();

            std::string name = json && json->isMember("name") ? (*json)["name"].asString() : "";
            std::string bg = json && json->isMember("bg_color") ? (*json)["bg_color"].asString() : "#1e293b";
            std::string text = json && json->isMember("text_color") ? (*json)["text_color"].asString() : "#ffffff";

            if (name.empty()) {
                Json::Value err;
                err["ok"] = false;
                err["error"] = "empty_name";
                callback(HttpResponse::newHttpJsonResponse(err));
                return;
            }

            Database::exec(
                "INSERT INTO tags_catalog (name, bg_color, text_color, system_tag, active, created_at) "
                "VALUES ('" + Database::escape(name) + "', '" + Database::escape(bg) + "', '" + Database::escape(text) + "', 0, 1, datetime('now')) "
                "ON CONFLICT(name) DO UPDATE SET "
                "bg_color=excluded.bg_color, "
                "text_color=excluded.text_color, "
                "active=1"
            );

            Json::Value ok;
            ok["ok"] = true;
            callback(HttpResponse::newHttpJsonResponse(ok));
        },
        { Post }
    );

    app().registerHandler(
        "/api/tags/delete",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();
            int id = json && json->isMember("id") ? (*json)["id"].asInt() : 0;

            if (id > 0) {
                Database::exec(
                    "UPDATE tags_catalog SET active=0 "
                    "WHERE id=" + std::to_string(id) + " "
                    "AND system_tag=0"
                );
            }

            Json::Value ok;
            ok["ok"] = true;
            callback(HttpResponse::newHttpJsonResponse(ok));
        },
        { Post }
    );
}