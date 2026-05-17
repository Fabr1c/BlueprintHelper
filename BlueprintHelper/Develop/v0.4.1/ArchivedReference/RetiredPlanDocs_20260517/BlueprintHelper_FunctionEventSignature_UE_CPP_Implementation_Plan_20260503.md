# BlueprintHelper Blueprint Function / Event Signature Management UE 侧 C++ 可执行实现计划

> 2026-05-14 状态转移：本文中的未达期待、待验证项和阻塞项已迁移到 [BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md](BlueprintHelper_UnmetExpectations_Consolidated_20260514_CN.md)。本文保留为历史上下文；开放项跟踪迁移完成，后续当前状态以总账为准。

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_FunctionEventSignature_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、函数体 / 事件体 Graph Write、接口资产函数签名编辑、签名迁移、调用点自动迁移

---

## 0. 实现目标

实现 Blueprint Function / Custom Event Signature Management 第一版工具簇：

```text
read_blueprint_functions
read_blueprint_events

add_blueprint_function
add_blueprint_custom_event

set_blueprint_function_properties
set_blueprint_event_properties

remove_blueprint_function
remove_blueprint_custom_event
```

本簇只处理：

```text
Function / Custom Event 的声明与签名层：
- 函数名 / 事件名
- 输入参数列表
- 输出参数列表
- Category / Tooltip / Access
- pure / callable / const / deprecated 等非图体属性
```

本簇不处理：

```text
函数体逻辑
事件体逻辑
Graph 节点实现
接口函数体实现
EventGraph 接入
调用点重连
自动迁移外部调用节点
Blueprint Interface Asset 的函数签名编辑
```

字段契约核心点：

```text
1. Function / Event Signature Management 只处理声明和签名层，不处理函数体 / 事件体逻辑。
2. 函数体 / 事件体逻辑仍由 Graph Write / Replace function_body / event_body 处理。
3. add_blueprint_function 只创建函数声明和入口，不写函数体。
4. add_blueprint_custom_event 只创建 Custom Event 声明和入口，不接入执行流。
5. 单个 add / set / remove 成功只返回 success=true。
6. 单个 no_op 通过 status=no_op 表达，不返回 reused_existing。
7. name_collision 只支持 fail_if_exists / reuse_if_exists。
8. 不支持 auto_rename / replace_existing。
9. 第一版不支持 rename function / event。
10. 第一版不支持 change existing function/event signature。
11. 第一版不支持 add/remove/reorder existing parameters。
12. set_blueprint_function_properties 只改非签名属性。
13. set_blueprint_event_properties 只改非签名属性。
14. remove_blueprint_function 必须 dry_run。
15. remove_blueprint_custom_event 必须 dry_run。
16. remove function 有 external_dependents 时 blocked。
17. remove custom event 只允许 custom_event，不允许 engine_event / interface_event。
18. read_blueprint_functions 不返回函数体 graph。
19. read_blueprint_events 不返回事件体 graph。
20. 写工具 validation 只返回 should_compile / should_save。
21. 写工具成功不返回 write_ref / transaction_id / review / safety。
22. data.schema 使用短命名。
23. 本簇所有成功写操作只返回 success=true。
24. 成功不返回计数、guid、entry_node_ref、graph_id、reused_existing。
25. no_op 通过 status=no_op 表达，data 中仍只返回 success=true。
26. 失败 / dry_run blocked 返回详细 error 或 conflicts。
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
AssetRegistry
```

建议确认下列头文件：

```cpp
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/UnrealType.h"
```

本簇涉及的 UE API 在不同 UE5 小版本中存在差异，尤其是：

```text
函数图创建
Custom Event 创建
参数 pin 增删
函数 flags / metadata 修改
Custom Event metadata 修改
```

因此必须封装兼容层，避免业务服务直接散落调用底层 API。

---

## 2. Phase A：新增类型文件

### 2.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperSignatureTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperSignatureTypes.cpp
```

若变量工具簇已经实现 `FBlueprintHelperVariableType`，本簇应复用该类型与转换服务，不重复定义一套 variable type。

### 2.2 新增枚举

```cpp
enum class EBlueprintHelperSignatureScope : uint8
{
    Function,
    CustomEvent
};

enum class EBlueprintHelperSignatureReadScope : uint8
{
    FunctionSignatures,
    EventSignatures
};

enum class EBlueprintHelperEventKind : uint8
{
    CustomEvent,
    EngineEvent,
    InterfaceEvent,
    OverrideEvent,
    Unknown
};

enum class EBlueprintHelperSignatureParamDirection : uint8
{
    Input,
    Output
};

enum class EBlueprintHelperSignatureNameCollisionPolicy : uint8
{
    FailIfExists,
    ReuseIfExists
};

enum class EBlueprintHelperFunctionAccess : uint8
{
    Public,
    Protected,
    Private
};

enum class EBlueprintHelperSignatureStage : uint8
{
    ParseInput,
    ResolveAsset,
    ResolveBlueprint,
    ResolveFunction,
    ResolveEvent,
    ResolveGraph,
    ResolveSignatureType,
    NameCollisionCheck,
    ValidateSignature,
    ValidateProperties,
    ValidateDependents,
    ValidateEventKind,
    DryRun,
    SnapshotBefore,
    AddFunction,
    AddCustomEvent,
    SetProperties,
    RemoveFunction,
    RemoveCustomEvent,
    MarkModified,
    Rollback
};

enum class EBlueprintHelperSignatureErrorCode : uint8
{
    InvalidRequest,
    AssetNotFound,
    TargetNotBlueprint,
    FunctionNotFound,
    EventNotFound,
    FunctionAlreadyExists,
    EventAlreadyExists,
    UnsupportedNameCollisionPolicy,
    UnsupportedSignatureType,
    UnsupportedSignatureContainer,
    UnsupportedSignatureMutation,
    FunctionRenameUnsupported,
    EventRenameUnsupported,
    FunctionSignatureChangeUnsupported,
    EventSignatureChangeUnsupported,
    InvalidFunctionSettings,
    InvalidEventSettings,
    InvalidSignatureSettings,
    UnsupportedProperty,
    ExternalDependentsExist,
    InternalCallersExist,
    InterfaceFunctionRemovalUnsupported,
    InheritedFunctionRemovalUnsupported,
    EventKindNotRemovable,
    RemoveFunctionDryRunRequired,
    RemoveCustomEventDryRunRequired,
    FunctionAddFailed,
    CustomEventAddFailed,
    FunctionPropertySetFailed,
    EventPropertySetFailed,
    FunctionRemoveFailed,
    CustomEventRemoveFailed,
    RollbackFailed,
    InternalError
};
```

### 2.3 字符串序列化

稳定输出：

```text
function
custom_event
function_signatures
event_signatures
input
output
public
protected
private
fail_if_exists
reuse_if_exists
custom_event
engine_event
interface_event
override_event
unsupported_signature_mutation
external_dependents_exist
event_kind_not_removable
```

不要输出 C++ enum 原名。

---

## 3. Phase B：Agent-facing DTO

### 3.1 Signature parameter

```cpp
struct FBlueprintHelperSignatureParam
{
    FString Name;
    FString Direction; // input | output
    FBlueprintHelperVariableType Type;

    TSharedPtr<FJsonValue> DefaultValue; // optional
    bool bPassByReference = false;

    TSharedRef<FJsonObject> ToJson() const;
};
```

Agent-facing 输出中，字段稿示例的 `inputs[] / outputs[]` 可以不输出 `direction`，因为 array 已表达方向。内部 DTO 可保留 direction，序列化到参数对象时建议：

```text
read_blueprint_functions:
  inputs[] / outputs[] 下默认不输出 direction，除非后续 schema 明确要求。

add request:
  内部解析可接受 inputs / outputs 分组，不要求 direction。
```

### 3.2 Function signature summary

```cpp
struct FBlueprintHelperFunctionSignatureSummary
{
    FString FunctionName;

    TOptional<FString> Category;
    TOptional<FString> Access; // public | protected | private
    TOptional<bool> bPure;
    TOptional<bool> bCallable;

    TArray<FBlueprintHelperSignatureParam> Inputs;
    TArray<FBlueprintHelperSignatureParam> Outputs;

    TSharedRef<FJsonObject> ToJson() const;
};
```

禁止默认返回：

```text
function_guid
graph_id
entry_node_ref
function body graph
node list
external_dependents
```

### 3.3 Event signature summary

```cpp
struct FBlueprintHelperEventSignatureSummary
{
    FString EventName;
    FString EventKind; // custom_event | engine_event | interface_event | override_event
    TArray<FBlueprintHelperSignatureParam> Inputs;

    TSharedRef<FJsonObject> ToJson() const;
};
```

禁止默认返回：

```text
event_guid
graph_id
entry_node_ref
event body graph
node list
```

### 3.4 read_blueprint_functions DTO

```cpp
struct FBlueprintHelperReadFunctionsData
{
    FString Schema = TEXT("ReadBlueprintFunctions.v1");
    FBlueprintHelperFunctionSignatureList Functions;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperFunctionSignatureList
{
    int32 FunctionCount = 0;
    TArray<FBlueprintHelperFunctionSignatureSummary> Items;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.5 read_blueprint_events DTO

```cpp
struct FBlueprintHelperReadEventsData
{
    FString Schema = TEXT("ReadBlueprintEvents.v1");
    FBlueprintHelperEventSignatureList Events;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperEventSignatureList
{
    int32 EventCount = 0;
    TArray<FBlueprintHelperEventSignatureSummary> Items;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.6 单成功 DTO

本簇所有成功写操作只返回 `success=true`。为避免不同工具重复定义，提供通用结构：

```cpp
struct FBlueprintHelperSingleSuccessResult
{
    bool bSuccess = true;

    TSharedRef<FJsonObject> ToJson() const;
};
```

具体 data：

```cpp
struct FBlueprintHelperFunctionResultData
{
    FString Schema; // AddBlueprintFunction.v1 / RemoveBlueprintFunction.v1
    FBlueprintHelperSingleSuccessResult FunctionResult;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperEventResultData
{
    FString Schema; // AddBlueprintCustomEvent.v1 / RemoveBlueprintCustomEvent.v1
    FBlueprintHelperSingleSuccessResult EventResult;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperSignaturePropertyResultData
{
    FString Schema; // SetBlueprintFunctionProperties.v1 / SetBlueprintEventProperties.v1
    FBlueprintHelperSingleSuccessResult PropertyResult;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.7 dry_run DTO

```cpp
struct FBlueprintHelperSignatureDryRunData
{
    FString Schema; // RemoveBlueprintFunctionDryRun.v1 / RemoveBlueprintCustomEventDryRun.v1
    FBlueprintHelperDryRunPayload DryRun;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.8 明确禁止字段

写工具成功 DTO 不包含：

```text
requested_count
added_count
removed_count
changed_count
no_op_count
reused_existing
function_guid
event_guid
entry_node_ref
graph_id
write_ref
transaction_id
journal_recorded
review
safety
safety_profile
before
after
signature_diff
```

---

## 4. Phase C：Signature 类型转换服务

### 4.1 复用 VariableTypeService

本簇的参数类型表达必须复用变量工具簇的 `FBlueprintHelperVariableTypeService`：

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
};
```

不要为 signature 单独暴露 UE `FEdGraphPinType` 全字段。

### 4.2 第一版支持类型

```text
bool
int
float
name
string
text
vector
rotator
transform
object
class
struct
enum
array
```

`set / map` 可：

```text
只读返回，写入返回 unsupported_signature_container
```

### 4.3 参数默认值

第一版可支持简单输入参数默认值：

```text
primitive
string/name/text
enum
soft object path
```

不支持复杂默认值时：

```text
unsupported_signature_type 或 invalid_signature_settings
```

创建函数 / Custom Event 时可接受 `default_value`，但不支持修改已有参数默认值，因为第一版不支持 signature mutation。

---

## 5. Phase D：SignatureCompat 兼容层

### 5.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperSignatureCompat.h
Source/BlueprintHelper/Private/Services/BlueprintHelperSignatureCompat.cpp
```

### 5.2 职责

集中封装 UE5.3+ 中函数 / Custom Event 签名相关 API：

```text
创建函数图
创建函数 entry/result 节点
设置函数输入 / 输出 pins
创建 Custom Event 节点
设置 Custom Event 参数 pins
读取函数签名
读取 Custom Event 签名
设置函数 flags / metadata
删除函数图
删除 Custom Event 节点 / 图
```

### 5.3 接口建议

```cpp
class FBlueprintHelperSignatureCompat
{
public:
    bool ReadFunctions(
        UBlueprint* Blueprint,
        TArray<FBlueprintHelperFunctionSignatureSummary>& OutFunctions,
        FBlueprintHelperToolError& OutError) const;

    bool ReadEvents(
        UBlueprint* Blueprint,
        TArray<FBlueprintHelperEventSignatureSummary>& OutEvents,
        FBlueprintHelperToolError& OutError) const;

    bool AddFunction(
        UBlueprint* Blueprint,
        const FBlueprintHelperAddFunctionRequest& Request,
        FBlueprintHelperToolError& OutError) const;

    bool AddCustomEvent(
        UBlueprint* Blueprint,
        const FBlueprintHelperAddCustomEventRequest& Request,
        FBlueprintHelperToolError& OutError) const;

    bool SetFunctionProperties(
        UBlueprint* Blueprint,
        const FString& FunctionName,
        const FBlueprintHelperFunctionPropertyPatch& Properties,
        bool& bOutChanged,
        FBlueprintHelperToolError& OutError) const;

    bool SetEventProperties(
        UBlueprint* Blueprint,
        const FString& EventName,
        const FBlueprintHelperEventPropertyPatch& Properties,
        bool& bOutChanged,
        FBlueprintHelperToolError& OutError) const;

    bool RemoveFunction(
        UBlueprint* Blueprint,
        const FString& FunctionName,
        FBlueprintHelperToolError& OutError) const;

    bool RemoveCustomEvent(
        UBlueprint* Blueprint,
        const FString& EventName,
        FBlueprintHelperToolError& OutError) const;
};
```

### 5.4 编译校验要求

该兼容层必须基于实际 UE5.3+ 引擎源码编译校验。不要在业务计划中假设不存在的 `FBlueprintEditorUtils` 函数一定可用。

---

## 6. Phase E：SignatureService

### 6.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperSignatureService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperSignatureService.cpp
```

### 6.2 服务接口

```cpp
class FBlueprintHelperSignatureService
{
public:
    FBlueprintHelperToolResultBase ReadBlueprintFunctions(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase ReadBlueprintEvents(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase AddBlueprintFunction(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase AddBlueprintCustomEvent(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase SetBlueprintFunctionProperties(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase SetBlueprintEventProperties(const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase RemoveBlueprintFunction(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperToolResultBase RemoveBlueprintCustomEvent(const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool ResolveBlueprint(
        const FString& AssetPath,
        UBlueprint*& OutBlueprint,
        FBlueprintHelperToolError& OutError) const;

    bool FunctionExists(UBlueprint* Blueprint, const FString& FunctionName) const;
    bool CustomEventExists(UBlueprint* Blueprint, const FString& EventName) const;
};
```

---

## 7. Phase F：请求结构

### 7.1 read_blueprint_functions

```cpp
struct FBlueprintHelperReadFunctionsRequest
{
    FString AssetPath;
};
```

### 7.2 read_blueprint_events

```cpp
struct FBlueprintHelperReadEventsRequest
{
    FString AssetPath;
};
```

### 7.3 add_blueprint_function

```cpp
struct FBlueprintHelperAddFunctionRequest
{
    FString AssetPath;
    FString FunctionName;

    EBlueprintHelperSignatureNameCollisionPolicy NameCollisionPolicy =
        EBlueprintHelperSignatureNameCollisionPolicy::FailIfExists;

    TArray<FBlueprintHelperSignatureParam> Inputs;
    TArray<FBlueprintHelperSignatureParam> Outputs;

    TOptional<FString> Category;
    TOptional<FString> Tooltip;
    TOptional<FString> Access; // public | protected | private
    TOptional<bool> bPure;
    TOptional<bool> bCallable;
    TOptional<bool> bConst;
    TOptional<bool> bDeprecated;
    TOptional<FString> DeprecationMessage;
};
```

### 7.4 add_blueprint_custom_event

```cpp
struct FBlueprintHelperAddCustomEventRequest
{
    FString AssetPath;
    FString EventName;

    EBlueprintHelperSignatureNameCollisionPolicy NameCollisionPolicy =
        EBlueprintHelperSignatureNameCollisionPolicy::FailIfExists;

    TArray<FBlueprintHelperSignatureParam> Inputs;

    TOptional<FString> Category;
    TOptional<FString> Tooltip;
    TOptional<bool> bDeprecated;
    TOptional<FString> DeprecationMessage;
};
```

### 7.5 set properties requests

```cpp
struct FBlueprintHelperFunctionPropertyPatch
{
    TOptional<FString> Category;
    TOptional<FString> Tooltip;
    TOptional<FString> Access;
    TOptional<bool> bPure;
    TOptional<bool> bCallable;
    TOptional<bool> bConst;
    TOptional<bool> bDeprecated;
    TOptional<FString> DeprecationMessage;
};

struct FBlueprintHelperEventPropertyPatch
{
    TOptional<FString> Category;
    TOptional<FString> Tooltip;
    TOptional<bool> bDeprecated;
    TOptional<FString> DeprecationMessage;
};
```

### 7.6 remove requests

```cpp
struct FBlueprintHelperRemoveFunctionRequest
{
    FString AssetPath;
    FString FunctionName;
    bool bDryRun = false;
};

struct FBlueprintHelperRemoveCustomEventRequest
{
    FString AssetPath;
    FString EventName;
    bool bDryRun = false;
};
```

---

## 8. Phase G：read_blueprint_functions

### 8.1 执行流程

```text
1. ParseRequest。
2. ResolveBlueprint。
3. 读取 Blueprint 自有函数图签名。
4. 序列化 function_count / items[]。
5. 不读取函数体图节点。
```

### 8.2 读取来源

优先遍历：

```cpp
Blueprint->FunctionGraphs
```

对每个函数图读取：

```text
函数名
Category
Access
Pure / Callable
FunctionEntry 输入 pins
FunctionResult 输出 pins
```

可使用：

```cpp
UK2Node_FunctionEntry
UK2Node_FunctionResult
```

### 8.3 过滤规则

第一版建议：

```text
1. 读取普通 Blueprint 函数。
2. 可读取 inherited/override 摘要，但标记不做 remove。
3. 不返回 interface function body 详情。
4. 不返回 Macro / EventGraph。
```

如果内部无法安全区分某类函数，先只返回 `FunctionGraphs` 中的可识别函数。

### 8.4 成功返回

```text
ok=true
status=completed
modified=false
data.schema=ReadBlueprintFunctions.v1
target.read_scope=function_signatures
```

不返回 validation。

---

## 9. Phase H：read_blueprint_events

### 9.1 执行流程

```text
1. ParseRequest。
2. ResolveBlueprint。
3. 遍历 EventGraph / Ubergraph 中 Event 节点。
4. 区分 custom_event / engine_event / interface_event / override_event。
5. 输出 event_count / items[]。
6. 不读取事件体图节点。
```

### 9.2 读取来源

遍历：

```cpp
Blueprint->UbergraphPages
```

节点类型：

```text
UK2Node_CustomEvent -> custom_event
UK2Node_Event -> engine_event / override_event / interface_event depending metadata/function source
```

### 9.3 EventKind 判定

建议封装：

```cpp
EBlueprintHelperEventKind ClassifyEventNode(const UEdGraphNode* Node);
```

判定策略：

```text
UK2Node_CustomEvent:
  custom_event

UK2Node_Event:
  如果 EventSignatureName 对应 Blueprint interface function → interface_event
  如果是引擎原生事件 / Actor lifecycle / Component event → engine_event
  如果重写父类 BlueprintImplementableEvent / override → override_event
  否则 unknown
```

### 9.4 成功返回

```text
ok=true
status=completed
modified=false
data.schema=ReadBlueprintEvents.v1
target.read_scope=event_signatures
```

不返回 validation。

---

## 10. Phase I：add_blueprint_function

### 10.1 边界

只创建：

```text
函数声明
函数图
Function Entry
Function Result（如有 outputs）
输入 / 输出 pins
非签名属性 metadata
```

不创建：

```text
函数体实现节点
Local Variables
调用节点
EventGraph 接入
BlueprintHelper-owned block
```

### 10.2 Name collision

```text
fail_if_exists + exists → failed / function_already_exists
reuse_if_exists + exists → no_op / success=true
```

不支持：

```text
auto_rename
replace_existing
```

### 10.3 Signature 创建

添加函数时可以带初始 inputs / outputs。  
这不违反“第一版不支持 change existing signature”，因为这是创建初始签名。

### 10.4 UE 实现路径

推荐封装到 `SignatureCompat.AddFunction`，内部可能使用：

```cpp
UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
    Blueprint,
    FName(*FunctionName),
    UEdGraph::StaticClass(),
    UEdGraphSchema_K2::StaticClass());

FBlueprintEditorUtils::AddFunctionGraph(Blueprint, NewGraph, /*bIsUserCreated=*/true, nullptr);
```

然后配置 Entry / Result pins：

```text
1. 找到 UK2Node_FunctionEntry。
2. 按 inputs 创建 user defined pins。
3. 如有 outputs，创建 / 找到 UK2Node_FunctionResult。
4. 按 outputs 创建 output pins。
```

实际 pin 创建 API 需按 UE5.3 源码确认，例如：

```text
FBlueprintEditorUtils::AddFunctionGraph
FBlueprintEditorUtils::AddFunctionInput
FBlueprintEditorUtils::AddFunctionOutput
```

如果没有直接 API，必须通过 Schema / UK2Node editable pins 实现，并刷新节点。

### 10.5 属性设置

创建后设置：

```text
category
tooltip
access
pure
callable
const
deprecated
deprecation_message
```

这些可通过：

```text
Blueprint metadata
FunctionEntry metadata
Function flags
UFunction metadata after compile? 
```

第一版以编辑器可持久化的 Blueprint graph metadata 为准，避免直接改 transient UFunction。

### 10.6 成功返回

```json
"function_result": {
  "success": true
}
```

validation：

```text
should_compile=true
should_save=true
```

no_op：

```text
status=no_op
modified=false
validation false/false
success=true
```

不返回 `entry_node_ref / graph_id / function_guid / reused_existing`。

---

## 11. Phase J：add_blueprint_custom_event

### 11.1 边界

只创建：

```text
Custom Event 声明
Custom Event 入口节点
输入 pins
metadata/category/tooltip
```

不做：

```text
事件体逻辑
执行流接入
BeginPlay/Tick/InputAction 等引擎事件创建
重复创建全局事件
Merge 到已有执行流
```

### 11.2 Name collision

```text
fail_if_exists + exists → failed / event_already_exists
reuse_if_exists + exists → no_op / success=true
```

不支持：

```text
auto_rename
replace_existing
```

### 11.3 UE 实现路径

Custom Event 创建建议：

```cpp
UEdGraph* TargetGraph = EnsureEventGraph(Blueprint);

UK2Node_CustomEvent* CustomEventNode =
    NewObject<UK2Node_CustomEvent>(TargetGraph);

CustomEventNode->CustomFunctionName = FName(*EventName);
CustomEventNode->NodePosX = ...
CustomEventNode->NodePosY = ...
CustomEventNode->AllocateDefaultPins();

TargetGraph->AddNode(CustomEventNode, true, false);
```

然后按 inputs 添加 user defined pins。  
实际 API 可能通过：

```text
CustomEventNode->CreateUserDefinedPin(...)
FEdGraphSchemaAction_K2NewNode
FBlueprintEditorUtils::AddNode
```

需以 UE5.3 源码编译校验。

### 11.4 EventGraph 选择

第一版策略：

```text
1. 如果 Blueprint 有 UbergraphPages[0]，使用第一个 EventGraph。
2. 如果没有，创建标准 EventGraph。
3. 不接入已有执行流。
4. 新 Custom Event 节点可以放在空白位置，但不返回坐标。
```

### 11.5 成功返回

```json
"event_result": {
  "success": true
}
```

validation：

```text
should_compile=true
should_save=true
```

---

## 12. Phase K：set_blueprint_function_properties

### 12.1 支持属性

第一版支持：

```text
category
tooltip
access
pure
callable
const
deprecated
deprecation_message
```

### 12.2 不支持字段

明确拒绝：

```text
function_name
inputs
outputs
parameters
return_values
signature
parameter_type
parameter_order
```

失败：

```text
error.code=unsupported_signature_mutation
stage=validate_properties
conflicts[].code=unsupported_property
```

### 12.3 执行流程

```text
1. ResolveBlueprint。
2. ResolveFunction。
3. ValidateProperties。
4. Snapshot before。
5. Apply metadata / flags。
6. MarkBlueprintAsStructurallyModified 或 MarkBlueprintAsModified。
7. 返回 success=true。
```

### 12.4 changed/no_op

本簇单个 set 成功只返回 success=true，不返回 changed_count。  
如果所有属性与现状一致：

```text
status=no_op
modified=false
validation false/false
success=true
```

如果有变化：

```text
status=applied
modified=true
validation true/true
success=true
```

### 12.5 风险字段

`pure / callable / const` 可能改变节点可调用性或执行 pin 行为。  
第一版仍允许设置，但必须：

```text
1. 校验不涉及参数变更。
2. Mark structurally modified。
3. validation.should_compile=true。
```

---

## 13. Phase L：set_blueprint_event_properties

### 13.1 支持属性

第一版支持：

```text
category
tooltip
deprecated
deprecation_message
```

### 13.2 不支持字段

明确拒绝：

```text
event_name
inputs
parameters
parameter_type
engine_event mutation
interface_event mutation
```

### 13.3 只允许 Custom Event?

字段稿说修改 Custom Event 的非签名属性。  
第一版建议：

```text
1. 只允许 target event_kind=custom_event。
2. 对 engine_event / interface_event / override_event 返回 event_kind_not_mutable 或 event_kind_not_removable。
```

若希望允许 engine_event tooltip/category，容易混淆引擎事件身份，第一版不做。

### 13.4 成功 / no_op

与 function properties 一致：

```text
applied/no_op
success=true
validation true/true 或 false/false
```

---

## 14. Phase M：remove_blueprint_function

### 14.1 dry_run 强制

删除函数必须 dry_run。

dry_run passed：

```text
ok=true
status=dry_run
modified=false
data.schema=RemoveBlueprintFunctionDryRun.v1
dry_run.result=passed
can_execute=true
```

dry_run blocked：

```text
ok=true
status=dry_run
modified=false
dry_run.result=blocked
blocked_by=[...]
conflicts[] 详细定位
```

### 14.2 可删除范围

第一版只允许删除：

```text
Blueprint 自己声明的普通函数
非 inherited
非 interface implementation
非 engine override
```

不允许删除：

```text
inherited function
interface implementation function
event graph / macro graph
construction script
```

### 14.3 引用 / dependents 检查

必须检查：

```text
1. 本蓝图内部调用点。
2. external_dependents。
3. interface implementation 关系。
4. inherited / override 关系。
```

第一版 external dependents 检查可分层：

```text
A. 如果已有 Asset Referencer / dependency index：查询外部 Blueprint 调用。
B. 如果没有可靠外部分析能力：Conservative 下 blocked=external_dependents_check_unavailable。
```

字段稿明确“有 external_dependents 时 blocked”。  
不要在无法检测时声称 external_dependent_count=0。  
建议实现内部状态：

```text
external_dependents_check_status = available | unavailable
```

当 unavailable 且 Safety Profile Conservative：

```text
dry_run blocked
blocked_by=["external_dependents_check_unavailable"]
```

若当前项目只要求内部调用检查，则可先仅检查内部并在文档/diagnostics 中标记外部检查后置；但实现计划建议保守。

### 14.4 内部调用扫描

扫描所有蓝图图表：

```cpp
for (UEdGraph* Graph : AllGraphs)
{
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
        {
            if (Call->FunctionReference.GetMemberName() == FunctionName)
            {
                InternalCallerCount++;
            }
        }
    }
}
```

如果 internal callers exist：

```text
blocked_by=["internal_callers_exist"]
```

### 14.5 正式删除

正式执行前重复 preflight。  
删除：

```cpp
FBlueprintEditorUtils::RemoveGraph(Blueprint, FunctionGraph, EGraphRemoveFlags::Recompile);
```

或使用 UE5.3 对应 function graph 删除 API。

必须：

```text
Blueprint->Modify()
MarkBlueprintAsStructurallyModified
MarkPackageDirty
```

### 14.6 成功返回

```json
"function_result": {
  "success": true
}
```

validation：

```text
should_compile=true
should_save=true
```

---

## 15. Phase N：remove_blueprint_custom_event

### 15.1 dry_run 强制

删除 Custom Event 必须 dry_run。

### 15.2 只允许 custom_event

dry_run 必须先判定 event_kind：

```text
custom_event → 可继续检查
engine_event → blocked event_kind_not_removable
interface_event → blocked event_kind_not_removable
override_event → blocked event_kind_not_removable
unknown → blocked event_kind_not_removable
```

### 15.3 引用检查

检查：

```text
1. 同蓝图内 Call Custom Event / function call 引用。
2. 事件绑定 / delegate binding 引用。
3. external_dependents，如可用。
```

若存在：

```text
blocked_by=["event_references_exist"] 或 external_dependents_exist
```

字段稿示例强调 `event_kind_not_removable`，但实现应同时覆盖引用阻断。

### 15.4 正式删除

正式执行前重复 preflight。  
删除节点：

```cpp
TargetGraph->Modify();
CustomEventNode->Modify();
TargetGraph->RemoveNode(CustomEventNode);
FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
Blueprint->MarkPackageDirty();
```

如果 Custom Event 关联 generated function entry 或 graph state，必须调用对应 UE editor utils 刷新 blueprint。

### 15.5 成功返回

```json
"event_result": {
  "success": true
}
```

validation：

```text
should_compile=true
should_save=true
```

---

## 16. Phase O：事务化 / rollback

### 16.1 写操作范围

需要 snapshot / rollback 的操作：

```text
add_blueprint_function
add_blueprint_custom_event
set_blueprint_function_properties
set_blueprint_event_properties
remove_blueprint_function
remove_blueprint_custom_event
```

### 16.2 Snapshot 内容

函数：

```text
Function graph package snapshot
FunctionEntry / FunctionResult node state
Function metadata / flags
Signature inputs / outputs
```

Custom Event：

```text
Event graph node snapshot
Input pins
Node metadata
Node links? add/remove 不接入执行流，但 remove 可能删除事件体后续节点，需 snapshot 子图关系
```

注意：本簇 remove custom event 是“删除声明和事件体”。如果 Custom Event 后面有实现节点，删除可能涉及图体。  
第一版建议：

```text
remove_blueprint_custom_event 只允许删除没有执行体后继连接 / 没有 graph references 的 isolated custom_event。
如果事件体存在后继节点或连接，dry_run blocked，要求用户使用 Replace / Cleanup / Graph Write 工具处理。
```

这样保持 Signature Management 不承担图体删除。

### 16.3 失败回滚

规则：

```text
1. 写入前 snapshot。
2. 写入中失败 → rollback。
3. rollback 成功 → modified=false。
4. rollback 失败 → modified=true / error.code=rollback_failed。
```

### 16.4 内部 Journal

UE 内部可按全局写工具策略记录：

```text
Transaction Journal
Review Store
rollback_data
```

Agent-facing 成功结果不返回：

```text
write_ref
transaction_id
journal_recorded
review
safety
```

---

## 17. Phase P：validation 规则

### 17.1 add function / custom event

```text
should_compile=true
should_save=true
```

### 17.2 set function / event properties

```text
should_compile=true
should_save=true
```

因为 `pure / callable / const / deprecated` 等属性可能影响编译或节点可见性。

### 17.3 remove function / custom event

```text
should_compile=true
should_save=true
```

### 17.4 no_op

```text
should_compile=false
should_save=false
```

### 17.5 read 工具

不返回 validation。

### 17.6 禁止字段

validation 不返回：

```text
compiled
saved
```

---

## 18. Phase Q：与 Graph Write / Replace 的边界

### 18.1 Graph Write

Signature Management 创建函数 / Custom Event 后，不写实现体。

推荐 Agent 流程：

```text
1. add_blueprint_function / add_blueprint_custom_event
2. ReplaceBlueprintGraph with replace_scope=function_body/event_body
3. compile_blueprint_asset
4. save_asset
```

### 18.2 ReplaceBlueprintGraph

已有规则保持：

```text
function_definition / event_definition：
  替换完整定义。
  若有 external_dependents，默认阻止并报告。

function_body / event_body：
  只替换内部实现。
  保留入口、签名、参数、返回值、调用身份稳定。
  不因 external_dependents 直接阻止，但 dry_run 必须报告。
```

本簇不替代 Replace。

### 18.3 Custom Event 接入执行流

`add_blueprint_custom_event` 不接入执行流。  
接入已有执行流必须：

```text
MergeBlueprintGraph
```

并显式目标接入点 / 插入策略。

---

## 19. Phase R：与 Blueprint Interface 的边界

本簇不编辑 Blueprint Interface 资产函数签名。

不负责：

```text
编辑 BPI 资产函数签名
添加接口函数到 BPI
删除接口函数
修改接口函数参数
```

接口相关由：

```text
Blueprint Interface Asset Editing
```

实现接口函数体由：

```text
Graph Write / Replace function_body
```

---

## 20. Phase S：RequestValidator / 权限

### 20.1 read_blueprint_functions

```cpp
RequireString(Payload, TEXT("asset_path"));
```

### 20.2 read_blueprint_events

```cpp
RequireString(Payload, TEXT("asset_path"));
```

### 20.3 add_blueprint_function

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("function_name"));
OptionalString(Payload, TEXT("name_collision")); // fail_if_exists | reuse_if_exists
OptionalArray(Payload, TEXT("inputs"));
OptionalArray(Payload, TEXT("outputs"));
OptionalString(Payload, TEXT("category"));
OptionalString(Payload, TEXT("tooltip"));
OptionalString(Payload, TEXT("access"));
OptionalBool(Payload, TEXT("pure"));
OptionalBool(Payload, TEXT("callable"));
OptionalBool(Payload, TEXT("const"));
OptionalBool(Payload, TEXT("deprecated"));
OptionalString(Payload, TEXT("deprecation_message"));

RejectField(Payload, TEXT("function_body"));
RejectField(Payload, TEXT("graph"));
RejectField(Payload, TEXT("nodes"));
```

### 20.4 add_blueprint_custom_event

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("event_name"));
OptionalString(Payload, TEXT("name_collision"));
OptionalArray(Payload, TEXT("inputs"));
OptionalString(Payload, TEXT("category"));
OptionalString(Payload, TEXT("tooltip"));
OptionalBool(Payload, TEXT("deprecated"));
OptionalString(Payload, TEXT("deprecation_message"));

RejectField(Payload, TEXT("event_body"));
RejectField(Payload, TEXT("connect_to"));
RejectField(Payload, TEXT("merge_strategy"));
RejectField(Payload, TEXT("nodes"));
```

### 20.5 set_blueprint_function_properties

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("function_name"));
RequireObject(Payload, TEXT("properties"));

RejectProperties(Payload.properties, [
  "function_name",
  "inputs",
  "outputs",
  "parameters",
  "return_values",
  "signature",
  "parameter_type",
  "parameter_order"
]);
```

### 20.6 set_blueprint_event_properties

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("event_name"));
RequireObject(Payload, TEXT("properties"));

RejectProperties(Payload.properties, [
  "event_name",
  "inputs",
  "parameters",
  "parameter_type",
  "signature"
]);
```

### 20.7 remove tools

```cpp
RequireString(Payload, TEXT("asset_path"));
RequireString(Payload, TEXT("function_name")); // function
RequireString(Payload, TEXT("event_name"));    // event
OptionalBool(Payload, TEXT("dry_run"));
```

### 20.8 权限

读工具：

```text
read_blueprint_functions
read_blueprint_events
```

只读，ReadOnly 下允许，不需要 write token。

写工具：

```text
add/set/remove function/event
```

需要写权限 / Token，并受 Safety Profile 约束。

删除工具高风险：

```text
remove_blueprint_function
remove_blueprint_custom_event
```

必须 dry_run，且 blocked 时不得继续正式写。

---

## 21. Phase T：Bridge Router 接入

### 21.1 新增 commands

```text
read_blueprint_functions
read_blueprint_events
add_blueprint_function
add_blueprint_custom_event
set_blueprint_function_properties
set_blueprint_event_properties
remove_blueprint_function
remove_blueprint_custom_event
```

### 21.2 Router 分支

```cpp
if (Request.Command == TEXT("read_blueprint_functions"))
{
    return MakeBridgeResponse(Request, SignatureService.ReadBlueprintFunctions(Request.Payload));
}
if (Request.Command == TEXT("read_blueprint_events"))
{
    return MakeBridgeResponse(Request, SignatureService.ReadBlueprintEvents(Request.Payload));
}
if (Request.Command == TEXT("add_blueprint_function"))
{
    return MakeBridgeResponse(Request, SignatureService.AddBlueprintFunction(Request.Payload));
}
if (Request.Command == TEXT("add_blueprint_custom_event"))
{
    return MakeBridgeResponse(Request, SignatureService.AddBlueprintCustomEvent(Request.Payload));
}
...
```

所有 handler 必须走统一：

```text
MakeBridgeResponse
```

不要根据 dry_run blocked / business no_op 二次改写 Bridge success/error。

---

## 22. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperSignatureContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperFunctionSignatureTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperEventSignatureTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperSignatureDryRunTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperSignatureRollbackTests.cpp
```

### 22.1 Contract tests

```text
1. read_blueprint_functions_contract
   - data.schema=ReadBlueprintFunctions.v1
   - functions.function_count/items
   - inputs/outputs present
   - no function body graph / graph_id / function_guid

2. read_blueprint_events_contract
   - data.schema=ReadBlueprintEvents.v1
   - events.event_count/items
   - event_kind present
   - no event body graph / graph_id / event_guid

3. add_blueprint_function_success_contract
   - function_result.success=true
   - no counts/guid/entry_node_ref/graph_id/reused_existing
   - validation should_compile/should_save only

4. add_blueprint_function_no_op_contract
   - status=no_op
   - success=true
   - no reused_existing

5. add_blueprint_custom_event_success_contract
   - event_result.success=true
   - no execution flow info

6. set_blueprint_function_properties_success_contract
   - property_result.success=true
   - no before/after/signature_diff

7. set_blueprint_function_properties_rejects_signature_mutation_contract
   - ok=false
   - error.code=unsupported_signature_mutation
   - conflicts[].property=inputs or outputs

8. remove_blueprint_function_dry_run_passed_contract
   - status=dry_run
   - dry_run result/can_execute only

9. remove_blueprint_function_dry_run_blocked_external_dependents_contract
   - blocked_by external_dependents_exist

10. remove_blueprint_custom_event_dry_run_blocked_event_kind_contract
   - event_kind_not_removable for BeginPlay / interface_event

11. remove_blueprint_custom_event_success_contract
   - event_result.success=true
```

### 22.2 Function runtime tests

```text
1. add_function_no_params
2. add_function_with_input_output_params
3. add_function_fail_if_exists
4. add_function_reuse_if_exists_no_op
5. add_function_does_not_create_body_nodes
6. set_function_category_tooltip
7. set_function_pure_callable_const
8. set_function_rejects_rename
9. set_function_rejects_input_change
10. remove_function_without_callers
11. remove_function_blocked_with_internal_callers
12. remove_function_blocked_with_external_dependents
```

### 22.3 Event runtime tests

```text
1. read_events_classifies_custom_and_engine_events
2. add_custom_event_no_inputs
3. add_custom_event_with_inputs
4. add_custom_event_does_not_connect_exec_flow
5. add_custom_event_reuse_if_exists_no_op
6. set_custom_event_tooltip
7. set_event_rejects_input_change
8. remove_custom_event_without_references
9. remove_engine_event_blocked
10. remove_interface_event_blocked
11. remove_custom_event_blocked_with_body_connections
```

### 22.4 Rollback tests

```text
1. add_function_failure_rolls_back
2. add_custom_event_failure_rolls_back
3. set_function_properties_failure_rolls_back
4. set_event_properties_failure_rolls_back
5. remove_function_failure_rolls_back
6. remove_custom_event_failure_rolls_back
7. rollback_failed_sets_modified_true
```

---

## 23. 推荐提交顺序

### Commit 1：DTO / Enum / SignatureParam

```text
Add Signature DTOs
Add signature scope / read scope / event kind / error enums
Reuse VariableTypeService for parameter types
Add short schema serializers
```

验收：

```text
read DTO 不返回 graph/function body。
write success DTO 只 success=true。
```

### Commit 2：SignatureCompat skeleton

```text
Add SignatureCompat wrapper
Implement ResolveBlueprint
Implement low-level read function/event signatures skeleton
Compile against UE5.3+ source
```

验收：

```text
底层 API 被隔离在 compat 层。
```

### Commit 3：read functions/events

```text
Implement read_blueprint_functions
Implement read_blueprint_events
Classify event_kind
Suppress body graph fields
```

验收：

```text
函数/事件签名读取可用。
```

### Commit 4：add_blueprint_function

```text
Implement add function declaration
Support initial inputs/outputs
Support fail_if_exists/reuse_if_exists
Reject body graph fields
Return success=true only
```

验收：

```text
函数体为空，不写实现节点。
```

### Commit 5：add_blueprint_custom_event

```text
Implement add Custom Event node
Support inputs
Do not connect execution flow
Support fail_if_exists/reuse_if_exists
```

验收：

```text
Custom Event 只创建入口，不接入执行流。
```

### Commit 6：set function/event properties

```text
Implement non-signature property patches
Reject signature mutation fields
Handle no_op
```

验收：

```text
rename/signature mutation 全部失败。
```

### Commit 7：remove function dry_run / execute

```text
Implement function delete dry_run
Check internal callers
Check external_dependents if capability available
Block inherited/interface functions
Implement delete and rollback
```

验收：

```text
external_dependents blocked。
```

### Commit 8：remove custom event dry_run / execute

```text
Implement custom event delete dry_run
Block engine_event/interface_event/override_event
Block body connections/references
Implement delete and rollback
```

验收：

```text
只有 custom_event 可删。
```

### Commit 9：Bridge / Validator / Auth

```text
Register all signature commands
Add request validators
Classify read/write permissions
Use ToolResultBuilder/MakeBridgeResponse
```

验收：

```text
读工具 ReadOnly 可用。
写工具需要 token。
删除必须 dry_run。
```

### Commit 10：Internal Journal / rollback integration

```text
Record internal Journal / Review for signature writes
Store rollback snapshots
Suppress transaction_id in Agent-facing success
```

验收：

```text
成功不返回 write_ref/transaction_id/review/safety。
```

### Commit 11：Contract regression

```text
Add success shape tests
Add no body graph tests
Add no forbidden fields tests
Add dry_run blocked tests
```

验收：

```text
字段稿验收项全部通过。
```

---

## 24. 第一版不做的内容

```text
1. 不写函数体逻辑。
2. 不写事件体逻辑。
3. 不接入 EventGraph 执行流。
4. 不支持 rename_blueprint_function。
5. 不支持 rename_blueprint_event。
6. 不支持修改已有函数 / 事件签名。
7. 不支持 add/remove/reorder existing parameters。
8. 不支持自动迁移调用点。
9. 不支持删除 inherited function。
10. 不支持删除 interface implementation function。
11. 不支持删除 engine_event / interface_event / override_event。
12. 不编辑 Blueprint Interface Asset 函数签名。
13. 不返回 function_guid / event_guid。
14. 不返回 entry_node_ref / graph_id。
15. 不返回 write_ref / transaction_id / review / safety。
```

---

## 25. 最小验收标准

读取函数签名：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_blueprint_functions",
  "trace_id": "trace_20260503_6601",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "read_scope": "function_signatures"
  },
  "data": {
    "schema": "ReadBlueprintFunctions.v1",
    "functions": {
      "function_count": 1,
      "items": [
        {
          "function_name": "TogglePhysicsDoor",
          "category": "Door",
          "access": "public",
          "pure": false,
          "callable": true,
          "inputs": [
            {
              "name": "bOpen",
              "type": {
                "category": "bool",
                "container": "single"
              }
            }
          ],
          "outputs": []
        }
      ]
    }
  }
}
```

添加函数：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_function",
  "trace_id": "trace_20260503_6701",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "function"
  },
  "data": {
    "schema": "AddBlueprintFunction.v1",
    "function_result": {
      "success": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

添加 Custom Event：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "add_blueprint_custom_event",
  "trace_id": "trace_20260503_6801",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "custom_event"
  },
  "data": {
    "schema": "AddBlueprintCustomEvent.v1",
    "event_result": {
      "success": true
    }
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

签名 mutation 失败：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_blueprint_function_properties",
  "trace_id": "trace_20260503_6902",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "function"
  },
  "error": {
    "code": "unsupported_signature_mutation",
    "stage": "validate_properties",
    "message": "Changing an existing function signature is not supported in the first version.",
    "retryable": false,
    "conflicts": [
      {
        "code": "unsupported_property",
        "property": "inputs"
      }
    ]
  }
}
```

删除函数 dry_run blocked：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "remove_blueprint_function",
  "trace_id": "trace_20260503_7102",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "signature_scope": "function"
  },
  "data": {
    "schema": "RemoveBlueprintFunctionDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": ["external_dependents_exist"],
      "conflicts": [
        {
          "code": "external_dependents_exist",
          "function_name": "TogglePhysicsDoor",
          "external_dependent_count": 2,
          "message": "The function is called by external assets."
        }
      ],
      "errors": []
    }
  }
}
```

必须不出现：

```text
requested_count
added_count
removed_count
changed_count
no_op_count
reused_existing
function_guid
event_guid
entry_node_ref
graph_id
function_body_graph
event_body_graph
node list
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

### 26.1 add function 误写函数体

风险：

```text
创建函数时顺便生成默认实现节点或 BlueprintHelper-owned block。
```

处理：

```text
add_blueprint_function 只创建声明/入口/签名。
函数体由 Replace/Graph Write 后续处理。
Contract test 检查无业务节点。
```

### 26.2 add custom event 误接入执行流

风险：

```text
Custom Event 创建后自动接到 BeginPlay 或现有执行链。
```

处理：

```text
add_blueprint_custom_event 不创建任何 Exec 连接。
接入执行流必须走 MergeBlueprintGraph。
```

### 26.3 签名修改误入 properties 工具

风险：

```text
set_blueprint_function_properties 接收到 inputs/outputs 并尝试修改。
```

处理：

```text
Validator 和 Service 双重拒绝。
错误码 unsupported_signature_mutation。
```

### 26.4 删除函数外部依赖检查不足

风险：

```text
外部 Blueprint 仍调用该函数，删除后其他资产编译失败。
```

处理：

```text
dry_run 必须检查 external_dependents。
检查能力不可用时 Conservative 下 blocked，不声称安全。
```

### 26.5 删除 Custom Event 误删引擎事件

风险：

```text
remove_blueprint_custom_event 传入 BeginPlay。
```

处理：

```text
Classify event_kind。
非 custom_event dry_run blocked。
```

### 26.6 成功返回泄露内部 Graph ID

风险：

```text
为了便于后续 Replace，add function 返回 graph_id / entry_node_ref。
```

处理：

```text
本簇成功只 success=true。
后续 Replace 通过 read logic / explicit function_name 定位。
