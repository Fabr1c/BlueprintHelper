# BlueprintHelper v0.5.4 Release Sync

Date: 2026-05-20

Status: current repository metadata and Agent-facing entry point sync.

## Version Surfaces

The current plugin version is `v0.5.4`.

| Surface | Current value |
| --- | --- |
| Unreal plugin descriptor | `Version: 504`, `VersionName: 0.5.4`, `IsBetaVersion: false` |
| `AgentFaceService/cli` package | `0.5.4` |
| `AgentFaceService/task-core` package | `0.5.4` |
| `AgentFaceService/mcp` package | `0.5.4` |
| MCP server metadata | `0.5.4` |
| Codex plugin manifest | `0.5.4` |
| Claude plugin manifest | `0.5.4` |
| Default settings version | `0.5.4` |

## User Entry Points

Recommended first-run install entry:

```cmd
.\install.cmd -ProjectFile <Project.uproject> -EngineRoot <UE root>
```

Recommended upgrade entry:

```cmd
.\upgrade.cmd
.\upgrade.cmd -CheckOnly
.\upgrade.cmd -Force
```

`install.ps1` and `update.ps1` remain implementation scripts behind the `.cmd` wrappers. User-facing docs should recommend `install.cmd` and `upgrade.cmd`.

## Capability Baseline

v0.5.4 keeps the v0.4.5 stable Agent-facing surface and includes the v0.5.0 performance implementation line.

- CLI-first ordinary asset work through TaskSpec, ReadContext, diagnostics, write sessions, task result queries, and debug bundle reads.
- Template-first AgentGuide workflow for reducing TaskSpec and tool payload field errors.
- Review/Debug v2 as the only active Review architecture baseline.
- ReadContext `logic_flow`, `logic_md`, and `logic_json` with `logic_json` reserved for anchor-capable write/debug work.
- Performance work covering preview reuse/dry-run policy, compiler fast path, Review IO batching, TaskRuntime execution layering, read snapshot/formatter reuse, GraphWrite cluster execution, and compile/save post-operation planning.
- Ordinary Agent MCP use remains restricted to editor lifecycle.

## Historical Notes

- `Develop/v0.4.5/README.md` is now a historical release note, not the current packaging checklist.
- `Develop/Plan/BlueprintHelper_v0.5.0_TaskSpecExecution_PerformanceOptimization_20260519_CN.md` and its `Optimization/` children remain the detailed implementation record for the v0.5.0 performance line.
