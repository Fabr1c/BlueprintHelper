# BlueprintHelper Agent 侧规则：MergeBlueprintGraph 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：MergeBlueprintGraph Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 MergeBlueprintGraph，包括 merge_scope、insert_strategy、merged_ref、dry_run、失败诊断和 rollback 处理。UE 字段映射见独立文档。

---

## 1. 工具职责

`MergeBlueprintGraph` 只负责：

```text
把新逻辑或已有逻辑引用接入一个明确的已有执行流。
```

Agent 应把 Merge 理解为：

```text
修改已有执行流连接关系。
```

因此 Merge 是高风险 Graph Write 工具，默认必须 dry_run。

---

## 2. 与其他 Graph Write 工具的边界

```text
Append：新增独立 owned block，不接入已有执行链。
Replace：替换明确目标的完整实现。
Patch：精确修改一个节点 / Pin / Link / 默认值。
Merge：改变已有执行流连接关系。
```

Agent 不得用 Append 代替 Merge 接入已有执行链。

---

## 3. target 规则

Merge 顶层 `target` 只表示执行路由范围。

字段：

```text
asset_path
graph
merge_scope
insert_strategy
```

Agent 不应期待：

```text
target_type
target_kind
```

---

## 4. merge_scope

允许：

```text
owned_block_call
custom_event_call
function_call
inline_nodes
event_entry_logic
```

第一版优先使用：

```text
owned_block_call
custom_event_call
function_call
```

`inline_nodes` 风险较高，除非工具能力明确支持且用户目标明确，否则不优先使用。

---

## 5. insert_strategy

必须显式指定：

```text
append_after
insert_between
branch_fork
```

Agent 不得让工具猜策略。

---

## 6. append_after

语义：

```text
把新逻辑接到 anchor Exec Pin 后方。
```

要求：

```text
anchor Exec Pin 当前没有后继。
```

如果 anchor 已有后继：

```text
dry_run blocked
blocked_by = anchor_exec_pin_already_connected
```

Agent 不得自动把 append_after 改成 insert_between 或 branch_fork。

---

## 7. insert_between

语义：

```text
断开 anchor Exec Pin 的原后继。
插入新逻辑。
新逻辑执行完后重接原后继。
```

这是高风险执行流修改，必须 dry_run。

---

## 8. branch_fork

语义：

```text
插入 Sequence 或等价分发节点，把原后继和新逻辑分到不同分支。
```

必须显式提供：

```text
sequence_order
```

示例：

```json
"sequence_order": [
  "original_successor",
  "inserted_logic"
]
```

Agent 不得让工具默认决定原逻辑先执行还是新逻辑先执行。

---

# 9. 成功返回极简规则

Merge 成功返回不是 execution diff，也不是 Journal 摘要。

Agent 只读取：

```text
data.merge_result.merged_ref
data.write_ref.transaction_id
validation
```

示例：

```json
{
  "data": {
    "schema": "MergeBlueprintGraph.v1",
    "merge_result": {
      "merged_ref": {
        "graph_id": "EventGraph",
        "anchor_ref": "BeginPlay0.Then",
        "inserted_ref": "EG_PhysicsDoor_TogglePhysicsDoor0"
      }
    },
    "write_ref": {
      "transaction_id": "tx_20260503_1501",
      "journal_recorded": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

Agent 不应期待：

```text
disconnected_links
created_links
execution_order_changed
affected_user_nodes
old_successor
new_successor
full_diff
review
safety
diagnostics
next
```

这些属于 Journal / Review / verbose/debug。

---

## 10. merged_ref 规则

`merged_ref` 表示本次正式接入命中的执行流引用。

字段：

```text
graph_id
anchor_ref
inserted_ref
sequence_ref
```

解释：

| 字段 | Agent 解释 |
|---|---|
| `graph_id` | 执行流所在图表 ID。 |
| `anchor_ref` | 接入锚点，例如 `BeginPlay0.Then`。 |
| `inserted_ref` | 被插入逻辑引用，例如 block_id / function ref / custom event ref。 |
| `sequence_ref` | branch_fork 插入 Sequence 时返回。 |

---

## 11. dry_run 规则

Merge 默认必须 dry_run。

dry_run passed：

```json
{
  "dry_run": {
    "result": "passed",
    "can_execute": true
  }
}
```

dry_run blocked：

```json
{
  "dry_run": {
    "result": "blocked",
    "can_execute": false,
    "blocked_by": [
      "anchor_exec_pin_already_connected"
    ],
    "conflicts": [
      {
        "code": "anchor_exec_pin_already_connected",
        "message": "append_after cannot be used because the anchor Exec Pin already has a successor."
      }
    ],
    "errors": []
  }
}
```

Agent 规则：

```text
1. result=passed + can_execute=true：可进入正式写入。
2. result=blocked + can_execute=false：不得正式写入。
3. ok=true/status=dry_run 只表示 dry_run 工具运行成功。
4. dry_run 工具自身失败才是 ok=false/status=failed/error。
```

dry_run 不返回 merge_plan / would_xxx。完整 execution diff 进入 Journal / Review / verbose/debug。

---

## 12. 正式失败规则

正式失败不返回：

```text
merge_result
write_ref
ownership
review
safety
diagnostics
next
```

但必须读取：

```text
error.code
error.stage
error.message
error.retryable
error.rollback_result
error.failed_item
error.conflicts
```

最小错误字段：

```text
code
stage
message
retryable
rollback_result
```

---

## 13. rollback_result 规则

| rollback_result | Agent 解释 |
|---|---|
| `not_needed` | preflight / resolve_anchor 阶段失败，未修改资产。 |
| `rolled_back` | 写入中失败但已回滚，通常 modified=false。 |
| `blocked` | 回滚被阻断，可能残留修改，必须 stop_and_report。 |
| `failed` | 回滚失败，可能残留修改，必须 stop_and_report。 |

如果 rollback blocked / failed：

```text
Agent 不得继续 compile / save / patch / merge / replace。
```

---

## 14. validation 规则

Merge 成功通常返回：

```text
validation.should_compile=true
validation.should_save=true
```

Agent 应根据 validation 继续 compile/save 闭环。

---

## 15. Agent 禁止行为

Agent 不得：

```text
1. 用 Append 接入已有执行流。
2. 在未指定 insert_strategy 时调用 Merge。
3. 在 branch_fork 未指定 sequence_order 时调用正式写入。
4. 自动把 append_after 改成 insert_between 或 branch_fork。
5. 期待 Merge 成功返回 execution diff。
6. 在 rollback blocked / failed 后继续写入。
```

---

## 16. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 哪个资产 / 图表的执行流被接入。
2. 接入了哪个逻辑引用。
3. 使用了哪种 insert_strategy。
4. 编译 / 保存结果。
5. 异常或未完成项。
```

不默认报告：

```text
transaction_id
review_status
journal_path
rollback_data
node_guid
pin_guid
created_links / disconnected_links 明细
```

---

## 17. 验收标准

```text
1. Agent 能识别 Merge 只负责接入已有执行流。
2. Agent 不用 Append 替代 Merge。
3. Agent 能区分 append_after / insert_between / branch_fork。
4. Agent 知道 append_after 不能用于已有后继 Exec Pin。
5. Agent 知道 branch_fork 必须显式 sequence_order。
6. Agent 能解析 merge_result.merged_ref。
7. Agent 不期待 execution diff。
8. Agent 能处理 dry_run passed / blocked。
9. Agent 能处理 error.rollback_result。
