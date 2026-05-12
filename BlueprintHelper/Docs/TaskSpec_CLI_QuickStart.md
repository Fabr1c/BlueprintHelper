# BlueprintHelper TaskSpec CLI QuickStart

This guide documents the optional TaskSpec CLI entry for shell-capable Agents.

Task orchestration mainline: Agent -> TaskSpec CLI -> Python Task Compiler -> Bridge preview/execute -> UE Task Runtime -> Existing Capability Clusters.

## Purpose

Use the CLI when an Agent can run shell commands and should avoid large MCP escaped JSON output. The CLI is an alternate Agent entry, not a replacement for the MCP task tools. It preserves TaskSpec-first orchestration and the same Python compilation, Bridge preview, and UE Task Runtime execution path.

## Prerequisites

- Complete the setup in [Install_MCP_QuickStart.md](Install_MCP_QuickStart.md).
- Build `task-core` and the CLI package so the CLI entry exists under `ClaudePlugin/cli/build/cli/`.
- Start Unreal Editor with BlueprintHelper loaded and the Bridge reachable.
- Prepare a `BlueprintHelper.TaskSpec.v1` file such as `.\task_spec.json`.

## Build

```powershell
cd <PLUGIN_ROOT>\ClaudePlugin\task-core
npm install
npm run build

cd <PLUGIN_ROOT>\ClaudePlugin\cli
npm install
npm run build
```

## Preview

Run preview first. This compiles `BlueprintHelper.TaskSpec.v1`, then uses the same Bridge preview and UE-side validation path as the MCP task tools.

```powershell
node <PLUGIN_ROOT>\ClaudePlugin\cli\build\cli\index.js task preview --file .\task_spec.json --format summary
```

Use `--format summary` for normal Agent loops so the shell returns compact text plus artifact paths instead of large escaped JSON blobs.

## Execute

Execute only after preview passes. This uses the same Python Task Compiler handoff, Bridge task execution, and UE Task Runtime execution path as `blueprinthelper_execute_task`.

```powershell
node <PLUGIN_ROOT>\ClaudePlugin\cli\build\cli\index.js task execute --file .\task_spec.json --format summary
```

## Read Full Result

Use the artifact paths returned by summary output for follow-up inspection. Use `--format json` only when the Agent truly needs the full JSON in context.

## Rules

- Keep writes TaskSpec-first. Any CLI write should start from `BlueprintHelper.TaskSpec.v1`.
- Preview before execute.
- The CLI is not a raw Bridge write surface.
- For schema and boundary details, use [MCP_Tools_API_Reference.md](MCP_Tools_API_Reference.md).
