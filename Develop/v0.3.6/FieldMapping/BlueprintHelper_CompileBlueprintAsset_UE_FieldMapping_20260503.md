# BlueprintHelper CompileBlueprintAsset UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：compile_blueprint_asset 字段确认稿  
本文边界：确认 compile_blueprint_asset 的 Agent-facing 返回字段、UE 侧结构体映射、编译成功、编译失败、工具自身失败规则。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

compile_blueprint_asset 采用以下字段口径：

```text
1. operation 固定为 compile_blueprint_asset。
2. compile 工具成功执行时使用 status=completed。
3. 蓝图是否编译通过由 data.compile_result.success 表达。
4. 编译 warning 不影响 compile_result.success。
5. 编译成功返回 warning_count，但不返回 error_count。
6. 编译失败时 ok=true / status=completed / compile_result.success=false。
7. 编译失败错误整合为 compile_result.markdown。
8. compile_result.markdown 只包含 block_id + message。
9. 编译工具自身失败时 ok=false / status=failed / error。
10. compile 不返回 validation。
11. compile 不返回 write_ref / transaction_id / review / safety。
12. compile 默认 modified=false。
```

---

## 1. 工具定位

`compile_blueprint_asset` 负责：

```text
编译一个明确 Blueprint 资产，并返回编译结果。
```

它不负责：

```text
修复错误
保存资产
修改蓝图图表
写 Graph
写 Component
写 Class Settings
写 Journal / Review
生成 transaction_id
```

---

## 2. ToolResultBase 约束

compile_blueprint_asset 使用 ToolResultBase 外壳。

成功执行时允许返回：

```text
ok
schema
operation
trace_id
status
modified
target
data.compile_result
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
"operation": "compile_blueprint_asset"
```

---

## 4. data.schema

```json
"schema": "CompileBlueprintAsset.v1"
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
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | 要编译的 Blueprint 资产路径。 |

---

# 6. 编译通过返回

## 6.1 无 warning

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "compile_blueprint_asset",
  "trace_id": "trace_20260503_2001",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  },

  "data": {
    "schema": "CompileBlueprintAsset.v1",
    "compile_result": {
      "success": true,
      "status": "succeeded",
      "warning_count": 0
    }
  }
}
```

## 6.2 有 warning

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "compile_blueprint_asset",
  "trace_id": "trace_20260503_2002",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  },

  "data": {
    "schema": "CompileBlueprintAsset.v1",
    "compile_result": {
      "success": true,
      "status": "succeeded",
      "warning_count": 2
    }
  }
}
```

规则：

```text
warning_count 不影响 compile_result.success。
success=true 表示蓝图编译通过。
```

---

# 7. 工具执行成功，但蓝图编译失败

编译流程成功执行，但蓝图本身有错误时：

```text
ok=true
status=completed
compile_result.success=false
```

示例：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "compile_blueprint_asset",
  "trace_id": "trace_20260503_2003",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  },

  "data": {
    "schema": "CompileBlueprintAsset.v1",
    "compile_result": {
      "success": false,
      "status": "failed",
      "warning_count": 1,
      "format": "markdown",
      "markdown": "## Compile Errors\n\n- `EG_PhysicsDoor_TogglePhysicsDoor0`: Cannot connect Object Reference pin to Boolean pin.\n- `EG_PhysicsDoor_OpenPhysicsDoor0`: Required input pin Target is not connected."
    }
  }
}
```

编译失败时不返回：

```text
error_count
messages[]
severity
node_ref
pin_ref
graph
ref object
validation
write_ref
transaction_id
```

Markdown 只保留：

```text
block_id + message
```

无法映射到 block_id 时使用：

```md
- `unmapped`: <message>
```

---

## 7.1 compile_result 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bSuccess` | `bool` | `data.compile_result.success` | `boolean` | 是 | 蓝图是否编译通过。warning 不影响该值。 |
| `Status` | `EBlueprintHelperCompileStatus` | `data.compile_result.status` | `string enum` | 是 | `succeeded` 或 `failed`。 |
| `WarningCount` | `int32` | `data.compile_result.warning_count` | `number` | 是 | 编译 warning 数量。 |
| `Format` | `FString` | `data.compile_result.format` | `string` | 编译失败时 | 固定为 `markdown`。 |
| `Markdown` | `FString` | `data.compile_result.markdown` | `string` | 编译失败时 | 编译错误 Markdown。 |

不返回：

```text
ErrorCount
Messages[]
NodeRef
PinRef
Severity
```

---

# 8. 工具自身失败

工具自身失败包括：

```text
资产不存在
目标不是 Blueprint
Bridge 断开
UE 编译 API 调用失败
```

示例：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "compile_blueprint_asset",
  "trace_id": "trace_20260503_2004",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_Missing"
  },

  "error": {
    "code": "asset_not_found",
    "stage": "resolve_asset",
    "message": "The requested Blueprint asset was not found.",
    "retryable": false
  }
}
```

工具自身失败才使用：

```text
ok=false
status=failed
error
```

蓝图编译失败不是工具自身失败。

---

# 9. error 字段映射

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperCompileToolErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperCompileStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表。 |

compile 工具自身失败不强制返回：

```text
rollback_result
```

因为 compile 不是事务写工具。

---

# 10. UE 侧建议结构体

```cpp
struct FBlueprintHelperCompileAssetResultData
{
    FString Schema; // CompileBlueprintAsset.v1
    FBlueprintHelperCompileResult CompileResult;
};

struct FBlueprintHelperCompileResult
{
    bool bSuccess = false;
    FString Status; // succeeded | failed
    int32 WarningCount = 0;

    // Only when compile failed.
    FString Format; // markdown
    FString Markdown;
};
```

不包含：

```cpp
int32 ErrorCount;
TArray<FBlueprintHelperCompileMessage> Messages;
FBlueprintHelperWriteRef WriteRef;
FBlueprintHelperValidationResult Validation;
```

---

# 11. 验收标准

```text
1. operation 固定为 compile_blueprint_asset。
2. data.schema 固定为 CompileBlueprintAsset.v1。
3. compile 工具执行成功时 status=completed。
4. 蓝图是否编译通过由 compile_result.success 表达。
5. warning_count 不影响 success。
6. 编译成功不返回 error_count。
7. 编译失败 ok=true / status=completed / success=false。
8. 编译失败错误用 compile_result.markdown。
9. markdown 只包含 block_id + message。
10. 工具自身失败时 ok=false / status=failed / error。
11. compile 不返回 validation。
12. compile 不返回 write_ref / transaction_id / review / safety。
13. compile 默认 modified=false。
```
