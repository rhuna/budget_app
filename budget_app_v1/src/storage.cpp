#include "storage.hpp"

#include <fstream>
#include <utility>
#include <nlohmann/json.hpp>

namespace budget
{

    using json = nlohmann::json;

    Storage::Storage(std::filesystem::path filePath)
        : m_filePath(std::move(filePath)) {}

    bool Storage::load(BudgetData &data, std::string &error) const
    {
        try
        {
            if (!std::filesystem::exists(m_filePath))
            {
                data = makeSampleData();
                return true;
            }

            std::ifstream in(m_filePath);
            if (!in)
            {
                error = "Could not open file for reading.";
                return false;
            }

            json j;
            in >> j;

            data.checkingBalance = j.value("checking", 0.0);
            data.emergencyFundBalance = j.value("emergency", 0.0);
            data.highInterestDebtBalance = j.value("debt", 0.0);
            data.savingsGoal = j.value("goal", 0.0);

            return true;
        }
        catch (const std::exception &e)
        {
            error = e.what();
            return false;
        }
    }

    bool Storage::save(const BudgetData &data, std::string &error) const
    {
        try
        {
            json j;
            j["checking"] = data.checkingBalance;
            j["emergency"] = data.emergencyFundBalance;
            j["debt"] = data.highInterestDebtBalance;
            j["goal"] = data.savingsGoal;

            if (m_filePath.has_parent_path())
            {
                std::filesystem::create_directories(m_filePath.parent_path());
            }

            std::ofstream out(m_filePath);
            if (!out)
            {
                error = "Could not open file for writing.";
                return false;
            }

            out << j.dump(4);
            return true;
        }
        catch (const std::exception &e)
        {
            error = e.what();
            return false;
        }
    }

    BudgetData makeSampleData()
    {
        BudgetData d;
        d.checkingBalance = 800.0;
        d.emergencyFundBalance = 1000.0;
        d.highInterestDebtBalance = 2500.0;
        d.savingsGoal = 5000.0;
        return d;
    }

} // namespace budget