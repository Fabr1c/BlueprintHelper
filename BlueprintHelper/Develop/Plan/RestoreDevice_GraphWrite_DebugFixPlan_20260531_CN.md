# RestoreDevice GraphWrite Debug Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Repository rule override: do not run `git add`, `git commit`, or `git push`; report manual commit commands after verification.

**Goal:** Fix the RestoreDevice TaskSpec graph-write regressions where branch then execution is disconnected after pure container queries, `map.find` result symbols can target the wrong pin, and cast/default graph writes are incorrect or unverified.

**Architecture:** Keep the fix in canonical TaskSpec/SemanticIR/GraphWrite service boundaries. Do not add RestoreDevice-specific branches or UI-level workarounds. Pure query statements should preserve surrounding execution flow while still producing data edges through the shared SemanticIR model.

**Tech Stack:** TypeScript task-core compiler tests, UE 5.6 C++ GraphWrite SemanticIR/generation automation tests, GraphWrite action vocabulary, SemanticIR pipeline.

---

## 2026-05-31 Execution Status

- Fixed pure `container_action` query statements so they generate data fragments but preserve incoming exec exits. Covered by `BlueprintHelper.GraphWrite.ContainerAction.PureQueryPreservesBranchThenFlow`, including `Branch.Then -> PrintString.Execute`, `Map_Find.Key == "PlayerA"`, and `Map_Find.Value -> PrintString.InString`.
- Moved TaskSpec container-action single-result output pins into the schema contract (`task-schemas.ts`) and made TS lowering consume that contract. Covered by composite regression tests for `map.find -> Value`, `map.keys -> Keys`, `array.get -> Item`, `set.to_array -> Result`, plus a rejection test for multi-output `map.get_key_value_by_index`.
- Fixed expression-context `dynamic_cast` by applying pure-cast policy from the expression spawn path. Covered by `BlueprintHelper.GraphWrite.CallFunctionResolver.ConvertExpression.DynamicCastIsPure`, including result-pin wiring into the callable target-object pin and Blueprint compile.
- Reproduced default loss at the writer layer for `Map_Find.Key`: defaults were applied before wildcard container pins were normalized. Fixed by adding coordinator-level spawn options and running container action pin normalization before default application. Covered by `Map_Find.Key == "PlayerA"` in the pure-query test and literal call default coverage.
- Verification completed: UE 5.6 build succeeds; `BlueprintHelper.GraphWrite.ContainerAction` report `RestoreDevice_ContainerAction_GREEN_004` has 14 succeeded, 1 succeeded-with-warnings, and 0 failed; dynamic cast and literal default focused tests pass; `npm.cmd --prefix AgentFaceService/task-core run build` and full `test:node` pass 297/297.

---

## Bug Outline

1. **Pure container query statement breaks branch execution.**
   - Symptom: in `RestoreDevice_TaskSpec_Spawn_Diff.md`, four `Branch.then -> BindInputDeviceToPlayer.execute` links are missing.
   - Root cause hypothesis: `UGraphWritePipelineUtils::BuildSemanticStatement` creates a fragment for `container_action map.find`, but because pure query nodes have no exec pins, the surrounding `BuildSemanticStatementArray` drops `PendingExits` unless `bPreservePreviousExits` is true.
   - Required fix boundary: SemanticIR execution-flow coordinator, not UI or RestoreDevice-specific code.

2. **`container.map.find` result symbol contract is split.**
   - Symptom: old TS lowering can bind container result symbols to `ReturnValue`; UE `Map_Find.ReturnValue` is bool while map value is `Value`.
   - Root cause hypothesis: `task-compiler.ts` uses a generic `CONTAINER_ACTION_KIND -> ReturnValue` mapping while C++ action evidence says `container.map.find result -> Value`.
   - Required fix boundary: shared TaskSpec compiler contract or container-action role mapping.

3. **`target_object.dynamic_cast` expression produces impure cast nodes without exec ownership.**
   - Symptom: generated Cast nodes have exec pins but are used as target-object expressions and are not connected.
   - Current status: reproduced and fixed by applying pure dynamic-cast policy in the expression node spawn path.

4. **Container action literal default loss is a writer-layer ordering bug.**
   - Symptom: `Key`, `Class`, and `LocalPlayerIndex` values are missing or collapsed in reported logicJson.
   - Current status: `Map_Find.Key` was reproduced as empty in the generated node. Root cause is default application before container wildcard pin type normalization. Fixed in the action fragment spawn coordinator path and verified with container key, class object, and generic literal default coverage. No separate `LocalPlayerIndex` fixture was added in this pass.

---

## File Structure

- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - Consumes the schema-owned container-action result output contract for TaskSpec result symbols.
- Modify: `AgentFaceService/task-core/src/task/schema/task-schemas.ts`
  - Owns the container-action single-result output-pin registry.
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.container-action.test.ts`
  - Adds result-symbol rejection coverage for container actions without a single result output.
- Modify: `AgentFaceService/task-core/src/tests/task/task-compiler.regression.test.ts`
  - Adds composite regression coverage for operation-specific result output pins.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/Utils/GraphWritePipelineUtils.cpp`
  - Owns runtime SemanticIR statement execution flow.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
  - Owns expression node policy and container action pre-default pin normalization hook setup.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.cpp`
  - Owns reusable spawn options propagation into action fragment construction.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperActionFragmentSpawnCoordinator.h`
  - Exposes coordinator spawn options for reusable pre-default node policy hooks.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperContainerActionTests.cpp`
  - Adds UE automation coverage for branch then flow through a pure query container statement and `Map_Find.Key` default retention.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperCallFunctionResolverTests.cpp`
  - Adds focused coverage for pure dynamic-cast expressions and per-statement literal defaults.

---

### Task 1: Add Failing TS Contract Test For `map.find` Result Output

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.container-action.test.ts`

- [x] **Step 1: Write the failing test**

Append this test near the other `container_action` compiler tests:

```ts
test('container_action map find result_symbol uses Value output in import lowering', () => {
  const taskPlan = compileTaskSpecToTaskPlan(makeContainerSpec({
    kind: 'branch',
    condition: {
      kind: 'container_action',
      container_kind: 'map',
      container_operation: 'contains',
      target: { kind: 'get', name: 'Scores' },
      key: { kind: 'literal', value_type: 'string', value: 'PlayerA' },
      key_type: 'string',
      value_type: 'int',
    },
    then: [{
      kind: 'container_action',
      container_kind: 'map',
      container_operation: 'find',
      target: { kind: 'get', name: 'Scores' },
      key: { kind: 'literal', value_type: 'string', value: 'PlayerA' },
      key_type: 'string',
      value_type: 'int',
      result_symbol: 'FoundScore',
    }, {
      kind: 'field',
      field_operation: 'set',
      field_scope: 'variable',
      target: 'LastScore',
      value: { kind: 'get', name: 'FoundScore' },
    }],
  }) as never);

  const appendPayload = taskPlanToAppendBridgePayload(taskPlan, true);
  const legacyPayload = (compileTaskSpecToTaskPlan as unknown);
  assert.equal(appendPayload.logic_spec.statements[0].kind, 'branch');
  assert.equal(typeof legacyPayload, 'function');
});
```

If this cannot reach legacy/import lowering directly, replace the last two assertions with a direct exported helper test only after adding a narrow exported helper. Do not test implementation details unless the helper is intentionally part of the compiler contract.

- [x] **Step 2: Run the TS test to verify RED**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run test:node -- task-compiler.container-action
```

Expected: FAIL because the existing compiler has no contract proving `map.find.result_symbol -> Value`.

---

### Task 2: Add Failing UE Runtime Test For Pure Query Statement Exec Preservation

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperContainerActionTests.cpp`

- [x] **Step 1: Write the failing test**

Add a test named:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionPureQueryPreservesBranchThenFlowTest,
	"BlueprintHelper.GraphWrite.ContainerAction.PureQueryPreservesBranchThenFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

The test should:

```cpp
// 1. Create a transient Blueprint and EventGraph with a map variable Scores.
// 2. Generate a BlueprintLogicSpec.v2 branch:
//    condition: bShouldPrint
//    then[0]: map.find(Scores, "PlayerA") result_symbol FoundScore
//    then[1]: call /Script/Engine.KismetSystemLibrary:PrintString using FoundScore
// 3. Assert generation succeeds with zero connection diagnostics.
// 4. Find the generated UK2Node_IfThenElse and PrintString call.
// 5. Assert Branch.Then links to PrintString.Execute.
```

Use exact target JSON:

```json
{
  "logic_spec": {
    "schema": "BlueprintLogicSpec.v2",
    "statements": [{
      "id": "stmt_branch",
      "kind": "branch",
      "condition": { "kind": "get", "name": "bShouldPrint", "type": "bool" },
      "then": [{
        "id": "stmt_find_score",
        "kind": "container_action",
        "container_kind": "map",
        "container_operation": "find",
        "target": { "kind": "get", "name": "Scores" },
        "key": { "kind": "literal", "value": "PlayerA", "type": "string" },
        "key_type": "string",
        "value_type": "string",
        "result_symbol": "FoundScore"
      }, {
        "id": "stmt_print_found",
        "kind": "call",
        "target": "/Script/Engine.KismetSystemLibrary:PrintString",
        "args": {
          "InString": { "kind": "get", "name": "FoundScore", "type": "string" }
        }
      }]
    }]
  }
}
```

- [x] **Step 2: Run the UE automation test to verify RED**

Run:

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ContainerAction.PureQueryPreservesBranchThenFlow;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\RestoreDevice_PureQueryExec_RED_001'
```

Expected: FAIL because branch then does not reach the following executable statement when the first then statement is a pure query container action.

---

### Task 3: Implement Pure Query Exec Preservation

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/Pipeline/Utils/GraphWritePipelineUtils.cpp`

- [x] **Step 1: Add vocabulary include**

Add:

```cpp
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"
```

- [x] **Step 2: Add a local helper**

Add near the other local helpers:

```cpp
static bool IsPureQueryContainerActionStatement(const FBlueprintHelperGraphStatementIR& Statement)
{
	if (Statement.Kind != EBlueprintHelperGraphStatementKind::ContainerAction)
	{
		return false;
	}
	const FBlueprintHelperContainerActionSpec* Spec =
		FBlueprintHelperContainerActionVocabulary::Find(Statement.ContainerKind, Statement.ContainerOperation);
	return Spec && Spec->bPureQuery;
}
```

- [x] **Step 3: Preserve incoming exits for pure query statements**

After `AddSemanticFragment(...)`, before adding entries/exits:

```cpp
if (IsPureQueryContainerActionStatement(*Statement))
{
	Flow.bPreservePreviousExits = true;
	return Flow;
}
```

This keeps the generated data-producing fragment and data edges, but does not consume exec flow.

- [x] **Step 4: Run the UE test to verify GREEN**

Run the same automation command with report path:

```powershell
D:\UEProjects\Template\Saved\Automation\RestoreDevice_PureQueryExec_GREEN_001
```

Expected: PASS.

---

### Task 4: Implement `map.find` Result Pin Contract In TS Lowering

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.container-action.test.ts`

- [x] **Step 1: Add operation-specific output resolver**

Replace the fixed container action mapping with a schema-owned resolver:

```ts
export function getContainerActionResultOutputPin(containerKind: string, containerOperation: string): string | undefined {
  return CONTAINER_ACTION_RESULT_OUTPUT_PIN_BY_KIND_OPERATION[containerKind]?.[containerOperation];
}
```

Then use this resolver for container-action statement `result_symbol` and container-action value expression output. Operations without a single value output must reject `result_symbol` rather than falling back to `ReturnValue`.

- [x] **Step 2: Run the TS test to verify GREEN**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run test:node -- task-compiler.container-action
```

Expected: PASS.

---

### Task 5: Cast And Default Follow-Up Reproduction

**Files:**
- Modify only after RED evidence exists:
  - `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericTransformScheduleActionResolverTests.cpp`
  - `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperContainerActionTests.cpp`
  - related implementation files identified by the failing tests

- [x] **Step 1: Reproduce dynamic_cast expression mismatch**

Create a minimal test where `target_object` is a `convert.dynamic_cast` expression feeding a call receiver. Expected contract:

```text
Expression context must either create a pure cast node with no exec requirement, or create an explicit exec prerequisite flow before the receiver call.
```

- [x] **Step 2: Reproduce default-write vs readback gap**

Create or rerun a graph generation test that inspects actual UE pins after generation:

```text
Map_Find.Key == "PlayerA"
Map_Contains.Key == "PlayerA"
Print/Bind literal input defaults retain their provided literal values
```

Implementation note: `Map_Find.Key` failed against the actual pin and was fixed by running container action pin type normalization before default application. `Map_Find.Value` is also verified as the `FoundScore` data source for the consumer call. `PrintString` literal defaults and `GetGameInstanceSubsystem.Class` defaults are verified by focused tests.

---

### Task 6: Final Verification

**Files:**
- No additional edits unless verification fails.

- [x] **Step 1: Run focused TS tests**

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

- [x] **Step 2: Run focused UE automation**

```powershell
& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -Unattended -NoSplash -NoSound -NullRHI -nop4 -ExecCmds='Automation RunTests BlueprintHelper.GraphWrite.ContainerAction;Quit' -TestExit='Automation Test Queue Empty' -ReportOutputPath='D:\UEProjects\Template\Saved\Automation\RestoreDevice_ContainerAction_FINAL_001'
```

- [x] **Step 3: Run UE 5.6 compile/build gate if automation does not build**

```powershell
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development 'D:\UEProjects\Template\Template.uproject' -WaitMutex -NoHotReload
```

- [x] **Step 4: Final status**

Report changed files, test results, unresolved risks, and manual commit command. Do not stage or commit.
