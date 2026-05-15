#include "TelegramFileService.h"

#include <curl/curl.h>
#include <json/json.h>

#include <sstream>
#include <iostream>

static size_t tgFileWrite(void* contents, size_t size, size_t nmemb, std::string* out)
{
    size_t total = size * nmemb;
    out->append((char*)contents, total);
    return total;
}

std::string TelegramFileService::httpGet(const std::string& url)
{
    CURL* curl = curl_easy_init();
    std::string response;

    if (!curl) return response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, tgFileWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cerr << "[TG FILE GET] " << curl_easy_strerror(res) << std::endl;
    }

    curl_easy_cleanup(curl);
    return response;
}

bool TelegramFileService::loadFileBytes(
    const std::string& botToken,
    const std::string& fileId,
    std::string& outBytes,
    std::string& outContentType
)
{
    std::string metaUrl = "https://api.telegram.org/bot" + botToken + "/getFile?file_id=" + fileId;
    std::string meta = httpGet(metaUrl);

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream ss(meta);

    if (!Json::parseFromStream(builder, ss, &root, &errs)) {
        return false;
    }

    if (!root.get("ok", false).asBool()) {
        return false;
    }

    std::string filePath = root["result"].get("file_path", "").asString();

    if (filePath.empty()) {
        return false;
    }

    std::string fileUrl = "https://api.telegram.org/file/bot" + botToken + "/" + filePath;
    outBytes = httpGet(fileUrl);

    if (filePath.find(".jpg") != std::string::npos || filePath.find(".jpeg") != std::string::npos) {
        outContentType = "image/jpeg";
    }
    else if (filePath.find(".png") != std::string::npos) {
        outContentType = "image/png";
    }
    else if (filePath.find(".webp") != std::string::npos) {
        outContentType = "image/webp";
    }
    else if (filePath.find(".ogg") != std::string::npos) {
        outContentType = "audio/ogg";
    }
    else if (filePath.find(".mp4") != std::string::npos) {
        outContentType = "video/mp4";
    }
    else {
        outContentType = "application/octet-stream";
    }

    return !outBytes.empty();
}