# BlueprintHelper Project Context / Project Marker / Setup State UE 侧 C++ 可执行实现计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_ProjectContext_SetupState_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++ + MCP runtime 只读聚合层  
不包含：MCP Server TypeScript 封装、Agent Skill 文档、Project Marker 写入 / 修复、settings.json 修复 / 迁移、`/blueprinthelper-setup` 实现

---

## 0. 实现目标

实现 Project Context / Project Marker / Setup State 第一版只读工具簇：

```text
read_project_context
check_project_marker
check_setup_state
```

该工具簇用于：

```text
识别当前是否处于 UE 项目上下文
检查 BlueprintHelper Project Marker 是否存在且有效
检查 BlueprintHelper setup/runtime 配置是否可用
向 Agent 提供最小工作流状态
```

它不用于：

```text
写入 Project Marker
修复 Project Marker
删除 Project Marker
运行 setup
迁移 settings.json
修复 settings.json
写全局 CLAUDE.md managed block
读取完整 CLAUDE.md / settings.json / AgentGuide
```

字段契约核心点：

```text
1. read_project_context 返回 project_context.status / project_detected / project_marker / workflow_enabled。
2. read_project_context 不返回 project_root / uproject_path / settings_path / CLAUDE.md 全文。
3. check_project_marker 返回 project_marker.status / workflow_enabled / reason。
4. check_project_marker 不返回 marker 文本 / 文件路径 / 行号。
5. check_setup_state 正常态只返回 setup_state.status=ok。
6. check_setup_state 异常态返回 status / reason。
7. check_setup_state 不展开 settings 具体损坏类型。
8. 本簇不负责 write_project_marker / repair_project_marker / setup migration。
9. Project Marker 写入仍需用户确认，不能静默执行。
10. 本簇所有工具 modified=false。
11. 本簇所有工具不返回 validation / write_ref / transaction_id / review / safety。
12. 所有 data.schema 使用短命名。
```

---

## 1. 边界与职责划分

### 1.1 本簇只读

三个工具都必须：

```text
modified=false
不写文件
不改 settings.json
不改 CLAUDE.md
不写 Transaction Journal
不写 Review Store
不生成 transaction_id
```

### 1.2 与 runtime_profile 的关系

`get_runtime_profile` 负责当前运行链路状态：

```text
Bridge
write_permission / Token
risk_command
config_status
unavailable capabilities
```

本簇负责项目/marker/setup 的最小只读状态：

```text
project_detected
project_marker
workflow_enabled
setup_state
```

不要把 runtime_profile 的所有字段复制到 `read_project_context`。

### 1.3 与 diagnostics 的关系

`diagnostics` 返回 Markdown 报告，用于安装、配置和运行链路排查。

本簇返回机器可读极简状态：

```text
ok / degraded / blocked
reason enum
```

不要返回 diagnostics Markdown，也不要展开 settings 字段级错误。

---

## 2. UE 模块依赖

确认 `BlueprintHelper.Build.cs` 至少包含：

```text
Core
CoreUObject
Engine
UnrealEd
Projects
Json
JsonUtilities
```

如果 Project Marker 检查需要读取项目根目录 `CLAUDE.md`，需要文件系统访问：

```cpp
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
```

注意：虽然内部需要读取路径，但 Agent-facing 结果不得返回本地绝对路径。

---

## 3. Phase A：新增类型文件

### 3.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperProjectContextTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperProjectContextTypes.cpp
```

### 3.2 新增枚举

```cpp
enum class EBlueprintHelperProjectReadScope : uint8
{
    ProjectContext,
    ProjectMarker,
    SetupState
};

enum class EBlueprintHelperProjectContextStatus : uint8
{
    Ok,
    Degraded,
    Blocked
};

enum class EBlueprintHelperProjectMarkerStatus : uint8
{
    Present,
    Missing,
    Invalid
};

enum class EBlueprintHelperSetupStateStatus : uint8
{
    Ok,
    Blocked
};

enum class EBlueprintHelperProjectContextStage : uint8
{
    ReadProjectContext,
    DetectProject,
    CheckProjectMarker,
    CheckSetupState,
    ReadProjectFile,
    ReadMarkerFile,
    ReadRuntimeConfig
};

enum class EBlueprintHelperProjectContextErrorCode : uint8
{
    InvalidRequest,
    ProjectContextCheckFailed,
    ProjectMarkerCheckFailed,
    SetupStateCheckFailed,
    ProjectFileUnreadable,
    MarkerFileUnreadable,
    SettingsStateUnavailable,
    InternalError
};
```

### 3.3 Reason 枚举建议

使用稳定字符串 reason：

```text
ue_project_not_detected
project_marker_missing
project_marker_invalid
setup_not_completed
config_unavailable
settings_state_unavailable
```

不要输出自然语言 reason，也不要输出 settings 具体损坏类型。

---

## 4. Phase B：Agent-facing DTO

### 4.1 read_project_context DTO

```cpp
struct FBlueprintHelperReadProjectContextResultData
{
    FString Schema = TEXT("ReadProjectContext.v1");
    FBlueprintHelperProjectContextStatusData ProjectContext;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperProjectContextStatusData
{
    FString Status; // ok | degraded | blocked
    bool bProjectDetected = false;

    // 项目已识别时返回。
    TOptional<FString> ProjectMarker; // present | missing | invalid

    bool bWorkflowEnabled = false;

    // 异常态返回。
    TOptional<FString> Reason;

    TSharedRef<FJsonObject> ToJson() const;
};
```

正常：

```json
{
  "schema": "ReadProjectContext.v1",
  "project_context": {
    "status": "ok",
    "project_detected": true,
    "project_marker": "present",
    "workflow_enabled": true
  }
}
```

项目未识别：

```json
{
  "schema": "ReadProjectContext.v1",
  "project_context": {
    "status": "blocked",
    "project_detected": false,
    "workflow_enabled": false,
    "reason": "ue_project_not_detected"
  }
}
```

### 4.2 check_project_marker DTO

```cpp
struct FBlueprintHelperCheckProjectMarkerResultData
{
    FString Schema = TEXT("CheckProjectMarker.v1");
    FBlueprintHelperProjectMarkerCheck ProjectMarker;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperProjectMarkerCheck
{
    FString Status; // present | missing | invalid
    bool bWorkflowEnabled = false;

    // missing/invalid 时返回。
    TOptional<FString> Reason;

    TSharedRef<FJsonObject> ToJson() const;
};
```

正常：

```json
{
  "schema": "CheckProjectMarker.v1",
  "project_marker": {
    "status": "present",
    "workflow_enabled": true
  }
}
```

### 4.3 check_setup_state DTO

```cpp
struct FBlueprintHelperCheckSetupStateResultData
{
    FString Schema = TEXT("CheckSetupState.v1");
    FBlueprintHelperSetupState SetupState;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperSetupState
{
    FString Status; // ok | blocked

    // blocked 时返回。
    TOptional<FString> Reason;

    TSharedRef<FJsonObject> ToJson() const;
};
```

正常态极简：

```json
{
  "schema": "CheckSetupState.v1",
  "setup_state": {
    "status": "ok"
  }
}
```

### 4.4 明确禁止字段

所有 DTO 都不得包含：

```cpp
FString ProjectRoot;
FString UProjectPath;
FString SettingsPath;
FString ClaudeMdPath;
FString ClaudeMdContent;
FString MarkerBody;
FString MarkerStartLine;
FString MarkerEndLine;
FString GlobalGuidancePath;
FBlueprintHelperValidationResult Validation;
FBlueprintHelperWriteRef WriteRef;
FString TransactionId;
FString ReviewStatus;
FString SafetyProfile;
```

---

## 5. Phase C：ProjectContextService

### 5.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperProjectContextService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperProjectContextService.cpp
```

### 5.2 服务接口

```cpp
class FBlueprintHelperProjectContextService
{
public:
    FBlueprintHelperToolResultBase ReadProjectContext(
        const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase CheckProjectMarker(
        const TSharedPtr<FJsonObject>& Payload) const;

    FBlueprintHelperToolResultBase CheckSetupState(
        const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool DetectProject(
        FBlueprintHelperDetectedProjectState& OutState,
        FBlueprintHelperToolError& OutError) const;

    bool CheckProjectMarkerInternal(
        const FBlueprintHelperDetectedProjectState& ProjectState,
        FBlueprintHelperProjectMarkerCheck& OutMarker,
        FBlueprintHelperToolError& OutError) const;

    bool CheckSetupStateInternal(
        FBlueprintHelperSetupState& OutSetupState,
        FBlueprintHelperToolError& OutError) const;
};
```

### 5.3 内部项目状态

```cpp
struct FBlueprintHelperDetectedProjectState
{
    bool bProjectDetected = false;

    // 仅内部使用，不序列化。
    FString ProjectRoot;
    FString UProjectPath;
    FString ProjectClaudeMdPath;
};
```

---

## 6. Phase D：项目识别

### 6.1 检测信号

第一版可用以下信号：

```text
1. FPaths::ProjectDir() 存在。
2. FPaths::GetProjectFilePath() 存在且后缀为 .uproject。
3. 当前进程是 UE Editor。
```

实现：

```cpp
FString ProjectDir = FPaths::ProjectDir();
FString ProjectFile = FPaths::GetProjectFilePath();

const bool bProjectDetected =
    !ProjectDir.IsEmpty() &&
    !ProjectFile.IsEmpty() &&
    FPaths::FileExists(ProjectFile);
```

不要在 Agent-facing 结果返回这些路径。

### 6.2 未识别项目

工具成功完成检查，但状态 blocked：

```text
ok=true
status=completed
data.project_context.status=blocked
project_detected=false
workflow_enabled=false
reason=ue_project_not_detected
```

不要返回 `error`，除非检查过程自身失败。

---

## 7. Phase E：Project Marker 检查

### 7.1 Marker 文件位置

内部检查项目根目录：

```text
<Project>/CLAUDE.md
```

第一版不默认检查 `AGENTS.md`，除非后续明确扩展。

### 7.2 Marker 边界

查找 managed block 边界：

```text
<!-- BEGIN BLUEPRINTHELPER PROJECT MARKER -->
<!-- END BLUEPRINTHELPER PROJECT MARKER -->
```

### 7.3 检查结果规则

```text
CLAUDE.md 不存在 → missing
CLAUDE.md 存在但无 marker → missing
恰好一个完整 marker → present
只有 begin 无 end → invalid
只有 end 无 begin → invalid
多个 begin/end 或嵌套 → invalid
marker 内容为空或缺少必要关键词 → invalid
```

必要关键词建议内部检查：

```text
BlueprintHelper
Agent-assisted Blueprint editing
runtime profile
Safety Profile
stop_and_report
```

不要把 marker 内容返回给 Agent。

### 7.4 CheckProjectMarkerInternal

```cpp
bool FBlueprintHelperProjectContextService::CheckProjectMarkerInternal(
    const FBlueprintHelperDetectedProjectState& ProjectState,
    FBlueprintHelperProjectMarkerCheck& OutMarker,
    FBlueprintHelperToolError& OutError) const
{
    if (!ProjectState.bProjectDetected)
    {
        OutMarker.Status = TEXT("missing");
        OutMarker.bWorkflowEnabled = false;
        OutMarker.Reason = TEXT("ue_project_not_detected");
        return true;
    }

    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *ProjectState.ProjectClaudeMdPath))
    {
        OutMarker.Status = TEXT("missing");
        OutMarker.bWorkflowEnabled = false;
        OutMarker.Reason = TEXT("project_marker_missing");
        return true;
    }

    // Count begin/end, validate marker.
}
```

### 7.5 不返回路径 / 文本 / 行号

即使内部解析出：

```text
ProjectClaudeMdPath
marker_start_line
marker_end_line
marker_body
```

也不得进入 Agent-facing result。

---

## 8. Phase F：Setup State 检查

### 8.1 SettingsService

复用 runtime profile 的 SettingsService：

```cpp
class FBlueprintHelperSettingsService
{
public:
    bool IsSetupCompleted() const;
    bool IsRuntimeConfigAvailable() const;
    FString GetSetupBlockedReason() const;
};
```

### 8.2 正常态

如果 setup 完成、runtime 所需 settings 可用：

```json
{
  "setup_state": {
    "status": "ok"
  }
}
```

不得额外返回：

```text
settings.valid
version
settings_path
profile
safety_profile
```

### 8.3 blocked

如果 setup 未完成：

```json
{
  "setup_state": {
    "status": "blocked",
    "reason": "setup_not_completed"
  }
}
```

如果 settings 不可用：

```json
{
  "setup_state": {
    "status": "blocked",
    "reason": "config_unavailable"
  }
}
```

### 8.4 不展开 settings 具体损坏类型

本工具不区分：

```text
settings missing
settings invalid
settings damaged
settings old_version
settings missing_fields
migration_failed
```

这些细节属于：

```text
/blueprinthelper-setup
/blueprinthelper-diagnostics
```

---

## 9. Phase G：read_project_context 实现

### 9.1 执行流程

```text
1. DetectProject。
2. 如果未识别项目：返回 blocked。
3. CheckProjectMarkerInternal。
4. 组合 project_context：
   - marker present → status=ok / workflow_enabled=true
   - marker missing → status=degraded / workflow_enabled=false / reason=project_marker_missing
   - marker invalid → status=degraded / workflow_enabled=false / reason=project_marker_invalid
```

### 9.2 是否检查 setup_state

`read_project_context` 字段稿只要求：

```text
project_detected
project_marker
workflow_enabled
```

不建议在该工具里同步展开 setup_state。  
如果 setup 状态影响 workflow_enabled，可以只在内部影响 status/reason，但不要返回 setup_state 对象。

推荐第一版：

```text
read_project_context 只看项目 + marker。
setup 单独由 check_setup_state 处理。
```

这样职责更清晰。

### 9.3 成功 ToolResult

```cpp
Result.bOk = true;
Result.Operation = TEXT("read_project_context");
Result.Status = TEXT("completed");
Result.bModified = false;
Result.Target = MakeReadScopeTarget(TEXT("project_context"));
Result.Data = Data.ToJson();
```

---

## 10. Phase H：check_project_marker 实现

### 10.1 执行流程

```text
1. DetectProject。
2. CheckProjectMarkerInternal。
3. 返回 CheckProjectMarker.v1。
```

### 10.2 项目未识别

可返回：

```json
{
  "project_marker": {
    "status": "missing",
    "workflow_enabled": false,
    "reason": "ue_project_not_detected"
  }
}
```

仍然：

```text
ok=true
status=completed
modified=false
```

### 10.3 Marker invalid

返回：

```text
status=invalid
workflow_enabled=false
reason=project_marker_invalid
```

不返回 invalid 的具体行号或文本。

---

## 11. Phase I：check_setup_state 实现

### 11.1 执行流程

```text
1. SettingsService.IsSetupCompleted。
2. SettingsService.IsRuntimeConfigAvailable。
3. 正常：status=ok。
4. 未 setup：status=blocked/reason=setup_not_completed。
5. config 不可用：status=blocked/reason=config_unavailable。
```

### 11.2 工具自身失败

如果 SettingsService 自身异常：

```text
ok=false
status=failed
error.code=setup_state_check_failed
stage=check_setup_state
retryable=true
```

不要伪装成 `setup_state.status=blocked`，因为这不是成功检查结果，而是工具自身失败。

---

## 12. Phase J：ToolResult 构建

### 12.1 read_project_context

```cpp
FBlueprintHelperToolResultBase Result;
Result.bOk = true;
Result.Operation = TEXT("read_project_context");
Result.Status = TEXT("completed");
Result.bModified = false;
Result.Target = MakeProjectReadTarget(TEXT("project_context"));

FBlueprintHelperReadProjectContextResultData Data;
Data.ProjectContext = BuildProjectContextStatus(...);

Result.Data = Data.ToJson();
```

### 12.2 check_project_marker

```cpp
Result.Operation = TEXT("check_project_marker");
Result.Target = MakeProjectReadTarget(TEXT("project_marker"));
Result.Data = MarkerData.ToJson();
```

### 12.3 check_setup_state

```cpp
Result.Operation = TEXT("check_setup_state");
Result.Target = MakeProjectReadTarget(TEXT("setup_state"));
Result.Data = SetupData.ToJson();
```

### 12.4 failure

```cpp
Result.bOk = false;
Result.Status = TEXT("failed");
Result.bModified = false;
Result.Error = MakeProjectContextError(...);
```

---

## 13. Phase K：Bridge Router 接入

### 13.1 新增 commands

```text
read_project_context
check_project_marker
check_setup_state
```

### 13.2 Router 分支

```cpp
if (Request.Command == TEXT("read_project_context"))
{
    return HandleReadProjectContext(Request);
}
if (Request.Command == TEXT("check_project_marker"))
{
    return HandleCheckProjectMarker(Request);
}
if (Request.Command == TEXT("check_setup_state"))
{
    return HandleCheckSetupState(Request);
}
```

### 13.3 Handler 模板

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleReadProjectContext(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result =
        ProjectContextService.ReadProjectContext(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("project context check failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

---

## 14. Phase L：RequestValidator / 权限

### 14.1 Validator

三个工具第一版都不需要业务参数：

```cpp
if (Command == TEXT("read_project_context") ||
    Command == TEXT("check_project_marker") ||
    Command == TEXT("check_setup_state"))
{
    // Empty payload allowed.
    return true;
}
```

可选未来参数：

```text
include_debug=false
```

字段稿未定义，第一版不实现。

### 14.2 权限

三者都是只读检查：

```text
不需要 write token
不生成 transaction
不写文件
不写 Journal
ReadOnly 下允许
modified=false
```

---

## 15. Phase M：隐私与白名单序列化

### 15.1 禁止直接序列化内部状态

不要直接序列化：

```cpp
FBlueprintHelperDetectedProjectState
FBlueprintHelperSettingsRawState
```

因为内部结构可能包含：

```text
project_root
uproject_path
settings_path
local file paths
marker body
```

必须使用 Agent-facing DTO 白名单输出。

### 15.2 路径脱敏

如果错误 message 来自底层文件系统，可能包含本地路径。构建 `error.message` 前应清理：

```text
C:\...
/Users/...
<Project>/...
```

错误 message 应简短：

```text
Project context could not be checked.
Project marker could not be checked.
Setup state could not be checked.
```

具体原因用 reason enum 表达。

---

## 16. Phase N：不提供写入工具

本阶段不得注册：

```text
write_project_marker
repair_project_marker
remove_project_marker
run_blueprinthelper_setup
migrate_settings
repair_settings
write_global_guidance
```

如果用户需要写 Project Marker：

```text
Agent 侧应请求用户确认后，通过普通文件系统流程或插件菜单处理。
```

UE runtime 只读工具不应静默写项目规则文件。

---

## 17. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperProjectContextContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperProjectMarkerTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperSetupStateTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperProjectContextPrivacyTests.cpp
```

### 17.1 Contract tests

```text
1. read_project_context_ok_contract
   - operation=read_project_context
   - data.schema=ReadProjectContext.v1
   - project_context.status=ok
   - project_detected=true
   - project_marker=present
   - workflow_enabled=true
   - modified=false
   - 不返回 validation/write_ref/transaction_id/path/text

2. read_project_context_missing_marker_contract
   - status=degraded
   - project_marker=missing
   - workflow_enabled=false
   - reason=project_marker_missing

3. check_project_marker_present_contract
   - data.schema=CheckProjectMarker.v1
   - project_marker.status=present
   - workflow_enabled=true
   - 不返回 marker body/path/line

4. check_project_marker_invalid_contract
   - status=invalid
   - reason=project_marker_invalid

5. check_setup_state_ok_minimal_contract
   - data.schema=CheckSetupState.v1
   - setup_state.status=ok
   - 不返回 config details

6. check_setup_state_blocked_contract
   - setup_state.status=blocked
   - reason=setup_not_completed 或 config_unavailable
```

### 17.2 Marker tests

```text
1. marker_missing_when_claude_md_absent
2. marker_missing_when_no_managed_block
3. marker_present_when_single_valid_block
4. marker_invalid_when_begin_without_end
5. marker_invalid_when_end_without_begin
6. marker_invalid_when_multiple_blocks
7. marker_invalid_when_required_keywords_missing
```

### 17.3 Setup tests

```text
1. setup_state_ok
2. setup_state_setup_not_completed
3. setup_state_config_unavailable
4. setup_state_does_not_expose_settings_damage_type
```

### 17.4 Privacy tests

```text
1. no_project_root_in_success
2. no_uproject_path_in_success
3. no_settings_path_in_success
4. no_marker_text_in_success
5. no_claude_md_content_in_success
6. no_local_path_in_error_message
```

---

## 18. 推荐提交顺序

### Commit 1：DTO 与序列化

```text
Add Project Context DTOs
Add project read scope / status / marker / setup enums
Add short schema serializers
```

验收：

```text
所有结果 modified=false。
无 validation/write_ref/transaction_id。
```

### Commit 2：项目识别

```text
Add ProjectContextService skeleton
Implement DetectProject using FPaths::ProjectDir/GetProjectFilePath
Return project_detected status without exposing paths
```

验收：

```text
项目未识别返回 blocked reason。
不返回 project_root。
```

### Commit 3：Project Marker 检查

```text
Implement CLAUDE.md marker parser
Detect present/missing/invalid
Suppress marker body/path/line output
```

验收：

```text
marker 状态准确。
隐私字段不泄露。
```

### Commit 4：Setup State 检查

```text
Integrate SettingsService
Implement check_setup_state ok/blocked
Suppress settings damage details
```

验收：

```text
正常态只返回 status=ok。
异常态只返回 status/reason。
```

### Commit 5：read_project_context 聚合

```text
Combine project detection and marker check
Return ok/degraded/blocked project_context
```

验收：

```text
workflow_enabled 与 marker 状态一致。
```

### Commit 6：Bridge / Validator / Auth

```text
Register read_project_context
Register check_project_marker
Register check_setup_state
Allow empty payloads
Classify tools as read-only
```

验收：

```text
ReadOnly 下可调用。
不需要 write token。
```

### Commit 7：Protocol / privacy regression

```text
Add contract tests
Add privacy leak tests
Add no-write-tools registration test
```

验收：

```text
字段稿验收项全部通过。
```

---

## 19. 第一版不做的内容

```text
1. 不写 Project Marker。
2. 不修复 Project Marker。
3. 不删除 Project Marker。
4. 不运行 /blueprinthelper-setup。
5. 不迁移 settings.json。
6. 不修复 settings.json。
7. 不写全局 CLAUDE.md managed block。
8. 不返回 project_root / uproject_path / settings_path。
9. 不返回 CLAUDE.md 全文。
10. 不返回 marker 文本 / 行号。
11. 不返回 setup 详细损坏类型。
12. 不写 Journal / Review。
```

---

## 20. 最小验收标准

read_project_context 正常：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_project_context",
  "status": "completed",
  "modified": false,
  "target": {
    "read_scope": "project_context"
  },
  "data": {
    "schema": "ReadProjectContext.v1",
    "project_context": {
      "status": "ok",
      "project_detected": true,
      "project_marker": "present",
      "workflow_enabled": true
    }
  }
}
```

read_project_context marker 缺失：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_project_context",
  "status": "completed",
  "modified": false,
  "target": {
    "read_scope": "project_context"
  },
  "data": {
    "schema": "ReadProjectContext.v1",
    "project_context": {
      "status": "degraded",
      "project_detected": true,
      "project_marker": "missing",
      "workflow_enabled": false,
      "reason": "project_marker_missing"
    }
  }
}
```

check_project_marker：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "check_project_marker",
  "status": "completed",
  "modified": false,
  "target": {
    "read_scope": "project_marker"
  },
  "data": {
    "schema": "CheckProjectMarker.v1",
    "project_marker": {
      "status": "present",
      "workflow_enabled": true
    }
  }
}
```

check_setup_state 正常：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "check_setup_state",
  "status": "completed",
  "modified": false,
  "target": {
    "read_scope": "setup_state"
  },
  "data": {
    "schema": "CheckSetupState.v1",
    "setup_state": {
      "status": "ok"
    }
  }
}
```

check_setup_state blocked：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "check_setup_state",
  "status": "completed",
  "modified": false,
  "target": {
    "read_scope": "setup_state"
  },
  "data": {
    "schema": "CheckSetupState.v1",
    "setup_state": {
      "status": "blocked",
      "reason": "config_unavailable"
    }
  }
}
```

必须不出现：

```text
project_root
uproject_path
settings_path
CLAUDE.md content
marker body
marker_start_line
marker_end_line
validation
write_ref
transaction_id
journal_recorded
review
safety
rollback_data
```

---

## 21. 实现风险

### 21.1 UE 内部需要路径但结果不得泄露

风险：

```text
错误 message 或 debug 字段把本地绝对路径带给 Agent。
```

处理：

```text
Agent-facing DTO 白名单输出。
error.message 统一短句。
路径只内部使用。
```

### 21.2 check_setup_state 展开过多配置细节

风险：

```text
把 settings missing/invalid/damaged/old_version 细节返回。
```

处理：

```text
check_setup_state 只返回 setup_not_completed 或 config_unavailable。
详细诊断由 diagnostics/setup 处理。
```

### 21.3 Project Marker 写入工具误注册

风险：

```text
为了方便实现顺手注册 write_project_marker。
```

处理：

```text
本阶段只注册三个 read-only commands。
加入测试确保写 marker 工具不存在。
```

### 21.4 read_project_context 与 runtime_profile 职责重叠

风险：

```text
read_project_context 返回 write_permission/tool_capabilities。
```

处理：

```text
只返回 project_context 状态。
runtime 链路交给 get_runtime_profile。
