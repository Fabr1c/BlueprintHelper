# BlueprintHelper DataTable UE 侧 C++ 可执行实现计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_DataTable_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、CSV/JSON 全量导入导出、RowStruct 修改、C++ struct/class 修改

---

## 0. 实现目标

实现 DataTable 第一版工具簇：

```text
read_data_table
read_data_table_rows
add_data_table_row
update_data_table_row
update_data_table_rows
remove_data_table_row
remove_data_table_rows
```

字段契约核心点：

```text
1. DataTable 第一版不做 CSV / JSON 全量导入导出。
2. DataTable 第一版不修改 C++ struct/class 或 RowStruct。
3. read_data_table 只返回 row_struct / row_count / row_names，不默认返回所有行。
4. read_data_table_rows 只返回请求行 values。
5. add_data_table_row 成功只返回 added_count。
6. add_data_table_row name_collision 只支持 fail_if_exists / reuse_if_exists。
7. update_data_table_row / rows 成功只返回 row_result 计数。
8. DataTable 批量更新默认事务化，invalid 时整批失败。
9. remove_data_table_row / rows 必须 dry_run。
10. remove 成功只返回 removed_count。
11. DataTable 写工具成功不返回 write_ref / transaction_id / review / safety。
12. DataTable 写工具成功保留 validation，但 validation 只返回 should_compile / should_save。
13. DataTable 写工具 validation 不返回 compiled / saved。
14. 通常 should_compile=false / should_save=true。
15. 所有 data.schema 使用短命名。
```

---

## 1. 当前依赖与模块要求

### 1.1 UE 模块依赖

确认 `BlueprintHelper.Build.cs` 至少包含：

```text
Core
CoreUObject
Engine
UnrealEd
Json
JsonUtilities
```

建议 DataTable 工具还需要：

```text
AssetRegistry
StructUtils（若后续使用 FInstancedStruct，可选）
```

第一版不需要 DataTableEditor 模块。

### 1.2 可复用基础服务

本计划假设 UE 插件已有或将统一具备：

```text
FBlueprintHelperToolResultBase
FBlueprintHelperToolResultBuilder
FBlueprintHelperScopedAssetMutation
FBlueprintHelperRequestValidator
FBlueprintHelperBridgeRouter
FBlueprintHelperFailedItem
FBlueprintHelperConflictItem
FBlueprintHelperDryRunResult
FBlueprintHelperValidationResult
FBlueprintHelperPropertyReflectionService
```

如果当前没有 `PropertyReflectionService`，DataTable 工具必须实现最小 `UScriptStruct <-> JSON` 转换服务，不要把转换逻辑散落在各个工具函数中。

---

## 2. Phase A：新增类型文件

### 2.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperDataTableTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperDataTableTypes.cpp
```

### 2.2 新增枚举

```cpp
enum class EBlueprintHelperDataScope : uint8
{
    DataTable,
    DataTableRow,
    DataTableRows
};

enum class EBlueprintHelperDataReadScope : uint8
{
    DataTable,
    DataTableRows
};

enum class EBlueprintHelperRowWriteMode : uint8
{
    Single,
    Batch
};

enum class EBlueprintHelperRowNameCollisionPolicy : uint8
{
    FailIfExists,
    ReuseIfExists
};

enum class EBlueprintHelperDataTableStage : uint8
{
    ParseInput,
    ResolveAsset,
    ResolveDataTable,
    ResolveRowStruct,
    ResolveRows,
    RowCollisionCheck,
    ValidateRows,
    ValidateFields,
    DryRun,
    SnapshotRows,
    AddRow,
    UpdateRows,
    RemoveRows,
    MarkModified,
    Rollback
};

enum class EBlueprintHelperDataTableErrorCode : uint8
{
    InvalidRequest,
    AssetNotFound,
    TargetNotDataTable,
    RowStructMissing,
    RowStructUnsupported,
    RowNotFound,
    RowAlreadyExists,
    UnsupportedNameCollisionPolicy,
    FieldNotFound,
    FieldNotWritable,
    TypeMismatch,
    ValueOutOfRange,
    InvalidDataTableRowSettings,
    RowExternalDependentsExist,
    RemoveDataTableRowDryRunRequired,
    RowAddFailed,
    RowUpdateFailed,
    RowRemoveFailed,
    RollbackFailed,
    InternalError
};
```

### 2.3 字符串序列化

稳定输出：

```text
data_table
data_table_row
data_table_rows
single
batch
fail_if_exists
reuse_if_exists
row_not_found
field_not_found
invalid_data_table_row_settings
```

不要输出 C++ enum 原名。

---

## 3. Phase B：Agent-facing DTO

### 3.1 read_data_table

```cpp
struct FBlueprintHelperReadDataTableResultData
{
    FString Schema = TEXT("ReadDataTable.v1");
    FBlueprintHelperDataTableSummary DataTable;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperDataTableSummary
{
    FString RowStruct;
    int32 RowCount = 0;
    TArray<FString> RowNames;

    TSharedRef<FJsonObject> ToJson() const;
};
```

输出：

```json
{
  "schema": "ReadDataTable.v1",
  "data_table": {
    "row_struct": "/Script/Game.WeaponRow",
    "row_count": 3,
    "row_names": ["Pistol", "Rifle", "Shotgun"]
  }
}
```

不返回：

```text
all rows
row values
full struct schema
serialized binary data
```

### 3.2 read_data_table_rows

```cpp
struct FBlueprintHelperReadDataTableRowsResultData
{
    FString Schema = TEXT("ReadDataTableRows.v1");
    TArray<FBlueprintHelperDataTableRowValue> Rows;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperDataTableRowValue
{
    FString RowName;
    TSharedPtr<FJsonObject> Values;

    TSharedRef<FJsonObject> ToJson() const;
};
```

输出：

```json
{
  "schema": "ReadDataTableRows.v1",
  "rows": [
    {
      "row_name": "Pistol",
      "values": {
        "Damage": 12,
        "Cooldown": 0.25,
        "Ammo": 12
      }
    }
  ]
}
```

### 3.3 write row_result

```cpp
struct FBlueprintHelperDataTableRowWriteResultData
{
    FString Schema; // AddDataTableRow.v1 / UpdateDataTableRow.v1 / RemoveDataTableRow.v1
    FBlueprintHelperDataTableRowWriteResult RowResult;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperDataTableRowWriteResult
{
    // update only
    TOptional<FString> Mode; // single | batch
    TOptional<int32> RequestedCount;
    TOptional<int32> UpdatedCount;
    TOptional<int32> ChangedCount;
    TOptional<int32> NoOpCount;

    // add only
    TOptional<int32> AddedCount;
    TOptional<bool> bReusedExisting;

    // remove only
    TOptional<int32> RemovedCount;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.4 validation DTO

DataTable 写工具的 validation 只允许：

```cpp
struct FBlueprintHelperDataTableValidation
{
    bool bShouldCompile = false;
    bool bShouldSave = false;

    TSharedRef<FJsonObject> ToJson() const;
};
```

不得返回：

```text
compiled
saved
```

---

## 4. Phase C：DataTableService

### 4.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperDataTableService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperDataTableService.cpp
```

### 4.2 服务接口

```cpp
class FBlueprintHelperDataTableService
{
public:
    FBlueprintHelperToolResultBase ReadDataTable(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase ReadDataTableRows(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase AddDataTableRow(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase UpdateDataTableRow(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase UpdateDataTableRows(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase RemoveDataTableRow(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase RemoveDataTableRows(const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool ResolveDataTable(
        const FString& AssetPath,
        UDataTable*& OutDataTable,
        FBlueprintHelperToolError& OutError) const;

    bool ResolveRowStruct(
        UDataTable* DataTable,
        UScriptStruct*& OutRowStruct,
        FBlueprintHelperToolError& OutError) const;

    bool RowExists(UDataTable* DataTable, const FName& RowName) const;
};
```

---

## 5. Phase D：资产与 RowStruct 解析

### 5.1 ResolveDataTable

```cpp
UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
UDataTable* DataTable = Cast<UDataTable>(Asset);
```

失败映射：

```text
asset_not_found
target_not_data_table
```

### 5.2 ResolveRowStruct

```cpp
UScriptStruct* RowStruct = DataTable->GetRowStruct();
```

如果 RowStruct 为空：

```text
error.code=row_struct_missing
stage=resolve_row_struct
```

第一版不修改 RowStruct，不替换 RowStruct。  
如果用户请求修改 RowStruct，应 stop/report 到上层；工具直接返回：

```text
row_struct_modification_unsupported
```

但该字段稿未定义专门错误码，可映射为：

```text
invalid_request
```

或新增：

```text
row_struct_modification_unsupported
```

建议新增错误码，便于 Agent 精确处理。

---

## 6. Phase E：UScriptStruct / JSON 转换服务

### 6.1 新增或复用服务

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperStructJsonService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperStructJsonService.cpp
```

### 6.2 接口

```cpp
class FBlueprintHelperStructJsonService
{
public:
    bool StructToJsonObject(
        UScriptStruct* StructType,
        const uint8* StructMemory,
        TSharedPtr<FJsonObject>& OutJson,
        FString& OutError) const;

    bool JsonObjectToStruct(
        UScriptStruct* StructType,
        const TSharedPtr<FJsonObject>& Json,
        uint8* StructMemory,
        TArray<FBlueprintHelperConflictItem>& OutConflicts) const;

    bool ApplyPartialJsonToStruct(
        UScriptStruct* StructType,
        const TSharedPtr<FJsonObject>& PartialJson,
        uint8* StructMemory,
        TArray<FBlueprintHelperConflictItem>& OutConflicts,
        bool& bOutChanged) const;
};
```

### 6.3 推荐实现

优先使用：

```cpp
FJsonObjectConverter::UStructToJsonObject
FJsonObjectConverter::JsonObjectToUStruct
```

对于 partial update，需要：

```text
1. 先复制现有 row 到临时 struct memory。
2. 对 partial JSON 的字段逐个查找 FProperty。
3. 对每个字段做 JSON -> FProperty value。
4. 比较 before/after 判定 changed/no_op。
```

不要要求 update 请求提供整行完整 values。

### 6.4 类型支持边界

第一版建议支持：

```text
bool
int32 / int64
float / double
FString / FName / FText
enum
FVector / FRotator / FLinearColor 等常见 struct
TSoftObjectPtr / FSoftObjectPath 字符串
```

第一版可阻断：

```text
复杂 UObject* 强引用
嵌套数组 / Map 的复杂对象
无法从 JSON 安全解析的字段
```

失败进入：

```text
error.conflicts[].code = type_mismatch / unsupported_field_type
```

---

## 7. Phase F：read_data_table

### 7.1 Request

```cpp
struct FBlueprintHelperReadDataTableRequest
{
    FString AssetPath;
};
```

### 7.2 实现

```cpp
UDataTable* DataTable = ResolveDataTable(...);
UScriptStruct* RowStruct = DataTable->GetRowStruct();

TArray<FName> RowNames = DataTable->GetRowNames();
```

RowStruct 路径：

```cpp
FString RowStructPath = RowStruct->GetPathName();
```

### 7.3 成功结果

```cpp
Data.DataTable.RowStruct = RowStructPath;
Data.DataTable.RowCount = RowNames.Num();
Data.DataTable.RowNames = ConvertNames(RowNames);
```

ToolResult：

```text
ok=true
status=completed
modified=false
operation=read_data_table
```

Read 工具不返回 validation。

---

## 8. Phase G：read_data_table_rows

### 8.1 Request

```cpp
struct FBlueprintHelperReadDataTableRowsRequest
{
    FString AssetPath;
    TArray<FName> RowNames;
};
```

### 8.2 输入规则

必须明确请求行名。  
如果 `row_names` 为空，第一版建议失败：

```text
error.code=invalid_request
message=read_data_table_rows requires explicit row_names.
```

避免误返回全表数据。

### 8.3 实现

```cpp
for (const FName& RowName : Request.RowNames)
{
    uint8* RowPtr = DataTable->FindRowUnchecked(RowName);
    if (!RowPtr)
    {
        Conflict row_not_found;
    }

    StructJsonService.StructToJsonObject(RowStruct, RowPtr, RowJson);
}
```

如果任一请求行不存在：

```text
ok=false
status=failed
modified=false
error.code=row_not_found
stage=resolve_rows
conflicts[] row_name
```

不做部分返回。

---

## 9. Phase H：add_data_table_row

### 9.1 Request

```cpp
struct FBlueprintHelperAddDataTableRowRequest
{
    FString AssetPath;
    FName RowName;
    TSharedPtr<FJsonObject> Values;

    EBlueprintHelperRowNameCollisionPolicy NameCollisionPolicy =
        EBlueprintHelperRowNameCollisionPolicy::FailIfExists;
};
```

### 9.2 输入 JSON

```json
{
  "asset_path": "/Game/Data/DT_Weapons",
  "row_name": "Pistol",
  "values": {
    "Damage": 12,
    "Cooldown": 0.25,
    "Ammo": 12
  },
  "name_collision": "fail_if_exists"
}
```

### 9.3 row collision

```text
fail_if_exists + row exists → failed / row_already_exists
reuse_if_exists + row exists → no_op / added_count=0 / reused_existing=true
```

不支持：

```text
auto_rename
replace_existing
```

### 9.4 构造新 row

推荐使用 `FStructOnScope`：

```cpp
FStructOnScope NewRow(RowStruct);
uint8* NewRowMemory = NewRow.GetStructMemory();

RowStruct->InitializeStruct(NewRowMemory);
StructJsonService.JsonObjectToStruct(RowStruct, Request.Values, NewRowMemory, Conflicts);
```

如果 Values 缺失字段：

```text
按 RowStruct 默认值保留。
```

如果字段无效：

```text
ok=false
status=failed
modified=false
error.code=invalid_data_table_row_settings
stage=validate_rows
```

### 9.5 AddRow

```cpp
DataTable->Modify();
DataTable->AddRow(Request.RowName, *reinterpret_cast<const uint8*>(NewRowMemory));
DataTable->MarkPackageDirty();
```

不同 UE 版本 `AddRow` 签名可能是：

```cpp
void AddRow(FName RowName, const uint8* RowData);
```

或按 struct 引用封装。实现时需以当前 UE5.3 源码为准写兼容 wrapper：

```cpp
FBlueprintHelperDataTableCompat::AddRow(DataTable, RowName, RowStruct, NewRowMemory);
```

### 9.6 成功返回

```json
{
  "data": {
    "schema": "AddDataTableRow.v1",
    "row_result": {
      "added_count": 1
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

成功不返回 row_name / values / before / after / write_ref。

---

## 10. Phase I：update_data_table_row / update_data_table_rows

### 10.1 Request

```cpp
struct FBlueprintHelperDataTableRowUpdateItem
{
    FName RowName;
    TSharedPtr<FJsonObject> Values; // partial fields
};

struct FBlueprintHelperUpdateDataTableRowsRequest
{
    FString AssetPath;
    TArray<FBlueprintHelperDataTableRowUpdateItem> Items;
    bool bIsBatch = false;
};
```

单行工具内部转换成 `Items.Num()==1`。

### 10.2 执行流程

```text
1. ResolveDataTable。
2. ResolveRowStruct。
3. 对所有 row 做预校验：
   - row 存在
   - field 存在
   - field 可写
   - JSON value 可转换
4. 任一 invalid → 整批失败，不修改任何 row。
5. Snapshot rows。
6. Begin ScopedAssetMutation。
7. 对每个 row 应用 partial values。
8. 统计 requested/updated/changed/no_op。
9. Mark dirty。
10. 返回 row_result。
```

### 10.3 事务化批量

必须先完整预校验，再执行写入。

写入中失败时：

```text
尝试恢复 snapshot。
恢复成功：modified=false。
恢复失败：modified=true，error.code=rollback_failed。
```

### 10.4 snapshot

```cpp
struct FBlueprintHelperDataTableRowSnapshot
{
    FName RowName;
    TArray<uint8> RowBytes;
};
```

复制：

```cpp
uint8* ExistingRow = DataTable->FindRowUnchecked(RowName);
TArray<uint8> Bytes;
Bytes.SetNum(RowStruct->GetStructureSize());
RowStruct->CopyScriptStruct(Bytes.GetData(), ExistingRow);
```

恢复：

```cpp
RowStruct->CopyScriptStruct(ExistingRow, Snapshot.RowBytes.GetData());
```

注意析构/内存生命周期：如果 snapshot 保存复杂属性，需要使用 `InitializeStruct` / `DestroyStruct` 辅助管理。更稳妥可用 `FStructOnScope` 列表保存 before 状态。

### 10.5 changed/no_op 计算

对每行：

```text
changed = before bytes / serialized JSON 与 after 比较。
```

推荐：

```cpp
bool bChanged = !StructJsonService.AreStructValuesEqual(RowStruct, BeforeMemory, AfterMemory);
```

如果字段值相同：

```text
no_op_count++
```

### 10.6 成功返回

单行：

```json
"row_result": {
  "mode": "single",
  "requested_count": 1,
  "updated_count": 1,
  "changed_count": 1,
  "no_op_count": 0
}
```

批量：

```json
"row_result": {
  "mode": "batch",
  "requested_count": 3,
  "updated_count": 3,
  "changed_count": 2,
  "no_op_count": 1
}
```

validation：

```json
"validation": {
  "should_compile": false,
  "should_save": true
}
```

不返回 compiled/saved。

---

## 11. Phase J：remove_data_table_row / remove_data_table_rows

### 11.1 Request

```cpp
struct FBlueprintHelperRemoveDataTableRowsRequest
{
    FString AssetPath;
    TArray<FName> RowNames;
    bool bIsBatch = false;
    bool bDryRun = false;
};
```

### 11.2 dry_run 强制

删除 row 是破坏性操作，必须支持 dry_run。

dry_run passed：

```json
{
  "schema": "RemoveDataTableRowDryRun.v1",
  "dry_run": {
    "result": "passed",
    "can_execute": true
  }
}
```

dry_run blocked：

```json
{
  "dry_run": {
    "result": "blocked",
    "can_execute": false,
    "blocked_by": ["row_external_dependents_exist"],
    "conflicts": [
      {
        "code": "row_external_dependents_exist",
        "row_name": "Pistol",
        "message": "The row appears to be referenced by external logic or assets."
      }
    ],
    "errors": []
  }
}
```

### 11.3 删除前检查

```text
1. 所有 row 必须存在。
2. row_names 不可为空。
3. 不支持模糊删除。
4. 如果当前版本无法可靠检测外部依赖，可选择：
   A. 不阻断，只记录内部 warning；
   B. Conservative 下 blocked=row_external_dependency_check_unavailable。
```

字段稿允许 blocked external dependents。推荐第一版保守：

```text
如果没有外部引用扫描能力，不声明存在 external dependents。
只做 row 存在性检查。
```

不要虚构依赖。

### 11.4 正式删除流程

```text
1. ResolveDataTable。
2. ResolveRowStruct。
3. Preflight row exists。
4. Snapshot rows。
5. Begin ScopedAssetMutation。
6. RemoveRow for each row.
7. Mark dirty。
8. 返回 removed_count。
```

UE API：

```cpp
DataTable->Modify();
DataTable->RemoveRow(RowName);
DataTable->MarkPackageDirty();
```

### 11.5 成功返回

```json
{
  "schema": "RemoveDataTableRow.v1",
  "row_result": {
    "removed_count": 1
  }
}
```

validation：

```json
{
  "should_compile": false,
  "should_save": true
}
```

---

## 12. Phase K：validation 规则

### 12.1 写成功

DataTable 写工具成功通常：

```json
"validation": {
  "should_compile": false,
  "should_save": true
}
```

### 12.2 no_op

```json
"validation": {
  "should_compile": false,
  "should_save": false
}
```

### 12.3 禁止字段

DataTable validation 不返回：

```text
compiled
saved
```

这与 Graph Write / UMG 等工具不同，DataTable 字段稿已明确收敛。

### 12.4 read 工具

read 工具不返回 validation。

---

## 13. Phase L：Journal / Review 边界

字段稿要求 DataTable 写工具成功不返回：

```text
write_ref
transaction_id
review
safety
```

建议 UE 内部仍按全局写操作规则记录：

```text
Transaction Journal
Review Store
rollback_data
```

但 Agent-facing 成功结果不暴露。

如果当前 Journal 系统尚未覆盖 DataTable，可先做 minimal internal journal：

```text
operation
asset_path
row_names
row_result
before row snapshots
after summary
validation
```

不要把该内部记录输出到普通成功结果。

---

## 14. Phase M：Bridge Router 接入

### 14.1 新增 commands

```text
read_data_table
read_data_table_rows
add_data_table_row
update_data_table_row
update_data_table_rows
remove_data_table_row
remove_data_table_rows
```

### 14.2 Router 分支

```cpp
if (Request.Command == TEXT("read_data_table"))
{
    return HandleReadDataTable(Request);
}
if (Request.Command == TEXT("read_data_table_rows"))
{
    return HandleReadDataTableRows(Request);
}
if (Request.Command == TEXT("add_data_table_row"))
{
    return HandleAddDataTableRow(Request);
}
...
```

### 14.3 Handler 模板

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadDataTable(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result =
        DataTableService.ReadDataTable(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("DataTable tool failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

---

## 15. Phase N：RequestValidator / 权限

### 15.1 read_data_table

```cpp
RequireString(Payload, TEXT("asset_path"));
```

### 15.2 read_data_table_rows

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireArray(Payload, TEXT("row_names"));
```

### 15.3 add_data_table_row

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("row_name"));
RequireObject(Payload, TEXT("values"));
OptionalString(Payload, TEXT("name_collision"));
```

### 15.4 update_data_table_row

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("row_name"));
RequireObject(Payload, TEXT("values"));
```

### 15.5 update_data_table_rows

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireArray(Payload, TEXT("rows")); // [{ row_name, values }]
```

### 15.6 remove_data_table_row / rows

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("row_name")); // single
RequireArray(Payload, TEXT("row_names")); // batch
OptionalBool(Payload, TEXT("dry_run"));
```

### 15.7 权限

写工具：

```text
add_data_table_row
update_data_table_row
update_data_table_rows
remove_data_table_row
remove_data_table_rows
```

需要写权限 / Token，并受 Safety Profile 约束。

读工具：

```text
read_data_table
read_data_table_rows
```

只读，ReadOnly 下允许，不需要 write token。

---

## 16. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperDataTableContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperDataTableReadTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperDataTableWriteTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperDataTableRollbackTests.cpp
```

### 16.1 Contract tests

```text
1. read_data_table_contract
   - schema=ReadDataTable.v1
   - 返回 row_struct / row_count / row_names
   - 不返回 all rows / values

2. read_data_table_rows_contract
   - schema=ReadDataTableRows.v1
   - 只返回请求行 values

3. add_row_success_contract
   - schema=AddDataTableRow.v1
   - added_count=1
   - 不返回 row_name / row_values / write_ref
   - validation 只包含 should_compile / should_save

4. add_row_reuse_no_op_contract
   - status=no_op
   - added_count=0
   - reused_existing=true
   - validation.should_save=false

5. update_row_success_contract
   - schema=UpdateDataTableRow.v1
   - row_result counts only
   - 不返回 before / after / row values

6. remove_row_dry_run_contract
   - schema=RemoveDataTableRowDryRun.v1
   - result/can_execute only

7. remove_row_success_contract
   - schema=RemoveDataTableRow.v1
   - removed_count=1
```

### 16.2 Read tests

```text
1. read_table_summary_does_not_return_rows
2. read_specific_rows
3. read_missing_row_fails
4. read_rows_requires_explicit_names
```

### 16.3 Write tests

```text
1. add_row_fail_if_exists
2. add_row_reuse_if_exists_no_op
3. add_row_invalid_field_fails
4. update_row_partial_fields
5. update_rows_batch_success
6. update_rows_batch_invalid_no_partial_apply
7. remove_row_dry_run_passed
8. remove_row_success
9. remove_rows_batch_success
```

### 16.4 Rollback tests

```text
1. update_batch_write_failure_rolls_back
2. remove_failure_rolls_back
3. add_failure_rolls_back
4. rollback_failed_sets_modified_true
```

---

## 17. 推荐提交顺序

### Commit 1：类型与序列化

```text
Add DataTable DTOs and schemas
Add DataTable scope / row write / error enums
Add validation serializer with only should_compile / should_save
```

验收：

```text
所有 data.schema 使用短命名。
DataTable validation 不输出 compiled/saved。
```

### Commit 2：DataTable resolve + read summary

```text
Add DataTableService skeleton
Resolve UDataTable / RowStruct
Implement read_data_table
```

验收：

```text
read_data_table 只返回 row_struct / row_count / row_names。
```

### Commit 3：read rows

```text
Implement read_data_table_rows
Add StructJsonService StructToJsonObject
Require explicit row_names
```

验收：

```text
只返回请求行 values。
```

### Commit 4：add row

```text
Implement add_data_table_row
Implement name_collision fail/reuse
Implement JsonObjectToStruct
Return added_count only
```

验收：

```text
不支持 auto_rename / replace_existing。
```

### Commit 5：update rows

```text
Implement update_data_table_row / rows
Implement partial field update
Make batch transactional
Return row_result counts only
```

验收：

```text
invalid 时整批失败，不部分应用。
```

### Commit 6：remove rows

```text
Implement remove_data_table_row / rows
Implement dry_run
Implement row snapshot rollback
Return removed_count only
```

验收：

```text
remove 必须支持 dry_run。
```

### Commit 7：Journal / rollback internals

```text
Record internal Journal / Review for DataTable writes
Store row snapshots as rollback_data
Suppress transaction_id in Agent-facing success
```

验收：

```text
成功结果不返回 write_ref / transaction_id。
```

### Commit 8：Bridge / Validator / Auth

```text
Register DataTable commands
Add request validators
Classify read/write permissions
Add tests
```

验收：

```text
读工具 ReadOnly 可用。
写工具需要 token。
```

### Commit 9：Protocol regression

```text
Add contract tests preventing before/after/all rows/write_ref in write success
Add contract tests for validation field shape
```

验收：

```text
字段稿验收项全部通过。
```

---

## 18. 第一版不做的内容

```text
1. 不做 CSV / JSON 全量导入导出。
2. 不返回所有行。
3. 不修改 RowStruct。
4. 不替换 DataTable RowStruct。
5. 不修改 C++ struct/class。
6. 不自动创建缺失 row。
7. 不支持 auto_rename。
8. 不支持 replace_existing。
9. 不返回 before / after。
10. 不返回 write_ref / transaction_id。
11. 不返回 compiled / saved。
12. 不自动 compile/save。
```

---

## 19. 最小验收标准

read_data_table：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_data_table",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "read_scope": "data_table"
  },
  "data": {
    "schema": "ReadDataTable.v1",
    "data_table": {
      "row_struct": "/Script/Game.WeaponRow",
      "row_count": 3,
      "row_names": ["Pistol", "Rifle", "Shotgun"]
    }
  }
}
```

add row：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_data_table_row",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "data_scope": "data_table_row"
  },
  "data": {
    "schema": "AddDataTableRow.v1",
    "row_result": {
      "added_count": 1
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

update rows：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "update_data_table_rows",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "data_scope": "data_table_rows"
  },
  "data": {
    "schema": "UpdateDataTableRow.v1",
    "row_result": {
      "mode": "batch",
      "requested_count": 3,
      "updated_count": 3,
      "changed_count": 2,
      "no_op_count": 1
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

remove dry_run：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_data_table_row",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/Data/DT_Weapons",
    "data_scope": "data_table_row"
  },
  "data": {
    "schema": "RemoveDataTableRowDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

必须不出现：

```text
write_ref
transaction_id
review
safety
before
after
all rows
row_values in write success
compiled
saved
```

---

## 20. 实现风险

### 20.1 UDataTable::AddRow 签名差异

风险：

```text
UE5.3+ DataTable API 细节不同。
```

处理：

```text
封装 FBlueprintHelperDataTableCompat::AddRow / RemoveRow / FindRow。
以当前引擎源码编译校验。
```

### 20.2 RowStruct 复杂字段 JSON 转换失败

风险：

```text
嵌套 UObject / array / map 字段难以安全解析。
```

处理：

```text
第一版只支持明确可转换类型。
不支持字段进入 error.conflicts。
```

### 20.3 批量更新部分成功

风险：

```text
逐行写入中途失败导致半更新。
```

处理：

```text
预校验所有 row/field。
写入前保存 snapshot。
失败时 rollback。
```

### 20.4 read_data_table_rows 被当成全表导出

风险：

```text
row_names 为空时返回所有行，Token 爆炸。
```

处理：

```text
第一版要求显式 row_names。
read_data_table 只返回 row_names。
```

### 20.5 validation 字段混用

风险：

```text
复用 Graph Write validation，意外返回 compiled/saved。
```

处理：

```text
DataTable 使用专用 validation serializer。
Contract test 锁定只返回 should_compile / should_save。
```
