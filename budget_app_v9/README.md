# BudgetPilot v17 Net Worth Upgrade

This upgrade builds on the v16.1 rebuild and adds a real **Net Worth + Accounts** layer.

Added:
- account asset tracking
- asset types like Cash, Savings, Investments
- live net worth calculation
- liquid assets calculation
- net worth trend chart based on saved monthly history
- editable accounts/assets tab

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

Important:
- I could not fully compile-test this here because your local build still depends on your local `third_party/imgui` and `third_party/glfw` trees.

## Build

```powershell
cd C:\dev\budget_app\budget_app_v17_networth_upgrade
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -S . -B build
cmake --build build --config Release
.\build\Release\BudgetPilot.exe
```

## Save file
data/budget_data.txt
