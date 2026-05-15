#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include <json/json.h>

struct TgBotConfig {
    int id = 0;
    std::string token;
    std::string channel;
    std::string geo;
};
struct TgSendFileResult {
    bool ok = false;
    std::string fileId;
    std::string rawResponse;
};

class TelegramService {
public:
    static void startAll();
    static void stopAll();

    static bool sendText(
        const std::string& botToken,
        long long chatId,
        const std::string& text
    );

    static bool sendFile(
        const std::string& botToken,
        long long chatId,
        const std::string& type,
        const std::string& filePath,
        const std::string& caption = ""
    );

    static TgSendFileResult sendFileBytes(
        const std::string& botToken,
        long long chatId,
        const std::string& type,
        const std::string& fileName,
        const std::string& bytes,
        const std::string& caption = ""
    );

private:
    static void pollingLoop(TgBotConfig bot);

    static std::string httpGet(const std::string& url);
    static std::string httpPostJson(
        const std::string& url,
        const std::string& jsonBody
    );

    static void handleUpdate(
        const TgBotConfig& bot,
        const Json::Value& update
    );

    static void saveIncomingMessage(
        const TgBotConfig& bot,
        long long tgUserId,
        const std::string& username,
        const std::string& fullName,
        const std::string& text,
        const std::string& subid
    );

private:
    static inline std::atomic_bool running{ false };
    static inline std::vector<std::thread> workers;
};