# BudgetPilot (C++ GUI Budget App)

BudgetPilot is a desktop budgeting app written in C++ with a Dear ImGui GUI.

## What it does
- Tracks **weekly income sources**
- Lets you add **recurring bills** with weekly, bi-weekly, monthly, quarterly, or yearly frequencies
- Converts recurring bills into a **weekly equivalent** so you can plan week by week
- Lets you build a **weekly spending plan** with categories like groceries, gas, eating out, and fun money
- Tracks **emergency fund**, **checking**, **debt**, and **savings goals**
- Generates **educational finance guidance** based on your data
- Saves your budget to `data/budget_data.json`

## Budgeting technique used
This repo uses a **zero-based budgeting** workflow for weekly planning.
That means every dollar of weekly income should be assigned a job:
- recurring bills
- essentials
- discretionary categories
- debt payoff
- emergency savings
- investing

Why this is a good fit:
- It works well with recurring bills
- It makes weekly planning easier
- It shows when you are over-committed before the week happens
- It supports intentional tradeoffs instead of vague monthly guesses

The app also displays guidance around:
- emergency fund progress
- savings rate
- essential-cost pressure
- debt payoff consistency

## Tech stack
- C++20
- CMake
- Dear ImGui
- GLFW
- OpenGL
- nlohmann/json

## Build
### Windows (Visual Studio generator)
```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\BudgetPilot.exe
```

### Windows (MinGW Makefiles)
```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
.\build\BudgetPilot.exe
```

### Linux
```bash
cmake -S . -B build
cmake --build build
./build/BudgetPilot
```

## Notes
- First launch creates sample data if no file exists yet.
- Data is stored in plain JSON for easy debugging.
- Advice in the app is educational and not a replacement for a CFP, CPA, attorney, or tax professional.

## Good next upgrades
- charts for category trends
- calendar-style bill due view
- CSV export/import
- bank import (OFX/CSV)
- bill reminders
- multiple budget profiles
- debt snowball vs avalanche planner
- sinking funds for annual expenses
