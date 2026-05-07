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
8. 只有 preview completed 后才 execute_task
9. get_task_result if needed
10. report summary
```

TaskSpec 必须描述：目标资产、feature_name、scope_policy、asset_policy、resources、components、variables、class_settings、behavior、validation。
`feature_name` 只作为任务显示名 / journal 标签；图表名、函数名、变量名、block_id 必须显式填写，不能由 `feature_name` 推断。不要填写 `intent`，执行后的 `generated_intent` 由编排层写入 Journal。

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

`merge_owned_graph` 使用 `branch_fork + owned_block_call` 时，Preview 是写入门禁。TaskSpec 必须显式给出 `sequence_order`，且只使用 `original_successor` / `inserted_logic`；`inserted.block_id` 必须在 Preview 阶段解析为已有的 BlueprintHelper-owned CustomEvent block。Preview blocked 时禁止 execute，也不要回退到底层工具。

execute_task 仍可能因 UE 当前状态、资产变化或 Editor 写入失败而失败；失败结果必须带非空 error code/message/stage，报告时使用该错误和 task result/journal，不展开底层 Bridge payload。

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

GraphWrite `branch_fork` 成功 execute 后必须读回。读回应确认：

- 新插入的 Sequence 或等价分发节点连接在 anchor 之后。
- `inserted` call 节点可达。
- 原 original successor 仍从 Sequence 分支可达。
- 受影响执行流无孤立节点。

execute_task 成功后，普通报告只输出任务摘要、目标资产、主要变更、编译/保存/未完成项，不展开完整 TaskPlan、child transaction、Journal 路径或底层 Bridge JSON。

## 2026-05-07 调用参数检查点

在执行 smoke 或写入任务前，对照：

```text
Resources/AgentGuide/Reference/04_MCP_Field_Templates_20260507.md
```

执行顺序中的参数形状应为：

```text
read_context: schema/read_type/target/view/context 位于工具参数根对象
preview_task: 根对象只有 task_spec
execute_task: 根对象只有 task_spec
get_task_result: 根对象只有 task_run_id
```

不要把 `schema`、`read_type`、`target` 或 `task_spec` 再包进额外 `args`。如果客户端要求对象字段传 JSON string，字段名仍保持在根对象。
