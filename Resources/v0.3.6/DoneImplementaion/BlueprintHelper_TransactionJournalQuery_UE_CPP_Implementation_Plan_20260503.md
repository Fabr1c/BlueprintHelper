# BlueprintHelper Transaction Journal / Review Query UE 侧 C++ 可执行实现计划

状态：[x] 已完成
日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_TransactionJournalQuery_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、Review UI Accept / Reject、rollback_data 导出、完整 diff 导出

---

## 0. 实现目标

实现两个只读查询工具：

```text
list_blueprint_helper_transactions
read_blueprint_helper_transaction
```

它们用于：

```text
用户明确查询事务
Debug
Rollback 定位
失败排查
审计摘要查看
```

它们不用于：

```text
普通写工具成功返回
Graph Write 执行闭环
Cleanup 执行闭环
Ownership 转换执行闭环
Review Accept / Reject
直接读取 rollback_data
```

字段契约核心点：

```text
1. list/read 都是只读工具，modified=false。
2. list 返回 transactions[] 摘要。
3. read 返回单个 transaction summary。
4. 事务查询工具可以返回 transaction_id，因为该工具就是事务查询。
5. 默认不返回 full diff / rollback_data / node snapshots。
6. 空结果不是失败。
7. Review Accept / Reject 第一版不做 Agent 写工具。
8. Rollback 工具内部读取 rollback_data，不要求 Agent 先读取 rollback_data。
```

---

## 1. 当前依赖与复用前提

本计划假设前序阶段已经实现或正在实现：

```text
FBlueprintHelperToolResultBase
FBlueprintHelperToolResultBuilder
FBlueprintHelperTransactionJournalService
FBlueprintHelperReviewStoreService
FBlueprintHelperTransactionJournalRecord
FBlueprintHelperPageInfo
FBlueprintHelperConflictItem
```

如果当前 Journal 还只是临时 JSON 文件，第一版应先实现文件系统索引读取，不强制引入 SQLite 或数据库。

推荐 Journal 存储路径：

```text
<Project>/Saved/BlueprintHelper/Transactions/Active/
<Project>/Saved/BlueprintHelper/Transactions/Archived/
<Project>/Saved/BlueprintHelper/Review/
```

第一版可只读取：

```text
Active/
Review/
```

Archived/Compacted 后续扩展。

---

## 2. Phase A：新增类型文件

### 2.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperTransactionQueryTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperTransactionQueryTypes.cpp
```

如果已有 Transaction Journal 类型文件，可在现有文件中新增 Query DTO，避免分散。

### 2.2 新增枚举

```cpp
enum class EBlueprintHelperTransactionQueryScope : uint8
{
    Asset,
    All,
    CleanupTransactions,
    GraphWriteTransactions,
    OwnershipTransactions,
    ReviewPending,
    RollbackAvailable,
    Transaction
};

enum class EBlueprintHelperTransactionDetailLevel : uint8
{
    Summary,
    Debug
};

enum class EBlueprintHelperTransactionQueryStage : uint8
{
    ParseInput,
    OpenJournalStore,
    ReadIndex,
    ResolveTransaction,
    ReadJournalRecord,
    ReadReviewRecord,
    ApplyFilters,
    BuildSummary
};

enum class EBlueprintHelperTransactionQueryErrorCode : uint8
{
    InvalidRequest,
    JournalStoreUnavailable,
    JournalIndexCorrupted,
    TransactionNotFound,
    TransactionRecordUnreadable,
    UnsupportedQueryScope,
    UnsupportedDetailLevel,
    CursorInvalid,
    InternalError
};
```

### 2.3 字符串序列化

稳定输出：

```text
asset
all
cleanup_transactions
review_pending
rollback_available
transaction
summary
list_blueprint_helper_transactions
read_blueprint_helper_transaction
```

不要输出 C++ enum 原名。

---

## 3. Phase B：list result 结构

### 3.1 List result data

```cpp
struct FBlueprintHelperListTransactionsResultData
{
    FString Schema = TEXT("ListBlueprintHelperTransactions.v1");
    TArray<FBlueprintHelperTransactionListItem> Transactions;
    FBlueprintHelperTransactionPageInfo Page;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperTransactionListItem
{
    FString TransactionId;
    FString Operation;
    FString Status;
    int32 AssetCount = 0;
    FString ReviewStatus;
    bool bRollbackAvailable = false;

    // 第一版可不默认输出，保留为 optional。
    TOptional<FString> CreatedAt;
    TOptional<FString> UpdatedAt;
    TOptional<FString> PrimaryAssetPath;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperTransactionPageInfo
{
    int32 Limit = 20;
    bool bHasMore = false;
    TOptional<FString> NextCursor;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.2 list 成功 JSON

```json
{
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
```

### 3.3 list 空结果

```json
{
  "schema": "ListBlueprintHelperTransactions.v1",
  "transactions": [],
  "page": {
    "limit": 20,
    "has_more": false
  }
}
```

空结果：

```text
ok=true
status=completed
modified=false
```

不是失败。

---

## 4. Phase C：read result 结构

### 4.1 Read result data

```cpp
struct FBlueprintHelperReadTransactionResultData
{
    FString Schema = TEXT("ReadBlueprintHelperTransaction.v1");
    FBlueprintHelperTransactionSummaryRecord Transaction;

    TSharedRef<FJsonObject> ToJson() const;
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

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperTransactionTargetSummary
{
    FString AssetPath;
    TOptional<FString> GraphName;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperTransactionOperationSummary
{
    TOptional<int32> AffectedAssets;
    TOptional<int32> AffectedOwnedBlocks;
    TOptional<int32> ConvertedCount;
    TOptional<int32> CleanedCount;
    TOptional<int32> CreatedAssetCount;
    TOptional<int32> CreatedComponentCount;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 4.2 read summary JSON

```json
{
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
```

### 4.3 明确禁止字段

read summary 不返回：

```text
rollback_data
full_diff
node snapshots
pin snapshots
node_path
pin_path
node_guid
pin_guid
complete diagnostics
```

---

## 5. Phase D：请求结构

### 5.1 list request

```cpp
struct FBlueprintHelperListTransactionsRequest
{
    EBlueprintHelperTransactionQueryScope QueryScope =
        EBlueprintHelperTransactionQueryScope::All;

    TOptional<FString> AssetPath;
    TOptional<FString> OperationFilter;
    TOptional<FString> StatusFilter;
    TOptional<FString> ReviewStatusFilter;

    int32 Limit = 20;
    TOptional<FString> Cursor;
};
```

### 5.2 read request

```cpp
struct FBlueprintHelperReadTransactionRequest
{
    FString TransactionId;
    EBlueprintHelperTransactionQueryScope QueryScope =
        EBlueprintHelperTransactionQueryScope::Transaction;

    EBlueprintHelperTransactionDetailLevel DetailLevel =
        EBlueprintHelperTransactionDetailLevel::Summary;
};
```

### 5.3 list target 输出

按资产查询：

```json
"target": {
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "query_scope": "asset"
}
```

按 cleanup 查询：

```json
"target": {
  "query_scope": "cleanup_transactions"
}
```

带分页：

```json
"target": {
  "query_scope": "all",
  "limit": 20,
  "cursor": "..."
}
```

### 5.4 read target 输出

```json
"target": {
  "transaction_id": "tx_20260503_1704",
  "query_scope": "transaction",
  "detail_level": "summary"
}
```

---

## 6. Phase E：TransactionJournalService 查询能力

### 6.1 扩展 Journal service

```cpp
class FBlueprintHelperTransactionJournalService
{
public:
    bool ListTransactions(
        const FBlueprintHelperListTransactionsRequest& Request,
        FBlueprintHelperTransactionListPage& OutPage,
        FBlueprintHelperToolError& OutError) const;

    bool ReadTransactionSummary(
        const FString& TransactionId,
        FBlueprintHelperTransactionSummaryRecord& OutRecord,
        FBlueprintHelperToolError& OutError) const;

    bool TryLoadJournalRecord(
        const FString& TransactionId,
        FBlueprintHelperTransactionJournalRecord& OutRecord,
        FString& OutError) const;

    bool TryLoadReviewStatus(
        const FString& TransactionId,
        FString& OutReviewStatus,
        FString& OutError) const;
};
```

### 6.2 Internal list page

```cpp
struct FBlueprintHelperTransactionListPage
{
    TArray<FBlueprintHelperTransactionListItem> Items;
    int32 Limit = 20;
    bool bHasMore = false;
    TOptional<FString> NextCursor;
};
```

### 6.3 Journal index

第一版建议实现轻量索引：

```text
<Project>/Saved/BlueprintHelper/Transactions/index.json
```

index item：

```json
{
  "transaction_id": "tx_20260503_1704",
  "operation": "cleanup_blueprint_helper_feature",
  "status": "applied",
  "target_assets": ["/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"],
  "review_status": "pending",
  "rollback_available": true,
  "created_at": "2026-05-03T17:04:00Z",
  "journal_path": "Active/tx_20260503_1704.json"
}
```

Agent-facing list 不返回 `journal_path`。

如果暂时没有 index，可扫描 `Active/*.json`。但仍要在 service 层抽象成 index 读取，后续可替换为缓存或数据库。

---

## 7. Phase F：list 实现

### 7.1 支持的 query_scope

第一版必须实现：

```text
all
asset
cleanup_transactions
review_pending
```

可选实现：

```text
graph_write_transactions
ownership_transactions
rollback_available
```

不支持时：

```text
ok=false
status=failed
error.code=unsupported_query_scope
```

### 7.2 filter 规则

```cpp
bool MatchesFilters(
    const FBlueprintHelperJournalIndexItem& Item,
    const FBlueprintHelperListTransactionsRequest& Request)
{
    // query_scope
    // asset_path
    // operation_filter
    // status_filter
    // review_status_filter
}
```

具体语义：

```text
query_scope=asset：
  item.target_assets 包含 asset_path。

query_scope=cleanup_transactions：
  operation in cleanup_blueprint_helper_block / cleanup_blueprint_helper_feature。

query_scope=review_pending：
  review_status=pending 或 pending_review。

query_scope=all：
  不额外限制。
```

### 7.3 分页

第一版 cursor 可以采用 offset cursor：

```text
cursor = base64("offset:20")
```

或纯数字字符串：

```text
cursor = "20"
```

推荐内部保守实现：

```cpp
int32 Offset = DecodeCursor(Request.Cursor).Get(0);
int32 Limit = FMath::Clamp(Request.Limit, 1, 100);
```

返回：

```text
page.limit = Limit
page.has_more = Offset + Limit < FilteredItems.Num()
page.next_cursor = has_more ? EncodeCursor(Offset + Limit) : unset
```

### 7.4 排序

默认：

```text
created_at desc
```

如果没有 created_at：

```text
transaction_id desc
```

### 7.5 成功 ToolResult

```cpp
Result.bOk = true;
Result.Operation = TEXT("list_blueprint_helper_transactions");
Result.Status = TEXT("completed");
Result.bModified = false;
Result.Target = MakeListTarget(Request);
Result.Data = ListData.ToJson();
```

---

## 8. Phase G：read 实现

### 8.1 transaction_id 定位

read 必须以 transaction_id 定位。

缺失：

```text
ok=false
status=failed
error.code=invalid_request
stage=parse_input
```

不存在：

```text
ok=false
status=failed
error.code=transaction_not_found
stage=resolve_transaction
```

### 8.2 detail_level

第一版仅支持：

```text
summary
```

如果请求：

```text
debug
```

两种选择：

```text
A. 返回 unsupported_detail_level
B. 当作 summary
```

推荐 A，避免用户误以为已经返回 debug 详情。

### 8.3 Summary 构建

从 Journal record 提取：

```text
transaction_id
operation
status
target_assets
targets graph summary
review_status
rollback_available
operation-specific summary
```

### 8.4 operation-specific summary

根据 operation 填充轻量字段：

#### Cleanup

```text
affected_assets
affected_owned_blocks
cleaned_count
```

#### ConvertBlueprintHelperBlockToUserOwned

```text
affected_assets
converted_count
```

#### Asset Factory

```text
affected_assets
created_asset_count
```

#### Component

```text
affected_assets
created_component_count
```

#### Graph Write

```text
affected_assets
affected_owned_blocks
```

字段只返回相关项，不强制所有字段存在。

### 8.5 Rollback available 计算

优先从 Journal record 读取：

```text
rollback_available
rollback_data_status
```

若没有该字段：

```text
rollback_available = rollback_data exists && not compacted && status=applied
```


---

## 9. Phase H：ReviewStoreService 读取

### 9.1 扩展 ReviewStoreService

```cpp
class FBlueprintHelperReviewStoreService
{
public:
    bool TryGetReviewStatus(
        const FString& TransactionId,
        FString& OutReviewStatus,
        FString& OutError) const;
};
```

### 9.2 默认 review_status

如果 Review record 不存在：

```text
review_status = "unknown"
```

不要让缺失 Review record 导致 list/read 失败，除非 Journal 明确标记 Review 必须存在而记录损坏。

### 9.3 Review 写入边界

本工具簇只读，不新增：

```text
accept_review_transaction
reject_review_transaction
```

Review Accept / Reject 第一版通过 UE Review UI 完成。

---

## 10. Phase I：ToolResult 构建

### 10.1 list 成功

```cpp
FBlueprintHelperToolResultBase BuildListSuccess(
    const FBlueprintHelperListTransactionsRequest& Request,
    const FBlueprintHelperTransactionListPage& Page)
{
    FBlueprintHelperToolResultBase Result;
    Result.bOk = true;
    Result.Schema = TEXT("BlueprintHelper.McpToolResult.v1");
    Result.Operation = TEXT("list_blueprint_helper_transactions");
    Result.Status = TEXT("completed");
    Result.bModified = false;
    Result.Target = MakeListTarget(Request);

    FBlueprintHelperListTransactionsResultData Data;
    Data.Transactions = Page.Items;
    Data.Page.Limit = Page.Limit;
    Data.Page.bHasMore = Page.bHasMore;
    Data.Page.NextCursor = Page.NextCursor;

    Result.Data = Data.ToJson();
    return Result;
}
```

### 10.2 read 成功

```cpp
Result.Operation = TEXT("read_blueprint_helper_transaction");
Result.Status = TEXT("completed");
Result.bModified = false;
Result.Target = MakeReadTarget(Request);
Result.Data = ReadData.ToJson();
```

### 10.3 失败

```cpp
Result.bOk = false;
Result.Status = TEXT("failed");
Result.bModified = false;
Result.Error = MakeTransactionQueryError(...);
```

---

## 11. Phase J：Bridge Router 接入

### 11.1 新增 commands

```text
list_blueprint_helper_transactions
read_blueprint_helper_transaction
```

### 11.2 Router 分支

```cpp
if (Request.Command == TEXT("list_blueprint_helper_transactions"))
{
    return HandleListBlueprintHelperTransactions(Request);
}

if (Request.Command == TEXT("read_blueprint_helper_transaction"))
{
    return HandleReadBlueprintHelperTransaction(Request);
}
```

### 11.3 Handler

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleListBlueprintHelperTransactions(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result =
        TransactionQueryService.List(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("list transactions failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

read handler 同理。

注意：

```text
空结果不是失败，BridgeResponse 必须 success。
```

---

## 12. Phase K：RequestValidator / 权限

### 12.1 list validator

```cpp
if (Command == TEXT("list_blueprint_helper_transactions"))
{
    RequireString(Payload, TEXT("query_scope"));
    OptionalString(Payload, TEXT("asset_path"));
    OptionalString(Payload, TEXT("operation_filter"));
    OptionalString(Payload, TEXT("status_filter"));
    OptionalString(Payload, TEXT("review_status_filter"));
    OptionalInt(Payload, TEXT("limit"));
    OptionalString(Payload, TEXT("cursor"));
}
```

### 12.2 read validator

```cpp
if (Command == TEXT("read_blueprint_helper_transaction"))
{
    RequireString(Payload, TEXT("transaction_id"));
    OptionalString(Payload, TEXT("query_scope"));    // default transaction
    OptionalString(Payload, TEXT("detail_level"));   // default summary
}
```

### 12.3 权限

这两个工具是只读查询：

```text
不需要 write token
不生成 transaction
不写 Journal
不修改 Review
modified=false
```

ReadOnly profile 下允许。

但如果 Journal store 包含敏感本地路径，Agent-facing 结果必须过滤：

```text
不返回 journal_path
不返回 local filesystem path
不返回 raw rollback_data path
```

---

## 13. Phase L：rollback_data / full diff 边界

read/list 默认不返回：

```text
rollback_data
full diff
node snapshots
pin snapshots
created_nodes
deleted_nodes
complete diagnostics
```

Rollback 工具内部应直接读取 Journal 的 rollback_data：

```text
RollbackCleanupTransaction(transaction_id)
  → UE 内部读取 rollback_data
  → 不需要 Agent read_transaction 暴露 rollback_data
```

如果未来需要 Debug 导出，另设：

```text
export_blueprint_helper_transaction_debug_bundle
```

不要把 debug bundle 混进 `read_blueprint_helper_transaction` 默认结果。

---

## 14. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperTransactionQueryContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperTransactionQueryStoreTests.cpp
```

### 14.1 list contract tests

```text
1. list_success_contract
   - ok=true
   - operation=list_blueprint_helper_transactions
   - status=completed
   - modified=false
   - data.schema=ListBlueprintHelperTransactions.v1
   - transactions[] 摘要包含 transaction_id/operation/status/asset_count/review_status/rollback_available
   - 不返回 rollback_data/full_diff/node_snapshots

2. list_empty_contract
   - ok=true
   - status=completed
   - transactions=[]
   - page.limit 存在
   - page.has_more=false

3. list_failed_contract
   - journal_store_unavailable
   - ok=false
   - status=failed
   - error.code=journal_store_unavailable
```

### 14.2 read contract tests

```text
1. read_summary_contract
   - ok=true
   - operation=read_blueprint_helper_transaction
   - status=completed
   - modified=false
   - data.schema=ReadBlueprintHelperTransaction.v1
   - data.transaction.transaction_id 存在
   - data.transaction.targets[] 存在
   - data.transaction.summary 存在
   - 不返回 rollback_data/full_diff/node_snapshots

2. read_missing_transaction_contract
   - ok=false
   - status=failed
   - error.code=transaction_not_found

3. read_unsupported_detail_level_contract
   - detail_level=debug
   - ok=false
   - error.code=unsupported_detail_level
```

### 14.3 Store tests

```text
1. list_filters_by_asset
2. list_filters_cleanup_transactions
3. list_filters_review_pending
4. list_pagination_returns_next_cursor
5. read_builds_cleanup_summary
6. read_builds_ownership_summary
7. missing_review_record_returns_unknown_review_status
8. journal_path_never_leaks_to_agent
```

---

## 15. 推荐提交顺序

### Commit 1：Query DTO 与序列化

```text
Add Transaction Query result types
Add list/read schema serialization
Add query scope / detail level / error enums
```

验收：

```text
能构造 list/read 成功 JSON。
结果不包含 rollback_data/full_diff。
```

### Commit 2：Journal index reader

```text
Add TransactionJournalService list/read index helpers
Scan Active/*.json if index missing
Normalize review_status and rollback_available
```

验收：

```text
能读取现有 transaction journal 摘要。
空目录返回 transactions=[]。
```

### Commit 3：list service

```text
Add ListBlueprintHelperTransactionsService
Implement query_scope filters
Implement asset_path / operation / status / review filters
Implement pagination
```

验收：

```text
query_scope=asset/all/cleanup_transactions/review_pending 可用。
```

### Commit 4：read service

```text
Add ReadBlueprintHelperTransactionService
Resolve transaction_id
Build summary record
Reject unsupported detail_level
```

验收：

```text
summary 不返回 full diff/rollback data。
transaction_not_found 正确失败。
```

### Commit 5：ReviewStore integration

```text
Read review_status from ReviewStore
Fallback unknown if review record absent
Do not modify Review state
```

验收：

```text
Review 缺失不导致查询失败。
```

### Commit 6：Bridge / Validator

```text
Register list_blueprint_helper_transactions
Register read_blueprint_helper_transaction
Add request validation
Keep tools read-only and token-free
```

验收：

```text
ReadOnly 下可调用。
modified=false。
```

### Commit 7：Protocol regression tests

```text
Add contract tests
Add no rollback_data leak tests
Add no journal_path leak tests
```

验收：

```text
Agent-facing 返回只含必要摘要。
```

---

## 16. 第一版不做的内容

```text
1. 不实现 Review Accept / Reject Agent 写工具。
2. 不导出 rollback_data。
3. 不导出 full diff。
4. 不导出 node snapshot。
5. 不导出 pin snapshot。
6. 不读取完整 diagnostics。
7. 不做任意 transaction rollback。
8. 不做 transaction compact / archive 操作。
9. 不返回 journal_path。
10. 不暴露本地文件系统路径。
```

---

## 17. 最小验收标准

list 成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "list_blueprint_helper_transactions",
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
      }
    ],
    "page": {
      "limit": 20,
      "has_more": false
    }
  }
}
```

list 空结果：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "list_blueprint_helper_transactions",
  "status": "completed",
  "modified": false,
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

read 成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_blueprint_helper_transaction",
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

必须不出现：

```text
rollback_data
full_diff
node snapshots
pin snapshots
journal_path
local filesystem path
review write actions
```

---

## 18. 实现风险

### 18.1 Journal index 不存在

风险：

```text
前期 transaction journal 只有散落 json 文件，没有 index。
```

处理：

```text
第一版扫描 Active/*.json。
后续异步维护 index.json。
```


风险：

```text
Journal 有记录，ReviewStore 缺记录。
```

处理：

```text
review_status=unknown。
不让查询失败。
```

### 18.3 rollback_available 判断过度承诺

风险：

```text
```

处理：

```text
rollback_available 只表示 rollback_data/路径存在。
正式能否执行由 Rollback 工具 dry_run 再判断。
```

### 18.4 泄露内部路径

风险：

```text
Journal record 内有 journal_path / local snapshot path。
```

处理：

```text
Agent-facing DTO 白名单输出。
不要直接序列化原始 Journal JSON。
```
