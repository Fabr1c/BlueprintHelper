# BlueprintHelper Blueprint Variables / Defaults / Local Variables UE 侧 C++ 可执行实现计划

状态：[x] 已完成
日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_BlueprintVariables_Defaults_LocalVariables_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、变量重命名、变量类型变更、Graph 引用自动迁移、C++ class/struct 修改

---

## 0. 实现目标

实现 Blueprint Variables / Defaults / Local Variables 第一版工具簇，分为三层：

```text
1. Blueprint Member Variable Declaration
   成员变量声明：变量名、类型、Category、Tooltip、Instance Editable、Expose on Spawn 等。

2. Blueprint Member Defaults
   成员变量默认值：Blueprint Class Defaults 中对应变量的默认值。

3. Blueprint Local Variables
   函数作用域内局部变量：必须绑定明确 function_name。
```

第一版工具清单：

```text
read_blueprint_member_variables
add_blueprint_member_variable
add_blueprint_member_variables
set_blueprint_member_variable_properties
set_blueprint_member_variables_properties
remove_blueprint_member_variable
remove_blueprint_member_variables

read_blueprint_member_defaults
set_blueprint_member_default
set_blueprint_member_defaults

read_blueprint_local_variables
add_blueprint_local_variable
add_blueprint_local_variables
set_blueprint_local_variable_properties
set_blueprint_local_variables_properties
remove_blueprint_local_variable
remove_blueprint_local_variables
```

核心字段契约：

```text
1. 成员变量声明、默认值、Local Variables 分工具。
2. add_blueprint_member_variable 只创建变量声明，不设置默认值。
3. 默认值通过 set_blueprint_member_default(s) 设置。
4. read_blueprint_member_variables 不默认返回默认值。
5. read_blueprint_member_defaults 只读取请求的成员变量默认值。
6. 成员变量 name_collision 只支持 fail_if_exists / reuse_if_exists。
7. 不支持 auto_rename / replace_existing。
8. 第一版不支持 rename member/local variable。
9. 第一版不支持 change member/local variable type。
10. set_blueprint_member_variable_properties 不允许修改 variable_name / variable_type。
11. remove_blueprint_member_variable 必须 dry_run。
12. remove member variable 有 Graph 引用时 blocked。
13. Local Variable 必须指定 function_name。
14. Local Variable 不支持 instance_editable / expose_on_spawn / replication / class default。
15. remove_blueprint_local_variable 必须 dry_run。
16. remove local variable 有函数图引用时 blocked。
17. 批量写默认事务化，任一 invalid 整批失败。
18. 写工具 validation 只返回 should_compile / should_save。
19. 写工具成功不返回 write_ref / transaction_id / review / safety。
20. data.schema 使用短命名。
21. 单个变量 add / set / remove 成功只返回 success=true。
22. 单个变量操作不返回 mode / requested_count / added_count / removed_count / changed_count / no_op_count。
23. 单个 no_op 通过 status=no_op 表达，不额外返回 reused_existing。
24. 批量变量操作可保留 requested_count / added_count / removed_count / changed_count / no_op_count 等计数。
25. 批量变量操作不需要 mode=batch，因为 operation 名已经区分。
```

---

## 1. UE 模块依赖

确认 `BlueprintHelper.Build.cs` 至少包含：

```text
Core
CoreUObject
Engine
UnrealEd
BlueprintGraph
Kismet
KismetCompiler
Json
JsonUtilities
```

建议确认下列头文件可用：

```cpp
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/UnrealType.h"
```

成员变量和 Local Variable 操作应优先使用 `FBlueprintEditorUtils`，避免手写 Blueprint 内部数组。

---

## 2. Phase A：新增类型文件

### 2.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperBlueprintVariableTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperBlueprintVariableTypes.cpp
```

### 2.2 新增枚举

```cpp
enum class EBlueprintHelperVariableScope : uint8
{
    Member,
    Local
};

enum class EBlueprintHelperDefaultScope : uint8
{
    MemberDefault,
    MemberDefaults
};

enum class EBlueprintHelperVariableReadScope : uint8
{
    MemberVariables,
    MemberDefaults,
    LocalVariables
};

enum class EBlueprintHelperVariableNameCollisionPolicy : uint8
{
    FailIfExists,
    ReuseIfExists
};

enum class EBlueprintHelperVariableStage : uint8
{
    ParseInput,
    ResolveAsset,
    ResolveBlueprint,
    ResolveFunction,
    ResolveVariable,
    ResolveVariableType,
    NameCollisionCheck,
    ValidateVariableDeclaration,
    ValidateVariableProperties,
    ValidateDefaults,
    ValidateReferences,
    DryRun,
    SnapshotBefore,
    AddVariable,
    SetProperties,
    SetDefaults,
    RemoveVariable,
    MarkModified,
    Rollback
};

enum class EBlueprintHelperVariableErrorCode : uint8
{
    InvalidRequest,
    AssetNotFound,
    TargetNotBlueprint,
    FunctionNotFound,
    FunctionGraphNotFound,
    VariableNotFound,
    LocalVariableNotFound,
    VariableAlreadyExists,
    LocalVariableAlreadyExists,
    UnsupportedNameCollisionPolicy,
    UnsupportedVariableType,
    UnsupportedVariableContainer,
    VariableTypeChangeUnsupported,
    VariableRenameUnsupported,
    InvalidMemberVariableSettings,
    InvalidMemberDefaultSettings,
    InvalidLocalVariableSettings,
    VariableReferencesExist,
    LocalVariableReferencesExist,
    RemoveVariableDryRunRequired,
    TypeMismatch,
    PropertyNotWritable,
    VariableAddFailed,
    VariablePropertySetFailed,
    VariableDefaultSetFailed,
    VariableRemoveFailed,
    RollbackFailed,
    InternalError
};
```

### 2.3 字符串序列化

稳定输出：

```text
member
local
member_variables
member_defaults
local_variables
member_default
member_defaults
fail_if_exists
reuse_if_exists
variable_already_exists
variable_references_exist
local_variable_references_exist
invalid_member_default_settings
```

不要输出 C++ enum 原名。

---

## 3. Phase B：Agent-facing DTO

### 3.1 variable_type

```cpp
struct FBlueprintHelperVariableType
{
    FString Category;  // bool | int | float | object | class | struct | enum | map ...
    TOptional<FString> Subtype;   // class / struct / enum / object path when relevant
    FString Container = TEXT("single"); // single | array | set | map

    TOptional<TSharedPtr<FBlueprintHelperVariableType>> KeyType;
    TOptional<TSharedPtr<FBlueprintHelperVariableType>> ValueType;

    TSharedRef<FJsonObject> ToJson() const;
};
```

示例：

```json
{
  "category": "object",
  "subtype": "/Script/Engine.StaticMeshComponent",
  "container": "single"
}
```

Map 示例：

```json
{
  "category": "map",
  "key_type": { "category": "name" },
  "value_type": { "category": "float" }
}
```

第一版建议：

```text
1. 支持 primitive / object / class / struct / enum / array。
2. set / map 可先只读或明确返回 unsupported_variable_container。
3. 不暴露完整 FEdGraphPinType。
4. 类型变更不支持。
```

### 3.2 变量摘要

```cpp
struct FBlueprintHelperVariableSummary
{
    FString VariableName;
    FBlueprintHelperVariableType VariableType;

    TOptional<FString> Category;
    TOptional<FString> Tooltip;

    // member only
    TOptional<bool> bInstanceEditable;
    TOptional<bool> bExposeOnSpawn;
    TOptional<bool> bPrivate;
    TOptional<bool> bBlueprintReadOnly;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.3 read_blueprint_member_variables DTO

```cpp
struct FBlueprintHelperReadMemberVariablesData
{
    FString Schema = TEXT("ReadBlueprintMemberVariables.v1");
    FBlueprintHelperMemberVariablesPayload MemberVariables;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperMemberVariablesPayload
{
    int32 VariableCount = 0;
    TArray<FBlueprintHelperVariableSummary> Variables;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.4 read_blueprint_member_defaults DTO

```cpp
struct FBlueprintHelperReadMemberDefaultsData
{
    FString Schema = TEXT("ReadBlueprintMemberDefaults.v1");
    FBlueprintHelperMemberDefaultsPayload Defaults;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperMemberDefaultsPayload
{
    int32 DefaultCount = 0;
    TSharedPtr<FJsonObject> Values;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.5 read_blueprint_local_variables DTO

```cpp
struct FBlueprintHelperReadLocalVariablesData
{
    FString Schema = TEXT("ReadBlueprintLocalVariables.v1");
    FBlueprintHelperLocalVariablesPayload LocalVariables;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperLocalVariablesPayload
{
    int32 VariableCount = 0;
    TArray<FBlueprintHelperVariableSummary> Variables;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.6 单变量成功 DTO

单个变量 add / set / remove 成功只返回：

```cpp
struct FBlueprintHelperSingleVariableResultData
{
    FString Schema; // AddBlueprintMemberVariable.v1 / RemoveBlueprintLocalVariable.v1 ...
    FBlueprintHelperSingleSuccessResult VariableResult;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperSingleSuccessResult
{
    bool bSuccess = true;

    TSharedRef<FJsonObject> ToJson() const;
};
```

Default 单项成功：

```cpp
struct FBlueprintHelperSingleDefaultResultData
{
    FString Schema = TEXT("SetBlueprintMemberDefault.v1");
    FBlueprintHelperSingleSuccessResult DefaultResult;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.7 批量结果 DTO

```cpp
struct FBlueprintHelperBatchVariableResult
{
    int32 RequestedCount = 0;

    TOptional<int32> AddedCount;
    TOptional<int32> RemovedCount;
    TOptional<int32> AppliedCount;
    TOptional<int32> ChangedCount;
    TOptional<int32> NoOpCount;

    TSharedRef<FJsonObject> ToJson() const;
};
```

注意：

```text
批量结果不返回 mode=batch。
单变量结果不返回任何 count。
```

### 3.8 dry_run DTO

```cpp
struct FBlueprintHelperVariableDryRunData
{
    FString Schema; // RemoveBlueprintMemberVariableDryRun.v1 / RemoveBlueprintLocalVariableDryRun.v1
    FBlueprintHelperDryRunPayload DryRun;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.9 明确禁止字段

所有变量写工具成功 DTO 不包含：

```text
mode
variable_guid
before
after
all_variables
all_defaults
write_ref
transaction_id
journal_recorded
review
safety
safety_profile
reused_existing
invalid_settings
conflicts=[]
```

---

## 4. Phase C：VariableType 转换服务

### 4.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperVariableTypeService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperVariableTypeService.cpp
```

### 4.2 服务接口

```cpp
class FBlueprintHelperVariableTypeService
{
public:
    bool FromJson(
        const TSharedPtr<FJsonObject>& Json,
        FEdGraphPinType& OutPinType,
        FBlueprintHelperToolError& OutError) const;

    FBlueprintHelperVariableType ToPublicType(
        const FEdGraphPinType& PinType) const;

    bool ValidateSupportedForWrite(
        const FBlueprintHelperVariableType& PublicType,
        FBlueprintHelperToolError& OutError) const;
};
```

### 4.3 category 映射

建议映射：

```text
bool      -> PC_Boolean
int       -> PC_Int
int64     -> PC_Int64
float     -> PC_Real + float subtype
double    -> PC_Real + double subtype
name      -> PC_Name
string    -> PC_String
text      -> PC_Text
vector    -> PC_Struct /Script/CoreUObject.Vector
rotator   -> PC_Struct /Script/CoreUObject.Rotator
transform -> PC_Struct /Script/CoreUObject.Transform
object    -> PC_Object + subtype class
class     -> PC_Class + subtype class
struct    -> PC_Struct + subtype struct
enum      -> PC_Byte or PC_Enum depending UE API
```

### 4.4 container 映射

```text
single -> None
array  -> EPinContainerType::Array
set    -> EPinContainerType::Set
map    -> EPinContainerType::Map
```

第一版如果不支持 set/map 写入：

```text
set/map read 时可返回。
write 时 unsupported_variable_container。
```

### 4.5 不暴露 FEdGraphPinType 全字段

Agent-facing 不返回：

```text
PinCategory
PinSubCategory
PinSubCategoryObject raw
bIsReference
bIsConst
bIsWeakPointer
bIsUObjectWrapper
```

这些由 UE 内部处理。

---

## 5. Phase D：BlueprintVariableService

### 5.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperBlueprintVariableService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperBlueprintVariableService.cpp
```

### 5.2 服务接口

```cpp
class FBlueprintHelperBlueprintVariableService
{
public:
    FBlueprintHelperToolResultBase ReadMemberVariables(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase AddMemberVariable(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase AddMemberVariables(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase SetMemberVariableProperties(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase SetMemberVariablesProperties(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase RemoveMemberVariable(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase RemoveMemberVariables(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase ReadMemberDefaults(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase SetMemberDefault(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase SetMemberDefaults(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase ReadLocalVariables(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase AddLocalVariable(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase AddLocalVariables(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase SetLocalVariableProperties(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase SetLocalVariablesProperties(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase RemoveLocalVariable(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase RemoveLocalVariables(const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool ResolveBlueprint(
        const FString& AssetPath,
        UBlueprint*& OutBlueprint,
        FBlueprintHelperToolError& OutError) const;

    bool ResolveFunctionGraph(
        UBlueprint* Blueprint,
        const FString& FunctionName,
        UEdGraph*& OutFunctionGraph,
        FBlueprintHelperToolError& OutError) const;
};
```

---

## 6. Phase E：资产 / 蓝图 / 函数解析

### 6.1 ResolveBlueprint

```cpp
UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
```

失败：

```text
asset_not_found
target_not_blueprint
```

### 6.2 ResolveFunctionGraph

Local Variable 工具必须指定 `function_name`。

```cpp
UEdGraph* FunctionGraph = FBlueprintEditorUtils::FindScopeGraph(Blueprint, FName(*FunctionName));
```

或遍历：

```cpp
Blueprint->FunctionGraphs
```

失败：

```text
function_not_found
function_graph_not_found
```

禁止：

```text
只用当前编辑器焦点
只用选中图表
只指定 graph_name 代替 function_name
```

---

## 7. Phase F：read_blueprint_member_variables

### 7.1 Request

```cpp
struct FBlueprintHelperReadMemberVariablesRequest
{
    FString AssetPath;
};
```

### 7.2 实现

使用：

```cpp
TArray<FBPVariableDescription> Variables = Blueprint->NewVariables;
```

或 `FBlueprintEditorUtils` 相关 helper。

对每个变量输出：

```text
variable_name
variable_type
category
tooltip
instance_editable
expose_on_spawn
private
blueprint_read_only
```

不输出：

```text
default_value
variable_guid
replication details unless后续确认
```

### 7.3 成功

```text
ok=true
status=completed
modified=false
data.schema=ReadBlueprintMemberVariables.v1
```

不返回 validation。

---

## 8. Phase G：add_blueprint_member_variable(s)

### 8.1 Request

```cpp
struct FBlueprintHelperAddMemberVariableRequest
{
    FString AssetPath;
    FString VariableName;
    FBlueprintHelperVariableType VariableType;

    EBlueprintHelperVariableNameCollisionPolicy NameCollisionPolicy =
        EBlueprintHelperVariableNameCollisionPolicy::FailIfExists;

    TOptional<FString> Category;
    TOptional<FString> Tooltip;
    TOptional<bool> bInstanceEditable;
    TOptional<bool> bExposeOnSpawn;
    TOptional<bool> bPrivate;
    TOptional<bool> bBlueprintReadOnly;
};
```

Batch：

```cpp
struct FBlueprintHelperAddMemberVariablesRequest
{
    FString AssetPath;
    TArray<FBlueprintHelperAddMemberVariableRequestItem> Variables;
};
```

### 8.2 执行边界

add 只创建声明，不设置默认值。  
即使 payload 传入 `default_value`，第一版也应：

```text
invalid_request 或忽略? 推荐 invalid_request。
```

避免 Agent 误以为默认值已设置。

### 8.3 Name collision

```text
fail_if_exists + exists → failed / variable_already_exists
reuse_if_exists + exists → no_op（单个）或 no_op_count++（批量）
```

不支持：

```text
auto_rename
replace_existing
```

### 8.4 批量事务化

批量 add 必须先预校验全部：

```text
1. 变量名合法。
2. 不存在非法重复。
3. 类型均支持。
4. name_collision 均合法。
5. 不存在 batch 内部重名冲突。
```

任一 invalid：

```text
ok=false
status=failed
modified=false
error.code=invalid_member_variable_settings
error.conflicts[]
不添加任何变量
```

### 8.5 UE API

添加成员变量：

```cpp
FBlueprintEditorUtils::AddMemberVariable(
    Blueprint,
    FName(*VariableName),
    PinType,
    DefaultValueString);
```

注意：

```text
DefaultValueString 应为空或类型默认值。
不要把 Agent default_value 写入这里。
```

设置元数据 / 属性：

```cpp
FBPVariableDescription* VarDesc = FindVariableDescription(Blueprint, VariableName);
VarDesc->Category = ...
VarDesc->PropertyFlags |= CPF_Edit
Blueprint->Modify();
FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
```

具体 flags 建议封装：

```text
SetMemberVariableFlags(Blueprint, VarName, Properties)
```

### 8.6 成功返回

单个：

```json
"variable_result": { "success": true }
```

批量：

```json
"variable_result": {
  "requested_count": 3,
  "added_count": 3,
  "no_op_count": 0
}
```

validation：

```text
should_compile=true
should_save=true
```

no_op 单个：

```text
status=no_op
modified=false
validation.should_compile=false
validation.should_save=false
variable_result.success=true
```

不返回 `reused_existing`。

---

## 9. Phase H：set_blueprint_member_variable_properties

### 9.1 支持属性

第一版支持：

```text
category
tooltip
instance_editable
expose_on_spawn
private
blueprint_read_only
```

明确不支持：

```text
variable_name
variable_type
default_value
replication
```

如果 payload 中出现不支持字段：

```text
ok=false
status=failed
error.code=invalid_member_variable_settings
error.conflicts[].code=property_not_writable / variable_type_change_unsupported / variable_rename_unsupported
```

### 9.2 单个成功

```json
"property_result": {
  "success": true
}
```

### 9.3 批量成功

```json
"property_result": {
  "requested_count": 3,
  "applied_count": 3,
  "changed_count": 2,
  "no_op_count": 1
}
```

不返回 mode。

### 9.4 changed/no_op

对每个变量比较 before / after，但不返回 before / after。

全部 no_op：

```text
status=no_op
modified=false
validation.should_compile=false
validation.should_save=false
```

部分 changed：

```text
status=applied
modified=true
```

### 9.5 validation

成员变量声明属性变化：

```text
should_compile=true
should_save=true
```

---

## 10. Phase I：remove_blueprint_member_variable(s)

### 10.1 dry_run 强制

删除成员变量是破坏性操作，必须支持 dry_run。

dry_run passed：

```text
ok=true
status=dry_run
modified=false
data.schema=RemoveBlueprintMemberVariableDryRun.v1
dry_run.result=passed
can_execute=true
```

dry_run blocked：

```text
ok=true
status=dry_run
modified=false
dry_run.result=blocked
blocked_by=["variable_references_exist"]
conflicts[] 包含 variable_name/reference_count
```

### 10.2 引用检查

必须检查 Graph 中变量 Get/Set 节点：

```cpp
for (UEdGraph* Graph : AllBlueprintGraphs)
{
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node))
        {
            if (Get->GetVarName() == VarName) ...
        }
        if (UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node))
        {
            if (Set->GetVarName() == VarName) ...
        }
    }
}
```

需要遍历：

```text
UbergraphPages
FunctionGraphs
MacroGraphs
DelegateSignatureGraphs
Intermediate generated graphs? 第一版可不扫 transient graphs
```

如果 reference_count > 0：

```text
blocked
```

### 10.3 外部引用

外部 Blueprint 对成员变量访问第一版可保守处理：

```text
如果无法可靠检查 external referencers，不阻断，但只检查本蓝图 Graph 引用。
```

不要虚构 external dependents。后续可接 Asset Dependency / Referencer 工具增强。

### 10.4 正式删除

正式执行前重复 preflight。

UE API：

```cpp
FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, FName(*VariableName));
FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
Blueprint->MarkPackageDirty();
```

### 10.5 批量事务化

批量 remove：

```text
先检查所有变量存在且无引用。
任一 invalid/blocked → 整批失败或 dry_run blocked。
正式执行时 snapshot 变量描述和默认值。
失败时 rollback。
```

### 10.6 成功返回

单个：

```json
"variable_result": { "success": true }
```

批量：

```json
"variable_result": {
  "requested_count": 2,
  "removed_count": 2
}
```

validation：

```text
should_compile=true
should_save=true
```

---

## 11. Phase J：read_blueprint_member_defaults

### 11.1 Request

```cpp
struct FBlueprintHelperReadMemberDefaultsRequest
{
    FString AssetPath;
    TArray<FString> VariableNames;
};
```

### 11.2 显式变量名

读取默认值必须指定变量名列表。  
如果 `variable_names` 为空，第一版建议失败：

```text
invalid_request
read_blueprint_member_defaults requires explicit variable_names.
```

原因：

```text
read_blueprint_member_variables 不默认返回默认值。
read defaults 应避免无意返回全部 Class Defaults。
```

### 11.3 实现

Blueprint Class Defaults 来源：

```cpp
UClass* GeneratedClass = Blueprint->GeneratedClass;
UObject* CDO = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
```

读取属性：

```cpp
FProperty* Property = FindFProperty<FProperty>(GeneratedClass, *VariableName);
```

使用 PropertyReflectionService：

```cpp
ReadObjectPropertiesToJson(CDO, VariableNames, MaxDepth=1, Values, Conflicts);
```

### 11.4 成功返回

```json
"defaults": {
  "default_count": 2,
  "values": {
    "OpenAngle": 90.0,
    "AutoCloseDelay": 2.5
  }
}
```

不返回所有 Class Defaults。

---

## 12. Phase K：set_blueprint_member_default(s)

### 12.1 Request

```cpp
struct FBlueprintHelperSetMemberDefaultItem
{
    FString VariableName;
    TSharedPtr<FJsonValue> Value;
};

struct FBlueprintHelperSetMemberDefaultsRequest
{
    FString AssetPath;
    TArray<FBlueprintHelperSetMemberDefaultItem> Defaults;
};
```

单个内部转换成 batch size 1。

### 12.2 执行边界

该工具设置 Blueprint Class Defaults 中成员变量默认值。

不负责：

```text
创建变量声明
修改变量类型
修改运行时实例
修改 DataAsset
修改组件默认值
```

如果变量不存在：

```text
variable_not_found
```

如果类型不匹配：

```text
type_mismatch
```

### 12.3 UE API

优先用 PropertyReflectionService 写 CDO：

```cpp
UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
CDO->Modify();
SetObjectPropertyFromJson(CDO, VariableName, Value, OutResult);
Blueprint->Modify();
FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
Blueprint->MarkPackageDirty();
```

注意：

```text
成员默认值修改通常 should_compile=false / should_save=true。
```

如果某些默认值写入要求重新编译才能稳定反映，可由后续测试决定，但字段稿示例为 false/true。

### 12.4 批量事务化

批量默认值：

```text
先预校验所有变量和类型。
任一 invalid → 整批失败，不写任何默认值。
写入前 snapshot before values。
失败时 rollback。
```

### 12.5 成功返回

单个：

```json
"default_result": { "success": true }
```

批量：

```json
"default_result": {
  "requested_count": 3,
  "applied_count": 3,
  "changed_count": 2,
  "no_op_count": 1
}
```

不返回 mode，不返回 before/after。

### 12.6 validation

```text
should_compile=false
should_save=true
```

no_op：

```text
should_compile=false
should_save=false
```

---

## 13. Phase L：read_blueprint_local_variables

### 13.1 Request

```cpp
struct FBlueprintHelperReadLocalVariablesRequest
{
    FString AssetPath;
    FString FunctionName;
};
```

### 13.2 实现

Local variables 通常存在于 function graph 的 variable descriptions 中。按 UE5.3 实际源码确认来源，优先使用：

```cpp
FBlueprintEditorUtils::GetLocalVariables(Blueprint, FunctionGraph, OutLocalVars)
```

或访问：

```text
UEdGraph::LocalVariables
```

具体 API 需要根据引擎源码验证。封装：

```cpp
FBlueprintHelperLocalVariableCompat
```

避免 UE API 差异扩散。

### 13.3 成功返回

```json
"local_variables": {
  "variable_count": 1,
  "variables": [
    {
      "variable_name": "TargetAngle",
      "variable_type": {
        "category": "float",
        "container": "single"
      }
    }
  ]
}
```

不返回 default/class flags。

---

## 14. Phase M：add_blueprint_local_variable(s)

### 14.1 Request

```cpp
struct FBlueprintHelperAddLocalVariableRequest
{
    FString AssetPath;
    FString FunctionName;
    FString VariableName;
    FBlueprintHelperVariableType VariableType;

    EBlueprintHelperVariableNameCollisionPolicy NameCollisionPolicy =
        EBlueprintHelperVariableNameCollisionPolicy::FailIfExists;

    TOptional<FString> Category;
    TOptional<FString> Tooltip;
};
```

### 14.2 不支持字段

Local Variable payload 若包含：

```text
instance_editable
expose_on_spawn
replication
class_default
default_value
```

则失败：

```text
invalid_local_variable_settings
```

### 14.3 UE API

根据 UE5.3 源码确认。常见路径是：

```cpp
FBlueprintEditorUtils::AddLocalVariable(
    Blueprint,
    FunctionGraph,
    FName(*VariableName),
    PinType);
```

如果具体 API 不存在，则需要通过 FunctionGraph local variable container 修改，但必须封装在 Compat：

```cpp
FBlueprintHelperLocalVariableCompat::AddLocalVariable(Blueprint, FunctionGraph, Name, PinType)
```

### 14.4 成功返回

单个：

```json
"variable_result": { "success": true }
```

批量：

```json
"variable_result": {
  "requested_count": 2,
  "added_count": 2,
  "no_op_count": 0
}
```

validation：

```text
should_compile=true
should_save=true
```

---

## 15. Phase N：set_blueprint_local_variable_properties

### 15.1 支持属性

第一版建议支持：

```text
category
tooltip
description
```

不支持：

```text
variable_name
variable_type
instance_editable
expose_on_spawn
replication
class_default
```

### 15.2 成功返回

单个：

```json
"property_result": { "success": true }
```

批量：

```json
"property_result": {
  "requested_count": 2,
  "applied_count": 2,
  "changed_count": 1,
  "no_op_count": 1
}
```

validation：

```text
should_compile=true
should_save=true
```

---

## 16. Phase O：remove_blueprint_local_variable(s)

### 16.1 dry_run 强制

删除 Local Variable 必须 dry_run，因为函数图内可能有 Local Variable Get/Set 节点。

### 16.2 引用检查

只需检查目标函数图内引用：

```text
UK2Node_VariableGet / UK2Node_VariableSet
VarName == LocalVarName
Scope == FunctionGraph or LocalVar metadata matches
```

如果引用存在：

```text
blocked_by=["local_variable_references_exist"]
conflicts[].reference_count=N
```

### 16.3 正式删除

正式执行前重复 preflight。

UE API 通过 Compat：

```cpp
FBlueprintHelperLocalVariableCompat::RemoveLocalVariable(
    Blueprint,
    FunctionGraph,
    FName(*VariableName));
```

### 16.4 成功返回

单个：

```json
"variable_result": { "success": true }
```

批量：

```json
"variable_result": {
  "requested_count": 2,
  "removed_count": 2
}
```

validation：

```text
should_compile=true
should_save=true
```

---

## 17. Phase P：事务化 / rollback

### 17.1 批量写事务化

适用：

```text
add_blueprint_member_variables
set_blueprint_member_variables_properties
remove_blueprint_member_variables
set_blueprint_member_defaults
add_blueprint_local_variables
set_blueprint_local_variables_properties
remove_blueprint_local_variables
```

规则：

```text
1. 先完整预校验。
2. 任一 invalid → ok=false/status=failed/modified=false。
3. 不做部分应用。
4. 正式写入前 snapshot。
5. 写入中失败 → rollback。
6. rollback 成功 → modified=false。
7. rollback 失败 → modified=true/error.code=rollback_failed。
```

### 17.2 Snapshot 内容

成员变量声明 snapshot：

```text
FBPVariableDescription
Blueprint metadata
Default value snapshot
Graph reference summary
```

默认值 snapshot：

```text
CDO property before values
```

Local variable snapshot：

```text
Function graph local variable descriptions
```

### 17.3 内部 Journal

按全局写工具策略，UE 内部可记录 Journal / Review / rollback_data。  
但 Agent-facing 成功结果不返回：

```text
write_ref
transaction_id
journal_recorded
review
safety
```

---

## 18. Phase Q：validation 规则

### 18.1 成员变量声明 add/remove/properties

```text
should_compile=true
should_save=true
```

### 18.2 成员变量默认值

```text
should_compile=false
should_save=true
```

### 18.3 Local Variables add/remove/properties

```text
should_compile=true
should_save=true
```

### 18.4 no_op

```text
should_compile=false
should_save=false
```

### 18.5 read 工具

不返回 validation。

### 18.6 禁止字段

validation 不返回：

```text
compiled
saved
```

---

## 19. Phase R：Graph Write / Class Defaults 边界

### 19.1 与 Graph Write

```text
1. Graph Write 不默认创建成员变量。
2. 如果新逻辑需要新变量，Agent 应先 add_blueprint_member_variable。
3. 默认值再用 set_blueprint_member_default。
4. Graph Write 只引用已存在变量。
5. Graph Write 成功结果不回显变量声明详情。
```

### 19.2 与 Class Settings / Class Defaults

```text
1. 修改成员变量默认值：优先 set_blueprint_member_default(s)。
2. 修改非成员变量的类默认属性：使用 set_class_default_properties。
3. add_blueprint_member_variable 不设置默认值。
```

---

## 20. Phase S：Bridge Router 接入

### 20.1 新增 commands

```text
read_blueprint_member_variables
add_blueprint_member_variable
add_blueprint_member_variables
set_blueprint_member_variable_properties
set_blueprint_member_variables_properties
remove_blueprint_member_variable
remove_blueprint_member_variables

read_blueprint_member_defaults
set_blueprint_member_default
set_blueprint_member_defaults

read_blueprint_local_variables
add_blueprint_local_variable
add_blueprint_local_variables
set_blueprint_local_variable_properties
set_blueprint_local_variables_properties
remove_blueprint_local_variable
remove_blueprint_local_variables
```

### 20.2 Router 分支

```cpp
if (Request.Command == TEXT("read_blueprint_member_variables"))
{
    return MakeBridgeResponse(Request, VariableService.ReadMemberVariables(Request.Payload));
}
if (Request.Command == TEXT("add_blueprint_member_variable"))
{
    return MakeBridgeResponse(Request, VariableService.AddMemberVariable(Request.Payload));
}
...
```

所有 handler 必须走统一：

```text
MakeBridgeResponse
```

不要根据业务 status 二次改写 Bridge success/error。

---

## 21. Phase T：RequestValidator / 权限

### 21.1 read_blueprint_member_variables

```cpp
RequireString(Payload, TEXT("asset_path"));
```

### 21.2 add_blueprint_member_variable

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("variable_name"));
RequireObject(Payload, TEXT("variable_type"));
OptionalString(Payload, TEXT("name_collision"));
OptionalString(Payload, TEXT("category"));
OptionalString(Payload, TEXT("tooltip"));
OptionalBool(Payload, TEXT("instance_editable"));
OptionalBool(Payload, TEXT("expose_on_spawn"));
OptionalBool(Payload, TEXT("private"));
OptionalBool(Payload, TEXT("blueprint_read_only"));

RejectField(Payload, TEXT("default_value"));
```

### 21.3 set_blueprint_member_variable_properties

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("variable_name"));
RequireObject(Payload, TEXT("properties"));

RejectProperties(Payload.properties, ["variable_name", "variable_type", "default_value"]);
```

### 21.4 remove_blueprint_member_variable

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("variable_name"));
OptionalBool(Payload, TEXT("dry_run"));
```

### 21.5 read_blueprint_member_defaults

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireArray(Payload, TEXT("variable_names"));
```

### 21.6 set_blueprint_member_default

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("variable_name"));
RequireAny(Payload, TEXT("value"));
```

### 21.7 local variable tools

All local tools require:

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("function_name"));
```

Add/set/remove additionally require variable fields.

Reject local unsupported fields:

```text
instance_editable
expose_on_spawn
replication
class_default
default_value
```

### 21.8 权限

读工具：

```text
read_blueprint_member_variables
read_blueprint_member_defaults
read_blueprint_local_variables
```

只读，ReadOnly 下允许，不需要 write token。

写工具：

```text
add/set/remove member/default/local
```

需要写权限 / Token，并受 Safety Profile 约束。

---

## 22. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperVariableContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperMemberVariableTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperMemberDefaultTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperLocalVariableTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperVariableRollbackTests.cpp
```

### 22.1 Contract tests

```text
1. read_member_variables_contract
   - schema=ReadBlueprintMemberVariables.v1
   - returns variable_count / variables
   - does not return defaults

2. add_member_variable_single_success_contract
   - variable_result.success=true
   - no mode/counts/reused_existing
   - validation should_compile/should_save only

3. add_member_variable_single_no_op_contract
   - status=no_op
   - success=true
   - no reused_existing

4. add_member_variables_batch_contract
   - requested_count/added_count/no_op_count
   - no mode=batch

5. set_member_variable_properties_single_contract
   - property_result.success=true
   - no before/after

6. remove_member_variable_dry_run_contract
   - status=dry_run
   - passed minimal

7. remove_member_variable_dry_run_blocked_contract
   - blocked_by variable_references_exist

8. read_member_defaults_contract
   - returns default_count / values only for requested names

9. set_member_default_single_contract
   - default_result.success=true
   - validation should_compile=false should_save=true

10. read_local_variables_contract
   - target.function_name present
   - local_variables payload

11. add_local_variable_single_contract
   - success=true only

12. remove_local_variable_dry_run_blocked_contract
   - local_variable_references_exist
```

### 22.2 Member variable runtime tests

```text
1. add_float_member_variable
2. add_object_member_variable
3. add_member_variable_fail_if_exists
4. add_member_variable_reuse_if_exists_no_op
5. add_member_variable_rejects_default_value
6. add_member_variables_batch_transactional_invalid
7. set_member_variable_category
8. set_member_variable_rejects_type_change
9. remove_member_variable_without_references
10. remove_member_variable_blocked_with_get_set_nodes
```

### 22.3 Member defaults tests

```text
1. read_requested_member_defaults
2. read_defaults_requires_variable_names
3. set_float_member_default
4. set_object_member_default
5. set_defaults_batch_success
6. set_defaults_batch_invalid_no_partial_apply
7. set_same_default_no_op
```

### 22.4 Local variable tests

```text
1. read_local_variables_requires_function_name
2. add_local_float_variable
3. add_local_variable_fail_if_exists
4. add_local_variable_rejects_instance_editable
5. set_local_variable_tooltip
6. remove_local_variable_without_references
7. remove_local_variable_blocked_with_function_references
8. local_tools_do_not_affect_member_variables
```

### 22.5 Rollback tests

```text
1. add_member_variables_write_failure_rolls_back
2. set_member_properties_failure_rolls_back
3. set_member_defaults_failure_rolls_back
4. remove_member_variables_failure_rolls_back
5. add_local_variables_failure_rolls_back
6. rollback_failed_sets_modified_true
```

---

## 23. 推荐提交顺序

### Commit 1：DTO / Enum / VariableType

```text
Add Blueprint Variable DTOs
Add variable scope/read/default enums
Add public variable_type serializer
Add FEdGraphPinType conversion service
```

验收：

```text
variable_type 不暴露 FEdGraphPinType 全量字段。
```

### Commit 2：Resolve Blueprint / Function Compat

```text
Add BlueprintVariableService skeleton
Resolve UBlueprint
Resolve function graph
Add LocalVariableCompat wrapper
```

验收：

```text
local variable tools 必须 function_name。
```

### Commit 3：read member variables

```text
Implement read_blueprint_member_variables
Serialize declarations only
Suppress defaults
```

验收：

```text
不返回 default values。
```

### Commit 4：add member variable(s)

```text
Implement add_blueprint_member_variable(s)
Support fail_if_exists/reuse_if_exists
Reject default_value
Make batch transactional
```

验收：

```text
单个成功只 success=true。
批量成功只 counts，无 mode。
```

### Commit 5：set member variable properties

```text
Implement set member variable properties
Reject variable_name / variable_type / default_value changes
Support batch transactional updates
```

验收：

```text
不支持 rename/type change。
```

### Commit 6：remove member variable(s)

```text
Implement dry_run reference scan
Implement remove member variable(s)
Block when graph references exist
Add rollback snapshots
```

验收：

```text
引用存在时 dry_run blocked。
```

### Commit 7：member defaults read/write

```text
Implement read_blueprint_member_defaults
Implement set_blueprint_member_default(s)
Use CDO property reflection
Batch transactional
```

验收：

```text
默认值读写与变量声明工具分离。
```

### Commit 8：read/add/set local variables

```text
Implement read_blueprint_local_variables
Implement add/set local variable tools
Reject member-only fields
```

验收：

```text
Local Variable 不支持 instance_editable/expose_on_spawn/class default。
```

### Commit 9：remove local variable(s)

```text
Implement local variable dry_run reference scan
Implement remove local variable(s)
Block function graph references
```

验收：

```text
函数图引用存在时 blocked。
```

### Commit 10：Bridge / Validator / Auth

```text
Register all variable commands
Add validators
Classify read/write permissions
Use ToolResultBuilder/MakeBridgeResponse
```

验收：

```text
读工具 ReadOnly 可用。
写工具需要 token。
```

### Commit 11：Internal Journal / rollback integration

```text
Record internal Journal / Review for variable writes
Store rollback snapshots
Suppress transaction_id in Agent-facing success
```

验收：

```text
成功不返回 write_ref/transaction_id/review/safety。
```

### Commit 12：Contract regression

```text
Add contract tests for single success shape
Add batch count/no-mode tests
Add validation shape tests
Add no forbidden fields tests
```

验收：

```text
字段稿验收项全部通过。
```

---

## 24. 第一版不做的内容

```text
1. 不支持 rename_blueprint_member_variable。
2. 不支持 change_blueprint_member_variable_type。
3. 不支持 rename_blueprint_local_variable。
4. 不支持 change_blueprint_local_variable_type。
5. 不自动替换所有 Graph 引用。
6. 不自动迁移默认值。
7. 不自动修复变量节点。
8. add_blueprint_member_variable 不设置默认值。
9. Local Variable 不支持 instance_editable / expose_on_spawn / replication / class default。
10. 不支持 auto_rename。
11. 不支持 replace_existing。
12. 不返回 variable_guid。
13. 不返回 before / after。
14. 不返回 write_ref / transaction_id / review / safety。
```

---

## 25. 最小验收标准

单成员变量添加成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_member_variable",
  "trace_id": "trace_20260503_6001",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "data": {
    "schema": "AddBlueprintMemberVariable.v1",
    "variable_result": {
      "success": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

单成员变量 no_op：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_member_variable",
  "trace_id": "trace_20260503_6002",
  "status": "no_op",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "data": {
    "schema": "AddBlueprintMemberVariable.v1",
    "variable_result": {
      "success": true
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": false
  }
}
```

批量成员变量添加成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_member_variables",
  "trace_id": "trace_20260503_6003",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "data": {
    "schema": "AddBlueprintMemberVariable.v1",
    "variable_result": {
      "requested_count": 3,
      "added_count": 3,
      "no_op_count": 0
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

成员默认值设置成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_blueprint_member_default",
  "trace_id": "trace_20260503_6010",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "default_scope": "member_default"
  },
  "data": {
    "schema": "SetBlueprintMemberDefault.v1",
    "default_result": {
      "success": true
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

Local Variable 添加成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_local_variable",
  "trace_id": "trace_20260503_6020",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "function_name": "TogglePhysicsDoor",
    "variable_scope": "local"
  },
  "data": {
    "schema": "AddBlueprintLocalVariable.v1",
    "variable_result": {
      "success": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

删除变量 dry_run blocked：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_member_variable",
  "trace_id": "trace_20260503_6030",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "variable_scope": "member"
  },
  "data": {
    "schema": "RemoveBlueprintMemberVariableDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": ["variable_references_exist"],
      "conflicts": [
        {
          "code": "variable_references_exist",
          "variable_name": "OpenAngle",
          "reference_count": 4,
          "message": "The variable is referenced by Blueprint graph nodes."
        }
      ],
      "errors": []
    }
  }
}
```

必须不出现：

```text
mode in single operation
requested_count in single operation
added_count in single operation
removed_count in single operation
changed_count in single operation
no_op_count in single operation
mode=batch in batch operation
reused_existing in no_op
variable_guid
before
after
all_variables
write_ref
transaction_id
journal_recorded
review
safety
validation.compiled
validation.saved
```

---

## 26. 实现风险

### 26.1 add 变量时误设置默认值

风险：

```text
为了方便把 default_value 传入 AddMemberVariable。
```

处理：

```text
add_blueprint_member_variable Validator 拒绝 default_value。
默认值必须走 set_blueprint_member_default。
```

### 26.2 单变量成功返回计数

风险：

```text
复用批量 result，导致单变量返回 requested_count/added_count。
```

处理：

```text
单变量专用 DTO。
Contract test 锁定 success=true only。
```

### 26.3 Local Variable API 差异

风险：

```text
UE5.3 Local Variables 的公开 API 与预期不同。
```

处理：

```text
封装 LocalVariableCompat。
以引擎源码编译校验。
```

### 26.4 删除变量漏扫引用

风险：

```text
变量仍被 Graph 节点引用，删除后蓝图编译失败。
```

处理：

```text
remove dry_run 必须扫描本蓝图所有相关 Graph。
引用存在 blocked。
外部引用扫描能力不足时不虚构 external blocker。
```

### 26.5 修改成员变量类型误入 properties 工具

风险：

```text
set_blueprint_member_variable_properties 接收到 variable_type 并执行。
```

处理：

```text
Validator 和 Service 双重拒绝。
错误码 variable_type_change_unsupported。
```

### 26.6 validation 字段混用

风险：

```text
复用旧 validation 输出 compiled/saved。
```

处理：

```text
只用 FBlueprintHelperValidationHint。
Contract test 禁止 compiled/saved。
