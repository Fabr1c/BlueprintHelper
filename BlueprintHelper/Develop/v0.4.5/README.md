# BlueprintHelper v0.4.5 Stable Release

Date: 2026-05-19

Status: historical stable release note for the v0.4.5 capability surface.

Superseded: current repository metadata is `v0.5.4` as of 2026-05-20. Use the repository root `README.md` for current version/install/upgrade entry points.

## Release Positioning

v0.4.5 is the stable Agent-facing release line after the Review/Debug v2 upgrade, AgentFace template sync, CLI output cleanup, and ReadContext `logic_flow` implementation.

This release is intended for ordinary Agents using the TaskSpec-first CLI workflow. MCP remains restricted to Editor lifecycle for ordinary workflows. Deprecated direct MCP task tools, legacy fixture fields, and ordinary-Agent access to internal Review actions are not part of this release surface.

## Stable Agent Surface

Default Agent-facing CLI tools:

```text
blueprint_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_context_capabilities
blueprinthelper_read_task_context
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

Compatibility lifecycle commands:

```text
blueprint_open_editor
blueprint_close_editor
```

`blueprinthelper_apply_review_action` remains plugin-development/internal only. It is marked expert-only and is intentionally absent from ordinary Agent templates.

## Main Capabilities

### TaskSpec-First Writes

- Ordinary writes use `BlueprintHelper.TaskSpec.v1`.
- Agents preview before execute through `blueprinthelper_preview_task`, `blueprinthelper_execute_task`, or grouped `bh task preview/execute`.
- Write permission is requested only after preview when the Editor reports write permission is disabled.
- CLI stdout is compact by default; full results are written to artifacts.
- Expert/raw diagnostics are separated behind `--expert` and `artifacts.debug_result`.

Covered stable write templates include:

- Blueprint class, Widget Blueprint, UserDefinedStruct, DataTable, and DataAsset creation.
- Blueprint components, member variables, class settings, interfaces, functions, custom events, event dispatchers, and guarded signature removal.
- Graph append, replace, patch, and merge operations using stable BlueprintHelper-owned anchors.
- UMG WidgetTree/property edits, DataTable row edits, and UObject/DataAsset property edits.

### ReadContext v0.4.5

ReadContext now has a three-tier logic reading strategy:

| Format | Use Case | Anchor Safety |
| --- | --- | --- |
| `logic_flow` | Small function/event/custom-event flow summaries | Not an anchor source |
| `logic_md` | Larger or more branched human-readable logic inspection | Not an anchor source |
| `logic_json` | Full structured graph reads, write anchors, patch/merge/debug | Anchor-capable |

`logic_flow` is implemented in task-core from structured `LogicJson.v1`, not from Markdown parsing. It returns `LogicFlow.v1` with `mode`, `flow`, `stats`, and `warnings`, and it strips raw LogicJson anchors and UE identity fields from the compact payload.

ReadContext capability discovery is available through `blueprinthelper_read_context_capabilities`. It is local to task-core, does not touch UE assets, and returns `ReadContextCapabilities.v1` with the current read type / target type / format matrix.

### Reference And Function Chain Context

- `blueprinthelper_read_reference_context` returns compact impact context for asset, member, local variable, event, dispatcher, widget, DataTable row, interface, and block targets.
- Reference context avoids GUID-first Agent contracts and rejects removed legacy fields such as `target_guid`, old `scope`, and old `include_samples`.
- `blueprinthelper_read_function_chain_context` returns a compact index of project-authored function/event/custom-event calls reachable from an entry point.
- Function chain results are an index for follow-up reads, not a full graph dump.

### Review/Debug v2

- ReviewPanel is now on the v2 Data / Service / Presenter / UI architecture line.
- Pending Review state is owned by ReviewStore.
- Row actions use canonical `FBlueprintHelperReviewActionIntent` and exact row bindings, not fuzzy SearchText execution.
- Accept/Reject success flows through ReviewCommandService, ReviewActionService, StoreChanged, PanelState rebuild, and SurfaceView refresh.
- DebugBundle captures action intent, service result, store refresh, presenter rebuild, and surface refresh context.
- Current ReviewPanel v2 notes report no known blocking ReviewPanel-level issue; future issues should be diagnosed through DebugBundle and fixed at Store/PanelState/SurfaceView/RowBinding boundaries.

### Diagnostics And Safety

- Static and runtime diagnostics are available from CLI.
- Debug case list/get/export are summary-oriented by default.
- Review record query remains available as a summary read path.
- Agent-facing result sanitization removes raw UE GUID-like fields and internal Review target keys from ordinary outputs.
- Write-session responses omit raw session/auth tokens; Agents must not pass `auth_session`, `auth_token`, or `BLUEPRINTHELPER_BRIDGE_TOKEN`.

### Compatibility

- UE 5.6 remains the production baseline.
- UE 5.3 BuildPlugin compatibility has been handled through explicit compatibility helpers and fallback classes without weakening the UE 5.6 path.
- Legacy `Develop/TestFixtures` and old MCP/Bridge compatibility fields are archived. Current schemas reject those old fields instead of silently accepting them.

## Verification Evidence

Current evidence recorded in Develop docs:

- `AgentFaceService/task-core`: TypeScript build passed.
- `AgentFaceService/task-core`: Node tests passed, 120/120, including `logic_flow`, capability discovery, output sanitization, and expert-only Review action checks.
- `AgentFaceService/cli`: TypeScript build passed.
- `AgentFaceService/cli`: Node tests passed, 39/39.
- ReadSpec retest after MCP Editor reopen: all 11 `BP_ThirdPersonCharacter_20260519` ReadSpecs completed with `--develop`, including `11_blueprint_logic_flow.json`.
- ReviewPanel v2 compile evidence: `TemplateEditor Win64 Development` passed on 2026-05-19 15:26; existing `STreeView::ItemHeight` deprecation warning is not introduced by this release line.
- UE 5.3 and UE 5.6 BuildPlugin checks passed in the compatibility audit.

## Known Non-Goals And Follow-Ups

These are not blockers for v0.4.5 stable:

- v0.5.0 performance work remains a follow-up line: preview reuse, `dry_run_mode` optimization, CallFunction resolution cache, compiler fast path/Python worker, Review IO batching, TaskRuntime three-layer execution, and read snapshot/formatter optimization.
- `logic_flow` is not a write anchor source. Use `logic_json` for patch, merge, replacement, or debug anchor work.
- Function scope / Local Variables Review still needs a focused current plan before implementation.
- Baseline semantic snapshot retention/compaction remains a future policy item; the current semantic target snapshot core path is complete.
- ReviewPanel v2 may still receive edge-case fixes, but they must follow the v2 boundaries and must not restore UI-local deletion, fuzzy action routing, transaction fallback, or delay/retry refresh patches.

## Packaging Checklist

Before packaging this release as v0.4.5 stable, update plugin metadata:

- `BlueprintHelper/BlueprintHelper.uplugin`
  - `Version`: `405`
  - `VersionName`: `0.4.5`
  - `IsBetaVersion`: `false`

This checklist is retained as historical v0.4.5 packaging context. Current repository metadata has since been bumped to `Version: 504`, `VersionName: 0.5.4`, and `IsBetaVersion: false`; do not use this file as the current packaging checklist.

## Source Documents

- `BlueprintHelper/Develop/Plan/BlueprintHelper_ReadContext_LogicFlow_Implementation_PLAN_20260519_CN.md`
- `BlueprintHelper/Develop/Plan/BlueprintHelper_ReviewPanelV2_ArchitecturePlan_20260519_CN.md`
- `BlueprintHelper/Develop/Plan/BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md`
- `BlueprintHelper/Develop/Plan/BlueprintHelper_v0.5.0_TaskSpecExecution_PerformanceOptimization_20260519_CN.md`
- `AgentFaceService/docs/CLI_Tools_API_Reference.md`
- `AgentFaceService/docs/TaskSpec_CLI_QuickStart.md`
- `AgentFaceService/agent-guide/Templates/`
- `BlueprintHelper/Develop/v0.4.4/ArchivedReference/CompletedDevelopDocs_20260519/`
