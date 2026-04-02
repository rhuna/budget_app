# BudgetPilot Pro

A desktop C++ budgeting app with a GUI, recurring bills, weekly planning, and practical finance advice.

## Features

- Dear ImGui desktop UI
- Recurring bills with weekly normalization
- Weekly plan categories
- Budget methods:
  - Zero-Based
  - 50/30/20
  - Hybrid
- Budget health score
- Emergency fund and debt tracking
- JSON save/load
- Advice engine with practical guidance

## Build on Windows with Visual Studio

Open **x64 Native Tools Command Prompt for VS 2022** or **Developer PowerShell for VS 2022**.

```powershell
cd C:\dev\budget_app\budget_app_repo_redo
Remove-Item -Recurse -Force build
cmake -S . -B build
cmake --build build --config Release
.\build\Release\BudgetPilot.exe
```

## Build notes

- The project fetches GLFW, Dear ImGui, and nlohmann/json automatically.
- Docs are disabled by default.
- The app auto-saves on exit to `data/budget_data.json`.

## Repo layout

- `include/models.hpp` - data structures
- `include/storage.hpp` - JSON persistence API
- `include/advice_engine.hpp` - budget scoring and advice
- `include/app.hpp` - app entry point
- `src/app.cpp` - GUI
- `src/storage.cpp` - save/load and sample data
- `src/advice_engine.cpp` - logic
- `src/main.cpp` - startup

## Budgeting philosophy

This app combines:
- **Zero-Based budgeting** for strict weekly control
- **50/30/20** for simple percentage-based decisions
- **Hybrid** for users who want both guardrails and intentional planning

## References to use in-app

- CFPB budgeting basics
- FDIC Money Smart
- Investor.gov saving and investing basics
