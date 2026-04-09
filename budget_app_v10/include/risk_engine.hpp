#pragma once
#include "models.hpp"
#include <vector>

namespace budget {

struct RiskSignal {
    std::string severity;
    std::string summary;
};

std::vector<RiskSignal> buildRiskSignals(const BudgetData& data, const BudgetSummary& summary);

} // namespace budget
