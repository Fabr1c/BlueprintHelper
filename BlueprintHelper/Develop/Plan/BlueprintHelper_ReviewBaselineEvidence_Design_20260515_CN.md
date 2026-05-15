# BlueprintHelper Review Baseline / Evidence 设计、规则与实现计划

日期：2026-05-15

状态：设计已确认，核心 baseline/evidence、snapshot restore、净效果过滤已部分实现并通过多轮编译/CLI 闭环验证；剩余差距以各轮记录为准。

## 1. 文档目标

本文定义 Review 系统后续的 baseline、evidence、Final Changes 去重、Reject / Accept、Graph normalize、DebugBundle 的统一规则。

核心目标：

1. Final Changes 面板中同一 scope 的多次修改只显示最终净变更。
2. Reject 永远回到该 scope 第一次 pending evidence 创建前的状态。
3. Accept 后当前状态成为新的 baseline。
4. `after == before` 时自动移除 Review，不保留 Debug evidence。
5. Review 系统关注文件最终状态，不追踪中间副作用。

## 2. 术语

`scope`：Review 最小可审查对象，例如变量、组件、函数图、DataTable row、DataAsset property、WidgetTree node。

`baseline / before`：第一次 pending evidence 创建前，该 scope 的规范化状态。

`after`：当前最新资产状态下，该 scope 的规范化状态。

`evidence`：描述某个 scope 从 `before` 到 `after` 的 Review 证据。Final Changes 只展示 evidence 的最终状态。

`scope identity`：用于判断两个操作是否属于同一 Review scope 的稳定身份。

`scope snapshot`：某个 scope 的可规范化状态描述。

`normalized hash`：对 scope snapshot 做规范化后得到的稳定 hash，用于判断 `after == before`。

## 3. 总体方案

采用“方案 2 为主，AssetBaseline 作为数据来源之一”的模型。

规则：

1. 第一次 scope 变更前，捕获该 scope 的 `before`。
2. 创建该 scope 的 evidence。
3. 后续同 scope 变更不创建新的 Final Changes Review 项。
4. 后续同 scope 变更只覆盖 evidence 的 `after`、`after_hash`、latest transaction、显示摘要和绘制锚点。
5. 后续同 scope 变更不能覆盖第一次捕获的 `before`、`before_hash`。
6. 如果最新 `after_hash == before_hash`，删除该 evidence。
7. Reject 使用 `before` 恢复该 scope。
8. Accept 删除该 evidence，并把当前状态视为该 scope 后续变更的新 baseline。

## 4. 用户确认规则

### 4.1 净效果为无变更

场景：

```text
A -> B -> A
```

规则：

1. Final Changes 自动移除该 Review。
2. Debug evidence 不保留。
3. 中间操作副作用不追踪。
4. 系统只关注文件最终状态。

### 4.2 删除后重建同名对象

规则：

1. 如果对象有 GUID，用 GUID 作为稳定身份。
2. 如果没有 GUID，不把删除后重建同名对象视为修改。
3. 无 GUID 时拆成“删除 + 新建”。

### 4.3 Rename

规则：

1. 不保留复杂 rename 语义。
2. Rename 拆成“删除旧对象 + 新增新对象”。
3. 如果新对象随后修改默认值或属性，归入新对象 scope。

示例：

```text
变量 A rename 到变量 B，再修改 B 默认值
```

拆成：

```text
删除变量 A
新增变量 B
修改变量 B 默认值
```

### 4.4 类型变化

规则：

1. 类型变化不做复杂字段级恢复。
2. 类型变化拆成“删除旧对象 + 新增新对象”。
3. 新对象的默认值修改归入新对象 scope。

### 4.5 Scope 重叠

规则：

1. 不做复杂重叠 scope 合并。
2. 拆成简单 scope 变更。
3. 子 scope 和父 scope 必须有明确关系，但 Final Changes 仍按简单 scope 展示。

### 4.6 用户手动修改冲突

规则：

1. 用户手动接管后 Reject 不阻塞。
2. Reject 统一回到 `before`。
3. 当前状态是否等于 latest `after` 不作为阻止 Reject 的条件。
4. DebugBundle 可以记录当前状态与 latest `after` 不一致，但不能因此拒绝执行 Reject。

## 5. Scope identity 规则

scope identity 必须稳定，不应依赖 UI 文本、局部化名称、UE 临时 object name。

建议 identity 优先级：

1. BlueprintHelper 写入时生成并写入的 stable metadata。
2. UE 原生 GUID，例如变量 GUID、SCS node GUID、Graph GUID、NodeGuid。
3. Canonical path，例如 `asset + target_kind + graph + property_path`。
4. 无 GUID 且无法稳定识别时，按删除/新增拆分，不做修改覆盖。

禁止作为主 identity：

1. `K2Node_CallFunction_1`。
2. `K2Node_CustomEvent_3`。
3. localized display name。
4. 临时 Slate row 文本。
5. 节点位置。

## 6. Scope snapshot 和 hash

### 6.1 判断 `after == before`

不能用原始 JSON 字符串比较，也不能用 UI 文本比较。

必须使用：

```text
scope snapshot -> normalize -> stable hash
```

判断流程：

```text
第一次变更前：
capture before scope snapshot
normalize before
hash before

每次写入后：
capture after scope snapshot
normalize after
hash after

if after_hash == before_hash:
    remove review evidence
else:
    update after evidence
```

### 6.2 Normalize 通用规则

1. 字段按稳定顺序排序。
2. map 按 key 排序。
3. object path 规范化。
4. 浮点数格式规范化。
5. 默认值格式规范化。
6. 空值与默认值是否等价由 scope resolver 决定。
7. 忽略 editor-only transient 字段。
8. 忽略 Review / Debug transient 字段。
9. 忽略局部化 display text。

### 6.3 典型 scope snapshot 内容

| Scope | Snapshot 内容 |
|---|---|
| Blueprint variable | stable id、name、type、default value、category、tooltip、metadata、replication、exposure flags |
| Component | stable id、name、class、parent、socket、attach rule、template property subset |
| Function signature | stable id、name、inputs、outputs、flags、category |
| Function graph body | normalized semantic graph |
| Macro graph body | normalized semantic graph |
| DataAsset property | property path、normalized value |
| DataTable row | row name、row struct path、normalized row values |
| WidgetTree node | stable id、name、class、parent、slot、normalized property subset |

## 7. Graph normalize 规则

Graph scope 必须使用 normalized semantic graph hash。

### 7.1 Graph hash 参与项

1. graph kind。
2. graph stable identity。
3. node stable identity。
4. node semantic kind。
5. function / event / variable / macro 引用。
6. pin 类型。
7. pin 默认值。
8. exec links。
9. data links。
10. BlueprintHelper stable block / fragment metadata。

### 7.2 Graph hash 不参与项

1. 节点位置。
2. comment 节点。
3. 纯视觉整理。
4. UE 临时 object name。
5. Review diff block transient node。
6. Debug transient node。
7. localized node title。

### 7.3 Comment 和整理线

规则：

1. 用户只加注释，不算 Graph Review。
2. 用户只整理线，不算 Graph Review。
3. reroute / knot 节点如果不改变真实执行或数据依赖，应在 normalize 时折叠。

示例：

```text
A -> Reroute -> B
```

normalize 成：

```text
A -> B
```

### 7.4 Stable node id 优先级

1. BlueprintHelper stable metadata。
2. UE NodeGuid。
3. semantic fingerprint。

semantic fingerprint 只能作为兜底，因为两个相同函数调用节点可能 fingerprint 相同。

### 7.5 第一版 Graph 粒度

第一版以 Graph body 作为净效果判断粒度。

```text
before_graph_hash != after_graph_hash
=> graph body changed
```

Graph diff 绘制仍可使用 fragment / block evidence、recorded bounds、node anchors 做局部展示。

## 8. Evidence 覆盖规则

### 8.1 第一次写入

```text
before = capture scope baseline
after = capture scope after write
create evidence
```

必须保存：

1. scope identity。
2. before snapshot ref 或 before snapshot inline payload。
3. before hash。
4. after snapshot ref 或 after snapshot inline payload。
5. after hash。
6. latest transaction id。
7. source transaction ids。
8. debug basis。
9. display summary。
10. diff anchor。

### 8.2 后续同 scope 写入

```text
existing evidence found by scope identity
before remains unchanged
after is replaced
latest transaction id is replaced
source transaction ids append
display summary is replaced
diff anchor is replaced
```

禁止覆盖：

1. before snapshot。
2. before hash。
3. first transaction id。
4. original baseline archive ref。

### 8.3 after == before

如果 `after_hash == before_hash`：

1. 删除 visible change。
2. 删除 atomic target。
3. 删除该 scope 的 debug evidence。
4. 如果 ReviewRecord 无剩余 visible changes，删除 ReviewRecord。

## 9. Reject 规则

Reject 的目标不是回到 latest transaction 前，而是回到 evidence 的 `before`。

流程：

```text
load evidence by change_id / scope identity
load before snapshot
restore current scope to before
capture after restore
normalize after restore
if restored_hash == before_hash:
    remove evidence
else:
    mark reject_failed / needs_action
```

用户手动修改冲突规则：

1. 不因为 current hash != latest after hash 阻塞 Reject。
2. Reject 直接恢复到 before。
3. DebugBundle 记录 current hash、latest after hash、before hash。

## 10. Accept 规则

Accept 表示当前 scope 状态成为新的 baseline。

流程：

```text
load evidence
remove evidence
discard before snapshot for that pending review
current asset state becomes future baseline source
```

注意：

1. Accept 后再次修改同 scope，应创建新的 evidence。
2. 新 evidence 的 before 是 Accept 后状态。

## 11. DebugBundle 规则

DebugBundle 不应只是 ReviewPanel session log。

DebugBundle 应按 Diff / evidence 创建诊断单元。

每条 evidence 的 DebugBundle 内容：

1. scope identity。
2. before hash。
3. after hash。
4. before snapshot ref。
5. after snapshot ref。
6. source transaction ids。
7. latest transaction id。
8. baseline archive session id。
9. diff route result。
10. geometry / anchor result。
11. Accept / Reject result。
12. current hash at action time。
13. restore result。

如果 `after == before` 导致 evidence 删除，则不保留 Debug evidence。

## 12. 数据结构建议

### 12.1 Review atomic target 补充字段

建议增加：

```text
scope_identity
scope_kind
before_snapshot_ref
before_hash
after_snapshot_ref
after_hash
first_transaction_id
latest_transaction_id
source_transaction_ids
baseline_archive_session_id
net_effect
```

### 12.2 VisibleChange 补充字段

建议增加：

```text
scope_identity
before_summary
after_summary
before_hash
after_hash
net_effect
```

### 12.3 ReviewRecord 补充规则

ReviewRecord 保存时必须执行：

1. 按 scope identity 合并。
2. latest after 覆盖。
3. original before 保留。
4. after == before 删除。
5. 稳定排序。

## 13. 实现计划

### 阶段 1：Scope identity 与净效果框架

1. 定义 `FBlueprintHelperReviewScopeIdentity`。
2. 定义 scope snapshot 接口。
3. 定义 normalize/hash 接口。
4. ReviewStore 保存前按 scope identity 合并 evidence。
5. 实现 `after == before` 自动删除。

完成标准：

1. 同变量多次修改只显示一条 Final Changes。
2. A -> B -> A 后 Review 自动消失。
3. source transaction ids 保留但不生成重复 visible changes。

### 阶段 2：非 Graph scope resolver

优先实现：

1. Blueprint variable。
2. Component。
3. DataAsset property。
4. DataTable row。
5. Function signature。

完成标准：

1. Reject 变量默认值回到 before。
2. Reject 新增变量删除变量。
3. Accept 后再次修改能创建新的 baseline。
4. 用户手动修改后 Reject 仍回到 before。

### 阶段 3：Graph semantic normalize

实现：

1. Graph body semantic snapshot。
2. comment node ignored。
3. node position ignored。
4. reroute / knot folded when logical links unchanged。
5. BlueprintHelper stable metadata 优先。
6. NodeGuid 兜底。
7. semantic fingerprint 末级兜底。

完成标准：

1. 只移动节点不产生 Review。
2. 只加 comment 不产生 Review。
3. 只加 reroute 且逻辑连接不变不产生 Review。
4. 实际 exec/data link 变化产生 Review。
5. Graph body A -> B -> A 后 Review 自动消失。

### 阶段 4：Reject restore 改造

实现：

1. Reject 不再以 latest transaction 前状态为目标。
2. Reject 以 evidence before 为目标。
3. current hash != latest after hash 不阻塞 Reject。
4. restore 后重新 hash 验证。

完成标准：

1. A -> B -> C Reject 回 A。
2. 用户手动改成 D 后 Reject 仍回 A。
3. restore 失败时明确标记 `reject_failed`。

### 阶段 5：DebugBundle 改造

实现：

1. DebugBundle 以 evidence / Diff 为诊断主键。
2. session log 只作为 runtime observations。
3. `after == before` 删除 evidence 时不保留 Debug evidence。
4. Reject/Accept 写入 before/after/current hash。

完成标准：

1. 单个 ReviewEvent 可导出完整 before/after/current 诊断。
2. Reject 失败能看出是 restore 失败、scope resolver 失败还是 anchor 失败。
3. UI diff 残留能定位到具体 evidence。

## 14. 测试矩阵

### 14.1 变量

1. 新增变量后 Reject，变量删除。
2. 修改变量默认值后 Reject，默认值恢复。
3. A -> B -> C 后 Final Changes 只有一条，Reject 回 A。
4. A -> B -> A 后 Final Changes 自动移除。
5. Accept 后再次修改，baseline 从 Accept 后状态开始。

### 14.2 函数签名

1. 新增函数后 Reject，函数删除。
2. 修改输入参数后 Reject，签名恢复。
3. Rename 拆成删除旧函数 + 新增新函数。

### 14.3 组件

1. 新增组件后 Reject，组件删除。
2. 修改组件属性后 Reject，属性恢复。
3. 修改 parent / attach 后按简单 scope 拆分。

### 14.4 DataAsset / DataTable

1. DataAsset property A -> B -> C，Final Changes 一条，Reject 回 A。
2. DataTable row A -> B -> A，Review 自动移除。
3. Struct 类型变化按删除/新增处理。

### 14.5 Graph

1. 只移动节点，不产生 Review。
2. 只加 comment，不产生 Review。
3. 只加 reroute 且逻辑连接不变，不产生 Review。
4. 新增 call function 节点，产生 Graph Review。
5. 修改 pin default，产生 Graph Review。
6. 修改 exec/data link，产生 Graph Review。
7. A -> B -> A 后 Review 自动移除。

## 15. 与当前实现的主要差距

当前实现已有：

1. Review archive session。
2. disk snapshot refs。
3. semantic baseline snapshot refs。
4. VisibleChange / AtomicTarget。
5. `baseline_hash` / `recorded_after_hash`。
6. `rollback_data_ref`。
7. Final Changes latest wins 的部分合并逻辑。

当前缺口：

1. baseline 不是按 scope first-before 锁定的主语义。
2. after 覆盖和 before 保留没有成为统一规则。
3. `after == before` 自动删除没有成为主路径。
4. Reject 仍偏向 transaction rollback / current hash guard。
5. Graph hash 仍可能依赖不稳定节点名或 anchor。
6. DebugBundle 仍偏 session log，需要转为 evidence-first。

## 16. 不做事项

1. 不追踪中间副作用。
2. 不保留净效果为无变更的 Debug evidence。
3. 不把 rename 作为复杂一等语义。
4. 不把类型变化作为复杂字段级修改。
5. 不把 comment 节点纳入 Graph Review。
6. 不把节点位置纳入 Graph Review。
7. 不把纯视觉整理线纳入 Graph Review。

## 17. 开放问题

当前无用户侧待确认问题。

实现前仍需由代码侧确认：

1. 每类 scope 当前能否提取稳定 GUID。
2. BlueprintHelper 已写入的 stable metadata 覆盖范围。
3. Graph semantic snapshot 是否能复用现有 GraphStatement / FragmentDAG 数据结构。
4. 现有 ReviewStore JSON schema 是否需要版本提升。
5. 旧 pending ReviewRecord 是否迁移，或标记为 old-contract record。


## 18. 2026-05-15 实现进度记录

### 第 1 轮：ReviewStore evidence-first 主路径骨架

状态：代码已修改，编译通过，继续实现后续阶段。

已完成：

1. `FBlueprintHelperReviewAtomicTarget` 增加 `ScopeIdentity`、`FirstTransactionId`、`BeforeSnapshotJson`、`AfterSnapshotJson`。
2. `FBlueprintHelperReviewVisibleChange` 增加 `ScopeIdentity`、`BeforeHash`、`AfterHash`、`BeforeSnapshotJson`、`AfterSnapshotJson`。
3. ReviewRecord JSON 持久化补充 scope/before/after 字段。
4. ReviewStore 合并逻辑开始保护 first-before：后续同 scope 合并时保留既有 `BaselineHash`、`BeforeSnapshotJson`、`RollbackDataRef`、`FirstTransactionId`。
5. ReviewStore 增加 `after == before` 净效果删除路径：target 的 `BaselineHash == RecordedAfterHash` 或 change 的 `BeforeHash == AfterHash` 时移除 evidence。
6. Reject 默认路径不再因为 `current_hash != recorded_after_hash` 直接返回 `current_state_changed`，为“用户手动接管后 Reject 仍回 before”让路。

距离期望差距：

1. 真实 scope snapshot resolver 尚未实现，当前仍主要复用既有 `BaselineHash` / `RecordedAfterHash`，部分 evidence 仍可能是 synthetic hash。
2. Graph semantic normalize 已完成第一版 hash 归一化，但还没有做端到端 UI 手工验证。
3. Reject restore 仍主要走既有 rollback_data_ref / graph append rollback 路径，尚未统一为 scope-specific restore from before snapshot。
4. 旧 pending ReviewRecord 迁移策略尚未实现。

阻塞内容：

1. 暂无设计阻塞；`TemplateEditor Win64 Development` 已编译通过。下一步继续 Graph hash normalize 和 scope resolver。

### 第 2 轮：Graph hash normalize 与净效果过滤边界

状态：代码已修改，`TemplateEditor Win64 Development` 编译通过，继续实现后续阶段。

已完成：

1. `ReviewStore` 的净效果过滤改为只移除“原本有 atomic target 且全部 target 回到 baseline”的记录，避免误删没有 atomic target 的 `NeedsAction` / 旧记录。
2. `Graph` 节点 hash 纳入 node class path，避免不同语义节点只因 pin 形态相同而产生相同 hash。
3. `Graph` 节点 hash 继续忽略节点位置、注释节点；新增忽略 `K2Node_Knot` / reroute 节点。
4. `Graph` pin link 进入 hash，执行线 / 数据线变化会影响 graph body 净效果判断。
5. reroute / knot 节点在 link hash 中按透传处理，单纯整理线不应产生 Graph Review。
6. Graph node 定位优先使用 `NodeGuid`，并兼容 `Digits` 形式的 GUID 字符串。

距离期望差距：

1. 尚未通过编辑器 UI 验证“只移动节点 / 只加注释 / 只整理 reroute 不产生 Review”。
2. 已验证 `A -> B -> A` / 同语义 Graph Review 自动消失：同语义 `replace_owned_graph` 执行后 pending ReviewRecord 为 0。
3. 旧 pending ReviewRecord 的迁移策略仍未实现。
4. Reject restore 仍主要依赖既有 rollback 路径，尚未完全切到 scope-specific restore from before snapshot。

阻塞内容：

1. 暂无设计阻塞；`TemplateEditor Win64 Development` 已编译通过。

### 第 3 轮：TaskRuntime target snapshot evidence 与变量/组件 Reject restore

状态：代码已修改，`TemplateEditor Win64 Development` 编译通过，继续实现剩余未满足项。

已完成：

1. `FBlueprintHelperReviewBaselineSnapshotService` 增加 `CaptureTargetSnapshot`，可按 `FBlueprintHelperReviewAtomicTarget` 生成 target 级 snapshot JSON 与 hash。
2. target snapshot 第一版覆盖 `blueprint_variable`、`component`、`signature`、`datatable_row`、`object_property` / `data_asset_property` / `class_default_property`、`asset_factory`，未知 target 回退为整资产 snapshot。
3. TaskRuntime 执行每个 step 前先构造 review evidence 并采集 before snapshot；step 成功后再采集 after snapshot，再写入 ReviewStore。
4. 同一 scope 的 A -> B -> C 合并时，ReviewStore 会保留第一次 before snapshot / baseline hash，并替换最新 after snapshot / after hash。
5. `RejectVisibleChangeWithDefaultDispatcher` 与带 options 的 `RejectVisibleChange` 增加 snapshot restore 优先路径。
6. `blueprint_variable` snapshot restore 第一版支持：新增变量 Reject 时删除变量；变量仍存在时恢复 `DefaultValue`。
7. `component` snapshot restore 第一版支持：新增组件 Reject 时删除 SCS 节点；组件仍存在时恢复组件模板的可编辑属性文本值。

距离期望差距：

1. 变量被删除后需要按 snapshot 重建变量的路径尚未实现；当前返回 `snapshot_restore_variable_recreate_required`。
2. 组件被删除后需要按 snapshot 重建组件及父子层级的路径尚未实现；当前返回 `snapshot_restore_component_recreate_required`。
3. UMG widget 的复杂重建尚未实现；基础删除与 widget property 回滚已接入。
4. Graph body 的 before/after snapshot restore 仍依赖既有 append rollback，尚未实现整 graph semantic restore。
5. 尚未通过编辑器 UI 手工验证 Reject 后 ReviewPanel reload 不复现残留。

阻塞内容：

1. 暂无编译阻塞；下一步继续补 DataTable / ObjectProperty 的 snapshot restore 与 root lifecycle reload 一致性。

### 第 4 轮：DataTable / ObjectProperty snapshot Reject restore

状态：代码已修改，`TemplateEditor Win64 Development` 编译通过，继续实现剩余未满足项。

已完成：

1. TaskRuntime evidence 现在保留 `component` 的原始 `ComponentPath`，保留 `object_property` / `data_asset_property` / `class_default_property` 的原始 `PropertyPath`，避免 target key 安全化后丢失定位信息。
2. `datatable_row` Reject restore 第一版支持：before 不存在时删除当前行；before 存在且当前行存在时用 snapshot 文本值导回当前行。
3. `object_property` / `data_asset_property` / `class_default_property` Reject restore 第一版支持：用 before snapshot 的文本值导回 UObject 顶层属性。
4. 上述 restore 路径均接入 `ShouldUseSnapshotRestore`，不会再落入只支持 Graph rollback 的路径。

距离期望差距：

1. DataTable 行被删除后需要按 snapshot 重建行的路径尚未实现；当前返回 `snapshot_restore_datatable_row_recreate_required`。
2. ObjectProperty 只支持顶层属性名；嵌套 property path 尚未实现。
3. UMG widget / widget property 尚未实现 snapshot restore。
4. Restore 后 ReviewPanel reload 是否完全清理残留仍需编辑器 UI 验证。

阻塞内容：

1. 暂无编译阻塞；下一步继续补 root lifecycle reload 一致性与复杂重建差距。

### 第 5 轮：UMG widget / widget property snapshot Reject restore

状态：代码已修改，`TemplateEditor Win64 Development` 编译通过，继续实现剩余未满足项。

已完成：

1. TaskRuntime evidence 现在对 `umg_widget_property` 保留原始 `PropertyPath`，避免 widget 属性路径被 target key 安全化后丢失。
2. `CaptureTargetSnapshot` 增加 `umg_widget` / `umg_widget_property` target snapshot。
3. `umg_widget` Reject restore 第一版支持：before 不存在时从 WidgetTree 删除当前 widget。
4. `umg_widget_property` Reject restore 第一版支持：before 存在且当前 widget/property 存在时，用 snapshot 文本值导回属性。
5. UMG restore 路径接入 `ShouldUseSnapshotRestore`，不会再落入只支持 Graph rollback 的路径。

距离期望差距：

1. widget 被删除后需要按 snapshot 重建 widget、slot、层级、命名的路径尚未实现；当前返回 `snapshot_restore_widget_recreate_required`。
2. `umg_widget` 整 widget 属性批量恢复暂未实现；当前只对新增 widget 删除和 widget property 单属性回滚有执行路径。
3. UMG restore 后 ReviewPanel reload 是否完全清理残留仍需编辑器 UI 验证。
4. 复杂重建类场景仍需要后续独立实现：变量重建、组件重建、DataTable row 重建、Widget 重建。

阻塞内容：

1. 暂无编译阻塞；下一步处理 ReviewPanel reload/root lifecycle 的状态一致性。

## 第 6 轮实现：ReviewPanel 操作后以 ReviewStore 为权威刷新

### 目标
- Accept/Reject/AcceptAll/RejectAll 后不再只依赖面板本地状态移除条目。
- 每次操作完成后强制重新读取 ReviewStore，避免已经处理的变量、组件、DT、DA、UMG Review 在重新打开面板后再次出现。

### 已完成
- ReviewPanel 单条 Accept 成功后重置可见变更签名，并立即从 ReviewStore 刷新。
- ReviewPanel 单条 Reject 成功后重置可见变更签名，并立即从 ReviewStore 刷新。
- lifecycle root Reject 成功后同样走 ReviewStore 刷新，避免 root/local 状态分叉。
- AcceptAll / RejectAll 成功后同样走 ReviewStore 刷新。
- 编译验证通过：TemplateEditor Win64 Development，UnrealBuildTool 返回 Result: Succeeded。

### 距离期望的差距
- 尚未在编辑器 UI 中完成手动覆盖验证，需要后续通过 ReviewPanel 点击 Accept/Reject 观察记录是否稳定消失。
- DebugBundle 仍未完全改成以 diff/evidence 为主索引的导出和回放链路。
- 复杂重建场景仍未完整实现：缺失变量重建、缺失组件重建、缺失 DataTable 行重建、缺失 UMG Widget 重建仍为后续缺口。

## 第 7 轮实现：Graph Review target 稳定节点匹配

### 目标
- Graph Review 的 Accept/Reject 不再只依赖 UEdGraphNode 对象名。
- 使用与 Review diff 定位一致的稳定锚点，降低节点对象重命名、GUID 格式差异导致的 rollback 命中失败。

### 已完成
- 新增 graph node 稳定匹配逻辑：支持节点对象名、NodeGuid Digits、可解析 GUID 字符串、BlueprintHelperBlockId、BlueprintHelperTransactionId、BlueprintHelperFeatureName。
- FindReviewNodeByAnchor 与 CollectRollbackNodesForTarget 统一走稳定匹配逻辑。
- 编译验证通过：TemplateEditor Win64 Development，UnrealBuildTool 返回 Result: Succeeded。

### 距离期望的差距
- 尚未进行编辑器内实际 Reject graph node/graph block 覆盖验证。
- Graph Review 的最终目标仍依赖现有 AtomicTarget/evidence 是否完整写入；若 evidence 缺 NodeGuid 或 block metadata，仍需要上游补齐。

## 第 8 轮实现：snapshot restore 缺失目标重建分支

### 目标
- Reject 回滚到 first-before baseline 时，如果目标在当前资产中已经缺失，不再直接进入 recreate_required 阻塞。
- 对 snapshot 已能描述的目标执行最小重建，再应用 before snapshot 的值。

### 已完成
- blueprint_variable：当 before 存在但当前变量缺失时，按 snapshot 中的 pin_category、pin_sub_category、pin_sub_category_object 重建变量，并恢复 category、guid、default_value。
- component：当 before 存在但当前 SCS 组件缺失时，按 snapshot 中的 component_class 创建 SCS 节点，并恢复组件模板属性。
- datatable_row：当 before 存在但当前 DataTable 行缺失时，使用 FDataTableEditorUtils::AddRow 重建行，再导入 snapshot 中的行值。
- umg_widget：当 before 存在但当前 Widget 缺失时，按 snapshot 中的 widget_class 重建 Widget；若 WidgetTree 没有 RootWidget，则设置为 RootWidget；umg_widget_property 继续恢复具体属性。
- umg_widget：完整 Widget target 现在会恢复 snapshot 中记录的可导出属性。
- 编译验证通过：TemplateEditor Win64 Development，UnrealBuildTool 返回 Result: Succeeded。

### 距离期望的差距
- component 重建暂未恢复父子层级和 attach 关系，当前是最小 SCS 节点重建。
- datatable_row 重建已覆盖行存在性和值恢复，但未在编辑器 UI 中验证 DataTable 详情刷新行为。
- umg_widget 重建暂未恢复父子层级、slot 信息和面板子项关系，只覆盖 Widget 对象存在性与属性恢复。
- nested property path 仍未实现，只支持顶层 property restore。

## 第 9 轮实现：snapshot restore 位置上下文

### 目标
- 缺失目标重建不只恢复对象存在性，还尽量恢复它在组件树或 WidgetTree 中的位置。

### 已完成
- component target snapshot 新增 parent_component。
- component restore 重建 SCS 节点时优先挂回 parent_component，父节点不存在时退回 AddNode。
- umg_widget target snapshot 新增 parent_widget、child_index、slot_class。
- umg_widget restore 重建 Widget 时优先插回 parent_widget 的 child_index；没有父信息且 WidgetTree 没有 RootWidget 时设为 RootWidget。
- 编译验证通过：TemplateEditor Win64 Development，UnrealBuildTool 返回 Result: Succeeded。

### 距离期望的差距
- component 的 socket/attach transform 等更细粒度上下文尚未进入 snapshot。
- UMG Slot 属性尚未完整恢复，目前只恢复父 Panel 和插入序号；slot_class 已记录但未做 slot 属性回放。
- UMG 非 Panel 父节点、复杂容器约束和 RootWidget 替换策略仍需编辑器内验证。

## 第 10 轮实现：DebugBundle 结构化读取与 evidence 字段扩展

### 目标
- DebugPanel 读取 DebugBundle 时不再只是展示 raw JSON。
- focus traversal/debug log 事件应包含足够的 Review evidence 关键信息，便于排查 Accept/Reject 未生效、baseline 不稳、diff 残留等问题。

### 已完成
- DebugBundle ChangeSummary 增加 scope_identity、before_hash、after_hash、has_before_snapshot、has_after_snapshot。
- DebugBundle atomic target summary 增加 scope_identity、first_transaction_id、latest_transaction_id、baseline_hash、recorded_after_hash、has_before_snapshot、has_after_snapshot。
- DebugBundleService 新增 LoadBundleSummaryText，可解析 bundle JSON 并输出 schema、session_id、created_at、updated_at、event count、event type 分布、首尾事件时间。
- ReviewPanel 的 LoadBundle 现在会在 raw JSON 前插入结构化摘要。
- 编译验证通过：TemplateEditor Win64 Development，UnrealBuildTool 返回 Result: Succeeded。

### 距离期望的差距
- DebugBundle 还没有做按事件类型的专用 UI 渲染，只是结构化摘要 + raw JSON。
- focus traversal 还没有重试等待 row geometry 就绪，可能仍会记录 pending/hidden 类噪声。
- 尚未补 automated test 覆盖 DebugBundle 读取摘要和 focus traversal 事件字段。

## 第 11 轮实现：Debug focus traversal 几何就绪等待

### 目标
- CaptureFocus 不再每 tick 无条件推进，避免刚切换 Review 目标时 Slate row geometry 尚未 ready 导致日志缺关键上下文。

### 已完成
- focus traversal 改为两阶段：先派发 focus 并触发选择/面板重建，再等待 row geometry 就绪。
- 每个目标最多等待 5 次 tick；期间写入 wait_geometry 事件。
- geometry 成功后写入 focus_ready 事件，失败达到上限后写入 focus_timeout 事件并继续遍历。
- focus traversal event 新增 reason 字段，用于记录 geometry_ready、slate_row_geometry_not_ready、no_row_surface_required 等原因。
- 编译验证通过：TemplateEditor Win64 Development，UnrealBuildTool 返回 Result: Succeeded。

### 距离期望的差距
- 该策略降低日志丢失概率，但仍未在编辑器 UI 中验证所有 surface 的 ready/timeout 分布。
- timeout 仍代表部分 surface 的 row geometry 可能无法解析，需要后续用生成的 DebugBundle 继续定位具体 surface。

## 第 12 轮实现：details/object property 嵌套路径 baseline 与 restore

### 目标
- ObjectProperty 写入链路支持嵌套 property_path 时，Review baseline 与 Reject restore 也必须支持同样路径，避免只支持顶层属性导致无法回滚。

### 已完成
- 将 FBlueprintHelperPropertyReflectionService::ResolvePropertyPath 提升为 public static，作为通用 property path 解析入口。
- Review target snapshot 的 object_property、data_asset_property、class_default_property 改为通过 ResolvePropertyPath 捕获属性值。
- snapshot 中记录 expected_type、resolve_error_code、resolve_error_message，便于 DebugBundle 排查路径解析失败。
- Reject restore 的 object_property、data_asset_property、class_default_property 改为通过 ResolvePropertyPath 找到最终 FProperty 和 ValuePtr 后导入 before value。
- 编译验证通过：TemplateEditor Win64 Development，UnrealBuildTool 返回 Result: Succeeded。

### 距离期望的差距
- 嵌套路径解析仍沿用现有 ObjectProperty 服务能力：支持 struct/object path，不支持数组下标、Map key、Set element 这类集合路径。
- 尚未在编辑器内验证 class_default_property 的 CDO 目标是否在所有 Review evidence 中都传入正确对象路径。

## 第 13 轮实现：property path 公共解析器复用

### 目标
- 避免 Review baseline/restore 复制 ObjectProperty 的路径解析逻辑。
- 保证 CLI ObjectProperty 写入、Review snapshot、Reject restore 对 property_path 的解析语义一致。

### 已完成
- FBlueprintHelperPropertyReflectionService::ResolvePropertyPath 从 private static 提升为 public static。
- Review baseline/restore 已复用该解析器。
- 首次编译失败原因：ResolvePropertyPath 原为 private 成员。
- 修复后编译验证通过：TemplateEditor Win64 Development，UnrealBuildTool 返回 Result: Succeeded。

### 距离期望的差距
- 暂未对数组/Map/Set 路径做扩展；这些集合路径仍不在 ResolvePropertyPath 当前语义内。

## 2026-05-15 Round 14 - class default baseline CDO 对齐

状态：已完成，已编译，已通过 CLI 复测。

问题：`set_class_default_properties` 写入成功并生成 ReviewRecord，但 `visible_changes` 为空。现场记录显示 Review 源事务存在，说明 TaskRuntime evidence 已创建，但最终可见变更被净效果过滤。

原因：`class_default_property` 快照抓取时复用了普通资产属性路径，解析对象是 `UBlueprint` 资产本身；实际写入发生在蓝图生成类 CDO 上。`PrimaryActorTick.TickInterval` 在 `UBlueprint` 上解析失败，before/after 都变成 `exists:false`，随后被净效果过滤。

修复：

1. `FBlueprintHelperReviewBaselineSnapshotService` 对 `class_default_property` 解析到 `GeneratedClass`/`SkeletonGeneratedClass` 的 CDO，再用统一 `ResolvePropertyPath` 捕获 before/after。
2. `FBlueprintHelperReviewActionService` 对 `class_default_property` Reject 也回写同一个 CDO，并标记蓝图修改。
3. 快照 JSON 记录 `property_owner_class` 和 `property_owner_path`，便于 DebugBundle 判断目标对象是否正确。

验证：

1. 编译通过：`TemplateEditor Win64 Development`。
2. MCP 启动编辑器成功，Bridge 可用。
3. CLI preview 通过：`RB_EditNestedClassDefault_VisibleChangeRetest`。
4. CLI execute 通过：`task_22BB6BF74F974DE79E6D04A80EA30B63`。
5. 新 ReviewRecord 已生成 `visible_changes[0]`，目标为 `class_default_property:PrimaryActorTick_TickInterval`。
6. before snapshot value 为 `0.250000`，after snapshot value 为 `0.500000`。

距离期望差距：Reject 回滚路径已按 CDO 目标修复并通过编译，但本轮尚未找到公开 CLI Review action 命令做自动 Reject 复测；若需要 UI/内部 action 级验证，需要继续补可自动化入口或通过 ReviewPanel 手动触发后导出 DebugBundle。

## 2026-05-15 第15轮实现记录：class default baseline 回滚闭环与净效果过滤

### 已完成

1. `class_default_property` 的 baseline snapshot/rollback 已对齐到 Blueprint GeneratedClass/SkeletonGeneratedClass 的 CDO，不再错误地对 UBlueprint 资产对象反射属性。
2. `ReviewRecord.atomic_targets` 已持久化 `before_snapshot_json` / `after_snapshot_json`，避免重新加载记录后 Reject 丢失 snapshot 只能退回 hash 路径。
3. `apply_review_action` 已接入 CLI/Bridge，可通过 `blueprinthelper_apply_review_action` 对 ReviewRecord 执行 Accept/Reject。
4. Python TaskSpec 编译链已补齐 `execution_policy.review_baseline_dirty_asset_policy`，与 TS 编译链一致，`save_before_archive` 可以传到实际 TaskPlan/Bridge 路径。
5. `ReviewStore` 已在 Build/Save/Query 三个边界过滤或删除 `visible_changes=[]` 的空 ReviewRecord，满足“净效果无变更不进入 Final Changes”的规则。

### 验证结果

1. 对 `/Game/BlueprintHelperCliSmoke/ReviewBaseline_20260515_1604/BP_RB_NestedPropertyActor` 将 `PrimaryActorTick.TickInterval` 从 `0.25` 改为 `0.5` 后，Review evidence 正确记录 before=`0.250000`、after=`0.500000`。
2. 通过 `blueprinthelper_apply_review_action` Reject 该 ReviewRecord 成功，返回 `status=rejected`、`rollback_mode=archive_baseline`。
3. Reject 后再次写入 `0.25`，执行结果为 `set_class_default_properties status=no_op`，`changed_count=0`，说明属性已回滚到基线值。
4. 新 no-op task `task_2E17711D417E142327A9728CE45457D1` 查询 pending ReviewRecord，结果为 `record_count=0`。
5. C++ 编译通过；`AgentFaceService/task-core` 与 `AgentFaceService/cli` build 均通过。

### 距离期望差距

1. 本轮已消除 class default baseline 回滚与 no-op 空 ReviewRecord 的已知差距。
2. 已在后续第17轮覆盖 graph normalize 的图体级 no-op 场景；同语义 `replace_owned_graph` 执行后 pending ReviewRecord 为 0。
## 2026-05-15 第16轮实现记录：Graph normalize 布局/注释 no-review 规则

### 已完成

1. Graph block-level hash 已改为稳定语义身份：优先使用 `BlueprintHelperNodeId` / `BlueprintHelperFragmentId` / `BlueprintHelperStatementId` metadata；缺失时回退到节点 class + pin/default 的本地语义签名，不再把节点 GUID 作为 block hash 的唯一身份来源。
2. 精确 `graph_node` target hash 仍保留 NodeGuid 优先，避免单节点操作误匹配。
3. `patch_blueprint_graph` 对 `set_node_position` / `set_node_comment` 不再写 Review journal；这类改动属于布局/注释层，不进入 Final Changes，也不产生 Debug evidence。
4. `patch_blueprint_graph` 对其他 patch 类型补上 block/node anchor，避免后续 review evidence 继续落入 `missing_atomic_targets`。

### 验证结果

1. 创建 owned graph `BH_GraphNormalize_20260515_170100`，append 执行成功，task=`task_AD07D92A4C338FB6443FFCA9FC63A632`。
2. 对同一 owned block 执行 `set_node_position`，task=`task_8F44675A4D4B111ABAF30096BF06FF8C`，patch changed=true，`write_ref.journal_recorded=false`，pending ReviewRecord 查询 `record_count=0`。
3. 对同一 owned block 执行 `set_node_comment`，task=`task_9708182947EAE88BAE95139766C626C7`，patch changed=true，`write_ref.journal_recorded=false`，pending ReviewRecord 查询 `record_count=0`。
4. 相关 C++ 修改已通过 `TemplateEditor Win64 Development` 编译。

### 距离期望差距

1. “只移动节点 / 只写节点注释不产生 Review”已通过 CLI 闭环验证。
2. 尚未覆盖“新增独立 comment node 不产生 Review”，因为当前 TaskSpec first-slice 未提供创建 comment node 的普通入口。
3. 已覆盖 `A -> B -> A` / 同语义 graph body 重建后自动消失场景：replace 生成新 UE 节点但语义 hash 相同，pending ReviewRecord 查询为 0。
4. reroute/knot 整理线的 no-review 行为当前 hash 会穿透 Knot，但本轮未构造真实 reroute 操作用例。
## 2026-05-15 第17轮实现记录：Graph 同语义替换净效果过滤

状态：已完成，已编译，已通过 CLI 闭环验证。

新增内容：
1. `replace_owned_graph` 写入 Review journal 时记录 graph block 的显式 baseline hash 与 recorded-after hash，避免删除旧节点再重建新节点后 baseline 使用 synthetic hash。
2. 新增节点 anchor 继续用于 ReviewPanel/Graph 定位，但同语义替换场景中为新增节点写入 before=after 的 target hash 覆盖，避免纯定位节点制造最终变更。
3. `FBlueprintHelperAppendJournalRecord` 增加 `BaselineHashesByTargetKey` 与 `RecordedAfterHashesByTargetKey`，供 transaction journal 在生成 atomic target 时使用显式 hash。

修复内容：
1. 修复 `A -> B -> A` / 同语义 graph body 重建后仍生成 pending ReviewRecord 的问题。
2. 修复 replace 删除旧节点、创建新节点导致 Graph Review 误判为有效变更的问题；最终净效果以稳定 semantic block hash 为准。

验证结果：
1. C++ 编译通过：`TemplateEditor Win64 Development`。
2. MCP 启动编辑器成功，Bridge 可用。
3. 执行同语义 `replace_owned_graph` TaskSpec：`task_CA4A6342445065322D9793ABBB0E9D76`。
4. 按 task_run_id 查询 pending ReviewRecord，结果为 `record_count=0`。

距离期望差距：
1. `set_node_position`、`set_node_comment`、同语义 `replace_owned_graph` 已完成 CLI 闭环验证。
2. 新增独立 comment node 与 reroute/knot 的真实创建/整理用例仍缺少普通 TaskSpec first-slice 入口，当前仅完成 hash 规则与不记录布局/注释 patch 的路径验证。

阻塞内容：
1. 暂无代码/编译阻塞；剩余差距属于测试入口覆盖不足，不影响当前 Graph 同语义净效果过滤结论。
## 2026-05-15 第18轮实现记录：ReplaceBlueprintGraph Review Reject 回滚执行器

状态：已完成，已编译，已通过 CLI 闭环验证。

新增内容：
1. Graph snapshot rollback_data 增加 UE 原生节点剪贴板文本 `exported_text`，并记录 `replace_scope`、`entry_identity`、`owner_block_id`。
2. `ReplaceBlueprintGraph` journal 写入上述可回放 rollback_data，后续 Reject 不再只依赖不可重建的摘要字段。
3. Review Reject 增加 `ReplaceBlueprintGraph` 回滚执行器：删除当前 block body 节点，导入 baseline 节点文本，恢复 ownership metadata，并在 function/event/custom_event body 场景重连保留 entry 到恢复后的第一个 exec body 节点。

修复内容：
1. 修复 Graph Review Reject 命中 `rollback_executor_unimplemented:ReplaceBlueprintGraph` 的问题。
2. 修复 Replace 回滚时 graph_block target 会把保留的 entry 节点纳入删除集合，导致 `replace_rollback_entry_not_found` 的问题；现在 event/function body 回滚只删除 body 节点并保留 entry。

验证结果：
1. C++ 编译通过：`TemplateEditor Win64 Development`。
2. MCP 启动编辑器成功，Bridge 可用。
3. 创建全新 owned graph fixture：`BH_GraphRejectRollback_20260515_174513`，append task=`task_DA30007242403642CE60189718614016`。
4. 执行 replace 变更：`task_71E0E3AB46624F30D40F2EBE22C877BB`，pending ReviewRecord 数量为 1。
5. 执行 `blueprinthelper_apply_review_action` Reject：`status=completed`、`succeeded=true`、`data.status=rejected`、message=`rejected`。
6. 按 replace task_run_id 再查 pending ReviewRecord，结果为 `record_count=0`。

距离期望差距：
1. Graph block 的 CLI Reject 路径已闭环；ReviewPanel UI 点击 Reject 的视觉刷新仍需人工验证。
2. 旧的失败测试记录缺少 `exported_text`，不会被新执行器自动补全；后续新记录均写入可回放 rollback_data。

阻塞内容：
1. 暂无代码/编译阻塞；剩余为 ReviewPanel UI 视觉验收边界。