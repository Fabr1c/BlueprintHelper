# BlueprintHelper Project Context / Project Marker / Setup State UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：Project Context / Project Marker / Setup State 字段确认稿  
本文边界：确认 `read_project_context`、`check_project_marker`、`check_setup_state` 三个只读工具的 Agent-facing 返回字段、UE/MCP 侧结构体映射、正常态极简返回、异常态 reason、隐私边界和非写入边界。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

Project Context / Project Marker / Setup State 工具簇采用以下字段口径：

```text
1. 增加 read_project_context 只读工具。
2. read_project_context 返回 project_context.status / project_detected / project_marker / workflow_enabled。
3. read_project_context 不返回 project_root / uproject_path / settings_path / CLAUDE.md 全文。
4. 增加 check_project_marker 只读工具。
5. check_project_marker 返回 project_marker.status / workflow_enabled / reason。
6. check_project_marker 不返回 marker 文本 / 文件路径 / 行号。
7. 增加 check_setup_state 只读工具。
8. check_setup_state 正常态只返回 setup_state.status=ok。
9. check_setup_state 异常态返回 status / reason。
10. check_setup_state 不展开 settings 具体损坏类型。
11. 本簇不负责 write_project_marker / repair_project_marker / setup migration。
12. Project Marker 写入仍需用户确认，不可静默执行。
13. 本簇所有工具 modified=false。
14. 本簇所有工具不返回 validation / write_ref / transaction_id / review / safety。
15. 所有 data.schema 使用短命名。
```

---

## 1. 工具簇边界

第一版覆盖：

```text
read_project_context
check_project_marker
check_setup_state
```

第一版不覆盖：

```text
write_project_marker
repair_project_marker
remove_project_marker
run_blueprinthelper_setup
migrate_settings
repair_settings
write_global_guidance
```

原因：

```text
1. /blueprinthelper-setup 是配置写入口，不属于 UE runtime 普通 MCP 工具。
2. Project Marker 写入项目 CLAUDE.md 需要用户确认，不能静默执行。
3. settings.json 修复 / 迁移 / 重建属于 setup 命令职责，不属于 Agent 普通运行时工具。
```

---

## 2. 通用返回原则

本簇全部为只读检查工具：

```text
modified=false
```

不返回：

```text
validation
write_ref
transaction_id
journal_recorded
review
safety
rollback_data
```

隐私 / 上下文控制：

```text
不返回本地绝对路径
不返回完整 settings.json
不返回 CLAUDE.md 全文
不返回 Project Marker 文本
不返回 Skill / AgentGuide 全文
```

所有 `data.schema` 使用短命名。

---

# 3. read_project_context

## 3.1 工具定位

`read_project_context` 负责读取当前 UE 项目的最小上下文状态。

它用于判断：

```text
当前是否识别为 UE 项目
是否存在 BlueprintHelper Project Marker
runtime 是否可继续执行 BlueprintHelper 工作流
```

它不负责：

```text
写入 Project Marker
读取完整 CLAUDE.md
读取完整 settings.json
读取完整 AgentGuide
修复 setup
迁移配置
```

---

## 3.2 operation

```json
"operation": "read_project_context"
```

---

## 3.3 target 字段

```json
"target": {
  "read_scope": "project_context"
}
```

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `ReadScope` | `EBlueprintHelperProjectReadScope` | `target.read_scope` | `string enum` | 是 | 固定为 `project_context`。 |

---

## 3.4 正常返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_project_context",
  "trace_id": "trace_20260503_4301",
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

---

## 3.5 项目未识别

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_project_context",
  "trace_id": "trace_20260503_4302",
  "status": "completed",
  "modified": false,

  "target": {
    "read_scope": "project_context"
  },

  "data": {
    "schema": "ReadProjectContext.v1",
    "project_context": {
      "status": "blocked",
      "project_detected": false,
      "workflow_enabled": false,
      "reason": "ue_project_not_detected"
    }
  }
}
```

说明：

```text
ok=true / status=completed 表示工具成功完成检查。
project_context.status=blocked 表示检查结果发现工作流不可继续。
```

---

## 3.6 Project Marker 缺失

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_project_context",
  "trace_id": "trace_20260503_4303",
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

---

## 3.7 project_context 字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Status` | `EBlueprintHelperProjectContextStatus` | `data.project_context.status` | `string enum` | 是 | `ok` / `degraded` / `blocked`。 |
| `bProjectDetected` | `bool` | `data.project_context.project_detected` | `boolean` | 是 | 是否识别到 UE 项目。 |
| `ProjectMarkerStatus` | `EBlueprintHelperProjectMarkerStatus` | `data.project_context.project_marker` | `string enum` | 项目已识别时 | `present` / `missing` / `invalid`。 |
| `bWorkflowEnabled` | `bool` | `data.project_context.workflow_enabled` | `boolean` | 是 | BlueprintHelper 工作流是否可用。 |
| `Reason` | `FString` 或 enum | `data.project_context.reason` | `string` | 异常态 | 稳定 reason。 |

不返回：

```text
project_root
uproject_path
absolute CLAUDE.md path
settings_path
marker text
CLAUDE.md content
```

---

# 4. check_project_marker

## 4.1 工具定位

`check_project_marker` 负责只读检查项目级 CLAUDE.md 中的 BlueprintHelper Project Marker 状态。

它不负责：

```text
写入 marker
修复 marker
删除 marker
返回 marker 文本
```

---

## 4.2 operation

```json
"operation": "check_project_marker"
```

---

## 4.3 target 字段

```json
"target": {
  "read_scope": "project_marker"
}
```

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `ReadScope` | `EBlueprintHelperProjectReadScope` | `target.read_scope` | `string enum` | 是 | 固定为 `project_marker`。 |

---

## 4.4 Marker 存在

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "check_project_marker",
  "trace_id": "trace_20260503_4401",
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

---

## 4.5 Marker 缺失

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "check_project_marker",
  "trace_id": "trace_20260503_4402",
  "status": "completed",
  "modified": false,

  "target": {
    "read_scope": "project_marker"
  },

  "data": {
    "schema": "CheckProjectMarker.v1",
    "project_marker": {
      "status": "missing",
      "workflow_enabled": false,
      "reason": "project_marker_missing"
    }
  }
}
```

---

## 4.6 Marker 损坏 / 多重 marker

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "check_project_marker",
  "trace_id": "trace_20260503_4403",
  "status": "completed",
  "modified": false,

  "target": {
    "read_scope": "project_marker"
  },

  "data": {
    "schema": "CheckProjectMarker.v1",
    "project_marker": {
      "status": "invalid",
      "workflow_enabled": false,
      "reason": "project_marker_invalid"
    }
  }
}
```

---

## 4.7 project_marker 字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Status` | `EBlueprintHelperProjectMarkerStatus` | `data.project_marker.status` | `string enum` | 是 | `present` / `missing` / `invalid`。 |
| `bWorkflowEnabled` | `bool` | `data.project_marker.workflow_enabled` | `boolean` | 是 | Project Marker 是否启用工作流。 |
| `Reason` | `FString` 或 enum | `data.project_marker.reason` | `string` | 异常态 | 稳定 reason。 |

不返回：

```text
marker_start_line
marker_end_line
CLAUDE.md content
marker body
local file path
```

这些属于 debug / diagnostics 扩展。

---

# 5. check_setup_state

## 5.1 工具定位

`check_setup_state` 负责只读检查 BlueprintHelper setup 是否完成，以及 runtime 所需配置是否可用。

它不负责：

```text
运行 setup
修复 settings.json
迁移 settings.json
写全局 CLAUDE.md managed block
写 Project Marker
```

---

## 5.2 operation

```json
"operation": "check_setup_state"
```

---

## 5.3 target 字段

```json
"target": {
  "read_scope": "setup_state"
}
```

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `ReadScope` | `EBlueprintHelperProjectReadScope` | `target.read_scope` | `string enum` | 是 | 固定为 `setup_state`。 |

---

## 5.4 setup 正常

正常态极简：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "check_setup_state",
  "trace_id": "trace_20260503_4501",
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

---

## 5.5 setup 未完成

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "check_setup_state",
  "trace_id": "trace_20260503_4502",
  "status": "completed",
  "modified": false,

  "target": {
    "read_scope": "setup_state"
  },

  "data": {
    "schema": "CheckSetupState.v1",
    "setup_state": {
      "status": "blocked",
      "reason": "setup_not_completed"
    }
  }
}
```

---

## 5.6 settings 不可用

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "check_setup_state",
  "trace_id": "trace_20260503_4503",
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

`check_setup_state` 不区分：

```text
settings missing
settings invalid
settings damaged
settings old_version
settings missing_fields
```

这些细节属于 `/blueprinthelper-setup` 或 `/blueprinthelper-diagnostics` 静态诊断职责。

---

## 5.7 setup_state 字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Status` | `EBlueprintHelperSetupStateStatus` | `data.setup_state.status` | `string enum` | 是 | `ok` / `blocked`。 |
| `Reason` | `FString` 或 enum | `data.setup_state.reason` | `string` | blocked 时 | 稳定 reason。 |

---

# 6. 与 runtime_profile / diagnostics 的关系

职责划分：

```text
runtime_profile：
- 面向当前任务执行链路。
- 判断 UE/MCP/Bridge/write_permission/能力是否可用。
- 正常态只返回 runtime_profile.status=ok。

read_project_context：
- 面向当前项目工作流。
- 判断项目是否识别、Project Marker 是否存在。

check_setup_state：
- 面向 setup 状态。
- 只返回 setup 是否 ok 或 blocked reason。

diagnostics：
- 面向安装 / 配置 / 运行链路诊断。
- 输出 Markdown 报告。
```

避免重复：

```text
read_project_context 不展开 runtime_profile。
check_setup_state 不展开 diagnostics。
runtime_profile 正常态不展开 Project Marker。
diagnostics 才负责更完整的问题报告。
```

---

# 7. write_project_marker 边界

第一版不做 MCP 写工具：

```text
write_project_marker
repair_project_marker
remove_project_marker
```

原因：

```text
1. Project Marker 写入项目 CLAUDE.md 是普通文件写入，不是 UE 资产写入。
2. 需要用户确认。
3. Agent 不能静默写入项目规则文件。
4. UE 插件侧也可提供菜单/命令手动写入/修复/移除。
```

如果后续设计写入工具，必须满足：

```text
dry_run
用户确认
只改 managed block
不覆盖用户内容
不返回 transaction_id
不进入 UE Transaction Journal
```

---

# 8. error 字段映射

本簇多数检查异常以 `data.*.status=blocked/degraded` 返回，工具自身失败才返回 `error`。

示例工具自身失败：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "read_project_context",
  "trace_id": "trace_20260503_4304",
  "status": "failed",
  "modified": false,

  "error": {
    "code": "project_context_check_failed",
    "stage": "read_project_context",
    "message": "Project context could not be checked.",
    "retryable": true
  }
}
```

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperProjectContextErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperProjectContextStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |

---

# 9. UE/MCP 建议结构体

```cpp
struct FBlueprintHelperReadProjectContextResultData
{
    FString Schema; // ReadProjectContext.v1
    FBlueprintHelperProjectContextStatus ProjectContext;
};

struct FBlueprintHelperProjectContextStatus
{
    FString Status; // ok | degraded | blocked
    bool bProjectDetected = false;
    FString ProjectMarker; // present | missing | invalid
    bool bWorkflowEnabled = false;
    FString Reason;
};

struct FBlueprintHelperCheckProjectMarkerResultData
{
    FString Schema; // CheckProjectMarker.v1
    FBlueprintHelperProjectMarkerCheck ProjectMarker;
};

struct FBlueprintHelperProjectMarkerCheck
{
    FString Status; // present | missing | invalid
    bool bWorkflowEnabled = false;
    FString Reason;
};

struct FBlueprintHelperCheckSetupStateResultData
{
    FString Schema; // CheckSetupState.v1
    FBlueprintHelperSetupState SetupState;
};

struct FBlueprintHelperSetupState
{
    FString Status; // ok | blocked
    FString Reason;
};
```

明确不包含：

```cpp
FString ProjectRoot
FString UProjectPath
FString SettingsPath
FString ClaudeMdContent
FString MarkerBody
FBlueprintHelperValidationResult Validation
FBlueprintHelperWriteRef WriteRef
FString TransactionId
```

---

# 10. 验收标准

```text
1. read_project_context 是只读工具。
2. read_project_context 返回 project_context.status / project_detected / project_marker / workflow_enabled。
3. read_project_context 不返回本地绝对路径或 CLAUDE.md 全文。
4. check_project_marker 是只读工具。
5. check_project_marker 返回 marker 状态，不返回 marker 文本。
6. check_setup_state 正常态只返回 setup_state.status=ok。
7. check_setup_state 异常态只返回 status / reason。
8. check_setup_state 不展开 settings 损坏细节。
9. 本簇不提供 marker 写入 / 修复工具。
10. Project Marker 写入必须用户确认，不能静默执行。
11. 本簇所有工具 modified=false。
12. 本簇不返回 validation / write_ref / transaction_id / review / safety。
13. data.schema 使用短命名。
