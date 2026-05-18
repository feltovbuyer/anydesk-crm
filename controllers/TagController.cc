#include "TagController.h"
#include "../db/Database.h"
#include <set>

#include <drogon/drogon.h>
#include <string>

using namespace drogon;

void TagController::registerRoutes()
{
    app().registerHandler(
        "/api/tags",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback)
        {
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
        "/api/broadcast/tags",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback)
        {
            Json::Value arr(Json::arrayValue);
            std::set<std::string> used;

            auto addTag = [&](const std::string& name,
                const std::string& bg,
                const std::string& text,
                int systemTag)
                {
                    if (name.empty()) return;
                    if (used.count(name)) return;

                    used.insert(name);

                    Json::Value item;
                    item["id"] = "";
                    item["name"] = name;
                    item["bg_color"] = bg;
                    item["text_color"] = text;
                    item["system_tag"] = systemTag;
                    item["active"] = 1;

                    arr.append(item);
                };

            auto catalogRows = Database::query(
                "SELECT id, name, bg_color, text_color, system_tag, active "
                "FROM tags_catalog "
                "WHERE active=1 "
                "ORDER BY system_tag DESC, name ASC"
            );

            for (const auto& r : catalogRows) {
                std::string name = r.count("name") ? r.at("name") : "";
                std::string bg = r.count("bg_color") ? r.at("bg_color") : "#334155";
                std::string text = r.count("text_color") ? r.at("text_color") : "#ffffff";
                int systemTag = r.count("system_tag") ? std::stoi(r.at("system_tag")) : 0;

                addTag(name, bg, text, systemTag);
            }

            auto botRows = Database::query(
                "SELECT name, geo "
                "FROM bots "
                "WHERE active=1 "
                "ORDER BY name ASC"
            );

            for (const auto& r : botRows) {
                std::string channel = r.count("name") ? r.at("name") : "";
                std::string geo = r.count("geo") ? r.at("geo") : "";

                addTag(channel, "#312e81", "#ddd6fe", 1);
                addTag(geo, "#14532d", "#bbf7d0", 1);
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