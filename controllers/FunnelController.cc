#include "FunnelController.h"
#include "../db/Database.h"
#include <drogon/drogon.h>
#include <drogon/MultiPart.h>

#include <drogon/drogon.h>
#include <string>

using namespace drogon;

static std::string escFunnel(const std::string& v)
{
    return Database::escape(v);
}

void FunnelController::registerRoutes()
{
    app().registerHandler(
        "/api/funnels",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto rows = Database::query(
                "SELECT id, name, type, active, created_at "
                "FROM funnels "
                "ORDER BY id DESC"
            );

            Json::Value arr(Json::arrayValue);

            for (const auto& r : rows) {
                Json::Value item;
                item["id"] = r.at("id");
                item["name"] = r.at("name");
                item["type"] = r.at("type");
                item["active"] = r.at("active");
                item["created_at"] = r.at("created_at");
                arr.append(item);
            }

            auto resp = HttpResponse::newHttpJsonResponse(arr);
            callback(resp);
        },
        { Get }
    );

    app().registerHandler(
        "/api/funnels/file/{1}",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback, std::string fileName) {
            std::string safeName = fileName;
            if (safeName.find("..") != std::string::npos || safeName.find("/") != std::string::npos || safeName.find("\\") != std::string::npos) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            std::string path = "C:/Users/Rawa1zon/Documents/anydesk-web/uploads/funnels/" + safeName;
            auto resp = HttpResponse::newFileResponse(path);
            callback(resp);
        },
        { Get }
    );

    app().registerHandler(
        "/api/funnels/create",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();

            std::string name = json && json->isMember("name") ? (*json)["name"].asString() : "";
            std::string type = json && json->isMember("type") ? (*json)["type"].asString() : "fd";

            if (name.empty()) {
                Json::Value err;
                err["ok"] = false;
                err["error"] = "empty_name";
                callback(HttpResponse::newHttpJsonResponse(err));
                return;
            }

            if (type != "fd" && type != "rd") {
                type = "fd";
            }

            Database::exec(
                "INSERT INTO funnels (name, type, active, created_at) "
                "VALUES ('" + escFunnel(name) + "', '" + escFunnel(type) + "', 1, datetime('now'))"
            );

            auto rows = Database::query("SELECT last_insert_rowid() AS id");
            std::string funnelId = rows.empty() ? "0" : rows[0].at("id");

            Database::exec(
                "INSERT INTO funnel_steps (funnel_id, title, sort_order, tag_after, created_at) "
                "VALUES (" + funnelId + ", '1 шаг', 1, '1 шаг', datetime('now'))"
            );

            auto stepRows = Database::query("SELECT last_insert_rowid() AS id");
            std::string stepId = stepRows.empty() ? "0" : stepRows[0].at("id");

            Database::exec(
                "INSERT INTO funnel_messages (step_id, message_type, text, file_path, delay_seconds, sort_order, created_at) "
                "VALUES (" + stepId + ", 'text', '', '', 2.5, 1, datetime('now'))"
            );

            Json::Value ok;
            ok["ok"] = true;
            ok["id"] = funnelId;
            callback(HttpResponse::newHttpJsonResponse(ok));
        },
        { Post }
    );

    app().registerHandler(
        "/api/funnels/{1}",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback, std::string funnelId) {
            auto f = Database::query(
                "SELECT id, name, type, active, created_at "
                "FROM funnels WHERE id=" + funnelId + " LIMIT 1"
            );

            if (f.empty()) {
                Json::Value err;
                err["ok"] = false;
                err["error"] = "not_found";
                callback(HttpResponse::newHttpJsonResponse(err));
                return;
            }

            Json::Value root;
            root["id"] = f[0].at("id");
            root["name"] = f[0].at("name");
            root["type"] = f[0].at("type");
            root["active"] = f[0].at("active");

            Json::Value steps(Json::arrayValue);

            auto stepRows = Database::query(
                "SELECT id, title, sort_order, tag_after "
                "FROM funnel_steps "
                "WHERE funnel_id=" + funnelId + " "
                "ORDER BY sort_order ASC, id ASC"
            );

            for (const auto& s : stepRows) {
                Json::Value step;
                std::string stepId = s.at("id");

                step["id"] = stepId;
                step["title"] = s.at("title");
                step["sort_order"] = s.at("sort_order");
                step["tag_after"] = s.at("tag_after");

                Json::Value messages(Json::arrayValue);

                auto msgRows = Database::query(
                    "SELECT id, message_type, text, file_path, delay_seconds, sort_order "
                    "FROM funnel_messages "
                    "WHERE step_id=" + stepId + " "
                    "ORDER BY sort_order ASC, id ASC"
                );

                for (const auto& m : msgRows) {
                    Json::Value msg;
                    msg["id"] = m.at("id");
                    msg["message_type"] = m.at("message_type");
                    msg["text"] = m.at("text");
                    msg["file_path"] = m.at("file_path");
                    msg["delay_seconds"] = m.at("delay_seconds");
                    msg["sort_order"] = m.at("sort_order");
                    messages.append(msg);
                }

                step["messages"] = messages;
                steps.append(step);
            }

            root["steps"] = steps;
            callback(HttpResponse::newHttpJsonResponse(root));
        },
        { Get }
    );
    app().registerHandler(
        "/api/funnels/update",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();

            int id = json && json->isMember("id") ? (*json)["id"].asInt() : 0;
            std::string name = json && json->isMember("name") ? (*json)["name"].asString() : "";
            std::string type = json && json->isMember("type") ? (*json)["type"].asString() : "fd";

            if (id <= 0 || name.empty()) {
                Json::Value err;
                err["ok"] = false;
                err["error"] = "bad_request";
                callback(HttpResponse::newHttpJsonResponse(err));
                return;
            }

            if (type != "fd" && type != "rd") type = "fd";

            Database::exec(
                "UPDATE funnels SET "
                "name='" + Database::escape(name) + "', "
                "type='" + Database::escape(type) + "' "
                "WHERE id=" + std::to_string(id)
            );

            Json::Value ok;
            ok["ok"] = true;
            callback(HttpResponse::newHttpJsonResponse(ok));
        },
        { Post }
    );

    app().registerHandler(
        "/api/funnels/add-step",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();

            int funnelId = json && json->isMember("funnel_id") ? (*json)["funnel_id"].asInt() : 0;

            if (funnelId <= 0) {
                Json::Value err;
                err["ok"] = false;
                err["error"] = "bad_funnel";
                callback(HttpResponse::newHttpJsonResponse(err));
                return;
            }

            auto rows = Database::query(
                "SELECT COALESCE(MAX(sort_order), 0) + 1 AS next_order "
                "FROM funnel_steps WHERE funnel_id=" + std::to_string(funnelId)
            );

            int nextOrder = rows.empty() ? 1 : std::stoi(rows[0].at("next_order"));
            std::string title = std::to_string(nextOrder) + " шаг";

            Database::exec(
                "INSERT INTO funnel_steps (funnel_id, title, sort_order, tag_after, created_at) "
                "VALUES (" + std::to_string(funnelId) + ", '" + title + "', " + std::to_string(nextOrder) + ", '" + title + "', datetime('now'))"
            );

            auto stepRows = Database::query("SELECT last_insert_rowid() AS id");
            std::string stepId = stepRows.empty() ? "0" : stepRows[0].at("id");

            Database::exec(
                "INSERT INTO funnel_messages (step_id, message_type, text, file_path, delay_seconds, sort_order, created_at) "
                "VALUES (" + stepId + ", 'text', '', '', 2.5, 1, datetime('now'))"
            );

            Json::Value ok;
            ok["ok"] = true;
            callback(HttpResponse::newHttpJsonResponse(ok));
        },
        { Post }
    );

    app().registerHandler(
        "/api/funnels/add-message",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();

            int stepId = json && json->isMember("step_id") ? (*json)["step_id"].asInt() : 0;

            if (stepId <= 0) {
                Json::Value err;
                err["ok"] = false;
                err["error"] = "bad_step";
                callback(HttpResponse::newHttpJsonResponse(err));
                return;
            }

            auto rows = Database::query(
                "SELECT COALESCE(MAX(sort_order), 0) + 1 AS next_order "
                "FROM funnel_messages WHERE step_id=" + std::to_string(stepId)
            );

            int nextOrder = rows.empty() ? 1 : std::stoi(rows[0].at("next_order"));

            Database::exec(
                "INSERT INTO funnel_messages (step_id, message_type, text, file_path, delay_seconds, sort_order, created_at) "
                "VALUES (" + std::to_string(stepId) + ", 'text', '', '', 2.5, " + std::to_string(nextOrder) + ", datetime('now'))"
            );

            Json::Value ok;
            ok["ok"] = true;
            callback(HttpResponse::newHttpJsonResponse(ok));
        },
        { Post }
    );

    app().registerHandler(
        "/api/funnels/update-message",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();

            int id = json && json->isMember("id") ? (*json)["id"].asInt() : 0;
            std::string text = json && json->isMember("text") ? (*json)["text"].asString() : "";
            double delay = json && json->isMember("delay_seconds") ? (*json)["delay_seconds"].asDouble() : 2.5;

            if (id <= 0) {
                Json::Value err;
                err["ok"] = false;
                err["error"] = "bad_message";
                callback(HttpResponse::newHttpJsonResponse(err));
                return;
            }

            Database::exec(
                "UPDATE funnel_messages SET "
                "text='" + Database::escape(text) + "', "
                "delay_seconds=" + std::to_string(delay) + " "
                "WHERE id=" + std::to_string(id)
            );

            Json::Value ok;
            ok["ok"] = true;
            callback(HttpResponse::newHttpJsonResponse(ok));
        },
        { Post }
    );

    app().registerHandler(
        "/api/funnels/upload-message-file",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            MultiPartParser parser;

            if (parser.parse(req) != 0) {
                Json::Value err;
                err["ok"] = false;
                err["error"] = "upload_parse_failed";
                callback(HttpResponse::newHttpJsonResponse(err));
                return;
            }

            int messageId = 0;
            std::string mediaType = "document";

            for (const auto& p : parser.getParameters()) {
                if (p.first == "message_id") {
                    try { messageId = std::stoi(p.second); }
                    catch (...) { messageId = 0; }
                }

                if (p.first == "message_type") {
                    mediaType = p.second;
                }
            }

            if (messageId <= 0) {
                Json::Value err;
                err["ok"] = false;
                err["error"] = "bad_message_id";
                callback(HttpResponse::newHttpJsonResponse(err));
                return;
            }

            if (mediaType != "photo" && mediaType != "voice" && mediaType != "document") {
                mediaType = "document";
            }

            auto files = parser.getFiles();

            if (files.empty()) {
                Json::Value err;
                err["ok"] = false;
                err["error"] = "no_file";
                callback(HttpResponse::newHttpJsonResponse(err));
                return;
            }

            auto file = files[0];

            std::string dir = "C:/Users/Rawa1zon/Documents/anydesk-web/uploads/funnels/";
            std::string filename = std::to_string(messageId) + "_" + file.getFileName();
            std::string fullPath = dir + filename;

            file.saveAs(fullPath);

            Database::exec(
                "UPDATE funnel_messages SET "
                "message_type='" + Database::escape(mediaType) + "', "
                "file_path='" + Database::escape(fullPath) + "' "
                "WHERE id=" + std::to_string(messageId)
            );

            Json::Value ok;
            ok["ok"] = true;
            ok["file_path"] = fullPath;
            ok["message_type"] = mediaType;
            callback(HttpResponse::newHttpJsonResponse(ok));
        },
        { Post }
    );

    app().registerHandler(
        "/api/funnels/delete-message",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();
            int id = json && json->isMember("id") ? (*json)["id"].asInt() : 0;

            if (id > 0) {
                auto rows = Database::query(
                    "SELECT step_id FROM funnel_messages "
                    "WHERE id=" + std::to_string(id) + " "
                    "LIMIT 1"
                );

                if (!rows.empty()) {
                    std::string stepId = rows[0].at("step_id");

                    Database::exec(
                        "DELETE FROM funnel_messages "
                        "WHERE id=" + std::to_string(id)
                    );

                    auto left = Database::query(
                        "SELECT COUNT(*) AS cnt "
                        "FROM funnel_messages "
                        "WHERE step_id=" + stepId
                    );

                    int cnt = 0;

                    if (!left.empty()) {
                        try {
                            cnt = std::stoi(left[0].at("cnt"));
                        }
                        catch (...) {
                            cnt = 0;
                        }
                    }

                    if (cnt <= 0) {
                        Database::exec(
                            "DELETE FROM funnel_steps "
                            "WHERE id=" + stepId
                        );
                    }
                }
            }

            Json::Value ok;
            ok["ok"] = true;
            callback(HttpResponse::newHttpJsonResponse(ok));
        },
        { Post }
    );
}