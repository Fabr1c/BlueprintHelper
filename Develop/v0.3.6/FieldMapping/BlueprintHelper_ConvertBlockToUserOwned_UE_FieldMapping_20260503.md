# BlueprintHelper ConvertBlueprintHelperBlockToUserOwned UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：ConvertBlueprintHelperBlockToUserOwned 字段确认稿  
本文边界：确认 ConvertBlueprintHelperBlockToUserOwned 的 Agent-facing 返回字段、UE 侧结构体映射、dry_run、正式成功、no_op、正式失败、validation 规则，以及“成功结果不返回 write_ref / transaction_id”的修正。

---

## 0. 本次同步结论

```text
1. operation 固定为 convert_blueprint_helper_block_to_user_owned。
2. 顶层 target 只保留 asset_path / graph / ownership_scope。
3. target 不默认返回 block_ref / block_id / target_type。
4. 输入层允许完整 block_id 或 graph_id + block_ref，但成功返回不回显。
5. ownership_scope 第一版只支持 block。
6. 必须 dry_run。
7. dry_run passed 极简：只返回 result / can_execute。
8. dry_run blocked 返回 blocked_by / conflicts / errors，冲突项中可包含 block_id / ref。
9. 正式成功返回 conversion_result.converted_count / validation。
10. 成功不返回 converted_ref / graph_id / block_ref / node list / metadata diff。
11. 成功结果不返回 write_ref / transaction_id / journal_recorded。
12. already_user_owned_policy 支持 error / ignore，默认 error。
13. already_user_owned_policy=ignore 时返回 no_op / converted_count=0 / conversion_status=already_user_owned。
14. 正式失败不返回 conversion_result，但 error 必须完整。
15. 只在 error.failed_item 或 conflicts 中返回错误 block_id / ref。
16. rollback blocked / failed 时允许 modified=true，并要求 Agent stop_and_report。
```

---

## 1. 工具定位

`ConvertBlueprintHelperBlockToUserOwned` 负责：

```text
把一个明确的 BlueprintHelper-owned block 转为 user-owned。
```

它修改的是：

```text
BlueprintHelper ownership metadata
NodeComment 中的 BlueprintHelper 管理标记
Journal / Review 中的 ownership 状态
```

它不做：

```text
删除节点
删除连线
修改业务逻辑
替换图表实现
Patch 节点 / Pin
Merge 执行流
Cleanup
Rollback
```

成功后，该 block 不再属于 BlueprintHelper 管理范围。

---

## 2. 成功结果不返回 write_ref 的修正
注意，不止该工具，还需要检查之前实现的 `Replace`、`Patch`、`Merge` 等工具，确认它们的成功结果也不返回 write_ref / transaction_id 等内部审计数据。
本工具的成功结果明确不返回：

```text
data.write_ref
data.write_ref.transaction_id
data.write_ref.journal_recorded
transaction
review
safety
```

原因：

```text
1. Agent 不需要知道事务 ID。
2. transaction_id / Journal / Review / rollback_data 属于 UE 侧内部审计系统。
3. Convert 成功后，目标 block 已经转为 user-owned，不应返回 managed handle。
4. 成功返回 transaction_id 会鼓励 Agent 把 UE 审计系统当成后续执行依赖。
5. 如果用户需要审查、回滚或排查事务，应通过 UE Review UI、Journal 查询工具、Debug 工具或用户明确请求的导出流程。
```

UE 插件内部仍应：

```text
生成 transaction_id
写 Transaction Journal
记录 before / after ownership diff
保存 rollback_data
进入 Review Store
```

但这些不默认暴露到 Agent-facing 成功返回体。

---

## 3. operation

固定使用：

```json
"operation": "convert_blueprint_helper_block_to_user_owned"
```

---

## 4. data.schema

dry_run：

```json
"schema": "ConvertBlueprintHelperBlockToUserOwnedDryRun.v1"
```

正式成功 / no_op：

```json
"schema": "ConvertBlueprintHelperBlockToUserOwned.v1"
```

失败：

```text
不返回 data.conversion_result。
失败原因只返回 error。
```

---

# 5. 顶层 target

顶层 `target` 只表达执行路由范围。

推荐结构：

```json
"target": {
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "graph": "EG_PhysicsDoor",
  "ownership_scope": "block"
}
```

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | 目标蓝图资产路径。 |
| `GraphName` | `FString` | `target.graph` | `string` | 推荐 | 目标 block 所在图表。 |
| `OwnershipScope` | `EBlueprintHelperOwnershipScope` | `target.ownership_scope` | `string enum` | 是 | 第一版固定为 `block`。 |

不默认返回：

```text
target.block_ref
target.block_id
target.target_type
target.target_kind
```

输入层可以接受：

```text
block_id = EG_PhysicsDoor_TogglePhysicsDoor0
```

或：

```text
graph_id + block_ref
```

但成功返回不回显这些 managed handle。

---

# 6. dry_run

本工具必须支持 dry_run。

原因：

```text
1. ownership 边界变化会影响后续 Cleanup / Replace / Patch 管理权限。
2. 转换后目标内容不再默认由 BlueprintHelper 管理。
3. 需要在写入前确认目标确实是 BlueprintHelper-owned block。
```

---

## 6.1 dry_run passed

dry_run passed 极简返回：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "convert_blueprint_helper_block_to_user_owned",
  "trace_id": "trace_20260503_1902",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "ownership_scope": "block"
  },
  "data": {
    "schema": "ConvertBlueprintHelperBlockToUserOwnedDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

dry_run passed 不返回：

```text
ownership_change_summary
current_owner
new_owner
journal_will_record_conversion
metadata diff
node list
full snapshot
```

---

## 6.2 dry_run blocked

blocked 时返回能定位问题的 block_id / ref。

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "convert_blueprint_helper_block_to_user_owned",
  "trace_id": "trace_20260503_1903",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "ownership_scope": "block"
  },
  "data": {
    "schema": "ConvertBlueprintHelperBlockToUserOwnedDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": ["target_not_owned"],
      "conflicts": [
        {
          "code": "target_not_owned",
          "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0",
          "message": "The target block is not BlueprintHelper-owned."
        }
      ],
      "errors": []
    }
  }
}
```

---

## 6.3 dry_run 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Result` | `EBlueprintHelperDryRunResult` | `data.dry_run.result` | `string enum` | 是 | `passed` 或 `blocked`。 |
| `bCanExecute` | `bool` | `data.dry_run.can_execute` | `boolean` | 是 | 是否可正式执行。 |
| `BlockedBy` | `TArray<FString>` | `data.dry_run.blocked_by` | `array<string>` | blocked 时 | 阻断 code 摘要。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `data.dry_run.conflicts` | `array<object>` | blocked 时 | 冲突列表。 |
| `Errors` | `TArray<FBlueprintHelperDryRunIssue>` | `data.dry_run.errors` | `array<object>` | blocked 时 | 错误列表。 |

---

# 7. 正式成功返回

正式成功只返回：

```text
conversion_result.converted_count
validation
```

不返回 write_ref。

示例：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "convert_blueprint_helper_block_to_user_owned",
  "trace_id": "trace_20260503_1901",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "ownership_scope": "block"
  },
  "data": {
    "schema": "ConvertBlueprintHelperBlockToUserOwned.v1",
    "conversion_result": {
      "converted_count": 1
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

成功不返回：

```text
converted_ref
graph_id
block_ref
block_id
converted_nodes
metadata_removed
comments_rewritten
metadata_before
metadata_after
write_ref
transaction_id
journal_recorded
review
safety
diagnostics
next
```

---

## 7.1 conversion_result 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `ConvertedCount` | `int32` | `data.conversion_result.converted_count` | `number` | 是 | 成功转换的 block 数量。 |
| `ConversionStatus` | `EBlueprintHelperConversionStatus` | `data.conversion_result.conversion_status` | `string enum` | no_op 时 | 例如 `already_user_owned`。 |
| `AlreadyUserOwnedPolicy` | `EBlueprintHelperAlreadyUserOwnedPolicy` | `data.conversion_result.already_user_owned_policy` | `string enum` | no_op 时 | `error` 或 `ignore`。 |

---

# 8. no_op

如果目标已经是 user-owned，并且调用允许幂等：

```text
already_user_owned_policy = error | ignore
```

默认：

```text
error
```

如果 `ignore`：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "convert_blueprint_helper_block_to_user_owned",
  "trace_id": "trace_20260503_1904",
  "status": "no_op",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "ownership_scope": "block"
  },
  "data": {
    "schema": "ConvertBlueprintHelperBlockToUserOwned.v1",
    "conversion_result": {
      "converted_count": 0,
      "conversion_status": "already_user_owned",
      "already_user_owned_policy": "ignore"
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

no_op 不返回：

```text
block_ref
block_id
write_ref
```

---

# 9. 正式失败

正式失败不返回：

```text
conversion_result
write_ref
review
safety
diagnostics
next
```

但 `error` 必须完整。

---

## 9.1 block 不存在

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "convert_blueprint_helper_block_to_user_owned",
  "trace_id": "trace_20260503_1905",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "ownership_scope": "block"
  },
  "error": {
    "code": "block_not_found",
    "stage": "resolve_target",
    "message": "The requested BlueprintHelper-owned block was not found.",
    "retryable": false,
    "rollback_result": "not_needed",
    "failed_item": {
      "type": "block",
      "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0"
    }
  }
}
```

---

## 9.2 target_not_owned

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "convert_blueprint_helper_block_to_user_owned",
  "trace_id": "trace_20260503_1906",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "ownership_scope": "block"
  },
  "error": {
    "code": "target_not_owned",
    "stage": "ownership_check",
    "message": "The target block is not BlueprintHelper-owned.",
    "retryable": false,
    "rollback_result": "not_needed",
    "failed_item": {
      "type": "block",
      "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0"
    }
  }
}
```

---

## 9.3 metadata 写入失败并回滚

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "convert_blueprint_helper_block_to_user_owned",
  "trace_id": "trace_20260503_1907",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "ownership_scope": "block"
  },
  "error": {
    "code": "ownership_metadata_write_failed",
    "stage": "write_metadata",
    "message": "Ownership metadata could not be converted to user-owned.",
    "retryable": false,
    "rollback_result": "rolled_back",
    "failed_item": {
      "type": "block",
      "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0"
    }
  }
}
```

---

## 9.4 rollback failed

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "convert_blueprint_helper_block_to_user_owned",
  "trace_id": "trace_20260503_1908",
  "status": "failed",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "ownership_scope": "block"
  },
  "error": {
    "code": "rollback_failed",
    "stage": "rollback",
    "message": "Ownership conversion failed and rollback could not restore the previous metadata state.",
    "retryable": false,
    "rollback_result": "failed",
    "conflicts": [
      {
        "code": "asset_state_changed_during_conversion",
        "target": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
      }
    ]
  }
}
```

此时：

```text
modified=true
Agent 必须 stop_and_report
不得继续 cleanup / patch / merge / replace / save
```

---

# 10. error 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperOwnershipErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperOwnershipStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |
| `RollbackResult` | `EBlueprintHelperRollbackResult` | `error.rollback_result` | `string enum` | 是 | 回滚结果。 |
| `FailedItem` | `FBlueprintHelperFailedItem` | `error.failed_item` | `object` | 可选 | 失败对象摘要。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表，仅失败定位必要时返回。 |

---

# 11. validation

正式成功通常返回：

```json
{
  "validation": {
    "should_compile": false,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

no_op 通常返回：

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

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bShouldCompile` | `bool` | `validation.should_compile` | `boolean` | 是 | 是否建议后续编译。Ownership metadata 转换通常不需要编译。 |
| `bShouldSave` | `bool` | `validation.should_save` | `boolean` | 是 | 是否建议后续保存。 |
| `bCompiled` | `bool` | `validation.compiled` | `boolean` | 是 | 本工具调用中是否已编译。 |
| `bSaved` | `bool` | `validation.saved` | `boolean` | 是 | 本工具调用中是否已保存。 |

---

# 12. UE 侧建议结构体

```cpp
struct FBlueprintHelperConvertBlockToUserOwnedResultData
{
    FString Schema; // ConvertBlueprintHelperBlockToUserOwned.v1
    FBlueprintHelperConvertBlockToUserOwnedResult ConversionResult;
};

struct FBlueprintHelperConvertBlockToUserOwnedDryRunData
{
    FString Schema; // ConvertBlueprintHelperBlockToUserOwnedDryRun.v1
    FBlueprintHelperDryRunResult DryRun;
};

struct FBlueprintHelperConvertBlockToUserOwnedResult
{
    int32 ConvertedCount = 0;
    FString ConversionStatus; // optional for no_op, e.g. already_user_owned
    FString AlreadyUserOwnedPolicy; // optional for no_op
};
```

明确不包含：

```cpp
FBlueprintHelperWriteRef WriteRef;
FString TransactionId;
FString GraphId;
FString BlockRef;
FString BlockId;
TArray<FString> ConvertedNodes;
int32 MetadataRemoved;
int32 CommentsRewritten;
```

UE 内部仍可在 Journal / Review / rollback_data 中保存这些审计数据。

---

# 13. 验收标准

```text
1. operation 固定为 convert_blueprint_helper_block_to_user_owned。
2. data.schema 固定为 ConvertBlueprintHelperBlockToUserOwned.v1。
3. dry_run schema 固定为 ConvertBlueprintHelperBlockToUserOwnedDryRun.v1。
4. target 只返回 asset_path / graph / ownership_scope。
5. target 不默认返回 block_ref / block_id / target_type。
6. 输入层允许 block_id 或 graph_id + block_ref。
7. ownership_scope 第一版只支持 block。
8. dry_run passed 只返回 result / can_execute。
9. dry_run blocked 返回 blocked_by / conflicts / errors。
10. 成功返回 conversion_result.converted_count。
11. 成功不返回 converted_ref / graph_id / block_ref / block_id。
12. 成功不返回 write_ref / transaction_id / journal_recorded。
13. already_user_owned_policy 支持 error / ignore，默认 error。
14. ignore 时返回 no_op / converted_count=0 / conversion_status=already_user_owned。
15. 正式失败不返回 conversion_result，但 error 保留完整诊断。
16. 错误定位信息只在 error.failed_item 或 conflicts 中返回 block_id / ref。
17. rollback blocked / failed 时允许 modified=true，并要求 Agent stop_and_report。
```
