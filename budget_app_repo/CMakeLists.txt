cmake_minimum_required(VERSION 3.22)
project(BudgetPilot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED OFF)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9b
)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(glfw imgui json)

set(IMGUI_SOURCES
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui_lib STATIC ${IMGUI_SOURCES})
target_include_directories(imgui_lib PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)

find_package(OpenGL REQUIRED)
target_link_libraries(imgui_lib PUBLIC OpenGL::GL)

if (WIN32)
  target_compile_definitions(imgui_lib PRIVATE)
endif()

add_executable(BudgetPilot
  src/main.cpp
  src/app.cpp
  src/storage.cpp
  src/advice_engine.cpp
)

target_include_directories(BudgetPilot PRIVATE include)
target_link_libraries(BudgetPilot PRIVATE imgui_lib nlohmann_json::nlohmann_json)

if (WIN32)
  target_link_libraries(BudgetPilot PRIVATE dwmapi)
endif()
