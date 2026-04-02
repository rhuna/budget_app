#include "app.hpp"
#include "advice_engine.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <cstdio>

namespace budget {

namespace {

double weeklyFrom(double amount, Frequency frequency) {
    switch (frequency) {
        case Frequency::Weekly: return amount;
        case Frequency::BiWeekly: return amount / 2.0;
        case Frequency::Monthly: return amount * 12.0 / 52.0;
        case Frequency::Quarterly: return amount * 4.0 / 52.0;
        case Frequency::Yearly: return amount / 52.0;
        default: return amount;
    }
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

void moneyText(const char* label, double value) {
    ImGui::Text("%s", label);
    ImGui::SameLine(290.0f);
    ImGui::Text("$%.2f", value);
}

void summaryCard(const char* title, const char* value) {
    ImGui::BeginChild(title, ImVec2(230, 75), true);
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

    GLFWwindow* window = glfwCreateWindow(1450, 900, "BudgetPilot Real-Time Budget Planner", nullptr, nullptr);
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

        BudgetSummary summary = buildSummary(data);
        auto advice = buildAdvice(data, summary);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(1430, 880), ImGuiCond_Once);
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

        char score[64], income[64], planned[64], actual[64], cover[64];
        std::snprintf(score, sizeof(score), "%d / 100", summary.healthScore);
        std::snprintf(income, sizeof(income), "$%.2f / week", summary.weeklyIncome);
        std::snprintf(planned, sizeof(planned), "$%.2f left planned", summary.weeklyPlannedLeftover);
        std::snprintf(actual, sizeof(actual), "$%.2f left actual", summary.weeklyActualLeftover);
        std::snprintf(cover, sizeof(cover), "%.1f weeks covered", summary.emergencyWeeksCovered);

        summaryCard("Budget Health", score); ImGui::SameLine();
        summaryCard("Weekly Income", income); ImGui::SameLine();
        summaryCard("Planned Leftover", planned); ImGui::SameLine();
        summaryCard("Actual Leftover", actual); ImGui::SameLine();
        summaryCard("Emergency Coverage", cover);

        ImGui::Separator();

        if (ImGui::BeginTabBar("BudgetTabs")) {
            if (ImGui::BeginTabItem("Live Budget")) {
                ImGui::TextWrapped("This page updates in real time as you add income, bills, variable expenses, tax reserve, one-time costs, and goal contributions.");
                ImGui::Separator();
                moneyText("Weekly income", summary.weeklyIncome);
                moneyText("Weekly fixed bills", summary.weeklyBills);
                moneyText("Weekly variable plan", summary.weeklyVariablePlanned);
                moneyText("Weekly actual variable", summary.weeklyVariableActual);
                moneyText("Weekly goal contributions", summary.weeklyGoals);
                moneyText("Weekly one-time costs", summary.weeklyOneTime);
                moneyText("Weekly tax reserve", summary.weeklyTaxReserve);
                ImGui::Separator();
                moneyText("Planned total outflow", summary.weeklyPlannedOutflow);
                moneyText("Actual total outflow", summary.weeklyActualOutflow);
                moneyText("Planned leftover", summary.weeklyPlannedLeftover);
                moneyText("Actual leftover", summary.weeklyActualLeftover);

                ImGui::Separator();
                ImGui::InputDouble("Checking balance", &data.checkingBalance, 10.0, 100.0, "%.2f");
                ImGui::InputDouble("Emergency fund balance", &data.emergencyFundBalance, 10.0, 100.0, "%.2f");
                ImGui::InputDouble("Debt balance", &data.debtBalance, 10.0, 100.0, "%.2f");
                ImGui::InputDouble("Savings goal", &data.savingsGoal, 10.0, 100.0, "%.2f");
                ImGui::InputDouble("Tax reserve per week", &data.taxReserveWeekly, 5.0, 20.0, "%.2f");

                ImGui::ProgressBar(static_cast<float>(summary.savingsProgress), ImVec2(300, 0));
                ImGui::Text("Emergency fund progress toward goal: %.0f%%", summary.savingsProgress * 100.0);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Income")) {
                static char name[128] = "";
                static float amount = 0.0f;
                static int freq = 0;
                static bool afterTax = true;

                ImGui::InputText("Income name", name, IM_ARRAYSIZE(name));
                ImGui::InputFloat("Amount", &amount, 1.0f, 10.0f, "%.2f");
                ImGui::Combo("Frequency", &freq, kFrequencies, IM_ARRAYSIZE(kFrequencies));
                ImGui::Checkbox("Already after tax", &afterTax);
                if (ImGui::Button("Add income") && name[0] != '\0') {
                    data.incomes.push_back({name, amount, indexToFrequency(freq), afterTax});
                    name[0] = '\0';
                    amount = 0.0f;
                    freq = 0;
                    afterTax = true;
                }

                ImGui::Separator();
                for (std::size_t i = 0; i < data.incomes.size(); ++i) {
                    auto& item = data.incomes[i];
                    ImGui::PushID(static_cast<int>(i) + 100);
                    if (ImGui::TreeNode(item.name.c_str())) {
                        ImGui::InputDouble("Amount", &item.amount, 1.0, 10.0, "%.2f");
                        int idx = frequencyToIndex(item.frequency);
                        if (ImGui::Combo("Frequency", &idx, kFrequencies, IM_ARRAYSIZE(kFrequencies))) {
                            item.frequency = indexToFrequency(idx);
                        }
                        ImGui::Checkbox("After tax", &item.afterTax);
                        ImGui::Text("Weekly value: $%.2f", weeklyFrom(item.amount, item.frequency));
                        if (ImGui::Button("Delete")) {
                            data.incomes.erase(data.incomes.begin() + static_cast<long long>(i));
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

            if (ImGui::BeginTabItem("Bills")) {
                static char billName[128] = "";
                static float billAmount = 0.0f;
                static int billFreq = 2;
                static int dueDay = 1;
                static bool essential = true;
                static bool autopay = false;

                ImGui::InputText("Bill name", billName, IM_ARRAYSIZE(billName));
                ImGui::InputFloat("Bill amount", &billAmount, 1.0f, 10.0f, "%.2f");
                ImGui::Combo("Bill frequency", &billFreq, kFrequencies, IM_ARRAYSIZE(kFrequencies));
                ImGui::SliderInt("Due day", &dueDay, 1, 31);
                ImGui::Checkbox("Essential bill", &essential);
                ImGui::Checkbox("Auto-pay", &autopay);
                if (ImGui::Button("Add bill") && billName[0] != '\0') {
                    data.bills.push_back({billName, billAmount, indexToFrequency(billFreq), dueDay, essential, autopay});
                    billName[0] = '\0';
                    billAmount = 0.0f;
                    billFreq = 2;
                    dueDay = 1;
                    essential = true;
                    autopay = false;
                }

                ImGui::Separator();
                for (std::size_t i = 0; i < data.bills.size(); ++i) {
                    auto& item = data.bills[i];
                    ImGui::PushID(static_cast<int>(i) + 500);
                    if (ImGui::TreeNode(item.name.c_str())) {
                        ImGui::InputDouble("Amount", &item.amount, 1.0, 10.0, "%.2f");
                        int idx = frequencyToIndex(item.frequency);
                        if (ImGui::Combo("Frequency", &idx, kFrequencies, IM_ARRAYSIZE(kFrequencies))) {
                            item.frequency = indexToFrequency(idx);
                        }
                        ImGui::SliderInt("Due day", &item.dueDay, 1, 31);
                        ImGui::Checkbox("Essential", &item.essential);
                        ImGui::Checkbox("Auto-pay", &item.autopay);
                        ImGui::Text("Weekly impact: $%.2f", weeklyFrom(item.amount, item.frequency));
                        if (ImGui::Button("Delete")) {
                            data.bills.erase(data.bills.begin() + static_cast<long long>(i));
                            ImGui::TreePop();
                            ImGui::PopID();
                            break;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                ImGui::TextWrapped("Every bill added here immediately changes the live budget numbers above.");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Variable Spending")) {
                static char varName[128] = "";
                static float plan = 0.0f;
                static float actual = 0.0f;
                static bool essentialVar = false;

                ImGui::InputText("Category", varName, IM_ARRAYSIZE(varName));
                ImGui::InputFloat("Weekly plan", &plan, 1.0f, 10.0f, "%.2f");
                ImGui::InputFloat("Actual so far", &actual, 1.0f, 10.0f, "%.2f");
                ImGui::Checkbox("Essential category", &essentialVar);

                if (ImGui::Button("Add variable category") && varName[0] != '\0') {
                    data.variableExpenses.push_back({varName, plan, actual, essentialVar});
                    varName[0] = '\0';
                    plan = 0.0f;
                    actual = 0.0f;
                    essentialVar = false;
                }

                ImGui::Separator();
                for (std::size_t i = 0; i < data.variableExpenses.size(); ++i) {
                    auto& item = data.variableExpenses[i];
                    ImGui::PushID(static_cast<int>(i) + 1000);
                    if (ImGui::TreeNode(item.name.c_str())) {
                        ImGui::InputDouble("Weekly plan", &item.weeklyPlan, 1.0, 10.0, "%.2f");
                        ImGui::InputDouble("Actual", &item.actual, 1.0, 10.0, "%.2f");
                        ImGui::Checkbox("Essential", &item.essential);
                        ImGui::Text("Remaining in category: $%.2f", item.weeklyPlan - item.actual);
                        if (ImGui::Button("Delete")) {
                            data.variableExpenses.erase(data.variableExpenses.begin() + static_cast<long long>(i));
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

            if (ImGui::BeginTabItem("Goals + One-Time")) {
                static char goalName[128] = "";
                static float weeklyContribution = 0.0f;
                static float currentBalance = 0.0f;
                static float targetBalance = 0.0f;

                ImGui::TextUnformatted("Goal / sinking funds");
                ImGui::InputText("Goal name", goalName, IM_ARRAYSIZE(goalName));
                ImGui::InputFloat("Weekly contribution", &weeklyContribution, 1.0f, 10.0f, "%.2f");
                ImGui::InputFloat("Current balance", &currentBalance, 1.0f, 10.0f, "%.2f");
                ImGui::InputFloat("Target balance", &targetBalance, 1.0f, 10.0f, "%.2f");
                if (ImGui::Button("Add goal bucket") && goalName[0] != '\0') {
                    data.goals.push_back({goalName, weeklyContribution, currentBalance, targetBalance});
                    goalName[0] = '\0';
                    weeklyContribution = 0.0f;
                    currentBalance = 0.0f;
                    targetBalance = 0.0f;
                }

                ImGui::Separator();

                static char oneName[128] = "";
                static float oneAmount = 0.0f;
                static bool thisWeek = true;
                ImGui::TextUnformatted("One-time items");
                ImGui::InputText("Expense name", oneName, IM_ARRAYSIZE(oneName));
                ImGui::InputFloat("Amount", &oneAmount, 1.0f, 10.0f, "%.2f");
                ImGui::Checkbox("Counts this week", &thisWeek);
                if (ImGui::Button("Add one-time expense") && oneName[0] != '\0') {
                    data.oneTimeExpenses.push_back({oneName, oneAmount, thisWeek});
                    oneName[0] = '\0';
                    oneAmount = 0.0f;
                    thisWeek = true;
                }

                ImGui::Separator();
                for (std::size_t i = 0; i < data.goals.size(); ++i) {
                    auto& item = data.goals[i];
                    ImGui::PushID(static_cast<int>(i) + 1500);
                    if (ImGui::TreeNode(item.name.c_str())) {
                        ImGui::InputDouble("Weekly contribution", &item.weeklyContribution, 1.0, 10.0, "%.2f");
                        ImGui::InputDouble("Current balance", &item.currentBalance, 1.0, 10.0, "%.2f");
                        ImGui::InputDouble("Target balance", &item.targetBalance, 1.0, 10.0, "%.2f");
                        if (ImGui::Button("Delete goal")) {
                            data.goals.erase(data.goals.begin() + static_cast<long long>(i));
                            ImGui::TreePop();
                            ImGui::PopID();
                            break;
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }

                for (std::size_t i = 0; i < data.oneTimeExpenses.size(); ++i) {
                    auto& item = data.oneTimeExpenses[i];
                    ImGui::PushID(static_cast<int>(i) + 2000);
                    if (ImGui::TreeNode(item.name.c_str())) {
                        ImGui::InputDouble("Amount", &item.amount, 1.0, 10.0, "%.2f");
                        ImGui::Checkbox("Counts this week", &item.thisWeek);
                        if (ImGui::Button("Delete one-time")) {
                            data.oneTimeExpenses.erase(data.oneTimeExpenses.begin() + static_cast<long long>(i));
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

            if (ImGui::BeginTabItem("Advice")) {
                ImGui::Text("Health Score: %d", summary.healthScore);
                ImGui::BulletText("Weekly income: $%.2f", summary.weeklyIncome);
                ImGui::BulletText("Planned outflow: $%.2f", summary.weeklyPlannedOutflow);
                ImGui::BulletText("Planned leftover: $%.2f", summary.weeklyPlannedLeftover);
                ImGui::BulletText("Actual leftover: $%.2f", summary.weeklyActualLeftover);
                ImGui::BulletText("Essential weekly load: $%.2f", summary.essentialWeekly);
                ImGui::Separator();

                for (const auto& line : advice) {
                    ImGui::BulletText("%s", line.c_str());
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

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
