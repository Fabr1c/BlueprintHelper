# GraphWrite BUG-001 BUG-002 Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复四簇 E2E smoke 中的两个 P1 blocker：Field component property path owner evidence 丢失，以及 EventDelegate public TaskSpec kind 未接入 TS compiler lowering。

**Architecture:** 本计划只扩展现有边界：BUG-001 落在 ActionContext demand/inference 边界，BUG-002 落在 AgentFace TS TaskSpec compiler lowering 边界。不得放宽 FieldVariableActionResolver 的 owner evidence 要求，不得让 C++ GraphSemanticIR parser 直接接受 `delegate.bind` 等 public dotted kind，不得把 Python compiler retire 混入本修复。

**Tech Stack:** UE 5.6 C++ automation tests, BlueprintHelper GraphWrite SemanticIR/ActionContext, TypeScript task-core compiler, Node test runner, BlueprintHelper CLI four-cluster smoke.

---

## Scope

In scope:

- BUG-001: `DoorMesh.RelativeRotation.Roll` 在 Field `property_path` statement 中需要保留 owner root `DoorMesh`，并投射 `field_owner_class` / component evidence。
- BUG-002: TS compiler 接受 Agent-facing `component_bound_event` / `delegate.*` public kind，并 lowering 到 internal canonical shape。
- Targeted tests 和四簇 smoke rerun。

Out of scope:

- Python compiler retire。
- Event `custom_event` / `override` / `native` declaration taxonomy 重构。
- C++ parser 直接支持 dotted public delegate kind。
- 非插件问题写入 bug 文档。

## File Structure

- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`
  - Responsibility: Field `property_path` statement demand target 选择 owner root，而不是完整 owner.member path。
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`
  - Responsibility: 覆盖 BUG-001 demand target 和 owner evidence 投射。
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - Responsibility: TS compiler public EventDelegate statement validation/lowering。
- Create: `AgentFaceService/task-core/src/task/compiler/task-compiler.event-delegate.test.ts`
  - Responsibility: 覆盖 BUG-002 public-to-internal lowering 和 forbidden internal shape。
- Modify after verification: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_FourClusterE2ESmoke_Bugs_20260524_CN.md`
  - Responsibility: 修复后更新 BUG-001/002 状态和验证证据。
- Modify after verification: `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/SmokeRecord_20260524_CN.md`
  - Responsibility: 写入 rerun 结果，不记录非插件 harness 事项为 bug。

## Task 1: BUG-001 Failing ActionContext Test

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp`

- [ ] **Step 1: Add a failing regression test after `FBlueprintHelperActionContextSingleDemandSetPropertyMapsToFieldVariableTest`**

Add this test block:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextPropertyPathStatementKeepsOwnerRootTargetTest,
	"BlueprintHelper.GraphWrite.ActionContext.FieldPropertyPath.KeepsOwnerRootTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextPropertyPathStatementKeepsOwnerRootTargetTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBlueprintHelperGraphStatementIR> Statement = MakeShared<FBlueprintHelperGraphStatementIR>();
	Statement->StatementId = TEXT("stmt_set_roll");
	Statement->Path = TEXT("$.statements[0]");
	Statement->Kind = EBlueprintHelperGraphStatementKind::Field;
	Statement->Target = TEXT("DoorMesh");
	Statement->Property = TEXT("RelativeRotation.Roll");
	Statement->FieldOperation = TEXT("set");
	Statement->FieldScope = TEXT("property_path");
	Statement->ResolvedTarget.Raw = TEXT("DoorMesh.RelativeRotation.Roll");
	Statement->ResolvedTarget.Owner = TEXT("DoorMesh");
	Statement->ResolvedTarget.PropertyPath = TEXT("RelativeRotation.Roll");
	Statement->ResolvedTarget.Type = TEXT("Rotator");

	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(Statement);
	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);

	TestEqual(TEXT("one field demand"), Demands.Num(), 1);
	if (Demands.Num() == 0)
	{
		return false;
	}

	TestEqual(TEXT("semantic"), Demands[0].SemanticKind, EBlueprintHelperActionSemanticKind::Field);
	TestEqual(TEXT("cluster"), Demands[0].ClusterKind, EBlueprintHelperSpawnerClusterKind::FieldVariableAction);
	TestEqual(TEXT("field operation"), Demands[0].FieldOperation, FString(TEXT("set")));
	TestEqual(TEXT("field scope"), Demands[0].FieldScope, FString(TEXT("property_path")));
	TestEqual(TEXT("target keeps owner root"), Demands[0].TargetPath, FString(TEXT("DoorMesh")));
	TestEqual(TEXT("property path keeps member path"), Demands[0].PropertyPath, FString(TEXT("RelativeRotation.Roll")));
	TestEqual(TEXT("query stays property path"), Demands[0].Query, FString(TEXT("RelativeRotation.Roll")));

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	FBlueprintHelperActionContextFieldSnapshot ComponentField;
	ComponentField.Name = TEXT("DoorMesh");
	ComponentField.OwnerClassPath = TEXT("/Game/Test/BP_Door.BP_Door_C");
	ComponentField.FieldPath = TEXT("/Game/Test/BP_Door.BP_Door_C.DoorMesh");
	ComponentField.PinCategory = TEXT("object");
	ComponentField.PinSubCategory = TEXT("StaticMeshComponent");
	ComponentField.bComponent = true;
	Snapshot.Fields.Add(ComponentField);

	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demands[0]);

	TestEqual(TEXT("field_name evidence resolves owner root"), Context.Evidence.FindRef(TEXT("field_name")), FString(TEXT("DoorMesh")));
	TestEqual(TEXT("field owner evidence projected"), Context.Evidence.FindRef(TEXT("field_owner_class")), FString(TEXT("/Game/Test/BP_Door.BP_Door_C")));
	TestEqual(TEXT("component property evidence projected"), Context.Evidence.FindRef(TEXT("component_property_name")), FString(TEXT("DoorMesh")));
	TestEqual(TEXT("semantic target root"), Context.Semantic.TargetPath, FString(TEXT("DoorMesh")));
	TestEqual(TEXT("semantic property path"), Context.Semantic.PropertyPath, FString(TEXT("RelativeRotation.Roll")));
	return true;
}
```

- [ ] **Step 2: Run the targeted automation test and verify it fails before implementation**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -run=Automation -Test='BlueprintHelper.GraphWrite.ActionContext.FieldPropertyPath.KeepsOwnerRootTarget' -unattended -nop4 -nosplash -NullRHI"
```

Expected before implementation:

```text
FAILED
target keeps owner root: expected DoorMesh, actual DoorMesh.RelativeRotation.Roll
```

If the local command format differs from the current automation wrapper, use the existing GraphWrite automation command from the smoke record and filter to `BlueprintHelper.GraphWrite.ActionContext.FieldPropertyPath.KeepsOwnerRootTarget`.

## Task 2: BUG-001 Minimal DemandCollector Fix

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp`

- [ ] **Step 1: Add a helper near `BuildStatementQuery`**

Insert this helper after `BuildStatementQuery`:

```cpp
static FString BuildStatementTargetPath(const FBlueprintHelperGraphStatementIR& Statement, const EBlueprintHelperActionSemanticKind SemanticKind)
{
	if (SemanticKind == EBlueprintHelperActionSemanticKind::Field
		&& Statement.FieldScope.Equals(TEXT("property_path"), ESearchCase::IgnoreCase))
	{
		return FirstNonEmpty(
			Statement.ResolvedTarget.Owner,
			Statement.Target,
			Statement.Name);
	}

	return FirstNonEmpty(
		Statement.ResolvedTarget.Raw,
		Statement.Target,
		Statement.Name);
}
```

- [ ] **Step 2: Route statement demand target through the helper**

Replace this block in `AppendDemandForStatement`:

```cpp
		BlueprintHelperActionContextDemandCollector::FirstNonEmpty(
			Statement.ResolvedTarget.Raw,
			Statement.Target,
			Statement.Name),
```

with:

```cpp
		BlueprintHelperActionContextDemandCollector::BuildStatementTargetPath(Statement, SemanticKind),
```

- [ ] **Step 3: Keep the resolver strict**

Do not change this guard in `BlueprintHelperFieldVariableActionResolver.cpp`:

```cpp
if (!bComponentRefSemantic && !bFieldAccessSemantic && ResolvedPath.OwnerClassPath.IsEmpty())
```

This guard is the correct safety boundary. The fix is to project owner evidence earlier, not to bypass the requirement.

- [ ] **Step 4: Run the targeted automation test**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -run=Automation -Test='BlueprintHelper.GraphWrite.ActionContext.FieldPropertyPath.KeepsOwnerRootTarget' -unattended -nop4 -nosplash -NullRHI"
```

Expected:

```text
PASSED
```

## Task 3: BUG-002 Failing TS Compiler Tests

**Files:**
- Create: `AgentFaceService/task-core/src/task/compiler/task-compiler.event-delegate.test.ts`

- [ ] **Step 1: Create the test file**

Create the file with:

```ts
import { strict as assert } from 'node:assert';
import test from 'node:test';

import { TaskSpecCompileError, compileTaskSpecToTaskPlan } from './task-compiler.js';

function makeDelegateSpec(statements: Array<Record<string, unknown>>) {
  return {
    schema: 'BlueprintHelper.TaskSpec.v1',
    context_id: 'ctx_event_delegate_ts',
    task_type: 'edit_blueprint_graph',
    feature_name: 'EventDelegateFeatureTs',
    target: {
      asset_path: '/Game/BP/BP_Door',
      target_type: 'blueprint',
    },
    scope_policy: {
      graph_name: 'EG_EventDelegateFeatureTs',
      allow_modify_user_nodes: false,
    },
    behavior: {
      graph_strategy: 'append_new_owned_graph',
      entries: [{
        entry_type: 'custom_event',
        name: 'ApplyDelegates',
        body: {
          schema: 'BlueprintLogicSpec.v1',
          statements,
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

function compileStatements(statements: Array<Record<string, unknown>>) {
  const taskPlan = compileTaskSpecToTaskPlan(makeDelegateSpec(statements) as never);
  const graphWriteStep = taskPlan.steps.find((step) => (step as Record<string, unknown>).capability === 'graph_write') as Record<string, unknown> | undefined;
  assert.ok(graphWriteStep);
  const write = graphWriteStep.write as { ops: Array<{ body: { statements: Record<string, unknown>[] } }> };
  return write.ops[0].body.statements;
}

test('component_bound_event public statement is preserved as canonical internal component_bound_event', () => {
  const [statement] = compileStatements([{
    kind: 'component_bound_event',
    component: 'TriggerBox',
    delegate: 'OnComponentBeginOverlap',
    handler: 'BH_HandleSmokeOverlap',
  }]);

  assert.equal(statement.kind, 'component_bound_event');
  assert.equal(statement.component, 'TriggerBox');
  assert.equal(statement.delegate, 'OnComponentBeginOverlap');
  assert.equal(statement.handler, 'BH_HandleSmokeOverlap');
  assert.equal(Object.hasOwn(statement, 'delegate_operation'), false);
});

test('delegate.bind public statement lowers to canonical delegate bind operation', () => {
  const [statement] = compileStatements([{
    kind: 'delegate.bind',
    target: 'TriggerBox',
    delegate: 'OnComponentBeginOverlap',
    handler: 'BH_HandleSmokeOverlap',
  }]);

  assert.equal(statement.kind, 'delegate');
  assert.equal(statement.delegate_operation, 'bind');
  assert.equal(statement.target, 'TriggerBox');
  assert.equal(statement.delegate, 'OnComponentBeginOverlap');
  assert.equal(statement.handler, 'BH_HandleSmokeOverlap');
});

test('delegate.unbind_all public statement lowers to canonical delegate clear operation', () => {
  const [statement] = compileStatements([{
    kind: 'delegate.unbind_all',
    target: 'TriggerBox',
    delegate: 'OnComponentBeginOverlap',
  }]);

  assert.equal(statement.kind, 'delegate');
  assert.equal(statement.delegate_operation, 'clear');
  assert.equal(statement.unbind_mode, 'all');
  assert.equal(statement.target, 'TriggerBox');
  assert.equal(statement.delegate, 'OnComponentBeginOverlap');
});

test('Agent-authored internal delegate statement is rejected before lowering', () => {
  assert.throws(
    () => compileStatements([{
      kind: 'delegate',
      delegate_operation: 'bind',
      target: 'TriggerBox',
      delegate: 'OnComponentBeginOverlap',
      handler: 'BH_HandleSmokeOverlap',
    }]),
    (err: unknown) => err instanceof TaskSpecCompileError
      && err.code === 'unsupported_statement_kind'
      && err.issues.some((issue) => issue.message.includes('Use component_bound_event or delegate.bind')),
  );
});
```

- [ ] **Step 2: Run the new test and verify it fails before implementation**

Run:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run build
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core\build\task\compiler\task-compiler.event-delegate.test.js
```

Expected before implementation:

```text
TaskSpecCompileError: Unsupported GraphWrite statement kind.
```

The first failing test should be `component_bound_event public statement is preserved as canonical internal component_bound_event`.

## Task 4: BUG-002 TS Compiler Lowering

**Files:**
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`

- [ ] **Step 1: Add EventDelegate constants near the GraphWrite kind constants**

Add this block immediately before `SUPPORTED_GRAPH_BODY_STATEMENT_KINDS`:

```ts
const PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES = new Map([
  ['component_bound_event', 'component_bound_event'],
  ['delegate.bind', 'bind'],
  ['delegate.assign', 'assign'],
  ['delegate.unbind', 'unbind'],
  ['delegate.unbind_all', 'clear'],
  ['delegate.call', 'call'],
]);
const INTERNAL_DELEGATE_STATEMENT_KIND = 'delegate';
const DELEGATE_STATEMENT_OPERATION_KINDS = new Set(['bind', 'assign', 'unbind', 'clear', 'call']);
const FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS = new Set([
  'delegate',
  'bind',
  'assign',
  'unbind',
  'unbind_all',
  'delegate_call',
  'delegate_clear',
]);
```

Change `SUPPORTED_GRAPH_BODY_STATEMENT_KINDS` to:

```ts
const SUPPORTED_GRAPH_BODY_STATEMENT_KINDS = new Set([
  'call',
  'field',
  'set',
  'set_property',
  'let',
  'control',
  'create',
  'convert',
  'schedule',
  ...PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES.keys(),
]);
```

- [ ] **Step 2: Add helper functions near `fieldOperationScope`**

Add:

```ts
function delegateStatementOperation(statement: Record<string, unknown>): string | undefined {
  const kind = typeof statement.kind === 'string' ? statement.kind : '';
  if (kind === INTERNAL_DELEGATE_STATEMENT_KIND) {
    const operation = typeof statement.delegate_operation === 'string' ? statement.delegate_operation : '';
    return DELEGATE_STATEMENT_OPERATION_KINDS.has(operation) ? operation : undefined;
  }
  const operation = PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES.get(kind);
  return operation && operation !== 'component_bound_event' ? operation : undefined;
}

function validateDelegateStatementShape(statement: Record<string, unknown>, path: string): void {
  const kind = typeof statement.kind === 'string' ? statement.kind : '';
  if (kind === 'component_bound_event') {
    getRequiredString(statement, 'component', `${path}.component`);
    getRequiredString(statement, 'delegate', `${path}.delegate`);
    getRequiredString(statement, 'handler', `${path}.handler`);
    return;
  }

  const operation = delegateStatementOperation(statement);
  if (!operation) {
    return;
  }

  getRequiredString(statement, 'target', `${path}.target`);
  getRequiredString(statement, 'delegate', `${path}.delegate`);
  if (operation === 'bind' || operation === 'assign' || operation === 'unbind') {
    getRequiredString(statement, 'handler', `${path}.handler`);
  }
  if (operation === 'call') {
    validateExpressionMap(statement.args, `${path}.args`);
  }
}
```

- [ ] **Step 3: Harden `validateSupportedStatements`**

At the start of the loop, before the generic unsupported-kind branch, add:

```ts
    if (FORBIDDEN_AGENT_DELEGATE_INTERNAL_KINDS.has(kind)) {
      throw new TaskSpecCompileError('unsupported_statement_kind', 'Unsupported GraphWrite statement kind.', [
        {
          code: 'unsupported_statement_kind',
          path: `${statementPath}.kind`,
          message: 'Use component_bound_event or delegate.bind/delegate.assign/delegate.unbind/delegate.unbind_all/delegate.call in Agent-facing TaskSpec. The compiler owns kind=delegate + delegate_operation lowering.',
        },
      ]);
    }
```

Then add this branch after the generic supported-kind check and before `kind === 'call'`:

```ts
    if (PUBLIC_DELEGATE_STATEMENT_KIND_ALIASES.has(kind)) {
      validateDelegateStatementShape(statementRecord, statementPath);
    } else if (kind === 'call') {
```

Adjust the existing `if (kind === 'call')` to `else if (kind === 'call')`.

- [ ] **Step 4: Lower public delegate statements in `cloneLogicStatementWithCompiledIds`**

Add this local after `kind` is computed:

```ts
  const delegateOperation = delegateStatementOperation(statementRecord);
```

Then add this branch after the `field` branch and before `let`:

```ts
  } else if (kind === 'component_bound_event') {
    out.kind = 'component_bound_event';
  } else if (delegateOperation) {
    out.kind = 'delegate';
    out.delegate_operation = delegateOperation;
    if (delegateOperation === 'unbind') {
      out.unbind_mode = 'single';
    } else if (delegateOperation === 'clear') {
      out.unbind_mode = 'all';
    } else if (delegateOperation === 'call' && isRecord(statementRecord.args)) {
      out.args = Object.fromEntries(
        Object.entries(statementRecord.args).map(([argName, argValue]) => [
          argName,
          cloneLogicExpressionWithCompiledIds(argValue, `${statementId}_arg_${toIdSegment(argName)}`),
        ]),
      );
    }
```

Keep the later `let` / `call` / `control` branches intact.

- [ ] **Step 5: Lower delegate statements in append-payload node compilation**

In `compileStatementFlow`, update the argument-compilation condition:

```ts
  const delegateOperation = delegateStatementOperation(statementRecord);
  if (kind === 'call' || kind === 'create' || kind === 'convert' || kind === 'schedule' || delegateOperation === 'call') {
```

In `compileStatementNode`, add delegate cases before the final unsupported-kind throw:

```ts
  if (kind === 'component_bound_event') {
    return omitUndefined({
      id: nodeId,
      kind: 'component_bound_event',
      component: getRequiredString(statementRecord, 'component', `${path}.component`),
      delegate: getRequiredString(statementRecord, 'delegate', `${path}.delegate`),
      handler: getRequiredString(statementRecord, 'handler', `${path}.handler`),
    }) as AgentImportNode;
  }

  const delegateOperation = delegateStatementOperation(statementRecord);
  if (delegateOperation) {
    return omitUndefined({
      id: nodeId,
      kind: 'delegate',
      target: getRequiredString(statementRecord, 'target', `${path}.target`),
      delegate: getRequiredString(statementRecord, 'delegate', `${path}.delegate`),
      handler: typeof statementRecord.handler === 'string' ? statementRecord.handler : undefined,
      delegate_operation: delegateOperation,
      unbind_mode: delegateOperation === 'unbind' ? 'single' : (delegateOperation === 'clear' ? 'all' : undefined),
      inputs: delegateOperation === 'call' ? compileArgs(statementRecord.args) : undefined,
    }) as AgentImportNode;
  }
```

- [ ] **Step 6: Run TS tests**

Run:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run build
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core\build\task\compiler\task-compiler.event-delegate.test.js
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run test:node
```

Expected:

```text
no thrown TaskSpecCompileError for component_bound_event or delegate.bind
all node tests pass
```

## Task 5: Targeted CLI Repro Rerun

**Files:**
- Read: `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/02_function_field_graph.json`
- Read: `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/03_event_delegate_graph.json`
- Write result artifacts through CLI: `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/Results/`

- [ ] **Step 1: Rebuild task-core and plugin if needed**

Run:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run build
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex
```

Expected:

```text
Result: Succeeded
```

- [ ] **Step 2: Preview BUG-001 fixture**

Run:

```powershell
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli\build\cli\index.js task preview --file D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\02_function_field_graph.json --format full
```

Expected:

```text
status=preview_passed
```

If a later Field issue appears after owner evidence is fixed, keep BUG-001 open and update the same bug entry with the new exact blocker. Do not close BUG-001 on a partial pass.

- [ ] **Step 3: Preview BUG-002 fixture**

Run:

```powershell
node D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\cli\build\cli\index.js task preview --file D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\03_event_delegate_graph.json --format full
```

Expected:

```text
status=preview_passed
```

If preview reaches Bridge but fails on EventDelegate semantic evidence, keep BUG-002 open and replace the owner area with the new failing layer. Do not reclassify compiler lowering as fixed until the public TaskSpec reaches Bridge preview.

## Task 6: Four-Cluster Smoke And Docs

**Files:**
- Modify: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_FourClusterE2ESmoke_Bugs_20260524_CN.md`
- Modify: `BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/SmokeRecord_20260524_CN.md`
- Modify if status changes: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md`
- Modify if gap closes: `BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md`

- [ ] **Step 1: Run the existing four-cluster smoke script**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\run_four_cluster_smoke.ps1"
```

Expected:

```text
02_function_field_graph.json preview_passed and executed
03_event_delegate_graph.json preview_passed and executed
04_generic_graph.json preview_passed and executed
05_generic_expected_diagnostics.json preview_blocked with accepted type_promotion diagnostic
```

- [ ] **Step 2: Run GraphWrite automation**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& 'E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UEProjects\Template\Template.uproject' -run=Automation -Test='BlueprintHelper.GraphWrite' -unattended -nop4 -nosplash -NullRHI"
```

Expected:

```text
failed=0
```

- [ ] **Step 3: Update docs with evidence only**

Use these status rules:

```text
If both targeted previews and full smoke pass:
  BUG-001 Status: FIXED
  BUG-002 Status: FIXED
  SmokeRecord Status: PASS
  CompletionStatus Field/EventDelegate rows: PASS with artifact paths
  Gap audit Field/Event smoke blocker: CLOSED or narrowed to any new blocker

If a new plugin blocker appears:
  Keep the existing bug open if same root path.
  Add a new bug only if the failure is a different plugin implementation defect.

If an editor lifecycle, local ExecutionPolicy, missing editor, or command invocation issue appears:
  Record it only under SmokeRecord non-bug harness notes.
  Do not add it to the bug document.
```

## Final Verification Commands

Run:

```powershell
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run build
npm.cmd --prefix D:\UEProjects\Template\Plugins\BlueprintHelper\AgentFaceService\task-core run test:node
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\UEProjects\Template\Plugins\BlueprintHelper\BlueprintHelper\Develop\PlanArtifacts\GraphWriteFourClusterE2ESmoke_20260524\run_four_cluster_smoke.ps1"
& 'E:\UE_5.6\Engine\Build\BatchFiles\Build.bat' TemplateEditor Win64 Development -Project='D:\UEProjects\Template\Template.uproject' -WaitMutex
```

Expected:

```text
task-core build passes
node tests pass
four-cluster smoke has no unexpected positive-path blocker
UE build Result: Succeeded
```

## Manual Commit Guidance

Workers must not run `git add`, `git commit`, or `git push` in this repository. After verification, report changed files and suggest this manual commit message:

```text
修复内容：
1. 修复 GraphWrite Field property_path statement owner evidence 投射，恢复 Function+Field E2E 预览。
2. 接入 TS TaskSpec compiler 的 EventDelegate public kind lowering，恢复 EventDelegate E2E 预览。
```

Suggested manual commands for the user only:

```powershell
git add BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.cpp `
        BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionContextPipelineTests.cpp `
        AgentFaceService/task-core/src/task/compiler/task-compiler.ts `
        AgentFaceService/task-core/src/task/compiler/task-compiler.event-delegate.test.ts `
        BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_FourClusterE2ESmoke_Bugs_20260524_CN.md `
        BlueprintHelper/Develop/PlanArtifacts/GraphWriteFourClusterE2ESmoke_20260524/SmokeRecord_20260524_CN.md `
        BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_FourClusterCompletionStatus_20260522_CN.md `
        BlueprintHelper/Develop/Gap/BlueprintHelper_GraphWrite_ArchitectureGaps_Audit_20260522_CN.md
git commit -m "fix: restore GraphWrite field and delegate smoke paths"
```

## Self-Review

- Spec coverage: BUG-001 has failing test, minimal C++ implementation, targeted automation, CLI preview, smoke rerun. BUG-002 has failing TS tests, compiler lowering implementation, targeted CLI preview, smoke rerun.
- Placeholder scan: no placeholder task text is required for execution; every code change step includes concrete code.
- Type consistency: `field_operation`, `field_scope`, `delegate_operation`, `unbind_mode`, `component_bound_event`, and `delegate.bind` names match the current schema/contract names.
