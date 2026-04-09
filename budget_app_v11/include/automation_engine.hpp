#pragma once
#include "models.hpp"
#include <vector>

namespace budget {

struct AutomationRule {
    std::string name;
    std::string description;
    bool enabled = true;
};

std::vector<AutomationRule> buildDefaultAutomationRules(const BudgetData& data, const BudgetSummary& summary);

} // namespace budget
