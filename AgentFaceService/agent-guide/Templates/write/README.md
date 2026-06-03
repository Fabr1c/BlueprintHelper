# Write Templates / 写入模板

## 中文

写入模板都是 TaskSpec-first。优先复制 bare TaskSpec 模板，并使用分组 CLI 命令：

```powershell
node .\AgentFaceService\cli\build\cli\index.js task preview --file .\task.json --fields status,summary,artifacts.full_result
node .\AgentFaceService\cli\build\cli\index.js task execute --file .\task.json --fields status,task_run_id,summary,artifacts.full_result
```

使用 `SEMANTIC_INDEX.md` 按 Agent 意图选择模板。

当使用 `blueprinthelper_preview_task` 或 `blueprinthelper_execute_task` 工具名命令时，使用带 `task_spec` 根字段的 wrapper 模板。

常见写入循环：

1. 用 `../read` 中的模板读取上下文。
2. 复制匹配的 TaskSpec 模板。
3. 替换占位符并删除未使用的可选字段。
4. Preview。
5. 只有 preview 提示 write permission disabled 时才 request write session。
6. Execute。
7. 读取 task result，并回读目标上下文。

## English

Write templates are TaskSpec-first. Prefer copying a bare TaskSpec template and using grouped CLI commands:

```powershell
node .\AgentFaceService\cli\build\cli\index.js task preview --file .\task.json --fields status,summary,artifacts.full_result
node .\AgentFaceService\cli\build\cli\index.js task execute --file .\task.json --fields status,task_run_id,summary,artifacts.full_result
```

Use `SEMANTIC_INDEX.md` to choose a template by Agent intent.

When using the tool-name commands `blueprinthelper_preview_task` or `blueprinthelper_execute_task`, use wrapper templates with the `task_spec` root field.

Common write loop:

1. Read context with a template from `../read`.
2. Copy a matching TaskSpec template.
3. Replace placeholders and remove unused optional fields.
4. Preview.
5. Request write session only if preview says write permission is disabled.
6. Execute.
7. Read task result and read back target context.

GraphWrite coverage notes:

1. Use `taskspec_graph_append_container_action_template.json` for first-class array, map, and set operations.
2. Use `taskspec_graph_append_event_delegate_template.json` for public `component_bound_event` and `delegate.*` statements. Do not author the compiler-internal `kind=delegate` shape directly.
3. Use `taskspec_graph_append_generic_schedule_template.json` for `timer_delegate_node` or `latent_or_async_node`; do not add `function_operation` to those generic schedule statements.
4. Use `taskspec_graph_append_generic_ops_template.json` for representative op, convert, construct/select, create, and branch examples. Remove unused example statements before preview.
5. Use `taskspec_edit_blueprint_class_settings_template.json` for interfaces, class defaults, and `behavior.reparent.new_parent_class`.
6. Use `taskspec_graph_merge_external_flow_template.json` only when read context exposes a stable external exec boundary in user-authored graph logic. Keep `scope_policy.allow_modify_user_nodes=false`, keep `external_mutation_policy.allowed_mutations=["exec_boundary_link"]`, and do not mix owned `merges[]` with `external_merges[]`.
