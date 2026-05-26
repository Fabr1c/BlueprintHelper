# GraphWrite Generality Preflight Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 GraphWrite 最终总测试前新增一个通用性前置门禁：GraphWrite-owned、ownership-filtered 的每个 capability operation 都通过 TaskSpec preview / execute / readback。普通可参数化 operation 生成 10 个同类型不同名节点，10 个全通过才算该 operation 通过；Singleton operation 不要求生成 10 个同类型不同名节点，只要求 1 个代表性 TaskSpec 成功并通过 readback。测试输出带统计图的通用性测试报告。

## 2026-05-25 Current Status Sync

- Status: OPEN / NOT IMPLEMENTED.
- 当前源码树未发现 `graphwrite-generality-*` TaskSpec matrix、spec factory、report writer、PowerShell runner 或 `Run-GraphWriteFinalWithGenerality.ps1` 的实际实现文件；本文件仍是待执行计划。
- 该计划应在 remaining evidence defects、capability contract expansion 完成后统一执行，用于最终能力面验收；`container_action` public shape 已在 `BlueprintHelper_GraphWrite_ContainerAction_FirstClassPlan_20260525_CN.md` 中实现并通过 focused gate，但它不替代本文件的 ownership-filtered 泛化验收。
- 完成标准仍以本文 Exit Criteria 为准，但 operation 数量必须先按 `BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md` 的 ownership filter 重算；原 45 个 normalized operations / 450 个 variants 是未过滤草案，不再作为最终计数口径。最终仍必须覆盖 ownership-filtered operations、按 `variantMode` 生成普通 10 variants / Singleton 1 variant、TaskSpec preview/execute/readback、JSON/CSV/Markdown/SVG report、`allOperationsPassed=true` final gate。

## 2026-05-26 Capability Cluster Sync

本次只更新通用性测试文档的输入口径，不表示 `graphwrite-generality-*` runner / report writer 已实现。最新能力簇来源以 `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts` 与 `AgentFaceService/docs/TaskSpec_UE_Editor_Capability_Matrix_20260521_CN.md` 的 2026-05-26 sections 为准。

当前机器可枚举合同计数：

| Source | Supported | Rejected / gated | Notes |
|---|---:|---:|---|
| Core clusters | 67 | 2 discussion-gated | `container_action` 已是 58 行 canonical owner；`field.struct_member_set` 归 Field。 |
| Logical operation groups | 72 | 13 rejected | `generic_ops.container.*`、`generic_ops.schedule.*`、`generic_ops.create.asset_action`、`op_coverage.array_identical`、`generic_ops.struct_select.set_fields_in_struct` 已删除。 |
| Contract supported rows before core-shape expansion | 139 | 15 | 这是 contract row 计数；最终 scored matrix 还要按 core-shape expansion 与去重规则生成。 |

| Capability source | Operation namespace | Runtime owner rule | Generality treatment |
|---|---|---|---|
| Stable runtime clusters | `function_action`、`field`、`event`、`asset_action`、`container_action`、`generic_schedule` | 使用 `GRAPHWRITE_CAPABILITY_CONTRACT.clusters` 的 supported operations。 | 作为 core matrix 种子；cluster contract 较粗的 `function_action` / `field` 需要展开成本文定义的具体 TaskSpec shapes。 |
| EventDelegate use-site | `event_delegate.*` | `EventDelegateActionCluster`；只消费已有 component / delegate / handler / signature evidence。 | supported operations 进入 10-variant preflight；`replace` / `merge` duplicate policy 等 rejected rows 只进入负例与排除报告。 |
| GenericOps logical groups | `generic_ops.control.*`、`generic_ops.transform.*`、`generic_ops.create.*`、`generic_ops.struct_select.*` | GenericOps 不是 runtime cluster，也不是 top-level `kind`；清理后不再包含 container、schedule、asset_action、function-backed 子集或 `set_fields_in_struct` 索引。 | supported rows 必须按 `variantMode` 生成普通 10 variants 或 Singleton 1 variant；`link_time_auto_conversion`、`split_pin`、`recombine_pin` 等 rejected rows 不计入通过率。 |
| OpCoverage logical group | `op.<operation>` / contract group `op_coverage` | `op` 仍是 Graph body expression semantic；FunctionAction-owned，不新增 `graphwrite_op` cluster。 | 当前 contract-supported OpCoverage 为 37 个 P0/P1 row；旧 `array_identical` P2 行已删除并归 `container.array.identical`。既有 TypePromotion 10 项只作为 core-shape expansion 进入 matrix。 |

硬边界保持不变：

- `TaskSpec` 是唯一全局上下文；每个 `statement[]` 必须能独立解释自己的 operation、type/pin/function/asset/handler evidence。
- 不允许把右键菜单、拖拽节点、拖拽 Pin、Slate selection、Content Browser/MyBlueprint/SCS 当前选中态作为可执行输入。
- 通用性测试只计 TaskSpec preview / execute / readback；ActionResolution focused test 与 capability matrix verification 只能作为前置证据，不能替代普通 10-variant / Singleton 1-variant final gate。

## 2026-05-25 Stability Closure Input

本节记录 GraphWrite stability closure 已完成的 focused gates，作为后续实现本 generality preflight 的输入证据；它不替代本文 ownership-filtered 泛化验收。

| Cluster / Gate | Focused status | Evidence |
|---|---|---|
| function_action + field | PASS | Focused gate id `BlueprintHelper.GraphWrite.FunctionFieldUnifiedSmoke`; covered by the full `Automation RunTests BlueprintHelper.GraphWrite` suite. |
| event taxonomy | PASS | Focused gate id `BlueprintHelper.GraphWrite.EventTaxonomy`; covered by the full `Automation RunTests BlueprintHelper.GraphWrite` suite. |
| asset_action | PASS | `Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.AssetAction`; `Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.AssetActionNoSyntheticSpawner`; weak query/node-class execute selectors rejected. |
| Review evidence | PASS | `Automation RunTests BlueprintHelper.Review.Producer.ClusterBuildsProducerOwnedEvidence`; `Automation RunTests BlueprintHelper.TaskRuntime.PostIO`。 |
| legacy parsed-plan removal | PASS | `Automation RunTests BlueprintHelper.GraphWrite.LegacyMainline`; `rg -n "parsed_node_plan_unsupported|FBlueprintGraphMutationPlan|FBlueprintGraphMutationNodePlan|FBlueprintGraphMutationLinkPlan|MakeNodePlanFromParsedNode|MakeLinkPlanFromParsedLink" BlueprintHelper/Source/BlueprintHelper` 仅命中 contract test 禁止词。 |
| container_action V1 | PASS | `Automation RunTests BlueprintHelper.GraphWrite.ContainerAction`；first-class TaskSpec public shape、C++ contract validation、FunctionAction-backed resolver、fragment role links、typed readback verifier、array-shaped result-symbol DAG metadata、endpoint pin_type JSON round-trip、array/map/set focused E2E 已落地。 |
| generic_schedule | PASS | `timer_delegate_node` / `latent_or_async_node` success path 已落地；focused gates 为 `Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule` 和 `Automation RunTests BlueprintHelper.GraphWrite.GenericSchedule`。最终 preflight 仍需生成 10-variant ownership-filtered TaskSpec/readback coverage。 |
| GenericOps logical matrix | PASS | 2026-05-26 cleanup 后 GenericOps 只保留 control / transform / create / struct_select supported rows；最终 preflight 仍需按当前 `generic_ops.*` supported rows 生成普通 10 variants 或 Singleton 1 variant。 |
| OpCoverage logical group | PASS | 2026-05-26 cleanup 后 OpCoverage supported contract 为 37 个 P0/P1 row；`array_identical` 不再是 OpCoverage row，最终 preflight 通过 `container.array.identical` 覆盖。TypePromotion 10 项作为 core-shape expansion 覆盖。 |
| EventDelegate use-site matrix | PASS | 2026-05-26 matrix update 记录 `BlueprintHelper.GraphWrite.EventDelegate` 9/9、`ActionResolution.EventDelegate` 16/16、`GraphStatement.EventDelegate` 6/6、`ToolResult.EventDelegate` 1/1 通过；最终 preflight 只消费已有 handler/signature evidence，不创建声明。 |
| full GraphWrite suite | FAIL CURRENT RUN | 2026-05-26 当前 FullTest 已执行；`Automation RunTests BlueprintHelper.GraphWrite` 返回 exit code `255`，日志解析为 `282/304` automation leaf 成功、`22/304` 失败，未生成 `index.json` 报告。 |
| full generality preflight | PENDING | 本文件下方的 matrix、factory、runner、report writer 仍未实现，不能标记 stable final。 |

## 2026-05-26 FullTest Current Run Evidence

本节覆盖本轮 FullTest 的当前证据；它不改变上方 capability contract denominator，只记录总测 gate 状态。

| Item | Result |
|---|---|
| Command | `E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UEProjects\Template\Template.uproject -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite;Quit" -TestExit="Automation Test Queue Empty" -ReportOutputPath="D:\UEProjects\Template\Saved\Automation\GraphWrite_FullTest_GeneralityPreflightDoc_20260526_001"` |
| Exit code | `255` |
| Parsed automation leaf total | `304` |
| Parsed automation leaf success | `282` |
| Parsed automation leaf failure | `22` |
| Automation pass rate | `92.76%` |
| Report path | `D:\UEProjects\Template\Saved\Automation\GraphWrite_FullTest_GeneralityPreflightDoc_20260526_001` |
| `index.json` | Missing; the report directory was not created for this failed run. |
| Source log | `D:\UEProjects\Template\Saved\Logs\Template.log` |
| Gate conclusion | FAIL. Current FullTest blocks marking GraphWrite generality/final gate complete. |

Current failing automation leaves:

| Failure group | Count | Failing leaves |
|---|---:|---|
| Append ownership/signature reuse | 2 | `Append.OwnershipWritesMetadataWithoutManagedComment`; `Append.ReusesSignatureEntry` |
| BlockScopedAnchors merge/insert flow | 7 | `BlockScopedAnchors.MergeBranchForkOwnedBlockCall`; `BlockScopedAnchors.MergeBranchForkUncompiledOwnedBlockCall`; `BlockScopedAnchors.MergeInsertBetween`; `BlockScopedAnchors.MergeInsertFlow`; `BlockScopedAnchors.MergeInsertFlowCustomEventCall`; `BlockScopedAnchors.MergeInsertFlowCustomEventCallDryRun`; `BlockScopedAnchors.MergeInsertFlowDisplayNameFunctionCall` |
| CallFunctionResolver | 4 | `CallFunctionResolver.Stress.BlueprintAuthoredInheritedFunctionGraphGenerationSpawns`; `CallFunctionResolver.ExecuteRevalidatesStableId`; `CallFunctionResolver.GeneratorDisplayNameSpawnsPrintString`; `CallFunctionResolver.GeneratorQualifiedNameSpawnsPrintString` |
| FunctionField unified smoke | 1 | `FunctionFieldUnifiedSmoke.CallFragmentSemanticKindOwnership` |
| LegacyMainline contract | 2 | `LegacyMainline.ActiveGraphWriteSourceLegacyTokenGate`; `LegacyMainline.EventDelegateDeclaredCapabilityMatchesSuccessPath` |
| Replace / TaskRuntime readback | 6 | `Replace.CustomEventBodyReconnectsEntryExec`; `TaskRuntime.CallFunction.DisplayNameReadBack`; `TaskRuntime.CallFunction.QualifiedNameReadBack`; `TaskRuntime.Merge.BranchForkOwnedBlockCallReadBack`; `TaskRuntime.P6.EvidenceBackedReadbackCoverage`; `TaskRuntime.Replace.CustomEventBodyReconnectsEntryExec` |

Observed primary blockers from the log:

- `FunctionFieldUnifiedSmoke.CallFragmentSemanticKindOwnership`: registry did not build the expected call fragment with literal args.
- `LegacyMainline.ActiveGraphWriteSourceLegacyTokenGate`: active source scan still sees forbidden wide-surface fallback tokens `make_struct` and `ref`.
- `LegacyMainline.EventDelegateDeclaredCapabilityMatchesSuccessPath`: `BlueprintHelperEventDelegateActionCluster.cpp` is missing the required current P5 event/delegate boundary evidence token.
- Replace/custom-event body and runtime readback tests fail on execute status, event-to-PrintString exec links, ownership metadata, or readback evidence.
- Branch-fork runtime readback still reports partial failure and cannot confirm sequence/call nodes.

## 2026-05-25 Ownership Filter Update

本计划的 operation matrix 必须先排除已有工具职责重合项，再作为 GraphWrite 最终验收矩阵：

| Category | Treatment in preflight |
|---|---|
| `BlueprintSignature` declarations: custom/override/native event declaration, function signature, event dispatcher, handler declaration | 作为 fixture/dependency；不计入 GraphWrite operation 成败。 |
| Delegate/component handler lookup or declaration creation | 作为 Signature/ActionContext projection 前置条件；GraphWrite 缺 evidence 时应返回 deterministic diagnostic。 |
| EventDelegate use-site: `component_bound_event`、`delegate bind/assign/unbind/clear/call` | 只在已有声明/evidence 完整时作为 use-site graph writing 验证；不把声明缺口算作 GraphWrite failure。 |
| Merge/Patch/ConnectPins mutation ownership | 不计入 Spawner-Oriented GraphStatement preflight，除非后续明确迁入 GraphStatement 主线。 |
| Evidence-heavy Generic paths | `type_promotion` 的 TaskSpec passthrough 与 resolver consumption 已存在，作为 final preflight coverage；`asset_action` Review policy 已收窄为 graph-level `graph_block`，由 `BlueprintHelper_GraphWrite_AssetActionReviewPolicy_GraphBlockPlan_20260525_CN.md` 固化，不进入 action-level Review target；`timer_delegate_node`、`latent_or_async_node` 已保留并落地为 GraphWrite Generic schedule success path，后续 preflight 只需按 projected evidence fixture 生成泛化用例。 |
| `anim_notify_event` | 归 Animation Blueprint / Animation tooling；当前 GraphWrite 只关注普通 Blueprint，不进入最终矩阵。 |
| broad `container_action` | `container_action` 已是唯一 container denominator，当前 contract 为 58 个 supported row；`make_*` 仍归 Generic create，`foreach` 归后续 control-flow。focused readback 已覆盖 wildcard 被替换成目标类型、target link 正确、编译无报错；最终矩阵仍需按 owned operation 的 `variantMode` 生成普通 10 variants 或 Singleton 1 variant。 |
| FunctionAction overlap | 容器操作会与 FunctionAction 高度重叠；最终矩阵前必须明确哪些作为普通 callable 计入 FunctionAction，哪些需要 first-class `container_action` 语义。 |
| GenericOps logical umbrella | `generic_ops.*` 只作为 logical operation catalog；不新增 runtime cluster，不扩展 top-level `kind`。清理后不再包含 container、schedule、asset_action、function-backed 子集或 struct member set duplicate row。最终矩阵按 contract `runtimeOwner` 路由，control/transform/create/struct_select 归 GenericAssetStructControlAction。 |
| OpCoverage logical group | `op_coverage` 不发布 `graphwrite_op` cluster；P0/P1 compact callable op 按 FunctionAction-owned `op.*` 进入矩阵；`array_identical` 已从 OpCoverage 删除并归 `container.array.identical`。excluded op 只进入 deterministic negative coverage。 |
| UI/editor interaction inputs | Action Menu、右键、拖拽、拖拽 Pin、Slate selection、Content Browser/MyBlueprint/SCS 当前选中态一律不计入 supported operation。需要等价能力时必须替换为 statement-local typed pin、function/class/asset path、handler/signature evidence。 |
| capability contract expansion | 2026-05-26 contract 已包含 `clusters` 与 `operationGroups` 双层来源；最终矩阵必须从 machine-readable contract 生成并保留手写 core-shape expansion，不能继续使用旧 45 项草案。 |
| ownership-filtered final generality preflight | 放到 capability contract expansion 之后执行，作为最终门禁。 |

**Architecture:** 通用性测试以 Agent-facing TaskSpec 为唯一计分入口，执行链路必须经过 `TaskSpec -> compiler lowering -> TaskPlan graph_write -> UE GraphWrite runtime -> graph readback`。Operation 矩阵是数据驱动的公共测试目录；fixture/setup 负责创建资产、变量、组件、签名和 handler 证据，但不计入 GraphWrite 正确率。报告生成器消费每个 operation 的 required variants 结果，输出 JSON/CSV/Markdown/SVG，并作为最终 80% 总测的前置 gate。

**Tech Stack:** TypeScript task-core/CLI, BlueprintHelper TaskSpec v1, UE 5.6 GraphWrite runtime, PowerShell orchestration, JSON/CSV/Markdown/SVG reports.

---

## Scope And Boundary

本计划新增的是“最终总测前置通用性测试”，不是替代 P0-P6 的能力测试。P0-P6 仍验证复杂需求正确率；本前置测试验证每个工具簇 operation 是否能稳定通过其 required variants：普通 operation 验证 10 个同类变体，Singleton operation 验证 1 个代表性成功路径。

计分规则固定如下：

| Rule | Contract |
|---|---|
| Unit under test | 一个 ownership-filtered capability operation id。格式可以是 core shape（如 `call.call`、`field.member_get`、`op.add`），也可以是 logical group id（如 `generic_ops.control.switch_enum`、`event_delegate.delegate.bind`）。 |
| Variant count | 每个 operation 必须声明 `variantMode`。普通可参数化 operation 生成并执行 10 个 variant，命名为 `GWGen_<OperationId>_00` 到 `GWGen_<OperationId>_09`；Singleton operation 只生成并执行 1 个代表性 variant，命名为 `GWGen_<OperationId>_00`。 |
| Singleton rule | Singleton 指同一 graph / asset 中无法通过“同类型不同名节点”获得有意义泛化覆盖的能力，例如 direct singleton control provider、已有签名 body target、唯一 use-site fixture 或其他由 matrix 显式标记的 singleton capability。Singleton 只需 1 次 TaskSpec preview / execute / readback 成功；不得为了凑 10 个节点复制无意义同构节点。 |
| Operation pass | 实际通过 variant 数必须等于该 operation 的 `requiredVariantCount`。普通 operation 为 `10/10`；Singleton operation 为 `1/1`。 |
| Operation fail | 任一 required variant 失败、缺少 readback、返回 unsupported、silent wrong graph、spawn/link/default/pin 错误。 |
| Fixture failure | 资产、组件、变量、Signature dependency、handler 创建失败记录为 `setup_failure`，不计入 GraphWrite 正确率，但该 operation 不能通过前置 gate。 |
| Matrix source | `GRAPHWRITE_CAPABILITY_CONTRACT.clusters`、`GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups`、以及本文保留的 core-shape expansion 共同生成；禁止再使用旧 45 项手写清单作为最终计数。 |
| Direct spawn boundary | `branch`、`sequence`、`return`、`select` 等唯一控制流允许 direct spawn，但仍必须走 `SpawnerClusterKind -> cluster -> semantic constraint -> evidence/provider -> shared spawn adapter`。 |
| Signature boundary | Handler、event dispatcher、custom event、function signature 的声明所有权属于 BlueprintSignature；GraphWrite/EventDelegate 只能消费已有声明/签名证据生成图节点、绑定节点、连接和 body 内容。 |
| Statement-local evidence boundary | `TaskSpec` 是唯一全局上下文；单个 `statement[]` 不得依赖前一条 statement 的隐式 pin/menu/selection context。跨 statement symbol 或 catalog 共享必须先另行设计。 |
| UI exclusion | Action Menu、右键菜单、拖拽、拖拽 Pin、Slate/UMG selection、display name alias 不能作为 supported operation 的执行依据；相关条目只能进入排除或 reference-only 证据。 |
| Score source | 只有 TaskSpec preview/execute/readback 结果能进入通用性分数；ActionResolution 单测只作为定位和契约防回退。 |

## Operation Matrix

实现时以代码化 matrix 作为单一测试目录。2026-05-26 起，最终矩阵不再手写旧 45 项清单，而是由 machine-readable capability contract 加上少量 core-shape expansion 生成。

矩阵生成顺序固定如下：

1. 读取 `GRAPHWRITE_CAPABILITY_CONTRACT.clusters` 中 `supportStatus="supported"` 的 stable runtime operations。
2. 读取 `GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups` 中 `supportStatus="supported"` 的 logical operations，并按每行 `runtimeOwner` 写入 matrix。
3. 补充 contract 粒度较粗、但通用性测试必须拆开的 core shapes：`call.call`、17 个 `field.*` capability IDs、`body.custom_event`、既有 TypePromotion 10 项。
4. 对 `supportStatus="rejected"` / `discussion-gated` rows 生成 exclusion / negative rows，不计入 operation pass rate。
5. 为每个 scored operation 设置 `variantMode` 与 `requiredVariantCount`：默认 `parameterized` / `10`，显式 Singleton 为 `singleton` / `1`。
6. 按 operation id 去重；若同一 id 的 runtime owner、required evidence、support status 或 variant mode 冲突，matrix test 必须失败。

### Core Shape Expansion

这些 operation 仍作为 GraphWrite 核心泛化项存在，因为当前 contract 的 runtime cluster 行粒度比 TaskSpec statement 粒度更粗。

| Operation id seed | Agent-facing shape | Runtime owner | Expected node family |
|---|---|---|---|
| `call.call` | `kind=call` | `FunctionAction` | `K2Node_CallFunction` |
| `field.*` 17 first-class capability IDs | `capability_id=field.member_get` etc.; legacy `kind=set/get` aliases must normalize to one concrete `field.*` id | `Field` | variable get/set, component ref, function/local/struct/object-pin/nested-property fragments |
| `body.custom_event` | existing Signature-owned custom event body | BlueprintSignature body boundary consumed by GraphWrite | body content under existing event entry |

The Field expansion is:

```text
field.member_get, field.member_set, field.local_get, field.local_set, field.component_ref_get,
field.inherited_member_get, field.inherited_member_set, field.sparse_data_get, field.function_param_get,
field.struct_member_get, field.struct_member_set, field.object_pin_member_get, field.object_pin_member_set,
field.component_ref_set, field.component_property_get, field.component_property_set, field.nested_property_path
```

Field-like UI/support/other-cluster/diagnostic rows such as `field.drag_get`、`field.pin_drag_set`、`field.split_struct_pin_support`、`component.add_component_node`、`field.by_ref_set` must be excluded or routed to their owning boundary.

### Runtime Contract Cluster Seeds

| Contract cluster | Included operations | Notes |
|---|---|---|
| `function_action` | `call_function`、`macro_like` plus core shape `call.call` | Function-like statements resolve through ActionContext and shared action adapters. |
| `field` | `field_access`、`component_ref` plus core field get/set expansion | First-class Field owns stable `field.capability_id` / resolver / evidence / readback paths; Field-specific facts do not move into GenericOps. |
| `event` | `custom_event` supported; override/native and delegate-bound entries remain discussion-gated | Declaration creation stays outside EventDelegate use-site scoring. |
| `asset_action` | `create.asset_action` | Requires projected ActionDatabase identity; no Content Browser selected state. |
| `container_action` | 58 个 `container.array.*` / `container.map.*` / `container.set.*` rows from contract cluster | `container_action` 是唯一 container denominator；不再存在 `generic_ops.container.*` 公共索引。 |
| `generic_schedule` | `schedule.timer_delegate_node`、`schedule.latent_or_async_node` | Requires selected schedule spawner evidence; latent nodes require `graph_latent_allowed`; timer delegate nodes require existing handler/signature evidence. |

### EventDelegate Use-Site Operations

All supported rows in contract group `event_delegate` enter the preflight:

```text
event_delegate.component_bound_event
event_delegate.delegate.bind
event_delegate.delegate.assign
event_delegate.delegate.unbind
event_delegate.delegate.call
event_delegate.delegate.clear
```

Rejected EventDelegate rows such as `event_delegate.component_bound_duplicate_policy.replace` and `event_delegate.component_bound_duplicate_policy.merge` must be reported as excluded / deterministic negative coverage. EventDelegate never creates event declarations, custom events, handlers, dispatcher declarations, or handler signature mutations.

### GenericOps Logical Operations

GenericOps is a public logical umbrella, not a runtime cluster. Every supported row below is scored by TaskSpec preview/execute/readback, but runtime execution still routes through its contract owner.

| Logical group | Operation id families | Runtime owner |
|---|---|---|
| `generic_ops.control` | `branch`、`sequence`、`return`、`switch_int`、`switch_string`、`switch_name`、`switch_enum`、`multi_gate`、`do_once`、`do_n`、`gate`、`flip_flop`、`for_loop`、`for_loop_with_break`、`foreach_loop`、`foreach_loop_with_break`、`while_loop` | `GenericAssetStructControlAction` |
| `generic_ops.transform` | `dynamic_cast`、`class_cast`、`type_promotion` | `GenericAssetStructControlAction` |
| `generic_ops.create` | `spawn_actor`、`create_widget`、`construct_object`、`make_array`、`make_map`、`make_set` | `GenericAssetStructControlAction` |
| `generic_ops.struct_select` | `make_struct`、`break_struct`、`select` | `GenericAssetStructControlAction` |

Rejected GenericOps rows such as `generic_ops.transform.link_time_auto_conversion`、`generic_ops.struct_select.split_pin`、`generic_ops.struct_select.recombine_pin` must not enter operation pass rate. They belong to linker/readback or future PinOperation boundaries.

### OpCoverage Operations

OpCoverage remains `FunctionAction` owned and must not publish `graphwrite_op` as a runtime cluster.

| Priority | Operation ids | Expected evidence |
|---|---|---|
| Existing TypePromotion | `add`、`subtract`、`multiply`、`divide`、`greater`、`greater_equal`、`less`、`less_equal`、`equal`、`not_equal` | typed operand/result evidence when required by TypePromotion path |
| P0 commutative callable | `bitwise_and`、`bitwise_or`、`boolean_and`、`boolean_or`、`boolean_nand`、`max`、`min`、`string_append` | `op.operation_id` plus stable callable evidence projected by catalog |
| P1 compact call-function | `boolean_not`、`boolean_xor`、`boolean_nor`、`bitwise_not`、`bitwise_xor`、`abs`、`modulo`、`negate`、`dot`、`dot3`、`cross`、`cross3`、`near_equal`、`intpoint_equal`、`transform_compose`、`equal_exact`、`not_equal_exact`、`equal_ignore_case`、`not_equal_ignore_case`、`datetime_add_datetime`、`datetime_add_timespan`、`datetime_subtract_datetime`、`datetime_subtract_timespan`、`datetime_equal`、`datetime_not_equal`、`datetime_greater`、`datetime_greater_equal`、`datetime_less`、`datetime_less_equal` | `op.operation_id` plus stable callable evidence projected by catalog |
| Removed duplicate | `array_identical` | Not an OpCoverage row; score through `container.array.identical`. |

Excluded OpCoverage inputs are `enum_equal`、`enum_not_equal`、SlateBrush equality、`convert_numeric`、`convert_string_text_name`、`array_map_set_mutation`、`validity_predicate`。They must reject deterministically or route to their owning future capability, not silently count as skipped.

The matrix must not silently skip an operation. If a supported operation is still unsupported in runtime, the preflight records `unsupported_intent` and fails that operation. Only values absent from the public GraphWrite contract or explicitly marked `rejected` / `discussion-gated` may be excluded from the score.

## File Structure

- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
  - Owns the machine-readable GraphWrite runtime clusters and logical operation groups. The generality matrix must read from this contract instead of duplicating capability rows.
- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
  - Owns public TaskSpec shape validation and exported operation constants such as `CONTAINER_ACTION_OPERATION_IDS` / `OP_COVERAGE_SUPPORTED_OPERATION_IDS`.
- Create: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.ts`
  - Single generated source for operation ids, public shapes, normalized operations, variant count, fixture requirements, expected readback signatures, runtime owner, support status, and exclusion reason.
- Create: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-spec-factory.ts`
  - Builds fixture TaskSpecs and operation TaskSpecs. It is pure data generation and does not call UE.
- Create: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-report.ts`
  - Aggregates per-variant results into JSON/CSV/Markdown/SVG report artifacts.
- Create: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.test.ts`
  - Node tests for matrix completeness, 10-variant generation, compiler lowering, and report math.
- Create: `BlueprintHelper/Develop/Scripts/Run-GraphWriteGeneralityPreflight.ps1`
  - Orchestrates CLI preview/execute/readback per operation and writes a raw run folder.
- Create: `BlueprintHelper/Develop/Scripts/Run-GraphWriteFinalWithGenerality.ps1`
  - Runs the preflight first; only runs the final P6/80% suite when the preflight summary passes.
- Create: `BlueprintHelper/Develop/Report/BlueprintHelper_GraphWrite_GeneralityPreflight_Report_<date>_CN.md`
  - Generated report target. The directory may be created by the report writer.
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`
  - Add a final-test gate row pointing to the latest generality report.

## Manual Commit Policy

Agents executing this plan must not run `git add`, `git commit`, or `git push`. At each checkpoint, record the intended file list and suggested commit message for the user to execute manually.

## Task 1: Align Public Capability Contract With Generality Matrix Inputs

**Files:**
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
- Test: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.test.ts`

> 2026-05-26 correction: do not use `task-contract.ts.supported_first_slice` as the operation matrix source. That file only describes a first TaskSpec slice; the generality preflight must read the full GraphWrite capability surface from `GRAPHWRITE_CAPABILITY_CONTRACT.clusters` and `GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups`.

- [ ] **Step 1: Add contract-source tests**

Add tests that assert:

```text
GRAPHWRITE_CAPABILITY_CONTRACT.clusters contains function_action, field, event, asset_action, container_action, generic_schedule.
GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups contains event_delegate, generic_ops.control, generic_ops.transform, generic_ops.create, generic_ops.struct_select, op_coverage.
No runtime cluster id is graphwrite_op, generic_ops, control, generic_transform, generic_create, struct_select, or generic_op.
Every supported operation group row has runtimeOwner and requiredEvidenceKeys.
Every rejected / discussion-gated row has a deterministic exclusion reason and is not counted as a scored operation.
```

- [ ] **Step 2: Add shape-expansion tests**

The matrix must add core-shape rows that are intentionally more specific than the contract cluster rows:

```text
call.call
field.member_get / field.member_set and the rest of the 17 first-class field.* capability IDs
body.custom_event as a BlueprintSignature-owned body target, not a runtime cluster
op.add/subtract/multiply/divide/greater/greater_equal/less/less_equal/equal/not_equal
```

Field rows should use the 17 first-class Field capability IDs from `BlueprintHelper_GraphWrite_Field_FirstClassCapabilityExpansionPlan_20260526_CN.md`; older `field.get.variable` / `field.set.property_path` labels may remain as compatibility aliases only if the matrix maps them to a concrete `field.*` capability id.

- [ ] **Step 3: Re-run the contract tests**

Run:

```powershell
Push-Location AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node
Pop-Location
```

Expected after this task: the generality matrix tests prove their source is the capability contract plus explicit core-shape expansion, not a stale hard-coded operation count.

## Task 2: Add The Operation Matrix

**Files:**
- Create: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.ts`
- Modify: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.test.ts`

> 2026-05-26 correction: this task is contract-derived. Do not implement a fixed 45-operation array. The matrix module should generate rows from `GRAPHWRITE_CAPABILITY_CONTRACT`, then append only the documented core-shape expansion rows. Any old hard-coded example in this section must be treated as illustrative structure only, not as the operation list.

- [ ] **Step 1: Create contract-derived matrix types and constants**

Create `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.ts` as a contract-derived builder, not as a literal operation array:

```ts
import {
  GRAPHWRITE_CAPABILITY_CONTRACT,
  type GraphWriteCapabilityContract,
  type GraphWriteOperationContract,
  type GraphWriteOperationGroupOperation,
  type GraphWriteSupportStatus,
} from '../schema/graphwrite-capability-contract.js';

export const GRAPHWRITE_GENERALITY_DEFAULT_VARIANT_COUNT = 10;
export const GRAPHWRITE_GENERALITY_SINGLETON_VARIANT_COUNT = 1;

export type GraphWriteGeneralityVariantMode = 'parameterized' | 'singleton';

export type GraphWriteGeneralityBoundary =
  | 'FunctionActionCluster'
  | 'FieldVariableActionCluster'
  | 'EventDelegateActionCluster'
  | 'GenericAssetStructControlActionCluster'
  | 'BlueprintSignatureBodyBoundary';

export type GraphWriteGeneralityMatrixSource = 'cluster' | 'operation_group' | 'core_shape';

export interface GraphWriteGeneralityOperationCase {
  operationId: string;
  owner: 'graph_write';
  source: GraphWriteGeneralityMatrixSource;
  supportStatus: Extract<GraphWriteSupportStatus, 'supported' | 'rejected' | 'discussion-gated'>;
  runtimeBoundary: GraphWriteGeneralityBoundary;
  publicShape: string;
  requiredEvidenceKeys: readonly string[];
  expectedReadback: string;
  excludedReason?: string;
  variantMode: GraphWriteGeneralityVariantMode;
  variantCount: number;
}

function matrixCase(input: Omit<GraphWriteGeneralityOperationCase, 'owner' | 'variantCount' | 'variantMode'> & {
  variantMode?: GraphWriteGeneralityVariantMode;
}): GraphWriteGeneralityOperationCase {
  const variantMode = input.variantMode ?? 'parameterized';
  return {
    owner: 'graph_write',
    variantMode,
    variantCount: variantMode === 'singleton'
      ? GRAPHWRITE_GENERALITY_SINGLETON_VARIANT_COUNT
      : GRAPHWRITE_GENERALITY_DEFAULT_VARIANT_COUNT,
    ...input,
  };
}

export const FIELD_CORE_CAPABILITY_IDS = [
  'field.member_get',
  'field.member_set',
  'field.local_get',
  'field.local_set',
  'field.component_ref_get',
  'field.inherited_member_get',
  'field.inherited_member_set',
  'field.sparse_data_get',
  'field.function_param_get',
  'field.struct_member_get',
  'field.struct_member_set',
  'field.object_pin_member_get',
  'field.object_pin_member_set',
  'field.component_ref_set',
  'field.component_property_get',
  'field.component_property_set',
  'field.nested_property_path',
] as const;

export const TYPE_PROMOTION_CORE_OPERATION_IDS = [
  'op.add',
  'op.subtract',
  'op.multiply',
  'op.divide',
  'op.greater',
  'op.greater_equal',
  'op.less',
  'op.less_equal',
  'op.equal',
  'op.not_equal',
] as const;

export const GRAPHWRITE_GENERALITY_CORE_SHAPE_EXPANSION = [
  matrixCase({
    operationId: 'call.call',
    source: 'core_shape',
    supportStatus: 'supported',
    runtimeBoundary: 'FunctionActionCluster',
    publicShape: 'kind=call',
    requiredEvidenceKeys: [],
    expectedReadback: 'K2Node_CallFunction',
  }),
  ...FIELD_CORE_CAPABILITY_IDS.map((operationId) =>
    matrixCase({
      operationId,
      source: 'core_shape',
      supportStatus: 'supported',
      runtimeBoundary: 'FieldVariableActionCluster',
      publicShape: `capability_id=${operationId}`,
      requiredEvidenceKeys: ['field.capability_id', 'field.capability_facts'],
      expectedReadback: 'Field readback facts',
    }),
  ),
  matrixCase({
    operationId: 'body.custom_event',
    source: 'core_shape',
    supportStatus: 'supported',
    runtimeBoundary: 'BlueprintSignatureBodyBoundary',
    publicShape: 'existing BlueprintSignature entry body',
    requiredEvidenceKeys: ['blueprint_signature.entry_evidence_id'],
    expectedReadback: 'body content under existing K2Node_CustomEvent',
    variantMode: 'singleton',
  }),
  ...TYPE_PROMOTION_CORE_OPERATION_IDS.map((operationId) =>
    matrixCase({
      operationId,
      source: 'core_shape',
      supportStatus: 'supported',
      runtimeBoundary: 'FunctionActionCluster',
      publicShape: `kind=op, op=${operationId.slice('op.'.length)}`,
      requiredEvidenceKeys: ['op.operation_id'],
      expectedReadback: 'K2Node_PromotableOperator',
    }),
  ),
] as const;

export const GRAPHWRITE_GENERALITY_OPERATION_MATRIX = buildGraphWriteGeneralityMatrix({
  capabilityContract: GRAPHWRITE_CAPABILITY_CONTRACT,
  coreShapeExpansion: GRAPHWRITE_GENERALITY_CORE_SHAPE_EXPANSION,
});
```

The builder must:

- include all supported `clusters` operations and all supported `operationGroups` operations;
- map `runtimeOwner` to the four current runtime clusters or the BlueprintSignature body boundary;
- attach required evidence keys and exclusion reasons from the contract;
- exclude `rejected` / `discussion-gated` rows from scored pass-rate math while preserving them in the report;
- fail tests if an operation appears twice with conflicting owner, evidence, or support status.

- [ ] **Step 2: Add matrix integrity tests**

Append these tests to `graphwrite-generality-matrix.test.ts`:

```ts
import { GRAPHWRITE_GENERALITY_OPERATION_MATRIX } from './graphwrite-generality-matrix.js';

test('GraphWrite generality matrix has stable unique operation ids and required variant counts per operation', () => {
  const ids = GRAPHWRITE_GENERALITY_OPERATION_MATRIX.map((entry) => entry.operationId);
  assert.equal(ids.length, new Set(ids).size);
  assert.ok(ids.length > 0);
  assert.ok(GRAPHWRITE_GENERALITY_OPERATION_MATRIX.every((entry) => entry.owner === 'graph_write'));
  for (const entry of GRAPHWRITE_GENERALITY_OPERATION_MATRIX) {
    assert.equal(entry.variantCount, entry.variantMode === 'singleton' ? 1 : 10, entry.operationId);
    assert.match(entry.operationId, /^[a-z0-9_]+(\.[a-z0-9_]+)+$/);
    assert.ok(entry.expectedReadback.length > 0, entry.operationId);
    if (entry.supportStatus !== 'supported') {
      assert.ok(entry.excludedReason, entry.operationId);
    }
  }
});

test('GraphWrite generality matrix is derived from current capability groups', () => {
  const ids = new Set(GRAPHWRITE_GENERALITY_OPERATION_MATRIX.map((entry) => entry.operationId));
  assert.ok(ids.has('field.member_get'));
  assert.ok(ids.has('event_delegate.delegate.bind'));
  assert.ok(ids.has('generic_ops.control.switch_enum'));
  assert.ok(ids.has('container_action.container.array.identical') || ids.has('container.array.identical'));
  assert.ok(ids.has('field.struct_member_set'));
  assert.ok(!ids.has('generic_ops.container.array.identical'));
  assert.ok(!ids.has('generic_ops.struct_select.set_fields_in_struct'));
  assert.ok(!ids.has('op_coverage.array_identical') && !ids.has('op.array_identical') && !ids.has('array_identical'));
  assert.ok(!ids.has('field.set.variable'));
  assert.ok(!ids.has('component_bound_event.bind'));
  assert.ok(!ids.has('delegate.bind'));
});
```

- [ ] **Step 3: Run matrix tests**

Run:

```powershell
Push-Location AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node
Pop-Location
```

Expected: matrix tests pass after Step 1 and Step 2.

## Task 3: Build Ten-Variant TaskSpec Factory

**Files:**
- Create: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-spec-factory.ts`
- Modify: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.test.ts`

> 2026-05-26 correction: factory generation must branch on the matrix row's capability namespace and runtime owner, not on the old hand-written `kind.operation` prefixes alone. `field.*` rows use first-class Field capability IDs, `event_delegate.*` rows consume handler/signature evidence, `generic_ops.*` rows use their contract owner, and `op_coverage` rows emit statement-local `op.*` evidence. The snippets below remain implementation scaffolding and must be adapted to the generated matrix shape.

- [ ] **Step 1: Add TaskSpec factory types**

Create `AgentFaceService/task-core/src/task/testing/graphwrite-generality-spec-factory.ts`:

```ts
import type { TaskSpec } from '../schema/task-schemas.js';
import {
  GRAPHWRITE_GENERALITY_OPERATION_MATRIX,
  type GraphWriteGeneralityOperationCase,
} from './graphwrite-generality-matrix.js';

export interface GraphWriteGeneralitySpecBundle {
  operation: GraphWriteGeneralityOperationCase;
  fixtureSpecs: TaskSpec[];
  graphWriteSpec: TaskSpec;
  expectedVariantNames: string[];
}

export interface GraphWriteGeneralitySpecInput {
  assetPath: string;
  graphName: string;
  operationId: string;
}

export function makeGraphWriteGeneralityBundles(input: Omit<GraphWriteGeneralitySpecInput, 'operationId'>): GraphWriteGeneralitySpecBundle[] {
  return GRAPHWRITE_GENERALITY_OPERATION_MATRIX.map((operation) => makeGraphWriteGeneralityBundle({
    ...input,
    operationId: operation.operationId,
  }));
}

export function makeGraphWriteGeneralityBundle(input: GraphWriteGeneralitySpecInput): GraphWriteGeneralitySpecBundle {
  const operation = GRAPHWRITE_GENERALITY_OPERATION_MATRIX.find((entry) => entry.operationId === input.operationId);
  if (!operation) {
    throw new Error(`Unknown GraphWrite generality operation: ${input.operationId}`);
  }
  const expectedVariantNames = Array.from({ length: operation.variantCount }, (_, index) => variantName(operation.operationId, index));
  return {
    operation,
    fixtureSpecs: makeFixtureSpecs(input.assetPath, input.graphName, operation, expectedVariantNames),
    graphWriteSpec: makeOperationSpec(input.assetPath, input.graphName, operation, expectedVariantNames),
    expectedVariantNames,
  };
}

export function variantName(operationId: string, index: number): string {
  return `GWGen_${operationId.replace(/[^a-z0-9]+/gi, '_')}_${String(index).padStart(2, '0')}`;
}
```

- [ ] **Step 2: Add fixture spec generation**

Append fixture generation to the same file:

```ts
function makeFixtureSpecs(
  assetPath: string,
  graphName: string,
  operation: GraphWriteGeneralityOperationCase,
  names: string[],
): TaskSpec[] {
  const specs: TaskSpec[] = [];
  if (operation.fixtures.includes('actor_blueprint')) {
    specs.push(makeCreateActorBlueprintSpec(assetPath));
  }
  if (operation.fixtures.includes('variables')) {
    specs.push(makeVariableFixtureSpec(assetPath, names));
  }
  if (operation.fixtures.includes('components')) {
    specs.push(makeComponentFixtureSpec(assetPath, names));
  }
  if (operation.fixtures.includes('custom_events') || operation.fixtures.includes('delegate_handlers')) {
    specs.push(makeSignatureFixtureSpec(assetPath, graphName, operation, names));
  }
  if (operation.fixtures.includes('event_dispatchers')) {
    specs.push(makeDispatcherFixtureSpec(assetPath, names));
  }
  return specs;
}

function makeCreateActorBlueprintSpec(assetPath: string): TaskSpec {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'create_asset',
    feature_name: 'GraphWriteGeneralityFixtureAsset',
    target: { asset_path: assetPath },
    behavior: {
      asset_strategy: 'ensure_asset',
      asset: {
        asset_type: 'blueprint_class',
        parent_class: '/Script/Engine.Actor',
        collision_policy: 'reuse_existing',
      },
    },
    execution_policy: { dry_run_mode: 'preview_required' },
    validation: { should_compile: true, should_save: true },
  } as TaskSpec;
}

function makeVariableFixtureSpec(assetPath: string, names: string[]): TaskSpec {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_variables',
    feature_name: 'GraphWriteGeneralityVariables',
    target: { asset_path: assetPath },
    behavior: {
      variable_strategy: 'member_variables',
      changes: names.map((name, index) => ({
        kind: 'ensure_member_variable',
        name,
        type: index % 2 === 0 ? 'float' : 'bool',
        default_value: index % 2 === 0 ? index : false,
      })),
    },
    execution_policy: { dry_run_mode: 'preview_required' },
    validation: { should_compile: true, should_save: true },
  } as TaskSpec;
}
```

- [ ] **Step 3: Add capability-namespace body generation**

Append operation generation to the same file. The dispatcher must branch by current capability namespace and `runtimeBoundary`, not by old bare operation prefixes:

```ts
function makeOperationSpec(
  assetPath: string,
  graphName: string,
  operation: GraphWriteGeneralityOperationCase,
  names: string[],
): TaskSpec {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    feature_name: `GraphWriteGenerality_${operation.operationId.replaceAll('.', '_')}`,
    target: { asset_path: assetPath },
    scope_policy: {
      graph_name: graphName,
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'replace_owned_graph',
      replace: {
        replace_scope: 'custom_event_body',
        selector: { kind: 'custom_event', event_name: 'GWGen_RunGeneralityPreflight' },
        body: {
          statements: names.map((name, index) => makeStatementForOperation(operation, name, index)),
        },
        options: { strict: true, preserve_layout: false },
      },
    },
    execution_policy: { dry_run_mode: 'preview_required' },
    validation: { should_compile: true, should_save: true },
  } as TaskSpec;
}

function makeStatementForOperation(operation: GraphWriteGeneralityOperationCase, name: string, index: number): Record<string, unknown> {
  if (operation.supportStatus !== 'supported') {
    return makeExpectedUnsupportedStatement(operation, name);
  }
  if (operation.operationId === 'call.call') {
    return { id: name, kind: 'call', target: 'PrintString', args: { InString: `${name}_value` } };
  }
  if (operation.operationId.startsWith('field.')) {
    return makeFieldCapabilityStatement(operation, name, index);
  }
  if (operation.operationId.startsWith('event_delegate.')) {
    return makeEventDelegateUseSiteStatement(operation, name);
  }
  if (operation.operationId.startsWith('generic_ops.')) {
    return makeGenericOpsStatement(operation, name, index);
  }
  if (operation.operationId.startsWith('op.') || operation.operationId.startsWith('op_coverage.')) {
    return makeOpCoverageStatement(operation, name, index);
  }
  if (operation.operationId === 'body.custom_event') {
    return { id: name, kind: 'call', target: 'PrintString', args: { InString: `${name}_body` } };
  }
  return makeContractClusterStatement(operation, name, index);
}
```

- [ ] **Step 4: Add namespace helper implementations**

Append helper implementations to the same file. The helpers must emit statement-local evidence and must not infer UI/menu/drag state:

```ts
function makeFieldCapabilityStatement(operation: GraphWriteGeneralityOperationCase, name: string, index: number): Record<string, unknown> {
  return {
    id: name,
    kind: 'field',
    capability_id: operation.operationId,
    capability_facts: {
      'field.capability_id': operation.operationId,
      'field.variant_name': name,
      'field.variant_index': index,
    },
  };
}

function makeEventDelegateUseSiteStatement(operation: GraphWriteGeneralityOperationCase, name: string): Record<string, unknown> {
  const delegateOperation = operation.operationId.replace('event_delegate.', '');
  return {
    id: name,
    kind: delegateOperation === 'component_bound_event' ? 'component_bound_event' : delegateOperation,
    context_evidence: {
      'event_delegate.operation_id': operation.operationId,
      'event_delegate.binding_object_kind': 'self',
      'event_delegate.delegate_property_name': `${name}_Dispatcher`,
      'event_delegate.handler_function_path': `/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality:${name}_Handler`,
      'event_delegate.handler_source_cluster': 'BlueprintSignature',
      'event_delegate.signature_evidence_id': `sig:${name}`,
    },
  };
}

function makeGenericOpsStatement(operation: GraphWriteGeneralityOperationCase, name: string, index: number): Record<string, unknown> {
  const [, family, ...operationParts] = operation.operationId.split('.');
  return {
    id: name,
    kind: family,
    [`${family}_operation`]: operationParts.join('.'),
    context_evidence: {
      'generic.operation_id': operation.operationId,
      'generic.variant_name': name,
      'generic.variant_index': index,
      required_evidence_keys: operation.requiredEvidenceKeys,
    },
  };
}

function makeOpCoverageStatement(operation: GraphWriteGeneralityOperationCase, name: string, index: number): Record<string, unknown> {
  const opId = operation.operationId.replace(/^op(_coverage)?\./, '');
  return {
    id: name,
    kind: 'call',
    target: 'PrintString',
    args: {
      InString: {
        kind: 'op',
        op: opId,
        context_evidence: {
          'op.operation_id': opId,
          'op.variant_index': index,
        },
      },
    },
  };
}

function makeExpectedUnsupportedStatement(operation: GraphWriteGeneralityOperationCase, name: string): Record<string, unknown> {
  return {
    id: name,
    kind: 'call',
    target: 'PrintString',
    context_evidence: {
      expected_failure: operation.excludedReason,
      operation_id: operation.operationId,
    },
  };
}
```

- [ ] **Step 5: Add compiler generation tests**

Append this test:

```ts
import { compileTaskSpecToTaskPlan, taskPlanToAppendBridgePayload } from '../compiler/task-compiler.js';
import { makeGraphWriteGeneralityBundles } from './graphwrite-generality-spec-factory.js';

test('GraphWrite generality factory emits one TaskSpec per operation with the required distinct variant ids', () => {
  const bundles = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality',
    graphName: 'EventGraph',
  });
  for (const bundle of bundles) {
    assert.equal(bundle.expectedVariantNames.length, bundle.operation.variantCount, bundle.operation.operationId);
    assert.equal(new Set(bundle.expectedVariantNames).size, bundle.operation.variantCount, bundle.operation.operationId);
    const plan = compileTaskSpecToTaskPlan(bundle.graphWriteSpec);
    const payload = taskPlanToAppendBridgePayload(plan, true);
    const logicSpec = payload.logic_spec as { statements?: unknown[] };
    assert.ok(Array.isArray(logicSpec.statements), bundle.operation.operationId);
    assert.equal(logicSpec.statements.length, bundle.operation.variantCount, bundle.operation.operationId);
  }
});
```

- [ ] **Step 6: Run compiler tests**

Run:

```powershell
Push-Location AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node
Pop-Location
```

Expected: build succeeds and the new factory tests pass.

## Task 4: Add Report Aggregation And Charts

**Files:**
- Create: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-report.ts`
- Modify: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.test.ts`

- [ ] **Step 1: Create report data types**

Create `AgentFaceService/task-core/src/task/testing/graphwrite-generality-report.ts`:

```ts
export interface GraphWriteGeneralityVariantResult {
  operationId: string;
  variantName: string;
  requiredVariantCount: number;
  previewStatus: 'pass' | 'fail';
  executeStatus: 'pass' | 'fail';
  readbackStatus: 'pass' | 'fail';
  failureKind: 'none' | 'setup_failure' | 'preview_failure' | 'execute_failure' | 'readback_failure' | 'unsupported_intent' | 'silent_wrong_graph';
  expectedReadback: string;
  actualReadback?: string;
  evidencePath?: string;
  message?: string;
}

export interface GraphWriteGeneralityOperationSummary {
  operationId: string;
  requiredVariants: number;
  totalVariants: number;
  passedVariants: number;
  operationPassed: boolean;
  failureKinds: Record<string, number>;
}

export interface GraphWriteGeneralitySummary {
  totalOperations: number;
  passedOperations: number;
  failedOperations: number;
  totalVariants: number;
  passedVariants: number;
  operationPassRate: number;
  variantPassRate: number;
  allOperationsPassed: boolean;
  operations: GraphWriteGeneralityOperationSummary[];
}
```

- [ ] **Step 2: Add summary and CSV generation**

Append:

```ts
export function summarizeGraphWriteGeneralityResults(results: GraphWriteGeneralityVariantResult[]): GraphWriteGeneralitySummary {
  const byOperation = new Map<string, GraphWriteGeneralityVariantResult[]>();
  for (const result of results) {
    const bucket = byOperation.get(result.operationId) ?? [];
    bucket.push(result);
    byOperation.set(result.operationId, bucket);
  }
  const operations = Array.from(byOperation.entries()).map(([operationId, variants]) => {
    const passedVariants = variants.filter((variant) => variant.previewStatus === 'pass' && variant.executeStatus === 'pass' && variant.readbackStatus === 'pass').length;
    const failureKinds: Record<string, number> = {};
    for (const variant of variants) {
      failureKinds[variant.failureKind] = (failureKinds[variant.failureKind] ?? 0) + 1;
    }
    const requiredVariants = variants[0]?.requiredVariantCount ?? 10;
    return {
      operationId,
      requiredVariants,
      totalVariants: variants.length,
      passedVariants,
      operationPassed: variants.length === requiredVariants && passedVariants === requiredVariants,
      failureKinds,
    };
  }).sort((a, b) => a.operationId.localeCompare(b.operationId));
  const totalVariants = results.length;
  const passedVariants = results.filter((variant) => variant.previewStatus === 'pass' && variant.executeStatus === 'pass' && variant.readbackStatus === 'pass').length;
  const passedOperations = operations.filter((operation) => operation.operationPassed).length;
  return {
    totalOperations: operations.length,
    passedOperations,
    failedOperations: operations.length - passedOperations,
    totalVariants,
    passedVariants,
    operationPassRate: operations.length > 0 ? passedOperations / operations.length : 0,
    variantPassRate: totalVariants > 0 ? passedVariants / totalVariants : 0,
    allOperationsPassed: operations.length > 0 && passedOperations === operations.length,
    operations,
  };
}

export function renderGraphWriteGeneralityCsv(results: GraphWriteGeneralityVariantResult[]): string {
  const header = 'operation_id,variant_name,preview_status,execute_status,readback_status,failure_kind,expected_readback,actual_readback,evidence_path,message';
  const rows = results.map((result) => [
    result.operationId,
    result.variantName,
    result.previewStatus,
    result.executeStatus,
    result.readbackStatus,
    result.failureKind,
    result.expectedReadback,
    result.actualReadback ?? '',
    result.evidencePath ?? '',
    result.message ?? '',
  ].map(csvCell).join(','));
  return [header, ...rows].join('\n') + '\n';
}

function csvCell(value: string): string {
  return `"${value.replaceAll('"', '""')}"`;
}
```

- [ ] **Step 3: Add Markdown and SVG charts**

Append:

```ts
export function renderGraphWriteGeneralityMarkdown(summary: GraphWriteGeneralitySummary, chartFiles: { operationChart: string; failureChart: string }): string {
  const percent = (value: number) => `${(value * 100).toFixed(1)}%`;
  const rows = summary.operations.map((operation) => (
    `| ${operation.operationId} | ${operation.operationPassed ? 'PASS' : 'FAIL'} | ${operation.passedVariants}/${operation.requiredVariants} | ${JSON.stringify(operation.failureKinds)} |`
  ));
  return [
    '# BlueprintHelper GraphWrite 通用性前置测试报告',
    '',
    `生成时间：${new Date().toISOString()}`,
    '',
    '## Summary',
    '',
    `- Operation pass rate: ${percent(summary.operationPassRate)} (${summary.passedOperations}/${summary.totalOperations})`,
    `- Variant pass rate: ${percent(summary.variantPassRate)} (${summary.passedVariants}/${summary.totalVariants})`,
    `- Gate: ${summary.allOperationsPassed ? 'PASS' : 'FAIL'}`,
    '',
    '## Charts',
    '',
    `![Operation pass/fail](${chartFiles.operationChart})`,
    '',
    `![Failure distribution](${chartFiles.failureChart})`,
    '',
    '## Operation Table',
    '',
    '| Operation | Result | Variants | Failure kinds |',
    '|---|---|---:|---|',
    ...rows,
    '',
  ].join('\n');
}

export function renderOperationPassSvg(summary: GraphWriteGeneralitySummary): string {
  const width = 720;
  const height = 220;
  const passWidth = Math.round(520 * summary.operationPassRate);
  const failWidth = 520 - passWidth;
  return [
    `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">`,
    '<rect width="100%" height="100%" fill="#ffffff"/>',
    '<text x="24" y="34" font-family="Arial" font-size="20" fill="#111111">GraphWrite Generality Operation Pass Rate</text>',
    '<rect x="120" y="80" width="520" height="42" fill="#e5e7eb"/>',
    `<rect x="120" y="80" width="${passWidth}" height="42" fill="#16a34a"/>`,
    `<rect x="${120 + passWidth}" y="80" width="${failWidth}" height="42" fill="#dc2626"/>`,
    `<text x="120" y="154" font-family="Arial" font-size="16" fill="#111111">PASS ${summary.passedOperations}/${summary.totalOperations}</text>`,
    `<text x="360" y="154" font-family="Arial" font-size="16" fill="#111111">VARIANTS ${summary.passedVariants}/${summary.totalVariants}</text>`,
    '</svg>',
  ].join('\n');
}

export function renderFailureDistributionSvg(summary: GraphWriteGeneralitySummary): string {
  const counts = new Map<string, number>();
  for (const operation of summary.operations) {
    for (const [kind, count] of Object.entries(operation.failureKinds)) {
      if (kind !== 'none') counts.set(kind, (counts.get(kind) ?? 0) + count);
    }
  }
  const entries = Array.from(counts.entries()).sort((a, b) => b[1] - a[1]);
  const width = 840;
  const rowHeight = 32;
  const height = Math.max(160, 80 + entries.length * rowHeight);
  const max = Math.max(1, ...entries.map((entry) => entry[1]));
  const bars = entries.map(([kind, count], index) => {
    const y = 72 + index * rowHeight;
    const barWidth = Math.round(520 * count / max);
    return `<text x="24" y="${y + 18}" font-family="Arial" font-size="14" fill="#111111">${kind}</text><rect x="260" y="${y}" width="${barWidth}" height="22" fill="#2563eb"/><text x="${270 + barWidth}" y="${y + 17}" font-family="Arial" font-size="14" fill="#111111">${count}</text>`;
  });
  return [
    `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">`,
    '<rect width="100%" height="100%" fill="#ffffff"/>',
    '<text x="24" y="34" font-family="Arial" font-size="20" fill="#111111">GraphWrite Generality Failure Distribution</text>',
    ...bars,
    '</svg>',
  ].join('\n');
}
```

- [ ] **Step 4: Add report math tests**

Append this test:

```ts
import {
  renderFailureDistributionSvg,
  renderGraphWriteGeneralityCsv,
  renderGraphWriteGeneralityMarkdown,
  renderOperationPassSvg,
  summarizeGraphWriteGeneralityResults,
  type GraphWriteGeneralityVariantResult,
} from './graphwrite-generality-report.js';

test('GraphWrite generality report requires all required variants for an operation pass and renders charts', () => {
  const results: GraphWriteGeneralityVariantResult[] = Array.from({ length: 10 }, (_, index) => ({
    operationId: 'call.call',
    variantName: `GWGen_call_call_${String(index).padStart(2, '0')}`,
    requiredVariantCount: 10,
    previewStatus: 'pass',
    executeStatus: index === 9 ? 'fail' : 'pass',
    readbackStatus: index === 9 ? 'fail' : 'pass',
    failureKind: index === 9 ? 'execute_failure' : 'none',
    expectedReadback: 'K2Node_CallFunction',
  }));
  const summary = summarizeGraphWriteGeneralityResults(results);
  assert.equal(summary.totalOperations, 1);
  assert.equal(summary.passedOperations, 0);
  assert.equal(summary.allOperationsPassed, false);
  assert.equal(summary.passedVariants, 9);
  assert.match(renderGraphWriteGeneralityCsv(results), /execute_failure/);
  assert.match(renderGraphWriteGeneralityMarkdown(summary, { operationChart: 'operation.svg', failureChart: 'failure.svg' }), /Gate: FAIL/);
  assert.match(renderOperationPassSvg(summary), /<svg/);
  assert.match(renderFailureDistributionSvg(summary), /execute_failure/);
});

test('GraphWrite generality report accepts one successful singleton variant', () => {
  const summary = summarizeGraphWriteGeneralityResults([{
    operationId: 'body.custom_event',
    variantName: 'GWGen_body_custom_event_00',
    requiredVariantCount: 1,
    previewStatus: 'pass',
    executeStatus: 'pass',
    readbackStatus: 'pass',
    failureKind: 'none',
    expectedReadback: 'body content under existing K2Node_CustomEvent',
  }]);
  assert.equal(summary.totalOperations, 1);
  assert.equal(summary.passedOperations, 1);
  assert.equal(summary.allOperationsPassed, true);
  assert.equal(summary.passedVariants, 1);
});
```

- [ ] **Step 5: Run report tests**

Run:

```powershell
Push-Location AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node
Pop-Location
```

Expected: report tests pass.

## Task 5: Add Runtime Preflight Orchestration

**Files:**
- Create: `BlueprintHelper/Develop/Scripts/Run-GraphWriteGeneralityPreflight.ps1`
- Modify: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-spec-factory.ts`
- Modify: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-report.ts`

- [ ] **Step 1: Add a spec writer entry point**

Add this export to `graphwrite-generality-spec-factory.ts`:

```ts
import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

export function writeGraphWriteGeneralitySpecs(input: {
  assetPath: string;
  graphName: string;
  outDir: string;
}): string[] {
  mkdirSync(input.outDir, { recursive: true });
  const bundles = makeGraphWriteGeneralityBundles({
    assetPath: input.assetPath,
    graphName: input.graphName,
  });
  const files: string[] = [];
  for (const bundle of bundles) {
    const operationDir = join(input.outDir, bundle.operation.operationId.replaceAll('.', '_'));
    mkdirSync(operationDir, { recursive: true });
    bundle.fixtureSpecs.forEach((spec, index) => {
      const file = join(operationDir, `fixture_${String(index + 1).padStart(2, '0')}.json`);
      writeFileSync(file, JSON.stringify(spec, null, 2), 'utf8');
      files.push(file);
    });
    const graphWriteFile = join(operationDir, 'graph_write.json');
    writeFileSync(graphWriteFile, JSON.stringify(bundle.graphWriteSpec, null, 2), 'utf8');
    writeFileSync(join(operationDir, 'expected_variants.json'), JSON.stringify({
      operation: bundle.operation,
      expectedVariantNames: bundle.expectedVariantNames,
    }, null, 2), 'utf8');
    files.push(graphWriteFile);
  }
  return files;
}
```

- [ ] **Step 2: Add a PowerShell runner skeleton with real CLI commands**

Create `BlueprintHelper/Develop/Scripts/Run-GraphWriteGeneralityPreflight.ps1`:

```powershell
param(
  [string]$PluginRoot = "D:\UEProjects\Template\Plugins\BlueprintHelper",
  [string]$AssetPath = "/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality",
  [string]$GraphName = "EventGraph",
  [string]$RunId = ("GraphWriteGenerality_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
)

$ErrorActionPreference = "Stop"
$TaskCore = Join-Path $PluginRoot "AgentFaceService\task-core"
$Cli = Join-Path $PluginRoot "AgentFaceService\cli"
$OutRoot = Join-Path "D:\UEProjects\Template\Saved\Automation" $RunId
$SpecRoot = Join-Path $OutRoot "specs"
$ResultRoot = Join-Path $OutRoot "results"
New-Item -ItemType Directory -Force -Path $SpecRoot, $ResultRoot | Out-Null

Push-Location $TaskCore
npm.cmd run build
Pop-Location
Push-Location $Cli
npm.cmd run build
Pop-Location

node (Join-Path $TaskCore "build\task\testing\write-graphwrite-generality-specs.js") --asset $AssetPath --graph $GraphName --out $SpecRoot

$OperationDirs = Get-ChildItem -Path $SpecRoot -Directory
$AllResults = @()
foreach ($OperationDir in $OperationDirs) {
  $OperationId = $OperationDir.Name.Replace("_", ".")
  $OperationResultDir = Join-Path $ResultRoot $OperationDir.Name
  New-Item -ItemType Directory -Force -Path $OperationResultDir | Out-Null

  Get-ChildItem -Path $OperationDir.FullName -Filter "fixture_*.json" | Sort-Object Name | ForEach-Object {
    $PreviewPath = Join-Path $OperationResultDir ($_.BaseName + "_preview.json")
    node (Join-Path $Cli "build\cli\index.js") task preview --file $_.FullName --format json --artifact-dir $OperationResultDir | Out-File -Encoding utf8 $PreviewPath
    $ExecutePath = Join-Path $OperationResultDir ($_.BaseName + "_execute.json")
    node (Join-Path $Cli "build\cli\index.js") task execute --file $_.FullName --format json --artifact-dir $OperationResultDir | Out-File -Encoding utf8 $ExecutePath
  }

  $GraphWriteSpec = Join-Path $OperationDir.FullName "graph_write.json"
  $PreviewFile = Join-Path $OperationResultDir "graph_write_preview.json"
  node (Join-Path $Cli "build\cli\index.js") task preview --file $GraphWriteSpec --format json --artifact-dir $OperationResultDir | Out-File -Encoding utf8 $PreviewFile
  $ExecuteFile = Join-Path $OperationResultDir "graph_write_execute.json"
  node (Join-Path $Cli "build\cli\index.js") task execute --file $GraphWriteSpec --format json --artifact-dir $OperationResultDir | Out-File -Encoding utf8 $ExecuteFile
}

node (Join-Path $TaskCore "build\task\testing\write-graphwrite-generality-report.js") --run $OutRoot --report "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\Report"
```

- [ ] **Step 3: Add the two Node bin wrappers**

Create TypeScript entry files:

`AgentFaceService/task-core/src/task/testing/write-graphwrite-generality-specs.ts`

```ts
#!/usr/bin/env node
import { writeGraphWriteGeneralitySpecs } from './graphwrite-generality-spec-factory.js';

const args = new Map<string, string>();
for (let index = 2; index < process.argv.length; index += 2) {
  args.set(process.argv[index], process.argv[index + 1]);
}

writeGraphWriteGeneralitySpecs({
  assetPath: args.get('--asset') ?? '/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality',
  graphName: args.get('--graph') ?? 'EventGraph',
  outDir: args.get('--out') ?? 'Saved/Automation/GraphWriteGenerality/specs',
});
```

`AgentFaceService/task-core/src/task/testing/write-graphwrite-generality-report.ts`

```ts
#!/usr/bin/env node
import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import {
  renderFailureDistributionSvg,
  renderGraphWriteGeneralityCsv,
  renderGraphWriteGeneralityMarkdown,
  renderOperationPassSvg,
  summarizeGraphWriteGeneralityResults,
  type GraphWriteGeneralityVariantResult,
} from './graphwrite-generality-report.js';

const args = new Map<string, string>();
for (let index = 2; index < process.argv.length; index += 2) {
  args.set(process.argv[index], process.argv[index + 1]);
}

const reportDir = args.get('--report') ?? 'BlueprintHelper/Develop/Report';
mkdirSync(reportDir, { recursive: true });

const results: GraphWriteGeneralityVariantResult[] = [];
const summary = summarizeGraphWriteGeneralityResults(results);
const date = new Date().toISOString().slice(0, 10).replaceAll('-', '');
const csvName = `BlueprintHelper_GraphWrite_GeneralityPreflight_Data_${date}.csv`;
const operationChartName = `BlueprintHelper_GraphWrite_GeneralityPreflight_OperationChart_${date}.svg`;
const failureChartName = `BlueprintHelper_GraphWrite_GeneralityPreflight_FailureChart_${date}.svg`;
const reportName = `BlueprintHelper_GraphWrite_GeneralityPreflight_Report_${date}_CN.md`;

writeFileSync(join(reportDir, csvName), renderGraphWriteGeneralityCsv(results), 'utf8');
writeFileSync(join(reportDir, operationChartName), renderOperationPassSvg(summary), 'utf8');
writeFileSync(join(reportDir, failureChartName), renderFailureDistributionSvg(summary), 'utf8');
writeFileSync(join(reportDir, reportName), renderGraphWriteGeneralityMarkdown(summary, {
  operationChart: operationChartName,
  failureChart: failureChartName,
}), 'utf8');
writeFileSync(join(reportDir, 'BlueprintHelper_GraphWrite_GeneralityPreflight_LatestSummary.json'), JSON.stringify(summary, null, 2), 'utf8');
```

The first pass of `write-graphwrite-generality-report.ts` intentionally writes a structurally valid failed report when no variant results are parsed. The same task must then replace the empty `results` array with parsed preview/execute/readback entries before exit criteria are accepted.

- [ ] **Step 4: Replace empty report parsing with real result extraction**

In `write-graphwrite-generality-report.ts`, replace `const results: GraphWriteGeneralityVariantResult[] = [];` with extraction from `$RunRoot/specs/*/expected_variants.json`, `graph_write_preview.json`, `graph_write_execute.json`, and graph readback artifacts. Each expected variant emits one `GraphWriteGeneralityVariantResult`:

```ts
const results: GraphWriteGeneralityVariantResult[] = readRunResults(args.get('--run') ?? '');
```

The helper must classify failures in this order:

1. missing fixture or fixture execute failure -> `setup_failure`
2. preview blocked -> `preview_failure`
3. execute failed -> `execute_failure`
4. unsupported operation code found -> `unsupported_intent`
5. readback missing expected node class or expected variant name -> `readback_failure`
6. readback exists but semantic metadata mismatches operation id -> `silent_wrong_graph`
7. all checks match -> `none`

- [ ] **Step 5: Run the preflight in preview-only mode first**

Run:

```powershell
.\BlueprintHelper\Develop\Scripts\Run-GraphWriteGeneralityPreflight.ps1 -RunId GraphWriteGenerality_DryRun_001
```

Expected: report files are created under `BlueprintHelper/Develop/Report`, and any current unsupported runtime operation is visible as failed operation rows rather than skipped rows.

## Task 6: Add Final Test Gate

**Files:**
- Create: `BlueprintHelper/Develop/Scripts/Run-GraphWriteFinalWithGenerality.ps1`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`

- [ ] **Step 1: Create final gate script**

Create `BlueprintHelper/Develop/Scripts/Run-GraphWriteFinalWithGenerality.ps1`:

```powershell
param(
  [string]$PluginRoot = "D:\UEProjects\Template\Plugins\BlueprintHelper",
  [string]$ProjectPath = "D:\UEProjects\Template\Template.uproject"
)

$ErrorActionPreference = "Stop"
$Preflight = Join-Path $PluginRoot "BlueprintHelper\Develop\Scripts\Run-GraphWriteGeneralityPreflight.ps1"
& $Preflight -PluginRoot $PluginRoot

$SummaryPath = Join-Path $PluginRoot "BlueprintHelper\Develop\Report\BlueprintHelper_GraphWrite_GeneralityPreflight_LatestSummary.json"
$Summary = Get-Content -Raw -Path $SummaryPath | ConvertFrom-Json
if ($Summary.allOperationsPassed -ne $true) {
  throw "GraphWrite final test blocked: generality preflight failed. See $SummaryPath"
}

& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $ProjectPath -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.Capability80;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite80_Final_WithGenerality'
```

- [ ] **Step 2: Add the gate row to the 80% test record**

Append this row to the final summary table in `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`:

```markdown
| Generality preflight gate | REQUIRED_BEFORE_FINAL | `BlueprintHelper/Develop/Report/BlueprintHelper_GraphWrite_GeneralityPreflight_LatestSummary.json`; final P6/80% suite may run only when `allOperationsPassed=true`. |
```

- [ ] **Step 3: Run the gate with the current implementation**

Run:

```powershell
.\BlueprintHelper\Develop\Scripts\Run-GraphWriteFinalWithGenerality.ps1
```

Expected while gaps remain: the script stops before P6 and points to the failed generality report. Expected after all operation gaps close: the preflight passes, then `BlueprintHelper.GraphWrite.Capability80` runs.

## Task 7: Verification

**Files:**
- All files modified or created above.

- [ ] **Step 1: Run TypeScript build and node tests**

Run:

```powershell
Push-Location AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node
Pop-Location
Push-Location AgentFaceService\cli
npm.cmd run build
npm.cmd run test:node
Pop-Location
```

Expected: all Node tests pass.

- [ ] **Step 2: Run focused GraphWrite preflight**

Run:

```powershell
.\BlueprintHelper\Develop\Scripts\Run-GraphWriteGeneralityPreflight.ps1 -RunId GraphWriteGenerality_Verification_001
```

Expected: `BlueprintHelper/Develop/Report/BlueprintHelper_GraphWrite_GeneralityPreflight_LatestSummary.json` exists and contains:

```json
{
  "totalOperations": "<ownership-filtered operation count>",
  "totalVariants": "<sum of requiredVariantCount; parameterized operations contribute 10, singleton operations contribute 1>"
}
```

If any operation fails, the report must still include every ownership-filtered operation and all generated variants. Non-GraphWrite-owned fixture/dependency rows must be reported separately and must not inflate the GraphWrite operation count.

- [ ] **Step 3: Run final gate**

Run:

```powershell
.\BlueprintHelper\Develop\Scripts\Run-GraphWriteFinalWithGenerality.ps1
```

Expected:

- If `allOperationsPassed=false`, the command stops before P6 and prints the report path.
- If `allOperationsPassed=true`, the command runs `BlueprintHelper.GraphWrite.Capability80`.

- [ ] **Step 4: Run source hygiene check**

Run:

```powershell
rg -n "blueprint_operations|manual_control_context|manual_control_semantic|delegate_call|delegate_clear|unsupported_event_delegate_cluster_semantic" BlueprintHelper\Source\BlueprintHelper\Private\Systems\ToolClusters\GraphWrite BlueprintHelper\Source\BlueprintHelper\Public\Systems\ToolClusters\GraphWrite
git diff --check
```

Expected: source scan has no active legacy mainline hits except intentional diagnostic tests or deletion-gate references; `git diff --check` exits `0`.

## Exit Criteria

- [ ] Generality matrix source is synchronized with `GRAPHWRITE_CAPABILITY_CONTRACT.clusters` and `GRAPHWRITE_CAPABILITY_CONTRACT.operationGroups`, plus the explicit core-shape expansion in this plan.
- [ ] Operation matrix covers all ownership-filtered supported capability operations after excluding existing-tool-owned declarations, fixtures, UI/editor-only inputs, rejected rows, and mutation-service-owned operations.
- [ ] Every operation declares `variantMode` and `requiredVariantCount`: ordinary parameterized operations generate exactly 10 variant names and one TaskSpec body containing exactly 10 GraphWrite statements or expression-backed nodes; Singleton operations generate exactly 1 variant and one successful representative TaskSpec/readback.
- [ ] Runtime preflight executes through TaskSpec preview/execute/readback; ActionResolution direct tests are not counted as score.
- [ ] Report artifacts include JSON, CSV, Markdown, operation pass/fail SVG, and failure distribution SVG.
- [ ] Final P6/80% total test is gated by `allOperationsPassed=true`.
- [ ] No automatic git staging, commit, or push was performed.

## Suggested Manual Commit Message

变更需求：
1. 按 2026-05-26 最新 GraphWrite 能力簇更新通用性测试矩阵来源。
2. 明确通用性矩阵从 capability contract 的 clusters / operationGroups 派生，并保留 Field、EventDelegate、GenericOps、OpCoverage 的 TaskSpec 边界。
3. 固定 UI/editor-menu/drag/pin 行为不进入 GraphWrite 通用性计分。
4. 增加 Singleton operation 只需 1 个代表性 TaskSpec/readback 成功的通用性测试规则。
5. 覆盖记录 2026-05-26 当前 GraphWrite FullTest 结果：282/304 成功、22/304 失败、exit code 255、报告缺失。
