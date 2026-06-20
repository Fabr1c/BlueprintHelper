# BlueprintHelper Codex Agent Entry

This package contains the Codex-facing BlueprintHelper plugin metadata, skill instructions, subagent definitions, and lifecycle MCP setup scripts.

Read `skills/blueprint-helper/SKILL.md` before using BlueprintHelper. The supported Agent-facing entry for ordinary TaskSpec reads and writes is the BlueprintHelper CLI under the sibling `AgentFaceService/cli` package. The global MCP endpoint is retained only for editor open/close/modal-dismiss lifecycle in ordinary Agent workflows.

Editor lifecycle is MCP-only for Agents. Do not use `bh open_editor`, `bh close_editor`, `blueprint_open_editor`, `blueprint_close_editor`, `blueprint_dismiss_editor_dialogs`, or `blueprint_close_editor_dialogs` through CLI to start, close, or dismiss modal dialogs in Unreal Editor. If the global MCP lifecycle tools are unavailable; use `mcp__blueprint_helper__blueprint_lifecycle_mcp_status` to diagnose stale lifecycle MCP cache before reporting failure when it is callable, report `lifecycle_mcp_unavailable`.

Deprecated MCP read/write/debug/task tools are not fallback paths for ordinary Agent workflows.

Use normal repository tools for source files, docs, JSON, config, tests, and build scripts. Use BlueprintHelper CLI only for Unreal Editor assets through the running Editor and Bridge.

When the task is ordinary BlueprintHelper plugin usage, do not inspect the BlueprintHelper plugin package or implementation source (`CodexPlugin/`, `ClaudePlugin/`, `AgentFaceService/`, or the UE `BlueprintHelper/` source) to learn how to use it. Use the installed skill instructions, AgentGuide, CLI reference, and templates instead. Reading plugin source is allowed only for explicit BlueprintHelper plugin development, installation repair, or debugging tasks.

## Mandatory Codex Subagents

For any BlueprintHelper editor-asset task, the Main Agent must use the configured Codex subagents:

```text
blueprint-explorer   -> Blueprint/UMG/DataAsset/DataTable context collection
sourcecode-explorer  -> repository source-code/schema/template context collection
task-worker          -> template-first TaskSpec construction, preview, execute, result filtering
```

The Main Agent performs preflight and owns allowed global MCP lifecycle tools. Subagents must not call MCP tools.

For writes, follow the TaskSpec-first closed loop:

```text
main preflight -> explorer context -> task-worker TaskSpec -> preview -> source-control checkout if needed -> write session if needed -> execute -> result -> main-agent next decision
```

In P4/Perforce or other UE source-control projects, run `blueprinthelper_source_control_status` or `blueprinthelper_source_control_checkout` for target assets after preview and before execute when assets may be read-only or save/close reports `checkout_required`. Stop on occupied, conflicted, unavailable, failed checkout, or not-editable states and report the returned agent message.

For complex CLI inputs, use the CLI catalog first:

```powershell
bh tools domains --format json
bh tools list <domain> <kind> --format json
bh tools templates families --workflow preview_execute --format json
bh tools templates compose --template <leaf_template_id> --out <task-spec.json> --format json
```

Follow `families.output.items[].navigation`. Only families that declare a `write_mode`
navigation level, such as `graph_write`, should use `write-modes`; non-GraphWrite
families should compose from the discovered leaf template id. Read only concrete
template paths returned by indexed quick-access/composer output, copy a returned JSON
template when needed, and use `--file`.

Never request or forward raw Bridge auth tokens. Interactive write approval belongs to the running Editor/Bridge session.

