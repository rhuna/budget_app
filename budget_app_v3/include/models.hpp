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

inline const char* frequencyToString(Frequency f) {
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
    double amount = 0.0;
    Frequency frequency = Frequency::BiWeekly;
    int payDay1 = 1;
    int payDay2 = 15;
    bool afterTax = true;
};

struct Bill {
    std::string name;
    double amount = 0.0;
    Frequency frequency = Frequency::Monthly;
    int dueDay = 1;
    bool essential = true;
    bool autopay = false;
};

struct VariableExpense {
    std::string name;
    double monthlyPlan = 0.0;
    double actual = 0.0;
    bool essential = false;
};

struct GoalBucket {
    std::string name;
    double monthlyContribution = 0.0;
    double currentBalance = 0.0;
    double targetBalance = 0.0;
};

struct OneTimeExpense {
    std::string name;
    double amount = 0.0;
    int dueDay = 1;
    bool active = true;
};

struct PlannerSettings {
    int month = 1;
    int year = 2026;
    float uiScale = 1.25f;
    int paycheckCount = 2;
};

struct DayCell {
    int day = 1;
    double inflow = 0.0;
    double outflow = 0.0;
    std::vector<std::string> labels;
};

struct PaycheckForecast {
    std::string label;
    int startDay = 1;
    int endDay = 15;
    double inflow = 0.0;
    double fixedBills = 0.0;
    double variableBudget = 0.0;
    double goalBudget = 0.0;
    double oneTimeBudget = 0.0;
    double taxReserve = 0.0;
    double leftover = 0.0;
};

struct BudgetData {
    std::vector<IncomeSource> incomes;
    std::vector<Bill> bills;
    std::vector<VariableExpense> variableExpenses;
    std::vector<GoalBucket> goals;
    std::vector<OneTimeExpense> oneTimeExpenses;

    double checkingBalance = 0.0;
    double emergencyFundBalance = 0.0;
    double debtBalance = 0.0;
    double savingsGoal = 0.0;
    double monthlyTaxReserve = 0.0;

    PlannerSettings settings;
};

struct BudgetSummary {
    double monthlyIncome = 0.0;
    double monthlyBills = 0.0;
    double monthlyVariablePlanned = 0.0;
    double monthlyVariableActual = 0.0;
    double monthlyGoals = 0.0;
    double monthlyOneTime = 0.0;
    double monthlyTaxReserve = 0.0;
    double monthlyPlannedOutflow = 0.0;
    double monthlyActualOutflow = 0.0;
    double monthlyPlannedLeftover = 0.0;
    double monthlyActualLeftover = 0.0;
    double essentialMonthly = 0.0;
    double emergencyMonthsCovered = 0.0;
    double savingsProgress = 0.0;
    int healthScore = 0;

    std::vector<DayCell> calendar;
    std::vector<PaycheckForecast> paychecks;
};

} // namespace budget
