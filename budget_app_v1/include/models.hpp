#pragma once

#include <string>
#include <vector>

namespace budget {

enum class Frequency {
    Weekly,
    BiWeekly,
    Monthly,
    Quarterly,
    Yearly
};

inline std::string frequencyToString(Frequency f) {
    switch (f) {
        case Frequency::Weekly: return "Weekly";
        case Frequency::BiWeekly: return "Bi-Weekly";
        case Frequency::Monthly: return "Monthly";
        case Frequency::Quarterly: return "Quarterly";
        case Frequency::Yearly: return "Yearly";
        default: return "Monthly";
    }
}

struct IncomeSource {
    std::string name;
    double weeklyAmount = 0.0;
};

struct RecurringBill {
    std::string name;
    double amount = 0.0;
    Frequency frequency = Frequency::Monthly;
    int dueDay = 1;
    bool essential = true;
    bool autopay = false;
};

struct WeeklyCategory {
    std::string name;
    double planned = 0.0;
    double spent = 0.0;
    bool essential = false;
};

enum class BudgetMethod {
    ZeroBased,
    FiftyThirtyTwenty,
    Hybrid
};

inline std::string budgetMethodToString(BudgetMethod m) {
    switch (m) {
        case BudgetMethod::ZeroBased: return "Zero-Based";
        case BudgetMethod::FiftyThirtyTwenty: return "50/30/20";
        case BudgetMethod::Hybrid: return "Hybrid";
        default: return "Zero-Based";
    }
}

struct WeeklyPlan {
    std::string weekLabel = "Current Week";
    double extraDebtPayment = 0.0;
    double emergencyFundContribution = 0.0;
    double investingContribution = 0.0;
    std::vector<WeeklyCategory> categories;
};

struct BudgetData {
    std::vector<IncomeSource> incomes;
    std::vector<RecurringBill> recurringBills;
    WeeklyPlan weeklyPlan;

    double emergencyFundBalance = 0.0;
    double highInterestDebtBalance = 0.0;
    double checkingBalance = 0.0;
    double savingsGoal = 0.0;

    BudgetMethod method = BudgetMethod::Hybrid;
};

struct BudgetSummary {
    double weeklyIncome = 0.0;
    double weeklyBills = 0.0;
    double weeklyPlanned = 0.0;
    double weeklySpent = 0.0;
    double weeklyLeftover = 0.0;
    double essentialBillRatio = 0.0;
    double savingsProgress = 0.0;
    double emergencyWeeksCovered = 0.0;
    double debtPressure = 0.0;
    int budgetHealthScore = 0;
};

} // namespace budget
