# GraphWrite Graph-local Value Producer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a general GraphWrite graph-local value producer path so a statement with `result_symbol` can expose its data output to later expressions in the same GraphBody without timer-specific branches.

**Architecture:** Keep the mainline as `TaskSpec GraphBody -> SemanticIR -> ActionContextPipeline -> ActionResolutionCore -> FragmentDAG -> Composer/Linker/MutationCoordinator -> UE Mutator`. Extend SemanticIR symbol registration and FragmentDAG endpoint selection as reusable GraphWrite services/utilities; do not route through legacy import-node fallback or add a `K2_SetTimer` special case. Treat Temporary symbol reads as graph-local values, not FieldVariable actions.

**Tech Stack:** UE 5.6 C++, BlueprintHelper GraphWrite SemanticIR, ActionContextPipeline, FragmentDAG, AgentFaceService TypeScript task compiler, Unreal Automation Tests, BlueprintHelper CLI preview/execute.

---

## Scope

This plan implements a general result-value contract for GraphWrite statements that produce a data output.

Included:
- `result_symbol` on value-producing `call`, `create`, `convert`, `schedule`, and existing query `container_action` statements.
- SemanticIR graph-local Temporary symbol registration for those statements.
- ActionContext demand suppression for Temporary field/get expressions.
- FragmentDAG endpoint selection for statement result symbols.
- TS compiler/schema behavior that preserves valid `result_symbol` and rejects invalid impure expression usage before UE sees invalid graphs.
- Focused tests and one editor/CLI smoke using a looping timer handle only as an example consumer of the general mechanism.

Excluded:
- Timeline curve editing.
- A timer-only resolver branch.
- Legacy import-node fallback.
- New public top-level GraphWrite tool shape beyond `result_symbol`.
- Automatic git staging, commits, or pushes. AGENTS.md requires manual git commands only.

## Current Failure Model

The failing timer-handle chain is a symptom of a wider gap:

```json
{
  "kind": "schedule",
  "schedule_operation": "timer_delegate_node",
  "target": "K2_SetTimerDelegate",
  "value_type": "TimerHandle",
  "result_symbol": "LoopTimerHandle",
  "args": {
    "Time": { "kind": "literal", "value_type": "float", "value": 0.75 },
    "bLooping": { "kind": "literal", "value_type": "bool", "value": true }
  }
}
```

Then:

```json
{
  "kind": "field",
  "field_operation": "set",
  "field_scope": "variable",
  "target": "LoopDoorTimerHandle",
  "value": { "kind": "get", "name": "LoopTimerHandle" }
}
```

Current behavior:
- `LoopTimerHandle` is not registered as a SemanticIR Temporary symbol for ordinary `schedule` or `call` statements.
- ActionContext sees the later `get` as a FieldVariable read and asks FieldVariable resolver for owner evidence.
- FragmentDAG has local symbol plumbing, but `BuildSimpleStatement` currently assumes container-action result pin/type semantics.
- `schedule` expression currently exposes `then` in DAG, which is an exec output, not a timer handle data output.

Expected behavior:
- `LoopTimerHandle` resolves as a graph-local Temporary.
- No FieldVariable demand is generated for that Temporary get.
- DAG connects the producing statement data output to the set statement input.
- The timer node executes because it is a statement in the exec chain, not an impure expression with a dangling exec pin.

## File Structure

Modify:
- `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/schema/task-schemas.ts`
  - Keep schema permissive for BlueprintLogic statements, but add or preserve validation helpers for result-symbol constraints where this file owns public shape validation.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - Preserve `result_symbol` on supported statement kinds.
  - Register statement outputs in the legacy flow compiler for parity.
  - Reject unsafe impure value expressions in GraphBody validation.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.graph-local-value-producer.test.ts`
  - New TypeScript tests for statement `result_symbol`, Temporary get lowering, and unsafe expression rejection.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
  - Add small helper declarations only if implementation needs shared signatures. Keep data structs focused.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
  - Register result symbols for value-producing statements.
  - Resolve later field/get expressions against symbol scopes as Temporary values.
  - Reject `result_symbol` on statements without a data result.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - Skip demand creation for Temporary field/get expressions.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp`
  - Replace container-only result-symbol endpoint logic with a general endpoint resolver.
  - Correct schedule expression output from exec `then` to a data output.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphLocalValueProducerTests.cpp`
  - New focused C++ automation tests.
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GraphLocalValueProducer_ImplementationPlan_20260527_CN.md`
  - Update implementation status and verification evidence as tasks complete.

Create only if useful for keeping responsibilities small:
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementResultEndpointUtils.h`
- `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementResultEndpointUtils.cpp`

Use the utility split if `BlueprintHelperGraphFragmentDagBuilderUtils.cpp` would otherwise gain more than one new helper group. The utility owns only statement-kind to output-endpoint/type mapping.

## Task 1: TypeScript Contract Tests

**Files:**
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.graph-local-value-producer.test.ts`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.ts`

- [ ] **Step 1: Add failing tests for result_symbol preservation and unsafe expression rejection**

Create `task-compiler.graph-local-value-producer.test.ts`:

```ts
import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan, taskPlanToAppendBridgePayload } from './task-compiler.js';

function makeGraphSpec(statements: Record<string, unknown>[]) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    task_type: 'edit_blueprint_graph',
    target: {
      asset_path: '/Game/BH_Tests/BP_GraphWriteGraphLocalValueProducer',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EventGraph',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'GW_GraphLocalValueProducer',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements,
        },
      }],
    },
    execution_policy: {
      dry_run_mode: 'full',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  };
}

test('call statement preserves result_symbol for UE graph-local value production', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeGraphSpec([{
    kind: 'call',
    target: '/Script/Engine.KismetSystemLibrary:K2_SetTimer',
    value_type: 'TimerHandle',
    result_symbol: 'LoopTimerHandle',
    args: {
      FunctionName: { kind: 'literal', value_type: 'string', value: 'HandlePulse' },
      Time: { kind: 'literal', value_type: 'float', value: 0.75 },
      bLooping: { kind: 'literal', value_type: 'bool', value: true },
    },
  }, {
    kind: 'field',
    field_operation: 'set',
    field_scope: 'variable',
    target: 'LoopDoorTimerHandle',
    value: { kind: 'get', name: 'LoopTimerHandle' },
  }]) as never);

  const payload = taskPlanToAppendBridgePayload(taskPlan, true);
  const statements = payload.logic_spec.statements as Record<string, unknown>[];

  assert.equal(statements[0].kind, 'call');
  assert.equal(statements[0].result_symbol, 'LoopTimerHandle');
  assert.deepEqual(statements[1].value, {
    kind: 'get',
    name: 'LoopTimerHandle',
    id: 'GW_GraphLocalValueProducer_stmt_2_value',
  });
});

test('schedule statement preserves result_symbol and projected schedule evidence', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeGraphSpec([{
    kind: 'schedule',
    schedule_operation: 'timer_delegate_node',
    target: 'K2_SetTimerDelegate',
    value_type: 'TimerHandle',
    result_symbol: 'LoopTimerHandle',
    context_evidence: {
      schedule_action_stable_id: 'action_database:/Script/Engine.KismetSystemLibrary:/Script/BlueprintGraph.K2Node_CallFunction:(FieldName="/Script/Engine.KismetSystemLibrary:K2_SetTimerDelegate",NodeName="/Script/BlueprintGraph.K2Node_CallFunction")',
      schedule_node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
      schedule_spawner_signature: '(FieldName="/Script/Engine.KismetSystemLibrary:K2_SetTimerDelegate",NodeName="/Script/BlueprintGraph.K2Node_CallFunction")',
      schedule_owner_path: '/Script/Engine.KismetSystemLibrary',
      schedule_query: 'K2_SetTimerDelegate',
    },
    args: {
      Time: { kind: 'literal', value_type: 'float', value: 0.75 },
      bLooping: { kind: 'literal', value_type: 'bool', value: true },
    },
  }]) as never);

  const payload = taskPlanToAppendBridgePayload(taskPlan, true);
  const statement = payload.logic_spec.statements[0] as Record<string, unknown>;

  assert.equal(statement.kind, 'schedule');
  assert.equal(statement.result_symbol, 'LoopTimerHandle');
  assert.equal((statement.context_evidence as Record<string, unknown>).schedule_query, 'K2_SetTimerDelegate');
});

test('impure call expression is rejected when used as a value expression', () => {
  assert.throws(
    () => compileTaskSpecToTaskPlan(makeGraphSpec([{
      kind: 'field',
      field_operation: 'set',
      field_scope: 'variable',
      target: 'LoopDoorTimerHandle',
      value: {
        kind: 'call',
        target: '/Script/Engine.KismetSystemLibrary:K2_SetTimer',
        args: {
          FunctionName: { kind: 'literal', value_type: 'string', value: 'HandlePulse' },
          Time: { kind: 'literal', value_type: 'float', value: 0.75 },
          bLooping: { kind: 'literal', value_type: 'bool', value: true },
        },
      },
    }]) as never),
    /impure call expressions require a statement result_symbol/,
  );
});
```

- [ ] **Step 2: Run the TypeScript tests and confirm the new test fails**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected before implementation:
- Build succeeds or fails only on the new code being absent.
- `impure call expression is rejected when used as a value expression` fails because current compiler allows the expression path.

## Task 2: TypeScript Compiler Contract

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/schema/task-schemas.ts` only if schema-level helpers need the same wording.
- Test: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.graph-local-value-producer.test.ts`

- [ ] **Step 1: Add a supported statement result-symbol helper**

In `task-compiler.ts`, near `SUPPORTED_GRAPH_BODY_STATEMENT_KINDS`, add:

```ts
const VALUE_PRODUCING_STATEMENT_KINDS = new Set([
  'call',
  'create',
  'convert',
  'schedule',
  CONTAINER_ACTION_KIND,
]);

function statementKindSupportsResultSymbol(kind: string): boolean {
  return VALUE_PRODUCING_STATEMENT_KINDS.has(kind);
}

function validateStatementResultSymbol(record: Record<string, unknown>, path: string): void {
  if (!Object.hasOwn(record, 'result_symbol')) return;

  getRequiredString(record, 'result_symbol', `${path}.result_symbol`);
  const kind = typeof record.kind === 'string' ? record.kind : '';
  if (!statementKindSupportsResultSymbol(kind)) {
    throw new TaskSpecCompileError('taskspec_semantic_invalid', 'result_symbol requires a value-producing statement.', [
      {
        code: 'taskspec_semantic_invalid',
        path: `${path}.result_symbol`,
        message: 'Use result_symbol only on call, create, convert, schedule, or query container_action statements.',
      },
    ]);
  }
}
```

- [ ] **Step 2: Call the helper during GraphBody statement validation**

In `validateSupportedStatement`, after `kind` is known and before branch-specific validation, add:

```ts
validateStatementResultSymbol(record, path);
```

Do not remove the existing container-action mutating-operation check; it remains narrower and still rejects mutating container operations with `result_symbol`.

- [ ] **Step 3: Register statement output symbols in the legacy flow compiler for parity**

In `compileStatementFlow`, after the input args for `call/create/convert/schedule` are compiled and before returning the entry/exits flow, add:

```ts
const resultSymbol = optionalString(statementRecord, 'result_symbol');
if (resultSymbol && statementKindSupportsResultSymbol(kind)) {
  const outputPin = kind === 'create' || kind === 'convert' || kind === 'schedule'
    ? 'value'
    : 'ReturnValue';
  context.symbols.set(resultSymbol.toLowerCase(), { output: `${nodeId}.${outputPin}` });
}
```

Keep the existing container-action branch as-is until C++ behavior is green. After C++ tests pass, normalize the container-action branch to call a shared helper if the code remains clear.

- [ ] **Step 4: Reject impure call/schedule value expressions**

In `validateSupportedExpression`, before recursive argument validation for `call` and `schedule`, add:

```ts
if (kind === 'call' || kind === 'schedule') {
  throw new TaskSpecCompileError('impure_expression_requires_statement', `${kind} expressions require a statement result_symbol.`, [
    {
      code: 'impure_expression_requires_statement',
      path: `${path}.kind`,
      message: `Use a ${kind} statement with result_symbol, then read the symbol with kind=get.`,
    },
  ]);
}
```

This is intentionally conservative. `op`, `construct`, `deconstruct`, `select`, `create`, `convert`, and query `container_action` can remain expression-capable because they are already value-like in GraphWrite.

- [ ] **Step 5: Run focused TS validation**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected:
- New graph-local value producer tests pass.
- Existing container-action tests still pass, including mutating `result_symbol` rejection.

## Task 3: SemanticIR Temporary Symbol Registration

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Modify only if needed: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
- Test: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphLocalValueProducerTests.cpp`

- [ ] **Step 1: Add failing SemanticIR tests**

Create `BlueprintHelperGraphLocalValueProducerTests.cpp` with:

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

namespace
{
static bool BuildLogicSpecForGraphLocalValueProducer(
	FAutomationTestBase& Test,
	const FString& Json,
	FBlueprintHelperGraphSemanticIR& OutIR)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!Test.TestTrue(TEXT("json parses"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
	{
		return false;
	}

	const bool bBuilt = FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(Root, OutIR);
	for (const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic : OutIR.Diagnostics)
	{
		if (Diagnostic.Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
		{
			Test.AddError(FString::Printf(TEXT("%s at %s: %s"), *Diagnostic.Code, *Diagnostic.Path, *Diagnostic.Message));
		}
	}
	return Test.TestTrue(TEXT("semantic ir builds"), bBuilt);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLocalValueProducerSemanticIRTest,
	"BlueprintHelper.GraphWrite.GraphLocalValueProducer.SemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLocalValueProducerSemanticIRTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"statements": [{
			"id": "stmt_make_handle",
			"kind": "call",
			"target": "/Script/Engine.KismetSystemLibrary:K2_SetTimer",
			"value_type": "TimerHandle",
			"result_symbol": "LoopTimerHandle",
			"args": {
				"FunctionName": { "kind": "literal", "value_type": "string", "value": "HandlePulse" },
				"Time": { "kind": "literal", "value_type": "float", "value": 0.75 },
				"bLooping": { "kind": "literal", "value_type": "bool", "value": true }
			}
		}, {
			"id": "stmt_cache_handle",
			"kind": "field",
			"field_operation": "set",
			"field_scope": "variable",
			"target": "LoopDoorTimerHandle",
			"value": { "kind": "get", "name": "LoopTimerHandle" }
		}]
	})JSON");

	FBlueprintHelperGraphSemanticIR IR;
	if (!BuildLogicSpecForGraphLocalValueProducer(*this, Json, IR))
	{
		return false;
	}

	TestEqual(TEXT("two statements"), IR.Statements.Num(), 2);
	TestTrue(TEXT("consumer value exists"), IR.Statements[1]->Value.IsValid());
	TestEqual(TEXT("consumer value is temporary"),
		IR.Statements[1]->Value->ResolvedTarget.Kind,
		EBlueprintHelperGraphTargetKind::Temporary);
	TestEqual(TEXT("temporary type is carried"),
		IR.Statements[1]->Value->ResolvedTarget.Type,
		FString(TEXT("TimerHandle")));
	return true;
}

#endif
```

Expected before implementation:
- The test fails because `LoopTimerHandle` is not registered before the later get is resolved.

- [ ] **Step 2: Add a result-symbol capability helper in SemanticIR.cpp**

Near the existing container-action validation helpers, add:

```cpp
static bool StatementKindCanReturnGraphLocalValue(const EBlueprintHelperGraphStatementKind Kind)
{
	switch (Kind)
	{
	case EBlueprintHelperGraphStatementKind::Call:
	case EBlueprintHelperGraphStatementKind::Create:
	case EBlueprintHelperGraphStatementKind::Convert:
	case EBlueprintHelperGraphStatementKind::Schedule:
	case EBlueprintHelperGraphStatementKind::ContainerAction:
		return true;
	default:
		return false;
	}
}

static FString ResolveStatementResultTypeToken(const FBlueprintHelperGraphStatementIR& Statement)
{
	if (!Statement.ValueType.TrimStartAndEnd().IsEmpty())
	{
		return Statement.ValueType.TrimStartAndEnd();
	}
	if (!Statement.ElementType.TrimStartAndEnd().IsEmpty())
	{
		return Statement.ElementType.TrimStartAndEnd();
	}
	if (!Statement.PinType.TrimStartAndEnd().IsEmpty())
	{
		return Statement.PinType.TrimStartAndEnd();
	}
	return Statement.ResolvedTarget.Type.TrimStartAndEnd();
}
```

The smoke/test specs carry `value_type: "TimerHandle"` as explicit semantic type evidence. Do not infer timer handle types from function names.

- [ ] **Step 3: Register ordinary statement result symbols after resolving each statement**

In `ResolveStatement`, after resolving statement arguments/value/condition/target object and before resolving child branch scopes, add:

```cpp
if (!Statement->ResultSymbolName.TrimStartAndEnd().IsEmpty())
{
	if (!StatementKindCanReturnGraphLocalValue(Statement->Kind))
	{
		FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
			OutIR,
			TEXT("result_symbol_statement_not_value_producing"),
			Statement->Path + TEXT(".result_symbol"),
			TEXT("result_symbol requires call, create, convert, schedule, or query container_action statement."));
	}
	else if (Statement->Kind != EBlueprintHelperGraphStatementKind::Let)
	{
		const FString ResultType = ResolveStatementResultTypeToken(*Statement);
		TSharedPtr<FBlueprintHelperGraphExpressionIR> ResultExpression = MakeShared<FBlueprintHelperGraphExpressionIR>();
		ResultExpression->Kind = EBlueprintHelperGraphExpressionKind::Field;
		ResultExpression->Target = Statement->ResultSymbolName;
		ResultExpression->Name = Statement->ResultSymbolName;
		ResultExpression->Type = ResultType;
		ResultExpression->ResolvedTarget.Kind = EBlueprintHelperGraphTargetKind::Temporary;
		ResultExpression->ResolvedTarget.Raw = Statement->ResultSymbolName;
		ResultExpression->ResolvedTarget.Member = Statement->ResultSymbolName;
		ResultExpression->ResolvedTarget.Type = ResultType;
		ResultExpression->ResolvedTarget.bVerifiedByContext = true;
		FBlueprintHelperGraphSemanticIRUtils::RegisterSymbol(
			OutIR,
			Statement->ResultSymbolName,
			Statement->StatementId,
			ResultExpression,
			Statement->Path + TEXT(".result_symbol"),
			ScopeStack);
	}
}
```

Keep the existing `let` symbol registration path intact. If the implementation can add an overload that registers a `FBlueprintHelperGraphSymbol` directly, prefer that cleaner helper over constructing a synthetic expression.

- [ ] **Step 4: Preserve container-action mutating rejection**

Do not remove:

```cpp
ValidateContainerActionContract(
	OutIR,
	Statement->Path,
	Statement->ContainerKind,
	Statement->ContainerOperation,
	Statement->Args,
	Statement->TargetObject,
	Statement->Target,
	Statement->ResultSymbolName,
	false);
```

This keeps mutating container operations from binding `result_symbol` unless their vocabulary says they return a pure query value.

- [ ] **Step 5: Run focused SemanticIR test**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GraphLocalValueProducer.SemanticIR;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GraphLocalValueProducer_SemanticIR_001'
```

Expected:
- The new SemanticIR test passes.
- Existing container-action result-symbol tests still pass in the later full focused run.

## Task 4: ActionContext Temporary Demand Suppression

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Test: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphLocalValueProducerTests.cpp`

- [ ] **Step 1: Add a failing demand-collector test**

Append to `BlueprintHelperGraphLocalValueProducerTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLocalValueProducerActionContextTest,
	"BlueprintHelper.GraphWrite.GraphLocalValueProducer.ActionContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLocalValueProducerActionContextTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"statements": [{
			"id": "stmt_make_handle",
			"kind": "call",
			"target": "/Script/Engine.KismetSystemLibrary:K2_SetTimer",
			"value_type": "TimerHandle",
			"result_symbol": "LoopTimerHandle",
			"args": {
				"FunctionName": { "kind": "literal", "value_type": "string", "value": "HandlePulse" },
				"Time": { "kind": "literal", "value_type": "float", "value": 0.75 },
				"bLooping": { "kind": "literal", "value_type": "bool", "value": true }
			}
		}, {
			"id": "stmt_cache_handle",
			"kind": "field",
			"field_operation": "set",
			"field_scope": "variable",
			"target": "LoopDoorTimerHandle",
			"value": { "kind": "get", "name": "LoopTimerHandle" }
		}]
	})JSON");

	FBlueprintHelperGraphSemanticIR IR;
	if (!BuildLogicSpecForGraphLocalValueProducer(*this, Json, IR))
	{
		return false;
	}

	TArray<FBlueprintHelperActionContextDemand> Demands;
	FBlueprintHelperActionContextDemandCollector::Collect(IR, Demands);

	const bool bHasTemporaryFieldDemand = Demands.ContainsByPredicate(
		[](const FBlueprintHelperActionContextDemand& Demand)
		{
			return Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Field
				&& Demand.TargetPath.Equals(TEXT("LoopTimerHandle"), ESearchCase::IgnoreCase);
		});

	TestFalse(TEXT("temporary get does not create field variable demand"), bHasTemporaryFieldDemand);
	return true;
}
```

Expected before implementation:
- The test fails if the Temporary expression still creates a Field demand.

- [ ] **Step 2: Skip Temporary field/get expressions in demand collection**

In `AppendDemandForExpression`, before `BuildDemand`, add:

```cpp
if (Expression.Kind == EBlueprintHelperGraphExpressionKind::Field
	&& Expression.ResolvedTarget.Kind == EBlueprintHelperGraphTargetKind::Temporary)
{
	if (Expression.TargetObject.IsValid())
	{
		AppendDemandForExpression(*Expression.TargetObject, OwnerStatementId, OutDemands);
	}
	if (Expression.Value.IsValid())
	{
		AppendDemandForExpression(*Expression.Value, OwnerStatementId, OutDemands);
	}
	return;
}
```

Temporary reads are satisfied by the GraphDAG symbol table. They are not ActionDatabase spawns and should not enter FieldVariable resolution.

- [ ] **Step 3: Run the ActionContext test**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GraphLocalValueProducer.ActionContext;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GraphLocalValueProducer_ActionContext_001'
```

Expected:
- No demand targets `LoopTimerHandle` as a FieldVariable.
- Statement demands for the producing `call` and consuming `field set` still exist.

## Task 5: FragmentDAG General Result Endpoint Resolver

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp`
- Create optional: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementResultEndpointUtils.h`
- Create optional: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementResultEndpointUtils.cpp`
- Test: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphLocalValueProducerTests.cpp`

- [ ] **Step 1: Add failing DAG tests for call and schedule result symbols**

Append to `BlueprintHelperGraphLocalValueProducerTests.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphLocalValueProducerFragmentDagTest,
	"BlueprintHelper.GraphWrite.GraphLocalValueProducer.FragmentDag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphLocalValueProducerFragmentDagTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT(R"JSON({
		"schema": "BlueprintLogicSpec.v2",
		"statements": [{
			"id": "stmt_make_handle",
			"kind": "call",
			"target": "/Script/Engine.KismetSystemLibrary:K2_SetTimer",
			"value_type": "TimerHandle",
			"result_symbol": "LoopTimerHandle",
			"args": {
				"FunctionName": { "kind": "literal", "value_type": "string", "value": "HandlePulse" },
				"Time": { "kind": "literal", "value_type": "float", "value": 0.75 },
				"bLooping": { "kind": "literal", "value_type": "bool", "value": true }
			}
		}, {
			"id": "stmt_cache_handle",
			"kind": "field",
			"field_operation": "set",
			"field_scope": "variable",
			"target": "LoopDoorTimerHandle",
			"value": { "kind": "get", "name": "LoopTimerHandle" }
		}]
	})JSON");

	FBlueprintHelperGraphSemanticIR IR;
	if (!BuildLogicSpecForGraphLocalValueProducer(*this, Json, IR))
	{
		return false;
	}

	FBlueprintHelperGraphFragmentDag Dag;
	TestTrue(TEXT("dag builds"), FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(IR, Dag));
	TestFalse(TEXT("dag has no errors"), Dag.HasErrors());

	const bool bHasReturnValueEdge = Dag.DataEdges.ContainsByPredicate(
		[](const FBlueprintHelperGraphFragmentDataEdge& Edge)
		{
			return Edge.From.FragmentId == TEXT("stmt_make_handle")
				&& Edge.From.PinName == TEXT("ReturnValue")
				&& Edge.To.FragmentId == TEXT("stmt_cache_handle")
				&& Edge.To.PinName == TEXT("value");
		});

	TestTrue(TEXT("call result_symbol feeds later field set"), bHasReturnValueEdge);
	return true;
}
```

- [ ] **Step 2: Add a result endpoint resolver**

In `BlueprintHelperGraphFragmentDagBuilderUtils.cpp`, near the existing endpoint helpers, add:

```cpp
static FString ResolveStatementResultEndpointName(const FBlueprintHelperGraphStatementIR& Statement)
{
	switch (Statement.Kind)
	{
	case EBlueprintHelperGraphStatementKind::Call:
		return TEXT("ReturnValue");
	case EBlueprintHelperGraphStatementKind::Create:
	case EBlueprintHelperGraphStatementKind::Convert:
	case EBlueprintHelperGraphStatementKind::Schedule:
		return TEXT("value");
	case EBlueprintHelperGraphStatementKind::ContainerAction:
		return TEXT("result");
	default:
		return FString();
	}
}

static FString ResolveStatementResultEndpointType(const FBlueprintHelperGraphStatementIR& Statement)
{
	const FString ContainerResultType = FBlueprintHelperGraphStatementTypeUtils::ResolveContainerActionResultTypeToken(
		Statement.ContainerKind,
		Statement.ContainerOperation,
		Statement.ElementType,
		Statement.KeyType,
		Statement.ValueType,
		Statement.PinType,
		Statement.KeyPinType,
		Statement.ValuePinType);
	if (!ContainerResultType.IsEmpty())
	{
		return ContainerResultType;
	}
	if (!Statement.ValueType.IsEmpty())
	{
		return Statement.ValueType;
	}
	if (!Statement.ElementType.IsEmpty())
	{
		return Statement.ElementType;
	}
	if (!Statement.PinType.IsEmpty())
	{
		return Statement.PinType;
	}
	if (!Statement.ResolvedTarget.Type.IsEmpty())
	{
		return Statement.ResolvedTarget.Type;
	}
	return FString();
}
```

If this helper becomes more than endpoint naming and type-token fallback, move it to the optional `BlueprintHelperGraphStatementResultEndpointUtils` files.

- [ ] **Step 3: Replace container-only result registration in BuildSimpleStatement**

Replace the current `if (!Statement->ResultSymbolName...)` block with:

```cpp
if (!Statement->ResultSymbolName.TrimStartAndEnd().IsEmpty())
{
	const FString ResultEndpointName = ResolveStatementResultEndpointName(*Statement);
	if (ResultEndpointName.IsEmpty())
	{
		AddBuilderDiagnostic(
			State,
			TEXT("result_symbol_statement_not_value_producing"),
			Statement->Path + TEXT(".result_symbol"),
			TEXT("result_symbol requires a statement with a data output."),
			EBlueprintHelperGraphFragmentDiagnosticSeverity::Error);
	}
	else
	{
		FBlueprintHelperGraphFragmentDagBuilderUtils::FBlueprintHelperDagDataProducer ResultProducer;
		ResultProducer.Endpoint = MakeDataOutput(
			FragmentId,
			ResultEndpointName,
			ResolveStatementResultEndpointType(*Statement));
		ApplyContainerActionResultEndpointType(
			Statement->ContainerKind,
			Statement->ContainerOperation,
			Statement->ElementType,
			Statement->KeyType,
			Statement->ValueType,
			Statement->PinType,
			Statement->KeyPinType,
			Statement->ValuePinType,
			ResultProducer.Endpoint);
		ResultProducer.SymbolId = NormalizeSymbolKey(Statement->ResultSymbolName);
		ResultProducer.Type = ResultProducer.Endpoint.Type;
		ResultProducer.Path = Statement->Path;
		RegisterSymbolProducer(Statement->ResultSymbolName, ResultProducer, Statement->Path, State, SymbolScopes);
	}
}
```

- [ ] **Step 4: Correct schedule expression data output**

In `BuildExpression`, change the schedule expression endpoint from:

```cpp
TEXT("then")
```

to:

```cpp
TEXT("value")
```

This makes expression endpoint naming consistent with TS and with the statement result endpoint. If Task 2 rejects all `schedule` expressions, this change still protects internal callers and future pure-schedule values from an exec/data mismatch.

- [ ] **Step 5: Run DAG tests**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GraphLocalValueProducer.FragmentDag;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GraphLocalValueProducer_FragmentDag_001'
```

Expected:
- `stmt_make_handle.ReturnValue -> stmt_cache_handle.value` exists for call.
- Existing container-action `result` endpoint tests still pass in the focused cluster run.

## Task 6: End-to-End Timer Handle Smoke

**Files:**
- Created temporary TaskSpecs under `D:/UEProjects/Template/Saved/BlueprintHelper/CodexSmoke/GraphLocalValueProducer_20260527_003/`
- Updated evidence in `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GraphLocalValueProducer_ImplementationPlan_20260527_CN.md`

- [x] **Step 1: Create a fresh smoke TaskSpec for a new Blueprint asset**

Create `D:/UEProjects/Template/Saved/BlueprintHelper/CodexSmoke/GraphLocalValueProducer_20260527_003/01_create_asset.json`:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "task_type": "create_asset",
  "target": {
    "asset_path": "/Game/BlueprintHelper/CodexSmoke/BP_GraphLocalValueProducer_20260527_003",
    "target_type": "blueprint"
  },
  "behavior": {
    "asset_strategy": "ensure_asset",
    "asset": {
      "asset_type": "blueprint",
      "parent_class": "/Script/Engine.Actor",
      "collision_policy": "fail_if_exists"
    }
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report"
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

Run:

```powershell
bh.cmd task preview --file 'D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\GraphLocalValueProducer_20260527_003\01_create_asset.json' --develop --format full
```

Then run execute with the preview token printed by the preview command:

```powershell
$PreviewToken = Read-Host 'Preview token from previous command'
bh.cmd task execute --file 'D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\GraphLocalValueProducer_20260527_003\01_create_asset.json' --preview-token $PreviewToken --develop --format full
```

Expected:
- Preview passed: `D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BlueprintHelper\Cli\preview_1779873242862_0001\result.json`
- Execute passed: `D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BlueprintHelper\Cli\task_CAFAD76D4FB47B7CCE39B586CCE44520\result.json`
- Compile reports 0 errors and 0 warnings.

- [x] **Step 2: Add a TimerHandle variable**

Create `02_add_handle_variable.json`:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "task_type": "edit_blueprint_variables",
  "target": {
    "asset_path": "/Game/BlueprintHelper/CodexSmoke/BP_GraphLocalValueProducer_20260527_003",
    "target_type": "blueprint"
  },
  "behavior": {
    "variable_strategy": "member_variables",
    "changes": [{
      "kind": "ensure_member_variable",
      "name": "LoopDoorTimerHandle",
      "variable_type": {
        "category": "struct",
        "object_path": "/Script/Engine.TimerHandle"
      }
    }]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report"
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

Run preview and execute with the same `bh.cmd task preview/execute` pattern.

Expected:
- Preview passed: `D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BlueprintHelper\Cli\preview_1779873263759_0001\result.json`
- Execute passed: `D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BlueprintHelper\Cli\task_262775064148E850FC60F2881CC059FB\result.json`
- Variable is added and compile reports 0 errors and 0 warnings.

- [x] **Step 3: Write a graph that stores a looping timer handle**

Create `03_write_looping_timer_handle_graph.json`:

```json
{
  "schema": "BlueprintHelper.TaskSpec.v1",
  "task_type": "edit_blueprint_graph",
  "target": {
    "asset_path": "/Game/BlueprintHelper/CodexSmoke/BP_GraphLocalValueProducer_20260527_003",
    "target_type": "blueprint"
  },
  "scope_policy": {
    "graph_name": "EventGraph",
    "allow_modify_user_nodes": false
  },
  "behavior": {
    "graph_strategy": "append_new_owned_graph",
    "entries": [{
      "entry_type": "custom_event",
      "name": "StartLoopingDoorPulse",
      "body": {
        "schema": "BlueprintLogicSpec.v1",
        "statements": [{
          "kind": "call",
          "target": "/Script/Engine.KismetSystemLibrary:K2_SetTimer",
          "value_type": "TimerHandle",
          "result_symbol": "LoopTimerHandle",
          "args": {
            "FunctionName": { "kind": "literal", "value_type": "string", "value": "HandleDoorPulse" },
            "Time": { "kind": "literal", "value_type": "float", "value": 0.75 },
            "bLooping": { "kind": "literal", "value_type": "bool", "value": true }
          }
        }, {
          "kind": "field",
          "field_operation": "set",
          "field_scope": "variable",
          "target": "LoopDoorTimerHandle",
          "value": { "kind": "get", "name": "LoopTimerHandle" }
        }]
      }
    }, {
      "entry_type": "custom_event",
      "name": "HandleDoorPulse",
      "body": {
        "schema": "BlueprintLogicSpec.v1",
        "statements": [{
          "kind": "call",
          "target": "/Script/Engine.KismetSystemLibrary:PrintString",
          "args": {
            "InString": { "kind": "literal", "value_type": "string", "value": "Pulse" }
          }
        }]
      }
    }]
  },
  "execution_policy": {
    "dry_run_mode": "full",
    "on_missing_capability": "stop_and_report",
    "review_baseline_dirty_asset_policy": "save_before_archive"
  },
  "validation": {
    "should_compile": true,
    "should_save": true
  }
}
```

Run:

```powershell
bh.cmd task preview --file 'D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\GraphLocalValueProducer_20260527_003\03_write_looping_timer_handle_graph.json' --develop --format full
```

Then run execute with the preview token printed by the preview command:

```powershell
$PreviewToken = Read-Host 'Preview token from previous command'
bh.cmd task execute --file 'D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\GraphLocalValueProducer_20260527_003\03_write_looping_timer_handle_graph.json' --preview-token $PreviewToken --develop --format full
```

Expected:
- Preview passed: `D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BlueprintHelper\Cli\preview_1779873294828_0001\result.json`
- Execute passed: `D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BlueprintHelper\Cli\task_BF47492149A138C4CE4FD6A675B797FC\result.json`
- Fragment DAG data edge: `StartLoopingDoorPulse_stmt_1.ReturnValue -> StartLoopingDoorPulse_stmt_2.value`, `symbol_id=looptimerhandle`
- Compile reports 0 errors and 0 warnings.
- Readback artifact: `D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BlueprintHelper\Cli\cli_1779873328248\result.json`
- Readback verification: `ok=true`, `start_nodes=3`, `handle_nodes=2`, `timer_to_set_data=true`, `timer_to_set_exec=true`, `entry_to_timer_exec=true`, `total_nodes=8`, `total_data_links=1`, `total_exec_links=3`.

## Task 7: Focused and Full Verification

**Files:**
- Modify only for evidence updates: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GraphLocalValueProducer_ImplementationPlan_20260527_CN.md`

- [x] **Step 1: Run focused GraphLocalValueProducer automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.GraphLocalValueProducer;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_GraphLocalValueProducer_Focused_003'
```

Observed:
- Exit code 0.
- Report: `D:\UEProjects\Template\Saved\Automation\GraphWrite_GraphLocalValueProducer_Focused_003\index.json`
- Counts: `succeeded=4`, `succeededWithWarnings=0`, `failed=0`, `notRun=0`.

- [x] **Step 2: Run existing container-action focused automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ContainerAction;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_ContainerAction_AfterGraphLocalValueProducer_002'
```

Observed:
- Exit code 0.
- Report: `D:\UEProjects\Template\Saved\Automation\GraphWrite_ContainerAction_AfterGraphLocalValueProducer_002\index.json`
- Counts: `succeeded=14`, `succeededWithWarnings=0`, `failed=0`, `notRun=0`.

- [x] **Step 3: Run full GraphWrite automation**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\GraphWrite_Full_AfterGraphLocalValueProducer_002'
```

Observed:
- Exit code 0.
- Report: `D:\UEProjects\Template\Saved\Automation\GraphWrite_Full_AfterGraphLocalValueProducer_002\index.json`
- Counts: `succeeded=312`, `succeededWithWarnings=7`, `failed=0`, `notRun=0`.

- [x] **Step 4: Run TS and plugin build gates**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex -FromMsBuild
git diff --check
git status --short
```

Observed:
- TS build passes.
- Node tests pass: `252` tests, `252` pass, `0` fail.
- UE plugin build passes.
- `git diff --check` final status is recorded under `Implementation Evidence`.
- `git status --short` final status is recorded under `Implementation Evidence`.

## Task 8: Documentation and Manual Git Guidance

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GraphLocalValueProducer_ImplementationPlan_20260527_CN.md`
- Modify if the generality test record needs the new scenario: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md`
- Modify if the real-case E2E record needs the timer-handle smoke result: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_80PercentCapability_TestRecord_20260522_CN.md`

- [x] **Step 1: Record live evidence in this plan**

## Implementation Evidence

| Gate | Command | Result | Artifact |
| --- | --- | --- | --- |
| TS build | `npm.cmd --prefix AgentFaceService/task-core run build` | PASS, exit code 0 | console |
| TS node tests | `npm.cmd --prefix AgentFaceService/task-core run test:node` | PASS, `252/252` tests | console |
| New-uasset E2E smoke | `bh.cmd task preview/execute/context read` on `_003` TaskSpecs | PASS, handle output consumed by later statement and saved to member variable | `D:\UEProjects\Template\Plugins\BlueprintHelper\Saved\BlueprintHelper\Cli\cli_1779873328248\result.json` |
| Focused GraphLocalValueProducer | `Automation RunTests BlueprintHelper.GraphWrite.GraphLocalValueProducer` | PASS, `4` succeeded, `0` failed | `D:\UEProjects\Template\Saved\Automation\GraphWrite_GraphLocalValueProducer_Focused_003\index.json` |
| ContainerAction regression | `Automation RunTests BlueprintHelper.GraphWrite.ContainerAction` | PASS, `14` succeeded, `0` failed | `D:\UEProjects\Template\Saved\Automation\GraphWrite_ContainerAction_AfterGraphLocalValueProducer_002\index.json` |
| Full GraphWrite | `Automation RunTests BlueprintHelper.GraphWrite` | PASS, `312` succeeded, `7` succeeded with warnings, `0` failed | `D:\UEProjects\Template\Saved\Automation\GraphWrite_Full_AfterGraphLocalValueProducer_002\index.json` |
| Plugin build | `Build.bat TemplateEditor Win64 Development` | PASS, exit code 0 | console |
| Diff check | `git diff --check` | PASS, exit code 0; CRLF normalization warnings only | console |
| Git status | `git status --short` | PASS, expected task files are modified/untracked; pre-existing `AGENT.md` remains unrelated | console |

- [x] **Step 2: Add manual commit suggestion to final implementation output**

Do not run `git add`, `git commit`, or `git push`. The final implementation response should suggest commands in this shape:

```powershell
git status --short
git add AgentFaceService/task-core/src/task/compiler/task-compiler.ts `
        AgentFaceService/task-core/src/task/compiler/task-compiler.graph-local-value-producer.test.ts `
        AgentFaceService/task-core/src/task/compiler/task-compiler.convert-schedule.test.ts `
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp `
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp `
        BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp `
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGraphLocalValueProducerTests.cpp `
        BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GraphLocalValueProducer_ImplementationPlan_20260527_CN.md
git commit -m "新增内容：GraphWrite 通用图内返回值符号" `
  -m "1. 将 result_symbol 扩展为 call/create/convert/schedule/container_action 的通用图内值生产者语义。" `
  -m "修复内容：" `
  -m "1. 修复普通 statement 返回值无法被后续 statement 消费的问题。" `
  -m "2. 修复 Temporary get 误进入 FieldVariable demand/resolver 的问题。" `
  -m "3. 修复 void call/schedule 缺少输出证据时仍可声明 result_symbol 的问题。" `
  -m "变更需求：" `
  -m "1. 收紧 schedule expression 和显式 impure call expression，要求使用 statement + result_symbol 消费返回值。"
```

No optional utility files were added under the plugin repo. The `_003` TaskSpecs live under `D:\UEProjects\Template\Saved\BlueprintHelper\CodexSmoke\GraphLocalValueProducer_20260527_003\` as run artifacts.

## Self-Review

- Spec coverage: The plan covers generic `result_symbol`, SemanticIR Temporary symbols, ActionContext suppression, FragmentDAG endpoint mapping, schedule output correction, schedule expression and explicitly impure call expression rejection, call/schedule output-evidence gating, and timer-handle smoke as a non-special-case verification.
- Scope: The plan avoids Timeline internals, timer-only resolver code, legacy fallback, and public API expansion beyond `result_symbol`.
- Type consistency: `LoopTimerHandle` is the graph-local Temporary symbol; `LoopDoorTimerHandle` is the Blueprint member variable; `ReturnValue`, `value`, and `result` are the only output endpoint names used by the plan.
- Architecture check: The implementation stays on the GraphWrite mainline and extends existing registry/resolver/builder boundaries.
- Git rule check: The plan does not instruct an agent to run automatic staging, commits, or pushes; it only gives manual commands for the user after verification.
