#include "storage.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

namespace budget
{
    using json = nlohmann::json;

    namespace
    {

        std::string frequencyToJson(Frequency frequency)
        {
            return frequencyToString(frequency);
        }

        Frequency frequencyFromJson(const std::string &value)
        {
            if (value == "Weekly")
                return Frequency::Weekly;
            if (value == "Bi-Weekly")
                return Frequency::BiWeekly;
            if (value == "Monthly")
                return Frequency::Monthly;
            if (value == "Quarterly")
                return Frequency::Quarterly;
            return Frequency::Yearly;
        }

        void to_json(json &j, const IncomeSource &item)
        {
            j = json::object();
            j["name"] = item.name;
            j["weeklyAmount"] = item.weeklyAmount;
        }

        void from_json(const json &j, IncomeSource &item)
        {
            item.name = j.value("name", "");
            item.weeklyAmount = j.value("weeklyAmount", 0.0);
        }

        void to_json(json &j, const RecurringBill &item)
        {
            j = json::object();
            j["name"] = item.name;
            j["amount"] = item.amount;
            j["frequency"] = frequencyToJson(item.frequency);
            j["dueDay"] = item.dueDay;
            j["essential"] = item.essential;
            j["autopay"] = item.autopay;
        }

        void from_json(const json &j, RecurringBill &item)
        {
            item.name = j.value("name", "");
            item.amount = j.value("amount", 0.0);
            item.frequency = frequencyFromJson(j.value("frequency", "Monthly"));
            item.dueDay = j.value("dueDay", 1);
            item.essential = j.value("essential", true);
            item.autopay = j.value("autopay", false);
        }

        void to_json(json &j, const WeeklyCategory &item)
        {
            j = json::object();
            j["name"] = item.name;
            j["planned"] = item.planned;
            j["spent"] = item.spent;
            j["essential"] = item.essential;
        }

        void from_json(const json &j, WeeklyCategory &item)
        {
            item.name = j.value("name", "");
            item.planned = j.value("planned", 0.0);
            item.spent = j.value("spent", 0.0);
            item.essential = j.value("essential", false);
        }

        void to_json(json &j, const WeeklyPlan &item)
        {
            j = json::object();
            j["weekLabel"] = item.weekLabel;
            j["extraDebtPayment"] = item.extraDebtPayment;
            j["emergencyFundContribution"] = item.emergencyFundContribution;
            j["investingContribution"] = item.investingContribution;
            j["categories"] = json::array();

            for (const auto &category : item.categories)
            {
                json categoryJson;
                to_json(categoryJson, category);
                j["categories"].push_back(categoryJson);
            }
        }

        void from_json(const json &j, WeeklyPlan &item)
        {
            item.weekLabel = j.value("weekLabel", "Current Week");
            item.extraDebtPayment = j.value("extraDebtPayment", 0.0);
            item.emergencyFundContribution = j.value("emergencyFundContribution", 0.0);
            item.investingContribution = j.value("investingContribution", 0.0);

            item.categories.clear();
            if (j.contains("categories") && j.at("categories").is_array())
            {
                for (const auto &categoryJson : j.at("categories"))
                {
                    WeeklyCategory category;
                    from_json(categoryJson, category);
                    item.categories.push_back(category);
                }
            }
        }

        void to_json(json &j, const BudgetData &item)
        {
            j = json::object();
            j["incomes"] = item.incomes;
            j["recurringBills"] = item.recurringBills;

            json weeklyPlanJson;
            to_json(weeklyPlanJson, item.weeklyPlan);
            j["weeklyPlan"] = weeklyPlanJson;

            j["emergencyFundBalance"] = item.emergencyFundBalance;
            j["highInterestDebtBalance"] = item.highInterestDebtBalance;
            j["checkingBalance"] = item.checkingBalance;
            j["savingsGoal"] = item.savingsGoal;
        }

        void from_json(const json &j, BudgetData &item)
        {
            item.incomes = j.value("incomes", std::vector<IncomeSource>{});
            item.recurringBills = j.value("recurringBills", std::vector<RecurringBill>{});

            if (j.contains("weeklyPlan") && j.at("weeklyPlan").is_object())
            {
                from_json(j.at("weeklyPlan"), item.weeklyPlan);
            }
            else
            {
                item.weeklyPlan = WeeklyPlan{};
            }

            item.emergencyFundBalance = j.value("emergencyFundBalance", 0.0);
            item.highInterestDebtBalance = j.value("highInterestDebtBalance", 0.0);
            item.checkingBalance = j.value("checkingBalance", 0.0);
            item.savingsGoal = j.value("savingsGoal", 0.0);
        }

    } // namespace

    Storage::Storage(std::filesystem::path filePath) : m_filePath(std::move(filePath)) {}

    bool Storage::load(BudgetData &data, std::string &error) const
    {
        if (!std::filesystem::exists(m_filePath))
        {
            data = makeSampleData();
            return true;
        }

        try
        {
            std::ifstream in(m_filePath);
            json j;
            in >> j;
            data = j.get<BudgetData>();
            return true;
        }
        catch (const std::exception &ex)
        {
            error = ex.what();
            return false;
        }
    }

    bool Storage::save(const BudgetData &data, std::string &error) const
    {
        try
        {
            std::filesystem::create_directories(m_filePath.parent_path());
            std::ofstream out(m_filePath);
            out << json(data).dump(2);
            return true;
        }
        catch (const std::exception &ex)
        {
            error = ex.what();
            return false;
        }
    }

    BudgetData makeSampleData()
    {
        BudgetData data;
        data.incomes = {
            {"Main paycheck", 950.00},
            {"Side income", 150.00}};

        data.recurringBills = {
            {"Rent", 1250.00, Frequency::Monthly, 1, true, true},
            {"Electric", 140.00, Frequency::Monthly, 12, true, true},
            {"Internet", 70.00, Frequency::Monthly, 7, true, true},
            {"Car insurance", 110.00, Frequency::Monthly, 18, true, true},
            {"Streaming", 18.00, Frequency::Monthly, 22, false, true}};

        data.weeklyPlan.weekLabel = "Week of April 1";
        data.weeklyPlan.extraDebtPayment = 50.0;
        data.weeklyPlan.emergencyFundContribution = 75.0;
        data.weeklyPlan.investingContribution = 40.0;
        data.weeklyPlan.categories = {
            {"Groceries", 120.0, 80.0, true},
            {"Gas", 55.0, 35.0, true},
            {"Eating out", 45.0, 28.0, false},
            {"Fun money", 40.0, 12.0, false}};

        data.emergencyFundBalance = 900.0;
        data.highInterestDebtBalance = 2200.0;
        data.checkingBalance = 640.0;
        data.savingsGoal = 3000.0;
        return data;
    }

} // namespace budget