# BlueprintHelper Agent 侧规则：Editor Lifecycle / PIE / Risk Command 使用规范

日期：2026-05-03  
适用范围：Claude Code / Agent Skill / BlueprintHelper AgentGuide  
状态：Editor Lifecycle / PIE / Risk Command Agent 侧规则确认稿  
本文边界：规定 Agent 如何调用和解释编辑器生命周期、PIE 启停、关闭编辑器风险命令工具，包括 risk_command 授权、dry_run、no_op、失败诊断、无 validation / transaction 返回规则。UE 字段映射见独立文档。

---

## 1. 工具簇边界

第一版包含：

```text
get_editor_lifecycle_status
start_pie_session
stop_pie_session
close_editor
```

第一版不包含：

```text
切换地图
重启 UE 编辑器
运行命令行 Cook / Build / Package
修改 Editor Preferences
修改 Project Settings
```

---

## 2. 通用规则

本簇工具不是资产写工具。

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

所有 `data.schema` 使用短命名。

---

# 3. get_editor_lifecycle_status

## 3.1 职责

`get_editor_lifecycle_status` 读取当前 UE 编辑器生命周期状态。

返回：

```text
editor_running
pie_running
unsaved_asset_count
```

示例：

```json
{
  "data": {
    "schema": "EditorLifecycleStatus.v1",
    "editor_lifecycle": {
      "editor_running": true,
      "pie_running": false,
      "unsaved_asset_count": 2
    }
  }
}
```

---

## 3.2 Agent 解释

Agent 可用该工具判断：

```text
UE 编辑器是否运行
PIE 是否运行
是否有未保存资产
```

它不是 runtime_profile 的替代品，也不返回权限 / Bridge / tool capability 状态。

---

# 4. start_pie_session

## 4.1 职责

`start_pie_session` 启动 Play In Editor。

它不负责：

```text
compile
save
修复编译错误
切换地图
修改 PIE 设置
```

如果前序写工具返回：

```json
"validation": {
  "should_compile": true,
  "should_save": true
}
```

Agent 应先按需调用：

```text
compile_blueprint_asset
save_asset
```

再启动 PIE。

---

## 4.2 成功返回

```json
{
  "data": {
    "schema": "StartPieSession.v1",
    "pie_result": {
      "started": true,
      "already_running": false
    }
  }
}
```

no_op：

```json
{
  "status": "no_op",
  "data": {
    "pie_result": {
      "started": false,
      "already_running": true
    }
  }
}
```

---

## 4.3 启动失败

如果编译错误阻断 PIE：

```json
{
  "error": {
    "code": "pie_start_failed",
    "stage": "start_pie",
    "conflicts": [
      {
        "code": "blueprint_compile_errors_exist",
        "message": "One or more Blueprint assets still have compile errors."
      }
    ]
  }
}
```

Agent 应回到 compile 结果和错误修复流程，不应把 PIE 启动失败误判为 Graph Write 成功。

---

# 5. stop_pie_session

## 5.1 职责

`stop_pie_session` 停止当前 PIE session。

---

## 5.2 成功返回

```json
{
  "data": {
    "schema": "StopPieSession.v1",
    "pie_result": {
      "stopped": true,
      "was_running": true
    }
  }
}
```

---

## 5.3 no_op

如果 PIE 未运行：

```json
{
  "status": "no_op",
  "data": {
    "pie_result": {
      "stopped": false,
      "was_running": false,
      "reason": "pie_not_running"
    }
  }
}
```

Agent 不应把该 no_op 当成失败。

---

# 6. close_editor

## 6.1 职责

`close_editor` 请求关闭 Unreal Editor。

这是风险命令。

Agent 只能在用户明确要求时调用。

---

## 6.2 必要条件

Agent 调用 close_editor 前必须确认：

```text
用户明确要求关闭编辑器
runtime_profile 未阻断 close_editor
risk_command 已启用并授权 close_editor
dry_run passed
```

如果存在未保存资产，必须按用户或工具策略处理。

---

## 6.3 必须 dry_run

close_editor 必须 dry_run。

dry_run passed：

```json
{
  "dry_run": {
    "result": "passed",
    "can_execute": true
  }
}
```

dry_run blocked：

```json
{
  "dry_run": {
    "result": "blocked",
    "can_execute": false,
    "blocked_by": [
      "risk_command_missing"
    ],
    "conflicts": [
      {
        "code": "risk_command_missing",
        "command": "close_editor",
        "message": "close_editor requires risk_command authorization."
      }
    ],
    "errors": []
  }
}
```

Agent 不得绕过 blocked 状态。

---

## 6.4 成功返回

```json
{
  "data": {
    "schema": "CloseEditor.v1",
    "close_result": {
      "close_requested": true
    }
  }
}
```

no_op：

```json
{
  "status": "no_op",
  "data": {
    "close_result": {
      "close_requested": false,
      "already_closing": true
    }
  }
}
```

---

## 6.5 Agent 禁止自动关闭编辑器

Agent 不得在以下场景自动调用 close_editor：

```text
普通蓝图任务完成后
编译/保存完成后
PIE 结束后
为了刷新配置
为了减少资源占用
```

只有用户明确要求关闭编辑器时才允许进入 close_editor 流程。

---

# 7. runtime_profile 与 risk_command 的关系

runtime_profile 异常态可能返回：

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
risk_command_missing 只阻断对应风险命令。
普通蓝图读写不因此阻断。
close_editor 必须受 risk_command 保护。
```

---

# 8. no_op 规则

本簇 no_op 不是失败。

典型 no_op：

```text
start_pie_session：PIE 已运行。
stop_pie_session：PIE 未运行。
close_editor：编辑器已在关闭流程。
```

Agent 应按状态解释，不应重试循环。

---

# 9. error 处理

常见错误：

```text
pie_start_failed
pie_stop_failed
risk_command_missing
risk_command_invalid
command_not_authorized
unsaved_assets_exist
editor_close_failed
```

Agent 根据 error.retryable / conflicts 判断是否重试、提示用户、或 stop_and_report。

---

# 10. Agent 禁止行为

Agent 不得：

```text
1. 把 lifecycle 工具当成资产写工具。
2. 期待 validation。
3. 期待 transaction_id。
4. 用 start_pie_session 替代 compile/save。
5. 在 PIE 编译错误阻断后继续启动 PIE。
6. 自动调用 close_editor。
7. 在 close_editor dry_run blocked 后正式关闭。
8. 把 risk_command_missing 误判为普通蓝图读写不可用。
```

---

# 11. 最终报告规则

正常完成时，Agent 可报告：

```text
1. 编辑器是否运行。
2. PIE 是否启动或停止。
3. close_editor 是否已请求。
4. 被阻断时的原因，如 risk_command_missing 或 unsaved_assets_exist。
```

不默认报告：

```text
transaction_id
review_status
validation
```

---

# 12. 验收标准

```text
1. Agent 能读取 editor lifecycle 状态。
2. Agent 能正确处理 start_pie no_op。
3. Agent 能正确处理 stop_pie no_op。
4. Agent 知道 start_pie 不负责 compile/save。
5. Agent 知道 close_editor 是风险命令。
6. Agent 只在用户明确要求时调用 close_editor。
7. Agent 必须对 close_editor 执行 dry_run。
8. Agent 不期待本簇返回 validation / transaction。
