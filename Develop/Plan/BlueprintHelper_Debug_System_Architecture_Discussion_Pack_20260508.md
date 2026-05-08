# BlueprintHelper Debug System Architecture Discussion Pack

Date: 2026-05-08
Purpose: collect all Debug-related documents and source references needed for web-side discussion of the BlueprintHelper Debug system architecture.
Status: discussion pack, no code change

## 1. Discussion Boundary

This document is for designing an independent Debug system across MCP, orchestration, and the UE plugin.

Confirmed boundaries:

- DebugExport should become a developer diagnostics system, not a normal Agent read/write path.
- The system should capture concrete error context and support exporting debug bundles.
- Normal Agents still use TaskSpec -> preview -> execute -> get task result.
- `large_payload_ref` is not the current Agent-facing roadmap.
- Large asset bodies should be read by targeted `logic_md` / `logic_json` slices, not default full export.
- Review / ReviewPanel remains user-side; Debug can link to Review records but must not become a Review UI substitute.
- Diagnostics and RuntimeProfile remain small read-only preflight / troubleshooting tools.

Primary question for discussion:

```text
How do ToolResultBase, trace_id, RuntimeProfile, Diagnostics, TaskRunJournal,
Transaction Journal, ReviewRecord, UE logs, Bridge errors, and optional asset
snapshots compose into one debuggable, privacy-bounded DebugCase / DebugBundle model?
```

## 2. Recommended Reading Order

| Order | File | Why it matters |
| --- | --- | --- |
| 1 | `Resources/Plan/BlueprintHelper_Current_TODO_20260506.md` | Current decision: build DebugExport as independent developer diagnostics; remove bulk-reference-first design. |
| 2 | `Resources/Plan/BlueprintHelper_v0.3.6_Current_Implementation_Gap_Matrix_20260505.md` | Current status matrix: Diagnostics done, RuntimeProfile done, ToolResultBase done, DebugExport partial and missing service/command/runtime integration. |
| 3 | `Resources/BlueprintHelper_Hybrid_SourceDocs_Sync_20260504/05_Validation_Diagnostics_Tools_Design_Synced_20260504.md` | Defines Diagnostics vs preflight vs dry_run vs Review and markdown diagnostic rules. |
| 4 | `Resources/v0.3.6/FieldMapping/BlueprintHelper_ToolResultBase_CommonEnvelope_UE_FieldMapping_20260503.md` | Defines common result/error envelope, `trace_id`, status rules, and compact debug placement. |
| 5 | `Resources/v0.3.6/FieldMapping/BlueprintHelper_DebugExport_LargePayload_UE_FieldMapping_20260503.md` | Old DebugExport / LargePayload field mapping. Use as prior art, not as current Agent bulk-read design. |
| 6 | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_DebugExport_LargePayload_UE_CPP_Implementation_Plan_20260503.md` | Old implementation plan. Useful for export scope and bundle manifest ideas, but current source only has DTO types. |
| 7 | `Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md` | Defines TaskRunJournal, partial failure, recovery, and debug data boundary. |
| 8 | `Docs/MCP_Tools_API_Reference.md` | Defines current MCP envelope and allowed Agent-facing diagnostics / task result surface. |
| 9 | `Resources/Plan/BlueprintHelper_Review_Transaction_Model_Discussion_Pack_20260507.md` | Defines how DebugExport should link to ReviewRecord without becoming Review storage. |

## 3. Core Debug Documents

| Area | File | Current meaning |
| --- | --- | --- |
| Current TODO | `Resources/Plan/BlueprintHelper_Current_TODO_20260506.md` | Tracks DebugExport as an active TODO: MCP + orchestration + plugin must capture/export concrete error context; no bulk-reference-first design. |
| Current gap matrix | `Resources/Plan/BlueprintHelper_v0.3.6_Current_Implementation_Gap_Matrix_20260505.md` | Records Diagnostics / RuntimeProfile / ToolResultBase as implemented and DebugExport as partial. |
| Diagnostics synced design | `Resources/BlueprintHelper_Hybrid_SourceDocs_Sync_20260504/05_Validation_Diagnostics_Tools_Design_Synced_20260504.md` | Canonical boundary for Diagnostics, runtime diagnostics, preflight, dry_run, and Review. |
| Safety profile / dry run | `Resources/BlueprintHelper_Hybrid_SourceDocs_Sync_20260504/07_Safety_Profile_DryRun_Design_Synced_20260504.md` | Related pre-execution safety context that debug bundles may need to reference. |
| ToolResultBase field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_ToolResultBase_CommonEnvelope_UE_FieldMapping_20260503.md` | Shared envelope, `ok`, `status`, `trace_id`, `error`, `conflicts`, `resource_ref`, and normal hiding of transaction/review/safety fields. |
| ToolResultBase implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_ToolResultBase_CommonEnvelope_UE_CPP_Implementation_Plan_20260503.md` | UE implementation plan for the common result builder and error protocol. |
| DebugExport field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_DebugExport_LargePayload_UE_FieldMapping_20260503.md` | Prior DebugExport / bundle / snapshot / large-payload contract. Keep as prior art; do not copy `large_payload_ref` into the current Agent path. |
| DebugExport implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_DebugExport_LargePayload_UE_CPP_Implementation_Plan_20260503.md` | Prior C++ implementation plan for export scopes, manifest, privacy boundary, and resource references. |
| TaskSpec / TaskPlan contract | `Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md` | Defines task-level journal, partial failure, blocked steps, and debug facts under `data.debug` only when useful. |
| MCP API reference | `Docs/MCP_Tools_API_Reference.md` | Current public tool result envelope and Agent-facing diagnostics/result surface. |
| Install quickstart | `Docs/Install_MCP_QuickStart.md` | Notes that MCP stdout is JSON-RPC and diagnostic logs go to stderr. |

## 4. Diagnostics And Runtime Documents

| Area | File | Current meaning |
| --- | --- | --- |
| Diagnostics field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_Diagnostics_UE_FieldMapping_20260503.md` | Static/runtime diagnostics contract. Markdown-only report, no JSON issue arrays by default. |
| Diagnostics implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_Diagnostics_UE_CPP_Implementation_Plan_20260503.md` | UE implementation plan for static/runtime diagnostics. |
| RuntimeProfile field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_RuntimeProfile_UE_FieldMapping_20260503.md` | Runtime facts used before planning/writes: Bridge state, config status, write permission, safety profile, capabilities. |
| RuntimeProfile implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_RuntimeProfile_UE_CPP_Implementation_Plan_20260503.md` | UE implementation plan for runtime profile service. |
| Project context / setup state field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_ProjectContext_SetupState_UE_FieldMapping_20260503.md` | Setup/config context useful for environment debug. |
| Project context / setup state implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_ProjectContext_SetupState_UE_CPP_Implementation_Plan_20260503.md` | UE implementation plan for setup/context checks. |
| Compile asset field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_CompileBlueprintAsset_UE_FieldMapping_20260503.md` | Compile diagnostics contract, relevant for post-write debug and failure localization. |
| Compile asset implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_CompileBlueprintAsset_UE_CPP_Implementation_Plan_20260503.md` | UE implementation plan for compile asset result capture. |
| Save asset field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_SaveAsset_UE_FieldMapping_20260503.md` | Save result contract, relevant for persistence/debug reports. |
| Save asset implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_SaveAsset_UE_CPP_Implementation_Plan_20260503.md` | UE implementation plan for save result handling. |
| Asset discovery/navigation field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_AssetDiscovery_EditorNavigation_UE_FieldMapping_20260503.md` | Asset lookup and editor navigation facts used in debug bundles. |
| Asset discovery/navigation implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_AssetDiscovery_EditorNavigation_UE_CPP_Implementation_Plan_20260503.md` | UE implementation plan for asset discovery/editor navigation. |
| AgentGuide diagnostics reference | `Resources/AgentGuide/Reference/03_Runtime_Profile_And_Diagnostics.md` | Current Agent-facing guidance: RuntimeProfile is preflight truth; Diagnostics explains environment/runtime blockers. |
| AgentGuide recovery workflow | `Resources/AgentGuide/Workflows/07_Safety_Validation_And_Recovery.md` | Current Agent recovery rules after preview/execute failure. |
| Runtime profile example | `Resources/Docs/Setup/RuntimeProfile_Example.json` | Example profile payload. Useful for debug bundle schema examples. |

## 5. Task Failure, Journal, And Review Linkage Documents

| Area | File | Current meaning |
| --- | --- | --- |
| TaskSpec / TaskPlan architecture | `Resources/Plan/BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md` | Shows how TaskRuntime, transaction, review, rollback, compile/save, and diagnostics fit under TaskSpec-first execution. |
| TaskSpec / TaskPlan contract | `Resources/Docs/TaskSpec_TaskPlan_Contract_20260504.md` | Defines partial failure and topology blocking fields for TaskRunJournal. |
| Transaction / Journal / Review design | `Resources/BlueprintHelper_Hybrid_SourceDocs_Sync_20260504/06_Transaction_Journal_Review_Design_Synced_20260504.md` | Defines Transaction Journal and Review as internal audit facts that Debug can reference. |
| TransactionJournalQuery field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_TransactionJournalQuery_UE_FieldMapping_20260503.md` | Read-only transaction query prior art; future ReviewRecord and DebugCase can consume these summaries. |
| TransactionJournalQuery implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_TransactionJournalQuery_UE_CPP_Implementation_Plan_20260503.md` | UE implementation plan for transaction query services. |
| Review transaction discussion pack | `Resources/Plan/BlueprintHelper_Review_Transaction_Model_Discussion_Pack_20260507.md` | Defines ReviewRecord discussion target and DebugExport linkage expectations. |
| P1 smoke rerun report | `Resources/Plan/BlueprintHelper_P1_Remaining_Gap_Smoke_20260507_Rerun.md` | Evidence of empty-error fallback and blocked diagnostics issues that motivated the Debug system. |
| P1 smoke plan/report | `Resources/Plan/BlueprintHelper_P1_Remaining_Gap_Smoke_20260507.md` | Earlier evidence for bridge/runtime preview/execute/read-back error handling. |

## 6. Historical Prior Art

These files are not current architecture sources of truth, but they contain useful debug/diagnostic prior art.

| Area | File | Use carefully |
| --- | --- | --- |
| Early MCP return protocol | `Resources/v0.3.0/Done_Module_BlueprintHelper_MCP_ReturnProtocol_Optimization_20260428.md` | Prior result-shape work. |
| MCP return design | `Resources/v0.3.0/Done_MCP返回结构优化_Design_20260430.md` | Prior structuredContent / diagnostics ideas. |
| MCP return tech spec | `Resources/v0.3.0/Done_MCP返回结构优化_TechSpec_20260430.md` | Prior buildBlueprintToolResult and diagnostics-array ideas. |
| MCP return change doc | `Resources/v0.3.0/Done_MCP返回结构优化_ChangeDoc_20260430.md` | Prior migration notes. |
| Object-first bridge payload plan | `Resources/v0.3.0/Done_BlueprintHelper_BridgePayload_ObjectFirst_ParallelExecutionPlan_20260501.md` | Prior object-first payload and resource-ref work. |
| Object-first migration notes | `Resources/v0.3.0/Done_BridgePayload_ObjectFirst_MigrationNotes_20260501.md` | Prior payload normalization notes. |
| Strict import diagnostics | `Resources/v0.3.0/DoneImplementPlan/SafetyP0P1_WorkerB_StrictImportDiagnostics_20260430.md` | Structured import failure diagnostics prior art. |
| Mutation transactions | `Resources/v0.3.0/DoneImplementPlan/SafetyP0P1_WorkerC_MutationTransactions_20260430.md` | Rollback/diagnostics prior art for non-import writes. |
| Bridge guard | `Resources/v0.3.0/DoneImplementPlan/SafetyP0P1_WorkerA_BridgeGuard_20260430.md` | Request/bridge guard prior art. |
| MCP regression surface | `Resources/v0.3.0/DoneImplementPlan/SafetyP0P1_WorkerE_MCPRegression_20260430.md` | Regression expectations around failure cases. |
| Early AI protocol | `Resources/v0.2.0/Done_AI_Protocol_20260408.md` | Historical resource/diagnostics protocol prior art. |
| Phase 0/1 plan | `Resources/v0.2.0/Done_Phase0_Phase1_Plan.md` | Early `FBlueprintHelperDiagnosticSet` prior art. |

## 7. UE Source Map

Runtime diagnostics services:

| File | Current role |
| --- | --- |
| `Source/BlueprintHelper/Public/Services/RuntimeDiagnostics/BlueprintHelperDiagnosticsService.h` and `Source/BlueprintHelper/Private/Services/RuntimeDiagnostics/BlueprintHelperDiagnosticsService.cpp` | Static/runtime diagnostics service. |
| `Source/BlueprintHelper/Public/Services/RuntimeDiagnostics/BlueprintHelperRuntimeProfileService.h` and `Source/BlueprintHelper/Private/Services/RuntimeDiagnostics/BlueprintHelperRuntimeProfileService.cpp` | RuntimeProfile service. |
| `Source/BlueprintHelper/Public/Services/RuntimeDiagnostics/BlueprintHelperContextService.h` and `Source/BlueprintHelper/Private/Services/RuntimeDiagnostics/BlueprintHelperContextService.cpp` | Editor/project context service. |
| `Source/BlueprintHelper/Public/Services/RuntimeDiagnostics/BlueprintHelperValidationService.h` and `Source/BlueprintHelper/Private/Services/RuntimeDiagnostics/BlueprintHelperValidationService.cpp` | Validation support. |
| `Source/BlueprintHelper/Public/Services/RuntimeDiagnostics/BlueprintHelperCompileService.h` and `Source/BlueprintHelper/Private/Services/RuntimeDiagnostics/BlueprintHelperCompileService.cpp` | Compile support. |
| `Source/BlueprintHelper/Public/Services/RuntimeDiagnostics/BlueprintHelperCompileAssetService.h` and `Source/BlueprintHelper/Private/Services/RuntimeDiagnostics/BlueprintHelperCompileAssetService.cpp` | Asset compile façade. |
| `Source/BlueprintHelper/Public/Services/RuntimeDiagnostics/BlueprintHelperAssetBrowseService.h` and `Source/BlueprintHelper/Private/Services/RuntimeDiagnostics/BlueprintHelperAssetBrowseService.cpp` | Asset lookup/navigation support. |
| `Source/BlueprintHelper/Public/Services/RuntimeDiagnostics/BlueprintHelperEditorCommandService.h` and `Source/BlueprintHelper/Private/Services/RuntimeDiagnostics/BlueprintHelperEditorCommandService.cpp` | Editor command/lifecycle support. |

Runtime diagnostics structures:

| File | Current role |
| --- | --- |
| `Source/BlueprintHelper/Public/Structure/RuntimeDiagnostics/BlueprintHelperDiagnosticsTypes.h` | Diagnostics markdown report DTO. |
| `Source/BlueprintHelper/Public/Structure/RuntimeDiagnostics/BlueprintHelperRuntimeProfileTypes.h` | Runtime profile DTOs. |
| `Source/BlueprintHelper/Public/Structure/RuntimeDiagnostics/BlueprintHelperProjectContextTypes.h` | Project context/setup state DTOs. |
| `Source/BlueprintHelper/Public/Structure/RuntimeDiagnostics/BlueprintHelperCompileAssetTypes.h` | Compile asset DTOs. |
| `Source/BlueprintHelper/Public/Structure/RuntimeDiagnostics/BlueprintHelperSaveAssetTypes.h` | Save asset DTOs. |
| `Source/BlueprintHelper/Public/Structure/RuntimeDiagnostics/BlueprintHelperAssetDiscoveryTypes.h` | Asset discovery DTOs. |
| `Source/BlueprintHelper/Public/Structure/RuntimeDiagnostics/BlueprintHelperEditorLifecycleTypes.h` | Editor lifecycle DTOs. |
| `Source/BlueprintHelper/Public/Structure/RuntimeDiagnostics/BlueprintHelperDebugExportTypes.h` | Current DebugExport DTO/error-code starting point. No complete service or command is present. |

Common result and task runtime:

| File | Current role |
| --- | --- |
| `Source/BlueprintHelper/Public/Structure/BlueprintHelperToolResultTypes.h` | UE common ToolResultBase DTO and error/review/transaction/status enums. |
| `Source/BlueprintHelper/Private/Structure/BlueprintHelperToolResultBuilder.cpp` | UE result builder and trace/transaction id helpers. |
| `Source/BlueprintHelper/Public/TaskRuntime/BlueprintHelperTaskRuntimeService.h` and `Source/BlueprintHelper/Private/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp` | Task preview/execute journal, partial failure, blocked downstream steps, recovery summary, compile/save post steps. |
| `Source/BlueprintHelper/Private/BlueprintHelper.cpp` | Service construction and bridge command wiring. Useful to verify whether DebugExport service/commands are actually wired. |

Review debug UI:

| File | Current role |
| --- | --- |
| `Source/BlueprintHelper/Public/Widgets/Review/BlueprintHelperReviewDebugText.h` and `Source/BlueprintHelper/Private/Widgets/Review/BlueprintHelperReviewDebugText.cpp` | Review UI debug text helper, not the global Debug system. |

Tests:

| File | Current role |
| --- | --- |
| `Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperSafetyTests.cpp` | Runtime diagnostics / safety tests. |
| `Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp` | ToolResultBase coverage for GraphWrite. |
| `Source/BlueprintHelper/Private/Tests/BlueprintComponent/BlueprintHelperComponentToolResultBaseTests.cpp` | ToolResultBase coverage for component operations. |

## 8. MCP / Orchestration Source Map

| File | Current role |
| --- | --- |
| `BlueprintHelper_MCP_Server/src/tools.ts` | Registers Agent-facing diagnostics/runtime profile tools and frozen legacy/debug tools. No DebugExport tool is currently registered. |
| `BlueprintHelper_MCP_Server/src/task-tools.ts` | Implements preview/execute/get task result; handles Bridge failures and task result storage. |
| `BlueprintHelper_MCP_Server/src/tool-result.ts` | MCP ToolResultBase normalization, `trace_id`, summary generation, error fallback behavior. |
| `BlueprintHelper_MCP_Server/src/mcp-response.ts` | Older/general MCP response builder and resource-link support. Useful prior art for debug bundle refs. |
| `BlueprintHelper_MCP_Server/src/bridge-client.ts` | Bridge transport layer and failure boundary. |
| `BlueprintHelper_MCP_Server/src/task-schemas.ts` | `TaskRunJournalSchema`, partial failure, step status, blocked dependencies, recovery fields. |
| `BlueprintHelper_MCP_Server/src/task-result-store.ts` | Stores/normalizes task run journals and transaction refs. |
| `BlueprintHelper_MCP_Server/src/task-contract.ts` | Contract metadata for TaskSpec/TaskPlan capabilities and default/frozen tool surface. |
| `BlueprintHelper_MCP_Server/python/blueprinthelper_task/errors.py` | Python compiler/orchestration error types. |
| `BlueprintHelper_MCP_Server/src/task-tools.regression.test.ts` | Regression coverage for preview/execute error fallbacks and task-result behavior. |
| `BlueprintHelper_MCP_Server/src/task-run-journal.schema.test.ts` | TaskRunJournal partial failure schema regression. |
| `BlueprintHelper_MCP_Server/src/task-result-store.test.ts` | TaskRunJournal normalization and transaction id extraction tests. |
| `BlueprintHelper_MCP_Server/src/tools.regression.test.ts` | Tool registry/frozen description regressions. |
| `BlueprintHelper_MCP_Server/src/mcp-response.object-first.test.ts` | Resource/ref and object-first response prior art. |

## 9. Current Implementation Facts

Implemented or source-integrated:

- Diagnostics service exists and returns markdown diagnostics through ToolResultBase.
- RuntimeProfile service exists and remains preflight fact source.
- ToolResultBase exists on UE and MCP sides.
- TaskRunJournal exists on UE and MCP sides.
- MCP task tools include fallback handling for empty Bridge / ToolResult errors.
- Compile/save/asset discovery/context services exist and can provide debug evidence.
- Review debug text helper exists inside the Review UI, but it is not the global Debug system.

Not implemented as a full Debug system:

- No complete `FBlueprintHelperDebugExportService` is present.
- No wired UE Bridge command for `export_debug_bundle`, `export_transaction_debug_bundle`, or `export_asset_logic_snapshot` is present.
- No MCP DebugExport tool is currently registered.
- No persistent `DebugCase` / `DebugBundle` index exists.
- No cross-layer correlation model links `trace_id`, `task_run_id`, `transaction_id`, `review_record_id`, and exported artifacts.
- Old `read_large_payload_ref` should not be carried forward as a normal Agent-facing read path.

## 10. Proposed Debug Architecture For Discussion

Discussion target:

```text
DebugCase = correlated evidence record for a failure, warning, blocked preview,
partial task failure, transaction conflict, Review rollback issue, or developer-requested export.

DebugBundle = sanitized artifact export for one DebugCase or explicit target.
```

Layer model:

| Layer | Responsibility | Candidate inputs |
| --- | --- | --- |
| Signal collection | Capture facts at failure/blocking points | ToolResultBase, BridgeResponse, RuntimeProfile, Diagnostics markdown, TaskRunJournal, compile/save result, transaction summaries, ReviewRecord refs, targeted context slices. |
| Correlation | Link evidence across layers | `debug_case_id`, `trace_id`, `task_run_id`, `preview_id`, `transaction_id`, `review_record_id`, asset paths, operation, stage. |
| Sanitization | Remove sensitive/private data | Tokens, secrets, full settings, full AgentGuide, local absolute paths, full project/source export. |
| Artifact export | Write a bounded bundle | Manifest JSON, compact markdown report, selected child results, log excerpts, optional targeted asset snapshots. |
| MCP access | Let developers or Agents request debug evidence when needed | Explicit debug export/query tools, not default AgentGuide write flow. |
| UI / Review linkage | Let user-side Review link to debug facts | ReviewRecord -> DebugCase / DebugBundle refs. |

## 11. Proposed DebugCase Shape For Discussion

This is a discussion target, not an implemented schema.

```json
{
  "schema": "BlueprintHelper.DebugCase.v1",
  "debug_case_id": "dbg_...",
  "created_at": "...",
  "source": "task_execute_failure",
  "severity": "error",
  "operation": "execute_task",
  "stage": "bridge.execute_task_plan",
  "trace_ids": ["trace_..."],
  "task_run_id": "task_...",
  "preview_id": "preview_...",
  "transaction_ids": ["tx_..."],
  "review_record_ids": ["review_..."],
  "asset_paths": ["/Game/..."],
  "error": {
    "code": "bridge_write_failed",
    "message": "Bridge write failed.",
    "retryable": false
  },
  "runtime_profile_ref": "inline_or_ref",
  "diagnostics_ref": "inline_markdown_or_ref",
  "task_run_journal_ref": "inline_or_ref",
  "tool_result_ref": "inline_or_ref",
  "bridge_response_ref": "inline_or_ref",
  "artifacts": [
    {
      "kind": "logic_md_slice",
      "ref": "resource://blueprinthelper/debug/..."
    }
  ],
  "redactions": ["local_absolute_paths", "tokens", "settings_full"],
  "recommended_next": "inspect_debug_bundle"
}
```

## 12. Proposed DebugBundle Shape For Discussion

This is a discussion target, not an implemented schema.

```json
{
  "schema": "BlueprintHelper.DebugBundleManifest.v1",
  "bundle_id": "bundle_...",
  "debug_case_id": "dbg_...",
  "format": "directory",
  "created_at": "...",
  "manifest_version": 1,
  "contents": [
    "manifest.json",
    "summary.md",
    "runtime_profile.json",
    "diagnostics.md",
    "task_run_journal.json",
    "tool_result.json",
    "bridge_response_summary.json",
    "transactions/tx_....json",
    "assets/BP_Example.logic.md"
  ],
  "privacy": {
    "contains_tokens": false,
    "contains_full_settings": false,
    "contains_local_absolute_paths": false,
    "contains_full_asset_raw_json": false
  }
}
```

## 13. Open Decisions For Web Discussion

1. DebugCase identity

Should one DebugCase map to one `trace_id`, one `task_run_id`, one failure event, or one user-visible incident?

2. Capture points

Which layers create DebugCase records automatically:

```text
MCP wrapper failure
Bridge transport failure
UE TaskRuntime preview blocker
UE TaskRuntime execute failure
partial_failure journal
compile/save failure
Review rollback blocked/failed
user explicit export request
```

3. DebugExport tool surface

Should the new public/debug surface be:

```text
blueprinthelper_export_debug_bundle
blueprinthelper_export_task_debug_bundle
blueprinthelper_export_transaction_debug_bundle
blueprinthelper_export_asset_debug_snapshot
blueprinthelper_get_debug_case
```

or a smaller single tool with `debug_scope`?

4. Agent exposure

Should normal Agents see DebugExport in AgentGuide? Current direction says no. If exposed, it should be as failure-only developer diagnostics, not as a default write/read tool.

5. Large payload replacement

Should `read_large_payload_ref` be deleted entirely, or replaced with a narrower debug-only artifact reader:

```text
read_debug_artifact_manifest
read_debug_artifact_chunk
```

6. Artifact storage

Where should debug artifacts live?

```text
Saved/BlueprintHelper/Debug/Cases
Saved/BlueprintHelper/Debug/Bundles
Saved/BlueprintHelper/Debug/Artifacts
```

7. Retention and cleanup

How long are debug cases retained, and who can compact/delete them?

8. Privacy policy

What exact redaction rules apply to settings, paths, environment variables, AgentGuide, source snippets, raw JSON, transaction payloads, and user asset content?

9. Review integration

Should ReviewRecord store `debug_case_ids`, `debug_bundle_refs`, or only derive them by transaction/task ids?

10. Transaction integration

Should TransactionJournal entries link to DebugCase on write failure only, or also on successful high-risk writes?

11. RuntimeProfile and Diagnostics snapshots

Should DebugCase inline these small payloads, or always store them as artifacts?

12. UI integration

Should the UE BlueprintHelper window have a developer Debug tab separate from the user Review tab?

13. Regression tests

Which paths need mandatory tests:

```text
empty Bridge error -> non-empty DebugCase
partial_failure -> debug case with blocked steps
compile failure -> compile diagnostics captured
transaction rollback blocked -> debug bundle export
privacy redaction of settings/tokens/paths
DebugExport tools still modified=false
```

## 14. Suggested Meeting Output

The web discussion should produce:

- `BlueprintHelper.DebugCase.v1` schema decision.
- `BlueprintHelper.DebugBundleManifest.v1` schema decision.
- Debug capture points and automatic vs explicit export policy.
- MCP tool surface decision.
- UE service / Bridge command / MCP wrapper implementation order.
- Privacy/redaction policy.
- Retention/cleanup policy.
- ReviewRecord and TransactionJournal linkage policy.
- Test plan and smoke fixture list.

