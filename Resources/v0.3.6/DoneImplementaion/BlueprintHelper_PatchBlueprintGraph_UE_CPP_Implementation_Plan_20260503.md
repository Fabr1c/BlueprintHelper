# BlueprintHelper PatchBlueprintGraph UE 侧 C++ 可执行实现计划

状态：[x] 已完成
日期：2026-05-03  
输入文档：`BlueprintHelper_PatchBlueprintGraph_UE_FieldMapping_20260503.md`  
适用范围：UE 插件侧 / C++ 实现  
目标版本：BlueprintHelper v0.4 / v0.5 前置协议收敛  

---

## 0. 计划目标

本计划把 `PatchBlueprintGraph` 字段映射文档转成 UE 侧可直接执行的 C++ 实现任务。

`PatchBlueprintGraph` 的目标不是追加完整逻辑、不是替换完整实现、也不是接入执行流，而是：

```text
精确修改一个明确目标点。
```

可修改范围：

```text
节点属性
Pin 默认值
Pin 连接
单条 Link
节点位置 / 注释 / 展示属性
BlueprintHelper-owned block 内的局部节点
```

与其他 Graph Write 工具的边界：

```text
AppendBlueprintGraph：追加新的独立 owned 逻辑块。
ReplaceBlueprintGraph：替换明确目标的完整实现。
MergeBlueprintGraph：接入已有执行流。
PatchBlueprintGraph：精确修改节点 / Pin / 默认值 / 连接。
```

本计划只覆盖 UE 插件侧 C++，不覆盖 MCP Server TypeScript 参数声明、Agent Skill 文档或用户引导文档。

---

## 1. 当前代码可复用基线

基于 `UE侧v0.3.4` 现有结构和前两份 Append / Replace 实现计划，Patch 应复用以下能力：

| 现有 / 已规划模块 | 当前能力 | Patch 中的用途 |
|---|---|---|
| `FBlueprintHelperGraphResolver` | 解析 Blueprint / Graph / FunctionGraph / EventGraph | 定位目标蓝图与目标图。 |
| `FBlueprintHelperScopedAssetMutation` | 基于 `FScopedTransaction` 的写入事务与 rollback | Patch 写入中失败时回滚。 |
| `FBlueprintHelperToolResultBase` | 统一 ToolResultBase | 输出 Patch 极简 Agent-facing 结果。 |
| `FBlueprintHelperWriteRef` | Graph Write 成功返回 transaction handle | 复用 `data.write_ref`。 |
| `FBlueprintHelperTransactionJournalService` | 写 Transaction Journal / Review Store | 记录 before / after / rollback_data；Agent-facing 不默认展开。 |
| `FBlueprintHelperOwnershipService` | 读取 / 写入 BlueprintHelper metadata | 判断 owned 节点，支持 Patch owned block 内节点。 |
| `FBlueprintHelperLogicJsonPathService`（需新增） | LogicJson group ref/path 反推 | 把 `node_ref / pin_ref / link_ref` 转换为 UE 节点 / Pin / Link。 |
| `UEdGraphSchema_K2` | Pin 连接合法性检查与创建连接 | `connect_pins / replace_link` 的核心 API。 |

不建议直接复用 `AgentImportGraph` 或 GraphBuildCore 的返回结构，因为 Patch 字段文档要求成功结果极简，不返回 `summary / before / after / old_value / new_value / diagnostics / ownership / review / safety`。

---

## 2. 字段契约硬约束

实现时必须满足以下字段契约。

### 2.1 operation

```json
"operation": "patch_blueprint_graph"
```

### 2.2 成功 data.schema

```json
"schema": "PatchBlueprintGraph.v1"
```

### 2.3 dry_run data.schema

```json
"schema": "PatchBlueprintGraphDryRun.v1"
```

### 2.4 成功返回只允许

```text
ok
schema
operation
trace_id
status
modified
target.asset_path
target.graph
target.patch_scope
data.schema
data.patch_result.patched_ref.graph_id
data.patch_result.patched_ref.node_ref
data.patch_result.patched_ref.pin_ref
data.patch_result.patched_ref.link_ref
data.patch_result.patched_ref.node_path   // 仅必要时
data.patch_result.patched_ref.pin_path    // 仅必要时
data.patch_result.patched_ref.link_path   // 仅必要时
data.patch_result.patch.patch_type
data.patch_result.patch.expected_old_state_provided
data.patch_result.patch.changed
data.write_ref.transaction_id
data.write_ref.journal_recorded
validation.should_compile
validation.should_save
validation.compiled
validation.saved
```

### 2.5 成功返回禁止

```text
target.target_type
target_kind
summary
before
after
old_value
new_value
patch_plan
full_diff
modified_nodes / modified_pins 计数
created_links / deleted_links 计数
ownership
review
safety
diagnostics
next
journal_path
rollback_data
```

### 2.6 dry_run 返回

`dry_run passed` 只返回：

```json
{
  "data": {
    "schema": "PatchBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

`dry_run blocked` 只返回：

```json
{
  "data": {
    "schema": "PatchBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [],
      "conflicts": [],
      "errors": []
    }
  }
}
```

不得返回：

```text
patch_plan
would_modify_nodes
would_create_links
would_delete_links
before
after
ownership
review
safety
diagnostics
next
```

### 2.7 正式失败返回

正式失败不返回：

```text
data.patch_result
data.write_ref
ownership
review
safety
diagnostics
next
```

但 `error` 必须包含：

```text
code
stage
message
retryable
rollback_result
failed_item    // 可选
conflicts      // 可选
```

---

## 3. Phase A：新增 Patch 专属类型文件

新增文件：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperPatchGraphTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperPatchGraphTypes.cpp
```

### 3.1 新增枚举

```cpp
enum class EBlueprintHelperPatchScope : uint8
{
    PinDefault,
    NodeProperty,
    NodeComment,
    NodePosition,
    ConnectPins,
    DisconnectLink,
    ReplaceLink,
    CallTarget,
    LocalVariableRef
};

enum class EBlueprintHelperPatchType : uint8
{
    SetPinDefault,
    SetNodeProperty,
    SetNodeComment,
    SetNodePosition,
    ConnectPins,
    DisconnectLink,
    ReplaceLink,
    SetCallTarget,
    RenameLocalVariableRef
};

enum class EBlueprintHelperPatchErrorCode : uint8
{
    InvalidPatchSchema,
    TargetBlueprintNotFound,
    TargetNotBlueprint,
    TargetGraphNotFound,
    TargetGroupNotFound,
    TargetNodeNotFound,
    TargetPinNotFound,
    TargetLinkNotFound,
    TargetAmbiguous,
    CrossGroupRefDisallowed,
    DisplayNameOnlyDisallowed,
    ExpectedOldStateMismatch,
    NodePropertyNotFound,
    NodePropertyNotWritable,
    PinDefaultNotWritable,
    PinTypeMismatch,
    LinkCreateFailed,
    LinkDisconnectFailed,
    UnsupportedPatchType,
    WritePermissionDisabled,
    ProfilePolicyViolation,
    JournalWriteFailed,
    RollbackBlocked,
    RollbackFailed,
    BridgeDisconnected
};

enum class EBlueprintHelperGraphWriteStage : uint8
{
    ParseInput,
    Auth,
    ResolveTarget,
    ResolveLogicJsonRef,
    Preflight,
    ReadBeforeState,
    ApplyPatch,
    ConnectPins,
    DisconnectPins,
    WriteJournal,
    Rollback
};
```

`EBlueprintHelperRollbackResult` 继续复用 Graph Write 公共枚举：

```text
not_needed
rolled_back
blocked
failed
```

---

## 4. Phase B：新增 Patch 专属结果结构

字段文档建议的 UE 侧结构为：

```cpp
struct FBlueprintHelperPatchGraphResultData
{
    FString Schema; // PatchBlueprintGraph.v1
    FBlueprintHelperPatchGraphResult PatchResult;
    FBlueprintHelperWriteRef WriteRef;
};

struct FBlueprintHelperPatchGraphResult
{
    FBlueprintHelperPatchedRef PatchedRef;
    FBlueprintHelperPatchSummary Patch;
};

struct FBlueprintHelperPatchedRef
{
    FString GraphId;

    // Default local refs.
    FString NodeRef;
    FString PinRef;
    FString LinkRef;

    // Optional fallback only when local refs are insufficient.
    FString NodePath;
    FString PinPath;
    FString LinkPath;
};

struct FBlueprintHelperPatchSummary
{
    FString PatchType;
    bool bExpectedOldStateProvided;
    bool bChanged;
};
```

落地时建议补齐 `ToJson()`，并保证空字段不输出。

```cpp
struct FBlueprintHelperPatchedRef
{
    FString GraphId;
    TOptional<FString> NodeRef;
    TOptional<FString> PinRef;
    TOptional<FString> LinkRef;
    TOptional<FString> NodePath;
    TOptional<FString> PinPath;
    TOptional<FString> LinkPath;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperPatchSummary
{
    EBlueprintHelperPatchType PatchType;
    bool bExpectedOldStateProvided = false;
    bool bChanged = false;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperPatchGraphResult
{
    FBlueprintHelperPatchedRef PatchedRef;
    FBlueprintHelperPatchSummary Patch;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperPatchGraphResultData
{
    FString Schema = TEXT("PatchBlueprintGraph.v1");
    FBlueprintHelperPatchGraphResult PatchResult;
    FBlueprintHelperWriteRef WriteRef;

    TSharedRef<FJsonObject> ToJson() const;
};
```

禁止在这些结构中加入：

```cpp
FString TargetKind;
FString Summary;
FString Before;
FString After;
FString OldValue;
FString NewValue;
```

这些只能进入 Journal / Review / verbose/debug。

---

## 5. Phase C：定义 Patch 请求结构

新增：

```cpp
struct FBlueprintHelperPatchGraphRequest
{
    FString AssetPath;
    FString GraphName;
    EBlueprintHelperPatchScope PatchScope;
    EBlueprintHelperPatchType PatchType;
    bool bDryRun = false;

    FBlueprintHelperLogicJsonTargetRef TargetRef;
    TSharedPtr<FJsonObject> PatchPayload;

    bool bExpectedOldStateProvided = false;
    TSharedPtr<FJsonObject> ExpectedOldState;
    TOptional<FString> ExpectedOldValue;
};
```

其中 `TargetRef` 建议统一承载 LogicJson 定位信息：

```cpp
struct FBlueprintHelperLogicJsonTargetRef
{
    FString GraphId;

    // Group context. Required when local refs are used.
    TOptional<FString> GroupEntryNodePath;
    TOptional<FString> GroupRef;

    // Default local refs.
    TOptional<FString> NodeRef;
    TOptional<FString> PinRef;
    TOptional<FString> LinkRef;

    // Fallback full paths.
    TOptional<FString> NodePath;
    TOptional<FString> PinPath;
    TOptional<FString> LinkPath;

    // Optional raw UE fallback. Expert/debug only.
    TOptional<FGuid> NodeGuid;
    TOptional<FGuid> PinGuid;
};
```

输入解析规则：

```text
1. patch_scope 必填。
2. patch_type 必填。
3. graph 必填。
4. node_ref / pin_ref / link_ref 必须携带 group 上下文，或能从 request 上下文唯一反推。
5. 如果只提供显示名 / Pin 名，不允许执行。
6. node_path / pin_path / link_path 可作为 fallback。
7. UE GUID 可作为底层 fallback，但不应成为 Agent 默认路径。
```

---

## 6. Phase D：新增 LogicJson 定位服务

Patch 的关键不是如何写值，而是如何稳定定位目标。新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperLogicJsonPathService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperLogicJsonPathService.cpp
```

### 6.1 服务职责

```cpp
class FBlueprintHelperLogicJsonPathService
{
public:
    bool ResolveNode(
        UEdGraph* Graph,
        const FBlueprintHelperLogicJsonTargetRef& TargetRef,
        UEdGraphNode*& OutNode,
        FBlueprintHelperPatchResolveError& OutError) const;

    bool ResolvePin(
        UEdGraph* Graph,
        const FBlueprintHelperLogicJsonTargetRef& TargetRef,
        UEdGraphPin*& OutPin,
        FBlueprintHelperPatchResolveError& OutError) const;

    bool ResolveLink(
        UEdGraph* Graph,
        const FBlueprintHelperLogicJsonTargetRef& TargetRef,
        FBlueprintHelperResolvedLink& OutLink,
        FBlueprintHelperPatchResolveError& OutError) const;

    FString BuildFullNodePathFromGroup(
        const FString& GroupEntryNodePath,
        const FString& NodeRef) const;

    FString BuildFullPinPathFromGroup(
        const FString& GroupEntryNodePath,
        const FString& NodeRef,
        const FString& PinRef) const;

    FString BuildFullLinkPathFromGroup(
        const FString& GroupEntryNodePath,
        const FString& NodeRef,
        const FString& LinkRef) const;
};
```

### 6.2 LogicJson path 规则

必须实现并测试：

```text
1. target_graph / blueprint / multi_target 使用 logic.groups[]。
2. node_ref / link_ref / pin_ref 是 group 内局部引用。
3. 不得跨 group 使用局部引用。
4. 需要完整路径时，从 group.entry.node_path 反推。
5. 仅靠显示名 / Pin 名不允许直接 Patch。
```

### 6.3 UE 节点查找策略

推荐优先级：

```text
1. node_path / pin_path / link_path 解析。
2. group_entry_node_path + node_ref / pin_ref / link_ref 反推路径解析。
3. UE node GUID / pin GUID fallback。
4. BlueprintHelper metadata / block_id scoped fallback。
5. 显示名仅用于错误消息，不作为定位依据。
```

### 6.4 link_ref 解析策略

LogicJson 第一版链接存储在 source node 的 outgoing `node.links` 内，不提供 top-level `logic.links`，也不提供 incoming_refs。

UE 侧解析 `link_ref` 时：

```text
1. 根据 group 上下文找到 source node。
2. 根据 link_ref 找到该 source node 的 outgoing link index 或 stable ref。
3. 根据 link 的目标 node_ref / pin_ref 查找目标 Pin。
4. 若 request 未提供 source node 上下文，必须 reverse-scan 当前 group 内所有 node.links。
5. 如果多个 link_ref 匹配，返回 target_ambiguous。
```

---

## 7. Phase E：expected_old_state / before-after 读取


新增：

```cpp
struct FBlueprintHelperPatchBeforeState
{
    FString StableRef;
    TSharedPtr<FJsonObject> StateJson;
    FString ValueText;
};

struct FBlueprintHelperPatchAfterState
{
    FString StableRef;
    TSharedPtr<FJsonObject> StateJson;
    FString ValueText;
};
```

### 7.1 读取 before state

按 patch_type 读取：

| patch_type | before state 内容 |
|---|---|
| `set_pin_default` | Pin 默认值、Pin 类型、Pin GUID、Owning Node GUID。 |
| `set_node_property` | 目标属性值、属性类型、是否 writable。 |
| `set_node_comment` | 原 NodeComment。 |
| `set_node_position` | NodePosX / NodePosY。 |
| `disconnect_link` | link 的 from/to Pin、Pin 类型。 |
| `replace_link` | 原 link from/to、新目标 Pin 预检信息。 |
| `set_call_target` | 原 function/event target。 |
| `rename_local_variable_ref` | 原变量名 / GUID / Scope。 |

### 7.2 expected_old_state 校验

```cpp
bool FBlueprintHelperPatchBlueprintGraphService::CheckExpectedOldState(
    const FBlueprintHelperPatchBeforeState& Current,
    const FBlueprintHelperPatchGraphRequest& Request,
    FBlueprintHelperPatchConflict& OutConflict) const;
```

校验规则：

```text
1. 未提供 expected_old_state：跳过匹配，但仍记录 before。
2. 提供 expected_old_value：按规范化文本比较。
3. 提供 expected_old_state：按 patch_type 所需字段比较。
4. 不匹配：expected_old_state_mismatch，modified=false，rollback_result=not_needed。
```

### 7.3 成功结果只暴露布尔

Agent-facing 成功结果只返回：

```json
"expected_old_state_provided": true
```

不返回 before / after / old_value / new_value。

---

## 8. Phase F：Patch 执行器设计

新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperPatchBlueprintGraphService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperPatchBlueprintGraphService.cpp
```

### 8.1 Service skeleton

```cpp
class FBlueprintHelperPatchBlueprintGraphService
{
public:
    FBlueprintHelperPatchBlueprintGraphService(
        const FBlueprintHelperGraphResolver& InGraphResolver,
        const FBlueprintHelperLogicJsonPathService& InPathService,
        const FBlueprintHelperTransactionJournalService& InJournalService);

    FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool ParseRequest(
        const TSharedPtr<FJsonObject>& Payload,
        FBlueprintHelperPatchGraphRequest& OutRequest,
        FBlueprintHelperToolError& OutError) const;

    bool ResolveTarget(
        const FBlueprintHelperPatchGraphRequest& Request,
        UBlueprint*& OutBlueprint,
        UEdGraph*& OutGraph,
        FBlueprintHelperResolvedPatchTarget& OutTarget,
        FBlueprintHelperToolError& OutError) const;

    bool Preflight(
        const FBlueprintHelperPatchGraphRequest& Request,
        UBlueprint* Blueprint,
        UEdGraph* Graph,
        const FBlueprintHelperResolvedPatchTarget& Target,
        FBlueprintHelperPatchBeforeState& OutBeforeState,
        TArray<FBlueprintHelperConflictItem>& OutConflicts,
        TArray<FBlueprintHelperDryRunIssue>& OutErrors) const;

    FBlueprintHelperToolResultBase ExecuteDryRun(... ) const;
    FBlueprintHelperToolResultBase ExecuteWrite(... ) const;

    bool ApplyPatch(
        const FBlueprintHelperPatchGraphRequest& Request,
        UBlueprint* Blueprint,
        UEdGraph* Graph,
        const FBlueprintHelperResolvedPatchTarget& Target,
        bool& bOutChanged,
        FBlueprintHelperToolError& OutError) const;
};
```

### 8.2 Resolved target

```cpp
struct FBlueprintHelperResolvedPatchTarget
{
    TOptional<UEdGraphNode*> Node;
    TOptional<UEdGraphPin*> Pin;
    TOptional<FBlueprintHelperResolvedLink> Link;

    FBlueprintHelperPatchedRef PatchedRef;
};

struct FBlueprintHelperResolvedLink
{
    UEdGraphNode* SourceNode = nullptr;
    UEdGraphPin* SourcePin = nullptr;
    UEdGraphNode* TargetNode = nullptr;
    UEdGraphPin* TargetPin = nullptr;
    FString LinkRef;
};
```

---

## 9. Phase G：各 patch_type 的 UE API 落点

### 9.1 set_pin_default

目标：修改 Pin 默认值。

核心 API：

```cpp
UEdGraphPin* Pin = Target.Pin.Get(nullptr);
Pin->Modify();
Pin->DefaultValue = NewValue;
```

对象 / 文本 / 名称默认值分别处理：

```cpp
Pin->DefaultObject = NewObject;
Pin->DefaultTextValue = FText::FromString(NewText);
Pin->DefaultValue = NewValue;
```

必须调用：

```cpp
Graph->NotifyGraphChanged();
FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
```

预检：

```text
1. Pin 必须存在。
2. Pin 必须允许默认值。
3. Pin 不能已有连接且不允许同时有默认值。
4. 值类型必须与 PinType 兼容。
```

错误码：

```text
target_pin_not_found
pin_default_not_writable
type_mismatch
expected_old_state_mismatch
```

### 9.2 set_node_property

目标：修改节点 UObject 属性或 K2Node 专属属性。

实现方式：

```cpp
FProperty* Property = FindFProperty<FProperty>(Node->GetClass(), *PropertyName);
if (!Property || !CanEditProperty(Property)) { ... }
Node->Modify();
SetPropertyValueFromJson(Property, Node, NewValueJson);
Node->ReconstructNode(); // 视属性类型决定
```

限制：

```text
1. 第一版只允许白名单属性。
2. 不允许任意 UObject 属性路径无限制写入。
3. 不允许修改破坏节点身份的关键字段，除非 patch_type 是 set_call_target。
```

白名单建议：

```text
bAdvancedView
EnabledState
NodeComment
NodePosX / NodePosY  // 也可由专用 patch_type 处理
```

### 9.3 set_node_comment

```cpp
Node->Modify();
Node->NodeComment = NewComment;
Node->bCommentBubbleVisible = bVisible;
```

注意：如果节点是 BlueprintHelper-owned，不能覆盖 ownership comment 的结构化前缀。推荐策略：

```text
1. BlueprintHelper metadata 仍是 ownership 事实来源。
2. NodeComment 若已有 [BlueprintHelper] block，应保留 managed header。
3. 用户 comment 写在 managed header 后方。
```

### 9.4 set_node_position

```cpp
Node->Modify();
Node->NodePosX = NewX;
Node->NodePosY = NewY;
Graph->NotifyGraphChanged();
```

此类修改通常：

```text
should_compile=false
should_save=true
```

但 Graph Write 统一可返回 `should_compile=true`，更保守；如果要优化，可按 patch_type 决定。

### 9.5 connect_pins

核心 API：

```cpp
const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
FPinConnectionResponse Response = Schema->CanCreateConnection(FromPin, ToPin);
if (Response.Response == CONNECT_RESPONSE_DISALLOW) { ... }

FromPin->Modify();
ToPin->Modify();
bChanged = Schema->TryCreateConnection(FromPin, ToPin);
```

限制：

```text
1. 只用于新建一条连接。
2. 如果目标连接已存在，返回 changed=false 或 no_op，仍写 Journal。
3. 如果 Exec Pin 已有后继且 schema 不允许多连，不能自动断开；应返回 conflict，让 Merge 处理执行流重排。
```

### 9.6 disconnect_link

```cpp
FromPin->Modify();
ToPin->Modify();
FromPin->BreakLinkTo(ToPin);
```

必须精确定位单条 link。

禁止：

```text
1. 仅凭 pin 名断开所有连接。
2. 批量 BreakAllPinLinks。
3. 模糊删除节点全部连接。
```

### 9.7 replace_link

流程：

```text
1. Resolve old link。
2. Resolve new target pin / source pin。
3. CanCreateConnection(new_from, new_to)。
4. Modify old pins / new pins。
5. Break old link。
6. TryCreateConnection new link。
7. 若连接失败，rollback。
```

注意：

```text
replace_link 是高风险连接修改，建议或要求 expected_old_state。
影响 Exec 流时 Conservative 下必须 dry_run。
```

### 9.8 set_call_target

适用于：

```text
UK2Node_CallFunction
UK2Node_CallDelegate
UK2Node_CustomEvent 引用类节点视具体类型而定
```

第一版建议收窄：

```text
只支持 UK2Node_CallFunction 的函数引用目标切换。
```

实现：

```cpp
UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
CallNode->Modify();
CallNode->SetFromFunction(NewFunction);
CallNode->ReconstructNode();
```

预检：

```text
1. 新函数存在。
2. 新旧签名兼容，或明确允许 Reconstruct 后重新连线。
3. 不允许静默断开外部连接。
```

第一版若无法保证签名兼容，建议返回：

```text
unsupported_patch_type 或 call_signature_mismatch
```

由 Replace 处理完整函数调用节点重建。

### 9.9 rename_local_variable_ref

第一版建议降级为后置能力。

原因：

```text
Local variable ref 涉及函数图、变量声明、Get/Set 节点、GUID、作用域和重构影响。
```

如果保留枚举，第一版实现可返回：

```text
unsupported_patch_type
```

并在 runtime profile unavailable 中标记高级 local variable ref patch 不可用。

---

## 10. Phase H：dry_run 实现

Patch dry_run 必须执行完整 preflight，但不修改资产。

流程：

```text
1. ParseRequest。
2. Resolve Blueprint / Graph。
3. Resolve LogicJson target ref。
4. Read before state。
5. Check expected_old_state。
6. Check patch_type 可执行性。
7. 对连接类 patch 调用 CanCreateConnection。
8. 返回极简 dry_run result。
```

### 10.1 dry_run passed

```cpp
return MakePatchDryRunPassedResult(Request.Target);
```

输出：

```json
{
  "ok": true,
  "operation": "patch_blueprint_graph",
  "status": "dry_run",
  "modified": false,
  "data": {
    "schema": "PatchBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

### 10.2 dry_run blocked

只输出：

```text
result
can_execute
blocked_by
conflicts
errors
```

常见 blocked_by：

```text
target_node_not_found
target_pin_not_found
target_link_not_found
target_ambiguous
cross_group_ref_disallowed
expected_old_state_mismatch
pin_type_mismatch
exec_flow_requires_merge
unsupported_patch_type
```

---

## 11. Phase I：正式写入流程

正式写入顺序：

```text
1. ParseRequest
2. ResolveBlueprint
3. ResolveGraph
4. Resolve LogicJson target
5. Read before state
6. Preflight / expected_old_state check
7. Begin FBlueprintHelperScopedAssetMutation
8. ApplyPatch
9. Read after state
10. Compare changed
11. Write Transaction Journal / Review record
12. Mark Blueprint / Graph modified
13. Return minimal success result
```

### 11.1 No-op 处理


```text
ok=true
status=applied
modified=false 或 true？
changed=false
```

建议：

```text
modified=false
patch.changed=false
validation.should_compile=false
validation.should_save=false
```

但如果内部仍写 Journal / Review，可能引入审计记录。第一版建议：

```text
No-op 不生成 transaction_id，不写 Journal，返回 status=no_op。
```

如果必须保持 Graph Write 成功结构，则可以：

```text
status=applied
modified=false
patch.changed=false
不返回 write_ref
```

但字段文档要求正式成功使用 `data.write_ref`。因此为了契约稳定，第一版建议把 no-op 也当作成功写工具调用：

```text
status=applied
modified=false
patch.changed=false
write_ref.transaction_id 仍生成
journal_recorded=true
validation.should_compile=false
validation.should_save=false
```

Journal 中标记 `changed=false`。

### 11.2 changed 判断

```cpp
bool bChanged = !ArePatchStatesEqual(BeforeState, AfterState);
```

Agent-facing 只返回：

```json
"changed": true
```

before / after 进入 Journal。

---

## 12. Phase J：Journal / Review 记录

Patch 成功或 no-op 均应写 Journal。Journal 不是 Agent-facing 成功结果镜像。

### 12.1 Patch Journal 最小结构

```json
{
  "schema": "BlueprintHelper.TransactionJournal.v1",
  "transaction_id": "tx_20260503_1401",
  "tool": "PatchBlueprintGraph",
  "status": "applied",
  "created_at": "2026-05-03T14:01:00Z",
  "target_assets": ["/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"],
  "graph": "EG_PhysicsDoor",
  "patch": {
    "patch_scope": "pin_default",
    "patch_type": "set_pin_default",
    "patched_ref": {
      "graph_id": "EG_PhysicsDoor",
      "node_ref": "Branch0",
      "pin_ref": "Condition"
    },
    "expected_old_state_provided": true,
    "changed": true
  },
  "before": {},
  "after": {},
  "rollback_data": {},
  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

### 12.2 rollback_data

按 patch_type 存储足够恢复的数据：

| patch_type | rollback_data |
|---|---|
| `set_pin_default` | 原 Pin 默认值 / DefaultObject / DefaultTextValue。 |
| `set_node_property` | 原属性值。 |
| `set_node_position` | 原 NodePosX / NodePosY。 |
| `connect_pins` | 新连接 from/to，用于 rollback 时断开。 |
| `disconnect_link` | 原连接 from/to，用于 rollback 时重连。 |
| `replace_link` | 原连接 from/to、新连接 from/to。 |
| `set_call_target` | 原函数引用 / 节点签名 / 断开的 Pin links。 |

---

## 13. Phase K：失败与 rollback

失败分支必须符合字段文档。

### 13.1 resolve/preflight 失败

```text
modified=false
rollback_result=not_needed
```

示例错误：

```text
target_pin_not_found
expected_old_state_mismatch
target_link_not_found
cross_group_ref_disallowed
```

### 13.2 写入中失败并成功回滚

```text
modified=false
rollback_result=rolled_back
```

示例错误：

```text
pin_type_mismatch
link_create_failed
link_disconnect_failed
node_property_not_writable
journal_write_failed
```

### 13.3 rollback blocked / failed

```text
modified=true
rollback_result=blocked / failed
```

Agent-facing error：

```json
{
  "code": "rollback_failed",
  "stage": "rollback",
  "message": "PatchBlueprintGraph failed and rollback could not restore the previous graph state.",
  "retryable": false,
  "rollback_result": "failed",
  "conflicts": [
    {
      "code": "asset_state_changed_during_write",
      "target": "/Game/..."
    }
  ]
}
```


---

## 14. Phase L：validation 规则

Patch 成功通常返回：

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

建议按 patch_type 精细化：

| patch_type | should_compile | should_save |
|---|---:|---:|
| `set_pin_default` | true | true |
| `set_node_property` | true | true |
| `set_node_comment` | false | true |
| `set_node_position` | false | true |
| `connect_pins` | true | true |
| `disconnect_link` | true | true |
| `replace_link` | true | true |
| `set_call_target` | true | true |
| `rename_local_variable_ref` | true | true |

如果实现复杂，第一版可保守统一：

```text
should_compile=true
should_save=true
compiled=false
saved=false
```

但 no-op 建议：

```text
should_compile=false
should_save=false
```

---

## 15. Phase M：Bridge Router 接入

### 15.1 新增 command

在 Bridge Router 中新增：

```cpp
if (Request.Command == TEXT("patch_blueprint_graph"))
{
    return HandlePatchBlueprintGraph(Request);
}
```

处理函数：

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandlePatchBlueprintGraph(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result = PatchGraphService.Execute(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("patch_blueprint_graph failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

### 15.2 RequestValidator

新增 payload 校验：

```cpp
if (CommandEquals(Command, TEXT("patch_blueprint_graph")))
{
    const FBlueprintHelperFieldRule Rules[] = {
        {TEXT("target"), EBlueprintHelperJsonExpectedType::Object, true},
        {TEXT("target.asset_path"), EBlueprintHelperJsonExpectedType::String, true},
        {TEXT("target.graph"), EBlueprintHelperJsonExpectedType::String, true},
        {TEXT("target.patch_scope"), EBlueprintHelperJsonExpectedType::String, true},
        {TEXT("patch_type"), EBlueprintHelperJsonExpectedType::String, true},
        {TEXT("patched_ref"), EBlueprintHelperJsonExpectedType::Object, true},
        {TEXT("patch"), EBlueprintHelperJsonExpectedType::Object, true},
        {TEXT("dry_run"), EBlueprintHelperJsonExpectedType::Bool, false}
    };
    return ValidateRules(Payload, Rules, OutError);
}
```

加入写命令集合：

```cpp
TEXT("patch_blueprint_graph")
```

这样 Token / write_permission 的现有写命令校验可复用。

---

## 16. Phase N：Build.cs 与 include

Patch 需要的核心 include：

```cpp
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"
```

现有模块通常需要：

```text
Core
CoreUObject
Engine
UnrealEd
BlueprintGraph
Kismet
Json
JsonUtilities
```

若 `UK2Node_CallFunction` 或 K2 schema 编译失败，优先检查 `BlueprintGraph` 是否在 PrivateDependencyModuleNames 中。

---

## 17. 推荐提交顺序

### Commit 1：Patch 类型与 JSON 序列化

```text
Add PatchBlueprintGraph result and dry_run types
Add patch scope/type/error/stage enums
Add patched_ref and patch summary JSON builders
```

验收：

```text
能构造 PatchBlueprintGraph.v1 / PatchBlueprintGraphDryRun.v1 JSON。
成功 JSON 不出现 before/after/old_value/new_value/ownership/review/safety/summary。
```

### Commit 2：LogicJson path resolver

```text
Add LogicJsonPathService
Resolve node_ref / pin_ref / link_ref with group context
Reject cross-group local refs
Reject display-name-only target
```

验收：

```text
node_ref + group_entry_node_path 能定位唯一节点。
pin_ref 能定位唯一 Pin。
link_ref 能定位唯一 outgoing link。
跨 group ref 返回 cross_group_ref_disallowed。
显示名-only 返回 display_name_only_disallowed。
```

### Commit 3：expected_old_state and before reader

```text
Add PatchBeforeState reader
Add expected_old_state comparison
Add per patch_type state normalization
```

验收：

```text
expected_old_state 匹配时通过。
expected_old_state 不匹配时返回 expected_old_state_mismatch。
未提供 expected_old_state 时仍记录 before。
```

### Commit 4：dry_run preflight

```text
Add PatchBlueprintGraphService dry_run path
Add preflight for pin default / node comment / node position / connect / disconnect / replace link
```

验收：

```text
dry_run passed 只返回 result/can_execute。
dry_run blocked 只返回 blocked_by/conflicts/errors。
dry_run 不修改资产。
```

### Commit 5：基础 patch apply

```text
Implement set_pin_default
Implement set_node_comment
Implement set_node_position
Implement connect_pins
Implement disconnect_link
Implement replace_link
```

验收：

```text
每个 patch_type 可成功修改目标。
Pin 连接使用 CanCreateConnection + TryCreateConnection。
Exec Pin 已有后继时不自动重排。
```

### Commit 6：transaction / journal / rollback

```text
Patch write path uses ScopedAssetMutation
Read before and after state
Write transaction journal
Rollback on failed apply or journal write failure
```

验收：

```text
写入失败可回滚。
Journal 写失败不能返回成功。
rollback blocked/failed 时 modified=true。
```

### Commit 7：Bridge command and validator

```text
Register patch_blueprint_graph command
Add request validation
Add write-command auth gate
```

验收：

```text
未携带 Token 时写命令被拒绝。
dry_run 不需要修改资产。
正式写入进入 write gate。
```

### Commit 8：Contract and integration tests

```text
Add PatchBlueprintGraph contract tests
Add PatchBlueprintGraph write tests
Add PatchBlueprintGraph rollback tests
```

验收：

```text
字段契约测试通过。
所有支持 patch_type 的写入 / rollback 测试通过。
```

---

## 18. 自动化测试清单

建议新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperPatchGraphContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperPatchGraphResolveTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperPatchGraphWriteTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperPatchGraphRollbackTests.cpp
```

### 18.1 Contract 测试

```text
1. success_result_minimal_contract
   - data.schema = PatchBlueprintGraph.v1
   - data.patch_result.patched_ref 存在
   - data.patch_result.patch.patch_type 存在
   - expected_old_state_provided 存在
   - changed 存在
   - data.write_ref.transaction_id 存在
   - 不包含 before / after / old_value / new_value / ownership / review / safety / summary

2. dry_run_passed_minimal_contract
   - status=dry_run
   - modified=false
   - data.schema = PatchBlueprintGraphDryRun.v1
   - result=passed
   - can_execute=true
   - 不包含 patch_plan / would_xxx / before / after

3. dry_run_blocked_contract
   - ok=true
   - status=dry_run
   - result=blocked
   - blocked_by / conflicts / errors 存在

4. failed_contract
   - status=failed
   - error.code / stage / message / retryable / rollback_result 存在
   - 不包含 patch_result / write_ref
```

### 18.2 Resolve 测试

```text
1. resolve_node_ref_with_group_context
2. resolve_pin_ref_with_group_context
3. resolve_link_ref_with_group_context
4. reject_cross_group_node_ref
5. reject_display_name_only_target
6. fallback_node_path_when_ref_insufficient
7. fallback_pin_path_when_ref_insufficient
8. fallback_link_path_when_ref_insufficient
```

### 18.3 Write 测试

```text
1. set_pin_default_changes_value
2. set_pin_default_noop_changed_false
3. set_node_comment_preserves_owned_header
4. set_node_position_changes_position
5. connect_pins_creates_link
6. disconnect_link_removes_exact_link_only
7. replace_link_breaks_old_and_creates_new
8. connect_pins_rejects_type_mismatch
9. connect_pins_does_not_reorder_exec_flow
10. expected_old_state_mismatch_blocks_write
```

### 18.4 Rollback 测试

```text
1. rollback_on_pin_default_apply_failed
2. rollback_on_connect_pins_failed
3. rollback_on_replace_link_failed_after_break
4. rollback_on_journal_write_failed
5. rollback_blocked_sets_modified_true
6. rollback_failed_sets_modified_true
```

---

## 19. 第一版不做的内容

为了保持 Patch 工具边界稳定，第一版明确不做：

```text
1. 不根据自然语言模糊查找节点。
2. 不仅凭节点显示名或 Pin 名执行修改。
3. 不跨 LogicJson group 使用 node_ref / pin_ref / link_ref。
4. 不做执行流重排；执行流接入和重排属于 MergeBlueprintGraph。
5. 不替换完整函数体 / 事件体；完整实现替换属于 ReplaceBlueprintGraph。
6. 不创建新逻辑块；新逻辑块属于 AppendBlueprintGraph。
7. 不返回 before / after / old_value / new_value。
8. 不返回 ownership / review / safety / diagnostics。
9. 不默认支持任意 UObject 属性路径写入。
10. 不默认支持 local variable ref rename；先作为后置能力或 unsupported_patch_type。
```

---

## 20. 最小验收标准

完成后，以下成功返回必须成立：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "patch_blueprint_graph",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "patch_scope": "pin_default"
  },
  "data": {
    "schema": "PatchBlueprintGraph.v1",
    "patch_result": {
      "patched_ref": {
        "graph_id": "EG_PhysicsDoor",
        "node_ref": "Branch0",
        "pin_ref": "Condition"
      },
      "patch": {
        "patch_type": "set_pin_default",
        "expected_old_state_provided": true,
        "changed": true
      }
    },
    "write_ref": {
      "transaction_id": "tx_xxx",
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

同时必须不出现：

```text
target_type
target_kind
summary
before
after
old_value
new_value
patch_plan
ownership
review
safety
diagnostics
next
journal_path
rollback_data
```

这是本轮 PatchBlueprintGraph UE 侧 C++ 实现是否合格的核心判定。

