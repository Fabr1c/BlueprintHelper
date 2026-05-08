# BlueprintHelper Runtime Profile UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：runtime_profile 字段确认稿  
本文边界：确认 runtime_profile 的 Agent-facing 返回字段、UE/MCP 聚合字段、正常态极简返回、异常态细节返回、负向稀疏 unavailable 结构，以及 data.schema 短命名规则。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

runtime_profile 采用以下字段口径：

```text
1. operation 建议使用 get_runtime_profile。
2. 顶层 schema 继续使用 BlueprintHelper.McpToolResult.v1。
3. data.schema 使用短命名 RuntimeProfile.v1。
4. runtime_profile 正常态只返回 runtime_profile.status=ok。
5. 正常态不返回 write_permission / bridge / config_status / risk_command / unavailable / active_profile / tool_capabilities / version 等细节。
6. 异常态、降级态、阻断态才返回必要诊断字段。
7. tool capabilities 使用负向稀疏模式，只列 unavailable / disabled / degraded / blocked 项。
8. runtime_profile 不是 tool schema，也不是 AgentGuide 替代品。
9. 命名偏好、蓝图/C++边界、详细工具规则由 CLAUDE.md / Skill / AgentGuide 提供，不在正常 runtime_profile 中展开。
10. write_permission / risk_command / bridge / config_status 只在异常或影响当前任务时返回。
11. runtime_profile 是只读工具，modified=false。
```

---

## 1. 工具定位

`get_runtime_profile` 负责：

```text
向 Agent 提供当前 UE/MCP/配置链路是否可执行任务的运行时事实。
```

它聚合：

```text
MCP Server 状态
UE Bridge 状态
UE 插件运行状态
settings.json 运行时配置状态
write_permission / Token 状态
risk_command 状态
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

---

## 2. operation

建议固定使用：

```json
"operation": "get_runtime_profile"
```

如 MCP 侧最终工具名为 `blueprinthelper_get_runtime_profile`，Agent-facing `operation` 仍建议保持：

```text
get_runtime_profile
```

---

## 3. data.schema

固定短命名：

```json
"schema": "RuntimeProfile.v1"
```

不使用：

```text
BlueprintHelper.RuntimeProfile.v1
BlueprintHelper.MCP.RuntimeProfile.v1
BlueprintHelper.Tools.RuntimeProfile.v1
```

---

# 4. 正常态返回

## 4.1 正常态 JSON

正常态只返回：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_runtime_profile",
  "trace_id": "trace_20260503_0001",
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

---

## 4.2 正常态字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `RuntimeStatus` | `EBlueprintHelperRuntimeStatus` | `data.runtime_profile.status` | `string enum` | 是 | 正常态固定为 `ok`。 |

---

## 4.3 正常态不返回字段

正常态不返回：

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

原因：

```text
1. 正常信息不污染 Agent 上下文。
2. Agent 规则来源是 CLAUDE.md / Skill / AgentGuide。
3. runtime_profile 只用于运行时异常、权限、阻断和降级判断。
4. 当前运行时正常时，Agent 只需要知道 status=ok。
```

---

# 5. 异常态 / 降级态返回

异常态才返回必要细节。

建议 `runtime_profile.status` 枚举：

```text
ok
degraded
blocked
config_unavailable
bridge_disconnected
```

也可以将具体原因放在子字段中，顶层保持：

```text
degraded
blocked
```

---

## 5.1 write_permission 不可用

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_runtime_profile",
  "trace_id": "trace_20260503_0002",
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

---

## 5.2 Bridge 断开

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_runtime_profile",
  "trace_id": "trace_20260503_0003",
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

---

## 5.3 config unavailable

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_runtime_profile",
  "trace_id": "trace_20260503_0004",
  "status": "completed",
  "modified": false,

  "data": {
    "schema": "RuntimeProfile.v1",
    "runtime_profile": {
      "status": "blocked",

      "config_status": {
        "status": "config_unavailable"
      },

      "write_permission": {
        "enabled": false,
        "reason": "config_unavailable"
      },

      "unavailable": [
        {
          "cluster": "runtime",
          "capability": "config",
          "status": "blocked",
          "reason": "config_unavailable"
        }
      ]
    }
  }
}
```

---

## 5.4 risk_command 不可用

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_runtime_profile",
  "trace_id": "trace_20260503_0005",
  "status": "completed",
  "modified": false,

  "data": {
    "schema": "RuntimeProfile.v1",
    "runtime_profile": {
      "status": "degraded",

      "risk_command": {
        "enabled": false,
        "reason": "risk_command_missing",
        "blocked_commands": [
          "close_editor"
        ]
      },

      "unavailable": [
        {
          "cluster": "lifecycle",
          "capability": "close_editor",
          "status": "blocked",
          "reason": "risk_command_missing"
        }
      ]
    }
  }
}
```

---

# 6. unavailable 负向稀疏结构

`unavailable` 只列当前不可用、禁用、降级或阻断的能力。

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Cluster` | `FString` 或 enum | `data.runtime_profile.unavailable[].cluster` | `string` | 是 | 能力簇。 |
| `Capability` | `FString` 或 enum | `data.runtime_profile.unavailable[].capability` | `string` | 是 | 能力名。 |
| `Status` | `EBlueprintHelperCapabilityStatus` | `data.runtime_profile.unavailable[].status` | `string enum` | 是 | `unavailable` / `disabled` / `degraded` / `blocked`。 |
| `Reason` | `FString` 或 enum | `data.runtime_profile.unavailable[].reason` | `string` | 是 | 稳定 reason 枚举。 |

不返回：

```text
severity
stop_and_report
message
required_tool
available tools
complete schema
```

Agent 根据任务需求、missing_capability_policy、当前 unavailable 项和是否存在安全替代路径自行判断 stop_and_report。

---

# 7. write_permission 字段

仅异常 / 降级 / 阻断时返回。

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bEnabled` | `bool` | `data.runtime_profile.write_permission.enabled` | `boolean` | 是 | 写权限是否可用。 |
| `Reason` | `FString` 或 enum | `data.runtime_profile.write_permission.reason` | `string` | disabled 时 | 稳定 reason 枚举。 |

reason 示例：

```text
token_missing
token_invalid
token_expired
token_missing_or_invalid
config_unavailable
setup_not_completed
safety_profile_read_only
```

---

# 8. bridge 字段

仅异常时返回。

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `BridgeStatus` | `EBlueprintHelperBridgeStatus` | `data.runtime_profile.bridge.status` | `string enum` | 是 | 例如 `disconnected`。 |

正常不返回：

```text
bridge.status=connected
```

---

# 9. config_status 字段

仅异常时返回。

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `ConfigStatus` | `EBlueprintHelperConfigStatus` | `data.runtime_profile.config_status.status` | `string enum` | 是 | 例如 `config_unavailable`。 |

正常不返回：

```text
config_status.valid
settings.valid
```

---

# 10. risk_command 字段

仅 risk command 不可用或阻断任务相关命令时返回。

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bEnabled` | `bool` | `data.runtime_profile.risk_command.enabled` | `boolean` | 是 | risk command 是否启用。 |
| `Reason` | `FString` 或 enum | `data.runtime_profile.risk_command.reason` | `string` | disabled 时 | 稳定 reason。 |
| `BlockedCommands` | `TArray<FString>` | `data.runtime_profile.risk_command.blocked_commands` | `array<string>` | 可选 | 被阻断的 lifecycle commands。 |

示例 reason：

```text
risk_command_missing
risk_command_invalid
command_not_authorized
```

---

# 11. error：runtime_profile 工具自身失败

如果 runtime_profile 工具自身失败，例如 MCP 内部异常：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "get_runtime_profile",
  "trace_id": "trace_20260503_0006",
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

注意：

```text
runtime_profile.status=blocked 是工具成功返回运行时阻断状态。
ok=false/status=failed 是工具自身失败。
```

---

# 12. UE/MCP 建议结构体

```cpp
struct FBlueprintHelperRuntimeProfileResultData
{
    FString Schema; // RuntimeProfile.v1
    FBlueprintHelperRuntimeProfile RuntimeProfile;
};

struct FBlueprintHelperRuntimeProfile
{
    FString Status; // ok | degraded | blocked

    // Optional only when abnormal/degraded/blocked.
    TOptional<FBlueprintHelperWritePermissionStatus> WritePermission;
    TOptional<FBlueprintHelperBridgeRuntimeStatus> Bridge;
    TOptional<FBlueprintHelperConfigRuntimeStatus> ConfigStatus;
    TOptional<FBlueprintHelperRiskCommandStatus> RiskCommand;
    TArray<FBlueprintHelperUnavailableCapability> Unavailable;
};

struct FBlueprintHelperWritePermissionStatus
{
    bool bEnabled = false;
    FString Reason;
};

struct FBlueprintHelperBridgeRuntimeStatus
{
    FString Status; // disconnected, etc.
};

struct FBlueprintHelperConfigRuntimeStatus
{
    FString Status; // config_unavailable, etc.
};

struct FBlueprintHelperRiskCommandStatus
{
    bool bEnabled = false;
    FString Reason;
    TArray<FString> BlockedCommands;
};

struct FBlueprintHelperUnavailableCapability
{
    FString Cluster;
    FString Capability;
    FString Status; // unavailable | disabled | degraded | blocked
    FString Reason;
};
```

正常态序列化时只输出：

```json
{
  "status": "ok"
}
```

---

# 13. 验收标准

```text
1. operation 固定为 get_runtime_profile。
2. data.schema 固定为 RuntimeProfile.v1。
3. 顶层 schema 为 BlueprintHelper.McpToolResult.v1。
4. 正常态只返回 runtime_profile.status=ok。
5. 正常态不返回 tool_capabilities / active_profile / write_permission / bridge / config_status / risk_command / unavailable。
6. 异常态才返回必要诊断字段。
7. unavailable 使用负向稀疏结构。
8. unavailable item 只包含 cluster / capability / status / reason。
9. runtime_profile 不是 tool schema。
10. runtime_profile 不是 AgentGuide 替代品。
11. 命名偏好、蓝图/C++边界、工具规则不在正常 runtime_profile 中展开。
12. runtime_profile 工具自身失败时 ok=false/status=failed/error。
