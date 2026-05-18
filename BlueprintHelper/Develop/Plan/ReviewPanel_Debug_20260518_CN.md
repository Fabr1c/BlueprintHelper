# ReviewPanel Debug 分析记录 2026-05-18

## 1. 本次输入来源

用户已完成 ReviewPanel 手动 UI 验证，并在以下文档中用 `- [x]`、`- [o]`、`- [ ]` 和行尾测试注释记录结果：

1. `BlueprintHelper/Develop/Plan/ReviewPanel_UI_ManualValidation_Runbook_20260518_CN.md`
2. `BlueprintHelper/Develop/Plan/ReviewPanel_UI_Validation_Runbook_20260515_2018_CN.md`

本文件只做 debug 分析归档，不把未修复问题标记为完成。

## 2. 总体判断

1. 部分 Graph Reject 无效的判断基本成立，但需要拆成两类：
   1. 旧 ReviewEvent 使用废弃 graph hash，当前 Reject 已迁移到 semantic snapshot hash guard，因此会被判定为 `current_state_changed` 或 `needs_action`，这是安全拦截，不是 rollback 能力本身缺失。
   2. 新 ReviewEvent 如果仍然 Reject 无效，则不是“旧 hash 兼容问题”，而是当前 semantic snapshot、target key、rollback journal 或 UI action-result 同步链路的缺陷。
2. 当前 UI 对 Reject 失败没有足够显式反馈。代码侧主要写入 DebugPanel log 和 `NeedsActionReason`，没有发现中央弹窗或右下角 toast 通知链路，因此用户感知为“点击无效”是真实问题。
3. DA/ST/DT 的 snapshot restore handler 已存在，不能简单归类为“未实现”。用户看到的失败更可能来自 hash guard、target key 解析、asset path 解析、或 action 后 presenter/highlight state 未刷新。
4. 多个 UI 问题是实际存在的视觉/交互问题，不应再归为自动化未覆盖：Details diff 框、MyBlueprint 双击跳图、DataTable 行高波动、处理后 row diff 残留、异步操作进度不可见。

## 3. Graph Reject 与 hash 迁移分析

### 3.1 现有代码行为

代码路径：

1. `FBlueprintHelperReviewRejectService::RejectVisibleChangeWithDefaultDispatcher`
2. `ValidateReviewRejectSemanticRecordedAfterHash`
3. `FBlueprintHelperReviewBaselineSnapshotService::CaptureTargetSnapshot`
4. 当前 snapshot hash 与 `FBlueprintHelperReviewAtomicTarget::RecordedAfterHash` 做严格比较。

已存在自动化用例：

1. `BlueprintHelper.Review.Store.UsesSemanticHashForGraphTarget`
2. `BlueprintHelper.Review.Reject.GraphTargetUsesSemanticHashGuard`

该用例明确构造 `RecordedAfterHash = "legacy_graph_hash"`，并断言 Reject 失败且进入 `NeedsAction/current_state_changed`。

### 3.2 是否真实存在

结论：真实存在，但应标为“旧记录/旧 hash schema 的安全拦截”。

如果用户当前点击的是 graph hash 迁移前生成的 ReviewEvent，则 Reject 无效符合当前代码的保护逻辑。它不是 rollback 没执行，而是执行前被 current-state guard 拦截。

### 3.3 需要补充的 DebugBundle 字段

为了避免后续只能猜测，Graph Reject 失败时 DebugBundle 应记录：

1. `hash_schema`: `semantic_snapshot` / `legacy_graph_hash` / `unknown`
2. `target_key`
3. `target_kind`
4. `recorded_after_hash`
5. `current_hash`
6. `baseline_hash`
7. `after_snapshot_hash`
8. `after_snapshot_present`
9. `before_snapshot_present`
10. `reject_guard_result`
11. `reject_failure_message`

### 3.4 建议处理策略

1. 对旧 hash schema 的 pending ReviewEvent，不应静默失败，应显示 `stale_review_hash_schema` 或 `current_state_changed`。
2. UI 需要给出明确提示：该 ReviewEvent 来自旧 hash 规则，不能安全 Reject，需要重新生成 ReviewEvent 或执行显式迁移。
3. 不建议无条件绕过 hash guard，否则会把 Reject 变成不安全 rollback。

## 4. 用户手动测试提到的问题清单

| 编号 | 问题 | 是否真实存在 | 分析 |
|---|---|---|---|
| RP-D-001 | 选择新增组件 Review 时 Details 能显示组件信息；选择变量时 Details 无法绘制 diff 框 | 是 | Details 定位与 diff 绘制是两条链路。代码会调用 DetailsView scroll/highlight，但 diff 框依赖 Slate row geometry 搜索，可能返回 `details_row_geometry_not_ready` 或 `no_matching_details_text`。用户观察符合“定位成功但 diff 绘制失败”。 |
| RP-D-002 | 双击 MyBlueprint 中 Diff 函数/事件无法让中央 GraphPanel 跳转，中央图表为空 | 是 | MyBlueprint row 只有 `NavigateChangeId` 非空且 `OnNavigateToGraph` 绑定时才处理双击。`NavigateChangeId` 当前依赖 change target text 与 row search text 匹配。若 signature/function/event Review 的 target text 与 graph row 名称不一致，双击会退回默认行为或选中后 GraphPanel 找不到图。 |
| RP-D-003 | Final Changes 面板内点击同一条 graph review 的 Reject 部分无效 | 是，需区分旧记录与新记录 | 旧 graph hash 记录会被 semantic hash guard 拦截，表现为 Reject 无效。新记录若仍失败，需要看 DebugBundle 的 `recorded_after_hash/current_hash` 和 rollback journal。 |
| RP-D-004 | target missing / current state changed 没有中央弹窗或右下角提示 | 是 | 代码中失败主要写入 `AddDebugMessage` 和 `NeedsActionReason`，未看到 `SNotification`/toast 通知链路。用户感知为无反馈成立。 |
| RP-D-005 | DataTable 下方 selected row details 看起来复用了旧 ST 行 | 基本成立 | DataTable presenter 当前构造 `FBlueprintHelperReviewDataAssetRowItem` 并调用 `GenerateDataAssetRow`，不是直接复用完整 `FBlueprintHelperReviewStructurePresenter`。视觉可能接近旧 ST/DataAsset row，而非最新结构化 ST presenter 统一入口。 |
| RP-D-006 | DataTable Row hover 时行高波动，padding 可能改错位置 | 是 | DataTable row 同时有 `SMultiColumnTableRow`、内层 `SBorder Padding(4,3)`、actions column visibility 变化。按钮从 collapsed 到 visible 可能改变 row desired size。ST row 也复用类似 row shell，因此相同波动会复现。 |
| RP-D-007 | DataTable 单行 Accept/Reject 后，行颜色仍被其他行 diff 污染，处理后的 Row 依然能 hover | 是 | RowHighlight state 以 surface/asset 为静态状态，并允许 fuzzy match。处理一行后如果同资产仍有 pending row，旧 search key 或相近 key 可能继续命中，造成已处理 row 残留颜色和 action。 |
| RP-D-008 | DataTable Reject 新增行后，真实 row 未删除，FinalReview 也未移除 | 是，需 DebugBundle 定位 | Snapshot restore 已支持 `datatable_row` 且 snapshot `exists=false` 时会 `RemoveRow`。如果真实行未删除，优先怀疑 hash guard 未通过、target key 未匹配到行、BeforeSnapshotJson 不正确、或 action result 失败但 UI 未提示。 |
| RP-D-009 | ST Reject 一个字段后，其他字段 Review 一起消失 | 是，非 graph hash 问题 | 更像 review target resolution / visible change collapse / target key 粒度过宽。Store merge 仍存在按 `LocationKey` 合并 visible change 的路径，如果多个字段落入同一 visible change 或 action target keys 被批量解析，Reject 会影响多个字段。 |
| RP-D-010 | DA Accept 后主 panel 刷新，但新增项都变成 modify | 需要进一步确认语义 | 如果 Accept 只归档当前 root/leaf，而同资产其它 property child review 仍 pending，颜色从 added 变为 modified 可能是剩余 child review 的颜色。若已无 pending 仍显示 modify，则是 refresh/highlight 残留。 |
| RP-D-011 | DA Reject 后属性真实值没有回滚 | 是，需 DebugBundle 定位 | ObjectProperty restore handler 已存在，bool 解析也有支持。如果没回滚，优先检查 `object_property` target 的 current hash guard、PropertyPath、BeforeSnapshotJson value，以及 Reject action result 是否进入 `RejectFailed/NeedsAction`。 |
| RP-D-012 | DetailsView 显示属性当前值但没有 diff 框 | 是 | 与 RP-D-001 相同，定位成功不代表 overlay geometry 成功。应单独修 Details diff geometry 或提供 details row native highlight。 |
| RP-D-013 | 单条 Accept/Reject 是否仍会误批量处理同 transaction 下其他 target | 部分存在 | 变量/组件路径之前已修过 target key 精确匹配，但 ST 字段和 DataTable row 当前仍有用户可见的误影响或残留问题。应按 target kind 分开验证，不能整体标完成。 |
| RP-D-014 | `current_state_changed` 场景未覆盖，无法确认 UI 行为 | 未覆盖，不算通过 | 代码支持进入 NeedsAction，但用户未触发该场景。UI 显式反馈又缺失，因此该项仍应作为待验证/待改进。 |
| RP-D-015 | Asset lifecycle root Reject 异步进度不可见 | 是，属于 UX 缺口 | 当前异步状态主要通过行状态和 DebugPanel log 表示。用户需要根 review 下方有一行进度/结果展示，这不是 rollback 正确性问题，是可观察性不足。 |

## 5. 旧手动文档中仍有参考价值的问题

旧文档 `ReviewPanel_UI_Validation_Runbook_20260515_2018_CN.md` 中部分问题已被最新文档覆盖或修正，当前只保留仍有参考价值的项：

1. DataTable row Reject 曾出现 “Final Changes 移除但 row 值没有回滚，diff 框在所有 row 都 archived 后才刷新”。该问题与 RP-D-007 / RP-D-008 同源，仍有价值。
2. Components/MyBlueprint 处理后 diff 框残留曾出现 “同 panel 仍有其他 pending 时整个 panel 状态不刷新”。该问题与当前 RowHighlight 静态状态和 fuzzy match 风险相关，仍有价值。
3. 旧文档提到 DA `object_property:bSmokeFlag` Reject、ST `struct_field:*` Reject 的具体错误。代码已有对应修复，但本次 DA/ST 仍出现新失败，应按新 DebugBundle 重新确认，不直接套用旧结论。
4. 旧文档的 DebugPanel `LoadBundle/CopyPath/CaptureFocus` 未完成项，在最新文档中已标为通过，可视为历史项，不再作为当前阻塞。

## 6. 当前最可能根因排序

1. 旧 graph hash 与 semantic snapshot hash 不兼容，导致旧 graph ReviewEvent Reject 被安全拦截。
2. Reject 失败只记录到 DebugPanel 或 item reason，没有用户可见通知，造成“无效点击”体验。
3. RowHighlight state 以 asset/surface 为范围，action 后虽然调用 `InvalidateAssetStates`，但随后重建时仍可能被剩余 pending change 的 fuzzy key 命中，造成颜色污染和 hover action 残留。
4. Store merge / action target resolution 对 ST 字段和 DataTable row 的 target 粒度仍可能过宽。
5. Details diff 依赖通用文本 geometry 搜索，变量/属性 Details row 不稳定，导致定位成功但 overlay 不出现。
6. DataTable selected row details 没有真正收敛到统一 ST presenter，因此新旧 row 风格和 padding 行为不一致。

## 7. 建议下一轮 Debug 顺序

1. 先导出一次包含失败 Graph Reject 的 DebugBundle，检查 `recorded_after_hash` 是否为 legacy graph hash 或是否无法与 `after_snapshot_json` 计算值对应。
2. 为 Reject failure 增加 UI 可见反馈：toast/通知 + Final Changes row 内状态文字，至少显示 `needs_action/current_state_changed/current_hash_unavailable`。
3. 在 DebugBundle 中补齐 hash guard 字段，避免后续只能凭 UI 猜测。
4. 修 RowHighlight key 生命周期：Accept/Reject 成功后按 `change_id` 和 `target_key` 精确移除高亮缓存，避免同 asset 其它 pending 通过 fuzzy match 污染已处理 row。
5. 修 ST/DataTable target 粒度：Reject 单字段/单行时必须只传单一 target key，并在 Store purge 后保留其它 pending target。
6. 修 Details diff：不要只依赖 DetailsView 文本搜索；对变量/组件/property 使用 property path/native detail row handle 建立稳定 anchor。
7. 再修 DataTable selected row details 统一复用 ST row/presenter，并固定 hover action 不改变 row desired height。

## 8. 本轮结论

用户提到的问题大部分真实存在。Graph Reject 的“部分无效”很可能包含旧 graph hash 迁移造成的安全拦截，但 UI 没有显式告知失败原因，这是独立且必须修的可观察性问题。DA/ST/DT 的 Reject 失败不应归咎于 graph hash，应分别按 snapshot restore、target key、row highlight state 和 store purge 粒度排查。

## 9. 阻塞内容

1. 当前没有本次失败点击对应的最新 DebugBundle 路径，因此无法把每个 Reject 失败精确归因到 `legacy_graph_hash`、`current_hash_unavailable`、`target_keys_not_found` 或 snapshot restore failure。
2. 若要确认“新生成的 graph ReviewEvent 是否仍 Reject 无效”，需要重新构造一批当前版本 ReviewEvent 并导出 DebugBundle。
