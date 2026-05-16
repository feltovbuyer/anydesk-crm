#pragma once

#include <string>

class AssignmentService {
public:
    static void reassignIfInactiveManager(int leadId);
    static bool transferLeadToManager(int leadId, const std::string& managerLogin);
};