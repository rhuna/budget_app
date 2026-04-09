# BudgetPilot v13 Scenario Planner + Multi-Month Forecast

This upgrade builds on v12 and adds:
- scenario planner
- named what-if scenarios
- side-by-side scenario results
- 3 to 12 month forecast
- projected checking chart
- projected debt chart
- forecast detail tab

It keeps:
- dashboard + charts
- calendar overview
- manage items editors
- paycheck allocations
- transactions
- debt payoff strategy
- paycheck templates
- history + insights
- plain text save file

## Still needed in third_party
- glfw
- imgui

## Build

```powershell
cd C:\dev\budget_app\budget_app_v13_scenario_forecast_upgrade
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -S . -B build
cmake --build build --config Release
.\build\Release\BudgetPilot.exe
```

## Save file
data/budget_data.txt
