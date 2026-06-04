# 05 - Edit Blueprint Workflow

## Goal

以 TaskSpec-first 修改 UE 资产。Agent 只提交 `BlueprintHelper.TaskSpec.v1`，由 TaskSpec 编译器生成 TaskPlan，由 UE Task Runtime 调用内部 capability。

## Preflight

确认:

- `asset_path`
- `graph_name` 或等价 TaskSpec 目标
- 是否允许创建资产
- 是否允许修改用户节点
- 是否允许接入已有执行流
- `validation.should_compile` 和 `validation.should_save`

## Standard Flow

```text
profile
-> read_context or read_reference_context
-> build TaskSpec
-> preview_task
-> repair or stop
-> request_write_session if write_permission is disabled
-> execute_task
-> get_task_result when needed
-> report summary
```

## Variables

用 `edit_blueprint_variables` 描述变量名、类型、默认值、分类和提示文本。已存在变量通过 policy 表达 reuse 或 fail，不在 execute 后猜测。

Member-variable replication stays nested under `configure_member_variable.properties[]`:

```json
{
  "kind": "configure_member_variable",
  "name": "DoorState",
  "properties": [
    {
      "property_path": "replication",
      "value": {
        "mode": "rep_notify",
        "condition": "owner_only"
      }
    }
  ]
}
```

Local-variable replication is unsupported. Do not place `property_path: "replication"` under `configure_local_variable`.

## Graph Logic

用 `edit_blueprint_graph` 描述入口、逻辑 body、资源引用和插入策略。锚点必须来自 `logic_json` grouped block。

函数调用语句中的 `args` 只表示函数参数:

`call.target` may be a native function name, a Blueprint display name, an owner-qualified native name, or an explicit component/member call for append-owned graph writes. Preview resolves it against the target Blueprint graph. If preview reports ambiguity, change `target` to an owner-qualified native name such as `/Script/Engine.KismetSystemLibrary:PrintString` and preview again. Legacy `call_function.name` is unsupported.

```json
{
  "kind": "call",
  "target": "PrintString",
  "args": {
    "InString": {
      "kind": "literal",
      "value_type": "string",
      "value": "message"
    }
  }
}
```

Append-owned graph writes may use explicit component/member calls. The object prefix must name a Blueprint member or component that can be read through a generated getter, and the function part is resolved by the UE graph-aware resolver:

```json
{
  "kind": "call",
  "target": "DoorMesh.AddAngularImpulseInDegrees",
  "args": {}
}
```

### Owned Link/Delete Patch

Use `patch_owned_graph` only when `blueprinthelper_read_context` with `view.format=logic_json` provides a grouped owned block with stable `block_id`, `group_entry_node_path`, `node_ref`, `pin_ref`, and, for link operations, `link_ref`. Put `block_id` and `group_entry_node_path` only on `target_ref`; `source_ref` and `replacement_ref` carry same-block `node_ref` and `pin_ref`, with the block derived by the compiler. P0-D owned link/delete patches do not use `expected_old_state`; link state comes from `target_ref.link_ref`. Use safe `delete_policy` for `delete_owned_node`. Do not use these templates for user-authored nodes or external graph anchors.

## Removal Or Rename

删除、重命名、断线和批量改动前先读取 reference context。preview blocked 时停止，不执行写入。

If a write session is required, the Editor prompt is simple accept/reject. If rejected, report the denied write session and do not execute.

## Reporting

普通报告只写任务状态、目标资产、主要变更、验证状态和剩余风险。不展开 TaskPlan、内部 capability 或原始 Bridge JSON。
