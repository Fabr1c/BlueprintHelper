# 09 - SideAgent Tool Execution

本文件只给负责 BlueprintHelper 工具调用的 SideAgent 使用。主 Agent 负责面向用户沟通，SideAgent 负责构造参数、调用工具、翻译结果并回交。

## 输入契约

SideAgent 必须从主 Agent 接收一个精简任务包，而不是完整对话或完整 `SKILL.md`。

任务包应包含：

- `user_goal`: 用户目标
- `target_asset_path`: 目标 UE 资产路径；未知时不得写入
- `target_graph`: 目标图表；图表写入任务需要
- `operation_mode`: `create_new` 或 `modify_existing`
- `safety_profile`: runtime_profile 中的当前安全档位
- `allowed_tools`: 允许调用的 BlueprintHelper 工具名
- `stop_conditions`: 必须停止并回交的条件
- `return_format`: 主 Agent 要求的结果摘要格式

如果任务包缺少目标资产、目标图表或创建/修改策略，返回 `clarification_required`，不要调用写入工具。

## 执行规则

1. 读取本文件和必要字段模板：`04_MCP_Field_Templates_20260507.md`。
2. 按需要读取任务 workflow，例如 `04_TaskSpec_Edit_Blueprint_Workflow.md`。
3. 使用 TaskSpec-first 工具链，不直接调用冻结或 legacy 底层入口。
4. 工具参数必须是 MCP schema root object，不要额外包 `args`。
5. `blueprinthelper_preview_task` 和 `blueprinthelper_execute_task` 必须把 TaskSpec 包在 `task_spec` 字段下。
6. 如果 `write_permission` 被禁用，只在 preview 成功后请求 write session。
7. SideAgent 不向用户回复，不请求用户确认；需要用户确认时把原因交回主 Agent。

## 允许的普通工具链

```text
blueprint_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_task_context
blueprinthelper_read_reference_context
blueprinthelper_preview_task
blueprinthelper_request_write_session
blueprinthelper_execute_task
blueprinthelper_get_task_result
```

`blueprint_open_editor` 只用于主 Agent 明确要求启动目标 Editor 的 preflight。调用时必须传显式 `project_file`。

## 停止条件

遇到以下情况立即停止并回交：

- `clarification_required`: 任务包缺少目标资产、目标图表或创建/修改策略
- `bridge_unavailable`: Bridge 不可达
- `profile_blocked`: runtime_profile 不允许写入
- `preview_blocked`: preview 返回阻断
- `capability_missing`: TaskSpec 或工具能力缺失
- `write_rejected`: write session 被拒绝
- `tool_failed`: 工具返回失败，且没有明确可安全修复的参数错误

不要自行扩大工具范围。不要把 frozen/legacy 工具当作恢复路径，除非主 Agent 的任务包明确授权。

## 结果翻译格式

SideAgent 回交给主 Agent 时使用中文摘要，并保留关键结构：

```yaml
status: success | clarification_required | blocked | failed
tool_calls:
  - tool: "<tool name>"
    key_args: "<only important arguments, no huge payload>"
    result: "<success/block/failure summary>"
asset_paths:
  - "/Game/..."
validation:
  preview: "<passed/blocked/not_run>"
  execute: "<passed/failed/not_run>"
  compile: "<passed/failed/not_requested/not_run>"
blocking_reason: "<none or translated reason>"
next_step: "<what main Agent should do next>"
```

不要回传大段 raw result、DebugBundle artifact 内容、token、settings 全文或本地 bundle 路径。需要证据时只回传摘要和稳定 id。

## 场景验收：可开关物理门

用户说：“在蓝图实现一个可以开关的物理门。”

如果主 Agent 没有给出目标资产或创建策略，SideAgent 应回交：

```yaml
status: clarification_required
blocking_reason: "缺少目标资产和创建/修改策略"
next_step: "请主 Agent 询问用户：修改已有门蓝图，还是创建新的 BP_PhysicsDoor？"
```

如果主 Agent 给出创建新资产任务包，SideAgent 才继续构造 TaskSpec。TaskSpec 应表达可验证目标，例如：

- 创建或修改 `BP_PhysicsDoor`
- 添加可配置 `bIsOpen`、`OpenAngle`、`OpenSpeed`
- 添加交互触发入口
- 实现 open/close 切换逻辑
- 使用 preview 验证再 execute

SideAgent 最终只把工具调用结果翻译给主 Agent，由主 Agent 向用户汇报。
