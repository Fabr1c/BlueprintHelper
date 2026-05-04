# BlueprintHelper ToolResultBase / Common Envelope / Error Protocol UE 侧 C++ 可执行实现计划

状态：[x] 已完成
日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_ToolResultBase_CommonEnvelope_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++ + Bridge ToolResult 序列化层  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、具体业务工具实现细节、Review UI、Transaction Journal 具体存储实现

---

## 0. 实现目标

建立所有 Agent-facing MCP 工具共同遵守的 C++ ToolResultBase 外壳和通用协议工具层。

覆盖：

```text
ToolResultBase 顶层 envelope
ok / status 语义
trace_id 与 transaction_id 区分
短 data.schema
target 公共规则
validation 公共规则
dry_run 公共规则
error / conflicts / failed_item 公共结构
page 公共结构
resource_ref / bundle_ref / snapshot_ref 公共边界
transaction / review / safety 默认不返回规则
路径压缩和完整 asset_path 规则
```

核心字段契约：

```text
1. 所有 Agent-facing 工具使用 ToolResultBase 外壳。
2. 顶层 schema 固定 BlueprintHelper.McpToolResult.v1。
3. data.schema 使用短命名。
4. operation 使用稳定短 operation 名。
5. trace_id 是日志追踪 ID，不等于 transaction_id。
6. status 第一版统一使用 completed / applied / no_op / dry_run / failed。
7. ok 表示工具调用是否成功，不等于业务是否通过。
8. 工具自身失败才 ok=false / status=failed / error。
9. diagnostics Markdown 有 Blocking 仍 ok=true / status=completed。
10. compile_result.success=false 仍 ok=true / status=completed。
11. dry_run blocked 仍 ok=true / status=dry_run。
12. 写工具 validation 只返回 should_compile / should_save。
13. validation 不返回 compiled / saved。
14. compile/save/read/runtime/diagnostics/lifecycle/debug/export/query 工具不返回 validation。
15. dry_run passed 默认只返回 result / can_execute。
16. dry_run blocked / failed 返回 blocked_by / conflicts / errors 必要摘要。
17. 成功结果不返回 write_ref / transaction_id / review / safety。
18. conflicts 只在失败/blocked 场景返回，不在成功场景返回空数组。
19. page 用于列表型只读工具，空列表不是失败。
20. resource_ref / bundle_ref / snapshot_ref 不使用本地绝对路径。
21. 写工具 target.asset_path 必须完整，不允许 %{path_filter}。
22. %{path_filter} 只限 find_assets 列表型结果。
```

---

## 1. 总体落点

### 1.1 必须集中实现

所有工具不得各自手写顶层 envelope。必须集中到：

```text
FBlueprintHelperToolResultBase
FBlueprintHelperToolResultBuilder
FBlueprintHelperToolResultSerializer
FBlueprintHelperToolError
FBlueprintHelperValidationHint
FBlueprintHelperDryRunPayload
FBlueprintHelperConflictItem
FBlueprintHelperFailedItem
FBlueprintHelperPageInfo
```

业务工具只负责填充业务 `target / data / validation / error`。顶层 `schema / trace_id / ok / status / modified` 由统一 builder 和 serializer 输出。

### 1.2 Bridge 只包装，不改语义

Bridge handler 只能根据 `FBlueprintHelperToolResultBase::bOk` 判断 Bridge success/error，不得读取业务 data 再二次判定。


```text
compile_result.success=false
diagnostics Markdown 包含 Blocking
dry_run.result=blocked
runtime_profile.status=blocked
check_setup_state.setup_state.status=blocked
```

---

## 2. Phase A：协议类型文件

新增或收敛：

```text
Source/BlueprintHelper/Public/Protocol/BlueprintHelperToolResultBase.h
Source/BlueprintHelper/Private/Protocol/BlueprintHelperToolResultBase.cpp
Source/BlueprintHelper/Public/Protocol/BlueprintHelperToolResultBuilder.h
Source/BlueprintHelper/Private/Protocol/BlueprintHelperToolResultBuilder.cpp
Source/BlueprintHelper/Public/Protocol/BlueprintHelperProtocolConstants.h
Source/BlueprintHelper/Public/Protocol/BlueprintHelperProtocolGuards.h
Source/BlueprintHelper/Private/Protocol/BlueprintHelperProtocolGuards.cpp
Source/BlueprintHelper/Public/Protocol/BlueprintHelperTargetBuilder.h
Source/BlueprintHelper/Private/Protocol/BlueprintHelperTargetBuilder.cpp
```

如果当前已有 `FBlueprintHelperToolResultBase`，应原地收敛，避免并行存在旧 envelope 和新 envelope。

---

## 3. Phase B：枚举与常量

### 3.1 Tool status

```cpp
enum class EBlueprintHelperToolStatus : uint8
{
    Completed,
    Applied,
    NoOp,
    DryRun,
    Failed
};
```

序列化：

```text
completed
applied
no_op
dry_run
failed
```

第一版禁止：

```text
success
error
blocked
skipped
partial
pending
```

### 3.2 Dry run result

```cpp
enum class EBlueprintHelperDryRunResult : uint8
{
    Passed,
    Blocked,
    Failed
};
```

序列化：

```text
passed
blocked
failed
```

### 3.3 协议常量

```cpp
namespace BlueprintHelperProtocol
{
    static const TCHAR* ToolResultSchema = TEXT("BlueprintHelper.McpToolResult.v1");

    namespace Status
    {
        static const TCHAR* Completed = TEXT("completed");
        static const TCHAR* Applied = TEXT("applied");
        static const TCHAR* NoOp = TEXT("no_op");
        static const TCHAR* DryRun = TEXT("dry_run");
        static const TCHAR* Failed = TEXT("failed");
    }

    namespace DryRun
    {
        static const TCHAR* Passed = TEXT("passed");
        static const TCHAR* Blocked = TEXT("blocked");
        static const TCHAR* Failed = TEXT("failed");
    }
}
```

---

## 4. Phase C：ToolResultBase

### 4.1 结构定义

```cpp
struct FBlueprintHelperToolResultBase
{
    bool bOk = false;
    FString Schema = TEXT("BlueprintHelper.McpToolResult.v1");
    FString Operation;
    FString TraceId;
    EBlueprintHelperToolStatus Status = EBlueprintHelperToolStatus::Completed;
    bool bModified = false;

    TSharedPtr<FJsonObject> Target;
    TSharedPtr<FJsonObject> Data;

    TOptional<FBlueprintHelperValidationHint> Validation;
    TOptional<FBlueprintHelperToolError> Error;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 4.2 顶层字段顺序

统一输出：

```text
ok
schema
operation
trace_id
status
modified
target
data
validation
error
```

### 4.3 Envelope 校验

`ToJson()` 前执行内部校验：

```text
1. schema 必须是 BlueprintHelper.McpToolResult.v1。
2. operation 非空。
3. trace_id 非空。
4. status 必须是 completed / applied / no_op / dry_run / failed。
5. ok=false 时 status 必须 failed，error 必须存在。
6. status=failed 时 ok 必须 false。
7. ok=true 时不应存在 error。
8. status=dry_run 时 modified 必须 false。
9. dry_run blocked 仍 ok=true/status=dry_run。
10. diagnostics Blocking / compile failed 等业务失败不得改写 ok=false。
```

测试构建中可以 `ensureAlwaysMsgf`；运行时不应因为协议校验导致 Editor 崩溃，必要时返回 `internal_error`。

---

## 5. Phase D：TraceId

新增：

```text
Source/BlueprintHelper/Public/Protocol/BlueprintHelperTraceId.h
Source/BlueprintHelper/Private/Protocol/BlueprintHelperTraceId.cpp
```

接口：

```cpp
class FBlueprintHelperTraceId
{
public:
    static FString NewTraceId();
};
```

格式建议：

```text
trace_yyyyMMdd_HHmmss_XXXX
```

规则：

```text
trace_id 是日志追踪 ID。
读操作、dry_run、compile、save、diagnostics、runtime_profile 都有 trace_id。
trace_id 不等于 transaction_id。
trace_id 不可被 Review / Journal / rollback 当作业务引用。
```

---

## 6. Phase E：ValidationHint

### 6.1 结构

```cpp
struct FBlueprintHelperValidationHint
{
    bool bShouldCompile = false;
    bool bShouldSave = false;

    TSharedRef<FJsonObject> ToJson() const;
};
```

输出：

```json
{
  "should_compile": false,
  "should_save": true
}
```

禁止：

```text
compiled
saved
compile_status
save_status
```

### 6.2 返回范围

只在写工具成功 / 写工具 no_op 中按需返回。

不返回 validation 的工具：

```text
compile_blueprint_asset
save_asset
read tools
runtime_profile
diagnostics
Transaction Journal Query
Editor Lifecycle
Debug / Export
Project Context
Asset Discovery
```

建议新增内部分类：

```cpp
enum class EBlueprintHelperToolCategory : uint8
{
    Read,
    Write,
    Compile,
    Save,
    RuntimeProfile,
    Diagnostics,
    Lifecycle,
    DebugExport,
    Query
};
```

并提供：

```cpp
bool BlueprintHelperProtocol::AllowValidation(EBlueprintHelperToolCategory Category);
```

---

## 7. Phase F：Error / Conflict / FailedItem

### 7.1 ToolError

```cpp
struct FBlueprintHelperToolError
{
    FString Code;
    FString Stage;
    FString Message;
    bool bRetryable = false;

    TArray<FBlueprintHelperConflictItem> Conflicts;
    TOptional<FBlueprintHelperFailedItem> FailedItem;

    TSharedRef<FJsonObject> ToJson() const;
};
```

输出：

```json
{
  "code": "asset_not_found",
  "stage": "resolve_asset",
  "message": "The requested asset was not found.",
  "retryable": false
}
```

只在非空时输出：

```text
conflicts
failed_item
```

不得输出：

```text
conflicts=[]
stack_trace
本地绝对路径
完整 payload
完整 settings.json
Token / secret
```

### 7.2 ConflictItem

```cpp
struct FBlueprintHelperConflictItem
{
    FString Code;
    TSharedPtr<FJsonObject> Fields;

    TSharedRef<FJsonObject> ToJson() const;
};
```

建议提供 builder，避免各工具随意构造字段：

```cpp
class FBlueprintHelperConflictBuilder
{
public:
    static FBlueprintHelperConflictItem PropertyNotFound(const FString& PropertyPath);
    static FBlueprintHelperConflictItem SlotTypeMismatch(const FString& WidgetName, const FString& SlotType, const FString& PropertyPath);
    static FBlueprintHelperConflictItem UnsavedAssetsExist(int32 UnsavedAssetCount);
    static FBlueprintHelperConflictItem RiskCommandMissing(const FString& CommandName);
};
```

### 7.3 FailedItem

```cpp
struct FBlueprintHelperFailedItem
{
    FString Type; // asset | widget | row | component | graph | node | pin | transaction
    TSharedPtr<FJsonObject> Fields;

    TSharedRef<FJsonObject> ToJson() const;
};
```

---

## 8. Phase G：DryRunPayload

### 8.1 结构

```cpp
struct FBlueprintHelperDryRunIssue
{
    FString Code;
    FString Message;
    TSharedPtr<FJsonObject> Fields;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperDryRunPayload
{
    EBlueprintHelperDryRunResult Result = EBlueprintHelperDryRunResult::Passed;
    bool bCanExecute = true;

    TArray<FString> BlockedBy;
    TArray<FBlueprintHelperConflictItem> Conflicts;
    TArray<FBlueprintHelperDryRunIssue> Errors;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 8.2 passed 极简

当 `Result=Passed`：

```json
{
  "result": "passed",
  "can_execute": true
}
```

不得输出：

```text
blocked_by
conflicts
errors
would_create
would_delete
would_modify
plan
transaction_id
```

### 8.3 blocked / failed

当 `Result=Blocked` 或 `Failed`：

```json
{
  "result": "blocked",
  "can_execute": false,
  "blocked_by": ["external_dependents_exist"],
  "conflicts": [
    {
      "code": "external_dependents_exist",
      "message": "External dependents exist for the requested target."
    }
  ],
  "errors": []
}
```

顶层必须仍是：

```text
ok=true
status=dry_run
modified=false
```

---

## 9. Phase H：PageInfo

```cpp
struct FBlueprintHelperPageInfo
{
    int32 Limit = 20;
    bool bHasMore = false;
    TOptional<FString> NextCursor;

    TSharedRef<FJsonObject> ToJson() const;
};
```

无下一页：

```json
{
  "limit": 20,
  "has_more": false
}
```

有下一页：

```json
{
  "limit": 20,
  "has_more": true,
  "next_cursor": "cursor_abc"
}
```

适用：

```text
find_assets
list_blueprint_helper_transactions
其他列表型只读查询
```

空列表：

```text
ok=true
status=completed
items=[]
```

不是失败。

---

## 10. Phase I：ResourceRef / BundleRef / SnapshotRef

### 10.1 通用规则

大 payload 不内联，使用：

```text
resource_ref
bundle_ref
snapshot_ref
```

统一 URI：

```text
resource://blueprinthelper/...
```

### 10.2 禁止

```text
C:\...
/Users/...
/home/...
file://...
bundle bytes
full payload inline
```

### 10.3 校验 helper

```cpp
class FBlueprintHelperResourceRefValidator
{
public:
    static bool IsBlueprintHelperResourceRef(const FString& Ref);
    static bool IsLocalAbsolutePath(const FString& Ref);
    static bool ValidateNoLocalPath(const FString& Ref, FString& OutError);
};
```

`ResourceRefRegistry` 可由 Debug / Export 工具实现；ToolResultBase 负责输出校验和测试守卫。

---

## 11. Phase J：TargetBuilder

新增：

```text
Source/BlueprintHelper/Public/Protocol/BlueprintHelperTargetBuilder.h
Source/BlueprintHelper/Private/Protocol/BlueprintHelperTargetBuilder.cpp
```

接口：

```cpp
class FBlueprintHelperTargetBuilder
{
public:
    static TSharedRef<FJsonObject> Asset(const FString& AssetPath);
    static TSharedRef<FJsonObject> AssetGraph(const FString& AssetPath, const FString& Graph);
    static TSharedRef<FJsonObject> AssetReadScope(const FString& AssetPath, const FString& ReadScope);
    static TSharedRef<FJsonObject> ReadScope(const FString& ReadScope);
    static TSharedRef<FJsonObject> QueryScope(const FString& QueryScope);
    static TSharedRef<FJsonObject> DiagnosticsMode(const FString& Mode);
    static TSharedRef<FJsonObject> LifecycleScope(const FString& Scope);
    static TSharedRef<FJsonObject> ExportScope(const FString& Scope);
};
```

写工具 target 必须拒绝：

```text
%{path_filter}/...
本地绝对路径
空 asset_path
```

错误码建议：

```text
invalid_target_asset_path
```

`%{path_filter}` 只允许 `find_assets` 的 `data.assets[].asset_path` 使用，不应出现在任何写工具 target 或单资产 read target。

---

## 12. Phase K：ToolResultBuilder

```cpp
class FBlueprintHelperToolResultBuilder
{
public:
    static FBlueprintHelperToolResultBase Completed(
        const FString& Operation,
        const TSharedPtr<FJsonObject>& Target,
        const TSharedPtr<FJsonObject>& Data);

    static FBlueprintHelperToolResultBase Applied(
        const FString& Operation,
        const TSharedPtr<FJsonObject>& Target,
        const TSharedPtr<FJsonObject>& Data,
        const FBlueprintHelperValidationHint& Validation);

    static FBlueprintHelperToolResultBase NoOp(
        const FString& Operation,
        const TSharedPtr<FJsonObject>& Target,
        const TSharedPtr<FJsonObject>& Data,
        TOptional<FBlueprintHelperValidationHint> Validation = TOptional<FBlueprintHelperValidationHint>());

    static FBlueprintHelperToolResultBase DryRun(
        const FString& Operation,
        const TSharedPtr<FJsonObject>& Target,
        const TSharedPtr<FJsonObject>& Data);

    static FBlueprintHelperToolResultBase Failed(
        const FString& Operation,
        const TSharedPtr<FJsonObject>& Target,
        const FBlueprintHelperToolError& Error);
};
```

Semantics：

```text
Completed：ok=true/status=completed/modified=false/validation absent by default。
Applied：ok=true/status=applied/modified=true/validation required unless special-cased。
NoOp：ok=true/status=no_op/modified=false。
DryRun：ok=true/status=dry_run/modified=false。
Failed：ok=false/status=failed/error present/data absent。
```

若 rollback failed 导致部分修改，可允许：

```cpp
Result.bModified = true;
```

但必须由业务工具显式设置。

---

## 13. Phase L：禁止成功默认返回 transaction / review / safety

### 13.1 默认禁止字段

普通成功结果不返回：

```text
write_ref
transaction_id
journal_recorded
review
safety
safety_profile
rollback_data
```

### 13.2 允许 transaction_id 的专用场景

只允许在专门查询 / debug / rollback 定位工具中出现：

```text
read_blueprint_helper_transaction
list_blueprint_helper_transactions
export_transaction_debug_bundle target.transaction_id
rollback cleanup command 输入 target
```

普通写工具成功不默认返回 `transaction_id`。

### 13.3 Graph Write 计划同步

如果旧实现或旧文档仍在成功结果中返回：

```text
write_ref
transaction_id
journal_recorded
review
safety
```

应按本 Common Envelope 协议统一迁移，除非某个工具后续拥有比本文件更新的字段确认稿明确保留例外。

---

## 14. Phase M：BridgeResponse 统一

新增：

```text
Source/BlueprintHelper/Public/Protocol/BlueprintHelperBridgeResponseBuilder.h
Source/BlueprintHelper/Private/Protocol/BlueprintHelperBridgeResponseBuilder.cpp
```

实现：

```cpp
FBlueprintHelperBridgeResponse MakeBridgeResponse(
    const FBlueprintHelperBridgeRequest& Req,
    const FBlueprintHelperToolResultBase& Result)
{
    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet()
                ? Result.Error->Message
                : TEXT("BlueprintHelper tool failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

所有 Bridge handler 迁移到该出口。

Bridge 不检查：

```text
data.compile_result.success
data.dry_run.result
data.runtime_profile.status
data.markdown contains Blocking
```

---

## 15. Phase N：Protocol Guard / 隐私守卫

### 15.1 Forbidden key scanner

测试和 debug 构建中扫描 JSON：

```cpp
class FBlueprintHelperProtocolGuard
{
public:
    static void ValidateEnvelope(const FBlueprintHelperToolResultBase& Result);
    static void ScanForbiddenKeys(const TSharedRef<FJsonObject>& Root);
    static void ValidateNoForbiddenSuccessFields(const TSharedRef<FJsonObject>& Root);
    static void ValidateNoLocalPaths(const TSharedRef<FJsonObject>& Root);
};
```

禁止 key：

```text
local_absolute_path
settings_json
claude_md_content
token
secret
password
api_key
stack_trace
```

### 15.2 本地路径扫描

扫描字符串值：

```text
C:\
/Users/
/home/
file://
```

放行：

```text
resource://blueprinthelper/...
/Game/...
/Script/...
```

---

## 16. Phase O：现有工具迁移清单

扫描 Bridge Router / command registry，逐个迁移：

```text
append_blueprint_graph
replace_blueprint_graph
patch_blueprint_graph
merge_blueprint_graph
cleanup_blueprint_helper_block
rollback_cleanup_transaction
convert_blueprint_helper_block_to_user_owned
compile_blueprint_asset
save_asset
find_assets
read_asset_summary
open_asset_in_editor
get_editor_context
get_runtime_profile
run_blueprinthelper_diagnostics
read_project_context
check_project_marker
check_setup_state
export_debug_bundle
export_transaction_debug_bundle
export_asset_logic_snapshot
read_large_payload_ref
UMG tools
DataTable tools
DataAsset tools
Asset Factory tools
Component tools
Class Settings tools
```

每个工具必须：

```text
1. 返回 FBlueprintHelperToolResultBase。
2. 使用 ToolResultBuilder。
3. 使用 TargetBuilder。
4. 使用短 data.schema。
5. 使用统一 ValidationHint。
6. 使用统一 ToolError。
7. 通过 MakeBridgeResponse 出口。
```

删除旧字段：

```text
transaction
write_ref
transaction_id
review
safety
safety_profile
diagnostics
compiled
saved
invalid_settings in success
conflicts=[] in success
```

---

## 17. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperToolResultBaseContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperToolResultStatusSemanticsTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperToolResultValidationTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperToolResultPrivacyTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperBridgeResponseSemanticsTests.cpp
```

### 17.1 Envelope tests

```text
1. all_results_have_top_level_schema
2. all_results_have_operation_trace_id_status_modified
3. data_schema_uses_short_name
4. status_enum_rejects_unknown_values
5. trace_id_is_not_transaction_id
```

### 17.2 ok/status semantics tests

```text
1. diagnostics_blocking_still_ok_completed
2. compile_failed_still_ok_completed
3. dry_run_blocked_still_ok_dry_run
4. runtime_profile_blocked_still_ok_completed
5. setup_state_blocked_still_ok_completed
6. tool_failure_is_ok_false_status_failed
```

### 17.3 validation tests

```text
1. write_success_validation_has_should_compile_should_save_only
2. validation_never_has_compiled_saved
3. compile_tool_has_no_validation
4. save_tool_has_no_validation
5. read_tools_have_no_validation
6. diagnostics_has_no_validation
7. lifecycle_has_no_validation
```

### 17.4 dry_run tests

```text
1. dry_run_passed_minimal_result_can_execute
2. dry_run_passed_no_plan_no_would_lists
3. dry_run_blocked_has_blocked_by_conflicts_errors
4. dry_run_status_modified_false
```

### 17.5 forbidden success field tests

```text
1. ordinary_success_has_no_write_ref
2. ordinary_success_has_no_transaction_id
3. ordinary_success_has_no_review
4. ordinary_success_has_no_safety
5. success_has_no_conflicts_empty_array
6. success_has_no_invalid_settings
```

### 17.6 page / resource tests

```text
1. empty_list_is_completed_success
2. page_has_limit_has_more
3. resource_ref_rejects_local_path
4. bundle_ref_uses_resource_scheme
5. snapshot_ref_uses_resource_scheme
```

### 17.7 target path tests

```text
1. write_target_asset_path_requires_full_path
2. write_target_rejects_path_filter_alias
3. single_asset_read_rejects_path_filter_alias
4. find_assets_allows_path_filter_alias_only_in_result_asset_path
```

### 17.8 Bridge tests

```text
1. bridge_success_for_compile_result_failed
2. bridge_success_for_diagnostics_blocking
3. bridge_success_for_dry_run_blocked
4. bridge_error_only_when_tool_result_ok_false
```

---

## 18. 推荐提交顺序

### Commit 1：协议常量 / 枚举

```text
Add ToolResult protocol constants
Add tool status enum
Add dry_run result enum
Add short schema validation helpers
```

验收：

```text
status 只允许 completed/applied/no_op/dry_run/failed。
```

### Commit 2：ToolResultBase / Serializer

```text
Implement FBlueprintHelperToolResultBase
Implement ToJson
Implement envelope validation
```

验收：

```text
顶层 schema 固定。
ok/status/error 基础关系正确。
```

### Commit 3：Error / Conflict / FailedItem

```text
Add ToolError
Add ConflictItem
Add FailedItem
Add builders and sanitization
```

验收：

```text
失败结果统一 error。
成功不输出 conflicts=[]。
```

### Commit 4：ValidationHint

```text
Add ValidationHint serializer
Remove compiled/saved
Add category-based validation guard
```

验收：

```text
写工具 validation 只有 should_compile/should_save。
```

### Commit 5：DryRun payload

```text
Add DryRun payload serializer
Enforce passed minimal shape
Enforce blocked/failed summary shape
```

验收：

```text
dry_run blocked still ok=true/status=dry_run。
```

### Commit 6：PageInfo / ResourceRef validators

```text
Add PageInfo
Add ResourceRefValidator
Add no-local-path guard
```

验收：

```text
空列表成功。
resource_ref 不泄露本地路径。
```

### Commit 7：TargetBuilder

```text
Add common target builders
Enforce full asset_path for write/single-asset tools
Restrict %{path_filter}
```

验收：

```text
写工具拒绝压缩路径。
```

### Commit 8：ToolResultBuilder

```text
Add Completed/Applied/NoOp/DryRun/Failed builders
Migrate first batch of tools
```

验收：

```text
业务工具不再手写 envelope。
```

### Commit 9：Bridge response unification

```text
Add MakeBridgeResponse helper
Migrate all Bridge handlers
Prevent business data from changing Bridge success/error
```

验收：

```text
compile failed / diagnostics blocking / dry_run blocked 仍 Bridge success。
```

### Commit 10：Forbidden field / privacy guard

```text
Add protocol guard
Scan forbidden keys and local paths in tests
Apply to all ordinary success results
```

验收：

```text
普通成功结果不返回 transaction/review/safety/path leaks。
```

### Commit 11：Full tool migration

```text
Migrate all existing tool results to ToolResultBase
Remove legacy result envelopes
Remove transaction/review/safety default output
```

验收：

```text
所有 Agent-facing 工具协议一致。
```

### Commit 12：Contract regression suite

```text
Add broad contract tests for every registered command
Fail CI if envelope fields drift
```

验收：

```text
字段稿验收项全部通过。
```

---

## 19. 第一版不做的内容

```text
1. 不实现完整 JSON Schema runtime validator。
2. 不把 ToolResultBase 绑定到 MCP Server TypeScript schema。
3. 不内联 Transaction Journal / Review。
4. 不提供 full debug payload。
5. 不输出 compiled/saved。
6. 不输出 success conflicts=[]。
7. 不支持任意 status 扩展。
8. 不允许业务工具绕过 ToolResultBuilder。
```

---

## 20. 最小验收标准

普通读工具：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_asset_summary",
  "trace_id": "trace_20260503_3601",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
    "read_scope": "asset_summary"
  },
  "data": {
    "schema": "ReadAssetSummary.v1",
    "asset": {
      "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor",
      "asset_type": "Blueprint",
      "asset_class": "/Script/Engine.Blueprint",
      "loaded": true
    }
  }
}
```

写工具成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "set_data_asset_property",
  "trace_id": "trace_20260503_2501",
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

dry_run blocked：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "close_editor",
  "trace_id": "trace_20260503_4202",
  "status": "dry_run",
  "modified": false,
  "target": {
    "lifecycle_scope": "editor"
  },
  "data": {
    "schema": "CloseEditorDryRun.v1",
    "dry_run": {
      "result": "blocked",
      "can_execute": false,
      "blocked_by": ["risk_command_missing"],
      "conflicts": [
        {
          "code": "risk_command_missing",
          "command": "close_editor"
        }
      ],
      "errors": []
    }
  }
}
```

工具失败：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_asset_summary",
  "trace_id": "trace_20260503_3602",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_Missing",
    "read_scope": "asset_summary"
  },
  "error": {
    "code": "asset_not_found",
    "stage": "resolve_asset",
    "message": "The requested asset was not found.",
    "retryable": false
  }
}
```

必须不出现：

```text
validation.compiled
validation.saved
write_ref
transaction_id
journal_recorded
review
safety
safety_profile
rollback_data
conflicts=[] in success
local absolute path
settings.json content
Token
secret
```

---

## 21. 实现风险

### 21.1 旧工具残留 write_ref / transaction_id

风险：Graph Write / Cleanup 旧实现还返回 `write_ref` 或 `transaction_id`。

处理：Common Envelope 协议回归测试统一扫描。普通成功结果全部禁止。仅 Transaction Query / Debug Export / Rollback 定位等专用工具允许 transaction_id。


风险：`compile_result.success=false` 或 diagnostics Blocking 被当成 `ok=false`。

处理：Bridge 统一只看 `ToolResultBase.bOk`。业务 data 不参与 Bridge error 判断。

### 21.3 validation 混入 compiled/saved

风险：复用旧 validation 结构导致写工具返回 `compiled/saved`。

处理：`FBlueprintHelperValidationHint` 结构只定义 `should_compile/should_save`，删除旧字段，并用 contract test 锁定。

### 21.4 target 使用压缩路径

风险：Agent 把 `find_assets` 返回的 `%{path_filter}` 直接传给写工具。

处理：TargetBuilder / RequestValidator 拒绝写工具和单资产读工具中的 alias。

### 21.5 成功场景返回空 conflicts

风险：工具统一初始化 `conflicts=[]` 并序列化。

处理：Optional / 非空时才输出，成功路径不设置 conflicts。
