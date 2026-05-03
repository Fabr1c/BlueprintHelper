# BlueprintHelper Agent 侧规则：Runtime Profile 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：runtime_profile Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释 runtime_profile，包括正常态极简返回、异常态诊断返回、负向稀疏 unavailable、write_permission、bridge、config_status、risk_command，以及 runtime_profile 与 AgentGuide / tool schema 的边界。UE 字段映射见独立文档。

---

## 1. 工具职责

`get_runtime_profile` 用于判断：

```text
当前 BlueprintHelper UE/MCP 运行链路是否可执行任务。
```

它告诉 Agent：

```text
当前是否正常
是否存在运行时阻断
是否存在写权限问题
是否存在 Bridge / config / risk_command 问题
是否存在不可用能力簇
```

它不告诉 Agent：

```text
完整工具 schema
完整可用工具列表
完整配置文件
命名偏好全文
蓝图/C++边界全文
Skill / AgentGuide 规则全文
Transaction Journal / Review 数据
```

---

## 2. data.schema 短命名

runtime_profile 使用：

```json
"data": {
  "schema": "RuntimeProfile.v1"
}
```

Agent 不应期待：

```text
BlueprintHelper.RuntimeProfile.v1
BlueprintHelper.MCP.RuntimeProfile.v1
BlueprintHelper.Tools.RuntimeProfile.v1
```

---

## 3. 正常态返回

正常态只返回：

```json
{
  "data": {
    "schema": "RuntimeProfile.v1",
    "runtime_profile": {
      "status": "ok"
    }
  }
}
```

Agent 应理解：

```text
当前运行链路正常。
```

正常态不应期待：

```text
write_permission
bridge
config_status
risk_command
unavailable
active_profile
tool_capabilities
version
available tools
naming_preference_summary
blueprint_cpp_boundary_summary
safety_profile
```

---

## 4. 异常态 / 降级态返回

当存在阻断、降级、不可用能力、写权限异常、Bridge 断开、配置不可用等情况时，runtime_profile 才返回细节。

示例：

```json
{
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

Agent 应根据异常细节决定：

```text
stop_and_report
只读降级
请求用户修复 Token / Bridge / setup
调整计划
```

---

## 5. runtime_profile.status

建议 Agent 识别：

```text
ok
degraded
blocked
```

含义：

| status | Agent 解释 |
|---|---|
| `ok` | 当前运行链路正常。 |
| `degraded` | 部分能力不可用，但可能仍可完成只读或部分任务。 |
| `blocked` | 当前任务相关能力可能无法继续，需根据 unavailable / reason 判断。 |

具体原因不要从 status 猜测，应读取异常字段。

---

## 6. unavailable 负向稀疏规则

runtime_profile 不列可用能力。

只在异常态返回：

```json
"unavailable": [
  {
    "cluster": "graph_write",
    "capability": "write",
    "status": "blocked",
    "reason": "write_permission_disabled"
  }
]
```

Agent 规则：

```text
1. unavailable 中列出的能力不可直接使用。
2. 未列出的能力不等于 tool schema 已确认。
3. runtime_profile 不是工具索引。
4. 具体 tool 参数仍以 MCP tool schema / tools 文档为准。
```

---

## 7. unavailable item 字段

每个 unavailable item 只包含：

```text
cluster
capability
status
reason
```

Agent 不应期待：

```text
severity
stop_and_report
message
required_tool
```

是否 stop_and_report 由 Agent 根据：

```text
当前任务需求
missing_capability_policy
unavailable status/reason
是否存在安全替代路径
```

自行判断。

---

## 8. write_permission

write_permission 只在异常 / 降级 / 阻断时返回。

示例：

```json
"write_permission": {
  "enabled": false,
  "reason": "token_missing"
}
```

Agent 规则：

```text
1. enabled=false 时不得执行写工具。
2. token_missing / token_invalid / token_expired 是安全状态，不是工具系统异常。
3. 用户补齐或刷新 Token 后，Agent 应重新读取 runtime_profile。
```

---

## 9. bridge

bridge 只在异常时返回。

示例：

```json
"bridge": {
  "status": "disconnected"
}
```

Agent 规则：

```text
bridge disconnected 时，禁止写入。
只读工具是否可用取决于具体 unavailable 项和工具状态。
```

---

## 10. config_status

config_status 只在异常时返回。

示例：

```json
"config_status": {
  "status": "config_unavailable"
}
```

Agent 规则：

```text
config_unavailable 时，write_permission 通常 disabled。
Agent 应 stop_and_report，并提示需要运行 setup 或修复配置。
```

---

## 11. risk_command

risk_command 只在异常、禁用或任务涉及风险命令时返回。

示例：

```json
"risk_command": {
  "enabled": false,
  "reason": "risk_command_missing",
  "blocked_commands": [
    "close_editor"
  ]
}
```

Agent 规则：

```text
risk_command 缺失只阻断对应 lifecycle / risk command。
普通蓝图读写不应因此阻断，除非 unavailable 明确影响当前任务。
```

---

## 12. runtime_profile 与 AgentGuide 的边界

Agent 不应从 runtime_profile 学习：

```text
命名规则
蓝图/C++边界
工具调用参数
工具 schema
完整 workflow
```

这些来自：

```text
CLAUDE.md
Skill
AgentGuide
tools/*.md
MCP tool schema
```

runtime_profile 只回答：

```text
当前运行时是否可用
当前是否存在权限 / Bridge / 配置 / 能力阻断
```

---

## 13. 读取时机

Agent 应在以下场景读取 runtime_profile：

```text
1. 每个任务的写入阶段开始前。
2. 新 UE 项目工作会话开始时。
3. Bridge / Token / 权限 / Safety Profile 可能变化后。
4. 工具返回权限或运行时异常后。
5. 用户要求诊断当前运行状态时。
```

不需要：

```text
每个写工具调用前重复读取。
```

连续写工具调用期间可沿用本阶段 runtime profile，除非出现运行时变化或工具异常。

---

## 14. 正常态处理

当 runtime_profile 返回：

```text
status=ok
```

Agent 不应向用户展开 runtime_profile 内容。

正常流程继续执行。

---

## 15. 异常态处理

当 runtime_profile 返回：

```text
status=degraded
status=blocked
```

Agent 应判断是否影响当前任务。

如果影响当前任务：

```text
stop_and_report
```

如果不影响当前任务：

```text
继续可安全执行的只读或不受影响步骤
```

---

## 16. 工具自身失败

如果 runtime_profile 工具自身失败：

```json
{
  "ok": false,
  "status": "failed",
  "error": {
    "code": "runtime_profile_unavailable"
  }
}
```

Agent 应将其视为运行时事实不可确认。

写任务中应 stop_and_report。

---

## 17. Agent 禁止行为

Agent 不得：

```text
1. 期待正常 runtime_profile 返回完整配置。
2. 期待正常 runtime_profile 返回 tool_capabilities。
3. 把 runtime_profile 当成 MCP tool schema。
4. 把 unavailable 未列出理解为工具参数已确认。
5. 在 write_permission.enabled=false 时执行写工具。
6. 在 bridge disconnected 时执行写工具。
7. 在 config_unavailable 时使用内置默认配置继续写。
8. 将 risk_command 缺失误判为所有蓝图读写不可用。
```

---

## 18. 验收标准

```text
1. Agent 能解析 runtime_profile.status=ok。
2. Agent 知道正常态没有细节字段。
3. Agent 能解析 degraded / blocked 异常字段。
4. Agent 能处理 unavailable 负向稀疏列表。
5. Agent 不把 runtime_profile 当工具 schema。
6. Agent 不期待 active_profile / tool_capabilities 正常返回。
7. Agent 能根据 write_permission / bridge / config_status / risk_command 做 stop_and_report 或降级。
