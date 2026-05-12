# BlueprintHelper Agent Onboarding Index

普通 Agent 只走 TaskSpec-first 主线。兼容、测试和专家入口可能仍存在于底层传输层，但这些冻结入口不在本指南中作为可选工具暴露。

主 Agent 命中 BlueprintHelper Skill 后，身份是面向用户的意图理解、目标确认和安全决策者。先确认目标资产、目标范围和创建/修改策略；需要实际调用 BlueprintHelper 工具时，给 SideAgent 一个语义化精简任务包，并让 SideAgent 读取 `09_SideAgent_Tool_Execution.md`。不要把完整对话或完整 Skill 原文传给 SideAgent。

如果当前 Claude 环境无法分派 SideAgent，但主 Agent 当前能运行所需的 BlueprintHelper CLI 命令，主 Agent 可以按 SideAgent 的单命令契约直接执行一次，并在结果中标记 `main_agent_direct_fallback`。只有当所需 BlueprintHelper CLI 命令本身不可用时，才报告 `tool_unavailable`。

默认流程:

```text
blueprint_get_runtime_profile
-> blueprinthelper_read_agent_guide
-> blueprinthelper_read_context summary or bounded structured read to estimate graph size
-> if graph has more than 80 nodes, use block-scoped or target-scoped reads instead of whole-graph logic_md
-> blueprinthelper_read_context or blueprinthelper_read_reference_context for the selected slice
-> build BlueprintHelper.TaskSpec.v1
-> blueprinthelper_preview_task
-> repair TaskSpec or stop_and_report
-> blueprinthelper_request_write_session if write_permission is disabled and the user accepts the simple Editor approval dialog
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
blueprinthelper_request_write_session
blueprinthelper_read_context
blueprinthelper_read_task_context
blueprinthelper_read_reference_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
```

Write authorization is running Editor/Bridge based: use `blueprinthelper_request_write_session` only after a successful preview when `write_permission` is disabled. The approved scope and lifetime are held by the running Editor, so delegated SideAgents can call BlueprintHelper tools after approval as long as they stay within that scope. The Editor UI is intentionally a minimal accept/reject prompt. If it is rejected, stop and report.

Ordinary Agents must not request, set, or forward `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session`; raw session data is not part of the Agent contract.

Read-only commands such as `bh blueprinthelper_read_context`, `bh blueprinthelper_read_task_context`, and `bh blueprinthelper_read_reference_context` do not require a write session. If these commands are unavailable, diagnose CLI installation, command registration, package build state, or Bridge connectivity instead of requesting write permission.

`blueprint_open_editor` 仅用于用户明确需要启动目标 Unreal Editor 的 preflight，不属于普通写入主线。

阅读顺序:

1. `references/01_Preflight_And_Boundary.md`
2. `references/02_TaskSpec_First_Tool_Selection.md`
3. `references/03_Runtime_Profile_And_Diagnostics.md`
4. `references/04_Tool_Surface_Field_Templates_20260512.md`
5. `references/09_SideAgent_Tool_Execution.md`
6. `references/04_TaskSpec_Edit_Blueprint_Workflow.md`
7. `references/05_Edit_Blueprint_Workflow.md`
8. `references/06_UMG_Data_Workflows.md`
9. `references/07_Safety_Validation_And_Recovery.md`

规则:

- Agent 写入 UE 资产时只提交 `BlueprintHelper.TaskSpec.v1`。
- TaskPlan、底层 capability、Bridge command 和冻结工具名不作为普通 Agent 选择项。
- preview blocked 时停止报告或修正 TaskSpec，不回退到冻结入口。
- 不清楚图表大小时，不要直接读取整个图表的 `logic_md`。先用 `summary` 或带 `max_items` 的结构化读取估算节点数量；如果节点数大于 80，采用目标范围、block 或引用影响面的分块读取策略。
