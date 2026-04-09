#include "execution_engine.hpp"
#include <algorithm>
#include <cmath>

namespace budget {

BudgetExecutionPlan buildExecutionPlan(const BudgetData& data, const BudgetSummary& summary) {
    BudgetExecutionPlan plan{};

    for (const auto& item : data.variableExpenses) {
        const double overspend = item.actual - item.monthlyPlan;
        if (overspend > 0.0) {
            plan.steps.push_back({
                "Bring " + item.name + " back to plan",
                "Recent actual spending is above the monthly plan. Tightening this category is a fast correction.",
                overspend,
                1
            });
        }
    }

    if (summary.monthlyPlannedLeftover < 0.0) {
        plan.steps.push_back({
            "Close the monthly deficit",
            "Current planned cashflow is negative. Reduce optional spending or increase income until the plan is positive.",
            std::abs(summary.monthlyPlannedLeftover),
            0
        });
    }

    if (summary.emergencyMonthsCovered < 1.0) {
        plan.steps.push_back({
            "Build emergency buffer",
            "Emergency coverage is below one month of essentials. Route part of surplus toward cash reserves.",
            100.0,
            2
        });
    }

    if (!summary.debtPlan.recommendedTarget.empty() && summary.monthlyPlannedLeftover > 0.0) {
        plan.steps.push_back({
            "Attack " + summary.debtPlan.recommendedTarget,
            "Direct monthly surplus toward the recommended debt target to cut interest faster.",
            summary.monthlyPlannedLeftover,
            1
        });
    }

    if (!summary.forecast.empty() && summary.forecast.back().projectedChecking < 0.0) {
        plan.steps.push_back({
            "Fix forecasted shortfall",
            "The current forecast ends with negative checking. Reduce commitments or add income before that window.",
            std::max(0.0, -summary.forecast.back().projectedChecking / std::max(1, (int)summary.forecast.size())),
            0
        });
    }

    std::sort(plan.steps.begin(), plan.steps.end(), [](const ExecutionStep& a, const ExecutionStep& b) {
        if (a.priority == b.priority) return a.monthlyImpact > b.monthlyImpact;
        return a.priority < b.priority;
    });

    for (const auto& step : plan.steps) {
        if (step.monthlyImpact > 0.0) plan.totalMonthlyImpact += step.monthlyImpact;
    }

    plan.closesDeficit = summary.monthlyPlannedLeftover >= 0.0 || plan.totalMonthlyImpact >= std::abs(summary.monthlyPlannedLeftover);
    return plan;
}

} // namespace budget
