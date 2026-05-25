# GraphWrite Broad ControlFlow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 GraphWrite 支持普通 Blueprint `EventGraph` / `FunctionGraph` 中右键菜单可创建的 UE Flow Control 节点，并把 `branch / sequence / return` 从 first-slice 扩展为完整 ControlFlow taxonomy。

**Architecture:** 对外保持统一 `kind=control` public shape；对内按 `native K2Node`、`switch family`、`macro-based control` 三类 resolver/builder 分层。所有节点必须经 `TaskSpec -> compiler lowering -> SemanticIR -> ActionContextPipeline -> ActionResolutionCore -> UE NodeSpawner evidence -> shared spawn adapter -> FragmentDAG/Composer -> readback`，不得走 FunctionAction fallback、不得伪造 success、不得在 builder 内散落硬编码分支。

**Tech Stack:** TypeScript TaskSpec compiler and node tests, UE 5.6 C++ BlueprintGraph/GraphWrite runtime, Unreal Automation tests, ActionDatabase/NodeSpawner evidence, GraphWrite readback/debug evidence.

---

## Scope

### In Scope

| Family | Public operation | UE node family | GraphWrite ownership |
| --- | --- | --- | --- |
| Native singleton | `branch` | `UK2Node_IfThenElse` | create node, bind condition, compose `then` / `else` bodies |
| Native singleton | `sequence` | `UK2Node_ExecutionSequence` | create node, ensure output count, expose ordered outputs |
| Native singleton | `return` | `UK2Node_FunctionResult` | create function return node and bind return values in FunctionGraph |
| Switch family | `switch_int` | `UK2Node_SwitchInteger` | create switch, configure cases, compose case/default bodies |
| Switch family | `switch_string` | `UK2Node_SwitchString` | create switch, configure cases, compose case/default bodies |
| Switch family | `switch_name` | `UK2Node_SwitchName` | create switch, configure cases, compose case/default bodies |
| Switch family | `switch_enum` | `UK2Node_SwitchEnum` | create switch using enum evidence, compose enum/default bodies |
| Native configurable | `multi_gate` | `UK2Node_MultiGate` | configure output count and flags, compose each output body |
| Native configurable | `do_once_multi_input` | `UK2Node_DoOnceMultiInput` | configure input/output pair count, expose reset pins |
| Standard macro | `for_loop` | `UK2Node_MacroInstance` | resolve StandardMacros spawner, bind first/last index, compose loop/completed bodies |
| Standard macro | `for_loop_with_break` | `UK2Node_MacroInstance` | resolve StandardMacros spawner, expose break pin, compose loop/completed bodies |
| Standard macro | `foreach_loop` | `UK2Node_MacroInstance` | resolve StandardMacros spawner, bind array, expose item/index output symbols, compose bodies |
| Standard macro | `foreach_loop_with_break` | `UK2Node_MacroInstance` | resolve StandardMacros spawner, bind array, expose break pin, compose bodies |
| Standard macro | `reverse_foreach_loop` | `UK2Node_MacroInstance` | resolve StandardMacros spawner when UE exposes it in Flow Control, bind array, compose bodies |
| Standard macro | `while_loop` | `UK2Node_MacroInstance` | resolve StandardMacros spawner, bind condition, compose loop/completed bodies |
| Standard macro | `gate` | `UK2Node_MacroInstance` | resolve StandardMacros spawner, expose enter/open/close/toggle/exit pins |
| Standard macro | `do_once` | `UK2Node_MacroInstance` | resolve StandardMacros spawner, bind start closed/reset semantics, compose output body |
| Standard macro | `do_n` | `UK2Node_MacroInstance` | resolve StandardMacros spawner, bind `n`, expose counter/output pins |
| Standard macro | `flip_flop` | `UK2Node_MacroInstance` | resolve StandardMacros spawner, compose `a` and `b` outputs |

### Out of Scope

| Item | Owner / Reason |
| --- | --- |
| Animation Blueprint exclusive graph/event nodes | Not ordinary Blueprint `EventGraph` / `FunctionGraph`. |
| UMG animation-specific nodes | Not ordinary Blueprint `EventGraph` / `FunctionGraph` Flow Control ownership. |
| Event entry/signature declaration nodes | `BlueprintSignature` / EventDelegate ownership boundary. |
| Latent/timer utility function calls such as `Delay` | `FunctionAction` or Generic `schedule` ownership; Broad ControlFlow adds cross-cluster coverage only, not duplicate creation semantics. |
| Data-flow `select` | Existing Generic `select` path, not exec Flow Control. |
| Project/user macro libraries | Not UE built-in Flow Control. Discovery tests must identify them as out-of-contract unless explicitly added later. |

## Public Shape

All public statements remain `kind=control`.

```json
{
  "kind": "control",
  "control": "switch_int",
  "selection": { "kind": "get", "target": "StateIndex" },
  "cases": [
    { "value": 0, "statements": [{ "kind": "call", "target": "Print String", "args": { "InString": "Idle" } }] },
    { "value": 1, "statements": [{ "kind": "call", "target": "Print String", "args": { "InString": "Open" } }] }
  ],
  "default": [{ "kind": "call", "target": "Print String", "args": { "InString": "Unknown" } }]
}
```

```json
{
  "kind": "control",
  "control": "foreach_loop",
  "array": { "kind": "get", "target": "Targets" },
  "element_symbol": "Target",
  "index_symbol": "TargetIndex",
  "body": [{ "kind": "call", "target": "Print String", "args": { "InString": { "kind": "get", "target": "Target" } } }],
  "completed": [{ "kind": "call", "target": "Print String", "args": { "InString": "Done" } }]
}
```

```json
{
  "kind": "control",
  "control": "multi_gate",
  "output_count": 3,
  "loop": true,
  "is_random": false,
  "start_index": 0,
  "outputs": [
    { "name": "A", "statements": [{ "kind": "call", "target": "Print String", "args": { "InString": "A" } }] },
    { "name": "B", "statements": [{ "kind": "call", "target": "Print String", "args": { "InString": "B" } }] },
    { "name": "C", "statements": [{ "kind": "call", "target": "Print String", "args": { "InString": "C" } }] }
  ]
}
```

## Target File Map

| Area | Files |
| --- | --- |
| Agent-facing control contract | `AgentFaceService/task-core/src/task/schema/graphwrite-control-flow-operations.ts`, `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`, `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts` |
| TaskSpec validation/lowering | `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`, focused node tests under `AgentFaceService/task-core/src/task/compiler` or existing task-core test folder |
| Semantic IR model | `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`, `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`, `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp` |
| ActionContext demands | `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp` |
| Control registry/resolution | `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperControlFlowOperationRegistry.h`, `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperControlFlowOperationRegistry.cpp`, `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.cpp`, existing Generic resolver/core files only as needed to consume registry evidence |
| Fragment builder/composer | `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.h`, `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.cpp`, `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`, `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp` |
| Tests | `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteControlFlowTests.cpp`, existing GraphWrite runtime test file only when shared harness helpers are needed |
| Docs | `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`, `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`, `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md` |

## Implementation Tasks

### Task 1: Add Agent-Facing ControlFlow Operation Contract

**Files:**
- Create: `AgentFaceService/task-core/src/task/schema/graphwrite-control-flow-operations.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`

- [ ] **Step 1: Write the failing contract test**

Add assertions that the public contract exposes a dedicated `control_flow` cluster and that it includes the complete first-pass operation set.

```ts
import { GRAPHWRITE_CONTROL_FLOW_OPERATION_IDS } from './graphwrite-control-flow-operations';
import { GRAPHWRITE_CAPABILITY_CONTRACT } from './graphwrite-capability-contract';

test('GraphWrite contract exposes broad ControlFlow operations', () => {
  const cluster = GRAPHWRITE_CAPABILITY_CONTRACT.clusters.find((entry) => entry.id === 'control_flow');
  expect(cluster).toBeDefined();
  expect(cluster?.evidence.projectionSource).toBe('ActionContext');
  expect(cluster?.operations.map((entry) => entry.id)).toEqual(
    GRAPHWRITE_CONTROL_FLOW_OPERATION_IDS.map((id) => `control.${id}`),
  );
  expect(cluster?.operations.every((entry) => entry.reviewEvidence === 'graph_surface_atomic_target')).toBe(true);
});
```

Run:

```powershell
npm --prefix AgentFaceService/task-core run test:node
```

Expected: fail because `control_flow` and the operation constants do not exist.

- [ ] **Step 2: Add the data-driven operation registry**

Create `graphwrite-control-flow-operations.ts`:

```ts
export const GRAPHWRITE_CONTROL_FLOW_OPERATION_IDS = [
  'branch',
  'sequence',
  'return',
  'switch_int',
  'switch_string',
  'switch_name',
  'switch_enum',
  'multi_gate',
  'do_once_multi_input',
  'for_loop',
  'for_loop_with_break',
  'foreach_loop',
  'foreach_loop_with_break',
  'reverse_foreach_loop',
  'while_loop',
  'gate',
  'do_once',
  'do_n',
  'flip_flop',
] as const;

export type GraphWriteControlFlowOperationId = (typeof GRAPHWRITE_CONTROL_FLOW_OPERATION_IDS)[number];

export const GRAPHWRITE_CONTROL_FLOW_OPERATION_SET = new Set<string>(GRAPHWRITE_CONTROL_FLOW_OPERATION_IDS);

export type GraphWriteControlFlowFamily = 'native_singleton' | 'switch_family' | 'native_configurable' | 'standard_macro';

export interface GraphWriteControlFlowOperationShape {
  readonly id: GraphWriteControlFlowOperationId;
  readonly family: GraphWriteControlFlowFamily;
  readonly requiredExpressionFields: readonly string[];
  readonly execBodyFields: readonly string[];
}

export const GRAPHWRITE_CONTROL_FLOW_OPERATION_SHAPES: readonly GraphWriteControlFlowOperationShape[] = [
  { id: 'branch', family: 'native_singleton', requiredExpressionFields: ['condition'], execBodyFields: ['then', 'else'] },
  { id: 'sequence', family: 'native_singleton', requiredExpressionFields: [], execBodyFields: [] },
  { id: 'return', family: 'native_singleton', requiredExpressionFields: [], execBodyFields: [] },
  { id: 'switch_int', family: 'switch_family', requiredExpressionFields: ['selection'], execBodyFields: ['cases', 'default'] },
  { id: 'switch_string', family: 'switch_family', requiredExpressionFields: ['selection'], execBodyFields: ['cases', 'default'] },
  { id: 'switch_name', family: 'switch_family', requiredExpressionFields: ['selection'], execBodyFields: ['cases', 'default'] },
  { id: 'switch_enum', family: 'switch_family', requiredExpressionFields: ['selection'], execBodyFields: ['cases', 'default'] },
  { id: 'multi_gate', family: 'native_configurable', requiredExpressionFields: [], execBodyFields: ['outputs'] },
  { id: 'do_once_multi_input', family: 'native_configurable', requiredExpressionFields: [], execBodyFields: ['inputs'] },
  { id: 'for_loop', family: 'standard_macro', requiredExpressionFields: ['first_index', 'last_index'], execBodyFields: ['body', 'completed'] },
  { id: 'for_loop_with_break', family: 'standard_macro', requiredExpressionFields: ['first_index', 'last_index'], execBodyFields: ['body', 'completed'] },
  { id: 'foreach_loop', family: 'standard_macro', requiredExpressionFields: ['array'], execBodyFields: ['body', 'completed'] },
  { id: 'foreach_loop_with_break', family: 'standard_macro', requiredExpressionFields: ['array'], execBodyFields: ['body', 'completed'] },
  { id: 'reverse_foreach_loop', family: 'standard_macro', requiredExpressionFields: ['array'], execBodyFields: ['body', 'completed'] },
  { id: 'while_loop', family: 'standard_macro', requiredExpressionFields: ['condition'], execBodyFields: ['body', 'completed'] },
  { id: 'gate', family: 'standard_macro', requiredExpressionFields: [], execBodyFields: ['then'] },
  { id: 'do_once', family: 'standard_macro', requiredExpressionFields: [], execBodyFields: ['then'] },
  { id: 'do_n', family: 'standard_macro', requiredExpressionFields: ['n'], execBodyFields: ['then'] },
  { id: 'flip_flop', family: 'standard_macro', requiredExpressionFields: [], execBodyFields: ['a', 'b'] },
];
```

- [ ] **Step 3: Extend the capability contract**

Add `control_flow` to `GraphWriteClusterContract.id` and append a cluster entry:

```ts
{
  id: 'control_flow',
  responsibility:
    'GraphWrite owns ordinary Blueprint EventGraph/FunctionGraph Flow Control use-site nodes through one kind=control public taxonomy and family-specific resolver/builders.',
  operations: GRAPHWRITE_CONTROL_FLOW_OPERATION_IDS.map((id) => ({
    id: `control.${id}`,
    kind: 'control',
    supportStatus: 'supported' as const,
    reviewEvidence: 'graph_surface_atomic_target' as const,
  })),
  evidence: {
    projectionSource: 'ActionContext',
    requiredKeys: ['control_kind', 'control_family', 'selected_spawner', 'node_class', 'pin_alias_contract'],
  },
  executeRevalidation: 'required',
}
```

- [ ] **Step 4: Run node tests**

Run:

```powershell
npm --prefix AgentFaceService/task-core run build
npm --prefix AgentFaceService/task-core run test:node
```

Expected: pass.

### Task 2: Extend TaskSpec Validation and Lowering

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Test: existing task-core node tests

- [ ] **Step 1: Write failing validation tests**

Add cases for supported and unsupported broad controls:

```ts
test('GraphWrite accepts broad control flow statements', () => {
  const statements = [
    { kind: 'control', control: 'switch_int', selection: { kind: 'get', target: 'Mode' }, cases: [{ value: 0, statements: [] }], default: [] },
    { kind: 'control', control: 'for_loop', first_index: 0, last_index: 3, body: [], completed: [] },
    { kind: 'control', control: 'foreach_loop', array: { kind: 'get', target: 'Targets' }, body: [], completed: [] },
    { kind: 'control', control: 'gate', then: [] },
    { kind: 'control', control: 'flip_flop', a: [], b: [] },
  ];

  expect(() => compileGraphWriteBodyForTest(statements)).not.toThrow();
});

test('GraphWrite rejects unknown control flow operations', () => {
  expect(() => compileGraphWriteBodyForTest([{ kind: 'control', control: 'anim_notify_event' }])).toThrow(/unsupported_control_kind/);
});
```

Use the existing task compiler test helper names from the local test file; if the helper is private, add a focused test through the public compile entrypoint used by existing TaskSpec tests.

- [ ] **Step 2: Import the operation registry**

Replace the current hard-coded set:

```ts
const SUPPORTED_GRAPH_BODY_CONTROL_KINDS = new Set(['branch', 'sequence', 'return']);
```

with:

```ts
import {
  GRAPHWRITE_CONTROL_FLOW_OPERATION_SET,
  GRAPHWRITE_CONTROL_FLOW_OPERATION_SHAPES,
  type GraphWriteControlFlowOperationId,
} from '../schema/graphwrite-control-flow-operations';

const SUPPORTED_GRAPH_BODY_CONTROL_KINDS = GRAPHWRITE_CONTROL_FLOW_OPERATION_SET;
const CONTROL_FLOW_SHAPES_BY_ID = new Map(
  GRAPHWRITE_CONTROL_FLOW_OPERATION_SHAPES.map((shape) => [shape.id, shape]),
);
```

- [ ] **Step 3: Add exact shape validation**

Add helpers in `task-compiler.ts`:

```ts
function validateControlBodyArray(value: unknown, path: string): void {
  validateSupportedStatements(Array.isArray(value) ? (value as BlueprintLogicStatement[]) : [], path);
}

function validateControlCaseArray(value: unknown, path: string): void {
  if (!Array.isArray(value)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'control switch requires cases array.', [
      { code: 'taskspec_semantic_invalid', path, message: 'Provide cases as an array.' },
    ]);
  }
  value.forEach((entry, index) => {
    if (!isRecord(entry)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'control switch case must be an object.', [
        { code: 'taskspec_semantic_invalid', path: `${path}[${index}]`, message: 'Provide case objects with value and statements.' },
      ]);
    }
    if (!Object.hasOwn(entry, 'value')) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', 'control switch case requires value.', [
        { code: 'taskspec_semantic_invalid', path: `${path}[${index}].value`, message: 'Provide a switch case value.' },
      ]);
    }
    validateControlBodyArray(entry.statements, `${path}[${index}].statements`);
  });
}

function validateControlFlowShape(record: Record<string, unknown>, controlKind: GraphWriteControlFlowOperationId, path: string): void {
  const shape = CONTROL_FLOW_SHAPES_BY_ID.get(controlKind);
  if (!shape) {
    throw new TaskSpecCompileError('unsupported_control_kind', 'Unsupported GraphWrite control kind.', [
      { code: 'unsupported_control_kind', path: `${path}.control`, message: `Use ${GRAPHWRITE_CONTROL_FLOW_OPERATION_SHAPES.map((entry) => entry.id).join(', ')}.` },
    ]);
  }

  shape.requiredExpressionFields.forEach((field) => {
    if (!Object.hasOwn(record, field)) {
      throw new TaskSpecCompileError('taskspec_semantic_invalid', `control ${controlKind} requires ${field}.`, [
        { code: 'taskspec_semantic_invalid', path: `${path}.${field}`, message: `control ${controlKind} requires ${field}.` },
      ]);
    }
    validateSupportedExpression(record[field], `${path}.${field}`);
  });

  if (controlKind.startsWith('switch_')) {
    validateControlCaseArray(record.cases, `${path}.cases`);
    validateControlBodyArray(record.default, `${path}.default`);
  }
  ['then', 'else', 'body', 'completed', 'a', 'b'].forEach((field) => {
    if (Object.hasOwn(record, field)) {
      validateControlBodyArray(record[field], `${path}.${field}`);
    }
  });
  if (Array.isArray(record.outputs)) {
    record.outputs.forEach((entry, index) => {
      if (isRecord(entry)) {
        validateControlBodyArray(entry.statements, `${path}.outputs[${index}].statements`);
      }
    });
  }
}
```

Then replace the current branch-only validation block with:

```ts
const controlKind = getControlStatementKind(statementRecord, statementPath) as GraphWriteControlFlowOperationId;
validateControlFlowShape(statementRecord, controlKind, statementPath);
```

- [ ] **Step 4: Preserve `kind=control` during lowering**

Current lowering sets `out.kind = controlKind`. Replace it with:

```ts
out.kind = 'control';
out.control_kind = controlKind;
out.control = controlKind;
```

Move all control expression/body cloning into a generic helper:

```ts
function cloneControlFlowStatementWithCompiledIds(
  statementRecord: Record<string, unknown>,
  out: Record<string, unknown>,
  statementId: string,
): void {
  ['condition', 'selection', 'first_index', 'last_index', 'array', 'n', 'start_index'].forEach((field) => {
    if (Object.hasOwn(statementRecord, field)) {
      out[field] = cloneLogicExpressionWithCompiledIds(statementRecord[field], `${statementId}_${toIdSegment(field)}`);
    }
  });
  ['then', 'else', 'body', 'completed', 'a', 'b'].forEach((field) => {
    if (Array.isArray(statementRecord[field])) {
      out[field] = cloneLogicStatementSequenceWithCompiledIds(statementRecord[field] as BlueprintLogicStatement[], `${statementId}_${toIdSegment(field)}`);
    }
  });
  if (Array.isArray(statementRecord.cases)) {
    out.cases = statementRecord.cases.map((entry, index) => {
      const record = entry as Record<string, unknown>;
      return {
        ...record,
        statements: cloneLogicStatementSequenceWithCompiledIds(
          Array.isArray(record.statements) ? (record.statements as BlueprintLogicStatement[]) : [],
          `${statementId}_case_${index + 1}`,
        ),
      };
    });
  }
  if (Array.isArray(statementRecord.outputs)) {
    out.outputs = statementRecord.outputs.map((entry, index) => {
      const record = isRecord(entry) ? entry : {};
      return {
        ...record,
        statements: cloneLogicStatementSequenceWithCompiledIds(
          Array.isArray(record.statements) ? (record.statements as BlueprintLogicStatement[]) : [],
          `${statementId}_output_${index + 1}`,
        ),
      };
    });
  }
}
```

- [ ] **Step 5: Compile statement flow for generic control**

Replace `branch/sequence/return` special casing with:

```ts
if (kind === 'control') {
  return compileControlFlowStatement(statementRecord, nodeId, path, context);
}
```

Implement `compileControlFlowStatement` so the graph compiler emits one node plus data links and exec links based on the operation registry:

```ts
function compileControlFlowStatement(
  statementRecord: Record<string, unknown>,
  nodeId: string,
  path: string,
  context: CompileFlowContext,
): CompiledStatementFlow {
  const controlKind = getControlStatementKind(statementRecord, path) as GraphWriteControlFlowOperationId;
  const node = compileStatementNode({ ...statementRecord, kind: 'control', control_kind: controlKind } as BlueprintLogicStatement, nodeId, path);
  const nodes: AgentImportNode[] = [node];
  const links: AgentImportLink[] = [];

  ['condition', 'selection', 'first_index', 'last_index', 'array', 'n', 'start_index'].forEach((field) => {
    if (!Object.hasOwn(statementRecord, field)) return;
    const flow = compileValueExpression(statementRecord[field], `${nodeId}_${field}`, `${path}.${field}`, context);
    nodes.push(...flow.nodes);
    links.push(...flow.links);
    if (flow.output) links.push({ kind: 'data', from: flow.output, to: `${nodeId}.${field}` });
    else node.inputs = { ...(node.inputs ?? {}), [field]: flow.defaultValue };
  });

  return compileControlFlowExecBodies(controlKind, statementRecord, nodeId, path, context, nodes, links);
}
```

`compileControlFlowExecBodies` must:

- return exits from `then/else` for `branch`;
- return the node's `then_0` style exits for `sequence`;
- return no exits for `return`;
- wire `body/completed`, `a/b`, `outputs[*]`, `cases[*]`, and `default` through existing `compileStatementSequence`;
- keep gate open/close/toggle/reset pins addressable by alias but do not invent hidden handler/signature behavior.

- [ ] **Step 6: Run TypeScript verification**

Run:

```powershell
npm --prefix AgentFaceService/task-core run build
npm --prefix AgentFaceService/task-core run test:node
```

Expected: pass.

### Task 3: Migrate SemanticIR to First-Class Generic Control

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`

- [ ] **Step 1: Write failing SemanticIR tests**

Add automation tests in `BlueprintHelperGraphWriteControlFlowTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteControlFlowSemanticIrParsesGenericControlTest,
	"BlueprintHelper.GraphWrite.ControlFlow.SemanticIR.ParsesGenericControl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteControlFlowSemanticIrParsesGenericControlTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
	Statement->SetStringField(TEXT("id"), TEXT("SwitchMode"));
	Statement->SetStringField(TEXT("kind"), TEXT("control"));
	Statement->SetStringField(TEXT("control_kind"), TEXT("switch_int"));
	Statement->SetStringField(TEXT("control"), TEXT("switch_int"));
	Statement->SetField(TEXT("selection"), MakeShared<FJsonValueObject>(MakeGetExpression(TEXT("Mode"))));
	Statement->SetArrayField(TEXT("cases"), MakeControlCases({TEXT("0"), TEXT("1")}));

	FBlueprintHelperGraphSemanticIR IR;
	TestTrue(TEXT("logic spec builds"), BuildLogicSpecWithStatements({Statement}, IR));
	TestEqual(TEXT("statement kind is generic control"), IR.Statements[0]->Kind, EBlueprintHelperGraphStatementKind::Control);
	TestEqual(TEXT("control kind is preserved"), IR.Statements[0]->ControlKind, TEXT("switch_int"));
	TestTrue(TEXT("case branch exists"), IR.Statements[0]->ExecBranches.Contains(TEXT("case:0")));
	return true;
}
```

Use local helper builders in the same test file so test JSON is explicit and independent.

- [ ] **Step 2: Add generic Control fields**

In `FBlueprintHelperGraphStatementIR`, add:

```cpp
FString ControlKind;
FString SwitchType;
FString EnumPath;
FString ElementSymbolName;
FString IndexSymbolName;
int32 OutputCount = 0;
TArray<FString> CaseValues;
TMap<FString, TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>> ExecBranches;
```

Keep `ThenStatements` and `ElseStatements` only as compatibility shims inside the same implementation pass; after pipeline and DAG use `ExecBranches`, branch should populate both `ExecBranches["then"]` / `ExecBranches["else"]` and the existing arrays until all call sites are migrated.

- [ ] **Step 3: Add enum and parsing support**

Add `Control` to `EBlueprintHelperGraphStatementKind`. Update `ParseStatementKind`:

```cpp
if (Kind.Equals(TEXT("control"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Control;
```

Update `StatementPatternName` and fragment-name helpers to return `control` for `Control`.

- [ ] **Step 4: Parse control-specific fields**

In `ParseStatement`, read:

```cpp
StatementObject->TryGetStringField(TEXT("control_kind"), Statement->ControlKind);
if (Statement->ControlKind.IsEmpty())
{
	StatementObject->TryGetStringField(TEXT("control"), Statement->ControlKind);
}
Statement->ControlKind = NormalizeFieldToken(Statement->ControlKind);
StatementObject->TryGetStringField(TEXT("switch_type"), Statement->SwitchType);
Statement->SwitchType = NormalizeFieldToken(Statement->SwitchType);
StatementObject->TryGetStringField(TEXT("enum_path"), Statement->EnumPath);
StatementObject->TryGetStringField(TEXT("element_symbol"), Statement->ElementSymbolName);
StatementObject->TryGetStringField(TEXT("index_symbol"), Statement->IndexSymbolName);
StatementObject->TryGetNumberField(TEXT("output_count"), Statement->OutputCount);
```

Parse body arrays into `ExecBranches`:

```cpp
ParseControlBranchArray(StatementObject, TEXT("then"), Path, *Statement, OutIR);
ParseControlBranchArray(StatementObject, TEXT("else"), Path, *Statement, OutIR);
ParseControlBranchArray(StatementObject, TEXT("body"), Path, *Statement, OutIR);
ParseControlBranchArray(StatementObject, TEXT("completed"), Path, *Statement, OutIR);
ParseControlBranchArray(StatementObject, TEXT("a"), Path, *Statement, OutIR);
ParseControlBranchArray(StatementObject, TEXT("b"), Path, *Statement, OutIR);
ParseSwitchCases(StatementObject, Path, *Statement, OutIR);
ParseMultiOutputBranches(StatementObject, Path, *Statement, OutIR);
```

`ParseSwitchCases` must create keys `case:<value>` and `default`. `ParseMultiOutputBranches` must create keys `out:<index>` and preserve friendly names through `CaseValues` or metadata.

- [ ] **Step 5: Validate generic control in SemanticIR**

In `ResolveStatement`, replace branch-only validation with operation-specific validation:

```cpp
case EBlueprintHelperGraphStatementKind::Control:
	ValidateControlFlowStatement(*Statement, OutIR);
	break;
```

`ValidateControlFlowStatement` must require:

- `branch`: bool `Condition`;
- `switch_*`: `selection` expression and at least one case;
- `switch_enum`: `enum_path`;
- `for_loop` / `for_loop_with_break`: `first_index` and `last_index`;
- `foreach_*` / `reverse_foreach_loop`: `array`;
- `while_loop`: bool `Condition`;
- `do_n`: `n`;
- `multi_gate`: `output_count >= 1` or `outputs.Num() >= 1`;
- `return`: no exec exits and valid only when target graph is a FunctionGraph during runtime build.

- [ ] **Step 6: Update demand collection**

`CollectFromStatementArray` must recurse through `ExecBranches` in addition to legacy arrays during migration:

```cpp
for (const TPair<FString, TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>>& Branch : Statement->ExecBranches)
{
	CollectFromStatementArray(Branch.Value, OutDemands);
}
```

`ToActionSemanticKind` must map `Control` to `EBlueprintHelperActionSemanticKind::Control`. `BuildStatementQuery` must return `Statement.ControlKind` for generic control.

- [ ] **Step 7: Run targeted C++ tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ControlFlow.SemanticIR;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: `BlueprintHelper.GraphWrite.ControlFlow.SemanticIR.*` passes.

### Task 4: Add ControlFlow Operation Registry and Resolver Evidence

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperControlFlowOperationRegistry.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperControlFlowOperationRegistry.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericActionProviderBoundary.cpp`
- Modify: Generic resolver/core files only where selected evidence is produced

- [ ] **Step 1: Write failing registry tests**

Add tests:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteControlFlowRegistryCoversExpectedOperationsTest,
	"BlueprintHelper.GraphWrite.ControlFlow.Registry.CoversExpectedOperations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteControlFlowRegistryCoversExpectedOperationsTest::RunTest(const FString& Parameters)
{
	const TCHAR* Expected[] = {
		TEXT("branch"), TEXT("sequence"), TEXT("return"),
		TEXT("switch_int"), TEXT("switch_string"), TEXT("switch_name"), TEXT("switch_enum"),
		TEXT("multi_gate"), TEXT("do_once_multi_input"),
		TEXT("for_loop"), TEXT("for_loop_with_break"), TEXT("foreach_loop"), TEXT("foreach_loop_with_break"),
		TEXT("reverse_foreach_loop"), TEXT("while_loop"), TEXT("gate"), TEXT("do_once"), TEXT("do_n"), TEXT("flip_flop")
	};

	for (const TCHAR* Operation : Expected)
	{
		FBlueprintHelperControlFlowOperationDescriptor Descriptor;
		TestTrue(FString::Printf(TEXT("%s is registered"), Operation),
			FBlueprintHelperControlFlowOperationRegistry::TryGet(Operation, Descriptor));
	}
	return true;
}
```

- [ ] **Step 2: Add descriptor types**

Create header:

```cpp
#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperControlFlowFamily : uint8
{
	NativeSingleton,
	SwitchFamily,
	NativeConfigurable,
	StandardMacro
};

struct BLUEPRINTHELPER_API FBlueprintHelperControlFlowOperationDescriptor
{
	FString OperationId;
	EBlueprintHelperControlFlowFamily Family = EBlueprintHelperControlFlowFamily::NativeSingleton;
	TArray<FString> QueryAliases;
	TArray<FString> ExpectedNodeClassNames;
	TArray<FString> DataInputAliases;
	TArray<FString> ExecInputAliases;
	TArray<FString> ExecOutputAliases;
	bool bRequiresFunctionGraph = false;
	bool bRequiresEnumPath = false;
	bool bRequiresSwitchCases = false;
	bool bRequiresOutputCount = false;
};

class BLUEPRINTHELPER_API FBlueprintHelperControlFlowOperationRegistry
{
public:
	static bool TryGet(const FString& OperationId, FBlueprintHelperControlFlowOperationDescriptor& OutDescriptor);
	static TArray<FBlueprintHelperControlFlowOperationDescriptor> GetAll();
	static FString NormalizeOperationId(const FString& Value);
};
```

- [ ] **Step 3: Add exact operation descriptors**

Create descriptors in the cpp file:

```cpp
static TArray<FBlueprintHelperControlFlowOperationDescriptor> BuildControlFlowDescriptors()
{
	return {
		{TEXT("branch"), EBlueprintHelperControlFlowFamily::NativeSingleton, {TEXT("branch")}, {TEXT("K2Node_IfThenElse")}, {TEXT("condition")}, {TEXT("execute")}, {TEXT("then"), TEXT("else")}},
		{TEXT("sequence"), EBlueprintHelperControlFlowFamily::NativeSingleton, {TEXT("sequence")}, {TEXT("K2Node_ExecutionSequence")}, {}, {TEXT("execute")}, {TEXT("then")}},
		{TEXT("return"), EBlueprintHelperControlFlowFamily::NativeSingleton, {TEXT("return")}, {TEXT("K2Node_FunctionResult")}, {TEXT("value")}, {TEXT("execute")}, {}, true},
		{TEXT("switch_int"), EBlueprintHelperControlFlowFamily::SwitchFamily, {TEXT("switch int"), TEXT("switch on int")}, {TEXT("K2Node_SwitchInteger")}, {TEXT("selection")}, {TEXT("execute")}, {TEXT("default")}},
		{TEXT("switch_string"), EBlueprintHelperControlFlowFamily::SwitchFamily, {TEXT("switch string"), TEXT("switch on string")}, {TEXT("K2Node_SwitchString")}, {TEXT("selection")}, {TEXT("execute")}, {TEXT("default")}},
		{TEXT("switch_name"), EBlueprintHelperControlFlowFamily::SwitchFamily, {TEXT("switch name"), TEXT("switch on name")}, {TEXT("K2Node_SwitchName")}, {TEXT("selection")}, {TEXT("execute")}, {TEXT("default")}},
		{TEXT("switch_enum"), EBlueprintHelperControlFlowFamily::SwitchFamily, {TEXT("switch enum"), TEXT("switch on enum")}, {TEXT("K2Node_SwitchEnum")}, {TEXT("selection")}, {TEXT("execute")}, {TEXT("default")}, false, true, true},
		{TEXT("multi_gate"), EBlueprintHelperControlFlowFamily::NativeConfigurable, {TEXT("multi gate"), TEXT("multigate")}, {TEXT("K2Node_MultiGate")}, {TEXT("is_random"), TEXT("loop"), TEXT("start_index")}, {TEXT("execute"), TEXT("reset")}, {TEXT("then")}, false, false, false, true},
		{TEXT("do_once_multi_input"), EBlueprintHelperControlFlowFamily::NativeConfigurable, {TEXT("do once multiinput"), TEXT("do once multi input")}, {TEXT("K2Node_DoOnceMultiInput")}, {}, {TEXT("execute"), TEXT("reset")}, {TEXT("then")}, false, false, false, true},
		{TEXT("for_loop"), EBlueprintHelperControlFlowFamily::StandardMacro, {TEXT("for loop")}, {TEXT("K2Node_MacroInstance")}, {TEXT("first_index"), TEXT("last_index")}, {TEXT("execute")}, {TEXT("loop_body"), TEXT("completed")}},
		{TEXT("for_loop_with_break"), EBlueprintHelperControlFlowFamily::StandardMacro, {TEXT("for loop with break")}, {TEXT("K2Node_MacroInstance")}, {TEXT("first_index"), TEXT("last_index")}, {TEXT("execute"), TEXT("break")}, {TEXT("loop_body"), TEXT("completed")}},
		{TEXT("foreach_loop"), EBlueprintHelperControlFlowFamily::StandardMacro, {TEXT("for each loop"), TEXT("foreach loop")}, {TEXT("K2Node_MacroInstance")}, {TEXT("array")}, {TEXT("execute")}, {TEXT("loop_body"), TEXT("completed")}},
		{TEXT("foreach_loop_with_break"), EBlueprintHelperControlFlowFamily::StandardMacro, {TEXT("for each loop with break"), TEXT("foreach loop with break")}, {TEXT("K2Node_MacroInstance")}, {TEXT("array")}, {TEXT("execute"), TEXT("break")}, {TEXT("loop_body"), TEXT("completed")}},
		{TEXT("reverse_foreach_loop"), EBlueprintHelperControlFlowFamily::StandardMacro, {TEXT("reverse for each loop"), TEXT("reverse foreach loop")}, {TEXT("K2Node_MacroInstance")}, {TEXT("array")}, {TEXT("execute")}, {TEXT("loop_body"), TEXT("completed")}},
		{TEXT("while_loop"), EBlueprintHelperControlFlowFamily::StandardMacro, {TEXT("while loop")}, {TEXT("K2Node_MacroInstance")}, {TEXT("condition")}, {TEXT("execute")}, {TEXT("loop_body"), TEXT("completed")}},
		{TEXT("gate"), EBlueprintHelperControlFlowFamily::StandardMacro, {TEXT("gate")}, {TEXT("K2Node_MacroInstance")}, {TEXT("start_closed")}, {TEXT("enter"), TEXT("open"), TEXT("close"), TEXT("toggle")}, {TEXT("exit")}},
		{TEXT("do_once"), EBlueprintHelperControlFlowFamily::StandardMacro, {TEXT("do once")}, {TEXT("K2Node_MacroInstance")}, {TEXT("start_closed")}, {TEXT("execute"), TEXT("reset")}, {TEXT("then")}},
		{TEXT("do_n"), EBlueprintHelperControlFlowFamily::StandardMacro, {TEXT("do n")}, {TEXT("K2Node_MacroInstance")}, {TEXT("n")}, {TEXT("execute"), TEXT("reset")}, {TEXT("then"), TEXT("counter")}},
		{TEXT("flip_flop"), EBlueprintHelperControlFlowFamily::StandardMacro, {TEXT("flip flop"), TEXT("flipflop")}, {TEXT("K2Node_MacroInstance")}, {}, {TEXT("execute")}, {TEXT("a"), TEXT("b"), TEXT("is_a")}},
	};
}
```

- [ ] **Step 4: Use registry in provider boundary**

Change the Control branch in `BlueprintHelperGenericActionProviderBoundary.cpp` so the reason text and support decision are registry-backed:

```cpp
case EBlueprintHelperActionSemanticKind::Control:
{
	FBlueprintHelperControlFlowOperationDescriptor Descriptor;
	const FString OperationId = FBlueprintHelperControlFlowOperationRegistry::NormalizeOperationId(Request.Semantic.Query);
	Boundary.Mode = OperationId.IsEmpty()
		? EBlueprintHelperGenericActionProviderMode::NeedsMoreSemanticContext
		: (FBlueprintHelperControlFlowOperationRegistry::TryGet(OperationId, Descriptor)
			? EBlueprintHelperGenericActionProviderMode::NodeSpawnerCandidate
			: EBlueprintHelperGenericActionProviderMode::Unsupported);
	Boundary.RequiredBuilder = TEXT("ControlFragmentBuilder");
	Boundary.Reason = OperationId.IsEmpty()
		? TEXT("control requires Semantic.Query/control_kind.")
		: TEXT("control resolves through ControlFlowOperationRegistry and family-specific UE spawner evidence.");
	return Boundary;
}
```

- [ ] **Step 5: Resolve by selected evidence, not fake success**

Update the Generic resolver path for `Semantic.Kind=Control` so:

- operation id comes from `Request.Semantic.Query`;
- registry descriptor determines acceptable node classes and query aliases;
- ActionDatabase candidates must be current and unique after graph/context filtering;
- selected result includes `control_kind`, `control_family`, `selected_spawner`, `node_class`, `pin_alias_contract`;
- no candidate returns `needs_more_semantic_context`;
- multiple candidates returns ambiguity diagnostics;
- no manual `NewObject<UK2Node_MacroInstance>` path is introduced.

- [ ] **Step 6: Add discovery guard**

Add an automation test that queries UE Flow Control action menu candidates for ordinary Actor Blueprint `EventGraph` and `FunctionGraph`. It should assert:

- every registry descriptor resolves to at least one candidate in the expected graph type, except `return` which is FunctionGraph-only;
- any UE built-in Flow Control candidate with node classes `K2Node_IfThenElse`, `K2Node_ExecutionSequence`, `K2Node_FunctionResult`, `K2Node_Switch*`, `K2Node_MultiGate`, `K2Node_DoOnceMultiInput`, or StandardMacros `K2Node_MacroInstance` is either in the registry or in an explicit out-of-scope list;
- FunctionAction-owned latent utility functions are reported as `function_action_owned`, not as missing ControlFlow.

- [ ] **Step 7: Run resolver tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ControlFlow.Registry;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: registry and resolver evidence tests pass.

### Task 5: Implement Family-Specific Control Fragment Building

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperControlFragmentBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentBuilderRegistry.cpp`

- [ ] **Step 1: Write failing builder tests**

Add tests that build fragments from prepared `FBlueprintHelperGraphStatementIR` for:

- `switch_int` creates `UK2Node_SwitchInteger` and exposes `selection`, `case:0`, `case:1`, `default`;
- `multi_gate` creates `UK2Node_MultiGate` with requested output count;
- `foreach_loop` creates `UK2Node_MacroInstance`, binds `array`, and exposes `loop_body`, `array_element`, `array_index`, `completed`;
- `gate` exposes `enter/open/close/toggle/exit`;
- unsupported graph type for `return` fails deterministically.

- [ ] **Step 2: Replace enum-specific public methods with generic dispatch**

Keep `BuildBranch`, `BuildSequence`, and `BuildReturn` as small wrappers if existing call sites still use them, but make `BuildStatement` call:

```cpp
return BuildControl(TargetGraph, ActionContextScope, Statement, OutFragment, OutError);
```

Add:

```cpp
static bool BuildControl(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionContextScope* ActionContextScope,
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError);
```

- [ ] **Step 3: Resolve the descriptor once**

Inside `BuildControl`:

```cpp
FBlueprintHelperControlFlowOperationDescriptor Descriptor;
const FString ControlKind = FBlueprintHelperControlFlowOperationRegistry::NormalizeOperationId(Statement.ControlKind);
if (!FBlueprintHelperControlFlowOperationRegistry::TryGet(ControlKind, Descriptor))
{
	OutError = FString::Printf(TEXT("unsupported_control_kind: %s."), *Statement.ControlKind);
	return false;
}
```

Then call `ResolveControlActionProvider` with `ControlKind`, and verify the spawned node class against `Descriptor.ExpectedNodeClassNames`.

- [ ] **Step 4: Add family-specific configuration**

Implement:

```cpp
static bool ConfigureSwitchControlNode(UK2Node* Node, const FBlueprintHelperGraphStatementIR& Statement, FBlueprintHelperNodeFragment& OutFragment, FString& OutError);
static bool ConfigureNativeConfigurableControlNode(UK2Node* Node, const FBlueprintHelperGraphStatementIR& Statement, FBlueprintHelperNodeFragment& OutFragment, FString& OutError);
static bool ConfigureMacroControlNode(UK2Node* Node, const FBlueprintHelperGraphStatementIR& Statement, FBlueprintHelperNodeFragment& OutFragment, FString& OutError);
static void PopulateControlPinAliases(UK2Node* Node, const FBlueprintHelperControlFlowOperationDescriptor& Descriptor, FBlueprintHelperNodeFragment& OutFragment);
```

Rules:

- Switch nodes add case pins from `Statement.CaseValues`, then bind aliases `case:<value>` and `default`.
- `multi_gate` and `do_once_multi_input` call UE add-pin APIs until the requested output/input count exists.
- Macro nodes rely on spawned macro pins; aliases normalize friendly names such as `Loop Body`, `Completed`, `Array Element`, `Array Index`, `A`, `B`, `Is A`, `Reset`.
- Literal defaults use existing `FBlueprintHelperActionNodeSpawnOptions.DefaultValues` instead of post-spawn hard patches when possible.

- [ ] **Step 5: Populate review/debug metadata**

Every control fragment must include:

```cpp
OutFragment.OwnershipTags.Add(TEXT("semantic_kind"), TEXT("control"));
OutFragment.OwnershipTags.Add(TEXT("control_kind"), ControlKind);
OutFragment.OwnershipTags.Add(TEXT("control_family"), ControlFamilyToString(Descriptor.Family));
OutFragment.OwnershipTags.Add(TEXT("node_class"), Node->GetClass()->GetName());
OutFragment.ReviewTargets.Add(Statement.StatementId.IsEmpty() ? FragmentId : Statement.StatementId);
```

Review evidence remains graph-level `graph_block`; operation-level detail goes to debug metadata/readback facts, not separate Review target kinds.

- [ ] **Step 6: Update fragment builder registry**

Route only `EBlueprintHelperGraphStatementKind::Control` through `FBlueprintHelperControlFragmentBuilder`. Remove new broad-control checks from other clusters.

- [ ] **Step 7: Run builder tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ControlFlow.Builder;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: builder tests pass.

### Task 6: Generalize Exec Body Composition

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp`

- [ ] **Step 1: Write failing composition tests**

Create tests that execute a GraphWrite body containing:

- `branch` with `then` and `else`;
- `switch_int` with two cases and default;
- `for_loop` with `body` and `completed`;
- `flip_flop` with `a` and `b`;
- `multi_gate` with three outputs.

Each test should assert the expected exec links exist by pin alias and that the final graph compiles.

- [ ] **Step 2: Replace branch-only composition with branch-map composition**

In `BuildSemanticStatement`, replace:

```cpp
if (Statement->Kind == EBlueprintHelperGraphStatementKind::Branch)
```

with a generic `BuildControlExecBranches` path:

```cpp
if (Statement->Kind == EBlueprintHelperGraphStatementKind::Control)
{
	return BuildControlExecBranches(
		TargetGraph,
		ActionContextScope,
		FragmentDag,
		*Statement,
		StatementFragment,
		GeneratedFragments,
		GeneratedFragmentIds,
		OutUnresolvedNodes,
		ConnectionDiagnostics,
		GeneratedNodeCount,
		CreatedConnectionCount);
}
```

`BuildControlExecBranches` must iterate `Statement.ExecBranches`, find matching output pin alias on `StatementFragment.PrimaryNode`, build each branch statement array, and append branch exits. Empty body branches preserve the source output pin as an exit.

- [ ] **Step 3: Add join behavior only where semantics need it**

Branch/switch/loop completed flows must not create fake hidden join nodes in the runtime builder. The pipeline should return all branch exits and let the next sequential statement connect to them, matching the current branch behavior. `return` returns no exits.

- [ ] **Step 4: Update FragmentDAG builder**

Replace branch-specific `then` / `else` DAG logic with a helper:

```cpp
static FBlueprintHelperDagExecFlow BuildControlBranchDag(
	const FBlueprintHelperGraphStatementIR& Statement,
	FBlueprintHelperGraphFragmentDagBuilderState& State,
	TArray<TMap<FString, FBlueprintHelperGraphSymbol>>& SymbolScopes);
```

The helper creates endpoint refs from branch-map keys:

- `then`, `else`;
- `case:<value>`, `default`;
- `body`, `completed`;
- `a`, `b`;
- `out:<index>`.

- [ ] **Step 5: Preserve loop output symbols**

For `foreach_loop`, `foreach_loop_with_break`, and `reverse_foreach_loop`, register `element_symbol` and `index_symbol` in the body branch scope using output aliases `array_element` and `array_index`.

- [ ] **Step 6: Run composition tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ControlFlow.Composition;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: composition tests pass.

### Task 7: Add Readback, DebugBundle, and Final Evidence Checks

**Files:**
- Modify readback helpers in `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteToolResultBaseTests.cpp` or move shared helpers into a focused utility if the existing file is too large.
- Modify GraphWrite readback/evidence source files only where existing readback helpers live.
- Test: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphWriteControlFlowTests.cpp`

- [ ] **Step 1: Write failing readback tests**

For each operation family, test readback by evidence:

| Operation | Required readback |
| --- | --- |
| `switch_int` | node class, `selection` data pin link/default, all case output pins, default output pin |
| `foreach_loop` | macro node, array pin linked, wildcard array pin resolved to expected type, body/completed links |
| `gate` | enter/open/close/toggle input aliases and exit output alias |
| `multi_gate` | output count, loop/is_random/start_index defaults, each output alias |
| `return` | FunctionGraph-only return node, value pin default/link |

- [ ] **Step 2: Add readback evidence shape**

Use graph-level Review evidence only, but add debug facts:

```json
{
  "semantic_kind": "control",
  "control_kind": "foreach_loop",
  "control_family": "standard_macro",
  "node_class": "K2Node_MacroInstance",
  "selected_spawner": "StandardMacros.ForEachLoop",
  "pin_alias_contract": ["execute", "array", "loop_body", "array_element", "array_index", "completed"]
}
```

- [ ] **Step 3: Compile after readback**

Every positive control-flow runtime test must compile the Blueprint after execution. Compile errors are a failing readback fallback, not a warning.

- [ ] **Step 4: Run evidence tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ControlFlow.Readback;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: readback tests pass.

### Task 8: Update Capability Docs and Generality Matrix

**Files:**
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md`

- [ ] **Step 1: Update the design doc**

Record:

- broad control is ordinary Blueprint `EventGraph` / `FunctionGraph` only;
- public shape is `kind=control` + `control/control_kind`;
- internal implementation is native K2Node / switch family / macro-based control;
- latent FunctionAction/Schedule FlowControl-category calls remain owned by their existing clusters;
- Review evidence stays `graph_block`, operation detail appears in debug/readback facts.

- [ ] **Step 2: Update capability contract doc**

Add a `control_flow` cluster table with all operation ids from Task 1 and status `supported` only after tests pass.

- [ ] **Step 3: Update final generality preflight matrix**

Replace current three-row control section:

```md
| `control.branch` | `kind=control, control=branch` | internal `kind=branch` | `K2Node_IfThenElse` |
| `control.sequence` | `kind=control, control=sequence` | internal `kind=sequence` | `K2Node_ExecutionSequence` |
| `control.return` | `kind=control, control=return` | internal `kind=return` | return node |
```

with rows for every operation in `GRAPHWRITE_CONTROL_FLOW_OPERATION_IDS`, using internal `kind=control, control_kind=<operation>`.

- [ ] **Step 4: Record exclusions**

Add an exclusions table for Animation Blueprint, UMG animation, Signature/event declarations, data-flow `select`, and latent FunctionAction/Schedule calls.

### Task 9: Full Verification

**Files:** no source edits unless a previous task fails.

- [ ] **Step 1: Run TypeScript gates**

```powershell
npm --prefix AgentFaceService/task-core run build
npm --prefix AgentFaceService/task-core run test:node
```

Expected: pass.

- [ ] **Step 2: Run focused Unreal automation**

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ControlFlow;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: all `BlueprintHelper.GraphWrite.ControlFlow.*` tests pass.

- [ ] **Step 3: Run GraphWrite suite**

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -Unattended -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite;Quit" -TestExit="Automation Test Queue Empty"
```

Expected: all GraphWrite automation tests pass.

- [ ] **Step 4: Build plugin/project**

```powershell
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
```

Expected: build succeeds.

- [ ] **Step 5: Run architecture scans**

```powershell
rg -n "unsupported_control_kind.*branch, sequence, or return|manual_control_context|manual_control_semantic|RequireDedicatedControlBuilderBoundary" BlueprintHelper/Source/BlueprintHelper AgentFaceService/task-core/src
rg -n "NewObject<UK2Node_MacroInstance>|fake success|control_action_atomic_target" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite AgentFaceService/task-core/src
git diff --check
```

Expected:

- first command has no active hits;
- second command has no active hits except test strings that intentionally assert absence;
- `git diff --check` exits `0`.

## Acceptance Criteria

- Agent-facing `kind=control` accepts the complete operation set in Task 1.
- Compiler lowering keeps generic `kind=control` and emits deterministic `control_kind`.
- C++ SemanticIR stores broad control as one first-class statement family, not enum-per-node scatter.
- Action resolution selects UE NodeSpawner evidence for native, switch, and macro control families.
- Macro control nodes are created through UE spawner evidence; no manual `UK2Node_MacroInstance` fallback.
- Body composition works for branch, switch, loop, multi-output, and stateful macro controls.
- Readback proves node class, selected spawner evidence, pin aliases, data links/defaults, exec links, wildcard resolution where relevant, and compile success.
- Review evidence remains graph-level `graph_block`; operation details are debug/readback facts.
- Existing FunctionAction/Schedule ownership is preserved for latent/timer FlowControl-category function calls.

## Manual Commit Guidance

Do not run `git add`, `git commit`, or `git push` from the implementation worker unless the user explicitly asks. At the end, report the touched files and suggest a manual commit message.
