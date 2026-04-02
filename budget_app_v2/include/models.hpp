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
    Frequency frequency = Frequency::Weekly;
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
    double weeklyPlan = 0.0;
    double actual = 0.0;
    bool essential = false;
};

struct GoalBucket {
    std::string name;
    double weeklyContribution = 0.0;
    double currentBalance = 0.0;
    double targetBalance = 0.0;
};

struct OneTimeExpense {
    std::string name;
    double amount = 0.0;
    bool thisWeek = true;
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
    double taxReserveWeekly = 0.0;
};

struct BudgetSummary {
    double weeklyIncome = 0.0;
    double weeklyBills = 0.0;
    double weeklyVariablePlanned = 0.0;
    double weeklyVariableActual = 0.0;
    double weeklyGoals = 0.0;
    double weeklyOneTime = 0.0;
    double weeklyTaxReserve = 0.0;
    double weeklyPlannedOutflow = 0.0;
    double weeklyActualOutflow = 0.0;
    double weeklyPlannedLeftover = 0.0;
    double weeklyActualLeftover = 0.0;
    double essentialWeekly = 0.0;
    double emergencyWeeksCovered = 0.0;
    double savingsProgress = 0.0;
    int healthScore = 0;
};

} // namespace budget
