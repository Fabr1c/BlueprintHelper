# Review Baseline Snapshot Future Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade Review baseline capture so pending review, diff, debug, and future rollback never depend on stale disk-only `.uasset` snapshots.

**Architecture:** Keep the current short-term behavior unchanged. Future work introduces an explicit dirty-asset baseline policy first, then adds live semantic snapshots for Blueprint, WidgetBlueprint, DataTable, DataAsset, and generic UObject surfaces. Disk `.uasset` snapshots become optional archive evidence instead of the only baseline source.

**Tech Stack:** Unreal Engine editor plugin C++, BlueprintHelper TaskRuntime, Review Store, Review Action Service, ReviewPanel, UE package dirty-state APIs, JSON archive records.

---

## Status

This is a future plan only. Do not implement in the current ReviewPanel v2 pass.

Current implementation captures `Review/Snapshots` by copying target asset `.uasset` files from disk when `execute_task_plan` creates an ArchiveSession. If a target asset is dirty in the editor before execution, the snapshot may represent the old saved disk state instead of the true in-memory execution baseline.

Severity: **P1 contract correctness risk**, not a current P0 compile or ReviewPanel rendering blocker.

Rationale:

- Clean saved assets produce valid disk baseline snapshots.
- Dirty target assets make disk-only snapshots stale.
- Current Reject flow primarily uses `rollback_data_ref` and `recorded_after_hash`, so this is not the sole current reject mechanism.
- Future full baseline diff, restore, and debug evidence would be unsafe if they treat disk snapshots as authoritative.

## Product Contract

Default behavior should match normal coding-tool practice: do not silently save user edits to create a baseline.

For text/code workflows, the baseline is normally the live file buffer or current workspace file content. If the tool cannot read unsaved editor buffers, it blocks instead of silently making user changes part of the baseline.

For BlueprintHelper, UE assets are structured binary/editor objects. The safer contract is:

- Default: block dirty target assets before archive creation.
- Explicit opt-in: save target assets before archive creation.
- Long term: capture live semantic baseline from loaded UE objects.

## Proposed TaskSpec Policy

Add a future execution policy field:

```json
{
  "execution_policy": {
    "review_baseline_dirty_asset_policy": "block"
  }
}
```

Supported values:

| Value | Behavior | Intended Use |
|---|---|---|
| `block` | If any target asset package is dirty before ArchiveSession creation, stop before writing. | Default production behavior |
| `save_before_archive` | After preview passes and before ArchiveSession creation, save target assets, then copy snapshots. | User-authorized automation/smoke runs |
| `allow_stale_disk_snapshot` | Keep current disk-copy behavior even when dirty assets exist, and mark snapshot evidence as potentially stale. | Diagnostics only |

## Future File Ownership

Likely files to modify when this plan is activated:

- `Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`
  - Parse `review_baseline_dirty_asset_policy`.
  - Check dirty state before `CaptureReviewBaselineSnapshots`.
  - Optionally save before archive.
  - Record policy and outcome in TaskRunJournal.

- `Source/BlueprintHelper/Public/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h`
  - Add small policy enum or local helper declarations if not kept private.

- `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewStoreService.cpp`
  - Persist baseline policy metadata in ArchiveSession JSON if the type is extended.

- `Source/BlueprintHelper/Public/Shared/Review/BlueprintHelperReviewTypes.h`
  - Add ArchiveSession fields such as `BaselineCaptureMode`, `BaselineDirtyPolicy`, `BaselineWarnings`.

- New future service:
  - `Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h`
  - `Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewBaselineSnapshotService.cpp`

- Tests:
  - `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`
  - New or existing TaskRuntime tests covering dirty policy behavior.

## Stage 1: Dirty Target Policy

### Task 1: Add Dirty Policy Parsing

- [ ] Add enum values for `block`, `save_before_archive`, and `allow_stale_disk_snapshot`.
- [ ] Parse missing policy as `block`.
- [ ] Reject unknown policy with a parse error.
- [ ] Add targeted automation for valid, default, and invalid policy values.

Expected behavior:

```text
missing policy -> block
review_baseline_dirty_asset_policy=block -> block
review_baseline_dirty_asset_policy=save_before_archive -> save_before_archive
review_baseline_dirty_asset_policy=allow_stale_disk_snapshot -> allow_stale_disk_snapshot
review_baseline_dirty_asset_policy=anything_else -> invalid_taskplan_execution_policy
```

### Task 2: Detect Dirty Target Packages Before Archive Creation

- [ ] Resolve `task_plan.target_assets`.
- [ ] Load each target asset.
- [ ] Read `Asset->GetOutermost()->IsDirty()`.
- [ ] If any target is dirty and policy is `block`, fail before `ArchiveSession` is saved.

Failure shape:

```json
{
  "code": "review_baseline_dirty_target_assets",
  "stage": "preflight",
  "message": "Review baseline requires saved target assets or review_baseline_dirty_asset_policy=save_before_archive.",
  "field": "task_plan.execution_policy.review_baseline_dirty_asset_policy"
}
```

### Task 3: Implement Explicit Save-Before-Archive

- [ ] If policy is `save_before_archive`, save dirty target assets after all dry-run/preview checks have passed and before `CaptureReviewBaselineSnapshots`.
- [ ] Use the existing asset save path where possible.
- [ ] Record each pre-archive save result in TaskRunJournal as a distinct pre-archive operation.
- [ ] If save fails, fail before ArchiveSession creation.

Contract note:

```text
User dirty edits saved by save_before_archive become the Review baseline.
They are not treated as changes introduced by the current TaskPlan and cannot be rejected as part of this review record.
```

### Task 4: Mark Stale Snapshot Evidence Explicitly

- [ ] If policy is `allow_stale_disk_snapshot` and any target asset is dirty, allow execution.
- [ ] Persist a warning in ArchiveSession and TaskRunJournal.
- [ ] Debug export must show `baseline_snapshot_trust=stale_disk_copy`.

Expected warning:

```text
Review baseline snapshot copied from disk while target asset was dirty in editor.
Snapshot is diagnostic evidence only and must not be used as authoritative rollback baseline.
```

## Stage 2: Live Semantic Snapshot Service

### Task 5: Create ReviewBaselineSnapshotService

- [ ] Add a service that captures semantic baselines from loaded UE objects before writes.
- [ ] Store semantic snapshots under:

```text
Saved/BlueprintHelper/Review/Snapshots/<archive_session_id>/<asset_hash>/baseline.semantic.json
```

- [ ] Keep optional disk package copy as:

```text
Saved/BlueprintHelper/Review/Snapshots/<archive_session_id>/<asset_hash>/baseline.uasset
```

### Task 6: Blueprint Semantic Baseline

- [ ] Capture graph names, node guids, node titles, pins, links, graph positions, variables, dispatchers, functions, macros, components, and class settings.
- [ ] Include stable target keys matching Review atomic targets.
- [ ] Compute semantic hash per target.

### Task 7: WidgetBlueprint Semantic Baseline

- [ ] Capture Blueprint graph and My Blueprint data.
- [ ] Capture WidgetTree hierarchy, widget names, classes, slots, and exposed property values.
- [ ] Use `umg_widget` and `umg_widget_property` target keys.

### Task 8: DataTable Semantic Baseline

- [ ] Capture row struct path.
- [ ] Capture row names and serialized row values.
- [ ] Use `datatable_row:<row_name>` target keys.

### Task 9: DataAsset and GenericObject Semantic Baseline

- [ ] Capture class path.
- [ ] Capture editable property paths and serialized values.
- [ ] Use `data_asset_property:<property_path>` and `object_property:<property_path>` target keys.

## Stage 3: Review/Diff/Debug Adoption

### Task 10: Prefer Semantic Baseline for Diff

- [ ] ReviewPanel and debug export should prefer `baseline.semantic.json` for human-readable before/after summaries.
- [ ] Disk `.uasset` remains evidence, not the primary diff source.

### Task 11: Reject Safety Integration

- [ ] Keep current `recorded_after_hash` guard.
- [ ] Add optional baseline semantic hash checks for improved diagnostics.
- [ ] Do not perform cross-transaction rollback automatically.

### Task 12: DebugBundle Summary

- [ ] Include dirty policy, dirty asset list, semantic snapshot refs, disk snapshot refs, and trust level.
- [ ] Keep MCP/debug APIs summary-only unless a local DebugBundle export is requested.

## Verification Plan

Targeted automation:

- `ReviewBaselinePolicyDefaultsToBlock`
- `ReviewBaselinePolicyRejectsUnknownValue`
- `ReviewBaselineDirtyTargetBlocksArchiveCreation`
- `ReviewBaselineSaveBeforeArchiveCapturesFreshDiskSnapshot`
- `ReviewBaselineAllowStaleWritesWarning`
- `ReviewBaselineSemanticSnapshotCapturesBlueprintSurfaces`
- `ReviewBaselineSemanticSnapshotCapturesWidgetTree`
- `ReviewBaselineSemanticSnapshotCapturesDataTableRows`
- `ReviewBaselineSemanticSnapshotCapturesDataAssetProperties`

Manual smoke:

- Dirty Blueprint target with default policy blocks before ArchiveSession creation.
- Dirty Blueprint target with `save_before_archive` saves first, then creates a snapshot.
- Dirty DataTable target with default policy blocks.
- Clean WidgetBlueprint still produces ReviewPanel changes normally.
- DebugBundle shows baseline trust metadata.

## Deferred Decisions

- Whether `save_before_archive` should be available globally or only for automation profiles.
- Whether semantic snapshots should be compacted after Accept.
- Whether rejected/needs_action records must retain full semantic snapshots indefinitely.
- Whether full `.uasset` package snapshots should be kept for large assets or only generated on explicit debug export.
