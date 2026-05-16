#pragma once

#include <string>
#include <vector>

struct LeadGroupInfo {
    int rootLeadId = 0;
    std::vector<int> leadIds;
    std::string mainChannel;
};

class DuplicateLeadService {
public:
    static int rootLeadId(int leadId);
    static LeadGroupInfo groupInfo(int leadId);

    static bool isDuplicate(int leadId);
    static std::string mainChannel(int leadId);

    static bool transferWholeGroupToManager(int leadId, const std::string& managerLogin);
    static std::vector<int> groupLeadIds(int leadId);

    static int findLeadForChannel(long long tgUserId, const std::string& channel);
    static int findRootLeadForUser(long long tgUserId);
    static bool markAsDuplicate(int leadId, int rootLeadId);
};