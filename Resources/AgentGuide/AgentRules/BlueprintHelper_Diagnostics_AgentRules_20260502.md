# BlueprintHelper Agent 侧规则：Diagnostics 使用规范

日期：2026-05-02  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：Diagnostics Agent 侧规则确认稿  
本文边界：只规定 Agent 如何调用和解释 Diagnostics。UE 字段映射见独立 UE 字段映射文档。

---

## 1. Diagnostics 的职责

Diagnostics 是只读诊断工具，用于定位安装、配置、Bridge、runtime 链路问题。

Agent 应区分：

```text
runtime_profile：
当前运行时事实摘要，供任务前判断。

diagnostics：
只读诊断报告，用于定位环境、配置、Bridge、runtime 链路问题。
```

Diagnostics 不用于：

```text
1. 判断某个具体蓝图任务能否完成。
2. 替代 runtime_profile。
3. 替代 AgentPlan。
4. 读取蓝图 LogicMD / LogicJson。
5. 执行修复。
6. 迁移 settings。
7. 写 CLAUDE.md。
8. 写 Project Marker。
```

---

## 2. 工具入口

Agent 可使用两个诊断入口：

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

## 3. 何时使用 Static Diagnostics

Static Diagnostics 用于安装与配置静态检查，不要求 UE Editor 正在运行。

Agent 可在以下情况使用：

```text
1. 用户询问 BlueprintHelper 安装是否正常。
2. setup 后需要只读检查。
3. Agent 找不到 BlueprintHelper Skill 入口。
4. Agent 怀疑全局 CLAUDE.md managed block 缺失。
5. Agent 怀疑 settings.json 不可用。
6. 当前目录疑似 UE 项目，需要检查 Project Marker。
```

Static Diagnostics 不应用于：

```text
1. 判断 UE Bridge 是否 connected。
2. 判断 Token 是否可写。
3. 判断 risk_command。
4. 判断某个蓝图工具是否能执行。
```

---

## 4. 何时使用 Runtime Diagnostics

Runtime Diagnostics 用于 UE / MCP / Bridge / runtime profile 链路检测。

Agent 可在以下情况使用：

```text
1. UE Editor 已运行但工具无法连接。
2. runtime_profile 不可用。
3. Bridge disconnected。
4. write_permission 异常，需要定位是 Token、config 还是其他状态。
5. risk_command 状态需要确认。
6. 用户要求检查当前运行链路是否正常。
```

Runtime Diagnostics 不应用于：

```text
1. 判断某个蓝图图表是否能修改。
2. 判断某个资产是否存在。
3. 替代 LogicMD / LogicJson。
4. 替代具体任务能力检查。
```

---

## 5. 返回体解释规则

Diagnostics 使用 ToolResultBase 外壳：

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
    "markdown": "..."
  }
}
```

Agent 应理解：

```text
顶层 schema = MCP 工具结果基础协议版本。
data.schema = Diagnostics 数据结构版本。
data.markdown = 实际诊断报告。
```

---

## 6. Markdown 解释规则

Diagnostics 只返回 Markdown，不额外返回 JSON 数组。

固定结构：

```md
## Blocking
None

## Warning
None

## Info
- code
```

Agent 必须按以下规则解释：

```text
1. Blocking 表示诊断发现阻断项。
2. Warning 表示风险、受限或非阻断问题。
3. Info 表示当前已确认的正常状态。
4. Blocking 和 Warning 必须出现。
5. Info 可选。
```

---

## 7. ok/status 解释规则

如果诊断执行成功，即使发现 Blocking，也应是：

```json
{
  "ok": true,
  "status": "completed"
}
```

这表示：

```text
诊断工具运行成功。
诊断结果中发现的问题写在 data.markdown。
```

只有 diagnostics 工具自身失败时，才是：

```json
{
  "ok": false,
  "status": "failed",
  "error": {}
}
```

Agent 不得把 Markdown 中的 Blocking 误判为工具调用失败。

---

## 8. Static Diagnostics 解读规则

Static Diagnostics 典型 code：

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
```

Agent 处理规则：

```text
1. settings.unavailable 出现在 Blocking 时，不能继续写任务。
2. global_guidance.missing 或 skill_entry.invalid 出现时，应提示用户运行 /blueprinthelper-setup 或检查安装。
3. project_marker.missing 通常是 Warning，不自动写入项目 CLAUDE.md；只有用户确认后才能写 Project Marker。
4. version.mismatch 是 Blocking；不能继续 setup 或写入。
```

---

## 9. Runtime Diagnostics 解读规则

Runtime Diagnostics 典型 code：

```text
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

Agent 处理规则：

```text
1. bridge.disconnected 出现在 Blocking 时，不得调用 UE 写工具。
2. runtime_profile.unavailable 出现在 Blocking 时，不得进入写入阶段。
3. config_status.unavailable 出现时，应 stop_and_report，并提示关闭 UE 后运行 setup。
4. write_permission.disabled 对只读任务不阻断；对写任务阻断。
5. risk_command.disabled 只阻断 close_editor / exec_console_command 等高风险命令，不阻断普通蓝图读写。
```

---

## 10. 不输出 Suggested action 的解释规则

Diagnostics Markdown 不输出 Suggested action。

Agent 可以根据 code 转述最小修复动作，但必须克制：

```text
1. 不编造 diagnostics 没有给出的状态。
2. 不展开本地路径。
3. 不要求用户做与 code 无关的操作。
4. 不把手动补齐当作 Agent 独立完成能力。
```

---

## 11. Diagnostics 与 stop_and_report

Diagnostics 本身不直接决定 stop_and_report。

Agent 应结合：

```text
1. 当前用户任务。
2. runtime_profile。
3. diagnostics Markdown 中的 Blocking / Warning。
4. active_profile.missing_capability_policy。
5. 是否存在安全替代路径。
```

如果当前任务是写任务，且 diagnostics 出现：

```text
bridge.disconnected
runtime_profile.unavailable
config_status.unavailable
write_permission.disabled
```

则 Agent 应 stop_and_report。

---

## 12. 不能用 Diagnostics 绕过 runtime_profile

Agent 不得这样做：

```text
runtime_profile 显示 write_permission.disabled
→ diagnostics 没有报错
→ Agent 继续写
```

正确规则：

```text
runtime_profile 是任务前运行时事实来源。
diagnostics 只用于定位问题。
diagnostics 不能覆盖 runtime_profile 的写权限、安全档位或能力缺失判断。
```

---

## 13. Agent 最终报告规则

正常任务完成时，不需要报告 diagnostics 内容。

只有以下情况需要报告：

```text
1. 用户明确要求诊断。
2. diagnostics 发现 Blocking。
3. diagnostics 发现影响当前任务的 Warning。
4. runtime_profile 异常后调用 diagnostics 定位原因。
5. Agent stop_and_report 需要说明阻断来源。
```

报告时应只转述相关 code 和含义，不展开完整 Markdown。

---

## 14. Agent 侧验收标准

```text
1. Agent 能区分 runtime_profile 与 diagnostics。
2. Agent 不用 diagnostics 替代 runtime_profile。
3. Agent 能正确读取 data.markdown。
4. Agent 不期待 blocking / warning / info JSON 数组。
5. Agent 理解 diagnostics 成功但发现 Blocking 时 ok=true。
6. Agent 只在 diagnostics 工具自身失败时把 ok=false 视为工具失败。
7. Agent 不从 diagnostics 中获取命名偏好或蓝图 / C++ 边界。
8. Agent 不用 diagnostics 判断具体蓝图任务能否完成。
9. Agent 不使用 diagnostics 绕过 write_permission / risk_command / Safety Profile。
10. Agent 只在必要时向用户报告 diagnostics 结果。
```
