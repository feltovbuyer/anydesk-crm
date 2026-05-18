#include "LeadController.h"
#include "../services/LeadService.h"
#include "../services/BotService.h"
#include "../services/TelegramService.h"
#include "../db/Database.h"
#include <filesystem>
#include <sstream>
#include "../services/TelegramFileService.h"

#include <drogon/drogon.h>

void LeadController::registerRoutes()
{
    drogon::app().registerHandler(
        "/api/leads",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            std::string folder = req->getParameter("folder");

            std::string role = "admin";
            std::string login = "";

            auto session = req->session();
            if (session) {
                try {
                    role = session->get<std::string>("role");
                    login = session->get<std::string>("login");
                }
                catch (...) {
                    role = "admin";
                    login = "";
                }
            }

            auto leads = LeadService::all(folder, role, login);

            Json::Value arr(Json::arrayValue);

            for (const auto& l : leads) {
                Json::Value item;
                item["id"] = l.id;
                item["tg_user_id"] = l.tgUserId;
                item["username"] = l.username;
                item["full_name"] = l.fullName;
                item["channel"] = l.channel;
                item["geo"] = l.geo;
                item["subid"] = l.subid;
                item["tags"] = l.tags;
                item["comment"] = l.comment;
                item["unread"] = l.unread;
                item["created_at"] = l.createdAt;
                item["last_message_at"] = l.lastMessageAt;
                item["last_message"] = l.lastMessage;
                arr.append(item);
            }

            Json::Value json;
            json["ok"] = true;
            json["leads"] = arr;

            callback(drogon::HttpResponse::newHttpJsonResponse(json));
        },
        { drogon::Get }
    );

    drogon::app().registerHandler(
        "/api/folder-counts",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            std::string role = "admin";
            std::string login = "";

            auto session = req->session();
            if (session) {
                try {
                    role = session->get<std::string>("role");
                    login = session->get<std::string>("login");
                }
                catch (...) {
                    role = "admin";
                    login = "";
                }
            }

            auto c = LeadService::counts(role, login);

            Json::Value json;
            json["ok"] = true;
            json["unread"] = c.unread;
            json["rd"] = c.rd;
            json["fd"] = c.fd;
            json["all"] = c.all;
            json["bad403"] = c.bad403;

            callback(drogon::HttpResponse::newHttpJsonResponse(json));
        },
        { drogon::Get }
    );

    drogon::app().registerHandler(
        "/api/lead/mark-unread",
        [](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            Json::Value json;

            int leadId = 0;
            try {
                leadId = std::stoi(req->getParameter("lead_id"));
            }
            catch (...) {
                leadId = 0;
            }

            if (leadId <= 0) {
                json["ok"] = false;
                json["error"] = "lead_id required";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            Database::exec(
                "UPDATE messages SET is_read=0 "
                "WHERE lead_id=" + std::to_string(leadId) + " "
                "AND sender='user'"
            );

            json["ok"] = true;
            callback(drogon::HttpResponse::newHttpJsonResponse(json));
        },
        { drogon::Post }
    );

    drogon::app().registerHandler(
        "/api/leads/add-tag",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            auto json = req->getJsonObject();

            if (!json) {
                Json::Value out;
                out["ok"] = false;
                callback(drogon::HttpResponse::newHttpJsonResponse(out));
                return;
            }

            int leadId = (*json)["lead_id"].asInt();
            std::string tag = (*json)["tag"].asString();

            if (leadId <= 0 || tag.empty()) {
                Json::Value out;
                out["ok"] = false;
                callback(drogon::HttpResponse::newHttpJsonResponse(out));
                return;
            }

            auto rows = Database::query(
                "SELECT tags FROM leads WHERE id=" + std::to_string(leadId) + " LIMIT 1"
            );

            std::string tags = rows.empty() ? "" : rows[0]["tags"];

            bool exists = false;

            std::stringstream ss(tags);
            std::string item;

            while (std::getline(ss, item, ',')) {
                while (!item.empty() && item.front() == ' ')
                    item.erase(0, 1);

                while (!item.empty() && item.back() == ' ')
                    item.pop_back();

                if (item == tag) {
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                if (!tags.empty())
                    tags += ", ";

                tags += tag;

                Database::exec(
                    "UPDATE leads SET tags='" +
                    Database::escape(tags) +
                    "' WHERE id=" + std::to_string(leadId)
                );
            }

            Json::Value out;
            out["ok"] = true;
            callback(drogon::HttpResponse::newHttpJsonResponse(out));
        },
        { drogon::Post }
    );

    drogon::app().registerHandler(
        "/api/messages/send-file",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            Json::Value json;

            drogon::MultiPartParser parser;

            if (parser.parse(req) != 0) {
                json["ok"] = false;
                json["error"] = "multipart parse failed";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            auto params = parser.getParameters();

            int leadId = 0;
            try {
                leadId = std::stoi(params["lead_id"]);
            }
            catch (...) {
                leadId = 0;
            }

            std::string mediaType = params["media_type"];
            std::string caption = params["caption"];

            if (leadId <= 0 || mediaType.empty()) {
                json["ok"] = false;
                json["error"] = "lead_id/media_type required";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            auto files = parser.getFiles();

            if (files.empty()) {
                json["ok"] = false;
                json["error"] = "file required";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            auto lead = LeadService::findById(leadId);

            if (lead.id <= 0) {
                json["ok"] = false;
                json["error"] = "lead not found";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            std::string token = BotService::tokenByChannel(lead.channel);

            if (token.empty()) {
                json["ok"] = false;
                json["error"] = "bot token not found";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            std::filesystem::create_directories("uploads");

            const auto& file = files[0];

            std::string filename = file.getFileName();
            if (filename.empty()) {
                filename = "upload.bin";
            }

            auto content = file.fileContent();
            std::string bytes(content.data(), content.size());

            auto sendResult = TelegramService::sendFileBytes(
                token,
                std::stoll(lead.tgUserId),
                mediaType,
                file.getFileName(),
                bytes,
                caption
            );

            if (!sendResult.ok) {
                json["ok"] = false;
                json["error"] = "telegram file send failed";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            std::string mediaId = sendResult.fileId;

            std::string safeText = Database::escape(caption);
            std::string safeType = Database::escape(mediaType);
            std::string safeMediaId = Database::escape(mediaId);

            Database::exec(
                "INSERT INTO messages (lead_id, sender, text, media_type, media_id, is_read, created_at) "
                "VALUES (" + std::to_string(leadId) + ", 'admin', '" + safeText + "', '" +
                safeType + "', '" + safeMediaId + "', 1, datetime('now'))"
            );

            LeadService::markRead(leadId);

            json["ok"] = true;
            callback(drogon::HttpResponse::newHttpJsonResponse(json));
        },
        { drogon::Post }
    );

    drogon::app().registerHandler(
        "/api/tg-file",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            std::string channel = req->getParameter("channel");
            std::string fileId = req->getParameter("file_id");

            Json::Value json;

            if (channel.empty() || fileId.empty()) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k400BadRequest);
                callback(resp);
                return;
            }

            std::string token = BotService::tokenByChannel(channel);

            if (token.empty()) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k404NotFound);
                callback(resp);
                return;
            }

            std::string bytes;
            std::string contentType;

            if (!TelegramFileService::loadFileBytes(token, fileId, bytes, contentType)) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k404NotFound);
                callback(resp);
                return;
            }

            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setContentTypeString(contentType);
            resp->setBody(std::move(bytes));
            callback(resp);
        },
        { drogon::Get }
    );

    drogon::app().registerHandler(
        "/api/messages/send",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            int leadId = 0;

            try {
                leadId = std::stoi(req->getParameter("lead_id"));
            }
            catch (...) {
                leadId = 0;
            }

            std::string text = req->getParameter("text");

            Json::Value json;

            if (leadId <= 0 || text.empty()) {
                json["ok"] = false;
                json["error"] = "lead_id/text required";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            auto lead = LeadService::findById(leadId);

            if (lead.id <= 0) {
                json["ok"] = false;
                json["error"] = "lead not found";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            std::string token = BotService::tokenByChannel(lead.channel);

            if (token.empty()) {
                json["ok"] = false;
                json["error"] = "bot token not found";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            bool sent = TelegramService::sendText(
                token,
                std::stoll(lead.tgUserId),
                text
            );

            if (!sent) {
                json["ok"] = false;
                json["error"] = "telegram send failed";
                callback(drogon::HttpResponse::newHttpJsonResponse(json));
                return;
            }

            LeadService::addMessage(leadId, "admin", text);
            LeadService::markRead(leadId);

            json["ok"] = true;
            callback(drogon::HttpResponse::newHttpJsonResponse(json));
        },
        { drogon::Post }
    );

    drogon::app().registerHandler(
        "/api/messages",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            int leadId = 0;

            try {
                leadId = std::stoi(req->getParameter("lead_id"));
            }
            catch (...) {
                leadId = 0;
            }

            auto messages = LeadService::messages(leadId);

            Json::Value arr(Json::arrayValue);

            for (const auto& m : messages) {
                Json::Value item;
                item["id"] = m.id;
                item["lead_id"] = m.leadId;
                item["sender"] = m.sender;
                item["text"] = m.text;
                item["media_type"] = m.mediaType;
                item["media_id"] = m.mediaId;
                item["created_at"] = m.createdAt;
                arr.append(item);
            }

            Json::Value json;
            json["ok"] = true;
            json["messages"] = arr;

            callback(drogon::HttpResponse::newHttpJsonResponse(json));
        },
        { drogon::Get }
    );
}