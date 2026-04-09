cmake_minimum_required(VERSION 3.20)
project(BudgetPilot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp")
    message(FATAL_ERROR "Local ImGui not found. Put Dear ImGui in third_party/imgui/")
endif()

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/CMakeLists.txt")
    message(FATAL_ERROR "Local GLFW not found. Put GLFW source in third_party/glfw/")
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/glfw)

add_library(imgui_lib
  third_party/imgui/imgui.cpp
  third_party/imgui/imgui_draw.cpp
  third_party/imgui/imgui_tables.cpp
  third_party/imgui/imgui_widgets.cpp
  third_party/imgui/backends/imgui_impl_glfw.cpp
  third_party/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_lib PUBLIC
  third_party/imgui
  third_party/imgui/backends
  third_party/glfw/include
)

target_link_libraries(imgui_lib PUBLIC glfw)

add_executable(BudgetPilot
  src/main.cpp
  src/app.cpp
  src/storage.cpp
  src/advice_engine.cpp
  src/financial_engine.cpp
  src/automation_engine.cpp
  src/risk_engine.cpp
)

target_include_directories(BudgetPilot PRIVATE include)
target_link_libraries(BudgetPilot PRIVATE imgui_lib OpenGL::GL glfw)

if (MSVC)
  target_compile_options(BudgetPilot PRIVATE /W4 /permissive-)
else()
  target_compile_options(BudgetPilot PRIVATE -Wall -Wextra -Wpedantic)
endif()
