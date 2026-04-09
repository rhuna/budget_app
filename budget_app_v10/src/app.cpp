#include "app.hpp"
#include "advice_engine.hpp"
#include "financial_engine.hpp"
#include "automation_engine.hpp"
#include "risk_engine.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <cfloat>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace budget {
namespace {

const char* kMonths[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
const char* kFrequencies[] = {"Weekly","Bi-Weekly","Monthly","Quarterly","Yearly"};
const char* kTxTypes[] = {"Expense", "Income"};
const char* kDebtStrategies[] = {"Avalanche", "Snowball"};

template <typename T>
void erase_index(std::vector<T> &v, std::size_t i)
{
    v.erase(v.begin() + static_cast<long long>(i));
}

double computeNetWorth(const BudgetData &data)
{
    double assets = data.checkingBalance + data.emergencyFundBalance;

    for (const auto &g : data.goals)
    {
        assets += g.currentBalance;
    }

    for (const auto &a : data.accountAssets)
    {
        assets += a.balance;
    }

    return assets - data.debtBalance;
}

double computeLiquidAssets(const BudgetData &data)
{
    double assets = data.checkingBalance + data.emergencyFundBalance;

    for (const auto &a : data.accountAssets)
    {
        if (a.type == "Cash" || a.type == "Savings")
        {
            assets += a.balance;
        }
    }

    return assets;
}

int frequencyToIndex(Frequency f) {
    switch (f) {
        case Frequency::Weekly: return 0;
        case Frequency::BiWeekly: return 1;
        case Frequency::Monthly: return 2;
        case Frequency::Quarterly: return 3;
        case Frequency::Yearly: return 4;
        default: return 2;
    }
}
Frequency indexToFrequency(int i) {
    switch (i) {
        case 0: return Frequency::Weekly;
        case 1: return Frequency::BiWeekly;
        case 2: return Frequency::Monthly;
        case 3: return Frequency::Quarterly;
        case 4: return Frequency::Yearly;
        default: return Frequency::Monthly;
    }
}
int debtStrategyToIndex(DebtStrategy s) { return s == DebtStrategy::Snowball ? 1 : 0; }
DebtStrategy indexToDebtStrategy(int i) { return i == 1 ? DebtStrategy::Snowball : DebtStrategy::Avalanche; }

void moneyText(const char* label, double value) {
    ImGui::Text("%s", label);
    ImGui::SameLine(320.0f);
    ImGui::Text("$%.2f", value);
}

void summaryCard(const char* title, const char* value) {
    ImGui::BeginChild(title, ImVec2(240, 80), true);
    ImGui::TextUnformatted(title);
    ImGui::Separator();
    ImGui::TextWrapped("%s", value);
    ImGui::EndChild();
}

void ensurePaycheckPlans(BudgetData& data) {
    const int count = data.settings.paycheckCount < 1 ? 1 : data.settings.paycheckCount;
    if (static_cast<int>(data.paycheckPlans.size()) < count) {
        for (int i = static_cast<int>(data.paycheckPlans.size()); i < count; ++i) {
            PaycheckPlan p;
            p.label = "Paycheck Window " + std::to_string(i + 1);
            p.startDay = i == 0 ? 1 : (i * 15) + 1;
            p.endDay = (i + 1) * 15;
            data.paycheckPlans.push_back(p);
        }
    } else if (static_cast<int>(data.paycheckPlans.size()) > count) {
        data.paycheckPlans.resize(static_cast<std::size_t>(count));
    }
}

void drawMiniBars(const char* id, const std::vector<double>& values, float height) {
    if (values.empty()) return;
    std::vector<float> plotValues;
    plotValues.reserve(values.size());
    float minValue = FLT_MAX;
    float maxValue = -FLT_MAX;
    for (double v : values) {
        float fv = static_cast<float>(v);
        plotValues.push_back(fv);
        if (fv < minValue) minValue = fv;
        if (fv > maxValue) maxValue = fv;
    }
    if (minValue == maxValue) {
        minValue -= 1.0f;
        maxValue += 1.0f;
    }
    ImGui::PlotHistogram(id, plotValues.data(), static_cast<int>(plotValues.size()), 0, nullptr, minValue, maxValue, ImVec2(0, height));
}

void addCurrentMonthSnapshot(BudgetData& data, const BudgetSummary& summary) {
    MonthSnapshot snap;
    snap.label = std::to_string(data.settings.year) + "-" + (data.settings.month < 10 ? "0" : "") + std::to_string(data.settings.month);
    snap.income = summary.monthlyIncome;
    snap.outflow = summary.monthlyActualOutflow;
    snap.leftover = summary.monthlyActualLeftover;
    snap.ledgerNet = summary.ledgerNet;
    snap.debtBalance = data.debtBalance;
    snap.emergencyFund = data.emergencyFundBalance;
    for (auto& existing : data.history) {
        if (existing.label == snap.label) { existing = snap; return; }
    }
    data.history.push_back(snap);
}

bool transactionExists(const BudgetData& data, const Transaction& t) {
    for (const auto& x : data.transactions) {
        if (x.dateLabel == t.dateLabel && x.note == t.note && x.amount == t.amount && x.type == t.type) return true;
    }
    return false;
}

std::vector<std::string> splitCsvBasic(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) out.push_back(item);
    return out;
}

int importCsvTransactions(BudgetData& data, const std::string& filepath) {
    std::ifstream in(filepath);
    if (!in) return -1;

    std::string line;
    bool headerSkipped = false;
    int added = 0;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto parts = splitCsvBasic(line);
        if (parts.size() < 3) continue;

        if (!headerSkipped) {
            const std::string maybeHeader = parts[0];
            if (maybeHeader.find("date") != std::string::npos || maybeHeader.find("Date") != std::string::npos) {
                headerSkipped = true;
                continue;
            }
            headerSkipped = true;
        }

        Transaction t;
        t.dateLabel = parts[0];
        t.note = parts[1];
        double amount = 0.0;
        try { amount = std::stod(parts[2]); } catch (...) { continue; }

        if (parts.size() >= 4) {
            t.category = parts[3];
        } else {
            t.category = "Imported";
        }

        if (amount < 0.0) {
            t.type = TransactionType::Expense;
            t.amount = -amount;
        } else {
            t.type = TransactionType::Income;
            t.amount = amount;
        }

        if (!transactionExists(data, t)) {
            data.transactions.push_back(t);
            ++added;
        }
    }

    applyAutoCategorization(data);
    return added;
}

} // namespace

void runApp(Storage& storage) {
    BudgetData data;
    std::string error;
    storage.load(data, error);

    if (!glfwInit()) return;
    GLFWwindow* window = glfwCreateWindow(1580, 1020, "BudgetPilot v16 Full Platform Upgrade", nullptr, nullptr);
    if (!window) { glfwTerminate(); return; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    char saveNotice[256] = "";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui::GetIO().FontGlobalScale = data.settings.uiScale;
        ensurePaycheckPlans(data);

        BudgetSummary summary = buildSummary(data);
        auto advice = buildAdvice(data, summary);
        auto autoBudget = buildAutoBudget(data, summary);
        auto goalProjections = buildGoalProjections(data);
        auto automationRules = buildDefaultAutomationRules(data, summary);
        auto riskSignals = buildRiskSignals(data, summary);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(1560, 1000), ImGuiCond_Once);
        ImGui::Begin("BudgetPilot");

        if (ImGui::Button("Save")) {
            std::string saveError;
            if (storage.save(data, saveError)) std::snprintf(saveNotice, sizeof(saveNotice), "Saved to data/budget_data.txt");
            else std::snprintf(saveNotice, sizeof(saveNotice), "Save failed: %s", saveError.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Sample Data")) {
            data = makeSampleData();
            std::snprintf(saveNotice, sizeof(saveNotice), "Reset to sample data");
        }
        ImGui::SameLine();
        if (ImGui::Button("Close Out This Month")) {
            addCurrentMonthSnapshot(data, summary);
            std::snprintf(saveNotice, sizeof(saveNotice), "Month snapshot saved");
        }
        ImGui::SameLine();
        if (ImGui::Button("Re-run Auto Categorization")) {
            applyAutoCategorization(data);
            std::snprintf(saveNotice, sizeof(saveNotice), "Auto categorization applied");
        }
        ImGui::SameLine();
        ImGui::TextWrapped("%s", saveNotice);

        ImGui::Separator();
        ImGui::TextUnformatted("Planner Controls");
        int monthIndex = data.settings.month - 1;
        if (monthIndex < 0) monthIndex = 0;
        if (monthIndex > 11) monthIndex = 11;
        if (ImGui::Combo("Month", &monthIndex, kMonths, 12)) data.settings.month = monthIndex + 1;
        ImGui::InputInt("Year", &data.settings.year);
        ImGui::SliderFloat("Zoom / UI scale", &data.settings.uiScale, 1.0f, 2.25f, "%.2fx");
        ImGui::SliderInt("Paycheck windows", &data.settings.paycheckCount, 1, 4);
        ImGui::SliderInt("Forecast months", &data.settings.forecastMonths, 3, 12);
        int debtStrategyIdx = debtStrategyToIndex(data.settings.debtStrategy);
        if (ImGui::Combo("Debt strategy", &debtStrategyIdx, kDebtStrategies, IM_ARRAYSIZE(kDebtStrategies))) {
            data.settings.debtStrategy = indexToDebtStrategy(debtStrategyIdx);
        }

        ImGui::Separator();
        char score[64], income[64], planned[64], actual[64], ledgerNet[64];
        std::snprintf(score, sizeof(score), "%d / 100", summary.healthScore);
        std::snprintf(income, sizeof(income), "$%.2f / month", summary.monthlyIncome);
        std::snprintf(planned, sizeof(planned), "$%.2f left planned", summary.monthlyPlannedLeftover);
        std::snprintf(actual, sizeof(actual), "$%.2f left actual", summary.monthlyActualLeftover);
        std::snprintf(ledgerNet, sizeof(ledgerNet), "$%.2f ledger net", summary.ledgerNet);

        summaryCard("Budget Health", score); ImGui::SameLine();
        summaryCard("Monthly Income", income); ImGui::SameLine();
        summaryCard("Planned Leftover", planned); ImGui::SameLine();
        summaryCard("Actual Leftover", actual); ImGui::SameLine();
        summaryCard("Ledger Net", ledgerNet);

        ImGui::Separator();

        if (ImGui::BeginTabBar("PlannerTabs")) {
            if (ImGui::BeginTabItem("Dashboard + Charts")) {
                moneyText("Monthly income", summary.monthlyIncome);
                moneyText("Monthly fixed bills", summary.monthlyBills);
                moneyText("Monthly variable plan", summary.monthlyVariablePlanned);
                moneyText("Monthly actual variable", summary.monthlyVariableActual);
                moneyText("Monthly goal contributions", summary.monthlyGoals);
                moneyText("Monthly one-time costs", summary.monthlyOneTime);
                moneyText("Monthly tax reserve", summary.monthlyTaxReserve);
                ImGui::Separator();
                moneyText("Planned total outflow", summary.monthlyPlannedOutflow);
                moneyText("Actual total outflow", summary.monthlyActualOutflow);
                moneyText("Planned leftover", summary.monthlyPlannedLeftover);
                moneyText("Actual leftover", summary.monthlyActualLeftover);
                ImGui::Separator();
                ImGui::TextUnformatted("Plan vs actual trend");
                drawMiniBars("##planactual", summary.chartPlanVsActual, 100.0f);
                ImGui::TextUnformatted("Paycheck leftover trend");
                drawMiniBars("##payleftover", summary.chartPaycheckLeftovers, 100.0f);
                ImGui::Separator();
                ImGui::TextUnformatted("Upcoming bills");
                for (const auto& line : summary.upcomingBills) ImGui::BulletText("%s", line.c_str());
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Calendar Overview")) {
                const int cols = 7;
                if (ImGui::BeginTable("CalendarTable", cols, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    for (int c = 0; c < cols; ++c) ImGui::TableSetupColumn("");
                    int dayIndex = 0;
                    for (int row = 0; row < 6; ++row) {
                        ImGui::TableNextRow();
                        for (int c = 0; c < cols; ++c) {
                            ImGui::TableSetColumnIndex(c);
                            if (dayIndex < static_cast<int>(summary.calendar.size())) {
                                const auto& cell = summary.calendar[static_cast<std::size_t>(dayIndex)];
                                ImGui::Text("Day %d", cell.day);
                                ImGui::Text("In: $%.0f", cell.inflow);
                                ImGui::Text("Out: $%.0f", cell.outflow);
                                for (const auto& label : cell.labels) ImGui::BulletText("%s", label.c_str());
                                ++dayIndex;
                            }
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Manage Items")) {
                static char incomeName[128] = "";
                static float incomeAmount = 0.0f;
                static int incomeFreq = 1;
                static int incomeDay1 = 1;
                static int incomeDay2 = 15;
                static bool incomeAfterTax = true;

                ImGui::TextUnformatted("Add income");
                ImGui::InputText("Income name", incomeName, IM_ARRAYSIZE(incomeName));
                ImGui::InputFloat("Income amount", &incomeAmount, 1.0f, 10.0f, "%.2f");
                ImGui::Combo("Income frequency", &incomeFreq, kFrequencies, IM_ARRAYSIZE(kFrequencies));
                ImGui::SliderInt("Income day 1", &incomeDay1, 1, 31);
                ImGui::SliderInt("Income day 2", &incomeDay2, 1, 31);
                ImGui::Checkbox("Income after tax", &incomeAfterTax);
                if (ImGui::Button("Add income item") && incomeName[0] != '\0') {
                    data.incomes.push_back({incomeName, incomeAmount, indexToFrequency(incomeFreq), incomeDay1, incomeDay2, incomeAfterTax});
                    incomeName[0] = '\0';
                    incomeAmount = 0.0f;
                }

                ImGui::Separator();
                static char billName[128] = "";
                static float billAmount = 0.0f;
                static int billFreq = 2;
                static int billDay = 1;
                static bool billEssential = true;
                static bool billAutopay = false;

                ImGui::TextUnformatted("Add bill");
                ImGui::InputText("Bill name", billName, IM_ARRAYSIZE(billName));
                ImGui::InputFloat("Bill amount", &billAmount, 1.0f, 10.0f, "%.2f");
                ImGui::Combo("Bill frequency", &billFreq, kFrequencies, IM_ARRAYSIZE(kFrequencies));
                ImGui::SliderInt("Bill due day", &billDay, 1, 31);
                ImGui::Checkbox("Bill essential", &billEssential);
                ImGui::Checkbox("Bill autopay", &billAutopay);
                if (ImGui::Button("Add bill item") && billName[0] != '\0') {
                    data.bills.push_back({billName, billAmount, indexToFrequency(billFreq), billDay, billEssential, billAutopay});
                    billName[0] = '\0';
                    billAmount = 0.0f;
                }

                ImGui::Separator();
                ImGui::TextUnformatted("Existing categories");
                for (std::size_t i = 0; i < data.variableExpenses.size(); ++i) {
                    auto& x = data.variableExpenses[i];
                    ImGui::PushID(static_cast<int>(i) + 3000);
                    if (ImGui::TreeNode((x.name + "##var").c_str())) {
                        ImGui::InputDouble("Monthly plan", &x.monthlyPlan, 1.0, 10.0, "%.2f");
                        ImGui::InputDouble("Actual", &x.actual, 1.0, 10.0, "%.2f");
                        ImGui::Checkbox("Essential", &x.essential);
                        if (ImGui::Button("Delete category")) {
                            data.variableExpenses.erase(data.variableExpenses.begin() + static_cast<long long>(i));
                            ImGui::TreePop(); ImGui::PopID(); break;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Paycheck Allocations")) {
                for (std::size_t i = 0; i < data.paycheckPlans.size(); ++i) {
                    auto& p = data.paycheckPlans[i];
                    ImGui::PushID(static_cast<int>(i) + 6000);
                    if (ImGui::TreeNode(p.label.c_str())) {
                        ImGui::SliderInt("Start day", &p.startDay, 1, 31);
                        ImGui::SliderInt("End day", &p.endDay, 1, 31);
                        ImGui::Checkbox("Use custom allocation", &p.useCustomAllocation);
                        ImGui::InputDouble("Custom variable budget", &p.customVariableBudget, 10.0, 50.0, "%.2f");
                        ImGui::InputDouble("Custom goal budget", &p.customGoalBudget, 10.0, 50.0, "%.2f");
                        ImGui::InputDouble("Custom tax reserve", &p.customTaxReserve, 10.0, 50.0, "%.2f");
                        ImGui::InputDouble("Custom extra debt", &p.customExtraDebt, 10.0, 50.0, "%.2f");
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Transactions")) {
                static char dateBuf[32] = "2026-04-08";
                static char catBuf[64] = "";
                static char noteBuf[128] = "";
                static float amountBuf = 0.0f;
                static int txType = 0;

                ImGui::InputText("Date", dateBuf, IM_ARRAYSIZE(dateBuf));
                ImGui::InputText("Category", catBuf, IM_ARRAYSIZE(catBuf));
                ImGui::InputText("Note", noteBuf, IM_ARRAYSIZE(noteBuf));
                ImGui::InputFloat("Amount", &amountBuf, 1.0f, 10.0f, "%.2f");
                ImGui::Combo("Type", &txType, kTxTypes, IM_ARRAYSIZE(kTxTypes));
                if (ImGui::Button("Add transaction") && catBuf[0] != '\0') {
                    Transaction t;
                    t.dateLabel = dateBuf;
                    t.category = catBuf;
                    t.note = noteBuf;
                    t.amount = amountBuf;
                    t.type = txType == 1 ? TransactionType::Income : TransactionType::Expense;
                    data.transactions.push_back(t);
                    catBuf[0] = '\0';
                    noteBuf[0] = '\0';
                    amountBuf = 0.0f;
                    txType = 0;
                }
                ImGui::Separator();
                moneyText("Ledger income", summary.ledgerIncomeTotal);
                moneyText("Ledger expense", summary.ledgerExpenseTotal);
                moneyText("Ledger net", summary.ledgerNet);
                ImGui::Separator();
                for (std::size_t i = 0; i < data.transactions.size(); ++i) {
                    auto& t = data.transactions[i];
                    ImGui::PushID(static_cast<int>(i) + 7000);
                    if (ImGui::TreeNode((t.dateLabel + " - " + t.category).c_str())) {
                        ImGui::Text("Type: %s", t.type == TransactionType::Income ? "Income" : "Expense");
                        ImGui::Text("Note: %s", t.note.c_str());
                        ImGui::Text("Amount: $%.2f", t.amount);
                        if (ImGui::Button("Delete transaction")) {
                            data.transactions.erase(data.transactions.begin() + static_cast<long long>(i));
                            ImGui::TreePop(); ImGui::PopID(); break;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Debt Payoff")) {
                moneyText("Total debt minimums", summary.debtPlan.totalMinimums);
                moneyText("Extra debt budget", summary.debtPlan.extraPaymentBudget);
                moneyText("Total planned debt payment", summary.debtPlan.totalPlannedDebtPayment);
                moneyText("Projected monthly interest", summary.debtPlan.projectedMonthlyInterest);
                ImGui::Text("Recommended target: %s", summary.debtPlan.recommendedTarget.empty() ? "None" : summary.debtPlan.recommendedTarget.c_str());
                ImGui::Text("Estimated months to primary payoff: %d", summary.debtPlan.estimatedMonthsToPrimaryPayoff);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Paycheck Templates")) {
                for (std::size_t i = 0; i < data.paycheckTemplates.size(); ++i) {
                    auto& t = data.paycheckTemplates[i];
                    ImGui::PushID(static_cast<int>(i) + 9000);
                    if (ImGui::TreeNode((t.name + "##template").c_str())) {
                        ImGui::InputDouble("Variable budget", &t.variableBudget, 1.0, 10.0, "%.2f");
                        ImGui::InputDouble("Goal budget", &t.goalBudget, 1.0, 10.0, "%.2f");
                        ImGui::InputDouble("Tax reserve", &t.taxReserve, 1.0, 10.0, "%.2f");
                        ImGui::InputDouble("Extra debt", &t.extraDebt, 1.0, 10.0, "%.2f");
                        if (ImGui::Button("Apply to all paycheck windows")) {
                            for (auto& p : data.paycheckPlans) {
                                p.useCustomAllocation = true;
                                p.customVariableBudget = t.variableBudget;
                                p.customGoalBudget = t.goalBudget;
                                p.customTaxReserve = t.taxReserve;
                                p.customExtraDebt = t.extraDebt;
                            }
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }


            if (ImGui::BeginTabItem("Net Worth + Accounts")) {
                const double netWorth = computeNetWorth(data);
                const double liquidAssets = computeLiquidAssets(data);
                moneyText("Net worth", netWorth);
                moneyText("Liquid assets", liquidAssets);
                moneyText("Debt balance", data.debtBalance);

                ImGui::Separator();
                ImGui::TextUnformatted("Net worth trend");
                drawMiniBars("##networthtrend", summary.chartNetWorthTrend, 90.0f);

                static char assetName[128] = "";
                static char assetType[64] = "Cash";
                static float assetBalance = 0.0f;
                ImGui::Separator();
                ImGui::InputText("Asset name", assetName, IM_ARRAYSIZE(assetName));
                ImGui::InputText("Asset type", assetType, IM_ARRAYSIZE(assetType));
                ImGui::InputFloat("Asset balance", &assetBalance, 10.0f, 100.0f, "%.2f");
                if (ImGui::Button("Add asset") && assetName[0] != '\0') {
                    data.accountAssets.push_back({assetName, assetType, assetBalance});
                    assetName[0] = '\0';
                    std::snprintf(assetType, sizeof(assetType), "%s", "Cash");
                    assetBalance = 0.0f;
                }

                ImGui::Separator();
                for (std::size_t i = 0; i < data.accountAssets.size(); ++i) {
                    auto& a = data.accountAssets[i];
                    ImGui::PushID(static_cast<int>(16000 + i));
                    if (ImGui::TreeNode((a.name + "##asset").c_str())) {
                        char typeBuf[64] = "";
                        std::snprintf(typeBuf, sizeof(typeBuf), "%s", a.type.c_str());
                        if (ImGui::InputText("Type", typeBuf, IM_ARRAYSIZE(typeBuf))) a.type = typeBuf;
                        ImGui::InputDouble("Balance", &a.balance, 10.0, 100.0, "%.2f");
                        if (ImGui::Button("Delete asset")) { erase_index(data.accountAssets, i); ImGui::TreePop(); ImGui::PopID(); break; }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("History + Insights")) {
                ImGui::TextUnformatted("Leftover history");
                drawMiniBars("##historyleftover", summary.chartHistoryLeftover, 100.0f);
                ImGui::TextUnformatted("Debt balance history");
                drawMiniBars("##historydebt", summary.chartHistoryDebt, 100.0f);
                ImGui::TextUnformatted("Current category actuals");
                drawMiniBars("##categoryactuals", summary.chartCategoryActuals, 100.0f);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Scenario Planner")) {
                for (const auto& result : summary.scenarioResults) {
                    if (ImGui::TreeNode(result.scenarioName.c_str())) {
                        moneyText("Monthly income", result.monthlyIncome);
                        moneyText("Monthly outflow", result.monthlyOutflow);
                        moneyText("Monthly leftover", result.monthlyLeftover);
                        moneyText("Ending checking", result.endingChecking);
                        moneyText("Ending emergency fund", result.endingEmergencyFund);
                        moneyText("Ending debt", result.endingDebt);
                        ImGui::Text("First negative month: %s", result.firstNegativeMonth < 0 ? "None" : std::to_string(result.firstNegativeMonth).c_str());
                        ImGui::TreePop();
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Multi-Month Forecast")) {
                ImGui::TextUnformatted("Projected checking");
                drawMiniBars("##forecastchecking", summary.chartForecastChecking, 100.0f);
                ImGui::TextUnformatted("Projected debt");
                drawMiniBars("##forecastdebt", summary.chartForecastDebt, 100.0f);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Daily Cashflow")) {
                std::vector<double> balances;
                for (const auto& p : summary.dailyBalance) balances.push_back(p.balance);
                drawMiniBars("##dailybal", balances, 120.0f);
                for (const auto& p : summary.dailyBalance) {
                    ImGui::BulletText("Day %d | Balance $%.2f | %s", p.day, p.balance, p.eventLabel.c_str());
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Optimization + Alerts")) {
                for (const auto& opt : summary.optimizations) ImGui::BulletText("%s (impact $%.2f / month)", opt.action.c_str(), opt.impact);
                ImGui::Separator();
                for (const auto& a : summary.alerts) ImGui::BulletText("[%s] %s", a.severity.c_str(), a.message.c_str());
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Bank Import + Automation")) {
                static char csvPath[260] = "";
                ImGui::InputText("CSV file path", csvPath, IM_ARRAYSIZE(csvPath));
                if (ImGui::Button("Import CSV")) {
                    const int added = importCsvTransactions(data, csvPath);
                    if (added >= 0) std::snprintf(saveNotice, sizeof(saveNotice), "Imported %d transactions", added);
                    else std::snprintf(saveNotice, sizeof(saveNotice), "CSV import failed");
                }

                static char merchantBuf[128] = "";
                static char categoryBuf[128] = "";
                ImGui::Separator();
                ImGui::InputText("Merchant contains", merchantBuf, IM_ARRAYSIZE(merchantBuf));
                ImGui::InputText("Category", categoryBuf, IM_ARRAYSIZE(categoryBuf));
                if (ImGui::Button("Add rule") && merchantBuf[0] != '\0' && categoryBuf[0] != '\0') {
                    data.autoCategoryRules.push_back({merchantBuf, categoryBuf});
                    merchantBuf[0] = '\0';
                    categoryBuf[0] = '\0';
                }

                ImGui::Separator();
                for (const auto& r : data.autoCategoryRules) {
                    ImGui::BulletText("%s -> %s", r.merchantContains.c_str(), r.category.c_str());
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Decision Simulator")) {
                static float carPayment = 450.0f;
                static float insuranceIncrease = 110.0f;
                static float gasIncrease = 80.0f;
                static float purchaseShock = 1500.0f;
                static float rentIncrease = 0.0f;
                static float incomeChange = 0.0f;

                ImGui::TextWrapped("Simulate car purchases, rent changes, and other major decisions.");
                ImGui::InputFloat("Car payment", &carPayment, 10.0f, 50.0f, "%.2f");
                ImGui::InputFloat("Insurance increase", &insuranceIncrease, 10.0f, 50.0f, "%.2f");
                ImGui::InputFloat("Gas increase", &gasIncrease, 10.0f, 50.0f, "%.2f");
                ImGui::InputFloat("One-time purchase shock", &purchaseShock, 50.0f, 100.0f, "%.2f");
                ImGui::InputFloat("Rent increase", &rentIncrease, 10.0f, 50.0f, "%.2f");
                ImGui::InputFloat("Monthly income change", &incomeChange, 10.0f, 50.0f, "%.2f");

                const double adjustedIncome = summary.monthlyIncome + incomeChange;
                const double adjustedOutflow = summary.monthlyPlannedOutflow + carPayment + insuranceIncrease + gasIncrease + rentIncrease + purchaseShock;
                const double adjustedLeftover = adjustedIncome - adjustedOutflow;
                const double twelveMonthImpact = adjustedLeftover * 12.0;

                ImGui::Separator();
                moneyText("Adjusted monthly income", adjustedIncome);
                moneyText("Adjusted monthly outflow", adjustedOutflow);
                moneyText("Adjusted monthly leftover", adjustedLeftover);
                moneyText("12-month cumulative impact", twelveMonthImpact);

                if (adjustedLeftover < 0.0) {
                    ImGui::TextWrapped("Result: this decision makes the monthly plan negative.");
                } else {
                    ImGui::TextWrapped("Result: this decision stays positive, but review forecast and alerts before committing.");
                }
                ImGui::EndTabItem();
            }


            if (ImGui::BeginTabItem("Auto Budget")) {
                ImGui::TextWrapped("Adaptive zero-based style allocation generated from your current numbers.");
                ImGui::Separator();
                for (const auto& row : autoBudget) {
                    moneyText(row.bucket.c_str(), row.amount);
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Goals Intelligence")) {
                ImGui::TextWrapped("Goal projections estimate how long each goal takes and what contribution reaches it faster.");
                ImGui::Separator();
                for (const auto& g : goalProjections) {
                    if (ImGui::TreeNode(g.goalName.c_str())) {
                        moneyText("Monthly contribution", g.monthlyContribution);
                        moneyText("Current balance", g.currentBalance);
                        moneyText("Target balance", g.targetBalance);
                        ImGui::Text("Months to goal: %.2f", g.monthsToGoal);
                        moneyText("Contribution for ~2 months sooner", g.contributionForTwoMonthsSooner);
                        ImGui::TreePop();
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Risk + Alerts")) {
                ImGui::TextWrapped("Higher-level risk engine signals layered on top of your existing alert system.");
                ImGui::Separator();
                for (const auto& r : riskSignals) {
                    ImGui::BulletText("[%s] %s", r.severity.c_str(), r.summary.c_str());
                }
                ImGui::Separator();
                ImGui::TextUnformatted("Existing alerts");
                for (const auto& a : summary.alerts) {
                    ImGui::BulletText("[%s] %s", a.severity.c_str(), a.message.c_str());
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Automation Rules")) {
                ImGui::TextWrapped("Rule ideas for automatic handling of paychecks, low balance states, and debt routing.");
                ImGui::Separator();
                for (const auto& rule : automationRules) {
                    if (ImGui::TreeNode(rule.name.c_str())) {
                        ImGui::Text("Enabled: %s", rule.enabled ? "Yes" : "No");
                        ImGui::TextWrapped("%s", rule.description.c_str());
                        ImGui::TreePop();
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Life Planner")) {
                static float lifeMoveCost = 2500.0f;
                static float lifeRentDelta = 200.0f;
                static float lifeIncomeDelta = 0.0f;
                static float lifeUtilityDelta = 75.0f;

                ImGui::TextWrapped("Model a broader life change like moving, rent increase, or job change.");
                ImGui::InputFloat("One-time move cost", &lifeMoveCost, 50.0f, 100.0f, "%.2f");
                ImGui::InputFloat("Monthly rent delta", &lifeRentDelta, 10.0f, 50.0f, "%.2f");
                ImGui::InputFloat("Monthly income delta", &lifeIncomeDelta, 10.0f, 50.0f, "%.2f");
                ImGui::InputFloat("Monthly utilities delta", &lifeUtilityDelta, 10.0f, 50.0f, "%.2f");

                const double adjustedIncome = summary.monthlyIncome + lifeIncomeDelta;
                const double adjustedOutflow = summary.monthlyPlannedOutflow + lifeMoveCost + lifeRentDelta + lifeUtilityDelta;
                const double adjustedLeftover = adjustedIncome - adjustedOutflow;

                ImGui::Separator();
                moneyText("Adjusted monthly income", adjustedIncome);
                moneyText("Adjusted monthly outflow", adjustedOutflow);
                moneyText("Adjusted monthly leftover", adjustedLeftover);
                ImGui::TextWrapped(adjustedLeftover >= 0.0
                    ? "This life change remains cashflow-positive under the current plan."
                    : "This life change breaks the monthly plan unless other changes are made.");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Advice")) {
                for (const auto& line : advice) ImGui::BulletText("%s", line.c_str());
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();

        ImGui::Render();
        int displayW = 0, displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    std::string ignored;
    storage.save(data, ignored);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

} // namespace budget
