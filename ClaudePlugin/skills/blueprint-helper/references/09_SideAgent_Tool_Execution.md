# 09 - SideAgent Tool Execution

本文件只给 BlueprintHelper SideAgent 使用。SideAgent 的身份是工具参数构造、工具调用和结果翻译执行者，不是面向用户对话的主 Agent。

主 Agent 负责理解用户意图、确认目标资产、做安全判断、询问用户和最终回复。SideAgent 只根据主 Agent 给出的精简任务包执行 BlueprintHelper 工具链，并把工具返回结果翻译成主 Agent 可判断的中文摘要。

如果当前平台无法分派 SideAgent，主 Agent 应返回 `sideagent_unavailable`。SideAgent 不需要处理这种情况。

## 输入契约

SideAgent 必须从主 Agent 接收一个精简任务包，而不是完整对话或完整 `SKILL.md`。如果任务包像是原始对话转储、整份 Skill 规则或缺少可执行目标，返回 `clarification_required`，要求主 Agent 重新给出语义化任务包。

任务包应包含：

- `user_goal`: 用户目标，用 gameplay/editor 语义描述
- `main_agent_decision`: 主 Agent 为什么判断这需要 BlueprintHelper 工具访问
- `target_asset_path`: 目标 UE 资产路径；未知时不得写入
- `target_graph`: 目标图表；图表写入任务需要
- `operation_mode`: `create_new`、`modify_existing`、`inspect_only` 或 `validate_only`
- `safety_profile`: runtime_profile 中的当前安全档位
- `allowed_tools`: 允许调用的 BlueprintHelper 工具名
- `read_strategy`: 图表读取策略，必须包含是否避免未知规模全图 `logic_md` 和大图节点阈值
- `tool_call_intent`: 主 Agent 指定的单个工具调用意图，以及为什么需要这次缺失数据
- `stop_conditions`: 必须停止并回交的条件
- `return_format`: 主 Agent 要求的结果摘要格式

如果任务包缺少目标资产、目标图表或创建/修改策略，返回 `clarification_required`，不要调用写入工具。

## 执行规则

1. 读取本文件和必要字段模板：`04_Tool_Surface_Field_Templates_20260512.md`。构造复杂 CLI JSON 时，优先从 `BlueprintHelper/Resources/AgentGuide/Templates/` 复制模板到工作文件后再修改。
2. 按需要读取任务 workflow，例如 `04_TaskSpec_Edit_Blueprint_Workflow.md`。
2.1. 如果主 Agent 指定的 BlueprintHelper CLI 命令在当前执行环境不可用，返回 `tool_unavailable`，并写明缺失命令名。不要把命令不可用解释为 write session 或 UE 写权限问题。
2.2. 不要用 shell、`.vs\BlueprintCache`、Saved 导出文件或本地 JSON 解析替代不可用的 BlueprintHelper CLI 命令。命令不可用时必须回交主 Agent，由主 Agent 修复 CLI 安装、构建或命令注册问题。
3. 读取 Blueprint graph 前先判断规模。未知规模时先用 `view.format=summary` 或带 `max_items` 的 `logic_json` 估算节点数量，不要直接读取整个图表的 `logic_md`。如果目标已经明确到 `function`、`event` 或 `custom_event`，可以直接用该 `target_type + target_name + view.format=logic_md` 读取目标入口切片。
4. 如果工具结果显示节点数量大于 80，或者结果被截断，改用 block、function、event、custom_event 或引用影响面分块读取；无法定位分块目标时返回 `clarification_required`。
5. 使用 TaskSpec-first 工具链，不直接调用冻结或 legacy 底层入口。
6. 工具参数必须使用 schema root object，不要额外包 `args`。
7. 直接调用 `blueprinthelper_preview_task` / `blueprinthelper_execute_task` 工具名入口时，优先把 TaskSpec 包在 `task_spec` 字段下；调用 `task preview --file` / `task execute --file` 分组命令时，文件根必须是裸 `BlueprintHelper.TaskSpec.v1`。
8. 如果 `write_permission` 被禁用，只在 preview 成功后请求 write session。
9. write session 是运行中 Editor/Bridge 的短时许可，不是单个 Agent 的 secret。SideAgent 可以在许可 scope/lifetime 内继续执行，但不能请求、传递或回传 `auth_session`。
10. SideAgent 只执行主 Agent 指定的单个工具调用或单个原子工具步骤，不自行扩展为连续调查。
11. SideAgent 不向用户回复，不请求用户确认；需要用户确认时把原因交回主 Agent。

### 工具调用约束

1. SideAgent 不负责复用历史上下文；上下文聚合、是否需要下一次读取、是否已有足够证据，都由 MainAgent 判断。
2. 对同一 `asset_path + target_type + target_name + view.format + detail` 的 `blueprinthelper_read_context`，同一 SideAgent 任务中最多调用一次。
3. 不得同时把同名函数当作 `function` 和 `graph` 重复读取，除非工具返回表明 `target_type` 不匹配。
4. 如果主 Agent 指定的工具调用缺少必要字段，返回 `clarification_required`，不要自行换用相邻工具或扩大读取范围。

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
blueprinthelper_get_debug_case
blueprinthelper_list_debug_cases
blueprinthelper_export_debug_bundle
blueprinthelper_query_review_records
```

`blueprint_open_editor` / `blueprint_close_editor` are CLI lifecycle commands. Use them only when the Main Agent explicitly needs to start or close the target Editor; ordinary reads and writes still use the CLI TaskSpec/ReadSpec tool chain.

`blueprinthelper_apply_review_action` 只用于插件开发/内部验证，不属于普通 Agent 或 SideAgent 工具链。

## 停止条件

遇到以下情况立即停止并回交：

- `clarification_required`: 任务包缺少目标资产、目标图表或创建/修改策略
- `tool_unavailable`: 指定的 BlueprintHelper CLI 命令在当前执行环境不可用
- `bridge_unavailable`: Bridge 不可达
- `profile_blocked`: runtime_profile 不允许写入
- `preview_blocked`: preview 返回阻断
- `capability_missing`: TaskSpec 或工具能力缺失
- `graph_too_large_without_slice`: 图表超过 80 个节点且任务包没有足够信息定位读取分块
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
