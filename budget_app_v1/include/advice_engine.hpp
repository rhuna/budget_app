#pragma once

#include "models.hpp"
#include <string>
#include <vector>

namespace budget
{



    BudgetSummary buildSummary(const BudgetData &data);

    // 🔥 FIX: NO MORE AdviceItem
    std::vector<std::string> buildAdvice(const BudgetData &data, const BudgetSummary &summary);

}