# BlueprintHelper Agent 侧规则：Diagnostics / Static & Runtime Diagnostics 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：Diagnostics Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 `/blueprinthelper-diagnostics` 与 `/blueprinthelper-diagnostics --runtime` 对应的诊断字段，包括 Markdown 报告、工具成功/诊断阻断区分、用户可见边界、无 validation / transaction 返回规则。UE 字段映射见独立文档。

---

## 1. 命令入口与 operation

命令入口保留：

```text
/blueprinthelper-diagnostics
/blueprinthelper-diagnostics --runtime
```

MCP Agent-facing 字段协议可统一为：

```text
operation=run_blueprinthelper_diagnostics
```

并通过：

```text
target.diagnostics_mode=static
target.diagnostics_mode=runtime
```

区分模式。

---

## 2. 工具职责

Diagnostics 用于：

```text
安装状态诊断
配置状态诊断
Bridge / MCP / runtime 链路诊断
write_permission / risk_command 状态诊断
Project Marker 只读检查
```

Diagnostics 不用于：

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

## 3. 通用返回规则

Diagnostics 是只读工具。

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
```

所有 `data.schema` 使用短命名：

```text
Diagnostics.v1
```

---

## 4. 报告格式

Diagnostics 报告只返回：

```text
data.markdown
```

不返回：

```text
data.blocking[]
data.warning[]
data.info[]
```

Markdown 固定分组：

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
5. code 使用 <domain>.<state>。
6. 不输出 Suggested action。
7. 不输出 action code。
8. 不输出本地绝对路径。
```

---

# 5. 工具成功与诊断阻断的区别

如果诊断命令执行成功，即使 Markdown 中有 Blocking，也返回：

```text
ok=true
status=completed
```

例如：

```md
## Blocking
- `bridge.disconnected`

## Warning
None
```

这表示：

```text
诊断工具执行成功。
诊断报告发现 bridge.disconnected 阻断。
```

只有诊断工具自身失败时才返回：

```text
ok=false
status=failed
error
```

---

# 6. static diagnostics

`diagnostics_mode=static` 对应：

```text
/blueprinthelper-diagnostics
```

它用于安装与配置静态诊断。

可能报告：

```text
version.match
version.mismatch
settings.valid
settings.invalid
global_guidance.present
skill_entry.valid
project_marker.present
project_marker.missing
```

Agent 不应期待：

```text
settings.json 字段级错误
settings.json 内容
本地 settings 路径
自动修复建议
```

---

# 7. runtime diagnostics

`diagnostics_mode=runtime` 对应：

```text
/blueprinthelper-diagnostics --runtime
```

它用于运行链路诊断。

可能报告：

```text
ue_editor.running
mcp_server.available
bridge.connected
bridge.disconnected
runtime_profile.available
config_status.valid
config_status.unavailable
write_permission.enabled
write_permission.disabled
risk_command.enabled
risk_command.disabled
```

---

## 7.1 write_permission.disabled

例如：

```md
## Warning
- `write_permission.disabled`
  - reason: `token_missing`
```

Agent 应理解：

```text
写操作不可用。
只读诊断工具仍成功。
Token 缺失是安全状态，不是工具系统异常。
```

---

## 7.2 risk_command.disabled

例如：

```md
## Warning
- `risk_command.disabled`
  - reason: `risk_command_missing`
  - blocked_commands: `close_editor`
```

Agent 应理解：

```text
risk_command_missing 只阻断 close_editor 等风险命令。
普通蓝图读写不因此阻断。
```

---

## 7.3 bridge.disconnected

例如：

```md
## Blocking
- `bridge.disconnected`
```

Agent 应理解：

```text
Bridge 断开会阻断需要 UE Bridge 的操作。
如果当前任务需要写入或读取 UE 运行时资产，应 stop_and_report。
```

---

# 8. 用户可见边界

Diagnostics 是 Agent-facing 诊断报告。

Agent 不应默认把完整 Markdown 展开给用户。

只有以下情况可以转述关键 code：

```text
用户明确要求诊断结果
当前任务发生 stop_and_report 需要解释阻断来源
用户要求排查 BlueprintHelper 环境
```

转述时应简短说明关键 code 和影响，不输出完整本地路径或配置内容。

---

# 9. 与 runtime_profile 的关系

Agent 应区分：

```text
runtime_profile：
- 当前任务运行时事实。
- 写任务前读取。
- 正常态只返回 runtime_profile.status=ok。
- 异常态才返回 write_permission / bridge / config_status / unavailable。

diagnostics：
- 诊断报告。
- 总是 Markdown。
- 用于环境解释、排错、stop_and_report 说明。
```

普通写任务前优先读取 runtime_profile，不默认跑 diagnostics。

---

# 10. 与 setup / project context 的关系

Agent 应区分：

```text
check_setup_state：
- 只判断 setup_state.status=ok 或 blocked reason。

check_project_marker：
- 只判断 marker 状态。

diagnostics：
- 输出更完整诊断 code。
```

Diagnostics 不修复 setup，不写 marker。

---

# 11. Agent 禁止行为

Agent 不得：

```text
1. 期待 diagnostics 返回 blocking/warning/info JSON 数组。
2. 把 Markdown 中的 Blocking 当成工具调用失败。
3. 期待 diagnostics 返回 validation。
4. 期待 diagnostics 返回 transaction_id。
5. 期待 diagnostics 输出 Suggested action。
6. 期待 diagnostics 输出本地绝对路径。
7. 把 risk_command_missing 判定为普通蓝图读写不可用。
8. 把 diagnostics 当成 runtime_profile 替代品。
9. 默认向用户展开完整 diagnostics Markdown。
```

---

# 12. 最终报告规则

当用户明确要求诊断，或 stop_and_report 需要解释阻断时，Agent 可报告：

```text
1. 关键 Blocking code。
2. 关键 Warning code。
3. 对当前任务的影响。
4. 最小必要修复方向。
```

不默认报告：

```text
完整 Markdown
本地路径
settings.json 内容
CLAUDE.md 内容
Suggested action 字段
```

---

# 13. 验收标准

```text
1. Agent 能用 diagnostics_mode 区分 static / runtime。
2. Agent 只读取 data.markdown。
3. Agent 不期待 JSON arrays。
4. Agent 能区分工具成功和诊断报告中的 Blocking。
5. Agent 知道 risk_command_missing 只影响 close_editor 等风险命令。
6. Agent 不把 diagnostics 当 runtime_profile。
7. Agent 不默认向用户展开完整诊断报告。
