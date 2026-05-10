# BlueprintHelper Review Transaction Model Discussion Pack

Date: 2026-05-07
Purpose: collect the Review-related design, implementation, and source references needed for web-side discussion of the Review transaction model.
Status: discussion pack, no code change

## 1. Discussion Boundary

This document is for designing the user-side Review transaction model.

Confirmed boundaries:

- Review and ReviewPanel are user-side only.
- Do not add Agent-facing ReviewPanel flows or AgentGuide instructions for Accept / Reject.
- Normal Agents should keep using TaskSpec -> preview -> execute -> get task result.
- TransactionJournalQuery should become part of a persisted Review transaction record consumer model.
- TaskRunJournal remains the Agent-visible task summary path.
- `large_payload_ref` is deleted from the current Debug direction.
- DebugCase / DebugBundle are separate developer diagnostics systems.

Primary question for the next design discussion:

```text
How do UE write transactions, TaskRunJournal, archive sessions, visible Review changes,
Accept / Reject actions, rollback, and ownership conversion compose into one durable
Review transaction record model?
```

## 2. Recommended Reading Order

Read these first for the web discussion:

| Order | File | Why it matters |
| --- | --- | --- |
| 1 | `Resources/Plan/BlueprintHelper_ReviewPanel_UserSide_Constraints_20260506.md` | Most precise current constraints: archive baseline, visible change semantics, Accept / Reject, read-only UI, atomic targets. |
| 2 | `Resources/Plan/BlueprintHelper_ReviewPanel_UserSide_Design_20260506.md` | Product and UI model for the user-side Review page. |
| 3 | `Resources/BlueprintHelper_Hybrid_SourceDocs_Sync_20260504/06_Transaction_Journal_Review_Design_Synced_20260504.md` | Original Transaction / Journal / Review rules: all writes journal internally, Agent-facing boundary, rollback and ownership policy. |
| 4 | `Resources/Plan/BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md` | Shows where TaskRunJournal, transaction_id, task_run_id grouping, Review, rollback, and UE Task Runtime fit in the TaskSpec-first architecture. |
| 5 | `Resources/v0.3.6/FieldMapping/BlueprintHelper_TransactionJournalQuery_UE_FieldMapping_20260503.md` | Query boundary and fields for transaction / review summaries. This is the main candidate to fold into the Review record model. |
| 6 | `Resources/Plan/BlueprintHelper_Current_TODO_20260506.md` | Current unresolved tasks and policy corrections. |
| 7 | `Resources/Plan/BlueprintHelper_v0.3.6_Current_Implementation_Gap_Matrix_20260505.md` | Current capability status matrix, including TransactionJournalQuery / Review, DebugExport, ownership, and non-owned anchors. |

## 3. Core Review Documents

| Area | File | Current meaning |
| --- | --- | --- |
| User Review UI design | `Resources/Plan/BlueprintHelper_ReviewPanel_UserSide_Design_20260506.md` | Defines the fake Blueprint Editor review page, final change list, diff frames, ReviewStore, ReviewAction, archive session service, and Accept / Reject semantics. |
| User Review constraints | `Resources/Plan/BlueprintHelper_ReviewPanel_UserSide_Constraints_20260506.md` | Defines the confirmed contract: read-only UI, archive baseline, final visible changes instead of raw transactions, atomic targets, compaction follow-up, and graph-space diff rendering. |
| User Review implementation plan | `Resources/Plan/BlueprintHelper_ReviewPanel_UserSide_Implementation_Plan_20260506.md` | Tracks first-slice implementation work and deferred backend work such as archive persistence, real Reject rollback, compaction, and TaskRunJournal grouping. |
| Transaction / Journal / Review synced design | `Resources/BlueprintHelper_Hybrid_SourceDocs_Sync_20260504/06_Transaction_Journal_Review_Design_Synced_20260504.md` | Canonical high-level design for internal write journaling, Agent-facing hiding of transaction details, rollback policy, Review UX, and ownership after Accept. |
| TaskSpec / TaskPlan architecture | `Resources/Plan/BlueprintHelper_Hybrid_TaskSpec_TaskPlan_Architecture_20260504.md` | Defines the four-layer architecture and identifies Review UI task_run_id grouping as part of the task-level model. |

## 4. Transaction Query, Rollback, Cleanup, Ownership

| Area | File | Current meaning |
| --- | --- | --- |
| Transaction query field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_TransactionJournalQuery_UE_FieldMapping_20260503.md` | Defines `list_blueprint_helper_transactions` and `read_blueprint_helper_transaction` as read-only query tools. Needs to be re-scoped as a Review transaction record consumer, not normal Agent workflow. |
| Transaction query implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_TransactionJournalQuery_UE_CPP_Implementation_Plan_20260503.md` | Source layer plan for transaction query services and store paths. |
| Rollback cleanup field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_RollbackCleanupTransaction_UE_FieldMapping_20260503.md` | Cleanup-transaction rollback field contract. It is narrower than general Review Reject. |
| Rollback cleanup implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_RollbackCleanupTransaction_UE_CPP_Implementation_Plan_20260503.md` | Source layer plan for rollback cleanup service. |
| Cleanup block field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_CleanupBlueprintHelperBlock_UE_FieldMapping_20260503.md` | Cleanup operation contract for BlueprintHelper-owned blocks. |
| Cleanup block implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_CleanupBlueprintHelperBlock_UE_CPP_Implementation_Plan_20260503.md` | Source layer plan for cleanup service. |
| GraphWrite setup cleanup | `Resources/BlueprintHelper_Hybrid_SourceDocs_Sync_20260504/GraphWrite_Setup_Cleanup_Synced_20260504.md` | Related cleanup / ownership context for GraphWrite-managed content. |

Ownership conversion sources:

| Area | File | Current meaning |
| --- | --- | --- |
| Convert block to user owned types | `Source/BlueprintHelper/Public/Structure/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedTypes.h` | Current typed contract is block-level BlueprintHelper-owned -> user-owned. |
| Convert block to user owned service | `Source/BlueprintHelper/Public/Services/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.h` | Public service entry for block conversion. |
| Convert block to user owned implementation | `Source/BlueprintHelper/Private/Services/CleanupOwnership/BlueprintHelperConvertBlockToUserOwnedService.cpp` | Current implementation path. |
| Cleanup ownership adapter | `Source/BlueprintHelper/Public/TaskRuntime/TaskPlanAdapters/CleanupOwnership/BlueprintHelperCleanupOwnershipTaskPlanAdapter.h` and `Source/BlueprintHelper/Private/TaskRuntime/TaskPlanAdapters/CleanupOwnership/BlueprintHelperCleanupOwnershipTaskPlanAdapter.cpp` | TaskRuntime adapter currently supports cleanup, convert block to user-owned, and rollback cleanup transaction. |
| Ownership metadata support | `Source/BlueprintHelper/Public/GraphSupport/BlueprintHelperOwnershipService.h` and `Source/BlueprintHelper/Private/GraphSupport/BlueprintHelperOwnershipService.cpp` | Low-level ownership metadata read/write support. |

Current ownership gap:

- BH-owned -> user-owned exists only at block level.
- Asset-level and graph-level conversion are not present as first-class contracts.
- User-owned -> BH-owned is not implemented yet.
- For Review, both directions should be discussed as user-side review actions or review-only tools, not normal Agent default tools.

## 5. TaskRunJournal And Agent-Facing Boundary

| Area | File | Current meaning |
| --- | --- | --- |
| MCP tool API reference | `Docs/MCP_Tools_API_Reference.md` | Defines TaskSpec-first tool shape and Agent-visible `TaskRunJournal.v1` result access. |
| Agent safety and recovery guide | `Resources/AgentGuide/Workflows/07_Safety_Validation_And_Recovery.md` | Agent-facing preview / execute recovery rules. It must not teach ReviewPanel operation. |
| Task result store | `BlueprintHelper_MCP_Server/src/task-result-store.ts` | MCP-side in-process and bridge journal normalization. |
| Task result store tests | `BlueprintHelper_MCP_Server/src/task-result-store.test.ts` | Tests transaction_id extraction and TaskRunJournal normalization. |
| Task schema | `BlueprintHelper_MCP_Server/src/task-schemas.ts` | Defines `TaskRunJournalSchema`, including `status`, step records, `blocked_by_step_ids`, `transaction_id`, and `recovery`. |
| TaskRunJournal schema regression | `BlueprintHelper_MCP_Server/src/task-run-journal.schema.test.ts` | Verifies partial failure with blocked dependent steps and recovery guidance. |
| Task tools | `BlueprintHelper_MCP_Server/src/task-tools.ts` | Implements preview, execute, and get task result. |
| UE Task Runtime | `Source/BlueprintHelper/Public/TaskRuntime/BlueprintHelperTaskRuntimeService.h` and `Source/BlueprintHelper/Private/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp` | Builds UE-side `TaskRunJournal.v1`, stores task journals in runtime memory, and reports partial failure / recovery. |

Separation that should remain:

```text
Agent-visible:
TaskRunJournal.v1, task_run_id, step status, preview / execute errors, recovery summary.

User Review-visible:
ReviewRecord, visible changes, source transaction chain, archive baseline, Accept / Reject state.

Internal / developer diagnostics:
Transaction Journal details, rollback_data, diff snapshots, debug bundles.
```

## 6. Review Source Implementation Map

Review DTOs and services:

| File | Current role |
| --- | --- |
| `Source/BlueprintHelper/Public/Structure/Review/BlueprintHelperReviewTypes.h` | Review enums, atomic targets, visible changes, transaction input, action result, status routing helpers. |
| `Source/BlueprintHelper/Public/Services/Review/BlueprintHelperReviewStoreService.h` | Public ReviewStore API: build visible changes, load pending changes, normalize graph block target ids. |
| `Source/BlueprintHelper/Private/Services/Review/BlueprintHelperReviewStoreService.cpp` | Reads `Saved/BlueprintHelper/Review`, collapses source transactions into visible changes, handles atomic target grouping and latest-wins behavior. |
| `Source/BlueprintHelper/Public/Services/Review/BlueprintHelperReviewActionService.h` | Public Review action API. |
| `Source/BlueprintHelper/Private/Services/Review/BlueprintHelperReviewActionService.cpp` | First-slice Accept works in memory; Reject returns needs-action because archive-baseline rollback backend is not wired yet. |
| `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp` | Tests color/status mapping, visible change collapse, atomic intersections, Accept, Reject needs-action, panel construction, graph bounds, debug text. |

Review UI:

| File | Current role |
| --- | --- |
| `Source/BlueprintHelper/Public/Widgets/Review/SBlueprintHelperReviewPanel.h` | Main user-side Review panel declaration. |
| `Source/BlueprintHelper/Private/Widgets/Review/SBlueprintHelperReviewPanel.cpp` | Slate Review page, final change list, fake Blueprint panels, graph preview, Accept / Reject buttons. |
| `Source/BlueprintHelper/Public/Widgets/Review/BlueprintHelperReviewGraphResolver.h` and `Source/BlueprintHelper/Private/Widgets/Review/BlueprintHelperReviewGraphResolver.cpp` | Graph resolving for review preview. |
| `Source/BlueprintHelper/Public/Widgets/Review/BlueprintHelperReviewGraphBounds.h` and `Source/BlueprintHelper/Private/Widgets/Review/BlueprintHelperReviewGraphBounds.cpp` | Graph-space bounds for diff frames. |
| `Source/BlueprintHelper/Public/Widgets/Review/BlueprintHelperReviewDiffBlockNode.h` and `Source/BlueprintHelper/Private/Widgets/Review/BlueprintHelperReviewDiffBlockNode.cpp` | Transient diff block node support. |
| `Source/BlueprintHelper/Public/Widgets/Review/BlueprintHelperReviewDebugText.h` and `Source/BlueprintHelper/Private/Widgets/Review/BlueprintHelperReviewDebugText.cpp` | Review debug text formatting. |
| `Source/BlueprintHelper/Private/Widgets/SBlueprintHelperMainWindow.cpp` | Hosts the Review tab in the BlueprintHelper window. |

Transaction services:

| File | Current role |
| --- | --- |
| `Source/BlueprintHelper/Public/Transactions/Transactions/BlueprintHelperTransactionJournalService.h` | Public append journal API. |
| `Source/BlueprintHelper/Private/Transactions/Transactions/BlueprintHelperTransactionJournalService.cpp` | Writes active transaction JSON and Review-store copies under project Saved paths. |
| `Source/BlueprintHelper/Public/Transactions/Transactions/BlueprintHelperTransactionQueryService.h` | Public query API. |
| `Source/BlueprintHelper/Private/Transactions/Transactions/BlueprintHelperTransactionQueryService.cpp` | Transaction query implementation. |
| `Source/BlueprintHelper/Public/Structure/Transactions/BlueprintHelperTransactionQueryTypes.h` | Transaction query DTOs. |

## 7. Diagnostics And DebugExport References

| Area | File | Current meaning |
| --- | --- | --- |
| Diagnostics synced design | `Resources/BlueprintHelper_Hybrid_SourceDocs_Sync_20260504/05_Validation_Diagnostics_Tools_Design_Synced_20260504.md` | Diagnostic tool boundaries and validation context. |
| Diagnostics field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_Diagnostics_UE_FieldMapping_20260503.md` | UE diagnostics field contract. |
| Diagnostics implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_Diagnostics_UE_CPP_Implementation_Plan_20260503.md` | Source implementation plan for diagnostics. |
| DebugExport field mapping | `Resources/v0.3.6/FieldMapping/BlueprintHelper_DebugExport_LargePayload_UE_FieldMapping_20260503.md` | Old large-payload/debug export mapping. Needs redesign as developer diagnostics, not bulk Agent reading. |
| DebugExport implementation plan | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_DebugExport_LargePayload_UE_CPP_Implementation_Plan_20260503.md` | Old implementation plan. Useful only as prior art for debug bundle shape. |
| DebugExport types | `Source/BlueprintHelper/Public/Structure/RuntimeDiagnostics/BlueprintHelperDebugExportTypes.h` | Current DTO/error code starting point for developer debug export. |
| Current gap matrix | `Resources/Plan/BlueprintHelper_v0.3.6_Current_Implementation_Gap_Matrix_20260505.md` | Tracks DebugExport as an independent developer diagnostics system. |
| Current TODO | `Resources/Plan/BlueprintHelper_Current_TODO_20260506.md` | Tracks DebugExport and TransactionJournalQuery / Review aggregation as active TODOs. |

Debug linkage decision for Review discussion:

- Do not use DebugBundle as the Review transaction storage model.
- ReviewRecord links to `debug_case_ids[]`; DebugCase links back to transactions, task_run_id, ReviewRecord, and error context.
- DebugBundle export starts from DebugCase and is generated only when users or developers need to debug Review / rollback / task failures.

## 8. Capability Producer Documents That Mention Review / Journal

These are not Review model documents, but they describe producers of transactions that the Review record model must consume.

| Producer | File |
| --- | --- |
| Replace GraphWrite | `Resources/v0.3.6/FieldMapping/BlueprintHelper_ReplaceBlueprintGraph_UE_FieldMapping_20260503.md` |
| Replace GraphWrite implementation | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_ReplaceBlueprintGraph_UE_CPP_Implementation_Plan_20260503.md` |
| Patch GraphWrite | `Resources/v0.3.6/FieldMapping/BlueprintHelper_PatchBlueprintGraph_UE_FieldMapping_20260503.md` |
| Patch GraphWrite implementation | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_PatchBlueprintGraph_UE_CPP_Implementation_Plan_20260503.md` |
| Merge GraphWrite | `Resources/v0.3.6/FieldMapping/BlueprintHelper_MergeBlueprintGraph_UE_FieldMapping_20260503.md` |
| Merge GraphWrite implementation | `Resources/v0.3.6/DoneImplementaion/BlueprintHelper_MergeBlueprintGraph_UE_CPP_Implementation_Plan_20260503.md` |
| Function / Event Signature | `Resources/Plan/BlueprintHelper_FunctionEventSignature_UE_CPP_Implementation_Plan_20260503.md` |
| Runtime profile boundary | `Resources/v0.3.6/FieldMapping/BlueprintHelper_RuntimeProfile_UE_FieldMapping_20260503.md` |
| Save asset boundary | `Resources/v0.3.6/FieldMapping/BlueprintHelper_SaveAsset_UE_FieldMapping_20260503.md` |
| Common tool result envelope | `Resources/v0.3.6/FieldMapping/BlueprintHelper_ToolResultBase_CommonEnvelope_UE_FieldMapping_20260503.md` |
| Logic read grouped ownership | `Resources/v0.3.6/FieldMapping/BlueprintHelper_LogicRead_Grouped_UE_FieldMapping_20260502.md` |

Discussion use:

- These files define how transaction producers identify targets.
- Review should not copy every producer schema into the UI.
- Review needs a normalized `ReviewAtomicTarget` and `ReviewVisibleChange` layer that can consume all producer-specific records.

## 9. Current Model Facts

Current persisted / runtime stores:

```text
UE Transaction Journal:
Saved/BlueprintHelper/Transactions/Active/<transaction_id>.json
Saved/BlueprintHelper/Transactions/Archived/<transaction_id>.json

UE Review Store copy:
Saved/BlueprintHelper/Review/<transaction_id>.json

TaskRunJournal:
UE runtime memory first, MCP in-process fallback.
```

Current flow:

```text
TaskSpec
-> TaskPlan
-> UE Task Runtime
-> capability service write
-> TransactionJournalService append
-> Review store copy
-> TaskRunJournal summary
-> user-side ReviewStore builds visible changes
```

Current Review visible-change behavior:

- Review UI shows final visible changes, not raw transaction rows.
- A visible change is computed from archive baseline to current asset state.
- Later transactions override only the atomic targets they touch.
- Multiple atomic targets may be grouped into one visible UI leaf.
- Accept / Reject operate on the visible change leaf.
- Superseded source transactions remain internal until compaction policy allows cleanup.

Current implementation state:

- Review DTOs, ReviewStore, ReviewAction, ReviewPanel, graph diff node support, and tests exist in source.
- ReviewStore can collapse transaction inputs into visible changes.
- ReviewAction first slice can Accept in memory.
- ReviewAction Reject currently returns needs-action because archive-baseline rollback backend is not wired.
- TransactionJournalService writes active transaction JSON and Review store copies.
- TransactionQuery C++ layer exists.
- TaskRunJournal schema, MCP store, UE runtime journal builder, and tests exist.
- Full persistent ReviewRecord aggregation across UE write transactions and task_run_id is still missing.

## 10. Proposed Review Record Shape For Discussion

This is a discussion target, not an implemented schema.

```json
{
  "schema": "BlueprintHelper.ReviewRecord.v1",
  "review_record_id": "review_...",
  "task_run_id": "task_...",
  "archive_session_id": "archive_...",
  "asset_paths": ["/Game/..."],
  "status": "pending_review",
  "transactions": [
    {
      "transaction_id": "tx_...",
      "operation": "replace_blueprint_graph",
      "status": "applied",
      "task_step_id": "step_...",
      "created_at": "..."
    }
  ],
  "visible_changes": [
    {
      "change_id": "change_...",
      "asset_path": "/Game/...",
      "surface": "graph",
      "graph_name": "EventGraph",
      "visual_group_key": "graph:EventGraph:block:...",
      "atomic_targets": [
        {
          "target_key": "node:...",
          "latest_transaction_id": "tx_...",
          "source_transaction_ids": ["tx_1", "tx_2"]
        }
      ],
      "status": "pending",
      "before_summary": "...",
      "after_summary": "..."
    }
  ],
  "review_actions": [
    {
      "action": "accept",
      "change_id": "change_...",
      "ownership_policy": "keep_managed",
      "created_at": "..."
    }
  ],
  "rollback": {
    "baseline": "archive_baseline",
    "status": "not_required"
  },
  "diagnostics": {
    "debug_case_ids": []
  }
}
```

## 11. Open Decisions For Web Discussion

1. ReviewRecord identity

Should one ReviewRecord map to one `task_run_id`, one asset, one archive session, or a combined group?

2. TaskRunJournal grouping

Should Review UI default to task-level groups, asset-level groups, visible-change groups, or a mixed tree?

3. TransactionJournalQuery scope

Should existing list/read transaction query become:

```text
ReviewRecord query
-> visible changes
-> source transaction summaries
-> optional debug / rollback details
```

instead of exposing raw transaction-first browsing?

4. Archive session persistence

What is the durable archive format, when is baseline captured, and who recovers unfinished sessions after editor crash?

5. Reject semantics

Reject currently means rollback affected visible targets to archive baseline. Need define item-level rollback conflict rules when one visible change spans graph, variables, components, and details.

6. Accept ownership policy

Default remains keep managed. Need decide where `Accept and convert to user-owned` lives, and whether conversion creates:

```text
review_action only
transaction only
both review_action and transaction
```

7. Ownership conversion scope

Needed scopes:

```text
block
graph
asset
```

Current implementation only has block-level BH-owned -> user-owned. User-owned -> BH-owned is missing and needs a separate contract.

8. Compaction policy

Need retention levels for:

```text
pending
accepted
rejected
rollback_blocked
rollback_failed
archived
compacted
```

9. Debug linkage

Resolved direction: ReviewRecord links to `debug_case_ids[]`; DebugBundle is generated from DebugCase and does not become an Agent bulk-read path.

10. Non-BlueprintHelper-owned anchors

Review and ownership migration need stable read/write anchors for user-owned and legacy graph content. Current GraphWrite mainline only has reliable block-scoped anchors for BlueprintHelper-owned blocks.

## 12. Suggested Meeting Output

The web discussion should produce:

- `BlueprintHelper.ReviewRecord.v1` schema decision.
- Archive session lifecycle and persistence decision.
- ReviewRecord query surface replacing raw TransactionJournalQuery as the user-facing model.
- Accept / Reject state machine.
- Ownership conversion tool scope and review-only exposure decision.
- DebugCase / DebugBundle linkage contract.
- TODO updates for implementation order.
