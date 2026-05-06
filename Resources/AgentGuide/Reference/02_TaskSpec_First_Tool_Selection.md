# 02 - TaskSpec-first 工具选择

普通 Agent 默认工具链：

```text
blueprinthelper_get_runtime_profile
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_reference_context
blueprinthelper_preview_task
blueprinthelper_execute_task
blueprinthelper_get_task_result
blueprinthelper_open_editor
blueprinthelper_close_editor
```

只读诊断使用：

```text
blueprinthelper_diagnostics
```

底层工具簇不作为普通主线：

```text
asset_create / add_component / set_component_properties / add_implemented_interface
append_blueprint_graph / replace_blueprint_graph / patch_blueprint_graph / merge_blueprint_graph
cleanup / rollback / ownership transfer
```

这些工具簇属于 TaskPlan capability、debug / expert、自动化测试和失败定位入口。

## Task 工具入参

`blueprinthelper_preview_task` 和 `blueprinthelper_execute_task` 的工具参数必须包一层 `task_spec`：

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1"
  }
}
```

不要把 `schema`、`task_type`、`target`、`behavior` 等 TaskSpec 顶层字段直接作为 MCP 工具参数。

## GraphWrite 写锚点

Patch/Merge 修改 BlueprintHelper-owned block 时，先用 `blueprinthelper_read_context` 读取 `logic_json`，再从 grouped block 中取：

```text
block_id
group_entry_node_path
node_ref
pin_ref
link_ref
```

`append_after` 使用 `block_id + group_entry_node_path + node_ref + pin_ref`。`insert_between` 还必须带 `link_ref`。不要只传 `link_ref`。

函数调用语句使用 `args`，每个参数值使用 `{ "kind": "literal", "value_type": "...", "value": ... }`；不要使用 `params` 或 plain value。
