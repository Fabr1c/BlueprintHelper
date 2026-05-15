# BlueprintHelper User Review Panel Constraints

> 2026-05-14 状态转移：本文中的未达期待、待验证项和阻塞项已迁移到 [BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md](BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md)。本文保留为历史上下文；开放项跟踪迁移完成，后续当前状态以总账为准。

Date: 2026-05-06
Scope: UE plugin user-side Review panel, archive session, visible diff semantics
Status: confirmed constraints

## 1. Product Boundary

The Review page is a fake Blueprint review page. It should look like the useful parts of the Blueprint Editor, but it is not a real editable Blueprint Editor.

It renders:

- Components panel.
- My Blueprint panel.
- Blueprint graph page.
- Blueprint details panel.

It does not render:

- Blueprint Editor title bar.
- Compile results panel.
- Find results panel.
- Output log or command bar.

The graph area may use `SGraphEditor`, but the surrounding page is custom Slate UI. The Review page is read-only.

## 2. Archive Session

When MCP obtains write capability for a token/session, it creates a write session archive.

The archive baseline is captured before the first write in that session. UE writes should checkpoint transaction data before and after writes. Editor shutdown only finalizes the archive; it must not be the only time archive state is saved.

If the editor process is closed or killed before finalization, MCP/Python lifetime is the recovery owner. On the next session, unfinished archive sessions enter recover/review state instead of being discarded.

Archive data is the rollback baseline for Review-visible changes. Reject returns the reviewed change back to the archive baseline, not merely to the previous transaction delta.

## 3. Transaction Records And Compaction

Journal/Review records are not removed while they are needed for pending review, recovery, rollback, or visible diff calculation.

After a visible change is accepted, superseded transaction data may be compacted:

- Keep the accepted final record.
- Preserve enough metadata for history and audit.
- Remove or compact superseded raw diff, rollback, and review snapshot data.
- Do not expose compacted records as pending Review items.

Data compaction policy is an explicit follow-up decision. It must define retention levels for pending, accepted, rejected, rollback blocked, rollback failed, archived, and compacted records before destructive compaction is enabled by default.

## 4. Visible Change Semantics

The Review UI does not primarily show raw transactions. It shows final visible changes.

A visible change is computed from:

```text
archive baseline -> current asset state
```

For the same logical location, later transactions override earlier transactions. The UI shows only the latest visible state for that location.

Example:

```text
Archive A: PrintString("Open")
T1:        PrintString("Opening")
T2:        PrintString("Door Opened")
```

The UI shows one visible change:

```text
Open -> Door Opened
```

`T1` is a superseded source transaction. It is not shown as an independent Review item.

## 5. Accept And Reject

Review actions operate on visible changes, not individual raw transaction deltas.

Accept:

- Accepts the visible change.
- Removes the visible change from the pending overlay.
- Marks the final accepted record accepted.
- Uses settings/profile safety level to decide whether BlueprintHelper ownership is kept or converted.
- Allows superseded source transaction data to be compacted.

Reject:

- Rejects the visible change.
- Rolls the affected target back to the archive baseline state for that visible change.
- Invalidates the visible change and its superseded source transaction chain.
- If rollback cannot safely be completed, leaves the item in needs action state.

Accept and Reject buttons may appear in three places:

- Final change list on the left.
- Hover controls on a diff frame.
- Asset-level AcceptAll / RejectAll controls at the bottom center of the graph page.

Hover buttons operate on the visible change represented by that diff frame.

## 6. Diff Color Rules

Diff frames are generated from visible changes.

Colors:

- Added content: green.
- Deleted content: red.
- Variable modification: yellow.
- Function signature internal modification: yellow.
- Event signature internal modification: yellow.
- Event dispatcher signature internal modification: yellow.

Rename is represented as delete plus add.

Example:

If `OpenDoor` changes signature by adding input parameter `Input`, the new `Input` parameter row or pin is highlighted with a yellow rectangle.

## 7. Final Change List

The left operation list is a final change list for the current graph/asset view. It is not a transaction list.

Clicking an item:

- Selects the visible change.
- Navigates to the corresponding fake panel or graph element.
- Flashes the matching diff frame.

This is required for changes that are not visible in the graph area, such as variables, components, and signature rows.

## 8. Read-Only Guarantee

Components and Details should reuse Blueprint Editor panel widgets where practical, but must keep them read-only.

The Review page must not:

- Modify graph nodes directly.
- Modify UMG widgets or components directly.
- Modify variables or signatures directly.
- Save or compile assets.
- Depend on the currently focused Blueprint Editor tab for destructive operations.

All writes must go through Review action services and archive/rollback policy.

## 9. Follow-Up Decisions

- Define data compaction levels and retention policy.
- Define settings/profile rule for ownership retention after Accept.
- Define archive persistence format and recovery scan rules.
- Define item-level rollback conflict detection when a visible change spans several asset sections.
- Define exact format for transient graph snapshots and deleted ghost nodes.

## 10. Atomic Target Constraint Update - 2026-05-06

Visible changes are computed at atomic-target granularity before UI grouping.

Rules:

- A later transaction overrides only the atomic targets it touches.
- Earlier transactions remain owners of non-intersecting atomic targets.
- A final tree leaf may contain multiple atomic targets and multiple latest transactions.
- Accept / Reject still operate on the whole final tree leaf.
- Compaction after Accept must only compact source data that is no longer referenced by any pending atomic target.

Example:

```text
T1 changes N1, N2
T2 changes N2, N3

visible ownership:
N1 -> T1
N2 -> T2
N3 -> T2
```

The UI must render this as one visual group when the targets share the same visual group key, but action semantics remain whole-leaf.

## 11. Diff Rendering Constraint Update - 2026-05-06

Graph diff frames must be graph-space content, not screen-space overlays. The current implementation uses transient comment-derived diff block nodes on a cloned preview graph so the real Blueprint asset is not modified.

Components, My Blueprint, and Details diff frames must remain panel-local. If exact row geometry cannot be obtained from reused UE widgets, the first supported fallback is a stable row-frame overlay inside that panel. Graph overlays must not be used as a fallback for side-panel changes.
