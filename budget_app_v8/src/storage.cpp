#include "storage.hpp"
#include <fstream>
#include <sstream>
#include <vector>

namespace budget {
namespace {

std::vector<std::string> split(const std::string& line, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, delim)) parts.push_back(item);
    return parts;
}

int toInt(const std::string& s, int fallback = 0) { try { return std::stoi(s); } catch (...) { return fallback; } }
double toDouble(const std::string& s, double fallback = 0.0) { try { return std::stod(s); } catch (...) { return fallback; } }
bool toBool(const std::string& s, bool fallback = false) {
    if (s == "1" || s == "true" || s == "TRUE") return true;
    if (s == "0" || s == "false" || s == "FALSE") return false;
    return fallback;
}
Frequency toFrequency(const std::string& s) {
    const int v = toInt(s, 2);
    switch (v) {
        case 0: return Frequency::Weekly;
        case 1: return Frequency::BiWeekly;
        case 2: return Frequency::Monthly;
        case 3: return Frequency::Quarterly;
        case 4: return Frequency::Yearly;
        default: return Frequency::Monthly;
    }
}
TransactionType toTxType(const std::string& s) { return toInt(s, 0) == 1 ? TransactionType::Income : TransactionType::Expense; }
DebtStrategy toDebtStrategy(const std::string& s) { return toInt(s, 0) == 1 ? DebtStrategy::Snowball : DebtStrategy::Avalanche; }
int fromFrequency(Frequency f) { return static_cast<int>(f); }
int fromTxType(TransactionType t) { return t == TransactionType::Income ? 1 : 0; }
int fromDebtStrategy(DebtStrategy s) { return s == DebtStrategy::Snowball ? 1 : 0; }

} // namespace

bool Storage::load(BudgetData& data, std::string& error) const {
    if (!std::filesystem::exists(m_filePath)) {
        data = makeSampleData();
        return true;
    }

    std::ifstream in(m_filePath);
    if (!in) {
        error = "Could not open file for reading.";
        return false;
    }

    data = BudgetData{};
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const auto parts = split(line, '|');
        if (parts.empty()) continue;

        if (parts[0] == "META" && parts.size() >= 12) {
            data.checkingBalance = toDouble(parts[1]);
            data.emergencyFundBalance = toDouble(parts[2]);
            data.debtBalance = toDouble(parts[3]);
            data.savingsGoal = toDouble(parts[4]);
            data.monthlyTaxReserve = toDouble(parts[5]);
            data.settings.month = toInt(parts[6], 4);
            data.settings.year = toInt(parts[7], 2026);
            data.settings.uiScale = static_cast<float>(toDouble(parts[8], 1.25));
            data.settings.paycheckCount = toInt(parts[9], 2);
            data.settings.debtStrategy = toDebtStrategy(parts[10]);
            data.settings.forecastMonths = toInt(parts[11], 6);
        } else if (parts[0] == "INCOME" && parts.size() >= 7) {
            data.incomes.push_back({parts[1], toDouble(parts[2]), toFrequency(parts[3]), toInt(parts[4], 1), toInt(parts[5], 15), toBool(parts[6], true)});
        } else if (parts[0] == "BILL" && parts.size() >= 7) {
            data.bills.push_back({parts[1], toDouble(parts[2]), toFrequency(parts[3]), toInt(parts[4], 1), toBool(parts[5], true), toBool(parts[6], false)});
        } else if (parts[0] == "VAR" && parts.size() >= 5) {
            data.variableExpenses.push_back({parts[1], toDouble(parts[2]), toDouble(parts[3]), toBool(parts[4], false)});
        } else if (parts[0] == "GOAL" && parts.size() >= 5) {
            data.goals.push_back({parts[1], toDouble(parts[2]), toDouble(parts[3]), toDouble(parts[4])});
        } else if (parts[0] == "ONE" && parts.size() >= 5) {
            data.oneTimeExpenses.push_back({parts[1], toDouble(parts[2]), toInt(parts[3], 1), toBool(parts[4], true)});
        } else if (parts[0] == "TX" && parts.size() >= 6) {
            data.transactions.push_back({parts[1], parts[2], parts[3], toDouble(parts[4]), toTxType(parts[5])});
        } else if (parts[0] == "PAY" && parts.size() >= 9) {
            data.paycheckPlans.push_back({parts[1], toInt(parts[2], 1), toInt(parts[3], 15), toDouble(parts[4]), toDouble(parts[5]), toDouble(parts[6]), toDouble(parts[7]), toBool(parts[8], false)});
        } else if (parts[0] == "TEMPLATE" && parts.size() >= 6) {
            data.paycheckTemplates.push_back({parts[1], toDouble(parts[2]), toDouble(parts[3]), toDouble(parts[4]), toDouble(parts[5])});
        } else if (parts[0] == "DEBT" && parts.size() >= 5) {
            data.debtAccounts.push_back({parts[1], toDouble(parts[2]), toDouble(parts[3]), toDouble(parts[4])});
        } else if (parts[0] == "HISTORY" && parts.size() >= 8) {
            data.history.push_back({parts[1], toDouble(parts[2]), toDouble(parts[3]), toDouble(parts[4]), toDouble(parts[5]), toDouble(parts[6]), toDouble(parts[7])});
        } else if (parts[0] == "SCENARIO" && parts.size() >= 8) {
            data.scenarios.push_back({parts[1], toDouble(parts[2]), toDouble(parts[3]), toDouble(parts[4]), toDouble(parts[5]), toDouble(parts[6]), toDouble(parts[7]), parts.size() >= 9 ? toBool(parts[8], false) : false});
        }
    }

    return true;
}

bool Storage::save(const BudgetData& data, std::string& error) const {
    if (m_filePath.has_parent_path()) std::filesystem::create_directories(m_filePath.parent_path());

    std::ofstream out(m_filePath);
    if (!out) {
        error = "Could not open file for writing.";
        return false;
    }

    out << "META|" << data.checkingBalance << "|" << data.emergencyFundBalance << "|" << data.debtBalance << "|" << data.savingsGoal << "|" << data.monthlyTaxReserve << "|" << data.settings.month << "|" << data.settings.year << "|" << data.settings.uiScale << "|" << data.settings.paycheckCount << "|" << fromDebtStrategy(data.settings.debtStrategy) << "|" << data.settings.forecastMonths << "\\n";
    for (const auto& x : data.incomes) out << "INCOME|" << x.name << "|" << x.amount << "|" << fromFrequency(x.frequency) << "|" << x.payDay1 << "|" << x.payDay2 << "|" << (x.afterTax ? 1 : 0) << "\\n";
    for (const auto& x : data.bills) out << "BILL|" << x.name << "|" << x.amount << "|" << fromFrequency(x.frequency) << "|" << x.dueDay << "|" << (x.essential ? 1 : 0) << "|" << (x.autopay ? 1 : 0) << "\\n";
    for (const auto& x : data.variableExpenses) out << "VAR|" << x.name << "|" << x.monthlyPlan << "|" << x.actual << "|" << (x.essential ? 1 : 0) << "\\n";
    for (const auto& x : data.goals) out << "GOAL|" << x.name << "|" << x.monthlyContribution << "|" << x.currentBalance << "|" << x.targetBalance << "\\n";
    for (const auto& x : data.oneTimeExpenses) out << "ONE|" << x.name << "|" << x.amount << "|" << x.dueDay << "|" << (x.active ? 1 : 0) << "\\n";
    for (const auto& x : data.transactions) out << "TX|" << x.dateLabel << "|" << x.category << "|" << x.note << "|" << x.amount << "|" << fromTxType(x.type) << "\\n";
    for (const auto& x : data.paycheckPlans) out << "PAY|" << x.label << "|" << x.startDay << "|" << x.endDay << "|" << x.customVariableBudget << "|" << x.customGoalBudget << "|" << x.customTaxReserve << "|" << x.customExtraDebt << "|" << (x.useCustomAllocation ? 1 : 0) << "\\n";
    for (const auto& x : data.paycheckTemplates) out << "TEMPLATE|" << x.name << "|" << x.variableBudget << "|" << x.goalBudget << "|" << x.taxReserve << "|" << x.extraDebt << "\\n";
    for (const auto& x : data.debtAccounts) out << "DEBT|" << x.name << "|" << x.balance << "|" << x.apr << "|" << x.minimumPayment << "\\n";
    for (const auto& x : data.history) out << "HISTORY|" << x.label << "|" << x.income << "|" << x.outflow << "|" << x.leftover << "|" << x.ledgerNet << "|" << x.debtBalance << "|" << x.emergencyFund << "\\n";
    for (const auto& x : data.scenarios) out << "SCENARIO|" << x.name << "|" << x.incomeMultiplier << "|" << x.billMultiplier << "|" << x.variableMultiplier << "|" << x.extraMonthlyIncome << "|" << x.extraMonthlyBills << "|" << x.oneTimeShock << "|" << (x.active ? 1 : 0) << "\\n";
    return true;
}

BudgetData makeSampleData() {
    BudgetData d;
    d.incomes = {{"Main paycheck", 1100.0, Frequency::BiWeekly, 1, 15, true}, {"Partner paycheck", 950.0, Frequency::BiWeekly, 8, 22, true}};
    d.bills = {{"Rent", 1400.0, Frequency::Monthly, 1, true, true}, {"Electric", 165.0, Frequency::Monthly, 14, true, true}, {"Internet", 80.0, Frequency::Monthly, 18, true, true}, {"Phone", 70.0, Frequency::Monthly, 21, true, true}, {"Car Insurance", 120.0, Frequency::Monthly, 25, true, true}, {"Streaming", 25.0, Frequency::Monthly, 26, false, true}};
    d.variableExpenses = {{"Groceries", 600.0, 320.0, true}, {"Gas", 280.0, 145.0, true}, {"Eating Out", 180.0, 60.0, false}, {"Household / Misc", 140.0, 35.0, false}};
    d.goals = {{"Emergency Fund", 400.0, 1200.0, 5000.0}, {"Car Repair Fund", 100.0, 300.0, 1000.0}};
    d.oneTimeExpenses = {{"Birthday gift", 40.0, 12, true}, {"School fee", 85.0, 28, true}};
    d.transactions = {{"2026-04-02", "Groceries", "Walmart", 92.15, TransactionType::Expense}, {"2026-04-03", "Gas", "Shell", 34.20, TransactionType::Expense}, {"2026-04-05", "Paycheck", "Side work", 250.00, TransactionType::Income}};
    d.paycheckPlans = {{"Paycheck Window 1", 1, 15, 500.0, 220.0, 0.0, 75.0, true}, {"Paycheck Window 2", 16, 30, 700.0, 280.0, 0.0, 50.0, true}};
    d.paycheckTemplates = {{"Tight Month", 450.0, 150.0, 0.0, 100.0}, {"Normal Month", 600.0, 250.0, 0.0, 75.0}, {"Aggressive Debt", 500.0, 150.0, 0.0, 200.0}};
    d.debtAccounts = {{"Credit Card A", 1800.0, 26.99, 55.0}, {"Credit Card B", 900.0, 19.99, 35.0}, {"Personal Loan", 3200.0, 11.5, 110.0}};
    d.history = {{"2026-01", 4100.0, 3825.0, 275.0, 180.0, 6400.0, 900.0}, {"2026-02", 4100.0, 3900.0, 200.0, 120.0, 6200.0, 1000.0}, {"2026-03", 4200.0, 3950.0, 250.0, 160.0, 6050.0, 1150.0}};
    d.scenarios = {{"Base", 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, true}, {"Income Drop", 0.85, 1.0, 1.0, 0.0, 0.0, 0.0, false}, {"Rent Increase", 1.0, 1.08, 1.0, 0.0, 150.0, 0.0, false}, {"Car Repair Shock", 1.0, 1.0, 1.0, 0.0, 0.0, 800.0, false}};
    d.checkingBalance = 900.0;
    d.emergencyFundBalance = 1200.0;
    d.debtBalance = 5900.0;
    d.savingsGoal = 5000.0;
    d.monthlyTaxReserve = 0.0;
    d.settings.month = 4;
    d.settings.year = 2026;
    d.settings.uiScale = 1.25f;
    d.settings.paycheckCount = 2;
    d.settings.debtStrategy = DebtStrategy::Avalanche;
    d.settings.forecastMonths = 6;
    return d;
}

} // namespace budget
