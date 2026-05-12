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

## Graph Logic

用 `edit_blueprint_graph` 描述入口、逻辑 body、资源引用和插入策略。锚点必须来自 `logic_json` grouped block。

函数调用语句中的 `args` 只表示函数参数:

`call_function.name` may be a native function name, a Blueprint display name, or an owner-qualified native name. Preview resolves it against the target Blueprint graph. If preview reports ambiguity, change `name` to an owner-qualified native name such as `/Script/Engine.KismetSystemLibrary:PrintString` and preview again.

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

Explicit component/member calls are not part of the first CallFunction resolver slice. Preview blocks them instead of guessing target object ownership:

```json
{
  "kind": "call_function",
  "name": "DoorMesh.AddAngularImpulseInDegrees",
  "args": {}
}
```

## Removal Or Rename

删除、重命名、断线和批量改动前先读取 reference context。preview blocked 时停止，不执行写入。

If a write session is required, the Editor prompt is simple accept/reject. If rejected, report the denied write session and do not execute.

## Reporting

普通报告只写任务状态、目标资产、主要变更、验证状态和剩余风险。不展开 TaskPlan、内部 capability 或原始 Bridge JSON。
