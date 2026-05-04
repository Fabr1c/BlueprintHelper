# 05 Validation / Diagnostics Tools 设计文档（已同步确认 Diff）

日期：2026-05-03  
工具簇：Validation / Diagnostics Tools / 验证与诊断工具簇  
状态：同步确认 Diff 后的修正版  
同步范围：Diagnostics Markdown 返回、ToolResultBase 解释、runtime diagnostics 边界、写后 validation 字段收敛。

---

## 0. 本次同步结论

本文件替换旧版中以下过期口径：

```text
1. /blueprinthelper-diagnostics 与 /blueprinthelper-diagnostics --runtime 返回 ToolResultBase 外壳，实际诊断报告在 data.markdown。
2. Diagnostics 不返回 blocking / warning / info JSON 数组。
3. Diagnostics Markdown 必须包含 ## Blocking 和 ## Warning，## Info 可选。
4. Diagnostics 工具执行成功时，即使 Markdown 中有 Blocking，也应 ok=true、status=completed。
5. Markdown Blocking 表示诊断发现环境或运行链路阻断项，不等于 MCP 工具调用失败。
6. 只有 ok=false / status=failed 表示 diagnostics 工具自身失败。
7. 普通写工具的 validation 仍可返回 should_compile / should_save，但不代表所有工具都默认返回 should_validate / recommended_next_tool。
```

---

## 1. 四个概念边界

```text
Graph Diagnostics = 当前蓝图 / 图表状态体检。
preflight = 写工具内部强制写入前安全检查。
dry_run = 写工具非写入预演模式。
Review = 写入后的 transaction 审查和回滚入口。
```

四者不能互相替代。

| 概念 | 发生时间 | 是否写资产 | 用途 |
|---|---:|---:|---|
| Diagnostics | 写入前 / 写入后 / 单独测试 | 否 | 检查安装、配置、Bridge、runtime 或资产 / 图表状态 |
| preflight | 写入前 | 否 | 判断本次写操作是否安全 |
| dry_run | 写入前 | 否 | 完整预演工具调用 |
| Review | 写入后 | 已写入 | 审查真实改动，Accept / Reject / rollback |

---

## 2. Diagnostics 分层

Diagnostics 分为两类：

```text
安装 / 配置 / Bridge / runtime 链路诊断
蓝图 / 图表 / 资产状态诊断
```

命令：

```text
/blueprinthelper-diagnostics
/blueprinthelper-diagnostics --runtime
```

工具：

```text
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
```

蓝图资产级诊断可继续规划：

```text
blueprint_graph_diagnostics
blueprint_asset_diagnostics
blueprint_project_diagnostics（后置）
```

---

## 3. Diagnostics 返回体

Diagnostics 使用 ToolResultBase 外壳：

```json
{
  "ok": true,
  "schema": "BlueprintHelper.McpToolResult.v1",
  "operation": "diagnostics_runtime",
  "trace_id": "trace_20260503_0001",
  "status": "completed",
  "modified": false,
  "data": {
    "schema": "BlueprintHelper.Diagnostics.v1",
    "mode": "runtime",
    "format": "markdown",
    "markdown": "## Blocking
None

## Warning
None

## Info
- bridge.connected"
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

## 4. Markdown 解释规则

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

规则：

```text
1. Blocking 表示诊断发现阻断项。
2. Warning 表示风险、受限或非阻断问题。
3. Info 表示当前已确认的正常状态。
4. Blocking 和 Warning 必须出现。
5. Info 可选。
```

Diagnostics 不返回：

```text
blocking[]
warning[]
info[]
structured_issues
summary_md
```

这些可以作为后续 graph diagnostics / asset diagnostics 的扩展，但不适用于当前安装 / runtime diagnostics 命令。

---

## 5. ok/status 解释规则

如果诊断命令执行成功，即使发现 Blocking，也应是：

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

## 6. Static Diagnostics

Static Diagnostics 用于安装与配置静态检查，不要求 UE Editor 正在运行。

典型 code：

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

处理规则：

```text
1. settings.unavailable 出现在 Blocking 时，不能继续写任务。
2. global_guidance.missing 或 skill_entry.invalid 出现时，应提示用户运行 /blueprinthelper-setup 或检查安装。
3. project_marker.missing 通常是 Warning，不自动写入项目 CLAUDE.md；只有用户确认后才能写 Project Marker。
4. version.mismatch 是 Blocking；不能继续 setup 或写入。
```

---

## 7. Runtime Diagnostics

Runtime Diagnostics 用于 UE / MCP / Bridge / runtime profile 链路检测。

典型 code：

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

处理规则：

```text
1. bridge.disconnected 出现在 Blocking 时，不得调用 UE 写工具。
2. runtime_profile.unavailable 出现在 Blocking 时，不得进入写入阶段。
3. config_status.unavailable 出现时，应 stop_and_report，并提示关闭 UE 后运行 setup。
4. write_permission.disabled 对只读任务不阻断；对写任务阻断。
5. risk_command.disabled 只阻断 close_editor / exec_console_command 等高风险命令，不阻断普通蓝图读写。
```

---

## 8. Diagnostics 与 runtime_profile 边界

runtime_profile：

```text
当前运行时事实摘要，供任务前判断。
```

Diagnostics：

```text
只读诊断报告，用于定位安装、配置、Bridge、runtime 链路问题。
```

Diagnostics 不得替代 runtime_profile。

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

## 9. Graph / Asset Diagnostics 后续边界

蓝图图表诊断可继续检查：

```text
duplicate_events
duplicate_custom_events
orphan_nodes
unreachable_nodes
unconnected_exec_flow
empty_function_body
dangling_links
invalid_pin_links
required_pin_missing
owned_block_integrity
event_entry_integrity
```

资产诊断可汇总：

```text
graph diagnostics
component tree diagnostics
class settings diagnostics
compile status
dirty / save state
ownership metadata integrity
transaction journal consistency
```

但这些蓝图级 diagnostics 仍然只读，不提供：

```text
auto_fix=true
```

AutoRepair 可以读取 Diagnostics 结果后另行调用写工具，但 Diagnostics 本身不直接修复。

---

## 10. compile / save / PIE

compile / save / PIE 不重复造新工具。

现有编译、保存、PIE 启停、编辑器命令工具保持原有归属，但在工作流层归入 Validation Workflow。

Validation Workflow 可包含：

```text
blueprint_graph_diagnostics
blueprint_asset_diagnostics
blueprint_compile_asset
blueprint_save_asset
editor_start_pie
editor_stop_pie
editor_run_smoke_test（后续可选）
```

Save 是落盘动作，应遵守 Safety Profile 与用户授权规则。

---

## 11. 写后 validation 字段

普通写工具可返回：

```json
{
  "validation": {
    "should_compile": true,
    "should_save": true,
    "compiled": false,
    "saved": false
  }
}
```

字段规则：

```text
1. should_compile / should_save 是 Agent 后续闭环提示。
2. compiled / saved 表示本工具调用中是否已经执行。
3. no_op 且 modified=false 时，通常 should_compile=false / should_save=false。
4. 自动 diagnostics / compile / save 由 Safety Profile 和 workflow 参数决定。
```

不再强制所有写工具默认返回：

```text
should_validate
recommended_next_tool
recommended_validation_workflow
```

这些字段可以在需要复杂工作流的工具中按需返回，但不是普通工具的统一字段义务。

---

## 12. 严重度分级

Graph / Asset Diagnostics 结果可采用四档：

```text
blocker
error
warning
info
```

语义：

```text
blocker：当前操作必须停止。
error：资产或图表存在明确错误，通常需要修复；是否阻断取决于操作和 Profile。
warning：风险或潜在问题，不一定阻断。
info：说明性信息。
```

安装 / runtime diagnostics 的 Markdown 中则用 `## Blocking / ## Warning / ## Info` 分区表达。

---

## 13. Agent 报告规则

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

## 14. 验收标准

```text
1. Agent 能区分 runtime_profile 与 diagnostics。
2. Agent 不用 diagnostics 替代 runtime_profile。
3. Diagnostics 返回 data.markdown。
4. Diagnostics 不返回 blocking / warning / info JSON 数组。
5. Diagnostics Markdown 必须包含 ## Blocking 和 ## Warning。
6. Diagnostics 执行成功但发现 Blocking 时仍 ok=true/status=completed。
7. 只有 ok=false/status=failed 才是 diagnostics 工具自身失败。
8. Agent 只在必要时向用户报告 diagnostics。
9. 普通写工具 validation 可只返回 should_compile / should_save / compiled / saved。
10. Diagnostics 永远只读，不执行修复。
```
---

# 2026-05-04 Task Context / Preview / Execute 同步

## 同步结论

Validation / Diagnostics 不替代 TaskSpec preview。新增三个任务级概念：

```text
TaskContextPack：给 Agent 生成 TaskSpec 前使用。
preview_task：校验 TaskSpec / TaskPlan / dry_run，不写资产。
execute_task：执行已通过 preview 的 TaskPlan。
```

## read_task_context

`blueprinthelper_read_task_context` 返回压缩上下文，不返回完整 RawJson 或巨量 LogicJson。

最小返回：

```json
{
  "schema": "BlueprintHelper.TaskContextPack.v1",
  "context_id": "ctx_20260504_0001",
  "runtime": {
    "write_permission": "enabled",
    "safety_profile": "Conservative",
    "missing_capability_policy": "stop_and_report"
  },
  "target": {
    "asset_path": "/Game/BP/BP_Door",
    "exists": true,
    "asset_type": "blueprint"
  },
  "blueprint_summary": {
    "components": [],
    "graphs": [],
    "implemented_interfaces": [],
    "variables": []
  },
  "resource_candidates": {},
  "recommended_constraints": {}
}
```

## preview_task

preview_task 不写资产。它可以返回：

```text
status=completed / preview_blocked / context_required / context_stale / failed
```

规则：

```text
TaskSpec schema/semantic 错误：ok=false,status=failed,error.issues。
TaskSpec 合法但预览阻断：ok=true,status=preview_blocked,data.preview。
需要更多上下文：ok=true,status=context_required,data.preview.issues[].context_query。
```

## execute_task

execute_task 只执行 preview 已通过的 TaskSpec / TaskPlan。

如果未 preview：

```text
error.code=preview_required
agent_action=run_preview_task_first
```

执行失败必须返回 execution_state：

```json
{
  "execution_state": {
    "started": true,
    "write_started": true,
    "modified": false,
    "rollback_result": "rolled_back",
    "safe_to_retry": false
  }
}
```

rollback blocked / failed 时：

```text
state_trust=unsafe
agent_action=stop_and_report
```
