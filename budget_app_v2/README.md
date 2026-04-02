# BudgetPilot Real-Time Upgrade

This version adds a live budget planner that recalculates immediately as you add or edit:

- income sources
- recurring bills
- variable weekly spending
- goal buckets / sinking funds
- one-time expenses
- weekly tax reserve
- balances for checking, emergency fund, debt, and savings target

## Build

Open Developer PowerShell for VS 2022 or x64 Native Tools Command Prompt for VS 2022.

```powershell
cd C:\dev\budget_app\budget_app_realtime_upgrade
Remove-Item -Recurse -Force build
cmake -S . -B build
cmake --build build --config Release
.\build\Release\BudgetPilot.exe
```

## What it accounts for

The live budget view is designed to account for:

- take-home income
- fixed recurring bills
- variable weekly spending
- sinking funds / savings goals
- one-time expenses this week
- weekly tax reserve
- emergency fund coverage
- debt pressure
