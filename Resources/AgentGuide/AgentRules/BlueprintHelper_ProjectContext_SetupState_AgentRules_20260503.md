# BlueprintHelper Agent 侧规则：Project Context / Project Marker / Setup State 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：Project Context / Project Marker / Setup State Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 `read_project_context`、`check_project_marker`、`check_setup_state` 三个只读工具，包括 Project Marker 边界、setup 状态边界、隐私字段边界、无 validation / transaction 返回规则。UE 字段映射见独立文档。

---

## 1. 工具簇职责

本簇用于只读检查：

```text
当前是否识别为 UE 项目
当前项目是否存在 BlueprintHelper Project Marker
当前项目是否启用 BlueprintHelper 工作流
setup 是否完成或可用
```

本簇不负责：

```text
写入 Project Marker
修复 Project Marker
删除 Project Marker
运行 /blueprinthelper-setup
修复 settings.json
迁移 settings.json
写全局 CLAUDE.md managed block
```

---

## 2. 工具列表

第一版包含：

```text
read_project_context
check_project_marker
check_setup_state
```

---

## 3. 通用规则

本簇全部为只读工具。

Agent 应期待：

```text
modified=false
```

Agent 不应期待：

```text
validation
write_ref
transaction_id
journal_recorded
review
safety
rollback_data
```

所有 `data.schema` 使用短命名。

---

## 4. 隐私 / 路径边界

本簇默认不返回：

```text
project_root
uproject_path
settings_path
absolute CLAUDE.md path
CLAUDE.md 全文
Project Marker 文本
Skill / AgentGuide 全文
settings.json 全文
```

如果需要更完整诊断，应使用 diagnostics/debug 类工具，而不是本簇普通只读工具。

---

# 5. read_project_context

## 5.1 职责

`read_project_context` 读取当前 UE 项目的最小上下文状态。

返回：

```text
project_context.status
project_detected
project_marker
workflow_enabled
reason
```

---

## 5.2 正常返回解释

```json
{
  "project_context": {
    "status": "ok",
    "project_detected": true,
    "project_marker": "present",
    "workflow_enabled": true
  }
}
```

Agent 应理解：

```text
当前 UE 项目已识别。
Project Marker 存在。
BlueprintHelper 工作流可用。
```

---

## 5.3 项目未识别

```json
{
  "project_context": {
    "status": "blocked",
    "project_detected": false,
    "workflow_enabled": false,
    "reason": "ue_project_not_detected"
  }
}
```

Agent 应理解：

```text
当前目录或运行上下文未识别为 UE 项目。
写入型 BlueprintHelper 工作流不可继续。
```

---

## 5.4 Project Marker 缺失

```json
{
  "project_context": {
    "status": "degraded",
    "project_detected": true,
    "project_marker": "missing",
    "workflow_enabled": false,
    "reason": "project_marker_missing"
  }
}
```

Agent 应理解：

```text
项目已识别，但尚未确认启用 BlueprintHelper Project Marker。
```

当用户首次要求修改蓝图且 Project Marker 缺失时：

```text
Agent 必须请求用户确认。
不得静默写入项目 CLAUDE.md。
```

---

# 6. check_project_marker

## 6.1 职责

`check_project_marker` 只读检查项目级 CLAUDE.md 中 BlueprintHelper Project Marker 状态。

它不写入、不修复、不删除 marker。

---

## 6.2 Marker 存在

```json
{
  "project_marker": {
    "status": "present",
    "workflow_enabled": true
  }
}
```

Agent 可继续按 BlueprintHelper 项目工作流处理。

---

## 6.3 Marker 缺失

```json
{
  "project_marker": {
    "status": "missing",
    "workflow_enabled": false,
    "reason": "project_marker_missing"
  }
}
```

Agent 不得静默补写 marker。

---

## 6.4 Marker invalid

```json
{
  "project_marker": {
    "status": "invalid",
    "workflow_enabled": false,
    "reason": "project_marker_invalid"
  }
}
```

Agent 应 stop_and_report 或请求用户确认修复流程，不得自行覆盖用户文件内容。

---

# 7. check_setup_state

## 7.1 职责

`check_setup_state` 只读检查 BlueprintHelper setup 是否完成，以及 runtime 所需配置是否可用。

---

## 7.2 setup 正常

正常态极简：

```json
{
  "setup_state": {
    "status": "ok"
  }
}
```

Agent 应理解 setup 可用，不需要展开配置细节。

---

## 7.3 setup blocked

```json
{
  "setup_state": {
    "status": "blocked",
    "reason": "setup_not_completed"
  }
}
```

或：

```json
{
  "setup_state": {
    "status": "blocked",
    "reason": "config_unavailable"
  }
}
```

Agent 应根据 reason stop_and_report，并提示用户走 setup / diagnostics 流程。

---

## 7.4 不展开 settings 具体损坏类型

`check_setup_state` 不区分：

```text
settings missing
settings invalid
settings damaged
settings old_version
settings missing_fields
```

这些属于：

```text
/blueprinthelper-setup
/blueprinthelper-diagnostics
```

的职责。

---

# 8. 与 runtime_profile / diagnostics 的关系

Agent 应按以下职责区分：

```text
runtime_profile：
- 当前任务运行链路。
- UE/MCP/Bridge/write_permission/能力是否可用。
- 正常态只返回 runtime_profile.status=ok。

read_project_context：
- 当前项目工作流。
- 项目是否识别、Project Marker 是否存在。

check_setup_state：
- setup 是否 ok 或 blocked。

diagnostics：
- 安装 / 配置 / 运行链路诊断。
- 输出 Markdown 报告。
```

Agent 不应要求 read_project_context 展开 runtime_profile，也不应要求 check_setup_state 展开 diagnostics。

---

# 9. Project Marker 写入边界

第一版没有：

```text
write_project_marker
repair_project_marker
remove_project_marker
```

规则：

```text
Project Marker 写入必须用户确认。
Agent 不得静默修改项目 CLAUDE.md。
```

如果后续设计写入工具，也必须：

```text
dry_run
用户确认
只改 managed block
不覆盖用户内容
不返回 transaction_id
不进入 UE Transaction Journal
```

---

# 10. 工具自身失败

本簇多数检查异常应通过 data status/reason 表达。

只有工具自身失败才返回：

```text
ok=false
status=failed
error
```

例如 MCP 无法完成项目上下文检查。

---

# 11. Agent 禁止行为

Agent 不得：

```text
1. 用本簇工具写 Project Marker。
2. 静默修改项目 CLAUDE.md。
3. 期待本簇返回 project_root / settings_path。
4. 期待本簇返回 CLAUDE.md 全文。
5. 期待 check_setup_state 返回 settings 损坏细节。
6. 把 Project Marker 缺失当成已经启用工作流。
7. 在 Project Marker 缺失时继续执行写入型蓝图任务。
8. 期待本簇返回 validation / transaction_id。
```

---

# 12. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 项目是否识别。
2. Project Marker 是否存在。
3. BlueprintHelper workflow 是否启用。
4. setup 是否 ok。
5. blocked reason。
```

不默认报告：

```text
project_root
本地绝对路径
CLAUDE.md 内容
settings.json 内容
transaction_id
```

---

# 13. 验收标准

```text
1. Agent 能解析 read_project_context。
2. Agent 能识别 project_marker missing / invalid。
3. Agent 知道 Project Marker 写入必须用户确认。
4. Agent 能解析 check_setup_state.status=ok。
5. Agent 能处理 setup blocked reason。
6. Agent 不期待本簇返回本地路径。
7. Agent 不期待 validation / transaction_id。
8. Agent 不用本簇工具修复 setup 或 marker。
