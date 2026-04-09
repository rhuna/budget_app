#pragma once
#include "models.hpp"
#include <vector>

namespace budget {

struct AutoBudgetAllocation {
    std::string bucket;
    double amount = 0.0;
};

struct GoalProjection {
    std::string goalName;
    double monthlyContribution = 0.0;
    double currentBalance = 0.0;
    double targetBalance = 0.0;
    double monthsToGoal = 0.0;
    double contributionForTwoMonthsSooner = 0.0;
};

std::vector<AutoBudgetAllocation> buildAutoBudget(const BudgetData& data, const BudgetSummary& summary);
std::vector<GoalProjection> buildGoalProjections(const BudgetData& data);

} // namespace budget
