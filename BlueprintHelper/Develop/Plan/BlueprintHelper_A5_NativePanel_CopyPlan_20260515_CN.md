# BlueprintHelper A5 Native Panel Copy Plan - 2026-05-15

## 1. 本轮边界

- 本轮先做：MyBlueprint、Components 的复制改名方案。
- 本轮暂缓：WidgetTree、Details。
- GraphPanel：只做底层绘制方案验证，不进入完整复制实现。
- 原则：UE 原生源码先放在 `Develop/Reference/A5_UE56_NativePanelSourceSnapshot/`，完成改名、裁剪和扩展点设计后，再把可编译切片迁移到 `Source`。
- C++ 类文件规则：每个 C++ 类单独一个 `.h/.cpp`；结构体、数据类、枚举除外。

## 2. MyBlueprint 复制改名方案

### 2.1 源文件

- `Develop/Reference/A5_UE56_NativePanelSourceSnapshot/Kismet/SMyBlueprint.h`
- `Develop/Reference/A5_UE56_NativePanelSourceSnapshot/Kismet/SMyBlueprint.cpp`
- `Develop/Reference/A5_UE56_NativePanelSourceSnapshot/GraphEditor/SGraphActionMenu.h`
- `Develop/Reference/A5_UE56_NativePanelSourceSnapshot/GraphEditor/SGraphActionMenu.cpp`

### 2.2 目标类和文件

- `SBlueprintHelperReviewMyBlueprintPanel`
- `SBlueprintHelperReviewGraphActionMenu`
- `SBlueprintHelperReviewGraphActionRow`
- `FBlueprintHelperReviewMyBlueprintActionCollector`
- `FBlueprintHelperReviewMyBlueprintRowAdapter`

目标目录建议：

- `Source/BlueprintHelper/Public/UI/Review/Native/MyBlueprint/`
- `Source/BlueprintHelper/Private/UI/Review/Native/MyBlueprint/`

### 2.3 保留内容

- 原生 MyBlueprint 的 section 体系：Graphs、Functions、Macros、Variables、Event Dispatchers、Local Variables、User Structs 等。
- `SGraphActionMenu` 的 tree/filter/section/row 生成模型。
- 原生 action item 的图标、分类、折叠、搜索和排序语义。
- read-only Blueprint outline 显示能力。

### 2.4 裁剪内容

- Add New 菜单。
- Rename / Delete / Duplicate / Cut / Copy / Paste。
- Category 拖拽和 action 拖拽。
- Graph open / focus / implement function 等编辑器导航命令，除非后续明确需要 ReviewPanel 内跳转。
- 依赖 `FBlueprintEditor` 的写操作路径。

### 2.5 Review 扩展点

- Row 背景色：在自有 `SBlueprintHelperReviewGraphActionRow` 内读取 `FBlueprintHelperReviewRowHighlightModel::GetRowBackgroundColor(...)`。
- Row 操作区：将 Accept / Reject 按钮放进 Row 内部，而不是外层 Overlay。
- Row key：使用 `FEdGraphSchemaAction` 的真实名称、section、graph/function/macro/variable target key 生成稳定搜索 key。
- 行几何：Row 创建时注册到 `FBlueprintHelperReviewSlateRowGeometryRegistry`，仅用于 debug 和定位，不再作为绘制主路径。
- ReviewAnchor 过滤：复制后只显示 UE 原生 action source，不再从 Review-only placeholder 反向注入到 MyBlueprint 正常内容。

### 2.6 接入点

- 替换 `FBlueprintHelperReviewMyBlueprintPresenter::BuildContent(...)` 内当前自建 `STreeView`。
- 保留 `FBlueprintHelperReviewMyBlueprintPresenter::ShouldShowChange(...)` 和 surface routing。
- 保留 `FBlueprintHelperReviewRowHighlightModel` 作为统一状态源。

### 2.7 验收条件

- MyBlueprint 面板显示宏分类。
- 不显示 `ReviewAnchor` 等非 UE 原生 outline 项。
- 变量、函数、宏、事件、dispatcher 的 Review row 背景由 Row 自身绘制。
- Accept / Reject 单条 Review 时只影响对应 change，不影响 sibling。
- Final Changes 选择变化时 MyBlueprint Row 高亮刷新。

## 3. Components 复制改名方案

### 3.1 源文件

- `Develop/Reference/A5_UE56_NativePanelSourceSnapshot/SubobjectEditor/SSubobjectEditor.h`
- `Develop/Reference/A5_UE56_NativePanelSourceSnapshot/SubobjectEditor/SSubobjectEditor.cpp`
- `Develop/Reference/A5_UE56_NativePanelSourceSnapshot/SubobjectEditor/SSubobjectBlueprintEditor.h`
- `Develop/Reference/A5_UE56_NativePanelSourceSnapshot/SubobjectEditor/SSubobjectBlueprintEditor.cpp`

### 3.2 目标类和文件

- `SBlueprintHelperReviewSubobjectPanel`
- `SBlueprintHelperReviewSubobjectTreeView`
- `SBlueprintHelperReviewSubobjectRow`
- `SBlueprintHelperReviewSubobjectBlueprintPanel`
- `FBlueprintHelperReviewSubobjectRowAdapter`

目标目录建议：

- `Source/BlueprintHelper/Public/UI/Review/Native/Components/`
- `Source/BlueprintHelper/Private/UI/Review/Native/Components/`

### 3.3 为什么必须复制

- 当前 ReviewPanel 使用 UE 原生 `SSubobjectBlueprintEditor`。
- UE 5.6 的 `SSubobjectBlueprintEditor` 是 `final`。
- `SSubobjectEditor::MakeTableRowWidget(...)` 是 protected，外部不能替换 Row 类型。
- 因此要让组件 Row 自己绘制 Diff 背景，不能靠继承覆盖，只能复制 Tree + Row 相关路径到 BlueprintHelper 自有类。

### 3.4 保留内容

- Subobject tree 数据源和 root/child 组织。
- Component display name、class icon、mobility icon、parent/child hierarchy。
- 搜索过滤和展开逻辑。
- `FindSlateNodeForVariableName(...)` / row lookup 等定位能力。

### 3.5 裁剪内容

- Add Component class combo。
- Promote to Blueprint。
- Drag/drop attach/detach/make root。
- Delete / duplicate / copy / paste。
- Rename。
- Context menu 写操作。
- 会修改 Blueprint 或 Actor 的事务路径。

### 3.6 Review 扩展点

- Row 背景：在 `SBlueprintHelperReviewSubobjectRow::OnPaint(...)` 中先绘制 Review fill，再调用原 row paint，保证背景覆盖整行而不是只覆盖某个 column。
- Row actions：在主 column 右侧添加 Accept / Reject，受 `GetRowActionsVisibility(...)` 控制。
- Row key：组件变量名、display string、component path、template readable name 都注册为 alias。
- Native component：native/inherited component 也必须通过同一个 row adapter 查询背景色，不能只处理新增 component。

### 3.7 接入点

- 替换 `FBlueprintHelperReviewBlueprintComponentsPresenter::BuildContent(...)` 里的 `SSubobjectBlueprintEditor`。
- `FBlueprintHelperReviewBlueprintComponentsPresenter::ResolveRowGeometry(...)` 改为读取自有 tree/row adapter。
- 旧 overlay fallback 可保留一阶段作为 debug fallback，但不是最终绘制主路径。

### 3.8 验收条件

- 新增组件 Row 的背景色由 Row 自身绘制。
- native / inherited / user component Row 均能保持原生显示。
- 选中 Final Changes 中某个组件 Review 后，Components 面板对应 Row 高亮。
- Row 内 Accept / Reject 能执行对应 change。
- 关闭重开 ReviewPanel 后，已 Accept / Reject 的组件 Review 不复活。

## 4. GraphPanel 底层绘制验证

### 4.1 当前验证结论

- UE 5.6 `SGraphEditorImpl` 在 private implementation 中直接 `SAssignNew(GraphPanel, SGraphPanel)`。
- `SGraphEditor` 对外没有参数允许传入自定义 GraphPanel 类型。
- 因此不能只复制一个 `SGraphPanel` 子类再直接塞回现有 `SGraphEditor`。
- `SGraphPanel::OnPaint(...)` 的层级顺序适合底层绘制：先 `PaintBackgroundAsLines(...)`，再安排 comment / node diff highlight / wire / node shadow / node paint。
- 正确 underlay hook 位置是 `PaintBackgroundAsLines(...)` 之后、`ArrangeChildNodes(...)` 和节点绘制之前。

### 4.2 可选路线

| 路线 | 是否复制 | 可行性 | 风险 | 结论 |
|---|---|---:|---:|---|
| 使用 `SGraphEditor.DiffResults` 合成 native node diff | 不复制 | 中 | 中 | 优先验证。可利用原生 `NodeDiffHighlightLayerID`，但可能只能覆盖节点级，不覆盖任意 bounds。 |
| 复制 `SGraphPanel` 并增加 underlay paint delegate | 部分复制 | 中 | 高 | 可以满足底层绘制，但还需要复制或改造 `SGraphEditorImpl` 才能实例化自有 Panel。 |
| 复制 `SGraphEditorImpl + SGraphPanel` | 完整复制 | 中 | 很高 | 能完全控制底层绘制，但会带入 GraphEditor 私有交互层维护成本。暂不进入。 |
| 继续外层 Overlay | 不复制 | 高 | 功能不满足 | 不符合“Diff 框绘制在图表底层”的目标。 |

### 4.3 推荐验证顺序

1. 先验证能否把 Review graph target 转换为 `FDiffSingleResult` 并传给 `SGraphEditor.DiffResults`。
2. 如果 native diff highlight 能满足“节点底层 Diff”，则不复制 GraphPanel。
3. 如果需要绘制任意 recorded bounds，则实现 `SBlueprintHelperReviewGraphPanel`，在 `PaintBackgroundAsLines(...)` 后加 underlay delegate。
4. 只有当必须用自有 GraphPanel 时，再评估复制 `SGraphEditorImpl` 的最小只读版本。

### 4.4 暂定验收条件

- Diff 框/底色在节点和连线下方，而不是浮在图表上方。
- 缩放、平移后位置稳定。
- 不影响 pin hit test、node selection、read-only graph browsing。
- GraphPanel alpha 保持 ReviewPanel 期望值，当前目标为 0.35。

## 5. 2026-05-15 当前实现状态

- 状态：MyBlueprint / Components native 类已迁入 `Source`，GraphPanel 底层绘制路径已实现，编译通过；ReviewPanel 视觉和交互仍等待人工验证。
- MyBlueprint：新增 `SBlueprintHelperReviewMyBlueprintPanel` 与 `SBlueprintHelperReviewMyBlueprintRow`，`FBlueprintHelperReviewMyBlueprintPresenter::BuildContent(...)` 改为只构建 outline 数据并挂接 native panel；不再把 Review-only placeholder / ReviewAnchor 反向注入 MyBlueprint 正常 outline。
- Components：新增 `SBlueprintHelperReviewComponentsPanel` 与 `SBlueprintHelperReviewComponentRow`，`FBlueprintHelperReviewBlueprintComponentsPresenter::BuildContent(...)` 不再直接使用 UE final 的 `SSubobjectBlueprintEditor`，改为 BlueprintHelper 自有 read-only component tree/row；Row 内部读取 `FBlueprintHelperReviewRowHighlightModel` 绘制背景色和 Accept / Reject 操作。
- GraphPanel：`UBlueprintHelperReviewDiffBlockNode` 改为派生 `UEdGraphNode_Comment`，利用 UE 原生 `SGraphPanel` comment layer 在连线和节点底层绘制 Review diff block；alpha 保持 0.35，不再依赖外层 overlay 作为主绘制层。
- 编译证据：2026-05-15 执行 `E:\UE_5.6\Engine\Build\BatchFiles\Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild`，结果 `Result: Succeeded`。

## 6. 距离期望差距

- WidgetTree / Details：本轮按约定暂缓，尚未迁入 native row/panel。
- Components：当前实现覆盖 Blueprint SCS component tree 和新增组件 row；native / inherited / CDO component 与 UE 原生 `SSubobjectBlueprintEditor` 的完全等价显示仍需在 ReviewPanel 中人工验证。
- GraphPanel：未复制 `SGraphPanel` / `SGraphEditorImpl`，采用低风险 comment-layer underlay；仍需人工验证缩放、平移、遮挡、pin hit test、node selection 是否满足期望。
- ReviewPanel UI：编译已通过，但 MyBlueprint row highlight、Components row highlight、Graph diff block 底层绘制、row 内 Accept / Reject 的最终行为需要用户在 ReviewPanel 中确认后才能标记为完成。
## 7. 2026-05-15 ReviewPanel 验证反馈修正

- 反馈：替换后 Row Diff 颜色变暗，MyBlueprint / Components 的 panel 和 row 视觉与 UE 原生不一致，hover 到 row 时 Accept / Reject 没有稳定显示在 row 右侧。
- 原因：上一版迁入的是 BlueprintHelper 自有 native Slate 类，不是完整复制 UE `SGraphActionMenu` / `SSubobject_RowWidget` 源码；Row 内部使用 `Brushes.Recessed + BorderBackgroundColor`，会和 Review Diff 色叠乘，导致颜色变暗；按钮槽也没有完全按整行右侧锚定。
- 本轮修正：MyBlueprint Row 改为原生 GraphActionMenu 类似结构，section 使用 `Brushes.Header` / `Brushes.Secondary`，row 使用 `DetailsView.TreeView.TableRow`；Diff 状态改为独立 Overlay 层，只绘制 `FBlueprintHelperReviewRowHighlightModel` 返回的 Diff 色，不再叠加 `Brushes.Recessed`。
- 本轮修正：Components Row 改为 `SceneOutliner.TableViewRow` 风格，接近 UE `SSubobject_RowWidget` 的 row style；组件显示名改为变量名清洗结果，避免 `None.xxx_GEN_VARIABLE` / `GetReadableName()` 泄漏；增加组件 class icon。
- 本轮修正：MyBlueprint / Components 的 Accept / Reject 改为整行 hover 且存在 Diff 时显示，按钮槽固定在 row 右侧。
- 距离期望差距：这仍不是完整复制 UE 私有源码的最终版本，而是按 UE native row style 修正后的可编译切片；完整 `SGraphActionMenu` / `SSubobjectEditor` 源码级复刻仍需单独阶段拆迁和裁剪。
- 编译状态：按当前会话要求，编译验证统一放到最后，当前尚未编译。

## 2026-05-15 MyBlueprint/Components Row polish update

- Components/MyBlueprint native Row 增加 25px 最小高度，并添加 bottom padding 5.0。
- Components Row 恢复 hover tooltip，显示组件名和组件类。
- MyBlueprint 保留空的 `Macros` 与 `Event Dispatchers` 分类，不再因空分类被移除。
- MyBlueprint 变量/事件分发器 Row 增加只读 UE pin type 显示，使用 `SPinTypeSelector::ConstructPinTypeImage`、`FBlueprintEditorUtils::GetIconFromPin` 和 `UEdGraphSchema_K2::TypeToText`。
- Graphs 分类下的 graph row 可以包含事件子项并支持折叠；EventGraph 下的 custom event 不再作为 Graphs 同级平铺。
- 编译证据：2026-05-15 `Build.bat TemplateEditor Win64 Development -Project=D:\UEProjects\Template\Template.uproject -WaitMutex -FromMsBuild` 返回 `Result: Succeeded`。
- 距离期望差距：仍需用户在 ReviewPanel 中做视觉验收，确认 row 高度、tooltip、变量类型显示和 Graph 折叠交互符合 UE 原生预期。