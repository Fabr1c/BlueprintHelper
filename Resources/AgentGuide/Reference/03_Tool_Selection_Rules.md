# 03 - 工具选择规则

## 1. 不要先猜工具名

Agent 应先判断任务是否可以表达为 TaskSpec。普通编辑任务默认走任务级工具，不再把复杂目标拆成大量底层原子 MCP 调用。底层工具名称可能随版本变化，工具描述是 debug / expert 模式和 Task Runtime capability 映射的最终依据。

默认主线：

```text
get_runtime_profile
 -> read_context / read_reference_context as needed
 -> produce TaskSpec
 -> preview_task
 -> execute_task
 -> get_task_result when needed
```

## 2. 用户意图映射表

| 用户意图 | 应选能力 | 不应选能力 |
|---|---|---|
| “查看这个蓝图做什么” | 资产搜索 + 逻辑导出 | 直接 raw JSON 全量读取 |
| “给蓝图加一个变量” | TaskSpec + preview_task + execute_task | 直接调用变量底层工具作为普通主线 |
| “给 EventGraph 加节点” | TaskSpec 描述目标图表、范围、锚点和验证 | 当前焦点图表写入 |
| “把 Widget 里按钮文案改掉” | TaskSpec；必要时 read_context 包含 WidgetTree 摘要 | 蓝图节点导入 |
| “改 DataTable 这一行” | TaskSpec；必要时读取表结构作为上下文 | 资产源码文件编辑 |
| “写一个 C++ 类” | 普通代码工具 | BlueprintHelper MCP |
| “编译整个项目” | MCP Server build_project 或普通构建命令 | 蓝图编译工具 |
| “打开 Unreal Editor” | open_editor | 资产浏览工具 |
| “运行 PIE” | 编辑器 PIE 命令 | 项目编译工具 |

## 3. 读格式选择

```text
Need understand?        -> logic_md
Need structured summary? -> logic_json
Need exact nodes/pins?   -> raw_json
Need import/replay?      -> raw_json-compatible JsonToBlueprint protocol
```

## 4. 写格式选择

普通 Agent 优先提交 TaskSpec，不提交 TaskPlan。TaskSpec 描述目标资产、允许修改范围、资源引用、组件/变量/行为/集成策略、失败策略和验证策略。架构主线是 Agent -> MCP Task Tools -> Python/MCP Task Compiler -> UE Task Runtime -> Existing Capability Clusters。Python / MCP Task Compiler 负责把 TaskSpec 编译为 TaskPlan，UE Task Runtime 负责调用底层能力簇。

当前 smoke-verified 写入边界：GraphWrite 只确认 `append_new_owned_graph + 新图名` 可执行，变量簇确认 `edit_blueprint_variables` 可执行。Replace/Patch/Merge、Component、Composite 等路径可能已有 TaskSpec/TaskPlan 合同，但必须以 preview 结果为准；preview blocked 时停止报告，不回退到底层原子写工具。

底层能力入口只在以下场景作为直接入口：

- debug / expert 模式。
- 自动化测试。
- 失败定位。
- Task Runtime 内部 capability 映射。
- 用户明确要求调用某个底层工具。

只有在以下情况使用 JSON 导入：

- 用户明确要求导入一段蓝图 JSON。
- 需要批量创建结构，底层工具调用成本过高。
- 已有可兼容的 JsonToBlueprint raw JSON。

不要用 `logic_json` 或 `logic_md` 直接导入。

## 5. 当前焦点上下文规则

允许使用当前激活图表 / 当前打开资产的情况：

- 用户明确说“当前打开的蓝图”或“当前图表”。
- 操作是只读查询。
- 操作失败不会破坏资产。

不允许依赖当前焦点的情况：

- 删除节点 / 变量 / 控件。
- 重命名。
- 批量写入。
- 导入 JSON。
- 保存资产。

## 6. 错误处理规则

- Bridge 不可用：报告编辑器或 Bridge 状态问题，提示需要启动编辑器或检查端口。
- TaskSpec schema / semantic 错误：根据 `error.issues[].path` 和 `suggested_patches` 修正 TaskSpec。
- preview blocked：不得继续 execute；报告 blocked_by / conflicts 或调整 TaskSpec。
- context_required：重新调用 `blueprinthelper_read_context` 或相关只读工具后再生成 TaskSpec。
- 资产找不到：先通过上下文或资产搜索消歧，不要凭空创建同名资产，除非 TaskSpec 明确允许创建。
- 图表找不到：在 TaskSpec 中修正目标图表或让用户确认；不要自动新建同名图表，除非 scope_policy / asset_policy 明确允许。
- 编译失败：停止继续写入，返回错误摘要和可能原因。
- 底层 Bridge / UE operation error：默认由 Python / MCP error normalizer 转换为任务级错误；普通 Agent 不直接消费原始 Bridge Error。
