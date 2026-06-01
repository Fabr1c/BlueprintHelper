# BlueprintHelper ReviewPanel Paged Scroll Pending Load Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 ReviewPanel 的 async pending load 完成后全量加载改为分页式 Scroll 加载，避免一次性把所有 pending ReviewEvent 应用到 GameThread UI/model。

**Architecture:** Pending load worker 只返回一页轻量 Review visible change summary；分页游标、已加载集合、是否还有下一页、in-flight 状态由可复用 model/coordinator 管理。UI 只绑定 `STreeView` scroll 事件并转发“接近底部，加载下一页”的 intent，不负责持久化、全量状态解释或异步生命周期协调。

**Tech Stack:** Unreal Engine 5.6 C++、Slate `STreeView`、Review v2、pending index、ReviewPanel presenter/model/coordinator、UE Automation Tests。

**Project rule override:** 本仓库 AGENTS 规则禁止 Codex 自动执行 `git add`、`git commit`、`git push`；本计划的 checkpoint 只记录建议提交范围，由用户手动提交。

---

## 0. Current Source Facts

- `FBlueprintHelperReviewPendingLoadCoordinator::RequestLoad()` 当前在 worker 中构造 `FBlueprintHelperReviewPendingLoadResult::Changes`，full reload 时调用 `Store->LoadPendingVisibleChanges()`，仍会返回全部 pending visible changes。
- `SBlueprintHelperReviewPanel::HandlePendingReviewLoadCompleted()` 当前在 GameThread 回调中对 `NextChanges` 计算全量 signature，然后调用 `ApplyVisibleChangesFromPendingLoad()`、`RefreshChangeTreeWidget()`、`LoadReviewAssetFromSelection()`、`RefreshMainWorkspaceAfterReviewStateChanged()`。
- `ApplyVisibleChangesFromPendingLoad()` 在 full reload 分支直接调用 `RefreshVisibleChanges(NextChanges)`；`RefreshVisibleChanges()` 清空并重建 `ChangeItems`、ReviewPanel state、tree item、row highlight。
- `RefreshChangeTreeWidget()` 当前会 `RequestTreeRefresh()` 后递归展开所有 root/child。即使 Slate 只虚拟化生成可见 row，当前数据源和 tree model 仍是全量。
- `FBlueprintHelperReviewPendingIndexService::QueryPendingVisibleChanges()` 当前只有 filter，没有 page size / cursor；查询后对全部匹配项排序并返回。
- UE 5.6 `STreeView` 支持 `.OnTreeViewScrolled(FOnTableViewScrolled)`，回调参数是 item scroll offset；`STableViewBase` 公开 `GetScrollOffset()` 和 `GetNumGeneratedChildren()`，可以实现接近底部时事件驱动加载，不需要 `ActiveTimer` 或延迟一帧 workaround。

## 1. Non-Negotiable Boundaries

- 不修改 TaskSpec、GraphWrite、GraphLayout、TaskRun 的职责边界。
- 不恢复 Review v1、Transaction、legacy fallback。
- 不在 worker 线程访问 `UObject*`、Blueprint editor、Graph、Slate widget。
- 不用 `ActiveTimer`、sleep、延迟一帧、retry loop 解决 UI 生命周期顺序；scroll 加载必须由明确的 Slate scroll event 或用户按钮 intent 触发。
- 不让 `SBlueprintHelperReviewPanel` 持久化 page/cursor 业务状态；它只转发 event 并消费 model snapshot。
- Layout diff / Review diff 边界不变；该改动只影响 ReviewPanel pending 数据加载和 UI 呈现性能。
- pending load 完成后不得再全量 `LoadPendingVisibleChanges()` 并一次性 `RefreshVisibleChanges(all)`。

---

## 2. File Structure

### Create

| File | Responsibility |
|---|---|
| `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPagedChangeModel.h` | ReviewPanel 分页 UI model：保存已加载 changes、cursor、total、has more、in-flight、scroll threshold 判断。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPagedChangeModel.cpp` | 实现 reset page、append page、incremental merge/remove、dedupe、selection 保留所需查询。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPagedChangeModelTests.cpp` | 验证分页 model 行为和 scroll threshold 判断。 |

### Modify

| File | Responsibility |
|---|---|
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewPendingIndex.h` | 新增 page cursor、page request/result DTO。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewPendingIndexService.h` | 新增 `QueryPendingVisibleChangePage()`；保留现有全量 query 供非 UI 调用和兼容测试。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewPendingIndexService.cpp` | 在 pending index 查询后按稳定排序应用 cursor/page size；返回 `bHasMore` 和 `NextCursor`。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPendingLoadCoordinator.h` | Request/Result 增加分页模式、page size、cursor、page metadata。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPendingLoadCoordinator.cpp` | full reload 不再返回全量 changes；reset/append page 只查一页；changed event 继续按 affected ids 增量查询。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/UI/BlueprintHelperUiSettings.h` | 增加 Review pending page size、scroll prefetch rows 设置。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/BlueprintHelperUiSettingsResolver.cpp` | 读取并 clamp 新设置。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/BlueprintHelperSettingsPresenter.cpp` | 在 Review 性能设置区加入中文 label/tip。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/SBlueprintHelperReviewPanel.h` | 持有 `FBlueprintHelperReviewPagedChangeModel`，增加 scroll/request/apply page 方法。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanelLayout.cpp` | `STreeView` 绑定 `.OnTreeViewScrolled()`；sidebar footer 显示加载状态和“加载更多”按钮。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp` | pending load completion 改为 apply page；scroll intent 触发 next page；移除 full reload 全量 apply 路径。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPendingIndexTests.cpp` | 增加 cursor/page 查询测试。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp` | 增加 pending load coordinator/reset page/append page 行为测试，或拆到新测试文件。 |

### Do Not Modify

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/*`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/*`
- `AgentFaceService/*`
- `CodexPlugin/*`
- `ClaudePlugin/*`

---

## Task 1: Pending Index Page Query

**Goal:** pending index 支持稳定 cursor 分页，ReviewPanel 可以只请求第一页或下一页。

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewPendingIndex.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewPendingIndexService.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewPendingIndexService.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPendingIndexTests.cpp`

- [ ] **Step 1: Write the failing page query test**

Append to `BlueprintHelperReviewPendingIndexTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingIndexQueryPageUsesStableCursorTest,
	"BlueprintHelper.Review.PendingIndex.QueryPageUsesStableCursor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingIndexQueryPageUsesStableCursorTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString ArchiveId = BlueprintHelperReviewPendingIndexTests::MakeUniqueArchiveId(TEXT("archive_pending_index_page"));
	const FString AssetPath = BlueprintHelperReviewPendingIndexTests::MakeUniqueAssetPath(TEXT("BP_PendingIndexPage"));
	const FString TaskRunId = TEXT("task_pending_index_page");
	TArray<FString> ReviewRecordIds;

	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FString ChangeId = FString::Printf(TEXT("tx_pending_index_page_%02d"), Index);
		FBlueprintHelperReviewRecord Record = BlueprintHelperReviewPendingIndexTests::MakeRecord(
			ArchiveId,
			AssetPath,
			TaskRunId,
			ChangeId);
		Record.ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(
			ArchiveId + FString::Printf(TEXT("_%02d"), Index),
			AssetPath);
		Record.VisibleChanges[0].ChangeId = ChangeId;
		Record.VisibleChanges[0].LatestEvidenceId = ChangeId;
		Record.VisibleChanges[0].DisplayLabel = ChangeId;
		ReviewRecordIds.Add(Record.ReviewRecordId);

		FString SaveError;
		TestTrue(TEXT("record saves before page query"), Store.SaveReviewRecord(Record, SaveError));
	}

	FBlueprintHelperReviewPendingIndexQuery Query;
	Query.AssetPathFilter = AssetPath;
	Query.TaskRunIdFilter = TaskRunId;
	Query.bSkipMissingAssetRecords = false;

	FBlueprintHelperReviewPendingIndexPageRequest FirstRequest;
	FirstRequest.Query = Query;
	FirstRequest.PageSize = 2;

	FBlueprintHelperReviewPendingIndexPage FirstPage;
	FString Error;
	FBlueprintHelperReviewPendingIndexService IndexService;
	TestTrue(TEXT("first page query succeeds"), IndexService.QueryPendingVisibleChangePage(FirstRequest, FirstPage, Error));
	TestEqual(TEXT("first page returns requested page size"), FirstPage.Changes.Num(), 2);
	TestTrue(TEXT("first page reports more rows"), FirstPage.bHasMore);
	TestTrue(TEXT("first page emits cursor"), FirstPage.NextCursor.IsSet());
	TestEqual(TEXT("first page total count is all matching rows"), FirstPage.TotalMatchingCount, 5);

	FBlueprintHelperReviewPendingIndexPageRequest SecondRequest = FirstRequest;
	SecondRequest.Cursor = FirstPage.NextCursor;
	FBlueprintHelperReviewPendingIndexPage SecondPage;
	TestTrue(TEXT("second page query succeeds"), IndexService.QueryPendingVisibleChangePage(SecondRequest, SecondPage, Error));
	TestEqual(TEXT("second page returns requested page size"), SecondPage.Changes.Num(), 2);
	TestNotEqual(TEXT("second page starts after first page cursor"),
		SecondPage.Changes[0].Change.ChangeId,
		FirstPage.Changes[0].Change.ChangeId);
	TestTrue(TEXT("second page still reports more rows"), SecondPage.bHasMore);

	FBlueprintHelperReviewPendingIndexPageRequest ThirdRequest = FirstRequest;
	ThirdRequest.Cursor = SecondPage.NextCursor;
	FBlueprintHelperReviewPendingIndexPage ThirdPage;
	TestTrue(TEXT("third page query succeeds"), IndexService.QueryPendingVisibleChangePage(ThirdRequest, ThirdPage, Error));
	TestEqual(TEXT("third page returns remaining row"), ThirdPage.Changes.Num(), 1);
	TestFalse(TEXT("third page has no more rows"), ThirdPage.bHasMore);

	for (const FString& ReviewRecordId : ReviewRecordIds)
	{
		FString DeleteError;
		Store.DeleteReviewRecord(ReviewRecordId, DeleteError);
	}
	return true;
}
```

- [ ] **Step 2: Run the failing test**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex
```

Then run the automation test in editor/automation context:

```text
Automation RunTests BlueprintHelper.Review.PendingIndex.QueryPageUsesStableCursor
```

Expected before implementation: compile fails because `FBlueprintHelperReviewPendingIndexPageRequest`, `FBlueprintHelperReviewPendingIndexPage`, and `QueryPendingVisibleChangePage()` do not exist.

- [ ] **Step 3: Add pending index page DTOs**

Add to `BlueprintHelperReviewPendingIndex.h`:

```cpp
struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingIndexPageCursor
{
	FString SortKey;
	FString ReviewRecordId;
	FString ChangeId;

	bool IsSet() const
	{
		return !SortKey.IsEmpty() || !ReviewRecordId.IsEmpty() || !ChangeId.IsEmpty();
	}
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingIndexPageRequest
{
	FBlueprintHelperReviewPendingIndexQuery Query;
	FBlueprintHelperReviewPendingIndexPageCursor Cursor;
	int32 PageSize = 100;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingIndexPage
{
	TArray<FBlueprintHelperReviewPendingVisibleChangeSummary> Changes;
	int32 TotalMatchingCount = 0;
	bool bHasMore = false;
	FBlueprintHelperReviewPendingIndexPageCursor NextCursor;
};
```

- [ ] **Step 4: Add the service method declaration**

Add to `FBlueprintHelperReviewPendingIndexService`:

```cpp
bool QueryPendingVisibleChangePage(
	const FBlueprintHelperReviewPendingIndexPageRequest& Request,
	FBlueprintHelperReviewPendingIndexPage& OutPage,
	FString& OutError) const;
```

- [ ] **Step 5: Implement stable page slicing**

In `BlueprintHelperReviewPendingIndexService.cpp`, after the existing `QueryPendingVisibleChanges()` implementation, add:

```cpp
namespace
{
	static int32 ComparePendingSummaryForPage(
		const FBlueprintHelperReviewPendingVisibleChangeSummary& Left,
		const FBlueprintHelperReviewPendingIndexPageCursor& Cursor)
	{
		if (Left.SortKey != Cursor.SortKey)
		{
			return Left.SortKey < Cursor.SortKey ? -1 : 1;
		}
		if (Left.ReviewRecordId != Cursor.ReviewRecordId)
		{
			return Left.ReviewRecordId < Cursor.ReviewRecordId ? -1 : 1;
		}
		if (Left.Change.ChangeId != Cursor.ChangeId)
		{
			return Left.Change.ChangeId < Cursor.ChangeId ? -1 : 1;
		}
		return 0;
	}

	static FBlueprintHelperReviewPendingIndexPageCursor MakePendingPageCursor(
		const FBlueprintHelperReviewPendingVisibleChangeSummary& Summary)
	{
		FBlueprintHelperReviewPendingIndexPageCursor Cursor;
		Cursor.SortKey = Summary.SortKey;
		Cursor.ReviewRecordId = Summary.ReviewRecordId;
		Cursor.ChangeId = Summary.Change.ChangeId;
		return Cursor;
	}
}

bool FBlueprintHelperReviewPendingIndexService::QueryPendingVisibleChangePage(
	const FBlueprintHelperReviewPendingIndexPageRequest& Request,
	FBlueprintHelperReviewPendingIndexPage& OutPage,
	FString& OutError) const
{
	OutPage = FBlueprintHelperReviewPendingIndexPage();

	TArray<FBlueprintHelperReviewPendingVisibleChangeSummary> AllMatches;
	if (!QueryPendingVisibleChanges(Request.Query, AllMatches, OutError))
	{
		return false;
	}

	OutPage.TotalMatchingCount = AllMatches.Num();
	const int32 ClampedPageSize = FMath::Clamp(Request.PageSize, 1, 1000);
	int32 StartIndex = 0;
	if (Request.Cursor.IsSet())
	{
		while (StartIndex < AllMatches.Num()
			&& ComparePendingSummaryForPage(AllMatches[StartIndex], Request.Cursor) <= 0)
		{
			++StartIndex;
		}
	}

	for (int32 Index = StartIndex; Index < AllMatches.Num() && OutPage.Changes.Num() < ClampedPageSize; ++Index)
	{
		OutPage.Changes.Add(AllMatches[Index]);
	}

	const int32 NextIndex = StartIndex + OutPage.Changes.Num();
	OutPage.bHasMore = NextIndex < AllMatches.Num();
	if (OutPage.Changes.Num() > 0)
	{
		OutPage.NextCursor = MakePendingPageCursor(OutPage.Changes.Last());
	}
	OutError.Reset();
	return true;
}
```

This first implementation still loads/sorts the lightweight index in memory, but it prevents UI and pending coordinator from hydrating/applying all visible changes at once. A later optimization can cache sorted index pages if the index itself becomes too large.

- [ ] **Step 6: Verify green**

Run:

```text
Automation RunTests BlueprintHelper.Review.PendingIndex
```

Expected: all pending index tests pass, including `QueryPageUsesStableCursor`.

---

## Task 2: Pending Load Coordinator Returns Pages

**Goal:** async pending load 的 full reload/reset path 只返回第一页；scroll append path 只返回下一页；changed event path 继续返回 affected changes。

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPendingLoadCoordinator.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPendingLoadCoordinator.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Write the failing coordinator test**

Append this test helper and test to `BlueprintHelperReviewStoreServiceTests.cpp` or a new `BlueprintHelperReviewPendingLoadCoordinatorTests.cpp` included in the same module:

```cpp
namespace BlueprintHelperReviewPendingLoadCoordinatorTests
{
	struct FPendingLoadLatentState
	{
		bool bDone = false;
		double StartSeconds = FPlatformTime::Seconds();
		FBlueprintHelperReviewPendingLoadResult Result;
	};

	static FBlueprintHelperReviewVisibleChange MakeVisibleChange(
		const FString& ChangeId,
		const FString& AssetPath)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = ChangeId;
		Change.AssetPath = AssetPath;
		Change.GraphName = TEXT("EventGraph");
		Change.LocationKey = TEXT("graph_node:") + ChangeId;
		Change.DisplayLabel = ChangeId;
		Change.LatestEvidenceId = ChangeId;
		Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
		return Change;
	}

	static FBlueprintHelperReviewRecord MakeRecord(
		const FString& ArchiveId,
		const FString& AssetPath,
		const FString& TaskRunId,
		const FString& ChangeId)
	{
		FBlueprintHelperReviewRecord Record;
		Record.ReviewRecordId = FBlueprintHelperReviewStoreService::MakeReviewRecordId(ArchiveId, AssetPath);
		Record.ArchiveSessionId = ArchiveId;
		Record.AssetPath = AssetPath;
		Record.SourceTaskRunIds.Add(TaskRunId);
		Record.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Record.StorageStatus = EBlueprintHelperReviewStorageStatus::Active;
		Record.VisibleChanges.Add(MakeVisibleChange(ChangeId, AssetPath));
		return Record;
	}
}

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
	FWaitForBlueprintHelperPendingLoadResult,
	TSharedPtr<BlueprintHelperReviewPendingLoadCoordinatorTests::FPendingLoadLatentState>,
	State,
	FAutomationTestBase*,
	Test)

bool FWaitForBlueprintHelperPendingLoadResult::Update()
{
	if (State->bDone)
	{
		return true;
	}
	if ((FPlatformTime::Seconds() - State->StartSeconds) > 5.0)
	{
		Test->AddError(TEXT("pending load coordinator did not complete within timeout"));
		return true;
	}
	return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPendingLoadFullReloadReturnsFirstPageOnlyTest,
	"BlueprintHelper.Review.PendingLoad.FullReloadReturnsFirstPageOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPendingLoadFullReloadReturnsFirstPageOnlyTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewStoreService Store;
	const FString AssetPath = TEXT("/Game/BlueprintHelperReview/BP_PendingLoadPage");
	const FString TaskRunId = TEXT("task_pending_load_page");
	TArray<FString> ReviewRecordIds;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FString ArchiveId = FString::Printf(TEXT("archive_pending_load_page_%02d_%s"),
			Index,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString ChangeId = FString::Printf(TEXT("tx_pending_load_page_%02d"), Index);
		FBlueprintHelperReviewRecord Record =
			BlueprintHelperReviewPendingLoadCoordinatorTests::MakeRecord(
				ArchiveId,
				AssetPath,
				TaskRunId,
				ChangeId);
		ReviewRecordIds.Add(Record.ReviewRecordId);

		FString SaveError;
		TestTrue(TEXT("record saves before coordinator load"), Store.SaveReviewRecord(Record, SaveError));
	}

	FBlueprintHelperReviewPerformanceSettings Settings;
	Settings.bValiditySweepEnabled = false;
	FBlueprintHelperReviewPendingLoadCoordinator Coordinator(&Store, Settings);
	FBlueprintHelperReviewPendingLoadRequest Request;
	Request.Mode = EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage;
	Request.PageSize = 2;
	Request.Source = TEXT("test_full_reload_page");
	Request.SourceEvent = FBlueprintHelperReviewStoreChangedEvent::FullReload();

	TSharedPtr<BlueprintHelperReviewPendingLoadCoordinatorTests::FPendingLoadLatentState> State =
		MakeShared<BlueprintHelperReviewPendingLoadCoordinatorTests::FPendingLoadLatentState>();
	Coordinator.RequestLoad(
		Request,
		FBlueprintHelperReviewPendingLoadCompleted::CreateLambda(
			[State](const FBlueprintHelperReviewPendingLoadResult& Result)
			{
				State->Result = Result;
				State->bDone = true;
			}));

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForBlueprintHelperPendingLoadResult(State, this));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, ReviewRecordIds, &Store]()
	{
		TestTrue(TEXT("coordinator result succeeds"), State->Result.bSucceeded);
		TestEqual(TEXT("full reload returns first page only"), State->Result.Changes.Num(), 2);
		TestEqual(TEXT("full reload total count reports all rows"), State->Result.TotalMatchingCount, 5);
		TestTrue(TEXT("full reload reports more pages"), State->Result.bHasMore);
		TestTrue(TEXT("full reload emits next cursor"), State->Result.NextCursor.IsSet());
		for (const FString& ReviewRecordId : ReviewRecordIds)
		{
			FString DeleteError;
			Store.DeleteReviewRecord(ReviewRecordId, DeleteError);
		}
		return true;
	}));
	return true;
}
```

Expected before implementation: compile fails because paged load mode/metadata do not exist. If the DTOs were added without changing loading behavior, the test fails because full reload returns all pending changes rather than 2.

- [ ] **Step 2: Add load mode and page metadata**

Add to `BlueprintHelperReviewPendingLoadCoordinator.h`:

```cpp
enum class EBlueprintHelperReviewPendingLoadMode : uint8
{
	ResetToFirstPage,
	AppendNextPage,
	RefreshChanged
};
```

Extend `FBlueprintHelperReviewPendingLoadRequest`:

```cpp
EBlueprintHelperReviewPendingLoadMode Mode = EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage;
FBlueprintHelperReviewPendingIndexPageCursor Cursor;
int32 PageSize = 100;
```

Extend `FBlueprintHelperReviewPendingLoadResult`:

```cpp
EBlueprintHelperReviewPendingLoadMode Mode = EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage;
FBlueprintHelperReviewPendingIndexPageCursor NextCursor;
int32 TotalMatchingCount = 0;
bool bHasMore = false;
```

- [ ] **Step 3: Replace full reload loading with page loading**

In `BlueprintHelperReviewPendingLoadCoordinator.cpp`, replace the full reload branch in `LoadChangedVisibleChanges()` with a page query helper:

```cpp
static FBlueprintHelperReviewPendingIndexPage LoadPendingVisibleChangePage(
	const FBlueprintHelperReviewStoreService* Store,
	const FBlueprintHelperReviewPendingLoadRequest& Request)
{
	FBlueprintHelperReviewPendingIndexPage Page;
	if (!Store)
	{
		return Page;
	}

	FBlueprintHelperReviewPendingIndexQuery Query;
	Query.AssetPathFilter = Request.AssetPathFilter;
	Query.bPendingOnly = true;
	Query.bSkipMissingAssetRecords = Request.AssetPathFilter.IsEmpty();

	FBlueprintHelperReviewPendingIndexPageRequest PageRequest;
	PageRequest.Query = Query;
	PageRequest.Cursor = Request.Cursor;
	PageRequest.PageSize = Request.PageSize;

	FString Error;
	FBlueprintHelperReviewPendingIndexService IndexService;
	if (!IndexService.QueryPendingVisibleChangePage(PageRequest, Page, Error))
	{
		Page = FBlueprintHelperReviewPendingIndexPage();
	}
	return Page;
}
```

When `Mode` is `ResetToFirstPage` or `AppendNextPage`, fill `Result.Changes` from `Page.Changes`, set `Result.NextCursor`, `Result.TotalMatchingCount`, `Result.bHasMore`, and never call `Store->LoadPendingVisibleChanges()`.

When `Mode` is `RefreshChanged`, keep the current changed-event path so a single Accept/Reject can update/remove affected ids without requiring scroll pagination.

- [ ] **Step 4: Keep validity candidates bounded**

For reset/append page requests, validity candidates should be built from the same page or remain bounded by existing `PendingLoadValidityCandidateBudget`. Do not scan all summaries on every append. Preferred first version:

```cpp
Result.ValidityCandidates = BlueprintHelperReviewPendingLoad::BuildValidityCandidatesFromSummaries(
	Result.Changes,
	SharedState->ReviewPerformanceSettings);
```

If this helper cannot be added cleanly in Task 2, keep existing budgeted validity scan only on `ResetToFirstPage` and skip it on `AppendNextPage`.

- [ ] **Step 5: Verify green**

Run:

```text
Automation RunTests BlueprintHelper.Review.PendingLoad
Automation RunTests BlueprintHelper.Review.PendingIndex
```

Expected: full reload page test proves coordinator does not return all pending rows.

---

## Task 3: Paged Change Model

**Goal:** 将 loaded pages、cursor、has-more、in-flight、dedupe、scroll threshold 判断从 Widget 挪到独立 model。

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPagedChangeModel.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPagedChangeModel.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPagedChangeModelTests.cpp`

- [ ] **Step 1: Write failing model tests**

Create `BlueprintHelperReviewPagedChangeModelTests.cpp` with tests:

```cpp
namespace BlueprintHelperReviewPagedChangeModelTests
{
	static FBlueprintHelperReviewVisibleChange MakeVisibleChange(
		const FString& ChangeId,
		const FString& AssetPath,
		const FString& TargetKey)
	{
		FBlueprintHelperReviewVisibleChange Change;
		Change.ChangeId = ChangeId;
		Change.AssetPath = AssetPath;
		Change.GraphName = TEXT("EventGraph");
		Change.LocationKey = TargetKey;
		Change.LatestEvidenceId = ChangeId;
		Change.ChangeKind = EBlueprintHelperReviewChangeKind::Modified;
		Change.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Change.DisplayLabel = TargetKey;
		return Change;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPagedChangeModelAppendDedupesTest,
	"BlueprintHelper.Review.PagedChangeModel.AppendDedupes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPagedChangeModelAppendDedupesTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewPagedChangeModel Model;

	FBlueprintHelperReviewPendingLoadResult FirstPage;
	FirstPage.Mode = EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage;
	FirstPage.Changes.Add(BlueprintHelperReviewPagedChangeModelTests::MakeVisibleChange(
		TEXT("change_a"),
		TEXT("/Game/BP_PageModel"),
		TEXT("graph_node:A")));
	FirstPage.Changes.Add(BlueprintHelperReviewPagedChangeModelTests::MakeVisibleChange(
		TEXT("change_b"),
		TEXT("/Game/BP_PageModel"),
		TEXT("graph_node:B")));
	FirstPage.TotalMatchingCount = 3;
	FirstPage.bHasMore = true;

	Model.ApplyPendingLoadResult(FirstPage);
	TestEqual(TEXT("first page loads two changes"), Model.GetLoadedChanges().Num(), 2);
	TestTrue(TEXT("model reports more pages"), Model.HasMorePages());

	FBlueprintHelperReviewPendingLoadResult SecondPage;
	SecondPage.Mode = EBlueprintHelperReviewPendingLoadMode::AppendNextPage;
	SecondPage.Changes.Add(FirstPage.Changes[1]);
	SecondPage.Changes.Add(BlueprintHelperReviewPagedChangeModelTests::MakeVisibleChange(
		TEXT("change_c"),
		TEXT("/Game/BP_PageModel"),
		TEXT("graph_node:C")));
	SecondPage.TotalMatchingCount = 3;
	SecondPage.bHasMore = false;

	Model.ApplyPendingLoadResult(SecondPage);
	TestEqual(TEXT("append page dedupes existing change"), Model.GetLoadedChanges().Num(), 3);
	TestFalse(TEXT("model reports no more pages"), Model.HasMorePages());
	return true;
}
```

Add a second pure behavior test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperReviewPagedChangeModelScrollNearEndRequestsNextPageTest,
	"BlueprintHelper.Review.PagedChangeModel.ScrollNearEndRequestsNextPage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperReviewPagedChangeModelScrollNearEndRequestsNextPageTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperReviewPagedChangeModel Model;

	FBlueprintHelperReviewPendingLoadResult FirstPage;
	FirstPage.Mode = EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage;
	FirstPage.TotalMatchingCount = 200;
	FirstPage.bHasMore = true;
	for (int32 Index = 0; Index < 100; ++Index)
	{
		FirstPage.Changes.Add(BlueprintHelperReviewPagedChangeModelTests::MakeVisibleChange(
			FString::Printf(TEXT("change_%03d"), Index),
			TEXT("/Game/BP_PageModel"),
			FString::Printf(TEXT("graph_node:%03d"), Index)));
	}
	Model.ApplyPendingLoadResult(FirstPage);

	TestFalse(TEXT("far from end does not request next page"),
		Model.ShouldRequestNextPage(/*ScrollOffset*/ 10.0, /*GeneratedRows*/ 20, /*LoadedRows*/ 100, /*PrefetchRows*/ 24));
	TestTrue(TEXT("near end requests next page"),
		Model.ShouldRequestNextPage(/*ScrollOffset*/ 60.0, /*GeneratedRows*/ 20, /*LoadedRows*/ 100, /*PrefetchRows*/ 24));

	Model.MarkPageRequestStarted();
	TestFalse(TEXT("no request when already loading"),
		Model.ShouldRequestNextPage(/*ScrollOffset*/ 80.0, /*GeneratedRows*/ 20, /*LoadedRows*/ 100, /*PrefetchRows*/ 24));
	return true;
}
```

- [ ] **Step 2: Add the model header**

Create `BlueprintHelperReviewPagedChangeModel.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UI/Review/BlueprintHelperReviewPendingLoadCoordinator.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewPagedChangeModel
{
public:
	void Reset();
	void MarkPageRequestStarted();
	void MarkPageRequestFinished();
	void ApplyPendingLoadResult(const FBlueprintHelperReviewPendingLoadResult& Result);
	bool ShouldRequestNextPage(
		double ScrollOffset,
		int32 GeneratedRowCount,
		int32 LoadedFlatRowCount,
		int32 PrefetchRows) const;

	const TArray<FBlueprintHelperReviewVisibleChange>& GetLoadedChanges() const;
	bool HasMorePages() const;
	bool IsPageRequestInFlight() const;
	int32 GetTotalMatchingCount() const;
	const FBlueprintHelperReviewPendingIndexPageCursor& GetNextCursor() const;

private:
	TArray<FBlueprintHelperReviewVisibleChange> LoadedChanges;
	TSet<FString> LoadedChangeIds;
	FBlueprintHelperReviewPendingIndexPageCursor NextCursor;
	int32 TotalMatchingCount = 0;
	bool bHasMorePages = false;
	bool bPageRequestInFlight = false;
};
```

- [ ] **Step 3: Implement the model**

Create `BlueprintHelperReviewPagedChangeModel.cpp`:

```cpp
#include "UI/Review/BlueprintHelperReviewPagedChangeModel.h"

void FBlueprintHelperReviewPagedChangeModel::Reset()
{
	LoadedChanges.Reset();
	LoadedChangeIds.Reset();
	NextCursor = FBlueprintHelperReviewPendingIndexPageCursor();
	TotalMatchingCount = 0;
	bHasMorePages = false;
	bPageRequestInFlight = false;
}

void FBlueprintHelperReviewPagedChangeModel::MarkPageRequestStarted()
{
	bPageRequestInFlight = true;
}

void FBlueprintHelperReviewPagedChangeModel::MarkPageRequestFinished()
{
	bPageRequestInFlight = false;
}

void FBlueprintHelperReviewPagedChangeModel::ApplyPendingLoadResult(
	const FBlueprintHelperReviewPendingLoadResult& Result)
{
	bPageRequestInFlight = false;
	if (Result.Mode == EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage)
	{
		LoadedChanges.Reset();
		LoadedChangeIds.Reset();
	}

	for (const FBlueprintHelperReviewVisibleChange& Change : Result.Changes)
	{
		if (!Change.ChangeId.IsEmpty() && LoadedChangeIds.Contains(Change.ChangeId))
		{
			continue;
		}
		if (!Change.ChangeId.IsEmpty())
		{
			LoadedChangeIds.Add(Change.ChangeId);
		}
		LoadedChanges.Add(Change);
	}

	NextCursor = Result.NextCursor;
	TotalMatchingCount = Result.TotalMatchingCount;
	bHasMorePages = Result.bHasMore;
}

bool FBlueprintHelperReviewPagedChangeModel::ShouldRequestNextPage(
	double ScrollOffset,
	int32 GeneratedRowCount,
	int32 LoadedFlatRowCount,
	int32 PrefetchRows) const
{
	if (bPageRequestInFlight || !bHasMorePages || LoadedFlatRowCount <= 0)
	{
		return false;
	}
	return ScrollOffset + GeneratedRowCount + FMath::Max(0, PrefetchRows) >= LoadedFlatRowCount;
}

const TArray<FBlueprintHelperReviewVisibleChange>& FBlueprintHelperReviewPagedChangeModel::GetLoadedChanges() const
{
	return LoadedChanges;
}

bool FBlueprintHelperReviewPagedChangeModel::HasMorePages() const
{
	return bHasMorePages;
}

bool FBlueprintHelperReviewPagedChangeModel::IsPageRequestInFlight() const
{
	return bPageRequestInFlight;
}

int32 FBlueprintHelperReviewPagedChangeModel::GetTotalMatchingCount() const
{
	return TotalMatchingCount;
}

const FBlueprintHelperReviewPendingIndexPageCursor& FBlueprintHelperReviewPagedChangeModel::GetNextCursor() const
{
	return NextCursor;
}
```

- [ ] **Step 4: Verify green**

Run:

```text
Automation RunTests BlueprintHelper.Review.PagedChangeModel
```

Expected: append/dedupe and scroll threshold tests pass.

---

## Task 4: ReviewPanel Scroll Integration

**Goal:** ReviewPanel 初次进入只应用第一页；用户滚动接近底部时请求下一页；GameThread 每次只 append 一页到 UI tree。

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/SBlueprintHelperReviewPanel.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanelLayout.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`

- [ ] **Step 1: Add panel fields and methods**

In `SBlueprintHelperReviewPanel.h`, include:

```cpp
#include "UI/Review/BlueprintHelperReviewPagedChangeModel.h"
```

Add methods:

```cpp
void RequestPendingReviewPage(
	const FString& Reason,
	EBlueprintHelperReviewPendingLoadMode Mode,
	const FBlueprintHelperReviewStoreChangedEvent& SourceEvent =
		FBlueprintHelperReviewStoreChangedEvent::FullReload());
void OnChangeTreeScrolled(double ScrollOffset);
int32 CountLoadedChangeTreeRows() const;
FText GetPendingPageStatusText() const;
FReply OnLoadMorePendingChanges();
```

Add fields:

```cpp
FBlueprintHelperReviewPagedChangeModel PagedChangeModel;
int32 PendingPageSize = 100;
int32 PendingScrollPrefetchRows = 24;
```

- [ ] **Step 2: Load page settings in construct**

In `SBlueprintHelperReviewPanel::Construct()`, after loading `ReviewPerformanceSettings`, set:

```cpp
PendingPageSize = FMath::Max(1, ReviewPerformanceSettings.PendingLoadPageSize);
PendingScrollPrefetchRows = FMath::Max(0, ReviewPerformanceSettings.PendingLoadScrollPrefetchRows);
```

The settings fields are added in Task 5.

- [ ] **Step 3: Bind tree scroll event**

In `BuildFinalChangeSidebar()`, add the scroll binding:

```cpp
SAssignNew(ChangeTreeView, STreeView<FReviewTreeItemPtr>)
	.TreeItemsSource(&ChangeTreeRootItems)
	.SelectionMode(ESelectionMode::Single)
	.OnGenerateRow(this, &SBlueprintHelperReviewPanel::GenerateChangeTreeRow)
	.OnGetChildren(this, &SBlueprintHelperReviewPanel::GetChangeTreeChildren)
	.OnSelectionChanged(this, &SBlueprintHelperReviewPanel::OnChangeTreeSelectionChanged)
	.OnTreeViewScrolled(this, &SBlueprintHelperReviewPanel::OnChangeTreeScrolled)
```

Add a sidebar footer below the tree:

```cpp
+ SVerticalBox::Slot()
.AutoHeight()
.Padding(6.0f, 2.0f)
[
	SNew(STextBlock)
	.Text(this, &SBlueprintHelperReviewPanel::GetPendingPageStatusText)
]
+ SVerticalBox::Slot()
.AutoHeight()
.Padding(6.0f, 2.0f)
[
	SNew(SButton)
	.Text(FText::FromString(TEXT("加载更多")))
	.Visibility_Lambda([this]()
	{
		return PagedChangeModel.HasMorePages() ? EVisibility::Visible : EVisibility::Collapsed;
	})
	.IsEnabled_Lambda([this]()
	{
		return !PagedChangeModel.IsPageRequestInFlight();
	})
	.OnClicked(this, &SBlueprintHelperReviewPanel::OnLoadMorePendingChanges)
]
```

The button is a deterministic fallback for cases where the first page does not fill the viewport and no scroll event fires.

- [ ] **Step 4: Change construct load request**

Replace:

```cpp
RequestPendingReviewLoad(TEXT("construct"));
```

with:

```cpp
RequestPendingReviewPage(
	TEXT("construct"),
	EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage);
```

- [ ] **Step 5: Implement page request**

Add:

```cpp
void SBlueprintHelperReviewPanel::RequestPendingReviewPage(
	const FString& Reason,
	EBlueprintHelperReviewPendingLoadMode Mode,
	const FBlueprintHelperReviewStoreChangedEvent& SourceEvent)
{
	if (!PendingLoadCoordinator.IsValid() || PagedChangeModel.IsPageRequestInFlight())
	{
		return;
	}

	FBlueprintHelperReviewPendingLoadRequest Request;
	Request.Source = Reason;
	Request.Mode = Mode;
	Request.PageSize = PendingPageSize;
	Request.Cursor = Mode == EBlueprintHelperReviewPendingLoadMode::AppendNextPage
		? PagedChangeModel.GetNextCursor()
		: FBlueprintHelperReviewPendingIndexPageCursor();
	Request.SourceEvent = SourceEvent;

	PagedChangeModel.MarkPageRequestStarted();
	PendingLoadCoordinator->RequestLoad(
		Request,
		FBlueprintHelperReviewPendingLoadCompleted::CreateSP(
			this,
			&SBlueprintHelperReviewPanel::HandlePendingReviewLoadCompleted));
}
```

Keep `RequestPendingReviewLoad()` only as a compatibility wrapper that routes to page mode:

```cpp
void SBlueprintHelperReviewPanel::RequestPendingReviewLoad(
	const FString& Reason,
	const FBlueprintHelperReviewStoreChangedEvent& SourceEvent)
{
	const bool bFullReload = SourceEvent.bRequiresFullReload
		|| (SourceEvent.ChangeIds.Num() == 0 && SourceEvent.AssetPaths.Num() == 0);
	RequestPendingReviewPage(
		Reason,
		bFullReload
			? EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage
			: EBlueprintHelperReviewPendingLoadMode::RefreshChanged,
		SourceEvent);
}
```

- [ ] **Step 6: Apply page result instead of full result**

At the start of `HandlePendingReviewLoadCompleted()`, call:

```cpp
PagedChangeModel.MarkPageRequestFinished();
```

Replace:

```cpp
const TArray<FBlueprintHelperReviewVisibleChange> NextChanges =
	BuildVisibleChangesAfterIncrementalLoad(Result);
```

with:

```cpp
PagedChangeModel.ApplyPendingLoadResult(Result);
const TArray<FBlueprintHelperReviewVisibleChange>& NextChanges =
	PagedChangeModel.GetLoadedChanges();
```

For `ResetToFirstPage`, preserve the previous selection if it is still in loaded changes; otherwise select the first loaded row. For `AppendNextPage`, do not call `LoadReviewAssetFromSelection()` unless selection changed.

Required behavior:

```cpp
const bool bSelectionChanged = !SelectedChange.IsValid()
	|| SelectedChange->ChangeId != PreviousSelectedChangeId;
if (bSelectionChanged)
{
	LoadReviewAssetFromSelection();
	RefreshMainWorkspaceAfterReviewStateChanged();
}
```

The implementation may still rebuild `ChangeTreeRootItems` for loaded pages, but it must only use `PagedChangeModel.GetLoadedChanges()`, not all pending rows.

- [ ] **Step 7: Implement scroll trigger**

Add:

```cpp
void SBlueprintHelperReviewPanel::OnChangeTreeScrolled(double ScrollOffset)
{
	if (!ChangeTreeView.IsValid())
	{
		return;
	}

	const int32 GeneratedRows = ChangeTreeView->GetNumGeneratedChildren();
	const int32 LoadedRows = CountLoadedChangeTreeRows();
	if (PagedChangeModel.ShouldRequestNextPage(
		ScrollOffset,
		GeneratedRows,
		LoadedRows,
		PendingScrollPrefetchRows))
	{
		RequestPendingReviewPage(
			TEXT("scroll_append"),
			EBlueprintHelperReviewPendingLoadMode::AppendNextPage);
	}
}
```

Add:

```cpp
int32 SBlueprintHelperReviewPanel::CountLoadedChangeTreeRows() const
{
	int32 Count = 0;
	TArray<FReviewTreeItemPtr> Stack = ChangeTreeRootItems;
	while (Stack.Num() > 0)
	{
		FReviewTreeItemPtr Item = Stack.Pop(EAllowShrinking::No);
		if (!Item.IsValid())
		{
			continue;
		}
		++Count;
		for (const FReviewTreeItemPtr& Child : Item->Children)
		{
			Stack.Add(Child);
		}
	}
	return Count;
}
```

Add:

```cpp
FReply SBlueprintHelperReviewPanel::OnLoadMorePendingChanges()
{
	RequestPendingReviewPage(
		TEXT("manual_load_more"),
		EBlueprintHelperReviewPendingLoadMode::AppendNextPage);
	return FReply::Handled();
}
```

- [ ] **Step 8: Add status text**

Add:

```cpp
FText SBlueprintHelperReviewPanel::GetPendingPageStatusText() const
{
	const int32 Loaded = PagedChangeModel.GetLoadedChanges().Num();
	const int32 Total = PagedChangeModel.GetTotalMatchingCount();
	if (PagedChangeModel.IsPageRequestInFlight())
	{
		return FText::FromString(FString::Printf(TEXT("正在加载 %d / %d"), Loaded, Total));
	}
	if (PagedChangeModel.HasMorePages())
	{
		return FText::FromString(FString::Printf(TEXT("已加载 %d / %d，滚动到底部继续加载"), Loaded, Total));
	}
	return FText::FromString(FString::Printf(TEXT("已加载 %d / %d"), Loaded, Total));
}
```

- [ ] **Step 9: Verify targeted behavior**

Run:

```text
Automation RunTests BlueprintHelper.Review.PagedChangeModel
Automation RunTests BlueprintHelper.Review.Panel
```

Manual editor verification:

1. Prepare many pending ReviewEvents.
2. Open BlueprintHelperWidget and select Review.
3. Confirm the tree initially shows only the first page count.
4. Scroll near the bottom.
5. Confirm the next page appends without long GameThread stall and without reloading the selected asset unless selection changes.

---

## Task 5: User Settings For Page Loading

**Goal:** 页面大小和 scroll prefetch 行数可配置，默认值保守，Tips 全中文。

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/BlueprintHelperUiSettings.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/BlueprintHelperUiSettingsResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/BlueprintHelperSettingsPresenter.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Config/BlueprintHelperSettingStoreTests.cpp`

- [ ] **Step 1: Write failing settings test**

Add to `BlueprintHelperSettingStoreTests.cpp` and include `UI/BlueprintHelperUiSettingsResolver.h` if not already included:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperSettingsReviewPerformancePendingPagingTest,
	"BlueprintHelper.Settings.ReviewPerformancePendingPaging",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperSettingsReviewPerformancePendingPagingTest::RunTest(const FString& Parameters)
{
	FString Error;
	const FScopedBlueprintHelperSettingFileBackup ProjectSettingBackup(
		FBlueprintHelperProjectConfigPaths::GetProjectSettingPath());
	const FScopedBlueprintHelperSettingFileBackup UserSettingBackup(
		FBlueprintHelperProjectConfigPaths::GetUserSettingOverridePath());

	TestTrue(TEXT("project setting fixture writes paging settings"),
		ProjectSettingBackup.Write(
			TEXT("{")
			TEXT("\"review\":{\"performance\":{")
			TEXT("\"pending_load_page_size\":37,")
			TEXT("\"pending_load_scroll_prefetch_rows\":9")
			TEXT("}}")
			TEXT("}"),
			Error));
	TestTrue(TEXT("user setting fixture clears overrides"), UserSettingBackup.Write(TEXT("{}"), Error));

	const FBlueprintHelperReviewPerformanceSettings Settings =
		FBlueprintHelperUiSettingsResolver::LoadReviewPerformanceSettings();
	TestEqual(TEXT("pending page size comes from settings"), Settings.PendingLoadPageSize, 37);
	TestEqual(TEXT("pending scroll prefetch rows comes from settings"),
		Settings.PendingLoadScrollPrefetchRows,
		9);
	return true;
}
```

Expected before implementation: compile fails because `PendingLoadPageSize` and `PendingLoadScrollPrefetchRows` do not exist.

- [ ] **Step 2: Add settings fields**

In `FBlueprintHelperReviewPerformanceSettings`:

```cpp
int32 PendingLoadPageSize = 100;
int32 PendingLoadScrollPrefetchRows = 24;
```

- [ ] **Step 3: Resolve and clamp settings**

In `FBlueprintHelperUiSettingsResolver::LoadReviewPerformanceSettings()`:

```cpp
Settings.PendingLoadPageSize = FMath::Clamp(
	GetInt(TEXT("review.performance.pending_load_page_size"), Settings.PendingLoadPageSize),
	1,
	1000);
Settings.PendingLoadScrollPrefetchRows = FMath::Clamp(
	GetInt(TEXT("review.performance.pending_load_scroll_prefetch_rows"), Settings.PendingLoadScrollPrefetchRows),
	0,
	500);
```

Use the local resolver helper names already present in the file; do not introduce a second settings parser.

- [ ] **Step 4: Add Chinese settings rows**

In `BlueprintHelperSettingsPresenter.cpp`, add rows near existing Review performance settings:

```cpp
LOCTEXT("ReviewPerformancePendingLoadPageSizeLabel", "Pending 分页大小"),
LOCTEXT("ReviewPerformancePendingLoadPageSizeHint", "控制 Review 面板每次从 pending index 加载多少条可见变更。数值越大，滚动次数越少，但单次 GameThread 应用成本越高。")
```

```cpp
LOCTEXT("ReviewPerformancePendingLoadScrollPrefetchRowsLabel", "Pending 滚动预加载行数"),
LOCTEXT("ReviewPerformancePendingLoadScrollPrefetchRowsHint", "控制 Review 变更树距离底部还剩多少行时提前加载下一页。设为 0 表示滚动到当前页末尾才加载。")
```

- [ ] **Step 5: Verify green**

Run:

```text
Automation RunTests BlueprintHelper.Settings
```

---

## Task 6: Remove Full Apply Assumptions And Guard Regressions

**Goal:** 防止未来再次把 ReviewPanel pending load 改回全量 apply。

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPendingLoadCoordinator.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review\BlueprintHelperReviewStoreServiceTests.cpp`

- [ ] **Step 1: Delete or quarantine full-load helper usage**

`BuildVisibleChangesAfterIncrementalLoad()` currently encourages building a full `NextChanges` snapshot. After Task 4, either remove it or narrow it to `RefreshChanged` only:

```cpp
TArray<FBlueprintHelperReviewVisibleChange> BuildVisibleChangesAfterIncrementalLoad(
	const FBlueprintHelperReviewPendingLoadResult& Result) const;
```

must not be called for `ResetToFirstPage` or `AppendNextPage`.

- [ ] **Step 2: Add assertion-style guard**

In `HandlePendingReviewLoadCompleted()`:

```cpp
ensureMsgf(
	Result.Mode != EBlueprintHelperReviewPendingLoadMode::ResetToFirstPage
		|| Result.Changes.Num() <= PendingPageSize,
	TEXT("ReviewPanel reset pending load returned more rows than page size."));
```

This is a guard, not the primary correctness mechanism.

- [ ] **Step 3: Add regression test**

Add test:

```cpp
"BlueprintHelper.Review.PendingLoad.ResetDoesNotReturnAllPendingChanges"
```

It should create more records than page size, request reset load, and assert:

```cpp
TestEqual(TEXT("reset result is exactly page size"), Result.Changes.Num(), PageSize);
TestTrue(TEXT("reset result reports more pages"), Result.bHasMore);
TestEqual(TEXT("reset result reports full total"), Result.TotalMatchingCount, TotalCreated);
```

- [ ] **Step 4: Verify broad Review tests**

Run:

```text
Automation RunTests BlueprintHelper.Review.PendingIndex
Automation RunTests BlueprintHelper.Review.PagedChangeModel
Automation RunTests BlueprintHelper.Review.PendingLoad
Automation RunTests BlueprintHelper.Review.Panel
Automation RunTests BlueprintHelper.Review
```

Then run compile:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex
```

---

## 3. Execution Notes

- Suggested implementation order: Task 1 -> Task 2 -> Task 3 -> Task 5 -> Task 4 -> Task 6.
- Task 1 and Task 3 are safest to assign to small code workers because their write sets are isolated.
- Task 2 and Task 4 should be handled by one integration owner or carefully sequenced workers because request/result semantics must match.
- Final audit should be a large read-only review focused on these risks:
  - `RequestPendingReviewLoad("construct")` no longer returns all rows.
  - `HandlePendingReviewLoadCompleted()` no longer computes full signatures over all pending rows.
  - Scroll path cannot issue duplicate append requests while one request is in flight.
  - Selection changes still load asset context correctly.
  - Accept/Reject incremental changed events still update loaded rows even when the affected row is not in the current loaded page.
  - No timer/delay workaround was introduced.

## 4. Execution Result (2026-06-01)

- Status: 已完成计划内实现，并额外修复最终审计发现的分页后批量 Accept/Reject 语义回归。
- Pending index 已增加稳定 cursor/page DTO 和 `QueryPendingVisibleChangePage()`；Reset/Append pending load 只返回单页结果。
- ReviewPanel 已接入 `FBlueprintHelperReviewPagedChangeModel`、`STreeView::OnTreeViewScrolled`、手动“加载更多”按钮、分页状态文本和 page-size/preload-row 设置。
- `AcceptAll` / `RejectAll` 不再从当前已加载分页行拼批量集合，改为通过 Presenter/CommandService 按资产从 pending index 查询全部 pending visible changes，并复用原有 batch action pipeline。
- `RefreshChanged` 命中当前选中 change 时会刷新详情区和主工作区，避免 selected identity 未变导致右侧内容停留在旧快照。
- 未修改 TaskSpec、GraphWrite、GraphLayout、TaskRun、AgentFaceService、CodexPlugin、ClaudePlugin 边界。

Verification:

```text
Build.bat TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex
Automation RunTests BlueprintHelper.Review.PagedChangeModel
Automation RunTests BlueprintHelper.Review.Panel.Command
Automation RunTests BlueprintHelper.Review.PendingLoad
Automation RunTests BlueprintHelper.Review
Automation RunTests BlueprintHelper.Settings
git diff --check
```

Known residual risk:

- `QueryPendingVisibleChangePage()` 当前仍会在 worker 侧读取并排序匹配的 lightweight pending summaries 后切页；本次目标是避免 GameThread/UI/model 全量 apply。若 pending index 本身继续放大，后续可增加 index-level cached page/range scan。

## 5. Suggested Manual Commit Scope

Only include files touched by this plan. Do not include unrelated dirty files such as `AGENT.md`, AgentFaceService task runner changes, GraphWrite runtime files, or unrelated Develop/Gaps docs.

Suggested commit message after implementation:

```text
新增内容：
1. ReviewPanel pending 数据分页查询和滚动加载模型
2. Review 性能设置增加 pending 分页大小和滚动预加载行数

修复内容：
1. 修复 async pending load 完成后一次性全量应用所有 pending changes 导致的阻塞卡顿
2. 修复 Review 变更树一次性重建全部 pending rows 的性能风险
3. 修复分页后 AcceptAll/RejectAll 只作用于当前已加载页的批量操作回归
```

Suggested manual commands after reviewing `git status`:

```powershell
git add BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewPendingIndex.h `
  BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewPendingIndexService.h `
  BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewPendingIndexService.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPendingLoadCoordinator.h `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPendingLoadCoordinator.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPagedChangeModel.h `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPagedChangeModel.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPanelCommandService.h `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPanelCommandService.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPanelPresenter.h `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPanelPresenter.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/UI/BlueprintHelperUiSettings.h `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/BlueprintHelperUiSettingsResolver.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/Settings/BlueprintHelperSettingsPresenter.cpp `
  BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/SBlueprintHelperReviewPanel.h `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanelLayout.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPendingIndexTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPagedChangeModelTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPendingLoadCoordinatorTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp `
  BlueprintHelper/Source/BlueprintHelper/Private/Tests/Config/BlueprintHelperSettingStoreTests.cpp `
  Debug/BlueprintHelper_ReviewPanel_PagedScrollPendingLoad_20260601.md `
  BlueprintHelper/Develop/Plan/BlueprintHelper_ReviewPanel_PagedScrollPendingLoad_ImplementationPlan_20260601_CN.md

git commit -m "新增内容：ReviewPanel pending 分页滚动加载"
```
