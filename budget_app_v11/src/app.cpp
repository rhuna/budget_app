#include "app.hpp"
#include "advice_engine.hpp"
#include "financial_engine.hpp"
#include "automation_engine.hpp"
#include "risk_engine.hpp"
#include "execution_engine.hpp"

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
#include <cstddef>

namespace budget {
namespace {

const char* kMonths[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
const char* kFrequencies[] = {"Weekly","Bi-Weekly","Monthly","Quarterly","Yearly"};
const char* kTxTypes[] = {"Expense", "Income"};
const char* kDebtStrategies[] = {"Avalanche", "Snowball"};

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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 12));
    ImGui::Text("%s", label);
    ImGui::SameLine(450.0f);
    ImGui::Text("$%.2f", value);
    ImGui::PopStyleVar();
}

bool bigButton(const char* label) {
    return ImGui::Button(label, ImVec2(170, 42));
}

void summaryCard(const char* title, const char* value) {
    ImGui::BeginChild(title, ImVec2(320, 120), true);
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
    bool headerHandled = false;
    int added = 0;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto parts = splitCsvBasic(line);
        if (parts.size() < 3) continue;

        if (!headerHandled) {
            if (parts[0].find("date") != std::string::npos || parts[0].find("Date") != std::string::npos) {
                headerHandled = true;
                continue;
            }
            headerHandled = true;
        }

        Transaction t;
        t.dateLabel = parts[0];
        t.note = parts[1];
        t.category = parts.size() >= 4 && !parts[3].empty() ? parts[3] : "Imported";

        double amount = 0.0;
        try { amount = std::stod(parts[2]); } catch (...) { continue; }

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

template <typename T>
void erase_index(std::vector<T>& v, std::size_t i) {
    v.erase(v.begin() + static_cast<long long>(i));
}

double computeNetWorth(const BudgetData& data) {
    double assets = data.checkingBalance + data.emergencyFundBalance;
    for (const auto& g : data.goals) assets += g.currentBalance;
    for (const auto& a : data.accountAssets) assets += a.balance;
    return assets - data.debtBalance;
}

double computeLiquidAssets(const BudgetData& data) {
    double assets = data.checkingBalance + data.emergencyFundBalance;
    for (const auto& a : data.accountAssets) {
        if (a.type == "Cash" || a.type == "Savings") assets += a.balance;
    }
    return assets;
}

template <typename T>
bool begin_editor_node(const std::string& prefix, const std::string& name, std::size_t i) {
    ImGui::PushID(static_cast<int>(i));
    return ImGui::TreeNode((prefix + name).c_str());
}

void end_editor_node() {
    ImGui::TreePop();
    ImGui::PopID();
}

} // namespace

void runApp(Storage& storage) {
    BudgetData data;
    std::string error;
    storage.load(data, error);

    if (!glfwInit()) return;
    GLFWwindow* window = glfwCreateWindow(1620, 1040, "BudgetPilot v21 UI Overhaul", nullptr, nullptr);
    if (!window) { glfwTerminate(); return; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    char notice[256] = "";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ensurePaycheckPlans(data);

        BudgetSummary summary = buildSummary(data);
        auto advice = buildAdvice(data, summary);
        auto autoBudget = buildAutoBudget(data, summary);
        auto goalProjections = buildGoalProjections(data);
        auto automationRules = buildDefaultAutomationRules(data, summary);
        auto riskSignals = buildRiskSignals(data, summary);
        auto executionPlan = buildExecutionPlan(data, summary);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiIO& io = ImGui::GetIO();
        const float scale = data.settings.uiScale;
        io.FontGlobalScale = scale;
        style.FramePadding = ImVec2(10.0f * scale, 8.0f * scale);
        style.ItemSpacing = ImVec2(10.0f * scale, 10.0f * scale);
        style.WindowPadding = ImVec2(12.0f * scale, 12.0f * scale);
        style.CellPadding = ImVec2(8.0f * scale, 6.0f * scale);
        style.ScrollbarSize = 16.0f * scale;
        style.GrabMinSize = 14.0f * scale;

        ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(1750, 1120), ImGuiCond_Once);
        ImGui::Begin("BudgetPilot");

        if (bigButton("Save")) {
            std::string saveError;
            if (storage.save(data, saveError)) std::snprintf(notice, sizeof(notice), "Saved to data/budget_data.txt");
            else std::snprintf(notice, sizeof(notice), "Save failed: %s", saveError.c_str());
        }
        ImGui::SameLine();
        if (bigButton("Reset Sample Data")) {
            data = makeSampleData();
            std::snprintf(notice, sizeof(notice), "Reset to sample data");
        }
        ImGui::SameLine();
        if (bigButton("Close Out This Month")) {
            addCurrentMonthSnapshot(data, summary);
            std::snprintf(notice, sizeof(notice), "Month snapshot saved");
        }
        ImGui::SameLine();
        if (bigButton("Re-run Auto Categorization")) {
            applyAutoCategorization(data);
            std::snprintf(notice, sizeof(notice), "Auto categorization applied");
        }
        ImGui::SameLine();
        ImGui::TextWrapped("%s", notice);

        ImGui::Separator();
        int monthIndex = data.settings.month - 1;
        if (monthIndex < 0) monthIndex = 0;
        if (monthIndex > 11) monthIndex = 11;
        if (ImGui::Combo("Month", &monthIndex, kMonths, 12)) data.settings.month = monthIndex + 1;
        ImGui::InputInt("Year", &data.settings.year);
        ImGui::SliderFloat("Zoom / UI scale", &data.settings.uiScale, 1.0f, 3.5f, "%.2fx");
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
                moneyText("Monthly goals", summary.monthlyGoals);
                moneyText("Monthly one-time costs", summary.monthlyOneTime);
                moneyText("Monthly tax reserve", summary.monthlyTaxReserve);
                ImGui::Separator();
                moneyText("Planned total outflow", summary.monthlyPlannedOutflow);
                moneyText("Actual total outflow", summary.monthlyActualOutflow);
                moneyText("Planned leftover", summary.monthlyPlannedLeftover);
                moneyText("Actual leftover", summary.monthlyActualLeftover);
                ImGui::TextUnformatted("Plan vs actual trend");
                drawMiniBars("##planactual", summary.chartPlanVsActual, 95.0f);
                ImGui::TextUnformatted("Paycheck leftover trend");
                drawMiniBars("##payleftover", summary.chartPaycheckLeftovers, 95.0f);
                for (const auto& line : summary.upcomingBills) ImGui::BulletText("%s", line.c_str());
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Calendar Overview")) {
                if (ImGui::BeginTable("CalendarTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    int dayIndex = 0;
                    for (int row = 0; row < 6; ++row) {
                        ImGui::TableNextRow();
                        for (int c = 0; c < 7; ++c) {
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

            if (ImGui::BeginTabItem("Full Editors")) {
                ImGui::TextWrapped("This tab is the full editability pass. Every major data set has add/edit/delete support here.");
                ImGui::Separator();

                if (ImGui::CollapsingHeader("Incomes", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static char addName[128] = "";
                    static float addAmount = 0.0f;
                    static int addFreq = 1;
                    static int addDay1 = 1;
                    static int addDay2 = 15;
                    static bool addAfterTax = true;
                    ImGui::InputText("Income name", addName, IM_ARRAYSIZE(addName));
                    ImGui::InputFloat("Income amount", &addAmount, 1.0f, 10.0f, "%.2f");
                    ImGui::Combo("Income frequency", &addFreq, kFrequencies, IM_ARRAYSIZE(kFrequencies));
                    ImGui::SliderInt("Income day 1", &addDay1, 1, 31);
                    ImGui::SliderInt("Income day 2", &addDay2, 1, 31);
                    ImGui::Checkbox("Income after tax", &addAfterTax);
                    if (ImGui::Button("Add income") && addName[0] != '\0') {
                        data.incomes.push_back({addName, addAmount, indexToFrequency(addFreq), addDay1, addDay2, addAfterTax});
                        addName[0] = '\0'; addAmount = 0.0f; addFreq = 1; addDay1 = 1; addDay2 = 15; addAfterTax = true;
                    }
                    for (std::size_t i = 0; i < data.incomes.size(); ++i) {
                        auto& x = data.incomes[i];
                        if (begin_editor_node<IncomeSource>("Income: ", x.name, i)) {
                            char nameBuf[128] = "";
                            std::snprintf(nameBuf, sizeof(nameBuf), "%s", x.name.c_str());
                            if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) x.name = nameBuf;
                            ImGui::InputDouble("Amount", &x.amount, 1.0, 10.0, "%.2f");
                            int idx = frequencyToIndex(x.frequency);
                            if (ImGui::Combo("Frequency", &idx, kFrequencies, IM_ARRAYSIZE(kFrequencies))) x.frequency = indexToFrequency(idx);
                            ImGui::SliderInt("Pay day 1", &x.payDay1, 1, 31);
                            ImGui::SliderInt("Pay day 2", &x.payDay2, 1, 31);
                            ImGui::Checkbox("After tax", &x.afterTax);
                            if (ImGui::Button("Delete income")) { erase_index(data.incomes, i); end_editor_node(); break; }
                            end_editor_node();
                        } else ImGui::PopID();
                    }
                }

                if (ImGui::CollapsingHeader("Bills", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static char addName[128] = "";
                    static float addAmount = 0.0f;
                    static int addFreq = 2;
                    static int addDay = 1;
                    static bool addEssential = true;
                    static bool addAutopay = false;
                    ImGui::InputText("Bill name", addName, IM_ARRAYSIZE(addName));
                    ImGui::InputFloat("Bill amount", &addAmount, 1.0f, 10.0f, "%.2f");
                    ImGui::Combo("Bill frequency", &addFreq, kFrequencies, IM_ARRAYSIZE(kFrequencies));
                    ImGui::SliderInt("Bill due day", &addDay, 1, 31);
                    ImGui::Checkbox("Bill essential", &addEssential);
                    ImGui::Checkbox("Bill autopay", &addAutopay);
                    if (ImGui::Button("Add bill") && addName[0] != '\0') {
                        data.bills.push_back({addName, addAmount, indexToFrequency(addFreq), addDay, addEssential, addAutopay});
                        addName[0] = '\0'; addAmount = 0.0f;
                    }
                    for (std::size_t i = 0; i < data.bills.size(); ++i) {
                        auto& x = data.bills[i];
                        if (begin_editor_node<Bill>("Bill: ", x.name, i)) {
                            char nameBuf[128] = "";
                            std::snprintf(nameBuf, sizeof(nameBuf), "%s", x.name.c_str());
                            if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) x.name = nameBuf;
                            ImGui::InputDouble("Amount", &x.amount, 1.0, 10.0, "%.2f");
                            int idx = frequencyToIndex(x.frequency);
                            if (ImGui::Combo("Frequency", &idx, kFrequencies, IM_ARRAYSIZE(kFrequencies))) x.frequency = indexToFrequency(idx);
                            ImGui::SliderInt("Due day", &x.dueDay, 1, 31);
                            ImGui::Checkbox("Essential", &x.essential);
                            ImGui::Checkbox("Autopay", &x.autopay);
                            if (ImGui::Button("Delete bill")) { erase_index(data.bills, i); end_editor_node(); break; }
                            end_editor_node();
                        } else ImGui::PopID();
                    }
                }

                if (ImGui::CollapsingHeader("Variable Categories", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static char addName[128] = "";
                    static float addPlan = 0.0f;
                    static float addActual = 0.0f;
                    static bool addEssential = false;
                    ImGui::InputText("Category name", addName, IM_ARRAYSIZE(addName));
                    ImGui::InputFloat("Monthly plan", &addPlan, 1.0f, 10.0f, "%.2f");
                    ImGui::InputFloat("Actual", &addActual, 1.0f, 10.0f, "%.2f");
                    ImGui::Checkbox("Category essential", &addEssential);
                    if (ImGui::Button("Add category") && addName[0] != '\0') {
                        data.variableExpenses.push_back({addName, addPlan, addActual, addEssential});
                        addName[0] = '\0'; addPlan = 0.0f; addActual = 0.0f; addEssential = false;
                    }
                    for (std::size_t i = 0; i < data.variableExpenses.size(); ++i) {
                        auto& x = data.variableExpenses[i];
                        if (begin_editor_node<VariableExpense>("Category: ", x.name, i)) {
                            char nameBuf[128] = "";
                            std::snprintf(nameBuf, sizeof(nameBuf), "%s", x.name.c_str());
                            if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) x.name = nameBuf;
                            ImGui::InputDouble("Monthly plan", &x.monthlyPlan, 1.0, 10.0, "%.2f");
                            ImGui::InputDouble("Actual", &x.actual, 1.0, 10.0, "%.2f");
                            ImGui::Checkbox("Essential", &x.essential);
                            if (ImGui::Button("Delete category")) { erase_index(data.variableExpenses, i); end_editor_node(); break; }
                            end_editor_node();
                        } else ImGui::PopID();
                    }
                }

                if (ImGui::CollapsingHeader("Goals", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static char addName[128] = "";
                    static float addMonthly = 0.0f;
                    static float addCurrent = 0.0f;
                    static float addTarget = 0.0f;
                    ImGui::InputText("Goal name", addName, IM_ARRAYSIZE(addName));
                    ImGui::InputFloat("Goal monthly contribution", &addMonthly, 1.0f, 10.0f, "%.2f");
                    ImGui::InputFloat("Goal current balance", &addCurrent, 1.0f, 10.0f, "%.2f");
                    ImGui::InputFloat("Goal target balance", &addTarget, 1.0f, 10.0f, "%.2f");
                    if (ImGui::Button("Add goal") && addName[0] != '\0') {
                        data.goals.push_back({addName, addMonthly, addCurrent, addTarget});
                        addName[0] = '\0'; addMonthly = addCurrent = addTarget = 0.0f;
                    }
                    for (std::size_t i = 0; i < data.goals.size(); ++i) {
                        auto& x = data.goals[i];
                        if (begin_editor_node<GoalBucket>("Goal: ", x.name, i)) {
                            char nameBuf[128] = "";
                            std::snprintf(nameBuf, sizeof(nameBuf), "%s", x.name.c_str());
                            if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) x.name = nameBuf;
                            ImGui::InputDouble("Monthly contribution", &x.monthlyContribution, 1.0, 10.0, "%.2f");
                            ImGui::InputDouble("Current balance", &x.currentBalance, 1.0, 10.0, "%.2f");
                            ImGui::InputDouble("Target balance", &x.targetBalance, 1.0, 10.0, "%.2f");
                            if (ImGui::Button("Delete goal")) { erase_index(data.goals, i); end_editor_node(); break; }
                            end_editor_node();
                        } else ImGui::PopID();
                    }
                }

                if (ImGui::CollapsingHeader("One-Time Expenses", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static char addName[128] = "";
                    static float addAmount = 0.0f;
                    static int addDay = 1;
                    static bool addActive = true;
                    ImGui::InputText("One-time name", addName, IM_ARRAYSIZE(addName));
                    ImGui::InputFloat("One-time amount", &addAmount, 1.0f, 10.0f, "%.2f");
                    ImGui::SliderInt("One-time due day", &addDay, 1, 31);
                    ImGui::Checkbox("One-time active", &addActive);
                    if (ImGui::Button("Add one-time") && addName[0] != '\0') {
                        data.oneTimeExpenses.push_back({addName, addAmount, addDay, addActive});
                        addName[0] = '\0'; addAmount = 0.0f; addDay = 1; addActive = true;
                    }
                    for (std::size_t i = 0; i < data.oneTimeExpenses.size(); ++i) {
                        auto& x = data.oneTimeExpenses[i];
                        if (begin_editor_node<OneTimeExpense>("One-time: ", x.name, i)) {
                            char nameBuf[128] = "";
                            std::snprintf(nameBuf, sizeof(nameBuf), "%s", x.name.c_str());
                            if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) x.name = nameBuf;
                            ImGui::InputDouble("Amount", &x.amount, 1.0, 10.0, "%.2f");
                            ImGui::SliderInt("Due day", &x.dueDay, 1, 31);
                            ImGui::Checkbox("Active", &x.active);
                            if (ImGui::Button("Delete one-time")) { erase_index(data.oneTimeExpenses, i); end_editor_node(); break; }
                            end_editor_node();
                        } else ImGui::PopID();
                    }
                }

                if (ImGui::CollapsingHeader("Debt Accounts", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static char addName[128] = "";
                    static float addBalance = 0.0f;
                    static float addApr = 0.0f;
                    static float addMin = 0.0f;
                    ImGui::InputText("Debt name", addName, IM_ARRAYSIZE(addName));
                    ImGui::InputFloat("Debt balance", &addBalance, 1.0f, 10.0f, "%.2f");
                    ImGui::InputFloat("Debt APR", &addApr, 0.1f, 1.0f, "%.2f");
                    ImGui::InputFloat("Debt minimum", &addMin, 1.0f, 10.0f, "%.2f");
                    if (ImGui::Button("Add debt account") && addName[0] != '\0') {
                        data.debtAccounts.push_back({addName, addBalance, addApr, addMin});
                        addName[0] = '\0'; addBalance = addApr = addMin = 0.0f;
                    }
                    for (std::size_t i = 0; i < data.debtAccounts.size(); ++i) {
                        auto& d = data.debtAccounts[i];
                        if (begin_editor_node<DebtAccount>("Debt: ", d.name, i)) {
                            char nameBuf[128] = "";
                            std::snprintf(nameBuf, sizeof(nameBuf), "%s", d.name.c_str());
                            if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) d.name = nameBuf;
                            ImGui::InputDouble("Balance", &d.balance, 1.0, 10.0, "%.2f");
                            ImGui::InputDouble("APR", &d.apr, 0.1, 1.0, "%.2f");
                            ImGui::InputDouble("Minimum payment", &d.minimumPayment, 1.0, 10.0, "%.2f");
                            if (ImGui::Button("Delete debt")) { erase_index(data.debtAccounts, i); end_editor_node(); break; }
                            end_editor_node();
                        } else ImGui::PopID();
                    }
                }

                if (ImGui::CollapsingHeader("Paycheck Plans", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (std::size_t i = 0; i < data.paycheckPlans.size(); ++i) {
                        auto& p = data.paycheckPlans[i];
                        if (begin_editor_node<PaycheckPlan>("Plan: ", p.label, i)) {
                            char labelBuf[128] = "";
                            std::snprintf(labelBuf, sizeof(labelBuf), "%s", p.label.c_str());
                            if (ImGui::InputText("Label", labelBuf, IM_ARRAYSIZE(labelBuf))) p.label = labelBuf;
                            ImGui::SliderInt("Start day", &p.startDay, 1, 31);
                            ImGui::SliderInt("End day", &p.endDay, 1, 31);
                            ImGui::Checkbox("Use custom allocation", &p.useCustomAllocation);
                            ImGui::InputDouble("Custom variable budget", &p.customVariableBudget, 10.0, 50.0, "%.2f");
                            ImGui::InputDouble("Custom goal budget", &p.customGoalBudget, 10.0, 50.0, "%.2f");
                            ImGui::InputDouble("Custom tax reserve", &p.customTaxReserve, 10.0, 50.0, "%.2f");
                            ImGui::InputDouble("Custom extra debt", &p.customExtraDebt, 10.0, 50.0, "%.2f");
                            if (ImGui::Button("Delete paycheck plan")) { erase_index(data.paycheckPlans, i); end_editor_node(); break; }
                            end_editor_node();
                        } else ImGui::PopID();
                    }
                }

                if (ImGui::CollapsingHeader("Paycheck Templates", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static char addName[128] = "";
                    static float addVar = 0.0f, addGoal = 0.0f, addTax = 0.0f, addDebt = 0.0f;
                    ImGui::InputText("Template name", addName, IM_ARRAYSIZE(addName));
                    ImGui::InputFloat("Template variable", &addVar, 1.0f, 10.0f, "%.2f");
                    ImGui::InputFloat("Template goal", &addGoal, 1.0f, 10.0f, "%.2f");
                    ImGui::InputFloat("Template tax", &addTax, 1.0f, 10.0f, "%.2f");
                    ImGui::InputFloat("Template debt", &addDebt, 1.0f, 10.0f, "%.2f");
                    if (ImGui::Button("Add template") && addName[0] != '\0') {
                        data.paycheckTemplates.push_back({addName, addVar, addGoal, addTax, addDebt});
                        addName[0] = '\0'; addVar = addGoal = addTax = addDebt = 0.0f;
                    }
                    for (std::size_t i = 0; i < data.paycheckTemplates.size(); ++i) {
                        auto& t = data.paycheckTemplates[i];
                        if (begin_editor_node<PaycheckTemplate>("Template: ", t.name, i)) {
                            char nameBuf[128] = "";
                            std::snprintf(nameBuf, sizeof(nameBuf), "%s", t.name.c_str());
                            if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) t.name = nameBuf;
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
                            if (ImGui::Button("Delete template")) { erase_index(data.paycheckTemplates, i); end_editor_node(); break; }
                            end_editor_node();
                        } else ImGui::PopID();
                    }
                }

                if (ImGui::CollapsingHeader("Scenarios", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static char addName[128] = "";
                    static float inMul = 1.0f, billMul = 1.0f, varMul = 1.0f, addInc = 0.0f, addBills = 0.0f, shock = 0.0f;
                    static bool active = false;
                    ImGui::InputText("Scenario name", addName, IM_ARRAYSIZE(addName));
                    ImGui::InputFloat("Income multiplier", &inMul, 0.01f, 0.05f, "%.2f");
                    ImGui::InputFloat("Bill multiplier", &billMul, 0.01f, 0.05f, "%.2f");
                    ImGui::InputFloat("Variable multiplier", &varMul, 0.01f, 0.05f, "%.2f");
                    ImGui::InputFloat("Extra monthly income", &addInc, 10.0f, 50.0f, "%.2f");
                    ImGui::InputFloat("Extra monthly bills", &addBills, 10.0f, 50.0f, "%.2f");
                    ImGui::InputFloat("One-time shock", &shock, 10.0f, 100.0f, "%.2f");
                    ImGui::Checkbox("Scenario active", &active);
                    if (ImGui::Button("Add scenario") && addName[0] != '\0') {
                        data.scenarios.push_back({addName, inMul, billMul, varMul, addInc, addBills, shock, active});
                        addName[0] = '\0'; inMul = billMul = varMul = 1.0f; addInc = addBills = shock = 0.0f; active = false;
                    }
                    for (std::size_t i = 0; i < data.scenarios.size(); ++i) {
                        auto& s = data.scenarios[i];
                        if (begin_editor_node<Scenario>("Scenario: ", s.name, i)) {
                            char nameBuf[128] = "";
                            std::snprintf(nameBuf, sizeof(nameBuf), "%s", s.name.c_str());
                            if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) s.name = nameBuf;
                            ImGui::Checkbox("Active", &s.active);
                            ImGui::InputDouble("Income multiplier", &s.incomeMultiplier, 0.01, 0.05, "%.2f");
                            ImGui::InputDouble("Bill multiplier", &s.billMultiplier, 0.01, 0.05, "%.2f");
                            ImGui::InputDouble("Variable multiplier", &s.variableMultiplier, 0.01, 0.05, "%.2f");
                            ImGui::InputDouble("Extra monthly income", &s.extraMonthlyIncome, 10.0, 50.0, "%.2f");
                            ImGui::InputDouble("Extra monthly bills", &s.extraMonthlyBills, 10.0, 50.0, "%.2f");
                            ImGui::InputDouble("One-time shock", &s.oneTimeShock, 10.0, 100.0, "%.2f");
                            if (ImGui::Button("Delete scenario")) { erase_index(data.scenarios, i); end_editor_node(); break; }
                            end_editor_node();
                        } else ImGui::PopID();
                    }
                }

                if (ImGui::CollapsingHeader("Automation Rules / Category Rules", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static char merchant[128] = "";
                    static char category[128] = "";
                    ImGui::InputText("Merchant contains", merchant, IM_ARRAYSIZE(merchant));
                    ImGui::InputText("Category", category, IM_ARRAYSIZE(category));
                    if (ImGui::Button("Add category rule") && merchant[0] != '\0' && category[0] != '\0') {
                        data.autoCategoryRules.push_back({merchant, category});
                        merchant[0] = '\0'; category[0] = '\0';
                    }
                    for (std::size_t i = 0; i < data.autoCategoryRules.size(); ++i) {
                        auto& r = data.autoCategoryRules[i];
                        if (begin_editor_node<AutoCategoryRule>("Rule: ", r.merchantContains, i)) {
                            char merchantBuf[128] = "";
                            char categoryBuf[128] = "";
                            std::snprintf(merchantBuf, sizeof(merchantBuf), "%s", r.merchantContains.c_str());
                            std::snprintf(categoryBuf, sizeof(categoryBuf), "%s", r.category.c_str());
                            if (ImGui::InputText("Merchant text", merchantBuf, IM_ARRAYSIZE(merchantBuf))) r.merchantContains = merchantBuf;
                            if (ImGui::InputText("Mapped category", categoryBuf, IM_ARRAYSIZE(categoryBuf))) r.category = categoryBuf;
                            if (ImGui::Button("Delete rule")) { erase_index(data.autoCategoryRules, i); end_editor_node(); break; }
                            end_editor_node();
                        } else ImGui::PopID();
                    }
                }

                if (ImGui::CollapsingHeader("Account Assets", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static char addName[128] = "";
                    static char addType[64] = "Cash";
                    static float addBal = 0.0f;
                    ImGui::InputText("Asset name", addName, IM_ARRAYSIZE(addName));
                    ImGui::InputText("Asset type", addType, IM_ARRAYSIZE(addType));
                    ImGui::InputFloat("Asset balance", &addBal, 10.0f, 100.0f, "%.2f");
                    if (ImGui::Button("Add asset") && addName[0] != '\0') {
                        data.accountAssets.push_back({addName, addType, addBal});
                        addName[0] = '\0'; std::snprintf(addType, sizeof(addType), "%s", "Cash"); addBal = 0.0f;
                    }
                    for (std::size_t i = 0; i < data.accountAssets.size(); ++i) {
                        auto& a = data.accountAssets[i];
                        if (begin_editor_node<AccountAsset>("Asset: ", a.name, i)) {
                            char nameBuf[128] = "";
                            char typeBuf[64] = "";
                            std::snprintf(nameBuf, sizeof(nameBuf), "%s", a.name.c_str());
                            std::snprintf(typeBuf, sizeof(typeBuf), "%s", a.type.c_str());
                            if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) a.name = nameBuf;
                            if (ImGui::InputText("Type", typeBuf, IM_ARRAYSIZE(typeBuf))) a.type = typeBuf;
                            ImGui::InputDouble("Balance", &a.balance, 10.0, 100.0, "%.2f");
                            if (ImGui::Button("Delete asset")) { erase_index(data.accountAssets, i); end_editor_node(); break; }
                            end_editor_node();
                        } else ImGui::PopID();
                    }
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
                    catBuf[0] = '\0'; noteBuf[0] = '\0'; amountBuf = 0.0f; txType = 0;
                }
                ImGui::Separator();
                moneyText("Ledger income", summary.ledgerIncomeTotal);
                moneyText("Ledger expense", summary.ledgerExpenseTotal);
                moneyText("Ledger net", summary.ledgerNet);
                ImGui::Separator();
                for (std::size_t i = 0; i < data.transactions.size(); ++i) {
                    auto& t = data.transactions[i];
                    if (begin_editor_node<Transaction>("Txn: ", t.dateLabel + " - " + t.category, i)) {
                        char dateEdit[32] = "";
                        char catEdit[64] = "";
                        char noteEdit[128] = "";
                        std::snprintf(dateEdit, sizeof(dateEdit), "%s", t.dateLabel.c_str());
                        std::snprintf(catEdit, sizeof(catEdit), "%s", t.category.c_str());
                        std::snprintf(noteEdit, sizeof(noteEdit), "%s", t.note.c_str());
                        if (ImGui::InputText("Date", dateEdit, IM_ARRAYSIZE(dateEdit))) t.dateLabel = dateEdit;
                        if (ImGui::InputText("Category", catEdit, IM_ARRAYSIZE(catEdit))) t.category = catEdit;
                        if (ImGui::InputText("Note", noteEdit, IM_ARRAYSIZE(noteEdit))) t.note = noteEdit;
                        ImGui::InputDouble("Amount", &t.amount, 1.0, 10.0, "%.2f");
                        int typeIdx = t.type == TransactionType::Income ? 1 : 0;
                        if (ImGui::Combo("Type", &typeIdx, kTxTypes, IM_ARRAYSIZE(kTxTypes))) {
                            t.type = typeIdx == 1 ? TransactionType::Income : TransactionType::Expense;
                        }
                        if (ImGui::Button("Delete transaction")) { erase_index(data.transactions, i); end_editor_node(); break; }
                        end_editor_node();
                    } else ImGui::PopID();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Net Worth + Accounts")) {
                const double netWorth = computeNetWorth(data);
                const double liquidAssets = computeLiquidAssets(data);
                moneyText("Net worth", netWorth);
                moneyText("Liquid assets", liquidAssets);
                moneyText("Debt balance", data.debtBalance);
                ImGui::TextUnformatted("Net worth trend");
                drawMiniBars("##networthtrend", summary.chartNetWorthTrend, 90.0f);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("History + Insights")) {
                drawMiniBars("##historyleftover", summary.chartHistoryLeftover, 90.0f);
                drawMiniBars("##historydebt", summary.chartHistoryDebt, 90.0f);
                drawMiniBars("##categoryactuals", summary.chartCategoryActuals, 90.0f);
                for (const auto& h : data.history) {
                    ImGui::BulletText("%s | income $%.2f | outflow $%.2f | leftover $%.2f", h.label.c_str(), h.income, h.outflow, h.leftover);
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Scenario Planner")) {
                if (summary.scenarioResults.empty()) {
                    ImGui::TextWrapped("No scenarios are available yet. Add or edit them in Full Editors.");
                } else {
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
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Multi-Month Forecast")) {
                drawMiniBars("##forecastchecking", summary.chartForecastChecking, 90.0f);
                drawMiniBars("##forecastdebt", summary.chartForecastDebt, 90.0f);
                for (const auto& month : summary.forecast) {
                    ImGui::BulletText("%s | checking $%.2f | debt $%.2f | leftover $%.2f", month.label.c_str(), month.projectedChecking, month.projectedDebt, month.projectedLeftover);
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Daily Cashflow")) {
                std::vector<double> balances;
                for (const auto& p : summary.dailyBalance) balances.push_back(p.balance);
                drawMiniBars("##dailybal", balances, 110.0f);
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
                    int added = importCsvTransactions(data, csvPath);
                    if (added >= 0) std::snprintf(notice, sizeof(notice), "Imported %d transactions", added);
                    else std::snprintf(notice, sizeof(notice), "CSV import failed");
                }
                ImGui::TextWrapped("Editable auto-categorization rules are in Full Editors.");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Decision Simulator")) {
                static float carPayment = 450.0f;
                static float insuranceIncrease = 110.0f;
                static float gasIncrease = 80.0f;
                static float purchaseShock = 1500.0f;
                static float rentIncrease = 0.0f;
                static float incomeChange = 0.0f;

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
                moneyText("Adjusted monthly income", adjustedIncome);
                moneyText("Adjusted monthly outflow", adjustedOutflow);
                moneyText("Adjusted monthly leftover", adjustedLeftover);
                moneyText("12-month cumulative impact", twelveMonthImpact);
                ImGui::TextWrapped(adjustedLeftover < 0.0 ? "Result: this decision makes the monthly plan negative." : "Result: this decision stays positive, but still review forecast and alerts.");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Auto Budget")) {
                ImGui::TextWrapped("Adaptive zero-based style allocation generated from your current numbers.");
                ImGui::Separator();
                for (const auto& row : autoBudget) moneyText(row.bucket.c_str(), row.amount);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Goals Intelligence")) {
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
                for (const auto& r : riskSignals) ImGui::BulletText("[%s] %s", r.severity.c_str(), r.summary.c_str());
                ImGui::Separator();
                ImGui::TextUnformatted("Existing alerts");
                for (const auto& a : summary.alerts) ImGui::BulletText("[%s] %s", a.severity.c_str(), a.message.c_str());
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Automation Rules")) {
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

                ImGui::InputFloat("One-time move cost", &lifeMoveCost, 50.0f, 100.0f, "%.2f");
                ImGui::InputFloat("Monthly rent delta", &lifeRentDelta, 10.0f, 50.0f, "%.2f");
                ImGui::InputFloat("Monthly income delta", &lifeIncomeDelta, 10.0f, 50.0f, "%.2f");
                ImGui::InputFloat("Monthly utilities delta", &lifeUtilityDelta, 10.0f, 50.0f, "%.2f");

                const double adjustedIncome = summary.monthlyIncome + lifeIncomeDelta;
                const double adjustedOutflow = summary.monthlyPlannedOutflow + lifeMoveCost + lifeRentDelta + lifeUtilityDelta;
                const double adjustedLeftover = adjustedIncome - adjustedOutflow;

                moneyText("Adjusted monthly income", adjustedIncome);
                moneyText("Adjusted monthly outflow", adjustedOutflow);
                moneyText("Adjusted monthly leftover", adjustedLeftover);
                ImGui::TextWrapped(adjustedLeftover >= 0.0
                    ? "This life change remains cashflow-positive under the current plan."
                    : "This life change breaks the monthly plan unless other changes are made.");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Execution Plan")) {
                moneyText("Total monthly improvement available", executionPlan.totalMonthlyImpact);
                ImGui::Text("Closes deficit: %s", executionPlan.closesDeficit ? "Yes" : "No");
                for (std::size_t i = 0; i < executionPlan.steps.size(); ++i) {
                    const auto& step = executionPlan.steps[i];
                    std::string title = std::to_string(i + 1) + ". " + step.title;
                    if (ImGui::TreeNode(title.c_str())) {
                        ImGui::TextWrapped("%s", step.description.c_str());
                        moneyText("Monthly impact", step.monthlyImpact);
                        ImGui::Text("Priority: %d", step.priority);
                        ImGui::TreePop();
                    }
                }
                ImGui::EndTabItem();
            }


            if (ImGui::BeginTabItem("UI + Accessibility")) {
                ImGui::TextWrapped("This overhaul makes the interface larger, easier to read, and easier to click.");
                ImGui::Separator();
                ImGui::SliderFloat("Live UI scale", &data.settings.uiScale, 1.0f, 3.5f, "%.2fx");
                ImGui::TextWrapped("Tips:");
                ImGui::BulletText("Use a scale around 1.50x to 2.00x for comfortable desktop reading.");
                ImGui::BulletText("Use a scale around 2.25x to 3.00x for high-resolution or distance viewing.");
                ImGui::BulletText("The UI now increases text, padding, spacing, and interactive target sizes together.");
                ImGui::Separator();
                ImGui::Text("Current scale: %.2fx", data.settings.uiScale);
                ImGui::Text("Frame padding: %.1f x %.1f", ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y);
                ImGui::Text("Item spacing: %.1f x %.1f", ImGui::GetStyle().ItemSpacing.x, ImGui::GetStyle().ItemSpacing.y);
                ImGui::TextWrapped("This tab is meant to make the application easier to use on higher DPI screens and from farther away.");
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
