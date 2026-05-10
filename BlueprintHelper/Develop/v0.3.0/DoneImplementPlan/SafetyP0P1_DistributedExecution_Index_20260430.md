# BlueprintHelper Safety P0/P1 Distributed Execution Index

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans. Track each worker document with checkbox steps and stop at the stated exit criteria.

**Goal:** Finish the remaining BlueprintHelper P0/P1 safety closure without resuming upper-layer feature expansion.

**Architecture:** The safety base is split by ownership boundary: Bridge request guard, strict import diagnostics, asset mutation transactions, AgentImportGraph hardening, MCP regression surface, and release gate. Workers must not widen `AgentImportGraph`, `ChangeReview`, or MCP structuredContent until Worker A/B/C/F have passed.

**Tech Stack:** Unreal Engine 5.6 on `F:/UE_5.6`, BlueprintHelper C++ editor plugin, MCP TypeScript server, UE Automation Tests, local Bridge protocol.

---

## Current Checkpoint

This document starts after the current safety implementation checkpoint. Do not redo completed work unless a verification failure proves it is broken.

Completed in this checkpoint:

- Added Bridge auth surface: top-level `auth_token`, `unauthorized`, `command_disabled`.
- Added `FBlueprintHelperRequestValidator` and routed Bridge requests through payload validation plus auth validation.
- Default-disabled `exec_console_command` and `close_editor`.
- Normalized raw export scope to `graph`, `blueprint`, `selection`; legacy `full_graph` and `full_blueprint` remain accepted.
- Added raw export `effective_scope`.
- Changed explicit graph lookup failure to hard fail with `graph_not_found`.
- Made validation graph-scoped for node ids and links; cross-graph links return `cross_graph_link_not_supported`.
- Made `import_json` default strict and return `status`, `nodes_created`, `links_connected`, `operations_applied`, `warnings`, `errors`, and `rolled_back`.
- Added transaction or rollback protection for key write services: delete nodes, DataTable row update, UObject property set, UMG add/remove/move/property set.
- Added safety automation tests under `BlueprintHelper.Safety`.

Verified commands:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\MCPServer
npm.cmd run build
```

```powershell
& 'F:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

```powershell
& 'F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/UnrealPractise/MrStone/MrStone.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.Safety;Quit'
```

All three completed successfully at this checkpoint.

Worker F final verification gate completed on 2026-04-30 17:49 Asia/Shanghai:

- `npm.cmd run build` passed in `MCPServer`.
- UE 5.6 build passed with `Result: Succeeded`.
- `Automation RunTests BlueprintHelper.Safety` passed with 17 tests and exit code 0.
- Added `BlueprintHelper.Safety.BridgeExport.ReturnsEffectiveScope` to directly cover the `effective_scope` Bridge response field.
- Sentry gate skipped because `SENTRY_AUTH_TOKEN`, `SENTRY_ORG`, or `SENTRY_PROJECT` is not configured.

## Pause Rule

After this checkpoint, pause upper-layer work:

- Do not expand `blueprint_import_agent_graph` capability.
- Do not start Change Review UI or review result surface.
- Do not continue MCP structuredContent optimization except regression fixes required by Worker E.
- Do not clean tracked `Binaries/` or `Intermediate/` changes unless the release owner explicitly decides how prebuilt artifacts are handled.

## Parallel Execution

| Worker | Document | Dependency | Write Scope |
|---|---|---|---|
| A | `SafetyP0P1_WorkerA_BridgeGuard_20260430.md` | none | Bridge request parsing, validator, Bridge protocol, MCP auth forwarding |
| B | `SafetyP0P1_WorkerB_StrictImportDiagnostics_20260430.md` | A for request semantics | validation, import service, generator diagnostics, pin/default/link strict behavior |
| C | `SafetyP0P1_WorkerC_MutationTransactions_20260430.md` | none | scoped mutation helper, DataTable, UMG, UObject, delete nodes |
| D | `SafetyP0P1_WorkerD_AgentImportGraphHardening_20260430.md` | A + B + C | AgentImportGraph only; no new feature capability |
| E | `SafetyP0P1_WorkerE_MCPRegression_20260430.md` | A + B | MCP schema, compatibility, regression fixtures |
| F | `SafetyP0P1_WorkerF_VerificationGate_20260430.md` | A + B + C + D + E | tests, automation command set, release gate docs |

Recommended order:

```text
A + C
-> B
-> D + E
-> F
```

## Integration Gate

The safety base is considered closed only when all of these are true:

- `npm.cmd run build` passes in `MCPServer`.
- UE 5.6 build passes from `F:/UE_5.6`.
- `Automation RunTests BlueprintHelper.Safety` passes.
- Strict import failure cases prove rollback for link/default/pin failure.
- `target_graph="EventGrph"` returns `graph_not_found` and does not write `EventGraph`.
- Untokened write commands return `unauthorized`.
- High-risk commands return `command_disabled` by default.
- `scope=full_graph/full_blueprint/graph/blueprint/selection` returns explicit `effective_scope`.
- DataTable multi-field update leaves no half-write on a failed field.
- UMG Move failure restores old parent, old index, and old slot layout data.
- UObject and Widget property writes reject read-only, edit-const, transient, and non-editor-editable fields.

## Shared Commands

Run MCP build:

```powershell
cd G:\UnrealPractise\MrStone\Plugins\BlueprintHelper\MCPServer
npm.cmd run build
```

Run UE build:

```powershell
& 'F:/UE_5.6/Engine/Build/BatchFiles/Build.bat' MrStoneEditor Win64 Development -Project='G:/UnrealPractise/MrStone/MrStone.uproject'
```

Run safety automation:

```powershell
& 'F:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'G:/UnrealPractise/MrStone/MrStone.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.Safety;Quit'
```

Inspect remaining strong Bridge field reads:

```powershell
rg "Get(String|Object|Array|Bool|Integer|Number)Field" Source/BlueprintHelper/Private/Bridge Source/BlueprintHelper/Public/Bridge
```
