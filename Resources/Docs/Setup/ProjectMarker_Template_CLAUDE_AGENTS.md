# BlueprintHelper Project Marker Template

This project uses BlueprintHelper for Unreal Editor asset operations.

Agent default workflow:

```text
get_runtime_profile → read_task_context → build TaskSpec → preview_task → execute_task → report summary
```

Do not use BlueprintHelper MCP to edit C++ / Config / Build.cs / Target.cs files. Use BlueprintHelper only for Unreal Editor asset operations.

Read:

- `BlueprintHelper/Resources/Docs/AgentGuide_TaskSpecFirst_20260504.md`
- `BlueprintHelper/Resources/Docs/SetupGuide_TaskSpecFirst_20260504.md`
