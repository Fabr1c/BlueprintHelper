# AgentFace TaskSpec UE 编辑器操作能力矩阵

日期：2026-05-21

## 范围

本文只记录 AgentFace 通过编写 `BlueprintHelper.TaskSpec.v1` 并执行 preview / execute 能触达的 UE Editor 写入能力。

典型路径：

```text
AgentFace 编写 TaskSpec
-> AgentFace task-core TypeScript compiler
-> BlueprintHelper.TaskPlan.v1
-> Bridge preview_task_plan / execute_task_plan
-> UE TaskRuntime
-> Runtime cluster adapter
-> UE 编辑器资产变更
```

不计入本文范围：

- direct Bridge 命令。
- editor lifecycle 命令，例如 open / close editor。
- read context / diagnostics / runtime profile / review query / debug bundle 等只读或辅助工具。
- Agent 直接编写 TaskPlan 或 raw bridge payload。

`preview` 负责校验和 dry-run；`execute` 在获得写权限后执行编辑器侧变更。`validation.should_compile` 和 `validation.should_save` 可以触发编译 / 保存，但它们是执行流水线副作用，不是独立 TaskSpec 编辑能力。

## 总览矩阵

| TaskSpec `task_type` | AgentFace 语义 | 可触达 UE 编辑器操作 | TaskPlan capability / adapter |
|---|---|---|---|
| `create_asset` | 确保资产存在 | 创建 Blueprint Class / Actor、Widget Blueprint、UserDefinedStruct、DataTable、DataAsset；运行时 AssetFactory 也识别 Blueprint Interface、InputAction、InputMappingContext 等 `asset_type` | `asset_factory` / `create_asset` |
| `edit_blueprint_components` | 编辑 Blueprint 组件树 | 添加或复用组件、配置组件属性、移除组件 | `blueprint_component` / `add_component`, `set_component_properties`, `remove_component` |
| `edit_blueprint_class_settings` | 编辑 Blueprint 类级设置 | 添加实现接口、移除实现接口、设置 class default properties、Reparent Blueprint | `blueprint_class_settings` / `add_implemented_interfaces`, `remove_implemented_interfaces`, `set_class_default_properties`, `reparent_blueprint` |
| `edit_blueprint_signature` | 编辑 Blueprint callable 签名 | 确保函数、接口函数、自定义事件、接口事件、事件分发器、override/native event；带引用上下文保护地移除签名 | `blueprint_signature` / `ensure_function`, `ensure_custom_event`, `ensure_event_dispatcher`, `ensure_override_event`, `remove_signature` |
| `edit_blueprint_variables` | 编辑 Blueprint 变量 | 成员变量增删改、成员默认值设置、函数 local variable 增删改 | `blueprint_variable` / `add_blueprint_member_variables`, `blueprint_variable_batch` |
| `edit_blueprint_graph` | 编辑 Blueprint graph body | append 新 owned custom-event graph、replace owned function/event/custom-event/block body、patch owned node comment/position/pin default、按 anchor merge 插入 flow | `graph_write` / `append_blueprint_graph`, `replace_blueprint_graph`, `patch_blueprint_graph`, `merge_blueprint_graph` |
| `edit_umg_widget` | 编辑 Widget Blueprint tree / property | 添加 widget、设置 widget property、移除 widget | `umg_widget` / `add_widget`, `set_widget_property`, `remove_widget` |
| `edit_data_table` | 编辑 DataTable 行 | 添加行、更新行、删除行 | `data_table` / `add_datatable_row`, `update_datatable_row`, `delete_datatable_row` |
| `edit_object_properties` | 编辑 UObject / 资产反射属性 | 按 `property_path` 设置单个或多个 reflected property | `object_property` / `set_object_property`, `set_object_properties` |
| `create_blueprint_feature` | 组合式 Blueprint feature authoring | 在一个语义 TaskSpec 中组合组件、变量、class settings、signature、graph body 和接口实现 | emits `blueprint_component`, `blueprint_variable`, `blueprint_class_settings`, `blueprint_signature`, `graph_write` |

## 详细能力

### `create_asset`

AgentFace 可表达：

- `behavior.asset_strategy = "ensure_asset"`
- `behavior.asset.asset_type`
- `behavior.asset.parent_class`
- `behavior.asset.fields[]`
- `behavior.asset.row_struct`
- `behavior.asset.data_asset_class`
- `behavior.asset.value_type`
- `behavior.asset.collision` / `collision_policy`

可触达编辑器操作：

- 创建 Content Browser 资产 package。
- 创建 Blueprint class / Actor 派生 Blueprint。
- 创建 Widget Blueprint。
- 创建 UserDefinedStruct，并写入支持的字段定义。
- 创建 DataTable，要求提供 `row_struct`。
- 创建 DataAsset，要求提供 `data_asset_class`。
- 创建 Enhanced Input 相关资产时，运行时 AssetFactory 支持 InputAction / InputMappingContext。

约束：

- `data_table` 必须提供 `row_struct`。
- `data_asset` 必须提供 `data_asset_class`。
- 编译 / 保存由 validation 字段控制。

### `edit_blueprint_components`

AgentFace change kinds：

- `ensure_component_present`
- `configure_component`
- `remove_component`

可触达编辑器操作：

- 向 Blueprint 组件层级添加组件。
- 复用已有同名组件。
- 设置组件 reflected properties。
- 移除命名组件。
- 支持 parent component、socket、attach rule、name collision policy 等语义字段。

### `edit_blueprint_class_settings`

AgentFace 可表达：

- `behavior.interfaces.ensure_present[]`
- `behavior.interfaces.ensure_absent[]`
- `behavior.class_defaults[]`
- `behavior.reparent.new_parent_class`

可触达编辑器操作：

- 添加 Blueprint implemented interfaces。
- 移除 Blueprint implemented interfaces。
- 设置 Blueprint class default properties。
- Reparent Blueprint 到 `behavior.reparent.new_parent_class` 指定的新父类。

明确不支持：

- 通过 legacy `behavior.parent_class` 修改父类；应使用 `behavior.reparent.new_parent_class`。

### `edit_blueprint_signature`

AgentFace change kinds：

- `ensure_function`
- `ensure_interface_function`
- `ensure_custom_event`
- `ensure_interface_event`
- `ensure_event_dispatcher`
- `ensure_override_event`
- `remove_signature`

可触达编辑器操作：

- 创建或复用 Blueprint function。
- 创建或复用 graph 中的 custom event。
- 创建或校验 event dispatcher。
- 按 `execute_policy` 创建 override/native event。
- 通过 reference-context guard 移除 function / event / dispatcher 签名。

约束：

- `remove_signature` 默认走引用上下文保护路径。
- 存在引用、签名不匹配或迁移不安全时，preview 可以阻塞 execute。
- Signature 没有单独的 direct Bridge 命令；AgentFace 通过 TaskSpec -> TaskPlan adapter -> TaskRuntime cluster 触达。

### `edit_blueprint_variables`

AgentFace strategies：

- `member_variables`
- `member_defaults`
- `local_variables`

AgentFace semantic operations：

- `ensure_member_variable`
- `set_member_variable_properties`
- `remove_member_variable`
- `set_member_default`
- `ensure_local_variable`
- `set_local_variable_properties`
- `remove_local_variable`

可触达编辑器操作：

- 添加 Blueprint member variables。
- 更新 member variable metadata / type / property。
- 移除 member variables。
- 设置 member defaults。
- 在指定 function 的 entry graph 上添加、更新、移除 local variables。

### `edit_blueprint_graph`

AgentFace graph strategies：

- `append_new_owned_graph`
- `replace_owned_graph`
- `patch_owned_graph`
- `merge_owned_graph`

AgentFace body statement kinds：

- `call`
- `set`
- `branch`
- `let`
- `return`

当前 compiler / runtime 路径支持的表达式形态包括：

- literal
- get / symbol read
- get_property
- call
- op
- construct
- deconstruct
- select

Graph body semantic operation 边界：

- 下列语义簇只属于 `BlueprintLogicSpec` / Graph body 写入，不直接代表资产级、签名级、组件树、WidgetTree、DataTable 或 UObject 属性编辑能力。

```text
数据流：
get
set
get_property
set_property
op
construct
deconstruct
select

执行流：
control

普通调用：
call

实例 / 类型 / 绑定 / 调度：
create
convert
bind
schedule
```

- `create` 只表达 Graph body 内的运行时实例创建语义，例如 SpawnActor、CreateWidget runtime node、ConstructObject 等；不创建 Content Browser 资产、不创建 Blueprint 组件、不创建 WidgetTree 设计时节点、不创建 function/event/macro/dispatcher 签名。
- `set_property` 只表达 Graph body 内对对象表达式或结构体表达式的属性写入节点；不替代 `edit_object_properties`、`edit_blueprint_components`、`edit_umg_widget`、`edit_blueprint_class_settings` 等资产级或模板级属性写入。
- `bind` 只表达 Graph body 内把已有 delegate / event source 绑定到已有或已由 Signature 工具簇确保的 handler；不创建 event dispatcher、custom event、function、component bound event 签名。
- `schedule` 只表达需要生命周期状态、多节点编排、回调 handler、timer handle 或 latent callback 的 Graph body 调度语义；单个 UFunction 能稳定表达的场景继续走 `call`。
- `control` 是执行流统一语义，后续应替代 `branch` / `return` 等顶级 statement kind；当前文档中旧字段仅描述现状，不作为新 AgentFace canonical 目标。

可触达编辑器操作：

- append BlueprintHelper-owned custom-event graph body。
- replace BlueprintHelper-owned function / event / custom-event / block body。
- patch owned node comment、node position、pin default。
- 使用 `append_after`、`insert_between`、`branch_fork` 等稳定 anchor 策略插入 flow。

约束：

- 普通 Agent 写入应保持在 BlueprintHelper-owned graph scope 内。
- patch / merge 需要来自 `read_context` / `logic_json` 的稳定 owned-block anchor。
- compiler 不允许 normalize legacy `call_function` / `set_member_variable` / `ref` / `compare` / `make_struct`；这些旧 Graph body shapes 必须按 unsupported kind 报错，不是 deprecated compatibility，也不允许 hidden fallback。
- Signature lifecycle 归 `edit_blueprint_signature`；GraphWrite / SemanticIR 只能消费已解析 scope，不负责创建 function / event / macro / dispatcher 签名。
- Asset lifecycle 归 `create_asset`；Graph body 的 `create` 不允许创建或确保 Content Browser 资产。
- Component / WidgetTree lifecycle 分别归 `edit_blueprint_components` / `edit_umg_widget`；Graph body 的 `create` / `set_property` 不允许绕过这些工具簇修改模板或设计时树。

### `edit_umg_widget`

AgentFace change kinds：

- `create_widget`
- `update_widget_property`
- `delete_widget`

可触达编辑器操作：

- 向 Widget Blueprint tree 添加 widget。
- 设置 widget property。
- 移除 widget。

明确不支持：

- `move_widget` 存在于更底层的 bridge / service surface，但当前 TaskSpec compiler 会显式拒绝它，因此不算 AgentFace 写 TaskSpec 可用能力。

### `edit_data_table`

AgentFace row actions：

- `add`
- `update`
- `delete`

可触达编辑器操作：

- 添加 DataTable row。
- 更新 DataTable row fields。
- 删除 DataTable row。

### `edit_object_properties`

AgentFace 可表达：

- `behavior.property_strategy = "property_edit"`
- `behavior.changes[].property_path`
- `behavior.changes[].value`

可触达编辑器操作：

- 设置 UObject / DataAsset / asset-backed target 的 reflected property。
- 单属性变更 lower 到 `set_object_property`。
- 多属性变更 lower 到 `set_object_properties`。

### `create_blueprint_feature`

AgentFace 可组合：

- `components[]`
- `variables[]`
- `class_settings.implemented_interfaces[]`
- `class_settings.class_defaults`
- `behavior` graph body
- `integration.interface`

可触达编辑器操作：

- 在一个 TaskSpec 中生成多步 TaskPlan。
- 添加 / 配置组件。
- 添加 member variables 和 defaults。
- 添加 implemented interface。
- 确保 interface function signature。
- 写入 interface implementation function body 或普通 graph body。

明确不支持：

- composite 内创建新资产；需要拆成独立 `create_asset` TaskSpec。
- 非 interface integration，例如 input binding，目前会被 compiler 拒绝。

## 执行流水线副作用

TaskSpec execute 可能产生：

- TaskRunJournal。
- Review evidence / Review records。
- preview 或 execute 诊断失败时的 debug case id。
- 按 validation 策略触发的 Blueprint compile / asset save。

这些是执行流水线结果，不是 AgentFace 可单独写出的 TaskSpec 编辑器操作。

## 不属于 TaskSpec 写入能力的现有工具

| 能力 | 排除原因 |
|---|---|
| `blueprint_open_editor`, `blueprint_close_editor` | editor lifecycle surface，不是 TaskSpec 写入能力；Agent-owned open/close 只能走全局 MCP lifecycle，CLI lifecycle 调用会被阻断。 |
| `blueprinthelper_read_context`, `blueprinthelper_read_reference_context`, `blueprinthelper_read_function_chain_context` | TaskSpec authoring 前的读链路工具。 |
| `blueprinthelper_diagnostics`, `blueprinthelper_diagnostics_runtime`, `get_runtime_profile` | 诊断 / profiling 工具，不表达编辑器写入。 |
| `blueprinthelper_query_review_records`, `blueprinthelper_get_debug_case`, `blueprinthelper_export_debug_bundle` | Review / debug 查询工具，不表达编辑器写入。 |
| `open_asset`, `list_assets`, `save_asset`, `undo`, `redo`, `play_in_editor`, `exec_console_command` | direct Bridge / editor command surface，不是 AgentFace 写 TaskSpec 的普通路径。 |
| direct GraphWrite / Component / Widget / DataTable bridge calls | AgentFace 应写 semantic TaskSpec，由 compiler 和 TaskRuntime adapter 转换，不应手写 raw bridge payload。 |

## 2026-05-26 GraphWrite Field capability matrix

Field-like GraphWrite statements are addressed by stable `field.capability_id` values plus statement-local `field.*` facts. Shared GraphWrite contracts consume generic `CapabilityId`, `CapabilityFacts`, and `ReadbackFacts`; Field-only meaning stays inside the Field registry, resolver, context projection, fragment builder, and readback helpers.

| Priority | Capability ID | Node family | Required local facts |
|---|---|---|---|
| P0 | `field.member_get` | `variable_get` | `field.member_name`; optional `field.owner_class`, `field.member_guid` |
| P0 | `field.member_set` | `variable_set` | `field.member_name`; optional `field.owner_class`, `field.member_guid` |
| P0 | `field.local_get` | `variable_get` | `field.function_name` or `field.local_scope`; local/member name |
| P0 | `field.local_set` | `variable_set` | `field.function_name` or `field.local_scope`; local/member name |
| P0 | `field.component_ref_get` | `component_variable_get` | `field.component_name`; optional component owner/kind facts |
| P1 | `field.inherited_member_get` | `variable_get` | `field.owner_class`, `field.member_name` |
| P1 | `field.inherited_member_set` | `variable_set` | `field.owner_class`, `field.member_name` |
| P1 | `field.sparse_data_get` | `variable_get` | `field.owner_class`, `field.member_name` |
| P1 | `field.function_param_get` | `variable_get` | `field.function_name`, parameter/member name, parameter evidence |
| P1 | `field.struct_member_get` | `break_struct` | `field.property_path`, root expression evidence |
| P1 | `field.struct_member_set` | `set_fields_in_struct` | `field.property_path`, root expression evidence, write value/default facts |
| P2 | `field.object_pin_member_get` | `variable_get_target` | `field.target_pin_ref`, `field.target_pin_type`, `field.target_pin_object_path`, `field.owner_class`, `field.member_name` |
| P2 | `field.object_pin_member_set` | `variable_set_target` | `field.target_pin_ref`, `field.target_pin_type`, `field.target_pin_object_path`, `field.owner_class`, `field.member_name` |
| P2 | `field.component_ref_set` | `component_variable_set` | `field.component_name`; optional component owner/kind facts |
| P2 | `field.component_property_get` | `component_property_get` | `field.target_pin_ref`, target pin type/object facts, `field.property_path` |
| P2 | `field.component_property_set` | `component_property_set` | `field.target_pin_ref`, target pin type/object facts, `field.property_path`, write value/default facts |
| P2 | `field.nested_property_path` | `property_path_fragment` | `field.property_path`, root expression evidence, path segment facts |

Excluded Field-like inputs:

| Category | IDs | Required behavior |
|---|---|---|
| UI-only evidence | `field.drag_get`, `field.drag_set`, `field.pin_drag_get`, `field.pin_drag_set` | Reject with `unsupported_ui_entry_not_statement`; caller must map them to stable first-class IDs before execution. |
| Support/readback-only | `field.split_struct_pin_support`, `field.recombine_struct_pin_support` | Internal support only; no TaskSpec user statement surface. |
| Other clusters | `control.function_return_write`, `function.selected_component_call`, `component.add_component_node` | Route or reject outside FieldVariableActionCluster. |
| Diagnostic / first-stage excluded | `field.unsupported_path_diagnostic`, `field.by_ref_set` | Diagnostic-only; `field.by_ref_set` rejects with `unsupported_by_ref_set_deferred`. |

## 2026-05-26 GraphWrite OpCoverage capability matrix

`op` remains a Graph body expression semantic owned by `FunctionActionCluster`; OpCoverage is a logical capability group, not a new `graphwrite_op` runtime cluster and not a top-level `kind` expansion. Operation-specific facts are statement-local `op.*` evidence consumed by the op catalog, evidence reader, resolver policy, and readback verifier.

| Priority | Operation IDs | Runtime owner | Required local facts |
|---|---|---|---|
| Existing | `add`, `subtract`, `multiply`, `divide`, `greater`, `greater_equal`, `less`, `less_equal`, `equal`, `not_equal` | `FunctionActionCluster` TypePromotion first | typed operand/result evidence when the TypePromotion path requires it |
| P0 | `bitwise_and`, `bitwise_or`, `boolean_and`, `boolean_or`, `boolean_nand`, `max`, `min`, `string_append` | `FunctionActionCluster` callable/operator evidence | `op.operation_id`; stable callable evidence projected by catalog |
| P1 | `boolean_not`, `boolean_xor`, `boolean_nor`, `bitwise_not`, `bitwise_xor`, `abs`, `modulo`, `negate`, `dot`, `dot3`, `cross`, `cross3`, `near_equal`, `intpoint_equal`, `transform_compose`, `equal_exact`, `not_equal_exact`, `equal_ignore_case`, `not_equal_ignore_case`, `datetime_add_datetime`, `datetime_add_timespan`, `datetime_subtract_datetime`, `datetime_subtract_timespan`, `datetime_equal`, `datetime_not_equal`, `datetime_greater`, `datetime_greater_equal`, `datetime_less`, `datetime_less_equal` | `FunctionActionCluster` compact call-function evidence | `op.operation_id`; stable callable evidence projected by catalog |
| Removed duplicate | `array_identical` | `container_action` | Not an OpCoverage row; score through `container.array.identical`. |

Excluded OpCoverage inputs:

| Category | IDs | Required behavior |
|---|---|---|
| Missing stable non-UI evidence | `enum_equal`, `enum_not_equal`, SlateBrush equality | Reject deterministically; do not infer success from display name, menu text, or UI state. |
| Deferred conversion taxonomy | `convert_numeric`, `convert_string_text_name` | Route to future convert/type-transform evidence work instead of OpCoverage. |
| Different semantic owner | `array_map_set_mutation`, `validity_predicate` | Reject from OpCoverage until a matching container/control/predicate capability owns the semantics. |

## 源码依据

- `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- `AgentFaceService/task-core/src/task/compiler/task-compiler-service.ts`
- `AgentFaceService/agent-guide/Templates/write/SEMANTIC_INDEX.md`
- `BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/TaskPlanAdapters/*`
- `BlueprintHelper/Source/BlueprintHelper/Public/Runtime/TaskRuntime/Clusters/*`
- `BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.cpp`
- `BlueprintHelper/Source/BlueprintHelper/Private/Entry/Bridge/Utils/BlueprintHelperBridgeRoutePlannerUtils.cpp`
## 2026-05-26 GraphWrite GenericOps capability matrix

GenericOps is a public logical umbrella for grouped GraphWrite operations. It is not a runtime cluster and it is not a top-level `kind` expansion. After the 2026-05-26 de-dup cleanup, it no longer publishes container, schedule, asset_action, function-backed, or struct-member-set duplicate rows; execution still routes through existing runtime owners.

| Logical group | Runtime owner | Evidence boundary |
|---|---|---|
| `generic_ops.control` | `GenericAssetStructControlAction` | `generic.control.*`; singleton control, dedicated control-flow, or StandardMacros evidence. |
| `generic_ops.transform` | `GenericAssetStructControlAction` | `dynamic_cast`、`class_cast`、`type_promotion`; function-backed conversions stay FunctionAction and are not GenericOps rows. |
| `generic_ops.create` | `GenericAssetStructControlAction` | `spawn_actor`、`create_widget`、`construct_object`、`make_array`、`make_map`、`make_set`; `asset_action` stays in the `asset_action` core cluster. |
| `generic_ops.struct_select` | `GenericAssetStructControlAction` | `make_struct`、`break_struct`、`select`; struct member set stays in `field.struct_member_set`. |

Clarification for Field: a first-class Field capability means stable `field.capability_id` plus Field-owned registry/resolver/evidence/readback paths. It does not mean adding Field-specific concepts to GenericOps, broad shared DTOs, or the core `kind` enum beyond the canonical Field semantic surface.

Verification snapshot (2026-05-26): `npm.cmd --prefix AgentFaceService/task-core run build`, `npm.cmd --prefix AgentFaceService/task-core run test:node` (217/217), UE 5.6 `TemplateEditor` build, `BlueprintHelper.GraphWrite.GenericOps` (22/22), `BlueprintHelper.GraphWrite.ActionResolution.Generic` (24/24), `BlueprintHelper.GraphWrite.ContainerAction`, and `BlueprintHelper.GraphWrite.GenericSchedule` passed for this matrix update.

## 2026-05-26 GraphWrite EventDelegate use-site capability matrix

EventDelegate is a GraphWrite use-site capability owned by `EventDelegateActionCluster`. It consumes existing component/delegate/handler/signature evidence and does not create event declarations, custom events, handlers, dispatcher declarations, or handler signature mutations. Public compact TaskSpec statement kinds lower to `kind="component_bound_event"` or `kind="delegate"` plus second-stage `delegate_operation`.

| Capability ID | Runtime owner | Required local facts | Boundary |
|---|---|---|---|
| `event_delegate.component_bound_event` | `EventDelegateActionCluster` | component binding owner/property/field/class, delegate owner/property/signature, handler path/source/signature evidence, duplicate policy | existing component-bound event use-site only |
| `event_delegate.delegate.bind` | `EventDelegateActionCluster` | binding object kind/evidence, delegate owner/property/signature, handler path/source/signature evidence | binds to existing handler only |
| `event_delegate.delegate.assign` | `EventDelegateActionCluster` | binding object kind/evidence, delegate owner/property/signature, handler path/source/signature evidence, `ue_delegate_manual_assign_factory` | manual assign factory; UE Assign spawner side effect is blocked |
| `event_delegate.delegate.unbind` | `EventDelegateActionCluster` | binding object kind/evidence, delegate owner/property/signature, handler evidence, `unbind_mode=single` | missing handler fails with `handler_required_for_unbind` |
| `event_delegate.delegate.call` | `EventDelegateActionCluster` | binding object kind/evidence, delegate owner/property/signature, call arg pin-type facts | call args/defaults/links are validated and read back |
| `event_delegate.delegate.clear` | `EventDelegateActionCluster` | binding object kind/evidence, delegate owner/property, `unbind_mode=all` | handler evidence is forbidden |
| duplicate `fail` / `return_existing` | `EventDelegateActionCluster` | duplicate policy and optional existing binding evidence | deterministic failure or existing binding reporting |
| duplicate `replace` / `merge` | none | duplicate policy | rejected with `duplicate_mutation_policy_blocked` |

Excluded EventDelegate domains:

- action menu simulation, drag menus, selected Slate/component state, Details panel delegate binding, UMG designer events, Animation Blueprint events.
- event declaration, custom event creation, handler creation, handler signature mutation.
- automatic upstream function/field/handler creation.

Verification snapshot (2026-05-26): AgentFace task-core build passed; `test:node` passed 195/195; UE 5.6 `TemplateEditor` build succeeded; focused automation passed for `BlueprintHelper.GraphWrite.EventDelegate` (9/9, including 3 EventDelegate.ActionContext tests), `BlueprintHelper.GraphWrite.ActionResolution.EventDelegate` (16/16), `BlueprintHelper.GraphWrite.GraphStatement.EventDelegate` (6/6), `BlueprintHelper.GraphWrite.ActionContext.EventDelegate` (2/2 legacy ActionContext focused suite), and `BlueprintHelper.GraphWrite.ToolResult.EventDelegate` (1/1).

## DataFlowCore Slice C 文档同步状态（2026-05-21）

Canonical Graph body statement/expression path:

```text
AgentFace schema/docs
-> AgentFace task-core TypeScript compiler
-> SemanticIR parser
-> Resolver
-> Pattern Registry
-> NodeFragment Builder
-> FragmentDAG
-> Composer/Linker
-> UE Mutator
-> Review/Debug
-> ReadContext/LogicFlow
```

- [x] 文档已记录：old NodeHandler / parsed-node fallback 不允许保留，也不是 deprecated compatibility。
- [x] 文档已记录：legacy Graph body shapes `call_function` / `set_member_variable` / `ref` / `compare` / `make_struct` 必须作为 unsupported kind 报错，不允许 compiler normalization、alias、deprecated mapping 或 hidden fallback。
- [x] 文档已记录：Graph body statement/expression canonical path 从 AgentFace schema/docs 经 AgentFace task-core TypeScript compiler、SemanticIR parser、Resolver、Pattern Registry、NodeFragment Builder、FragmentDAG、Composer/Linker、UE Mutator，到 Review/Debug 与 ReadContext/LogicFlow。
- [ ] 未完成/待验证：本次 Slice C 未验证 canonical TS compiler、UE SemanticIR/Resolver/FragmentDAG/Mutator、Review/Debug、ReadContext/LogicFlow 的当前代码状态。
- [o] 部分完成：UE compile、editor preview/execute smoke、construct/deconstruct Vector smoke 已记录；TS tests、field-list/candidate preview、LogicFlow/readback 仍未在本次同步中验证。

## 2026-05-24 Struct / TypeStructure 能力同步

- `construct` / `deconstruct` 不归入 broad `create`。
- 当前 canonical lowering：`SpawnerClusterKind=GenericAssetStructControlAction` + `SemanticFamily=Struct|TypeStructure` + `TypeOperation=Construct|Deconstruct`。
- 已验证：`construct Vector` 和 `deconstruct Vector` 可通过 TaskSpec GraphWrite preview/execute，并且生成蓝图编译通过。
- 已验证：缺少 construct type 时 preview 阻断，错误码为 `needs_more_semantic_context`，不写资产。
- 未验证：候选字段发现列表、歧义候选列表、TS/Python 单元测试、LogicFlow/readback。
