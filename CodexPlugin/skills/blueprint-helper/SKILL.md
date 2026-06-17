---
name: blueprint-helper
description: Use for BlueprintHelper UE editor-asset workflows: lifecycle MCP, CLI-first reads/writes, evidence conflict handling, and dispatching BlueprintHelper sideAgents.
---

# BlueprintHelper For Codex

## Role

Use this skill when a user asks Codex to work with Unreal Editor assets through BlueprintHelper, including Blueprint graphs, variables, functions, macros, components, class settings, interfaces, UMG widgets, DataAssets, DataTables, object values, compile/save/open validation, Bridge/runtime checks, or editor-asset diagnostics.

Do not use BlueprintHelper for normal repository files. Use normal Codex shell and edit tools for C++, TypeScript, Python, JSON, config, docs, tests, build scripts, and source search.

For ordinary plugin usage, do not inspect the BlueprintHelper plugin package or implementation source merely to learn how to use the plugin. Use this skill, installed BlueprintHelper agent skills, generated `.blueprinthelper/AgentWorkFlow.md`, AgentGuide, CLI reference, and runtime CLI discovery instead.

## Startup Rule

When the project marker points to BlueprintHelper workflow guidance, read and obey:

```text
.blueprinthelper/AgentWorkFlow.md
```

The generated workflow document is the MainAgent bootstrap. It defines editor lifecycle ownership, CLI-first ordinary asset work, evidence conflict policy, and when to dispatch sideAgents.

## MainAgent Boundary

The MainAgent owns:

- user intent, clarification questions, final response, and user-facing tradeoffs;
- target asset, graph/function/event/widget/table/object scope confirmation;
- create-vs-modify strategy and modification boundary;
- lightweight CLI/runtime preflight before dispatch;
- global MCP editor lifecycle;
- source-control gate and write-session gate before write execution delegation;
- deciding whether to dispatch `blueprint-explorer`, `sourcecode-explorer`, or `task-worker`;
- interpreting compact sideAgent results into user-facing success, blocker, evidence conflict, or capability-boundary reports.

The MainAgent must not choose concrete read templates, write templates, composer paths, candidate shortcut entries, or TaskSpec internals for ordinary user writes. Those choices belong to the sideAgent roles.

## Lifecycle And Evidence

Call editor lifecycle only through global MCP tools:

```text
mcp__blueprint_helper__blueprint_open_editor
mcp__blueprint_helper__blueprint_close_editor
mcp__blueprint_helper__blueprint_dismiss_editor_dialogs
mcp__blueprint_helper__blueprint_close_editor_dialogs
```

Do not run `bh open_editor`, `bh close_editor`, `blueprint_open_editor`, `blueprint_close_editor`, `blueprint_dismiss_editor_dialogs`, or `blueprint_close_editor_dialogs` through CLI or shell fallbacks to start, close, or dismiss modal dialogs in Unreal Editor. If global MCP lifecycle is unavailable, stop and report `lifecycle_mcp_unavailable`.

The supported entry for ordinary BlueprintHelper reads, writes, diagnostics, compile/save validation, write-session requests, and result queries is the BlueprintHelper CLI. Deprecated MCP ordinary read/write/debug/task tools are forbidden for Agent workflows. Do not use them as fallback.

If read evidence, Editor screenshots/visible state, preview, execute, or readback evidence disagree, report `evidence_conflict` and do not inspect `.uasset`, `.umap`, or other Unreal binary asset files as fallback evidence.

## Dispatch Guide

- Dispatch `blueprint-explorer` when UE editor-asset evidence is needed.
- Dispatch `sourcecode-explorer` only when complementary source-side grounding is required.
- Dispatch `task-worker` only after target asset, scope, operation intent, modification boundary, safety gates, and evidence sufficiency are clear.
- Stop or ask the user before write delegation when the target asset, scope, create/modify strategy, or modification boundary is ambiguous.
- If a sideAgent wait times out after an already long wait, do not close the sideAgent directly. Send a progress check asking for current status, blockers, and whether it can be closed; wait once more for a bounded interval. Close only after the sideAgent completes, explicitly reports it can be closed, or reaches an errored/shutdown status. If the progress check also times out, report `sideagent_timeout_unconfirmed` to the user and ask whether to keep waiting or close it.

Report results in the user's language. Do not claim completion unless preview, execute, and readback evidence support it.
