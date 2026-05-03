# BlueprintHelper Agent 侧规则：Runtime Profile 使用规范

日期：2026-05-02  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：Runtime Profile Agent 侧规则确认稿  
本文边界：只规定 Agent 如何读取和使用 `blueprinthelper_get_runtime_profile`。UE 结构体和字段映射见独立 UE 字段映射文档。

---

## 1. Runtime Profile 的职责

`blueprinthelper_get_runtime_profile` 是 Agent 在写入阶段前读取的运行时事实接口。

它告诉 Agent：

```text
1. BlueprintHelper 当前运行状态。
2. UE Bridge 是否连通。
3. 配置是否可用。
4. 写权限是否可用。
5. risk command 是否可用。
6. 当前 Safety Profile。
7. 当前不可用、禁用、降级或阻断的能力簇。
```

它不告诉 Agent：

```text
1. 完整工具使用说明。
2. 完整 MCP tool schema。
3. 命名偏好全文。
4. 蓝图 / C++ 开发边界全文。
5. 完整 setup profile。
6. 完整 diagnostics。
7. Transaction / Review 历史。
```

---

## 2. Agent 规则来源链路

命名偏好、蓝图 / C++ 边界和具体工具使用规则不由 runtime_profile 注入。

Agent 应按以下链路读取规则：

```text
项目 CLAUDE.md managed block
→ BlueprintHelper AgentGuide / Skill 入口
→ 工具簇索引文件
→ 具体工具簇规则文件
→ 当前任务所需 MCP tool schema
→ runtime_profile 运行时事实修正
```

因此：

```text
runtime_profile 只提供运行时状态。
AgentGuide / tools 索引负责解释工具簇、能力边界和调用规则。
MCP tool schema 负责具体参数。
```

---

## 3. 何时读取 runtime_profile

Agent 必须在每个写入任务的写入阶段开始前读取一次 runtime_profile。

写入任务包括：

```text
1. 创建蓝图资产。
2. 修改蓝图图表。
3. 增删改组件。
4. 修改 UMG。
5. 修改 DataAsset / DataTable。
6. Append / Replace / Patch / Merge 图表逻辑。
7. Cleanup / Rollback / ConvertToUserOwned。
8. 保存前需要确认写权限的自动流程。
```

只读任务通常不要求 write_permission，但仍可按需读取 runtime_profile 判断 Bridge 是否可用。

---

## 4. 何时重新读取 runtime_profile

同一写入阶段内可复用本次 runtime_profile。

遇到以下情况必须重新读取：

```text
1. Bridge disconnected / reconnected。
2. UE Editor 重启。
3. 项目切换。
4. Token 缺失、无效、过期或刷新。
5. write_permission 变化。
6. settings.json 或 Safety Profile 变化。
7. 工具返回 tool unavailable / command unregistered。
8. 写工具返回状态不可信。
9. runtime_profile 与实际 MCP schema 明显不一致。
10. 用户手动修复阻断后要求继续。
```

---

## 5. schema 读取规则

runtime_profile 采用双层 schema：

```json
{
  "schema": "BlueprintHelper.McpToolResult.v1",
  "data": {
    "schema": "BlueprintHelper.RuntimeProfile.v1"
  }
}
```

Agent 应理解为：

```text
顶层 schema = MCP 工具结果基础协议版本。
data.schema = runtime_profile 数据结构版本。
```

Agent 不应把 `data.schema` 当作 MCP 返回体总协议版本。

---

## 6. write_permission 规则

字段：

```json
{
  "write_permission": {
    "enabled": true,
    "reason": "ok"
  }
}
```

规则：

```text
1. enabled=false 时，不得调用任何写工具。
2. Token 缺失 / 无效 / 过期不是工具系统异常，而是预期安全状态。
3. enabled=false 且当前任务需要写入时，Agent 必须 stop_and_report。
4. 只读分析任务可继续执行只读工具。
```

常见 reason：

```text
ok
token_missing
token_invalid
token_expired
token_missing_or_invalid
config_unavailable
setup_not_completed
safety_profile_read_only
write_disabled
unknown
```

---

## 7. risk_command 规则

字段：

```json
{
  "risk_command": {
    "enabled": false,
    "reason": "risk_command_missing",
    "blocked_commands": [
      "close_editor",
      "exec_console_command"
    ]
  }
}
```

规则：

```text
1. risk_command 只影响高风险生命周期或命令执行工具。
2. risk_command 不阻断普通蓝图读写。
3. 如果用户任务要求 close_editor / exec_console_command，而该命令在 blocked_commands 中，Agent 必须 stop_and_report。
4. Agent 不得用其他普通工具绕过 risk_command 限制。
```

---

## 8. active_profile 规则

字段：

```json
{
  "active_profile": {
    "safety_profile": "conservative",
    "missing_capability_policy": "stop_and_report"
  }
}
```

规则：

```text
1. safety_profile 只从 runtime_profile.active_profile 读取。
2. 单次工具结果中的 safety 不携带 safety_profile。
3. Agent 不得从历史对话或记忆中假设当前 safety_profile。
4. missing_capability_policy 当前按 stop_and_report 处理。
```

Safety Profile 解释规则由 AgentGuide / Safety Profile 文档提供，不由 runtime_profile 展开。

---

## 9. tool_capabilities 负向稀疏规则

runtime_profile 使用 unavailable_only 模式：

```json
{
  "tool_capabilities": {
    "mode": "unavailable_only",
    "unavailable": []
  }
}
```

Agent 必须按以下语义理解：

```text
1. runtime_profile 只返回当前不可用、禁用、降级或阻断的能力。
2. 未出现在 unavailable 列表中的能力，不代表 runtime_profile 已展开确认其完整 schema。
3. 可用工具簇和工具规则由 AgentGuide / tools 索引提供。
4. 具体调用参数仍以 MCP tool schema 为准。
5. runtime_profile 不是工具索引，不是 schema 文档。
```

---

## 10. unavailable item 规则

单项结构：

```json
{
  "cluster": "graph_write",
  "capability": "merge",
  "status": "unavailable",
  "reason": "not_implemented"
}
```

字段含义：

| 字段 | Agent 解释 |
|---|---|
| `cluster` | 工具簇。 |
| `capability` | 子能力。 |
| `status` | 当前能力状态。 |
| `reason` | 稳定原因码。 |

Agent 不应期待以下字段：

```text
severity
stop_and_report
message
required_tool
```

因为：

```text
1. severity 取决于当前任务。
2. stop_and_report 由任务需求、missing_capability_policy、不可用能力和可替代路径共同决定。
3. message 由 Agent 根据 reason 转述。
4. required_tool 应来自 AgentGuide / MCP schema，不来自 runtime_profile。
```

---

## 11. status 解释规则

| status | Agent 解释 |
|---|---|
| `unavailable` | 当前版本没有、未实现或未注册。 |
| `disabled` | 能力存在，但被配置或 profile 禁用。 |
| `degraded` | 能力可用，但能力降级。 |
| `blocked` | 当前运行状态阻断。 |

示例：

```json
{
  "cluster": "graph_write",
  "capability": "merge",
  "status": "unavailable",
  "reason": "not_implemented"
}
```

如果当前任务需要 MergeBlueprintGraph，且无安全替代路径，Agent 必须 stop_and_report。

---

## 12. reason 解释规则

| reason | Agent 解释 |
|---|---|
| `not_implemented` | 当前版本未实现。 |
| `not_registered` | MCP tool 或 UE command 未注册。 |
| `version_unsupported` | 当前绑定版本不支持。 |
| `config_unavailable` | 配置不可用。 |
| `write_permission_disabled` | 写权限整体不可用。 |
| `token_missing` | 写 Token 缺失。 |
| `token_invalid` | 写 Token 无效。 |
| `token_expired` | 写 Token 过期。 |
| `safety_profile_read_only` | 当前 Safety Profile 为 ReadOnly。 |
| `bridge_disconnected` | UE Bridge 未连接。 |
| `editor_not_running` | UE Editor 未运行。 |
| `resource_store_unavailable` | resource_ref 存储不可用。 |
| `dependency_missing` | 能力依赖缺失。 |
| `unknown` | 未知原因。 |

Agent 可根据这些 reason 向用户转述最小修复动作，但不得把缺失能力伪装成已完成。

---

## 13. stop_and_report 判断规则

runtime_profile 不直接返回 `stop_and_report=true`。

Agent 应按以下条件判断：

```text
当前任务需要某能力
+
该能力出现在 tool_capabilities.unavailable 中
+
active_profile.missing_capability_policy = stop_and_report
+
无安全替代路径
=
stop_and_report
```

stop_and_report 报告必须包含：

```text
1. 用户任务目标。
2. 已读取的 runtime_profile 关键信息。
3. 阻断的 cluster / capability / status / reason。
4. 为什么不能继续写。
5. 用户可执行的最小修复动作。
```

不得包含：

```text
1. 猜测工具可用。
2. 尝试用无关工具绕过。
3. 继续写半成品。
4. 要求用户手动补齐以绕过 Safety Profile 或 dry_run conflict。
```

---

## 14. 能力簇索引规则

Agent 应从 AgentGuide / tools 索引理解标准能力簇。

第一版标准簇包括：

```text
logic_read
raw_json_export
asset_factory
blueprint_component
blueprint_class_settings
graph_write
transaction
review
cleanup
validation
enhanced_input
umg
data_asset
data_table
editor_lifecycle
```

runtime_profile 只返回这些簇中当前不可用、禁用、降级或阻断的项。

---

## 15. 不可用能力处理示例

### 示例 A：Merge 不可用

runtime_profile：

```json
{
  "cluster": "graph_write",
  "capability": "merge",
  "status": "unavailable",
  "reason": "not_implemented"
}
```

任务：把新逻辑接入已有 BeginPlay。

Agent 处理：

```text
1. 该任务需要 MergeBlueprintGraph。
2. merge 不可用。
3. missing_capability_policy=stop_and_report。
4. 不得用 Append 替代接入已有执行流。
5. stop_and_report。
```

### 示例 B：Enhanced Input 不可用

runtime_profile：

```json
{
  "cluster": "enhanced_input",
  "capability": "edit_mapping_context",
  "status": "unavailable",
  "reason": "not_implemented"
}
```

任务：完整实现 F 键交互并绑定 IMC。

Agent 处理：

```text
1. 任务需要 edit_mapping_context。
2. 能力不可用。
3. 如果用户要求 Agent 独立完成完整功能，必须 stop_and_report。
4. 不能把用户手动绑定 IMC 计入 Agent 独立完成能力。
```

### 示例 C：resource_ref 降级

runtime_profile：

```json
{
  "cluster": "raw_json_export",
  "capability": "resource_ref",
  "status": "degraded",
  "reason": "resource_store_unavailable"
}
```

任务：读取小型蓝图 LogicMD。

Agent 处理：

```text
1. 当前任务不需要 raw_json resource_ref。
2. 可继续。
3. 不需要 stop_and_report。
```

---

## 16. runtime_profile 与 diagnostics 的边界

Agent 应区分：

```text
runtime_profile：
当前运行时事实摘要，供任务前判断。

diagnostics：
只读诊断命令，用于安装、配置、Bridge、runtime 链路问题定位。
```

runtime_profile 不展开完整 diagnostics。  
如果 runtime_profile 返回 `config_status=config_unavailable` 或 `bridge_status=disconnected`，Agent 可按任务需要调用 diagnostics 工具进一步定位，但不得继续写操作。

---

## 17. Agent 最终报告规则

正常完成任务时，Agent 最终报告不需要展开完整 runtime_profile。

只有以下情况需要提及 runtime_profile：

```text
1. Bridge / config / write_permission 异常。
2. risk_command 阻断用户请求。
3. tool_capabilities 中能力缺失导致 stop_and_report。
4. Safety Profile 限制当前操作。
5. 用户要求调试或查看能力状态。
```

---

## 18. Agent 侧验收标准

```text
1. Agent 在写入阶段前读取 runtime_profile。
2. Agent 不从单次工具结果读取 safety_profile。
3. Agent 不期待 runtime_profile 返回命名偏好或蓝图 / C++ 边界。
4. Agent 从 AgentGuide / tools 索引读取可用工具簇和调用规则。
5. Agent 将 tool_capabilities 视为 unavailable_only 负向稀疏列表。
6. Agent 不把未列出的能力理解为 runtime_profile 已完整展开 schema。
7. Agent 遇到任务必需能力不可用且无安全替代路径时 stop_and_report。
8. Agent 不使用 Append 替代 Merge 去接入已有执行流。
9. Agent 不使用无关工具绕过 risk_command。
10. Agent 不继续写半成品。
```
