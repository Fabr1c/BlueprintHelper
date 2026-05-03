# BlueprintHelper MergeBlueprintGraph UE 侧 C++ 可执行实现计划

日期：2026-05-03  
输入文档：`BlueprintHelper_MergeBlueprintGraph_UE_FieldMapping_20260503.md`  
适用范围：UE 插件侧 / C++ 实现  
目标版本：BlueprintHelper v0.4 / v0.5 前置协议收敛  

---

## 0. 计划目标

本计划把 `MergeBlueprintGraph` 字段映射文档转成 UE 侧可直接执行的 C++ 实现任务。

`MergeBlueprintGraph` 的职责不是新增独立逻辑、不是替换完整实现，也不是改 Pin 默认值，而是：

```text
把新逻辑或已有逻辑引用接入一个明确的已有执行流。
```

与其他 Graph Write 工具的边界：

```text
AppendBlueprintGraph：追加新的独立 owned 逻辑块，不接入已有执行链。
ReplaceBlueprintGraph：替换明确目标的完整实现。
PatchBlueprintGraph：精确修改节点 / Pin / Link / 默认值。
MergeBlueprintGraph：改变已有执行流连接关系，把逻辑插入已有 Exec 链。
```

本计划只覆盖 UE 插件侧 C++，不覆盖 MCP Server TypeScript tool schema、Agent Skill 文档或用户引导文档。

---

## 1. 当前代码可复用基线

基于 `UE侧v0.3.4` 现有结构和前几份 Graph Write 实现计划，Merge 应复用以下能力：

| 现有 / 已规划模块 | 当前能力 | Merge 中的用途 |
|---|---|---|
| `FBlueprintHelperGraphResolver` | 解析 Blueprint / Graph / FunctionGraph / EventGraph | 定位接入执行流所在图表。 |
| `FBlueprintHelperScopedAssetMutation` | 基于 `FScopedTransaction` 的写入事务与 rollback | Merge 写入中失败时整体回滚。 |
| `FBlueprintHelperToolResultBase` | 统一 ToolResultBase | 输出 Merge 极简 Agent-facing 结果。 |
| `FBlueprintHelperWriteRef` | Graph Write 成功返回 transaction handle | 复用 `data.write_ref`。 |
| `FBlueprintHelperTransactionJournalService` | 写 Transaction Journal / Review Store | 记录断开的旧连接、新增连接、执行顺序变化、rollback_data。 |
| `FBlueprintHelperOwnershipService` | 读取 BlueprintHelper metadata | 解析 `owned_block_call` 的目标 block。 |
| `FBlueprintHelperLogicJsonPathService`（需新增或复用 Patch 计划） | LogicJson group ref/path 反推 | 把 anchor 的 `node_ref / pin_ref / node_path / pin_path` 转换为 UE 节点与 Pin。 |
| `UEdGraphSchema_K2` | Pin 连接合法性检查与创建连接 | 所有 `append_after / insert_between / branch_fork` 连接操作的核心 API。 |
| `UK2Node_Sequence` | Sequence 节点 | `branch_fork` 插入分支分发节点。 |
| `UK2Node_CallFunction` / K2 call node 生成路径 | 函数调用节点 | `function_call` 与部分 custom event / owned block 调用。 |

不建议直接复用 `AgentImportGraph` 的返回结构，因为 Merge 字段文档要求成功结果极简，不返回 `merge_plan / disconnected_links / created_links / execution_order_changed / affected_user_nodes / diagnostics / ownership / review / safety`。

---

## 2. 字段契约硬约束

实现时必须满足以下字段契约。

### 2.1 operation

```json
"operation": "merge_blueprint_graph"
```

### 2.2 成功 data.schema

```json
"schema": "MergeBlueprintGraph.v1"
```

### 2.3 dry_run data.schema

```json
"schema": "MergeBlueprintGraphDryRun.v1"
```

### 2.4 顶层 target 只允许

```text
target.asset_path
target.graph
target.merge_scope
target.insert_strategy
```

禁止返回：

```text
target.target_type
target.target_kind
```

当前 `FBlueprintHelperTargetRef::ToJson()` 会固定输出 `target_type`，因此 Merge 不能直接使用旧 `TargetRef` 序列化，必须新增 Graph Write 专用 target serializer，或让 `FBlueprintHelperTargetRef` 支持 `bEmitTargetType=false`。

### 2.5 成功返回只允许

```text
ok
schema
operation
trace_id
status
modified
target.asset_path
target.graph
target.merge_scope
target.insert_strategy
data.schema
data.merge_result.merged_ref.graph_id
data.merge_result.merged_ref.anchor_ref
data.merge_result.merged_ref.inserted_ref
data.merge_result.merged_ref.sequence_ref    // branch_fork 时
data.write_ref.transaction_id
data.write_ref.journal_recorded
validation.should_compile
validation.should_save
validation.compiled
validation.saved
```

### 2.6 成功返回禁止

```text
target_type
target_kind
merge_plan
disconnected_links
created_links
execution_order_changed
affected_user_nodes
before / after
full_diff
ownership
review
safety
diagnostics
next
journal_path
rollback_data
old_successor
new_successor
summary
```

这些信息必须进入 Transaction Journal / Review / verbose / debug，而不是默认 Agent-facing 成功返回。

### 2.7 dry_run 返回

`dry_run passed` 只返回：

```json
{
  "data": {
    "schema": "MergeBlueprintGraphDryRun.v1",
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
    "schema": "MergeBlueprintGraphDryRun.v1",
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
merge_plan
would_disconnect_links
would_create_links
would_insert_sequence
execution_order_preview
affected_user_nodes
before / after
ownership
review
safety
diagnostics
next
```

### 2.8 正式失败返回

正式失败不返回：

```text
data.merge_result
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

## 3. Phase A：新增 Merge 专属类型文件

新增文件：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperMergeGraphTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperMergeGraphTypes.cpp
```

### 3.1 枚举

```cpp
enum class EBlueprintHelperMergeScope : uint8
{
    OwnedBlockCall,
    CustomEventCall,
    FunctionCall,
    InlineNodes,
    EventEntryLogic
};

enum class EBlueprintHelperInsertStrategy : uint8
{
    AppendAfter,
    InsertBetween,
    BranchFork
};

enum class EBlueprintHelperMergeStage : uint8
{
    ParseInput,
    Auth,
    ResolveTarget,
    ResolveAnchor,
    ResolveInsertedLogic,
    Preflight,
    DryRun,
    CreateInsertedNode,
    DisconnectOriginalSuccessor,
    CreateSequence,
    CreateLinks,
    WriteJournal,
    Rollback
};

enum class EBlueprintHelperMergeErrorCode : uint8
{
    TargetBlueprintNotFound,
    TargetGraphNotFound,
    TargetGraphTypeInvalid,
    AnchorNodeNotFound,
    AnchorPinNotFound,
    AnchorPinNotExec,
    AnchorExecPinAlreadyConnected,
    AnchorExecPinHasMultipleSuccessors,
    OriginalSuccessorNotFound,
    InsertedLogicNotFound,
    InsertedLogicNotCallable,
    InsertedLogicHasNoExecPins,
    InsertedLogicSignatureMismatch,
    SequenceOrderRequired,
    SequenceOrderInvalid,
    UnsupportedMergeScope,
    UnsupportedInsertStrategy,
    PinTypeMismatch,
    LinkCreateFailed,
    LinkDisconnectFailed,
    SchemaRejected,
    JournalWriteFailed,
    RollbackBlocked,
    RollbackFailed,
    WritePermissionDisabled,
    ProfilePolicyViolation,
    BridgeDisconnected
};
```

`inline_nodes` 和 `event_entry_logic` 可先进入枚举，但第一版服务层返回 `unsupported_merge_scope`，避免 schema 与实现边界不一致。

### 3.2 结果结构

```cpp
struct FBlueprintHelperMergedRef
{
    FString GraphId;
    FString AnchorRef;
    FString InsertedRef;
    FString SequenceRef; // branch_fork only

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperMergeGraphResult
{
    FBlueprintHelperMergedRef MergedRef;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperMergeGraphResultData
{
    FString Schema = TEXT("MergeBlueprintGraph.v1");
    FBlueprintHelperMergeGraphResult MergeResult;
    FBlueprintHelperWriteRef WriteRef;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperMergeDryRunResult
{
    FString Result; // passed | blocked
    bool bCanExecute = true;
    TArray<FString> BlockedBy;
    TArray<FBlueprintHelperDryRunIssue> Conflicts;
    TArray<FBlueprintHelperDryRunIssue> Errors;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperMergeDryRunData
{
    FString Schema = TEXT("MergeBlueprintGraphDryRun.v1");
    FBlueprintHelperMergeDryRunResult DryRun;

    TSharedRef<FJsonObject> ToJson() const;
};
```

`FBlueprintHelperWriteRef`、`FBlueprintHelperDryRunIssue`、`FBlueprintHelperFailedItem`、`FBlueprintHelperConflictItem` 应复用 Append / Replace / Patch 已规划的公共 Graph Write 类型，避免每个工具各定义一套。

---

## 4. Phase B：新增 Graph Write 专用 target serializer

Merge 成功返回禁止 `target_type`，但当前 `FBlueprintHelperTargetRef::ToJson()` 会固定输出 `target_type`。需要新增一个不输出 `target_type` 的目标结构。

新增公共结构：

```cpp
struct FBlueprintHelperMergeTargetRef
{
    FString AssetPath;
    FString Graph;
    EBlueprintHelperMergeScope MergeScope;
    EBlueprintHelperInsertStrategy InsertStrategy;

    TSharedRef<FJsonObject> ToJson() const
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("asset_path"), AssetPath);
        Json->SetStringField(TEXT("graph"), Graph);
        Json->SetStringField(TEXT("merge_scope"), MergeScopeToString(MergeScope));
        Json->SetStringField(TEXT("insert_strategy"), InsertStrategyToString(InsertStrategy));
        return Json;
    }
};
```

实现方式有两种：

```text
方案 A：扩展 FBlueprintHelperToolResultBase，允许设置 CustomTargetJson。
方案 B：MergeService 不使用 TargetRef，而是在 ToolResultBase::ToJson 后替换 target 字段。
```

推荐方案 A：

```cpp
struct FBlueprintHelperToolResultBase
{
    TSharedPtr<FJsonObject> CustomTargetJson;

    TSharedRef<FJsonObject> ToJson() const
    {
        ...
        if (CustomTargetJson.IsValid())
        {
            Json->SetObjectField(TEXT("target"), CustomTargetJson);
        }
        else if (Target.IsSet())
        {
            Json->SetObjectField(TEXT("target"), Target->ToJson());
        }
        ...
    }
};
```

同一改造也适用于 Replace / Patch，因为这几份字段文档都要求 Graph Write 顶层 target 不默认返回 `target_type`。

---

## 5. Phase C：新增 MergeBlueprintGraphService

新增文件：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperMergeBlueprintGraphService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperMergeBlueprintGraphService.cpp
```

服务接口：

```cpp
class FBlueprintHelperMergeBlueprintGraphService
{
public:
    FBlueprintHelperMergeBlueprintGraphService(
        const FBlueprintHelperGraphResolver& InGraphResolver,
        const FBlueprintHelperOwnershipService& InOwnershipService,
        const FBlueprintHelperLogicJsonPathService& InPathService,
        const FBlueprintHelperTransactionJournalService& InJournalService);

    FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool ParseRequest(const TSharedPtr<FJsonObject>& Payload, FBlueprintHelperMergeRequest& OutRequest, FBlueprintHelperToolError& OutError) const;
    bool ResolveTarget(const FBlueprintHelperMergeRequest& Request, FBlueprintHelperMergeContext& OutContext, FBlueprintHelperToolError& OutError) const;
    bool ResolveAnchor(const FBlueprintHelperMergeRequest& Request, FBlueprintHelperMergeContext& Context, FBlueprintHelperToolError& OutError) const;
    bool ResolveInsertedLogic(const FBlueprintHelperMergeRequest& Request, FBlueprintHelperMergeContext& Context, FBlueprintHelperToolError& OutError) const;
    FBlueprintHelperMergePreflightResult Preflight(const FBlueprintHelperMergeRequest& Request, const FBlueprintHelperMergeContext& Context) const;

    FBlueprintHelperToolResultBase ExecuteDryRun(const FBlueprintHelperMergeRequest& Request, const FBlueprintHelperMergePreflightResult& Preflight) const;
    FBlueprintHelperToolResultBase ExecuteWrite(const FBlueprintHelperMergeRequest& Request, FBlueprintHelperMergeContext& Context) const;
};
```

内部请求结构：

```cpp
struct FBlueprintHelperMergeRequest
{
    FString AssetPath;
    FString GraphName;
    EBlueprintHelperMergeScope MergeScope;
    EBlueprintHelperInsertStrategy InsertStrategy;

    // Anchor from LogicJson path/ref.
    FString AnchorNodeRef;
    FString AnchorPinRef;
    FString AnchorNodePath;
    FString AnchorPinPath;

    // Required for reconstructing local ref from group context.
    FString GroupEntryNodePath;

    // Inserted logic reference.
    FString InsertedBlockId;
    FString InsertedBlockRef;
    FString InsertedFunctionName;
    FString InsertedCustomEventName;

    // Branch fork only.
    TArray<FString> SequenceOrder;

    bool bDryRun = false;
};
```

第一版不支持 `inline_nodes`，所以不在 request 中加入新节点数组，避免 Merge 与 Append 职责混合。

---

## 6. Phase D：Anchor 定位实现

Merge 必须明确 anchor，否则失败。

### 6.1 输入允许形式

允许以下形式之一：

```text
1. anchor_node_path + anchor_pin_path
2. group_entry_node_path + anchor_node_ref + anchor_pin_ref
3. UE node_guid + pin_guid（后置兼容，第一版可选）
```

不允许：

```text
1. 仅节点显示名。
2. 仅 Pin 显示名。
3. 跨 LogicJson group 使用 node_ref / pin_ref。
4. target graph 内自动搜索 BeginPlay / Tick 并修改。
```

### 6.2 LogicJson ref 反推 path

如果输入是局部 ref：

```text
group_entry_node_path = logic.groups[i].entry.node_path
anchor_node_ref = "BeginPlay0"
anchor_pin_ref = "Then"
```

路径服务负责反推：

```cpp
FString NodePath = PathService.MakeNodePathFromGroupEntry(GroupEntryNodePath, AnchorNodeRef);
FString PinPath = PathService.MakePinPathFromNodePath(NodePath, AnchorPinRef);
```

再解析到 UE 对象：

```cpp
UEdGraphNode* AnchorNode = PathService.ResolveNode(Graph, NodePath);
UEdGraphPin* AnchorPin = PathService.ResolvePin(AnchorNode, PinPath);
```

### 6.3 Anchor Pin 校验

必须校验：

```cpp
if (!AnchorPin)
{
    return Error(anchor_pin_not_found, resolve_anchor, not_needed);
}

if (AnchorPin->Direction != EGPD_Output)
{
    return Error(anchor_pin_not_exec, resolve_anchor, not_needed);
}

if (AnchorPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
{
    return Error(anchor_pin_not_exec, resolve_anchor, not_needed);
}
```

第一版要求 anchor 是 Exec 输出 Pin。

### 6.4 原后继解析

```cpp
TArray<UEdGraphPin*> Successors;
for (UEdGraphPin* LinkedPin : AnchorPin->LinkedTo)
{
    if (LinkedPin && LinkedPin->Direction == EGPD_Input)
    {
        Successors.Add(LinkedPin);
    }
}
```

不同策略的规则：

| insert_strategy | Anchor 后继要求 |
|---|---|
| `append_after` | 必须没有后继。已有后继则 blocked / failed：`anchor_exec_pin_already_connected`。 |
| `insert_between` | 必须有且仅有一个后继。0 个后继可以建议改用 append_after，但工具不自动改策略。多个后继返回 `anchor_exec_pin_has_multiple_successors`。 |
| `branch_fork` | 允许 0 或 1 个后继。多个后继第一版返回 `anchor_exec_pin_has_multiple_successors`，因为 K2 Exec 正常应单连。 |

---

## 7. Phase E：Inserted logic 解析

Merge 第一版优先支持：

```text
owned_block_call
custom_event_call
function_call
```

### 7.1 owned_block_call

目标：把已有 BlueprintHelper-owned block 作为调用接入执行流。

输入建议：

```json
{
  "merge_scope": "owned_block_call",
  "inserted": {
    "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0"
  }
}
```

解析步骤：

```text
1. 通过 OwnershipService / Journal 查找 block_id。
2. 确认 block 属于同一 Blueprint 或当前 Merge 支持的可调用范围。
3. 找到 block entry。
4. 第一版仅支持 Custom Event entry 或 Function entry。
5. 如果 block entry 不可调用，返回 inserted_logic_not_callable。
```

如果 block entry 是 Custom Event：

```text
生成 Custom Event 调用节点。
inserted_ref 返回完整 block_id 或文档约定的 block 引用，例如 EG_PhysicsDoor_TogglePhysicsDoor0。
```

如果 block entry 是 Function：

```text
生成 Function Call 节点。
inserted_ref 返回 function 名或 block_id，第一版建议保持 block_id，便于后续追踪。
```

### 7.2 custom_event_call

输入：

```json
{
  "merge_scope": "custom_event_call",
  "inserted": {
    "custom_event": "TogglePhysicsDoor"
  }
}
```

解析：

```text
1. 在目标 Blueprint 中查找唯一 Custom Event。
2. 确认事件可调用。
3. 生成调用节点。
4. inserted_ref = CustomEventName。
```

多个同名或无法唯一定位时返回：

```text
inserted_logic_not_found
inserted_logic_not_callable
```

### 7.3 function_call

输入：

```json
{
  "merge_scope": "function_call",
  "inserted": {
    "function": "InitializePhysicsDoor"
  }
}
```

解析：

```text
1. 在 Blueprint skeleton/generated class 或函数图中确认函数存在。
2. 检查函数有 Exec 输入/输出，或至少可生成符合策略的执行节点。
3. 生成 UK2Node_CallFunction。
4. inserted_ref = FunctionName。
```

失败：

```text
inserted_logic_not_found
inserted_logic_not_callable
inserted_logic_has_no_exec_pins
inserted_logic_signature_mismatch
```

### 7.4 生成 call node 的 API 落点

可优先复用现有 `CallFunctionNodeHandler` 的节点生成逻辑。若单独实现，基本落点为：

```cpp
UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
CallNode->SetFromFunction(TargetFunction);
Graph->AddNode(CallNode, true, false);
CallNode->CreateNewGuid();
CallNode->PostPlacedNewNode();
CallNode->AllocateDefaultPins();
```

Custom Event 调用如果现有 handler 已支持，应复用 handler，避免手写不完整的 K2 节点构建路径。

---

## 8. Phase F：强制 dry_run 与 preflight

Merge 默认必须 dry_run。

服务层规则：

```text
1. 如果 Request.bDryRun=true：只执行 preflight，返回 dry_run 结果。
2. 如果 Request.bDryRun=false：仍先执行同一套 preflight。
3. Conservative / Safety Profile 要求前置 dry_run 时，若没有已通过的 dry_run token 或 workflow 标记，返回 profile_policy_violation 或 require_dry_run。
```

当前字段文档只规定 dry_run 返回内容，不规定 dry_run token。UE 第一版可以采用：

```text
MCP / Agent 调用正式写入前必须先调用 dry_run。
UE 服务正式写入仍重复 preflight，防止 TOCTOU。
```

### 8.1 dry_run passed

只返回：

```json
{
  "result": "passed",
  "can_execute": true
}
```

### 8.2 dry_run blocked 典型 code

```text
anchor_pin_not_found
anchor_pin_not_exec
anchor_exec_pin_already_connected
anchor_exec_pin_has_multiple_successors
sequence_order_required
sequence_order_invalid
inserted_logic_not_found
inserted_logic_not_callable
inserted_logic_has_no_exec_pins
unsupported_merge_scope
unsupported_insert_strategy
pin_type_mismatch
profile_policy_violation
```

### 8.3 dry_run 不返回 plan

即使内部计算了：

```text
old successor
new links
sequence node
execution order preview
affected user nodes
```

也不进入 Agent-facing dry_run 返回，只写入 internal diagnostics / verbose / debug，或在正式执行后写 Journal。

---

## 9. Phase G：append_after 实现

### 9.1 语义

```text
把 inserted logic 接到 anchor Exec Pin 后方。
```

前提：

```text
AnchorPin 当前没有后继。
```

### 9.2 preflight

```cpp
if (AnchorPin->LinkedTo.Num() > 0)
{
    BlockedBy.Add(TEXT("anchor_exec_pin_already_connected"));
    return Blocked;
}
```

不得自动改成 `insert_between` 或 `branch_fork`。

### 9.3 写入步骤

```text
1. 创建 inserted call node。
2. 找到 inserted call node 的 Exec 输入 Pin。
3. 使用 Schema->CanCreateConnection(anchor_pin, inserted_exec_in) 检查。
4. 使用 Schema->TryCreateConnection(anchor_pin, inserted_exec_in) 连接。
5. inserted call node 的 Exec 输出 Pin 不连接任何后继。
6. 写 Journal。
```

### 9.4 成功返回

```json
"merged_ref": {
  "graph_id": "EventGraph",
  "anchor_ref": "BeginPlay0.Then",
  "inserted_ref": "EG_PhysicsDoor_TogglePhysicsDoor0"
}
```

不返回 created_links。

---

## 10. Phase H：insert_between 实现

### 10.1 语义

```text
断开 anchor Exec Pin 的原后继。
插入 inserted logic。
inserted logic 执行完后重接原后继。
```

### 10.2 preflight

```cpp
if (Successors.Num() == 0)
{
    return BlockedOrError(TEXT("original_successor_not_found"));
}

if (Successors.Num() > 1)
{
    return BlockedOrError(TEXT("anchor_exec_pin_has_multiple_successors"));
}

UEdGraphPin* OriginalSuccessorPin = Successors[0];
```

还要检查：

```text
anchor → inserted_exec_in 可连接
inserted_exec_out → original_successor 可连接
```

### 10.3 写入步骤

```text
1. Snapshot 原连接：anchor_pin -> original_successor_pin。
2. 创建 inserted call node。
3. 找到 inserted_exec_in / inserted_exec_out。
4. Mutation.Modify(anchor_pin owner / original successor owner / graph)。
5. BreakLinkTo(original_successor_pin)。
6. TryCreateConnection(anchor_pin, inserted_exec_in)。
7. TryCreateConnection(inserted_exec_out, original_successor_pin)。
8. 任一步失败：rollback。
9. 写 Journal。
```

推荐 C++ 片段：

```cpp
const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

AnchorPin->BreakLinkTo(OriginalSuccessorPin);

if (!Schema->TryCreateConnection(AnchorPin, InsertedExecIn))
{
    return FailAndRollback(TEXT("link_create_failed"), TEXT("create_links"));
}

if (!Schema->TryCreateConnection(InsertedExecOut, OriginalSuccessorPin))
{
    return FailAndRollback(TEXT("link_create_failed"), TEXT("create_links"));
}
```

### 10.4 Journal 必须记录但成功不返回

Journal 记录：

```text
disconnected_links: anchor -> original_successor
created_links: anchor -> inserted, inserted -> original_successor
execution_order_changed: true
affected_user_nodes: anchor owner / original successor owner
rollback_data: original link + inserted node + inserted links
```

成功结果不返回这些字段。

---

## 11. Phase I：branch_fork 实现

### 11.1 语义

```text
插入 Sequence 节点或等价分发节点，把原后继和 inserted logic 分到不同分支。
```

必须显式传入：

```json
"sequence_order": ["original_successor", "inserted_logic"]
```

或：

```json
"sequence_order": ["inserted_logic", "original_successor"]
```

不得由工具默认决定执行顺序。

### 11.2 preflight

```cpp
if (InsertStrategy == BranchFork && SequenceOrder.Num() == 0)
{
    return Blocked(TEXT("sequence_order_required"));
}

if (!IsValidSequenceOrder(SequenceOrder))
{
    return Blocked(TEXT("sequence_order_invalid"));
}
```

有效值第一版只允许：

```text
original_successor
inserted_logic
```

如果 anchor 没有原后继，`sequence_order` 中不能包含 `original_successor`。

### 11.3 创建 Sequence 节点

```cpp
UK2Node_Sequence* SequenceNode = NewObject<UK2Node_Sequence>(Graph);
Graph->AddNode(SequenceNode, true, false);
SequenceNode->CreateNewGuid();
SequenceNode->PostPlacedNewNode();
SequenceNode->AllocateDefaultPins();
```

若需要两个输出分支，确保有足够 Then pin。UE 的 `UK2Node_Sequence` 通常可通过添加 input/output pin 支持多个 then。实现中建议封装：

```cpp
UEdGraphPin* EnsureSequenceThenPin(UK2Node_Sequence* SequenceNode, int32 Index);
```

若第一版只支持两个 then，可强制创建或查找：

```text
Then_0
Then_1
```

具体 Pin 名需以 UE5.3 实际 `UK2Node_Sequence` 生成结果为准，不能硬编码英文显示名，必须通过 PinType / Direction / Schema 或 handler 辅助定位。

### 11.4 写入步骤：存在原后继

```text
1. Snapshot 原连接 anchor -> original_successor。
2. 创建 Sequence 节点。
3. 创建 inserted call node。
4. 断开 anchor -> original_successor。
5. 连接 anchor -> sequence.exec_in。
6. 按 sequence_order：
   - original_successor 分支：sequence.ThenX -> original_successor
   - inserted_logic 分支：sequence.ThenY -> inserted_exec_in
7. inserted_logic 的 Exec 输出默认不接回 original_successor。
8. 写 Journal。
```

注意：`branch_fork` 不是 `insert_between`，不把 inserted logic 执行完后重接原后继。它是并列分发。

### 11.5 写入步骤：没有原后继

如果 anchor 没有原后继：

```text
1. branch_fork 仍可插入 Sequence，但 sequence_order 只能包含 inserted_logic。
2. 也可以返回 blocked，建议 Agent 改用 append_after。
```

第一版推荐更严格：

```text
没有 original_successor 时，branch_fork 返回 sequence_order_invalid，建议使用 append_after。
```

这样可以避免无意义地插入 Sequence 节点。

### 11.6 成功返回

```json
"merged_ref": {
  "graph_id": "EventGraph",
  "anchor_ref": "BeginPlay0.Then",
  "inserted_ref": "EG_PhysicsDoor_TogglePhysicsDoor0",
  "sequence_ref": "Sequence0"
}
```

不返回分支顺序、created_links、affected_user_nodes；这些进入 Journal。

---

## 12. Phase J：Transaction Journal / rollback_data

Merge 是高风险写工具，正式执行必须生成 transaction_id 并写 Journal。

Journal 内部记录：

```json
{
  "schema": "BlueprintHelper.TransactionJournal.v1",
  "transaction_id": "tx_xxx",
  "tool": "MergeBlueprintGraph",
  "status": "applied",
  "target_assets": ["/Game/.../BP"],
  "operations": [
    {
      "type": "merge_execution_flow",
      "graph": "EventGraph",
      "merge_scope": "owned_block_call",
      "insert_strategy": "insert_between",
      "anchor_ref": "BeginPlay0.Then",
      "inserted_ref": "EG_PhysicsDoor_TogglePhysicsDoor0",
      "sequence_ref": "Sequence0",
      "disconnected_links": [],
      "created_links": [],
      "execution_order_changed": true,
      "affected_user_nodes": [],
      "rollback_data": {}
    }
  ],
  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

Agent-facing 成功只返回：

```text
merged_ref
write_ref
validation
```

Journal 写入失败：

```text
error.code = journal_write_failed
error.stage = write_journal
rollback_result = rolled_back / blocked / failed
不返回 data.write_ref
不报告成功
```

---

## 13. Phase K：rollback 策略

### 13.1 rollback 数据最小内容

`append_after`：

```text
created inserted call node
created link anchor -> inserted
previous anchor links empty
```

`insert_between`：

```text
original link anchor -> original_successor
created inserted call node
created links anchor -> inserted, inserted -> original_successor
```

`branch_fork`：

```text
original link anchor -> original_successor
created sequence node
created inserted call node
created links anchor -> sequence, sequence.ThenX -> original_successor, sequence.ThenY -> inserted
sequence_order
```

### 13.2 rollback 执行原则

```text
1. 删除本次创建的 links。
2. 删除本次创建的 inserted call node。
3. 删除本次创建的 sequence node。
4. 恢复原 anchor -> original_successor link。
5. 恢复 Graph dirty 状态。
```

### 13.3 rollback blocked 判定

如果 rollback 前发现：

```text
1. anchor pin 已被其他操作改连。
2. original successor pin 不存在。
3. inserted node 被用户或其他 transaction 修改。
4. sequence node 被改动。
5. graph 状态与 rollback_data 不一致。
```

返回：

```text
error.code = rollback_blocked 或 rollback_failed
error.stage = rollback
modified = true
rollback_result = blocked / failed
```

Agent 必须 stop_and_report，不得继续 compile/save/patch/merge/replace。

---

## 14. Phase L：正式成功返回构造

### 14.1 insert_between 成功

```cpp
FBlueprintHelperMergeGraphResultData Data;
Data.Schema = TEXT("MergeBlueprintGraph.v1");
Data.MergeResult.MergedRef.GraphId = Request.GraphName;
Data.MergeResult.MergedRef.AnchorRef = MakeAnchorRef(Context.AnchorNode, Context.AnchorPin);
Data.MergeResult.MergedRef.InsertedRef = Context.InsertedRef;
Data.WriteRef.TransactionId = TransactionId;
Data.WriteRef.bJournalRecorded = true;
```

返回 JSON：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "merge_blueprint_graph",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/.../BP",
    "graph": "EventGraph",
    "merge_scope": "owned_block_call",
    "insert_strategy": "insert_between"
  },
  "data": {
    "schema": "MergeBlueprintGraph.v1",
    "merge_result": {
      "merged_ref": {
        "graph_id": "EventGraph",
        "anchor_ref": "BeginPlay0.Then",
        "inserted_ref": "EG_PhysicsDoor_TogglePhysicsDoor0"
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

### 14.2 branch_fork 成功

只额外返回：

```json
"sequence_ref": "Sequence0"
```

---

## 15. Phase M：正式失败返回构造

### 15.1 anchor 解析失败

```cpp
return MakeFailedResult(
    Request,
    TEXT("anchor_pin_not_found"),
    TEXT("resolve_anchor"),
    TEXT("The requested anchor pin was not found."),
    false,
    TEXT("not_needed"));
```

### 15.2 append_after 后继冲突

正式写入前重复 preflight。如果 anchor 已有后继：

```text
error.code = anchor_exec_pin_already_connected
error.stage = preflight
rollback_result = not_needed
modified = false
```

### 15.3 连接失败并回滚成功

```text
error.code = link_create_failed
error.stage = create_links
rollback_result = rolled_back
modified = false
failed_item.type = link
```

### 15.4 rollback failed

```text
error.code = rollback_failed
error.stage = rollback
rollback_result = failed
modified = true
conflicts[] = asset_state_changed_during_write
```

失败不返回：

```text
merge_result
write_ref
```

---

## 16. Phase N：Bridge Router 接入

### 16.1 Router 新增命令

修改：

```text
Source/BlueprintHelper/Private/Bridge/BlueprintHelperBridgeRouter.cpp
Source/BlueprintHelper/Public/Bridge/BlueprintHelperBridgeRouter.h
```

新增：

```cpp
if (Request.Command == TEXT("merge_blueprint_graph"))
{
    return HandleMergeBlueprintGraph(Request);
}
```

处理函数：

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleMergeBlueprintGraph(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result = MergeGraphService.Execute(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("merge_blueprint_graph failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

### 16.2 RequestValidator 新增校验

修改：

```text
Source/BlueprintHelper/Private/Bridge/BlueprintHelperRequestValidator.cpp
```

新增规则：

```cpp
if (CommandEquals(Command, TEXT("merge_blueprint_graph")))
{
    RequireObject(Payload, TEXT("target"));
    RequireString(Target, TEXT("asset_path"));
    RequireString(Target, TEXT("graph"));
    RequireString(Target, TEXT("merge_scope"));
    RequireString(Target, TEXT("insert_strategy"));
    RequireObject(Payload, TEXT("anchor"));
    RequireObject(Payload, TEXT("inserted"));

    if (InsertStrategy == TEXT("branch_fork"))
    {
        RequireArray(Payload, TEXT("sequence_order"));
    }
}
```

### 16.3 写命令权限

把 `merge_blueprint_graph` 加入写命令集合，使 Token / write_permission gate 生效：

```cpp
TEXT("merge_blueprint_graph")
```

---

## 17. Phase O：Build.cs 与 UE API 依赖

当前 `BlueprintHelper.Build.cs` 已包含：

```text
BlueprintGraph
UnrealEd
GraphEditor
Kismet
CoreUObject
Engine
Json
JsonUtilities
```

Merge 需要的主要头：

```cpp
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Sequence.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
```

通常不需要新增模块依赖。若 `UK2Node_Sequence` 或其他 K2 节点头出现链接 / 编译问题，优先确认 `BlueprintGraph` 在 PublicDependencyModuleNames 中。

---

## 18. 推荐提交顺序

### Commit 1：Merge 类型与字段序列化

```text
Add MergeBlueprintGraph C++ result and dry_run types
Add merge_scope / insert_strategy / merge stage / merge error enums
Add merged_ref JSON builders
Add Graph Write custom target serializer without target_type
```

验收：

```text
可以构造 MergeBlueprintGraph.v1 / MergeBlueprintGraphDryRun.v1 JSON。
成功 JSON 不出现 target_type / merge_plan / disconnected_links / created_links / ownership / review / safety。
```

### Commit 2：MergeService skeleton 与 payload parse

```text
Add FBlueprintHelperMergeBlueprintGraphService
Parse target.asset_path / graph / merge_scope / insert_strategy
Parse anchor
Parse inserted
Parse sequence_order
Reject unsupported inline_nodes / event_entry_logic in v1
```

验收：

```text
unsupported merge_scope 返回 unsupported_merge_scope。
branch_fork 缺 sequence_order 返回 sequence_order_required。
```

### Commit 3：Anchor resolver

```text
Resolve anchor from node_path / pin_path
Resolve anchor from group_entry_node_path + node_ref + pin_ref
Validate Exec output pin
Detect successor count
```

验收：

```text
anchor_pin_not_found
anchor_pin_not_exec
anchor_exec_pin_already_connected
anchor_exec_pin_has_multiple_successors
```

### Commit 4：Inserted logic resolver

```text
Resolve owned_block_call through ownership metadata / journal
Resolve custom_event_call
Resolve function_call
Create inserted call node using existing handlers where possible
```

验收：

```text
inserted_logic_not_found
inserted_logic_not_callable
inserted_logic_has_no_exec_pins
```

### Commit 5：dry_run flow

```text
Implement mandatory Merge dry_run
Return passed minimal result
Return blocked minimal result
Do not expose merge_plan or affected user nodes
```

验收：

```text
dry_run passed only result/can_execute。
dry_run blocked only blocked_by/conflicts/errors。
```

### Commit 6：append_after write

```text
Implement append_after
Create inserted call node
Connect anchor -> inserted
Rollback on link failure
```

验收：

```text
Anchor with no successor succeeds。
Anchor with successor blocks/fails with anchor_exec_pin_already_connected。
```

### Commit 7：insert_between write

```text
Implement insert_between
Disconnect original successor
Connect anchor -> inserted
Connect inserted -> original successor
Record rollback_data
```

验收：

```text
原连接恢复正确。
失败时不残留 inserted node / broken links。
```

### Commit 8：branch_fork write

```text
Implement branch_fork
Create Sequence
Apply explicit sequence_order
Connect original successor and inserted logic to Sequence then pins
Return sequence_ref
```

验收：

```text
sequence_order 缺失 blocked。
sequence_order 控制原后继和新逻辑顺序。
成功只额外返回 sequence_ref。
```

### Commit 9：Journal / success / failure contract

```text
Write Merge transaction journal before success
Return merged_ref + write_ref + validation
Journal failure rolls back and returns journal_write_failed
rollback blocked/failed sets modified=true
```

验收：

```text
成功结果极简。
失败不返回 merge_result / write_ref。
rollback_failed 时 modified=true。
```

### Commit 10：Bridge 接入与权限

```text
Register merge_blueprint_graph command
Add RequestValidator rules
Add write permission / token gate
Add automation tests
```

验收：

```text
未授权写入被拒绝。
dry_run 不修改资产。
正式写入重复 preflight。
```

---

## 19. 自动化测试清单

建议新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperMergeGraphContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperMergeGraphAnchorTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperMergeGraphStrategyTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperMergeGraphRollbackTests.cpp
```

### 19.1 Contract 测试

```text
1. success_result_minimal_contract
   - 包含 ok/schema/operation/trace_id/status/modified/target/data/validation
   - target 只包含 asset_path / graph / merge_scope / insert_strategy
   - data.schema = MergeBlueprintGraph.v1
   - data.merge_result.merged_ref.graph_id
   - data.merge_result.merged_ref.anchor_ref
   - data.merge_result.merged_ref.inserted_ref
   - data.write_ref.transaction_id
   - 不包含 target_type / merge_plan / disconnected_links / created_links / affected_user_nodes

2. branch_fork_success_contract
   - branch_fork 成功返回 sequence_ref
   - 不返回 sequence_order / created_links

3. dry_run_passed_minimal_contract
   - status=dry_run
   - modified=false
   - data.schema = MergeBlueprintGraphDryRun.v1
   - data.dry_run.result=passed
   - data.dry_run.can_execute=true
   - 不包含 merge_plan / would_xxx

4. dry_run_blocked_contract
   - ok=true
   - status=dry_run
   - result=blocked
   - blocked_by / conflicts / errors 存在
```

### 19.2 Anchor 测试

```text
1. resolve_anchor_by_full_path
2. resolve_anchor_by_group_ref
3. reject_anchor_pin_not_found
4. reject_anchor_pin_not_exec
5. reject_cross_group_ref
6. reject_display_name_only
```

### 19.3 Strategy 测试

```text
1. append_after_succeeds_when_anchor_has_no_successor
2. append_after_blocks_when_anchor_has_successor
3. insert_between_succeeds_with_single_successor
4. insert_between_blocks_without_successor
5. insert_between_blocks_with_multiple_successors
6. branch_fork_requires_sequence_order
7. branch_fork_respects_sequence_order_original_first
8. branch_fork_respects_sequence_order_inserted_first
```

### 19.4 Inserted logic 测试

```text
1. merge_owned_block_call_resolves_owned_block
2. merge_owned_block_call_rejects_missing_block
3. merge_custom_event_call_resolves_unique_event
4. merge_custom_event_call_rejects_missing_event
5. merge_function_call_resolves_function
6. merge_function_call_rejects_no_exec_function
```

### 19.5 Rollback 测试

```text
1. rollback_append_after_link_failure
2. rollback_insert_between_after_disconnect_failure
3. rollback_insert_between_after_second_link_failure_restores_original_link
4. rollback_branch_fork_removes_sequence_and_inserted_node
5. rollback_journal_write_failed
6. rollback_blocked_sets_modified_true
```

---

## 20. 第一版不做的内容

第一版明确不做：

```text
1. 不支持 inline_nodes 直接插入一段新节点。
2. 不支持 event_entry_logic 自动创建或修改事件入口。
3. 不自动查找 BeginPlay / Tick / InputAction。
4. 不自动选择 append_after / insert_between / branch_fork。
5. 不自动决定 sequence_order。
6. 不跨 LogicJson group 使用局部 ref。
7. 不按显示名模糊定位节点或 Pin。
8. 不返回 disconnected_links / created_links / affected_user_nodes。
9. 不返回 merge_plan / execution_order_preview。
10. 不用 Append 代替 Merge 接入已有执行流。
```

---

## 21. 最小验收标准

完成后，`insert_between` 成功返回必须符合：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "merge_blueprint_graph",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EventGraph",
    "merge_scope": "owned_block_call",
    "insert_strategy": "insert_between"
  },
  "data": {
    "schema": "MergeBlueprintGraph.v1",
    "merge_result": {
      "merged_ref": {
        "graph_id": "EventGraph",
        "anchor_ref": "BeginPlay0.Then",
        "inserted_ref": "EG_PhysicsDoor_TogglePhysicsDoor0"
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

并且必须不出现：

```text
target_type
merge_plan
disconnected_links
created_links
execution_order_changed
affected_user_nodes
ownership
review
safety
diagnostics
next
rollback_data
journal_path
```

`branch_fork` 成功只允许在 `merged_ref` 内额外出现：

```json
"sequence_ref": "Sequence0"
```

这就是 MergeBlueprintGraph UE 侧 C++ 第一版是否合格的核心判定。
