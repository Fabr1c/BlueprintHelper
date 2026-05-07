# BlueprintHelper Agent Onboarding Index

普通 Agent 只走 TaskSpec-first 主线。MCP 中仍注册了兼容、测试和专家入口，但这些冻结入口不在本指南中作为可选工具暴露。

默认流程:

```text
blueprint_get_runtime_profile
-> blueprinthelper_read_agent_guide
-> blueprinthelper_read_context or blueprinthelper_read_reference_context
-> build BlueprintHelper.TaskSpec.v1
-> blueprinthelper_preview_task
-> repair TaskSpec or stop_and_report
-> blueprinthelper_execute_task
-> blueprinthelper_get_task_result
-> report summary
```

允许的 Agent-facing 工具:

```text
blueprinthelper_read_agent_guide
blueprint_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
blueprinthelper_read_context
blueprinthelper_read_task_context
blueprinthelper_read_reference_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
```

`blueprint_open_editor` 仅用于用户明确需要启动目标 Unreal Editor 的 preflight，不属于普通写入主线。

阅读顺序:

1. `Resources/AgentGuide/Reference/01_Preflight_And_Boundary.md`
2. `Resources/AgentGuide/Reference/02_TaskSpec_First_Tool_Selection.md`
3. `Resources/AgentGuide/Reference/03_Runtime_Profile_And_Diagnostics.md`
4. `Resources/AgentGuide/Reference/04_MCP_Field_Templates_20260507.md`
5. `Resources/AgentGuide/Workflows/04_TaskSpec_Edit_Blueprint_Workflow.md`
6. `Resources/AgentGuide/Workflows/05_Edit_Blueprint_Workflow.md`
7. `Resources/AgentGuide/Workflows/06_UMG_Data_Workflows.md`
8. `Resources/AgentGuide/Workflows/07_Safety_Validation_And_Recovery.md`

规则:

- Agent 写入 UE 资产时只提交 `BlueprintHelper.TaskSpec.v1`。
- TaskPlan、底层 capability、Bridge command 和冻结工具名不作为普通 Agent 选择项。
- preview blocked 时停止报告或修正 TaskSpec，不回退到冻结入口。
