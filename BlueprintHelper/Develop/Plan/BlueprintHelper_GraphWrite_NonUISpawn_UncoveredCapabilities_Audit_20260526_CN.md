# BlueprintHelper GraphWrite Non-UI Spawn Uncovered Capabilities Audit

日期：2026-05-26

## 1. 审计口径

本审计回答“BlueprintHelper 可以覆盖但当前尚未覆盖或未完全贯通的能力”。这里的“可以覆盖”限定为非 UI 强关联 Spawn：

- 可以通过 `TaskSpec -> SemanticIR -> ActionContext -> ActionResolution -> NodeSpawner / ActionDatabase / direct K2 node provider -> Fragment / readback` 表达。
- 不需要模拟右键菜单、拖拽、拖 Pin、Details 面板、UMG Designer、Slate selection 或 editor widget 状态。
- 不创建资产生命周期、Blueprint Signature、组件模板、WidgetTree 设计时对象，除非它们只是 fixture/evidence 依赖。
- 已有其他工具 ownership 的能力不并入 GraphWrite；只记录 GraphWrite 可消费的 use-site 或 graph-body node spawn 部分。

结论口径：下表不是新的完成率，也不是 final generality preflight 结果；它是当前源码/文档边界下的“未覆盖但可规划”清单。

## 2. 优先结论

当前最值得补的不是 UI 菜单行为，而是这些非 UI Spawn 能力：

1. 宽控制流 public TaskSpec 与 body composition：`switch_*`、`multi_gate`、StandardMacros loop/gate/do/flip-flop 家族。
2. Function-backed transform / schedule / create 的 normalized operation 覆盖：当前 contract 比 runtime first-class 支持更宽。
3. Runtime graph-body predicate / special-expression nodes：`IsValid` predicate、`MathExpression` / `GetClassDefaults` 等特殊 K2 node。
4. ActionDatabase asset-backed graph nodes：当前已有 `asset_action` 证据边界，但还没有按常见节点族做 first-class taxonomy 与 readback 矩阵。

## 3. 可覆盖但尚未完全覆盖的能力

| Priority | 能力族 | 当前状态 | 为什么属于非 UI Spawn | 建议 owner / 入口 |
|---|---|---|---|---|
| P0 | 宽控制流 public shape：`switch_int`、`switch_string`、`switch_name`、`switch_enum`、`multi_gate` | capability contract 与 C++ resolver 已有控制流证据路径，但 Agent-facing compiler 仍只接受 `branch` / `sequence` / `return`；完整 body composition 计划仍未落地。 | `UK2Node_Switch*` / `UK2Node_MultiGate` 可由 node class/provider 创建，case/default/output pin 可由 statement-local evidence 配置，不依赖菜单。 | `GenericAssetStructControlActionCluster`，扩展 `kind=control` public shape 与 Fragment body branch composition。 |
| P0 | StandardMacros 控制流：`do_once`、`do_n`、`gate`、`flip_flop`、`for_loop`、`for_loop_with_break`、`foreach_loop`、`foreach_loop_with_break`、`while_loop` | C++ GenericOps resolver 可消费 `generic.macro.graph_path` 与 `generic.macro.pin_shape_snapshot`，但 public compiler 未接受这些 control kinds，final body/readback matrix 未闭环。 | `UK2Node_MacroInstance` 可用明确 macro graph path + pin snapshot 创建；array/condition/index/body pins 可由 TaskSpec evidence 驱动。 | `GenericAssetStructControlActionCluster`，复用 macro control fragment/readback。 |
| P0 | BroadControlFlow 计划中未进入当前 runtime vocabulary 的 `do_once_multi_input`、`reverse_foreach_loop` | `BroadControlFlowPlan` 将其列入范围，但当前 GenericOps contract / boundary 不在支持集内。 | `UK2Node_DoOnceMultiInput` 和 StandardMacros reverse foreach 都是 graph node spawn/configure，不是 UI 操作。 | 先扩展 control operation registry，再进入 Generic cluster。 |
| P1 | Function-backed conversions：numeric、string/name/text、enum、blueprint autocast、object/class soft reference conversion | `generic_ops.transform` contract 已列出这些 operation，但 runtime Generic transform resolver 只支持 `dynamic_cast`、`class_cast`、`type_promotion`；OpCoverage 也把 `convert_numeric` / `convert_string_text_name` 排除到 convert taxonomy。 | 这些本质是 callable/autocast/typed conversion node，可由 typed pin evidence + callable/spawner evidence 选择，不需要 UI。 | Function-backed operation 归 `FunctionActionCluster`；generic cast node 归 Generic。需要 normalized `convert.*` taxonomy。 |
| P1 | Function-backed schedule：timer by function/handle、clear/pause/unpause timer、delay、retriggerable delay、delay until next tick、generic latent function call、async proxy delegate connection | contract 已列出 `generic_ops.schedule.*`，但 runtime Generic schedule resolver 只支持 `timer_delegate_node` 和 `latent_or_async_node`；FunctionAction 目前是 query-driven `schedule_function` / `latent_or_async_function`，不是逐 operation matrix。 | Timer/latent/async callable nodes 能由 ActionDatabase/callable evidence 创建；handler/signature 只作为 dependency evidence。 | Function-backed schedule 归 `FunctionActionCluster`，Generic schedule node 保持 Generic。 |
| P1 | Function-backed create / spawn / construct 与 async action | contract 列出 `async_action`、`function_backed_create`、`function_backed_spawn`、`function_backed_construct`，但 Generic create resolver当前只支持 `spawn_actor`、`create_widget`、`construct_object`、`make_array/map/set`、`asset_action`。 | BlueprintCallable factory / async action node 可由 callable evidence 或 ActionDatabase evidence 生成。 | Function-backed create 归 `FunctionActionCluster`；asset-backed graph node 归 Generic asset action。 |
| P1 | `asset_backed_graph_node` 的常见节点族 taxonomy | contract 将其列为 Generic create operation，ActionDatabase projection 已用于 `asset_action`，但 runtime `GenericCreateActionResolver::IsSupportedCreateOperation` 未接受 `asset_backed_graph_node`。 | DataTable、Material Parameter Collection、asset-specific action nodes 可通过 projected ActionDatabase spawner identity 创建。 | `GenericAssetStructControlActionCluster` / `asset_action` evidence reader；需要节点族 readback。 |
| P1 | Struct / select 泛化：`make_struct`、`break_struct`、`set_fields_in_struct`、typed `select` | 当前已有 Generic struct/select resolver 与 hardening tests，但仍主要是 focused coverage；final ownership-filtered matrix 尚未把多 struct、多字段、多 select result type proof 全部跑通。 | `K2Node_MakeStruct`、`K2Node_BreakStruct`、`K2Node_SetFieldsInStruct`、`K2Node_Select` 都可由 type path、selected field paths、result-type proof 驱动。 | `GenericAssetStructControlActionCluster`；补充 10-variant readback matrix。 |
| P1 | Component target function call / call-on-member | Field matrix 将 `function.selected_component_call` 标为 other-cluster；当前 FunctionAction 已有 target object pin/type evidence，但没有 first-class component-ref call taxonomy。 | 对已投影的 component reference 调用函数是普通 `K2Node_CallFunction` / `K2Node_CallFunctionOnMember` 选择，不需要选中组件 UI 状态。 | `FunctionActionCluster`，以 `component_ref` evidence 表达 target object，禁止 selected Slate/component state。 |
| P2 | Predicate family：`validity_predicate` / IsValid / IsNotValid | OpCoverage 当前按 different semantic owner 拒绝，不应作为 plain `op`；尚无独立 predicate/control taxonomy。 | IsValid 类节点/宏可由 node class 或 callable/macro evidence 生成，并能作为 branch condition 或 expression。 | 新建 predicate semantic 或归 Generic control predicate；不要塞回 OpCoverage。 |
| P2 | ContainerAction V1 之外的容器能力 | 核心 array/map/set V1 已覆盖，但 `foreach` 明确归 future control-flow；custom predicate container operation 仍未进入稳定 TaskSpec 语义。 | 容器 callable 本身可 FunctionAction-backed spawn；body-producing foreach 与 predicate callback 需要 control/predicate ownership，而不是 UI。 | `foreach` 归 broad control-flow；predicate callback 先设计 Predicate/Function boundary。 |
| P2 | Special expression nodes：`MathExpression`、`GetClassDefaults`、`CallParentFunction` 等 | UE BlueprintGraph 暴露这些 K2 node 类型；当前 GraphWrite 对 `FormatText` 已有 parsed/readback 支持，`CallFunctionOnMember` 更接近普通 FunctionAction target-call ownership，因此本行只保留尚未 first-class taxonomy/readback 的特殊节点族。 | 它们是稳定 K2 node / NodeSpawner families，可由 explicit node class + evidence + readback 表达。 | 按节点族拆到 FunctionAction 或 Generic special-expression，不做 UI menu 模拟。 |
| P2 | Timeline-like scheduler | GenericOps UE source-read handoff 将 timeline-like scheduler 标为待确认；当前 schedule implementation 只闭合 timer delegate node 与 latent/async node。 | Timeline node spawn 本身可非 UI，但通常涉及 Blueprint timeline template/member lifecycle。 | 先做 ownership split：GraphWrite 只写 use-site node；timeline template lifecycle 另归 Signature/asset-like lifecycle。 |

## 4. 当前应继续排除的项

| 项 | 排除原因 |
|---|---|
| `field.drag_get` / `field.drag_set` / `field.pin_drag_get` / `field.pin_drag_set` | UI-only evidence；调用方必须映射为 stable `field.*` capability 后再执行。 |
| split / recombine struct pin 作为用户 statement | 当前是 support/readback-only 或 linker/pin-shape 行为，不是 GraphWrite statement operation。 |
| Event / custom event / function / macro / dispatcher declaration | Signature lifecycle ownership；GraphWrite/EventDelegate 只能消费已有 declaration/signature/handler evidence。 |
| EventDelegate duplicate `replace` / `merge` | 当前设计性拒绝，避免在 use-site writer 中隐式删除/合并既有绑定。 |
| `enum_equal` / `enum_not_equal` | 当前缺少稳定非 UI spawn evidence；不能靠右键菜单或 display name 推断。若后续能证明 enum typed callable/operator evidence，再另开 FunctionAction op 扩展。 |
| SlateBrush equality | UI 类型 equality，当前没有稳定普通 Blueprint graph-body evidence。 |
| Details panel delegate binding、UMG Designer events、Animation Blueprint events | UI/editor-domain 或非普通 Blueprint graph body；不计入 GraphWrite spawn coverage。 |
| Content Browser asset creation、Blueprint component template、WidgetTree design-time nodes | 分别属于 asset lifecycle、component lifecycle、UMG widget lifecycle，不是 Graph body Spawn。 |
| `component.add_component_node` / `UK2Node_AddComponent` | 当前证据仍与 `UBlueprintComponentNodeSpawner`、component class/template、add component template/name 输入绑定，归 component-tool / other-cluster ownership；不得在本轮把它提升为 GraphWrite 可覆盖项。 |
| selected Slate/component state | 可以用 explicit component_ref evidence 取代；不能把“当前选中”作为 TaskSpec 语义来源。 |

## 5. 建议落地顺序

1. 先补 `kind=control` broad control-flow public shape：这是当前最明确的“C++ resolver 边界已有、Agent-facing 没贯通”的缺口。
2. 再拆 Function-backed `convert/create/schedule` operation matrix：让 contract 中已有的 function-backed operation 能被 TaskSpec、runtime、readback 和 final preflight 同步计数。
3. 再做 predicate / special expression nodes：这些需要新的语义 owner，不应混入 Field 或 OpCoverage。
4. 最后把 asset-backed graph node taxonomy 做成可扩展 registry：以 projected ActionDatabase identity + node-family readback 为准，不以菜单文本为准。

## 6. 证据索引

- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:1531-1544`：当前 Graph body statement 支持 `control`，但 `SUPPORTED_GRAPH_BODY_CONTROL_KINDS` 仅为 `branch`、`sequence`、`return`。
- `AgentFaceService/task-core/src/task/compiler/task-compiler.ts:1864-1899`：unsupported control kind 会直接失败并提示只用 `branch`、`sequence`、`return`。
- `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts:236-249`、`:420-438`：GenericOps contract 已列出 switch、multi_gate 与 StandardMacros 控制流。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.cpp:43-63`、`:158-193`：C++ boundary 可识别 dedicated control 与 StandardMacros evidence。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericCreateActionResolver.cpp:189-199`：runtime create resolver 当前只支持 7 个 create operation。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp:390-402`：runtime generic transform/schedule resolver 当前只支持 3 个 transform 与 2 个 schedule operation。
- `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts:340-400`：contract 中 function-backed transform/create/schedule operation 面更宽。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.cpp:95-105`：`enum_equal`、`enum_not_equal`、conversion、container mutation、validity predicate 当前走 excluded op。
- `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.cpp:82-92`：Field 拒绝 UI drag/pin drag、support-only、component/add-component 与 by-ref set。
- `BlueprintHelper/Develop/Evidence/BlueprintHelper_GraphWrite_Field_UEEditorCapability_EngineSourceReadResult_20260525_CN.md:80`、`:151`、`:212`：`component.add_component_node` 仍指向 component template / component-tool ownership，不作为本轮 GraphWrite 候选。
- `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_BroadControlFlowPlan_20260525_CN.md:15-35`：宽控制流计划列出 switch、multi_gate、do_once_multi_input、StandardMacros loop/gate/do/flip-flop。
- `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_ContainerAction_FirstClassPlan_20260525_CN.md:75`：`foreach` 明确归 future control-flow，而非 container_action V1。
- `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md:427`：EventDelegate 排除 action menu、drag menus、Details panel、UMG designer、Animation Blueprint events。
