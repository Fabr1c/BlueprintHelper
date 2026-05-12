# BlueprintHelper Review / Debug Parallel Execution Plan

> **For agentic workers:** Use `subagent-driven-development` or `executing-plans` when implementing a phase. Track steps with checkbox syntax and do not cross the phase ownership locks.

**Goal:** Allow two independent sessions to develop Review and Debug in parallel without fighting over TaskRuntime, ToolResult, module startup, Bridge routing, or TransactionJournal shared seams.

**Architecture:** Review remains the user audit system. Debug remains the developer diagnostics and local bundle export system. Transaction and TaskRuntime are shared fact seams, so each phase assigns one owner for shared files while the other session works only inside its own system directories.

**Tech Stack:** Unreal Engine C++ plugin, TypeScript MCP server, Python orchestration, UE Automation, Node/Python regression tests, Markdown status documents.

---

## 0. Baseline And Branching

Current baseline expectation:

```text
The tool-cluster entry refactor is already closed.
The baseline branch builds in the local UE environment.
Both Review and Debug sessions must start from the same baseline commit.
Speed optimization, broad source relocation, and new Animation Blueprint / Material tools are out of this plan.
```

Recommended branches:

```text
codex/debug-system-p1
codex/review-system-p1
codex/review-debug-integration
```

Branch rules:

```text
1. Create both feature branches from the same clean baseline commit.
2. Do not start P1/P2 shared-seam work on a dirty branch.
3. Merge Debug P1 into the integration branch before Review opens shared seams in P2.
4. Merge Review P2 into the integration branch before final cross-system linkage.
5. If a session discovers unrelated dirty files, do not revert them; either ignore them or stop if they block the assigned phase.
```

Shared contract locked by this plan:

```text
ToolResultBase exposes only debug_case_ids[] summary references.
ToolResultBase never exposes DebugBundle artifact content.
ReviewRecord stores debug_case_ids[] for Debug linkage.
ReviewRecord never stores DebugBundle local paths.
DebugCase can store review_record_ids[].
Review Reject failed / needs_action reports to DebugEntry.
Review code does not write DebugCase files directly.
```

---

## 1. Phase Locks

### P1: Debug owns shared seams

Debug session may modify:

```text
Source/BlueprintHelper/Public/Shared/BlueprintHelperToolResultTypes.h
Source/BlueprintHelper/Private/Shared/BlueprintHelperToolResultBuilder.cpp
Source/BlueprintHelper/Public/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h
Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp
Source/BlueprintHelper/Public/Systems/Transactions
Source/BlueprintHelper/Private/Systems/Transactions
Source/BlueprintHelper/Public/Entry/Bridge
Source/BlueprintHelper/Private/Entry/Bridge
Source/BlueprintHelper/Public/Entry/BlueprintHelper.h
Source/BlueprintHelper/Private/Entry/BlueprintHelper.cpp
ClaudePlugin/mcp/src
```

Debug session should create or extend:

```text
Source/BlueprintHelper/Public/Shared/Debug
Source/BlueprintHelper/Private/Shared/Debug
Source/BlueprintHelper/Public/Systems/Debug
Source/BlueprintHelper/Private/Systems/Debug
Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics
```

Review session during P1 may modify only:

```text
Source/BlueprintHelper/Public/Shared/Review
Source/BlueprintHelper/Private/Shared/Review
Source/BlueprintHelper/Public/Systems/Review
Source/BlueprintHelper/Private/Systems/Review
Source/BlueprintHelper/Private/Tests/Review
Develop/Design/BlueprintHelper_Review_System_Boundary_v1_20260508.md
```

Review session during P1 must not modify:

```text
TaskRuntimeService
ToolResultBase / ToolResultBuilder
BridgeRouter / RoutePlanner
TransactionJournal
BlueprintHelper module startup
MCP tool registration
```

P1 exit gate:

```text
Debug branch merged into codex/review-debug-integration.
Review branch rebased onto or merged with Debug P1 before P2 starts.
git diff --check passes on the integration branch.
```

### P2: Review owns shared seams

Review session may modify:

```text
TaskRuntimeService
TransactionJournal
TaskRuntimeCluster implementations
ReviewStore / ReviewAction
ReviewPanel query/action consumption
Tool-cluster producer evidence builders
```

Debug session during P2 may modify only:

```text
Source/BlueprintHelper/Public/Shared/Debug
Source/BlueprintHelper/Private/Shared/Debug
Source/BlueprintHelper/Public/Systems/Debug
Source/BlueprintHelper/Private/Systems/Debug
Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics
Developer Debug UI internals
Debug docs
```

Debug session during P2 must not modify:

```text
TaskRuntimeService
ToolResultBase / ToolResultBuilder
TransactionJournal
ReviewStore / ReviewAction
BridgeRouter / RoutePlanner
BlueprintHelper module startup
```

P2 exit gate:

```text
Review branch merged into codex/review-debug-integration.
Debug branch rebased onto or merged with Review P2 before P3 starts.
Review producer and rollback tests pass.
```

### P3: Integration owns cross-system links

Only the integration branch handles:

```text
Review Reject failed / needs_action -> DebugEntry
DebugEntry -> DebugCase review_record_ids[]
ReviewRecord debug_case_ids[] persistence
DebugBundle Review summary artifact
Transaction links with review_reject_failed role
Module construction ordering across Debug, Review, TaskRuntime, Bridge, and UI
```

P3 must not add new broad feature scope. It only connects systems already implemented in P1 and P2.

---

## 2. Debug Session Tasks

### P1 Debug: Shared foundation and automatic capture

- [x] Add `DebugEvent.v1`, `DebugCase.v1`, and `DebugBundleManifest.v1` DTOs under Shared/Debug.
- [x] Add DebugCase store service that writes only under `Saved/BlueprintHelper/Debug`.
- [x] Add unified DebugEntry facade with best-effort event recording.
- [x] Add `debug_case_ids[]` to ToolResultBase failure output only.
- [x] Add Bridge / MCP summary query for `get_debug_case`; do not add DebugBundle readers.
- [x] Capture DebugCase for malformed Bridge response and transport failure.
- [x] Capture DebugCase for TaskRuntime preview blocker, execute failure, and partial_failure.
- [x] Capture DebugCase for compile/save failure. Save failure and compile tool-level failures are wired; real Blueprint compile diagnostics are resolved in P2 by returning a ToolResult failure with `data.compile_result.success=false`.
- [x] Capture DebugCase for transaction rollback failure.
- [x] Add tests proving no MCP DebugBundle artifact reader or large-payload debug path exists.

Debug progress sync, 2026-05-09:

```text
P1 Debug has been merged into the main workspace and checked against the P1 task list.
Confirmed present: DTOs, DebugCase store, DebugEntry facade, ToolResultBase debug_case_ids[] failure-only output, get_debug_case Bridge/MCP query, malformed Bridge request capture, Bridge transport failure capture, TaskRuntime preview/execute/partial failure capture, save failure capture, transaction rollback failure capture, and MCP no-DebugBundle-reader regression tests.
Carryover issue handed to P2: Blueprint compile diagnostics where the asset exists but compilation fails returned ToolResult ok=true with data.compile_result.success=false, so these real compile failures did not surface debug_case_ids[] and were not treated as TaskRuntime post-operation failures.
P2 resolution: real Blueprint compile diagnostics failure now returns ToolResult `ok=false`, keeps `data.compile_result.success=false`, uses `compile_failed`, and attaches DebugCase when DebugEntry is available.
Verification run in this session: git diff --check passed; MCP npm.cmd test passed with Python 44 tests OK and Node 140 tests passing.
Full UE build was not run in this sync.
```

### P2 Debug: Debug internals only

- [x] P1 carryover: make real Blueprint compile failure produce a DebugCase-visible failure path, or explicitly define and test the diagnostic-success behavior so DebugCase capture is not expected for data.compile_result.success=false.
- [x] Implement DebugBundle manifest and summary export.
- [x] Implement standard privacy redaction rules for tokens, full settings, local absolute paths, full raw JSON, and source file content.
- [x] Add skipped artifact reporting when redaction cannot safely pass.
- [x] Add DebugCase cleanup policy for resolved low-severity cases without deleting needs_action or rollback_failed cases.
- [x] Add Developer Debug UI internals for list, export, and cleanup without touching Review UI.

Debug progress sync, 2026-05-09:

```text
P2 Debug internals have been merged into the main workspace.
Implemented: real Blueprint compile diagnostics now produce a DebugCase-visible failure result; DebugBundle summary export writes manifest.json plus summary.json under Saved/BlueprintHelper/Debug/Bundles with relative refs only; standard redaction covers tokens, full settings/snapshots, local absolute paths, raw JSON fields, and source content; skipped artifacts are reported for unsafe full event payloads; cleanup archives resolved low-severity cases without deleting needs_action or rollback_failed cases; DebugEntry exposes developer-internal list, export, and cleanup results without touching Review UI.
Tests added: RuntimeDiagnostics Debug automation tests cover compile failure behavior, redacted bundle summary export, cleanup policy, and developer UI internals.
Verification run in this session: git diff --check passed for tracked changes with only LF/CRLF warnings; Debug P2 new-file trailing whitespace scan passed; conflict marker scan found no matches; MCP npm.cmd test passed with Python 44 tests OK and Node 140 tests passing.
Full UE Build Tool was intentionally skipped because the user will run the unified compile.
```

### P3 Debug integration

- [x] Accept Review-supplied `review_record_id` when recording Review reject failed / needs_action events.
- [x] Store `review_record_ids[]` on DebugCase.
- [x] Export Review summary artifact into DebugBundle without storing local bundle paths in ReviewRecord.

Debug progress sync, 2026-05-09:

```text
P3 Debug integration has been merged into the main workspace.
Implemented: Review reject needs_action / reject_failed now passes review_record_id into DebugEntry; DebugEvent, DebugCase, and DebugCaseSummary persist review_record_ids[]; DebugBundle summary export can use ReviewStore to write relative review/*.summary.json artifacts into the bundle manifest; ReviewRecord remains free of DebugBundle local paths and legacy DebugExportRefs have been removed from the active contract.
Tests added: RuntimeDiagnostics Debug automation covers review_record_ids summary persistence and Review summary artifact export; Review integration tests now assert reject needs_action and reject_failed DebugCases link back to the originating ReviewRecord.
Verification run in this session: git diff --check passed for tracked changes with only LF/CRLF warnings; Debug P3 new-file trailing whitespace scan passed; conflict marker scan found no matches; MCP npm.cmd test passed with Python 44 tests OK and Node 140 tests passing.
Full UE Build Tool was intentionally skipped because the user will run the unified compile.
```

Debug completion sync, 2026-05-09:

```text
Remaining Debug-side contract gap closed: DebugEvent, DebugCase, and DebugCaseSummary now persist transaction_links[] as summary-only transaction references.
Producers wired: Review reject needs_action / reject_failed records the source transaction as role=review_reject_failed; rollback cleanup failure records the target transaction as role=rollback_target.
Tests added: RuntimeDiagnostics Debug entry summary test now asserts transaction_links[] serialization; Review integration reject tests assert DebugCases link the originating source transaction summary.
Verification run in this session: git diff --check passed for tracked changes with only LF/CRLF warnings; Debug completion file trailing whitespace scan passed; conflict marker scan found no matches; MCP npm.cmd test passed with Python 44 tests OK and Node 140 tests passing.
Full UE Build Tool was intentionally skipped because the user will run the unified compile.
```

---

## 3. Review Session Tasks

### P1 Review: Internal persistence and status propagation only

- [x] Add `debug_case_ids[]` on ReviewRecord and remove legacy `DebugExportRefs` from the active contract.
- [x] Persist `review_actions[]` after Accept, Reject, RejectAll, and ConvertOwnerBlock.
- [x] Propagate per-target status to visible-change status and record status.
- [x] Add unit tests for Accept action history and status propagation.
- [x] Add unit tests for Reject status transitions using explicit options, without invoking TaskRuntime.
- [x] Add unit tests for RejectAll target-by-target behavior.
- [x] Add ConvertOwnerBlock policy gate tests without transaction execution.

### P2 Review: Shared seams, producer evidence, rollback

- [x] Create ArchiveSession at TaskRuntime execute start.
- [x] Capture allowed target asset baseline before first real write.
- [x] Store baseline snapshots under `Saved/BlueprintHelper/Review/Snapshots/<archive_session_id>/...`.
- [x] Move Review evidence production into each asset-mutating tool cluster.
- [x] Require every producer evidence item to contain transaction id, archive session id, task run id, asset path, target kind, target anchor, visual group key, baseline hash, recorded-after hash, and rollback data ref.
- [x] Remove fallback evidence from the main TaskRuntime path; keep only a defensive failure or test-only compatibility path.
- [x] Persist ReviewRecord by `archive_session_id + asset_path`.
- [x] Make ReviewPanel load through ReviewRecordQuery rather than raw transaction browsing.
- [x] Implement Reject mechanical rollback dispatcher keyed by target kind.
- [x] Reject must fail without mutation if anchor, rollback ref, or `current_hash == recorded_after_hash` check fails.
- [x] RejectAll iterates pending filtered targets without dependency ordering.
- [x] Implement ConvertOwnerBlock for `bh_to_user` and `user_to_bh`, creating a transaction and review action.

Review progress sync, 2026-05-09:

```text
P1 Review: implemented and covered by Review action/store tests.
P2 Review: implemented. Producer-owned evidence now routes through task runtime clusters, including AssetFactory; journal-backed GraphWrite stays journal-owned.
P2 verification run in this session: direct cl compile passed for BlueprintHelperReviewStoreServiceTests.cpp, BlueprintHelperTaskRuntimeClusterHubTests.cpp, BlueprintHelperGraphWriteTaskRuntimeCluster.cpp, and BlueprintHelperTaskRuntimeService.cpp.
Full UBT build was not continued because the user requested no build.
```

### P3 Review integration

- [x] On Reject needs_action or reject_failed, call DebugEntry rather than writing Debug files.
- [x] Persist returned `debug_case_id` into ReviewRecord `debug_case_ids[]`.
- [x] Ensure ReviewRecord never stores DebugBundle local path.

Review progress sync, 2026-05-09:

```text
P3 Review: implemented for Review-side reject failure paths.
Reject needs_action and reject_failed now record a DebugCase through DebugEntry and persist only debug_case_ids[] on ReviewRecord.
Reject needs_action and reject_failed now include a summary-only transaction link with role=review_reject_failed.
ReviewRecord still does not store DebugBundle local paths; tests assert the new failure-linking path persists only debug_case_ids[].
P3 verification run in this session: direct cl /Y- compile passed for BlueprintHelperReviewStoreServiceTests.cpp, BlueprintHelperReviewActionService.cpp, and BlueprintHelper.cpp.
Full UBT build was not run because the user requested no build.
```

---

## 4. Shared Ordering And Construction Rules

Module construction order in integration:

```text
TransactionJournal
Debug services
Review services
Tool cluster services
TaskRuntimeService
BridgeRouter
UI
```

Reason:

```text
TaskRuntime needs Transaction, Debug, Review, and tool services.
Review Reject failure can call DebugEntry.
DebugBundle can read Review summaries by stable ids.
UI consumes Review and Debug query/action services after both are constructed.
```

Shared DTO rules:

```text
ToolResultBase: debug_case_ids[] only; no DebugBundle path or artifact content.
ReviewRecord: debug_case_ids[] only for Debug linkage; no DebugBundle path.
DebugCase: review_record_ids[] and transaction_links[] summaries only.
Transaction link roles: succeeded_before_failure, failed_transaction, rollback_failed, review_reject_failed.
```

---

## 5. Verification Matrix

P1 Debug required:

```text
DebugCase minimal schema/store tests
ToolResult failure includes debug_case_ids[]
get_debug_case returns summary only
No MCP artifact reader
No large payload debug reader
TaskRuntime preview blocker creates DebugCase
TaskRuntime partial_failure creates DebugCase
Compile/save failure creates DebugCase
Rollback failure creates DebugCase
```

P1 Review required:

```text
Persisted action history
Record/change/target status propagation
Accept internal state
Reject internal state
RejectAll target iteration
ConvertOwnerBlock policy gate
```

P2 Review required:

```text
ArchiveSession created before first write
Baseline snapshot captured before mutation
Every write cluster emits producer-owned WriteReviewEvidence.v1
ReviewStore does not infer missing anchors
Reject success mutates exactly selected targets
Reject TOCTOU mismatch does not mutate asset
ReviewPanel loads via ReviewRecordQuery
```

P2 Debug required:

```text
DebugBundle creates manifest and summary
Standard redaction removes sensitive content
Skipped artifacts are reported
Cleanup does not remove needs_action or rollback_failed
Developer Debug UI does not expose Review editing
```

P3 integration required:

```text
Review Reject failed creates DebugCase
ReviewRecord links debug_case_ids[]
DebugCase links review_record_ids[]
DebugBundle includes Review summary artifact
ReviewRecord does not store bundle path
Transaction links include review_reject_failed role
```

Full commands:

```powershell
git diff --check
```

```powershell
cd ClaudePlugin/mcp
npm.cmd test
```

```powershell
F:/UE_5.6/Engine/Build/BatchFiles/Build.bat MrStoneEditor Win64 Development -Project=G:/UnrealPractise/MrStone/MrStone.uproject -WaitMutex -NoUBA -MaxParallelActions=1
```

UE Automation focus:

```text
BlueprintHelper.Review
BlueprintHelper.Review.Producer
BlueprintHelper.Review.Rollback
BlueprintHelper.Debug
BlueprintHelper.RuntimeDiagnostics
```

---

## 6. Worker Assignment Rules

Use these rules inside each session:

```text
Documentation, source search, and current-state summaries:
  gpt-5.3-codex-spark xhigh

Diff review, compile diagnostics, fixture comparison, and test execution:
  gpt-5.4 xhigh

DTO design, TaskRuntime integration, rollback, DebugEntry, and Review state machines:
  gpt-5.5 xhigh
```

Concurrency limits:

```text
No two workers may edit the same file.
No worker may cross the current phase ownership lock.
Shared files are edited only by the phase owner.
Integration-only links are deferred to P3.
```

---

## 7. Status Log

- [x] 2026-05-08: Parallel execution plan created.
- [ ] P0: Clean baseline commit selected for both sessions.
- [x] P1 Debug: Shared-seam foundation merged into main workspace; real Blueprint compile failure DebugCase behavior resolved in P2.
- [x] P1 Review: Review-internal action persistence complete.
- [x] P1 Gate: Debug P1 merged before Review opens shared seams.
- [x] P2 Review: ArchiveSession, producer evidence, and rollback complete.
- [x] P2 Debug: Real compile failure carryover, bundle export, redaction, cleanup, and Developer Debug UI internals implemented; UE compile verification pending.
- [x] P2 Gate: Review P2 merged before cross-system linkage.
- [x] P3: Review -> Debug integration implemented; targeted Review cl verification passed; full UE build intentionally not run.
- [ ] Full verification complete.

2026-05-09 Review status sync: Review-side P1, P2, and P3 tasks are complete in the merged workspace. Targeted verification was limited to Review action/store files by request; full build remains open.
