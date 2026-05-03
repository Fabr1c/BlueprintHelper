# BlueprintHelper ReplaceBlueprintGraph UE 侧 C++ 可执行实现计划

日期：2026-05-03  
输入文档：`BlueprintHelper_ReplaceBlueprintGraph_UE_FieldMapping_20260503.md`  
适用范围：UE 插件侧 / C++ 实现  
目标版本：BlueprintHelper v0.4 / v0.5 前置协议收敛  

---

## 0. 计划目标

本计划把 `ReplaceBlueprintGraph` 字段映射文档转成 UE 侧可直接执行的 C++ 实现任务。

`ReplaceBlueprintGraph` 的目标不是追加新逻辑，也不是接入已有执行流，而是：

```text
替换一个明确目标的完整实现。
```

可替换目标包括：

```text
BlueprintHelper-owned block
function_body
event_body
custom_event_body
function_definition
event_definition
graph
```

与其他 Graph Write 工具的边界：

```text
AppendBlueprintGraph：追加新的独立 owned 逻辑块。
MergeBlueprintGraph：接入已有执行流。
PatchBlueprintGraph：精确修改节点 / Pin / 默认值 / 连接。
Cleanup 工具簇：清理旧 block。
ReplaceBlueprintGraph：替换明确目标的完整实现。
```

本计划只覆盖 UE 插件侧 C++，不覆盖 MCP Server TypeScript 参数声明、Agent Skill 文档或用户引导文档。

---

## 1. 当前代码可复用基线

基于 `UE侧v0.3.4` 源码静态检查，Replace 可以复用以下现有能力：

| 现有模块 | 当前能力 | Replace 中的用途 |
|---|---|---|
| `FBlueprintHelperGraphResolver` | 解析 Blueprint / Graph / Target | 定位目标蓝图、函数图、事件图。 |
| `FBlueprintHelperAgentImportService` | 解析 AgentImportGraph，创建节点和连线，支持 strict rollback / dry_run | 抽出 GraphBuildCore，作为 Replace 写入新实现的底座。 |
| `FBlueprintHelperScopedAssetMutation` | 基于 `FScopedTransaction` 的写入事务与 rollback | Replace 写入中失败时回滚。 |
| `FBlueprintHelperToolResultBase` | 统一 ToolResultBase，序列化时默认不输出 safety / transaction / review | 输出 Replace 的极简 Agent-facing 结果。 |
| `FBlueprintHelperRuntimeProfileService` | runtime profile 已存在 | 写入前 profile 由 Agent 调用；UE 写命令继续走 Token / write gate。 |
| `FBlueprintHelperRequestValidator` | 写命令 Token 校验集合 | 增加 `replace_blueprint_graph`。 |
| NodeHandlers | `custom_event / event / call / branch / sequence / variable / comment` 等节点生成 | 新实现 body 的节点生成底层。 |

不建议直接复用 `ImportAgentGraph` 的返回结构，因为 Replace 文档要求成功结果极简，不能返回 `summary / node count / ownership / diagnostics / review / safety`。

---

## 2. 字段契约硬约束

实现时必须满足以下字段契约。

### 2.1 operation

```json
"operation": "replace_blueprint_graph"
```

### 2.2 成功 data.schema

```json
"schema": "ReplaceBlueprintGraph.v1"
```

### 2.3 dry_run data.schema

```json
"schema": "ReplaceBlueprintGraphDryRun.v1"
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
target.replace_scope
data.schema
data.replace_result.replaced_ref.graph_id
data.replace_result.replaced_ref.target_ref
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
replace_plan
before
after
full_diff
deleted_nodes / created_nodes / modified_nodes 计数
ownership
review
safety
diagnostics
next
journal_path
rollback_data
```

### 2.6 dry_run 成功只返回

```json
{
  "data": {
    "schema": "ReplaceBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

### 2.7 dry_run blocked 只返回

```text
result=blocked
can_execute=false
blocked_by
conflicts
errors
```

### 2.8 正式失败只返回 error

正式失败不得返回：

```text
data.replace_result
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
failed_item 可选
conflicts 可选
```

---

## 3. 新增文件规划

### 3.1 Replace 专属类型

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperReplaceGraphTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperReplaceGraphTypes.cpp
```

### 3.2 Replace 服务

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperReplaceBlueprintGraphService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperReplaceBlueprintGraphService.cpp
```

### 3.3 Graph Write 共用服务

如果 Append 已经实现，下列服务应直接复用；如果还没有实现，应在本阶段一起补齐：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperGraphWriteTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperGraphWriteTypes.cpp

Source/BlueprintHelper/Public/Services/BlueprintHelperWriteRefTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperWriteRefTypes.cpp

Source/BlueprintHelper/Public/Services/BlueprintHelperBlockIdService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperBlockIdService.cpp

Source/BlueprintHelper/Public/Services/BlueprintHelperOwnershipService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperOwnershipService.cpp

Source/BlueprintHelper/Public/Services/BlueprintHelperTransactionJournalService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperTransactionJournalService.cpp

Source/BlueprintHelper/Public/Services/BlueprintHelperGraphSnapshotService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperGraphSnapshotService.cpp

Source/BlueprintHelper/Public/Services/BlueprintHelperGraphBuildService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperGraphBuildService.cpp
```

`GraphBuildService` 是从 `FBlueprintHelperAgentImportService` 抽出的底层构建器，避免 Replace 直接调用旧 Import 工具并继承旧返回字段。

---

## 4. 新增枚举

### 4.1 `EBlueprintHelperReplaceScope`

```cpp
enum class EBlueprintHelperReplaceScope : uint8
{
    BlockImplementation,
    FunctionBody,
    EventBody,
    CustomEventBody,
    FunctionDefinition,
    EventDefinition,
    Graph
};
```

序列化：

```text
block_implementation
function_body
event_body
custom_event_body
function_definition
event_definition
graph
```

### 4.2 `EBlueprintHelperGraphWriteStage`

建议 Graph Write 共用：

```cpp
enum class EBlueprintHelperGraphWriteStage : uint8
{
    ParseInput,
    Auth,
    ResolveTarget,
    Preflight,
    DryRun,
    SnapshotBefore,
    DeleteOldImplementation,
    CreateNodes,
    CreateLinks,
    PreserveEntry,
    UpdateDefinition,
    WriteMetadata,
    WriteJournal,
    Rollback
};
```

序列化：

```text
parse_input
auth
resolve_target
preflight
dry_run
snapshot_before
delete_old_implementation
create_nodes
create_links
preserve_entry
update_definition
write_metadata
write_journal
rollback
```

### 4.3 `EBlueprintHelperReplaceErrorCode`

```cpp
enum class EBlueprintHelperReplaceErrorCode : uint8
{
    TargetBlueprintNotFound,
    TargetNotBlueprint,
    TargetGraphNotFound,
    TargetBlockNotFound,
    TargetFunctionNotFound,
    TargetEventNotFound,
    TargetCustomEventNotFound,
    TargetAmbiguous,
    TargetNotOwned,
    ReplaceScopeUnsupported,
    SignatureChangeDisallowed,
    EntryIdentityChangeDisallowed,
    ExternalDependentsMayBreak,
    UserNodeModificationNotAllowed,
    SchemaRejected,
    NodeCreateFailed,
    LinkCreateFailed,
    PinNotFound,
    PinTypeMismatch,
    OwnershipWriteFailed,
    JournalWriteFailed,
    RollbackBlocked,
    RollbackFailed,
    WritePermissionDisabled,
    ProfilePolicyViolation,
    BridgeDisconnected
};
```

### 4.4 Rollback 结果

如果当前项目已有旧枚举，应补齐到：

```cpp
enum class EBlueprintHelperRollbackResult : uint8
{
    NotNeeded,
    RolledBack,
    Blocked,
    Failed
};
```

序列化：

```text
not_needed
rolled_back
blocked
failed
```

---

## 5. 新增结果结构体

### 5.1 `FBlueprintHelperReplacedRef`

```cpp
struct FBlueprintHelperReplacedRef
{
    FString GraphId;
    FString TargetRef;

    TSharedRef<FJsonObject> ToJson() const;
};
```

JSON：

```json
{
  "graph_id": "EG_PhysicsDoor",
  "target_ref": "TogglePhysicsDoor0"
}
```

规则：

```text
owned block 替换：target_ref = 原 block_ref。
function_body 替换：target_ref = 函数图名 / 函数名。
event_body 替换：target_ref = 事件入口名或稳定 entry_ref。
custom_event_body 替换：target_ref = Custom Event 名或原 block_ref。
graph 替换：target_ref = graph_id。
```

不返回：

```text
target_kind
entry_type
entry_name
summary
```

目标类型由 `target.replace_scope` 推导。

### 5.2 `FBlueprintHelperReplaceGraphResult`

```cpp
struct FBlueprintHelperReplaceGraphResult
{
    FBlueprintHelperReplacedRef ReplacedRef;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 5.3 `FBlueprintHelperReplaceGraphResultData`

```cpp
struct FBlueprintHelperReplaceGraphResultData
{
    FString Schema = TEXT("ReplaceBlueprintGraph.v1");
    FBlueprintHelperReplaceGraphResult ReplaceResult;
    FBlueprintHelperWriteRef WriteRef;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 5.4 `FBlueprintHelperReplaceDryRunData`

```cpp
struct FBlueprintHelperReplaceDryRunResult
{
    FString Result; // passed | blocked
    bool bCanExecute = false;
    TArray<FString> BlockedBy;
    TArray<FBlueprintHelperGraphWriteIssue> Conflicts;
    TArray<FBlueprintHelperGraphWriteIssue> Errors;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperReplaceDryRunData
{
    FString Schema = TEXT("ReplaceBlueprintGraphDryRun.v1");
    FBlueprintHelperReplaceDryRunResult DryRun;

    TSharedRef<FJsonObject> ToJson() const;
};
```

---

## 6. Replace 请求模型

字段映射文档只定义 Agent-facing 返回字段。UE 侧仍需要明确输入模型。建议第一版输入如下：

```json
{
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "replace_scope": "block_implementation"
  },
  "selector": {
    "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0",
    "target_ref": "TogglePhysicsDoor0",
    "entry_name": "TogglePhysicsDoor",
    "node_path": "optional LogicJson path"
  },
  "replacement": {
    "nodes": [],
    "links": []
  },
  "options": {
    "dry_run": false,
    "strict": true,
    "preserve_layout": false
  }
}
```

### 6.1 输入字段规则

| 字段 | 规则 |
|---|---|
| `target.asset_path` | 必填，目标 Blueprint 资产路径。 |
| `target.graph` | 必填，目标图表 / 函数图 / 事件图名。 |
| `target.replace_scope` | 必填。 |
| `selector.block_id` | `block_implementation` 优先使用。 |
| `selector.target_ref` | owned block 时可为 block_ref；function_body 时可为函数名。 |
| `selector.entry_name` | event/custom_event body 目标入口。 |
| `selector.node_path` | 精确定位时可用，来源应为 LogicJson。 |
| `replacement.nodes` | 新实现节点。 |
| `replacement.links` | 新实现连线。 |
| `options.dry_run` | dry_run 时不得修改资产。 |
| `options.strict` | 第一版固定 true。 |

### 6.2 输入禁用项

```text
不允许 create_missing_variables=true。
不允许 save_after_write=true。
不允许 compile_after_write=true，除非后续 workflow 明确设计。
不允许 safety_profile override。
不允许 no_journal=true。
不允许 no_review=true。
```

---

## 7. Target Resolver 设计

新增：

```text
FBlueprintHelperReplaceTargetResolver
```

可以内嵌在 `FBlueprintHelperReplaceBlueprintGraphService` 内，也可以独立文件。

### 7.1 输出结构

```cpp
struct FBlueprintHelperResolvedReplaceTarget
{
    UBlueprint* Blueprint = nullptr;
    UEdGraph* Graph = nullptr;
    EBlueprintHelperReplaceScope Scope;

    FString AssetPath;
    FString GraphName;
    FString GraphId;
    FString TargetRef;

    FString OriginalBlockId;
    FString OriginalBlockRef;
    bool bIsBlueprintHelperOwned = false;

    UEdGraphNode* EntryNode = nullptr;
    UK2Node_FunctionEntry* FunctionEntry = nullptr;
    TArray<UK2Node_FunctionResult*> FunctionResults;

    TArray<UEdGraphNode*> NodesToDelete;
    TArray<UEdGraphNode*> NodesToPreserve;
    TArray<UEdGraphNode*> ExistingOwnedNodes;
    TArray<UEdGraphNode*> ExistingUserNodes;

    bool bExternalDependentsMayBreak = false;
};
```

---

## 8. replace_scope 逐项实现策略

## 8.1 `block_implementation`

### 定位规则

优先级：

```text
1. selector.block_id
2. target.graph + selector.target_ref → full_block_id = graph + "_" + target_ref
3. selector.node_path 定位 entry node 后读取 metadata.block_id
```

必须满足：

```text
1. 找到带 BlueprintHelperBlockId 的 owned nodes。
2. block_id 与请求目标一致。
3. 不包含 user-owned / unknown 节点。
4. block 所在图表与 target.graph 一致。
```

失败码：

```text
target_block_not_found
target_not_owned
target_ambiguous
```

### 替换策略

```text
1. 删除该 block_id 对应的旧 nodes / links。
2. 写入 replacement.nodes / replacement.links。
3. 保留原 block_id。
4. replaced_ref.target_ref 返回原 block_ref。
5. 新节点继续写相同 block_id 的 ownership metadata。
```

### 关键约束

```text
不能生成新 block_ref。
不能接管额外用户节点。
不能删除同图表其他 block。
不能删除 unknown 节点。
```

---

## 8.2 `function_body`

### 定位规则

```text
1. target.graph 必须能解析为 FunctionGraph。
2. 找到且仅找到一个 UK2Node_FunctionEntry。
3. 保存 FunctionEntry 的签名、参数、返回值。
4. 找到所有 UK2Node_FunctionResult，作为返回节点保留或重建约束对象。
```

### 替换策略

第一版建议：

```text
1. 保留 UEdGraph 对象。
2. 保留 UK2Node_FunctionEntry。
3. 保留函数名、参数 Pin、返回 Pin、外部可调用身份。
4. 删除 body 内部普通节点。
5. 可删除旧 FunctionResult 并重建，但必须保持返回签名一致。
6. replacement 必须能连接到原 entry / result 语义。
```

更安全的第一版实现：

```text
保留 FunctionEntry。
保留 FunctionResult 节点。
只删除非 Entry / Result 的普通节点。
新逻辑连接到已有 Entry / Result。
```

### 签名校验

dry_run / preflight 必须检查：

```text
1. replacement 不新增 / 删除函数参数。
2. replacement 不修改返回值签名。
3. replacement 不修改函数名。
4. replacement 不修改 AccessSpecifier / Pure / Const 等定义属性。
```

违反时：

```text
error.code = signature_change_disallowed
error.stage = preflight
rollback_result = not_needed
```

---

## 8.3 `custom_event_body`

### 定位规则

```text
1. target.graph 必须是事件图 / Ubergraph。
2. selector.entry_name 或 selector.node_path 必须唯一定位 UK2Node_CustomEvent。
3. 如果 Custom Event 属于 BlueprintHelper-owned block，保留原 block_id。
4. 如果 Custom Event 是用户手写目标，默认不接管 ownership。
```

### 替换策略

```text
1. 保留 UK2Node_CustomEvent 本体。
2. 保留事件名和参数 Pin。
3. 删除从该 Custom Event exec 输出可达的旧 body 节点。
4. 遇到其他 entry node、其他 owned block、未知共享节点时停止并 blocked。
5. 写入新 body。
6. 如果原目标是 owned block，则新节点继续写原 block_id。
7. 如果原目标是用户手写目标，则不写 block_id ownership；只写 transaction journal / review diff。
```

### 失败条件

```text
entry_identity_change_disallowed
signature_change_disallowed
affected_user_nodes_requires_confirmation
target_ambiguous
```

---

## 8.4 `event_body`

### 定位规则

```text
1. target.graph 必须是事件图 / Ubergraph。
2. selector.entry_name / node_path 必须唯一定位 UK2Node_Event 或兼容事件入口。
3. 不允许模糊按显示名替换多个同类事件。
```

### 替换策略

```text
1. 保留 UK2Node_Event 本体。
2. 保留事件身份、参数、绑定来源。
3. 删除该事件入口后方可达 body。
4. 不跨越其他 entry group。
5. 写入新 body。
```

### 特别限制

对 BeginPlay / Tick / InputAction / Overlap / Hit 等全局事件：

```text
Replace 可以在用户明确指定该事件时替换其 body。
但不得删除或重建事件入口本体。
不得改变事件签名。
不得自动接入其他执行流；接入已有执行链属于 Merge。
```

---

## 8.5 `function_definition`

该范围属于高风险。第一阶段不建议直接完整实现定义替换。

### 第一版策略

```text
1. 支持 resolve_target + dry_run。
2. 如果 external_dependents 存在，返回 dry_run blocked。
3. 如果请求会改变签名 / 函数名 / 外部调用身份，返回 blocked。
4. 正式写入默认不支持，返回 replace_scope_unsupported，除非后续单独实现 DefinitionReplace 子服务。
```

### 后续完整实现需要

```text
1. 查询外部引用。
2. 捕获函数定义 before snapshot。
3. 删除 / 重建 FunctionGraph 或重建函数元数据。
4. 修复调用方引用。
5. 编译全局依赖。
```

该能力不应混入第一版 body replace。

---

## 8.6 `event_definition`

同 `function_definition`，第一版建议：

```text
resolve + dry_run blocked / unsupported
```

原因：事件定义替换可能影响事件入口身份、外部绑定、引擎回调、输入绑定和编译引用。

正式支持应作为后续高风险迁移能力单独设计。

---

## 8.7 `graph`

### 定位规则

```text
target.graph 必须唯一。
```

### 第一版策略

```text
1. 只允许显式 graph scope。
2. 必须 dry_run。
3. Conservative 下不得自动执行。
4. 正式执行必须删除整个图表内目标范围节点并重建。
5. 如果图表内存在用户节点，必须 blocked 或要求更高 Profile / 用户确认。
```

### 实现建议

第一版可先实现 owned-only graph replace：

```text
图表内全部节点都是同一批 BlueprintHelper-owned 内容 → 可替换。
图表内存在 user / unknown 节点 → blocked。
```

---

## 9. Preflight / dry_run 设计

Replace 替换任何已有目标前都必须 dry_run。

### 9.1 Preflight 检查项

```text
1. payload schema 合法。
2. asset_path 存在且是 UBlueprint。
3. graph 存在且类型符合 replace_scope。
4. replace_scope 合法。
5. selector 能唯一定位目标。
6. body scope 不改变 entry identity。
7. body scope 不改变函数 / 事件签名。
8. block_implementation 目标必须是 owned block。
9. 用户手写目标默认不接管 ownership。
10. function_definition / event_definition 存在 external_dependents 时 blocked。
11. replacement 节点 kind 均有 handler。
12. replacement link 的 pin 可解析且类型兼容。
13. 不允许 replacement 隐式连接目标外用户节点。
14. 不允许 create_missing_variables。
15. 不允许 auto save / auto compile。
```

### 9.2 dry_run passed 返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "replace_blueprint_graph",
  "trace_id": "trace_xxx",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/.../BP_Target",
    "graph": "OpenPhysicsDoor",
    "replace_scope": "function_body"
  },
  "data": {
    "schema": "ReplaceBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

### 9.3 dry_run blocked 返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "replace_blueprint_graph",
  "trace_id": "trace_xxx",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "/Game/.../BP_Target",
    "graph": "OpenPhysicsDoor",
    "replace_scope": "function_definition"
  },
  "data": {
    "schema": "ReplaceBlueprintGraphDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": ["external_dependents_may_break"],
      "conflicts": [
        {
          "code": "external_dependents_may_break",
          "message": "Function definition replacement may break external callers."
        }
      ],
      "errors": []
    }
  }
}
```

### 9.4 dry_run 不返回项

```text
replace_plan
would_xxx
will_delete_nodes
will_create_nodes
affected_user_nodes
block_ref
transaction_id
ownership
review
safety
diagnostics
next
```

完整 replace plan 只能进入：

```text
Journal / Review / verbose / debug
```

---

## 10. GraphSnapshot / rollback_data

Replace 会删除旧节点，因此不能只依赖“创建失败后删除新节点”的简单 rollback。必须新增快照服务。

### 10.1 `FBlueprintHelperGraphSnapshotService`

```cpp
class FBlueprintHelperGraphSnapshotService
{
public:
    FBlueprintHelperGraphSnapshot CaptureTargetSnapshot(
        UBlueprint* Blueprint,
        UEdGraph* Graph,
        const TArray<UEdGraphNode*>& Nodes) const;

    bool RestoreTargetSnapshot(
        UBlueprint* Blueprint,
        UEdGraph* Graph,
        const FBlueprintHelperGraphSnapshot& Snapshot,
        FString& OutError) const;
};
```

### 10.2 Snapshot 内容

```text
graph_name
node_guid
node_class
node_title
node_position
pins
pin defaults
links
node metadata
node comment
entry identity
function signature summary
ownership summary
```

### 10.3 两种 rollback

| 场景 | 机制 |
|---|---|
| 写入中失败 | 优先用 `FBlueprintHelperScopedAssetMutation::Rollback()`。 |
| Review Reject / 后续 rollback_transaction | 使用 Journal 中的 rollback_data / GraphSnapshot。 |

### 10.4 运行时失败 rollback 规则

```text
preflight 失败：modified=false, rollback_result=not_needed。
写入中失败且 scoped rollback 成功：modified=false, rollback_result=rolled_back。
写入中失败且 rollback blocked / failed：modified=true, rollback_result=blocked / failed。
```

---

## 11. 正式写入流程

```text
1. ParseRequest
2. ResolveBlueprint
3. ResolveReplaceTarget
4. Preflight
5. 如果 dry_run=true：返回 ReplaceBlueprintGraphDryRun.v1
6. Capture before snapshot
7. Generate transaction_id
8. Begin FBlueprintHelperScopedAssetMutation
9. Modify Blueprint / Graph / old nodes
10. Delete old implementation body / target nodes
11. Build replacement nodes / links through GraphBuildService
12. Reconnect preserved entry / result / boundary nodes
13. 如果 owned block：写原 block_id ownership metadata
14. 写 Transaction Journal / Review rollback_data
15. MarkGraphChanged / MarkBlueprintAsStructurallyModified / MarkPackageDirty
16. Commit scoped mutation
17. 返回 ReplaceBlueprintGraph.v1 极简成功结果
```

### 11.1 删除旧实现

删除节点前必须：

```cpp
Mutation.Modify(Blueprint);
Mutation.Modify(Graph);
for (UEdGraphNode* Node : NodesToDelete)
{
    Mutation.Modify(Node);
}
```

删除方式建议：

```cpp
FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
```

如果具体节点类型不兼容，再退回：

```cpp
Node->DestroyNode();
```

删除后调用：

```cpp
Graph->NotifyGraphChanged();
FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
```

### 11.2 创建新实现

不要让 Replace 直接调用旧 `ImportAgentGraph`。应抽出：

```cpp
FBlueprintHelperGraphBuildService::BuildIntoGraph(...)
```

输入：

```cpp
struct FBlueprintHelperGraphBuildRequest
{
    UBlueprint* Blueprint = nullptr;
    UEdGraph* Graph = nullptr;
    TArray<FBlueprintHelperAgentImportNode> Nodes;
    TArray<FBlueprintHelperAgentImportLink> Links;
    bool bCreateMissingVariables = false;
    bool bStrict = true;
};
```

输出：

```cpp
struct FBlueprintHelperGraphBuildResult
{
    TMap<FString, UEdGraphNode*> IdToNode;
    TArray<UEdGraphNode*> CreatedNodes;
    TArray<FBlueprintHelperCreatedLinkRef> CreatedLinks;
    TArray<FBlueprintHelperGraphWriteIssue> Errors;
    TArray<FBlueprintHelperGraphWriteIssue> Conflicts;
};
```

Replace 中固定：

```text
bCreateMissingVariables=false
bStrict=true
```

---

## 12. ownership 处理

### 12.1 owned block 替换

```text
保留原 block_id。
replaced_ref.target_ref 返回原 block_ref。
新节点继续写原 block_id。
旧 block_id 不递增、不生成新 block_ref。
```

节点 metadata：

```text
BlueprintHelperOwned=true
BlueprintHelperBlockId=<original block_id>
BlueprintHelperTransactionId=<new transaction_id>
BlueprintHelperTool=ReplaceBlueprintGraph
BlueprintHelperFeatureName=<old or resolved feature>
```

NodeComment：

```text
[BlueprintHelper]
block_id=EG_PhysicsDoor_TogglePhysicsDoor0
tx=tx_20260503_1301
tool=ReplaceBlueprintGraph
```

### 12.2 用户手写目标替换

默认规则：

```text
不生成 block_id。
不接管 ownership。
不写 BlueprintHelperOwned metadata。
只写 Transaction Journal / Review diff / rollback_data。
```

如果未来支持“替换后交给 BlueprintHelper 管理”，必须单独参数和 dry_run 提示，不放入第一版默认路径。

---

## 13. Journal 记录

`ReplaceBlueprintGraph` 成功必须写 Journal。Journal 写失败不能报告成功。

### 13.1 Journal 最小字段

```json
{
  "schema": "BlueprintHelper.TransactionJournal.v1",
  "transaction_id": "tx_20260503_1301",
  "tool": "ReplaceBlueprintGraph",
  "status": "applied",
  "target_assets": ["/Game/.../BP_Target"],
  "replace_scope": "block_implementation",
  "target": {
    "asset_path": "/Game/.../BP_Target",
    "graph": "EG_PhysicsDoor",
    "target_ref": "TogglePhysicsDoor0",
    "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0"
  },
  "before_snapshot": {},
  "after_snapshot_summary": {},
  "rollback_data": {},
  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

### 13.2 Agent-facing 成功只返回

```json
"write_ref": {
  "transaction_id": "tx_20260503_1301",
  "journal_recorded": true
}
```

不返回：

```text
journal_path
review_status
rollback_data
summary
before / after
```

---

## 14. 成功结果构造

### 14.1 owned block 成功结果

```cpp
FBlueprintHelperReplaceGraphResultData Data;
Data.Schema = TEXT("ReplaceBlueprintGraph.v1");
Data.ReplaceResult.ReplacedRef.GraphId = Resolved.GraphId;
Data.ReplaceResult.ReplacedRef.TargetRef = Resolved.OriginalBlockRef;
Data.WriteRef.TransactionId = TransactionId;
Data.WriteRef.bJournalRecorded = true;
```

顶层 target：

```cpp
Target.AssetPath = AssetPath;
Target.Graph = GraphName;
Target.ReplaceScope = ReplaceScopeToString(Scope);
```

注意：顶层 target 不输出 `target_type`。

### 14.2 function_body 成功结果

```cpp
Data.ReplaceResult.ReplacedRef.GraphId = FunctionGraph->GetName();
Data.ReplaceResult.ReplacedRef.TargetRef = FunctionGraph->GetName();
```

### 14.3 event/custom_event body 成功结果

```cpp
Data.ReplaceResult.ReplacedRef.GraphId = Graph->GetName();
Data.ReplaceResult.ReplacedRef.TargetRef = EntryNameOrStableEntryRef;
```

---

## 15. 正式失败构造

### 15.1 preflight / resolve 失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "replace_blueprint_graph",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/.../BP_Target",
    "graph": "EG_PhysicsDoor",
    "replace_scope": "block_implementation"
  },
  "error": {
    "code": "target_block_not_found",
    "stage": "resolve_target",
    "message": "The requested BlueprintHelper-owned block was not found.",
    "retryable": false,
    "rollback_result": "not_needed"
  }
}
```

### 15.2 写入中失败并成功回滚

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "replace_blueprint_graph",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/.../BP_Target",
    "graph": "EG_PhysicsDoor",
    "replace_scope": "block_implementation"
  },
  "error": {
    "code": "link_create_failed",
    "stage": "create_links",
    "message": "Replacement graph links could not be created.",
    "retryable": false,
    "rollback_result": "rolled_back",
    "failed_item": {
      "type": "link",
      "ref": "links[4]"
    }
  }
}
```

### 15.3 rollback failed

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "replace_blueprint_graph",
  "status": "failed",
  "modified": true,
  "target": {
    "asset_path": "/Game/.../BP_Target",
    "graph": "EG_PhysicsDoor",
    "replace_scope": "block_implementation"
  },
  "error": {
    "code": "rollback_failed",
    "stage": "rollback",
    "message": "ReplaceBlueprintGraph failed and rollback could not restore the previous graph state.",
    "retryable": false,
    "rollback_result": "failed",
    "conflicts": [
      {
        "code": "asset_state_changed_during_write",
        "target": "/Game/.../BP_Target"
      }
    ]
  }
}
```

失败结果不得包含 `replace_result / write_ref / ownership / review / safety / diagnostics / next`。

---

## 16. Bridge Router 接入

### 16.1 `BlueprintHelperBridgeRouter.h`

增加前置声明：

```cpp
class FBlueprintHelperReplaceBlueprintGraphService;
```

构造函数增加依赖：

```cpp
const FBlueprintHelperReplaceBlueprintGraphService& InReplaceGraphService
```

成员：

```cpp
const FBlueprintHelperReplaceBlueprintGraphService& ReplaceGraphService;
```

处理函数：

```cpp
FBlueprintHelperBridgeResponse HandleReplaceBlueprintGraph(const FBlueprintHelperBridgeRequest& Req) const;
```

### 16.2 `BlueprintHelperBridgeRouter.cpp`

在命令分发中加入：

```cpp
if (Command == TEXT("replace_blueprint_graph"))
{
    return HandleReplaceBlueprintGraph(Request);
}
```

实现：

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReplaceBlueprintGraph(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result = ReplaceGraphService.Execute(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("replace_blueprint_graph failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

如果当前 BridgeResponse 对 `ok=false` 的业务失败统一走 `bSuccess=false`，先保持现有桥接语义；MCP 层仍应以 `structuredContent.ok/status/error` 为准。

---

## 17. RequestValidator 接入

### 17.1 写命令集合

在 `FBlueprintHelperRequestValidator::IsWriteCommand` 中加入：

```cpp
TEXT("replace_blueprint_graph")
```

### 17.2 payload 校验

新增校验：

```cpp
if (Command == TEXT("replace_blueprint_graph"))
{
    // target 必填 object
    // target.asset_path 必填 string
    // target.graph 必填 string
    // target.replace_scope 必填 string
    // selector 必填 object
    // replacement 必填 object
    // replacement.nodes 必填 array
    // replacement.links 可选 array
    // options.dry_run 可选 bool
}
```

Token / write permission 的底层校验继续复用现有写命令路径。

---

## 18. BlueprintHelper 模块初始化接入

### 18.1 `BlueprintHelper.h`

增加：

```cpp
class FBlueprintHelperReplaceBlueprintGraphService;
TUniquePtr<FBlueprintHelperReplaceBlueprintGraphService> ReplaceGraphService;
```

### 18.2 `BlueprintHelper.cpp`

初始化顺序建议：

```cpp
GraphBuildService = MakeUnique<FBlueprintHelperGraphBuildService>(...);
GraphSnapshotService = MakeUnique<FBlueprintHelperGraphSnapshotService>(...);
ReplaceGraphService = MakeUnique<FBlueprintHelperReplaceBlueprintGraphService>(
    *Resolver,
    *GraphBuildService,
    *GraphSnapshotService,
    *BlockIdService,
    *OwnershipService,
    *TransactionJournalService);
```

Router 构造时传入：

```cpp
*ReplaceGraphService
```

释放时按反向顺序 `Reset()`。

---

## 19. Build.cs 检查

Replace 需要以下 UE API：

```cpp
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
```

`BlueprintHelper.Build.cs` 应确保包含：

```text
Core
CoreUObject
Engine
UnrealEd
BlueprintGraph
Kismet
GraphEditor
Json
JsonUtilities
```

如果编译期找不到 `UK2Node_FunctionEntry / UK2Node_FunctionResult / UK2Node_CustomEvent`，优先确认 `BlueprintGraph` 在依赖列表中。

---

## 20. 实现提交顺序

### Commit 1：Replace 类型与序列化

```text
Add ReplaceBlueprintGraph result and dry_run C++ types
Add EBlueprintHelperReplaceScope
Add EBlueprintHelperReplaceErrorCode
Add EBlueprintHelperGraphWriteStage if not already shared
Add replaced_ref JSON serialization
```

验收：

```text
能构造 ReplaceBlueprintGraph.v1 成功 JSON。
能构造 ReplaceBlueprintGraphDryRun.v1 dry_run JSON。
成功 JSON 不出现 target_type / target_kind / summary / ownership / review / safety。
```

### Commit 2：GraphSnapshotService

```text
Add FBlueprintHelperGraphSnapshotService
Capture target node snapshots
Capture pin defaults and links
Capture ownership metadata and comments
Prepare rollback_data JSON for Journal
```

验收：

```text
能对 owned block / function_body / custom_event_body 捕获 before snapshot。
Snapshot 不进入 Agent-facing 成功结果。
```

### Commit 3：GraphBuildService 抽取

```text
Extract reusable build core from AgentImportService
Keep import_agent_graph behavior unchanged
GraphBuildService supports build into existing graph
GraphBuildService supports strict failure and created-node tracking
```

验收：

```text
旧 AgentImportGraph 测试不退化。
Replace 可以通过 GraphBuildService 创建 custom_event/call/branch/sequence/comment 等节点。
```

### Commit 4：ReplaceTargetResolver

```text
Resolve block_implementation by block_id / target_ref
Resolve function_body by FunctionGraph
Resolve custom_event_body by entry_name / node_path
Resolve event_body by entry_name / node_path
Reject unsupported definition scopes for first write phase
```

验收：

```text
target_block_not_found / target_not_owned / target_ambiguous 能正确返回。
function_body 能找到 FunctionEntry。
custom_event_body 能唯一定位 Custom Event。
```

### Commit 5：dry_run 极简返回

```text
Implement Replace dry_run passed
Implement Replace dry_run blocked
Hide replace_plan / would_xxx from Agent-facing result
Keep internal plan for Journal / debug only
```

验收：

```text
dry_run passed 只返回 result/can_execute。
dry_run blocked 只返回 blocked_by/conflicts/errors。
dry_run 不修改 Blueprint。
```

### Commit 6：block_implementation 正式写入

```text
Delete old owned block nodes
Build replacement nodes and links
Preserve original block_id / block_ref
Write ownership metadata and NodeComment
Write Journal
Return replaced_ref + write_ref + validation
```

验收：

```text
owned block 替换后 block_id 不变。
replaced_ref.target_ref 是原 block_ref。
成功结果不返回 summary / ownership。
Journal 写失败时整体失败并 rollback。
```

### Commit 7：function_body 正式写入

```text
Preserve FunctionEntry
Preserve or validate FunctionResult
Delete internal body nodes
Build replacement implementation
Validate signature unchanged
Write Journal
Return replaced_ref
```

验收：

```text
函数名、参数、返回签名不变。
FunctionEntry 不被删除。
签名变化返回 signature_change_disallowed。
```

### Commit 8：custom_event_body / event_body 正式写入

```text
Preserve event/custom-event entry node
Delete reachable body nodes only
Do not cross into other entry groups
Build replacement body
Owned target keeps original block_id
User target does not receive ownership
```

验收：

```text
Custom Event 本体不变。
事件参数不变。
不会删除其他 entry group。
用户手写目标默认不写 owned metadata。
```

### Commit 9：graph scope / definition scope 策略

```text
Add graph scope preflight
Allow owned-only graph replace or return blocked for user content
Return replace_scope_unsupported for function_definition / event_definition formal write until dedicated migration service exists
```

验收：

```text
Conservative 下 graph scope 不自动执行。
function_definition 有 external_dependents 时 dry_run blocked。
正式写 definition scope 不静默执行危险迁移。
```

### Commit 10：Bridge / Validator / Module wiring

```text
Register replace_blueprint_graph command
Add write-command token gate
Add router handler
Wire service in module startup
Add automation tests
```

验收：

```text
未授权写调用被拒绝。
replace_blueprint_graph 能通过 Bridge 返回 ToolResultBase。
```

---

## 21. 自动化测试计划

新增文件：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperReplaceGraphContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperReplaceGraphDryRunTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperReplaceGraphWriteTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperReplaceGraphRollbackTests.cpp
```

### 21.1 Contract Tests

```text
replace_success_contract_owned_block
replace_success_contract_function_body
replace_dry_run_passed_contract
replace_dry_run_blocked_contract
replace_failed_contract
```

检查：

```text
operation=replace_blueprint_graph
data.schema=ReplaceBlueprintGraph.v1
dry_run schema=ReplaceBlueprintGraphDryRun.v1
顶层 target 不含 target_type
成功 data.replace_result.replaced_ref 存在
成功不含 summary / target_kind / ownership / review / safety / diagnostics
失败不含 replace_result / write_ref
```

### 21.2 Target Resolver Tests

```text
resolve_owned_block_by_block_id
resolve_owned_block_by_graph_and_target_ref
resolve_function_body_graph
resolve_custom_event_body_by_entry_name
resolve_event_body_by_node_path
reject_ambiguous_target
reject_target_not_owned_for_block_implementation
```

### 21.3 DryRun Tests

```text
dry_run_block_implementation_passed
dry_run_function_body_signature_change_blocked
dry_run_function_definition_external_dependents_blocked
dry_run_graph_user_nodes_blocked
dry_run_target_not_found_failed_or_blocked
```

### 21.4 Write Tests

```text
replace_owned_block_preserves_block_id
replace_owned_block_returns_original_block_ref
replace_function_body_preserves_function_entry
replace_function_body_preserves_signature
replace_custom_event_body_preserves_entry_node
replace_user_custom_event_body_does_not_write_ownership
replace_writes_journal_before_success
```

### 21.5 Rollback Tests

```text
rollback_on_node_create_failed
rollback_on_link_create_failed
rollback_on_ownership_write_failed
rollback_on_journal_write_failed
rollback_failed_sets_modified_true
```

---

## 22. 第一版明确不做

```text
1. 不用 Replace 追加新逻辑块。
2. 不用 Replace 接入已有执行流。
3. 不用 Replace 精确改单个 Pin / 默认值 / Link。
4. 不模糊查找同名目标。
5. 不自动接管用户手写目标 ownership。
6. 不返回 replace_plan / summary / diff / node counts。
7. 不返回 review / safety / diagnostics / rollback_data。
8. 不自动 save。
9. 不自动 compile。
10. 不在 function_definition / event_definition 中静默执行高风险迁移。
```

---

## 23. 最小验收标准

完成后，owned block 替换成功必须返回：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "replace_blueprint_graph",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "replace_scope": "block_implementation"
  },
  "data": {
    "schema": "ReplaceBlueprintGraph.v1",
    "replace_result": {
      "replaced_ref": {
        "graph_id": "EG_PhysicsDoor",
        "target_ref": "TogglePhysicsDoor0"
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
target_kind
summary
replace_plan
before
after
full_diff
deleted_nodes / created_nodes / modified_nodes
ownership
review
safety
diagnostics
next
journal_path
rollback_data
```

这是 ReplaceBlueprintGraph UE 侧 C++ 实现是否合格的核心判定。
