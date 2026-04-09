#include "app.hpp"
#include "advice_engine.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cfloat>

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
        if (existing.label == snap.label) {
            existing = snap;
            return;
        }
    }
    data.history.push_back(snap);
}

} // namespace

void runApp(Storage& storage) {
    BudgetData data;
    std::string error;
    storage.load(data, error);

    if (!glfwInit()) return;
    GLFWwindow* window = glfwCreateWindow(1540, 980, "BudgetPilot Monthly Planner v12 Full Restore", nullptr, nullptr);
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

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(1520, 960), ImGuiCond_Once);
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
                drawMiniBars("##planactual", summary.chartPlanVsActual, 120.0f);
                ImGui::TextUnformatted("Paycheck leftover trend");
                drawMiniBars("##payleftover", summary.chartPaycheckLeftovers, 120.0f);
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
                ImGui::TextUnformatted("Incomes");
                for (std::size_t i = 0; i < data.incomes.size(); ++i) {
                    auto& x = data.incomes[i];
                    ImGui::PushID(static_cast<int>(i) + 1000);
                    if (ImGui::TreeNode((x.name + "##income").c_str())) {
                        ImGui::InputDouble("Amount", &x.amount, 1.0, 10.0, "%.2f");
                        int idx = frequencyToIndex(x.frequency);
                        if (ImGui::Combo("Frequency", &idx, kFrequencies, IM_ARRAYSIZE(kFrequencies))) x.frequency = indexToFrequency(idx);
                        ImGui::SliderInt("Pay day 1", &x.payDay1, 1, 31);
                        ImGui::SliderInt("Pay day 2", &x.payDay2, 1, 31);
                        ImGui::Checkbox("After tax", &x.afterTax);
                        if (ImGui::Button("Delete income")) {
                            data.incomes.erase(data.incomes.begin() + static_cast<long long>(i));
                            ImGui::TreePop(); ImGui::PopID(); break;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                ImGui::Separator();
                ImGui::TextUnformatted("Bills");
                for (std::size_t i = 0; i < data.bills.size(); ++i) {
                    auto& x = data.bills[i];
                    ImGui::PushID(static_cast<int>(i) + 2000);
                    if (ImGui::TreeNode((x.name + "##bill").c_str())) {
                        ImGui::InputDouble("Amount", &x.amount, 1.0, 10.0, "%.2f");
                        int idx = frequencyToIndex(x.frequency);
                        if (ImGui::Combo("Frequency", &idx, kFrequencies, IM_ARRAYSIZE(kFrequencies))) x.frequency = indexToFrequency(idx);
                        ImGui::SliderInt("Due day", &x.dueDay, 1, 31);
                        ImGui::Checkbox("Essential", &x.essential);
                        ImGui::Checkbox("Autopay", &x.autopay);
                        if (ImGui::Button("Delete bill")) {
                            data.bills.erase(data.bills.begin() + static_cast<long long>(i));
                            ImGui::TreePop(); ImGui::PopID(); break;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                ImGui::Separator();
                ImGui::TextUnformatted("Categories");
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

                ImGui::Separator();
                ImGui::TextUnformatted("Goals");
                for (std::size_t i = 0; i < data.goals.size(); ++i) {
                    auto& x = data.goals[i];
                    ImGui::PushID(static_cast<int>(i) + 4000);
                    if (ImGui::TreeNode((x.name + "##goal").c_str())) {
                        ImGui::InputDouble("Monthly contribution", &x.monthlyContribution, 1.0, 10.0, "%.2f");
                        ImGui::InputDouble("Current balance", &x.currentBalance, 1.0, 10.0, "%.2f");
                        ImGui::InputDouble("Target balance", &x.targetBalance, 1.0, 10.0, "%.2f");
                        if (ImGui::Button("Delete goal")) {
                            data.goals.erase(data.goals.begin() + static_cast<long long>(i));
                            ImGui::TreePop(); ImGui::PopID(); break;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                ImGui::Separator();
                ImGui::TextUnformatted("One-time expenses");
                for (std::size_t i = 0; i < data.oneTimeExpenses.size(); ++i) {
                    auto& x = data.oneTimeExpenses[i];
                    ImGui::PushID(static_cast<int>(i) + 5000);
                    if (ImGui::TreeNode((x.name + "##one").c_str())) {
                        ImGui::InputDouble("Amount", &x.amount, 1.0, 10.0, "%.2f");
                        ImGui::SliderInt("Due day", &x.dueDay, 1, 31);
                        ImGui::Checkbox("Active", &x.active);
                        if (ImGui::Button("Delete one-time")) {
                            data.oneTimeExpenses.erase(data.oneTimeExpenses.begin() + static_cast<long long>(i));
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

                ImGui::Separator();
                static char debtName[128] = "";
                static float debtBalance = 0.0f;
                static float debtApr = 0.0f;
                static float debtMin = 0.0f;

                ImGui::InputText("Debt name", debtName, IM_ARRAYSIZE(debtName));
                ImGui::InputFloat("Debt balance", &debtBalance, 1.0f, 10.0f, "%.2f");
                ImGui::InputFloat("APR %", &debtApr, 0.1f, 1.0f, "%.2f");
                ImGui::InputFloat("Minimum payment", &debtMin, 1.0f, 10.0f, "%.2f");
                if (ImGui::Button("Add debt account") && debtName[0] != '\0') {
                    data.debtAccounts.push_back({debtName, debtBalance, debtApr, debtMin});
                    debtName[0] = '\0';
                    debtBalance = debtApr = debtMin = 0.0f;
                }

                ImGui::Separator();
                for (std::size_t i = 0; i < data.debtAccounts.size(); ++i) {
                    auto& d = data.debtAccounts[i];
                    ImGui::PushID(static_cast<int>(i) + 8000);
                    if (ImGui::TreeNode((d.name + "##debt").c_str())) {
                        ImGui::InputDouble("Balance", &d.balance, 1.0, 10.0, "%.2f");
                        ImGui::InputDouble("APR", &d.apr, 0.1, 1.0, "%.2f");
                        ImGui::InputDouble("Minimum payment", &d.minimumPayment, 1.0, 10.0, "%.2f");
                        if (ImGui::Button("Delete debt")) {
                            data.debtAccounts.erase(data.debtAccounts.begin() + static_cast<long long>(i));
                            ImGui::TreePop(); ImGui::PopID(); break;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Paycheck Templates")) {
                static char templateName[128] = "";
                static float tplVar = 0.0f;
                static float tplGoal = 0.0f;
                static float tplTax = 0.0f;
                static float tplDebt = 0.0f;

                ImGui::InputText("Template name", templateName, IM_ARRAYSIZE(templateName));
                ImGui::InputFloat("Template variable budget", &tplVar, 1.0f, 10.0f, "%.2f");
                ImGui::InputFloat("Template goal budget", &tplGoal, 1.0f, 10.0f, "%.2f");
                ImGui::InputFloat("Template tax reserve", &tplTax, 1.0f, 10.0f, "%.2f");
                ImGui::InputFloat("Template extra debt", &tplDebt, 1.0f, 10.0f, "%.2f");
                if (ImGui::Button("Add template") && templateName[0] != '\0') {
                    data.paycheckTemplates.push_back({templateName, tplVar, tplGoal, tplTax, tplDebt});
                    templateName[0] = '\0';
                    tplVar = tplGoal = tplTax = tplDebt = 0.0f;
                }

                ImGui::Separator();
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
                        if (ImGui::Button("Delete template")) {
                            data.paycheckTemplates.erase(data.paycheckTemplates.begin() + static_cast<long long>(i));
                            ImGui::TreePop(); ImGui::PopID(); break;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("History + Insights")) {
                ImGui::TextWrapped("Monthly closeout history and insights charts.");
                ImGui::Separator();
                ImGui::TextUnformatted("Leftover history");
                drawMiniBars("##historyleftover", summary.chartHistoryLeftover, 120.0f);
                ImGui::TextUnformatted("Debt balance history");
                drawMiniBars("##historydebt", summary.chartHistoryDebt, 120.0f);
                ImGui::TextUnformatted("Current category actuals");
                drawMiniBars("##categoryactuals", summary.chartCategoryActuals, 120.0f);

                ImGui::Separator();
                for (std::size_t i = 0; i < data.history.size(); ++i) {
                    const auto& h = data.history[i];
                    ImGui::PushID(static_cast<int>(i) + 12000);
                    if (ImGui::TreeNode(h.label.c_str())) {
                        moneyText("Income", h.income);
                        moneyText("Outflow", h.outflow);
                        moneyText("Leftover", h.leftover);
                        moneyText("Ledger net", h.ledgerNet);
                        moneyText("Debt balance", h.debtBalance);
                        moneyText("Emergency fund", h.emergencyFund);
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
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
