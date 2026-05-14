#pragma once

#include <string>
#include <vector>

struct BotItem
{
    int id;
    std::string name;
    std::string token;
    std::string geo;
    std::string funnel;
    int active;
    std::string createdAt;
};

class BotService
{
public:
    static bool create(
        const std::string& name,
        const std::string& token,
        const std::string& geo,
        const std::string& funnel
    );

    static std::vector<BotItem> all();

    static std::string tokenByChannel(const std::string& channel);
};