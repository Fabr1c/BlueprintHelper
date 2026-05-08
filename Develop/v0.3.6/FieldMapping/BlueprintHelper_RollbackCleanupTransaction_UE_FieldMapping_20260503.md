# BlueprintHelper RollbackCleanupTransaction UE 字段映射计划

日期：2026-05-03  
状态：字段确认稿  
范围：RollbackCleanupTransaction 的 Agent-facing 返回字段、UE 侧结构体映射、dry_run、成功、no_op、失败、write_ref 与 validation。

---

## 0. 字段结论

```text
1. operation=rollback_cleanup_transaction。
2. target 只保留 transaction_id / asset_path / rollback_scope，不返回 target_type。
3. 只回滚 cleanup transaction，不做通用 rollback。
4. rollback_scope=cleanup_transaction。
5. 必须 dry_run。
6. dry_run 返回 rollback_summary / blocked_by / conflicts / errors。
7. rollback_summary 不包含 transaction_id / source_operation。
8. dry_run 不返回 rollback_data / node snapshot / full diff。
9. 正式成功返回 rollback_result / write_ref / validation。
10. rollback_result 区分 rolled_back_transaction_id 与本次 write_ref.transaction_id。
11. 成功不返回 restored_nodes / restored_links / full snapshot。
12. already_rolled_back_policy 支持 error / ignore，默认 error。
13. ignore 时返回 no_op / rollback_status=already_rolled_back。
14. 正式失败不返回 rollback_result，但 error 必须完整。
15. rollback 写操作自身 blocked / failed 时允许 modified=true，并要求 Agent stop_and_report。
```

---

## 1. 工具定位

`RollbackCleanupTransaction` 只回滚 Cleanup 工具产生的 transaction。第一版只适用于：

```text
CleanupBlueprintHelperBlock
CleanupBlueprintHelperFeature
```

不适用于任意 Graph Write 回滚、资产创建回滚、普通 Component / Class Settings 回滚、无 rollback_data 的 transaction、已经 compacted 的 transaction。通用回滚后续应另设 `RollbackTransaction`。

---

## 2. operation / schema

```json
{
  "operation": "rollback_cleanup_transaction"
}
```

dry_run：

```json
"schema": "RollbackCleanupTransactionDryRun.v1"
```

正式成功 / no_op：

```json
"schema": "RollbackCleanupTransaction.v1"
```

失败时不返回 `data.rollback_result`，只返回 `error`。

---

## 3. target 字段

```json
"target": {
  "transaction_id": "tx_20260503_1704",
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "rollback_scope": "cleanup_transaction"
}
```

| UE 字段 | UE 类型建议 | JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `TransactionId` | `FString` | `target.transaction_id` | string | 是 | 被回滚的 cleanup transaction ID。 |
| `AssetPath` | `FString` | `target.asset_path` | string | 推荐 | 涉及资产路径，可从 Journal 推导。 |
| `RollbackScope` | `EBlueprintHelperRollbackScope` | `target.rollback_scope` | string enum | 是 | 固定为 `cleanup_transaction`。 |

不返回 `target_type / target_kind / block_ref / feature_name`。rollback 的事实来源是 `transaction_id + Journal`。

---

## 4. dry_run 字段

Rollback 是高风险写操作，必须 dry_run。dry_run 检查 transaction 是否存在、是否属于 cleanup、rollback_data 是否完整、是否已回滚、当前资产状态是否仍可恢复、是否存在用户或后续 transaction 修改冲突。

### 4.1 dry_run passed

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "rollback_cleanup_transaction",
  "status": "dry_run",
  "modified": false,
  "target": {
    "transaction_id": "tx_20260503_1704",
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "rollback_scope": "cleanup_transaction"
  },
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

### 4.2 dry_run blocked

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "rollback_cleanup_transaction",
  "status": "dry_run",
  "modified": false,
  "target": {
    "transaction_id": "tx_20260503_1704",
    "rollback_scope": "cleanup_transaction"
  },
  "data": {
    "schema": "RollbackCleanupTransactionDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "rollback_summary": {
        "rollback_data_available": false
      },
      "blocked_by": ["rollback_data_unavailable"],
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

### 4.3 rollback_summary 映射

| UE 字段 | UE 类型建议 | JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AffectedAssets` | `int32` | `data.dry_run.rollback_summary.affected_assets` | number | 可选 | 受影响资产数量。 |
| `RestorableBlocks` | `int32` | `data.dry_run.rollback_summary.restorable_blocks` | number | 可选 | 可恢复 owned block 数量。 |
| `bRestorableNodesAvailable` | `bool` | `data.dry_run.rollback_summary.restorable_nodes_available` | boolean | 可选 | 节点恢复数据是否可用。 |
| `bRollbackDataAvailable` | `bool` | `data.dry_run.rollback_summary.rollback_data_available` | boolean | 是 | rollback_data 是否可用。 |
| `bRestorableLinksAvailable` | `bool` | `data.dry_run.rollback_summary.restorable_links_available` | boolean | 可选 | 连线恢复数据是否可用。 |
| `bAssetStateChecked` | `bool` | `data.dry_run.rollback_summary.asset_state_checked` | boolean | 可选 | 是否已检查当前资产状态。 |

明确不返回 `transaction_id / source_operation / rollback_data / node snapshots / full diff`。

---

## 5. 正式成功字段

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

`rolled_back_transaction_id` 是被回滚的 cleanup transaction；`write_ref.transaction_id` 是本次 rollback 写操作自己的 transaction。

---

## 6. no_op 字段

`already_rolled_back_policy = error | ignore`，默认 `error`。`ignore` 时返回：

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

---

## 7. 正式失败字段

正式失败不返回 `rollback_result / write_ref / review / safety / diagnostics / next`，但 `error` 必须完整。

最小 error：

```json
{
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
```

`rollback_result=rolled_back` 表示本次 rollback 写操作失败后，已撤销本次 rollback 的部分恢复动作，不表示目标 cleanup transaction 已经成功恢复。`rollback_result=blocked/failed` 且 `modified=true` 时，Agent 必须 stop_and_report。

---

## 8. UE 侧建议结构体

```cpp
struct FBlueprintHelperRollbackCleanupResultData
{
    FString Schema; // RollbackCleanupTransaction.v1
    FBlueprintHelperRollbackCleanupResult RollbackResult;
    FBlueprintHelperWriteRef WriteRef;
};

struct FBlueprintHelperRollbackCleanupDryRunData
{
    FString Schema; // RollbackCleanupTransactionDryRun.v1
    FBlueprintHelperRollbackDryRunResult DryRun;
};

struct FBlueprintHelperRollbackDryRunResult
{
    FString Result; // passed | blocked
    bool bCanExecute;
    FBlueprintHelperRollbackSummary RollbackSummary;
    TArray<FString> BlockedBy;
    TArray<FBlueprintHelperConflictItem> Conflicts;
    TArray<FBlueprintHelperDryRunIssue> Errors;
};

struct FBlueprintHelperRollbackSummary
{
    int32 AffectedAssets = 0;
    int32 RestorableBlocks = 0;
    bool bRestorableNodesAvailable = false;
    bool bRollbackDataAvailable = false;
    bool bRestorableLinksAvailable = false;
    bool bAssetStateChecked = false;
};

struct FBlueprintHelperRollbackCleanupResult
{
    FString RolledBackTransactionId;
    FString RollbackStatus; // succeeded | already_rolled_back
    FString AlreadyRolledBackPolicy; // optional for no_op
};

struct FBlueprintHelperWriteRef
{
    FString TransactionId;
    bool bJournalRecorded;
};
```

---

## 9. 验收标准

```text
1. operation 固定为 rollback_cleanup_transaction。
2. target 只返回 transaction_id / asset_path / rollback_scope。
3. RollbackCleanupTransaction 只回滚 cleanup transaction。
4. rollback 必须 dry_run。
5. dry_run 返回 rollback_summary / blocked_by / conflicts / errors。
6. rollback_summary 不包含 transaction_id / source_operation。
7. dry_run 不返回 rollback_data / node snapshot / full diff。
8. 正式成功返回 data.rollback_result / write_ref / validation。
9. rollback_result 区分 rolled_back_transaction_id 与 write_ref.transaction_id。
10. already_rolled_back_policy 支持 error / ignore。
11. 正式失败不返回 rollback_result，但 error 保留完整诊断。
12. rollback 写操作自身 blocked / failed 时允许 modified=true，并要求 Agent stop_and_report。
```
