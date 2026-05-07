# 02 - Agent-facing Capability Index

本页只列普通 Agent 可见工具。底层 capability 和兼容工具保留在 MCP 注册表中，但不作为普通 Agent 调用面。

## Allowed Tools

| Tool | Purpose |
|---|---|
| `blueprinthelper_read_agent_guide` | 读取本指南入口 |
| `blueprint_get_runtime_profile` | 读取 Bridge、写权限、安全档位和能力状态 |
| `blueprinthelper_diagnostics` | 静态安装和配置诊断 |
| `blueprinthelper_diagnostics_runtime` | Editor/Bridge 可达时的运行时诊断 |
| `blueprinthelper_read_context` | 按 ReadSpec 读取压缩上下文 |
| `blueprinthelper_read_task_context` | 读取构造 TaskSpec 所需上下文 |
| `blueprinthelper_read_reference_context` | 高风险修改前读取引用和影响面 |
| `blueprinthelper_preview_task` | 校验 TaskSpec 并 dry-run |
| `blueprinthelper_execute_task` | preview 通过后执行 TaskSpec |
| `blueprinthelper_get_task_result` | 查询任务结果 |

`blueprint_open_editor` 只在用户明确要求启动目标 Editor 或 preflight 发现 Editor 未启动时使用。

## TaskSpec Capabilities

普通 Agent 通过 `task_type` 和 `behavior` 表达能力:

| Task type | Use |
|---|---|
| `create_asset` | 创建或复用受支持资产 fixture |
| `edit_blueprint_graph` | 修改 BlueprintHelper-owned 图表逻辑 |
| `edit_blueprint_variables` | 变量、默认值和局部变量 |
| `edit_blueprint_components` | 组件树变更 |
| `edit_blueprint_class_settings` | 接口和 class default |
| `edit_umg_widget` | Widget tree 和属性变更 |
| `edit_data_table` | DataTable 行变更 |
| `edit_object_properties` | UObject 属性变更 |
| `create_blueprint_feature` | 组合式 Blueprint 功能 |
| `manage_blueprinthelper_ownership` | BlueprintHelper-owned block 生命周期 |

如果 TaskSpec preview 返回 missing capability，按结果报告缺口，不改用冻结入口。
