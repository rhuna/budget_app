#include "financial_engine.hpp"
#include <algorithm>
#include <cmath>

namespace budget {

std::vector<AutoBudgetAllocation> buildAutoBudget(const BudgetData& data, const BudgetSummary& summary) {
    std::vector<AutoBudgetAllocation> out;

    const double income = std::max(0.0, summary.monthlyIncome);
    const double bills = std::max(0.0, summary.monthlyBills);
    const double goals = std::max(0.0, summary.monthlyGoals);
    const double variable = std::max(0.0, summary.monthlyVariablePlanned);
    const double oneTime = std::max(0.0, summary.monthlyOneTime);
    const double tax = std::max(0.0, data.monthlyTaxReserve);

    out.push_back({"Bills", bills});
    out.push_back({"Variable Spending", variable});
    out.push_back({"Goals", goals});
    out.push_back({"One-Time / Sinking", oneTime});
    out.push_back({"Tax Reserve", tax});

    const double committed = bills + variable + goals + oneTime + tax;
    const double remaining = income - committed;

    if (remaining > 0.0) {
        const double toEmergency = remaining * 0.35;
        const double toDebt = data.debtBalance > 0.0 ? remaining * 0.45 : 0.0;
        const double toInvest = std::max(0.0, remaining - toEmergency - toDebt);
        out.push_back({"Emergency Buffer", toEmergency});
        if (toDebt > 0.0) out.push_back({"Extra Debt Paydown", toDebt});
        out.push_back({"Future / Investing", toInvest});
    } else {
        out.push_back({"Deficit", remaining});
    }

    return out;
}

std::vector<GoalProjection> buildGoalProjections(const BudgetData& data) {
    std::vector<GoalProjection> out;
    for (const auto& goal : data.goals) {
        GoalProjection g;
        g.goalName = goal.name;
        g.monthlyContribution = goal.monthlyContribution;
        g.currentBalance = goal.currentBalance;
        g.targetBalance = goal.targetBalance;

        const double remaining = std::max(0.0, goal.targetBalance - goal.currentBalance);
        if (goal.monthlyContribution > 0.0) {
            g.monthsToGoal = remaining / goal.monthlyContribution;
            const double fasterMonths = std::max(1.0, g.monthsToGoal - 2.0);
            g.contributionForTwoMonthsSooner = remaining / fasterMonths;
        } else {
            g.monthsToGoal = 0.0;
            g.contributionForTwoMonthsSooner = 0.0;
        }

        out.push_back(g);
    }
    return out;
}

} // namespace budget
