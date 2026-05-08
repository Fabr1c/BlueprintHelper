# BlueprintHelper Editor Lifecycle / PIE / Risk Command UE 侧 C++ 可执行实现计划

状态：[x] 已完成
日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_EditorLifecycle_RiskCommand_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、切换地图、重启 UE、Cook / Build / Package、Editor Preferences / Project Settings 修改

---

## 0. 实现目标

实现 Editor Lifecycle / PIE / Risk Command 第一版工具簇：

```text
get_editor_lifecycle_status
start_pie_session
stop_pie_session
close_editor
```

该簇负责：

```text
启动 PIE
停止 PIE
在明确授权下请求关闭编辑器
```

该簇不负责：

```text
保存资产
编译资产
修复编译错误
切换地图
修改 PIE 设置
重启编辑器
修改 Editor Preferences
修改 Project Settings
写 Transaction Journal
写 Review
```

字段契约核心点：

```text
1. get_editor_lifecycle_status 是只读工具，modified=false。
2. start_pie_session 成功返回 started / already_running。
3. start_pie_session 不负责 compile/save。
4. start_pie_session 编译错误阻断时通过 error.conflicts 返回。
5. stop_pie_session 成功返回 stopped / was_running。
6. stop_pie_session 未运行时返回 no_op / reason=pie_not_running。
7. close_editor 必须用户明确要求，Agent 不得自动调用。
8. close_editor 必须受 risk_command 保护。
9. close_editor 必须 dry_run。
10. close_editor dry_run blocked 返回 risk_command_missing / unsaved_assets_exist 等冲突。
11. close_editor 成功只返回 close_requested。
12. 本簇所有工具 modified=false。
13. 本簇所有工具不返回 validation / write_ref / transaction_id / review / safety。
14. 所有 data.schema 使用短命名。
```

---

## 1. UE 模块依赖

确认 `BlueprintHelper.Build.cs` 至少包含：

```text
Core
CoreUObject
Engine
UnrealEd
LevelEditor
Slate
SlateCore
EditorSubsystem
Projects
```

可能需要的头：

```cpp
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Kismet2/DebuggerCommands.h"
#include "PlayWorldCommandCallbacks.h"
#include "FileHelpers.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "HAL/PlatformMisc.h"
```

PIE 启停 API 在不同 UE 版本存在差异。建议封装 `FBlueprintHelperEditorLifecycleCompat`，避免服务层直接散落引擎 API 判断。

---

## 2. Phase A：新增类型文件

### 2.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperEditorLifecycleTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperEditorLifecycleTypes.cpp
```

### 2.2 新增枚举

```cpp
enum class EBlueprintHelperLifecycleScope : uint8
{
    Editor,
    Pie
};

enum class EBlueprintHelperLifecycleReadScope : uint8
{
    EditorLifecycle
};

enum class EBlueprintHelperLifecycleStage : uint8
{
    ParseInput,
    ReadLifecycleStatus,
    CheckPieStatus,
    CheckUnsavedAssets,
    AuthorizeCommand,
    DryRun,
    StartPie,
    StopPie,
    RequestCloseEditor
};

enum class EBlueprintHelperLifecycleErrorCode : uint8
{
    InvalidRequest,
    EditorUnavailable,
    PieStartFailed,
    PieStopFailed,
    BlueprintCompileErrorsExist,
    RiskCommandMissing,
    RiskCommandInvalid,
    CommandNotAuthorized,
    UnsavedAssetsExist,
    CloseEditorDryRunRequired,
    CloseEditorFailed,
    InternalError
};
```

### 2.3 字符串序列化

稳定输出：

```text
editor
pie
editor_lifecycle
pie_start_failed
pie_stop_failed
blueprint_compile_errors_exist
risk_command_missing
unsaved_assets_exist
pie_not_running
```

不要输出 C++ enum 原名。

---

## 3. Phase B：Agent-facing DTO

### 3.1 get_editor_lifecycle_status DTO

```cpp
struct FBlueprintHelperEditorLifecycleStatusData
{
    FString Schema = TEXT("EditorLifecycleStatus.v1");
    FBlueprintHelperEditorLifecycleStatus EditorLifecycle;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperEditorLifecycleStatus
{
    bool bEditorRunning = false;
    bool bPieRunning = false;
    int32 UnsavedAssetCount = 0;

    TSharedRef<FJsonObject> ToJson() const;
};
```

输出：

```json
{
  "schema": "EditorLifecycleStatus.v1",
  "editor_lifecycle": {
    "editor_running": true,
    "pie_running": false,
    "unsaved_asset_count": 2
  }
}
```

### 3.2 start_pie_session / stop_pie_session DTO

```cpp
struct FBlueprintHelperPieResultData
{
    FString Schema; // StartPieSession.v1 or StopPieSession.v1
    FBlueprintHelperPieResult PieResult;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperPieResult
{
    TOptional<bool> bStarted;
    TOptional<bool> bAlreadyRunning;

    TOptional<bool> bStopped;
    TOptional<bool> bWasRunning;

    TOptional<FString> Reason;

    TSharedRef<FJsonObject> ToJson() const;
};
```

Start 成功：

```json
{
  "schema": "StartPieSession.v1",
  "pie_result": {
    "started": true,
    "already_running": false
  }
}
```

Stop no_op：

```json
{
  "schema": "StopPieSession.v1",
  "pie_result": {
    "stopped": false,
    "was_running": false,
    "reason": "pie_not_running"
  }
}
```

### 3.3 close_editor DTO

```cpp
struct FBlueprintHelperCloseEditorResultData
{
    FString Schema = TEXT("CloseEditor.v1");
    FBlueprintHelperCloseEditorResult CloseResult;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperCloseEditorResult
{
    bool bCloseRequested = false;

    // no_op only
    TOptional<bool> bAlreadyClosing;

    TSharedRef<FJsonObject> ToJson() const;
};
```

成功：

```json
{
  "schema": "CloseEditor.v1",
  "close_result": {
    "close_requested": true
  }
}
```

### 3.4 close_editor dry_run DTO

复用通用 `FBlueprintHelperDryRunResult`：

```cpp
struct FBlueprintHelperCloseEditorDryRunData
{
    FString Schema = TEXT("CloseEditorDryRun.v1");
    FBlueprintHelperDryRunResult DryRun;

    TSharedRef<FJsonObject> ToJson() const;
};
```

dry_run passed：

```json
{
  "schema": "CloseEditorDryRun.v1",
  "dry_run": {
    "result": "passed",
    "can_execute": true
  }
}
```

dry_run blocked：

```json
{
  "schema": "CloseEditorDryRun.v1",
  "dry_run": {
    "result": "blocked",
    "can_execute": false,
    "blocked_by": [],
    "conflicts": [],
    "errors": []
  }
}
```

### 3.5 明确禁止字段

所有生命周期工具 DTO 均不包含：

```cpp
FBlueprintHelperValidationResult
FBlueprintHelperWriteRef
FString TransactionId
FString ReviewStatus
FString SafetyProfile
FString JournalPath
```

---

## 4. Phase C：EditorLifecycleService

### 4.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperEditorLifecycleService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperEditorLifecycleService.cpp
```

### 4.2 服务接口

```cpp
class FBlueprintHelperEditorLifecycleService
{
public:
    FBlueprintHelperToolResultBase GetEditorLifecycleStatus(
        const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase StartPieSession(
        const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase StopPieSession(
        const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase CloseEditor(
        const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool IsPieRunning() const;
    int32 GetUnsavedAssetCount() const;

    bool IsCloseEditorAuthorized(FBlueprintHelperToolError& OutError) const;
    bool BuildCloseEditorDryRun(
        FBlueprintHelperDryRunResult& OutDryRun) const;
};
```

---

## 5. Phase D：Lifecycle compat helper

### 5.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperEditorLifecycleCompat.h
Source/BlueprintHelper/Private/Services/BlueprintHelperEditorLifecycleCompat.cpp
```

### 5.2 接口

```cpp
class FBlueprintHelperEditorLifecycleCompat
{
public:
    static bool IsEditorRunning();
    static bool IsPieRunning();
    static bool StartPie(FString& OutError);
    static bool StopPie(FString& OutError);
    static bool RequestCloseEditor(FString& OutError);
    static int32 CountUnsavedAssets();

    static bool IsEditorAlreadyClosing();
};
```


优先：

```cpp
GEditor && GEditor->PlayWorld != nullptr
```

也可结合：

```cpp
GEditor->IsPlayingSessionInEditor()
```

根据 UE5.3 实际 API 校验。

### 5.4 Unsaved asset count

可使用：

```cpp
TArray<UPackage*> DirtyPackages;
FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);
FEditorFileUtils::GetDirtyWorldPackages(DirtyWorldPackages);
```

或扫描 loaded packages：

```cpp
ForEachObjectOfClass(UPackage::StaticClass(), ...)
Package->IsDirty()
```

推荐第一版：

```text
Content dirty packages + World dirty packages
```

返回计数，不返回路径列表，避免上下文膨胀。

---

## 6. Phase E：get_editor_lifecycle_status 实现

### 6.1 Request

无必填参数：

```json
{}
```

### 6.2 执行流程

```text
1. 检查 GEditor 是否有效。
2. 读取 editor_running。
3. 读取 pie_running。
4. 统计 unsaved_asset_count。
5. 返回 EditorLifecycleStatus.v1。
```

### 6.3 成功 ToolResult

```cpp
Result.bOk = true;
Result.Operation = TEXT("get_editor_lifecycle_status");
Result.Status = TEXT("completed");
Result.bModified = false;
Result.Target = MakeReadScopeTarget(TEXT("editor_lifecycle"));

FBlueprintHelperEditorLifecycleStatusData Data;
Data.EditorLifecycle.bEditorRunning = FBlueprintHelperEditorLifecycleCompat::IsEditorRunning();
Data.EditorLifecycle.bPieRunning = FBlueprintHelperEditorLifecycleCompat::IsPieRunning();
Data.EditorLifecycle.UnsavedAssetCount = FBlueprintHelperEditorLifecycleCompat::CountUnsavedAssets();
Result.Data = Data.ToJson();
```

### 6.4 失败

如果 GEditor 不可用：

```text
ok=false
status=failed
error.code=editor_unavailable
stage=read_lifecycle_status
retryable=true
```

---

## 7. Phase F：start_pie_session 实现

### 7.1 工具边界

`start_pie_session` 不做：

```text
compile
save
map switch
PIE setting mutation
```

如果蓝图有编译错误，应由前序 `compile_blueprint_asset` 暴露；start PIE 失败时只能将 UE 返回的阻断转成 `error.conflicts`。

### 7.2 Request

第一版无必填参数：

```json
{}
```

后续可扩展：

```text
map
play_mode
```

但当前字段稿不定义，第一版不实现。

### 7.3 no_op

如果 PIE 已运行：

```text
ok=true
status=no_op
modified=false
schema=StartPieSession.v1
pie_result.started=false
pie_result.already_running=true
```

### 7.4 启动 PIE

可用 API 方案按项目实际验证：

方案 A：Editor request play session：

```cpp
FRequestPlaySessionParams Params;
GEditor->RequestPlaySession(Params);
```

方案 B：调用 LevelEditor play command 回调。

推荐封装在 `FBlueprintHelperEditorLifecycleCompat::StartPie()` 中，避免服务层依赖具体引擎 API。

### 7.5 成功返回

```json
{
  "schema": "StartPieSession.v1",
  "pie_result": {
    "started": true,
    "already_running": false
  }
}
```

ToolResult：

```text
ok=true
status=completed
modified=false
target.lifecycle_scope=pie
```

### 7.6 失败映射

如果 UE 返回启动失败：

```text
error.code=pie_start_failed
stage=start_pie
retryable=true
```

如果可识别编译错误阻断：

```json
"conflicts": [
  {
    "code": "blueprint_compile_errors_exist",
    "message": "One or more Blueprint assets still have compile errors."
  }
]
```

不要在 start_pie 中返回 compile markdown。编译错误细节来自 `compile_blueprint_asset`。

---

## 8. Phase G：stop_pie_session 实现

### 8.1 Request

无必填参数：

```json
{}
```

### 8.2 no_op

如果 PIE 未运行：

```text
ok=true
status=no_op
modified=false
schema=StopPieSession.v1
pie_result.stopped=false
pie_result.was_running=false
pie_result.reason=pie_not_running
```

### 8.3 停止 PIE

封装：

```cpp
FBlueprintHelperEditorLifecycleCompat::StopPie(OutError);
```

可能实现：

```cpp
GEditor->RequestEndPlayMap();
```

或使用 UE 当前版本的 stop play session API。

### 8.4 成功返回

```text
ok=true
status=completed
modified=false
schema=StopPieSession.v1
pie_result.stopped=true
pie_result.was_running=true
```

### 8.5 失败

```text
ok=false
status=failed
modified=false
error.code=pie_stop_failed
stage=stop_pie
retryable=true
```

---

## 9. Phase H：risk_command 服务

### 9.1 新增 / 复用 RiskCommandService

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperRiskCommandService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperRiskCommandService.cpp
```

### 9.2 接口

```cpp
class FBlueprintHelperRiskCommandService
{
public:
    bool IsRiskCommandEnabled() const;
    bool IsCommandAuthorized(const FString& CommandName) const;
    FString GetDisabledReason() const;
    TArray<FString> GetBlockedCommands() const;
};
```

### 9.3 close_editor 授权要求

必须满足：

```text
risk_command enabled
close_editor authorized
用户明确请求 close_editor
```

“用户明确请求”由 Agent / MCP 调用上下文保障。UE 层可以要求 payload 包含：

```json
{
  "confirm_intent": "close_editor"
}
```

字段稿没有定义该字段，但为了防止误调用，建议第一版实现：

```text
正式 close_editor 需要 confirm_intent=close_editor。
dry_run 可不需要。
```

若担心破坏字段稿，可将该字段作为输入层安全参数，不进入 Agent-facing 返回。

---

## 10. Phase I：close_editor dry_run

### 10.1 Request

```cpp
struct FBlueprintHelperCloseEditorRequest
{
    bool bDryRun = false;
    FString ConfirmIntent;
    FString UnsavedAssetPolicy; // optional: block | allow, default block
};
```

第一版策略：

```text
unsaved_asset_policy 默认 block。
不自动保存。
不自动 discard。
```

### 10.2 dry_run checks

```text
1. risk_command 是否启用。
2. close_editor 是否授权。
3. 当前是否已经处于 closing。
4. unsaved_asset_count 是否为 0。
5. PIE 是否运行；可选择阻断或先要求 stop_pie_session。
```

建议第一版：

```text
PIE running 时 blocked=pie_running。
unsaved_asset_count > 0 时 blocked=unsaved_assets_exist。
risk_command missing 时 blocked=risk_command_missing。
```

### 10.3 dry_run passed

```json
{
  "schema": "CloseEditorDryRun.v1",
  "dry_run": {
    "result": "passed",
    "can_execute": true
  }
}
```

### 10.4 dry_run blocked

`blocked_by` 可包含：

```text
risk_command_missing
command_not_authorized
unsaved_assets_exist
pie_running
editor_already_closing
```

conflict 示例：

```json
{
  "code": "unsaved_assets_exist",
  "unsaved_asset_count": 3,
  "message": "There are unsaved assets. Close policy does not allow closing with unsaved changes."
}
```

不要返回资产路径列表，只返回 count，避免膨胀和隐私泄露。

---

## 11. Phase J：close_editor 正式执行

### 11.1 强制 preflight

正式执行时必须重复 dry_run 等价检查，不能只信任旧 dry_run。

```text
1. risk_command enabled。
2. command authorized。
3. confirm_intent=close_editor。
4. unsaved assets policy 允许。
5. PIE 未运行或策略允许。
```

### 11.2 no_op：already closing

如果编辑器已经处于关闭流程：

```text
ok=true
status=no_op
modified=false
schema=CloseEditor.v1
close_result.close_requested=false
close_result.already_closing=true
```

### 11.3 请求关闭

推荐封装：

```cpp
FBlueprintHelperEditorLifecycleCompat::RequestCloseEditor(OutError);
```

可能实现：

```cpp
FPlatformMisc::RequestExit(false);
```

或使用 Editor 关闭 API。必须避免弹交互窗口导致 Agent 阻塞。若存在 unsaved assets，则应在 dry_run/preflight 中阻断，而不是依赖 UE 关闭确认弹窗。

### 11.4 成功返回

```json
{
  "schema": "CloseEditor.v1",
  "close_result": {
    "close_requested": true
  }
}
```

ToolResult：

```text
ok=true
status=completed
modified=false
target.lifecycle_scope=editor
```

### 11.5 正式失败

示例：

```text
risk_command_missing
command_not_authorized
unsaved_assets_exist
close_editor_failed
```

不返回：

```text
validation
write_ref
transaction_id
safety
```

虽然 close_editor 受 safety/risk 保护，但失败信息通过 `error` 和 `conflicts` 表达，不返回 `safety` 对象。

---

## 12. Phase K：ToolResult 构建

### 12.1 lifecycle status

```cpp
FBlueprintHelperToolResultBase Result;
Result.bOk = true;
Result.Operation = TEXT("get_editor_lifecycle_status");
Result.Status = TEXT("completed");
Result.bModified = false;
Result.Target = MakeReadScopeTarget(TEXT("editor_lifecycle"));
Result.Data = Data.ToJson();
```

### 12.2 PIE success / no_op

```cpp
Result.Operation = TEXT("start_pie_session");
Result.Status = bAlreadyRunning ? TEXT("no_op") : TEXT("completed");
Result.bModified = false;
Result.Target = MakeLifecycleScopeTarget(TEXT("pie"));
```

### 12.3 close dry_run

```cpp
Result.Operation = TEXT("close_editor");
Result.Status = TEXT("dry_run");
Result.bModified = false;
Result.Target = MakeLifecycleScopeTarget(TEXT("editor"));
Result.Data = DryRunData.ToJson();
```

### 12.4 failure

```cpp
Result.bOk = false;
Result.Status = TEXT("failed");
Result.bModified = false;
Result.Error = MakeLifecycleError(...);
```

---

## 13. Phase L：Bridge Router 接入

### 13.1 新增 commands

```text
get_editor_lifecycle_status
start_pie_session
stop_pie_session
close_editor
```

### 13.2 Router 分支

```cpp
if (Request.Command == TEXT("get_editor_lifecycle_status"))
{
    return HandleGetEditorLifecycleStatus(Request);
}
if (Request.Command == TEXT("start_pie_session"))
{
    return HandleStartPieSession(Request);
}
if (Request.Command == TEXT("stop_pie_session"))
{
    return HandleStopPieSession(Request);
}
if (Request.Command == TEXT("close_editor"))
{
    return HandleCloseEditor(Request);
}
```

### 13.3 Handler 模板

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleCloseEditor(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result =
        EditorLifecycleService.CloseEditor(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("close_editor failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

---

## 14. Phase M：RequestValidator / 权限

### 14.1 get_editor_lifecycle_status

```cpp
// Empty payload allowed.
```

### 14.2 start_pie_session / stop_pie_session

```cpp
// Empty payload allowed in v1.
```

后续扩展参数暂不支持：

```text
map
play_mode
viewport_mode
```

传入未知高风险字段应返回 invalid_request，避免用户误以为已生效。

### 14.3 close_editor

```cpp
OptionalBool(Payload, TEXT("dry_run"));
OptionalString(Payload, TEXT("confirm_intent"));
OptionalString(Payload, TEXT("unsaved_asset_policy"));
```

正式执行建议要求：

```text
dry_run=false
confirm_intent=close_editor
```

### 14.4 权限分类

只读：

```text
get_editor_lifecycle_status
```

UI / lifecycle：

```text
start_pie_session
stop_pie_session
```

风险命令：

```text
close_editor
```

权限建议：

```text
get_editor_lifecycle_status：ReadOnly 允许，不需要 token。
start_pie_session：不需要写资产 token，但受 lifecycle command policy 约束。
stop_pie_session：不需要写资产 token，但受 lifecycle command policy 约束。
close_editor：必须 risk_command 授权，不使用普通 write token 替代。
```

---

## 15. Phase N：Runtime Profile 集成

`get_runtime_profile` 中应暴露 risk command 异常：

```json
"risk_command": {
  "enabled": false,
  "reason": "risk_command_missing",
  "blocked_commands": ["close_editor"]
}
```

同时 unavailable：

```json
{
  "cluster": "lifecycle",
  "capability": "close_editor",
  "status": "blocked",
  "reason": "risk_command_missing"
}
```


```text
runtime_profile.status=degraded
```

而不是 blocked，除非当前任务就是 close_editor。

---

## 16. Phase O：不写 Journal / Review

本簇不写：

```text
Transaction Journal
Review Store
rollback_data
```

原因：

```text
1. 生命周期操作不是资产内容变更。
2. start/stop PIE 不需要审计 diff。
3. close_editor 是风险命令，授权由 risk_command 负责，不通过 Review 处理。
```

可选：内部 transient activity log 记录 close request，但不得作为 Transaction Journal 暴露。

---

## 17. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperEditorLifecycleContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperEditorLifecycleRuntimeTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperRiskCommandTests.cpp
```

### 17.1 Contract tests

```text
1. lifecycle_status_contract
   - operation=get_editor_lifecycle_status
   - data.schema=EditorLifecycleStatus.v1
   - editor_running / pie_running / unsaved_asset_count
   - modified=false
   - 不返回 validation / write_ref / transaction_id

2. start_pie_success_contract
   - operation=start_pie_session
   - data.schema=StartPieSession.v1
   - pie_result.started/already_running
   - modified=false

3. start_pie_no_op_contract
   - status=no_op
   - started=false
   - already_running=true

4. stop_pie_success_contract
   - data.schema=StopPieSession.v1
   - stopped=true
   - was_running=true

5. stop_pie_no_op_contract
   - status=no_op
   - stopped=false
   - was_running=false
   - reason=pie_not_running

6. close_editor_dry_run_passed_contract
   - status=dry_run
   - data.schema=CloseEditorDryRun.v1
   - result/can_execute only

7. close_editor_dry_run_blocked_contract
   - blocked_by/conflicts/errors
   - risk_command_missing 或 unsaved_assets_exist

8. close_editor_success_contract
   - data.schema=CloseEditor.v1
   - close_requested=true
   - modified=false
   - 不返回 validation / safety
```

### 17.2 Runtime tests

```text
1. lifecycle_status_counts_unsaved_assets
2. start_pie_when_not_running
3. start_pie_when_already_running_no_op
4. stop_pie_when_running
5. stop_pie_when_not_running_no_op
6. close_editor_blocked_without_risk_command
7. close_editor_blocked_with_unsaved_assets
8. close_editor_requires_confirm_intent
9. close_editor_does_not_write_journal
```

### 17.3 Policy tests

```text
1. close_editor_not_authorized_by_write_token
2. close_editor_requires_risk_command
3. runtime_profile_reports_close_editor_blocked_as_lifecycle_unavailable
4. close_editor_not_auto_called_by_task_completion
```

---

## 18. 推荐提交顺序

### Commit 1：DTO 与序列化

```text
Add Editor Lifecycle DTOs
Add lifecycle scope / stage / error enums
Add short schemas
```

验收：

```text
所有返回不包含 validation/write_ref/transaction_id。
```

### Commit 2：Lifecycle compat helper

```text
Add EditorLifecycleCompat
Implement IsPieRunning
Implement CountUnsavedAssets
Implement StartPie / StopPie wrappers
```

验收：

```text
PIE API 与 UE5.3 编译通过。
```

### Commit 3：get_editor_lifecycle_status

```text
Implement status service
Return editor_running / pie_running / unsaved_asset_count
```

验收：

```text
ReadOnly 可调用。
modified=false。
```

### Commit 4：start / stop PIE

```text
Implement start_pie_session
Implement stop_pie_session
Handle no_op states
Map failures to error.conflicts
```

验收：

```text
PIE 已运行/未运行 no_op 语义正确。
start 不做 compile/save。
```

### Commit 5：RiskCommandService

```text
Implement risk command status
Authorize close_editor separately from write token
Expose blocked commands to runtime_profile
```

验收：

```text
close_editor 无 risk_command 时 blocked。
```

### Commit 6：close_editor dry_run

```text
Implement close_editor dry_run
Check risk command
Check unsaved assets
Check PIE running
Return blocked conflicts
```

验收：

```text
dry_run passed 极简。
blocked 返回 risk_command_missing/unsaved_assets_exist。
```

### Commit 7：close_editor execute

```text
Implement formal close request
Require confirm_intent
Repeat preflight
Return close_requested only
```

验收：

```text
成功只返回 close_requested。
不写 Journal。
```

### Commit 8：Bridge / Validator / Runtime Profile integration

```text
Register lifecycle commands
Add validators
Classify risk command
Update runtime_profile unavailable for close_editor
```

验收：

```text
close_editor 不被普通 write token 授权。
```

### Commit 9：Protocol regression tests

```text
Add contract tests
Add no validation/write_ref/safety tests
Add risk_command policy tests
```

验收：

```text
字段稿验收项全部通过。
```

---

## 19. 第一版不做的内容

```text
1. 不切换地图。
2. 不重启 UE 编辑器。
3. 不运行 Cook / Build / Package。
4. 不修改 PIE 设置。
5. 不修改 Editor Preferences。
6. 不修改 Project Settings。
7. start_pie 不自动 compile。
8. start_pie 不自动 save。
9. close_editor 不自动保存未保存资产。
10. close_editor 不自动丢弃未保存资产。
11. close_editor 不接受普通 write token 替代 risk_command。
12. 不写 Transaction Journal / Review。
```

---

## 20. 最小验收标准

lifecycle status：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_editor_lifecycle_status",
  "status": "completed",
  "modified": false,
  "target": {
    "read_scope": "editor_lifecycle"
  },
  "data": {
    "schema": "EditorLifecycleStatus.v1",
    "editor_lifecycle": {
      "editor_running": true,
      "pie_running": false,
      "unsaved_asset_count": 2
    }
  }
}
```

start PIE：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "start_pie_session",
  "status": "completed",
  "modified": false,
  "target": {
    "lifecycle_scope": "pie"
  },
  "data": {
    "schema": "StartPieSession.v1",
    "pie_result": {
      "started": true,
      "already_running": false
    }
  }
}
```

stop PIE no_op：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "stop_pie_session",
  "status": "no_op",
  "modified": false,
  "target": {
    "lifecycle_scope": "pie"
  },
  "data": {
    "schema": "StopPieSession.v1",
    "pie_result": {
      "stopped": false,
      "was_running": false,
      "reason": "pie_not_running"
    }
  }
}
```

close dry_run blocked：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "close_editor",
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
          "command": "close_editor",
          "message": "close_editor requires risk_command authorization."
        }
      ],
      "errors": []
    }
  }
}
```

close success：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "close_editor",
  "status": "completed",
  "modified": false,
  "target": {
    "lifecycle_scope": "editor"
  },
  "data": {
    "schema": "CloseEditor.v1",
    "close_result": {
      "close_requested": true
    }
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
modified=true
```

---

## 21. 实现风险

### 21.1 PIE API 版本差异

风险：

```text
UE5.3+ PIE 启停 API 与示例不同。
```

处理：

```text
集中封装 EditorLifecycleCompat。
以引擎源码编译校验。
Runtime tests 覆盖 PIE 启停。
```

### 21.2 start_pie 偷偷承担 compile/save

风险：

```text
Agent 直接 start PIE，工具内部自动编译保存，破坏工具边界。
```

处理：

```text
start_pie 不调用 compile/save。
编译错误只作为 error.conflicts 返回。
```

### 21.3 close_editor 被普通写权限授权

风险：

```text
持有 write token 就能关编辑器。
```

处理：

```text
close_editor 必须走 risk_command。
write token 不可替代。
Policy test 覆盖。
```

### 21.4 close_editor 因未保存资产弹窗阻塞

风险：

```text
UE 关闭流程弹出保存确认，Agent 阻塞。
```

处理：

```text
dry_run/preflight 检查 unsaved_asset_count。
默认 unsaved_asset_policy=block。
不自动保存或丢弃。
```

### 21.5 modified 语义误用

风险：

```text
start/stop PIE 或 close_editor 修改 Editor runtime state，被返回 modified=true。
```

处理：

```text
本簇 modified=false 固定。
Contract test 锁定。
