#include "LeadController.h"
#include "../services/LeadService.h"
#include "../services/BotService.h"
#include "../services/TelegramService.h"
#include "../db/Database.h"
#include <filesystem>
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
            auto leads = LeadService::all(folder);

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
            auto c = LeadService::counts();

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