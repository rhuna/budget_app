# BudgetPilot v21 UI Overhaul

This upgrade builds on v20 and focuses on readability, click targets, and overall usability.

Added / improved:
- larger summary cards
- bigger money rows
- bigger top action buttons
- stronger live UI scaling
- larger spacing and padding
- larger scrollbars and grab handles
- dedicated **UI + Accessibility** tab
- larger default main window size

Kept:
- full editors
- dashboard + charts
- calendar overview
- transactions
- debt payoff
- history + insights
- scenario planner
- multi-month forecast
- daily cashflow
- optimization + alerts
- bank CSV import
- decision simulator
- net worth + accounts
- auto budget
- goals intelligence
- risk + alerts
- automation rules
- life planner
- execution plan

Important:
- I could not fully compile-test this here because your local build still depends on your local `third_party/imgui` and `third_party/glfw` trees.

## Build

```powershell
cd C:\dev\budget_app\budget_app_v21_ui_overhaul
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -S . -B build
cmake --build build --config Release
.\build\Release\BudgetPilot.exe
```

## Save file
data/budget_data.txt
