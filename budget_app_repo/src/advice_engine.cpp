#include "advice_engine.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace budget {
namespace {

ResourceLink cfpb(const std::string& title, const std::string& url) {
    return {title, "CFPB", url};
}

ResourceLink fdic(const std::string& title, const std::string& url) {
    return {title, "FDIC", url};
}

ResourceLink investorGov(const std::string& title, const std::string& url) {
    return {title, "Investor.gov", url};
}

std::string money(double value) {
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(2);
    out << "$" << value;
    return out.str();
}

} // namespace

std::vector<AdviceItem> buildAdvice(const BudgetData& data, const BudgetSummary& summary) {
    std::vector<AdviceItem> items;

    if (!summary.zeroBasedBalanced) {
        AdviceItem item;
        item.title = "Balance this week to zero";
        if (summary.weeklyLeftover > 0.0) {
            item.detail = "You still have " + money(summary.weeklyLeftover) +
                          " unassigned this week. Give every dollar a job by routing it to emergency savings, debt payoff, or a known true expense.";
        } else {
            item.detail = "You are over-assigned by " + money(std::fabs(summary.weeklyLeftover)) +
                          ". Trim discretionary categories or reduce optional goals so the weekly plan matches actual income.";
        }
        item.resources = {
            cfpb("Budgeting: How to create a budget and stick with it", "https://www.consumerfinance.gov/about-us/blog/budgeting-how-to-create-a-budget-and-stick-with-it/"),
            fdic("Budgeting and Shopping", "https://www.fdic.gov/consumer-resource-center/chapter-3-budgeting-and-shopping")
        };
        items.push_back(item);
    }

    if (summary.emergencyFundMonths < 1.0) {
        AdviceItem item;
        item.title = "Build your starter emergency fund";
        item.detail = "Your current emergency fund covers about " + std::to_string(summary.emergencyFundMonths).substr(0, 4) +
                      " months of essential weekly spending. A first milestone is a starter cushion, then progress toward multiple months of core expenses.";
        item.resources = {
            cfpb("An essential guide to building an emergency fund", "https://www.consumerfinance.gov/an-essential-guide-to-building-an-emergency-fund/"),
            investorGov("Build wealth over time through saving and investing", "https://www.investor.gov/introduction-investing/investing-basics/building-wealth-over-time")
        };
        items.push_back(item);
    }

    if (data.highInterestDebtBalance > 0.0 && data.weeklyPlan.extraDebtPayment <= 0.0) {
        AdviceItem item;
        item.title = "Add a dedicated debt payoff line";
        item.detail = "You have high-interest debt but no extra weekly debt payment planned. Even a small fixed amount improves consistency and protects cash flow.";
        item.resources = {
            cfpb("Your Money, Your Goals toolkit", "https://www.consumerfinance.gov/consumer-tools/educator-tools/your-money-your-goals/toolkit/"),
            investorGov("Making the most of your lump sum payment", "https://www.investor.gov/additional-resources/general-resources/publications-research/info-sheets/lump-sum-payouts-questions-1")
        };
        items.push_back(item);
    }

    if (summary.savingsRate < 0.10) {
        AdviceItem item;
        item.title = "Increase savings rate over time";
        item.detail = "Your planned savings and investing rate is below 10% of weekly income. Consider increasing the automatic amount after every raise, paid-off bill, or expense cut.";
        item.resources = {
            investorGov("Build wealth over time through saving and investing", "https://www.investor.gov/files/building-wealth-new"),
            cfpb("How to save for emergencies and the future", "https://www.consumerfinance.gov/about-us/blog/how-save-emergencies-and-future/")
        };
        items.push_back(item);
    }

    const double essentialRatio = summary.weeklyIncome > 0.0 ? summary.weeklyEssentials / summary.weeklyIncome : 0.0;
    if (essentialRatio > 0.6) {
        AdviceItem item;
        item.title = "Essentials are eating too much of the week";
        item.detail = "Essential costs are using about " + std::to_string(essentialRatio * 100.0).substr(0, 4) +
                      "% of weekly income. Review fixed bills, renegotiate contracts, or cut low-value subscriptions so cash flow becomes more flexible.";
        item.resources = {
            fdic("Money Smart", "https://www.fdic.gov/consumer-resource-center/money-smart"),
            cfpb("Adult financial education tools and resources", "https://www.consumerfinance.gov/consumer-tools/educator-tools/adult-financial-education/tools-and-resources/")
        };
        items.push_back(item);
    }

    if (items.empty()) {
        AdviceItem item;
        item.title = "Your plan looks disciplined";
        item.detail = "You currently have a balanced weekly plan. Keep reviewing categories weekly and move leftover cash toward emergency savings, high-interest debt, or long-term investing.";
        item.resources = {
            investorGov("Build wealth over time through saving and investing", "https://www.investor.gov/introduction-investing/investing-basics/building-wealth-over-time"),
            cfpb("Budgeting: How to create a budget and stick with it", "https://www.consumerfinance.gov/about-us/blog/budgeting-how-to-create-a-budget-and-stick-with-it/")
        };
        items.push_back(item);
    }

    return items;
}

} // namespace budget
