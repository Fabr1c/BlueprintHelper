# BlueprintHelper User Review Panel Design

Date: 2026-05-06
Scope: Scheme B user-side Review UI for BlueprintHelper
Status: design approved for implementation planning

## 1. Goal

Add a user-facing Review tab to the existing BlueprintHelper window. The tab shows pending final asset changes in a fake Blueprint Editor page, lets the user inspect visible diffs, and supports Accept / Reject actions backed by archive sessions and Review records.

## 2. Non-Goals

This design does not embed a full editable Blueprint Editor.

It does not expose Agent-side Accept / Reject tools.

It does not implement arbitrary raw transaction browsing as the primary UI.

It does not implement final Review data compaction policy in the first UI slice. The policy is tracked as a follow-up constraint.

## 3. Window Structure

Use Scheme B:

- Add `SBlueprintHelperMainWindow`.
- Keep the existing `SHelperMainWidget` as the JSON/clipboard tool tab.
- Add a new `SBlueprintHelperReviewPanel` tab.
- Change the module spawn entry to instantiate `SBlueprintHelperMainWindow`.

This keeps Review code out of `SHelperMainWidget` and avoids touching GraphWrite or TaskRuntime code paths.

## 4. Review Page Layout

`SBlueprintHelperReviewPanel` renders a fake Blueprint Editor workspace:

```text
┌─────────────────────┬───────────────────────────────┬─────────────────────┐
│ Components           │ Graph page                    │ Details             │
│                      │                               │                     │
├─────────────────────┤                               │                     │
│ My Blueprint         │                               │                     │
│ Final change list    │                               │                     │
└─────────────────────┴───────────────────────────────┴─────────────────────┘
```

The graph page uses `SGraphEditor` for the middle preview when graph data is available. It remains read-only.

The side panels should reuse Blueprint Editor widgets when a Blueprint asset path can be resolved:

- Components: read-only `SSubobjectBlueprintEditor`.
- My Blueprint: `SMyBlueprint` bound to the reviewed Blueprint.
- Details: `SKismetInspector` with property editing disabled.

Review-specific diff frames are rendered as overlays on top of these reused panels.

The page omits:

- Title bar.
- Compile result panel.
- Find result panel.
- Output log.

## 5. UI Components

### `SBlueprintHelperReviewPanel`

Owns the Review tab layout, selected asset, selected visible change, and action callbacks.

Responsibilities:

- Load and refresh Review model.
- Render Components / My Blueprint / Graph / Details regions.
- Navigate from final change list to a visual diff frame.
- Trigger Accept / Reject / AcceptAll / RejectAll through services.

### `SBlueprintHelperReviewGraphView`

Renders the graph page.

Responsibilities:

- Build read-only `SGraphEditor` content or empty-state graph content.
- Render diff overlays over graph nodes, pins, or ghost nodes.
- Show hover Accept / Reject controls for the selected diff frame.
- Flash a diff frame after navigation.

### `SBlueprintHelperReviewTreePanel`

Renders Components and My Blueprint data.

Responsibilities:

- Show component additions, removals, and modifications.
- Show graphs, functions, variables, macros, and event dispatchers.
- Highlight visible changes using the same color rules.

### `SBlueprintHelperReviewDetailsPanel`

Renders details for the selected visible change.

Responsibilities:

- Show target path, change kind, latest source transaction, and source transaction chain.
- Show before / after summary.
- Show rollback state and needs-action reason.

## 6. Data Model

Introduce Review-facing DTOs:

```cpp
enum class EBlueprintHelperReviewChangeKind
{
    Added,
    Removed,
    Modified,
    SignatureModified,
    RenameAsDeleteAdd
};

enum class EBlueprintHelperReviewChangeStatus
{
    Pending,
    Accepted,
    Rejected,
    NeedsAction,
    Superseded
};

struct FBlueprintHelperReviewVisibleChange
{
    FString ChangeId;
    FString AssetPath;
    FString GraphName;
    FString LocationKey;
    FString LatestTransactionId;
    TArray<FString> SourceTransactionIds;
    EBlueprintHelperReviewChangeKind ChangeKind;
    EBlueprintHelperReviewChangeStatus Status;
    FString DisplayLabel;
    FString BeforeSummary;
    FString AfterSummary;
};
```

`FBlueprintHelperReviewVisibleChange` is the user-facing unit. It may be backed by one transaction or a chain of superseded transactions.

## 7. Services

### `FBlueprintHelperReviewStoreService`

Read/write Review records and build visible changes.

Responsibilities:

- Scan active Review records and archive sessions.
- Group source transactions by asset and logical location.
- Collapse multiple transactions at one location to the latest visible change.
- Mark visible changes accepted or rejected.
- Track superseded transaction chains for compaction.

### `FBlueprintHelperReviewActionService`

Executes user Review actions.

Responsibilities:

- Accept visible change.
- Reject visible change by rolling back to archive baseline.
- AcceptAll / RejectAll for the current asset.
- Return needs-action state when rollback is blocked.

### Archive Session Service

May be implemented as a new service or an extension of Journal/Review services.

Responsibilities:

- Create session archive when MCP obtains write capability.
- Persist baseline and checkpoints before/after writes.
- Finalize on editor shutdown when possible.
- Recover unfinished sessions when MCP/Python survives editor closure.

## 8. Diff Rendering

The overlay color is based on visible change kind:

- Added: green.
- Removed: red.
- Modified variable: yellow.
- Signature internal modification: yellow.
- Rename: rendered as one red removal and one green addition.

The overlay unit is the visible change frame. Hovering a frame shows Accept / Reject buttons in the lower-right corner of the frame.

The first UI slice may use coarse overlay placement per panel/selected graph block. Exact row/node/pin geometry anchoring is a follow-up, but the interaction model must already be frame-hover based.

## 9. Action Semantics

Accept:

- Operates on a visible change.
- Removes its diff frame from pending UI.
- Marks the final accepted record accepted.
- Compacts superseded source transaction data when policy allows.
- Applies ownership behavior based on setting profile.

Reject:

- Operates on a visible change.
- Rolls the target back to archive baseline for that visible change.
- Removes pending overlay if rollback succeeds.
- Leaves needs-action state if rollback is blocked or failed.

AcceptAll / RejectAll:

- Operate on the current asset's visible changes.
- Use the visible change list order, not raw transaction order.
- Stop on rollback blocked or failed.

## 10. Conflict And Recovery Rules

If a visible change has multiple source transactions, only the latest final visible change appears in the UI.

Superseded transactions are internal. They are not independently rejectable in the main UI.

If Accept succeeds, superseded data is eligible for compaction.

If Reject succeeds, the visible change chain returns to archive baseline.

If rollback cannot safely restore the baseline, the change remains in needs action with the rollback error.

## 11. Implementation Boundaries

Avoid editing:

- GraphWrite services.
- TaskRuntime lowering.
- MCP server schemas.
- Existing `SHelperMainWidget` internals unless unavoidable.

Expected minimal existing-file edits:

- `Source/BlueprintHelper/Private/BlueprintHelper.cpp`
- `Source/BlueprintHelper/Public/BlueprintHelper.h`

New files should live under:

- `Source/BlueprintHelper/Private/Widgets/Review`
- `Source/BlueprintHelper/Public/Services/Review`
- `Source/BlueprintHelper/Private/Services/Review`
- `Source/BlueprintHelper/Public/Structure/Review`
- `Source/BlueprintHelper/Private/Tests/Review`

## 12. Testing Strategy

Unit tests should cover:

- Visible change collapse from T1/T2 source transaction chain.
- Rename rendered as delete plus add.
- Added / removed / modified / signature modified color mapping.
- Accept marks latest visible change accepted and superseded data compactable.
- Reject requests archive-baseline rollback.
- Review model never exposes raw rollback data to UI by default.

UI construction should be smoke-tested by constructing the Slate panel with an empty model and a synthetic visible change model.

## 13. Follow-Up Decisions

- Finalize Review data compaction policy.
- Finalize settings/profile ownership policy after Accept.
- Persist TaskRunJournal to disk and connect it to Review grouping.
- Add richer graph deleted-node ghost rendering.
- Add branch-fork and cross-asset visible change rollback rules.

## 14. Next-Iteration Design Update - 2026-05-06

The active implementation now treats final visible changes as a tree:

```text
FinalChangeTreeSidebar | ReviewEditorPage

ReviewEditorPage:
Components | My Blueprint | GraphEditor | Details
```

Tree roots are asset paths. Leaves are merged final changes. A leaf may contain several atomic targets and several latest transactions.

Atomic target fields now include asset, surface, graph, stable target key, visual group key, node GUID, property path, component path, and optional graph bounds.

The Store first collapses by atomic target, then groups by asset plus visual group key. For example:

```text
T1 owns N1, N2
T2 owns N2, N3

final leaf:
N1 -> T1
N2 -> T2
N3 -> T2
```

Accept / Reject still operate on the whole final leaf.

Graph diff rendering now uses a transient graph clone plus `UBlueprintHelperReviewDiffBlockNode`, a `UEdGraphNode_Comment` subclass. These nodes are added only to the preview graph. They draw as textless colored blocks in graph space, below normal nodes and links, and move with pan / zoom.

Side panel diff rendering remains panel-local. Components, My Blueprint, and Details use stable diff row frames over the reused panels until exact row geometry is available.

Current implementation constraints:

- Graph bounds should be provided by Review records when possible.
- Missing graph bounds fall back to node GUID lookup.
- Missing node GUID / bounds fall back to a selected default block.
- Details selection is still object-level, not exact property handle selection.
