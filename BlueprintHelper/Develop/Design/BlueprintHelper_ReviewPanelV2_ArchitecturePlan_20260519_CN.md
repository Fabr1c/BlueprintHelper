# BlueprintHelper ReviewPanel v2 三层架构重构计划

日期：2026-05-19

## 0. 执行进度

更新时间：2026-05-19 16:08

- [x] 阶段 1：已补齐 v2 Data 基础类型，包括 `FBlueprintHelperReviewPanelState`、`FBlueprintHelperReviewSurfaceDiffModel`、`FBlueprintHelperReviewRowBinding`、`FBlueprintHelperReviewActionIntent`、`FBlueprintHelperReviewTransientActionState`。
- [x] 阶段 2：已新增 `FBlueprintHelperReviewPanelStateService`，集中负责从 pending Review 构建 PanelState、RowBinding、Surface target keys，并解析 ActionIntent 到待处理 Review。
- [x] 阶段 3：Row action 已迁移到 `FBlueprintHelperReviewActionIntent`，`OnAcceptChangeId`、`OnRejectChangeId`、`AcceptHighlightedRow`、`RejectHighlightedRow`、`FindActionRowHighlightEntry` 旧接口已移除。
- [x] 阶段 4：Accept/Reject 成功路径不再由 UI 本地删除 `ChangeItems`，改为通过 `ReviewCommandService -> ReviewActionService -> ReviewStoreChanged` 驱动刷新。
- [x] 阶段 5：`AcceptVisibleChange` 在找不到 persisted target 时不再伪成功，避免 UI 误认为 Review 已处理。
- [x] 阶段 6：已新增独立 `FBlueprintHelperReviewSurfaceView` 类；`FBlueprintHelperReviewSurfaceViewCoordinator` 只持有 SurfaceView 对象并调用其 `RefreshOverlay/RefreshRows`，不再保存裸 registration lambda。
- [x] 阶段 7：Reject queued/preparing/mutating 状态写入 `FBlueprintHelperReviewTransientActionState`；失败结果展示写入 `FBlueprintHelperReviewPresenterErrorState`。Review UI 层已移除对 `VisibleChange.Status/NeedsActionReason` 的状态适配写入。
- [o] 阶段 8：Graph diff block 外部已转换为 ActionIntent，但内部 block 节点配置仍以 change id 为局部显示标识；这不是执行路径旧兼容，但仍需后续统一命名和数据模型。
- [o] 阶段 9：已执行多轮编辑器端 ReviewPanel 覆盖测试。Component、MyBlueprint、WidgetTree、DT、ST、DA、Details、Graph 的主要 Row action、StoreChanged 刷新、Selection 保持、DA/ST token exact Diff 绘制均已完成修复并通过用户侧基本验证；当前未发现 ReviewPanel 级大 bug，剩余内容按小项/边界项继续跟踪。

编译结果：2026-05-19 15:26 已通过 `TemplateEditor Win64 Development` 编译。仍存在现有 `STreeView::ItemHeight` UE 弃用警告，非本轮改动引入。

距离期望差距：

1. `SBlueprintHelperReviewPanel` 仍负责组装各 SurfaceView 的 overlay/row refresh 能力；后续如需进一步解耦，可继续把各 Surface 的具体能力拆到独立 Components/MyBlueprint/DataTable 等 SurfaceView 子类。
2. 编辑器端覆盖测试已经基本确认“Row action -> StoreChanged -> Panel/Row 局部刷新”的链路可用；若后续出现 Surface 局部刷新边界问题，应优先从 `ReviewPanelStateService` 投影、RowBinding、SurfaceView 刷新入口排查，不能回退到 UI 本地删项或 delay/retry。

本轮额外记录：

1. 已将 PowerShell `rg` 复杂 regex 引号问题写入 `BlueprintHelper_CLI_Tips_20260514_CN.md`。

## 1. 背景

ReviewPanel 当前已经存在部分事件驱动能力，例如 Store 变更回调、RowGeometry 生命周期广播、RowHighlight 状态广播等，但整体仍处于旧架构和 v2 架构混合状态。

当前主要问题不是单个 Panel 的刷新失效，而是职责边界混杂：UI 层仍然直接修改 `ChangeItems`，Row action 仍通过 `SearchText` 和 `RowHighlightModel` 间接查找 ReviewEvent，`SBlueprintHelperReviewPanel` 同时承担 View、状态缓存、命令编排和刷新调度职责。这会导致 Component、MyBlueprint、WidgetTree、DT、ST、DA、Details 的刷新行为不一致，并且容易产生误 Accept/Reject、FinalChange 与子 Panel 状态不一致、Diff 残留等问题。

本计划目标是把 ReviewPanel v2 完整迁移到 `Data / Service / Presenter / UI` 架构，不兼容旧架构，不保留旧 action/fuzzy/transaction 路径。

## 2. 硬性规则

1. UI 层唯一允许的生命周期同步方式是事件驱动。
2. 禁止用 delay、ActiveTimer retry、Timer retry、AsyncTask-as-delay、geometry retry counter 解决 Row 生成顺序或刷新顺序问题。
3. Accept/Reject 执行必须使用 canonical identity，不允许用 fuzzy SearchText 解析执行目标。
4. ReviewStore 是 pending Review 的唯一权威状态。
5. UI 不允许直接删除、修改、覆盖 ReviewEvent 的业务状态。
6. 旧 transaction Review、legacy rollback、legacy anchor action 路径全部移除，不做兼容。
7. 新架构必须保持高内聚、低耦合、职责清晰。

## 3. 目标架构

```text
Data
  ReviewStore / VisibleChange / SurfaceDiffState / RowBinding

Service
  ReviewQueryService
  ReviewPanelStateService
  ReviewCommandService
  ReviewActionService
  ReviewSnapshotRestoreService
  ReviewNotificationService

Presenter
  ReviewPanelPresenter
  FinalChangePresenter
  SurfacePresenter
  RowPresenter

UI
  SBlueprintHelperReviewPanel
  SFinalChangeTree
  SComponentPanel
  SMyBlueprintPanel
  SWidgetTreePanel
  SDataTablePanel
  SDataAssetPanel
  SDetailsPanel
```

## 4. Data 层职责

Data 层只保存状态，不执行行为，不调用 Service，不调用 UI。

### 4.1 `FBlueprintHelperReviewPanelState`

ReviewPanel 的完整视图状态快照。

内容包括：

1. 当前 pending visible changes。
2. FinalChange tree model。
3. 每个 Surface 的 diff model。
4. 当前 selection。
5. transient action state。
6. debug-visible state。

### 4.2 `FBlueprintHelperReviewSurfaceDiffModel`

某个 Surface 的 Diff 投影。

适用 Surface：

1. Components。
2. MyBlueprint。
3. WidgetTree。
4. DataTable。
5. DataAsset。
6. Details。
7. Graph。

### 4.3 `FBlueprintHelperReviewRowBinding`

Row 的唯一身份绑定。

建议字段：

```cpp
struct FBlueprintHelperReviewRowBinding
{
    FString AssetPath;
    FString ChangeId;
    FString AtomicTargetId;
    FString TargetKey;
    EBlueprintHelperReviewSurface Surface = EBlueprintHelperReviewSurface::Unknown;
};
```

要求：

1. Row action 必须使用该 binding。
2. `ChangeId + AtomicTargetId + Surface` 是执行语义的主键。
3. `DisplayLabel`、`SearchText`、fuzzy key 只能用于显示、定位、debug。

### 4.4 `FBlueprintHelperReviewActionIntent`

UI 发出的用户意图。

建议字段：

```cpp
struct FBlueprintHelperReviewActionIntent
{
    EBlueprintHelperReviewActionKind Action;
    FBlueprintHelperReviewRowBinding Binding;
    FString SourceWidget;
};
```

Data 层禁止内容：

1. 不保存 Slate 指针。
2. 不调用 ActionService。
3. 不执行 rollback。
4. 不保存旧 transaction 兼容字段。

## 5. Service 层职责

Service 层负责业务执行和状态转换，不直接持有 Slate Widget。

### 5.1 `FBlueprintHelperReviewQueryService`

职责：

1. 从 ReviewStore 读取 pending changes。
2. 提供只读查询接口。
3. 不构建 UI Widget。

### 5.2 `FBlueprintHelperReviewPanelStateService`

职责：

1. 输入 pending visible changes。
2. 输出 `FBlueprintHelperReviewPanelState`。
3. 构建 FinalChange tree。
4. 构建每个 Surface 的 diff model。
5. 生成 RowBinding。
6. 处理 lifecycle root 与 child 的树关系。

这是 v2 的核心状态投影服务。

### 5.3 `FBlueprintHelperReviewCommandService`

职责：

1. 接收 `FBlueprintHelperReviewActionIntent`。
2. 校验 binding 是否完整。
3. 分派 Accept / Reject / Batch。
4. 调用 `FBlueprintHelperReviewActionService`。
5. 不直接刷新 UI。

### 5.4 `FBlueprintHelperReviewActionService`

职责：

1. 执行真实 Accept。
2. 执行真实 Reject。
3. 更新 Review evidence/status。
4. 触发 ReviewStore pending 状态变化。

### 5.5 `FBlueprintHelperReviewSnapshotRestoreService`

职责：

1. 使用 evidence before snapshot 作为 Reject 目标。
2. 执行 Blueprint、Component、WidgetTree、DataAsset、DataTable、Signature、Graph 等 snapshot restore。
3. 只处理业务恢复，不处理 UI。

### 5.6 `FBlueprintHelperReviewNotificationService`

职责：

1. 统一 Accept/Reject 成功、失败、部分成功、全失败、处理中提示。
2. 输出用户友好的本地化文本。
3. 不暴露后台数据字段给 UI 文案。

## 6. Presenter 层职责

Presenter 层负责把 Data 投影成 UI 可消费模型，不执行真实业务。

### 6.1 `FBlueprintHelperReviewPanelPresenter`

职责：

1. 订阅 ReviewStore pending changed。
2. 调用 `ReviewPanelStateService` 重建 `ReviewPanelState`。
3. 对 UI 广播 `PanelStateChanged`。
4. 分发 UI action intent 到 `ReviewCommandService`。

### 6.2 `FBlueprintHelperFinalChangePresenter`

职责：

1. 从 `ReviewPanelState` 构建左侧 FinalChange tree。
2. 处理 selection model。
3. 不执行 Accept/Reject。

### 6.3 `FBlueprintHelperReviewSurfacePresenter`

职责：

1. 从 `ReviewSurfaceDiffModel` 构建 Surface 行模型。
2. 给 Row 提供 diff color、action visibility、binding。
3. 不直接调用 ReviewActionService。

### 6.4 `FBlueprintHelperReviewRowPresenter`

职责：

1. 给单个 Row 提供显示文本。
2. 给单个 Row 提供 `FBlueprintHelperReviewRowBinding`。
3. 给单个 Row 提供 diff state。

Presenter 禁止内容：

1. 不直接改 ReviewStore。
2. 不执行 rollback。
3. 不保存 Slate row geometry 作为业务状态。
4. 不使用 delay/retry。

## 7. UI 层职责

UI 层只负责渲染和发出 intent。

UI 允许：

1. 渲染 Presenter 提供的 RowModel。
2. 发送 `FBlueprintHelperReviewActionIntent`。
3. 响应 `PanelStateChanged` 刷新自身。
4. 广播 Row geometry registered。

UI 禁止：

1. 直接删除 `ChangeItems`。
2. 直接修改 ReviewEvent status。
3. 通过 SearchText fuzzy 查找 action 目标。
4. 保存 Diff 是否存在的业务状态。
5. 用 delay/ActiveTimer/retry 补生命周期问题。

## 8. 标准执行链路

### 8.1 Accept 单条

```text
UI Row Accept
-> FBlueprintHelperReviewActionIntent{ChangeId, AtomicTargetId, Surface}
-> ReviewPanelPresenter::DispatchAction
-> ReviewCommandService::Accept
-> ReviewActionService::AcceptVisibleChange
-> ReviewStore 更新 evidence status
-> ReviewStore 广播 PendingReviewChanged
-> ReviewPanelPresenter::OnStoreChanged
-> ReviewPanelStateService::BuildState
-> UI 收到 PanelStateChanged
-> 对应 Row diff 消失
```

### 8.2 Reject 单条

```text
UI Row Reject
-> FBlueprintHelperReviewActionIntent{ChangeId, AtomicTargetId, Surface}
-> ReviewPanelPresenter::DispatchAction
-> ReviewCommandService::Reject
-> ReviewSnapshotRestoreService restore before snapshot
-> ReviewActionService 标记 rejected
-> ReviewStore 广播 PendingReviewChanged
-> ReviewPanelPresenter::OnStoreChanged
-> ReviewPanelStateService::BuildState
-> UI 收到 PanelStateChanged
-> 对应 Row diff 消失或显示失败状态
```

### 8.3 Reject lifecycle root

```text
UI Reject Root
-> ReviewActionIntent(root ChangeId)
-> ReviewCommandService 识别 lifecycle root
-> ReviewActionService restore root
-> ReviewStore 根据 ParentChangeId / AtomicTargets 重新计算 pending
-> ReviewStore 广播 PendingReviewChanged
-> ReviewPanelStateService 重建 FinalChange tree 和 SurfaceDiffModel
-> Root 与子 ReviewEvent 一起从 UI 消失
```

子项是否消失必须由 Store 新状态决定，UI 不允许手动删 child。

## 9. 当前实现偏离点

| 目标要求 | 当前实现 | 风险 |
|---|---|---|
| ReviewStore 是唯一权威 | `SBlueprintHelperReviewPanel` 直接 `ChangeItems.Remove/RemoveAll` | Store/UI 双权威 |
| Row action 精确绑定 | Row 通过 `SearchText` 查 `RowHighlightModel` | 可能误操作其他 ReviewEvent |
| fuzzy 只用于定位 | action fallback 也使用 fuzzy | Accept/Reject 可能误命中 |
| Surface 刷新统一 | `SBlueprintHelperReviewPanel::OnRowHighlightStateChanged` 中 switch 各 Surface | 新增或修改 Surface 容易漏刷新 |
| Row 只读模型投影 | Row 从静态 `RowHighlightModel` 查询状态 | 静态状态可能残留 |
| StoreChanged 是唯一业务刷新入口 | action 后手动刷新，再 StoreChanged 补刷 | 顺序竞争，状态不一致 |
| UI 层只发 intent | UI 层参与状态删除、状态刷新、命令编排 | 违背高内聚低耦合 |
| 无旧架构兼容 | 仍存在旧 RowHighlight action、legacy 风格回调 | v2 行为反复回退 |

## 10. 必须移除的旧路径

1. `SBlueprintHelperReviewPanel` 中 Accept/Reject 成功后直接删除 `ChangeItems` 的路径。
2. `FBlueprintHelperReviewRowHighlightModel::AcceptHighlightedRow`。
3. `FBlueprintHelperReviewRowHighlightModel::RejectHighlightedRow`。
4. `FindActionRowHighlightEntry` 中 action fuzzy fallback。
5. 以 `SearchText` 作为 action identity 的所有 Row 按钮实现。
6. 主 Panel 中按 Surface 手写的刷新 switch。
7. 旧 transaction Review、legacy rollback、legacy cleanup 相关入口。
8. UI 层 delay/retry/ActiveTimer geometry 补偿。

## 11. 分阶段实施计划

### 阶段 1：建立 v2 Data 类型

交付内容：

1. `FBlueprintHelperReviewPanelState`。
2. `FBlueprintHelperReviewSurfaceDiffModel`。
3. `FBlueprintHelperReviewRowBinding`。
4. `FBlueprintHelperReviewActionIntent`。
5. `FBlueprintHelperReviewTransientActionState`。

完成标准：

1. 新类型不依赖 Slate。
2. Row action identity 不再依赖 SearchText。
3. 类型命名和字段职责清晰。

### 阶段 2：实现 `ReviewPanelStateService`

交付内容：

1. 从 pending visible changes 构建 FinalChange tree。
2. 从 pending visible changes 构建 SurfaceDiffModel。
3. 构建 lifecycle root / child 关系。
4. 生成 RowBinding。

完成标准：

1. Component/MyBlueprint/WidgetTree/DT/ST/DA/Details 都能从同一状态服务拿到 diff model。
2. Store 快照相同则 UI 投影稳定一致。
3. 新增资产必须成为该资产 ReviewEvent 的 Root。

### 阶段 3：重写 Row action 链路

交付内容：

1. 所有 Row 持有 `FBlueprintHelperReviewRowBinding`。
2. Row button 只发送 `FBlueprintHelperReviewActionIntent`。
3. 删除 RowHighlightModel action API。
4. fuzzy matching 只保留给 geometry/debug。

完成标准：

1. 点击任意 Row 的 Accept/Reject 只命中自身 ReviewEvent。
2. 不再出现 Event 被点但 Function 被 Reject 的问题。
3. 不再出现同名/近似名称误命中问题。

### 阶段 4：StoreChanged 成为唯一业务刷新入口

交付内容：

1. Accept 成功后不再 UI 本地删除 ChangeItems。
2. Reject 成功后不再 UI 本地删除 ChangeItems。
3. lifecycle root cascade 不再由 UI 手动删 child。
4. `RefreshFromReviewStoreIfChanged` 是唯一改写 pending UI model 的入口。

完成标准：

1. FinalChange 与子 Panel 状态永远一致。
2. WidgetTree/Component 子项被 root Reject 后，对应 child ReviewEvent 同步消失。
3. 不再出现子 Panel Diff 消失但 FinalChange 仍在，或反过来的状态。

### 阶段 5：统一 Surface 刷新接口

交付内容：

1. 定义 Surface View / Presenter 统一刷新接口。
2. Component/MyBlueprint/WidgetTree/DT/ST/DA/Details 全部实现统一入口。
3. 主 Panel 不再按 Surface 手写刷新细节。

完成标准：

1. 所有 Surface 接收同一类 `PanelStateChanged` 或 `SurfaceDiffModelChanged` 事件。
2. 任意单条 Accept/Reject 后，对应 Surface Row 立即根据新状态刷新。
3. 不再需要处理完全部 ReviewEvent 才刷新。

### 阶段 6：清理旧架构

交付内容：

1. 删除旧 action fallback。
2. 删除旧 RowHighlight 静态 action 状态。
3. 删除旧 transaction Review 残留。
4. 删除 UI-local 业务状态补丁。

完成标准：

1. 编译通过。
2. `rg` 检索不到旧 action API 被 UI 调用。
3. `rg` 检索不到 transaction Review 旧路径。
4. `rg` 检索不到 UI delay/retry 作为刷新补偿。

### 阶段 7：覆盖测试

测试范围：

1. Blueprint asset root。
2. Component add/modify/reject。
3. MyBlueprint function/event/macro/dispatcher/variable。
4. WidgetTree root 与 child。
5. DataAsset property。
6. DataTable row 与 field。
7. Structure row/field。
8. Details row focus/diff。
9. GraphPanel navigation/diff。

完成标准：

1. 单条 Accept/Reject 命中准确。
2. 批量 Accept/Reject 状态准确。
3. FinalChange 与子 Surface 状态一致。
4. 所有 Surface 行刷新即时、事件驱动。
5. DebugBundle 能记录 action intent、service result、store refresh、presenter rebuild、surface refresh。

## 12. 风险与处理

### 12.1 迁移范围大

风险：一次性替换可能引入编译问题。

处理：按阶段提交，阶段内保持编译通过。

### 12.2 旧 UI 代码耦合较深

风险：`SBlueprintHelperReviewPanel` 当前承担过多职责。

处理：先抽 Data 和 Service，再逐步让 UI 退化为 View。

### 12.3 Surface 之间能力不一致

风险：Component/MyBlueprint/WidgetTree/DT/ST/DA/Details 原本刷新方式不同。

处理：统一到 `SurfaceDiffModel`，Surface 只负责渲染自己的模型。

### 12.4 Reject 真实失败不是 UI 问题

风险：UI 链路修好后，Snapshot restore 仍可能失败。

处理：失败应通过 Service result 和 DebugBundle 暴露，不在 UI 层隐藏或补偿。

## 13. 验收标准

ReviewPanel v2 完成后必须满足：

1. `Data / Service / Presenter / UI` 四类职责边界清晰。
2. UI 只渲染和发 intent。
3. Presenter 只投影视图模型。
4. Service 只执行业务和状态转换。
5. Data 只保存状态。
6. Store 是 pending Review 唯一权威。
7. 所有 Surface 刷新走统一模型。
8. 所有 Row action 使用 canonical binding。
9. 不存在旧 action/fuzzy/transaction 兼容路径。
10. 不存在 delay/retry UI 补丁。
11. 编译通过。
12. 覆盖测试通过，未完成项必须写明差距。

## 2026-05-19 Row Diff 重绘调试记录

- 现象：每个标题下 Accept 一个 Review 后，FinalChange 数量下降且 Store refresh 触发，但 Component/MyBlueprint 等 Row 仍显示 Diff 高亮。
- DebugBundle 结论：`Accept change ... success=1`、`ReviewUiRefresh reason=store_refresh`、各 Surface `ReviewRowHighlightState ... result=refresh_rows` 都存在，说明 Accept 服务和事件广播链路有效。
- 根因定位：Row 背景颜色查询使用 fuzzy match，而 Row action binding 使用 exact match；已处理的 exact ReviewEvent 被移除后，Row 仍可能被其他剩余 ReviewEvent 模糊命中并继续显示 Diff，造成“Accept/Rej 没有重绘”的错觉。
- 修复：Row 背景颜色查询改为 exact identity；fuzzy match 不再参与 native Row Diff 绘制身份判定。
- 距离期望差距：需要编辑器内手动验证 Component/MyBlueprint/WidgetTree/DT/ST/DA Row 在单条 Accept/Reject 后是否立即移除对应 Diff。

## 2026-05-19 Store Refresh 后 Selection 回退修复

- 现象：Accept 下一个资产的 ReviewEvent 后，ReviewPanel 跳回上一个资产或列表顶部资产。
- DebugBundle 结论：Accept 成功后 `ReviewUiRefresh reason=store_refresh` 的 `preferredAsset` 回退到旧资产，说明 Store refresh 后的选择策略没有以本次被处理的 change 所属资产为锚点。
- 根因定位：`RefreshFromReviewStoreIfChanged()` 在原 `SelectedChangeId` 被 Accept/Reject 移除后直接选择 `ChangeItems[0]`，绕过了已有的 `SelectNextChangeAfterRemoval()` 同资产后继选择策略。
- 修复：Store refresh 前记录原选中 change 的 asset path 和 index；refresh 后如果原 change 已消失，调用 `SelectNextChangeAfterRemoval(PreviousSelectedAssetPath, PreviousSelectedIndex)`，优先停留在同资产下一条 pending Review；同资产清空后才跳到后续资产。
- 距离期望差距：需要编辑器内手动验证跨资产连续 Accept/Reject 时不会回跳到上一个资产。

## 2026-05-19 DA/ST Row Diff 精确 Token 匹配修复

- 现象：DA 和 ST 的 Row 不再绘制 Diff。
- DebugBundle/Record 结论：DA ReviewEvent 的精确 key 为 `object_property:SmokeLabel` / `object_property:SmokeFloat`；ST ReviewEvent 的精确 key 为 `struct_field:DisplayName` / `struct_field:Damage` 等。DA/ST Row 的 `SearchText` 是复合字符串，包含这些 key 作为 token，但不是整串等于 key。
- 根因定位：上次为避免 fuzzy 误命中，将 Row 背景查询改为整串 exact matching；这修复了 Component/MyBlueprint 的误高亮，但 DA/ST/DT 这类复合 SearchText 需要 token exact alias。
- 修复：`FindExactRowHighlightEntry()` 先查整串 exact，再按空白拆分 SearchText 并逐个 token 做 exact 查找；不恢复 fuzzy contains 匹配。由于颜色、Action 可见性、Row Accept/Reject 都复用该函数，DA/ST Row Diff 与 Row Action 会一并恢复。
- 距离期望差距：DA/ST/DT Row 已恢复 Diff 绘制路径；后续若出现单 Surface 刷新异常，按统一 RowBinding/SurfaceView 刷新链路排查，不再引入 Surface 专用 UI 补丁。

## 2026-05-19 ReviewPanel 基本稳定状态同步

- 当前结论：ReviewPanel v2 经过本轮修复后，暂未发现阻断级或系统性大 bug。此前反复出现的 Row Diff 残留、Accept/Reject 后跳回旧资产、DA/ST Row 不绘制 Diff 等问题，均已定位到具体链路并完成通用性修复。
- 已确认不修改项：`DA_RP_UI_Object.SmokeLabel` Reject 后回到非空字符串属于当前 Review evidence before snapshot 的预期行为。该 Review 的 baseline 为 `ObjectProperty baseline`，不是空字符串，因此不应修改 rollback 逻辑。
- 架构约束：后续 ReviewPanel 问题仍按 v2 三层/四层职责处理，即 Data/Service/Presenter/UI 分离，Store 是唯一 pending Review 权威，UI 只发 intent 和渲染状态，不允许恢复旧 transaction、旧 action fallback、UI 本地删项、delay/retry 刷新补丁。
- 当前剩余：没有明确阻塞内容。若用户继续发现小问题，需要基于最新 DebugBundle 判断是 evidence 生成、snapshot restore、PanelState 投影、SurfaceView 刷新，还是具体 RowBinding 缺失，不再按单 Panel 局部缝补。
