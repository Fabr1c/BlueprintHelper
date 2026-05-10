# ReviewPanel C++ Coding Style 拆分清单

## 2026-05-10 执行进度同步

已修复本轮“Git/迁移损坏”的实际问题：Git object 检查无致命损坏，损坏点是 ReviewPanel 迁移半成品中的重复定义、文件内 helper class 和编译错误。当前 Review UI 迁移代码已经恢复到可编译状态。

已完成：
- `BlueprintHelperReviewSurfacePresenter.h/cpp` 已变为兼容 umbrella/空壳；Graph、Components、MyBlueprint、ObjectDetails、RowHighlight、SurfaceRouter、SurfaceFrameBuilder、GeometryProbe、SlateRowGeometryRegistry 已拆为独立文件对。
- `BlueprintHelperReviewAssetPresenters.h/cpp` 已变为兼容 umbrella/空壳；WidgetTree、DataTable、DataAsset、Structure、AssetPresenterTypes、PresenterWidgetUtils 已拆为独立文件对。
- `FBlueprintHelperReviewSurfaceFrameBuilderPrivate` 已删除，拆成 `BlueprintHelperReviewReadableTextUtils`、`BlueprintHelperReviewSurfaceFrameGeometryUtils`、`BlueprintHelperReviewSurfaceFrameDebugUtils`、`BlueprintHelperReviewSurfaceFrameWidgetUtils` 和 `SBlueprintHelperReviewDiffFrame`。
- `SBlueprintHelperReviewPanel.cpp` 已拆出 `SBlueprintHelperReviewPanelLayout.cpp` 和 `SBlueprintHelperReviewPanelDebug.cpp`。
- `FSBlueprintHelperReviewPanelLocalUtils` 已删除，拆成 `BlueprintHelperReviewPanelStyle` 和 `BlueprintHelperReviewPanelGeometryUtils`。

验证：
- `git diff --check -- Source\BlueprintHelper\Private\UI\Review Source\BlueprintHelper\Public\UI\Review` 通过；仅有 Git 行尾提示。
- UE build 已通过：`F:\UE_5.6\Engine\Build\BatchFiles\Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReload`。

仍待完成：
- `SBlueprintHelperReviewPanel.cpp` 当前仍约 1363 行，下一步继续拆 `ChangeTree`、`Actions`、`Surfaces`、`Details`。
- `BlueprintHelperReviewStoreServiceTests.cpp` 当前仍约 5369 行，测试文件拆分未开始。
- `BlueprintHelperReviewDataTablePresenter.cpp` 内仍有 `SBlueprintHelperReviewDataTableRow` 文件内 widget class，后续需要按同样规则拆独立文件对。
- 旧独立文件 `BlueprintHelperReviewGraphBounds.cpp`、`BlueprintHelperReviewGraphResolver.cpp`、`BlueprintHelperReviewAssetContext.cpp` 仍存在 namespace/static helper 遗留，暂按原“不建议拆分”清单保留，但如果严格执行 skill 的 namespace 规则，后续需要单列 P2 清理。

## 目标

按 `workspace/skills/cpp-coding-style/SKILL.md` 和 BlueprintHelper 当前 C++ 风格，把 ReviewPanel v2 近期快速迭代后膨胀的源码拆成更小的职责单元。本文只列拆分清单和执行边界，不直接改变行为。

## 判定标准

本轮按以下 C++ coding style 标准判断是否需要拆分：

1. 每个 class 必须有独立且匹配的 `.h/.cpp` 文件对。文件名按项目惯例使用去掉 `F`/`S`/`U` 前缀后的类名，例如 `FBlueprintHelperReviewRowHighlightModel` 对应 `BlueprintHelperReviewRowHighlightModel.h/cpp`。
2. 简单 struct 可以按清晰目的分组到 `Types.h/cpp` 文件对。行为变重的 struct 必须升级为独立 class 文件对。
3. Header 只放声明、include、forward declaration、enum、struct/class 声明和成员声明。禁止在 `.h` 内写函数体、构造函数体、getter/setter body、lambda helper 或 static helper implementation。
4. 禁止新增 `namespace`、anonymous namespace、namespace-scope `static` helper、namespace-scope free function。现有 private namespace 拆分时必须消除。
5. 原本会写成 private namespace/static helper 的逻辑，必须拆成按职责命名的 utility class，且每个 utility class 一个 `.h/.cpp` 文件对，静态函数只在 class 内声明，在 `.cpp` 定义。
6. 复杂 Slate UI 应拆成 presenter 或 widget class 文件对，父 Panel 只保留布局、选择、服务调用和刷新编排。
7. 测试按合同域拆分，避免一个 test cpp 同时覆盖 routing、store、action、panel 构造、graph bounds、row highlight、asset context。
8. 迁移顺序必须保持 UE build 可控：先拆纯移动/重命名，再拆公共类型，最后拆测试。

## 当前源码体量

| 文件 | 当前行数 | 结论 |
|---|---:|---|
| `Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp` | 5369 | 必须拆分 |
| `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfacePresenter.cpp` | 2 | 已拆分为空壳 |
| `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanel.cpp` | 1363 | 必须继续拆分 |
| `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanelLayout.cpp` | 391 | 已拆出 |
| `Source/BlueprintHelper/Private/UI/Review/SBlueprintHelperReviewPanelDebug.cpp` | 89 | 已拆出 |
| `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewAssetPresenters.cpp` | 2 | 已拆分为空壳 |
| `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewSurfaceFrameBuilder.cpp` | 208 | 已拆分 |
| `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewGraphBounds.cpp` | 571 | 暂缓，保持独立 |
| `Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewSurfacePresenter.h` | 14 | 已拆分为 umbrella |
| `Source/BlueprintHelper/Public/UI/Review/SBlueprintHelperReviewPanel.h` | 170 | 需要瘦身 |
| `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewDiffBlockNode.cpp` | 204 | 不拆 |
| `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewAssetContext.cpp` | 202 | 不拆 |
| `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewGraphResolver.cpp` | 60 | 不拆 |
| `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewDebugText.cpp` | 6 | 不拆 |

## P0 拆分清单

### 1. 拆 `BlueprintHelperReviewSurfacePresenter.h/cpp`

当前问题：

- 同时包含 geometry probe、row highlight model、surface router、frame builder、row registry、graph presenter、component presenter、MyBlueprint presenter、details presenter。
- Public header 对外暴露过多内部 UI 类型，后续任何 presenter 改动都会扩大编译影响。
- `.cpp` 内 private namespace 过大，且不符合当前 cpp-coding-style；静态 helper 必须改成 utility class。

目标文件：

| 新文件 | 职责 |
|---|---|
| `Public/UI/Review/BlueprintHelperReviewPresenterTypes.h` / `Private/UI/Review/BlueprintHelperReviewPresenterTypes.cpp` | 共享 presenter args、geometry anchor、delegates、基础 state，只放简单 struct/enum |
| `Public/UI/Review/BlueprintHelperReviewSurfaceRouter.h` / `Private/UI/Review/BlueprintHelperReviewSurfaceRouter.cpp` | class `FBlueprintHelperReviewSurfaceRouter`，负责 `RouteChangeToSurface`、main workspace/details ownership 判定、route debug summary |
| `Public/UI/Review/BlueprintHelperReviewRowHighlightModel.h` / `Private/UI/Review/BlueprintHelperReviewRowHighlightModel.cpp` | class `FBlueprintHelperReviewRowHighlightModel`，负责 Row highlight state、颜色、selected row action visibility、debug dedupe |
| `Public/UI/Review/SBlueprintHelperReviewGeometryProbe.h` / `Private/UI/Review/SBlueprintHelperReviewGeometryProbe.cpp` | class `SBlueprintHelperReviewGeometryProbe` |
| `Public/UI/Review/BlueprintHelperReviewSlateRowGeometryRegistry.h` / `Private/UI/Review/BlueprintHelperReviewSlateRowGeometryRegistry.cpp` | class `FBlueprintHelperReviewSlateRowGeometryRegistry`，负责 row widget 注册、row geometry 解析 |
| `Public/UI/Review/BlueprintHelperReviewSurfaceFrameBuilder.h` / `Private/UI/Review/BlueprintHelperReviewSurfaceFrameBuilder.cpp` | class `FBlueprintHelperReviewSurfaceFrameBuilder`，负责 Graph/legacy overlay frame、readable title、diff frame style |
| `Public/UI/Review/BlueprintHelperReviewGraphPresenter.h` / `Private/UI/Review/BlueprintHelperReviewGraphPresenter.cpp` | class `FBlueprintHelperReviewGraphPresenter`，负责 Graph presenter state、preview graph clone、diff block 插入、jump |
| `Public/UI/Review/BlueprintHelperReviewBlueprintComponentsPresenter.h` / `Private/UI/Review/BlueprintHelperReviewBlueprintComponentsPresenter.cpp` | class `FBlueprintHelperReviewBlueprintComponentsPresenter`，负责 `SSubobjectBlueprintEditor` 内容构建和 component row 定位 |
| `Public/UI/Review/BlueprintHelperReviewMyBlueprintPresenter.h` / `Private/UI/Review/BlueprintHelperReviewMyBlueprintPresenter.cpp` | class `FBlueprintHelperReviewMyBlueprintPresenter`，负责 MyBlueprint 只读 tree、Graphs/Functions/Macros/Variables/Event Dispatchers sections |
| `Public/UI/Review/BlueprintHelperReviewObjectDetailsPresenter.h` / `Private/UI/Review/BlueprintHelperReviewObjectDetailsPresenter.cpp` | class `FBlueprintHelperReviewObjectDetailsPresenter`，负责 Details content 和 property/details surface ownership |
| `Public/UI/Review/BlueprintHelperReviewGraphDiffBlockBuilder.h` / `Private/UI/Review/BlueprintHelperReviewGraphDiffBlockBuilder.cpp` | class `FBlueprintHelperReviewGraphDiffBlockBuilder`，替代 graph diff 相关 private namespace helper |
| `Public/UI/Review/BlueprintHelperReviewGraphJumpController.h` / `Private/UI/Review/BlueprintHelperReviewGraphJumpController.cpp` | class `FBlueprintHelperReviewGraphJumpController`，替代 graph jump 相关 private namespace helper |

保留兼容：

- 第一阶段保留 `BlueprintHelperReviewSurfacePresenter.h` 作为 umbrella include，内部只 include 新头文件。
- 第二阶段再逐步把 include 方替换为精确头文件。

检查项：

- [x] `BlueprintHelperReviewSurfacePresenter.cpp` 拆完后行数降到 300 行以内，或完全变成兼容空壳。
- [ ] 每个新 `.cpp` 只 include 自己需要的 UE/Slate headers。
- [x] Public header 不 include `SSubobjectBlueprintEditor.h`、`GraphEditor.h` 等重量级私有实现头。
- [ ] `BLUEPRINTHELPER_API` 只保留给测试或跨模块确实需要的类型。
- [x] 拆完后 `BlueprintHelperReviewSurfacePresenter.cpp` 不再包含 `namespace BlueprintHelperReviewSurfacePresenterPrivate`。
- [x] 不新增任何 namespace-scope static/free helper function；所有 helper 进入对应 utility class。
- [ ] 新增 `.h` 不包含函数体。

### 2. 拆 `BlueprintHelperReviewAssetPresenters.h/cpp`

当前问题：

- WidgetTree、DataTable、DataAsset、Structure presenter 共用一个 `.cpp`。
- 文件同时包含 UMG、DataTableEditor、PropertyEditor、StructureEditorUtils 依赖，编译耦合过大。
- 私有 helper 如 `BuildLine`、`BuildSummaryPanel`、target kind helper 同时服务多个 presenter，且目前适合被误写成 namespace/static helper。

目标文件：

| 新文件 | 职责 |
|---|---|
| `Public/UI/Review/BlueprintHelperReviewAssetPresenterTypes.h` / `Private/UI/Review/BlueprintHelperReviewAssetPresenterTypes.cpp` | WidgetTree/DataTable/DataAsset/Structure row state，只放简单 struct/enum |
| `Public/UI/Review/BlueprintHelperReviewWidgetTreePresenter.h` / `Private/UI/Review/BlueprintHelperReviewWidgetTreePresenter.cpp` | class `FBlueprintHelperReviewWidgetTreePresenter`，负责 WidgetBlueprint tree 读取、owned `STreeView`、UMG row highlight |
| `Public/UI/Review/BlueprintHelperReviewDataTablePresenter.h` / `Private/UI/Review/BlueprintHelperReviewDataTablePresenter.cpp` | class `FBlueprintHelperReviewDataTablePresenter`，负责 DataTable native cached rows、columns、row actions |
| `Public/UI/Review/BlueprintHelperReviewDataAssetPresenter.h` / `Private/UI/Review/BlueprintHelperReviewDataAssetPresenter.cpp` | class `FBlueprintHelperReviewDataAssetPresenter`，负责 DataAsset / GenericObject details rows、`IPropertyRowGenerator` |
| `Public/UI/Review/BlueprintHelperReviewStructurePresenter.h` / `Private/UI/Review/BlueprintHelperReviewStructurePresenter.cpp` | class `FBlueprintHelperReviewStructurePresenter`，负责 UserDefinedStruct summary 和 field rows |
| `Private/UI/Review/BlueprintHelperReviewPresenterWidgetUtils.h` / `Private/UI/Review/BlueprintHelperReviewPresenterWidgetUtils.cpp` | class `FBlueprintHelperReviewPresenterWidgetUtils`，负责私有共享 Slate 小组件、line builder、target kind helper |

保留兼容：

- 第一阶段保留 `BlueprintHelperReviewAssetPresenters.h` 作为 umbrella include。
- `DataAssetPresenter` 可以临时调用 `StructurePresenter`，但最终 Structure 应有独立 `ShouldShowChange` 和 `BuildContent`。

检查项：

- [ ] UMG presenter 不 include DataTableEditor headers。
- [ ] DataTable presenter 不 include PropertyEditor headers。
- [ ] DataAsset presenter 不 include WidgetBlueprint headers。
- [ ] Structure 分类不再落入 `generic_object` 的 UI 文案。
- [x] `BlueprintHelperReviewAssetPresenters.cpp` 不再包含 `namespace BlueprintHelperReviewAssetPresentersPrivate`。
- [x] `BuildLine`、`BuildSummaryPanel`、target kind helper 进入 `FBlueprintHelperReviewPresenterWidgetUtils` 或更窄 utility class，不保留 namespace-scope helper。

### 3. 拆 `SBlueprintHelperReviewPanel.h/cpp`

当前问题：

- Panel 同时负责布局、change tree、debug text、Accept/Reject、RejectAll、lifecycle root tree、asset load、details selection、row geometry resolve、overlay refresh。
- `FSBlueprintHelperReviewPanelLocalUtils` 在 `.cpp` 顶部过大，混入文本搜索、widget tree 递归、geometry 匹配。
- Header 持有所有 presenter state，导致 `SBlueprintHelperReviewPanel.h` 必须 include 大量 presenter headers。
- 当前 local utils class 没有独立文件对，不符合“每个 class 一个 `.h/.cpp` 文件对”的规则。

目标文件：

| 新文件 | 职责 |
|---|---|
| `Private/UI/Review/SBlueprintHelperReviewPanel.cpp` | `Construct` 和顶层布局 |
| `Private/UI/Review/SBlueprintHelperReviewPanelLayout.cpp` | `BuildFinalChangeSidebar`、`BuildComponentsPanel`、`BuildMyBlueprintPanel`、`BuildGraphPanel`、`BuildDetailsPanel`、`BuildDebugPanel` |
| `Private/UI/Review/SBlueprintHelperReviewPanelChangeTree.cpp` | change tree row、tree item 构建、asset lifecycle root nesting、selection next |
| `Private/UI/Review/SBlueprintHelperReviewPanelActions.cpp` | `OnAccept*`、`OnReject*`、RejectAll、lifecycle root cascade 调用 |
| `Private/UI/Review/SBlueprintHelperReviewPanelSurfaces.cpp` | surface content rebuild、overlay refresh、main workspace 选择 |
| `Private/UI/Review/SBlueprintHelperReviewPanelDetails.cpp` | Details object resolve、details selection、property displayed event |
| `Private/UI/Review/SBlueprintHelperReviewPanelDebug.cpp` | debug message append、dedupe、copy |
| `Private/UI/Review/BlueprintHelperReviewPanelGeometryUtils.h` / `Private/UI/Review/BlueprintHelperReviewPanelGeometryUtils.cpp` | class `FBlueprintHelperReviewPanelGeometryUtils`，负责 widget text 读取、row text matching、geometry recursion helper |
| `Private/UI/Review/BlueprintHelperReviewPanelStyle.h` / `Private/UI/Review/BlueprintHelperReviewPanelStyle.cpp` | class `FBlueprintHelperReviewPanelStyle`，负责 ReviewPanel 局部颜色和 style accessors，替代 `FSBlueprintHelperReviewPanelLocalUtils` 中的 inline static color |
| `Public/UI/Review/SBlueprintHelperReviewPanel.h` | 只保留 public Slate args、最小 private 状态和成员声明 |

注意：

- 可以把同一个类的 member function 分散到多个 `.cpp`，不改变 public ABI。
- 不使用 `SBlueprintHelperReviewPanel.Actions.cpp` 这种带双扩展的文件名，因为当前源码树没有这种命名惯例。

检查项：

- [ ] `SBlueprintHelperReviewPanel.cpp` 拆完后只保留 layout shell，不再包含 geometry recursion helper。
- [ ] `SBlueprintHelperReviewPanel.h` 通过 forward declaration 减少 presenter headers include。
- [ ] Accept/Reject 行为不依赖 UI row widget 指针。
- [ ] Debug 更新不在 resize/row geometry 刷新中重复刷文本框。
- [x] 删除 `FSBlueprintHelperReviewPanelLocalUtils`，或拆成独立 class 文件对。
- [ ] Panel 拆分出的 `.cpp` 不声明新的 helper class；需要 helper 时放入独立 utility class 文件对。

### 4. 拆 Review 测试文件

当前问题：

- `BlueprintHelperReviewStoreServiceTests.cpp` 超过 6000 行，覆盖 store、surface routing、row highlight、panel widget、asset context、action service、graph bounds。
- 新增回归时很难判断应放在哪个 section。
- 一个测试文件改动会触发大范围冲突。

目标文件：

| 新文件 | 覆盖范围 |
|---|---|
| `Private/Tests/Review/BlueprintHelperReviewTestUtils.h` / `Private/Tests/Review/BlueprintHelperReviewTestUtils.cpp` | class `FBlueprintHelperReviewTestUtils`，负责 MakeReviewBlueprint、MakeReviewDataTable、common visible change builder |
| `Private/Tests/Review/BlueprintHelperReviewRoutingTests.cpp` | surface routing、atomic target routing、visible change collapse |
| `Private/Tests/Review/BlueprintHelperReviewRowHighlightTests.cpp` | row highlight color、selected actions、row registry、geometry pending |
| `Private/Tests/Review/BlueprintHelperReviewPresenterTests.cpp` | Components/MyBlueprint/WidgetTree/DT/DA/ST presenter content |
| `Private/Tests/Review/BlueprintHelperReviewPanelTests.cpp` | Panel construct、tree nesting、asset selection、layout contract |
| `Private/Tests/Review/BlueprintHelperReviewStoreTests.cpp` | record identity、pending query、debug_case_ids、lifecycle metadata |
| `Private/Tests/Review/BlueprintHelperReviewActionTests.cpp` | Accept/Reject/RejectAll、lifecycle cascade、TOCTOU |
| `Private/Tests/Review/BlueprintHelperReviewGraphBoundsTests.cpp` | Graph bounds、target key、recorded bounds |

检查项：

- [ ] 每个测试文件控制在 800-1200 行以内。
- [ ] 测试名保持原 Automation path，不因文件移动改变。
- [ ] 公共 test utils 不依赖 Slate widget 构造，除非文件名明确是 UI test utils。
- [ ] 拆分后 `Automation RunTests BlueprintHelper.Review.UI` 和 `BlueprintHelper.Review.VisibleChange` 仍能按原组运行。
- [ ] 测试 helper 不使用 namespace 或 namespace-scope static function，统一进入 `FBlueprintHelperReviewTestUtils` 或更窄的 test utility class。

## P1 拆分清单

### 5. 拆 readable title / label 生成逻辑

当前位置：

- `FBlueprintHelperReviewSurfaceFrameBuilder::BuildReadableChangeTitle`
- ReviewPanel final change row 文案逻辑
- asset factory prefix stripping 测试

目标文件：

| 新文件 | 职责 |
|---|---|
| `Public/UI/Review/BlueprintHelperReviewReadableText.h` / `Private/UI/Review/BlueprintHelperReviewReadableText.cpp` | class `FBlueprintHelperReviewReadableText`，负责 final change title、asset kind suffix、target display name、package prefix stripping |

检查项：

- [ ] `asset_factory:_Game_BlueprintHelper_Smoke_BP_SmokeActor` 显示为 `新增了[BP_SmokeActor]蓝图资产`。
- [ ] `ReplaceBlueprintGraph` 显示为 `替换了[EventGraph]图表` 或等价图表文案。
- [ ] title 生成不依赖 Slate frame builder。

### 6. 拆 graph workspace diff block 插入逻辑

当前位置：

- `BlueprintHelperReviewSurfacePresenter.cpp` private namespace 中的 graph diff block helper。
- `BlueprintHelperReviewGraphBounds.cpp` 已独立，但 Graph presenter 仍混有 clone、insert、jump、debug。

目标文件：

| 新文件 | 职责 |
|---|---|
| `Private/UI/Review/BlueprintHelperReviewGraphDiffBlockBuilder.h` / `Private/UI/Review/BlueprintHelperReviewGraphDiffBlockBuilder.cpp` | class `FBlueprintHelperReviewGraphDiffBlockBuilder`，负责从 visible changes 添加 diff block nodes |
| `Private/UI/Review/BlueprintHelperReviewGraphJumpController.h` / `Private/UI/Review/BlueprintHelperReviewGraphJumpController.cpp` | class `FBlueprintHelperReviewGraphJumpController`，负责 selected diff block 定位和 jump debug |

检查项：

- [ ] `GraphPresenter` 只负责构建 `SGraphEditor` 和调用 builder/controller。
- [ ] Graph bounds 失败 debug 保留完整字段。
- [ ] 非 Graph anchor 不进入 diff block builder。

### 7. 拆 Details row 定位策略

当前问题：

- Details presenter 负责显示 Details。
- Panel 仍保留 `ResolveDetailsRowGeometry`、widget text recursion、details object resolve。

目标文件：

| 新文件 | 职责 |
|---|---|
| `Public/UI/Review/BlueprintHelperReviewDetailsPresenter.h` | Details content/overlay public contract |
| `Private/UI/Review/BlueprintHelperReviewDetailsGeometryResolver.h` / `Private/UI/Review/BlueprintHelperReviewDetailsGeometryResolver.cpp` | class `FBlueprintHelperReviewDetailsGeometryResolver`，负责 Details row locate、property text matching、displayed properties event |

检查项：

- [ ] Panel 不再知道 Details 内部 text recursion。
- [ ] Details row 找不到时只输出 pending/hidden，不画假框。

## P2 拆分清单

### 8. 消除 namespace / static helper 遗留

目标：

- 拆分不是只移动文件，还必须把当前 `namespace BlueprintHelperReviewSurfacePresenterPrivate`、`namespace BlueprintHelperReviewAssetPresentersPrivate` 这类实现方式替换为 utility class。
- 所有 helper class 都必须有独立 `.h/.cpp` 文件对，header declaration-only，implementation 全部进 `.cpp`。

候选 utility class：

| 新 class 文件对 | 替代内容 |
|---|---|
| `BlueprintHelperReviewRouteTargetUtils.h/cpp` | surface target kind 判断、atomic target 过滤、target key alias 生成 |
| `BlueprintHelperReviewSlateWidgetTextUtils.h/cpp` | Slate widget text 读取、递归匹配、text normalization |
| `BlueprintHelperReviewChangeTitleUtils.h/cpp` | 可读标题、asset path prefix stripping、asset kind suffix |
| `BlueprintHelperReviewGraphNodeMatchUtils.h/cpp` | Graph node target key / guid / label matching |

检查项：

- [ ] `rg -n "namespace BlueprintHelperReview" Source/BlueprintHelper/Private/UI/Review Source/BlueprintHelper/Public/UI/Review` 无命中。
- [ ] `rg -n "^\\s*static\\s+.*\\(" Source/BlueprintHelper/Private/UI/Review -g "*.cpp"` 中不再出现 namespace-scope helper function。
- [ ] 新增 utility class header 无函数体。

### 9. 收窄 Public API

目标：

- 所有只在 ReviewPanel 内部使用的类型优先移到 `Private/UI/Review`。
- Public 只保留跨模块或自动化测试确实需要的合同。

候选：

- `SBlueprintHelperReviewGeometryProbe` 如只被 Private presenters 使用，可以移入 Private header。
- `FBlueprintHelperReviewSlateRowGeometryRegistry` 如只被 Private presenters 使用，可以移入 Private header。
- `FBlueprintHelperReviewSurfaceFrameBuilder` 如不再被测试直接调用，可移为 Private。

检查项：

- [ ] Public header include 数下降。
- [ ] 新增 include 不引入循环依赖。
- [ ] 迁移后 UE build 通过。

### 10. 清理 umbrella include

第一阶段可以保留：

- `BlueprintHelperReviewSurfacePresenter.h`
- `BlueprintHelperReviewAssetPresenters.h`

最终目标：

- 调用方直接 include 精确 presenter header。
- umbrella header 只保留一到两个版本作为兼容层，然后删除。

检查项：

- [ ] `rg "BlueprintHelperReviewSurfacePresenter.h"` 只剩必要兼容引用。
- [ ] `rg "BlueprintHelperReviewAssetPresenters.h"` 只剩必要兼容引用。

## 不建议拆分的文件

| 文件 | 原因 |
|---|---|
| `BlueprintHelperReviewAssetContext.h/cpp` | 资产加载和分类职责单一，当前规模可控 |
| `BlueprintHelperReviewGraphResolver.h/cpp` | graph resolve 职责单一 |
| `BlueprintHelperReviewGraphBounds.h/cpp` | Graph bounds 算法虽有复杂度，但已独立，先不要和 presenter 拆分混在一起 |
| `BlueprintHelperReviewDiffBlockNode.h/cpp` | 自定义 graph node widget 独立且规模可控 |
| `BlueprintHelperReviewDebugText.h/cpp` | 很小，不需要拆 |

## 推荐执行顺序

1. 建立测试基线：`BlueprintHelper.Review.UI`、`BlueprintHelper.Review.VisibleChange`、`git diff --check`。
2. 先拆 Public 类型头：新增 `PresenterTypes`、`RowHighlightModel`、`SurfaceRouter`、`SurfaceFrameBuilder`，保留 umbrella header。
3. 拆 `BlueprintHelperReviewSurfacePresenter.cpp`，每拆一个 presenter 就跑对应 Review UI automation。
4. 拆 `BlueprintHelperReviewAssetPresenters.cpp`，先 WidgetTree，再 DataTable，再 DataAsset，最后 Structure。
5. 拆 `SBlueprintHelperReviewPanel.cpp`，先无行为风险的 Debug/Layout，再拆 ChangeTree/Actions，最后拆 Surfaces/Details。
6. 消除拆分后遗留的 namespace/static helper，改为 utility class 文件对。
7. 拆测试文件，并保持原 automation test names。
8. 删除或收窄 umbrella include。
9. 做一次 grouped build + automation。

## 每阶段验证

### Static

```powershell
git diff --check
```

### Build

```powershell
F:\UE_5.6\Engine\Build\BatchFiles\Build.bat MrStoneEditor Win64 Development -Project="G:\UnrealPractise\MrStone\MrStone.uproject" -WaitMutex -NoHotReload
```

### Automation

```text
Automation RunTests BlueprintHelper.Review.UI
Automation RunTests BlueprintHelper.Review.VisibleChange
Automation RunTests BlueprintHelper.Review.GraphBounds
```

## 完成判定

- [ ] `BlueprintHelperReviewSurfacePresenter.cpp` 不再承载多个 presenter。
- [ ] `BlueprintHelperReviewAssetPresenters.cpp` 不再同时 include UMG、DataTableEditor、PropertyEditor、StructureEditorUtils。
- [ ] `SBlueprintHelperReviewPanel.cpp` 单文件低于 800 行，且主要保留 layout/orchestration。
- [ ] Review 测试按职责拆成多个文件，单文件不超过 1200 行。
- [ ] Public header 不再暴露不必要的 Slate 私有实现细节。
- [ ] 所有原有 automation test path 保持不变。
- [ ] Review UI 相关新增 class 都有唯一 `.h/.cpp` 文件对。
- [ ] 新增或迁移后的 header 只有声明，没有函数体。
- [ ] Review UI 相关源码不再新增 namespace、anonymous namespace、namespace-scope static helper 或 namespace-scope free function。
- [ ] UE build 和 targeted automation 通过。
