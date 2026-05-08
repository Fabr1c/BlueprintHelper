# BlueprintHelper DataAsset UE 侧 C++ 可执行实现计划

状态：[x] 已完成
日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_DataAsset_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、DataAsset 创建、DataAsset class 修改、C++ struct/class 修改、CSV/JSON 全量导入导出、资产迁移 / Schema migration

---

## 0. 实现目标

实现 DataAsset 第一版工具簇：

```text
read_data_asset_properties
set_data_asset_property
set_data_asset_properties
```

字段契约核心点：

```text
1. DataAsset 第一版不做 CSV / JSON 全量导入导出。
2. DataAsset 第一版不修改 C++ struct/class。
3. read_data_asset_properties 返回 values；写工具不回显 before/after。
4. set_data_asset_property / set_data_asset_properties 成功只返回 property_result 计数。
5. DataAsset 批量属性写默认事务化，invalid 时整批失败。
6. DataAsset 写工具成功不返回 write_ref / transaction_id / review / safety。
7. DataAsset 写工具成功保留 validation，但 validation 只返回 should_compile / should_save。
8. DataAsset 写工具 validation 不返回 compiled / saved。
9. DataAsset 写工具通常 should_compile=false / should_save=true。
10. 所有 data.schema 使用短命名。
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
AssetRegistry
```

DataAsset 属性读取与写入主要依赖 UObject 反射，不需要专门 Editor 模块。若现有保存 / dirty 标记服务已经在 Editor 模块内，则沿用现有依赖。

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
FBlueprintHelperPropertyReflectionService
FBlueprintHelperValidationResult
```

如果当前没有 `PropertyReflectionService`，DataAsset 工具应先抽象该服务，再实现 DataAsset；不要把 FProperty 反射逻辑写死在 DataAsset 工具里。

---

## 2. Phase A：新增类型文件

### 2.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperDataAssetTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperDataAssetTypes.cpp
```

如 DataTable 已经新增通用 `Data` 类型，可复用 `EBlueprintHelperDataScope`、`EBlueprintHelperDataReadScope` 和 `FBlueprintHelperPropertyWriteResult`，但 DataAsset 自身的 schema DTO 建议单独定义，避免字段误串。

### 2.2 新增枚举

```cpp
enum class EBlueprintHelperDataAssetReadScope : uint8
{
    DataAssetProperties
};

enum class EBlueprintHelperDataAssetScope : uint8
{
    DataAssetProperty
};

enum class EBlueprintHelperPropertyWriteMode : uint8
{
    Single,
    Batch
};

enum class EBlueprintHelperDataAssetStage : uint8
{
    ParseInput,
    ResolveAsset,
    ValidateAssetType,
    ValidateProperties,
    SnapshotProperties,
    SetProperty,
    MarkModified,
    Rollback
};

enum class EBlueprintHelperDataAssetErrorCode : uint8
{
    InvalidRequest,
    AssetNotFound,
    TargetNotDataAsset,
    UnsupportedAssetClass,
    PropertyNotFound,
    PropertyNotWritable,
    TypeMismatch,
    ValueOutOfRange,
    UnsupportedPropertyType,
    InvalidDataAssetPropertySettings,
    PropertySetFailed,
    RollbackFailed,
    InternalError
};
```

### 2.3 字符串序列化

稳定输出：

```text
data_asset_properties
data_asset_property
single
batch
invalid_data_asset_property_settings
property_not_found
type_mismatch
```

不要输出 C++ enum 原名。

---

## 3. Phase B：Agent-facing DTO

### 3.1 read_data_asset_properties DTO

```cpp
struct FBlueprintHelperReadDataAssetPropertiesResultData
{
    FString Schema = TEXT("ReadDataAssetProperties.v1");
    FBlueprintHelperDataAssetProperties Properties;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperDataAssetProperties
{
    FString AssetClass;
    int32 PropertyCount = 0;
    TSharedPtr<FJsonObject> Values;

    TSharedRef<FJsonObject> ToJson() const;
};
```

成功示例：

```json
{
  "schema": "ReadDataAssetProperties.v1",
  "properties": {
    "asset_class": "/Script/Game.WeaponConfigDataAsset",
    "property_count": 3,
    "values": {
      "Damage": 25.0,
      "Cooldown": 0.35,
      "DisplayName": "Blaster"
    }
  }
}
```

读取工具允许返回 values，因为这是读取职责。

### 3.2 set_data_asset_property / set_data_asset_properties DTO

```cpp
struct FBlueprintHelperSetDataAssetPropertyResultData
{
    FString Schema = TEXT("SetDataAssetProperty.v1");
    FBlueprintHelperPropertyWriteResult PropertyResult;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperPropertyWriteResult
{
    FString Mode; // single | batch
    int32 RequestedCount = 0;
    int32 AppliedCount = 0;
    int32 ChangedCount = 0;
    int32 NoOpCount = 0;

    TSharedRef<FJsonObject> ToJson() const;
};
```

成功只输出：

```json
{
  "schema": "SetDataAssetProperty.v1",
  "property_result": {
    "mode": "batch",
    "requested_count": 3,
    "applied_count": 3,
    "changed_count": 2,
    "no_op_count": 1
  }
}
```

### 3.3 DataAsset 专用 validation

DataAsset 写工具 validation 只允许：

```cpp
struct FBlueprintHelperDataAssetValidation
{
    bool bShouldCompile = false;
    bool bShouldSave = false;

    TSharedRef<FJsonObject> ToJson() const;
};
```

禁止返回：

```text
compiled
saved
```

### 3.4 明确禁止字段

写工具成功 DTO 不包含：

```cpp
FBlueprintHelperWriteRef
FString TransactionId
TArray<FBlueprintHelperInvalidSetting> InvalidSettings
TSharedPtr<FJsonObject> Before
TSharedPtr<FJsonObject> After
TSharedPtr<FJsonObject> AllProperties
TArray<FString> PropertyPaths
FString ReviewStatus
FString SafetyProfile
```

---

## 4. Phase C：DataAssetService

### 4.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperDataAssetService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperDataAssetService.cpp
```

### 4.2 服务接口

```cpp
class FBlueprintHelperDataAssetService
{
public:
    FBlueprintHelperToolResultBase ReadDataAssetProperties(
        const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase SetDataAssetProperty(
        const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase SetDataAssetProperties(
        const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool ResolveDataAsset(
        const FString& AssetPath,
        UObject*& OutDataAsset,
        FBlueprintHelperToolError& OutError) const;

    bool IsSupportedDataAsset(UObject* Asset) const;

    FBlueprintHelperToolResultBase ExecuteSetProperties(
        const FBlueprintHelperSetDataAssetPropertiesRequest& Request,
        bool bIsBatch) const;
};
```

### 4.3 DataAsset 类型判断

第一版建议允许：

```cpp
Asset->IsA<UDataAsset>()
Asset->IsA<UPrimaryDataAsset>()
```

也可允许 Blueprint 派生 DataAsset 实例，只要最终资产对象是 `UDataAsset` 或子类。

失败映射：

```text
asset_not_found
target_not_data_asset
unsupported_asset_class
```

---

## 5. Phase D：请求结构

### 5.1 read_data_asset_properties request

```cpp
struct FBlueprintHelperReadDataAssetPropertiesRequest
{
    FString AssetPath;

    // 建议支持显式属性读取，避免默认深展开。
    TArray<FString> PropertyNames;

    // 可选，默认 false。
    bool bIncludeDefaults = false;

    // 可选，默认 1。
    int32 MaxDepth = 1;
};
```

### 5.2 read 输入 JSON

```json
{
  "asset_path": "/Game/Data/DA_WeaponConfig",
  "property_names": ["Damage", "Cooldown", "DisplayName"],
  "max_depth": 1
}
```

字段稿允许读取“指定属性或属性摘要”。第一版建议：

```text
property_names 为空时，返回浅层可序列化普通属性摘要。
不要深展开复杂对象引用。
```

如果 Token 控制更严格，也可要求 property_names 显式提供；但字段稿示例允许直接返回 values，因此第一版可实现“浅层所有简单属性”。

### 5.3 set_data_asset_property request

```cpp
struct FBlueprintHelperSetDataAssetPropertyRequest
{
    FString AssetPath;
    FString PropertyPath;
    TSharedPtr<FJsonValue> Value;
};
```

输入：

```json
{
  "asset_path": "/Game/Data/DA_WeaponConfig",
  "property": "Damage",
  "value": 25.0
}
```

兼容字段名：

```text
property_path
property
```

内部统一为 `PropertyPath`。

### 5.4 set_data_asset_properties request

```cpp
struct FBlueprintHelperSetDataAssetPropertyItem
{
    FString PropertyPath;
    TSharedPtr<FJsonValue> Value;
};

struct FBlueprintHelperSetDataAssetPropertiesRequest
{
    FString AssetPath;
    TArray<FBlueprintHelperSetDataAssetPropertyItem> Items;
};
```

输入：

```json
{
  "asset_path": "/Game/Data/DA_WeaponConfig",
  "properties": [
    { "property": "Damage", "value": 25.0 },
    { "property": "Cooldown", "value": 0.35 },
    { "property": "DisplayName", "value": "Blaster" }
  ]
}
```

---

## 6. Phase E：属性反射服务

### 6.1 复用 / 新增 PropertyReflectionService

```cpp
class FBlueprintHelperPropertyReflectionService
{
public:
    bool ReadObjectPropertiesToJson(
        UObject* Object,
        const TArray<FString>& PropertyPaths,
        int32 MaxDepth,
        TSharedPtr<FJsonObject>& OutValues,
        TArray<FBlueprintHelperConflictItem>& OutConflicts) const;

    bool ValidateObjectPropertySet(
        UObject* Object,
        const FString& PropertyPath,
        const TSharedPtr<FJsonValue>& Value,
        FBlueprintHelperPropertyValidationResult& OutResult) const;

    bool SetObjectPropertyFromJson(
        UObject* Object,
        const FString& PropertyPath,
        const TSharedPtr<FJsonValue>& Value,
        FBlueprintHelperPropertySetResult& OutResult) const;

    bool SnapshotObjectProperties(
        UObject* Object,
        const TArray<FString>& PropertyPaths,
        FBlueprintHelperObjectPropertySnapshot& OutSnapshot,
        FString& OutError) const;

    bool RestoreObjectProperties(
        UObject* Object,
        const FBlueprintHelperObjectPropertySnapshot& Snapshot,
        FString& OutError) const;
};
```

### 6.2 支持属性类型

第一版建议支持：

```text
bool
integer
float / double
FString
FName
FText
enum
FVector / FRotator / FTransform / FLinearColor 等常见 UStruct
TSoftObjectPtr / FSoftObjectPath 字符串
simple arrays
```

第一版阻断：

```text
复杂 UObject* 强引用
复杂嵌套 Map
不安全对象创建
Class 默认对象修改
Transient / NonEdit 属性
BlueprintReadOnly 且非 EditAnywhere 属性
```

失败进入：

```text
error.conflicts[].code = property_not_writable / unsupported_property_type / type_mismatch
```

### 6.3 可写性判断

属性必须满足：

```text
存在
非 transient
非 deprecated
可编辑或允许工具写入
不是只读 / const
```

C++ 参考：

```cpp
FProperty* Property = FindFPropertyByPath(Object->GetClass(), PropertyPath);
if (!Property || Property->HasAnyPropertyFlags(CPF_Transient))
{
    // invalid
}
```


---

## 7. Phase F：read_data_asset_properties 实现

### 7.1 执行流程

```text
1. ParseRequest。
2. ResolveDataAsset。
3. 解析 property_names / max_depth。
4. 读取指定属性或浅层简单属性。
5. 返回 asset_class / property_count / values。
```

### 7.2 asset_class

```cpp
Properties.AssetClass = DataAsset->GetClass()->GetPathName();
```

对于 Blueprint Generated Class，可能得到：

```text
/Game/Data/BP_WeaponConfig.BP_WeaponConfig_C
```

如果字段稿希望 `/Script/Game.WeaponConfigDataAsset`，则：

```text
C++ DataAsset class 返回 /Script/...
Blueprint DataAsset class 返回 GeneratedClass path 或 class path。
```

不要伪造 `/Script` 路径。

### 7.3 values 输出

```cpp
TSharedPtr<FJsonObject> Values;
PropertyReflectionService.ReadObjectPropertiesToJson(
    DataAsset,
    Request.PropertyNames,
    Request.MaxDepth,
    Values,
    Conflicts);
```

如果某个请求属性不存在：

```text
ok=false
status=failed
error.code=property_not_found
stage=validate_properties
conflicts[].property
```

读取工具不返回 validation。

---

## 8. Phase G：set_data_asset_property / set_data_asset_properties 实现

### 8.1 单属性统一为批量

```cpp
FBlueprintHelperSetDataAssetPropertiesRequest BatchRequest;
BatchRequest.AssetPath = Request.AssetPath;
BatchRequest.Items.Add({ Request.PropertyPath, Request.Value });
return ExecuteSetProperties(BatchRequest, false);
```

### 8.2 批量执行流程

```text
1. ParseRequest。
2. ResolveDataAsset。
3. 对所有 item 做预校验：
   - property 存在
   - property 可写
   - JSON value 可转换
4. 任一 invalid：整批失败，不写任何属性。
5. Snapshot properties。
6. Begin ScopedAssetMutation。
7. Apply all properties。
8. 统计 requested/applied/changed/no_op。
9. MarkPackageDirty。
10. 内部 Journal / Review 记录。
11. 返回 property_result + validation。
```

### 8.3 事务化批量

预校验阶段不能写入。  
写入中任一失败：

```text
尝试 RestoreObjectProperties。
恢复成功：modified=false。
恢复失败：modified=true，error.code=rollback_failed。
```

### 8.4 changed/no_op 计算

`FBlueprintHelperPropertySetResult` 应包含：

```cpp
bool bChanged;
bool bApplied;
```

如果新值与旧值一致：

```text
applied_count += 1
changed_count += 0
no_op_count += 1
```

如果所有都是 no_op：

```text
status=no_op
modified=false
validation.should_compile=false
validation.should_save=false
```

字段稿示例只给单属性 no_op，批量全 no_op 可同理返回 no_op。

如果部分 changed，部分 no_op：

```text
status=applied
modified=true
```

### 8.5 成功返回

单属性：

```json
{
  "schema": "SetDataAssetProperty.v1",
  "property_result": {
    "mode": "single",
    "requested_count": 1,
    "applied_count": 1,
    "changed_count": 1,
    "no_op_count": 0
  }
}
```

批量：

```json
{
  "schema": "SetDataAssetProperty.v1",
  "property_result": {
    "mode": "batch",
    "requested_count": 3,
    "applied_count": 3,
    "changed_count": 2,
    "no_op_count": 1
  }
}
```

### 8.6 validation

成功：

```json
"validation": {
  "should_compile": false,
  "should_save": true
}
```

no_op：

```json
"validation": {
  "should_compile": false,
  "should_save": false
}
```

禁止返回：

```text
compiled
saved
```

---

## 9. Phase H：失败处理

### 9.1 批量 invalid

批量 property 写默认事务化，只要存在 invalid 项，整批失败。

返回：

```text
ok=false
status=failed
modified=false
error.code=invalid_data_asset_property_settings
stage=validate_properties
error.conflicts[] 问题项
```

不返回：

```text
property_result
validation
write_ref
transaction_id
before
after
```

### 9.2 conflicts 字段建议

```json
{
  "code": "property_not_found",
  "property": "DamageWrong"
}
```

```json
{
  "code": "type_mismatch",
  "property": "Cooldown",
  "expected_type": "float"
}
```

```json
{
  "code": "property_not_writable",
  "property": "InternalId"
}
```

### 9.3 写入中失败

```text
error.code=property_set_failed
stage=set_property
rollback_result=rolled_back? 
```

字段稿 error 表没有 rollback_result。  

```text
写入中失败并成功回滚：
  ok=false
  status=failed
  modified=false
  error.code=property_set_failed

rollback failed：
  ok=false
  status=failed
  modified=true
  error.code=rollback_failed
  error.stage=rollback
```

不强制返回 rollback_result，除非 ToolResultBase error 通用结构已经支持。字段稿未要求，Agent-facing 可省略。

---

## 10. Phase I：Journal / Review 边界

字段稿要求 DataAsset 写工具成功不返回：

```text
write_ref
transaction_id
review
safety
```

建议 UE 内部仍记录：

```text
Transaction Journal
Review Store
rollback_data
```

内部 Journal 可记录：

```json
{
  "operation": "set_data_asset_properties",
  "target_assets": ["/Game/Data/DA_WeaponConfig"],
  "property_summary": {
    "requested_count": 3,
    "applied_count": 3,
    "changed_count": 2,
    "no_op_count": 1
  },
  "before_snapshot_ref": "...",
  "after_summary": {
    "changed_properties": ["Damage", "Cooldown"]
  },
  "rollback_data": {
    "type": "data_asset_property_snapshot",
    "properties": ["Damage", "Cooldown", "DisplayName"]
  }
}
```

不要把 transaction_id / write_ref 输出给 Agent-facing 成功结果。

如果当前 Journal 系统未覆盖 DataAsset，可先实现 minimal internal journal，但不影响字段结果。

---

## 11. Phase J：Bridge Router 接入

### 11.1 新增 commands

```text
read_data_asset_properties
set_data_asset_property
set_data_asset_properties
```

### 11.2 Router 分支

```cpp
if (Request.Command == TEXT("read_data_asset_properties"))
{
    return HandleReadDataAssetProperties(Request);
}
if (Request.Command == TEXT("set_data_asset_property"))
{
    return HandleSetDataAssetProperty(Request);
}
if (Request.Command == TEXT("set_data_asset_properties"))
{
    return HandleSetDataAssetProperties(Request);
}
```

### 11.3 Handler 模板

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSetDataAssetProperties(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result =
        DataAssetService.SetDataAssetProperties(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("DataAsset tool failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

---

## 12. Phase K：RequestValidator / 权限

### 12.1 read_data_asset_properties

```cpp
RequireString(Payload, TEXT("asset_path"));
OptionalArray(Payload, TEXT("property_names"));
OptionalBool(Payload, TEXT("include_defaults"));
OptionalInt(Payload, TEXT("max_depth"));
```

### 12.2 set_data_asset_property

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("property")); // or property_path
RequireAny(Payload, TEXT("value"));
```

### 12.3 set_data_asset_properties

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireArray(Payload, TEXT("properties")); // [{ property, value }]
```

### 12.4 权限

读工具：

```text
read_data_asset_properties
```

只读，ReadOnly 下允许，不需要 write token。

写工具：

```text
set_data_asset_property
set_data_asset_properties
```

需要写权限 / Token，并受 Safety Profile 约束。

---

## 13. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperDataAssetContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperDataAssetReadTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperDataAssetWriteTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperDataAssetRollbackTests.cpp
```

### 13.1 Contract tests

```text
1. read_data_asset_properties_contract
   - operation=read_data_asset_properties
   - data.schema=ReadDataAssetProperties.v1
   - properties.asset_class 存在
   - property_count 存在
   - values 存在
   - 不返回 validation

2. set_data_asset_property_success_contract
   - operation=set_data_asset_property
   - data.schema=SetDataAssetProperty.v1
   - property_result only
   - validation 只包含 should_compile / should_save
   - 不返回 before / after / all_properties / write_ref / transaction_id

3. set_data_asset_properties_success_contract
   - mode=batch
   - requested/applied/changed/no_op counts
   - 不返回 invalid_settings

4. set_data_asset_property_no_op_contract
   - status=no_op
   - modified=false
   - changed_count=0
   - no_op_count=1
   - validation.should_save=false

5. set_data_asset_properties_invalid_contract
   - ok=false
   - status=failed
   - error.conflicts[] 存在
   - 不返回 property_result
```

### 13.2 Read tests

```text
1. read_explicit_properties
2. read_simple_properties_without_deep_object_expansion
3. read_missing_property_fails
4. read_non_data_asset_fails
```

### 13.3 Write tests

```text
1. set_float_property
2. set_text_or_string_property
3. set_enum_property
4. set_soft_object_path_property
5. set_batch_properties_success
6. set_batch_invalid_no_partial_apply
7. set_same_value_no_op
8. set_readonly_property_fails
9. set_unsupported_property_type_fails
```

### 13.4 Rollback tests

```text
1. batch_write_failure_rolls_back
2. property_set_failure_rolls_back
3. rollback_failed_sets_modified_true
4. journal_failure_rolls_back_if_internal_journal_enabled
```

---

## 14. 推荐提交顺序

### Commit 1：类型与序列化

```text
Add DataAsset DTOs and short schemas
Add data asset scope / stage / error enums
Add DataAsset validation serializer with only should_compile / should_save
```

验收：

```text
写工具结果不会输出 compiled/saved。
写工具结果不会输出 write_ref / transaction_id。
```

### Commit 2：DataAsset resolve + read

```text
Add DataAssetService skeleton
Resolve UDataAsset / UPrimaryDataAsset
Implement read_data_asset_properties
```

验收：

```text
读取返回 asset_class / property_count / values。
读工具不返回 validation。
```

### Commit 3：PropertyReflectionService

```text
Implement object property read/write helpers
Support simple property types
Validate writable properties
```

验收：

```text
property_not_found / type_mismatch / property_not_writable 可稳定返回。
```

### Commit 4：单属性写入

```text
Implement set_data_asset_property
Snapshot before value
Apply value
Return property_result counts
Handle no_op
```

验收：

```text
成功只返回 property_result。
no_op validation.should_save=false。
```

### Commit 5：批量事务写入

```text
Implement set_data_asset_properties
Prevalidate all items
Apply transactionally
Rollback on failure
```

验收：

```text
invalid 时整批失败，不部分应用。
```

### Commit 6：Internal Journal / rollback integration

```text
Record internal Journal / Review for DataAsset writes
Store property snapshots as rollback_data
Suppress transaction_id in Agent-facing success
```

验收：

```text
Agent-facing 成功结果不返回 transaction_id。
内部 Journal 可用于 Review/rollback。
```

### Commit 7：Bridge / Validator / Auth

```text
Register DataAsset commands
Add request validators
Classify read/write permissions
Add tests
```

验收：

```text
读工具 ReadOnly 可用。
写工具需要 token。
```

### Commit 8：Protocol regression

```text
Add contract tests preventing before/after/all_properties/write_ref in success
Add validation shape tests
```

验收：

```text
字段稿验收项全部通过。
```

---

## 15. 第一版不做的内容

```text
1. 不创建 DataAsset。
2. 不修改 DataAsset class。
3. 不修改 C++ class / struct。
4. 不做 CSV / JSON 全量导入导出。
5. 不做资产迁移 / Schema migration。
6. 不深展开复杂对象引用。
7. 不返回 before / after。
8. 不返回 all_properties。
9. 不返回 invalid_settings。
10. 不返回 write_ref / transaction_id。
11. 不返回 compiled / saved。
12. 不自动 compile/save。
```

---

## 16. 最小验收标准

read：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_data_asset_properties",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "/Game/Data/DA_WeaponConfig",
    "read_scope": "data_asset_properties"
  },
  "data": {
    "schema": "ReadDataAssetProperties.v1",
    "properties": {
      "asset_class": "/Script/Game.WeaponConfigDataAsset",
      "property_count": 3,
      "values": {
        "Damage": 25.0,
        "Cooldown": 0.35,
        "DisplayName": "Blaster"
      }
    }
  }
}
```

single write：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_data_asset_property",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/Data/DA_WeaponConfig",
    "data_scope": "data_asset_property"
  },
  "data": {
    "schema": "SetDataAssetProperty.v1",
    "property_result": {
      "mode": "single",
      "requested_count": 1,
      "applied_count": 1,
      "changed_count": 1,
      "no_op_count": 0
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

batch write:

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_data_asset_properties",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/Data/DA_WeaponConfig",
    "data_scope": "data_asset_property"
  },
  "data": {
    "schema": "SetDataAssetProperty.v1",
    "property_result": {
      "mode": "batch",
      "requested_count": 3,
      "applied_count": 3,
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

必须不出现：

```text
write_ref
transaction_id
review
safety
before
after
all_properties
invalid_settings
compiled
saved
```

---

## 17. 实现风险

### 17.1 DataAsset class 判断过窄

风险：

```text
Blueprint 生成的 DataAsset 实例 class path 不同于 /Script 原生 class。
```

处理：

```text
使用 IsA<UDataAsset>() / IsA<UPrimaryDataAsset>() 判断，不用路径字符串判断。
```

### 17.2 复杂属性误写

风险：

```text
```

处理：

```text
第一版只支持白名单类型。
复杂类型返回 unsupported_property_type。
```

### 17.3 批量写部分成功

风险：

```text
逐项写入中途失败造成半配置。
```

处理：

```text
预校验全部属性。
写入前 snapshot。
失败时 rollback。
```

### 17.4 validation 字段混用

风险：

```text
复用 Graph Write validation，意外返回 compiled/saved。
```

处理：

```text
DataAsset 使用专用 validation serializer。
Contract test 锁定只返回 should_compile / should_save。
```

### 17.5 read 过度展开

风险：

```text
读取复杂对象引用或深层数组导致 Token 暴涨。
```

处理：

```text
默认 max_depth=1。
复杂对象引用输出简短路径或阻断。
不做深层全量导出。
```
