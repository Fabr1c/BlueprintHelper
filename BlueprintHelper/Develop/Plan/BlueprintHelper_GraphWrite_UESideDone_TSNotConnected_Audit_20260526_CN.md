# BlueprintHelper GraphWrite UE-Side Done but TS Not Connected Audit

日期：2026-05-26

## 1. 口径

本文件只统计“UE/C++ 侧已经有 resolver / provider / fragment builder / readback 或自动化测试证据，但 AgentFaceService / TaskSpec / TS compiler 没有公开入口、没有 lowering，或只在能力 contract 中列出但 graph-body compiler 不接受”的项。

不统计 UI/menu/drag/pin-drag、Details/UMG/Animation editor-domain、asset lifecycle、component template lifecycle，也不把“TS contract 写了但 UE resolver 没真正支持”的项算入本清单。

## 2. 确定的 UE 已做但 TS compiler / IR 路由未接

| Priority | 能力 | UE 侧现状 | TS 侧断点 | 结论 |
|---|---|---|---|---|
| P0 | Dedicated control flow：`switch_int`、`switch_string`、`switch_name`、`switch_enum`、`multi_gate` | UE boundary 已识别 dedicated control vocabulary，并能按 `case_values` / `dynamic_output_count` 进入 `ControlFlowFragmentBuilder`；自动化已验证 `switch_int` 和 `multi_gate` resolved、spawned、readback。 | TS contract 已发布这些 operation，但 TS compiler 的 `SUPPORTED_GRAPH_BODY_CONTROL_KINDS` 仍只有 `branch`、`sequence`、`return`；`getControlStatementKind()` 对其他 control 直接 `unsupported_control_kind`。 | UE done / TS contract partial / compiler missing |
| P0 | StandardMacros control flow：`do_once`、`do_n`、`gate`、`flip_flop`、`for_loop`、`for_loop_with_break`、`foreach_loop`、`foreach_loop_with_break`、`while_loop` | UE boundary 已识别 StandardMacros vocabulary，并要求 `generic.macro.graph_path` 与 `generic.macro.pin_shape_snapshot`；`MacroControlFragmentBuilder` 与自动化已验证 `for_loop` macro resolved/spawned/readback。 | TS contract 已列出 StandardMacros operation，但 TS graph-body `control` 仍只允许 `branch/sequence/return`。Agent-facing TaskSpec 无可通过的 compiler 路由。 | UE done / TS contract partial / compiler missing |
| P1 | Function-backed create owner：`create_function` 以及 contract 层的 `async_action`、`function_backed_create`、`function_backed_spawn`、`function_backed_construct` | UE `FunctionSemanticActionResolver` 支持 `Create + function_operation=create_function`；GenericCreate resolver 明确把 `async_action/function_backed_*` 判为 wrong owner，要求走 FunctionAction。 | TS `create` lowering 只输出 `create_operation`、`target/class_path/asset_path`、pin type 与 `context_evidence`；`function_operation` 只在 convert/schedule 语义字段中被 compiler 和 UE demand collector 正式处理，Create 没有公开/稳定 lowering。 | UE owner boundary done / TS + IR create route missing |

## 3. TS 半接但还不是 first-class 的项

| Priority | 能力 | 当前状态 | 建议口径 |
|---|---|---|---|
| P1 | Function-backed transform family：`function_conversion`、`blueprint_autocast`、`numeric_conversion`、`string_name_text_conversion`、`enum_conversion`、`object_to_soft_object`、`class_to_soft_class` | UE 已把 function-backed transform 与 generic transform 分 owner：function-backed 走 `FunctionActionCluster`，generic resolver 会拒绝 wrong owner；TS compiler 可以透传 `function_operation=convert_function` 与 `transform_operation`，但 Agent-facing contract/template 主要示例仍是 `dynamic_cast`，同族 function-backed convert 没有 first-class public shape。 | 记为 TS partial，不是 compiler 完全没接；补的是 Agent-facing operation vocabulary、模板和校验矩阵。 |
| P1 | `asset_action` | UE 已有 ActionDatabase projection、stable id / spawner signature evidence、stale/ambiguous 校验和 dedicated resolver；TS compiler 可透传 `create_operation` 与 `context_evidence`，但 Agent-facing contract 第一层未列出 `create`，模板只给 `construct_object` 代表例，没有把 `asset_action` 做成直接可用的任务形态。 | 记为 TS partial；不是 UE 缺实现，也不是 raw compiler blocker，缺的是 public TaskSpec ergonomics 与模板。 |
| P1 | Function-backed schedule family：`timer_by_function_name`、`timer_by_handle`、clear/pause/unpause timer、`delay`、`retriggerable_delay`、`delay_until_next_tick`、`generic_latent_function_call`、`async_proxy_output_delegate_connection` | UE 已区分 generic schedule 与 FunctionAction schedule owner；function-backed schedule 走 FunctionAction，generic schedule resolver 拒绝 wrong owner。TS compiler 能透传 `function_operation=schedule_function/latent_or_async_function`，但公开模板只覆盖 `timer_delegate_node` 这类 generic schedule 小段。 | 记为 TS partial；补的是 Agent-facing schedule vocabulary，而不是 UE resolver。 |
| P1 | `set_fields_in_struct` | UE `StructFieldFragmentBuilder` 支持 `make_struct`、`break_struct`、`set_fields_in_struct`，并对 `set_fields_in_struct` 要求 `generic.struct.selected_field_paths`；测试覆盖 evidence reader / builder boundary。TS 侧公开表达主要是 `construct`、`deconstruct`、`select` 和 `set_property`，没有 `generic_ops.struct_select.set_fields_in_struct` 的 first-class public shape。 | 记为 TS partial，不应算作完全没接；补的是 first-class generic struct operation 入口和 readback 矩阵，而不是从零实现 UE。 |
| P2 | `select` result type proof | UE `SelectFragmentBuilder` 已要求 `generic.select.result_type_proof` 或 resolved result type，并能配置 `UK2Node_Select`。TS 已有 `select` expression lowering，但 result proof 主要靠 `context_evidence` 透传，没有专门的 public validation/ergonomic shape。 | 记为 TS partial；不是 blocker，但 final coverage 不能只按“TS 有 select kind”算满。 |

## 4. 不应算入“UE 已做 TS 未接”的项

| 项 | 原因 |
|---|---|
| `asset_backed_graph_node` | TS contract 与 `ActionResolutionCore` 词表出现了该 token，但实际 `GenericCreateActionResolver::IsSupportedCreateOperation()` 只接受 `asset_action`，不接受 `asset_backed_graph_node`；这不是 UE 已完成。 |
| `interface_dynamic_cast` | TS contract 列出，但 UE generic transform resolver 当前只支持 `dynamic_cast`、`class_cast`、`type_promotion`；这是 TS/contract ahead，不是 TS 未接 UE。 |
| `timer_delegate_node`、`latent_or_async_node` | TS compiler 已有 `schedule` public shape、ownership-mix 校验和测试；UE resolver/readback 也接通。 |
| Function-backed convert / schedule 作为 raw compiler 路由 | TS 已能保留 `function_operation`、`transform_operation`、`schedule_operation`，并禁止 generic schedule owner mixing；它们的问题是 public/template partial，而不是 compiler completely missing。 |
| `container_action` V1 | TS public `kind=container_action`、operation validation、role expression validation 与 UE FunctionAction-backed container path 已接通。 |
| EventDelegate use-site | TS public `component_bound_event` 与 `delegate.*` lowering 已接到 UE `component_bound_event` / `delegate + delegate_operation` 内部形状。 |

## 5. 证据索引

- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:1552-1565`：TS graph-body statement 支持 `control`，但 control 子种类集合只包含 `branch`、`sequence`、`return`。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:1908-1920`：TS 对非 `branch/sequence/return` 的 control 抛 `unsupported_control_kind`。
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
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperStructFieldFragmentBuilder.cpp:17-29`、`:60-90`：UE struct field builder 支持 `set_fields_in_struct` 并要求 selected field evidence。
- `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperStructSelectHardeningTests.cpp:118-171`：UE 测试覆盖 `set_fields_in_struct` evidence reader 与 builder boundary。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:1566-1579`、`:2635-2666`：TS expression 已有 `construct`、`deconstruct`、`select`，但不是 first-class `generic_ops.struct_select.set_fields_in_struct` public shape。
