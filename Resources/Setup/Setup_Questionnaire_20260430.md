# BlueprintHelper Setup Questionnaire

文档版本：2026-04-30

本问卷用于生成 `.blueprinthelper/agent-profile.json`。每个问题都必须能映射到 profile 字段，并影响 Agent 的实际行为。

## 输出位置

```text
.blueprinthelper/agent-profile.json
```

## A. 蓝图与 C++ 职责边界

| ID | 问题 | 选项 | 写入字段 | Agent 行为影响 |
|---|---|---|---|---|
| A1 | 蓝图主要负责哪些逻辑 | `presentation`、`orchestration`、`gameplay_simple`、`gameplay_complex`、`prototype_only` | `development_boundary.blueprint_responsibilities` | 判断需求是否适合用蓝图实现 |
| A2 | C++ 应优先承担哪些逻辑 | `core_systems`、`performance_sensitive`、`networking`、`save_load`、`math_heavy`、`engine_integration` | `development_boundary.cpp_responsibilities` | 判断何时建议转 C++ |
| A3 | 蓝图函数复杂度超过什么阈值时建议迁移 C++ | 节点数、分支数、循环数、跨资产调用数 | `blueprint_style.complexity_thresholds` | 生成重构建议和风险提示 |
| A4 | 允许 Agent 主动建议 C++ 改动吗 | `never`、`suggest_only`、`ask_before_edit`、`allowed_in_repo` | `agent_permissions.cpp_edit_policy` | 控制是否可改源码 |

## B. Agent 修改权限

| ID | 问题 | 选项 | 写入字段 | Agent 行为影响 |
|---|---|---|---|---|
| B1 | Agent 是否可以修改 Blueprint 资产 | `read_only`、`ask_every_time`、`allow_low_risk`、`allow_with_plan` | `agent_permissions.blueprint_mutation` | 控制写操作是否需要确认 |
| B2 | Agent 是否可以修改 UMG Widget | `read_only`、`ask_every_time`、`allow_layout_only`、`allow_properties_and_layout` | `agent_permissions.umg_mutation` | 控制 UMG 工具使用范围 |
| B3 | Agent 是否可以修改 DataAsset | `read_only`、`ask_every_time`、`allow_properties` | `agent_permissions.data_asset_mutation` | 控制 UObject 属性写入 |
| B4 | Agent 是否可以修改 DataTable | `read_only`、`ask_every_time`、`allow_update_only`、`allow_add_update_delete` | `agent_permissions.datatable_mutation` | 控制行新增、更新、删除权限 |
| B5 | Agent 是否可以启动 PIE、关闭编辑器、构建项目 | `ask_every_time`、`allow_pie_only`、`allow_build_only`、`allow_lifecycle_tools` | `agent_permissions.editor_command_policy` | 控制编辑器命令和本地进程工具 |

## C. 蓝图风格阈值

| ID | 问题 | 选项 | 写入字段 | Agent 行为影响 |
|---|---|---|---|---|
| C1 | 单个蓝图函数推荐最大节点数 | 数值，默认 30 | `blueprint_style.complexity_thresholds.max_nodes_per_function` | LogicMD 读图后标记复杂函数 |
| C2 | 单个蓝图函数推荐最大分支数 | 数值，默认 5 | `blueprint_style.complexity_thresholds.max_branches_per_function` | 标记流程复杂度 |
| C3 | 单个函数是否允许 Tick 内复杂逻辑 | `forbid`、`allow_small`、`allow_with_comment` | `blueprint_style.tick_policy` | Tick 相关修改前提示 |
| C4 | Cast 使用偏好 | `avoid`、`allow_when_simple`、`prefer_interface`、`no_preference` | `blueprint_style.cast_policy` | 生成节点计划时选择接口或 Cast |
| C5 | Event Dispatcher 使用偏好 | `prefer_for_ui`、`prefer_for_decoupling`、`avoid_unless_needed` | `blueprint_style.dispatcher_policy` | 控制事件通信建议 |

## D. Logic 读取策略

| ID | 问题 | 选项 | 写入字段 | Agent 行为影响 |
|---|---|---|---|---|
| D1 | 默认读取格式 | `logic_md`、`logic_json`、`raw_json_when_needed` | `logic_export_policy.default_format` | 控制读图首选工具 |
| D2 | 什么时候允许导出 RawJson | `never_without_reason`、`when_importing`、`when_debugging_links`、`always_allowed` | `logic_export_policy.raw_json_policy` | 降低 token 噪音和误用风险 |
| D3 | LogicJson 是否包含节点 ID | `false`、`true_when_planning_edits`、`always` | `logic_export_policy.include_node_ids` | 控制定位节点的细节 |
| D4 | LogicJson 是否包含坐标 | `false`、`true_when_layout_debugging`、`always` | `logic_export_policy.include_positions` | 控制布局调试信息 |

## E. 命名与搜索策略

| ID | 问题 | 选项 | 写入字段 | Agent 行为影响 |
|---|---|---|---|---|
| E1 | 蓝图函数命名风格 | `VerbObject`、`Verb_Object`、`ProjectConvention` | `naming_policy.function_style` | 新增函数命名 |
| E2 | 变量命名风格 | `bPrefixForBool`、`NoBoolPrefix`、`ProjectConvention` | `naming_policy.variable_style` | 新增变量命名 |
| E3 | Event Dispatcher 命名风格 | `OnEventName`、`EventNameChanged`、`ProjectConvention` | `naming_policy.dispatcher_style` | 新增 dispatcher 命名 |
| E4 | 搜索资产时优先使用什么 | `asset_path`、`class_filter_then_name`、`symbol_index_when_available` | `search_policy.asset_search_order` | 控制资产定位流程 |
| E5 | 是否要求 ToolTip、Category、Comment | `required_for_public_api`、`recommended`、`not_required` | `naming_policy.documentation_requirement` | 新增变量/函数时附带说明 |

## F. 写操作确认策略

| ID | 问题 | 选项 | 写入字段 | Agent 行为影响 |
|---|---|---|---|---|
| F1 | 低风险写操作是否需要二次确认 | `always`、`when_batching`、`not_required_after_plan` | `mutation_policy.low_risk_confirmation` | 控制小变更确认 |
| F2 | 高风险写操作是否必须先计划 | `always` | `mutation_policy.high_risk_requires_plan` | 高风险工具必须先计划 |
| F3 | 写后是否自动编译 | `always`、`when_blueprint_changed`、`ask` | `mutation_policy.compile_after_mutation` | 控制编译步骤 |
| F4 | 写后是否自动保存 | `ask`、`save_after_successful_compile`、`never_auto_save` | `mutation_policy.save_policy` | 控制保存步骤 |
| F5 | 严格导入失败时如何处理 | `rollback`、`allow_partial_only_when_user_asks` | `mutation_policy.partial_failure_policy` | 控制 strict 和 allow_partial |

## G. 最小推荐答案

如果用户暂时不想配置细节，可以生成以下保守 profile：

```json
{
  "version": 1,
  "development_boundary": {
    "blueprint_responsibilities": ["presentation", "orchestration", "gameplay_simple"],
    "cpp_responsibilities": ["core_systems", "performance_sensitive", "networking", "save_load"],
    "prefer_cpp_when_threshold_exceeded": true
  },
  "agent_permissions": {
    "cpp_edit_policy": "suggest_only",
    "blueprint_mutation": "allow_with_plan",
    "umg_mutation": "ask_every_time",
    "data_asset_mutation": "ask_every_time",
    "datatable_mutation": "ask_every_time",
    "editor_command_policy": "ask_every_time"
  },
  "blueprint_style": {
    "complexity_thresholds": {
      "max_nodes_per_function": 30,
      "max_branches_per_function": 5,
      "max_loops_per_function": 2,
      "max_cross_asset_calls": 5
    },
    "tick_policy": "allow_small",
    "cast_policy": "prefer_interface",
    "dispatcher_policy": "prefer_for_decoupling"
  },
  "logic_export_policy": {
    "default_format": "logic_md",
    "raw_json_policy": "when_importing",
    "include_node_ids": "true_when_planning_edits",
    "include_positions": "true_when_layout_debugging"
  },
  "naming_policy": {
    "function_style": "VerbObject",
    "variable_style": "ProjectConvention",
    "dispatcher_style": "OnEventName",
    "documentation_requirement": "recommended"
  },
  "search_policy": {
    "asset_search_order": ["asset_path", "class_filter_then_name", "symbol_index_when_available"]
  },
  "mutation_policy": {
    "low_risk_confirmation": "not_required_after_plan",
    "high_risk_requires_plan": true,
    "compile_after_mutation": "when_blueprint_changed",
    "save_policy": "ask",
    "partial_failure_policy": "rollback"
  }
}
```

## Agent Use

When a profile exists, read it before planning any editor-asset mutation. If no profile exists, use the conservative defaults above and state that setup has not been completed.
