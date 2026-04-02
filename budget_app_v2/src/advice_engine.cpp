#include "advice_engine.hpp"

#include <algorithm>

namespace budget {

namespace {

double weeklyFrom(double amount, Frequency frequency) {
    switch (frequency) {
        case Frequency::Weekly: return amount;
        case Frequency::BiWeekly: return amount / 2.0;
        case Frequency::Monthly: return amount * 12.0 / 52.0;
        case Frequency::Quarterly: return amount * 4.0 / 52.0;
        case Frequency::Yearly: return amount / 52.0;
        default: return amount;
    }
}

} // namespace

BudgetSummary buildSummary(const BudgetData& data) {
    BudgetSummary s{};

    for (const auto& income : data.incomes) {
        s.weeklyIncome += weeklyFrom(income.amount, income.frequency);
    }

    for (const auto& bill : data.bills) {
        const double weekly = weeklyFrom(bill.amount, bill.frequency);
        s.weeklyBills += weekly;
        if (bill.essential) s.essentialWeekly += weekly;
    }

    for (const auto& item : data.variableExpenses) {
        s.weeklyVariablePlanned += item.weeklyPlan;
        s.weeklyVariableActual += item.actual;
        if (item.essential) s.essentialWeekly += item.weeklyPlan;
    }

    for (const auto& goal : data.goals) {
        s.weeklyGoals += goal.weeklyContribution;
    }

    for (const auto& oneTime : data.oneTimeExpenses) {
        if (oneTime.thisWeek) s.weeklyOneTime += oneTime.amount;
    }

    s.weeklyTaxReserve = data.taxReserveWeekly;
    s.weeklyPlannedOutflow = s.weeklyBills + s.weeklyVariablePlanned + s.weeklyGoals + s.weeklyOneTime + s.weeklyTaxReserve;
    s.weeklyActualOutflow = s.weeklyBills + s.weeklyVariableActual + s.weeklyGoals + s.weeklyOneTime + s.weeklyTaxReserve;
    s.weeklyPlannedLeftover = s.weeklyIncome - s.weeklyPlannedOutflow;
    s.weeklyActualLeftover = s.weeklyIncome - s.weeklyActualOutflow;

    if (s.essentialWeekly > 0.0) {
        s.emergencyWeeksCovered = data.emergencyFundBalance / s.essentialWeekly;
    }
    if (data.savingsGoal > 0.0) {
        s.savingsProgress = std::clamp(data.emergencyFundBalance / data.savingsGoal, 0.0, 1.0);
    }

    int score = 50;
    if (s.weeklyPlannedLeftover >= 0.0) score += 15; else score -= 20;
    if (s.weeklyActualLeftover >= 0.0) score += 10; else score -= 10;
    if (s.emergencyWeeksCovered >= 4.0) score += 10; else if (s.emergencyWeeksCovered < 2.0) score -= 10;
    if (data.debtBalance <= 0.0) score += 5; else if (data.debtBalance > s.weeklyIncome * 10.0) score -= 10;
    if (data.taxReserveWeekly > 0.0) score += 5;
    s.healthScore = std::clamp(score, 0, 100);

    return s;
}

std::vector<std::string> buildAdvice(const BudgetData& data, const BudgetSummary& summary) {
    std::vector<std::string> advice;

    if (summary.weeklyPlannedLeftover < 0.0) {
        advice.emplace_back("Your weekly plan is negative. Reduce variable categories first, then review subscriptions and nonessential bills.");
    } else {
        advice.emplace_back("Your plan is positive. Give leftover money a job: debt, emergency fund, sinking funds, or investing.");
    }

    if (summary.emergencyWeeksCovered < 4.0) {
        advice.emplace_back("Emergency coverage is below a 4-week cushion. Prioritize building a cash buffer before increasing wants.");
    } else {
        advice.emplace_back("Emergency coverage is improving. Keep protecting that buffer while pushing extra cash toward debt or savings.");
    }

    if (data.debtBalance > 0.0) {
        advice.emplace_back("Debt is active. Pay minimums on everything, then target the highest-rate balance with extra weekly cash.");
    }

    if (data.taxReserveWeekly <= 0.0) {
        advice.emplace_back("If any income is side income or self-employment income, add a weekly tax reserve.");
    }

    if (summary.weeklyOneTime > 0.0) {
        advice.emplace_back("You have one-time expenses this week. Turn predictable irregular costs into sinking funds so they stop surprising the budget.");
    }

    if (summary.weeklyVariableActual > summary.weeklyVariablePlanned) {
        advice.emplace_back("Actual variable spending is above plan. Track groceries, fuel, eating out, and household items separately to find the leak faster.");
    }

    advice.emplace_back("This build recalculates in real time using income, bills, variable spending, goal contributions, tax reserve, and one-time expenses.");

    return advice;
}

} // namespace budget
