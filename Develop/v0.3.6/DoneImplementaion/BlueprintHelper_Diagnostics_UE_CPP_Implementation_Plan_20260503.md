# BlueprintHelper Diagnostics / Static & Runtime Diagnostics UE 侧 C++ 可执行实现计划

状态：[x] 已完成
日期：2026-05-03  
来源字段稿：`BlueprintHelper_Diagnostics_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++ + MCP diagnostics 命令桥接  
不包含：setup 修复、settings 迁移、Project Marker 写入、全局 CLAUDE.md 写入、AgentPlan、具体蓝图任务能力判断

---

## 0. 实现目标

实现两个命令入口：

```text
/blueprinthelper-diagnostics
/blueprinthelper-diagnostics --runtime
```

MCP ToolResult 统一为：

```text
operation = run_blueprinthelper_diagnostics
target.diagnostics_mode = static | runtime
data.schema = Diagnostics.v1
data.markdown = <diagnostics markdown>
```

核心协议：诊断报告只返回 `data.markdown`；不返回 `blocking[] / warning[] / info[]` JSON 数组；Markdown 必须包含 `## Blocking` 与 `## Warning`，为空写 `None`；`## Info` 可选。诊断报告中存在 Blocking 项时，工具仍应返回 `ok=true/status=completed`；只有诊断工具自身执行失败才返回 `ok=false/status=failed/error`。

---

## 1. 工具边界

Diagnostics 负责安装、配置、Bridge/MCP/runtime 链路、write_permission、risk_command、Project Marker 的只读诊断。

Diagnostics 不负责：

```text
修复配置
迁移 settings.json
写 Project Marker
写全局 CLAUDE.md
读取蓝图 LogicMD / LogicJson
生成 AgentPlan
判断具体蓝图任务是否能完成
执行写工具
输出 Suggested action / action code
```

所有 diagnostics 调用固定：

```text
modified=false
不返回 validation
不返回 write_ref
不返回 transaction_id
不返回 journal_recorded
不返回 review
不返回 safety
不返回 rollback_data
```

---

## 2. Phase A：新增类型文件

新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperDiagnosticsTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperDiagnosticsTypes.cpp
```

枚举：

```cpp
enum class EBlueprintHelperDiagnosticsMode : uint8
{
    Static,
    Runtime
};

enum class EBlueprintHelperDiagnosticsStage : uint8
{
    ParseInput,
    RunDiagnostics,
    CollectStaticDiagnostics,
    CollectRuntimeDiagnostics,
    CheckVersion,
    CheckSettings,
    CheckGlobalGuidance,
    CheckSkillEntry,
    CheckProjectMarker,
    CheckUEEditor,
    CheckMcpServer,
    CheckBridge,
    CheckRuntimeProfile,
    CheckWritePermission,
    CheckRiskCommand,
    BuildMarkdown
};

enum class EBlueprintHelperDiagnosticsErrorCode : uint8
{
    InvalidRequest,
    DiagnosticsFailed,
    StaticDiagnosticsFailed,
    RuntimeDiagnosticsFailed,
    MarkdownBuildFailed,
    InternalError
};
```

稳定字符串：

```text
static
runtime
run_blueprinthelper_diagnostics
diagnostics_failed
run_diagnostics
```

不要输出 C++ enum 原名。

---

## 3. Phase B：Agent-facing DTO

```cpp
struct FBlueprintHelperDiagnosticsResultData
{
    FString Schema = TEXT("Diagnostics.v1");
    FString Markdown;
    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperDiagnosticsTarget
{
    FString DiagnosticsMode; // static | runtime
    TSharedRef<FJsonObject> ToJson() const;
};
```

明确禁止字段：

```text
blocking[]
warning[]
info[]
validation
write_ref
transaction_id
review
safety
suggested_action
action_code
local_absolute_path
settings_json
claude_md_content
```

---

## 4. Phase C：内部诊断模型

Agent-facing 只返回 Markdown；内部可以用结构化模型构建报告。

```cpp
struct FBlueprintHelperDiagnosticMarkdownItem
{
    FString Code; // bridge.connected, settings.invalid, etc.
    TMap<FString, FString> Details;
};

struct FBlueprintHelperDiagnosticsReportModel
{
    TArray<FBlueprintHelperDiagnosticMarkdownItem> Blocking;
    TArray<FBlueprintHelperDiagnosticMarkdownItem> Warning;
    TArray<FBlueprintHelperDiagnosticMarkdownItem> Info;
};
```

该模型不得直接序列化给 Agent。

code 统一格式：

```text
<domain>.<state>
```

示例：

```text
version.match
settings.valid
settings.invalid
global_guidance.present
skill_entry.valid
project_marker.present
project_marker.missing
ue_editor.running
mcp_server.available
bridge.connected
bridge.disconnected
runtime_profile.available
config_status.valid
config_status.unavailable
write_permission.enabled
write_permission.disabled
risk_command.enabled
risk_command.disabled
```

---

## 5. Phase D：Markdown Builder

新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperDiagnosticsMarkdownBuilder.h
Source/BlueprintHelper/Private/Services/BlueprintHelperDiagnosticsMarkdownBuilder.cpp
```

接口：

```cpp
class FBlueprintHelperDiagnosticsMarkdownBuilder
{
public:
    FString Build(const FBlueprintHelperDiagnosticsReportModel& Report) const;

private:
    void AppendSection(
        FStringBuilderBase& Builder,
        const FString& SectionName,
        const TArray<FBlueprintHelperDiagnosticMarkdownItem>& Items,
        bool bSectionRequired) const;

    FString FormatItem(const FBlueprintHelperDiagnosticMarkdownItem& Item) const;
};
```

输出规则：

```text
1. 固定顺序：Blocking -> Warning -> Info。
2. Blocking 必须存在。
3. Warning 必须存在。
4. Info 可选。
5. Blocking / Warning 为空时写 None。
6. 不输出 Suggested action。
7. 不输出 action code。
8. 不输出本地绝对路径。
```

条目格式：

```md
- `version.match`
```

带 detail：

```md
- `write_permission.disabled`
  - reason: `token_missing`
```

risk_command 示例：

```md
- `risk_command.disabled`
  - reason: `risk_command_missing`
  - blocked_commands: `close_editor`
```

---

## 6. Phase E：Diagnostics Sanitizer

新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperDiagnosticsSanitizer.h
Source/BlueprintHelper/Private/Services/BlueprintHelperDiagnosticsSanitizer.cpp
```

职责：防止底层错误消息或配置读取结果泄露本地路径、settings 内容、CLAUDE.md 内容、Token、secret。

接口：

```cpp
class FBlueprintHelperDiagnosticsSanitizer
{
public:
    FString SanitizeMarkdown(const FString& Markdown) const;
    FString SanitizeMessage(const FString& Message) const;
};
```

策略：

```text
1. Markdown code 必须来自白名单或稳定枚举。
2. detail value 只允许 reason / blocked_commands 等短枚举。
3. 不直接拼接底层异常原文。
4. error.message 使用固定短句。
```

---

## 7. Phase F：DiagnosticsService

新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperDiagnosticsService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperDiagnosticsService.cpp
```

接口：

```cpp
class FBlueprintHelperDiagnosticsService
{
public:
    FBlueprintHelperToolResultBase RunDiagnostics(
        const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool ParseRequest(
        const TSharedPtr<FJsonObject>& Payload,
        EBlueprintHelperDiagnosticsMode& OutMode,
        FBlueprintHelperToolError& OutError) const;

    bool CollectStaticDiagnostics(
        FBlueprintHelperDiagnosticsReportModel& OutReport,
        FBlueprintHelperToolError& OutError) const;

    bool CollectRuntimeDiagnostics(
        FBlueprintHelperDiagnosticsReportModel& OutReport,
        FBlueprintHelperToolError& OutError) const;

    FBlueprintHelperToolResultBase BuildSuccess(
        EBlueprintHelperDiagnosticsMode Mode,
        const FBlueprintHelperDiagnosticsReportModel& Report) const;
};
```

---

## 8. Phase G：Static Diagnostics 实现

Static diagnostics 检查静态安装与配置：

```text
version.match / version.mismatch
settings.valid / settings.invalid
global_guidance.present / global_guidance.missing
skill_entry.valid / skill_entry.invalid
project_marker.present / project_marker.missing / project_marker.invalid
```

正常示例：

```md
## Blocking
None

## Warning
None

## Info
- `version.match`
- `settings.valid`
- `global_guidance.present`
- `skill_entry.valid`
```

settings invalid：

```md
## Blocking
- `settings.invalid`

## Warning
None

## Info
- `version.match`
- `global_guidance.present`
```

Project Marker 缺失：

```md
## Blocking
None

## Warning
- `project_marker.missing`

## Info
- `version.match`
- `settings.valid`
- `global_guidance.present`
- `skill_entry.valid`
```

不得展开：

```text
settings 字段级错误
settings.json 路径
settings.json 内容
Project Marker 文本
CLAUDE.md 全文
自动修复建议
```

---

## 9. Phase H：Runtime Diagnostics 实现

Runtime diagnostics 检查 UE/MCP/Bridge/runtime 链路：

```text
ue_editor.running
mcp_server.available
bridge.connected / bridge.disconnected
runtime_profile.available
config_status.valid / config_status.unavailable
write_permission.enabled / write_permission.disabled
risk_command.enabled / risk_command.disabled
```

正常：

```md
## Blocking
None

## Warning
None

## Info
- `ue_editor.running`
- `mcp_server.available`
- `bridge.connected`
- `runtime_profile.available`
- `config_status.valid`
- `write_permission.enabled`
- `risk_command.enabled`
```

Token 缺失：

```md
## Blocking
None

## Warning
- `write_permission.disabled`
  - reason: `token_missing`

## Info
- `ue_editor.running`
- `mcp_server.available`
- `bridge.connected`
- `runtime_profile.available`
- `config_status.valid`
```

Bridge 断开：

```md
## Blocking
- `bridge.disconnected`

## Warning
None

## Info
- `ue_editor.running`
- `mcp_server.available`
```

risk_command 缺失：

```md
## Blocking
None

## Warning
- `risk_command.disabled`
  - reason: `risk_command_missing`
  - blocked_commands: `close_editor`

## Info
- `ue_editor.running`
- `mcp_server.available`
- `bridge.connected`
- `runtime_profile.available`
- `config_status.valid`
- `write_permission.enabled`
```

config unavailable：

```md
## Blocking
- `config_status.unavailable`

## Warning
- `write_permission.disabled`
  - reason: `config_unavailable`

## Info
- `ue_editor.running`
- `mcp_server.available`
- `bridge.connected`
- `runtime_profile.available`
```

重点：`risk_command_missing` 只阻断 `close_editor`，不阻断普通蓝图读写。

---

## 10. Phase I：ToolResult 构建

成功构建：

```cpp
FBlueprintHelperToolResultBase Result;
Result.bOk = true;
Result.Schema = TEXT("BlueprintHelper.McpToolResult.v1");
Result.Operation = TEXT("run_blueprinthelper_diagnostics");
Result.Status = TEXT("completed");
Result.bModified = false;
Result.Target = MakeDiagnosticsTarget(Mode);

FBlueprintHelperDiagnosticsResultData Data;
Data.Schema = TEXT("Diagnostics.v1");
Data.Markdown = MarkdownBuilder.Build(Report);
Data.Markdown = Sanitizer.SanitizeMarkdown(Data.Markdown);

Result.Data = Data.ToJson();
return Result;
```

错误边界：

```text
Report.Blocking.Num() > 0 仍然 ok=true/status=completed。
只有诊断工具自身失败才 ok=false/status=failed。
```

工具自身失败示例：

```json
{
  "ok": false,
  "operation": "run_blueprinthelper_diagnostics",
  "status": "failed",
  "modified": false,
  "target": {
    "diagnostics_mode": "runtime"
  },
  "error": {
    "code": "diagnostics_failed",
    "stage": "run_diagnostics",
    "message": "BlueprintHelper diagnostics could not be executed.",
    "retryable": true
  }
}
```

---

## 11. Phase J：Bridge Router 接入

UE Bridge command：

```text
run_blueprinthelper_diagnostics
```

命令入口映射：

```text
/blueprinthelper-diagnostics           -> diagnostics_mode=static
/blueprinthelper-diagnostics --runtime -> diagnostics_mode=runtime
```

Router 分支：

```cpp
if (Request.Command == TEXT("run_blueprinthelper_diagnostics"))
{
    return HandleRunBlueprintHelperDiagnostics(Request);
}
```

Handler：

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleRunBlueprintHelperDiagnostics(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result =
        DiagnosticsService.RunDiagnostics(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("diagnostics failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

---

## 12. Phase K：RequestValidator / 权限

Validator：

```cpp
if (Command == TEXT("run_blueprinthelper_diagnostics"))
{
    OptionalString(Payload, TEXT("diagnostics_mode")); // static | runtime
}
```

默认：

```text
diagnostics_mode=static
```

未知 mode：

```text
ok=false
status=failed
error.code=invalid_request
stage=parse_input
```

权限：

```text
不需要 write token
不生成 transaction
不写 Journal
ReadOnly 下允许
modified=false
```

即使 `write_permission.disabled`，Diagnostics 也必须能运行。

---

## 13. Phase L：现有服务集成

### 13.1 SettingsService

Diagnostics 读取：

```text
settings.valid / settings.invalid
config_status.valid / config_status.unavailable
write_permission reason
```

但不返回 settings 详情。

### 13.2 RuntimeProfileService

Runtime diagnostics 应调用 RuntimeProfileService 的内部 facts collector，而不是调用 Agent-facing runtime_profile JSON。

原因：runtime_profile 正常态只返回 `status=ok`，Diagnostics 需要内部 facts 生成 Info code。

### 13.3 ProjectContextService

Static diagnostics 可复用：

```text
DetectProject
CheckProjectMarkerInternal
CheckSetupStateInternal
```

只输出 Markdown code，不输出路径/marker 文本。

### 13.4 RiskCommandService

Runtime diagnostics 输出：

```text
risk_command.enabled
risk_command.disabled reason=risk_command_missing blocked_commands=close_editor
```

---

## 14. Phase M：code 白名单

建议新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperDiagnosticsCodes.h
```

集中定义稳定 code，避免自然语言漂移：

```cpp
namespace BlueprintHelperDiagnosticsCodes
{
    static const TCHAR* VersionMatch = TEXT("version.match");
    static const TCHAR* SettingsValid = TEXT("settings.valid");
    static const TCHAR* SettingsInvalid = TEXT("settings.invalid");
    static const TCHAR* GlobalGuidancePresent = TEXT("global_guidance.present");
    static const TCHAR* SkillEntryValid = TEXT("skill_entry.valid");
    static const TCHAR* ProjectMarkerPresent = TEXT("project_marker.present");
    static const TCHAR* ProjectMarkerMissing = TEXT("project_marker.missing");
    static const TCHAR* UEEditorRunning = TEXT("ue_editor.running");
    static const TCHAR* McpServerAvailable = TEXT("mcp_server.available");
    static const TCHAR* BridgeConnected = TEXT("bridge.connected");
    static const TCHAR* BridgeDisconnected = TEXT("bridge.disconnected");
    static const TCHAR* RuntimeProfileAvailable = TEXT("runtime_profile.available");
    static const TCHAR* ConfigStatusValid = TEXT("config_status.valid");
    static const TCHAR* ConfigStatusUnavailable = TEXT("config_status.unavailable");
    static const TCHAR* WritePermissionEnabled = TEXT("write_permission.enabled");
    static const TCHAR* WritePermissionDisabled = TEXT("write_permission.disabled");
    static const TCHAR* RiskCommandEnabled = TEXT("risk_command.enabled");
    static const TCHAR* RiskCommandDisabled = TEXT("risk_command.disabled");
}
```

---

## 15. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperDiagnosticsContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperDiagnosticsMarkdownTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperDiagnosticsRuntimeTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperDiagnosticsPrivacyTests.cpp
```

Contract tests：

```text
1. static_diagnostics_success_contract
   - operation=run_blueprinthelper_diagnostics
   - target.diagnostics_mode=static
   - data.schema=Diagnostics.v1
   - data.markdown exists
   - modified=false
   - 不返回 validation/write_ref/transaction_id
   - 不返回 blocking/warning/info JSON arrays

2. runtime_diagnostics_success_contract
   - target.diagnostics_mode=runtime
   - data.markdown exists

3. diagnostics_with_blocking_still_ok_contract
   - markdown contains ## Blocking with item
   - ok=true
   - status=completed

4. diagnostics_tool_failure_contract
   - ok=false
   - status=failed
   - error.code=diagnostics_failed
```

Markdown tests：

```text
1. markdown_contains_blocking_and_warning_sections
2. markdown_writes_none_for_empty_blocking_warning
3. markdown_info_optional
4. markdown_section_order_is_blocking_warning_info
5. markdown_does_not_include_suggested_action
6. markdown_does_not_include_action_code
```

Runtime tests：

```text
1. runtime_token_missing_warning_not_blocking
2. runtime_bridge_disconnected_blocking
3. runtime_risk_command_missing_warning_not_blocking
4. runtime_config_unavailable_blocking
5. risk_command_missing_lists_close_editor
```

Privacy tests：

```text
1. diagnostics_no_local_absolute_path
2. diagnostics_no_settings_json_content
3. diagnostics_no_claude_md_content
4. diagnostics_no_token_or_secret
5. diagnostics_error_message_sanitized
```

---

## 16. 推荐提交顺序

### Commit 1：DTO / Enum / Markdown Builder

```text
Add Diagnostics DTOs
Add diagnostics mode/stage/error enums
Add DiagnosticsMarkdownBuilder
Add markdown section-order tests
```

验收：`data.schema=Diagnostics.v1`，只输出 `data.markdown`，Blocking/Warning 必有。

### Commit 2：DiagnosticsSanitizer / code whitelist

```text
Add diagnostics code constants
Add sanitizer
Prevent local path / settings / CLAUDE / token leaks
```

验收：隐私测试通过。

### Commit 3：static diagnostics collector

```text
Implement CollectStaticDiagnostics
Check version/settings/global guidance/skill entry/project marker
Return markdown report
```

验收：`settings.invalid` 出现在 Blocking；`project_marker.missing` 出现在 Warning。

### Commit 4：runtime diagnostics collector

```text
Implement CollectRuntimeDiagnostics
Check UE editor/MCP/Bridge/runtime profile/config/write permission/risk command
```

验收：`token_missing` 是 Warning；`bridge.disconnected` 是 Blocking；`risk_command_missing` 是 Warning 且 `blocked_commands=close_editor`。

### Commit 5：ToolResult semantics

```text
Ensure diagnostics report Blocking does not make ok=false
Only internal execution failure returns ok=false
```

验收：diagnostics_with_blocking_still_ok_contract 通过。

### Commit 6：Bridge / Validator / command entry

```text
Register run_blueprinthelper_diagnostics
Map /blueprinthelper-diagnostics to static
Map /blueprinthelper-diagnostics --runtime to runtime
Allow diagnostics without write token
```

验收：ReadOnly 下可运行；`write_permission.disabled` 不阻止 diagnostics。

### Commit 7：Protocol regression

```text
Add no JSON arrays tests
Add no validation/write_ref/transaction_id tests
Add no suggested action tests
```

验收：字段稿验收项全部通过。

---

## 17. 第一版不做的内容

```text
1. 不修复配置。
2. 不迁移 settings.json。
3. 不写 Project Marker。
4. 不写全局 CLAUDE.md。
5. 不读取蓝图 LogicMD / LogicJson。
6. 不判断具体蓝图任务是否能完成。
7. 不生成 AgentPlan。
8. 不执行写工具。
9. 不返回 blocking/warning/info JSON arrays。
10. 不输出 Suggested action / action code。
11. 不输出本地绝对路径。
12. 不输出 settings.json / CLAUDE.md 全文。
```

---

## 18. 最小验收标准

static 正常：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "run_blueprinthelper_diagnostics",
  "status": "completed",
  "modified": false,
  "target": {
    "diagnostics_mode": "static"
  },
  "data": {
    "schema": "Diagnostics.v1",
    "markdown": "## Blocking
None

## Warning
None

## Info
- `version.match`
- `settings.valid`
- `global_guidance.present`
- `skill_entry.valid`"
  }
}
```

runtime token 缺失：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "run_blueprinthelper_diagnostics",
  "status": "completed",
  "modified": false,
  "target": {
    "diagnostics_mode": "runtime"
  },
  "data": {
    "schema": "Diagnostics.v1",
    "markdown": "## Blocking
None

## Warning
- `write_permission.disabled`
  - reason: `token_missing`

## Info
- `ue_editor.running`
- `mcp_server.available`
- `bridge.connected`
- `runtime_profile.available`
- `config_status.valid`"
  }
}
```

runtime bridge disconnected：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "run_blueprinthelper_diagnostics",
  "status": "completed",
  "modified": false,
  "target": {
    "diagnostics_mode": "runtime"
  },
  "data": {
    "schema": "Diagnostics.v1",
    "markdown": "## Blocking
- `bridge.disconnected`

## Warning
None

## Info
- `ue_editor.running`
- `mcp_server.available`"
  }
}
```

工具自身失败：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "run_blueprinthelper_diagnostics",
  "status": "failed",
  "modified": false,
  "target": {
    "diagnostics_mode": "runtime"
  },
  "error": {
    "code": "diagnostics_failed",
    "stage": "run_diagnostics",
    "message": "BlueprintHelper diagnostics could not be executed.",
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
blocking[]
warning[]
info[]
Suggested action
action_code
local absolute path
settings.json content
CLAUDE.md content
Token
secret
```

---

## 19. 实现风险

### 19.1 把诊断 Blocking 当成工具失败

风险：`bridge.disconnected` 出现在 Markdown Blocking 后返回 `ok=false`。

处理：ToolResult 成功与诊断报告内容分离，Contract test 锁定。

### 19.2 返回 JSON arrays

风险：为了方便 Agent 解析，额外返回 `data.blocking[]`。

处理：字段稿明确只返回 `data.markdown`，Contract test 禁止 `blocking/warning/info` arrays。

### 19.3 输出修复建议

风险：Markdown 中加入 Suggested action。


### 19.4 泄露本地路径或配置

风险：底层异常或 settings parser 把路径/内容拼入 markdown。

处理：Sanitizer + code 白名单 + 固定 message，隐私测试覆盖。

### 19.5 risk_command_missing 被误判为普通写阻断

风险：risk_command 缺失导致 Agent 认为蓝图写入不可用。

处理：Diagnostics 中 `risk_command.disabled` 是 Warning，`blocked_commands` 仅 `close_editor`，普通写权限由 `write_permission` 判断。
