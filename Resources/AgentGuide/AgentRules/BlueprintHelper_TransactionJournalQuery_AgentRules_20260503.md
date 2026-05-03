# BlueprintHelper Agent 侧规则：Transaction Journal / Review Query 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：Transaction Journal / Review Query Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 `list_blueprint_helper_transactions` 与 `read_blueprint_helper_transaction`，以及 transaction_id / Journal / Review / rollback_data 的暴露边界。UE 字段映射见独立文档。

---

## 1. 工具簇定位

Transaction Journal / Review Query 是只读工具簇。

用途：

```text
用户明确查询事务
Debug
Review 状态查看
Rollback 定位
失败排查
审计摘要查看
```

不用于：

```text
普通写工具成功返回
Graph Write 执行闭环
Cleanup 执行闭环
Ownership 转换执行闭环
Review Accept / Reject
直接读取 rollback_data
```

---

## 2. transaction_id 暴露原则

普通写工具成功结果不返回：

```text
transaction_id
write_ref
journal_recorded
review
rollback_data
```

Agent 只有在以下场景才通过专用只读工具获取 transaction_id：

```text
1. 用户明确询问事务、Review、Journal。
2. Debug / 失败排查需要。
3. Rollback 定位需要。
4. 用户要求导出审计摘要。
```

---

# 3. list_blueprint_helper_transactions

## 3.1 工具职责

`list_blueprint_helper_transactions` 用于：

```text
按条件列出 BlueprintHelper Transaction Journal 的摘要。
```

它不用于：

```text
回滚 transaction
修改 Review 状态
导出完整 diff
返回 rollback_data
读取完整 node snapshot
```

---

## 3.2 operation

```json
"operation": "list_blueprint_helper_transactions"
```

---

## 3.3 target 规则

`target` 表示查询范围。

常见字段：

```text
asset_path
query_scope
operation_filter
status_filter
review_status_filter
limit
cursor
```

示例：

```json
"target": {
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "query_scope": "asset"
}
```

---

## 3.4 成功返回解释

示例：

```json
{
  "data": {
    "schema": "ListBlueprintHelperTransactions.v1",
    "transactions": [
      {
        "transaction_id": "tx_20260503_1704",
        "operation": "cleanup_blueprint_helper_feature",
        "status": "applied",
        "asset_count": 1,
        "review_status": "pending",
        "rollback_available": true
      }
    ],
    "page": {
      "limit": 20,
      "has_more": false
    }
  }
}
```

Agent 可读取：

```text
transaction_id
operation
status
asset_count
review_status
rollback_available
page
```

Agent 不应期待：

```text
full_diff
rollback_data
node snapshots
created_nodes
deleted_nodes
complete target list
```

---

## 3.5 空结果

如果返回：

```json
"transactions": []
```

且：

```text
ok=true
status=completed
```

Agent 应理解为空结果，不是失败。

---

# 4. read_blueprint_helper_transaction

## 4.1 工具职责

`read_blueprint_helper_transaction` 用于：

```text
读取一个明确 transaction 的审计摘要。
```

它不用于：

```text
回滚 transaction
修改 Review 状态
默认返回 rollback_data
默认返回 full diff
```

---

## 4.2 operation

```json
"operation": "read_blueprint_helper_transaction"
```

---

## 4.3 定位规则

read 必须以：

```text
transaction_id
```

定位。

示例：

```json
"target": {
  "transaction_id": "tx_20260503_1704",
  "query_scope": "transaction",
  "detail_level": "summary"
}
```

Agent 不应通过：

```text
block_ref
feature_name
graph
entry_name
```

重新推断要读取的 transaction。

---

## 4.4 detail_level

默认：

```text
summary
```

第一版可只支持 summary。

可选扩展：

```text
debug
```

debug 不应默认启用。

---

## 4.5 summary 返回解释

示例：

```json
{
  "data": {
    "schema": "ReadBlueprintHelperTransaction.v1",
    "transaction": {
      "transaction_id": "tx_20260503_1704",
      "operation": "cleanup_blueprint_helper_feature",
      "status": "applied",
      "review_status": "pending",
      "rollback_available": true,
      "targets": [
        {
          "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
          "graph": "EG_PhysicsDoor"
        }
      ],
      "summary": {
        "affected_assets": 1,
        "affected_owned_blocks": 3
      }
    }
  }
}
```

Agent 可读取：

```text
transaction.transaction_id
transaction.operation
transaction.status
transaction.review_status
transaction.rollback_available
transaction.targets
transaction.summary
```

---

## 4.6 summary 字段规则

`summary` 是 operation-specific 的轻量摘要。

可能包括：

```text
affected_assets
affected_owned_blocks
converted_count
cleaned_count
created_asset_count
created_component_count
```

不是每个 transaction 都有全部字段。

Agent 不应假设所有字段都存在。

---

## 4.7 不返回 rollback_data

read 默认不返回：

```text
rollback_data
```

原因：

```text
1. rollback_data 是 UE 内部恢复实现细节。
2. 数据可能很大。
3. Rollback 工具应自己读取 Journal 内部 rollback_data。
4. Agent 正常不需要读取恢复快照。
```

如果需要导出调试包，应由单独工具处理，例如：

```text
export_blueprint_helper_transaction_debug_bundle
```

---

## 4.8 不返回 full diff / snapshots

read 默认不返回：

```text
full diff
before snapshot
after snapshot
node snapshot
pin snapshot
complete diagnostics
```

这些属于 UE Review UI、debug bundle 或内部 Journal 数据。

---

# 5. Review 状态规则

只读查询工具可以返回：

```text
review_status
```

但第一版不做 Agent 写工具：

```text
accept_review_transaction
reject_review_transaction
```

原因：

```text
1. Review Accept / Reject 是用户审查行为。
2. 应在 UE Review UI 中完成。
3. Agent 不应默认代表用户接受或拒绝审计结果。
```

Agent 不得在普通任务中要求或等待 Review Accept / Reject 作为完成条件。

---

# 6. Rollback 与 read_transaction 的关系

Rollback 工具内部读取：

```text
Journal
rollback_data
```

Agent 不需要先通过 `read_blueprint_helper_transaction` 获取 rollback_data。

正确流程：

```text
1. 用户要求回滚某个 cleanup transaction。
2. Agent 使用 transaction_id 调用 RollbackCleanupTransaction dry_run。
3. Rollback 工具内部读取 rollback_data 并判断能否恢复。
```

不正确流程：

```text
1. Agent 先通过 read_transaction 获取 rollback_data。
2. 再把 rollback_data 传回 Rollback 工具。
```

---

# 7. 查询失败规则

如果 query 工具失败，Agent 根据 error 处理。

示例：

```json
{
  "ok": false,
  "status": "failed",
  "error": {
    "code": "transaction_not_found",
    "stage": "resolve_transaction",
    "message": "The requested BlueprintHelper transaction was not found.",
    "retryable": false
  }
}
```

常见错误：

```text
journal_store_unavailable
transaction_not_found
invalid_query_scope
cursor_invalid
```

---

# 8. Agent 禁止行为

Agent 不得：

```text
1. 期待普通写工具成功返回 transaction_id。
2. 把 read_transaction 当作 rollback_data 导出工具。
3. 用 block_ref / feature_name 推断 read_transaction 目标。
4. 代表用户执行 Review Accept / Reject。
5. 在普通任务中等待 Review 状态。
6. 把 transactions=[] 当成失败。
7. 期待 list 返回 full diff / node snapshots。
8. 期待 read 默认返回 full diff / rollback_data。
```

---

# 9. 最终报告规则

当用户查询事务时，Agent 可报告：

```text
1. 找到多少事务。
2. 每个事务的 operation / status / review_status / rollback_available。
3. 读取单个事务时报告 targets 和 summary。
```

不默认报告：

```text
rollback_data
full diff
node snapshot
Journal 文件路径
```

---

# 10. 验收标准

```text
1. Agent 能用 list_blueprint_helper_transactions 查询事务摘要。
2. Agent 能处理 transactions=[] 空结果。
3. Agent 能用 read_blueprint_helper_transaction 读取单个事务摘要。
4. Agent 必须用 transaction_id 定位 read。
5. Agent 不期待 rollback_data。
6. Agent 不期待 full diff。
7. Agent 不通过 query 工具修改 Review 状态。
8. Agent 不要求普通写工具返回 transaction_id。
