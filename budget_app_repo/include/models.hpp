#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

    namespace budget
{

    using json = nlohmann::json;

    enum class Frequency
    {
        Weekly,
        BiWeekly,
        Monthly,
        Quarterly,
        Yearly
    };

    inline std::string frequencyToString(Frequency f)
    {
        switch (f)
        {
        case Frequency::Weekly:
            return "Weekly";
        case Frequency::BiWeekly:
            return "Bi-Weekly";
        case Frequency::Monthly:
            return "Monthly";
        case Frequency::Quarterly:
            return "Quarterly";
        default:
            return "Yearly";
        }
    }

    struct IncomeSource
    {
        std::string name;
        double weeklyAmount = 0.0;
    };

    struct RecurringBill
    {
        std::string name;
        double amount = 0.0;
        Frequency frequency = Frequency::Monthly;
        int dueDay = 1;
        bool essential = true;
        bool autopay = false;
    };

    struct WeeklyCategory
    {
        std::string name;
        double planned = 0.0;
        double spent = 0.0;
        bool essential = false;
    };

    struct WeeklyPlan
    {
        std::string weekLabel = "Current Week";
        double extraDebtPayment = 0.0;
        double emergencyFundContribution = 0.0;
        double investingContribution = 0.0;
        std::vector<WeeklyCategory> categories;
    };

    struct BudgetData
    {
        std::vector<IncomeSource> incomes;
        std::vector<RecurringBill> recurringBills;
        WeeklyPlan weeklyPlan;

        double emergencyFundBalance = 0.0;
        double highInterestDebtBalance = 0.0;
        double checkingBalance = 0.0;
        double savingsGoal = 0.0;
    };

} // namespace budget