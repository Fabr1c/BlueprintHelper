> 2026-05-17 修订：BlueprintHelper CLI 是普通 TaskSpec/ReadSpec/诊断/结果查询入口；MCP 只保留 editor open、editor close lifecycle 入口。废弃 MCP 普通工具不作为 fallback。

# BlueprintHelper Agent Onboarding Index

SideAgent unavailability is controlled by the active installed plugin skill, not by this shared guide. Codex must report `sideagent_unavailable` when required Codex sideAgents cannot be dispatched and must not use local Main Agent execution as a fallback. Claude may use `main_agent_direct_fallback` only when the Claude skill explicitly permits a single-command fallback under the SideAgent contract. Report `tool_unavailable` only when the required BlueprintHelper CLI command is not installed or callable.

CLI is the ordinary TaskSpec/read/debug-summary mainline. Global MCP owns Editor lifecycle when an Agent must open or close Unreal Editor. Do not use plugin-local MCP or deprecated MCP ordinary tools for lifecycle or asset workflows.

普通 Agent 只走 CLI TaskSpec-first 主线。废弃 MCP 普通工具即使仍有历史代码，也不在本指南中作为可选工具暴露，不作为 fallback。

如果 `read_context`、截图/Editor 画面、preview、execute 或 readback 证据冲突，返回 `evidence_conflict` 并 stop_and_report。不要读取 `.uasset`、`.umap` 或其它 UE 二进制资产文件作为 fallback 事实源。

默认流程:

```text
blueprint_get_runtime_profile
-> blueprinthelper_read_agent_guide
-> practical C++ plus Blueprint architecture gate before write dispatch
-> blueprinthelper_find_assets when the Unreal asset_path is unknown
-> bh context read / blueprinthelper_read_reference_context / blueprinthelper_read_function_chain_context as needed
-> evidence_conflict means stop_and_report, not binary fallback
-> build BlueprintHelper.TaskSpec.v1
-> bh task preview --file <task-spec.json>
-> repair TaskSpec or stop_and_report
-> blueprinthelper_source_control_status/checkout if source control requires checkout or close/save reports checkout_required
-> blueprinthelper_request_write_session if write_permission is disabled and the user accepts the simple Editor approval dialog
-> bh task execute --file <task-spec.json> --preview-token <preview_token>
-> bh task result --id <task_run_id>
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
bh context read --file <read-spec.json> | --stdin
bh tools read-templates families --format json
bh tools read-templates clusters --family blueprint --format json
bh tools read-templates list --family blueprint --cluster logic --format json
bh tools read-templates compose --template blueprint.logic.function.flow --out .tmp/readspec-template-composer/blueprint_function_logic_flow.readspec.json --format json
bh tools read-templates compose --template blueprint.logic.function.json_delta --out .tmp/readspec-template-composer/blueprint_function_logic_delta.readspec.json --format json
blueprinthelper_read_reference_context
blueprinthelper_read_function_chain_context
bh task preview --file <task-spec.json>
bh task execute --file <task-spec.json> --preview-token <preview_token>
bh task result --id <task_run_id>
blueprinthelper_get_debug_case
blueprinthelper_list_debug_cases
blueprinthelper_export_debug_bundle
blueprinthelper_query_review_records
```

`blueprinthelper_apply_review_action` is plugin-development/internal and is not part of ordinary Agent-facing templates or workflows.

Write authorization is running Editor/Bridge based: use `blueprinthelper_request_write_session` only after a successful preview when `write_permission` is disabled. The approved scope and lifetime are held by the running Editor, so delegated SideAgents can call BlueprintHelper tools after approval as long as they stay within that scope. The Editor UI is intentionally a minimal accept/reject prompt. If it is rejected, stop and report.

Source-control checkout is a separate pre-write gate. In P4/Perforce or other UE source-control projects, use `blueprinthelper_source_control_status` or `blueprinthelper_source_control_checkout` after preview and before execute when target assets may be read-only or when close/save reports `checkout_required`. Stop on `checked_out_by_other`, `source_control_conflicted`, `source_control_unavailable`, `checkout_failed`, or `not_editable` and report the returned agent message.

Ordinary Agents must not request, set, or forward `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session`; raw session data is not part of the Agent contract.

Read-only commands such as `bh blueprinthelper_find_assets`, `bh context read`, `bh tools read-templates ...`, `bh blueprinthelper_read_reference_context`, and `bh blueprinthelper_read_function_chain_context` do not require a write session. If these commands are unavailable, diagnose CLI installation, command registration, package build state, or Bridge connectivity instead of requesting write permission.

When the Unreal `asset_path` is unknown, call `bh blueprinthelper_find_assets` first. When the Unreal `asset_path` is already known, compose a ReadSpec and call `bh context read`. Do not infer Unreal `asset_path` values from filesystem `.uasset` paths. If multiple candidates are returned, narrow the request or ask for confirmation before any write flow. A write request must resolve one explicit Unreal `asset_path` before `bh task preview`.

CLI output is optimized for Agent use. Use `--omit operation,status` when the default summary is useful but envelope fields are not needed. Use `--select` / `--fields` when only a small whitelist is needed, such as `task_run_id`, `summary.target_assets`, or `artifacts.full_result`. Use `--max-bytes` as a hard budget guard; the full payload remains available through the artifact path.

Agent-facing tool and template selection is CLI-owned:

```powershell
bh tools domains --format json
bh tools list <domain> <kind> --format json
bh tools templates families --workflow preview_execute --format json
bh tools read-templates families --format json
```

After choosing the capability, use the TaskSpec composer or flat ReadSpec composer navigation to select a concrete `template_id` and compose a temporary JSON file. Do not scan `Templates/` or old semantic indexes for tool selection. Prefer composer output, editing placeholders, and calling the CLI with `--file` instead of authoring large JSON directly in shell command strings.

When `logic_flow` has already been read for a Blueprint logic target and the next missing data is GraphWrite location evidence, anchors, pins, links, or boundary identity, choose the same-target `*.json_delta` ReadSpec template and run `bh context read` with `view.format=logic_json_delta_after_logic_flow`. If the delta output lacks required write-location fields, stop with `missing_capability`; if it conflicts with the earlier `logic_flow` or visible/readback evidence, stop with `evidence_conflict`. Do not use UE binary asset reads as fallback.

Agent 面向的工具和模板选择由 CLI catalog 负责。先选择 domain/kind/capability，再通过 TaskSpec composer 或 ReadSpec composer 选择 quick-access 并生成临时 JSON；不要扫描 `Templates/` 目录来选择工具。

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
8. Use literal CLI discovery commands, not slash-joined shorthand: `bh tools domains --format json` and `bh tools list <domain> <kind> --format json` for catalog discovery; `bh tools templates families --workflow preview_execute --format json`, `bh tools templates quick-access --family graph_write --cluster generic_ops --operation let --write-mode graph.append --format json`, and `bh tools templates compose --family graph_write --write-mode graph.append --templates "generic_ops.let.default(generic_ops.expression.literal)" --out .tmp/taskspec-template-composer/graph_append.taskspec.json --format json` for TaskSpec template composition.
9. `AgentFaceService/agent-guide/Workflows/05_Edit_Blueprint_Workflow.md`
10. `AgentFaceService/agent-guide/Workflows/06_UMG_Data_Workflows.md`
11. `AgentFaceService/agent-guide/Workflows/07_Safety_Validation_And_Recovery.md`

规则:

- Agent 写入 UE 资产时只提交 `BlueprintHelper.TaskSpec.v1`。
- TaskPlan、底层 capability、Bridge command 和冻结工具名不作为普通 Agent 选择项。
- preview blocked 时停止报告或修正 TaskSpec，不回退到冻结入口。
- 废弃 MCP 普通工具不作为普通 Agent 可选入口或 fallback。
- 证据冲突时只允许 `stop_and_report`；不要把 `.uasset`、`.umap` 或其它 UE 二进制资产文件当作 fallback 事实源。
Additional asset-path routing rules:

- Before UE asset writes, decide whether complex logic belongs in C++ and whether `sourcecode-worker` must implement source contracts first.
- Unknown Unreal `asset_path` -> `blueprinthelper_find_assets`; known Unreal `asset_path` -> compose a ReadSpec and run `bh context read`.
- Write requests must resolve one explicit Unreal `asset_path` before `bh task preview`.
- Do not infer Unreal `asset_path` values from filesystem `.uasset` paths.
- If `blueprinthelper_find_assets` returns multiple candidates, narrow the request or ask for confirmation before writes.
