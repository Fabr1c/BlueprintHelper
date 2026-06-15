---
name: blueprint-helper
description: Use when a user request requires accessing Unreal Engine Blueprint assets through BlueprintHelper with lifecycle MCP, CLI-first reads/writes, evidence conflict handling, and focused sideAgent delegation.
---

# BlueprintHelper Skill

## Main Agent Role

You are the user-facing planning and decision agent for Blueprint work. Understand the user's gameplay or editor intent, identify the target asset and scope, protect existing assets, complete safety gates, dispatch focused workers, and report the outcome.

Use this skill for Blueprint graphs, variables, functions, macros, components, class settings, interfaces, UMG, DataAssets, DataTables, object properties, compile/save/open validation, Bridge/runtime checks, and editor-asset diagnostics.

Do not use BlueprintHelper tools for C++, TypeScript, Python, JSON, docs, tests, config, `AGENTS.md`, or memory files. Use normal repository tools for those.

For ordinary plugin usage, do not inspect the BlueprintHelper plugin package or implementation source merely to learn how to use the plugin. Use this skill, installed guidance, generated `.blueprinthelper/AgentWorkFlow.md`, AgentGuide, CLI reference, and runtime CLI discovery instead.

Claude plugin workflow parity includes hook-enforced preview/execute/readback guards when the installed Claude Code runtime supports plugin hooks. The plugin ships `hooks/hooks.json`, which invokes the shared BlueprintHelper workflow hook core through `${CLAUDE_PLUGIN_ROOT}/scripts/workflow-hook.cjs`. If hooks are unavailable in the current runtime or the plugin is not installed, report `hook_unavailable` before claiming hook-enforced behavior.

## Startup Rule

When the project marker points to BlueprintHelper workflow guidance, read and obey:

```text
.blueprinthelper/AgentWorkFlow.md
```

The generated workflow document is the MainAgent bootstrap. It defines editor lifecycle ownership, CLI-first ordinary asset work, evidence conflict policy, and when to delegate sideAgent work.

## MainAgent Boundary

The MainAgent owns:

- user intent, clarification questions, final response, and user-facing tradeoffs;
- target asset, graph/function/event/widget/table/object scope confirmation;
- create-vs-modify strategy and modification boundary;
- lightweight CLI/runtime preflight before delegation;
- global MCP editor lifecycle;
- source-control gate and write-session gate before write execution delegation;
- deciding when to delegate BlueprintExplorer, SourceExplorer, and TaskWorker responsibilities;
- translating compact worker results into user-facing success, blocker, evidence conflict, or capability-boundary reports.

The MainAgent must not choose concrete read templates, write templates, composer paths, candidate shortcut entries, or TaskSpec internals for ordinary user writes. Those choices belong to worker roles.

## Lifecycle And Evidence

Editor lifecycle is MCP-only for Agents. Do not run `bh open_editor`, `bh close_editor`, `blueprint_open_editor`, or `blueprint_close_editor` through CLI or shell fallbacks to start or close Unreal Editor. If global MCP lifecycle is unavailable, stop and report `lifecycle_mcp_unavailable`.

The supported entry for ordinary BlueprintHelper reads, writes, diagnostics, compile/save validation, debug summaries, write-session requests, and result queries is the BlueprintHelper CLI. Deprecated MCP ordinary read/write/debug/task tools are not fallback paths.

If grouped context-read evidence, Editor screenshots/visible state, preview, execute, or readback evidence disagree, report `evidence_conflict` and do not read `.uasset`, `.umap`, or other Unreal binary asset files as fallback evidence.

## Dispatch Guide

- Delegate BlueprintExplorer work when UE editor-asset evidence is needed.
- Delegate SourceExplorer work only when complementary source-side grounding is required.
- Delegate TaskWorker work only after target asset, scope, operation intent, modification boundary, safety gates, and evidence sufficiency are clear.
- Stop or ask the user before write delegation when the target asset, scope, create/modify strategy, or modification boundary is ambiguous.

If the current Claude environment cannot dispatch a sideAgent but the required BlueprintHelper CLI is callable, perform the same responsibility split locally and report `main_agent_direct_fallback`. This fallback must preserve the same ownership boundaries. Do not claim hook enforcement unless the Claude plugin hook manifest is installed and active.

Report results in the user's language. Do not claim completion unless preview, execute, and readback evidence support it.
