# BudgetPilot v18 Core Engine Upgrade

This upgrade builds on v17 and keeps existing functionality while adding a new financial operating system layer.

Added:
- Auto Budget tab with adaptive zero-based style allocation
- Goals Intelligence tab with projected months to goal
- Risk + Alerts tab with higher-level risk signals
- Automation Rules tab with rule suggestions
- Life Planner tab for larger life-change simulations
- new engine modules:
  - financial_engine
  - automation_engine
  - risk_engine

Kept:
- dashboard + charts
- calendar overview
- manage items
- paycheck allocations
- transactions
- debt payoff
- paycheck templates
- history + insights
- scenario planner
- multi-month forecast
- daily cashflow
- optimization + alerts
- bank CSV import + automation rules
- decision simulator
- net worth + accounts

Important:
- I added the new engine layer as source files and UI tabs, but I could not fully compile-test it here because your local build still depends on your local `third_party/imgui` and `third_party/glfw` trees.

## Build

```powershell
cd C:\dev\budget_app\budget_app_v18_core_engine_upgrade
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -S . -B build
cmake --build build --config Release
.\build\Release\BudgetPilot.exe
```

## Save file
data/budget_data.txt
