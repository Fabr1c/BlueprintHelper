# BlueprintHelper save_asset UE 侧 C++ 可执行实现计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_SaveAsset_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、自动编译、Graph Write、Journal / Review

---

## 0. 实现目标

`save_asset` 负责保存一个明确资产。

它不负责：

```text
编译
修复
Graph Write
Component 写入
Class Settings 写入
Journal / Review
生成 transaction_id
rollback
```

字段契约核心点：

```text
1. operation 固定为 save_asset。
2. save 成功返回 data.save_result.saved / was_dirty。
3. 资产不 dirty 时返回 status=no_op / reason=asset_not_dirty。
4. 保存失败返回 error。
5. save 不返回 validation。
6. save 不返回 write_ref / transaction_id / review / safety。
7. save 默认 modified=false。
```

`modified=false` 的含义：

```text
save 本身不代表 Agent 修改资产内容。
保存只是把已有 dirty 状态落盘。
```

---

## 1. 当前依赖与复用前提

本计划假设 UE 插件侧已有或将统一具备：

```text
FBlueprintHelperToolResultBase
FBlueprintHelperToolResultBuilder
FBlueprintHelperAssetResolver / AssetBrowseService
FBlueprintHelperBridgeRouter
FBlueprintHelperRequestValidator
FBlueprintHelperConflictItem
```

如果当前已有 `SaveAsset` / `AssetBrowseService::SaveAsset`，应改造现有实现的返回体，不新增第二套保存路径。

---

## 2. Phase A：新增 / 收敛类型文件

### 2.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperSaveAssetTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperSaveAssetTypes.cpp
```

如果已有保存类型文件，则直接收敛到本文字段。

### 2.2 新增枚举

```cpp
enum class EBlueprintHelperSaveStage : uint8
{
    ParseInput,
    ResolveAsset,
    ResolvePackage,
    CheckDirty,
    SavePackage
};

enum class EBlueprintHelperSaveErrorCode : uint8
{
    InvalidRequest,
    AssetNotFound,
    AssetLoadFailed,
    PackageNotFound,
    PackageNotWritable,
    SaveFailed,
    FileLocked,
    SourceControlCheckoutFailed,
    BridgeDisconnected,
    InternalError
};
```

### 2.3 字符串序列化

稳定输出：

```text
operation = save_asset
data.schema = SaveAsset.v1
status = completed | no_op | failed
reason = asset_not_dirty
error.code = save_failed / asset_not_found / file_locked
```

不要输出 C++ 枚举原名。

---

## 3. Phase B：Agent-facing 结果结构

### 3.1 Save result data

```cpp
struct FBlueprintHelperSaveAssetResultData
{
    FString Schema = TEXT("SaveAsset.v1");
    FBlueprintHelperSaveResult SaveResult;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperSaveResult
{
    bool bSaved = false;
    bool bWasDirty = false;

    // no_op 时使用，例如 asset_not_dirty。
    TOptional<FString> Reason;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.2 成功 JSON

```json
{
  "schema": "SaveAsset.v1",
  "save_result": {
    "saved": true,
    "was_dirty": true
  }
}
```

### 3.3 no_op JSON

```json
{
  "schema": "SaveAsset.v1",
  "save_result": {
    "saved": false,
    "was_dirty": false,
    "reason": "asset_not_dirty"
  }
}
```

### 3.4 明确禁止的字段

不要在 save 结果结构中加入：

```cpp
FBlueprintHelperValidationResult Validation;
FBlueprintHelperWriteRef WriteRef;
FString TransactionId;
FString ReviewStatus;
FString SafetyProfile;
FString RollbackData;
```

save 不是 BlueprintHelper 写事务，不进入 Journal / Review。

---

## 4. Phase C：输入与目标解析

### 4.1 Request 结构

```cpp
struct FBlueprintHelperSaveAssetRequest
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

短期兼容旧字段时可接受：

```text
target_asset
assetPath
package_path
```

但 result 中统一输出：

```json
"target": {
  "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
}
```

### 4.3 Asset 解析

```cpp
bool ResolveAssetForSave(
    const FString& AssetPath,
    UObject*& OutAsset,
    UPackage*& OutPackage,
    FBlueprintHelperToolError& OutError);
```

解析规则：

```text
1. asset_path 缺失 → invalid_request。
2. 资产不存在 → asset_not_found。
3. 资产加载失败 → asset_load_failed。
4. Package 不存在 → package_not_found。
5. Package 不可写或无法解析文件名 → package_not_writable / save_failed。
```

---

## 5. Phase D：Save service

### 5.1 新增 / 改造服务

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperSaveAssetService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperSaveAssetService.cpp
```

如果已有资产保存服务，则将其输出收敛为本文结构。

### 5.2 服务接口

```cpp
class FBlueprintHelperSaveAssetService
{
public:
    FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool ParseRequest(
        const TSharedPtr<FJsonObject>& Payload,
        FBlueprintHelperSaveAssetRequest& OutRequest,
        FBlueprintHelperToolError& OutError) const;

    bool ResolveAsset(
        const FBlueprintHelperSaveAssetRequest& Request,
        UObject*& OutAsset,
        UPackage*& OutPackage,
        FBlueprintHelperToolError& OutError) const;

    bool SavePackage(
        UObject* Asset,
        UPackage* Package,
        FBlueprintHelperSaveFailureInfo& OutFailure) const;
};
```

### 5.3 Failure info

```cpp
struct FBlueprintHelperSaveFailureInfo
{
    FString Code;
    FString Stage;
    FString Message;
    bool bRetryable = false;
    TArray<FBlueprintHelperConflictItem> Conflicts;
};
```

---

## 6. Phase E：dirty 检查与 no_op

### 6.1 dirty 检查

保存前读取：

```cpp
const bool bWasDirty = Package->IsDirty();
```

如果不 dirty：

```text
ok=true
status=no_op
modified=false
save_result.saved=false
save_result.was_dirty=false
save_result.reason=asset_not_dirty
```

不要调用 SavePackage。

### 6.2 no_op result

```cpp
FBlueprintHelperToolResultBase Result;
Result.bOk = true;
Result.Operation = TEXT("save_asset");
Result.Status = TEXT("no_op");
Result.bModified = false;
Result.Target = MakeTarget(AssetPath);

FBlueprintHelperSaveAssetResultData Data;
Data.SaveResult.bSaved = false;
Data.SaveResult.bWasDirty = false;
Data.SaveResult.Reason = TEXT("asset_not_dirty");

Result.Data = Data.ToJson();
```

---

## 7. Phase F：保存实现

### 7.1 推荐 API

优先使用 Unreal Editor 资产工具链：

```cpp
UPackage* Package = Asset->GetOutermost();
FString PackageFilename;
FPackageName::TryConvertLongPackageNameToFilename(
    Package->GetName(),
    PackageFilename,
    FPackageName::GetAssetPackageExtension());

UPackage::SavePackage(
    Package,
    Asset,
    EObjectFlags::RF_Public | EObjectFlags::RF_Standalone,
    *PackageFilename,
    GError,
    nullptr,
    true,
    true,
    SAVE_None);
```

UE5.3+ 也可使用 `FSavePackageArgs`：

```cpp
FSavePackageArgs SaveArgs;
SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
SaveArgs.Error = GError;
SaveArgs.SaveFlags = SAVE_None;

const bool bSaved = UPackage::SavePackage(
    Package,
    Asset,
    *PackageFilename,
    SaveArgs);
```

根据当前项目 UE 版本统一选择，不在同一代码路径混用。

### 7.2 Editor helper 方案

如果当前已有 AssetTools / EditorFileUtils 保存路径，可以复用：

```cpp
TArray<UPackage*> PackagesToSave;
PackagesToSave.Add(Package);

const bool bSaved = FEditorFileUtils::PromptForCheckoutAndSave(
    PackagesToSave,
    false, // bCheckDirty
    false  // bPromptToSave
);
```

注意：

```text
不要弹 UI。
不要要求用户交互。
失败时返回 error。
```

若 `PromptForCheckoutAndSave` 可能弹窗，不推荐用于 Agent 工具默认路径。

### 7.3 成功 result

保存成功：

```text
ok=true
status=completed
modified=false
save_result.saved=true
save_result.was_dirty=true
```

保存后可确认：

```cpp
const bool bStillDirty = Package->IsDirty();
```

如果 SavePackage 返回 true 但仍 dirty，应按保存失败处理：

```text
error.code=save_failed
error.stage=save_package
retryable=true
```

---

## 8. Phase G：保存失败处理

保存失败是工具失败：

```text
ok=false
status=failed
modified=false
error
```

不返回：

```text
save_result
validation
write_ref
transaction_id
rollback_result
```

### 8.1 常见失败映射

| 场景 | error.code | stage | retryable |
|---|---|---|---|
| 资产不存在 | asset_not_found | resolve_asset | false |
| Package 不存在 | package_not_found | resolve_package | false |
| Package 文件名无法解析 | package_not_writable | resolve_package | false |
| 文件锁定 | save_failed 或 file_locked | save_package | true |
| Source Control checkout 失败 | source_control_checkout_failed | save_package | true |
| SavePackage 返回 false | save_failed | save_package | true |
| Bridge 异常 | bridge_disconnected | save_package | true |

### 8.2 conflict 示例

```json
"conflicts": [
  {
    "code": "file_locked",
    "message": "The asset file appears to be locked by another process."
  }
]
```

### 8.3 rollback_result

save 失败不强制返回：

```text
rollback_result
```

因为 save 不是 BlueprintHelper 写事务，也没有 rollback_data。

---

## 9. Phase H：ToolResult 构建

### 9.1 保存成功

```cpp
FBlueprintHelperToolResultBase BuildSaveSuccess(
    const FString& TraceId,
    const FString& AssetPath,
    bool bWasDirty)
{
    FBlueprintHelperToolResultBase Result;
    Result.bOk = true;
    Result.Schema = TEXT("BlueprintHelper.McpToolResult.v1");
    Result.Operation = TEXT("save_asset");
    Result.TraceId = TraceId;
    Result.Status = TEXT("completed");
    Result.bModified = false;
    Result.Target = MakeSaveTarget(AssetPath);

    FBlueprintHelperSaveAssetResultData Data;
    Data.SaveResult.bSaved = true;
    Data.SaveResult.bWasDirty = bWasDirty;

    Result.Data = Data.ToJson();
    return Result;
}
```

### 9.2 no_op

```cpp
Result.Status = TEXT("no_op");
Data.SaveResult.bSaved = false;
Data.SaveResult.bWasDirty = false;
Data.SaveResult.Reason = TEXT("asset_not_dirty");
```

### 9.3 失败

```cpp
Result.bOk = false;
Result.Status = TEXT("failed");
Result.bModified = false;
Result.Error = MakeSaveError(...);
```

---

## 10. Phase I：Bridge Router 接入

### 10.1 新增 command

```text
save_asset
```

### 10.2 Router 分支

```cpp
if (Request.Command == TEXT("save_asset"))
{
    return HandleSaveAsset(Request);
}
```

### 10.3 Handler

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleSaveAsset(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result =
        SaveAssetService.Execute(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("save_asset failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

---

## 11. Phase J：RequestValidator / 权限

### 11.1 Validator

```cpp
if (Command == TEXT("save_asset"))
{
    RequireString(Payload, TEXT("asset_path"));
}
```

### 11.2 权限分类

save 是落盘动作，不是 Journal 写事务，但它会持久化资产。

建议：

```text
ReadOnly：禁止 save。
Conservative：允许 save，但由 workflow / 用户授权控制。
Standard：允许 save。
```

如果没有 write token 或当前 Safety Profile 禁止保存：

```text
ok=false
status=failed
error.code=write_permission_disabled 或 profile_policy_violation
```

字段稿要求不返回 safety，因此用 `error` 表达策略阻断。

### 11.3 是否加入 write command 集合

建议加入“持久化写命令”集合：

```text
save_asset
```

但不要生成 transaction_id。

也就是说：

```text
需要写权限 / Token
不写 Journal
不返回 write_ref
```

---

## 12. Phase K：不写 Journal / Review

save 不写：

```text
Transaction Journal
Review Store
rollback_data
```

原因：

```text
1. save 只是持久化已存在的 dirty 修改。
2. 具体修改由前序写工具的 transaction 记录。
3. save 本身不是可 Review 的逻辑变更。
4. save 不需要 rollback。
```

如果将来需要审计“保存动作”，应进入轻量 activity log，而不是 Transaction Journal。

---

## 13. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperSaveAssetContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperSaveAssetRuntimeTests.cpp
```

### 13.1 Contract tests

```text
1. save_success_contract
   - ok=true
   - operation=save_asset
   - status=completed
   - modified=false
   - data.schema=SaveAsset.v1
   - save_result.saved=true
   - save_result.was_dirty=true
   - 不返回 validation
   - 不返回 write_ref / transaction_id

2. save_no_op_contract
   - ok=true
   - status=no_op
   - modified=false
   - save_result.saved=false
   - save_result.was_dirty=false
   - save_result.reason=asset_not_dirty

3. save_failed_contract
   - ok=false
   - status=failed
   - error.code 存在
   - 不返回 save_result
   - 不返回 rollback_result
```

### 13.2 Runtime tests

```text
1. save_dirty_blueprint_returns_saved_true
2. save_clean_blueprint_returns_no_op
3. save_missing_asset_returns_asset_not_found
4. save_non_writable_package_returns_failed
5. save_result_never_returns_validation
6. save_result_never_returns_write_ref
7. save_does_not_write_transaction_journal
```

---

## 14. 推荐提交顺序

### Commit 1：类型与序列化

```text
Add SaveAsset result types
Add save stage / error enums
Add JSON serialization without validation/write_ref
```

验收：

```text
能生成 completed / no_op / failed 三类返回。
不会输出 validation / write_ref / transaction_id。
```

### Commit 2：Asset resolve

```text
Add SaveAssetService skeleton
Parse asset_path
Resolve UObject and UPackage
Return asset_not_found / package_not_found errors
```

验收：

```text
missing asset 正确 ok=false。
target 输出 asset_path。
```

### Commit 3：Dirty/no_op

```text
Check Package->IsDirty
Return no_op for clean asset
```

验收：

```text
clean asset 不调用 SavePackage。
返回 reason=asset_not_dirty。
```

### Commit 4：SavePackage

```text
Implement non-interactive package save
Map SavePackage failure to error
Confirm package no longer dirty after success
```

验收：

```text
dirty asset 保存后 status=completed。
保存失败 status=failed。
```

### Commit 5：Bridge / Validator / Auth

```text
Register save_asset command
Add payload validation
Add persistent-write permission gate
Add tests
```

验收：

```text
ReadOnly 下 save 被策略阻止。
Conservative 下有权限时可保存。
```

### Commit 6：Protocol regression

```text
Add contract tests preventing validation/write_ref/transaction_id in save responses
Ensure save does not write Transaction Journal
```

验收：

```text
save 结果符合字段稿。
```

---

## 15. 第一版不做的内容

```text
1. 不编译。
2. 不修复。
3. 不保存多个资产。
4. 不递归保存依赖。
5. 不写 Journal / Review。
6. 不生成 transaction_id。
7. 不返回 validation。
8. 不返回 write_ref。
9. 不返回 source control 详细对象。
10. 不弹出任何用户交互 UI。
```

---

## 16. 最小验收标准

保存成功：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "save_asset",
  "status": "completed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  },
  "data": {
    "schema": "SaveAsset.v1",
    "save_result": {
      "saved": true,
      "was_dirty": true
    }
  }
}
```

no_op：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "save_asset",
  "status": "no_op",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  },
  "data": {
    "schema": "SaveAsset.v1",
    "save_result": {
      "saved": false,
      "was_dirty": false,
      "reason": "asset_not_dirty"
    }
  }
}
```

失败：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "save_asset",
  "status": "failed",
  "modified": false,
  "target": {
    "asset_path": "/Game/BlueprintHelperTest/Door/BP_BH_PhysicsDoor"
  },
  "error": {
    "code": "save_failed",
    "stage": "save_package",
    "message": "The asset package could not be saved.",
    "retryable": true
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
rollback_result
```

---

## 17. 实现风险

### 17.1 SavePackage API 版本差异

风险：

```text
UE5.3+ SavePackage 推荐使用 FSavePackageArgs。
```

处理：

```text
按当前项目最低 UE 版本封装一层 SavePackageCompat。
```

### 17.2 保存接口弹 UI

风险：

```text
FEditorFileUtils::PromptForCheckoutAndSave 可能弹出交互 UI。
```

处理：

```text
默认使用非交互 SavePackage。
若使用 EditorFileUtils，必须确保 bPromptToSave=false 且不阻塞 Agent。
```

### 17.3 保存成功但 Package 仍 dirty

风险：

```text
API 返回 true 但资产仍 dirty。
```

处理：

```text
二次检查 Package->IsDirty。
仍 dirty 时返回 save_failed。
```

### 17.4 权限语义混淆

风险：

```text
save 不生成 transaction，但仍是持久化动作。
```

处理：

```text
save 进入持久化写权限 gate。
但不写 Journal，不返回 write_ref。
```
