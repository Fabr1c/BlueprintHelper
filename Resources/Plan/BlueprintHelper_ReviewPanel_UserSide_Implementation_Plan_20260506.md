# BlueprintHelper User Review Panel Implementation Plan

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

- Added confirmed constraints document: `Resources/Plan/BlueprintHelper_ReviewPanel_UserSide_Constraints_20260506.md`.
- Added design document: `Resources/Plan/BlueprintHelper_ReviewPanel_UserSide_Design_20260506.md`.
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

- `git diff --check -- Source/BlueprintHelper/BlueprintHelper.Build.cs Source/BlueprintHelper/Private/Widgets/Review/SBlueprintHelperReviewPanel.cpp Source/BlueprintHelper/Private/Widgets/Review/SBlueprintHelperReviewPanel.h Source/BlueprintHelper/Public/Structure/Review/BlueprintHelperReviewTypes.h Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp Resources/Plan/BlueprintHelper_ReviewPanel_UserSide_Implementation_Plan_20260506.md Resources/Plan/BlueprintHelper_ReviewPanel_UserSide_Design_20260506.md Resources/Plan/BlueprintHelper_ReviewPanel_UserSide_Constraints_20260506.md` passed.
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
