#include "advice_engine.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace budget {

namespace {

int daysInMonth(int month, int year) {
    switch (month) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
        case 4: case 6: case 9: case 11: return 30;
        case 2: {
            const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
            return leap ? 29 : 28;
        }
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

bool dayInPaycheckRange(int day, int start, int end) {
    return day >= start && day <= end;
}

} // namespace

BudgetSummary buildSummary(const BudgetData& data) {
    BudgetSummary s{};
    const int monthDays = daysInMonth(data.settings.month, data.settings.year);

    for (const auto& income : data.incomes) {
        if (income.frequency == Frequency::BiWeekly) {
            s.monthlyIncome += income.amount * 2.0;
        } else if (income.frequency == Frequency::Monthly) {
            s.monthlyIncome += income.amount;
        } else {
            s.monthlyIncome += monthlyFrom(income.amount, income.frequency);
        }
    }

    for (const auto& bill : data.bills) {
        const double monthly = monthlyFrom(bill.amount, bill.frequency);
        s.monthlyBills += monthly;
        if (bill.essential) s.essentialMonthly += monthly;
    }

    for (const auto& v : data.variableExpenses) {
        s.monthlyVariablePlanned += v.monthlyPlan;
        s.monthlyVariableActual += v.actual;
        if (v.essential) s.essentialMonthly += v.monthlyPlan;
    }

    for (const auto& g : data.goals) {
        s.monthlyGoals += g.monthlyContribution;
    }

    for (const auto& o : data.oneTimeExpenses) {
        if (o.active) s.monthlyOneTime += o.amount;
    }

    s.monthlyTaxReserve = data.monthlyTaxReserve;
    s.monthlyPlannedOutflow = s.monthlyBills + s.monthlyVariablePlanned + s.monthlyGoals + s.monthlyOneTime + s.monthlyTaxReserve;
    s.monthlyActualOutflow = s.monthlyBills + s.monthlyVariableActual + s.monthlyGoals + s.monthlyOneTime + s.monthlyTaxReserve;
    s.monthlyPlannedLeftover = s.monthlyIncome - s.monthlyPlannedOutflow;
    s.monthlyActualLeftover = s.monthlyIncome - s.monthlyActualOutflow;

    if (s.essentialMonthly > 0.0) {
        s.emergencyMonthsCovered = data.emergencyFundBalance / s.essentialMonthly;
    }
    if (data.savingsGoal > 0.0) {
        s.savingsProgress = std::clamp(data.emergencyFundBalance / data.savingsGoal, 0.0, 1.0);
    }

    int score = 50;
    if (s.monthlyPlannedLeftover >= 0.0) score += 15; else score -= 20;
    if (s.monthlyActualLeftover >= 0.0) score += 10; else score -= 10;
    if (s.emergencyMonthsCovered >= 1.0) score += 10; else if (s.emergencyMonthsCovered < 0.5) score -= 10;
    if (data.debtBalance <= 0.0) score += 5; else if (data.debtBalance > s.monthlyIncome * 3.0) score -= 10;
    if (data.monthlyTaxReserve > 0.0) score += 5;
    s.healthScore = std::clamp(score, 0, 100);

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
            } else if (income.frequency == Frequency::Monthly) {
                if (day == income.payDay1) {
                    cell.inflow += income.amount;
                    cell.labels.push_back("Income: " + income.name);
                }
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
    const int segmentSize = static_cast<int>(std::ceil(monthDays / static_cast<double>(paycheckCount)));

    for (int i = 0; i < paycheckCount; ++i) {
        PaycheckForecast p;
        p.startDay = i * segmentSize + 1;
        p.endDay = std::min(monthDays, (i + 1) * segmentSize);
        p.label = "Paycheck Window " + std::to_string(i + 1);

        for (const auto& income : data.incomes) {
            if (income.frequency == Frequency::BiWeekly) {
                if (dayInPaycheckRange(income.payDay1, p.startDay, p.endDay)) p.inflow += income.amount;
                if (dayInPaycheckRange(income.payDay2, p.startDay, p.endDay)) p.inflow += income.amount;
            } else if (income.frequency == Frequency::Monthly) {
                if (dayInPaycheckRange(income.payDay1, p.startDay, p.endDay)) p.inflow += income.amount;
            } else {
                p.inflow += monthlyFrom(income.amount, income.frequency) / paycheckCount;
            }
        }

        for (const auto& bill : data.bills) {
            if (bill.frequency == Frequency::Monthly) {
                if (dayInPaycheckRange(bill.dueDay, p.startDay, p.endDay)) p.fixedBills += bill.amount;
            } else {
                p.fixedBills += monthlyFrom(bill.amount, bill.frequency) / paycheckCount;
            }
        }

        p.variableBudget = s.monthlyVariablePlanned / paycheckCount;
        p.goalBudget = s.monthlyGoals / paycheckCount;
        p.taxReserve = data.monthlyTaxReserve / paycheckCount;

        for (const auto& item : data.oneTimeExpenses) {
            if (item.active && dayInPaycheckRange(item.dueDay, p.startDay, p.endDay)) {
                p.oneTimeBudget += item.amount;
            }
        }

        p.leftover = p.inflow - p.fixedBills - p.variableBudget - p.goalBudget - p.oneTimeBudget - p.taxReserve;
        s.paychecks.push_back(p);
    }

    return s;
}

std::vector<std::string> buildAdvice(const BudgetData& data, const BudgetSummary& summary) {
    std::vector<std::string> advice;

    if (summary.monthlyPlannedLeftover < 0.0) {
        advice.emplace_back("Your monthly plan is negative. Cut variable categories first, then move through subscriptions, services, and nonessential recurring bills.");
    } else {
        advice.emplace_back("Your monthly plan is positive. Assign the leftover deliberately to debt, emergency savings, sinking funds, or investing.");
    }

    if (summary.emergencyMonthsCovered < 1.0) {
        advice.emplace_back("Emergency coverage is under one month of essentials. Build a stronger buffer before expanding wants.");
    } else {
        advice.emplace_back("Emergency coverage is improving. Keep protecting it while paying down debt and funding true expenses.");
    }

    if (data.debtBalance > 0.0) {
        advice.emplace_back("Debt is still active. Use the paycheck forecast tab to see which pay window has room for extra principal payments.");
    }

    if (data.monthlyTaxReserve <= 0.0) {
        advice.emplace_back("If any income is irregular or self-employment income, add a monthly tax reserve so the forecast reflects money that is not really spendable.");
    }

    if (!summary.paychecks.empty()) {
        bool anyNegative = false;
        for (const auto& p : summary.paychecks) {
            if (p.leftover < 0.0) {
                anyNegative = true;
                break;
            }
        }
        if (anyNegative) {
            advice.emplace_back("At least one paycheck window goes negative. Move due dates when possible or pre-fund that window using the earlier paycheck.");
        }
    }

    advice.emplace_back("Use the UI scale slider to zoom the app in and make everything easier to read.");

    return advice;
}

} // namespace budget
