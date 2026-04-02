#include "app.hpp"
#include "advice_engine.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

namespace budget
{

    void runApp(Storage &storage)
    {
        BudgetData data;
        std::string error;
        storage.load(data, error);

        if (!glfwInit())
            return;

        GLFWwindow *window = glfwCreateWindow(1280, 720, "BudgetPilot", NULL, NULL);
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            BudgetSummary summary = buildSummary(data);
            auto advice = buildAdvice(data, summary);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin("Budget Dashboard");

            ImGui::Text("Weekly Income: $%.2f", summary.weeklyIncome);
            ImGui::Text("Weekly Bills: $%.2f", summary.weeklyBills);
            ImGui::Text("Leftover: $%.2f", summary.weeklyLeftover);
            ImGui::Text("Health Score: %d", summary.budgetHealthScore);

            ImGui::Separator();
            ImGui::Text("Advice:");

            for (auto &a : advice)
            {
                ImGui::BulletText("%s", a.c_str());
            }

            if (ImGui::Button("Save"))
            {
                storage.save(data, error);
            }

            ImGui::End();

            ImGui::Render();
            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            glViewport(0, 0, w, h);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

}