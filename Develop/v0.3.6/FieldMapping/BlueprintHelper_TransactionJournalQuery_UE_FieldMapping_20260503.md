# BlueprintHelper Transaction Journal / Review Query UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：Transaction Journal / Review Query 字段确认稿  
本文边界：确认 `list_blueprint_helper_transactions` 与 `read_blueprint_helper_transaction` 两个只读查询工具的 Agent-facing 返回字段、UE 侧结构体映射、空结果、失败结果，以及 rollback_data / Review 写入边界。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

Transaction Journal / Review Query 采用以下字段口径：

```text
1. 普通写工具成功结果不返回 transaction_id / write_ref。
2. transaction_id / Journal / Review / rollback_data 属于 UE 侧内部审计系统。
3. 只有用户明确查询、Debug、Review、Rollback 定位时，才通过专用只读工具暴露必要摘要。
4. 增加只读工具 list_blueprint_helper_transactions。
5. list 使用 operation=list_blueprint_helper_transactions。
6. list 顶层 target 表达 query_scope / asset_path / filter / limit / cursor。
7. list 成功返回 transactions[] 摘要。
8. transactions[] 可返回 transaction_id，因为该工具就是事务查询。
9. transactions[] 默认只返回 operation / status / asset_count / review_status / rollback_available。
10. list 不返回 full diff / rollback_data / node snapshots。
11. 空结果 status=completed / transactions=[]，不是失败。
12. 增加只读工具 read_blueprint_helper_transaction。
13. read 使用 operation=read_blueprint_helper_transaction。
14. read 必须以 transaction_id 定位。
15. read 默认 detail_level=summary。
16. read summary 返回 transaction / targets / summary。
17. read 不默认返回 rollback_data / full diff / node snapshots。
18. Review Accept / Reject 第一版不做 Agent 写工具，只通过 UE Review UI。
19. Rollback 工具内部读取 rollback_data，不需要 Agent 通过 read_transaction 获取。
```

---

# 1. 工具簇定位

Transaction Journal / Review Query 工具簇是只读工具簇。

它服务于：

```text
用户明确查询事务
Debug
Review 状态查看
Rollback 定位
失败排查
审计摘要查看
```

它不服务于：

```text
普通写工具成功返回
Graph Write 执行闭环
Cleanup 执行闭环
Ownership 转换执行闭环
Review Accept / Reject
直接读取 rollback_data
```

---

# 2. list_blueprint_helper_transactions

## 2.1 工具定位

`list_blueprint_helper_transactions` 负责：

```text
按条件列出 BlueprintHelper Transaction Journal 的摘要。
```

它是只读工具。

不负责：

```text
回滚 transaction
修改 Review 状态
导出完整 diff
返回 rollback_data
读取完整 node snapshot
```

---

## 2.2 operation

固定使用：

```json
"operation": "list_blueprint_helper_transactions"
```

---

## 2.3 data.schema

```json
"schema": "ListBlueprintHelperTransactions.v1"
```

---

## 2.4 target 字段

查询类工具的 `target` 表达查询范围。

按资产查询：

```json
"target": {
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "query_scope": "asset"
}
```

按 cleanup transaction 查询：

```json
"target": {
  "query_scope": "cleanup_transactions"
}
```

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 可选 | 限定目标资产。 |
| `QueryScope` | `EBlueprintHelperTransactionQueryScope` | `target.query_scope` | `string enum` | 是 | 查询范围。 |
| `OperationFilter` | `FString` 或 enum | `target.operation_filter` | `string` | 可选 | 按 operation 过滤。 |
| `StatusFilter` | `FString` 或 enum | `target.status_filter` | `string` | 可选 | 按 transaction status 过滤。 |
| `ReviewStatusFilter` | `FString` 或 enum | `target.review_status_filter` | `string` | 可选 | 按 Review 状态过滤。 |
| `Limit` | `int32` | `target.limit` | `number` | 可选 | 分页大小。默认建议 20。 |
| `Cursor` | `FString` | `target.cursor` | `string` | 可选 | 分页游标。 |

建议 `query_scope` 枚举：

```text
asset
all
cleanup_transactions
graph_write_transactions
ownership_transactions
review_pending
rollback_available
```

第一版可只实现：

```text
asset
all
cleanup_transactions
review_pending
```

---

## 2.5 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "list_blueprint_helper_transactions",
  "trace_id": "trace_20260503_2201",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "query_scope": "asset"
  },

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
      },
      {
        "transaction_id": "tx_20260503_1901",
        "operation": "convert_blueprint_helper_block_to_user_owned",
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

---

## 2.6 transactions[] 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `TransactionId` | `FString` | `data.transactions[].transaction_id` | `string` | 是 | 事务 ID。此查询工具允许返回。 |
| `Operation` | `FString` 或 enum | `data.transactions[].operation` | `string` | 是 | 产生事务的 operation。 |
| `Status` | `FString` 或 enum | `data.transactions[].status` | `string` | 是 | 事务状态。 |
| `AssetCount` | `int32` | `data.transactions[].asset_count` | `number` | 是 | 涉及资产数量。 |
| `ReviewStatus` | `FString` 或 enum | `data.transactions[].review_status` | `string` | 是 | Review 状态。 |
| `bRollbackAvailable` | `bool` | `data.transactions[].rollback_available` | `boolean` | 是 | 是否有可用 rollback 数据或回滚路径。 |

可选字段：

| UE 字段 | MCP JSON 路径 | 说明 |
|---|---|---|
| `CreatedAt` | `data.transactions[].created_at` | 创建时间。第一版可不返回。 |
| `UpdatedAt` | `data.transactions[].updated_at` | 更新时间。第一版可不返回。 |
| `PrimaryAssetPath` | `data.transactions[].primary_asset_path` | 主资产路径。第一版可不返回。 |

默认不返回：

```text
full_diff
rollback_data
node snapshots
created_nodes
deleted_nodes
diagnostics
complete target list
```

---

## 2.7 page 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Limit` | `int32` | `data.page.limit` | `number` | 是 | 当前页大小。 |
| `bHasMore` | `bool` | `data.page.has_more` | `boolean` | 是 | 是否还有下一页。 |
| `NextCursor` | `FString` | `data.page.next_cursor` | `string` | 有下一页时 | 下一页游标。 |

---

## 2.8 空结果

空结果不是失败：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "list_blueprint_helper_transactions",
  "trace_id": "trace_20260503_2202",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "query_scope": "asset"
  },

  "data": {
    "schema": "ListBlueprintHelperTransactions.v1",
    "transactions": [],
    "page": {
      "limit": 20,
      "has_more": false
    }
  }
}
```

---

## 2.9 查询失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "list_blueprint_helper_transactions",
  "trace_id": "trace_20260503_2203",
  "status": "failed",
  "modified": false,

  "target": {
    "query_scope": "asset"
  },

  "error": {
    "code": "journal_store_unavailable",
    "stage": "open_journal_store",
    "message": "BlueprintHelper Transaction Journal store is unavailable.",
    "retryable": true
  }
}
```

---

# 3. read_blueprint_helper_transaction

## 3.1 工具定位

`read_blueprint_helper_transaction` 负责：

```text
读取一个明确 transaction 的审计摘要。
```

它是只读工具。

默认不返回：

```text
full diff
rollback_data
node snapshot
pin snapshot
complete diagnostics
```

---

## 3.2 operation

固定使用：

```json
"operation": "read_blueprint_helper_transaction"
```

---

## 3.3 data.schema

```json
"schema": "ReadBlueprintHelperTransaction.v1"
```

---

## 3.4 target 字段

```json
"target": {
  "transaction_id": "tx_20260503_1704",
  "query_scope": "transaction",
  "detail_level": "summary"
}
```

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `TransactionId` | `FString` | `target.transaction_id` | `string` | 是 | 要读取的 transaction。 |
| `QueryScope` | `EBlueprintHelperTransactionQueryScope` | `target.query_scope` | `string enum` | 是 | 固定为 `transaction`。 |
| `DetailLevel` | `EBlueprintHelperTransactionDetailLevel` | `target.detail_level` | `string enum` | 可选 | 默认 `summary`。 |

`detail_level` 建议：

```text
summary
debug
```

第一版可只实现：

```text
summary
```

---

## 3.5 summary 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_blueprint_helper_transaction",
  "trace_id": "trace_20260503_2301",
  "status": "completed",
  "modified": false,

  "target": {
    "transaction_id": "tx_20260503_1704",
    "query_scope": "transaction",
    "detail_level": "summary"
  },

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

---

## 3.6 data.transaction 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `TransactionId` | `FString` | `data.transaction.transaction_id` | `string` | 是 | 当前读取的 transaction。 |
| `Operation` | `FString` 或 enum | `data.transaction.operation` | `string` | 是 | 产生事务的 operation。 |
| `Status` | `FString` 或 enum | `data.transaction.status` | `string` | 是 | 事务状态。 |
| `ReviewStatus` | `FString` 或 enum | `data.transaction.review_status` | `string` | 是 | Review 状态。 |
| `bRollbackAvailable` | `bool` | `data.transaction.rollback_available` | `boolean` | 是 | 是否有可用 rollback 路径。 |
| `Targets` | `TArray<FBlueprintHelperTransactionTargetSummary>` | `data.transaction.targets` | `array<object>` | 是 | 目标摘要。 |
| `Summary` | `FBlueprintHelperTransactionSummary` | `data.transaction.summary` | `object` | 是 | 事务摘要。 |

---

## 3.7 targets[] 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `data.transaction.targets[].asset_path` | `string` | 是 | 目标资产。 |
| `GraphName` | `FString` | `data.transaction.targets[].graph` | `string` | 可选 | 目标图表 / 函数图。 |

默认不返回：

```text
node_path
pin_path
node_guid
pin_guid
full target list
```

---

## 3.8 summary 字段映射

`summary` 是 operation-specific 的轻量摘要。

可选字段：

| UE 字段 | MCP JSON 路径 | 说明 |
|---|---|---|
| `AffectedAssets` | `data.transaction.summary.affected_assets` | 受影响资产数。 |
| `AffectedOwnedBlocks` | `data.transaction.summary.affected_owned_blocks` | 受影响 owned block 数。 |
| `ConvertedCount` | `data.transaction.summary.converted_count` | ownership 转换数量。 |
| `CleanedCount` | `data.transaction.summary.cleaned_count` | cleanup 清理数量。 |
| `CreatedAssetCount` | `data.transaction.summary.created_asset_count` | 创建资产数量。 |
| `CreatedComponentCount` | `data.transaction.summary.created_component_count` | 创建组件数量。 |

规则：

```text
按 operation 返回相关字段，不强制所有字段都存在。
```

不返回：

```text
node list
pin list
full diff
before snapshot
after snapshot
rollback_data
complete diagnostics
```

---

## 3.9 Markdown 摘要可选但非默认

可选返回：

```json
{
  "data": {
    "schema": "ReadBlueprintHelperTransaction.v1",
    "format": "markdown",
    "markdown": "## Transaction Summary\n\n- Transaction: `tx_20260503_1704`\n- Operation: `cleanup_blueprint_helper_feature`\n- Status: applied\n- Review: pending\n- Rollback available: true\n- Affected assets: 1\n- Affected owned blocks: 3"
  }
}
```

第一版默认使用结构化 summary。Markdown 可作为 debug / UI 展示扩展。

---

## 3.10 transaction 不存在

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_blueprint_helper_transaction",
  "trace_id": "trace_20260503_2302",
  "status": "failed",
  "modified": false,

  "target": {
    "transaction_id": "tx_missing",
    "query_scope": "transaction"
  },

  "error": {
    "code": "transaction_not_found",
    "stage": "resolve_transaction",
    "message": "The requested BlueprintHelper transaction was not found.",
    "retryable": false
  }
}
```

---

# 4. rollback_data 边界

第一版不通过 `read_blueprint_helper_transaction` 默认返回 rollback_data。

原因：

```text
1. rollback_data 可能很大。
2. rollback_data 是 UE 侧恢复实现细节。
3. Agent 正常不需要读取恢复快照。
4. Rollback 工具应自己读取 Journal 内部 rollback_data。
```

如后续需要，应单独设计：

```text
export_blueprint_helper_transaction_debug_bundle
```

---

# 5. Review 状态写入边界

第一版不做 Agent 写工具：

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

只读工具可以返回：

```text
review_status
```

但不修改 Review 状态。

---

# 6. UE 侧建议结构体

```cpp
struct FBlueprintHelperListTransactionsResultData
{
    FString Schema; // ListBlueprintHelperTransactions.v1
    TArray<FBlueprintHelperTransactionListItem> Transactions;
    FBlueprintHelperPageInfo Page;
};

struct FBlueprintHelperTransactionListItem
{
    FString TransactionId;
    FString Operation;
    FString Status;
    int32 AssetCount = 0;
    FString ReviewStatus;
    bool bRollbackAvailable = false;

    // Optional extensions.
    FString CreatedAt;
    FString UpdatedAt;
    FString PrimaryAssetPath;
};

struct FBlueprintHelperPageInfo
{
    int32 Limit = 20;
    bool bHasMore = false;
    FString NextCursor;
};

struct FBlueprintHelperReadTransactionResultData
{
    FString Schema; // ReadBlueprintHelperTransaction.v1
    FBlueprintHelperTransactionSummaryRecord Transaction;
};

struct FBlueprintHelperTransactionSummaryRecord
{
    FString TransactionId;
    FString Operation;
    FString Status;
    FString ReviewStatus;
    bool bRollbackAvailable = false;
    TArray<FBlueprintHelperTransactionTargetSummary> Targets;
    FBlueprintHelperTransactionOperationSummary Summary;
};

struct FBlueprintHelperTransactionTargetSummary
{
    FString AssetPath;
    FString GraphName;
};

struct FBlueprintHelperTransactionOperationSummary
{
    int32 AffectedAssets = 0;
    int32 AffectedOwnedBlocks = 0;
    int32 ConvertedCount = 0;
    int32 CleanedCount = 0;
    int32 CreatedAssetCount = 0;
    int32 CreatedComponentCount = 0;
};
```

---

# 7. 验收标准

```text
1. list operation 固定为 list_blueprint_helper_transactions。
2. list data.schema 固定为 ListBlueprintHelperTransactions.v1。
3. list 是只读工具，modified=false。
4. list target 表达 query_scope / asset_path / filter / limit / cursor。
5. list 成功返回 transactions[] 摘要。
6. transactions[] 可返回 transaction_id。
7. transactions[] 默认不返回 full diff / rollback_data / node snapshots。
8. 空结果 status=completed / transactions=[]。
9. read operation 固定为 read_blueprint_helper_transaction。
10. read data.schema 固定为 ReadBlueprintHelperTransaction.v1。
11. read 必须以 transaction_id 定位。
12. read 默认 detail_level=summary。
13. read summary 返回 transaction / targets / summary。
14. read 不默认返回 rollback_data / full diff / node snapshots。
15. Review Accept / Reject 第一版不做 Agent 写工具。
16. Rollback 工具内部读取 rollback_data，不要求 Agent 先读 rollback_data。
