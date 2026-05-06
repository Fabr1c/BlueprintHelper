# 04 - TaskSpec 修改蓝图工作流

标准流程：

```text
1. get_runtime_profile
2. read_context / read_reference_context as needed
3. build TaskSpec
4. preview_task
5. 如果 context_required/context_stale：重新 read_context / read_reference_context
6. 如果 TaskSpec error：按 suggested_patch 修正
7. 如果 preview_blocked：stop_and_report 或修改 TaskSpec
8. execute_task
9. get_task_result if needed
10. report summary
```

TaskSpec 必须描述：目标资产、feature_name、scope_policy、asset_policy、resources、components、variables、class_settings、behavior、validation。

调用 `blueprinthelper_preview_task` / `blueprinthelper_execute_task` 时，MCP 工具参数固定为：

```json
{
  "task_spec": {
    "schema": "BlueprintHelper.TaskSpec.v1"
  }
}
```

不要把完整 TaskSpec 的顶层字段直接平铺到工具参数里。

Patch/Merge 已有 BlueprintHelper-owned block 时，先用 `blueprinthelper_read_context` 读取 `logic_json`。写入锚点必须来自 grouped block：`block_id + group_entry_node_path + node_ref + pin_ref`，`insert_between` 额外需要 `link_ref`。不要把全图级 `nodes[0]`、显示名、GUID-first selector 当普通主线写锚点。

GraphWrite body 内的函数调用使用 `args`，每个参数值是结构化 literal：

```json
{
  "kind": "call_function",
  "name": "PrintString",
  "args": {
    "InString": {
      "kind": "literal",
      "value_type": "string",
      "value": "message"
    }
  }
}
```

execute_task 成功后，普通报告只输出任务摘要、目标资产、主要变更、编译/保存/未完成项，不展开完整 TaskPlan、child transaction、Journal 路径或底层 Bridge JSON。
