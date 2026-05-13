# 02 - TaskSpec-first Tool Selection

默认工具链:

```text
blueprint_get_runtime_profile
blueprinthelper_read_agent_guide
blueprinthelper_read_context
blueprinthelper_read_reference_context
blueprinthelper_preview_task
blueprinthelper_request_write_session
blueprinthelper_execute_task
blueprinthelper_get_task_result
```

只读诊断:

```text
blueprinthelper_diagnostics
blueprinthelper_diagnostics_runtime
```

`blueprint_open_editor` 只用于显式启动目标 Editor 的 preflight。

`blueprinthelper_request_write_session` is the ordinary interactive write authorization path. Use it after preview and before execute only when write permission is disabled and the user approves the Editor-side prompt.
The Editor prompt is intentionally a simple accept/reject dialog. If the user rejects it, stop and report. Approval applies to the running Editor/Bridge for the approved scope and lifetime, so SideAgents can execute BlueprintHelper tools after approval without receiving secret session data. Do not request or pass `BLUEPRINTHELPER_BRIDGE_TOKEN`, `auth_token`, or `auth_session`.
## Task Tool Arguments

`blueprinthelper_preview_task` 和 `blueprinthelper_execute_task` 的工具参数必须包一层 `task_spec`:

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1"
  }
}
```

不要把 TaskSpec 顶层字段直接平铺到无关工具根参数中。`blueprinthelper_preview_task` 和 `blueprinthelper_execute_task` 只接受根字段 `task_spec`。不要额外包 `args`。

## Context And Anchors

Patch/Merge 修改 BlueprintHelper-owned block 时，先用 `blueprinthelper_read_context` 读取 `logic_json`，再从 grouped block 中取:

```text
block_id
group_entry_node_path
node_ref
pin_ref
link_ref
```

`append_after` 使用 `block_id + group_entry_node_path + node_ref + pin_ref`。`insert_between` 还必须带 `link_ref`。

Graph body 里的函数调用使用 `args` 表达函数参数。这里的 `args` 不是 BlueprintHelper 工具参数包装。

## Frozen Boundary

底层 capability 入口只供 Task Runtime、测试和专家诊断使用。普通 Agent 不从 AgentGuide 选择这些入口。preview blocked 时停止报告或修正 TaskSpec。

普通 Agent 不操作 ReviewPanel，不展开 ReviewRecord 内部状态。工具返回 `debug_case_ids[]` 时，可以用 summary-only `blueprinthelper_get_debug_case` 定位问题，但不能读取 DebugBundle artifact 内容、本地 bundle 路径、raw payload、source content 或 token/settings 全文。
