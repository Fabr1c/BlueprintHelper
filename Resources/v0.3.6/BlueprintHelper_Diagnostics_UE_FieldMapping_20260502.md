# BlueprintHelper 第 2 簇：Diagnostics UE 字段映射设计

日期：2026-05-02  
适用范围：BlueprintHelper v0.4 / v0.5 前置协议收敛  
状态：Diagnostics 字段确认稿  
本文边界：仅确认 Diagnostics 工具的 UE 侧结构体、字段映射和 MCP 返回体。Agent 使用规则另见独立 Agent 侧规则文档。

---

## 1. 设计定位

Diagnostics 是只读诊断工具，用于定位安装、配置、Bridge、runtime 链路问题。

Diagnostics 不负责：

```text
1. 判断某个具体蓝图任务能否完成。
2. 读取某个蓝图 LogicMD / LogicJson。
3. 生成 AgentPlan。
4. 返回完整 MCP tool schema。
5. 返回完整 settings.json。
6. 执行修复、迁移或写入。
```

Diagnostics 与 Runtime Profile 的分工：

```text
runtime_profile = 当前运行时事实摘要，用于任务前判断能不能做。
diagnostics = 只读诊断报告，用于定位环境、配置、Bridge、runtime 链路问题。
```

---

## 2. 已确认决策

| 项 | 决策 |
|---|---|
| Diagnostics 返回外壳 | 使用第 0 簇 `ToolResultBase`。 |
| Diagnostics 内容 | 放入 `data.markdown`。 |
| blocking / warning / info | 不额外返回 JSON 数组，只返回 Markdown。 |
| 是否生成 transaction | 不生成。 |
| 是否进入 Review | 不进入。 |
| 是否返回 safety | 不返回。 |
| 是否返回 validation | 不返回。 |
| 是否返回 target | 默认不返回。未来若出现资产级专用诊断可另行设计。 |
| 诊断成功但发现 Blocking | `ok=true`，`status=completed`，Blocking 写入 Markdown。 |
| 诊断工具自身执行失败 | `ok=false`，`status=failed`，返回 `error`。 |

---

## 3. 工具入口

建议保留两个诊断入口：

```text
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
```

或命令形式：

```text
/blueprinthelper-diagnostics
/blueprinthelper-diagnostics --runtime
```

---

## 4. Static Diagnostics 边界

Static Diagnostics 不要求 Unreal Editor 正在运行。

检查：

```text
1. 三端安装状态。
2. settings.json 是否存在 / 可读 / 当前版本。
3. 全局 CLAUDE.md managed block。
4. Skill 入口是否有效。
5. 当前目录是否像 UE 项目。
6. 项目 CLAUDE.md Project Marker 是否存在。
```

不检查：

```text
1. UE Bridge。
2. write token。
3. risk command。
4. 当前 runtime_profile。
5. 具体蓝图工具能力。
```

---

## 5. Runtime Diagnostics 边界

Runtime Diagnostics 用于 UE / MCP / Bridge / runtime profile 链路检测。

检查：

```text
1. UE Editor 是否运行。
2. MCP Server 是否可用。
3. Bridge 是否 connected。
4. runtime_profile 是否 available。
5. config_status。
6. write_permission。
7. risk_command。
8. project_root 是否可用于 Project Marker 检查。
```

不检查：

```text
1. 具体蓝图任务能否完成。
2. 某个资产是否存在。
3. 某个图表能否修改。
4. LogicMD / LogicJson 内容。
5. AgentPlan。
```

---

## 6. Agent 可见返回体示例

### 6.1 Static Diagnostics 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "diagnostics",
  "trace_id": "trace_20260502_0201",
  "status": "completed",
  "modified": false,
  "data": {
    "schema": "BlueprintHelper.Diagnostics.v1",
    "mode": "static",
    "format": "markdown",
    "markdown": "## Blocking\nNone\n\n## Warning\n- project_marker.missing\n\n## Info\n- version.match\n- settings.valid\n- global_guidance.present\n- skill_entry.valid"
  }
}
```

### 6.2 Runtime Diagnostics 成功返回

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "diagnostics_runtime",
  "trace_id": "trace_20260502_0202",
  "status": "completed",
  "modified": false,
  "data": {
    "schema": "BlueprintHelper.Diagnostics.v1",
    "mode": "runtime",
    "format": "markdown",
    "markdown": "## Blocking\nNone\n\n## Warning\n- risk_command.disabled\n  - reason: risk_command_missing\n  - blocked_commands: close_editor, exec_console_command\n\n## Info\n- ue_editor.running\n- mcp_server.available\n- bridge.connected\n- runtime_profile.available\n- config_status.valid\n- write_permission.enabled"
  }
}
```

### 6.3 诊断工具自身失败

```json
{
  "ok": false,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "diagnostics_runtime",
  "trace_id": "trace_20260502_0203",
  "status": "failed",
  "modified": false,
  "error": {
    "code": "diagnostics_failed",
    "stage": "execute",
    "message": "Runtime diagnostics failed.",
    "retryable": true,
    "rollback_result": "not_needed"
  }
}
```

注意：

```text
诊断成功但发现 Blocking，不属于 diagnostics 工具自身失败。
此时仍返回 ok=true，status=completed。
```

---

# 7. UE 侧建议结构体

建议新增或整理以下结构体：

```cpp
FBlueprintHelperDiagnosticsData
FBlueprintHelperDiagnosticsMarkdownReport
FBlueprintHelperDiagnosticsCodeLine
```

Diagnostics 顶层仍使用第 0 簇：

```cpp
FBlueprintHelperToolResultBase
```

其中 `Data` 指向：

```cpp
FBlueprintHelperDiagnosticsData
```

序列化后的返回路径为：

```text
data.*
```

---

# 8. 字段映射表

## 8.1 FBlueprintHelperToolResultBase 到 MCP 返回体

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `bOk` | `bool` | `ok` | `boolean` | 是 | 诊断工具是否执行成功。 |
| `Schema` | `FString` | `schema` | `string` | 是 | 固定为 `BlueprintHelper.McpToolResult.v1`。 |
| `Operation` | `FString` 或 enum | `operation` | `string` | 是 | `diagnostics` 或 `diagnostics_runtime`。 |
| `TraceId` | `FString` | `trace_id` | `string` | 是 | MCP / UE / 日志跨层追踪 ID。 |
| `Status` | `EBlueprintHelperToolStatus` | `status` | `string enum` | 是 | 成功为 `completed`。 |
| `bModified` | `bool` | `modified` | `boolean` | 是 | Diagnostics 是只读工具，固定为 `false`。 |
| `Data` | `FBlueprintHelperDiagnosticsData` | `data` | `object` | 成功时 | Diagnostics 实际数据。 |
| `Target` | 不使用 | 不返回 | - | 否 | Diagnostics 默认不针对具体资产。 |
| `Safety` | 不使用 | 不返回 | - | 否 | Diagnostics 不携带 safety。 |
| `Transaction` | 不使用 | 不返回 | - | 否 | 只读工具，不生成 transaction。 |
| `Review` | 不使用 | 不返回 | - | 否 | 不进入 Review。 |
| `Validation` | 不使用 | 不返回 | - | 否 | 不做 compile/save validation。 |
| `Error` | `FBlueprintHelperToolError` | `error` | `object` | 失败时 | 仅 diagnostics 工具自身失败时返回。 |

---

## 8.2 FBlueprintHelperDiagnosticsData

| UE 字段 | UE 类型建议 | MCP JSON 路径 | JSON 类型 | 必填 | 说明 |
|---|---|---|---|---:|---|
| `Schema` | `FString` | `data.schema` | `string` | 是 | 固定为 `BlueprintHelper.Diagnostics.v1`。 |
| `Mode` | `EBlueprintHelperDiagnosticsMode` | `data.mode` | `string enum` | 是 | `static` 或 `runtime`。 |
| `Format` | `EBlueprintHelperDiagnosticsFormat` | `data.format` | `string enum` | 是 | 当前固定为 `markdown`。 |
| `Markdown` | `FString` | `data.markdown` | `string` | 是 | Markdown 诊断报告。 |

不返回：

```text
data.blocking[]
data.warning[]
data.info[]
```

说明：

```text
Diagnostics 只返回 Markdown，不额外返回 blocking / warning / info JSON 数组。
```

---

## 8.3 DiagnosticsMode 枚举

### UE 枚举建议

```cpp
enum class EBlueprintHelperDiagnosticsMode
{
    Static,
    Runtime
};
```

### MCP 返回值

| UE 枚举 | JSON 值 |
|---|---|
| `Static` | `static` |
| `Runtime` | `runtime` |

---

## 8.4 DiagnosticsFormat 枚举

### UE 枚举建议

```cpp
enum class EBlueprintHelperDiagnosticsFormat
{
    Markdown
};
```

### MCP 返回值

| UE 枚举 | JSON 值 |
|---|---|
| `Markdown` | `markdown` |

---

# 9. Markdown 固定格式

Diagnostics Markdown 固定为：

```md
## Blocking
None

## Warning
None

## Info
- code
```

规则：

```text
1. Blocking 必须出现。
2. Warning 必须出现。
3. Info 可选。
4. 如果 Blocking 为空，写 None。
5. 如果 Warning 为空，写 None。
6. Info 没有必要时可省略。
```

---

# 10. Markdown 内容约束

Diagnostics Markdown 不输出：

```text
1. Suggested action。
2. 本地绝对路径。
3. 完整 settings.json。
4. 完整 MCP tool schema。
5. 完整 setup profile。
6. 自然语言长解释。
7. 具体任务能否完成的判断。
```

Diagnostics Markdown 只输出稳定 code 和必要枚举补充信息。

---

# 11. code 命名规则

使用稳定 code：

```text
<domain>.<state>
```

示例：

```text
version.match
version.mismatch
settings.valid
settings.unavailable
global_guidance.present
global_guidance.missing
skill_entry.valid
skill_entry.invalid
project_marker.present
project_marker.missing

ue_editor.running
ue_editor.not_running
mcp_server.available
mcp_server.unavailable
bridge.connected
bridge.disconnected
runtime_profile.available
runtime_profile.unavailable
config_status.valid
config_status.unavailable
write_permission.enabled
write_permission.disabled
risk_command.enabled
risk_command.disabled
```

---

# 12. Static Diagnostics 推荐最小输出

## Blocking

可能 code：

```text
version.mismatch
settings.unavailable
global_guidance.missing
skill_entry.invalid
```

## Warning

可能 code：

```text
project_marker.missing
settings.old_version
settings.missing_fields
```

## Info

推荐最小 code：

```text
version.match
settings.valid
global_guidance.present
skill_entry.valid
project_marker.present
```

Project Marker 规则：

```text
只有当前目录或父级可识别为 UE 项目时，才检查 Project Marker。
```

---

# 13. Runtime Diagnostics 推荐最小输出

## Blocking

可能 code：

```text
ue_editor.not_running
mcp_server.unavailable
bridge.disconnected
runtime_profile.unavailable
config_status.unavailable
```

## Warning

可能 code：

```text
write_permission.disabled
risk_command.disabled
project_marker.missing
```

## Info

推荐最小 code：

```text
ue_editor.running
mcp_server.available
bridge.connected
runtime_profile.available
config_status.valid
write_permission.enabled
risk_command.enabled
project_marker.present
```

说明：

```text
risk_command.disabled 是 Warning，不阻断普通蓝图读写。
write_permission.disabled 是否阻断，取决于当前任务是否需要写操作。
```

---

# 14. UE 侧服务建议

建议新增：

```cpp
class FBlueprintHelperDiagnosticsService
```

职责：

```text
1. 执行 static diagnostics。
2. 执行 runtime diagnostics。
3. 构建 Markdown 报告。
4. 保证 Blocking / Warning 固定输出。
5. 保证 diagnostics 不写任何文件。
6. 保证 diagnostics 不修复、不迁移、不进入 Profile 表单。
```

不负责：

```text
1. 自动修复 settings。
2. 自动写 CLAUDE.md。
3. 自动写 Project Marker。
4. 自动刷新 Token。
5. 自动启动 UE Editor。
6. 判断具体蓝图任务能否完成。
```

---

# 15. 验收标准

```text
1. diagnostics 使用 ToolResultBase 顶层结构。
2. diagnostics 成功返回 data.schema = BlueprintHelper.Diagnostics.v1。
3. diagnostics 成功返回 data.format = markdown。
4. diagnostics 不返回 blocking / warning / info JSON 数组。
5. diagnostics 不返回 target / safety / transaction / review / validation。
6. diagnostics 不生成 transaction_id。
7. diagnostics 不写任何文件。
8. diagnostics 不执行修复。
9. diagnostics 成功但发现 Blocking 时仍 ok=true。
10. diagnostics 工具自身失败时才 ok=false，并返回 error。
11. Markdown 中 Blocking 和 Warning 必须出现。
12. Markdown 不输出 Suggested action、本地绝对路径、完整 settings、完整 tool schema。
```

---

# 16. 待后续讨论项

```text
1. Diagnostics code 完整枚举表。
2. Diagnostics 是否需要 debug / verbose 模式。
3. Diagnostics 是否需要单独支持 JSON 输出给未来 UI。
4. Project Marker 检查的项目识别规则细节。
5. runtime diagnostics 与 runtime_profile 的具体调用顺序。
```
