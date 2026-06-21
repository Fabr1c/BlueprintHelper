# Claude SideAgent Adapter

Use these rules when porting the Codex mandatory subagent workflow to Claude Code.

## Mandatory sideAgents

For any BlueprintHelper editor-asset task, the Main Agent must delegate context gathering and TaskSpec execution to static Claude sideAgents instead of constructing long ad-hoc subagent prompts.

SideAgents:

```text
blueprint-explorer   -> Blueprint/UMG/DataAsset/DataTable/editor-asset context collection
sourcecode-explorer  -> repository source-code/schema/template context collection
sourcecode-worker    -> architecture-approved C++/DataAsset/interface source edits and verification
task-worker          -> template-first TaskSpec construction, preview, execute, result filtering
```

Default sideAgent profiles are configured with:

```yaml
blueprint-explorer:
  model: haiku
  reasoning: high
sourcecode-explorer:
  model: haiku
  reasoning: high
sourcecode-worker:
  model: opus
  reasoning: high
task-worker:
  model: sonnet
  reasoning: high
```

Claude agent frontmatter uses `model: haiku` for explorer sideAgents, `model: opus` for `sourcecode-worker`, and `model: sonnet` for `task-worker`. Reasoning depth is expressed in the sideAgent instructions and compact task package as `reasoning: high`; do not add undocumented frontmatter fields unless the installed Claude Code version explicitly supports them.

## Main Agent ownership

The Main Agent owns:

- user intent and clarification questions;
- target asset, graph, widget, table, or object scope confirmation;
- safety decisions and write boundary decisions;
- project/editor preflight;
- Bridge/runtime availability checks;
- global MCP editor lifecycle tools;
- final user response;
- closed-loop decisions after sideAgent results.

Only the Main Agent may call:

```text
mcp__blueprint_helper__blueprint_open_editor
mcp__blueprint_helper__blueprint_close_editor
mcp__blueprint_helper__blueprint_dismiss_editor_dialogs
mcp__blueprint_helper__blueprint_close_editor_dialogs
```

SideAgents must not call MCP tools.

## Dispatch budget

For each user request, dispatch at most:

- one `blueprint-explorer`;
- one `sourcecode-explorer`;
- one `sourcecode-worker` when the Main Agent architecture gate requires source edits before Blueprint wiring;
- one `task-worker` per preview/execute attempt.

If `task-worker` returns a failure, the Main Agent may dispatch one corrected package or one bounded additional context request. Do not start broad recursive exploration.

## Token-saving rule

The Main Agent should never paste the sideAgent role/instructions into the task. It should pass only the compact task package. Role, constraints, allowed commands, output contract, and model policy live in `agents/*.md`.

## Failure rule

If Claude cannot dispatch sideAgents, stop and report:

```text
sideagent_unavailable
```

Do not silently fall back to local Main Agent execution for BlueprintHelper editor-asset writes unless the user explicitly disables sideAgents.

