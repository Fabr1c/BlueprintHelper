# BlueprintHelper GraphWrite UE-Side Done but TS Not Connected Audit

日期：2026-05-26

> 2026-05-27 同步：本审计保留为“接线前”历史输入。`kind="control"` 的 dedicated control 与 StandardMacros control、`schedule.timer_delegate_node`、`schedule.latent_or_async_node`、`generic_ops.struct_select.select` 已不再按“UE 已做但 TS 未接”或“未支持”统计。当前 `_005` 通用性报告中的相关 `unsupported_intent` 是旧 fixture / evidence 字段不匹配，不是能力未实现。

## 1. 口径

本文件只统计“UE/C++ 侧已经有 resolver / provider / fragment builder / readback 或自动化测试证据，但 AgentFaceService / TaskSpec / TS compiler 没有公开入口、没有 lowering，或只在能力 contract 中列出但 graph-body compiler 不接受”的项。

不统计 UI/menu/drag/pin-drag、Details/UMG/Animation editor-domain、asset lifecycle、component template lifecycle，也不把“TS contract 写了但 UE resolver 没真正支持”的项算入本清单。

2026-05-26 最新去重后，`generic_ops.container.*`、`generic_ops.schedule.*`、`generic_ops.create.asset_action`、`op_coverage.array_identical`、`generic_ops.struct_select.set_fields_in_struct` 和 GenericOps 中的 function-backed 子集不再作为“UE 已做 TS 未接”的 GraphWrite backlog。它们要么归 canonical owner，要么归 FunctionAction 语义路径。

## 2. 确定的 UE 已做但 TS compiler / IR 路由未接

| Priority | 能力 | UE 侧现状 | TS 侧断点 | 结论 |
|---|---|---|---|---|
| P0 | Dedicated control flow：`switch_int`、`switch_string`、`switch_name`、`switch_enum`、`multi_gate` | UE boundary 已识别 dedicated control vocabulary，并能按 `case_values` / `dynamic_output_count` 进入 `ControlFlowFragmentBuilder`；自动化已验证 `switch_int` 和 `multi_gate` resolved、spawned、readback。 | 2026-05-26 TS connect 后，compiler 已接受并 lower 这些 `kind="control"` operations；旧 `unsupported_control_kind` 断点已关闭。 | RESOLVED；只剩通用性 E2E fixture/readback 复验 |
| P0 | StandardMacros control flow：`do_once`、`do_n`、`gate`、`flip_flop`、`for_loop`、`for_loop_with_break`、`foreach_loop`、`foreach_loop_with_break`、`while_loop` | UE boundary 已识别 StandardMacros vocabulary，并要求 `generic.macro.graph_path` 与 `generic.macro.pin_shape_snapshot`；`MacroControlFragmentBuilder` 与自动化已验证 `for_loop` macro resolved/spawned/readback。 | 2026-05-26 TS connect 后，compiler 已接受 StandardMacros `kind="control"` operations；旧 `_005` 失败来自 fixture graph path / evidence 字段，不是 compiler 未接。 | RESOLVED；只剩通用性 E2E fixture/readback 复验 |
| P1 | Function-backed create owner：`create_function` | UE `FunctionSemanticActionResolver` 支持 `Create + function_operation=create_function`；GenericCreate resolver 明确把 function-backed create 判为 wrong owner，要求走 FunctionAction。 | TS `create` lowering 只输出 `create_operation`、`target/class_path/asset_path`、pin type 与 `context_evidence`；`function_operation` 只在 convert/schedule 语义字段中被 compiler 和 UE demand collector 正式处理，Create 没有公开/稳定 lowering。 | FunctionAction route question；不再作为 GenericOps public row |

## 3. TS 半接但还不是 first-class 的项

| Priority | 能力 | 当前状态 | 建议口径 |
|---|---|---|---|
| P1 | Function-backed transform family | UE 已把 function-backed transform 与 generic transform 分 owner：function-backed 走 `FunctionActionCluster`，generic resolver 会拒绝 wrong owner；TS compiler 可以透传 `function_operation=convert_function` 与 `transform_operation`。 | 不再作为 GenericOps public vocabulary；若后续补模板，应归 FunctionAction 路径。 |
| P1 | Function-backed schedule family | UE 已区分 generic schedule 与 FunctionAction schedule owner；function-backed schedule 走 FunctionAction，generic schedule resolver 拒绝 wrong owner。TS compiler 能透传 `function_operation=schedule_function/latent_or_async_function`。 | 不再作为 GenericOps public vocabulary；若后续补模板，应归 FunctionAction 路径。 |
| P2 | `select` result type proof | UE `SelectFragmentBuilder` 已要求 `generic.select.result_type_proof` 或 resolved result type，并能配置 `UK2Node_Select`。TS 已有 `select` expression lowering，并可通过 statement-local `context_evidence` 传入 result proof。 | 已支持；旧 `_005` 失败来自 fake/unresolved result proof fixture。final coverage 仍需用真实 proof 重跑。 |

## 4. 不应算入“UE 已做 TS 未接”的项

| 项 | 原因 |
|---|---|
| `timer_delegate_node`、`latent_or_async_node` | TS compiler 已有 `schedule` public shape、ownership-mix 校验和测试；UE resolver/readback 也接通。 |
| `asset_action` | 保留在 canonical `asset_action` core cluster；不再作为 `generic_ops.create.asset_action` backlog。 |
| `set_fields_in_struct` | 保留在 canonical `field.struct_member_set`；不再作为 `generic_ops.struct_select.set_fields_in_struct` backlog。 |
| `array_identical` | 保留在 canonical `container.array.identical`；不再作为 OpCoverage backlog。 |
| Function-backed convert / schedule 作为 raw compiler 路由 | TS 已能保留 `function_operation`、`transform_operation`、`schedule_operation`，并禁止 generic schedule owner mixing；它们的问题是 public/template partial，而不是 compiler completely missing。 |
| `container_action` V1 | TS public `kind=container_action`、operation validation、role expression validation 与 UE FunctionAction-backed container path 已接通。 |
| EventDelegate use-site | TS public `component_bound_event` 与 `delegate.*` lowering 已接到 UE `component_bound_event` / `delegate + delegate_operation` 内部形状。 |

## 5. 证据索引

- 历史证据：`AgentFaceService/task-core/src/task/compiler/task-compiler.ts:1552-1565`、`:1908-1920` 曾只允许 `branch/sequence/return` 并对其他 control 抛 `unsupported_control_kind`；该断点已由 TS connect 工作关闭。
- `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts:236-249`、`:420-438`：contract 已列出 switch、multi_gate、StandardMacros control operation。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.cpp:43-62`、`:158-192`：UE boundary 已识别 dedicated control 与 StandardMacros。
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperControlFlowExtensionTests.cpp:83-118`：UE 自动化验证 `switch_int` / `multi_gate` resolver、spawn、readback。
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperStandardMacroControlFlowTests.cpp:65-102`：UE 自动化验证 `for_loop` StandardMacro resolver、spawn、readback。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionSemanticActionResolver.cpp:139-151`：UE FunctionAction 支持 Create/Convert/Schedule 的 second-stage operation，其中 Create 只接受 `create_function`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.cpp:125-131`、`:213-217`：UE GenericCreate 将 function-backed create operation 判为 wrong owner。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:1581-1588`、`:3482-3490`：TS 只为 convert/schedule 复制 `function_operation` 等语义字段。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:3458-3479`：TS create lowering 未输出 top-level `function_operation`。
- `AgentFaceService/task-core/src/task/schema/task-contract.ts:69-83`：Agent-facing contract 第一层 statement/expression 列表尚未列出 `create`、`convert`、`schedule` 等 broad generic ops，尽管 compiler 内部可处理部分形状。
- `AgentFaceService/agent-guide/Templates/write/taskspec_graph_append_generic_ops_template.json:51-66`、`:140-149`：模板覆盖 `dynamic_cast` 与 `construct_object` 代表例，未覆盖 function-backed transform 或 `asset_action`。
- `AgentFaceService/agent-guide/Templates/write/taskspec_graph_append_generic_schedule_template.json:43-63`：模板覆盖 `timer_delegate_node`，未覆盖 function-backed schedule family。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp:626-638`、`:700-715`、`:1138-1157`：UE demand collector 只对 Convert/Schedule 应用 explicit function/transform/schedule evidence；Create evidence 不设置 FunctionOperation。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperStructFieldFragmentBuilder.cpp:17-29`、`:60-90`：UE struct field builder 支持 canonical `field.struct_member_set`。
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperStructSelectHardeningTests.cpp:118-171`：UE 测试覆盖 struct field evidence reader 与 builder boundary。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:1566-1579`、`:2635-2666`：TS expression 已有 `construct`、`deconstruct`、`select`；GenericOps 不再发布 struct member set 索引。
