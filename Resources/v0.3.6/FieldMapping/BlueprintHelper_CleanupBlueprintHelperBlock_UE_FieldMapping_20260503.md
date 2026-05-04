# BlueprintHelper CleanupBlueprintHelperBlock UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：CleanupBlueprintHelperBlock 字段确认稿  
本文边界：确认 CleanupBlueprintHelperBlock 的 Agent-facing 返回字段、UE 侧结构体映射、cleaned_ref、missing_policy、dry_run、正式失败、write_ref 与 validation 规则。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

CleanupBlueprintHelperBlock 采用成功极简返回规则：

```text
1. operation 固定为 cleanup_blueprint_helper_block。
2. 顶层 target 只保留 asset_path / graph / cleanup_scope / block_ref，不返回 target_type。
3. 输入层允许完整 block_id，但成功返回优先 graph_id + block_ref。
4. 成功返回使用 data.cleanup_result.cleaned_ref。
5. 成功返回不返回 deleted_nodes / deleted_links / summary。
6. 成功返回 write_ref.transaction_id / journal_recorded。
7. missing_policy 支持 error / ignore，默认 error。
8. missing_policy=ignore 且目标缺失时返回 status=no_op / modified=false / missing=true。
9. 明确 block cleanup 的 dry_run passed 极简。
10. dry_run blocked 返回 blocked_by / conflicts / errors。
11. 正式失败不返回 cleanup_result，但 error 必须完整。
12. ownership 冲突必须失败，不能清理非 owned 内容。
13. rollback blocked / failed 时允许 modified=true，并要求 Agent stop_and_report。
```

---

## 1. 工具定位

`CleanupBlueprintHelperBlock` 只负责：

```text
删除一个明确的 BlueprintHelper-owned block。
```

它只接受明确目标：

```text
block_id
```

或者压缩引用：

```text
asset_path + graph_id + block_ref
```

不接受：

```text
entry_name
display_name
custom_event name
模糊名称
按图表批量删除
按功能名批量删除
```

这些属于：

```text
CleanupBlueprintHelperFeature
```

---

## 2. ToolResultBase 约束

正式成功允许返回：

```text
ok
schema
operation
trace_id
status
modified
target
data.cleanup_result.cleaned_ref
data.write_ref
validation
```

默认不返回：

```text
target_type
deleted_nodes
deleted_links
deleted_node_count
deleted_link_count
external_dependencies
external_dependents
ownership
review
safety
diagnostics
next
rollback_data
journal_path
```

---

## 3. operation

固定使用：

```json
"operation": "cleanup_blueprint_helper_block"
```

---

## 4. data.schema

正式成功 / no_op：

```json
"schema": "CleanupBlueprintHelperBlock.v1"
```

dry_run：

```json
"schema": "CleanupBlueprintHelperBlockDryRun.v1"
```

失败：

```text
不返回 data.cleanup_result。
失败原因只返回 error。
```

---

# 5. 顶层 target

顶层 `target` 只表达执行路由范围。

建议结构：

```json
"target": {
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "graph": "EG_PhysicsDoor",
  "cleanup_scope": "block",
  "block_ref": "TogglePhysicsDoor0"
}
```

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | 目标蓝图资产路径。 |
| `GraphName` | `FString` | `target.graph` | `string` | 推荐 | 目标 block 所在图表。 |
| `CleanupScope` | `EBlueprintHelperCleanupScope` | `target.cleanup_scope` | `string enum` | 是 | 固定为 `block`。 |
| `BlockRef` | `FString` | `target.block_ref` | `string` | 推荐 | 目标 block 局部引用。 |
| `BlockId` | `FString` | `target.block_id` | `string` | 输入兼容 | 完整 block_id，可作为输入目标。 |

Agent-facing 成功返回优先使用：

```text
graph_id + block_ref
```

输入层可接受完整：

```text
block_id
```

不返回：

```text
target.target_type
```

---

# 6. 正式成功返回

## 6.1 JSON 示例

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "cleanup_blueprint_helper_block",
  "trace_id": "trace_20260503_1601",
  "status": "applied",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "cleanup_scope": "block",
    "block_ref": "TogglePhysicsDoor0"
  },

  "data": {
    "schema": "CleanupBlueprintHelperBlock.v1",

    "cleanup_result": {
      "cleaned_ref": {
        "graph_id": "EG_PhysicsDoor",
        "block_ref": "TogglePhysicsDoor0"
      }
    },

    "write_ref": {
      "transaction_id": "tx_20260503_1601",
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

---

## 6.2 data.cleanup_result.cleaned_ref

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `GraphId` | `FString` | `data.cleanup_result.cleaned_ref.graph_id` | `string` | 推荐 | 目标 block 所在图表 ID。 |
| `BlockRef` | `FString` | `data.cleanup_result.cleaned_ref.block_ref` | `string` | 推荐 | 被清理 block 的局部引用。 |
| `BlockId` | `FString` | `data.cleanup_result.cleaned_ref.block_id` | `string` | fallback | 无法安全拆分时返回完整 block_id。 |

完整 block_id 反推：

```text
full_block_id = graph_id + "_" + block_ref
```

默认建议拆为：

```text
graph_id + block_ref
```

---

## 6.3 data.write_ref

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `TransactionId` | `FString` | `data.write_ref.transaction_id` | `string` | 是 | 本次正式写工具调用的 transaction ID。 |
| `bJournalRecorded` | `bool` | `data.write_ref.journal_recorded` | `boolean` | 是 | Journal 是否已成功记录。 |

---

# 7. missing_policy

支持：

```text
error
ignore
```

默认：

```text
error
```

| policy | 行为 |
|---|---|
| `error` | block 不存在时报错。 |
| `ignore` | block 不存在时返回 no_op。适合恢复、批处理、重复清理。 |

---

## 7.1 missing_policy=ignore 的 no_op 返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "cleanup_blueprint_helper_block",
  "trace_id": "trace_20260503_1602",
  "status": "no_op",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "cleanup_scope": "block",
    "block_ref": "TogglePhysicsDoor0"
  },

  "data": {
    "schema": "CleanupBlueprintHelperBlock.v1",

    "cleanup_result": {
      "cleaned_ref": {
        "graph_id": "EG_PhysicsDoor",
        "block_ref": "TogglePhysicsDoor0"
      },
      "missing_policy": "ignore",
      "missing": true
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

no_op 可返回：

```text
missing_policy
missing=true
```

用于说明为什么没有修改。

---

# 8. dry_run 返回

## 8.1 dry_run passed

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "cleanup_blueprint_helper_block",
  "trace_id": "trace_20260503_1603",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "cleanup_scope": "block",
    "block_ref": "TogglePhysicsDoor0"
  },

  "data": {
    "schema": "CleanupBlueprintHelperBlockDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

## 8.2 dry_run blocked

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "cleanup_blueprint_helper_block",
  "trace_id": "trace_20260503_1604",
  "status": "dry_run",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "cleanup_scope": "block",
    "block_ref": "TogglePhysicsDoor0"
  },

  "data": {
    "schema": "CleanupBlueprintHelperBlockDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "external_dependents_exist"
      ],
      "conflicts": [
        {
          "code": "external_dependents_exist",
          "message": "The target block is referenced by external logic."
        }
      ],
      "errors": []
    }
  }
}
```

dry_run 不返回：

```text
would_delete_nodes
would_delete_links
will_keep
plan
ownership
review
safety
diagnostics
next
```

---

# 9. 正式失败返回

正式失败不返回：

```text
data.cleanup_result
data.write_ref
review
safety
diagnostics
next
```

但 `error` 必须完整。

## 9.1 block 不存在

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "cleanup_blueprint_helper_block",
  "trace_id": "trace_20260503_1605",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "cleanup_scope": "block",
    "block_ref": "TogglePhysicsDoor0"
  },

  "error": {
    "code": "block_not_found",
    "stage": "resolve_target",
    "message": "The requested BlueprintHelper-owned block was not found.",
    "retryable": false,
    "rollback_result": "not_needed"
  }
}
```

## 9.2 ownership 冲突

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "cleanup_blueprint_helper_block",
  "trace_id": "trace_20260503_1606",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "cleanup_scope": "block",
    "block_ref": "TogglePhysicsDoor0"
  },

  "error": {
    "code": "target_not_owned",
    "stage": "ownership_check",
    "message": "CleanupBlueprintHelperBlock can only delete BlueprintHelper-owned blocks.",
    "retryable": false,
    "rollback_result": "not_needed",
    "conflicts": [
      {
        "code": "ownership_mismatch",
        "target": "EG_PhysicsDoor_TogglePhysicsDoor0"
      }
    ]
  }
}
```

## 9.3 写入中失败并回滚

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "cleanup_blueprint_helper_block",
  "trace_id": "trace_20260503_1607",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "cleanup_scope": "block",
    "block_ref": "TogglePhysicsDoor0"
  },

  "error": {
    "code": "node_delete_failed",
    "stage": "delete_nodes",
    "message": "A node in the target block could not be deleted.",
    "retryable": false,
    "rollback_result": "rolled_back",
    "failed_item": {
      "type": "node",
      "ref": "node_ref[3]"
    }
  }
}
```

## 9.4 rollback blocked / failed

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "cleanup_blueprint_helper_block",
  "trace_id": "trace_20260503_1608",
  "status": "failed",
  "modified": true,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "cleanup_scope": "block",
    "block_ref": "TogglePhysicsDoor0"
  },

  "error": {
    "code": "rollback_failed",
    "stage": "rollback",
    "message": "CleanupBlueprintHelperBlock failed and rollback could not restore the target block.",
    "retryable": false,
    "rollback_result": "failed",
    "conflicts": [
      {
        "code": "asset_state_changed_during_write",
        "target": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
      }
    ]
  }
}
```

---

# 10. error 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperCleanupErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperCleanupStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |
| `RollbackResult` | `EBlueprintHelperRollbackResult` | `error.rollback_result` | `string enum` | 是 | 回滚结果。 |
| `FailedItem` | `FBlueprintHelperFailedItem` | `error.failed_item` | `object` | 可选 | 失败对象摘要。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表，仅失败定位必要时返回。 |

---

# 11. validation

成功通常返回：

```json
{
  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

no_op 且 modified=false 时：

```json
{
  "validation": {
    "should_compile": false,
    "should_save": false,
    "compiled": false,
    "saved": false
  }
}
```

---

# 12. UE 侧建议结构体

```cpp
struct FBlueprintHelperCleanupBlockResultData
{
    FString Schema; // CleanupBlueprintHelperBlock.v1
    FBlueprintHelperCleanupBlockResult CleanupResult;
    FBlueprintHelperWriteRef WriteRef;
};

struct FBlueprintHelperCleanupBlockResult
{
    FBlueprintHelperCleanedRef CleanedRef;
    FString MissingPolicy; // optional for no_op
    bool bMissing = false; // optional for no_op
};

struct FBlueprintHelperCleanedRef
{
    FString GraphId;
    FString BlockRef;
    FString BlockId; // fallback only when graph_id + block_ref cannot be safely derived
};

struct FBlueprintHelperWriteRef
{
    FString TransactionId;
    bool bJournalRecorded;
};
```

---

# 13. 验收标准

```text
1. operation 固定为 cleanup_blueprint_helper_block。
2. data.schema 固定为 CleanupBlueprintHelperBlock.v1。
3. dry_run schema 固定为 CleanupBlueprintHelperBlockDryRun.v1。
4. 顶层 target 不返回 target_type。
5. 成功返回 data.cleanup_result.cleaned_ref。
6. cleaned_ref 优先返回 graph_id + block_ref。
7. 输入层允许完整 block_id。
8. 成功不返回 deleted_nodes / deleted_links / summary。
9. 成功返回 data.write_ref。
10. missing_policy 支持 error / ignore，默认 error。
11. missing_policy=ignore 且目标缺失时返回 no_op / missing=true。
12. ownership 冲突必须失败。
13. dry_run passed 极简。
14. dry_run blocked 返回 blocked_by / conflicts / errors。
15. 正式失败不返回 cleanup_result，但 error 保留诊断信息。
16. rollback blocked / failed 时允许 modified=true，并要求 Agent stop_and_report。
