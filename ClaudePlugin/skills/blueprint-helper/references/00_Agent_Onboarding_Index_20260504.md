> 2026-05-17 修订：BlueprintHelper CLI 是普通 TaskSpec/ReadSpec/诊断/结果查询入口；MCP 只保留 editor open、editor close lifecycle 入口。废弃 MCP 普通工具不作为 fallback。

# BlueprintHelper Agent Onboarding Index

If the current Agent environment cannot dispatch a SideAgent but the Main Agent can run the required BlueprintHelper CLI command, the Main Agent may execute one command locally under the SideAgent single-command contract and mark the result as `main_agent_direct_fallback`. Report `tool_unavailable` only when the required BlueprintHelper CLI command is not available.

CLI is the ordinary TaskSpec/read/debug-summary mainline. Global MCP owns Editor lifecycle when an Agent must open or close Unreal Editor. Do not use plugin-local MCP or deprecated MCP ordinary tools for lifecycle or asset workflows.

普通 Agent 只走 CLI TaskSpec-first 主线。废弃 MCP 普通工具即使仍有历史代码，也不在本指南中作为可选工具暴露，不作为 fallback。

默认流程:

```text
blueprint_get_runtime_profile
-> blueprinthelper_read_agent_guide
-> blueprinthelper_read_task_context or blueprinthelper_read_context or blueprinthelper_read_reference_context or blueprinthelper_read_function_chain_context
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
blueprinthelper_read_function_chain_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_get_debug_case
blueprinthelper_list_debug_cases
blueprinthelper_export_debug_bundle
blueprinthelper_query_review_records
```

`blueprinthelper_apply_review_action` is plugin-development/internal and is not part of ordinary Agent-facing templates or workflows.

Write authorization is running Editor/Bridge based: use `blueprinthelper_request_write_session` only after a successful preview when `write_permission` is disabled. The approved scope and lifetime are held by the running Editor, so delegated SideAgents can call BlueprintHelper tools after approval as long as they stay within that scope. The Editor UI is intentionally a minimal accept/reject prompt. If it is rejected, stop and report.

Ordinary Agents must not request, set, or forward `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session`; raw session data is not part of the Agent contract.

Read-only commands such as `bh blueprinthelper_read_context`, `bh blueprinthelper_read_task_context`, `bh blueprinthelper_read_reference_context`, and `bh blueprinthelper_read_function_chain_context` do not require a write session. If these commands are unavailable, diagnose CLI installation, command registration, package build state, or Bridge connectivity instead of requesting write permission.

CLI output is optimized for Agent use. Use `--omit operation,status` when the default summary is useful but envelope fields are not needed. Use `--select` / `--fields` when only a small whitelist is needed, such as `task_run_id`, `summary.target_assets`, or `artifacts.full_result`. Use `--max-bytes` as a hard budget guard; the full payload remains available through the artifact path.

Template-first authoring is available at `AgentFaceService/agent-guide/Templates/README.md`. Prefer copying a matching JSON template, editing placeholders, and calling the CLI with `--file` instead of authoring large JSON directly in shell command strings.

`blueprint_open_editor` / `blueprint_close_editor` 仅用于用户明确需要启动或关闭目标 Unreal Editor 的 lifecycle preflight；Agent 工作流优先调用全局 MCP 生命周期工具。

阅读顺序:

1. `AgentFaceService/agent-guide/Reference/01_Preflight_And_Boundary.md`
2. `AgentFaceService/agent-guide/Reference/02_TaskSpec_First_Tool_Selection.md`
3. `AgentFaceService/agent-guide/Reference/03_Runtime_Profile_And_Diagnostics.md`
4. `AgentFaceService/agent-guide/Reference/04_Tool_Surface_Field_Templates_20260512.md`
5. `AgentFaceService/agent-guide/Templates/README.md`
6. `AgentFaceService/agent-guide/Workflows/04_TaskSpec_Edit_Blueprint_Workflow.md`
7. `AgentFaceService/agent-guide/Workflows/05_Edit_Blueprint_Workflow.md`
8. `AgentFaceService/agent-guide/Workflows/06_UMG_Data_Workflows.md`
9. `AgentFaceService/agent-guide/Workflows/07_Safety_Validation_And_Recovery.md`

规则:

- Agent 写入 UE 资产时只提交 `BlueprintHelper.TaskSpec.v1`。
- TaskPlan、底层 capability、Bridge command 和冻结工具名不作为普通 Agent 选择项。
- preview blocked 时停止报告或修正 TaskSpec，不回退到冻结入口。
- 废弃 MCP 普通工具不作为普通 Agent 可选入口或 fallback。
