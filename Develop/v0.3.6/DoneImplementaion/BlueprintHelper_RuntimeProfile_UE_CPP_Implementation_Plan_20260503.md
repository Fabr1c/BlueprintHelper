# BlueprintHelper get_runtime_profile UE 侧 C++ 可执行实现计划

状态：[x] 已完成
日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置实现  
来源字段稿：`BlueprintHelper_RuntimeProfile_UE_FieldMapping_20260503.md`  
实现范围：UE 插件侧 C++ + UE Bridge 聚合层  
不包含：MCP Server TypeScript 工具注册、AgentGuide / Skill 文档、setup 写入逻辑、diagnostics markdown 工具

---

## 0. 实现目标

`get_runtime_profile` 向 Agent 提供当前 UE/MCP/配置链路是否可执行任务的运行时事实。

它聚合：

```text
不可用工具簇能力
```

它不负责：

```text
展开完整工具 schema
替代 AgentGuide
返回所有可用工具
返回项目规则全文
返回命名偏好全文
返回蓝图/C++边界全文
返回 Transaction Journal / Review 数据
```

字段契约核心点：

```text
1. operation 固定为 get_runtime_profile。
2. data.schema 固定短命名 RuntimeProfile.v1。
3. 正常态只返回 runtime_profile.status=ok。
4. 正常态不返回 version / bridge / config_status / write_permission / risk_command / active_profile / unavailable。
5. 异常态、降级态、阻断态才返回必要诊断字段。
6. unavailable 使用负向稀疏结构。
7. unavailable item 只包含 cluster / capability / status / reason。
8. runtime_profile 是只读工具，modified=false。
9. runtime_profile.status=blocked 仍是工具成功执行；只有工具自身失败才 ok=false/status=failed。
```

---

## 1. 当前依赖与实现边界

本计划假设当前 UE 插件已经有或即将有：

```text
FBlueprintHelperToolResultBase
FBlueprintHelperToolResultBuilder
FBlueprintHelperBridgeRouter
FBlueprintHelperRequestValidator
Bridge auth token 校验逻辑
risk command gate
settings.json 读取逻辑
tool registry / command registry
```


---

## 2. Phase A：新增 Runtime Profile 类型文件

### 2.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperRuntimeProfileTypes.h
Source/BlueprintHelper/Private/Services/BlueprintHelperRuntimeProfileTypes.cpp
```

如当前已有 diagnostics/runtime 类型，可在现有文件中追加 DTO，但不要与 diagnostics markdown DTO 混用。

### 2.2 新增枚举

```cpp
enum class EBlueprintHelperRuntimeStatus : uint8
{
    Ok,
    Degraded,
    Blocked,
    ConfigUnavailable,
    BridgeDisconnected
};

enum class EBlueprintHelperCapabilityStatus : uint8
{
    Unavailable,
    Disabled,
    Degraded,
    Blocked
};

enum class EBlueprintHelperWritePermissionReason : uint8
{
    Ok,
    TokenMissing,
    TokenInvalid,
    TokenExpired,
    TokenMissingOrInvalid,
    ConfigUnavailable,
    SetupNotCompleted,
    SafetyProfileReadOnly
};

enum class EBlueprintHelperRiskCommandReason : uint8
{
    Ok,
    RiskCommandMissing,
    RiskCommandInvalid,
    CommandNotAuthorized
};

enum class EBlueprintHelperConfigRuntimeStatus : uint8
{
    Valid,
    ConfigUnavailable
};

enum class EBlueprintHelperBridgeRuntimeStatus : uint8
{
    Connected,
    Disconnected
};

enum class EBlueprintHelperRuntimeProfileStage : uint8
{
    CollectRuntimeProfile,
    ReadSettings,
    CheckBridge,
    CheckWritePermission,
    CheckRiskCommand,
    CheckCapabilities
};

enum class EBlueprintHelperRuntimeProfileErrorCode : uint8
{
    RuntimeProfileUnavailable,
    SettingsReadFailed,
    InternalError
};
```

### 2.3 字符串序列化要求

稳定输出：

```text
ok
degraded
blocked
config_unavailable
bridge_disconnected
token_missing
token_invalid
token_expired
token_missing_or_invalid
risk_command_missing
risk_command_invalid
command_not_authorized
unavailable
disabled
```

不要输出 C++ 枚举名。

---

## 3. Phase B：Agent-facing 数据结构

### 3.1 Result data

```cpp
struct FBlueprintHelperRuntimeProfileResultData
{
    FString Schema = TEXT("RuntimeProfile.v1");
    FBlueprintHelperRuntimeProfile RuntimeProfile;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.2 Runtime profile

```cpp
struct FBlueprintHelperRuntimeProfile
{
    FString Status; // ok | degraded | blocked | config_unavailable | bridge_disconnected

    TOptional<FBlueprintHelperWritePermissionStatus> WritePermission;
    TOptional<FBlueprintHelperBridgeStatusData> Bridge;
    TOptional<FBlueprintHelperConfigStatusData> ConfigStatus;
    TOptional<FBlueprintHelperRiskCommandStatus> RiskCommand;

    TArray<FBlueprintHelperUnavailableCapability> Unavailable;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.3 Optional 子结构

```cpp
struct FBlueprintHelperWritePermissionStatus
{
    bool bEnabled = false;
    FString Reason;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperBridgeStatusData
{
    FString Status; // disconnected

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperConfigStatusData
{
    FString Status; // config_unavailable

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperRiskCommandStatus
{
    bool bEnabled = false;
    FString Reason;
    TArray<FString> BlockedCommands;

    TSharedRef<FJsonObject> ToJson() const;
};

struct FBlueprintHelperUnavailableCapability
{
    FString Cluster;
    FString Capability;
    FString Status; // unavailable | disabled | degraded | blocked
    FString Reason;

    TSharedRef<FJsonObject> ToJson() const;
};
```

### 3.4 正常态序列化硬约束

如果 `RuntimeProfile.Status == "ok"`，`ToJson()` 必须只输出：

```json
{
  "status": "ok"
}
```

即使内部对象中已计算出 version、write_permission、risk_command，也不能在正常态输出。

### 3.5 禁止正常态输出字段

正常态不得输出：

```text
version
bridge
config_status
write_permission
risk_command
active_profile
tool_capabilities
unavailable
available tools
naming_preference_summary
blueprint_cpp_boundary_summary
safety_profile
missing_capability_policy
recommended_workflow
project_root
```

---

## 4. Phase C：新增 RuntimeProfileService

### 4.1 新增文件

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperRuntimeProfileService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperRuntimeProfileService.cpp
```

### 4.2 服务接口

```cpp
class FBlueprintHelperRuntimeProfileService
{
public:
    FBlueprintHelperToolResultBase Execute(const TSharedPtr<FJsonObject>& Payload) const;

private:
    bool CollectRuntimeFacts(
        FBlueprintHelperRuntimeFacts& OutFacts,
        FBlueprintHelperToolError& OutError) const;

    FBlueprintHelperRuntimeProfile BuildRuntimeProfile(
        const FBlueprintHelperRuntimeFacts& Facts) const;

    void AddUnavailableCapabilities(
        const FBlueprintHelperRuntimeFacts& Facts,
        FBlueprintHelperRuntimeProfile& InOutProfile) const;
};
```

### 4.3 Runtime facts 内部结构

该结构可包含完整运行时事实，但不直接序列化给 Agent。

```cpp
struct FBlueprintHelperRuntimeFacts
{
    bool bBridgeConnected = true;
    bool bConfigAvailable = true;

    bool bWritePermissionEnabled = true;
    FString WritePermissionReason = TEXT("ok");

    bool bRiskCommandEnabled = true;
    FString RiskCommandReason = TEXT("ok");
    TArray<FString> BlockedRiskCommands;

    // 内部使用，不正常态输出。
    FString Version;
    FString SafetyProfile;
    FString MissingCapabilityPolicy;

    TArray<FBlueprintHelperUnavailableCapability> UnavailableCapabilities;
};
```

---

## 5. Phase D：运行时事实采集


UE 侧 runtime profile 被调用时，通常说明 Bridge 已经连通。  

```cpp
Facts.bBridgeConnected = BridgeState.IsConnected();
```

如果 Bridge 断开：

```text
runtime_profile.status=blocked
bridge.status=disconnected
unavailable:
  cluster=runtime
  capability=bridge
  status=blocked
  reason=bridge_disconnected
```

注意：如果 Bridge 断开到连工具都无法调用，则该工具可能根本无法返回。MCP 侧可自行返回同形结构。本计划只处理 UE 能返回的场景。


复用 SettingsService：

```cpp
Facts.bConfigAvailable = SettingsService.IsRuntimeConfigAvailable();
```

如果不可用：

```text
runtime_profile.status=blocked 或 config_unavailable
config_status.status=config_unavailable
write_permission.enabled=false
write_permission.reason=config_unavailable
unavailable:
  cluster=runtime
  capability=config
  status=blocked
  reason=config_unavailable
```

不要在 runtime profile 中展开 settings 细节、缺失字段、路径。


写权限由现有 token gate / profile gate 计算：

```cpp
Facts.bWritePermissionEnabled = AuthService.IsWritePermissionEnabled();
Facts.WritePermissionReason = AuthService.GetWritePermissionReason();
```

若不可用：

```text
runtime_profile.status=degraded
write_permission.enabled=false
write_permission.reason=token_missing / token_invalid / safety_profile_read_only
unavailable:
  cluster=graph_write
  capability=write
  status=blocked
  reason=write_permission_disabled
```

如果 ReadOnly profile 禁止写：

```text
write_permission.reason=safety_profile_read_only
```


risk command gate 计算：

```cpp
Facts.bRiskCommandEnabled = RiskCommandService.IsEnabled();
Facts.RiskCommandReason = RiskCommandService.GetReason();
Facts.BlockedRiskCommands = RiskCommandService.GetBlockedCommands();
```

若仅 close_editor 不可用：

```text
runtime_profile.status=degraded
risk_command.enabled=false
risk_command.reason=risk_command_missing
risk_command.blocked_commands=["close_editor"]
unavailable:
  cluster=lifecycle
  capability=close_editor
  status=blocked
  reason=risk_command_missing
```

普通蓝图读写不应因为 `close_editor` blocked 而整体 status=blocked。

### 5.5 tool capability negative sparse

只记录不可用/禁用/降级/阻断项：

```cpp
Facts.UnavailableCapabilities.Add({
    Cluster: TEXT("graph_write"),
    Capability: TEXT("merge"),
    Status: TEXT("unavailable"),
    Reason: TEXT("not_implemented")
});
```

不要返回完整可用工具列表。

### 5.6 unavailable item 字段限制

每项只允许：

```text
cluster
capability
status
reason
```

不得输出：

```text
severity
stop_and_report
message
required_tool
available_tools
schema
```

---

## 6. Phase E：status 判定规则

### 6.1 正常态

如果所有关键事实正常，且没有 unavailable 项需要向 Agent 报告：

```text
status=ok
```

输出：

```json
{
  "runtime_profile": {
    "status": "ok"
  }
}
```

### 6.2 degraded

适用于：

```text
只读任务仍可执行，但部分能力不可用。
写权限不可用但读能力可用。
risk_command close_editor 不可用。
某些非当前任务关键能力 degraded。
```

输出必要字段：

```text
write_permission
risk_command
unavailable
```

### 6.3 blocked

适用于：

```text
Bridge disconnected
config unavailable
runtime profile 无法确认执行安全
关键 runtime 能力阻断
```

输出必要字段：

```text
bridge
config_status
write_permission
unavailable
```

### 6.4 config_unavailable / bridge_disconnected


```text
config_unavailable
bridge_disconnected
```

推荐第一版采用：

```text
status=blocked
```

并通过子字段表达具体原因：

```json
"config_status": {"status": "config_unavailable"}
```

但如果前端/测试希望直接 status=config_unavailable，也应统一到同一个测试口径。建议以字段稿示例为准：异常示例使用 `status=blocked`。

---

## 7. Phase F：ToolResult 构建

### 7.1 正常态

```cpp
FBlueprintHelperToolResultBase BuildRuntimeProfileOk(
    const FString& TraceId)
{
    FBlueprintHelperToolResultBase Result;
    Result.bOk = true;
    Result.Schema = TEXT("BlueprintHelper.McpToolResult.v1");
    Result.Operation = TEXT("get_runtime_profile");
    Result.TraceId = TraceId;
    Result.Status = TEXT("completed");
    Result.bModified = false;

    FBlueprintHelperRuntimeProfileResultData Data;
    Data.RuntimeProfile.Status = TEXT("ok");

    Result.Data = Data.ToJson();
    return Result;
}
```

输出必须是：

```json
{
  "data": {
    "schema": "RuntimeProfile.v1",
    "runtime_profile": {
      "status": "ok"
    }
  }
}
```

### 7.2 degraded / blocked

```cpp
FBlueprintHelperRuntimeProfileResultData Data;
Data.RuntimeProfile.Status = TEXT("degraded");
Data.RuntimeProfile.WritePermission = ...;
Data.RuntimeProfile.Unavailable = ...;
```

输出只包含异常相关字段。

### 7.3 工具自身失败

如果采集 runtime profile 本身发生内部异常：

```cpp
Result.bOk = false;
Result.Operation = TEXT("get_runtime_profile");
Result.Status = TEXT("failed");
Result.bModified = false;
Result.Error = {
    code = "runtime_profile_unavailable",
    stage = "collect_runtime_profile",
    message = "Runtime profile could not be collected.",
    retryable = true
};
```

注意：

```text
runtime_profile.status=blocked 是工具成功返回运行时阻断。
ok=false/status=failed 是工具自身失败。
```

---

## 8. Phase G：Bridge Router 接入

### 8.1 新增 command

```text
get_runtime_profile
```

如果 MCP 工具名是：

```text
blueprinthelper_get_runtime_profile
```

UE Bridge command 仍建议使用：

```text
get_runtime_profile
```

Agent-facing operation 固定：

```text
get_runtime_profile
```

### 8.2 Router 分支

```cpp
if (Request.Command == TEXT("get_runtime_profile"))
{
    return HandleGetRuntimeProfile(Request);
}
```

### 8.3 Handler

```cpp
FBlueprintHelperBridgeResponse FBlueprintHelperBridgeRouter::HandleGetRuntimeProfile(
    const FBlueprintHelperBridgeRequest& Req) const
{
    FBlueprintHelperToolResultBase Result =
        RuntimeProfileService.Execute(Req.Payload);

    FBlueprintHelperBridgeResponse Resp = Result.bOk
        ? FBlueprintHelperBridgeResponse::Success(Req.RequestId)
        : FBlueprintHelperBridgeResponse::Error(
            Req.RequestId,
            EBlueprintHelperBridgeError::ExecutionFailed,
            Result.Error.IsSet() ? Result.Error->Message : TEXT("runtime profile failed"));

    Resp.Result = Result.ToJson();
    return Resp;
}
```

### 8.4 Request payload

runtime profile 第一版不需要业务参数：

```json
{}
```

可选扩展：

```text
task_hint
required_capabilities
```

但本字段稿没有定义这些字段，第一版不实现。

---

## 9. Phase H：RequestValidator / 权限

### 9.1 Validator

```cpp
if (Command == TEXT("get_runtime_profile"))
{
    // Empty payload allowed.
    return true;
}
```

### 9.2 权限

runtime profile 是只读工具：

```text
不需要 write token
不生成 transaction
不写 Journal
modified=false
ReadOnly 下允许
```

即使 write_permission 不可用，也不能阻止 runtime profile 返回。  
否则 Agent 无法知道写权限缺失。

---

## 10. Phase I：与 diagnostics 的边界

runtime_profile 不是 diagnostics markdown。

runtime_profile：

```text
任务前运行时事实摘要。
正常态极简。
异常态返回必要机器可读字段。
```

diagnostics：

```text
安装 / 配置 / Bridge / runtime 链路排查。
返回 data.markdown。
Blocking/Warning/Info 分区。
```

本工具不返回：

```text
data.markdown
blocking/warning/info markdown
settings 字段级错误
完整版本号列表
Project Marker 检查
local filesystem path
```

如果 runtime profile 返回 `config_status.config_unavailable`，Agent 可以另行调用 diagnostics 定位，但 runtime_profile 不展开细节。

---

## 11. Phase J：SettingsService 最小实现

如果当前 settings.json 读取逻辑尚未抽象，先新增：

```text
Source/BlueprintHelper/Public/Services/BlueprintHelperSettingsService.h
Source/BlueprintHelper/Private/Services/BlueprintHelperSettingsService.cpp
```

最小接口：

```cpp
class FBlueprintHelperSettingsService
{
public:
    bool IsRuntimeConfigAvailable() const;
    FString GetConfigUnavailableReason() const;

    bool IsWritePermissionEnabled() const;
    FString GetWritePermissionReason() const;

    bool IsRiskCommandEnabled() const;
    FString GetRiskCommandReason() const;
    TArray<FString> GetBlockedRiskCommands() const;

    TArray<FBlueprintHelperUnavailableCapability> GetUnavailableCapabilities() const;
};
```

正常态不要序列化这些详细结果。

---

## 12. Phase K：Capability unavailable 收敛

### 12.1 能力簇建议

```text
runtime
graph_write
asset_factory
component
class_settings
cleanup
rollback
ownership
validation
lifecycle
enhanced_input
logic_read
```

### 12.2 capability 示例

```text
bridge
config
write
append
replace
patch
merge
cleanup_block
rollback_cleanup_transaction
convert_block_to_user_owned
compile
save
close_editor
input_mapping_edit
```

### 12.3 reason 示例

```text
not_implemented
disabled_by_profile
write_permission_disabled
bridge_disconnected
config_unavailable
risk_command_missing
token_missing
token_invalid
```

### 12.4 不要返回 severity

Agent 的 stop_and_report 决策不由 runtime_profile 直接下达。  
runtime profile 只报告事实：

```json
{
  "cluster": "graph_write",
  "capability": "merge",
  "status": "unavailable",
  "reason": "not_implemented"
}
```

---

## 13. 自动化测试计划

新增：

```text
Source/BlueprintHelper/Private/Tests/BlueprintHelperRuntimeProfileContractTests.cpp
Source/BlueprintHelper/Private/Tests/BlueprintHelperRuntimeProfileStateTests.cpp
```

### 13.1 Contract tests

```text
1. runtime_profile_ok_minimal_contract
   - ok=true
   - operation=get_runtime_profile
   - status=completed
   - modified=false
   - data.schema=RuntimeProfile.v1
   - data.runtime_profile.status=ok
   - 不返回 version / bridge / config_status / write_permission / risk_command / active_profile / unavailable

2. runtime_profile_write_permission_degraded_contract
   - status=completed
   - runtime_profile.status=degraded
   - write_permission.enabled=false
   - write_permission.reason=token_missing
   - unavailable[] 存在
   - unavailable item 只有 cluster/capability/status/reason

3. runtime_profile_bridge_blocked_contract
   - runtime_profile.status=blocked
   - bridge.status=disconnected
   - unavailable 包含 runtime/bridge/blocked/bridge_disconnected

4. runtime_profile_config_unavailable_contract
   - runtime_profile.status=blocked
   - config_status.status=config_unavailable
   - write_permission.enabled=false
   - write_permission.reason=config_unavailable

5. runtime_profile_tool_failure_contract
   - ok=false
   - status=failed
   - error.code=runtime_profile_unavailable
```

### 13.2 State tests

```text
1. normal_profile_suppresses_all_details
2. token_missing_returns_write_permission_only_when_degraded
3. risk_command_missing_does_not_block_regular_runtime
4. config_unavailable_blocks_write_permission
5. unavailable_does_not_include_available_tools
6. unavailable_does_not_include_stop_and_report
7. read_only_profile_returns_write_permission_disabled
```

---

## 14. 推荐提交顺序

### Commit 1：DTO 与序列化

```text
Add RuntimeProfile result types
Add runtime status / capability status enums
Add normal-state minimal serialization
```

验收：

```text
status=ok 时只输出 runtime_profile.status=ok。
```

### Commit 2：Settings / auth / risk facts

```text
Add RuntimeProfileService facts collection
Read config availability
Read write permission/token status
Read risk command status
```

验收：

```text
token_missing / config_unavailable / risk_command_missing 可映射。
```

### Commit 3：negative sparse unavailable

```text
Build unavailable[] only for abnormal capabilities
Restrict unavailable item fields
Suppress available tools
```

验收：

```text
unavailable item 无 severity/message/required_tool。
```

### Commit 4：status 判定

```text
Implement ok/degraded/blocked decision
Ensure normal state suppresses details
Ensure bridge/config blocked states return necessary fields
```

验收：

```text
正常态极简，异常态可诊断。
```

### Commit 5：Bridge / Validator

```text
Register get_runtime_profile command
Allow empty payload
Make command token-free and read-only
```

验收：

```text
write_permission disabled 时仍能调用 runtime_profile。
```

### Commit 6：Contract tests

```text
Add normal/minimal tests
Add abnormal/degraded tests
Add tool-failure tests
```

验收：

```text
字段稿所有验收项通过。
```

---

## 15. 第一版不做的内容

```text
1. 不返回完整 tool schema。
2. 不返回所有可用工具。
3. 不返回 active_profile。
4. 不返回 safety_profile。
5. 不返回 naming_preference_summary。
6. 不返回 blueprint_cpp_boundary_summary。
7. 不返回 version。
8. 不返回 project_root。
9. 不返回 Transaction Journal / Review 数据。
10. 不返回 diagnostics markdown。
11. 不输出 unavailable severity / message / stop_and_report / required_tool。
12. 不支持 task_hint 参数。
```

---

## 16. 最小验收标准

正常态：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_runtime_profile",
  "status": "completed",
  "modified": false,
  "data": {
    "schema": "RuntimeProfile.v1",
    "runtime_profile": {
      "status": "ok"
    }
  }
}
```

write_permission 异常：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_runtime_profile",
  "status": "completed",
  "modified": false,
  "data": {
    "schema": "RuntimeProfile.v1",
    "runtime_profile": {
      "status": "degraded",
      "write_permission": {
        "enabled": false,
        "reason": "token_missing"
      },
      "unavailable": [
        {
          "cluster": "graph_write",
          "capability": "write",
          "status": "blocked",
          "reason": "write_permission_disabled"
        }
      ]
    }
  }
}
```

Bridge 断开：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_runtime_profile",
  "status": "completed",
  "modified": false,
  "data": {
    "schema": "RuntimeProfile.v1",
    "runtime_profile": {
      "status": "blocked",
      "bridge": {
        "status": "disconnected"
      },
      "unavailable": [
        {
          "cluster": "runtime",
          "capability": "bridge",
          "status": "blocked",
          "reason": "bridge_disconnected"
        }
      ]
    }
  }
}
```

工具自身失败：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_runtime_profile",
  "status": "failed",
  "modified": false,
  "error": {
    "code": "runtime_profile_unavailable",
    "stage": "collect_runtime_profile",
    "message": "Runtime profile could not be collected.",
    "retryable": true
  }
}
```

---

## 17. 实现风险

### 17.1 正常态意外泄露详细字段

风险：

```text
内部 facts 对象被直接序列化，导致 version / safety_profile / write_permission 正常态泄露。
```

处理：

```text
RuntimeProfile.ToJson() 必须按 status 白名单输出。
Contract test 锁定正常态只返回 status。
```

### 17.2 runtime_profile 被误用为 tool schema

风险：

```text
Agent 看到 unavailable=[] 后误以为所有工具 schema 都已确认。
```

处理：

```text
正常态不返回 unavailable=[]。
异常态只返回 unavailable items。
不返回 available tools。
```

### 17.3 write_permission disabled 阻止 runtime_profile 自身

风险：

```text
token missing 时 runtime_profile 也被 auth gate 拦截。
```

处理：

```text
get_runtime_profile 必须是 token-free read-only command。
```

### 17.4 risk_command 缺失被误判为全局 blocked

风险：

```text
close_editor 不可用导致普通蓝图写入被阻止。
```

处理：

```text
risk_command_missing → degraded。
unavailable 只列 lifecycle/close_editor。
```
