# 02 - BlueprintHelper MCP 功能索引

## 1. 总览

BlueprintHelper MCP 的能力应先按任务编排层理解，再按底层能力簇理解。普通 Agent 默认面对少量任务级工具；底层能力入口仍保留，但主要作为 UE Task Runtime 内部 capability、debug / expert 工具和自动化测试入口。实际工具名称以 MCP `tools/list` 返回为准；本索引用于指导 Agent 选择工具类别。

## 2. Agent-facing 任务级工具

默认主线：

```text
Agent -> MCP Task Tools -> Python/MCP Task Compiler -> UE Task Runtime -> Existing Capability Clusters
```

普通 Agent 默认使用：

```text
blueprinthelper_get_runtime_profile
blueprinthelper_diagnostics
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_reference_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_open_editor
blueprinthelper_close_editor
```

职责分层：

| 工具 | 用途 |
|---|---|
| `blueprinthelper_get_runtime_profile` | 读取版本、Bridge、写权限、安全档位和不可用能力 |
| `blueprinthelper_diagnostics` | 读取静态或运行时诊断，不写资产 |
| `blueprinthelper_read_agent_guide` | 返回 AgentGuide 入口索引 |
| `blueprinthelper_read_context` | 按 ReadSpec 返回 ReadContextPack / LogicMD / LogicJson 等只读上下文 |
| `blueprinthelper_read_reference_context` | 返回引用上下文，用于引用查看、preview blocked 解释和高风险修改前影响面分析 |
| `blueprinthelper_preview_task` | 校验 TaskSpec，生成 TaskPlan 摘要，执行 dry_run / preflight |
| `blueprinthelper_execute_task` | 执行已通过 preview 的 TaskSpec 所编译出的 TaskPlan，返回任务级摘要 |
| `blueprinthelper_get_task_result` | 查询 task_run_id 对应的任务结果、验证状态和必要摘要 |
| `blueprinthelper_open_editor` | 启动或打开 Unreal Editor |
| `blueprinthelper_close_editor` | 关闭 Unreal Editor |

TaskSpec 是 Agent-facing 的语义任务规格，普通 Agent 提交 TaskSpec，不提交 TaskPlan。TaskPlan 是 Task Compiler 输出给 UE Task Runtime 的执行计划。TaskRunJournal 是 UE 侧记录一次任务执行和多个 child transaction 的审计容器。

## 3. 连接与编辑器控制

用途：确认 Bridge 可用、启动编辑器、关闭编辑器、执行控制台命令、PIE 启停、Undo / Redo。

典型场景：

- 用户要求“打开项目编辑器”。
- 用户要求“运行 PIE 测一下”。
- 写操作失败后需要 Undo。
- 需要执行控制台命令验证状态。

注意：`open_editor` 和 `build_project` 属于 MCP Server 本地进程能力，不是 UE Bridge 命令。调用它们前必须确认环境变量已配置。

**强制要求：** `UE_ENGINE_DIR` 和 `UE_PROJECT_FILE` 必须使用**绝对路径**（如 `F:/UE_5.6` 和 `G:/UnrealPractise/MrStone/MrStone.uproject`）。不支持相对路径。MCP Server v0.4.0+ 会自动展开 `${workspaceFolder}` 等模板变量，但最终值必须是绝对路径。

## 4. 资产浏览与资产信息

用途：查找、打开、保存、获取资产元信息。普通 Agent 生成 TaskSpec 前，应优先通过 `blueprinthelper_read_context` 获取压缩上下文；底层资产浏览工具用于上下文服务、debug / expert 模式或失败定位。

典型场景：

- 用户只给了蓝图名，没有给完整路径。
- 需要确认目标资产是否存在。
- 写入后保存指定资产。

底层 debug / expert 流程：

```text
search asset -> inspect asset info -> open asset if needed -> perform specific operation -> save asset
```

## 5. 蓝图结构能力

用途：读取和修改 Blueprint 内部结构。普通 Agent 不应直接把复杂编辑拆成多个结构工具调用；应把意图写入 TaskSpec，由 Task Compiler 生成 TaskPlan，再交给 UE Task Runtime 调用这些能力。

包含：

- 列出图表。
- 列出变量。
- 列出事件分发器。
- 添加 / 删除变量。
- 添加 / 删除函数图。
- 添加 / 删除宏图。
- 添加 / 删除事件分发器。
- 添加 / 删除节点。
- 导出 / 导入蓝图 JSON。
- 编译蓝图。

能力要求：图表级操作必须指定资产路径和图表名。作为 TaskPlan step 调用时，这些字段由 Task Compiler 明确展开。

## 6. JSON / Logic 视图能力

用途：在 raw JSON、logic JSON、logic MD 之间选择合适的 Agent 读写格式。

| 格式 | 用途 | 是否建议作为默认读取 |
|---|---|---|
| `logic_md` | 让 Agent 或用户快速理解蓝图逻辑 | 是 |
| `logic_json` | 让 Agent 结构化理解执行流和数据依赖 | 是 |
| `raw_json` | 导入、回放、精确节点/Pin/GUID 操作 | 否，仅精修时用 |

原则：读逻辑用 logic；准备精确写回或兼容导入时才用 raw。

### Bridge 响应格式 (v2.2+)

| 字段 | 类型 | 说明 |
|------|------|------|
| `payload` | object | 结构化 RawJson 对象（主要字段，可直接使用） |
| `json` | object | 兼容性别名，与 payload 内容相同，同为对象 |
| `json_text` | string | 仅在请求 `include_json_text: true` 时出现的序列化字符串（兼容旧消费者） |
| `format` | string | `"raw_json"` |
| `schema` | string | `"BlueprintHelper.JsonToBlueprint.v2.2"` |
| `importable` | boolean | 对于 RawJson 始终为 `true`；对于 LogicJson/LogicMD 始终为 `false` |
| `stats` | object | `{ nodes: N, links: M }` 统计信息 |
| `diagnostics` | array | 诊断消息数组 |

### 旧消费者迁移

- **旧：** `JSON.parse(result.json)`
- **新：** `result.payload ?? result.json`
- **兼容模式：** 请求 `include_json_text: true` 或使用 MCP `legacy_text_json` 模式

## 7. UMG Widget 能力

用途：读取 WidgetTree、添加/删除/移动控件、读写 Widget 属性。普通 Agent 应通过 TaskSpec 表达目标、范围、资源和验证策略；底层 UMG 工具由 Task Runtime 或 debug / expert 模式使用。

典型场景：

- 添加按钮、文本、图片、容器。
- 修改控件文本、可见性、锚点、尺寸、样式属性。
- 调整 WidgetTree 层级。

风险点：UMG 属性路径和控件命名必须明确，批量删除前必须读取树结构。

## 8. UObject / DataAsset 属性能力

用途：读取和写入 UObject / DataAsset 的属性。

典型场景：

- 调整配置 DataAsset。
- 修改 Gameplay 数据对象。
- 批量设置资产属性。

风险点：需要先读取属性类型和当前值，不要把字符串猜成软引用、枚举或结构体。

## 9. DataTable 能力

用途：读取、添加、更新、删除 DataTable 行。

典型场景：

- 新增配置行。
- 修正某个 RowName 的字段。
- 删除错误数据行。

风险点：必须先读取表结构或样例行，确认字段名、字段类型、RowName。

## 10. Function / Event Signature 能力

用途：读取和管理 Blueprint 函数 / Custom Event 的声明与签名层。

当前定位：

- 作为未来 UE Capability Cluster 保留。
- 由 TaskPlan step 调用，不作为普通 Agent 主流程入口。
- 只处理声明、签名和非签名属性，不写函数体 / 事件体逻辑，不接入执行流。

函数体、事件体、执行流仍归属 Graph Write / Replace / Patch / Merge 能力簇。

## 11. 编译、校验与保存

用途：写操作后验证结果。

默认规则：

- TaskSpec 的 validation 字段使用 `validation.should_compile` / `validation.should_save` 描述是否编译、保存；diagnostics 策略后续随 Task Runtime 能力扩展。
- TaskPlan 的 execution_policy 字段使用 `execution_policy.should_compile` / `execution_policy.should_save` 承接编译、保存策略。
- UE Task Runtime 负责按 TaskPlan 执行 compile / diagnostics / save。
- 底层写工具的 validation 只提示 `validation.should_compile` / `validation.should_save`，不代表已完成编译或保存。
- 如果编译失败，报告错误，不要继续叠加写入。
