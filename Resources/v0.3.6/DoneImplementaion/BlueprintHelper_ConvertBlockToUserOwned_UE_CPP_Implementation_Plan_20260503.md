# BlueprintHelper ConvertBlueprintHelperBlockToUserOwned UE 侧 C++ 可执行实现计划

状态：[x] 已完成
日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_ConvertBlockToUserOwned_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、Review UI 视觉实现

---

## 0. 实现目标

`ConvertBlueprintHelperBlockToUserOwned` 的目标是把一个明确的 BlueprintHelper-owned block 转为 user-owned。

它修改：

```text
BlueprintHelper ownership metadata
NodeComment 中的 BlueprintHelper managed 标记
rollback_data
```

它不修改：

```text
业务节点逻辑
Pin 默认值
节点连接
执行流
图表结构
组件树
资产文件路径
```

成功后：

```text
目标 block 不再属于 BlueprintHelper 管理范围。
后续 Cleanup / ReplaceOwned / PatchOwned 不应再默认管理该 block。
```

Agent-facing 成功结果只返回：

```text
data.conversion_result.converted_count
validation
```

明确不返回：

```text
write_ref
transaction_id
journal_recorded
converted_ref
graph_id
block_ref
block_id
converted_nodes
metadata diff
review
safety
diagnostics
next
```

UE 内部仍必须生成 transaction、写 Journal、保存 rollback_data、进入 Review Store，但这些不是默认 Agent-facing 成功字段。

---

## 1. 当前依赖与复用前提

本计划假设前面 Graph Write / Cleanup 阶段已经实现或正在实现以下基础服务：

```text
FBlueprintHelperToolResultBase
FBlueprintHelperToolResultBuilder
FBlueprintHelperGraphResolver
FBlueprintHelperBlockIdService
FBlueprintHelperOwnershipService
FBlueprintHelperTransactionJournalService
FBlueprintHelperReviewStoreService
FBlueprintHelperScopedAssetMutation
FBlueprintHelperFailedItem
FBlueprintHelperConflictItem
FBlueprintHelperDryRunIssue
EBlueprintHelperRollbackResult
```

如果当前分支中这些服务尚未完整存在，则本工具应先补最小实现：

```text
Block 定位
Ownership 读取 / 写入 / 清除
Journal 记录
Rollback metadata snapshot
ToolResultBase 序列化
```

不要为本工具单独实现一套 transaction / ownership / error JSON。

---

## 2. Phase A：新增类型文件

### 2.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperConvertBlockToUserOwnedTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperConvertBlockToUserOwnedTypes.cpp
```

### 2.2 新增枚举

```cpp
enum class EBlueprintHelperOwnershipScope : uint8
{
    Block
};

enum class EBlueprintHelperAlreadyUserOwnedPolicy : uint8
{
    Error,
    Ignore
};

enum class EBlueprintHelperConversionStatus : uint8
{
    Converted,
    AlreadyUserOwned
};

enum class EBlueprintHelperOwnershipStage : uint8
{
    ParseInput,
    ResolveTarget,
    OwnershipCheck,
    DryRun,
    SnapshotBefore,
    WriteMetadata,
    WriteNodeComment,
    WriteJournal,
    WriteReview,
    Rollback
};

enum class EBlueprintHelperOwnershipErrorCode : uint8
{
    InvalidRequest,
    UnsupportedOwnershipScope,
    TargetBlueprintNotFound,
    TargetGraphNotFound,
    BlockNotFound,
    TargetNotOwned,
    AlreadyUserOwned,
    OwnershipMetadataWriteFailed,
    NodeCommentWriteFailed,
    JournalWriteFailed,
    ReviewWriteFailed,
    RollbackBlocked,
    RollbackFailed,
    AssetStateChangedDuringConversion,
    WritePermissionDisabled,
    ProfilePolicyViolation,
    BridgeDisconnected
};
```

### 2.3 字符串转换要求

所有枚举必须稳定序列化为字段稿中的 snake_case：

```text
convert_blueprint_helper_block_to_user_owned
block
error
ignore
already_user_owned
target_not_owned
ownership_metadata_write_failed
```

不要输出 C++ 枚举名，例如：

```text
EBlueprintHelperOwnershipErrorCode::TargetNotOwned
```

---

## 3. Phase B：新增 Agent-facing 结果结构

字段稿明确要求成功结果不包含 `write_ref`。因此结果结构体也不要放 `FBlueprintHelperWriteRef`。

### 3.1 正式成功 / no_op data

```cpp
struct FBlueprintHelperConvertBlockToUserOwnedResult
{
    int32 ConvertedCount = 0;

    // no_op 时使用，例如 already_user_owned。
    TOptional<FString> ConversionStatus;

    // no_op 时使用，例如 ignore。
    TOptional<FString> AlreadyUserOwnedPolicy;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperConvertBlockToUserOwnedResultData
{
    FString Schema = TEXT("ConvertBlueprintHelperBlockToUserOwned.v1");
    FBlueprintHelperConvertBlockToUserOwnedResult ConversionResult;

    TSharedRef<FJsonObject> ToJson() const;
};
```

JSON 规则：

```json
{
  "schema": "ConvertBlueprintHelperBlockToUserOwned.v1",
  "conversion_result": {
    "converted_count": 1
  }
}
```

no_op 规则：

```json
{
  "schema": "ConvertBlueprintHelperBlockToUserOwned.v1",
  "conversion_result": {
    "converted_count": 0,
    "conversion_status": "already_user_owned",
    "already_user_owned_policy": "ignore"
  }
}
```

### 3.2 dry_run data

```cpp
struct FBlueprintHelperConvertBlockToUserOwnedDryRunData
{
    FString Schema = TEXT("ConvertBlueprintHelperBlockToUserOwnedDryRun.v1");
    FBlueprintHelperDryRunResult DryRun;

    TSharedRef<FJsonObject> ToJson() const;
};
```

dry_run passed 只返回：

```json
{
  "schema": "ConvertBlueprintHelperBlockToUserOwnedDryRun.v1",
  "dry_run": {
    "result": "passed",
    "can_execute": true
  }
}
```

dry_run blocked 才返回：

```json
{
  "schema": "ConvertBlueprintHelperBlockToUserOwnedDryRun.v1",
  "dry_run": {
    "result": "blocked",
    "can_execute": false,
    "blocked_by": [],
    "conflicts": [],
    "errors": []
  }
}
```

### 3.3 明确禁止的结构字段

不要在本工具结果结构体中加入：

```cpp
FBlueprintHelperWriteRef WriteRef;
FString TransactionId;
FString GraphId;
FString BlockRef;
FString BlockId;
TArray<FString> ConvertedNodes;
int32 MetadataRemoved;
int32 CommentsRewritten;
```

这些只允许进入 Journal / Review / rollback_data / debug。

---

## 4. Phase C：输入模型与目标解析

### 4.1 新增 request 结构

```cpp
struct FBlueprintHelperConvertBlockToUserOwnedRequest
{
    FString AssetPath;
    FString GraphName;
    EBlueprintHelperOwnershipScope OwnershipScope = EBlueprintHelperOwnershipScope::Block;

    // 输入层允许二选一：
    FString BlockId;
    FString GraphId;
    FString BlockRef;

    EBlueprintHelperAlreadyUserOwnedPolicy AlreadyUserOwnedPolicy =
        EBlueprintHelperAlreadyUserOwnedPolicy::Error;

    bool bDryRun = false;
};
```

### 4.2 输入允许形式

形式 A：完整 block_id。

```json
{
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "graph": "EG_PhysicsDoor",
  "ownership_scope": "block",
  "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0",
  "dry_run": true
}
```

形式 B：graph_id + block_ref。

```json
{
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "graph": "EG_PhysicsDoor",
  "ownership_scope": "block",
  "graph_id": "EG_PhysicsDoor",
  "block_ref": "TogglePhysicsDoor0",
  "dry_run": true
}
```

内部统一为：

```cpp
ResolvedBlockId = BlockId.IsEmpty()
    ? BlockIdService.MakeFullBlockId(GraphId, BlockRef)
    : BlockId;
```

### 4.3 Agent-facing target

无论输入是否带 `block_id / block_ref`，成功、no_op、dry_run passed 的 target 只返回：

```json
{
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
  "graph": "EG_PhysicsDoor",
  "ownership_scope": "block"
}
```

blocked / failed 需要定位问题时，只能在：

```text
data.dry_run.conflicts[]
error.failed_item
error.conflicts[]
```

中返回 `block_id / ref`。

---

## 5. Phase D：新增服务类

### 5.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperConvertBlockToUserOwnedService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperConvertBlockToUserOwnedService.cpp
```

### 5.2 服务接口

```cpp
class FBlueprintHelperConvertBlockToUserOwnedService
{
public:
    FBlueprintHelperConvertBlockToUserOwnedService(
        const FBlueprintHelperGraphResolver& InGraphResolver,
        const FBlueprintHelperBlockIdService& InBlockIdService,
        const FBlueprintHelperOwnershipService& InOwnershipService,
        const FBlueprintHelperTransactionJournalService& InJournalService,
        const FBlueprintHelperReviewStoreService& InReviewStoreService);

    FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool ParseRequest(
        const TSharedPtr<FJsonObject>& Payload,
        FBlueprintHelperConvertBlockToUserOwnedRequest& OutRequest,
        FBlueprintHelperToolError& OutError) const;

    bool ResolveTarget(
        const FBlueprintHelperConvertBlockToUserOwnedRequest& Request,
        FBlueprintHelperResolvedOwnedBlock& OutTarget,
        FBlueprintHelperToolError& OutError) const;

    FBlueprintHelperToolResultBase ExecuteDryRun(
        const FBlueprintHelperConvertBlockToUserOwnedRequest& Request,
        const FBlueprintHelperResolvedOwnedBlock& Target) const;

    FBlueprintHelperToolResultBase ExecuteWrite(
        const FBlueprintHelperConvertBlockToUserOwnedRequest& Request,
        const FBlueprintHelperResolvedOwnedBlock& Target) const;
};
```

### 5.3 目标解析结果

```cpp
struct FBlueprintHelperResolvedOwnedBlock
{
    UBlueprint* Blueprint = nullptr;
    UEdGraph* Graph = nullptr;

    FString AssetPath;
    FString GraphName;
    FString GraphId;
    FString BlockRef;
    FString BlockId;

    TArray<UEdGraphNode*> BlockNodes;

    bool bBlockExists = false;
    bool bIsBlueprintHelperOwned = false;
    bool bAlreadyUserOwned = false;
};
```

---

## 6. Phase E：OwnershipService 扩展

本工具需要读、清除、转换 ownership。建议扩展已有 OwnershipService。

```cpp
class FBlueprintHelperOwnershipService
{
public:
    bool IsBlueprintHelperOwnedNode(const UEdGraphNode* Node, FString* OutBlockId = nullptr) const;

    bool FindNodesByBlockId(
        UBlueprint* Blueprint,
        const FString& BlockId,
        TArray<UEdGraphNode*>& OutNodes,
        FString& OutError) const;

    bool ValidateBlockOwnership(
        const TArray<UEdGraphNode*>& Nodes,
        const FString& ExpectedBlockId,
        FBlueprintHelperOwnershipValidationResult& OutResult) const;

    bool ConvertBlockToUserOwned(
        const TArray<UEdGraphNode*>& Nodes,
        const FString& BlockId,
        FBlueprintHelperOwnershipConversionSnapshot& OutBeforeSnapshot,
        FString& OutError) const;

    bool RestoreBlueprintHelperOwnership(
        const FBlueprintHelperOwnershipConversionSnapshot& Snapshot,
        FString& OutError) const;
};
```

### 6.1 Ownership 读取规则

以 UObject metadata 为稳定事实来源：

```text
BlueprintHelperOwned
BlueprintHelperBlockId
BlueprintHelperTransactionId
BlueprintHelperTool
BlueprintHelperFeatureName
```

NodeComment 只作为人类显示和兜底诊断，不作为唯一判断依据。

### 6.2 转换策略

转换时，对目标 block 内每个节点：

```text
1. 保存 before snapshot：
   - metadata key/value
   - NodeComment
   - node guid / stable node ref
   - graph name
2. 清除或改写 BlueprintHelper ownership metadata。
3. 清理 NodeComment 中的 [BlueprintHelper] managed block。
4. 不修改业务注释内容。
5. 不删除节点。
6. 不断开连线。
```

推荐 NodeComment 清理规则：

```text
如果 NodeComment 只包含 [BlueprintHelper] block，则清空或移除该 managed block。
如果 NodeComment 中包含用户注释 + [BlueprintHelper] block，只删除 managed block，保留用户注释。
```

### 6.3 Metadata 清理规则

删除：

```text
BlueprintHelperOwned
BlueprintHelperBlockId
BlueprintHelperTransactionId
BlueprintHelperTool
BlueprintHelperFeatureName
```

可选保留内部历史痕迹到 Journal，不保留到节点 metadata。

不建议写：

```text
BlueprintHelperConvertedToUserOwned=true
```

因为转换后节点应从 BlueprintHelper 管理集合中退出，不能继续靠 metadata 被扫描到。

---

## 7. Phase F：dry_run 实现

本工具必须 dry_run。

### 7.1 dry_run passed 检查

dry_run passed 条件：

```text
1. Blueprint 资产存在。
2. Graph 存在或可从 block_id 解析。
3. ownership_scope=block。
4. block 能唯一解析。
5. block 内节点全部 BlueprintHelper-owned。
7. 当前 Safety Profile 允许 ownership 转换。
```

返回极简：

```json
{
  "result": "passed",
  "can_execute": true
}
```

不得返回：

```text
ownership_change_summary
current_owner
new_owner
journal_will_record_conversion
metadata diff
node list
full snapshot
```

### 7.2 dry_run blocked 条件

blocked 示例：

```text
unsupported_ownership_scope
block_not_found
target_not_owned
mixed_ownership_block
already_user_owned
target_ambiguous
asset_state_unreadable
profile_policy_violation
```

blocked 可在 conflicts 中返回 block_id：

```json
{
  "code": "target_not_owned",
  "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0",
  "message": "The target block is not BlueprintHelper-owned."
}
```

### 7.3 already_user_owned_policy

如果目标已是 user-owned：

```text
already_user_owned_policy=error
→ dry_run blocked
→ 正式执行 failed

already_user_owned_policy=ignore
→ dry_run passed? 不建议
→ 推荐正式执行 no_op
```

为了让 dry_run 表达“可以安全调用正式 no_op”，建议：

```text
dry_run result=passed
can_execute=true
```

但 Journal 不需要写入，正式执行返回 no_op。

---

## 8. Phase G：正式写入实现

### 8.1 正式执行前重复 preflight

正式执行不能依赖旧 dry_run 结果。必须重新检查：

```text
Blueprint 存在
Graph 存在
block 唯一
already_user_owned_policy
```

避免 TOCTOU。

### 8.2 执行顺序

```text
1. ParseRequest
2. ResolveTarget
3. Preflight / ownership check
4. 如果 already_user_owned_policy=ignore 且已 user-owned：返回 no_op
5. Begin FBlueprintHelperScopedAssetMutation
6. 生成 internal transaction_id
7. Snapshot ownership metadata / NodeComment
8. 清除 BlueprintHelper metadata / managed NodeComment
9. MarkBlueprintAsStructurallyModified 或 MarkPackageDirty
10. 写 Transaction Journal
11. 写 Review Store
12. 返回 conversion_result.converted_count
```

### 8.3 是否需要编译

Ownership metadata 转换通常不影响 K2 编译结果：

```json
{
  "should_compile": false,
  "should_save": true,
  "compiled": false,
  "saved": false
}
```

no_op：

```json
{
  "should_compile": false,
  "should_save": false,
  "compiled": false,
  "saved": false
}
```

### 8.4 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "convert_blueprint_helper_block_to_user_owned",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "ownership_scope": "block"
  },
  "data": {
    "schema": "ConvertBlueprintHelperBlockToUserOwned.v1",
    "conversion_result": {
      "converted_count": 1
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

注意：这里没有 `write_ref`。

---

## 9. Phase H：Journal / Review 内部记录

虽然成功结果不返回 transaction，但 UE 内部必须记录。

### 9.1 Journal record 建议

```json
{
  "schema": "BlueprintHelper.TransactionJournal.v1",
  "transaction_id": "tx_20260503_1901",
  "operation": "convert_blueprint_helper_block_to_user_owned",
  "status": "applied",
  "target_assets": [
    "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  ],
  "ownership_conversion": {
    "ownership_scope": "block",
    "block_id": "EG_PhysicsDoor_TogglePhysicsDoor0",
    "graph": "EG_PhysicsDoor",
    "converted_count": 1,
    "converted_nodes": [
      {
        "node_guid": "...",
        "node_ref": "Branch0"
      }
    ]
  },
  "before": {
    "ownership_metadata_snapshot_ref": "..."
  },
  "after": {
    "ownership": "user_owned"
  },
  "rollback_data": {
    "type": "ownership_conversion",
    "restore_metadata": true,
    "restore_node_comment": true
  },
  "validation": {
    "should_compile": false,
    "should_save": true
  }
}
```

### 9.2 Review Store

Review 记录：

```text
review_status=pending_review
tool=ConvertBlueprintHelperBlockToUserOwned
target_asset
graph
ownership_scope=block
block_id
converted_count
rollback_available=true
```

Review UI 可显示被转换的 block，但 Agent-facing 不返回该信息。

---

## 10. Phase I：失败与 rollback

正式失败不返回：

```text
conversion_result
write_ref
review
safety
diagnostics
next
```

### 10.1 解析 / 预检失败

```text
modified=false
rollback_result=not_needed
```

常见错误：

```text
block_not_found
target_not_owned
already_user_owned
unsupported_ownership_scope
target_ambiguous
```

错误定位信息放在：

```text
error.failed_item
error.conflicts
```

### 10.2 写 metadata 失败并成功回滚

```text
modified=false
rollback_result=rolled_back
error.code=ownership_metadata_write_failed
error.stage=write_metadata
```

### 10.3 NodeComment 写失败并成功回滚

```text
modified=false
rollback_result=rolled_back
error.code=node_comment_write_failed
error.stage=write_node_comment
```

### 10.4 Journal 写失败

Journal 写失败不能报告成功。

策略：

```text
1. 尝试恢复 metadata / NodeComment snapshot。
2. 成功恢复：modified=false, rollback_result=rolled_back。
3. 恢复失败：modified=true, rollback_result=failed。
```

### 10.5 rollback blocked / failed

```text
modified=true
error.code=rollback_failed 或 rollback_blocked
error.stage=rollback
rollback_result=failed 或 blocked
```

此时 Agent 必须 stop_and_report，不得继续：

```text
cleanup
patch
merge
replace
save
```

---

## 11. Phase J：Bridge Router 接入

### 11.1 新增 command

```text
convert_blueprint_helper_block_to_user_owned
```

### 11.2 Router 分支

```cpp
if (Request.Command == TEXT("convert_blueprint_helper_block_to_user_owned"))
{
    return HandleConvertBlueprintHelperBlockToUserOwned(Request);
}
```

### 11.3 Handler

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleConvertBlueprintHelperBlockToUserOwned(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result =
        ConvertBlockToUserOwnedService.Execute(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("convert failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

### 11.4 RequestValidator

新增规则：

```cpp
if (Command == TEXT("convert_blueprint_helper_block_to_user_owned"))
{
    RequireString(Payload, TEXT("asset_path"));
    RequireString(Payload, TEXT("ownership_scope"));
    OptionalString(Payload, TEXT("graph"));
    OptionalString(Payload, TEXT("block_id"));
    OptionalString(Payload, TEXT("graph_id"));
    OptionalString(Payload, TEXT("block_ref"));
    OptionalString(Payload, TEXT("already_user_owned_policy"));
    OptionalBool(Payload, TEXT("dry_run"));
}
```

校验：

```text
block_id 与 graph_id+block_ref 至少提供一组。
ownership_scope 必须为 block。
already_user_owned_policy 缺省为 error。
```

### 11.5 写权限

该命令属于 UE 写操作，必须加入写命令集合：

```text
convert_blueprint_helper_block_to_user_owned
```

ReadOnly 下：

```text
dry_run 可执行
正式写入返回 ProfilePolicyViolation
```

---

## 12. Phase K：兼容前置工具修正

字段稿特别提示：该工具成功结果不返回 `write_ref / transaction_id`，并要求检查之前实现的 `Replace / Patch / Merge` 等工具是否也错误返回了内部审计数据。

因此本阶段必须新增一个协议审查任务：

```text
检查所有 Ownership / 普通能力工具成功结果：
- ConvertBlueprintHelperBlockToUserOwned 不返回 write_ref。
- Asset Factory / Component / Class Settings 不返回 transaction/review/safety。
- Graph Write 是否返回 write_ref 以各自最新字段稿为准。
```

不要把 Convert 的“无 write_ref”机械套到所有 Graph Write。  
当前 Convert 字段稿只明确要求本工具不返回 write_ref，并提醒检查其他工具是否仍符合最新确认稿。若 Replace / Patch / Merge 的最新字段稿仍要求 write_ref，则以各自字段稿为准；若后续 mapping 文档统一修正为 Graph Write 成功也不返回 write_ref，则再做统一迁移。

---

## 13. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperConvertBlockToUserOwnedContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperConvertBlockToUserOwnedWriteTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperConvertBlockToUserOwnedRollbackTests.cpp
```

### 13.1 Contract tests

```text
1. success_contract_minimal
   - operation=convert_blueprint_helper_block_to_user_owned
   - data.schema=ConvertBlueprintHelperBlockToUserOwned.v1
   - data.conversion_result.converted_count=1
   - 不包含 data.write_ref
   - 不包含 transaction_id
   - 不包含 block_ref/block_id/converted_ref
   - validation.should_compile=false
   - validation.should_save=true

2. dry_run_passed_contract
   - status=dry_run
   - modified=false
   - data.schema=ConvertBlueprintHelperBlockToUserOwnedDryRun.v1
   - data.dry_run.result=passed
   - data.dry_run.can_execute=true
   - 不包含 ownership_change_summary / node list

3. dry_run_blocked_contract
   - status=dry_run
   - result=blocked
   - blocked_by/conflicts/errors 存在
   - conflicts 可包含 block_id

4. no_op_already_user_owned_ignore_contract
   - status=no_op
   - modified=false
   - converted_count=0
   - conversion_status=already_user_owned
   - already_user_owned_policy=ignore
   - 不包含 write_ref
```

### 13.2 Write tests

```text
1. convert_owned_block_removes_metadata
2. convert_owned_block_removes_managed_node_comment
3. convert_preserves_user_comment_text
4. convert_does_not_delete_nodes
5. convert_does_not_delete_links
6. convert_writes_internal_journal
7. convert_writes_review_record
8. convert_marks_package_dirty
```

### 13.3 Failure tests

```text
1. block_not_found_returns_failed_item
2. target_not_owned_returns_conflict_block_id
3. unsupported_scope_fails
4. already_user_owned_error_fails
5. metadata_write_failure_rolls_back
6. node_comment_write_failure_rolls_back
7. journal_write_failure_rolls_back
8. rollback_failed_sets_modified_true
```

---

## 14. 推荐提交顺序

### Commit 1：类型与序列化

```text
Add ConvertBlueprintHelperBlockToUserOwned result and dry_run types
Add ownership scope / policy / error enums
Add JSON serialization without write_ref
```

验收：

```text
成功 data 中没有 write_ref。
no_op data 中没有 write_ref。
dry_run passed 极简。
```

### Commit 2：OwnershipService 扩展

```text
Add ownership metadata read helpers
Add FindNodesByBlockId
Add ConvertBlockToUserOwned
Add RestoreBlueprintHelperOwnership
Add NodeComment managed block stripper
```

验收：

```text
可定位 owned block。
可清理 metadata。
可保留用户注释。
可从 snapshot 恢复。
```

### Commit 3：Service dry_run

```text
Add ConvertBlockToUserOwnedService
Parse target
Resolve block_id / graph_id+block_ref
Implement dry_run passed / blocked
Handle already_user_owned_policy
```

验收：

```text
dry_run 不写资产。
blocked 冲突可带 block_id。
```

### Commit 4：Service write

```text
Implement write transaction
Snapshot metadata/comment
Convert metadata/comment
Write Journal and Review internally
Return conversion_result only
```

验收：

```text
正式成功 converted_count=1。
不返回 write_ref。
Journal 内部存在。
```

### Commit 5：rollback and failures

```text
Rollback metadata/comment on failure
Handle journal_write_failed
Handle rollback_failed modified=true
```

验收：

```text
rollback failed 时 modified=true。
```

### Commit 6：Bridge / Validator / Auth

```text
Register command
Add request validation
Add write permission gate
Add tests
```

验收：

```text
ReadOnly 正式写入被阻止。
dry_run 可在 ReadOnly 下运行。
```

### Commit 7：协议回归审查

```text
Add contract tests preventing write_ref in Convert success/no_op
Audit ordinary tools for transaction/review/safety leaks
Document Graph Write exceptions by field mapping
```

验收：

```text
Convert 成功永不返回 transaction_id。
普通能力工具不默认返回 transaction/review/safety。
```

---

## 15. 第一版不做的内容

```text
1. 不支持 feature 批量转换。
2. 不支持 component_group ownership 转换。
3. 不支持 event_entry ownership 转换。
4. 不支持重新接管 user-owned block。
5. 不返回 converted_ref。
6. 不返回 block_ref / block_id。
7. 不返回 transaction_id。
8. 不返回 metadata diff。
9. 不删除任何节点或连线。
10. 不自动 save。
```

---

## 16. 最小验收标准

正式成功必须形如：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "convert_blueprint_helper_block_to_user_owned",
  "status": "applied",
  "modified": true,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "graph": "EG_PhysicsDoor",
    "ownership_scope": "block"
  },
  "data": {
    "schema": "ConvertBlueprintHelperBlockToUserOwned.v1",
    "conversion_result": {
      "converted_count": 1
    }
  },
  "validation": {
    "should_compile": false,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

必须不出现：

```text
write_ref
transaction_id
journal_recorded
converted_ref
graph_id
block_ref
block_id
converted_nodes
metadata_removed
comments_rewritten
review
safety
diagnostics
next
```

这是本工具是否符合字段稿的核心判定。

---

## 17. 实现风险

### 17.1 NodeComment 清理误删用户注释

风险：

```text
用户在 BlueprintHelper NodeComment 后追加了自己的说明。
```

处理：

```text
只删除 [BlueprintHelper] managed block。
保留 managed block 前后的普通文本。
添加单测覆盖。
```

### 17.2 Metadata 清理后 Journal 缺失

风险：

```text
```

处理：

```text
Journal 写失败必须触发 rollback。
rollback failed 时 modified=true 并 stop_and_report。
```

### 17.3 混合 ownership block

风险：

```text
同一 block_id 下部分节点 owned，部分节点已被用户改写或 metadata 缺失。
```

处理：

```text
dry_run blocked。
正式写入 preflight failed。
不做部分转换。
```

### 17.4 转换后后续工具误认为仍 owned

风险：

```text
Cleanup / ReplaceOwned / PatchOwned 只靠 Journal 判断 ownership。
```

处理：

```text
Journal 只作为审计历史，不作为当前 ownership 唯一事实。
```
