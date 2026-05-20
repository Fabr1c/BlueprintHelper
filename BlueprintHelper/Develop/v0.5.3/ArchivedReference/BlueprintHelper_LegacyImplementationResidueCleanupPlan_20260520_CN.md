# BlueprintHelper Legacy Implementation Residue Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 清除 UE 侧仍会污染当前架构的旧工具实现、旧兼容入口、空壳占位和旧 Review fallback，让运行时路径只保留当前 TaskRuntime / SemanticIR / Review v2 语义。

**Architecture:** 清理遵循“先移除运行时可触达入口，再移除兼容 fallback，最后收敛底层实现细节”的顺序。所有删除都以现有新架构作为唯一来源：TaskRuntime 作为普通工具执行入口，GraphWrite 只接受 `logic_spec` / SemanticIR 主路径，Review 只使用 v2 `AtomicTargets` / evidence before snapshot，不再靠 legacy location/hash/anchor 猜测。

**Tech Stack:** UE 5.6 C++、BlueprintHelper Bridge、TaskRuntime、GraphWrite SemanticIR、Review v2、Unreal Automation Tests、PowerShell、UnrealBuildTool。

---

## 0. 审计基线

本计划基于 2026-05-20 并发审计结论：

- GraphWrite 主写入链路已经封禁旧 `nodes/links` 直接建图路径，但 `append/replace` 仍构造 `BlueprintHelper.AgentImportGraph` 兼容 payload。
- GraphWrite 底层 `NodeHandlers` 仍直接 `NewObject<UK2Node> + AddNode + AllocateDefaultPins`，这不是立即阻断，但需要迁移到统一 node factory/mutator。
- Review v2 主链路没有旧 transaction 驱动，但仍有 `LegacyLocationMatchesSurface`、legacy merge key、legacy graph anchor 标记。
- `AnimationBlueprint` / `Material` 仍存在空壳 route / cluster / public enum，但没有真实能力。
- `rollback_cleanup_transaction`、`cleanup_blueprint_helper_block`、`convert_blueprint_helper_block_to_user_owned` 在 C++ 源码内无命中。
- `CleanupOwnership` 还有空目录脚手架。
- `import_json` / `import_agent_graph` 仍绕过 TaskRuntime，并返回专用 result schema。

## 1. Scope

### In Scope

- 删除运行时可触达的旧兼容 fallback。
- 删除没有真实执行能力的空壳工具簇。
- 移除旧 `AgentImportGraph` 命名和兼容 payload。
- 把 `import_json` / `import_agent_graph` 从普通工具面冻结或迁入 TaskRuntime。
- 为清理结果增加静态回归测试。
- 编译验证。

### Out of Scope

- 不实现 Timeline/Timer/Latent 新框架。
- 不重写全部 `NodeHandlers` 到 node factory；本计划只建立迁移边界和第一层 factory seam。
- 不兼容历史 ReviewRecord；历史脏数据允许显示不完整，不再用 legacy fallback 猜测修复。

## 2. File Structure Map

### Bridge / ToolSurface

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h`
  - 删除 `AnimationBlueprint` / `Material` route cluster 枚举。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp`
  - 删除 `AnimationBlueprint` / `Material` cluster name 和 command cluster 映射。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp`
  - 删除空壳 cluster dispatch 分支。
  - 处理 `import_json` / `import_agent_graph` 的迁移或冻结。
- Delete: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/Routes/BlueprintHelperAnimationBlueprintBridgeRoutes.h`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelperAnimationBlueprintBridgeRoutes.cpp`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/Routes/BlueprintHelperMaterialBridgeRoutes.h`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelperMaterialBridgeRoutes.cpp`

### TaskRuntime

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h`
  - 删除 AnimationBlueprint / Material cluster 成员和类型。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.cpp`
  - 删除 AnimationBlueprint / Material cluster 初始化、注册、统计。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterHubUtils.cpp`
  - 确认 route table 不含空壳 cluster。
- Delete: `BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/Clusters/AnimationBlueprint/BlueprintHelperAnimationBlueprintTaskRuntimeCluster.h`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/AnimationBlueprint/BlueprintHelperAnimationBlueprintTaskRuntimeCluster.cpp`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/Clusters/Material/BlueprintHelperMaterialTaskRuntimeCluster.h`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/Material/BlueprintHelperMaterialTaskRuntimeCluster.cpp`

### Review v2

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Shared/Review/BlueprintHelperReviewTargetKindRegistry.cpp`
  - 删除 `LegacyLocationMatchesSurface` 运行时 fallback。
  - `BlueprintHelperReviewShouldShowOnSurface()` 在无 explicit targets 时返回 false。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Shared/Review/BlueprintHelperReviewTargetKindRegistry.h`
  - 删除 `LegacyLocationMatchesSurface` 声明。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewStoreMergeUtils.cpp`
  - 删除 legacy collapse key。
  - 只允许 `change_id` 和 active lifecycle root 折叠。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/Utils/BlueprintHelperReviewGraphBoundsUtils.cpp`
  - 删除 `legacy` anchor source fallback。
  - 缺 structured anchor 时输出 `none`。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewGraphBoundsUtils.h`
  - 如存在 legacy 计数字段，删除或改为 structured/none。

### GraphWrite

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.cpp`
  - 删除 `BlueprintHelper.AgentImportGraph` payload 构造。
  - 改为原生 SemanticIR payload DTO 或直接调用 GraphWrite generation pipeline。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp`
  - 同 Append。
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphWriteSemanticPayload.h`
  - 新 DTO：描述 target asset、graph、mode、options、logic_spec。
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphWriteSemanticPayload.cpp`
  - DTO JSON 解析/构建工具。

### CleanupOwnership

- Delete empty directory: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/CleanupOwnership`
- Delete empty directory: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/TaskPlanAdapters/CleanupOwnership`
- Delete empty directory: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/CleanupOwnership`
- Delete empty directory: `BlueprintHelper/Source/BlueprintHelper/Private/Shared/CleanupOwnership`

### Tests

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Bridge/BlueprintHelperBridgeRoutePlannerTests.cpp`
  - 增加旧 route/cluster 字符串保持 Unknown 的测试。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/TaskRuntime/BlueprintHelperTaskRuntimeClusterHubTests.cpp`
  - 增加 AnimationBlueprint/Material 不存在的静态断言或运行时 unknown 测试。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`
  - 增加无 `AtomicTargets` 的 Review v2 不再 legacy route 到 Surface 的测试。
  - 增加 legacy collapse key 不折叠两个不同 ChangeId 的测试。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`
  - 增加 Append/Replace 不再输出 `BlueprintHelper.AgentImportGraph` 的测试。

## 3. Task Breakdown

### Task 1: 删除 AnimationBlueprint / Material 空壳 route 和 cluster

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/BlueprintHelperBridgeRoutePlanner.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.cpp`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/Routes/BlueprintHelperAnimationBlueprintBridgeRoutes.h`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelperAnimationBlueprintBridgeRoutes.cpp`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Public/Entry/Bridge/Routes/BlueprintHelperMaterialBridgeRoutes.h`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Routes/BlueprintHelperMaterialBridgeRoutes.cpp`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/Clusters/AnimationBlueprint/BlueprintHelperAnimationBlueprintTaskRuntimeCluster.h`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/AnimationBlueprint/BlueprintHelperAnimationBlueprintTaskRuntimeCluster.cpp`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/Clusters/Material/BlueprintHelperMaterialTaskRuntimeCluster.h`
- Delete: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/Material/BlueprintHelperMaterialTaskRuntimeCluster.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Bridge/BlueprintHelperBridgeRoutePlannerTests.cpp`

- [ ] **Step 1: Write the failing route planner test**

Add this test to `BlueprintHelperBridgeRoutePlannerTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperRetiredEmptyClustersAreUnknownTest,
	"BlueprintHelper.Bridge.RoutePlanner.RetiredEmptyClustersAreUnknown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperRetiredEmptyClustersAreUnknownTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperBridgeRoutePlan AnimationPlan =
		FBlueprintHelperBridgeRoutePlanner::BuildPlan(TEXT("animation_blueprint"));
	TestFalse(TEXT("animation_blueprint route is not known"), AnimationPlan.bKnownCommand);
	TestEqual(TEXT("animation_blueprint cluster is Unknown"),
		AnimationPlan.Cluster,
		EBlueprintHelperBridgeRouteCluster::Unknown);

	const FBlueprintHelperBridgeRoutePlan MaterialPlan =
		FBlueprintHelperBridgeRoutePlanner::BuildPlan(TEXT("material"));
	TestFalse(TEXT("material route is not known"), MaterialPlan.bKnownCommand);
	TestEqual(TEXT("material cluster is Unknown"),
		MaterialPlan.Cluster,
		EBlueprintHelperBridgeRouteCluster::Unknown);

	return true;
}
```

- [ ] **Step 2: Run the focused compile to confirm the current code still compiles before deletion**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReloadFromIDE
```

Expected: compile succeeds before behavior removal. If Unreal Editor is open and the DLL is locked, close it with global MCP `blueprint_close_editor(save_all=true)` and rerun the same command.

- [ ] **Step 3: Remove public enum entries**

In `BlueprintHelperBridgeRoutePlanner.h`, change:

```cpp
	Review,
	AnimationBlueprint,
	Material
```

to:

```cpp
	Review
```

- [ ] **Step 4: Remove cluster names and command mappings**

In `BlueprintHelperBridgeRoutePlannerUtils.cpp`, remove:

```cpp
	{EBlueprintHelperBridgeRouteCluster::AnimationBlueprint, TEXT("AnimationBlueprint")},
	{EBlueprintHelperBridgeRouteCluster::Material, TEXT("Material")},
```

Remove any entries in `GBlueprintHelperBridgeRouteCommandClusters` that map commands to those clusters.

- [ ] **Step 5: Remove router members and dispatch branches**

In `BlueprintHelperBridgeRouter.cpp`, delete includes, constructor members, and `switch` / `if` dispatch branches that reference:

```cpp
FBlueprintHelperAnimationBlueprintBridgeRoutes
FBlueprintHelperMaterialBridgeRoutes
EBlueprintHelperBridgeRouteCluster::AnimationBlueprint
EBlueprintHelperBridgeRouteCluster::Material
```

- [ ] **Step 6: Remove TaskRuntime cluster members**

In `BlueprintHelperTaskRuntimeClusterHub.h` and `.cpp`, delete all members and includes for:

```cpp
FBlueprintHelperAnimationBlueprintTaskRuntimeCluster
FBlueprintHelperMaterialTaskRuntimeCluster
```

- [ ] **Step 7: Delete empty implementation files**

Run:

```powershell
Remove-Item -LiteralPath "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Entry\Bridge\Routes\BlueprintHelperAnimationBlueprintBridgeRoutes.h"
Remove-Item -LiteralPath "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Entry\Bridge\Routes\BlueprintHelperAnimationBlueprintBridgeRoutes.cpp"
Remove-Item -LiteralPath "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Entry\Bridge\Routes\BlueprintHelperMaterialBridgeRoutes.h"
Remove-Item -LiteralPath "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Entry\Bridge\Routes\BlueprintHelperMaterialBridgeRoutes.cpp"
Remove-Item -LiteralPath "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Runtime\TaskRuntime\Clusters\AnimationBlueprint\BlueprintHelperAnimationBlueprintTaskRuntimeCluster.h"
Remove-Item -LiteralPath "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Runtime\TaskRuntime\Clusters\AnimationBlueprint\BlueprintHelperAnimationBlueprintTaskRuntimeCluster.cpp"
Remove-Item -LiteralPath "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Runtime\TaskRuntime\Clusters\Material\BlueprintHelperMaterialTaskRuntimeCluster.h"
Remove-Item -LiteralPath "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Runtime\TaskRuntime\Clusters\Material\BlueprintHelperMaterialTaskRuntimeCluster.cpp"
```

- [ ] **Step 8: Verify no symbol residue**

Run:

```powershell
rg -n "AnimationBlueprint|MaterialBridgeRoutes|MaterialTaskRuntimeCluster|AnimationBlueprintTaskRuntimeCluster" "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper"
```

Expected: only real Material feature files remain, not bridge route or empty TaskRuntime cluster symbols.

- [ ] **Step 9: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper
git commit -m "变更需求：
1. 移除 AnimationBlueprint/Material 空壳 route 和 TaskRuntime cluster"
```

### Task 2: 移除 Review legacy surface fallback

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Shared/Review/BlueprintHelperReviewTargetKindRegistry.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Shared/Review/BlueprintHelperReviewTargetKindRegistry.h`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Write the failing no-target routing test**

Add this test to `BlueprintHelperReviewStoreServiceTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewV2RequiresExplicitSurfaceTargetsTest,
	"BlueprintHelper.Review.V2.RequiresExplicitSurfaceTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewV2RequiresExplicitSurfaceTargetsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange Change;
	Change.ChangeId = TEXT("change_without_targets");
	Change.DisplayLabel = TEXT("component DoorFrame");
	Change.LocationKey = TEXT("component DoorFrame");
	Change.ChangeKind = EBlueprintHelperReviewChangeKind::ComponentAdded;
	Change.GraphName.Reset();
	Change.AtomicTargets.Reset();

	TestFalse(TEXT("Components surface does not use legacy location fallback"),
		BlueprintHelperReviewShouldShowInComponents(Change));
	TestFalse(TEXT("MyBlueprint surface does not use legacy location fallback"),
		BlueprintHelperReviewShouldShowInMyBlueprint(Change));
	TestFalse(TEXT("Graph surface does not use legacy graph fallback"),
		BlueprintHelperReviewShouldShowInGraph(Change));

	return true;
}
```

- [ ] **Step 2: Remove the public legacy declaration**

In `BlueprintHelperReviewTargetKindRegistry.h`, delete:

```cpp
static bool LegacyLocationMatchesSurface(
	const FString& NormalizedLocation,
	const FString& GraphName,
	EBlueprintHelperReviewChangeKind ChangeKind,
	EBlueprintHelperReviewSurface Surface);
```

- [ ] **Step 3: Delete the implementation**

In `BlueprintHelperReviewTargetKindRegistry.cpp`, delete the full `FBlueprintHelperReviewTargetKindRegistry::LegacyLocationMatchesSurface(...)` function.

- [ ] **Step 4: Make v2 surface routing explicit-target only**

Change `BlueprintHelperReviewShouldShowOnSurface()` to:

```cpp
bool BlueprintHelperReviewShouldShowOnSurface(
	const FBlueprintHelperReviewVisibleChange& Change,
	EBlueprintHelperReviewSurface Surface)
{
	if (!BlueprintHelperReviewHasExplicitTargets(Change))
	{
		return false;
	}

	return Surface == EBlueprintHelperReviewSurface::Details
		? BlueprintHelperReviewCountDetailsTargets(Change) > 0
		: BlueprintHelperReviewCountSurfaceTargets(Change, Surface) > 0;
}
```

- [ ] **Step 5: Verify no LegacyLocationMatchesSurface residue**

Run:

```powershell
rg -n "LegacyLocationMatchesSurface|BlueprintHelperReviewLegacyTextMatchesSurface" "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper"
```

Expected: no runtime source matches. Test names containing old behavior should be deleted or rewritten to v2 explicit-target expectations.

- [ ] **Step 6: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper
git commit -m "修复内容：
1. 移除 Review v2 的 legacy surface location fallback"
```

### Task 3: 移除 Review legacy merge collapse key

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/Utils/BlueprintHelperReviewStoreMergeUtils.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Write the failing merge test**

Add this test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewV2DoesNotCollapseByLegacyScopeIdentityTest,
	"BlueprintHelper.Review.V2.DoesNotCollapseByLegacyScopeIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewV2DoesNotCollapseByLegacyScopeIdentityTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewVisibleChange First;
	First.ChangeId = TEXT("change_a");
	First.AssetPath = TEXT("/Game/BP_A");
	First.DisplayLabel = TEXT("variable SmokeLabel");
	First.LocationKey = TEXT("variable SmokeLabel");
	First.ChangeKind = EBlueprintHelperReviewChangeKind::VariableModified;

	FBlueprintHelperReviewVisibleChange Second = First;
	Second.ChangeId = TEXT("change_b");

	TArray<FBlueprintHelperReviewVisibleChange> Changes;
	Changes.Add(First);
	Changes.Add(Second);

	FBlueprintHelperReviewStoreMergeUtils::CollapseVisibleChangesLatestWins(Changes, TEXT("test"));

	TestEqual(TEXT("different v2 change ids do not collapse by legacy scope"), Changes.Num(), 2);
	return true;
}
```

- [ ] **Step 2: Remove legacy index map**

In `BlueprintHelperReviewStoreMergeUtils.cpp`, remove:

```cpp
TMap<FString, int32> ExistingIndexByLegacyKey;
```

and all additions to `ExistingIndexByLegacyKey`.

- [ ] **Step 3: Remove legacy collapse reason**

In `BlueprintHelperReviewFindCollapseReason()`, delete:

```cpp
const FString ExistingLegacyKey =
	FBlueprintHelperReviewStoreMergeUtils::MakeLoadedVisibleChangeCollapseKey(Existing);
const FString IncomingLegacyKey =
	FBlueprintHelperReviewStoreMergeUtils::MakeLoadedVisibleChangeCollapseKey(Incoming);
if (!ExistingLegacyKey.IsEmpty() && ExistingLegacyKey == IncomingLegacyKey)
{
	OutReason = TEXT("scope_identity");
	return true;
}
```

- [ ] **Step 4: Retire MakeLoadedVisibleChangeCollapseKey**

Delete `MakeLoadedVisibleChangeCollapseKey()` if no caller remains. If tests still reference it, update tests to `MakeLoadedVisibleChangeChangeIdCollapseKey()` or `MakeLoadedVisibleChangeLifecycleRootCollapseKey()`.

- [ ] **Step 5: Verify no scope identity fold**

Run:

```powershell
rg -n "scope_identity|MakeLoadedVisibleChangeCollapseKey|ExistingIndexByLegacyKey" "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper"
```

Expected: no runtime matches.

- [ ] **Step 6: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper
git commit -m "修复内容：
1. 移除 Review v2 的 legacy scope 合并回退"
```

### Task 4: 移除 Graph bounds legacy anchor source

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/Utils/BlueprintHelperReviewGraphBoundsUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewGraphBoundsUtils.h`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Write the failing graph anchor test**

Add this test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewGraphBoundsMissingStructuredAnchorIsNoneTest,
	"BlueprintHelper.Review.GraphBounds.MissingStructuredAnchorIsNone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewGraphBoundsMissingStructuredAnchorIsNoneTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewAtomicTarget Target;
	Target.bHasGraphBounds = false;
	Target.AnchorJson.Reset();

	const FBlueprintHelperReviewGraphBoundsUtils::FRecordedGraphBounds Bounds =
		FBlueprintHelperReviewGraphBoundsUtils::GetRecordedBoundsForTarget(Target);

	TestFalse(TEXT("missing structured anchor has no bounds"), Bounds.bHasGraphBounds);
	TestEqual(TEXT("missing structured anchor source is none"), Bounds.AnchorSource, FString(TEXT("none")));
	return true;
}
```

- [ ] **Step 2: Change missing anchor source to none**

In `TryReadAnchorJson()`, replace:

```cpp
else if (OutRecordedBounds.AnchorSource.IsEmpty())
{
	OutRecordedBounds.AnchorSource = TEXT("legacy");
}
```

with:

```cpp
else if (OutRecordedBounds.AnchorSource.IsEmpty())
{
	OutRecordedBounds.AnchorSource = TEXT("none");
}
```

- [ ] **Step 3: Stop classifying direct graph bounds as legacy**

In `GetRecordedBoundsForTarget()`, replace:

```cpp
RecordedBounds.AnchorSource = TEXT("legacy");
```

with:

```cpp
RecordedBounds.AnchorSource = TEXT("structured");
```

If `bHasGraphBounds` is treated as old data in current model, replace it with:

```cpp
RecordedBounds.AnchorSource = TEXT("none");
```

and require `AnchorJson` for structured anchors.

- [ ] **Step 4: Remove legacy summary text**

Change `BuildAnchorSourceSummary()` so it returns only:

```cpp
if (DebugCounters.StructuredAnchorSourceCount > 0)
{
	return TEXT("structured");
}
return TEXT("none");
```

Remove `LegacyAnchorSourceCount` if no writer remains.

- [ ] **Step 5: Verify no legacy anchor residue**

Run:

```powershell
rg -n "LegacyAnchorSourceCount|anchor_source.*legacy|TEXT\\(\"legacy\"\\)" "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\UI\Review" "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\UI\Review"
```

Expected: no Review UI graph bounds legacy anchor matches.

- [ ] **Step 6: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper
git commit -m "修复内容：
1. 移除 GraphPanel diff 的 legacy anchor source fallback"
```

### Task 5: 移除 Append/Replace 的 AgentImportGraph 兼容 payload

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphWriteSemanticPayload.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphWriteSemanticPayload.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`

- [ ] **Step 1: Write the failing payload string test**

Add this test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteDoesNotEmitAgentImportGraphPayloadTest,
	"BlueprintHelper.GraphWrite.SemanticPayload.DoesNotEmitAgentImportGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteDoesNotEmitAgentImportGraphPayloadTest::RunTest(const FString& Parameters)
{
	FString SourceText;
	FFileHelper::LoadFileToString(
		SourceText,
		*FPaths::ConvertRelativePathToFull(
			FPaths::Combine(
				FPaths::ProjectPluginsDir(),
				TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.cpp"))));

	TestFalse(TEXT("Append service does not emit AgentImportGraph"),
		SourceText.Contains(TEXT("BlueprintHelper.AgentImportGraph")));

	FString ReplaceText;
	FFileHelper::LoadFileToString(
		ReplaceText,
		*FPaths::ConvertRelativePathToFull(
			FPaths::Combine(
				FPaths::ProjectPluginsDir(),
				TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.cpp"))));

	TestFalse(TEXT("Replace service does not emit AgentImportGraph"),
		ReplaceText.Contains(TEXT("BlueprintHelper.AgentImportGraph")));

	return true;
}
```

- [ ] **Step 2: Create the semantic payload DTO header**

Create `BlueprintHelperGraphWriteSemanticPayload.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

class FJsonObject;

struct FBlueprintHelperGraphWriteSemanticPayload
{
	FString TargetBlueprint;
	FString TargetGraph;
	FString Mode;
	bool bCompile = false;
	bool bSave = false;
	bool bStrict = true;
	bool bDryRun = false;
	bool bCreateMissingVariables = false;
	bool bReconstructExistingNodes = false;
	TSharedPtr<FJsonObject> LogicSpec;

	TSharedRef<FJsonObject> ToJsonObject() const;
	FString ToJsonString() const;
};
```

- [ ] **Step 3: Create the semantic payload implementation**

Create `BlueprintHelperGraphWriteSemanticPayload.cpp`:

```cpp
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphWriteSemanticPayload.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

TSharedRef<FJsonObject> FBlueprintHelperGraphWriteSemanticPayload::ToJsonObject() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.GraphWriteSemanticPayload"));
	Root->SetStringField(TEXT("version"), TEXT("1.0"));
	Root->SetStringField(TEXT("target_blueprint"), TargetBlueprint);
	Root->SetStringField(TEXT("target_graph"), TargetGraph);
	Root->SetStringField(TEXT("mode"), Mode);

	TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
	Options->SetBoolField(TEXT("compile"), bCompile);
	Options->SetBoolField(TEXT("save"), bSave);
	Options->SetBoolField(TEXT("strict"), bStrict);
	Options->SetBoolField(TEXT("dry_run"), bDryRun);
	Options->SetBoolField(TEXT("create_missing_variables"), bCreateMissingVariables);
	Options->SetBoolField(TEXT("reconstruct_existing_nodes"), bReconstructExistingNodes);
	Root->SetObjectField(TEXT("options"), Options);

	if (LogicSpec.IsValid())
	{
		Root->SetObjectField(TEXT("logic_spec"), LogicSpec);
	}

	return Root;
}

FString FBlueprintHelperGraphWriteSemanticPayload::ToJsonString() const
{
	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	FJsonSerializer::Serialize(ToJsonObject(), Writer);
	return JsonText;
}
```

- [ ] **Step 4: Replace Append payload builder body**

In `BlueprintHelperAppendBlueprintGraphService.cpp`, include:

```cpp
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphWriteSemanticPayload.h"
```

Replace `BuildSemanticGraphWritePayload()` with:

```cpp
FString FBlueprintHelperAppendBlueprintGraphService::BuildSemanticGraphWritePayload(
	const FAppendRequest& Request) const
{
	FBlueprintHelperGraphWriteSemanticPayload Payload;
	Payload.TargetBlueprint = Request.AssetPath;
	Payload.TargetGraph = Request.GraphName;
	Payload.Mode = TEXT("append");
	Payload.bCompile = false;
	Payload.bSave = false;
	Payload.bStrict = true;
	Payload.bDryRun = false;
	Payload.bCreateMissingVariables = false;
	Payload.bReconstructExistingNodes = Request.bReuseExistingEntries;
	Payload.LogicSpec = Request.LogicSpec;
	return Payload.ToJsonString();
}
```

- [ ] **Step 5: Replace Replace payload builder body**

In `BlueprintHelperReplaceBlueprintGraphService.cpp`, include the same header and replace `BuildSemanticGraphWritePayload()` with:

```cpp
FString FBlueprintHelperReplaceBlueprintGraphService::BuildSemanticGraphWritePayload(
	const FReplaceRequest& Request) const
{
	FBlueprintHelperGraphWriteSemanticPayload Payload;
	Payload.TargetBlueprint = Request.AssetPath;
	Payload.TargetGraph = Request.GraphName;
	Payload.Mode = TEXT("append");
	Payload.bCompile = false;
	Payload.bSave = false;
	Payload.bStrict = true;
	Payload.bDryRun = false;
	Payload.bCreateMissingVariables = false;
	Payload.bReconstructExistingNodes = false;
	Payload.LogicSpec = Request.LogicSpec;
	return Payload.ToJsonString();
}
```

- [ ] **Step 6: Update parser acceptance if needed**

If `AgentImportJsonParser` currently requires `schema == "BlueprintHelper.AgentImportGraph"` for this internal payload, change that parser or call site to consume `BlueprintHelper.GraphWriteSemanticPayload`. The accepted schema check should be:

```cpp
if (!Root->TryGetStringField(TEXT("schema"), Schema)
	|| Schema != TEXT("BlueprintHelper.GraphWriteSemanticPayload"))
{
	// Return the existing error object with code graph_write_semantic_payload_schema_required.
}
```

- [ ] **Step 7: Verify AgentImportGraph no longer appears in GraphWrite runtime**

Run:

```powershell
rg -n "BlueprintHelper.AgentImportGraph|AgentImport 兼容 payload|AgentImport payload" "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite"
```

Expected: no matches in GraphWrite runtime source.

- [ ] **Step 8: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper
git commit -m "变更需求：
1. 移除 GraphWrite Append/Replace 的 AgentImportGraph 兼容 payload"
```

### Task 6: 直接删除 import_json / import_agent_graph 旧入口

**Files:**

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/BlueprintHelperBridgeRouter.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge\BlueprintHelperRequestValidator.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperSafetyTests.cpp`

- [ ] **Step 1: Write failing retired-import test**

Add a route-level test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperRetiredImportCommandsAreRemovedTest,
	"BlueprintHelper.Bridge.ImportCommands.RetiredImportCommandsAreRemoved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperRetiredImportCommandsAreRemovedTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperBridgeRoutePlan ImportJsonPlan =
		FBlueprintHelperBridgeRoutePlanner::BuildPlan(TEXT("import_json"));
	TestFalse(TEXT("import_json is not a public command"), ImportJsonPlan.bKnownCommand);

	const FBlueprintHelperBridgeRoutePlan AgentImportPlan =
		FBlueprintHelperBridgeRoutePlanner::BuildPlan(TEXT("import_agent_graph"));
	TestFalse(TEXT("import_agent_graph is not a public command"), AgentImportPlan.bKnownCommand);

	return true;
}
```

- [ ] **Step 2: Remove import command route mapping**

In route planner utilities, remove command entries for:

```cpp
TEXT("import_json")
TEXT("import_agent_graph")
```

- [ ] **Step 3: Remove BridgeRouter handlers from public dispatch**

In `BlueprintHelperBridgeRouter.cpp`, remove dispatch to:

```cpp
HandleImportJson
HandleImportAgentGraph
```

If internal tests still need direct import, keep service classes but do not expose them through Bridge commands.

- [ ] **Step 4: Remove validator entries for retired public import commands**

In `BlueprintHelperRequestValidator.cpp`, remove payload schema branches for `import_json` and `import_agent_graph`.

- [ ] **Step 5: Verify no public import command remains**

Run:

```powershell
rg -n "import_json|import_agent_graph" "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Entry\Bridge" "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Public\Entry\Bridge"
```

Expected: no Bridge route or validator matches.

- [ ] **Step 6: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper
git commit -m "变更需求：
1. 删除 v0.1.0 旧 import_json/import_agent_graph 入口"
```

### Task 7: 删除 CleanupOwnership 空目录并补回归检查

**Files:**

- Delete empty directory: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/Clusters/CleanupOwnership`
- Delete empty directory: `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/TaskPlanAdapters/CleanupOwnership`
- Delete empty directory: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/CleanupOwnership`
- Delete empty directory: `BlueprintHelper/Source/BlueprintHelper/Private/Shared/CleanupOwnership`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/RuntimeDiagnostics/BlueprintHelperSafetyTests.cpp`

- [x] **Step 1: Add static residue test**

Add:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperRetiredCleanupOwnershipCommandsStayRemovedTest,
	"BlueprintHelper.Runtime.RetiredCleanupOwnershipCommandsStayRemoved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperRetiredCleanupOwnershipCommandsStayRemovedTest::RunTest(const FString& Parameters)
{
	const FString SourceRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("BlueprintHelper/BlueprintHelper/Source/BlueprintHelper")));

	const FString SearchNeedles[] =
	{
		TEXT("rollback_cleanup_transaction"),
		TEXT("cleanup_blueprint_helper_block"),
		TEXT("convert_blueprint_helper_block_to_user_owned")
	};

	for (const FString& Needle : SearchNeedles)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *SourceRoot, TEXT("*.cpp"), true, false);
		IFileManager::Get().FindFilesRecursive(Files, *SourceRoot, TEXT("*.h"), true, false);

		for (const FString& File : Files)
		{
			FString Text;
			if (FFileHelper::LoadFileToString(Text, *File))
			{
				TestFalse(FString::Printf(TEXT("retired cleanup symbol absent: %s in %s"), *Needle, *File),
					Text.Contains(Needle));
			}
		}
	}

	return true;
}
```

- [x] **Step 2: Delete empty directories**

Run:

```powershell
$dirs = @(
  "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Runtime\TaskRuntime\Clusters\CleanupOwnership",
  "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Runtime\TaskRuntime\TaskPlanAdapters\CleanupOwnership",
  "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\CleanupOwnership",
  "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper\Private\Shared\CleanupOwnership"
)
foreach ($dir in $dirs) {
  if (Test-Path -LiteralPath $dir) {
    Remove-Item -LiteralPath $dir -Recurse
  }
}
```

- [x] **Step 3: Verify no CleanupOwnership path remains**

Run:

```powershell
Get-ChildItem -LiteralPath "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper" -Recurse -Directory -Filter "CleanupOwnership"
```

Expected: no output.

Execution note (2026-05-20 Task 7):
- Added `FBlueprintHelperRetiredCleanupOwnershipCommandsStayRemovedTest` in `BlueprintHelperSafetyTests.cpp`.
- Deleted the 4 listed Private CleanupOwnership empty directories plus matching empty Public/Test CleanupOwnership scaffolds found by the source-wide directory check.
- Directory verification produced no output after deletion.
- Retired command `rg` verification only matches the new static regression test needles in `BlueprintHelperSafetyTests.cpp`.
- UE compile intentionally not run in this worker; final compile is owned by the main thread.

- [ ] **Step 4: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper
git commit -m "快速修复：
1. 删除 CleanupOwnership 空目录脚手架"
```

### Task 8: 建立 GraphWrite node factory seam

**Files:**

- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphNodeFactory.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphNodeFactory.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/NodeHandlers/BranchNodeHandler.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/NodeHandlers/SequenceNodeHandler.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/NodeHandlers/CustomEventNodeHandler.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp`

- [ ] **Step 1: Create factory header**

Create:

```cpp
#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UK2Node;

class BLUEPRINTHELPER_API FBlueprintHelperGraphNodeFactory
{
public:
	template <typename TNode>
	static TNode* SpawnK2Node(UEdGraph* TargetGraph, const FVector2D& Location)
	{
		if (!TargetGraph)
		{
			return nullptr;
		}

		TNode* Node = NewObject<TNode>(TargetGraph);
		TargetGraph->AddNode(Node, true, false);
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->NodePosX = static_cast<int32>(Location.X);
		Node->NodePosY = static_cast<int32>(Location.Y);
		Node->AllocateDefaultPins();
		return Node;
	}
};
```

- [ ] **Step 2: Create factory cpp**

Create:

```cpp
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphNodeFactory.h"
```

- [ ] **Step 3: Migrate BranchNodeHandler**

Replace direct creation:

```cpp
UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(TargetGraph);
TargetGraph->AddNode(BranchNode, true, false);
BranchNode->CreateNewGuid();
BranchNode->PostPlacedNewNode();
BranchNode->NodePosX = static_cast<int32>(NodeData.X);
BranchNode->NodePosY = static_cast<int32>(NodeData.Y);
BranchNode->AllocateDefaultPins();
```

with:

```cpp
UK2Node_IfThenElse* BranchNode =
	FBlueprintHelperGraphNodeFactory::SpawnK2Node<UK2Node_IfThenElse>(
		TargetGraph,
		FVector2D(NodeData.X, NodeData.Y));
```

- [ ] **Step 4: Migrate SequenceNodeHandler**

Use:

```cpp
UK2Node_ExecutionSequence* SequenceNode =
	FBlueprintHelperGraphNodeFactory::SpawnK2Node<UK2Node_ExecutionSequence>(
		TargetGraph,
		FVector2D(NodeData.X, NodeData.Y));
```

- [ ] **Step 5: Migrate CustomEventNodeHandler**

Use:

```cpp
UK2Node_CustomEvent* EventNode =
	FBlueprintHelperGraphNodeFactory::SpawnK2Node<UK2Node_CustomEvent>(
		TargetGraph,
		FVector2D(NodeData.X, NodeData.Y));
```

Then keep event-specific field assignment exactly where it belongs.

- [ ] **Step 6: Verify first factory seam compiles**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReloadFromIDE
```

Expected: compile succeeds. This task intentionally migrates only three handlers to prove the seam before bulk migration.

- [ ] **Step 7: Commit**

```bash
git add BlueprintHelper/Source/BlueprintHelper
git commit -m "变更需求：
1. 建立 GraphWrite 统一 node factory seam"
```

### Task 9: 全量静态残留检查

**Files:**

- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_LegacyImplementationResidueCleanupPlan_20260520_CN.md`

- [ ] **Step 1: Run retired symbol checks**

Run:

```powershell
rg -n "TimelineNodeHandler|FParsedTimelineReference|EParsedBlueprintNodeType::Timeline|BlueprintHelper.AgentImportGraph|LegacyLocationMatchesSurface|scope_identity|LegacyAnchorSourceCount|rollback_cleanup_transaction|cleanup_blueprint_helper_block|convert_blueprint_helper_block_to_user_owned" "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper"
```

Expected: no matches, except historical tests that explicitly assert retired symbols remain absent.

- [ ] **Step 2: Run architecture residue checks**

Run:

```powershell
rg -n "AnimationBlueprintTaskRuntimeCluster|MaterialTaskRuntimeCluster|AnimationBlueprintBridgeRoutes|MaterialBridgeRoutes|CleanupOwnership" "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Source\BlueprintHelper"
```

Expected: no matches.

- [ ] **Step 3: Compile**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReloadFromIDE
```

Expected: `Result: Succeeded`.

- [ ] **Step 4: Update this plan document with real status**

Append:

```markdown
## 2026-05-20 Execution Status

- [x] Empty route/cluster placeholders removed.
- [x] Review legacy surface fallback removed.
- [x] Review legacy merge key removed.
- [x] Graph bounds legacy anchor source removed.
- [x] Append/Replace AgentImportGraph payload removed.
- [x] Public import_json/import_agent_graph frozen.
- [x] CleanupOwnership empty scaffolds removed.
- [x] First GraphWrite node factory seam established.
- [x] Compile passed.

Distance from expectation:

1. Remaining direct `NewObject<UK2Node>` calls in NodeHandlers are bounded implementation details until each handler is migrated to `FBlueprintHelperGraphNodeFactory`.
2. `nodes/links` may still appear in read/analysis-only paths; they are not allowed to become write input.
```

- [ ] **Step 5: Commit**

```bash
git add BlueprintHelper/Develop/Plan/BlueprintHelper_LegacyImplementationResidueCleanupPlan_20260520_CN.md BlueprintHelper/Source/BlueprintHelper
git commit -m "修复内容：
1. 完成旧实现残留清理静态验证"
```

## 4. Acceptance Criteria

- `rg` 不再找到旧 Timeline 写入实现符号。
- `rg` 不再找到 `BlueprintHelper.AgentImportGraph` 运行时 payload。
- `rg` 不再找到 `LegacyLocationMatchesSurface`。
- `rg` 不再找到 `scope_identity` Review merge fallback。
- `rg` 不再找到 `LegacyAnchorSourceCount`。
- `rg` 不再找到 `AnimationBlueprintBridgeRoutes` / `MaterialBridgeRoutes` / 空壳 TaskRuntime cluster。
- `rollback_cleanup_transaction`、`cleanup_blueprint_helper_block`、`convert_blueprint_helper_block_to_user_owned` 保持无源码命中。
- 编译通过 `TemplateEditor Win64 Development`。
- Review v2 无 explicit targets 的记录不会被 UI 猜测展示到任意 Surface。
- Append/Replace 不再通过 `BlueprintHelper.AgentImportGraph` 命名路径。

## 5. Execution Notes

- 每个任务完成后单独提交。
- 每次提交消息使用用户指定格式，只输出有内容的标题。
- 如果编译因 `UnrealEditor-BlueprintHelper.dll` 被占用失败，先用全局 MCP `blueprint_close_editor(save_all=true)` 关闭编辑器，再重跑同一编译命令。
- 不使用 delay、ActiveTimer retry、Timer retry、AsyncTask-as-delay、geometry retry counter 修 UI 生命周期问题。
- 不恢复旧工具兼容路径。
- 不把旧记录展示兼容作为清理阻塞项。

## 2026-05-20 Execution Status

- [x] Task 1: AnimationBlueprint / Material 空壳 route 和 TaskRuntime cluster 已移除。
- [x] Task 1 编译验证已通过：`TemplateEditor Win64 Development` 构建成功。
- [ ] Task 1 提交未完成：`git commit` 被 `.git/index.lock` 权限阻塞，未生成 commit。
- [x] Task 6 已按新决策删除 `import_json` / `import_agent_graph` 公开入口：Bridge route、validator、router handler 和旧测试期望已清理。
- [x] Task 6 编译验证已通过：`TemplateEditor Win64 Development` 构建成功。
- [x] Task 2: Review v2 legacy surface fallback 已移除，缺少 explicit targets 时不再猜测 Surface。
- [x] Task 3: Review legacy merge collapse key 已移除，不再按 `scope_identity` 或旧 location key 折叠不同 ChangeId。
- [x] Task 4: Graph bounds legacy anchor source / legacy 统计字段已移除，缺少 structured anchor 时输出 none。
- [x] Task 5: GraphWrite Append/Replace `BlueprintHelper.AgentImportGraph` 兼容 payload 已移除，改为 semantic payload DTO。
- [x] Task 2-5 静态检查已通过：Review legacy fallback、GraphWrite AgentImport payload 均无残留命中。
- [x] Task 2-5 编译验证已通过：`TemplateEditor Win64 Development` 构建成功。

Distance from expectation:

1. Task 1 的代码和编译状态已达到期望，但提交需要用户手动处理 `.git/index.lock` 权限问题后执行。
2. Task 6 的代码和编译状态已达到期望；提交仍受 Task 1 同一 `.git/index.lock` 权限问题影响，需要用户手动提交。
3. Task 8-9 尚未完成：GraphWrite node factory seam、最终全量静态检查和计划收尾状态仍待执行。Task 7 已完成代码/静态检查，未在本 worker 执行 UE 编译。

## 2026-05-20 Final Execution Status

- [x] Task 1: AnimationBlueprint / Material 空壳 route 和 TaskRuntime cluster 已移除。
- [x] Task 2: Review v2 legacy surface fallback 已移除。
- [x] Task 3: Review legacy merge collapse key 已移除。
- [x] Task 4: Graph bounds legacy anchor source / legacy 统计字段已移除。
- [x] Task 5: GraphWrite Append/Replace `BlueprintHelper.AgentImportGraph` 兼容 payload 已移除，已改为 semantic payload DTO。
- [x] Task 6: `import_json` / `import_agent_graph` 公开 Bridge 入口已直接删除。
- [x] Task 6 扩展清理：`AgentImportService` / `AgentImportJsonParser` / `AgentImportSemanticExecutor` 内部旧链路已删除，因为它同样属于 v0.1.0 import 设计。
- [x] Task 7: CleanupOwnership 空目录和旧公开命令脚手架已移除，静态回归检查已补充。
- [x] Task 8: GraphWrite node factory seam 已建立，Branch / Sequence / CustomEvent 三个 handler 已迁移到统一 factory。
- [x] Task 9: 旧 import 链路、Review legacy fallback、GraphWrite AgentImport payload、空壳 route/cluster、CleanupOwnership 目录、首批直接 K2Node 创建点的静态检查已通过。
- [x] Task 9: `TemplateEditor Win64 Development` 编译已通过。

Distance from expectation:

1. 本轮计划内旧实现残留清理已完成。
2. CleanupOwnership 旧命令字符串仅保留在 `BlueprintHelperSafetyTests.cpp` 的静态回归 needle 中，用于防止旧公开命令重新出现。
3. GraphWrite node factory seam 只迁移了计划要求的 Branch / Sequence / CustomEvent 三个 handler；其他 handler 的直接创建迁移属于后续扩展，不属于本轮完成标准。
4. Git 提交未执行；按仓库规则需要用户手动提交。
