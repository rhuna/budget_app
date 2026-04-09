# BudgetPilot v11 - V9 Plus Debt Payoff + Paycheck Templates

This build keeps the v9 structure and adds:
- debt payoff strategy view
- avalanche vs snowball selector
- debt account editor
- recommended debt target
- projected monthly interest
- estimated months to primary payoff
- paycheck templates
- apply template to all paycheck windows

It keeps:
- v9 dashboard + charts
- v9 calendar overview
- manage items
- paycheck allocations
- transaction ledger
- plain text save file

## Still needed in third_party
- glfw
- imgui

## Build

```powershell
cd C:\dev\budget_app\budget_app_v11_v9_plus_debt_templates
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -S . -B build
cmake --build build --config Release
.\build\Release\BudgetPilot.exe
```

## Save file
data/budget_data.txt
