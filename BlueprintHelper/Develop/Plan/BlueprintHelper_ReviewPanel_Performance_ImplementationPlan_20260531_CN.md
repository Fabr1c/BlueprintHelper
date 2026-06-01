# BlueprintHelper ReviewPanel Performance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 BlueprintHelperWidget 打开、ReviewPanel pending 加载、Accept / Reject / AcceptAll / RejectAll 在 ReviewRecord 数量较多时的 GameThread 阻塞卡顿。

**Architecture:** Review v2 仍是唯一基线。UI 只负责显示、输入事件转发和轻量绑定；pending 数据加载、索引、异步协调、action 执行和增量刷新分别下沉到 ReviewStore / coordinator / presenter / model 边界。禁止用延迟一帧、ActiveTimer、重试 loop 或 UI 本地分支掩盖生命周期问题。

**Tech Stack:** Unreal Engine 5.6 C++、Slate、Review v2、ReviewStore JSON、UE Automation Tests、ThreadPool worker、GameThread apply。

**Project rule override:** 本仓库 AGENTS 规则禁止 Codex 自动执行 `git add`、`git commit`、`git push`。本文每个 checkpoint 只记录建议提交范围和提交消息，由用户手动执行。

---

## 0. Evidence And Priority

本计划的 5 个任务全部必做，按优先级执行：

| Priority | Required task | Main symptom fixed |
|---|---|---|
| P0 | Lazy ReviewPanel construction and no open-time pending scan | BlueprintHelperWidget 打开卡顿 |
| P1 | Pending ReviewStore index | `LoadPendingVisibleChanges()` 全量枚举、全量读 JSON |
| P2 | Async pending load coordinator, low-speed validity sweep, and GameThread snapshot apply | Review tab / store changed 刷新阻塞；外部删除/改名导致的无效 ReviewEvent 自动清理 |
| P3 | Incremental Acc/Rej action pipeline | 单个 Accept / Reject 后全量刷新和 UI 重建 |
| P4 | Batch AcceptAll / RejectAll | 批量动作每条都拷贝 snapshot、执行 action、触发刷新 |

### Current source facts

- 新增需求：BlueprintHelperWidget 打开时启动低速 ReviewEvent 有效性扫描。扫描必须检查真实资产状态，例如资产已删除、变量已删除、函数已删除、函数已改名、图/组件/Widget/DataTable row 等 anchor 已不存在；这些都视为该 ReviewEvent / visible target 无效，需要从 pending Review 中移除。
- 有效性扫描不能阻塞 GameThread。P2 worker pending load 负责从 pending index 产出候选集合；真实资产读取只能通过独立 coordinator 限速投递到 GameThread，每帧有数量和耗时预算。
- `SBlueprintHelperMainWindow::Construct()` 在 `SWidgetSwitcher` slot 中直接构造 `SBlueprintHelperReviewPanel`，即使默认 tab 不是 Review。
- `SBlueprintHelperReviewPanel::Construct()` 当前在没有 `InitialChanges` 时同步调用 `ReviewPanelPresenter->LoadPendingVisibleChanges()`。
- `FBlueprintHelperReviewStoreService::QueryReviewRecords()` 当前枚举 `Review/Records/*.json`，逐个 `LoadFileToString`、JSON 反序列化、`ReadReviewRecordFromJson` 后才过滤。
- 当前本地 `Saved/BlueprintHelper/Review/Records` 有 `3712` 个 JSON，总大小约 `179 MB`。
- `ExecuteAcceptChange()` 在 UI 事件中同步 `BuildPendingChangeSnapshot()` 并同步 `HandleActionIntent()`。
- `Reject` 已有 async prepare，但 mutation 和多次 `RefreshReviewUiAfterStateChanged()` 仍在 UI 链路中同步执行。
- `OnAcceptAll()` 当前对每个 item 都重新 `BuildPendingChangeSnapshot()` 并调用一次 action。
- `RefreshReviewUiAfterStateChanged()` 会重建 ReviewPanel state、tree、tree widget、选中资产和主工作区。

### Non-negotiable boundaries

- 不修改 TaskSpec / GraphWrite / GraphLayout / TaskRun 的职责边界。
- 不把 Review 数据加载、action mutation、批量执行或持久化写入放进 Slate widget。
- 不让 Review UI 自己维护与 ReviewStore 冲突的状态解释。
- 不引入 Review v1 / Transaction / legacy fallback 兼容路径。
- 不用 `ActiveTimer`、延迟一帧、固定 sleep、retry loop 作为 UI 生命周期修复。
- Worker 线程不得访问 `UObject*`、Graph、Blueprint editor、Slate widget。
- ReviewEvent 有效性扫描的 worker 部分只能读取 pending index / ReviewRecord JSON / immutable DTO；所有真实资产、Blueprint、Graph、变量、函数、组件、WidgetTree、DataTable row 检查都必须在 GameThread 且按预算执行。
- 低速扫描是 service/coordinator 生命周期，不是 UI ordering workaround；禁止用 Slate `ActiveTimer`、sleep、延迟一帧或 retry loop 实现。
- 无效 ReviewEvent 清理必须通过 ReviewStore 增量事件发布；不能由 ReviewPanel 直接删除本地 UI row 后跳过持久化。
- Reject 的资产回滚仍在 GameThread 执行；worker 只处理纯数据读取、索引、JSON parse、请求合并。
- Store changed 必须携带可增量消费的变更事件，不能只靠无参数 `FSimpleDelegate` 触发全量 reload。

---

## 1. File Structure

### Create

| File | Responsibility |
|---|---|
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewPerformanceTrace.h` | 轻量性能计时和日志 scope，统一记录 open/load/action/apply 耗时。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewPerformanceTrace.cpp` | 实现 scoped timer、record count、byte count、threshold logging。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewPendingIndex.h` | Pending index DTO：record summary、visible change summary、changed event。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewPendingIndex.cpp` | DTO JSON 序列化、反序列化、query filter、stale 检查。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewPendingIndexService.h` | Pending index service：load/save/rebuild/apply record saved/deleted/query summaries。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewPendingIndexService.cpp` | 实现 `.blueprinthelper` 外 Review storage 下的 pending index 文件维护。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPendingLoadCoordinator.h` | ReviewPanel pending load coordinator：request id、coalesce、worker solve、GameThread callback。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPendingLoadCoordinator.cpp` | 实现异步 pending summary/full hydration 加载，废弃过期请求。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewTargetValidityTypes.h` | ReviewEvent / visible target 有效性 DTO、无效原因、限速预算、候选集合。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewTargetValidityTypes.cpp` | 有效性 DTO 与 reason string helper。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewTargetValidityResolver.h` | GameThread 真实资产 anchor 检查接口。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewTargetValidityResolver.cpp` | 检查资产、Graph、函数/custom event、变量、组件、Widget、DataTable row、DataAsset property 是否仍存在。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewValiditySweepCoordinator.h` | 低速 ReviewEvent 有效性扫描 coordinator，拥有队列、预算、coalesce、invalid cleanup apply。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewValiditySweepCoordinator.cpp` | Widget open 和 pending load 后的限速扫描；worker hydrate record，GameThread validate asset anchors，ReviewStore 增量移除无效 target。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPanelModel.h` | ReviewPanel canonical UI model：summary list、hydrated selection、transient action states、incremental apply。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPanelModel.cpp` | 实现树构建输入、change id lookup、batch remove/update、selection preservation。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewActionCoordinator.h` | Accept / Reject / batch action coordinator：统一 action pipeline 和 store changed event。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewActionCoordinator.cpp` | 实现单条/批量 action request、GameThread mutation、worker-safe store IO 分段。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPerformanceTraceTests.cpp` | 验证 trace scope 不改变行为，阈值和计数可读。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPendingIndexTests.cpp` | 验证 pending index rebuild/query/update/delete/stale behavior。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewTargetValidityTests.cpp` | 验证资产/变量/函数/图/组件/Widget/DataTable anchor 外部删除或改名后会产生 invalid result。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPanelModelTests.cpp` | 验证 model 增量 apply、selection preservation、batch removal。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewActionCoordinatorTests.cpp` | 验证 action coordinator 不触发全量 pending reload，并正确发布 changed event。 |

### Modify

| File | Responsibility |
|---|---|
| `BlueprintHelper/Source/BlueprintHelper/Public/UI/SBlueprintHelperMainWindow.h` | 保存 lazy page host、ReviewPanel widget weak ptr、page construction state。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/SBlueprintHelperMainWindow.cpp` | 不在 main window construct 阶段构造 ReviewPanel；点击 Review tab 时懒构造；Widget 打开时启动低速 ReviewEvent 有效性扫描。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/SBlueprintHelperReviewPanel.h` | 持有 model、load coordinator、action coordinator；移除 UI 本地全量 pending load 所有权。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanelLayout.cpp` | ReviewPanel construct 不同步读取 pending records；显示 loading/empty state 并启动 async load。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp` | 将 `RefreshVisibleChanges`、`RefreshFromReviewStoreIfChanged`、Acc/Rej UI action 改为 model/coordinator 驱动。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPanelPresenter.h` | 暴露 async load request 和 action request 入口，保留 presenter 为 UI 事件边界。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPanelPresenter.cpp` | 转发到 load/action coordinator，不直接同步 `LoadPendingVisibleChanges()`。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPanelCommandService.h` | 接收 indexed changed event / batch intent，避免每次 action 只通知无参 refresh。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPanelCommandService.cpp` | action 成功后发布 affected record/change ids，不触发无条件全量 reload。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewStoreService.h` | 新增 pending summary query、changed event、batch save/update API、invalid target purge API。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewStoreService.cpp` | 集成 pending index，保留 full record read 只用于 hydration/action/validity sweep；无效 target 清理发布增量事件。 |
| `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewActionService.h` | 新增 grouped batch accept/reject API。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp` | 按 record 分组读写，避免每个 visible change 单独 load/save。 |
| `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/Utils/BlueprintHelperReviewPanelAsyncUtils.cpp` | 继续只负责 async task lifecycle，不承载业务状态。 |

### Do not modify

- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/*`
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/GraphLayout/*`
- `AgentFaceService/*`
- `CodexPlugin/*`
- `ClaudePlugin/*`

---

## Task 1: P0 Lazy ReviewPanel Construction

**Goal:** BlueprintHelperWidget 打开时不构造 ReviewPanel，不执行 pending record scan。只有用户进入 Review tab 时才构造 ReviewPanel 并启动异步加载。

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewPerformanceTrace.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewPerformanceTrace.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/SBlueprintHelperMainWindow.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/SBlueprintHelperMainWindow.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanelLayout.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPerformanceTraceTests.cpp`

- [ ] **Step 1: Add performance trace scope**

Create `BlueprintHelperReviewPerformanceTrace.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewPerformanceScope
{
public:
	FBlueprintHelperReviewPerformanceScope(
		const TCHAR* InName,
		int32 InWarnThresholdMs = 16);
	~FBlueprintHelperReviewPerformanceScope();

	void AddCount(const TCHAR* Key, int64 Value);
	void AddBytes(const TCHAR* Key, int64 Value);

private:
	const TCHAR* Name = TEXT("ReviewPerf");
	double StartSeconds = 0.0;
	int32 WarnThresholdMs = 16;
	TArray<FString> Counters;
};
```

Create `BlueprintHelperReviewPerformanceTrace.cpp`:

```cpp
#include "Systems/Review/BlueprintHelperReviewPerformanceTrace.h"

#include "HAL/PlatformTime.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBlueprintHelperReviewPerf, Log, All);
DEFINE_LOG_CATEGORY(LogBlueprintHelperReviewPerf);

FBlueprintHelperReviewPerformanceScope::FBlueprintHelperReviewPerformanceScope(
	const TCHAR* InName,
	int32 InWarnThresholdMs)
	: Name(InName)
	, StartSeconds(FPlatformTime::Seconds())
	, WarnThresholdMs(InWarnThresholdMs)
{
}

FBlueprintHelperReviewPerformanceScope::~FBlueprintHelperReviewPerformanceScope()
{
	const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	const FString CounterText = FString::Join(Counters, TEXT(" "));
	if (ElapsedMs >= WarnThresholdMs)
	{
		UE_LOG(LogBlueprintHelperReviewPerf, Warning, TEXT("%s ms=%.2f %s"), Name, ElapsedMs, *CounterText);
		return;
	}
	UE_LOG(LogBlueprintHelperReviewPerf, Verbose, TEXT("%s ms=%.2f %s"), Name, ElapsedMs, *CounterText);
}

void FBlueprintHelperReviewPerformanceScope::AddCount(const TCHAR* Key, int64 Value)
{
	Counters.Add(FString::Printf(TEXT("%s=%lld"), Key, Value));
}

void FBlueprintHelperReviewPerformanceScope::AddBytes(const TCHAR* Key, int64 Value)
{
	Counters.Add(FString::Printf(TEXT("%s_bytes=%lld"), Key, Value));
}
```

- [ ] **Step 2: Replace eager switcher slots with lazy page hosts**

In `SBlueprintHelperMainWindow.h`, add:

```cpp
TSharedPtr<SBox> PageHost;
TArray<TSharedPtr<SWidget>> ConstructedPages;
TWeakPtr<SBlueprintHelperReviewPanel> ReviewPanelWidget;

void EnsurePageConstructed(int32 PageIndex);
TSharedRef<SWidget> BuildToolsPage();
TSharedRef<SWidget> BuildReviewPage();
TSharedRef<SWidget> BuildLayoutPage();
TSharedRef<SWidget> BuildSettingsPage();
void ShowPage(int32 PageIndex);
```

In `SBlueprintHelperMainWindow::Construct`, replace the `SWidgetSwitcher` body with:

```cpp
ConstructedPages.SetNum(4);

ChildSlot
[
	SNew(SVerticalBox)
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(MainWindowSettings.TabBarPadding)
	[
		BuildTabBar()
	]
	+ SVerticalBox::Slot()
	.FillHeight(1.0f)
	[
		SAssignNew(PageHost, SBox)
	]
];

EnsurePageConstructed(ActivePageIndex);
```

If `BuildTabBar()` is not extracted in this task, keep the existing tab bar code inline and only replace the page area slot with `PageHost`.

- [ ] **Step 3: Add lazy page construction**

Add to `SBlueprintHelperMainWindow.cpp`:

```cpp
void SBlueprintHelperMainWindow::EnsurePageConstructed(int32 PageIndex)
{
	FBlueprintHelperReviewPerformanceScope Scope(TEXT("MainWindow.EnsurePageConstructed"), 8);
	Scope.AddCount(TEXT("page"), PageIndex);

	if (!ConstructedPages.IsValidIndex(PageIndex))
	{
		return;
	}
	if (!ConstructedPages[PageIndex].IsValid())
	{
		switch (PageIndex)
		{
		case 0:
			ConstructedPages[PageIndex] = BuildToolsPage();
			break;
		case 1:
			ConstructedPages[PageIndex] = BuildReviewPage();
			break;
		case 2:
			ConstructedPages[PageIndex] = BuildLayoutPage();
			break;
		case 3:
			ConstructedPages[PageIndex] = BuildSettingsPage();
			break;
		default:
			ConstructedPages[PageIndex] = SNullWidget::NullWidget;
			break;
		}
	}
	if (PageHost.IsValid())
	{
		PageHost->SetContent(ConstructedPages[PageIndex].ToSharedRef());
	}
}

void SBlueprintHelperMainWindow::ShowPage(int32 PageIndex)
{
	ActivePageIndex = PageIndex;
	EnsurePageConstructed(ActivePageIndex);
}
```

Update `ShowToolsPage`, `ShowReviewPage`, `ShowLayoutPage`, `ShowSettingsPage` to call `ShowPage(0..3)` and return `FReply::Handled()`.

- [ ] **Step 4: Move page construction code into builders**

Add:

```cpp
TSharedRef<SWidget> SBlueprintHelperMainWindow::BuildToolsPage()
{
	return SNew(SBlueprintHelperTaskSpecWorkbench)
		.GraphResolver(GraphResolver);
}

TSharedRef<SWidget> SBlueprintHelperMainWindow::BuildReviewPage()
{
	TSharedRef<SBlueprintHelperReviewPanel> Panel =
		SNew(SBlueprintHelperReviewPanel)
		.ReviewStoreService(ReviewStoreService)
		.ReviewActionService(ReviewActionService);
	ReviewPanelWidget = Panel;
	return Panel;
}

TSharedRef<SWidget> SBlueprintHelperMainWindow::BuildLayoutPage()
{
	return SNew(SBlueprintHelperLayoutRuleEditor)
		.InitialRuleSetJson(FBlueprintHelperGraphLayoutCoordinator::LoadConfiguredRuleSetJson())
		.DefaultRuleSetJson(FBlueprintHelperGraphLayoutCoordinator::GetDefaultRuleSetJson())
		.OnImportJson(FBlueprintHelperLayoutRuleEditorImportJson::CreateStatic(&FBlueprintHelperGraphLayoutCoordinator::LoadConfiguredRuleSetJson))
		.OnExportJson(FBlueprintHelperLayoutRuleEditorExportJson::CreateStatic(&FBlueprintHelperGraphLayoutCoordinator::SaveConfiguredRuleSetJson))
		.OnValidateJson(FBlueprintHelperLayoutRuleEditorValidateJson::CreateStatic(&FBlueprintHelperGraphLayoutCoordinator::ValidateRuleSetJson));
}

TSharedRef<SWidget> SBlueprintHelperMainWindow::BuildSettingsPage()
{
	return SNew(SBlueprintHelperSettingsPanel);
}
```

- [ ] **Step 5: Remove ReviewPanel construct-time pending scan**

In `SBlueprintHelperReviewPanel::Construct`, replace the sync load block:

```cpp
TArray<FBlueprintHelperReviewVisibleChange> InitialChanges = InArgs._InitialChanges;
if (InitialChanges.Num() == 0 && ReviewPanelPresenter.IsValid())
{
	InitialChanges = ReviewPanelPresenter->LoadPendingVisibleChanges();
}
RefreshVisibleChanges(InitialChanges);
```

with:

```cpp
TArray<FBlueprintHelperReviewVisibleChange> InitialChanges = InArgs._InitialChanges;
RefreshVisibleChanges(InitialChanges);
LastVisibleChangeRefreshSignature = BuildVisibleChangeRefreshSignature(InitialChanges);
```

Task 3 will add explicit async load start. Until Task 3 lands, the Review tab may initially show empty state after lazy construction; do not reintroduce sync loading in UI.

- [ ] **Step 6: Verify P0 manually**

Run:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex
```

Expected:

```text
Result: Succeeded
```

Manual editor check:

1. Open BlueprintHelperWidget with default tab `Tools` or `Layout`.
2. Confirm log does not contain `ReviewStore.QueryReviewRecords` during initial widget open.
3. Click `Review`.
4. Confirm ReviewPanel is constructed only at this click.

Suggested checkpoint message:

```text
变更需求：
1. ReviewPanel 改为懒构造，BlueprintHelperWidget 打开不再同步扫描 pending Review records
```

---

## Task 2: P1 Pending ReviewStore Index

**Goal:** `LoadPendingVisibleChanges()` 不再通过全量读取所有 ReviewRecord JSON 来判断 pending。Pending list 先从 index 读取 summary；full record 只在 hydration 或 action 时按 id 读取。

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewPendingIndex.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewPendingIndex.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewPendingIndexService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewPendingIndexService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewStoreService.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewStoreService.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPendingIndexTests.cpp`

- [ ] **Step 1: Define pending index DTO**

Create `BlueprintHelperReviewPendingIndex.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingChangeSummary
{
	FString ReviewRecordId;
	FString ChangeId;
	FString AssetPath;
	FString GraphName;
	FString LocationKey;
	FString DisplayLabel;
	FString ParentChangeId;
	FString LatestEvidenceId;
	FString TargetKind;
	FString Surface;
	FString Status;
	bool bIsAssetLifecycleRoot = false;
	TArray<FString> TargetKeys;
	TArray<FString> SourceEvidenceIds;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingRecordIndexEntry
{
	FString ReviewRecordId;
	FString FileName;
	FString AssetPath;
	FString ArchiveSessionId;
	FString Status;
	FString StorageStatus;
	int64 FileSizeBytes = 0;
	FDateTime FileTimestampUtc;
	TArray<FBlueprintHelperReviewPendingChangeSummary> PendingChanges;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingIndex
{
	FString Schema = TEXT("BlueprintHelper.ReviewPendingIndex.v1");
	int32 Version = 1;
	FDateTime BuiltAtUtc;
	TArray<FBlueprintHelperReviewPendingRecordIndexEntry> Records;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewStoreChangedEvent
{
	TArray<FString> ReviewRecordIds;
	TArray<FString> ChangeIds;
	TArray<FString> AssetPaths;
	bool bRequiresFullReload = false;

	static FBlueprintHelperReviewStoreChangedEvent FullReload();
	static FBlueprintHelperReviewStoreChangedEvent RecordsChanged(
		const TArray<FString>& InReviewRecordIds,
		const TArray<FString>& InChangeIds,
		const TArray<FString>& InAssetPaths);
};
```

- [ ] **Step 2: Implement DTO JSON conversion**

In `BlueprintHelperReviewPendingIndex.cpp`, implement:

```cpp
TSharedRef<FJsonObject> WritePendingIndexToJson(const FBlueprintHelperReviewPendingIndex& Index);
bool ReadPendingIndexFromJson(const TSharedPtr<FJsonObject>& Json, FBlueprintHelperReviewPendingIndex& OutIndex);
FBlueprintHelperReviewPendingRecordIndexEntry MakePendingIndexEntryFromRecord(
	const FBlueprintHelperReviewRecord& Record,
	const FString& FileName,
	int64 FileSizeBytes,
	const FDateTime& FileTimestampUtc);
```

The summary must include enough fields to show the sidebar row and route later hydration:

```cpp
Summary.ReviewRecordId = Record.ReviewRecordId;
Summary.ChangeId = Change.ChangeId;
Summary.AssetPath = Change.AssetPath.IsEmpty() ? Record.AssetPath : Change.AssetPath;
Summary.GraphName = Change.GraphName;
Summary.LocationKey = Change.LocationKey;
Summary.DisplayLabel = Change.DisplayLabel;
Summary.ParentChangeId = Change.ParentChangeId;
Summary.LatestEvidenceId = Change.LatestEvidenceId;
Summary.TargetKind = Change.TargetKind;
Summary.Surface = BlueprintHelperReviewSurfaceToString(Change.Surface);
Summary.Status = BlueprintHelperReviewChangeStatusToString(Change.Status);
Summary.bIsAssetLifecycleRoot = Change.bIsAssetLifecycleRoot;
for (const FBlueprintHelperReviewAtomicTarget& Target : Change.AtomicTargets)
{
	if (!Target.TargetKey.IsEmpty())
	{
		Summary.TargetKeys.Add(Target.TargetKey);
	}
}
Summary.SourceEvidenceIds = Change.SourceEvidenceIds;
```

- [ ] **Step 3: Add index service**

Create `BlueprintHelperReviewPendingIndexService.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndex.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewPendingIndexService
{
public:
	FString GetIndexPath() const;

	bool LoadIndex(FBlueprintHelperReviewPendingIndex& OutIndex, FString& OutError) const;
	bool SaveIndex(const FBlueprintHelperReviewPendingIndex& Index, FString& OutError) const;
	bool RebuildIndexFromRecords(FBlueprintHelperReviewPendingIndex& OutIndex, FString& OutError) const;

	bool ApplyRecordSaved(
		const FBlueprintHelperReviewRecord& Record,
		FBlueprintHelperReviewStoreChangedEvent& OutEvent,
		FString& OutError) const;

	bool ApplyRecordDeleted(
		const FString& ReviewRecordId,
		FBlueprintHelperReviewStoreChangedEvent& OutEvent,
		FString& OutError) const;

	TArray<FBlueprintHelperReviewPendingChangeSummary> QueryPendingSummaries(
		const FString& AssetPathFilter = TEXT("")) const;
};
```

`GetIndexPath()` returns:

```cpp
FBlueprintHelperReviewConfig Config = FBlueprintHelperReviewConfigResolver::Load();
return FPaths::Combine(Config.GetReviewRootDir(), TEXT("PendingIndex.v1.json"));
```

Use the existing Review root, not `.blueprinthelper`, because the index belongs to Review runtime storage rather than user-authored configuration.

- [ ] **Step 4: Rebuild index by reading records once**

In `RebuildIndexFromRecords`, enumerate record files once and write only pending summaries:

```cpp
TArray<FString> Files;
IFileManager::Get().FindFiles(Files, *(RecordsDir / TEXT("*.json")), true, false);
for (const FString& File : Files)
{
	const FString Path = RecordsDir / File;
	FFileStatData Stat = IFileManager::Get().GetStatData(*Path);
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *Path))
	{
		continue;
	}
	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	FBlueprintHelperReviewRecord Record;
	if (FJsonSerializer::Deserialize(Reader, Json)
		&& Json.IsValid()
		&& FBlueprintHelperReviewStoreJsonUtils::ReadReviewRecordFromJson(Json, Record))
	{
		FBlueprintHelperReviewPendingRecordIndexEntry Entry =
			MakePendingIndexEntryFromRecord(Record, File, Stat.FileSize, Stat.ModificationTime);
		if (Entry.PendingChanges.Num() > 0)
		{
			OutIndex.Records.Add(MoveTemp(Entry));
		}
	}
}
```

This full rebuild is allowed only when index is missing or stale. Normal open and normal action refresh must not call this path.

- [ ] **Step 5: Integrate index updates into store writes**

In `FBlueprintHelperReviewStoreService::SaveReviewRecord`, after the record file is successfully saved:

```cpp
FBlueprintHelperReviewPendingIndexService IndexService;
FBlueprintHelperReviewStoreChangedEvent ChangedEvent;
FString IndexError;
if (!IndexService.ApplyRecordSaved(Record, ChangedEvent, IndexError))
{
	UE_LOG(LogBlueprintHelperReviewStore, Warning, TEXT("Pending index update failed after save: %s"), *IndexError);
}
NotifyPendingReviewChanged(ChangedEvent);
```

In `DeleteReviewRecord` and record-deleting `PurgeReviewTargets`, call `ApplyRecordDeleted` and notify the same changed event.

- [ ] **Step 6: Add summary query API**

In `FBlueprintHelperReviewStoreService` add:

```cpp
TArray<FBlueprintHelperReviewPendingChangeSummary> QueryPendingChangeSummaries(
	const FString& AssetPathFilter = TEXT("")) const;

FDelegateHandle AddPendingReviewChangedHandler(
	TDelegate<void(const FBlueprintHelperReviewStoreChangedEvent&)> Handler) const;
void NotifyPendingReviewChanged(const FBlueprintHelperReviewStoreChangedEvent& Event) const;
```

Keep the existing no-arg delegate only until all current call sites in this plan are migrated. At the end of Task 4, remove the no-arg path if no call site remains.

- [ ] **Step 7: Add pending index tests**

Create automation tests that:

1. Save three records: pending, accepted, rejected.
2. Rebuild index.
3. Assert query returns only pending changes.
4. Save the pending record as accepted.
5. Assert index query no longer returns it.
6. Delete a record and assert index removes the entry.

Expected assertions:

```cpp
TestEqual(TEXT("Only pending summaries are indexed"), Summaries.Num(), 1);
TestEqual(TEXT("Pending summary keeps record id"), Summaries[0].ReviewRecordId, PendingRecord.ReviewRecordId);
TestTrue(TEXT("Accepted record removed from pending index"), UpdatedSummaries.Num() == 0);
TestTrue(TEXT("Deleted record removed from pending index"), DeletedSummaries.Num() == 0);
```

- [ ] **Step 8: Verify P1**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.Review.PendingIndex; Quit' -TestExit='Automation Test Queue Empty'
```

Expected:

```text
BlueprintHelper.Review.PendingIndex: 0 failed
```

Suggested checkpoint message:

```text
新增内容：
1. 新增 Review pending index，pending 列表不再依赖打开时全量读取所有 ReviewRecord
```

---

## Task 3: P2 Async Pending Load Coordinator And Validity Sweep

**Goal:** Review tab 首次打开和 store changed 刷新都走 worker load -> GameThread model apply。BlueprintHelperWidget 打开时额外启动低速 ReviewEvent 有效性扫描：worker 从 pending index / record JSON 产出候选，GameThread 按预算读取真实资产并移除资产/变量/函数/图/组件等已失效的 ReviewEvent。GameThread 不执行磁盘全量读、不解析 179 MB JSON、不同步构建 full visible changes。

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPendingLoadCoordinator.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPendingLoadCoordinator.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewTargetValidityTypes.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewTargetValidityTypes.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewTargetValidityResolver.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewTargetValidityResolver.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewValiditySweepCoordinator.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewValiditySweepCoordinator.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPanelModel.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPanelModel.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/SBlueprintHelperMainWindow.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/SBlueprintHelperMainWindow.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewStoreService.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewStoreService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/SBlueprintHelperReviewPanel.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanelLayout.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPanelPresenter.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewPanelModelTests.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewTargetValidityTests.cpp`

- [ ] **Step 1: Add canonical panel model**

Create `BlueprintHelperReviewPanelModel.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndex.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewPanelModel
{
public:
	void ReplaceSummaries(const TArray<FBlueprintHelperReviewPendingChangeSummary>& InSummaries);
	void ApplyStoreChangedSummaries(
		const FBlueprintHelperReviewStoreChangedEvent& Event,
		const TArray<FBlueprintHelperReviewPendingChangeSummary>& ChangedSummaries);
	void ApplyHydratedChange(const FBlueprintHelperReviewVisibleChange& Change);
	void MarkActionInProgress(const FString& ChangeId, const FString& ActionName);
	void ApplyActionResult(const FString& ChangeId, bool bRemoveFromPending, const FString& Message);

	TArray<FBlueprintHelperReviewVisibleChange> BuildVisibleChangesForTree() const;
	TArray<FBlueprintHelperReviewVisibleChange> BuildPendingActionSnapshot() const;
	bool TryGetSelectedChange(FBlueprintHelperReviewVisibleChange& OutChange) const;
	void PreserveOrSelectAfterChange(const FString& PreviousChangeId);

private:
	TArray<FBlueprintHelperReviewPendingChangeSummary> Summaries;
	TMap<FString, FBlueprintHelperReviewVisibleChange> HydratedChangesById;
	TSet<FString> ActionInProgressChangeIds;
	FString SelectedChangeId;
};
```

Model conversion from summary to visible change must produce a lightweight visible change with ids, label, asset path, parent, lifecycle root flag, target keys, and source evidence ids. It must not invent before/after snapshots.

- [ ] **Step 2: Add pending load coordinator**

Create `BlueprintHelperReviewPendingLoadCoordinator.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndex.h"
#include "Systems/Review/BlueprintHelperReviewTargetValidityTypes.h"

class FBlueprintHelperReviewStoreService;

struct BLUEPRINTHELPER_API FBlueprintHelperReviewPendingLoadResult
{
	int64 RequestId = 0;
	TArray<FBlueprintHelperReviewPendingChangeSummary> Summaries;
	TArray<FBlueprintHelperReviewValidityCandidate> ValidityCandidates;
	FBlueprintHelperReviewStoreChangedEvent SourceEvent;
	FString Error;
};

DECLARE_DELEGATE_OneParam(FBlueprintHelperReviewPendingLoadCompleted, const FBlueprintHelperReviewPendingLoadResult&);

class BLUEPRINTHELPER_API FBlueprintHelperReviewPendingLoadCoordinator
{
public:
	explicit FBlueprintHelperReviewPendingLoadCoordinator(const FBlueprintHelperReviewStoreService* InStoreService);
	~FBlueprintHelperReviewPendingLoadCoordinator();

	int64 RequestInitialLoad(const FString& AssetPathFilter, FBlueprintHelperReviewPendingLoadCompleted OnCompleted);
	int64 RequestChangedLoad(
		const FBlueprintHelperReviewStoreChangedEvent& Event,
		const FString& AssetPathFilter,
		FBlueprintHelperReviewPendingLoadCompleted OnCompleted);
	void CancelOutstanding();
	void SetValidityCandidateBudget(int32 MaxCandidatesPerLoad);

private:
	const FBlueprintHelperReviewStoreService* StoreService = nullptr;
	TAtomic<int64> LastIssuedRequestId { 0 };
	TAtomic<int64> LastAppliedRequestId { 0 };
	int32 MaxValidityCandidatesPerLoad = 256;
};
```

- [ ] **Step 3: Implement worker query and GameThread callback**

In coordinator `.cpp`, worker may call only index / JSON-store APIs:

```cpp
Result.Summaries = StoreService->QueryPendingChangeSummaries(AssetPathFilter);
Result.ValidityCandidates = FBlueprintHelperReviewValidityCandidate::FromPendingSummaries(
	Result.Summaries,
	MaxValidityCandidatesPerLoad);
```

Completion must return to GameThread:

```cpp
AsyncTask(ENamedThreads::GameThread, [WeakThis, Result = MoveTemp(Result), OnCompleted]()
{
	if (!WeakThis.IsValid())
	{
		return;
	}
	OnCompleted.ExecuteIfBound(Result);
});
```

Do not call `FPackageName::DoesPackageExist`, `LoadObject`, `UEdGraph`, `UBlueprint`, `UDataTable`, `UWidgetTree`, or Slate APIs on the worker.

- [ ] **Step 4: Add validity DTOs**

Create `BlueprintHelperReviewTargetValidityTypes.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndex.h"

enum class EBlueprintHelperReviewInvalidReason : uint8
{
	None,
	AssetMissing,
	AssetClassMismatch,
	GraphMissing,
	NodeMissing,
	FunctionMissingOrRenamed,
	CustomEventMissingOrRenamed,
	VariableMissingOrRenamed,
	ComponentMissingOrRenamed,
	WidgetMissingOrRenamed,
	DataTableRowMissing,
	DataAssetPropertyMissing,
	UnsupportedTarget
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewValidityCandidate
{
	FString ReviewRecordId;
	FString ChangeId;
	FString AssetPath;
	FString GraphName;
	FString TargetKey;
	FString TargetKind;
	FString Surface;
	FString DisplayLabel;

	static TArray<FBlueprintHelperReviewValidityCandidate> FromPendingSummaries(
		const TArray<FBlueprintHelperReviewPendingChangeSummary>& Summaries,
		int32 Limit);
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewValidityResult
{
	FBlueprintHelperReviewValidityCandidate Candidate;
	bool bValid = true;
	EBlueprintHelperReviewInvalidReason InvalidReason = EBlueprintHelperReviewInvalidReason::None;
	FString Detail;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewValiditySweepBudget
{
	int32 MaxRecordHydrationsPerWorkerBatch = 8;
	int32 MaxGameThreadTargetsPerFrame = 2;
	double MaxGameThreadMillisecondsPerFrame = 1.0;
	int32 MaxInvalidPurgesPerBatch = 32;
};

const TCHAR* BlueprintHelperReviewInvalidReasonToString(EBlueprintHelperReviewInvalidReason Reason);
```

`FromPendingSummaries` must expand each summary target key into a separate candidate. If a summary has no target keys, create one candidate with an empty target key so the resolver can still validate asset-level existence.

- [ ] **Step 5: Add GameThread target validity resolver**

Create `BlueprintHelperReviewTargetValidityResolver.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewTargetValidityTypes.h"

class BLUEPRINTHELPER_API FBlueprintHelperReviewTargetValidityResolver
{
public:
	FBlueprintHelperReviewValidityResult ValidateOnGameThread(
		const FBlueprintHelperReviewValidityCandidate& Candidate) const;

private:
	FBlueprintHelperReviewValidityResult MakeInvalid(
		const FBlueprintHelperReviewValidityCandidate& Candidate,
		EBlueprintHelperReviewInvalidReason Reason,
		const FString& Detail) const;
};
```

Implementation requirements:

```cpp
check(IsInGameThread());
```

Validation rules:

| Target | Invalid when |
|---|---|
| Asset-level review | package/object path no longer resolves |
| Graph body / graph block / node target | Blueprint graph is missing, or recorded node/block anchor no longer exists |
| Function | function graph or function entry name/guid no longer exists; renamed function is invalid under the old anchor |
| Custom event | custom event node name/guid no longer exists; renamed event is invalid under the old anchor |
| Variable / property / field | Blueprint variable/member/property path no longer exists; renamed variable is invalid under the old anchor |
| Component / SCS node | component variable or SCS node no longer exists |
| Widget | WidgetTree named widget no longer exists |
| DataTable row | row name no longer exists |
| DataAsset property | property path no longer resolves |

Unsupported target kinds must return `UnsupportedTarget` and must not be purged automatically until the test suite explicitly covers them.

- [ ] **Step 6: Add low-speed validity sweep coordinator**

Create `BlueprintHelperReviewValiditySweepCoordinator.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewTargetValidityTypes.h"

class FBlueprintHelperReviewStoreService;
class FBlueprintHelperReviewTargetValidityResolver;

class BLUEPRINTHELPER_API FBlueprintHelperReviewValiditySweepCoordinator
{
public:
	FBlueprintHelperReviewValiditySweepCoordinator(
		const FBlueprintHelperReviewStoreService* InStoreService,
		FBlueprintHelperReviewValiditySweepBudget InBudget);
	~FBlueprintHelperReviewValiditySweepCoordinator();

	void StartSlowSweepOnWidgetOpen();
	void EnqueueCandidatesFromPendingLoad(const TArray<FBlueprintHelperReviewValidityCandidate>& Candidates);
	void TickGameThread(float DeltaSeconds);
	void Cancel();

private:
	void RequestCandidateBatchFromWorker();
	void ApplyInvalidResults(const TArray<FBlueprintHelperReviewValidityResult>& InvalidResults);

	const FBlueprintHelperReviewStoreService* StoreService = nullptr;
	FBlueprintHelperReviewValiditySweepBudget Budget;
	TQueue<FBlueprintHelperReviewValidityCandidate, EQueueMode::Mpsc> PendingCandidates;
	TArray<FBlueprintHelperReviewValidityResult> PendingInvalidResults;
	TAtomic<bool> bCancelled { false };
};
```

Coordinator behavior:

1. `StartSlowSweepOnWidgetOpen()` starts a worker request that queries pending summaries from the index, not full records.
2. Worker may hydrate a small number of ReviewRecords per batch only to extract pure target DTOs; it must not touch `UObject`.
3. `TickGameThread` validates at most `MaxGameThreadTargetsPerFrame` candidates and exits early once `MaxGameThreadMillisecondsPerFrame` is reached.
4. Invalid results are purged through `FBlueprintHelperReviewStoreService::PurgeInvalidReviewTargets`.
5. Purge publishes one `FBlueprintHelperReviewStoreChangedEvent` with affected record ids / change ids / asset paths.
6. The coordinator must be owned by a reusable service or main window presenter boundary. It must not be owned by `SBlueprintHelperReviewPanel` only, because the scan starts when BlueprintHelperWidget opens even if Review tab is never selected.

Add this API to `FBlueprintHelperReviewStoreService`:

```cpp
bool PurgeInvalidReviewTargets(
	const TArray<FBlueprintHelperReviewValidityResult>& InvalidResults,
	FBlueprintHelperReviewStoreChangedEvent& OutChangedEvent,
	FString& OutError) const;
```

Implementation rules:

```cpp
// Group invalid targets by ReviewRecordId.
// Load each affected record once.
// Remove only matching target keys/change ids.
// If a visible change has no remaining atomic targets, remove that visible change.
// If a record has no remaining visible changes, delete the record.
// Update the pending index through FBlueprintHelperReviewPendingIndexService.
// Return one changed event with all affected record ids, change ids, and asset paths.
```

- [ ] **Step 7: Start async load from ReviewPanel construct and queue validity candidates**

In `SBlueprintHelperReviewPanel::Construct`, after layout wiring and delegate registration:

```cpp
ReviewPanelModel = MakeUnique<FBlueprintHelperReviewPanelModel>();
PendingLoadCoordinator = MakeUnique<FBlueprintHelperReviewPendingLoadCoordinator>(InArgs._ReviewStoreService);
RequestPendingReviewRefresh(TEXT("initial_open"), FBlueprintHelperReviewStoreChangedEvent::FullReload());
```

Add:

```cpp
void SBlueprintHelperReviewPanel::RequestPendingReviewRefresh(
	const FString& Reason,
	const FBlueprintHelperReviewStoreChangedEvent& Event)
{
	if (!PendingLoadCoordinator)
	{
		return;
	}
	PendingLoadCoordinator->RequestChangedLoad(
		Event,
		TEXT(""),
		FBlueprintHelperReviewPendingLoadCompleted::CreateSP(
			this,
			&SBlueprintHelperReviewPanel::HandlePendingReviewLoadCompleted));
}
```

When `HandlePendingReviewLoadCompleted` receives a result, forward the candidates to the coordinator owned by the main window presenter:

```cpp
if (ReviewValiditySweepCoordinator.IsValid())
{
	ReviewValiditySweepCoordinator->EnqueueCandidatesFromPendingLoad(Result.ValidityCandidates);
}
```

In `SBlueprintHelperMainWindow::Construct`, start the low-speed sweep after services are assigned but before any page is constructed:

```cpp
ReviewValiditySweepCoordinator = MakeShared<FBlueprintHelperReviewValiditySweepCoordinator>(
	ReviewStoreService,
	FBlueprintHelperReviewValiditySweepBudget());
ReviewValiditySweepCoordinator->StartSlowSweepOnWidgetOpen();
```

- [ ] **Step 8: Apply load result through model**

Add:

```cpp
void SBlueprintHelperReviewPanel::HandlePendingReviewLoadCompleted(
	const FBlueprintHelperReviewPendingLoadResult& Result)
{
	if (!ReviewPanelModel)
	{
		return;
	}
	const FString PreviousChangeId = SelectedChange.IsValid() ? SelectedChange->ChangeId : FString();
	if (Result.SourceEvent.bRequiresFullReload)
	{
		ReviewPanelModel->ReplaceSummaries(Result.Summaries);
	}
	else
	{
		ReviewPanelModel->ApplyStoreChangedSummaries(Result.SourceEvent, Result.Summaries);
	}
	ReviewPanelModel->PreserveOrSelectAfterChange(PreviousChangeId);
	RefreshVisibleChanges(ReviewPanelModel->BuildVisibleChangesForTree());
	RefreshChangeTreeWidget();
}
```

This is the only place initial pending summaries become UI rows.

- [ ] **Step 9: Convert store changed handler to event-driven load**

Replace no-arg handler:

```cpp
FSimpleDelegate::CreateSP(this, &SBlueprintHelperReviewPanel::RefreshFromReviewStoreIfChanged)
```

with event handler:

```cpp
FBlueprintHelperReviewStoreChangedDelegate::CreateSP(
	this,
	&SBlueprintHelperReviewPanel::HandleReviewStoreChanged)
```

`HandleReviewStoreChanged` calls `RequestPendingReviewRefresh(TEXT("store_changed"), Event)`.

- [ ] **Step 10: Add model and validity tests**

Tests must verify:

```cpp
Model.ReplaceSummaries({SummaryA, SummaryB});
TestEqual(TEXT("Two tree changes"), Model.BuildVisibleChangesForTree().Num(), 2);

Model.MarkActionInProgress(SummaryA.ChangeId, TEXT("accept"));
Model.ApplyActionResult(SummaryA.ChangeId, true, TEXT("accepted"));
TestEqual(TEXT("One pending change remains"), Model.BuildVisibleChangesForTree().Num(), 1);

Model.ApplyHydratedChange(FullChangeB);
TestTrue(TEXT("Hydrated change is used for pending snapshot"),
	Model.BuildPendingActionSnapshot()[0].BeforeSnapshotJson == FullChangeB.BeforeSnapshotJson);
```

Add validity tests:

```cpp
FBlueprintHelperReviewValidityCandidate MissingAsset;
MissingAsset.AssetPath = TEXT("/Game/BlueprintHelperTests/MissingAsset.MissingAsset");
const FBlueprintHelperReviewValidityResult MissingAssetResult =
	Resolver.ValidateOnGameThread(MissingAsset);
TestFalse(TEXT("Missing asset invalidates review target"), MissingAssetResult.bValid);
TestEqual(TEXT("Missing asset reason"), MissingAssetResult.InvalidReason, EBlueprintHelperReviewInvalidReason::AssetMissing);

FBlueprintHelperReviewValidityCandidate RenamedFunction = MakeFunctionCandidate(BP, TEXT("OldFunctionName"));
RenameBlueprintFunctionForTest(BP, TEXT("OldFunctionName"), TEXT("NewFunctionName"));
const FBlueprintHelperReviewValidityResult RenamedFunctionResult =
	Resolver.ValidateOnGameThread(RenamedFunction);
TestFalse(TEXT("Renamed function invalidates old review target"), RenamedFunctionResult.bValid);
TestEqual(TEXT("Renamed function reason"), RenamedFunctionResult.InvalidReason, EBlueprintHelperReviewInvalidReason::FunctionMissingOrRenamed);

FBlueprintHelperReviewValidityCandidate DeletedVariable = MakeVariableCandidate(BP, TEXT("OldVariableName"));
DeleteBlueprintVariableForTest(BP, TEXT("OldVariableName"));
const FBlueprintHelperReviewValidityResult DeletedVariableResult =
	Resolver.ValidateOnGameThread(DeletedVariable);
TestFalse(TEXT("Deleted variable invalidates review target"), DeletedVariableResult.bValid);
TestEqual(TEXT("Deleted variable reason"), DeletedVariableResult.InvalidReason, EBlueprintHelperReviewInvalidReason::VariableMissingOrRenamed);
```

- [ ] **Step 11: Verify P2**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.Review.PanelModel; BlueprintHelper.Review.TargetValidity; Quit' -TestExit='Automation Test Queue Empty'
```

Expected:

```text
BlueprintHelper.Review.PanelModel: 0 failed
BlueprintHelper.Review.TargetValidity: 0 failed
```

Manual editor check:

1. Open BlueprintHelperWidget.
2. Confirm low-speed validity sweep starts without constructing ReviewPanel and without synchronous `QueryReviewRecords`.
3. Click Review.
4. Confirm UI remains responsive while pending rows load.
5. Delete or rename a reviewed Blueprint variable/function in a test asset.
6. Reopen BlueprintHelperWidget and confirm the old ReviewEvent is removed by validity sweep through a store changed event.
7. Confirm logs show per-frame target validation budget, not a long GameThread validation block.

Suggested checkpoint message:

```text
变更需求：
1. ReviewPanel pending 加载改为 worker query + GameThread model apply，并在 Widget 打开时启动低速 ReviewEvent 有效性扫描
```

---

## Task 4: P3 Incremental Acc/Rej Action Pipeline

**Goal:** 单个 Accept / Reject 不再同步拷贝全部 pending changes、不再通过无参 store delegate 触发全量 reload、不再重建整个 ReviewPanel 工作区。动作结果只更新受影响 change/record/asset。

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewActionCoordinator.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewActionCoordinator.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewPanelCommandService.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPanelCommandService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewPanelPresenter.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewActionCoordinatorTests.cpp`

- [ ] **Step 1: Define action request/result**

Create `BlueprintHelperReviewActionCoordinator.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/Review/BlueprintHelperReviewActionService.h"
#include "Systems/Review/BlueprintHelperReviewPendingIndex.h"

enum class EBlueprintHelperReviewCoordinatedAction : uint8
{
	Accept,
	Reject
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewActionRequest
{
	EBlueprintHelperReviewCoordinatedAction Action = EBlueprintHelperReviewCoordinatedAction::Accept;
	FBlueprintHelperReviewVisibleChange Change;
	TArray<FBlueprintHelperReviewVisibleChange> PendingSnapshotForCascade;
	FBlueprintHelperReviewRejectOptions RejectOptions;
	FString Source;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewActionCompleted
{
	FString ChangeId;
	FBlueprintHelperReviewActionResult ActionResult;
	FBlueprintHelperReviewCascadeActionResult CascadeResult;
	FBlueprintHelperReviewStoreChangedEvent StoreChangedEvent;
	bool bCascade = false;
};

DECLARE_DELEGATE_OneParam(FBlueprintHelperReviewActionCompletedDelegate, const FBlueprintHelperReviewActionCompleted&);

class BLUEPRINTHELPER_API FBlueprintHelperReviewActionCoordinator
{
public:
	FBlueprintHelperReviewActionCoordinator(
		const FBlueprintHelperReviewActionService* InActionService,
		const FBlueprintHelperReviewStoreService* InStoreService);

	void Execute(
		const FBlueprintHelperReviewActionRequest& Request,
		FBlueprintHelperReviewActionCompletedDelegate OnCompleted) const;

private:
	const FBlueprintHelperReviewActionService* ActionService = nullptr;
	const FBlueprintHelperReviewStoreService* StoreService = nullptr;
};
```

- [ ] **Step 2: Move single action execution out of widget branches**

In `SBlueprintHelperReviewPanel::ExecuteAcceptChange`, replace direct presenter sync call with:

```cpp
ReviewPanelModel->MarkActionInProgress(Item->ChangeId, TEXT("accept"));
RefreshTreeRowForChange(Item->ChangeId);

FBlueprintHelperReviewActionRequest Request;
Request.Action = EBlueprintHelperReviewCoordinatedAction::Accept;
Request.Change = *Item;
Request.Source = TEXT("review_panel");

ActionCoordinator->Execute(
	Request,
	FBlueprintHelperReviewActionCompletedDelegate::CreateSP(
		this,
		&SBlueprintHelperReviewPanel::HandleReviewActionCompleted));
```

In `ExecuteRejectChange`, keep async reject option preparation, then create the same request with `Action = Reject` and the prepared `RejectOptions`.

- [ ] **Step 3: Build cascade snapshot only when needed**

Remove unconditional `BuildPendingChangeSnapshot()` from `ExecuteAcceptChange`.

For reject lifecycle root only:

```cpp
if (Item->bIsAssetLifecycleRoot && ReviewPanelModel)
{
	Request.PendingSnapshotForCascade = ReviewPanelModel->BuildPendingActionSnapshot();
}
```

Non-root Accept and non-root Reject must not copy all pending changes.

- [ ] **Step 4: Publish affected store changed event**

In `FBlueprintHelperReviewPanelCommandService::NotifyStoreChangedIfSucceeded`, replace no-arg notify with:

```cpp
FBlueprintHelperReviewStoreChangedEvent Event =
	FBlueprintHelperReviewStoreChangedEvent::RecordsChanged(
		{ Result.ReviewRecordId },
		{ Result.ChangeId },
		{ Result.AssetPath });
ReviewStoreService->NotifyPendingReviewChanged(Event);
```

If `FBlueprintHelperReviewActionResult` lacks `ReviewRecordId`, `ChangeId`, or `AssetPath`, add those fields and populate them in `AcceptReviewTargets`, `RejectReviewTargets`, and cascade result assembly.

- [ ] **Step 5: Apply action result incrementally**

Add:

```cpp
void SBlueprintHelperReviewPanel::HandleReviewActionCompleted(
	const FBlueprintHelperReviewActionCompleted& Completed)
{
	const bool bSucceeded = Completed.bCascade
		? Completed.CascadeResult.RootResult.bSucceeded
		: Completed.ActionResult.bSucceeded;
	ReviewPanelModel->ApplyActionResult(
		Completed.ChangeId,
		bSucceeded,
		bSucceeded ? TEXT("done") : Completed.ActionResult.Message);
	RequestPendingReviewRefresh(TEXT("action_completed"), Completed.StoreChangedEvent);
	RefreshChangeTreeWidget();
	RefreshMainWorkspaceForAffectedAsset(Completed.StoreChangedEvent.AssetPaths);
}
```

`RefreshMainWorkspaceForAffectedAsset` must only reload the selected asset workspace when the selected asset is in `AssetPaths`. It must not call `RefreshReviewUiAfterStateChanged()` for every action.

- [ ] **Step 6: Retire full refresh from single action path**

Remove action path calls to:

```cpp
RefreshReviewUiAfterStateChanged(TEXT("reject_queued"), ...);
RefreshReviewUiAfterStateChanged(TEXT("reject_mutating"), ...);
RefreshReviewUiAfterStateChanged(TEXT("reject_change_failed"), ...);
```

Replace them with:

```cpp
RefreshTreeRowForChange(ChangeId);
SyncReviewRowHighlightStatesForChange(ChangeId);
```

If a helper does not exist, add a helper that invalidates only the affected row and current surface overlay state.

- [ ] **Step 7: Add action coordinator tests**

Tests must verify:

```cpp
TestEqual(TEXT("Accept does not request full reload"), Completed.StoreChangedEvent.bRequiresFullReload, false);
TestTrue(TEXT("Accept reports affected change id"), Completed.StoreChangedEvent.ChangeIds.Contains(Change.ChangeId));
TestTrue(TEXT("Accept reports affected asset"), Completed.StoreChangedEvent.AssetPaths.Contains(Change.AssetPath));
```

For reject lifecycle root:

```cpp
TestTrue(TEXT("Lifecycle root reject uses cascade result"), Completed.bCascade);
TestEqual(TEXT("Cascade receives pending snapshot only for root"), CapturedPendingSnapshot.Num(), ExpectedCascadeCount);
```

- [ ] **Step 8: Verify P3**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.Review.ActionCoordinator; BlueprintHelper.Review.Action; Quit' -TestExit='Automation Test Queue Empty'
```

Expected:

```text
BlueprintHelper.Review.ActionCoordinator: 0 failed
BlueprintHelper.Review.Action: 0 failed
```

Manual editor check:

1. Load Review tab with thousands of pending changes.
2. Accept one row.
3. Confirm only the row/model updates; no full pending scan log appears.
4. Reject one non-root row.
5. Confirm UI does not freeze while preparing options; mutation completes and only affected rows refresh.

Suggested checkpoint message:

```text
修复内容：
1. Accept / Reject 统一走增量 action pipeline，移除单条动作后的全量 ReviewPanel 刷新
```

---

## Task 5: P4 Batch AcceptAll / RejectAll

**Goal:** AcceptAll / RejectAll 按 record 分组批处理，只构建一次 action set，只发布一次 batch changed event，不对每个 visible change 执行一次 full snapshot 和一次 store notify。

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/Review/BlueprintHelperReviewActionService.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/Review/BlueprintHelperReviewActionService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewActionCoordinator.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewActionCoordinator.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanelLayout.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewActionCoordinatorTests.cpp`

- [ ] **Step 1: Add grouped batch request**

Add to `BlueprintHelperReviewActionCoordinator.h`:

```cpp
struct BLUEPRINTHELPER_API FBlueprintHelperReviewBatchActionRequest
{
	EBlueprintHelperReviewCoordinatedAction Action = EBlueprintHelperReviewCoordinatedAction::Accept;
	TArray<FBlueprintHelperReviewVisibleChange> Changes;
	FBlueprintHelperReviewRejectOptions RejectOptions;
	FString Source;
};

struct BLUEPRINTHELPER_API FBlueprintHelperReviewBatchActionCompleted
{
	int32 RequestedCount = 0;
	int32 SucceededCount = 0;
	int32 FailedCount = 0;
	TArray<FBlueprintHelperReviewActionResult> Results;
	FBlueprintHelperReviewStoreChangedEvent StoreChangedEvent;
};

DECLARE_DELEGATE_OneParam(FBlueprintHelperReviewBatchActionCompletedDelegate, const FBlueprintHelperReviewBatchActionCompleted&);
```

- [ ] **Step 2: Add ReviewActionService batch APIs**

In `BlueprintHelperReviewActionService.h` add:

```cpp
FBlueprintHelperReviewBatchActionResult AcceptReviewTargetsBatch(
	const TMap<FString, TArray<FString>>& TargetKeysByReviewRecordId) const;

FBlueprintHelperReviewBatchActionResult RejectReviewTargetsBatch(
	const TMap<FString, TArray<FString>>& TargetKeysByReviewRecordId,
	const FBlueprintHelperReviewRejectOptions& Options) const;
```

Define `FBlueprintHelperReviewBatchActionResult` with:

```cpp
bool bSucceeded = false;
int32 RequestedCount = 0;
int32 SucceededCount = 0;
int32 FailedCount = 0;
TArray<FBlueprintHelperReviewActionResult> Results;
TArray<FString> ChangedReviewRecordIds;
TArray<FString> ChangedChangeIds;
TArray<FString> ChangedAssetPaths;
FString Message;
```

- [ ] **Step 3: Implement batch grouping without repeated load/save**

In `AcceptReviewTargetsBatch`:

```cpp
for (const TPair<FString, TArray<FString>>& Pair : TargetKeysByReviewRecordId)
{
	FBlueprintHelperReviewRecord Record;
	FString Error;
	if (!Store.LoadReviewRecordById(Pair.Key, Record, Error))
	{
		AddFailedResult(Pair.Key, Error);
		continue;
	}
	ApplyAcceptTargetsToLoadedRecord(Record, Pair.Value, RecordResult);
	if (!Store.SaveReviewRecord(Record, Error))
	{
		AddFailedResult(Pair.Key, Error);
		continue;
	}
	AddChangedRecord(Record);
}
Store.NotifyPendingReviewChanged(BatchChangedEvent);
```

`ApplyAcceptTargetsToLoadedRecord` and reject equivalent must be private helpers in the `.cpp`, not widget code.

- [ ] **Step 4: Replace OnAcceptAll loop**

In `SBlueprintHelperReviewPanel::OnAcceptAll`, replace per-item direct action calls with:

```cpp
FBlueprintHelperReviewBatchActionRequest Request;
Request.Action = EBlueprintHelperReviewCoordinatedAction::Accept;
Request.Changes = ReviewPanelModel->BuildVisibleChangesForTree();
Request.Source = TEXT("review_panel_accept_all");
ActionCoordinator->ExecuteBatch(
	Request,
	FBlueprintHelperReviewBatchActionCompletedDelegate::CreateSP(
		this,
		&SBlueprintHelperReviewPanel::HandleReviewBatchActionCompleted));
```

Filter by selected asset before sending the request:

```cpp
Request.Changes = FilterChangesForSelectedAsset(Request.Changes, SelectedAssetPath);
```

- [ ] **Step 5: Replace OnRejectAll loop**

Use the same batch request with `Action = Reject`. Reuse existing reject options preparation for selected asset, but prepare once per batch:

```cpp
Request.Action = EBlueprintHelperReviewCoordinatedAction::Reject;
Request.RejectOptions = PreparedBatchRejectOptions;
```

Reject lifecycle root ordering:

1. Group asset lifecycle roots first.
2. Reject root records before child records.
3. Only remove children from model after root reject succeeds.
4. Failed roots leave children visible and marked with the failure message.

- [ ] **Step 6: Apply batch result once**

Add:

```cpp
void SBlueprintHelperReviewPanel::HandleReviewBatchActionCompleted(
	const FBlueprintHelperReviewBatchActionCompleted& Completed)
{
	for (const FBlueprintHelperReviewActionResult& Result : Completed.Results)
	{
		ReviewPanelModel->ApplyActionResult(
			Result.ChangeId,
			Result.bSucceeded,
			Result.Message);
	}
	RequestPendingReviewRefresh(TEXT("batch_action_completed"), Completed.StoreChangedEvent);
	RefreshChangeTreeWidget();
	RefreshMainWorkspaceForAffectedAsset(Completed.StoreChangedEvent.AssetPaths);
	ShowReviewActionNotification(
		TEXT("batch"),
		FString::Printf(TEXT("Batch %d/%d"), Completed.SucceededCount, Completed.RequestedCount),
		Completed.FailedCount == 0 ? EReviewActionNotificationState::Success : EReviewActionNotificationState::Fail,
		true,
		false);
}
```

- [ ] **Step 7: Add batch tests**

Test setup:

1. Create two review records.
2. Each record contains two pending visible changes.
3. Call AcceptAll through coordinator batch.
4. Assert each record was loaded once and saved once.
5. Assert one changed event contains both record ids and four change ids.

Expected assertions:

```cpp
TestEqual(TEXT("Requested count"), Completed.RequestedCount, 4);
TestEqual(TEXT("Succeeded count"), Completed.SucceededCount, 4);
TestEqual(TEXT("Changed record count"), Completed.StoreChangedEvent.ReviewRecordIds.Num(), 2);
TestEqual(TEXT("Changed change count"), Completed.StoreChangedEvent.ChangeIds.Num(), 4);
TestFalse(TEXT("Batch does not require full reload"), Completed.StoreChangedEvent.bRequiresFullReload);
```

- [ ] **Step 8: Verify P4**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.Review.ActionCoordinator; BlueprintHelper.Review.Action; BlueprintHelper.Review.PendingIndex; Quit' -TestExit='Automation Test Queue Empty'
```

Expected:

```text
BlueprintHelper.Review.ActionCoordinator: 0 failed
BlueprintHelper.Review.Action: 0 failed
BlueprintHelper.Review.PendingIndex: 0 failed
```

Manual editor check:

1. Load Review tab with thousands of pending changes.
2. Select one asset and click `AcceptAllAssetChange`.
3. Confirm one batch notification and one batch store changed event.
4. Click global `RejectAll` in a test asset set.
5. Confirm lifecycle root ordering remains correct and UI does not freeze from per-item refresh loops.

Suggested checkpoint message:

```text
修复内容：
1. AcceptAll / RejectAll 改为按 ReviewRecord 分组批处理，避免逐条全量 snapshot 和逐条刷新
```

---

## 2026-06-01 最新状态 / 验证结果

### 已落地性能优化

- Paged pending load / 分页滚动加载已补入 ReviewPanel pending load 链路：async worker 完成后不再一次性把全部 `ChangeItems` 应用到 UI，滚动分页继续加载后续 rows；`AcceptAll` / `RejectAll` 通过完整 pending 数据源执行，避免只处理当前已加载页。
- Pending load worker 与低速 validity sweep 已继续收敛：pending load 走 worker 侧轻量 summary query / sort，GameThread 只做分页 model apply；validity sweep 继续保持 service/coordinator 生命周期，真实资产、变量、函数、graph block 等 anchor 检查按预算回到 GameThread 执行，不放入 Slate widget 本地分支。
- RowHighlight no-op broadcast 已跳过：`FBlueprintHelperReviewRowHighlightModel::RebuildSurfaceState` 先比较语义状态，未变化时保留 revision 且不广播；Reject queue 状态刷新不再重建 surface highlight。该修复用于消除 Detail diff/graph 区域 flicker，以及 DebugBundle `Revision` 刷屏。
- Reject service benchmark 当前样本未复现 10s 到 1min 级后端耗时：`graph_block` `reject_targets_ms` 约 `60.29ms`，`variable` `reject_targets_ms` 约 `64.80ms`。
- 已新增真实 ReviewPanel Reject click timing 链路，并写入结构化 DebugBundle 事件 `review_reject_timing`；同时保留 `RejectPerf ...` 文本日志，覆盖从 UI intent、queued、prepare、mutation、store changed、pending load、success feedback 到 panel refresh 的阶段耗时。

### DebugBundle 只读分析结果

- 最近 DebugBundle：`D:/UEProjects/Template/Saved/BlueprintHelper/Debug/ReviewPanelBundles/review_panel_20260601_062934.json`
- `updated_at`: `2026-06-01T06:30:34.852Z`
- 本地文件时间：`2026-06-01 14:30:34.884 +08:00`
- 事件计数：找到 `55` 条 `review_reject_timing` 和 `55` 条 `RejectPerf`，对应 `5` 个 `change_id`。

| Change | Final total | 最慢单阶段 |
|---|---:|---|
| `variable RPTimingCounter` | `1142.23ms` | `panel_refresh_applied 292.01ms` |
| `graph body task_E37... step_4` | `2188.77ms` | `mutation_started 631.94ms` |
| `graph body task_3480... step_6` | `3610.74ms` | `mutation_started 1041.20ms` |
| `variable RPTimingThreshold` | `1504.42ms` | `panel_refresh_applied 315.18ms` |
| `asset_factory create asset` | `2307.54ms` | `store_changed_event 934.40ms` |

- 全局最慢单阶段：`mutation_started 1041.20ms`，对应 `graph body task_3480... step_6`。
- 按阶段累计耗时最高项：`mutation_started 2239.64ms`、`panel_refresh_applied 1993.06ms`、`store_changed_event 1565.45ms`。
- 当前证据说明真实点击耗时已经可分段定位：后端 service benchmark 约 60ms，但真实 ReviewPanel 链路仍在 `mutation_started`、`panel_refresh_applied`、`store_changed_event` 阶段出现更高墙钟耗时；后续优化应继续沿 `review_reject_timing` 分段，而不是只看 service benchmark。

### 组件层级测试进度

- 当前组件层级测试已创建/执行到二级组件。
- 三级组件在同一 TaskPlan 内的父子依赖 preview 被 `parent_component_not_found: CH_A_GrandChildScene` 阻断。
- 下一步不应在同一大 TaskPlan 内继续堆叠父子依赖；应拆成更细粒度任务，让父组件创建、保存、读回后再执行子组件 preview / execute。

---

## Final Verification Matrix

Run after all five tasks:

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex
```

Expected:

```text
Result: Succeeded
```

Run Review automation:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NullRHI -ExecCmds='Automation RunTests BlueprintHelper.Review; Quit' -TestExit='Automation Test Queue Empty'
```

Expected:

```text
BlueprintHelper.Review: 0 failed
```

Manual performance acceptance:

| Scenario | Acceptance |
|---|---|
| Open BlueprintHelperWidget on non-Review tab | No synchronous `QueryReviewRecords` full scan during open. |
| First click Review tab | UI remains responsive while pending rows load. |
| Single Accept | No full pending reload; affected row removed or updated. |
| Single Reject | Prepare may run async; mutation result updates affected rows only. |
| AcceptAll / RejectAll | One batch event per click; no per-item full snapshot loop. |
| 3712 record local data set | Logs show index/summary load path instead of 179 MB full parse on GameThread. |
| External asset changes | Deleted asset, deleted/renamed Blueprint variable, deleted/renamed function, missing graph/component/widget/DataTable row are removed from pending Review through low-speed validity sweep without a long GameThread block. |

## Self-review

- Spec coverage: all 5 requested priorities are represented by Task 1 through Task 5 and marked required; the new low-speed ReviewEvent validity scan is explicitly added to P2.
- Boundary coverage: UI keeps display/input ownership only; store/index/coordinator/model own data and lifecycle work.
- Worker safety: worker tasks only read index/ReviewRecord JSON summaries; UObject asset-anchor validation and Reject asset mutation remain GameThread with explicit budgets.
- Review v2: no Review v1, Transaction fallback, legacy anchor fallback, or layout diff changes are introduced.
- Placeholder scan: this document contains no unresolved placeholder markers and no unspecified implementation bucket.

## Final suggested commit message

```text
变更需求：
1. ReviewPanel 性能改造计划：懒构造、pending index、异步加载、低速有效性扫描、增量 Acc/Rej、批量 AccAll/RejAll
```

Manual commands after implementation files are complete:

```powershell
git status --short
git add BlueprintHelper/Develop/Plan/BlueprintHelper_ReviewPanel_Performance_ImplementationPlan_20260531_CN.md
git commit -m "docs: plan ReviewPanel performance architecture"
```
