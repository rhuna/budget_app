#pragma once

#include "models.hpp"
#include <vector>

namespace budget {

std::vector<AdviceItem> buildAdvice(const BudgetData& data, const BudgetSummary& summary);

} // namespace budget
