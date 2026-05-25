# GraphWrite Generic Schedule Success Path Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 `timer_delegate_node` 和 `latent_or_async_node` 成为 GraphWrite Generic schedule 的真实成功路径，同时保持 handler / signature 创建归属在 `BlueprintSignature` 或 ActionContext projection 边界内。

**Architecture:** `GraphWrite` 只消费已投影的 schedule spawner evidence、graph latent permission 和 handler/signature evidence，负责创建 schedule use-site node、必要的 delegate link 和 graph_block 级 Review evidence。`BlueprintSignature` 负责 handler/signature declaration；`FunctionActionCluster` 继续负责普通 `schedule_function` / `latent_or_async_function` callable。Generic schedule 不走 FunctionAction fallback，不按 `UK2Node_*` 硬编码伪造成功，不在 resolver 内扫描或创建 handler。

**Tech Stack:** TypeScript TaskSpec compiler/contract, UE 5.6 C++ GraphWrite ActionContext / ActionResolution / FragmentBuilder, Unreal Automation tests, BlueprintHelper CLI smoke.

---

## 2026-05-25 Execution Status

Status: IMPLEMENTED / FOCUSED GATES PASS.

Implemented scope:

| Area | Result |
|---|---|
| TaskSpec contract/compiler | `generic_schedule` contract added; `timer_delegate_node` / `latent_or_async_node` require projected evidence; public schedule statements/expressions reject `function_operation` owner mixing. |
| Schedule evidence/resolver | `FBlueprintHelperProjectedScheduleActionEvidence` added; `timer_delegate_node` and `latent_or_async_node` revalidate selected current ActionDatabase spawners through `FBlueprintHelperActionDatabaseProjectionService`; missing/false evidence returns deterministic errors. |
| Fragment/readback | `FBlueprintHelperDelegateLinkFragmentUtils` owns CreateDelegate spawn/link readback; EventDelegate and Generic timer schedule reuse it; latent/async schedule spawns through the shared action fragment adapter. |
| Boundary preservation | GraphWrite consumes BlueprintSignature/ActionContext handler evidence only; it does not create handler functions, custom events, dispatchers, or signatures. |

Focused evidence collected during implementation:

```text
npm.cmd --prefix AgentFaceService/task-core run test:node -- graphwrite-capability-contract convert-schedule
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReloadFromIDE
Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule
Automation RunTests BlueprintHelper.GraphWrite.GenericSchedule
rg -n "function_operation.*timer_delegate_node|function_operation.*latent_or_async_node|timer_delegate_node.*function_operation|latent_or_async_node.*function_operation" AgentFaceService/task-core/src BlueprintHelper/Source/BlueprintHelper
rg -n "UBlueprintNodeSpawner::Create\(" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp
```

The full verification suite still needs to be run after final edits: full TS test suite, full `BlueprintHelper.GraphWrite` automation suite, `git diff --check`, and final read-only review.

---

## Decision Record

| Topic | Decision |
|---|---|
| `timer_delegate_node` ownership | GraphWrite Generic schedule owns the timer/delegate schedule node and the link to an existing handler. Handler declaration/signature must already exist or come from a `blueprint_signature` dependency step. |
| `latent_or_async_node` ownership | GraphWrite Generic schedule owns ActionDatabase-selected latent/async node creation only when `graph_latent_allowed=true` and selected spawner evidence is complete. It does not create callback handler/signature declarations. |
| Editor auto handler behavior | Treat UE editor auto-create behavior as UI convenience, not GraphWrite ownership. TaskPlan must split it into a `blueprint_signature` dependency plus a GraphWrite body/use-site step. |
| Review policy | Keep graph-level Review evidence: one `graph_block` / `graph_surface_atomic_target` for the GraphWrite step. No per-schedule atomic target in this plan. |
| Public shape | Generic schedule uses `kind="schedule"` plus `schedule_operation="timer_delegate_node"` or `"latent_or_async_node"` and no `function_operation`. Function-owned schedule uses `function_operation="schedule_function"` or `"latent_or_async_function"` and must not carry a Generic `schedule_operation`. |

## Ownership Boundary

| Capability | Owner | GraphWrite behavior when missing |
|---|---|---|
| Custom event / function handler declaration | `blueprint_signature` | Return deterministic diagnostic `handler_evidence_missing`; do not create the handler. |
| Handler function path and signature evidence id | ActionContext projection from Signature step | Return deterministic diagnostic `handler_evidence_missing` or `signature_evidence_id_missing`. |
| Timer delegate schedule node | GraphWrite Generic schedule | Resolve through ActionDatabase projected spawner evidence, spawn through shared adapter, then link existing handler via reusable delegate-link helper. |
| Latent / async schedule node | GraphWrite Generic schedule | Resolve through ActionDatabase projected spawner evidence, require `graph_latent_allowed=true`, spawn through shared adapter. |
| Ordinary Kismet latent/timer callable | `FunctionActionCluster` | Use existing FunctionAction resolver path, not Generic schedule. |
| Delegate bind / assign / unbind / call | `EventDelegateActionCluster` | Remains separate from Generic schedule. |

## Evidence Contract

`context_evidence` keys for Generic schedule:

| Key | Required for timer | Required for latent/async | Meaning |
|---|---:|---:|---|
| `schedule_action_stable_id` | yes | yes | Stable identity of the selected current ActionDatabase spawner. |
| `schedule_node_class` | yes | yes | Node class path from current ActionDatabase candidate. |
| `schedule_spawner_signature` | yes | yes | `UBlueprintNodeSpawner::GetSpawnerSignature().ToString()`. |
| `schedule_owner_path` | yes | yes | ActionDatabase owner path. |
| `schedule_query` | optional | optional | Human query used only to narrow candidates before projected identity is complete. |
| `schedule_menu_name` | optional | optional | Menu name for stronger evidence matching. |
| `schedule_category` | optional | optional | Category for stronger evidence matching. |
| `schedule_delegate_pin_name` | optional | no | Explicit delegate input pin to link for timer delegate nodes. If absent, the linker must find exactly one compatible delegate input pin. |
| `handler_name` | yes | no | Existing handler name from Signature/ActionContext. |
| `handler_function_path` | yes | no | Existing handler function path from Signature/ActionContext. |
| `handler_source_cluster` | yes | no | Must be `BlueprintSignature` or another explicit non-GraphWrite source. |
| `signature_evidence_id` | yes | no | Stable evidence id for the pre-existing handler/signature. |
| `graph_latent_allowed` | no | yes | Must be `true`, `1`, or `yes` for latent/async success. |

Failure codes are part of the contract:

| Condition | Error code |
|---|---|
| Generic schedule has no `schedule_operation` | `needs_more_semantic_context` |
| `function_operation` is present with Generic `schedule_operation` | `ambiguous_convert_schedule_owner` in C++; `unsupported_schedule_owner_mix` in TaskSpec compiler validation |
| Schedule selected spawner evidence is incomplete | `schedule_spawner_evidence_missing` |
| Current ActionDatabase has no matching candidate | `schedule_spawner_not_found` |
| Current ActionDatabase has multiple matching candidates | `schedule_spawner_ambiguous` |
| Timer delegate lacks handler/signature evidence | `handler_evidence_missing` or `signature_evidence_id_missing` |
| Latent/async graph permission is false | `latent_function_not_allowed_in_graph` |
| Timer delegate linker cannot find exactly one delegate input pin | `timer_delegate_pin_ambiguous` or `timer_delegate_pin_missing` |

## File Structure

- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
  - Add `generic_schedule` contract entry and required evidence keys.
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`
  - Assert Generic schedule operations, evidence keys, review policy, and no FunctionAction ownership mixing.
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
  - Reject public GraphWrite schedule statements/expressions that combine `function_operation` with `timer_delegate_node` or `latent_or_async_node`.
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.convert-schedule.test.ts`
  - Add passing Generic schedule examples without `function_operation` and failing mixed-owner examples.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h`
  - Add `FBlueprintHelperProjectedScheduleActionEvidence` and read/write helpers.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.cpp`
  - Implement schedule evidence parsing and stable id helper.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h`
  - Neutral ActionDatabase projection service shared by `asset_action` and Generic schedule.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.cpp`
  - Implements ActionDatabase scanning, ActionFilter use, projected identity matching, and candidate metadata.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.cpp`
  - Keep asset_action behavior but delegate common candidate projection to the neutral service.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp`
  - Resolve `timer_delegate_node` and `latent_or_async_node` through the neutral projection service.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperDelegateLinkFragmentUtils.h`
  - Reusable create-delegate spawning and delegate-pin linking helper.
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperDelegateLinkFragmentUtils.cpp`
  - Moves the reusable parts currently embedded in `BlueprintHelperEventDelegateFragmentBuilder.cpp`.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp`
  - Use `BlueprintHelperDelegateLinkFragmentUtils` without behavior change.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
  - After Generic timer schedule spawn, create/link the delegate node to the existing handler evidence.
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericTransformScheduleActionResolverTests.cpp`
  - Replace no-success expectations with evidence-gated success and deterministic missing-evidence failures.
- Create or modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericScheduleFragmentTests.cpp`
  - Focused fragment/readback tests for timer delegate link and latent/async graph permission.
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`
  - Mark Generic schedule success path as plan-ready until implementation verifies it.
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md`
  - Keep `schedule.timer_delegate_node` and `schedule.latent_or_async_node` in the ownership-filtered final matrix, gated by this plan.
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`
  - Sync ownership decision and success-path criteria.

## Task 1: Public Contract And Compiler Ownership Guard

**Files:**
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.ts`
- Modify: `AgentFaceService/task-core/src/task/schema/graphwrite-capability-contract.test.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.ts`
- Modify: `AgentFaceService/task-core/src/task/compiler/task-compiler.convert-schedule.test.ts`

- [ ] **Step 1: Add failing contract tests for Generic schedule**

Add this test to `graphwrite-capability-contract.test.ts`:

```ts
it('pins Generic schedule to projected spawner evidence and graph-level review evidence', () => {
  const genericSchedule = GRAPHWRITE_CAPABILITY_CONTRACT.clusters.find((cluster) => cluster.id === 'generic_schedule');

  assert.ok(genericSchedule);
  assert.equal(genericSchedule.evidence.projectionSource, 'UE ActionDatabase');
  assert.equal(genericSchedule.executeRevalidation, 'required');
  assert.deepEqual(genericSchedule.evidence.requiredKeys, [
    'schedule_action_stable_id',
    'schedule_node_class',
    'schedule_spawner_signature',
    'schedule_owner_path',
  ]);
  assert.equal(
    genericSchedule.operations.find((operation) => operation.id === 'schedule.timer_delegate_node')?.reviewEvidence,
    'graph_surface_atomic_target',
  );
  assert.equal(
    genericSchedule.operations.find((operation) => operation.id === 'schedule.latent_or_async_node')?.reviewEvidence,
    'graph_surface_atomic_target',
  );
});
```

- [ ] **Step 2: Run the contract test and verify it fails**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run test:node -- graphwrite-capability-contract
```

Expected before implementation: fail with a missing `generic_schedule` cluster.

- [ ] **Step 3: Add the Generic schedule contract**

In `graphwrite-capability-contract.ts`, extend the cluster id union:

```ts
export interface GraphWriteClusterContract {
  readonly id: 'function_action' | 'field' | 'event' | 'asset_action' | 'generic_schedule';
```

Add constants:

```ts
const GENERIC_SCHEDULE_REQUIRED_EVIDENCE_KEYS = [
  'schedule_action_stable_id',
  'schedule_node_class',
  'schedule_spawner_signature',
  'schedule_owner_path',
] as const;

const TIMER_DELEGATE_REQUIRED_EVIDENCE_KEYS = [
  ...GENERIC_SCHEDULE_REQUIRED_EVIDENCE_KEYS,
  'handler_name',
  'handler_function_path',
  'handler_source_cluster',
  'signature_evidence_id',
] as const;

const LATENT_OR_ASYNC_REQUIRED_EVIDENCE_KEYS = [
  ...GENERIC_SCHEDULE_REQUIRED_EVIDENCE_KEYS,
  'graph_latent_allowed',
] as const;
```

Add this cluster after `asset_action`:

```ts
{
  id: 'generic_schedule',
  responsibility: 'GraphWrite owns Generic schedule use-site nodes only when selected ActionDatabase spawner evidence and external handler/signature evidence are projected.',
  operations: [
    {
      id: 'schedule.timer_delegate_node',
      kind: 'schedule',
      supportStatus: 'supported',
      reviewEvidence: 'graph_surface_atomic_target',
      requiredEvidenceKeys: TIMER_DELEGATE_REQUIRED_EVIDENCE_KEYS,
    },
    {
      id: 'schedule.latent_or_async_node',
      kind: 'schedule',
      supportStatus: 'supported',
      reviewEvidence: 'graph_surface_atomic_target',
      requiredEvidenceKeys: LATENT_OR_ASYNC_REQUIRED_EVIDENCE_KEYS,
    },
  ],
  evidence: {
    projectionSource: 'UE ActionDatabase',
    requiredKeys: GENERIC_SCHEDULE_REQUIRED_EVIDENCE_KEYS,
  },
  executeRevalidation: 'required',
},
```

- [ ] **Step 4: Add compiler tests for owner mixing**

Add these tests to `task-compiler.convert-schedule.test.ts`:

```ts
test('generic schedule statement compiles without function_operation ownership mixing', () => {
  const input = {
    kind: 'schedule',
    schedule_operation: 'timer_delegate_node',
    context_evidence: {
      schedule_action_stable_id: 'action_database:/Script/Engine.KismetSystemLibrary:/Script/BlueprintGraph.K2Node_CallFunction:sig',
      schedule_node_class: '/Script/BlueprintGraph.K2Node_CallFunction',
      schedule_spawner_signature: 'sig',
      schedule_owner_path: '/Script/Engine.KismetSystemLibrary',
      handler_name: 'HandleTimerElapsed',
      handler_function_path: '/Game/BP/BP_Timer.HandleTimerElapsed',
      handler_source_cluster: 'BlueprintSignature',
      signature_evidence_id: 'signature:function:HandleTimerElapsed',
    },
    args: {
      time: { kind: 'literal', value_type: 'number', value: 0.25 },
    },
  };

  for (const statement of [compileTaskPlanStatement(input), compileBridgeStatement(input)]) {
    assert.equal(statement.kind, 'schedule');
    assert.equal(statement.schedule_operation, 'timer_delegate_node');
    assert.equal(Object.hasOwn(statement, 'function_operation'), false);
    assert.deepEqual(statement.context_evidence, input.context_evidence);
  }
});

test('generic schedule rejects function_operation ownership mixing', () => {
  const input = {
    kind: 'schedule',
    function_operation: 'schedule_function',
    schedule_operation: 'timer_delegate_node',
    args: {},
  };

  assert.throws(
    () => compileTaskPlanStatement(input),
    /unsupported_schedule_owner_mix/,
  );
});
```

- [ ] **Step 5: Implement compiler validation**

Add this helper near `copyConvertScheduleSemanticFields` in `task-compiler.ts`:

```ts
const GENERIC_SCHEDULE_OPERATIONS = new Set(['timer_delegate_node', 'latent_or_async_node']);

function validateConvertScheduleOwnership(record: Record<string, unknown>, path: string): void {
  const kind = typeof record.kind === 'string' ? record.kind : '';
  if (kind !== 'schedule') {
    return;
  }

  const functionOperation = typeof record.function_operation === 'string' ? record.function_operation.trim() : '';
  const scheduleOperation = typeof record.schedule_operation === 'string' ? record.schedule_operation.trim().toLowerCase() : '';
  if (functionOperation.length > 0 && GENERIC_SCHEDULE_OPERATIONS.has(scheduleOperation)) {
    throw new TaskSpecCompileError('unsupported_schedule_owner_mix', 'Generic schedule operations must not specify function_operation.', [
      {
        code: 'unsupported_schedule_owner_mix',
        path: `${path}.function_operation`,
        message: 'Remove function_operation for timer_delegate_node or latent_or_async_node. Use function_operation only for FunctionAction-owned schedule_function or latent_or_async_function.',
      },
    ]);
  }
}
```

Call it from both statement and expression validation paths after the existing `kind` check:

```ts
validateConvertScheduleOwnership(statementRecord, statementPath);
```

and:

```ts
validateConvertScheduleOwnership(expressionRecord, path);
```

- [ ] **Step 6: Run Node tests**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
```

Expected after implementation: all Node tests pass.

## Task 2: Neutral ActionDatabase Projection Service

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Public/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.cpp`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperActionResolutionContractTests.cpp`

- [ ] **Step 1: Add failing contract scan for neutral projection**

Add a source contract test that reads `BlueprintHelperGenericTransformScheduleActionResolver.cpp` and fails if `UBlueprintNodeSpawner::Create(` appears inside schedule resolution logic for `timer_delegate_node` or `latent_or_async_node`.

Expected before implementation: test fails if schedule success is attempted with synthetic spawner creation; it may pass before success code exists.

- [ ] **Step 2: Add schedule evidence struct**

In `BlueprintHelperProjectedSpawnerEvidence.h`, add:

```cpp
struct FBlueprintHelperProjectedScheduleActionEvidence
{
	FString StableId;
	FString NodeClassPath;
	FString SpawnerSignature;
	FString OwnerPath;
	FString Query;
	FString MenuName;
	FString Category;
	FString DelegatePinName;
	FString HandlerName;
	FString HandlerFunctionPath;
	FString HandlerSourceCluster;
	FString SignatureEvidenceId;
	FString GraphLatentAllowed;

	bool HasSelector() const;
	bool HasProjectedIdentity() const;
	bool HasTimerHandlerEvidence() const;
	bool IsGraphLatentAllowed() const;
};
```

Add declarations:

```cpp
static FBlueprintHelperProjectedScheduleActionEvidence ReadScheduleActionEvidence(
	const FBlueprintHelperActionResolutionRequest& Request);
static void WriteScheduleActionEvidence(
	const FBlueprintHelperProjectedScheduleActionEvidence& Evidence,
	TMap<FString, FString>& OutContextEvidence);
static FString MakeScheduleActionStableId(
	const UObject* ActionOwner,
	const UBlueprintNodeSpawner* Spawner,
	const UClass* NodeClass);
```

- [ ] **Step 3: Implement schedule evidence parsing**

In `BlueprintHelperProjectedSpawnerEvidence.cpp`, implement the struct using the keys from this plan:

```cpp
bool FBlueprintHelperProjectedScheduleActionEvidence::HasSelector() const
{
	return !StableId.IsEmpty()
		|| !NodeClassPath.IsEmpty()
		|| !SpawnerSignature.IsEmpty()
		|| !OwnerPath.IsEmpty()
		|| !Query.IsEmpty()
		|| !MenuName.IsEmpty()
		|| !Category.IsEmpty();
}

bool FBlueprintHelperProjectedScheduleActionEvidence::HasProjectedIdentity() const
{
	return !StableId.IsEmpty()
		&& !NodeClassPath.IsEmpty()
		&& !SpawnerSignature.IsEmpty()
		&& !OwnerPath.IsEmpty();
}

bool FBlueprintHelperProjectedScheduleActionEvidence::HasTimerHandlerEvidence() const
{
	return !HandlerName.IsEmpty()
		&& !HandlerFunctionPath.IsEmpty()
		&& !HandlerSourceCluster.IsEmpty()
		&& !SignatureEvidenceId.IsEmpty();
}

bool FBlueprintHelperProjectedScheduleActionEvidence::IsGraphLatentAllowed() const
{
	return GraphLatentAllowed.Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| GraphLatentAllowed == TEXT("1")
		|| GraphLatentAllowed.Equals(TEXT("yes"), ESearchCase::IgnoreCase);
}
```

Read/write functions must use `schedule_action_stable_id`, `schedule_node_class`, `schedule_spawner_signature`, `schedule_owner_path`, `schedule_query`, `schedule_menu_name`, `schedule_category`, `schedule_delegate_pin_name`, `handler_name`, `handler_function_path`, `handler_source_cluster`, `signature_evidence_id`, and `graph_latent_allowed`.

- [ ] **Step 4: Create neutral projection service**

Create `BlueprintHelperActionDatabaseProjectionService.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

class UBlueprint;
class UBlueprintNodeSpawner;
class UEdGraph;

struct FBlueprintHelperProjectedActionDatabaseEvidence
{
	FString StableId;
	FString NodeClassPath;
	FString SpawnerSignature;
	FString OwnerPath;
	FString Query;
	FString MenuName;
	FString Category;
};

struct FBlueprintHelperActionDatabaseProjectionRequest
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* TargetGraph = nullptr;
	FBlueprintHelperProjectedActionDatabaseEvidence RequiredEvidence;
	FString Query;
	FString ErrorPrefix = TEXT("action_database");
};

struct FBlueprintHelperActionDatabaseProjectedCandidate
{
	const UObject* ActionOwner = nullptr;
	UBlueprintNodeSpawner* Spawner = nullptr;
	UClass* NodeClass = nullptr;
	FString StableId;
	FString NodeClassPath;
	FString SpawnerSignature;
	FString OwnerPath;
	FString Query;
	FString MenuName;
	FString Category;
};

struct FBlueprintHelperActionDatabaseProjectionResult
{
	EBlueprintHelperActionResolutionStatus Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	FString ErrorCode;
	FString Message;
	TArray<FBlueprintHelperActionDatabaseProjectedCandidate> Candidates;
};

class FBlueprintHelperActionDatabaseProjectionService
{
public:
	static FBlueprintHelperActionDatabaseProjectionResult Project(
		const FBlueprintHelperActionDatabaseProjectionRequest& Request);
};
```

Implement it by moving the existing ActionDatabase scan/filter/candidate matching logic from `BlueprintHelperAssetActionProjectionService.cpp`. Keep the exact behavior: refresh ActionDatabase, build `FBlueprintActionContext`, apply `FBlueprintActionFilter::BPFILTER_RejectIncompatibleThreadSafety`, match exact identity fields first, then query/menu/category.

- [ ] **Step 5: Reuse neutral projection in asset_action without behavior change**

In `BlueprintHelperAssetActionProjectionService.cpp`, replace duplicated registry scanning with a call to `FBlueprintHelperActionDatabaseProjectionService::Project`. Map the neutral candidate into `FBlueprintHelperAssetActionProjectedCandidate` and preserve current error codes:

```cpp
asset_action_spawner_not_found
asset_action_spawner_ambiguous
needs_more_semantic_context
```

- [ ] **Step 6: Run focused ActionResolution tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution;Quit"
```

Expected: existing ActionResolution tests still pass; asset_action behavior does not regress.

## Task 3: Generic Schedule Resolver Success Path

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericTransformScheduleActionResolverTests.cpp`

- [ ] **Step 1: Add failing resolver tests**

In `BlueprintHelperGenericTransformScheduleActionResolverTests.cpp`, replace the blanket "requires spawner evidence" test with four cases:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleTimerRequiresHandlerEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule.TimerRequiresHandlerEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleTimerRequiresHandlerEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericTransformScheduleRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule);
	Request.Semantic.ScheduleOperation = TEXT("timer_delegate_node");
	Request.ContextEvidence.Add(TEXT("schedule_action_stable_id"), TEXT("projected-id"));
	Request.ContextEvidence.Add(TEXT("schedule_node_class"), TEXT("/Script/BlueprintGraph.K2Node_CallFunction"));
	Request.ContextEvidence.Add(TEXT("schedule_spawner_signature"), TEXT("projected-signature"));
	Request.ContextEvidence.Add(TEXT("schedule_owner_path"), TEXT("/Script/Engine.KismetSystemLibrary"));

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("timer without handler status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("timer without handler code"), Result.ErrorCode, FString(TEXT("handler_evidence_missing")));
	TestFalse(TEXT("timer without handler has no spawner"), Result.SelectedSpawner.IsValid());
	return true;
}
```

Add a latent-permission test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleLatentRequiresGraphPermissionTest,
	"BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule.LatentRequiresGraphPermission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleLatentRequiresGraphPermissionTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericTransformScheduleTestBlueprint();
	UEdGraph* Graph = GetGenericTransformScheduleTestGraph(Blueprint);

	FBlueprintHelperActionResolutionRequest Request =
		MakeGenericTransformScheduleRequest(Blueprint, Graph, EBlueprintHelperActionSemanticKind::Schedule);
	Request.Semantic.ScheduleOperation = TEXT("latent_or_async_node");
	Request.ContextEvidence.Add(TEXT("schedule_action_stable_id"), TEXT("projected-id"));
	Request.ContextEvidence.Add(TEXT("schedule_node_class"), TEXT("/Script/BlueprintGraph.K2Node_AsyncAction"));
	Request.ContextEvidence.Add(TEXT("schedule_spawner_signature"), TEXT("projected-signature"));
	Request.ContextEvidence.Add(TEXT("schedule_owner_path"), TEXT("/Script/Engine"));
	Request.ContextEvidence.Add(TEXT("graph_latent_allowed"), TEXT("false"));

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperActionResolutionCore::Resolve(Request);

	TestEqual(TEXT("latent not allowed status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("latent not allowed code"), Result.ErrorCode, FString(TEXT("latent_function_not_allowed_in_graph")));
	return true;
}
```

Add one positive timer projection test and one positive latent/async projection test using current ActionDatabase evidence from a known candidate. The test helper must obtain the evidence from `FBlueprintHelperActionDatabaseProjectionService::Project` first, then feed it back into the resolver so the success path proves revalidation rather than synthetic node creation.

- [ ] **Step 2: Implement schedule evidence validation**

Add helper functions to `BlueprintHelperGenericTransformScheduleActionResolver.cpp`:

```cpp
static FBlueprintHelperActionResolutionResult MakeScheduleInvalidResult(
	const TCHAR* ErrorCode,
	const FString& Message)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::InvalidRequest;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}

static bool IsTimerOperation(const FString& Operation)
{
	return Operation.Equals(TEXT("timer_delegate_node"), ESearchCase::IgnoreCase);
}

static bool IsLatentOrAsyncOperation(const FString& Operation)
{
	return Operation.Equals(TEXT("latent_or_async_node"), ESearchCase::IgnoreCase);
}
```

Validation rules:

```cpp
if (!Evidence.HasProjectedIdentity())
{
	return MakeScheduleInvalidResult(
		TEXT("schedule_spawner_evidence_missing"),
		TEXT("Generic schedule requires projected ActionDatabase spawner identity evidence."));
}

if (IsTimerOperation(Operation) && !Evidence.HasTimerHandlerEvidence())
{
	return MakeScheduleInvalidResult(
		TEXT("handler_evidence_missing"),
		TEXT("timer_delegate_node requires projected handler and signature evidence from BlueprintSignature or ActionContext."));
}

if (IsLatentOrAsyncOperation(Operation) && !Evidence.IsGraphLatentAllowed())
{
	return MakeScheduleInvalidResult(
		TEXT("latent_function_not_allowed_in_graph"),
		TEXT("latent_or_async_node requires graph_latent_allowed=true evidence."));
}
```

- [ ] **Step 3: Resolve selected schedule spawner through ActionDatabase**

In `ResolveSchedule`, call the neutral projection service:

```cpp
const FBlueprintHelperProjectedScheduleActionEvidence Evidence =
	FBlueprintHelperProjectedSpawnerEvidence::ReadScheduleActionEvidence(Context.GetRequest());

FBlueprintHelperActionDatabaseProjectionRequest ProjectionRequest;
ProjectionRequest.Blueprint = Context.GetRequest().Blueprint;
ProjectionRequest.TargetGraph = Context.GetRequest().TargetGraph;
ProjectionRequest.RequiredEvidence.StableId = Evidence.StableId;
ProjectionRequest.RequiredEvidence.NodeClassPath = Evidence.NodeClassPath;
ProjectionRequest.RequiredEvidence.SpawnerSignature = Evidence.SpawnerSignature;
ProjectionRequest.RequiredEvidence.OwnerPath = Evidence.OwnerPath;
ProjectionRequest.RequiredEvidence.Query = Evidence.Query;
ProjectionRequest.RequiredEvidence.MenuName = Evidence.MenuName;
ProjectionRequest.RequiredEvidence.Category = Evidence.Category;
ProjectionRequest.Query = Evidence.Query;
ProjectionRequest.ErrorPrefix = TEXT("schedule");

const FBlueprintHelperActionDatabaseProjectionResult Projection =
	FBlueprintHelperActionDatabaseProjectionService::Project(ProjectionRequest);
```

Map outcomes:

```cpp
if (Projection.Status == EBlueprintHelperActionResolutionStatus::NotFound)
{
	return MakeUnsupportedResult(TEXT("schedule_spawner_not_found"), TEXT("Generic schedule projected evidence did not match any current ActionDatabase spawner."));
}
if (Projection.Status == EBlueprintHelperActionResolutionStatus::Ambiguous)
{
	FBlueprintHelperActionResolutionResult Result;
	Result.Status = EBlueprintHelperActionResolutionStatus::Ambiguous;
	Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Result.ErrorCode = TEXT("schedule_spawner_ambiguous");
	Result.Message = Projection.Message;
	return Result;
}
```

On success, return a normal resolved result:

```cpp
const FBlueprintHelperActionDatabaseProjectedCandidate& Match = Projection.Candidates[0];
FBlueprintHelperActionResolutionResult Result;
Result.Status = EBlueprintHelperActionResolutionStatus::Resolved;
Result.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
Result.SelectedStableId = Match.StableId;
Result.SelectedSpawner = Match.Spawner;
Result.SpawnerClass = Match.Spawner ? Match.Spawner->GetClass()->GetPathName() : FString();
Result.NodeClass = Match.NodeClassPath;
Result.MatchReason = FString::Printf(TEXT("generic_schedule operation=%s"), *Operation);
FBlueprintHelperCallFunctionCandidateInfo Candidate;
Candidate.StableId = Match.StableId;
Candidate.DisplayName = Match.MenuName;
Candidate.Category = Match.Category;
Candidate.NodeClassPath = Match.NodeClassPath;
Candidate.MatchReason = Result.MatchReason;
Candidate.Score = 100;
Candidate.bGraphCompatible = true;
Candidate.bFromActionDatabase = true;
Result.CandidateActions.Add(MoveTemp(Candidate));
return Result;
```

- [ ] **Step 4: Run focused resolver tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule;Quit"
```

Expected: missing evidence returns deterministic diagnostics; complete projected evidence resolves exactly one current ActionDatabase spawner.

## Task 4: Timer Delegate Fragment Link Without Handler Creation

**Files:**
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperDelegateLinkFragmentUtils.h`
- Create: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperDelegateLinkFragmentUtils.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperEventDelegateFragmentBuilder.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`
- Create or modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericScheduleFragmentTests.cpp`

- [ ] **Step 1: Extract delegate-link helper**

Create `BlueprintHelperDelegateLinkFragmentUtils.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

class UK2Node;
class UK2Node_CreateDelegate;
class UEdGraph;
struct FBlueprintHelperNodeFragment;

struct FBlueprintHelperDelegateLinkRequest
{
	FString StatementId;
	FString HandlerName;
	FString DelegatePinName;
	FVector2D Location = FVector2D::ZeroVector;
};

class FBlueprintHelperDelegateLinkFragmentUtils
{
public:
	static UK2Node_CreateDelegate* SpawnCreateDelegateNode(
		UEdGraph* TargetGraph,
		const FBlueprintHelperDelegateLinkRequest& Request,
		FString& OutError);

	static bool ConnectCreateDelegateToDelegateInput(
		UEdGraph* TargetGraph,
		UK2Node* PrimaryNode,
		UK2Node_CreateDelegate* CreateDelegateNode,
		const FBlueprintHelperDelegateLinkRequest& Request,
		FBlueprintHelperNodeFragment& OutFragment,
		FString& OutError);
};
```

Implementation requirements:

```cpp
// Spawn must use FBlueprintHelperActionNodeSpawnerAdapter::InvokeNodeSpawner.
// It may create UBlueprintNodeSpawner::Create(UK2Node_CreateDelegate::StaticClass())
// only for the auxiliary CreateDelegate node because UE ActionDatabase does not
// expose a selected user menu action for this internal link node.
```

Delegate input pin selection:

```cpp
// If Request.DelegatePinName is not empty, find that input pin and require it to accept the CreateDelegate output.
// If Request.DelegatePinName is empty, scan input pins and require exactly one schema-compatible pin.
// Return timer_delegate_pin_missing or timer_delegate_pin_ambiguous when the link target is not deterministic.
```

- [ ] **Step 2: Keep EventDelegate behavior unchanged**

Replace the local `SpawnCreateDelegateNode` and `ConnectCreateDelegateToPrimary` logic in `BlueprintHelperEventDelegateFragmentBuilder.cpp` with the new helper. Preserve existing ownership tags and pin bindings:

```cpp
OutFragment.PinBindings.Add(TEXT("create_delegate.event"), Link.From);
OutFragment.PinBindings.Add(TEXT("delegate.event"), Link.To);
```

- [ ] **Step 3: Add timer schedule post-spawn link**

In `BuildActionProviderFragment`, after `BuildResolvedActionFragment` succeeds, add a narrow branch that is generic by semantic and operation, not by node class:

```cpp
const bool bTimerDelegateSchedule =
	SemanticKind == EBlueprintHelperActionSemanticKind::Schedule
	&& Request.ScheduleOperation.Equals(TEXT("timer_delegate_node"), ESearchCase::IgnoreCase);
if (bTimerDelegateSchedule)
{
	FBlueprintHelperDelegateLinkRequest LinkRequest;
	LinkRequest.StatementId = Request.FragmentId;
	LinkRequest.HandlerName = Request.ContextEvidence.FindRef(TEXT("handler_name"));
	LinkRequest.DelegatePinName = Request.ContextEvidence.FindRef(TEXT("schedule_delegate_pin_name"));
	LinkRequest.Location = FVector2D(Request.Location.X + 240.0, Request.Location.Y);
	UK2Node_CreateDelegate* CreateDelegateNode =
		FBlueprintHelperDelegateLinkFragmentUtils::SpawnCreateDelegateNode(TargetGraph, LinkRequest, OutError);
	if (!CreateDelegateNode)
	{
		return false;
	}
	OutFragment.Nodes.Add(CreateDelegateNode);
	if (!FBlueprintHelperDelegateLinkFragmentUtils::ConnectCreateDelegateToDelegateInput(
		TargetGraph,
		OutFragment.PrimaryNode,
		CreateDelegateNode,
		LinkRequest,
		OutFragment,
		OutError))
	{
		return false;
	}
	OutFragment.OwnershipTags.Add(TEXT("schedule_operation"), TEXT("timer_delegate_node"));
	OutFragment.OwnershipTags.Add(TEXT("handler_source_cluster"), Request.ContextEvidence.FindRef(TEXT("handler_source_cluster")));
	OutFragment.OwnershipTags.Add(TEXT("signature_evidence_id"), Request.ContextEvidence.FindRef(TEXT("signature_evidence_id")));
}
```

The branch is allowed because it is scoped to semantic operation `timer_delegate_node` and consumes reusable delegate-link behavior. It must not create a handler, function graph, event entry, dispatcher, or signature row.

- [ ] **Step 4: Add fragment readback test**

Create a focused test that:

```cpp
// 1. Builds an Actor Blueprint event graph fixture.
// 2. Creates or confirms a handler through BlueprintSignature fixture code before GraphWrite runs.
// 3. Runs a GraphWrite schedule statement with timer_delegate_node evidence.
// 4. Asserts the fragment contains the schedule primary node and one UK2Node_CreateDelegate.
// 5. Asserts UK2Node_CreateDelegate::GetFunctionName() equals HandleTimerElapsed.
// 6. Asserts the CreateDelegate output pin links to the schedule node delegate input pin.
// 7. Asserts GraphWrite did not create an extra handler graph/signature.
```

Use this automation name:

```cpp
BlueprintHelper.GraphWrite.GenericSchedule.TimerDelegateFragmentLinksExistingHandler
```

- [ ] **Step 5: Run focused fragment tests**

Run:

```powershell
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.GenericSchedule;Quit"
```

Expected: timer schedule creates the schedule node plus CreateDelegate link; no handler/signature is created by GraphWrite.

## Task 5: Latent / Async Readback And Diagnostics

**Files:**
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericTransformScheduleActionResolverTests.cpp`
- Modify: `BlueprintHelper/Source/BlueprintHelper/Private/Tests/GraphWrite/BlueprintHelperGenericScheduleFragmentTests.cpp`
- Modify if needed: `BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.cpp`

- [ ] **Step 1: Add latent/async success fixture**

Add a helper that queries current ActionDatabase for a normal Blueprint-compatible async action:

```cpp
static bool TryProjectLatentOrAsyncScheduleEvidence(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	FBlueprintHelperProjectedScheduleActionEvidence& OutEvidence)
{
	FBlueprintHelperActionDatabaseProjectionRequest ProjectionRequest;
	ProjectionRequest.Blueprint = Blueprint;
	ProjectionRequest.TargetGraph = Graph;
	ProjectionRequest.RequiredEvidence.Query = TEXT("Async Load Primary Asset");
	ProjectionRequest.Query = TEXT("Async Load Primary Asset");
	ProjectionRequest.ErrorPrefix = TEXT("schedule");

	const FBlueprintHelperActionDatabaseProjectionResult Projection =
		FBlueprintHelperActionDatabaseProjectionService::Project(ProjectionRequest);
	if (Projection.Status != EBlueprintHelperActionResolutionStatus::Resolved || Projection.Candidates.Num() != 1)
	{
		return false;
	}

	const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate = Projection.Candidates[0];
	OutEvidence.StableId = Candidate.StableId;
	OutEvidence.NodeClassPath = Candidate.NodeClassPath;
	OutEvidence.SpawnerSignature = Candidate.SpawnerSignature;
	OutEvidence.OwnerPath = Candidate.OwnerPath;
	OutEvidence.Query = Candidate.Query;
	OutEvidence.MenuName = Candidate.MenuName;
	OutEvidence.Category = Candidate.Category;
	OutEvidence.GraphLatentAllowed = TEXT("true");
	return true;
}
```

If the chosen ActionDatabase query resolves to a different unique async action in UE 5.6, update only the query string and expected node family in this helper; do not add a resolver special case.

- [ ] **Step 2: Assert latent/async selected spawner readback**

The focused test must assert:

```cpp
TestEqual(TEXT("latent schedule status"), Result.Status, EBlueprintHelperActionResolutionStatus::Resolved);
TestTrue(TEXT("latent schedule spawner is current ActionDatabase spawner"), Result.SelectedSpawner.IsValid());
TestTrue(TEXT("latent schedule candidate is database backed"), Result.CandidateActions[0].bFromActionDatabase);
TestTrue(TEXT("latent schedule match reason"), Result.MatchReason.Contains(TEXT("latent_or_async_node")));
```

For fragment readback:

```cpp
// Primary node exists.
// Primary node class path equals the projected schedule_node_class.
// The statement ownership tag includes semantic_kind=schedule.
// The statement ownership tag includes schedule_operation=latent_or_async_node.
// The graph compiles without latent-not-allowed diagnostics.
```

- [ ] **Step 3: Assert diagnostics stay deterministic**

Add negative tests for:

```text
schedule_spawner_evidence_missing
schedule_spawner_not_found
schedule_spawner_ambiguous
latent_function_not_allowed_in_graph
timer_delegate_pin_missing
timer_delegate_pin_ambiguous
```

Each test must assert `SelectedSpawner.IsValid() == false` except the pin-link tests, where resolution succeeds and fragment building fails with the pin diagnostic.

## Task 6: Documentation Sync And Final Verification

**Files:**
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`
- Modify: `BlueprintHelper/Develop/Plan/BlueprintHelper_GraphWrite_GeneralityPreflightTest_Plan_20260525_CN.md`
- Modify: `BlueprintHelper/Develop/Design/BlueprintHelper_GraphStatementFramework_Design_20260521_CN.md`

- [ ] **Step 1: Sync capability status**

After implementation, update `BlueprintHelper_GraphWrite_CapabilityContract_20260525_CN.md`:

```markdown
| `timer_delegate_node` | `kind=schedule, schedule_operation=timer_delegate_node`, projected ActionDatabase spawner evidence, BlueprintSignature handler evidence. | SUPPORTED: resolver revalidates current ActionDatabase spawner; fragment builder links to existing handler via CreateDelegate; GraphWrite does not create handler/signature. |
| `latent_or_async_node` | `kind=schedule, schedule_operation=latent_or_async_node`, projected ActionDatabase spawner evidence, `graph_latent_allowed=true`. | SUPPORTED: resolver revalidates current ActionDatabase spawner; fragment builder spawns through shared adapter; false latent permission blocks deterministically. |
```

- [ ] **Step 2: Sync generality preflight matrix**

Keep these rows in the final matrix and reference this plan:

```markdown
| `schedule.timer_delegate_node` | Generic schedule success path | Requires projected schedule spawner evidence plus BlueprintSignature handler evidence. |
| `schedule.latent_or_async_node` | Generic schedule success path | Requires projected schedule spawner evidence plus `graph_latent_allowed=true`. |
```

- [ ] **Step 3: Run full verification**

Run:

```powershell
npm.cmd --prefix AgentFaceService/task-core run build
npm.cmd --prefix AgentFaceService/task-core run test:node
& "E:\UE_5.6\Engine\Build\BatchFiles\Build.bat" TemplateEditor Win64 Development -Project="D:\UEProjects\Template\Template.uproject" -WaitMutex -NoHotReload
& "E:\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UEProjects\Template\Template.uproject" -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests BlueprintHelper.GraphWrite.ActionResolution.Generic.Schedule;Automation RunTests BlueprintHelper.GraphWrite.GenericSchedule;Automation RunTests BlueprintHelper.GraphWrite;Quit"
rg -n "function_operation.*timer_delegate_node|function_operation.*latent_or_async_node|timer_delegate_node.*function_operation|latent_or_async_node.*function_operation" AgentFaceService/task-core/src BlueprintHelper/Source/BlueprintHelper
rg -n "UBlueprintNodeSpawner::Create\\(" BlueprintHelper/Source/BlueprintHelper/Private/Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericTransformScheduleActionResolver.cpp
git diff --check
```

Expected:

```text
Node build/test passes.
UE build passes.
Focused Generic schedule tests pass.
Full BlueprintHelper.GraphWrite suite passes.
Ownership-mixing scan has no production-code hits.
GenericTransformScheduleActionResolver has no UBlueprintNodeSpawner::Create hit for schedule success.
git diff --check reports no whitespace errors.
```

## Execution Notes

- Do not add `function_operation` to Generic schedule TaskSpec examples.
- Do not allow resolver success from only `schedule_node_class`; stable id, node class, spawner signature, and owner path must match a current ActionDatabase candidate.
- Do not create custom events, functions, dispatchers, override/native entries, or handler signatures from GraphWrite.
- Do not move delegate bind/assign/unbind/call into Generic schedule.
- Do not add per-schedule Review atomic target in this plan; Review stays graph_block level.
- Do not run `git add`, `git commit`, or `git push`. Provide manual commit commands after execution.

## Suggested Manual Commit Message After Execution

```text
新增内容：
1. GraphWrite Generic schedule success path for timer_delegate_node and latent_or_async_node
2. Projected ActionDatabase schedule evidence contract and readback coverage

变更需求：
1. Split schedule handler/signature ownership to BlueprintSignature and ActionContext projection
```
