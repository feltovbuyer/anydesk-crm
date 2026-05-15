#include "TelegramService.h"

#include "../db/Database.h"

#include <curl/curl.h>
#include <json/json.h>

#include <chrono>
#include <iostream>
#include <sstream>

static size_t curlWrite(void* contents, size_t size, size_t nmemb, std::string* out)
{
    size_t total = size * nmemb;
    out->append((char*)contents, total);
    return total;
}

static std::string urlEncode(const std::string& value)
{
    CURL* curl = curl_easy_init();
    if (!curl) return value;

    char* encoded = curl_easy_escape(curl, value.c_str(), (int)value.size());
    std::string result = encoded ? encoded : value;

    if (encoded) curl_free(encoded);
    curl_easy_cleanup(curl);

    return result;
}

static std::string normalizeStartPayload(std::string s)
{
    for (char& c : s) {
        if (c == '.') c = '_';
    }
    return s;
}

static std::string extractStartArg(const std::string& text)
{
    if (text.rfind("/start ", 0) == 0) {
        return normalizeStartPayload(text.substr(7));
    }

    if (text.rfind("/start", 0) == 0 && text.size() > 6) {
        return normalizeStartPayload(text.substr(6));
    }

    return "";
}

void TelegramService::startAll()
{
    if (running.exchange(true)) {
        return;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    auto rows = Database::query(
        "SELECT id, token, name AS channel, geo "
        "FROM bots "
        "WHERE active = 1"
    );

    std::cout << "[TG] active bots loaded: " << rows.size() << std::endl;

    for (const auto& r : rows) {
        TgBotConfig bot;
        bot.id = std::stoi(r.at("id"));
        bot.token = r.at("token");
        bot.channel = r.at("channel");
        bot.geo = r.at("geo");

        workers.emplace_back([bot]() {
            pollingLoop(bot);
            });

        std::cout << "[TG] polling started: " << bot.channel << std::endl;
    }
}

void TelegramService::stopAll()
{
    running = false;

    for (auto& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }

    workers.clear();
    curl_global_cleanup();
}

void TelegramService::pollingLoop(TgBotConfig bot)
{
    long long offset = 0;

    while (running) {
        try {
            std::ostringstream url;
            url << "https://api.telegram.org/bot" << bot.token
                << "/getUpdates?timeout=25&limit=50";

            if (offset > 0) {
                url << "&offset=" << offset;
            }

            std::string body = httpGet(url.str());

            Json::Value root;
            Json::CharReaderBuilder builder;
            std::string errs;
            std::istringstream ss(body);

            if (!Json::parseFromStream(builder, ss, &root, &errs)) {
                std::cerr << "[TG] JSON parse error: " << errs << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            if (!root["ok"].asBool()) {
                std::cerr << "[TG] getUpdates not ok: " << body << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(3));
                continue;
            }

            for (const auto& upd : root["result"]) {
                long long updateId = upd["update_id"].asInt64();
                offset = updateId + 1;

                handleUpdate(bot, upd);
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[TG] polling exception: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        catch (...) {
            std::cerr << "[TG] unknown polling exception" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }
}

std::string TelegramService::httpGet(const std::string& url)
{
    CURL* curl = curl_easy_init();
    std::string response;

    if (!curl) return response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 35L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cerr << "[CURL GET] " << curl_easy_strerror(res) << std::endl;
    }

    curl_easy_cleanup(curl);
    return response;
}

std::string TelegramService::httpPostJson(const std::string& url, const std::string& jsonBody)
{
    CURL* curl = curl_easy_init();
    std::string response;

    if (!curl) return response;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cerr << "[CURL POST] " << curl_easy_strerror(res) << std::endl;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
}

void TelegramService::handleUpdate(const TgBotConfig& bot, const Json::Value& update)
{
    if (!update.isMember("message")) {
        return;
    }

    const Json::Value& msg = update["message"];

    if (!msg.isMember("from") || !msg.isMember("chat")) {
        return;
    }

    long long tgUserId = msg["from"]["id"].asInt64();

    std::string username = msg["from"].get("username", "").asString();

    std::string firstName = msg["from"].get("first_name", "").asString();
    std::string lastName = msg["from"].get("last_name", "").asString();

    std::string fullName = firstName;
    if (!lastName.empty()) {
        if (!fullName.empty()) fullName += " ";
        fullName += lastName;
    }

    std::string text = msg.get("text", "").asString();

    std::string mediaType = "text";
    std::string mediaId;

    if (msg.isMember("photo") && msg["photo"].isArray() && !msg["photo"].empty()) {
        mediaType = "photo";
        mediaId = msg["photo"][msg["photo"].size() - 1]["file_id"].asString();
        text = msg.get("caption", "").asString();
    }
    else if (msg.isMember("voice")) {
        mediaType = "voice";
        mediaId = msg["voice"]["file_id"].asString();
        text = msg.get("caption", "").asString();
    }
    else if (msg.isMember("document")) {
        mediaType = "document";
        mediaId = msg["document"]["file_id"].asString();
        text = msg.get("caption", "").asString();
    }
    else if (msg.isMember("video")) {
        mediaType = "video";
        mediaId = msg["video"]["file_id"].asString();
        text = msg.get("caption", "").asString();
    }
    else if (msg.isMember("video_note")) {
        mediaType = "video_note";
        mediaId = msg["video_note"]["file_id"].asString();
        text = "";
    }

    if (mediaType == "text" && text.empty()) {
        return;
    }

    std::string subid;

    if (mediaType == "text" && text.rfind("/start", 0) == 0) {
        subid = extractStartArg(text);

        if (!subid.empty()) {
            text = "/start " + subid;
        }

        saveIncomingMessage(bot, tgUserId, username, fullName, text, subid);
        return;
    }

    if (mediaType == "text") {
        saveIncomingMessage(bot, tgUserId, username, fullName, text, "");
        return;
    }

    saveIncomingMedia(bot, tgUserId, username, fullName, text, mediaType, mediaId);
}

void TelegramService::saveIncomingMessage(
    const TgBotConfig& bot,
    long long tgUserId,
    const std::string& username,
    const std::string& fullName,
    const std::string& text,
    const std::string& subid
)
{
    std::string safeUsername = Database::escape(username);
    std::string safeFullName = Database::escape(fullName);
    std::string safeText = Database::escape(text);
    std::string safeSubid = Database::escape(subid);
    std::string safeChannel = Database::escape(bot.channel);
    std::string safeGeo = Database::escape(bot.geo);

    Database::exec(
        "INSERT INTO leads (tg_user_id, username, full_name, channel, geo, subid, created_at, last_message_at) "
        "VALUES ('" + std::to_string(tgUserId) + "', '" + safeUsername + "', '" + safeFullName + "', '" +
        safeChannel + "', '" + safeGeo + "', '" + safeSubid + "', datetime('now'), datetime('now')) "
        "ON CONFLICT(tg_user_id) DO UPDATE SET "
        "username=excluded.username, "
        "full_name=excluded.full_name, "
        "channel=excluded.channel, "
        "geo=excluded.geo, "
        "subid=CASE WHEN excluded.subid != '' THEN excluded.subid ELSE leads.subid END, "
        "last_message_at=datetime('now')"
    );

    auto rows = Database::query(
        "SELECT id FROM leads "
        "WHERE tg_user_id = '" + std::to_string(tgUserId) + "' "
        "LIMIT 1"
    );

    if (rows.empty()) {
        return;
    }

    std::string leadId = rows[0].at("id");

    Database::exec(
        "INSERT INTO messages (lead_id, sender, text, media_type, media_id, created_at, is_read) "
        "VALUES (" + leadId + ", 'user', '" + safeText + "', 'text', '', datetime('now'), 0)"
    );
}
void TelegramService::saveIncomingMedia(
    const TgBotConfig& bot,
    long long tgUserId,
    const std::string& username,
    const std::string& fullName,
    const std::string& text,
    const std::string& mediaType,
    const std::string& mediaId
)
{
    std::string safeUsername = Database::escape(username);
    std::string safeFullName = Database::escape(fullName);
    std::string safeText = Database::escape(text);
    std::string safeChannel = Database::escape(bot.channel);
    std::string safeGeo = Database::escape(bot.geo);
    std::string safeType = Database::escape(mediaType);
    std::string safeMediaId = Database::escape(mediaId);

    Database::exec(
        "INSERT INTO leads (tg_user_id, username, full_name, channel, geo, created_at, last_message_at) "
        "VALUES ('" + std::to_string(tgUserId) + "', '" + safeUsername + "', '" + safeFullName + "', '" +
        safeChannel + "', '" + safeGeo + "', datetime('now'), datetime('now')) "
        "ON CONFLICT(tg_user_id) DO UPDATE SET "
        "username=excluded.username, "
        "full_name=excluded.full_name, "
        "channel=excluded.channel, "
        "geo=excluded.geo, "
        "last_message_at=datetime('now')"
    );

    auto rows = Database::query(
        "SELECT id FROM leads "
        "WHERE tg_user_id = '" + std::to_string(tgUserId) + "' "
        "LIMIT 1"
    );

    if (rows.empty()) {
        return;
    }

    std::string leadId = rows[0].at("id");

    Database::exec(
        "INSERT INTO messages (lead_id, sender, text, media_type, media_id, created_at, is_read) "
        "VALUES (" + leadId + ", 'user', '" + safeText + "', '" +
        safeType + "', '" + safeMediaId + "', datetime('now'), 0)"
    );
}

bool TelegramService::sendText(
    const std::string& botToken,
    long long chatId,
    const std::string& text
)
{
    Json::Value body;
    body["chat_id"] = Json::Int64(chatId);
    body["text"] = text;

    Json::StreamWriterBuilder writer;
    std::string jsonBody = Json::writeString(writer, body);

    std::string url = "https://api.telegram.org/bot" + botToken + "/sendMessage";

    std::string response = httpPostJson(url, jsonBody);

    return response.find("\"ok\":true") != std::string::npos;
}
bool TelegramService::sendFile(
    const std::string& botToken,
    long long chatId,
    const std::string& type,
    const std::string& filePath,
    const std::string& caption
)
{
    std::string method;
    std::string field;

    if (type == "photo") {
        method = "sendPhoto";
        field = "photo";
    }
    else if (type == "document") {
        method = "sendDocument";
        field = "document";
    }
    else if (type == "voice") {
        method = "sendVoice";
        field = "voice";
    }
    else if (type == "video") {
        method = "sendVideo";
        field = "video";
    }
    else if (type == "video_note") {
        method = "sendVideoNote";
        field = "video_note";
    }
    else {
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string url = "https://api.telegram.org/bot" + botToken + "/" + method;
    std::string chatIdStr = std::to_string(chatId);

    curl_mime* mime = curl_mime_init(curl);

    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "chat_id");
    curl_mime_data(part, chatIdStr.c_str(), CURL_ZERO_TERMINATED);

    if (!caption.empty() && type != "video_note") {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "caption");
        curl_mime_data(part, caption.c_str(), CURL_ZERO_TERMINATED);
    }

    part = curl_mime_addpart(mime);
    curl_mime_name(part, field.c_str());
    curl_mime_filedata(part, filePath.c_str());

    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    CURLcode res = curl_easy_perform(curl);

    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    std::cout << "[TG sendFile] type=" << type << std::endl;
    std::cout << "[TG sendFile] path=" << filePath << std::endl;
    std::cout << "[TG sendFile] curl=" << curl_easy_strerror(res) << std::endl;
    std::cout << "[TG sendFile] response=" << response << std::endl;

    return res == CURLE_OK && response.find("\"ok\":true") != std::string::npos;
}
TgSendFileResult TelegramService::sendFileBytes(
    const std::string& botToken,
    long long chatId,
    const std::string& type,
    const std::string& fileName,
    const std::string& bytes,
    const std::string& caption
)
{
    TgSendFileResult result;

    std::string method;
    std::string field;

    if (type == "photo") {
        method = "sendPhoto";
        field = "photo";
    }
    else if (type == "document") {
        method = "sendDocument";
        field = "document";
    }
    else if (type == "voice") {
        method = "sendVoice";
        field = "voice";
    }
    else if (type == "video") {
        method = "sendVideo";
        field = "video";
    }
    else if (type == "video_note") {
        method = "sendVideoNote";
        field = "video_note";
    }
    else {
        return result;
    }

    CURL* curl = curl_easy_init();
    if (!curl) return result;

    std::string url = "https://api.telegram.org/bot" + botToken + "/" + method;

    curl_mime* mime = curl_mime_init(curl);

    auto part = curl_mime_addpart(mime);
    curl_mime_name(part, "chat_id");
    std::string chatIdStr = std::to_string(chatId);
    curl_mime_data(part, chatIdStr.c_str(), CURL_ZERO_TERMINATED);

    if (!caption.empty() && type != "video_note") {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "caption");
        curl_mime_data(part, caption.c_str(), CURL_ZERO_TERMINATED);
    }

    part = curl_mime_addpart(mime);
    curl_mime_name(part, field.c_str());
    curl_mime_filename(part, fileName.c_str());
    curl_mime_data(part, bytes.data(), bytes.size());

    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    CURLcode res = curl_easy_perform(curl);

    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    result.rawResponse = response;

    std::cout << "[TG sendFileBytes] " << response << std::endl;

    if (res != CURLE_OK) {
        return result;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream ss(response);

    if (!Json::parseFromStream(builder, ss, &root, &errs)) {
        return result;
    }

    if (!root.get("ok", false).asBool()) {
        return result;
    }

    result.ok = true;

    const Json::Value& msg = root["result"];

    if (type == "photo" && msg.isMember("photo") && msg["photo"].isArray() && !msg["photo"].empty()) {
        result.fileId = msg["photo"][msg["photo"].size() - 1]["file_id"].asString();
    }
    else if (type == "document" && msg.isMember("document")) {
        result.fileId = msg["document"]["file_id"].asString();
    }
    else if (type == "voice" && msg.isMember("voice")) {
        result.fileId = msg["voice"]["file_id"].asString();
    }
    else if (type == "video" && msg.isMember("video")) {
        result.fileId = msg["video"]["file_id"].asString();
    }
    else if (type == "video_note" && msg.isMember("video_note")) {
        result.fileId = msg["video_note"]["file_id"].asString();
    }

    return result;
}