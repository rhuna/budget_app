#include "storage.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

namespace budget {

using json = nlohmann::json;

namespace {

Frequency intToFrequency(int value) {
    switch (value) {
        case 0: return Frequency::Weekly;
        case 1: return Frequency::BiWeekly;
        case 2: return Frequency::Monthly;
        case 3: return Frequency::Quarterly;
        case 4: return Frequency::Yearly;
        default: return Frequency::Monthly;
    }
}

void saveIncome(json& arr, const IncomeSource& item) {
    json j;
    j["name"] = item.name;
    j["amount"] = item.amount;
    j["frequency"] = static_cast<int>(item.frequency);
    j["afterTax"] = item.afterTax;
    arr.push_back(j);
}

void saveBill(json& arr, const Bill& item) {
    json j;
    j["name"] = item.name;
    j["amount"] = item.amount;
    j["frequency"] = static_cast<int>(item.frequency);
    j["dueDay"] = item.dueDay;
    j["essential"] = item.essential;
    j["autopay"] = item.autopay;
    arr.push_back(j);
}

void saveVariable(json& arr, const VariableExpense& item) {
    json j;
    j["name"] = item.name;
    j["weeklyPlan"] = item.weeklyPlan;
    j["actual"] = item.actual;
    j["essential"] = item.essential;
    arr.push_back(j);
}

void saveGoal(json& arr, const GoalBucket& item) {
    json j;
    j["name"] = item.name;
    j["weeklyContribution"] = item.weeklyContribution;
    j["currentBalance"] = item.currentBalance;
    j["targetBalance"] = item.targetBalance;
    arr.push_back(j);
}

void saveOneTime(json& arr, const OneTimeExpense& item) {
    json j;
    j["name"] = item.name;
    j["amount"] = item.amount;
    j["thisWeek"] = item.thisWeek;
    arr.push_back(j);
}

} // namespace

bool Storage::load(BudgetData& data, std::string& error) const {
    try {
        if (!std::filesystem::exists(m_filePath)) {
            data = makeSampleData();
            return true;
        }

        std::ifstream in(m_filePath);
        if (!in) {
            error = "Could not open file for reading.";
            return false;
        }

        json j;
        in >> j;

        data = BudgetData{};
        data.checkingBalance = j.value("checkingBalance", 0.0);
        data.emergencyFundBalance = j.value("emergencyFundBalance", 0.0);
        data.debtBalance = j.value("debtBalance", 0.0);
        data.savingsGoal = j.value("savingsGoal", 0.0);
        data.taxReserveWeekly = j.value("taxReserveWeekly", 0.0);

        if (j.contains("incomes") && j["incomes"].is_array()) {
            for (const auto& it : j["incomes"]) {
                IncomeSource x;
                x.name = it.value("name", "");
                x.amount = it.value("amount", 0.0);
                x.frequency = intToFrequency(it.value("frequency", 0));
                x.afterTax = it.value("afterTax", true);
                data.incomes.push_back(x);
            }
        }

        if (j.contains("bills") && j["bills"].is_array()) {
            for (const auto& it : j["bills"]) {
                Bill x;
                x.name = it.value("name", "");
                x.amount = it.value("amount", 0.0);
                x.frequency = intToFrequency(it.value("frequency", 2));
                x.dueDay = it.value("dueDay", 1);
                x.essential = it.value("essential", true);
                x.autopay = it.value("autopay", false);
                data.bills.push_back(x);
            }
        }

        if (j.contains("variableExpenses") && j["variableExpenses"].is_array()) {
            for (const auto& it : j["variableExpenses"]) {
                VariableExpense x;
                x.name = it.value("name", "");
                x.weeklyPlan = it.value("weeklyPlan", 0.0);
                x.actual = it.value("actual", 0.0);
                x.essential = it.value("essential", false);
                data.variableExpenses.push_back(x);
            }
        }

        if (j.contains("goals") && j["goals"].is_array()) {
            for (const auto& it : j["goals"]) {
                GoalBucket x;
                x.name = it.value("name", "");
                x.weeklyContribution = it.value("weeklyContribution", 0.0);
                x.currentBalance = it.value("currentBalance", 0.0);
                x.targetBalance = it.value("targetBalance", 0.0);
                data.goals.push_back(x);
            }
        }

        if (j.contains("oneTimeExpenses") && j["oneTimeExpenses"].is_array()) {
            for (const auto& it : j["oneTimeExpenses"]) {
                OneTimeExpense x;
                x.name = it.value("name", "");
                x.amount = it.value("amount", 0.0);
                x.thisWeek = it.value("thisWeek", true);
                data.oneTimeExpenses.push_back(x);
            }
        }

        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

bool Storage::save(const BudgetData& data, std::string& error) const {
    try {
        json j;
        j["checkingBalance"] = data.checkingBalance;
        j["emergencyFundBalance"] = data.emergencyFundBalance;
        j["debtBalance"] = data.debtBalance;
        j["savingsGoal"] = data.savingsGoal;
        j["taxReserveWeekly"] = data.taxReserveWeekly;

        j["incomes"] = json::array();
        for (const auto& item : data.incomes) saveIncome(j["incomes"], item);

        j["bills"] = json::array();
        for (const auto& item : data.bills) saveBill(j["bills"], item);

        j["variableExpenses"] = json::array();
        for (const auto& item : data.variableExpenses) saveVariable(j["variableExpenses"], item);

        j["goals"] = json::array();
        for (const auto& item : data.goals) saveGoal(j["goals"], item);

        j["oneTimeExpenses"] = json::array();
        for (const auto& item : data.oneTimeExpenses) saveOneTime(j["oneTimeExpenses"], item);

        if (m_filePath.has_parent_path()) {
            std::filesystem::create_directories(m_filePath.parent_path());
        }

        std::ofstream out(m_filePath);
        if (!out) {
            error = "Could not open file for writing.";
            return false;
        }

        out << j.dump(2);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

BudgetData makeSampleData() {
    BudgetData d;
    d.incomes = {
        {"Main paycheck", 1100.0, Frequency::Weekly, true},
        {"Side work", 250.0, Frequency::Weekly, true}
    };

    d.bills = {
        {"Rent", 1400.0, Frequency::Monthly, 1, true, true},
        {"Electric", 165.0, Frequency::Monthly, 14, true, true},
        {"Internet", 80.0, Frequency::Monthly, 18, true, true},
        {"Phone", 70.0, Frequency::Monthly, 21, true, true},
        {"Car Insurance", 120.0, Frequency::Monthly, 25, true, true},
        {"Streaming", 25.0, Frequency::Monthly, 26, false, true}
    };

    d.variableExpenses = {
        {"Groceries", 150.0, 90.0, true},
        {"Gas", 70.0, 35.0, true},
        {"Eating Out", 50.0, 20.0, false},
        {"Household / Misc", 40.0, 10.0, false}
    };

    d.goals = {
        {"Emergency Fund", 100.0, 1200.0, 5000.0},
        {"Car Repair Fund", 25.0, 300.0, 1000.0}
    };

    d.oneTimeExpenses = {
        {"Birthday gift", 40.0, true}
    };

    d.checkingBalance = 900.0;
    d.emergencyFundBalance = 1200.0;
    d.debtBalance = 2800.0;
    d.savingsGoal = 5000.0;
    d.taxReserveWeekly = 0.0;
    return d;
}

} // namespace budget
