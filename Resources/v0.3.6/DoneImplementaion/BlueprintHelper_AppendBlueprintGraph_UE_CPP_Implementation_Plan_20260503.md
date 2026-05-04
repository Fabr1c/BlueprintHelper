# BlueprintHelper AppendBlueprintGraph UE 侧 C++ 可执行实现计划

状态：[x] 已完成
日期：2026-05-03  
适用范围：BlueprintHelper UE 插件侧 / UE5.3+  
目标版本：v0.4 / v0.5 前置实现  
依据文档：`BlueprintHelper_AppendBlueprintGraph_UE_FieldMapping_20260503_synced.md`  

---

## 0. 目标与边界

本计划用于在 UE 插件侧实现 `AppendBlueprintGraph`。

核心目标：

```text
新增 AppendBlueprintGraph，第一版允许创建新的 EG_{FeatureName} 图表
或向已有空图表追加独立 BlueprintHelper-owned block。
```

`AppendBlueprintGraph` 只负责：

```text
1. 追加新的独立 BlueprintHelper-owned 逻辑块。
2. 创建新的 EG_{FeatureName} 事件图表。
3. 向已有空图表追加独立逻辑块。
4. 创建唯一命名的 Custom Event。
5. 创建普通节点。
6. 创建新节点之间的连线。
7. 写入 BlueprintHelper-owned Metadata + NodeComment。
8. 内部写 Transaction Journal / Review / rollback_data。
```

`AppendBlueprintGraph` 禁止：

```text
1. 自动连接已有节点。
2. 自动接入已有执行流。
3. 覆盖旧节点。
4. 删除旧节点。
5. 清理旧 block。
6. 修改用户节点。
7. 创建 BeginPlay / Tick / InputAction / Overlap / Hit 等全局事件节点。
8. 创建函数图。
9. 在 Conservative 下追加到函数图。
```

相关工具边界：

```text
接入已有执行流：MergeBlueprintGraph
替换已有实现：ReplaceBlueprintGraph
精确修改节点 / Pin / 默认值 / 连线：PatchBlueprintGraph
```

---

## 1. 当前实现基线判断

当前 UE 侧 v0.3.4 已有可复用底座：

```text
FBlueprintHelperAgentImportService
FBlueprintHelperGraphResolver
FBlueprintHelperScopedAssetMutation
FBlueprintHelperToolResultBase / FBlueprintHelperToolResultBuilder
TextToBlueprintGenerator / NodeHandler
```

推荐实现方式：

```text
新增 AppendBlueprintGraphService
↓
复用 AgentImportService 的解析 / 节点生成能力
↓
不直接复用旧 import_agent_graph 的 Agent-facing 返回结构
↓
Append 服务自行负责 preflight、目标图表创建、block_id / ownership / journal / 极简 ToolResultBase
```

原因：

```text
1. 新字段协议要求 operation 固定为 append_blueprint_graph。
2. 成功只返回 graph_id / graph_name / block_refs / write_ref / validation。
3. 不返回 ownership / review / safety / diagnostics / node_guid / pin_guid / created_nodes。
4. dry_run 成功只返回 result=passed / can_execute=true。
5. dry_run blocked 只返回 blocked_by / conflicts / errors。
6. Journal 写入失败不能报告成功。
7. ownership metadata / NodeComment 写入失败必须整体失败并回滚。
```

---

## 2. Phase A：新增 Append 专属类型文件

### 2.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperAppendGraphTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperAppendGraphTypes.cpp
```

### 2.2 新增枚举

```cpp
enum class EBlueprintHelperAppendStage : uint8
{
    ParseInput,
    Auth,
    ResolveTarget,
    Preflight,
    CreateGraph,
    CreateNodes,
    ConnectPins,
    WriteMetadata,
    WriteJournal,
    Rollback
};

enum class EBlueprintHelperAppendErrorCode : uint8
{
    TargetBlueprintNotFound,
    TargetNotBlueprint,
    TargetGraphNotEmpty,
    TargetGraphTypeInvalid,
    CustomEventAlreadyExists,
    GlobalEventCreationDisallowed,
    FunctionNotFound,
    EventNotFound,
    CallSignatureMismatch,
    PinNotFound,
    PinTypeMismatch,
    SchemaRejected,
    NodeCreateFailed,
    LinkCreateFailed,
    OwnershipWriteFailed,
    JournalWriteFailed,
    RollbackBlocked,
    RollbackFailed,
    WritePermissionDisabled,
    ProfilePolicyViolation,
    BridgeDisconnected
};

enum class EBlueprintHelperDryRunResult : uint8
{
    Passed,
    Blocked
};
```

### 2.3 Rollback 结果枚举

如旧枚举不满足字段协议，需要同步扩展为：

```cpp
enum class EBlueprintHelperRollbackResult : uint8
{
    NotNeeded,
    RolledBack,
    Blocked,
    Failed
};
```

JSON 输出必须映射为：

```text
not_needed
rolled_back
blocked
failed
```

---

## 3. Phase B：新增 Append 专属结果结构

### 3.1 成功结果结构

```cpp
struct FBlueprintHelperAppendGraphInfo
{
    FString GraphId;
    FString GraphName;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperWriteRef
{
    FString TransactionId;
    bool bJournalRecorded = false;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperAppendGraphResult
{
    FBlueprintHelperAppendGraphInfo Graph;
    TArray<FString> BlockRefs;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperAppendGraphResultData
{
    FString Schema = TEXT("AppendBlueprintGraph.v1");
    FBlueprintHelperAppendGraphResult AppendResult;
    FBlueprintHelperWriteRef WriteRef;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.2 dry_run 结果结构

```cpp
struct FBlueprintHelperDryRunIssue
{
    FString Code;
    FString Message;
    FString Target;
    FString Source;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperAppendDryRunResult
{
    EBlueprintHelperDryRunResult Result = EBlueprintHelperDryRunResult::Passed;
    bool bCanExecute = true;
    TArray<FString> BlockedBy;
    TArray<FBlueprintHelperDryRunIssue> Conflicts;
    TArray<FBlueprintHelperDryRunIssue> Errors;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperAppendDryRunData
{
    FString Schema = TEXT("AppendBlueprintGraphDryRun.v1");
    FBlueprintHelperAppendDryRunResult DryRun;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.3 block_ref 规则

`block_refs` 是 `TArray<FString>`，不是 block 对象数组。

完整 `block_id` 反推规则：

```text
full_block_id = graph_id + "_" + block_ref
```

示例：

```text
graph_id = EG_PhysicsDoor
block_ref = TogglePhysicsDoor0
full_block_id = EG_PhysicsDoor_TogglePhysicsDoor0
```

成功结果不返回：

```text
blocks[].entry_type
blocks[].entry_name
node_guid
pin_guid
node_path
pin_path
link_path
created_nodes
created_links
```

这些进入：

```text
Transaction Journal
Review Store
rollback_data
verbose/debug
```

---

## 4. Phase C：新增 BlockId / Ownership / Journal 基础服务

### 4.1 新增 FBlueprintHelperBlockIdService

文件：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperBlockIdService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperBlockIdService.cpp
```

接口：

```cpp
class FBlueprintHelperBlockIdService
{
public:
    FString MakeBlockRef(
        UBlueprint* Blueprint,
        UEdGraph* Graph,
        const FString& EntryName) const;

    FString MakeFullBlockId(
        const FString& GraphId,
        const FString& BlockRef) const;
};
```

规则：

```text
block_ref = {EntryName}{Index}
full_block_id = {GraphId}_{BlockRef}
```

递增作用域：

```text
同一蓝图 + 同一图表 + 同一入口名
```

实现要求：

```text
1. 扫描当前图表已有 BlueprintHelperBlockId metadata。
2. 扫描本次 request 中同名 entry。
3. 必要时读取 Journal 中同图表同 entry 的历史记录。
4. 取最大 index + 1。
```

### 4.2 新增 FBlueprintHelperOwnershipService

文件：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperOwnershipService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperOwnershipService.cpp
```

接口：

```cpp
class FBlueprintHelperOwnershipService
{
public:
    bool WriteNodeOwnership(
        UBlueprint* Blueprint,
        UEdGraphNode* Node,
        const FString& BlockId,
        const FString& TransactionId,
        const FString& FeatureName,
        FString& OutError) const;

    bool WriteBlockOwnership(
        UBlueprint* Blueprint,
        const TArray<UEdGraphNode*>& Nodes,
        const FString& BlockId,
        const FString& TransactionId,
        const FString& FeatureName,
        FString& OutError) const;
};
```

Metadata 写入建议：

```cpp
UPackage* Package = Node->GetOutermost();
UMetaData* MetaData = Package ? Package->GetMetaData() : nullptr;

if (!MetaData)
{
    OutError = TEXT("metadata_unavailable");
    return false;
}

MetaData->SetValue(Node, TEXT("BlueprintHelperOwned"), TEXT("true"));
MetaData->SetValue(Node, TEXT("BlueprintHelperBlockId"), *BlockId);
MetaData->SetValue(Node, TEXT("BlueprintHelperTransactionId"), *TransactionId);
MetaData->SetValue(Node, TEXT("BlueprintHelperTool"), TEXT("AppendBlueprintGraph"));
MetaData->SetValue(Node, TEXT("BlueprintHelperFeatureName"), *FeatureName);
```

NodeComment 写入建议：

```cpp
Node->NodeComment = FString::Printf(
    TEXT("[BlueprintHelper]\nblock_id=%s\ntx=%s\ntool=AppendBlueprintGraph"),
    *BlockId,
    *TransactionId);
```

失败规则：

```text
任何一个 owned node 写 metadata 或 NodeComment 失败
→ 整个 Append 失败
→ rollback
→ error.code = ownership_write_failed
→ error.stage = write_metadata
```

成功结果不返回 ownership 字段。

### 4.3 新增 FBlueprintHelperTransactionJournalService

文件：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperTransactionJournalService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperTransactionJournalService.cpp
```

接口：

```cpp
class FBlueprintHelperTransactionJournalService
{
public:
    FString GenerateTransactionId() const;

    bool WriteAppendJournal(
        const FBlueprintHelperAppendJournalRecord& Record,
        FString& OutError) const;
};
```

落盘路径：

```text
<Project>/Saved/BlueprintHelper/Transactions/Active/tx_xxx.json
<Project>/Saved/BlueprintHelper/Review/
```

Journal 记录：

```text
transaction_id
tool = AppendBlueprintGraph
status
target_assets
graph
blocks
created_nodes
created_links
diff_summary
rollback_data
validation
```

Agent-facing 成功只返回：

```json
"write_ref": {
  "transaction_id": "...",
  "journal_recorded": true
}
```

Journal 写入失败：

```text
error.code = journal_write_failed
error.stage = write_journal
rollback_result = rolled_back / blocked / failed
不返回 write_ref
不报告成功
```

---

## 5. Phase D：新增 AppendBlueprintGraphService

### 5.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperAppendBlueprintGraphService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperAppendBlueprintGraphService.cpp
```

### 5.2 服务接口

```cpp
class FBlueprintHelperAppendBlueprintGraphService
{
public:
    FBlueprintHelperAppendBlueprintGraphService(
        const FBlueprintHelperGraphResolver& InResolver,
        const FBlueprintHelperAgentImportService& InAgentImportService,
        const FBlueprintHelperBlockIdService& InBlockIdService,
        const FBlueprintHelperOwnershipService& InOwnershipService,
        const FBlueprintHelperTransactionJournalService& InJournalService);

    FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
    FBlueprintHelperAppendRequest ParseRequest(const TSharedPtr<FJsonObject>& Payload) const;
    FBlueprintHelperAppendPreflightResult Preflight(const FBlueprintHelperAppendRequest& Request) const;
    UEdGraph* FindOrCreateAppendGraph(UBlueprint* Blueprint, const FString& GraphName, FString& OutError) const;
    FBlueprintHelperToolResultBase ExecuteDryRun(const FBlueprintHelperAppendRequest& Request) const;
    FBlueprintHelperToolResultBase ExecuteWrite(const FBlueprintHelperAppendRequest& Request) const;
};
```

### 5.3 输入结构建议

```json
{
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor"
  },
  "feature_name": "PhysicsDoor",
  "nodes": [],
  "links": [],
  "dry_run": false
}
```

内部可以转换为旧 AgentImportGraph 兼容结构：

```json
{
  "schema": "BlueprintHelper.AgentImportGraph",
  "version": "1.0",
  "target_blueprint": "...",
  "target_graph": "...",
  "mode": "append",
  "nodes": [],
  "links": [],
  "options": {
    "compile": false,
    "save": false,
    "strict": true,
    "dry_run": false,
    "create_missing_variables": false,
    "reconstruct_existing_nodes": false
  }
}
```

第一版 Append 不应自动创建缺失变量。

---

## 6. Phase E：目标图表创建与约束

### 6.1 引擎 API 落点

```cpp
UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(
    Blueprint,
    FName(*GraphName),
    UEdGraph::StaticClass(),
    UEdGraphSchema_K2::StaticClass());

FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Graph);
FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
```

### 6.2 图表处理规则

```cpp
UEdGraph* FBlueprintHelperAppendBlueprintGraphService::FindOrCreateAppendGraph(
    UBlueprint* Blueprint,
    const FString& GraphName,
    FString& OutError) const
{
    // 1. 在 Blueprint->UbergraphPages 查找 GraphName
    // 2. 如果不存在：CreateNewGraph + AddUbergraphPage
    // 3. 如果存在且 Graph->Nodes.Num() == 0：允许写入
    // 4. 如果存在且非空：target_graph_not_empty
    // 5. 如果同名存在于 FunctionGraphs / MacroGraphs：target_graph_type_invalid
}
```

禁止：

```text
1. 创建 FunctionGraph。
2. 修改 FunctionGraph。
3. 追加到非空事件图。
4. 自动改名图表。
5. 依赖当前编辑器焦点。
```

---

## 7. Phase F：Preflight / dry_run 实现

### 7.1 Preflight 检查项

```text
1. payload schema / nodes / links 合法。
2. asset_path 存在且是 UBlueprint。
3. target.graph 存在时必须是 UbergraphPage。
4. target.graph 非空时失败。
5. 不允许 event kind 创建 BeginPlay / Tick / ConstructionScript / InputAction / Overlap / Hit。
6. Custom Event 名称必须唯一。
7. call 节点引用的函数必须存在。
8. call 节点引用的事件必须存在。
9. link 两端 node/pin 必须能解析。
10. 通过 UEdGraphSchema_K2::CanCreateConnection / TryCreateConnection 预判 Pin 兼容性。
```

### 7.2 dry_run 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "append_blueprint_graph",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "graph",
    "graph": "EG_PhysicsDoor"
  },
  "data": {
    "schema": "AppendBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

### 7.3 dry_run blocked 返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "append_blueprint_graph",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "graph",
    "graph": "EG_PhysicsDoor"
  },
  "data": {
    "schema": "AppendBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "target_graph_not_empty"
      ],
      "conflicts": [
        {
          "code": "target_graph_not_empty",
          "message": "AppendBlueprintGraph cannot append to a non-empty existing graph."
        }
      ],
      "errors": []
    }
  }
}
```

dry_run 不返回：

```text
plan
would_xxx
references
graph action
block_ref
block_id
write_ref
transaction_id
ownership
review
safety
diagnostics
next
warnings
```

---

## 8. Phase G：正式写入流程

正式写入顺序：

```text
1. ParseRequest
2. ResolveBlueprint
3. Preflight
4. Begin FBlueprintHelperScopedAssetMutation
5. FindOrCreateAppendGraph
6. 调用 AgentImport 内部构建能力生成节点和连线
7. 按 Custom Event entry 分组生成 block_ref / full_block_id
8. 写 ownership metadata + NodeComment
9. 生成 rollback_data / diff / Journal record
10. 写 Transaction Journal / Review Store
11. MarkBlueprintAsStructurallyModified / MarkPackageDirty
12. 返回极简 ToolResultBase
```

### 8.1 抽取 Graph Build Core

从 `FBlueprintHelperAgentImportService` 抽出可复用构建层：

```text
FBlueprintHelperAgentGraphBuildService
```

建议接口：

```cpp
struct FBlueprintHelperGraphBuildRequest
{
    UBlueprint* Blueprint = nullptr;
    UEdGraph* Graph = nullptr;
    TArray<FBlueprintHelperAgentImportNode> Nodes;
    TArray<FBlueprintHelperAgentImportLink> Links;
    bool bCreateMissingVariables = false;
};

struct FBlueprintHelperGraphBuildResult
{
    TMap<FString, UEdGraphNode*> IdToNode;
    TArray<UEdGraphNode*> CreatedNodes;
    TArray<FBlueprintHelperCreatedLinkRef> CreatedLinks;
    FBlueprintHelperDiagnosticSet Diagnostics;
};

class FBlueprintHelperAgentGraphBuildService
{
public:
    bool BuildIntoGraph(
        const FBlueprintHelperGraphBuildRequest& Request,
        FBlueprintHelperGraphBuildResult& OutResult,
        FString& OutError) const;
};
```

旧 `import_agent_graph` 继续调用该 BuildService，保证旧功能不退化。

### 8.2 连线策略

连线前检查：

```cpp
const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
const FPinConnectionResponse Response = Schema->CanCreateConnection(FromPin, ToPin);

if (Response.Response == CONNECT_RESPONSE_DISALLOW)
{
    // error.code = pin_type_mismatch 或 link_create_failed
    // error.stage = connect_pins
}
```

正式连线：

```cpp
if (!Schema->TryCreateConnection(FromPin, ToPin))
{
    // rollback
}
```

---

## 9. Phase H：block 归属算法

### 9.1 第一版分组规则

```text
1. 每个 custom_event 节点是一个 block entry。
2. block_ref = EntryName + Index。
3. full_block_id = graph_id + "_" + block_ref。
4. 从 entry 出发沿 exec links 做 BFS。
5. 遇到其他 custom_event entry 时停止，不跨入另一个 block。
6. data dependency 节点如果只被一个 block 使用，归属该 block。
7. data dependency 节点如果被多个 block 共享，第一版直接 schema_rejected。
8. comment 节点如果 contains 的节点全部属于同一 block，则归属该 block。
9. comment 节点跨 block 时不写 block metadata，只进 Journal snapshot。
```

### 9.2 成功返回 block_refs

```json
"block_refs": [
  "TogglePhysicsDoor0",
  "OpenPhysicsDoor0",
  "ClosePhysicsDoor0"
]
```

不返回：

```text
entry_type
entry_name
full block_id
created_nodes
created_links
```

---

## 10. Phase I：正式失败处理

正式失败只返回：

```text
ok=false
status=failed
modified=false 或 true
target
error
```

正式失败不返回：

```text
data.append_result
data.write_ref
data.ownership
review
safety
diagnostics
next
created_nodes
created_links
rollback_data
```

### 10.1 preflight 失败

```text
modified=false
rollback_result=not_needed
```

### 10.2 写入中失败且 rollback 成功

```text
modified=false
rollback_result=rolled_back
```

### 10.3 写入中失败且 rollback blocked / failed

```text
modified=true
rollback_result=blocked 或 failed
Agent 必须 stop_and_report
不得继续 compile/save/patch
```

### 10.4 ownership_write_failed

```json
{
  "error": {
    "code": "ownership_write_failed",
    "stage": "write_metadata",
    "message": "BlueprintHelper ownership metadata could not be written.",
    "retryable": false,
    "rollback_result": "rolled_back"
  }
}
```

### 10.5 journal_write_failed

```json
{
  "error": {
    "code": "journal_write_failed",
    "stage": "write_journal",
    "message": "Transaction Journal could not be written.",
    "retryable": false,
    "rollback_result": "rolled_back"
  }
}
```

---

## 11. Phase J：Bridge Router 接入

### 11.1 Router 新增命令

```cpp
if (Request.Command == TEXT("append_blueprint_graph"))
{
    return HandleAppendBlueprintGraph(Request);
}
```

### 11.2 Handler

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleAppendBlueprintGraph(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result = AppendGraphService.Execute(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("append_blueprint_graph failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

### 11.3 Validator 新增规则

```cpp
if (CommandEquals(Command, TEXT("append_blueprint_graph")))
{
    const FBlueprintHelperFieldRule Rules[] = {
        {TEXT("target"), EBlueprintHelperJsonExpectedType::Object, true},
        {TEXT("feature_name"), EBlueprintHelperJsonExpectedType::String, false},
        {TEXT("nodes"), EBlueprintHelperJsonExpectedType::Array, true},
        {TEXT("links"), EBlueprintHelperJsonExpectedType::Array, false},
        {TEXT("dry_run"), EBlueprintHelperJsonExpectedType::Bool, false},
    };

    return ValidateRules(Payload, Rules, OutError);
}
```

写命令集合加入：

```text
append_blueprint_graph
```

---

## 12. Phase K：Build.cs 与 Include 检查

当前模块依赖应确认包含：

```text
BlueprintGraph
UnrealEd
GraphEditor
Kismet
Engine
CoreUObject
Json
JsonUtilities
```

Append 相关 include：

```cpp
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "UObject/MetaData.h"
```

如 `UMetaData` 编译失败，优先检查 `CoreUObject` 是否在 PrivateDependencyModuleNames 中。

---

## 13. 推荐提交顺序

### Commit 1：Append 类型与序列化

```text
Add AppendBlueprintGraph C++ result and dry_run types
Add append error/stage enums
Add write_ref and block_refs JSON builders
```

验收：

```text
可以构造 AppendBlueprintGraph.v1 / AppendBlueprintGraphDryRun.v1 JSON。
成功 JSON 不出现 ownership/review/safety/diagnostics/summary。
```

### Commit 2：BlockId / Ownership / Journal 基础

```text
Add BlockIdService
Add OwnershipService
Add TransactionJournalService skeleton
Write node UMetaData and NodeComment
Write minimal transaction journal json
```

验收：

```text
能生成 graph_id + block_ref → full_block_id。
能给 UEdGraphNode 写 BlueprintHelperOwned / BlockId / TransactionId。
Journal 写失败可返回 journal_write_failed。
```

### Commit 3：Graph target preflight

```text
Add AppendBlueprintGraphService preflight
Validate blueprint asset and graph target
Create new EG graph or reuse empty graph
Reject non-empty graph
Reject function/macro graph
Reject global event nodes
Reject duplicate custom event names
```

验收：

```text
新图表可创建。
空图表可写入。
非空图表返回 target_graph_not_empty。
函数图返回 target_graph_type_invalid。
BeginPlay/Tick/InputAction 返回 global_event_creation_disallowed。
```

### Commit 4：Extract Agent graph build core

```text
Extract reusable graph node/link builder from AgentImportService
Keep import_agent_graph behavior unchanged
AppendGraph uses build core without exposing old result format
```

验收：

```text
旧 import_agent_graph 自动化测试不退化。
AppendGraph 可以生成 custom_event/call/branch/sequence/comment 节点。
```

### Commit 5：Append write transaction

```text
AppendGraph owns FBlueprintHelperScopedAssetMutation
Snapshot graph before write
Create nodes and links
Rollback on node/link/default-value failure
```

验收：

```text
pin_type_mismatch 后不残留新图表或半成品节点。
link_create_failed 后不残留 broken block。
rollback_result 正确输出 rolled_back / blocked / failed。
```

### Commit 6：Block grouping and ownership

```text
Group nodes by custom_event entry
Generate block_refs
Write ownership metadata/comment
Reject ambiguous shared nodes in v1
```

验收：

```text
成功返回 block_refs。
完整 block_id 可由 graph_id + "_" + block_ref 反推。
节点 metadata 和 NodeComment 存在。
Agent-facing 结果仍不返回 ownership。
```

### Commit 7：Journal and success result

```text
Write append transaction journal before success response
Return data.append_result.graph
Return data.append_result.block_refs
Return data.write_ref
Return validation
```

验收：

```text
正式成功 JSON 与文档字段完全一致。
Journal 写失败时 rollback，且不返回 write_ref。
```

### Commit 8：Bridge command and validator

```text
Register append_blueprint_graph command
Add payload validation
Add write-command auth gate
Add regression tests
```

验收：

```text
未携带 Token 时写命令被拒绝。
dry_run 不修改资产。
正式写入需要 auth_token。
```

---

## 14. 自动化测试清单

### 14.1 Contract Tests

建议新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperAppendGraphContractTests.cpp
```

测试项：

```text
1. success_result_minimal_contract
   - 包含 ok/schema/operation/trace_id/status/modified/target/data/validation
   - data.schema = AppendBlueprintGraph.v1
   - data.append_result.graph.graph_id
   - data.append_result.block_refs[]
   - data.write_ref.transaction_id
   - 不包含 ownership/review/safety/diagnostics/summary

2. dry_run_passed_minimal_contract
   - status=dry_run
   - modified=false
   - data.schema = AppendBlueprintGraphDryRun.v1
   - data.dry_run.result=passed
   - data.dry_run.can_execute=true
   - 不包含 plan/would_xxx/block_ref/transaction_id

3. dry_run_blocked_contract
   - ok=true
   - status=dry_run
   - result=blocked
   - blocked_by/conflicts/errors 存在
```

### 14.2 Write Tests

建议新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperAppendGraphWriteTests.cpp
```

测试项：

```text
1. append_creates_new_eg_graph
2. append_to_existing_empty_graph
3. append_rejects_existing_non_empty_graph
4. append_rejects_global_event
5. append_rejects_duplicate_custom_event
6. append_returns_block_refs_only
7. append_writes_metadata_and_node_comment
8. append_writes_journal_before_success
```

### 14.3 Rollback Tests

建议新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperAppendGraphRollbackTests.cpp
```

测试项：

```text
1. rollback_on_node_create_failed
2. rollback_on_pin_not_found
3. rollback_on_pin_type_mismatch
4. rollback_on_ownership_write_failed
5. rollback_on_journal_write_failed
6. rollback_blocked_sets_modified_true
```

---

## 15. 第一版不做内容

第一版明确不做：

```text
1. 不接入已有 BeginPlay / Tick / IA / Overlap 执行流。
2. 不修改非空用户图表。
3. 不创建函数图。
4. 不创建全局事件节点。
5. 不自动创建变量。
6. 不自动创建缺失函数或缺失事件。
7. 不返回完整 block_id、node_guid、pin_guid、node_path、link_path。
8. 不返回 ownership/review/safety。
9. 不把 dry_run 当成 plan 展示工具。
10. 不把 Journal / Review 详情泄漏到 Agent-facing 成功结果。
```

---

## 16. 最小成功返回验收

正式成功返回必须满足：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "append_blueprint_graph",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "target_type": "graph",
    "graph": "EG_PhysicsDoor"
  },
  "data": {
    "schema": "AppendBlueprintGraph.v1",
    "append_result": {
      "graph": {
        "graph_id": "EG_PhysicsDoor",
        "graph_name": "EG_PhysicsDoor"
      },
      "block_refs": [
        "TogglePhysicsDoor0"
      ]
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

必须不出现：

```text
ownership
review
safety
diagnostics
summary
created_nodes
created_links
node_guid
pin_guid
rollback_data
journal_path
```

---

## 17. 关键风险点

### 17.1 不要把旧 import_agent_graph 返回体直接包出来

旧返回体通常包含：

```text
created_nodes
created_links
diagnostics
warnings
rollback_count
```

这些与 AppendBlueprintGraph 字段协议冲突。Append 必须独立构建 Agent-facing 返回。

### 17.2 不要在 dry_run 里预生成 block_ref

dry_run 不返回：

```text
block_ref
block_id
transaction_id
write_ref
```

原因是 dry_run 不应制造 ID 预占语义。

### 17.3 Journal 写入必须在成功返回前完成

否则会出现：

```text
Agent 收到 success
但 UE 内部无法审计 / rollback / review
```

字段协议要求：

```text
Journal 写入失败时 Graph Write 不能报告成功。
```

### 17.4 ownership 写入失败不是 warning

ownership metadata / NodeComment 是 Append 成功语义的一部分。失败必须整体失败并回滚。

### 17.5 Graph 创建失败要纳入 rollback


---

## 18. 最终完成定义

本计划完成后，应达到：

```text
1. UE Bridge 可调用 append_blueprint_graph。
2. dry_run passed / blocked 返回符合极简字段协议。
3. 正式成功返回 graph / block_refs / write_ref / validation。
4. 正式失败只返回 error。
5. 新增节点写入 BlueprintHelper-owned Metadata + NodeComment。
6. Journal 成功写入后才返回 applied。
7. 写入中失败能 rollback，不留下半成品。
8. 非空图表、全局事件、重复 Custom Event、Pin 不兼容都能被阻断。
9. 旧 import_agent_graph 功能不退化。
10. 测试覆盖 contract / write / rollback 三类场景。
```
