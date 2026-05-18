#include "BroadcastService.h"
#include "../db/Database.h"

#include <curl/curl.h>
#include <map>
#include <json/json.h>
#include <iostream>
#include <memory>


std::string BroadcastService::esc(const std::string& s)
{
    std::string out = s;
    size_t pos = 0;

    while ((pos = out.find("'", pos)) != std::string::npos) {
        out.replace(pos, 1, "''");
        pos += 2;
    }

    return out;
}

std::string BroadcastService::buildWhere(
    const std::vector<std::string>& includeTags,
    const std::vector<std::string>& excludeTags,
    const std::string& dateFrom,
    const std::string& dateTo
)
{
    std::string sql = " WHERE 1=1";

    auto tagCond = [&](const std::string& tag) {
        std::string t = esc(tag);

        return
            "("
            "tags LIKE '%" + t + "%' "
            "OR geo = '" + t + "' "
            "OR channel = '" + t + "'"
            ")";
        };

    for (const auto& tag : includeTags) {
        sql += " AND " + tagCond(tag);
    }

    for (const auto& tag : excludeTags) {
        sql += " AND NOT " + tagCond(tag);
    }

    if (!dateFrom.empty()) {
        sql += " AND created_at >= '" + esc(dateFrom) + "'";
    }

    if (!dateTo.empty()) {
        sql += " AND created_at <= '" + esc(dateTo) + " 23:59:59'";
    }

    return sql;
}

int BroadcastService::previewCount(const BroadcastPreviewRequest& req)
{
    std::string sql =
        "SELECT COUNT(*) AS cnt FROM leads" +
        buildWhere(req.includeTags, req.excludeTags, req.dateFrom, req.dateTo);

    auto rows = Database::query(sql);

    if (rows.empty()) return 0;

    return std::stoi(rows[0]["cnt"]);
}

BroadcastStartResult BroadcastService::start(const BroadcastStartRequest& req)
{
    BroadcastStartResult result;

    if (req.text.empty() && req.filePath.empty()) {
        return result;
    }

    std::string sql =
        "SELECT id, tg_user_id, channel "
        "FROM leads" +
        buildWhere(req.includeTags, req.excludeTags, req.dateFrom, req.dateTo);

    auto leads = Database::query(sql);

    result.total = static_cast<int>(leads.size());

    for (const auto& lead : leads) {
        std::string chatId = lead.count("tg_user_id") ? lead.at("tg_user_id") : "";
        std::string channel = lead.count("channel") ? lead.at("channel") : "";

        if (chatId.empty() || channel.empty()) {
            result.failed++;
            continue;
        }

        auto botRows = Database::query(
            "SELECT token FROM bots "
            "WHERE name='" + esc(channel) + "' "
            "AND active=1 "
            "LIMIT 1"
        );

        if (botRows.empty() || !botRows[0].count("token") || botRows[0].at("token").empty()) {
            result.failed++;
            continue;
        }

        std::string botToken = botRows[0].at("token");

        TgSendResult tg;

        long long tgChatId = 0;
        try {
            tgChatId = std::stoll(chatId);
        }
        catch (...) {
            result.failed++;
            continue;
        }

        if (!req.filePath.empty()) {
            tg = sendTelegramMedia(
                botToken,
                chatId,
                req.text,
                req.attachmentType,
                req.filePath
            );
        }
        else {
            tg = sendTelegramText(
                botToken,
                chatId,
                req.text
            );
        }

        if (tg.ok) {
            result.sent++;

            std::string mediaType = req.filePath.empty() ? "text" : req.attachmentType;
            std::string safeText = Database::escape(req.text);
            std::string safeMediaType = Database::escape(mediaType);
            std::string safeMediaId = Database::escape(tg.mediaId);

            Database::exec(
                "INSERT INTO messages (lead_id, sender, text, media_type, media_id, created_at, is_read) "
                "VALUES (" + lead.at("id") + ", 'admin', '" + safeText + "', '" + safeMediaType + "', '" + safeMediaId + "', datetime('now'), 1)"
            );

            Database::exec(
                "UPDATE leads SET last_message_at=datetime('now') WHERE id=" + lead.at("id")
            );
        }
        else {
            result.failed++;
        }
    }

    return result;
}

BroadcastService::TgSendResult BroadcastService::sendTelegramText(
    const std::string& botToken,
    const std::string& chatId,
    const std::string& text
)
{
    TgSendResult out;

    CURL* curl = curl_easy_init();
    if (!curl) return out;

    std::string url = "https://api.telegram.org/bot" + botToken + "/sendMessage";
    std::string response;

    auto writeCb = [](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
        auto* s = static_cast<std::string*>(userdata);
        s->append(ptr, size * nmemb);
        return size * nmemb;
        };

    curl_mime* form = curl_mime_init(curl);

    curl_mimepart* part = curl_mime_addpart(form);
    curl_mime_name(part, "chat_id");
    curl_mime_data(part, chatId.c_str(), CURL_ZERO_TERMINATED);

    part = curl_mime_addpart(form);
    curl_mime_name(part, "text");
    curl_mime_data(part, text.c_str(), CURL_ZERO_TERMINATED);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_mime_free(form);
    curl_easy_cleanup(curl);

    out.ok = res == CURLE_OK && httpCode >= 200 && httpCode < 300;
    out.mediaId = "";

    return out;
}
BroadcastService::TgSendResult BroadcastService::sendTelegramMedia(
    const std::string& botToken,
    const std::string& chatId,
    const std::string& text,
    const std::string& attachmentType,
    const std::string& filePath
)
{
    TgSendResult out;

    CURL* curl = curl_easy_init();
    if (!curl) return out;

    std::string method = "sendDocument";
    std::string field = "document";

    if (attachmentType == "photo") {
        method = "sendPhoto";
        field = "photo";
    }
    else if (attachmentType == "voice") {
        method = "sendVoice";
        field = "voice";
    }

    std::string url = "https://api.telegram.org/bot" + botToken + "/" + method;
    std::string response;

    auto writeCb = [](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
        auto* s = static_cast<std::string*>(userdata);
        s->append(ptr, size * nmemb);
        return size * nmemb;
        };

    curl_mime* form = curl_mime_init(curl);

    curl_mimepart* part = curl_mime_addpart(form);
    curl_mime_name(part, "chat_id");
    curl_mime_data(part, chatId.c_str(), CURL_ZERO_TERMINATED);

    if (!text.empty()) {
        part = curl_mime_addpart(form);
        curl_mime_name(part, "caption");
        curl_mime_data(part, text.c_str(), CURL_ZERO_TERMINATED);
    }

    part = curl_mime_addpart(form);
    curl_mime_name(part, field.c_str());
    curl_mime_filedata(part, filePath.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_mime_free(form);
    curl_easy_cleanup(curl);

    out.ok = res == CURLE_OK && httpCode >= 200 && httpCode < 300;

    if (!out.ok) {
        std::cout << "[BROADCAST TG ERROR] " << response << std::endl;
        return out;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    if (!reader->parse(response.c_str(), response.c_str() + response.size(), &root, &errs)) {
        return out;
    }

    const Json::Value& msg = root["result"];

    if (attachmentType == "photo") {
        const Json::Value& photos = msg["photo"];
        if (photos.isArray() && photos.size() > 0) {
            out.mediaId = photos[photos.size() - 1]["file_id"].asString();
        }
    }
    else if (attachmentType == "voice") {
        out.mediaId = msg["voice"]["file_id"].asString();
    }
    else {
        out.mediaId = msg["document"]["file_id"].asString();
    }

    return out;
}