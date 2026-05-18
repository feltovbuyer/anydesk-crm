#pragma once

#include <string>

class LeadTagService {
public:
    static void setFunnelTag(int leadId, const std::string& tag);
    static void finishFd(int leadId);

private:
    static std::string normalizeTags(const std::string& tags);
    static std::string removeFunnelTags(const std::string& tags);
};