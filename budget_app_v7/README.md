# BudgetPilot v12 Full Restore Upgrade

This redo restores the functionality that was missing and keeps everything from v11, then adds history + insights.

Included:
- dashboard + charts
- calendar overview
- full manage items editors
- paycheck allocations
- transaction add/delete
- debt payoff strategy + debt editor
- paycheck templates + apply-to-all
- month closeout snapshots
- history + insights charts
- advice tab

## Still needed in third_party
- glfw
- imgui

## Build

```powershell
cd C:\dev\budget_app\budget_app_v12_full_restore_upgrade
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -S . -B build
cmake --build build --config Release
.\build\Release\BudgetPilot.exe
```

## Save file
data/budget_data.txt
