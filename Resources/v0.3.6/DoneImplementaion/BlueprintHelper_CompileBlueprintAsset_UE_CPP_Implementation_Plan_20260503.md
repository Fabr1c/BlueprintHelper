# BlueprintHelper compile_blueprint_asset UE 侧 C++ 可执行实现计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_CompileBlueprintAsset_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、Graph Write 自动修复逻辑

---

## 0. 实现目标

`compile_blueprint_asset` 负责编译一个明确 Blueprint 资产，并返回编译结果。

它不负责：

```text
修复蓝图错误
保存资产
修改蓝图图表
写 Graph
写 Component
写 Class Settings
生成 transaction_id
写 Journal / Review
```

字段契约的核心点：

```text
1. 工具调用成功执行时：ok=true / status=completed。
2. 蓝图是否编译通过由 data.compile_result.success 表达。
3. 蓝图编译失败不是工具自身失败。
4. 编译失败时错误合并到 data.compile_result.markdown。
5. markdown 只包含 block_id + message。
6. compile 不返回 validation。
7. compile 不返回 write_ref / transaction_id / review / safety。
8. compile 默认 modified=false。
```

---

## 1. 当前依赖与复用前提

本计划假设现有 UE 插件侧已存在或即将存在：

```text
FBlueprintHelperToolResultBase
FBlueprintHelperToolResultBuilder
FBlueprintHelperGraphResolver 或 AssetResolver
FBlueprintHelperOwnershipService
FBlueprintHelperBridgeRouter
FBlueprintHelperRequestValidator
FBlueprintHelperConflictItem
```

如果当前已有 `FBlueprintHelperCompileService`，本计划应优先改造该服务，而不是新增第二套 compile 实现。

---

## 2. Phase A：新增 / 收敛类型文件

### 2.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperCompileAssetTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperCompileAssetTypes.cpp
```

如果已有 compile 类型文件，则在现有文件中收敛字段，不新增重复类型。

### 2.2 新增枚举

```cpp
enum class EBlueprintHelperCompileStatus : uint8
{
    Succeeded,
    Failed
};

enum class EBlueprintHelperCompileStage : uint8
{
    ParseInput,
    ResolveAsset,
    ValidateAssetType,
    Compile,
    CollectMessages,
    MapMessagesToBlockIds
};

enum class EBlueprintHelperCompileToolErrorCode : uint8
{
    InvalidRequest,
    AssetNotFound,
    TargetNotBlueprint,
    BlueprintLoadFailed,
    CompileApiFailed,
    BridgeDisconnected,
    InternalError
};
```

### 2.3 字符串序列化

稳定输出：

```text
operation = compile_blueprint_asset
data.schema = CompileBlueprintAsset.v1
status = completed | failed
compile_result.status = succeeded | failed
error.code = asset_not_found / target_not_blueprint / compile_api_failed
```

不要输出 C++ enum 原名。

---

## 3. Phase B：Agent-facing 结果结构

### 3.1 Compile result data

```cpp
struct FBlueprintHelperCompileAssetResultData
{
    FString Schema = TEXT("CompileBlueprintAsset.v1");
    FBlueprintHelperCompileResult CompileResult;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperCompileResult
{
    bool bSuccess = false;
    FString Status; // succeeded | failed
    int32 WarningCount = 0;

    // Only when compile failed.
    TOptional<FString> Format;   // markdown
    TOptional<FString> Markdown;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.2 明确禁止的字段

结果结构不要包含：

```cpp
int32 ErrorCount;
TArray<FBlueprintHelperCompileMessage> Messages;
FBlueprintHelperWriteRef WriteRef;
FBlueprintHelperValidationResult Validation;
FString TransactionId;
FString ReviewStatus;
FString SafetyProfile;
```

这些不属于 compile 工具 Agent-facing 返回。

---

## 4. Phase C：输入与目标解析

### 4.1 Request 结构

```cpp
struct FBlueprintHelperCompileAssetRequest
{
    FString AssetPath;
};
```

### 4.2 输入 JSON

```json
{
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
}
```

兼容旧字段时可短期接受：

```text
target_blueprint
assetPath
```

但 UE 侧 result 统一输出：

```json
"target": {
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
}
```

### 4.3 目标解析

新增或复用：

```cpp
bool ResolveBlueprintAsset(
    const FString& AssetPath,
    UBlueprint*& OutBlueprint,
    FBlueprintHelperToolError& OutError);
```

规则：

```text
1. asset_path 缺失 → ok=false / status=failed / error.code=invalid_request。
2. 资产不存在 → ok=false / status=failed / error.code=asset_not_found。
3. 资产存在但不是 UBlueprint → ok=false / status=failed / error.code=target_not_blueprint。
4. Blueprint 加载失败 → ok=false / status=failed / error.code=blueprint_load_failed。
```

这些是工具自身失败，不是蓝图编译失败。

---

## 5. Phase D：Compile service

### 5.1 新增 / 改造服务文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperCompileAssetService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperCompileAssetService.cpp
```

如果当前已有：

```text
FBlueprintHelperCompileService
```

则将其对外返回改造成本文结构。

### 5.2 服务接口

```cpp
class FBlueprintHelperCompileAssetService
{
public:
    FBlueprintHelperCompileAssetService(
        const FBlueprintHelperOwnershipService& InOwnershipService);

    FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool ParseRequest(
        const TSharedPtr<FJsonObject>& Payload,
        FBlueprintHelperCompileAssetRequest& OutRequest,
        FBlueprintHelperToolError& OutError) const;

    bool ResolveBlueprint(
        const FBlueprintHelperCompileAssetRequest& Request,
        UBlueprint*& OutBlueprint,
        FBlueprintHelperToolError& OutError) const;

    bool CompileBlueprint(
        UBlueprint* Blueprint,
        FBlueprintHelperCompileRawResult& OutRawResult,
        FBlueprintHelperToolError& OutToolError) const;

    FBlueprintHelperCompileResult BuildCompileResult(
        UBlueprint* Blueprint,
        const FBlueprintHelperCompileRawResult& RawResult) const;
};
```

### 5.3 Raw result

内部可保留完整消息，但不得默认返回给 Agent。

```cpp
struct FBlueprintHelperCompileRawMessage
{
    FString Severity; // error | warning | info
    FString Message;
    TWeakObjectPtr<UObject> SourceObject;
    TWeakObjectPtr<UEdGraphNode> Node;
};

struct FBlueprintHelperCompileRawResult
{
    bool bCompileApiSucceeded = false;
    bool bBlueprintCompileSucceeded = false;
    int32 WarningCount = 0;
    TArray<FBlueprintHelperCompileRawMessage> Messages;
};
```

---

## 6. Phase E：调用 UE 编译 API

### 6.1 推荐 API

在 Editor 模块中优先使用：

```cpp
FKismetEditorUtilities::CompileBlueprint(Blueprint);
```

编译前确认：

```cpp
if (!Blueprint)
{
    // asset_not_found / target_not_blueprint
}
```

编译后通过 Blueprint 状态判断结果：

```cpp
const bool bCompileSucceeded =
    Blueprint->Status == BS_UpToDate ||
    Blueprint->Status == BS_UpToDateWithWarnings;
```

warning 不影响 success。

### 6.2 Message collection

UE 编译消息的捕获有几种实现路径，按当前代码基础选择：

#### 方案 A：复用现有 CompileService 的消息收集

如果已有 compile service 能拿到 compiler results，应复用。

#### 方案 B：使用 Message Log 读取

可从 `FMessageLog` / compiler log 中汇总本次 compile 产生的错误与 warning。

#### 方案 C：第一版保守 fallback

如果短期无法稳定抓取所有编译消息：

```text
1. success=true 时只返回 warning_count。
2. success=false 时 markdown 至少返回一条 unmapped 错误：
   - `unmapped`: Blueprint compilation failed. See Unreal Editor Compiler Results for details.
```

但验收阶段仍应补充真正的 block_id + message 映射。

### 6.3 modified=false

compile 工具默认：

```json
"modified": false
```

注意：UE 编译可能刷新 GeneratedClass 或内部状态，但对 Agent-facing 工具语义不视为项目写操作，不生成 transaction，不写 Journal，不返回 modified=true。

---

## 7. Phase F：warning / error 语义

### 7.1 warning

warning 计数：

```cpp
WarningCount = CountMessagesBySeverity(RawResult.Messages, TEXT("warning"));
```

编译成功且 warning_count > 0：

```json
{
  "success": true,
  "status": "succeeded",
  "warning_count": 2
}
```

不返回 warning markdown。

### 7.2 compile error

如果编译流程成功执行，但蓝图有错误：

```json
{
  "ok": true,
  "status": "completed",
  "compile_result": {
    "success": false,
    "status": "failed",
    "warning_count": 1,
    "format": "markdown",
    "markdown": "## Compile Errors\n\n- `EG_PhysicsDoor_TogglePhysicsDoor0`: Cannot connect Object Reference pin to Boolean pin."
  }
}
```

不得返回：

```text
error_count
messages[]
severity
node_ref
pin_ref
graph
validation
write_ref
transaction_id
```

---

## 8. Phase G：block_id 映射

字段稿要求编译失败 Markdown 只包含：

```text
block_id + message
```

无法映射时使用：

```md
- `unmapped`: <message>
```

### 8.1 映射优先级

```text
1. 如果消息能关联到 UEdGraphNode：
   读取节点 metadata BlueprintHelperBlockId。
2. 如果消息关联到 Pin：
   通过 Pin->GetOwningNode() 读取 metadata。
3. 如果消息关联到 Graph：
   尝试查找错误 node；找不到则 unmapped。
4. 如果 SourceObject 是 Blueprint / GeneratedClass：
   unmapped。
```

### 8.2 OwnershipService 扩展

```cpp
class FBlueprintHelperOwnershipService
{
public:
    bool TryGetBlockIdForNode(
        const UEdGraphNode* Node,
        FString& OutBlockId) const;
};
```

### 8.3 Markdown 合并规则

内部可能出现多个错误映射到同一个 block_id。Markdown 输出可以逐条保留：

```md
## Compile Errors

- `EG_PhysicsDoor_TogglePhysicsDoor0`: Cannot connect Object Reference pin to Boolean pin.
- `EG_PhysicsDoor_TogglePhysicsDoor0`: Required input pin Target is not connected.
- `unmapped`: Function Foo could not be found.
```

不输出：

```text
node_ref
pin_ref
graph name
severity
compiler token id
object path
```

### 8.4 Message 清洗

编译消息进入 markdown 前应清洗：

```text
1. 移除绝对本地路径。
2. 移除长 object path，除非消息本身只有 object path 才能理解。
3. 移除重复空白。
4. 保留 UE 编译错误核心描述。
```

---

## 9. Phase H：ToolResult 构建

### 9.1 编译通过，无 warning

```cpp
FBlueprintHelperToolResultBase Result;
Result.bOk = true;
Result.Schema = TEXT("BlueprintHelper.McpToolResult.v1");
Result.Operation = TEXT("compile_blueprint_asset");
Result.Status = TEXT("completed");
Result.bModified = false;
Result.Target = MakeTarget(AssetPath);

FBlueprintHelperCompileAssetResultData Data;
Data.CompileResult.bSuccess = true;
Data.CompileResult.Status = TEXT("succeeded");
Data.CompileResult.WarningCount = 0;

Result.Data = Data.ToJson();
```

### 9.2 编译通过，有 warning

同上，只设置：

```cpp
Data.CompileResult.WarningCount = WarningCount;
```

### 9.3 编译失败

```cpp
Data.CompileResult.bSuccess = false;
Data.CompileResult.Status = TEXT("failed");
Data.CompileResult.WarningCount = WarningCount;
Data.CompileResult.Format = TEXT("markdown");
Data.CompileResult.Markdown = BuildCompileErrorMarkdown(...);
```

仍然：

```text
Result.bOk = true
Result.Status = completed
Result.bModified = false
```

### 9.4 工具自身失败

```cpp
Result.bOk = false;
Result.Status = TEXT("failed");
Result.bModified = false;
Result.Error = MakeToolError(...);
```

---

## 10. Phase I：Bridge Router 接入

### 10.1 新增 command

```text
compile_blueprint_asset
```

### 10.2 Router 分支

```cpp
if (Request.Command == TEXT("compile_blueprint_asset"))
{
    return HandleCompileBlueprintAsset(Request);
}
```

### 10.3 Handler

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleCompileBlueprintAsset(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result =
        CompileAssetService.Execute(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("compile tool failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

注意：

```text
Blueprint 编译失败时 Result.bOk=true，所以 BridgeResponse 应是 success。
```

不要把 `compile_result.success=false` 转成 Bridge error。

---

## 11. Phase J：RequestValidator / 权限

### 11.1 Validator

```cpp
if (Command == TEXT("compile_blueprint_asset"))
{
    RequireString(Payload, TEXT("asset_path"));
}
```

### 11.2 权限

compile 不是 Graph Write，也不生成 transaction。  
但它会调用 UE 编译 API，可能修改 GeneratedClass 或 transient compile state。

建议权限分类：

```text
ReadOnly：允许 compile? 需要由 Safety Profile 决定。
Conservative：允许 compile。
```

如果项目策略认为 compile 属于非持久验证动作，则可在 ReadOnly 下允许。  
如果 ReadOnly 严格禁止任何可能改变 Editor 状态的动作，则返回：

```text
ProfilePolicyViolation
```

字段稿没有定义 compile 的 safety 字段，因此本工具即便被 Profile 拦截，也只通过 `error` 表达，不返回 `safety`。

推荐第一版：

```text
compile_blueprint_asset 归入 validation command，不要求 write Token，不生成 transaction。
```

但如果现有 Bridge 将 compile 归入 write command 集合，需在 runtime profile 中保持一致，避免 Agent 认为可编译但工具被 Token 拒绝。

---

## 12. Phase K：不写 Journal / Review

compile 不写：

```text
Transaction Journal
Review Store
rollback_data
```

原因：

```text
1. compile 是验证工具。
2. 不属于用户可 Review 的资产变更。
3. 不生成 transaction_id。
4. 不需要 rollback。
```

如果需要记录最近编译结果，应放入 Diagnostics 或 transient validation cache，不进入 Transaction Journal。

---

## 13. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperCompileAssetContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperCompileAssetRuntimeTests.cpp
```

### 13.1 Contract tests

```text
1. compile_success_contract_no_warning
   - ok=true
   - operation=compile_blueprint_asset
   - status=completed
   - modified=false
   - data.schema=CompileBlueprintAsset.v1
   - compile_result.success=true
   - compile_result.status=succeeded
   - compile_result.warning_count=0
   - 不返回 validation
   - 不返回 write_ref / transaction_id

2. compile_success_contract_with_warning
   - success=true
   - warning_count>0
   - 不返回 error_count
   - 不返回 markdown

3. compile_failed_blueprint_contract
   - ok=true
   - status=completed
   - compile_result.success=false
   - compile_result.status=failed
   - compile_result.format=markdown
   - compile_result.markdown 存在
   - 不返回 error_count / messages[]

4. compile_tool_failed_contract
   - asset_not_found
   - ok=false
   - status=failed
   - error.code=asset_not_found
```

### 13.2 Runtime tests

```text
1. compile_valid_blueprint_returns_success
2. compile_blueprint_with_warning_returns_success_true
3. compile_blueprint_with_error_returns_success_false
4. compile_error_markdown_maps_owned_block_id
5. compile_error_markdown_uses_unmapped_when_no_block_id
6. compile_non_blueprint_asset_fails_tool
7. compile_missing_asset_fails_tool
```

---

## 14. 推荐提交顺序

### Commit 1：类型与 ToolResult 序列化

```text
Add CompileBlueprintAsset result types
Add compile status and compile tool error enums
Add JSON serialization without validation/write_ref
```

验收：

```text
能生成 success / failed compile_result JSON。
不会输出 error_count / messages / validation / write_ref。
```

### Commit 2：Asset resolve 与 Service skeleton

```text
Add CompileBlueprintAssetService
Parse asset_path
Resolve UBlueprint
Return tool failure for missing/non-blueprint asset
```

验收：

```text
missing asset → ok=false/status=failed/error。
non-blueprint → ok=false/status=failed/error。
```

### Commit 3：UE compile API 接入

```text
Call FKismetEditorUtilities::CompileBlueprint
Determine BS_UpToDate / BS_UpToDateWithWarnings
Return success/status/warning_count
```

验收：

```text
有效蓝图返回 ok=true/status=completed。
compile 默认 modified=false。
```

### Commit 4：编译消息采集

```text
Collect compiler warnings/errors
Count warnings
Build failed markdown
```

验收：

```text
warning 不影响 success。
compile error 输出 markdown。
```

### Commit 5：block_id 映射

```text
Map UEdGraphNode compiler messages to BlueprintHelperBlockId metadata
Fallback to unmapped
Sanitize messages
```

验收：

```text
Markdown 只包含 block_id + message。
无 block_id 时 unmapped。
```

### Commit 6：Bridge / Validator / Tests

```text
Register compile_blueprint_asset
Add payload validation
Add contract tests and runtime tests
```

验收：

```text
Bridge 层不会把 compile_result.success=false 当成工具失败。
```

---

## 15. 第一版不做的内容

```text
1. 不修复编译错误。
2. 不保存资产。
3. 不返回 full compiler message array。
4. 不返回 node_ref / pin_ref / graph。
5. 不返回 error_count。
6. 不返回 validation。
7. 不返回 write_ref / transaction_id。
8. 不写 Journal / Review。
9. 不生成 rollback_data。
10. 不自动调用 diagnostics。
```

---

## 16. 最小验收标准

编译通过：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "compile_blueprint_asset",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  },
  "data": {
    "schema": "CompileBlueprintAsset.v1",
    "compile_result": {
      "success": true,
      "status": "succeeded",
      "warning_count": 0
    }
  }
}
```

编译失败：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "compile_blueprint_asset",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  },
  "data": {
    "schema": "CompileBlueprintAsset.v1",
    "compile_result": {
      "success": false,
      "status": "failed",
      "warning_count": 1,
      "format": "markdown",
      "markdown": "## Compile Errors\n\n- `EG_PhysicsDoor_TogglePhysicsDoor0`: Cannot connect Object Reference pin to Boolean pin."
    }
  }
}
```

工具自身失败：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "compile_blueprint_asset",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_Missing"
  },
  "error": {
    "code": "asset_not_found",
    "stage": "resolve_asset",
    "message": "The requested Blueprint asset was not found.",
    "retryable": false
  }
}
```

必须不出现：

```text
validation
write_ref
transaction_id
journal_recorded
review
safety
rollback_data
journal_path
error_count
messages[]
node_ref
pin_ref
severity
```

---

## 17. 实现风险

### 17.1 UE 编译消息捕获不稳定

风险：

```text
FKismetEditorUtilities::CompileBlueprint 后不容易稳定拿到结构化 compiler messages。
```

处理：

```text
第一版允许 fallback 到 unmapped markdown。
后续补充 MessageLog / compiler results 绑定。
```

### 17.2 把蓝图编译失败误判为工具失败

风险：

```text
compile_result.success=false 被 Bridge 或 MCP 包装成 ok=false。
```

处理：

```text
Contract test 必须覆盖。
BridgeResponse success 与 compile_result.success 分离。
```

### 17.3 warning 影响 success

风险：

```text
BS_UpToDateWithWarnings 被当作 failed。
```

处理：

```text
BS_UpToDateWithWarnings 仍 success=true。
warning_count 单独返回。
```

### 17.4 block_id 映射失败

风险：

```text
错误消息没有 node source object。
```

处理：

```text
使用 unmapped。
不要输出 node_ref / pin_ref / graph 来补偿。
```
