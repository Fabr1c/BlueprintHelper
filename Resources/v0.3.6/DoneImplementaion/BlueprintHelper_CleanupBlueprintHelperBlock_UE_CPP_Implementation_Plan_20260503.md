# BlueprintHelper CleanupBlueprintHelperBlock UE 侧 C++ 可执行实现计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_CleanupBlueprintHelperBlock_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++  
状态：可执行实现计划

---

## 0. 实现目标

本计划用于把 `CleanupBlueprintHelperBlock` 落地为 UE 插件侧可执行 C++ 工具。

工具目标：

```text
删除一个明确的 BlueprintHelper-owned block。
```

它只接受明确目标：

```text
1. block_id
2. asset_path + graph_id/graph + block_ref
```

不接受：

```text
entry_name
display_name
custom_event name
模糊名称
按图表批量删除
按功能名批量删除
```

这些属于后续 `CleanupBlueprintHelperFeature`。

Agent-facing 返回必须遵守字段确认稿：

```text
1. operation = cleanup_blueprint_helper_block
2. 顶层 target 不返回 target_type
3. 成功返回 data.cleanup_result.cleaned_ref
4. 成功不返回 deleted_nodes / deleted_links / summary
5. 成功返回 data.write_ref.transaction_id / journal_recorded
6. missing_policy=ignore 且目标缺失时返回 status=no_op / modified=false / missing=true
7. dry_run passed 极简
8. dry_run blocked 返回 blocked_by / conflicts / errors
9. ownership 冲突必须失败，不能清理非 owned 内容
10. rollback blocked / failed 时允许 modified=true，并要求 Agent stop_and_report
```

---

## 1. 当前实现基线判断

基于前序 Graph Write 计划，UE 侧应已经或即将具备以下可复用基础：

```text
FBlueprintHelperToolResultBase
FBlueprintHelperToolResultBuilder
FBlueprintHelperGraphResolver
FBlueprintHelperScopedAssetMutation
FBlueprintHelperOwnershipService
FBlueprintHelperTransactionJournalService
FBlueprintHelperWriteRef
FBlueprintHelperFailedItem
FBlueprintHelperConflictItem
EBlueprintHelperRollbackResult
```

`CleanupBlueprintHelperBlock` 不应复用 Append / Replace / Patch / Merge 的业务执行逻辑，但应复用：

```text
1. ToolResultBase 序列化
2. write_ref 序列化
3. error 序列化
4. target 序列化
5. Journal / Review 写入
6. rollback_data 结构
7. ownership metadata 扫描
8. LogicJson node_ref / link_ref 辅助记录
```

---

## 2. 新增文件

建议新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperCleanupBlockTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperCleanupBlockTypes.cpp

Source/BlueprintHelper/Public/Services/BlueprintHelperCleanupBlueprintHelperBlockService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperCleanupBlueprintHelperBlockService.cpp
```

如果 Cleanup 后续要扩展 Feature Cleanup，可预留统一目录：

```text
Source/BlueprintHelper/Public/Services/Cleanup/
Source/BlueprintHelper/Private/Services/Cleanup/
```

但第一版不强制拆目录。

---

## 3. 新增枚举

### 3.1 cleanup_scope

```cpp
enum class EBlueprintHelperCleanupScope : uint8
{
    Block
};
```

第一版固定输出：

```json
"cleanup_scope": "block"
```

### 3.2 missing_policy

```cpp
enum class EBlueprintHelperMissingPolicy : uint8
{
    Error,
    Ignore
};
```

默认：

```text
Error
```

### 3.3 cleanup stage

```cpp
enum class EBlueprintHelperCleanupStage : uint8
{
    ParseInput,
    ResolveTarget,
    ResolveGraph,
    ResolveBlock,
    OwnershipCheck,
    DependencyCheck,
    DryRun,
    Snapshot,
    DeleteLinks,
    DeleteNodes,
    WriteJournal,
    Rollback
};
```

输出字符串建议：

```text
parse_input
resolve_target
resolve_graph
resolve_block
ownership_check
dependency_check
dry_run
snapshot
delete_links
delete_nodes
write_journal
rollback
```

### 3.4 cleanup error code

```cpp
enum class EBlueprintHelperCleanupErrorCode : uint8
{
    InvalidRequest,
    TargetBlueprintNotFound,
    TargetNotBlueprint,
    TargetGraphNotFound,
    BlockNotFound,
    TargetNotOwned,
    OwnershipMismatch,
    ExternalDependentsExist,
    NodeDeleteFailed,
    LinkDeleteFailed,
    JournalWriteFailed,
    RollbackBlocked,
    RollbackFailed,
    WritePermissionDisabled,
    ProfilePolicyViolation,
    BridgeDisconnected
};
```

输出字符串建议：

```text
invalid_request
target_blueprint_not_found
target_not_blueprint
target_graph_not_found
block_not_found
target_not_owned
ownership_mismatch
external_dependents_exist
node_delete_failed
link_delete_failed
journal_write_failed
rollback_blocked
rollback_failed
write_permission_disabled
profile_policy_violation
bridge_disconnected
```

---

## 4. UE 侧结果结构体

字段稿建议结构如下，第一版按此实现。

```cpp
struct FBlueprintHelperCleanedRef
{
    FString GraphId;
    FString BlockRef;

    // fallback only when graph_id + block_ref cannot be safely derived
    FString BlockId;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperCleanupBlockResult
{
    FBlueprintHelperCleanedRef CleanedRef;

    // only for no_op when missing_policy=ignore
    TOptional<FString> MissingPolicy;
    TOptional<bool> bMissing;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperCleanupBlockResultData
{
    FString Schema = TEXT("CleanupBlueprintHelperBlock.v1");
    FBlueprintHelperCleanupBlockResult CleanupResult;
    TOptional<FBlueprintHelperWriteRef> WriteRef;

    TSharedRef<FJsonObject> ToJson() const;
};
```

规则：

```text
1. applied 成功必须包含 write_ref。
2. missing_policy=ignore 且目标缺失导致 no_op 时，不生成 transaction_id，不返回 write_ref。
3. no_op 返回 cleanup_result.missing_policy / cleanup_result.missing。
4. 成功默认使用 graph_id + block_ref。
5. 只有无法安全拆分完整 block_id 时，才 fallback 返回 cleaned_ref.block_id。
```

---

## 5. 输入请求结构

建议第一版输入支持两种目标形式。

### 5.1 完整 block_id 输入

```json
{
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0",
  "missing_policy": "error",
  "dry_run": false
}
```

### 5.2 压缩引用输入

```json
{
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "graph": "EG_PhysicsDoor",
  "block_ref": "TogglePhysicsDoor0",
  "missing_policy": "error",
  "dry_run": false
}
```

### 5.3 C++ request

```cpp
struct FBlueprintHelperCleanupBlockRequest
{
    FString AssetPath;
    FString GraphName;
    FString GraphId;
    FString BlockRef;
    FString BlockId;
    EBlueprintHelperMissingPolicy MissingPolicy = EBlueprintHelperMissingPolicy::Error;
    bool bDryRun = false;

    bool HasFullBlockId() const;
    bool HasCompressedRef() const;
    FString GetEffectiveBlockId() const;
};
```

`GetEffectiveBlockId()` 规则：

```text
if block_id provided:
    use block_id

else if graph or graph_id + block_ref provided:
    full_block_id = graph_id_or_graph + "_" + block_ref

else:
    invalid_request
```

---

## 6. target 序列化规则

Agent-facing `target` 只表达执行路由范围。

```cpp
TSharedRef<FJsonObject> MakeCleanupBlockTargetJson(
    const FBlueprintHelperCleanupBlockRequest& Req,
    const FBlueprintHelperResolvedCleanupBlockTarget& Target)
{
    // asset_path
    // graph
    // cleanup_scope = "block"
    // block_ref if known
    // block_id only as input compatibility / fallback when block_ref cannot be derived
}
```

正式成功推荐输出：

```json
"target": {
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "graph": "EG_PhysicsDoor",
  "cleanup_scope": "block",
  "block_ref": "TogglePhysicsDoor0"
}
```

不返回：

```text
target_type
deleted_nodes
deleted_links
ownership
review
safety
diagnostics
next
```

---

## 7. Block 定位服务

建议在 `FBlueprintHelperOwnershipService` 中补充只读扫描能力，或者新增：

```text
FBlueprintHelperOwnedBlockIndexService
```

### 7.1 新增结构

```cpp
struct FBlueprintHelperOwnedBlockNodeRef
{
    TWeakObjectPtr<UEdGraphNode> Node;
    FString NodeRef;
    FString NodePath;
};

struct FBlueprintHelperResolvedCleanupBlockTarget
{
    UBlueprint* Blueprint = nullptr;
    UEdGraph* Graph = nullptr;

    FString AssetPath;
    FString GraphId;
    FString GraphName;
    FString BlockRef;
    FString BlockId;

    TArray<UEdGraphNode*> OwnedNodes;
    TArray<UEdGraphNode*> NonOwnedConflictingNodes;

    TArray<UEdGraphPin*> InternalPins;
    TArray<FBlueprintHelperLinkSnapshot> InternalLinks;
    TArray<FBlueprintHelperLinkSnapshot> ExternalIncomingLinks;
    TArray<FBlueprintHelperLinkSnapshot> ExternalOutgoingLinks;

    bool bFound = false;
    bool bOwned = false;
    bool bHasExternalDependents = false;
};
```

### 7.2 定位流程

```text
1. Resolve Blueprint by asset_path.
2. Resolve target graph:
   - if graph provided, search by graph name.
   - if only block_id provided, scan all graphs for nodes with matching metadata.
3. Resolve block:
   - scan graph nodes metadata: BlueprintHelperBlockId == full_block_id.
   - collect all matching nodes.
4. If no node found:
   - missing_policy=error → failed block_not_found.
   - missing_policy=ignore → no_op missing=true.
5. Ownership check:
   - every node in target block must have BlueprintHelperOwned=true.
   - every node in target block must have matching BlueprintHelperBlockId.
   - mismatch → target_not_owned / ownership_mismatch.
6. Link scan:
   - internal links: source and target both in block.
   - external incoming links: outside node -> block node.
   - external outgoing links: block node -> outside node.
7. External dependents:
   - external incoming links normally means outside logic depends on this block.
   - for explicit call-node references, scan call sites if possible.
```

### 7.3 ownership 冲突规则

任何 ownership 冲突必须失败：

```text
code = target_not_owned
stage = ownership_check
rollback_result = not_needed
modified = false
```

不要尝试删除来源不明节点，也不要只删除部分 owned 节点。

---

## 8. 依赖检查

CleanupBlock 应只删除明确 owned block，但删除仍可能破坏外部逻辑。

第一版阻断条件建议：

```text
1. external incoming exec/data links exist.
2. outside nodes call a Custom Event / Function entry that belongs to this block.
3. outside nodes use variables / temporaries produced only by this block.
```

字段稿的 dry_run blocked 示例使用：

```text
external_dependents_exist
```

实现策略：

```cpp
if (Resolved.ExternalIncomingLinks.Num() > 0 || Resolved.bHasExternalDependents)
{
    BlockedBy.Add(TEXT("external_dependents_exist"));
}
```

注意：

```text
external_dependencies（block 依赖外部内容）不是默认阻断。
external_dependents（外部依赖 block）默认阻断。
```

正式执行前即使 dry_run 已 passed，也必须重新依赖检查，避免 TOCTOU。

---

## 9. dry_run 实现

CleanupBlock 必须支持 dry_run。

### 9.1 dry_run passed

返回：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "cleanup_blueprint_helper_block",
  "status": "dry_run",
  "modified": false,
  "target": {
    "asset_path": "...",
    "graph": "EG_PhysicsDoor",
    "cleanup_scope": "block",
    "block_ref": "TogglePhysicsDoor0"
  },
  "data": {
    "schema": "CleanupBlueprintHelperBlockDryRun.v1",
    "dry_run": {
      "result": "passed",
      "can_execute": true
    }
  }
}
```

### 9.2 dry_run blocked

返回：

```json
{
  "data": {
    "schema": "CleanupBlueprintHelperBlockDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": [
        "external_dependents_exist"
      ],
      "conflicts": [
        {
          "code": "external_dependents_exist",
          "message": "The target block is referenced by external logic."
        }
      ],
      "errors": []
    }
  }
}
```

不得返回：

```text
would_delete_nodes
would_delete_links
will_keep
plan
ownership
review
safety
diagnostics
next
```

### 9.3 dry_run no_op

如果 `missing_policy=ignore` 且 block 缺失，建议不返回 dry_run passed，而返回 no_op：

```text
ok=true
status=no_op
modified=false
data.schema=CleanupBlueprintHelperBlock.v1
cleanup_result.missing_policy=ignore
cleanup_result.missing=true
validation.should_compile=false
validation.should_save=false
```

原因：

```text
目标缺失不是可执行的 cleanup 预演，而是恢复/重复清理场景的幂等 no_op。
```

---

## 10. 正式执行流程

正式执行必须走事务式删除。

```text
1. Parse request.
2. Resolve Blueprint.
3. Resolve block.
4. missing_policy=ignore + missing → no_op.
5. Ownership check.
6. Dependency check.
7. Begin FBlueprintHelperScopedAssetMutation.
8. Snapshot before state:
   - nodes
   - pins
   - links
   - node metadata
   - node comments
   - graph membership
9. Break links:
   - internal links
   - allowed outgoing links
   - any links attached to owned nodes
10. Delete owned nodes.
11. Mark Blueprint structurally modified.
12. Write Journal / Review record.
13. Return applied success.
```

---

## 11. 删除实现细节

### 11.1 link 删除

UE 图表连接通常可通过 pin links 操作：

```cpp
void BreakPinLink(UEdGraphPin* A, UEdGraphPin* B)
{
    if (!A || !B)
    {
        return;
    }

    A->BreakLinkTo(B);
}
```

或通过 schema：

```cpp
const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
Schema->BreakPinLinks(*Pin, true);
```

建议：

```text
1. 先记录所有 links snapshot。
2. 对每个 owned node 的每个 pin 调用 BreakAllPinLinks。
3. 删除节点前确认 owned node 不再有外部 pin link。
```

### 11.2 node 删除

推荐使用 Blueprint editor utils：

```cpp
FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
```

或：

```cpp
Graph->RemoveNode(Node);
```

优先使用 `FBlueprintEditorUtils::RemoveNode`，因为它会处理蓝图图表内部维护逻辑。

### 11.3 graph 修改标记

```cpp
FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
Blueprint->MarkPackageDirty();
```

不在 CleanupBlock 内自动 compile/save。只返回：

```json
"validation": {
  "should_compile": true,
  "should_save": true,
  "compiled": false,
  "saved": false
}
```

---

## 12. rollback_data

Cleanup 是破坏性写入，必须在 Journal 中记录完整 rollback_data。

建议记录：

```json
{
  "rollback_data": {
    "type": "cleanup_blueprint_helper_block",
    "asset_path": "...",
    "graph": "EG_PhysicsDoor",
    "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0",
    "nodes": [
      {
        "node_guid": "...",
        "node_class": "...",
        "node_title": "...",
        "position": [0, 0],
        "metadata": {},
        "node_comment": "...",
        "raw_node_snapshot_ref": "..."
      }
    ],
    "links": [
      {
        "from_node_guid": "...",
        "from_pin_guid": "...",
        "to_node_guid": "...",
        "to_pin_guid": "..."
      }
    ]
  }
}
```

Agent-facing 返回不得包含 rollback_data。

---

## 13. Journal / Review 写入

正式 applied 成功前必须写入 Journal。

Journal record 建议包含：

```text
schema = BlueprintHelper.TransactionJournal.v1
transaction_id
tool = CleanupBlueprintHelperBlock
status = applied
target_assets
target.graph
target.block_id
target.block_ref
deleted_nodes
deleted_links
external_dependencies
external_dependents
diff_summary
rollback_data
validation
review_status = pending_review
```

成功返回只暴露：

```json
"write_ref": {
  "transaction_id": "tx_...",
  "journal_recorded": true
}
```

Journal 写入失败：

```text
error.code = journal_write_failed
stage = write_journal
rollback_result = rolled_back / blocked / failed
不返回 cleanup_result
不返回 write_ref
```

---

## 14. no_op 行为

`missing_policy=ignore` 且 block 缺失时：

```text
ok=true
status=no_op
modified=false
data.schema=CleanupBlueprintHelperBlock.v1
data.cleanup_result.cleaned_ref = graph_id + block_ref 或 block_id fallback
data.cleanup_result.missing_policy = ignore
data.cleanup_result.missing = true
validation.should_compile=false
validation.should_save=false
不生成 transaction_id
不写 Journal
不返回 write_ref
```

如果未来要记录“已经缺失”的审计信息，应作为 Journal maintenance / recovery 逻辑处理，不混入 Agent-facing CleanupBlock no_op 成功结果。

---

## 15. 成功返回

正式 applied 成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "cleanup_blueprint_helper_block",
  "trace_id": "trace_20260503_1601",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "cleanup_scope": "block",
    "block_ref": "TogglePhysicsDoor0"
  },
  "data": {
    "schema": "CleanupBlueprintHelperBlock.v1",
    "cleanup_result": {
      "cleaned_ref": {
        "graph_id": "EG_PhysicsDoor",
        "block_ref": "TogglePhysicsDoor0"
      }
    },
    "write_ref": {
      "transaction_id": "tx_20260503_1601",
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

不得返回：

```text
deleted_nodes
deleted_links
deleted_node_count
deleted_link_count
summary
external_dependencies
external_dependents
ownership
review
safety
diagnostics
next
rollback_data
journal_path
```

---

## 16. 正式失败

失败只返回 `error`，不返回 `cleanup_result` 或 `write_ref`。

### 16.1 block_not_found

```text
ok=false
status=failed
modified=false
error.code=block_not_found
error.stage=resolve_target
error.rollback_result=not_needed
```

### 16.2 target_not_owned

```text
ok=false
status=failed
modified=false
error.code=target_not_owned
error.stage=ownership_check
error.rollback_result=not_needed
conflicts[].code=ownership_mismatch
```

### 16.3 node_delete_failed

```text
ok=false
status=failed
modified=false
error.code=node_delete_failed
error.stage=delete_nodes
error.rollback_result=rolled_back
failed_item.type=node
```

### 16.4 rollback_failed / rollback_blocked

```text
ok=false
status=failed
modified=true
error.code=rollback_failed 或 rollback_blocked
error.stage=rollback
error.rollback_result=failed 或 blocked
```

Agent 后续必须 stop_and_report，不得继续 compile/save/patch/merge/replace。

---

## 17. Bridge Router 接入

### 17.1 command 注册

在 `BlueprintHelperBridgeRouter` 增加：

```cpp
if (Request.Command == TEXT("cleanup_blueprint_helper_block"))
{
    return HandleCleanupBlueprintHelperBlock(Request);
}
```

### 17.2 handler

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleCleanupBlueprintHelperBlock(
    const FBlueprintHelperBridgeRequest& Request)
{
    FBlueprintHelperToolResultBase Result =
        CleanupBlockService.Execute(Request.Payload);

    FBlueprintHelperBridgeResponse Response = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Request.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Request.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet()
                ? Result.Error->Message
                : TEXT("CleanupBlueprintHelperBlock failed"));

    Response.Result = Result.ToJson();
    return Response;
}
```

### 17.3 写权限

加入写命令集合：

```cpp
TEXT("cleanup_blueprint_helper_block")
```

dry_run 虽不修改资产，但可以沿用同一个 command，并通过 payload `dry_run=true` 进入预演路径。

如果当前 write permission disabled：

```text
正式写入：write_permission_disabled
dry_run：可允许，具体按 Safety Profile / runtime profile 决定
```

---

## 18. Request Validator 接入

在 `BlueprintHelperRequestValidator` 增加：

```cpp
if (Command == TEXT("cleanup_blueprint_helper_block"))
{
    // Required:
    // asset_path
    //
    // One of:
    // block_id
    // OR graph + block_ref
    //
    // Optional:
    // missing_policy
    // dry_run
}
```

校验规则：

```text
1. asset_path 必填。
2. block_id 与 graph+block_ref 二选一。
3. block_id 和 graph+block_ref 同时出现时，必须一致，否则 invalid_request。
4. missing_policy 只能是 error / ignore，默认 error。
5. cleanup_scope 如果输入层出现，只能是 block。
```

---

## 19. 与 Existing Services 的关系

### 19.1 与 Append / Replace / Patch / Merge

```text
Append / Replace / Patch / Merge 写入 owned block。
CleanupBlock 只删除 owned block。
```

CleanupBlock 不应依赖这些工具的返回结果，而应依赖：

```text
1. node metadata
2. Journal block record
3. graph scan
```

### 19.2 与 CleanupFeature

`CleanupBlueprintHelperFeature` 后续可以复用：

```text
FBlueprintHelperOwnedBlockIndexService
FBlueprintHelperCleanupDeletionExecutor
FBlueprintHelperCleanupJournalBuilder
```

但第一版 `CleanupBlock` 不接受多字段 fuzzy match。

### 19.3 与 Review Reject

Review Reject 可调用 rollback_transaction，而不是直接调用 CleanupBlock。

CleanupBlock 生成的 transaction 被用户 Reject 时，由 Review/rollback 系统恢复已删除 block。

---

## 20. 内部类拆分建议

建议不要把所有逻辑塞进一个 Service。

```cpp
class FBlueprintHelperCleanupBlueprintHelperBlockService
{
public:
    FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload);
};

class FBlueprintHelperOwnedBlockResolver
{
public:
    EBlueprintHelperResolveResult Resolve(
        const FBlueprintHelperCleanupBlockRequest& Request,
        FBlueprintHelperResolvedCleanupBlockTarget& OutTarget,
        FBlueprintHelperToolError& OutError);
};

class FBlueprintHelperCleanupDependencyAnalyzer
{
public:
    FBlueprintHelperCleanupDependencyResult Analyze(
        const FBlueprintHelperResolvedCleanupBlockTarget& Target);
};

class FBlueprintHelperCleanupDeletionExecutor
{
public:
    bool DeleteBlock(
        const FBlueprintHelperResolvedCleanupBlockTarget& Target,
        FBlueprintHelperCleanupDeletionResult& OutResult,
        FBlueprintHelperToolError& OutError);
};

class FBlueprintHelperCleanupJournalBuilder
{
public:
    FBlueprintHelperTransactionJournalRecord BuildBlockCleanupJournal(...);
};
```

这样 `CleanupFeature` 后续可以复用 resolver / analyzer / executor。

---

## 21. 测试计划

新增测试文件：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperCleanupBlockContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperCleanupBlockResolveTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperCleanupBlockWriteTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperCleanupBlockRollbackTests.cpp
```

### 21.1 Contract 测试

```text
1. applied_result_minimal_contract
   - 包含 data.cleanup_result.cleaned_ref
   - 包含 data.write_ref
   - 不包含 deleted_nodes / deleted_links / summary / ownership / review / safety

2. no_op_missing_ignore_contract
   - status=no_op
   - modified=false
   - cleanup_result.missing_policy=ignore
   - cleanup_result.missing=true
   - 不包含 write_ref

3. dry_run_passed_contract
   - status=dry_run
   - data.schema=CleanupBlueprintHelperBlockDryRun.v1
   - dry_run.result=passed
   - dry_run.can_execute=true
   - 不包含 would_delete_nodes / would_delete_links

4. dry_run_blocked_contract
   - blocked_by / conflicts / errors
```

### 21.2 Resolve 测试

```text
1. resolve_by_full_block_id
2. resolve_by_graph_and_block_ref
3. block_id_and_graph_block_ref_mismatch
4. block_not_found_error_policy
5. block_not_found_ignore_policy
6. target_graph_not_found
```

### 21.3 Ownership 测试

```text
1. rejects_non_owned_nodes
2. rejects_mixed_block_nodes
3. rejects_nodes_with_missing_block_id_metadata
4. rejects_block_id_metadata_mismatch
```

### 21.4 Dependency 测试

```text
1. dry_run_blocks_external_dependents
2. allows_external_dependencies
3. blocks_external_incoming_exec_link
4. blocks_external_incoming_data_link
```

### 21.5 Write 测试

```text
1. deletes_owned_block_nodes
2. deletes_owned_block_links
3. preserves_other_blocks_in_same_graph
4. preserves_user_nodes
5. writes_journal_before_success
6. returns_write_ref_only_on_applied
```

### 21.6 Rollback 测试

```text
1. rollback_on_node_delete_failed
2. rollback_on_link_delete_failed
3. rollback_on_journal_write_failed
4. rollback_failed_sets_modified_true
```

---

## 22. 推荐提交顺序

### Commit 1：类型与返回契约

```text
Add CleanupBlueprintHelperBlock result and dry_run types
Add cleanup enums
Add cleaned_ref serializer
Add no_op serializer
```

验收：

```text
可以生成 applied / no_op / dry_run / failed JSON。
字段契约与确认稿一致。
```

### Commit 2：request parser 与 validator

```text
Parse asset_path / block_id / graph / block_ref / missing_policy / dry_run
Validate block target ambiguity
Register cleanup_blueprint_helper_block command
```

验收：

```text
block_id 和 graph+block_ref 二选一。
missing_policy 默认 error。
非法 missing_policy 返回 invalid_request。
```

### Commit 3：Owned block resolver

```text
Implement graph scan by BlueprintHelperBlockId metadata
Resolve full block_id and compressed ref
Return no_op for missing_policy=ignore
Reject target_not_owned
```

验收：

```text
能按 full block_id 和 graph+block_ref 找到 owned nodes。
非 owned 节点不会被清理。
```

### Commit 4：dependency analyzer and dry_run

```text
Scan internal/external links
Detect external dependents
Implement dry_run passed / blocked
```

验收：

```text
外部 incoming link blocked。
dry_run 不返回 would_delete_nodes / plan。
```

### Commit 5：deletion executor

```text
Snapshot block nodes and links
Break links
Delete nodes
Mark blueprint structurally modified
Handle delete failure and rollback
```

验收：

```text
删除只影响目标 block。
同图其他 block 和用户节点保留。
失败可回滚。
```

### Commit 6：Journal / Review

```text
Build cleanup journal record
Write rollback_data
Write review entry
Return write_ref only after journal success
```

验收：

```text
Journal 写失败不返回成功。
成功返回 transaction_id / journal_recorded。
```

### Commit 7：full regression tests

```text
Add automation tests
Add contract snapshot tests
Add rollback tests
```

验收：

```text
所有 CleanupBlock 合同测试通过。
Append/Replace/Patch/Merge 既有测试不退化。
```

---

## 23. 第一版不做的内容

```text
1. 不按 feature_name 清理。
2. 不按 entry_name / display_name 清理。
3. 不模糊匹配 block。
4. 不删除用户节点。
5. 不清理混合 ownership block。
6. 不返回 deleted_nodes / deleted_links / summary。
7. 不返回 review / safety / ownership。
8. 不在 no_op 中生成 transaction_id。
9. 不自动 cascade 删除依赖该 block 的外部逻辑。
10. 不自动 compile/save。
```

---

## 24. 最小验收标准

### 24.1 applied

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "cleanup_blueprint_helper_block",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "cleanup_scope": "block",
    "block_ref": "TogglePhysicsDoor0"
  },
  "data": {
    "schema": "CleanupBlueprintHelperBlock.v1",
    "cleanup_result": {
      "cleaned_ref": {
        "graph_id": "EG_PhysicsDoor",
        "block_ref": "TogglePhysicsDoor0"
      }
    },
    "write_ref": {
      "transaction_id": "tx_20260503_1601",
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

### 24.2 no_op

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "cleanup_blueprint_helper_block",
  "status": "no_op",
  "modified": false,
  "data": {
    "schema": "CleanupBlueprintHelperBlock.v1",
    "cleanup_result": {
      "cleaned_ref": {
        "graph_id": "EG_PhysicsDoor",
        "block_ref": "TogglePhysicsDoor0"
      },
      "missing_policy": "ignore",
      "missing": true
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": false,
    "compiled": false,
    "saved": false
  }
}
```

### 24.3 不允许出现的字段

```text
target_type
deleted_nodes
deleted_links
deleted_node_count
deleted_link_count
summary
external_dependencies
external_dependents
ownership
review
safety
diagnostics
next
rollback_data
journal_path
```

---

## 25. 实现完成定义

`CleanupBlueprintHelperBlock` 完成定义：

```text
1. 只删除明确 BlueprintHelper-owned block。
2. ownership 冲突必失败。
3. missing_policy=ignore 可幂等 no_op。
4. dry_run 极简并能阻断 external_dependents。
5. 正式执行事务式删除并写 Journal / Review。
6. 成功 Agent-facing 返回只含 cleaned_ref / write_ref / validation。
7. 失败只返回 error，rollback blocked/failed 时 modified=true。
8. 自动化测试覆盖 contract / resolve / ownership / dependency / write / rollback。
```
