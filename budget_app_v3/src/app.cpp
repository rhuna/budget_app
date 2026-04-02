#include "app.hpp"
#include "advice_engine.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <cstdio>

namespace budget {

namespace {

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

Frequency indexToFrequency(int index) {
    switch (index) {
        case 0: return Frequency::Weekly;
        case 1: return Frequency::BiWeekly;
        case 2: return Frequency::Monthly;
        case 3: return Frequency::Quarterly;
        case 4: return Frequency::Yearly;
        default: return Frequency::Monthly;
    }
}

const char* kFrequencies[] = {"Weekly", "Bi-Weekly", "Monthly", "Quarterly", "Yearly"};
const char* kMonths[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

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

} // namespace

void runApp(Storage& storage) {
    BudgetData data;
    std::string error;
    storage.load(data, error);

    if (!glfwInit()) return;

    GLFWwindow* window = glfwCreateWindow(1540, 960, "BudgetPilot Monthly Planner", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return;
    }

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

        ImGui::GetStyle().ScaleAllSizes(1.0f);
        ImGui::GetIO().FontGlobalScale = data.settings.uiScale;

        BudgetSummary summary = buildSummary(data);
        auto advice = buildAdvice(data, summary);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(1520, 940), ImGuiCond_Once);
        ImGui::Begin("BudgetPilot");

        if (ImGui::Button("Save")) {
            std::string saveError;
            if (storage.save(data, saveError)) {
                std::snprintf(saveNotice, sizeof(saveNotice), "Saved to data/budget_data.json");
            } else {
                std::snprintf(saveNotice, sizeof(saveNotice), "Save failed: %s", saveError.c_str());
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Sample Data")) {
            data = makeSampleData();
            std::snprintf(saveNotice, sizeof(saveNotice), "Reset to sample data");
        }
        ImGui::SameLine();
        ImGui::TextWrapped("%s", saveNotice);

        ImGui::Separator();
        ImGui::TextUnformatted("Planner Controls");
        ImGui::Combo("Month", &data.settings.month, kMonths, 12);
        data.settings.month += 1;
        if (data.settings.month > 12) data.settings.month = 12;
        ImGui::InputInt("Year", &data.settings.year);
        ImGui::SliderFloat("Zoom / UI scale", &data.settings.uiScale, 1.0f, 2.25f, "%.2fx");
        ImGui::SliderInt("Paycheck windows", &data.settings.paycheckCount, 1, 4);

        ImGui::Separator();

        char score[64], income[64], planned[64], actual[64], cover[64];
        std::snprintf(score, sizeof(score), "%d / 100", summary.healthScore);
        std::snprintf(income, sizeof(income), "$%.2f / month", summary.monthlyIncome);
        std::snprintf(planned, sizeof(planned), "$%.2f left planned", summary.monthlyPlannedLeftover);
        std::snprintf(actual, sizeof(actual), "$%.2f left actual", summary.monthlyActualLeftover);
        std::snprintf(cover, sizeof(cover), "%.2f months covered", summary.emergencyMonthsCovered);

        summaryCard("Budget Health", score); ImGui::SameLine();
        summaryCard("Monthly Income", income); ImGui::SameLine();
        summaryCard("Planned Leftover", planned); ImGui::SameLine();
        summaryCard("Actual Leftover", actual); ImGui::SameLine();
        summaryCard("Emergency Coverage", cover);

        ImGui::Separator();

        if (ImGui::BeginTabBar("PlannerTabs")) {
            if (ImGui::BeginTabItem("Monthly Overview")) {
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
                ImGui::InputDouble("Checking balance", &data.checkingBalance, 10.0, 100.0, "%.2f");
                ImGui::InputDouble("Emergency fund balance", &data.emergencyFundBalance, 10.0, 100.0, "%.2f");
                ImGui::InputDouble("Debt balance", &data.debtBalance, 10.0, 100.0, "%.2f");
                ImGui::InputDouble("Savings goal", &data.savingsGoal, 10.0, 100.0, "%.2f");
                ImGui::InputDouble("Monthly tax reserve", &data.monthlyTaxReserve, 10.0, 50.0, "%.2f");

                ImGui::ProgressBar(static_cast<float>(summary.savingsProgress), ImVec2(320, 0));
                ImGui::Text("Emergency fund progress: %.0f%%", summary.savingsProgress * 100.0);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Calendar")) {
                ImGui::TextWrapped("Calendar-style due dates: green numbers show inflow, red numbers show outflow. Events listed below each day.");
                ImGui::Separator();

                const int cols = 7;
                if (ImGui::BeginTable("CalendarTable", cols, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    for (int c = 0; c < cols; ++c) {
                        ImGui::TableSetupColumn("");
                    }
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
                                for (const auto& label : cell.labels) {
                                    ImGui::BulletText("%s", label.c_str());
                                }
                                ++dayIndex;
                            }
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Paycheck Forecast")) {
                ImGui::TextWrapped("Paycheck-by-paycheck forecasting splits the month into windows and shows what each window can safely carry.");
                ImGui::Separator();
                for (std::size_t i = 0; i < summary.paychecks.size(); ++i) {
                    const auto& p = summary.paychecks[i];
                    ImGui::PushID(static_cast<int>(i) + 1000);
                    if (ImGui::TreeNode(p.label.c_str())) {
                        ImGui::Text("Days: %d - %d", p.startDay, p.endDay);
                        moneyText("Inflow", p.inflow);
                        moneyText("Fixed bills", p.fixedBills);
                        moneyText("Variable budget", p.variableBudget);
                        moneyText("Goal budget", p.goalBudget);
                        moneyText("One-time budget", p.oneTimeBudget);
                        moneyText("Tax reserve", p.taxReserve);
                        moneyText("Leftover", p.leftover);
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Income + Bills")) {
                static char incomeName[128] = "";
                static float incomeAmount = 0.0f;
                static int incomeFreq = 1;
                static int payDay1 = 1;
                static int payDay2 = 15;
                static bool afterTax = true;

                ImGui::TextUnformatted("Income");
                ImGui::InputText("Income name", incomeName, IM_ARRAYSIZE(incomeName));
                ImGui::InputFloat("Income amount", &incomeAmount, 1.0f, 10.0f, "%.2f");
                ImGui::Combo("Income frequency", &incomeFreq, kFrequencies, IM_ARRAYSIZE(kFrequencies));
                ImGui::SliderInt("Pay day 1", &payDay1, 1, 31);
                ImGui::SliderInt("Pay day 2", &payDay2, 1, 31);
                ImGui::Checkbox("Already after tax", &afterTax);
                if (ImGui::Button("Add income") && incomeName[0] != '\0') {
                    data.incomes.push_back({incomeName, incomeAmount, indexToFrequency(incomeFreq), payDay1, payDay2, afterTax});
                    incomeName[0] = '\0';
                    incomeAmount = 0.0f;
                }

                ImGui::Separator();

                static char billName[128] = "";
                static float billAmount = 0.0f;
                static int billFreq = 2;
                static int dueDay = 1;
                static bool essential = true;
                static bool autopay = false;

                ImGui::TextUnformatted("Bills");
                ImGui::InputText("Bill name", billName, IM_ARRAYSIZE(billName));
                ImGui::InputFloat("Bill amount", &billAmount, 1.0f, 10.0f, "%.2f");
                ImGui::Combo("Bill frequency", &billFreq, kFrequencies, IM_ARRAYSIZE(kFrequencies));
                ImGui::SliderInt("Bill due day", &dueDay, 1, 31);
                ImGui::Checkbox("Essential bill", &essential);
                ImGui::Checkbox("Auto-pay", &autopay);
                if (ImGui::Button("Add bill") && billName[0] != '\0') {
                    data.bills.push_back({billName, billAmount, indexToFrequency(billFreq), dueDay, essential, autopay});
                    billName[0] = '\0';
                    billAmount = 0.0f;
                }

                ImGui::Separator();

                for (std::size_t i = 0; i < data.incomes.size(); ++i) {
                    auto& item = data.incomes[i];
                    ImGui::PushID(static_cast<int>(i) + 2000);
                    if (ImGui::TreeNode(item.name.c_str())) {
                        ImGui::InputDouble("Amount", &item.amount, 1.0, 10.0, "%.2f");
                        int idx = frequencyToIndex(item.frequency);
                        if (ImGui::Combo("Frequency", &idx, kFrequencies, IM_ARRAYSIZE(kFrequencies))) item.frequency = indexToFrequency(idx);
                        ImGui::SliderInt("Pay day 1", &item.payDay1, 1, 31);
                        ImGui::SliderInt("Pay day 2", &item.payDay2, 1, 31);
                        if (ImGui::Button("Delete income")) {
                            data.incomes.erase(data.incomes.begin() + static_cast<long long>(i));
                            ImGui::TreePop();
                            ImGui::PopID();
                            break;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                for (std::size_t i = 0; i < data.bills.size(); ++i) {
                    auto& item = data.bills[i];
                    ImGui::PushID(static_cast<int>(i) + 3000);
                    if (ImGui::TreeNode(item.name.c_str())) {
                        ImGui::InputDouble("Amount", &item.amount, 1.0, 10.0, "%.2f");
                        int idx = frequencyToIndex(item.frequency);
                        if (ImGui::Combo("Frequency", &idx, kFrequencies, IM_ARRAYSIZE(kFrequencies))) item.frequency = indexToFrequency(idx);
                        ImGui::SliderInt("Due day", &item.dueDay, 1, 31);
                        ImGui::Checkbox("Essential", &item.essential);
                        if (ImGui::Button("Delete bill")) {
                            data.bills.erase(data.bills.begin() + static_cast<long long>(i));
                            ImGui::TreePop();
                            ImGui::PopID();
                            break;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Spending + Goals")) {
                static char varName[128] = "";
                static float monthlyPlan = 0.0f;
                static float actual = 0.0f;
                static bool essentialVar = false;

                ImGui::TextUnformatted("Variable spending");
                ImGui::InputText("Category", varName, IM_ARRAYSIZE(varName));
                ImGui::InputFloat("Monthly plan", &monthlyPlan, 1.0f, 10.0f, "%.2f");
                ImGui::InputFloat("Actual so far", &actual, 1.0f, 10.0f, "%.2f");
                ImGui::Checkbox("Essential category", &essentialVar);
                if (ImGui::Button("Add variable category") && varName[0] != '\0') {
                    data.variableExpenses.push_back({varName, monthlyPlan, actual, essentialVar});
                    varName[0] = '\0';
                    monthlyPlan = 0.0f;
                    actual = 0.0f;
                }

                ImGui::Separator();

                static char goalName[128] = "";
                static float monthlyContribution = 0.0f;
                static float currentBalance = 0.0f;
                static float targetBalance = 0.0f;

                ImGui::TextUnformatted("Goals / sinking funds");
                ImGui::InputText("Goal name", goalName, IM_ARRAYSIZE(goalName));
                ImGui::InputFloat("Monthly contribution", &monthlyContribution, 1.0f, 10.0f, "%.2f");
                ImGui::InputFloat("Current balance", &currentBalance, 1.0f, 10.0f, "%.2f");
                ImGui::InputFloat("Target balance", &targetBalance, 1.0f, 10.0f, "%.2f");
                if (ImGui::Button("Add goal") && goalName[0] != '\0') {
                    data.goals.push_back({goalName, monthlyContribution, currentBalance, targetBalance});
                    goalName[0] = '\0';
                    monthlyContribution = 0.0f;
                    currentBalance = 0.0f;
                    targetBalance = 0.0f;
                }

                ImGui::Separator();

                static char oneName[128] = "";
                static float oneAmount = 0.0f;
                static int oneDue = 1;
                static bool oneActive = true;

                ImGui::TextUnformatted("One-time expenses");
                ImGui::InputText("Expense name", oneName, IM_ARRAYSIZE(oneName));
                ImGui::InputFloat("Amount", &oneAmount, 1.0f, 10.0f, "%.2f");
                ImGui::SliderInt("Due day", &oneDue, 1, 31);
                ImGui::Checkbox("Active", &oneActive);
                if (ImGui::Button("Add one-time expense") && oneName[0] != '\0') {
                    data.oneTimeExpenses.push_back({oneName, oneAmount, oneDue, oneActive});
                    oneName[0] = '\0';
                    oneAmount = 0.0f;
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Advice")) {
                ImGui::Text("Health Score: %d", summary.healthScore);
                ImGui::BulletText("Monthly income: $%.2f", summary.monthlyIncome);
                ImGui::BulletText("Planned outflow: $%.2f", summary.monthlyPlannedOutflow);
                ImGui::BulletText("Planned leftover: $%.2f", summary.monthlyPlannedLeftover);
                ImGui::BulletText("Actual leftover: $%.2f", summary.monthlyActualLeftover);
                ImGui::BulletText("Essential monthly load: $%.2f", summary.essentialMonthly);
                ImGui::Separator();

                for (const auto& line : advice) {
                    ImGui::BulletText("%s", line.c_str());
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        data.settings.month -= 1;
        if (data.settings.month < 1) data.settings.month = 1;

        ImGui::End();

        ImGui::Render();
        int displayW = 0;
        int displayH = 0;
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
