#include "automation_engine.hpp"

namespace budget {

std::vector<AutomationRule> buildDefaultAutomationRules(const BudgetData& data, const BudgetSummary& summary) {
    std::vector<AutomationRule> rules;

    rules.push_back({"Paycheck Buffer Sweep", "If a paycheck lands and checking is above your baseline, move excess toward savings or debt.", true});
    rules.push_back({"Bill Reminder", "If a monthly bill is due soon, surface a reminder in the alerts area.", true});
    rules.push_back({"Debt Overflow Rule", "If monthly leftover is positive, route part of it toward the recommended debt target.", data.debtBalance > 0.0});
    rules.push_back({"Emergency Fund Rule", "If emergency coverage is below one month, prioritize emergency savings before lifestyle spending.", summary.emergencyMonthsCovered < 1.0});
    rules.push_back({"Low Buffer Guardrail", "If projected checking turns negative, stop optional spending and flag risk immediately.", true});

    return rules;
}

} // namespace budget
