# BlueprintHelper Review v2 Component Lifecycle Reject Cascade Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复组件父级 Reject 后实际 SCS 子组件残留、子 ReviewEvent 残留的问题，并保证分页 PendingLoad 后仍能正确级联。

**Architecture:** 以 Review v2 数据模型为唯一基线，把组件/Widget lifecycle 父子关系固化到 Review model / pending index 可保留的紧凑元数据里；Reject action service 基于完整 lifecycle 树执行 deepest-first restore，再做 per-target ReviewRecord pruning。UI 只转发用户操作，不推断组件层级，也不使用 timer/delay 修复刷新。

**Tech Stack:** Unreal Engine 5.6 C++、BlueprintHelper Review v2、SimpleConstructionScript、Automation Tests、BlueprintHelper CLI/Bridge。

---

## Evidence Summary

- MCP 已启动编辑器并确认 Bridge 可用。
- CLI 回读显示 `CH_RootA` 和 `CH_B_ChildScene` 已消失，但其子组件仍存在并被挂到 `DefaultSceneRoot` 下。
- ReviewRecord 仍有 `CH_A_GrandChildScene` 与 `CH_B_LeafMesh` 两个 pending component ReviewEvent。
- `PendingReviewIndex` 当前剥掉 snapshot，而 `LinkPendingChildrenToLifecycleRoots` 的 component parent link 依赖 snapshot 中的 `parent_component`。
- `CascadeRejectLifecycleChildrenAfterRootResult` 只按 `ParentChangeId` 级联；当前 pending/page model 没有该字段。
- `RestoreComponentFromSnapshot` 对 `exists=false` 直接 `SimpleConstructionScript->RemoveNode(Node)`，未处理子节点，UE 会保留/reparent 子 SCS nodes。

## File Structure

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Shared/Review/BlueprintHelperReviewTypes.h`
  - 为 `FBlueprintHelperReviewAtomicTarget` 增加 compact lifecycle metadata 字段。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewStoreJsonUtils.cpp`
  - 序列化/反序列化 compact lifecycle metadata。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewStoreTargetUtils.h`
  - 暴露 lifecycle metadata 派生与链接 API。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewStoreTargetUtils.cpp`
  - 从 target/snapshot/anchor 派生 lifecycle object/parent key，并让 pending index strip snapshot 后仍能链接。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewRejectService.cpp`
  - 改为基于完整 lifecycle graph，component/widget root 先 deepest-first reject descendants，再 reject root；不再整记录删除 child records。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp`
  - 为 lifecycle root reject 提供完整 pending changes 输入，避免 UI 当前页成为级联边界。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.cpp`
  - 对 component exists=false restore 增加 children guard，防止漏级联时 UE reparent 子节点。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPendingIndexTests.cpp`
  - 添加跨记录 component parent link 回归。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`
  - 添加 Reject component lifecycle root 的 deepest-first / mixed-record 回归。
- Update: `Debug/BlueprintHelper_ComponentHierarchyReviewEvents_20260601.md`
  - 记录修复后验证结果。

---

### Task 1: Add Failing Pending Index Parent-Link Test

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPendingIndexTests.cpp`

- [ ] **Step 1: Add a helper that creates a component change with parent snapshot data**

Add this helper near `MakeComponentRootChange`:

```cpp
static FBlueprintHelperReviewVisibleChange MakeComponentRootChangeWithParent(
	const FString& ChangeId,
	const FString& AssetPath,
	const FString& ComponentName,
	const FString& ParentComponentName)
{
	FBlueprintHelperReviewVisibleChange Change =
		MakeComponentRootChange(ChangeId, AssetPath, ComponentName);
	const FString AfterSnapshot = FString::Printf(
		TEXT("{\"schema\":\"BlueprintHelper.ReviewTargetSnapshot.v2\",\"target_kind\":\"component\",\"target_key\":\"component:%s\",\"exists\":true,\"name\":\"%s\",\"parent_component\":\"%s\"}"),
		*ComponentName,
		*ComponentName,
		*ParentComponentName);
	Change.AfterSnapshotJson = AfterSnapshot;
	Change.AtomicTargets[0].AfterSnapshotJson = AfterSnapshot;
	Change.AtomicTargets[0].AnchorJson = FString::Printf(
		TEXT("{\"component_name\":\"%s\",\"parent_component\":\"%s\"}"),
		*ComponentName,
		*ParentComponentName);
	return Change;
}
```

- [ ] **Step 2: Add the failing regression**

Add:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingIndexPageLinksCrossRecordComponentParentAfterSnapshotStripTest,
	"BlueprintHelper.Review.PendingIndex.PageLinksCrossRecordComponentParentAfterSnapshotStrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingIndexPageLinksCrossRecordComponentParentAfterSnapshotStripTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString AssetPath = BlueprintHelperReviewPendingIndexTests::MakeUniqueAssetPath(TEXT("BP_PendingIndexComponentParent"));

	FBlueprintHelperReviewRecord ParentRecord = BlueprintHelperReviewPendingIndexTests::MakeRecordWithChange(
		BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_parent_component")),
		AssetPath,
		TEXT("task_pending_index_parent_component"),
		BlueprintHelperReviewPendingIndexTests::MakeComponentRootChange(
			TEXT("tx_pending_index_parent_component"),
			AssetPath,
			TEXT("ParentComp")));

	FBlueprintHelperReviewRecord ChildRecord = BlueprintHelperReviewPendingIndexTests::MakeRecordWithChange(
		BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_child_component")),
		AssetPath,
		TEXT("task_pending_index_child_component"),
		BlueprintHelperReviewPendingIndexTests::MakeComponentRootChangeWithParent(
			TEXT("tx_pending_index_child_component"),
			AssetPath,
			TEXT("ChildComp"),
			TEXT("ParentComp")));

	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(ParentRecord.ReviewRecordId);
	BlueprintHelperReviewPendingIndexTests::DeleteRecordFile(ChildRecord.ReviewRecordId);

	FString SaveError;
	TestTrue(TEXT("parent record saves"), Store.SaveReviewRecord(ParentRecord, SaveError));
	TestTrue(TEXT("child record saves"), Store.SaveReviewRecord(ChildRecord, SaveError));

	FBlueprintHelperReviewPendingIndexQuery Query;
	Query.AssetPathFilter = AssetPath;
	Query.bSkipMissingAssetRecords = false;

	FBlueprintHelperReviewPendingIndexPageRequest Request;
	Request.Query = Query;
	Request.PageSize = 10;

	FBlueprintHelperReviewPendingIndexPage Page;
	FString Error;
	FBlueprintHelperReviewPendingIndexService IndexService;
	TestTrue(TEXT("page query succeeds"), IndexService.QueryPendingVisibleChangePage(Request, Page, Error));
	TestEqual(TEXT("page includes parent and child"), Page.Changes.Num(), 2);
	if (Page.Changes.Num() == 2)
	{
		const FBlueprintHelperReviewPendingVisibleChangeSummary* ParentSummary = Page.Changes.FindByPredicate(
			[](const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary)
			{
				return Summary.Change.DisplayLabel == TEXT("ParentComp");
			});
		const FBlueprintHelperReviewPendingVisibleChangeSummary* ChildSummary = Page.Changes.FindByPredicate(
			[](const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary)
			{
				return Summary.Change.DisplayLabel == TEXT("ChildComp");
			});
		const FBlueprintHelperReviewVisibleChange* Parent = ParentSummary ? &ParentSummary->Change : nullptr;
		const FBlueprintHelperReviewVisibleChange* Child = ChildSummary ? &ChildSummary->Change : nullptr;

		TestNotNull(TEXT("parent exists"), Parent);
		TestNotNull(TEXT("child exists"), Child);
		if (Parent && Child)
		{
			TestEqual(TEXT("child keeps parent change id after pending index snapshot strip"),
				Child->ParentChangeId,
				Parent->ChangeId);
		}
	}

	FString DeleteError;
	Store.DeleteReviewRecord(ParentRecord.ReviewRecordId, DeleteError);
	Store.DeleteReviewRecord(ChildRecord.ReviewRecordId, DeleteError);
	return true;
}
```

- [ ] **Step 3: Run the failing test**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.Review.PendingIndex.PageLinksCrossRecordComponentParentAfterSnapshotStrip; Quit" -TestExit="Automation Test Queue Empty"
```

Expected before implementation: FAIL because `Child->ParentChangeId` is empty.

### Task 2: Persist Compact Lifecycle Parent Metadata

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Shared/Review/BlueprintHelperReviewTypes.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewStoreJsonUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewStoreTargetUtils.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewStoreTargetUtils.cpp`

- [ ] **Step 1: Add fields to `FBlueprintHelperReviewAtomicTarget`**

Add after `ComponentPath`:

```cpp
FString LifecycleObjectKey;
FString LifecycleParentKey;
```

Key format:

```text
component:<lowercase component name>
widget:<lowercase widget name>
asset:asset
```

- [ ] **Step 2: Serialize and read the new fields**

In `ReviewAtomicTargetToJson`, add:

```cpp
if (!Target.LifecycleObjectKey.IsEmpty()) Json->SetStringField(TEXT("lifecycle_object_key"), Target.LifecycleObjectKey);
if (!Target.LifecycleParentKey.IsEmpty()) Json->SetStringField(TEXT("lifecycle_parent_key"), Target.LifecycleParentKey);
```

In atomic target readback, add:

```cpp
TargetJson->TryGetStringField(TEXT("lifecycle_object_key"), Target.LifecycleObjectKey);
TargetJson->TryGetStringField(TEXT("lifecycle_parent_key"), Target.LifecycleParentKey);
```

- [ ] **Step 3: Derive metadata before snapshot stripping**

Create a helper in `BlueprintHelperReviewStoreTargetUtils.cpp`:

```cpp
static FString BlueprintHelperReviewMakeCompactLifecycleKey(const TCHAR* Kind, const FString& Name)
{
	FString Normalized = BlueprintHelperReviewExtractLifecycleName(Name);
	if (!Kind || Normalized.IsEmpty())
	{
		return FString();
	}
	return FString::Printf(TEXT("%s:%s"), Kind, *Normalized);
}
```

Then update target metadata before save/read/pending index paths by ensuring:

```cpp
if (HandlerKind == EBlueprintHelperReviewTargetHandlerKind::Component)
{
	Target.LifecycleObjectKey = BlueprintHelperReviewMakeCompactLifecycleKey(
		TEXT("component"),
		BlueprintHelperReviewLifecycleObjectNameFromTarget(Target));

	FString ParentComponentName;
	if (BlueprintHelperReviewTryGetTargetSnapshotString(Target, TEXT("parent_component"), ParentComponentName)
		|| BlueprintHelperReviewTryGetTargetAnchorString(Target, TEXT("parent_component"), ParentComponentName))
	{
		Target.LifecycleParentKey = BlueprintHelperReviewMakeCompactLifecycleKey(TEXT("component"), ParentComponentName);
	}
}
```

Add `BlueprintHelperReviewTryGetTargetAnchorString` beside the existing snapshot helper and parse `Target.AnchorJson` as JSON.

- [ ] **Step 4: Link children using compact metadata first**

Update `BlueprintHelperReviewCollectLifecycleRootKeys` and `BlueprintHelperReviewCollectLifecycleParentKeys`:

```cpp
if (!Target.LifecycleObjectKey.IsEmpty())
{
	OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(Change.AssetPath, TEXT("object"), Target.LifecycleObjectKey));
	continue;
}
```

```cpp
if (!Target.LifecycleParentKey.IsEmpty())
{
	OutKeys.AddUnique(BlueprintHelperReviewLifecycleKey(Change.AssetPath, TEXT("object"), Target.LifecycleParentKey));
	continue;
}
```

Keep snapshot fallback for old records only after compact metadata is absent.

- [ ] **Step 5: Run pending index test again**

Run the Task 1 test command.

Expected after implementation: PASS.

### Task 3: Guard Component Snapshot Restore Against Child Reparenting

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewSnapshotRestoreService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Add failing component restore guard test**

Add a test that creates a Blueprint SCS parent/child pair, then attempts to restore the parent component to `exists=false` without first removing the child.

Expected assertion:

```cpp
TestFalse(TEXT("component restore refuses to remove parent with children"), bRestored);
TestTrue(TEXT("error reports children guard"), Error.Contains(TEXT("snapshot_restore_component_has_children")));
```

- [ ] **Step 2: Implement the guard**

In `RestoreComponentFromSnapshot`, before `RemoveNode(Node)`:

```cpp
if (Node->GetChildNodes().Num() > 0)
{
	OutError = FString::Printf(
		TEXT("snapshot_restore_component_has_children:%s"),
		*ComponentName);
	return false;
}
```

- [ ] **Step 3: Run the guard test**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.Review.Action.ComponentSnapshotRestoreRefusesParentWithChildren; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: PASS after guard implementation.

### Task 4: Rewrite Component/Widget Lifecycle Reject Cascade

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewRejectService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewActionService.h`

- [ ] **Step 1: Add failing mixed-record cascade test**

Create a test where one child record has two changes:

```text
ParentA
  ChildA
ParentB
  ChildB
```

Reject `ParentA`.

Expected:

```cpp
TestTrue(TEXT("ChildA removed by cascade"), !PendingChangeIds.Contains(TEXT("child_a_change_id")));
TestTrue(TEXT("ChildB remains because it belongs to another branch"), PendingChangeIds.Contains(TEXT("child_b_change_id")));
```

- [ ] **Step 2: Build descendant list from complete pending graph**

Replace record-level child deletion with change-level collection:

```cpp
static TArray<FBlueprintHelperReviewVisibleChange> CollectLifecycleDescendantsDeepestFirst(
	const FBlueprintHelperReviewVisibleChange& Root,
	const TArray<FBlueprintHelperReviewVisibleChange>& PendingChanges)
```

Sort descendants by depth descending, then execution order descending.

- [ ] **Step 3: Reject descendants before component/widget root**

For component/widget lifecycle roots:

```cpp
for (const FBlueprintHelperReviewVisibleChange& Descendant : DescendantsDeepestFirst)
{
	FString ChildReviewRecordId;
	TArray<FString> ChildTargetKeys;
	if (FBlueprintHelperReviewActionTargetUtils::TryResolvePersistedReviewChange(
		Descendant,
		ChildReviewRecordId,
		ChildTargetKeys))
	{
		FBlueprintHelperReviewActionResult ChildResult =
			RejectReviewTargets(ChildReviewRecordId, ChildTargetKeys, Options);
		if (!ChildResult.bSucceeded)
		{
			return MakeCascadeFailureWithChildResult(Root, ChildResult);
		}
	}
}
```

Then reject the root. Do not call `DeleteReviewRecordAndLinkedDebugCases` for descendant records directly.

- [ ] **Step 4: Keep asset factory behavior separate**

For asset factory lifecycle roots, keep asset-level semantics separate:

```cpp
if (RootIsAssetFactory)
{
	// Existing asset-root behavior can delete same-asset pending records after root succeeds.
}
```

Component/widget lifecycle roots must not use whole child-record deletion.

- [ ] **Step 5: Run cascade tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.Review.Action.RejectComponentLifecycleRoot; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: component parent reject removes only descendants in that branch and keeps unrelated branch changes.

### Task 5: Ensure Action Service Uses Complete Pending Lifecycle Graph

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPanelCommandService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewActionService.h`

- [ ] **Step 1: Add an action-service overload that loads complete pending changes**

Add:

```cpp
FBlueprintHelperReviewCascadeActionResult RejectLifecycleRootVisibleChange(
	const FBlueprintHelperReviewVisibleChange& Root,
	const FBlueprintHelperReviewRejectOptions& Options) const;
```

Implementation:

```cpp
FBlueprintHelperReviewStoreService Store;
const TArray<FBlueprintHelperReviewVisibleChange> PendingChanges =
	Store.LoadPendingVisibleChanges(Root.AssetPath);
return RejectLifecycleRootVisibleChange(Root, PendingChanges, Options);
```

- [ ] **Step 2: Route UI command service to the overload**

The UI should not pass only the current page as the cascade boundary. Replace UI-local snapshot dependency with action service loading the full pending model.

- [ ] **Step 3: Keep presenter/UI signatures thin**

UI still sends:

```cpp
RejectLifecycleRootVisibleChange(Change, Options)
```

It must not derive component children from Slate rows, details panel rows, or component tree UI.

### Task 6: Runtime Fixture Verification

**Files:**
- Update: `Debug/BlueprintHelper_ComponentHierarchyReviewEvents_20260601.md`

- [ ] **Step 1: Regenerate a clean component hierarchy fixture**

Use the existing TaskSpecs under:

```text
Debug/TaskSpecs/ComponentHierarchy_20260601_143207/
```

or create a new timestamped fixture if the current one has already been mutated by manual Reject testing.

- [ ] **Step 2: Reject parent components in ReviewPanel**

Test two paths:

```text
Reject CH_B_ChildScene
Reject CH_RootA
```

- [ ] **Step 3: Read back components**

Run:

```powershell
bh.cmd context read --file Debug\TaskSpecs\ComponentHierarchy_20260601_143207\read_components.json --format json
```

Expected:

```text
CH_B_ChildScene absent
CH_B_LeafMesh absent
CH_RootA absent
CH_A_ChildScene absent
CH_A_ChildMesh absent
CH_A_GrandChildScene absent
CH_A_GrandChildMesh absent
```

- [ ] **Step 4: Read back ReviewRecords**

Run:

```powershell
bh.cmd blueprinthelper_query_review_records --file Debug\TaskSpecs\ComponentHierarchy_20260601_143207\query_review.json --format json
```

Expected:

```text
No pending ReviewEvent remains for rejected component subtrees.
Unrelated same-asset ReviewEvents remain if they are outside the rejected branch.
```

### Task 7: Final Regression Sweep

**Files:**
- Update: `Debug/BlueprintHelper_ComponentHierarchyReviewEvents_20260601.md`

- [ ] **Step 1: Run targeted tests**

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.Review.PendingIndex.PageLinksCrossRecordComponentParentAfterSnapshotStrip; Automation RunTests BlueprintHelper.Review.Action.RejectComponentLifecycleRoot; Quit" -TestExit="Automation Test Queue Empty"
```

Expected: PASS.

- [ ] **Step 2: Build plugin**

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development 'D:\UEProjects\Template\Template.uproject' -WaitMutex
```

Expected: build succeeds.

- [ ] **Step 3: Update Debug evidence**

Append:

```text
Implementation completed.
Targeted tests passed.
MCP/CLI readback confirmed rejected component subtrees are removed and ReviewEvents are pruned.
```

## Self-Review

- Spec coverage:
  - Pending/page model parent metadata: Task 1, Task 2.
  - Actual SCS child reparenting bug: Task 3, Task 4, Task 6.
  - Cross-task/cross-record child ReviewEvent residuals: Task 4, Task 5, Task 6.
  - UI architecture boundary: Task 5.
- Placeholder scan:
  - 未留下占位实现缺口。
- Risk notes:
  - `FBlueprintHelperReviewAtomicTarget` schema changes require JSON readback defaults for older records.
  - Existing records that only have snapshot parent data can be repaired during load because raw ReviewRecord still stores full snapshots; pending index can be rebuilt.
- If a component has user-authored children not represented by ReviewRecords, the new restore guard should leave root as `needs_action` instead of corrupting the SCS hierarchy.

## 2026-06-01 Implementation Progress

- Completed compact lifecycle metadata on review atomic targets and JSON persistence.
- Completed pending/page lifecycle parent linking after snapshot stripping.
- Completed component restore guard for `exists=false` targets with live child SCS nodes.
- Completed component/widget lifecycle root reject through deepest-first descendant restore and per-target pruning.
- Completed action-service overload that loads the complete pending lifecycle graph instead of relying on current UI page rows.
- Completed ReviewPanel command/presenter routing to the unified action-service lifecycle root path.
- Completed CLI Bridge route alignment for `blueprinthelper_apply_review_action` so `review_record_id + target_keys` reaches the same ReviewActionService pipeline.
- Completed retry recovery for `reject_failed` lifecycle roots by allowing them to participate in parent linking.
- Verified runtime fixture `Debug\TaskSpecs\ComponentHierarchy_20260601_154420`:
  - Reject `CH_B_ChildScene` removed `CH_B_LeafMesh` and `CH_B_ChildScene`.
  - Reject `CH_RootA` removed `CH_A_ChildScene`, `CH_A_ChildMesh`, `CH_A_GrandChildScene`, `CH_A_GrandChildMesh`, and `CH_RootA`.
  - Component readback left only `DefaultSceneRoot`, `CH_RootB`, and `CH_AuxRoot`.
  - ReviewRecord query left only unrelated `CH_RootB`/`CH_AuxRoot` pending plus asset factory `needs_action`.
- Targeted automation coverage added/passed:
  - `BlueprintHelper.Review.PendingIndex.PageLinksCrossRecordComponentParentAfterSnapshotStrip`
  - `BlueprintHelper.Review.PendingIndex.PageLinksRejectFailedLifecycleRoot`
  - `BlueprintHelper.Review.Action.ComponentSnapshotRestoreRefusesParentWithChildren`
  - `BlueprintHelper.Review.Action.RejectComponentLifecycleRoot`
  - Existing lifecycle reject regressions remained passing.
- Build passed:
  - `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development D:\UEProjects\Template\Template.uproject -WaitMutex`
- Final regression sweep passed with one `UnrealEditor-Cmd` invocation per test:
  - `BlueprintHelper.Review.PendingIndex.PageLinksCrossRecordComponentParentAfterSnapshotStrip`
  - `BlueprintHelper.Review.PendingIndex.PageLinksRejectFailedLifecycleRoot`
  - `BlueprintHelper.Review.Action.ComponentSnapshotRestoreRefusesParentWithChildren`
  - `BlueprintHelper.Review.Action.RejectComponentLifecycleRoot`
  - `BlueprintHelper.Review.Action.RejectLifecycleRootRemovesChildren`
  - `BlueprintHelper.Review.Action.AssetRootRejectUsesLifecycleCascade`
  - `BlueprintHelper.Review.Action.RejectLifecycleRootFailureKeepsChildren`
  - `BlueprintHelper.Review.PanelCommand.RejectLifecycleRootRequiresActionServiceDiagnostic`
- Final UBT check passed with `Target is up to date`.

## 2026-06-01 Audit Follow-up

- Fixed audit P1: hash drift no longer blocks Reject.
  - `HashGuard*` fields remain diagnostic evidence.
  - Reject result now follows evidence-before snapshot restore and the real restore error.
  - Updated hash drift tests away from `current_state_changed` as a blocking reason.
- Fixed audit P2: CLI `apply_review_action` now only enters lifecycle cascade for matched lifecycle roots with `bRejectRemovesChildren`, matching UI semantics.
- Fixed audit P2 residual: asset-root cascade child discovery now includes `reject_failed` children.
- Added coverage for the audit gaps:
  - `BlueprintHelper.Review.PendingIndex.PageLinksComponentParentFromSnapshotOnly`
  - `BlueprintHelper.Review.PanelCommand.RejectLifecycleRootLoadsFullPendingGraph`
  - `BlueprintHelper.Review.Action.CollectLifecycleDescendantsDeepestFirst`
- Final audit-triggered regression sweep passed for 15 targeted tests, including lifecycle cascade, pending-index parent linking, PanelCommand lifecycle root path, and hash drift diagnostics.
- Final build passed:
  - `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development D:\UEProjects\Template\Template.uproject -WaitMutex`
- Final read-only audit reported no P0/P1/P2 blocking findings after the audit follow-up changes.
