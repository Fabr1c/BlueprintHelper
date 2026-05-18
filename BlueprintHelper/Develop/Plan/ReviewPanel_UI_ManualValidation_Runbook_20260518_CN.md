# ReviewPanel 用户手动 UI 待处理 Runbook 2026-05-18

本文已移除明确通过的 `[x]` 项，只保留未通过 `[ ]`、部分通过 `[o]`，以及虽然曾标 `[x]` 但测试注释明确暴露残留问题的项目。旧 Review 记录删除后，后续重新生成 ReviewEvent 时按本文件复测。

## 0. 测试前置

1. 删除旧 ReviewStore / Review 记录后，重新构造当前版本生成的 ReviewEvent。
2. 打开 `Blueprint Helper` 面板，切换到 `Review` 页。
3. 所有 Reject 失败都需要导出 DebugBundle，并记录失败时的 selected change id、target key、target kind、recorded/current hash。
4. 本轮不再使用旧 graph hash 记录判断新功能是否可用；旧记录只用于确认 `stale/current_state_changed` 是否有明确 UI 提示。

## 1. Blueprint / Details / MyBlueprint

- [ ] 选择变量 Review 时，右侧 DetailsView 应绘制对应变量/property diff 框。`当前：选择新增组件 Review 时 Details 能显示组件全部信息；选择变量时右侧 Details 无法绘制 diff 框。`
- [ ] 双击 MyBlueprint 中的 Diff 函数/事件，应让中央 GraphPanel 跳转到对应图表。`当前：双击无法跳转，中间图表为空。`

复测要点：
1. 组件 Details 能定位不等于变量 Details diff 通过。
2. 双击跳图需要验证 GraphPanel 不为空，且跳到对应函数/事件图。

## 2. GraphPanel / Graph Review

- [ ] Final Changes 面板内点击同一条 graph review 的 Accept / Reject 也必须生效。`当前：出现部分 Reject 无效。`
- [ ] target already missing、current state changed、stale hash schema 等失败状态需要有可见反馈。`当前：没有中央弹窗或右下角提示显示失败。`

复测要点：
1. 新生成的 graph ReviewEvent 必须使用当前 semantic snapshot hash。
2. 旧 hash 记录如果被拒绝，应显示 `current_state_changed` 或 `stale_review_hash_schema`，不能表现为静默无效。
3. DebugBundle 需要包含 `target_key`、`target_kind`、`baseline_hash`、`recorded_after_hash`、`current_hash`、`hash_schema`。

## 3. DataTable

- [ ] 下方 selected row details 应复用当前 Structure/ST 行展示字段和值。`当前：看起来仍像旧 ST 行。`
- [ ] Row border 上下 padding 应固定为 3.0，hover 不应引发行高波动。`当前：Hover 到 Row 上时行高有波动，疑似 padding 或 action visibility 改错位置。`
- [ ] 单行 Accept / Reject 后，已处理 row 不应继续被其它 pending row 的 diff 颜色污染，也不应继续显示 hover action。`当前：处理后行颜色仍被其它行 diff 颜色污染，并且处理后的 Row 依然能被 Hover。`
- [ ] Reject 新增行后，真实 row data 应删除，Final Changes 与 DataTable presenter 同步移除对应 diff。`当前：Reject 新增行后，行没有被删除，FinalReview 也没有移除。`

复测要点：
1. 同一个 DataTable 至少生成一条纯新增行和一条 add+update 合并行。
2. Reject 纯新增行后，在真实 DataTable 中该行必须消失。
3. 接受/拒绝一行后，其它 pending row 仍应可操作，但已处理 row 不应残留 diff 或 action。

## 4. Structure / ST

- [ ] 字段 row 命中 diff 后行高不能浮动。`当前：命中 diff 后行高仍会浮动，和 DataTable 一样。`
- [ ] Reject 单个字段变更后，只能影响该字段，结构字段真实状态回到 baseline，Final Changes 同步移除该字段 review。`当前：Reject 一个字段后，其它字段 Review 一起消失。`

复测要点：
1. 结构体至少生成 3 个字段 Review。
2. Reject 中间字段时，前后字段 Review 必须保留 pending。
3. 需要确认 Store purge / target key 粒度是否只针对单个 `struct_field`。

## 5. DataAsset / ObjectProperty / Details

- [ ] Accept 后主 panel diff 状态语义需要正确。`当前：确实刷新了，但原本都是新增，Accept 后都变成 modify。`
- [ ] Reject 后属性真实值必须回滚，Final Changes 与主 panel diff 同步刷新。`当前：Reject 后也没有回滚。`
- [ ] DetailsView 显示属性当前值后，也需要绘制对应 diff。`当前：确实显示了属性当前值，但没有绘制 diff。`

复测要点：
1. DataAsset 至少覆盖 string、float、bool 三类属性。
2. Reject bool 属性时需要确认空串/True/False 的 baseline 值恢复。
3. DebugBundle 需要记录 property path、before snapshot、after snapshot、current hash。

## 6. Accept / Reject 策略

- [o] 单条 Accept / Reject 不应误批量处理同 transaction 下其它 target。`当前：前面仍有误处理迹象，尤其 ST/DataTable 仍需复测。`
- [o] Reject 当前状态与 recorded after hash 不一致时，UI 应显示 `needs_action/current_state_changed`，review 保持 pending。`当前：本轮未覆盖该场景。`
- [ ] Asset lifecycle root Reject 异步进度需要可见。`当前：异步操作不知道多久才完成，根 review 下方需要占一行显示进度/成功/失败。`

复测要点：
1. 变量/组件、ST 字段、DataTable 行、DataAsset 属性分别验证单 target 操作。
2. 人为制造 current-state mismatch 后，必须确认 UI 可见反馈和 pending 保留。
3. Asset root Reject 时应显示异步状态，而不是只靠 DebugPanel log。

## 7. 重新生成 ReviewEvent 建议覆盖集

旧 Review 记录删除后，建议重新构造以下当前版本 ReviewEvent：

1. Blueprint：新增组件、变量、函数/事件签名、至少一个可跳转图表。
2. Graph：当前 semantic hash 版本的 graph node/block Review，用于验证 Final Changes Reject。
3. DataTable：一条纯新增行，一条 add+update 合并行，字段包含 string、float/int、bool。
4. Structure：至少 3 个字段，便于验证 Reject 单字段不影响其它字段。
5. DataAsset：string、float、bool 三个 ObjectProperty Review。
6. Asset lifecycle root：新增资产 root + child reviews，用于验证异步进度和 cascade 行为。

## 8. 本轮阻塞内容

1. 等待用户删除旧 Review 记录后重新生成 ReviewEvent。
2. 没有最新失败点击对应的 DebugBundle 前，无法把每个 Reject 失败精确归因到旧 hash、current hash mismatch、target key 不匹配或 snapshot restore 失败。