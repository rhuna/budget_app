#include "advice_engine.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace budget {

namespace {

double normalizedWeeklyAmount(const RecurringBill& bill) {
    switch (bill.frequency) {
        case Frequency::Weekly: return bill.amount;
        case Frequency::BiWeekly: return bill.amount / 2.0;
        case Frequency::Monthly: return bill.amount * 12.0 / 52.0;
        case Frequency::Quarterly: return bill.amount * 4.0 / 52.0;
        case Frequency::Yearly: return bill.amount / 52.0;
        default: return bill.amount;
    }
}

double essentialWeeklyBills(const BudgetData& data) {
    double total = 0.0;
    for (const auto& bill : data.recurringBills) {
        if (bill.essential) total += normalizedWeeklyAmount(bill);
    }
    return total;
}

double totalWeeklyBills(const BudgetData& data) {
    double total = 0.0;
    for (const auto& bill : data.recurringBills) {
        total += normalizedWeeklyAmount(bill);
    }
    return total;
}

double totalWeeklyIncome(const BudgetData& data) {
    double total = 0.0;
    for (const auto& income : data.incomes) {
        total += income.weeklyAmount;
    }
    return total;
}

double totalPlannedCategories(const BudgetData& data) {
    double total = 0.0;
    for (const auto& cat : data.weeklyPlan.categories) {
        total += cat.planned;
    }
    return total;
}

double totalSpentCategories(const BudgetData& data) {
    double total = 0.0;
    for (const auto& cat : data.weeklyPlan.categories) {
        total += cat.spent;
    }
    return total;
}

} // namespace

BudgetSummary buildSummary(const BudgetData& data) {
    BudgetSummary s;
    s.weeklyIncome = totalWeeklyIncome(data);
    s.weeklyBills = totalWeeklyBills(data);
    s.weeklyPlanned = totalPlannedCategories(data)
        + data.weeklyPlan.extraDebtPayment
        + data.weeklyPlan.emergencyFundContribution
        + data.weeklyPlan.investingContribution;
    s.weeklySpent = totalSpentCategories(data);
    s.weeklyLeftover = s.weeklyIncome - s.weeklyBills - s.weeklyPlanned;

    const double essentials = essentialWeeklyBills(data);
    if (s.weeklyIncome > 0.0) {
        s.essentialBillRatio = essentials / s.weeklyIncome;
    }

    if (data.savingsGoal > 0.0) {
        s.savingsProgress = std::clamp(data.emergencyFundBalance / data.savingsGoal, 0.0, 1.0);
    }

    if (essentials > 0.0) {
        s.emergencyWeeksCovered = data.emergencyFundBalance / essentials;
    }

    if (s.weeklyIncome > 0.0) {
        s.debtPressure = std::clamp(data.highInterestDebtBalance / (s.weeklyIncome * 12.0), 0.0, 3.0);
    }

    int score = 50;
    if (s.weeklyLeftover >= 0.0) score += 15; else score -= 20;
    if (s.essentialBillRatio <= 0.6) score += 10; else if (s.essentialBillRatio > 0.8) score -= 10;
    if (s.emergencyWeeksCovered >= 4.0) score += 10; else if (s.emergencyWeeksCovered < 2.0) score -= 10;
    if (data.highInterestDebtBalance <= 0.0) score += 10; else if (s.debtPressure > 1.0) score -= 10;
    if (data.weeklyPlan.investingContribution > 0.0) score += 5;
    s.budgetHealthScore = std::clamp(score, 0, 100);

    return s;
}

std::vector<std::string> buildAdvice(const BudgetData& data, const BudgetSummary& summary) {
    std::vector<std::string> advice;

    std::ostringstream method;
    method << "Current budgeting method: " << budgetMethodToString(data.method)
           << ". Use Zero-Based when every dollar needs a job, 50/30/20 for simpler guardrails, and Hybrid for flexible planning with savings targets.";
    advice.push_back(method.str());

    if (summary.weeklyLeftover < 0.0) {
        advice.push_back("Your weekly plan is overspent. Cut optional categories first, then review recurring subscriptions or large fixed bills.");
    } else {
        advice.push_back("Your weekly plan leaves cash after bills and goals. Keep assigning that leftover intentionally instead of letting it disappear.");
    }

    if (summary.emergencyWeeksCovered < 4.0) {
        advice.push_back("Emergency fund coverage is still thin. Push toward 4 to 6 weeks of essential expenses before raising lifestyle spending.");
    } else {
        advice.push_back("Emergency fund coverage is getting healthier. Maintain the habit while paying down high-interest debt.");
    }

    if (data.highInterestDebtBalance > 0.0) {
        advice.push_back("High-interest debt is still active. After minimums, direct extra cash to the highest-rate balance first for the strongest math.");
    } else {
        advice.push_back("No high-interest debt on file. Shift more weekly cash toward savings and investing.");
    }

    if (summary.essentialBillRatio > 0.7) {
        advice.push_back("Essentials are consuming a large share of weekly income. Review housing, transport, insurance, and phone/internet plans for relief.");
    }

    if (data.weeklyPlan.investingContribution <= 0.0 && summary.weeklyLeftover > 0.0) {
        advice.push_back("You have room to start a small investing habit. Even a modest weekly contribution can build consistency once your buffer is stable.");
    }

    advice.push_back("Useful references: CFPB budgeting basics, FDIC money smart materials, and Investor.gov guidance on saving and long-term investing.");

    return advice;
}

} // namespace budget
