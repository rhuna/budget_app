#include "advice_engine.hpp"
#include <algorithm>
#include <cmath>

namespace budget {
namespace {

int daysInMonth(int month, int year) {
    switch (month) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
        case 4: case 6: case 9: case 11: return 30;
        case 2: { const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0); return leap ? 29 : 28; }
        default: return 30;
    }
}

double monthlyFrom(double amount, Frequency frequency) {
    switch (frequency) {
        case Frequency::Weekly: return amount * 52.0 / 12.0;
        case Frequency::BiWeekly: return amount * 26.0 / 12.0;
        case Frequency::Monthly: return amount;
        case Frequency::Quarterly: return amount / 3.0;
        case Frequency::Yearly: return amount / 12.0;
        default: return amount;
    }
}

bool dayInRange(int day, int start, int end) { return day >= start && day <= end; }

PaycheckPlan defaultPlanForWindow(int i, int monthDays, int paycheckCount) {
    const int segmentSize = static_cast<int>(std::ceil(monthDays / static_cast<double>(paycheckCount)));
    PaycheckPlan p;
    p.label = "Paycheck Window " + std::to_string(i + 1);
    p.startDay = i * segmentSize + 1;
    p.endDay = std::min(monthDays, (i + 1) * segmentSize);
    return p;
}

DebtPlanResult buildDebtPlan(const BudgetData& data, double monthlyLeftover) {
    DebtPlanResult result{};
    if (data.debtAccounts.empty()) return result;

    std::vector<DebtAccount> accounts = data.debtAccounts;
    for (const auto& d : accounts) {
        result.totalMinimums += d.minimumPayment;
        result.projectedMonthlyInterest += d.balance * (d.apr / 100.0) / 12.0;
    }

    result.extraPaymentBudget = std::max(0.0, monthlyLeftover);
    result.totalPlannedDebtPayment = result.totalMinimums + result.extraPaymentBudget;

    if (data.settings.debtStrategy == DebtStrategy::Avalanche) {
        std::sort(accounts.begin(), accounts.end(), [](const DebtAccount& a, const DebtAccount& b) {
            if (a.apr == b.apr) return a.balance > b.balance;
            return a.apr > b.apr;
        });
    } else {
        std::sort(accounts.begin(), accounts.end(), [](const DebtAccount& a, const DebtAccount& b) {
            if (a.balance == b.balance) return a.apr > b.apr;
            return a.balance < b.balance;
        });
    }

    result.recommendedTarget = accounts.front().name;
    const double targetBalance = accounts.front().balance;
    const double targetMin = accounts.front().minimumPayment;
    const double targetExtra = std::max(0.0, result.totalPlannedDebtPayment - targetMin);
    const double targetMonthly = targetMin + targetExtra - (accounts.front().balance * (accounts.front().apr / 100.0) / 12.0);
    if (targetMonthly > 0.0) {
        result.estimatedMonthsToPrimaryPayoff = static_cast<int>(std::ceil(targetBalance / targetMonthly));
    }
    return result;
}

} // namespace

BudgetSummary buildSummary(const BudgetData& data) {
    BudgetSummary s{};
    const int monthDays = daysInMonth(data.settings.month, data.settings.year);

    for (const auto& income : data.incomes) {
        if (income.frequency == Frequency::BiWeekly) s.monthlyIncome += income.amount * 2.0;
        else if (income.frequency == Frequency::Monthly) s.monthlyIncome += income.amount;
        else s.monthlyIncome += monthlyFrom(income.amount, income.frequency);
    }

    for (const auto& bill : data.bills) {
        const double monthly = monthlyFrom(bill.amount, bill.frequency);
        s.monthlyBills += monthly;
        if (bill.essential) s.essentialMonthly += monthly;
        s.upcomingBills.push_back(std::to_string(bill.dueDay) + ": " + bill.name + " ($" + std::to_string(static_cast<int>(bill.amount)) + ")");
    }
    std::sort(s.upcomingBills.begin(), s.upcomingBills.end());

    for (const auto& item : data.variableExpenses) {
        s.monthlyVariablePlanned += item.monthlyPlan;
        s.monthlyVariableActual += item.actual;
        if (item.essential) s.essentialMonthly += item.monthlyPlan;
    }

    for (const auto& goal : data.goals) s.monthlyGoals += goal.monthlyContribution;
    for (const auto& one : data.oneTimeExpenses) if (one.active) s.monthlyOneTime += one.amount;

    for (const auto& tx : data.transactions) {
        if (tx.type == TransactionType::Expense) s.ledgerExpenseTotal += tx.amount;
        else s.ledgerIncomeTotal += tx.amount;
    }
    s.ledgerNet = s.ledgerIncomeTotal - s.ledgerExpenseTotal;

    s.monthlyTaxReserve = data.monthlyTaxReserve;
    s.monthlyPlannedOutflow = s.monthlyBills + s.monthlyVariablePlanned + s.monthlyGoals + s.monthlyOneTime + s.monthlyTaxReserve;
    s.monthlyActualOutflow = s.monthlyBills + s.monthlyVariableActual + s.monthlyGoals + s.monthlyOneTime + s.monthlyTaxReserve;
    s.monthlyPlannedLeftover = s.monthlyIncome - s.monthlyPlannedOutflow;
    s.monthlyActualLeftover = s.monthlyIncome - s.monthlyActualOutflow;

    if (s.essentialMonthly > 0.0) s.emergencyMonthsCovered = data.emergencyFundBalance / s.essentialMonthly;
    if (data.savingsGoal > 0.0) s.savingsProgress = std::clamp(data.emergencyFundBalance / data.savingsGoal, 0.0, 1.0);

    s.debtPlan = buildDebtPlan(data, s.monthlyPlannedLeftover);

    int score = 50;
    if (s.monthlyPlannedLeftover >= 0.0) score += 15; else score -= 20;
    if (s.monthlyActualLeftover >= 0.0) score += 10; else score -= 10;
    if (s.emergencyMonthsCovered >= 1.0) score += 10; else if (s.emergencyMonthsCovered < 0.5) score -= 10;
    if (data.debtBalance <= 0.0) score += 5; else if (data.debtBalance > s.monthlyIncome * 3.0) score -= 10;
    if (data.monthlyTaxReserve > 0.0) score += 5;
    if (s.ledgerNet < 0.0) score -= 5;
    s.healthScore = std::clamp(score, 0, 100);

    s.chartPlanVsActual = { s.monthlyIncome, s.monthlyPlannedOutflow, s.monthlyActualOutflow, s.monthlyPlannedLeftover, s.monthlyActualLeftover };

    s.calendar.resize(static_cast<std::size_t>(monthDays));
    for (int day = 1; day <= monthDays; ++day) {
        auto& cell = s.calendar[static_cast<std::size_t>(day - 1)];
        cell.day = day;
        for (const auto& income : data.incomes) {
            if (income.frequency == Frequency::BiWeekly) {
                if (day == income.payDay1 || day == income.payDay2) {
                    cell.inflow += income.amount;
                    cell.labels.push_back("Paycheck: " + income.name);
                }
            } else if (income.frequency == Frequency::Monthly && day == income.payDay1) {
                cell.inflow += income.amount;
                cell.labels.push_back("Income: " + income.name);
            }
        }
        for (const auto& bill : data.bills) {
            if (bill.frequency == Frequency::Monthly && day == bill.dueDay) {
                cell.outflow += bill.amount;
                cell.labels.push_back("Bill: " + bill.name);
            }
        }
        for (const auto& item : data.oneTimeExpenses) {
            if (item.active && day == item.dueDay) {
                cell.outflow += item.amount;
                cell.labels.push_back("One-time: " + item.name);
            }
        }
    }

    s.paychecks.clear();
    const int paycheckCount = std::max(1, data.settings.paycheckCount);
    for (int i = 0; i < paycheckCount; ++i) {
        const PaycheckPlan fallback = defaultPlanForWindow(i, monthDays, paycheckCount);
        const PaycheckPlan* plan = i < static_cast<int>(data.paycheckPlans.size()) ? &data.paycheckPlans[static_cast<std::size_t>(i)] : &fallback;

        PaycheckForecast p;
        p.label = plan->label;
        p.startDay = plan->startDay;
        p.endDay = plan->endDay;

        for (const auto& income : data.incomes) {
            if (income.frequency == Frequency::BiWeekly) {
                if (dayInRange(income.payDay1, p.startDay, p.endDay)) p.inflow += income.amount;
                if (dayInRange(income.payDay2, p.startDay, p.endDay)) p.inflow += income.amount;
            } else if (income.frequency == Frequency::Monthly) {
                if (dayInRange(income.payDay1, p.startDay, p.endDay)) p.inflow += income.amount;
            } else {
                p.inflow += monthlyFrom(income.amount, income.frequency) / paycheckCount;
            }
        }

        for (const auto& bill : data.bills) {
            if (bill.frequency == Frequency::Monthly) {
                if (dayInRange(bill.dueDay, p.startDay, p.endDay)) p.fixedBills += bill.amount;
            } else {
                p.fixedBills += monthlyFrom(bill.amount, bill.frequency) / paycheckCount;
            }
        }

        if (plan->useCustomAllocation) {
            p.variableBudget = plan->customVariableBudget;
            p.goalBudget = plan->customGoalBudget;
            p.taxReserve = plan->customTaxReserve;
            p.extraDebt = plan->customExtraDebt;
        } else {
            p.variableBudget = s.monthlyVariablePlanned / paycheckCount;
            p.goalBudget = s.monthlyGoals / paycheckCount;
            p.taxReserve = data.monthlyTaxReserve / paycheckCount;
            p.extraDebt = data.debtBalance > 0.0 ? std::max(0.0, s.monthlyPlannedLeftover / paycheckCount) : 0.0;
        }

        for (const auto& item : data.oneTimeExpenses) {
            if (item.active && dayInRange(item.dueDay, p.startDay, p.endDay)) p.oneTimeBudget += item.amount;
        }

        p.leftover = p.inflow - p.fixedBills - p.variableBudget - p.goalBudget - p.oneTimeBudget - p.taxReserve - p.extraDebt;
        s.chartPaycheckLeftovers.push_back(p.leftover);
        s.paychecks.push_back(p);
    }

    return s;
}

std::vector<std::string> buildAdvice(const BudgetData& data, const BudgetSummary& summary) {
    std::vector<std::string> advice;
    if (summary.monthlyPlannedLeftover < 0.0) advice.emplace_back("Your monthly plan is negative. Cut variable categories first, then review subscriptions and nonessential recurring bills.");
    else advice.emplace_back("Your monthly plan is positive. Assign leftover money to debt, emergency savings, sinking funds, or investing.");
    if (summary.emergencyMonthsCovered < 1.0) advice.emplace_back("Emergency coverage is under one month of essentials. Build a stronger buffer before expanding wants.");
    else advice.emplace_back("Emergency coverage is improving. Keep protecting it while paying down debt and funding true expenses.");
    if (data.debtBalance > 0.0) advice.emplace_back("Debt strategy view is active. Use Avalanche for highest-rate-first or Snowball for smallest-balance-first momentum.");
    if (data.monthlyTaxReserve <= 0.0) advice.emplace_back("If any income is irregular or self-employment income, add a monthly tax reserve.");
    if (summary.ledgerNet < 0.0) advice.emplace_back("Your transaction ledger is net negative. Review recent spending categories and compare them to the monthly plan.");
    bool anyNegative = false;
    for (const auto& p : summary.paychecks) if (p.leftover < 0.0) { anyNegative = true; break; }
    if (anyNegative) advice.emplace_back("At least one paycheck window is negative. Shift bill timing or lower custom allocation in that window.");
    if (!summary.debtPlan.recommendedTarget.empty()) advice.emplace_back("Current debt target: " + summary.debtPlan.recommendedTarget + ".");
    advice.emplace_back("This build keeps the v9 dashboard and calendar overview, then adds debt payoff strategy tools and paycheck templates.");
    return advice;
}

} // namespace budget
