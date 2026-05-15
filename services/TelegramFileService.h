#pragma once

#include <string>

class TelegramFileService {
public:
    static bool loadFileBytes(
        const std::string& botToken,
        const std::string& fileId,
        std::string& outBytes,
        std::string& outContentType
    );

private:
    static std::string httpGet(const std::string& url);
};