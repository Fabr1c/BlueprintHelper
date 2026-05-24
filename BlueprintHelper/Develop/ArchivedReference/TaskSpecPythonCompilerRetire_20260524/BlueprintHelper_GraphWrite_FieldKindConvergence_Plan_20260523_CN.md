# GraphWrite Field Kind Convergence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Converge GraphWrite Field semantics from four internal first-stage kinds (`get`, `set`, `get_property`, `set_property`) to one first-stage `Field` semantic plus `field_operation=get|set` and `field_scope=variable|property_path`.

**Architecture:** AgentFace public input may keep compact `get/set/get_property/set_property` syntax, but compiler lowering, C++ SemanticIR, ActionContext, and ActionResolution must use `Field` as the only FieldVariable first-stage semantic. `field_operation` and `field_scope` become the second-stage operation fields used by demand collection, projected evidence, resolver behavior, stable evidence, diagnostics, and debug output.

**Tech Stack:** UE 5.6 C++, GraphWrite SemanticIR, ActionContextPipeline, FieldVariableActionCluster, Python TaskSpec compiler, TypeScript task-core compiler, Unreal Automation Tests, Node/Python compiler tests.

---

## Execution Rules

- Use subagent-driven development for execution. Use a fresh worker for each P phase.
- Recommended worker routing: architecture/new semantic boundary tasks use `5.5xhigh`; code migration tasks use `5.5high`; medium audits use `5.4mini-0xhigh`; small source-contract audits can use `codex5.3spark`.
- Do not run `git add`, `git commit`, or `git push`. At the end, report touched files and a suggested commit message only.
- Gap5/EventDelegate must already be green before this plan is executed. If EventDelegate taxonomy tests fail, stop and finish Gap5 first; do not mix EventDelegate fixes into this Field slice.
- Do not change `create`, `convert`, `schedule`, `call`, `op`, `construct`, `deconstruct`, `select`, or `control` taxonomy in this slice.
- Do not introduce compatibility fallback inside ActionResolution. Public AgentFace syntax preservation happens only at compiler/lowering boundaries.

## Canonical Field Taxonomy

Agent-facing syntax accepted by schema/compiler:

```text
statement kind=set
statement kind=set_property
expression kind=get
expression kind=get_property
```

Internal TaskPlan / SemanticIR / ActionContext / ActionResolution target:

```text
kind=field
field_operation=get|set
field_scope=variable|property_path
```

Mapping:

| AgentFace input | Internal kind | `field_operation` | `field_scope` |
|---|---|---|---|
| statement `set` | `field` | `set` | `variable` |
| statement `set_property` | `field` | `set` | `property_path` |
| expression `get` | `field` | `get` | `variable` |
| expression `get_property` | `field` | `get` | `property_path` |

ActionResolution invariant:

```text
Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction
Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Field
Request.Semantic.FieldOperation = "get" | "set"
Request.Semantic.FieldScope = "variable" | "property_path"
```

## File Structure

- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/schema/task-contract.ts`
  - Keep public accepted Field syntactic kinds documented.
  - Add notes that runtime lowering emits `field + field_operation + field_scope`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - Lower TypeScript graph body Field statements/expressions to internal `kind: "field"`.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.field.test.ts`
  - Node tests proving TypeScript compiler lowers public Field syntax to internal Field taxonomy.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
  - Lower Python graph body Field statements/expressions to internal `kind: "field"`.
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/tests/test_graph_write_field_statements.py`
  - Python tests proving compiler lowering and validation.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
  - Replace internal FieldVariable first-stage values with `Field`; add `FieldOperation` and `FieldScope` to semantic constraints.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp`
  - Stringify `Field` as `field`; remove string output for removed internal first-stage FieldVariable kinds.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
  - Add `FieldOperation` and `FieldScope` to `FBlueprintHelperActionContextDemand`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - Map Field syntax to `SemanticKind=Field` plus operation/scope.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
  - Project `field_operation` and `field_scope` evidence.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
  - Include `FieldOperation` and `FieldScope` in semantic constraint hash.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
  - Add Field statement/expression representation with second-stage fields.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
  - Parse internal `kind=field` and validate second-stage operation/scope.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`
  - Parse/stringify `field`, `field_operation`, and `field_scope`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
  - Request Field ActionContext via the projected bundle only.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.cpp`
  - Own only `EBlueprintHelperActionSemanticKind::Field`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp`
  - Drive variable/property get/set behavior from `FieldOperation` and `FieldScope`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
  - Add source-contract test preventing FieldVariable top-level semantic drift.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`
  - Add demand/projection tests for `field_operation` and `field_scope`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp`
  - Update resolver tests to use `Field + operation/scope`.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
  - Mark Field taxonomy convergence as implemented after verification.
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`
  - Update FieldVariableActionCluster progress after verification.

---

## P0: Baseline And RED Contracts

**Goal:** Prove current code still exposes `Get/Set/GetProperty/SetProperty` as internal first-stage semantics, then use that failure to drive migration.

**Files:**
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/tests/test_graph_write_field_statements.py`
- Create: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.field.test.ts`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [ ] **Step P0.1: Add Python compiler RED tests**

Create `AgentFaceService/task-core/python/tests/test_graph_write_field_statements.py` with:

```python
import unittest

from blueprinthelper_task.compiler.graph_write_append import compile_graph_write_append


def make_field_spec(statements):
    return {
        "schema": "BlueprintHelper.TaskSpec.v1",
        "context_id": "ctx_field_statements",
        "task_type": "edit_blueprint_graph",
        "feature_name": "FieldFeature",
        "target": {
            "asset_path": "/Game/BP/BP_Door",
            "target_type": "blueprint",
        },
        "scope_policy": {
            "graph_name": "EG_FieldFeature",
            "allow_modify_user_nodes": False,
        },
        "behavior": {
            "graph_strategy": "append_new_owned_graph",
            "entries": [{
                "entry_type": "custom_event",
                "name": "ApplyFields",
                "body": {
                    "schema": "BlueprintLogicSpec.v1",
                    "statements": statements,
                },
            }],
        },
        "execution_policy": {
            "dry_run_mode": "full",
            "on_missing_capability": "stop_and_report",
        },
        "validation": {
            "should_compile": False,
            "should_save": False,
        },
    }


def compiled_statement(statement):
    result = compile_graph_write_append(make_field_spec([statement]), dry_run=True)
    graph_write_step = next(
        step for step in result["task_plan"]["steps"]
        if step["capability"] == "graph_write"
    )
    return graph_write_step["write"]["ops"][0]["body"]["statements"][0]


class GraphWriteFieldStatementCompilerTests(unittest.TestCase):
    def test_set_lowers_to_field_variable_set(self):
        statement = compiled_statement({
            "kind": "set",
            "target": "bIsClosed",
            "value": {"kind": "literal", "value_type": "bool", "value": True},
        })

        self.assertEqual(statement["kind"], "field")
        self.assertEqual(statement["field_operation"], "set")
        self.assertEqual(statement["field_scope"], "variable")
        self.assertEqual(statement["target"], "bIsClosed")

    def test_set_property_lowers_to_field_property_set(self):
        statement = compiled_statement({
            "kind": "set_property",
            "target": "DoorMesh",
            "property_path": "RelativeRotation",
            "value": {
                "kind": "construct",
                "type": "Rotator",
                "args": {"Yaw": {"kind": "literal", "value_type": "number", "value": 90}},
            },
        })

        self.assertEqual(statement["kind"], "field")
        self.assertEqual(statement["field_operation"], "set")
        self.assertEqual(statement["field_scope"], "property_path")
        self.assertEqual(statement["target"], "DoorMesh")
        self.assertEqual(statement["property_path"], "RelativeRotation")

    def test_get_expression_lowers_to_field_variable_get(self):
        statement = compiled_statement({
            "kind": "set",
            "target": "CachedHealth",
            "value": {"kind": "get", "target": "CurrentHealth"},
        })

        value = statement["value"]
        self.assertEqual(value["kind"], "field")
        self.assertEqual(value["field_operation"], "get")
        self.assertEqual(value["field_scope"], "variable")
        self.assertEqual(value["target"], "CurrentHealth")

    def test_get_property_expression_lowers_to_field_property_get(self):
        statement = compiled_statement({
            "kind": "set",
            "target": "YawCache",
            "value": {
                "kind": "get_property",
                "target": "DoorMesh",
                "property_path": "RelativeRotation.Yaw",
            },
        })

        value = statement["value"]
        self.assertEqual(value["kind"], "field")
        self.assertEqual(value["field_operation"], "get")
        self.assertEqual(value["field_scope"], "property_path")
        self.assertEqual(value["target"], "DoorMesh")
        self.assertEqual(value["property_path"], "RelativeRotation.Yaw")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step P0.2: Add TypeScript compiler RED tests**

Create `AgentFaceService/task-core/src/task/compiler/task-compiler.field.test.ts` with:

```ts
import { strict as assert } from 'node:assert';
import test from 'node:test';

import { compileTaskSpecToTaskPlan } from './task-compiler.js';

function makeFieldSpec(statement: Record<string, unknown>) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_field_statement_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'FieldFeatureTs',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EG_FieldFeatureTs',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'ApplyFields',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements: [statement],
        },
      }],
    },
    execution_policy: {
      dry_run_mode: 'full',
      on_missing_capability: 'stop_and_report',
    },
    validation: {
      should_compile: false,
      should_save: false,
    },
  };
}

function compileFirstStatement(statement: Record<string, unknown>) {
  const taskPlan = compileTaskSpecToTaskPlan(makeFieldSpec(statement) as never);
  const graphWriteStep = taskPlan.steps.find((step) => step.capability === 'graph_write');
  assert.ok(graphWriteStep);
  return graphWriteStep.write.ops[0].body.statements[0] as Record<string, unknown>;
}

test('set lowers to field variable set', () => {
  const statement = compileFirstStatement({
    kind: 'set',
    target: 'bIsClosed',
    value: { kind: 'literal', value_type: 'bool', value: true },
  });

  assert.equal(statement.kind, 'field');
  assert.equal(statement.field_operation, 'set');
  assert.equal(statement.field_scope, 'variable');
  assert.equal(statement.target, 'bIsClosed');
});

test('get_property lowers to field property get inside a value expression', () => {
  const statement = compileFirstStatement({
    kind: 'set',
    target: 'YawCache',
    value: {
      kind: 'get_property',
      target: 'DoorMesh',
      property_path: 'RelativeRotation.Yaw',
    },
  });

  const value = statement.value as Record<string, unknown>;
  assert.equal(value.kind, 'field');
  assert.equal(value.field_operation, 'get');
  assert.equal(value.field_scope, 'property_path');
  assert.equal(value.target, 'DoorMesh');
  assert.equal(value.property_path, 'RelativeRotation.Yaw');
});
```

- [ ] **Step P0.3: Add C++ source contract RED test**

Append this test to `BlueprintHelperActionResolutionContractTests.cpp` before `#endif`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionResolutionFieldSemanticTaxonomyContractTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Contract.FieldSemanticTaxonomy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionResolutionFieldSemanticTaxonomyContractTest::RunTest(const FString& Parameters)
{
	const TArray<FString> SourcePaths = {
		BuildGraphWritePrivateSourcePath(TEXT("ActionResolution"), TEXT("BlueprintHelperFieldVariableActionCluster.cpp")),
		BuildGraphWritePrivateSourcePath(TEXT("ActionResolution"), TEXT("BlueprintHelperFieldVariableActionResolver.cpp")),
		BuildGraphWritePrivateSourcePath(TEXT("ActionResolution/Context"), TEXT("BlueprintHelperActionContextDemandCollector.cpp")),
		BuildGraphWritePrivateSourcePath(TEXT("ActionResolution/Context"), TEXT("BlueprintHelperActionContextInferenceService.cpp")),
		BuildGraphWritePrivateSourcePath(TEXT("GraphStatement"), TEXT("BlueprintHelperGraphStatementBuilder.cpp"))
	};

	const TArray<FString> ForbiddenTokens = {
		TEXT("EBlueprintHelperActionSemanticKind::Get"),
		TEXT("EBlueprintHelperActionSemanticKind::Set"),
		TEXT("EBlueprintHelperActionSemanticKind::GetProperty"),
		TEXT("EBlueprintHelperActionSemanticKind::SetProperty")
	};

	bool bClean = true;
	for (const FString& Token : ForbiddenTokens)
	{
		bClean &= ScanSpecificGraphWriteSourcesForForbiddenToken(*this, SourcePaths, Token);
	}

	FString FieldClusterSource;
	if (FFileHelper::LoadFileToString(FieldClusterSource, *SourcePaths[0]))
	{
		bClean &= TestTrue(TEXT("Field cluster owns Field first-stage semantic"), FieldClusterSource.Contains(TEXT("EBlueprintHelperActionSemanticKind::Field")));
	}
	else
	{
		AddError(FString::Printf(TEXT("Field cluster source could not be read: %s"), *SourcePaths[0]));
		bClean = false;
	}

	TestTrue(TEXT("FieldVariable uses Field first-stage semantic plus second-stage field operation/scope"), bClean);
	return bClean;
}
```

- [ ] **Step P0.4: Add C++ ActionContext RED tests**

Update `BlueprintHelperActionContextPipelineTests.cpp` by replacing `FBlueprintHelperActionContextSingleDemandSetPropertyMapsToFieldVariableTest` expectations with:

```cpp
TestEqual(TEXT("SetProperty cluster kind"), Demand.ClusterKind, EBlueprintHelperSpawnerClusterKind::FieldVariableAction);
TestEqual(TEXT("SetProperty first-stage semantic"), Demand.SemanticKind, EBlueprintHelperActionSemanticKind::Field);
TestEqual(TEXT("SetProperty field operation"), Demand.FieldOperation, FString(TEXT("set")));
TestEqual(TEXT("SetProperty field scope"), Demand.FieldScope, FString(TEXT("property_path")));
TestEqual(TEXT("SetProperty query"), Demand.Query, FString(TEXT("DoorMesh.RelativeRotation")));
TestTrue(TEXT("SetProperty requires typed pins"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::TypedPins));
TestTrue(TEXT("SetProperty requires target"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::Target));
```

Add a second test below it:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextFieldProjectionEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionContext.FieldSemantic.ProjectionEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextFieldProjectionEvidenceTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionContextDemand Demand;
	Demand.StatementId = TEXT("stmt_field_set");
	Demand.SourcePath = TEXT("$.statements[0]");
	Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	Demand.SemanticKind = EBlueprintHelperActionSemanticKind::Field;
	Demand.Query = TEXT("bIsClosed");
	Demand.TargetPath = TEXT("bIsClosed");
	Demand.FieldOperation = TEXT("set");
	Demand.FieldScope = TEXT("variable");

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	Snapshot.Graph.BlueprintClassPath = TEXT("/Game/Test/BP_Door.BP_Door_C");

	FBlueprintHelperActionContextFieldSnapshot Field;
	Field.Name = TEXT("bIsClosed");
	Field.OwnerClassPath = TEXT("/Game/Test/BP_Door.BP_Door_C");
	Field.PinCategory = TEXT("bool");
	Snapshot.Fields.Add(Field);

	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demand);

	TestEqual(TEXT("semantic"), Context.Semantic.Kind, EBlueprintHelperActionSemanticKind::Field);
	TestEqual(TEXT("field operation"), Context.Semantic.FieldOperation, FString(TEXT("set")));
	TestEqual(TEXT("field scope"), Context.Semantic.FieldScope, FString(TEXT("variable")));
	TestEqual(TEXT("field operation evidence"), Context.Evidence.FindRef(TEXT("field_operation")), FString(TEXT("set")));
	TestEqual(TEXT("field scope evidence"), Context.Evidence.FindRef(TEXT("field_scope")), FString(TEXT("variable")));
	TestEqual(TEXT("field name evidence"), Context.Evidence.FindRef(TEXT("field_name")), FString(TEXT("bIsClosed")));
	return true;
}
```

- [ ] **Step P0.5: Run RED checks**

Run Python RED:

```powershell
Push-Location AgentFaceService/task-core
python -m unittest discover -s python/tests -t python -p "test_graph_write_field_statements.py"
Pop-Location
```

Expected: FAIL before implementation because compiled statements/expressions still use `set`, `set_property`, `get`, or `get_property`.

Run TypeScript RED:

```powershell
Push-Location AgentFaceService/task-core
npm run build
npm run test:node
Pop-Location
```

Expected: FAIL before implementation because the new Node tests expect `kind: "field"` output.

Run C++ RED:

```powershell
& 'E:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UEProjects/Template/Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract.FieldSemanticTaxonomy;BlueprintHelper.GraphWrite.ActionContext.FieldSemantic;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:/UEProjects/Template/Saved/Automation/GraphWrite_FieldKind_RED_001'
```

Expected: FAIL before implementation because C++ still references `Get/Set/GetProperty/SetProperty` as first-stage action semantic values.

- [ ] **Step P0.6: Audit and worker correction gate**

Audit requirements:

```text
P0 complete only if all new tests fail for the expected Field taxonomy reason.
If a test fails because of syntax, missing include, unrelated build break, or EventDelegate drift, worker fixes the test setup before moving to P1.
No implementation file may be changed in P0 except test files.
```

---

## P1: AgentFace Compiler Lowering

**Goal:** Keep public AgentFace Field syntax stable while lowering emitted graph body to `kind=field`.

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/python/blueprinthelper_task/compiler/graph_write_append.py`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/AgentFaceService/task-core/src/task/schema/task-contract.ts`

- [ ] **Step P1.1: Add Python field lowering helpers**

In `graph_write_append.py`, add helpers near the supported graph body kind constants:

```python
FIELD_STATEMENT_KIND_MAP = {
    "set": ("set", "variable"),
    "set_property": ("set", "property_path"),
}

FIELD_EXPRESSION_KIND_MAP = {
    "get": ("get", "variable"),
    "get_property": ("get", "property_path"),
}


def _is_field_statement_kind(kind: str) -> bool:
    return kind in FIELD_STATEMENT_KIND_MAP


def _is_field_expression_kind(kind: str) -> bool:
    return kind in FIELD_EXPRESSION_KIND_MAP


def _apply_field_statement_taxonomy(out: Dict[str, Any], source_kind: str) -> None:
    operation, scope = FIELD_STATEMENT_KIND_MAP[source_kind]
    out["kind"] = "field"
    out["field_operation"] = operation
    out["field_scope"] = scope


def _apply_field_expression_taxonomy(out: Dict[str, Any], source_kind: str) -> None:
    operation, scope = FIELD_EXPRESSION_KIND_MAP[source_kind]
    out["kind"] = "field"
    out["field_operation"] = operation
    out["field_scope"] = scope
```

- [ ] **Step P1.2: Lower Python statements to `field`**

In the statement normalization path that currently keeps `set` and `set_property`, change the output construction so both use:

```python
if kind in {"set", "set_property"}:
    _apply_field_statement_taxonomy(out, kind)
    out["target"] = _required_string(statement, "target", f"{path}.target")
    if kind == "set_property":
        property_path = required_graph_body_property_path(statement, path)
        out["property_path"] = property_path
        out["property"] = property_path
    out["value"] = _normalize_expression(statement.get("value"), f"{path}.value")
```

Keep validation accepting public `set` and `set_property`; only emitted internal body changes.

- [ ] **Step P1.3: Lower Python expressions to `field`**

In the expression normalization path, replace direct output of `get` and `get_property` with:

```python
if kind in {"get", "get_property"}:
    out = {
        "id": expression_id,
        "target": _required_string(expression, "target", f"{path}.target"),
    }
    _apply_field_expression_taxonomy(out, kind)
    if kind == "get_property":
        property_path = required_graph_body_property_path(expression, path)
        out["property_path"] = property_path
        out["property"] = property_path
    return out
```

Use the existing local variable names in the file. If the current function uses `node_id` rather than `expression_id`, set `"id": node_id`.

- [ ] **Step P1.4: Add TypeScript field lowering helpers**

In `task-compiler.ts`, add helpers near `SUPPORTED_GRAPH_BODY_STATEMENT_KINDS`:

```ts
const FIELD_STATEMENT_KIND_MAP = new Map([
  ['set', { operation: 'set', scope: 'variable' }],
  ['set_property', { operation: 'set', scope: 'property_path' }],
]);

const FIELD_EXPRESSION_KIND_MAP = new Map([
  ['get', { operation: 'get', scope: 'variable' }],
  ['get_property', { operation: 'get', scope: 'property_path' }],
]);

function applyFieldTaxonomy(record: Record<string, unknown>, operation: string, scope: string): void {
  record.kind = 'field';
  record.field_operation = operation;
  record.field_scope = scope;
}
```

- [ ] **Step P1.5: Lower TypeScript statements and expressions**

In TypeScript statement compilation, when source `kind` is `set` or `set_property`, output:

```ts
const field = FIELD_STATEMENT_KIND_MAP.get(kind);
if (field) {
  const out: Record<string, unknown> = {
    id: nodeId,
    target: getRequiredString(statementRecord, 'target', `${path}.target`),
    value: normalizeGraphBodyExpression(statementRecord.value, `${path}.value`),
  };
  applyFieldTaxonomy(out, field.operation, field.scope);
  if (kind === 'set_property') {
    const propertyPath = requiredGraphBodyPropertyPath(statementRecord, path);
    out.property_path = propertyPath;
    out.property = propertyPath;
  }
  return out;
}
```

In expression compilation, when source `kind` is `get` or `get_property`, output:

```ts
const field = FIELD_EXPRESSION_KIND_MAP.get(kind);
if (field) {
  const out: Record<string, unknown> = {
    id: nodeId,
    target: getRequiredString(expression, 'target', `${path}.target`),
  };
  applyFieldTaxonomy(out, field.operation, field.scope);
  if (kind === 'get_property') {
    const propertyPath = requiredGraphBodyPropertyPath(expression, path);
    out.property_path = propertyPath;
    out.property = propertyPath;
  }
  return out;
}
```

Use existing function names if the compiler already has normalization helpers with different names; do not change public validation messages except to mention that `get/set/get_property/set_property` are public syntax lowered to internal `field`.

- [ ] **Step P1.6: Update task contract notes**

In `task-contract.ts`, keep public lists:

```ts
statement_kinds: ['call', 'set', 'set_property', 'let', 'control', ...]
expression_kinds: ['literal', 'get', 'get_property', 'call', 'op', 'construct', 'deconstruct', 'select']
```

Add a capability note under graph write first-slice metadata:

```ts
field_taxonomy: {
  agent_facing_statement_kinds: ['set', 'set_property'],
  agent_facing_expression_kinds: ['get', 'get_property'],
  internal_first_stage_semantic: 'field',
  second_stage_fields: ['field_operation', 'field_scope'],
}
```

- [ ] **Step P1.7: Run compiler tests**

```powershell
Push-Location AgentFaceService/task-core
python -m unittest discover -s python/tests -t python -p "test_graph_write_field_statements.py"
npm run build
npm run test:node
Pop-Location
```

Expected: Python Field tests PASS; TypeScript Field tests PASS; existing compiler tests still PASS.

- [ ] **Step P1.8: Audit and worker correction gate**

Audit requirements:

```text
Public AgentFace syntax still accepts set, set_property, get, get_property.
Emitted internal graph body uses kind=field with field_operation and field_scope.
No ActionResolution C++ file has been modified in P1.
If compiler output emits field_operation without field_scope, or field_scope without field_operation, worker corrects before P2.
```

---

## P2: C++ SemanticIR Field Representation

**Goal:** Teach C++ SemanticIR to consume compiler-lowered `kind=field` and carry second-stage Field fields.

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphSemanticIRUtils.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementTypeUtils.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.cpp`

- [ ] **Step P2.1: Update SemanticIR enums and fields**

In `BlueprintHelperGraphSemanticIR.h`, change Field-specific enum shape to:

```cpp
enum class EBlueprintHelperGraphStatementKind : uint8
{
	Unknown,
	Call,
	Field,
	Branch,
	Sequence,
	Let,
	Return,
	ComponentBoundEvent,
	Delegate
};

enum class EBlueprintHelperGraphExpressionKind : uint8
{
	Unknown,
	Literal,
	Field,
	Call,
	Op,
	Construct,
	Deconstruct,
	Select
};
```

Add these fields to both statement and expression IR structs:

```cpp
FString FieldOperation;
FString FieldScope;
```

- [ ] **Step P2.2: Parse `kind=field`**

In `BlueprintHelperGraphSemanticIRUtils.cpp`, parse internal Field as:

```cpp
if (Kind.Equals(TEXT("field"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphStatementKind::Field;
```

and:

```cpp
if (Kind.Equals(TEXT("field"), ESearchCase::IgnoreCase)) return EBlueprintHelperGraphExpressionKind::Field;
```

Do not add C++ parser compatibility for raw `get`, `set`, `get_property`, or `set_property`. Those are public AgentFace compiler inputs, not internal C++ SemanticIR inputs after this plan.

- [ ] **Step P2.3: Validate Field operation and scope**

In `BlueprintHelperGraphSemanticIR.cpp`, when parsing a Field statement/expression, require:

```cpp
Object->TryGetStringField(TEXT("field_operation"), Node->FieldOperation);
Object->TryGetStringField(TEXT("field_scope"), Node->FieldScope);

if (!(Node->FieldOperation == TEXT("get") || Node->FieldOperation == TEXT("set")))
{
	FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
		OutIR,
		TEXT("field_operation_unsupported"),
		Path + TEXT(".field_operation"),
		FString::Printf(TEXT("Unsupported field operation: %s."), *Node->FieldOperation));
}

if (!(Node->FieldScope == TEXT("variable") || Node->FieldScope == TEXT("property_path")))
{
	FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
		OutIR,
		TEXT("field_scope_unsupported"),
		Path + TEXT(".field_scope"),
		FString::Printf(TEXT("Unsupported field scope: %s."), *Node->FieldScope));
}
```

Also enforce statement Field operation:

```cpp
if (Statement->Kind == EBlueprintHelperGraphStatementKind::Field
	&& Statement->FieldOperation != TEXT("set"))
{
	FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
		OutIR,
		TEXT("field_statement_operation_unsupported"),
		Path + TEXT(".field_operation"),
		TEXT("Field statements currently support field_operation=set."));
}
```

and expression Field operation:

```cpp
if (Expression->Kind == EBlueprintHelperGraphExpressionKind::Field
	&& Expression->FieldOperation != TEXT("get"))
{
	FBlueprintHelperGraphSemanticIRUtils::AddDiagnostic(
		OutIR,
		TEXT("field_expression_operation_unsupported"),
		Path + TEXT(".field_operation"),
		TEXT("Field expressions currently support field_operation=get."));
}
```

- [ ] **Step P2.4: Update type utils and pipeline switches**

Every switch that previously handled `Set` and `SetProperty` should now handle `Field` and branch on `FieldOperation` / `FieldScope`.

Use this local helper pattern in affected `.cpp` files:

```cpp
static bool IsFieldSetStatement(const FBlueprintHelperGraphStatementIR& Statement)
{
	return Statement.Kind == EBlueprintHelperGraphStatementKind::Field
		&& Statement.FieldOperation == TEXT("set");
}

static bool IsFieldPropertyScope(const FString& FieldScope)
{
	return FieldScope == TEXT("property_path");
}
```

For expression switches, replace `Get` and `GetProperty` handling with:

```cpp
case EBlueprintHelperGraphExpressionKind::Field:
	// Use Expression.FieldOperation and Expression.FieldScope.
	break;
```

- [ ] **Step P2.5: Run SemanticIR and compile checks**

```powershell
& 'E:/UE_5.6/Engine/Build/BatchFiles/Build.bat' TemplateEditor Win64 Development 'D:/UEProjects/Template/Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected: compile fails until all old C++ Field-specific enum references have been migrated, then succeeds.

- [ ] **Step P2.6: Audit and worker correction gate**

Audit requirements:

```text
C++ SemanticIR accepts internal kind=field.
C++ SemanticIR rejects missing or unsupported field_operation/field_scope with explicit diagnostics.
No C++ SemanticIR parser path accepts raw get/set/get_property/set_property after compiler lowering.
If direct public payloads still depend on raw Field kinds, worker records the caller and migrates that caller to compiler-lowered payload before P3.
```

---

## P3: ActionSemantic And ActionContext Projection

**Goal:** Replace `Get/Set/GetProperty/SetProperty` first-stage ActionResolution semantics with `Field`.

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [ ] **Step P3.1: Update Action semantic core type**

In `BlueprintHelperActionResolutionCore.h`, replace:

```cpp
Get,
Set,
GetProperty,
SetProperty,
```

with:

```cpp
Field,
```

In `FBlueprintHelperActionSemanticConstraints`, add:

```cpp
FString FieldOperation;
FString FieldScope;
```

- [ ] **Step P3.2: Update semantic stringification**

In `BlueprintHelperActionResolutionCore.cpp`, add:

```cpp
case EBlueprintHelperActionSemanticKind::Field: return TEXT("field");
```

Remove `get`, `set`, `get_property`, and `set_property` action semantic stringification from this enum switch.

- [ ] **Step P3.3: Add ActionContext demand fields**

In `FBlueprintHelperActionContextDemand`, add:

```cpp
FString FieldOperation;
FString FieldScope;
```

Update `BuildSingleDemand` and private `BuildDemand` signatures to accept:

```cpp
const FString& FieldOperation,
const FString& FieldScope,
```

Callers that are not Field semantics pass empty strings.

- [ ] **Step P3.4: Map SemanticIR Field into ActionContext demand**

In `BlueprintHelperActionContextDemandCollector.cpp`, when statement kind is Field:

```cpp
Demand.SemanticKind = EBlueprintHelperActionSemanticKind::Field;
Demand.FieldOperation = Statement.FieldOperation;
Demand.FieldScope = Statement.FieldScope;
Demand.DefaultValues.FindOrAdd(TEXT("field_operation")) = Statement.FieldOperation;
Demand.DefaultValues.FindOrAdd(TEXT("field_scope")) = Statement.FieldScope;
```

For expression kind Field:

```cpp
Demand.SemanticKind = EBlueprintHelperActionSemanticKind::Field;
Demand.FieldOperation = Expression.FieldOperation;
Demand.FieldScope = Expression.FieldScope;
Demand.DefaultValues.FindOrAdd(TEXT("field_operation")) = Expression.FieldOperation;
Demand.DefaultValues.FindOrAdd(TEXT("field_scope")) = Expression.FieldScope;
```

In `ApplyDemandKinds`, replace the FieldVariable cluster branch with:

```cpp
case EBlueprintHelperActionSemanticKind::Field:
	Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::TypedPins);
	Demand.RequiredKinds.Add(EBlueprintHelperActionContextDemandKind::Target);
	break;
```

- [ ] **Step P3.5: Project Field operation/scope through inference**

In `BlueprintHelperActionContextInferenceService.cpp`, copy the demand fields:

```cpp
Context.Semantic.FieldOperation = Demand.FieldOperation;
Context.Semantic.FieldScope = Demand.FieldScope;
BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("field_operation"), Demand.FieldOperation);
BlueprintHelperActionContextInference::AddEvidenceIfPresent(Context, TEXT("field_scope"), Demand.FieldScope);
```

- [ ] **Step P3.6: Include Field operation/scope in semantic hash**

In `BlueprintHelperActionContextBundleProjector.cpp`, append to `BuildSemanticConstraintsHash`:

```cpp
Stable += TEXT("|field_operation:");
Stable += Semantic.FieldOperation;
Stable += TEXT("|field_scope:");
Stable += Semantic.FieldScope;
```

- [ ] **Step P3.7: Run ActionContext tests**

```powershell
& 'E:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UEProjects/Template/Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionContext;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:/UEProjects/Template/Saved/Automation/GraphWrite_FieldKind_ActionContext_001'
```

Expected: ActionContext tests PASS; Field demand/projection tests record `SemanticKind=Field`, `field_operation`, and `field_scope`.

- [ ] **Step P3.8: Audit and worker correction gate**

Audit requirements:

```text
No ActionContext code maps FieldVariable behavior through Get/Set/GetProperty/SetProperty action semantic values.
BundleProjector hash changes when field_operation or field_scope changes.
Debug/evidence still has enough information to distinguish get vs set and variable vs property_path.
If operation/scope is only stored in ContextEvidence but not FBlueprintHelperActionSemanticConstraints, worker corrects before P4.
```

---

## P4: FieldVariable Cluster And Resolver Migration

**Goal:** Make FieldVariableActionCluster consume only `Field + field_operation + field_scope`.

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionCluster.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldVariableActionResolver.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperFieldVariableActionClusterTests.cpp`

- [ ] **Step P4.1: Update cluster ownership**

In `BlueprintHelperFieldVariableActionCluster.cpp`, replace ownership switch with:

```cpp
bool FBlueprintHelperFieldVariableActionCluster::OwnsSemanticKind(EBlueprintHelperActionSemanticKind Kind)
{
	return Kind == EBlueprintHelperActionSemanticKind::Field;
}
```

- [ ] **Step P4.2: Add resolver helpers**

In `BlueprintHelperFieldVariableActionResolver.cpp`, replace `IsPropertySemanticKind` and `IsWritableSemanticKind` with:

```cpp
static FString GetFieldOperation(const FBlueprintHelperActionResolutionRequest& Request)
{
	FString Operation = Request.Semantic.FieldOperation.TrimStartAndEnd().ToLower();
	if (Operation.IsEmpty())
	{
		Operation = Request.ContextEvidence.FindRef(TEXT("field_operation")).TrimStartAndEnd().ToLower();
	}
	return Operation;
}

static FString GetFieldScope(const FBlueprintHelperActionResolutionRequest& Request)
{
	FString Scope = Request.Semantic.FieldScope.TrimStartAndEnd().ToLower();
	if (Scope.IsEmpty())
	{
		Scope = Request.ContextEvidence.FindRef(TEXT("field_scope")).TrimStartAndEnd().ToLower();
	}
	return Scope;
}

static bool IsWritableFieldOperation(const FString& FieldOperation)
{
	return FieldOperation == TEXT("set");
}

static bool IsPropertyFieldScope(const FString& FieldScope)
{
	return FieldScope == TEXT("property_path");
}
```

- [ ] **Step P4.3: Validate resolver semantic shape**

At the start of resolver `Resolve`, after semantic ownership check:

```cpp
const FString FieldOperation = GetFieldOperation(Request);
const FString FieldScope = GetFieldScope(Request);

if (Request.Semantic.Kind != EBlueprintHelperActionSemanticKind::Field
	|| !(FieldOperation == TEXT("get") || FieldOperation == TEXT("set"))
	|| !(FieldScope == TEXT("variable") || FieldScope == TEXT("property_path")))
{
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ErrorCode = TEXT("needs_more_semantic_context");
	Result.Message = FString::Printf(
		TEXT("FieldVariableActionCluster requires semantic=field with field_operation=get|set and field_scope=variable|property_path; semantic=%s operation=%s scope=%s."),
		*FBlueprintHelperActionResolutionCore::SemanticKindToString(Request.Semantic.Kind),
		*FieldOperation,
		*FieldScope);
	return Result;
}
```

- [ ] **Step P4.4: Drive node class and property behavior from operation/scope**

Replace:

```cpp
TSubclassOf<UK2Node_Variable> NodeClass = IsWritableSemanticKind(Request.Semantic.Kind)
```

with:

```cpp
TSubclassOf<UK2Node_Variable> NodeClass = IsWritableFieldOperation(FieldOperation)
```

Replace:

```cpp
const bool bPropertySemantic = IsPropertySemanticKind(Request.Semantic.Kind);
```

with:

```cpp
const bool bPropertySemantic = IsPropertyFieldScope(FieldScope);
```

- [ ] **Step P4.5: Update stable ids and diagnostics**

Replace stable id construction with:

```cpp
CandidateInfo.StableId = FString::Printf(
	TEXT("field_variable:%s:%s:%s:%s"),
	Request.Blueprint ? *Request.Blueprint->GetPathName() : TEXT("unknown_blueprint"),
	*FieldName,
	*FieldOperation,
	*FieldScope);
```

Replace diagnostic text that currently prints only semantic kind with text that prints:

```cpp
semantic=field operation=%s scope=%s
```

using `FieldOperation` and `FieldScope`.

- [ ] **Step P4.6: Update FieldVariable automation tests**

In `BlueprintHelperFieldVariableActionClusterTests.cpp`, change `MakeFieldVariableActionRequest` signature to:

```cpp
static FBlueprintHelperActionResolutionRequest MakeFieldVariableActionRequest(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& FieldOperation,
	const FString& FieldScope,
	const FString& Query)
{
	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FieldVariableAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeFieldVariableActionTestObjectName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("field_variable_projected_context");
	Request.SemanticConstraintsHash = TEXT("field_variable_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Field;
	Request.Semantic.FieldOperation = FieldOperation;
	Request.Semantic.FieldScope = FieldScope;
	Request.Semantic.Query = Query;
	Request.Semantic.TargetPath = Query;
	Request.ContextEvidence.Add(TEXT("field_operation"), FieldOperation);
	Request.ContextEvidence.Add(TEXT("field_scope"), FieldScope);
	Request.MaxCandidates = 8;
	return Request;
}
```

Update existing test calls:

```cpp
MakeFieldVariableActionRequest(Blueprint, Graph, TEXT("get"), TEXT("variable"), TEXT("SmokeFloat"));
MakeFieldVariableActionRequest(Blueprint, Graph, TEXT("set"), TEXT("variable"), TEXT("bSmokeFlag"));
MakeFieldVariableActionRequest(Blueprint, Graph, TEXT("get"), TEXT("property_path"), TEXT("DoorMesh.RelativeRotation"));
MakeFieldVariableActionRequest(Blueprint, Graph, TEXT("set"), TEXT("property_path"), TEXT("DoorMesh.SimulatePhysics"));
```

Update stable id assertions:

```cpp
TestTrue(TEXT("stable id contains get operation"), Result.CandidateActions[0].StableId.Contains(TEXT(":get:variable")));
TestTrue(TEXT("stable id contains set property operation"), Result.SelectedStableId.Contains(TEXT(":set:property_path")));
```

- [ ] **Step P4.7: Run FieldVariable tests**

```powershell
& 'E:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UEProjects/Template/Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.FieldVariable;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:/UEProjects/Template/Saved/Automation/GraphWrite_FieldKind_FieldVariable_001'
```

Expected: FieldVariable automation PASS; positive get/set/property cases still resolve selected spawners; missing evidence cases still return `missing_required_evidence`.

- [ ] **Step P4.8: Audit and worker correction gate**

Audit requirements:

```text
FieldVariableActionCluster owns only Field.
Resolver does not compare Request.Semantic.Kind to Get, Set, GetProperty, or SetProperty.
Resolver chooses VariableGet vs VariableSet from field_operation.
Resolver chooses variable vs property_path behavior from field_scope.
Stable evidence includes enough second-stage detail to distinguish the four original public operations.
```

---

## P5: Builders, Debug Evidence, And Documentation

**Goal:** Remove remaining internal first-stage FieldVariable references from builder, debug, metrics, and docs.

**Files:**
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphFragmentDagBuilderUtils.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperGraphWriteCapabilityMetrics.cpp`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
- Modify: `D:/UEProjects/Template/Plugins/BlueprintHelper/BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`

- [ ] **Step P5.1: Update GraphStatementBuilder semantic resolution**

Replace any local expression/statement semantic mapping for `Get`, `Set`, `GetProperty`, `SetProperty` with:

```cpp
return EBlueprintHelperActionSemanticKind::Field;
```

and ensure the projected request carries:

```cpp
Request.Semantic.FieldOperation
Request.Semantic.FieldScope
```

Builder code must not infer cluster or operation locally; it must consume the projected ActionContext request.

- [ ] **Step P5.2: Update FragmentDAG/debug metadata**

Where fragment/debug metadata currently writes semantic kind as `get`, `set`, `get_property`, or `set_property`, change output to:

```cpp
AddMetadata(Fragment, TEXT("semantic.kind"), TEXT("field"));
AddMetadata(Fragment, TEXT("semantic.field_operation"), Request.Semantic.FieldOperation);
AddMetadata(Fragment, TEXT("semantic.field_scope"), Request.Semantic.FieldScope);
```

If the surrounding code does not have `Request`, use the statement/expression IR fields:

```cpp
AddMetadata(Fragment, TEXT("semantic.kind"), TEXT("field"));
AddMetadata(Fragment, TEXT("semantic.field_operation"), Statement.FieldOperation);
AddMetadata(Fragment, TEXT("semantic.field_scope"), Statement.FieldScope);
```

- [ ] **Step P5.3: Update metrics/readback expectations**

In capability metrics and debug summaries, keep `semantic_kind=field` and add or preserve operation/scope detail:

```cpp
Json->SetStringField(TEXT("semantic_kind"), TEXT("field"));
Json->SetStringField(TEXT("field_operation"), FieldOperation);
Json->SetStringField(TEXT("field_scope"), FieldScope);
```

If existing consumers only display `semantic_kind`, they should show `field`; operation/scope belongs in detail evidence, not in first-stage semantic.

- [ ] **Step P5.4: Update design/status docs**

In `BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`, mark the Field row as implemented:

```markdown
| `get`, `set`, `get_property`, `set_property` | `Field` | `field_operation=get/set`, `field_scope=variable/property_path` | Implemented; public AgentFace syntax remains compact, internal GraphWrite uses Field first-stage semantic. |
```

In `BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`, update FieldVariableActionCluster notes:

```markdown
Field taxonomy 已收敛到 `Field + field_operation + field_scope`；resolver 仍以 projected `field_name`、`property_path`、owner、pin/type evidence 为显式输入，不再把 get/set/get_property/set_property 作为 ActionResolution first-stage semantic。
```

- [ ] **Step P5.5: Run source scan**

```powershell
rg -n "EBlueprintHelperActionSemanticKind::(Get|Set|GetProperty|SetProperty)|Request\\.Semantic\\.Kind == EBlueprintHelperActionSemanticKind::(Get|Set|GetProperty|SetProperty)|SemanticKindToString\\(.*(Get|SetProperty|GetProperty|Set)" BlueprintHelper/Source/BlueprintHelper
```

Expected: no live implementation hits. Test files may mention removed tokens only inside explicit forbidden-token tests.

- [ ] **Step P5.6: Audit and worker correction gate**

Audit requirements:

```text
DebugBundle and metrics do not invent separate meanings for get/set/get_property/set_property.
Docs state public syntax vs internal semantic boundary.
No unrelated create/convert/schedule/call/op taxonomy changes are present.
```

---

## P6: Verification, Regression, And Closure

**Goal:** Prove Field taxonomy migration is complete without breaking GraphWrite regression or UE 5.6 compile.

**Files:**
- No planned new source files in P6.
- Modify docs only if verification results need to be recorded.

- [ ] **Step P6.1: Run compiler tests**

```powershell
Push-Location AgentFaceService/task-core
npm run build
npm run test:python
npm run test:node
Pop-Location
```

Expected: all TypeScript, Python, and Node tests PASS.

- [ ] **Step P6.2: Run focused UE automation**

```powershell
& 'E:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UEProjects/Template/Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Contract;BlueprintHelper.GraphWrite.ActionContext;BlueprintHelper.GraphWrite.ActionResolution.FieldVariable;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:/UEProjects/Template/Saved/Automation/GraphWrite_FieldKind_Focused_001'
```

Expected: focused GraphWrite contract, ActionContext, and FieldVariable suites PASS.

- [ ] **Step P6.3: Run broader GraphWrite regression**

```powershell
& 'E:/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UEProjects/Template/Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:/UEProjects/Template/Saved/Automation/GraphWrite_FieldKind_Regression_001'
```

Expected: no failed or not-run GraphWrite tests. Existing warnings are allowed only if they match known pre-existing warning classes and do not mention Field taxonomy.

- [ ] **Step P6.4: Run UE 5.6 compile**

```powershell
& 'E:/UE_5.6/Engine/Build/BatchFiles/Build.bat' TemplateEditor Win64 Development 'D:/UEProjects/Template/Template.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected: build succeeds.

- [ ] **Step P6.5: Run diff hygiene**

```powershell
git diff --check
git status --short
```

Expected: no whitespace errors. Status must show only files modified for this Field convergence slice plus pre-existing user changes that are explicitly excluded from commit instructions.

- [ ] **Step P6.6: Final audit and worker correction gate**

Audit requirements:

```text
Completeness:
- Field public syntax is accepted by Python and TypeScript compilers.
- Emitted internal body uses kind=field with field_operation and field_scope.
- C++ ActionSemantic has one Field first-stage value for FieldVariableActionCluster.
- ActionContext demand, inference, projection, hash, resolver, debug, and tests all carry operation/scope.
- FieldVariable tests prove get/set/property positive and missing-evidence behavior.

Deviation:
- No Field migration relies on legacy fallback, direct variable spawn shortcut, or local builder-owned semantic-to-cluster projection.
- No unrelated semantic family was migrated in this slice.
- Public AgentFace syntax remains compact and unchanged.

Worker correction:
- If any completeness item is missing, worker fixes before closure.
- If any deviation exists, worker removes it before closure.
- If verification is blocked by environment, worker records exact command, output path, and blocker instead of marking complete.
```

## Final Completion Report Format

At completion, report:

```text
Plan: BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FieldKindConvergence_Plan_20260523_CN.md
Modified files:
- ...

Verification:
- npm run build / test:python / test:node: PASS or exact blocker
- UE focused automation: PASS or exact blocker path
- UE GraphWrite regression: PASS or exact blocker path
- UE 5.6 compile: PASS or exact blocker
- git diff --check: PASS or exact blocker

Suggested commit message:
变更需求：
1. 收敛 GraphWrite Field 语义为 Field + field_operation + field_scope

修复内容：
1. 移除 FieldVariableActionCluster 对 get/set/get_property/set_property 顶层 ActionSemantic 的依赖
```

Do not run git add, git commit, or git push.
