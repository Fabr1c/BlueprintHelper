# BlueprintHelper Diagnostics / Static & Runtime Diagnostics UE 字段映射计划

日期：2026-05-03  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：Diagnostics 字段确认稿  
本文边界：确认 `/blueprinthelper-diagnostics` 与 `/blueprinthelper-diagnostics --runtime` 对应的 Agent-facing 字段协议、UE/MCP 返回结构、Markdown 报告格式、工具成功/诊断阻断区分、隐私边界和只读边界。Agent 使用规则见独立文档。

---

## 0. 本次同步结论

Diagnostics 工具簇采用以下字段口径：

```text
1. 诊断命令入口保留 /blueprinthelper-diagnostics 与 /blueprinthelper-diagnostics --runtime。
2. MCP Agent-facing 字段协议可统一为 operation=run_blueprinthelper_diagnostics，并用 target.diagnostics_mode=static | runtime 区分。
3. data.schema 使用短命名 Diagnostics.v1。
4. 诊断报告只返回 data.markdown。
5. 不返回 blocking / warning / info JSON 数组。
6. Markdown 必须包含 ## Blocking 和 ## Warning。
7. ## Info 可选。
8. Blocking / Warning 为空时写 None。
9. 诊断报告内可包含 Blocking 项，但工具仍返回 ok=true / status=completed。
10. 只有诊断工具自身失败时才返回 ok=false / status=failed / error。
11. Diagnostics 不返回 validation / write_ref / transaction_id / review / safety。
12. Diagnostics modified=false。
13. Diagnostics 不输出 Suggested action / action code。
14. Diagnostics 不输出本地绝对路径 / settings.json / CLAUDE.md 全文。
15. runtime diagnostics 可报告 write_permission.disabled / risk_command.disabled / bridge.disconnected。
16. risk_command_missing 只阻断 close_editor 等风险命令，不阻断普通蓝图读写。
17. diagnostics 是 Agent-facing 诊断报告；只有用户明确要求或 stop_and_report 需要解释阻断来源时，Agent 才向用户转述关键 code，不默认展开完整报告。
```

---

## 1. 命令入口与 MCP operation

用户 / Agent 可见命令入口保留：

```text
/blueprinthelper-diagnostics
/blueprinthelper-diagnostics --runtime
```

MCP Agent-facing 字段协议可统一为：

```json
"operation": "run_blueprinthelper_diagnostics"
```

并通过：

```json
"target": {
  "diagnostics_mode": "static"
}
```

或：

```json
"target": {
  "diagnostics_mode": "runtime"
}
```

区分诊断模式。

说明：

```text
命令入口可以是两个。
ToolResult operation 可以是一个。
字段协议用 diagnostics_mode 区分。
```

---

## 2. 工具定位

Diagnostics 只负责：

```text
安装状态诊断
配置状态诊断
Bridge / MCP / runtime 链路诊断
write_permission / risk_command 状态诊断
Project Marker 只读检查
```

它不负责：

```text
修复配置
迁移 settings.json
写 Project Marker
写全局 CLAUDE.md
读取蓝图 LogicMD / LogicJson
生成 AgentPlan
判断具体蓝图任务是否能完成
执行写工具
```

---

## 3. 通用返回原则

Diagnostics 是只读工具：

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

报告只放在：

```text
data.markdown
```

不返回：

```text
data.blocking[]
data.warning[]
data.info[]
```

所有 `data.schema` 使用短命名：

```text
Diagnostics.v1
```

---

# 4. data.schema

固定：

```json
"schema": "Diagnostics.v1"
```

不使用：

```text
BlueprintHelper.Diagnostics.v1
BlueprintHelper.MCP.Diagnostics.v1
BlueprintHelper.Tools.Diagnostics.v1
```

---

# 5. static diagnostics

## 5.1 target

```json
"target": {
  "diagnostics_mode": "static"
}
```

字段映射：

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `DiagnosticsMode` | `EBlueprintHelperDiagnosticsMode` | `target.diagnostics_mode` | `string enum` | 是 | `static` 或 `runtime`。 |

---

## 5.2 static 正常结果

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "run_blueprinthelper_diagnostics",
  "trace_id": "trace_20260503_5001",
  "status": "completed",
  "modified": false,

  "target": {
    "diagnostics_mode": "static"
  },

  "data": {
    "schema": "Diagnostics.v1",
    "markdown": "## Blocking\nNone\n\n## Warning\nNone\n\n## Info\n- `version.match`\n- `settings.valid`\n- `global_guidance.present`\n- `skill_entry.valid`"
  }
}
```

---

## 5.3 static：Project Marker 缺失

如果当前目录识别为 UE 项目，且 Project Marker 缺失：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "run_blueprinthelper_diagnostics",
  "trace_id": "trace_20260503_5002",
  "status": "completed",
  "modified": false,

  "target": {
    "diagnostics_mode": "static"
  },

  "data": {
    "schema": "Diagnostics.v1",
    "markdown": "## Blocking\nNone\n\n## Warning\n- `project_marker.missing`\n\n## Info\n- `version.match`\n- `settings.valid`\n- `global_guidance.present`\n- `skill_entry.valid`"
  }
}
```

说明：

```text
ok=true / status=completed 表示诊断工具执行成功。
project_marker.missing 是诊断报告内容，不是工具失败。
```

---

## 5.4 static：配置损坏

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "run_blueprinthelper_diagnostics",
  "trace_id": "trace_20260503_5003",
  "status": "completed",
  "modified": false,

  "target": {
    "diagnostics_mode": "static"
  },

  "data": {
    "schema": "Diagnostics.v1",
    "markdown": "## Blocking\n- `settings.invalid`\n\n## Warning\nNone\n\n## Info\n- `version.match`\n- `global_guidance.present`"
  }
}
```

不展开：

```text
settings 字段级错误
settings.json 内容
本地 settings 路径
自动修复建议
```

---

# 6. runtime diagnostics

## 6.1 target

```json
"target": {
  "diagnostics_mode": "runtime"
}
```

---

## 6.2 runtime 正常

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "run_blueprinthelper_diagnostics",
  "trace_id": "trace_20260503_5101",
  "status": "completed",
  "modified": false,

  "target": {
    "diagnostics_mode": "runtime"
  },

  "data": {
    "schema": "Diagnostics.v1",
    "markdown": "## Blocking\nNone\n\n## Warning\nNone\n\n## Info\n- `ue_editor.running`\n- `mcp_server.available`\n- `bridge.connected`\n- `runtime_profile.available`\n- `config_status.valid`\n- `write_permission.enabled`\n- `risk_command.enabled`"
  }
}
```

---

## 6.3 runtime：Token 缺失

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "run_blueprinthelper_diagnostics",
  "trace_id": "trace_20260503_5102",
  "status": "completed",
  "modified": false,

  "target": {
    "diagnostics_mode": "runtime"
  },

  "data": {
    "schema": "Diagnostics.v1",
    "markdown": "## Blocking\nNone\n\n## Warning\n- `write_permission.disabled`\n  - reason: `token_missing`\n\n## Info\n- `ue_editor.running`\n- `mcp_server.available`\n- `bridge.connected`\n- `runtime_profile.available`\n- `config_status.valid`"
  }
}
```

Token 缺失不是工具系统异常。它表示：

```text
写操作不可用。
只读诊断仍成功。
```

---

## 6.4 runtime：Bridge 断开

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "run_blueprinthelper_diagnostics",
  "trace_id": "trace_20260503_5103",
  "status": "completed",
  "modified": false,

  "target": {
    "diagnostics_mode": "runtime"
  },

  "data": {
    "schema": "Diagnostics.v1",
    "markdown": "## Blocking\n- `bridge.disconnected`\n\n## Warning\nNone\n\n## Info\n- `ue_editor.running`\n- `mcp_server.available`"
  }
}
```

仍然是：

```text
ok=true
status=completed
```

因为诊断工具执行成功，报告发现阻断项。

---

## 6.5 runtime：risk_command 缺失

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "run_blueprinthelper_diagnostics",
  "trace_id": "trace_20260503_5104",
  "status": "completed",
  "modified": false,

  "target": {
    "diagnostics_mode": "runtime"
  },

  "data": {
    "schema": "Diagnostics.v1",
    "markdown": "## Blocking\nNone\n\n## Warning\n- `risk_command.disabled`\n  - reason: `risk_command_missing`\n  - blocked_commands: `close_editor`\n\n## Info\n- `ue_editor.running`\n- `mcp_server.available`\n- `bridge.connected`\n- `runtime_profile.available`\n- `config_status.valid`\n- `write_permission.enabled`"
  }
}
```

解释：

```text
risk_command_missing 只阻断 close_editor 等风险命令。
普通蓝图读写不因此阻断。
```

---

# 7. 工具自身失败

只有诊断工具本身无法执行时才返回 `ok=false`。

例如 MCP 无法执行诊断命令：

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "run_blueprinthelper_diagnostics",
  "trace_id": "trace_20260503_5201",
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

# 8. Markdown 格式规则

Diagnostics Markdown 固定分组顺序：

```md
## Blocking
...

## Warning
...

## Info
...
```

规则：

```text
1. Blocking 必须存在。
2. Warning 必须存在。
3. Info 可选。
4. Blocking / Warning 为空时写 None。
5. code 使用固定命名规范 <domain>.<state>。
6. 不输出 Suggested action。
7. 不输出 action code。
8. 不输出本地绝对路径。
```

示例 code：

```text
bridge.connected
bridge.disconnected
runtime_profile.available
write_permission.enabled
write_permission.disabled
risk_command.enabled
risk_command.disabled
config_status.valid
config_status.unavailable
version.match
version.mismatch
settings.valid
settings.invalid
project_marker.present
project_marker.missing
```

---

# 9. 与 runtime_profile 的关系

```text
runtime_profile：
- 当前任务运行时事实。
- 正常态只返回 status=ok。
- 异常态才返回 write_permission / bridge / config_status / unavailable。

diagnostics：
- Agent-facing 诊断报告。
- 总是 Markdown。
- 可以列 Blocking / Warning / Info。
- 不替代 runtime_profile。
```

Agent 使用建议：

```text
普通写任务前读取 runtime_profile。
需要解释环境问题或 stop_and_report 需要阻断来源时调用 diagnostics。
```

---

# 10. 与 setup / project context 的关系

```text
check_setup_state：
- 只判断 setup_state.status=ok 或 blocked reason。
- 不展开 settings 细节。

check_project_marker：
- 只判断 marker 状态。

diagnostics：
- 面向安装 / 配置 / 运行链路诊断。
- 通过 Markdown 报告更完整状态 code。
```

---

# 11. 用户可见边界

Diagnostics 是 Agent-facing 诊断报告。

规则：

```text
1. 不默认向用户展开完整 diagnostics Markdown。
2. 用户明确要求诊断结果时，Agent 可以转述关键 code。
3. stop_and_report 需要解释阻断来源时，Agent 可以转述关键 code。
4. Agent 不应把 diagnostics 当作用户操作指南。
5. Diagnostics 不包含 Suggested action。
```

---

# 12. error 字段映射

| UE/MCP 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Code` | `EBlueprintHelperDiagnosticsErrorCode` | `error.code` | `string enum` | 是 | 稳定错误码。 |
| `Stage` | `EBlueprintHelperDiagnosticsStage` | `error.stage` | `string enum` | 是 | 失败阶段。 |
| `Message` | `FString` | `error.message` | `string` | 是 | 简短人类可读信息。 |
| `bRetryable` | `bool` | `error.retryable` | `boolean` | 是 | Agent 是否可直接重试。 |

---

# 13. UE/MCP 建议结构体

```cpp
struct FBlueprintHelperDiagnosticsResultData
{
    FString Schema; // Diagnostics.v1
    FString Markdown;
};

struct FBlueprintHelperDiagnosticsTarget
{
    FString DiagnosticsMode; // static | runtime
};
```

明确不包含：

```cpp
TArray<FBlueprintHelperDiagnosticItem> Blocking;
TArray<FBlueprintHelperDiagnosticItem> Warning;
TArray<FBlueprintHelperDiagnosticItem> Info;
FBlueprintHelperValidationResult Validation;
FBlueprintHelperWriteRef WriteRef;
FString TransactionId;
FString SuggestedAction;
FString ActionCode;
FString LocalAbsolutePath;
FString SettingsJson;
FString ClaudeMdContent;
```

---

# 14. 验收标准

```text
1. /blueprinthelper-diagnostics 与 /blueprinthelper-diagnostics --runtime 命令入口保留。
2. MCP operation 可统一为 run_blueprinthelper_diagnostics。
3. target.diagnostics_mode 区分 static / runtime。
4. data.schema 固定为 Diagnostics.v1。
5. 诊断报告只返回 data.markdown。
6. 不返回 blocking / warning / info JSON 数组。
7. Markdown 包含 ## Blocking 和 ## Warning。
8. ## Info 可选。
9. Blocking / Warning 为空时写 None。
10. Markdown 有 Blocking 时，ToolResult 仍 ok=true / status=completed。
11. 工具自身失败时才 ok=false / status=failed。
12. 不返回 validation / write_ref / transaction_id / review / safety。
13. 不输出 Suggested action / action code。
14. 不输出本地路径 / settings.json / CLAUDE.md 全文。
15. risk_command_missing 不阻断普通蓝图读写。
