# BlueprintHelper Debug / Export Bundle / Large Payload UE 字段映射计划

2026-05-09 状态：已废弃。当前 Debug 合同删除 `read_large_payload_ref` 路径，Review 只链接 `debug_case_ids[]`，DebugBundle 只通过开发者导出生成，不作为 MCP large-payload 读取通道。

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：Debug / Export Bundle / Large Payload 字段确认稿  
本文边界：确认 debug bundle、transaction debug bundle、asset logic snapshot、large payload ref 读取工具的 Agent-facing 返回字段、UE/MCP 侧结构体映射、resource_ref / bundle_ref / snapshot_ref、隐私边界和大 payload 边界。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

Debug / Export Bundle / Large Payload 工具簇采用以下字段口径：

```text
1. 增加 export_debug_bundle。
2. export_debug_bundle 成功返回 export_result.exported / bundle_ref / format。
3. export_debug_bundle 不内联 bundle 内容。
4. export_debug_bundle 不包含 Token / secret / 完整 settings.json / CLAUDE.md 全文。
5. 增加 export_transaction_debug_bundle。
6. export_transaction_debug_bundle 必须以 transaction_id 定位。
7. export_transaction_debug_bundle 成功返回 bundle_ref / format。
8. export_transaction_debug_bundle 不用于普通 rollback 流程。
9. 增加 export_asset_logic_snapshot。
10. export_asset_logic_snapshot 必须指定 asset_path / snapshot_type。
11. export_asset_logic_snapshot 成功返回 snapshot_ref / format。
12. export_asset_logic_snapshot 不内联完整 LogicJson / RawJson / WidgetTree。
13. 增加 read_large_payload_ref。
14. read_large_payload_ref 支持 summary / chunk。
15. read_large_payload_ref 不默认 full 读取。
16. resource_ref / bundle_ref / snapshot_ref 不使用本地绝对路径。
17. 本簇所有工具 modified=false。
18. 本簇所有工具不返回 validation / write_ref / transaction_id / review / safety。
19. 所有 data.schema 使用短命名。
```

---

## 1. 工具簇边界

第一版覆盖：

```text
export_debug_bundle
export_transaction_debug_bundle
export_asset_logic_snapshot
read_large_payload_ref
```

第一版不覆盖：

```text
在线上传日志
自动提交 GitHub Issue
自动打包整个项目
导出完整 Content 目录
导出源码工程
导出敏感调试包
```

---

## 2. 通用返回原则

Debug / Export 工具默认：

```text
modified=false
```

不返回：

```text
validation
write_ref
transaction_id
journal_recorded
review
safety
```

大 payload 规则：

```text
不把大 payload 直接塞进普通 ToolResult
大内容通过 resource_ref / file_ref / bundle_ref / snapshot_ref 返回
普通 Agent 执行流程不依赖 Debug Bundle
Debug Bundle 只服务排错、审计导出、失败分析
```

所有 `data.schema` 使用短命名。

---

# 3. export_debug_bundle

## 3.1 工具定位

`export_debug_bundle` 负责导出当前 BlueprintHelper 运行环境的调试包。

可以包含：

```text
runtime 摘要
diagnostics Markdown
MCP tool registry 摘要
UE Bridge 状态摘要
最近错误摘要
版本状态摘要
```

不应默认包含：

```text
完整 settings.json
Token / secret
完整 CLAUDE.md
完整 Skill / AgentGuide
完整 Transaction Journal
完整资产快照
私有源码
整个项目目录
本地绝对路径
```

---

## 3.2 operation

```json
"operation": "export_debug_bundle"
```

---

## 3.3 target 字段

```json
"target": {
  "export_scope": "runtime_debug"
}
```

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `ExportScope` | `EBlueprintHelperExportScope` | `target.export_scope` | `string enum` | 是 | 固定或默认 `runtime_debug`。 |

---

## 3.4 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "export_debug_bundle",
  "trace_id": "trace_20260503_4601",
  "status": "completed",
  "modified": false,

  "target": {
    "export_scope": "runtime_debug"
  },

  "data": {
    "schema": "ExportDebugBundle.v1",
    "export_result": {
      "exported": true,
      "bundle_ref": "resource://blueprinthelper/debug/debug_bundle_20260503_4601.zip",
      "format": "zip"
    }
  }
}
```

---

## 3.5 export_result 字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bExported` | `bool` | `data.export_result.exported` | `boolean` | 是 | 是否成功导出。 |
| `BundleRef` | `FString` | `data.export_result.bundle_ref` | `string` | 是 | 导出包引用。 |
| `Format` | `FString` 或 enum | `data.export_result.format` | `string` | 是 | 例如 `zip`。 |

成功不返回：

```text
bundle bytes
full diagnostics inline
settings.json
tokens
local absolute path
```

---

## 3.6 导出失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "export_debug_bundle",
  "trace_id": "trace_20260503_4602",
  "status": "failed",
  "modified": false,

  "target": {
    "export_scope": "runtime_debug"
  },

  "error": {
    "code": "debug_bundle_export_failed",
    "stage": "write_bundle",
    "message": "Debug bundle could not be exported.",
    "retryable": true
  }
}
```

---

# 4. export_transaction_debug_bundle

## 4.1 工具定位

`export_transaction_debug_bundle` 负责导出某个明确 transaction 的调试包。

只用于：

```text
用户明确要求审计导出
失败排查
回滚冲突排查
Review UI 外部分析
```

不用于：

```text
普通 rollback 流程
普通写工具成功闭环
普通 Review 状态查询
```

Rollback 工具应内部读取 rollback_data，不需要 Agent 导出 transaction debug bundle 再传回。

---

## 4.2 operation

```json
"operation": "export_transaction_debug_bundle"
```

---

## 4.3 target 字段

```json
"target": {
  "transaction_id": "tx_20260503_1704",
  "export_scope": "transaction_debug"
}
```

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `TransactionId` | `FString` | `target.transaction_id` | `string` | 是 | 目标 transaction。 |
| `ExportScope` | `EBlueprintHelperExportScope` | `target.export_scope` | `string enum` | 是 | 固定为 `transaction_debug`。 |

---

## 4.4 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "export_transaction_debug_bundle",
  "trace_id": "trace_20260503_4701",
  "status": "completed",
  "modified": false,

  "target": {
    "transaction_id": "tx_20260503_1704",
    "export_scope": "transaction_debug"
  },

  "data": {
    "schema": "ExportTransactionDebugBundle.v1",
    "export_result": {
      "exported": true,
      "bundle_ref": "resource://blueprinthelper/transactions/tx_20260503_1704_debug.zip",
      "format": "zip"
    }
  }
}
```

说明：

```text
这里允许 target.transaction_id，因为该工具就是 transaction debug 导出工具。
普通写工具成功返回仍不暴露 transaction_id。
```

---

## 4.5 transaction 不存在

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "export_transaction_debug_bundle",
  "trace_id": "trace_20260503_4702",
  "status": "failed",
  "modified": false,

  "target": {
    "transaction_id": "tx_missing",
    "export_scope": "transaction_debug"
  },

  "error": {
    "code": "transaction_not_found",
    "stage": "resolve_transaction",
    "message": "The requested transaction was not found.",
    "retryable": false
  }
}
```

---

# 5. export_asset_logic_snapshot

## 5.1 工具定位

`export_asset_logic_snapshot` 负责导出某个资产的逻辑快照，用于人工排查或离线比较。

可以导出：

```text
LogicMD
LogicJson
RawJson
WidgetTree snapshot
ClassSettings snapshot
DataAsset / DataTable snapshot
```

但默认不应一次全导。输入层必须指定：

```text
snapshot_type
```

例如：

```text
logic_md
logic_json
raw_json
widget_tree
class_settings
data_table_rows
```

---

## 5.2 operation

```json
"operation": "export_asset_logic_snapshot"
```

---

## 5.3 target 字段

```json
"target": {
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "export_scope": "asset_snapshot",
  "snapshot_type": "logic_json"
}
```

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `AssetPath` | `FString` | `target.asset_path` | `string` | 是 | 要导出的资产路径。必须完整路径。 |
| `ExportScope` | `EBlueprintHelperExportScope` | `target.export_scope` | `string enum` | 是 | 固定为 `asset_snapshot`。 |
| `SnapshotType` | `EBlueprintHelperSnapshotType` | `target.snapshot_type` | `string enum` | 是 | 快照类型。 |

---

## 5.4 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "export_asset_logic_snapshot",
  "trace_id": "trace_20260503_4801",
  "status": "completed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "export_scope": "asset_snapshot",
    "snapshot_type": "logic_json"
  },

  "data": {
    "schema": "ExportAssetLogicSnapshot.v1",
    "export_result": {
      "exported": true,
      "snapshot_ref": "resource://blueprinthelper/snapshots/BP_BH_PhysicsDoor_logic_json_20260503.json",
      "format": "json"
    }
  }
}
```

---

## 5.5 snapshot export_result 字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bExported` | `bool` | `data.export_result.exported` | `boolean` | 是 | 是否成功导出。 |
| `SnapshotRef` | `FString` | `data.export_result.snapshot_ref` | `string` | 是 | 快照引用。 |
| `Format` | `FString` 或 enum | `data.export_result.format` | `string` | 是 | 例如 `json` / `md`。 |

不返回：

```text
完整 LogicJson inline
完整 RawJson inline
完整 WidgetTree snapshot inline
```

---

## 5.6 unsupported snapshot_type

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "export_asset_logic_snapshot",
  "trace_id": "trace_20260503_4802",
  "status": "failed",
  "modified": false,

  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "export_scope": "asset_snapshot",
    "snapshot_type": "raw_binary"
  },

  "error": {
    "code": "unsupported_snapshot_type",
    "stage": "validate_snapshot_type",
    "message": "The requested snapshot type is not supported.",
    "retryable": false,
    "conflicts": [
      {
        "code": "unsupported_snapshot_type",
        "snapshot_type": "raw_binary"
      }
    ]
  }
}
```

---

# 6. read_large_payload_ref

## 6.1 工具定位

`read_large_payload_ref` 负责读取前面工具返回的 `resource_ref / bundle_ref / snapshot_ref` 摘要或内容片段。

不默认一次返回完整大文件。

建议支持：

```text
mode=summary
mode=chunk
```

第一版建议不做：

```text
mode=full
```

---

## 6.2 operation

```json
"operation": "read_large_payload_ref"
```

---

## 6.3 target 字段

```json
"target": {
  "resource_ref": "resource://blueprinthelper/snapshots/BP_BH_PhysicsDoor_logic_json_20260503.json",
  "read_scope": "large_payload",
  "mode": "summary"
}
```

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `ResourceRef` | `FString` | `target.resource_ref` | `string` | 是 | 大 payload 引用。可接收 bundle_ref / snapshot_ref 的值。 |
| `ReadScope` | `EBlueprintHelperLargePayloadReadScope` | `target.read_scope` | `string enum` | 是 | 固定为 `large_payload`。 |
| `Mode` | `EBlueprintHelperLargePayloadReadMode` | `target.mode` | `string enum` | 是 | `summary` / `chunk`。 |
| `ChunkIndex` | `int32` | `target.chunk_index` | `number` | chunk 时 | 分片索引。 |

---

## 6.4 summary 返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_large_payload_ref",
  "trace_id": "trace_20260503_4901",
  "status": "completed",
  "modified": false,

  "target": {
    "resource_ref": "resource://blueprinthelper/snapshots/BP_BH_PhysicsDoor_logic_json_20260503.json",
    "read_scope": "large_payload",
    "mode": "summary"
  },

  "data": {
    "schema": "ReadLargePayloadRef.v1",
    "payload": {
      "available": true,
      "format": "json",
      "size_bytes": 48231,
      "chunk_count": 5
    }
  }
}
```

---

## 6.5 chunk 返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_large_payload_ref",
  "trace_id": "trace_20260503_4902",
  "status": "completed",
  "modified": false,

  "target": {
    "resource_ref": "resource://blueprinthelper/snapshots/BP_BH_PhysicsDoor_logic_json_20260503.json",
    "read_scope": "large_payload",
    "mode": "chunk",
    "chunk_index": 0
  },

  "data": {
    "schema": "ReadLargePayloadRef.v1",
    "payload": {
      "format": "json",
      "chunk_index": 0,
      "chunk_count": 5,
      "content": "{ "logic": { ... } }",
      "truncated": false
    }
  }
}
```

说明：

```text
chunk 模式可以返回内容，但必须是受控分片。
不允许普通工具直接返回完整大对象。
```

---

## 6.6 payload 字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bAvailable` | `bool` | `data.payload.available` | `boolean` | summary 时 | resource 是否可用。 |
| `Format` | `FString` 或 enum | `data.payload.format` | `string` | 是 | payload 格式。 |
| `SizeBytes` | `int64` | `data.payload.size_bytes` | `number` | summary 时 | payload 大小。 |
| `ChunkCount` | `int32` | `data.payload.chunk_count` | `number` | 是 | 分片数量。 |
| `ChunkIndex` | `int32` | `data.payload.chunk_index` | `number` | chunk 时 | 当前分片索引。 |
| `Content` | `FString` | `data.payload.content` | `string` | chunk 时 | 当前分片内容。 |
| `bTruncated` | `bool` | `data.payload.truncated` | `boolean` | chunk 时 | 当前分片内容是否截断。 |

---

## 6.7 resource_ref 不存在

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_large_payload_ref",
  "trace_id": "trace_20260503_4903",
  "status": "failed",
  "modified": false,

  "target": {
    "resource_ref": "resource://blueprinthelper/missing.json",
    "read_scope": "large_payload"
  },

  "error": {
    "code": "resource_ref_not_found",
    "stage": "resolve_resource_ref",
    "message": "The requested large payload resource reference was not found.",
    "retryable": false
  }
}
```

---

# 7. resource_ref / bundle_ref / snapshot_ref 命名

字段含义区分：

```text
resource_ref：通用大 payload 引用。
bundle_ref：zip / bundle 类导出包引用。
snapshot_ref：资产快照引用。
```

底层可以是同一种 URI / handle，但 Agent-facing 字段名按语义区分。

不返回本地绝对路径，例如：

```text
C:\Users\...\Saved\BlueprintHelper\...
```

返回：

```text
resource://blueprinthelper/...
```

建议统一使用：

```text
resource://blueprinthelper/...
```

---

# 8. 权限 / 隐私边界

Debug bundle 默认不包含：

```text
Token
secret
完整 settings.json
用户完整 CLAUDE.md
私有源码
整个项目目录
绝对路径
```

如果确实需要包含敏感信息，应单独设计：

```text
export_sensitive_debug_bundle
```

并要求用户明确确认。第一版不做。

---

# 9. error 字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperDebugExportErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperDebugExportStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |
| `Conflicts` | `TArray<FBlueprintHelperConflictItem>` | `error.conflicts` | `array<object>` | 可选 | 冲突列表。 |

---

# 10. UE/MCP 建议结构体

```cpp
struct FBlueprintHelperExportDebugBundleResultData
{
    FString Schema; // ExportDebugBundle.v1
    FBlueprintHelperExportResult ExportResult;
};

struct FBlueprintHelperExportTransactionDebugBundleResultData
{
    FString Schema; // ExportTransactionDebugBundle.v1
    FBlueprintHelperExportResult ExportResult;
};

struct FBlueprintHelperExportAssetLogicSnapshotResultData
{
    FString Schema; // ExportAssetLogicSnapshot.v1
    FBlueprintHelperExportResult ExportResult;
};

struct FBlueprintHelperExportResult
{
    bool bExported = false;
    FString BundleRef;
    FString SnapshotRef;
    FString Format;
};

struct FBlueprintHelperReadLargePayloadRefResultData
{
    FString Schema; // ReadLargePayloadRef.v1
    FBlueprintHelperLargePayload Payload;
};

struct FBlueprintHelperLargePayload
{
    bool bAvailable = false;
    FString Format;
    int64 SizeBytes = 0;
    int32 ChunkCount = 0;
    int32 ChunkIndex = 0;
    FString Content;
    bool bTruncated = false;
};
```

明确不包含：

```cpp
FBlueprintHelperValidationResult
FBlueprintHelperWriteRef
FString TransactionId // except target.transaction_id for export_transaction_debug_bundle
FString LocalAbsolutePath
TArray<uint8> BundleBytes
FString FullPayloadInline
FString Token
FString SettingsJson
FString ClaudeMdContent
```

---

# 11. 验收标准

```text
1. export_debug_bundle 返回 bundle_ref，不内联 bundle。
2. export_debug_bundle 不包含 Token / secret / 完整 settings.json / CLAUDE.md 全文。
3. export_transaction_debug_bundle 以 transaction_id 定位。
4. export_transaction_debug_bundle 不参与普通 rollback。
5. export_asset_logic_snapshot 必须指定 snapshot_type。
6. export_asset_logic_snapshot 返回 snapshot_ref，不内联完整快照。
7. read_large_payload_ref 支持 summary / chunk。
8. read_large_payload_ref 不默认 full 读取。
9. resource_ref / bundle_ref / snapshot_ref 不使用本地绝对路径。
10. 本簇所有工具 modified=false。
11. 本簇不返回 validation / write_ref / transaction_id / review / safety。
12. data.schema 使用短命名。
