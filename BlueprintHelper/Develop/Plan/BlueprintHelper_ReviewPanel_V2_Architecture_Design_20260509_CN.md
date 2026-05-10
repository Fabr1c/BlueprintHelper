# BlueprintHelper ReviewPanel v2 架构设计

日期: 2026-05-09

状态: 草案，待用户审核。在本文档确认之前不要实施。

## 背景

最新用户端 ReviewPanel 导出显示，ReviewPanel 可以加载当前 smoke 的 ReviewRecord，但面板未能将多个 review 变更映射到正确的视觉表面。

从 `C:/Users/26227/Desktop/ReviewPanelExportLog.txt` 观察到的示例：

- `BH_SmokeCustomEvent_0509` 选择了 `/Game/BlueprintHelper/Smoke/BP_ClassSettingsSmoke` 且 `graph="EventGraph"`，但 Graph bounds 失败，错误为 `targets=1 graphTargets=0 skippedSurface=1`。
- `SmokeSceneComp` 和 `variable SmokeHealth` 选择了正确的资产，但 component 和 variable 的 diff 框架未能可靠地定位到真实 UI 行。
- `DT_SmokeDamageTable` 和 `WBP_WidgetSmoke` 显示为可审查的变更，但当前面板只有以 Blueprint 为中心的内容区域。
- `/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke` 下的 `tx_1778317276165` 证明，当 evidence 具有真正的 `surface="graph"` 目标时，Graph review 可以正常工作。

当前实现以 `SBlueprintHelperReviewPanel` 为中心，使用一个 `UBlueprint` 指针作为其资产模型。这对于 DataAsset、DataTable、UserWidget 以及未来的资产类型来说过于狭隘。

## 目标

1. ReviewPanel 将任何 review 资产加载为 `UObject`，而不仅仅是 `UBlueprint`。
2. ReviewPanel 通过显式的 review anchor 来路由每个 visible change，而不是从标签猜测。
3. Graph diff 块仅针对真正的 graph anchor 创建。
4. Components、variables、class defaults、UMG widgets、DataTable rows、DataAsset properties 以及通用 object properties 各自拥有定义的 presenter 路径。
5. Diff 框架首先基于稳定的 anchor 契约，同时具备文档化的回退行为。
6. Review 操作行为在此架构阶段保持不变：Accept、Reject、RejectAll 仍然在现有的 `FBlueprintHelperReviewActionService` 上操作。
7. Debug 导出继续显示路由和几何诊断信息，以便用户端 review 可以证明每个 surface。

## 非目标

- 不在本阶段重新设计 Reject 依赖阻塞。
- 不改变 DebugCase 或 DebugBundle 契约。
- 不删除 `Saved` 中的现有 ReviewRecord 文件。
- 在设计阶段不要求 UE 构建变更。
- 不试图在第一个实现阶段为每个编辑器 widget 实现完美的 Slate 行几何。

## 当前根因

### 1. 资产模型仅限 Blueprint

当前面板状态仅存储：

```cpp
TWeakObjectPtr<UBlueprint> ReviewBlueprint;
TStrongObjectPtr<UBlueprint> PreviewBlueprint;
TStrongObjectPtr<UEdGraph> PreviewGraph;
```

这意味着 DataTable、DataAsset 或非 Blueprint 的 UObject 无法成为主审查对象。Widget Blueprint 可以作为 Blueprint 加载，但面板仍然没有 UMG 树或设计器 presenter。

### 2. Surface 分类不是路由契约

`BlueprintHelperReviewShouldShowInGraph` 在 `Change.GraphName` 被设置时返回 true，即使所有 atomic targets 都是 `surface="my_blueprint"` 或 `surface="details"`。

这导致了以下路径：

```text
Change 有 graph_name
-> Graph 面板显示它
-> Graph bounds builder 只接受 surface=graph
-> skippedSurface=1
-> 选中的 fallback rect 绘制在通用位置
```

这就是 `BH_SmokeCustomEvent_0509` 出现在 Graph 中，但没有生成正确的 Graph diff 框的原因。

### 3. 面板框架几何是硬编码的

Component、MyBlueprint 和 Details 框架通过字符串桶和固定像素放置。示例包括 `DefaultSceneRoot`、`SmokeValue`、`FakeDiffProperty` 以及常量的行偏移。这作为 smoke 脚手架是有用的，但不能作为真正的用户端 ReviewPanel 行为。

### 4. Evidence 没有表达足够的 Anchor 类型

当前的 `EBlueprintHelperReviewSurface` 有：

```text
Graph
Components
MyBlueprint
Details
```

真正的 review targets 需要更精确的 anchor 种类：

- Graph block
- Graph node
- Blueprint component
- Blueprint variable
- Blueprint function 或 event signature
- Blueprint dispatcher
- Class default property
- Object property
- UMG widget tree node
- UMG widget property
- DataTable row
- DataAsset property
- Asset factory summary

其中一些仍然可以在相同的面板区域中渲染，但它们需要不同的 anchor 契约。

## 提议的架构

### Review 资产上下文

引入一个小型资产上下文层：

```cpp
enum class EBlueprintHelperReviewAssetKind : uint8
{
    Unknown,
    Blueprint,
    WidgetBlueprint,
    DataTable,
    DataAsset,
    GenericObject
};

struct FBlueprintHelperReviewAssetContext
{
    FString AssetPath;
    EBlueprintHelperReviewAssetKind AssetKind = EBlueprintHelperReviewAssetKind::Unknown;
    TWeakObjectPtr<UObject> AssetObject;
    TWeakObjectPtr<UBlueprint> Blueprint;
    TWeakObjectPtr<UDataTable> DataTable;
    TWeakObjectPtr<UObject> DefaultObject;
};
```

职责：

- 规范化 package 路径和 object 路径。
- 加载对象而不强制所有内容通过 `LoadObject<UBlueprint>`。
- 从加载的对象和生成的类检测资产种类。
- 仅在资产为 Blueprint-like 时提供 `Blueprint`。
- 为通用 details 和非 Blueprint presenter 提供 `AssetObject`。

初始实现文件：

- `Source/BlueprintHelper/Public/UI/Review/BlueprintHelperReviewAssetContext.h`
- `Source/BlueprintHelper/Private/UI/Review/BlueprintHelperReviewAssetContext.cpp`

### Surface Presenter 模型

将面板渲染拆分为 presenter。第一个版本可以是轻量级辅助类或函数，而不是完整的插件框架。

推荐的接口：

```cpp
struct FBlueprintHelperReviewSurfaceBuildArgs
{
    const FBlueprintHelperReviewAssetContext* AssetContext = nullptr;
    TArray<TSharedPtr<FBlueprintHelperReviewVisibleChange>> ChangeItems;
    TSharedPtr<FBlueprintHelperReviewVisibleChange> SelectedChange;
};

class IBlueprintHelperReviewSurfacePresenter
{
public:
    virtual ~IBlueprintHelperReviewSurfacePresenter() = default;
    virtual bool CanRender(const FBlueprintHelperReviewAssetContext& Context) const = 0;
    virtual bool ShouldShowChange(const FBlueprintHelperReviewVisibleChange& Change) const = 0;
    virtual TSharedRef<SWidget> BuildContent(const FBlueprintHelperReviewSurfaceBuildArgs& Args) = 0;
    virtual TSharedRef<SWidget> BuildOverlay(const FBlueprintHelperReviewSurfaceBuildArgs& Args) = 0;
};
```

面板保持对选择、操作、调试消息和布局的所有权。Presenter 拥有其 surface 的内容和叠加几何。

初始 presenter：

- `GraphPresenter`
- `BlueprintComponentsPresenter`
- `MyBlueprintPresenter`
- `ObjectDetailsPresenter`

第二阶段 presenter：

- `UMGWidgetTreePresenter`
- `DataTablePresenter`
- `DataAssetPresenter`

### Anchor 路由

引入辅助函数，将显式的 atomic targets 视为权威来源。

规则：

1. 如果变更具有任何 atomic targets，则仅当至少一个 target 属于该 surface 或 anchor 种类时，surface 才应显示它。
2. 当存在显式 targets 且没有一个是 Graph 时，单独的 `GraphName` 不得强制 Graph 渲染。
3. 如果变更没有 atomic targets，遗留回退可以使用 `GraphName`、`LocationKey` 和 `ChangeKind`。
4. Debug 输出必须包含路由决策，例如：

```text
ReviewRoute change=... assetKind=Blueprint selectedSurface=Graph explicitTargets=1 graphTargets=0 result=hidden reason=no_graph_anchor
```

这直接解决了 `skippedSurface=1` 的失败。

### Graph Presenter

Graph presenter 职责：

- 仅为 Blueprint 和 WidgetBlueprint 资产解析选中的 graph。
- 将源 graph 克隆到临时的 preview graph。
- 仅为 `surface="graph"` 的 targets 添加 diff block 节点。
- 仅在存在选中的 Graph diff block 时跳转。
- 如果选中的变更不可 graph-routable，显示非 graph 占位符而不是空的 K2 graph。

预期行为：

- `tx_1778317276165` 保持 graph-routable，应绘制正确的 block。
- 仅具有 `surface="my_blueprint"` 的 `BH_SmokeCustomEvent_0509` 不再绘制 Graph fallback block。

后续 evidence 工作：

- 签名创建应在创建或更改真正的 K2 event/function graph node 时，同时发出 MyBlueprint signature anchor 和 Graph anchor。

### Components Presenter

第一阶段行为：

- 仅在审查的 Blueprint 生成类是 Actor 类时使用 `SSubobjectBlueprintEditor`。
- 仅为 component anchors 显示 component diff 叠加。
- 将当前的近似叠加保留为回退，但在 debug 中将其标记为 `fallback_geometry`。

第二阶段行为：

- 如果 Unreal 暴露稳定的路径，从实际的 component tree widget 解析 component 行几何。
- 如果无法读取行几何，显示基于确定性列表的 review 视图，而不是假装精确的编辑器行放置。

### MyBlueprint Presenter

第一阶段行为：

- 对 Blueprint 和 WidgetBlueprint 资产使用 `SMyBlueprint`。
- 仅在 atomic targets 显式指向 MyBlueprint 时显示 signature、variable、dispatcher、macro 和 function anchors。
- 将当前的近似叠加保留为回退。

第二阶段行为：

- 在可能的情况下从实际的 MyBlueprint 条目解析树行几何。
- 为 variables 和 signatures 提供确定性 anchor 列表回退。

### Object Details Presenter

第一阶段行为：

- 对 `AssetObject`、`Blueprint`、生成的 CDO 或选中的 details 对象使用 `SKismetInspector`。
- 支持 DataAsset、DataTable 资产摘要、class default、object property 和 asset factory summary 作为可审查的 details。
- 仅为 details anchors 显示 Details diff 叠加。

第二阶段行为：

- 对于 DataAsset 和通用 UObject 属性，如果 Slate 行访问稳定，按属性路径解析 detail row。
- 否则在只读 details 上方或旁边显示属性路径 review 列表。

### UMG Widget Presenter

第二阶段行为：

- 检测 WidgetBlueprint 资产。
- 从 `WidgetTree` 构建只读 widget tree view。
- 将 `umg_widget` 和 `umg_widget_property` anchors 路由到 widget tree 或 property view。
- 对于 `WBP_WidgetSmoke` 和 `SmokeText`，在 UMG 特定的 surface 中显示 widget，而不是通用的 Details。

推荐的 anchor 字段：

```json
{
  "surface": "umg_widget_tree",
  "target_kind": "umg_widget",
  "target_key": "umg_widget:SmokeText",
  "widget_path": "RootCanvas/SmokeText"
}
```

### DataTable Presenter

第二阶段行为：

- 检测 `UDataTable`。
- 显示只读行和 row struct 摘要。
- 将 `datatable_row` anchors 路由到行级 diff 框架。
- 对于没有行编辑的资产创建，显示资产摘要框架。

推荐的 anchor 字段：

```json
{
  "surface": "data_table",
  "target_kind": "datatable_row",
  "target_key": "datatable_row:DamageSmall",
  "row_name": "DamageSmall"
}
```

### DataAsset Presenter

第二阶段行为：

- 检测 DataAsset 或通用 UObject 资产。
- 显示只读 details。
- 按属性路径路由 `object_property` 或 `data_asset_property` anchors。

推荐的 anchor 字段：

```json
{
  "surface": "data_asset",
  "target_kind": "data_asset_property",
  "target_key": "data_asset_property:Config.Health",
  "property_path": "Config.Health"
}
```

## 数据契约变更

### 最小第一阶段契约

先保持 `EBlueprintHelperReviewSurface` 不变，但收紧语义：

- `Graph`：仅 graph nodes、graph blocks、graph pins、graph links。
- `Components`：Blueprint component tree targets。
- `MyBlueprint`：variables、functions、macros、signatures、dispatchers。
- `Details`：object/class/default/property/data summary targets。

更新 `BlueprintHelperReviewShouldShowInGraph`，使显式 targets 覆盖 `GraphName`。

### v2 契约扩展

在第一阶段稳定后，添加可选的 `AnchorKind` 或扩展 `Surface`。

候选枚举：

```cpp
enum class EBlueprintHelperReviewAnchorKind : uint8
{
    GraphBlock,
    GraphNode,
    BlueprintComponent,
    BlueprintVariable,
    BlueprintSignature,
    ClassDefaultProperty,
    ObjectProperty,
    UMGWidget,
    UMGWidgetProperty,
    DataTableRow,
    DataAssetProperty,
    AssetSummary
};
```

这避免了为每个非 graph 资产重载 `Details`。

## 迁移策略

### 阶段 1：路由和资产上下文

范围：

- 添加资产上下文加载器。
- 保持现有布局。
- 将 `ReviewBlueprint` 替换为 `ReviewAssetContext`。
- 仅在需要时将 `ReviewBlueprint` 保留为派生的便利状态。
- 当存在显式 targets 时，使 Graph surface 严格。
- 使 Details surface 显示非 Blueprint 资产对象。

预期用户可见结果：

- DataTable/DataAsset 不再显示为空 Blueprint review。
- Graph 面板不再为 MyBlueprint 或 Details 变更绘制 fallback graph 框。
- 现有的真正 Graph review 仍然工作。

### 阶段 2：Presenter 提取

范围：

- 将 Graph 逻辑移出 `SBlueprintHelperReviewPanel`。
- 将 Components、MyBlueprint 和 Details 框架构建移入专用 presenter 文件。
- 将旧的几何回退保留在 presenter 代码内，而不是面板中。

预期用户可见结果：

- 面板代码变为路由器加操作外壳。
- Surface 特定的 bug 被隔离。

### 阶段 3：UMG 和 Data Presenter

范围：

- 添加 UMG widget tree presenter。
- 添加 DataTable presenter。
- 添加 DataAsset presenter。
- 添加 presenter 特定的自动化测试和用户 smoke 检查清单。

预期用户可见结果：

- `WBP_WidgetSmoke / SmokeText` 显示在面向 UMG 的 review surface 中。
- `DT_SmokeDamageTable` 显示为表格或资产摘要 review surface。
- DataAsset/ObjectProperty review 具有非空的只读 details surface。

### 阶段 4：真实几何 Anchors

范围：

- 在可行的情况下，用真实 anchor 几何替换硬编码的 component、MyBlueprint 和 Details 像素框架。
- 在 Unreal Slate 内部不暴露稳定几何的地方，使用确定性 review-list 渲染，而不是伪造精确放置。

预期用户可见结果：

- Diff 框要么对齐到真实行，要么清晰地渲染为确定性 review 卡片。

## 测试计划

### 自动化测试

在以下路径添加或更新测试：

```text
Source/BlueprintHelper/Private/Tests/Review/BlueprintHelperReviewStoreServiceTests.cpp
```

阶段 1 测试：

- `ReviewShouldShowInGraphRequiresGraphTargetWhenTargetsAreExplicit`
- `ReviewPanelConstructsWithDataTableVisibleChange`
- `ReviewPanelConstructsWithGenericObjectVisibleChange`
- `ReviewPanelDoesNotGraphRouteMyBlueprintOnlySignatureChange`

阶段 2 测试：

- Presenter 路由仅为 Graph anchors 返回 Graph。
- Components presenter 接受 component anchors 并拒绝 graph/details anchors。
- Details presenter 接受 object property 和 asset factory anchors。

阶段 3 测试：

- UMG widget anchor 路由到 UMG presenter。
- DataTable row anchor 路由到 DataTable presenter。
- DataAsset property anchor 路由到 DataAsset presenter。

### 手动 Smoke

使用 disposable 资产：

- `/Game/BlueprintHelper/Smoke/BP_ClassSettingsSmoke`
- `/Game/BlueprintHelper/Smoke/BP_TaskSpecSmoke`
- `/Game/BlueprintHelper/Smoke/WBP_WidgetSmoke`
- `/Game/BlueprintHelper/Smoke/DT_SmokeDamageTable`
- 一个 disposable DataAsset fixture

用户端检查：

- 选择真正的 Graph 变更：面板跳转到 graph diff block。
- 选择 MyBlueprint signature/variable 变更：graph 不绘制 fallback block；MyBlueprint presenter 显示该变更。
- 选择 component 变更：component presenter 显示 component review 框架或 review-list 回退。
- 选择 UMG widget 变更：UMG presenter 显示 widget tree 条目或 review-list 回退。
- 选择 DataTable 变更：DataTable presenter 显示表格摘要或行条目。
- Accept 和 Reject 操作仍然更新状态。
- Debug 导出包含路由决策，且没有过时的 `debug_export_refs`。

## 待用户确认的开放决策

1. 阶段 1 是否应保持当前的四列布局，还是可以根据选中的资产种类切换中心面板？
2. 对于阶段 4，一旦存在确定性 review-list 回退，是否应立即移除不准确的硬编码框架？
3. UMG 和 DataTable 应该获得新的 `EBlueprintHelperReviewSurface` 值，还是应首先使用 `Details` 加上 `target_kind` 路由？
4. 签名创建应仅在创建或修改具体的 graph node 时发出 Graph target，还是应始终尝试指向目标 graph？
5. DataTable 资产创建应仅作为资产摘要审查，而行写入应作为行级变更审查？

## 用户选择的决策

1. 在阶段 1 中根据选中的资产种类切换中心面板驱动不同内容。
2. 仅在确定性 review-list 回退存在后才移除不准确的硬编码框架。
3. 在阶段 1 中保持 `EBlueprintHelperReviewSurface` 不变。在阶段 2 或阶段 3 中添加更丰富的 anchor 种类。
4. 仅在创建或修改具体的 graph node 或 block 时发出 Graph targets。
5. 将 DataTable 创建视为资产摘要；将行写入视为 DataTable row anchors。

## 批准后的首个实现切片

如果本设计获得批准，首个实现切片应为：

1. 添加 `BlueprintHelperReviewAssetContext`。
2. 为 Graph 添加严格的显式 target 路由。
3. 更新 `SBlueprintHelperReviewPanel` 以使用资产上下文和通用对象 details。
4. 为路由和非 Blueprint 构造路径添加回归测试。
5. 运行 `git diff --check`以及 UE 自动化
