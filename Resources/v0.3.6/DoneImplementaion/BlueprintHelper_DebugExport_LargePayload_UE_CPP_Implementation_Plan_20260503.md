# BlueprintHelper Debug / Export Bundle / Large Payload UE 侧 C++ 可执行实现计划

状态：[x] 已完成
日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_DebugExport_LargePayload_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++ + MCP resource-ref 读取桥接  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、在线上传日志、自动提交 GitHub Issue、打包整个项目、导出源码工程、敏感调试包

---

## 0. 实现目标

实现 Debug / Export Bundle / Large Payload 第一版工具簇：

```text
export_debug_bundle
export_transaction_debug_bundle
export_asset_logic_snapshot
read_large_payload_ref
```

该工具簇用于：

```text
排错
审计导出
失败分析
事务 debug bundle 导出
资产逻辑快照导出
大 payload 分片读取
```

它不用于：

```text
普通写工具成功闭环
普通 rollback 流程
普通 Review 查询
完整项目导出
源码工程导出
敏感信息导出
```

字段契约核心点：

```text
1. export_debug_bundle 成功返回 exported / bundle_ref / format。
2. export_debug_bundle 不内联 bundle 内容。
3. export_debug_bundle 不包含 Token / secret / 完整 settings.json / CLAUDE.md 全文。
4. export_transaction_debug_bundle 必须以 transaction_id 定位。
5. export_transaction_debug_bundle 成功返回 bundle_ref / format。
6. export_transaction_debug_bundle 不用于普通 rollback 流程。
7. export_asset_logic_snapshot 必须指定 asset_path / snapshot_type。
8. export_asset_logic_snapshot 成功返回 snapshot_ref / format。
9. export_asset_logic_snapshot 不内联完整 LogicJson / RawJson / WidgetTree。
10. read_large_payload_ref 支持 summary / chunk。
11. read_large_payload_ref 不默认 full 读取。
12. resource_ref / bundle_ref / snapshot_ref 不使用本地绝对路径。
13. 本簇所有工具 modified=false。
14. 本簇所有工具不返回 validation / write_ref / transaction_id / review / safety。
15. 所有 data.schema 使用短命名。
```

---

## 1. 总体边界

### 1.1 只读 / 导出工具

本簇虽然会在 `Saved/BlueprintHelper/...` 下写出 debug 文件或临时 bundle，但 Agent-facing 语义不是资产写入：

```text
modified=false
不修改 UE 资产
不保存资产
不编译资产
不写 Transaction Journal
不写 Review
不生成新的审计 transaction_id
```

### 1.2 大 payload 规则

普通 ToolResult 中不得直接塞入大内容：

```text
bundle bytes
完整 LogicJson
完整 RawJson
完整 WidgetTree snapshot
完整 Transaction Journal
完整 diagnostics bundle
```

必须通过以下引用返回：

```text
resource_ref
bundle_ref
snapshot_ref
```

再由 `read_large_payload_ref` 以 `summary` 或 `chunk` 读取。

### 1.3 隐私边界

默认导出不得包含：

```text
Token
secret
完整 settings.json
完整 CLAUDE.md
完整 Skill / AgentGuide
私有源码
整个项目目录
本地绝对路径
```

本簇第一版不设计 `export_sensitive_debug_bundle`。

---

## 2. UE 模块依赖

确认 `BlueprintHelper.Build.cs` 至少包含：

```text
Core
CoreUObject
Engine
UnrealEd
Json
JsonUtilities
Projects
AssetRegistry
UMG
UMGEditor（如导出 WidgetTree snapshot）
BlueprintGraph（如导出 LogicJson/RawJson）
```

文件 / 压缩相关：

```cpp
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Compression.h"
```

如果要生成 zip，UE C++ 原生 zip 支持有限。第一版可选：

```text
A. format=zip：使用项目已有 zip 工具或第三方模块。
B. format=directory：生成目录 bundle。
C. format=json：对 transaction / snapshot 导出单文件。
```

字段稿示例使用 `zip`，若当前项目无 zip 能力，建议先实现轻量 bundle 目录 + manifest，并在 Agent-facing `format` 使用：

```text
directory
```

但如果验收固定要求 zip，则需要引入 zip writer 或封装平台压缩。

---

## 3. Phase A：新增类型文件

### 3.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperDebugExportTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperDebugExportTypes.cpp
```

### 3.2 新增枚举

```cpp
enum class EBlueprintHelperExportScope : uint8
{
    RuntimeDebug,
    TransactionDebug,
    AssetSnapshot
};

enum class EBlueprintHelperSnapshotType : uint8
{
    LogicMd,
    LogicJson,
    RawJson,
    WidgetTree,
    ClassSettings,
    DataTableRows,
    DataAssetProperties
};

enum class EBlueprintHelperLargePayloadReadScope : uint8
{
    LargePayload
};

enum class EBlueprintHelperLargePayloadReadMode : uint8
{
    Summary,
    Chunk
};

enum class EBlueprintHelperDebugExportStage : uint8
{
    ParseInput,
    ValidateExportScope,
    ValidateSnapshotType,
    ResolveAsset,
    ResolveTransaction,
    CollectRuntimeDebug,
    CollectTransactionDebug,
    CollectAssetSnapshot,
    SanitizeBundle,
    WriteBundle,
    WriteSnapshot,
    RegisterResourceRef,
    ResolveResourceRef,
    ReadPayloadSummary,
    ReadPayloadChunk
};

enum class EBlueprintHelperDebugExportErrorCode : uint8
{
    InvalidRequest,
    UnsupportedExportScope,
    UnsupportedSnapshotType,
    AssetNotFound,
    TransactionNotFound,
    DebugBundleExportFailed,
    TransactionDebugBundleExportFailed,
    AssetSnapshotExportFailed,
    ResourceRefNotFound,
    ResourceRefExpired,
    ResourceRefForbidden,
    ChunkIndexOutOfRange,
    PayloadReadFailed,
    SensitiveContentBlocked,
    InternalError
};
```

### 3.3 字符串序列化

稳定输出：

```text
runtime_debug
transaction_debug
asset_snapshot
logic_md
logic_json
raw_json
widget_tree
class_settings
data_table_rows
data_asset_properties
large_payload
summary
chunk
resource_ref_not_found
unsupported_snapshot_type
```

不要输出 C++ enum 原名。

---

## 4. Phase B：Agent-facing DTO

### 4.1 ExportResult

```cpp
struct FBlueprintHelperExportResult
{
    bool bExported = false;

    // export_debug_bundle / export_transaction_debug_bundle
    TOptional<FString> BundleRef;

    // export_asset_logic_snapshot
    TOptional<FString> SnapshotRef;

    FString Format; // zip | json | md | directory

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 4.2 export_debug_bundle DTO

```cpp
struct FBlueprintHelperExportDebugBundleResultData
{
    FString Schema = TEXT("ExportDebugBundle.v1");
    FBlueprintHelperExportResult ExportResult;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 4.3 export_transaction_debug_bundle DTO

```cpp
struct FBlueprintHelperExportTransactionDebugBundleResultData
{
    FString Schema = TEXT("ExportTransactionDebugBundle.v1");
    FBlueprintHelperExportResult ExportResult;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 4.4 export_asset_logic_snapshot DTO

```cpp
struct FBlueprintHelperExportAssetLogicSnapshotResultData
{
    FString Schema = TEXT("ExportAssetLogicSnapshot.v1");
    FBlueprintHelperExportResult ExportResult;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 4.5 read_large_payload_ref DTO

```cpp
struct FBlueprintHelperReadLargePayloadRefResultData
{
    FString Schema = TEXT("ReadLargePayloadRef.v1");
    FBlueprintHelperLargePayload Payload;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperLargePayload
{
    // summary mode
    TOptional<bool> bAvailable;
    FString Format;
    TOptional<int64> SizeBytes;

    // summary + chunk
    int32 ChunkCount = 0;

    // chunk mode
    TOptional<int32> ChunkIndex;
    TOptional<FString> Content;
    TOptional<bool> bTruncated;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 4.6 明确禁止字段

所有 DTO 不包含：

```cpp
FBlueprintHelperValidationResult
FBlueprintHelperWriteRef
FString TransactionId // export_transaction target 中允许，但 data 中不作为 write_ref
FString ReviewStatus
FString SafetyProfile
FString LocalAbsolutePath
TArray<uint8> BundleBytes
FString FullPayloadInline
FString Token
FString SettingsJson
FString ClaudeMdContent
FString JournalPath
```

---

## 5. Phase C：ResourceRefRegistry

### 5.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperResourceRefRegistry.h
Source/BlueprintHelper/Private/Services/BlueprintHelperResourceRefRegistry.cpp
```

### 5.2 职责

`ResourceRefRegistry` 负责把内部文件路径映射为 Agent-facing 安全 URI：

```text
resource://blueprinthelper/debug/...
resource://blueprinthelper/transactions/...
resource://blueprinthelper/snapshots/...
```

它不向 Agent 暴露本地路径。

### 5.3 接口

```cpp
class FBlueprintHelperResourceRefRegistry
{
public:
    bool RegisterFile(
        const FString& Namespace,
        const FString& LocalFilePath,
        const FString& Format,
        FString& OutResourceRef,
        FBlueprintHelperToolError& OutError);

    bool ResolveRef(
        const FString& ResourceRef,
        FBlueprintHelperResolvedResourceRef& OutResolved,
        FBlueprintHelperToolError& OutError) const;

    bool IsValidResourceRef(const FString& ResourceRef) const;
};
```

### 5.4 Resolved 内部结构

```cpp
struct FBlueprintHelperResolvedResourceRef
{
    FString ResourceRef;

    // 内部使用，不序列化。
    FString LocalFilePath;

    FString Format;
    int64 SizeBytes = 0;
};
```

### 5.5 URI 规则

合法：

```text
resource://blueprinthelper/debug/debug_bundle_20260503_4601.zip
resource://blueprinthelper/transactions/tx_20260503_1704_debug.zip
resource://blueprinthelper/snapshots/BP_BH_PhysicsDoor_logic_json_20260503.json
```

非法：

```text
C:\Users\...
/Users/...
file://...
../../...
```

### 5.6 安全检查

`ResolveRef` 必须：

```text
1. 校验 scheme=resource。
2. 校验 host=blueprinthelper。
3. 校验路径 namespace 白名单。
4. 映射到 Saved/BlueprintHelper/Exports 下的内部路径。
5. 防止 .. 路径穿越。
6. 检查文件存在。
```

---

## 6. Phase D：DebugExportService

### 6.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperDebugExportService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperDebugExportService.cpp
```

### 6.2 服务接口

```cpp
class FBlueprintHelperDebugExportService
{
public:
    FBlueprintHelperToolResultBase ExportDebugBundle(
        const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase ExportTransactionDebugBundle(
        const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase ExportAssetLogicSnapshot(
        const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase ReadLargePayloadRef(
        const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool WriteRuntimeDebugBundle(
        FString& OutLocalPath,
        FString& OutFormat,
        FBlueprintHelperToolError& OutError) const;

    bool WriteTransactionDebugBundle(
        const FString& TransactionId,
        FString& OutLocalPath,
        FString& OutFormat,
        FBlueprintHelperToolError& OutError) const;

    bool WriteAssetSnapshot(
        const FString& AssetPath,
        EBlueprintHelperSnapshotType SnapshotType,
        FString& OutLocalPath,
        FString& OutFormat,
        FBlueprintHelperToolError& OutError) const;
};
```

---

## 7. Phase E：export_debug_bundle

### 7.1 Request

```cpp
struct FBlueprintHelperExportDebugBundleRequest
{
    EBlueprintHelperExportScope ExportScope =
        EBlueprintHelperExportScope::RuntimeDebug;
};
```

输入：

```json
{
  "export_scope": "runtime_debug"
}
```

空 payload 可默认：

```text
runtime_debug
```

### 7.2 可包含内容

第一版 debug bundle manifest：

```json
{
  "schema": "BlueprintHelper.DebugBundleManifest.v1",
  "created_at": "2026-05-03T46:01:00Z",
  "bundle_type": "runtime_debug",
  "contents": [
    "runtime_summary.json",
    "diagnostics.md",
    "recent_errors.json",
    "tool_registry_summary.json"
  ]
}
```

可包含：

```text
runtime_summary.json
diagnostics.md
tool_registry_summary.json
recent_errors.json
version_summary.json
```

### 7.3 必须脱敏 / 不包含

不得包含：

```text
Token
secret
完整 settings.json
完整 CLAUDE.md
完整 Skill / AgentGuide
完整 Transaction Journal
完整资产快照
私有源码
项目绝对路径
```

如果 runtime_summary 内部需要路径，只写：

```text
project_detected=true
project_marker=present
config_status=valid
```

不要写：

```text
project_root=C:\...
settings_path=C:\...
```

### 7.4 写出路径

内部建议：

```text
<Project>/Saved/BlueprintHelper/Exports/Debug/debug_bundle_<timestamp>/
```

或 zip：

```text
<Project>/Saved/BlueprintHelper/Exports/Debug/debug_bundle_<timestamp>.zip
```

Agent-facing 返回：

```text
resource://blueprinthelper/debug/debug_bundle_<timestamp>.zip
```

### 7.5 成功 ToolResult

```json
{
  "schema": "ExportDebugBundle.v1",
  "export_result": {
    "exported": true,
    "bundle_ref": "resource://blueprinthelper/debug/debug_bundle_20260503_4601.zip",
    "format": "zip"
  }
}
```

不内联 bundle 内容。

---

## 8. Phase F：export_transaction_debug_bundle

### 8.1 Request

```cpp
struct FBlueprintHelperExportTransactionDebugBundleRequest
{
    FString TransactionId;
    EBlueprintHelperExportScope ExportScope =
        EBlueprintHelperExportScope::TransactionDebug;
};
```

输入：

```json
{
  "transaction_id": "tx_20260503_1704",
  "export_scope": "transaction_debug"
}
```

### 8.2 Transaction 定位

复用：

```cpp
FBlueprintHelperTransactionJournalService::TryLoadJournalRecord(TransactionId, ...)
```

不存在：

```text
ok=false
status=failed
error.code=transaction_not_found
stage=resolve_transaction
```

### 8.3 可包含内容

导出包可包含：

```text
transaction_summary.json
journal_record_sanitized.json
review_summary.json
diff_summary.json
rollback_summary.json
```

不默认包含：

```text
完整 rollback_data
完整 node snapshots
完整 pin snapshots
本地绝对路径
```

如果 debug 场景必须包含 rollback_data，应按脱敏和体积控制写入 bundle 内部，但 Agent-facing 不内联。第一版建议：

```text
rollback_data_summary.json
```

而不是完整 rollback_data。

### 8.4 不用于普通 rollback

明确实现边界：

```text
RollbackCleanupTransaction 直接内部读取 Journal rollback_data。
不得要求 Agent 先 export_transaction_debug_bundle 再 rollback。
```

### 8.5 成功返回

```json
{
  "schema": "ExportTransactionDebugBundle.v1",
  "export_result": {
    "exported": true,
    "bundle_ref": "resource://blueprinthelper/transactions/tx_20260503_1704_debug.zip",
    "format": "zip"
  }
}
```

---

## 9. Phase G：export_asset_logic_snapshot

### 9.1 Request

```cpp
struct FBlueprintHelperExportAssetLogicSnapshotRequest
{
    FString AssetPath;
    EBlueprintHelperExportScope ExportScope =
        EBlueprintHelperExportScope::AssetSnapshot;
    EBlueprintHelperSnapshotType SnapshotType;
};
```

输入：

```json
{
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "export_scope": "asset_snapshot",
  "snapshot_type": "logic_json"
}
```

### 9.2 路径规则

必须完整 `asset_path`。  
不接受：

```text
%{path_filter}
本地绝对路径
object path with .AssetName? 视 resolver 兼容
```

### 9.3 支持 snapshot_type

第一版建议支持：

```text
logic_md
logic_json
raw_json
widget_tree
class_settings
data_table_rows
data_asset_properties
```

如果对应读取服务尚未实现，可返回：

```text
unsupported_snapshot_type
```

或：

```text
snapshot_type_unavailable
```

建议使用字段稿已给的：

```text
unsupported_snapshot_type
```

### 9.4 复用读取服务

不要重复实现逻辑导出。应复用现有 read 工具内部核心：

```text
LogicMD / LogicJson service
RawJson exporter
WidgetService read tree core
ClassSettings read core
DataTable read rows core
DataAsset read properties core
```

但 export 不把内容放入 ToolResult，而是写到 resource file。

### 9.5 格式规则

```text
logic_md -> md
logic_json -> json
raw_json -> json
widget_tree -> json
class_settings -> json
data_table_rows -> json
data_asset_properties -> json
```

### 9.6 成功返回

```json
{
  "schema": "ExportAssetLogicSnapshot.v1",
  "export_result": {
    "exported": true,
    "snapshot_ref": "resource://blueprinthelper/snapshots/BP_BH_PhysicsDoor_logic_json_20260503.json",
    "format": "json"
  }
}
```

不返回：

```text
完整 LogicJson inline
完整 RawJson inline
完整 WidgetTree snapshot inline
local file path
```

---

## 10. Phase H：read_large_payload_ref

### 10.1 Request

```cpp
struct FBlueprintHelperReadLargePayloadRefRequest
{
    FString ResourceRef;
    EBlueprintHelperLargePayloadReadScope ReadScope =
        EBlueprintHelperLargePayloadReadScope::LargePayload;
    EBlueprintHelperLargePayloadReadMode Mode =
        EBlueprintHelperLargePayloadReadMode::Summary;

    TOptional<int32> ChunkIndex;
};
```

输入 summary：

```json
{
  "resource_ref": "resource://blueprinthelper/snapshots/BP_BH_PhysicsDoor_logic_json_20260503.json",
  "mode": "summary"
}
```

输入 chunk：

```json
{
  "resource_ref": "resource://blueprinthelper/snapshots/BP_BH_PhysicsDoor_logic_json_20260503.json",
  "mode": "chunk",
  "chunk_index": 0
}
```

### 10.2 不支持 full

第一版不实现：

```text
mode=full
```

如果收到：

```text
ok=false
status=failed
error.code=invalid_request 或 unsupported_read_mode
```

建议新增错误码：

```text
unsupported_read_mode
```

若保持字段稿错误码集合，可用：

```text
invalid_request
```

### 10.3 summary 实现

```cpp
Resolved = ResourceRefRegistry.ResolveRef(ResourceRef);
SizeBytes = IFileManager::Get().FileSize(*Resolved.LocalFilePath);
ChunkCount = FMath::CeilToInt((double)SizeBytes / ChunkSizeBytes);
```

默认 chunk size：

```text
16 KB 或 32 KB
```

建议配置项：

```text
LargePayloadChunkSizeBytes = 16384
```

返回：

```json
{
  "payload": {
    "available": true,
    "format": "json",
    "size_bytes": 48231,
    "chunk_count": 5
  }
}
```

### 10.4 chunk 实现

```cpp
int64 Offset = ChunkIndex * ChunkSizeBytes;
Read bytes/string slice.
```

只支持文本内容：

```text
json
md
txt
log
```

zip/binary bundle 的 chunk 读取第一版可选择：

```text
A. 返回 base64 chunk。
B. 禁止 chunk 读取 binary，要求外部下载。
```

字段稿示例 chunk `content` 是字符串。建议第一版：

```text
仅文本 payload 支持 chunk content。
zip bundle summary 可读，但 chunk 返回 resource_ref_for_binary_unsupported。
```

如果必须读 zip，则返回 base64 并加：

```text
format=zip_base64_chunk
```

但字段稿未定义 base64，第一版不建议。

### 10.5 chunk 返回

```json
{
  "payload": {
    "format": "json",
    "chunk_index": 0,
    "chunk_count": 5,
    "content": "{ \"logic\": { ... } }",
    "truncated": false
  }
}
```

### 10.6 chunk index out of range

```text
ok=false
status=failed
error.code=chunk_index_out_of_range
stage=read_payload_chunk
```

### 10.7 resource_ref not found

```text
ok=false
status=failed
error.code=resource_ref_not_found
stage=resolve_resource_ref
```

---

## 11. Phase I：Sanitizer

### 11.1 新增服务

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperDebugSanitizer.h
Source/BlueprintHelper/Private/Services/BlueprintHelperDebugSanitizer.cpp
```

### 11.2 接口

```cpp
class FBlueprintHelperDebugSanitizer
{
public:
    FString SanitizeText(const FString& Input) const;
    TSharedPtr<FJsonObject> SanitizeJsonObject(const TSharedPtr<FJsonObject>& Input) const;
    bool IsForbiddenKey(const FString& Key) const;
};
```

### 11.3 禁止 key

大小写不敏感匹配：

```text
token
secret
password
api_key
apikey
auth
authorization
credential
private_key
settings_json
claude_md_content
local_path
absolute_path
```

### 11.4 路径脱敏

替换本地路径：

```text
C:\Users\...\Project
/Users/name/Project
/home/name/Project
```

为：

```text
<local_path>
```

或直接删除字段。

### 11.5 settings.json 处理

不得写完整 settings.json。  
只允许写摘要：

```json
{
  "config_status": "valid",
  "setup_completed": true
}
```

---

## 12. Phase J：Bundle manifest

### 12.1 manifest schema

每个 bundle / snapshot 建议写 manifest：

```json
{
  "schema": "BlueprintHelper.ExportManifest.v1",
  "type": "runtime_debug",
  "created_at": "2026-05-03T46:01:00Z",
  "format": "zip",
  "redacted": true,
  "contents": [
    {
      "name": "runtime_summary.json",
      "format": "json"
    }
  ]
}
```

### 12.2 manifest 不返回 Agent-facing

manifest 在 bundle 内部。  
ToolResult 只返回：

```text
bundle_ref / snapshot_ref / format
```

---

## 13. Phase K：ToolResult 构建

### 13.1 export_debug_bundle 成功

```cpp
FBlueprintHelperToolResultBase Result;
Result.bOk = true;
Result.Operation = TEXT("export_debug_bundle");
Result.Status = TEXT("completed");
Result.bModified = false;
Result.Target = MakeExportScopeTarget(TEXT("runtime_debug"));

FBlueprintHelperExportDebugBundleResultData Data;
Data.ExportResult.bExported = true;
Data.ExportResult.BundleRef = BundleRef;
Data.ExportResult.Format = Format;

Result.Data = Data.ToJson();
```

### 13.2 export_transaction_debug_bundle 成功

```cpp
Result.Operation = TEXT("export_transaction_debug_bundle");
Result.Target = {
    transaction_id,
    export_scope = transaction_debug
};
Data.Schema = ExportTransactionDebugBundle.v1;
```

注意：

```text
target.transaction_id 允许。
data 不返回 write_ref / transaction_id。
```

### 13.3 export_asset_logic_snapshot 成功

```cpp
Result.Operation = TEXT("export_asset_logic_snapshot");
Result.Target = {
    asset_path,
    export_scope = asset_snapshot,
    snapshot_type
};
Data.ExportResult.SnapshotRef = SnapshotRef;
```

### 13.4 read_large_payload_ref 成功

```cpp
Result.Operation = TEXT("read_large_payload_ref");
Result.Target = {
    resource_ref,
    read_scope = large_payload,
    mode,
    chunk_index?
};
Result.Data = PayloadData.ToJson();
```

### 13.5 failure

```cpp
Result.bOk = false;
Result.Status = TEXT("failed");
Result.bModified = false;
Result.Error = MakeDebugExportError(...);
```

---

## 14. Phase L：Bridge Router 接入

### 14.1 新增 commands

```text
export_debug_bundle
export_transaction_debug_bundle
export_asset_logic_snapshot
read_large_payload_ref
```

### 14.2 Router 分支

```cpp
if (Request.Command == TEXT("export_debug_bundle"))
{
    return HandleExportDebugBundle(Request);
}
if (Request.Command == TEXT("export_transaction_debug_bundle"))
{
    return HandleExportTransactionDebugBundle(Request);
}
if (Request.Command == TEXT("export_asset_logic_snapshot"))
{
    return HandleExportAssetLogicSnapshot(Request);
}
if (Request.Command == TEXT("read_large_payload_ref"))
{
    return HandleReadLargePayloadRef(Request);
}
```

### 14.3 Handler 模板

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleExportDebugBundle(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result =
        DebugExportService.ExportDebugBundle(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("debug export failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

---

## 15. Phase M：RequestValidator / 权限

### 15.1 export_debug_bundle

```cpp
OptionalString(Payload, TEXT("export_scope")); // default runtime_debug
```

如果传入非 runtime_debug：

```text
unsupported_export_scope
```

### 15.2 export_transaction_debug_bundle

```cpp
RequireString(Payload, TEXT("transaction_id"));
OptionalString(Payload, TEXT("export_scope")); // must transaction_debug if present
```

### 15.3 export_asset_logic_snapshot

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("snapshot_type"));
OptionalString(Payload, TEXT("export_scope")); // must asset_snapshot if present
```

`asset_path` 必须完整路径，不接受：

```text
%{path_filter}
本地绝对路径
```

### 15.4 read_large_payload_ref

```cpp
RequireString(Payload, TEXT("resource_ref"));
RequireString(Payload, TEXT("mode")); // summary | chunk
OptionalInt(Payload, TEXT("chunk_index"));
```

chunk mode：

```text
chunk_index 必填。
```

### 15.5 权限

默认：

```text
不需要 write token
modified=false
ReadOnly 下可用
```

但出于隐私，建议加 debug export policy：

```text
export_debug_bundle：允许。
export_transaction_debug_bundle：允许，但只导出 sanitized summary。
export_asset_logic_snapshot：允许，但只导出明确 snapshot_type。
read_large_payload_ref：只允许 resource://blueprinthelper/...。
```

如果未来支持敏感 bundle，必须另设风险授权，不复用本工具。

---

## 16. Phase N：与 Transaction / Rollback 的边界

### 16.1 Transaction debug bundle

允许：

```text
target.transaction_id
bundle_ref
format
```

不允许：

```text
用于普通 rollback
替代 read_blueprint_helper_transaction
替代 Review UI
```

### 16.2 Rollback 工具

Rollback 工具内部读取：

```text
Journal rollback_data
```

不需要 Agent：

```text
export_transaction_debug_bundle
read_large_payload_ref
```

### 16.3 Transaction Query

查询摘要用：

```text
list_blueprint_helper_transactions
read_blueprint_helper_transaction
```

Debug bundle 只用于显式 debug 导出。

---

## 17. Phase O：与 Logic Read 的边界

`export_asset_logic_snapshot` 可复用 LogicMD / LogicJson / RawJson 的内部读取能力，但不替代常规 read tools。

普通 Agent 理解蓝图仍用：

```text
ReadBlueprintLogicMdByTarget
ReadBlueprintLogicJsonByTarget
```

只有以下场景用 snapshot：

```text
用户明确要求导出
大 payload
失败排查
离线比较
debug bundle
```

---

## 18. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperDebugExportContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperDebugExportPrivacyTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperLargePayloadRefTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperAssetSnapshotExportTests.cpp
```

### 18.1 Contract tests

```text
1. export_debug_bundle_success_contract
   - operation=export_debug_bundle
   - data.schema=ExportDebugBundle.v1
   - export_result.exported=true
   - bundle_ref 存在
   - format 存在
   - 不内联 bundle 内容
   - modified=false
   - 不返回 validation/write_ref/transaction_id

2. export_transaction_debug_bundle_success_contract
   - target.transaction_id 存在
   - data.schema=ExportTransactionDebugBundle.v1
   - bundle_ref 存在
   - 不返回 rollback_data inline

3. export_asset_logic_snapshot_success_contract
   - target.asset_path / snapshot_type
   - data.schema=ExportAssetLogicSnapshot.v1
   - snapshot_ref 存在
   - 不内联 LogicJson

4. read_large_payload_summary_contract
   - data.schema=ReadLargePayloadRef.v1
   - payload.available / format / size_bytes / chunk_count
   - 不返回 content

5. read_large_payload_chunk_contract
   - payload.chunk_index / chunk_count / content / truncated
```

### 18.2 Privacy tests

```text
1. debug_bundle_does_not_include_token
2. debug_bundle_does_not_include_secret
3. debug_bundle_does_not_include_full_settings_json
4. debug_bundle_does_not_include_claude_md_content
5. refs_do_not_use_local_absolute_path
6. error_messages_do_not_leak_local_paths
7. resource_ref_rejects_file_scheme
8. resource_ref_rejects_path_traversal
```

### 18.3 Large payload tests

```text
1. summary_existing_ref
2. summary_missing_ref_fails
3. chunk_index_zero_reads_first_chunk
4. chunk_index_out_of_range_fails
5. mode_full_rejected
6. binary_zip_chunk_rejected_or_base64_policy_explicit
```

### 18.4 Snapshot tests

```text
1. export_logic_md_snapshot
2. export_logic_json_snapshot
3. export_widget_tree_snapshot
4. unsupported_snapshot_type_fails
5. compressed_asset_path_rejected
```

---

## 19. 推荐提交顺序

### Commit 1：DTO 与 ResourceRefRegistry

```text
Add Debug Export DTOs
Add resource://blueprinthelper ref registry
Add ref validation and path traversal protection
```

验收：

```text
bundle_ref/snapshot_ref 不含本地路径。
非法 ref 被拒绝。
```

### Commit 2：Sanitizer

```text
Add DebugSanitizer
Redact token/secret/path/settings/CLAUDE content
Add privacy tests
```

验收：

```text
默认 debug bundle 不含敏感字段。
```

### Commit 3：export_debug_bundle

```text
Implement runtime debug bundle export
Write manifest and sanitized summaries
Register bundle_ref
```

验收：

```text
成功只返回 exported/bundle_ref/format。
不内联 bundle。
```

### Commit 4：export_transaction_debug_bundle

```text
Resolve transaction_id
Export sanitized transaction debug summary
Register bundle_ref
```

验收：

```text
transaction_not_found 正确失败。
不输出 rollback_data inline。
```

### Commit 5：export_asset_logic_snapshot

```text
Implement snapshot_type validation
Reuse Logic/Widget/Class/Data read cores
Write snapshot file
Register snapshot_ref
```

验收：

```text
必须指定 asset_path/snapshot_type。
不内联完整快照。
```

### Commit 6：read_large_payload_ref summary/chunk

```text
Implement summary mode
Implement chunk mode for text payload
Reject mode=full
```

验收：

```text
summary 不返回 content。
chunk 返回受控分片。
```

### Commit 7：Bridge / Validator / Permissions

```text
Register debug export commands
Add validators
Classify tools read-only/debug export
Reject local paths and compressed asset paths where needed
```

验收：

```text
ReadOnly 下可调用。
modified=false。
```

### Commit 8：Protocol regression

```text
Add no validation/write_ref/transaction_id tests
Add no local path leak tests
Add no inline bundle tests
```

验收：

```text
字段稿验收项全部通过。
```

---

## 20. 第一版不做的内容

```text
1. 不在线上传日志。
2. 不自动提交 GitHub Issue。
3. 不打包整个项目。
4. 不导出完整 Content 目录。
5. 不导出源码工程。
6. 不导出敏感调试包。
7. 不内联完整 LogicJson / RawJson。
8. 不内联 bundle bytes。
9. 不默认 full 读取大 payload。
10. 不返回本地绝对路径。
11. 不把 transaction debug bundle 用于普通 rollback。
12. 不返回 validation / write_ref / transaction_id / review / safety。
```

---

## 21. 最小验收标准

export_debug_bundle：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "export_debug_bundle",
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

export_transaction_debug_bundle：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "export_transaction_debug_bundle",
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

export_asset_logic_snapshot：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "export_asset_logic_snapshot",
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

read_large_payload_ref summary：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_large_payload_ref",
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

read_large_payload_ref chunk：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_large_payload_ref",
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
      "content": "{ \"logic\": { ... } }",
      "truncated": false
    }
  }
}
```

必须不出现：

```text
local absolute path
file:// path
bundle bytes inline
full LogicJson inline
full RawJson inline
settings.json
Token
secret
CLAUDE.md content
validation
write_ref
transaction_id as write result
journal_recorded
review
safety
```

---

## 22. 实现风险

### 22.1 bundle_ref 泄露本地路径

风险：

```text
直接返回 Saved/BlueprintHelper/Exports/... 的绝对路径。
```

处理：

```text
所有导出统一注册到 ResourceRefRegistry。
Agent-facing 只返回 resource://blueprinthelper/...。
```

### 22.2 Debug bundle 含敏感配置

风险：

```text
把完整 settings.json / token / CLAUDE.md 写进 bundle。
```

处理：

```text
DebugSanitizer 白名单导出。
禁止敏感 key。
隐私测试覆盖。
```

### 22.3 Large payload 被 full 读取

风险：

```text
read_large_payload_ref mode=full 导致 Token 爆炸。
```

处理：

```text
第一版仅 summary/chunk。
full 返回 invalid_request。
```

### 22.4 binary zip chunk 语义不清

风险：

```text
chunk content 字符串无法表达 zip bytes。
```

处理：

```text
第一版 zip 只提供 summary，不提供 chunk content；或后续明确 base64 协议。
```

### 22.5 Snapshot 导出绕过正常 read 工具边界

风险：

```text
Agent 用 snapshot 替代 LogicMD/LogicJson 正常读取，造成 payload 膨胀。
```

处理：

```text
普通读取仍使用 read tools。
snapshot 仅用于用户明确导出 / debug。
Tool description 明确用途。
