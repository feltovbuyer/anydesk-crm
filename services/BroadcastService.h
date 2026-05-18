#pragma once
#include <vector>
#include <string>

struct BroadcastPreviewRequest {
    std::vector<std::string> includeTags;
    std::vector<std::string> excludeTags;
    std::string dateFrom;
    std::string dateTo;
};

struct BroadcastStartRequest {
    std::vector<std::string> includeTags;
    std::vector<std::string> excludeTags;
    std::string dateFrom;
    std::string dateTo;
    std::string text;
    std::string attachmentType;
    std::string filePath;
};

struct BroadcastStartResult {
    int total = 0;
    int sent = 0;
    int failed = 0;
};

class BroadcastService {
public:
    static int previewCount(const BroadcastPreviewRequest& req);
    static BroadcastStartResult start(const BroadcastStartRequest& req);

private:
    static std::string esc(const std::string& s);

    static std::string buildWhere(
        const std::vector<std::string>& includeTags,
        const std::vector<std::string>& excludeTags,
        const std::string& dateFrom,
        const std::string& dateTo
    );

    struct TgSendResult {
        bool ok = false;
        std::string mediaId;
    };

    static TgSendResult sendTelegramText(
        const std::string& botToken,
        const std::string& chatId,
        const std::string& text
    );

    static TgSendResult sendTelegramMedia(
        const std::string& botToken,
        const std::string& chatId,
        const std::string& text,
        const std::string& attachmentType,
        const std::string& filePath
    );
};