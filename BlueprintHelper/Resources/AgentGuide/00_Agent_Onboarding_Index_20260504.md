> 2026-05-14 修订：BlueprintHelper CLI 是唯一正常 Agent-facing 工具入口；MCP 重新废弃冻结，仅保留遗留兼容/排查，不进入普通任务规划。`blueprint_open_editor` 与 `blueprint_close_editor` 均走 CLI。

# BlueprintHelper Agent Onboarding Index

If the current Claude environment cannot dispatch a SideAgent but the Main Agent can run the required BlueprintHelper CLI command, the Main Agent may execute one command locally under the SideAgent single-command contract and mark the result as `main_agent_direct_fallback`. Report `tool_unavailable` only when the required BlueprintHelper CLI command is not available.

CLI is the ordinary TaskSpec/read/debug-summary mainline. MCP is retained for Editor lifecycle, debug, recovery, and commands that need a long-lived host process. Prefer CLI for `blueprint_open_editor` and `blueprint_close_editor`.

普通 Agent 只走 CLI TaskSpec-first 主线。兼容、测试和专家入口可能仍存在于底层传输层，但这些冻结入口不在本指南中作为可选工具暴露。

默认流程:

```text
blueprint_get_runtime_profile
-> blueprinthelper_read_agent_guide
-> blueprinthelper_read_context or blueprinthelper_read_reference_context
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

CLI output is optimized for Agent use. Use `--omit operation,status` when the default summary is useful but envelope fields are not needed. Use `--select` / `--fields` when only a small whitelist is needed, such as `task_run_id`, `summary.target_assets`, or `artifacts.full_result`. Use `--max-bytes` as a hard budget guard; the full payload remains available through the artifact path.

`blueprint_open_editor` 仅用于用户明确需要启动目标 Unreal Editor 的 preflight，不属于普通写入主线。

阅读顺序:

1. `Resources/AgentGuide/Reference/01_Preflight_And_Boundary.md`
2. `Resources/AgentGuide/Reference/02_TaskSpec_First_Tool_Selection.md`
3. `Resources/AgentGuide/Reference/03_Runtime_Profile_And_Diagnostics.md`
4. `Resources/AgentGuide/Reference/04_Tool_Surface_Field_Templates_20260512.md`
5. `Resources/AgentGuide/Workflows/04_TaskSpec_Edit_Blueprint_Workflow.md`
6. `Resources/AgentGuide/Workflows/05_Edit_Blueprint_Workflow.md`
7. `Resources/AgentGuide/Workflows/06_UMG_Data_Workflows.md`
8. `Resources/AgentGuide/Workflows/07_Safety_Validation_And_Recovery.md`

规则:

- Agent 写入 UE 资产时只提交 `BlueprintHelper.TaskSpec.v1`。
- TaskPlan、底层 capability、Bridge command 和冻结工具名不作为普通 Agent 选择项。
- preview blocked 时停止报告或修正 TaskSpec，不回退到冻结入口。
