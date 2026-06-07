> 2026-05-17 修订：BlueprintHelper CLI 是普通 TaskSpec/ReadSpec/诊断/结果查询入口；MCP 只保留 editor open、editor close lifecycle 入口。废弃 MCP 普通工具不作为 fallback。

# BlueprintHelper Agent Onboarding Index

SideAgent unavailability is controlled by the active installed plugin skill, not by this shared guide. Codex must report `sideagent_unavailable` when required Codex sideAgents cannot be dispatched and must not use local Main Agent execution as a fallback. Claude may use `main_agent_direct_fallback` only when the Claude skill explicitly permits a single-command fallback under the SideAgent contract. Report `tool_unavailable` only when the required BlueprintHelper CLI command is not installed or callable.

CLI is the ordinary TaskSpec/read/debug-summary mainline. Global MCP owns Editor lifecycle when an Agent must open or close Unreal Editor. Do not use plugin-local MCP or deprecated MCP ordinary tools for lifecycle or asset workflows.

普通 Agent 只走 CLI TaskSpec-first 主线。废弃 MCP 普通工具即使仍有历史代码，也不在本指南中作为可选工具暴露，不作为 fallback。

默认流程:

```text
blueprint_get_runtime_profile
-> blueprinthelper_read_agent_guide
-> blueprinthelper_find_assets when the Unreal asset_path is unknown
-> blueprinthelper_read_context or blueprinthelper_read_reference_context or blueprinthelper_read_function_chain_context
-> build BlueprintHelper.TaskSpec.v1
-> blueprinthelper_preview_task
-> repair TaskSpec or stop_and_report
-> blueprinthelper_source_control_status/checkout if source control requires checkout or close/save reports checkout_required
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
blueprinthelper_source_control_status
blueprinthelper_source_control_checkout
blueprinthelper_find_assets
blueprinthelper_read_context
blueprinthelper_read_context_capabilities
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

Source-control checkout is a separate pre-write gate. In P4/Perforce or other UE source-control projects, use `blueprinthelper_source_control_status` or `blueprinthelper_source_control_checkout` after preview and before execute when target assets may be read-only or when close/save reports `checkout_required`. Stop on `checked_out_by_other`, `source_control_conflicted`, `source_control_unavailable`, `checkout_failed`, or `not_editable` and report the returned agent message.

Ordinary Agents must not request, set, or forward `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session`; raw session data is not part of the Agent contract.

Read-only commands such as `bh blueprinthelper_find_assets`, `bh blueprinthelper_read_context`, `bh blueprinthelper_read_context_capabilities`, `bh blueprinthelper_read_reference_context`, and `bh blueprinthelper_read_function_chain_context` do not require a write session. If these commands are unavailable, diagnose CLI installation, command registration, package build state, or Bridge connectivity instead of requesting write permission.

When the Unreal `asset_path` is unknown, call `bh blueprinthelper_find_assets` first. When the Unreal `asset_path` is already known, go directly to `bh blueprinthelper_read_context`. Do not infer Unreal `asset_path` values from filesystem `.uasset` paths. If multiple candidates are returned, narrow the request or ask for confirmation before any write flow. A write request must resolve one explicit Unreal `asset_path` before `blueprinthelper_preview_task`.

CLI output is optimized for Agent use. Use `--omit operation,status` when the default summary is useful but envelope fields are not needed. Use `--select` / `--fields` when only a small whitelist is needed, such as `task_run_id`, `summary.target_assets`, or `artifacts.full_result`. Use `--max-bytes` as a hard budget guard; the full payload remains available through the artifact path.

Agent-facing tool and template selection is CLI-owned:

```powershell
bh tools domains --format json
bh tools list <domain> <kind> --format json
bh tools templates <tool_id> --format json
```

After `bh tools templates <tool_id>` returns a template dispatch package, read only the returned template paths. Do not scan `Templates/` or old semantic indexes for tool selection. Prefer copying the returned JSON template, editing placeholders, and calling the CLI with `--file` instead of authoring large JSON directly in shell command strings.

Agent 面向的工具和模板选择由 CLI catalog 负责。先选择 domain/kind/tool_id，再只读取 `bh tools templates <tool_id>` 返回的具体模板路径；不要扫描 `Templates/` 目录来选择工具。

`blueprint_open_editor` / `blueprint_close_editor` 指全局 MCP lifecycle 工具，不是 CLI direct tool。用户明确需要启动或关闭目标 Unreal Editor 时，统一调用 `mcp__blueprint_helper__blueprint_open_editor` / `mcp__blueprint_helper__blueprint_close_editor`；不要通过 CLI lifecycle alias 做兼容路径。
如果全局 MCP lifecycle 工具不可用，返回 `lifecycle_mcp_unavailable`；不要改用 `bh open_editor` / `bh close_editor` 或 direct CLI lifecycle 命令启动/关闭 Editor。

阅读顺序:

1. `AgentFaceService/agent-guide/Reference/01_Preflight_And_Boundary.md`
2. `AgentFaceService/agent-guide/Reference/03_Runtime_Profile_And_Diagnostics.md`
3. `AgentFaceService/agent-guide/Reference/05_UE_Blueprint_Write_Architecture_Rules.md`
4. `AgentFaceService/agent-guide/Reference/06_UE_Blueprint_Write_CodingStyle.md`
5. `AgentFaceService/agent-guide/Reference/07_LogicFlow_Syntax_Rules.md`
6. `AgentFaceService/docs/TaskSpec_CLI_QuickStart.md`
7. `AgentFaceService/agent-guide/Workflows/04_TaskSpec_Edit_Blueprint_Workflow.md`
8. Use `bh tools domains/list/templates` and `bh tools templates quick-access/compose` to build one concrete CLI input when a task requires TaskSpec or ReadSpec JSON.
9. `AgentFaceService/agent-guide/Workflows/05_Edit_Blueprint_Workflow.md`
10. `AgentFaceService/agent-guide/Workflows/06_UMG_Data_Workflows.md`
11. `AgentFaceService/agent-guide/Workflows/07_Safety_Validation_And_Recovery.md`

规则:

- Agent 写入 UE 资产时只提交 `BlueprintHelper.TaskSpec.v1`。
- TaskPlan、底层 capability、Bridge command 和冻结工具名不作为普通 Agent 选择项。
- preview blocked 时停止报告或修正 TaskSpec，不回退到冻结入口。
- 废弃 MCP 普通工具不作为普通 Agent 可选入口或 fallback。
Additional asset-path routing rules:

- Unknown Unreal `asset_path` -> `blueprinthelper_find_assets`; known Unreal `asset_path` -> `blueprinthelper_read_context`.
- Write requests must resolve one explicit Unreal `asset_path` before `blueprinthelper_preview_task`.
- Do not infer Unreal `asset_path` values from filesystem `.uasset` paths.
- If `blueprinthelper_find_assets` returns multiple candidates, narrow the request or ask for confirmation before writes.
