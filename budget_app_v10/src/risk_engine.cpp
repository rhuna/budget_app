#include "risk_engine.hpp"

namespace budget {

std::vector<RiskSignal> buildRiskSignals(const BudgetData& data, const BudgetSummary& summary) {
    std::vector<RiskSignal> out;

    if (summary.monthlyPlannedLeftover < 0.0) {
        out.push_back({"HIGH", "Monthly plan is negative. Current plan does not cashflow safely."});
    }
    if (summary.emergencyMonthsCovered < 1.0) {
        out.push_back({"MEDIUM", "Emergency fund is under one month of essential expenses."});
    }
    if (!summary.forecast.empty() && summary.forecast.back().projectedChecking < 0.0) {
        out.push_back({"HIGH", "Forecasted checking balance goes negative before the end of the forecast window."});
    }
    if (data.debtBalance > summary.monthlyIncome * 3.0 && summary.monthlyIncome > 0.0) {
        out.push_back({"MEDIUM", "Debt load is high relative to monthly income."});
    }
    if (summary.ledgerNet < 0.0) {
        out.push_back({"MEDIUM", "Recent ledger activity is net negative."});
    }

    return out;
}

} // namespace budget
