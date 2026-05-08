# BlueprintHelper Review 系统完整边界 v1

日期：2026-05-08  
状态：本轮讨论确认版  
适用范围：BlueprintHelper UE 插件侧 Review / Transaction / Journal / rollback / ownership conversion / DebugExport 设计  
来源：基于 `BlueprintHelper_Review_Transaction_Model_Discussion_Pack_20260507.md` 与本轮 Review v1 决策同步整理。

---

## 0. 总结

Review 系统固定为 **用户侧持久审查系统**。

它不属于普通 Agent 执行闭环，不进入 AgentGuide 的 Accept / Reject 默认流程，也不让 Agent 操作 ReviewPanel。

固定分层：

```text
Agent-visible:
  TaskRunJournal.v1
  task_run_id
  step status
  preview / execute errors
  recovery summary

User Review-visible:
  ReviewRecord
  ReviewVisibleChange
  ReviewAtomicTarget
  archive baseline
  Accept / Reject state

Internal / developer diagnostics:
  Transaction Journal details
  rollback_data
  diff snapshots
  DebugExport bundle
```

核心模型：

```text
ArchiveSession
-> ReviewRecord
-> ReviewVisibleChange
-> ReviewAtomicTarget
-> ReviewAnchor / OwnedAnchor
```

---

## 1. Review 不属于 Agent 默认工作流

正常 Agent 仍走：

```text
TaskSpec -> preview -> execute -> get task result
```

Agent 最终报告默认只输出：

```text
任务是否完成
修改资产摘要
主要新增 / 修改逻辑
编译 / 验证结果
保存结果
异常或未完成项
```

Agent 默认不输出：

```text
transaction_id
block_id
ReviewRecord id
Review 状态
rollback_data
Journal 路径
完整 diff
DebugExport payload
Accept / Reject 操作说明
```

ReviewPanel 是用户侧工具，用户在 UE 插件 UI 内审查改动。

---

## 2. ReviewRecord identity

采用：

```text
ReviewRecord = archive_session_id + asset_path
```

不采用：

```text
ReviewRecord = task_run_id
ReviewRecord = transaction_id
```

原因：一次 `task_run_id` 可能跨多个资产写入。Review 若绑定 task_run，会把多个资产强行包成一个审查对象，不利于用户部分 Accept / Reject。

固定结构：

```text
task_run_id
-> archive_session_id
   -> ReviewRecord(asset_path=/Game/A)
   -> ReviewRecord(asset_path=/Game/B)
   -> ReviewRecord(asset_path=/Game/C)
```

`task_run_id` 只作为来源追踪字段，不作为 Review 操作边界。

---

## 3. Review UI grouping

采用：

```text
Asset-first Review tree
```

默认 UI 树：

```text
Review
  Asset: /Game/Door/BP_PhysicsDoor
    Graph changes
    Component changes
    Class Settings changes

  Asset: /Game/Interaction/BPI_Interactable
    Asset changes
    Interface signature changes
```

`task_run_id` 只作为：

```text
来源标记
筛选维度
次级分组
trace linkage
```

不作为强制 Accept / Reject 边界。

---

## 4. TransactionJournalQuery scope

采用双层查询模型：

```text
User / Review UI:
  ReviewRecordQuery

Internal / developer diagnostics:
  TransactionJournalQuery
```

### 4.1 ReviewRecordQuery

Review UI 的主查询面。

返回：

```text
ReviewRecord
ReviewVisibleChange
ReviewAtomicTarget summary
source_transaction_summary
ReviewAction history
current review status
```

### 4.2 TransactionJournalQuery

保留为内部 / 开发者诊断接口。

用途：

```text
DebugExport
rollback failure diagnosis
transaction replay investigation
source transaction inspection
developer-only diagnostics
```

不作为用户侧 Review 的默认浏览模型。

---

## 5. ArchiveSession persistence

采用：

```text
每个 task_run 创建一个 archive_session_id
```

ReviewRecord 仍按资产拆分。

### 5.1 创建时机

```text
UE TaskRuntime execute 开始前创建 archive_session_id。
在首个真实写操作前，对本次 allowed_assets 捕获 baseline。
```

### 5.2 建议持久化路径

```text
Saved/BlueprintHelper/Review/ArchiveSessions/<archive_session_id>.json
Saved/BlueprintHelper/Review/Records/<review_record_id>.json
Saved/BlueprintHelper/Review/Snapshots/<archive_session_id>/<asset_hash>/
```

### 5.3 Crash recovery

UE 插件启动时扫描 active archive sessions：

```text
如果 session 有 applied transactions 但 ReviewRecord 未完成聚合：重建 ReviewRecord。
如果 baseline 缺失：对应 ReviewRecord 标记 needs_action。
```

---

## 6. Reject semantics

已固定：

```text
允许部分 Reject。
Reject 不做依赖判断。
Reject 只处理用户选中的目标。
Reject 不因同 task_run_id / transaction_id / asset group 自动级联。
断链、缺失引用、编译错误交给 UE 原生报错 / Blueprint compile / asset diagnostics 暴露。
```

### 6.1 允许部分 Reject

允许范围：

```text
1. 同一个 task_run_id 下，只 Reject 某些资产的改动。
2. 同一个资产下，只 Reject 某些 visible changes。
3. 同一个 visible change 下，展开后只 Reject 部分 atomic targets。
4. Reject 不自动级联到其他资产、其他 visible changes、其他 transactions。
```

### 6.2 不做依赖判断

Reject 不检查：

```text
external_dependents
referenced_by
cross transaction dependency
后续 transaction dependency
其他资产是否引用该资产
删除变量后哪些节点断线
删除接口后哪些调用失效
删除 InputAction 后哪些蓝图事件失效
Reject 后项目是否仍能编译通过
```

这些交给：

```text
Unreal Editor 原生编译错误
资产删除 / 保存 / 加载报错
Blueprint compile result
Review UI validation_result
后续用户或 Agent 修复流程
```

### 6.3 Reject 做机械检查

Reject 只检查：

```text
anchor 是否能定位
rollback_data 是否存在
archive baseline 是否可读
当前目标是否仍匹配 recorded_after_hash
UE API 是否能执行恢复 / 删除 / 回滚
```

### 6.4 TOCTOU 检测

采用严格检测：

```text
current_hash 必须等于 recorded_after_hash。
```

如果用户在 Review 期间手动改过同一目标：

```text
不覆盖用户新修改。
标记 needs_action 或 reject_failed。
reason = current_state_changed。
```

### 6.5 RejectAll

RejectAll 对当前过滤列表中的 pending targets 逐项执行。

规则：

```text
不做依赖排序。
不因某个 target 被引用而跳过。
不因某个 target 造成编译错误而停止。
只有 native rollback 执行失败的 target 标记 failed / needs_action。
```

---

## 7. Accept ownership policy

采用：

```text
Accept and convert owner = review_action + ownership conversion transaction
```

默认：

```text
Accept and keep managed
```

普通 Accept 不改变 ownership。

### 7.1 Setting Profile policy

需要加入 Setting Profile policy：

```text
review_accept_default_ownership = keep_managed
review_allow_convert_owner_on_accept = true | false
review_convert_owner_requires_explicit_user_action = true
review_convert_owner_generates_transaction = true
review_convert_owner_block_allowed_directions = [bh_to_user, user_to_bh]
```

建议默认：

```text
review_accept_default_ownership = keep_managed
review_allow_convert_owner_on_accept = true
review_convert_owner_requires_explicit_user_action = true
review_convert_owner_generates_transaction = true
review_convert_owner_block_allowed_directions = [bh_to_user, user_to_bh]
```

### 7.2 Convert on Accept 执行顺序

用户选择 convert 时：

```text
1. 写入 review_action。
2. 执行 ConvertOwnerBlock。
3. 生成 ownership conversion transaction。
4. 写入 Transaction Journal。
5. ReviewRecord 记录 action 与 source transaction。
```

---

## 8. Ownership conversion scope

v1 只支持 block 级 ownership 转换。

工具固定命名：

```text
ConvertOwnerBlock
```

废弃旧命名：

```text
ConvertBlueprintHelperBlockToUserOwned
```

### 8.1 支持双向转换

```text
bh_to_user
user_to_bh
```

### 8.2 bh_to_user

语义：

```text
BlueprintHelper-owned block -> user-owned block
```

行为：

```text
清除或转换 BlueprintHelper ownership metadata。
清理或转换 NodeComment。
该 block 后续不再被 Cleanup / replace_owned 默认管理。
生成 transaction。
写入 Journal / Review history。
```

### 8.3 user_to_bh

语义：

```text
user-owned block -> BlueprintHelper-owned block
```

行为：

```text
将用户明确指定的一组节点 / 连线接管为 BH-owned block。
写入 block_id。
写入 ownership metadata / NodeComment。
生成 transaction。
写入 Journal / Review history。
```

`user_to_bh` 必须满足：

```text
用户显式动作或明确授权。
Setting Profile policy 允许。
目标 block 范围明确。
有 entry anchor。
有 included node anchors。
有 included link anchors。
有 desired block name / block_ref。
```

禁止：

```text
单个 user node anchor 自动视为 block。
模糊选择用户节点后自动接管。
Agent 自动 user_to_bh。
graph 级 conversion。
asset 级 conversion。
```

---

## 9. Compaction policy

采用状态分层 compact。

### 9.1 状态规则

```text
pending:
  保留完整 rollback_data、review_snapshot、source transactions。

accepted:
  可在策略允许后 compact。

rejected:
  rollback 成功后可 compact。

needs_action / reject_failed:
  必须保留完整 rollback_data、error、target state、debug linkage。

compacted:
  只保留 source_transaction_summary。
  不承诺自动 rollback。
  不承诺完整审计 diff。
```

### 9.2 compacted 最终字段

`compacted` 状态下，ReviewRecord 的持久 payload 只保留：

```json
{
  "storage_status": "compacted",
  "source_transaction_summary": {
    "transaction_count": 3,
    "task_run_ids": ["task_..."],
    "operation_kinds": ["append_blueprint_graph", "convert_owner_block"],
    "asset_paths": ["/Game/..."],
    "created_at_first": "...",
    "created_at_last": "...",
    "final_review_status": "accepted"
  }
}
```

固定规则：

```text
compacted = 只保留 source_transaction_summary。
```

---

## 10. DebugExport linkage

采用：

```text
ReviewRecord 只保存 debug_export_ref。
DebugExport 独立作为 developer diagnostics system。
```

ReviewRecord 不内联 DebugExport payload。

DebugExport 可包含：

```text
ReviewRecord summary
visible_change summary
atomic_target anchors
snapshots
rollback fragment
native error context
source_transaction_summary
```

但 DebugExport：

```text
不进入 Agent 默认 TaskRunJournal。
不作为 ReviewRecord 主存储模型。
不作为 Agent bulk-read path。
```

---

## 11. Non-BlueprintHelper-owned anchors

### 11.1 定义

```text
Non-BH-owned anchor 是 Review 定位锚点。
它不是 ownership。
它不是 block_id。
它不是 Agent 默认写入 handle。
```

它服务于：

```text
ReviewPanel 展示用户内容被怎样影响。
ReviewVisibleChange 折叠。
ReviewAtomicTarget 定位。
Reject 时恢复 / 删除 / 回滚用户选择的目标。
DebugExport 定位问题上下文。
```

它不服务于：

```text
Agent 默认写入定位。
Cleanup 默认删除。
Replace / Patch 默认接管用户节点。
ownership 判断。
自动把用户内容变成 BlueprintHelper-owned。
```

### 11.2 与 BH-owned anchor 的关系

```text
BH-owned anchor:
  BlueprintHelper 自己创建 / 接管的内容的稳定管理锚点。

Non-BH-owned anchor:
  用户内容 / legacy 内容 / 非 BlueprintHelper 管理内容的 Review 定位锚点。
```

二者不完全对称：

| 项 | BH-owned anchor | Non-BH-owned anchor |
|---|---|---|
| 表示 ownership | 是 | 否 |
| 可被 Cleanup 默认管理 | 是 | 否 |
| 可作为 owned Patch / Replace 目标 | 是 | 否 |
| 可用于 Review 展示 | 是 | 是 |
| 可用于 Reject 回滚定位 | 是 | 是 |
| 写入节点 Metadata / NodeComment | 通常是 | 否 |
| Agent 默认可依赖 | owned 工具边界内可用 | 禁止 |

固定一句话：

```text
Non-BH-owned anchor 是 BH-owned anchor 在 Review 定位层面的对应物，
但不是 ownership 层面的对应物。
```

### 11.3 存储边界

Non-BH-owned anchor 只保存在：

```text
Transaction Journal
ReviewRecord
ReviewVisibleChange / ReviewAtomicTarget
ReviewSnapshot
rollback_data
DebugExport
```

禁止默认写入：

```text
用户节点 metadata
用户节点 NodeComment
普通 Agent-facing TaskRunJournal
普通 ToolResultBase 成功结果
GraphWrite 成功摘要
AgentGuide 默认写入流程
```

原因：用户内容不应因为被 Review 观察过，就被污染 metadata。

### 11.4 使用边界

Non-BH-owned anchor 只供：

```text
Review 展示
visible change collapse
Reject rollback
DebugExport
ReviewRecordQuery
```

不供：

```text
Agent 默认 Patch
Agent 默认 Merge
Agent 默认 Replace
Cleanup 默认删除
ownership 接管
```

### 11.5 覆盖范围

覆盖所有 ReviewAtomicTarget 类型：

```text
graph_node
graph_link
graph_pin
component
component_property
class_setting
class_default_property
implemented_interface
asset
function_signature
event_signature
variable
local_variable
```

### 11.6 生成时机

Non-BH-owned anchor 由 producer 写工具在写入 transaction 时生成，并随 transaction 一起写入 Transaction Journal / Review Store。

规则：

```text
producer 写工具负责写入 affected atomic targets。
每个 atomic target 必须携带 ReviewAnchor。
ReviewStore 只负责折叠 visible changes，不临时猜测 anchor。
ReviewPanel 只负责展示，不生成 anchor。
```

### 11.7 Reject 规则

Reject 使用 anchor 定位目标并执行 rollback。

Reject 做：

```text
anchor 定位
rollback_data 可用性检查
TOCTOU / 当前状态冲突检查
UE 原生执行结果检查
```

Reject 不做：

```text
external_dependents 检查
referenced_by 检查
跨 transaction 依赖检查
编译安全依赖检查
```

### 11.8 严格 TOCTOU

Reject 前必须检查：

```text
anchor identity 仍可定位。
current_hash == recorded_after_hash。
```

如果当前目标已被用户或其他流程改过：

```text
不覆盖。
标记 reject_failed 或 needs_action。
reason = current_state_changed。
```

### 11.9 target_key

采用：

```text
target_key = hash(normalized_anchor_identity)
```

ReviewStore 按 target_key 折叠 visible changes。

同一个目标被多个 transaction 修改时：

```text
source_transaction_ids = [tx_1, tx_2, tx_3]
latest_transaction_id = tx_3
before = archive baseline
after = current final state
```

`transaction_id` 不参与 target_key 主身份。  
`display_name` 不参与 target_key 主身份。

### 11.10 snapshot

anchor snapshot 只用于展示，不能作为唯一定位依据。

可保存：

```text
node_title_snapshot
pin_name_snapshot
component_class_snapshot
property_owner_class_snapshot
position_snapshot
asset_type_snapshot
```

定位优先：

```text
GUID / property_path / asset_path 优先。
display name 只用于展示。
display name 不作为唯一定位依据。
```

如果 GUID / property_path / asset_path 定位失败：

```text
ReviewPanel 仍可展示 snapshot。
Reject 自动回滚失败或进入 needs_action。
不做 display name / position / title 模糊恢复。
```

### 11.11 与 ConvertOwnerBlock 的关系

Non-BH-owned anchor 可以作为 `ConvertOwnerBlock(user_to_bh)` 的候选材料，但不能自动触发转换。

`ConvertOwnerBlock(user_to_bh)` 必须需要：

```text
用户显式动作。
明确 block 范围。
entry anchor。
included node anchors。
included link anchors。
desired block name / block_ref。
Setting Profile policy 允许。
```

禁止：

```text
看到 Non-BH-owned anchor 后自动接管。
把单个 user node anchor 当成 block。
让 Agent 自动 user_to_bh。
```

---

## 12. Review status model

### 12.1 ReviewAtomicTarget status

建议状态：

```text
pending
accepted
rejected
needs_action
reject_failed
```

说明：

```text
needs_action:
  未执行或不能安全执行机械回滚，例如 current_state_changed、rollback_data_missing、target_not_found。

reject_failed:
  已尝试执行，但 UE API / 原生操作失败。
```

不使用依赖型：

```text
reject_blocked_by_external_dependents
reject_blocked_by_referenced_by
reject_blocked_by_transaction_dependency
```

### 12.2 ReviewVisibleChange status

由内部 atomic targets 派生：

```text
pending
accepted
rejected
mixed
needs_action
```

### 12.3 ReviewRecord status

同样派生：

```text
pending_review
accepted
rejected
mixed
needs_action
```

---

## 13. 建议 ReviewRecord schema 草案

```json
{
  "schema": "BlueprintHelper.ReviewRecord.v1",
  "review_record_id": "review_...",
  "archive_session_id": "archive_...",
  "asset_path": "/Game/...",
  "source_task_run_ids": ["task_..."],
  "status": "pending_review",
  "storage_status": "active",
  "visible_changes": [
    {
      "change_id": "change_...",
      "surface": "graph",
      "visual_group_key": "graph:EventGraph:block:...",
      "status": "pending",
      "atomic_targets": [
        {
          "target_key": "hash(normalized_anchor_identity)",
          "target_kind": "graph_node",
          "ownership": "blueprinthelper_owned | user_owned | unknown",
          "latest_transaction_id": "tx_...",
          "source_transaction_ids": ["tx_1", "tx_2"],
          "anchor": {},
          "recorded_after_hash": "...",
          "baseline_hash": "...",
          "status": "pending"
        }
      ]
    }
  ],
  "review_actions": [
    {
      "action": "accept | reject | convert_owner_block",
      "target_keys": ["..."],
      "ownership_policy": "keep_managed | convert_owner_block",
      "created_at": "...",
      "source_transaction_id": "tx_..."
    }
  ],
  "diagnostics": {
    "debug_export_refs": []
  },
  "source_transaction_summary": null
}
```

`compacted` 后：

```json
{
  "schema": "BlueprintHelper.ReviewRecord.v1",
  "review_record_id": "review_...",
  "storage_status": "compacted",
  "source_transaction_summary": {
    "transaction_count": 3,
    "task_run_ids": ["task_..."],
    "operation_kinds": ["append_blueprint_graph", "convert_owner_block"],
    "asset_paths": ["/Game/..."],
    "created_at_first": "...",
    "created_at_last": "...",
    "final_review_status": "accepted"
  }
}
```

---

## 14. ReviewPanel UI 边界

ReviewPanel 是只读审查面板，不是第二个 Blueprint Editor。

允许：

```text
展示最终 visible changes。
展示 graph-space diff frame。
展示新增绿色高亮。
展示删除红色 snapshot。
Accept / Reject / AcceptAll / RejectAll。
Open in Blueprint Editor 作为用户主动辅助动作。
```

禁止：

```text
直接编辑真实蓝图节点。
修改新增节点。
编辑删除 snapshot。
修改组件树 / Class Settings。
作为 Graph Write 输入面板。
让 Agent 操作 Accept / Reject。
```

---

## 15. v1 明确不做

```text
1. Agent-facing ReviewPanel flow。
2. AgentGuide Accept / Reject 操作说明。
3. raw transaction-first Review UI。
4. DebugExport 作为 Review 存储。
5. large_payload_ref 作为 Agent 当前读取方向。
6. 跨 transaction 自动级联 rollback。
7. Reject 依赖分析。
8. Agent 默认使用 Non-BH-owned anchor 写入。
9. graph 级 ownership conversion。
10. asset 级 ownership conversion。
11. 自动 user-owned -> BH-owned 接管。
12. ReviewPanel 内编辑真实资产。
```

---

## 16. 最终决策表

| # | 决策项 | 结论 |
|---:|---|---|
| 1 | ReviewRecord identity | `archive_session_id + asset_path` |
| 2 | TaskRunJournal grouping | Asset-first tree；task_run_id 只做来源筛选 / 次级分组 |
| 3 | TransactionJournalQuery scope | ReviewRecordQuery 为用户主查询；TransactionJournalQuery 为 developer diagnostics |
| 4 | ArchiveSession persistence | 每个 task_run 创建一个 archive_session_id |
| 5 | Reject semantics | 允许部分 Reject；Reject 不做依赖判断 |
| 6 | Accept ownership policy | `review_action + ownership conversion transaction`，纳入 Setting Profile policy |
| 7 | Ownership conversion scope | `ConvertOwnerBlock`，block 级双向转换 |
| 8 | Compaction policy | 状态分层；compacted 只保留 `source_transaction_summary` |
| 9 | DebugExport linkage | ReviewRecord 只保存 `debug_export_ref` |
| 10 | Non-BH-owned anchors | 仅 Review 定位，不是 ownership，不是 Agent 写入 handle |

