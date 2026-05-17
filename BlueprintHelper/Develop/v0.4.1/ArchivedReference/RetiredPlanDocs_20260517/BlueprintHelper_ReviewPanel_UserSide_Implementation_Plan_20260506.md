# BlueprintHelper User Review Panel Implementation Plan

> 2026-05-14 状态转移：本文中的未达期待、待验证项和阻塞项已迁移到 [BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md](BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md)。本文保留为历史上下文；开放项跟踪迁移完成，后续当前状态以总账为准。

> For agentic workers: implement task-by-task. Steps use checkbox syntax for tracking.

Goal: Build the first user-side Review UI slice with Scheme B, a fake Blueprint Review page, final visible change list, read-only GraphEditor page, and service-backed Accept/Reject callbacks.

Architecture: Add Review DTOs and services first, then Slate widgets that consume the Review model. Existing `SHelperMainWidget` stays as the JSON tool tab inside a new `SBlueprintHelperMainWindow`.

Tech Stack: UE 5.6 editor module C++, Slate, `SGraphEditor`, Unreal Automation Tests.

---

### Task 1: Review Visible Change Model

Files:

- Create: `Source/BlueprintHelper/Public/Structure/Review/BlueprintHelperReviewTypes.h`
- Create: `Source/BlueprintHelper/Public/Services/Review/BlueprintHelperReviewStoreService.h`
- Create: `Source/BlueprintHelper/Private/Services/Review/BlueprintHelperReviewStoreService.cpp`
- Create: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [x] Step 1: Write failing automation tests for color mapping, rename-as-delete-add, and T1/T2 visible change collapse.
- [x] Step 2: Run the Review test command and verify it fails because Review types/services do not exist.
- [x] Step 3: Add Review DTOs and `FBlueprintHelperReviewStoreService::BuildVisibleChanges`.
- [ ] Step 4: Run the Review tests and verify they pass.

### Task 2: Review Action Service

Files:

- Create: `Source/BlueprintHelper/Public/Services/Review/BlueprintHelperReviewActionService.h`
- Create: `Source/BlueprintHelper/Private/Services/Review/BlueprintHelperReviewActionService.cpp`
- Modify: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [x] Step 1: Add failing tests for Accept marking a visible change accepted and Reject requesting archive-baseline rollback.
- [x] Step 2: Implement `FBlueprintHelperReviewActionService` with in-memory first-slice behavior and needs-action fallback for real rollback.
- [ ] Step 3: Run the Review tests and verify they pass.

### Task 3: Fake Blueprint Review Slate Widgets

Files:

- Create: `Source/BlueprintHelper/Private/Widgets/SBlueprintHelperMainWindow.h`
- Create: `Source/BlueprintHelper/Private/Widgets/SBlueprintHelperMainWindow.cpp`
- Create: `Source/BlueprintHelper/Private/Widgets/Review/SBlueprintHelperReviewPanel.h`
- Create: `Source/BlueprintHelper/Private/Widgets/Review/SBlueprintHelperReviewPanel.cpp`

- [x] Step 1: Add a widget construction automation test for empty and synthetic Review model construction.
- [x] Step 2: Implement `SBlueprintHelperMainWindow` with Tools and Review pages.
- [x] Step 3: Implement `SBlueprintHelperReviewPanel` with components, my blueprint, final change list, read-only `SGraphEditor`, details, hover action surface, and AcceptAll/RejectAll controls.
- [ ] Step 4: Run widget construction tests.

### Task 4: Module Integration

Files:

- Modify: `Source/BlueprintHelper/Public/BlueprintHelper.h`
- Modify: `Source/BlueprintHelper/Private/BlueprintHelper.cpp`

- [x] Step 1: Instantiate Review Store and Action services in the module.
- [x] Step 2: Spawn `SBlueprintHelperMainWindow` instead of `SHelperMainWidget`.
- [x] Step 3: Keep existing menu and toolbar actions unchanged.

### Task 5: Verification

Commands:

- `Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject"`
- `Automation RunTests BlueprintHelper.Review`

Expected:

- C++ build succeeds.
- Review tests pass.
- Existing BlueprintHelper window opens with Tools and Review pages.

Known limits:

- Archive session persistence and compaction policy are represented by service boundaries and state fields, not fully implemented in this UI slice.
- Real rollback is not forced for non-supported transaction types; UI shows needs-action.

## Progress Sync - 2026-05-06

Implemented:

- Added confirmed constraints document: `Develop/Plan/BlueprintHelper_ReviewPanel_UserSide_Constraints_20260506.md`.
- Added design document: `Develop/Plan/BlueprintHelper_ReviewPanel_UserSide_Design_20260506.md`.
- Added Review visible change DTOs and visible-change collapse service.
- Added Review action service for first-slice Accept / Reject semantics.
- Added Review automation tests for color mapping, rename expansion, T1/T2 collapse, Accept, Reject, and widget construction.
- Added Review surface classification tests for Components / Graph / Details / My Blueprint overlay routing.
- Added `SBlueprintHelperMainWindow` so the existing Tools page and the new Review page are separate pages.
- Added `SBlueprintHelperReviewPanel` fake Blueprint Review UI with Components, My Blueprint/final changes, read-only `SGraphEditor`, Details, hover Accept/Reject, and graph-bottom AcceptAll/RejectAll.
- Updated the fake page to reuse UE Blueprint Editor widgets where available: read-only `SSubobjectBlueprintEditor` for Components, `SMyBlueprint` for My Blueprint, and `SKismetInspector` for Details.
- Changed diff frames to rounded overlay frames that reveal Accept / Reject only on hover, and reused the same frame behavior for left-side entries, graph blocks, and details rows.
- Added module dependencies required by the reused editor panels: `PropertyEditor` and `SubobjectEditor`.
- Wired module startup/shutdown and tab spawn to use the new main window shell.
- Fixed UE 5.6 include path for `SOverlay.h`: use `Widgets/SOverlay.h`.
- Fixed `SAssignNew(ChangeListView, ...)` compile issue by making `BuildMyBlueprintPanel()` non-const.
- Fixed Slate delegate const-binding compile issue by making `BuildSelectedDiffOverlay()` and `BuildActionButtonBar()` non-const.

Verification status:

- `git diff --check -- Source/BlueprintHelper/BlueprintHelper.Build.cs Source/BlueprintHelper/Private/Widgets/Review/SBlueprintHelperReviewPanel.cpp Source/BlueprintHelper/Private/Widgets/Review/SBlueprintHelperReviewPanel.h Source/BlueprintHelper/Public/Structure/Review/BlueprintHelperReviewTypes.h Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp Develop/Plan/BlueprintHelper_ReviewPanel_UserSide_Implementation_Plan_20260506.md Develop/Plan/BlueprintHelper_ReviewPanel_UserSide_Design_20260506.md Develop/Plan/BlueprintHelper_ReviewPanel_UserSide_Constraints_20260506.md` passed.
- Local Codex environment has `G:\UE_5.3`, but the project declares `EngineAssociation` 5.6 and `G:\UE_5.6\Engine\Build\BatchFiles\Build.bat` is not present, so the UE 5.6 project build was not run here.
- Earlier project `Build.bat` attempts could not complete inside Codex because `G:\UnrealPractise\MrStone\Intermediate` files were permission-locked. User-side compile progressed far enough to report source compile issues, and the reported issues have been addressed.
- Review automation tests are written but not yet confirmed passing because the full UE build/test run has not completed in this environment.

Deferred:

- Archive session persistence across MCP/Python/editor lifecycle.
- Real archive-baseline rollback backend.
- Review data compaction policy and destructive compaction implementation.
- Setting/profile rule for ownership removal after Accept.
- Real deleted-node ghost rendering inside GraphEditor overlays.
- True per-node/per-pin overlay placement in `SGraphEditor`; current first slice uses a selected diff frame overlay.
- Exact per-row geometry binding for reused Components / My Blueprint / Details panels; current first slice overlays routed diff frames over the reused panels.
- Hard-blocking every possible context-menu mutation path inside reused UE widgets if a widget exposes one without an active `FBlueprintEditor`.
- Persisted TaskRunJournal to Review grouping.

Known omissions:

- Components, My Blueprint, and Details panels reuse real UE widgets only when a visible change has a resolvable Blueprint asset path; otherwise they fall back to Review placeholders or final diff rows.
- Details panel uses a read-only `SKismetInspector` for the selected Blueprint/graph object plus a diff overlay; it does not yet select exact variable/signature property handles from structured snapshots.
- `AcceptAll` currently clears the in-memory visible list in the UI slice; it does not yet write final accepted records or compact superseded source transactions.
- `RejectAll` marks items needs-action until archive-baseline rollback is wired.
- Review Store loads coarse records from Saved Review JSON and collapses by derived `LocationKey`; exact location keys require future archive/diff snapshot schema.

## Progress Sync - 2026-05-06 Next Iteration

Implemented:

- Added atomic Review targets and surface routing (`Graph`, `Components`, `MyBlueprint`, `Details`) so final leaves can contain multiple atom targets.
- Updated Store collapse to resolve transaction intersections per atomic target: later transactions own intersecting atoms, non-intersecting atoms keep their original transaction ownership.
- Added tests for T1 N1/N2 plus T2 N2/N3 atomic collapse, multi-surface leaves, mixed-latest Accept compaction behavior, and empty Review panel construction.
- Split the UI layout into `FinalChangeTreeSidebar | ReviewEditorPage`.
- Added asset-root tree rendering in the final change sidebar, with leaf rows as the user operation unit.
- Kept the fake Blueprint page as Components / My Blueprint / Graph / Details.
- Added panel-local diff stacks for Components, My Blueprint, and Details.
- Fixed active Chinese panel titles for Components, My Blueprint, and Details.
- Changed Graph diff rendering from a centered screen overlay to transient graph-space diff blocks.
- Added `UBlueprintHelperReviewDiffBlockNode`, a transient `UEdGraphNode_Comment` subclass that renders under graph nodes and links without modifying the real Blueprint graph.
- GraphEditor now clones the source graph into a transient preview graph before adding Review diff blocks.
- Graph diff blocks expose hover Accept / Reject controls and jump-to-selected behavior.
- AcceptAll / RejectAll now operate on the current selected asset instead of blindly clearing all visible changes.

Verification status:

- Static active-code scan found one active definition each for `BuildComponentsPanel`, `BuildMyBlueprintPanel`, `BuildDetailsPanel`, and `BuildGraphEditorWidget` after excluding disabled legacy blocks.
- Whitespace scan over touched Review source/header/test files found no trailing whitespace.
- `git diff --check` over tracked touched Review paths passed; new Review files are untracked in this workspace, so they were checked separately with the whitespace scan.
- UE build attempted with `G:\UE_5.3\Engine\Build\BatchFiles\Build.bat`, but it stopped before compiling plugin code because UBT could not write `G:\UnrealPractise\MrStone\Intermediate\Build\BuildRules\MrStoneModuleRules.dll` (`UnauthorizedAccessException`). The project still declares `EngineAssociation` 5.6, and no local `G:\UE_5.6` build path is available in this environment.

Deferred:

- Real archive-baseline rollback backend for Reject / RejectAll.
- Persistent Review record writes for Accept / Reject.
- Destructive data compaction after Accept; current action service only reports compaction eligibility.
- Exact row geometry binding inside reused UE Components / My Blueprint / Details widgets; current side panels use stable panel-local diff rows.
- Deleted-node ghost rendering and pin-level ghost nodes in GraphEditor.
- Removing disabled legacy UI blocks from `SBlueprintHelperReviewPanel.cpp`; they are excluded from active compilation but should be cleaned when the next compile pass is green.

Known omissions:

- Graph bounds are best when Review records provide `GraphPosition` / `GraphSize`; otherwise the UI falls back to node GUID lookup or a selected default block.
- Graph diff block hover buttons live inside the transient preview graph and still rely on the first-slice in-memory action service.
- `SSubobjectBlueprintEditor`, `SMyBlueprint`, and `SKismetInspector` are reused read-only at the Slate level, but deeper context-menu hardening remains a follow-up.

## Progress Sync - 2026-05-06 Crash Fix

Implemented:

- Fixed the `FBlueprintEditorUtils::FindBlueprintForNodeChecked(/Engine/Transient.EdGraph_18:K2Node_CallFunction_1)` crash path.
- The Review graph preview now creates a transient `UBlueprint` owner before cloning graph data, so K2 nodes in the preview graph can resolve an owning Blueprint.
- The earlier full Blueprint duplication approach is superseded by the Transient Blueprint Collision Fix below.
- Keep a fallback path that clones only the selected graph, but the clone now uses the transient preview Blueprint as its outer instead of `GetTransientPackage()`.
- Empty graph placeholders are also created under the transient preview Blueprint.
- Removed unreachable old code in `SBlueprintHelperReviewPanel.cpp`: disabled `#if 0` blocks, old final-change `SListView` references, old selected screen-space Graph overlay, and old selected Details overlay.
- Replaced remaining active mojibake action labels in the Review action bar with ASCII `AcceptAll` / `RejectAll`.

Verification status:

- `git diff --check -- Source/BlueprintHelper/Private/Widgets/Review/SBlueprintHelperReviewPanel.cpp Source/BlueprintHelper/Private/Widgets/Review/SBlueprintHelperReviewPanel.h` passed.
- Trailing whitespace scan over `SBlueprintHelperReviewPanel.cpp/.h` passed.
- Static scan found no active `ChangeListView`, `BuildSelectedDiffOverlay`, `BuildSelectedDetailsDiffOverlay`, `#if 0`, `#endif`, `SListView`, or `GenerateChangeRow` references.
- Static scan found no known mojibake tokens in active `SBlueprintHelperReviewPanel.cpp/.h`.
- UE build was retried with local `G:\UE_5.3\Engine\Build\BatchFiles\Build.bat`, but UBT still stopped before compiling plugin code because it cannot write `G:\UnrealPractise\MrStone\Intermediate\Build\BuildRules\MrStoneModuleRules.dll`.

Deferred:

- Full UE compile verification and Review automation run remain blocked in this environment by the `Intermediate\BuildRules` permission issue.

## Progress Sync - 2026-05-06 Transient Blueprint Collision Fix

Implemented:

- Replaced the full transient Blueprint duplication path from the previous crash fix.
- Review graph preview now creates a uniquely named lightweight transient `UBlueprint` shell with copied metadata pointers only.
- The selected graph is cloned under that preview Blueprint shell, so K2 nodes still resolve an owning Blueprint for editor utilities.
- Preview graph membership is attached to the matching Blueprint graph list when the source graph is an ubergraph, function, macro, or delegate signature graph.
- The Review preview no longer duplicates `GeneratedClass`, `SkeletonGeneratedClass`, or class default objects into `/Engine/Transient`.

Superseded:

- The earlier note "Prefer duplicating the reviewed Blueprint into the transient package" is no longer valid. It caused `/Engine/Transient.BP_*_C` and `Default__BP_*_C` name collisions when Unreal still had REINST classes alive.

Verification status:

- Static scan confirms `SBlueprintHelperReviewPanel.cpp` no longer contains `DuplicateObject<UBlueprint>`.
- Full UE compile verification and editor smoke testing remain blocked in this environment by the `Intermediate\BuildRules` permission issue until the user compiles locally or clears the file lock.

Deferred:

- Confirm the Review widget can open repeatedly in the live UE 5.6 editor after local rebuild.

## Progress Sync - 2026-05-06 Side Panel And Graph Bounds Follow-up

Implemented:

- My Blueprint panel no longer treats Graph-only visible changes as My Blueprint diff rows.
- Empty side-panel diff stacks now return `SNullWidget`, so an empty overlay does not cover or intercept the reused UE panel.
- Right-side column now renders Details above a new Debug panel.
- Former bottom DebugMessage/status text now writes into the Debug panel instead of a bottom bar.
- Graph diff bounds are tighter and now prefer real node/block matches over stored graph bounds.
- Review Store graph records derive graph atomic targets from `created_nodes`, `blocks`, and rollback `node_guids`, giving the Review panel better node/block anchors for underlay bounds.
- Graph node matching now accepts node GUIDs, object names from `created_nodes`, and BlueprintHelper block ids found in node comments.

Verification status:

- `git diff --check` over the touched Review widget/test paths passed.
- Trailing whitespace scan over the touched Review widget/test paths passed.
- Static scan found no active `DuplicateObject<UBlueprint>`, `ChangeListView`, disabled legacy `#if 0`, or old selected overlay builders in the touched Review widget paths.
- UE build verification is still blocked by `G:\UnrealPractise\MrStone\Intermediate\Build\BuildRules\MrStoneModuleRules.dll` write permission before plugin compilation starts.

Deferred:

- Live editor screenshot validation after the user rebuilds with the local UE 5.6 environment.
- Exact row-geometry frames inside reused My Blueprint / Details widgets remain future work; this pass prevents empty or graph-only overlays from covering those panels.

## Progress Sync - 2026-05-06 Review Store Compile Fix

Implemented:

- Fixed `FJsonObject::TryGetObjectField("rollback_data", ...)` usage in `BlueprintHelperReviewStoreService.cpp`.
- The object-form `rollback_data` branch now uses UE's required `const TSharedPtr<FJsonObject>*` out pointer and copies the pointed shared pointer into the local rollback object.

Verification status:

- Static scan confirms the Review Store no longer passes `TSharedPtr<FJsonObject>` or raw `FJsonObject*` directly to `TryGetObjectField`.
- `git diff --check` over the touched Review Store and Review plan files passed.
- UE build verification remains blocked in this environment by `G:\UnrealPractise\MrStone\Intermediate\Build\BuildRules\MrStoneModuleRules.dll` write permission before plugin compilation starts.

Deferred:

- Local UE 5.6 compile confirmation after the user rebuilds.

## Progress Sync - 2026-05-07 Side Panel Row Frame Preview

Implemented:

- Replaced Components / My Blueprint / Details side-panel diff overlays with `SCanvas` row-frame previews.
- Side-panel frames are now empty bordered rectangles with no diff text, so the reused UE panels remain visible underneath.
- Hovering a side-panel frame still exposes Accept / Reject controls.
- Components frame preview anchors component rows by stable component target text.
- My Blueprint frame preview anchors function, component, variable/property, macro, and dispatcher buckets by stable target text and row order.
- Details frame preview uses the same empty-frame rendering path.

Verification status:

- `git diff --check` over the touched Review panel and plan files passed.
- Trailing whitespace scan over the touched Review panel and plan files passed.
- Static scan found no active `DuplicateObject<UBlueprint>`, `ChangeListView`, disabled legacy `#if 0`, or old selected overlay builders in the touched Review widget paths.
- UE build was retried with local `G:\UE_5.3\Engine\Build\BatchFiles\Build.bat`, but UBT still stops before plugin compilation because it cannot write `G:\UnrealPractise\MrStone\Intermediate\Build\BuildRules\MrStoneModuleRules.dll`.

Deferred:

- Exact UE row geometry binding remains deferred; this is the first path-indexed stable-row approximation for the figure-2 preview shape.
- Pixel-perfect tuning of the hard-coded row offsets should be done against the next live editor screenshot.

## Progress Sync - 2026-05-07 Graph Diff Comment-Style Bounds

Implemented:

- Added `BlueprintHelperReviewGraphBounds` helpers for graph target matching and comment-style diff bounds.
- Graph diff bounds now prefer transient preview graph nodes, not source graph nodes.
- Bounds matching now accepts `NodeGuid`, `TargetKey`, `PinPath`, `VisualGroupKey`, and display-label candidates, so records such as `graph:EventGraph/node:K2Node_CallFunction_1` can resolve to the actual graph node.
- `SGraphEditor::GetBoundsForNode` is used when available, matching the same widget-bounds source used by UE's selected-node comment sizing path.
- Fallback sizing still uses `NodePosX/Y`, `NodeWidth/NodeHeight`, and `UEdGraphSchema_K2::EstimateNodeHeight` when graph-editor node widgets are not available yet.
- Diff block insertion now happens after the transient `SGraphEditor` is constructed, followed by `NotifyGraphChanged()`, so bounds can be computed from graph-editor node widgets without touching the real Blueprint asset.
- Removed old unreachable panel-local graph matching helpers from `SBlueprintHelperReviewPanel`.
- Added an automation test for TargetKey-based graph bounds with 50px comment-style padding.

Verification status:

- `git diff --check` over tracked touched Review panel/test/plan files passed.
- Trailing whitespace scan over the new `BlueprintHelperReviewGraphBounds.cpp/.h` files passed.
- `Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReloadFromIDE` did not reach C++ compilation because UBT cannot write/rename files under `G:\UnrealPractise\MrStone\Intermediate`, ending on `SourceFileCache.bin` access denied.

Deferred:

- Full `Automation RunTests BlueprintHelper.Review` confirmation is pending until the project `Intermediate` write lock/permission issue is cleared.
- The Review UI still does not call UE's real Create Comment action because that action creates a real comment node and marks the Blueprint structurally modified.
- Exact graph bounds can still fall back to stored node dimensions if `SGraphEditor::GetBoundsForNode` has not built widgets on the first pass.

Omitted:

- No real Blueprint asset mutation was added.
- No MCP asset operation was used for this UI-only C++ change.

## Progress Sync - 2026-05-07 Graph Bounds Test Compile Fix

Implemented:

- Fixed UE 5.6 automation-test compile ambiguity in `FBlueprintHelperReviewGraphBoundsTargetKeyTest`.
- Replaced numeric `TestEqual` calls on `FVector2D` values with `TestTrue + FMath::IsNearlyEqual` and explicit `float` casts.

Verification status:

- `git diff --check -- Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp` passed.
- `Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReloadFromIDE` was retried, but UBT still stopped before plugin C++ compilation because it cannot write project and sibling-plugin `Intermediate` files such as `Module.MrStone.9.cpp`, `LiveCodingInfo.json`, and response files.

Deferred:

- Full UE compile and Review automation confirmation remain dependent on the user-side compile run because the Codex environment is still blocked by project `Intermediate` write permissions.

Omitted:

- No production code behavior changed in this compile fix.

## Progress Sync - 2026-05-07 Graph Diff Delayed Resize And Asset Sidebar Actions

Implemented:

- Fixed the Graph Diff resize timing issue by adding a short active-timer refresh after the transient `SGraphEditor` is constructed.
- The Review graph now creates an initial fallback diff block, then rebuilds graph diff blocks for three active-timer ticks so `SGraphEditor::GetBoundsForNode` can use node widget bounds after Slate layout.
- Rebuild removes only transient `UBlueprintHelperReviewDiffBlockNode` nodes from the preview graph, then re-adds diff blocks from current visible changes. Real Blueprint assets remain untouched.
- Restored graph diff padding to UE Comment-style `50.0f`, matching `FEdGraphSchemaAction_K2AddComment` and the graph bounds automation test.
- Added `AcceptAllAssetChange` and `RejectAllAssetChange` buttons to the bottom of the left Final Change sidebar.
- These new sidebar buttons reuse existing asset-layer `OnAcceptAll` / `OnRejectAll` behavior, so they apply to the selected asset's pending visible changes.

Verification status:

- `git diff --check -- Source/BlueprintHelper/Private/Widgets/Review/SBlueprintHelperReviewPanel.cpp Source/BlueprintHelper/Public/Widgets/Review/SBlueprintHelperReviewPanel.h Source/BlueprintHelper/Private/Widgets/Review/BlueprintHelperReviewGraphBounds.cpp Develop/Plan/BlueprintHelper_ReviewPanel_UserSide_Implementation_Plan_20260506.md` passed.
- Trailing whitespace scan over the touched Review panel, graph-bounds helper, and plan files passed.
- Static scan confirms `AcceptAllAssetChange`, `RejectAllAssetChange`, `TickGraphDiffBoundsRefresh`, and `CommentStylePadding = 50.0f` are present.
- `Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReloadFromIDE` was retried, but UBT still stopped before plugin C++ compilation because it cannot write/rename project and sibling-plugin `Intermediate` files, ending on `G:\UnrealPractise\MrStone\Intermediate\Build\SourceFileCache.bin` access denied.

Deferred:

- Full UE compile and Review automation confirmation still depend on clearing the project/sibling-plugin `Intermediate` write permission issue in this Codex environment.

Omitted:

- No real Blueprint asset mutation or MCP asset operation was added.

## Progress Sync - 2026-05-07 Graph Block Target Normalization

Implemented:

- Re-analyzed the live DebugMessage log for `tx_1777905076009`. The selected change now resolves the correct source graph, but only the custom event node matched because the Review target block came from the journal as a short block ref while current node metadata uses full block id.
- Checked the Append design rule: full block id is `graph_id + "_" + block_ref`.
- Removed the temporary broad suffix-matching direction for `BlueprintHelperBlockId`; GraphBounds keeps exact metadata matching.
- Added `FBlueprintHelperReviewStoreService::NormalizeGraphBlockTargetId`.
- Review Store now normalizes `blocks` journal entries when building graph atomic targets. A short block ref such as `BH_TaskSpecSmokeEvent_20260504_0010` becomes `BH_TaskSpecSmoke_20260504_001_BH_TaskSpecSmokeEvent_20260504_0010`; an already-full block id is preserved.
- Added tests for block target normalization and full-block-id metadata bounds.

Verification status:

- `git diff --check` over the touched Review Store, GraphBounds, Review tests, and plan files passed.
- Trailing whitespace scan over the same files passed.
- `Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReloadFromIDE` was retried, but UBT still stopped before plugin C++ compilation because it cannot rename/write project and sibling-plugin `Intermediate` files, ending on `G:\UnrealPractise\MrStone\Intermediate\Build\SourceFileCache.bin` access denied.

Deferred:

- User-side build/run confirmation is needed for the new Store normalization.
- If `tx_1777905076009` still only matches one node after this change, the next DebugMessage should include the generated full block target id and current node block metadata for direct comparison.

Omitted:

- No real Blueprint asset mutation or MCP asset operation was added.

## Progress Sync - 2026-05-07 SourceGraph Metadata Bounds

Implemented:

- Analyzed the copied DebugMessage log. The evidence shows `editorBounds=0` on every successful graph bound and successful rectangles are all from `fallbackBounds`.
- Confirmed the first successful selected change starts from `previewNodes=4`, so the small `pos=(284,-212) size=(260,136)` rectangle is not caused by later DiffBlock nodes polluting the graph.
- Added a graph-bounds automation test for `BlueprintHelperBlockId` metadata: one block target should wrap multiple nodes that share the same block id.
- `BlueprintHelperReviewGraphBounds` now matches graph targets against node metadata keys `BlueprintHelperBlockId`, `BlueprintHelperTransactionId`, and `BlueprintHelperFeatureName`.
- Bounds matching now de-duplicates repeated node matches and reports `duplicateMatches`.
- Debug summary now includes `matched="NodeName[source]@(x,y,w,h)"` details so the next live run can show exactly which nodes were included in each Diff rectangle.
- Review graph bounds now use the read-only source graph for fallback matching when available, while still drawing transient DiffBlock nodes into the preview graph. This keeps metadata available without mutating assets.
- Debug messages now include `boundsGraph="..."` to confirm whether bounds came from the source graph or preview graph.

Verification status:

- `git diff --check` over the touched graph-bounds, Review panel, and Review tests passed.
- Trailing whitespace scan over the same files passed.
- `Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReloadFromIDE` was retried in Codex, but UBT still stopped before plugin C++ compilation because it cannot rename/write project and sibling-plugin `Intermediate` files, ending on `G:\UnrealPractise\MrStone\Intermediate\Build\SourceFileCache.bin` access denied.
- User-side build passed for this metadata-bounds change.
- User-side DebugMessage confirms the source-graph fallback path is active. Example: `tx_1778068978617` on `BH_TaskSpecSmoke_20260504_001` now matches `K2Node_CustomEvent_0`, `K2Node_CallFunction_1`, and `K2Node_CallFunction_2`, producing `size=(1348.0,262.0)`.
- User screenshot confirms at least one Graph Diff frame now resizes to the expected block-level rectangle and wraps three nodes.

Deferred:

- The remaining `editorBounds=0` timing issue is intentionally not fixed by timer in this pass.
- Some Append journal records only contain `blocks` plus two `created_nodes`; Review bounds can only wrap the currently matchable atomic targets. Later transaction overlays still need their own leaf or a future atomic-chain compaction/merge rule.
- `BH_TaskSpecSmoke_20260505_001` currently resolves to `EventGraph` in the Review UI because the requested graph is not found in the loaded Blueprint. The next UI fix should avoid falling back to `EventGraph` for a selected graph-specific Review change and instead render an empty/missing-graph Review graph with a clear DebugMessage.

Omitted:

- No real Blueprint asset mutation or MCP asset operation was added.

## Progress Sync - 2026-05-07 Remove Graph Diff Timer And Add Debug List

Implemented:

- Removed the Graph Diff active-timer resize path from `SBlueprintHelperReviewPanel`.
- Removed the timer-driven transient DiffBlock delete/rebuild flow so mouse-up handling no longer races with timer-created graph node churn.
- Kept Graph Diff creation as a single initial pass when the Review `SGraphEditor` is built.
- Converted the right-side Debug area from a single status text into a list-style DebugMessage panel.
- Added DebugMessage entries for selected change, graph editor build, graph bounds result, fallback rectangle use, transient DiffBlock creation, jump result, and Accept/Reject operations.
- Extended `BlueprintHelperReviewGraphBounds::BuildBoundsForTargets` with an optional debug summary containing target counts, skipped surface/graph counts, candidate count, matched node count, editor-widget bounds count, fallback-node bounds count, record-bounds count, padding, final position, final size, graph name, and graph node count.
- Updated the graph-bounds automation test to assert the current `20.0f` padding and to verify debug summary fields.

Verification status:

- `git diff --check` over the touched Review panel, graph-bounds helper, graph-bounds test, and plan files passed.
- Trailing whitespace scan over the same files passed.
- Static scan confirms the Graph Diff bounds active-timer symbols and timer-driven rebuild/remove helpers are no longer present; only the existing flash timer remains.
- `Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReloadFromIDE` was retried, but UBT still stopped before plugin C++ compilation because it cannot rename/write project and sibling-plugin `Intermediate` files, ending on `G:\UnrealPractise\MrStone\Intermediate\Build\SourceFileCache.bin` access denied.

Deferred:

- No final crash fix beyond timer removal is claimed yet. The next decision should use the DebugMessage output from a live editor run.
- If the crash persists without the timer, the next likely investigation point is whether `UEdGraphNode_Comment` still routes through `SGraphNodeComment` interaction paths despite the custom visual widget.
- A deterministic Slate mouse-up crash automation test remains deferred because the issue depends on live GraphEditor interaction.

Omitted:

- No real Blueprint asset mutation or MCP asset operation was added.
- No new graph refresh/upsert strategy was introduced in this pass.

## Progress Sync - 2026-05-07 Debug Copy And Bounds Log Analysis

Implemented:

- Replaced the Debug panel's non-copyable list rows with a read-only `SMultiLineEditableTextBox`.
- Added `CopyAll` in the Debug panel header; it copies the current DebugMessage text to the system clipboard.
- Added `BlueprintHelperReviewDebugText::BuildCopyableText` so DebugMessage export order and line breaks are testable outside Slate.
- Added an automation test for copyable DebugMessage text ordering and line breaks.

Debug log analysis:

- Current live logs show `editorBounds=0` for all successful Graph Diff bounds; the UI is not receiving real `SGraphEditor::GetBoundsForNode` geometry in the current one-pass build.
- Successful bounds are coming from `fallbackBounds`, which explains why several Diff boxes share `pos=(284,-212)` and `size=(260,136)`.
- `skippedGraph=1` or `skippedGraph=3` means those visible changes belong to a different graph than the current Review graph, so skipping them in the current GraphEditor is expected.
- `candidates=0` means the Review record does not provide a node guid, target key, pin path, visual group key, or display label usable for graph-node matching; those changes need stored graph bounds or better target recording.
- `graphNodeCount` increases during DiffBlock insertion because transient DiffBlock nodes are added to the preview graph. If the mouse-up crash persists without the removed timer, the next investigation point is still the comment-derived DiffBlock node path.

Verification status:

- `git diff --check` over the touched Review panel, DebugText helper/test, and plan files passed.
- Trailing whitespace scan over the same files passed.
- Static scan confirms the old non-copyable Debug `SListView` symbols and removed Graph Diff timer/rebuild symbols are absent.
- `Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReloadFromIDE` was retried, but UBT still stopped before plugin C++ compilation because it cannot rename/write project and sibling-plugin `Intermediate` files, ending on `G:\UnrealPractise\MrStone\Intermediate\Build\SourceFileCache.bin` access denied.
- User-side build has now passed after this Debug copy change, so the copied-text UI path is compile-validated outside the Codex sandbox.

Deferred:

- No graph bounds fix was added in this pass; this pass only makes Debug text copyable and records the current log interpretation.

Omitted:

- No real Blueprint asset mutation or MCP asset operation was added.

## Progress Sync - 2026-05-07 Missing Requested Graph Fallback Fix

Implemented:

- Analyzed the copied live DebugMessage log. The multi-node Diff rectangle path is now validated for existing graphs: `tx_1777992254751` on `BH_Smoke_Rerun_20260505` produced `size=(1300.0,263.0)` and the user screenshot confirms the frame wraps three nodes.
- Identified a separate Review UI fallback issue: selecting `tx_1777914068182` requested `BH_TaskSpecSmoke_20260505_001`, but the loaded Blueprint did not contain that graph, so the panel fell back to `EventGraph`.
- Added `BlueprintHelperReviewGraphResolver`. Explicit requested graph names now only resolve to that exact graph; missing requested graphs return null instead of falling back to `EventGraph`.
- `SBlueprintHelperReviewPanel::ResolveGraphForSelectedChange` now uses the resolver.
- Added a DebugMessage when a selected graph-specific Review change cannot resolve its source graph, so the panel reports `sourceGraph="<none>"` instead of silently showing the wrong graph.
- Added an automation test for the resolver rule: missing explicit graph does not fall back, while an empty requested graph can still use the default graph.

Verification status:

- `git diff --check` over the touched Review panel, graph resolver, Review tests, and plan files passed.
- Trailing whitespace scan over the touched Review panel, graph resolver, Review tests, and plan files passed.
- `Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReloadFromIDE` was retried, but UBT still stopped before plugin C++ compilation because it cannot rename/write project and sibling-plugin `Intermediate` files, ending on `G:\UnrealPractise\MrStone\Intermediate\Build\SourceFileCache.bin` access denied.

Deferred:

- Live run confirmation is still needed for the new graph resolver behavior. User-side compile is confirmed in the 2026-05-09 sync below.
- `tx_1777905076009` can still produce a smaller frame when older journal `created_nodes` no longer exist in the current graph after later transactions. That is an atomic-chain/current-node attribution issue, not the missing-graph fallback issue fixed here.
- `editorBounds=0` remains intentionally deferred while timer-based resizing is removed.

## Progress Sync - 2026-05-09 User Compile Confirmation

Status update:

- User-side compile has passed, so the new graph resolver files are now compile-confirmed outside the Codex sandbox.
- The previous compile blocker for `BlueprintHelperReviewGraphResolver` is closed.
- No new Codex-side UE compile was run in this sync.

Still open:

- Live ReviewPanel run confirmation is still useful for graph selection and current-node attribution behavior.
- `tx_1777905076009` can still produce a smaller frame when older journal `created_nodes` no longer exist in the current graph after later transactions.
- `editorBounds=0` remains intentionally deferred while timer-based resizing is removed.

Omitted:

- No real Blueprint asset mutation or MCP asset operation was added.
