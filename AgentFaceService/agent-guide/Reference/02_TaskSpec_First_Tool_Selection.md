# 02 - TaskSpec-first Tool Selection

## Tool Catalog Flow

Agent-facing tool and template selection is CLI-owned:

```powershell
bh tools domains --format json
bh tools list <domain> <kind> --format json
bh tools templates <tool_id> --format json
```

After `bh tools templates <tool_id>` returns a template dispatch package, read
only the returned template paths. Do not scan `Templates/` or old semantic
indexes for tool selection.

## 中文

Agent 面向的工具和模板选择由 CLI catalog 负责：

```powershell
bh tools domains --format json
bh tools list <domain> <kind> --format json
bh tools templates <tool_id> --format json
```

选择 `tool_id` 后，只读取 `bh tools templates <tool_id>` 返回的模板路径。不要扫描
`Templates/` 目录来选择工具。

## Default Tool Chain

```text
blueprint_get_runtime_profile
blueprinthelper_read_agent_guide
blueprinthelper_find_assets
blueprinthelper_read_context
blueprinthelper_read_reference_context
blueprinthelper_read_function_chain_context
blueprinthelper_preview_task
blueprinthelper_request_write_session
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_get_debug_case
blueprinthelper_list_debug_cases
blueprinthelper_export_debug_bundle
blueprinthelper_query_review_records
```

Read-only diagnostics:

```text
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
```

`blueprint_open_editor` / `blueprint_close_editor` represent only the global MCP
lifecycle tools for explicitly opening or closing the target Editor. Ordinary
asset reads/writes, TaskSpec preview/execute, diagnostics, and result query use
the CLI. Editor lifecycle compatibility paths also use the global MCP; do not use
CLI lifecycle aliases. CLI lifecycle invocation is blocked. If lifecycle MCP is
unavailable, return `lifecycle_mcp_unavailable`. Deprecated MCP ordinary tools
are not fallback paths.

`blueprinthelper_request_write_session` is the ordinary interactive write
authorization path. Use it after preview and before execute only when write
permission is disabled and the user approves the Editor-side prompt. Approval
applies to the running Editor/Bridge for the approved scope and lifetime, so
SideAgents can execute BlueprintHelper tools after approval without receiving
secret session data. Do not request or pass `BLUEPRINTHELPER_BRIDGE_TOKEN`,
`auth_token`, or `auth_session`.

## Asset Path Routing

- Unknown Unreal `asset_path` -> `blueprinthelper_find_assets`.
- Known Unreal `asset_path` -> `blueprinthelper_read_context`.
- Write requests must resolve one explicit Unreal `asset_path` before `blueprinthelper_preview_task`.
- Do not infer Unreal `asset_path` values from filesystem `.uasset` paths.
- If `blueprinthelper_find_assets` returns multiple candidates, narrow the request or ask for confirmation before writes.

## Task Tool Arguments

`blueprinthelper_preview_task` and `blueprinthelper_execute_task` tool-name
entries prefer the `task_spec` wrapper:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1"
  }
}
```

Grouped `task preview --file` and `task execute --file` commands use a bare
`BlueprintHelper.TaskSpec.v1` file. Do not add an outer `args` wrapper and do
not pass `{ "task_spec": ... }` to grouped commands.

## Context And Anchors

For patch/merge changes to BlueprintHelper-owned blocks, first use
`blueprinthelper_read_context` with `logic_json`, then take these anchors from
the grouped block:

```text
block_id
group_entry_node_path
node_ref
pin_ref
link_ref
```

`append_after` uses `block_id + group_entry_node_path + node_ref + pin_ref`.
`insert_between` must also carry `link_ref`.

Graph body function calls may use `args`; that is not a BlueprintHelper tool
argument wrapper.

## Frozen Boundary

Low-level capability entries are for Task Runtime, tests, and expert diagnostics.
Ordinary Agents do not choose these entries from AgentGuide. When preview is
blocked, stop and report or repair the TaskSpec.

Ordinary Agents do not operate ReviewPanel or expand ReviewRecord internals. When
tool results return `debug_case_ids[]`, a summary-only `blueprinthelper_get_debug_case`
may be used to locate the issue, but Agents must not read DebugBundle artifact
contents, local bundle paths, raw payloads, source content, or full token/settings
files.

`blueprinthelper_apply_review_action` is plugin-development/internal and does not
enter ordinary Agent tool selection.
