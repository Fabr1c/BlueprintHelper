# BlueprintHelper SaveAsset UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：save_asset 字段确认稿  
本文边界：确认 save_asset 的 Agent-facing 返回字段、UE 侧结构体映射、保存成功、no_op、保存失败规则。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

save_asset 采用以下字段口径：

```text
1. operation 固定为 save_asset。
2. save 成功返回 save_result.saved / was_dirty。
3. save no_op 返回 status=no_op / reason=asset_not_dirty。
4. save 失败返回 error。
5. save 不返回 validation。
6. save 不返回 write_ref / transaction_id / review / safety。
7. save 默认 modified=false。
```

---

## 1. 工具定位

`save_asset` 负责：

```text
保存一个明确资产。
```

它不负责：

```text
编译
修复
Graph Write
Component 写入
Class Settings 写入
Journal / Review
生成 transaction_id
```

---

## 2. ToolResultBase 约束

save_asset 使用 ToolResultBase 外壳。

成功执行时允许返回：

```text
ok
schema
operation
trace_id
status
modified
target
data.save_result
```

不返回：

```text
validation
write_ref
transaction_id
journal_recorded
review
safety
rollback_data
journal_path
```

---

## 3. operation

固定使用：

```json
"operation": "save_asset"
```

---

## 4. data.schema

```json
"schema": "SaveAsset.v1"
```

---

## 5. target 字段

```json
"target": {
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
}
```

字段映射：

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | 要保存的资产路径。 |

---

# 6. 保存成功

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "save_asset",
  "trace_id": "trace_20260503_2101",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  },

  "data": {
    "schema": "SaveAsset.v1",
    "save_result": {
      "saved": true,
      "was_dirty": true
    }
  }
}
```

说明：

```text
modified=false：save 本身不代表 Agent 修改资产内容。
saved=true：资产已保存。
was_dirty=true：保存前资产有未保存修改。
```

---

# 7. no_op：资产本来不 dirty

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "save_asset",
  "trace_id": "trace_20260503_2102",
  "status": "no_op",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  },

  "data": {
    "schema": "SaveAsset.v1",
    "save_result": {
      "saved": false,
      "was_dirty": false,
      "reason": "asset_not_dirty"
    }
  }
}
```

---

# 8. 保存失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "save_asset",
  "trace_id": "trace_20260503_2103",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  },

  "error": {
    "code": "save_failed",
    "stage": "save_package",
    "message": "The asset package could not be saved.",
    "retryable": true,
    "conflicts": [
      {
        "code": "file_locked",
        "message": "The asset file appears to be locked by another process."
      }
    ]
  }
}
```

保存失败不返回：

```text
rollback_result
validation
write_ref
transaction_id
```

---

## 8.1 save_result 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bSaved` | `bool` | `data.save_result.saved` | `boolean` | 是 | 本次是否实际保存。 |
| `bWasDirty` | `bool` | `data.save_result.was_dirty` | `boolean` | 是 | 保存前资产是否 dirty。 |
| `Reason` | `FString` | `data.save_result.reason` | `string` | no_op 时 | 例如 `asset_not_dirty`。 |

---

# 9. error 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperSaveErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperSaveStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表。 |

save 失败不强制返回：

```text
rollback_result
```

因为 save 不是 BlueprintHelper 写事务。

---

# 10. UE 侧建议结构体

```cpp
struct FBlueprintHelperSaveAssetResultData
{
    FString Schema; // SaveAsset.v1
    FBlueprintHelperSaveResult SaveResult;
};

struct FBlueprintHelperSaveResult
{
    bool bSaved = false;
    bool bWasDirty = false;
    FString Reason; // optional for no_op
};
```

不包含：

```cpp
FBlueprintHelperValidationResult Validation;
FBlueprintHelperWriteRef WriteRef;
FString TransactionId;
```

---

# 11. 验收标准

```text
1. operation 固定为 save_asset。
2. data.schema 固定为 SaveAsset.v1。
3. save 成功返回 save_result.saved / was_dirty。
4. save no_op 返回 status=no_op / reason=asset_not_dirty。
5. save 失败返回 error。
6. save 不返回 validation。
7. save 不返回 write_ref / transaction_id / review / safety。
8. save 默认 modified=false。
```
