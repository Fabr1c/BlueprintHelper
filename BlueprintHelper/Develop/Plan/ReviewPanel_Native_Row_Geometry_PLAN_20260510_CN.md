# ReviewPanel Native Row Geometry Implementation Plan

> 2026-05-14 状态转移：本文中的未达期待、待验证项和阻塞项已迁移到 [BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md](BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md)。本文保留为历史上下文；开放项跟踪迁移完成，后续当前状态以总账为准。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 ReviewPanel 的 Blueprint 组件、WidgetTree、我的蓝图三个内置面板生成真实行级 Diff 高亮，不再用带文字的覆盖卡片伪装成精准定位。

**Architecture:** Blueprint 组件面板继续复用 UE 的 `SSubobjectBlueprintEditor`，通过公开 tree/node API 定位真实 Slate row。WidgetTree 和我的蓝图改为 ReviewPanel 自己拥有的只读 `STreeView` presenter，row 由 ReviewPanel 生成并注册 geometry，Diff overlay 只绘制半透明背景框，不绘制文字。Graph、DataTable、DataAsset 现有主工作区逻辑不在本计划内重写。

**Tech Stack:** Unreal Engine 5.6 Slate, `SSubobjectBlueprintEditor`, `STreeView`, `SGraphEditor`, BlueprintHelper ReviewPanel presenters, UE automation tests.

---

## Execution Status - 2026-05-10 01:02

- [x] Stage 1 shared row overlay contract implemented: row geometry frames are textless/actionless; built-in panel text fallback is hidden for Components/MyBlueprint/UMGWidgetTree/Details.
- [x] Stage 2 Components implementation compiled: ReviewPanel stores `SSubobjectBlueprintEditor`, resolves `component:*` targets through `FindSlateNodeForVariableName`, and emits `mode=subobject_row` on real row geometry.
- [x] Stage 3 WidgetTree implementation compiled: Widget Blueprint structure panel uses owned readonly `STreeView` data from `WidgetBlueprint->WidgetTree` and registers `umg_widget:*` aliases as `mode=owned_tree_row`.
- [x] Stage 4 MyBlueprint first implementation compiled: ReviewPanel now uses owned readonly MyBlueprint `STreeView` rows, plus review-anchor rows for missing Blueprint data, and registers aliases as `mode=owned_tree_row`.
- [x] Build verified after final `SMyBlueprint` cleanup: `Build.bat MrStoneEditor Win64 Development -Project="G:/UnrealPractise/MrStone/MrStone.uproject" -WaitMutex -NoHotReload` -> `Result: Succeeded`.
- [x] Automation verified: after final cleanup `BlueprintHelper.Review.UI` -> 30 passed, 0 failed; overlay regressions `PresenterOverlayHidesBuiltInPanelFallbackWithoutSlateRowGeometry` and `PresenterOverlayUsesStableSlateRowGeometry` also passed.
- [ ] Manual smoke still required for live row geometry: BP actor component/MyBlueprint rows, Widget Blueprint WidgetTree rows, Graph diff block bounds, and ReviewPanel debug export.

## Scope

### In Scope

- Blueprint Components 面板：继续使用 `SSubobjectBlueprintEditor`，为 `component:*` target 读取真实 row geometry。
- Widget Blueprint 结构面板：用 ReviewPanel 自建只读 `STreeView` 替换当前 summary list，数据来自 `WidgetBlueprint->WidgetTree`。
- My Blueprint 面板：用 ReviewPanel 自建只读 presenter 替换直接嵌入 `SMyBlueprint`，覆盖变量、函数、事件、dispatcher、signature review anchors。
- 内置面板 Diff 样式：只画半透明背景和可选边线，不显示 review text。
- Debug 输出明确区分 `subobject_row`、`owned_tree_row`、`hidden`、`pending_scroll`、`no_visible_row`。
- 自动化覆盖 surface routing、row key 注册、fallback 抑制、Graph 不回退。

### Out Of Scope

- DataTable、DataAsset、UserDefinedStruct 原生 Row 复用。
- Graph diff bounds 算法重写。
- Accept、Reject、RejectAll 行为重写。
- Review/Snapshots 保存策略和强制保存策略。
- 真实 UE 编辑器 dock tab 复用。

## Current Problems

- Components、WidgetTree、MyBlueprint、Details 等内置面板目前在没有稳定 row geometry 时已经隐藏 text fallback，但仍缺少可稳定定位的真实 row 来源。
- `SSubobjectBlueprintEditor` 是真实 UE 组件面板，但当前 presenter 没有保留 widget 指针，也没有用 `FindSlateNodeForVariableName` 和 `TreeWidget->WidgetFromItem` 做 row 定位。
- WidgetTree presenter 现在是 summary text list，不是 row-owned tree，无法稳定覆盖具体 widget 行。
- MyBlueprint 当前直接嵌 `SMyBlueprint`，内部 `SGraphActionMenu` 私有，无法可靠拿到 action row widget。
- 用户要求组件、WidgetTree、我的蓝图的 Diff 框不带文字，并且像代码 review 一样在行上画半透明背景。

## Design Contracts

### Row Geometry Contract

- 只有真实可见 Slate row 可以生成精准 Diff 框。
- Row 不可见、被过滤、未完成 layout、或无法映射 target 时，不绘制假框。
- 对 Components row，允许第一次 miss 时请求 scroll into view 并在下一次 tick 刷新 overlay。
- 对 WidgetTree 和 MyBlueprint row，presenter 自己拥有 row，生成 row 时注册 key，因此应避免 text fallback。
- 所有内置面板 fallback 均为 hidden debug，不渲染文字卡片。

### Diff Visual Contract

- Diff 框内容为空 widget，只负责画色块。
- added 使用绿色，modified 使用黄/琥珀色，removed 使用红色。
- 默认行级 padding 为 10 px。
- 背景透明度为 0.6，选中态可以强化边线或轻微提高 fill，但不遮挡文本可读性。
- 框必须 clip 到所属 surface，不跨 panel 覆盖。

### Target Key Contract

- Components:
  - `component:SmokeComp`
  - `component_property:SmokeComp.PropertyName`
  - fallback alias: `SmokeComp`
- WidgetTree:
  - `umg_widget:SmokeText`
  - `umg_widget_property:SmokeText.Text`
  - fallback alias: `SmokeText`
- MyBlueprint:
  - `blueprint_variable:SmokeHP`
  - `signature:BH_SmokeFunc`
  - `signature:ReceiveAnyDamage`
  - `event_dispatcher:OnSomething`
  - fallback alias: target name only.

## File Structure

### Modify

- `Source/BlueprintHelper/Public/UI/Review/SBlueprintHelperReviewPanel.h`
  - Add persistent presenter state for Components, WidgetTree, and MyBlueprint.
  - Add refresh/deferred geometry flags if needed.

- `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
  - Pass presenter state into content and overlay builders.
  - Keep layout ownership unchanged.
  - Rebuild overlays after deferred component scroll/layout if requested.

- `Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewSurfacePresenter.h`
  - Add Components presenter state.
  - Add MyBlueprint owned presenter state and row item type.
  - Extend surface presenter args with optional row locator state.

- `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfacePresenter.cpp`
  - Implement `SSubobjectBlueprintEditor` row geometry resolution.
  - Replace direct `SMyBlueprint` content with owned readonly MyBlueprint tree.
  - Remove built-in panel text fallback paths for Components/MyBlueprint.

- `Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewAssetPresenters.h`
  - Add WidgetTree presenter state and row item type.

- `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewAssetPresenters.cpp`
  - Replace WidgetTree summary list with owned readonly `STreeView`.
  - Register WidgetTree rows by target key aliases.

- `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`
  - Add or update automation tests for row-key registration, surface routing, hidden fallback, and widget construction.

- `Source/BlueprintHelper/BlueprintHelper.Build.cs`
  - Only modify if the implementation needs an additional public UE editor module. Expected no new module for Components and MyBlueprint; WidgetTree may already have UMG dependencies through existing code.

### Do Not Modify

- `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewGraphBounds.cpp`
- `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewStoreService.cpp`, except if a test proves target key normalization is wrong.
- DataTable/DataAsset presenter internals, except for compile fixes caused by shared signature changes.

## Implementation Plan

### Stage 1: Shared Row Geometry Host

**Files:**
- Modify: `Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewSurfacePresenter.h`
- Modify: `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfacePresenter.cpp`
- Test: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [x] **Step 1: Add explicit geometry modes**

Add debug mode names used by all presenters:

```cpp
static constexpr const TCHAR* ReviewGeometryModeSlateRow = TEXT("slate_row");
static constexpr const TCHAR* ReviewGeometryModeSubobjectRow = TEXT("subobject_row");
static constexpr const TCHAR* ReviewGeometryModeOwnedTreeRow = TEXT("owned_tree_row");
static constexpr const TCHAR* ReviewGeometryModeHidden = TEXT("hidden");
```

Keep existing `ReviewFrameGeometry` log format, but make the mode field reflect the resolver that produced geometry.

- [x] **Step 2: Make built-in panel fallback hidden-only**

In the helper that builds panel Diff frames, keep this behavior:

```cpp
if (ShouldSuppressTextReviewListFallback(Surface))
{
    LogReviewFrameGeometryHidden(...);
    return SNullWidget::NullWidget;
}
```

Ensure `Components`, `MyBlueprint`, `UMGWidgetTree`, and `Details` are included in the suppress list. Do not suppress DataTable/DataAsset center workspace fallback in this plan.

- [x] **Step 3: Make row frames textless**

For any `slate_row`, `subobject_row`, or `owned_tree_row` frame, call `BuildDiffFrame` with empty content:

```cpp
SNew(SBox)
.WidthOverride(1.0f)
.HeightOverride(1.0f)
```

Do not include readable change title, target kind, transaction id, or action buttons inside row overlays.

- [x] **Step 4: Add tests for no text fallback in built-in panels**

Add an automation test named:

```cpp
BlueprintHelper.Review.VisibleChange.BuiltInPanelsDoNotRenderTextFallbackWithoutRowGeometry
```

The test creates visible changes for `Components`, `MyBlueprint`, `UMGWidgetTree`, and `Details`, constructs overlay without registered row geometry, and asserts debug contains:

```text
ReviewFrameGeometry change=... surface=components mode=review_list result=hidden
ReviewFrameGeometry change=... surface=my_blueprint mode=review_list result=hidden
ReviewFrameGeometry change=... surface=umg_widget_tree mode=review_list result=hidden
ReviewFrameGeometry change=... surface=details mode=review_list result=hidden
```

Expected: no generated fallback widget containing the readable title.

### Stage 2: Native Components Row Geometry

**Files:**
- Modify: `Source/BlueprintHelper/Public/UI/Review/SBlueprintHelperReviewPanel.h`
- Modify: `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
- Modify: `Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewSurfacePresenter.h`
- Modify: `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfacePresenter.cpp`
- Test: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [x] **Step 1: Store the native components widget**

Add presenter state:

```cpp
struct FBlueprintHelperReviewComponentsPresenterState
{
    TSharedPtr<SSubobjectBlueprintEditor> SubobjectEditor;
    bool bRequestedDeferredGeometryRefresh = false;
};
```

Store this state on `SBlueprintHelperReviewPanel` and pass it to Components content and overlay builders.

- [x] **Step 2: Return `SSubobjectBlueprintEditor` through state**

Change Components content build to assign the native widget:

```cpp
TSharedRef<SSubobjectBlueprintEditor> Widget =
    SAssignNew(State.SubobjectEditor, SSubobjectBlueprintEditor)
    .ObjectContext(ActorCDO)
    .AllowEditing(false)
    .HideComponentClassCombo(true);
return Widget;
```

When no Blueprint actor CDO exists, reset `State.SubobjectEditor`.

- [x] **Step 3: Resolve component target to subobject tree node**

Implement a resolver that extracts component names from:

```text
component:SmokeComp
component_property:SmokeComp.RelativeLocation
SmokeComp
```

Then call:

```cpp
const FName VariableName(*ComponentName);
FSubobjectEditorTreeNodePtrType Node =
    State.SubobjectEditor->FindSlateNodeForVariableName(VariableName);
```

If that fails and the target has a known object path, attempt `FindSlateNodeForObject`.

- [x] **Step 4: Resolve visible row geometry**

If node exists:

```cpp
TSharedPtr<SSubobjectEditorDragDropTree> Tree = State.SubobjectEditor->GetDragDropTree();
TSharedPtr<ITableRow> Row = Tree.IsValid() ? Tree->WidgetFromItem(Node) : nullptr;
```

If `Row` is valid, convert row absolute geometry into the overlay local space and return mode `subobject_row`. Apply 10 px padding.

If `Row` is missing, call:

```cpp
Tree->RequestScrollIntoView(Node);
State.bRequestedDeferredGeometryRefresh = true;
```

Log:

```text
ReviewFrameGeometry change=... surface=components mode=subobject_row result=hidden reason=pending_scroll target="component:SmokeComp"
```

- [x] **Step 5: Refresh after layout**

When `bRequestedDeferredGeometryRefresh` is true, register an active timer or deferred refresh from `SBlueprintHelperReviewPanel` that rebuilds the Components overlay after Slate has a row widget.

Expected behavior:

- First frame may log `pending_scroll`.
- Next refresh draws `mode=subobject_row result=shown`.
- If still no row, log `no_visible_row`.

- [ ] **Step 6: Add component geometry tests**

Add tests:

```cpp
BlueprintHelper.Review.UI.ComponentsPresenterKeepsNativeSubobjectEditor
BlueprintHelper.Review.VisibleChange.ComponentTargetUsesSubobjectRowResolver
```

The first test constructs a Blueprint actor review context and asserts Components content is not placeholder. The second test exercises target-name parsing and hidden behavior when row widget is not visible. Live geometry drawing is verified manually because Slate row visibility depends on editor layout.

### Stage 3: Owned WidgetTree STreeView Presenter

**Files:**
- Modify: `Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewAssetPresenters.h`
- Modify: `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewAssetPresenters.cpp`
- Modify: `Source/BlueprintHelper/Public/UI/Review/SBlueprintHelperReviewPanel.h`
- Modify: `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
- Test: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [x] **Step 1: Add WidgetTree row state**

Add:

```cpp
struct FBlueprintHelperReviewWidgetTreeRowItem
{
    FName WidgetName;
    FString WidgetClass;
    int32 Depth = 0;
    TArray<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>> Children;
};

struct FBlueprintHelperReviewWidgetTreePresenterState
{
    TArray<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>> RootItems;
    TSharedPtr<STreeView<TSharedPtr<FBlueprintHelperReviewWidgetTreeRowItem>>> TreeView;
};
```

Store state on `SBlueprintHelperReviewPanel`.

- [x] **Step 2: Build rows from `WidgetBlueprint->WidgetTree`**

Use `UWidgetBlueprint*` from `ReviewAssetContext.Blueprint`. If `WidgetTree` is missing, show the existing placeholder.

Traverse from `WidgetTree->RootWidget`, then child widgets for panels:

```cpp
if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
{
    for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
    {
        UWidget* Child = Panel->GetChildAt(Index);
        BuildChildItem(Child, ParentItem, Depth + 1);
    }
}
```

Also include named widgets discovered through `WidgetTree->GetAllWidgets` if they are not reachable from root, under a read-only group row named `Unparented Widgets`.

- [x] **Step 3: Generate row widgets and register geometry**

Each `OnGenerateRow` returns an `STableRow` with icon, widget name, and class name. Register aliases:

```cpp
RegisterRow(AssetPath, EBlueprintHelperReviewSurface::UMGWidgetTree, FString::Printf(TEXT("umg_widget:%s"), *Name), RowWidget);
RegisterRow(AssetPath, EBlueprintHelperReviewSurface::UMGWidgetTree, FString::Printf(TEXT("umg_widget_property:%s"), *Name), RowWidget);
RegisterRow(AssetPath, EBlueprintHelperReviewSurface::UMGWidgetTree, Name, RowWidget);
```

Do not register summary lines such as `Widget count`.

- [x] **Step 4: Keep selection and expansion deterministic**

Expand all rows by default for review mode. When selected change is a WidgetTree change, request scroll into view for the matching row and refresh overlay on the next tick.

- [x] **Step 5: Add WidgetTree tests**

Add tests:

```cpp
BlueprintHelper.Review.UI.WidgetTreePresenterBuildsOwnedTreeFromWidgetBlueprint
BlueprintHelper.Review.VisibleChange.WidgetTreeTargetRegistersOwnedRowKeys
BlueprintHelper.Review.UI.WidgetTreeChangeDoesNotRouteToDetails
```

Expected:

- `SmokeText` appears as a row item.
- `umg_widget:SmokeText` and `umg_widget_property:SmokeText` resolve to the same row key.
- Details route remains hidden for UMG widget changes.

### Stage 4: Owned MyBlueprint Presenter

**Files:**
- Modify: `Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewSurfacePresenter.h`
- Modify: `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfacePresenter.cpp`
- Modify: `Source/BlueprintHelper/Public/UI/Review/SBlueprintHelperReviewPanel.h`
- Modify: `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
- Test: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [x] **Step 1: Add MyBlueprint row state**

Add:

```cpp
enum class EBlueprintHelperReviewMyBlueprintRowKind : uint8
{
    Category,
    Variable,
    Function,
    Event,
    Dispatcher,
    Graph,
    ReviewAnchor
};

struct FBlueprintHelperReviewMyBlueprintRowItem
{
    EBlueprintHelperReviewMyBlueprintRowKind Kind = EBlueprintHelperReviewMyBlueprintRowKind::ReviewAnchor;
    FName Name;
    FString DisplayText;
    FString TargetKey;
    TArray<TSharedPtr<FBlueprintHelperReviewMyBlueprintRowItem>> Children;
};

struct FBlueprintHelperReviewMyBlueprintPresenterState
{
    TArray<TSharedPtr<FBlueprintHelperReviewMyBlueprintRowItem>> RootItems;
    TSharedPtr<STreeView<TSharedPtr<FBlueprintHelperReviewMyBlueprintRowItem>>> TreeView;
};
```

- [x] **Step 2: Populate rows from Blueprint data**

Build category rows:

```text
Graphs
Functions
Events
Variables
Dispatchers
Review Anchors
```

Populate from:

- `Blueprint->UbergraphPages`
- `Blueprint->FunctionGraphs`
- `Blueprint->MacroGraphs`
- `Blueprint->DelegateSignatureGraphs`
- `Blueprint->NewVariables`

For variables, use `FBPVariableDescription::VarName`. For delegate variables, detect multicast delegate pin types and place them under Dispatchers.

- [x] **Step 3: Add rows for review anchors not present in Blueprint data**

Scan current visible changes for `Surface == MyBlueprint`. If an anchor key is not represented by real Blueprint data, add a row under `Review Anchors` with the user-facing title from `BuildReadableChangeTitle`.

This prevents override signatures like `ReceiveAnyDamage` from disappearing when UE does not expose an already-created graph row.

- [x] **Step 4: Generate row widgets and register aliases**

Each generated row registers:

```cpp
RegisterRow(AssetPath, EBlueprintHelperReviewSurface::MyBlueprint, TargetKey, RowWidget);
RegisterRow(AssetPath, EBlueprintHelperReviewSurface::MyBlueprint, Name.ToString(), RowWidget);
```

For signature rows, register both:

```text
signature:BH_SmokeFunc
BH_SmokeFunc
```

For variables:

```text
blueprint_variable:SmokeHP
SmokeHP
```

For dispatchers:

```text
event_dispatcher:OnDamage
OnDamage
```

- [x] **Step 5: Remove direct `SMyBlueprint` dependency from ReviewPanel content**

Stop constructing `SMyBlueprint` in review mode. Keep `SKismetInspector` details read-only, but do not rely on `SMyBlueprint` for row geometry.

Expected result:

- MyBlueprint row geometry is owned by ReviewPanel.
- Row target matching is deterministic.
- Internal `SGraphActionMenu` private state is no longer part of the ReviewPanel contract.

- [ ] **Step 6: Add MyBlueprint tests**

Add tests:

```cpp
BlueprintHelper.Review.UI.MyBlueprintPresenterBuildsOwnedRows
BlueprintHelper.Review.VisibleChange.MyBlueprintVariableRegistersOwnedRowKey
BlueprintHelper.Review.VisibleChange.MyBlueprintSignatureRegistersOwnedRowKey
BlueprintHelper.Review.VisibleChange.MyBlueprintMissingOverrideCreatesReviewAnchorRow
```

Expected:

- `blueprint_variable:SmokeHP` resolves to a row.
- `signature:BH_SmokeFunc` resolves to a row.
- `signature:ReceiveAnyDamage` has a deterministic row even if not present in `FunctionGraphs`.

### Stage 5: Integration Refresh And Debug

**Files:**
- Modify: `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp`
- Modify: `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfacePresenter.cpp`
- Test: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`

- [x] **Step 1: Centralize overlay rebuild after selection**

After `SelectedChange` changes, rebuild content before overlay for surfaces whose row list depends on the selected asset:

```text
ComponentsContentBox -> ComponentsDiffStackBox
MyBlueprintContentBox -> MyBlueprintDiffStackBox
MainWorkspaceContent -> MainWorkspaceDiffStackBox
DetailsContentBox -> DetailsDiffStackBox
```

WidgetTree and MyBlueprint owned presenters must register rows before `BuildReviewListOverlay` tries to resolve geometry.

- [x] **Step 2: Add debug summary per surface**

For every selected change, include:

```text
ReviewFrameGeometry change=<id> surface=<surface> mode=<subobject_row|owned_tree_row|slate_row|review_list> result=<shown|hidden> reason=<reason> target="<target>"
```

Required reasons:

- `target_match`
- `pending_scroll`
- `no_visible_row`
- `no_row_key`
- `no_stable_slate_geometry`
- `partial_slate_row_geometry`

- [x] **Step 3: Ensure Details no longer owns primary overlays**

Details should remain auxiliary. Add a regression assertion that UMG, component, and MyBlueprint targets do not produce `surface=Details result=shown`.

### Stage 6: Verification

**Files:**
- Test: `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp`
- Manual smoke assets:
  - `/Game/BlueprintHelper/Smoke/BP_SmokeActor`
  - `/Game/BlueprintHelper/Smoke/WBP_Smoke`
  - `/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke`

- [x] **Step 1: Run format check**

Run:

```powershell
git diff --check
```

Expected:

```text
no whitespace errors
```

- [x] **Step 2: Build plugin/editor**

Run:

```powershell
F:\UE_5.6\Engine\Build\BatchFiles\Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReload
```

Expected:

```text
Result: Succeeded
```

If build fails on unrelated `BlueprintHelperRequestValidator::GetConfiguredToken` or locked DLL issues, record it as external build blocker and do not mark this plan verified.

- [x] **Step 3: Run targeted Review UI automation**

Run grouped Review UI tests:

```powershell
F:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe "G:\UnrealPractise\MrStone\MrStone.uproject" -ExecCmds="Automation RunTests BlueprintHelper.Review.UI;Quit" -unattended -nop4 -nosplash
```

Expected:

```text
0 failed
```

- [ ] **Step 4: Manual smoke for Blueprint actor**

Open ReviewPanel and select `/Game/BlueprintHelper/Smoke/BP_SmokeActor`.

Verify:

- `SmokeComp` component change highlights the real Components row.
- `SmokeHP` variable change highlights the MyBlueprint row.
- `BH_SmokeFunc`, `BH_SmokeCustomEvent`, `ReceiveAnyDamage` signature rows highlight in MyBlueprint.
- `ReplaceBlueprintGraph` highlights the center Graph workspace.
- Built-in row highlights have no text inside the Diff frame.

- [ ] **Step 5: Manual smoke for Widget Blueprint**

Open ReviewPanel and select `/Game/BlueprintHelper/Smoke/WBP_Smoke`.

Verify:

- Components slot title is `Widget Tree`.
- `SmokeText` change highlights the WidgetTree row.
- MyBlueprint remains present below WidgetTree.
- Center workspace remains Graph for Widget Blueprint.
- Details does not show primary UMG overlays.

- [ ] **Step 6: Debug export verification**

Export ReviewPanel debug data.

Verify:

- Contains `ReviewFrameGeometry ... mode=subobject_row result=shown` for component row.
- Contains `ReviewFrameGeometry ... mode=owned_tree_row result=shown` for WidgetTree and MyBlueprint rows.
- Does not contain `debug_export_refs`.
- Does not contain built-in surface `mode=review_list result=shown` for Components/MyBlueprint/UMGWidgetTree/Details.

## Risk Register

| Risk | Severity | Mitigation |
|---|---:|---|
| `SSubobjectBlueprintEditor` row not visible because tree virtualizes rows | Medium | Request scroll into view, defer overlay rebuild one tick, hide if still unavailable |
| Component target name differs from SCS variable name | Medium | Support `component:*`, property target prefix stripping, and object fallback |
| WidgetTree children are not all reachable from root | Medium | Add unparented widget group from `WidgetTree->GetAllWidgets` |
| MyBlueprint override signatures are not present in Blueprint graph arrays | High | Add deterministic Review Anchor rows generated from visible changes |
| Owned MyBlueprint presenter visually differs from UE native MyBlueprint | Medium | Use UE style brushes, icon names, categories, and readonly tree behavior |
| Row geometry resolves before Slate layout | Medium | Build content before overlay, use active timer for deferred refresh |
| Existing automation cannot validate live geometry | Medium | Unit-test routing/key registration and rely on manual smoke for live Slate row placement |

## Expected End State

- Blueprint actor ReviewPanel uses real component rows, owned MyBlueprint rows, and Graph workspace.
- Widget Blueprint ReviewPanel uses owned WidgetTree rows, owned MyBlueprint rows, and Graph workspace.
- Components, WidgetTree, MyBlueprint, and Details do not render text cards as precise Diff overlays.
- Diff frames are translucent row backgrounds with no embedded text.
- Debug output tells whether a row was drawn from native component row, owned tree row, registered Slate row, or hidden because geometry was unavailable.
