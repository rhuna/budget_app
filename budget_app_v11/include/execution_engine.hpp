#pragma once
#include "models.hpp"
#include <string>
#include <vector>

namespace budget {

struct ExecutionStep {
    std::string title;
    std::string description;
    double monthlyImpact = 0.0;
    int priority = 0;
};

struct BudgetExecutionPlan {
    std::vector<ExecutionStep> steps;
    double totalMonthlyImpact = 0.0;
    bool closesDeficit = false;
};

BudgetExecutionPlan buildExecutionPlan(const BudgetData& data, const BudgetSummary& summary);

} // namespace budget
