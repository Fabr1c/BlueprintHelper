# GraphWrite ContainerAction First-Class Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为普通 Blueprint 的核心 array/map/set 容器操作新增 first-class `container_action` 语义、TaskSpec public shape、FunctionAction-backed 解析执行和可靠 readback gate。

**Architecture:** `container_action` 是 Agent-facing 的稳定容器语义层，不是 `asset_action` 兜底，也不是一批 `UK2Node_*` 特判。Public TaskSpec 使用 `kind=container_action`；C++ 侧通过独立 vocabulary/resolver 将容器语义投影为 UE 标准 callable/action evidence，并复用 FunctionAction/shared adapter 生成节点。Review evidence 继续使用已决策的 graph-level `graph_block`，不引入 container/action atomic target。

**Tech Stack:** TypeScript task-core compiler/schema, BlueprintHelper TaskSpec v1, UE 5.6 GraphWrite SemanticIR/ActionResolution/FragmentDAG, C++ automation tests, Blueprint readback/compile assertions.

---

## 2026-05-25 Implementation Status

- Status: IMPLEMENTED / FOCUSED GATE PASS.
- TypeScript side now exposes first-class `container_action` in TaskSpec contract/schema/compiler lowering and capability contract tests.
- C++ side now has first-class `ContainerAction` SemanticIR, ActionContext demand projection, data-driven vocabulary, FunctionAction-backed resolver, fragment/DAG role links, typed wildcard promotion, readback verifier, and focused automation.
- Focused runtime evidence: `Automation RunTests BlueprintHelper.GraphWrite.ContainerAction` found 9 tests and passed `SemanticIR`, `ContractValidation`, `ActionContext`, `Vocabulary`, `Resolver`, `FragmentDag`, `ArrayResultFragmentDag`, `EndpointPinTypeJsonRoundTrip`, and `FocusedE2E`.
- Focused E2E covers `array.add`, `map.contains`, and `set.to_array` against a transient Blueprint with real array/map/set member variables, target links, wildcard promotion, typed role/output readback, result output, and Blueprint compile gate.
- Array-shaped query evidence: `ArrayResultFragmentDag` verifies `map.keys`, `map.values`, and `set.to_array` expose first-class `array` result-symbol container metadata in the GraphFragment DAG instead of only scalar type tokens.
- JSON ingestion evidence: `EndpointPinTypeJsonRoundTrip` verifies serialized fragment endpoints preserve `pin_type` category/container metadata when read back through the shared GraphFragment DAG model parser.
- Runtime contract evidence: direct C++ `logic_spec` rejects unsupported operations, missing required roles, and `result_symbol` on mutating operations before resolver fallback.
- Full runtime evidence: `Automation RunTests BlueprintHelper.GraphWrite` found 203 tests and exited with code 0 after the ContainerAction implementation.
- Remaining outside this plan: ownership-filtered final generality preflight remains governed by `BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md`.

---

## Scope Decisions

### First-Class ContainerAction V1

V1 只覆盖普通 Blueprint 中高频、标准、可由容器 target/type template 稳定描述并能 readback 的 array/map/set 操作。`make_array`、`make_map`、`make_set` 已由 Generic create first slice 覆盖，不进入本计划。struct make/break、field get/set、component_ref、project-specific helper 函数继续由既有簇负责。

| OperationId | Public shape | Runtime owner | Notes |
|---|---|---|---|
| `container.array.get` | expression | container_action -> FunctionAction-backed callable | Reads array item by index. |
| `container.array.set` | statement | container_action -> FunctionAction-backed callable | Writes item by index. |
| `container.array.add` | statement | container_action -> FunctionAction-backed callable | Mutates target array. |
| `container.array.add_unique` | statement | container_action -> FunctionAction-backed callable | Mutates target array. |
| `container.array.append` | statement | container_action -> FunctionAction-backed callable | Appends array/items compatible with element type. |
| `container.array.insert` | statement | container_action -> FunctionAction-backed callable | Mutates target array at index. |
| `container.array.remove_item` | statement | container_action -> FunctionAction-backed callable | Removes matching item. |
| `container.array.remove_index` | statement | container_action -> FunctionAction-backed callable | Removes item at index. |
| `container.array.clear` | statement | container_action -> FunctionAction-backed callable | Clears target array. |
| `container.array.contains` | expression | container_action -> FunctionAction-backed callable | Pure/query style result. |
| `container.array.find` | expression | container_action -> FunctionAction-backed callable | Returns index/result. |
| `container.array.length` | expression | container_action -> FunctionAction-backed callable | Returns integer length. |
| `container.map.add` | statement | container_action -> FunctionAction-backed callable | Mutates target map. |
| `container.map.remove` | statement | container_action -> FunctionAction-backed callable | Mutates target map. |
| `container.map.find` | expression | container_action -> FunctionAction-backed callable | Query by key. |
| `container.map.contains` | expression | container_action -> FunctionAction-backed callable | Query by key. |
| `container.map.keys` | expression | container_action -> FunctionAction-backed callable | Returns key array. |
| `container.map.values` | expression | container_action -> FunctionAction-backed callable | Returns value array. |
| `container.map.clear` | statement | container_action -> FunctionAction-backed callable | Clears target map. |
| `container.map.length` | expression | container_action -> FunctionAction-backed callable | Returns integer length. |
| `container.set.add` | statement | container_action -> FunctionAction-backed callable | Mutates target set. |
| `container.set.remove` | statement | container_action -> FunctionAction-backed callable | Mutates target set. |
| `container.set.contains` | expression | container_action -> FunctionAction-backed callable | Query by item. |
| `container.set.clear` | statement | container_action -> FunctionAction-backed callable | Clears target set. |
| `container.set.length` | expression | container_action -> FunctionAction-backed callable | Returns integer length. |
| `container.set.to_array` | expression | container_action -> FunctionAction-backed callable | Converts set to array. |

### Kept In FunctionAction

| Case | Reason |
|---|---|
| Project/plugin helper functions that accept array/map/set parameters | They are ordinary callable functions, not stable GraphWrite container vocabulary. |
| Sort/filter/predicate/custom comparator operations | They need predicate/body/delegate semantics and should stay FunctionAction or a future dedicated transform/control feature. |
| Non-standard editor/plugin menu actions | They are not stable UE baseline container operations. |
| Operations already expressible as plain `call` and not requiring container wildcard/readback semantics | FunctionAction is sufficient and should remain the owner. |

### Explicitly Not In ContainerAction V1

| Case | Owner |
|---|---|
| `make_array` / `make_map` / `make_set` | Generic create first slice. |
| struct `make` / `break` | construct/deconstruct. |
| `foreach` / body-producing loop nodes | Future `control.foreach` or control-flow plan, because it owns body graph/exec flow rather than a single container callable. |
| Review target granularity below graph block | Existing GraphWrite `graph_block` Review policy. |

## Public Shape

Canonical statement shape:

```json
{
  "kind": "container_action",
  "container_kind": "array",
  "container_operation": "add",
  "target": { "kind": "get", "name": "Items" },
  "item": { "kind": "literal", "value": 7 },
  "element_type": "int"
}
```

Canonical expression shape:

```json
{
  "kind": "container_action",
  "container_kind": "map",
  "container_operation": "contains",
  "target": { "kind": "get", "name": "Scores" },
  "key": { "kind": "literal", "value": "PlayerA" },
  "key_type": "string",
  "value_type": "int"
}
```

Statement result binding shape:

```json
{
  "kind": "container_action",
  "container_kind": "array",
  "container_operation": "find",
  "target": { "kind": "get", "name": "Items" },
  "item": { "kind": "literal", "value": 7 },
  "element_type": "int",
  "result_symbol": "FoundIndex"
}
```

Field contract:

| Field | Required when | Meaning |
|---|---|---|
| `kind` | always | Must be `container_action`. |
| `container_kind` | always | `array`, `map`, or `set`. |
| `container_operation` | always | Snake-case operation token from V1 vocabulary. |
| `target` | always | Container variable/field/expression. String shorthand may compile to `{ "kind": "get", "name": "<string>" }`. |
| `element_type` | array/set when target type cannot infer it | Desired element pin type token. |
| `key_type` / `value_type` | map when target type cannot infer them | Desired map key/value pin type tokens. |
| `item` | item-based array/set operations | Value expression for element/item pin. |
| `items` | append operations | Array expression or repeated element expressions. |
| `key` / `value` | map key/value operations | Key/value expressions. |
| `index` | index-based array operations | Integer expression. |
| `result_symbol` | statement form of query operations | Symbol name for downstream statements. |
| `context_evidence` | only for projected evidence | Optional compiler/runtime evidence passthrough; not used as a hardcoded selector. |

## Readback Pass Criteria

An operation passes only when all applicable assertions are true:

| Gate | Required assertion |
|---|---|
| Node identity | Readback identifies a container callable/action matching `container_kind + container_operation`, not merely any function node. |
| Target link | Target container pin is linked to the expected variable/field/expression. |
| Wildcard promotion | Container/item/key/value/result pins are not unresolved wildcard pins after node reconstruction. |
| Type compatibility | Promoted pin types match target inference or explicit `element_type` / `key_type` / `value_type`. |
| Input values | `item`, `items`, `key`, `value`, and `index` defaults or links match the TaskSpec. |
| Output use | Expression/query operations expose the expected output pin and `result_symbol` can be consumed by following statements. |
| Exec flow | Mutating statement operations have valid exec input/output links; pure query expressions are not required to have exec pins. |
| Compile result | Blueprint compile returns no errors. Warnings are allowed only when the warning is unrelated to the generated graph and is recorded in the smoke report. |
| Review evidence | GraphWrite Review evidence contains the existing `graph_block` target with asset path, graph name, operation kind, and task step index. |

---

## File Structure

### TypeScript TaskSpec Surface

- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
  - Add `container_action` to supported GraphWrite statement/expression surface.
  - Add a contract section listing V1 operation ids and FunctionAction-backed runtime owner.
- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
  - Add strict public shape validation helpers for `container_action`.
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
  - Add a `container_action` capability group or operation family with `reviewEvidence="graph_surface_atomic_target"`.
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - Normalize public `container_action` statement/expression to canonical internal GraphWrite payload.
- Create: `AgentFaceService/task-core/src/task/compiler/task-compiler.container-action.test.ts`
  - Cover schema/compile lowering for array/map/set mutating and query forms.

### C++ Semantic And Resolution

- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
  - Add `ContainerAction` statement/expression kinds and explicit container fields.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.cpp`
  - Parse `container_kind`, `container_operation`, `element_type`, `key_type`, `value_type`, and role expressions.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - Project container kind, operation, target type, and pin-type demands into ActionContext.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
  - Add `EBlueprintHelperActionSemanticKind::ContainerAction`.
  - Add explicit `ContainerKind` and `ContainerOperation` constraints.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp`
  - Add `container_action` string conversion and validation.
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h`
  - Central V1 operation vocabulary and role requirements.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.cpp`
  - Data-driven operation table.
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.h`
  - Resolver boundary that maps container semantic constraints to FunctionAction-compatible callable evidence.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.cpp`
  - Reuses ActionDatabase/function resolution; does not call `UBlueprintNodeSpawner::Create` directly.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.cpp`
  - Route `ContainerAction` semantic kind to `FBlueprintHelperContainerActionResolver`.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
  - Build container statement/expression fragments through the shared action spawn coordinator.

### Readback And Tests

- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperContainerActionReadbackVerifier.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperContainerActionReadbackVerifier.cpp`
  - Validate node identity, target links, wildcard promotion, pin values, result pins, exec flow, and compile status.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperContainerActionTests.cpp`
  - Focused C++ resolver/builder/readback tests.
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md`
  - Add `container_action` V1 operation ids to the final ownership-filtered matrix after implementation.
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`
  - Mark broad `container_action` as covered when implementation and readback pass.

---

## Task 1: Add TaskSpec Contract And Public Shape Tests

**Files:**
- Modify: `AgentFaceService/task-core/src/task/schema/task-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
- Create: `AgentFaceService/task-core/src/task/compiler/task-compiler.container-action.test.ts`

- [ ] **Step 1: Add red tests for public statement and expression shapes**

Create `AgentFaceService/task-core/src/task/compiler/task-compiler.container-action.test.ts`:

```ts
import assert from 'node:assert/strict';
import { test } from 'node:test';

import { compileTaskSpecToTaskPlan, taskPlanToAppendBridgePayload } from './task-compiler.js';

function makeContainerSpec(statement: Record<string, unknown>) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    target: {
      asset_path: '/Game/BH_Tests/BP_GraphWriteContainer',
      asset_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [
        {
          kind: 'custom_event',
          name: 'GW_ContainerSmoke',
          body: [statement],
        },
      ],
    },
  };
}

test('container_action array add lowers as first-class GraphWrite statement', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeContainerSpec({
    kind: 'container_action',
    container_kind: 'array',
    container_operation: 'add',
    target: { kind: 'get', name: 'Items' },
    item: { kind: 'literal', value: 7 },
    element_type: 'int',
  }) as never);

  const payload = taskPlanToAppendBridgePayload(taskPlan.steps.find((step) => step.capability === 'graph_write') as never);
  const statement = payload.logic_spec.statements[0];

  assert.equal(statement.kind, 'container_action');
  assert.equal(statement.container_kind, 'array');
  assert.equal(statement.container_operation, 'add');
  assert.equal(statement.element_type, 'int');
  assert.deepEqual(statement.target, { kind: 'get', name: 'Items' });
  assert.deepEqual(statement.item, { kind: 'literal', value: 7 });
});

test('container_action map contains lowers as first-class GraphWrite expression', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeContainerSpec({
    kind: 'let',
    name: 'bHasScore',
    value: {
      kind: 'container_action',
      container_kind: 'map',
      container_operation: 'contains',
      target: { kind: 'get', name: 'Scores' },
      key: { kind: 'literal', value: 'PlayerA' },
      key_type: 'string',
      value_type: 'int',
    },
  }) as never);

  const payload = taskPlanToAppendBridgePayload(taskPlan.steps.find((step) => step.capability === 'graph_write') as never);
  const value = payload.logic_spec.statements[0].value;

  assert.equal(value.kind, 'container_action');
  assert.equal(value.container_kind, 'map');
  assert.equal(value.container_operation, 'contains');
  assert.equal(value.key_type, 'string');
  assert.equal(value.value_type, 'int');
});

test('container_action rejects unsupported foreach in V1', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeContainerSpec({
      kind: 'container_action',
      container_kind: 'array',
      container_operation: 'foreach',
      target: { kind: 'get', name: 'Items' },
      element_type: 'int',
    }) as never),
    (err: unknown) => err instanceof Error && err.message.includes('Unsupported container_operation'),
  );
});
```

- [ ] **Step 2: Run the red tests**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run test:node -- task-compiler.container-action.test
```

Expected: the new tests fail because `container_action` is not yet in the supported statement/expression surface.

- [ ] **Step 3: Add schema/contract vocabulary**

In `AgentFaceService/task-core/src/task/schema/task-contract.ts`, add `container_action` to the GraphWrite statement and expression surfaces and add the V1 operation list:

```ts
container_action: {
  public_shape: 'kind=container_action with container_kind, container_operation, target, typed role expressions',
  runtime_owner: 'FunctionAction-backed container resolver',
  review_evidence: 'graph_surface_atomic_target',
  first_class_operations: [
    'container.array.get',
    'container.array.set',
    'container.array.add',
    'container.array.add_unique',
    'container.array.append',
    'container.array.insert',
    'container.array.remove_item',
    'container.array.remove_index',
    'container.array.clear',
    'container.array.contains',
    'container.array.find',
    'container.array.length',
    'container.map.add',
    'container.map.remove',
    'container.map.find',
    'container.map.contains',
    'container.map.keys',
    'container.map.values',
    'container.map.clear',
    'container.map.length',
    'container.set.add',
    'container.set.remove',
    'container.set.contains',
    'container.set.clear',
    'container.set.length',
    'container.set.to_array',
  ],
  excluded_operations: [
    'make_array',
    'make_map',
    'make_set',
    'foreach',
    'custom predicate operations',
  ],
}
```

In the `supported_first_slice.statement_kinds` array, add:

```ts
'container_action',
```

In the `supported_first_slice.expression_kinds` array, add:

```ts
'container_action',
```

- [ ] **Step 4: Add validation helpers**

In `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`, add these constants near the other GraphWrite kind constants:

```ts
const CONTAINER_ACTION_KIND = 'container_action';
const SUPPORTED_CONTAINER_KINDS = new Set(['array', 'map', 'set']);
const SUPPORTED_CONTAINER_OPERATIONS = new Map<string, ReadonlySet<string>>([
  ['array', new Set(['get', 'set', 'add', 'add_unique', 'append', 'insert', 'remove_item', 'remove_index', 'clear', 'contains', 'find', 'length'])],
  ['map', new Set(['add', 'remove', 'find', 'contains', 'keys', 'values', 'clear', 'length'])],
  ['set', new Set(['add', 'remove', 'contains', 'clear', 'length', 'to_array'])],
]);
```

Add this validation helper:

```ts
function validateContainerActionShape(record: Record<string, unknown>, path: string): void {
  const containerKind = getRequiredString(record, 'container_kind', `${path}.container_kind`).trim().toLowerCase();
  const containerOperation = getRequiredString(record, 'container_operation', `${path}.container_operation`).trim().toLowerCase();
  if (!SUPPORTED_CONTAINER_KINDS.has(containerKind)) {
    throw new TaskSpecCompileError('unsupported_container_kind', `Unsupported container_kind: ${containerKind}`, [
      { code: 'unsupported_container_kind', path: `${path}.container_kind`, message: 'Use array, map, or set.' },
    ]);
  }
  if (!SUPPORTED_CONTAINER_OPERATIONS.get(containerKind)?.has(containerOperation)) {
    throw new TaskSpecCompileError('unsupported_container_operation', `Unsupported container_operation: ${containerKind}.${containerOperation}`, [
      { code: 'unsupported_container_operation', path: `${path}.container_operation`, message: 'Use a first-class V1 container operation.' },
    ]);
  }
  if (!Object.hasOwn(record, 'target')) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'container_action requires target.', [
      { code: 'taskspec_semantic_invalid', path: `${path}.target`, message: 'Provide the target container expression.' },
    ]);
  }
}
```

- [ ] **Step 5: Wire validation into statements and expressions**

In `validateSupportedStatements`, allow `container_action`:

```ts
} else if (kind === CONTAINER_ACTION_KIND) {
  validateContainerActionShape(statementRecord, statementPath);
  validateSupportedExpression(statementRecord.target, `${statementPath}.target`);
  for (const role of ['item', 'items', 'key', 'value', 'index'] as const) {
    if (Object.hasOwn(statementRecord, role)) {
      validateSupportedExpression(statementRecord[role], `${statementPath}.${role}`);
    }
  }
```

In `validateSupportedExpression`, allow `container_action` with the same role validation.

- [ ] **Step 6: Re-run TypeScript tests**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected: all existing tests pass plus the new `container_action` tests.

---

## Task 2: Lower ContainerAction Without Smuggling It Through Plain Call

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Test: `AgentFaceService/task-core/src/task/compiler/task-compiler.container-action.test.ts`

- [ ] **Step 1: Add canonical copy function**

Add this helper next to `copyConvertScheduleSemanticFields`:

```ts
const GRAPH_CONTAINER_ACTION_FIELDS = [
  'container_kind',
  'container_operation',
  'element_type',
  'key_type',
  'value_type',
  'target',
  'item',
  'items',
  'key',
  'value',
  'index',
  'result_symbol',
  'context_evidence',
] as const;

function copyContainerActionSemanticFields(source: Record<string, unknown>, target: Record<string, unknown>): void {
  GRAPH_CONTAINER_ACTION_FIELDS.forEach((field) => {
    if (Object.hasOwn(source, field)) {
      target[field] = source[field];
    }
  });
}
```

- [ ] **Step 2: Preserve canonical shape during statement lowering**

In the statement normalization branch, add:

```ts
if (kind === CONTAINER_ACTION_KIND) {
  const lowered: Record<string, unknown> = { kind: CONTAINER_ACTION_KIND };
  copyContainerActionSemanticFields(statementRecord, lowered);
  return lowered as BlueprintLogicStatement;
}
```

- [ ] **Step 3: Preserve canonical shape during expression lowering**

In the expression normalization branch, add:

```ts
if (kind === CONTAINER_ACTION_KIND) {
  const lowered: Record<string, unknown> = { kind: CONTAINER_ACTION_KIND };
  copyContainerActionSemanticFields(expressionRecord, lowered);
  return lowered as BlueprintLogicExpression;
}
```

- [ ] **Step 4: Verify no legacy `call` downgrade**

Extend `task-compiler.container-action.test.ts`:

```ts
test('container_action does not lower to plain call', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeContainerSpec({
    kind: 'container_action',
    container_kind: 'set',
    container_operation: 'contains',
    target: { kind: 'get', name: 'Tags' },
    item: { kind: 'literal', value: 'Ready' },
    element_type: 'string',
    result_symbol: 'bHasReady',
  }) as never);

  const payload = taskPlanToAppendBridgePayload(taskPlan.steps.find((step) => step.capability === 'graph_write') as never);
  assert.equal(payload.logic_spec.statements[0].kind, 'container_action');
  assert.notEqual(payload.logic_spec.statements[0].kind, 'call');
});
```

- [ ] **Step 5: Run TypeScript tests**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected: all tests pass.

---

## Task 3: Add C++ SemanticIR Container Fields

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperContainerActionTests.cpp`

- [ ] **Step 1: Add red parser test**

Create `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperContainerActionTests.cpp` with an initial parser test:

```cpp
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionSemanticIRTest,
	"BlueprintHelper.GraphWrite.ContainerAction.SemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionSemanticIRTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintHelper.GraphWrite.SemanticIR.v1",
		"entry": { "kind": "custom_event", "name": "GW_Container" },
		"statements": [{
			"kind": "container_action",
			"container_kind": "array",
			"container_operation": "add",
			"target": { "kind": "get", "name": "Items" },
			"item": { "kind": "literal", "value": 7 },
			"element_type": "int"
		}]
	})JSON");

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("json parses"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());

	FBlueprintHelperGraphSemanticIR IR;
	TArray<FBlueprintHelperGraphSemanticDiagnostic> Diagnostics;
	const bool bBuilt = FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(Root, IR, Diagnostics);

	TestTrue(TEXT("semantic ir builds"), bBuilt);
	TestEqual(TEXT("one statement"), IR.Statements.Num(), 1);
	TestEqual(TEXT("statement kind"), IR.Statements[0]->Kind, EBlueprintHelperGraphStatementKind::ContainerAction);
	TestEqual(TEXT("container kind"), IR.Statements[0]->ContainerKind, FString(TEXT("array")));
	TestEqual(TEXT("container operation"), IR.Statements[0]->ContainerOperation, FString(TEXT("add")));
	TestEqual(TEXT("element type"), IR.Statements[0]->PinType, FString(TEXT("int")));
	return true;
}
```

- [ ] **Step 2: Add enum and fields**

In `BlueprintHelperGraphSemanticIR.h`, add `ContainerAction` to both statement and expression enums:

```cpp
ContainerAction,
```

Add explicit fields to `FBlueprintHelperGraphExpressionIR` and `FBlueprintHelperGraphStatementIR`:

```cpp
FString ContainerKind;
FString ContainerOperation;
```

Reuse existing `PinType`, `KeyPinType`, `ValuePinType`, `TargetObject`, `Value`, and `Args` for role values; do not introduce duplicate role storage unless readback requires it.

- [ ] **Step 3: Parse JSON fields**

In `BlueprintGraphJsonParser.cpp`, map `kind=container_action` to `ContainerAction` and read:

```cpp
NodeObject->TryGetStringField(TEXT("container_kind"), Result.ContainerKind);
NodeObject->TryGetStringField(TEXT("container_operation"), Result.ContainerOperation);
NodeObject->TryGetStringField(TEXT("element_type"), Result.PinType);
NodeObject->TryGetStringField(TEXT("key_type"), Result.KeyPinType);
NodeObject->TryGetStringField(TEXT("value_type"), Result.ValuePinType);
```

Map role expressions:

```cpp
ReadExpressionField(NodeObject, TEXT("target"), Result.TargetObject);
ReadExpressionField(NodeObject, TEXT("item"), Result.Args.FindOrAdd(TEXT("item")));
ReadExpressionField(NodeObject, TEXT("items"), Result.Args.FindOrAdd(TEXT("items")));
ReadExpressionField(NodeObject, TEXT("key"), Result.Args.FindOrAdd(TEXT("key")));
ReadExpressionField(NodeObject, TEXT("value"), Result.Args.FindOrAdd(TEXT("value")));
ReadExpressionField(NodeObject, TEXT("index"), Result.Args.FindOrAdd(TEXT("index")));
```

Use the parser's existing expression-field helper names in the actual patch; if the helper has a different local name, keep the same behavior and avoid a new parser abstraction.

- [ ] **Step 4: Add ActionResolution semantic kind**

In `BlueprintHelperActionResolutionCore.h`, add:

```cpp
ContainerAction,
```

to `EBlueprintHelperActionSemanticKind`, then add fields to `FBlueprintHelperActionSemanticConstraints`:

```cpp
FString ContainerKind;
FString ContainerOperation;
```

In `BlueprintHelperActionResolutionCore.cpp`, add:

```cpp
case EBlueprintHelperActionSemanticKind::ContainerAction: return TEXT("container_action");
```

- [ ] **Step 5: Project ActionContext demand**

In `BlueprintHelperActionContextDemandCollector.cpp`, add an `ApplyContainerActionStatementEvidence` and expression equivalent:

```cpp
static void ApplyContainerActionStatementEvidence(
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperActionContextDemand& InOutDemand)
{
	if (InOutDemand.SemanticKind != EBlueprintHelperActionSemanticKind::ContainerAction)
	{
		return;
	}

	InOutDemand.ContainerKind = Statement.ContainerKind.TrimStartAndEnd().ToLower();
	InOutDemand.ContainerOperation = Statement.ContainerOperation.TrimStartAndEnd().ToLower();
	InOutDemand.Query = InOutDemand.ContainerKind + TEXT(".") + InOutDemand.ContainerOperation;
	if (!Statement.PinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("element"), Statement.PinType.TrimStartAndEnd());
		InOutDemand.ContainerElementPinType = MakePinTypeFromToken(Statement.PinType);
	}
	if (!Statement.KeyPinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("key"), Statement.KeyPinType.TrimStartAndEnd());
		InOutDemand.ContainerKeyPinType = MakePinTypeFromToken(Statement.KeyPinType);
	}
	if (!Statement.ValuePinType.TrimStartAndEnd().IsEmpty())
	{
		InOutDemand.ArgumentTypes.Add(TEXT("value"), Statement.ValuePinType.TrimStartAndEnd());
		InOutDemand.ContainerValuePinType = MakePinTypeFromToken(Statement.ValuePinType);
	}
}
```

Add matching fields to `FBlueprintHelperActionContextDemand` if they do not already exist:

```cpp
FString ContainerKind;
FString ContainerOperation;
```

- [ ] **Step 6: Run parser-focused automation**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ContainerAction.SemanticIR;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: the SemanticIR test passes.

---

## Task 4: Add Data-Driven Container Vocabulary

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperContainerActionTests.cpp`

- [ ] **Step 1: Add red vocabulary test**

Append this test to `BlueprintHelperContainerActionTests.cpp`:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionVocabularyTest,
	"BlueprintHelper.GraphWrite.ContainerAction.Vocabulary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionVocabularyTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperContainerActionSpec* ArrayAdd =
		FBlueprintHelperContainerActionVocabulary::Find(TEXT("array"), TEXT("add"));
	TestNotNull(TEXT("array add vocabulary"), ArrayAdd);
	TestEqual(TEXT("array add operation id"), ArrayAdd->OperationId, FString(TEXT("container.array.add")));
	TestTrue(TEXT("array add mutates"), ArrayAdd->bMutatesTarget);
	TestTrue(TEXT("array add requires item"), ArrayAdd->RequiredRoles.Contains(TEXT("item")));

	const FBlueprintHelperContainerActionSpec* MapContains =
		FBlueprintHelperContainerActionVocabulary::Find(TEXT("map"), TEXT("contains"));
	TestNotNull(TEXT("map contains vocabulary"), MapContains);
	TestFalse(TEXT("map contains is query"), MapContains->bMutatesTarget);
	TestTrue(TEXT("map contains requires key"), MapContains->RequiredRoles.Contains(TEXT("key")));

	TestNull(TEXT("foreach excluded"), FBlueprintHelperContainerActionVocabulary::Find(TEXT("array"), TEXT("foreach")));
	return true;
}
```

- [ ] **Step 2: Add vocabulary header**

Create `BlueprintHelperContainerActionVocabulary.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperContainerActionSpec
{
	FString OperationId;
	FString ContainerKind;
	FString ContainerOperation;
	FString FunctionQuery;
	TArray<FString> RequiredRoles;
	bool bMutatesTarget = false;
	bool bPureQuery = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperContainerActionVocabulary
{
public:
	static const FBlueprintHelperContainerActionSpec* Find(const FString& ContainerKind, const FString& ContainerOperation);
	static TArray<FBlueprintHelperContainerActionSpec> All();
};
```

- [ ] **Step 3: Add data-driven table**

Create `BlueprintHelperContainerActionVocabulary.cpp` with a static table. The `FunctionQuery` value should match UE menu/function search text, not a raw node class:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"

namespace
{
static FString Normalize(const FString& Value)
{
	return Value.TrimStartAndEnd().ToLower();
}

static const TArray<FBlueprintHelperContainerActionSpec>& Specs()
{
	static const TArray<FBlueprintHelperContainerActionSpec> Items = {
		{ TEXT("container.array.get"), TEXT("array"), TEXT("get"), TEXT("Array Get"), { TEXT("target"), TEXT("index") }, false, true },
		{ TEXT("container.array.set"), TEXT("array"), TEXT("set"), TEXT("Array Set"), { TEXT("target"), TEXT("index"), TEXT("item") }, true, false },
		{ TEXT("container.array.add"), TEXT("array"), TEXT("add"), TEXT("Array Add"), { TEXT("target"), TEXT("item") }, true, false },
		{ TEXT("container.array.add_unique"), TEXT("array"), TEXT("add_unique"), TEXT("Add Unique"), { TEXT("target"), TEXT("item") }, true, false },
		{ TEXT("container.array.append"), TEXT("array"), TEXT("append"), TEXT("Append Array"), { TEXT("target"), TEXT("items") }, true, false },
		{ TEXT("container.array.insert"), TEXT("array"), TEXT("insert"), TEXT("Array Insert"), { TEXT("target"), TEXT("index"), TEXT("item") }, true, false },
		{ TEXT("container.array.remove_item"), TEXT("array"), TEXT("remove_item"), TEXT("Remove Item"), { TEXT("target"), TEXT("item") }, true, false },
		{ TEXT("container.array.remove_index"), TEXT("array"), TEXT("remove_index"), TEXT("Remove Index"), { TEXT("target"), TEXT("index") }, true, false },
		{ TEXT("container.array.clear"), TEXT("array"), TEXT("clear"), TEXT("Clear Array"), { TEXT("target") }, true, false },
		{ TEXT("container.array.contains"), TEXT("array"), TEXT("contains"), TEXT("Array Contains"), { TEXT("target"), TEXT("item") }, false, true },
		{ TEXT("container.array.find"), TEXT("array"), TEXT("find"), TEXT("Array Find"), { TEXT("target"), TEXT("item") }, false, true },
		{ TEXT("container.array.length"), TEXT("array"), TEXT("length"), TEXT("Array Length"), { TEXT("target") }, false, true },
		{ TEXT("container.map.add"), TEXT("map"), TEXT("add"), TEXT("Map Add"), { TEXT("target"), TEXT("key"), TEXT("value") }, true, false },
		{ TEXT("container.map.remove"), TEXT("map"), TEXT("remove"), TEXT("Map Remove"), { TEXT("target"), TEXT("key") }, true, false },
		{ TEXT("container.map.find"), TEXT("map"), TEXT("find"), TEXT("Map Find"), { TEXT("target"), TEXT("key") }, false, true },
		{ TEXT("container.map.contains"), TEXT("map"), TEXT("contains"), TEXT("Map Contains"), { TEXT("target"), TEXT("key") }, false, true },
		{ TEXT("container.map.keys"), TEXT("map"), TEXT("keys"), TEXT("Map Keys"), { TEXT("target") }, false, true },
		{ TEXT("container.map.values"), TEXT("map"), TEXT("values"), TEXT("Map Values"), { TEXT("target") }, false, true },
		{ TEXT("container.map.clear"), TEXT("map"), TEXT("clear"), TEXT("Clear Map"), { TEXT("target") }, true, false },
		{ TEXT("container.map.length"), TEXT("map"), TEXT("length"), TEXT("Map Length"), { TEXT("target") }, false, true },
		{ TEXT("container.set.add"), TEXT("set"), TEXT("add"), TEXT("Set Add"), { TEXT("target"), TEXT("item") }, true, false },
		{ TEXT("container.set.remove"), TEXT("set"), TEXT("remove"), TEXT("Set Remove"), { TEXT("target"), TEXT("item") }, true, false },
		{ TEXT("container.set.contains"), TEXT("set"), TEXT("contains"), TEXT("Set Contains"), { TEXT("target"), TEXT("item") }, false, true },
		{ TEXT("container.set.clear"), TEXT("set"), TEXT("clear"), TEXT("Clear Set"), { TEXT("target") }, true, false },
		{ TEXT("container.set.length"), TEXT("set"), TEXT("length"), TEXT("Set Length"), { TEXT("target") }, false, true },
		{ TEXT("container.set.to_array"), TEXT("set"), TEXT("to_array"), TEXT("Set To Array"), { TEXT("target") }, false, true },
	};
	return Items;
}
}

const FBlueprintHelperContainerActionSpec* FBlueprintHelperContainerActionVocabulary::Find(
	const FString& ContainerKind,
	const FString& ContainerOperation)
{
	const FString Kind = Normalize(ContainerKind);
	const FString Operation = Normalize(ContainerOperation);
	for (const FBlueprintHelperContainerActionSpec& Spec : Specs())
	{
		if (Normalize(Spec.ContainerKind) == Kind && Normalize(Spec.ContainerOperation) == Operation)
		{
			return &Spec;
		}
	}
	return nullptr;
}

TArray<FBlueprintHelperContainerActionSpec> FBlueprintHelperContainerActionVocabulary::All()
{
	return Specs();
}
```

- [ ] **Step 4: Run vocabulary automation**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ContainerAction.Vocabulary;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: vocabulary test passes.

---

## Task 5: Resolve ContainerAction Through FunctionAction-Backed Evidence

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.cpp`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperContainerActionTests.cpp`

- [ ] **Step 1: Add red resolver test**

Append:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionResolverTest,
	"BlueprintHelper.GraphWrite.ContainerAction.Resolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionResolverTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.StatementId = TEXT("container-array-add");
	Request.ProjectedContextHash = TEXT("container-context");
	Request.SemanticConstraintsHash = TEXT("container-semantic");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::ContainerAction;
	Request.Semantic.ContainerKind = TEXT("array");
	Request.Semantic.ContainerOperation = TEXT("add");
	Request.Semantic.Query = TEXT("array.add");
	Request.Semantic.ContainerElementPinType.Category = TEXT("int");
	Request.Semantic.ArgumentTypes.Add(TEXT("element"), TEXT("int"));

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperContainerActionResolver::Resolve(Request);

	TestTrue(TEXT("resolved or context-specific not found"), Result.Status == EBlueprintHelperActionResolutionStatus::Resolved || Result.Status == EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestNotEqual(TEXT("not asset action"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	if (Result.Status == EBlueprintHelperActionResolutionStatus::Resolved)
	{
		TestTrue(TEXT("selected function or spawner exists"), Result.SelectedFunction.IsValid() || Result.SelectedSpawner.IsValid());
		TestTrue(TEXT("candidate describes container"), Result.MatchReason.Contains(TEXT("container.array.add")));
	}
	return true;
}
```

- [ ] **Step 2: Add resolver header**

Create:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class BLUEPRINTHELPER_API FBlueprintHelperContainerActionResolver
{
public:
	static FBlueprintHelperActionResolutionResult Resolve(const FBlueprintHelperActionResolutionRequest& Request);
};
```

- [ ] **Step 3: Implement resolver as FunctionAction adapter**

Create `BlueprintHelperContainerActionResolver.cpp`. The implementation must:

1. Look up `FBlueprintHelperContainerActionVocabulary::Find`.
2. Reject unsupported operation with `unsupported_container_operation`.
3. Validate required roles using semantic/default args evidence.
4. Build a function-style request with `Semantic.Kind=Call`, `FunctionOperation="container_action"`, `Query=Spec.FunctionQuery`, and the same pin-type constraints.
5. Call the existing FunctionAction resolution path.
6. Stamp `MatchReason` with `Spec.OperationId`.

Skeleton:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFunctionActionCluster.h"

namespace
{
static FBlueprintHelperActionResolutionResult MakeInvalid(const FString& Code, const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Result.ErrorCode = Code;
	Result.Message = Message;
	return Result;
}
}

FBlueprintHelperActionResolutionResult FBlueprintHelperContainerActionResolver::Resolve(
	const FBlueprintHelperActionResolutionRequest& Request)
{
	const FBlueprintHelperContainerActionSpec* Spec =
		FBlueprintHelperContainerActionVocabulary::Find(Request.Semantic.ContainerKind, Request.Semantic.ContainerOperation);
	if (!Spec)
	{
		return MakeInvalid(TEXT("unsupported_container_operation"), TEXT("Unsupported container_action operation."));
	}

	FBlueprintHelperActionResolutionRequest FunctionRequest = Request;
	FunctionRequest.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	FunctionRequest.Semantic.Kind = EBlueprintHelperActionSemanticKind::Call;
	FunctionRequest.Semantic.FunctionOperation = TEXT("container_action");
	FunctionRequest.Semantic.Query = Spec->FunctionQuery;
	FunctionRequest.ContextEvidence.Add(TEXT("container_action_operation_id"), Spec->OperationId);
	FunctionRequest.ContextEvidence.Add(TEXT("container_action_kind"), Spec->ContainerKind);
	FunctionRequest.ContextEvidence.Add(TEXT("container_action_operation"), Spec->ContainerOperation);

	FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperFunctionActionCluster::Resolve(FunctionRequest);
	if (Result.IsResolved())
	{
		Result.MatchReason = Spec->OperationId + TEXT(" via ") + Result.MatchReason;
	}
	return Result;
}
```

If `FBlueprintHelperFunctionActionCluster::Resolve` is not static in the current source, instantiate/use the existing cluster resolver entrypoint instead. Do not duplicate the function candidate scan.

- [ ] **Step 4: Route ContainerAction in FunctionActionCluster**

In `BlueprintHelperFunctionActionCluster.cpp`, add:

```cpp
if (Semantic.Kind == EBlueprintHelperActionSemanticKind::ContainerAction)
{
	return FBlueprintHelperContainerActionResolver::Resolve(Request);
}
```

Add the include for `BlueprintHelperContainerActionResolver.h`.

- [ ] **Step 5: Run resolver automation**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ContainerAction.Resolver;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: resolver test passes or reports a deterministic context error until a full Blueprint/Graph fixture is added in Task 6. It must not return `asset_action` and must not synthesize a node spawner.

---

## Task 6: Build ContainerAction Fragments And Role Links

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuildRequest.h`
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperContainerActionTests.cpp`

- [ ] **Step 1: Add builder red test**

Add a fixture-backed test that creates a test Blueprint with an `int` array member `Items`, builds a `container_action array.add`, and asserts the fragment DAG contains one action node and one target link.

Expected assertion names:

```cpp
TestTrue(TEXT("container action fragment has action node"), Fragment.Nodes.Num() >= 1);
TestTrue(TEXT("container action fragment records target link"), Fragment.Links.ContainsByPredicate(...));
TestTrue(TEXT("container action metadata includes operation id"), Fragment.Metadata.Contains(TEXT("container.array.add")));
```

Use existing GraphWrite test fixture helpers for Blueprint/graph creation; do not create a new asset factory helper if one already exists in `Private/Tests/GraphWrite`.

- [ ] **Step 2: Extend build request**

In `BlueprintHelperGraphFragmentBuildRequest.h`, add:

```cpp
FString ContainerKind;
FString ContainerOperation;
```

Map them from IR in the builder request construction path.

- [ ] **Step 3: Build through shared coordinator**

In `BlueprintHelperGraphStatementBuilder.cpp`, add a `BuildContainerActionFragment` branch that:

1. Creates an action resolution request with `Semantic.Kind=ContainerAction`.
2. Copies `ContainerKind`, `ContainerOperation`, target type, pin type, key/value type, and role args.
3. Calls the shared `FBlueprintHelperActionFragmentSpawnCoordinator`.
4. Adds metadata:

```cpp
Fragment.Metadata.Add(TEXT("container_action_operation_id"), FString::Printf(TEXT("container.%s.%s"), *ContainerKind, *ContainerOperation));
Fragment.Metadata.Add(TEXT("container_kind"), ContainerKind);
Fragment.Metadata.Add(TEXT("container_operation"), ContainerOperation);
```

- [ ] **Step 4: Route role expressions**

Role expression mapping:

| Public role | Internal arg pin role |
|---|---|
| `target` | target/self/container pin |
| `item` | item/value pin |
| `items` | source array/items pin |
| `key` | key pin |
| `value` | value pin |
| `index` | index pin |

Use existing argument expression handling in the GraphStatement builder. If a pin name differs across UE functions, let the resolver/vocabulary expose role aliases instead of hardcoding pin names in the builder.

- [ ] **Step 5: Run builder automation**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ContainerAction;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: SemanticIR, vocabulary, resolver, and builder tests pass.

---

## Task 7: Add Readback Verifier And Focused E2E Smoke

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperContainerActionReadbackVerifier.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperContainerActionReadbackVerifier.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperContainerActionTests.cpp`
- Add fixture JSON under: `BlueprintHelper/Develop/PlanArtifacts/GraphWriteContainerAction_20260525/`

- [ ] **Step 1: Add readback verifier contract**

Create header:

```cpp
#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;

struct FBlueprintHelperContainerActionReadbackExpectation
{
	FString OperationId;
	FString ContainerKind;
	FString ContainerOperation;
	FString TargetName;
	FString ElementType;
	FString KeyType;
	FString ValueType;
	TArray<FString> RequiredRoles;
	bool bRequiresExecFlow = false;
	bool bRequiresOutput = false;
};

class FBlueprintHelperContainerActionReadbackVerifier
{
public:
	static bool Verify(
		const UBlueprint* Blueprint,
		const UEdGraph* Graph,
		const FBlueprintHelperContainerActionReadbackExpectation& Expectation,
		FString& OutFailure);
};
```

- [ ] **Step 2: Implement readback checks**

Create implementation that scans graph nodes and verifies:

```cpp
// Required checks:
// 1. Find generated node whose metadata/title/candidate evidence matches OperationId.
// 2. Find target container pin linked to TargetName.
// 3. Verify wildcard pins are promoted: PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard.
// 4. Verify container pin has expected EPinContainerType for array/map/set.
// 5. Verify item/key/value/index pins exist when RequiredRoles contains them.
// 6. Verify output pin exists when bRequiresOutput is true.
// 7. Verify exec pins exist and are linked when bRequiresExecFlow is true.
```

Use existing graph/pin utilities in GraphWrite tests where available. The verifier should return `false` with an explicit `OutFailure` string instead of using `check`/assert.

- [ ] **Step 3: Add focused TaskSpec fixtures**

Add fixture JSON files:

```text
BlueprintHelper/Develop/PlanArtifacts/GraphWriteContainerAction_20260525/array_add.json
BlueprintHelper/Develop/PlanArtifacts/GraphWriteContainerAction_20260525/map_contains.json
BlueprintHelper/Develop/PlanArtifacts/GraphWriteContainerAction_20260525/set_to_array.json
```

Each fixture uses `task_type=edit_blueprint_graph`, `append_new_owned_graph`, and `kind=container_action`.

Example `array_add.json` body:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "task_type": "edit_blueprint_graph",
  "target": {
    "asset_path": "/Game/BH_Tests/BP_GraphWriteContainer",
    "asset_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "EventGraph",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [
      {
        "kind": "custom_event",
        "name": "GW_ArrayAdd",
        "body": [
          {
            "kind": "container_action",
            "container_kind": "array",
            "container_operation": "add",
            "target": { "kind": "get", "name": "Items" },
            "item": { "kind": "literal", "value": 7 },
            "element_type": "int"
          }
        ]
      }
    ]
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

- [ ] **Step 4: Add focused E2E automation**

In `BlueprintHelperContainerActionTests.cpp`, add an E2E test that:

1. Creates or reuses a test Blueprint.
2. Ensures member variables:
   - `Items`: array of `int`
   - `Scores`: map `string -> int`
   - `Tags`: set of `string`
3. Runs GraphWrite preview/execute for the three fixtures.
4. Runs `FBlueprintHelperContainerActionReadbackVerifier::Verify`.
5. Compiles the Blueprint and fails on compile errors.

- [ ] **Step 5: Run focused E2E**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ContainerAction;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: all focused container tests pass.

---

## Task 8: Update Capability Contract And Final Generality Matrix

**Files:**
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md`
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`

- [ ] **Step 1: Add contract red test**

In `graphwrite-capability-contract.test.ts`, add:

```ts
it('publishes first-class container_action operations with graph-level review evidence', () => {
  const containerAction = GRAPHWRITE_CAPABILITY_CONTRACT.clusters.find((cluster) => cluster.id === 'container_action');

  assert.ok(containerAction);
  assert.equal(containerAction.executeRevalidation, 'not-required');
  assert.equal(containerAction.evidence.projectionSource, 'ActionContext');
  assert.ok(containerAction.operations.some((operation) => operation.id === 'container.array.add'));
  assert.ok(containerAction.operations.some((operation) => operation.id === 'container.map.contains'));
  assert.ok(containerAction.operations.some((operation) => operation.id === 'container.set.to_array'));
  assert.ok(containerAction.operations.every((operation) => operation.reviewEvidence === 'graph_surface_atomic_target'));
});
```

- [ ] **Step 2: Extend contract type**

In `graphwrite-capability-contract.ts`, extend cluster id union:

```ts
readonly id: 'function_action' | 'field' | 'event' | 'asset_action' | 'container_action';
```

Add cluster:

```ts
{
  id: 'container_action',
  responsibility: 'GraphWrite owns first-class ordinary Blueprint array/map/set container semantics; execution resolves through ActionContext and FunctionAction-backed callable evidence.',
  operations: [
    'container.array.get',
    'container.array.set',
    'container.array.add',
    'container.array.add_unique',
    'container.array.append',
    'container.array.insert',
    'container.array.remove_item',
    'container.array.remove_index',
    'container.array.clear',
    'container.array.contains',
    'container.array.find',
    'container.array.length',
    'container.map.add',
    'container.map.remove',
    'container.map.find',
    'container.map.contains',
    'container.map.keys',
    'container.map.values',
    'container.map.clear',
    'container.map.length',
    'container.set.add',
    'container.set.remove',
    'container.set.contains',
    'container.set.clear',
    'container.set.length',
    'container.set.to_array',
  ].map((id) => ({
    id,
    kind: 'container_action',
    supportStatus: 'supported' as const,
    reviewEvidence: 'graph_surface_atomic_target' as const,
  })),
  evidence: {
    projectionSource: 'ActionContext',
    requiredKeys: [],
  },
  executeRevalidation: 'not-required',
}
```

- [ ] **Step 3: Sync docs**

Update the three docs to say:

```text
container_action V1 is supported for core array/map/set operations via first-class TaskSpec public shape and FunctionAction-backed runtime resolution. make_* remains Generic create; foreach remains control-flow owned.
```

- [ ] **Step 4: Run TS build/test**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected: build passes and node tests pass.

---

## Task 9: Full Verification

**Files:**
- All files touched by Tasks 1-8.

- [ ] **Step 1: Run source scans**

Run:

```powershell
rg -n "asset_action.*container|container_action.*UBlueprintNodeSpawner::Create|NodeClass == UK2Node|foreach" BlueprintHelper/Source/BlueprintHelper AgentFaceService/task-core/src
```

Expected:
- No `asset_action` ownership of `container_action`.
- No direct `UBlueprintNodeSpawner::Create` in `BlueprintHelperContainerActionResolver.cpp`.
- No raw `NodeClass == UK2Node_*` branch in container resolver.
- `foreach` appears only in explicit exclusion diagnostics/tests/docs.

- [ ] **Step 2: Run TypeScript verification**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected: all task-core tests pass.

- [ ] **Step 3: Run focused UE automation**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ContainerAction;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: focused container automation passes.

- [ ] **Step 4: Run GraphWrite suite**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: full GraphWrite automation passes.

- [ ] **Step 5: Build plugin**

Run:

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected: build succeeds.

- [ ] **Step 6: Check whitespace and manual commit scope**

Run:

```powershell
git diff --check
git status --short
```

Expected: no whitespace errors. Do not run `git add`, `git commit`, or `git push`; report the exact files that belong to this task so the user can commit manually.

## Execution Notes

- Do not implement `container_action` inside `asset_action`.
- Do not use query-only or node-class-only success as proof.
- Do not create hardcoded branches for each `UK2Node_*`; the V1 operation table is allowed because it is public vocabulary, but the resolver must still select real UE callable/action evidence.
- Do not count fixture setup failures as GraphWrite correctness failures; report them as setup failures and block the operation gate.
- Do not expand final generality preflight counts until this plan and the evidence-defect plans are complete.

## Suggested Manual Commit Message After Execution

```text
新增内容：
1. GraphWrite first-class container_action public shape and capability contract
2. ContainerAction vocabulary, resolver, readback verifier, and focused tests
```
