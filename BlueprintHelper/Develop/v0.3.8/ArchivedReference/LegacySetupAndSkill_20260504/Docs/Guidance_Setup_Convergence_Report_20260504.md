# BlueprintHelper AgentGuide / SetupGuide 收敛报告

日期：2026-05-04  
输入依据：同步架构文档、TaskSpec / TaskPlan 混合编排方案、已同步字段协议 Diff、旧 AgentGuide / Setup 流程。  
输出对象：Agent Guide、Setup Guide、Resources 补丁包。

---

## 1. 本次收敛主线

本次收敛将 Agent 引导文档从“底层工具簇选择手册”调整为 **TaskSpec-first Agent 操作规约**。

旧主线：

```text
Agent 直接选择 Asset Factory / Component / Class Settings / Graph Write 等底层 MCP 工具
```

新主线：

```text
Agent
→ blueprinthelper_get_runtime_profile
→ blueprinthelper_read_context / blueprinthelper_read_reference_context
→ BlueprintHelper.TaskSpec.v1
→ blueprinthelper_preview_task
→ blueprinthelper_execute_task
→ blueprinthelper_get_task_result
```

底层能力簇不废弃，但降级为：

```text
TaskPlan capability
UE Task Runtime 内部能力
debug / expert 工具
自动化测试入口
迁移期 fallback
```

---

## 2. Agent Guide 主要变更

### 2.1 默认工具链变更

Agent Guide 中普通写任务的默认流程已改为：

```text
get_runtime_profile -> read_context / read_reference_context -> TaskSpec -> preview_task -> execute_task
```

不再要求普通 Agent 输出完整底层 MCP 工具序列。

### 2.2 迁移期 fallback 明确化

新增 fallback 规则：

```text
1. task tools 可用：使用 TaskSpec-first。
2. task tools 不可用且 SetupProfile 允许 fallback：可进入 debug / expert 底层工具模式。
3. task tools 不可用且 fallback 未允许：stop_and_report。
```

### 2.3 runtime_profile 权威化

明确：

```text
safety_profile 只从 runtime_profile.active_profile 读取。
runtime_profile.tool_capabilities 是 unavailable_only。
runtime_profile 不是 MCP schema，也不是完整工具索引。
```

### 2.4 diagnostics 边界固定

明确：

```text
diagnostics 只读。
实际报告在 data.markdown。
Markdown Blocking 不等于工具调用失败。
diagnostics 不覆盖 runtime_profile。
```

### 2.5 底层工具返回字段收敛

普通能力工具成功结果默认只消费：

```text
status
modified
data.*_result
validation
```

不再默认期待：

```text
transaction
review
safety
rollback_data
before / after
```

Graph Write / Cleanup / Rollback / Ownership 等需要后续引用的流程才按需返回 write_ref / block_ref / rollback handle。

---

## 3. Setup Guide 主要变更

### 3.1 Setup 文档职责收窄

Setup Guide 只负责用户引导侧：

```text
安装
配置
诊断
SetupProfile
runtime_profile
Project Marker
Skill entry
偏好采集
Troubleshooting
```

不再混入 Agent 操作步骤或底层 MCP API Reference。

### 3.2 新增 TaskSpec-first 配置

SetupProfile 中新增或明确：

```json
{
  "agent_entry_mode": "task_spec_first",
  "fallback_when_task_tools_unavailable": "stop_and_report",
  "missing_capability_policy": "stop_and_report"
}
```

### 3.3 安全档位固定为五档

```text
ReadOnly
Conservative
Standard
AutoRepair
Expert
```

默认推荐：`Conservative`。

### 3.4 Setup Wizard 问答收敛

Setup Wizard 需要采集：

```text
项目 agent-profile 的 `environment.ue_engine_dir` 绝对路径；`.uproject` 只作为工具调用的显式 `project_file`
Safety Profile
TaskSpec-first fallback 策略
Blueprint / C++ 边界
命名规则
Graph Write 修改用户内容策略
Enhanced Input 自动创建 / 自动编辑边界
Review / Journal / rollback retention
Project Marker 写入确认
```

### 3.5 Project Marker 模板更新

CLAUDE.md / AGENTS.md 默认指向：

```text
runtime_profile
read_context / read_reference_context
TaskSpec
preview_task
execute_task
```

并声明底层能力工具只用于 debug / expert / migration fallback。

---

## 4. 输出文件

```text
BlueprintHelper_AgentGuide_TaskSpecFirst_20260504.md
BlueprintHelper_SetupGuide_TaskSpecFirst_20260504.md
BlueprintHelper_Guidance_Setup_TaskSpecFirst_patch_20260504.zip
```

补丁包主要路径：

```text
BlueprintHelper/Resources/Docs/
BlueprintHelper/Resources/AgentGuide/
BlueprintHelper/Resources/Setup/
BlueprintHelper/Resources/Setup/templates/
BlueprintHelper/Resources/Skills/BlueprintHelper/
```

---

## 5. 建议后续同步点

1. 当 `blueprinthelper_read_context / preview_task / execute_task` 实现后，把 Setup smoke test 改成真实 task preview。
2. 当 TaskPlan schema 稳定后，把 `TaskSpec` 示例替换成正式 schema 引用。
3. 当 runtime_profile 实现后，用真实字段替换当前文档中的建议字段。
4. 当 Review UI 支持 task_run_id 分组后，补充 Setup 中的 task-level Review 策略。
5. 当 fallback 策略稳定后，决定是否让 Claude Code 默认允许 `capability_debug_allowed`。
