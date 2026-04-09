#pragma once
#include <string>
#include <vector>

namespace budget {

enum class Frequency { Weekly, BiWeekly, Monthly, Quarterly, Yearly };
enum class TransactionType { Expense, Income };
enum class DebtStrategy { Avalanche, Snowball };

struct IncomeSource { std::string name; double amount = 0.0; Frequency frequency = Frequency::BiWeekly; int payDay1 = 1; int payDay2 = 15; bool afterTax = true; };
struct Bill { std::string name; double amount = 0.0; Frequency frequency = Frequency::Monthly; int dueDay = 1; bool essential = true; bool autopay = false; };
struct VariableExpense { std::string name; double monthlyPlan = 0.0; double actual = 0.0; bool essential = false; };
struct GoalBucket { std::string name; double monthlyContribution = 0.0; double currentBalance = 0.0; double targetBalance = 0.0; };
struct OneTimeExpense { std::string name; double amount = 0.0; int dueDay = 1; bool active = true; };
struct Transaction { std::string dateLabel; std::string category; std::string note; double amount = 0.0; TransactionType type = TransactionType::Expense; };
struct PaycheckPlan { std::string label; int startDay = 1; int endDay = 15; double customVariableBudget = 0.0; double customGoalBudget = 0.0; double customTaxReserve = 0.0; double customExtraDebt = 0.0; bool useCustomAllocation = false; };
struct PaycheckTemplate { std::string name; double variableBudget = 0.0; double goalBudget = 0.0; double taxReserve = 0.0; double extraDebt = 0.0; };
struct DebtAccount { std::string name; double balance = 0.0; double apr = 0.0; double minimumPayment = 0.0; };

struct DebtPlanResult {
    std::string recommendedTarget;
    double totalMinimums = 0.0;
    double extraPaymentBudget = 0.0;
    double totalPlannedDebtPayment = 0.0;
    double projectedMonthlyInterest = 0.0;
    int estimatedMonthsToPrimaryPayoff = 0;
};

struct MonthSnapshot { std::string label; double income = 0.0; double outflow = 0.0; double leftover = 0.0; double ledgerNet = 0.0; double debtBalance = 0.0; double emergencyFund = 0.0; };
struct Scenario { std::string name; double incomeMultiplier = 1.0; double billMultiplier = 1.0; double variableMultiplier = 1.0; double extraMonthlyIncome = 0.0; double extraMonthlyBills = 0.0; double oneTimeShock = 0.0; bool active = false; };
struct ForecastMonth { std::string label; double projectedIncome = 0.0; double projectedOutflow = 0.0; double projectedLeftover = 0.0; double projectedChecking = 0.0; double projectedEmergencyFund = 0.0; double projectedDebt = 0.0; };
struct ScenarioResult { std::string scenarioName; double monthlyIncome = 0.0; double monthlyOutflow = 0.0; double monthlyLeftover = 0.0; double endingChecking = 0.0; double endingEmergencyFund = 0.0; double endingDebt = 0.0; int firstNegativeMonth = -1; };
struct DailyBalancePoint { int day = 1; double balance = 0.0; std::string eventLabel; };
struct AutoCategoryRule { std::string merchantContains; std::string category; };
struct AccountAsset { std::string name; std::string type; double balance = 0.0; };
struct OptimizationResult { std::string action; double impact = 0.0; };
struct AlertItem { std::string severity; std::string message; };

struct PlannerSettings {
    int month = 4;
    int year = 2026;
    float uiScale = 1.25f;
    int paycheckCount = 2;
    DebtStrategy debtStrategy = DebtStrategy::Avalanche;
    int forecastMonths = 6;
};

struct DayCell { int day = 1; double inflow = 0.0; double outflow = 0.0; std::vector<std::string> labels; };
struct PaycheckForecast { std::string label; int startDay = 1; int endDay = 15; double inflow = 0.0; double fixedBills = 0.0; double variableBudget = 0.0; double goalBudget = 0.0; double oneTimeBudget = 0.0; double taxReserve = 0.0; double extraDebt = 0.0; double leftover = 0.0; };

struct BudgetData {
    std::vector<IncomeSource> incomes;
    std::vector<Bill> bills;
    std::vector<VariableExpense> variableExpenses;
    std::vector<GoalBucket> goals;
    std::vector<OneTimeExpense> oneTimeExpenses;
    std::vector<Transaction> transactions;
    std::vector<PaycheckPlan> paycheckPlans;
    std::vector<PaycheckTemplate> paycheckTemplates;
    std::vector<DebtAccount> debtAccounts;
    std::vector<MonthSnapshot> history;
    std::vector<Scenario> scenarios;
    std::vector<AutoCategoryRule> autoCategoryRules;
    std::vector<AccountAsset> accountAssets;
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
    double ledgerExpenseTotal = 0.0;
    double ledgerIncomeTotal = 0.0;
    double ledgerNet = 0.0;
    int healthScore = 0;
    std::vector<DayCell> calendar;
    std::vector<PaycheckForecast> paychecks;
    std::vector<std::string> upcomingBills;
    std::vector<double> chartPlanVsActual;
    std::vector<double> chartPaycheckLeftovers;
    std::vector<double> chartHistoryLeftover;
    std::vector<double> chartHistoryDebt;
    std::vector<double> chartCategoryActuals;
    std::vector<double> chartForecastChecking;
    std::vector<double> chartForecastDebt;
    std::vector<double> chartNetWorthTrend;
    DebtPlanResult debtPlan;
    std::vector<ForecastMonth> forecast;
    std::vector<ScenarioResult> scenarioResults;
    std::vector<DailyBalancePoint> dailyBalance;
    std::vector<OptimizationResult> optimizations;
    std::vector<AlertItem> alerts;
};

} // namespace budget
