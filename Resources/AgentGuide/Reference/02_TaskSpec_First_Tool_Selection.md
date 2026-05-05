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
