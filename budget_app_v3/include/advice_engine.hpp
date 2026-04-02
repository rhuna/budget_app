#pragma once

#include "models.hpp"
#include <string>
#include <vector>

namespace budget {

BudgetSummary buildSummary(const BudgetData& data);
std::vector<std::string> buildAdvice(const BudgetData& data, const BudgetSummary& summary);

} // namespace budget
