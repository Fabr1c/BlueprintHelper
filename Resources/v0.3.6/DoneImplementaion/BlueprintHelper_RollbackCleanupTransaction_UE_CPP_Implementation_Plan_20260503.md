# BlueprintHelper RollbackCleanupTransaction UE 侧 C++ 可执行实现计划

日期：2026-05-03  
输入依据：`BlueprintHelper_RollbackCleanupTransaction_UE_FieldMapping_20260503.md`  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
实现范围：UE 插件侧 C++  
不包含：MCP Server TypeScript 工具 schema、Agent Skill 文档、Review UI 交互实现

---

## 0. 实现目标

新增 `RollbackCleanupTransaction` UE 侧能力，专门回滚 Cleanup 工具产生的 transaction。

第一版只支持回滚：

```text
CleanupBlueprintHelperBlock
CleanupBlueprintHelperFeature
```

不支持：

```text
通用 Graph Write 回滚
资产创建回滚
普通 Component / Class Settings 回滚
无 rollback_data 的 transaction
已经 compacted 且无法恢复的 transaction
```

通用回滚后续应另设：

```text
RollbackTransaction
```

本工具的核心语义是：

```text
读取 cleanup transaction 的 Journal / rollback_data
→ dry_run 检查是否仍可安全恢复
→ 正式执行时恢复被 cleanup 删除的 owned block / 节点 / 连线 / metadata
→ 为本次 rollback 写操作生成新的 transaction_id
→ 写入新的 Journal / Review 记录
→ 返回极简 Agent-facing 结果
```

---

## 1. 字段契约摘要

### 1.1 operation

固定：

```json
"operation": "rollback_cleanup_transaction"
```

### 1.2 target

Agent-facing target 只返回：

```json
{
  "transaction_id": "tx_20260503_1704",
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "rollback_scope": "cleanup_transaction"
}
```

不返回：

```text
target_type
target_kind
block_ref
feature_name
source_operation
```

### 1.3 dry_run

Rollback 是高风险写操作，必须 dry_run。

dry_run 返回：

```text
data.dry_run.result
data.dry_run.can_execute
data.dry_run.rollback_summary
data.dry_run.blocked_by
data.dry_run.conflicts
data.dry_run.errors
```

dry_run 不返回：

```text
rollback_data
node snapshot
full diff
transaction_id
source_operation
```

### 1.4 正式成功

正式成功返回：

```text
data.rollback_result.rolled_back_transaction_id
data.rollback_result.rollback_status
data.write_ref.transaction_id
data.write_ref.journal_recorded
validation
```

必须区分：

```text
rolled_back_transaction_id = 被回滚的 cleanup transaction
write_ref.transaction_id = 本次 rollback 写操作自己的 transaction
```

### 1.5 no_op

`already_rolled_back_policy` 支持：

```text
error
ignore
```

默认：

```text
error
```

`ignore` 且目标已回滚时返回：

```text
status = no_op
modified = false
rollback_status = already_rolled_back
```

### 1.6 正式失败

失败不返回：

```text
data.rollback_result
data.write_ref
review
safety
diagnostics
next
```

但 `error` 必须完整。

---

## 2. 复用前置服务

RollbackCleanupTransaction 应复用或依赖以下现有 / 新增服务：

```text
FBlueprintHelperTransactionJournalService
FBlueprintHelperReviewStoreService
FBlueprintHelperOwnershipService
FBlueprintHelperGraphResolver
FBlueprintHelperScopedAssetMutation
FBlueprintHelperToolResultBase
FBlueprintHelperToolResultBuilder
```

如果前面 CleanupBlueprintHelperBlock 已经实现：

```text
TransactionJournalService
Cleanup rollback_data schema
Owned block metadata scanner
Node snapshot serializer
Link snapshot serializer
```

RollbackCleanupTransaction 应直接消费这些数据，不重新设计 snapshot 格式。

---

## 3. 新增文件

建议新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperRollbackCleanupTransactionService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperRollbackCleanupTransactionService.cpp

Source/BlueprintHelper/Public/Services/BlueprintHelperRollbackCleanupTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperRollbackCleanupTypes.cpp
```

如已有 Graph Write 通用类型文件，可复用：

```text
FBlueprintHelperWriteRef
FBlueprintHelperConflictItem
FBlueprintHelperDryRunIssue
FBlueprintHelperFailedItem
EBlueprintHelperRollbackResult
```

---

## 4. 新增枚举

```cpp
enum class EBlueprintHelperRollbackScope : uint8
{
    CleanupTransaction
};

enum class EBlueprintHelperRollbackCleanupStage : uint8
{
    ParseInput,
    ResolveTransaction,
    ValidateTransaction,
    LoadJournal,
    LoadRollbackData,
    CheckAlreadyRolledBack,
    ResolveAssets,
    CheckAssetState,
    CheckOwnership,
    RestoreGraphs,
    RestoreNodes,
    RestoreLinks,
    RestoreMetadata,
    WriteJournal,
    Rollback
};

enum class EBlueprintHelperRollbackCleanupErrorCode : uint8
{
    TransactionNotFound,
    TransactionTypeMismatch,
    RollbackDataUnavailable,
    RollbackDataCompacted,
    AlreadyRolledBack,
    AssetNotFound,
    AssetStateConflict,
    GraphNotFound,
    GraphStateConflict,
    OwnershipConflict,
    RestoreNodeFailed,
    RestoreLinkFailed,
    RestoreMetadataFailed,
    JournalWriteFailed,
    WritePermissionDisabled,
    ProfilePolicyViolation,
    BridgeDisconnected,
    RollbackBlocked,
    RollbackFailed
};

enum class EBlueprintHelperAlreadyRolledBackPolicy : uint8
{
    Error,
    Ignore
};

enum class EBlueprintHelperRollbackCleanupStatus : uint8
{
    Succeeded,
    AlreadyRolledBack
};
```

`EBlueprintHelperRollbackResult` 必须能序列化为：

```text
not_needed
rolled_back
blocked
failed
```

注意：

```text
error.rollback_result 表示“本次 rollback 写操作自身失败后是否撤销了部分恢复动作”。
data.rollback_result.rollback_status 表示“目标 cleanup transaction 的回滚状态”。
两者不能混用。
```

---

## 5. UE 侧结构体

### 5.1 正式结果

```cpp
struct FBlueprintHelperRollbackCleanupResultData
{
    FString Schema = TEXT("RollbackCleanupTransaction.v1");
    FBlueprintHelperRollbackCleanupResult RollbackResult;
    FBlueprintHelperWriteRef WriteRef;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperRollbackCleanupResult
{
    FString RolledBackTransactionId;
    FString RollbackStatus; // succeeded | already_rolled_back
    FString AlreadyRolledBackPolicy; // optional for no_op

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 5.2 dry_run 结果

```cpp
struct FBlueprintHelperRollbackCleanupDryRunData
{
    FString Schema = TEXT("RollbackCleanupTransactionDryRun.v1");
    FBlueprintHelperRollbackDryRunResult DryRun;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperRollbackDryRunResult
{
    FString Result; // passed | blocked
    bool bCanExecute = false;
    FBlueprintHelperRollbackSummary RollbackSummary;
    TArray<FString> BlockedBy;
    TArray<FBlueprintHelperConflictItem> Conflicts;
    TArray<FBlueprintHelperDryRunIssue> Errors;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperRollbackSummary
{
    int32 AffectedAssets = 0;
    int32 RestorableBlocks = 0;
    bool bRestorableNodesAvailable = false;
    bool bRollbackDataAvailable = false;
    bool bRestorableLinksAvailable = false;
    bool bAssetStateChecked = false;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 5.3 请求结构

```cpp
struct FBlueprintHelperRollbackCleanupRequest
{
    FString TransactionId;
    FString AssetPath; // optional, may be derived from Journal
    EBlueprintHelperRollbackScope RollbackScope = EBlueprintHelperRollbackScope::CleanupTransaction;
    EBlueprintHelperAlreadyRolledBackPolicy AlreadyRolledBackPolicy = EBlueprintHelperAlreadyRolledBackPolicy::Error;
    bool bDryRun = true;
};
```

---

## 6. Service 接口

```cpp
class FBlueprintHelperRollbackCleanupTransactionService
{
public:
    FBlueprintHelperRollbackCleanupTransactionService(
        FBlueprintHelperTransactionJournalService& InJournalService,
        FBlueprintHelperReviewStoreService& InReviewStore,
        FBlueprintHelperGraphResolver& InGraphResolver,
        FBlueprintHelperOwnershipService& InOwnershipService);

    FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload);

private:
    bool ParseRequest(
        const TSharedPtr<FJsonObject>& Payload,
        FBlueprintHelperRollbackCleanupRequest& OutRequest,
        FBlueprintHelperToolError& OutError) const;

    FBlueprintHelperRollbackPreflightResult Preflight(
        const FBlueprintHelperRollbackCleanupRequest& Request) const;

    FBlueprintHelperToolResultBase ExecuteDryRun(
        const FBlueprintHelperRollbackCleanupRequest& Request,
        const FBlueprintHelperRollbackPreflightResult& Preflight) const;

    FBlueprintHelperToolResultBase ExecuteRollback(
        const FBlueprintHelperRollbackCleanupRequest& Request,
        const FBlueprintHelperRollbackPreflightResult& Preflight);
};
```

---

## 7. Journal 读取要求

### 7.1 必须读取的 Journal 字段

RollbackCleanupTransaction 的事实来源是：

```text
transaction_id + Journal
```

必须读取：

```text
schema
transaction_id
operation / source_operation
status
target_assets
cleanup_scope
affected_blocks
deleted_nodes
deleted_links
rollback_data
rollback_status
storage_status
created_at
```

### 7.2 transaction 类型校验

只允许：

```text
operation = cleanup_blueprint_helper_block
operation = cleanup_blueprint_helper_feature
```

如果实际是：

```text
append_blueprint_graph
replace_blueprint_graph
patch_blueprint_graph
merge_blueprint_graph
asset_create
component write
class settings write
```

返回：

```json
{
  "code": "transaction_type_mismatch",
  "stage": "validate_transaction",
  "rollback_result": "not_needed"
}
```

### 7.3 rollback_data 可用性

必须检查：

```text
rollback_data 存在
rollback_data 未 compact 到不可恢复状态
rollback_data schema 版本可读
节点 snapshot 可用
连线 snapshot 可用
metadata snapshot 可用
```

不可用时 dry_run blocked：

```text
blocked_by = rollback_data_unavailable
```

正式调用若跳过 dry_run 或状态变化导致不可用，则失败：

```text
error.code = rollback_data_unavailable
error.stage = load_rollback_data
rollback_result = not_needed
modified = false
```

---

## 8. dry_run 预检流程

dry_run 必须执行以下检查：

```text
1. transaction 是否存在。
2. transaction 是否 cleanup 类型。
3. rollback_data 是否完整可用。
4. transaction 是否已经 rollback。
5. target assets 是否存在。
6. 当前资产状态是否仍可安全恢复。
7. 目标图表是否存在或可恢复。
8. 是否存在用户或后续 transaction 修改冲突。
9. 待恢复 block_id 是否与当前资产中已有 block 冲突。
10. 待恢复节点 GUID / fallback identity 是否冲突。
11. 待恢复链接两端节点是否可解析。
12. 待恢复 ownership metadata 是否可写。
```

### 8.1 passed 返回

```json
{
  "data": {
    "schema": "RollbackCleanupTransactionDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true,
      "rollback_summary": {
        "affected_assets": 1,
        "restorable_blocks": 3,
        "restorable_nodes_available": true,
        "rollback_data_available": true
      },
      "blocked_by": [],
      "conflicts": [],
      "errors": []
    }
  }
}
```

### 8.2 blocked 返回

```json
{
  "data": {
    "schema": "RollbackCleanupTransactionDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "rollback_summary": {
        "rollback_data_available": false
      },
      "blocked_by": [
        "rollback_data_unavailable"
      ],
      "conflicts": [
        {
          "code": "rollback_data_unavailable",
          "message": "Rollback data for the cleanup transaction is unavailable or compacted."
        }
      ],
      "errors": []
    }
  }
}
```

dry_run 不得返回：

```text
rollback_data
node snapshot
link snapshot
full diff
source_operation
transaction_id in rollback_summary
```

---

## 9. already_rolled_back_policy

### 9.1 默认 error

如果 Journal 记录目标 cleanup transaction 已经回滚：

```text
rollback_status = succeeded
```

且请求未显式：

```text
already_rolled_back_policy = ignore
```

则返回失败：

```json
{
  "ok": false,
  "status": "failed",
  "modified": false,
  "error": {
    "code": "already_rolled_back",
    "stage": "check_already_rolled_back",
    "message": "The cleanup transaction has already been rolled back.",
    "retryable": false,
    "rollback_result": "not_needed"
  }
}
```

### 9.2 ignore no_op

若：

```text
already_rolled_back_policy = ignore
```

返回：

```json
{
  "ok": true,
  "status": "no_op",
  "modified": false,
  "data": {
    "schema": "RollbackCleanupTransaction.v1",
    "rollback_result": {
      "rolled_back_transaction_id": "tx_20260503_1704",
      "rollback_status": "already_rolled_back",
      "already_rolled_back_policy": "ignore"
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": false,
    "compiled": false,
    "saved": false
  }
}
```

no_op 不生成新的 write_ref。

---

## 10. 正式 rollback 执行流程

正式执行必须要求上一次 dry_run passed，或在本次正式调用内部重新执行完整 preflight。

推荐流程：

```text
1. ParseRequest
2. Load target cleanup Journal
3. Re-run full preflight
4. Begin FBlueprintHelperScopedAssetMutation per affected asset
5. Restore graphs if cleanup removed empty graph / owned graph
6. Restore nodes from rollback_data
7. Restore pins/defaults/node properties
8. Restore links
9. Restore BlueprintHelper ownership metadata + NodeComment
10. MarkBlueprintAsStructurallyModified
11. MarkPackageDirty
12. Generate new rollback write transaction_id
13. Write rollback transaction Journal
14. Update original cleanup transaction rollback_status=succeeded
15. Write Review Store entry for rollback operation if required by policy
16. Return success result
```

### 10.1 节点恢复

恢复节点应使用 Journal 中的 `rollback_data.node_snapshots`。

实现策略：

```text
优先使用 RawJson / LogicJson node snapshot 中的 UE node class 与 serialized properties。
若已有 RawJson import/replay 能恢复节点，则复用 ImportService 的低层 node reconstruction。
恢复后必须重新映射 old_node_ref / old_guid → new UEdGraphNode*。
```

注意：

```text
不要依赖旧 UEdGraphNode* 指针。
不要假设 deleted snapshot 在当前 asset 中仍存在对象。
```

### 10.2 连线恢复

恢复顺序：

```text
1. 先恢复所有节点。
2. 重建 pin 映射。
3. 对 rollback_data.link_snapshots 逐条恢复。
4. 使用 UEdGraphSchema_K2::TryCreateConnection。
5. 失败时整体回滚本次 rollback 写操作。
```

### 10.3 metadata 恢复

对恢复的 owned block 必须恢复：

```text
BlueprintHelperOwned
BlueprintHelperBlockId
BlueprintHelperTransactionId
BlueprintHelperTool
BlueprintHelperFeatureName
NodeComment
```

其中 `BlueprintHelperTransactionId` 应保留原 owned block 的创建 transaction，还是更新为 rollback transaction，需要在 Journal schema 中明确。

建议第一版：

```text
节点 ownership metadata 中的 BlueprintHelperTransactionId 保持原 block 创建 transaction。
rollback transaction 只记录在 Journal / Review 中。
```

原因：

```text
rollback 不是重新创建一个新 owned block，而是恢复被 cleanup 删除的原 block。
```

---

## 11. 冲突检测

正式执行前必须重新读取当前资产状态，不能直接信任 dry_run 结果。

需要阻断的冲突：

```text
1. 同名 graph 已被用户创建且非 rollback 目标。
2. 同 block_id / block_ref 已存在，且不是同一 cleanup transaction 的恢复对象。
3. 目标节点 GUID / fallback identity 已被其他节点占用。
4. 目标位置被用户或后续 transaction 修改。
5. 原 cleanup transaction 后已有 accepted transaction 依赖被删除 block 的缺失状态。
6. rollback_data 与当前资产 schema 不兼容。
7. 恢复链接会连接到不存在或类型不兼容的 Pin。
8. ownership metadata 无法恢复。
```

出现冲突时：

```text
dry_run blocked 或正式失败
modified=false
rollback_result=not_needed
```

如果冲突发生在已经开始恢复之后，且本次恢复无法完全撤销：

```text
modified=true
error.rollback_result=blocked 或 failed
Agent 必须 stop_and_report
```

---

## 12. 正式成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "rollback_cleanup_transaction",
  "status": "applied",
  "modified": true,
  "target": {
    "transaction_id": "tx_20260503_1704",
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "rollback_scope": "cleanup_transaction"
  },
  "data": {
    "schema": "RollbackCleanupTransaction.v1",
    "rollback_result": {
      "rolled_back_transaction_id": "tx_20260503_1704",
      "rollback_status": "succeeded"
    },
    "write_ref": {
      "transaction_id": "tx_20260503_1804",
      "journal_recorded": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

不得返回：

```text
restored_nodes
restored_links
full snapshot
rollback_data
source_operation
review
safety
diagnostics
next
```

---

## 13. 正式失败返回

### 13.1 validate_transaction 失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "rollback_cleanup_transaction",
  "status": "failed",
  "modified": false,
  "target": {
    "transaction_id": "tx_20260503_1704",
    "rollback_scope": "cleanup_transaction"
  },
  "error": {
    "code": "transaction_type_mismatch",
    "stage": "validate_transaction",
    "message": "RollbackCleanupTransaction can only rollback cleanup transactions.",
    "retryable": false,
    "rollback_result": "not_needed",
    "conflicts": [
      {
        "code": "expected_cleanup_transaction",
        "actual_operation": "replace_blueprint_graph"
      }
    ]
  }
}
```

### 13.2 rollback 写操作失败但撤销成功

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "rollback_cleanup_transaction",
  "status": "failed",
  "modified": false,
  "target": {
    "transaction_id": "tx_20260503_1704",
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "rollback_scope": "cleanup_transaction"
  },
  "error": {
    "code": "restore_link_failed",
    "stage": "restore_links",
    "message": "A link in the cleanup rollback data could not be restored.",
    "retryable": false,
    "rollback_result": "rolled_back",
    "failed_item": {
      "type": "link",
      "ref": "rollback_links[4]"
    }
  }
}
```

这里的 `rollback_result=rolled_back` 表示：

```text
本次 rollback 写操作已经撤销自身的部分恢复动作。
不表示目标 cleanup transaction 已恢复成功。
```

### 13.3 rollback 写操作自身 blocked / failed

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "rollback_cleanup_transaction",
  "status": "failed",
  "modified": true,
  "target": {
    "transaction_id": "tx_20260503_1704",
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "rollback_scope": "cleanup_transaction"
  },
  "error": {
    "code": "rollback_failed",
    "stage": "rollback",
    "message": "RollbackCleanupTransaction failed and the rollback operation itself could not be fully reverted.",
    "retryable": false,
    "rollback_result": "failed",
    "conflicts": [
      {
        "code": "asset_state_changed_during_rollback",
        "target": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
      }
    ]
  }
}
```

规则：

```text
modified=true 时，Agent 必须 stop_and_report。
不得继续 compile / save / patch / merge / replace / cleanup。
```

---

## 14. Journal 写入

正式成功时应写两类状态：

### 14.1 新 rollback transaction

```json
{
  "schema": "BlueprintHelper.TransactionJournal.v1",
  "transaction_id": "tx_20260503_1804",
  "operation": "rollback_cleanup_transaction",
  "status": "applied",
  "target_assets": [
    "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  ],
  "rollback": {
    "rolled_back_transaction_id": "tx_20260503_1704",
    "rollback_scope": "cleanup_transaction",
    "rollback_status": "succeeded"
  },
  "restored_refs": {
    "blocks": [
      "EG_PhysicsDoor_TogglePhysicsDoor0"
    ]
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

### 14.2 原 cleanup transaction 状态更新

```json
{
  "transaction_id": "tx_20260503_1704",
  "rollback_status": "succeeded",
  "rolled_back_by_transaction_id": "tx_20260503_1804",
  "storage_status": "active"
}
```

不得覆盖原 cleanup transaction 的 rollback_data，除非后续 compact 策略允许。

---

## 15. Bridge Router 接入

### 15.1 command 注册

新增：

```text
rollback_cleanup_transaction
```

### 15.2 写权限

这是写工具，必须进入 Token / write_permission 校验集合。

即使 `dry_run=true` 不修改资产，也建议走只读授权路径；正式执行必须写权限。

如果当前框架只按 command 识别写命令：

```text
rollback_cleanup_transaction
```

应作为写命令注册。

### 15.3 Router 处理

```cpp
if (Request.Command == TEXT("rollback_cleanup_transaction"))
{
    return HandleRollbackCleanupTransaction(Request);
}
```

### 15.4 Validator

payload 最小字段：

```json
{
  "transaction_id": "tx_20260503_1704",
  "rollback_scope": "cleanup_transaction",
  "dry_run": true,
  "already_rolled_back_policy": "error"
}
```

校验规则：

```text
transaction_id 必填
rollback_scope 必填，且只能是 cleanup_transaction
dry_run 可选，但正式执行前必须通过工具内部 preflight
already_rolled_back_policy 可选，默认 error
```

---

## 16. 自动化测试计划

新增测试文件：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperRollbackCleanupContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperRollbackCleanupDryRunTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperRollbackCleanupExecutionTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperRollbackCleanupFailureTests.cpp
```

### 16.1 Contract 测试

```text
1. operation 固定为 rollback_cleanup_transaction。
2. target 只返回 transaction_id / asset_path / rollback_scope。
3. dry_run schema = RollbackCleanupTransactionDryRun.v1。
4. success schema = RollbackCleanupTransaction.v1。
5. rollback_summary 不包含 transaction_id / source_operation。
6. success 区分 rolled_back_transaction_id 与 write_ref.transaction_id。
7. no_op 不返回 write_ref。
8. failed 不返回 rollback_result / write_ref。
```

### 16.2 dry_run 测试

```text
1. cleanup transaction + complete rollback_data → passed。
2. transaction not found → blocked / failed according to command mode。
3. non-cleanup transaction → blocked。
4. rollback_data missing → blocked。
5. rollback_data compacted → blocked。
6. already rolled back + policy=error → blocked。
7. already rolled back + policy=ignore → no_op path。
8. asset state conflict → blocked。
```

### 16.3 execution 测试

```text
1. rollback CleanupBlueprintHelperBlock restores nodes。
2. rollback restores links。
3. rollback restores metadata and NodeComment。
4. rollback writes new transaction Journal。
5. rollback updates original cleanup transaction rollback_status。
6. validation.should_compile=true / should_save=true。
```

### 16.4 failure / recovery 测试

```text
1. restore_node_failed rolls back this rollback operation。
2. restore_link_failed rolls back this rollback operation。
3. journal_write_failed rolls back this rollback operation。
4. rollback operation rollback failed → modified=true。
5. modified=true failure forces stop_and_report condition。
```

---

## 17. 推荐提交顺序

### Commit 1：类型与序列化

```text
Add RollbackCleanupTransaction result and dry_run types
Add rollback_summary serialization
Add rollback_scope and already_rolled_back_policy enums
```

验收：

```text
能输出 dry_run passed / blocked / success / no_op / failed 的契约 JSON。
```

### Commit 2：Journal 查询与 transaction 类型校验

```text
Add TransactionJournal lookup by transaction_id
Validate cleanup transaction type
Validate rollback_data availability
Validate already rolled back status
```

验收：

```text
非 cleanup transaction 返回 transaction_type_mismatch。
rollback_data unavailable 返回 rollback_data_unavailable。
```

### Commit 3：dry_run preflight

```text
Implement RollbackCleanupTransaction dry_run
Check affected assets
Check restorable blocks
Check rollback_data and asset state
Return rollback_summary
```

验收：

```text
dry_run 不写资产。
dry_run 不返回 rollback_data / snapshots / full diff。
```

### Commit 4：恢复执行器

```text
Restore graphs / nodes / links from cleanup rollback_data
Restore metadata and NodeComment
Use ScopedAssetMutation
Rollback this rollback operation on failure
```

验收：

```text
成功恢复 cleanup 删除的 owned block。
失败不残留半恢复状态。
```

### Commit 5：Journal / Review 更新

```text
Write new rollback transaction Journal
Update original cleanup transaction rollback_status
Record rolled_back_by_transaction_id
Return write_ref
```

验收：

```text
rolled_back_transaction_id != write_ref.transaction_id。
Journal 写失败不能报告成功。
```

### Commit 6：Bridge 接入与测试

```text
Register rollback_cleanup_transaction command
Add request validation
Add write permission gate
Add automation tests
```

验收：

```text
正式执行需要写权限。
dry_run 与正式执行字段契约通过快照测试。
```

---

## 18. 第一版不做内容

```text
1. 不做通用 RollbackTransaction。
2. 不回滚 Append / Replace / Patch / Merge。
3. 不回滚资产创建。
4. 不回滚 Component / Class Settings 普通写操作。
5. 不从 compacted summary 恢复。
6. 不返回 node snapshot / full diff。
7. 不自动级联回滚后续 transaction。
8. 不在 Agent-facing 结果中返回 Review / safety / diagnostics。
9. 不把 already_rolled_back 当成功恢复。
10. 不允许绕过 dry_run。
```

---

## 19. 最小验收标准

最终正式成功必须符合：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "rollback_cleanup_transaction",
  "status": "applied",
  "modified": true,
  "target": {
    "transaction_id": "tx_20260503_1704",
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "rollback_scope": "cleanup_transaction"
  },
  "data": {
    "schema": "RollbackCleanupTransaction.v1",
    "rollback_result": {
      "rolled_back_transaction_id": "tx_20260503_1704",
      "rollback_status": "succeeded"
    },
    "write_ref": {
      "transaction_id": "tx_20260503_1804",
      "journal_recorded": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

并且不得包含：

```text
source_operation
rollback_data
node snapshot
restored_nodes
restored_links
full diff
review
safety
diagnostics
next
```

---
