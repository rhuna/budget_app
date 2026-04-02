# BudgetPilot Monthly Planner Upgrade

This version adds:

- fuller monthly planning
- calendar-style due dates
- paycheck-by-paycheck forecasting
- UI zoom / scale control to make everything bigger
- monthly fixed, variable, goal, tax, and one-time planning

## Build

Open Developer PowerShell for VS 2022 or x64 Native Tools Command Prompt for VS 2022.

```powershell
cd C:\dev\budget_app\budget_app_monthly_planner_upgrade
Remove-Item -Recurse -Force build
cmake -S . -B build
cmake --build build --config Release
.\build\Release\BudgetPilot.exe
```

## Main tabs

- Monthly Overview
- Calendar
- Paycheck Forecast
- Income + Bills
- Spending + Goals
- Advice

## Notes

Use the Zoom / UI scale slider near the top of the app to enlarge the interface.
