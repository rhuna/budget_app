#pragma once

#include "models.hpp"
#include <filesystem>
#include <string>

namespace budget
{

    class Storage
    {
    public:
        explicit Storage(std::filesystem::path filePath);
        bool load(BudgetData &data, std::string &error) const;
        bool save(const BudgetData &data, std::string &error) const;

    private:
        std::filesystem::path m_filePath;
    };

    BudgetData makeSampleData();

} // namespace budget
