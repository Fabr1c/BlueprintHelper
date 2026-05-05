# BlueprintHelper Agent Onboarding Index — TaskSpec-first

普通 Agent 默认不直接选择大量底层 MCP 工具。默认流程：

```text
blueprinthelper_get_runtime_profile
→ blueprinthelper_read_agent_guide
→ blueprinthelper_read_context / blueprinthelper_read_reference_context as needed
→ build BlueprintHelper.TaskSpec.v1
→ blueprinthelper_preview_task
→ repair TaskSpec / stop_and_report
→ blueprinthelper_execute_task
→ blueprinthelper_get_task_result
→ report task summary
```

阅读顺序：

1. `Resources/Docs/AgentGuide_TaskSpecFirst_20260504.md`
2. `Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md`
3. `Resources/Plan/BlueprintHelper_Current_Capability_Integration_Smoke_20260505.md`
4. `Resources/Plan/BlueprintHelper_TaskSpec_UE_Smoke_Test_20260504.md`
5. `Resources/AgentGuide/Reference/01_Preflight_And_Boundary.md`
6. `Resources/AgentGuide/Reference/02_TaskSpec_First_Tool_Selection.md`
7. `Resources/AgentGuide/Reference/03_Runtime_Profile_And_Diagnostics.md`
8. `Resources/AgentGuide/Workflows/04_TaskSpec_Edit_Blueprint_Workflow.md`
9. `Resources/Docs/SetupGuide_TaskSpecFirst_20260504.md`

底层 Asset Factory / Component / Class Settings / Graph Write / Validation / Cleanup 工具簇保留为 TaskPlan capability、debug / expert 工具和测试入口。
