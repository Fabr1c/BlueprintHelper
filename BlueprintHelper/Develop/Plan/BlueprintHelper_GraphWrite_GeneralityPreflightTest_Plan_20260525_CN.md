# GraphWrite Generality Preflight Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 GraphWrite 最终总测试前新增一个通用性前置门禁：GraphWrite-owned、ownership-filtered 的每个 `kind + operation` 都通过 TaskSpec 生成 10 个同类型不同名节点，10 个全通过才算该 operation 通过，并输出带统计图的通用性测试报告。

## 2026-05-25 Current Status Sync

- Status: OPEN / NOT IMPLEMENTED.
- 当前源码树未发现 `graphwrite-generality-*` TaskSpec matrix、spec factory、report writer、PowerShell runner 或 `Run-GraphWriteFinalWithGenerality.ps1` 的实际实现文件；本文件仍是待执行计划。
- 该计划应在 remaining evidence defects、capability contract expansion 完成后统一执行，用于最终能力面验收；`container_action` public shape 已在 `BlueprintHelper_GraphWrite_ContainerAction_FirstClassPlan_20260525_CN.md` 中实现并通过 focused gate，但它不替代本文件的 ownership-filtered 泛化验收。
- 完成标准仍以本文 Exit Criteria 为准，但 operation 数量必须先按 `BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md` 的 ownership filter 重算；原 45 个 normalized operations / 450 个 variants 是未过滤草案，不再作为最终计数口径。最终仍必须覆盖 ownership-filtered operations、每项 10 variants、TaskSpec preview/execute/readback、JSON/CSV/Markdown/SVG report、`allOperationsPassed=true` final gate。

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
| full GraphWrite suite | PASS | `Automation RunTests BlueprintHelper.GraphWrite`；该套件通过说明当前核心 GraphWrite automation gates 已绿，但不替代 ownership-filtered 泛化验收。 |
| full generality preflight | PENDING | 本文件下方的 matrix、factory、runner、report writer 仍未实现，不能标记 stable final。 |

## 2026-05-25 Ownership Filter Update

本计划的 operation matrix 必须先排除已有工具职责重合项，再作为 GraphWrite 最终验收矩阵：

| Category | Treatment in preflight |
|---|---|
| `BlueprintSignature` declarations: custom/override/native event declaration, function signature, event dispatcher, handler declaration | 作为 fixture/dependency；不计入 GraphWrite operation 成败。 |
| Delegate/component handler lookup or declaration creation | 作为 Signature/ActionContext projection 前置条件；GraphWrite 缺 evidence 时应返回 deterministic diagnostic。 |
| EventDelegate use-site: `component_bound_event`、`delegate bind/assign/unbind/clear/call` | 只在已有声明/evidence 完整时作为 use-site graph writing 验证；不把声明缺口算作 GraphWrite failure。 |
| Merge/Patch/ConnectPins mutation ownership | 不计入 Spawner-Oriented GraphStatement preflight，除非后续明确迁入 GraphStatement 主线。 |
| Evidence-heavy Generic paths | `type_promotion` 的 TaskSpec passthrough 与 resolver consumption 已存在，作为 final preflight coverage；`asset_action` Review policy 已收窄为 graph-level `graph_block`，由 `BlueprintHelper_GraphWrite_AssetActionReviewPolicy_GraphBlockPlan_20260525_CN.md` 固化，不进入 action-level Review target；`timer_delegate_node`、`latent_or_async_node` 已决定保留为 GraphWrite Generic schedule success path，并由 `BlueprintHelper_GraphWrite_GenericScheduleSuccessPathPlan_20260525_CN.md` 补齐 success evidence/readback。 |
| `anim_notify_event` | 归 Animation Blueprint / Animation tooling；当前 GraphWrite 只关注普通 Blueprint，不进入最终矩阵。 |
| broad `container_action` | `BlueprintHelper_GraphWrite_ContainerAction_FirstClassPlan_20260525_CN.md` 已实现 V1 first-class 范围：核心 array/map/set 操作进入 `container_action` public shape，执行复用 FunctionAction-backed callable evidence；`make_*` 仍归 Generic create，`foreach` 归后续 control-flow。focused readback 已覆盖 wildcard 被替换成目标类型、target link 正确、编译无报错；最终矩阵仍需按 owned operation 生成 10 variants。 |
| FunctionAction overlap | 容器操作会与 FunctionAction 高度重叠；最终矩阵前必须明确哪些作为普通 callable 计入 FunctionAction，哪些需要 first-class `container_action` 语义。 |
| capability contract expansion | `container_action` slice 已对齐到 contract；完整 contract expansion 仍需等 remaining evidence defects 关闭后再统一重算最终矩阵。 |
| ownership-filtered final generality preflight | 放到 capability contract expansion 之后执行，作为最终门禁。 |

**Architecture:** 通用性测试以 Agent-facing TaskSpec 为唯一计分入口，执行链路必须经过 `TaskSpec -> compiler lowering -> TaskPlan graph_write -> UE GraphWrite runtime -> graph readback`。Operation 矩阵是数据驱动的公共测试目录；fixture/setup 负责创建资产、变量、组件、签名和 handler 证据，但不计入 GraphWrite 正确率。报告生成器消费每个 operation 的 10 个 variant 结果，输出 JSON/CSV/Markdown/SVG，并作为最终 80% 总测的前置 gate。

**Tech Stack:** TypeScript task-core/CLI, BlueprintHelper TaskSpec v1, UE 5.6 GraphWrite runtime, PowerShell orchestration, JSON/CSV/Markdown/SVG reports.

---

## Scope And Boundary

本计划新增的是“最终总测前置通用性测试”，不是替代 P0-P6 的能力测试。P0-P6 仍验证复杂需求正确率；本前置测试验证每个工具簇 operation 是否能稳定泛化到 10 个同类变体。

计分规则固定如下：

| Rule | Contract |
|---|---|
| Unit under test | 一个 normalized `operation_id`，格式为 `kind.operation` 或 `kind.operation.scope`。 |
| Variant count | 每个 operation 必须生成并执行 10 个 variant，命名为 `GWGen_<OperationId>_00` 到 `GWGen_<OperationId>_09`。 |
| Operation pass | 10 个 variant 全部 preview、execute、readback 通过。 |
| Operation fail | 任意一个 variant 失败、缺少 readback、返回 unsupported、silent wrong graph、spawn/link/default/pin 错误。 |
| Fixture failure | 资产、组件、变量、Signature dependency、handler 创建失败记录为 `setup_failure`，不计入 GraphWrite 正确率，但该 operation 不能通过前置 gate。 |
| Direct spawn boundary | `branch`、`sequence`、`return`、`select` 等唯一控制流允许 direct spawn，但仍必须走 `SpawnerClusterKind -> cluster -> semantic constraint -> evidence/provider -> shared spawn adapter`。 |
| Signature boundary | Handler、event dispatcher、custom event、function signature 的声明所有权属于 BlueprintSignature；GraphWrite/EventDelegate 只能消费已有声明/签名证据生成图节点、绑定节点、连接和 body 内容。 |
| Score source | 只有 TaskSpec preview/execute/readback 结果能进入通用性分数；ActionResolution 单测只作为定位和契约防回退。 |

## Operation Matrix

实现时以一个代码化 matrix 作为单一测试目录。下面是首版必须覆盖的 normalized operations。

| OperationId | Agent-facing shape | Lowered shape | Expected node family |
|---|---|---|---|
| `call.call` | `kind=call` | `kind=call` | `K2Node_CallFunction` |
| `field.set.variable` | `kind=set` or `kind=field, field_operation=set, field_scope=variable` | `kind=field, field_operation=set` | `K2Node_VariableSet` |
| `field.set.property_path` | `kind=set_property` or explicit field | `kind=field, field_operation=set, field_scope=property_path` | property setter / call-backed property node |
| `field.set.component_ref` | explicit field | `kind=field, field_operation=set, field_scope=component_ref` | component field setter |
| `field.set.field_access` | explicit field | `kind=field, field_operation=set, field_scope=field_access` | field access setter |
| `field.get.variable` | `kind=get` or explicit field expression | `kind=field, field_operation=get` | `K2Node_VariableGet` |
| `field.get.property_path` | `kind=get_property` or explicit field expression | `kind=field, field_operation=get, field_scope=property_path` | property getter / call-backed property node |
| `field.get.component_ref` | explicit field expression | `kind=field, field_operation=get, field_scope=component_ref` | component ref getter |
| `field.get.field_access` | explicit field expression | `kind=field, field_operation=get, field_scope=field_access` | field access getter |
| `op.add` | `kind=op, op=add` | `SemanticKind=op, query=add` | `K2Node_PromotableOperator` |
| `op.subtract` | `kind=op, op=subtract` | `SemanticKind=op, query=subtract` | `K2Node_PromotableOperator` |
| `op.multiply` | `kind=op, op=multiply` | `SemanticKind=op, query=multiply` | `K2Node_PromotableOperator` |
| `op.divide` | `kind=op, op=divide` | `SemanticKind=op, query=divide` | `K2Node_PromotableOperator` |
| `op.greater` | `kind=op, op=greater` | `SemanticKind=op, query=greater` | `K2Node_PromotableOperator` |
| `op.greater_equal` | `kind=op, op=greater_equal` | `SemanticKind=op, query=greater_equal` | `K2Node_PromotableOperator` |
| `op.less` | `kind=op, op=less` | `SemanticKind=op, query=less` | `K2Node_PromotableOperator` |
| `op.less_equal` | `kind=op, op=less_equal` | `SemanticKind=op, query=less_equal` | `K2Node_PromotableOperator` |
| `op.equal` | `kind=op, op=equal` | `SemanticKind=op, query=equal` | `K2Node_PromotableOperator` |
| `op.not_equal` | `kind=op, op=not_equal` | `SemanticKind=op, query=not_equal` | `K2Node_PromotableOperator` |
| `construct.construct` | `kind=construct` | `TypeOperation=construct` | `K2Node_MakeStruct` |
| `deconstruct.deconstruct` | `kind=deconstruct` | `TypeOperation=deconstruct` | `K2Node_BreakStruct` |
| `select.select` | `kind=select` | singleton select provider | `K2Node_Select` |
| `control.branch` | `kind=control, control=branch` | internal `kind=branch` | `K2Node_IfThenElse` |
| `control.sequence` | `kind=control, control=sequence` | internal `kind=sequence` | `K2Node_ExecutionSequence` |
| `control.return` | `kind=control, control=return` | internal `kind=return` | return node |
| `event.custom_event` | existing custom event entry/body | EventDelegate use-site evidence | custom event body graph entry |
| `component_bound_event.bind` | `kind=component_bound_event` | `kind=component_bound_event` | `K2Node_ComponentBoundEvent` |
| `delegate.bind` | `kind=delegate.bind` | `kind=delegate, delegate_operation=bind` | `K2Node_AddDelegate` + `K2Node_CreateDelegate` |
| `delegate.assign` | `kind=delegate.assign` | `kind=delegate, delegate_operation=assign` | `K2Node_AssignDelegate` |
| `delegate.unbind` | `kind=delegate.unbind` | `kind=delegate, delegate_operation=unbind, unbind_mode=single` | `K2Node_RemoveDelegate` |
| `delegate.clear` | `kind=delegate.unbind_all` | `kind=delegate, delegate_operation=clear, unbind_mode=all` | `K2Node_ClearDelegate` |
| `delegate.call` | `kind=delegate.call` | `kind=delegate, delegate_operation=call` | `K2Node_CallDelegate` |
| `create.spawn_actor` | `kind=create, create_operation=spawn_actor` | create operation evidence | `K2Node_SpawnActorFromClass` |
| `create.create_widget` | `kind=create, create_operation=create_widget` | create operation evidence | `K2Node_CreateWidget` |
| `create.construct_object` | `kind=create, create_operation=construct_object` | create operation evidence | `K2Node_GenericCreateObject` |
| `create.make_array` | `kind=create, create_operation=make_array` | create operation evidence | `K2Node_MakeArray` |
| `create.make_map` | `kind=create, create_operation=make_map` | create operation evidence | `K2Node_MakeMap` |
| `create.make_set` | `kind=create, create_operation=make_set` | create operation evidence | `K2Node_MakeSet` |
| `create.asset_action` | `kind=create, create_operation=asset_action` | projected ActionDatabase spawner evidence | ActionDatabase selected node |
| `convert.dynamic_cast` | `kind=convert, transform_operation=dynamic_cast` | transform operation evidence | `K2Node_DynamicCast` |
| `convert.class_cast` | `kind=convert, transform_operation=class_cast` | transform operation evidence | `K2Node_ClassDynamicCast` |
| `convert.type_promotion` | `kind=convert, transform_operation=type_promotion` | type promotion evidence | promotion-backed node |
| `schedule.timer_delegate_node` | `kind=schedule, schedule_operation=timer_delegate_node` | projected ActionDatabase schedule spawner evidence + BlueprintSignature handler evidence | timer/delegate schedule node linked to existing handler |
| `schedule.latent_or_async_node` | `kind=schedule, schedule_operation=latent_or_async_node` | projected ActionDatabase schedule spawner evidence + `graph_latent_allowed=true` | latent/async callable node |

The matrix must not silently skip an operation. If an operation is still unsupported in runtime, the preflight records `unsupported_intent` and fails that operation. Only a value not present in the public GraphWrite contract may be excluded.

## File Structure

- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
  - Owns the public TaskSpec/TaskPlan contract. It must list every GraphWrite statement/expression kind that the compiler actually accepts.
- Create: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.ts`
  - Single source for operation ids, public shapes, normalized operations, variant count, fixture requirements, expected readback signatures, and operation cluster ownership.
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

## Task 1: Align Public Contract With Compiler-Supported GraphWrite Shapes

**Files:**
- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
- Test: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.test.ts`

- [ ] **Step 1: Add the contract expectations test**

Create the first test block in `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.test.ts`:

```ts
import assert from 'node:assert/strict';
import test from 'node:test';
import { TASK_PROTOCOL_CONTRACT_V1 } from '../schema/task-contract.js';

test('GraphWrite public contract lists every compiler-supported statement and expression shape used by the generality preflight', () => {
  const firstSlice = TASK_PROTOCOL_CONTRACT_V1.supported_first_slice;
  assert.deepEqual(
    new Set(firstSlice.statement_kinds),
    new Set([
      'call',
      'field',
      'set',
      'set_property',
      'let',
      'control',
      'create',
      'convert',
      'schedule',
      'component_bound_event',
      'delegate.bind',
      'delegate.assign',
      'delegate.unbind',
      'delegate.unbind_all',
      'delegate.call',
    ]),
  );
  assert.deepEqual(
    new Set(firstSlice.expression_kinds),
    new Set([
      'literal',
      'field',
      'get',
      'get_property',
      'call',
      'op',
      'construct',
      'deconstruct',
      'select',
      'create',
      'convert',
      'schedule',
    ]),
  );
});
```

- [ ] **Step 2: Run the failing contract test**

Run:

```powershell
Push-Location AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node
Pop-Location
```

Expected before contract update: the test fails because the contract omits at least `field`, `create`, `convert`, `schedule` from statement kinds and omits `field`, `create`, `convert`, `schedule` from expression kinds.

- [ ] **Step 3: Update the public contract**

In `AgentFaceService/task-core/src/task/schema/task-contract.ts`, update `supported_first_slice.statement_kinds` and `supported_first_slice.expression_kinds` to match the exact arrays in Step 1. Keep the delegate boundary text unchanged: Agent-facing compact delegate shapes remain public, compiler-internal `kind=delegate + delegate_operation` remains non-agent-facing.

- [ ] **Step 4: Re-run the contract test**

Run:

```powershell
Push-Location AgentFaceService\task-core
npm.cmd run build
npm.cmd run test:node
Pop-Location
```

Expected after contract update: the new contract test passes.

## Task 2: Add The Operation Matrix

**Files:**
- Create: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.ts`
- Modify: `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.test.ts`

- [ ] **Step 1: Create the matrix types and constants**

Create `AgentFaceService/task-core/src/task/testing/graphwrite-generality-matrix.ts`:

```ts
export const GRAPHWRITE_GENERALITY_VARIANT_COUNT = 10;

export type GraphWriteGeneralityCluster =
  | 'FunctionActionCluster'
  | 'FieldVariableActionCluster'
  | 'EventDelegateActionCluster'
  | 'GenericAssetStructControlActionCluster';

export type GraphWriteGeneralityFixture =
  | 'actor_blueprint'
  | 'variables'
  | 'components'
  | 'custom_events'
  | 'event_dispatchers'
  | 'delegate_handlers'
  | 'widget_class'
  | 'asset_action_evidence';

export interface GraphWriteGeneralityOperationCase {
  operationId: string;
  owner: 'graph_write';
  publicKind: string;
  normalizedKind: string;
  operationField?: string;
  operationValue?: string;
  fieldScope?: string;
  cluster: GraphWriteGeneralityCluster;
  expectedNodeClass: string;
  fixtures: GraphWriteGeneralityFixture[];
  variantCount: number;
}

function op(input: Omit<GraphWriteGeneralityOperationCase, 'owner' | 'variantCount'>): GraphWriteGeneralityOperationCase {
  return { owner: 'graph_write', ...input, variantCount: GRAPHWRITE_GENERALITY_VARIANT_COUNT };
}

export const GRAPHWRITE_GENERALITY_OPERATION_MATRIX: GraphWriteGeneralityOperationCase[] = [
  op({ operationId: 'call.call', publicKind: 'call', normalizedKind: 'call', cluster: 'FunctionActionCluster', expectedNodeClass: 'K2Node_CallFunction', fixtures: ['actor_blueprint'] }),
  op({ operationId: 'field.set.variable', publicKind: 'set', normalizedKind: 'field', operationField: 'field_operation', operationValue: 'set', fieldScope: 'variable', cluster: 'FieldVariableActionCluster', expectedNodeClass: 'K2Node_VariableSet', fixtures: ['actor_blueprint', 'variables'] }),
  op({ operationId: 'field.set.property_path', publicKind: 'set_property', normalizedKind: 'field', operationField: 'field_operation', operationValue: 'set', fieldScope: 'property_path', cluster: 'FieldVariableActionCluster', expectedNodeClass: 'K2Node_CallFunction', fixtures: ['actor_blueprint', 'components'] }),
  op({ operationId: 'field.set.component_ref', publicKind: 'field', normalizedKind: 'field', operationField: 'field_operation', operationValue: 'set', fieldScope: 'component_ref', cluster: 'FieldVariableActionCluster', expectedNodeClass: 'K2Node_VariableSet', fixtures: ['actor_blueprint', 'components'] }),
  op({ operationId: 'field.set.field_access', publicKind: 'field', normalizedKind: 'field', operationField: 'field_operation', operationValue: 'set', fieldScope: 'field_access', cluster: 'FieldVariableActionCluster', expectedNodeClass: 'K2Node_CallFunction', fixtures: ['actor_blueprint', 'variables'] }),
  op({ operationId: 'field.get.variable', publicKind: 'get', normalizedKind: 'field', operationField: 'field_operation', operationValue: 'get', fieldScope: 'variable', cluster: 'FieldVariableActionCluster', expectedNodeClass: 'K2Node_VariableGet', fixtures: ['actor_blueprint', 'variables'] }),
  op({ operationId: 'field.get.property_path', publicKind: 'get_property', normalizedKind: 'field', operationField: 'field_operation', operationValue: 'get', fieldScope: 'property_path', cluster: 'FieldVariableActionCluster', expectedNodeClass: 'K2Node_CallFunction', fixtures: ['actor_blueprint', 'components'] }),
  op({ operationId: 'field.get.component_ref', publicKind: 'field', normalizedKind: 'field', operationField: 'field_operation', operationValue: 'get', fieldScope: 'component_ref', cluster: 'FieldVariableActionCluster', expectedNodeClass: 'K2Node_VariableGet', fixtures: ['actor_blueprint', 'components'] }),
  op({ operationId: 'field.get.field_access', publicKind: 'field', normalizedKind: 'field', operationField: 'field_operation', operationValue: 'get', fieldScope: 'field_access', cluster: 'FieldVariableActionCluster', expectedNodeClass: 'K2Node_CallFunction', fixtures: ['actor_blueprint', 'variables'] }),
  ...['add', 'subtract', 'multiply', 'divide', 'greater', 'greater_equal', 'less', 'less_equal', 'equal', 'not_equal'].map((operation) => op({ operationId: `op.${operation}`, publicKind: 'op', normalizedKind: 'op', operationField: 'op', operationValue: operation, cluster: 'FunctionActionCluster', expectedNodeClass: 'K2Node_PromotableOperator', fixtures: ['actor_blueprint'] })),
  op({ operationId: 'construct.construct', publicKind: 'construct', normalizedKind: 'construct', operationField: 'type_operation', operationValue: 'construct', cluster: 'GenericAssetStructControlActionCluster', expectedNodeClass: 'K2Node_MakeStruct', fixtures: ['actor_blueprint'] }),
  op({ operationId: 'deconstruct.deconstruct', publicKind: 'deconstruct', normalizedKind: 'deconstruct', operationField: 'type_operation', operationValue: 'deconstruct', cluster: 'GenericAssetStructControlActionCluster', expectedNodeClass: 'K2Node_BreakStruct', fixtures: ['actor_blueprint'] }),
  op({ operationId: 'select.select', publicKind: 'select', normalizedKind: 'select', operationField: 'control_operation', operationValue: 'select', cluster: 'GenericAssetStructControlActionCluster', expectedNodeClass: 'K2Node_Select', fixtures: ['actor_blueprint'] }),
  ...['branch', 'sequence', 'return'].map((operation) => op({ operationId: `control.${operation}`, publicKind: 'control', normalizedKind: operation, operationField: 'control', operationValue: operation, cluster: 'GenericAssetStructControlActionCluster', expectedNodeClass: operation === 'branch' ? 'K2Node_IfThenElse' : operation === 'sequence' ? 'K2Node_ExecutionSequence' : 'K2Node_FunctionResult', fixtures: ['actor_blueprint'] })),
  op({ operationId: 'event.custom_event', publicKind: 'custom_event_entry', normalizedKind: 'event', operationField: 'event_operation', operationValue: 'custom_event', cluster: 'EventDelegateActionCluster', expectedNodeClass: 'K2Node_CustomEvent', fixtures: ['actor_blueprint', 'custom_events'] }),
  op({ operationId: 'component_bound_event.bind', publicKind: 'component_bound_event', normalizedKind: 'component_bound_event', cluster: 'EventDelegateActionCluster', expectedNodeClass: 'K2Node_ComponentBoundEvent', fixtures: ['actor_blueprint', 'components', 'delegate_handlers'] }),
  ...['bind', 'assign', 'unbind', 'clear', 'call'].map((operation) => op({ operationId: `delegate.${operation}`, publicKind: operation === 'clear' ? 'delegate.unbind_all' : `delegate.${operation}`, normalizedKind: 'delegate', operationField: 'delegate_operation', operationValue: operation, cluster: 'EventDelegateActionCluster', expectedNodeClass: operation === 'bind' ? 'K2Node_AddDelegate' : operation === 'assign' ? 'K2Node_AssignDelegate' : operation === 'unbind' ? 'K2Node_RemoveDelegate' : operation === 'clear' ? 'K2Node_ClearDelegate' : 'K2Node_CallDelegate', fixtures: ['actor_blueprint', 'event_dispatchers', ...(operation === 'call' || operation === 'clear' ? [] : ['delegate_handlers'])] })),
  ...['spawn_actor', 'create_widget', 'construct_object', 'make_array', 'make_map', 'make_set', 'asset_action'].map((operation) => op({ operationId: `create.${operation}`, publicKind: 'create', normalizedKind: 'create', operationField: 'create_operation', operationValue: operation, cluster: 'GenericAssetStructControlActionCluster', expectedNodeClass: operation === 'spawn_actor' ? 'K2Node_SpawnActorFromClass' : operation === 'create_widget' ? 'K2Node_CreateWidget' : operation === 'construct_object' ? 'K2Node_GenericCreateObject' : operation === 'make_array' ? 'K2Node_MakeArray' : operation === 'make_map' ? 'K2Node_MakeMap' : operation === 'make_set' ? 'K2Node_MakeSet' : 'ActionDatabase', fixtures: operation === 'create_widget' ? ['actor_blueprint', 'widget_class'] : operation === 'asset_action' ? ['actor_blueprint', 'asset_action_evidence'] : ['actor_blueprint'] })),
  ...['dynamic_cast', 'class_cast', 'type_promotion'].map((operation) => op({ operationId: `convert.${operation}`, publicKind: 'convert', normalizedKind: 'convert', operationField: 'transform_operation', operationValue: operation, cluster: 'GenericAssetStructControlActionCluster', expectedNodeClass: operation === 'class_cast' ? 'K2Node_ClassDynamicCast' : operation === 'type_promotion' ? 'K2Node_PromotableOperator' : 'K2Node_DynamicCast', fixtures: ['actor_blueprint'] })),
  ...['timer_delegate_node', 'latent_or_async_node'].map((operation) => op({ operationId: `schedule.${operation}`, publicKind: 'schedule', normalizedKind: 'schedule', operationField: 'schedule_operation', operationValue: operation, cluster: 'GenericAssetStructControlActionCluster', expectedNodeClass: operation === 'timer_delegate_node' ? 'TimerDelegateNode' : 'LatentOrAsyncNode', fixtures: ['actor_blueprint', 'event_dispatchers'] })),
];
```

- [ ] **Step 2: Add matrix integrity tests**

Append these tests to `graphwrite-generality-matrix.test.ts`:

```ts
import {
  GRAPHWRITE_GENERALITY_OPERATION_MATRIX,
  GRAPHWRITE_GENERALITY_VARIANT_COUNT,
} from './graphwrite-generality-matrix.js';

test('GraphWrite generality matrix has stable unique operation ids and exactly ten variants per operation', () => {
  const ids = GRAPHWRITE_GENERALITY_OPERATION_MATRIX.map((entry) => entry.operationId);
  assert.equal(ids.length, new Set(ids).size);
  assert.ok(ids.length > 0);
  assert.ok(GRAPHWRITE_GENERALITY_OPERATION_MATRIX.every((entry) => entry.owner === 'graph_write'));
  for (const entry of GRAPHWRITE_GENERALITY_OPERATION_MATRIX) {
    assert.equal(entry.variantCount, GRAPHWRITE_GENERALITY_VARIANT_COUNT, entry.operationId);
    assert.match(entry.operationId, /^[a-z0-9_]+\.[a-z0-9_]+(\.[a-z0-9_]+)?$/);
    assert.ok(entry.expectedNodeClass.length > 0, entry.operationId);
  }
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

- [ ] **Step 1: Add TaskSpec factory types**

Create `AgentFaceService/task-core/src/task/testing/graphwrite-generality-spec-factory.ts`:

```ts
import type { TaskSpec } from '../schema/task-schemas.js';
import {
  GRAPHWRITE_GENERALITY_OPERATION_MATRIX,
  GRAPHWRITE_GENERALITY_VARIANT_COUNT,
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
  const expectedVariantNames = Array.from({ length: GRAPHWRITE_GENERALITY_VARIANT_COUNT }, (_, index) => variantName(operation.operationId, index));
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

- [ ] **Step 3: Add operation body generation**

Append operation generation to the same file:

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
    feature_name: `GraphWriteGenerality_${operation.operationId}`,
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
  const literalNumber = { kind: 'literal', value_type: 'number', value: index + 1 };
  const literalBool = { kind: 'literal', value_type: 'bool', value: index % 2 === 0 };
  if (operation.operationId === 'call.call') {
    return { id: name, kind: 'call', target: 'PrintString', args: { InString: `${name}_value` } };
  }
  if (operation.operationId.startsWith('field.set.')) {
    return makeFieldSetStatement(operation, name, index, literalNumber, literalBool);
  }
  if (operation.operationId.startsWith('field.get.')) {
    return { id: name, kind: 'call', target: 'PrintString', args: { InString: makeFieldGetExpression(operation, name) } };
  }
  if (operation.operationId.startsWith('op.')) {
    return { id: name, kind: 'call', target: 'PrintString', args: { InString: { kind: 'op', op: operation.operationValue, left: literalNumber, right: { kind: 'literal', value_type: 'number', value: 1 } } } };
  }
  if (operation.operationId === 'control.branch') {
    return { id: name, kind: 'control', control: 'branch', condition: literalBool, then: [{ kind: 'call', target: 'PrintString', args: { InString: `${name}_then` } }], else: [{ kind: 'call', target: 'PrintString', args: { InString: `${name}_else` } }] };
  }
  if (operation.operationId === 'control.sequence') {
    return { id: name, kind: 'control', control: 'sequence' };
  }
  if (operation.operationId === 'control.return') {
    return { id: name, kind: 'control', control: 'return' };
  }
  if (operation.operationId.startsWith('delegate.')) {
    return makeDelegateStatement(operation, name);
  }
  return makeGenericExpressionStatement(operation, name, index);
}
```

- [ ] **Step 4: Add complete branch helpers**

Append helper implementations to the same file:

```ts
function makeFieldSetStatement(
  operation: GraphWriteGeneralityOperationCase,
  name: string,
  index: number,
  literalNumber: Record<string, unknown>,
  literalBool: Record<string, unknown>,
): Record<string, unknown> {
  if (operation.fieldScope === 'variable') {
    return { id: name, kind: index < 5 ? 'set' : 'field', target: name, field_operation: 'set', field_scope: 'variable', value: index % 2 === 0 ? literalNumber : literalBool };
  }
  if (operation.fieldScope === 'property_path') {
    return { id: name, kind: index < 5 ? 'set_property' : 'field', target: 'RootComponent', property_path: 'RelativeLocation.X', field_operation: 'set', field_scope: 'property_path', value: literalNumber };
  }
  return { id: name, kind: 'field', target: name, field_operation: 'set', field_scope: operation.fieldScope, value: literalNumber, context_evidence: { field_name: name } };
}

function makeFieldGetExpression(operation: GraphWriteGeneralityOperationCase, name: string): Record<string, unknown> {
  if (operation.fieldScope === 'variable') {
    return { kind: 'get', target: name };
  }
  if (operation.fieldScope === 'property_path') {
    return { kind: 'get_property', target: 'RootComponent', property_path: 'RelativeLocation.X' };
  }
  return { kind: 'field', target: name, field_operation: 'get', field_scope: operation.fieldScope, context_evidence: { field_name: name } };
}

function makeDelegateStatement(operation: GraphWriteGeneralityOperationCase, name: string): Record<string, unknown> {
  const delegateName = `${name}_Dispatcher`;
  const handler = `${name}_Handler`;
  if (operation.operationValue === 'clear') {
    return { id: name, kind: 'delegate.unbind_all', target: 'self', delegate: delegateName };
  }
  if (operation.operationValue === 'call') {
    return { id: name, kind: 'delegate.call', target: 'self', delegate: delegateName, args: {} };
  }
  return { id: name, kind: `delegate.${operation.operationValue}`, target: 'self', delegate: delegateName, handler };
}

function makeGenericExpressionStatement(operation: GraphWriteGeneralityOperationCase, name: string, index: number): Record<string, unknown> {
  if (operation.operationId === 'construct.construct') {
    return { id: name, kind: 'call', target: 'PrintString', args: { InString: { kind: 'construct', type: '/Script/CoreUObject.Vector', args: { X: index, Y: index + 1, Z: index + 2 } } } };
  }
  if (operation.operationId === 'deconstruct.deconstruct') {
    return { id: name, kind: 'call', target: 'PrintString', args: { InString: { kind: 'deconstruct', type: '/Script/CoreUObject.Vector', value: { kind: 'construct', type: '/Script/CoreUObject.Vector', args: { X: index, Y: index, Z: index } } } } };
  }
  if (operation.operationId === 'select.select') {
    return { id: name, kind: 'call', target: 'PrintString', args: { InString: { kind: 'select', condition: { kind: 'literal', value_type: 'bool', value: index % 2 === 0 }, options: [`${name}_A`, `${name}_B`] } } };
  }
  if (operation.publicKind === 'create') {
    return { id: name, kind: 'call', target: 'PrintString', args: { InString: { kind: 'create', create_operation: operation.operationValue, class_path: '/Script/Engine.Actor', pin_type: 'float', args: {} } } };
  }
  if (operation.publicKind === 'convert') {
    return { id: name, kind: 'call', target: 'PrintString', args: { InString: { kind: 'convert', transform_operation: operation.operationValue, target_class_path: '/Script/Engine.Actor', args: { value: { kind: 'literal', value_type: 'object', value: 'self' } } } } };
  }
  if (operation.publicKind === 'schedule') {
    return { id: name, kind: 'call', target: 'PrintString', args: { InString: { kind: 'schedule', schedule_operation: operation.operationValue, args: { delay: { kind: 'literal', value_type: 'number', value: 0.1 } } } } };
  }
  if (operation.operationId === 'event.custom_event') {
    return { id: name, kind: 'call', target: 'PrintString', args: { InString: `${name}_event_body` } };
  }
  if (operation.operationId === 'component_bound_event.bind') {
    return { id: name, kind: 'component_bound_event', component: `${name}_Component`, delegate: 'OnComponentBeginOverlap', handler: `${name}_Handler` };
  }
  return { id: name, kind: 'call', target: 'PrintString', args: { InString: name } };
}
```

- [ ] **Step 5: Add compiler generation tests**

Append this test:

```ts
import { compileTaskSpecToTaskPlan, taskPlanToAppendBridgePayload } from '../compiler/task-compiler.js';
import { makeGraphWriteGeneralityBundles } from './graphwrite-generality-spec-factory.js';

test('GraphWrite generality factory emits one TaskSpec per operation with ten distinct variant ids', () => {
  const bundles = makeGraphWriteGeneralityBundles({
    assetPath: '/Game/BlueprintHelper/Generality/BP_GraphWriteGenerality',
    graphName: 'EventGraph',
  });
  for (const bundle of bundles) {
    assert.equal(bundle.expectedVariantNames.length, 10, bundle.operation.operationId);
    assert.equal(new Set(bundle.expectedVariantNames).size, 10, bundle.operation.operationId);
    const plan = compileTaskSpecToTaskPlan(bundle.graphWriteSpec);
    const payload = taskPlanToAppendBridgePayload(plan, true);
    const logicSpec = payload.logic_spec as { statements?: unknown[] };
    assert.ok(Array.isArray(logicSpec.statements), bundle.operation.operationId);
    assert.equal(logicSpec.statements.length, 10, bundle.operation.operationId);
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
  previewStatus: 'pass' | 'fail';
  executeStatus: 'pass' | 'fail';
  readbackStatus: 'pass' | 'fail';
  failureKind: 'none' | 'setup_failure' | 'preview_failure' | 'execute_failure' | 'readback_failure' | 'unsupported_intent' | 'silent_wrong_graph';
  expectedNodeClass: string;
  actualNodeClass?: string;
  evidencePath?: string;
  message?: string;
}

export interface GraphWriteGeneralityOperationSummary {
  operationId: string;
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
    return {
      operationId,
      totalVariants: variants.length,
      passedVariants,
      operationPassed: variants.length === 10 && passedVariants === 10,
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
  const header = 'operation_id,variant_name,preview_status,execute_status,readback_status,failure_kind,expected_node_class,actual_node_class,evidence_path,message';
  const rows = results.map((result) => [
    result.operationId,
    result.variantName,
    result.previewStatus,
    result.executeStatus,
    result.readbackStatus,
    result.failureKind,
    result.expectedNodeClass,
    result.actualNodeClass ?? '',
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
    `| ${operation.operationId} | ${operation.operationPassed ? 'PASS' : 'FAIL'} | ${operation.passedVariants}/10 | ${JSON.stringify(operation.failureKinds)} |`
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

test('GraphWrite generality report requires all ten variants for an operation pass and renders charts', () => {
  const results: GraphWriteGeneralityVariantResult[] = Array.from({ length: 10 }, (_, index) => ({
    operationId: 'call.call',
    variantName: `GWGen_call_call_${String(index).padStart(2, '0')}`,
    previewStatus: 'pass',
    executeStatus: index === 9 ? 'fail' : 'pass',
    readbackStatus: index === 9 ? 'fail' : 'pass',
    failureKind: index === 9 ? 'execute_failure' : 'none',
    expectedNodeClass: 'K2Node_CallFunction',
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
  "totalVariants": "<totalOperations * 10>"
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

- [ ] Public TaskSpec contract and compiler-supported GraphWrite shapes are synchronized.
- [ ] Operation matrix covers all ownership-filtered normalized GraphWrite operations listed in this plan after excluding existing-tool-owned declarations, fixtures, and mutation-service-owned operations.
- [ ] Every operation generates exactly 10 variant names and one TaskSpec body containing exactly 10 GraphWrite statements or expression-backed nodes.
- [ ] Runtime preflight executes through TaskSpec preview/execute/readback; ActionResolution direct tests are not counted as score.
- [ ] Report artifacts include JSON, CSV, Markdown, operation pass/fail SVG, and failure distribution SVG.
- [ ] Final P6/80% total test is gated by `allOperationsPassed=true`.
- [ ] No automatic git staging, commit, or push was performed.

## Suggested Manual Commit Message

变更需求：
1. 新增 GraphWrite 最终总测前置通用性测试计划，覆盖每个 kind + operation 的 10 变体 TaskSpec 门禁。
2. 固定通用性报告输出为 JSON/CSV/Markdown/SVG，并要求最终 P6/80% 总测先通过通用性 gate。
