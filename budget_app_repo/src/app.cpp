#include "app.hpp"
#include "advice_engine.hpp"
#include "models.hpp"
#include "storage.hpp"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#include <windows.h>
#endif
#include <GL/gl.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <string>
#include <vector>

namespace budget {
namespace {

struct InputTextBuffer {
    char data[128]{};

    explicit InputTextBuffer(const std::string& value = {}) {
        set(value);
    }

    void set(const std::string& value) {
        std::memset(data, 0, sizeof(data));
        std::strncpy(data, value.c_str(), sizeof(data) - 1);
    }

    std::string str() const {
        return std::string(data);
    }
};

void drawMoneyText(const char* label, double amount) {
    ImGui::Text("%s", label);
    ImGui::SameLine(240.0f);
    ImGui::Text("$%.2f", amount);
}

} // namespace

double frequencyToWeekly(Frequency frequency, double amount) {
    switch (frequency) {
        case Frequency::Weekly: return amount;
        case Frequency::BiWeekly: return amount / 2.0;
        case Frequency::Monthly: return amount * 12.0 / 52.0;
        case Frequency::Quarterly: return amount * 4.0 / 52.0;
        case Frequency::Yearly: return amount / 52.0;
    }
    return amount;
}

const char* frequencyToString(Frequency frequency) {
    switch (frequency) {
        case Frequency::Weekly: return "Weekly";
        case Frequency::BiWeekly: return "Bi-Weekly";
        case Frequency::Monthly: return "Monthly";
        case Frequency::Quarterly: return "Quarterly";
        case Frequency::Yearly: return "Yearly";
    }
    return "Monthly";
}

Frequency frequencyFromIndex(int index) {
    switch (index) {
        case 0: return Frequency::Weekly;
        case 1: return Frequency::BiWeekly;
        case 2: return Frequency::Monthly;
        case 3: return Frequency::Quarterly;
        case 4: return Frequency::Yearly;
        default: return Frequency::Monthly;
    }
}

int frequencyToIndex(Frequency frequency) {
    switch (frequency) {
        case Frequency::Weekly: return 0;
        case Frequency::BiWeekly: return 1;
        case Frequency::Monthly: return 2;
        case Frequency::Quarterly: return 3;
        case Frequency::Yearly: return 4;
    }
    return 2;
}

BudgetSummary summarize(const BudgetData& data) {
    BudgetSummary s;

    for (const auto& income : data.incomes) {
        s.weeklyIncome += income.weeklyAmount;
    }

    for (const auto& bill : data.recurringBills) {
        const double weekly = frequencyToWeekly(bill.frequency, bill.amount);
        s.weeklyRecurringBills += weekly;
        if (bill.essential) {
            s.weeklyEssentials += weekly;
        }
    }

    for (const auto& category : data.weeklyPlan.categories) {
        s.weeklyPlannedTotal += category.planned;
        s.weeklyDiscretionarySpent += category.spent;
        if (category.essential) {
            s.weeklyEssentials += category.planned;
        } else {
            s.weeklyDiscretionaryPlanned += category.planned;
        }
    }

    s.weeklyAssigned = s.weeklyRecurringBills
        + s.weeklyPlannedTotal
        + data.weeklyPlan.extraDebtPayment
        + data.weeklyPlan.emergencyFundContribution
        + data.weeklyPlan.investingContribution;

    s.weeklyAvailableToAssign = s.weeklyIncome;
    s.weeklyLeftover = s.weeklyIncome - s.weeklyAssigned;
    s.zeroBasedBalanced = std::abs(s.weeklyLeftover) < 0.01;

    const double savingsWeekly = data.weeklyPlan.emergencyFundContribution + data.weeklyPlan.investingContribution;
    s.savingsRate = s.weeklyIncome > 0.0 ? savingsWeekly / s.weeklyIncome : 0.0;
    s.emergencyFundMonths = s.weeklyEssentials > 0.0 ? data.emergencyFundBalance / (s.weeklyEssentials * 52.0 / 12.0) : 0.0;

    return s;
}

int BudgetApp::run() {
    if (!glfwInit()) {
        return 1;
    }

    const char* glslVersion = "#version 130";
    GLFWwindow* window = glfwCreateWindow(1480, 960, "BudgetPilot", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    const auto dataPath = std::filesystem::current_path() / "data" / "budget_data.json";
    Storage storage(dataPath);
    BudgetData data;
    std::string statusMessage;
    std::string error;
    if (!storage.load(data, error)) {
        data = makeSampleData();
        statusMessage = "Load failed. Started with sample data: " + error;
    }

    InputTextBuffer newIncomeName("New income");
    double newIncomeAmount = 500.0;

    InputTextBuffer newBillName("New bill");
    double newBillAmount = 100.0;
    int newBillFrequencyIndex = 2;
    int newBillDueDay = 1;
    bool newBillEssential = true;
    bool newBillAutopay = true;

    InputTextBuffer weekLabelBuffer(data.weeklyPlan.weekLabel);
    InputTextBuffer newCategoryName("New category");
    double newCategoryPlanned = 50.0;
    double newCategorySpent = 0.0;
    bool newCategoryEssential = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const BudgetSummary summary = summarize(data);
        const std::vector<AdviceItem> adviceItems = buildAdvice(data, summary);

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
        ImGui::Begin("BudgetPilotMain", nullptr,
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::Button("Save")) {
                std::string saveError;
                if (storage.save(data, saveError)) {
                    statusMessage = "Saved to data/budget_data.json";
                } else {
                    statusMessage = "Save failed: " + saveError;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset to sample")) {
                data = makeSampleData();
                weekLabelBuffer.set(data.weeklyPlan.weekLabel);
                statusMessage = "Sample data loaded.";
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(statusMessage.c_str());
            ImGui::EndMenuBar();
        }

        ImGui::Text("Budget method: zero-based weekly planning with recurring-bill normalization");
        ImGui::TextWrapped("Every income dollar gets assigned to bills, essentials, discretionary spending, debt, emergency savings, or investing. Recurring bills are converted to weekly equivalents so you can plan each week with fewer surprises.");
        ImGui::Separator();

        if (ImGui::BeginTable("Dashboard", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("Weekly income\n$%.2f", summary.weeklyIncome);
            ImGui::TableSetColumnIndex(1); ImGui::Text("Assigned this week\n$%.2f", summary.weeklyAssigned);
            ImGui::TableSetColumnIndex(2); ImGui::Text("Left to assign\n$%.2f", summary.weeklyLeftover);
            ImGui::TableSetColumnIndex(3); ImGui::Text("Emergency fund\n%.2f months", summary.emergencyFundMonths);
            ImGui::EndTable();
        }

        if (ImGui::BeginTabBar("MainTabs")) {
            if (ImGui::BeginTabItem("Snapshot")) {
                ImGui::Columns(2, nullptr, true);
                ImGui::Text("Accounts & Goals");
                ImGui::InputDouble("Checking balance", &data.checkingBalance, 0.0, 0.0, "%.2f");
                ImGui::InputDouble("Emergency fund balance", &data.emergencyFundBalance, 0.0, 0.0, "%.2f");
                ImGui::InputDouble("High-interest debt balance", &data.highInterestDebtBalance, 0.0, 0.0, "%.2f");
                ImGui::InputDouble("Savings goal", &data.savingsGoal, 0.0, 0.0, "%.2f");

                ImGui::NextColumn();
                ImGui::Text("Calculated weekly metrics");
                drawMoneyText("Recurring bills", summary.weeklyRecurringBills);
                drawMoneyText("Essential weekly costs", summary.weeklyEssentials);
                drawMoneyText("Discretionary planned", summary.weeklyDiscretionaryPlanned);
                drawMoneyText("Discretionary spent", summary.weeklyDiscretionarySpent);
                ImGui::Text("Savings + investing rate");
                ImGui::SameLine(240.0f);
                ImGui::Text("%.2f%%", summary.savingsRate * 100.0);
                ImGui::Text("Zero-based balanced");
                ImGui::SameLine(240.0f);
                ImGui::Text("%s", summary.zeroBasedBalanced ? "Yes" : "No");
                ImGui::Columns(1);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Income")) {
                ImGui::Text("Weekly income sources");
                for (int i = 0; i < static_cast<int>(data.incomes.size()); ++i) {
                    auto& income = data.incomes[i];
                    ImGui::PushID(i);
                    InputTextBuffer name(income.name);
                    name.set(income.name);
                    ImGui::Text("%s - $%.2f/week", income.name.c_str(), income.weeklyAmount);
                    ImGui::SameLine(300.0f);
                    if (ImGui::Button("Remove")) {
                        data.incomes.erase(data.incomes.begin() + i);
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
                ImGui::Separator();
                ImGui::InputText("Income name", newIncomeName.data, sizeof(newIncomeName.data));
                ImGui::InputDouble("Weekly amount", &newIncomeAmount, 0.0, 0.0, "%.2f");
                if (ImGui::Button("Add income") && !newIncomeName.str().empty()) {
                    data.incomes.push_back({newIncomeName.str(), std::max(0.0, newIncomeAmount)});
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Recurring Bills")) {
                ImGui::TextWrapped("Bills are normalized to weekly amounts so the app can build a realistic weekly plan even when rent or utilities are monthly.");
                if (ImGui::BeginTable("BillsTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Bill");
                    ImGui::TableSetupColumn("Amount");
                    ImGui::TableSetupColumn("Frequency");
                    ImGui::TableSetupColumn("Due day");
                    ImGui::TableSetupColumn("Weekly equiv.");
                    ImGui::TableSetupColumn("Essential");
                    ImGui::TableSetupColumn("Action");
                    ImGui::TableHeadersRow();

                    for (int i = 0; i < static_cast<int>(data.recurringBills.size()); ++i) {
                        const auto& bill = data.recurringBills[i];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(bill.name.c_str());
                        ImGui::TableSetColumnIndex(1); ImGui::Text("$%.2f", bill.amount);
                        ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(frequencyToString(bill.frequency));
                        ImGui::TableSetColumnIndex(3); ImGui::Text("%d", bill.dueDay);
                        ImGui::TableSetColumnIndex(4); ImGui::Text("$%.2f", frequencyToWeekly(bill.frequency, bill.amount));
                        ImGui::TableSetColumnIndex(5); ImGui::TextUnformatted(bill.essential ? "Yes" : "No");
                        ImGui::TableSetColumnIndex(6);
                        ImGui::PushID(i + 1000);
                        if (ImGui::SmallButton("Delete")) {
                            data.recurringBills.erase(data.recurringBills.begin() + i);
                            ImGui::PopID();
                            break;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }

                ImGui::Separator();
                ImGui::InputText("Bill name", newBillName.data, sizeof(newBillName.data));
                ImGui::InputDouble("Bill amount", &newBillAmount, 0.0, 0.0, "%.2f");
                const char* frequencies[] = {"Weekly", "Bi-Weekly", "Monthly", "Quarterly", "Yearly"};
                ImGui::Combo("Frequency", &newBillFrequencyIndex, frequencies, IM_ARRAYSIZE(frequencies));
                ImGui::SliderInt("Due day", &newBillDueDay, 1, 31);
                ImGui::Checkbox("Essential bill", &newBillEssential);
                ImGui::Checkbox("Autopay", &newBillAutopay);
                if (ImGui::Button("Add recurring bill") && !newBillName.str().empty()) {
                    data.recurringBills.push_back({
                        newBillName.str(),
                        std::max(0.0, newBillAmount),
                        frequencyFromIndex(newBillFrequencyIndex),
                        newBillDueDay,
                        newBillEssential,
                        newBillAutopay
                    });
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Weekly Plan")) {
                ImGui::InputText("Plan label", weekLabelBuffer.data, sizeof(weekLabelBuffer.data));
                data.weeklyPlan.weekLabel = weekLabelBuffer.str();
                ImGui::InputDouble("Extra debt payment", &data.weeklyPlan.extraDebtPayment, 0.0, 0.0, "%.2f");
                ImGui::InputDouble("Emergency fund contribution", &data.weeklyPlan.emergencyFundContribution, 0.0, 0.0, "%.2f");
                ImGui::InputDouble("Investing contribution", &data.weeklyPlan.investingContribution, 0.0, 0.0, "%.2f");
                ImGui::Separator();

                if (ImGui::BeginTable("PlanTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Category");
                    ImGui::TableSetupColumn("Planned");
                    ImGui::TableSetupColumn("Spent");
                    ImGui::TableSetupColumn("Remaining");
                    ImGui::TableSetupColumn("Essential");
                    ImGui::TableSetupColumn("Action");
                    ImGui::TableHeadersRow();

                    for (int i = 0; i < static_cast<int>(data.weeklyPlan.categories.size()); ++i) {
                        auto& category = data.weeklyPlan.categories[i];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(category.name.c_str());
                        ImGui::TableSetColumnIndex(1); ImGui::Text("$%.2f", category.planned);
                        ImGui::TableSetColumnIndex(2); ImGui::Text("$%.2f", category.spent);
                        ImGui::TableSetColumnIndex(3); ImGui::Text("$%.2f", category.planned - category.spent);
                        ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(category.essential ? "Yes" : "No");
                        ImGui::TableSetColumnIndex(5);
                        ImGui::PushID(i + 3000);
                        if (ImGui::SmallButton("Delete")) {
                            data.weeklyPlan.categories.erase(data.weeklyPlan.categories.begin() + i);
                            ImGui::PopID();
                            break;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }

                ImGui::Separator();
                ImGui::InputText("Category name", newCategoryName.data, sizeof(newCategoryName.data));
                ImGui::InputDouble("Category planned", &newCategoryPlanned, 0.0, 0.0, "%.2f");
                ImGui::InputDouble("Category spent", &newCategorySpent, 0.0, 0.0, "%.2f");
                ImGui::Checkbox("Category essential", &newCategoryEssential);
                if (ImGui::Button("Add weekly category") && !newCategoryName.str().empty()) {
                    data.weeklyPlan.categories.push_back({newCategoryName.str(), std::max(0.0, newCategoryPlanned), std::max(0.0, newCategorySpent), newCategoryEssential});
                }

                ImGui::Spacing();
                ImGui::Text("Weekly cash-flow check");
                drawMoneyText("Income", summary.weeklyIncome);
                drawMoneyText("Assigned", summary.weeklyAssigned);
                drawMoneyText("Leftover / gap", summary.weeklyLeftover);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Advice")) {
                ImGui::TextWrapped("This guidance is educational, not individualized investment, legal, or tax advice. It reacts to the numbers you enter and points to government consumer-finance resources.");
                ImGui::Separator();
                for (const auto& item : adviceItems) {
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
                    if (ImGui::CollapsingHeader(item.title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::TextWrapped("%s", item.detail.c_str());
                        ImGui::Spacing();
                        ImGui::Text("Resources:");
                        for (const auto& resource : item.resources) {
                            ImGui::BulletText("%s (%s)", resource.title.c_str(), resource.organization.c_str());
                            ImGui::TextWrapped("%s", resource.url.c_str());
                        }
                    }
                    ImGui::PopStyleVar();
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
        glClearColor(0.10f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    std::string saveError;
    storage.save(data, saveError);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace budget
